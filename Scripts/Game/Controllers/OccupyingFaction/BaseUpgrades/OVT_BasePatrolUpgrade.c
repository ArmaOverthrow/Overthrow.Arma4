class OVT_BasePatrolUpgrade : OVT_BaseUpgrade
{
	[Attribute("1", desc: "Deactivate patrols when noone around")]
	bool m_bDeactivate;
	
	ref array<ref EntityID> m_Groups;
	ref array<ref ResourceName> m_ProxiedGroups;
	ref array<ref vector> m_ProxiedPositions;	
	int m_iProxedResources = 0;
	
	protected const int DEACTIVATE_FREQUENCY = 10000;
	protected const int DEACTIVATE_RANGE = 2500;
	
	bool m_bSpawned = true;
	
	override void PostInit()
	{
		m_Groups = new array<ref EntityID>;
		m_ProxiedGroups = new array<ref ResourceName>;
		m_ProxiedPositions = new array<ref vector>;
		
		
		float freq = s_AIRandomGenerator.RandFloatXY(DEACTIVATE_FREQUENCY - 1000, DEACTIVATE_FREQUENCY + 1000);
		if(m_bDeactivate)
		{			
			GetGame().GetCallqueue().CallLater(CheckUpdate, freq, true, m_BaseController.GetOwner());	
		}else{
			GetGame().GetCallqueue().CallLater(CheckClean, freq, true, m_BaseController.GetOwner());	
		}
	}
	
	protected void CheckClean()
	{
		//Clean up ghost/killed groups
		array<EntityID> remove = {};
		foreach(EntityID id : m_Groups)
		{
			SCR_AIGroup group = GetGroup(id);
			if(!group)
			{
				remove.Insert(id);
			}else if(group.GetAgentsCount() == 0)
			{
				SCR_EntityHelper.DeleteEntityAndChildren(group);
				remove.Insert(id);
			}
		}
		foreach(EntityID id : remove)			
		{
			m_Groups.RemoveItem(id);
		}
	}
	
	protected void CheckUpdate()
	{
		if(!m_BaseController.IsOccupyingFaction())
		{
			CheckClean();
			return;
		}
		bool inrange = PlayerInRange();
		if(inrange && !m_bSpawned)
		{
			foreach(int i, ResourceName res : m_ProxiedGroups)
			{
				BuyPatrol(0, res, m_ProxiedPositions[i]);
			}
			m_ProxiedGroups.Clear();
			m_ProxiedPositions.Clear();
			m_iProxedResources = 0;
			
			m_bSpawned = true;					
		}else if(!inrange && m_bSpawned){
			foreach(EntityID id : m_Groups)
			{
				SCR_AIGroup group = GetGroup(id);
				if(!group) continue;
				m_iProxedResources += group.GetAgentsCount() * m_Config.m_Difficulty.baseResourceCost;
				m_ProxiedGroups.Insert(EPF_Utils.GetPrefabName(group));
				m_ProxiedPositions.Insert(group.GetOrigin());
				SCR_EntityHelper.DeleteEntityAndChildren(group);
			}
			m_Groups.Clear();
			m_bSpawned = false;
		}else{
			CheckClean();
		}
	}
	
	protected bool PlayerInRange()
	{		
		return OVT_Global.PlayerInRange(m_BaseController.GetOwner().GetOrigin(), DEACTIVATE_RANGE);
	}
	
	protected int BuyPatrol(float threat, ResourceName res = "", vector pos = "0 0 0")
	{
		OVT_Faction faction = m_Config.GetOccupyingFaction();
		if(!faction) return 0;
				
		if(res == ""){
			res = faction.GetRandomGroupByType(OVT_GroupType.LIGHT_INFANTRY);
			if(threat > 25) res = faction.GetRandomGroupByType(OVT_GroupType.HEAVY_INFANTRY);
		}
		
		BaseWorld world = GetGame().GetWorld();
					
		if(pos[0] == 0)
		{
			pos = s_AIRandomGenerator.GenerateRandomPointInRadius(5,50, m_BaseController.GetOwner().GetOrigin());			
		}
		
		float surfaceY = world.GetSurfaceY(pos[0], pos[2]);
		if (pos[1] < surfaceY)
		{
			pos[1] = surfaceY;
		}
		
		IEntity group = OVT_Global.SpawnEntityPrefab(res, pos);
		
		m_Groups.Insert(group.GetID());
		
		SCR_AIGroup aigroup = SCR_AIGroup.Cast(group);
		
		AddWaypoints(aigroup);
		
		int newres = aigroup.m_aUnitPrefabSlots.Count() * m_Config.m_Difficulty.baseResourceCost;
			
		return newres;
	}
	
	protected void AddWaypoints(SCR_AIGroup aigroup)
	{
		
	}
	
	override int GetResources()
	{
		int res = 0;
		foreach(EntityID id : m_Groups)
		{
			SCR_AIGroup group = GetGroup(id);
			if(!group) continue;
			res += group.GetAgentsCount() * m_Config.m_Difficulty.baseResourceCost;			
		}
		return res + m_iProxedResources;
	}
	
	override int Spend(int resources, float threat)
	{
		int spent = 0;
		
		while(resources > 0)
		{
			int newres = m_Config.m_Difficulty.baseResourceCost * 4;
			if(!PlayerInRange())
			{
				OVT_Faction faction = m_Config.GetOccupyingFaction();
				ResourceName res = faction.GetRandomGroupByType(OVT_GroupType.LIGHT_INFANTRY);
				m_iProxedResources += newres;
				m_ProxiedGroups.Insert(res);
				m_ProxiedPositions.Insert(m_BaseController.GetOwner().GetOrigin());
			}else{
				newres = BuyPatrol(threat);
				m_bSpawned = true;
			}
			
			if(newres > resources){
				newres = resources;
				//todo: delete some soldiers when overspending
			}
			
			spent += newres;
			resources -= newres;
		}
		
		return spent;
	}
	
	protected SCR_AIGroup GetGroup(EntityID id)
	{
		IEntity ent = GetGame().GetWorld().FindEntityByID(id);
		return SCR_AIGroup.Cast(ent);
	}
	
	override OVT_BaseUpgradeData Serialize()
	{
		OVT_BaseUpgradeData struct = super.Serialize();
		
		struct.resources = 0; //Do not respend any resources
		
		foreach(EntityID id : m_Groups)
		{
			IEntity group = GetGame().GetWorld().FindEntityByID(id);
			SCR_AIGroup aigroup = SCR_AIGroup.Cast(group);
			if(!aigroup) continue;
			if(aigroup.GetAgentsCount() > 0)
			{
				OVT_BaseUpgradeGroupData g = new OVT_BaseUpgradeGroupData();
				
				string res = EPF_Utils.GetPrefabName(aigroup);
				g.prefab = res;				
				g.position = group.GetOrigin();
				
				struct.groups.Insert(g);
			}			
		}
		
		foreach(int i, ResourceName res : m_ProxiedGroups)
		{
			OVT_BaseUpgradeGroupData g = new OVT_BaseUpgradeGroupData();
			g.prefab = res;
			g.position = m_ProxiedPositions[i];
			
			struct.groups.Insert(g);
		}
		
		return struct;
	}
	
	override bool Deserialize(OVT_BaseUpgradeData struct)
	{
		if(!m_BaseController.IsOccupyingFaction()) return true;
		foreach(OVT_BaseUpgradeGroupData g : struct.groups)
		{
			BuyPatrol(0, g.prefab, g.position);
		}
		return true;
	}
	
	void ~OVT_BasePatrolUpgrade()
	{
		GetGame().GetCallqueue().Remove(CheckUpdate);	
		
		if(m_Groups)
		{
			m_Groups.Clear();
			m_Groups = null;
		}
		if(m_ProxiedGroups)
		{
			m_ProxiedGroups.Clear();
			m_ProxiedGroups = null;
		}
		if(m_ProxiedPositions)
		{
			m_ProxiedPositions.Clear();
			m_ProxiedPositions = null;
		}		
	}
}