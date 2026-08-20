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

	//! The interaction layer the owner's physics body had before a reservation zeroed it. Local to
	//! this machine on purpose - every peer has its own physics body and reads its own layer back.
	//! -1 = nothing saved (interaction layers are bitmasks, 0 would be a legal-looking value).
	protected int m_iSavedInteractionLayer = -1;

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
	//! Zeroing the interaction layer is what removes it from collision; SetActive(INACTIVE) is what
	//! keeps the freed body from being simulated meanwhile. The pre-reservation layer is saved per
	//! machine and restored on release, then the body is woken so it can settle if the world changed
	//! under it. Runs on the authority from SetReserved() and on proxies from OnReservedChanged(),
	//! because each machine resolves collision against its own copy.
	protected void ApplyPhysicsState(bool reserved)
	{
		IEntity owner = GetOwner();
		if (!owner)
			return;

		Physics phys = owner.GetPhysics();
		if (!phys)
			return;

		if (reserved)
		{
			if (m_iSavedInteractionLayer == -1)
				m_iSavedInteractionLayer = phys.GetInteractionLayer();

			phys.SetActive(ActiveState.INACTIVE);
			phys.SetInteractionLayer(0);
		}
		else
		{
			if (m_iSavedInteractionLayer != -1)
			{
				phys.SetInteractionLayer(m_iSavedInteractionLayer);
				m_iSavedInteractionLayer = -1;
			}

			phys.SetActive(ActiveState.ACTIVE);
		}
	}
}
