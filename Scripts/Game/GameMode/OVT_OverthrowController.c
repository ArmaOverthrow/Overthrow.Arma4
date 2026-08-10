[EntityEditorProps(category: "Overthrow", description: "Controller entity for overthrow-specific client-server communication")]
class OVT_OverthrowControllerClass : GenericEntityClass
{
}

//------------------------------------------------------------------------------------------------
//! Controller entity owned by each player for modular client-server communication.
//! Components attached to this entity handle specific domains of functionality.
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
	//! RPC to notify the owning client about their player ID assignment
	//! This allows the client's PlayerManager to properly map the controller
	[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
	void RpcDo_NotifyOwnerAssignment(int playerId)
	{
		// Update the local player manager's controller mapping
		OVT_PlayerManagerComponent playerManager = OVT_Global.GetPlayers();
		if (playerManager)
		{
			playerManager.RegisterControllerForPlayer(playerId, this);
		}

		// This runs exactly once, on the owning client - the right moment to hook that client's
		// chat up to its own controller's admin commands (server-gated; registration is not a
		// permission).
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