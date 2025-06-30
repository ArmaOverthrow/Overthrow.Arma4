//! Save data for individual player loadouts using EPF persistent scripted state
[
	EPF_PersistentScriptedStateSettings(OVT_PlayerLoadout),
	EDF_DbName.Automatic()
]
class OVT_PlayerLoadoutSaveData : EPF_ScriptedStateSaveData
{
	string m_sLoadoutName;
	string m_sPlayerId;
	string m_sDescription;
	int m_iCreatedTimestamp;
	int m_iLastUsedTimestamp;
	bool m_bIsTemplate;
	bool m_bIsOfficerTemplate;
	ref array<ref OVT_LoadoutItem> m_aItems;
	ref array<string> m_aQuickSlotItems;
	ref OVT_LoadoutMetadata m_Metadata;
	
	//! Read data from scripted state
	override EPF_EReadResult ReadFrom(notnull Managed scriptedState)
	{
		OVT_PlayerLoadout loadout = OVT_PlayerLoadout.Cast(scriptedState);
		if (!loadout)
			return EPF_EReadResult.ERROR;
			
		m_sLoadoutName = loadout.m_sLoadoutName;
		m_sPlayerId = loadout.m_sPlayerId;
		m_sDescription = loadout.m_sDescription;
		m_iCreatedTimestamp = loadout.m_iCreatedTimestamp;
		m_iLastUsedTimestamp = loadout.m_iLastUsedTimestamp;
		m_bIsTemplate = loadout.m_bIsTemplate;
		m_bIsOfficerTemplate = loadout.m_bIsOfficerTemplate;
		m_aItems = loadout.m_aItems;
		m_aQuickSlotItems = loadout.m_aQuickSlotItems;
		m_Metadata = loadout.m_Metadata;
		
		return EPF_EReadResult.OK;
	}
	
	//! Apply data to scripted state
	override EPF_EApplyResult ApplyTo(notnull Managed scriptedState)
	{
		OVT_PlayerLoadout loadout = OVT_PlayerLoadout.Cast(scriptedState);
		if (!loadout)
			return EPF_EApplyResult.ERROR;
			
		loadout.m_sLoadoutName = m_sLoadoutName;
		loadout.m_sPlayerId = m_sPlayerId;
		loadout.m_sDescription = m_sDescription;
		loadout.m_iCreatedTimestamp = m_iCreatedTimestamp;
		loadout.m_iLastUsedTimestamp = m_iLastUsedTimestamp;
		loadout.m_bIsTemplate = m_bIsTemplate;
		loadout.m_bIsOfficerTemplate = m_bIsOfficerTemplate;
		loadout.m_aItems = m_aItems;
		loadout.m_aQuickSlotItems = m_aQuickSlotItems;
		loadout.m_Metadata = m_Metadata;
		
		return EPF_EApplyResult.OK;
	}
}