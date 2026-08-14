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
	OVT_Faction m_Faction;
	
	void Init(OVT_BaseControllerComponent base, OVT_OccupyingFactionManager occupyingFactionManager, OVT_OverthrowConfigComponent config)
	{
		m_BaseController = base;
		m_occupyingFactionManager = occupyingFactionManager;		
		OVT_Global.GetConfig() = config;
		m_Economy = OVT_Global.GetEconomy();
		
		m_Faction = OVT_Global.GetConfig().GetOccupyingFaction();
		
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
	
	int SpendToAllocation(float threat, int max = -1)
	{
		int toSpend = (m_iResourceAllocation * OVT_Global.GetConfig().m_Difficulty.baseResourceCost) - GetResources();
		if(max >= 0 && toSpend > max) toSpend = max;
		return Spend(toSpend, threat);
	}
	
	protected OVT_FactionComposition GetCompositionConfig(string tag)
	{
		OVT_Faction faction = OVT_Global.GetConfig().GetOccupyingFaction();
		return faction.GetCompositionConfig(tag);
	}

	//------------------------------------------------------------------------------------------------
	//! Positional index of this upgrade's base in OVT_OccupyingFactionManager.m_Bases, for the GM
	//! group registry. That index - not OVT_BaseData.id - is the join key clients already receive
	//! through the occupying faction's JIP stream, so it is what a GM record must carry.
	//!
	//! Every dereference is guarded and -1 means "not resolvable". This is called from inside spawn
	//! paths whose behaviour must not change, so it must be incapable of throwing.
	//! \return The base index, or -1.
	protected int GetBaseOriginIndex()
	{
		if (!m_BaseController) return -1;

		OVT_OccupyingFactionManager occupying = m_occupyingFactionManager;
		if (!occupying) occupying = OVT_Global.GetOccupyingFaction();
		if (!occupying) return -1;

		IEntity owner = m_BaseController.GetOwner();
		if (!owner) return -1;

		OVT_BaseData data = occupying.GetNearestBase(owner.GetOrigin());
		if (!data) return -1;

		return occupying.GetBaseIndex(data);
	}
	
	OVT_BaseUpgradeData Serialize()
	{
		OVT_BaseUpgradeData struct = new OVT_BaseUpgradeData();
		struct.type = ClassName();
		struct.resources = GetResources();
		
		return struct;		
	}
	
	bool Deserialize(OVT_BaseUpgradeData struct)
	{
		if(!m_BaseController.IsOccupyingFaction()) return true;
		Spend(struct.resources, m_iMinimumThreat);
		
		return true;
	}
}