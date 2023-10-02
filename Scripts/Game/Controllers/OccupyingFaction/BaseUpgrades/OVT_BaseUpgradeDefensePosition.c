class OVT_BaseUpgradeDefensePosition : OVT_BasePatrolUpgrade
{
	ref map<int,int> m_Guards;
	
	override void PostInit()
	{
		super.PostInit();
		
		m_Guards = new map<int,int>;
	}
	
	override int Spend(int resources, float threat)
	{
		int spent = 0;
		
		foreach(int id, vector pos : m_BaseController.m_aDefendPositions)
		{
			if(resources < OVT_Global.GetConfig().m_Difficulty.baseResourceCost * 4) break;
			
			bool needsGuard = false;
			if(!m_Guards.Contains(id))
			{
				needsGuard = true;
			}else{
				//Check if still exists
				OVT_VirtualizedGroupData data = OVT_VirtualizedGroupData.Get(m_Guards[id]);
				if(!data) needsGuard = true;
			}
			if(needsGuard)
			{
				if(BuyGuard(id, pos))
				{
					int newres = OVT_Global.GetConfig().m_Difficulty.baseResourceCost * 4;
					resources -= newres;
					spent += newres;
				}							
			}
		}
		
		return spent;
	}
	
	protected bool BuyGuard(int id, vector pos)
	{	
		vector spawnpos = OVT_Global.GetRandomNonOceanPositionNear(m_BaseController.GetOwner().GetOrigin(), 50);			
		OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
		
		ResourceName res = OVT_Global.GetConfig().GetOccupyingFaction().m_aHeavyInfantryPrefabSlots[0];
		
		array<vector> waypoints();
		OVT_VirtualizedGroupData data = virtualization.Create(res, pos, waypoints);
		m_Guards[id] = data.id;
		m_aGroups.Insert(data.id);
					
		return true;
	}
}