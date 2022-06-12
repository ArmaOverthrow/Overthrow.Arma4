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
		
		IEntity home = re.GetHome(OVT_PlayerIdentityComponent.GetPersistentIDFromPlayerID(playerId));
				
		vector spawnPosition = home.GetOrigin();
		vector spawnRotation = vector.Zero;
		
		if(m_bSpawnHere) spawnPosition = m_Owner.GetOrigin();
		
		BaseWorld world = GetGame().GetWorld();
		
		//Snap to the nearest navmesh point
		AIPathfindingComponent pathFindindingComponent = AIPathfindingComponent.Cast(this.FindComponent(AIPathfindingComponent));
		if (pathFindindingComponent && pathFindindingComponent.GetClosestPositionOnNavmesh(spawnPosition, "10 10 10", spawnPosition))
		{
			float groundHeight = world.GetSurfaceY(spawnPosition[0], spawnPosition[2]);
			if (spawnPosition[1] < groundHeight)
				spawnPosition[1] = groundHeight;
		}
		
		GenericEntity spawned = DoSpawn(loadout.GetLoadoutResource(), spawnPosition, spawnRotation);
		loadout.OnLoadoutSpawned(spawned, playerId);
		
		return spawned;
	}
}