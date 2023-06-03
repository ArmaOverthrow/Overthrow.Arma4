class OVT_BaseUpgradeTowerGuard : OVT_BasePatrolUpgrade
{
	ref array<ref EntityID> m_Towers;
	ref map<ref EntityID,ref EntityID> m_TowerGuards;
	
	override void PostInit()
	{
		super.PostInit();
		
		m_Towers = new array<ref EntityID>;
		m_TowerGuards = new map<ref EntityID,ref EntityID>;
		
		FindTowers();
	}
	
	protected void FindTowers()
	{
		GetGame().GetWorld().QueryEntitiesBySphere(m_BaseController.GetOwner().GetOrigin(), m_Config.m_Difficulty.baseRange, CheckTowerAddToArray, FilterTowerEntities, EQueryEntitiesFlags.ALL);
	}
	
	protected bool FilterTowerEntities(IEntity entity)
	{			
		if(entity.ClassName() == "SCR_DestructibleBuildingEntity") {
			
			SCR_MapDescriptorComponent mapdesc = SCR_MapDescriptorComponent.Cast(entity.FindComponent(SCR_MapDescriptorComponent));
			if(mapdesc)
			{
				EMapDescriptorType type = mapdesc.GetBaseType();
				if(type == EMapDescriptorType.MDT_TOWER) return true;
			}
		}
		return false;
	}
	
	protected bool CheckTowerAddToArray(IEntity entity)
	{
		m_Towers.Insert(entity.GetID());
		return true;
	}
	
	protected override void CheckUpdate()
	{
		if(!m_BaseController.IsOccupyingFaction())
		{
			CheckClean();
			return;
		}
		bool inrange = PlayerInRange();
		if(inrange && !m_bSpawned)
		{
			Spend(m_iProxedResources, OVT_Global.GetOccupyingFaction().m_iThreat);
			m_bSpawned = true;
		}else if(!inrange && m_bSpawned){
			foreach(EntityID id : m_Groups)
			{
				SCR_AIGroup group = GetGroup(id);
				if(!group) continue;
				m_iProxedResources += group.GetAgentsCount() * m_Config.m_Difficulty.baseResourceCost;
				
				SCR_EntityHelper.DeleteEntityAndChildren(group);
			}
			m_Groups.Clear();
			m_TowerGuards.Clear();
			m_bSpawned = false;
		}else{
			CheckClean();
		}
	}
	
	override int Spend(int resources, float threat)
	{
		int spent = 0;
		
		foreach(EntityID id : m_Towers)
		{
			if(resources < m_Config.m_Difficulty.baseResourceCost) break;
			
			bool needsGuard = false;
			if(!m_TowerGuards.Contains(id))
			{
				needsGuard = true;
			}else{
				//Check if still alive
				SCR_AIGroup group = GetGroup(m_TowerGuards[id]);				
				if(group)
				{
					if(group.GetAgentsCount() == 0) needsGuard = true;					
				}else{
					needsGuard = true;
				}
			}
			if(needsGuard)
			{
				if(!PlayerInRange()){
					m_iProxedResources += m_Config.m_Difficulty.baseResourceCost;
					spent += m_Config.m_Difficulty.baseResourceCost;
				}else{
					if(BuyGuard(id))
					{
						m_bSpawned = true;
						resources -= m_Config.m_Difficulty.baseResourceCost;
						spent += m_Config.m_Difficulty.baseResourceCost;
					}
				}				
			}
		}
		
		return spent;
	}
	
	protected bool BuyGuard(EntityID towerID)
	{
		
		IEntity tower = GetGame().GetWorld().FindEntityByID(towerID);
		
		array<Managed> outComponents = {};
		SCR_AISmartActionSentinelComponent component;
		SCR_AISmartActionSentinelComponent sentinel;
		array<string> outTags = {};
		tower.FindComponents(SCR_AISmartActionSentinelComponent, outComponents);
		
		foreach (Managed outComponent : outComponents)
		{
			component = SCR_AISmartActionSentinelComponent.Cast(outComponent);
			if (!component)
				continue;
			component.GetTags(outTags);
			if (outTags.Contains("CoverPost") && component.IsActionAccessible())
			{
				sentinel = component;
			}
		}
				
		if(!sentinel) return false;
		
		vector actionPos = tower.GetOrigin() + sentinel.GetActionOffset() - "0 1.3 0";
				
		SCR_AIGroup group = SCR_AIGroup.Cast(OVT_Global.SpawnEntityPrefab(m_Config.GetOccupyingFaction().m_aGroupSniperPrefab, actionPos));
						
		AIWaypoint wp = m_Config.SpawnActionWaypoint(actionPos, tower, "CoverPost");
		group.AddWaypoint(wp);
		
		m_Groups.Insert(group.GetID());
		m_TowerGuards[towerID] = group.GetID();
		
		return true;
	}
	
	void ~OVT_BaseUpgradeTowerGuard()
	{
		if(m_Towers)
		{
			m_Towers.Clear();
			m_Towers = null;
		}
		if(m_TowerGuards)
		{
			m_TowerGuards.Clear();
			m_TowerGuards = null;
		}
	}
}