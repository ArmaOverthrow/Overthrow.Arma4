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
		GetGame().GetWorld().QueryEntitiesBySphere(m_BaseController.GetOwner().GetOrigin(), m_BaseController.m_iRange, CheckTowerAddToArray, FilterTowerEntities, EQueryEntitiesFlags.ALL);
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
		//Disable despawning for now
		return;
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
				if(BuyGuard(id))
				{
					resources -= m_Config.m_Difficulty.baseResourceCost;
					spent += m_Config.m_Difficulty.baseResourceCost;
				}
			}
		}
		
		return spent;
	}
	
	protected bool BuyGuard(EntityID towerID)
	{
		IEntity tower = GetGame().GetWorld().FindEntityByID(towerID);
		SCR_AISmartActionSentinelComponent sentinel = SCR_AISmartActionSentinelComponent.Cast(tower.FindComponent(SCR_AISmartActionSentinelComponent));
		if(!sentinel) return false;
		
		vector wTrans[4];
		tower.GetWorldTransform(wTrans);
		
		vector actionPos = sentinel.GetActionOffset().Multiply4(wTrans);		
		
		EntitySpawnParams params = EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		params.Transform[3] = actionPos;
		SCR_AIGroup group = SCR_AIGroup.Cast(GetGame().SpawnEntityPrefab(Resource.Load(m_Config.GetOccupyingFaction().m_aGroupSniperPrefab), null, params));
						
		AIWaypoint wp = SpawnActionWaypoint(actionPos, tower, "CoverPost");
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