[ComponentEditorProps(category: "Persistence", description: "Must be attached to the gamemode entity to setup the persistence system.")]
class OVT_PersistenceManagerComponentClass : ScriptComponentClass
{
}

//! WORK IN PROGRESS (vanilla-persistence feature, currently paused).
//! The original draft was written against an assumed vanilla API (SCR_PersistenceSystem.TriggerSave,
//! PersistenceCollection.GetOrCreate, DB_BASE_DIR) that does not exist in Reforger 1.7.0.54 — the real
//! API is per-entity PersistenceSystem.Save() and config-driven collections. Those call sites are
//! stubbed with TODO(vanilla-persistence) markers so the project compiles; the event hooks below use
//! the real vanilla API and are kept live.
class OVT_PersistenceManagerComponent : ScriptComponent
{
	protected SCR_PersistenceSystem m_PersistenceSystem;
	protected World m_World;

	//! Trigger the shutdown save. ScriptComponent has no OnGameEnd engine event on 1.7.0 —
	//! this must be called explicitly (e.g. by the game mode) during shutdown.
	void OnGameEnd()
	{
		// Only save if the game was actually started
		// If player exited from the start menu without starting, don't save
		OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if (!mode || !mode.HasGameStarted())
		{
			Print("[Overthrow] Game never started, skipping save on exit");
			return;
		}

		if (mode)
			mode.PreShutdownPersist();

		// TODO(vanilla-persistence): trigger a shutdown save. SCR_PersistenceSystem has no global
		// TriggerSave() — the real API is per-entity PersistenceSystem.Save(entityOrState, ESaveGameType.SHUTDOWN).
		if (m_PersistenceSystem)
			Print("[Overthrow] Shutdown save not implemented yet (vanilla-persistence WIP)", LogLevel.WARNING);
	}

	//! Trigger a manual save via vanilla persistence system
	void SaveGame()
	{
		// Only save if the game has been started
		OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if (!mode || !mode.HasGameStarted())
		{
			Print("[Overthrow] Cannot save - game not started yet", LogLevel.WARNING);
			return;
		}

		// TODO(vanilla-persistence): implement via PersistenceSystem.Save() per tracked entity/state.
		if (m_PersistenceSystem)
			Print("[Overthrow] Manual save not implemented yet (vanilla-persistence WIP)", LogLevel.WARNING);
	}

	//! Trigger an auto-save via vanilla persistence system
	void AutoSave()
	{
		OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if (!mode || !mode.HasGameStarted())
			return;

		// TODO(vanilla-persistence): implement via PersistenceSystem.Save() per tracked entity/state.
		if (m_PersistenceSystem)
			Print("[Overthrow] Auto-save not implemented yet (vanilla-persistence WIP)", LogLevel.WARNING);
	}


	bool HasSaveGame()
	{
#ifdef PLATFORM_CONSOLE
		return false;
#endif
		// TODO(vanilla-persistence): detect an existing vanilla-persistence save. The draft checked an
		// EPF-style DB_BASE_DIR that does not exist in the vanilla API.
		return false;
	}

	void WipeSave()
	{
#ifdef PLATFORM_CONSOLE
		return;
#endif
		// TODO(vanilla-persistence): wipe the vanilla-persistence save storage once its location/API is known.
		Print("[Overthrow] WipeSave not implemented yet (vanilla-persistence WIP)", LogLevel.WARNING);
	}

	override event void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);

		if (!Replication.IsServer())
			return;

		// Get vanilla persistence system instance
		m_PersistenceSystem = SCR_PersistenceSystem.GetScriptedInstance();
		if (!m_PersistenceSystem)
		{
			Print("[Overthrow] Failed to get SCR_PersistenceSystem instance!", LogLevel.ERROR);
			return;
		}

		// Hook into vanilla persistence system events
		m_PersistenceSystem.GetOnStateChanged().Insert(OnPersistenceStateChanged);
		m_PersistenceSystem.GetOnBeforeSave().Insert(OnBeforeSave);
		m_PersistenceSystem.GetOnAfterSave().Insert(OnAfterSave);

		// TODO(vanilla-persistence): collections are config-driven in the real API (PersistenceCollection
		// is sealed with a private constructor — there is no GetOrCreate()). The draft's 12 collections
		// (Towns, Players, Economy, Factions, RealEstate, Recruits, Config, Placeables, Buildings, Bases,
		// Characters, Loadouts) must be defined in the persistence system config instead.

		Print("[Overthrow] Persistence manager initialized with vanilla system", LogLevel.NORMAL);
	}

	//! Called when persistence system state changes
	protected void OnPersistenceStateChanged(EPersistenceSystemState oldState, EPersistenceSystemState newState)
	{
		Print(string.Format("[Overthrow] Persistence state changed: %1 → %2", oldState, newState), LogLevel.NORMAL);

		if (newState == EPersistenceSystemState.ACTIVE)
		{
			Print("[Overthrow] Persistence system is now active", LogLevel.NORMAL);
		}
	}

	//! Called before save operation begins
	protected void OnBeforeSave(ESaveGameType saveType)
	{
		Print(string.Format("[Overthrow] Preparing to save (type: %1)...", saveType), LogLevel.NORMAL);

		// Perform any pre-save cleanup or validation here
		OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if (mode)
			mode.PreShutdownPersist();
	}

	//! Called after save operation completes
	protected void OnAfterSave(ESaveGameType saveType, bool success)
	{
		if (success)
		{
			Print(string.Format("[Overthrow] Save completed successfully (type: %1)", saveType), LogLevel.NORMAL);
		}
		else
		{
			Print(string.Format("[Overthrow] Save failed! (type: %1)", saveType), LogLevel.ERROR);
		}
	}
}
