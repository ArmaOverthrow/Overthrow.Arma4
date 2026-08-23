//------------------------------------------------------------------------------------------------
//! Draws the selected AI group's waypoint route in the Game Master's 3D view.
//!
//! NOT A COMPONENT, NOT A WIDGET, NO PREFAB, NO LAYOUT, NO GUID. A plain Managed object owned by
//! modded SCR_EditModeEditorUIComponent, which descends from MenuRootSubComponent and therefore hands
//! this class MenuRootBase.GetOnMenuUpdate() - the same per-frame invoker vanilla's own waypoint
//! renderer uses (SCR_WaypointLinesEditorUIComponent.c:171-173). Living there also gives the feature
//! its GM gate structurally: Mode_Edit.layout is instantiated only by EditorModeEdit.et, the one
//! unlimited editor mode. No IsLimited() or role check belongs on this side.
//!
//! WHY Shape AND ONLY Shape. Vanilla's renderer additionally projects screen-space legs into the
//! shared editor canvas (SetDrawCommands). This one does not: m_wCanvas is shared and a direct
//! SetDrawCommands discards every other user's commands, and a 3D line with NOZBUFFER already draws
//! through terrain, which is the only thing the canvas path was buying.
//!
//! THE SEAM IS RESOLVED PER USE, NEVER CACHED - the rule OVT_GMPanelUIComponent.c:21 states, for the
//! same reason: OVT_ControllerComponent<T>.Get() is null on a dedicated server and before ownership
//! assignment, and a cached null would never heal. The ROUTE object is likewise re-read from the seam
//! each frame rather than held; the only thing this class caches about a route is the group RplId it
//! asked for, which is what Detach() drops.
//!
//! THE GROUP VERTEX IS LIVE, THE WAYPOINTS ARE A SNAPSHOT. Waypoint entities exist only on the server
//! (no vanilla waypoint prefab carries an RplComponent), so their positions can only be the ones the
//! fan carried. The group is a replicated entity, so vertex 0 is re-read every frame and a moving
//! group stays attached to its route. If the group stops resolving, the waypoint chain is drawn alone
//! rather than drawn back to the origin of the world.
//!
//! PRINT-FREE. No selection, a non-group selection, no route, an empty route and a route that lands
//! after the GM moved on are all normal states in which this class draws nothing and says nothing.
//------------------------------------------------------------------------------------------------
class OVT_GMWaypointRenderer : Managed
{
	//------------------------------------------------------------------------------------------------
	// VISUAL CONSTANTS - deliberately not [Attribute]s: this class has no prefab to tune them from.
	//------------------------------------------------------------------------------------------------

	//! Amber, slightly transparent: reads against grass, forest and road alike.
	protected static const int COLOUR_BASE_R = 255;
	protected static const int COLOUR_BASE_G = 190;
	protected static const int COLOUR_BASE_B = 60;
	protected static const int COLOUR_BASE_A = 220;

	//! Cyan, opaque: the current waypoint and the leg leading to it.
	protected static const int COLOUR_HIGHLIGHT_R = 60;
	protected static const int COLOUR_HIGHLIGHT_G = 220;
	protected static const int COLOUR_HIGHLIGHT_B = 255;
	protected static const int COLOUR_HIGHLIGHT_A = 255;

	//! Line thickness in screen pixels (Shape.c:37's last parameter).
	protected static const float LINE_THICKNESS = 2;

	//! Radius in metres of the xz-plane circle marking each waypoint, so vertices stay legible from the
	//! top-down camera a GM actually uses.
	protected static const float CIRCLE_RADIUS = 1.5;

	//! Slice count for those circles; 0 means "engine default".
	protected static const int CIRCLE_SLICES = 0;

	//! The server's cap is 32 waypoints; vertex 0 is the group, hence 33.
	protected static const int MAX_WAYPOINTS = 32;

	//------------------------------------------------------------------------------------------------
	// STATE
	//------------------------------------------------------------------------------------------------

	//! The mode root's menu, kept only so Detach() can unsubscribe from the exact same invoker.
	protected MenuRootBase m_Menu;

	//! The editor's SELECTED filter, kept for the same reason. It outlives this object.
	protected SCR_BaseEditableEntityFilter m_SelectedFilter;

	//! Whether a one-shot filter re-subscription is sitting in the call queue (Detach() must pull it).
	protected bool m_bRetryQueued;

	//! The group whose route this renderer asked for and will draw: the dedupe key for rapid clicks and
	//! box-select, and the test a late fan must pass before it is drawn.
	protected RplId m_RequestedGroupRplId;

	//! The two colours, packed once at attach (vanilla does the same, SCR_WaypointLinesEditorUIComponent.c:153).
	protected int m_iColourBase;
	protected int m_iColourHighlight;

	//! Re-issued every frame and never accumulated: ONCE shapes live for exactly one frame, which is
	//! why no Shape reference is held anywhere in this class.
	protected ShapeFlags m_eFlags = ShapeFlags.VISIBLE | ShapeFlags.NOZBUFFER | ShapeFlags.ONCE;

	//! THE DRAW PATH ALLOCATES NOTHING. A fixed buffer refilled in place: 1 group vertex + the 32
	//! waypoint cap.
	protected vector m_aPoints[33];

	//! Reused out-buffer for the SELECTED filter's contents; touched on selection changes only.
	protected ref set<SCR_EditableEntityComponent> m_aSelection = new set<SCR_EditableEntityComponent>();

	protected int m_iDbgTick; // TEMP TRACE

	//------------------------------------------------------------------------------------------------
	// LIFECYCLE
	//------------------------------------------------------------------------------------------------

	//------------------------------------------------------------------------------------------------
	//! Subscribes everything: the per-frame draw hook, the editor's SELECTED filter and the gm-state
	//! seam's two client invokers.
	//!
	//! Every subscription is individually optional. The menu may be null, the entity manager may not
	//! have built its filters yet (one deferred retry, then silence) and the seam is null on a machine
	//! with no local controller. Each absent source degrades one behaviour and prints nothing.
	//! \param[in] menu The EDIT-mode root's menu, from MenuRootSubComponent.GetMenu().
	void Attach(MenuRootBase menu)
	{
		m_RequestedGroupRplId = RplId.Invalid();

		m_iColourBase = Color.FromRGBA(COLOUR_BASE_R, COLOUR_BASE_G, COLOUR_BASE_B, COLOUR_BASE_A).PackToInt();
		m_iColourHighlight = Color.FromRGBA(COLOUR_HIGHLIGHT_R, COLOUR_HIGHLIGHT_G, COLOUR_HIGHLIGHT_B, COLOUR_HIGHLIGHT_A).PackToInt();

		if (menu)
		{
			m_Menu = menu;
			m_Menu.GetOnMenuUpdate().Insert(OnMenuUpdate);
		}

		Print(string.Format("[OVT-WPVIZ] client: Attach - menu %1 (draw hook %2)",
			menu != null, menu != null), LogLevel.NORMAL); // TEMP TRACE

		SubscribeFilter();

		OVT_GMRequestComponent gm = OVT_ControllerComponent<OVT_GMRequestComponent>.Get();
		if (gm)
		{
			gm.GetOnRouteUpdated().Insert(OnRouteUpdated);
			gm.GetOnStateCleared().Insert(OnStateCleared);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Removes every subscription Attach() made and the deferred retry it may have queued, then drops
	//! the route this renderer was drawing.
	//!
	//! The SELECTED filter, the menu and the seam component ALL OUTLIVE THIS OBJECT. A missed removal
	//! here does not leak memory quietly - it wakes a dead renderer on somebody else's event.
	void Detach()
	{
		if (m_bRetryQueued)
		{
			GetGame().GetCallqueue().Remove(RetrySubscribeFilter);
			m_bRetryQueued = false;
		}

		if (m_Menu)
		{
			m_Menu.GetOnMenuUpdate().Remove(OnMenuUpdate);
			m_Menu = null;
		}

		if (m_SelectedFilter)
		{
			m_SelectedFilter.GetOnChanged().Remove(OnSelectionChanged);
			m_SelectedFilter = null;
		}

		OVT_GMRequestComponent gm = OVT_ControllerComponent<OVT_GMRequestComponent>.Get();
		if (gm)
		{
			gm.GetOnRouteUpdated().Remove(OnRouteUpdated);
			gm.GetOnStateCleared().Remove(OnStateCleared);
		}

		m_RequestedGroupRplId = RplId.Invalid();
		m_aSelection.Clear();
	}

	//------------------------------------------------------------------------------------------------
	//! Subscribes to the editor's SELECTED filter, with ONE deferred retry.
	//!
	//! The editor's entity manager initialises around the UI, so the filter can legitimately be absent
	//! for the frame in which the mode root attaches. One CallLater(.., 0) covers that; if the filter
	//! is still missing on the retry the feature simply never draws, silently. This is a one-shot
	//! retry and not a poll on purpose - a poll would keep a dead editor's renderer warm.
	protected void SubscribeFilter()
	{
		SCR_BaseEditableEntityFilter filter = SCR_BaseEditableEntityFilter.GetInstance(EEditableEntityState.SELECTED);
		if (!filter)
		{
			if (m_bRetryQueued)
				return;

			m_bRetryQueued = true;
			GetGame().GetCallqueue().CallLater(RetrySubscribeFilter, 0);
			return;
		}

		m_SelectedFilter = filter;
		m_SelectedFilter.GetOnChanged().Insert(OnSelectionChanged);
	}

	//------------------------------------------------------------------------------------------------
	//! The one deferred re-attempt. The queued flag is held UP across the call, which is exactly what
	//! makes SubscribeFilter() give up instead of queueing another one - "retry once", not a poll. It
	//! is lowered afterwards so Detach() does not try to cancel a call that has already fired.
	protected void RetrySubscribeFilter()
	{
		m_bRetryQueued = true;
		SubscribeFilter();
		m_bRetryQueued = false;
	}

	//------------------------------------------------------------------------------------------------
	// SELECTION
	//------------------------------------------------------------------------------------------------

	//! The GM's selection changed.
	//!
	//! THE INSERTED AND REMOVED SETS ARE IGNORED, BY CONVENTION. Every Overthrow consumer of this
	//! filter re-reads the filter's current contents and acts on entities[0], so the panel's text and
	//! this drawing can never describe two different entities. Multi-select therefore draws the first
	//! entity's route, and if the first entity is not a group nothing is drawn at all.
	//! \param[in] state The filter's state (always SELECTED here).
	//! \param[in] entitiesInsert Ignored - see above.
	//! \param[in] entitiesRemove Ignored - see above.
	protected void OnSelectionChanged(EEditableEntityState state, set<SCR_EditableEntityComponent> entitiesInsert, set<SCR_EditableEntityComponent> entitiesRemove)
	{
		if (!m_SelectedFilter)
			return;

		m_aSelection.Clear();
		m_SelectedFilter.GetEntities(m_aSelection);

		if (m_aSelection.IsEmpty())
		{
			ClearRoute();
			return;
		}

		// Clicking a soldier in the world selects its CHARACTER editable, not the group - the GROUP
		// editable is only selected directly via its map/hierarchy icon. GetAIGroup() is vanilla's own
		// resolver for exactly this: a GROUP returns itself, a CHARACTER returns its parent group, a
		// VEHICLE returns its AI occupants' group, everything else returns null
		// (SCR_EditableCharacterComponent.c:607, SCR_EditableGroupComponent.c:365).
		SCR_EditableEntityComponent editable = m_aSelection[0];
		if (editable)
			editable = editable.GetAIGroup();

		if (!editable || editable.GetEntityType() != EEditableEntityType.GROUP)
		{
			Print("[OVT-WPVIZ] client: selection is not a GROUP - nothing to draw", LogLevel.NORMAL); // TEMP TRACE
			ClearRoute();
			return;
		}

		RplId groupRplId = GetEntityRplId(editable.GetOwner());
		if (groupRplId == RplId.Invalid())
		{
			Print("[OVT-WPVIZ] client: GROUP has NO valid RplId - cannot ask", LogLevel.WARNING); // TEMP TRACE
			ClearRoute();
			return;
		}

		// Box-select and rapid clicking fire several changes for one visible action, and re-asking
		// would clear the route mid-draw for no gain.
		if (groupRplId == m_RequestedGroupRplId)
			return;

		OVT_GMRequestComponent gm = OVT_ControllerComponent<OVT_GMRequestComponent>.Get();
		if (!gm)
			return;

		Print(string.Format("[OVT-WPVIZ] client: asking for group RplId %1", groupRplId), LogLevel.NORMAL); // TEMP TRACE

		m_RequestedGroupRplId = groupRplId;
		gm.RequestGroupWaypoints(groupRplId);
	}

	//------------------------------------------------------------------------------------------------
	//! A complete route fan landed in the seam's store.
	//!
	//! The store is shared, so this re-syncs the dedupe key with whatever actually arrived: if some
	//! other consumer ever asks the seam for a different group, this renderer must not go on believing
	//! the store holds its own request. Nothing is drawn from here - OnMenuUpdate re-reads the store.
	protected void OnRouteUpdated()
	{
		OVT_GMRequestComponent gm = OVT_ControllerComponent<OVT_GMRequestComponent>.Get();
		if (!gm)
			return;

		OVT_GMWaypointRoute route = gm.GetRoute();
		if (!route)
			return;

		Print(string.Format("[OVT-WPVIZ] client: route committed - group %1, waypoints %2, currentIndex %3, complete %4",
			route.m_GroupRplId, route.m_aWaypoints.Count(), route.m_iCurrentIndex, route.m_bComplete), LogLevel.NORMAL); // TEMP TRACE

		m_RequestedGroupRplId = route.m_GroupRplId;
	}

	//------------------------------------------------------------------------------------------------
	//! The editor closed and the seam emptied its stores. Forget the request so a later fan for the
	//! old group cannot be drawn against a new session's selection.
	protected void OnStateCleared()
	{
		Print("[OVT-WPVIZ] client: OnStateCleared - dedupe key RESET (route will stop drawing)", LogLevel.WARNING); // TEMP TRACE

		m_RequestedGroupRplId = RplId.Invalid();
	}

	//------------------------------------------------------------------------------------------------
	//! Drops the drawn route immediately: the selection is no longer a group this feature can draw.
	//! The store is cleared as well as the dedupe key, so the next frame draws nothing even though the
	//! seam still holds the last complete fan.
	protected void ClearRoute()
	{
		m_RequestedGroupRplId = RplId.Invalid();

		OVT_GMRequestComponent gm = OVT_ControllerComponent<OVT_GMRequestComponent>.Get();
		if (!gm)
			return;

		OVT_GMWaypointRoute route = gm.GetRoute();
		if (route)
			route.Clear();
	}

	//------------------------------------------------------------------------------------------------
	// DRAW
	//------------------------------------------------------------------------------------------------

	//! One frame of the route. Called from MenuRootBase.GetOnMenuUpdate() while EDIT mode is up.
	//!
	//! Nothing is drawn unless there is a COMPLETE route holding at least one waypoint AND that route
	//! belongs to the group this renderer last asked about - which is what makes a fan arriving after
	//! the GM moved on harmless rather than confusing.
	//! \param[in] timeSlice Frame time; unused - the drawing is stateless and re-issued whole.
	protected void OnMenuUpdate(float timeSlice)
	{
		// TEMP TRACE - throttled to roughly once every 2s at 60fps
		m_iDbgTick += 1;
		bool say = (m_iDbgTick % 120) == 1;

		OVT_GMRequestComponent gm = OVT_ControllerComponent<OVT_GMRequestComponent>.Get();
		if (!gm)
		{
			if (say) Print("[OVT-WPVIZ] draw: TICKING but no seam component", LogLevel.WARNING);
			return;
		}

		OVT_GMWaypointRoute route = gm.GetRoute();
		if (!route || !route.HasRoute())
		{
			if (say) Print("[OVT-WPVIZ] draw: TICKING but HasRoute() false", LogLevel.NORMAL);
			return;
		}

		if (route.m_GroupRplId != m_RequestedGroupRplId)
		{
			if (say) Print(string.Format("[OVT-WPVIZ] draw: TICKING but id MISMATCH - route %1 vs requested %2",
				route.m_GroupRplId, m_RequestedGroupRplId), LogLevel.WARNING);
			return;
		}

		int count = route.m_aWaypoints.Count();
		if (count > MAX_WAYPOINTS)
			count = MAX_WAYPOINTS;

		// Vertex 0 is the group's LIVE position. When it no longer resolves - the group died, went
		// dormant or was never replicated to this client - the chain starts at waypoint 0 instead of
		// running back to the world origin.
		int offset = 0;
		IEntity groupEntity = ResolveEntity(route.m_GroupRplId);
		if (groupEntity)
		{
			m_aPoints[0] = groupEntity.GetOrigin();
			offset = 1;
		}

		for (int i = 0; i < count; i++)
		{
			m_aPoints[i + offset] = route.m_aWaypoints[i].m_vPos;
		}

		int pointCount = count + offset;
		int currentIndex = route.m_iCurrentIndex;

		if (say) Print(string.Format("[OVT-WPVIZ] draw: DRAWING %1 points (offset %2, groupEntity %3), first %4",
			pointCount, offset, groupEntity != null, m_aPoints[0]), LogLevel.NORMAL); // TEMP TRACE

		// PER-LEG CreateLine, deliberately NOT one CreateLines strip. Two reasons, both learned in the
		// 2026-08-16 play-test: a CreateLines strip fed from a member fixed array with num < the
		// declared size did not render at all (CreateLine and CreateCircle in the same frame did -
		// vanilla only ever passes a LOCAL array sized exactly to num, e.g. SCR_AIWorld.c:241), and
		// drawing the highlight leg OVER a base strip z-fights, which reads as flashing. Per leg, each
		// segment is drawn exactly once in its final colour: no overdraw, no flicker, and a route is at
		// most 33 tiny native calls for the one selected group.
		for (int v = 0; v < pointCount - 1; v++)
		{
			// The leg from vertex v terminates at waypoint (v + 1 - offset); when the group vertex is
			// absent there is no leg into waypoint 0 at all.
			int colour = m_iColourBase;
			if (OVT_GMWaypointFormat.IsHighlightLeg(v + 1 - offset, currentIndex))
				colour = m_iColourHighlight;

			Shape.CreateLine(colour, m_eFlags, m_aPoints[v], m_aPoints[v + 1], LINE_THICKNESS);
		}

		// The closing leg exists only when LegCount() says the route has one more leg than it has
		// waypoints. CreateLinesLoop cannot be used: it would close back to the GROUP vertex.
		int legCount = OVT_GMWaypointFormat.LegCount(count, route.IsCyclic());
		if (legCount > count && count >= 2)
			Shape.CreateLine(m_iColourBase, m_eFlags, m_aPoints[count - 1 + offset], m_aPoints[offset], LINE_THICKNESS);

		// Waypoint markers, the current one in the highlight colour.
		for (int i = 0; i < count; i++)
		{
			int colour = m_iColourBase;
			if (i == currentIndex)
				colour = m_iColourHighlight;

			Shape.CreateCircle(colour, m_eFlags, m_aPoints[i + offset], CIRCLE_RADIUS, CIRCLE_SLICES, LINE_THICKNESS);
		}
	}

	//------------------------------------------------------------------------------------------------
	// LOCAL HELPERS
	//------------------------------------------------------------------------------------------------

	//! An entity's networked name. Deliberately re-implemented here rather than borrowed from another
	//! feature's widget component: a three-line accessor is not worth a dependency that would make this
	//! renderer stop working whenever that widget failed to create.
	//! \param[in] entity The entity to name.
	//! \return Its RplId, or RplId.Invalid() when it is not replicated.
	protected RplId GetEntityRplId(IEntity entity)
	{
		if (!entity)
			return RplId.Invalid();

		RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
		if (!rpl)
			return RplId.Invalid();

		return rpl.Id();
	}

	//------------------------------------------------------------------------------------------------
	//! The reverse: a networked name back to this client's copy of the entity.
	//! \param[in] rplId The id carried by the route.
	//! \return The entity, or null when the id resolves to nothing here.
	protected IEntity ResolveEntity(RplId rplId)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(rplId));
		if (!rpl)
			return null;

		return rpl.GetEntity();
	}
}
