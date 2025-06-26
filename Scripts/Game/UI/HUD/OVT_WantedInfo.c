/*

*/

class OVT_WantedInfo : SCR_InfoDisplay {	
	OVT_PlayerWantedComponent m_Wanted = null;
	CharacterPerceivableComponent m_Percieve = null;
	SCR_CharacterFactionAffiliationComponent m_FactionAffiliation = null;
	
	// Cache widgets to avoid repeated lookups
	protected ImageWidget m_wUndercoverIcon = null;
	protected Widget m_wUndercoverFrame = null;
	
	protected void InitCharacter()
	{
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(SCR_PlayerController.GetLocalControlledEntity());
		if (!character)
			return;
				
		m_Wanted = OVT_PlayerWantedComponent.Cast(character.FindComponent(OVT_PlayerWantedComponent));
		m_Percieve = CharacterPerceivableComponent.Cast(character.FindComponent(CharacterPerceivableComponent));
		m_FactionAffiliation = SCR_CharacterFactionAffiliationComponent.Cast(character.FindComponent(SCR_CharacterFactionAffiliationComponent));
		
		// Cache the undercover widgets
		if (m_wRoot)
		{
			m_wUndercoverFrame = m_wRoot.FindWidget("Frame0.Undercover");
			Widget undercoverWidget = m_wRoot.FindWidget("Frame0.Undercover.UndercoverIcon");
			if (undercoverWidget)
				m_wUndercoverIcon = ImageWidget.Cast(undercoverWidget);
		}
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
		// Use cached widget if available, otherwise find it
		if (!m_wUndercoverIcon)
		{
			if (!m_wRoot)
				return;
				
			Widget undercoverWidget = m_wRoot.FindWidget("Frame0.Undercover.UndercoverIcon");
			if(!undercoverWidget) 
				return;
			
			m_wUndercoverIcon = ImageWidget.Cast(undercoverWidget);
			m_wUndercoverFrame = m_wRoot.FindWidget("Frame0.Undercover");
		}
		
		if(!m_wUndercoverIcon) 
			return;
		
		// Default to white color
		Color iconColor = Color.White;
		bool showIcon = false;
		
		// Check if player is disguised using the wanted component
		if (m_Wanted)
		{
			bool isDisguised = m_Wanted.IsDisguisedAsOccupying();
			Print(string.Format("[Overthrow] IsDisguisedAsOccupying() returned: %1", isDisguised));
			
			if (isDisguised)
			{
				// Blue icon for disguised as occupying faction
				iconColor = Color.FromSRGBA(0, 100, 200, 255); // Blue color
				showIcon = true;
				Print("[Overthrow] Setting icon color to BLUE - disguised as occupying");
			}
		}
		else
		{
			Print("[Overthrow] WARNING: m_Wanted is null!");
		}
		
		// If not disguised, check normal faction
		if (!showIcon && m_FactionAffiliation)
		{
			// Get current faction (not perceived - we want actual faction)
			FactionAffiliationComponent factionComp = FactionAffiliationComponent.Cast(SCR_PlayerController.GetLocalControlledEntity().FindComponent(FactionAffiliationComponent));
			if (factionComp)
			{
				Faction currentFaction = factionComp.GetAffiliatedFaction();
				if (currentFaction)
				{
					string factionKey = currentFaction.GetFactionKey();
					
					// Set icon color based on current faction
					if(factionKey == "CIV")
					{
						// White icon for civilian
						iconColor = Color.White;
						showIcon = true;
						Print("[Overthrow] Setting icon color to WHITE - civilian");
					}
					else if(factionKey == "US" || factionKey == "USSR")
					{
						// Blue icon when actually part of occupying faction (disguised)
						iconColor = Color.FromSRGBA(0, 100, 200, 255); // Blue color
						showIcon = true;
						Print("[Overthrow] Setting icon color to BLUE - occupying faction");
					}
					else if(factionKey == "FIA")
					{
						// Red icon for FIA/resistance  
						iconColor = Color.FromSRGBA(200, 50, 50, 255); // Red color
						showIcon = true;
						Print("[Overthrow] Setting icon color to RED - FIA");
					}
				}
			}
		}
		
		// Apply the color and visibility
		if (showIcon)
		{
			// Set visibility first
			m_wUndercoverIcon.SetVisible(true);
			m_wUndercoverIcon.SetOpacity(1.0);
			
			// Load the image from the imageset first
			m_wUndercoverIcon.LoadImageFromSet(0, "{C7691945DE01FB28}UI/Imagesets/overthrow_mapicons.imageset", "clothes");
			
			// Apply color using the standard base game approach
			m_wUndercoverIcon.SetColor(iconColor);
			
			// Also set color on parent frame if it exists (for inheritance)
			if (m_wUndercoverFrame)
			{
				m_wUndercoverFrame.SetColor(iconColor);
			}
			
			// Debug logging
			Print(string.Format("[Overthrow] Icon color set to - R=%1 G=%2 B=%3 A=%4", 
				iconColor.R() * 255, 
				iconColor.G() * 255, 
				iconColor.B() * 255, 
				iconColor.A() * 255));
		}
		else
		{
			m_wUndercoverIcon.SetVisible(false);
		}
	}
}