class OVT_OverthrowGameModeClass: SCR_BaseGameModeClass
{
};

//------------------------------------------------------------------------------------------------
//! Main game mode logic for Overthrow.
//! Handles game initialization, player management, component lifecycle, and core game flow.
class OVT_OverthrowGameMode : SCR_BaseGameMode
{
	//! UI Context for the start game menu.
	[Attribute()]
	ref OVT_UIContext m_StartGameUIContext;

	//! Prefab resource for the camera used for the single player menu at the start of the game.
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Start Camera Prefab", params: "et")]
	ResourceName m_StartCameraPrefab;

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

	//------------------------------------------------------------------------------------------------
	//! Checks if the game mode has completed its initialization process.
	//! \\return True if the game is initialized, false otherwise.
	bool IsInitialized()
	{
		return m_bGameInitialized;
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
					Print("[Overthrow] Overthrow: Setting occupying faction to config value (" + config.m_ConfigFile.occupyingFaction + ")");
					config.SetOccupyingFaction(config.m_ConfigFile.occupyingFaction);
				}else{
					Print("[Overthrow] Overthrow: Setting occupying faction to default (" + config.m_sDefaultOccupyingFaction + ")");
					config.SetOccupyingFaction(config.m_sDefaultOccupyingFaction);
				}
				
				if(config.m_ConfigFile.supportingFaction != "" && config.m_ConfigFile.supportingFaction != "FIA")
				{
					Print("[Overthrow] Overthrow: Setting supporting faction to config value (" + config.m_ConfigFile.supportingFaction + ")");
					config.SetSupportingFaction(config.m_ConfigFile.supportingFaction);
				}else{
					Print("[Overthrow] Overthrow: Setting supporting faction to default (" + config.m_sDefaultSupportingFaction + ")");
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
	//! Finalizes game startup, initializes various managers, and sets difficulty.
	//! Closes the start game UI and transitions into the active game state.
	void DoStartGame()
	{
		FactionManager fm = GetGame().GetFactionManager();
		OVT_Global.GetConfig().m_iPlayerFactionIndex = fm.GetFactionIndex(fm.GetFactionByKey(OVT_Global.GetConfig().m_sPlayerFaction));
		OVT_Global.GetConfig().m_iSupportingFactionIndex = fm.GetFactionIndex(fm.GetFactionByKey(OVT_Global.GetConfig().m_sSupportingFaction));
		OVT_Global.GetConfig().m_iOccupyingFactionIndex = fm.GetFactionIndex(fm.GetFactionByKey(OVT_Global.GetConfig().m_sOccupyingFaction));

		m_StartGameUIContext.CloseLayout();
		m_bGameStarted = true;

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
		
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if(config.m_ConfigFile)
		{			
			if(config.m_ConfigFile.difficulty != "")
			{
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
				config.m_Difficulty.gunDealerSellPriceMultiplier = config.m_ConfigFile.gunDealerSellPriceMultiplier;
				config.m_Difficulty.startingCash = config.m_ConfigFile.startingCash;
				config.m_Difficulty.procurementMultiplier = config.m_ConfigFile.procurementMultiplier;
			}
		}

		// Save config with user selections after game start
		if(RplSession.Mode() == RplMode.None || RplSession.Mode() == RplMode.Listen)
		{
			// Update config with current faction selections
			if(config.m_ConfigFile)
			{
				config.m_ConfigFile.occupyingFaction = config.m_sOccupyingFaction;
				config.m_ConfigFile.supportingFaction = config.m_sSupportingFaction;
			}
			config.SaveConfig();
			Print("[Overthrow] Configuration saved with user selections");
		}

		Print("[Overthrow] Overthrow Starting");
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
	//! Called every frame. Handles debug commands and manages the start camera lifecycle.
	//! \\param[in] owner The entity owning this component (the GameMode entity).
	//! \\param[in] timeSlice The time elapsed since the last frame.
	override void EOnFrame(IEntity owner, float timeSlice)
	{
		super.EOnFrame(owner, timeSlice);

		if(DiagMenu.GetValue(200))
		{
			m_EconomyManager.DoAddPlayerMoney(SCR_PlayerController.GetLocalPlayerId(),1000);
			DiagMenu.SetValue(200,0);
		}

		if(DiagMenu.GetValue(201))
		{
			OVT_TownData town = OVT_Global.GetTowns().GetNearestTown(SCR_PlayerController.GetLocalControlledEntity().GetOrigin());
			if(town)
			{
				town.support = town.population;
			}
			DiagMenu.SetValue(201,0);
		}

		if(DiagMenu.GetValue(202))
		{
			OVT_TownData town = OVT_Global.GetTowns().GetNearestTown(SCR_PlayerController.GetLocalControlledEntity().GetOrigin());
			if(town)
			{
				OVT_Global.GetTowns().ChangeTownControl(town, OVT_Global.GetConfig().GetPlayerFactionIndex());
			}
			DiagMenu.SetValue(202,0);
		}

		if(DiagMenu.GetValue(203))
		{
			OVT_Global.GetOccupyingFaction().WinBattle();
			DiagMenu.SetValue(203,0);
		}

		if(DiagMenu.GetValue(204))
		{
			foreach(OVT_TownData town : m_TownManager.m_Towns)
			{
				int townID = OVT_Global.GetTowns().GetTownID(town);
				m_TownManager.TryAddSupportModifierByName(townID, "RecruitmentPosters");
				m_TownManager.TryAddSupportModifierByName(townID, "RecruitmentPosters");
				m_TownManager.TryAddSupportModifierByName(townID, "RecruitmentPosters");
				m_TownManager.TryAddSupportModifierByName(townID, "RecruitmentPosters");
				m_TownManager.TryAddSupportModifierByName(townID, "RecruitmentPosters");
			}
			DiagMenu.SetValue(204,0);
		}

		if(DiagMenu.GetValue(205))
		{
			OVT_Global.GetSkills().GiveXP(SCR_PlayerController.GetLocalPlayerId(),100);
			DiagMenu.SetValue(205,0);
		}

		if((IsMaster() && (RplSession.Mode() == RplMode.None || RplSession.Mode() == RplMode.Listen)) && !m_bCameraSet)
		{
			SetRandomCameraPosition();
		}

		if(m_bGameInitialized)
		{
			if(m_pCamera)
			{
				SCR_EntityHelper.DeleteEntityAndChildren(m_pCamera);
				m_pCamera = null;
			}
		}
		m_StartGameUIContext.EOnFrame(owner, timeSlice);
	}

	//------------------------------------------------------------------------------------------------
	//! Persists necessary data before the game mode shuts down.
	//! (Currently empty)
	void PreShutdownPersist()
	{

	}
	
	//! Array to store references to bus stop entities (currently unused).
	protected ref array<IEntity> m_BusStops = {};
		
	//! Predefined locations for potential bus stop spawns.
	protected ref array<vector> m_HardcodedBusStopLocations = {
	    "4413.912 12.042 10687.312", 
	    "4824.945 169.693 6968.863", 
	    "9706.01 12.132 1565.451"
	};
	
	//------------------------------------------------------------------------------------------------
	//! Selects a random bus stop location from the hardcoded list.
	//! \\return A random vector position from m_HardcodedBusStopLocations, or vector.Zero if the list is empty.
	protected vector GetRandomHardcodedBusStop()
	{
	    if (m_HardcodedBusStopLocations.Count() > 0)
	    {
	        int randomIndex = s_AIRandomGenerator.RandInt(0, m_HardcodedBusStopLocations.Count() - 1);
	        return m_HardcodedBusStopLocations[randomIndex];
	    }
	
	    return vector.Zero; // No hardcoded bus stops found
	}
	 vector spawnLocation
	
	//------------------------------------------------------------------------------------------------
	//! Spawns a player at a randomly selected hardcoded bus stop location.
	//! If no hardcoded locations exist, attempts to spawn at the first available town center.
	//! \\param[in] playerId The ID of the player to spawn.
	void SpawnPlayerAtBusStop(int playerId)
	{
			spawnLocation = GetRandomHardcodedBusStop();
			if (spawnLocation != vector.Zero)
			{
			    Print("[Overthrow] Spawning player at hard Coded bus stop: " + spawnLocation.ToString());
			    m_RealEstate.SetHomePos(playerId, spawnLocation);
			}
			else
			 {
		       	Print("[Overthrow] No bus stops found. Using fallback.");
		        foreach (OVT_TownData town : m_TownManager.m_Towns)
		        {
		            if (town)
		            {
		                m_RealEstate.SetHomePos(playerId, town.location);
		                break;
		            }
		        }
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
	    
	    // Check if this player has already been prepared in this session (to prevent duplicates in hosted multiplayer)
	    if(m_aInitializedPlayers.Contains(persistentId))
	    {
	        Print("[Overthrow] Player " + persistentId + " already prepared in this session, skipping duplicate PreparePlayer call");
	        return;
	    }
	    
	    m_PlayerManager.SetupPlayer(playerId, persistentId);
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
	            SpawnPlayerAtBusStop(playerId);
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
		}		
	}

	//------------------------------------------------------------------------------------------------
	//! Sets the main menu camera to a random predefined position.
	//! Only executes if the camera hasn't been set already.
	protected void SetRandomCameraPosition()
	{
		CameraManager cameraMgr = GetGame().GetCameraManager();
		if(!cameraMgr) return;
		BaseWorld world = GetGame().GetWorld();

		int cameraIndex = s_AIRandomGenerator.RandInt(0, OVT_Global.GetConfig().m_aCameraPositions.Count()-1);
		OVT_CameraPosition pos = OVT_Global.GetConfig().m_aCameraPositions[cameraIndex];

		IEntity cam = OVT_Global.SpawnEntityPrefab(m_StartCameraPrefab, pos.position, "0 0 0", false);
		if(cam)
		{
			CameraBase camera = CameraBase.Cast(cam);
			camera.SetName("StartCam");
			camera.SetAngles(pos.angles);
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

		DiagMenu.RegisterBool(200, "lctrl+lalt+g", "Give $1000", "Cheats");
		DiagMenu.SetValue(200, 0);

		DiagMenu.RegisterBool(201, "lctrl+lalt+s", "Give 100% support", "Cheats");
		DiagMenu.SetValue(201, 0);

		DiagMenu.RegisterBool(202, "lctrl+lalt+c", "Capture Town", "Cheats");
		DiagMenu.SetValue(202, 0);

		DiagMenu.RegisterBool(203, "lctrl+lalt+w", "Win Battle", "Cheats");
		DiagMenu.SetValue(203, 0);

		DiagMenu.RegisterBool(204, "lctrl+lalt+r", "Poster all towns", "Cheats");
		DiagMenu.SetValue(204, 0);

		DiagMenu.RegisterBool(205, "lctrl+lalt+x", "Give 100 XP", "Cheats");
		DiagMenu.SetValue(205, 0);

		if(SCR_Global.IsEditMode())
			return;

		Print("[Overthrow] Initializing Overthrow");

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

		m_StartGameUIContext.Init(owner, null);
		m_StartGameUIContext.RegisterInputs();

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
					m_StartGameUIContext.ShowLayout();
				}
			}
		}
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
