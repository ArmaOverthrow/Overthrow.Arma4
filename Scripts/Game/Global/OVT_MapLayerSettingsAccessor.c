//------------------------------------------------------------------------------------------------
//! The engine-touching half of the per-profile map layer filter store.
//!
//! Everything that can be decided without the engine lives in OVT_MapLayerPrefsStore, which the
//! Logic tier pins. What is left here is the BaseContainer plumbing that no unit tier can reach:
//! getting the settings module, the WriteToInstance/ReadFromInstance pair (whose names read
//! backwards - WriteToInstance is the LOAD), the mandatory null guard on an array the loader may not
//! have written, the schema-version decision, and the explicit flush to disk.
//!
//! THREE RULES, each paid for by a real bug somewhere:
//!
//!  1. NEVER parallel arrays. SCR_HintSettings keeps ids and repeat counts in two arrays and has to
//!     erase BOTH when their lengths disagree. One array of self-describing structs cannot get out
//!     of step with itself.
//!  2. A store that fails to load degrades to "show everything" - never to a crash and never to a
//!     half-loaded set. A missing module, a null array and a version mismatch all take that path,
//!     and all three land on the pre-feature map, which is the safe answer.
//!  3. Write the WHOLE record every time, never a delta. The engine THROTTLES SaveUserSettings():
//!     two calls microseconds apart leave only the FIRST on disk - the second is DROPPED, not
//!     deferred. Whole-record writes make a dropped flush lose nothing, because the next one carries
//!     every key again. The caller's part of that bargain is to flush at the two user-separated
//!     points (panel close, map close) rather than on every toggle, so the two cannot collapse into
//!     one throttle window.
//!
//! Every entry point is a no-op returning false on a headless/dedicated server
//! (System.IsConsoleApp). A server has no player profile to remember anything in, and touching the
//! store there would be a write nobody asked for. Callers degrade to an in-memory store, so every
//! toggle still works for the session.
//------------------------------------------------------------------------------------------------
class OVT_MapLayerSettingsAccessor
{
	//! The settings-module class name. GetModule takes the class name as a string; there is no typed
	//! overload, so this is the one place the name is spelled.
	static const string MODULE_NAME = "OVT_MapLayerSettings";

	//------------------------------------------------------------------------------------------------
	//! Loads the profile's hidden-key record into the given store.
	//!
	//! A VERSION MISMATCH clears the keys and rewrites the block at the current version immediately,
	//! rather than leaving a stale version on disk to be re-detected every session.
	//!
	//! \param[in] store The store to fill. Replaced wholesale, so a reload never merges with stale
	//! state.
	//! \return True when the profile was actually read. False on console/headless or when the engine
	//! has no such module, in which case the store is empty (everything visible) and the caller
	//! carries on in memory.
	static bool Load(notnull OVT_MapLayerPrefsStore store)
	{
		if (System.IsConsoleApp())
			return false;

		BaseContainer container = GetModuleContainer();
		if (!container)
			return false;

		OVT_MapLayerSettings settings = new OVT_MapLayerSettings();
		BaseContainerTools.WriteToInstance(settings, container);

		// MANDATORY: the loader legitimately hands back a null array for a member that has never been
		// written, and every later reader assumes it is real.
		if (!settings.m_aHidden)
			settings.m_aHidden = new array<ref OVT_MapHiddenLayerEntry>();

		array<string> keys = new array<string>();
		foreach (OVT_MapHiddenLayerEntry entry : settings.m_aHidden)
		{
			if (!entry)
				continue;

			if (entry.m_sKey == "")
				continue;

			keys.Insert(entry.m_sKey);
		}

		// LoadFrom owns the version decision: a mismatch clears the set and adopts the current
		// version. Reproducing that test here would be a second copy of the rule.
		store.LoadFrom(keys, settings.m_iVersion);

		if (settings.m_iVersion != OVT_MapLayerPrefsStore.CURRENT_VERSION)
		{
			Print("[Overthrow.Map] Map layer filter store is at schema version " + settings.m_iVersion.ToString() + ", expected " + OVT_MapLayerPrefsStore.CURRENT_VERSION.ToString() + " - the stored keys have been discarded and the block rewritten. Every layer is visible again.", LogLevel.WARNING);

			Save(store);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the whole record - every hidden key and the version - and flushes it to disk.
	//!
	//! Read-modify-write against the live container rather than a blind overwrite, so a member this
	//! version of the code does not know about is preserved rather than blanked. The two members we
	//! DO own are always written together, from one in-memory source of truth, which is what makes
	//! them incapable of disagreeing with each other.
	//!
	//! \param[in] store The store whose hidden keys are to be persisted.
	//! \return True when the record was written. False on console/headless or with no such module.
	static bool Save(notnull OVT_MapLayerPrefsStore store)
	{
		if (System.IsConsoleApp())
			return false;

		BaseContainer container = GetModuleContainer();
		if (!container)
			return false;

		OVT_MapLayerSettings settings = new OVT_MapLayerSettings();
		BaseContainerTools.WriteToInstance(settings, container);

		if (!settings.m_aHidden)
			settings.m_aHidden = new array<ref OVT_MapHiddenLayerEntry>();
		else
			settings.m_aHidden.Clear();

		array<string> keys = new array<string>();
		store.WriteTo(keys);

		foreach (string key : keys)
		{
			OVT_MapHiddenLayerEntry entry = new OVT_MapHiddenLayerEntry();
			entry.m_sKey = key;
			settings.m_aHidden.Insert(entry);
		}

		settings.m_iVersion = OVT_MapLayerPrefsStore.CURRENT_VERSION;

		BaseContainerTools.ReadFromInstance(settings, container);

		GetGame().UserSettingsChanged();
		GetGame().SaveUserSettings();

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Empties the profile's hidden-key record, putting every map layer back on. Used by the
	//! version-reset path in testing and by anything that needs to put a profile back the way it
	//! found it.
	//! \return True when the record was written.
	static bool Reset()
	{
		OVT_MapLayerPrefsStore empty = new OVT_MapLayerPrefsStore();

		return Save(empty);
	}

	//------------------------------------------------------------------------------------------------
	//! The settings module container, or null.
	//!
	//! ALWAYS null-guarded by every caller: GetGameUserSettings() itself can be null very early in
	//! startup, and GetModule() answers null for a class the engine has not registered. Neither is an
	//! error worth a log line every frame - the caller degrades to an in-memory store.
	//! \return The live container backing OVT_MapLayerSettings, or null.
	protected static BaseContainer GetModuleContainer()
	{
		UserSettings settings = GetGame().GetGameUserSettings();
		if (!settings)
			return null;

		return settings.GetModule(MODULE_NAME);
	}
}
