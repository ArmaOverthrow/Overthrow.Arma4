class OVT_BaseUpgradeDefensePatrol : OVT_BasePatrolUpgrade
{
	protected override int BuyPatrol()
	{
		OVT_Faction faction = m_Config.GetOccupyingFaction();
		BaseWorld world = GetGame().GetWorld();
		
		ResourceName res = faction.m_aGroupInfantryPrefabSlots.GetRandomElement();
			
		EntitySpawnParams spawnParams = new EntitySpawnParams;
		spawnParams.TransformMode = ETransformMode.WORLD;
		
		vector pos = m_BaseController.GetOwner().GetOrigin();
		
		float surfaceY = world.GetSurfaceY(pos[0], pos[2]);
		if (pos[1] < surfaceY)
		{
			pos[1] = surfaceY;
		}
		
		spawnParams.Transform[3] = pos;
		IEntity group = GetGame().SpawnEntityPrefab(Resource.Load(res), world, spawnParams);
		
		m_Groups.Insert(group.GetID());
		
		SCR_AIGroup aigroup = SCR_AIGroup.Cast(group);
		
		AddWaypoints(aigroup);
		
		int newres = aigroup.m_aUnitPrefabSlots.Count() * m_Config.m_Difficulty.resourcesPerSoldier;
			
		return newres;
	}
	
	protected void AddWaypoints(SCR_AIGroup aigroup)
	{
		array<AIWaypoint> queueOfWaypoints = new array<AIWaypoint>();
		
		if(m_BaseController.m_AllCloseSlots.Count() > 2)
		{
			AIWaypoint firstWP;
			for(int i; i< 3; i++)
			{
				IEntity randomSlot = GetGame().GetWorld().FindEntityByID(m_BaseController.m_AllCloseSlots.GetRandomElement());
				AIWaypoint wp = SpawnPatrolWaypoint(randomSlot.GetOrigin());
				if(i==0) firstWP = wp;
				queueOfWaypoints.Insert(wp);
				
				AIWaypoint wait = SpawnWaitWaypoint(randomSlot.GetOrigin(), s_AIRandomGenerator.RandFloatXY(45, 75));								
				queueOfWaypoints.Insert(wait);
			}
			AIWaypointCycle cycle = AIWaypointCycle.Cast(SpawnWaypoint(m_Config.m_pCycleWaypointPrefab, firstWP.GetOrigin()));
			cycle.SetWaypoints(queueOfWaypoints);
			cycle.SetRerunCounter(-1);
			aigroup.AddWaypoint(cycle);
		}
	}
}