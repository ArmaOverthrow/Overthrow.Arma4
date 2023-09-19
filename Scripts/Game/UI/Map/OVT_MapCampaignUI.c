class OVT_MapCampaignUI : SCR_MapUIElementContainer
{
	[Attribute("{9B45BD0282167D90}UI/Layouts/Map/BaseElement.layout", params: "layout")]
	protected ResourceName m_sBaseElement;
	
	[Attribute("{B6846D46BDF6311E}UI/Layouts/Map/TownElement.layout", params: "layout")]
	protected ResourceName m_sTownElement;
	
	protected OVT_CampaignMapUIElement m_SelectedElement;
	protected OVT_MapInfoUI m_MapInfo;
	
	override void Init()
	{
		super.Init();
		
		OVT_MapInfoUI mapInfo = OVT_MapInfoUI.Cast(m_MapEntity.GetMapUIComponent(OVT_MapInfoUI));
		if (mapInfo)
		{
			m_MapInfo = mapInfo;
		}
	}
	
	protected void InitBases()
	{
		OVT_OccupyingFactionManager of = OVT_Global.GetOccupyingFaction();
		if(!of) return;
		
		foreach (OVT_BaseData baseData : of.m_Bases)
		{
			Widget w = GetGame().GetWorkspace().CreateWidgets(m_sBaseElement, m_wIconsContainer);
			OVT_CampaignMapUIBase handler = OVT_CampaignMapUIBase.Cast(w.FindHandler(OVT_CampaignMapUIBase));
			
			if (!handler)
				return;
			
			handler.GetOnMapIconClick().Insert(OnBaseClick);

			handler.SetParent(this);
			handler.InitBase(baseData);
			m_mIcons.Set(w, handler);

			FrameSlot.SetSizeToContent(w, true);
			FrameSlot.SetAlignment(w, 0.5, 0.5);
		}		
	}
	
	protected void InitTowns()
	{
		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if(!towns) return;
		
		foreach(OVT_TownData town : towns.m_Towns)
		{
			Widget w = GetGame().GetWorkspace().CreateWidgets(m_sTownElement, m_wIconsContainer);
			OVT_CampaignMapUITown handler = OVT_CampaignMapUITown.Cast(w.FindHandler(OVT_CampaignMapUITown));
			
			if (!handler)
				return;
			
			handler.GetOnMapIconClick().Insert(OnTownClick);

			handler.SetParent(this);
			handler.InitTown(town);
			m_mIcons.Set(w, handler);

			FrameSlot.SetSizeToContent(w, true);
			FrameSlot.SetAlignment(w, 0.5, 0.5);
		}
	}
	
	protected void OnBaseClick(OVT_CampaignMapUIBase handler)
	{
		Print("Base Clicked");
		
		if(m_SelectedElement) m_SelectedElement.DeselectIcon();
		handler.SelectIcon();
		m_SelectedElement = handler;
	}
	
	protected void OnTownClick(OVT_CampaignMapUITown handler)
	{
		Print("Town Clicked");
		
		if(m_SelectedElement) m_SelectedElement.DeselectIcon();
		handler.SelectIcon();
		m_SelectedElement = handler;
		
		if(m_MapInfo)
		{
			m_MapInfo.SelectTown(handler.GetTownData());
		}
	}
	
	override void OnMapOpen(MapConfiguration config)
	{
		super.OnMapOpen(config);

		InitBases();
		InitTowns();
		UpdateIcons();
	}
}