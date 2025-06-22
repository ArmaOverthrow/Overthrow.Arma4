class OVT_LoadoutManagerComponentClass: OVT_ComponentClass
{
};

//! Manages player equipment loadouts with individual file persistence
//! Provides API for saving, loading, and managing equipment loadouts for players and AI units
//! Uses EPF PersistentScriptedState pattern for individual loadout files
class OVT_LoadoutManagerComponent: OVT_Component
{
	//! Static instance for easy access
	static OVT_LoadoutManagerComponent s_Instance;
	
	//! Get singleton instance
	static OVT_LoadoutManagerComponent GetInstance()
	{
		if (!s_Instance)
		{
			BaseGameMode pGameMode = GetGame().GetGameMode();
			if (pGameMode)
				s_Instance = OVT_LoadoutManagerComponent.Cast(pGameMode.FindComponent(OVT_LoadoutManagerComponent));
		}
		return s_Instance;
	}
	
	//! Invoker called when loadout is saved (args: string playerId, string loadoutName)
	ref ScriptInvoker m_OnLoadoutSaved = new ScriptInvoker();
	
	//! Invoker called when loadout is loaded (args: string playerId, string loadoutName)
	ref ScriptInvoker m_OnLoadoutLoaded = new ScriptInvoker();
	
	//! Invoker called when loadout is applied to entity (args: IEntity entity, string loadoutName)
	ref ScriptInvoker m_OnLoadoutApplied = new ScriptInvoker();
	
	//! Active loadouts cache for quick access (key = playerId_loadoutName)
	protected ref map<string, ref OVT_PlayerLoadout> m_mActiveLoadouts;
	
	
	//! Mapping from logical keys to EPF IDs (key = playerId_loadoutName, value = EPF ID)
	protected ref map<string, string> m_mLoadoutIdMapping;
	
	//! Initialize component
	override void OnPostInit(IEntity owner)
	{
		super.OnPostInit(owner);
		s_Instance = this;
		m_mActiveLoadouts = new map<string, ref OVT_PlayerLoadout>();
		m_mLoadoutIdMapping = new map<string, string>();
		
		SetEventMask(owner, EntityEvent.INIT);
	}
	
	//! Initialize after game loads - load existing loadouts to prevent duplicates
	override void EOnInit(IEntity owner)
	{
		super.EOnInit(owner);
		
		// TODO: In Phase 3, we should load existing loadouts here to populate cache
	}
	
	//! Save current equipment of entity as a loadout
	void SaveLoadout(string playerId, string loadoutName, IEntity sourceEntity, string description = "", bool isOfficerTemplate = false)
	{
		if (!sourceEntity || loadoutName.IsEmpty() || playerId.IsEmpty())
		{
			Print("[OVT_LoadoutManagerComponent] Invalid parameters for SaveLoadout", LogLevel.ERROR);
			return;
		}
		
		string key = GetLoadoutKey(playerId, loadoutName);
		OVT_PlayerLoadout loadout;
		
		// Check if loadout already exists - if so, delete the old one first
		string oldEpfId;
		if (m_mLoadoutIdMapping.Find(key, oldEpfId))
		{
			DeleteLoadoutByEpfId(oldEpfId);
			
			// Remove from all caches
			m_mActiveLoadouts.Remove(key);
			m_mLoadoutIdMapping.Remove(key);
		}
		
		// Create new loadout
		loadout = new OVT_PlayerLoadout();
		loadout.Initialize(loadoutName, playerId, description);
		loadout.SetAsOfficerTemplate(isOfficerTemplate);
		
		// Extract equipment from entity
		if (ExtractEquipmentFromEntity(sourceEntity, loadout))
		{
				
			// Save to persistent storage and get save data with correct EDF ID
			EPF_ScriptedStateSaveData saveData = loadout.Save();
			
			if (saveData)
			{
				string edfId = saveData.GetId();
					
				// Cache the loadout and ID mapping (use EDF ID for loading)
				m_mActiveLoadouts.Set(key, loadout);
				m_mLoadoutIdMapping.Set(key, edfId);
				
					
				// Notify listeners
				m_OnLoadoutSaved.Invoke(playerId, loadoutName);
			}
			else
			{
				Print(string.Format("[OVT_LoadoutManagerComponent] Failed to get save data for loadout '%1'", loadoutName), LogLevel.ERROR);
			}
		}
		else
		{
			// If extraction failed, clean up
			Print(string.Format("[OVT_LoadoutManagerComponent] Failed to extract equipment for loadout '%1'", loadoutName), LogLevel.ERROR);
		}
	}
	
	//! Load and apply loadout to entity from equipment box
	void LoadLoadout(string playerId, string loadoutName, IEntity targetEntity, IEntity equipmentBox)
	{
			
		if (!targetEntity || !equipmentBox || loadoutName.IsEmpty() || playerId.IsEmpty())
		{
			Print(string.Format("[OVT_LoadoutManagerComponent] Invalid parameters - TargetEntity: %1, EquipmentBox: %2, LoadoutName empty: %3, PlayerID empty: %4", 
				!targetEntity, !equipmentBox, loadoutName.IsEmpty(), playerId.IsEmpty()), LogLevel.ERROR);
			return;
		}
		
		string key = GetLoadoutKey(playerId, loadoutName);
			
		// Check cache first
		OVT_PlayerLoadout cachedLoadout;
		if (m_mActiveLoadouts.Find(key, cachedLoadout))
		{
				ApplyLoadoutToEntityFromBox(cachedLoadout, targetEntity, equipmentBox);
			return;
		}
		else
		{
				// Print all cached loadout keys for debugging
			for (int i = 0; i < m_mActiveLoadouts.Count(); i++)
			{
				string cacheKey = m_mActiveLoadouts.GetKey(i);
				}
			
			// Try to load from EPF
				if (LoadLoadoutFromEPF(playerId, loadoutName))
			{
				// Successfully loaded, try applying again
				if (m_mActiveLoadouts.Find(key, cachedLoadout))
				{
						ApplyLoadoutToEntityFromBox(cachedLoadout, targetEntity, equipmentBox);
					return;
				}
			}
			
			}
	}
	
	//! Load and apply loadout to entity (spawns items, used for RPC calls)
	void LoadLoadout(string playerId, string loadoutName, IEntity targetEntity)
	{
		if (!targetEntity || loadoutName.IsEmpty() || playerId.IsEmpty())
		{
			Print("[OVT_LoadoutManagerComponent] Invalid parameters for LoadLoadout", LogLevel.ERROR);
			return;
		}
		
		string key = GetLoadoutKey(playerId, loadoutName);
		
		// Check cache first
		OVT_PlayerLoadout cachedLoadout;
		if (m_mActiveLoadouts.Find(key, cachedLoadout))
		{
			ApplyLoadoutToEntity(cachedLoadout, targetEntity);
			return;
		}
		
		// TODO: Implement async loadout loading in Phase 2
	}
	
	//! Apply loadout directly to entity
	bool ApplyLoadoutToEntity(OVT_PlayerLoadout loadout, IEntity targetEntity)
	{
		if (!loadout || !targetEntity)
			return false;
		
		// Update usage stats
		loadout.UpdateLastUsed();
		
		// Apply equipment to entity
		bool success = ApplyEquipmentToEntity(loadout, targetEntity);
		
		if (success)
		{
			// Notify listeners
			m_OnLoadoutApplied.Invoke(targetEntity, loadout.m_sLoadoutName);
		}
		
		return success;
	}
	
	//! Apply loadout to entity using items from equipment box
	bool ApplyLoadoutToEntityFromBox(OVT_PlayerLoadout loadout, IEntity targetEntity, IEntity equipmentBox)
	{
		string loadoutName = "NULL";
		if (loadout)
			loadoutName = loadout.m_sLoadoutName;
			
		if (!loadout || !targetEntity || !equipmentBox)
		{
			Print(string.Format("[OVT_LoadoutManagerComponent] Invalid parameters - Loadout: %1, TargetEntity: %2, EquipmentBox: %3", 
				!loadout, !targetEntity, !equipmentBox), LogLevel.ERROR);
			return false;
		}
		
		// Get storage components
		InventoryStorageManagerComponent targetStorageManager = EPF_Component<InventoryStorageManagerComponent>.Find(targetEntity);
		InventoryStorageManagerComponent boxStorageManager = EPF_Component<InventoryStorageManagerComponent>.Find(equipmentBox);
		
		if (!targetStorageManager || !boxStorageManager)
		{
			Print(string.Format("[OVT_LoadoutManagerComponent] Missing storage managers - Target: %1, Box: %2", 
				!targetStorageManager, !boxStorageManager), LogLevel.ERROR);
			return false;
		}
		
			
		// Update usage stats
		loadout.UpdateLastUsed();
		
		// Apply equipment from box to entity
		bool success = ApplyEquipmentFromBox(loadout, targetEntity, targetStorageManager, equipmentBox, boxStorageManager);
		
		
		if (success)
		{
			// Notify listeners
			m_OnLoadoutApplied.Invoke(targetEntity, loadout.m_sLoadoutName);
		}
		
		return success;
	}
	
	//! Delete a loadout
	void DeleteLoadout(string playerId, string loadoutName)
	{
		if (loadoutName.IsEmpty() || playerId.IsEmpty())
			return;
		
		// Remove from cache
		string key = GetLoadoutKey(playerId, loadoutName);
		m_mActiveLoadouts.Remove(key);
		
		// Delete from repository
		OVT_LoadoutRepository.DeleteLoadout(playerId, loadoutName);
		
		}
	
	//! Get all loadouts for a player (simplified for Phase 1)
	void GetPlayerLoadouts(string playerId)
	{
		OVT_LoadoutRepository.GetPlayerLoadouts(playerId);
	}
	
	//! Get template loadouts (simplified for Phase 1)
	void GetTemplateLoadouts()
	{
		OVT_LoadoutRepository.GetTemplateLoadouts();
	}
	
	//! Get officer template loadouts (simplified for Phase 1)
	void GetOfficerTemplateLoadouts()
	{
		OVT_LoadoutRepository.GetOfficerTemplateLoadouts();
	}
	
	//! Get regular template loadouts (simplified for Phase 1)
	void GetRegularTemplateLoadouts()
	{
		OVT_LoadoutRepository.GetRegularTemplateLoadouts();
	}
	
	//! Save officer template loadout (officer-only function)
	void SaveOfficerTemplate(string playerId, string loadoutName, IEntity sourceEntity, string description = "")
	{
		// Check if player is an officer
		OVT_PlayerManagerComponent playerMgr = OVT_Global.GetPlayers();
		int playerIdInt = playerMgr.GetPlayerIDFromPersistentID(playerId);
		
		if (!OVT_Global.GetResistanceFaction().IsOfficer(playerIdInt))
		{
			Print(string.Format("[OVT_LoadoutManagerComponent] Player %1 is not an officer, cannot save officer template", playerId), LogLevel.WARNING);
			return;
		}
		
		SaveLoadout(playerId, loadoutName, sourceEntity, description, true);
	}
	
	//! Check if loadout exists
	bool LoadoutExists(string playerId, string loadoutName)
	{
		string key = GetLoadoutKey(playerId, loadoutName);
		return m_mActiveLoadouts.Contains(key);
	}
	
	//! Extract equipment from entity and populate loadout
	protected bool ExtractEquipmentFromEntity(IEntity entity, OVT_PlayerLoadout loadout)
	{
		InventoryStorageManagerComponent storageManager = EPF_Component<InventoryStorageManagerComponent>.Find(entity);
		if (!storageManager)
		{
			Print("[OVT_LoadoutManagerComponent] No inventory storage manager found on entity", LogLevel.ERROR);
			return false;
		}
		
		// Get the character's root storage component specifically
		SCR_CharacterInventoryStorageComponent charStorage = SCR_CharacterInventoryStorageComponent.Cast(entity.FindComponent(SCR_CharacterInventoryStorageComponent));
		if (!charStorage)
		{
			Print("[OVT_LoadoutManagerComponent] No SCR_CharacterInventoryStorageComponent found on entity", LogLevel.ERROR);
			return false;
		}
		
		loadout.ClearItems();
		
		// Track equipped items to avoid duplication with storage
		set<IEntity> equippedItems = new set<IEntity>();
		
		// First, identify equipped items (weapons in hands, worn clothing)
		ExtractEquippedItems(entity, loadout, equippedItems);
		
		// Extract quick slots (but track items that are already equipped)
		ExtractQuickSlots(entity, loadout);
		
		// Extract items ONLY from the character's root storage (excludes nested items)
		int storagePriority = charStorage.GetPriority();
		EStoragePurpose storagePurpose = charStorage.GetPurpose();
		int slotsCount = charStorage.GetSlotsCount();
		
		
		int itemCount = 0;
		
		// Process each slot in the character's root storage
		for (int slotIndex = 0; slotIndex < slotsCount; slotIndex++)
		{
			IEntity rootItem = charStorage.Get(slotIndex);
			if (!rootItem)
				continue; // Empty slot
			
			// Skip if this item is already tracked as equipped
			if (equippedItems.Contains(rootItem))
			{
					continue;
			}
			
			// Get item prefab resource name
			ResourceName itemPrefab = EPF_Utils.GetPrefabName(rootItem);
			if (itemPrefab.IsEmpty())
				continue;
			
			// Create loadout item with EPF-style slot information
			OVT_LoadoutItem loadoutItem = new OVT_LoadoutItem();
			loadoutItem.m_sResourceName = itemPrefab;
			loadoutItem.m_iSlotIndex = slotIndex;
			loadoutItem.m_iStoragePriority = storagePriority;
			loadoutItem.m_eStoragePurpose = storagePurpose;
			loadoutItem.m_bIsEquipped = false; // This is in storage, not equipped
			
			
			// Extract weapon attachments if this is a weapon
			WeaponComponent weaponComp = WeaponComponent.Cast(rootItem.FindComponent(WeaponComponent));
			if (weaponComp)
			{
				ExtractWeaponAttachments(rootItem, loadoutItem);
			}
			
			// Extract nested items if this item has storage (like backpack, vest pockets, etc.)
			ExtractNestedItems(rootItem, loadoutItem);
			
			// Add custom properties (can be extended later)
			AddItemProperties(rootItem, loadoutItem);
			
			// Add to loadout
			loadout.AddItem(loadoutItem);
			itemCount++;
		}
		
		return itemCount > 0;
	}
	
	//! Apply equipment from loadout to entity
	protected bool ApplyEquipmentToEntity(OVT_PlayerLoadout loadout, IEntity entity)
	{
		InventoryStorageManagerComponent storageManager = EPF_Component<InventoryStorageManagerComponent>.Find(entity);
		if (!storageManager)
		{
			Print("[OVT_LoadoutManagerComponent] No inventory storage manager found on entity", LogLevel.ERROR);
			return false;
		}
		
		// Clear existing equipment first
		ClearEntityEquipment(entity, storageManager);
		
		array<ref OVT_LoadoutItem> items = loadout.GetItems();
		int successCount = 0;
		int totalItems = items.Count();
		
		// Apply all items using EPF-style slot targeting
		foreach (OVT_LoadoutItem item : items)
		{
			if (item.m_bIsEquipped)
			{
				// Handle equipped items (weapons in hands)
				if (ApplyEquippedItem(item, entity))
					successCount++;
			}
			else
			{
				// Handle storage items
				if (ApplyLoadoutItem(item, entity, storageManager))
					successCount++;
			}
		}
		
			
		// Apply quick slots after all items are in place
		ApplyQuickSlots(entity, loadout);
		
		return successCount > 0;
	}
	
	//! Apply equipment from box to entity
	protected bool ApplyEquipmentFromBox(OVT_PlayerLoadout loadout, IEntity targetEntity, InventoryStorageManagerComponent targetStorageManager, IEntity equipmentBox, InventoryStorageManagerComponent boxStorageManager)
	{
		array<ref OVT_LoadoutItem> items = loadout.GetItems();
		int successCount = 0;
		int totalItems = items.Count();
		
			
		// Process all items with EPF-style slot targeting
		foreach (OVT_LoadoutItem item : items)
		{
			
			bool success = false;
			if (item.m_bIsEquipped)
			{
				// Equipped items can't come from box - they need to be spawned
				success = ApplyEquippedItem(item, targetEntity);
			}
			else
			{
				success = ApplyLoadoutItemFromBox(item, targetEntity, targetStorageManager, boxStorageManager);
			}
			
			if (success)
			{
				successCount++;
			}
			else
			{
				Print(string.Format("[OVT_LoadoutManagerComponent] Failed to apply item: %1", item.m_sResourceName));
			}
		}
		
		// Apply quick slots after all items are in place
		ApplyQuickSlots(targetEntity, loadout);
		
		return successCount > 0;
	}
	
	//! Collect current equipment from entity before replacing
	protected void CollectCurrentEquipment(IEntity entity, InventoryStorageManagerComponent storageManager, out array<IEntity> removedItems)
	{
		array<BaseInventoryStorageComponent> storages = new array<BaseInventoryStorageComponent>();
		
		// Get all storages from entity
		storageManager.GetStorages(storages, EStoragePurpose.PURPOSE_LOADOUT_PROXY);
		storageManager.GetStorages(storages, EStoragePurpose.PURPOSE_DEPOSIT);
		
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			array<IEntity> items = new array<IEntity>();
			storage.GetAll(items);
			
			foreach (IEntity item : items)
			{
				if (storageManager.TryRemoveItemFromStorage(item, storage))
				{
					removedItems.Insert(item);
				}
			}
		}
	}
	
	//! Apply a single loadout item from equipment box (EPF-style with exact slot targeting)
	protected bool ApplyLoadoutItemFromBox(OVT_LoadoutItem loadoutItem, IEntity targetEntity, InventoryStorageManagerComponent targetStorageManager, InventoryStorageManagerComponent boxStorageManager)
	{
		if (!loadoutItem || loadoutItem.m_sResourceName.IsEmpty())
			return false;
		
		// Find matching item in equipment box
		IEntity foundItem = FindItemInBox(loadoutItem.m_sResourceName, boxStorageManager);
		if (!foundItem)
		{
			// Item not found in box, skip silently
			return false;
		}
		
		// If this item is a container, empty its contents back into the box before transferring
		// This prevents conflicts with our nested item restoration system
		EmptyContainerIntoBox(foundItem, boxStorageManager);
		
		// Find the exact storage component on target that matches the saved storage properties
		BaseInventoryStorageComponent targetStorage = FindMatchingStorage(targetStorageManager, 
			loadoutItem.m_iStoragePriority, loadoutItem.m_eStoragePurpose);
		
		if (!targetStorage)
		{
			return false;
		}
		
		// Check if the specific slot already has an item that needs to be swapped
		IEntity currentItem = targetStorage.Get(loadoutItem.m_iSlotIndex);
		if (currentItem)
		{			
			// Remove current item from the specific slot
			if (!targetStorageManager.TryRemoveItemFromStorage(currentItem, targetStorage))
			{
				return false;
			}
		}
		
		// Find which storage the box item is in and remove it
		BaseInventoryStorageComponent itemStorage = null;
		array<BaseInventoryStorageComponent> boxStorages = new array<BaseInventoryStorageComponent>();
		boxStorageManager.GetStorages(boxStorages);
		
		foreach (BaseInventoryStorageComponent storage : boxStorages)
		{
			if (storage.Contains(foundItem))
			{
				itemStorage = storage;
				break;
			}
		}
		
		if (!itemStorage || !boxStorageManager.TryRemoveItemFromStorage(foundItem, itemStorage))
		{
			// Failed to remove from box, put current item back if we removed one
			if (currentItem)
				targetStorageManager.TryInsertItemInStorage(currentItem, targetStorage, loadoutItem.m_iSlotIndex);
			return false;
		}
		
		// Try to place new item in the exact slot
		if (targetStorageManager.TryInsertItemInStorage(foundItem, targetStorage, loadoutItem.m_iSlotIndex))
		{
			
			// Apply nested items if this item is a container (using our loadout data, not box contents)
			if (loadoutItem.HasChildItems())
			{
				ApplyNestedItems(foundItem, loadoutItem, boxStorageManager);
			}
			
			// Successfully placed new item, put old item in box if there was one
			if (currentItem)
			{
				if (!boxStorageManager.TryInsertItem(currentItem))
				{
					Print(string.Format("[OVT_LoadoutManagerComponent] Could not fit replaced item in box, deleting"));
					SCR_EntityHelper.DeleteEntityAndChildren(currentItem);
				}
			}
			return true;
		}
		else
		{
			// Failed to place new item, restore everything
			boxStorageManager.TryInsertItem(foundItem); // Put new item back in box
			if (currentItem)
				targetStorageManager.TryInsertItemInStorage(currentItem, targetStorage, loadoutItem.m_iSlotIndex); // Put old item back
			return false;
		}
	}
	
	//! Find matching storage component by priority and purpose (like EPF does)
	protected BaseInventoryStorageComponent FindMatchingStorage(InventoryStorageManagerComponent storageManager, int priority, EStoragePurpose purpose)
	{
		array<BaseInventoryStorageComponent> storages = new array<BaseInventoryStorageComponent>();
		storageManager.GetStorages(storages);
		
		foreach (BaseInventoryStorageComponent storage : storages)
		{
			if (storage.GetPriority() == priority && storage.GetPurpose() == purpose)
			{
				return storage;
			}
		}
		
		return null;
	}
	
	//! Find item in equipment box by resource name using box's UniversalInventoryStorageComponent
	protected IEntity FindItemInBox(string resourceName, InventoryStorageManagerComponent boxStorageManager)
	{
		// Get the equipment box entity from the storage manager
		IEntity boxEntity = boxStorageManager.GetOwner();
		if (!boxEntity)
		{
			Print("[OVT_LoadoutManagerComponent] FindItemInBox: Could not get box entity", LogLevel.ERROR);
			return null;
		}
		
		// Use the box's UniversalInventoryStorageComponent directly
		UniversalInventoryStorageComponent boxStorage = UniversalInventoryStorageComponent.Cast(boxEntity.FindComponent(UniversalInventoryStorageComponent));
		if (!boxStorage)
		{
			Print("[OVT_LoadoutManagerComponent] FindItemInBox: Equipment box has no UniversalInventoryStorageComponent", LogLevel.ERROR);
			return null;
		}
		
		// Get all items from the box storage (only root items)
		array<IEntity> allItems = new array<IEntity>();
		boxStorage.GetAll(allItems);
		
		// Search through all items
		foreach (IEntity item : allItems)
		{
			// Check if this item matches what we're looking for
			ResourceName itemResourceName = EPF_Utils.GetPrefabName(item);
			if (itemResourceName == resourceName)
			{
				return item;
			}
			
			// If this item is a container, search recursively within it
			IEntity foundInContainer = FindItemInContainer(resourceName, item);
			if (foundInContainer)
			{
				return foundInContainer;
			}
		}
		
		return null;
	}
	
	//! Empty a container's contents into the equipment box before transferring the container
	protected void EmptyContainerIntoBox(IEntity containerEntity, InventoryStorageManagerComponent boxStorageManager)
	{
		// Check for UniversalInventoryStorageComponent first (most containers)
		UniversalInventoryStorageComponent universalStorage = UniversalInventoryStorageComponent.Cast(containerEntity.FindComponent(UniversalInventoryStorageComponent));
		if (universalStorage)
		{
				EmptyUniversalStorageIntoBox(universalStorage, boxStorageManager, containerEntity);
			return;
		}
		
		// Fallback to InventoryStorageManagerComponent (for complex entities)
		InventoryStorageManagerComponent inventoryStorageManager = InventoryStorageManagerComponent.Cast(containerEntity.FindComponent(InventoryStorageManagerComponent));
		if (inventoryStorageManager)
		{
				EmptyStorageManagerIntoBox(inventoryStorageManager, boxStorageManager, containerEntity);
			return;
		}
		
	}
	
	//! Empty UniversalInventoryStorageComponent into box
	protected void EmptyUniversalStorageIntoBox(UniversalInventoryStorageComponent universalStorage, InventoryStorageManagerComponent boxStorageManager, IEntity containerEntity)
	{
		array<IEntity> itemsToMove = new array<IEntity>();
		
		// Collect all items from the universal storage
		for (int slotIndex = 0, slotsCount = universalStorage.GetSlotsCount(); slotIndex < slotsCount; slotIndex++)
		{
			IEntity item = universalStorage.Get(slotIndex);
			if (item)
			{
				itemsToMove.Insert(item);
			}
		}
		
		// Move all collected items to the box
		foreach (IEntity item : itemsToMove)
		{
			// Try to move item directly to box
			array<BaseInventoryStorageComponent> boxStorages = new array<BaseInventoryStorageComponent>();
			boxStorageManager.GetStorages(boxStorages, EStoragePurpose.PURPOSE_ANY);
			
			bool moved = false;
			foreach (BaseInventoryStorageComponent boxStorage : boxStorages)
			{
				if (boxStorageManager.TryMoveItemToStorage(item, boxStorage))
				{
							moved = true;
					break;
				}
			}
			
			if (!moved)
			{
				// If we can't move to box, try to delete the item instead
				if (boxStorageManager.TryDeleteItem(item))
				{
					Print(string.Format("[OVT_LoadoutManagerComponent] Box full, deleted item from container: %1", EPF_Utils.GetPrefabName(item)), LogLevel.WARNING);
				}
				else
				{
					Print(string.Format("[OVT_LoadoutManagerComponent] Failed to move or delete item from container: %1", EPF_Utils.GetPrefabName(item)), LogLevel.ERROR);
				}
			}
		}
		
		}
	
	//! Empty InventoryStorageManagerComponent into box
	protected void EmptyStorageManagerIntoBox(InventoryStorageManagerComponent containerStorageManager, InventoryStorageManagerComponent boxStorageManager, IEntity containerEntity)
	{
		array<BaseInventoryStorageComponent> containerStorages = new array<BaseInventoryStorageComponent>();
		containerStorageManager.GetStorages(containerStorages, EStoragePurpose.PURPOSE_ANY);
		
		array<IEntity> itemsToMove = new array<IEntity>();
		
		// First, collect all items that need to be moved
		foreach (BaseInventoryStorageComponent storage : containerStorages)
		{
			for (int slotIndex = 0, slotsCount = storage.GetSlotsCount(); slotIndex < slotsCount; slotIndex++)
			{
				IEntity item = storage.Get(slotIndex);
				if (item)
				{
					itemsToMove.Insert(item);
				}
			}
		}
		
		// Now move all collected items to the box
		foreach (IEntity item : itemsToMove)
		{
			// Try to move item directly to box
			array<BaseInventoryStorageComponent> boxStorages = new array<BaseInventoryStorageComponent>();
			boxStorageManager.GetStorages(boxStorages, EStoragePurpose.PURPOSE_ANY);
			
			bool moved = false;
			foreach (BaseInventoryStorageComponent boxStorage : boxStorages)
			{
				if (boxStorageManager.TryMoveItemToStorage(item, boxStorage))
				{
							moved = true;
					break;
				}
			}
			
			if (!moved)
			{
				// If we can't move to box, try to delete the item instead
				if (boxStorageManager.TryDeleteItem(item))
				{
					Print(string.Format("[OVT_LoadoutManagerComponent] Box full, deleted item from container: %1", EPF_Utils.GetPrefabName(item)), LogLevel.WARNING);
				}
				else
				{
					Print(string.Format("[OVT_LoadoutManagerComponent] Failed to move or delete item from container: %1", EPF_Utils.GetPrefabName(item)), LogLevel.ERROR);
				}
			}
		}
	}
	
	//! Recursively search for item within a container
	protected IEntity FindItemInContainer(string resourceName, IEntity containerEntity)
	{
		// Check for UniversalInventoryStorageComponent first (most containers)
		UniversalInventoryStorageComponent universalStorage = UniversalInventoryStorageComponent.Cast(containerEntity.FindComponent(UniversalInventoryStorageComponent));
		if (universalStorage)
		{
			// Search directly in the universal storage (it IS the storage)
			for (int slotIndex = 0, slotsCount = universalStorage.GetSlotsCount(); slotIndex < slotsCount; slotIndex++)
			{
				IEntity item = universalStorage.Get(slotIndex);
				if (!item)
					continue; // Empty slot
				
				// Check if this item matches what we're looking for
				ResourceName itemResourceName = EPF_Utils.GetPrefabName(item);
				if (itemResourceName == resourceName)
				{
					return item;
				}
				
				// Recursively search if this item is also a container
				IEntity foundInNestedContainer = FindItemInContainer(resourceName, item);
				if (foundInNestedContainer)
				{
					return foundInNestedContainer;
				}
			}
			return null;
		}
		
		// Fallback to InventoryStorageManagerComponent (for complex entities)
		InventoryStorageManagerComponent inventoryStorageManager = InventoryStorageManagerComponent.Cast(containerEntity.FindComponent(InventoryStorageManagerComponent));
		if (!inventoryStorageManager)
			return null; // Not a container
		
		// Get all storages within this container
		array<BaseInventoryStorageComponent> containerStorages = new array<BaseInventoryStorageComponent>();
		inventoryStorageManager.GetStorages(containerStorages, EStoragePurpose.PURPOSE_ANY);
		
		foreach (BaseInventoryStorageComponent storage : containerStorages)
		{
			// Search each slot in this container's storage
			for (int slotIndex = 0, slotsCount = storage.GetSlotsCount(); slotIndex < slotsCount; slotIndex++)
			{
				IEntity item = storage.Get(slotIndex);
				if (!item)
					continue; // Empty slot
				
				// Check if this item matches what we're looking for
				ResourceName itemResourceName = EPF_Utils.GetPrefabName(item);
				if (itemResourceName == resourceName)
				{
					return item;
				}
				
				// Recursively search if this item is also a container
				IEntity foundInNestedContainer = FindItemInContainer(resourceName, item);
				if (foundInNestedContainer)
				{
					return foundInNestedContainer;
				}
			}
		}
		
		return null;
	}
	
	//! Load loadout from EPF storage
	protected bool LoadLoadoutFromEPF(string playerId, string loadoutName)
	{
		
		string key = GetLoadoutKey(playerId, loadoutName);
		
		// Check if we have a cached EPF ID for this loadout
		string epfId;
		if (!m_mLoadoutIdMapping.Find(key, epfId))
		{
			Print(string.Format("[OVT_LoadoutManagerComponent] No EPF ID mapping found for key: %1", key));
			// Try to find EPF files by scanning (this is expensive, but needed for cross-session loading)
			return ScanAndLoadFromEPF(playerId, loadoutName);
		}
		
		
		// Load loadout using EPF persistent scripted state loader
		OVT_PlayerLoadout loadout = EPF_PersistentScriptedStateLoader<OVT_PlayerLoadout>.Load(epfId);
		if (loadout)
		{
			
			// Cache the loaded loadout
			m_mActiveLoadouts.Set(key, loadout);
			
			return true;
		}
		else
		{
			Print(string.Format("[OVT_LoadoutManagerComponent] EPF_PersistentScriptedStateLoader returned null for ID: %1", epfId));
		}
		
		Print(string.Format("[OVT_LoadoutManagerComponent] Failed to load from EPF with ID: %1", epfId));
		return false;
	}
	
	//! Scan EPF directory for loadout files (expensive fallback)
	protected bool ScanAndLoadFromEPF(string playerId, string loadoutName)
	{
		Print(string.Format("[OVT_LoadoutManagerComponent] ScanAndLoadFromEPF not implemented yet"));
		// TODO: Implement EPF directory scanning if needed
		// For now, this is a placeholder for cross-session loading without cached EPF IDs
		return false;
	}
	
	//! Generate unique key for loadout caching
	protected string GetLoadoutKey(string playerId, string loadoutName)
	{
		return string.Format("%1_%2", playerId, loadoutName);
	}
	
	//! Clear all cached loadouts
	void ClearCache()
	{
		m_mActiveLoadouts.Clear();
	}
	
	//! Get cached loadout count
	int GetCachedLoadoutCount()
	{
		return m_mActiveLoadouts.Count();
	}
	
	//! Get loadout ID mapping for save data
	map<string, string> GetLoadoutIdMapping()
	{
		return m_mLoadoutIdMapping;
	}
	
	//! Restore loadout ID mapping from save data
	void RestoreLoadoutIdMapping(map<string, string> mapping)
	{
		if (!mapping)
		{
			Print(string.Format("[OVT_LoadoutManagerComponent] No loadout ID mapping to restore (mapping is null)"));
			return;
		}
			
		m_mLoadoutIdMapping.Clear();
		
		for (int i = 0; i < mapping.Count(); i++)
		{
			string key = mapping.GetKey(i);
			string value = mapping.GetElement(i);
			m_mLoadoutIdMapping.Set(key, value);
		}
	}
	
	//! Get available loadout names for a player (returns array of loadout names)
	array<string> GetAvailableLoadouts(string playerId)
	{
		array<string> loadoutNames = new array<string>();
		
		// Add officer templates first (if player is officer or if loadouts are public)
		// TODO: In Phase 4, we could load officer templates from EPF
		
		// Add player's personal loadouts from cache
		for (int i = 0; i < m_mLoadoutIdMapping.Count(); i++)
		{
			string key = m_mLoadoutIdMapping.GetKey(i);
			
			// Parse key format: playerId_loadoutName
			array<string> keyParts = new array<string>();
			key.Split("_", keyParts, false);
			
			if (keyParts.Count() >= 2)
			{
				string keyPlayerId = keyParts[0];
				if (keyPlayerId == playerId)
				{
					// Extract loadout name (everything after first underscore)
					string loadoutName = key.Substring(playerId.Length() + 1, key.Length() - playerId.Length() - 1);
					loadoutNames.Insert(loadoutName);
				}
			}
		}
		
		return loadoutNames;
	}
	
	//! Extract equipped items (weapons in hands, worn clothing) to avoid duplication
	protected void ExtractEquippedItems(IEntity entity, OVT_PlayerLoadout loadout, out set<IEntity> equippedItems)
	{
		// Get character controller to access equipped weapons
		CharacterControllerComponent characterController = CharacterControllerComponent.Cast(entity.FindComponent(CharacterControllerComponent));
		if (characterController)
		{
			// Get equipped weapon (in hands)
			BaseWeaponManagerComponent weaponManager = BaseWeaponManagerComponent.Cast(entity.FindComponent(BaseWeaponManagerComponent));
			if (weaponManager)
			{
				BaseWeaponComponent currentWeapon = weaponManager.GetCurrent();
				if (currentWeapon)
				{
					IEntity weaponEntity = currentWeapon.GetOwner();
					if (weaponEntity)
					{
						ResourceName weaponPrefab = EPF_Utils.GetPrefabName(weaponEntity);
						if (!weaponPrefab.IsEmpty())
						{
							// Create loadout item for equipped weapon
							OVT_LoadoutItem weaponItem = new OVT_LoadoutItem();
							weaponItem.m_sResourceName = weaponPrefab;
							weaponItem.m_bIsEquipped = true;
							weaponItem.m_iSlotIndex = -1; // Special value for equipped items
							weaponItem.m_iStoragePriority = -1;
							weaponItem.m_eStoragePurpose = EStoragePurpose.PURPOSE_ANY;
							
							// Extract weapon attachments
							ExtractWeaponAttachments(weaponEntity, weaponItem);
							
							loadout.AddItem(weaponItem);
							equippedItems.Insert(weaponEntity);
							
							Print(string.Format("[OVT_LoadoutManagerComponent] Extracted equipped weapon: %1", weaponPrefab));
						}
					}
				}
			}
		}
		
		// Note: For worn clothing items (uniform, vest, helmet), they appear in regular storage slots
		// but with specific purposes. We don't need special handling for them since they're not
		// duplicated in quick slots - only weapons and tools typically are.
	}
	
	//! Extract nested items from containers (backpacks, vest pockets, etc.)
	protected void ExtractNestedItems(IEntity containerEntity, OVT_LoadoutItem containerLoadoutItem)
	{
			
		// Check for UniversalInventoryStorageComponent first (most containers)
		UniversalInventoryStorageComponent universalStorage = UniversalInventoryStorageComponent.Cast(containerEntity.FindComponent(UniversalInventoryStorageComponent));
		if (universalStorage)
		{
			ExtractNestedItemsFromUniversalStorage(containerEntity, containerLoadoutItem, universalStorage);
			return;
		}
		
		// Fallback to InventoryStorageManagerComponent (for complex entities like characters)
		InventoryStorageManagerComponent itemStorageManager = InventoryStorageManagerComponent.Cast(containerEntity.FindComponent(InventoryStorageManagerComponent));
		if (itemStorageManager)
		{
			ExtractNestedItemsFromStorageManager(containerEntity, containerLoadoutItem, itemStorageManager);
			return;
		}
	}
	
	//! Extract nested items from UniversalInventoryStorageComponent
	protected void ExtractNestedItemsFromUniversalStorage(IEntity containerEntity, OVT_LoadoutItem containerLoadoutItem, UniversalInventoryStorageComponent universalStorage)
	{
		// The UniversalInventoryStorageComponent IS the storage, not a manager
		// So we iterate through its slots directly
		int storagePriority = universalStorage.GetPriority();
		EStoragePurpose storagePurpose = universalStorage.GetPurpose();
		int slotsCount = universalStorage.GetSlotsCount();
		
		// Iterate through each slot in this container storage
		for (int slotIndex = 0; slotIndex < slotsCount; slotIndex++)
		{
			IEntity nestedEntity = universalStorage.Get(slotIndex);
			if (!nestedEntity)
				continue; // Empty slot
			
			// Get nested item prefab resource name
			ResourceName nestedItemPrefab = EPF_Utils.GetPrefabName(nestedEntity);
			if (nestedItemPrefab.IsEmpty())
				continue;
			
			// Create nested loadout item
			OVT_LoadoutItem nestedLoadoutItem = new OVT_LoadoutItem();
			nestedLoadoutItem.m_sResourceName = nestedItemPrefab;
			nestedLoadoutItem.m_iSlotIndex = slotIndex;
			nestedLoadoutItem.m_iStoragePriority = storagePriority;
			nestedLoadoutItem.m_eStoragePurpose = storagePurpose;
			nestedLoadoutItem.m_bIsEquipped = false;
			
			
			// Extract weapon attachments if this nested item is a weapon
			WeaponComponent weaponComp = WeaponComponent.Cast(nestedEntity.FindComponent(WeaponComponent));
			if (weaponComp)
			{
				ExtractWeaponAttachments(nestedEntity, nestedLoadoutItem);
			}
			
			// Recursively extract items from this nested item (if it's also a container)
			ExtractNestedItems(nestedEntity, nestedLoadoutItem);
			
			// Add custom properties
			AddItemProperties(nestedEntity, nestedLoadoutItem);
			
			// Add nested item as child of container item
			containerLoadoutItem.AddChildItem(nestedLoadoutItem);
		}
	}
	
	//! Extract nested items from InventoryStorageManagerComponent (fallback)
	protected void ExtractNestedItemsFromStorageManager(IEntity containerEntity, OVT_LoadoutItem containerLoadoutItem, InventoryStorageManagerComponent itemStorageManager)
	{
		// Get all storages within this item
		array<BaseInventoryStorageComponent> itemStorages = new array<BaseInventoryStorageComponent>();
		itemStorageManager.GetStorages(itemStorages, EStoragePurpose.PURPOSE_ANY);
				
		foreach (BaseInventoryStorageComponent storage : itemStorages)
		{
			// Get storage identification info
			int storagePriority = storage.GetPriority();
			EStoragePurpose storagePurpose = storage.GetPurpose();
			
			
			// Iterate through each slot in this nested storage
			for (int slotIndex = 0, slotsCount = storage.GetSlotsCount(); slotIndex < slotsCount; slotIndex++)
			{
				IEntity nestedEntity = storage.Get(slotIndex);
				if (!nestedEntity)
					continue; // Empty slot
				
				// Get nested item prefab resource name
				ResourceName nestedItemPrefab = EPF_Utils.GetPrefabName(nestedEntity);
				if (nestedItemPrefab.IsEmpty())
					continue;
				
				// Create nested loadout item
				OVT_LoadoutItem nestedLoadoutItem = new OVT_LoadoutItem();
				nestedLoadoutItem.m_sResourceName = nestedItemPrefab;
				nestedLoadoutItem.m_iSlotIndex = slotIndex;
				nestedLoadoutItem.m_iStoragePriority = storagePriority;
				nestedLoadoutItem.m_eStoragePurpose = storagePurpose;
				nestedLoadoutItem.m_bIsEquipped = false;
								
				// Extract weapon attachments if this nested item is a weapon
				WeaponComponent weaponComp = WeaponComponent.Cast(nestedEntity.FindComponent(WeaponComponent));
				if (weaponComp)
				{
					ExtractWeaponAttachments(nestedEntity, nestedLoadoutItem);
				}
				
				// Recursively extract items from this nested item (if it's also a container)
				ExtractNestedItems(nestedEntity, nestedLoadoutItem);
				
				// Add custom properties
				AddItemProperties(nestedEntity, nestedLoadoutItem);
				
				// Add nested item as child of container item
				containerLoadoutItem.AddChildItem(nestedLoadoutItem);
		}
	}
	}
	
	//! Extract weapon attachments from weapon entity
	protected void ExtractWeaponAttachments(IEntity weapon, OVT_LoadoutItem loadoutItem)
	{
		WeaponAttachmentsStorageComponent attachmentStorage = WeaponAttachmentsStorageComponent.Cast(weapon.FindComponent(WeaponAttachmentsStorageComponent));
		if (!attachmentStorage)
			return;
		
		// Get all attachment slots
		array<BaseInventoryStorageComponent> attachmentSlots = new array<BaseInventoryStorageComponent>();
		attachmentStorage.GetOwnedStorages(attachmentSlots, 1, true);
		
		foreach (BaseInventoryStorageComponent slot : attachmentSlots)
		{
			array<IEntity> attachments = new array<IEntity>();
			slot.GetAll(attachments);
			
			foreach (IEntity attachment : attachments)
			{
				ResourceName attachmentPrefab = EPF_Utils.GetPrefabName(attachment);
				if (!attachmentPrefab.IsEmpty())
				{
					loadoutItem.AddAttachment(attachmentPrefab);
				}
			}
		}
	}
	
	//! Get slot type from storage component
	protected int GetSlotTypeFromStorage(BaseInventoryStorageComponent storage)
	{
		if (!storage)
			return -1;
		
		// Get storage purpose to determine slot type
		EStoragePurpose purpose = storage.GetPurpose();
		
		switch (purpose)
		{
			case EStoragePurpose.PURPOSE_LOADOUT_PROXY:
				return 0; // Equipment slot (uniform, vest, helmet, etc.)
			case EStoragePurpose.PURPOSE_DEPOSIT:
				return 1; // Storage slot (backpack, ammo pouches, etc.)
			default:
				// Most character equipment seems to use purpose 9, treat as equipment
				return 0; // Equipment slot
		}
		
		return -1; // Fallback (should not reach here)
	}
	
	//! Add custom properties to loadout item
	protected void AddItemProperties(IEntity item, OVT_LoadoutItem loadoutItem)
	{
		// Add any custom properties here
		// For now, we don't extract specific properties, but this can be extended
		
		// Example: Store damage state if needed
		DamageManagerComponent damageComp = DamageManagerComponent.Cast(item.FindComponent(DamageManagerComponent));
		if (damageComp)
		{
			float healthState = damageComp.GetHealthScaled();
			if (healthState < 1.0)
			{
				loadoutItem.SetProperty("health", healthState.ToString());
			}
		}
	}
	
	//! Clear existing equipment from entity
	protected void ClearEntityEquipment(IEntity entity, InventoryStorageManagerComponent storageManager)
	{
		// Get all storages and clear them
		array<BaseInventoryStorageComponent> allStorages = new array<BaseInventoryStorageComponent>();
		storageManager.GetStorages(allStorages, EStoragePurpose.PURPOSE_ANY);
		
		foreach (BaseInventoryStorageComponent storage : allStorages)
		{
			array<IEntity> items = new array<IEntity>();
			storage.GetAll(items);
			
			foreach (IEntity item : items)
			{
				// Try to delete the item
				if (storageManager.TryDeleteItem(item))
				{
					// Successfully deleted
				}
				else
				{
					// If delete fails, try to remove it manually
					SCR_EntityHelper.DeleteEntityAndChildren(item);
				}
			}
		}
	}
	
	//! Apply equipped item (weapon in hands) to entity
	protected bool ApplyEquippedItem(OVT_LoadoutItem loadoutItem, IEntity entity)
	{
		if (!loadoutItem || loadoutItem.m_sResourceName.IsEmpty())
			return false;
		
		// Spawn the weapon entity
		EntitySpawnParams spawnParams();
		spawnParams.Transform[3] = entity.GetOrigin(); // Spawn at entity location
		
		Resource weaponResource = Resource.Load(loadoutItem.m_sResourceName);
		if (!weaponResource)
		{
			Print(string.Format("[OVT_LoadoutManagerComponent] Failed to load weapon resource: %1", loadoutItem.m_sResourceName), LogLevel.WARNING);
			return false;
		}
		
		IEntity weaponEntity = GetGame().SpawnEntityPrefab(weaponResource, GetGame().GetWorld(), spawnParams);
		if (!weaponEntity)
		{
			Print(string.Format("[OVT_LoadoutManagerComponent] Failed to spawn weapon: %1", loadoutItem.m_sResourceName), LogLevel.WARNING);
			return false;
		}
		
		// Apply weapon attachments
		if (loadoutItem.HasAttachments())
		{
			ApplyWeaponAttachments(weaponEntity, loadoutItem);
		}
		
		// Apply custom properties
		ApplyItemProperties(weaponEntity, loadoutItem);
		
		// Equip the weapon to the character
		BaseWeaponManagerComponent weaponManager = BaseWeaponManagerComponent.Cast(entity.FindComponent(BaseWeaponManagerComponent));
		if (!weaponManager)
		{
			Print("[OVT_LoadoutManagerComponent] No weapon manager found on entity", LogLevel.WARNING);
			SCR_EntityHelper.DeleteEntityAndChildren(weaponEntity);
			return false;
		}
		
		// Get the weapon component and try to equip it
		BaseWeaponComponent weaponComponent = BaseWeaponComponent.Cast(weaponEntity.FindComponent(BaseWeaponComponent));
		if (!weaponComponent)
		{
			Print(string.Format("[OVT_LoadoutManagerComponent] No weapon component found on weapon entity: %1", loadoutItem.m_sResourceName), LogLevel.WARNING);
			SCR_EntityHelper.DeleteEntityAndChildren(weaponEntity);
			return false;
		}
		
		// Try to select the weapon (this should add it to the weapon manager)
		weaponManager.SelectWeapon(weaponComponent);
		
		return true;
	}
	
	//! Apply individual loadout item to entity
	protected bool ApplyLoadoutItem(OVT_LoadoutItem loadoutItem, IEntity entity, InventoryStorageManagerComponent storageManager)
	{
		if (!loadoutItem || loadoutItem.m_sResourceName.IsEmpty())
			return false;
		
		// Spawn the item entity
		EntitySpawnParams spawnParams();
		spawnParams.Transform[3] = entity.GetOrigin(); // Spawn at entity location
		
		Resource itemResource = Resource.Load(loadoutItem.m_sResourceName);
		if (!itemResource)
		{
			Print(string.Format("[OVT_LoadoutManagerComponent] Failed to load resource: %1", loadoutItem.m_sResourceName), LogLevel.WARNING);
			return false;
		}
		
		IEntity itemEntity = GetGame().SpawnEntityPrefab(itemResource, GetGame().GetWorld(), spawnParams);
		if (!itemEntity)
		{
			Print(string.Format("[OVT_LoadoutManagerComponent] Failed to spawn item: %1", loadoutItem.m_sResourceName), LogLevel.WARNING);
			return false;
		}
		
		// Apply weapon attachments if this is a weapon
		if (loadoutItem.HasAttachments())
		{
			ApplyWeaponAttachments(itemEntity, loadoutItem);
		}
		
		// Apply custom properties
		ApplyItemProperties(itemEntity, loadoutItem);
		
		// Find the exact storage component that matches the saved storage properties
		BaseInventoryStorageComponent targetStorage = FindMatchingStorage(storageManager, 
			loadoutItem.m_iStoragePriority, loadoutItem.m_eStoragePurpose);
		
		if (!targetStorage)
		{
			Print(string.Format("[OVT_LoadoutManagerComponent] No matching storage found for spawned item %1 (priority: %2, purpose: %3)", 
				loadoutItem.m_sResourceName, loadoutItem.m_iStoragePriority, loadoutItem.m_eStoragePurpose));
			SCR_EntityHelper.DeleteEntityAndChildren(itemEntity);
			return false;
		}
		
		// Try to insert the item into the exact slot
		bool insertSuccess = storageManager.TryInsertItemInStorage(itemEntity, targetStorage, loadoutItem.m_iSlotIndex);
		
		if (!insertSuccess)
		{
			// If exact slot failed, try any slot in the same storage
			insertSuccess = storageManager.TryInsertItemInStorage(itemEntity, targetStorage);
		}
		
		if (!insertSuccess)
		{
			// Failed to insert, delete the spawned item
			SCR_EntityHelper.DeleteEntityAndChildren(itemEntity);
			Print(string.Format("[OVT_LoadoutManagerComponent] Failed to insert item into storage: %1", loadoutItem.m_sResourceName), LogLevel.WARNING);
			return false;
		}
		
		// Apply nested items if this item is a container (spawn new items since this is spawning mode)
		if (loadoutItem.HasChildItems())
		{
			ApplyNestedItemsSpawn(itemEntity, loadoutItem);
		}
		
		return true;
	}
	
	//! Apply nested items to a container item
	protected void ApplyNestedItems(IEntity containerEntity, OVT_LoadoutItem containerLoadoutItem, InventoryStorageManagerComponent boxStorageManager)
	{
		// For containers, we need to use the storage component directly, not a storage manager
		UniversalInventoryStorageComponent universalStorage = UniversalInventoryStorageComponent.Cast(containerEntity.FindComponent(UniversalInventoryStorageComponent));
		if (universalStorage)
		{
				ApplyNestedItemsToUniversalStorage(containerEntity, containerLoadoutItem, universalStorage, boxStorageManager);
			return;
		}
		
		// Fallback to InventoryStorageManagerComponent (for complex entities like characters)
		InventoryStorageManagerComponent inventoryStorageManager = InventoryStorageManagerComponent.Cast(containerEntity.FindComponent(InventoryStorageManagerComponent));
		if (inventoryStorageManager)
		{
				ApplyNestedItemsToStorageManager(containerEntity, containerLoadoutItem, inventoryStorageManager, boxStorageManager);
			return;
		}
		
	}
	
	//! Apply nested items to a container item by spawning new items (used when spawning loadouts)
	protected void ApplyNestedItemsSpawn(IEntity containerEntity, OVT_LoadoutItem containerLoadoutItem)
	{
		// For containers, we need to use the storage component directly, not a storage manager
		UniversalInventoryStorageComponent universalStorage = UniversalInventoryStorageComponent.Cast(containerEntity.FindComponent(UniversalInventoryStorageComponent));
		if (universalStorage)
		{
			ApplyNestedItemsSpawnToUniversalStorage(containerEntity, containerLoadoutItem, universalStorage);
			return;
		}
		
		// Fallback to InventoryStorageManagerComponent (for complex entities like characters)
		InventoryStorageManagerComponent inventoryStorageManager = InventoryStorageManagerComponent.Cast(containerEntity.FindComponent(InventoryStorageManagerComponent));
		if (inventoryStorageManager)
		{
			ApplyNestedItemsSpawnToStorageManager(containerEntity, containerLoadoutItem, inventoryStorageManager);
			return;
		}
	}
	
	//! Apply nested items by spawning to UniversalInventoryStorageComponent
	protected void ApplyNestedItemsSpawnToUniversalStorage(IEntity containerEntity, OVT_LoadoutItem containerLoadoutItem, UniversalInventoryStorageComponent universalStorage)
	{
		array<ref OVT_LoadoutItem> childItems = containerLoadoutItem.GetChildItems();
		if (!childItems)
			return;
		
		foreach (OVT_LoadoutItem childItem : childItems)
		{
			// Spawn the nested item
			EntitySpawnParams spawnParams();
			spawnParams.Transform[3] = containerEntity.GetOrigin();
			
			Resource itemResource = Resource.Load(childItem.m_sResourceName);
			if (!itemResource)
			{
				Print(string.Format("[OVT_LoadoutManagerComponent] Failed to load nested item resource: %1", childItem.m_sResourceName), LogLevel.WARNING);
				continue;
			}
			
			IEntity nestedItemEntity = GetGame().SpawnEntityPrefab(itemResource, GetGame().GetWorld(), spawnParams);
			if (!nestedItemEntity)
			{
				Print(string.Format("[OVT_LoadoutManagerComponent] Failed to spawn nested item: %1", childItem.m_sResourceName), LogLevel.WARNING);
				continue;
			}
			
			// Apply weapon attachments if this nested item is a weapon
			if (childItem.HasAttachments())
			{
				ApplyWeaponAttachments(nestedItemEntity, childItem);
			}
			
			// Apply custom properties
			ApplyItemProperties(nestedItemEntity, childItem);
			
			// Get the container's storage manager to insert the item
			InventoryStorageManagerComponent containerStorageManager = InventoryStorageManagerComponent.Cast(containerEntity.FindComponent(InventoryStorageManagerComponent));
			if (!containerStorageManager)
			{
				Print(string.Format("[OVT_LoadoutManagerComponent] Container %1 has no InventoryStorageManagerComponent for spawned item insertion", containerLoadoutItem.m_sResourceName), LogLevel.WARNING);
				SCR_EntityHelper.DeleteEntityAndChildren(nestedItemEntity);
				continue;
			}
			
			// Try to insert the nested item into the exact slot within the container
			bool insertSuccess = containerStorageManager.TryInsertItemInStorage(nestedItemEntity, universalStorage, childItem.m_iSlotIndex);
			
			if (!insertSuccess)
			{
				// If exact slot failed, try any slot in the same storage
				insertSuccess = containerStorageManager.TryInsertItemInStorage(nestedItemEntity, universalStorage);
			}
			
			if (!insertSuccess)
			{
				// Failed to insert nested item, delete it
				SCR_EntityHelper.DeleteEntityAndChildren(nestedItemEntity);
				Print(string.Format("[OVT_LoadoutManagerComponent] Failed to insert spawned nested item into container: %1", childItem.m_sResourceName), LogLevel.WARNING);
				continue;
			}
			
			// Recursively apply nested items if this child item is also a container
			if (childItem.HasChildItems())
			{
				ApplyNestedItemsSpawn(nestedItemEntity, childItem);
			}
		}
	}
	
	//! Apply nested items by spawning to InventoryStorageManagerComponent (fallback)
	protected void ApplyNestedItemsSpawnToStorageManager(IEntity containerEntity, OVT_LoadoutItem containerLoadoutItem, InventoryStorageManagerComponent inventoryStorageManager)
	{
		array<ref OVT_LoadoutItem> childItems = containerLoadoutItem.GetChildItems();
		if (!childItems)
			return;
		
		foreach (OVT_LoadoutItem childItem : childItems)
		{
			// Spawn the nested item
			EntitySpawnParams spawnParams();
			spawnParams.Transform[3] = containerEntity.GetOrigin();
			
			Resource itemResource = Resource.Load(childItem.m_sResourceName);
			if (!itemResource)
			{
				Print(string.Format("[OVT_LoadoutManagerComponent] Failed to load nested item resource: %1", childItem.m_sResourceName), LogLevel.WARNING);
				continue;
			}
			
			IEntity nestedItemEntity = GetGame().SpawnEntityPrefab(itemResource, GetGame().GetWorld(), spawnParams);
			if (!nestedItemEntity)
			{
				Print(string.Format("[OVT_LoadoutManagerComponent] Failed to spawn nested item: %1", childItem.m_sResourceName), LogLevel.WARNING);
				continue;
			}
			
			// Apply weapon attachments if this nested item is a weapon
			if (childItem.HasAttachments())
			{
				ApplyWeaponAttachments(nestedItemEntity, childItem);
			}
			
			// Apply custom properties
			ApplyItemProperties(nestedItemEntity, childItem);
			
			// Find the exact storage component within the container
			BaseInventoryStorageComponent targetStorage = FindMatchingStorage(inventoryStorageManager, 
				childItem.m_iStoragePriority, childItem.m_eStoragePurpose);
			
			if (!targetStorage)
			{
				Print(string.Format("[OVT_LoadoutManagerComponent] No matching storage found in container for spawned nested item %1", childItem.m_sResourceName), LogLevel.WARNING);
				SCR_EntityHelper.DeleteEntityAndChildren(nestedItemEntity);
				continue;
			}
			
			// Try to insert the nested item into the exact slot within the container
			bool insertSuccess = inventoryStorageManager.TryInsertItemInStorage(nestedItemEntity, targetStorage, childItem.m_iSlotIndex);
			
			if (!insertSuccess)
			{
				// If exact slot failed, try any slot in the same storage within the container
				insertSuccess = inventoryStorageManager.TryInsertItemInStorage(nestedItemEntity, targetStorage);
			}
			
			if (!insertSuccess)
			{
				// Failed to insert nested item, delete it
				SCR_EntityHelper.DeleteEntityAndChildren(nestedItemEntity);
				Print(string.Format("[OVT_LoadoutManagerComponent] Failed to insert spawned nested item into container: %1", childItem.m_sResourceName), LogLevel.WARNING);
				continue;
			}
			
			// Recursively apply nested items if this child item is also a container
			if (childItem.HasChildItems())
			{
				ApplyNestedItemsSpawn(nestedItemEntity, childItem);
			}
		}
	}
	
	//! Apply nested items to UniversalInventoryStorageComponent
	protected void ApplyNestedItemsToUniversalStorage(IEntity containerEntity, OVT_LoadoutItem containerLoadoutItem, UniversalInventoryStorageComponent universalStorage, InventoryStorageManagerComponent boxStorageManager)
	{
		array<ref OVT_LoadoutItem> childItems = containerLoadoutItem.GetChildItems();
		if (!childItems)
			return;
		
		foreach (OVT_LoadoutItem childItem : childItems)
		{
			// Only use items that exist in the box - never spawn new ones
			IEntity foundItem = FindItemInBox(childItem.m_sResourceName, boxStorageManager);
			if (!foundItem)
			{
				Print(string.Format("[OVT_LoadoutManagerComponent] Nested item %1 not found in box, skipping", childItem.m_sResourceName));
				continue; // Silently fail if item not in box
			}
			
			Print(string.Format("[OVT_LoadoutManagerComponent] Found existing nested item in box: %1", childItem.m_sResourceName));
			
			// Apply weapon attachments if this nested item is a weapon (only if it wasn't already applied)
			if (childItem.HasAttachments())
			{
				ApplyWeaponAttachments(foundItem, childItem);
			}
			
			// Apply custom properties
			ApplyItemProperties(foundItem, childItem);
			
			// Use the box storage manager to move the item from box to container
			bool insertSuccess = boxStorageManager.TryMoveItemToStorage(foundItem, universalStorage, childItem.m_iSlotIndex);
			
			if (!insertSuccess)
			{
				// If exact slot failed, try any slot in the container
				insertSuccess = boxStorageManager.TryMoveItemToStorage(foundItem, universalStorage);
			}
			
			if (!insertSuccess)
			{
				// Failed to insert nested item - leave it in the box
				Print(string.Format("[OVT_LoadoutManagerComponent] Failed to insert nested item into container: %1", childItem.m_sResourceName), LogLevel.WARNING);
				continue;
			}
			
			// Recursively apply nested items if this child item is also a container
			if (childItem.HasChildItems())
			{
				ApplyNestedItems(foundItem, childItem, boxStorageManager);
			}
			
			Print(string.Format("[OVT_LoadoutManagerComponent] Successfully applied nested item %1 to container %2", 
				childItem.m_sResourceName, containerLoadoutItem.m_sResourceName));
		}
	}
	
	//! Apply nested items to InventoryStorageManagerComponent (fallback)
	protected void ApplyNestedItemsToStorageManager(IEntity containerEntity, OVT_LoadoutItem containerLoadoutItem, InventoryStorageManagerComponent inventoryStorageManager, InventoryStorageManagerComponent boxStorageManager)
	{
		array<ref OVT_LoadoutItem> childItems = containerLoadoutItem.GetChildItems();
		if (!childItems)
			return;
		
		foreach (OVT_LoadoutItem childItem : childItems)
		{
			// Only use items that exist in the box - never spawn new ones
			IEntity foundItem = FindItemInBox(childItem.m_sResourceName, boxStorageManager);
			if (!foundItem)
			{
				continue; // Silently fail if item not in box
			}
			
			// Apply weapon attachments if this nested item is a weapon
			if (childItem.HasAttachments())
			{
				ApplyWeaponAttachments(foundItem, childItem);
			}
			
			// Apply custom properties
			ApplyItemProperties(foundItem, childItem);
			
			// Find the exact storage component within the container
			BaseInventoryStorageComponent targetStorage = FindMatchingStorage(inventoryStorageManager, 
				childItem.m_iStoragePriority, childItem.m_eStoragePurpose);
			
			if (!targetStorage)
			{
				continue;
			}
			
			// Try to insert the found item into the exact slot within the container
			bool insertSuccess = inventoryStorageManager.TryInsertItemInStorage(foundItem, targetStorage, childItem.m_iSlotIndex);
			
			if (!insertSuccess)
			{
				// If exact slot failed, try any slot in the same storage within the container
				insertSuccess = inventoryStorageManager.TryInsertItemInStorage(foundItem, targetStorage);
			}
			
			if (!insertSuccess)
			{
				Print(string.Format("[OVT_LoadoutManagerComponent] Failed to insert nested item into container: %1", childItem.m_sResourceName), LogLevel.WARNING);
				continue;
			}
			
			// Recursively apply nested items if this child item is also a container
			if (childItem.HasChildItems())
			{
				ApplyNestedItems(foundItem, childItem, boxStorageManager);
			}
			
		}
	}
	
	//! Extract quick slots from entity
	protected void ExtractQuickSlots(IEntity entity, OVT_PlayerLoadout loadout)
	{
		SCR_CharacterInventoryStorageComponent charInventory = SCR_CharacterInventoryStorageComponent.Cast(entity.FindComponent(SCR_CharacterInventoryStorageComponent));
		if (!charInventory)
			return;
		
		array<string> quickSlotItems = new array<string>();
		
		// Extract items from all quick slots (usually 10 slots)
		for (int i = 0; i < 10; i++)
		{
			SCR_QuickslotBaseContainer container = charInventory.GetContainerFromQuickslot(i);
			if (!container)
			{
				quickSlotItems.Insert(""); // Empty slot
				continue;
			}
			
			// Get the entity from the container
			SCR_QuickslotEntityContainer entityContainer = SCR_QuickslotEntityContainer.Cast(container);
			if (entityContainer)
			{
				IEntity quickSlotItem = entityContainer.GetEntity();
				if (quickSlotItem)
				{
					ResourceName itemPrefab = EPF_Utils.GetPrefabName(quickSlotItem);
					quickSlotItems.Insert(itemPrefab);
				}
				else
				{
					quickSlotItems.Insert(""); // Empty slot
				}
			}
			else
			{
				quickSlotItems.Insert(""); // Empty slot
			}
		}
		
		loadout.SetQuickSlotItems(quickSlotItems);
	}
	
	//! Apply quick slots to entity
	protected void ApplyQuickSlots(IEntity entity, OVT_PlayerLoadout loadout)
	{
		SCR_CharacterInventoryStorageComponent charInventory = SCR_CharacterInventoryStorageComponent.Cast(entity.FindComponent(SCR_CharacterInventoryStorageComponent));
		if (!charInventory)
			return;
		
		array<string> quickSlotItems = loadout.GetQuickSlotItems();
		if (!quickSlotItems)
			return;
		
		// Apply items to quick slots
		for (int i = 0; i < quickSlotItems.Count() && i < 10; i++)
		{
			string itemPrefab = quickSlotItems[i];
			if (itemPrefab.IsEmpty())
				continue; // Skip empty slots
			
			// Find the item in the character's inventory OR equipped weapons
			InventoryStorageManagerComponent storageManager = InventoryStorageManagerComponent.Cast(entity.FindComponent(InventoryStorageManagerComponent));
			if (!storageManager)
				continue;
			
			IEntity foundItem = null;
			
			// First check equipped weapons
			BaseWeaponManagerComponent weaponManager = BaseWeaponManagerComponent.Cast(entity.FindComponent(BaseWeaponManagerComponent));
			if (weaponManager)
			{
				BaseWeaponComponent currentWeapon = weaponManager.GetCurrent();
				if (currentWeapon)
				{
					IEntity currentWeaponEntity = currentWeapon.GetOwner();
					if (currentWeaponEntity && EPF_Utils.GetPrefabName(currentWeaponEntity) == itemPrefab)
					{
						foundItem = currentWeaponEntity;
					}
				}
				
				// Check other weapons in weapon manager if current weapon doesn't match
				if (!foundItem)
				{
					array<IEntity> weapons = new array<IEntity>();
					weaponManager.GetWeaponsList(weapons);
					foreach (IEntity weapon : weapons)
					{
						if (EPF_Utils.GetPrefabName(weapon) == itemPrefab)
						{
							foundItem = weapon;
							break;
						}
					}
				}
			}
			
			// If not found in equipped weapons, check storage
			if (!foundItem)
			{
				array<IEntity> items = new array<IEntity>();
				storageManager.GetItems(items);
				
				foreach (IEntity item : items)
				{
					if (EPF_Utils.GetPrefabName(item) == itemPrefab)
					{
						foundItem = item;
						break; // Only assign the first matching item
					}
				}
			}
			
			// Set the found item to quick slot
			if (foundItem)
			{
				int result = charInventory.StoreItemToQuickSlot(foundItem, i, false);
			}
		}
	}
	
	//! Apply weapon attachments to weapon entity
	protected void ApplyWeaponAttachments(IEntity weapon, OVT_LoadoutItem loadoutItem)
	{
		WeaponAttachmentsStorageComponent attachmentStorage = WeaponAttachmentsStorageComponent.Cast(weapon.FindComponent(WeaponAttachmentsStorageComponent));
		if (!attachmentStorage)
			return;
		
		array<string> attachments = loadoutItem.GetAttachments();
		if (!attachments || attachments.IsEmpty())
			return;
		
		foreach (string attachmentPrefab : attachments)
		{
			// Spawn attachment
			EntitySpawnParams spawnParams();
			spawnParams.Transform[3] = weapon.GetOrigin();
			
			Resource attachmentResource = Resource.Load(attachmentPrefab);
			if (!attachmentResource)
			{
				Print(string.Format("[OVT_LoadoutManagerComponent] Failed to load attachment resource: %1", attachmentPrefab), LogLevel.WARNING);
				continue;
			}
			
			IEntity attachmentEntity = GetGame().SpawnEntityPrefab(attachmentResource, GetGame().GetWorld(), spawnParams);
			if (!attachmentEntity)
			{
				Print(string.Format("[OVT_LoadoutManagerComponent] Failed to spawn attachment: %1", attachmentPrefab), LogLevel.WARNING);
				continue;
			}
			
			// Try to attach to weapon
			InventoryStorageManagerComponent weaponStorageManager = EPF_Component<InventoryStorageManagerComponent>.Find(weapon);
			if (weaponStorageManager)
			{
				if (!weaponStorageManager.TryInsertItem(attachmentEntity))
				{
					// If attachment failed, clean up
					SCR_EntityHelper.DeleteEntityAndChildren(attachmentEntity);
				}
			}
		}
	}
	
	//! Apply custom properties to item entity
	protected void ApplyItemProperties(IEntity item, OVT_LoadoutItem loadoutItem)
	{
		map<string, string> properties = loadoutItem.GetProperties();
		if (!properties || properties.IsEmpty())
			return;
		
		// Apply health state if stored
		string healthStr;
		if (properties.Find("health", healthStr))
		{
			float health = healthStr.ToFloat();
			if (health > 0 && health < 1.0)
			{
				DamageManagerComponent damageComp = DamageManagerComponent.Cast(item.FindComponent(DamageManagerComponent));
				if (damageComp)
				{
					// Set damage to restore health state
					float damage = 1.0 - health;
					damageComp.SetHealthScaled(health);
				}
			}
		}
		
		// Add more property applications here as needed
	}
	
	//! Sanitize string for use in IDs
	protected string SanitizeForId(string input)
	{
		string result = "";
		for (int i = 0; i < input.Length(); i++)
		{
			string char = input.Get(i);
			// Only allow alphanumeric and underscore
			if ((char >= "a" && char <= "z") || 
			    (char >= "A" && char <= "Z") || 
			    (char >= "0" && char <= "9") || 
			    char == "_")
			{
				result += char;
			}
			else
			{
				result += "_"; // Replace special chars with underscore
			}
		}
		return result;
	}
	
	//! Delete loadout by EPF ID
	protected void DeleteLoadoutByEpfId(string epfId)
	{
		if (epfId.IsEmpty())
			return;
		
		// Delete from EPF database
		EPF_PersistenceManager persistenceManager = EPF_PersistenceManager.GetInstance();
		if (persistenceManager)
		{
			EPF_PersistentScriptedStateSettings settings = EPF_PersistentScriptedStateSettings.Get(OVT_PlayerLoadout);
			if (settings && settings.m_tSaveDataType)
			{
				// Find and delete the save data
				array<ref EDF_DbEntity> findResults = persistenceManager
					.GetDbContext()
					.FindAll(settings.m_tSaveDataType, EDF_DbFind.Id().Equals(epfId), limit: 1)
					.GetEntities();
					
				if (findResults && !findResults.IsEmpty())
				{
					EDF_DbEntity saveData = findResults.Get(0);
					persistenceManager.GetDbContext().RemoveAsync(saveData);
				}
			}
		}
	}
}