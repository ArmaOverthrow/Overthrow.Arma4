class OVT_OccupyingFactionDeathSupportModifier : OVT_SupportModifier
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
		// Only add support modifier if town stability is below 50%
		OVT_TownData town = m_Towns.GetNearestTownInRange(entity.GetOrigin());
		if (town && town.stability < 50)
		{
			int townID = m_Towns.GetTownID(town);
			m_Towns.TryAddSupportModifier(townID, m_iIndex);
		}
	}
}