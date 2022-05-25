class OVT_TownManagerComponentClass: OVT_ComponentClass
{
};

class OVT_TownData
{
	EntityID markerID;
	string name;
	int population;
	int stability;
	int support;
	string faction;
}

class OVT_VillageData
{
	EntityID markerID;
	string name;
}

class OVT_TownManagerComponent: OVT_Component
{
	[Attribute( defvalue: "1200", desc: "Range to search cities for houses")]
	int m_iCityRange;
	
	[Attribute( defvalue: "600", desc: "Range to search towns for houses")]
	int m_iTownRange;
	
	[Attribute( defvalue: "250", desc: "Range to search villages for houses")]
	int m_iVillageRange;
	
	[Attribute( defvalue: "2", desc: "Default occupants per house")]
	int m_iDefaultHouseOccupants;
	
	[Attribute( defvalue: "3", desc: "Occupants per villa house")]
	int m_iVillaOccupants;
	
	[Attribute( defvalue: "5", desc: "Occupants per town house")]
	int m_iTownOccupants;
	
	ref array<OVT_TownData> m_Towns;
	ref array<OVT_VillageData> m_Villages;
	
	protected OVT_TownData m_CheckTown;
	
	override void OnPostInit(IEntity owner)
	{	
		super.OnPostInit(owner);
		
		if (SCR_Global.IsEditMode()) return;
		
		InitializeTowns();
	}
	
	protected void InitializeTowns()
	{
		Print("Finding cities, towns and villages");
		
		m_Towns = new array<OVT_TownData>;
		m_Villages = new array<OVT_VillageData>;
		
		
		GetGame().GetWorld().QueryEntitiesBySphere("0 0 0", 99999999, CheckCityTownAddPopulation, FilterCityTownEntities, EQueryEntitiesFlags.STATIC);
		
	}
	
	protected bool CheckCityTownAddPopulation(IEntity entity)
	{	
		MapDescriptorComponent mapdesc = MapDescriptorComponent.Cast(entity.FindComponent(MapDescriptorComponent));
		if (mapdesc){
			if(mapdesc.GetBaseType() == EMapDescriptorType.MDT_NAME_VILLAGE)
			{
				ProcessVillage(entity, mapdesc);
			}else{
				ProcessCityTown(entity, mapdesc);
			}
		}
		return true;
	}
	
	protected void ProcessVillage(IEntity entity, MapDescriptorComponent mapdesc)
	{
		OVT_VillageData village = new OVT_VillageData();
			
		village.markerID = entity.GetID();
		village.name = mapdesc.Item().GetDisplayName();
		
		Print(village.name);
		m_Villages.Insert(village);
	}
	
	protected void ProcessCityTown(IEntity entity, MapDescriptorComponent mapdesc)
	{
		OVT_TownData town = new OVT_TownData();
			
		town.markerID = entity.GetID();
		town.name = mapdesc.Item().GetDisplayName();
		town.population = 0;
		town.stability = 100;
		town.support = 0;
		town.faction = m_Config.m_sOccupyingFaction;
		
		m_CheckTown = town;
		
		int range = m_iTownRange;
		if(mapdesc.GetBaseType() == 59) range = m_iCityRange;
		
		GetGame().GetWorld().QueryEntitiesBySphere(entity.GetOrigin(), range, CheckHouseAddPopulation, FilterHouseEntities, EQueryEntitiesFlags.STATIC);
		
		Print(town.name + ": pop. " + town.population.ToString());
		m_Towns.Insert(town);
	}
	
	protected bool FilterCityTownEntities(IEntity entity) 
	{		
		MapDescriptorComponent mapdesc = MapDescriptorComponent.Cast(entity.FindComponent(MapDescriptorComponent));
		if (mapdesc){	
			int type = mapdesc.GetBaseType();
			if(type == EMapDescriptorType.MDT_NAME_CITY) return true;
			if(type == EMapDescriptorType.MDT_NAME_VILLAGE) return true;
			if(type == EMapDescriptorType.MDT_NAME_TOWN) return true;
		}
				
		return false;		
	}
	
	protected bool CheckHouseAddPopulation(IEntity entity)
	{
		VObject mesh = entity.GetVObject();
		if(mesh){
			string res = mesh.GetResourceName();
			int pop = m_iDefaultHouseOccupants;
			if(res.IndexOf("/Villa/") > -1)
				pop = m_iVillaOccupants;
			if(res.IndexOf("/Town/") > -1)
				pop = m_iTownOccupants;
			
			m_CheckTown.population += pop;
		}
				
		return true;
	}
	
	protected bool FilterHouseEntities(IEntity entity) 
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