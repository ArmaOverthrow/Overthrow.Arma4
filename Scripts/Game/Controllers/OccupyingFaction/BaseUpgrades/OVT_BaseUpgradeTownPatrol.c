class OVT_BaseUpgradeTownPatrol : OVT_BasePatrolUpgrade
{
	[Attribute("2000", desc: "Max range of towns to patrol")]
	float m_fRange;
	
	protected OVT_TownManagerComponent m_Towns;
	protected ref array<ref OVT_TownData> m_TownsInRange;
	protected ref map<ref int, ref EntityID> m_Patrols;
		
	override void PostInit()
	{
		super.PostInit();
		
		m_Towns = OVT_Global.GetTowns();
		m_TownsInRange = new array<ref OVT_TownData>;
		m_Patrols = new map<ref int, ref EntityID>;
		
		OVT_Global.GetTowns().GetTownsWithinDistance(m_BaseController.GetOwner().GetOrigin(), m_fRange, m_TownsInRange);
	}
	
	override void OnUpdate(int timeSlice)
	{
		OVT_TownModifierSystem system = m_Towns.GetModifierSystem(OVT_TownStabilityModifierSystem);
		if(!system) return;
		
		OVT_TownModifierSystem support = m_Towns.GetModifierSystem(OVT_TownSupportModifierSystem);
		if(!support) return;
		
		//Check on our patrols and update stability/support
		for(int i; i< m_Patrols.Count(); i++)
		{
			int townId = m_Patrols.GetKey(i);
			EntityID id = m_Patrols.GetElement(i);
			IEntity ent = GetGame().GetWorld().FindEntityByID(id);
			if(ent)
			{
				SCR_AIGroup group = SCR_AIGroup.Cast(ent);
				if(group.GetAgentsCount() > 0)
				{
					OVT_TownData town = m_Towns.m_Towns[townId];
					float dist = vector.Distance(town.location, group.GetOrigin());
					if(dist < 50)
					{						
						if(town.support >= 75)
						{
							system.TryAddByName(townId, "RecentPatrolNegative");
							system.RemoveByName(townId, "RecentPatrolPositive");
						}else{
							system.TryAddByName(townId, "RecentPatrolPositive");
							system.RemoveByName(townId, "RecentPatrolNegative");
						}			
						support.TryAddByName(townId, "RecentPatrol");			
					}
				}
			}
		}
	}
	
	override int Spend(int resources, float threat)
	{
		int spent = 0;
		
		foreach(OVT_TownData town : m_TownsInRange)
		{			
			if(resources <= 0) break;
			if(!OVT_Global.PlayerInRange(town.location, 2500)) continue;
			if(!m_Patrols.Contains(town.id))
			{
				int newres = BuyTownPatrol(town, threat);
				
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
					m_occupyingFactionManager.RecoverResources(agentCount * m_Config.m_Difficulty.baseResourceCost);
					
					m_Patrols.Remove(town.id);
					SCR_EntityHelper.DeleteEntityAndChildren(aigroup);	
					
					//send another one
					int newres = BuyTownPatrol(town, threat);
				
					spent += newres;
					resources -= newres;			
				}
			}
		}
		
		return spent;
	}
	
	protected int BuyTownPatrol(OVT_TownData town, float threat)
	{
		OVT_Faction faction = m_Config.GetOccupyingFaction();
				
		ResourceName res = faction.m_aLightTownPatrolPrefab;
		if(threat > 15) res = faction.GetRandomGroupByType(OVT_GroupType.LIGHT_INFANTRY);
		if(threat > 50) res = faction.GetRandomGroupByType(OVT_GroupType.HEAVY_INFANTRY);		
		
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
		m_Patrols[town.id] = group.GetID();
		
		SCR_AIGroup aigroup = SCR_AIGroup.Cast(group);
		
		AddWaypoints(aigroup, town);
		
		int newres = aigroup.m_aUnitPrefabSlots.Count() * m_Config.m_Difficulty.baseResourceCost;
			
		return newres;
	}
	
	protected void AddWaypoints(SCR_AIGroup aigroup, OVT_TownData town)
	{		
		array<AIWaypoint> queueOfWaypoints = new array<AIWaypoint>();
		array<RplId> shops = m_Economy.GetAllShopsInTown(town);
		if(shops.Count() == 0)
		{
			//To-Do: find some random buildings
		}
							
		aigroup.AddWaypoint(m_Config.SpawnPatrolWaypoint(town.location));			
		aigroup.AddWaypoint(m_Config.SpawnWaitWaypoint(town.location, s_AIRandomGenerator.RandFloatXY(15, 50)));								
		
		foreach(RplId id : shops)
		{
			RplComponent rpl = RplComponent.Cast(Replication.FindItem(id));
			vector pos = rpl.GetEntity().GetOrigin();
			aigroup.AddWaypoint(m_Config.SpawnPatrolWaypoint(pos));			
			aigroup.AddWaypoint(m_Config.SpawnWaitWaypoint(pos, s_AIRandomGenerator.RandFloatXY(15, 50)));
		}
		
		aigroup.AddWaypoint(m_Config.SpawnPatrolWaypoint(m_BaseController.GetOwner().GetOrigin()));
		
	}
	
	override OVT_BaseUpgradeStruct Serialize(inout array<string> rdb)
	{
		OVT_BaseUpgradeStruct struct = new OVT_BaseUpgradeStruct();
		struct.type = ClassName();
		struct.resources = GetResources();
		return struct;
	}
	
	override bool Deserialize(OVT_BaseUpgradeStruct struct, array<string> rdb)
	{
		if(!m_BaseController.IsOccupyingFaction()) return true;
		Spend(struct.resources, m_iMinimumThreat);
		
		return true;
	}
	
	void ~OVT_BaseUpgradeTownPatrol()
	{
		if(m_Patrols)
		{
			m_Patrols.Clear();
			m_Patrols = null;
		}
		if(m_TownsInRange)
		{
			m_TownsInRange.Clear();
			m_TownsInRange = null;
		}
	}
}