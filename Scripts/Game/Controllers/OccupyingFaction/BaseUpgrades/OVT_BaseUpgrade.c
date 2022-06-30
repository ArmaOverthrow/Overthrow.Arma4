class OVT_BaseUpgrade : ScriptAndConfig
{
	[Attribute(defvalue: "200", UIWidgets.EditBox, desc: "Resources to allocate (-1 = ignore)")]
	int m_iResourceAllocation;
	
	[Attribute(defvalue: "1", UIWidgets.EditBox, desc: "Priority (1 = critical, 100 = max)")]
	int m_iPriority;
	
	[Attribute(defvalue: "0", UIWidgets.EditBox, desc: "Minimum threat level to appear")]
	int m_iMinimumThreat;
		
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
		return Spend((m_iResourceAllocation * m_Config.m_Difficulty.baseResourceCost) - GetResources(), threat);
	}
	
	protected OVT_FactionComposition GetCompositionConfig(string tag)
	{
		OVT_Faction faction = m_Config.GetOccupyingFaction();
		return faction.GetCompositionConfig(tag);
	}
	
	OVT_BaseUpgradeStruct Serialize()
	{
		OVT_BaseUpgradeStruct struct = new OVT_BaseUpgradeStruct();
		struct.m_sType = ClassName();
		struct.m_iResources = GetResources();
		
		return struct;		
	}
	
	bool Deserialize(OVT_BaseUpgradeStruct struct)
	{
		Spend(struct.m_iResources, m_iMinimumThreat);
		
		return true;
	}
}

class OVT_BaseUpgradeStruct : SCR_JsonApiStruct
{
	string m_sType;
	int m_iResources;
	vector m_vLocation;
	ref array<ref OVT_BaseUpgradeGroupStruct> m_aGroups = {};
	ref array<ref OVT_VehicleStruct> m_aVehicles = {};
	string m_sTag = "";
		
	void OVT_BaseUpgradeStruct()
	{
		RegV("m_sType");
		RegV("m_iResources");
		RegV("m_vLocation");
		RegV("m_aGroups");
		RegV("m_aVehicles");
		RegV("m_sTag");
	}
}

class OVT_BaseUpgradeGroupStruct : SCR_JsonApiStruct
{
	string m_sType;
	vector m_vLocation;
	
	void OVT_BaseUpgradeGroupStruct()
	{
		RegV("m_sType");
		RegV("m_vLocation");
	}
}