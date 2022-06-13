class OVT_BaseUpgradeTownPatrol : OVT_BasePatrolUpgrade
{
	[Attribute("2000", desc: "Max range of towns to patrol")]
	float m_fRange;
	
	protected ref array<ref OVT_TownData> m_TownsInRange;
	protected ref map<int, EntityID> m_Patrols;
		
	override void PostInit()
	{
		super.PostInit();
		
		m_TownsInRange = new array<ref OVT_TownData>;
		m_Patrols = new map<int, EntityID>;
		
		OVT_TownManagerComponent.GetInstance().GetTownsWithinDistance(m_BaseController.GetOwner().GetOrigin(), m_fRange, m_TownsInRange);
	}
	
	override int Spend(int resources)
	{
		int spent = 0;
		
		foreach(OVT_TownData town : m_TownsInRange)
		{
			if(resources <= 0) break;
			if(!m_Patrols.Contains(town.id))
			{
				int newres = BuyTownPatrol(town);
				
				spent += newres;
				resources -= newres;
			}else{
				//Check if theyre back
				SCR_AIGroup aigroup = GetGroup(m_Patrols[town.id]);
				float distance = vector.Distance(aigroup.GetOrigin(), m_BaseController.GetOwner().GetOrigin());
				int agentCount = aigroup.GetAgentsCount();
				if(distance < 20 || agentCount == 0)
				{
					//Recover any resources
					m_occupyingFactionManager.RecoverResources(agentCount * m_Config.m_Difficulty.resourcesPerSoldier);
					
					m_Patrols.Remove(town.id);
					SCR_Global.DeleteEntityAndChildren(aigroup);	
					
					//send another one
					int newres = BuyTownPatrol(town);
				
					spent += newres;
					resources -= newres;			
				}
			}
		}
		
		return spent;
	}
	
	protected int BuyTownPatrol(OVT_TownData town)
	{
		OVT_Faction faction = m_Config.GetOccupyingFaction();
				
		ResourceName res = faction.m_aLightTownPatrolPrefab;
		
		BaseWorld world = GetGame().GetWorld();
			
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
		
		AddWaypoints(aigroup, town);
		
		int newres = aigroup.m_aUnitPrefabSlots.Count() * m_Config.m_Difficulty.resourcesPerSoldier;
			
		return newres;
	}
	
	protected void AddWaypoints(SCR_AIGroup aigroup, OVT_TownData town)
	{		
		array<AIWaypoint> queueOfWaypoints = new array<AIWaypoint>();
		
		if(m_BaseController.m_AllCloseSlots.Count() > 2)
		{						
			aigroup.AddWaypoint(SpawnPatrolWaypoint(town.location));
			
			aigroup.AddWaypoint(SpawnWaitWaypoint(town.location, s_AIRandomGenerator.RandFloatXY(45, 75)));								
			
			aigroup.AddWaypoint(SpawnPatrolWaypoint(m_BaseController.GetOwner().GetOrigin()));
			
		}
	}
	
	void ~OVT_BaseUpgradeTownPatrol()
	{
		m_Patrols.Clear();
		m_TownsInRange.Clear();		
	}
}