class OVT_EconomyInfo : SCR_InfoDisplay {	
	OVT_EconomyManagerComponent m_Economy;
	OVT_OccupyingFactionManager m_OccupyingFaction;
	OVT_NotificationManagerComponent m_Notify;
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
	}
	
	protected void InitCharacter()
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (!character)
			return;
		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(character);
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
		
		if(m_fOverrideCounter >= 2)
		{
			UpdateOverride();
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
			text.SetTextFormat("<color rgba='226,168,79,255'><action name='OverthrowMainMenu'/></color> %1",m_FoundOverride.m_UiInfo.GetName());
		}else{
			if(!OVT_Global.GetOverthrow().m_bHasOpenedMenu)
			{
				RichTextWidget text = RichTextWidget.Cast(m_wRoot.FindAnyWidget("MainMenuOverrideText"));
				text.SetText("<color rgba='226,168,79,255'><action name='OverthrowMainMenu'/></color> #OVT_Open_Overthrow_Menu");
				m_wRoot.FindAnyWidget("MainMenuOverride").SetVisible(true);
			}else{
				m_wRoot.FindAnyWidget("MainMenuOverride").SetVisible(false);
			}
		}
	}
	
	protected bool FindOverride(IEntity entity)
	{
		OVT_MainMenuContextOverrideComponent found = OVT_MainMenuContextOverrideComponent.Cast(entity.FindComponent(OVT_MainMenuContextOverrideComponent));
		if(found) {
			float dist = vector.Distance(entity.GetOrigin(),m_player.GetOrigin());
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
		int points = (m_OccupyingFaction.m_iQRFPoints / config.m_Difficulty.QRFPointsToWin) * 100;
		
		if(points > 0)
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
