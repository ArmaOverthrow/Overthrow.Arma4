class OVT_CivilianDeathStabilityModifier : OVT_StabilityModifier
{
	
	override void OnPostInit()
	{
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (gameMode)
		{
			gameMode.GetOnPlayerKilled().Insert(OnPlayerKilled);
		}
		
		OVT_OverthrowGameMode overthrowGameMode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if (overthrowGameMode)
		{
			overthrowGameMode.GetOnCharacterKilled().Insert(OnCharacterKilled);
		}
	}
	
	override void OnDestroy()
	{
		SCR_BaseGameMode mode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (mode)
			mode.GetOnPlayerKilled().Remove(OnPlayerKilled);
			
		OVT_OverthrowGameMode overthrowGameMode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if (overthrowGameMode)
			overthrowGameMode.GetOnCharacterKilled().Remove(OnCharacterKilled);
	}
	
	protected void OnCharacterKilled(IEntity victim, IEntity instigator)
	{
		if (!victim)
			return;
			
		// Skip players
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(victim);
		if (!character)
			return;
			
		// Check if it's a player
		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(victim);
		if (playerId > 0)
			return;
			
		// Check if it's a recruit
		OVT_RecruitManagerComponent recruitManager = OVT_Global.GetRecruits();
		if (recruitManager && recruitManager.GetRecruitFromEntity(victim))
			return;
			
		// Check faction
		FactionAffiliationComponent factionComp = FactionAffiliationComponent.Cast(victim.FindComponent(FactionAffiliationComponent));
		if (!factionComp)
			return;
			
		Faction faction = factionComp.GetAffiliatedFaction();
		if (!faction)
			return;
			
		// Only apply for civilian faction
		if (faction.GetFactionKey() == "CIV")
		{
			AddModifierToNearestTownInRange(victim.GetOrigin());
		}
	}
	
	protected void OnPlayerKilled(notnull SCR_InstigatorContextData instigatorContextData)
	{
		// We don't apply civilian death modifier for player deaths
		return;
	}
}