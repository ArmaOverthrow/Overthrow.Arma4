class OVT_BasePatrolUpgrade : OVT_BaseUpgrade
{	
	protected ref array<int> m_aGroups = new array<int>;
	protected const int UPDATE_FREQUENCY = 10000;
	protected int m_iNumGroups = 0;
	
	override void PostInit()
	{		
		
		float freq = s_AIRandomGenerator.RandFloatXY(UPDATE_FREQUENCY - 1000, UPDATE_FREQUENCY + 1000);
				
		GetGame().GetCallqueue().CallLater(CheckUpdate, freq, true, m_BaseController.GetOwner());	
	}
	
	protected void CheckUpdate()
	{
		
	}
			
	protected int BuyPatrol(float threat, ResourceName res = "", vector pos = "0 0 0", OVT_GroupOrder order = OVT_GroupOrder.PATROL)
	{
		m_iNumGroups++;
		
		OVT_Faction faction = OVT_Global.GetConfig().GetOccupyingFaction();
		if(!faction) return 0;
				
		if(res == ""){
			res = faction.GetRandomGroupByType(OVT_GroupType.LIGHT_INFANTRY);
			if(threat > 25) res = faction.GetRandomGroupByType(OVT_GroupType.HEAVY_INFANTRY);
		}
		
		BaseWorld world = GetGame().GetWorld();
					
		if(pos[0] == 0)
		{
			pos = s_AIRandomGenerator.GenerateRandomPointInRadius(5,50, m_BaseController.GetOwner().GetOrigin());			
		}
		
		float surfaceY = world.GetSurfaceY(pos[0], pos[2]);
		if (pos[1] < surfaceY)
		{
			pos[1] = surfaceY;
		}
		
		array<vector> waypoints();
		if(GetWaypoints(waypoints))
		{
			OVT_VirtualizationManagerComponent virtualization = OVT_Global.GetVirtualization();
			OVT_VirtualizedGroupData data = virtualization.Create(res, pos, waypoints, false, order);
			m_aGroups.Insert(data.id);
		}
		
		int newres = 8 * OVT_Global.GetConfig().m_Difficulty.baseResourceCost;
			
		return newres;
	}
	
	protected void AddWaypoints(SCR_AIGroup aigroup)
	{
		
	}
	
	protected bool GetWaypoints(inout array<vector> waypoints)
	{
		return true;
	}
	
	override int GetResources()
	{
		return 0;
	}
	
	override int Spend(int resources, float threat)
	{
		int spent = 0;
		
		while(resources > 0)
		{			
			OVT_Faction faction = OVT_Global.GetConfig().GetOccupyingFaction();
			ResourceName res = faction.GetRandomGroupByType(OVT_GroupType.LIGHT_INFANTRY);
			vector randompos = OVT_Global.GetRandomNonOceanPositionNear(m_BaseController.GetOwner().GetOrigin(), 20);			
			int newres = BuyPatrol(threat, res, randompos);	
			
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
	
	override OVT_BaseUpgradeData Serialize()
	{
		OVT_BaseUpgradeData struct = super.Serialize();
						
		return struct;
	}
	
	override bool Deserialize(OVT_BaseUpgradeData struct)
	{
		
		return true;
	}
	
	void ~OVT_BasePatrolUpgrade()
	{
		GetGame().GetCallqueue().Remove(CheckUpdate);	
	}
}