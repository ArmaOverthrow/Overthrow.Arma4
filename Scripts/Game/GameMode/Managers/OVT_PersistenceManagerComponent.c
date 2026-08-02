[ComponentEditorProps(category: "Persistence", description: "Must be attached to the gamemode entity to setup the persistence system.")]
class OVT_PersistenceManagerComponentClass : ScriptComponentClass
{
}

//------------------------------------------------------------------------------------------------
//! Overthrow's facade over Reforger's vanilla persistence layer.
//!
//! TWO engine layers sit behind this one component - do not conflate them:
//!
//!  1. SCR_PersistenceSystem - a server-only world system that tracks instances and serializes
//!     them. Registered for every Overthrow world by the SCR_PersistenceSystem entry in
//!     Configs/Systems/ChimeraSystemsConfig.conf, and configured by
//!     Configs/Systems/Persistence/Overthrow.conf. It has NO global save method; its Save() takes a
//!     single instance.
//!  2. SaveGameManager - a global engine singleton that owns save POINTS (create / list / load /
//!     delete). "Press save" talks to this one.
//!
//! Everything here runs on the authority only (the persistence system is SystemLocation Server and
//! clients cannot write saves). On a client HasSaveGame() stays false, exactly as before.
//------------------------------------------------------------------------------------------------
class OVT_PersistenceManagerComponent : ScriptComponent
{
	//! The vanilla persistence world system. Null on clients, and null on the server if the
	//! SCR_PersistenceSystem entry ever disappears from the systems config.
	protected SCR_PersistenceSystem m_PersistenceSystem;

	//! Cached answer to "does a save point exist for this mission?".
	//!
	//! SaveGameManager.GetSaves() is ASYNC-ONLY, so a synchronous accessor can only serve a cache.
	//! It is seeded by a GetSaves() query in OnPostInit and kept current by the manager's
	//! OnSaveCreated / OnSaveDeleted events. Consequence callers must live with: between OnPostInit
	//! and the first GetSaves callback the answer is a stale false.
	protected bool m_bHasSaveGame;

	//! True between a save request made through this component and its completion callback.
	protected bool m_bSaveInProgress;

	//! How many save points requested through this component have completed successfully since this
	//! world loaded. Monotonic, and reset by definition when the world (and this component) is
	//! replaced. It is the only honest answer to "did the save I just asked for finish?" - IsSaveInProgress()
	//! cannot distinguish "finished" from "never started", and HasSaveGame() is true the moment ANY
	//! save exists, including one taken before the caller asked for theirs.
	protected int m_iCompletedSaves;

	//! True from LoadLatestSave() until the request has either been handed to the engine or given up.
	//!
	//! It is NOT cleared when the load succeeds: a successful load transitions the game state, which
	//! tears this component down with the world. Callers waiting on a load therefore see this stay
	//! true right up to the transition, which is exactly the signal they need - the engine's own
	//! transition flag is still false while the (asynchronous) save-list query is outstanding.
	protected bool m_bLoadInProgress;

	//! Why the last load request could not proceed. Empty while one is in flight or after a hand-off.
	protected string m_sLastLoadDiagnostic;

	//! True from ReapplyLatestSaveData() until the persistence system reports its last result.
	protected bool m_bReapplyInProgress;

	//! How many instance re-applications have completed with an OK status since this world loaded.
	protected int m_iCompletedReapplies;

	//! Why the last re-application did not complete cleanly. Empty after a successful one, which is
	//! what makes it usable as a single "did that work?" question.
	protected string m_sLastReapplyDiagnostic;

	//! Fired with (bool success) when a save requested through this component completes.
	//! This is the honest save signal - subscribe to it instead of assuming a save worked.
	protected ref ScriptInvoker m_OnSaveFinished = new ScriptInvoker();

	//------------------------------------------------------------------------------------------------
	//! \return Invoker fired with (bool success) when a save requested through this component ends.
	ScriptInvoker GetOnSaveFinished()
	{
		return m_OnSaveFinished;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True when a save point exists for the current mission (cached - see m_bHasSaveGame).
	bool HasSaveGame()
	{
		return m_bHasSaveGame;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True while a save requested through this component is still in flight.
	bool IsSaveInProgress()
	{
		return m_bSaveInProgress;
	}

	//------------------------------------------------------------------------------------------------
	//! Count of save points requested through this component that have completed successfully.
	//! Read it before requesting a save and wait for it to increase to know YOUR save landed.
	//! \return The count for this world, starting at zero.
	int GetCompletedSaveCount()
	{
		return m_iCompletedSaves;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True while a load requested through LoadLatestSave() is still in flight.
	bool IsLoadInProgress()
	{
		return m_bLoadInProgress;
	}

	//------------------------------------------------------------------------------------------------
	//! Why the last LoadLatestSave() could not proceed.
	//! Empty string when a load was handed to the engine, when none has been requested, or - because
	//! a successful load replaces the world and therefore this component - after one has succeeded.
	//! \return The diagnostic, or an empty string.
	string GetLastLoadDiagnostic()
	{
		return m_sLastLoadDiagnostic;
	}

	//------------------------------------------------------------------------------------------------
	//! \return True while a re-application requested through ReapplyLatestSaveData() is in flight.
	bool IsReapplyInProgress()
	{
		return m_bReapplyInProgress;
	}

	//------------------------------------------------------------------------------------------------
	//! \return How many instance re-applications have completed with an OK status in this world.
	int GetCompletedReapplyCount()
	{
		return m_iCompletedReapplies;
	}

	//------------------------------------------------------------------------------------------------
	//! Why the last ReapplyLatestSaveData() did not complete cleanly.
	//! Empty when none has been requested and empty after one that succeeded, so a non-empty value
	//! always means the most recent attempt failed and says how.
	//! \return The diagnostic, or an empty string.
	string GetLastReapplyDiagnostic()
	{
		return m_sLastReapplyDiagnostic;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether THIS session was launched from a save point, as opposed to "some save exists on disk".
	//!
	//! This is the synchronous, unambiguous signal for "the campaign was continued" and it is what a
	//! load-on-continue flow should branch on. HasSaveGame() is deliberately NOT that: a save can
	//! exist while the player started a brand new playthrough.
	//! \return True when the SaveGameManager reports an active save for this session.
	bool IsPlayingLoadedSave()
	{
		SaveGameManager manager = GetGame().GetSaveGameManager();
		if (!manager)
			return false;

		return manager.GetActiveSave() != null;
	}

	//------------------------------------------------------------------------------------------------
	//! \return The live vanilla persistence system, or null when it is not available on this machine.
	SCR_PersistenceSystem GetPersistenceSystem()
	{
		return m_PersistenceSystem;
	}

	//------------------------------------------------------------------------------------------------
	//! Requests a player-initiated save point.
	//! Result is reported through GetOnSaveFinished(), never assumed.
	void SaveGame()
	{
		RequestSavePoint(ESaveGameType.MANUAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Requests an automatic save point.
	//! Every call creates a NEW save point. If the save list ever needs bounding, the vanilla tool for
	//! it is RequestSavePointOverwrite() on the latest AUTO save (SCR_SaveSessionToolbarAction.c:75).
	void AutoSave()
	{
		RequestSavePoint(ESaveGameType.AUTO);
	}

	//------------------------------------------------------------------------------------------------
	//! Requests the shutdown save.
	//!
	//! ScriptComponent has no engine game-end event on 1.7.0, so this must be called explicitly
	//! (e.g. by the game mode) during shutdown - it is not wired to anything by itself.
	void OnGameEnd()
	{
		// If the player quit from the start menu without ever starting, there is nothing to save.
		OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if (!mode || !mode.HasGameStarted())
		{
			Print("[Overthrow] Game never started, skipping save on exit");
			return;
		}

		mode.PreShutdownPersist();
		RequestSavePoint(ESaveGameType.SHUTDOWN);
	}

	//------------------------------------------------------------------------------------------------
	//! Continues the campaign from its most recent save point, BY RESTARTING THE SESSION.
	//!
	//! THIS IS THE PRODUCTION "CONTINUE CAMPAIGN" PATH and it is one of two ways to get saved data
	//! back. Compare ReapplyLatestSaveData() below:
	//!
	//!   LoadLatestSave()        - engine loads the savepoint and takes the session through a
	//!                             GAME-STATE TRANSITION into it. Everything comes back, including
	//!                             world entities. The current session ends.
	//!   ReapplyLatestSaveData() - persistence system re-reads the stored record for instances that
	//!                             are ALREADY LIVE and applies it to them. No transition, session
	//!                             continues, only the instances asked for are touched.
	//!
	//! Asynchronous in two stages, because the SaveGameManager offers no synchronous way to name a
	//! save: this asks for the mission's save list, and the callback loads the last entry (vanilla's
	//! own "load the latest save" idiom - SCR_ScenarioUICommon.c:74-77). The load itself performs a
	//! game-state transition, so on success this component does not live to see the end of it; poll
	//! IsLoadInProgress() to know a request is still outstanding and GetLastLoadDiagnostic() to find
	//! out why one never got that far.
	void LoadLatestSave()
	{
		m_sLastLoadDiagnostic = "";

		if (!Replication.IsServer())
		{
			m_sLastLoadDiagnostic = "only the authority can load a save point";
			Print("[Overthrow] Cannot load - clients cannot load save points", LogLevel.WARNING);
			return;
		}

		SaveGameManager manager = GetGame().GetSaveGameManager();
		if (!manager)
		{
			m_sLastLoadDiagnostic = "there is no SaveGameManager on this machine";
			Print("[Overthrow] Cannot load - no SaveGameManager", LogLevel.ERROR);
			return;
		}

		if (manager.IsBusy())
		{
			m_sLastLoadDiagnostic = "the save system is busy with another operation";
			Print("[Overthrow] Cannot load - the save system is busy", LogLevel.WARNING);
			return;
		}

		m_bLoadInProgress = true;
		manager.GetSaves(manager.GetCurrentMissionResource(), new SaveGameObtainCallback(OnLatestSaveObtained));
	}

	//------------------------------------------------------------------------------------------------
	//! SaveGameObtainCallback delegate for LoadLatestSave() - arity must match SaveGameObtainDelegate.
	//! \param[in] success Whether the save-list query itself succeeded.
	//! \param[in] saves Save points found for the current mission, oldest first.
	protected void OnLatestSaveObtained(bool success, array<SaveGame> saves)
	{
		bool found = false;
		if (success && saves)
			found = saves.Count() > 0;

		m_bHasSaveGame = found;

		if (!found)
		{
			m_bLoadInProgress = false;
			m_sLastLoadDiagnostic = "no save point exists for this mission";
			Print("[Overthrow] Cannot load - no save point exists for this mission", LogLevel.WARNING);
			return;
		}

		SaveGameManager manager = GetGame().GetSaveGameManager();
		if (!manager)
		{
			m_bLoadInProgress = false;
			m_sLastLoadDiagnostic = "the SaveGameManager disappeared before the load could start";
			Print("[Overthrow] Cannot load - no SaveGameManager", LogLevel.ERROR);
			return;
		}

		// transition: true - this is an in-session load, so the engine takes the session through a
		// game-state transition into the saved world (SCR_RewindComponent.c:105 does the same).
		// m_bLoadInProgress stays true on purpose; see the member's own comment.
		Print("[Overthrow] Loading the latest save point", LogLevel.NORMAL);
		manager.Load(saves[saves.Count() - 1], true);
	}

	//------------------------------------------------------------------------------------------------
	//! Re-reads the persisted record for the game mode and applies it to the LIVE session.
	//!
	//! No world transition, no session restart: the persistence system fetches the stored data for
	//! instances that already exist and runs their serializers' Deserialize over them
	//! (PersistenceSystem.RequestLoad - vanilla's own use is SCR_SpawnLogic.c:309-314, which
	//! re-applies a live PlayerController's stored data).
	//!
	//! WHAT IT COVERS. The instance is the game mode ENTITY, because that is the tracked unit: the
	//! native Persistence component sits on the game mode prefab, and its EntityPersistenceConfig in
	//! Configs/Systems/Persistence/Overthrow.conf owns BOTH the entity serializer and the
	//! ComponentSerializers list. Manager components are not tracked in their own right and have no
	//! id of their own, so the entity is the only thing that can be asked for - and asking for it is
	//! what re-runs every component serializer bound to it.
	//!
	//! WHAT IT DOES NOT COVER. Anything outside the game mode entity's record - world entities,
	//! characters, vehicles, placeables. Restoring those means restarting the session, which is
	//! LoadLatestSave().
	//!
	//! Every serializer reached this way runs against an ALREADY RUNNING campaign, so each one must
	//! be idempotent on a live session; see OVT_OverthrowGameModeSerializer's Deserialize and
	//! OVT_OverthrowGameMode.RestoreStartedCampaign() for how that is guaranteed there.
	//!
	//! Poll IsReapplyInProgress() for completion and read GetLastReapplyDiagnostic() afterwards - it
	//! is empty exactly when the re-application succeeded.
	void ReapplyLatestSaveData()
	{
		m_sLastReapplyDiagnostic = "";

		if (!Replication.IsServer())
		{
			m_sLastReapplyDiagnostic = "only the authority can read persisted data";
			Print("[Overthrow] Cannot re-apply saved data - clients have no persistence system", LogLevel.WARNING);
			return;
		}

		if (!m_PersistenceSystem)
		{
			m_sLastReapplyDiagnostic = "there is no persistence system in this world";
			Print("[Overthrow] Cannot re-apply saved data - no persistence system", LogLevel.ERROR);
			return;
		}

		if (m_PersistenceSystem.GetState() != EPersistenceSystemState.ACTIVE)
		{
			m_sLastReapplyDiagnostic = "the persistence system is not active yet";
			Print("[Overthrow] Cannot re-apply saved data - the persistence system is not active", LogLevel.WARNING);
			return;
		}

		IEntity owner = GetOwner();
		if (!owner)
		{
			m_sLastReapplyDiagnostic = "the persistence manager has no owner entity";
			Print("[Overthrow] Cannot re-apply saved data - no owner entity", LogLevel.ERROR);
			return;
		}

		if (!m_PersistenceSystem.IsTracked(owner))
		{
			m_sLastReapplyDiagnostic = "the game mode entity is not tracked, so it has no stored record - check the Persistence component on the game mode prefab";
			Print("[Overthrow] Cannot re-apply saved data - the game mode entity is not tracked", LogLevel.ERROR);
			return;
		}

		PersistenceLoadRequest request();
		request.Instances = {owner};

		m_bReapplyInProgress = true;
		m_PersistenceSystem.RequestLoad(request, new PersistenceResultCallback(OnReapplyResult));
	}

	//------------------------------------------------------------------------------------------------
	//! PersistenceResultCallback delegate for ReapplyLatestSaveData().
	//! Arity must match PersistenceResultDelegate exactly (SCR_SpawnLogic.c:318 is the model).
	//! \param[in] statusCode OK when the instance's stored data was found and applied.
	//! \param[in] result The instance the result belongs to.
	//! \param[in] isLast True on the final result of the request.
	//! \param[in] context Unused - no context was passed to the callback.
	protected void OnReapplyResult(EPersistenceStatusCode statusCode, Managed result, bool isLast, Managed context)
	{
		if (statusCode == EPersistenceStatusCode.OK)
		{
			m_iCompletedReapplies += 1;
			Print("[Overthrow] Saved data re-applied to the live session", LogLevel.NORMAL);
		}
		else
		{
			m_sLastReapplyDiagnostic = string.Format("the persistence system answered %1 when re-reading the stored game mode record",
				typename.EnumToString(EPersistenceStatusCode, statusCode));
			Print("[Overthrow] Re-applying saved data FAILED: " + m_sLastReapplyDiagnostic, LogLevel.ERROR);
		}

		if (isLast)
			m_bReapplyInProgress = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Deletes every save point of the current mission. Asynchronous.
	//! Purge() is vanilla's own bulk delete (SCR_BaseGameMode.HandleOnGameModeEndSaveData uses it);
	//! listing the saves and deleting them one by one would do the same thing with more moving parts.
	void WipeSave()
	{
		SaveGameManager manager = GetGame().GetSaveGameManager();
		if (!manager)
		{
			Print("[Overthrow] Cannot wipe saves - no SaveGameManager", LogLevel.ERROR);
			return;
		}

		manager.Purge(manager.GetCurrentMissionResource(), -1, new SaveGameOperationCallback(OnPurgeCompleted));
	}

	//------------------------------------------------------------------------------------------------
	//! The single save path behind SaveGame() / AutoSave() / OnGameEnd().
	//! \param[in] saveType Which kind of save point to ask the SaveGameManager for.
	protected void RequestSavePoint(ESaveGameType saveType)
	{
		OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if (!mode || !mode.HasGameStarted())
		{
			Print("[Overthrow] Cannot save - game not started yet", LogLevel.WARNING);
			NotifySaveFinished(false);
			return;
		}

		SaveGameManager manager = GetGame().GetSaveGameManager();
		if (!manager)
		{
			Print("[Overthrow] Cannot save - no SaveGameManager", LogLevel.ERROR);
			NotifySaveFinished(false);
			return;
		}

		if (!manager.IsSavingAllowed())
		{
			Print("[Overthrow] Cannot save - saving is currently disabled or disallowed", LogLevel.WARNING);
			NotifySaveFinished(false);
			return;
		}

		// Same gate vanilla uses (SCR_CreateNewSaveDialog.c:45-49, SCR_SaveSessionToolbarAction.c:69-71):
		// only ask for a blocking save outside a replication session, where a hitch costs nobody else.
		ESaveGameRequestFlags flags;
		if (RplSession.Mode() == RplMode.None)
			flags = ESaveGameRequestFlags.BLOCKING;

		m_bSaveInProgress = true;
		manager.RequestSavePoint(saveType, string.Empty, flags, new SaveGameOperationCallback(OnSavePointCompleted));
	}

	//------------------------------------------------------------------------------------------------
	//! SaveGameOperationCallback delegate - arity must match SaveGameOperationDelegate.
	//! \param[in] success Whether the save point was committed.
	protected void OnSavePointCompleted(bool success)
	{
		if (success)
		{
			m_bHasSaveGame = true;
			m_iCompletedSaves += 1;
			Print("[Overthrow] Save point created", LogLevel.NORMAL);
		}
		else
		{
			Print("[Overthrow] Save point FAILED", LogLevel.ERROR);
		}

		NotifySaveFinished(success);
	}

	//------------------------------------------------------------------------------------------------
	//! SaveGameOperationCallback delegate for WipeSave().
	//! \param[in] success Whether the purge completed.
	protected void OnPurgeCompleted(bool success)
	{
		if (success)
			Print("[Overthrow] Save data wiped", LogLevel.NORMAL);
		else
			Print("[Overthrow] Save wipe FAILED", LogLevel.ERROR);

		RefreshSaveCache();
	}

	//------------------------------------------------------------------------------------------------
	//! Clears the in-flight flag and tells subscribers what actually happened.
	//! \param[in] success Outcome to report.
	protected void NotifySaveFinished(bool success)
	{
		m_bSaveInProgress = false;
		m_OnSaveFinished.Invoke(success);
	}

	//------------------------------------------------------------------------------------------------
	//! Re-asks the SaveGameManager whether this mission has any save points. Asynchronous.
	protected void RefreshSaveCache()
	{
		SaveGameManager manager = GetGame().GetSaveGameManager();
		if (!manager)
			return;

		manager.GetSaves(manager.GetCurrentMissionResource(), new SaveGameObtainCallback(OnSavesObtained));
	}

	//------------------------------------------------------------------------------------------------
	//! SaveGameObtainCallback delegate - arity must match SaveGameObtainDelegate.
	//! \param[in] success Whether the query itself succeeded.
	//! \param[in] saves Save points found for the mission filter that was passed.
	protected void OnSavesObtained(bool success, array<SaveGame> saves)
	{
		bool found = false;
		if (success && saves)
			found = saves.Count() > 0;

		m_bHasSaveGame = found;
		PrintFormat("[Overthrow] Save scan complete, save present: %1", m_bHasSaveGame.ToString());
	}

	//------------------------------------------------------------------------------------------------
	//! SaveGameManager event: a save point was written. Connected with EventProvider.ConnectEvent.
	//! \param[in] save The save point that was created.
	[ReceiverAttribute()]
	protected void OnSaveGameCreated(SaveGame save)
	{
		if (!save)
			return;

		SaveGameManager manager = GetGame().GetSaveGameManager();
		if (manager && save.GetMissionResource() != manager.GetCurrentMissionResource())
			return;

		m_bHasSaveGame = true;
	}

	//------------------------------------------------------------------------------------------------
	//! SaveGameManager event: a save point was deleted. Whether it was the LAST one for this mission
	//! can only be answered by re-scanning, so that is what this does.
	//! \param[in] save The save point that was deleted.
	[ReceiverAttribute()]
	protected void OnSaveGameDeleted(SaveGame save)
	{
		RefreshSaveCache();
	}

	//------------------------------------------------------------------------------------------------
	override event void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!Replication.IsServer())
			return;

		m_PersistenceSystem = SCR_PersistenceSystem.GetScriptedInstance();
		if (!m_PersistenceSystem)
		{
			// Not fatal for the SaveGameManager half below, but nothing will actually be serialized.
			Print("[Overthrow] No SCR_PersistenceSystem for this world - check the SCR_PersistenceSystem entry in Configs/Systems/ChimeraSystemsConfig.conf", LogLevel.ERROR);
		}
		else
		{
			m_PersistenceSystem.GetOnStateChanged().Insert(OnPersistenceStateChanged);
			m_PersistenceSystem.GetOnBeforeSave().Insert(OnBeforeSave);
			m_PersistenceSystem.GetOnAfterSave().Insert(OnAfterSave);
		}

		SaveGameManager manager = GetGame().GetSaveGameManager();
		if (manager)
		{
			EventProvider.ConnectEvent(manager.OnSaveCreated, OnSaveGameCreated);
			EventProvider.ConnectEvent(manager.OnSaveDeleted, OnSaveGameDeleted);
		}

		RefreshSaveCache();

		Print("[Overthrow] Persistence manager initialized with vanilla system", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Drops every subscription. The autotest harness reloads the world several times per process, so
	//! leaving a receiver connected to the (process-lifetime) SaveGameManager would point it at a dead
	//! component.
	override event void OnDelete(IEntity owner)
	{
		SaveGameManager manager = GetGame().GetSaveGameManager();
		if (manager)
		{
			EventProvider.DisconnectEvent(manager.OnSaveCreated, OnSaveGameCreated);
			EventProvider.DisconnectEvent(manager.OnSaveDeleted, OnSaveGameDeleted);
		}

		if (m_PersistenceSystem)
		{
			m_PersistenceSystem.GetOnStateChanged().Remove(OnPersistenceStateChanged);
			m_PersistenceSystem.GetOnBeforeSave().Remove(OnBeforeSave);
			m_PersistenceSystem.GetOnAfterSave().Remove(OnAfterSave);
			m_PersistenceSystem = null;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Persistence system state changed.
	//! There is no OnAfterLoad invoker on 1.7.0 - reaching ACTIVE is the "data is in" signal, which is
	//! how vanilla itself detects it (SCR_ReconnectSerializer.c:45-48).
	//! \param[in] oldState State being left.
	//! \param[in] newState State being entered.
	protected void OnPersistenceStateChanged(EPersistenceSystemState oldState, EPersistenceSystemState newState)
	{
		PrintFormat("[Overthrow] Persistence state changed: %1 -> %2", oldState, newState);

		if (newState != EPersistenceSystemState.ACTIVE)
			return;

		// Playing from a save means a save demonstrably exists, and this answer needs no round trip.
		if (IsPlayingLoadedSave())
			m_bHasSaveGame = true;

		Print("[Overthrow] Persistence system is now active", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Called before any save (including engine-initiated ones), on the authority.
	//! \param[in] saveType Kind of save being taken.
	protected void OnBeforeSave(ESaveGameType saveType)
	{
		PrintFormat("[Overthrow] Preparing to save (type: %1)...", saveType);

		OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if (mode)
			mode.PreShutdownPersist();
	}

	//------------------------------------------------------------------------------------------------
	//! Called after any save completes, on the authority. Fires for engine-initiated saves too, which
	//! is why the cache is updated here as well as in the operation callback.
	//! \param[in] saveType Kind of save that was taken.
	//! \param[in] success Whether it succeeded.
	protected void OnAfterSave(ESaveGameType saveType, bool success)
	{
		if (success)
		{
			m_bHasSaveGame = true;
			PrintFormat("[Overthrow] Save completed successfully (type: %1)", saveType);
		}
		else
		{
			Print(string.Format("[Overthrow] Save failed! (type: %1)", saveType), LogLevel.ERROR);
		}
	}
}
