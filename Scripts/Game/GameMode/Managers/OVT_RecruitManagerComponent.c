//! Manager component for AI recruit system
[BaseContainerProps(configRoot: true)]
class OVT_RecruitManagerComponentClass : OVT_ComponentClass
{
}

//! Manages AI recruits for all players
class OVT_RecruitManagerComponent : OVT_Component
{
	//! Maximum number of recruits per player
	static const int MAX_RECRUITS_PER_PLAYER = 16;

	//! How far the requesting player may be from a recruitment tent, in metres.
	//!
	//! The magic 20 both tent paths used before it had a name. It is not a precision instrument: the
	//! tent's own action radius is under a metre, so this only has to make a forged request from
	//! across the map fail.
	static const float TENT_MAX_DISTANCE = 20;

	//! How far in front of a recruitment tent a recruit is put down, in metres.
	//!
	//! Along the TENT'S forward axis, not a world direction - see ResolveTentSpawnPosition(). Far
	//! enough to clear the tent's own footprint, close enough that "at the tent" is the honest
	//! description of where the recruit appears.
	static const float TENT_SPAWN_FORWARD_OFFSET = 4;

	//! The shipped fixed offset, kept for the caller that has no tent entity to take a facing from.
	static const vector TENT_SPAWN_FALLBACK_OFFSET = "2 0 2";

	//! How far a tent recruit may scatter from its spawn anchor, in metres.
	//!
	//! Uniform over a disc, so back-to-back recruits do not stack on one point while they wait for
	//! their run to formation. Small enough that the whole disc stays inside the spawn point's
	//! cleared area in front of the tent.
	static const float TENT_SPAWN_SCATTER_RADIUS = 3;

	//! How far around a bare tent position to look for the tent entity itself, in metres.
	//!
	//! The legacy tent action's RPC only ever carries a position, and its action sits on the tent's
	//! table child - so the tent ROOT is always within a couple of metres of what it sends. Wide
	//! enough to absorb that, narrow enough that two tents would have to overlap to confuse it.
	static const float TENT_LOOKUP_RADIUS = 10;

	//! ValidateTentRecruit(): the request may proceed.
	static const int TENT_RECRUIT_OK = 0;

	//! ValidateTentRecruit(): a manager the check needs is missing. Not reachable in a live campaign.
	static const int TENT_RECRUIT_UNAVAILABLE = 1;

	//! ValidateTentRecruit(): the requester has no persistent id, or no body in the world.
	static const int TENT_RECRUIT_NO_IDENTITY = 2;

	//! ValidateTentRecruit(): the requester is already at MAX_RECRUITS_PER_PLAYER.
	static const int TENT_RECRUIT_AT_CAP = 3;

	//! ValidateTentRecruit(): the nearest town has no supporter to give.
	static const int TENT_RECRUIT_NO_SUPPORTERS = 4;

	//! ValidateTentRecruit(): the requester is further than TENT_MAX_DISTANCE from the tent.
	static const int TENT_RECRUIT_TOO_FAR = 5;

	//! Prefab to use for spawning recruit bodies (both new recruits and restored ones)
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Recruit Character Prefab", params: "et")]
	ResourceName m_sRecruitPrefab;

	//! Empty, non-playable, delete-when-empty AI group that INACTIVE recruits are parked in.
	//!
	//! One is spawned per cluster of parked recruits and destroyed by vanilla when its last member
	//! leaves - see PlaceRecruitInInactiveGroup(). It must carry OVT_InactiveRecruitGroupComponent
	//! or the manager will refuse to use it: that component is the only way a group is recognised as
	//! one of ours, and it is what deletes the group's defend waypoint.
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Inactive Recruit Group Prefab", params: "et")]
	ResourceName m_sInactiveGroupPrefab;

	//! Singleton instance
	static OVT_RecruitManagerComponent s_Instance;
	
	//! All recruits indexed by recruit ID
	[NonSerialized()]
	ref map<string, ref OVT_RecruitData> m_mRecruits;
	
	//! Recruit IDs indexed by owner persistent ID
	[NonSerialized()]
	ref map<string, ref array<string>> m_mRecruitsByOwner;
	
	//! Map of entity IDs to recruit IDs for quick lookup
	[NonSerialized()]
	ref map<EntityID, string> m_mEntityToRecruit;
	
	//! Client-side mapping using replication IDs for cross-network entity identification
	[NonSerialized()]
	ref map<RplId, string> m_mRplIdToRecruit;
	
	//! Offline player timers for recruit despawning
	[NonSerialized()]
	ref map<string, float> m_mOfflinePlayerTimers;

	//! Despawn time for recruits when player is offline (10 minutes)
	static const float OFFLINE_DESPAWN_TIME = 600.0;

	//! Recruit ids whose stored body is being fetched from the persistence system right now.
	//!
	//! PersistenceSystem.RequestSpawn() is ASYNCHRONOUS, so between the request and its callback a
	//! recruit has neither a body in the world nor a request that anything can see. Without this list
	//! the "already in world" guard in RespawnPlayerRecruits() would let a second call start a second
	//! request for the same recruit and end up with two bodies.
	[NonSerialized()]
	ref array<string> m_aPendingBodySpawns;

	//! The persistence collection recruit bodies belong to, resolved once and cached.
	//!
	//! Held without `ref` on purpose: the collection is owned by the persistence system and is only
	//! obtainable from it (PersistenceCollection is sealed with a private constructor). This mirrors
	//! SCR_SpawnLogic.m_CharacterCollection exactly.
	protected PersistenceCollection m_RecruitBodyCollection;

	//! Scratch for FindTentAtPosition()'s query filter; only meaningful during the query.
	protected IEntity m_TentSearched;

	//! Name of the collection recruit bodies are stored in.
	//!
	//! VERIFIED, not assumed. A recruit prefab inherits Prefabs/Characters/Core/Character_Base.et,
	//! which carries the native Persistence component, and is matched by vanilla's AI-unit config
	//! {64EACAC5BFDB31EC} (Configuration/AI/AIUnit.conf, which inherits Character.conf's prefab rule).
	//! Vanilla Common.conf:95-97 puts that config in collection {64EACAC5B77ED31B}, whose Name is
	//! "Character" (Common.conf:7-10). Overthrow's override of the same GUID only sets SelfSpawn 0 -
	//! it changes neither the collection nor the inventory serializers.
	static const string RECRUIT_BODY_COLLECTION = "Character";

	//! How long to wait for a spawn request to answer before giving the recruit a fresh body (ms).
	static const int BODY_SPAWN_TIMEOUT_MS = 15000;

	//! How often the server re-reads every online owner's recruits and pushes status to that owner.
	//!
	//! TEN SECONDS IS A DELIBERATE FLOOR, NOT A GUESS. At the 16-recruit cap this is 1.6 owner-
	//! targeted RPCs per second per player, which is negligible; the reason not to go faster is the
	//! READ, not the send - each recruit costs a weapon-slot walk and an inventory magazine query.
	//! Nothing in this feature needs sub-second accuracy: the map marker and the roster row show
	//! "does this recruit have a gun and bullets", which changes on the scale of a firefight, not a
	//! frame.
	static const int STATUS_SYNC_INTERVAL_MS = 10000;

	//! Hold timer for a parked recruit's wait waypoint (BUG-170). One day of continuous session,
	//! i.e. never in practice - the inactive groups are session-scoped and rebuilt on every boot.
	static const float INACTIVE_HOLD_WAIT_SECONDS = 86400;

	//! Event fired when a recruit is added
	ref ScriptInvoker m_OnRecruitAdded = new ScriptInvoker();
	
	//! Event fired when a recruit is removed
	ref ScriptInvoker m_OnRecruitRemoved = new ScriptInvoker();
	
	//! Event fired when a recruit gains XP
	ref ScriptInvoker m_OnRecruitXPGained = new ScriptInvoker();

	//! Event fired when a recruit is parked or brought back into its owner's squad.
	//! Invoked with (OVT_RecruitData recruit, bool inactive).
	//!
	//! FIRED FROM BOTH SIDES OF THE WIRE, exactly once per machine: on the server (and the listen
	//! host) from SetRecruitInactive after the record is written, and on a remote client from
	//! RpcDo_RecruitActiveStateChanged. The RPC handler self-guards on RplMode.Client, so a listen
	//! host does not get it twice.
	ref ScriptInvoker m_OnRecruitActiveStateChanged = new ScriptInvoker();
	
	//------------------------------------------------------------------------------------------------
	//! Get singleton instance
	static OVT_RecruitManagerComponent GetInstance()
	{
		return s_Instance;
	}
	
	//------------------------------------------------------------------------------------------------
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		
		s_Instance = this;
		
		m_mRecruits = new map<string, ref OVT_RecruitData>;
		m_mRecruitsByOwner = new map<string, ref array<string>>;
		m_mEntityToRecruit = new map<EntityID, string>;
		m_mRplIdToRecruit = new map<RplId, string>;
		m_mOfflinePlayerTimers = new map<string, float>;
		m_aPendingBodySpawns = new array<string>;

		SetEventMask(owner, EntityEvent.INIT);
		
		if (SCR_Global.IsEditMode())
			return;
			
		// Connect to player events
		OVT_PlayerManagerComponent playerManager = OVT_Global.GetPlayers();
		if (playerManager)
		{
			playerManager.m_OnPlayerConnected.Insert(OnPlayerConnected);
			playerManager.m_OnPlayerDisconnected.Insert(OnPlayerDisconnected);
		}
		
		// Connect to respawn system events
		OVT_RespawnSystemComponent respawnSystem = OVT_RespawnSystemComponent.Cast(owner.FindComponent(OVT_RespawnSystemComponent));
		if (respawnSystem)
		{
			ScriptInvoker onGroupCreated = respawnSystem.GetOnPlayerGroupCreated();
			if (onGroupCreated)
				onGroupCreated.Insert(OnPlayerGroupCreated);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		
		if (!GetGame().InPlayMode())
			return;
			
		// Subscribe to universal character death events
		OVT_OverthrowGameMode overthrowGameMode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if (overthrowGameMode)
		{
			overthrowGameMode.GetOnCharacterKilled().Insert(OnCharacterKilled);
		}
		
		// Subscribe to AI kills for XP tracking (still needed for faction-specific rewards)
		OVT_OccupyingFactionManager occupyingFaction = OVT_Global.GetOccupyingFaction();
		if (occupyingFaction)
		{
			occupyingFaction.m_OnAIKilled.Insert(OnAIKilled);
		}
		
		// Subscribe to player connect/disconnect events
		OVT_PlayerManagerComponent playerManager = OVT_Global.GetPlayers();
		if (playerManager)
		{
			playerManager.m_OnPlayerConnected.Insert(OnPlayerConnected);
			playerManager.m_OnPlayerDisconnected.Insert(OnPlayerDisconnected);
		}
		
		// Start offline timer processing
		GetGame().GetCallqueue().CallLater(ProcessOfflineTimers, 1000, true);

		// Start the recruit status push. Started UNCONDITIONALLY and guarded inside the tick rather
		// than guarded here: Replication.IsServer() at EOnInit time is not something this component
		// has ever depended on, and a timer that starts and then refuses to do anything costs one
		// null-ish branch every ten seconds on a client, while a timer that never starts because the
		// guard read false too early would cost the whole feature with no symptom.
		GetGame().GetCallqueue().CallLater(SweepRecruitStatus, STATUS_SYNC_INTERVAL_MS, true);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get number of recruits owned by a player
	int GetRecruitCount(string playerPersistentId)
	{
		if (!m_mRecruitsByOwner.Contains(playerPersistentId))
			return 0;
			
		return m_mRecruitsByOwner[playerPersistentId].Count();
	}
	
	//------------------------------------------------------------------------------------------------
	//! Check if player can recruit more AI
	bool CanRecruit(string playerPersistentId)
	{
		return GetRecruitCount(playerPersistentId) < MAX_RECRUITS_PER_PLAYER;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get all recruits owned by a player
	array<ref OVT_RecruitData> GetPlayerRecruits(string playerPersistentId)
	{
		array<ref OVT_RecruitData> recruits = new array<ref OVT_RecruitData>;
		
		if (!m_mRecruitsByOwner.Contains(playerPersistentId))
			return recruits;
			
		array<string> recruitIds = m_mRecruitsByOwner[playerPersistentId];
		foreach (string recruitId : recruitIds)
		{
			OVT_RecruitData recruit = m_mRecruits[recruitId];
			if (recruit)
				recruits.Insert(recruit);
		}
		
		return recruits;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Whether a recruit is currently INACTIVE - owned, but out of its owner's group holding position.
	//!
	//! STATE ONLY. This reads the record and nothing else; it says nothing about whether the recruit
	//! has a body (m_bIsOnline) or where that body is. An unknown recruit id is ACTIVE rather than an
	//! error, because every caller of this is a display or a filter and "not inactive" is the safe
	//! answer for something that does not exist.
	//! \param[in] recruitId The recruit to look up.
	//! \return True when the record exists and is marked inactive.
	bool IsRecruitInactive(string recruitId)
	{
		OVT_RecruitData recruit = GetRecruit(recruitId);
		if (!recruit)
			return false;

		return recruit.m_bInactive;
	}

	//------------------------------------------------------------------------------------------------
	//! A player's recruits filtered by active/inactive state, in table order.
	//!
	//! The two halves of GetPlayerRecruits(), so that a caller that wants one section of the roster
	//! does not have to re-implement the filter. Table order is preserved: it is what makes a roster
	//! screen and a failing play-test agree on which recruit is "the third one".
	//! \param[in] persId Owning player's persistent id.
	//! \param[in] inactive True for the inactive half, false for the active half.
	//! \return A fresh list, empty when the player owns no recruits in that state. Never null.
	array<ref OVT_RecruitData> GetPlayerRecruitsByState(string persId, bool inactive)
	{
		array<ref OVT_RecruitData> filtered = new array<ref OVT_RecruitData>;

		// NOT named `owned`: that is a reserved EnforceScript keyword and the compile error it produces
		// ("Expected name, not a keyword") names the line, not the identifier.
		array<ref OVT_RecruitData> ownedRecruits = GetPlayerRecruits(persId);
		foreach (OVT_RecruitData recruit : ownedRecruits)
		{
			if (recruit.m_bInactive == inactive)
				filtered.Insert(recruit);
		}

		return filtered;
	}

	//------------------------------------------------------------------------------------------------
	//! Get recruit data by ID
	OVT_RecruitData GetRecruit(string recruitId)
	{
		if (!m_mRecruits.Contains(recruitId))
			return null;
			
		return m_mRecruits[recruitId];
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get recruit data from character entity
	OVT_RecruitData GetRecruitFromEntity(IEntity entity)
	{
		if (!entity)
			return null;
		
		// On server, use entity ID mapping
		if (Replication.IsServer())
		{
			EntityID entityId = entity.GetID();
			if (!m_mEntityToRecruit.Contains(entityId))
				return null;
				
			string recruitId = m_mEntityToRecruit[entityId];
			return GetRecruit(recruitId);
		}
		
		// On client, use replication ID mapping
		RplComponent rplComponent = RplComponent.Cast(entity.FindComponent(RplComponent));
		if (!rplComponent)
			return null;
			
		RplId rplId = rplComponent.Id();
		if (!m_mRplIdToRecruit.Contains(rplId))
			return null;
			
		string recruitId = m_mRplIdToRecruit[rplId];
		return GetRecruit(recruitId);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get recruit entity by recruit ID
	IEntity GetRecruitEntity(string recruitId)
	{
		return FindRecruitEntity(recruitId);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get recruits owned by a player within a specified radius of a position
	//!
	//! The RECORD-shaped twin of GetPlayerRecruitEntitiesInRadius(), and it takes the same option for
	//! the same reason: the fast-travel fare is previewed on the CLIENT from this method and charged on
	//! the SERVER from that one, so if only one of them dropped parked recruits the panel would price a
	//! squad the server does not gather - exactly the client/server drift OVT_FastTravelService exists
	//! to prevent. Safe on a client: m_bInactive arrives with the JIP payload and is kept current by
	//! RpcDo_RecruitActiveStateChanged.
	//!
	//! \param[in] playerPersistentId The owning player.
	//! \param[in] position Centre of the search.
	//! \param[in] radius Search radius in metres.
	//! \param[in] excludeInactive Leave PARKED recruits out of the answer. Defaults to false, so an
	//!            existing caller keeps the answer it always had.
	//! \return The matching records. Never null.
	array<ref OVT_RecruitData> GetPlayerRecruitsInRadius(string playerPersistentId, vector position, float radius, bool excludeInactive = false)
	{
		array<ref OVT_RecruitData> nearbyRecruits = new array<ref OVT_RecruitData>;

		if (!m_mRecruitsByOwner.Contains(playerPersistentId))
			return nearbyRecruits;

		array<string> recruitIds = m_mRecruitsByOwner[playerPersistentId];
		foreach (string recruitId : recruitIds)
		{
			OVT_RecruitData recruit = m_mRecruits[recruitId];
			if (!recruit || !recruit.m_bIsOnline)
				continue;

			if (excludeInactive && recruit.m_bInactive)
				continue;

			// Find the recruit entity to get current position
			IEntity recruitEntity = FindRecruitEntity(recruitId);
			if (!recruitEntity)
				continue;
				
			// Check if recruit is within radius
			float distance = vector.Distance(position, recruitEntity.GetOrigin());
			if (distance <= radius)
			{
				nearbyRecruits.Insert(recruit);
			}
		}
		
		return nearbyRecruits;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get recruit entities owned by a player within a specified radius of a position
	//!
	//! \param[in] playerPersistentId The owning player.
	//! \param[in] position Centre of the search.
	//! \param[in] radius Search radius in metres.
	//! \param[in] excludeInactive Leave PARKED recruits out of the answer. Defaults to false because
	//!            the majority of callers - the loadouts screen, for one - are asking "which of my
	//!            recruits am I standing next to", and an inactive recruit beside you is still
	//!            equippable. Fast travel passes TRUE: a parked recruit is parked precisely so that it
	//!            stays where it is, and dragging a garrison along would be the opposite of the order
	//!            the player gave it.
	//! \return The matching bodies. Never null.
	array<IEntity> GetPlayerRecruitEntitiesInRadius(string playerPersistentId, vector position, float radius, bool excludeInactive = false)
	{
		array<IEntity> nearbyRecruitEntities = new array<IEntity>;

		if (!m_mRecruitsByOwner.Contains(playerPersistentId))
			return nearbyRecruitEntities;

		array<string> recruitIds = m_mRecruitsByOwner[playerPersistentId];
		foreach (string recruitId : recruitIds)
		{
			OVT_RecruitData recruit = m_mRecruits[recruitId];
			if (!recruit || !recruit.m_bIsOnline)
				continue;

			if (excludeInactive && recruit.m_bInactive)
				continue;

			// Find the recruit entity to get current position
			IEntity recruitEntity = FindRecruitEntity(recruitId);
			if (!recruitEntity)
				continue;
				
			// Check if recruit is within radius
			float distance = vector.Distance(position, recruitEntity.GetOrigin());
			if (distance <= radius)
			{
				nearbyRecruitEntities.Insert(recruitEntity);
			}
		}
		
		return nearbyRecruitEntities;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Applies persisted recruit records to the live manager.
	//!
	//! Called from OVT_RecruitManagerSerializer.Deserialize().
	//!
	//! The owner index is REBUILT from the records rather than restored from the save, so the two can
	//! never disagree. m_bIsOnline is left alone: a record that already exists keeps whatever the live
	//! session knows about its body, and a record being recreated from storage starts offline because
	//! the body is spawned back when its owner returns (RespawnPlayerRecruits).
	//!
	//! THE BODY ID IS ADOPTED ONLY WHEN THE LIVE RECORD HAS NONE, for the same reason m_bIsOnline is
	//! left alone: on a real load the record is brand new and takes the stored id; when saved data is
	//! re-applied to a RUNNING campaign the live record already points at the body standing in the
	//! world, and that is the more current fact.
	//!
	//! THE INACTIVE FLAG IS ADOPTED UNCONDITIONALLY, because unlike the two exceptions above it is a
	//! campaign fact rather than a session fact - see the comment at the assignment.
	//!
	//! NO RPC. Clients receive the whole table through RplSave/RplLoad instead - see the serializer.
	//!
	//! IDEMPOTENT: existing records are filled in place and owner lists never gain duplicates.
	//! \param[in] records Persisted recruit records, may be null.
	void ApplyPersistedRecruits(array<ref OVT_PersistedRecruit> records)
	{
		if (!records)
			return;

		if (!m_mRecruits)
			m_mRecruits = new map<string, ref OVT_RecruitData>;

		if (!m_mRecruitsByOwner)
			m_mRecruitsByOwner = new map<string, ref array<string>>;

		foreach (OVT_PersistedRecruit record : records)
		{
			if (!record)
				continue;

			if (record.recruitId == "" || record.ownerPersistentId == "")
			{
				Print("[Overthrow] Skipping a saved recruit with no id or no owner", LogLevel.WARNING);
				continue;
			}

			OVT_RecruitData recruit = GetRecruit(record.recruitId);
			if (!recruit)
			{
				recruit = new OVT_RecruitData();
				recruit.m_sRecruitId = record.recruitId;
				recruit.m_bIsOnline = false;
				m_mRecruits[record.recruitId] = recruit;
			}

			recruit.m_sOwnerPersistentId = record.ownerPersistentId;
			recruit.m_sName = record.name;
			recruit.m_iKills = record.kills;
			recruit.m_iXP = record.xp;
			recruit.m_iLevel = record.level;
			recruit.m_bIsTraining = record.isTraining;
			recruit.m_fTrainingCompleteTime = record.trainingCompleteTime;
			recruit.m_vLastKnownPosition = record.lastKnownPosition;
			recruit.m_iTownId = record.townId;

			// ADOPTED UNCONDITIONALLY, like every other record field and unlike m_bIsOnline. Inactive
			// is a campaign fact the player chose - the same kind of thing as m_bIsTraining - not a
			// fact about this session, so the saved state is the one being restored and the live value
			// is what is being replaced. That is what the idempotency contract asks for: re-applying
			// saved data to a running campaign must reproduce the SAVED state of everything the save
			// describes, and only session-only facts (the online flag, the entity mapping, a live body
			// id) are protected from it.
			recruit.m_bInactive = record.inactive;

			if (recruit.m_sBodyPersistenceId == "")
				recruit.m_sBodyPersistenceId = record.bodyPersistenceId;

			ApplyPersistedRecruitSkills(recruit, record);

			if (!m_mRecruitsByOwner.Contains(record.ownerPersistentId))
				m_mRecruitsByOwner[record.ownerPersistentId] = new array<string>;

			array<string> ownerRecruits = m_mRecruitsByOwner[record.ownerPersistentId];
			if (ownerRecruits.Find(record.recruitId) == -1)
				ownerRecruits.Insert(record.recruitId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Rebuilds one recruit's skills from the parallel key/level arrays a save record carries.
	//! \param[in] recruit The live record being filled.
	//! \param[in] record The saved record being read.
	protected void ApplyPersistedRecruitSkills(notnull OVT_RecruitData recruit, notnull OVT_PersistedRecruit record)
	{
		if (!recruit.m_mSkills)
			recruit.m_mSkills = new map<string, int>();

		recruit.m_mSkills.Clear();

		if (!record.skillKeys || !record.skillLevels)
			return;

		int count = record.skillKeys.Count();
		if (record.skillLevels.Count() < count)
			count = record.skillLevels.Count();

		for (int i = 0; i < count; i++)
		{
			string key = record.skillKeys[i];
			if (key == "")
				continue;

			recruit.m_mSkills.Set(key, record.skillLevels[i]);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Add a new recruit
	string AddRecruit(string ownerPersistentId, IEntity characterEntity, string name = "")
	{
		if (!CanRecruit(ownerPersistentId))
			return "";
			
		// Generate unique record ID
		string recruitId = GenerateRecruitId(ownerPersistentId);
		
		// Create recruit data
		OVT_RecruitData recruit = new OVT_RecruitData();
		recruit.m_sRecruitId = recruitId;
		recruit.m_sOwnerPersistentId = ownerPersistentId;
		recruit.m_sName = name;
		
		if (name.IsEmpty())
		{
			// Get the civilian's actual name from the identity component
			SCR_CharacterIdentityComponent identity = SCR_CharacterIdentityComponent.Cast(characterEntity.FindComponent(SCR_CharacterIdentityComponent));
			if (identity)
			{
				string format, firstName, alias, surname;
				identity.GetFormattedFullName(format, firstName, alias, surname);
				
				// Build the full name manually instead of using the format string
				if (!alias.IsEmpty())
				{
					recruit.m_sName = firstName + " \"" + alias + "\" " + surname;
				}
				else
				{
					recruit.m_sName = firstName + " " + surname;
				}
				
				Print("[Overthrow] Extracted name from identity: " + recruit.m_sName);
			}
			else
			{
				recruit.m_sName = GenerateRecruitName();
			}
		}
			
		// Store position
		recruit.m_vLastKnownPosition = characterEntity.GetOrigin();
		
		// Set hometown to nearest town
		OVT_TownManagerComponent townManager = OVT_Global.GetTowns();
		if (townManager)
		{
			OVT_TownData nearestTown = townManager.GetNearestTown(characterEntity.GetOrigin());
			if (nearestTown)
			{
				recruit.m_iTownId = townManager.GetTownID(nearestTown);
			}
		}
		
		// Mark as online since entity exists
		recruit.m_bIsOnline = true;
		
		// Add to collections
		m_mRecruits[recruitId] = recruit;
		
		if (!m_mRecruitsByOwner.Contains(ownerPersistentId))
			m_mRecruitsByOwner[ownerPersistentId] = new array<string>;
			
		m_mRecruitsByOwner[ownerPersistentId].Insert(recruitId);
		
		// Map entity to recruit
		m_mEntityToRecruit[characterEntity.GetID()] = recruitId;

		// A town civilian is rebuild-on-boot AI, so BUG-118's spawn-side untracking has already
		// released this body - or still holds a queued release for it (registration is lazy).
		// Recruitment promotes the body into the recalled-by-id category, so withdraw any pending
		// release and register it again; without this the body never reaches a save and the
		// recruit's gear cannot survive one (BUG-131).
		OVT_PersistenceManagerComponent.CancelUntrackTransient(characterEntity);
		if (!OVT_PersistenceTracking.IsTracked(characterEntity))
			OVT_PersistenceTracking.Track(characterEntity);

		// The recruit id identifies the RECORD. The BODY has its own identity, handed out by the
		// persistence system, and remembering it here is what lets this exact character - with whatever
		// it is carrying - be spawned back later instead of a fresh one. Registration is lazy, so the
		// id may not resolve yet; the record then keeps an empty id until the pre-save sync
		// (SyncRecruitPositions) or the despawn path (ReserveRecruitBody) materialises it with a
		// Save().
		CaptureRecruitBodyId(recruit, characterEntity, false);

		m_OnRecruitAdded.Invoke(recruit);
		
		// Broadcast recruit creation to all clients
		BroadcastRecruitCreated(recruitId, ownerPersistentId, recruit.m_sName, recruit.m_vLastKnownPosition, characterEntity);
		
		return recruitId;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Remove a recruit
	void RemoveRecruit(string recruitId)
	{
		OVT_RecruitData recruit = GetRecruit(recruitId);
		if (!recruit)
			return;
		
		string ownerPersistentId = recruit.m_sOwnerPersistentId;
			
		// Remove from owner's list
		if (m_mRecruitsByOwner.Contains(recruit.m_sOwnerPersistentId))
		{
			array<string> ownerRecruits = m_mRecruitsByOwner[recruit.m_sOwnerPersistentId];
			ownerRecruits.RemoveItem(recruitId);
			
			if (ownerRecruits.IsEmpty())
				m_mRecruitsByOwner.Remove(recruit.m_sOwnerPersistentId);
		}
		
		// Remove from main collection
		m_mRecruits.Remove(recruitId);

		// Remove entity/replication lookups still pointing at this record, or GetRecruitFromEntity()
		// keeps resolving the dead ID for as long as the body lives (BUG-004). Scanned by value: the
		// body may have been remapped since this record last saw it.
		array<EntityID> staleEntityIds = {};
		foreach (EntityID entityId, string mappedRecruitId : m_mEntityToRecruit)
		{
			if (mappedRecruitId == recruitId)
				staleEntityIds.Insert(entityId);
		}
		foreach (EntityID staleEntityId : staleEntityIds)
		{
			m_mEntityToRecruit.Remove(staleEntityId);
		}

		array<RplId> staleRplIds = {};
		foreach (RplId rplId, string mappedRplRecruitId : m_mRplIdToRecruit)
		{
			if (mappedRplRecruitId == recruitId)
				staleRplIds.Insert(rplId);
		}
		foreach (RplId staleRplId : staleRplIds)
		{
			m_mRplIdToRecruit.Remove(staleRplId);
		}

		// Broadcast recruit removal to all clients
		BroadcastRecruitRemoved(recruitId, ownerPersistentId);
		
		m_OnRecruitRemoved.Invoke(recruit);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Add experience to a recruit
	void AddRecruitXP(string recruitId, int xp)
	{
		OVT_RecruitData recruit = GetRecruit(recruitId);
		if (!recruit)
			return;
			
		int oldLevel = recruit.GetLevel();
		recruit.AddXP(xp);
		int newLevel = recruit.GetLevel();

		m_OnRecruitXPGained.Invoke(recruit, xp);

		// XP and kills live only in the server's record; without this broadcast a dedicated-server
		// (or listen-host) client's roster shows 0 kills / 0 XP forever, since RpcDo_RecruitUpdated
		// is the only thing that writes them client-side outside the JIP snapshot.
		BroadcastRecruitUpdate(recruit);

		// Notify if leveled up
		if (newLevel > oldLevel)
		{
			OVT_PlayerData playerData = OVT_Global.GetPlayers().GetPlayer(recruit.m_sOwnerPersistentId);
			if (playerData && !playerData.IsOffline())
			{
				OVT_Global.GetNotify().SendTextNotification("RecruitLevelUp", playerData.id, recruit.m_sName, newLevel.ToString());
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Rename a recruit and update both data and CharacterIdentityComponent
	bool RenameRecruit(string recruitId, string newName)
	{
		OVT_RecruitData recruit = GetRecruit(recruitId);
		if (!recruit)
			return false;
			
		// Validate name
		if (newName.IsEmpty() || newName.Length() > 32)
			return false;
		
		// Update recruit name in data
		recruit.SetName(newName);
		
		// Update the entity's CharacterIdentityComponent for visual display
		IEntity recruitEntity = GetRecruitEntity(recruitId);
		if (recruitEntity)
		{
			CharacterIdentityComponent identityComponent = CharacterIdentityComponent.Cast(recruitEntity.FindComponent(CharacterIdentityComponent));
			if (identityComponent)
			{
				Identity identity = identityComponent.GetIdentity();
				if (identity)
				{
					// Parse the new name into parts (First, Last, or First Middle Last)
					array<string> nameParts = {};
					newName.Split(" ", nameParts, true); // true = keep empty strings
					
					if (nameParts.Count() == 1)
					{
						// Single name - set as first name, clear alias and surname
						identity.SetName(nameParts[0]);
						identity.SetAlias("");
						identity.SetSurname("");
					}
					else if (nameParts.Count() == 2)
					{
						// Two names - first and last, clear alias
						identity.SetName(nameParts[0]);
						identity.SetAlias("");
						identity.SetSurname(nameParts[1]);
					}
					else if (nameParts.Count() >= 3)
					{
						// Three or more names - first, middle as alias, last as surname
						identity.SetName(nameParts[0]);
						identity.SetAlias(nameParts[1]);
						identity.SetSurname(nameParts[2]);
					}
					
					// Commit changes to apply them
					identityComponent.CommitChanges();
				}
			}
		}
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Handle universal character death (works for all characters including recruits)
	protected void OnCharacterKilled(IEntity victim, IEntity instigator)
	{
		if (!victim)
			return;
			
		// Check if the victim is a recruit
		OVT_RecruitData victimRecruit = GetRecruitFromEntity(victim);
		if (!victimRecruit)
			return;
			
		// Notify owner before removing the recruit
		OVT_PlayerData ownerData = OVT_Global.GetPlayers().GetPlayer(victimRecruit.m_sOwnerPersistentId);
		if (ownerData && !ownerData.IsOffline())
		{
			OVT_Global.GetNotify().SendTextNotification("RecruitDied", ownerData.id, victimRecruit.m_sName);
		}

		Print("[Overthrow] Recruit died: " + victimRecruit.m_sRecruitId);

		// A DEAD RECRUIT MUST NOT COME BACK. Dropping the record is what does it - the next save point
		// simply does not contain the recruit (OVT_RecruitManagerSerializer writes the map) - but the
		// body id is cleared first so that nothing holding a reference to this record object can ask the
		// persistence system to spawn the corpse back. The body itself stays in the world as lootable
		// remains, exactly as before.
		victimRecruit.m_sBodyPersistenceId = "";

		// A pending spawn request is deliberately NOT cancelled here. A recruit with a request in flight
		// has no body in the world and so cannot be the victim, but if that ever changes, the callback's
		// "the record is gone" branch deletes the body it was handed and drops its stored data - whereas
		// forgetting the request would leak an unowned character into the world.

		// Remove entity mapping
		m_mEntityToRecruit.Remove(victim.GetID());

		// Remove the recruit entirely from the system
		RemoveRecruit(victimRecruit.m_sRecruitId);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Handle AI kills by recruits for XP rewards (recruit death detection moved to OnCharacterKilled)
	protected void OnAIKilled(IEntity victim, IEntity instigator)
	{
		if (!victim || !instigator)
			return;
			
		// Check if killer is a recruit (for XP rewards)
		OVT_RecruitData killerRecruit = GetRecruitFromEntity(instigator);
		if (!killerRecruit)
			return;
			
		// Check if victim is also a recruit - don't award XP for recruit vs recruit kills
		OVT_RecruitData victimRecruit = GetRecruitFromEntity(victim);
		if (victimRecruit)
			return;
			
		// Check faction to ensure victim is an enemy
		FactionAffiliationComponent victimFaction = FactionAffiliationComponent.Cast(victim.FindComponent(FactionAffiliationComponent));
		if (!victimFaction)
			return;
			
		Faction faction = victimFaction.GetAffiliatedFaction();
		if (!faction)
			return;
			
		// Only award XP for killing occupying faction members.
		//
		// BUG-107. This used to test against the hardcoded keys "US" and "USSR", which is the pair of
		// factions that HAPPEN to be the shipping choices - so a campaign configured with any other
		// occupier awarded recruits no XP at all, silently. The occupier is a config value; ask the
		// config. Precedent: SCR_CharacterDamageManagerComponent.c:91.
		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (!config)
			return;

		string factionKey = faction.GetFactionKey();
		if (factionKey != config.m_sOccupyingFaction)
			return;
			
		// Award XP
		killerRecruit.m_iKills++;
		AddRecruitXP(killerRecruit.m_sRecruitId, 10); // 10 XP per enemy kill
	}
	
	//------------------------------------------------------------------------------------------------
	//! Generate unique recruit record ID
	protected string GenerateRecruitId(string ownerPersistentId)
	{
		string randomId;
		int maxAttempts = 100;
		int attempts = 0;
		
		// Keep generating until we get a unique ID
		while (attempts < maxAttempts)
		{
			// Generate random ID: recruit_playerid_timestamp_randomhex
			int timestamp = System.GetUnixTime();
			string randomHex = GenerateRandomHex(6); // 6 character hex string
			randomId = string.Format("recruit_%1_%2_%3", ownerPersistentId, timestamp, randomHex);
			attempts++;
			
			// Break if we found a unique ID
			if (!m_mRecruits.Contains(randomId))
				break;
		}
		
		if (attempts >= maxAttempts)
		{
			Print("[Overthrow] Warning: Failed to generate unique recruit ID after " + maxAttempts + " attempts");
		}
		
		return randomId;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Generate random hexadecimal string of specified length
	protected string GenerateRandomHex(int length)
	{
		string hex = "0123456789abcdef";
		string result = "";
		
		for (int i = 0; i < length; i++)
		{
			int randomIndex = Math.RandomInt(0, hex.Length());
			result += hex.Get(randomIndex);
		}
		
		return result;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Spawn a recruit entity at specified position with civilian loadout
	SCR_ChimeraCharacter SpawnRecruit(vector position, vector orientation = "0 0 0")
	{
		if (m_sRecruitPrefab.IsEmpty())
		{
			Print("[Overthrow] Error: No recruit prefab configured!");
			return null;
		}
		
		// Spawn the recruit character directly (no group)
		SCR_ChimeraCharacter recruitEntity = OVT_Global.SpawnCharacterEntity(m_sRecruitPrefab, position, orientation);
		if (!recruitEntity)
			return null;
				
		OVT_Global.ApplyCivilianLoadout(recruitEntity);
		
		// Activate AI for the spawned recruit
		AIControlComponent aiControl = AIControlComponent.Cast(recruitEntity.FindComponent(AIControlComponent));
		if (aiControl)
		{
			aiControl.ActivateAI();
		}
		else
		{
			Print("[Overthrow] WARNING: No AIControlComponent found on spawned recruit");
		}
		
		// The body IS tracked by the persistence system - the recruit prefab inherits Character_Base.et,
		// which carries the native Persistence component - so it is saved with its inventory like any
		// other character. AttachRecruitBody() writes its id onto the recruit record, which is how the
		// same body (and the same gear) is asked for again later.

		// Note: OVT_PlayerOwnerComponent should be set by the caller (typically RecruitCivilian)
		// This method just spawns the entity, ownership setup happens elsewhere
		
		return recruitEntity;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Server-side method to recruit a civilian. Returns false when no recruit record was created,
	//! so callers can abort their transaction (refunds, orphan cleanup).
	bool RecruitCivilian(SCR_ChimeraCharacter civilian, int playerId)
	{
		if (!civilian) return false;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (!players) return false;

		string persId = players.GetPersistentIDFromPlayerID(playerId);
		if (persId.IsEmpty()) return false;

		// Double-check recruit limit on server
		if (!CanRecruit(persId)) return false;
		
		// Enable wanted system for the recruited civilian
		OVT_PlayerWantedComponent wantedComp = OVT_PlayerWantedComponent.Cast(civilian.FindComponent(OVT_PlayerWantedComponent));
		if (wantedComp)
		{
			wantedComp.EnableWantedSystem();
		}
		
		// Set the player owner component
		OVT_PlayerOwnerComponent ownerComp = OVT_PlayerOwnerComponent.Cast(civilian.FindComponent(OVT_PlayerOwnerComponent));
		if (ownerComp)
		{
			ownerComp.SetPlayerOwner(persId);
		}
		
		// Add to recruit manager
		string recruitId = AddRecruit(persId, civilian);
		if (recruitId.IsEmpty()) return false;

		// Set recruit faction to match player faction
		SetRecruitFaction(persId, civilian);
		
		// Note: BroadcastRecruitCreated is already called in AddRecruit method
		// No need to broadcast again here to avoid duplicates
		
		// Add to the player's group through the slave-group path (RequestAddAIAgent) - the same
		// route the respawn flow uses. Slave-group membership is commanded by player id, so it
		// survives the owner dying; forcing the agent into the MASTER group's array only worked
		// while the player's own agent was still in that group, which stops being true after
		// their first death (Overthrow's handover never travels the vanilla spawn pipeline that
		// would re-register the new body's agent).
		AddRecruitToPlayerGroup(persId, civilian);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether this player may recruit at a tent standing where they are standing.
	//!
	//! THE WHOLE VALIDATION FOR A TENT RECRUIT, MINUS MONEY. Every caller has its own price - the
	//! plain action charges half the base recruit cost, the equipped purchase charges that plus a
	//! gear fee - so funds are deliberately NOT checked here: a shared check would have to be told
	//! the amount, and a caller that forgot to tell it would silently pass. Supporters and money are
	//! taken by the caller, AFTER SpawnTentRecruit() has proven a recruit exists.
	//!
	//! Order is the shipped one (cap, then supporters, then distance) so that a refactor of the
	//! legacy handler cannot change which refusal a player sees first.
	//! \param[in] tentPos Where the tent is.
	//! \param[in] playerId Runtime id of the requesting player.
	//! \return TENT_RECRUIT_OK, or the first rule that refused.
	int ValidateTentRecruit(vector tentPos, int playerId)
	{
		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (!players) return TENT_RECRUIT_UNAVAILABLE;

		OVT_TownManagerComponent towns = OVT_Global.GetTowns();
		if (!towns) return TENT_RECRUIT_UNAVAILABLE;

		PlayerManager playerManager = GetGame().GetPlayerManager();
		if (!playerManager) return TENT_RECRUIT_UNAVAILABLE;

		string persId = players.GetPersistentIDFromPlayerID(playerId);
		if (persId.IsEmpty()) return TENT_RECRUIT_NO_IDENTITY;

		// At the cap RecruitCivilian() bails, which would orphan a freshly spawned civilian.
		if (!CanRecruit(persId)) return TENT_RECRUIT_AT_CAP;

		// TakeSupportersFromNearestTown() silently no-ops when the town has none, so asking first is
		// the only way the transaction can refuse instead of quietly skipping its cost.
		if (!towns.NearestTownHasSupporters(tentPos)) return TENT_RECRUIT_NO_SUPPORTERS;

		IEntity playerEntity = playerManager.GetPlayerControlledEntity(playerId);
		if (!playerEntity) return TENT_RECRUIT_NO_IDENTITY;

		if (vector.Distance(playerEntity.GetOrigin(), tentPos) > TENT_MAX_DISTANCE) return TENT_RECRUIT_TOO_FAR;

		return TENT_RECRUIT_OK;
	}

	//------------------------------------------------------------------------------------------------
	//! Spawn a recruit at a recruitment tent and give it to a player. THE ONLY tent spawn.
	//!
	//! ! NO MONEY AND NO SUPPORTERS ARE TAKEN HERE. The caller owns its own transaction and must take
	//! them AFTER this returns non-null - the shipped ordering, and the only one that cannot charge a
	//! player for a recruit that failed to appear (OVT_PlayerCommsComponent.c:1528).
	//!
	//! ! ON FAILURE TO OWN THE RECRUIT, THE BODY IS DELETED. RecruitCivilian() can still refuse after
	//! the character exists (the cap is re-checked inside it), and an unowned civilian left standing
	//! at the tent is a bug players find before we do.
	//!
	//! THE TENT ENTITY IS OPTIONAL, and a null one is recovered here. The equipped purchase names
	//! the tent by RplId and validates it, so it always has the entity. The legacy action only ever
	//! sent a bare position, and giving it an entity would mean changing its RPC signature - which
	//! would make its refactor a redesign. It passes null, and this method looks the tent up from
	//! the position instead (FindTentAtPosition), so BOTH callers get the full placement treatment:
	//! the tent's own OVT_SpawnPointComponent when it has one, facing included. Only when no tent
	//! entity can be found at all does the shipped fixed offset remain the anchor.
	//! \param[in] tent The tent's ROOT entity, or null when the caller only knows a position.
	//! \param[in] tentPos Where the tent is. Used to recover the tent when tent is null.
	//! \param[in] playerId Runtime id of the recruiting player.
	//! \return The recruit's body, or null when nothing was spawned or nothing was owned.
	SCR_ChimeraCharacter SpawnTentRecruit(IEntity tent, vector tentPos, int playerId)
	{
		if (!tent)
			tent = FindTentAtPosition(tentPos);

		vector spawnPos = ResolveTentSpawnPosition(tent, tentPos);
		vector spawnAngles = ResolveTentSpawnAngles(tent);

		SCR_ChimeraCharacter recruit = SpawnRecruit(spawnPos, spawnAngles);
		if (!recruit) return null;

		if (!RecruitCivilian(recruit, playerId))
		{
			// Never leave an unowned civilian standing at the tent
			SCR_EntityHelper.DeleteEntityAndChildren(recruit);
			return null;
		}

		return recruit;
	}

	//------------------------------------------------------------------------------------------------
	//! Where a tent recruit is put down.
	//!
	//! Four steps, and the fourth one has a trap in it:
	//!
	//!  1. ANCHOR. The tent's own OVT_SpawnPointComponent when it carries one - the authored point,
	//!     placed in Workbench where the ground in front of the tent is actually clear. Without the
	//!     component, the tent's forward axis from its world transform, so the recruit appears in
	//!     front of the tent however the tent was rotated when it was built - the shipped fixed
	//!     world-space "2 0 2" put it in a different place relative to the tent for every
	//!     orientation. Without even a tent entity, that shipped offset, unchanged.
	//!  2. SCATTER. A uniform random point in a TENT_SPAWN_SCATTER_RADIUS disc around the anchor, so
	//!     consecutive recruits spread out instead of stacking on one point.
	//!  3. GROUND CLAMP. The anchor inherits the tent's Y (or the spawn point's, which is for a
	//!     different X/Z than the scattered one), which on a slope is not the ground three metres
	//!     away. FindSafeSpawnPosition() reads its own GetSurfaceY() and then never uses it - it
	//!     searches within +0..2 m of the Y IT WAS GIVEN - so clamping before the call is the only
	//!     thing that puts the search at ground level.
	//!  4. ! skipSpawnPointSearch MUST STAY TRUE. With the search enabled the function returns the
	//!     closest OVT_SpawnPointComponent within 15 m and returns BEFORE the TraceBox is built,
	//!     ignoring the box entirely (OVT_Global.c:406-440). Tents are built at bases and FOBs, and
	//!     both OVT_BaseController.et and OverthrowMobileFOBDeployed.et carry that component - so a
	//!     tent built near one could drop every recruit on the respawn marker instead of at the
	//!     tent. The tent's OWN spawn point is read directly in step 1, never through that search.
	//! \param[in] tent The tent's root entity, or null.
	//! \param[in] fallbackPos Anchor used when there is no tent entity.
	//! \return A world position with a collision-checked box at it.
	vector ResolveTentSpawnPosition(IEntity tent, vector fallbackPos)
	{
		vector anchor = fallbackPos + TENT_SPAWN_FALLBACK_OFFSET;

		if (tent)
		{
			OVT_SpawnPointComponent spawnPoint = OVT_SpawnPointComponent.Cast(tent.FindComponent(OVT_SpawnPointComponent));
			vector forward = tent.GetWorldTransformAxis(2);
			forward[1] = 0;

			if (spawnPoint)
				anchor = spawnPoint.GetSpawnPoint();
			else if (forward.Length() > 0.01)
				anchor = tent.GetOrigin() + (forward.Normalized() * TENT_SPAWN_FORWARD_OFFSET);
			else
				anchor = tent.GetOrigin() + TENT_SPAWN_FALLBACK_OFFSET;
		}

		float scatterAngle = s_AIRandomGenerator.RandFloatXY(0, Math.PI2);
		float scatterDistance = TENT_SPAWN_SCATTER_RADIUS * Math.Sqrt(s_AIRandomGenerator.RandFloat01());
		anchor[0] = anchor[0] + Math.Cos(scatterAngle) * scatterDistance;
		anchor[2] = anchor[2] + Math.Sin(scatterAngle) * scatterDistance;

		BaseWorld world = GetGame().GetWorld();
		if (world)
			anchor[1] = world.GetSurfaceY(anchor[0], anchor[2]);

		return OVT_Global.FindSafeSpawnPosition(anchor, "-0.5 0 -0.5", "0.5 2 0.5", true);
	}

	//------------------------------------------------------------------------------------------------
	//! The tent ROOT entity at a bare position, or null.
	//!
	//! The legacy tent action's RPC only ever carried a position (changing that would make its
	//! refactor a redesign - see RpcAsk_RecruitFromTent), so the entity is recovered server-side
	//! instead: the first RecruitmentTent buildable within TENT_LOOKUP_RADIUS. Matching on
	//! OVT_BuildableComponent's type - NOT on OVT_SpawnPointComponent, which base controllers and
	//! deployed FOBs also carry - is what keeps a tent built at a base from resolving to the base.
	//! \param[in] pos Where the caller says the tent is.
	//! \return The tent root, or null when nothing matched.
	protected IEntity FindTentAtPosition(vector pos)
	{
		m_TentSearched = null;
		GetGame().GetWorld().QueryEntitiesBySphere(pos, TENT_LOOKUP_RADIUS, null, FilterTentEntity, EQueryEntitiesFlags.ALL);
		return m_TentSearched;
	}

	//! Query filter for FindTentAtPosition(). Stores the match on the member and always returns
	//! false, exactly like OVT_TownManagerComponent.FindTownMarker() - the one proven shape for a
	//! find-one query in this codebase.
	protected bool FilterTentEntity(IEntity entity)
	{
		OVT_BuildableComponent buildable = OVT_BuildableComponent.Cast(entity.FindComponent(OVT_BuildableComponent));
		if (buildable && buildable.GetBuildableType() == "RecruitmentTent")
			m_TentSearched = entity;

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Which way a tent recruit faces.
	//!
	//! The tent's own yaw, so the recruit is looking the same way the tent is - out of it, given the
	//! spawn point is on the tent's forward axis. Pitch and roll are dropped: a character standing on
	//! sloped ground is upright, and a tent placed on a slope is not.
	//! \param[in] tent The tent's root entity, or null.
	//! \return Yaw/pitch/roll angles for the spawn, or zero when there is no tent entity.
	vector ResolveTentSpawnAngles(IEntity tent)
	{
		if (!tent) return "0 0 0";

		vector angles = tent.GetYawPitchRoll();

		return Vector(angles[0], 0, 0);
	}

	//------------------------------------------------------------------------------------------------
	//! Generate random recruit name
	protected string GenerateRecruitName()
	{
		// TODO: Implement proper name generation
		// For now, use generic names
		array<string> firstNames = {
			"Alex", "Jordan", "Morgan", "Casey", "Riley",
			"Taylor", "Jamie", "Cameron", "Drew", "Blake"
		};
		
		array<string> lastNames = {
			"Smith", "Johnson", "Williams", "Brown", "Jones",
			"Garcia", "Miller", "Davis", "Rodriguez", "Martinez"
		};
		
		int firstIndex = Math.RandomInt(0, firstNames.Count());
		int lastIndex = Math.RandomInt(0, lastNames.Count());
		
		return firstNames[firstIndex] + " " + lastNames[lastIndex];
	}
	
	//------------------------------------------------------------------------------------------------
	//! Restore character identity name onto a body that was just spawned from a record
	protected void RestoreCharacterIdentity(IEntity characterEntity, string fullName)
	{
		SCR_CharacterIdentityComponent identity = SCR_CharacterIdentityComponent.Cast(characterEntity.FindComponent(SCR_CharacterIdentityComponent));
		if (!identity)
		{
			Print("[Overthrow] WARNING: No identity component found for recruit restore");
			return;
		}
		
		// Parse the stored name back into parts
		string firstName, alias, surname;
		ParseFullName(fullName, firstName, alias, surname);
		
		// Set the identity parts
		Identity characterIdentity = identity.GetIdentity();
		if (characterIdentity)
		{
			characterIdentity.SetName(firstName);
			characterIdentity.SetAlias(alias);
			characterIdentity.SetSurname(surname);
			Print("[Overthrow] Restored identity for recruit: " + fullName);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Parse full name back into components
	protected void ParseFullName(string fullName, out string firstName, out string alias, out string surname)
	{
		firstName = "";
		alias = "";
		surname = "";
		
		// Handle format: John "Alias" Doe or John Doe
		if (fullName.Contains("\""))
		{
			// Format with alias: John "Alias" Doe
			array<string> parts = {};
			fullName.Split(" ", parts, true);
			
			if (parts.Count() >= 3)
			{
				firstName = parts[0];
				
				// Find alias between quotes
				for (int i = 1; i < parts.Count(); i++)
				{
					if (parts[i].StartsWith("\"") && parts[i].EndsWith("\""))
					{
						alias = parts[i].Substring(1, parts[i].Length() - 2); // Remove quotes
						
						// Everything after alias is surname
						for (int j = i + 1; j < parts.Count(); j++)
						{
							if (!surname.IsEmpty()) surname += " ";
							surname += parts[j];
						}
						break;
					}
				}
			}
		}
		else
		{
			// Simple format: John Doe
			array<string> parts = {};
			fullName.Split(" ", parts, true);
			
			if (parts.Count() >= 2)
			{
				firstName = parts[0];
				for (int i = 1; i < parts.Count(); i++)
				{
					if (!surname.IsEmpty()) surname += " ";
					surname += parts[i];
				}
			}
			else if (parts.Count() == 1)
			{
				firstName = parts[0];
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Handle player connection - respawn their recruits
	protected void OnPlayerConnected(string playerPersistentId, int playerId)
	{		
		// Cancel offline timer
		if (m_mOfflinePlayerTimers.Contains(playerPersistentId))
		{
			m_mOfflinePlayerTimers.Remove(playerPersistentId);
		}
		
		// Recruit respawning will be triggered when player group is created
	}
	
	//------------------------------------------------------------------------------------------------
	//! Handle player disconnection - start offline timer
	protected void OnPlayerDisconnected(string playerPersistentId, int playerId)
	{		
		// Start offline timer
		m_mOfflinePlayerTimers[playerPersistentId] = OFFLINE_DESPAWN_TIME;

		// Remember where the bodies are, so they come back there
		SyncRecruitPositions();
	}
	
	//------------------------------------------------------------------------------------------------
	//! Brings a returning player's recruits back into the world.
	//!
	//! WHEN A RECRUIT BODY IS IN PLAY IS AN OWNER-PRESENCE QUESTION, NOT A CAMPAIGN-START ONE. A
	//! recruit's body is reserved - alive, tracked, hidden in place -
	//! OFFLINE_DESPAWN_TIME after its owner leaves (DespawnPlayerRecruits)
	//! and comes back when that owner returns and has a group - this is called from
	//! RespawnRecruitsDelayed, off the group-created event. That is exactly the lifecycle the previous
	//! EPF implementation had, and it is what a returning player expects: their squad is where they left
	//! it, carrying what they were carrying, and nobody else's recruits are standing around in an empty
	//! world.
	//!
	//! It is therefore also the whole of the "recruits survive a save" story. Records are restored by
	//! OVT_RecruitManagerSerializer while the world loads; the BODIES are asked back from the
	//! persistence system here, by the id the record carries, the moment their owner is in the game.
	//! Nothing self-spawns a recruit (AI characters are SelfSpawn 0 - see Overthrow.conf), which is
	//! exactly why the manager has to ask.
	//!
	//! THIS ALSO COVERS RECRUITS WHOSE BODY WAS ALIVE WHEN THE SAVE WAS TAKEN. Such a body was written
	//! into the save point like any tracked character, but SelfSpawn 0 means nobody brings it back on
	//! load - so after a quit/continue the record has an id and no body in the world, which is the same
	//! state as a despawned one and takes the same path.
	//!
	//! IDEMPOTENT: a recruit that already has a body in the world, or a request in flight for one, is
	//! never given a second one.
	//! \param[in] playerPersistentId The returning owner.
	protected void RespawnPlayerRecruits(string playerPersistentId)
	{
		if (!m_mRecruitsByOwner.Contains(playerPersistentId))
			return;

		array<string> recruitIds = m_mRecruitsByOwner[playerPersistentId];

		foreach (string recruitId : recruitIds)
		{
			OVT_RecruitData recruit = m_mRecruits[recruitId];
			if (!recruit)
				continue;

			// A body already in the world is either still in play (quick reconnect, the offline
			// timer never fired) or reserved - hidden in place since the owner left. Either way it
			// is the same character carrying the same inventory, so wake it and put it back where
			// the record says it belongs; no storage round trip is involved.
			IEntity existingEntity = FindRecruitEntity(recruitId);
			if (existingEntity)
			{
				Print("[Overthrow] Recruit " + recruitId + " already in world, placing it");
				UnreserveRecruitBody(recruit, existingEntity);
				PlaceRecruitInWorld(playerPersistentId, recruit, existingEntity);
				BroadcastRecruitUpdate(recruit);
				continue;
			}

			// A body is already on its way for this recruit - the spawn request has not answered yet
			if (m_aPendingBodySpawns && m_aPendingBodySpawns.Find(recruitId) != -1)
			{
				Print("[Overthrow] Recruit " + recruitId + " already has a body spawn in flight");
				continue;
			}

			SpawnRecruitBody(playerPersistentId, recruit);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Gives one recruit a body, preferring the one it actually had.
	//!
	//! TWO ROUTES, IN ORDER OF FIDELITY:
	//!   1. the record names a stored body -> ask the persistence system for THAT character
	//!      (RequestPersistedRecruitBody). It comes back with the gear, wounds and stance it was
	//!      released with, because vanilla serializes all of that for any tracked character;
	//!   2. otherwise, or if the request fails -> spawn the recruit prefab and dress it as a civilian
	//!      (SpawnFreshRecruitBody). A recruit is never lost to a missing or unreadable stored body.
	//!
	//! Route 1 is ASYNCHRONOUS: this returns having only started the request, and
	//! OnRecruitBodySpawned() finishes the job - including falling through to route 2.
	//! \param[in] playerPersistentId The owning player.
	//! \param[in] recruit The record to give a body to.
	protected void SpawnRecruitBody(string playerPersistentId, notnull OVT_RecruitData recruit)
	{
		if (RequestPersistedRecruitBody(playerPersistentId, recruit))
			return;

		SpawnFreshRecruitBody(playerPersistentId, recruit);
	}

	//------------------------------------------------------------------------------------------------
	//! Asks the persistence system to spawn back the exact character this recruit was last using.
	//!
	//! Vanilla's own model is SCR_SpawnLogic.c:368-374, which fetches a returning player's character
	//! from the same collection the same way. The request is filtered to a single id, so the callback
	//! is invoked once.
	//! \param[in] playerPersistentId The owning player.
	//! \param[in] recruit The record naming the stored body.
	//! \return True when a request was sent and the callback now owns the outcome; false when there is
	//! nothing to ask for, so the caller must spawn a fresh body.
	protected bool RequestPersistedRecruitBody(string playerPersistentId, notnull OVT_RecruitData recruit)
	{
		if (recruit.m_sBodyPersistenceId == "")
			return false;

		SCR_PersistenceSystem persistence = SCR_PersistenceSystem.GetScriptedInstance();
		if (!persistence)
			return false;

		// Asking a system that is still loading would answer UNAVAILABLE; vanilla guards the same way
		// (SCR_SpawnLogic.c:302-307).
		if (persistence.GetState() != EPersistenceSystemState.ACTIVE)
			return false;

		PersistenceCollection collection = GetRecruitBodyCollection(persistence);
		if (!collection)
			return false;

		// Vanilla's own validation of a stored id before using it (SCR_VoiceoverSystemSerializer.c:90).
		// A null UUID still stringifies to a zero-filled value, so never compare, always ask.
		if (!UUID.IsUUID(recruit.m_sBodyPersistenceId))
		{
			// Whatever is stored is not a UUID - it can never resolve, so stop carrying it around
			recruit.m_sBodyPersistenceId = "";
			return false;
		}

		UUID bodyId = recruit.m_sBodyPersistenceId;
		string recruitId = recruit.m_sRecruitId;

		if (!m_aPendingBodySpawns)
			m_aPendingBodySpawns = new array<string>;

		// MUST be marked pending BEFORE the request is sent: an instance the system already has in memory
		// completes the callback IMMEDIATELY, i.e. from inside RequestSpawn(), and the callback's first
		// act is to consume this entry.
		if (m_aPendingBodySpawns.Find(recruitId) == -1)
			m_aPendingBodySpawns.Insert(recruitId);

		Print("[Overthrow] Requesting stored body " + recruit.m_sBodyPersistenceId + " for recruit " + recruitId);

		PersistenceSpawnRequest request();
		request.Collection = collection;
		request.Include = {bodyId};

		Tuple2<string, string> spawnContext(recruitId, playerPersistentId);
		PersistenceResultCallback callback(OnRecruitBodySpawned, spawnContext);
		persistence.RequestSpawn(request, callback);

		// Insurance only, and a no-op once the callback has answered. The engine documents the callback
		// as always firing, but a recruit that never heard back would otherwise be left without a body
		// for the rest of the session.
		GetGame().GetCallqueue().CallLater(OnRecruitBodySpawnTimeout, BODY_SPAWN_TIMEOUT_MS, false, recruitId, playerPersistentId);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! PersistenceResultCallback delegate for RequestPersistedRecruitBody().
	//!
	//! Arity must match PersistenceResultDelegate exactly (the model is SCR_SpawnLogic.c:378).
	//! \param[in] statusCode OK when the stored body was found and instantiated.
	//! \param[in] result The spawned instance on OK; on failure it is the id that could not be fetched.
	//! \param[in] isLast True on the final result of the request (always, for a single-id request).
	//! \param[in] context Tuple2 of recruit id and owner persistent id.
	protected void OnRecruitBodySpawned(EPersistenceStatusCode statusCode, Managed result, bool isLast, Managed context)
	{
		Tuple2<string, string> spawnContext = Tuple2<string, string>.Cast(context);
		if (!spawnContext)
			return;

		string recruitId = spawnContext.param1;
		string playerPersistentId = spawnContext.param2;

		// Only the first answer for a recruit is acted on. Anything after it - a duplicate result, or a
		// late answer to a request the timeout already gave up on - must not build a second body.
		if (!m_aPendingBodySpawns)
			return;

		int pendingIndex = m_aPendingBodySpawns.Find(recruitId);
		if (pendingIndex == -1)
			return;

		m_aPendingBodySpawns.Remove(pendingIndex);

		IEntity bodyEntity;
		if (statusCode == EPersistenceStatusCode.OK)
			bodyEntity = IEntity.Cast(result);

		OVT_RecruitData recruit = GetRecruit(recruitId);
		if (!recruit)
		{
			// The recruit was dismissed or died while its body was being fetched. Nobody owns this
			// character now, so delete it AND drop its stored data - otherwise it would be spawnable
			// again forever.
			if (bodyEntity)
			{
				Print("[Overthrow] Stored body arrived for a recruit that no longer exists (" + recruitId + ") - discarding it");
				OVT_PersistenceTracking.Untrack(bodyEntity, false);
				SCR_EntityHelper.DeleteEntityAndChildren(bodyEntity);
			}
			return;
		}

		if (!bodyEntity)
		{
			// Stored body could not be brought back: wiped save data, a prefab that no longer exists, an
			// unreadable record. A recruit is never lost to this - clear the dead id and build a fresh
			// body, which is the pre-existing behaviour (civilian loadout).
			Print(string.Format("[Overthrow] Persistence answered %1 for recruit %2's stored body - spawning a fresh one",
				typename.EnumToString(EPersistenceStatusCode, statusCode), recruitId), LogLevel.WARNING);

			recruit.m_sBodyPersistenceId = "";
			SpawnFreshRecruitBody(playerPersistentId, recruit);
			return;
		}

		// The owner may have left again while the request was in flight. Putting a body in play for
		// somebody who is not there is precisely what the offline despawn undoes, so undo it now:
		// the body is reserved in place - alive, tracked, hidden - for the next time they return.
		if (!IsPlayerOnline(playerPersistentId))
		{
			Print("[Overthrow] Owner of recruit " + recruitId + " left before their body arrived - reserving it in place");
			ReserveRecruitBody(recruit, bodyEntity);
			BroadcastRecruitUpdate(recruit);
			return;
		}

		AttachRecruitBody(playerPersistentId, recruit, bodyEntity);

		Print("[Overthrow] Recruit " + recruitId + " restored from its stored body, gear intact");
	}

	//------------------------------------------------------------------------------------------------
	//! Gives up on a spawn request that never answered and falls back to a fresh body.
	//!
	//! No-op in the normal case: the callback has already cleared the pending entry by the time this
	//! runs.
	//! \param[in] recruitId The recruit whose request was sent.
	//! \param[in] playerPersistentId The owner the request was made for.
	protected void OnRecruitBodySpawnTimeout(string recruitId, string playerPersistentId)
	{
		if (!m_aPendingBodySpawns)
			return;

		int pendingIndex = m_aPendingBodySpawns.Find(recruitId);
		if (pendingIndex == -1)
			return;

		m_aPendingBodySpawns.Remove(pendingIndex);

		OVT_RecruitData recruit = GetRecruit(recruitId);
		if (!recruit)
			return;

		// Something may have given the recruit a body by other means in the meantime
		if (FindRecruitEntity(recruitId))
			return;

		if (!IsPlayerOnline(playerPersistentId))
			return;

		Print("[Overthrow] No answer to recruit " + recruitId + "'s body spawn request - spawning a fresh one", LogLevel.WARNING);

		recruit.m_sBodyPersistenceId = "";
		SpawnFreshRecruitBody(playerPersistentId, recruit);
	}

	//------------------------------------------------------------------------------------------------
	//! Builds a brand new body for a recruit from the recruit prefab, at its last known position.
	//!
	//! THE FALLBACK, NOT THE NORMAL PATH. This is what a recruit gets when no stored body can be found
	//! for it, and it is where the old "gear resets to civilian" behaviour lives: the record (name, XP,
	//! level, kills, skills, training) survives, the inventory does not, because there is nothing to
	//! read it from.
	//! \param[in] playerPersistentId The owning player.
	//! \param[in] recruit The record to build a body for.
	//! \return The spawned character, or null when it could not be placed.
	protected IEntity SpawnFreshRecruitBody(string playerPersistentId, notnull OVT_RecruitData recruit)
	{
		vector position = recruit.m_vLastKnownPosition;
		if (position == vector.Zero)
			position = FindRecruitFallbackPosition(playerPersistentId);

		if (position == vector.Zero)
		{
			Print("[Overthrow] Cannot respawn recruit " + recruit.m_sRecruitId + ": no known position and owner not in world", LogLevel.WARNING);
			return null;
		}

		SCR_ChimeraCharacter recruitEntity = SpawnRecruit(position);
		if (!recruitEntity)
		{
			Print("[Overthrow] Failed to respawn recruit body: " + recruit.m_sRecruitId, LogLevel.WARNING);
			return null;
		}

		// This is a DIFFERENT character to whatever the record used to point at, so the old id must not
		// survive the swap - AttachRecruitBody() puts the new body's id in its place.
		recruit.m_sBodyPersistenceId = "";

		AttachRecruitBody(playerPersistentId, recruit, recruitEntity);

		Print("[Overthrow] Recruit " + recruit.m_sRecruitId + " respawned and added to group");

		return recruitEntity;
	}

	//------------------------------------------------------------------------------------------------
	//! The persistence collection recruit bodies are stored in, resolved once and kept.
	//!
	//! Cached the way vanilla caches it (SCR_SpawnLogic.SetupPersistenceCollections, :51-55) - the
	//! lookup is by name against the loaded configuration and does not change during a session.
	//! \param[in] persistence The live persistence system.
	//! \return The collection, or null when the loaded configuration does not contain it.
	protected PersistenceCollection GetRecruitBodyCollection(notnull SCR_PersistenceSystem persistence)
	{
		if (!m_RecruitBodyCollection)
		{
			m_RecruitBodyCollection = persistence.FindCollection(RECRUIT_BODY_COLLECTION);

			if (!m_RecruitBodyCollection)
				Print("[Overthrow] No '" + RECRUIT_BODY_COLLECTION + "' persistence collection - recruit bodies cannot be spawned back with their gear", LogLevel.WARNING);
		}

		return m_RecruitBodyCollection;
	}

	//------------------------------------------------------------------------------------------------
	//! Whether a player is currently connected, by their persistent identity id.
	//!
	//! Asks the engine's player list rather than OVT_PlayerManagerComponent's id map, because that map
	//! keeps a disconnected player's last runtime id and would report them as present.
	//! \param[in] playerPersistentId The player to look for.
	//! \return True when that player is connected right now.
	protected bool IsPlayerOnline(string playerPersistentId)
	{
		if (playerPersistentId.IsEmpty())
			return false;

		array<int> connectedPlayers = {};
		GetGame().GetPlayerManager().GetPlayers(connectedPlayers);

		foreach (int playerId : connectedPlayers)
		{
			string uid = OVT_Global.GetPlayerUID(playerId);
			if (!uid.IsEmpty() && uid == playerPersistentId)
				return true;
		}

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! Notes the persistence id of a recruit's body on its record.
	//!
	//! The id is what RequestPersistedRecruitBody() asks for later, so this is the single point where
	//! "which character IS this recruit" is written down. An id that cannot be read is never allowed to
	//! overwrite one that could: an empty answer leaves the record untouched.
	//! \param[in] recruit The record to write to.
	//! \param[in] recruitEntity The body.
	//! \param[in] materialise True to write the body's record first when it has no id yet. Only pass
	//! true where writing a record is wanted anyway - it is a real storage write, not a lookup.
	protected void CaptureRecruitBodyId(notnull OVT_RecruitData recruit, notnull IEntity recruitEntity, bool materialise)
	{
		string bodyId = OVT_PersistenceTracking.GetPersistentId(recruitEntity);

		if (bodyId.IsEmpty() && materialise)
		{
			// Registration is lazy, so an instance the system has never written may not have an identity
			// to hand out yet. Writing its record is what gives it one.
			SCR_ChimeraCharacter character = SCR_ChimeraCharacter.Cast(recruitEntity);
			if (character)
			{
				OVT_PersistenceTracking.Save(recruitEntity);
				bodyId = OVT_PersistenceTracking.GetPersistentId(recruitEntity);
			}
		}

		if (bodyId.IsEmpty())
			return;

		recruit.m_sBodyPersistenceId = bodyId;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes a recruit body out of play WITHOUT destroying it - alive, TRACKED, hidden in place.
	//!
	//! THE REPLACEMENT FOR SAVE-AND-RELEASE (BUG-130). The old path here wrote the character to
	//! storage, released tracking keeping the record, and deleted the entity - vanilla's own idiom,
	//! and the one BUG-086 measured as NOT durable: the kept record dies within minutes of its
	//! entity, no restart required, so a returning owner's recruits answered NOT_FOUND and came back
	//! as fresh prefabs in civilian clothes. Nothing may depend on a record outliving its entity, so
	//! the entity now outlives the absence instead (OVT_PersistenceReservation - the same remedy
	//! player bodies and locked vehicles got). In-session fidelity is by construction (nothing is
	//! serialized, nothing is rebuilt), and at the next save point the body serializes as the
	//! ordinary live tracked character it is - which is the across-a-restart path that was always
	//! green.
	//!
	//! The AI agent is deactivated BEFORE the flags are cleared, so a walking recruit does not keep
	//! feeding movement into an entity that no longer simulates. The entity mapping is KEPT - and
	//! written here, because the owner-left-mid-flight path hands in a storage-spawned body that
	//! AttachRecruitBody() has never seen - since the mapping is what lets RespawnPlayerRecruits()
	//! find and wake the body later, what keeps SyncRecruitPositions() refreshing its id before
	//! every save, and what keeps the BUG-118 OnAgentAdded exclusion protecting it.
	//! \param[in] recruit The record whose body is being parked.
	//! \param[in] recruitEntity The body to reserve.
	protected void ReserveRecruitBody(notnull OVT_RecruitData recruit, notnull IEntity recruitEntity)
	{
		m_mEntityToRecruit[recruitEntity.GetID()] = recruit.m_sRecruitId;

		recruit.m_vLastKnownPosition = recruitEntity.GetOrigin();

		// materialise: the id is what the post-restart respawn asks for, and a body that has never
		// been written may not have been given one yet. The write is wanted here.
		CaptureRecruitBodyId(recruit, recruitEntity, true);

		AIControlComponent aiControl = AIControlComponent.Cast(recruitEntity.FindComponent(AIControlComponent));
		if (aiControl)
			aiControl.DeactivateAI();

		OVT_PersistenceReservation.Reserve(recruitEntity);

		recruit.m_bIsOnline = false;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts a reserved recruit body back in play, exactly where it was left.
	//!
	//! Safe on a body that was never reserved (the quick-reconnect case, where the offline timer has
	//! not fired yet): Release() is idempotent and ActivateAI() on an active agent is a no-op.
	//! \param[in] recruit The record whose body is coming back.
	//! \param[in] recruitEntity The body to wake.
	protected void UnreserveRecruitBody(notnull OVT_RecruitData recruit, notnull IEntity recruitEntity)
	{
		OVT_PersistenceReservation.Release(recruitEntity);

		AIControlComponent aiControl = AIControlComponent.Cast(recruitEntity.FindComponent(AIControlComponent));
		if (aiControl)
			aiControl.ActivateAI();

		recruit.m_bIsOnline = true;
	}

	//------------------------------------------------------------------------------------------------
	//! Where to put a recruit whose record has no position (only possible for a record written before
	//! the body was ever placed): next to its owner.
	//! \param[in] playerPersistentId The owning player.
	//! \return The owner's position, or vector.Zero when the owner has no character in the world.
	protected vector FindRecruitFallbackPosition(string playerPersistentId)
	{
		OVT_PlayerManagerComponent playerManager = OVT_Global.GetPlayers();
		if (!playerManager)
			return vector.Zero;

		int playerId = playerManager.GetPlayerIDFromPersistentID(playerPersistentId);
		if (playerId < 1)
			return vector.Zero;

		IEntity playerEntity = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!playerEntity)
			return vector.Zero;

		return playerEntity.GetOrigin();
	}

	//------------------------------------------------------------------------------------------------
	//! Links a body to its recruit record and puts it where the record says it belongs.
	//!
	//! Used by BOTH routes - a body restored from storage and a fresh one off the prefab - so everything
	//! here has to be safe to re-apply to a character that already has the right values.
	//!
	//! WHERE THE BODY ENDS UP IS PlaceRecruitInWorld()'s decision, not this method's: an ACTIVE recruit
	//! goes into its owner's squad, an INACTIVE one into a parked group holding position. This is one of
	//! the only two call sites of that fork.
	//! \param[in] playerPersistentId The owning player.
	//! \param[in] recruit The record this body belongs to.
	//! \param[in] recruitEntity The body.
	protected void AttachRecruitBody(string playerPersistentId, notnull OVT_RecruitData recruit, notnull IEntity recruitEntity)
	{
		string recruitId = recruit.m_sRecruitId;

		// Entity mapping (server) and replication mapping (clients)
		m_mEntityToRecruit[recruitEntity.GetID()] = recruitId;

		RplComponent rplComponent = RplComponent.Cast(recruitEntity.FindComponent(RplComponent));
		if (rplComponent)
		{
			m_mRplIdToRecruit[rplComponent.Id()] = recruitId;
		}

		// A body normally arrives already tracked - the persistence system spawned it, or the native
		// Persistence component on Character_Base.et registered the fresh prefab - so ASK before
		// registering rather than re-registering blind.
		if (!OVT_PersistenceTracking.IsTracked(recruitEntity))
			OVT_PersistenceTracking.Track(recruitEntity);

		// Whichever route produced this body, the record must point at THIS character from now on
		CaptureRecruitBodyId(recruit, recruitEntity, false);

		// SpawnRecruit() does this for a fresh prefab; a body handed back by the persistence system has
		// never been through it, and a recruit whose AI is not running cannot be commanded. ActivateAI()
		// on an already active agent is a no-op, so both routes can run it.
		AIControlComponent aiControl = AIControlComponent.Cast(recruitEntity.FindComponent(AIControlComponent));
		if (aiControl)
			aiControl.ActivateAI();

		// The spawned prefab comes with a random identity - put the recruit's name back on it
		RestoreCharacterIdentity(recruitEntity, recruit.m_sName);

		recruit.m_bIsOnline = true;
		recruit.m_vLastKnownPosition = recruitEntity.GetOrigin();

		OVT_PlayerOwnerComponent ownerComp = OVT_PlayerOwnerComponent.Cast(recruitEntity.FindComponent(OVT_PlayerOwnerComponent));
		if (ownerComp)
		{
			if (ownerComp.GetPlayerOwnerUid() != playerPersistentId)
				ownerComp.SetPlayerOwner(playerPersistentId);
		}
		else
		{
			Print("[Overthrow] WARNING: No OVT_PlayerOwnerComponent found on respawned recruit: " + recruitId, LogLevel.WARNING);
		}

		// Set recruit faction to match player faction before adding to group
		SetRecruitFaction(playerPersistentId, recruitEntity);

		// The prefab's m_eAISkillDefault is authoritative for recruits. Vanilla's
		// SCR_AICombatComponentSerializer restores the SAVED skill over it on load, so a body from
		// a save predating a prefab skill change keeps the old value forever (and re-saves it) -
		// which made skill tuning appear to do nothing on existing recruits.
		SCR_AICombatComponent combatComponent = SCR_AICombatComponent.Cast(recruitEntity.FindComponent(SCR_AICombatComponent));
		if (combatComponent)
			combatComponent.ResetAISkill();

		// Into the owner's squad, or into a parked group if the record says this recruit is inactive
		PlaceRecruitInWorld(playerPersistentId, recruit, recruitEntity);

		// Broadcast updated recruit status to all clients
		BroadcastRecruitUpdate(recruit);
	}

	//------------------------------------------------------------------------------------------------
	//! Writes every live body's position AND persistence id back onto its record.
	//!
	//! The record's last known position is where a rebuilt body is placed, and the body id is which
	//! character gets asked for - both are what the save point carries
	//! (OVT_RecruitManagerSerializer), so this is what makes a recruit come back where you left them,
	//! as who you left them.
	//!
	//! REFRESHING THE ID MATTERS FOR RECRUITS WHO ARE STILL STANDING THERE. A body that never despawns
	//! never goes through ReserveRecruitBody(), so this hook is the only place its id is written down
	//! before the save. Without it, quitting with your squad beside you would bring them back in
	//! civilian clothes.
	//!
	//! Called before every save (OVT_OverthrowGameMode.PreShutdownPersist) and when an owner
	//! disconnects. It is deliberately NOT a per-frame update: the position only has to be true at the
	//! moments a body can stop existing.
	//!
	//! Also drops entity mappings whose entity is gone, which is the only sweep those mappings get
	//! outside FindRecruitEntity().
	void SyncRecruitPositions()
	{
		if (!m_mEntityToRecruit)
			return;

		array<EntityID> staleEntities = {};

		foreach (EntityID entityId, string recruitId : m_mEntityToRecruit)
		{
			IEntity recruitEntity = GetGame().GetWorld().FindEntityByID(entityId);
			if (!recruitEntity)
			{
				staleEntities.Insert(entityId);
				continue;
			}

			OVT_RecruitData recruit = GetRecruit(recruitId);
			if (!recruit)
				continue;

			recruit.m_vLastKnownPosition = recruitEntity.GetOrigin();

			// materialise: a live body may be registered but never yet written, and an id it has not
			// been given cannot be stored. The save point is about to write this character anyway.
			CaptureRecruitBodyId(recruit, recruitEntity, true);
		}

		// Removing inside the loop above would invalidate the iteration.
		foreach (EntityID staleId : staleEntities)
		{
			m_mEntityToRecruit.Remove(staleId);
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Reads a live recruit body's fighting condition into the status mask its owner is shown.
	//!
	//! READS ONLY. Nothing here writes to the entity, the record or the world; every value is
	//! measured and handed to OVT_RecruitStatus.Derive(), which owns the packing. That split is what
	//! lets Tier A pin the meaning of the mask while this half - which needs a body, an inventory and
	//! a damage manager - stays play-test territory.
	//!
	//! WEAPONS ARE READ FROM EVERY SLOT, NOT FROM THE HANDS. A slung rifle or a holstered pistol
	//! lives in the weapon manager's slots and is invisible to an in-hands read, so a recruit walking
	//! with its rifle on its back would report UNARMED - the same trap ExtractEquippedItems() exists
	//! to avoid (BUG-044).
	//!
	//! WHAT "HAS AMMO" MEANS HERE: the recruit can either fire right now (a muzzle reports rounds,
	//! which includes a chambered one) or reload (the inventory holds a magazine that fits one of the
	//! weapons it is carrying). Both halves are needed and neither alone is honest - a full rifle
	//! with no spares is armed and dangerous, and an empty rifle with four magazines in the vest is
	//! not "out of ammo" in any sense a player would recognise. The reload half is the engine's own
	//! GetMagazineCountByWeapon(), the same query vanilla's AI uses to decide whether it can resupply
	//! (SCR_AICombatComponent.c:530), so magazine-well compatibility is the engine's answer and not a
	//! hand-rolled one.
	//! \param[in] recruitEntity The live body to measure. Null is a mask of 0.
	//! \return A mask of OVT_RecruitStatus flags.
	protected int ReadRecruitStatus(IEntity recruitEntity)
	{
		if (!recruitEntity)
			return 0;

		bool armed = false;
		bool hasAmmo = false;

		BaseWeaponManagerComponent weaponManager = BaseWeaponManagerComponent.Cast(recruitEntity.FindComponent(BaseWeaponManagerComponent));
		if (weaponManager)
		{
			// The inventory manager is resolved once for the whole weapon walk, and its absence is
			// survivable: a recruit with no inventory manager can still report loaded rounds.
			InventoryStorageManagerComponent inventory = InventoryStorageManagerComponent.Cast(recruitEntity.FindComponent(InventoryStorageManagerComponent));

			array<WeaponSlotComponent> weaponSlots = new array<WeaponSlotComponent>();
			weaponManager.GetWeaponsSlots(weaponSlots);

			foreach (WeaponSlotComponent slot : weaponSlots)
			{
				if (!slot)
					continue;

				IEntity weaponEntity = slot.GetWeaponEntity();
				if (!weaponEntity)
					continue;

				armed = true;

				// Once something can be fired there is nothing left to learn from the other slots.
				if (hasAmmo)
					continue;

				BaseWeaponComponent weapon = BaseWeaponComponent.Cast(weaponEntity.FindComponent(BaseWeaponComponent));
				if (!weapon)
					continue;

				BaseMuzzleComponent muzzle = weapon.GetCurrentMuzzle();
				if (muzzle && muzzle.GetAmmoCount() > 0)
				{
					hasAmmo = true;
					continue;
				}

				if (inventory && inventory.GetMagazineCountByWeapon(weapon) > 0)
					hasAmmo = true;
			}
		}

		// Same two reads the roster row already makes (OVT_RecruitListEntryHandler.PopulateFromEntity),
		// kept in step with it deliberately: two screens describing one recruit differently is worse
		// than either being slightly coarse.
		bool wounded = false;
		SCR_CharacterDamageManagerComponent damageManager = SCR_CharacterDamageManagerComponent.Cast(recruitEntity.FindComponent(SCR_CharacterDamageManagerComponent));
		if (damageManager)
			wounded = damageManager.GetState() != EDamageState.UNDAMAGED;

		bool unconscious = false;
		CharacterControllerComponent characterController = CharacterControllerComponent.Cast(recruitEntity.FindComponent(CharacterControllerComponent));
		if (characterController)
			unconscious = characterController.IsUnconscious();

		return OVT_RecruitStatus.Derive(armed, hasAmmo, wounded, unconscious);
	}

	//------------------------------------------------------------------------------------------------
	//! SERVER: every STATUS_SYNC_INTERVAL_MS, tell each ONLINE owner where its recruits are and what
	//! condition they are in.
	//!
	//! WHY A PUSH AND NOT A CLIENT-SIDE READ. A parked recruit is by definition somewhere its owner
	//! is not, so its entity is not streamed to that client and a client-side read would report every
	//! distant recruit as unarmed (decision D11). The server is the only machine that can see all of
	//! them, and one source keeps the map tag and the roster row from disagreeing.
	//!
	//! ! IT MUST NEVER CALL SyncRecruitPositions(). That method also materialises each body's
	//! PERSISTENCE id and writes to storage - save-point work, done before a save and on disconnect,
	//! not tick work. Doing it every ten seconds would put the persistence system on a timer for no
	//! gain. This sweep writes m_vLastKnownPosition and nothing else, which is the half that costs
	//! nothing and which the inactive-group clustering reads (OVT_RecruitInactiveGrouping rule 4).
	//!
	//! THE OWNER LIST IS SNAPSHOTTED FIRST. Sending an RPC can reach code that removes a recruit -
	//! and a removal can empty an owner's list, which drops the owner's key from m_mRecruitsByOwner -
	//! so walking that map live would be mutating it mid-iteration. GetPlayerRecruits() returns a
	//! fresh array for the same reason.
	//!
	//! Skipped, in this order and each for a different reason: an owner with no controller is offline
	//! or still connecting and has nobody to tell; an owner whose controller carries no command
	//! component predates the prefab wiring and would be a silent no-op; a recruit with no body has
	//! nothing to measure and its last known position is already the best answer anyone has.
	protected void SweepRecruitStatus()
	{
		if (!Replication.IsServer())
			return;

		if (!m_mRecruitsByOwner || m_mRecruitsByOwner.IsEmpty())
			return;

		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (!players)
			return;

		array<string> ownerIds = {};
		for (int i = 0; i < m_mRecruitsByOwner.Count(); i++)
		{
			ownerIds.Insert(m_mRecruitsByOwner.GetKey(i));
		}

		foreach (string ownerId : ownerIds)
		{
			OVT_OverthrowController controller = players.GetController(ownerId);
			if (!controller)
				continue;

			OVT_RecruitCommandComponent commands = OVT_RecruitCommandComponent.Cast(controller.FindComponent(OVT_RecruitCommandComponent));
			if (!commands)
				continue;

			array<ref OVT_RecruitData> ownedRecruits = GetPlayerRecruits(ownerId);
			foreach (OVT_RecruitData recruit : ownedRecruits)
			{
				if (!recruit || !recruit.m_bIsOnline)
					continue;

				IEntity recruitEntity = FindRecruitEntity(recruit.m_sRecruitId);
				if (!recruitEntity)
					continue;

				recruit.m_vLastKnownPosition = recruitEntity.GetOrigin();

				commands.SendRecruitStatus(recruit.m_sRecruitId, recruit.m_vLastKnownPosition, ReadRecruitStatus(recruitEntity));
			}
		}
	}

	//------------------------------------------------------------------------------------------------
	//! Process offline timers for recruit despawning
	protected void ProcessOfflineTimers()
	{
		if (m_mOfflinePlayerTimers.IsEmpty())
			return;
			
		array<string> toRemove = {};
		
		foreach (string playerPersistentId, float timer : m_mOfflinePlayerTimers)
		{
			timer -= 1.0;
			
			if (timer <= 0)
			{
				Print("[Overthrow] Offline timer expired for player: " + playerPersistentId);
				DespawnPlayerRecruits(playerPersistentId);
				toRemove.Insert(playerPersistentId);
			}
			else
			{
				m_mOfflinePlayerTimers[playerPersistentId] = timer;
			}
		}
		
		// Remove expired timers
		foreach (string playerPersistentId : toRemove)
		{
			m_mOfflinePlayerTimers.Remove(playerPersistentId);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Takes an offline player's recruit bodies out of play without losing them.
	//!
	//! DESPAWN IS RESERVE, NOT DELETE (BUG-130). Each body stays alive, tracked and hidden where it
	//! stands (ReserveRecruitBody), and the recruit remembers which character it is. When the owner
	//! comes back, RespawnPlayerRecruits() wakes that same body - or, after a restart, asks the
	//! persistence system for it by the id the record carries - and the recruit returns carrying
	//! exactly what it was carrying.
	//! Public so the persistence tier can drive the despawn half of the lifecycle directly
	//! (OVT_TEST_Persistence_RecruitDespawnReservesBody); production callers are the offline timers.
	//! \param[in] playerPersistentId The player who has been offline long enough.
	void DespawnPlayerRecruits(string playerPersistentId)
	{
		if (!m_mRecruitsByOwner.Contains(playerPersistentId))
			return;

		array<string> recruitIds = m_mRecruitsByOwner[playerPersistentId];
		Print("[Overthrow] Despawning " + recruitIds.Count() + " recruits for offline player: " + playerPersistentId);

		foreach (string recruitId : recruitIds)
		{
			OVT_RecruitData recruit = m_mRecruits[recruitId];
			if (!recruit)
				continue;

			IEntity recruitEntity = FindRecruitEntity(recruitId);
			if (!recruitEntity)
				continue;

			// Keeps the body alive, tracked and hidden in place; remembers its id and position
			ReserveRecruitBody(recruit, recruitEntity);

			// Broadcast updated recruit status to all clients
			BroadcastRecruitUpdate(recruit);

			Print("[Overthrow] Despawned recruit: " + recruitId + " (reserved body " + recruit.m_sBodyPersistenceId + ")");
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Find recruit entity by persistent ID
	//!
	//! STALE MAPPINGS ARE COLLECTED, THEN REMOVED. Removing from m_mEntityToRecruit inside the foreach
	//! over that same map invalidates the iteration - the same hazard SyncRecruitPositions() documents
	//! and avoids the same way. The pruning behaviour itself is unchanged and callers that snapshot
	//! before calling this (RemoveRecruitsFromGroup, MoveRecruitsToGroup) still need to.
	IEntity FindRecruitEntity(string recruitId)
	{
		// On server, use entity ID mapping
		if (Replication.IsServer())
		{
			IEntity found;
			array<EntityID> staleEntities = {};

			foreach (EntityID entityId, string mappedRecruitId : m_mEntityToRecruit)
			{
				if (mappedRecruitId != recruitId)
					continue;

				IEntity entity = GetGame().GetWorld().FindEntityByID(entityId);
				if (entity)
				{
					found = entity;
					break;
				}

				staleEntities.Insert(entityId); // Clean up stale mapping, after the loop
			}

			foreach (EntityID staleId : staleEntities)
			{
				m_mEntityToRecruit.Remove(staleId);
			}

			return found;
		}

		// On client, use replication ID mapping
		foreach (RplId rplId, string mappedRecruitId : m_mRplIdToRecruit)
		{
			if (mappedRecruitId == recruitId)
			{
				RplComponent rplComponent = RplComponent.Cast(Replication.FindItem(rplId));
				if (rplComponent)
					return rplComponent.GetEntity();
			}
		}
		
		return null;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Set recruit faction to match player faction
	protected void SetRecruitFaction(string playerPersistentId, IEntity recruitEntity)
	{
		// Always the configured resistance faction, never vanilla's per-player faction registry:
		// Overthrow players don't go through vanilla faction selection, so
		// SCR_FactionManager.GetPlayerFaction() returns null or CIV - the old guard compared
		// against that and silently left recruits affiliated CIV, which is friendly to every
		// faction, so recruits never classified anyone as an enemy and never fought (BUG-146)
		SCR_CharacterFactionAffiliationComponent recruitFactionComp = SCR_CharacterFactionAffiliationComponent.Cast(
			recruitEntity.FindComponent(SCR_CharacterFactionAffiliationComponent)
		);

		if (!recruitFactionComp)
		{
			Print("[Overthrow] No character faction component found on recruit");
			return;
		}

		recruitFactionComp.SetAffiliatedFactionByKey(OVT_Global.GetConfig().m_sPlayerFaction);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Add recruit to player's group
	protected void AddRecruitToPlayerGroup(string playerPersistentId, IEntity recruitEntity)
	{
		// Find player by persistent ID
		OVT_PlayerManagerComponent playerManager = OVT_Global.GetPlayers();
		if (!playerManager)
			return;
			
		int playerId = playerManager.GetPlayerIDFromPersistentID(playerPersistentId);
		if (playerId == 0)
		{
			Print("[Overthrow] Player not online, cannot add recruit to group: " + playerPersistentId);
			return;
		}
		
		// Get player controller
		SCR_PlayerController playerController = SCR_PlayerController.Cast(
			GetGame().GetPlayerManager().GetPlayerController(playerId)
		);
		
		if (!playerController)
		{
			Print("[Overthrow] No player controller found for ID: " + playerId);
			return;
		}
		
		// Get group component
		SCR_PlayerControllerGroupComponent groupController = SCR_PlayerControllerGroupComponent.Cast(
			playerController.FindComponent(SCR_PlayerControllerGroupComponent)
		);
		
		if (!groupController)
		{
			Print("[Overthrow] No group controller found for player: " + playerId);
			return;
		}
		
		// Verify player has a group and is the leader
		int groupId = groupController.GetGroupID();
		if (groupId == -1)
		{
			Print("[Overthrow] Cannot add recruit to group: player " + playerId + " is in no group", LogLevel.WARNING);
			return;
		}

		SCR_GroupsManagerComponent groupsManager = SCR_GroupsManagerComponent.GetInstance();
		if (!groupsManager)
			return;

		SCR_AIGroup group = groupsManager.FindGroup(groupId);
		if (!group)
		{
			Print("[Overthrow] Cannot add recruit to group: group " + groupId + " not found", LogLevel.WARNING);
			return;
		}

		// MEMBERSHIP, NOT LEADERSHIP (decision D8). This used to demand group.GetLeaderID() == playerId.
		// Under the shared-group model an owner standing inside a friend's group is a MEMBER and not the
		// leader, so the leader test made "recruit a civilian while in someone else's group" bail here
		// and do nothing at all - silently, because there was no failure the player could see. The
		// recruit belongs in whatever group its OWNER is in, whoever happens to lead that group; the
		// leader commands it once it is there, which is plain vanilla slave-group behaviour.
		if (!group.IsPlayerInGroup(playerId))
		{
			Print("[Overthrow] Cannot add recruit to group: player " + playerId + " is not a member of group " + groupId + " (leader is " + group.GetLeaderID() + ")", LogLevel.WARNING);
			return;
		}

		if (!group.GetSlave())
		{
			Print("[Overthrow] Cannot add recruit to group: group " + groupId + " has no slave group (commanding manager missing at group creation?)", LogLevel.WARNING);
			return;
		}

		// The AI has to be running before it can take orders
		AIControlComponent aiControl = AIControlComponent.Cast(recruitEntity.FindComponent(AIControlComponent));
		if (aiControl)
			aiControl.ActivateAI();

		// Put the recruit in the player's slave group HERE, on the server.
		//
		// This used to broadcast RpcDo_AddRecruitToGroup to EVERY client (6 s pre-delay plus a
		// 10 x 2 s entity-resolution ladder) purely so the owning client could call
		// RequestAddAIAgent - whose server handler, RPC_AskAddAIAgent, does nothing but call
		// AddAIToSlaveGroup after re-checking leadership we have already checked. We are the
		// server and we are holding the entity, so call it directly: no round trip, no timing
		// ladder, no dependency on the owner's client being responsive. AddAIToSlaveGroup
		// broadcasts membership itself (AskAddAiMemberToGroup -> RPC_DoAddAIMemberToGroup), so
		// every machine still learns the recruit is a group member.
		SCR_ChimeraCharacter recruitCharacter = SCR_ChimeraCharacter.Cast(recruitEntity);
		if (recruitCharacter)
			groupController.AddAIToSlaveGroup(recruitCharacter, group);
	}

	//------------------------------------------------------------------------------------------------
	//! Where a recruit's body goes when it arrives in the world: the owner's squad, or a parked group.
	//!
	//! THE ONE FORK. Both places that put a recruit body under command call this and nothing else -
	//! AttachRecruitBody() (a body just spawned or just restored from storage) and the already-in-world
	//! branch of RespawnPlayerRecruits() (a reserved body being woken). Everything else about the
	//! reservation and respawn flow is untouched by the inactive feature on purpose: that code is what
	//! BUG-130/131 fixed, and it is the most carefully engineered path in the recruits feature.
	//!
	//! RECONSTRUCTED CLUSTERS ARE APPROXIMATE, BY DESIGN. Bodies come back at their stored positions,
	//! so recruits that were within the cluster radius of each other when parked re-cluster into
	//! roughly the same groups - but only roughly, because the spawn requests are asynchronous and the
	//! grouping depends on the order the bodies arrive. That is accepted (decision D8, risk R4): the
	//! group has no identity a player can observe, and positions and behaviour are unchanged either
	//! way. It is written down here so nobody later reports it as a bug.
	//!
	//! \param[in] playerPersistentId The owning player.
	//! \param[in] recruit The record, whose m_bInactive decides the fork.
	//! \param[in] recruitEntity The body being placed.
	protected void PlaceRecruitInWorld(string playerPersistentId, notnull OVT_RecruitData recruit, notnull IEntity recruitEntity)
	{
		if (recruit.m_bInactive)
		{
			if (PlaceRecruitInInactiveGroup(recruit, recruitEntity))
				return;

			// A RECRUIT IS NEVER LEFT IN NO GROUP. The flag is deliberately NOT cleared here: the
			// player's choice survives a failure to honour it, and the roster disagreeing with the
			// world is a symptom of the error the placement has already logged - not something to hide
			// by quietly rewriting the record.
			Print("[Overthrow] Recruit " + recruit.m_sRecruitId + " is marked inactive but could not be parked - putting it in its owner's group for now", LogLevel.WARNING);
		}

		AddRecruitToPlayerGroup(playerPersistentId, recruitEntity);
	}

	//------------------------------------------------------------------------------------------------
	//! Makes a recruit inactive (parked, holding position) or active (back in its owner's squad).
	//!
	//! THE ONE SERVER ENTRY POINT for this state. Every route a player can take - the held action on
	//! the body, the roster button - arrives here, so there is exactly one place that decides what a
	//! transition consists of, one place that writes the flag, and one place that tells the clients.
	//!
	//! IT VALIDATES, IT DOES NOT TRUST. The caller supplies a recruit id and a desired state and
	//! nothing else; existence, liveness and "is this even a change" are all re-derived here. A request
	//! for the state a recruit is already in is REFUSED rather than re-run, because re-running a
	//! deactivation would pull the recruit out of the inactive group it is already in and build it a
	//! second one. (Ownership is re-checked by the caller that knows who is asking - Phase 3's
	//! controller component - because this method is also driven by paths where nobody is asking.)
	//!
	//! THE FLAG IS WRITTEN LAST, AND ONLY ON SUCCESS. If the world half of the transition fails, the
	//! record still says what is actually true, so the roster, the map and the save all stay honest and
	//! the player can simply try again.
	//!
	//! \param[in] recruitId The recruit to change.
	//! \param[in] inactive True to park it, false to bring it back into the squad.
	//! \return True when the state actually changed.
	bool SetRecruitInactive(string recruitId, bool inactive)
	{
		if (!Replication.IsServer())
			return false;

		OVT_RecruitData recruit = GetRecruit(recruitId);
		if (!recruit)
		{
			Print("[Overthrow] SetRecruitInactive: no such recruit " + recruitId, LogLevel.WARNING);
			return false;
		}

		if (recruit.m_bInactive == inactive)
			return false;

		IEntity recruitEntity = FindRecruitEntity(recruitId);
		if (!recruitEntity)
		{
			// Nothing to move. This state is about where a BODY stands, so a recruit that has none
			// cannot be parked or unparked - the respawn fork honours whatever the record says when the
			// body eventually comes back.
			Print("[Overthrow] SetRecruitInactive: recruit " + recruitId + " has no body in the world", LogLevel.WARNING);
			return false;
		}

		if (inactive)
		{
			if (!DeactivateRecruit(recruit, recruitEntity))
				return false;
		}
		else
		{
			if (!ReactivateRecruit(recruit, recruitEntity))
				return false;
		}

		recruit.m_bInactive = inactive;
		BroadcastRecruitActiveState(recruit);
		m_OnRecruitActiveStateChanged.Invoke(recruit, inactive);

		Print("[Overthrow] Recruit " + recruitId + " is now " + DescribeInactiveState(inactive), LogLevel.NORMAL);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! "INACTIVE" or "ACTIVE" in words, for a log line.
	//!
	//! A method rather than an inline conditional because EnforceScript has no ternary operator, and a
	//! four-line if/else inside a Print() call buries the thing being logged.
	//! \param[in] inactive The state to describe.
	//! \return The label.
	protected string DescribeInactiveState(bool inactive)
	{
		if (inactive)
			return "INACTIVE (parked, holding position)";

		return "ACTIVE (back in its owner's group)";
	}

	//------------------------------------------------------------------------------------------------
	//! The world half of parking a recruit: out of the owner's slave group, into an inactive one.
	//!
	//! The slave-group exit is PlaceRecruitInInactiveGroup()'s, not this method's - see its header for
	//! why it has to own the exit. What lives here is the half that only the deliberate transition
	//! wants: the ROLLBACK.
	//!
	//! \param[in] recruit The record being parked.
	//! \param[in] recruitEntity Its body.
	//! \return True when the recruit is now in an inactive group.
	protected bool DeactivateRecruit(notnull OVT_RecruitData recruit, notnull IEntity recruitEntity)
	{
		if (PlaceRecruitInInactiveGroup(recruit, recruitEntity))
			return true;

		// The body may now be in NO group at all - the exit ran and the placement did not. Put it back
		// where it came from rather than leaving live AI with nobody commanding it, and refuse the
		// transition: the record is never written, so nothing downstream comes to believe this recruit
		// is parked. (This is the ONE place a failed placement can be undone; the respawn fork cannot
		// roll anything back, because there is nowhere the body came from.)
		Print("[Overthrow] Recruit " + recruit.m_sRecruitId + " could not be parked - returning it to its owner's group", LogLevel.WARNING);
		AddRecruitToPlayerGroup(recruit.m_sOwnerPersistentId, recruitEntity);

		return false;
	}

	//------------------------------------------------------------------------------------------------
	//! The world half of bringing a parked recruit back: out of the inactive group, into the squad.
	//!
	//! A recruit that turns out NOT to be in one of our groups is still reactivated. That is a
	//! legitimate state, not an error: an owner who disconnects has their recruits' AI deactivated
	//! (ReserveRecruitBody), which can empty the inactive group and let vanilla destroy it, and the
	//! group is derived state that is rebuilt on their return.
	//!
	//! \param[in] recruit The record being reactivated.
	//! \param[in] recruitEntity Its body.
	//! \return True - reactivation is not refused. A recruit that could not be put into its owner's
	//!         group is reported and left ACTIVE, which is honest and self-heals on the next group
	//!         membership change (MoveRecruitsToGroup takes every online, active recruit).
	protected bool ReactivateRecruit(notnull OVT_RecruitData recruit, notnull IEntity recruitEntity)
	{
		RemoveRecruitFromInactiveGroup(recruitEntity);

		AddRecruitToPlayerGroup(recruit.m_sOwnerPersistentId, recruitEntity);

		if (!FindRecruitParentGroup(recruitEntity))
			Print("[Overthrow] Recruit " + recruit.m_sRecruitId + " was reactivated but ended up in no group - is its owner in one?", LogLevel.WARNING);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Puts a recruit's body into an INACTIVE group: the nearest owned one, or a brand new one.
	//!
	//! ==========================================================================================
	//! TWO THINGS MUST NEVER BE DONE TO THE GROUP THIS CREATES:
	//!
	//!  1. NEVER CHANGE THIS GROUP'S LIFECYCLE POLICY. It is left at the default, Manual, and
	//!     SCR_AIGroup.EOnFrame (Entities/SCR_AIGroup.c:313-318) only runs the proximity accumulator
	//!     for groups whose policy is ProximityDriven - so a Manual group is never proximity-despawned.
	//!     A proximity-driven inactive group would DELETE THE RECRUIT BODIES at 800 m, which is the
	//!     whole feature gone (risk R2).
	//!  2. NEVER ROUTE THIS GROUP THROUGH THE SPAWNING API'S CLEANUP HELPERS. They delete every member
	//!     soldier of the group they are handed (OVT_EntitySpawningAPI.c:379-400) - and the members of
	//!     this one are the player's recruits.
	//! ==========================================================================================
	//!
	//! CLUSTERING, IN TWO HALVES. Which records could be hosting a nearby group is decided by the pure
	//! OVT_RecruitInactiveGrouping.SelectClusterCandidates(); resolving those ids to bodies, asking
	//! each body's parent group whether it is one of ours, and taking the first that is, happens here,
	//! because only that half needs the world. First suitable host wins (decision D10), which keeps the
	//! outcome deterministic and therefore reproducible in a failing play-test.
	//!
	//! IT OWNS THE EXIT FROM WHATEVER GROUP THE BODY IS IN, and it has to, because a body can reach it
	//! ALREADY PARENTED by two different routes: the deactivate transition (it is standing in its
	//! owner's slave group) and the respawn fork (a reserved body was only ever DeactivateAI'd, not
	//! removed from anything, so a returning owner's parked recruit can still be a member of the group
	//! it was parked in). AddAIEntityToGroup refuses to move an already-parented agent
	//! (Entities/SCR_AIGroup.c:1930-1932), so leaving that to the callers would make the outcome depend
	//! on which of them remembered - and on an engine behaviour (does DeactivateAI unparent?) that is
	//! not worth depending on either way.
	//!
	//! \param[in] recruit The record being parked. Its last known position is refreshed here.
	//! \param[in] recruitEntity Its body, wherever it currently is.
	//! \return True when the body is in an inactive group.
	protected bool PlaceRecruitInInactiveGroup(notnull OVT_RecruitData recruit, notnull IEntity recruitEntity)
	{
		vector position = recruitEntity.GetOrigin();

		// The record's position IS the clustering input for the NEXT recruit parked near this one
		// (OVT_RecruitInactiveGrouping rule 4), and until Phase 4's status sweep lands nothing else
		// refreshes it between a body being placed and a save point. Writing it here is what makes
		// "park two recruits side by side" cluster them even when both have walked a long way since
		// their bodies were spawned.
		recruit.m_vLastKnownPosition = position;

		SCR_AIGroup currentGroup = FindRecruitParentGroup(recruitEntity);

		// ALREADY PARKED. Idempotent on purpose - the respawn fork re-runs for a body that never left
		// its inactive group, and "put it back where it already is" must be a no-op rather than a
		// second group. Re-clustering it would also be wrong: the group it is in is by definition a
		// group of its owner's parked recruits at this spot.
		if (currentGroup && OVT_InactiveRecruitGroupComponent.Cast(currentGroup.FindComponent(OVT_InactiveRecruitGroupComponent)))
			return true;

		// Any OTHER group has to be left through the full exit, which also broadcasts the membership
		// change - a bare RemoveAgent would leave the recruit showing in the owner's group UI on every
		// client forever.
		if (currentGroup)
		{
			SCR_GroupsManagerComponent groupsManager = SCR_GroupsManagerComponent.GetInstance();
			if (groupsManager)
				RemoveRecruitFromSlaveGroup(recruitEntity, currentGroup, groupsManager);
		}

		SCR_AIGroup hostGroup = FindInactiveClusterHost(recruit, position);
		if (hostGroup)
		{
			if (AddRecruitAgentToGroup(hostGroup, recruitEntity))
			{
				Print("[Overthrow] Recruit " + recruit.m_sRecruitId + " joined a nearby inactive group (" + hostGroup.GetAgentsCount().ToString() + " parked there now)", LogLevel.NORMAL);
				return true;
			}

			// A group of its own is a better answer than no group: the recruit is out of its owner's
			// squad either way, and it is the group that carries the defend order.
			Print("[Overthrow] Recruit " + recruit.m_sRecruitId + " could not join the nearby inactive group - giving it one of its own", LogLevel.WARNING);
		}

		return CreateInactiveGroupFor(recruit, recruitEntity, position);
	}

	//------------------------------------------------------------------------------------------------
	//! Spawns one inactive-recruit group, dresses it, and puts the first recruit in it.
	//!
	//! Everything that makes the group usable happens while it is still EMPTY, because SetFaction()
	//! rewrites the faction affiliation of every agent already in a group
	//! (Entities/SCR_AIGroup.c:2112-2120), and the defend waypoint should be waiting for the first
	//! member rather than arriving after it.
	//!
	//! The prohibitions in PlaceRecruitInInactiveGroup()'s header apply to the group built here.
	//!
	//! \param[in] recruit The record being parked.
	//! \param[in] recruitEntity Its body.
	//! \param[in] position Where to build the group and aim its defend waypoint.
	//! \return True when the group exists and holds this recruit. On false NOTHING is left behind.
	protected bool CreateInactiveGroupFor(notnull OVT_RecruitData recruit, notnull IEntity recruitEntity, vector position)
	{
		if (m_sInactiveGroupPrefab.IsEmpty())
		{
			Print("[Overthrow] No inactive-recruit group prefab is set on the recruit manager - recruit " + recruit.m_sRecruitId + " cannot be parked", LogLevel.ERROR);
			return false;
		}

		IEntity groupEntity = OVT_Global.SpawnEntityPrefab(m_sInactiveGroupPrefab, position);
		SCR_AIGroup group = SCR_AIGroup.Cast(groupEntity);
		if (!group)
		{
			if (groupEntity)
				SCR_EntityHelper.DeleteEntityAndChildren(groupEntity);

			Print("[Overthrow] The inactive-recruit group prefab did not produce an SCR_AIGroup - recruit " + recruit.m_sRecruitId + " cannot be parked", LogLevel.ERROR);
			return false;
		}

		OVT_InactiveRecruitGroupComponent marker = OVT_InactiveRecruitGroupComponent.Cast(group.FindComponent(OVT_InactiveRecruitGroupComponent));
		if (!marker)
		{
			// REFUSED, NOT TOLERATED. Without the marker this group could never be recognised as a
			// cluster host again, and nothing would ever delete its defend waypoint - so a prefab
			// missing the component would silently leak one group and one waypoint per parked recruit.
			SCR_EntityHelper.DeleteEntityAndChildren(group);
			Print("[Overthrow] The inactive-recruit group prefab has no OVT_InactiveRecruitGroupComponent - recruit " + recruit.m_sRecruitId + " cannot be parked", LogLevel.ERROR);
			return false;
		}

		marker.SetOwnerPersistentId(recruit.m_sOwnerPersistentId);

		OVT_OverthrowConfigComponent config = OVT_Global.GetConfig();
		if (config)
		{
			// The same faction the recruit bodies themselves carry (SetRecruitFaction), so the group and
			// its members agree about who they are.
			FactionManager factionManager = GetGame().GetFactionManager();
			if (factionManager)
			{
				Faction playerFaction = factionManager.GetFactionByKey(config.m_sPlayerFaction);
				if (playerFaction)
					group.SetFaction(playerFaction);
			}

			// Hold in place with a plain wait waypoint (BUG-170, revising decision D9). The garrison-style
			// defend waypoint continuously re-manages the group - cover picks, stance changes, order
			// barks - which on a parked roadside recruit is an audible, visible re-order loop. A waiting
			// group still engages perceived threats (the town patrols wait at points via the same
			// waypoint and return fire, OVT_TownController.c:175). The timer is effectively infinite for
			// a session-scoped group: it is rebuilt from the records on every boot (D7/D8), so the
			// wait never expires in practice.
			AIWaypoint waypoint = config.SpawnWaitWaypoint(position, INACTIVE_HOLD_WAIT_SECONDS);
			if (waypoint)
			{
				group.AddWaypoint(waypoint);

				// Handing it to the marker is what deletes it when the group dies. AddWaypoint() does
				// NOT take ownership: vanilla destroys the waypoints IT spawned explicitly
				// (SCR_AIGroup.DestroyEntities :1871-1886), and this group is destroyed by vanilla's
				// delete-when-empty far more often than by anything here.
				marker.SetWaypoint(waypoint);
			}
		}

		// SESSION-SCOPED BY CONSTRUCTION (decision D7, BUG-118). The grouping is rebuilt from the
		// recruit records on the next boot, so a persistence record for one of these groups would be a
		// permanent orphan. Same rule, and the same call, every waypoint Overthrow spawns already gets
		// (OVT_Global.SpawnEntityPrefab).
		OVT_PersistenceManagerComponent.UntrackTransient(groupEntity);

		if (!AddRecruitAgentToGroup(group, recruitEntity) || group.GetAgentsCount() < 1)
		{
			// VANILLA WILL NOT CLEAN THIS UP. m_bDeleteWhenEmpty deletes a group whose last member
			// LEAVES; its own attribute description says it will "*not* delete the group when it starts
			// empty" (Entities/SCR_AIGroup.c:95-97), and OnEmpty() is only raised by a removal. A group
			// that never received its first agent is therefore ours to destroy, here, or it stands in
			// the world with a defend waypoint and nobody in it for the rest of the session.
			Print("[Overthrow] Recruit " + recruit.m_sRecruitId + " could not be added to its new inactive group - deleting the group", LogLevel.WARNING);
			SCR_EntityHelper.DeleteEntityAndChildren(group);
			return false;
		}

		Print("[Overthrow] Recruit " + recruit.m_sRecruitId + " parked in a new inactive group at " + position.ToString(), LogLevel.NORMAL);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! An inactive group near a spot that one of this owner's OTHER parked recruits is already in.
	//!
	//! The candidate ids come from the pure selector; this walks them in table order and returns the
	//! first whose body is actually in a group carrying our marker component. A candidate whose body
	//! has gone, or who turns out not to be in one of our groups after all, is skipped rather than
	//! treated as an error - the records and the world are allowed to be a frame apart.
	//!
	//! \param[in] recruit The recruit being placed - excluded from its own candidate list.
	//! \param[in] position Where it is standing.
	//! \return A group to join, or null when one must be created.
	protected SCR_AIGroup FindInactiveClusterHost(notnull OVT_RecruitData recruit, vector position)
	{
		// SelectClusterCandidates returns a FRESH array, which is also the snapshot the loop below
		// needs: FindRecruitEntity() prunes stale entries from m_mEntityToRecruit as it walks, so
		// iterating a live manager collection while calling it is a known hazard. Nothing here iterates
		// a collection this manager owns.
		array<string> candidates = OVT_RecruitInactiveGrouping.SelectClusterCandidates(
			GetPlayerRecruits(recruit.m_sOwnerPersistentId),
			recruit.m_sRecruitId,
			position,
			OVT_RecruitInactiveGrouping.DEFAULT_CLUSTER_RADIUS);

		foreach (string candidateId : candidates)
		{
			IEntity candidateEntity = FindRecruitEntity(candidateId);
			if (!candidateEntity)
				continue;

			SCR_AIGroup candidateGroup = FindRecruitParentGroup(candidateEntity);
			if (!candidateGroup)
				continue;

			if (!OVT_InactiveRecruitGroupComponent.Cast(candidateGroup.FindComponent(OVT_InactiveRecruitGroupComponent)))
				continue;

			return candidateGroup;
		}

		return null;
	}

	//------------------------------------------------------------------------------------------------
	//! Takes a recruit's body out of the inactive group it is in, if it is in one.
	//!
	//! THE GROUP IS NOT DELETED HERE, EVER. If that was its last member, vanilla's OnEmpty() has
	//! already queued the deletion for the next frame (Entities/SCR_AIGroup.c:2442-2455), and the
	//! defend waypoint goes with it through OVT_InactiveRecruitGroupComponent.OnDelete(). Deleting it
	//! here as well would be a double delete of an entity vanilla is still holding a pointer to.
	//!
	//! \param[in] recruitEntity The body to pull out.
	//! \return True when it was in one of our groups and has now left it.
	protected bool RemoveRecruitFromInactiveGroup(notnull IEntity recruitEntity)
	{
		SCR_AIGroup parentGroup = FindRecruitParentGroup(recruitEntity);
		if (!parentGroup)
			return false;

		if (!OVT_InactiveRecruitGroupComponent.Cast(parentGroup.FindComponent(OVT_InactiveRecruitGroupComponent)))
			return false;

		parentGroup.RemoveAgentFromControlledEntity(recruitEntity);

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! Adds a recruit's agent to a group and CONFIRMS it arrived.
	//!
	//! AddAIEntityToGroup RETURNS TRUE WITHOUT MOVING an agent that already belongs to a group -
	//! "Add to group only if some other system wasn't faster" (Entities/SCR_AIGroup.c:1930-1932) - so
	//! its return value alone cannot tell "joined" apart from "was already somewhere else". The agent
	//! hierarchy is the only honest confirmation, and the create-then-fail guard depends on it.
	//!
	//! \param[in] group The group to join.
	//! \param[in] recruitEntity The body.
	//! \return True only when the recruit's agent's parent group IS this group afterwards.
	protected bool AddRecruitAgentToGroup(notnull SCR_AIGroup group, notnull IEntity recruitEntity)
	{
		if (!group.AddAIEntityToGroup(recruitEntity))
			return false;

		return FindRecruitParentGroup(recruitEntity) == group;
	}

	//------------------------------------------------------------------------------------------------
	//! The AI group a recruit's body currently belongs to, straight from the agent hierarchy.
	//!
	//! The engine's hierarchy is the authority on where an agent is - not the replicated m_aAIMembers
	//! list, which is bookkeeping for the UI and can be a frame behind.
	//! \param[in] recruitEntity The body.
	//! \return Its parent group, or null when it has no agent or is in no group.
	protected SCR_AIGroup FindRecruitParentGroup(notnull IEntity recruitEntity)
	{
		AIControlComponent aiControl = AIControlComponent.Cast(recruitEntity.FindComponent(AIControlComponent));
		if (!aiControl)
			return null;

		AIAgent agent = aiControl.GetAIAgent();
		if (!agent)
			return null;

		return SCR_AIGroup.Cast(agent.GetParentGroup());
	}

	//------------------------------------------------------------------------------------------------
	//! Moves every one of an owner's LIVE recruits into another group's slave group. SERVER ONLY.
	//!
	//! This is the "recruits follow their owner IN" half of the reactor
	//! (OVT_PlayerGroupManagerComponent.OnGroupPlayerAdded). Ownership is NOT touched: no line below
	//! writes m_sOwnerPersistentId or OVT_PlayerOwnerComponent, and none ever may. A recruit inside a
	//! shared group is commanded by that group's leader and still owned, renamed, dismissed, equipped
	//! and XP-credited by the player who recruited it.
	//!
	//! THE TARGET GROUP IS AN ARGUMENT AND MUST STAY ONE. It cannot be read back from the owner's
	//! group controller here: during a group SWITCH vanilla fires both membership invokers while
	//! SCR_PlayerControllerGroupComponent.m_iGroupID still holds the OLD group id (it is only written
	//! afterwards, in RPC_AskJoinGroup at Groups/SCR_PlayerControllerGroupComponent.c:892), so a
	//! GetGroupID() lookup at this moment would place the recruits back where they came from.
	//!
	//! \param[in] ownerPersistentId The recruits' owner.
	//! \param[in] targetGroup The MASTER group the owner is now in - the recruits go into its slave.
	//! \param[in] ownerOnline Resolved by the caller from PlayerManager.GetPlayerController(), never
	//!            from the player record's own liveness accessor (decision D9). False transfers
	//!            nothing.
	//! \return How many recruits were actually placed in the target group's slave group. Phase 5 uses
	//!         this for the leader's "X joined with N recruits" notification, so it must stay the count
	//!         that ARRIVED - not the count the owner holds on paper.
	int MoveRecruitsToGroup(string ownerPersistentId, notnull SCR_AIGroup targetGroup, bool ownerOnline)
	{
		if (!Replication.IsServer())
			return 0;

		if (ownerPersistentId.IsEmpty())
			return 0;

		// SelectTransferable returns a FRESH array of ids, which is also the snapshot the loop below
		// needs: FindRecruitEntity() prunes stale entries from m_mEntityToRecruit as it walks, so
		// iterating a live manager collection while calling it is a known hazard (task T6.7). Nothing
		// here iterates a collection this manager owns.
		int skippedOffline = 0;
		int skippedInactive = 0;
		array<string> recruitIds = OVT_GroupRecruitTransfer.SelectTransferable(GetPlayerRecruits(ownerPersistentId), ownerOnline, skippedOffline, skippedInactive);

		if (recruitIds.IsEmpty())
		{
			if (skippedOffline > 0 || skippedInactive > 0)
				Print("[Overthrow] MoveRecruitsToGroup: none of " + ownerPersistentId + "'s recruits followed them into group " + targetGroup.GetGroupID() + " (" + skippedOffline + " have no body in the world, " + skippedInactive + " are parked)", LogLevel.NORMAL);

			return 0;
		}

		if (!targetGroup.GetSlave())
		{
			Print("[Overthrow] MoveRecruitsToGroup: group " + targetGroup.GetGroupID() + " has no slave group - " + recruitIds.Count() + " recruits of " + ownerPersistentId + " cannot follow (commanding manager missing at group creation?)", LogLevel.WARNING);
			return 0;
		}

		SCR_PlayerControllerGroupComponent groupController = FindOwnerGroupController(ownerPersistentId);
		if (!groupController)
		{
			Print("[Overthrow] MoveRecruitsToGroup: no group controller for owner " + ownerPersistentId + " - " + recruitIds.Count() + " recruits cannot follow them into group " + targetGroup.GetGroupID(), LogLevel.WARNING);
			return 0;
		}

		int moved = 0;
		foreach (string recruitId : recruitIds)
		{
			IEntity recruitEntity = FindRecruitEntity(recruitId);
			if (!recruitEntity)
			{
				// The record says this recruit has a body and the world disagrees. Say so instead of
				// dereferencing: a recruit mid-despawn or mid-respawn is the one shape this can take.
				Print("[Overthrow] MoveRecruitsToGroup: recruit " + recruitId + " is marked as having a body but no entity was found - skipped", LogLevel.WARNING);
				continue;
			}

			// The AI has to be running before it can take orders. ActivateAI() on an already active
			// agent is a no-op, so this is safe for a recruit that was already following its owner.
			AIControlComponent aiControl = AIControlComponent.Cast(recruitEntity.FindComponent(AIControlComponent));
			if (aiControl)
				aiControl.ActivateAI();

			// Same call the recruit path already uses (AddRecruitToPlayerGroup above): it does
			// slaveGroup.AddAgentFromControlledEntity() plus AskAddAiMemberToGroup(), and the latter
			// broadcasts membership to every machine itself
			// (Groups/SCR_PlayerControllerGroupComponent.c:1470-1495).
			SCR_ChimeraCharacter recruitCharacter = SCR_ChimeraCharacter.Cast(recruitEntity);
			if (!recruitCharacter)
				continue;

			groupController.AddAIToSlaveGroup(recruitCharacter, targetGroup);
			moved++;
		}

		Print("[Overthrow] Moved " + moved + " of " + ownerPersistentId + "'s recruits into group " + targetGroup.GetGroupID() + " (" + skippedOffline + " skipped for having no body in the world, " + skippedInactive + " skipped for being parked)", LogLevel.NORMAL);

		return moved;
	}

	//------------------------------------------------------------------------------------------------
	//! Pulls every one of an owner's recruits back out of a group's slave group. SERVER ONLY.
	//!
	//! This is the "recruits follow their owner OUT" half of the reactor
	//! (OVT_PlayerGroupManagerComponent.OnGroupPlayerRemoved) and it must run SYNCHRONOUSLY inside the
	//! removal frame. Two independent reasons:
	//!   1. the ex-leader must stop commanding somebody else's AI the moment that somebody leaves;
	//!   2. an emptied group is queued for destruction by vanilla's own removal handler and destroyed
	//!      NEXT FRAME (DeleteGroupDelayed -> DeleteGroups, Groups/SCR_GroupsManagerComponent.c:721-730,
	//!      :1647-1654). UnregisterGroup only deletes the slave group when it has no agents left
	//!      (:1035-1040, with vanilla's own "mourTodo: handle what the AIs should do in case their
	//!      master group is deleted" beside it) - so recruits left behind here are AI stranded in a
	//!      leaked slave group whose master no longer exists.
	//!
	//! Ownership is NOT touched. Nothing below writes m_sOwnerPersistentId or OVT_PlayerOwnerComponent.
	//!
	//! IT DELIBERATELY NEEDS NO PLAYER CONTROLLER. The move-IN half calls
	//! SCR_PlayerControllerGroupComponent.AddAIToSlaveGroup on the owner's controller, but the move-OUT
	//! half must keep working for an owner who no longer HAS one - a disconnect is a removal, and it is
	//! the removal where leaving recruits behind is worst.
	//!
	//! THE PER-RECRUIT EXIT ITSELF LIVES IN RemoveRecruitFromSlaveGroup(), which this loop calls once
	//! per recruit. It was extracted so that the inactive-recruit path performs the identical sequence
	//! rather than a second, drifting copy of it; the "keep in step with vanilla" note travels with the
	//! sequence, on that method.
	//!
	//! \param[in] ownerPersistentId The recruits' owner.
	//! \param[in] exGroup The MASTER group being left - recruits are pulled out of its slave.
	void RemoveRecruitsFromGroup(string ownerPersistentId, notnull SCR_AIGroup exGroup)
	{
		if (!Replication.IsServer())
			return;

		if (ownerPersistentId.IsEmpty())
			return;

		if (!m_mRecruitsByOwner.Contains(ownerPersistentId))
			return;

		SCR_AIGroup slaveGroup = exGroup.GetSlave();
		if (!slaveGroup)
			return;

		SCR_GroupsManagerComponent groupsManager = SCR_GroupsManagerComponent.GetInstance();
		if (!groupsManager)
			return;

		// Cheap early bail with a diagnostic that names the group and the owner - the per-recruit exit
		// below re-resolves this and can only report per recruit.
		if (!RplComponent.Cast(slaveGroup.FindComponent(RplComponent)))
		{
			Print("[Overthrow] RemoveRecruitsFromGroup: the slave group of group " + exGroup.GetGroupID() + " has no RplComponent - " + ownerPersistentId + "'s recruits cannot be removed from it", LogLevel.WARNING);
			return;
		}

		// SNAPSHOT before iterating: FindRecruitEntity() prunes stale entries from m_mEntityToRecruit as
		// it walks, and the removal reaches back into vanilla group state. Neither may run while this
		// loop is holding a live manager collection open (task T6.7).
		array<string> recruitIds = new array<string>();
		foreach (string ownedId : m_mRecruitsByOwner[ownerPersistentId])
		{
			recruitIds.Insert(ownedId);
		}

		int removed = 0;
		foreach (string recruitId : recruitIds)
		{
			IEntity recruitEntity = FindRecruitEntity(recruitId);
			if (!recruitEntity)
				continue;

			if (RemoveRecruitFromSlaveGroup(recruitEntity, slaveGroup, groupsManager))
				removed++;
		}

		Print("[Overthrow] Pulled " + removed + " of " + ownerPersistentId + "'s recruits out of group " + exGroup.GetGroupID() + "'s slave group", LogLevel.NORMAL);
	}

	//------------------------------------------------------------------------------------------------
	//! Takes ONE recruit out of ONE slave group - the whole four-step exit, in one place.
	//!
	//! THE SINGLE IMPLEMENTATION OF THE EXIT. Two callers need it: the group reactor pulling a whole
	//! squad out when its owner leaves a group (RemoveRecruitsFromGroup), and the parking path taking
	//! one recruit out of wherever it is (PlaceRecruitInInactiveGroup). They must not drift apart,
	//! because the sequence is not obvious and getting it wrong strands AI in a leaked group rather
	//! than failing visibly.
	//!
	//! IT DELIBERATELY NEEDS NO PLAYER CONTROLLER. Vanilla's mirror
	//! (SCR_PlayerControllerGroupComponent.RemoveAiFromSlaveGroup,
	//! Groups/SCR_PlayerControllerGroupComponent.c:1526-1552) reads nothing off the component it is
	//! called on, so this is that method inlined, minus the dependency - which is what keeps it working
	//! for an owner who has already disconnected. The three steps it inlines are
	//! Deactivate-the-slave-when-the-last-AI-leaves (:1536-1537), RemoveAgentFromControlledEntity, then
	//! AskRemoveAiMemberFromGroup (:1557), which broadcasts RPC_DoRemoveAIMemberFromGroup to every
	//! machine. KEEP IT IN STEP WITH THAT METHOD when Reforger updates (recruits checklist R10).
	//!
	//! THE PARENT-GROUP CHECK IS NOT A FORMALITY. A recruit whose body exists but who was never
	//! placed (its owner was mid-respawn, say) has no business being handed to RemoveAgent, and the
	//! engine's agent hierarchy is the authority on where it is - not the replicated m_aAIMembers
	//! list, which is bookkeeping for the UI.
	//!
	//! \param[in] recruitEntity The recruit's body.
	//! \param[in] slaveGroup The slave group it should be leaving.
	//! \param[in] groupsManager The groups manager, used to broadcast the membership change.
	//! \return True when this recruit was actually in that slave group and has now left it.
	protected bool RemoveRecruitFromSlaveGroup(IEntity recruitEntity, SCR_AIGroup slaveGroup, SCR_GroupsManagerComponent groupsManager)
	{
		if (!recruitEntity || !slaveGroup || !groupsManager)
			return false;

		AIControlComponent aiControl = AIControlComponent.Cast(recruitEntity.FindComponent(AIControlComponent));
		if (!aiControl)
			return false;

		AIAgent agent = aiControl.GetAIAgent();
		if (!agent)
			return false;

		if (SCR_AIGroup.Cast(agent.GetParentGroup()) != slaveGroup)
			return false;

		RplComponent characterRplComponent = RplComponent.Cast(recruitEntity.FindComponent(RplComponent));
		if (!characterRplComponent)
		{
			Print("[Overthrow] RemoveRecruitFromSlaveGroup: recruit body " + recruitEntity + " has no RplComponent - left in the slave group", LogLevel.WARNING);
			return false;
		}

		RplComponent slaveRplComponent = RplComponent.Cast(slaveGroup.FindComponent(RplComponent));
		if (!slaveRplComponent)
		{
			Print("[Overthrow] RemoveRecruitFromSlaveGroup: slave group " + slaveGroup.GetGroupID() + " has no RplComponent - the recruit cannot be removed from it", LogLevel.WARNING);
			return false;
		}

		// Vanilla deactivates the slave group as its LAST AI leaves (:1536-1537), which is what stops
		// an empty slave group being ticked. AddAIToSlaveGroup re-activates it (:1480-1481), so this
		// round-trips for a group that later gains recruits again.
		if (slaveGroup.GetAgentsCount() == 1)
			slaveGroup.Deactivate();

		slaveGroup.RemoveAgentFromControlledEntity(recruitEntity);
		groupsManager.AskRemoveAiMemberFromGroup(slaveRplComponent.Id(), characterRplComponent.Id());

		return true;
	}

	//------------------------------------------------------------------------------------------------
	//! The group component on an owner's player controller, or null when they are not in the session.
	//!
	//! AddAIToSlaveGroup is effectively a static helper on that component - it reads the group and the
	//! groups manager, never `this` - but it is called on the OWNER's controller deliberately, so that
	//! the "whose AI is this" relationship stays readable in the code and keeps working if vanilla ever
	//! starts consulting the component it is called on. The move-OUT path deliberately does NOT use
	//! this: see RemoveRecruitsFromGroup.
	//!
	//! \param[in] ownerPersistentId Persistent id of the owning player.
	//! \return The owner's group component, or null.
	protected SCR_PlayerControllerGroupComponent FindOwnerGroupController(string ownerPersistentId)
	{
		OVT_PlayerManagerComponent playerManager = OVT_Global.GetPlayers();
		if (!playerManager)
			return null;

		int playerId = playerManager.GetPlayerIDFromPersistentID(ownerPersistentId);
		if (playerId < 1)
			return null;

		PlayerController playerController = GetGame().GetPlayerManager().GetPlayerController(playerId);
		if (!playerController)
			return null;

		return SCR_PlayerControllerGroupComponent.Cast(playerController.FindComponent(SCR_PlayerControllerGroupComponent));
	}

	//------------------------------------------------------------------------------------------------
	//! Saves recruit data for network replication (Join-In-Progress)
	//!
	//! m_sBodyPersistenceId is DELIBERATELY NOT SENT. It is a server-side handle into the persistence
	//! system, which only exists on the authority (SystemLocation Server); a client can neither resolve
	//! it nor spawn anything with it, and adding it would change the JIP wire format for nothing.
	//! Clients learn about a recruit's body the way they always have - through the RplId below.
	//! \param[in] writer The ScriptBitWriter to write data to
	//! \return true if serialization is successful
	override bool RplSave(ScriptBitWriter writer)
	{
		// Write number of recruits
		int recruitCount = m_mRecruits.Count();
		writer.WriteInt(recruitCount);
		
		// Write each recruit's data
		for (int i = 0; i < recruitCount; i++)
		{
			string recruitId = m_mRecruits.GetKey(i);
			OVT_RecruitData recruit = m_mRecruits.GetElement(i);
			
			if (!recruit)
				continue;
			
			// Write basic recruit data
			writer.WriteString(recruitId);
			writer.WriteString(recruit.m_sOwnerPersistentId);
			writer.WriteString(recruit.m_sName);
			writer.WriteInt(recruit.m_iXP);
			writer.WriteInt(recruit.m_iKills);
			writer.WriteInt(recruit.m_iLevel);
			writer.WriteVector(recruit.m_vLastKnownPosition);
			writer.WriteBool(recruit.m_bIsTraining);
			writer.WriteFloat(recruit.m_fTrainingCompleteTime);
			writer.WriteBool(recruit.m_bIsOnline);
			writer.WriteInt(recruit.m_iTownId);
			
			// Write replication ID for client entity mapping
			RplId recruitRplId = RplId.Invalid();
			IEntity recruitEntity = FindRecruitEntity(recruitId);
			if (recruitEntity)
			{
				RplComponent rplComponent = RplComponent.Cast(recruitEntity.FindComponent(RplComponent));
				if (rplComponent)
					recruitRplId = rplComponent.Id();
			}
			writer.WriteRplId(recruitRplId);
			
			// Write skills map
			int skillCount = recruit.m_mSkills.Count();
			writer.WriteInt(skillCount);
			for (int j = 0; j < skillCount; j++)
			{
				string skillName = recruit.m_mSkills.GetKey(j);
				int skillLevel = recruit.m_mSkills.GetElement(j);
				writer.WriteString(skillName);
				writer.WriteInt(skillLevel);
			}

			// APPENDED LAST, and it must stay last. The JIP payload is positional: RplLoad reads these
			// fields back in exactly this order, so a new field may only ever go on the end of the
			// per-recruit block. Both sides of this wire ship together, so there is no version to
			// negotiate - the order IS the format.
			writer.WriteBool(recruit.m_bInactive);
		}

		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Loads recruit data received from server during Join-In-Progress
	//! \param[in] reader The ScriptBitReader to read data from
	//! \return true if deserialization is successful
	override bool RplLoad(ScriptBitReader reader)
	{
		int recruitCount;
		if (!reader.ReadInt(recruitCount))
			return false;
		
		// Clear existing data
		m_mRecruits.Clear();
		m_mRecruitsByOwner.Clear();
		m_mRplIdToRecruit.Clear();
		
		// Read each recruit's data
		for (int i = 0; i < recruitCount; i++)
		{
			string recruitId, ownerPersistentId, name;
			int xp, kills, level, townId;
			vector lastKnownPosition;
			bool isTraining, isOnline;
			float trainingCompleteTime;
			
			// Read basic recruit data
			if (!reader.ReadString(recruitId)) return false;
			if (!reader.ReadString(ownerPersistentId)) return false;
			if (!reader.ReadString(name)) return false;
			if (!reader.ReadInt(xp)) return false;
			if (!reader.ReadInt(kills)) return false;
			if (!reader.ReadInt(level)) return false;
			if (!reader.ReadVector(lastKnownPosition)) return false;
			if (!reader.ReadBool(isTraining)) return false;
			if (!reader.ReadFloat(trainingCompleteTime)) return false;
			if (!reader.ReadBool(isOnline)) return false;
			if (!reader.ReadInt(townId)) return false;
			
			// Read replication ID for client entity mapping
			RplId recruitRplId;
			if (!reader.ReadRplId(recruitRplId)) return false;
			
			// Create recruit data
			OVT_RecruitData recruit = new OVT_RecruitData();
			recruit.m_sRecruitId = recruitId;
			recruit.m_sOwnerPersistentId = ownerPersistentId;
			recruit.m_sName = name;
			recruit.m_iXP = xp;
			recruit.m_iKills = kills;
			recruit.m_iLevel = level;
			recruit.m_vLastKnownPosition = lastKnownPosition;
			recruit.m_bIsTraining = isTraining;
			recruit.m_fTrainingCompleteTime = trainingCompleteTime;
			recruit.m_bIsOnline = isOnline;
			recruit.m_iTownId = townId;
			
			// Read skills map
			int skillCount;
			if (!reader.ReadInt(skillCount)) return false;
			for (int j = 0; j < skillCount; j++)
			{
				string skillName;
				int skillLevel;
				if (!reader.ReadString(skillName)) return false;
				if (!reader.ReadInt(skillLevel)) return false;
				recruit.m_mSkills[skillName] = skillLevel;
			}

			// LAST in the per-recruit block, matching RplSave. Read order must equal write order.
			bool isInactive;
			if (!reader.ReadBool(isInactive)) return false;
			recruit.m_bInactive = isInactive;

			// Add to collections
			m_mRecruits[recruitId] = recruit;
			
			// Add replication ID mapping if valid
			if (recruitRplId != RplId.Invalid())
			{
				m_mRplIdToRecruit[recruitRplId] = recruitId;
			}
			
			if (!m_mRecruitsByOwner.Contains(ownerPersistentId))
				m_mRecruitsByOwner[ownerPersistentId] = new array<string>;
			m_mRecruitsByOwner[ownerPersistentId].Insert(recruitId);
		}
		
		return true;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Called when a player group is created - triggers recruit respawning
	protected void OnPlayerGroupCreated(int playerId, int groupId, string playerName)
	{
		// Wait 3000ms to ensure the player is in a group before their recruits are asked back
		GetGame().GetCallqueue().CallLater(RespawnRecruitsDelayed, 3000, false, playerId, 0);
	}

	//! How many times RespawnRecruitsDelayed may re-arm itself before giving up, matching the cap on
	//! its sibling ladder OVT_SpawnLogic.CreateAndJoinGroupDelayed. At 500 ms a retry that is 5 s of
	//! grace after the initial 3 s wait.
	static const int RESPAWN_RECRUITS_MAX_RETRIES = 10;

	//------------------------------------------------------------------------------------------------
	//! Delayed recruit respawning, once the returning player actually has a group.
	//!
	//! THE RETRY LADDER IS CAPPED (task T4.5). Both re-arm branches below used to schedule another
	//! CallLater with no counter at all, so a player who never satisfied the condition produced a
	//! 500 ms timer that ran for the rest of the session. The cap is the same 10 its sibling
	//! CreateAndJoinGroupDelayed has used all along, and exhaustion is a WARNING naming the
	//! consequence rather than a silent stop.
	//!
	//! THE LEADER TEST BECAME A MEMBERSHIP TEST (decision D8) - see the guard below.
	//!
	//! \param[in] playerId The returning player.
	//! \param[in] retryCount How many times this ladder has already re-armed. Callers start at 0.
	protected void RespawnRecruitsDelayed(int playerId, int retryCount)
	{
		if (retryCount > RESPAWN_RECRUITS_MAX_RETRIES)
		{
			Print("[Overthrow] Gave up respawning recruits for player " + playerId + " after " + RESPAWN_RECRUITS_MAX_RETRIES + " retries - they never ended up in a group, so their recruits stay stored until they are given one again", LogLevel.WARNING);
			return;
		}

		// Get player's persistent ID
		string playerPersistentId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		if (playerPersistentId.IsEmpty())
			return;

		// Verify the player is actually in a group before respawning recruits
		SCR_PlayerController playerController = SCR_PlayerController.Cast(
			GetGame().GetPlayerManager().GetPlayerController(playerId)
		);

		if (!playerController)
			return;

		SCR_PlayerControllerGroupComponent groupController = SCR_PlayerControllerGroupComponent.Cast(
			playerController.FindComponent(SCR_PlayerControllerGroupComponent)
		);

		if (!groupController)
			return;

		int groupId = groupController.GetGroupID();
		if (groupId == -1)
		{
			// Retry after another delay. Counted against the SAME cap as the membership branch below:
			// the point of the cap is that no branch of this ladder may re-arm forever, and a player
			// who is still groupless 5 s after their group was created is a case for the own-group
			// reactor (OVT_PlayerGroupManagerComponent), not for an unbounded timer here.
			GetGame().GetCallqueue().CallLater(RespawnRecruitsDelayed, 500, false, playerId, retryCount + 1);
			return;
		}

		// Check the player is in the group their controller points at
		SCR_GroupsManagerComponent groupsManager = SCR_GroupsManagerComponent.GetInstance();
		if (!groupsManager)
			return;

		SCR_AIGroup group = groupsManager.FindGroup(groupId);
		if (!group)
			return;

		// MEMBERSHIP, NOT LEADERSHIP (decision D8). This used to demand group.GetLeaderID() == playerId,
		// which under the shared-group model is false for every player who reconnects into - or spawns
		// and then joins - somebody else's group: their whole recruit roster would then never be asked
		// back from storage, and the ladder below would re-arm every 500 ms forever waiting for a
		// leadership that is never coming.
		if (!group.IsPlayerInGroup(playerId))
		{
			// Retry after another delay
			GetGame().GetCallqueue().CallLater(RespawnRecruitsDelayed, 500, false, playerId, retryCount + 1);
			return;
		}

		// Now it's safe to respawn recruits
		RespawnPlayerRecruits(playerPersistentId);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Broadcast recruit creation to all clients (server only)
	void BroadcastRecruitCreated(string recruitId, string ownerPersistentId, string recruitName, vector position, IEntity recruitEntity = null)
	{
		// Get replication ID for entity mapping
		RplId recruitRplId = RplId.Invalid();
		if (recruitEntity)
		{
			RplComponent rplComponent = RplComponent.Cast(recruitEntity.FindComponent(RplComponent));
			if (rplComponent)
				recruitRplId = rplComponent.Id();
		}
		else
		{
			// Fallback to finding entity (for compatibility)
			IEntity foundEntity = FindRecruitEntity(recruitId);
			if (foundEntity)
			{
				RplComponent rplComponent = RplComponent.Cast(foundEntity.FindComponent(RplComponent));
				if (rplComponent)
					recruitRplId = rplComponent.Id();
			}
		}
		
		Rpc(RpcDo_RecruitCreated, recruitId, ownerPersistentId, recruitName, position, recruitRplId);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Broadcast recruit removal to all clients (server only) 
	void BroadcastRecruitRemoved(string recruitId, string ownerPersistentId)
	{
		Rpc(RpcDo_RecruitRemoved, recruitId, ownerPersistentId);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Broadcast recruit update to all clients (server only)
	void BroadcastRecruitUpdate(OVT_RecruitData recruit)
	{
		if (!recruit)
			return;
			
		// Get replication ID for entity mapping
		RplId recruitRplId = RplId.Invalid();
		IEntity recruitEntity = FindRecruitEntity(recruit.m_sRecruitId);
		if (recruitEntity)
		{
			RplComponent rplComponent = RplComponent.Cast(recruitEntity.FindComponent(RplComponent));
			if (rplComponent)
				recruitRplId = rplComponent.Id();
		}
		
		// Note: Using 8 parameters (limit), including replication ID
		Rpc(RpcDo_RecruitUpdated, recruit.m_sRecruitId, recruit.m_sOwnerPersistentId, recruit.m_sName, 
			recruit.m_iXP, recruit.m_iKills, recruit.m_iLevel, recruit.m_vLastKnownPosition, recruitRplId);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Broadcast one recruit's ACTIVE/INACTIVE state to all clients (server only).
	//!
	//! A DEDICATED RPC ON PURPOSE. The obvious alternative - one more parameter on
	//! RpcDo_RecruitUpdated - is not available: that handler is already at the 8-parameter limit noted
	//! at its call site, Rpc() has an untyped variadic prototype, and a wrong arity therefore compiles
	//! perfectly cleanly and then dies silently at the wire (the BUG-090 failure mode). Two scalars of
	//! proven types cost nothing and cannot be got wrong that way.
	//! \param[in] recruit The record whose state has just changed. Ignored when null.
	void BroadcastRecruitActiveState(OVT_RecruitData recruit)
	{
		if (!recruit)
			return;

		Rpc(RpcDo_RecruitActiveStateChanged, recruit.m_sRecruitId, recruit.m_bInactive);
	}

	//------------------------------------------------------------------------------------------------
	//! RPC method to handle a recruit's active/inactive state change on clients.
	//! \param[in] recruitId The recruit that changed.
	//! \param[in] inactive The new state: true means out of the owner's group, holding position.
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RecruitActiveStateChanged(string recruitId, bool inactive)
	{
		// Only process on clients, not the server (server already handled it)
		if (RplSession.Mode() != RplMode.Client)
			return;

		// Nothing is created here. A client that has never heard of this recruit has no record to
		// carry the flag onto, and will get the state with the record itself - through the JIP payload
		// or through RpcDo_RecruitCreated.
		OVT_RecruitData recruit = m_mRecruits.Get(recruitId);
		if (!recruit)
			return;

		recruit.m_bInactive = inactive;

		// Client-side listeners (the roster screen, the map layer) redraw from here. The server fires
		// the same invoker from SetRecruitInactive, so both sides see the change exactly once.
		m_OnRecruitActiveStateChanged.Invoke(recruit, inactive);
	}

	//------------------------------------------------------------------------------------------------
	//! RPC method to handle recruit creation on clients
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RecruitCreated(string recruitId, string ownerPersistentId, string recruitName, vector position, RplId recruitRplId)
	{			
		// Create recruit data on client
		OVT_RecruitData recruit = new OVT_RecruitData();
		recruit.m_sRecruitId = recruitId;
		recruit.m_sOwnerPersistentId = ownerPersistentId;
		recruit.m_sName = recruitName;
		recruit.m_vLastKnownPosition = position;
		recruit.m_bIsOnline = true; // Newly created recruits are online
		recruit.m_bIsTraining = false;
		recruit.m_iXP = 0;
		recruit.m_iLevel = 1;
		recruit.m_iKills = 0;
		recruit.m_fTrainingCompleteTime = 0;
		
		// Set hometown to nearest town on client
		OVT_TownManagerComponent townManager = OVT_Global.GetTowns();
		if (townManager)
		{
			OVT_TownData nearestTown = townManager.GetNearestTown(position);
			if (nearestTown)
			{
				recruit.m_iTownId = townManager.GetTownID(nearestTown);
			}
		}
		
		// Add to collections
		m_mRecruits[recruitId] = recruit;
		
		// Add replication ID mapping for client-side entity lookup
		if (recruitRplId != RplId.Invalid())
		{
			m_mRplIdToRecruit[recruitRplId] = recruitId;
		}
		
		if (!m_mRecruitsByOwner.Contains(ownerPersistentId))
			m_mRecruitsByOwner[ownerPersistentId] = new array<string>;
		m_mRecruitsByOwner[ownerPersistentId].Insert(recruitId);
		
		// Fire event
		m_OnRecruitAdded.Invoke(recruit);
	}
	
	//------------------------------------------------------------------------------------------------
	//! RPC method to handle recruit removal on clients
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RecruitRemoved(string recruitId, string ownerPersistentId)
	{
		// Only process on clients, not the server (server already handled it)
		if (RplSession.Mode() != RplMode.Client)
			return;
			
		// Get recruit data before removing
		OVT_RecruitData recruit = m_mRecruits.Get(recruitId);
		
		// Clean up replication ID mapping
		for (int i = m_mRplIdToRecruit.Count() - 1; i >= 0; i--)
		{
			if (m_mRplIdToRecruit.GetElement(i) == recruitId)
			{
				m_mRplIdToRecruit.RemoveElement(i);
				break;
			}
		}
		
		// Remove from collections
		m_mRecruits.Remove(recruitId);
		
		if (m_mRecruitsByOwner.Contains(ownerPersistentId))
		{
			array<string> ownerRecruits = m_mRecruitsByOwner[ownerPersistentId];
			int index = ownerRecruits.Find(recruitId);
			if (index != -1)
				ownerRecruits.Remove(index);
				
			// Clean up empty arrays
			if (ownerRecruits.Count() == 0)
				m_mRecruitsByOwner.Remove(ownerPersistentId);
		}
		
		// Fire event
		if (recruit)
			m_OnRecruitRemoved.Invoke(recruit);
	}
	
	//------------------------------------------------------------------------------------------------
	//! RPC method to handle recruit updates on clients
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_RecruitUpdated(string recruitId, string ownerPersistentId, string name, 
		int xp, int kills, int level, vector lastKnownPosition, RplId recruitRplId)
	{
		// Only process on clients, not the server (server already handled it)
		if (RplSession.Mode() != RplMode.Client)
			return;
			
		// Find existing recruit data
		OVT_RecruitData recruit = m_mRecruits.Get(recruitId);
		if (!recruit)
		{
			// Recruit doesn't exist on client, create it
			recruit = new OVT_RecruitData();
			recruit.m_sRecruitId = recruitId;
			recruit.m_sOwnerPersistentId = ownerPersistentId;
			
			// Add to collections
			m_mRecruits[recruitId] = recruit;
			
			if (!m_mRecruitsByOwner.Contains(ownerPersistentId))
				m_mRecruitsByOwner[ownerPersistentId] = new array<string>;
			m_mRecruitsByOwner[ownerPersistentId].Insert(recruitId);
		}
		
		// Add/update replication ID mapping for client-side entity lookup
		if (recruitRplId != RplId.Invalid())
		{
			m_mRplIdToRecruit[recruitRplId] = recruitId;
		}
		
		// Update recruit data (most important fields for status display)
		recruit.m_sName = name;
		recruit.m_iXP = xp;
		recruit.m_iKills = kills;
		recruit.m_iLevel = level;
		recruit.m_vLastKnownPosition = lastKnownPosition;
		
		// Determine online status from replication ID
		recruit.m_bIsOnline = (recruitRplId != RplId.Invalid());
	}
	
}
