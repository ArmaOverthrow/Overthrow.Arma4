class OVT_OccupyingFactionDeathStabilityModifier : OVT_StabilityModifier
{
	
	override void OnPostInit()
	{
		OVT_Global.GetOccupyingFaction().m_OnAIKilled.Insert(OnAIKilled);
	}
	
	override void OnDestroy()
	{
		OVT_Global.GetOccupyingFaction().m_OnAIKilled.Remove(OnAIKilled);
	}
	
	protected void OnAIKilled(IEntity entity, IEntity instigator)
	{
		AddModifierToNearestTownInRange(entity.GetOrigin());
	}
}