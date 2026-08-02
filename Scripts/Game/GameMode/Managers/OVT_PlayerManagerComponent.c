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
	//! Applies persisted player records to the live player table.
	//!
	//! Called from OVT_PlayerManagerSerializer.Deserialize().
	//!
	//! UPDATES IN PLACE, NEVER REPLACES. EPF's OVT_PlayerSaveData.ApplyTo() swapped the whole
	//! OVT_PlayerData object into the map, which was harmless when it only ever ran before anyone had
	//! connected. Persisted data can now also be re-applied to a LIVE session, where swapping the
	//! object would throw away the runtime player ID inside it (making a connected player look
	//! offline) and invalidate every reference already handed out. So each record is written onto the
	//! existing entry and only a MISSING one is created.
	//!
	//! DERIVED STATE IS REBUILT, NOT RESTORED. The skill-effect outputs on OVT_PlayerData are
	//! [NonSerialized()] and are not in the save; they are produced by replaying the effects of every
	//! earned skill level. That replay is what m_OnPlayerDataLoaded drives
	//! (OVT_SkillManagerComponent.OnPlayerDataLoaded), so the derived fields are reset immediately
	//! before it fires - which is also what makes a second pass over the same records safe.
	//!
	//! NO RPC. Clients receive the whole table through RplSave/RplLoad instead - see the serializer.
	//! \param[in] records Persisted player records, may be null.
	void ApplyPersistedPlayers(array<ref OVT_PersistedPlayer> records)
	{
		if (!records)
			return;

		if (!m_mPlayers)
			m_mPlayers = new map<string, ref OVT_PlayerData>();

		OVT_SkillManagerComponent skills = OVT_Global.GetSkills();

		foreach (OVT_PersistedPlayer record : records)
		{
			if (!record)
				continue;

			if (record.persistentId == "")
			{
				Print("[Overthrow] Skipping a saved player record with no persistent ID", LogLevel.WARNING);
				continue;
			}

			OVT_PlayerData player = GetPlayer(record.persistentId);
			if (!player)
			{
				player = new OVT_PlayerData();
				m_mPlayers[record.persistentId] = player;

				// A player who is already connected when their record comes back out of storage keeps
				// the runtime ID SetupPlayer() gave them.
				int liveId = GetPlayerIDFromPersistentID(record.persistentId);
				if (liveId > 0)
					player.id = liveId;
			}

			player.name = record.name;
			player.home = record.home;
			player.camp = record.camp;
			player.money = record.money;
			player.initialized = record.initialized;
			player.isOfficer = record.isOfficer;
			player.kills = record.kills;
			player.xp = record.xp;
			player.levelNotified = record.levelNotified;

			// THE BODY ID IS ADOPTED ONLY WHEN THE LIVE RECORD HAS NONE, for the same reason the record
			// object is updated rather than replaced: on a real load the record is brand new and takes the
			// stored id; when saved data is re-applied to a RUNNING campaign the live record already
			// points at the character the player is standing in, and that is the more current fact.
			if (player.m_sBodyPersistenceId == "")
				player.m_sBodyPersistenceId = record.bodyPersistenceId;

			ApplyPersistedSkills(player, record, skills);

			player.ResetSkillEffects();
			m_OnPlayerDataLoaded.Invoke(player, record.persistentId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Rebuilds one player's skill levels from the parallel key/level arrays a save record carries.
	//!
	//! A key the skill config no longer defines is DROPPED. Keeping it would hand
	//! OVT_SkillManagerComponent.OnPlayerDataLoaded() a key whose GetSkill() returns null, and it
	//! dereferences that immediately - a removed or renamed skill would turn every load into a crash.
	//! \param[in] player The live record being filled.
	//! \param[in] record The saved record being read.
	//! \param[in] skills The skill manager used to validate keys, may be null.
	protected void ApplyPersistedSkills(notnull OVT_PlayerData player, notnull OVT_PersistedPlayer record, OVT_SkillManagerComponent skills)
	{
		if (!player.skills)
			player.skills = new map<string, int>();

		player.skills.Clear();

		if (!record.skillKeys || !record.skillLevels)
			return;

		// Only worth validating against a skill manager that actually has a config loaded.
		bool canValidate = skills && skills.m_Skills && skills.m_Skills.m_aSkills;

		int count = record.skillKeys.Count();
		if (record.skillLevels.Count() < count)
			count = record.skillLevels.Count();

		for (int i = 0; i < count; i++)
		{
			string key = record.skillKeys[i];
			if (key == "")
				continue;

			if (canValidate && !skills.GetSkill(key))
			{
				Print(string.Format("[Overthrow] Dropping saved skill '%1' - the skill config no longer defines it", key), LogLevel.WARNING);
				continue;
			}

			player.skills.Set(key, record.skillLevels[i]);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Writes every connected player's CHARACTER persistence id back onto their record.
	//!
	//! The stored id is which character gets asked for when that player next spawns
	//! (OVT_SpawnLogic.RequestPersistedPlayerBody), and it is what the save point carries
	//! (OVT_PlayerManagerSerializer), so this is what makes a player come back as themselves - carrying
	//! what they were carrying, standing where they saved - rather than as a fresh civilian.
	//!
	//! THIS HOOK IS THE ONLY PLACE A STILL-PLAYING PLAYER'S ID IS WRITTEN DOWN. A player who never
	//! disconnects never passes through the disconnect capture, so without this, quitting to the menu
	//! mid-session and continuing would be the one case that still lost gear. That is exactly the
	//! lesson OVT_RecruitManagerComponent.SyncRecruitPositions() already encodes for recruits.
	//!
	//! Called before every save (OVT_OverthrowGameMode.PreShutdownPersist) on the authority. It is
	//! deliberately NOT a per-frame update: the id only has to be true at the moments a body can stop
	//! existing.
	//!
	//! A DEAD PLAYER IS SKIPPED. Death is complete loss - OVT_SpawnLogic clears the id when a player is
	//! killed, and a save taken in the frame between the death and the respawn must not put the corpse's
	//! id back. The corpse itself persists on its own (see OVT_DeadCharacterPersistenceConfigRule), so
	//! nothing is lost by ignoring it here.
	void SyncPlayerBodyIds()
	{
		array<int> connectedPlayers = {};
		GetGame().GetPlayerManager().GetPlayers(connectedPlayers);

		foreach (int playerId : connectedPlayers)
		{
			string persistentId = GetPersistentIDFromPlayerID(playerId);
			if (persistentId.IsEmpty())
				continue;

			OVT_PlayerData player = GetPlayer(persistentId);
			if (!player)
				continue;

			IEntity character = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
			if (!character)
				continue;

			// materialise: a live body may be registered but never yet written, and an id it has not been
			// given cannot be stored. The save point is about to write this character anyway.
			CapturePlayerBodyId(player, character, true);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Notes the persistence id of a player's body on their record.
	//!
	//! The id is what OVT_SpawnLogic asks for later, so this is the single point where "which character
	//! IS this player" is written down. An id that cannot be read is never allowed to overwrite one that
	//! could: an empty answer leaves the record untouched.
	//! \param[in] player The record to write to.
	//! \param[in] character The body.
	//! \param[in] materialise True to write the body's record first when it has no id yet. Only pass true
	//! where writing a record is wanted anyway - it is a real storage write, not a lookup.
	//! \return True when an id was captured.
	bool CapturePlayerBodyId(notnull OVT_PlayerData player, notnull IEntity character, bool materialise)
	{
		if (IsCharacterDead(character))
			return false;

		string bodyId = OVT_PersistenceTracking.GetPersistentId(character);

		if (bodyId.IsEmpty() && materialise)
		{
			// Registration is lazy, so an instance the system has never written may not have an identity
			// to hand out yet. Writing its record is what gives it one.
			OVT_PersistenceTracking.Save(character);
			bodyId = OVT_PersistenceTracking.GetPersistentId(character);
		}

		if (bodyId.IsEmpty())
			return false;

		player.m_sBodyPersistenceId = bodyId;
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a character entity is a corpse.
	//!
	//! Vanilla's own test for the same question, on the same class - SCR_SpawnLogic.c:423 refuses to hand
	//! a persistence-restored character back to a player when GetCharacterController().IsDead().
	//! \param[in] entity The entity to test. Anything that is not a character answers false.
	//! \return True when the entity is a character whose controller reports it dead.
	static bool IsCharacterDead(IEntity entity)
	{
		ChimeraCharacter character = ChimeraCharacter.Cast(entity);
		if (!character)
			return false;

		CharacterControllerComponent controller = character.GetCharacterController();
		if (!controller)
			return false;

		return controller.IsDead();
	}

	//------------------------------------------------------------------------------------------------
	//! Checks if the local player holds the officer role.
	//! \return True if the local player is an officer, false otherwise.
	bool LocalPlayerIsOfficer()
	{
		int localId = SCR_PossessingManagerComponent.GetPlayerIdFromControlledEntity(SCR_PlayerController.GetLocalControlledEntity());
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
	//! If the mapping doesn't exist, it attempts to create it using OVT_Global.GetPlayerUID and calls SetupPlayer.
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
		int playerId = SCR_PossessingManagerComponent.GetPlayerIdFromControlledEntity(controlled);
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
				overthrowController.NotifyOwnerAssignment(playerId);
			}
		}
		else
		{
			Print("[Overthrow] ERROR: No controller found for player " + playerId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Drops a disconnecting player's runtime<->persistent ID mappings.
	//!
	//! Runtime player IDs are session-scoped and CAN BE REUSED by a later joiner. A stale entry left
	//! behind by a departed player would make persistent-ID lookups resolve to whichever live player
	//! inherited the number (money streams, controller lookups, notifications), so both directions of
	//! the mapping are removed here. The OVT_PlayerData record in m_mPlayers is deliberately KEPT -
	//! that is what lets a reconnecting player come back as themselves; SetupPlayer rebuilds the
	//! mappings when they do.
	//!
	//! Called from OVT_OverthrowGameMode.OnPlayerDisconnected, after m_OnPlayerDisconnected has fired,
	//! so disconnect listeners can still translate the departing player's IDs.
	//! \param[in] playerId The runtime integer ID of the disconnecting player.
	void ClearPlayerIdMappings(int playerId)
	{
		if(!m_mPersistentIDs.Contains(playerId)) return;

		string persId = m_mPersistentIDs[playerId];
		m_mPersistentIDs.Remove(playerId);

		// Only drop the reverse entry while it still points at this runtime ID - it may already
		// belong to a new session of the same player.
		if(m_mPlayerIDs.Contains(persId) && m_mPlayerIDs[persId] == playerId)
		{
			m_mPlayerIDs.Remove(persId);
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
		
		// Note: We keep the player data (m_mPlayers) for when they reconnect; the session-scoped
		// ID mappings are dropped separately by ClearPlayerIdMappings on disconnect
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