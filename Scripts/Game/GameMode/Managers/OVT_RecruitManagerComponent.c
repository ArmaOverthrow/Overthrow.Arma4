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
	
	//! Prefab to use for spawning recruit bodies (both new recruits and restored ones)
	[Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Recruit Character Prefab", params: "et")]
	ResourceName m_sRecruitPrefab;
	
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

	//! Event fired when a recruit is added
	ref ScriptInvoker m_OnRecruitAdded = new ScriptInvoker();
	
	//! Event fired when a recruit is removed
	ref ScriptInvoker m_OnRecruitRemoved = new ScriptInvoker();
	
	//! Event fired when a recruit gains XP
	ref ScriptInvoker m_OnRecruitXPGained = new ScriptInvoker();
	
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
	array<ref OVT_RecruitData> GetPlayerRecruitsInRadius(string playerPersistentId, vector position, float radius)
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
	array<IEntity> GetPlayerRecruitEntitiesInRadius(string playerPersistentId, vector position, float radius)
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

		// The recruit id identifies the RECORD. The BODY has its own identity, handed out by the
		// persistence system, and remembering it here is what lets this exact character - with whatever
		// it is carrying - be spawned back later instead of a fresh one. A civilian standing in a town is
		// already tracked (Character_Base.et carries the native Persistence component), so this normally
		// resolves immediately; when it does not, the record simply keeps an empty id and falls back to a
		// fresh body.
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
			
		// Only award XP for killing occupying faction members
		string factionKey = faction.GetFactionKey();
		if (factionKey != "US" && factionKey != "USSR")
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
	//! Server-side method to recruit a civilian
	void RecruitCivilian(SCR_ChimeraCharacter civilian, int playerId)
	{
		if (!civilian) return;
		
		OVT_PlayerManagerComponent players = OVT_Global.GetPlayers();
		if (!players) return;
		
		string persId = players.GetPersistentIDFromPlayerID(playerId);
		if (persId.IsEmpty()) return;
		
		// Double-check recruit limit on server
		if (!CanRecruit(persId)) return;
		
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
		
		// Set recruit faction to match player faction
		SetRecruitFaction(persId, civilian);
		
		// Note: BroadcastRecruitCreated is already called in AddRecruit method
		// No need to broadcast again here to avoid duplicates
		
		// Add to player's group using the proper API
		
		// Force the server's authoritative group manager to do it directly
		SCR_GroupsManagerComponent groupsManager = SCR_GroupsManagerComponent.GetInstance();
		if (groupsManager)
		{
    		// Find the player's existing native engine group
    		SCR_AIGroup playerGroup = groupsManager.GetPlayerGroup(playerId);
    		if (playerGroup)
    		{
        	// Violently force the AI agent directly into the group array on the server
        	playerGroup.AddAgent(civilian.GetAIAgent());
    		}
		}		
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
	//! WHEN A RECRUIT BODY EXISTS IS AN OWNER-PRESENCE QUESTION, NOT A CAMPAIGN-START ONE. A recruit's
	//! body is saved and released OFFLINE_DESPAWN_TIME after its owner leaves (DespawnPlayerRecruits)
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

			// Check if recruit is already spawned in world
			IEntity existingEntity = FindRecruitEntity(recruitId);
			if (existingEntity)
			{
				Print("[Overthrow] Recruit " + recruitId + " already in world, adding to group");
				AddRecruitToPlayerGroup(playerPersistentId, existingEntity);
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

		// The owner may have left again while the request was in flight. Putting a body in the world for
		// somebody who is not there is precisely what the offline despawn undoes, so undo it now:
		// save-and-release keeps the character and its gear for the next time they return.
		if (!IsPlayerOnline(playerPersistentId))
		{
			Print("[Overthrow] Owner of recruit " + recruitId + " left before their body arrived - releasing it again");
			ReleaseRecruitBody(recruit, bodyEntity);
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
	//! Writes a recruit body's record, releases tracking WITHOUT dropping that record, and removes the
	//! body from the world.
	//!
	//! THE POINT OF THE ORDER. Save() writes the character - inventory included - into storage; only
	//! then is its id read back and remembered; only then is tracking released with the data kept
	//! (StopTracking(entity, removeData: false)). Deleting a tracked entity without that last step
	//! drops its stored record too, and the recruit would come back in a fresh civilian shirt.
	//!
	//! This is vanilla's own idiom for a character that is about to be deleted but must survive -
	//! SCR_SpawnLogic.OnPlayerEntityCleanup_S (:214-216) does exactly this to a disconnecting player's
	//! character, and SCR_SpawnLogic.c:107-108 to their controller. OVT_VehicleManagerComponent uses it
	//! for locked vehicles.
	//! \param[in] recruit The record that must outlive the body.
	//! \param[in] recruitEntity The body to release and delete.
	protected void ReleaseRecruitBody(notnull OVT_RecruitData recruit, notnull IEntity recruitEntity)
	{
		// Remember where the body was - it is spawned back here when the owner returns
		recruit.m_vLastKnownPosition = recruitEntity.GetOrigin();

		// The explicit Save() is the materialisation, so the capture below is a pure lookup
		OVT_PersistenceTracking.Save(recruitEntity);
		CaptureRecruitBodyId(recruit, recruitEntity, false);
		OVT_PersistenceTracking.Untrack(recruitEntity, true);

		recruit.m_bIsOnline = false;

		m_mEntityToRecruit.Remove(recruitEntity.GetID());

		SCR_EntityHelper.DeleteEntityAndChildren(recruitEntity);
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
	//! Links a body to its recruit record and puts it back under the owner's command.
	//!
	//! Used by BOTH routes - a body restored from storage and a fresh one off the prefab - so everything
	//! here has to be safe to re-apply to a character that already has the right values.
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

		// Add to player's group
		AddRecruitToPlayerGroup(playerPersistentId, recruitEntity);

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
	//! never goes through ReleaseRecruitBody(), so this hook is the only place its id is written down
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
	//! Takes an offline player's recruit bodies out of the world without losing them.
	//!
	//! DESPAWN IS SAVE-AND-RELEASE, NOT DELETE. Each body is written to storage and released from
	//! tracking with its record KEPT (ReleaseRecruitBody), and the recruit remembers which character it
	//! was. When the owner comes back, RespawnPlayerRecruits() asks for that character again and the
	//! recruit returns carrying exactly what it was carrying - across a reconnect and across a
	//! quit/continue alike.
	//! \param[in] playerPersistentId The player who has been offline long enough.
	protected void DespawnPlayerRecruits(string playerPersistentId)
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

			// Writes the body, remembers its id and position, releases tracking keeping the data, deletes
			ReleaseRecruitBody(recruit, recruitEntity);

			// Broadcast updated recruit status to all clients
			BroadcastRecruitUpdate(recruit);

			Print("[Overthrow] Despawned recruit: " + recruitId + " (stored body " + recruit.m_sBodyPersistenceId + ")");
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Find recruit entity by persistent ID
	IEntity FindRecruitEntity(string recruitId)
	{
		// On server, use entity ID mapping
		if (Replication.IsServer())
		{
			foreach (EntityID entityId, string mappedRecruitId : m_mEntityToRecruit)
			{
				if (mappedRecruitId == recruitId)
				{
					IEntity entity = GetGame().GetWorld().FindEntityByID(entityId);
					if (entity)
						return entity;
					else
						m_mEntityToRecruit.Remove(entityId); // Clean up stale mapping
				}
			}
			return null;
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
		// Find player by persistent ID
		OVT_PlayerManagerComponent playerManager = OVT_Global.GetPlayers();
		if (!playerManager)
			return;
			
		int playerId = playerManager.GetPlayerIDFromPersistentID(playerPersistentId);
		if (playerId == 0)
		{
			Print("[Overthrow] Player not online for faction setting: " + playerPersistentId);
			return;
		}
		
		// Get player faction
		SCR_FactionManager factionManager = SCR_FactionManager.Cast(GetGame().GetFactionManager());
		if (!factionManager)
		{
			Print("[Overthrow] No faction manager available");
			return;
		}
		
		Faction playerFaction = factionManager.GetPlayerFaction(playerId);
		if (!playerFaction)
		{
			Print("[Overthrow] No player faction found for ID: " + playerId);
			return;
		}
		
		// Get recruit's current faction
		SCR_CharacterFactionAffiliationComponent recruitFactionComp = SCR_CharacterFactionAffiliationComponent.Cast(
			recruitEntity.FindComponent(SCR_CharacterFactionAffiliationComponent)
		);
		
		if (!recruitFactionComp)
		{
			Print("[Overthrow] No character faction component found on recruit");
			return;
		}
		
		Faction currentFaction = recruitFactionComp.GetAffiliatedFaction();
		
		// Set recruit faction to match player
		if (currentFaction != playerFaction)
		{
			recruitFactionComp.SetAffiliatedFactionByKey(OVT_Global.GetConfig().GetPlayerFaction().GetFactionKey());
		}
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
			return;
		
		SCR_GroupsManagerComponent groupsManager = SCR_GroupsManagerComponent.GetInstance();
		if (!groupsManager)
			return;
		
		SCR_AIGroup group = groupsManager.FindGroup(groupId);
		if (!group)
			return;
		
		if (group.GetLeaderID() != playerId)
			return;
		
		// Add as AI agent to player's group via RPC to client
		SCR_ChimeraCharacter recruitCharacter = SCR_ChimeraCharacter.Cast(recruitEntity);
		if (!recruitCharacter)
		{
			Print("[Overthrow] Failed to cast recruit entity to SCR_ChimeraCharacter");
			return;
		}
		
		// Use RPC to client to trigger proper group join notifications
		RplComponent rplComponent = RplComponent.Cast(recruitEntity.FindComponent(RplComponent));
		if (rplComponent)
		{
			int localPlayerId = SCR_PlayerController.GetLocalPlayerId();
			if (localPlayerId == playerId)
			{
				// Host/server - execute immediately
				RpcDo_AddRecruitToGroup(rplComponent.Id(), playerId);
			}
			else
			{
				// Client - delay RPC to allow entity replication
				GetGame().GetCallqueue().CallLater(DelayedRpcAddRecruitToGroup, 6000, false, rplComponent.Id(), playerId);
			}
		}
		else
		{
			Print("[Overthrow] No RplComponent found on recruit entity for group addition");
		}
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
		// Wait 2000ms to ensure player is in a group and the leader
		GetGame().GetCallqueue().CallLater(RespawnRecruitsDelayed, 3000, false, playerId);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Delayed recruit respawning to ensure player is group leader
	protected void RespawnRecruitsDelayed(int playerId)
	{
		// Get player's persistent ID
		string playerPersistentId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		if (playerPersistentId.IsEmpty())
			return;
			
		// Verify player is actually a group leader before respawning recruits
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
			// Retry after another delay
			GetGame().GetCallqueue().CallLater(RespawnRecruitsDelayed, 500, false, playerId);
			return;
		}
		
		// Check if player is group leader
		SCR_GroupsManagerComponent groupsManager = SCR_GroupsManagerComponent.GetInstance();
		if (!groupsManager)
			return;
		
		SCR_AIGroup group = groupsManager.FindGroup(groupId);
		if (!group)
			return;
		
		if (group.GetLeaderID() != playerId)
		{
			// Retry after another delay
			GetGame().GetCallqueue().CallLater(RespawnRecruitsDelayed, 500, false, playerId);
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
	
	//------------------------------------------------------------------------------------------------
	//! Delayed RPC call to allow entity replication to clients
	protected void DelayedRpcAddRecruitToGroup(RplId recruitRplId, int playerId)
	{
		Rpc(RpcDo_AddRecruitToGroup, recruitRplId, playerId);
	}
	
	//------------------------------------------------------------------------------------------------
	//! RPC method to request client add AI to group
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	protected void RpcDo_AddRecruitToGroup(RplId recruitRplId, int targetPlayerId)
	{			
		// Only process if this is the target player
		int localPlayerId = SCR_PlayerController.GetLocalPlayerId();
		if (localPlayerId != targetPlayerId)
			return;
		
		RpcDo_AddRecruitToGroupWithRetry(recruitRplId, targetPlayerId, 0);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Retry logic for adding recruit to group on client
	protected void RpcDo_AddRecruitToGroupWithRetry(RplId recruitRplId, int targetPlayerId, int attemptCount)
	{
		// Find the recruit entity on client
		RplComponent rplComponent = RplComponent.Cast(Replication.FindItem(recruitRplId));
		if (!rplComponent)
		{
			// Retry up to 10 times (20 seconds total)
			if (attemptCount < 10)
			{
				GetGame().GetCallqueue().CallLater(RpcDo_AddRecruitToGroupWithRetry, 2000, false, recruitRplId, targetPlayerId, attemptCount + 1);
				return;
			}
			else
			{
				Print("[Overthrow] Failed to find recruit entity after 10 retry attempts");
				return;
			}
		}
			
		IEntity recruitEntity = rplComponent.GetEntity();
		if (!recruitEntity)
		{
			// Retry if entity not available yet
			if (attemptCount < 10)
			{
				GetGame().GetCallqueue().CallLater(RpcDo_AddRecruitToGroupWithRetry, 2000, false, recruitRplId, targetPlayerId, attemptCount + 1);
				return;
			}
			else
			{
				Print("[Overthrow] Failed to get recruit entity after 10 retry attempts");
				return;
			}
		}
		
		//Make sure AI is activated
		AIControlComponent aiControl = AIControlComponent.Cast(recruitEntity.FindComponent(AIControlComponent));
		if (aiControl)
		{
			aiControl.ActivateAI();
		}
		
		// Get local player controller and add AI to group
		int localPlayerId = SCR_PlayerController.GetLocalPlayerId();
		SCR_PlayerController playerController = SCR_PlayerController.Cast(
			GetGame().GetPlayerManager().GetPlayerController(localPlayerId)
		);
		
		if (!playerController)
			return;
			
		SCR_PlayerControllerGroupComponent groupController = SCR_PlayerControllerGroupComponent.Cast(
			playerController.FindComponent(SCR_PlayerControllerGroupComponent)
		);
		
		if (groupController)
		{
			groupController.RequestAddAIAgent(SCR_ChimeraCharacter.Cast(recruitEntity), localPlayerId);
		}
	}
	
}
