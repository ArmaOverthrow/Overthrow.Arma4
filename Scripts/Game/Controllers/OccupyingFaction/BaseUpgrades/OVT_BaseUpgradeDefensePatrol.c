class OVT_BaseUpgradeDefensePatrol : OVT_BasePatrolUpgrade
{
	
	
	protected override void AddWaypoints(SCR_AIGroup aigroup)
	{
		array<AIWaypoint> queueOfWaypoints = new array<AIWaypoint>();
		AIWaypoint firstWP;
		
			
		for(int i=0; i< 4; i++)
		{
			vector randompos = OVT_Global.GetRandomNonOceanPositionNear(m_BaseController.GetOwner().GetOrigin(), OVT_Global.GetConfig().m_Difficulty.baseRange);			
			AIWaypoint wp = OVT_Global.GetConfig().SpawnPatrolWaypoint(randompos);
			if(i==0) firstWP = wp;
			queueOfWaypoints.Insert(wp);
			
			wp = OVT_Global.GetConfig().SpawnSearchAndDestroyWaypoint(randompos);			
			queueOfWaypoints.Insert(wp);			
		}			
		AIWaypointCycle cycle = AIWaypointCycle.Cast(OVT_Global.GetConfig().SpawnWaypoint(OVT_Global.GetConfig().m_pCycleWaypointPrefab, firstWP.GetOrigin()));
		cycle.SetWaypoints(queueOfWaypoints);
		cycle.SetRerunCounter(-1);
		aigroup.AddWaypoint(cycle);		
	}
	
	protected override bool GetWaypoints(inout array<vector> waypoints)
	{
		for(int i=0; i< 4; i++)
		{
			vector randompos = OVT_Global.GetRandomNonOceanPositionNear(m_BaseController.GetOwner().GetOrigin(), OVT_Global.GetConfig().m_Difficulty.baseRange);			
			waypoints.Insert(randompos);			
		}
		return true;
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
			
			int newres = OVT_Global.GetConfig().m_Difficulty.baseResourceCost * 4;
			
			OVT_Faction faction = OVT_Global.GetConfig().GetOccupyingFaction();
			ResourceName res = faction.GetRandomGroupByType(type);
			
			vector spawnpos = OVT_Global.GetRandomNonOceanPositionNear(m_BaseController.GetOwner().GetOrigin(), 50);			
			BuyPatrol(threat, res, spawnpos);
			
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