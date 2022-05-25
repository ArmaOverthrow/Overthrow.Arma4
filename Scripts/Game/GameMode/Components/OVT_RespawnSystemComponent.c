//------------------------------------------------------------------------------------------------
class OVT_RespawnSystemComponentClass : SCR_RespawnSystemComponentClass
{
};

//! Scripted implementation that handles spawning and respawning of players.
//! Should be attached to a GameMode entity.
[ComponentEditorProps(icon: HYBRID_COMPONENT_ICON)]
class OVT_RespawnSystemComponent : SCR_RespawnSystemComponent
{
	protected override GenericEntity RequestSpawn(int playerId)
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (gameMode && !gameMode.CanPlayerRespawn(playerId))
		{
			Print("Requested spawn denied! GameMode returned false in CanPlayerRespawn() for playerId=" + playerId, LogLevel.WARNING);
			return null;
		}

		SCR_BasePlayerLoadout loadout = GetPlayerLoadout(playerId);
		if (!loadout)
		{
			Print(LOG_HEAD+" No valid entity to spawn could be returned in RequestSpawn. Are there valid loadouts for the target player faction?", LogLevel.ERROR);
			return null;
		}	
		
		OVT_RealEstateManagerComponent re = OVT_RealEstateManagerComponent.Cast(gameMode.FindComponent(OVT_RealEstateManagerComponent));
		if(!re){
			Print("Real Estate Manager not found. Game Mode entity requires a OVT_RealEstateManagerComponent!");
			return null;
		}
		
		IEntity home = re.GetHome(playerId);
				
		vector spawnPosition = home.GetOrigin();
		vector spawnRotation = vector.Zero;
		
		GenericEntity spawned = DoSpawn(loadout.GetLoadoutResource(), spawnPosition, spawnRotation);
		loadout.OnLoadoutSpawned(spawned, playerId);
		
		return spawned;
	}
}