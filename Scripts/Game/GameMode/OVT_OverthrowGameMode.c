class OVT_OverthrowGameModeClass: SCR_BaseGameModeClass
{
};

class OVT_OverthrowGameMode : SCR_BaseGameMode
{
	protected OVT_OverthrowConfigComponent m_Config;
	ref array<OVT_TownData> m_Towns;
	protected OVT_TownData m_CheckTown;
	
	protected const int CITY_RANGE = 1200;
	protected const int TOWN_RANGE = 600;
	
	protected override void OnPlayerSpawned(int playerId, IEntity controlledEntity)
	{
		super.OnPlayerSpawned(playerId, controlledEntity);
		
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
	
	override void StartGameMode()
	{
		super.StartGameMode();
		
		Print("Initializing Overthrow");
		
		m_Config = OVT_OverthrowConfigComponent.GetInstance();
		
		InitializeTowns();
	}
	
	void InitializeTowns()
	{
		Print("Finding Cities and Towns");
		
		m_Towns = new array<OVT_TownData>;
		
		GetGame().GetWorld().QueryEntitiesBySphere("0 0 0", 99999999, CheckCityTownEntity, FilterCityTownEntities, EQueryEntitiesFlags.STATIC);
		
	}
	
	bool CheckCityTownEntity(IEntity entity)
	{	
		MapDescriptorComponent mapdesc = MapDescriptorComponent.Cast(entity.FindComponent(MapDescriptorComponent));
		if (mapdesc){
			
			OVT_TownData town = new OVT_TownData();
			
			town.markerID = entity.GetID();
			town.name = mapdesc.Item().GetDisplayName();
			town.population = 0;
			town.stability = 100;
			town.support = 0;
			town.faction = m_Config.m_sOccupyingFaction;
			
			m_CheckTown = town;
			
			int range = TOWN_RANGE;
			if(mapdesc.GetBaseType() == 59) range = CITY_RANGE;
			
			GetGame().GetWorld().QueryEntitiesBySphere(entity.GetOrigin(), range, CheckHouseEntity, FilterHouseEntities, EQueryEntitiesFlags.STATIC);
			
			Print(mapdesc.Item().GetDisplayName() + ": pop. " + town.population.ToString());
			m_Towns.Insert(town);
		}
		return true;
	}
	
	bool FilterCityTownEntities(IEntity entity) 
	{
		// City: 59
		// Village: 60
		// Town: 61
		
		MapDescriptorComponent mapdesc = MapDescriptorComponent.Cast(entity.FindComponent(MapDescriptorComponent));
		if (mapdesc){	
			int type = mapdesc.GetBaseType();
			if(type == 59) return true;
			if(type == 61) return true;
		}
				
		return false;		
	}
	
	bool CheckHouseEntity(IEntity entity)
	{
		VObject mesh = entity.GetVObject();
		if(mesh){
			string res = mesh.GetResourceName();
			int pop = 2;
			if(res.IndexOf("/Villa/") > -1)
				pop = 3;
			if(res.IndexOf("/Town/") > -1)
				pop = 5;
			
			m_CheckTown.population += pop;
		}
				
		return true;
	}
	
	bool FilterHouseEntities(IEntity entity) 
	{
		if(entity.ClassName() == "SCR_DestructibleBuildingEntity"){
			VObject mesh = entity.GetVObject();
			
			if(mesh){
				string res = mesh.GetResourceName();
				if(res.IndexOf("/Houses/") > -1)
					return true;
			}
		}
		return false;
	}
}

class OVT_TownData
{
	EntityID markerID;
	string name;
	int population;
	int stability;
	int support;
	string faction;
}