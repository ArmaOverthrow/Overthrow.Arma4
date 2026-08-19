[EntityEditorProps(category: "Overthrow", description: "Controller entity for overthrow-specific client-server communication")]
class OVT_OverthrowControllerClass : GenericEntityClass
{
}

//------------------------------------------------------------------------------------------------
//! Controller entity owned by each player for modular client-server communication.
//! Components attached to this entity handle specific domains of functionality.
//!
//! Reach a component on the LOCAL player's controller with
//! OVT_ControllerComponent<OVT_SomeComponent>.Get() - never with a new OVT_Global getter.
class OVT_OverthrowController : GenericEntity
{
	// Progress event handler (invoked by all components extending OVT_BaseServerProgressComponent)
	protected ref OVT_ProgressEventHandler m_ProgressEvents = new OVT_ProgressEventHandler();

	//------------------------------------------------------------------------------------------------
	//! Get the progress event handler
	OVT_ProgressEventHandler GetProgressEvents()
	{
		return m_ProgressEvents;
	}

	//------------------------------------------------------------------------------------------------
	//! Tells the owning client which player this controller belongs to.
	//!
	//! ! CONTRACT: THIS FIRES ONCE PER OWNERSHIP **ASSIGNMENT**, WHICH IS NOT ONCE PER PLAYER.
	//! OVT_PlayerManagerComponent.AssignControllerOwnership() sends it unconditionally, and SetupPlayer()
	//! calls that on BOTH of its branches - the fresh-spawn branch and the already-mapped branch a
	//! reconnect or a Continue takes (OVT_PlayerManagerComponent.c:653 and :671). It is also sent when
	//! the GiveExt() above it did NOT happen (invalid RplIdentity, missing RplComponent, PlayerController
	//! not ready), because the notify block sits outside those guards.
	//!
	//! EVERY CONSUMER MUST THEREFORE BE IDEMPOTENT. Running twice must produce the same state as running
	//! once - a plain field write is fine, an Insert() into an invoker is not (that is why
	//! OVT_AdminCommandsComponent.RegisterChatCommands() carries a guard flag). Do not "fix" the
	//! double-fire by trying to make this fire once: a reconnecting player genuinely needs the
	//! re-assignment, and the second delivery is the only thing that re-establishes their client state.
	//!
	//! WHERE IT ACTUALLY RUNS - corrected 2026-08-19, this used to say "a remote owning client ONLY".
	//! It runs on whichever machine OWNS this controller, and on a listen host or in single player that
	//! is the SENDING machine itself. The engine's own routing table settles it (ArmaReforger
	//! scripts/GameLib/replication/RplDocs.c:549-557, and the worked example at :1844-1853): for an RPC
	//! invoked on the server, RplRcver.Owner is delivered "On Client Owner" when some client owns the
	//! item, but "On Server" - i.e. invoked directly, as a plain method call - when the SERVER is the
	//! owner. NotifyOwnerAssignment() sends it right after GiveExt() hands ownership to the target
	//! player's RplIdentity, which on a host/SP session is the local identity.
	//!
	//! OBSERVED: in a Workbench single-player play-test the admin chat commands registered by the third
	//! consumer below exist and work (user report, 2026-08-19) - which is only possible if this body
	//! ran. INFERRED from the same table, not yet observed: the identical thing on a listen host.
	//!
	//! (That third consumer now refuses to register in a SHIPPED offline session - a product rule, not
	//! a replication one, enforced inside OVT_AdminCommandsComponent.RegisterChatCommands() rather than
	//! here. This handler still runs there; the component simply declines. Do not read a missing
	//! "/give-money" in a released single-player game as a delivery failure of this RPC.)
	//!
	//! Keep every consumer here idempotent and non-exclusive anyway. Nothing about the above guarantees
	//! delivery on a machine that is NOT the owner, so state that other machines need still has to have
	//! another route (the two caches below both do; see their notes).
	//! \param[in] playerId The runtime player id this controller was assigned to.
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	void RpcDo_NotifyOwnerAssignment(int playerId)
	{
		// Update the local player manager's controller mapping
		OVT_PlayerManagerComponent playerManager = OVT_Global.GetPlayers();
		if (playerManager)
		{
			playerManager.RegisterControllerForPlayer(playerId, this);
		}

		// Fast path for OVT_Global.GetController(). Skipped on a genuine id mismatch (this controller
		// belongs to somebody else) because the cache, unlike the playerId-keyed map above, has no key
		// to be wrong under - a mis-cached controller would be returned for every local request. An
		// unknown local id (<= 0, the client has not been told yet) is NOT a mismatch and still caches.
		int localPlayerId = SCR_PlayerController.GetLocalPlayerId();
		if (localPlayerId <= 0 || localPlayerId == playerId)
		{
			OVT_Global.SetLocalController(this);
		}

		// Hook this client's chat up to its own controller's admin commands (server-gated; registration
		// is not a permission). Idempotent on the component's side - see the contract note above.
		OVT_AdminCommandsComponent adminCommands = OVT_AdminCommandsComponent.Cast(FindComponent(OVT_AdminCommandsComponent));
		if (adminCommands)
			adminCommands.RegisterChatCommands();
	}

	//------------------------------------------------------------------------------------------------
	//! Notify owner about their assignment (wrapper for RPC call)
	void NotifyOwnerAssignment(int playerId)
	{
		Rpc(RpcDo_NotifyOwnerAssignment, playerId);
	}
}
