class OVT_ReservationSyncComponentClass : OVT_ComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Replicates the BUG-086 reservation state to clients, closing the "motionless ghost" residual.
//!
//! WHY THIS EXISTS. OVT_PersistenceReservation hides a reserved entity with ClearFlags(), which is
//! a LOCAL engine call - the authority stops rendering, tracing and simulating it, but a client
//! already streaming the entity keeps rendering its own copy, and a client that streams it in
//! later (JIP, or walking into range) gets the prefab's default flags, i.e. fully visible. Server
//! owners reported exactly that: a disconnected player's body standing frozen, unkillable but
//! visible. The reservation doc predicted the residual and named Reserve()/Release() as the places
//! to fix it - this component is that fix's replication half.
//!
//! HOW IT WORKS. The authority mirrors the reservation state into m_bReserved
//! (OVT_PersistenceReservation.Reserve/Release are the only writers). RplProp replicates it to
//! every proxy - streamed-in state included, which is what covers JIP and late streamers - and the
//! proxy-side callback applies the VISUAL half of the reservation locally (VISIBLE and TRACEABLE)
//! plus the COLLISION half (ApplyPhysicsState - the entity flags never touched the physics body,
//! which is BUG-189). ACTIVE is deliberately NOT cleared on proxies; the proxy must keep accepting
//! replication updates, and simulation authority is the server's, which already cleared it.
//!
//! The same vanilla pattern: SCR_ResourceComponent.m_bIsVisible
//! ([RplProp(onRplName: "OnVisibilityChanged")], SCR_ResourceComponent.c:115).
//!
//! WHERE IT LIVES. On every prefab whose instances can be reserved: the player character
//! (Character_Player.et), the recruit character (Character_CIV_Recruit.et) and the ownable vehicle
//! bases (Wheeled_Base.et, Helicopter_Base.et). An entity without this component just keeps the old
//! authority-only behaviour - Reserve() treats it as optional.
//------------------------------------------------------------------------------------------------
class OVT_ReservationSyncComponent : OVT_Component
{
	[RplProp(onRplName: "OnReservedChanged")]
	protected bool m_bReserved;

	//! The simulation state the owner's physics body had before a reservation took it out of the
	//! physics world. Local to this machine on purpose - every peer has its own body and reads its
	//! own state back. NONE is a legal value, so the guard is a separate flag, not a sentinel.
	protected SimulationState m_eSavedSimulationState;
	protected bool m_bHasSavedSimulationState;

	//! Per-GEOM interaction masks a character's body had before it was reserved. Local to this
	//! machine, like the simulation state above. Null means "not currently zeroed".
	protected ref array<int> m_aSavedGeomLayers;

	//! Set true to put characters back through the physics half - the A/B switch for the frozen-legs
	//! regression described on ApplyPhysicsState().
	static bool s_bPhysicsHalfOnCharacters = false;

	//------------------------------------------------------------------------------------------------
	//! Mirrors the reservation state and broadcasts it. Authority only; the authority's own entity
	//! flags are OVT_PersistenceReservation's business, not this component's - but the physics body
	//! is applied here on the authority too, because collision is a physics-world property the entity
	//! flags never touched (BUG-189).
	//! \param[in] reserved The state Reserve()/Release() just applied.
	void SetReserved(bool reserved)
	{
		if (m_bReserved == reserved)
			return;

		m_bReserved = reserved;
		ApplyPhysicsState(reserved);
		Replication.BumpMe();
	}

	//------------------------------------------------------------------------------------------------
	//! \return The replicated reservation state - what clients are being told, not the entity flags.
	bool IsReserved()
	{
		return m_bReserved;
	}

	//------------------------------------------------------------------------------------------------
	//! Proxy-side: applies the visual half of the reservation to this machine's copy of the entity.
	//! Runs on every replicated change and when the initial streamed-in state carries a non-default
	//! value - the JIP path.
	protected void OnReservedChanged()
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;

		if (m_bReserved)
			owner.ClearFlags(EntityFlags.VISIBLE | EntityFlags.TRACEABLE);
		else
			owner.SetFlags(EntityFlags.VISIBLE | EntityFlags.TRACEABLE);

		ApplyPhysicsState(m_bReserved);
	}

	//------------------------------------------------------------------------------------------------
	//! The collision half of the reservation, applied to THIS machine's physics body.
	//!
	//! WHY THE ENTITY FLAGS ARE NOT ENOUGH. VISIBLE/TRACEABLE/ACTIVE govern rendering, traces and
	//! entity ticking - none of them unregisters the rigid body from the physics world, and a sleeping
	//! body still collides. So a reserved vehicle was a car you could not see, shoot or use, but drove
	//! straight into (BUG-189) - on clients from the moment BUG-185's fix hid it there too, and on the
	//! authority machine (single player, listen host, server-side AI traffic) all along.
	//!
	//! SIMULATION STATE, NOT INTERACTION LAYERS. This zeroed the body's interaction layer and restored
	//! the saved mask until 2026-08-23. GetInteractionLayer() answers for the BODY while collision is
	//! resolved per geometry, so the restore stamped one aggregate mask onto every geom: a released
	//! vehicle came back with its chassis and its wheels on the same layer and could no longer drive
	//! itself out of its own parking spot - it moved only when something shoved it, and the server put
	//! it straight back where it was parked. SimulationState.NONE is the documented way to take a body
	//! out of BOTH the simulation and the collision world, it is one body-level value, and restoring
	//! the state saved at reserve time is exactly symmetric.
	//!
	//! Runs on the authority from SetReserved() and on proxies from OnReservedChanged(), because each
	//! machine resolves collision against its own copy.
	protected void ApplyPhysicsState(bool reserved)
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;

		// CHARACTERS ARE SKIPPED. Their body is driven by the character controller, and a NONE ->
		// restore round trip behind its back leaves the animation graph frozen on any PROXY that ran
		// it - the character slides with its legs still (reported after a reserved player reconnected;
		// clients that streamed the body in fresh afterwards were fine, which is what points here).
		// BUG-189, the reason this half exists, was a reserved VEHICLE standing as an invisible
		// obstacle; a reserved character is already non-traceable and inactive on the authority.
		bool skipCharacter = !s_bPhysicsHalfOnCharacters && ChimeraCharacter.Cast(owner) != null;
		if (skipCharacter)
		{
			ApplyCharacterCollision(reserved);

			// A release still falls through when an EARLIER build reserved this character through the
			// simulation-state path and left a state to hand back; otherwise there is nothing to undo.
			if (reserved || !m_bHasSavedSimulationState)
				return;
		}

		Physics phys = owner.GetPhysics();
		if (!phys)
			return;

		if (reserved)
		{
			if (!m_bHasSavedSimulationState)
			{
				m_eSavedSimulationState = phys.GetSimulationState();
				m_bHasSavedSimulationState = true;
			}

			phys.SetActive(ActiveState.INACTIVE);
			phys.ChangeSimulationState(SimulationState.NONE);
		}
		else
		{
			SimulationState restored = SimulationState.SIMULATION;
			if (m_bHasSavedSimulationState)
			{
				restored = m_eSavedSimulationState;
				m_bHasSavedSimulationState = false;
			}

			phys.ChangeSimulationState(restored);

			// Woken after the state is back so it settles on a fresh contact set, not a stale one.
			phys.SetActive(ActiveState.ACTIVE);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! The collision half FOR CHARACTERS, done without touching the physics body's simulation state.
	//!
	//! WHY NOT ApplyPhysicsState's route. SimulationState.NONE unregisters the body, and handing back a
	//! bare enum value does not restore what the character controller reads: on any machine that ran
	//! the round trip the proxy computed movement speed 0 forever after, so a released player slid
	//! along with motionless legs while stances and gestures - discrete replicated commands - still
	//! worked. Locomotion is derived from the body; stance is not. That is the tell.
	//!
	//! WHY PER GEOM. Collision is resolved per geometry, and GetInteractionLayer() answers for the
	//! whole BODY - restoring that one aggregate mask onto every geom is what previously left a
	//! released vehicle with its chassis and wheels on the same layer, unable to drive itself out of
	//! its parking spot. Saving and restoring each geom's own mask is exact. Vanilla works per geom for
	//! the same reason (SCR_PhysicsHelper.RemapInteractionLayer).
	//!
	//! Mask 0 means the geometry belongs to no layer, so nothing tests against it - the body stays in
	//! the physics world, keeps its transform and its velocity tracking, and simply stops being
	//! something you can walk into.
	protected void ApplyCharacterCollision(bool reserved)
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;

		Physics phys = owner.GetPhysics();
		if (!phys)
			return;

		int numGeoms = phys.GetNumGeoms();

		if (reserved)
		{
			if (m_aSavedGeomLayers)
				return;

			m_aSavedGeomLayers = new array<int>();
			for (int i = 0; i < numGeoms; i++)
			{
				m_aSavedGeomLayers.Insert(phys.GetGeomInteractionLayer(i));
				phys.SetGeomInteractionLayer(i, 0);
			}

			return;
		}

		if (!m_aSavedGeomLayers)
			return;

		for (int i = 0; i < numGeoms && i < m_aSavedGeomLayers.Count(); i++)
			phys.SetGeomInteractionLayer(i, m_aSavedGeomLayers.Get(i));

		m_aSavedGeomLayers = null;
	}
}
