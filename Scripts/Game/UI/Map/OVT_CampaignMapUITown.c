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
		
		if(!m_wSymbolUI) return;
		
		SCR_MilitarySymbol baseIcon = new SCR_MilitarySymbol();
		
		if(m_TownData.IsOccupyingFaction())
		{
			baseIcon.SetIdentity(EMilitarySymbolIdentity.OPFOR);
		}else{
			baseIcon.SetIdentity(EMilitarySymbolIdentity.BLUFOR);
		}
		
		baseIcon.SetDimension(EMilitarySymbolDimension.INSTALLATION);
		
		m_wSymbolUI.Update(baseIcon);
	}
}