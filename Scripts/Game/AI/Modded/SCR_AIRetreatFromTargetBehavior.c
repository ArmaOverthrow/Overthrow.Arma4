//! Modded version to prevent unwanted recruits from retreating from occupying forces
modded class SCR_AIRetreatFromTargetBehavior : SCR_AIBehaviorBase
{
	//------------------------------------------------------------------------------------------------
	//! Override CustomEvaluate to prevent unwanted recruits from retreating
	override float CustomEvaluate()
	{
		// Check if we're in Overthrow game mode
		OVT_OverthrowConfigComponent config = OVT_OverthrowConfigComponent.GetInstance();
		if (config)
		{
			// Check if I'm an unwanted recruit who should appear as civilian
			IEntity myEntity = m_Utility.m_OwnerEntity;
			if (myEntity)
			{
				FactionAffiliationComponent myFactionComp = FactionAffiliationComponent.Cast(myEntity.FindComponent(FactionAffiliationComponent));
				if (myFactionComp)
				{
					Faction myFaction = myFactionComp.GetAffiliatedFaction();
					if (myFaction)
					{
						string playerFactionKey = config.m_sPlayerFaction;
						string myFactionKey = myFaction.GetFactionKey();
						
						// Check if I'm a recruit (in player faction) who should appear as civilian when not wanted
						if (myFactionKey == playerFactionKey)
						{
							OVT_PlayerWantedComponent myWantedComp = OVT_PlayerWantedComponent.Cast(myEntity.FindComponent(OVT_PlayerWantedComponent));
							if (myWantedComp && myWantedComp.GetWantedLevel() < 1)
							{
								// I'm an unwanted recruit appearing as civilian - don't retreat from anyone
								Complete();
								return 0;
							}
						}
					}
				}
			}
		}
		
		// For all other cases, use original logic
		return super.CustomEvaluate();
	}
}