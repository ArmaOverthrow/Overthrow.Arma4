class OVT_EconomyInfo : SCR_InfoDisplay {	
	OVT_EconomyManagerComponent m_Economy = null;
	string m_playerId;
	
	//------------------------------------------------------------------------------------------------
	override event void OnInit(IEntity owner)
	{
		super.OnInit(owner);
				
		m_Economy = OVT_Global.GetEconomy();
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
						
		TextWidget w = TextWidget.Cast(m_wRoot.FindWidget("Frame0.EconomyInfoPanel.Money.MoneyText"));
		w.SetText("$" + m_Economy.GetPlayerMoney(m_playerId));
	}
}
