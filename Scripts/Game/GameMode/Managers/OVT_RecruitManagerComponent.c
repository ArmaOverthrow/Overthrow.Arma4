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
		
		SetEventMask(owner, EntityEvent.INIT);
	}
	
	//------------------------------------------------------------------------------------------------
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		
		if (!GetGame().InPlayMode())
			return;
			
		// Subscribe to character death events
		SCR_BaseGameMode gameMode = SCR_BaseGameMode.Cast(GetGame().GetGameMode());
		if (gameMode)
		{
			gameMode.GetOnPlayerKilled().Insert(OnCharacterKilled);
		}
		
		// Subscribe to AI kills for XP tracking
		OVT_OccupyingFactionManager occupyingFaction = OVT_Global.GetOccupyingFaction();
		if (occupyingFaction)
		{
			occupyingFaction.m_OnAIKilled.Insert(OnAIKilled);
		}
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
			if (recruit && !recruit.m_bIsDead)
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
			
		// Generate unique ID
		string recruitId = GenerateRecruitId();
		
		// Create recruit data
		OVT_RecruitData recruit = new OVT_RecruitData();
		recruit.m_sRecruitId = recruitId;
		recruit.m_sOwnerPersistentId = ownerPersistentId;
		recruit.m_sName = name;
		
		if (name.IsEmpty())
			recruit.m_sName = GenerateRecruitName();
			
		// Store position
		recruit.m_vLastKnownPosition = characterEntity.GetOrigin();
		
		// Add to collections
		m_mRecruits[recruitId] = recruit;
		
		if (!m_mRecruitsByOwner.Contains(ownerPersistentId))
			m_mRecruitsByOwner[ownerPersistentId] = new array<string>;
			
		m_mRecruitsByOwner[ownerPersistentId].Insert(recruitId);
		
		// Map entity to recruit
		m_mEntityToRecruit[characterEntity.GetID()] = recruitId;
		
		// Add persistence component to character
		if (!characterEntity.FindComponent(EPF_PersistenceComponent))
		{
			EPF_PersistenceComponent persistenceComp = EPF_PersistenceComponent.Cast(
				characterEntity.FindComponent(EPF_PersistenceComponent)
			);
			if (!persistenceComp)
			{
				// TODO: Add persistence component dynamically if possible
			}
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
	//! Handle character death
	protected void OnCharacterKilled(notnull SCR_InstigatorContextData instigatorContextData)
	{
		IEntity victimEntity = instigatorContextData.GetVictimEntity();
		if (!victimEntity)
			return;
			
		// Check if the killed entity is a recruit
		OVT_RecruitData recruit = GetRecruitFromEntity(victimEntity);
		if (!recruit)
			return;
			
		// Mark as dead
		recruit.m_bIsDead = true;
		recruit.m_vLastKnownPosition = victimEntity.GetOrigin();
		
		// Remove entity mapping
		m_mEntityToRecruit.Remove(victimEntity.GetID());
		
		// Notify owner
		OVT_PlayerData ownerData = OVT_Global.GetPlayers().GetPlayer(recruit.m_sOwnerPersistentId);
		if (ownerData && !ownerData.IsOffline())
		{
			OVT_Global.GetNotify().SendTextNotification("RecruitDied", ownerData.id, recruit.m_sName);
		}
		
		// Don't process further - no XP for recruit vs recruit kills
	}
	
	//------------------------------------------------------------------------------------------------
	//! Handle AI kills by recruits for XP rewards
	protected void OnAIKilled(IEntity victim, IEntity instigator)
	{
		if (!instigator || !victim)
			return;
			
		// Check if killer is a recruit
		OVT_RecruitData killerRecruit = GetRecruitFromEntity(instigator);
		if (!killerRecruit)
			return;
			
		// Make sure victim is not a recruit or civilian
		OVT_RecruitData victimRecruit = GetRecruitFromEntity(victim);
		if (victimRecruit)
			return; // Don't reward for killing other recruits
			
		// Check faction to ensure it's an enemy
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
	//! Generate unique recruit ID
	protected string GenerateRecruitId()
	{
		return System.GetMachineName() + "_" + System.GetTickCount().ToString();
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
}