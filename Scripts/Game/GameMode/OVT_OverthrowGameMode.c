class OVT_OverthrowGameModeClass: SCR_BaseGameModeClass
{
};

class OVT_OverthrowGameMode : SCR_BaseGameMode
{
	protected OVT_OverthrowConfigComponent m_Config;
	protected OVT_TownManagerComponent m_TownManager;
	protected OVT_OccupyingFactionManager m_OccupyingFactionManager;
	protected OVT_RealEstateManagerComponent m_RealEstate;
	protected OVT_VehicleManagerComponent m_VehicleManager;
	protected OVT_EconomyManagerComponent m_EconomyManager;
	protected OVT_PlayerManagerComponent m_PlayerManager;
	
	ref set<string> m_aInitializedPlayers;
	ref set<string> m_aHintedPlayers;
	
	protected void OnPlayerIDRegistered(int playerId, string persistentId)
	{
		IEntity controlledEntity = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		
		if(m_aInitializedPlayers.Contains(persistentId))
		{
			//Existing player			
			int cost = m_Config.m_Difficulty.respawnCost;
			m_EconomyManager.TakePlayerMoney(playerId, cost);
		}else{
			//New player
			int cash = m_Config.m_Difficulty.startingCash;
			m_EconomyManager.AddPlayerMoney(playerId, cash);
			
			IEntity home = m_RealEstate.GetHome(persistentId);
			if(!home)
			{
				//spawn system already assigned them a house, so make nearest house their home
				IEntity house = m_TownManager.GetNearestHouse(controlledEntity.GetOrigin());
				m_RealEstate.SetOwner(playerId, house);
				m_RealEstate.SetHome(playerId, house);
				m_VehicleManager.SpawnStartingCar(house, persistentId);
			}
			
			m_aInitializedPlayers.Insert(persistentId);
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
		
		if(!Replication.IsServer()) return;
		
		GetGame().GetTimeAndWeatherManager().SetDayDuration(86400 / m_Config.m_iTimeMultiplier);
		
		m_PlayerManager = OVT_PlayerManagerComponent.GetInstance();		
		m_PlayerManager.m_OnPlayerRegistered.Insert(OnPlayerIDRegistered);
		
		m_RealEstate = OVT_RealEstateManagerComponent.GetInstance();
		
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
	
	void ~OVT_OverthrowGameMode()
	{
		if(!m_PlayerManager) return;
		m_PlayerManager.m_OnPlayerRegistered.Remove(OnPlayerRegistered);
	}
}