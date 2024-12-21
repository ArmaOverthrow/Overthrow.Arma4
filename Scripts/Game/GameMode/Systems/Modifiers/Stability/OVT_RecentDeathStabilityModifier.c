class OVT_RecentDeathStabilityModifier : OVT_StabilityModifier
{
	
	override void OnPostInit()
	{
		OVT_Global.GetOccupyingFaction().m_OnAIKilled.Insert(OnAIKilled);
		
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (gameMode)
		{
			gameMode.GetOnPlayerKilled().Insert(OnPlayerKilled);
		}
	}
	
	override void OnDestroy()
	{
		OVT_Global.GetOccupyingFaction().m_OnAIKilled.Remove(OnAIKilled);
		SCR_BaseGameMode mode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		mode.GetOnPlayerKilled().Remove(OnPlayerKilled);
	}
	
	protected void OnAIKilled(IEntity entity, IEntity instigator)
	{
		AddModifierToNearestTownInRange(entity.GetOrigin());
	}
	
	protected void OnPlayerKilled(notnull SCR_InstigatorContextData instigatorContextData)
	{
		AddModifierToNearestTownInRange(instigatorContextData.GetVictimEntity().GetOrigin());
	}
}