/*

*/

class OVT_WantedInfo : SCR_InfoDisplay {	
	OVT_PlayerWantedComponent m_Wanted = null;
	CharacterPerceivableComponent m_Percieve = null;
	SCR_CharacterFactionAffiliationComponent m_FactionAffiliation = null;
	
	protected void InitCharacter()
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (!character)
			return;
				
		m_Wanted = OVT_PlayerWantedComponent.Cast(character.FindComponent(OVT_PlayerWantedComponent));
		m_Percieve = CharacterPerceivableComponent.Cast(character.FindComponent(CharacterPerceivableComponent));
		m_FactionAffiliation = SCR_CharacterFactionAffiliationComponent.Cast(character.FindComponent(SCR_CharacterFactionAffiliationComponent));
	}
		
	private override event void UpdateValues(IEntity owner, float timeSlice)
	{	
		if(!m_Wanted)
		{
			InitCharacter();
		}
		UpdateWantedLevel();
		UpdateSeen();
		UpdateUndercoverStatus();
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
			w.SetOpacity(m_Wanted.m_fVisualRecognitionFactor);
		}else{
			w.SetVisible(false);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	// Update Undercover Status Icon (using clothing store icon)
	//------------------------------------------------------------------------------------------------
	void UpdateUndercoverStatus()
	{
		if (!m_wRoot)
			return;
		
		Widget undercoverWidget = m_wRoot.FindWidget("Frame0.Undercover.UndercoverIcon");
		if(!undercoverWidget) 
			return;
		
		ImageWidget icon = ImageWidget.Cast(undercoverWidget);
		if(!icon) 
			return;
		
		// Check if player is disguised using the wanted component
		if (m_Wanted && m_Wanted.IsDisguisedAsOccupying())
		{
			// Blue icon for disguised as occupying faction
			icon.SetColor(Color.FromSRGBA(0, 100, 200, 255)); // Blue color
			icon.SetVisible(true);
			return;
		}
		
		// Otherwise check the faction affiliation
		if (!m_FactionAffiliation)
		{
			icon.SetVisible(false);
			return;
		}
		
		// Get current faction (not perceived - we want actual faction)
		FactionAffiliationComponent factionComp = FactionAffiliationComponent.Cast(SCR_PlayerController.GetLocalControlledEntity().FindComponent(FactionAffiliationComponent));
		if (!factionComp)
		{
			icon.SetVisible(false);
			return;
		}
		
		Faction currentFaction = factionComp.GetAffiliatedFaction();
		if (!currentFaction)
		{
			icon.SetVisible(false);
			return;
		}
		
		string factionKey = currentFaction.GetFactionKey();
		
		// Set icon color based on current faction
		if(factionKey == "CIV")
		{
			// White icon for civilian
			icon.SetColor(Color.White);
			icon.SetVisible(true);
		}
		else if(factionKey == "US" || factionKey == "USSR")
		{
			// Blue icon when actually part of occupying faction (disguised)
			icon.SetColor(Color.FromSRGBA(0, 100, 200, 255)); // Blue color
			icon.SetVisible(true);
		}
		else if(factionKey == "FIA")
		{
			// Red icon for FIA/resistance
			icon.SetColor(Color.FromSRGBA(200, 50, 50, 255)); // Red color
			icon.SetVisible(true);
		}
		else
		{
			// Hide icon when no specific faction
			icon.SetVisible(false);
		}
	}
}