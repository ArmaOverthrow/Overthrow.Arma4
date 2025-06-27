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
	
	//! Force add player faction mapping on client (workaround for replication issues)
	void ForceClientFactionMapping(int playerId, int factionIndex)
	{
		// Create player faction info using the static factory method
		SCR_PlayerFactionInfo playerFactionInfo = SCR_PlayerFactionInfo.Create(playerId);
		playerFactionInfo.SetFactionIndex(factionIndex);
		
		// Add to protected mapping (we can access this since we extend SCR_FactionManager)
		m_MappedPlayerFactionInfo.Set(playerId, playerFactionInfo);
		
		Print(string.Format("[Overthrow] Force-added faction mapping for player %1 to faction index %2", playerId, factionIndex));
	}
}

