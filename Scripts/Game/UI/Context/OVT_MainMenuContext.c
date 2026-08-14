class OVT_MainMenuContext : OVT_UIContext
{
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Notification layout", params: "layout")]
	ResourceName m_NotificationLayout;
	
	ref OVT_MainMenuWidgets m_Widgets;
	OVT_TownManagerComponent m_TownManager;
	OVT_RealEstateManagerComponent m_RealEstate;
	
	OVT_MainMenuContextOverrideComponent m_FoundOverride;
	protected float m_fFoundRange = -1;

	//! Campaign request component this menu subscribed to for a pending save result.
	//! Cached rather than re-resolved: the controller seam can answer differently later in the session
	//! (a respawn or a reconnect re-assigns the controller), and the result can arrive after one.
	protected OVT_CampaignRequestComponent m_SaveRequestComms;
		
	override void PostInit()
	{		
		m_TownManager = OVT_Global.GetTowns();
		m_RealEstate = OVT_Global.GetRealEstate();
		m_Widgets = new OVT_MainMenuWidgets();
	}
	
	override void ShowLayout()
	{
		m_FoundOverride = null;
		m_fFoundRange = -1;
		GetGame().GetWorld().QueryEntitiesBySphere(m_Owner.GetOrigin(), 50, null, FindOverride, EQueryEntitiesFlags.ALL);
		
		if(m_FoundOverride)
		{
			OVT_UIManagerComponent ui = OVT_UIManagerComponent.Cast(m_Owner.FindComponent(OVT_UIManagerComponent));
			if(ui)
			{
				OVT_UIContext context = ui.GetContextByString(m_FoundOverride.m_ContextName);
				if(context)
				{
					// Check if context is already active to prevent multiple instances
					if(context.IsActive())
						return;
						
					if(m_FoundOverride.m_ContextName == "OVT_ShopContext")
					{
						OVT_ShopComponent shop = OVT_ComponentFinder<OVT_ShopComponent>.Find(m_FoundOverride.GetOwner());
						if(shop)
						{
							OVT_ShopContext shopContext = OVT_ShopContext.Cast(context);
							shopContext.SetShop(shop);
						}
					}
					context.ShowLayout();
					return;
				}
			}
		}
		
		OVT_Global.GetOverthrow().m_bHasOpenedMenu = true;
		
		super.ShowLayout();
	}
	
	protected bool FindOverride(IEntity entity)
	{
		OVT_MainMenuContextOverrideComponent found = OVT_MainMenuContextOverrideComponent.Cast(entity.FindComponent(OVT_MainMenuContextOverrideComponent));
		if(found) {
			float dist = vector.Distance(entity.GetOrigin(),m_Owner.GetOrigin());
			if(dist > found.m_fRange) return false;
			if(m_fFoundRange == -1 || m_fFoundRange > dist)
			{
				bool got = true;
				if(found.m_bMustOwnBase)
				{
					OVT_BaseData base = OVT_Global.GetOccupyingFaction().GetNearestBase(entity.GetOrigin());
					if(!base || base.IsOccupyingFaction())
					{
						got = false;
					}
				}
				if(got)
				{
					m_FoundOverride = found;
					m_fFoundRange = dist;
				}
			}			
		}
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
		img.LoadImageTexture(0, town.ControllingFactionData().GetUIInfo().GetIconPath());
				
		SCR_ButtonTextComponent comp;

		// Place
		comp = SCR_ButtonTextComponent.GetButtonText("Place", m_wRoot);
		if (comp)
		{
			//! This is the main menu's initial gamepad focus - without it a controller/console player
			//! opens the menu with nothing focused and has nothing to navigate from. It lives on the
			//! first entry wired up here, so it must MOVE with the first entry if the entry order ever
			//! changes (it was relocated off the deleted "Map Info" row by map/legacy-retirement).
			GetGame().GetWorkspace().SetFocusedWidget(comp.GetRootWidget());
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
		
		// Manage Recruits
		comp = SCR_ButtonTextComponent.GetButtonText("Manage Recruits", m_wRoot);
		if (comp)
		{
			comp.m_OnClicked.Insert(ManageRecruits);
		}
		
		// Character Sheet
		comp = SCR_ButtonTextComponent.GetButtonText("Character Sheet", m_wRoot);
		if (comp)
		{
			comp.m_OnClicked.Insert(CharacterSheet);
		}
		
		// Tips - the documented route to the modal tutorial popup. There is no gameplay keybinding
		// for escalating a HUD tip (risk R3 fired in Phase 5 and again in Phase 6), so this entry IS
		// the escalation path: the popup's prompt opens this menu and pushes its entry into
		// OVT_TutorialContext, and this button opens it.
		//
		// ALWAYS ENABLED (BUG-133). It used to be gated on HasEntry(), which looked like a courtesy and
		// was in fact a trap: "Don't show tips again" makes OVT_TutorialComponent drop every delivery
		// BEFORE an entry ever reaches the context, so HasEntry() was false forever afterwards, this
		// button was dead forever afterwards, and the only control that could turn tips back on lived
		// inside the popup it disabled. The screen behind it now resolves something to read for itself
		// (OVT_TutorialContext.ShowForReview), so there is nothing left for a gate to protect against.
		comp = SCR_ButtonTextComponent.GetButtonText("Tips", m_wRoot);
		if (comp)
		{
			comp.m_OnClicked.Insert(Tips);
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
		if (m_UIManager.GetContext(OVT_PlaceContext))
		{	// A hack, we need to close PlaceContext to avoid both of them being open at the same time
			OVT_PlaceContext.Cast(m_UIManager.GetContext(OVT_PlaceContext)).Cancel();
		}
		m_UIManager.ShowContext(OVT_BuildContext);		
	}
	
	private void RealEstate()
	{
		CloseLayout();
		m_UIManager.ShowContext(OVT_RealEstateContext);		
	}
	
	private void ManageRecruits()
	{
		CloseLayout();
		m_UIManager.ShowContext(OVT_RecruitsContext);		
	}
	
	private void CharacterSheet()
	{
		CloseLayout();
		m_UIManager.ShowContext(OVT_CharacterSheetContext);
	}

	//------------------------------------------------------------------------------------------------
	//! Opens the tutorial tip screen in its full, focusable form.
	//!
	//! ShowForReview() rather than ShowLayout(): the context resolves WHAT to show for itself - the tip
	//! from this session, else one the player has already read, else the welcome - so this menu never
	//! has to know the fallback chain, and the entry above never has to guess whether pressing it will
	//! do anything.
	//!
	//! It can still legitimately refuse - OVT_TutorialContext.CanShowLayout() will not open on top of a
	//! placement ghost, and the main menu IS reachable mid-placement; nor will it open when this
	//! install resolves no tutorial entries at all. Say so rather than looking broken.
	private void Tips()
	{
		CloseLayout();

		OVT_TutorialContext tutorial = OVT_TutorialContext.Cast(m_UIManager.GetContext(OVT_TutorialContext));
		if(!tutorial) return;

		tutorial.ShowForReview();

		if(!tutorial.IsActive())
			ShowHint("#OVT-Tutorial_NoneAvailable");
	}
	
	private void Save()
	{
		CloseLayout();
		if(!OVT_Global.GetResistanceFaction().IsLocalPlayerOfficer())
		{
			SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-MustBeOfficer");
			return;
		}

		OVT_CampaignRequestComponent comms = OVT_ControllerComponent<OVT_CampaignRequestComponent>.Get();
		if(!comms)
		{
			ShowSaveResult(false);
			return;
		}

		// Saving is asynchronous and can genuinely fail. Report what happened instead of claiming
		// success the instant the request is sent (BUG-006).
		comms.GetOnSaveResult().Remove(OnSaveResult);
		comms.GetOnSaveResult().Insert(OnSaveResult);
		m_SaveRequestComms = comms;
		comms.RequestSave();
	}

	//------------------------------------------------------------------------------------------------
	//! Result of the save this menu requested.
	//! \param[in] success Whether the server actually committed a save point.
	protected void OnSaveResult(bool success)
	{
		if(m_SaveRequestComms)
		{
			m_SaveRequestComms.GetOnSaveResult().Remove(OnSaveResult);
			m_SaveRequestComms = null;
		}

		ShowSaveResult(success);
	}

	//------------------------------------------------------------------------------------------------
	//! Shows the save outcome to the player.
	//! \param[in] success Whether the save succeeded.
	protected void ShowSaveResult(bool success)
	{
		SCR_HintManagerComponent hints = SCR_HintManagerComponent.GetInstance();
		if(!hints)
			return;

		if(success)
		{
			hints.ShowCustom("#OVT-Saved");
		}
		else
		{
			hints.ShowCustom("#OVT-SaveFailed");
		}
	}
}