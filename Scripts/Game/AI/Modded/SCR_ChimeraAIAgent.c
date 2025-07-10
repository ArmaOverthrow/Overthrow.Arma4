//! Modded version to fix friendly AI targeting disguised players
modded class SCR_ChimeraAIAgent : ChimeraAIAgent
{
	//------------------------------------------------------------------------------------------------
	//! Override IsPerceivedEnemy to check if target is a disguised friendly before treating as enemy
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
}