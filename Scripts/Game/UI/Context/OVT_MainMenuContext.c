class OVT_MainMenuContext : OVT_UIContext
{
	ref OVT_MainMenuWidgets m_Widgets;
	OVT_TownManagerComponent m_TownManager;
		
	override void PostInit()
	{		
		m_TownManager = OVT_Global.GetTowns();
		m_Widgets = new OVT_MainMenuWidgets();
	}
	
	override void OnShow()
	{		
		m_Widgets.Init(m_wRoot);
		
		OVT_TownData town = m_TownManager.GetNearestTown(m_Owner.GetOrigin());
		SCR_MapDescriptorComponent desc = m_TownManager.GetNearestTownMarker(town.location);
		
		m_Widgets.m_TownNameText.SetText(desc.Item().GetDisplayName());
		m_Widgets.m_TownInfoText.SetTextFormat("#OVT-Population:%1\n#OVT-Stability: %2%\n#OVT-Support: %3%", town.population, town.stability, town.SupportPercentage());
		
		
		SCR_ButtonTextComponent comp;

		// Map Info
		comp = SCR_ButtonTextComponent.GetButtonText("Map Info", m_wRoot);
		if (comp)
		{
			GetGame().GetWorkspace().SetFocusedWidget(comp.GetRootWidget());
			comp.m_OnClicked.Insert(MapInfo);
		}
		
		// Fast Travel
		comp = SCR_ButtonTextComponent.GetButtonText("Fast Travel", m_wRoot);
		if (comp)
		{
			comp.m_OnClicked.Insert(FastTravel);
		}
		
		// Place
		comp = SCR_ButtonTextComponent.GetButtonText("Place", m_wRoot);
		if (comp)
		{
			comp.m_OnClicked.Insert(Place);
		}
	}
	
	private void MapInfo()
	{
		CloseLayout();
		OVT_MapContext.Cast(m_UIManager.GetContext(OVT_MapContext)).EnableMapInfo();		
	}
	
	private void FastTravel()
	{
		CloseLayout();
		OVT_MapContext.Cast(m_UIManager.GetContext(OVT_MapContext)).EnableFastTravel();		
	}
	
	private void Place()
	{
		CloseLayout();
		m_UIManager.ShowContext(OVT_PlaceContext);		
	}
}