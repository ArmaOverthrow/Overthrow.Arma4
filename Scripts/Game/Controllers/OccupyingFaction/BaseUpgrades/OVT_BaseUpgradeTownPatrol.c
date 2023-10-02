class OVT_BaseUpgradeTownPatrol : OVT_BasePatrolUpgrade
{
	[Attribute("3000", desc: "Max range of towns to patrol")]
	float m_fRange;
	
	protected OVT_TownManagerComponent m_Towns;
	protected ref array<ref OVT_TownData> m_TownsInRange;
	protected ref map<int, int> m_Patrols;
	protected ref map<int, bool> m_SpottedPatrols;
		
	override void PostInit()
	{
		super.PostInit();
		
		m_Towns = OVT_Global.GetTowns();
		m_TownsInRange = new array<ref OVT_TownData>;
		m_Patrols = new map<int, int>;
		m_SpottedPatrols = new map<int, bool>;
		
		OVT_Global.GetTowns().GetTownsWithinDistance(m_BaseController.GetOwner().GetOrigin(), m_fRange, m_TownsInRange);
	}
	
	override void CheckUpdate()
	{
		OVT_TownModifierSystem system = m_Towns.GetModifierSystem(OVT_TownStabilityModifierSystem);
		if(!system) return;
		
		OVT_TownModifierSystem support = m_Towns.GetModifierSystem(OVT_TownSupportModifierSystem);
		if(!support) return;
		
		//Check on our patrols and update stability/support
		for(int i=0; i< m_Patrols.Count(); i++)
		{
			int townId = m_Patrols.GetKey(i);
			int id = m_Patrols.GetElement(i);
			OVT_VirtualizedGroupData data = OVT_VirtualizedGroupData.Get(id);
			
			if(data)
			{				
				OVT_TownData town = m_Towns.m_Towns[townId];
				float dist = vector.Distance(town.location, data.pos);
				if(town.SupportPercentage() > 25 && !m_SpottedPatrols[townId] && dist < m_Towns.GetTownRange(town))
				{
					m_SpottedPatrols[townId] = true;
					OVT_Global.GetNotify().SendTextNotification("PatrolSpotted",-1,m_Towns.GetTownName(townId));
				}
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
	
	override int Spend(int resources, float threat)
	{
		int spent = 0;
		
		foreach(OVT_TownData town : m_TownsInRange)
		{		
			int townID = m_Towns.GetTownID(town);	
			if(resources <= 0) break;
			if(!m_Patrols.Contains(townID))
			{
				int newres = BuyTownPatrol(town, threat);					
				spent += newres;
				resources -= newres;
			}else{
				//Check if theyre dead
				OVT_VirtualizedGroupData data = OVT_VirtualizedGroupData.Get(m_Patrols[townID]);
				
				if(!data) {
					m_Patrols.Remove(townID);
					int newres = BuyTownPatrol(town, threat);					
					spent += newres;
					resources -= newres;
					continue;
				}
			}
		}
		
		return spent;
	}
	
	protected int BuyTownPatrol(OVT_TownData town, float threat)
	{
		int townID = OVT_Global.GetTowns().GetTownID(town);
		OVT_Faction faction = OVT_Global.GetConfig().GetOccupyingFaction();
				
		ResourceName res = faction.m_aLightTownPatrolPrefab;
		if(threat > 15) res = faction.GetRandomGroupByType(OVT_GroupType.LIGHT_INFANTRY);
		if(threat > 50) res = faction.GetRandomGroupByType(OVT_GroupType.HEAVY_INFANTRY);		
		
		BaseWorld world = GetGame().GetWorld();
		
		vector pos = m_BaseController.GetOwner().GetOrigin();
		pos = OVT_Global.GetRandomNonOceanPositionNear(pos, 15);
		
		float surfaceY = world.GetSurfaceY(pos[0], pos[2]);
		if (pos[1] < surfaceY)
		{
			pos[1] = surfaceY;
		}
		
		OVT_VirtualizationManagerComponent virt = OVT_Global.GetVirtualization();
		array<vector> waypoints()
		if(GetTownWaypoints(waypoints, town))
		{
			OVT_VirtualizedGroupData data = virt.Create(res, pos, waypoints);
		
			m_aGroups.Insert(data.id);
			m_Patrols[townID] = data.id;
			m_SpottedPatrols[townID] = false;
			
			int newres = 2 * OVT_Global.GetConfig().m_Difficulty.baseResourceCost;
			
			return newres;
		}
		
		return 0;
	}
	
	protected bool GetTownWaypoints(inout array<vector> waypoints, OVT_TownData town)
	{	
		waypoints.Insert(OVT_Global.GetRandomNonOceanPositionNear(town.location, 250));
		waypoints.Insert(OVT_Global.GetRandomNonOceanPositionNear(town.location, 250));
		waypoints.Insert(OVT_Global.GetRandomNonOceanPositionNear(town.location, 250));
		
		return true;
	}
	
	override OVT_BaseUpgradeData Serialize()
	{
		return null;
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