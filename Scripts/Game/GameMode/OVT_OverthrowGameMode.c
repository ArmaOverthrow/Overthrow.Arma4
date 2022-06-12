class OVT_OverthrowGameModeClass: SCR_BaseGameModeClass
{
};

class OVT_OverthrowGameMode : SCR_BaseGameMode
{
	protected OVT_OverthrowConfigComponent m_Config;
	protected OVT_TownManagerComponent m_TownManager;
	protected OVT_OccupyingFactionManager m_OccupyingFactionManager;
	protected OVT_VehicleManagerComponent m_VehicleManager;
	protected OVT_EconomyManagerComponent m_EconomyManager;
	
	ref set<string> m_aInitializedPlayers;
	ref set<string> m_aHintedPlayers;
	
	protected override void OnPlayerSpawned(int playerId, IEntity controlledEntity)
	{
		super.OnPlayerSpawned(playerId, controlledEntity);
		
		string persId = OVT_PlayerIdentityComponent.GetPersistentIDFromPlayerID(playerId);
		
		if(m_aInitializedPlayers.Contains(persId))
		{
			//Existing player
			//(note this playerId isnt persistent between connections until we get a steam ID or something)
			int cost = OVT_OverthrowConfigComponent.GetInstance().m_Difficulty.respawnCost;
			OVT_EconomyManagerComponent.GetInstance().TakePlayerMoney(persId, cost);
		}else{
			int cash = OVT_OverthrowConfigComponent.GetInstance().m_Difficulty.startingCash;
			OVT_EconomyManagerComponent.GetInstance().AddPlayerMoney(persId, cash);
			
			m_aInitializedPlayers.Insert(persId);
		}
		
		OVT_PlayerWantedComponent wanted = OVT_PlayerWantedComponent.Cast(controlledEntity.FindComponent(OVT_PlayerWantedComponent));
		if(!wanted){
			Print("Player spawn prefab is missing OVT_PlayerWantedComponent!");
		}else{
			wanted.SetWantedLevel(0);
		}
		
		FactionAffiliationComponent faction = FactionAffiliationComponent.Cast(controlledEntity.FindComponent(FactionAffiliationComponent));
		if(!faction){
			Print("Player spawn prefab is missing FactionAffiliationComponent!");
		}else{
			faction.SetAffiliatedFactionByKey("");
		}
		
		#ifdef OVERTHROW_DEBUG
		Print("Player spawned");
		#endif
	}
	
	override void EOnInit(IEntity owner) //!EntityEvent.INIT
	{
		super.EOnInit(owner);
		
		if(SCR_Global.IsEditMode())
			return;		
		
		#ifdef OVERTHROW_DEBUG
		Print("Initializing Overthrow");
		#endif
		
		m_Config = OVT_OverthrowConfigComponent.GetInstance();
		
		GetGame().GetTimeAndWeatherManager().SetDayDuration(86400 / m_Config.m_iTimeMultiplier);
		
		m_EconomyManager = OVT_EconomyManagerComponent.Cast(FindComponent(OVT_EconomyManagerComponent));		
		if(m_EconomyManager)
		{
			#ifdef OVERTHROW_DEBUG
			Print("Initializing Economy");
			#endif
			
			m_EconomyManager.Init(this);
		}
		
		m_TownManager = OVT_TownManagerComponent.Cast(FindComponent(OVT_TownManagerComponent));		
		if(m_TownManager)
		{
			#ifdef OVERTHROW_DEBUG
			Print("Initializing Towns");
			#endif
			
			m_TownManager.Init(this);
		}
		
		m_OccupyingFactionManager = OVT_OccupyingFactionManager.Cast(FindComponent(OVT_OccupyingFactionManager));		
		if(m_OccupyingFactionManager)
		{
			#ifdef OVERTHROW_DEBUG
			Print("Initializing Occupying Faction");
			#endif
			
			m_OccupyingFactionManager.Init(this);
		}
		
		m_VehicleManager = OVT_VehicleManagerComponent.Cast(FindComponent(OVT_VehicleManagerComponent));		
		if(m_VehicleManager)
		{
			#ifdef OVERTHROW_DEBUG
			Print("Initializing Vehicles");
			#endif
			
			m_VehicleManager.Init(this);
		}
		
	}
	
	void OnPlayerSpawnedLocal(string playerId)
	{
		if(!m_aHintedPlayers.Contains(playerId))
		{
			SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-IntroHint","#OVT-Overthrow",20);
			m_aHintedPlayers.Insert(playerId);
		}		
	}
	
	//------------------------------------------------------------------------------------------------
	void OVT_OverthrowGameMode(IEntitySource src, IEntity parent)
	{
		m_aInitializedPlayers = new set<string>;
		m_aHintedPlayers = new set<string>;
	}
}