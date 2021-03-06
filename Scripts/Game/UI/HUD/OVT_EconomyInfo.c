class OVT_EconomyInfo : SCR_InfoDisplay {	
	OVT_EconomyManagerComponent m_Economy;
	OVT_OccupyingFactionManager m_OccupyingFaction;
	string m_playerId;
	
	//------------------------------------------------------------------------------------------------
	override event void OnInit(IEntity owner)
	{
		super.OnInit(owner);
				
		m_Economy = OVT_Global.GetEconomy();
		m_OccupyingFaction = OVT_Global.GetOccupyingFaction();
	}
	
	protected void InitCharacter()
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (!character)
			return;
		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(character);
		m_playerId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);	
	}
		
	private override event void UpdateValues(IEntity owner, float timeSlice)
	{	
		if(!m_playerId){
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
		
		if(m_OccupyingFaction.m_iQRFPoints > 0)
		{
			rf.SetCurrent(m_OccupyingFaction.m_iQRFPoints);
			of.SetCurrent(0);
		}else{
			of.SetCurrent(Math.AbsFloat(m_OccupyingFaction.m_iQRFPoints));
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
}
