//------------------------------------------------------------------------------------------------
//! Handles the spawning and respawning logic for players within the Overthrow game mode.
//! Extends the base EPF respawn system to integrate with Overthrow-specific player data and spawning rules.
//! Should be attached to the OVT_OverthrowGameMode entity.
[ComponentEditorProps(icon: HYBRID_COMPONENT_ICON)]
class OVT_RespawnSystemComponent : EPF_BaseSpawnLogic
{	
	protected OVT_OverthrowGameMode m_Overthrow;
	protected ref array<IEntity> m_FoundBases = {};
	
	//! Event fired when a player group is created
	//! Parameters: playerId, groupId, playerName
	ref ScriptInvoker m_OnPlayerGroupCreated = new ScriptInvoker();
	
	[Attribute(defvalue: "{3A99A99836F6B3DC}Prefabs/Characters/Factions/INDFOR/FIA/Character_Player.et")]
	ResourceName m_rDefaultPrefab;
	
	//------------------------------------------------------------------------------------------------
	//! Gets the default player character prefab resource name.
	//! \param playerId ID of the player for whom the prefab is requested (unused).
	//! \param characterPersistenceId Persistence ID of the character (unused).
	//! \return The resource name of the default player character prefab.
	override protected ResourceName GetCreationPrefab(int playerId, string characterPersistenceId)
	{
		return m_rDefaultPrefab;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Called when the player's unique ID becomes available. Prepares the player in the game mode.
	//! Retries if the game mode is not initialized or the UID is not yet available.
	//! \param playerId ID of the player whose UID is now available.
	protected override void DoSpawn_S(int playerId)
	{
		OVT_OverthrowGameMode mode = OVT_OverthrowGameMode.Cast(GetGame().GetGameMode());
		if(!mode.IsInitialized())
		{
			//Game has not started yet
			OnPlayerRegisterFailed(playerId);
			return;
		}
		
		string playerUid = EPF_Utils.GetPlayerUID(playerId);
		if (!playerUid)
		{
			//Still no player UID?
			Print("WARNING: Early OnUidAvailable detected. Retrying...", LogLevel.WARNING);
			OnPlayerRegisterFailed(playerId);
			return;
		}
		
		mode.PreparePlayer(playerId, playerUid);
		
		super.DoSpawn_S(playerId);		
	}
	
	//------------------------------------------------------------------------------------------------
	//! Handles failed player registration attempts by scheduling a retry.
	//! \param playerId ID of the player whose registration failed.
	void OnPlayerRegisterFailed(int playerId)
	{
		int delay = Math.RandomFloat(900, 1100);
		GetGame().GetCallqueue().CallLater(OnPlayerRegistered_S, delay, false, playerId);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Determines the spawn position and orientation for a player character based on their home location.
	//! \param[in] playerId ID of the player being spawned (unused).
	//! \param[in] characterPersistenceId Persistence ID used to retrieve player data.
	//! \param[out] position The calculated spawn position vector.
	//! \param[out] yawPitchRoll The calculated spawn orientation vector (defaults to "0 0 0").
	protected override void GetCreationPosition(int playerId, string characterPersistenceId, out vector position, out vector yawPitchRoll)
	{
		OVT_PlayerData player = OVT_PlayerData.Get(characterPersistenceId);
		if(player)
		{
			vector homePos = player.home;
			
			// Check if the home location is safe to spawn at
			if(IsSpawnLocationSafe(homePos))
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
	
	void CreateAndJoinGroup(int playerId)
	{		
		
		SCR_PlayerController playerController = SCR_PlayerController.Cast(m_pPlayerManager.GetPlayerController(playerId));
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
	
	//! Delayed group creation with retry mechanism
	void CreateAndJoinGroupDelayed(int playerId, int retryCount)
	{
		if (retryCount > 10)
		{
			Print("[Overthrow] Failed to create group for player " + playerId + " after 10 retries", LogLevel.ERROR);
			return;
		}
		
		SCR_PlayerController playerController = SCR_PlayerController.Cast(m_pPlayerManager.GetPlayerController(playerId));
		if (!playerController)
		{
			// Still not ready, retry again
			GetGame().GetCallqueue().CallLater(CreateAndJoinGroupDelayed, 500, false, playerId, retryCount + 1);
			return;
		}
		
		// Player controller is ready, proceed with group creation
		CreateAndJoinGroup(playerId);
	}
		
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
		
		// Force client-side faction mapping via RPC as backup
		if (Replication.IsServer())
		{
			SCR_FactionManager factionManager = SCR_FactionManager.Cast(mgr);
			if (factionManager)
			{
				int factionIndex = factionManager.GetFactionIndex(playerFaction);
				RPC_ForceClientFactionMapping(playerId, factionIndex);
				Rpc(RPC_ForceClientFactionMapping, playerId, factionIndex);
			}
		}
	}
	
	//! Force client to add player faction mapping when normal replication fails
	[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
	void RPC_ForceClientFactionMapping(int playerId, int factionIndex)
	{
		if (Replication.IsServer()) return; // Only execute on clients
		
		OVT_OverthrowFactionManager factionManager = OVT_OverthrowFactionManager.Cast(GetGame().GetFactionManager());
		if (!factionManager) return;
		
		// Use our extended faction manager to force the mapping
		factionManager.ForceClientFactionMapping(playerId, factionIndex);
	}
	
	override void OnCharacterLoadComplete(int playerId, EPF_EntitySaveData saveData, EPF_PersistenceComponent persistenceComponent)
	{			
		// Group creation now happens in HandoverToPlayer
		super.OnCharacterLoadComplete(playerId, saveData, persistenceComponent);	
	}
	
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
	
	//! Filter for entities that might have base controllers
	protected bool FilterBaseEntities(IEntity entity)
	{
		return entity.FindComponent(OVT_BaseControllerComponent) != null;
	}
	
	//! Check if entity has base controller and add to found bases
	protected bool CheckForBaseController(IEntity entity)
	{
		if(entity.FindComponent(OVT_BaseControllerComponent))
		{
			m_FoundBases.Insert(entity);
		}
		return true;
	}
	
	//! Find a safe spawn location when home is compromised
	protected vector FindSafeSpawnLocation(int playerId, string characterPersistenceId)
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
					if(realEstate)
						realEstate.SetHomePos(playerId, safeSpawn);
					return safeSpawn;
				}
			}
		}
		
		// Last resort: Use default spawn location
		vector fallbackPos = "5000 0 5000"; // Default fallback position
		if(realEstate)
			realEstate.SetHomePos(playerId, fallbackPos);
		return fallbackPos;
	}
	
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
	//! Override HandoverToPlayer to ensure group creation happens after player takes control
	protected override void HandoverToPlayer(int playerId, IEntity character)
	{
		super.HandoverToPlayer(playerId, character);
		
		// Schedule group creation after handover completes
		GetGame().GetCallqueue().CallLater(CreateAndJoinGroup, 3000, false, playerId);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Called after a player character has been created and spawned into the world.
	//! Initializes the character's inventory with the default civilian loadout and difficulty-specific starting items.
	//! Marks the player's first spawn as complete.
	//! \param playerId ID of the player whose character was created.
	//! \param characterPersistenceId Persistence ID of the created character.
	//! \param character The newly created character entity.
	protected override void OnCharacterCreated(int playerId, string characterPersistenceId, IEntity character)
	{
		super.OnCharacterCreated(playerId, characterPersistenceId, character);
		
		InventoryStorageManagerComponent storageManager = EPF_Component<InventoryStorageManagerComponent>.Find(character);
		
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
			
			player.firstSpawn = false;
		}				
	}
	
	//------------------------------------------------------------------------------------------------
	//! Spawns a default item based on a loadout slot definition and attempts to place it in the character's inventory.
	//! Handles random selection from choices within the loadout slot and spawning of nested items.
	//! \param storageManager The inventory manager component of the character.
	//! \param loadoutItem The loadout slot definition containing item choices and stored items.
	//! \return The spawned entity representing the primary item from the slot, or null if spawning failed.
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
	//! Called on the server when a player is killed.
	//! Charges the player the respawn cost via the economy system.
	//! \param playerId ID of the player who was killed.
	//! \param playerEntity Entity of the killed player.
	//! \param killerEntity Entity that killed the player (can be null).
	//! \param killer Instigator information about the killer.
	override void OnPlayerKilled_S(int playerId, IEntity playerEntity, IEntity killerEntity, notnull Instigator killer)
	{
		super.OnPlayerKilled_S(playerId, playerEntity, killerEntity, killer);
		
		if (!Replication.IsServer()) return;
		OVT_Global.GetEconomy().ChargeRespawn(playerId);
	}
}