class OVT_OverthrowGameModeClass: SCR_BaseGameModeClass
{
};

class OVT_OverthrowGameMode : SCR_BaseGameMode
{
	protected OVT_OverthrowConfigComponent m_Config;
	protected OVT_TownManagerComponent m_TownManager;
	protected OVT_OccupyingFactionManager m_OccupyingFactionManager;
	protected OVT_EconomyManagerComponent m_EconomyManager;
	
	ref array<int> m_aInitializedPlayers;
	
	protected override void OnPlayerSpawned(int playerId, IEntity controlledEntity)
	{
		super.OnPlayerSpawned(playerId, controlledEntity);
		
		if(m_aInitializedPlayers.Contains(playerId))
		{
			//Existing player
			//(note this playerId isnt persistent between connections until we get a steam ID or something)
			int cost = OVT_OverthrowConfigComponent.GetInstance().m_Difficulty.respawnCost;
			OVT_EconomyManagerComponent.GetInstance().TakePlayerMoney(playerId, cost);
		}else{
			int cash = OVT_OverthrowConfigComponent.GetInstance().m_Difficulty.startingCash;
			OVT_EconomyManagerComponent.GetInstance().AddPlayerMoney(playerId, cash);
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
		
		Print("Player spawned");
	}
	
	override void EOnInit(IEntity owner) //!EntityEvent.INIT
	{
		super.EOnInit(owner);
		
		if(SCR_Global.IsEditMode())
			return;		
		
		Print("Initializing Overthrow");
		
		m_Config = OVT_OverthrowConfigComponent.GetInstance();
		
		GetGame().GetTimeAndWeatherManager().SetDayDuration(86400 / m_Config.m_iTimeMultiplier);
		
		m_TownManager = OVT_TownManagerComponent.Cast(FindComponent(OVT_TownManagerComponent));		
		if(m_TownManager)
		{
			Print("Initializing Towns");
			m_TownManager.Init(this);
		}
		
		m_OccupyingFactionManager = OVT_OccupyingFactionManager.Cast(FindComponent(OVT_OccupyingFactionManager));		
		if(m_OccupyingFactionManager)
		{
			Print("Initializing Occupying Faction");
			m_OccupyingFactionManager.Init(this);
		}
		
		m_EconomyManager = OVT_EconomyManagerComponent.Cast(FindComponent(OVT_EconomyManagerComponent));		
		if(m_EconomyManager)
		{
			Print("Initializing Economy");
			m_EconomyManager.Init(this);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	void OVT_OverthrowGameMode(IEntitySource src, IEntity parent)
	{
		m_aInitializedPlayers = new array<int>;
	}
}