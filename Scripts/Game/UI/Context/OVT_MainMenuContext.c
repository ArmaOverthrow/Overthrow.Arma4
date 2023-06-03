class OVT_MainMenuContext : OVT_UIContext
{
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Notification layout", params: "layout")]
	ResourceName m_NotificationLayout;
	
	ref OVT_MainMenuWidgets m_Widgets;
	OVT_TownManagerComponent m_TownManager;
	OVT_RealEstateManagerComponent m_RealEstate;
	
	OVT_MainMenuContextOverrideComponent m_FoundOverride;
		
	override void PostInit()
	{		
		m_TownManager = OVT_Global.GetTowns();
		m_RealEstate = OVT_Global.GetRealEstate();
		m_Widgets = new OVT_MainMenuWidgets();
	}
	
	override void ShowLayout()
	{
		m_FoundOverride = null;
		GetGame().GetWorld().QueryEntitiesBySphere(m_Owner.GetOrigin(), 5, null, FindOverride, EQueryEntitiesFlags.ALL);
		
		if(m_FoundOverride)
		{
			OVT_UIManagerComponent ui = OVT_UIManagerComponent.Cast(m_Owner.FindComponent(OVT_UIManagerComponent));
			if(ui)
			{
				OVT_UIContext context = ui.GetContextByString(m_FoundOverride.m_ContextName);
				if(context)
				{
					context.ShowLayout();
					return;
				}
			}
		}
		
		super.ShowLayout();
	}
	
	protected bool FindOverride(IEntity entity)
	{
		OVT_MainMenuContextOverrideComponent found = OVT_MainMenuContextOverrideComponent.Cast(entity.FindComponent(OVT_MainMenuContextOverrideComponent));
		if(found) m_FoundOverride = found;
		return false;
	}
	
	override void OnShow()
	{		
		m_Widgets.Init(m_wRoot);
		
		OVT_TownData town = m_TownManager.GetNearestTown(m_Owner.GetOrigin());
		int townID = m_TownManager.GetTownID(town);
		m_Widgets.m_TownNameText.SetText(m_TownManager.GetTownName(townID));
		m_Widgets.m_TownInfoText.SetTextFormat("#OVT-Population: %1\n#OVT-Stability: %2%\n#OVT-Supporters: %3 (%4%)", town.population, town.stability, town.support, town.SupportPercentage());
		
		ImageWidget img = ImageWidget.Cast(m_wRoot.FindAnyWidget("ControllingFaction"));
		img.LoadImageTexture(0, town.ControllingFaction().GetUIInfo().GetIconPath());
				
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
		
		// Real Estate
		comp = SCR_ButtonTextComponent.GetButtonText("Real Estate", m_wRoot);
		if (comp)
		{
			IEntity building = m_RealEstate.GetNearestBuilding(m_Owner.GetOrigin());
			if(!m_RealEstate.BuildingIsOwnable(building)){
				comp.SetEnabled(false);
			}else{
				comp.m_OnClicked.Insert(RealEstate);
			}
		}
		
		// Save
		comp = SCR_ButtonTextComponent.GetButtonText("Save", m_wRoot);
		if (comp)
		{
			comp.m_OnClicked.Insert(Save);
		}
		
		//Logs
		Widget container = m_wRoot.FindAnyWidget("LogContainer");
		Widget child = container.GetChildren();
		while(child)
		{
			container.RemoveChild(child);
			child = container.GetChildren();
		}
		
		WorkspaceWidget workspace = GetGame().GetWorkspace(); 
		
		OVT_NotificationManagerComponent notify = OVT_Global.GetNotify();
		foreach(OVT_NotificationData data : notify.m_aNotifications)
		{
			Widget w = workspace.CreateWidgets(m_NotificationLayout, container);
			TextWidget tw = TextWidget.Cast(w.FindAnyWidget("NotificationText"));
			tw.SetTextFormat(data.msg.m_UIInfo.GetDescription(),data.param1,data.param2,data.param3);
			TextWidget time = TextWidget.Cast(w.FindAnyWidget("Time"));
			time.SetTextFormat("%1:%2",data.time.m_iHours.ToString(2),data.time.m_iMinutes.ToString(2));
			
			
			if(data.msg.m_UIInfo.GetIconSetName() != "")
			{
				ImageWidget icon = ImageWidget.Cast(w.FindAnyWidget("Icon"));
				data.msg.m_UIInfo.SetIconTo(icon);
			}			
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
	
	private void RealEstate()
	{
		CloseLayout();
		m_UIManager.ShowContext(OVT_RealEstateContext);		
	}
	
	private void Save()
	{
		CloseLayout();
		if(!OVT_Global.GetResistanceFaction().IsLocalPlayerOfficer())
		{
			SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-MustBeOfficer");
			return;
		}
		OVT_Global.GetServer().RequestSave();
		SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-Saved");
	}
}