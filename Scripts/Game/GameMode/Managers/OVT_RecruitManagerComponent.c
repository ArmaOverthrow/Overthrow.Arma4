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
	
	//! Prefab to use for spawning recruits (should have EPF persistence enabled)
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
			respawnSystem.m_OnPlayerGroupCreated.Insert(OnPlayerGroupCreated);
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
	//! Add a new recruit
	string AddRecruit(string ownerPersistentId, IEntity characterEntity, string name = "")
	{
		if (!CanRecruit(ownerPersistentId))
			return "";
			
		// Generate unique EPF-compatible ID
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
		
		// Enable EPF persistence for recruit (disabled for regular civilians)
		EPF_PersistenceComponent persistenceComp = EPF_PersistenceComponent.Cast(
			characterEntity.FindComponent(EPF_PersistenceComponent)
		);
		
		if (!persistenceComp)
		{
			Print("[Overthrow] WARNING: Character entity missing EPF_PersistenceComponent! Recruit persistence may not work correctly.");
		}
		else
		{
			// Set unique persistent ID for EPF
			persistenceComp.SetPersistentId(recruitId);
			Print("[Overthrow] Enabled EPF persistence for recruit: " + recruitId);
		}
		
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
	//! Generate unique recruit ID compatible with EPF persistence
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
		
		// Note: BroadcastRecruitCreated is already called in AddRecruit method
		// No need to broadcast again here to avoid duplicates
		
		// Add to player's group using the proper API
		SCR_PlayerController playerController = SCR_PlayerController.Cast(GetGame().GetPlayerManager().GetPlayerController(playerId));
		if (playerController)
		{
			SCR_PlayerControllerGroupComponent groupController = SCR_PlayerControllerGroupComponent.Cast(playerController.FindComponent(SCR_PlayerControllerGroupComponent));
			if (groupController)
			{
				// Ensure the recruit is not controlled by any player before adding as AI agent
				if (GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(civilian) == 0)
				{
					groupController.RequestAddAIAgent(civilian, playerId);
				}
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
	//! Restore character identity name after loading from EPF
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
		
		// Save all recruit states
		SavePlayerRecruits(playerPersistentId);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Respawn recruits for a player using EPF
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
			
			// Load recruit character via EPF
			EPF_PersistenceManager persistenceManager = EPF_PersistenceManager.GetInstance();
			if (persistenceManager)
			{
				Print("[Overthrow] Loading recruit from EPF: " + recruitId);
				// Create callback context with player and recruit info
				ref Tuple2<string, string> context = new Tuple2<string, string>(playerPersistentId, recruitId);
				EDF_DataCallbackSingle<IEntity> callback(this, "OnRecruitLoaded", context);
				
				// Use EPF_PersistentWorldEntityLoader to load the recruit
				EPF_PersistentWorldEntityLoader.LoadAsync(EPF_CharacterSaveData, recruitId, callback);
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Callback when recruit is loaded from EPF
	void OnRecruitLoaded(IEntity recruitEntity, Managed context)
	{
		if (!recruitEntity)
		{
			Print("[Overthrow] Failed to load recruit entity from EPF");
			return;
		}
		
		Tuple2<string, string> recruitContext = Tuple2<string, string>.Cast(context);
		if (!recruitContext)
		{
			Print("[Overthrow] Invalid context in OnRecruitLoaded");
			return;
		}
		
		string playerPersistentId = recruitContext.param1;
		string recruitId = recruitContext.param2;
		
		Print("[Overthrow] Successfully loaded recruit: " + recruitId);
		
		// Activate AI for the loaded recruit
		AIControlComponent aiControl = AIControlComponent.Cast(recruitEntity.FindComponent(AIControlComponent));
		if (aiControl)
		{
			aiControl.ActivateAI();
			Print("[Overthrow] Activated AI for recruit: " + recruitId);
			
			// Ensure the AI agent has a proper group
			AIAgent agent = aiControl.GetAIAgent();
			if (agent)
			{
				SCR_AIGroup aiGroup = SCR_AIGroup.Cast(agent.GetParentGroup());
				if (!aiGroup)
				{
					Print("[Overthrow] Recruit has no AI group, will be handled by player group assignment");
				}
			}
		}
		else
		{
			Print("[Overthrow] WARNING: No AIControlComponent found on recruit: " + recruitId);
		}
		
		//Enable EPF Saving
		// Enable EPF persistence for recruit (disabled for regular civilians)
		EPF_PersistenceComponent persistenceComp = EPF_PersistenceComponent.Cast(
			recruitEntity.FindComponent(EPF_PersistenceComponent)
		);
		
		if (!persistenceComp)
		{
			Print("[Overthrow] WARNING: Character entity missing EPF_PersistenceComponent! Recruit persistence may not work correctly.");
		}
		
		// Update entity mapping
		m_mEntityToRecruit[recruitEntity.GetID()] = recruitId;
		
		// Update replication ID mapping for client access
		RplComponent rplComponent = RplComponent.Cast(recruitEntity.FindComponent(RplComponent));
		if (rplComponent)
		{
			m_mRplIdToRecruit[rplComponent.Id()] = recruitId;
		}
		
		// Get recruit data
		OVT_RecruitData recruit = m_mRecruits[recruitId];
		if (!recruit)
		{
			Print("[Overthrow] WARNING: No recruit data found for: " + recruitId);
			return;
		}
		
		// Restore the character's name to the identity component (EPF doesn't save this)
		RestoreCharacterIdentity(recruitEntity, recruit.m_sName);
		
		// Mark recruit as online
		recruit.m_bIsOnline = true;
		
		// Set recruit faction to match player faction before adding to group
		SetRecruitFaction(playerPersistentId, recruitEntity);
		
		// Add to player's group
		AddRecruitToPlayerGroup(playerPersistentId, recruitEntity);
		
		// Broadcast updated recruit status to all clients
		BroadcastRecruitUpdate(recruit);
		
		Print("[Overthrow] Recruit " + recruitId + " respawned and added to group");
	}
	
	//------------------------------------------------------------------------------------------------
	//! Save all recruits for a player via EPF
	protected void SavePlayerRecruits(string playerPersistentId)
	{
		if (!m_mRecruitsByOwner.Contains(playerPersistentId))
			return;
			
		array<string> recruitIds = m_mRecruitsByOwner[playerPersistentId];
		Print("[Overthrow] Saving " + recruitIds.Count() + " recruits for player: " + playerPersistentId);
		
		foreach (string recruitId : recruitIds)
		{
			OVT_RecruitData recruit = m_mRecruits[recruitId];
			if (!recruit)
				continue;
				
			// Find character entity
			IEntity recruitEntity = FindRecruitEntity(recruitId);
			if (!recruitEntity)
				continue;
				
			// Force save via EPF
			EPF_PersistenceComponent persistence = EPF_PersistenceComponent.Cast(
				recruitEntity.FindComponent(EPF_PersistenceComponent)
			);
			
			if (persistence)
			{
				persistence.Save();
				Print("[Overthrow] Saved recruit via EPF: " + recruitId);
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
	//! Despawn recruits for an offline player but keep database records
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
				
			// Save current state before despawn
			EPF_PersistenceComponent persistence = EPF_PersistenceComponent.Cast(
				recruitEntity.FindComponent(EPF_PersistenceComponent)
			);
			
			if (persistence)
			{
				persistence.Save();
				Print("[Overthrow] Saved recruit before despawn: " + recruitId);
			}
			
			// Mark recruit as offline
			recruit.m_bIsOnline = false;
			
			// Remove entity mapping
			m_mEntityToRecruit.Remove(recruitEntity.GetID());
			
			// Remove from world
			SCR_EntityHelper.DeleteEntityAndChildren(recruitEntity);
			
			// Broadcast updated recruit status to all clients
			BroadcastRecruitUpdate(recruit);
			
			Print("[Overthrow] Despawned recruit: " + recruitId);
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
			recruitFactionComp.SetAffiliatedFaction(playerFaction);
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
		GetGame().GetCallqueue().CallLater(RespawnRecruitsDelayed, 2000, false, playerId);
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