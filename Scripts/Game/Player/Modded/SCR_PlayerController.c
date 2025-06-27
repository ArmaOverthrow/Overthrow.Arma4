modded class SCR_PlayerController
{
	//! Client requests server to restore possession after inventory closes
	void RequestRestorePossession()
	{
		int playerId = GetPlayerId();
		Print(string.Format("[SCR_PlayerController] Client: Requesting possession restore for player %1", playerId), LogLevel.NORMAL);
		Rpc(RpcAsk_RestorePossessionOVT, playerId);
	}
	
	//! Server-side RPC handler for restoring possession
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_RestorePossessionOVT(int playerId)
	{
		Print(string.Format("[SCR_PlayerController] Server: Restoring possession for player %1", playerId), LogLevel.NORMAL);
		
		// Get player controller
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (!playerController)
		{
			Print("[SCR_PlayerController] Server: Player controller not found", LogLevel.ERROR);
			return;
		}
		
		IEntity currentPossessed = playerController.GetControlledEntity();
		Print(string.Format("[SCR_PlayerController] Server: Current possessed entity: %1", currentPossessed), LogLevel.NORMAL);
		
		// Restore possession to null (back to original entity)
		playerController.SetPossessedEntity(null);
		
		IEntity restoredEntity = playerController.GetControlledEntity();
		Print(string.Format("[SCR_PlayerController] Server: Restored to entity: %1", restoredEntity), LogLevel.NORMAL);
	}
}