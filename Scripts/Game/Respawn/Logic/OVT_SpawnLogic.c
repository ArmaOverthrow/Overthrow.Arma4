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
//!   3. Respawn after death. The old base re-created the character one frame after OnPlayerKilled_S;
//!      that is preserved, and death now also CLEARS the stored body id so the rebuild can only ever be
//!      a fresh civilian one. What is NOT ported is its dead-body dance (fresh id for the corpse, force
//!      self-spawn): the corpse is matched to its own SelfSpawn 1 configuration instead - see
//!      OVT_DeadCharacterPersistenceConfigRule.
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
	//! \param[in] playerId The player who will control the character.
	//! \param[in] characterPersistenceId The player's Overthrow persistent id, used to pick the position.
	protected void CreateFreshCharacter(int playerId, string characterPersistenceId)
	{
		// Every route here is asynchronous - a deferred call, a failed spawn request, a timeout - so
		// re-check the player before building anything. Spawning first and asking afterwards would
		// leave a dressed, networked character with no owner when the player has left (HandoverToPlayer
		// logs and returns, deleting nothing). A LIVING controlled entity means somebody else already
		// finished the job; a dead one is the death-respawn path, which must proceed.
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

		vector position, yawPitchRoll;
		GetCreationPosition(playerId, characterPersistenceId, position, yawPitchRoll);

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
		HandoverToPlayer(playerId, character);
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
			return false;

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
		// disconnect handler has already run without a character to write, so do its job here instead:
		// save-and-release keeps the character and its gear for the next time they connect.
		PlayerController playerController = GetGame().GetPlayerManager().GetPlayerController(playerId);
		if (!playerController)
		{
			if (bodyEntity)
			{
				Print("[Overthrow] Player " + playerId + " left before their stored body arrived - releasing it again");
				OVT_PersistenceTracking.Save(bodyEntity);
				OVT_PersistenceTracking.Untrack(bodyEntity, true);
				SCR_EntityHelper.DeleteEntityAndChildren(bodyEntity);
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

		SetCivilianFaction(playerId);

		int groupId = group.GetGroupID();
		//Is the player not already in a group?
		if(groupId == -1)
		{
			SCR_GroupsManagerComponent groupsManager = SCR_GroupsManagerComponent.GetInstance();
			if (!groupsManager)
			{
				// Groups manager not ready, retry later
				GetGame().GetCallqueue().CallLater(CreateAndJoinGroupDelayed, 500, false, playerId, 0);
				return;
			}

			SCR_FactionManager factionManager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
			if (!factionManager)
				return;

			Faction faction = factionManager.GetPlayerFaction(playerId);
			if (!faction)
				return;

			SCR_AIGroup newGroup = groupsManager.CreateNewPlayableGroup(faction);
			if (!newGroup)
			{
				Print("[Overthrow] Failed to create group for player " + playerId, LogLevel.WARNING);
				return;
			}

			string playerName = GetGame().GetPlayerManager().GetPlayerName(playerId);
			newGroup.SetName(playerName);

			int groupID = newGroup.GetGroupID();

			SCR_PlayerControllerGroupComponent groupController = SCR_PlayerControllerGroupComponent.GetPlayerControllerComponent(playerId);
			if (!groupController)
			{
				Print("[Overthrow] Failed to get group controller for player " + playerId, LogLevel.WARNING);
				return;
			}

			groupController.RequestJoinGroup(groupID);

			Print("[Overthrow] Created group " + groupID + " for player " + playerName + " (ID: " + playerId + ")", LogLevel.NORMAL);
			Print("[Overthrow] Group faction: " + faction.GetFactionKey() + ", Player entity: " + playerController.GetControlledEntity(), LogLevel.NORMAL);

			// Fire the group created event
			m_OnPlayerGroupCreated.Invoke(playerId, groupID, playerName);
		}
		else
		{
			Print("[Overthrow] Player " + playerId + " already in group " + groupId, LogLevel.NORMAL);
		}
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
		PlayerController playerController = GetGame().GetPlayerManager().GetPlayerController(playerId);
		if (!playerController) return;

		SCR_PlayerFactionAffiliationComponent factionComponent = SCR_PlayerFactionAffiliationComponent.Cast(playerController.FindComponent(SCR_PlayerFactionAffiliationComponent));
		if (!factionComponent) return;

		FactionManager mgr = GetGame().GetFactionManager();
		if (!mgr) return;

		Faction playerFaction = mgr.GetFactionByKey(OVT_Global.GetConfig().GetPlayerFaction().GetFactionKey());
		if (!playerFaction) return;

		// This properly triggers faction manager updates and replication
		factionComponent.RequestFaction(playerFaction);
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

		int selection = s_AIRandomGenerator.RandInt(0, loadoutItem.m_aChoices.Count() - 1);
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
	//! Called on the server when a player is killed. Charges the respawn cost and rebuilds a character.
	//!
	//! The rebuild is deferred by one frame, as the old persistence-framework base did: a new character
	//! cannot be handed over on the same frame the old one died.
	//!
	//! DEATH IS COMPLETE LOSS, AND THIS IS WHERE THAT IS ENFORCED. The player's record carries the
	//! persistence id of the body they were using, and CreateCharacter() would otherwise spawn that exact
	//! body - gear included - straight back. Clearing the id before the rebuild is scheduled is what
	//! guarantees a killed player always takes the fresh-civilian route, in this session and in every
	//! session after it.
	//!
	//! The corpse is left in the world deliberately: it is lootable remains, and it now persists in its
	//! own right. The old persistence framework forced it to take a fresh id and to self-spawn so it could
	//! be looted after a restart; the same end is now reached through configuration -
	//! OVT_DeadCharacterPersistenceConfigRule matches a dead character to a SelfSpawn 1 config, and
	//! OVT_OverthrowGameMode's kill hook asks for the re-match. (Vanilla reaches it through
	//! SCR_SpawnLogic.OnPlayerEntityChanged_S, which Overthrow never travels: that is raised by
	//! SCR_RespawnSystemComponent.EmitPlayerEntityChange_S, and Overthrow hands characters over itself.)
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
			player.m_sBodyPersistenceId = "";

		GetGame().GetCallqueue().Call(CreateCharacter, playerId, playerUid);
	}

	//------------------------------------------------------------------------------------------------

}
