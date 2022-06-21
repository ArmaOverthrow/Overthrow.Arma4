class OVT_BaseUpgrade : ScriptAndConfig
{
	[Attribute(defvalue: "200", UIWidgets.EditBox, desc: "Resources to allocate (-1 = ignore)")]
	int m_iResourceAllocation;
	
	[Attribute(defvalue: "1", UIWidgets.EditBox, desc: "Priority (1 = critical, 100 = max)")]
	int m_iPriority;
		
	OVT_BaseControllerComponent m_BaseController;
	OVT_OccupyingFactionManager m_occupyingFactionManager;
	OVT_EconomyManagerComponent m_Economy;
	OVT_OverthrowConfigComponent m_Config;
	OVT_Faction m_Faction;
	
	void Init(OVT_BaseControllerComponent base, OVT_OccupyingFactionManager occupyingFactionManager, OVT_OverthrowConfigComponent config)
	{
		m_BaseController = base;
		m_occupyingFactionManager = occupyingFactionManager;		
		m_Config = config;
		m_Economy = OVT_Global.GetEconomy();
		
		m_Faction = m_Config.GetOccupyingFaction();
		
		PostInit();
	}
	
	void PostInit()
	{
	
	}
	
	void OnUpdate(int timeSlice)
	{
	
	}
	
	int GetResources()
	{
		return 0;
	}
	
	int Spend(int resources, float threat)
	{
		return 0;
	}
	
	int SpendToAllocation(float threat)
	{
		return Spend(m_iResourceAllocation - GetResources(), threat);
	}
}