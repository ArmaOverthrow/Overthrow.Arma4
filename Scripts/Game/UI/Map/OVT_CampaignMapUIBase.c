class OVT_CampaignMapUIBase : SCR_MapUIElement
{
	protected OVT_BaseData m_BaseData;
	protected string m_sFactionKey;
	protected Widget m_wBaseIcon;
	protected SCR_MilitarySymbolUIComponent m_wSymbolUI;
	
	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);

		m_wBaseIcon = Widget.Cast(w.FindAnyWidget("SideSymbol"));
		m_wSymbolUI = SCR_MilitarySymbolUIComponent.Cast(m_wBaseIcon.FindHandler(SCR_MilitarySymbolUIComponent));				
	}
	
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
	
	protected void SetIconFaction(Faction faction)
	{
		if(!faction) return;
		m_sFactionKey = faction.GetFactionKey();
		SetBaseIconFactionColor(faction);
	}
	
	string GetFactionKey()
	{
		return m_sFactionKey;
	}
	//------------------------------------------------------------------------------------------------
	Color GetFactionColor()
	{
		return GetColorForFaction(m_sFactionKey);
	}
		
	void SetBaseIconFactionColor(Faction faction)
	{
		if (!m_wBaseIcon)
			return;

		Color color;
		
		if (faction)
			color = faction.GetFactionColor();
		else
			color = GetColorForFaction("");

		m_wBaseIcon.SetColor(color);
		if (m_wGradient)
			m_wGradient.SetColor(color);
	}
	
	override vector GetPos()
	{
		if (m_BaseData)
			return m_BaseData.location;
		
		return vector.Zero;
	}
}