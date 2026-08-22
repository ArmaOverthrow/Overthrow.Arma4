class OVT_Global : Managed
{
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

	//------------------------------------------------------------------------------------------------
	//! CLIENT-ONLY. The UI manager on the local player's controlled character.
	//!
	//! Null-guarded because a dead, pre-spawn or start-menu player controls NOTHING: the unguarded
	//! version dereferenced GetLocalControlledEntity() directly and was a guaranteed VME with no body,
	//! which is exactly the state the menus this serves are opened in. Callers must null-check.
	//! \return The UI manager, or null when there is no local controlled entity / no such component.
	static OVT_UIManagerComponent GetUI()
	{
		IEntity player = SCR_PlayerController.GetLocalControlledEntity();
		if (!player) return null;

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
	//! FAST PATH ONLY for GetController(), never the truth on its own.
	//!
	//! Set by OVT_OverthrowController.RpcDo_NotifyOwnerAssignment, i.e. only on the machine that OWNS
	//! that controller. That includes a single-player session and (inferred from the engine routing
	//! table, see the corrected note on RpcDo_NotifyOwnerAssignment) a listen host, where the owner is
	//! the sending machine and the engine invokes the RPC body directly - the older claim here, that
	//! this field is "null forever" on those machines, was wrong.
	//!
	//! It is still a cache in front of the lookup and NEVER a replacement for it (decision D9): it is
	//! unset before ownership is assigned, and any machine that is not the owner never sets it at all.
	static OVT_OverthrowController s_LocalController;

	//------------------------------------------------------------------------------------------------
	//! Records the local player's controller as it is assigned. Idempotent by construction - a plain
	//! field write - which matters because ownership assignment fires once per ASSIGNMENT, not once per
	//! player (reconnect and Continue both re-assign; see OVT_OverthrowController).
	//! \param[in] controller The controller entity this machine's player owns.
	static void SetLocalController(OVT_OverthrowController controller)
	{
		s_LocalController = controller;
	}

	//------------------------------------------------------------------------------------------------
	//! Get the local player's overthrow controller entity
	//!
	//! The cached assignment is tried first and is validated on every read (non-null AND not deleted) -
	//! a controller entity has its own lifetime and is destroyed on disconnect/cleanup, so a stale
	//! handle here would outlive the entity it names.
	//!
	//! The controlled-entity route is kept first among the lookups and unchanged so the living path
	//! cannot regress. The fallback exists because a dead player awaiting respawn may control nothing,
	//! and without it every controller component - and therefore every client->server request - is
	//! unreachable while dead. Neither may be removed: the cache is empty until ownership is assigned,
	//! and on any machine that does not own the controller it is never populated at all.
	//! \return Controller entity or null if not found/on server
	static OVT_OverthrowController GetController()
	{
		if (s_LocalController)
		{
			if (!s_LocalController.IsDeleted())
				return s_LocalController;

			s_LocalController = null;
		}

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
	
	static OVT_OverthrowGameMode GetOverthrow()
	{
		return OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
	}
	
	static OVT_OverthrowConfigComponent GetConfig()
	{
		return OVT_OverthrowConfigComponent.GetInstance();
	}
	
	//------------------------------------------------------------------------------------------------
	//! The live difficulty settings.
	//!
	//! Null-guarded on BOTH steps. GetConfig() is null before the config component exists, and on a
	//! CLIENT m_Difficulty stays null until the config's RplLoad lands - so the sub-object needs the
	//! guard as much as the accessor does, and the unguarded version was a VME in exactly the window a
	//! joining client spends reading difficulty values.
	//!
	//! BUG-078 still applies to what this returns on a client: several fields may hold prefab defaults
	//! rather than the server's values. Server-authoritative checks must read the SERVER's copy.
	//! \return The difficulty settings, or null before the config (or its difficulty object) exists.
	static OVT_DifficultySettings GetDifficulty()
	{
		OVT_OverthrowConfigComponent config = GetConfig();
		if (!config) return null;

		return config.m_Difficulty;
	}
	
	static OVT_EconomyManagerComponent GetEconomy()
	{
		return OVT_EconomyManagerComponent.GetInstance();
	}
	
	static OVT_ResourceManagerComponent GetResources()
	{
		return OVT_ResourceManagerComponent.GetInstance();
	}

	static OVT_ResourceProductionManagerComponent GetProduction()
	{
		return OVT_ResourceProductionManagerComponent.GetInstance();
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

	//------------------------------------------------------------------------------------------------
	//! The occupying faction's objective director - what it has decided to take back, and how far
	//! along the ramp it is.
	//! Present on clients too (the game mode entity exists there), but every entry point on it is
	//! server-gated: the objective is a server-side decision and nothing about it replicates except
	//! two read-only rows on the GM panel.
	//! \return The objective director, or null before the game mode exists.
	static OVT_ObjectiveDirectorComponent GetObjectiveDirector()
	{
		return OVT_ObjectiveDirectorComponent.GetInstance();
	}

	//------------------------------------------------------------------------------------------------
	//! The server-side registry of virtualized AI groups and ambient spawn sources.
	//! Present on clients too (the game mode entity exists there), but every entry point on it is
	//! server-gated - nothing in virtualization has a client half.
	//! \return The virtualization manager, or null before the game mode exists.
	static OVT_VirtualizationManagerComponent GetVirtualization()
	{
		return OVT_VirtualizationManagerComponent.GetInstance();
	}

	//------------------------------------------------------------------------------------------------
	//! Walks dormant virtualized groups along their own waypoint plans, on top of the virtualization
	//! registry. Present on clients too (the game mode entity exists there), but it is server-only:
	//! nothing about virtual movement replicates or is persisted, and a client's state map is never
	//! even allocated.
	//! \return The virtual movement manager, or null before the game mode exists.
	static OVT_VirtualMovementManagerComponent GetVirtualMovement()
	{
		return OVT_VirtualMovementManagerComponent.GetInstance();
	}

	//------------------------------------------------------------------------------------------------
	//! Town civilian ambience - one ambient spawn source per town, on top of the virtualization seam.
	//! Present on clients too (the game mode entity exists there), but every entry point on it is
	//! server-gated: nothing about an ambient civilian replicates or is persisted.
	//! \return The civilian ambience manager, or null before the game mode exists.
	static OVT_CivilianAmbienceManagerComponent GetCivilianAmbience()
	{
		return OVT_CivilianAmbienceManagerComponent.GetInstance();
	}

	static OVT_RecruitManagerComponent GetRecruits()
	{
		return OVT_RecruitManagerComponent.GetInstance();
	}

	static OVT_HighCommandManagerComponent GetHighCommand()
	{
		return OVT_HighCommandManagerComponent.GetInstance();
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

	//------------------------------------------------------------------------------------------------
	//! Forwarder to OVT_WorldUtils.PlayerInRange(). Kept on the locator because of call-site volume only.
	static bool PlayerInRange(vector pos, int range)
	{
		return OVT_WorldUtils.PlayerInRange(pos, range);
	}

	//------------------------------------------------------------------------------------------------
	//! Forwarder to OVT_WorldUtils.SpawnEntityPrefab(). Kept on the locator because of call-site volume only.
	static IEntity SpawnEntityPrefab(ResourceName prefab, vector origin, vector orientation = "0 0 0", bool global = true)
	{
		return OVT_WorldUtils.SpawnEntityPrefab(prefab, origin, orientation, global);
	}

	//------------------------------------------------------------------------------------------------
	//! Forwarder to OVT_PrefabUtils.GetPrefabName(). Kept on the locator because of call-site volume only.
	static ResourceName GetPrefabName(IEntity entity)
	{
		return OVT_PrefabUtils.GetPrefabName(entity);
	}
	
	//! Centralized method to show hints throughout Overthrow
	static void ShowHint(string text)
	{
		SCR_HintManagerComponent hintManager = SCR_HintManagerComponent.GetInstance();
		if (hintManager)
			hintManager.ShowCustom(text);
	}
}
