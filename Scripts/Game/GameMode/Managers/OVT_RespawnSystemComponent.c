//------------------------------------------------------------------------------------------------
class OVT_RespawnSystemComponentClass : EPF_BaseRespawnSystemComponentClass
{
};

//------------------------------------------------------------------------------------------------
//! Handles the spawning and respawning logic for players within the Overthrow game mode.
//! Extends the base EPF respawn system to integrate with Overthrow-specific player data and spawning rules.
//! Should be attached to the OVT_OverthrowGameMode entity.
[ComponentEditorProps(icon: HYBRID_COMPONENT_ICON)]
class OVT_RespawnSystemComponent : EPF_BaseRespawnSystemComponent
{	
	protected OVT_OverthrowGameMode m_Overthrow;
	
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
	protected override void OnUidAvailable(int playerId)
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
		
		super.OnUidAvailable(playerId);		
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
			position = player.home;
			yawPitchRoll = "0 0 0";
		}
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