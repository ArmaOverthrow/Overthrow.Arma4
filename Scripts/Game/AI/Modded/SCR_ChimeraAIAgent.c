//! Modded version to fix friendly AI targeting disguised players
modded class SCR_ChimeraAIAgent : ChimeraAIAgent
{
	//------------------------------------------------------------------------------------------------
	//! Override IsPerceivedEnemy to check if target is a disguised friendly before treating as enemy
	//! Also handles recruits appearing as civilians to avoid being afraid of occupying forces
	override bool IsPerceivedEnemy(IEntity entity)
	{
		// Check if we're in Overthrow game mode
		OVT_OverthrowConfigComponent config = OVT_OverthrowConfigComponent.GetInstance();
		if (!config)
		{
			// Not in Overthrow mode, use original base game logic
			return super.IsPerceivedEnemy(entity);
		}
		
		Faction myFaction;
		if (m_FactionAffiliationComponent)
			myFaction = m_FactionAffiliationComponent.GetAffiliatedFaction();
		
		if (!myFaction)
			return false;
		
		// Get player faction from config
		string playerFactionKey = config.m_sPlayerFaction;
		string myFactionKey = myFaction.GetFactionKey();
		
		// Check if I'm a recruit (in player faction) who should appear as civilian when not wanted
		if (myFactionKey == playerFactionKey)
		{
			OVT_PlayerWantedComponent myWantedComp = OVT_PlayerWantedComponent.Cast(GetControlledEntity().FindComponent(OVT_PlayerWantedComponent));
			if (myWantedComp && myWantedComp.GetWantedLevel() < 1)
			{
				// I'm a recruit appearing as civilian - I should not perceive any factions as enemies
				// This prevents recruits from being afraid of occupying forces when not wanted
				// Only exception is if target also has wanted component and is wanted
				OVT_PlayerWantedComponent targetWantedComp = OVT_PlayerWantedComponent.Cast(entity.FindComponent(OVT_PlayerWantedComponent));
				if (targetWantedComp)
				{
					// Target is also a player/recruit - check if they're wanted
					return targetWantedComp.GetWantedLevel() >= 1;
				}
				
				// Target is not a player/recruit, but I'm appearing as civilian, so no one is enemy to me
				return false;
			}
		}
		
		// Check if this AI is player-aligned (player faction or civilian)
		bool isPlayerAligned = (myFactionKey == playerFactionKey || myFactionKey == "CIV");
		
		// If we're player-aligned AI, check if target has a wanted component (is therefore also player aligned, regardless of faction)
		if (isPlayerAligned)
		{			
			OVT_PlayerWantedComponent wantedComp = OVT_PlayerWantedComponent.Cast(entity.FindComponent(OVT_PlayerWantedComponent));
			if (wantedComp)
			{				
				// Just never shoot at a friendly, disguised or not
				return false;
			}
		}
		
		// For all other cases, use normal faction logic
		return super.IsPerceivedEnemy(entity);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Override IsEnemy faction overload to handle recruits appearing as civilians
	override bool IsEnemy(Faction otherFaction)
	{
		// Check if we're in Overthrow game mode
		OVT_OverthrowConfigComponent config = OVT_OverthrowConfigComponent.GetInstance();
		if (!config)
		{
			// Not in Overthrow mode, use original base game logic
			return super.IsEnemy(otherFaction);
		}
		
		Faction myFaction;
		if (m_FactionAffiliationComponent)
			myFaction = m_FactionAffiliationComponent.GetAffiliatedFaction();
		
		if (!myFaction || !otherFaction)
			return false;
		
		// Get player faction from config
		string playerFactionKey = config.m_sPlayerFaction;
		string myFactionKey = myFaction.GetFactionKey();
		
		// Check if I'm a recruit (in player faction) who should appear as civilian when not wanted
		if (myFactionKey == playerFactionKey)
		{
			OVT_PlayerWantedComponent myWantedComp = OVT_PlayerWantedComponent.Cast(GetControlledEntity().FindComponent(OVT_PlayerWantedComponent));
			if (myWantedComp && myWantedComp.GetWantedLevel() < 1)
			{
				// I'm a recruit appearing as civilian - no faction should be enemy to me
				// This prevents retreat behaviors from being triggered
				return false;
			}
		}
		
		// For all other cases, use normal faction logic
		return super.IsEnemy(otherFaction);
	}
	
}