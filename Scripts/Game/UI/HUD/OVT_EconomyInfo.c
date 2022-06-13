/*

*/

class OVT_EconomyInfo : SCR_InfoDisplayExtended {
	ref OVT_EconomyInfoWidgets widgets;
	
	OVT_EconomyManagerComponent m_Economy = null;
	string m_playerId;
	
	//------------------------------------------------------------------------------------------------
	override bool DisplayStartDrawInit(IEntity owner)
	{
		CreateLayout();
		
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(owner);
		if (!character)
			return false;
		
		m_Economy = OVT_EconomyManagerComponent.GetInstance();
		
		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(character);
		m_playerId = OVT_PlayerManagerComponent.GetInstance().GetPersistentIDFromPlayerID(playerId);
	
		return true;
	}
		
	override void DisplayUpdate(IEntity owner, float timeSlice)
	{	
		UpdateMoney();
	}
	
	//------------------------------------------------------------------------------------------------
	// Create the layout
	//------------------------------------------------------------------------------------------------
	void CreateLayout()
	{		
		// Destroy existing layout
		DestroyLayout();
		
		// Create weapon info layout
		SCR_HUDManagerComponent manager = SCR_HUDManagerComponent.GetHUDManager();
		if (manager)
			m_wRoot = manager.CreateLayout(OVT_EconomyInfoWidgets.s_sLayout, m_eLayer);

		if (!m_wRoot)
			return;
		
		widgets = new OVT_EconomyInfoWidgets();
		widgets.Init(m_wRoot);
	}
	
	//------------------------------------------------------------------------------------------------
	// Destroy the layout
	//------------------------------------------------------------------------------------------------
	void DestroyLayout()
	{
		if (m_wRoot)
			m_wRoot.RemoveFromHierarchy();
			
		m_wRoot = null;
	}	
			
	//------------------------------------------------------------------------------------------------
	// Update Money
	//------------------------------------------------------------------------------------------------
	void UpdateMoney()
	{
		if (!m_Economy)
			return;
						
		widgets.m_MoneyText.SetText("$" + m_Economy.GetPlayerMoney(m_playerId));
	}
}
