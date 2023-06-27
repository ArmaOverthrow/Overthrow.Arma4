class OVT_BaseUpgradeDefensePatrol : OVT_BasePatrolUpgrade
{
	
	
	protected override void AddWaypoints(SCR_AIGroup aigroup)
	{
		array<AIWaypoint> queueOfWaypoints = new array<AIWaypoint>();
		AIWaypoint firstWP;
		
		if(m_BaseController.m_AllCloseSlots.Count() > 2)
		{
			
			for(int i=0; i< 3; i++)
			{
				IEntity randomSlot = GetGame().GetWorld().FindEntityByID(m_BaseController.m_AllCloseSlots.GetRandomElement());
				AIWaypoint wp = m_Config.SpawnPatrolWaypoint(randomSlot.GetOrigin());
				if(i==0) firstWP = wp;
				queueOfWaypoints.Insert(wp);
				
				AIWaypoint wait = m_Config.SpawnWaitWaypoint(randomSlot.GetOrigin(), s_AIRandomGenerator.RandFloatXY(45, 75));								
				queueOfWaypoints.Insert(wait);
			}			
			AIWaypointCycle cycle = AIWaypointCycle.Cast(m_Config.SpawnWaypoint(m_Config.m_pCycleWaypointPrefab, firstWP.GetOrigin()));
			cycle.SetWaypoints(queueOfWaypoints);
			cycle.SetRerunCounter(-1);
			aigroup.AddWaypoint(cycle);
		}else{
			//Just defend the flag
			aigroup.AddWaypoint(m_Config.SpawnDefendWaypoint(m_BaseController.GetOwner().GetOrigin()));
		}				
		
	}
	
	override int Spend(int resources, float threat)
	{
		int spent = 0;
		
		while(resources > 0)
		{
			OVT_GroupType type = OVT_GroupType.LIGHT_INFANTRY;
			
			if(m_iNumGroups == 0 || threat > 50)
			{
				type = OVT_GroupType.ANTI_TANK;
			}else if(threat > 25){
				type = OVT_GroupType.HEAVY_INFANTRY;
			}
			
			int newres = m_Config.m_Difficulty.baseResourceCost * 4;
			
			OVT_Faction faction = m_Config.GetOccupyingFaction();
			ResourceName res = faction.GetRandomGroupByType(type);
			m_iProxedResources += newres;
			m_ProxiedGroups.Insert(res);
			m_ProxiedPositions.Insert(m_BaseController.GetOwner().GetOrigin());			
			
			if(newres > resources){
				newres = resources;
				//todo: delete some soldiers when overspending
			}
			
			spent += newres;
			resources -= newres;
			m_iNumGroups++;
		}
		
		return spent;
	}
}