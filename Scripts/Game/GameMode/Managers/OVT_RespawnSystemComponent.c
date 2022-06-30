//------------------------------------------------------------------------------------------------
class OVT_RespawnSystemComponentClass : SCR_RespawnSystemComponentClass
{
};

//! Scripted implementation that handles spawning and respawning of players.
//! Should be attached to a GameMode entity.
[ComponentEditorProps(icon: HYBRID_COMPONENT_ICON)]
class OVT_RespawnSystemComponent : SCR_RespawnSystemComponent
{
	[Attribute(defvalue: "0", desc: "Respawn here only (for testing)")]
	bool m_bSpawnHere;
	IEntity m_Owner;
	
	override void OnInit(IEntity owner)
	{
		super.OnInit(owner);
		m_Owner = owner;
	}
	
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
		
		vector home = re.GetHome(OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId));
		vector spawnPosition = m_Owner.GetOrigin();
		
		if(home[0] != 0){
			spawnPosition = home;
		}else{
			IEntity newhome = OVT_Global.GetTowns().GetRandomStartingHouse();
			spawnPosition = newhome.GetOrigin();
		}
		vector spawnRotation = vector.Zero;
		
		if(m_bSpawnHere) spawnPosition = m_Owner.GetOrigin();
		
		BaseWorld world = GetGame().GetWorld();
		
		spawnPosition = OVT_Global.FindSafeSpawnPosition(spawnPosition);
		
		GenericEntity spawned = DoSpawn(loadout.GetLoadoutResource(), spawnPosition, spawnRotation);
		loadout.OnLoadoutSpawned(spawned, playerId);
		
		return spawned;
	}
	
	override bool CanSetFaction(int playerId, int factionIndex)
	{
		if (factionIndex == SCR_PlayerRespawnInfo.RESPAWN_INFO_INVALID_INDEX)
			return true;

		// Disallow invalid factions
		Faction faction = GetFactionByIndex(factionIndex);
		if (!faction)
			return false;

		SCR_Faction scriptedFaction = SCR_Faction.Cast(faction);	
			
		//Verify that faction is playable Faction. If not then it checks if the spawned player has GM rights
		if (scriptedFaction && !scriptedFaction.IsPlayable())
		{
			SCR_EditorManagerCore core = SCR_EditorManagerCore.Cast(SCR_EditorManagerCore.GetInstance(SCR_EditorManagerCore));
			if (!core)
				return false;
		
			//Check if has GM rights if not return false
			SCR_EditorManagerEntity editorManager = core.GetEditorManager(playerId);
			return editorManager && !editorManager.IsLimited(); 
		}

		return true;
	}
}