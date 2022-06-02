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
	
	void Init(IEntity owner)
	{		
		m_iThreat = m_Config.m_Difficulty.baseThreat;
		m_iResources = m_Config.m_Difficulty.startingResources;
		
		InitializeBases();
		
		GetGame().GetCallqueue().CallLater(CheckUpdate, OF_UPDATE_FREQUENCY, true, owner);		
	}
	
	void InitializeBases()
	{
		Print("Finding bases");
		
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
		
		Print("Adding base near " + closestTown.name);
		
		IEntity baseEntity = SpawnBaseController(ent.GetOrigin());
		
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
}