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
				
		m_Widgets.m_TownNameText.SetText(m_TownManager.GetTownName(town.id));
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
		
		// Resistance
		comp = SCR_ButtonTextComponent.GetButtonText("Resistance", m_wRoot);
		if (comp)
		{
			comp.m_OnClicked.Insert(Resistance);
		}
		
		// Jobs
		comp = SCR_ButtonTextComponent.GetButtonText("Jobs", m_wRoot);
		if (comp)
		{
			comp.m_OnClicked.Insert(Jobs);
		}
		
		// Build
		comp = SCR_ButtonTextComponent.GetButtonText("Build", m_wRoot);
		if (comp)
		{
			comp.m_OnClicked.Insert(Build);
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
	
	private void Resistance()
	{
		CloseLayout();
		m_UIManager.ShowContext(OVT_ResistanceMenuContext);		
	}
	
	private void Jobs()
	{
		CloseLayout();
		m_UIManager.ShowContext(OVT_JobsContext);		
	}
	
	private void Build()
	{
		CloseLayout();
		m_UIManager.ShowContext(OVT_BuildContext);		
	}
}