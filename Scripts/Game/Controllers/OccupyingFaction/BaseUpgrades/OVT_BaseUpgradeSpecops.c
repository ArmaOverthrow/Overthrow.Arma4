class OVT_BaseUpgradeSpecops : OVT_BasePatrolUpgrade
{
	protected OVT_TargetData m_CurrentTarget;
	
	protected int m_iCaptureTimer;
	protected bool m_bCapturing = false;
	
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
	
	override void OnUpdate(int timeSlice)
	{
		if(!m_CurrentTarget) return;
		foreach(EntityID id : m_Groups)
		{
			SCR_AIGroup aigroup = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(id));
			if(!aigroup) continue;
			if(aigroup.GetAgentsCount() == 0) {
				m_Groups.RemoveItem(id);
			}
			
			if(vector.Distance(aigroup.GetOrigin(), m_CurrentTarget.location) < 20)
			{
				//Group has arrived
				//To-Do: Destroy stuff if needed
				bool inrange = OVT_Global.PlayerInRange(m_CurrentTarget.location, 200);
				if(m_CurrentTarget.type == OVT_TargetType.BASE)
				{
					OVT_BaseData data = m_occupyingFactionManager.GetNearestBase(m_CurrentTarget.location);
					OVT_BaseControllerComponent base = m_occupyingFactionManager.GetBase(data.entId);
					
					//Try to take this base back with full QRF
					if(!data.IsOccupyingFaction() && !m_occupyingFactionManager.m_CurrentQRF)
					{
						m_occupyingFactionManager.StartBaseQRF(base);
						m_CurrentTarget.completed = true;
						m_CurrentTarget = null;
						return;
					}
				}else if(m_CurrentTarget.type == OVT_TargetType.BROADCAST_TOWER)
				{
					OVT_RadioTowerData data = m_occupyingFactionManager.GetNearestRadioTower(m_CurrentTarget.location);
					if(!data.IsOccupyingFaction() && !m_occupyingFactionManager.m_CurrentQRF && m_occupyingFactionManager.m_iResources > m_Config.m_Difficulty.maxQRF)
					{
						if(!m_bCapturing)
						{
							m_bCapturing = true;
							OVT_Global.GetNotify().SendTextNotification("RadioTowerCapture",-1,OVT_Global.GetTowns().GetTownName(m_CurrentTarget.location));
						}else{
							m_iCaptureTimer = m_iCaptureTimer - timeSlice;
							if(m_iCaptureTimer <=0)
							{
								m_occupyingFactionManager.ChangeRadioTowerControl(data, m_Config.GetOccupyingFactionIndex());
								m_CurrentTarget.completed = true;
								m_CurrentTarget = null;									
								return;
							}		
						}			
					}
				}else if(!inrange){
					//Remove target and wait for new orders
					m_CurrentTarget.completed = true;
					m_CurrentTarget = null;
					return;
				}				
			}
		}
		
		if(HasTarget() && m_Groups.Count() == 0)
		{
			//Remove target and wait for new orders
			m_CurrentTarget = null;
		}
	}
	
	int SetTarget(OVT_TargetData target)
	{
		m_CurrentTarget = target;
		OVT_Faction faction = m_Config.GetOccupyingFaction();
		if(!faction) return 0;
		
		m_iCaptureTimer = 600000;
		m_bCapturing = false;
		
		//Send any remaining existing groups to new target
		int groupsSent = 0;
		int spent = 0;
		foreach(EntityID id : m_Groups)
		{
			SCR_AIGroup aigroup = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(id));
			if(!aigroup) continue;
			if(aigroup.GetAgentsCount() == 0) continue;
			
			array<AIWaypoint> waypoints = {};
			aigroup.GetWaypoints(waypoints);
			
			foreach(AIWaypoint wp : waypoints)
			{
				aigroup.RemoveWaypoint(wp);
			}
			
			AddWaypoints(aigroup);
			groupsSent++;
		}
		
		if(groupsSent == 0)
		{
			//Send specops
			spent += BuyPatrol(0, faction.GetRandomGroupByType(OVT_GroupType.SPECIAL_FORCES));			
		}
		return spent;
	}
	
	override void AddWaypoints(SCR_AIGroup aigroup)
	{
		if(!m_CurrentTarget) return;

		aigroup.AddWaypoint(m_Config.SpawnPatrolWaypoint(m_CurrentTarget.location));
		aigroup.AddWaypoint(m_Config.SpawnDefendWaypoint(m_CurrentTarget.location));		
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