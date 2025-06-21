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
			
		EntityID entityId = entity.GetID();
		if (!m_mEntityToRecruit.Contains(entityId))
			return null;
			
		string recruitId = m_mEntityToRecruit[entityId];
		return GetRecruit(recruitId);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Add a new recruit
	string AddRecruit(string ownerPersistentId, IEntity characterEntity, string name = "")
	{
		if (!CanRecruit(ownerPersistentId))
			return "";
			
		// Generate unique EPF-compatible ID
		int sequence = GetNextRecruitSequence(ownerPersistentId);
		string recruitId = GenerateRecruitId(ownerPersistentId, sequence);
		
		// Create recruit data
		OVT_RecruitData recruit = new OVT_RecruitData();
		recruit.m_sRecruitId = recruitId;
		recruit.m_sOwnerPersistentId = ownerPersistentId;
		recruit.m_sEntityPersistentId = recruitId; // Same as recruit ID for EPF
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
			// Change save type from MANUAL to INTERVAL_SHUTDOWN for recruits
			EPF_PersistenceComponentClass persistenceSettings = EPF_PersistenceComponentClass.Cast(persistenceComp.GetComponentData(characterEntity));
			if (persistenceSettings)
			{
				persistenceSettings.m_eSaveType = EPF_ESaveType.INTERVAL_SHUTDOWN;
				Print("[Overthrow] Set recruit save type to INTERVAL_SHUTDOWN: " + recruitId);
			}
			
			// Set unique persistent ID for EPF
			persistenceComp.SetPersistentId(recruitId);
			Print("[Overthrow] Enabled EPF persistence for recruit: " + recruitId);
		}
		
		m_OnRecruitAdded.Invoke(recruit);
		
		return recruitId;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Remove a recruit
	void RemoveRecruit(string recruitId)
	{
		OVT_RecruitData recruit = GetRecruit(recruitId);
		if (!recruit)
			return;
			
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
	protected string GenerateRecruitId(string ownerPersistentId, int sequence)
	{
		return string.Format("recruit_%1_%2", ownerPersistentId, sequence);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get next recruit sequence number for an owner
	protected int GetNextRecruitSequence(string ownerPersistentId)
	{
		if (!m_mRecruitsByOwner.Contains(ownerPersistentId))
			return 1;
			
		return m_mRecruitsByOwner[ownerPersistentId].Count() + 1;
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
		Print("[Overthrow] Player connected: " + playerPersistentId);
		
		// Cancel offline timer
		if (m_mOfflinePlayerTimers.Contains(playerPersistentId))
		{
			m_mOfflinePlayerTimers.Remove(playerPersistentId);
			Print("[Overthrow] Cancelled offline timer for player: " + playerPersistentId);
		}
		
		// Recruit respawning will be triggered when player group is created
	}
	
	//------------------------------------------------------------------------------------------------
	//! Handle player disconnection - start offline timer
	protected void OnPlayerDisconnected(string playerPersistentId, int playerId)
	{
		Print("[Overthrow] Player disconnected: " + playerPersistentId);
		
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
		Print("[Overthrow] Respawning " + recruitIds.Count() + " recruits for player: " + playerPersistentId);
		
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
		
		// Update entity mapping
		m_mEntityToRecruit[recruitEntity.GetID()] = recruitId;
		
		// Get recruit data
		OVT_RecruitData recruit = m_mRecruits[recruitId];
		if (!recruit)
		{
			Print("[Overthrow] WARNING: No recruit data found for: " + recruitId);
			return;
		}
		
		// Restore the character's name to the identity component (EPF doesn't save this)
		RestoreCharacterIdentity(recruitEntity, recruit.m_sName);
		
		// Add to player's group
		AddRecruitToPlayerGroup(playerPersistentId, recruitEntity);
		
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
			
			// Remove entity mapping
			m_mEntityToRecruit.Remove(recruitEntity.GetID());
			
			// Remove from world
			SCR_EntityHelper.DeleteEntityAndChildren(recruitEntity);
			
			Print("[Overthrow] Despawned recruit: " + recruitId);
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Find recruit entity by persistent ID
	protected IEntity FindRecruitEntity(string recruitId)
	{
		// Search through all entities with EPF persistence
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
		
		// Add as AI agent to player's group
		SCR_ChimeraCharacter recruitCharacter = SCR_ChimeraCharacter.Cast(recruitEntity);
		if (!recruitCharacter)
		{
			Print("[Overthrow] Failed to cast recruit entity to SCR_ChimeraCharacter");
			return;
		}
		
		groupController.RequestAddAIAgent(recruitCharacter, playerId);
		Print("[Overthrow] Added recruit to player group: " + playerId);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Called when a player group is created - triggers recruit respawning
	protected void OnPlayerGroupCreated(int playerId, int groupId, string playerName)
	{
		Print("[Overthrow] Player group created, respawning recruits for player: " + playerId);
		
		// Get player's persistent ID
		string playerPersistentId = OVT_Global.GetPlayers().GetPersistentIDFromPlayerID(playerId);
		if (playerPersistentId.IsEmpty())
			return;
			
		// Now it's safe to respawn recruits
		RespawnPlayerRecruits(playerPersistentId);
	}
	
}