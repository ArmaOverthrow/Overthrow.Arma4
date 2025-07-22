modded class OVT_MainMenuContext
{
	OVT_OccupyingFactionManager m_OccupyingFactionManager;
	protected int m_iFrameCounter = 0;
	protected const int UPDATE_INTERVAL_FRAMES = 300; // Update every ~5 seconds (60 FPS * 5)
	
	override void PostInit()
	{		
		super.PostInit();
		
		// Try to get the faction manager immediately
		m_OccupyingFactionManager = OVT_Global.GetOccupyingFaction();
		
		if(m_OccupyingFactionManager)
		{
			Print(string.Format("TSE_MainMenuContext: Successfully initialized with faction manager. Resources: %1, Threat: %2", 
				m_OccupyingFactionManager.m_iResources, 
				m_OccupyingFactionManager.m_iThreat));
				
			// Request current faction data from server if we're on client
			if(!Replication.IsServer())
			{
				m_OccupyingFactionManager.RequestFactionData();
			}
		}
		else
		{
			Print("TSE_MainMenuContext: Faction manager not available yet, will retry...");
			// Retry getting the faction manager after a delay
			GetGame().GetCallqueue().CallLater(RetryGetFactionManager, 2000);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Retry getting the faction manager if it wasn't available initially
	protected void RetryGetFactionManager()
	{
		if(!m_OccupyingFactionManager)
		{
			m_OccupyingFactionManager = OVT_Global.GetOccupyingFaction();
			
			if(m_OccupyingFactionManager)
			{
				Print(string.Format("TSE_MainMenuContext: Successfully got faction manager on retry. Resources: %1, Threat: %2", 
					m_OccupyingFactionManager.m_iResources, 
					m_OccupyingFactionManager.m_iThreat));
			}
			else
			{
				Print("TSE_MainMenuContext: Still failed to get faction manager, will retry again...");
				GetGame().GetCallqueue().CallLater(RetryGetFactionManager, 3000);
			}
		}
	}
	
	override void OnShow()
	{		
		super.OnShow();
		
		// Add our faction information display
		AddFactionInfoDisplay();
	}
	
	override void OnFrame(float timeSlice)
	{
		super.OnFrame(timeSlice);
		
		// Update faction info periodically
		if(m_bIsActive && m_OccupyingFactionManager)
		{
			m_iFrameCounter++;
			if(m_iFrameCounter >= UPDATE_INTERVAL_FRAMES)
			{
				UpdateFactionInfoDisplay();
				m_iFrameCounter = 0;
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Adds the faction resources and threat display to the existing UI
	protected void AddFactionInfoDisplay()
	{
		if(!m_wRoot || !m_OccupyingFactionManager) return;
		
		// Find the town info text widget to add our info below it
		TextWidget townInfoText = m_Widgets.m_TownInfoText;
		if(!townInfoText) return;
		
		// Get current town info
		OVT_TownData town = m_TownManager.GetNearestTown(m_Owner.GetOrigin());
		if(!town) return;
		
		// Get faction data with error checking
		int ofResources = 0;
		float resistanceThreat = 0;
		
		if(m_OccupyingFactionManager)
		{
			ofResources = m_OccupyingFactionManager.m_iResources;
			resistanceThreat = m_OccupyingFactionManager.m_iThreat;
			
			Print(string.Format("TSE_MainMenuContext: Faction data - Resources: %1, Threat: %2 (IsServer: %3)", 
				ofResources, resistanceThreat, Replication.IsServer()));
		}
		else
		{
			Print("TSE_MainMenuContext: ERROR - m_OccupyingFactionManager is null!");
			
			// Try to get it again
			m_OccupyingFactionManager = OVT_Global.GetOccupyingFaction();
			if(m_OccupyingFactionManager)
			{
				ofResources = m_OccupyingFactionManager.m_iResources;
				resistanceThreat = m_OccupyingFactionManager.m_iThreat;
				Print(string.Format("TSE_MainMenuContext: Got faction manager on retry - Resources: %1, Threat: %2", ofResources, resistanceThreat));
			}
		}
		
		// Create enhanced town info with faction data
		string enhancedInfo = string.Format("#OVT-Population: %1\n#OVT-Stability: %2%\n#OVT-Supporters: %3 (%4%)\n\nOF Resources: %5\nResistance Threat: %6", 
			town.population, 
			town.stability, 
			town.support, 
			town.SupportPercentage(),
			ofResources,
			Math.Floor(resistanceThreat)
		);
		
		// Update the town info text with our enhanced information
		townInfoText.SetText(enhancedInfo);
		
		// Apply color coding based on threat level
		if(resistanceThreat > 1000)
		{
			// High threat - red tint
			townInfoText.SetColor(Color.FromSRGBA(255, 200, 200, 255));
		}
		else if(resistanceThreat > 500)
		{
			// Medium threat - orange tint
			townInfoText.SetColor(Color.FromSRGBA(255, 220, 200, 255));
		}
		else
		{
			// Low threat - normal color
			townInfoText.SetColor(Color.FromSRGBA(255, 255, 255, 255));
		}
		
		Print("TSE_MainMenuContext: Added faction resources and threat display");
	}
	
	//------------------------------------------------------------------------------------------------
	//! Updates the faction information display
	protected void UpdateFactionInfoDisplay()
	{
		if(!m_wRoot || !m_OccupyingFactionManager) return;
		
		// Get current faction data with error checking
		int ofResources = 0;
		float resistanceThreat = 0;
		
		if(m_OccupyingFactionManager)
		{
			ofResources = m_OccupyingFactionManager.m_iResources;
			resistanceThreat = m_OccupyingFactionManager.m_iThreat;
		}
		else
		{
			Print("TSE_MainMenuContext: ERROR - m_OccupyingFactionManager is null in UpdateFactionInfoDisplay!");
			
			// Try alternative method - get from game mode using public methods
			OVT_OverthrowGameMode gameMode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
			if(gameMode)
			{
				OVT_OccupyingFactionManager altManager = OVT_OccupyingFactionManager.GetInstance();
				if(altManager)
				{
					ofResources = altManager.m_iResources;
					resistanceThreat = altManager.m_iThreat;
					Print(string.Format("TSE_MainMenuContext: Got data from GetInstance() - Resources: %1, Threat: %2", ofResources, resistanceThreat));
				}
			}
		}
		
		// Find the town info text widget
		TextWidget townInfoText = m_Widgets.m_TownInfoText;
		if(!townInfoText) return;
		
		// Get current town data
		OVT_TownData town = m_TownManager.GetNearestTown(m_Owner.GetOrigin());
		if(!town) return;
		
		// Update with current faction data
		string enhancedInfo = string.Format("#OVT-Population: %1\n#OVT-Stability: %2%\n#OVT-Supporters: %3 (%4%)\n\nOF Resources: %5\nResistance Threat: %6", 
			town.population, 
			town.stability, 
			town.support, 
			town.SupportPercentage(),
			ofResources,
			Math.Floor(resistanceThreat)
		);
		
		townInfoText.SetText(enhancedInfo);
		
		// Update color coding
		if(resistanceThreat > 1000)
		{
			townInfoText.SetColor(Color.FromSRGBA(255, 200, 200, 255));
		}
		else if(resistanceThreat > 500)
		{
			townInfoText.SetColor(Color.FromSRGBA(255, 220, 200, 255));
		}
		else
		{
			townInfoText.SetColor(Color.FromSRGBA(255, 255, 255, 255));
		}
	}
} 