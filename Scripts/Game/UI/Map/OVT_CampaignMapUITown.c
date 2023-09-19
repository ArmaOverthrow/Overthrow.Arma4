class OVT_CampaignMapUITown : OVT_CampaignMapUIElement
{
	protected OVT_TownData m_TownData;	
	
	void InitTown(OVT_TownData townData)
	{
		m_TownData = townData;
		InitTownIcon();
	}
	
	OVT_TownData GetTownData()
	{
		return m_TownData;
	}
	
	override vector GetPos()
	{
		if (m_TownData)
			return m_TownData.location;
		
		return vector.Zero;
	}
	
	protected void InitTownIcon()
	{
		if (!m_TownData)
			return;

		Faction f = GetGame().GetFactionManager().GetFactionByIndex(m_TownData.faction);
		
		SetIconFaction(f);	
		
		SizeLayoutWidget size = SizeLayoutWidget.Cast(m_wBaseIcon.GetChildren());
		if(!size) return;
		
		if(m_TownData.size == 1)
		{
			size.SetWidthOverride(16);
			size.SetHeightOverride(16);
		}
		if(m_TownData.size == 2)
		{
			size.SetWidthOverride(24);
			size.SetHeightOverride(24);
		}
		if(m_TownData.size == 3)
		{
			size.SetWidthOverride(32);
			size.SetHeightOverride(32);
		}
	}
}