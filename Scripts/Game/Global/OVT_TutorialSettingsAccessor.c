//------------------------------------------------------------------------------------------------
//! The engine-touching half of the per-machine seen store.
//!
//! Everything that can be decided without the engine lives in OVT_TutorialSeenStore, which the Logic
//! tier pins. What is left here is the BaseContainer plumbing that no unit tier can reach: getting
//! the settings module, the WriteToInstance/ReadFromInstance pair (whose names read backwards -
//! WriteToInstance is the LOAD), the mandatory null guard on an array the loader may not have
//! written, the schema-version decision, and the explicit flush to disk.
//!
//! THREE RULES, each paid for by a real bug somewhere:
//!
//!  1. NEVER parallel arrays. SCR_HintSettings keeps ids and repeat counts in two arrays and has to
//!     erase BOTH when their lengths disagree (SCR_GameplaySettings.c:204-210). One array of
//!     self-describing structs cannot get out of step with itself.
//!  2. A store that fails to load degrades to "show the tip again" - never to a crash and never to a
//!     half-loaded set. A missing module, a null array and a version mismatch all take that path.
//!  3. Flush on EVERY mutation (plan decision D8). The engine documentation reserves
//!     SaveUserSettings() for "very important cases" and SCR_HintSettings settles for
//!     UserSettingsChanged() alone; Overthrow players alt-F4 and crash far more often than they exit
//!     cleanly, and losing seen state to an unclean exit would reproduce the exact m_aHintedPlayers
//!     bug this feature exists to fix. SCR_FilterSet.c:344 is the precedent for flushing explicitly.
//!
//! Every entry point is a no-op returning false on a headless/dedicated server (System.IsConsoleApp),
//! mirroring SCR_HintManagerComponent's own guard. A server has no player profile to remember
//! anything in, and touching the store there would be a write nobody asked for.
//------------------------------------------------------------------------------------------------
class OVT_TutorialSettingsAccessor
{
	//! The settings-module class name. GetModule takes the class name as a string; there is no
	//! typed overload, so this is the one place the name is spelled.
	static const string MODULE_NAME = "OVT_TutorialSettings";

	//------------------------------------------------------------------------------------------------
	//! Loads the profile's seen record into the given store.
	//!
	//! A VERSION MISMATCH clears the ids and rewrites the block at the current version immediately,
	//! rather than leaving a stale version on disk to be re-detected every session. The tips-disabled
	//! flag survives that reset: it is a preference, not a record of ids.
	//!
	//! \param[in] store The store to fill. Cleared first, so a reload never merges with stale state.
	//! \param[out] tipsDisabled Receives the player's "Don't show tips again" choice. Written false
	//! when the store is unavailable, which is the safe default - tips show.
	//! \return True when the profile was actually read. False on console/headless or when the engine
	//! has no such module, in which case the store is empty and the caller carries on in memory.
	static bool Load(notnull OVT_TutorialSeenStore store, out bool tipsDisabled)
	{
		tipsDisabled = false;

		if (System.IsConsoleApp())
			return false;

		BaseContainer container = GetModuleContainer();
		if (!container)
			return false;

		OVT_TutorialSettings settings = new OVT_TutorialSettings();
		BaseContainerTools.WriteToInstance(settings, container);

		// MANDATORY: the loader legitimately hands back a null array for a member that has never
		// been written, and every later reader assumes it is real.
		if (!settings.m_aSeen)
			settings.m_aSeen = new array<ref OVT_SeenTutorialEntry>();

		array<string> ids = new array<string>();
		foreach (OVT_SeenTutorialEntry entry : settings.m_aSeen)
		{
			if (!entry)
				continue;

			if (entry.m_sId == "")
				continue;

			ids.Insert(entry.m_sId);
		}

		// LoadFrom owns the version decision: a mismatch clears the set and adopts the current
		// version. Reproducing that test here would be a second copy of the rule.
		store.LoadFrom(ids, settings.m_iVersion);

		tipsDisabled = settings.m_bTipsDisabled;

		if (settings.m_iVersion != OVT_TutorialSeenStore.CURRENT_VERSION)
		{
			Print("[Overthrow.Tutorial] Seen store is at schema version " + settings.m_iVersion.ToString() + ", expected " + OVT_TutorialSeenStore.CURRENT_VERSION.ToString() + " - the stored ids have been discarded and the block rewritten. Tips shown before the change will appear once more.", LogLevel.WARNING);

			Save(store, tipsDisabled);
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Writes the whole record - ids, version and the tips flag - and flushes it to disk.
	//!
	//! Read-modify-write against the live container rather than a blind overwrite, so a member this
	//! version of the code does not know about is preserved rather than blanked. The two members we
	//! DO own are always written together, from one in-memory source of truth, which is what makes
	//! them incapable of disagreeing with each other.
	//!
	//! THE FLUSH IS THROTTLED BY THE ENGINE. Measured 2026-08-07: two SaveUserSettings() calls
	//! microseconds apart leave only the FIRST on disk - the second is DROPPED, not deferred - while
	//! two calls six seconds apart both land. That is survivable here precisely because this writes
	//! the WHOLE record every time rather than a delta: a dropped flush loses nothing permanently,
	//! because the next mutation (or the clean-exit write) carries every id again. The only real
	//! exposure is an unclean exit in the seconds after a second dismissal, which costs one repeated
	//! tip. A delta-based writer would have lost that id for good.
	//!
	//! \param[in] store The store whose ids are to be persisted.
	//! \param[in] tipsDisabled The player's "Don't show tips again" choice.
	//! \return True when the record was written. False on console/headless or with no such module.
	static bool Save(notnull OVT_TutorialSeenStore store, bool tipsDisabled)
	{
		if (System.IsConsoleApp())
			return false;

		BaseContainer container = GetModuleContainer();
		if (!container)
			return false;

		OVT_TutorialSettings settings = new OVT_TutorialSettings();
		BaseContainerTools.WriteToInstance(settings, container);

		if (!settings.m_aSeen)
			settings.m_aSeen = new array<ref OVT_SeenTutorialEntry>();
		else
			settings.m_aSeen.Clear();

		array<string> ids = new array<string>();
		store.WriteTo(ids);

		foreach (string id : ids)
		{
			OVT_SeenTutorialEntry entry = new OVT_SeenTutorialEntry();
			entry.m_sId = id;
			settings.m_aSeen.Insert(entry);
		}

		settings.m_iVersion = OVT_TutorialSeenStore.CURRENT_VERSION;
		settings.m_bTipsDisabled = tipsDisabled;

		BaseContainerTools.ReadFromInstance(settings, container);

		GetGame().UserSettingsChanged();
		GetGame().SaveUserSettings();

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Empties the profile's seen record and clears the tips flag. Used by the version-reset path in
	//! testing and by anything that needs to put a profile back the way it found it.
	//! \return True when the record was written.
	static bool Reset()
	{
		OVT_TutorialSeenStore empty = new OVT_TutorialSeenStore();

		return Save(empty, false);
	}

	//------------------------------------------------------------------------------------------------
	//! The settings module container, or null.
	//!
	//! ALWAYS null-guarded by every caller: GetGameUserSettings() itself can be null very early in
	//! startup, and GetModule() answers null for a class the engine has not registered. Neither is an
	//! error worth a log line every frame - the caller degrades to an in-memory store.
	//! \return The live container backing OVT_TutorialSettings, or null.
	protected static BaseContainer GetModuleContainer()
	{
		UserSettings settings = GetGame().GetGameUserSettings();
		if (!settings)
			return null;

		return settings.GetModule(MODULE_NAME);
	}
}
