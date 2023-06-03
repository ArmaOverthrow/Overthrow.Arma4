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
		}else{
			//Walk to and from the flag to a random position
			AIWaypoint wp = m_Config.SpawnPatrolWaypoint(s_AIRandomGenerator.GenerateRandomPointInRadius(40,100,m_BaseController.GetOwner().GetOrigin()));
			queueOfWaypoints.Insert(wp);
			
			firstWP = wp;
				
			AIWaypoint wait = m_Config.SpawnWaitWaypoint(wp.GetOrigin(), s_AIRandomGenerator.RandFloatXY(45, 75));								
			queueOfWaypoints.Insert(wait);
			
			wp = m_Config.SpawnPatrolWaypoint(s_AIRandomGenerator.GenerateRandomPointInRadius(5,20,m_BaseController.GetOwner().GetOrigin()));
			queueOfWaypoints.Insert(wp);
			
			wait = m_Config.SpawnWaitWaypoint(wp.GetOrigin(), s_AIRandomGenerator.RandFloatXY(45, 75));								
			queueOfWaypoints.Insert(wait);
		}
				
		AIWaypointCycle cycle = AIWaypointCycle.Cast(m_Config.SpawnWaypoint(m_Config.m_pCycleWaypointPrefab, firstWP.GetOrigin()));
		cycle.SetWaypoints(queueOfWaypoints);
		cycle.SetRerunCounter(-1);
		aigroup.AddWaypoint(cycle);
	}
}