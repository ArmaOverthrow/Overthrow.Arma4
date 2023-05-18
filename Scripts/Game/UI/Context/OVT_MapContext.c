class OVT_MapContext : OVT_UIContext
{
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Modifier Layout", params: "layout")]
	ResourceName m_ModLayout;
	
	[Attribute(uiwidget: UIWidgets.ColorPicker)]
	ref Color m_NegativeModifierColor;
	
	[Attribute(uiwidget: UIWidgets.ColorPicker)]
	ref Color m_PositiveModifierColor;
	
	OVT_TownManagerComponent m_TownManager;
	OVT_RealEstateManagerComponent m_RealEstate;
	OVT_ResistanceFactionManager m_Resistance;
	OVT_OccupyingFactionManager m_OccupyingFaction;
	
	OVT_TownData m_SelectedTown;
	
	protected bool m_bMapInfoActive = false;
	protected bool m_bFastTravelActive = false;
	protected bool m_bBusTravelActive = false;
	
	protected const int MAX_HOUSE_TRAVEL_DIS = 25;
	protected const int MAX_FOB_TRAVEL_DIS = 40;
	protected const int MIN_TRAVEL_DIS = 500;
	
	override void PostInit()
	{		
		m_TownManager = OVT_Global.GetTowns();
		m_RealEstate = OVT_Global.GetRealEstate();
		m_Resistance = OVT_Global.GetResistanceFaction();
		m_OccupyingFaction = OVT_Global.GetOccupyingFaction();
		
		SCR_MapEntity.GetOnMapClose().Insert(DisableMapInfo);
		SCR_MapEntity.GetOnMapClose().Insert(DisableFastTravel);
		
	}
	
	SCR_MapGadgetComponent GetMap()
	{
		SCR_GadgetManagerComponent mgr = SCR_GadgetManagerComponent.Cast(m_Owner.FindComponent(SCR_GadgetManagerComponent));
		if(!mgr) return null;
		
		IEntity ent = mgr.GetQuickslotGadgetByType(EGadgetType.MAP);
		if(!ent) return null;
				
		SCR_MapGadgetComponent comp = SCR_MapGadgetComponent.Cast(ent.FindComponent(SCR_MapGadgetComponent));
		if(!comp) return null;
		
		return comp;
	}
	
	bool CanFastTravel(vector pos, out string reason)
	{	
		if(m_Config.m_bDebugMode) return true;
		
		reason = "#OVT-CannotFastTravelThere";	
		float dist;
		
		dist = vector.Distance(pos, m_Owner.GetOrigin());
		if(dist < MIN_TRAVEL_DIS)
		{
			reason = "#OVT-CannotFastTravelDistance";
			return false;	
		}
		
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (!character)
			return false;
		
		OVT_PlayerWantedComponent m_Wanted = OVT_PlayerWantedComponent.Cast(character.FindComponent(OVT_PlayerWantedComponent));
		if(m_Wanted.GetWantedLevel() > 0)
		{
			reason = "#OVT-CannotFastTravelWanted";
			return false;
		}		
		
		if(m_OccupyingFaction.m_bQRFActive && m_Config.m_Difficulty.QRFFastTravelMode != OVT_QRFFastTravelMode.FREE)
		{
			if(m_Config.m_Difficulty.QRFFastTravelMode == OVT_QRFFastTravelMode.DISABLED)
			{
				reason = "#OVT-CannotFastTravelDuringQRF";
				return false;
			}
			dist = vector.Distance(m_OccupyingFaction.m_vQRFLocation, pos);		
			if(dist < OVT_QRFControllerComponent.QRF_RANGE)
			{
				reason = "#OVT-CannotFastTravelToQRF";
				return false;
			}
		}
		
		if(m_Resistance.DistanceToNearestCamp(pos) < MAX_HOUSE_TRAVEL_DIS)
		{
			return true;
		}
		
		IEntity house = m_RealEstate.GetNearestOwned(m_sPlayerID, pos);
		if(house)
		{
			if(m_RealEstate.IsRented(house.GetID())) return false;
			OVT_RealEstateConfig config = m_RealEstate.GetConfig(house);
			if(config.m_IsWarehouse) return false;
			dist = vector.Distance(house.GetOrigin(), pos);				
			if(dist < MAX_HOUSE_TRAVEL_DIS) return true;
		}
		
		house = m_RealEstate.GetNearestRented(m_sPlayerID, pos);
		if(house)
		{
			OVT_RealEstateConfig config = m_RealEstate.GetConfig(house);
			if(config.m_IsWarehouse) return false;
			dist = vector.Distance(house.GetOrigin(), pos);							
			if(dist < MAX_HOUSE_TRAVEL_DIS) return true;
		}
		
		vector fob = m_Resistance.GetNearestFOB(pos);		
		dist = vector.Distance(fob, pos);
		if(dist < MAX_FOB_TRAVEL_DIS) return true;		
		
		OVT_BaseData base = m_OccupyingFaction.GetNearestBase(pos);
		if(base && !base.IsOccupyingFaction())
		{
			dist = vector.Distance(base.location, pos);
			if(dist < base.closeRange) return true;
		}
		
		return false;
	}
	
	bool ShowMap()
	{
		SCR_MapGadgetComponent comp = GetMap();
		if(!comp) return false;
		
		comp.SetMapMode(true);
		return true;
	}
	
	void HideMap()
	{
		SCR_MapGadgetComponent comp = GetMap();
		if(!comp) return;
		
		comp.SetMapMode(false);
	}
	
	void EnableMapInfo()
	{		
		if(!ShowMap())
		{
			ShowHint("#OVT-MustHaveMap");
			return;
		}
		m_bMapInfoActive = true;
		
		ShowLayout();
		m_SelectedTown = m_TownManager.GetNearestTown(m_Owner.GetOrigin());
		
		ShowTownInfo();
	}
	
	protected void ShowTownInfo()
	{
		if(!m_wRoot) return;
		if(!m_SelectedTown) return;
		
		ImageWidget img = ImageWidget.Cast(m_wRoot.FindAnyWidget("ControllingFaction"));
		img.LoadImageTexture(0, m_SelectedTown.ControllingFaction().GetUIInfo().GetIconPath());
				
		TextWidget widget = TextWidget.Cast(m_wRoot.FindAnyWidget("TownName"));
		widget.SetText(m_TownManager.GetTownName(m_SelectedTown.id));
		
		widget = TextWidget.Cast(m_wRoot.FindAnyWidget("Population"));
		widget.SetText(m_SelectedTown.population.ToString());
		
		widget = TextWidget.Cast(m_wRoot.FindAnyWidget("Distance"));
		float distance = vector.Distance(m_SelectedTown.location, m_Owner.GetOrigin());
		string dis, units;
		SCR_Global.GetDistForHUD(distance, false, dis, units);
		widget.SetText(dis + " " + units);
		
		widget = TextWidget.Cast(m_wRoot.FindAnyWidget("Stability"));
		widget.SetText(m_SelectedTown.stability.ToString() + "%");
		
		widget = TextWidget.Cast(m_wRoot.FindAnyWidget("Support"));		
		widget.SetText(m_SelectedTown.support.ToString() + " (" + m_SelectedTown.SupportPercentage().ToString() + "%)");
		
		Widget container = m_wRoot.FindAnyWidget("StabilityModContainer");
		Widget child = container.GetChildren();
		while(child)
		{
			container.RemoveChild(child);
			child = container.GetChildren();
		}
		array<int> done = new array<int>;
		OVT_TownModifierSystem system = m_TownManager.GetModifierSystem(OVT_TownStabilityModifierSystem);
		WorkspaceWidget workspace = GetGame().GetWorkspace(); 
		foreach(OVT_TownModifierData data : m_SelectedTown.stabilityModifiers)
		{
			if(done.Contains(data.id)) continue;
			
			OVT_ModifierConfig mod = system.m_Config.m_aModifiers[data.id];
			
			Widget w = workspace.CreateWidgets(m_ModLayout, container);
			TextWidget tw = TextWidget.Cast(w.FindAnyWidget("Text"));
			
			int effect = mod.baseEffect;
			if(mod.flags & OVT_ModifierFlags.STACKABLE)
			{
				effect = 0;
				//count all present
				foreach(OVT_TownModifierData check : m_SelectedTown.stabilityModifiers)
				{
					if(check.id == data.id) effect += mod.baseEffect;
				}
			}
			
			tw.SetText(effect.ToString() + "% " + mod.title);
			
			PanelWidget panel = PanelWidget.Cast(w.FindAnyWidget("Background"));
			if(mod.baseEffect < 0)
			{
				panel.SetColor(m_NegativeModifierColor);
			}else{
				panel.SetColor(m_PositiveModifierColor);
			}
			done.Insert(data.id);
		}
		
		container = m_wRoot.FindAnyWidget("SupportModContainer");
		child = container.GetChildren();
		while(child)
		{
			container.RemoveChild(child);
			child = container.GetChildren();
		}
		done.Clear();
		
		system = m_TownManager.GetModifierSystem(OVT_TownSupportModifierSystem);
		foreach(OVT_TownModifierData data : m_SelectedTown.supportModifiers)
		{
			if(done.Contains(data.id)) continue;
			OVT_ModifierConfig mod = system.m_Config.m_aModifiers[data.id];
			Widget w = workspace.CreateWidgets(m_ModLayout, container);
			TextWidget tw = TextWidget.Cast(w.FindAnyWidget("Text"));
			int effect = mod.baseEffect;
			if(mod.flags & OVT_ModifierFlags.STACKABLE)
			{
				effect = 0;
				//count all present
				foreach(OVT_TownModifierData check : m_SelectedTown.supportModifiers)
				{
					if(check.id == data.id) effect += mod.baseEffect;
				}
			}
			
			tw.SetText(effect.ToString() + "% " + mod.title);
			
			PanelWidget panel = PanelWidget.Cast(w.FindAnyWidget("Background"));
			if(mod.baseEffect < 0)
			{
				panel.SetColor(m_NegativeModifierColor);
			}else{
				panel.SetColor(m_PositiveModifierColor);
			}
			done.Insert(data.id);
		}
	}
	
	void EnableFastTravel()
	{
		if(!ShowMap())
		{
			ShowHint("#OVT-MustHaveMap");
			return;
		}
		m_bFastTravelActive = true;
	}
	
	void EnableBusTravel()
	{
		if(!ShowMap())
		{
			ShowHint("#OVT-MustHaveMap");
			return;
		}
		m_bBusTravelActive = true;
	}
	
	void DisableMapInfo()
	{
		m_bMapInfoActive = false;
	}
	
	void DisableFastTravel()
	{
		m_bFastTravelActive = false;
	}
	
	void DisableBusTravel()
	{
		m_bBusTravelActive = false;
	}
	
	override void RegisterInputs()
	{
		super.RegisterInputs();
		if(!m_InputManager) return;
		
		m_InputManager.AddActionListener("MapSelect", EActionTrigger.DOWN, MapClick);
		m_InputManager.AddActionListener("MenuBack", EActionTrigger.DOWN, MapExit);
		m_InputManager.AddActionListener("GadgetMap", EActionTrigger.DOWN, MapExit);
	}
	
	override void UnregisterInputs()
	{
		super.UnregisterInputs();
		if(!m_InputManager) return;
		
		m_InputManager.RemoveActionListener("MapSelect", EActionTrigger.DOWN, MapClick);
		m_InputManager.RemoveActionListener("MenuBack", EActionTrigger.DOWN, MapExit);
		m_InputManager.RemoveActionListener("GadgetMap", EActionTrigger.DOWN, MapExit);
	}
	
	protected void MapExit(float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		if(!m_bFastTravelActive && !m_bMapInfoActive && !m_bBusTravelActive) return;
		CloseLayout();
		DisableMapInfo();
		DisableFastTravel();
		DisableBusTravel();
		HideMap();
	}
	
	void MapClick(float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		if(!m_bFastTravelActive && !m_bMapInfoActive && !m_bBusTravelActive) return;
		SCR_MapGadgetComponent comp = GetMap();
		if(!comp) return;
		
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if(!mapEntity) return;
		
		float x,y;
		
		
		mapEntity.GetMapCursorWorldPosition(x,y);
		float groundHeight = GetGame().GetWorld().GetSurfaceY(x,y);
		
		vector pos = Vector(x,groundHeight,y);
		
		if(m_bFastTravelActive)
		{	
			string error;
			if(!CanFastTravel(pos, error))
			{
				ShowHint(error);
				HideMap();
				DisableFastTravel();
				return;
			}
			
			int cost = m_Config.m_Difficulty.fastTravelCost;
			
			if(m_Config.m_bDebugMode) cost = 0;
			
			if(!m_Economy.PlayerHasMoney(m_sPlayerID, cost))
			{
				ShowHint("#OVT-CannotAfford");
				HideMap();
				DisableFastTravel();
				return;
			}
					
			pos = OVT_Global.FindSafeSpawnPosition(pos);
			
			HideMap();
			DisableFastTravel();
			
			IEntity player = SCR_PlayerController.GetLocalControlledEntity();
			if(player)
			{
				//If in a vehicle, make sure we are the driver first
				ChimeraCharacter character = ChimeraCharacter.Cast(player);
				if(character && character.IsInVehicle())
				{
					CompartmentAccessComponent compartmentAccess = character.GetCompartmentAccessComponent();
					if (compartmentAccess)
					{
						BaseCompartmentSlot slot = compartmentAccess.GetCompartment();
						if(SCR_CompartmentAccessComponent.GetCompartmentType(slot) == ECompartmentType.Pilot)
						{
							if(cost > 0)
								m_Economy.TakePlayerMoney(m_iPlayerID, cost);
							SCR_Global.TeleportPlayer(pos);
						}else{
							ShowHint("#OVT-MustBeDriver");
						}
					}
				}else{
					if(cost > 0)
						m_Economy.TakePlayerMoney(m_iPlayerID, cost);
					SCR_Global.TeleportPlayer(pos);
				}				
			}			
		}	
		
		if(m_bBusTravelActive)
		{			
			SCR_MapDescriptorComponent stop = m_TownManager.GetNearestBusStop(pos);
			if(!stop)
			{
				ShowHint("#OVT-NeedBusStop");
				DisableBusTravel();
				HideMap();
				return;
			}
			float dist = vector.Distance(pos, m_Owner.GetOrigin());
			int cost = Math.Round((dist / 1000) * m_Config.m_Difficulty.busTicketPrice);
			
			if(!m_Economy.PlayerHasMoney(m_sPlayerID, cost))
			{
				ShowHint("#OVT-CannotAfford");
				HideMap();
				DisableBusTravel();
				return;
			}
			
			HideMap();
			DisableBusTravel();
			
			IEntity player = SCR_PlayerController.GetLocalControlledEntity();
			if(player)
			{								
				ChimeraCharacter character = ChimeraCharacter.Cast(player);
				if(character && character.IsInVehicle())
				{
					ShowHint("#OVT-MustExitVehicle");
					DisableBusTravel();
					HideMap();
					return;
				}
				
				if(cost > 0)
					m_Economy.TakePlayerMoney(m_iPlayerID, cost);
				SCR_Global.TeleportPlayer(pos);
			}
		}
		
		if(m_bMapInfoActive)
		{
			m_SelectedTown = m_TownManager.GetNearestTown(pos);		
			ShowTownInfo();
		}	
	}

}