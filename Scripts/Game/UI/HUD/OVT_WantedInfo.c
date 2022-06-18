/*

*/

class OVT_WantedInfo : SCR_InfoDisplay {	
	OVT_PlayerWantedComponent m_Wanted = null;
	
	protected void InitCharacter()
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (!character)
			return;
				
		m_Wanted = OVT_PlayerWantedComponent.Cast(character.FindComponent(OVT_PlayerWantedComponent));
	}
		
	private override event void UpdateValues(IEntity owner, float timeSlice)
	{	
		if(!m_Wanted)
		{
			InitCharacter();
		}
		UpdateWantedLevel();
		UpdateSeen();
	}

			
	//------------------------------------------------------------------------------------------------
	// Update Wanted Level Indicator
	//------------------------------------------------------------------------------------------------
	void UpdateWantedLevel()
	{
		if (!m_Wanted)
			return;
		
		if(!m_wRoot)
			return;
		
		int i;		
		for(i=0; i<m_Wanted.GetWantedLevel(); i++){
			Widget w = m_wRoot.FindWidget("Frame0.WantedLevel.Star" + i);
			if(w) w.SetVisible(true);
		}
		
		for(; i<5; i++){
			Widget w = m_wRoot.FindWidget("Frame0.WantedLevel.Star" + i);
			if(w) w.SetVisible(false);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	// Update Seen Indicator
	//------------------------------------------------------------------------------------------------
	void UpdateSeen()
	{
		if (!m_Wanted)
			return;
		
		if(!m_wRoot)
			return;
		
		Widget w = m_wRoot.FindWidget("Frame0.Seen.SeenEye");
		
		if(m_Wanted.IsSeen())
		{
			w.SetVisible(true);
		}else{
			w.SetVisible(false);
		}
	}
}