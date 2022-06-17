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
	
	OVT_TownData m_SelectedTown;
	
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
		
		ShowLayout();
		m_SelectedTown = m_TownManager.GetNearestTown(m_Owner.GetOrigin());
		
		ShowTownInfo();
	}
	
	protected void ShowTownInfo()
	{
		if(!m_wRoot) return;
		if(!m_SelectedTown) return;
		
		SCR_MapDescriptorComponent marker = m_TownManager.GetNearestTownMarker(m_SelectedTown.location);
		
		TextWidget widget = TextWidget.Cast(m_wRoot.FindAnyWidget("TownName"));
		widget.SetText(marker.Item().GetDisplayName());
		
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
		widget.SetText(m_SelectedTown.SupportPercentage().ToString() + "%");
		
		Widget container = m_wRoot.FindAnyWidget("StabilityModContainer");
		Widget child = container.GetChildren();
		while(child)
		{
			container.RemoveChild(child);
			child = container.GetChildren();
		}
		array<int> done = new array<int>;
		foreach(int index : m_SelectedTown.stabilityModifiers)
		{
			if(done.Contains(index)) continue;
			OVT_StabilityModifierConfig mod = m_TownManager.m_StabilityModifiers.m_aStabilityModifiers[index];
			WorkspaceWidget workspace = GetGame().GetWorkspace(); 
			Widget w = workspace.CreateWidgets(m_ModLayout, container);
			TextWidget tw = TextWidget.Cast(w.FindAnyWidget("Text"));
			
			int effect = mod.baseEffect;
			if(mod.flags & OVT_StabilityModifierFlags.STACKABLE)
			{
				effect = 0;
				//count all present
				foreach(int check : m_SelectedTown.stabilityModifiers)
				{
					if(check == index) effect += mod.baseEffect;
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
			done.Insert(index);
		}
		
		container = m_wRoot.FindAnyWidget("SupportModContainer");
		child = container.GetChildren();
		while(child)
		{
			container.RemoveChild(child);
			child = container.GetChildren();
		}
		done.Clear();
		foreach(int index : m_SelectedTown.supportModifiers)
		{
			if(done.Contains(index)) continue;
			OVT_SupportModifierConfig mod = m_TownManager.m_SupportModifiers.m_aSupportModifiers[index];
			WorkspaceWidget workspace = GetGame().GetWorkspace(); 
			Widget w = workspace.CreateWidgets(m_ModLayout, container);
			TextWidget tw = TextWidget.Cast(w.FindAnyWidget("Text"));
			int effect = mod.baseEffect;
			if(mod.flags & OVT_SupportModifierFlags.STACKABLE)
			{
				effect = 0;
				//count all present
				foreach(int check : m_SelectedTown.supportModifiers)
				{
					if(check == index) effect += mod.baseEffect;
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
			done.Insert(index);
		}
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
		if(!m_bFastTravelActive && !m_bMapInfoActive) return;
		CloseLayout();
		DisableMapInfo();
		DisableFastTravel();
		HideMap();
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
		
		vector pos = Vector(x,0,y);
		
		if(m_bFastTravelActive)
		{			
			//Snap to the nearest navmesh point
			AIPathfindingComponent pathFindindingComponent = AIPathfindingComponent.Cast(m_Owner.FindComponent(AIPathfindingComponent));
			if (pathFindindingComponent && pathFindindingComponent.GetClosestPositionOnNavmesh(pos, "10 10 10", pos))
			{
				float groundHeight = GetGame().GetWorld().GetSurfaceY(pos[0], pos[2]);
				if (pos[1] < groundHeight)
					pos[1] = groundHeight;
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
							SCR_Global.TeleportPlayer(pos);
						}else{
							ShowHint("#OVT-MustBeDriver");
						}
					}
				}else{
					SCR_Global.TeleportPlayer(pos);
				}				
			}			
		}	
		
		if(m_bMapInfoActive)
		{
			m_SelectedTown = m_TownManager.GetNearestTown(pos);		
			ShowTownInfo();
		}	
	}

}