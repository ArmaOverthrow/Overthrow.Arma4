class OVT_OverthrowGameModeClass: SCR_BaseGameModeClass
{
};

class OVT_OverthrowGameMode : SCR_BaseGameMode
{
	[Attribute()]
	ref OVT_UIContext m_StartGameUIContext;
	
	protected OVT_OverthrowConfigComponent m_Config;
	protected OVT_TownManagerComponent m_TownManager;
	protected OVT_OccupyingFactionManager m_OccupyingFactionManager;
	protected OVT_RealEstateManagerComponent m_RealEstate;
	protected OVT_VehicleManagerComponent m_VehicleManager;
	protected OVT_EconomyManagerComponent m_EconomyManager;
	protected OVT_PlayerManagerComponent m_PlayerManager;
	
	ref set<string> m_aInitializedPlayers;
	ref set<string> m_aHintedPlayers;
	
	protected bool m_bGameInitialized = false;
	
	bool IsInitialized()
	{
		return m_bGameInitialized;
	}
	
	void StartGame()
	{
		Rpc(RpcAsk_StartGame);
	}
	
	protected void DoStartGame()
	{		
		if(m_EconomyManager)
		{
			#ifdef OVERTHROW_DEBUG
			Print("Starting Economy");
			#endif
			
			m_EconomyManager.PostGameStart();
		}
				
		if(m_TownManager)
		{
			#ifdef OVERTHROW_DEBUG
			Print("Starting Towns");
			#endif
			
			m_TownManager.PostGameStart();
		}
				
		if(m_OccupyingFactionManager)
		{
			#ifdef OVERTHROW_DEBUG
			Print("Starting Occupying Faction");
			#endif
			
			m_OccupyingFactionManager.PostGameStart();
		}		
		
		#ifdef OVERTHROW_DEBUG
		Print("Allowing player spawn");
		#endif
		m_bGameInitialized = true;
	}
	
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		super.EOnFrame(owner, timeSlice);
		if(m_bGameInitialized) return;
		m_StartGameUIContext.EOnFrame(owner, timeSlice);
	}
	
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
		
		m_Config = OVT_Global.GetConfig();				
		m_PlayerManager = OVT_Global.GetPlayers();		
		m_RealEstate = OVT_Global.GetRealEstate();
		
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
		
		m_StartGameUIContext.Init(owner, null);
		m_StartGameUIContext.RegisterInputs();	
		
		if(!IsMaster()) {
			//show wait screen?
			return;
		}
		
		GetGame().GetTimeAndWeatherManager().SetDayDuration(86400 / m_Config.m_iTimeMultiplier);
		m_PlayerManager.m_OnPlayerRegistered.Insert(OnPlayerIDRegistered);
		
		//Are we a dedicated server?
		if(RplSession.Mode() == RplMode.Dedicated)
		{
			//need to tell someone to open start game menu
		}else{
			m_StartGameUIContext.ShowLayout();
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
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_StartGame()
	{
		DoStartGame();
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