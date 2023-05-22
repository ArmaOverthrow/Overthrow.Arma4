class OVT_SupportModifier : OVT_Modifier
{
	void AddModifierToNearestTown(vector pos)
	{
		OVT_TownData town = m_Towns.GetNearestTown(pos);
		int townID = m_Towns.GetTownID(town);
		m_Towns.TryAddSupportModifier(townID, m_iIndex);
	}
	
	void AddModifierToNearestTownInRange(vector pos)
	{
		OVT_TownData town = m_Towns.GetNearestTownInRange(pos);
		if(town){
			int townID = m_Towns.GetTownID(town);
			m_Towns.TryAddSupportModifier(townID, m_iIndex);
		}
	}
}