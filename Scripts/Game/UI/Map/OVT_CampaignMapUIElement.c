class OVT_CampaignMapUIElement : SCR_MapUIElement
{
	protected string m_sFactionKey;
	protected Widget m_wBaseIcon;
	protected SCR_MilitarySymbolUIComponent m_wSymbolUI;
	
	protected ref ScriptInvoker m_OnMapIconEnter;
	protected ref ScriptInvoker m_OnMapIconClick;
	protected ref ScriptInvoker m_OnMapIconSelected;
	
	ScriptInvoker GetOnMapIconEnter()
	{
		if (!m_OnMapIconEnter)
			m_OnMapIconEnter = new ScriptInvoker();

		return m_OnMapIconEnter;
	}
	
	ScriptInvoker GetOnMapIconClick()
	{
		if (!m_OnMapIconClick)
			m_OnMapIconClick = new ScriptInvoker();

		return m_OnMapIconClick;
	}
	
	override bool OnMouseButtonDown(Widget w, int x, int y, int button)
	{
		if (m_OnMapIconClick)
			m_OnMapIconClick.Invoke(this);

		return false;
	}
	
	override void SelectIcon(bool invoke=true)
	{
		m_wSelectImg.SetVisible(true);
	}
	
	void DeselectIcon()
	{
		m_wSelectImg.SetVisible(false);
	}
	
	override bool OnMouseEnter(Widget w, int x, int y)
	{
		SCR_UITaskManagerComponent tm = SCR_UITaskManagerComponent.GetInstance();
		if (tm && !tm.IsTaskListOpen())
		{
			GetGame().GetWorkspace().SetFocusedWidget(w);
		}

		if (m_OnMapIconEnter)
			m_OnMapIconEnter.Invoke();

		super.OnMouseEnter(w, x, y);
		
		m_wHighlightImg.SetVisible(true);

		return false;
	}
	
	override bool OnMouseLeave(Widget w, Widget enterW, int x, int y)
	{
		super.OnMouseLeave(w, enterW, x, y);
		m_wHighlightImg.SetVisible(false);
		return false;
	}
	
	override void HandlerAttached(Widget w)
	{
		super.HandlerAttached(w);

		m_wBaseIcon = Widget.Cast(w.FindAnyWidget("SideSymbol"));
		m_wSymbolUI = SCR_MilitarySymbolUIComponent.Cast(m_wBaseIcon.FindHandler(SCR_MilitarySymbolUIComponent));				
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
		return vector.Zero;
	}
}