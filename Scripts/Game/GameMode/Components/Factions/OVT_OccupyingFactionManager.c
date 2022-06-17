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
	ref array<RplId> m_Bases;
	ref array<vector> m_BasesToSpawn;
	
	const int OF_UPDATE_FREQUENCY = 60000;
	
	ref ScriptInvoker<IEntity> m_OnAIKilled = new ScriptInvoker<IEntity>;
	
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
	
	void OVT_OccupyingFactionManager()
	{
		m_Bases = new array<RplId>;	
		m_BasesToSpawn = new array<vector>;	
	}
	
	void Init(IEntity owner)
	{	
		if(!Replication.IsServer()) return;
			
		m_iThreat = m_Config.m_Difficulty.baseThreat;
		m_iResources = m_Config.m_Difficulty.startingResources;
		
		InitializeBases();		
	}
	
	void PostGameStart()
	{
		GetGame().GetCallqueue().CallLater(CheckUpdate, OF_UPDATE_FREQUENCY, true, GetOwner());
		GetGame().GetCallqueue().CallLater(SpawnBaseControllers, 0);
	}
	
	OVT_BaseControllerComponent GetNearestBase(vector pos)
	{
		IEntity nearestBase;
		float nearest = 9999999;
		foreach(RplId id : m_Bases)
		{
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
			IEntity marker = rpl.GetEntity();
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
		foreach(RplId id : m_Bases)
		{
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
			IEntity marker = rpl.GetEntity();
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
		
		GetGame().GetWorld().QueryEntitiesBySphere("0 0 0", 99999999, CheckBaseAdd, FilterBaseEntities, EQueryEntitiesFlags.STATIC);
	}
	
	protected void SpawnBaseControllers()
	{
		foreach(vector pos : m_BasesToSpawn)
		{		
			IEntity baseEntity = SpawnBaseController(pos);
			
			OVT_BaseControllerComponent base = OVT_BaseControllerComponent.Cast(baseEntity.FindComponent(OVT_BaseControllerComponent));
			
			m_Bases.Insert(base.GetRpl().Id());
		}
		m_BasesToSpawn.Clear();
		m_BasesToSpawn = null;
		GetGame().GetCallqueue().CallLater(DistributeInitialResources, 100);
	}
	
	protected void DistributeInitialResources()
	{
		//Distribute initial resources		
		foreach(RplId id : m_Bases)
		{
			OVT_BaseControllerComponent base = GetBase(id);
			m_iResources -= base.SpendResources(m_Config.m_Difficulty.initialResourcesPerBase, m_iThreat);			
			
			if(m_iResources <= 0) break;
		}
	}
	
	OVT_BaseControllerComponent GetBase(RplId id)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
		IEntity marker = rpl.GetEntity();
		return OVT_BaseControllerComponent.Cast(marker.FindComponent(OVT_BaseControllerComponent));
	}
	
	bool CheckBaseAdd(IEntity ent)
	{		
		#ifdef OVERTHROW_DEBUG
		OVT_TownManagerComponent townManager = OVT_Global.GetTowns();
		OVT_TownData closestTown = townManager.GetNearestTown(ent.GetOrigin());
		Print("Adding base near " + closestTown.name);
		#endif
		
		m_BasesToSpawn.Insert(ent.GetOrigin());		
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
		
		//Every 6 hrs get more resources
		if((time.m_iHours == 0 
			|| time.m_iHours == 6 
			|| time.m_iHours == 12 
			|| time.m_iHours == 18)
			 && 
			time.m_iMinutes == 0)
		{
			GainResources();
		}
		
		//Every hour distribute resources we have, if any
		if(m_iResources > 0 && time.m_iMinutes == 0)
		{
			//To-Do: prioritize bases that need it/are under threat
			foreach(RplId id : m_Bases)
			{
				OVT_BaseControllerComponent base = GetBase(id);
				m_iResources -= base.SpendResources(m_iResources, m_iThreat);			
				
				if(m_iResources <= 0) {
					m_iResources = 0;
				}
			}
		}
	}
	
	void GainResources()
	{
		
		int newResources = m_Config.m_Difficulty.baseResourcesPerTick + (m_Config.m_Difficulty.resourcesPerTick * m_iThreat);
		
		m_iResources += newResources;
		
		Print ("OF Gained Resources: " + newResources.ToString());			
	}
	
	//RPC Methods
	
	override bool RplSave(ScriptBitWriter writer)
	{		
		//Send JIP bases
		writer.Write(m_Bases.Count(), 32); 
		for(int i; i<m_Bases.Count(); i++)
		{
			writer.WriteRplId(m_Bases[i]);
		}
		
		return true;
	}
	
	override bool RplLoad(ScriptBitReader reader)
	{				
		//Recieve JIP towns
		int length;
		RplId id;
		
		if (!reader.Read(length, 32)) return false;
		for(int i; i<length; i++)
		{			
			if (!reader.ReadRplId(id)) return false;
			m_Bases.Insert(id);
		}
		return true;
	}
	
	void ~OVT_OccupyingFactionManager()
	{
		GetGame().GetCallqueue().Remove(CheckUpdate);	
		
		if(m_Bases)
		{
			m_Bases.Clear();
			m_Bases = null;
		}			
	}
}