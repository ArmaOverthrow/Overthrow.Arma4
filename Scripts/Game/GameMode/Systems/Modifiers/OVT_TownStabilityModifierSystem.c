class OVT_TownStabilityModifierSystem : OVT_TownModifierSystem
{
	protected override void TryAddModifier(int townId, int index)
	{
		m_TownManager.TryAddStabilityModifier(townId, index);
	}
	
	protected override void RemoveModifier(int townId, int index)
	{
		m_TownManager.RemoveStabilityModifier(townId, index);
	}
	
	protected override void OnTimeout(int townId, int index)
	{
		m_TownManager.TimeoutStabilityModifier(townId, index);
	}
}