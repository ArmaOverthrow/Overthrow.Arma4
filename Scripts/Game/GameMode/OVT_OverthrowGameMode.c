class OVT_OverthrowGameModeClass: SCR_BaseGameModeClass
{
};

class OVT_OverthrowGameMode : SCR_BaseGameMode
{
	[Attribute()]
	ref OVT_UIContext m_StartGameUIContext;
	
	[Attribute()]
	ResourceName m_PlayerCommsPrefab;
	
	protected OVT_OverthrowConfigComponent m_Config;
	protected OVT_TownManagerComponent m_TownManager;
	protected OVT_OccupyingFactionManager m_OccupyingFactionManager;
	protected OVT_ResistanceFactionManager m_ResistanceFactionManager;
	protected OVT_RealEstateManagerComponent m_RealEstate;
	protected OVT_VehicleManagerComponent m_VehicleManager;
	protected OVT_EconomyManagerComponent m_EconomyManager;
	protected OVT_PlayerManagerComponent m_PlayerManager;
	protected OVT_JobManagerComponent m_JobManager;
	
	OVT_PlayerCommsEntity m_Server;
	
	ref set<string> m_aInitializedPlayers;
	ref set<string> m_aHintedPlayers;
	
	ref map<string, EntityID> m_mPlayerGroups;
	
	protected bool m_bGameInitialized = false;
			
	bool IsInitialized()
	{
		return m_bGameInitialized;
	}
	
	void DoStartNewGame()
	{
		if(m_OccupyingFactionManager)
		{
			Print("Starting Occupying Faction");
			
			m_OccupyingFactionManager.NewGameStart();
		}	
	}
	
	void DoStartGame()
	{
		m_StartGameUIContext.CloseLayout();
		
		if(RplSession.Mode() == RplMode.Dedicated)
		{
			Print("Spawning comms entity for dedicated server");					
			IEntity entity = GetGame().SpawnEntityPrefab(Resource.Load(m_PlayerCommsPrefab), GetGame().GetWorld());
			m_Server = OVT_PlayerCommsEntity.Cast(entity);
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
		
		if(m_EconomyManager)
		{
			Print("Starting Jobs");
			
			m_JobManager.PostGameStart();
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
		
		if(m_bGameInitialized) return;
		m_StartGameUIContext.EOnFrame(owner, timeSlice);
	}
	
	protected override void OnPlayerRegistered(int playerId)
	{
		super.OnPlayerRegistered(playerId);
		if(!Replication.IsServer()) return;
		
		PlayerController playerController = GetGame().GetPlayerManager().GetPlayerController(playerId);
		if(!playerController) return;
		
		Print("Spawning player comms entity for player " + playerId);
		RplIdentity playerRplID = playerController.GetRplIdentity();		
		IEntity entity = GetGame().SpawnEntityPrefab(Resource.Load(m_PlayerCommsPrefab), GetGame().GetWorld());
		
		RplComponent rpl = RplComponent.Cast(entity.FindComponent(RplComponent));
		
		Print("Assigning comms to player " + playerId);
		rpl.Give(playerRplID);
	}
	
	protected override void OnPlayerDisconnected(int playerId, KickCauseCode cause, int timeout)
	{
		string persId = m_PlayerManager.GetPersistentIDFromPlayerID(playerId);
		IEntity controlledEntity = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		
		if(controlledEntity)
		{
			m_ResistanceFactionManager.m_mPlayerPositions[persId] = controlledEntity.GetOrigin();
		}
		
		m_aInitializedPlayers.Remove(m_aInitializedPlayers.Find(persId));
		
		super.OnPlayerDisconnected(playerId, cause, timeout);
	}
	
	protected void OnPlayerIDRegistered(int playerId, string persistentId)
	{
		IEntity controlledEntity = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if(m_ResistanceFactionManager.m_Officers.Count() == 0)
		{
			m_ResistanceFactionManager.AddOfficer(playerId);
		}
				
		if(m_EconomyManager.m_mMoney.Contains(persistentId))
		{
			Print("Player exists, respawn");
			
			//Existing player	
			if(m_aInitializedPlayers.Contains(persistentId))
			{
				int cost = m_Config.m_Difficulty.respawnCost;
				m_EconomyManager.TakePlayerMoney(playerId, cost);
			}else{
				m_aInitializedPlayers.Insert(persistentId);
				
				//Make sure player is at home or last position (when loading save)
				vector home = m_RealEstate.GetHome(persistentId);
				if(m_ResistanceFactionManager.m_mPlayerPositions.Contains(persistentId))
				{
					home = m_ResistanceFactionManager.m_mPlayerPositions[persistentId];
				}
				float dist = vector.Distance(controlledEntity.GetOrigin(), home);				
				if(dist > 5)
				{
					m_PlayerManager.TeleportPlayer(playerId, home);
				}
			}
		}else{
			//New player
			Print("Adding start cash to player " + playerId);
			int cash = m_Config.m_Difficulty.startingCash;
			m_EconomyManager.AddPlayerMoney(playerId, cash);
			
			vector home = m_RealEstate.GetHome(persistentId);
			if(home[0] == 0)
			{
				Print("Adding home to player " + playerId);
				//spawn system already assigned them a house, so make nearest house their home
				IEntity house = m_TownManager.GetNearestStartingHouse(controlledEntity.GetOrigin());
				m_RealEstate.SetOwner(playerId, house);
				m_RealEstate.SetHome(playerId, house);
				
				Print("Spawning car for player " + playerId);
				m_VehicleManager.SpawnStartingCar(house, persistentId);
			}
			
			m_aInitializedPlayers.Insert(persistentId);
		}
		
		if(!m_mPlayerGroups.Contains(persistentId))
		{
			//Spawn player group
			EntitySpawnParams spawnParams = new EntitySpawnParams;
			spawnParams.TransformMode = ETransformMode.WORLD;		
			spawnParams.Transform[3] = controlledEntity.GetOrigin();
			IEntity entity = GetGame().SpawnEntityPrefab(Resource.Load(m_Config.m_pPlayerGroupPrefab), GetGame().GetWorld(), spawnParams);
			m_mPlayerGroups[persistentId] = entity.GetID();
		}
		SCR_AIGroup group = SCR_AIGroup.Cast(GetGame().GetWorld().FindEntityByID(m_mPlayerGroups[persistentId]));
		if(group)
		{			
			group.AddPlayer(playerId);
		}		
		
		OVT_PlayerWantedComponent wanted = OVT_PlayerWantedComponent.Cast(controlledEntity.FindComponent(OVT_PlayerWantedComponent));
		if(!wanted){
			Print("Player spawn prefab is missing OVT_PlayerWantedComponent!");
		}else{
			wanted.SetWantedLevel(0);
		}
	}
	
	override void EOnInit(IEntity owner) //!EntityEvent.INIT
	{
		super.EOnInit(owner);
		
		DiagMenu.RegisterBool(200, "lctrl+lalt+g", "Give $1000", "Cheats");
		DiagMenu.SetValue(200, 0);
		
		if(SCR_Global.IsEditMode())
			return;		
				
		Print("Initializing Overthrow");
		
		m_Config = OVT_Global.GetConfig();				
		m_PlayerManager = OVT_Global.GetPlayers();		
		m_RealEstate = OVT_Global.GetRealEstate();
		
		m_EconomyManager = OVT_EconomyManagerComponent.Cast(FindComponent(OVT_EconomyManagerComponent));		
		if(m_EconomyManager)
		{
			Print("Initializing Economy");
			m_EconomyManager.Init(this);
		}
		
		m_TownManager = OVT_TownManagerComponent.Cast(FindComponent(OVT_TownManagerComponent));		
		if(m_TownManager)
		{
			Print("Initializing Towns");
			
			m_TownManager.Init(this);
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
		
		m_StartGameUIContext.Init(owner, null);
		m_StartGameUIContext.RegisterInputs();	
		
		if(!IsMaster()) {
			//show wait screen?
			return;
		}
		
		GetGame().GetTimeAndWeatherManager().SetDayDuration(86400 / m_Config.m_iTimeMultiplier);
		m_PlayerManager.m_OnPlayerRegistered.Insert(OnPlayerIDRegistered);
		
		//Are we a dedicated server?
		if(RplSession.Mode() == RplMode.Dedicated)
		{
			//need to tell someone to open start game menu
			//RemoteStartGame();
			GetGame().GetCallqueue().CallLater(DoStartGame, 0);			
		}else{
			m_StartGameUIContext.ShowLayout();
		}
	}
	
	void OnPlayerSpawnedLocal(string playerId)
	{
		if(!m_aHintedPlayers.Contains(playerId))
		{
			SCR_HintManagerComponent.GetInstance().ShowCustom("#OVT-IntroHint","#OVT-Overthrow",20);
			m_aHintedPlayers.Insert(playerId);
		}		
	}
	
	protected void RemoteStartGame()
	{
		//any players online yet?
		array<int> players = new array<int>;
		PlayerManager mgr = GetGame().GetPlayerManager();
		int numplayers = mgr.GetPlayers(players);
		
		if(numplayers > 0)
		{
			//tell the first player to start the game (not working atm?)
			Rpc(RpcDo_ShowStartGame, players[0]); 			
		}else{
			//try again in a couple seconds
			GetGame().GetCallqueue().CallLater(RemoteStartGame, 2000);
		}
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_ShowStartGame(int playerId)
	{
		int localId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(SCR_PlayerController.GetLocalControlledEntity());
		if(playerId != localId) return;
		
		m_StartGameUIContext.ShowLayout();
	}
	
	void StartNewGame()
	{
		Print("Overthrow: Requesting Start Game");
		Rpc(RpcAsk_StartNewGame);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_StartNewGame()
	{
		Print ("Overthrow: Start New Game Requested");
		DoStartNewGame();
		DoStartGame();
	}
	
	void StartGame()
	{
		Print("Overthrow: Requesting Start Game");
		Rpc(RpcAsk_StartGame);
	}
	
	[RplRpc(RplChannel.Reliable, RplRcver.Server)]
	protected void RpcAsk_StartGame()
	{
		Print ("Overthrow: Start Game Requested");
		DoStartGame();
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
		if(!m_PlayerManager) return;
		m_PlayerManager.m_OnPlayerRegistered.Remove(OnPlayerRegistered);
	}
}