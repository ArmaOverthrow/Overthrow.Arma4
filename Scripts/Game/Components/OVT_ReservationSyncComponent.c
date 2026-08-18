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
//! proxy-side callback applies the VISUAL half of the reservation locally: VISIBLE and TRACEABLE.
//! ACTIVE is deliberately NOT cleared on proxies; the proxy must keep accepting replication
//! updates, and simulation authority is the server's, which already cleared it.
//!
//! The same vanilla pattern: SCR_ResourceComponent.m_bIsVisible
//! ([RplProp(onRplName: "OnVisibilityChanged")], SCR_ResourceComponent.c:115).
//!
//! WHERE IT LIVES. On every prefab whose instances can be reserved: the player character
//! (Character_Player.et) and the ownable vehicle bases (Wheeled_Base.et, Helicopter_Base.et).
//! An entity without this component just keeps the old authority-only behaviour - Reserve()
//! treats it as optional.
//------------------------------------------------------------------------------------------------
class OVT_ReservationSyncComponent : OVT_Component
{
	[RplProp(onRplName: "OnReservedChanged")]
	protected bool m_bReserved;

	//------------------------------------------------------------------------------------------------
	//! Mirrors the reservation state and broadcasts it. Authority only; the authority's own entity
	//! flags are OVT_PersistenceReservation's business, not this component's.
	//! \param[in] reserved The state Reserve()/Release() just applied.
	void SetReserved(bool reserved)
	{
		if (m_bReserved == reserved)
			return;

		m_bReserved = reserved;
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
	}
}
