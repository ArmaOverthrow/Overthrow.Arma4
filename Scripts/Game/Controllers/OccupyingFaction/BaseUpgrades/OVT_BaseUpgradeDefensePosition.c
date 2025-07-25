class OVT_BaseUpgradeDefensePosition : OVT_BasePatrolUpgrade
{
	ref map<int,ref EntityID> m_Guards;
	
	override void PostInit()
	{
		super.PostInit();
		
		m_Guards = new map<int,ref EntityID>;
	}
	
	protected override void CheckUpdate()
	{
		if(!m_BaseController.IsOccupyingFaction())
		{
			CheckClean();
			return;
		}
		bool inrange = PlayerInRange() && !m_occupyingFactionManager.m_CurrentQRF;
		if(inrange && !m_bSpawned)
		{
			Spend(m_iProxedResources, OVT_Global.GetOccupyingFaction().m_iThreat);
			m_bSpawned = true;
		}else if(!inrange && m_bSpawned){
			foreach(EntityID id : m_Groups)
			{
				SCR_AIGroup group = GetGroup(id);
				if(!group) continue;
				m_iProxedResources += group.GetAgentsCount() * OVT_Global.GetConfig().m_Difficulty.baseResourceCost;
				
				SCR_EntityHelper.DeleteEntityAndChildren(group);
			}
			m_Groups.Clear();
			m_Guards.Clear();
			m_bSpawned = false;
		}else{
			CheckClean();
		}
	}
	
	override int Spend(int resources, float threat)
	{
		int spent = 0;
		int maxSpawnCount = OVT_Global.GetConfig().m_Difficulty.defenseGroupsBaseMax;
		int spawnCount = Math.Clamp(Math.RandomInt(0, m_BaseController.m_aDefendPositions.Count()), 0, maxSpawnCount);
		if(m_BaseController.m_aDefendPositions.Count() == 0) return spent;
		
		foreach(int id, vector pos : m_BaseController.m_aDefendPositions)
		{
			if(resources < OVT_Global.GetConfig().m_Difficulty.baseResourceCost * 4 || spawnCount <= 0) break;
			
			bool needsGuard = false;
			if(!m_Guards.Contains(id))
			{
				needsGuard = true;
			}else{
				//Check if still alive
				SCR_AIGroup group = GetGroup(m_Guards[id]);				
				if(group)
				{
					if(group.GetAgentsCount() == 0) needsGuard = true;					
				}else{
					needsGuard = true;
				}
			}
			if(needsGuard)
			{
				spawnCount--;
				if(!PlayerInRange()){
					m_iProxedResources += OVT_Global.GetConfig().m_Difficulty.baseResourceCost * 4;
					spent += OVT_Global.GetConfig().m_Difficulty.baseResourceCost * 4;
				}else{
					if(BuyGuard(id, pos))
					{
						m_bSpawned = true;
						resources -= OVT_Global.GetConfig().m_Difficulty.baseResourceCost * 4;
						spent += OVT_Global.GetConfig().m_Difficulty.baseResourceCost * 4;
					}
				}				
			}
		}
		
		return spent;
	}
	
	protected bool BuyGuard(int id, vector pos)
	{	
		vector spawnpos = OVT_Global.GetRandomNonOceanPositionNear(m_BaseController.GetOwner().GetOrigin(), 50);			
					
		SCR_AIGroup group = SCR_AIGroup.Cast(OVT_Global.SpawnEntityPrefab(OVT_Global.GetConfig().GetOccupyingFaction().m_aHeavyInfantryPrefabSlots[0], spawnpos));
						
		AIWaypoint wp = OVT_Global.GetConfig().SpawnDefendWaypoint(pos);
		group.AddWaypoint(wp);
		
		m_Groups.Insert(group.GetID());
		m_Guards[id] = group.GetID();
		
		return true;
	}
	
	override OVT_BaseUpgradeData Serialize()
	{
		OVT_BaseUpgradeData struct = new OVT_BaseUpgradeData();
		struct.type = ClassName();
		struct.resources = GetResources();
		
		return struct;		
	}
	
	override bool Deserialize(OVT_BaseUpgradeData struct)
	{
		if(!m_BaseController.IsOccupyingFaction()) return true;
		Spend(struct.resources, m_iMinimumThreat);
		
		return true;
	}
}