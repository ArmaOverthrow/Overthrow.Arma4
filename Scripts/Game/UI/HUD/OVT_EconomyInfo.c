class OVT_EconomyInfo : SCR_InfoDisplay {	
	OVT_EconomyManagerComponent m_Economy;
	OVT_OccupyingFactionManager m_OccupyingFaction;
	OVT_NotificationManagerComponent m_Notify;
	OVT_RealEstateManagerComponent m_RealEstate;
	string m_playerId;
	SCR_ChimeraCharacter m_player;
	
	float m_fCounter = 8;
	float m_fOverrideCounter = 0;
	int m_iCurrentTownId = -1;
	bool m_bTownShowing = false;
	
	protected OVT_MainMenuContextOverrideComponent m_FoundOverride;
	protected float m_fFoundRange=-1;
	
	//------------------------------------------------------------------------------------------------
	override event void OnInit(IEntity owner)
	{
		super.OnInit(owner);

		m_Economy = OVT_Global.GetEconomy();
		m_OccupyingFaction = OVT_Global.GetOccupyingFaction();
		m_Notify = OVT_Global.GetNotify();
		m_RealEstate = OVT_Global.GetRealEstate();

		// Hide town panel initially until UpdateTown() populates it with correct data
		// Check if m_wRoot exists first (may not be initialized yet)
		if (m_wRoot)
		{
			Widget panel = m_wRoot.FindAnyWidget("Town");
			if(panel)
				panel.SetVisible(false);
		}
	}
	
	protected void InitCharacter()
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (!character)
			return;
		int playerId = SCR_PossessingManagerComponent.GetPlayerIdFromControlledEntity(character);
		m_playerId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);	
		
		m_player = character;
	}
		
	private override event void UpdateValues(IEntity owner, float timeSlice)
	{	
		m_fCounter += timeSlice;
		m_fOverrideCounter += timeSlice;
		if(!m_player){
			InitCharacter();
		}
		UpdateMoney();
		if(m_OccupyingFaction.m_bQRFActive)
		{
			ShowQRF();
			UpdateQRF();
		}else{
			HideQRF();
		}
		
		if(m_fOverrideCounter >= 1)
		{
			m_fOverrideCounter = 0;
			if(!UpdateVehicleHint())
			{
				UpdateOverride();
			}
		}
		
		UpdateNotification(timeSlice);
		
		
		if(m_fCounter > 10)
		{
			m_fCounter = 0;
			UpdateTown();
		}
		
#ifdef WORKBENCH
		//UpdateDebug();
#endif				
	}
	
	void UpdateOverride()
	{
		m_FoundOverride = null;
		m_fFoundRange = -1;
		GetGame().GetWorld().QueryEntitiesBySphere(m_player.GetOrigin(), 50, null, FindOverride, EQueryEntitiesFlags.ALL);		
		
		if(m_FoundOverride && m_FoundOverride.CanShow(m_player))
		{
			m_wRoot.FindAnyWidget("MainMenuOverride").SetVisible(true);
			RichTextWidget text = RichTextWidget.Cast(m_wRoot.FindAnyWidget("MainMenuOverrideText"));
			SCR_UIInfo info = m_FoundOverride.m_UiInfo;
			if(info)
				text.SetTextFormat("<color rgba='226,168,79,255'><action name='OverthrowMainMenu'/></color> %1",info.GetName());
		}else{
			if(OVT_Global.GetOverthrow().m_bHasOpenedMenu)
			{
				m_wRoot.FindAnyWidget("MainMenuOverride").SetVisible(false);
				return;
			}			
			RichTextWidget text = RichTextWidget.Cast(m_wRoot.FindAnyWidget("MainMenuOverrideText"));
			text.SetText("<color rgba='226,168,79,255'><action name='OverthrowMainMenu'/></color> #OVT_Open_Overthrow_Menu");
			m_wRoot.FindAnyWidget("MainMenuOverride").SetVisible(true);
		}
	}
	
	// FindOverride gets called a lot, so make it a tiny bit faster by having the pointer here
	protected Managed m_foundComponent;
	protected bool FindOverride(IEntity entity)
	{
		m_foundComponent = entity.FindComponent(OVT_MainMenuContextOverrideComponent);
		if(!m_foundComponent) return false;
		
		OVT_MainMenuContextOverrideComponent m_overrideComponent = OVT_MainMenuContextOverrideComponent.Cast(m_foundComponent);
		if(!m_overrideComponent) return false;
		
		float dist = vector.Distance(entity.GetOrigin(),m_player.GetOrigin());
		if(dist > m_overrideComponent.m_fRange) return false;
		
		if(m_fFoundRange == -1 || m_fFoundRange > dist)
		{
			bool got = true;
			if(m_overrideComponent.m_bMustOwnBase)
			{
				OVT_BaseData base = OVT_Global.GetOccupyingFaction().GetNearestBase(entity.GetOrigin());
				if(!base || base.IsOccupyingFaction())
				{
					got = false;
				}
			}
			if(got)
			{
				m_FoundOverride = m_overrideComponent;
				m_fFoundRange = dist;
			}
		}
		
		return false;
	}
	
	bool UpdateVehicleHint()
	{
		if(!m_player) return false;
		
		SCR_CompartmentAccessComponent compartment = SCR_CompartmentAccessComponent.Cast(m_player.FindComponent(SCR_CompartmentAccessComponent));
		if(!compartment || !compartment.IsInCompartment() || compartment.GetCompartment().GetType() != ECompartmentType.PILOT)
		{
			return false;
		}
		
		vector pos = m_player.GetOrigin();
		bool hasButtons = false;
		
		OVT_WarehouseData warehouse = m_RealEstate.GetNearestWarehouse(pos, 40);
		if(warehouse)
		{
			IEntity warehouseEntity = m_RealEstate.GetNearestBuilding(warehouse.location, 10);
			if(warehouseEntity)
			{
				EntityID id = warehouseEntity.GetID();
				bool isOwned = m_RealEstate.IsOwned(id);
				bool isOwner = m_RealEstate.IsOwner(m_playerId, id);
				bool isRented = m_RealEstate.IsRented(id);
				bool isAccessible = (!warehouse.isPrivate && isOwned && !isRented) || (warehouse.isPrivate && isOwner && !isRented) || isRented;
				if(isAccessible) hasButtons = true;
			}
		}
		
		if(!hasButtons)
		{
			RplId port = m_Economy.GetNearestPort(pos);
			if(port)
			{
				RplComponent rpl = RplComponent.Cast(Replication.FindItem(port));
				if(rpl)
				{
					float dist = vector.Distance(pos, rpl.GetEntity().GetOrigin());
					if(dist < 20) hasButtons = true;
				}
			}
		}
		
		if(hasButtons)
		{
			m_wRoot.FindAnyWidget("MainMenuOverride").SetVisible(true);
			RichTextWidget text = RichTextWidget.Cast(m_wRoot.FindAnyWidget("MainMenuOverrideText"));
			text.SetText("<color rgba='226,168,79,255'><action name='OverthrowVehicleMenu'/></color> #OVT_Vehicle_Menu");
			return true;
		}
		
		return false;
	}
	
	void UpdateDebug()
	{
		Widget d = m_wRoot.FindAnyWidget("Debug");
		d.SetVisible(true);
		
		TextWidget text = TextWidget.Cast(m_wRoot.FindAnyWidget("DebugText"));
		
		CharacterPerceivableComponent percieve = EPF_Component<CharacterPerceivableComponent>.Find(m_player);
		if(percieve)
		{
			text.SetText(percieve.GetVisualRecognitionFactor().ToString());
		}
	}
	
	void UpdateTown()
	{		
		if(m_bTownShowing) {
			Widget panel = m_wRoot.FindAnyWidget("Town");
			panel.SetVisible(false);
			m_bTownShowing = false;
			return;
		}	
		if(!m_player) return;
		OVT_TownManagerComponent tm = OVT_Global.GetTowns();	
		OVT_TownData town = tm.GetNearestTown(m_player.GetOrigin());
		int townID = OVT_Global.GetTowns().GetTownID(town);
		if(m_iCurrentTownId != townID)
		{
			m_iCurrentTownId = townID;
			ImageWidget img = ImageWidget.Cast(m_wRoot.FindAnyWidget("ControllingFaction"));
			img.LoadImageTexture(0, town.ControllingFactionData().GetUIInfo().GetIconPath());
					
			TextWidget widget = TextWidget.Cast(m_wRoot.FindAnyWidget("TownName"));
			widget.SetText(tm.GetTownName(townID));
			
			widget = TextWidget.Cast(m_wRoot.FindAnyWidget("Population"));
			widget.SetText(town.population.ToString());
			
			widget = TextWidget.Cast(m_wRoot.FindAnyWidget("Stability"));
			widget.SetText(town.stability.ToString() + "%");
			
			widget = TextWidget.Cast(m_wRoot.FindAnyWidget("Support"));		
			widget.SetText(town.support.ToString() + " (" + town.SupportPercentage().ToString() + "%)");
			
			Widget panel = m_wRoot.FindAnyWidget("Town");
			panel.SetVisible(true);
			
			m_bTownShowing = true;
		}
	}
	
	void HideQRF()
	{
		m_wRoot.FindAnyWidget("QRF").SetVisible(false);
	}
	
	void ShowQRF()
	{
		m_wRoot.FindAnyWidget("QRF").SetVisible(true);
	}
	
	void UpdateQRF()
	{
		TextWidget w = TextWidget.Cast(m_wRoot.FindAnyWidget("QRFTimerText"));
		if(m_OccupyingFaction.m_iQRFTimer > 0)
		{
			w.SetText("#OVT-BattleStartsIn " + Math.Floor(m_OccupyingFaction.m_iQRFTimer / 1000).ToString());
		}else{
			w.SetText("#OVT-BattleProgress");
		}
		SliderWidget of = SliderWidget.Cast(m_wRoot.FindAnyWidget("QRFOccupying"));
		SliderWidget rf = SliderWidget.Cast(m_wRoot.FindAnyWidget("QRFResistance"));
		
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		int points = Math.Round(((float)m_OccupyingFaction.m_iQRFPoints / (float)config.m_Difficulty.QRFPointsToWin) * 100);
		
		if(points >= 0)
		{
			rf.SetCurrent(points);
			of.SetCurrent(0);
		}else{
			of.SetCurrent(Math.AbsFloat(points));
			rf.SetCurrent(0);
		}
	}
			
	//------------------------------------------------------------------------------------------------
	// Update Money
	//------------------------------------------------------------------------------------------------
	void UpdateMoney()
	{
		if (!m_Economy)
			return;
		if(!m_wRoot)
			return;
						
		TextWidget w = TextWidget.Cast(m_wRoot.FindAnyWidget("MoneyText"));
		w.SetText("$" + m_Economy.GetPlayerMoney(m_playerId));
	}
	
	void UpdateNotification(float timeSlice)
	{
		if (!m_Notify)
			return;
		if(!m_wRoot)
			return;
		
		Widget notify = m_wRoot.FindAnyWidget("Notify");
		TextWidget w = TextWidget.Cast(m_wRoot.FindAnyWidget("NotificationText"));
		
		if(m_Notify.m_aNotifications.Count() == 0)
		{
			notify.SetVisible(false);
			return;
		}
		
		OVT_NotificationData data = m_Notify.m_aNotifications.Get(0);
		if(!data)
		{
			notify.SetVisible(false);
			return;
		}
		
		if(data.displayTimer <= 0)
		{
			notify.SetVisible(false);
			return;
		}
		
		data.displayTimer = data.displayTimer - timeSlice;
		
		w.SetTextFormat(data.msg.m_UIInfo.GetDescription(), data.param1, data.param2, data.param3);
		notify.SetVisible(true);
		
		if(data.msg.m_UIInfo.GetIconSetName() != "")
		{
			ImageWidget icon = ImageWidget.Cast(m_wRoot.FindAnyWidget("NotificationIcon"));
			data.msg.m_UIInfo.SetIconTo(icon);
		}			
	}
}
