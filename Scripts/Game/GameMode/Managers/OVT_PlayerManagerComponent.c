class OVT_PlayerManagerComponentClass: OVT_ComponentClass
{
};

//------------------------------------------------------------------------------------------------
//! Manages player data persistence and access across the game session.
//! Handles mapping between player IDs and persistent IDs, storing player-specific data like money, home location, skills, etc.
//! Also responsible for replicating player data to clients joining in progress (JIP).

class OVT_PlayerManagerComponent: OVT_Component
{		
	//------------------------------------------------------------------------------------------------
	//! Static instance of the player manager component for easy access.
	static OVT_PlayerManagerComponent s_Instance;
	
	[Attribute("{6246D0740A99F50B}Prefabs/GameMode/OVT_OverthrowController.et", uiwidget: UIWidgets.ResourceNamePicker, desc: "Overthrow Controller Prefab", params: "et")]
	ResourceName m_OverthrowControllerPrefab;

	//------------------------------------------------------------------------------------------------
	//! Returns the static instance of the OVT_PlayerManagerComponent.
	//! Creates the instance if it doesn't exist by finding it on the active GameMode.
	//! \return The singleton instance of OVT_PlayerManagerComponent, or null if not found.
	static OVT_PlayerManagerComponent GetInstance()
	{
		if (!s_Instance)
		{
			BaseGameMode pGameMode = GetGame().GetGameMode();
			if (pGameMode)
				s_Instance = OVT_PlayerManagerComponent.Cast(pGameMode.FindComponent(OVT_PlayerManagerComponent));
		}

		return s_Instance;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Invoker called when player data is fully loaded (e.g., after replication).
	ref ScriptInvoker m_OnPlayerDataLoaded = new ScriptInvoker();
	
	//------------------------------------------------------------------------------------------------
	//! Invoker called when a player connects (args: string persistentId, int playerId)
	ref ScriptInvoker m_OnPlayerConnected = new ScriptInvoker();
	
	//------------------------------------------------------------------------------------------------
	//! Invoker called when a player disconnects (args: string persistentId, int playerId)
	ref ScriptInvoker m_OnPlayerDisconnected = new ScriptInvoker();
	
	//------------------------------------------------------------------------------------------------
	//! Maps runtime Player IDs (int) to their persistent string IDs (string).
	protected ref map<int, string> m_mPersistentIDs;
	
	//------------------------------------------------------------------------------------------------
	//! Maps persistent string IDs (string) back to runtime Player IDs (int).
	protected ref map<string, int> m_mPlayerIDs;
	
	//------------------------------------------------------------------------------------------------
	//! Stores the OVT_PlayerData object for each player, keyed by their persistent ID (string).
	ref map<string, ref OVT_PlayerData> m_mPlayers;
	
	//------------------------------------------------------------------------------------------------
	//! Maps player IDs to their controller entities for network ownership management.
	protected ref map<int, IEntity> m_mPlayerControllers;
	
	//------------------------------------------------------------------------------------------------
	//! Initializes the component's internal maps.
	//! \param owner The entity this component is attached to.
	void Init(IEntity owner)
	{
		Print("[Overthrow] PlayerManager init - existing m_mPlayers: " + m_mPlayers);
		if(m_mPlayers)
		{
			Print("[Overthrow] WARNING: PlayerManager Init() called but m_mPlayers already exists with " + m_mPlayers.Count() + " players");
		}
		m_mPersistentIDs = new map<int, string>;
		m_mPlayerIDs = new map<string, int>;
		m_mPlayers = new map<string, ref OVT_PlayerData>;
		m_mPlayerControllers = new map<int, IEntity>;
		Print("[Overthrow] PlayerManager init complete - new m_mPlayers: " + m_mPlayers);
		
		// Subscribe to player disconnect events
		if(Replication.IsServer())
		{
			GetGame().GetCallqueue().CallLater(CheckDisconnectedPlayers, 5000, true); // Check every 5 seconds
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Retrieves the player data object for a given persistent ID.
	//! \param[in] persId The persistent string ID of the player.
	//! \return The OVT_PlayerData object for the player, or null if not found.
	OVT_PlayerData GetPlayer(string persId)
	{
		if(m_mPlayers.Contains(persId)) return m_mPlayers[persId];
		return null;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Checks if the local player holds the officer role.
	//! \return True if the local player is an officer, false otherwise.
	bool LocalPlayerIsOfficer()
	{
		int localId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(SCR_PlayerController.GetLocalControlledEntity());
		string persId = GetPersistentIDFromPlayerID(localId);
		OVT_PlayerData player = GetPlayer(persId);
		return player.isOfficer;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Retrieves the player data object for a given runtime player ID.
	//! \param[in] playerId The runtime integer ID of the player.
	//! \return The OVT_PlayerData object for the player, or null if not found.
	OVT_PlayerData GetPlayer(int playerId)
	{		
		return GetPlayer(GetPersistentIDFromPlayerID(playerId));
	}
	
	//------------------------------------------------------------------------------------------------
	//! Retrieves the player's name using their persistent ID.
	//! \param[in] persId The persistent string ID of the player.
	//! \return The player's name, or an empty string if the player is not found.
	string GetPlayerName(string persId)
	{
		OVT_PlayerData player = GetPlayer(persId);
		if(player) return player.name;
		return "";
	}
	
	//------------------------------------------------------------------------------------------------
	//! Retrieves the player's name using their runtime player ID.
	//! \param[in] playerId The runtime integer ID of the player.
	//! \return The player's name, or an empty string if the player is not found.
	string GetPlayerName(int playerId)
	{
		return GetPlayerName(GetPersistentIDFromPlayerID(playerId));
	}
	
	//------------------------------------------------------------------------------------------------
	//! Retrieves the persistent string ID associated with a runtime player ID.
	//! If the mapping doesn't exist, it attempts to create it using EPF_Utils.GetPlayerUID and calls SetupPlayer.
	//! Includes a Workbench-specific hack to limit player IDs for testing.
	//! \param[in] playerId The runtime integer ID of the player.
	//! \return The persistent string ID for the player.
	string GetPersistentIDFromPlayerID(int playerId)
	{
		if(playerId < 1) return "";
		if(!m_mPersistentIDs.Contains(playerId)) {
			return "";
		}
		return m_mPersistentIDs[playerId];
	}
	
	//------------------------------------------------------------------------------------------------
	//! Retrieves the persistent string ID associated with a player's controlled entity.
	//! \param[in] controlled The entity controlled by the player.
	//! \return The persistent string ID for the player controlling the entity.
	string GetPersistentIDFromControlledEntity(IEntity controlled)
	{
		int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(controlled);
		return GetPersistentIDFromPlayerID(playerId);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Retrieves the runtime player ID associated with a persistent string ID.
	//! \param[in] id The persistent string ID of the player.
	//! \return The runtime integer ID for the player, or -1 if not found.
	int GetPlayerIDFromPersistentID(string id)
	{
		if(!m_mPlayerIDs.Contains(id)) return -1;
		return m_mPlayerIDs[id];
	}
	
	//------------------------------------------------------------------------------------------------
	//! Retrieves the controller entity for a given player ID.
	//! \param[in] playerId The runtime integer ID of the player.
	//! \return The OVT_OverthrowController for the player, or null if not found.
	OVT_OverthrowController GetController(int playerId)
	{
		if(!m_mPlayerControllers.Contains(playerId)) return null;
		IEntity controller = m_mPlayerControllers[playerId];
		// Check if entity still exists
		if(!controller || controller.IsDeleted()) 
		{
			m_mPlayerControllers.Remove(playerId);
			return null;
		}
		return OVT_OverthrowController.Cast(controller);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Registers a controller entity for a player (called by the controller's RPC on clients)
	//! \param[in] playerId The runtime integer ID of the player.
	//! \param[in] controller The controller entity to register.
	void RegisterControllerForPlayer(int playerId, IEntity controller)
	{
		if (controller)
		{
			m_mPlayerControllers[playerId] = controller;
			Print("[Overthrow] Client registered controller for player " + playerId);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Retrieves the controller entity for a given persistent ID.
	//! \param[in] persistentId The persistent string ID of the player.
	//! \return The OVT_OverthrowController for the player, or null if not found.
	OVT_OverthrowController GetController(string persistentId)
	{
		int playerId = GetPlayerIDFromPersistentID(persistentId);
		if(playerId == -1) return null;
		return GetController(playerId);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Sets up the player's data mappings and initializes their OVT_PlayerData if it doesn't exist.
	//! Stores the mapping between runtime ID and persistent ID, retrieves the player name, and assigns the runtime ID to the data object.
	//! If running on the server, it replicates this registration to all clients.
	//! \param[in] playerId The runtime integer ID of the player.
	//! \param[in] persistentId The persistent string ID of the player.
	void SetupPlayer(int playerId, string persistentId)
	{
		// Validate persistent ID
		if(!persistentId || persistentId.IsEmpty())
		{
			Print("[Overthrow] ERROR: SetupPlayer called with empty/null persistentId for playerId: " + playerId);
			return;
		}
		
		Print("Setting up player: " + persistentId + " with playerId: " + playerId);
		
		// Check if this persistent ID is already mapped to a different player ID
		if(m_mPlayerIDs.Contains(persistentId))
		{
			int existingPlayerId = m_mPlayerIDs[persistentId];
			if(existingPlayerId != playerId)
			{
				Print("[Overthrow] WARNING: Persistent ID " + persistentId + " already mapped to playerId " + existingPlayerId + ", now being mapped to " + playerId);
				Print("[Overthrow] This may indicate player duplication in hosted multiplayer mode");
			}
		}
		
		m_mPersistentIDs[playerId] = persistentId;
		m_mPlayerIDs[persistentId] = playerId;
		
		OVT_PlayerData player = GetPlayer(persistentId);		
					
		if(!player)
		{
			player = new OVT_PlayerData;
			m_mPlayers[persistentId] = player;			
		}
		
		player.name = GetGame().GetPlayerManager().GetPlayerName(playerId);
		
		player.id = playerId;
		
		if(!Replication.IsServer())
		{
			return;
		}
		
		// Spawn controller entity for the player
		if(!m_OverthrowControllerPrefab || m_mPlayerControllers.Contains(playerId))
		{
			Rpc(RpcDo_RegisterPlayer, playerId, persistentId);
			// Could be a reconnection with still existing controller, re-assign ownership and notify the client
			AssignControllerOwnership(playerId);
			return;
		}
		
		EntitySpawnParams params = EntitySpawnParams();
		params.TransformMode = ETransformMode.WORLD;
		
		IEntity controller = GetGame().SpawnEntityPrefab(Resource.Load(m_OverthrowControllerPrefab), null, params);
		if(!controller)
		{
			Print("[Overthrow] ERROR: Failed to spawn controller entity for player " + playerId);
			Rpc(RpcDo_RegisterPlayer, playerId, persistentId);
			return;
		}
		
		m_mPlayerControllers[playerId] = controller;
		
		// Assign ownership to the player and notify the client
		AssignControllerOwnership(playerId);

		Print("[Overthrow] Created controller entity for player " + playerId + " (" + persistentId + ")");
				
		Rpc(RpcDo_RegisterPlayer, playerId, persistentId);
	}

	void AssignControllerOwnership(int playerId)
	{
		IEntity controller = m_mPlayerControllers[playerId];
		if(controller)
		{
			RplComponent rplComponent = RplComponent.Cast(controller.FindComponent(RplComponent));
			if(rplComponent)
			{
				// Get player controller and its replication identity
				PlayerController playerController = GetGame().GetPlayerManager().GetPlayerController(playerId);
				if(playerController)
				{
					// Get player replication ID (not identical to player ID!)
					RplIdentity playerRplID = playerController.GetRplIdentity();
					if(playerRplID.IsValid())
					{
						// Give ownership with notification
						rplComponent.GiveExt(playerRplID, true);
					}
				}
			}
			// Notify the owning client about their controller assignment
			OVT_OverthrowController overthrowController = OVT_OverthrowController.Cast(controller);
			if (overthrowController)
			{
				overthrowController.Rpc(overthrowController.RpcDo_NotifyOwnerAssignment, playerId);
			}
		}
		else
		{
			Print("[Overthrow] ERROR: No controller found for player " + playerId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Periodically checks for disconnected players and cleans up their controller entities.
	//! Only runs on the server. Iterates through all tracked player controllers and removes
	//! entities for players who are no longer connected.
	void CheckDisconnectedPlayers()
	{
		if(!Replication.IsServer()) return;
		
		array<int> disconnectedPlayers = {};
		PlayerManager playerManager = GetGame().GetPlayerManager();
		
		// Find disconnected players
		foreach(int playerId, IEntity controller : m_mPlayerControllers)
		{
			// Check if player is still connected
			if(!playerManager.IsPlayerConnected(playerId))
			{
				disconnectedPlayers.Insert(playerId);
			}
		}
		
		// Clean up disconnected player controllers
		foreach(int playerId : disconnectedPlayers)
		{
			CleanupPlayerController(playerId);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Cleans up the controller entity for a specific player.
	//! Removes the entity from the world and from tracking maps.
	//! \param[in] playerId The runtime integer ID of the player whose controller should be cleaned up.
	void CleanupPlayerController(int playerId)
	{
		if(!m_mPlayerControllers.Contains(playerId)) return;
		
		IEntity controller = m_mPlayerControllers[playerId];
		if(controller && !controller.IsDeleted())
		{
			// Delete the entity
			delete controller;
			Print("[Overthrow] Cleaned up controller entity for disconnected player " + playerId);
		}
		
		// Remove from tracking
		m_mPlayerControllers.Remove(playerId);
		
		// Note: We keep the player data and persistent ID mappings for when they reconnect
	}
	
	//RPC Methods
	
	//------------------------------------------------------------------------------------------------
	//! Saves the state of all managed players for replication (e.g., for JIP).
	//! Writes player count, then iterates through players writing persistent ID, runtime ID, and all OVT_PlayerData fields.
	//! \param[in,out] writer The ScriptBitWriter to write data to.
	//! \return True if saving was successful.
	override bool RplSave(ScriptBitWriter writer)
	{
		//Send JIP Players
		writer.WriteInt(m_mPlayers.Count());
		for(int i=0; i<m_mPlayers.Count(); i++)
		{
			OVT_PlayerData player = m_mPlayers.GetElement(i);
			writer.WriteString(m_mPlayers.GetKey(i));
			writer.WriteInt(player.id);
			writer.WriteInt(player.money);
			writer.WriteVector(player.home);
			writer.WriteVector(player.camp);
			writer.WriteString(player.name);
			writer.WriteBool(player.isOfficer);
			
			writer.WriteInt(player.skills.Count());
			for(int t=0; t<player.skills.Count(); t++)
			{
				writer.WriteString(player.skills.GetKey(t));
				writer.WriteInt(player.skills.GetElement(t));
			}
			
			writer.WriteInt(player.kills);
			writer.WriteInt(player.xp);
		}		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Loads the state of players received via replication (e.g., for JIP clients).
	//! Reads player count, then iterates reading persistent ID and runtime ID. If the player data doesn't exist locally, it's created.
	//! Populates the OVT_PlayerData fields from the stream and updates ID mappings. Finally notifies the skill system.
	//! \param[in,out] reader The ScriptBitReader to read data from.
	//! \return True if loading was successful, false on read error.
	override bool RplLoad(ScriptBitReader reader)
	{		
		
		int length, playerId, skilllength, level;
		string persId, skill;
		
		//Recieve JIP players
		if (!reader.ReadInt(length)) return false;
		for(int i=0; i<length; i++)
		{
			if (!reader.ReadString(persId)) return false;
			if (!reader.ReadInt(playerId)) return false;
			OVT_PlayerData player = GetPlayer(persId);
			if(!player)
			{
				player = new OVT_PlayerData;
				m_mPlayers[persId] = player;
				player.id = playerId;		
			}
			m_mPersistentIDs[playerId] = persId;
			m_mPlayerIDs[persId] = playerId;
			
			if (!reader.ReadInt(player.money)) return false;
			if (!reader.ReadVector(player.home)) return false;
			if (!reader.ReadVector(player.camp)) return false;
			if (!reader.ReadString(player.name)) return false;
			if (!reader.ReadBool(player.isOfficer)) return false;
			
			if (!reader.ReadInt(skilllength)) return false;
			for(int t=0; t<skilllength; t++)
			{
				if (!reader.ReadString(skill)) return false;
				if (!reader.ReadInt(level)) return false;
				player.skills[skill] = level;
			}
			
			if(!reader.ReadInt(player.kills)) return false;
			if(!reader.ReadInt(player.xp)) return false;
			
			OVT_Global.GetSkills().OnPlayerDataLoaded(player, persId);
		}
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! RPC call invoked on all clients (Broadcast) to register a player.
	//! Calls SetupPlayer locally to ensure all clients have the player's mappings and basic data.
	//! \param[in] playerId The runtime integer ID of the player being registered.
	//! \param[in] s The persistent string ID of the player being registered.
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RegisterPlayer(int playerId, string s)
	{
		SetupPlayer(playerId, s);
	}
	
}