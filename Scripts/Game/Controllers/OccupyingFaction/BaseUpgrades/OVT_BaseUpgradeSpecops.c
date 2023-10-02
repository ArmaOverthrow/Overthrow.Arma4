class OVT_BaseUpgradeSpecops : OVT_BasePatrolUpgrade
{
	protected OVT_TargetData m_CurrentTarget;
	
	override int Spend(int resources, float threat)
	{
		return 0;
	}
	
	bool HasTarget()
	{
		return m_CurrentTarget != null;
	}
	
	OVT_TargetData CurrentTarget()
	{
		return m_CurrentTarget;
	}
	
	override void CheckUpdate()
	{
		if(!m_CurrentTarget) return;		
		
		if(HasTarget() && m_aGroups.Count() == 0)
		{
			//Remove target and wait for new orders
			m_CurrentTarget = null;
		}
	}
	
	int SetTarget(OVT_TargetData target)
	{
		m_CurrentTarget = target;
		OVT_Faction faction = OVT_Global.GetConfig().GetOccupyingFaction();
		if(!faction) return 0;
		
		//Send any remaining existing groups to new target
		int groupsSent = 0;
		int spent = 0;
		foreach(int id : m_aGroups)
		{
			OVT_VirtualizedGroupData data = OVT_VirtualizedGroupData.Get(id);
			if(!data) continue;
			array<vector> waypoints();
			
			if(GetWaypoints(waypoints))
			{
				data.SetWaypoints(waypoints);								
			}
			
			groupsSent++;
		}
		
		if(groupsSent == 0)
		{
			//Send specops
			ResourceName res = faction.GetRandomGroupByType(OVT_GroupType.SPECIAL_FORCES);
			spent += OVT_Global.GetConfig().m_Difficulty.baseResourceCost * 4;	
			BuyPatrol(0, res);
		}
		return spent;
	}
	
	override bool GetWaypoints(inout array<vector> waypoints)
	{
		if(!m_CurrentTarget) return true;

		waypoints.Insert(m_CurrentTarget.location);	
		
		return true;
	}
	
	override OVT_BaseUpgradeData Serialize()
	{
		return null;
	}
	
	override bool Deserialize(OVT_BaseUpgradeData struct)	
	{		
		return true;
	}
}