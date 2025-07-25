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
	protected const float RECRUIT_TRAVEL_RADIUS = 50.0; // Radius to search for recruits to fast travel with
	
	override void PostInit()
	{		
		m_TownManager = OVT_Global.GetTowns();
		m_RealEstate = OVT_Global.GetRealEstate();
		m_Resistance = OVT_Global.GetResistanceFaction();
		m_OccupyingFaction = OVT_Global.GetOccupyingFaction();
		
		SCR_MapEntity.GetOnMapClose().Insert(OnMapExit);		
		
	}
	
	SCR_MapGadgetComponent GetMap()
	{
		SCR_GadgetManagerComponent mgr = SCR_GadgetManagerComponent.Cast(m_Owner.FindComponent(SCR_GadgetManagerComponent));
		if(!mgr) return null;
		
		IEntity ent = mgr.GetQuickslotGadgetByType(EGadgetType.MAP);
		if(!ent) {		
			ent = mgr.GetGadgetByType(EGadgetType.MAP);
		}
		
		if(!ent) return null;
				
		SCR_MapGadgetComponent comp = SCR_MapGadgetComponent.Cast(ent.FindComponent(SCR_MapGadgetComponent));
		if(!comp) return null;
		
		return comp;
	}
	
	bool CanFastTravel(vector pos, out string reason)
	{	
		if(OVT_Global.GetConfig().m_bDebugMode) return true;
		
		reason = "CannotFastTravelThere";	
		float dist;
		
		dist = vector.Distance(pos, m_Owner.GetOrigin());
		if(dist < OVT_Global.GetConfig().m_Difficulty.minFastTravelDistance)
		{
			reason = "CannotFastTravelDistance";
			return false;	
		}
		
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (!character)
			return false;
		
		OVT_PlayerWantedComponent m_Wanted = OVT_PlayerWantedComponent.Cast(character.FindComponent(OVT_PlayerWantedComponent));
		if(m_Wanted.GetWantedLevel() > 0)
		{
			reason = "CannotFastTravelWanted";
			return false;
		}		
		
		if(m_OccupyingFaction.m_bQRFActive && OVT_Global.GetConfig().m_Difficulty.QRFFastTravelMode != OVT_QRFFastTravelMode.FREE)
		{
			if(OVT_Global.GetConfig().m_Difficulty.QRFFastTravelMode == OVT_QRFFastTravelMode.DISABLED)
			{
				reason = "CannotFastTravelDuringQRF";
				return false;
			}
			dist = vector.Distance(m_OccupyingFaction.m_vQRFLocation, pos);		
			if(dist < OVT_QRFControllerComponent.QRF_RANGE)
			{
				reason = "CannotFastTravelToQRF";
				return false;
			}
		}
		
		IEntity house = m_RealEstate.GetNearestOwned(m_sPlayerID, pos, MAX_HOUSE_TRAVEL_DIS);
		if(house)
		{
			if(m_RealEstate.IsRented(house.GetID())) return false;
			OVT_RealEstateConfig config = m_RealEstate.GetConfig(house);
			if(config.m_IsWarehouse) return false;
			return true;
		}
		
		house = m_RealEstate.GetNearestRented(m_sPlayerID, pos, MAX_HOUSE_TRAVEL_DIS);
		if(house)
		{
			OVT_RealEstateConfig config = m_RealEstate.GetConfig(house);
			if(config.m_IsWarehouse) return false;
			return true;
		}
		
		OVT_CampData camp = m_Resistance.GetNearestCampData(pos);
		if(camp) {
			dist = vector.Distance(camp.location, pos);
			if(dist < MAX_FOB_TRAVEL_DIS) {
				// Allow fast travel if it's the player's own camp or if it's public
				if(camp.owner == m_sPlayerID || !camp.isPrivate) {
					return true;
				}
			}
		}
		
		vector fob = m_Resistance.GetNearestFOB(pos);		
		dist = vector.Distance(fob, pos);
		if(dist < MAX_FOB_TRAVEL_DIS) return true;		
		
		OVT_BaseData base = m_OccupyingFaction.GetNearestBase(pos);
		if(base && !base.IsOccupyingFaction())
		{
			dist = vector.Distance(base.location, pos);
			if(dist < OVT_Global.GetConfig().m_Difficulty.baseCloseRange) return true;
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
			ShowNotification("MustHaveMap");
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
		
		int townID = m_TownManager.GetTownID(m_SelectedTown);
		
		ImageWidget img = ImageWidget.Cast(m_wRoot.FindAnyWidget("ControllingFaction"));
		img.LoadImageTexture(0, m_SelectedTown.ControllingFactionData().GetUIInfo().GetIconPath());
				
		TextWidget widget = TextWidget.Cast(m_wRoot.FindAnyWidget("TownName"));
		widget.SetText(m_TownManager.GetTownName(townID));
		
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
		autoptr array<int> done = new array<int>;
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
			ShowNotification("MustHaveMap");
			return;
		}
		m_bFastTravelActive = true;
	}
	
	void EnableBusTravel()
	{
		if(!ShowMap())
		{
			ShowNotification("MustHaveMap");
			return;
		}
		m_bBusTravelActive = true;
	}
	
	void OnMapExit(MapConfiguration config)
	{
		DisableMapInfo();
		DisableFastTravel();
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
				ShowNotification(error);
				HideMap();
				DisableFastTravel();
				return;
			}
			
			int cost = OVT_Global.GetConfig().m_Difficulty.fastTravelCost;
			
			// Calculate additional cost for nearby recruits
			int recruitCount = 0;
			if (!OVT_Global.GetConfig().m_bDebugMode)
			{
				OVT_RecruitManagerComponent recruitManager = OVT_RecruitManagerComponent.GetInstance();
				if (recruitManager)
				{
					array<ref OVT_RecruitData> nearbyRecruits = recruitManager.GetPlayerRecruitsInRadius(m_sPlayerID, m_Owner.GetOrigin(), RECRUIT_TRAVEL_RADIUS);
					recruitCount = nearbyRecruits.Count();
					cost += recruitCount * OVT_Global.GetConfig().m_Difficulty.fastTravelCost; // Same cost per recruit
				}
			}
			
			if(OVT_Global.GetConfig().m_bDebugMode) cost = 0;
			
			if(!m_Economy.PlayerHasMoney(m_sPlayerID, cost))
			{
				ShowNotification("CannotAfford");
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
						if(slot.GetType() == ECompartmentType.PILOT)
						{
							if(cost > 0)
								m_Economy.TakePlayerMoney(m_iPlayerID, cost);
							OVT_Global.GetServer().RequestFastTravel(m_iPlayerID, pos);
						}else{
							ShowNotification("MustBeDriver");
						}
					}
				}else{
					if(cost > 0)
						m_Economy.TakePlayerMoney(m_iPlayerID, cost);
					
					// Use recruit-aware fast travel for non-vehicle travel
					if (recruitCount > 0)
					{
						OVT_Global.GetServer().RequestFastTravelWithRecruits(m_iPlayerID, pos, RECRUIT_TRAVEL_RADIUS);
					}
					else
					{
						SCR_Global.TeleportPlayer(m_iPlayerID, pos);
					}					
				}				
			}			
		}	
		
		if(m_bBusTravelActive)
		{			
			SCR_MapDescriptorComponent stop = m_TownManager.GetNearestBusStop(pos);
			if(!stop)
			{
				ShowNotification("NeedBusStop");
				DisableBusTravel();
				HideMap();
				return;
			}
			float dist = vector.Distance(pos, m_Owner.GetOrigin());
			int cost = Math.Round((dist / 1000) * OVT_Global.GetConfig().m_Difficulty.busTicketPrice);
			
			// Calculate additional cost for nearby recruits
			int recruitCount = 0;
			OVT_RecruitManagerComponent recruitManager = OVT_RecruitManagerComponent.GetInstance();
			if (recruitManager)
			{
				array<ref OVT_RecruitData> nearbyRecruits = recruitManager.GetPlayerRecruitsInRadius(m_sPlayerID, m_Owner.GetOrigin(), RECRUIT_TRAVEL_RADIUS);
				recruitCount = nearbyRecruits.Count();
				cost += recruitCount * Math.Round((dist / 1000) * OVT_Global.GetConfig().m_Difficulty.busTicketPrice); // Same cost per recruit
			}
			
			if(!m_Economy.PlayerHasMoney(m_sPlayerID, cost))
			{
				ShowNotification("CannotAfford");
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
					ShowNotification("MustExitVehicle");
					DisableBusTravel();
					HideMap();
					return;
				}
				
				if(cost > 0)
					m_Economy.TakePlayerMoney(m_iPlayerID, cost);
				
				// Use recruit-aware fast travel for bus travel as well
				if (recruitCount > 0)
				{
					OVT_Global.GetServer().RequestFastTravelWithRecruits(m_iPlayerID, pos, RECRUIT_TRAVEL_RADIUS);
				}
				else
				{
					SCR_Global.TeleportPlayer(m_iPlayerID, pos);
				}
			}
		}
		
		if(m_bMapInfoActive)
		{
			m_SelectedTown = m_TownManager.GetNearestTown(pos);
			Print(string.Format("[Overthrow] Threat at clicked location: %1",OVT_Global.GetOccupyingFaction().GetThreatByLocation(pos)));
				
			ShowTownInfo();
		}	
	}

}