//------------------------------------------------------------------------------------------------
//! Spawn logic for Overthrow game mode.
//! This class contains the actual spawn logic and is referenced by OVT_RespawnSystemComponent.
//!
//! IT EXTENDS VANILLA SCR_SpawnLogic DIRECTLY. It used to extend the old persistence framework's base
//! spawn logic, which supplied three things Overthrow depended on. All three are now implemented here,
//! on vanilla paths:
//!
//!   1. OnPlayerAuditSuccess_S -> ExcuteInitialLoadOrSpawn_S. Vanilla's OnPlayerAuditSuccess_S only
//!      assigns the player controller's persistence id; nothing else in vanilla starts a spawn for a
//!      game mode that does not use a respawn menu. Without this call DoSpawn_S never runs at all.
//!   2. Character CREATION (CreateCharacter/HandoverToPlayer). Overthrow owns player identity and setup,
//!      so vanilla's controller-driven load half stays switched off (SetupPersistenceCollections() below
//!      is a no-op) - but the character half is HERE, in Overthrow's own shape: a player whose record
//!      names a stored body gets THAT body back, gear and position intact
//!      (RequestPersistedPlayerBody -> OnPlayerBodySpawned -> AttachRestoredPlayerBody), and everything
//!      else falls through to CreateFreshCharacter. Nothing self-spawns a player character, which is
//!      exactly why the spawn logic has to ask.
//!   3. Respawn after death. The old base re-created the character one frame after OnPlayerKilled_S.
//!      Overthrow no longer does: death now DEFERS creation until the player has chosen where to be
//!      put (OnPlayerKilled_S -> BeginAwaitingRespawn -> CompleteRespawn), and it CLEARS the stored
//!      body id so the eventual rebuild can only ever be a fresh civilian one. What is NOT ported is
//!      the old base's dead-body dance (fresh id for the corpse, force self-spawn): the game mode's
//!      kill hook marks the corpse's config to self-spawn instead - see
//!      OVT_PersistenceTracking.MarkForSelfSpawn.
//!
//! A CONSEQUENCE OF THAT DEFERRAL WORTH KNOWING BEFORE EDITING ANYTHING HERE: a dead player keeps
//! control of their CORPSE for as long as the respawn screen is up, which is minutes rather than the
//! single frame it used to be. Several guards in this file ask whether a player already has a
//! controlled entity; each is written knowing that the honest question is "do they have a character
//! that can still play", and a corpse is not one.
[BaseContainerProps(category: "Respawn")]
class OVT_SpawnLogic : SCR_SpawnLogic
{
	[Attribute(defvalue: "{3A99A99836F6B3DC}Prefabs/Characters/Factions/INDFOR/FIA/Character_Player.et")]
	ResourceName m_rDefaultPrefab;

	//! The live instance, set in OnInit - the game mode drives deferred character creation through it
	//! (manager-singleton idiom; overwritten on every world init, never self-healing).
	protected static OVT_SpawnLogic s_Instance;

	//------------------------------------------------------------------------------------------------
	//! \return The world's spawn logic instance, or null before the respawn system initializes.
	static OVT_SpawnLogic GetInstance()
	{
		return s_Instance;
	}

	//------------------------------------------------------------------------------------------------
	override void OnInit(SCR_RespawnSystemComponent owner)
	{
		super.OnInit(owner);
		s_Instance = this;
	}

	protected ref array<IEntity> m_FoundBases = {};

	//! Players whose stored body is being fetched from the persistence system right now.
	//!
	//! PersistenceSystem.RequestSpawn() is ASYNCHRONOUS, so between the request and its callback a player
	//! has neither a character in the world nor a request that anything can see. Without this list a
	//! second CreateCharacter() for the same player - the audit retry does exactly that - would start a
	//! second request and end up handing over two characters.
	protected ref array<int> m_aPendingBodySpawns = {};

	//! Players who are dead and have not yet chosen where their next character is created.
	//!
	//! THE SAME ONE-SHOT CLAIM AS m_aPendingBodySpawns ABOVE, for the same reason: the entry is the
	//! right to be spawned once, and CompleteRespawn() consumes it BEFORE it builds anything. A
	//! duplicate request, a late packet, or a request from a player who is not dead finds no entry and
	//! returns without spawning, so one death can only ever produce one character.
	//!
	//! Keyed on the RUNTIME player id, which a later joiner can inherit - so the entry has to be
	//! dropped on disconnect (OnPlayerDisconnected_S) rather than left to expire.
	//!
	//! Server-side, per player, consumed on use and gone when the world ends. There is nothing here a
	//! client can leave armed and nothing here to read stale.
	protected ref array<int> m_aAwaitingRespawn = {};

	//! The persistence collection player bodies belong to, resolved once and cached.
	//!
	//! DELIBERATELY NOT SCR_SpawnLogic.m_CharacterCollection. That field is vanilla's switch for routing
	//! player spawn through an asynchronous RequestLoad on the player CONTROLLER, and Overthrow's
	//! SetupPersistenceCollections() override leaves it null precisely to keep RequestPlayerData_S on its
	//! synchronous path - populating it would stop DoSpawn_S/SetupPlayer from ever running (see the
	//! override below). This is a separate handle used only for character spawn requests.
	//!
	//! Held without `ref` on purpose: the collection is owned by the persistence system and is only
	//! obtainable from it (PersistenceCollection is sealed with a private constructor).
	protected PersistenceCollection m_PlayerBodyCollection;

	//! Name of the collection player bodies are stored in.
	//!
	//! VERIFIED, not assumed. Overthrow's player prefab inherits Prefabs/Characters/Core/Character_Base.et,
	//! which carries the native Persistence component, and is matched by vanilla's character configuration
	//! chain. Vanilla Common.conf:88-90 and :95-97 both put those configs in collection
	//! {64EACAC5B77ED31B}, whose Name is "Character" (Common.conf:7-10) - the same collection
	//! SCR_SpawnLogic.c:369 fetches a returning player's character from.
	static const string PLAYER_BODY_COLLECTION = "Character";

	//! How long to wait for a spawn request to answer before giving the player a fresh body (ms).
	static const int BODY_SPAWN_TIMEOUT_MS = 15000;

	//! Poll interval while waiting for a continued session to finish reading its saved data (ms).
	static const int PERSISTED_DATA_WAIT_MS = 250;

	//! How many times to poll before spawning anyway. 20 x 250 ms = 5 s, after which a player is given a
	//! fresh character rather than left staring at a loading screen.
	static const int PERSISTED_DATA_MAX_WAITS = 20;

	//! How often to re-ask a still-choosing player's machine to put the respawn screen up (ms).
	//!
	//! DELIBERATELY NOT A TIMEOUT, and the distinction is the whole design. Nothing counts these down
	//! and nothing spawns anybody when they run out - the tick exists so that a lost show RPC, or a
	//! client whose controller had not registered yet when it died, still ends up with a screen. The
	//! player waits as long as they like and is never spawned somewhere they did not pick.
	static const int RESPAWN_REASK_MS = 5000;

	//! Event fired when a player group is created
	//! Parameters: playerId, groupId, playerName
	ref ScriptInvoker m_OnPlayerGroupCreated = new ScriptInvoker();

	//------------------------------------------------------------------------------------------------
	//! Overthrow owns player identity and spawn setup (SetupPlayer + manager serializers). Leaving the
	//! vanilla Player/Character collections unresolved keeps SCR_SpawnLogic.RequestPlayerData_S on its
	//! synchronous no-persistence path; without this, an ACTIVE persistence system reroutes player spawn
	//! through an async RequestLoad and DoSpawn_S/SetupPlayer never runs.
	override protected void SetupPersistenceCollections(SCR_RespawnSystemComponent owner)
	{
	}

	//------------------------------------------------------------------------------------------------
	//! Starts the spawn sequence once the joining player has been audited.
	//! Vanilla's implementation stops after registering the controller with persistence, so the spawn
	//! kick-off has to be added here (the old persistence-framework base did exactly this).
	override void OnPlayerAuditSuccess_S(int playerId)
	{
		super.OnPlayerAuditSuccess_S(playerId);
		ExcuteInitialLoadOrSpawn_S(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Gets the default player character prefab resource name.
	protected ResourceName GetCreationPrefab(int playerId, string characterPersistenceId)
	{
		return m_rDefaultPrefab;
	}

	//------------------------------------------------------------------------------------------------
	//! Called when spawning a player.
	protected override void DoSpawn_S(int playerId)
	{
		Print("[Overthrow] OVT_SpawnLogic.DoSpawn_S called for playerId: " + playerId);

		string playerUid = OVT_Global.GetPlayerUID(playerId);
		if (!playerUid)
		{
			Print("[Overthrow] WARNING: Persistent UID not available yet for playerId: " + playerId + ", retrying...", LogLevel.WARNING);
			OnPlayerRegisterFailed(playerId);
			return;
		}

		OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());

		// Always setup player data (creates OVT_PlayerData object needed for character creation)
		Print("[Overthrow] Setting up player data for: " + playerUid);
		OVT_Global.GetPlayers().SetupPlayer(playerId, playerUid);

		// Only do full preparation if game has already started (loaded save or dedicated server)
		// For new games, full preparation happens when the user clicks "Start Game".
		//
		// THE CHARACTER IS ALWAYS CREATED IMMEDIATELY, even before the start menu completes - this
		// is the 1.6 contract and it is LOAD-BEARING: the possessed character is what tells the
		// engine the local player is ready, which is what dismisses the loading screen. (Vanilla
		// game modes get the same signal from their deploy MENU; Overthrow's start menu is a
		// workspace layout, not a menu, so possession is the only signal. Deferring creation until
		// Start Game left the engine's loading spinner up forever - 2026-08-02 play-test.)
		// Pre-menu spawns use the safe-location fallback (home is unset); Start Game assigns the
		// home and teleports the player to it via FinalizePlayerPreparation.
		if (mode.HasGameStarted())
		{
			Print("[Overthrow] Game already started, fully preparing player: " + playerUid);
			mode.FinalizePlayerPreparation(playerId, playerUid);
		}
		else
		{
			Print("[Overthrow] Game not started yet, deferring full player preparation until start menu is complete");
		}

		SpawnDeferredPlayer(playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Creates the player's character once the campaign is actually running - called from
	//! FinalizePlayerPreparation after the home exists, and from DoSpawn_S for players joining an
	//! already-started game. Safe to call more than once per player.
	void SpawnDeferredPlayer(int playerId)
	{
		if (!Replication.IsServer())
			return;

		PlayerController playerController = GetGame().GetPlayerManager().GetPlayerController(playerId);
		if (!playerController)
			return;

		// Already has a body (fresh spawn completes synchronously; the restore path's own pending
		// set guards its async window).
		if (playerController.GetControlledEntity())
			return;

		string playerUid = OVT_Global.GetPlayerUID(playerId);
		if (!playerUid)
		{
			Print("[Overthrow] WARNING: SpawnDeferredPlayer has no persistent UID for playerId: " + playerId, LogLevel.WARNING);
			return;
		}

		// A CONTINUED SESSION MAY NOT HAVE READ ITS PLAYER RECORDS YET, and the stored body id lives in
		// those records. Spawning first would silently hand the player a fresh civilian character and lose
		// exactly the gear this whole path exists to preserve. Only the character creation waits - player
		// setup has already run and is idempotent.
		if (WaitForPersistedPlayerData(playerId, playerUid, 0))
			return;

		CreateCharacter(playerId, playerUid);
	}

	//------------------------------------------------------------------------------------------------
	//! Postpones character creation while a continued session is still loading its saved data.
	//!
	//! ONLY EVER WAITS ON A SESSION THAT WAS LAUNCHED FROM A SAVE POINT. A new campaign, a dedicated
	//! server starting fresh, and the autotest world all fail the IsPlayingLoadedSave() check and spawn
	//! synchronously exactly as before - this cannot change the fresh-spawn path.
	//!
	//! BOUNDED. After PERSISTED_DATA_MAX_WAITS polls the player is spawned anyway: a character late is
	//! recoverable, no character at all is not.
	//! \param[in] playerId The player waiting to spawn.
	//! \param[in] characterPersistenceId Their Overthrow persistent id.
	//! \param[in] attempt How many polls have already happened.
	//! \return True when the caller must return and let the retry finish the job.
	protected bool WaitForPersistedPlayerData(int playerId, string characterPersistenceId, int attempt)
	{
		if (attempt >= PERSISTED_DATA_MAX_WAITS)
		{
			Print("[Overthrow] Saved data still not applied for player " + playerId + " - spawning them anyway", LogLevel.WARNING);
			return false;
		}

		OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if (!mode)
			return false;

		OVT_PersistenceManagerComponent persistenceManager = mode.GetPersistence();
		if (!persistenceManager)
			return false;

		if (!persistenceManager.IsPlayingLoadedSave())
			return false;

		SCR_PersistenceSystem persistence = SCR_PersistenceSystem.GetScriptedInstance();
		if (!persistence)
			return false;

		// EPersistenceSystemState is ordered INIT, SETUP, ACTIVE, SHUTDOWN, FAILURE - ordinal comparison is
		// legal and is how vanilla reads it. Anything at ACTIVE or beyond is as loaded as it will ever get.
		if (persistence.GetState() >= EPersistenceSystemState.ACTIVE)
			return false;

		GetGame().GetCallqueue().CallLater(RetryCreateCharacter, PERSISTED_DATA_WAIT_MS, false, playerId, characterPersistenceId, attempt + 1);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Poll body of WaitForPersistedPlayerData().
	//! \param[in] playerId The player waiting to spawn.
	//! \param[in] characterPersistenceId Their Overthrow persistent id.
	//! \param[in] attempt How many polls have already happened.
	protected void RetryCreateCharacter(int playerId, string characterPersistenceId, int attempt)
	{
		// They may have given up and left while the session was still loading
		PlayerController playerController = GetGame().GetPlayerManager().GetPlayerController(playerId);
		if (!playerController)
			return;

		// Something else may have given them a character meanwhile - a second wait-chain started by
		// another SpawnDeferredPlayer call, or the restore path answering. A corpse does not count:
		// the death path re-creates a character while the controller still references the dead one.
		IEntity controlled = playerController.GetControlledEntity();
		if (controlled && !OVT_PlayerManagerComponent.IsCharacterDead(controlled))
			return;

		if (WaitForPersistedPlayerData(playerId, characterPersistenceId, attempt))
			return;

		CreateCharacter(playerId, characterPersistenceId);
	}

	//------------------------------------------------------------------------------------------------
	//! Gives a player a character, preferring the one they actually had.
	//!
	//! TWO ROUTES, IN ORDER OF FIDELITY:
	//!   1. the player's record names a stored body -> ask the persistence system for THAT character
	//!      (RequestPersistedPlayerBody). It comes back with the gear, wounds, stance and POSITION it was
	//!      stored with, because vanilla serializes all of that for any tracked character. This is what
	//!      makes "quit, continue, carry on where you left off" true;
	//!   2. otherwise, or if the request fails -> spawn the default player prefab at the player's home
	//!      and dress it in the civilian loadout (CreateFreshCharacter). A player is NEVER left without a
	//!      character; the worst outcome of any failure is the pre-existing fresh-spawn behaviour.
	//!
	//! Route 1 is ASYNCHRONOUS: this returns having only started the request, and OnPlayerBodySpawned()
	//! finishes the job - including falling through to route 2.
	//!
	//! A BRAND NEW CAMPAIGN NEVER TAKES ROUTE 1. A player with no stored body id fails the very first
	//! check in RequestPersistedPlayerBody() and this call is synchronous and byte-for-byte the previous
	//! behaviour.
	//! \param[in] playerId The player who will control the character.
	//! \param[in] characterPersistenceId The player's Overthrow persistent id.
	protected void CreateCharacter(int playerId, string characterPersistenceId)
	{
		if (RequestPersistedPlayerBody(playerId, characterPersistenceId))
			return;

		CreateFreshCharacter(playerId, characterPersistenceId);
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns a fresh character for a player and hands control of it over to them.
	//!
	//! Ported from the old persistence-framework base's CreateCharacter with its persistence half
	//! removed: the spawn is a plain global prefab spawn (its own spawn helper was the same three
	//! lines), and there is no id to assign because Overthrow keys player state on its own persistent
	//! id, not on an entity id.
	//!
	//! THE FALLBACK, NOT THE NORMAL PATH once a campaign has been saved once - but the only path on a new
	//! one. This is also where "death is complete loss" is implemented: OnPlayerKilled_S clears the stored
	//! body id, so a killed player always arrives here and always gets the civilian loadout.
	//!
	//! Every caller that is not a respawn choice uses THIS two-argument form, which asks
	//! GetCreationPosition() where to put the character exactly as it always has.
	//! \param[in] playerId The player who will control the character.
	//! \param[in] characterPersistenceId The player's Overthrow persistent id, used to pick the position.
	protected void CreateFreshCharacter(int playerId, string characterPersistenceId)
	{
		CreateFreshCharacterAt(playerId, characterPersistenceId, false, vector.Zero);
	}

	//------------------------------------------------------------------------------------------------
	//! CreateFreshCharacter() with the option of a caller-chosen spawn position.
	//!
	//! THE CHOSEN POSITION IS A PARAMETER, NEVER STORED, AND THAT IS THE POINT. The obvious defect in a
	//! deferred respawn is a stale position override leaking into a spawn that is not a respawn - an
	//! initial spawn, the persisted-data retry, the body-spawn timeout, either body-spawn fallback -
	//! and putting the override in a member or a per-player map would make that a lifetime question to
	//! be reasoned about. As a parameter it is a TYPE question instead: every caller that is not a
	//! respawn choice calls the two-argument wrapper above, which passes false, and there is no state
	//! anywhere for anything to leak from.
	//!
	//! The chosen position is used as given. It is only ever a vector the SERVER produced from its own
	//! eligible-location enumeration, so it is already a place the server was willing to offer;
	//! re-deriving or re-safety-checking it here would move the player away from the location they
	//! deliberately picked.
	//! \param[in] playerId The player who will control the character.
	//! \param[in] characterPersistenceId The player's Overthrow persistent id.
	//! \param[in] useChosenPosition True to spawn at chosenPosition, false to ask GetCreationPosition().
	//! \param[in] chosenPosition Where to spawn. Ignored unless useChosenPosition is true.
	protected void CreateFreshCharacterAt(int playerId, string characterPersistenceId, bool useChosenPosition, vector chosenPosition)
	{
		// Every route here is asynchronous - a deferred call, a failed spawn request, a timeout, and
		// now a player reading a respawn screen - so re-check the player before building anything.
		// Spawning first and asking afterwards would leave a dressed, networked character with no owner
		// when the player has left (HandoverToPlayer logs and returns, deleting nothing). A LIVING
		// controlled entity means somebody else already finished the job; a dead one is the
		// death-respawn path, which must proceed.
		//
		// THE CORPSE TOLERANCE IS LOAD-BEARING AND ITS WINDOW IS NOW LONG. It used to cover the single
		// frame between a death and the automatic rebuild; deferred respawn stretches it to however long
		// the player takes to choose. The test is unchanged because it was never a "how recently did
		// they die" test - it asks whether the player has a character that can still play, and a corpse
		// has never been one.
		PlayerController playerController = GetGame().GetPlayerManager().GetPlayerController(playerId);
		if (!playerController)
		{
			Print("[Overthrow] Player " + playerId + " left before their character could be created - not spawning one", LogLevel.WARNING);
			return;
		}

		IEntity controlled = playerController.GetControlledEntity();
		if (controlled && !OVT_PlayerManagerComponent.IsCharacterDead(controlled))
		{
			Print("[Overthrow] Player " + playerId + " already controls a character - not spawning another", LogLevel.WARNING);
			return;
		}

		ResourceName prefab = GetCreationPrefab(playerId, characterPersistenceId);

		// THE ONLY BRANCH THIS SPLIT ADDS. Without a chosen position this is the untouched call it has
		// always been, which is what keeps "respawn at home" - and every non-respawn spawn - on exactly
		// the behaviour they had before deferred respawn existed. The zero orientation matches what
		// GetCreationPosition() itself uses for every position it picks except a logout transform.
		vector position, yawPitchRoll;
		if (useChosenPosition)
		{
			position = chosenPosition;
			yawPitchRoll = "0 0 0";
		}
		else
		{
			GetCreationPosition(playerId, characterPersistenceId, position, yawPitchRoll);
		}

		EntitySpawnParams spawnParams();
		spawnParams.TransformMode = ETransformMode.WORLD;
		Math3D.AnglesToMatrix(yawPitchRoll, spawnParams.Transform);
		spawnParams.Transform[3] = position + "0 0.1 0"; // Anti lethal terrain clipping

		IEntity character = GetGame().SpawnEntityPrefab(Resource.Load(prefab), GetGame().GetWorld(), spawnParams);
		if (!character)
		{
			Print("[Overthrow] Failed to spawn player character from prefab: " + prefab, LogLevel.ERROR);
			return;
		}

		OnCharacterCreated(playerId, characterPersistenceId, character);

		// LAST RESORT FOR EQUIPMENT, and only on this path. When the stored body comes back there is
		// nothing to do - it arrives wearing what it was wearing. This is the branch where it did not,
		// and without it a player whose character record the engine refuses to return loses everything
		// they were carrying despite having done nothing wrong (measured on a dedicated server
		// 2026-08-05: record written correctly, RequestSpawn answered NOT_FOUND).
		//
		// AFTER OnCharacterCreated, deliberately: that dresses the character in the civilian loadout and
		// hands out first-spawn starting items, so applying the snapshot first would be overwritten.
		// Death never reaches here with a snapshot, because OnPlayerKilled_S drops it.
		ApplyLogoutGearSnapshot(playerId, characterPersistenceId, character);

		HandoverToPlayer(playerId, character);
	}

	//------------------------------------------------------------------------------------------------
	//! Dresses a freshly built character in the gear its player was carrying when they last left.
	//!
	//! Consumed on use: the snapshot describes one specific moment, and leaving it in place would let a
	//! later fresh spawn - after a death, say - hand the same kit out a second time.
	//! \param[in] playerId The player receiving the character.
	//! \param[in] characterPersistenceId Their Overthrow persistent id.
	//! \param[in] character The freshly spawned body.
	protected void ApplyLogoutGearSnapshot(int playerId, string characterPersistenceId, notnull IEntity character)
	{
		OVT_LoadoutManagerComponent loadouts = OVT_Global.GetLoadouts();
		if (!loadouts)
			return;

		OVT_PlayerLoadout snapshot = loadouts.GetLoadout(characterPersistenceId, OVT_LoadoutManagerComponent.LOGOUT_SNAPSHOT_NAME);
		if (!snapshot)
			return;

		if (loadouts.ApplyLoadoutToEntity(snapshot, character))
			Print("[Overthrow] Player " + playerId + " could not be given their stored body - their logout gear was restored onto a fresh character instead", LogLevel.NORMAL);
		else
			Print("[Overthrow] Player " + playerId + " has a logout gear snapshot but it could not be applied", LogLevel.WARNING);

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (players)
			players.ClearPlayerGearSnapshot(characterPersistenceId);
	}

	//------------------------------------------------------------------------------------------------
	//! Asks the persistence system to spawn back the exact character this player was last using.
	//!
	//! Vanilla's own model is SCR_SpawnLogic.c:368-374, which fetches a returning player's character from
	//! the same collection the same way. The request is filtered to a single id, so the callback is
	//! invoked once.
	//! \param[in] playerId The player who will control the character.
	//! \param[in] characterPersistenceId The player's Overthrow persistent id.
	//! \return True when a request was sent (or one is already in flight) and the callback now owns the
	//! outcome; false when there is nothing to ask for, so the caller must spawn a fresh character.
	protected bool RequestPersistedPlayerBody(int playerId, string characterPersistenceId)
	{
		OVT_PlayerData player = OVT_PlayerData.Get(characterPersistenceId);
		if (!player)
			return false;

		if (player.m_sBodyPersistenceId == "")
		{
			// Says so out loud, because a silent return here and a silent return from an OLD build look
			// identical in a server log - and "the id was never stored" and "the id was stored but the
			// record is gone" need completely different fixes.
			PrintFormat("[Overthrow] Player %1 has no stored body id - spawning a fresh character (last known position %2)",
				playerId.ToString(), player.m_vLastKnownPosition.ToString());
			return false;
		}

		// A body is already on its way for this player - the spawn request has not answered yet. Claiming
		// the spawn keeps the caller from building a second character alongside the one being fetched.
		if (m_aPendingBodySpawns.Find(playerId) != -1)
		{
			Print("[Overthrow] Player " + playerId + " already has a body spawn in flight");
			return true;
		}

		SCR_PersistenceSystem persistence = SCR_PersistenceSystem.GetScriptedInstance();
		if (!persistence)
			return false;

		// Asking a system that is still loading would answer UNAVAILABLE; vanilla guards the same way
		// (SCR_SpawnLogic.c:302-307).
		if (persistence.GetState() != EPersistenceSystemState.ACTIVE)
			return false;

		PersistenceCollection collection = GetPlayerBodyCollection(persistence);
		if (!collection)
			return false;

		// Vanilla's own validation of a stored id before using it (SCR_VoiceoverSystemSerializer.c:90).
		// A null UUID still stringifies to a zero-filled value, so never compare, always ask.
		if (!UUID.IsUUID(player.m_sBodyPersistenceId))
		{
			// Whatever is stored is not a UUID - it can never resolve, so stop carrying it around
			player.m_sBodyPersistenceId = "";
			return false;
		}

		UUID bodyId = player.m_sBodyPersistenceId;

		// FAST PATH: THE BODY IS USUALLY ALREADY STANDING IN THE WORLD AFTER A RESTART, and asking for a
		// SPAWN is then the wrong question.
		//
		// The player-character configuration self-spawns ({64ECE6462993EA13}, SelfSpawn 1 in
		// Overthrow.conf), so the engine instantiates the stored body while the world loads - long before
		// its owner reconnects. At that point it is a LIVE TRACKED INSTANCE, not a pending stored record,
		// and RequestSpawn() has nothing left to spawn: it answers NOT_FOUND. Measured on a dedicated
		// server across three restarts (2026-08-04, 2026-08-05) - the record was verified present in the
		// save blob with a valid configuration, and the request still answered NOT_FOUND while the body
		// stood unclaimed in the world and the player got a fresh civilian beside it.
		//
		// FindById() is the matching question - "find a TRACKED instance by id" - and it is exactly how
		// vanilla locates self-spawned player characters after a load
		// (SCR_PlayerReconnectData.RemoveUnusedCharacters, SCR_ReconnectSerializer.c:66).
		//
		// The answer is handed to OnPlayerBodySpawned() rather than adopted here, so the live-instance
		// path and the spawn-request path go through ONE set of guards: owner left, corpse, player
		// already has a character, and the attach itself.
		Managed liveInstance = persistence.FindById(bodyId);
		IEntity liveBody = IEntity.Cast(liveInstance);
		if (liveBody)
		{
			Print("[Overthrow] Player " + playerId + "'s stored body is already in the world - adopting it instead of requesting a spawn");

			// Marked pending first for the same reason as the request below: the callback consumes it.
			m_aPendingBodySpawns.Insert(playerId);

			Tuple2<int, string> liveContext(playerId, characterPersistenceId);
			OnPlayerBodySpawned(EPersistenceStatusCode.OK, liveBody, true, liveContext);
			return true;
		}

		// MUST be marked pending BEFORE the request is sent: an instance the system already has in memory
		// completes the callback IMMEDIATELY, i.e. from inside RequestSpawn(), and the callback's first
		// act is to consume this entry.
		m_aPendingBodySpawns.Insert(playerId);

		Print("[Overthrow] Requesting stored body " + player.m_sBodyPersistenceId + " for player " + playerId);

		PersistenceSpawnRequest request();
		request.Collection = collection;
		request.Include = {bodyId};

		Tuple2<int, string> spawnContext(playerId, characterPersistenceId);
		PersistenceResultCallback callback(OnPlayerBodySpawned, spawnContext);
		persistence.RequestSpawn(request, callback);

		// Insurance only, and a no-op once the callback has answered. The engine documents the callback as
		// always firing, but a player who never heard back would be left with no character at all - the
		// one outcome this feature must never produce.
		GetGame().GetCallqueue().CallLater(OnPlayerBodySpawnTimeout, BODY_SPAWN_TIMEOUT_MS, false, playerId, characterPersistenceId);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! PersistenceResultCallback delegate for RequestPersistedPlayerBody().
	//!
	//! Arity must match PersistenceResultDelegate exactly (the model is SCR_SpawnLogic.c:378).
	//! \param[in] statusCode OK when the stored body was found and instantiated.
	//! \param[in] result The spawned instance on OK; on failure it is the id that could not be fetched.
	//! \param[in] isLast True on the final result of the request (always, for a single-id request).
	//! \param[in] context Tuple2 of runtime player id and Overthrow persistent id.
	protected void OnPlayerBodySpawned(EPersistenceStatusCode statusCode, Managed result, bool isLast, Managed context)
	{
		Tuple2<int, string> spawnContext = Tuple2<int, string>.Cast(context);
		if (!spawnContext)
			return;

		int playerId = spawnContext.param1;
		string characterPersistenceId = spawnContext.param2;

		// Only the first answer for a player is acted on. Anything after it - a duplicate result, or a
		// late answer to a request the timeout already gave up on - must not build a second character.
		int pendingIndex = m_aPendingBodySpawns.Find(playerId);
		if (pendingIndex == -1)
			return;

		m_aPendingBodySpawns.Remove(pendingIndex);

		IEntity bodyEntity;
		if (statusCode == EPersistenceStatusCode.OK)
			bodyEntity = IEntity.Cast(result);

		OVT_PlayerData player = OVT_PlayerData.Get(characterPersistenceId);

		// The player may have left again while the request was in flight. Putting their body in the world
		// for somebody who is not there would leave an unowned character standing around, and the
		// disconnect handler has already run without a character to reserve, so do its job here instead:
		// hide it in place, still tracked, exactly as if they had been holding it when they left.
		PlayerController playerController = GetGame().GetPlayerManager().GetPlayerController(playerId);
		if (!playerController)
		{
			if (bodyEntity)
			{
				Print("[Overthrow] Player " + playerId + " left before their stored body arrived - reserving it for their next visit");
				OVT_PersistenceTracking.Save(bodyEntity);
				OVT_PersistenceReservation.Reserve(bodyEntity);
			}
			return;
		}

		if (!bodyEntity)
		{
			// Stored body could not be brought back: wiped save data, a prefab that no longer exists, an
			// unreadable record. A player is never lost to this - clear the dead id and build a fresh
			// character, which is the pre-existing behaviour (home position, civilian loadout).
			Print(string.Format("[Overthrow] Persistence answered %1 for player %2's stored body - spawning a fresh one",
				typename.EnumToString(EPersistenceStatusCode, statusCode), playerId), LogLevel.WARNING);

			if (player)
				player.m_sBodyPersistenceId = "";

			CreateFreshCharacter(playerId, characterPersistenceId);
			return;
		}

		// A CORPSE MUST NEVER BE HANDED BACK TO A PLAYER. Unreachable by construction - the id is cleared
		// on death and SyncPlayerBodyIds() refuses to capture a dead character - but vanilla guards the
		// same case on the same call (SCR_SpawnLogic.c:422-424) because possessing a dead character raises
		// no further death events, which would strand the player permanently. The body is left standing in
		// the world rather than deleted: if it really is a corpse, it is lootable remains and belongs
		// there, and it is not this method's business to decide whose record it is.
		if (OVT_PlayerManagerComponent.IsCharacterDead(bodyEntity))
		{
			Print("[Overthrow] Stored body for player " + playerId + " came back dead - spawning a fresh one", LogLevel.WARNING);

			if (player)
				player.m_sBodyPersistenceId = "";

			CreateFreshCharacter(playerId, characterPersistenceId);
			return;
		}

		// Something gave this player a character while the request was in flight. Handing over a second
		// one would leave the first orphaned, so the arrival is discarded WITH its stored data - nothing
		// points at it any more, and leaving the record behind would make it spawnable forever.
		if (playerController.GetControlledEntity())
		{
			Print("[Overthrow] Player " + playerId + " already has a character - discarding their stored body", LogLevel.WARNING);
			OVT_PersistenceTracking.Untrack(bodyEntity, false);
			SCR_EntityHelper.DeleteEntityAndChildren(bodyEntity);
			return;
		}

		AttachRestoredPlayerBody(playerId, characterPersistenceId, bodyEntity);
	}

	//------------------------------------------------------------------------------------------------
	//! Links a restored body to its player and hands control of it over.
	//!
	//! DELIBERATELY DOES NOT CALL OnCharacterCreated(). That method exists to DRESS a new character - it
	//! spawns the civilian loadout into empty slots and, on a player's very first spawn, adds the
	//! difficulty's starting items. Running it on a body that already carries the player's own equipment
	//! would fight for the same loadout slots and hand out starting items a second time. The only thing it
	//! does that still applies is clearing firstSpawn, which is done here directly.
	//!
	//! No position is chosen either: a body spawned back from storage arrives at the transform it was
	//! stored with, which is why continuing a campaign puts you where you saved.
	//! \param[in] playerId The player taking control.
	//! \param[in] characterPersistenceId The player's Overthrow persistent id.
	//! \param[in] character The restored body.
	protected void AttachRestoredPlayerBody(int playerId, string characterPersistenceId, notnull IEntity character)
	{
		// A body handed back by the persistence system arrives already tracked, so ASK before registering
		// rather than re-registering blind.
		if (!OVT_PersistenceTracking.IsTracked(character))
			OVT_PersistenceTracking.Track(character);

		// THE BODY IS NORMALLY HIDDEN WHEN IT GETS HERE and must be visible and simulating again before
		// its player is put inside it. Two routes arrive reserved - the one this whole design is built
		// on (the body was reserved at disconnect and found again by FindById) and the post-restart one
		// (self-spawned, then re-reserved by ReserveOfflinePlayerBodies) - and a third does not (a
		// genuine RequestSpawn of a stored record, which arrives with the prefab's flags). Release() is
		// idempotent, so this single call covers all three.
		OVT_PersistenceReservation.Release(character);

		OVT_PlayerData player = OVT_PlayerData.Get(characterPersistenceId);
		if (player)
		{
			// The record must point at THIS character from now on. Normally the same id it asked for, but
			// re-read rather than assumed.
			string bodyId = OVT_PersistenceTracking.GetPersistentId(character);
			if (bodyId != "")
				player.m_sBodyPersistenceId = bodyId;

			// A restored body is by definition not a first spawn - whatever starting items this player was
			// owed were handed out in the session that created it.
			player.firstSpawn = false;
		}

		HandoverToPlayer(playerId, character);

		Print("[Overthrow] Player " + playerId + " restored from their stored body, gear intact");
	}

	//------------------------------------------------------------------------------------------------
	//! Gives up on a spawn request that never answered and falls back to a fresh character.
	//!
	//! No-op in the normal case: the callback has already cleared the pending entry by the time this runs.
	//! \param[in] playerId The player whose request was sent.
	//! \param[in] characterPersistenceId The player's Overthrow persistent id.
	protected void OnPlayerBodySpawnTimeout(int playerId, string characterPersistenceId)
	{
		int pendingIndex = m_aPendingBodySpawns.Find(playerId);
		if (pendingIndex == -1)
			return;

		m_aPendingBodySpawns.Remove(pendingIndex);

		PlayerController playerController = GetGame().GetPlayerManager().GetPlayerController(playerId);
		if (!playerController)
			return;

		// Something may have given the player a character by other means in the meantime
		if (playerController.GetControlledEntity())
			return;

		Print("[Overthrow] No answer to player " + playerId + "'s body spawn request - spawning a fresh one", LogLevel.WARNING);

		OVT_PlayerData player = OVT_PlayerData.Get(characterPersistenceId);
		if (player)
			player.m_sBodyPersistenceId = "";

		CreateFreshCharacter(playerId, characterPersistenceId);
	}

	//------------------------------------------------------------------------------------------------
	//! The persistence collection player bodies are stored in, resolved once and kept.
	//!
	//! Cached the way vanilla caches it (SCR_SpawnLogic.SetupPersistenceCollections, :51-55) - the lookup
	//! is by name against the loaded configuration and does not change during a session.
	//! \param[in] persistence The live persistence system.
	//! \return The collection, or null when the loaded configuration does not contain it.
	protected PersistenceCollection GetPlayerBodyCollection(notnull SCR_PersistenceSystem persistence)
	{
		if (!m_PlayerBodyCollection)
		{
			m_PlayerBodyCollection = persistence.FindCollection(PLAYER_BODY_COLLECTION);

			if (!m_PlayerBodyCollection)
				Print("[Overthrow] No '" + PLAYER_BODY_COLLECTION + "' persistence collection - player bodies cannot be spawned back with their gear", LogLevel.WARNING);
		}

		return m_PlayerBodyCollection;
	}

	//------------------------------------------------------------------------------------------------
	//! Vanilla's possession-change hook, MINUS the re-match of the body a player just stopped
	//! controlling.
	//!
	//! WHAT VANILLA DOES AND WHY IT BREAKS OVERTHROW. SCR_SpawnLogic.OnPlayerEntityChanged_S calls
	//! m_Persistence.ReloadConfig(previousEntity) under the comment "Check that the old entity does not
	//! count as player anymore". That is correct for vanilla, which brings a returning player's body
	//! back by SelfSpawn and does not care what an abandoned one is matched to. In Overthrow it is
	//! destructive: the player-character config is the ONLY character config with SelfSpawn 1
	//! ({64ECE6462993EA13}, overridden in Overthrow.conf), and a body that stops being player-controlled
	//! re-matches onto the AI character config, which is deliberately SelfSpawn 0 so managers do not
	//! double their garrisons. A record under a no-self-spawn config is DROPPED at load - so the body
	//! OVT_PlayerData.m_sBodyPersistenceId points at stops existing, and the player comes back as a
	//! fresh civilian with their gear gone. Measured on a dedicated server 2026-08-04 and 2026-08-05:
	//! the id was stored and asked for, and persistence answered NOT_FOUND.
	//!
	//! KEEPING THE PLAYER CONFIG IS THE POINT. The body must stay matched to the configuration that
	//! self-spawns, because that is the only mechanism that survives a restart (SetConfig cannot be
	//! used - see OVT_PersistenceTracking.MarkForSelfSpawn). A body a player has left therefore comes
	//! back into the world at load and is handed straight back to them by id.
	//!
	//! Everything else vanilla does here is preserved: the entity-lost notification and the pending
	//! possession hand-off, plus the re-match of the NEW entity, which is the half that is actually
	//! wanted (it is what recognises a freshly possessed body as a player's).
	//! \param[in] playerId The player whose controlled entity changed.
	//! \param[in] previousEntity The body they stopped controlling, may be null.
	//! \param[in] newEntity The body they now control, may be null.
	override void OnPlayerEntityChanged_S(int playerId, IEntity previousEntity, IEntity newEntity)
	{
		if (!newEntity)
			OnPlayerEntityLost_S(playerId);

		SCR_PersistenceSystem persistence = SCR_PersistenceSystem.GetScriptedInstance();
		if (persistence)
		{
			// Deliberately NOT ReloadConfig(previousEntity) - see above.
			if (previousEntity)
			{
				EntityPersistenceConfig previousConfig = EntityPersistenceConfig.Cast(persistence.GetConfig(previousEntity));
				if (previousConfig)
					PrintFormat("[Overthrow] Player %1 released a body; its config is left alone (selfSpawn %2)",
						playerId.ToString(), previousConfig.m_bSelfSpawn.ToString());
			}

			if (newEntity)
				persistence.ReloadConfig(newEntity);
		}

		ApplyPendingPosession(playerId);

		// EVERY route that gives a player a body arrives here - fresh creation (HandoverToPlayer calls
		// this explicitly), the restored-body route, the death respawn, and vanilla's reconnect
		// component (SCR_ReconnectComponent.ApplyData -> EmitPlayerEntityChange_S). Scheduling the
		// faction/group setup ONLY from HandoverToPlayer meant any player who arrived by one of the
		// other routes silently ended up with no faction and no group, which is the whole BUG-088
		// symptom set: commanding denied, no group indicator, recruits stuck in their own group.
		// CreateAndJoinGroup is idempotent (it no-ops when the player already has a faction and a
		// group), so the duplicate call from HandoverToPlayer is harmless.
		if (newEntity)
			GetGame().GetCallqueue().CallLater(CreateAndJoinGroup, 3000, false, playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Handles failed player registration attempts by scheduling a retry.
	void OnPlayerRegisterFailed(int playerId)
	{
		int delay = Math.RandomFloat(900, 1100);
		GetGame().GetCallqueue().CallLater(OnPlayerAuditSuccess_S, delay, false, playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Determines the spawn position and orientation for a player character based on their home location.
	protected void GetCreationPosition(int playerId, string characterPersistenceId, out vector position, out vector yawPitchRoll)
	{
		OVT_PlayerData player = OVT_PlayerData.Get(characterPersistenceId);
		if(player)
		{
			// WHERE THEY LOGGED OUT BEATS HOME. A player who quit (or whose server restarted) comes back
			// standing where they left, which is what the stored body would have given them had its
			// record survived - so a lost character record costs the gear it was carrying and nothing
			// else. Cleared on death, so this branch never resurrects anyone at their own corpse.
			//
			// Not safety-checked against IsSpawnLocationSafe: the player chose to log out there, and
			// second-guessing it would teleport them across the map for standing near a patrol.
			if(player.m_vLastKnownPosition != vector.Zero)
			{
				position = player.m_vLastKnownPosition;
				yawPitchRoll = player.m_vLastKnownAngles;
				return;
			}

			vector homePos = player.home;

			// An unassigned home is the zero vector - never treat it as a real position (a body at
			// the world origin ends up underground). Pre-start-menu spawns land here and take the
			// safe-location fallback WITHOUT writing it as their home: FinalizePlayerPreparation
			// only assigns a starting house (and starting car) to a player whose home is still
			// unset, so persisting the placeholder position would cost the player both.
			if(homePos == vector.Zero)
			{
				position = FindSafeSpawnLocation(playerId, characterPersistenceId, false);
				yawPitchRoll = "0 0 0";
			}
			// Check if the home location is safe to spawn at
			else if(IsSpawnLocationSafe(homePos))
			{
				position = homePos;
				yawPitchRoll = "0 0 0";
			}
			else
			{
				// Home is not safe, find alternative spawn location
				vector safePos = FindSafeSpawnLocation(playerId, characterPersistenceId);
				position = safePos;
				yawPitchRoll = "0 0 0";
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	void CreateAndJoinGroup(int playerId)
	{
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (!playerController)
		{
			// Player controller not ready yet, retry later
			GetGame().GetCallqueue().CallLater(CreateAndJoinGroupDelayed, 500, false, playerId, 0);
			return;
		}

		// Check if the player has a controlled entity
		IEntity controlledEntity = playerController.GetControlledEntity();
		if (!controlledEntity)
		{
			// Player doesn't have a controlled entity yet, retry later
			GetGame().GetCallqueue().CallLater(CreateAndJoinGroupDelayed, 500, false, playerId, 0);
			return;
		}

		SCR_PlayerControllerGroupComponent group = SCR_PlayerControllerGroupComponent.Cast(playerController.FindComponent(SCR_PlayerControllerGroupComponent));
		if(!group)
		{
			Print("[Overthrow] ERROR: Player controller has no group component! Player ID: " + playerId, LogLevel.ERROR);
			return;
		}

		// STAYS HERE, NOT IN THE MANAGER. Assigning the faction fires vanilla's
		// SCR_GroupsManagerComponent.OnPlayerFactionChanged (Groups/SCR_GroupsManagerComponent.c:1101),
		// which REMOVES the player from any group they are in (:1130) and resets their group ids (:1139).
		// That is correct exactly once, at spawn, and would be destructive from the reactor's restore
		// path - which is why EnsureOwnGroup() does not do it. It is a no-op after the first call
		// (SetCivilianFaction returns early when the player already has the right faction).
		SetCivilianFaction(playerId);

		int groupId = group.GetGroupID();
		//Is the player already in a group?
		if(groupId != -1)
		{
			Print("[Overthrow] Player " + playerId + " already in group " + groupId, LogLevel.NORMAL);
			return;
		}

		// Readiness probe only - the manager is what actually uses it. Kept here because "the groups
		// manager is not up yet" is the one failure this path has always RETRIED rather than given up on.
		if (!SCR_GroupsManagerComponent.GetInstance())
		{
			// Groups manager not ready, retry later
			GetGame().GetCallqueue().CallLater(CreateAndJoinGroupDelayed, 500, false, playerId, 0);
			return;
		}

		OVT_PlayerGroupManagerComponent playerGroups = OVT_PlayerGroupManagerComponent.GetInstance();
		if (!playerGroups)
		{
			// Manager not initialised yet, retry later
			GetGame().GetCallqueue().CallLater(CreateAndJoinGroupDelayed, 500, false, playerId, 0);
			return;
		}

		// The whole "create this player's own private group and put them in it" body now lives on
		// OVT_PlayerGroupManagerComponent, because the reactor's return-to-own-group path needs exactly
		// the same thing. EnsureOwnGroup is idempotent and logs every failure with its reason, so a -1
		// here has already been explained in the log.
		int groupID = playerGroups.EnsureOwnGroup(playerId);
		if (groupID == -1)
			return;

		string playerName = GetGame().GetPlayerManager().GetPlayerName(playerId);

		// Fire the group created event.
		//
		// THIS IS THE ONLY PLACE IT FIRES (decision D6). OVT_RecruitManagerComponent subscribes to it via
		// OVT_RespawnSystemComponent.GetOnPlayerGroupCreated() and answers by respawning the player's
		// whole recruit roster - which is right for a player arriving in the world and wrong for a player
		// merely moving between groups, so the reactor never fires it.
		m_OnPlayerGroupCreated.Invoke(playerId, groupID, playerName);
	}

	//------------------------------------------------------------------------------------------------
	//! Delayed group creation with retry mechanism
	void CreateAndJoinGroupDelayed(int playerId, int retryCount)
	{
		if (retryCount > 10)
		{
			Print("[Overthrow] Failed to create group for player " + playerId + " after 10 retries", LogLevel.ERROR);
			return;
		}

		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (!playerController)
		{
			// Still not ready, retry again
			GetGame().GetCallqueue().CallLater(CreateAndJoinGroupDelayed, 500, false, playerId, retryCount + 1);
			return;
		}

		// Player controller is ready, proceed with group creation
		CreateAndJoinGroup(playerId);
	}

	//------------------------------------------------------------------------------------------------
	void SetCivilianFaction(int playerId)
	{
		string wantedKey = OVT_Global.GetConfig().GetPlayerFaction().GetFactionKey();

		PlayerController playerController = GetGame().GetPlayerManager().GetPlayerController(playerId);
		if (!playerController)
		{
			Print("[Overthrow] SetCivilianFaction: no player controller for player " + playerId, LogLevel.WARNING);
			return;
		}

		SCR_PlayerFactionAffiliationComponent factionComponent = SCR_PlayerFactionAffiliationComponent.Cast(playerController.FindComponent(SCR_PlayerFactionAffiliationComponent));
		if (!factionComponent)
		{
			Print("[Overthrow] SetCivilianFaction: player " + playerId + " has no SCR_PlayerFactionAffiliationComponent", LogLevel.ERROR);
			return;
		}

		FactionManager mgr = GetGame().GetFactionManager();
		if (!mgr)
		{
			Print("[Overthrow] SetCivilianFaction: no FactionManager", LogLevel.ERROR);
			return;
		}

		Faction playerFaction = mgr.GetFactionByKey(wantedKey);
		if (!playerFaction)
		{
			Print("[Overthrow] SetCivilianFaction: faction key '" + wantedKey + "' does not exist in this world", LogLevel.ERROR);
			return;
		}

		// Already ours - don't re-fire OnPlayerFactionSet_S on every respawn
		if (factionComponent.GetAffiliatedFaction() == playerFaction)
		{
			Print("[Overthrow] SetCivilianFaction: player " + playerId + " is already " + wantedKey);
			return;
		}

		// RequestFaction() is the CLIENT's request API: it marshals Rpc_RequestFaction_S, an
		// RplRcver.Server RPC that only travels when the caller is a proxy. This runs on the
		// server, so in a replicated session the request was silently dropped and the player was
		// left on whatever faction (if any) vanilla had given them - which is why the group menu
		// listed a stranger's faction and the player's own group was nowhere to be found
		// (SCR_GroupsManagerComponent.GetPlayerGroup resolves the group through the faction).
		// SetFaction_S is vanilla's server-side assignment - SCR_ReconnectComponent and the
		// faction-affiliation persistence serializer both use it the same way.
		bool assigned;
		if (Replication.IsServer())
			assigned = factionComponent.SetFaction_S(playerFaction);
		else
			assigned = factionComponent.RequestFaction(playerFaction);

		// One line that answers "did the player get a faction, and if not, how far did we get?" -
		// the whole BUG-088 chain hangs off this call, and every earlier failure of it was silent.
		string resultingKey = "<none>";
		Faction resulting = factionComponent.GetAffiliatedFaction();
		if (resulting)
			resultingKey = resulting.GetFactionKey();

		Print("[Overthrow] SetCivilianFaction: player " + playerId + " -> " + wantedKey
			+ " (isServer=" + Replication.IsServer().ToString() + ", accepted=" + assigned.ToString()
			+ ", now=" + resultingKey + ")", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Check if a spawn location is safe (not controlled by occupying faction)
	protected bool IsSpawnLocationSafe(vector location)
	{
		// Check if there's a base near this location
		OVT_BaseControllerComponent nearbyBase = GetNearbyBase(location, 100); // 100m radius

		if(nearbyBase)
		{
			// Check if base is controlled by occupying faction
			int baseFaction = nearbyBase.GetControllingFaction();
			int occupyingFactionIndex = OVT_Global.GetConfig().GetOccupyingFactionIndex();

			if(baseFaction == occupyingFactionIndex)
			{
				return false;
			}
		}

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Find a nearby base within the specified radius
	protected OVT_BaseControllerComponent GetNearbyBase(vector location, float radius)
	{
		// Clear previous results
		m_FoundBases.Clear();

		// Query for base controllers near the location
		GetGame().GetWorld().QueryEntitiesBySphere(location, radius, CheckForBaseController, FilterBaseEntities, EQueryEntitiesFlags.ALL);

		// Return the first base found (if any)
		foreach(IEntity entity : m_FoundBases)
		{
			OVT_BaseControllerComponent baseController = OVT_BaseControllerComponent.Cast(entity.FindComponent(OVT_BaseControllerComponent));
			if(baseController)
				return baseController;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Filter for entities that might have base controllers
	protected bool FilterBaseEntities(IEntity entity)
	{
		return entity.FindComponent(OVT_BaseControllerComponent) != null;
	}

	//------------------------------------------------------------------------------------------------
	//! Check if entity has base controller and add to found bases
	protected bool CheckForBaseController(IEntity entity)
	{
		if(entity.FindComponent(OVT_BaseControllerComponent))
		{
			m_FoundBases.Insert(entity);
		}
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Find a safe spawn location when home is compromised or not yet assigned.
	//! \param[in] updateHome When true the chosen position becomes the player's new home (the
	//! compromised-home case). Pre-start placeholder spawns pass false: their home must stay unset
	//! so FinalizePlayerPreparation still assigns a starting house.
	protected vector FindSafeSpawnLocation(int playerId, string characterPersistenceId, bool updateHome = true)
	{
		OVT_PlayerData player = OVT_PlayerData.Get(characterPersistenceId);

		// First try: Check if player has an owned house that's safe
		OVT_RealEstateManagerComponent realEstate = OVT_Global.GetRealEstate();
		if(realEstate)
		{
			vector safeHousePos = FindSafeOwnedHouse(playerId, realEstate);
			if(safeHousePos != vector.Zero)
			{
				// Update their home to this safe house
				if(updateHome)
					realEstate.SetHomePos(playerId, safeHousePos);
				return safeHousePos;
			}
		}

		// Second try: Find a random safe town location
		OVT_TownManagerComponent townManager = OVT_Global.GetTowns();
		if(townManager && townManager.m_Towns)
		{
			foreach(OVT_TownData town : townManager.m_Towns)
			{
				if(town && IsSpawnLocationSafe(town.location))
				{
					vector safeSpawn = OVT_Global.FindSafeSpawnPosition(town.location);
					// Update their home to this safe town
					if(updateHome && realEstate)
						realEstate.SetHomePos(playerId, safeSpawn);
					return safeSpawn;
				}
			}
		}

		// Last resort: Use default spawn location
		vector fallbackPos = "5000 0 5000"; // Default fallback position
		if(updateHome && realEstate)
			realEstate.SetHomePos(playerId, fallbackPos);
		return fallbackPos;
	}

	//------------------------------------------------------------------------------------------------
	//! Find a safe house that the player owns
	protected vector FindSafeOwnedHouse(int playerId, OVT_RealEstateManagerComponent realEstate)
	{
		// Get player's persistent ID
		string persId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);

		// Get all owned buildings using the existing method
		set<EntityID> ownedBuildings = realEstate.GetOwned(persId);

		if(!ownedBuildings || ownedBuildings.IsEmpty())
		{
			return vector.Zero;
		}

		// Check each owned building for safety
		foreach(EntityID buildingId : ownedBuildings)
		{
			IEntity building = GetGame().GetWorld().FindEntityByID(buildingId);
			if(!building) continue;

			vector housePos = building.GetOrigin();
			if(IsSpawnLocationSafe(housePos))
			{
				return housePos;
			}
		}
		return vector.Zero;
	}

	//------------------------------------------------------------------------------------------------
	//! Hands network ownership and control of a spawned character to a player, then schedules the group.
	//!
	//! The first three steps are the old persistence-framework base's HandoverToPlayer verbatim, minus
	//! its database callback - that only existed to clear an entry from its in-flight-character map,
	//! which had no other reader in Overthrow. All three are plain vanilla calls.
	//! \param[in] playerId The player taking control.
	//! \param[in] character The character to hand over.
	protected void HandoverToPlayer(int playerId, IEntity character)
	{
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (!playerController)
		{
			Print("[Overthrow] Cannot hand over character - no player controller for playerId: " + playerId, LogLevel.ERROR);
			return;
		}

		playerController.SetInitialMainEntity(character);

		SCR_BaseGameMode gamemode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (gamemode)
			gamemode.OnPlayerEntityChanged_S(playerId, null, character);

		SCR_RespawnComponent respawn = GetPlayerRespawnComponent_S(playerId);
		if (respawn)
			respawn.NotifySpawn(character);

		// Schedule group creation after handover completes
		GetGame().GetCallqueue().CallLater(CreateAndJoinGroup, 3000, false, playerId);
	}

	//------------------------------------------------------------------------------------------------
	//! Called after a player character has been created and spawned into the world.
	//! Initializes the character's inventory with the default civilian loadout and difficulty-specific starting items.
	protected void OnCharacterCreated(int playerId, string characterPersistenceId, IEntity character)
	{
		InventoryStorageManagerComponent storageManager = OVT_ComponentFinder<InventoryStorageManagerComponent>.Find(character);

		array<ResourceName> doneStartingItems = {};
		OVT_PlayerData player = OVT_PlayerData.Get(characterPersistenceId);
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();

		if(storageManager)
		{
			foreach (OVT_LoadoutSlot loadoutItem : OVT_Global.GetConfig().m_CivilianLoadout.m_aSlots)
			{
				if(loadoutItem.m_fSkipChance > 0)
				{
					float rnd = s_AIRandomGenerator.RandFloat01();
					if(rnd <= loadoutItem.m_fSkipChance) continue;
				}

				IEntity slotEntity = SpawnDefaultCharacterItem(storageManager, loadoutItem);
				if (!slotEntity) continue;

				if(config && config.m_Difficulty && player && player.firstSpawn)
				{
					foreach(ResourceName res : config.m_Difficulty.startingItems)
					{
						if(doneStartingItems.Contains(res)) continue;

						EntitySpawnParams spawnParams();
						spawnParams.Transform[3] = storageManager.GetOwner().GetOrigin();

						IEntity spawnedItem = GetGame().SpawnEntityPrefab(Resource.Load(res), GetGame().GetWorld(), spawnParams);
						if(!spawnedItem)
						{
							doneStartingItems.Insert(res);
							continue;
						}
						bool foundStorage = false;
						array<Managed> outComponents();
						slotEntity.FindComponents(BaseInventoryStorageComponent, outComponents);
						foreach (Managed componentRef : outComponents)
						{
							BaseInventoryStorageComponent storageComponent = BaseInventoryStorageComponent.Cast(componentRef);

							if(storageComponent.GetPurpose() & EStoragePurpose.PURPOSE_DEPOSIT)
							{
								if (!storageManager.TryInsertItemInStorage(spawnedItem, storageComponent)) continue;

								InventoryItemComponent inventoryItemComponent = InventoryItemComponent.Cast(spawnedItem.FindComponent(InventoryItemComponent));
								if (inventoryItemComponent && !inventoryItemComponent.GetParentSlot()) continue;

								foundStorage = true;
								doneStartingItems.Insert(res);
								break;
							}
						}
						if(!foundStorage)
						{
							SCR_EntityHelper.DeleteEntityAndChildren(spawnedItem);
						}
					}
				}

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
					Print("Failed to insert item " + slotEntity + " " + loadoutStorage + " " + suitableSlotId, LogLevel.WARNING);
					SCR_EntityHelper.DeleteEntityAndChildren(slotEntity);
				}
			}

			// Defensive: this line is now reachable (character creation runs again), and a null record
			// here would abort the whole spawn before HandoverToPlayer.
			if (player)
				player.firstSpawn = false;
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns a default item based on a loadout slot definition
	protected IEntity SpawnDefaultCharacterItem(InventoryStorageManagerComponent storageManager, OVT_LoadoutSlot loadoutItem)
	{
		if(!storageManager) return null;

		int selection = s_AIRandomGenerator.RandInt(0, loadoutItem.m_aChoices.Count());
		ResourceName prefab = loadoutItem.m_aChoices[selection];

		EntitySpawnParams spawnParams();
		spawnParams.Transform[3] = storageManager.GetOwner().GetOrigin();

		IEntity slotEntity = GetGame().SpawnEntityPrefab(Resource.Load(prefab), GetGame().GetWorld(), spawnParams);
		if (!slotEntity) return null;

		if (loadoutItem.m_aStoredItems)
		{
			array<Managed> outComponents();
			slotEntity.FindComponents(BaseInventoryStorageComponent, outComponents);

			foreach (ResourceName storedItem : loadoutItem.m_aStoredItems)
			{
				IEntity spawnedItem = GetGame().SpawnEntityPrefab(Resource.Load(storedItem), GetGame().GetWorld(), spawnParams);
				if(!spawnedItem) continue;

				foreach (Managed componentRef : outComponents)
				{
					BaseInventoryStorageComponent storageComponent = BaseInventoryStorageComponent.Cast(componentRef);

					if(storageComponent.GetPurpose() & EStoragePurpose.PURPOSE_DEPOSIT)
					{
						if (!storageManager.TryInsertItemInStorage(spawnedItem, storageComponent)) continue;

						InventoryItemComponent inventoryItemComponent = InventoryItemComponent.Cast(spawnedItem.FindComponent(InventoryItemComponent));
						if (inventoryItemComponent && !inventoryItemComponent.GetParentSlot()) continue;
						break;
					}
				}
			}
		}

		return slotEntity;
	}

	//------------------------------------------------------------------------------------------------
	//! Called on the server when a player is killed. Charges the respawn cost and hands the player to
	//! the respawn screen.
	//!
	//! NO CHARACTER IS CREATED HERE ANY MORE. Until deferred respawn this method ended by scheduling
	//! CreateCharacter() one frame later, which put the player back at home whether they liked it or
	//! not. It now ends by asking them where they want to go, and BeginAwaitingRespawn() owns
	//! everything after that. Everything before that last line is unchanged, and the ORDER is what
	//! makes it safe to defer: the cost is charged, the stored body id is cleared, the last known
	//! position is cleared and the gear snapshot is dropped BEFORE the wait begins, so none of them can
	//! be affected by how long the player takes to choose or by which location they pick.
	//!
	//! THE COST IS CHARGED AT DEATH, NOT AT RESPAWN, and that is a decision rather than an accident.
	//! Charging here means the fee cannot vary by destination and there is nothing for a player to game
	//! by picking one location over another - the location choice is free, respawning is not.
	//!
	//! DEATH IS COMPLETE LOSS, AND THIS IS WHERE THAT IS ENFORCED. The player's record carries the
	//! persistence id of the body they were using, and CreateCharacter() would otherwise spawn that exact
	//! body - gear included - straight back. Clearing the id before the rebuild is scheduled is what
	//! guarantees a killed player always takes the fresh-civilian route, in this session and in every
	//! session after it.
	//!
	//! The corpse is left in the world deliberately: it is lootable remains, and it now persists in its
	//! own right. The old persistence framework forced it to take a fresh id and to self-spawn so it could
	//! be looted after a restart; the same end is now reached by OVT_OverthrowGameMode's kill hook, which
	//! flips the corpse's matched config to self-spawn (OVT_PersistenceTracking.MarkForSelfSpawn).
	//! (Vanilla reaches it through SCR_SpawnLogic.OnPlayerEntityChanged_S, which Overthrow never travels:
	//! that is raised by SCR_RespawnSystemComponent.EmitPlayerEntityChange_S, and Overthrow hands
	//! characters over itself.)
	override void OnPlayerKilled_S(int playerId, IEntity playerEntity, IEntity killerEntity, notnull Instigator killer)
	{
		super.OnPlayerKilled_S(playerId, playerEntity, killerEntity, killer);

		if (!Replication.IsServer()) return;

		OVT_Global.GetEconomy().ChargeRespawn(playerId);

		string playerUid = OVT_Global.GetPlayerUID(playerId);
		if (!playerUid)
		{
			Print("[Overthrow] Cannot respawn playerId " + playerId + " - no persistent UID", LogLevel.ERROR);
			return;
		}

		OVT_PlayerData player = OVT_PlayerData.Get(playerUid);
		if (player)
		{
			player.m_sBodyPersistenceId = "";

			// Death sends the player home, so the place they died must stop being a spawn candidate -
			// otherwise the fresh civilian would appear standing on their own corpse.
			player.m_vLastKnownPosition = vector.Zero;
			player.m_vLastKnownAngles = vector.Zero;
		}

		// Death is complete loss, and that has to hold for the gear snapshot too - it is applied on the
		// same fresh-spawn path a death respawn takes, so leaving it would hand a killed player their
		// kit straight back and quietly undo the whole ethos.
		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (players)
			players.ClearPlayerGearSnapshot(playerUid);

		BeginAwaitingRespawn(playerId, playerUid);
	}

	//------------------------------------------------------------------------------------------------
	//! Puts a killed player in front of the respawn screen instead of giving them a character.
	//!
	//! REPLACES A ONE-FRAME AUTOMATIC REBUILD WITH AN UNBOUNDED WAIT, and everything else in this
	//! region exists to make that safe. The player is not spawned again until they pick somewhere, and
	//! the only ways out of the wait are: they pick a location, they pick home, their pick has become
	//! ineligible and falls back to home, they disconnect, something else hands them a living
	//! character, or the world ends. Nothing counts down and nothing spawns them on their behalf.
	//!
	//! A CAPABILITY CHECK AT t=0, NOT A TIMEOUT. If this player's controller cannot show a respawn
	//! screen then no amount of waiting will produce one, so that is decided here, once, at the moment
	//! of death - and the answer is to spawn them immediately, which is exactly the behaviour this
	//! feature replaced. A misconfigured controller prefab degrades to the old game; it does not leave
	//! a player dead forever. Resolved through the same lookup the re-ask tick uses, so a check that
	//! passes here is a promise the tick can keep.
	//! \param[in] playerId The player who has just died.
	//! \param[in] characterPersistenceId Their Overthrow persistent id.
	protected void BeginAwaitingRespawn(int playerId, string characterPersistenceId)
	{
		// Idempotent. Two death events for one player would otherwise start two re-ask chains, and the
		// player would be asked twice as often forever.
		if (m_aAwaitingRespawn.Find(playerId) != -1)
			return;

		OVT_RespawnRequestComponent requests = FindRespawnRequests(playerId);
		if (!requests)
		{
			Print("[Overthrow] Player " + playerId + " has no respawn request component - either no Overthrow controller entity exists for them, or OVT_RespawnRequestComponent is missing from Prefabs/GameMode/OVT_OverthrowController.et. Spawning them immediately instead of asking where they want to go", LogLevel.ERROR);

			// THE LINE DEFERRED RESPAWN REPLACED, REPRODUCED EXACTLY, one-frame delay included. That
			// delay is not incidental: this is reached from inside the kill handler, and a new character
			// cannot be handed over on the same frame the old one died. Spawning synchronously here
			// would make the degrade path behave WORSE than the behaviour it exists to fall back to.
			GetGame().GetCallqueue().Call(CreateCharacter, playerId, characterPersistenceId);
			return;
		}

		m_aAwaitingRespawn.Insert(playerId);

		Print("[Overthrow] Player " + playerId + " is awaiting a respawn choice - no character will be created until they pick");

		requests.AskShowRespawnScreen(playerId);

		GetGame().GetCallqueue().CallLater(ReAskRespawnScreen, RESPAWN_REASK_MS, false, playerId, characterPersistenceId);
	}

	//------------------------------------------------------------------------------------------------
	//! Asks again, every RESPAWN_REASK_MS, for as long as the player is still choosing.
	//!
	//! HOW "NO TIMEOUT" AND "NEVER STRANDED" ARE RECONCILED. The show request is not assumed to have
	//! arrived: it is re-sent forever, so a dropped RPC or a client whose respawn screen was not ready
	//! when it died still ends up looking at one. What is never done is spawning the player because
	//! time passed - the wait has no deadline, only exits.
	//!
	//! The chain cancels itself by not rescheduling. Nothing removes it from the call queue, and
	//! nothing should: GetCallqueue().Remove() takes a METHOD, not an argument list, so cancelling one
	//! player's tick that way would cancel every other player's too. The awaiting entry IS the
	//! subscription, and one already-queued tick firing after the entry is gone is harmless by design.
	//! \param[in] playerId The player who is still choosing.
	//! \param[in] characterPersistenceId Their Overthrow persistent id.
	protected void ReAskRespawnScreen(int playerId, string characterPersistenceId)
	{
		// 1. Not awaiting any more - they picked, they left, or an exit below already fired. This is
		//    the normal end of the chain and is deliberately silent.
		if (m_aAwaitingRespawn.Find(playerId) == -1)
			return;

		// 2. Gone without a disconnect event this path can see. Dropping the entry rather than leaving
		//    it matters because runtime player ids are reused: a later joiner inheriting this number
		//    must not inherit a claim to be spawned.
		PlayerController playerController = GetGame().GetPlayerManager().GetPlayerController(playerId);
		if (!playerController)
		{
			DropAwaitingRespawn(playerId);
			Print("[Overthrow] Player " + playerId + " is gone while awaiting a respawn choice - dropping the awaiting entry", LogLevel.WARNING);
			return;
		}

		// 3. Something else gave them a LIVING character - the degrade path, an admin, a future
		//    feature. A corpse is not that: it is what they are waiting to be rid of, and treating it
		//    as a character here would strand them permanently in the one state this whole region
		//    exists to end.
		IEntity controlled = playerController.GetControlledEntity();
		if (controlled && !OVT_PlayerManagerComponent.IsCharacterDead(controlled))
		{
			DropAwaitingRespawn(playerId);
			Print("[Overthrow] Player " + playerId + " has a character again by other means - they are no longer awaiting a respawn choice");
			return;
		}

		// 4. Still dead, still connected, still choosing. Ask again and come back.
		//
		// A missing component here can only mean their controller entity went away while they stayed
		// connected, which nothing in the tree does. It is loud rather than fatal: rescheduling keeps
		// the only exit that can still spawn them open, whereas dropping the entry would strand them
		// and spawning them would put them somewhere they never picked.
		OVT_RespawnRequestComponent requests = FindRespawnRequests(playerId);
		if (requests)
			requests.AskShowRespawnScreen(playerId);
		else
			Print("[Overthrow] Player " + playerId + " is awaiting a respawn choice but their respawn request component has vanished - cannot show them the screen", LogLevel.WARNING);

		GetGame().GetCallqueue().CallLater(ReAskRespawnScreen, RESPAWN_REASK_MS, false, playerId, characterPersistenceId);
	}

	//------------------------------------------------------------------------------------------------
	//! Creates the character a player has just chosen the location for.
	//!
	//! The only entry point OVT_RespawnRequestComponent calls, and the only place a respawn choice
	//! turns into a character. It is reached having already been range-checked and identity-resolved
	//! by that component; what it adds is the one-shot claim and the position decision.
	//!
	//! ! IT CALLS THE FRESH-CHARACTER PATH DIRECTLY AND NEVER CreateCharacter(), deliberately. Death
	//! has already cleared the stored body id, so the persisted-body route could not do anything
	//! anyway - and going nowhere near it means a respawn choice can never interact with body
	//! restoration, the pending-body-spawn set, or the spawn-request timeout.
	//!
	//! HOME NEEDS NO CODE HERE AND THAT IS WHY IT ALWAYS WORKS. It routes through the untouched
	//! two-argument fresh-character path, whose position chain already handles an unsafe or unassigned
	//! home. Home is never enumerated, never eligibility-checked and never QRF-checked, so "respawn at
	//! home is always available, even inside an active QRF" is true by construction rather than by a
	//! special case that could be got wrong.
	//!
	//! ! WHAT OK ACTUALLY PROMISES, precisely: that the claim was consumed and the untouched
	//! fresh-character path was entered. That path returns nothing and can still fail inside itself -
	//! the player left, or the character prefab would not spawn - and this cannot see it, so a failure
	//! there leaves a player with no claim, no character and a client that was told OK. Deliberately
	//! not papered over here: making the spawn path report success means threading a return value
	//! through the method every non-respawn spawn also uses, and re-arming the claim on a guess would
	//! flash a failure hint on the common path every time possession had not registered yet. The
	//! residual case needs the player prefab itself to fail to spawn, which strands players at their
	//! FIRST spawn too, by the same route, with the same ERROR line - it is not a hazard this feature
	//! introduces.
	//! \param[in] playerId The player who chose, resolved server-side from their controller entity.
	//! \param[in] destination An OVT_RespawnDestination value, already range-checked by the caller.
	//! \param[in] requestedPos The position the client named. A lookup key, never a spawn position.
	//! \return An OVT_RespawnResult code.
	int CompleteRespawn(int playerId, int destination, vector requestedPos)
	{
		if (!Replication.IsServer())
			return OVT_RespawnResult.SPAWN_FAILED;

		string characterPersistenceId = OVT_Global.GetPlayerUID(playerId);
		if (!characterPersistenceId)
		{
			// Their awaiting entry is deliberately left alone: an id that cannot be resolved now may
			// resolve on the next re-ask, and consuming the claim would take away their last exit.
			Print("[Overthrow] Cannot complete a respawn for player " + playerId + " - no persistent UID", LogLevel.WARNING);
			return OVT_RespawnResult.NO_PLAYER;
		}

		// THE ONE-SHOT CLAIM, AND IT IS BEFORE EVERY SPAWNING BRANCH FOR ONE REASON: this is what makes
		// two characters for one death impossible. A duplicate click, a re-sent packet, a request that
		// races the re-ask tick, or a request from a player who is not dead at all all arrive here,
		// find no entry, and leave without building anything.
		if (!DropAwaitingRespawn(playerId))
		{
			Print("[Overthrow] Respawn request from player " + playerId + " ignored - they are not awaiting a respawn choice", LogLevel.WARNING);

			// NOT_ELIGIBLE rather than SPAWN_FAILED, because SPAWN_FAILED promises the player is still
			// awaiting and must be re-asked - and the commonest way to get here is a second click from
			// somebody whose first click already worked.
			return OVT_RespawnResult.NOT_ELIGIBLE;
		}

		if (destination == OVT_RespawnDestination.HOME)
		{
			CreateFreshCharacter(playerId, characterPersistenceId);
			return OVT_RespawnResult.OK;
		}

		// The client named a place; the server looks it up in its OWN enumeration and spawns at its own
		// recorded vector. requestedPos is a key and never reaches the spawn.
		vector resolvedPos;
		int resolved = OVT_RespawnService.ResolveRespawnPosition(characterPersistenceId, destination, requestedPos, resolvedPos);
		if (resolved != OVT_RespawnResult.OK)
		{
			// The location went ineligible between the screen drawing it and the pick arriving - the
			// base fell, the camp was made private, a QRF started - or the vector matched nothing the
			// server offers. Falling back to home rather than refusing is the difference between "you
			// are somewhere you did not choose" and "you are still dead": the player is always alive at
			// the end of this, and the result code is what tells them why they are at home.
			Print("[Overthrow] Player " + playerId + " picked a respawn location the server does not offer - sending them home instead", LogLevel.WARNING);
			CreateFreshCharacter(playerId, characterPersistenceId);
			return OVT_RespawnResult.OK_FELL_BACK_HOME;
		}

		CreateFreshCharacterAt(playerId, characterPersistenceId, true, resolvedPos);
		return OVT_RespawnResult.OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Consumes one player's awaiting-respawn claim.
	//!
	//! Find-then-Remove, the same idiom m_aPendingBodySpawns uses, so that "was this player awaiting?"
	//! and "they are not any more" are one indivisible answer at every call site.
	//! \param[in] playerId The player to drop.
	//! \return True when an entry was found and removed, false when there was none.
	protected bool DropAwaitingRespawn(int playerId)
	{
		int index = m_aAwaitingRespawn.Find(playerId);
		if (index == -1)
			return false;

		m_aAwaitingRespawn.Remove(index);
		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! One player's respawn request component, resolved on the server from their controller entity.
	//!
	//! Deliberately NOT OVT_Global.GetRespawnRequests(), which resolves the LOCAL machine's controller
	//! and would answer with the host's own component for every player on a listen server.
	//! \param[in] playerId The player whose component to find.
	//! \return The component, or null when the player has no controller entity or it lacks one.
	protected OVT_RespawnRequestComponent FindRespawnRequests(int playerId)
	{
		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (!players)
			return null;

		OVT_OverthrowController controller = players.GetController(playerId);
		if (!controller)
			return null;

		return OVT_RespawnRequestComponent.Cast(controller.FindComponent(OVT_RespawnRequestComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Ends a leaving player's respawn wait without spawning them.
	//!
	//! NO CHARACTER IS CREATED HERE, on purpose. A player who reconnects is spawned by the normal join
	//! path, and death has already cleared both their stored body id and their last known position, so
	//! that path puts them at home - which is where the respawn screen's guaranteed option would have
	//! put them anyway. Building a character for somebody who is not here would leave it standing
	//! unowned in the world.
	//!
	//! The entry must be dropped rather than left to lapse because runtime player ids are session
	//! scoped and can be reused: a later joiner inheriting this number must not inherit a live claim
	//! to be spawned.
	//! \param[in] playerId The player who left.
	//! \param[in] cause Why they left. Passed straight to vanilla.
	//! \param[in] timeout Disconnect timeout. Passed straight to vanilla.
	override void OnPlayerDisconnected_S(int playerId, KickCauseCode cause, int timeout)
	{
		super.OnPlayerDisconnected_S(playerId, cause, timeout);

		if (!DropAwaitingRespawn(playerId))
			return;

		Print("[Overthrow] Player " + playerId + " left while awaiting a respawn choice - dropping the awaiting entry, they will spawn at home when they return");
	}

	//------------------------------------------------------------------------------------------------

}
