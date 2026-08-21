class OVT_Global : Managed
{
	//------------------------------------------------------------------------------------------------
	//! Gets the prefab an entity was spawned from, resolving prefab inheritance the way the engine does.
	//!
	//! Walks the container ancestry to the first container that actually came from a .et file, which is
	//! what makes this correct for nested/inherited prefabs where EntityPrefabData.GetPrefabName() can
	//! answer with an intermediate name. This is the vanilla replacement for the prefab-name utility the
	//! old persistence framework provided, and is deliberately identical to it.
	//! \param[in] entity The entity to look up.
	//! \return Its prefab resource name, or an empty resource name.
	static ResourceName GetPrefabName(IEntity entity)
	{
		if (!entity)
			return ResourceName.Empty;

		EntityPrefabData prefabData = entity.GetPrefabData();
		if (!prefabData)
			return ResourceName.Empty;

		return SCR_BaseContainerTools.GetPrefabResourceName(prefabData.GetPrefab());
	}

	//! CLI parameter that opts a session in to synthesised development UIDs. See GetPlayerUID().
	static const string DEV_UID_CLI_PARAM = "ovtDevUid";

	//! Prefix of a synthesised development UID. Deliberately distinctive so one showing up in a save,
	//! a log or a player record is immediately recognisable as not a real platform identity.
	static const string DEV_UID_PREFIX = "DEV_";

	//------------------------------------------------------------------------------------------------
	//! Gets the platform-stable identity id of a connected player.
	//!
	//! This is the string Overthrow keys every player record on. Vanilla replacement for the player-UID
	//! utility the old persistence framework provided, which was a one-line forward to exactly this call.
	//!
	//! DEVELOPMENT FALLBACK: a server started without a backend connection (tools/launch-server.sh
	//! --mode local, i.e. the engine's -server route) authenticates nobody, so every player's identity
	//! id is empty forever. Overthrow keys everything on that string, so with no identity OVT_SpawnLogic
	//! .DoSpawn_S can never register the player and retries once a second indefinitely - the player sits
	//! at the spawn camera and never enters the world. When the session opted in, synthesise a UID from
	//! the runtime player id instead: distinct per connected client (DEV_1, DEV_2, ...) and computed
	//! identically on server and clients, so no extra replication is needed.
	//!
	//! Gated on a CLI parameter and NOT merely on "the identity is empty", deliberately. An empty
	//! identity is also the normal transient state while a real player is still authenticating, so
	//! synthesising on sight would hand a legitimate player a fresh blank record instead of waiting for
	//! their real one. A production server never passes the parameter, so the fallback cannot fire there.
	//! \param[in] playerId Runtime player id.
	//! \return The identity id; a synthesised DEV_ id when the session opted in and no identity exists;
	//!         otherwise an empty string, exactly as before.
	static string GetPlayerUID(int playerId)
	{
		string identity = SCR_PlayerIdentityUtils.GetPlayerIdentityId(playerId);

		// The backend can transiently answer the per-player lookup with the NULL UUID
		// ("00000000-0000-...") instead of an empty string - observed in a Workbench session
		// 2026-08-20, where the engine's own "### Updating player" line already carried the zero id.
		// It is non-empty, so it defeats both vanilla's name-hash fallback (SCR_PlayerIdentityUtils
		// fires it only on EMPTY) and the empty-identity guards below, and a whole campaign ends up
		// keyed to "00000000-..." - a player no session can ever be. Treat it as "no identity" and
		// recover one.
		UUID identityUuid = identity;
		if (identity != string.Empty && identityUuid.IsNull())
			identity = RecoverNullIdentity(playerId);

		if (identity != string.Empty)
			return identity;

		if (!System.IsCLIParam(DEV_UID_CLI_PARAM))
			return string.Empty;

		if (playerId < 1)
			return string.Empty;

		return DEV_UID_PREFIX + playerId.ToString();
	}

	//------------------------------------------------------------------------------------------------
	//! Recovers a usable identity for a player whose backend lookup answered the NULL UUID.
	//!
	//! Order matters, and each step is chosen to reproduce the id the player would have had in a
	//! healthy session:
	//!
	//!   1. On a NON-DEDICATED session, the HOST's own player gets the local authenticator identity
	//!      (BackendAuthenticatorApi.GetIdentityId()) - the same profile identity the per-player
	//!      lookup returns when it is not misbehaving, so a campaign started under a flaked session
	//!      and one started under a healthy session key to the SAME id.
	//!   2. Failing that, the name-hash UUID vanilla itself derives for an EMPTY identity on
	//!      non-dedicated sessions (SCR_PlayerIdentityUtils.c:26-36) - that fallback is unreachable
	//!      when the backend answers the zero id rather than an empty string, so it is reproduced
	//!      here verbatim. Deterministic per player name, therefore stable across sessions.
	//!   3. A DEDICATED server never synthesises: a real identity may still arrive, and handing a
	//!      joining player a made-up record would be worse than making them wait. Empty string means
	//!      "not ready", which every caller already handles.
	//! \param[in] playerId Runtime player id whose backend identity came back as the NULL UUID.
	//! \return A stable identity id, or an empty string when none can be recovered.
	protected static string RecoverNullIdentity(int playerId)
	{
		if (RplSession.Mode() == RplMode.Dedicated)
		{
			Print("[Overthrow] Backend answered the NULL UUID for player " + playerId + " on a dedicated server - treating the identity as not ready", LogLevel.ERROR);
			return string.Empty;
		}

		if (playerId == SCR_PlayerController.GetLocalPlayerId())
		{
			string localIdentity = BackendAuthenticatorApi.GetIdentityId();
			UUID localUuid = localIdentity;
			if (localIdentity != string.Empty && !localUuid.IsNull())
			{
				Print("[Overthrow] Backend answered the NULL UUID for player " + playerId + " - recovered the local profile identity " + localIdentity, LogLevel.WARNING);
				return localIdentity;
			}
		}

		string playerName = GetGame().GetPlayerManager().GetPlayerName(playerId);
		if (playerName.IsEmpty())
			return string.Empty;

		// Vanilla's own name-hash derivation, byte for byte (SCR_PlayerIdentityUtils.c:26-36).
		int splitLength = Math.Max(1, playerName.Length() / 3);
		string split1 = Math.AbsInt(playerName.Substring(0, splitLength).Hash()).ToString(8, true);
		string split2 = Math.AbsInt(playerName.Substring(splitLength, splitLength).Hash()).ToString(8, true);
		int doubleSplit = splitLength * 2;
		string split3 = Math.AbsInt(playerName.Substring(doubleSplit, playerName.Length() - doubleSplit).Hash()).ToString(8, true);
		string derived = string.Format("00bbbddd-%1-%2-%3-%4%5", split1.Substring(0, 4), split1.Substring(4, 4), split2.Substring(0, 4), split2.Substring(4, 4), split3);
		derived.ToLower();

		Print("[Overthrow] Backend answered the NULL UUID for player " + playerId + " - derived the name-hash identity " + derived, LogLevel.WARNING);
		return derived;
	}

	static OVT_PlayerCommsComponent GetServer()
	{		
		if(Replication.IsServer())
		{
			return OVT_PlayerCommsComponent.Cast(GetOverthrow().FindComponent(OVT_PlayerCommsComponent));
		}		
		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		return OVT_PlayerCommsComponent.Cast(player.FindComponent(OVT_PlayerCommsComponent));
	}

	static OVT_UIManagerComponent GetUI()
	{	
		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		return OVT_UIManagerComponent.Cast(player.FindComponent(OVT_UIManagerComponent));
	}
	
	//------------------------------------------------------------------------------------------------
	//! CLIENT-ONLY. Resolves the persistent id of the player sitting at THIS machine.
	//!
	//! Never call this from anything the server can reach, for exactly the same reason
	//! SCR_PlayerController.GetLocalPlayerId() is itself client-only: on a dedicated server there is no
	//! local player, and on a listen server it silently answers with the host's identity. Server code must
	//! take the persistent id as a parameter instead.
	//!
	//! Unlike the older map-UI idiom, this does NOT go through the controlled entity. A dead player may
	//! have no controlled entity at all, and every consumer of the persistent id fails closed on an empty
	//! string (private camps filter out, houses do not populate, the controller cannot be found) - a screen
	//! that draws nothing and logs nothing. The runtime player id survives death, so this does too.
	//!
	//! Failure mode is unchanged from the entity-based route: GetLocalPlayerId() answers 0 with no player
	//! controller, and GetPersistentIDFromPlayerID() answers "" for any id below 1 or not yet registered.
	//! \return The local player's persistent id, or an empty string when it cannot be resolved.
	static string GetLocalPersistentId()
	{
		OVT_PlayerManagerComponent players = GetPlayers();
		if (!players) return "";

		return players.GetPersistentIDFromPlayerID(SCR_PlayerController.GetLocalPlayerId());
	}

	//------------------------------------------------------------------------------------------------
	//! Get the local player's overthrow controller entity
	//!
	//! The controlled-entity route is kept first and unchanged so the living path cannot regress. The
	//! fallback exists because a dead player awaiting respawn may control nothing, and without it every
	//! controller component - and therefore every client->server request - is unreachable while dead.
	//! \return Controller entity or null if not found/on server
	static OVT_OverthrowController GetController()
	{
		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		if (player)
		{
			int playerId = SCR_PossessingManagerComponent.GetPlayerIdFromControlledEntity(player);
			return GetPlayers().GetController(playerId);
		}

		int localPlayerId = SCR_PlayerController.GetLocalPlayerId();
		if (localPlayerId < 1) return null;

		OVT_PlayerManagerComponent players = GetPlayers();
		if (!players) return null;

		return players.GetController(localPlayerId);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Convenience method to get container transfer component
	//! \return Container transfer component or null
	static OVT_ContainerTransferComponent GetContainerTransfer()
	{
		OVT_OverthrowController controller = GetController();
		if (!controller) return null;
		
		return OVT_ContainerTransferComponent.Cast(controller.FindComponent(OVT_ContainerTransferComponent));
	}
	
	//------------------------------------------------------------------------------------------------
	//! Convenience method to get the server-authoritative shop transaction component
	//! \return Shop transaction component or null
	static OVT_ShopTransactionComponent GetShopTransactions()
	{
		OVT_OverthrowController controller = GetController();
		if (!controller) return null;

		return OVT_ShopTransactionComponent.Cast(controller.FindComponent(OVT_ShopTransactionComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Convenience method to get the radio tower sabotage relay component
	//! \return Tower sabotage component or null
	static OVT_TowerSabotageComponent GetTowerSabotage()
	{
		OVT_OverthrowController controller = GetController();
		if (!controller) return null;

		return OVT_TowerSabotageComponent.Cast(controller.FindComponent(OVT_TowerSabotageComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Convenience method to get the server-authoritative travel request component
	//! \return Travel request component or null
	static OVT_TravelRequestComponent GetTravelRequests()
	{
		OVT_OverthrowController controller = GetController();
		if (!controller) return null;

		return OVT_TravelRequestComponent.Cast(controller.FindComponent(OVT_TravelRequestComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Convenience method to get the server-authoritative respawn request component
	//!
	//! CLIENT-ONLY, like every other accessor built on GetController(). This is the one that depends
	//! on GetController()'s no-controlled-entity fallback: its caller is a player with no character,
	//! and without that branch this would answer null for exactly the state it exists to serve.
	//! \return Respawn request component or null
	static OVT_RespawnRequestComponent GetRespawnRequests()
	{
		OVT_OverthrowController controller = GetController();
		if (!controller) return null;

		return OVT_RespawnRequestComponent.Cast(controller.FindComponent(OVT_RespawnRequestComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Convenience method to get the server-authoritative town uprising request component
	//! \return Uprising request component or null
	static OVT_UprisingRequestComponent GetUprisingRequests()
	{
		OVT_OverthrowController controller = GetController();
		if (!controller) return null;

		return OVT_UprisingRequestComponent.Cast(controller.FindComponent(OVT_UprisingRequestComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Convenience method to get the illegal-action (wanted escalation) relay component
	//! \return Illegal action component or null
	static OVT_IllegalActionComponent GetIllegalActions()
	{
		OVT_OverthrowController controller = GetController();
		if (!controller) return null;

		return OVT_IllegalActionComponent.Cast(controller.FindComponent(OVT_IllegalActionComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Convenience method to get the recruit command relay component
	//!
	//! CLIENT-ONLY, like every other accessor built on GetController(), and null until
	//! OVT_OverthrowController.RpcDo_NotifyOwnerAssignment has registered this player's controller -
	//! an ASYNC step, so a caller that runs on the first frame of a spawn or a join can legitimately
	//! see null here and must guard rather than assume. Null forever on a dedicated server.
	//! \return Recruit command component or null
	static OVT_RecruitCommandComponent GetRecruitCommands()
	{
		OVT_OverthrowController controller = GetController();
		if (!controller) return null;

		return OVT_RecruitCommandComponent.Cast(controller.FindComponent(OVT_RecruitCommandComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Convenience method to get the local player's tutorial delivery component
	//!
	//! Null until OVT_OverthrowController.RpcDo_NotifyOwnerAssignment has registered the controller,
	//! and null forever on a dedicated server. Every caller must guard - a dropped client-local
	//! trigger is acceptable, a script error is not.
	//! \return Tutorial component or null
	static OVT_TutorialComponent GetTutorials()
	{
		OVT_OverthrowController controller = GetController();
		if (!controller) return null;

		return OVT_TutorialComponent.Cast(controller.FindComponent(OVT_TutorialComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Convenience method for battlefield looting
	//! \param[in] vehicle Target vehicle to loot into
	//! \param[in] searchRadius Search radius for lootable items
	static void LootBattlefield(IEntity vehicle, float searchRadius = 25.0)
	{
		OVT_ContainerTransferComponent transfer = GetContainerTransfer();
		if (transfer && transfer.IsAvailable())
		{
			transfer.LootBattlefield(vehicle, searchRadius);
		}
	}
	
	static OVT_OverthrowGameMode GetOverthrow()
	{
		return OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
	}
	
	static OVT_OverthrowConfigComponent GetConfig()
	{
		return OVT_OverthrowConfigComponent.GetInstance();
	}
	
	static OVT_DifficultySettings GetDifficulty()
	{
		return GetConfig().m_Difficulty;
	}
	
	static OVT_EconomyManagerComponent GetEconomy()
	{
		return OVT_EconomyManagerComponent.GetInstance();
	}
	
	static OVT_PlayerManagerComponent GetPlayers()
	{
		return OVT_PlayerManagerComponent.GetInstance();
	}
	
	static OVT_RealEstateManagerComponent GetRealEstate()
	{
		return OVT_RealEstateManagerComponent.GetInstance();
	}
	
	static OVT_VehicleManagerComponent GetVehicles()
	{
		return OVT_VehicleManagerComponent.GetInstance();
	}
	
	static OVT_TownManagerComponent GetTowns()
	{
		return OVT_TownManagerComponent.GetInstance();
	}
	
	static OVT_OccupyingFactionManager GetOccupyingFaction()
	{
		return OVT_OccupyingFactionManager.GetInstance();
	}
	
	static OVT_ResistanceFactionManager GetResistanceFaction()
	{
		return OVT_ResistanceFactionManager.GetInstance();
	}
	
	static OVT_JobManagerComponent GetJobs()
	{
		return OVT_JobManagerComponent.GetInstance();
	}
	
	//------------------------------------------------------------------------------------------------
	//! Registry of every OVT_MapMarkerComponent in the world (bus stops, POIs).
	//! Populated by a client-safe world scan on every machine - no replication, no persistence.
	//! \return Map marker manager or null before the game mode exists
	static OVT_MapMarkerManagerComponent GetMapMarkers()
	{
		return OVT_MapMarkerManagerComponent.GetInstance();
	}

	static OVT_NotificationManagerComponent GetNotify()
	{
		return OVT_NotificationManagerComponent.GetInstance();
	}
	
	static OVT_OverthrowFactionManager GetFactions()
	{
		return OVT_OverthrowFactionManager.Cast(GetGame().GetFactionManager());
	}
	
	static OVT_SkillManagerComponent GetSkills()
	{
		return OVT_SkillManagerComponent.GetInstance();
	}
	
	static OVT_InventoryManagerComponent GetInventory()
	{
		return OVT_InventoryManagerComponent.GetInstance();
	}
	
	static OVT_DeploymentManagerComponent GetDeploymentManager()
	{
		return OVT_DeploymentManagerComponent.GetInstance();
	}
	
	static OVT_RecruitManagerComponent GetRecruits()
	{
		return OVT_RecruitManagerComponent.GetInstance();
	}
	
	static OVT_LoadoutManagerComponent GetLoadouts()
	{
		return OVT_LoadoutManagerComponent.GetInstance();
	}

	//! The server-side tutorial registry and dispatcher. Present on clients too (the game mode entity
	//! exists there), but only ever subscribed and dispatching on the server.
	static OVT_TutorialManagerComponent GetTutorialManager()
	{
		return OVT_TutorialManagerComponent.GetInstance();
	}
	
	static bool PlayerInRange(vector pos, int range)
	{		
		array<int> players = new array<int>;
		PlayerManager mgr = GetGame().GetPlayerManager();
		int numplayers = mgr.GetPlayers(players);
		
		if(numplayers > 0)
		{
			IEntity player;
			DamageManagerComponent dmg;
			foreach(int playerID : players)
			{
				player = mgr.GetPlayerControlledEntity(playerID);				
				if(!player) continue;
				dmg = DamageManagerComponent.Cast(player.FindComponent(DamageManagerComponent));
				if(dmg && dmg.IsDestroyed())
				{
					//Is dead, ignore
					continue;
				}
				float distance = vector.Distance(player.GetOrigin(), pos);
				if(distance < range)
				{
					return true;
				}
			}
		}
		
		return false;
	}
	
	static int NearestPlayer(vector pos)
	{		
		array<int> players = new array<int>;
		PlayerManager mgr = GetGame().GetPlayerManager();
		int numplayers = mgr.GetPlayers(players);
		
		float nearestDist = 9999999;
		int nearest = -1;
		
		if(numplayers > 0)
		{
			IEntity player;
			foreach(int playerID : players)
			{
				player = mgr.GetPlayerControlledEntity(playerID);
				if(!player) continue;
				float distance = vector.Distance(player.GetOrigin(), pos);
				if(distance < nearestDist)
				{
					nearestDist = distance;
					nearest = playerID;
				}
			}
		}
		
		return nearest;
	}
	
	// Static array for spawn point search results
	static ref array<IEntity> s_SpawnPointSearchResults;
	
	static vector FindSafeSpawnPosition(vector pos, vector mins = "-0.5 0 -0.5", vector maxs = "0.5 2 0.5", bool skipSpawnPointSearch = false)
	{
		vector foundPos;
		TryFindSafeSpawnPosition(pos, foundPos, mins, maxs, skipSpawnPointSearch);
		return foundPos;
	}

	//! As FindSafeSpawnPosition, but the failure is REPORTED instead of hidden: when neither an
	//! authored spawn point nor the random search finds a clear spot, foundPos is the input position
	//! unchanged and the return is false, so the caller chooses its own fallback rather than
	//! inheriting a position that is known to collide. FindSafeSpawnPosition's silent return of the
	//! original position is what embedded fast-travelled recruits inside starter-house walls - the
	//! 30 probes all collide inside a wall cavity and the colliding input came back looking like an
	//! answer.
	static bool TryFindSafeSpawnPosition(vector pos, out vector foundPos, vector mins = "-0.5 0 -0.5", vector maxs = "0.5 2 0.5", bool skipSpawnPointSearch = false)
	{
		foundPos = pos;

		// First check for nearby entities with spawn point components (unless skipped for performance)
		if (!skipSpawnPointSearch)
		{
			if (!s_SpawnPointSearchResults)
				s_SpawnPointSearchResults = {};
			
			s_SpawnPointSearchResults.Clear();
			GetGame().GetWorld().QueryEntitiesBySphere(pos, 15, null, FilterSpawnPointEntities, EQueryEntitiesFlags.ALL);
			
			// If we found spawn point components, use the closest one exactly (no ground adjustment)
			if (s_SpawnPointSearchResults.Count() > 0)
			{
				IEntity closestEntity = null;
				float closestDistance = 999999;
				
				// Find the closest spawn point entity
				foreach (IEntity entity : s_SpawnPointSearchResults)
				{
					float distance = vector.Distance(entity.GetOrigin(), pos);
					if (distance < closestDistance)
					{
						closestDistance = distance;
						closestEntity = entity;
					}
				}
				
				if (closestEntity)
				{
					OVT_SpawnPointComponent spawnComp = OVT_SpawnPointComponent.Cast(closestEntity.FindComponent(OVT_SpawnPointComponent));
					if (spawnComp)
					{
						foundPos = spawnComp.GetSpawnPoint();
						return true;
					}
				}
			}
		}

		//a crude and brute-force way to find a spawn position, try to improve this later
		int i = 0;
		
		BaseWorld world = GetGame().GetWorld();
		float ground = world.GetSurfaceY(pos[0],pos[2]);
		
		vector checkpos;
		TraceBox trace;
		while(i < 30)
		{
			i++;
			
			//Get a random vector in a 2m radius sphere centered on pos and above the ground
			checkpos = s_AIRandomGenerator.GenerateRandomPointInRadius(0,2,pos,false);
			checkpos[1] = pos[1] + s_AIRandomGenerator.RandFloatXY(0, 2);
						
			//check if a box on that position collides with anything
			trace = new TraceBox;
			trace.Flags = TraceFlags.ENTS;
			trace.Start = checkpos;
			trace.Mins = mins;
			trace.Maxs = maxs;
			
			float result = world.TracePosition(trace, null);
				
			if (result < 0)
			{
				//collision, try again
				continue;
			}else{
				//no collision, this pos is safe
				foundPos = checkpos;
				return true;
			}
		}

		// Every probe collided - foundPos is still the (colliding) input position
		return false;
	}
	
	//! Find safe vehicle spawn position with rotation
	//! skipAuthoredSpots skips ONLY the parking/vehicle-point query and keeps the road search: the
	//! 15 m sphere answers "whose parking is nearest", not "whose parking is this", so a destination
	//! that merely sits NEAR somebody's authored parking (a player-placed camp beside a house) would
	//! inherit a spot that belongs to the neighbour. Callers who know the destination authors no
	//! vehicle arrival pass true and go straight to the nearest road.
	static bool FindSafeVehicleSpawnPosition(vector pos, out vector position, out vector angles, bool skipSpawnPointSearch = false, bool skipAuthoredSpots = false)
	{
		// First check for nearby entities with parking or vehicle spawn point components (unless skipped for performance)
		if (!skipSpawnPointSearch && !skipAuthoredSpots)
		{
			if (!s_SpawnPointSearchResults)
				s_SpawnPointSearchResults = {};
			
			s_SpawnPointSearchResults.Clear();
			GetGame().GetWorld().QueryEntitiesBySphere(pos, 15, null, FilterVehicleSpawnEntities, EQueryEntitiesFlags.ALL);
			
			// First priority: Find the closest parking component
			if (s_SpawnPointSearchResults.Count() > 0)
			{
				IEntity closestParkingEntity = null;
				IEntity closestSpawnEntity = null;
				float closestParkingDistance = 999999;
				float closestSpawnDistance = 999999;
				
				foreach (IEntity entity : s_SpawnPointSearchResults)
				{
					float distance = vector.Distance(entity.GetOrigin(), pos);
					
					// Check for parking component first (priority)
					OVT_ParkingComponent parkingComp = OVT_ParkingComponent.Cast(entity.FindComponent(OVT_ParkingComponent));
					if (parkingComp && distance < closestParkingDistance)
					{
						closestParkingDistance = distance;
						closestParkingEntity = entity;
					}
					
					// Also check for spawn point component with vehicle spawn points (fallback)
					OVT_SpawnPointComponent spawnComp = OVT_SpawnPointComponent.Cast(entity.FindComponent(OVT_SpawnPointComponent));
					if (spawnComp && spawnComp.HasVehicleSpawnPoints() && distance < closestSpawnDistance)
					{
						closestSpawnDistance = distance;
						closestSpawnEntity = entity;
					}
				}
				
				// Use parking component if available (higher priority)
				if (closestParkingEntity)
				{
					OVT_ParkingComponent parkingComp = OVT_ParkingComponent.Cast(closestParkingEntity.FindComponent(OVT_ParkingComponent));
					if (parkingComp)
					{
						vector parkingMat[4];
						if (parkingComp.GetParkingSpot(parkingMat, OVT_ParkingType.PARKING_CAR, true)) // Skip obstruction check for fast travel
						{
							position = parkingMat[3];
							angles = Math3D.MatrixToAngles(parkingMat);
							return true;
						}
					}
				}
				
				// Fallback to spawn point component with vehicle spawn points
				if (closestSpawnEntity)
				{
					OVT_SpawnPointComponent spawnComp = OVT_SpawnPointComponent.Cast(closestSpawnEntity.FindComponent(OVT_SpawnPointComponent));
					if (spawnComp && spawnComp.GetVehicleSpawnPoint(position, angles))
					{
						return true;
					}
				}
			}
		}
		
		// Nothing authored here. Put the vehicle on the nearest road, facing the way traffic runs, so it
		// arrives somewhere it can be driven away from (BUG-165). Skipped along with the other spatial
		// queries when the caller asked for the cheap path.
		if (!skipSpawnPointSearch)
		{
			if (FindNearestRoadSpawn(pos, ROAD_SPAWN_MAX_DISTANCE, position, angles))
				return true;

			// No road worth walking back from. Ring out around the destination instead - near it, but
			// deliberately NOT on it.
			if (FindVehicleSpawnNear(pos, position, angles))
				return true;
		}

		// Final fallback. skipSpawnPointSearch is forced TRUE here, and that is the BUG-165 fix rather
		// than a tidy-up: passing the caller's flag through let this land on a CHARACTER spawn point.
		// FindSafeSpawnPosition's spawn-point branch returns the closest OVT_SpawnPointComponent's
		// pedestrian point and never looks at the box it was handed, so a camp - which authors four
		// character points and no vehicle points - parked the car 2 m from the tent centre, on top of it.
		position = FindSafeSpawnPosition(pos, "-1.5 0 -3", "1.5 2.5 3", true);
		angles = "0 0 0";
		return false; // Indicates fallback was used
	}

	//! How far Overthrow will reach for a road to put a fast-travelled vehicle on.
	//!
	//! A vehicle wants to arrive somewhere it can drive away from, but it must still arrive at the place
	//! the player asked for. Past this the road belongs to somewhere else and the trip would end in a long
	//! walk back, which is worse than an awkward park.
	static const float ROAD_SPAWN_MAX_DISTANCE = 200.0;

	//------------------------------------------------------------------------------------------------
	//! The nearest road position to a point, and the direction traffic runs along it.
	//!
	//! Unlike FindNearestRoad, which answers a position only, this also derives the road's HEADING from
	//! the polyline segment nearest the point, so a vehicle can be set down pointing along the road
	//! instead of across it.
	//!
	//! Fails closed on everything: no AI world, no road manager, a negative query result, a road with
	//! fewer than two points, or a degenerate segment all return false and leave the caller on its own
	//! fallback. None of this is play-tested engine API with a vanilla call site to copy, so it is written
	//! to give up rather than to guess.
	//! \param[in] center Position to search from.
	//! \param[in] maxDistance Give up beyond this many metres.
	//! \param[out] position A point ON the road, at ground height.
	//! \param[out] angles Yaw/pitch/roll pointing along the direction of travel.
	//! \return True when a road was found within maxDistance.
	static bool FindNearestRoadSpawn(vector center, float maxDistance, out vector position, out vector angles)
	{
		position = center;
		angles = "0 0 0";

		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld)
			return false;

		RoadNetworkManager roadManager = aiWorld.GetRoadNetworkManager();
		if (!roadManager)
			return false;

		BaseRoad foundRoad;
		float distance;
		if (roadManager.GetClosestRoad(center, foundRoad, distance, false) < 0)
			return false;

		if (!foundRoad || distance > maxDistance)
			return false;

		array<vector> points = {};
		foundRoad.GetPoints(points);

		// A direction needs a segment, not a vertex.
		if (points.Count() < 2)
			return false;

		int closestIndex = 0;
		float closestDistance = 999999;
		for (int i = 0; i < points.Count(); i++)
		{
			float pointDistance = vector.Distance(points[i], center);
			if (pointDistance < closestDistance)
			{
				closestDistance = pointDistance;
				closestIndex = i;
			}
		}

		// The segment leaving the closest vertex, or the one arriving at it when that vertex is the end.
		vector segStart = points[closestIndex];
		vector segEnd;
		if (closestIndex + 1 < points.Count())
		{
			segEnd = points[closestIndex + 1];
		}
		else
		{
			segStart = points[closestIndex - 1];
			segEnd = points[closestIndex];
		}

		// Flattened: a vehicle's heading is a compass bearing, and the road's slope is the terrain's job.
		vector direction = segEnd - segStart;
		direction[1] = 0;
		if (direction.Length() < 0.01)
			return false;

		direction.Normalize();

		// Project the destination onto the segment so the vehicle lands at the point of the road NEAREST
		// the place the player asked for, rather than at whichever polyline vertex happened to be closest.
		float segmentLength = vector.Distance(segStart, segEnd);
		float along = vector.Dot(center - segStart, direction);
		if (along < 0)
			along = 0;

		if (along > segmentLength)
			along = segmentLength;

		position = segStart + (direction * along);
		position[1] = GetGame().GetWorld().GetSurfaceY(position[0], position[2]);

		vector roadMat[4];
		Math3D.DirectionAndUpMatrix(direction, vector.Up, roadMat);
		angles = Math3D.MatrixToAngles(roadMat);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! A vehicle-sized clear spot in a ring AROUND a position - near it, deliberately not on it.
	//!
	//! The last resort before the crude random search, and the reason it exists: FindSafeSpawnPosition
	//! only ever samples a 2 m sphere centred on the destination, which for a camp or any other placed
	//! structure means "inside the thing you travelled to". This starts outside the footprint and works
	//! outward, testing a vehicle-sized box at each candidate.
	//!
	//! The vehicle faces AWAY from the centre, so it is pointing at open ground rather than at whatever
	//! it just arrived next to.
	//! \param[in] center The position travelled to.
	//! \param[out] position A clear vehicle-sized spot, at ground height.
	//! \param[out] angles Yaw/pitch/roll facing away from centre.
	//! \return True when a clear spot was found.
	static bool FindVehicleSpawnNear(vector center, out vector position, out vector angles)
	{
		position = center;
		angles = "0 0 0";

		BaseWorld world = GetGame().GetWorld();
		if (!world)
			return false;

		// Start clear of a camp's footprint and widen. Eight bearings is enough to find a gap without
		// turning an arrival into a spatial query storm.
		array<float> radii = {8.0, 12.0, 16.0, 22.0};

		foreach (float radius : radii)
		{
			for (int i = 0; i < 8; i++)
			{
				float angle = i * 45.0 * Math.DEG2RAD;
				vector offset = Vector(Math.Sin(angle) * radius, 0, Math.Cos(angle) * radius);

				vector candidate = center + offset;
				candidate[1] = world.GetSurfaceY(candidate[0], candidate[2]);

				TraceBox trace = new TraceBox;
				trace.Flags = TraceFlags.ENTS;
				trace.Start = candidate;
				trace.Mins = "-1.5 0 -3";
				trace.Maxs = "1.5 2.5 3";

				if (world.TracePosition(trace, null) < 0)
					continue; // collision, try the next bearing

				position = candidate;

				vector facing = offset;
				facing[1] = 0;
				facing.Normalize();

				vector faceMat[4];
				Math3D.DirectionAndUpMatrix(facing, vector.Up, faceMat);
				angles = Math3D.MatrixToAngles(faceMat);

				return true;
			}
		}

		return false;
	}
	
	//! Filter function to find entities with parking or vehicle spawn point components
	static bool FilterVehicleSpawnEntities(IEntity entity)
	{
		// Check for parking component (priority)
		OVT_ParkingComponent parkingComp = OVT_ParkingComponent.Cast(entity.FindComponent(OVT_ParkingComponent));
		if (parkingComp)
		{
			s_SpawnPointSearchResults.Insert(entity);
			return false;
		}
		
		// Check for spawn point component with vehicle spawn points
		OVT_SpawnPointComponent spawnComp = OVT_SpawnPointComponent.Cast(entity.FindComponent(OVT_SpawnPointComponent));
		if (spawnComp && spawnComp.HasVehicleSpawnPoints())
		{
			s_SpawnPointSearchResults.Insert(entity);
		}
		return false;
	}
	
	//! Filter function to find entities with spawn point components
	static bool FilterSpawnPointEntities(IEntity entity)
	{
		OVT_SpawnPointComponent spawnComp = OVT_SpawnPointComponent.Cast(entity.FindComponent(OVT_SpawnPointComponent));
		if (spawnComp)
		{
			s_SpawnPointSearchResults.Insert(entity);
		}
		return false;
	}
	
	static void TransferToWarehouse(RplId from)
	{
		OVT_RealEstateManagerComponent realEstate = GetRealEstate();
		IEntity fromEntity = RplComponent.Cast(Replication.FindItem(from)).GetEntity();
		
		if(!fromEntity) return;
		
		InventoryStorageManagerComponent fromStorage = InventoryStorageManagerComponent.Cast(fromEntity.FindComponent(InventoryStorageManagerComponent));		
				
		if(!fromStorage) return;
		
		OVT_WarehouseData warehouse = realEstate.GetNearestWarehouse(fromEntity.GetOrigin(), 50);
		if(!warehouse) return;
						
		array<IEntity> items = new array<IEntity>;
		fromStorage.GetItems(items);
		if(items.Count() == 0) return;
		
		map<ResourceName,int> collated = new map<ResourceName,int>;
				
		foreach(IEntity item : items)
		{
			if(!item) continue;			
			ResourceName res = OVT_Global.GetPrefabName(item);
			if(fromStorage.TryDeleteItem(item))
			{			
				if(!collated.Contains(res)) collated[res] = 0;
				collated[res] = collated[res] + 1;
			}
		}
		
		for(int i=0; i<collated.Count(); i++)
		{
			ResourceName res = collated.GetKey(i);
			realEstate.AddToWarehouse(warehouse, res, collated[res]);
		}
		
		// Play sound if one is defined
		SimpleSoundComponent simpleSoundComp = SimpleSoundComponent.Cast(fromEntity.FindComponent(SimpleSoundComponent));
		if (simpleSoundComp)
		{
			vector mat[4];
			fromEntity.GetWorldTransform(mat);
			
			simpleSoundComp.SetTransformation(mat);
			simpleSoundComp.PlayStr("LOAD_VEHICLE");
		}	
	}
	
	static void TakeFromWarehouseToVehicle(int warehouseId, string id, int qty, RplId from)
	{
		IEntity fromEntity = RplComponent.Cast(Replication.FindItem(from)).GetEntity();		
		if(!fromEntity) return;
		
		InventoryStorageManagerComponent fromStorage = InventoryStorageManagerComponent.Cast(fromEntity.FindComponent(InventoryStorageManagerComponent));				
		if(!fromStorage) return;
		
		OVT_RealEstateManagerComponent re = OVT_Global.GetRealEstate();
		
		OVT_WarehouseData warehouse = re.m_aWarehouses[warehouseId];
		
		if(warehouse.inventory[id] < qty) qty = warehouse.inventory[id];
		int actual = 0;
		
		for(int i = 0; i < qty; i++)
		{
			if(fromStorage.TrySpawnPrefabToStorage(id))
			{
				actual++;
			}
		}
		
		re.TakeFromWarehouse(warehouse, id, actual);
	}
	
	static IEntity SpawnEntityPrefab(ResourceName prefab, vector origin, vector orientation = "0 0 0", bool global = true)
	{
		EntitySpawnParams spawnParams();

		spawnParams.TransformMode = ETransformMode.WORLD;

		Math3D.AnglesToMatrix(orientation, spawnParams.Transform);
		spawnParams.Transform[3] = origin;

		IEntity entity;
		if (global)
			entity = GetGame().SpawnEntityPrefab(Resource.Load(prefab), GetGame().GetWorld(), spawnParams);
		else
			entity = GetGame().SpawnEntityPrefabLocal(Resource.Load(prefab), GetGame().GetWorld(), spawnParams);

		// Waypoints are session-scoped by construction - every one Overthrow spawns is rebuilt with
		// its group on the next boot, so a persistence record for one is a permanent orphan (BUG-118).
		if (AIWaypoint.Cast(entity))
			OVT_PersistenceManagerComponent.UntrackTransient(entity);

		return entity;
	}

	static IEntity SpawnEntityPrefabMatrix(ResourceName prefab, vector mat[4], bool global = true)
	{
		EntitySpawnParams spawnParams();

		spawnParams.TransformMode = ETransformMode.WORLD;
		spawnParams.Transform = mat;

		IEntity entity;
		if (global)
			entity = GetGame().SpawnEntityPrefab(Resource.Load(prefab), GetGame().GetWorld(), spawnParams);
		else
			entity = GetGame().SpawnEntityPrefabLocal(Resource.Load(prefab), GetGame().GetWorld(), spawnParams);

		// Same waypoint rule as SpawnEntityPrefab() above (BUG-118).
		if (AIWaypoint.Cast(entity))
			OVT_PersistenceManagerComponent.UntrackTransient(entity);

		return entity;
	}
	
	//! Clear the aiming state a player leaves on a character after possessing it (BUG-147).
	//! Releasing a possessed AI never resets the player-driven aim, which pins the character's
	//! body yaw forever - the AI's only aim-stop lives in LookAction.bt and never fires for an
	//! aim it did not start, so the recruit walks backwards staring at wherever the player last
	//! looked. Server-side, call after SetPossessedEntity(null) with the ex-possessed entity.
	static void ResetAIAimState(IEntity character)
	{
		if (!character)
			return;

		SCR_CharacterControllerComponent controller = SCR_CharacterControllerComponent.Cast(character.FindComponent(SCR_CharacterControllerComponent));
		if (controller)
		{
			controller.SetWeaponRaised(false);
			controller.SetForcedFreeLook(false);
			controller.ResetPersistentStates(true, true);

			AimingComponent aiming = controller.GetAimingComponent();
			if (aiming)
			{
				aiming.SetAimingRotationWanted(vector.Zero);
				aiming.SetAimingRotation(vector.Zero);
			}
		}

		// Reset the AI-side look bookkeeping too, so the next look request starts clean
		AIControlComponent aiControl = AIControlComponent.Cast(character.FindComponent(AIControlComponent));
		if (aiControl)
		{
			AIAgent agent = aiControl.GetAIAgent();
			if (agent)
			{
				SCR_AIUtilityComponent utility = SCR_AIUtilityComponent.Cast(agent.FindComponent(SCR_AIUtilityComponent));
				if (utility && utility.m_LookAction)
				{
					utility.m_LookAction.Cancel();
					utility.m_LookAction.Complete();
				}
			}
		}
	}

	//! Spawn a character entity directly without creating a group
	static SCR_ChimeraCharacter SpawnCharacterEntity(ResourceName prefab, vector origin, vector orientation = "0 0 0")
	{
		EntitySpawnParams spawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		
		Math3D.AnglesToMatrix(orientation, spawnParams.Transform);
		spawnParams.Transform[3] = origin;
		
		// Load the prefab resource
		Resource resource = Resource.Load(prefab);
		if (!resource)
		{
			Print("[Overthrow] Error: Could not load prefab resource: " + prefab);
			return null;
		}
		
		// Spawn the entity directly
		IEntity spawnedEntity = GetGame().SpawnEntityPrefab(resource, GetGame().GetWorld(), spawnParams);
		if (!spawnedEntity)
		{
			Print("[Overthrow] Error: Failed to spawn entity from prefab: " + prefab);
			return null;
		}
		
		// Cast to character and return
		SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(spawnedEntity);
		if (!character)
		{
			Print("[Overthrow] Error: Spawned entity is not a SCR_ChimeraCharacter: " + spawnedEntity);
			// Clean up the spawned entity since it's not what we expected
			SCR_EntityHelper.DeleteEntityAndChildren(spawnedEntity);
			return null;
		}
		
		return character;
	}
	
	static bool IsOceanAtPosition(vector checkpos)
	{		
		World world = GetGame().GetWorld();
		return 1 > world.GetSurfaceY(checkpos[0],checkpos[2]);
		return world.GetOceanBaseHeight() > world.GetSurfaceY(checkpos[0],checkpos[2]);
	}
	
	static vector GetRandomNonOceanPositionNear(vector pos, float range)
	{
		int i = 0;
		vector checkpos;
		BaseWorld world = GetGame().GetWorld();
		vector worldMin, worldMax;
		world.GetBoundBox(worldMin, worldMax);

		while(i < 30)
		{
			i++;

			checkpos = s_AIRandomGenerator.GenerateRandomPointInRadius(0,range,pos,false);

			// Keep the point on the map, or AI given a waypoint here can never reach it
			if(checkpos[0] < worldMin[0]) checkpos[0] = worldMin[0];
			if(checkpos[0] > worldMax[0]) checkpos[0] = worldMax[0];
			if(checkpos[2] < worldMin[2]) checkpos[2] = worldMin[2];
			if(checkpos[2] > worldMax[2]) checkpos[2] = worldMax[2];

			if(!OVT_Global.IsOceanAtPosition(checkpos))
			{
				checkpos[1] = world.GetSurfaceY(checkpos[0],checkpos[2]) + 1;
				return checkpos;
			}
		}

		return pos;
	}
	
	static bool GetNearbyBodiesAndWeapons(vector pos, int range, out array<IEntity> entities)
	{
		m_Bodies = new array<IEntity>;
		GetGame().GetWorld().QueryEntitiesBySphere(pos, range, FilterDeadBodiesAndWeapons);
		entities.InsertAll(m_Bodies);
		
		return true;
	}
	
	protected static ref array<IEntity> m_Bodies;
	
	protected static bool FilterDeadBodiesAndWeapons(IEntity ent)
	{		
		DamageManagerComponent dmg = OVT_ComponentFinder<DamageManagerComponent>.Find(ent);
		if(dmg && dmg.IsDestroyed())
		{
			m_Bodies.Insert(ent);
			return true;
		}
		
		WeaponComponent weapon = OVT_ComponentFinder<WeaponComponent>.Find(ent);
		if(weapon) m_Bodies.Insert(ent);
				
		return true;
	}
	
	static SCR_EditableVehicleUIInfo GetVehicleUIInfo(ResourceName res)
	{
		Resource holder = BaseContainerTools.LoadContainer(res);
		if (holder)
		{
			IEntitySource ent = holder.GetResource().ToEntitySource();
			for(int t=0; t<ent.GetComponentCount(); t++)
			{
				IEntityComponentSource comp = ent.GetComponent(t);
				if(comp.GetClassName() == "SCR_EditableVehicleComponent")
				{
					SCR_EditableVehicleUIInfo info;
					comp.Get("m_UIInfo",info);
					return info;
				}
			}
		}
		return null;
	}
	
	static SCR_EditableEntityUIInfo GetEditableUIInfo(ResourceName res)
	{
		Resource holder = BaseContainerTools.LoadContainer(res);
		if (holder)
		{
			IEntitySource ent = holder.GetResource().ToEntitySource();
			for(int t=0; t<ent.GetComponentCount(); t++)
			{
				IEntityComponentSource comp = ent.GetComponent(t);
				if(comp.GetClassName() == "SCR_EditableVehicleComponent")
				{
					SCR_EditableEntityUIInfo info;
					comp.Get("m_UIInfo",info);
					return info;
				}
			}
		}
		return null;
	}
	
	static UIInfo GetItemUIInfo(ResourceName prefab)
	{
		IEntitySource entitySource = SCR_BaseContainerTools.FindEntitySource(Resource.Load(prefab));
		if (entitySource)
		{
		    for(int nComponent, componentCount = entitySource.GetComponentCount(); nComponent < componentCount; nComponent++)
		    {
		        IEntityComponentSource componentSource = entitySource.GetComponent(nComponent);
		        if(componentSource.GetClassName().ToType().IsInherited(InventoryItemComponent))
		        {
		            BaseContainer attributesContainer = componentSource.GetObject("Attributes");
		            if (attributesContainer)
		            {
		                BaseContainer itemDisplayNameContainer = attributesContainer.GetObject("ItemDisplayName");
		                if (itemDisplayNameContainer)
		                {
		                    UIInfo resultInfo = UIInfo.Cast(BaseContainerTools.CreateInstanceFromContainer(itemDisplayNameContainer));
		                    return resultInfo;
		                }
		            }
		        }
		    }
		}
		return null;
	}
	
	//! call RandomizeCivilianClothes for whole group
	static void RandomizeCivilianGroupClothes(SCR_AIGroup aigroup)
	{
		array<AIAgent> civs = new array<AIAgent>;
		aigroup.GetAgents(civs);
		foreach(AIAgent agent : civs)
		{
			RandomizeCivilianClothes(agent);
		}
	}
	
	//! Randomize clothes for civilian. Accounts for config properties like SkipChance and PlayerOnly.
	static void RandomizeCivilianClothes(AIAgent agent)
	{
		IEntity civ = agent.GetControlledEntity();
		ApplyCivilianLoadout(civ);
	}
	
	//! Apply civilian loadout to any character entity
	static void ApplyCivilianLoadout(IEntity character)
	{
		InventoryStorageManagerComponent storageManager = OVT_ComponentFinder<InventoryStorageManagerComponent>.Find(character);
		if (!storageManager)
			return;
		foreach (OVT_LoadoutSlot loadoutItem : OVT_Global.GetConfig().m_CivilianLoadout.m_aSlots)
		{
			if (loadoutItem.m_bPlayerOnly) continue;
			
			if (loadoutItem.m_fSkipChance > 0)
			{
				float rnd = s_AIRandomGenerator.RandFloat01();
				if(rnd <= loadoutItem.m_fSkipChance) continue; 
			}
			
			IEntity slotEntity = OVT_Global.SpawnDefaultCharacterItem(storageManager, loadoutItem);
			if (!slotEntity) continue;
			
			array<BaseInventoryStorageComponent> storages = new array<BaseInventoryStorageComponent>;
			storageManager.GetStorages(storages, EStoragePurpose.PURPOSE_LOADOUT_PROXY);
			
			BaseInventoryStorageComponent loadoutStorage;
			int suitableSlotId = -1;
			if (!storages.IsEmpty()) {
				loadoutStorage = storages[0];
				InventoryStorageSlot suitableSlot = loadoutStorage.FindSuitableSlotForItem(slotEntity);
				if (suitableSlot) {
					suitableSlotId = suitableSlot.GetID();
				}
			}
			
			if (!loadoutStorage || suitableSlotId == -1 || !storageManager.TryReplaceItem(slotEntity, loadoutStorage, suitableSlotId))
			{
				SCR_EntityHelper.DeleteEntityAndChildren(slotEntity);
			}
		}
	}
	
	static IEntity SpawnDefaultCharacterItem(InventoryStorageManagerComponent storageManager, OVT_LoadoutSlot loadoutItem)
	{
		int selection = s_AIRandomGenerator.RandInt(0, loadoutItem.m_aChoices.Count());
		ResourceName prefab = loadoutItem.m_aChoices[selection];
		
		EntitySpawnParams spawnParams();
		spawnParams.Transform[3] = storageManager.GetOwner().GetOrigin();
		
		IEntity slotEntity = GetGame().SpawnEntityPrefab(Resource.Load(prefab), GetGame().GetWorld(), spawnParams);
		if (!slotEntity) return null;
		
		return slotEntity;
	}
	
	//! Centralized method to show hints throughout Overthrow
	static void ShowHint(string text)
	{
		SCR_HintManagerComponent hintManager = SCR_HintManagerComponent.GetInstance();
		if (hintManager)
			hintManager.ShowCustom(text);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Find the nearest road position to a given location
	//! @param center Starting position to search from
	//! @param searchRadius Maximum distance to search for roads
	//! @return Position on nearest road, or original position if no road found
	static vector FindNearestRoad(vector center)
	{
		SCR_AIWorld aiWorld = SCR_AIWorld.Cast(GetGame().GetAIWorld());
		if (!aiWorld)
			return center;
			
		RoadNetworkManager roadManager = aiWorld.GetRoadNetworkManager();
		if (!roadManager)
			return center;
			
		BaseRoad foundRoad;
		float distance;
		int result = roadManager.GetClosestRoad(center, foundRoad, distance, false);
				
		if (result >= 0 && foundRoad && foundRoad.GetWidth() > 0)
		{
			// Try to get a reachable waypoint on the road
			vector roadPos;
			if (roadManager.GetReachableWaypointInRoad(center, center, 500, roadPos))
				return roadPos;
		}
		
		// If no road found within range, return original position
		return center;
	}
}