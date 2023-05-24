[EPF_ComponentSaveDataType(OVT_OccupyingFactionManager), BaseContainerProps()]
class OVT_OccupyingFactionSaveDataClass : EPF_ComponentSaveDataClass
{
};

[EDF_DbName.Automatic()]
class OVT_OccupyingFactionSaveData : EPF_ComponentSaveData
{	
	int m_iResources;
	float m_iThreat;
	array<ref OVT_BaseData> m_Bases;
	array<ref OVT_RadioTowerData> m_RadioTowers;
	
	override EPF_EReadResult ReadFrom(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{		
		OVT_OccupyingFactionManager of = OVT_OccupyingFactionManager.Cast(component);
		
		m_iResources = of.m_iResources;
		m_iThreat = of.m_iThreat;
		m_Bases = of.m_Bases;
		m_RadioTowers = of.m_RadioTowers;
		
		return EPF_EReadResult.OK;
	}
	
	override EPF_EApplyResult ApplyTo(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{
		OVT_OccupyingFactionManager of = OVT_OccupyingFactionManager.Cast(component);
		
		of.m_iResources = m_iResources;
		of.m_iThreat = m_iThreat;		
		of.m_bDistributeInitial = false;
		
		foreach(OVT_BaseData base : m_Bases)
		{
			OVT_BaseData existing = of.GetNearestBase(base.location);
			if(!existing) continue;
			existing.faction = base.faction;
		}
		
		foreach(OVT_RadioTowerData tower : m_RadioTowers)
		{
			OVT_RadioTowerData existing = of.GetNearestRadioTower(tower.location);
			if(!existing) continue;
			existing.faction = tower.faction;
		}
		
		return EPF_EApplyResult.OK;
	}
}