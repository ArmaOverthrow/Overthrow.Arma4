class OVT_OverthrowFactionManagerClass: SCR_FactionManagerClass
{
};

class OVT_OverthrowFactionManager : SCR_FactionManager
{
	[Attribute("", UIWidgets.Object, "These configs extend the above configs to provide nore data that Overthrow needs")]
	protected ref array<ref OVT_Faction> m_aOverthrowFactions;
	
	OVT_Faction GetOverthrowFactionByIndex(int index)
	{
		string key = GetFactionByIndex(index).GetFactionKey();
		return GetOverthrowFactionByKey(key);
	}
	
	OVT_Faction GetOverthrowFactionByKey(string key)
	{
		foreach(OVT_Faction faction : m_aOverthrowFactions)
		{
			if(faction.m_sFactionKey == key) return faction;
		}
		return null;
	}
	
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		
		foreach(OVT_Faction faction : m_aOverthrowFactions)
		{
			faction.Init();
		}
	}
}

