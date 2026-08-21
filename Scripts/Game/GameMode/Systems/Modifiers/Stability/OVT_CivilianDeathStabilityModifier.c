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
	
	//------------------------------------------------------------------------------------------------
	//! Any character death in the world. Only an AI civilian's counts here - see
	//! OVT_Modifier.IsCivilianCharacterDeath for what a player death used to cost the player.
	protected void OnCharacterKilled(IEntity victim, IEntity instigator)
	{
		if(!IsCivilianCharacterDeath(victim)) return;

		AddModifierToNearestTownInRange(victim.GetOrigin());
	}
	
	protected void OnPlayerKilled(notnull SCR_InstigatorContextData instigatorContextData)
	{
		// We don't apply civilian death modifier for player deaths
		return;
	}
}