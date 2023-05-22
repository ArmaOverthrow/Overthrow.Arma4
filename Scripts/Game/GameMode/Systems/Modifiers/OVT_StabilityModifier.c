class OVT_StabilityModifier : OVT_Modifier
{
	void AddModifierToNearestTown(vector pos)
	{
		OVT_TownData town = m_Towns.GetNearestTown(pos);
		int townID = m_Towns.GetTownID(town);
		m_Towns.TryAddStabilityModifier(townID, m_iIndex);
	}
	
	void AddModifierToNearestTownInRange(vector pos)
	{
		OVT_TownData town = m_Towns.GetNearestTownInRange(pos);
		if(town){
			int townID = OVT_Global.GetTowns().GetTownID(town);
			m_Towns.TryAddStabilityModifier(townID, m_iIndex);
		}
	}
}