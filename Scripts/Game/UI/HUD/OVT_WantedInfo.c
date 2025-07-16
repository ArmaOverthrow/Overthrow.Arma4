/*

*/

class OVT_WantedInfo : SCR_InfoDisplay {	
	OVT_PlayerWantedComponent m_Wanted = null;
	CharacterPerceivableComponent m_Percieve = null;
	SCR_CharacterFactionAffiliationComponent m_FactionAffiliation = null;
	
	// Cache widgets to avoid repeated lookups
	protected ImageWidget m_wUndercoverIcon = null;
	protected Widget m_wUndercoverFrame = null;
	
	protected float m_fUpdateCounter = 2;
	
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
		m_fUpdateCounter += timeSlice;		
		if(!m_Wanted)
		{
			InitCharacter();
		}
		if(m_fUpdateCounter >= 1.0)
		{
			m_fUpdateCounter = 0;
			UpdateWantedLevel();
			UpdateSeen();
			UpdateUndercoverStatus();
		}
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
		string occupyingFactionKey = OVT_Global.GetConfig().GetOccupyingFaction().GetFactionKey();
		string supportingFactionKey = OVT_Global.GetConfig().GetSupportingFaction().GetFactionKey();
		string playerFactionKey = OVT_Global.GetConfig().GetPlayerFaction().GetFactionKey();
		
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
			int wantedLevel = m_Wanted.GetWantedLevel();
			
			
			if (isDisguised)
			{
				// Red icon if wanted while disguised, blue if not wanted
				if (wantedLevel > 0)
				{
					iconColor = Color.FromSRGBA(200, 50, 50, 255); // Red color - disguise compromised
				}
				else
				{
					iconColor = Color.FromSRGBA(0, 100, 200, 255); // Blue color - disguise active
				}
				showIcon = true;
			}
			else
			{
			}
		}
		else
		{
		}
		
		// If not disguised, check perceived faction
		if (!showIcon && m_FactionAffiliation)
		{
			// Check perceived faction first (what enemies see us as)
			Faction perceivedFaction = m_FactionAffiliation.GetPerceivedFaction();
			string factionKey = "";
			
			if (perceivedFaction)
			{
				factionKey = perceivedFaction.GetFactionKey();
			}
			else
			{
				// No perceived faction means we appear as civilian
				factionKey = "CIV";
			}
			
			// WORKAROUND: If disguise was blocked due to incomplete uniform AND we're perceived as occupying faction, force civilian status
			if (m_Wanted && m_Wanted.IsDisguiseBlockedByIncompleteUniform() && factionKey == occupyingFactionKey)
			{
				factionKey = "CIV";
			}
			
			// Set icon color based on current faction and wanted level
			int wantedLevel = 0;
			if (m_Wanted)
			{
				wantedLevel = m_Wanted.GetWantedLevel();
			}
			
			if(factionKey == "CIV")
			{
				// Red if wanted or visibly armed, white if not
				bool isArmed = false;
				if (m_Wanted)
				{
					isArmed = m_Wanted.IsVisiblyArmed();
				}
				if (wantedLevel > 0 || isArmed)
				{
					iconColor = Color.FromSRGBA(200, 50, 50, 255); // Red color - wanted or armed
				}
				else
				{
					iconColor = Color.White; // White - normal civilian
				}
				showIcon = true;
			}
			else if(factionKey == occupyingFactionKey)
			{
				// This shouldn't happen unless disguised (handled above)
				// But just in case, show blue
				iconColor = Color.FromSRGBA(0, 100, 200, 255); // Blue color
				showIcon = true;
			}
			else if(factionKey == playerFactionKey || factionKey == supportingFactionKey)
			{
				// Always red for FIA/resistance or supporting faction
				iconColor = Color.FromSRGBA(200, 50, 50, 255); // Red color
				showIcon = true;
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
		}
		else
		{
			m_wUndercoverIcon.SetVisible(false);
		}
	}
}