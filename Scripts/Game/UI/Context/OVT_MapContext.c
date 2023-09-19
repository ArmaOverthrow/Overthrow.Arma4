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
	
	protected bool m_bFastTravelActive = false;
	protected bool m_bBusTravelActive = false;
	
	protected const int MAX_HOUSE_TRAVEL_DIS = 40;
	protected const int MAX_FOB_TRAVEL_DIS = 100;
	protected const int MIN_TRAVEL_DIS = 500;
	
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
		if(!ent) return null;
				
		SCR_MapGadgetComponent comp = SCR_MapGadgetComponent.Cast(ent.FindComponent(SCR_MapGadgetComponent));
		if(!comp) return null;
		
		return comp;
	}
	
	bool CanFastTravel(vector pos, out string reason)
	{	
		if(OVT_Global.GetConfig().m_bDebugMode) return true;
		
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
		
		if(m_OccupyingFaction.m_bQRFActive && OVT_Global.GetConfig().m_Difficulty.QRFFastTravelMode != OVT_QRFFastTravelMode.FREE)
		{
			if(OVT_Global.GetConfig().m_Difficulty.QRFFastTravelMode == OVT_QRFFastTravelMode.DISABLED)
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
		
		if(m_Resistance.m_bFOBDeployed)
		{
			dist = vector.Distance(m_Resistance.m_vFOBLocation, pos);
			if(dist < MAX_FOB_TRAVEL_DIS) return true;
		}
		
		vector fob = m_Resistance.GetNearestCamp(pos);		
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
	
	void OnMapExit(MapConfiguration config)
	{
		DisableFastTravel();
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
		if(!m_bFastTravelActive && !m_bBusTravelActive) return;
		CloseLayout();
		DisableFastTravel();
		DisableBusTravel();
		HideMap();
	}
	
	void MapClick(float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		if(!m_bFastTravelActive && !m_bBusTravelActive) return;
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
			
			int cost = OVT_Global.GetConfig().m_Difficulty.fastTravelCost;
			
			if(OVT_Global.GetConfig().m_bDebugMode) cost = 0;
			
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
							SCR_Global.TeleportPlayer(m_iPlayerID, pos);
						}else{
							ShowHint("#OVT-MustBeDriver");
						}
					}
				}else{
					if(cost > 0)
						m_Economy.TakePlayerMoney(m_iPlayerID, cost);
					SCR_Global.TeleportPlayer(m_iPlayerID, pos);
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
			int cost = Math.Round((dist / 1000) * OVT_Global.GetConfig().m_Difficulty.busTicketPrice);
			
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
				SCR_Global.TeleportPlayer(m_iPlayerID, pos);
			}
		}
	}

}