//------------------------------------------------------------------------------------------------
//! The engine-touching half of the per-profile LOCAL option store.
//!
//! It holds the BaseContainer plumbing: the module lookup, WriteToInstance/ReadFromInstance, and
//! the flush to disk. It copies OVT_MapLayerSettingsAccessor exactly.
//!
//! Every entry point is a no-op on a headless or dedicated server, which has no player profile.
//------------------------------------------------------------------------------------------------
class OVT_OptionsLocalSettingsAccessor
{
	//! The settings-module class name. `GetModule` takes it as a string; there is no typed overload.
	static const string MODULE_NAME = "OVT_OptionsLocalSettings";

	//------------------------------------------------------------------------------------------------
	//! Loads the profile's LOCAL record into the given store.
	//!
	//! A version mismatch clears the store and rewrites the block at the current version at once,
	//! rather than leaving a stale version on disk to be re-detected every session.
	//!
	//! \param[in] store The store to fill. Replaced wholesale, so a reload never merges with stale
	//! state.
	//! \return True when the profile was actually read. False on console/headless or when the engine
	//! has no such module, in which case the store is empty and the caller carries on in memory.
	static bool Load(notnull OVT_OptionsLocalStore store)
	{
		if (System.IsConsoleApp())
			return false;

		BaseContainer container = GetModuleContainer();
		if (!container)
			return false;

		OVT_OptionsLocalSettings settings = new OVT_OptionsLocalSettings();
		BaseContainerTools.WriteToInstance(settings, container);

		if (!settings.m_aEntries)
			settings.m_aEntries = new array<ref OVT_LocalOptionEntry>();

		array<string> ids = new array<string>();
		array<string> values = new array<string>();

		foreach (OVT_LocalOptionEntry entry : settings.m_aEntries)
		{
			if (!entry)
				continue;

			if (entry.m_sId == "")
				continue;

			ids.Insert(entry.m_sId);
			values.Insert(entry.m_sValue);
		}

		bool adopted = store.FromPairs(settings.m_iVersion, ids, values);

		if (!adopted)
		{
			Print("[Overthrow] Local option store is at schema version " + settings.m_iVersion.ToString() + ", expected " + OVT_OptionsLocalStore.CURRENT_VERSION.ToString() + " - the stored values have been discarded and the block rewritten.", LogLevel.WARNING);

			Save(store);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the whole record, every id and value and the version, and flushes it to disk.
	//!
	//! Read-modify-write against the live container, so a member this version of the code does not
	//! own is preserved rather than blanked. The whole record is written every time, never a delta,
	//! because the engine throttles `SaveUserSettings()` and drops the second of two close calls (R6).
	//!
	//! \param[in] store The store whose values are to be persisted.
	//! \return True when the record was written. False on console/headless or with no such module.
	static bool Save(notnull OVT_OptionsLocalStore store)
	{
		if (System.IsConsoleApp())
			return false;

		BaseContainer container = GetModuleContainer();
		if (!container)
			return false;

		OVT_OptionsLocalSettings settings = new OVT_OptionsLocalSettings();
		BaseContainerTools.WriteToInstance(settings, container);

		if (!settings.m_aEntries)
			settings.m_aEntries = new array<ref OVT_LocalOptionEntry>();
		else
			settings.m_aEntries.Clear();

		array<string> ids = new array<string>();
		array<string> values = new array<string>();
		store.ToPairs(ids, values);

		for (int i = 0; i < ids.Count(); i++)
		{
			OVT_LocalOptionEntry entry = new OVT_LocalOptionEntry();
			entry.m_sId = ids[i];
			entry.m_sValue = values[i];
			settings.m_aEntries.Insert(entry);
		}

		settings.m_iVersion = OVT_OptionsLocalStore.CURRENT_VERSION;

		BaseContainerTools.ReadFromInstance(settings, container);

		GetGame().UserSettingsChanged();
		GetGame().SaveUserSettings();

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Empties the profile's LOCAL record.
	//! \return True when the record was written.
	static bool Reset()
	{
		OVT_OptionsLocalStore empty = new OVT_OptionsLocalStore();

		return Save(empty);
	}

	//------------------------------------------------------------------------------------------------
	//! The settings module container, or null.
	//!
	//! Always null-guarded by every caller: `GetGameUserSettings()` can be null very early in
	//! startup, and `GetModule()` answers null for a class the engine has not registered.
	//! \return The live container backing `OVT_OptionsLocalSettings`, or null.
	protected static BaseContainer GetModuleContainer()
	{
		UserSettings settings = GetGame().GetGameUserSettings();
		if (!settings)
			return null;

		return settings.GetModule(MODULE_NAME);
	}
}
