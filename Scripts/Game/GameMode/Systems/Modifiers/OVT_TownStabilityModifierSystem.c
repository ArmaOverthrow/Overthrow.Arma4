class OVT_TownStabilityModifierSystem : OVT_TownModifierSystem
{
	protected override array<ref OVT_TownModifierData> GetModifiers(OVT_TownData town)
	{
		if(!town) return null;

		return town.stabilityModifiers;
	}

	protected override bool TryAddModifier(int townId, int index)
	{
		return m_TownManager.TryAddStabilityModifier(townId, index);
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