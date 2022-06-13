class OVT_MapContext : OVT_UIContext
{
	OVT_TownManagerComponent m_TownManager;
	OVT_RealEstateManagerComponent m_RealEstate;
	
	protected bool m_bMapInfoActive = false;
	protected bool m_bFastTravelActive = false;
	
	override void PostInit()
	{		
		m_TownManager = OVT_Global.GetTowns();
		m_RealEstate = OVT_Global.GetRealEstate();
		
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
	
	void ShowMap()
	{
		SCR_MapGadgetComponent comp = GetMap();
		if(!comp) return;
		
		comp.SetMapMode(true);
	}
	
	void HideMap()
	{
		SCR_MapGadgetComponent comp = GetMap();
		if(!comp) return;
		
		comp.SetMapMode(false);
	}
	
	void EnableMapInfo()
	{
		m_bMapInfoActive = true;
		ShowMap();
	}
	
	void EnableFastTravel()
	{
		m_bFastTravelActive = true;
		ShowMap();
	}
	
	void DisableMapInfo()
	{
		m_bMapInfoActive = false;
	}
	
	void DisableFastTravel()
	{
		m_bFastTravelActive = false;
	}
	
	override void RegisterInputs()
	{
		super.RegisterInputs();
		
		m_InputManager.AddActionListener("MapSelect", EActionTrigger.DOWN, MapClick);
	}
	
	override void UnregisterInputs()
	{
		super.UnregisterInputs();
		
		m_InputManager.RemoveActionListener("MapSelect", EActionTrigger.DOWN, MapClick);
	}
	
	void MapClick(float value = 1, EActionTrigger reason = EActionTrigger.DOWN)
	{
		if(!m_bFastTravelActive && !m_bMapInfoActive) return;
		SCR_MapGadgetComponent comp = GetMap();
		if(!comp) return;
		
		SCR_MapEntity mapEntity = SCR_MapEntity.GetMapInstance();
		if(!mapEntity) return;
		
		float x,y;
		
		mapEntity.GetMapCursorWorldPosition(x,y);
		
		if(m_bFastTravelActive)
		{
			vector spawnPosition = "0 0 0";
			spawnPosition[0] = x;
			spawnPosition[2] = y;
			
			//Snap to the nearest navmesh point
			AIPathfindingComponent pathFindindingComponent = AIPathfindingComponent.Cast(m_Owner.FindComponent(AIPathfindingComponent));
			if (pathFindindingComponent && pathFindindingComponent.GetClosestPositionOnNavmesh(spawnPosition, "10 10 10", spawnPosition))
			{
				float groundHeight = GetGame().GetWorld().GetSurfaceY(spawnPosition[0], spawnPosition[2]);
				if (spawnPosition[1] < groundHeight)
					spawnPosition[1] = groundHeight;
			}
			
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
							SCR_Global.TeleportPlayer(spawnPosition);
						}else{
							ShowHint("#OVT-MustBeDriver");
						}
					}
				}else{
					SCR_Global.TeleportPlayer(spawnPosition);
				}				
			}			
		}	
		
		if(m_bMapInfoActive)
		{
			HideMap();
			DisableMapInfo();
		}	
	}

}