class OVT_BaseControllerComponentClass: OVT_ComponentClass
{
};

class OVT_BaseControllerComponent: OVT_Component
{
	[Attribute(defvalue: "0", UIWidgets.EditBox, desc: "Resources to allocate for testing only")]
	int m_iTestingResources;
	
	[Attribute(defvalue: "200", UIWidgets.EditBox, desc: "Resources to allocate to defense")]
	int m_iDefenseAllocation;
	
	int m_iRange = 250;
	int m_iCloseRange = 150;
	
	string m_sName;
	ref array<ref EntityID> m_DefenseGroups;
	ref array<ref EntityID> m_ReserveGroups;
	
	ref array<ref EntityID> m_AllSlots;
	ref array<ref EntityID> m_AllCloseSlots;
	ref array<ref EntityID> m_SmallSlots;
	ref array<ref EntityID> m_MediumSlots;
	ref array<ref EntityID> m_LargeSlots;
	ref array<ref EntityID> m_SmallRoadSlots;
	ref array<ref EntityID> m_MediumRoadSlots;
	
	
	override void OnPostInit(IEntity owner)
	{	
		super.OnPostInit(owner);
		
		if (SCR_Global.IsEditMode()) return;
		
		InitializeBase();
	}
	
	void InitializeBase()
	{
		m_DefenseGroups = new array<ref EntityID>;
		m_ReserveGroups = new array<ref EntityID>;
		
		m_AllSlots = new array<ref EntityID>;
		m_AllCloseSlots = new array<ref EntityID>;
		m_SmallSlots = new array<ref EntityID>;
		m_MediumSlots = new array<ref EntityID>;
		m_LargeSlots = new array<ref EntityID>;
		m_SmallRoadSlots = new array<ref EntityID>;
		m_MediumRoadSlots = new array<ref EntityID>;
		
		FindSlots();
		
		if(m_iTestingResources > 0){
			SpendResources(m_iTestingResources);
		}
	}
	
	void FindSlots()
	{
		GetGame().GetWorld().QueryEntitiesBySphere(GetOwner().GetOrigin(), m_iRange, CheckSlotAddToArray, FilterSlotEntities, EQueryEntitiesFlags.STATIC);
	}
	
	bool FilterSlotEntities(IEntity entity)
	{
		if(entity.ClassName() == "SCR_SiteSlotEntity") return true;
		return false;
	}
	
	bool CheckSlotAddToArray(IEntity entity)
	{
		m_AllSlots.Insert(entity.GetID());
		
		float distance = vector.Distance(entity.GetOrigin(), GetOwner().GetOrigin());
		if(distance < m_iCloseRange)
		{
			m_AllCloseSlots.Insert(entity.GetID());
		}
		
		string name = entity.GetPrefabData().GetPrefabName();
		if(name.IndexOf("FlatSmall") > -1) m_SmallSlots.Insert(entity.GetID());
		if(name.IndexOf("FlatMedium") > -1) m_MediumSlots.Insert(entity.GetID());
		if(name.IndexOf("FlatLarge") > -1) m_LargeSlots.Insert(entity.GetID());
		if(name.IndexOf("RoadSmall") > -1) m_SmallRoadSlots.Insert(entity.GetID());
		if(name.IndexOf("RoadMedium") > -1) m_MediumRoadSlots.Insert(entity.GetID());
		
		return true;
	}
	
	int SpendResources(int resources, float threat = 0)
	{
		int spent = 0;
		
		int defenseRes = GetDefenseResources();
		int reserveRes = GetReserveResources();
		
		OVT_Faction faction = m_Config.GetOccupyingFaction();
		BaseWorld world = GetGame().GetWorld();
		
		while(defenseRes < m_iDefenseAllocation && resources > 0)
		{
			
			ResourceName res = faction.m_aGroupInfantryPrefabSlots.GetRandomElement();
			
			EntitySpawnParams spawnParams = new EntitySpawnParams;
			spawnParams.TransformMode = ETransformMode.WORLD;
			
			vector pos = GetOwner().GetOrigin();
			
			float surfaceY = world.GetSurfaceY(pos[0], pos[2]);
			if (pos[1] < surfaceY)
			{
				pos[1] = surfaceY;
			}
			
			spawnParams.Transform[3] = pos;
			IEntity group = GetGame().SpawnEntityPrefab(Resource.Load(res), world, spawnParams);
			
			m_DefenseGroups.Insert(group.GetID());
			
			SCR_AIGroup aigroup = SCR_AIGroup.Cast(group);
			
			AddDefensePatrolWaypoints(aigroup);
			
			int newres = aigroup.m_aUnitPrefabSlots.Count() * m_Config.m_Difficulty.resourcesPerSoldier;
				
			if(newres > resources){
				newres = resources;
				//todo: delete some soldiers when overspending
			}
			
			defenseRes += newres;
			spent += newres;
			resources -= newres;
		}
		
		return spent;
	}	
	
	void AddDefensePatrolWaypoints(SCR_AIGroup aigroup)
	{
		array<AIWaypoint> queueOfWaypoints = new array<AIWaypoint>();
		
		if(m_AllCloseSlots.Count() > 2)
		{
			AIWaypoint firstWP;
			for(int i; i< 3; i++)
			{
				IEntity randomSlot = GetGame().GetWorld().FindEntityByID(m_AllCloseSlots.GetRandomElement());
				AIWaypoint wp = SpawnWaypoint(m_Config.m_pPatrolWaypointPrefab, randomSlot.GetOrigin());
				if(i==0) firstWP = wp;
				queueOfWaypoints.Insert(wp);
			}
			AIWaypointCycle cycle = AIWaypointCycle.Cast(SpawnWaypoint(m_Config.m_pCycleWaypointPrefab, firstWP.GetOrigin()));
			cycle.SetWaypoints(queueOfWaypoints);
			cycle.SetRerunCounter(-1);
			aigroup.AddWaypoint(cycle);
		}
	}
	
	int GetDefenseResources()
	{
		int res = 0;
		foreach(EntityID id : m_DefenseGroups)
		{
			SCR_AIGroup group = GetGroup(id);
			res += group.GetAgentsCount() * m_Config.m_Difficulty.resourcesPerSoldier;			
		}
		return res;
	}
	
	int GetReserveResources()
	{
		int res = 0;
		foreach(EntityID id : m_ReserveGroups)
		{
			SCR_AIGroup group = GetGroup(id);
			res += group.GetAgentsCount() * m_Config.m_Difficulty.resourcesPerSoldier;			
		}
		return res;
	}
	
	protected SCR_AIGroup GetGroup(EntityID id)
	{
		IEntity ent = GetGame().GetWorld().FindEntityByID(id);
		return SCR_AIGroup.Cast(ent);
	}
	
	protected AIWaypoint SpawnWaypoint(ResourceName res, vector pos)
	{
		EntitySpawnParams params = EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = pos;
		AIWaypoint wp = AIWaypoint.Cast(GetGame().SpawnEntityPrefabLocal(Resource.Load(res), null, params));
		return wp;
	}
}