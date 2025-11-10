[ComponentEditorProps(category: "Persistence", description: "Must be attached to the gamemode entity to setup the persistence system.")]
class OVT_PersistenceManagerComponentClass : ScriptComponentClass
{
}

class OVT_PersistenceManagerComponent : ScriptComponent
{
	protected SCR_PersistenceSystem m_PersistenceSystem;
	protected World m_World;
	
	override event void OnGameEnd()
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

		// Trigger shutdown save via vanilla persistence system
		if (m_PersistenceSystem)
			m_PersistenceSystem.TriggerSave(ESaveGameType.SHUTDOWN);

		super.OnGameEnd();
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

		if (m_PersistenceSystem)
		{
			Print("[Overthrow] Triggering manual save...", LogLevel.NORMAL);
			m_PersistenceSystem.TriggerSave(ESaveGameType.MANUAL);
		}
	}

	//! Trigger an auto-save via vanilla persistence system
	void AutoSave()
	{
		OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if (!mode || !mode.HasGameStarted())
			return;

		if (m_PersistenceSystem)
		{
			Print("[Overthrow] Triggering auto-save...", LogLevel.NORMAL);
			m_PersistenceSystem.TriggerSave(ESaveGameType.AUTO);
		}
	}
	
	
	bool HasSaveGame()
	{
#ifdef PLATFORM_CONSOLE
		return false;
#endif
		return FileIO.FileExists(DB_BASE_DIR + "/RootEntityCollections");
	}
	
	void WipeSave()
	{
#ifdef PLATFORM_CONSOLE
		return;
#endif
		FileIO.FindFiles(DeleteFileCallback, DB_BASE_DIR, ".json");
		FileIO.FindFiles(DeleteFileCallback, DB_BASE_DIR, ".bin");
		FileIO.FindFiles(DeleteFileCallback, DB_BASE_DIR, string.Empty);
		FileIO.DeleteFile(DB_BASE_DIR);
	}
	
	//------------------------------------------------------------------------------------------------
	protected void DeleteFileCallback(string path, FileAttribute attributes)
	{
		FileIO.DeleteFile(path);
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

		// Configure persistence collections
		ConfigureCollections();

		Print("[Overthrow] Persistence manager initialized with vanilla system", LogLevel.NORMAL);
	}

	//! Configure all 12 Overthrow persistence collections
	//! Collections organize save data by system/entity type for better organization and granular control
	protected void ConfigureCollections()
	{
		// Define save types mask (all collections support manual, auto, and shutdown saves)
		ESaveGameType saveTypes = ESaveGameType.MANUAL | ESaveGameType.AUTO | ESaveGameType.SHUTDOWN;

		// MANAGER COLLECTIONS (7)
		// These save manager component data

		PersistenceCollection.GetOrCreate("OverthrowTowns").SetSaveTypes(saveTypes);
		Print("[Overthrow] Configured collection: OverthrowTowns", LogLevel.NORMAL);

		PersistenceCollection.GetOrCreate("OverthrowPlayers").SetSaveTypes(saveTypes);
		Print("[Overthrow] Configured collection: OverthrowPlayers", LogLevel.NORMAL);

		PersistenceCollection.GetOrCreate("OverthrowEconomy").SetSaveTypes(saveTypes);
		Print("[Overthrow] Configured collection: OverthrowEconomy", LogLevel.NORMAL);

		PersistenceCollection.GetOrCreate("OverthrowFactions").SetSaveTypes(saveTypes);
		Print("[Overthrow] Configured collection: OverthrowFactions", LogLevel.NORMAL);

		PersistenceCollection.GetOrCreate("OverthrowRealEstate").SetSaveTypes(saveTypes);
		Print("[Overthrow] Configured collection: OverthrowRealEstate", LogLevel.NORMAL);

		PersistenceCollection.GetOrCreate("OverthrowRecruits").SetSaveTypes(saveTypes);
		Print("[Overthrow] Configured collection: OverthrowRecruits", LogLevel.NORMAL);

		PersistenceCollection.GetOrCreate("OverthrowConfig").SetSaveTypes(saveTypes);
		Print("[Overthrow] Configured collection: OverthrowConfig", LogLevel.NORMAL);

		// ENTITY COLLECTIONS (4)
		// These save entity spawn data and state

		PersistenceCollection.GetOrCreate("OverthrowPlaceables").SetSaveTypes(saveTypes);
		Print("[Overthrow] Configured collection: OverthrowPlaceables", LogLevel.NORMAL);

		PersistenceCollection.GetOrCreate("OverthrowBuildings").SetSaveTypes(saveTypes);
		Print("[Overthrow] Configured collection: OverthrowBuildings", LogLevel.NORMAL);

		PersistenceCollection.GetOrCreate("OverthrowBases").SetSaveTypes(saveTypes);
		Print("[Overthrow] Configured collection: OverthrowBases", LogLevel.NORMAL);

		PersistenceCollection.GetOrCreate("OverthrowCharacters").SetSaveTypes(saveTypes);
		Print("[Overthrow] Configured collection: OverthrowCharacters", LogLevel.NORMAL);

		// STATE COLLECTIONS (1)
		// These save global state data via proxy PersistentState objects

		PersistenceCollection.GetOrCreate("OverthrowLoadouts").SetSaveTypes(saveTypes);
		Print("[Overthrow] Configured collection: OverthrowLoadouts", LogLevel.NORMAL);

		Print("[Overthrow] Configured 12 persistence collections", LogLevel.NORMAL);
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