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
	//! It runs ONLY on a machine that actually receives an RplRcver.Owner RPC: a remote owning client.
	//! A listen-server host does not receive its own owner-targeted RPCs, and in RplMode.None (single
	//! player) nothing is replicated at all - which is why nothing here may be the ONLY route to any
	//! state it establishes. Both consumers below are caches/registrations with working fallbacks.
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
