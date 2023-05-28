[EPF_ComponentSaveDataType(OVT_TownManagerComponent), BaseContainerProps()]
class OVT_TownSaveDataClass : EPF_ComponentSaveDataClass
{
};

[EDF_DbName.Automatic()]
class OVT_TownSaveData : EPF_ComponentSaveData
{
	array<ref OVT_TownData> m_aTowns;
	
	override EPF_EReadResult ReadFrom(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{		
		OVT_TownManagerComponent towns = OVT_TownManagerComponent.Cast(component);
		
		m_aTowns = towns.m_Towns;
		
		return EPF_EReadResult.OK;
	}
	
	override EPF_EApplyResult ApplyTo(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{
		OVT_TownManagerComponent towns = OVT_TownManagerComponent.Cast(component);
				
		foreach(OVT_TownData town : m_aTowns)
		{
			OVT_TownData existing = towns.GetNearestTown(town.location);
			if(!existing) continue;
			
			existing.CopyFrom(town);
		}
				
		return EPF_EApplyResult.OK;
	}
}