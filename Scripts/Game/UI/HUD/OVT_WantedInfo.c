/*

*/

class OVT_WantedInfo : SCR_InfoDisplayExtended {
	ref OVT_WantedInfoWidgets widgets;
	
	OVT_PlayerWantedComponent m_Wanted = null;
	
	protected const string LEVEL_INDICATOR = "*";
	protected const string SEEN_INDICATOR = "o_o";
	
	//------------------------------------------------------------------------------------------------
	override bool DisplayStartDrawInit(IEntity owner)
	{
		CreateLayout();
		
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(owner);
		if (!character)
			return false;
		
		
		
		m_Wanted = OVT_PlayerWantedComponent.Cast(character.FindComponent(OVT_PlayerWantedComponent));
		if (!m_Wanted)
			return false;
	
		return true;
	}
		
	override void DisplayUpdate(IEntity owner, float timeSlice)
	{	
		UpdateWantedLevel();
		UpdateSeen();
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
			m_wRoot = manager.CreateLayout(OVT_WantedInfoWidgets.s_sLayout, m_eLayer);

		if (!m_wRoot)
			return;
		
		widgets = new OVT_WantedInfoWidgets();
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
	// Update Wanted Level Indicator
	//------------------------------------------------------------------------------------------------
	void UpdateWantedLevel()
	{
		if (!m_Wanted)
			return;
		
		string text = "";
		for(int i=0; i<m_Wanted.GetWantedLevel(); i++){
			text += LEVEL_INDICATOR;
		}
		
		widgets.m_WantedText.SetText(text);
	}
	
	//------------------------------------------------------------------------------------------------
	// Update Seen Indicator
	//------------------------------------------------------------------------------------------------
	void UpdateSeen()
	{
		if (!m_Wanted)
			return;
		
		string text = "";
		
		if(m_Wanted.IsSeen())
		{
			text += SEEN_INDICATOR;
		}
		
		text += " " + m_Wanted.m_iLastSeen + " " + m_Wanted.m_iWantedTimer;
		
		widgets.m_SeenText.SetText(text);
	}
}