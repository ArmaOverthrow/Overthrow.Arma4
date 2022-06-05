class OVT_MainMenu
{
	OVT_MainMenuWidgets m_Widgets;
	OVT_TownManagerComponent m_TownManager;
	OVT_UIManagerComponent m_UIManager;
	
	void OnUpdate(IEntity player)
	{
		if(!m_TownManager) return;
		
		OVT_TownData town = m_TownManager.GetNearestTown(player.GetOrigin());
		
		m_Widgets.m_TownNameText.SetText(town.name);
		m_Widgets.m_TownInfoText.SetTextFormat("#OVT-Population:%1\n#OVT-Stability: %2%\n#OVT-Support: %3%", town.population, town.stability, town.support);
	}
	
	void OVT_MainMenu(OVT_MainMenuWidgets widgets, Widget root, OVT_UIManagerComponent uimanager)
	{
		m_TownManager = OVT_TownManagerComponent.GetInstance();
		m_Widgets = widgets;
		m_UIManager = uimanager;
		
		SCR_ButtonTextComponent comp;

		// Map Info
		comp = SCR_ButtonTextComponent.GetButtonText("Map Info", root);
		if (comp)
		{
			GetGame().GetWorkspace().SetFocusedWidget(comp.GetRootWidget());
			comp.m_OnClicked.Insert(MapInfo);
		}
		
		// Place
		comp = SCR_ButtonTextComponent.GetButtonText("Place", root);
		if (comp)
		{
			comp.m_OnClicked.Insert(Place);
		}
	}
	
	private void MapInfo()
	{
		Print("Map Info button was clicked");
	}
	
	private void Place()
	{
		m_UIManager.OpenPlaceMenu();		
	}
}