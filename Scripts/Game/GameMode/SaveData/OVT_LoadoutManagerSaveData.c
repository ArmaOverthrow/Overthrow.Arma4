//! Save data for LoadoutManagerComponent (minimal - actual loadouts saved individually)
[EPF_ComponentSaveDataType(OVT_LoadoutManagerComponent), BaseContainerProps()]
class OVT_LoadoutManagerSaveDataClass : EPF_ComponentSaveDataClass {};

class OVT_LoadoutManagerSaveData : EPF_ComponentSaveData
{
	// Manager component has minimal save data since individual loadouts
	// are saved separately using EPF_PersistentScriptedState pattern
	
	//! Cache statistics for debugging
	int m_iCachedLoadoutsCount;
	
	//! Mapping from logical keys to EPF IDs for persistence
	ref map<string, string> m_mLoadoutIdMapping;
	
	//! Read data from component
	override EPF_EReadResult ReadFrom(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{
		OVT_LoadoutManagerComponent loadoutManager = OVT_LoadoutManagerComponent.Cast(component);
		if (!loadoutManager)
			return EPF_EReadResult.ERROR;
			
		m_iCachedLoadoutsCount = loadoutManager.GetCachedLoadoutCount();
		m_mLoadoutIdMapping = loadoutManager.GetLoadoutIdMapping();
		return EPF_EReadResult.OK;
	}
	
	//! Apply data to component
	override EPF_EApplyResult ApplyTo(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{
		OVT_LoadoutManagerComponent loadoutManager = OVT_LoadoutManagerComponent.Cast(component);
		if (!loadoutManager)
			return EPF_EApplyResult.ERROR;
		
		// Restore ID mapping
		if (m_mLoadoutIdMapping)
			loadoutManager.RestoreLoadoutIdMapping(m_mLoadoutIdMapping);
		
		return EPF_EApplyResult.OK;
	}
}