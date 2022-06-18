class OVT_SupportModifier : OVT_Modifier
{
	void AddModifierToNearestTown(vector pos)
	{
		OVT_TownData town = m_Towns.GetNearestTown(pos);
		m_Towns.TryAddSupportModifier(town.id, m_iIndex);
	}
	
	void AddModifierToNearestTownInRange(vector pos)
	{
		OVT_TownData town = m_Towns.GetNearestTownInRange(pos);
		if(town)
			m_Towns.TryAddSupportModifier(town.id, m_iIndex);
	}
}