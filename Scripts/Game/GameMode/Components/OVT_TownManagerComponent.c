class OVT_TownManagerComponentClass: OVT_ComponentClass
{
};

class OVT_TownData : Managed
{
	int id;
	vector location;
	int population;
	int stability;
	int support;
	int faction;
	int size;
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
	
	protected int m_iTownCount=0;
	
	ref array<ref OVT_TownData> m_Towns;
	protected IEntity m_EntitySearched;
	
	protected OVT_TownData m_CheckTown;
	
	protected ref array<ref EntityID> m_Houses;
	 
	static OVT_TownManagerComponent s_Instance;
	
	static OVT_TownManagerComponent GetInstance()
	{
		if (!s_Instance)
		{
			BaseGameMode pGameMode = GetGame().GetGameMode();
			if (pGameMode)
				s_Instance = OVT_TownManagerComponent.Cast(pGameMode.FindComponent(OVT_TownManagerComponent));
		}

		return s_Instance;
	}
	
	void OVT_TownManagerComponent()
	{
		m_Towns = new array<ref OVT_TownData>;
	}
	
	void Init(IEntity owner)
	{		
		if(!Replication.IsServer()) return;
		InitializeTowns();
	}
	
	IEntity GetRandomHouse()
	{
		m_Houses = new array<ref EntityID>;
		OVT_TownData town = m_Towns.GetRandomElement();
		
		GetGame().GetWorld().QueryEntitiesBySphere(town.location, m_iCityRange, CheckHouseAddToArray, FilterHouseEntities, EQueryEntitiesFlags.STATIC);
		
		return GetGame().GetWorld().FindEntityByID(m_Houses.GetRandomElement());
	}
	
	IEntity GetNearestHouse(vector pos)
	{
		m_Houses = new array<ref EntityID>;		
		GetGame().GetWorld().QueryEntitiesBySphere(pos, 25, CheckHouseAddToArray, FilterHouseEntities, EQueryEntitiesFlags.STATIC);
		
		float nearest = 26;
		IEntity nearestEnt;		
		
		foreach(EntityID id : m_Houses)
		{			
			IEntity ent = GetGame().GetWorld().FindEntityByID(id);
			float dist = vector.Distance(ent.GetOrigin(), pos);
			if(dist < nearest)
			{
				nearest = dist;
				nearestEnt = ent;
			}
		}
		
		return nearestEnt;
	}
	
	IEntity GetRandomHouseInTown(OVT_TownData town)
	{
		m_Houses = new array<ref EntityID>;		
		
		GetGame().GetWorld().QueryEntitiesBySphere(town.location, m_iTownRange, CheckHouseAddToArray, FilterHouseEntities, EQueryEntitiesFlags.STATIC);
		
		return GetGame().GetWorld().FindEntityByID(m_Houses.GetRandomElement());
	}
	
	OVT_TownData GetNearestTown(vector pos)
	{
		OVT_TownData nearestTown;
		float nearest = 9999999;
		foreach(OVT_TownData town : m_Towns)
		{
			float distance = vector.Distance(town.location, pos);
			if(distance < nearest){
				nearest = distance;
				nearestTown = town;
			}
		}
		return nearestTown;
	}
	
	SCR_MapDescriptorComponent GetNearestTownMarker(vector pos)
	{	
		m_EntitySearched = null;	
		GetGame().GetWorld().QueryEntitiesBySphere(pos, 5, null, FindTownMarker, EQueryEntitiesFlags.STATIC);
		if(!m_EntitySearched) return null;
		
		return SCR_MapDescriptorComponent.Cast(m_EntitySearched.FindComponent(SCR_MapDescriptorComponent));
	}
	
	void GetTownsWithinDistance(vector pos, float maxDistance, out array<ref OVT_TownData> towns)
	{
		foreach(OVT_TownData town : m_Towns)
		{
			float distance = vector.Distance(town.location, pos);
			if(distance < maxDistance){
				towns.Insert(town);
			}
		}
	}
	
	protected void InitializeTowns()
	{
		#ifdef OVERTHROW_DEBUG
		Print("Finding cities, towns and villages");
		#endif	
		
		GetGame().GetWorld().QueryEntitiesBySphere("0 0 0", 99999999, CheckCityTownAddPopulation, FilterCityTownEntities, EQueryEntitiesFlags.STATIC);
		GetGame().GetCallqueue().CallLater(SpawnTownControllers, 0);
	}
	
	protected void SpawnTownControllers()
	{
		foreach(OVT_TownData town : m_Towns)
		{
			EntitySpawnParams spawnParams = new EntitySpawnParams;
			spawnParams.TransformMode = ETransformMode.WORLD;		
			spawnParams.Transform[3] = town.location;
			IEntity controller = GetGame().SpawnEntityPrefab(Resource.Load(m_Config.m_pTownControllerPrefab), GetGame().GetWorld(), spawnParams);
		}
	}
	
	protected bool CheckCityTownAddPopulation(IEntity entity)
	{	
		MapDescriptorComponent mapdesc = MapDescriptorComponent.Cast(entity.FindComponent(MapDescriptorComponent));
		if (mapdesc){
			ProcessTown(entity, mapdesc);
		}
		return true;
	}
	
	protected void ProcessTown(IEntity entity, MapDescriptorComponent mapdesc)
	{
		OVT_TownData town = new OVT_TownData();
		
		Faction faction = GetGame().GetFactionManager().GetFactionByKey(m_Config.m_sOccupyingFaction);
			
		town.id = m_iTownCount;
		town.location = entity.GetOrigin();
		town.population = 0;
		town.support = 0;
		town.faction = GetGame().GetFactionManager().GetFactionIndex(faction);
		
		m_iTownCount++;
		
		if(mapdesc.GetBaseType() == EMapDescriptorType.MDT_NAME_VILLAGE) town.size = 1;
		if(mapdesc.GetBaseType() == EMapDescriptorType.MDT_NAME_TOWN) town.size = 2;
		if(mapdesc.GetBaseType() == EMapDescriptorType.MDT_NAME_CITY) town.size = 3;
		
		m_CheckTown = town;
		
		int range = m_iTownRange;
		if(town.size == 1) range = m_iVillageRange;
		if(town.size == 3) range = m_iCityRange;
		
		//Randomize stability
		int stability = 100;
		if(town.size == 3)
		{
			stability = s_AIRandomGenerator.RandFloatXY(90, 100);
		}
		if(town.size == 2)
		{
			stability = s_AIRandomGenerator.RandFloatXY(80, 100);
		}
		if(town.size == 1)
		{
			stability = s_AIRandomGenerator.RandFloatXY(65, 100);
		}
		town.stability = stability;
		
		GetGame().GetWorld().QueryEntitiesBySphere(entity.GetOrigin(), range, CheckHouseAddPopulation, FilterHouseEntities, EQueryEntitiesFlags.STATIC);
		
		#ifdef OVERTHROW_DEBUG
		Print(town.name + ": pop. " + town.population.ToString());
		#endif
		
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
	
	protected bool FindTownMarker(IEntity entity) 
	{		
		bool got = false;
		MapDescriptorComponent mapdesc = MapDescriptorComponent.Cast(entity.FindComponent(MapDescriptorComponent));
		if (mapdesc){	
			int type = mapdesc.GetBaseType();
			if(type == EMapDescriptorType.MDT_NAME_CITY) got = true;
			if(type == EMapDescriptorType.MDT_NAME_VILLAGE) got = true;
			if(type == EMapDescriptorType.MDT_NAME_TOWN) got = true;
		}
		
		if(got)
		{
			m_EntitySearched = entity;
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
	
	protected bool CheckHouseAddToArray(IEntity entity)
	{
		m_Houses.Insert(entity.GetID());
				
		return true;
	}
	
	protected bool FilterHouseEntities(IEntity entity) 
	{
		if(entity.ClassName() == "SCR_DestructibleBuildingEntity"){
			VObject mesh = entity.GetVObject();
			
			if(mesh){
				string res = mesh.GetResourceName();
				if(res.IndexOf("/Military/") > -1) return false;
				if(res.IndexOf("/Industrial/") > -1) return false;
				if(res.IndexOf("/Recreation/") > -1) return false;
				
				if(res.IndexOf("/Houses/") > -1){
					if(res.IndexOf("_ruin") > -1) return false;
					if(res.IndexOf("/Shed/") > -1) return false;
					if(res.IndexOf("/Garage/") > -1) return false;
					if(res.IndexOf("/HouseAddon/") > -1) return false;
					return true;
				}
					
			}
		}
		return false;
	}
	
	//RPC Methods
	
	override bool RplSave(ScriptBitWriter writer)
	{		
		//Send JIP towns
		writer.Write(m_Towns.Count(), 32); 
		for(int i; i<m_Towns.Count(); i++)
		{
			OVT_TownData town = m_Towns[i];
			writer.Write(town.id, 32);
			writer.WriteVector(town.location);
			writer.Write(town.population, 32);
			writer.Write(town.stability, 32);
			writer.Write(town.support, 32);
			writer.Write(town.faction, 32);
			writer.Write(town.size, 32);
		}
		
		return true;
	}
	
	override bool RplLoad(ScriptBitReader reader)
	{				
		//Recieve JIP towns
		int length;
		
		if (!reader.Read(length, 32)) return false;
		for(int i; i<length; i++)
		{
			OVT_TownData town = new OVT_TownData();
			
			if (!reader.Read(town.id, 32)) return false;
			if (!reader.ReadVector(town.location)) return false;		
			if (!reader.Read(town.population, 32)) return false;		
			if (!reader.Read(town.stability, 32)) return false;		
			if (!reader.Read(town.support, 32)) return false;		
			if (!reader.Read(town.faction, 32)) return false;		
			if (!reader.Read(town.size, 32)) return false;		
			
			m_Towns.Insert(town);
		}
		return true;
	}
}