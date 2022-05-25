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

class OVT_TownManagerComponent: OVT_Component
{
	[Attribute( defvalue: "1200", desc: "Range to search cities for houses")]
	int m_iCityRange;
	
	[Attribute( defvalue: "600", desc: "Range to search towns for houses")]
	int m_iTownRange;
	
	ref array<OVT_TownData> m_Towns;
	protected OVT_TownData m_CheckTown;
	
	override void OnPostInit(IEntity owner)
	{	
		super.OnPostInit(owner);
		
		if (SCR_Global.IsEditMode()) return;
		
		InitializeTowns();
	}
	
	protected void InitializeTowns()
	{
		Print("Finding Cities and Towns");
		
		m_Towns = new array<OVT_TownData>;
		
		GetGame().GetWorld().QueryEntitiesBySphere("0 0 0", 99999999, CheckCityTownAddPopulation, FilterCityTownEntities, EQueryEntitiesFlags.STATIC);
		
	}
	
	protected bool CheckCityTownAddPopulation(IEntity entity)
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
			
			int range = m_iTownRange;
			if(mapdesc.GetBaseType() == 59) range = m_iCityRange;
			
			GetGame().GetWorld().QueryEntitiesBySphere(entity.GetOrigin(), range, CheckHouseAddPopulation, FilterHouseEntities, EQueryEntitiesFlags.STATIC);
			
			Print(mapdesc.Item().GetDisplayName() + ": pop. " + town.population.ToString());
			m_Towns.Insert(town);
		}
		return true;
	}
	
	protected bool FilterCityTownEntities(IEntity entity) 
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
	
	protected bool CheckHouseAddPopulation(IEntity entity)
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