class OVT_BaseUpgradeCheckpoints : OVT_BasePatrolUpgrade
{		
	protected IEntity m_SpawnedCheckpoint;
		
	override int Spend(int resources, float threat)
	{
		int spent = 0;
		
		foreach(EntityID id : m_BaseController.m_LargeRoadSlots)
		{
			if(m_BaseController.m_aSlotsFilled.Contains(id)) continue;
			if(resources < 60) break;
			IEntity slot = GetGame().GetWorld().FindEntityByID(id);
			
			
			spent += 60;
			resources -= 60;
			m_SpawnedCheckpoint = SpawnCheckpoint(slot, m_Faction.m_aLargeCheckpointPrefab);
			m_BaseController.m_aSlotsFilled.Insert(id);
			
			if(resources < (OVT_Global.GetConfig().m_Difficulty.baseResourceCost * 4)) break;
			int newres = BuyPatrol(threat, m_Faction.m_aGroupInfantryPrefabSlots[0], slot.GetOrigin());
			spent += newres;
			resources -= newres;
		}
		
		foreach(EntityID id : m_BaseController.m_MediumRoadSlots)
		{
			if(m_BaseController.m_aSlotsFilled.Contains(id)) continue;
			if(resources < 40) break;
			IEntity slot = GetGame().GetWorld().FindEntityByID(id);
			spent += 40;
			resources -= 40;
			m_SpawnedCheckpoint = SpawnCheckpoint(slot, m_Faction.m_aMediumCheckpointPrefab);
			if(!m_SpawnedCheckpoint) continue;
			
			m_BaseController.m_aSlotsFilled.Insert(id);
			
			if(resources < (OVT_Global.GetConfig().m_Difficulty.baseResourceCost * 4)) break;
			int newres = BuyPatrol(threat, m_Faction.m_aGroupInfantryPrefabSlots[0], slot.GetOrigin());
			spent += newres;
			resources -= newres;
		}
		
		return spent;
	}
	
	override void AddWaypoints(SCR_AIGroup aigroup)
	{
		if(!m_SpawnedCheckpoint) return;
		if(!aigroup) return;
		aigroup.AddWaypoint(OVT_Global.GetConfig().SpawnDefendWaypoint(m_SpawnedCheckpoint.GetOrigin()));
	}
	
	protected IEntity SpawnCheckpoint(IEntity slot, ResourceName res)
	{		
		vector mat[4];
		slot.GetTransform(mat);		
		IEntity ent = OVT_Global.SpawnEntityPrefabMatrix(res, mat);
		return ent;
	}
}