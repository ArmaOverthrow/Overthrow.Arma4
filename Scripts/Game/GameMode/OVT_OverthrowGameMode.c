class OVT_OverthrowGameModeClass: SCR_BaseGameModeClass
{
};

class OVT_OverthrowGameMode : SCR_BaseGameMode
{
	[Attribute()]
	ref OVT_UIContext m_StartGameUIContext;

	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Start Camera Prefab", params: "et")]
	ResourceName m_StartCameraPrefab;

	protected OVT_OverthrowConfigComponent m_Config;
	protected OVT_TownManagerComponent m_TownManager;
	protected OVT_OccupyingFactionManager m_OccupyingFactionManager;
	protected OVT_ResistanceFactionManager m_ResistanceFactionManager;
	protected OVT_RealEstateManagerComponent m_RealEstate;
	protected OVT_VehicleManagerComponent m_VehicleManager;
	protected OVT_EconomyManagerComponent m_EconomyManager;
	protected OVT_PlayerManagerComponent m_PlayerManager;
	protected OVT_JobManagerComponent m_JobManager;
	protected OVT_SkillManagerComponent m_SkillManager;
	protected OVT_PersistenceManagerComponent m_Persistence;

	protected CameraBase m_pCamera;

	ref set<string> m_aInitializedPlayers;
	ref set<string> m_aHintedPlayers;

	ref map<string, EntityID> m_mPlayerGroups;

	protected bool m_bGameInitialized = false;
	protected bool m_bCameraSet = false;
	protected bool m_bGameStarted = false;
	protected bool m_bRequestStartOnPostProcess = false;

	bool m_bHasOpenedMenu = false;

	bool IsInitialized()
	{
		return m_bGameInitialized;
	}

	void DoStartNewGame()
	{
		bool isDedicated = RplSession.Mode() == RplMode.Dedicated;
#ifdef WORKBENCH
		isDedicated = true; //To test dedicated server config
#endif
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		m_Config = config;
		
		if(isDedicated)
		{
			if(config.m_ConfigFile && config.m_ConfigFile.occupyingFaction != "" && config.m_ConfigFile.occupyingFaction != "FIA")
			{
				Print("Overthrow: Setting occupying faction to config value (" + config.m_ConfigFile.occupyingFaction + ")");
				config.SetOccupyingFaction(config.m_ConfigFile.occupyingFaction);
			}else{
				Print("Overthrow: Setting occupying faction to default (" + config.m_sDefaultOccupyingFaction + ")");
				config.SetOccupyingFaction(config.m_sDefaultOccupyingFaction);
			}
		}
		m_Config.SetBaseAndTownOwners();

		if(m_OccupyingFactionManager)
		{
			Print("Starting New Occupying Faction");

			m_OccupyingFactionManager.NewGameStart();
		}
	}

	OVT_PersistenceManagerComponent GetPersistence()
	{
		return m_Persistence;
	}

	void DoStartGame()
	{
		FactionManager fm = GetGame().GetFactionManager();
		OVT_Global.GetConfig().m_iPlayerFactionIndex = fm.GetFactionIndex(fm.GetFactionByKey(OVT_Global.GetConfig().m_sPlayerFaction));
		OVT_Global.GetConfig().m_iOccupyingFactionIndex = fm.GetFactionIndex(fm.GetFactionByKey(OVT_Global.GetConfig().m_sOccupyingFaction));

		m_StartGameUIContext.CloseLayout();
		m_bGameStarted = true;

		if(!OVT_Global.GetConfig().m_Difficulty)
		{
			Print("No difficulty settings found! Reverting to default");
			OVT_Global.GetConfig().m_Difficulty = new OVT_DifficultySettings();
		}

		if(m_EconomyManager)
		{
			Print("Starting Economy");

			m_EconomyManager.PostGameStart();
		}

		if(m_TownManager)
		{
			Print("Starting Towns");

			m_TownManager.PostGameStart();
		}

		if(m_OccupyingFactionManager)
		{
			Print("Starting Occupying Faction");

			m_OccupyingFactionManager.PostGameStart();
		}

		if(m_ResistanceFactionManager)
		{
			Print("Starting Resistance Faction");

			m_ResistanceFactionManager.PostGameStart();
		}

		if(m_JobManager)
		{
			Print("Starting Jobs");

			m_JobManager.PostGameStart();
		}

		if(m_SkillManager)
		{
			Print("Starting Skills");

			m_SkillManager.PostGameStart();
		}

		Print("Overthrow Starting");
		m_bGameInitialized = true;
	}

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

		if(!(IsMaster() && RplSession.Mode() == RplMode.Dedicated) && !m_bCameraSet)
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

	void PreShutdownPersist()
	{

	}

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

	void PreparePlayer(int playerId, string persistentId)
	{
		if(!Replication.IsServer()) return;

		m_PlayerManager.SetupPlayer(playerId, persistentId);
		OVT_PlayerData player = m_PlayerManager.GetPlayer(persistentId);

		if(!player.isOfficer && RplSession.Mode() == RplMode.None)
		{
			//In single player, make the player an officer
			m_ResistanceFactionManager.AddOfficer(playerId);
		}

#ifdef WORKBENCH
		if(!player.isOfficer && playerId == 1)
		{
			//In workbench, make the first player an officer
			m_ResistanceFactionManager.AddOfficer(playerId);
		}
#endif

		if(player.initialized)
		{
			//Existing player
			if(m_aInitializedPlayers.Contains(persistentId))
			{
				int cost = OVT_Global.GetConfig().m_Difficulty.respawnCost;
				m_EconomyManager.TakePlayerMoney(playerId, cost);
			}else{
				Print("Preparing returning player: " + persistentId);
				//This is a returning player, don't charge them hospital fees
				m_aInitializedPlayers.Insert(persistentId);
			}
			player.firstSpawn = false;
		}else{
			//New player
			Print("Preparing NEW player: " + persistentId);
			int cash = OVT_Global.GetConfig().m_Difficulty.startingCash;
			m_EconomyManager.AddPlayerMoney(playerId, cash);

			vector home = m_RealEstate.GetHome(persistentId);
			if(home[0] == 0)
			{

				IEntity house = OVT_Global.GetTowns().GetRandomStartingHouse();
				if(!house)
				{
					//No houses left on the map, spawn in a town
					int index = s_AIRandomGenerator.RandInt(0, m_TownManager.m_Towns.Count()-1);
					OVT_TownData town = m_TownManager.m_Towns[index];
					m_RealEstate.SetHomePos(playerId, town.location);
				}else{
					m_RealEstate.SetOwner(playerId, house);
					m_RealEstate.SetHome(playerId, house);

					m_VehicleManager.SpawnStartingCar(house, persistentId);
				}
			}
			player.initialized = true;
			player.firstSpawn = true;
			m_aInitializedPlayers.Insert(persistentId);
		}
	}

	protected override void OnPlayerSpawned(int playerId, IEntity controlledEntity)
	{
		OVT_PlayerWantedComponent wanted = OVT_PlayerWantedComponent.Cast(controlledEntity.FindComponent(OVT_PlayerWantedComponent));
		if(!wanted){
			Print("Player spawn prefab is missing OVT_PlayerWantedComponent!");
		}else{
			wanted.SetWantedLevel(0);
		}
	}

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

	override void EOnInit(IEntity owner) //!EntityEvent.INIT
	{
		super.EOnInit(owner);

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

		Print("Initializing Overthrow");

		OVT_Global.GetConfig() = OVT_Global.GetConfig();
		m_PlayerManager = OVT_Global.GetPlayers();
		m_RealEstate = OVT_Global.GetRealEstate();

		m_TownManager = OVT_TownManagerComponent.Cast(FindComponent(OVT_TownManagerComponent));
		if(m_TownManager)
		{
			Print("Initializing Towns");

			m_TownManager.Init(this);
		}

		m_EconomyManager = OVT_EconomyManagerComponent.Cast(FindComponent(OVT_EconomyManagerComponent));
		if(m_EconomyManager)
		{
			Print("Initializing Economy");
			m_EconomyManager.Init(this);
		}

		m_OccupyingFactionManager = OVT_OccupyingFactionManager.Cast(FindComponent(OVT_OccupyingFactionManager));
		if(m_OccupyingFactionManager)
		{
			Print("Initializing Occupying Faction");

			m_OccupyingFactionManager.Init(this);
		}

		m_ResistanceFactionManager = OVT_ResistanceFactionManager.Cast(FindComponent(OVT_ResistanceFactionManager));
		if(m_ResistanceFactionManager)
		{
			Print("Initializing Resistance Faction");

			m_ResistanceFactionManager.Init(this);
		}

		m_VehicleManager = OVT_VehicleManagerComponent.Cast(FindComponent(OVT_VehicleManagerComponent));
		if(m_VehicleManager)
		{
			Print("Initializing Vehicles");

			m_VehicleManager.Init(this);
		}

		m_JobManager = OVT_JobManagerComponent.Cast(FindComponent(OVT_JobManagerComponent));
		if(m_JobManager)
		{
			Print("Initializing Jobs");

			m_JobManager.Init(this);
		}

		m_SkillManager = OVT_SkillManagerComponent.Cast(FindComponent(OVT_SkillManagerComponent));
		if(m_SkillManager)
		{
			Print("Initializing Skills");

			m_SkillManager.Init(this);
		}

		m_StartGameUIContext.Init(owner, null);
		m_StartGameUIContext.RegisterInputs();

		if(!IsMaster()) {
			//show wait screen?
			return;
		}

		OVT_Global.GetConfig().LoadConfig();

		//Default weather (add config for this later)
		ChimeraWorld world = GetGame().GetWorld();
		TimeAndWeatherManagerEntity time = world.GetTimeAndWeatherManager();
		time.ForceWeatherTo(false, "Cloudy");

		m_Persistence = OVT_PersistenceManagerComponent.Cast(FindComponent(OVT_PersistenceManagerComponent));
		if(m_Persistence)
		{
			Print("Initializing Persistence");
			if(m_Persistence.HasSaveGame())
			{
				m_bCameraSet = true;
				m_bRequestStartOnPostProcess = true;
			}else{
				Print("No save game detected");
				if(RplSession.Mode() == RplMode.Dedicated)
				{
					Print("Dedicated server, starting new game");
					DoStartNewGame();
					m_bRequestStartOnPostProcess = true;
				}else{
					m_StartGameUIContext.ShowLayout();
				}
			}
		}
	}

	override event void OnWorldPostProcess(World world)
	{
		Print("World Post Processing complete..");
		super.OnWorldPostProcess(world);
		if(m_bRequestStartOnPostProcess)
		{
			GetGame().GetCallqueue().CallLater(DoStartGame);
		}
	};

	void OnPlayerSpawnedLocal(string playerId)
	{
		if(!m_aHintedPlayers.Contains(playerId))
		{
			SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-IntroHint","#OVT-Overthrow",20);
			m_aHintedPlayers.Insert(playerId);
		}
	}

	//------------------------------------------------------------------------------------------------
	void OVT_OverthrowGameMode(IEntitySource src, IEntity parent)
	{
		m_aInitializedPlayers = new set<string>;
		m_aHintedPlayers = new set<string>;
		m_mPlayerGroups = new map<string, EntityID>;
	}

	void ~OVT_OverthrowGameMode()
	{
		m_aInitializedPlayers.Clear();
		m_aHintedPlayers.Clear();
		m_mPlayerGroups.Clear();
	}
}
