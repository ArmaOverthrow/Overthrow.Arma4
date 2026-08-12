//! Client-only singleton that composites every registered OVT_MapCanvasLayer into ONE ordered
//! command list and issues ONE CanvasWidget.SetDrawCommands per submit.
//!
//! WHY THIS EXISTS. Every canvas layer resolves the SAME CanvasWidget (vanilla's map layout has a
//! single DrawingWidget and Overthrow does not own that layout). A layer that calls SetDrawCommands
//! with its own private list therefore REPLACES whatever the previous layer submitted - the last
//! module to Update wins and every earlier layer's commands are discarded. That is invisible while
//! only one layer is live, and becomes a "my new overlay broke the restriction rings" bug the moment
//! a second one is.
//!
//! HOW. Each layer keeps its own command list as a "bucket", cleared and refilled by its own Draw().
//! On submit the compositor stamps that layer's bucket with the current frame token, rebuilds the one
//! shared list from every bucket whose stamp is current - in m_iDrawOrder order, lowest first, so a
//! lower draw order is drawn FIRST and therefore sits UNDERNEATH - and calls SetDrawCommands once.
//!
//! FLUSHING ON EVERY SUBMIT IS DELIBERATE and must not be "optimised" into last-submitter detection.
//! There is no reliable way to know which layer runs last (the active-module order follows config, but
//! the reactivation path re-matches modules by type and any layer that early-returns breaks the
//! assumption silently), and a scheme that waits for all layers to submit wedges forever the moment one
//! stops - a frozen canvas is far harder to diagnose than a slightly redundant rebuild. The final
//! submit of any frame always produces the complete list, so correctness needs no completion detection.
//! Cost is one concatenation per layer per frame, immaterial next to the draw itself.
//! The rejected alternatives are recorded in docs/features/map/territory-overlay/implementation.md K2 -
//! they are settled, not open.
//!
//! CLIENT ONLY. Map modules only Update on a machine that has a map, so this singleton is never
//! constructed on a headless server. Nothing here reaches for a manager or the network, and every
//! entry point null-guards, because this runs every frame while the map is open.
class OVT_MapCanvasCompositor : Managed
{
	//-----------------------------------------------------------------------
	// STATIC
	//-----------------------------------------------------------------------

	protected static ref OVT_MapCanvasCompositor s_Instance;

	//! Frame token used before a layer has ever submitted. Any real world time compares unequal to it.
	protected static const float NO_STAMP = -1;

	//-----------------------------------------------------------------------
	// MEMBER VARIABLES
	//-----------------------------------------------------------------------

	//! Registered layers in draw order, m_iDrawOrder ascending. Weak refs - the map entity owns the
	//! modules, the compositor only borrows them, and a strong ref here would outlive the map.
	protected ref array<OVT_MapCanvasLayer> m_aLayers;

	//! Frame token each layer last submitted at. Parallel to m_aLayers, mutated only by Register and
	//! Unregister so the two cannot drift.
	protected ref array<float> m_aStamps;

	//! The one shared list handed to the canvas. SetDrawCommands takes a POINTER to this array rather
	//! than copying it, so it must stay alive for as long as the canvas draws - which is why it is
	//! owned by the singleton, cleared and refilled in place, and never reassigned.
	protected ref array<ref CanvasWidgetCommand> m_aCommands;

	//-----------------------------------------------------------------------
	// LIFECYCLE
	//-----------------------------------------------------------------------

	void OVT_MapCanvasCompositor()
	{
		m_aLayers = new array<OVT_MapCanvasLayer>();
		m_aStamps = new array<float>();
		m_aCommands = new array<ref CanvasWidgetCommand>();
	}

	//! Lazily created on first use. Client-only by construction: the only callers are canvas layers,
	//! which only run where a map exists.
	static OVT_MapCanvasCompositor GetInstance()
	{
		if (!s_Instance)
		{
			s_Instance = new OVT_MapCanvasCompositor();
		}

		return s_Instance;
	}

	//-----------------------------------------------------------------------
	// REGISTRATION
	//-----------------------------------------------------------------------

	//! Register a layer for compositing, keeping the list sorted by m_iDrawOrder ascending.
	//! Idempotent - registering an already-registered layer does nothing.
	//! \param layer the layer to register
	void Register(OVT_MapCanvasLayer layer)
	{
		if (!layer)
			return;

		if (m_aLayers.Find(layer) > -1)
			return;

		int order = layer.GetDrawOrder();

		// Insert before the first layer with a STRICTLY greater order, so layers sharing an order keep
		// their registration order rather than shuffling between map opens
		int insertAt = m_aLayers.Count();
		for (int i = 0; i < m_aLayers.Count(); i++)
		{
			OVT_MapCanvasLayer other = m_aLayers[i];
			if (!other)
				continue;

			if (other.GetDrawOrder() > order)
			{
				insertAt = i;
				break;
			}
		}

		if (insertAt >= m_aLayers.Count())
		{
			m_aLayers.Insert(layer);
			m_aStamps.Insert(NO_STAMP);
		}
		else
		{
			m_aLayers.InsertAt(layer, insertAt);
			m_aStamps.InsertAt(NO_STAMP, insertAt);
		}
	}

	//! Remove a layer from compositing. Safe to call for a layer that is not registered.
	//! \param layer the layer to unregister
	void Unregister(OVT_MapCanvasLayer layer)
	{
		if (!layer)
			return;

		int index = m_aLayers.Find(layer);
		if (index < 0)
			return;

		// RemoveOrdered, never Remove - Remove backfills with the last element and would silently
		// scramble the draw order
		m_aLayers.RemoveOrdered(index);

		if (index < m_aStamps.Count())
		{
			m_aStamps.RemoveOrdered(index);
		}

		// Nothing left to draw. The canvas still points at the shared array, so emptying it in place is
		// what actually clears the canvas - there is no layer left to trigger a flush.
		if (m_aLayers.IsEmpty())
		{
			m_aCommands.Clear();
		}
	}

	//! How many commands the LAST flush handed to the canvas, across every layer.
	//!
	//! This is the number a frame budget is actually measured against - one layer's own count says
	//! nothing about what the canvas was asked to draw. Read it at the START of a frame rather than
	//! straight after your own submit: the list is rebuilt on every submit from the buckets stamped
	//! for the current frame, so a layer that submits early would only ever see itself and whichever
	//! layers happen to run before it. By the next frame the previous one is complete.
	//! \return Commands in the last composited list.
	int GetCompositedCommandCount()
	{
		if (!m_aCommands)
			return 0;

		return m_aCommands.Count();
	}

	//! The registered layers, ordered by m_iDrawOrder ascending (lowest drawn first, underneath).
	//! PUBLIC API, not an internal detail: map/map-layers builds one toggle row per entry, keyed by
	//! GetLayerId, labelled from GetDisplayName and driven by SetLayerVisible.
	//! The returned array is the compositor's own - read it, do not mutate it.
	array<OVT_MapCanvasLayer> GetLayers()
	{
		return m_aLayers;
	}

	//-----------------------------------------------------------------------
	// COMPOSITING
	//-----------------------------------------------------------------------

	//! Stamp the submitting layer's bucket with this frame's token, rebuild the shared command list from
	//! every current-stamped bucket in draw order, and hand it to the canvas.
	//! Called by OVT_MapCanvasLayer.Update for EVERY layer, EVERY frame, visible or not.
	//! \param layer the layer submitting its bucket
	//! \param canvas the canvas widget to draw into
	void SubmitAndFlush(OVT_MapCanvasLayer layer, CanvasWidget canvas)
	{
		if (!layer || !canvas)
			return;

		int index = m_aLayers.Find(layer);
		if (index < 0)
		{
			// Defensive: a layer that resolved a canvas but never registered would otherwise have its
			// commands silently dropped on every frame. Adopt it instead of losing it.
			Register(layer);
			index = m_aLayers.Find(layer);
			if (index < 0)
				return;
		}

		float token = GetFrameToken();

		if (index < m_aStamps.Count())
		{
			m_aStamps[index] = token;
		}

		m_aCommands.Clear();

		int count = m_aLayers.Count();
		if (m_aStamps.Count() < count)
			count = m_aStamps.Count();

		for (int i = 0; i < count; i++)
		{
			// Skip buckets filled in an earlier frame: a layer that stopped submitting - hidden,
			// deactivated, or bailing on a null canvas - drops out of the composite next frame
			if (m_aStamps[i] != token)
				continue;

			OVT_MapCanvasLayer other = m_aLayers[i];
			if (!other)
				continue;

			array<ref CanvasWidgetCommand> bucket = other.GetCommands();
			if (!bucket)
				continue;

			// Copied one by one, not InsertAll: array<T>.InsertAll's parameter strips the element's ref,
			// so it will not accept an array of strong refs
			foreach (CanvasWidgetCommand cmd : bucket)
			{
				m_aCommands.Insert(cmd);
			}
		}

		canvas.SetDrawCommands(m_aCommands);
	}

	//-----------------------------------------------------------------------
	// PROTECTED METHODS
	//-----------------------------------------------------------------------

	//! World time is identical for every module within one frame, which is exactly the property the
	//! stamp needs. Falls back to a constant when there is no world, which stamps every bucket the same
	//! and degrades to "composite everything" rather than to "composite nothing".
	protected float GetFrameToken()
	{
		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return 0;

		return world.GetWorldTime();
	}
}
