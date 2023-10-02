class OVT_BaseUpgradeTowerGuard : OVT_BasePatrolUpgrade
{
	ref array<ref EntityID> m_Towers;
	ref map<ref EntityID,int> m_TowerGuards;
	
	override void PostInit()
	{
		super.PostInit();
		
		m_Towers = new array<ref EntityID>;
		m_TowerGuards = new map<ref EntityID,int>;
		
		FindTowers();
	}
	
	protected void FindTowers()
	{
		GetGame().GetWorld().QueryEntitiesBySphere(m_BaseController.GetOwner().GetOrigin(), OVT_Global.GetConfig().m_Difficulty.baseRange, CheckTowerAddToArray, FilterTowerEntities, EQueryEntitiesFlags.ALL);
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
	
	override int Spend(int resources, float threat)
	{
		int spent = 0;
		
		foreach(EntityID id : m_Towers)
		{
			if(resources < OVT_Global.GetConfig().m_Difficulty.baseResourceCost) break; //We are out of resources
			
			bool needsGuard = false;
			if(!m_TowerGuards.Contains(id))
			{
				needsGuard = true;
			}else{
				//Check if still alive
				OVT_VirtualizedGroupData data = OVT_VirtualizedGroupData.Get(m_TowerGuards[id]);
				if(!data) needsGuard = true;
			}
			if(needsGuard)
			{
				if(BuyGuard(id))
				{
					int newres = OVT_Global.GetConfig().m_Difficulty.baseResourceCost;
					resources -= newres;
					spent += newres;
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
		
		OVT_VirtualizationManagerComponent virt = OVT_Global.GetVirtualization();
		array<vector> waypoints()
		OVT_VirtualizedGroupData data = virt.Create(OVT_Global.GetConfig().GetOccupyingFaction().m_aGroupSniperPrefab, actionPos, waypoints);
		
		m_aGroups.Insert(data.id);
		m_TowerGuards[towerID] = data.id;
		
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