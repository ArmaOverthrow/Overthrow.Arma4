class OVT_OccupyingFactionManagerClass: OVT_ComponentClass
{
	override bool DependsOn(string className)
	{
		if(className == "OVT_TownManagerComponent") return true;
		
		return false;
	}
}

class OVT_OccupyingFactionManager: OVT_Component
{	
	int m_iResources;
	float m_iThreat;
	ref array<ref EntityID> m_Bases;
	
	const int OF_UPDATE_FREQUENCY = 60000;
	
	static OVT_OccupyingFactionManager s_Instance;
	
	static OVT_OccupyingFactionManager GetInstance()
	{
		if (!s_Instance)
		{
			BaseGameMode pGameMode = GetGame().GetGameMode();
			if (pGameMode)
				s_Instance = OVT_OccupyingFactionManager.Cast(pGameMode.FindComponent(OVT_OccupyingFactionManager));
		}

		return s_Instance;
	}
	
	void Init(IEntity owner)
	{		
		m_iThreat = m_Config.m_Difficulty.baseThreat;
		m_iResources = m_Config.m_Difficulty.startingResources;
		
		InitializeBases();
		
		GetGame().GetCallqueue().CallLater(CheckUpdate, OF_UPDATE_FREQUENCY, true, owner);		
		
	}
	
	OVT_BaseControllerComponent GetNearestBase(vector pos)
	{
		IEntity nearestBase;
		float nearest = 9999999;
		foreach(EntityID id : m_Bases)
		{
			IEntity marker = GetGame().GetWorld().FindEntityByID(id);
			float distance = vector.Distance(marker.GetOrigin(), pos);
			if(distance < nearest){
				nearest = distance;
				nearestBase = marker;
			}
		}
		if(!nearestBase) return null;
		return OVT_BaseControllerComponent.Cast(nearestBase.FindComponent(OVT_BaseControllerComponent));
	}
	
	void GetBasesWithinDistance(vector pos, float maxDistance, out array<OVT_BaseControllerComponent> bases)
	{
		foreach(EntityID id : m_Bases)
		{
			IEntity marker = GetGame().GetWorld().FindEntityByID(id);
			float distance = vector.Distance(marker.GetOrigin(), pos);
			if(distance < maxDistance){
				OVT_BaseControllerComponent base = OVT_BaseControllerComponent.Cast(marker.FindComponent(OVT_BaseControllerComponent));
				bases.Insert(base);
			}
		}
	}
	
	void RecoverResources(int resources)
	{
		m_iResources += resources;
	}
	
	void InitializeBases()
	{
		#ifdef OVERTHROW_DEBUG
		Print("Finding bases");
		#endif
		
		m_Bases = new array<ref EntityID>;		
		
		GetGame().GetWorld().QueryEntitiesBySphere("0 0 0", 99999999, CheckBaseAdd, FilterBaseEntities, EQueryEntitiesFlags.STATIC);
		
		//Distribute initial resources
		
		foreach(EntityID id : m_Bases)
		{
			OVT_BaseControllerComponent base = GetBase(id);
			m_iResources -= base.SpendResources(m_Config.m_Difficulty.initialResourcesPerBase, m_iThreat);			
			
			if(m_iResources <= 0) break;
		}
	}
	
	OVT_BaseControllerComponent GetBase(EntityID id)
	{
		return OVT_BaseControllerComponent.Cast(GetGame().GetWorld().FindEntityByID(id).FindComponent(OVT_BaseControllerComponent));
	}
	
	bool CheckBaseAdd(IEntity ent)
	{
		OVT_TownManagerComponent townManager = OVT_TownManagerComponent.GetInstance();
		
		OVT_TownData closestTown = townManager.GetNearestTown(ent.GetOrigin());
		
		#ifdef OVERTHROW_DEBUG
		Print("Adding base near " + closestTown.name);
		#endif
		
		IEntity baseEntity = SpawnBaseController(ent.GetOrigin());
		
		OVT_BaseControllerComponent base = OVT_BaseControllerComponent.Cast(baseEntity.FindComponent(OVT_BaseControllerComponent));

		m_Bases.Insert(baseEntity.GetID());
		
		return true;
	}
	
	IEntity SpawnBaseController(vector loc)
	{
		EntitySpawnParams params = EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = loc;
		return GetGame().SpawnEntityPrefabLocal(Resource.Load(m_Config.m_pBaseControllerPrefab), null, params));
		
	}
	
	bool FilterBaseEntities(IEntity entity)
	{
		MapDescriptorComponent mapdesc = MapDescriptorComponent.Cast(entity.FindComponent(MapDescriptorComponent));
		if (mapdesc){	
			int type = mapdesc.GetBaseType();
			if(type == EMapDescriptorType.MDT_NAME_GENERIC) {				
				if(mapdesc.Item().GetDisplayName() == "#AR-MapLocation_Military") return true;
			}
		}
				
		return false;	
	}
	
	void CheckUpdate()
	{
		TimeContainer time = m_Time.GetTime();		
		
		//Every 6 hrs distribute resources
		if((time.m_iHours == 0 
			|| time.m_iHours == 6 
			|| time.m_iHours == 12 
			|| time.m_iHours == 18)
			 && 
			time.m_iMinutes == 0)
		{
			DistributeResources();
		}
	}
	
	void DistributeResources()
	{
		
		int newResources = m_Config.m_Difficulty.baseResourcesPerTick + (m_Config.m_Difficulty.resourcesPerTick * m_iThreat);
		
		m_iResources += newResources;
		
		Print ("OF Distributing Resources: " + newResources.ToString());
	}
	
	void ~OVT_OccupyingFactionManager()
	{
		GetGame().GetCallqueue().Remove(CheckUpdate);				
	}
}