class OVT_OverthrowGameModeClass: SCR_BaseGameModeClass
{
};

//------------------------------------------------------------------------------------------------
//! Main game mode logic for Overthrow.
//! Handles game initialization, player management, component lifecycle, and core game flow.
class OVT_OverthrowGameMode : SCR_BaseGameMode
{
	//! UI Context for the start game menu (configured in Workbench with layout)
	[Attribute()]
	ref OVT_StartGameContext m_StartGameUIContext;

	//! UI Context for the respawn screen (configured in Workbench with layout)
	//!
	//! Hosted here rather than on the player character deliberately: OVT_UIManagerComponent lives on
	//! the character and closes every context it owns from its own player-death handler, so a screen
	//! registered there is torn down by the event that must open it. One instance per machine, driven
	//! by OVT_RespawnScreenHandlerComponent on the player controller.
	[Attribute()]
	ref OVT_RespawnContext m_RespawnUIContext;

	//! Prefab resource for the camera used for the single player menu at the start of the game.
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Start Camera Prefab", params: "et")]
	ResourceName m_StartCameraPrefab;
	
	//! Array of fallback home positions if no houses are available
	protected ref array<IEntity> m_aFallbackSpawnPositions = {};
	protected ref array<IEntity> m_aStartCameraPositions = {};

	//! Reference to the Overthrow configuration component.
	protected OVT_OverthrowConfigComponent m_Config;
	//! Reference to the town manager component.
	protected OVT_TownManagerComponent m_TownManager;
	//! Reference to the occupying faction manager component.
	protected OVT_OccupyingFactionManager m_OccupyingFactionManager;
	//! Reference to the resistance faction manager component.
	protected OVT_ResistanceFactionManager m_ResistanceFactionManager;
	//! Reference to the real estate manager component.
	protected OVT_RealEstateManagerComponent m_RealEstate;
	//! Reference to the vehicle manager component.
	protected OVT_VehicleManagerComponent m_VehicleManager;
	//! Reference to the economy manager component.
	protected OVT_EconomyManagerComponent m_EconomyManager;
	//! Reference to the player manager component.
	protected OVT_PlayerManagerComponent m_PlayerManager;
	//! Reference to the job manager component.
	protected OVT_JobManagerComponent m_JobManager;
	//! Reference to the skill manager component.
	protected OVT_SkillManagerComponent m_SkillManager;
	//! Reference to the persistence manager component.
	protected OVT_PersistenceManagerComponent m_Persistence;
	//! Reference to the deployment manager component.
	protected OVT_DeploymentManagerComponent m_Deployment;
	//! Reference to the perceived faction manager component.
	protected SCR_PerceivedFactionManagerComponent m_PerceivedFactionManager;

	//! Reference to the start camera entity.
	protected CameraBase m_pCamera;

	//! Set of persistent IDs for players who have fully initialized.
	ref set<string> m_aInitializedPlayers;
	//! Set of persistent IDs for players who have received the intro hint.
	ref set<string> m_aHintedPlayers;

	//! Map of persistent player IDs to their group entity IDs.
	ref map<string, EntityID> m_mPlayerGroups;

	//! Flag indicating if the core game components and logic have been initialized.
	protected bool m_bGameInitialized = false;
	//! Flag indicating if the initial start camera has been positioned.
	protected bool m_bCameraSet = false;
	//! Flag indicating if the game has officially started (after potential setup phases).
	protected bool m_bGameStarted = false;
	//! Flag to trigger game start during the OnWorldPostProcess phase, typically used after loading a save.
	protected bool m_bRequestStartOnPostProcess = false;
	//! Flag indicating that OnWorldPostProcess has already run for this world.
	//! Lets a late restore (persistence deserializes after post-process) still reach DoStartGame().
	protected bool m_bWorldPostProcessed = false;
	//! Flag indicating that a DoStartGame() call has already been scheduled.
	//! DoStartGame() is not idempotent - a second call re-runs every PostGameStart() and re-registers
	//! repeating timers - so every scheduling path goes through ScheduleStartGame().
	protected bool m_bStartGameScheduled = false;

	//! Tracks if the player has opened the Overthrow menu at least once.
	bool m_bHasOpenedMenu = false;
	
	//! Event fired when any character is killed (regardless of faction)
	ref ScriptInvoker<IEntity, IEntity> m_OnCharacterKilled = new ScriptInvoker<IEntity, IEntity>();

	//------------------------------------------------------------------------------------------------
	//! Checks if the game mode has completed its initialization process.
	//! \\return True if the game is initialized, false otherwise.
	bool IsInitialized()
	{
		return m_bGameInitialized;
	}

	//------------------------------------------------------------------------------------------------
	//! Checks if the game has actually started (user clicked start, or dedicated server auto-started).
	//! \\return True if the game has started, false if still in menu/setup phase.
	bool HasGameStarted()
	{
		return m_bGameStarted;
	}

	//------------------------------------------------------------------------------------------------
	//! Gets the start game UI context (configured in the game mode prefab)
	//! \\return The start game context with layout configured
	OVT_StartGameContext GetStartGameContext()
	{
		return m_StartGameUIContext;
	}

	//------------------------------------------------------------------------------------------------
	//! Gets the respawn screen UI context (configured in the game mode prefab)
	//! \\return The respawn context with layout configured, or null when the prefab is missing it
	OVT_RespawnContext GetRespawnContext()
	{
		return m_RespawnUIContext;
	}


	//------------------------------------------------------------------------------------------------
	//! Initializes settings and components for a new game session.
	//! Reads configuration for factions and sets initial ownership.
	void DoStartNewGame()
	{
		bool isDedicated = RplSession.Mode() == RplMode.Dedicated || RplSession.Mode() == RplMode.Listen;
#ifdef WORKBENCH
		// Only treat as dedicated if actually running as dedicated/listen server
		// isDedicated = true; //To test dedicated server config
#endif
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		m_Config = config;
		
		if(isDedicated)
		{			
			if(config.m_ConfigFile)
			{
				if(config.m_ConfigFile.occupyingFaction != "" && config.m_ConfigFile.occupyingFaction != "FIA")
				{
					Print("[Overthrow] Overthrow_Config.json: Setting occupying faction to config value (" + config.m_ConfigFile.occupyingFaction + ")");
					config.SetOccupyingFaction(config.m_ConfigFile.occupyingFaction);
				}else{
					Print("[Overthrow] Overthrow_Config.json: Setting occupying faction to default (" + config.m_sDefaultOccupyingFaction + ")");
					config.SetOccupyingFaction(config.m_sDefaultOccupyingFaction);
				}
				
				if(config.m_ConfigFile.supportingFaction != "" && config.m_ConfigFile.supportingFaction != "FIA")
				{
					Print("[Overthrow] Overthrow_Config.json: Setting supporting faction to config value (" + config.m_ConfigFile.supportingFaction + ")");
					config.SetSupportingFaction(config.m_ConfigFile.supportingFaction);
				}else{
					Print("[Overthrow] Overthrow_Config.json: Setting supporting faction to default (" + config.m_sDefaultSupportingFaction + ")");
					config.SetSupportingFaction(config.m_sDefaultSupportingFaction);
				}
			}
		}
		m_Config.SetBaseAndTownOwners();

		if(m_OccupyingFactionManager)
		{
			Print("[Overthrow] Starting New Occupying Faction");

			m_OccupyingFactionManager.NewGameStart();
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Retrieves the persistence manager component instance.
	//! \\return The persistence manager component.
	OVT_PersistenceManagerComponent GetPersistence()
	{
		return m_Persistence;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Gets the universal character killed event
	//! \\return Script invoker for character killed events (victim, instigator)
	ScriptInvoker<IEntity, IEntity> GetOnCharacterKilled()
	{
		return m_OnCharacterKilled;
	}

	//------------------------------------------------------------------------------------------------
	//! Retrieves the perceived faction manager component instance.
	//! \\return The perceived faction manager component.
	SCR_PerceivedFactionManagerComponent GetPerceivedFactionManager()
	{
		return m_PerceivedFactionManager;
	}

	//------------------------------------------------------------------------------------------------
	//! Finalizes game startup, initializes various managers, and sets difficulty.
	//! Closes the start game UI and transitions into the active game state.
	void DoStartGame()
	{
		FactionManager fm = GetGame().GetFactionManager();
		OVT_Global.GetConfig().m_iPlayerFactionIndex = fm.GetFactionIndex(fm.GetFactionByKey(OVT_Global.GetConfig().m_sPlayerFaction));
		OVT_Global.GetConfig().m_iSupportingFactionIndex = fm.GetFactionIndex(fm.GetFactionByKey(OVT_Global.GetConfig().m_sSupportingFaction));
		OVT_Global.GetConfig().m_iOccupyingFactionIndex = fm.GetFactionIndex(fm.GetFactionByKey(OVT_Global.GetConfig().m_sOccupyingFaction));

		m_bGameStarted = true;

		// Prepare all connected players now that the game has started
		PrepareConnectedPlayers();

		if(!OVT_Global.GetConfig().m_Difficulty)
		{
			Print("[Overthrow] No difficulty settings found! Reverting to default");
			OVT_Global.GetConfig().m_Difficulty = new OVT_DifficultySettings();
		}

		if(m_EconomyManager)
		{
			Print("[Overthrow] Starting Economy");

			m_EconomyManager.PostGameStart();
		}

		if(m_TownManager)
		{
			Print("[Overthrow] Starting Towns");

			m_TownManager.PostGameStart();
		}

		if(m_OccupyingFactionManager)
		{
			Print("[Overthrow] Starting Occupying Faction");

			m_OccupyingFactionManager.PostGameStart();
		}

		if(m_ResistanceFactionManager)
		{
			Print("[Overthrow] Starting Resistance Faction");

			m_ResistanceFactionManager.PostGameStart();
		}

		if(m_JobManager)
		{
			Print("[Overthrow] Starting Jobs");

			m_JobManager.PostGameStart();
		}

		if(m_SkillManager)
		{
			Print("[Overthrow] Starting Skills");

			m_SkillManager.PostGameStart();
		}
		
		if(m_Deployment)
		{
			Print("[Overthrow] Starting Deployment");

			m_Deployment.PostGameStart();
		}
		
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if(config.m_ConfigFile)
		{			
			if(config.m_ConfigFile.difficulty != "")
			{
				Print("[Overthrow] Overthrow_Config.json - setting difficulty to " + config.m_ConfigFile.difficulty);
				foreach(OVT_DifficultySettings preset : config.m_aDifficultyPresets)
				{
					if(preset.name == config.m_ConfigFile.difficulty)
					{
						config.m_Difficulty = preset;
						break;
					}
				}
			}
			
			config.m_Difficulty.showPlayerOnMap = config.m_ConfigFile.showPlayerPosition;
			
			if(config.m_ConfigFile.overrideDifficulty)
			{
				Print("[Overthrow] Overthrow_Config.json - overriding difficulty settings in config");
				config.m_Difficulty.gunDealerSellPriceMultiplier = config.m_ConfigFile.gunDealerSellPriceMultiplier;
				config.m_Difficulty.startingCash = config.m_ConfigFile.startingCash;
				config.m_Difficulty.procurementMultiplier = config.m_ConfigFile.procurementMultiplier;
				config.m_Difficulty.vehiclePriceMultiplier = config.m_ConfigFile.vehiclePriceMultiplier;
			}
		}

		Print("[Overthrow] Overthrow Starting - setting m_bGameInitialized = true");
		m_bGameInitialized = true;

		// Every start path - new game, continued save, dedicated - funnels through here, so this is
		// where the campaign starts saving itself. No-op on clients and when already scheduled.
		if (m_Persistence)
			m_Persistence.StartAutosaves();
	}

	//------------------------------------------------------------------------------------------------
	//! Engine session-end hook (raised by game.c:755 at every session end, including a dedicated
	//! server stopping). The shutdown save lives here: without it, only vanilla's pause-menu
	//! save-and-exit ever wrote one, and a dedicated server restart lost everything since the last
	//! manual save. RequestSavePoint() gates on HasGameStarted(), so quitting from the start menu
	//! still saves nothing.
	override void OnGameEnd()
	{
		if (IsMaster() && m_Persistence)
			m_Persistence.OnGameEnd();

		super.OnGameEnd();
	}
	
	//------------------------------------------------------------------------------------------------
	//! Schedules DoStartGame() for the next call-queue tick, at most once per world.
	//!
	//! Two independent paths want the campaign started without the start menu - EOnInit when this
	//! session was launched from a save point, and the persistence layer when the saved game mode
	//! says the campaign was already running. Neither knows about the other, and DoStartGame() is not
	//! re-entrant, so both go through here.
	protected void ScheduleStartGame()
	{
		if (m_bStartGameScheduled)
			return;

		m_bStartGameScheduled = true;
		GetGame().GetCallqueue().CallLater(DoStartGame);
	}

	//------------------------------------------------------------------------------------------------
	//! Restores "this campaign was already started" while loading a save point.
	//!
	//! Called by OVT_OverthrowGameModeSerializer when the save says the campaign was running. It
	//! deliberately runs the SECOND start path only:
	//!   DoStartNewGame() GENERATES a campaign (base/town ownership, occupying faction seed) and must
	//!   never run on a restore, or freshly generated ownership overwrites what was just loaded;
	//!   DoStartGame() flips the started/initialized flags and runs each manager's PostGameStart().
	//! That is precisely what the old EPF path did, and doing it through the shipped flag rather than
	//! by writing m_bGameStarted directly keeps the restore honest: HasGameStarted() becomes true
	//! because the campaign really did start, not because a serializer said so.
	//!
	//! IDEMPOTENT BY CONTRACT. Saved data can also be re-applied to an ALREADY RUNNING session
	//! (OVT_PersistenceManagerComponent.ReapplyLatestSaveData), which re-runs the game mode
	//! serializer's Deserialize and lands here a second time. A campaign that has already started
	//! must not have its start sequence touched again - DoStartGame() is not re-entrant - so that case
	//! returns before anything is written. ScheduleStartGame() carries its own once-only guard as a
	//! second line of defence.
	void RestoreStartedCampaign()
	{
		if (!IsMaster())
			return;

		// Already running: nothing to restore, and nothing here may be re-applied on top of it.
		if (m_bGameStarted)
			return;

		// The start camera belongs to the new-campaign menu flow; a continued campaign never shows it.
		m_bCameraSet = true;
		m_bRequestStartOnPostProcess = true;

		// Deserialization can land either side of OnWorldPostProcess depending on when the persistence
		// system reaches the game mode entity, so cover both: the flag above is read by
		// OnWorldPostProcess, and this covers the case where it has already been and gone.
		if (m_bWorldPostProcessed)
			ScheduleStartGame();
	}

	//------------------------------------------------------------------------------------------------
	//! Decides how a dedicated server starts once the async save scan has answered: continue the
	//! existing campaign from its latest save point, or generate a new one when none exists.
	//!
	//! A dedicated server boots a FRESH world unless the server config loads a session save, so at
	//! EOnInit "is there a campaign already?" cannot be answered synchronously. BOUNDED: after ~10s
	//! without an answer the server starts a new game rather than never coming up (the pre-existing
	//! behaviour, on a delay).
	//! \param[in] attempt How many polls have already happened.
	protected void DecideDedicatedStart(int attempt)
	{
		// Something else started the campaign meanwhile (server config loaded a session save).
		if (m_bGameStarted)
			return;

		if (m_Persistence && !m_Persistence.IsSaveCacheSeeded() && attempt < 40)
		{
			GetGame().GetCallqueue().CallLater(DecideDedicatedStart, 250, false, attempt + 1);
			return;
		}

		if (m_Persistence && m_Persistence.HasSaveGame())
		{
			Print("[Overthrow] Dedicated server: a save exists for this mission - continuing the campaign");
			m_Persistence.LoadLatestSave();
			GetGame().GetCallqueue().CallLater(CheckDedicatedContinue, 2000, false, 0);
			return;
		}

		Print("[Overthrow] Dedicated server: no existing campaign - starting a new game");
		StartNewDedicatedGame();
	}

	//------------------------------------------------------------------------------------------------
	//! How many CheckDedicatedContinue polls (2 s apart) to allow before declaring the load dead.
	//! A successful engine hand-off transitions the world in well under this; a minute of nothing
	//! means the engine failed after the hand-off (corrupt or incompatible save), which raises no
	//! callback and leaves IsLoadInProgress() true forever.
	protected static const int DEDICATED_CONTINUE_MAX_POLLS = 30;

	//------------------------------------------------------------------------------------------------
	//! Watches a dedicated continue that was handed to the engine. A successful load replaces the
	//! world (this component included) and IsLoadInProgress() stays true right up to the transition,
	//! so still being here with the load no longer in flight means it failed - fall back to a new
	//! game rather than leaving the server sessionless. BOUNDED: an engine-side failure AFTER the
	//! hand-off never clears the in-progress flag, so after DEDICATED_CONTINUE_MAX_POLLS the load is
	//! treated as failed too - otherwise that exact failure would leave the server polling forever.
	//! \param[in] attempt How many polls have already happened.
	protected void CheckDedicatedContinue(int attempt)
	{
		if (m_bGameStarted)
			return;

		if (m_Persistence && m_Persistence.IsLoadInProgress())
		{
			if (attempt < DEDICATED_CONTINUE_MAX_POLLS)
			{
				GetGame().GetCallqueue().CallLater(CheckDedicatedContinue, 2000, false, attempt + 1);
				return;
			}

			Print("[Overthrow] Dedicated continue never completed - the engine did not finish loading the save", LogLevel.ERROR);
			StartNewDedicatedGame();
			return;
		}

		string diagnostic = "";
		if (m_Persistence)
			diagnostic = m_Persistence.GetLastLoadDiagnostic();

		Print("[Overthrow] Dedicated continue failed (" + diagnostic + ") - starting a new game instead", LogLevel.WARNING);
		StartNewDedicatedGame();
	}

	//------------------------------------------------------------------------------------------------
	//! Generates and starts a new campaign on a dedicated server. The decision above can land either
	//! side of OnWorldPostProcess, so the start is requested the same both ways: through the flag
	//! when post-process is still coming, directly when it has already been and gone (the same
	//! two-sided pattern as RestoreStartedCampaign).
	protected void StartNewDedicatedGame()
	{
		DoStartNewGame();
		m_bRequestStartOnPostProcess = true;
		if (m_bWorldPostProcessed)
			ScheduleStartGame();
	}

	//------------------------------------------------------------------------------------------------
	//! Executes post-load logic after persistence data has been loaded.
	//! Primarily handles real estate post-load procedures.
	void DoPostLoad()
	{
		if(!IsMaster()) return;

		if(m_RealEstate)
		{
			Print("[Overthrow] Real Estate Post-Load");

			m_RealEstate.OnPostLoad(this);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Prepares all currently connected players who haven't been prepared yet.
	//! Called when starting a new game after the start menu is completed.
	void PrepareConnectedPlayers()
	{
		if (!Replication.IsServer()) return;

		Print("[Overthrow] Finalizing preparation for all connected players");

		// Ensure initialized players set exists
		if (!m_aInitializedPlayers)
		{
			m_aInitializedPlayers = new set<string>;
		}

		// Get all connected players from the player manager
		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager)
		{
			Print("[Overthrow] ERROR: PlayerManager not available in PrepareConnectedPlayers", LogLevel.ERROR);
			return;
		}

		// Get list of connected player IDs
		array<int> playerIds = {};
		playerManager.GetPlayers(playerIds);

		// Iterate through all connected players
		foreach (int playerId : playerIds)
		{
			string playerUid = OVT_Global.GetPlayerUID(playerId);

			if (!playerUid || playerUid.IsEmpty())
			{
				Print("[Overthrow] WARNING: Skipping player " + playerId + " - no persistent UID available yet", LogLevel.WARNING);
				continue;
			}

			// A CONTINUE REBUILDS THE WORLD, AND DoSpawn_S DOES NOT RUN AGAIN. On the new-game path
			// SetupPlayer() has already run (OVT_SpawnLogic.DoSpawn_S) and this is a no-op. On the load
			// path it has NOT: loading a save point replaces the world, so the player manager - and its
			// session ID maps - are brand new, while the DoSpawn_S that mapped this player ran in the
			// world instance that was just thrown away. The player is already connected, so nothing
			// re-runs it. Without this, GetPersistentIDFromPlayerID() answers "" for the whole continued
			// session: the HUD reads $0 for a player whose record holds their real balance, every
			// playerId-keyed lookup misses, and the player never gets an OVT_OverthrowController.
			// Keyed on the mapping rather than on m_aInitializedPlayers, and placed above that check,
			// because the mapping is more fundamental than finalization - a player who is somehow
			// finalized but unmapped still needs it.
			if (m_PlayerManager && m_PlayerManager.GetPersistentIDFromPlayerID(playerId) == "")
			{
				Print("[Overthrow] Player " + playerId + " has no session ID mapping (continued campaign) - running SetupPlayer");
				m_PlayerManager.SetupPlayer(playerId, playerUid);
			}

			// Check if player has already been finalized
			if (m_aInitializedPlayers.Contains(playerUid))
			{
				Print("[Overthrow] Player " + playerUid + " (ID: " + playerId + ") already finalized, skipping");
				continue;
			}

			Print("[Overthrow] Finalizing player preparation: " + playerUid + " (ID: " + playerId + ")");
			FinalizePlayerPreparation(playerId, playerUid);
		}

		Print("[Overthrow] Finished finalizing " + playerIds.Count() + " connected players");
	}

	//------------------------------------------------------------------------------------------------
	//! Called every frame. Handles debug commands and manages the start camera lifecycle.
	//! \\param[in] owner The entity owning this component (the GameMode entity).
	//! \\param[in] timeSlice The time elapsed since the last frame.
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		super.EOnFrame(owner, timeSlice);

		if(DiagMenu.GetValue(250))
		{
			m_EconomyManager.DoAddPlayerMoney(SCR_PlayerController.GetLocalPlayerId(),1000);
			DiagMenu.SetValue(250,0);
		}

		if(DiagMenu.GetValue(251))
		{
			OVT_TownData town = OVT_Global.GetTowns().GetNearestTown(SCR_PlayerController.GetLocalControlledEntity().GetOrigin());
			if(town)
			{
				town.support = town.population;
			}
			DiagMenu.SetValue(251,0);
		}

		if(DiagMenu.GetValue(252))
		{
			OVT_TownData town = OVT_Global.GetTowns().GetNearestTown(SCR_PlayerController.GetLocalControlledEntity().GetOrigin());
			if(town)
			{
				OVT_Global.GetTowns().ChangeTownControl(town, OVT_Global.GetConfig().GetPlayerFactionIndex());
			}
			DiagMenu.SetValue(252,0);
		}

		if(DiagMenu.GetValue(254))
		{
			vector origin = SCR_PlayerController.GetLocalControlledEntity().GetOrigin();
			int playerId = SCR_PlayerController.GetLocalPlayerId();

			OVT_Global.GetServer().InstantCaptureBase(origin, playerId);
			DiagMenu.SetValue(254,0);
		}

		if(DiagMenu.GetValue(255))
		{
			OVT_Global.GetSkills().GiveXP(SCR_PlayerController.GetLocalPlayerId(),100);
			DiagMenu.SetValue(255,0);
		}

		if((IsMaster() && (RplSession.Mode() == RplMode.None || RplSession.Mode() == RplMode.Listen)) && !m_bCameraSet)
		{
			SetRandomCameraPosition();
		}

		if(m_bGameInitialized)
		{
			if(m_pCamera)
			{
				Print("[Overthrow] Switching from start camera to next available camera");

				CameraManager cameraMgr = GetGame().GetCameraManager();
				if (cameraMgr)
				{
					// Switch to the next camera (should be the player's camera)
					cameraMgr.SetNextCamera();
					Print("[Overthrow] Switched to next camera");
				}

				// Now delete the start camera
				Print("[Overthrow] Deleting start camera");
				SCR_EntityHelper.DeleteEntityAndChildren(m_pCamera);
				m_pCamera = null;
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Last chance to write live world facts back into the manager records a save point will carry.
	//!
	//! Called from OVT_PersistenceManagerComponent.OnBeforeSave() for EVERY save (and on the shutdown
	//! path), on the authority. Anything a manager tracks lazily - because keeping it current every
	//! frame would be wasted work - has to be brought up to date here, or the save stores a value that
	//! was true some time ago.
	//!
	//! Serializers must not do this themselves: they are pure codecs, and a codec that goes looking
	//! for world entities would behave differently depending on when the system reached it.
	void PreShutdownPersist()
	{
		if (!IsMaster())
			return;

		// Recruit bodies are deleted and rebuilt from their record's last known position, which is only
		// refreshed when a body appears or disappears. Without this, a save taken mid-session brings
		// recruits back where they were hired rather than where they were left.
		OVT_RecruitManagerComponent recruits = OVT_RecruitManagerComponent.GetInstance();
		if (recruits)
			recruits.SyncRecruitPositions();

		// The same problem for the players themselves: a player who is standing in the world right now
		// has never passed through the disconnect capture, so this is the only place their character's
		// persistence id is written down before the save. Without it, quit-to-menu and continue would
		// hand them a fresh civilian body instead of the one they were carrying gear in.
		if (m_PlayerManager)
			m_PlayerManager.SyncPlayerBodyIds();

		// And the same for owned vehicles: the registration a save carries has to describe where the
		// vehicle is NOW, or a rebuild from it puts the player's car back where they bought it.
		OVT_VehicleManagerComponent vehicles = OVT_VehicleManagerComponent.GetInstance();
		if (vehicles)
			vehicles.SyncVehicleRecords();
	}

	//------------------------------------------------------------------------------------------------
	//! Marks a character that has just died to spawn back on load, so its corpse survives a continue.
	//!
	//! WHY A CORPSE NEEDS MARKING AT ALL. Every character is tracked from the moment it spawns
	//! (Character_Base.et carries the native Persistence component), and the configuration it is matched
	//! with while ALIVE never self-spawns - deliberately, because Overthrow's managers rebuild live AI
	//! themselves (decision v2-5) and doubled AI on load is the catastrophe Phase 3 exists to prevent.
	//! But nothing rebuilds corpses, so a dead character kept under that configuration is saved and then
	//! never spawned back (BUG-018). Death is the one moment the answer changes, and this event - raised
	//! from the damage manager's damage-state-changed invoker - is where Overthrow hears about it.
	//!
	//! DISABLED 2026-08-04, AND THIS IS THE THIRD FAILED ATTEMPT AT BUG-018 - read before trying a fourth.
	//! Attempt one bound a scripted PersistenceConfigRule in Overthrow.conf: measured, the engine never
	//! consults script-defined conf rules (IsMatch called zero times in 301 forced re-matches). Attempt
	//! two replaced it with a runtime config flip - GetConfig() -> m_bSelfSpawn -> SetConfig() - through
	//! OVT_PersistenceTracking.MarkForSelfSpawn(). Measured 2026-08-04 by decoding the save blobs
	//! directly: a SCRIPTED config (which is what SetConfig produces) is written with an EMPTY
	//! configuration store name, and the loader cannot resolve one - every such record fails with
	//! "Unable to locate configuruation ''" / "Attempted to deserialize meta data without configuration".
	//! Across eleven save files the correlation was exact: 17 records written scripted, 17 records with an
	//! empty store name, zero scripted records that were readable. So the flip never brought a corpse
	//! back, and each one it marked also poisoned the save with a record the engine logs an error for on
	//! every load. Vanilla has no GetConfig/SetConfig call site anywhere, which is why this path is broken.
	//!
	//! WHAT REPLACES IT. Nothing, for AI corpses - the only mechanism that actually survives a load is
	//! SelfSpawn declared in a .conf, and the AI character config deliberately has SelfSpawn 0 to stop
	//! Overthrow's managers doubling every garrison (decision v2-5). PLAYER corpses do now come back, as
	//! a side effect of the player-character config gaining SelfSpawn 1 in Overthrow.conf. BUG-018 stays
	//! open for AI corpses and needs a config that can distinguish a dead character at LOAD time, which
	//! no native rule currently offers.
	//! \param[in] victim The character that died.
	//! \param[in] instigator Whoever killed it, unused here.
	protected void OnCharacterKilledPersist(IEntity victim, IEntity instigator)
	{
		if (!victim)
			return;

		// Deliberately empty - see above. Do not reinstate MarkForSelfSpawn() here; it writes records the
		// loader refuses to read.
	}

	//------------------------------------------------------------------------------------------------
	//! Neutralizes vanilla's end-of-game save purge. Overthrow's campaign IS the save.
	//!
	//! Vanilla behavior being removed (SCR_BaseGameMode.HandleOnGameModeEndSaveData, 1.7.0.54):
	//! at game-mode end the authority disables saving and then - on dedicated servers, and in the
	//! Workbench - runs PersistenceSystem.ClearStorage(PersistenceSessionStorage) followed by
	//! SaveGameManager.Purge() on the active playthrough, unless -keepSessionSave is passed or the
	//! persistence config says ShouldKeepSessionData(). That is correct for a one-shot scenario whose
	//! save is worthless once the round ends; for a persistent revolution campaign it deletes the
	//! whole playthrough on every restart cycle.
	//!
	//! Only the destructive half is dropped. Saving is still turned off once the mode has ended, so
	//! nothing writes to a finished session. Doing it here rather than relying on -keepSessionSave
	//! means server owners cannot lose their campaign by forgetting a CLI flag.
	protected override void HandleOnGameModeEndSaveData()
	{
		if (!IsMaster())
			return;

		// After the game mode completes no more saving needs to be done.
		SaveGameManager manager = GetGame().GetSaveGameManager();
		if (manager)
			manager.SetSavingAllowed(false);
	}


	//------------------------------------------------------------------------------------------------
	//! Selects a random fallback location from the detected list.
	//! \\return A random vector position from m_aFallbackSpawnPositions, or vector.Zero if the list is empty.
	protected vector GetRandomFallbackPosition()
	{
	    if (m_aFallbackSpawnPositions.Count() > 0)
	    {
	        int randomIndex = s_AIRandomGenerator.RandInt(0, m_aFallbackSpawnPositions.Count());
	        return m_aFallbackSpawnPositions[randomIndex].GetOrigin();
	    }
	
	    return vector.Zero;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Selects a random start camera location from the detected list.
	//! \\return A random entity from m_aStartCameraPositions, or the game mode if the list is empty.
	protected IEntity GetRandomStartCameraPosition()
	{
	    if (m_aStartCameraPositions.Count() > 0)
	    {
	        int randomIndex = s_AIRandomGenerator.RandInt(0, m_aStartCameraPositions.Count());
	        return m_aStartCameraPositions[randomIndex];
	    }
	
	    return this;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Spawns a player at a randomly selected hardcoded bus stop location.
	//! If no hardcoded locations exist, attempts to spawn at the first available town center.
	//! \\param[in] playerId The ID of the player to spawn.
	void SpawnPlayerAtFallbackPosition(int playerId)
	{
			vector spawnLocation = GetRandomFallbackPosition();
			if (spawnLocation != vector.Zero)
			{
			    Print("[Overthrow] Spawning player at fallback position: " + spawnLocation.ToString());
			    m_RealEstate.SetHomePos(playerId, spawnLocation);
			}
			else
			 {
		       	Print("[Overthrow] No bus stops found. Use current town center");
		        m_RealEstate.SetHomePos(playerId, m_TownManager.m_Towns[m_RealEstate.m_iStartingTownId].location);
	    	}
		

	}

	//------------------------------------------------------------------------------------------------
	//! Handles player role changes. Grants officer status if the player becomes an admin.
	//! \\param[in] playerId The ID of the player whose role changed.
	//! \\param[in] roleFlags The new EPlayerRole flags assigned to the player.
	protected override void OnPlayerRoleChange(int playerId, EPlayerRole roleFlags)
	{
		super.OnPlayerRoleChange(playerId, roleFlags);

		if(SCR_Global.IsAdminRole(roleFlags))
		{
			string persId = m_PlayerManager.GetPersistentIDFromPlayerID(playerId);
			if(persId == "") return;
			OVT_PlayerData player = m_PlayerManager.GetPlayer(persId);
			if(!player) return;
			if(!player.isOfficer)
			{
				m_ResistanceFactionManager.AddOfficer(playerId);
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Handles player disconnection. Persists the character, releases tracking and removes the player from the initialized list.
	//! \\param[in] playerId The ID of the disconnecting player.
	//! \\param[in] cause The reason for disconnection.
	//! \\param[in] timeout The disconnection timeout duration.
	protected override void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
	{
		string persId = m_PlayerManager.GetPersistentIDFromPlayerID(playerId);
		IEntity controlledEntity = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);

		// Mark the record offline whether or not the player still controls a body - a player who
		// leaves without one is just as gone.
		//
		// CORRECTED BY map/respawn: this used to say a player who leaves while dead or on the respawn
		// screen HAS no controlled entity. Since the death path defers character creation until the
		// player picks a spawn, they keep possessing their CORPSE for as long as the choice takes, so
		// the branch below now runs on a corpse rather than being skipped. That is safe - every capture
		// it makes (body id, transform, gear) is independently dead-guarded, so death still means total
		// loss - and the corpse is not claimed on reconnect, because OVT_ReconnectComponent refuses a
		// dead body. Net effect on reconnect: no body id, no last-known position, no gear snapshot, so
		// they spawn at home, which is what the deferred death path intends.
		OVT_PlayerData player = m_PlayerManager.GetPlayer(persId);
		if(player)
		{
			player.id = -1;
		}

		if(controlledEntity)
		{
			// Write the leaving player's character record. The body is NOT released and NOT deleted:
			// OVT_ReconnectComponent claims it moments from now (inside super.OnPlayerDisconnected
			// below) and OVT_PersistenceReservation hides it in place, still tracked. This write is
			// therefore an insurance policy, not a hand-off - it makes sure the record on disk matches
			// the body as the player left it even if the server dies before the next save point.
			//
			// WHAT USED TO BE HERE, AND WHY IT IS GONE. Until 2026-08-05 this was Save() +
			// StopTracking(keepData) and vanilla then deleted the body, on the assumption that the kept
			// record could be asked for again later. It cannot: released records were measured being
			// pruned within ten minutes, in session, with no restart (BUG-086).
			OVT_PersistenceTracking.Save(controlledEntity);

			// The explicit Save() above is the materialisation, so reading the id here is a pure lookup.
			// Remembering it is what lets OVT_SpawnLogic find THIS character again when the player
			// reconnects - by FindById while the reservation is still standing, or by RequestSpawn after
			// a restart has turned it back into a stored record.
			if(player)
			{
				m_PlayerManager.CapturePlayerBodyId(player, controlledEntity, false);

				// Stored separately from the id and unconditionally: if the character record does not
				// survive to the next session, this is what still puts the player back where they left.
				m_PlayerManager.CapturePlayerBodyTransform(player, controlledEntity);

				// ...and this is what puts their equipment back in the same case.
				m_PlayerManager.CapturePlayerGearSnapshot(persId, controlledEntity);
			}

			// SURVIVING THE RESTART is the half the reservation cannot do by itself: a live tracked body
			// is written into the save point like anything else, but only a record whose config says
			// SelfSpawn is instantiated again at load - everything else is dropped outright. Reported at
			// the exact moment the body is reserved, so a server log answers "was it saved as a player or
			// as an AI corpse?" without another round trip.
			SCR_PersistenceSystem persistence = SCR_PersistenceSystem.GetScriptedInstance();
			if (persistence)
			{
				EntityPersistenceConfig bodyConfig = EntityPersistenceConfig.Cast(persistence.GetConfig(controlledEntity));
				if (!bodyConfig)
					Print("[Overthrow] Leaving player's body has NO persistence config - its record cannot survive the restart", LogLevel.WARNING);
				else if (!bodyConfig.m_bSelfSpawn)
					Print("[Overthrow] Leaving player's body is matched to a config that does NOT self-spawn - its record will be dropped at load and they will spawn fresh", LogLevel.WARNING);
				else
					Print("[Overthrow] Leaving player's body is matched to a self-spawning config - it should survive a restart", LogLevel.NORMAL);
			}

			// NOTHING RELEASES TRACKING HERE ANY MORE. The body must stay tracked: that is what makes it
			// serialize with the next save point and self-spawn on the next load. super's own delete is
			// vetoed by OVT_ReconnectComponent.HandlePlayerDisconnect(), which also hides it.
		}

		int i = m_aInitializedPlayers.Find(persId);

		if(i > -1)
			m_aInitializedPlayers.Remove(i);

		// Notify listeners that player has disconnected
		m_PlayerManager.m_OnPlayerDisconnected.Invoke(persId, playerId);

		// After listeners have run, drop the session-scoped ID mappings so a runtime ID reused by a
		// later joiner can never resolve to this player (the OVT_PlayerData record itself is kept).
		m_PlayerManager.ClearPlayerIdMappings(playerId);

		super.OnPlayerDisconnected(playerId, cause, timeout);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Prepares a player for entry into the game. Sets up player data, assigns officer status if needed,
	//! finds or assigns a home (house or bus stop), spawns a starting car if applicable, and manages initial state.
	//! \\param[in] playerId The numeric ID of the player.
	//! \\param[in] persistentId The persistent string ID of the player.
	void PreparePlayer(int playerId, string persistentId)
	{
	    if (!Replication.IsServer()) return;

	    // Validate persistent ID
	    if(!persistentId || persistentId.IsEmpty())
	    {
	        Print("[Overthrow] ERROR: PreparePlayer called with empty/null persistentId for playerId: " + playerId);
	        return;
	    }

	    // Setup player data first (creates OVT_PlayerData object)
	    m_PlayerManager.SetupPlayer(playerId, persistentId);

	    // Then finalize preparation (home, money, officer status, etc.)
	    FinalizePlayerPreparation(playerId, persistentId);
	}

	//------------------------------------------------------------------------------------------------
	//! Finalizes player preparation by assigning officer status, home, and starting vehicle.
	//! This is separated from SetupPlayer so it can be deferred until the game starts.
	//! \\param[in] playerId The numeric ID of the player.
	//! \\param[in] persistentId The persistent string ID of the player.
	void FinalizePlayerPreparation(int playerId, string persistentId)
	{
	    if (!Replication.IsServer()) return;

	    // Validate persistent ID
	    if(!persistentId || persistentId.IsEmpty())
	    {
	        Print("[Overthrow] ERROR: FinalizePlayerPreparation called with empty/null persistentId for playerId: " + playerId);
	        return;
	    }

	    // Check if this player has already been prepared in this session (to prevent duplicates in hosted multiplayer)
	    if(m_aInitializedPlayers.Contains(persistentId))
	    {
	        Print("[Overthrow] Player " + persistentId + " already finalized in this session, skipping duplicate FinalizePlayerPreparation call");
	        return;
	    }

	    // Notify listeners that player has connected
	    m_PlayerManager.m_OnPlayerConnected.Invoke(persistentId, playerId);
	    OVT_PlayerData player = m_PlayerManager.GetPlayer(persistentId);
	    if (!player)
	        return;
	
	    // Ensure the player is an officer in single-player mode or if they're the host in hosted multiplayer
	    if (!player.isOfficer && (RplSession.Mode() == RplMode.None || (RplSession.Mode() == RplMode.Listen && playerId == 1)))
	    {
	        Print("[Overthrow] Making player " + playerId + " an officer (Mode: " + RplSession.Mode() + ")");
	        m_ResistanceFactionManager.AddOfficer(playerId);
	    }
	
	    // Check if the player has a valid home
	    vector home = m_RealEstate.GetHome(persistentId);
		Print(home.ToString() + " Home status");
	    if (home[0] == 0) // No home assigned
	    {
	        IEntity house = OVT_Global.GetRealEstate().GetRandomStartingHouse();
	        if (!house)
	        {
	            // No starting houses available, spawn at a bus stop
	            Print("[Overthrow] No Starting homes left. Spawning at bus stop.");
	            SpawnPlayerAtFallbackPosition(playerId);
	        }
	        else
	        {
	            // Assign a house and spawn a starting car
	            m_RealEstate.SetOwner(playerId, house);
	            m_RealEstate.SetHome(playerId, house);
	            m_VehicleManager.SpawnStartingCar(house, persistentId);
				Print("[Overthrow] Player assigned home at " + house.GetOrigin());
	        }
	    }
	    if (player.initialized)
	    {
	        // Handle existing players
	        if (m_aInitializedPlayers.Contains(persistentId))
	        {
	            // Optionally, handle respawn costs or penalties here
	            Print("[Overthrow] Respawning existing player: " + persistentId);
	        }
	        else
	        {
	            // Returning players who weren't marked as initialized
	            Print("[Overthrow] Preparing returning player: " + persistentId);
	            m_aInitializedPlayers.Insert(persistentId);
	        }
	        player.firstSpawn = false; // Not the first spawn
	    }
	    else
	    {
	        // Handle new players
	        Print("[Overthrow] Preparing NEW player: " + persistentId);
	
	        int cash = OVT_Global.GetConfig().m_Difficulty.startingCash;
	        m_EconomyManager.AddPlayerMoney(playerId, cash);
	
	        player.initialized = true;
	        player.firstSpawn = true; // Mark as first spawn
	        m_aInitializedPlayers.Insert(persistentId);
	    }
	
	    // Ensure the player is an officer if listed in the config file
	    if (!player.isOfficer)
	    {
	        OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
	        if (config.m_ConfigFile && config.m_ConfigFile.officers && config.m_ConfigFile.officers.Find(persistentId) > -1)
	        {
	            m_ResistanceFactionManager.AddOfficer(playerId);
	        }
	    }

	    // Teleport player to their assigned home if they're already spawned
	    TeleportPlayerToHome(playerId, persistentId);

	    // The player's character is created only now, once a home exists to spawn at. Before the
	    // start menu completes there deliberately IS no character (no home yet -> body at the world
	    // origin, and possessing it steals the camera/input from the menu - 2026-08-02 play-test).
	    OVT_SpawnLogic spawnLogic = OVT_SpawnLogic.GetInstance();
	    if (spawnLogic)
	        spawnLogic.SpawnDeferredPlayer(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Teleports a player to their assigned home position if they're already spawned.
	//! \\param[in] playerId The numeric ID of the player.
	//! \\param[in] persistentId The persistent string ID of the player.
	protected void TeleportPlayerToHome(int playerId, string persistentId)
	{
	    if (!Replication.IsServer()) return;

	    // Get the player's home position
	    vector homePos = m_RealEstate.GetHome(persistentId);
	    if (homePos[0] == 0 && homePos[1] == 0 && homePos[2] == 0)
	    {
	        Print("[Overthrow] WARNING: Cannot teleport player - no valid home position", LogLevel.WARNING);
	        return;
	    }

	    // Get the player's controlled entity
	    PlayerManager playerManager = GetGame().GetPlayerManager();
	    if (!playerManager) return;

	    IEntity playerEntity = playerManager.GetPlayerControlledEntity(playerId);
	    if (!playerEntity)
	    {
	        // Player might not be spawned yet, this is fine
	        Print("[Overthrow] Player entity not yet spawned, will spawn at home naturally");
	        return;
	    }

	    // Teleport the player to their home
	    Print("[Overthrow] Teleporting player " + playerId + " to home: " + homePos.ToString());
	    SCR_Global.TeleportPlayer(playerId, homePos);
	}


	//------------------------------------------------------------------------------------------------
	//! Called when a player character entity is spawned into the world.
	//! Resets the player's wanted level.
	//! \\param[in] playerId The ID of the player whose character spawned.
	//! \\param[in] controlledEntity The newly spawned character entity.
	protected override void OnPlayerSpawned(int playerId, IEntity controlledEntity)
	{
		OVT_PlayerWantedComponent wanted = OVT_PlayerWantedComponent.Cast(controlledEntity.FindComponent(OVT_PlayerWantedComponent));
		if(!wanted){
			Print("[Overthrow] Player spawn prefab is missing OVT_PlayerWantedComponent!");
		}else{
			wanted.SetWantedLevel(0);
			// Temporarily disable wanted system to prevent immediate re-application
			wanted.DisableWantedSystem();
			// Re-enable after 5 seconds to give player time to orient themselves
			GetGame().GetCallqueue().CallLater(ReenableWantedSystem, 5000, false, wanted);
		}		
	}
	
	protected void ReenableWantedSystem(OVT_PlayerWantedComponent wanted)
	{
		if(wanted)
			wanted.EnableWantedSystem();
	}

	//------------------------------------------------------------------------------------------------
	//! Sets the main menu camera to a random predefined position.
	//! Only executes if the camera hasn't been set already.
	protected void SetRandomCameraPosition()
	{
		CameraManager cameraMgr = GetGame().GetCameraManager();
		if(!cameraMgr) return;
		
		IEntity startCameraPos = GetRandomStartCameraPosition();
		
		IEntity cam = OVT_Global.SpawnEntityPrefab(m_StartCameraPrefab, startCameraPos.GetOrigin(), "0 0 0", false);
		if(cam)
		{
			CameraBase camera = CameraBase.Cast(cam);
			camera.SetName("StartCam");
			camera.SetAngles(startCameraPos.GetAngles());
			cameraMgr.SetCamera(camera);
			m_bCameraSet = true;
			m_pCamera = camera;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Called when the entity is initialized. Initializes managers, UI contexts, persistence, and diag menus.
	//! Determines whether to start a new game, load a save, or show the start menu.
	//! \\param[in] owner The entity owning this component (the GameMode entity).
	override void EOnInit(IEntity owner) //!EntityEvent.INIT
	{
		super.EOnInit(owner);

		m_aInitializedPlayers = new set<string>;
		m_aHintedPlayers = new set<string>;

		DiagMenu.RegisterBool(250, "lctrl+lalt+g", "Give $1000", "Overthrow");
		DiagMenu.SetValue(250, 0);

		DiagMenu.RegisterBool(251, "lctrl+lalt+s", "Give 100% support", "Overthrow");
		DiagMenu.SetValue(251, 0);

		DiagMenu.RegisterBool(252, "lctrl+lalt+c", "Capture Town", "Overthrow");
		DiagMenu.SetValue(252, 0);

		DiagMenu.RegisterBool(254, "lctrl+lalt+r", "Capture Nearest Base", "Overthrow");
		DiagMenu.SetValue(254, 0);

		DiagMenu.RegisterBool(255, "lctrl+lalt+x", "Give 100 XP", "Overthrow");
		DiagMenu.SetValue(255, 0);

		if(SCR_Global.IsEditMode())
			return;

		Print("[Overthrow] Initializing Overthrow");
		
		//Find fallback spawn positions and start camera positions
		GetGame().GetWorld().QueryEntitiesBySphere("0 0 0", 99999999, FilterPositionEntities, null, EQueryEntitiesFlags.STATIC);
		
		Print(string.Format("[Overthrow] Found %1 fallback home spawns", m_aFallbackSpawnPositions.Count().ToString()));

		m_Config = OVT_Global.GetConfig();
		m_PlayerManager = OVT_PlayerManagerComponent.Cast(FindComponent(OVT_PlayerManagerComponent));
		if(m_PlayerManager)
		{
			Print("[Overthrow] Initializing Players");
			// Ensure singleton points to the actual component instance
			OVT_PlayerManagerComponent.s_Instance = m_PlayerManager;
			m_PlayerManager.Init(this);
		}
		
		
		m_RealEstate = OVT_Global.GetRealEstate();

		m_TownManager = OVT_TownManagerComponent.Cast(FindComponent(OVT_TownManagerComponent));
		if(m_TownManager)
		{
			Print("[Overthrow] Initializing Towns");

			m_TownManager.Init(this);
		}

		m_EconomyManager = OVT_EconomyManagerComponent.Cast(FindComponent(OVT_EconomyManagerComponent));
		if(m_EconomyManager)
		{
			Print("[Overthrow] Initializing Economy");
			m_EconomyManager.Init(this);
		}

		m_OccupyingFactionManager = OVT_OccupyingFactionManager.Cast(FindComponent(OVT_OccupyingFactionManager));
		if(m_OccupyingFactionManager)
		{
			Print("[Overthrow] Initializing Occupying Faction");

			m_OccupyingFactionManager.Init(this);
		}

		m_ResistanceFactionManager = OVT_ResistanceFactionManager.Cast(FindComponent(OVT_ResistanceFactionManager));
		if(m_ResistanceFactionManager)
		{
			Print("[Overthrow] Initializing Resistance Faction");

			m_ResistanceFactionManager.Init(this);
		}

		m_VehicleManager = OVT_VehicleManagerComponent.Cast(FindComponent(OVT_VehicleManagerComponent));
		if(m_VehicleManager)
		{
			Print("[Overthrow] Initializing Vehicles");

			m_VehicleManager.Init(this);
		}

		m_JobManager = OVT_JobManagerComponent.Cast(FindComponent(OVT_JobManagerComponent));
		if(m_JobManager)
		{
			Print("[Overthrow] Initializing Jobs");

			m_JobManager.Init(this);
		}

		m_SkillManager = OVT_SkillManagerComponent.Cast(FindComponent(OVT_SkillManagerComponent));
		if(m_SkillManager)
		{
			Print("[Overthrow] Initializing Skills");

			m_SkillManager.Init(this);
		}
		
		m_Deployment = OVT_DeploymentManagerComponent.Cast(FindComponent(OVT_DeploymentManagerComponent));
		if(m_Deployment)
		{
			Print("[Overthrow] Initializing Deployment");

			m_Deployment.Init(this);
		}

		if(!IsMaster()) {
			return;
		}

		// A character's persistence configuration is chosen when it starts being tracked and never
		// self-spawns while ALIVE (Overthrow rebuilds live AI itself). Corpses must self-spawn on load or
		// they vanish from the save (BUG-018), so the kill hook flips that one bit at the one moment the
		// answer changes - see OVT_PersistenceTracking.MarkForSelfSpawn(). Server only: the persistence
		// system is registered SystemLocation Server, and this event is raised on clients too.
		m_OnCharacterKilled.Insert(OnCharacterKilledPersist);

		OVT_Global.GetConfig().LoadConfig();

		//Dynamic weather enabled by default (add config for this later)
		//ChimeraWorld world = GetGame().GetWorld();
		//TimeAndWeatherManagerEntity time = world.GetTimeAndWeatherManager();
		//time.ForceWeatherTo(false, "Cloudy");

		m_Persistence = OVT_PersistenceManagerComponent.Cast(FindComponent(OVT_PersistenceManagerComponent));
		if(m_Persistence)
		{
			Print("[Overthrow] Initializing Persistence");
			// IsPlayingLoadedSave() - not HasSaveGame() - is the question that matters here: "was THIS
			// session launched from a save point?". It is synchronous and unambiguous, whereas
			// HasSaveGame() answers "does a save exist somewhere on disk", is served from an async
			// cache that has usually not resolved yet at this point in boot, and is true even when the
			// player deliberately started a brand new playthrough.
			if(m_Persistence.IsPlayingLoadedSave())
			{
				Print("[Overthrow] Session was launched from a save point, continuing the campaign");
				m_bCameraSet = true;
				m_bRequestStartOnPostProcess = true;
			}else{
				Print("[Overthrow] Not launched from a save point");
				if(RplSession.Mode() == RplMode.Dedicated)
				{
					// Whether a campaign already exists is unanswerable right now: HasSaveGame()
					// is an async cache that has not seeded yet. Starting a new game regardless
					// (the old behaviour) generated a fresh campaign OVER an existing one and the
					// next autosave buried it. Defer the decision until the save scan answers.
					Print("[Overthrow] Dedicated server, waiting for the save scan before starting");
					GetGame().GetCallqueue().CallLater(DecideDedicatedStart, 250, false, 0);
				}else{
					Print("[Overthrow] Will show start menu when player is ready");
					// Start menu (or the continue of an existing campaign) is driven by
					// OVT_PlayerStartMenuHandlerComponent once the local player is ready
				}
			}
		}

		m_PerceivedFactionManager = SCR_PerceivedFactionManagerComponent.Cast(FindComponent(SCR_PerceivedFactionManagerComponent));
		if(m_PerceivedFactionManager)
		{
			Print("[Overthrow] Initializing Perceived Faction Manager");
		}
	}
	
	bool FilterPositionEntities(IEntity entity)
	{
		OVT_FallbackHomePos pos = OVT_FallbackHomePos.Cast(entity);
		if(pos)
		{
			m_aFallbackSpawnPositions.Insert(entity);
		}else{
			OVT_StartCameraPos cameraPos = OVT_StartCameraPos.Cast(entity);
			if(cameraPos)
			{
				m_aStartCameraPositions.Insert(entity);
			}
		}

		return true;
	}
		
	//------------------------------------------------------------------------------------------------
	//! Client-side callback when a player spawns (currently empty).
	//! \\param[in] entity The spawned player entity.
	protected void OnPlayerSpawnClient(IEntity entity)
	{
		
	}

	//------------------------------------------------------------------------------------------------
	//! Called after the world finishes its post-processing phase.
	//! Schedules post-load logic and potential game start calls.
	//! \\param[in] world The game world.
	override event void OnWorldPostProcess(World world)
	{
		Print("[Overthrow] World Post Processing complete..");
		super.OnWorldPostProcess(world);
		SCR_FuelConsumptionComponent.SetGlobalFuelConsumptionScale(1.0);//Chris - Changed Global Fuel Consumption to 1
		m_bWorldPostProcessed = true;
		GetGame().GetCallqueue().CallLater(DoPostLoad);
		if(m_bRequestStartOnPostProcess)
		{
			ScheduleStartGame();
		}
	};

	//------------------------------------------------------------------------------------------------
	//! Called locally when the local player spawns. Shows an introductory hint if not already shown.
	//! \\param[in] playerId The persistent ID of the local player.
	void OnPlayerSpawnedLocal(string playerId)
	{
		// Only show the hint if the game has actually started (user clicked "Start Game")
		// Don't show it while they're still in the start menu
		if (!m_bGameStarted)
			return;

		if(!m_aHintedPlayers.Contains(playerId))
		{
			SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-IntroHint","#OVT-Overthrow",20);
			m_aHintedPlayers.Insert(playerId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Constructor for OVT_OverthrowGameMode. Initializes player group map.
	//! \\param[in] src Entity source information.
	//! \\param[in] parent Parent entity.
	void OVT_OverthrowGameMode(IEntitySource src, IEntity parent)
	{		
		m_mPlayerGroups = new map<string, EntityID>;
	}

}
