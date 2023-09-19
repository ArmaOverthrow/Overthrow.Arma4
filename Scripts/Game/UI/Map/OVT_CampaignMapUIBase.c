class OVT_CampaignMapUIBase : OVT_CampaignMapUIElement
{
	protected OVT_BaseData m_BaseData;	
	
	void InitBase(OVT_BaseData baseData)
	{
		m_BaseData = baseData;

		InitBaseIcon();
	}
	
	protected void InitBaseIcon()
	{
		if (!m_BaseData)
			return;

		Faction f = GetGame().GetFactionManager().GetFactionByKey(m_BaseData.faction);
		
		SetIconFaction(f);	
		
		if(!m_wSymbolUI) return;
		
		SCR_MilitarySymbol baseIcon = new SCR_MilitarySymbol();
		
		if(m_BaseData.IsOccupyingFaction())
		{
			baseIcon.SetIdentity(EMilitarySymbolIdentity.OPFOR);
		}else{
			baseIcon.SetIdentity(EMilitarySymbolIdentity.BLUFOR);
		}
		
		baseIcon.SetDimension(EMilitarySymbolDimension.INSTALLATION);
		
		m_wSymbolUI.Update(baseIcon);
	}
	
	override vector GetPos()
	{
		if (m_BaseData)
			return m_BaseData.location;
		
		return vector.Zero;
	}
}