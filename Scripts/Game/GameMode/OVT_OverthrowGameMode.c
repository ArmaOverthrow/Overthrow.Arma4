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
			string playerUid = EPF_Utils.GetPlayerUID(playerId);

			if (!playerUid || playerUid.IsEmpty())
			{
				Print("[Overthrow] WARNING: Skipping player " + playerId + " - no persistent UID available yet", LogLevel.WARNING);
				continue;
			}

			// Check if player has already been finalized
			if (m_aInitializedPlayers.Contains(playerUid))
			{
				Print("[Overthrow] Player " + playerUid + " (ID: " + playerId + ") already finalized, skipping");
				continue;
			}

			Print("[Overthrow] Finalizing player preparation: " + playerUid + " (ID: " + playerId + ")");
			// SetupPlayer was already called in DoSpawn_S, so just finalize (home, officer, etc.)
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
	//! Persists necessary data before the game mode shuts down.
	//! (Currently empty)
	void PreShutdownPersist()
	{

	}
	
	
	//------------------------------------------------------------------------------------------------
	//! Selects a random fallback location from the detected list.
	//! \\return A random vector position from m_aFallbackSpawnPositions, or vector.Zero if the list is empty.
	protected vector GetRandomFallbackPosition()
	{
	    if (m_aFallbackSpawnPositions.Count() > 0)
	    {
	        int randomIndex = s_AIRandomGenerator.RandInt(0, m_aFallbackSpawnPositions.Count() - 1);
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
	        int randomIndex = s_AIRandomGenerator.RandInt(0, m_aStartCameraPositions.Count() - 1);
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
	//! Handles player disconnection. Pauses persistence tracking and removes the player from the initialized list.
	//! \\param[in] playerId The ID of the disconnecting player.
	//! \\param[in] cause The reason for disconnection.
	//! \\param[in] timeout The disconnection timeout duration.
	protected override void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
	{
		string persId = m_PlayerManager.GetPersistentIDFromPlayerID(playerId);
		IEntity controlledEntity = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);

		if(controlledEntity)
		{
			OVT_PlayerData player = m_PlayerManager.GetPlayer(persId);
			if(player)
			{
				player.id = -1;
			}

			EPF_PersistenceComponent persistence = EPF_PersistenceComponent.Cast(controlledEntity.FindComponent(EPF_PersistenceComponent));
			if(persistence)
			{
				persistence.PauseTracking();
				persistence.Save();
			}
		}

		int i = m_aInitializedPlayers.Find(persId);

		if(i > -1)
			m_aInitializedPlayers.Remove(i);
		
		// Notify listeners that player has disconnected
		m_PlayerManager.m_OnPlayerDisconnected.Invoke(persId, playerId);

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

		OVT_Global.GetConfig() = OVT_Global.GetConfig();
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

		OVT_Global.GetConfig().LoadConfig();

		//Dynamic weather enabled by default (add config for this later)
		//ChimeraWorld world = GetGame().GetWorld();
		//TimeAndWeatherManagerEntity time = world.GetTimeAndWeatherManager();
		//time.ForceWeatherTo(false, "Cloudy");

		m_Persistence = OVT_PersistenceManagerComponent.Cast(FindComponent(OVT_PersistenceManagerComponent));
		if(m_Persistence)
		{
			Print("[Overthrow] Initializing Persistence");
			if(m_Persistence.HasSaveGame())
			{
				m_bCameraSet = true;
				m_bRequestStartOnPostProcess = true;
			}else{
				Print("[Overthrow] No save game detected");
				if(RplSession.Mode() == RplMode.Dedicated)
				{
					Print("[Overthrow] Dedicated server, starting new game");
					DoStartNewGame();
					m_bRequestStartOnPostProcess = true;
				}else{
					Print("[Overthrow] Will show start menu when player is ready");
					// Start menu will be shown on client when NotifyReadyForSpawn is received
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
		GetGame().GetCallqueue().CallLater(DoPostLoad);
		if(m_bRequestStartOnPostProcess)
		{
			GetGame().GetCallqueue().CallLater(DoStartGame);
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
