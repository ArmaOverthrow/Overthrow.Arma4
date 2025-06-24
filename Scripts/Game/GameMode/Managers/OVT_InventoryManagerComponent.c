//------------------------------------------------------------------------------------------------
//! Class definition for the OVT_InventoryManagerComponent.
class OVT_InventoryManagerComponentClass: OVT_ComponentClass
{	
};

//------------------------------------------------------------------------------------------------
//! Centralized inventory management system for Overthrow
//! Handles all storage transfers, container operations, and inventory transactions
//! Incorporates insights from loadout manager for robust inventory handling
[EPF_ComponentSaveDataType(OVT_InventoryManagerComponent)]
class OVT_InventoryManagerSaveDataClass : EPF_ComponentSaveDataClass {};

class OVT_InventoryManagerSaveData : EPF_ComponentSaveData
{
	// Currently no persistent data needed, but structure is here for future use
	
	override EPF_EReadResult ReadFrom(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{
		return EPF_EReadResult.OK;
	}
	
	override EPF_EApplyResult ApplyTo(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
	{
		return EPF_EApplyResult.OK;
	}
}

//------------------------------------------------------------------------------------------------
//! Progress callback interface for storage operations
class OVT_StorageProgressCallback
{
	//! Called when progress updates (progress 0.0 to 1.0)
	void OnProgressUpdate(float progress, int currentItem, int totalItems, string operation) {}
	
	//! Called when operation completes successfully
	void OnComplete(int itemsTransferred, int itemsSkipped) {}
	
	//! Called when operation fails or is cancelled
	void OnError(string errorMessage) {}
}

//------------------------------------------------------------------------------------------------
//! Chained callback class to call multiple callbacks in sequence
class OVT_ChainedProgressCallback : OVT_StorageProgressCallback
{
	protected ref OVT_StorageProgressCallback m_FirstCallback;
	protected ref OVT_StorageProgressCallback m_SecondCallback;
	
	void OVT_ChainedProgressCallback(OVT_StorageProgressCallback first, OVT_StorageProgressCallback second)
	{
		m_FirstCallback = first;
		m_SecondCallback = second;
	}
	
	override void OnProgressUpdate(float progress, int currentItem, int totalItems, string operation)
	{
		if (m_FirstCallback) m_FirstCallback.OnProgressUpdate(progress, currentItem, totalItems, operation);
		if (m_SecondCallback) m_SecondCallback.OnProgressUpdate(progress, currentItem, totalItems, operation);
	}
	
	override void OnComplete(int itemsTransferred, int itemsSkipped)
	{		
		if (m_FirstCallback) 
		{
			m_FirstCallback.OnComplete(itemsTransferred, itemsSkipped);
		}
		
		if (m_SecondCallback) 
		{
			m_SecondCallback.OnComplete(itemsTransferred, itemsSkipped);
		}
	}
	
	override void OnError(string errorMessage)
	{
		if (m_FirstCallback) m_FirstCallback.OnError(errorMessage);
		if (m_SecondCallback) m_SecondCallback.OnError(errorMessage);
	}
}

//------------------------------------------------------------------------------------------------
//! Wrapper class to connect UI context with progress callback interface
class OVT_ProgressUICallbackWrapper : OVT_StorageProgressCallback
{
	protected ref OVT_StorageProgressUIContext m_UIContext;
	protected int m_iCurrentContainer = 0;
	protected int m_iTotalContainers = 0;
	
	void OVT_ProgressUICallbackWrapper(OVT_StorageProgressUIContext uiContext)
	{
		m_UIContext = uiContext;
	}
	
	override void OnProgressUpdate(float progress, int currentItem, int totalItems, string operation)
	{
		if (!m_UIContext) return;
		
		// Update the UI context with progress information
		m_UIContext.UpdateProgress(progress, m_iCurrentContainer, m_iTotalContainers, currentItem, totalItems, operation);
	}
	
	override void OnComplete(int itemsTransferred, int itemsSkipped)
	{
		m_UIContext.OnOperationComplete(itemsTransferred, itemsSkipped);
	}
	
	override void OnError(string errorMessage)
	{
		if (!m_UIContext) return;
		
		m_UIContext.OnOperationError(errorMessage);
	}
	
	//! Sets container progress information
	void SetContainerProgress(int currentContainer, int totalContainers)
	{
		m_iCurrentContainer = currentContainer;
		m_iTotalContainers = totalContainers;
	}
}

//------------------------------------------------------------------------------------------------
//! Storage operation configuration
class OVT_StorageOperationConfig
{
	bool showProgress = true;
	bool playSound = true;
	bool removeEmptyContainers = false;
	float delayBetweenItems = 150; // milliseconds - increased for server performance
	float delayBetweenContainers = 300; // milliseconds - delay between containers
	float searchRadius = 75.0; // meters for container collection
	int itemsPerChunk = 5; // items to process per frame to prevent server hitches
	
	void OVT_StorageOperationConfig(bool progress = true, bool sound = true, bool cleanup = false, float itemDelay = 150, float containerDelay = 300, float radius = 75.0, int chunkSize = 5)
	{
		showProgress = progress;
		playSound = sound;
		removeEmptyContainers = cleanup;
		delayBetweenItems = itemDelay;
		delayBetweenContainers = containerDelay;
		searchRadius = radius;
		itemsPerChunk = chunkSize;
	}
}

//------------------------------------------------------------------------------------------------
//! Centralized inventory manager component for the game mode
class OVT_InventoryManagerComponent: OVT_Component
{
	protected static OVT_InventoryManagerComponent s_Instance;
	
	// Active transfer operations tracking
	protected ref map<string, ref OVT_StorageProgressCallback> m_mActiveOperations = new map<string, ref OVT_StorageProgressCallback>();
	protected ref array<string> m_aOperationQueue = {};
	protected bool m_bProcessingQueue = false;
	
	//------------------------------------------------------------------------------------------------
	void Init(IEntity owner)
	{
		s_Instance = this;
	}
	
	//------------------------------------------------------------------------------------------------
	//------------------------------------------------------------------------------------------------
	//! Gets the singleton instance of the OVT_InventoryManagerComponent.
	//! \return The static instance or null if not found.
	static OVT_InventoryManagerComponent GetInstance()
	{
		if (!s_Instance)
		{
			BaseGameMode pGameMode = GetGame().GetGameMode();
			if (pGameMode)
				s_Instance = OVT_InventoryManagerComponent.Cast(pGameMode.FindComponent(OVT_InventoryManagerComponent));
		}

		return s_Instance;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Enhanced storage transfer with progress tracking and error handling
	//! \param fromEntity Source entity with storage
	//! \param toEntity Target entity with storage
	//! \param config Operation configuration
	//! \param callback Progress callback (optional)
	//! \return Operation ID for tracking
	string TransferStorage(IEntity fromEntity, IEntity toEntity, OVT_StorageOperationConfig config = null, OVT_StorageProgressCallback callback = null)
	{
		// SERVER-SIDE ONLY: All inventory operations must happen on server
		if (!Replication.IsServer())
		{
			Print("[Overthrow] TransferStorage: Attempted to run on client - inventory operations are server-side only!", LogLevel.WARNING);
			if (callback) callback.OnError("Inventory operations must be performed server-side");
			return "";
		}
		
		if (!fromEntity || !toEntity)
		{
			if (callback) callback.OnError("Invalid entities provided");
			return "";
		}
		
		OVT_StorageOperationConfig operationConfig = config;
		if (!operationConfig) operationConfig = new OVT_StorageOperationConfig();
		
		string operationId = GenerateOperationId();
		if (callback) m_mActiveOperations.Set(operationId, callback);
				
		// Start transfer operation with small initial delay
		GetGame().GetCallqueue().CallLater(PerformStorageTransfer, 50, false, operationId, fromEntity, toEntity, operationConfig);
		
		return operationId;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Transfer storage using RplIds (legacy compatibility)
	//! \param from Source entity RplId
	//! \param to Target entity RplId
	//! \param config Operation configuration
	//! \param callback Progress callback (optional)
	//! \return Operation ID for tracking
	string TransferStorageByRplId(RplId from, RplId to, OVT_StorageOperationConfig config = null, OVT_StorageProgressCallback callback = null)
	{
		// SERVER-SIDE ONLY check
		if (!Replication.IsServer())
		{
			Print("[Overthrow] TransferStorageByRplId: Attempted to run on client - inventory operations are server-side only!", LogLevel.WARNING);
			if (callback) callback.OnError("Inventory operations must be performed server-side");
			return "";
		}
		
		IEntity fromEntity = GetEntityFromRplId(from);
		IEntity toEntity = GetEntityFromRplId(to);
		
		return TransferStorage(fromEntity, toEntity, config, callback);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Collect and transfer items from all containers within radius to target vehicle
	//! \param centerPos Center position for search
	//! \param targetVehicle Vehicle to transfer items to
	//! \param config Operation configuration
	//! \param callback Progress callback (optional)
	//! \return Operation ID for tracking
	string CollectContainersToVehicle(vector centerPos, IEntity targetVehicle, OVT_StorageOperationConfig config = null, OVT_StorageProgressCallback callback = null)
	{
		if (!targetVehicle)
		{
			if (callback) callback.OnError("Invalid target vehicle");
			return "";
		}
		
		OVT_StorageOperationConfig operationConfig = config;
		if (!operationConfig) operationConfig = new OVT_StorageOperationConfig();
		
		string operationId = GenerateOperationId();
		if (callback) m_mActiveOperations.Set(operationId, callback);
		
		// Find containers and start collection process
		GetGame().GetCallqueue().CallLater(StartContainerCollection, 10, false, operationId, centerPos, targetVehicle, operationConfig);
		
		return operationId;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Enhanced FOB undeployment with container collection
	//! \param deployedFOB Deployed FOB entity
	//! \param mobileFOB Mobile FOB truck entity
	//! \param callback Progress callback (optional)
	//! \return Operation ID for tracking
	string UndeployFOBWithCollection(IEntity deployedFOB, IEntity mobileFOB, OVT_StorageProgressCallback callback = null)
	{
		OVT_StorageOperationConfig config = new OVT_StorageOperationConfig(true, true, true, 100, 300, 75.0, 3);
		return CollectContainersToVehicle(deployedFOB.GetOrigin(), mobileFOB, config, callback);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Enhanced FOB undeployment with automatic progress dialog
	//! \param deployedFOB Deployed FOB entity
	//! \param mobileFOB Mobile FOB truck entity
	//! \param showProgressDialog Whether to show progress dialog
	//! \param playerId Player ID for UI context (required for progress dialog)
	//! \param callback Progress callback (optional)
	//! \return Operation ID for tracking
	string UndeployFOBWithProgress(IEntity deployedFOB, IEntity mobileFOB, bool showProgressDialog = true, int playerId = -1, OVT_StorageProgressCallback callback = null)
	{
		if (!showProgressDialog || playerId == -1)
		{
			return UndeployFOBWithCollection(deployedFOB, mobileFOB, callback);
		}
		
		// Get player entity and UI manager
		IEntity playerEntity = GetGame().GetPlayerManager().GetPlayerControlledEntity(playerId);
		if (!playerEntity)
		{
			Print("[Overthrow] UndeployFOBWithProgress: Could not find player entity, proceeding without UI", LogLevel.WARNING);
			return UndeployFOBWithCollection(deployedFOB, mobileFOB, null);
		}
		
		OVT_UIManagerComponent uiManager = EPF_Component<OVT_UIManagerComponent>.Find(playerEntity);
		if (!uiManager)
		{
			Print("[Overthrow] UndeployFOBWithProgress: Could not find UI manager, proceeding without UI", LogLevel.WARNING);
			return UndeployFOBWithCollection(deployedFOB, mobileFOB, null);
		}
		
		// Get the storage progress context
		OVT_StorageProgressUIContext progressContext = OVT_StorageProgressUIContext.Cast(uiManager.GetContext(OVT_StorageProgressUIContext));
		if (!progressContext)
		{
			Print("[Overthrow] UndeployFOBWithProgress: Could not find storage progress context, proceeding without UI", LogLevel.WARNING);
			return UndeployFOBWithCollection(deployedFOB, mobileFOB, null);
		}
		
		// Set up progress context
		string operationId = GenerateOperationId();
		progressContext.SetupOperation("FOB Undeployment", operationId, true);
		progressContext.ShowLayout();
		
		// Create progress callback wrapper for the UI context
		OVT_ProgressUICallbackWrapper callbackWrapper = new OVT_ProgressUICallbackWrapper(progressContext);
		
		// Use enhanced configuration for FOB operations
		OVT_StorageOperationConfig config = new OVT_StorageOperationConfig(true, true, true, 150, 400, 75.0, 3);
		
		// Chain callbacks - if we have a provided callback, create a chained callback
		OVT_StorageProgressCallback finalCallback = callbackWrapper;
		if (callback)
		{
			// Create a chained callback that calls both UI wrapper and provided callback
			finalCallback = new OVT_ChainedProgressCallback(callbackWrapper, callback);
		}
		
		// Start the operation with the chained callback
		if (finalCallback) 
		{
			m_mActiveOperations.Set(operationId, finalCallback);
		}
		GetGame().GetCallqueue().CallLater(StartContainerCollection, 50, false, 
			operationId, deployedFOB.GetOrigin(), mobileFOB, config);
		
		return operationId;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Transfer storage with automatic progress dialog
	//! \param fromEntity Source entity
	//! \param toEntity Target entity
	//! \param showProgressDialog Whether to show progress dialog
	//! \param operationTitle Title for progress dialog
	//! \return Operation ID for tracking
	string TransferStorageWithProgress(IEntity fromEntity, IEntity toEntity, bool showProgressDialog = true, string operationTitle = "Storage Transfer")
	{
		if (!showProgressDialog)
		{
			return TransferStorage(fromEntity, toEntity, null, null);
		}
		
		// Use enhanced configuration for visible transfers
		OVT_StorageOperationConfig config = new OVT_StorageOperationConfig(true, true, false, 100, 200, 10.0, 5);
		
		// Note: Progress UI must be shown from client-side code before calling this method
		// The server-side inventory manager cannot create UI elements
		return TransferStorage(fromEntity, toEntity, config, null);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Transfer items to warehouse (existing functionality)
	//! \param fromEntity Source entity
	//! \param callback Progress callback (optional)
	//! \return Operation ID for tracking
	string TransferToWarehouse(IEntity fromEntity, OVT_StorageProgressCallback callback = null)
	{
		if (!fromEntity)
		{
			if (callback) callback.OnError("Invalid source entity");
			return "";
		}
		
		OVT_RealEstateManagerComponent realEstate = OVT_Global.GetRealEstate();
		if (!realEstate)
		{
			if (callback) callback.OnError("Real estate manager not available");
			return "";
		}
		
		string operationId = GenerateOperationId();
		if (callback) m_mActiveOperations.Set(operationId, callback);
		
		GetGame().GetCallqueue().CallLater(PerformWarehouseTransfer, 10, false, operationId, fromEntity);
		
		return operationId;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Cancel an ongoing operation
	//! \param operationId Operation to cancel
	void CancelOperation(string operationId)
	{
		OVT_StorageProgressCallback callback = m_mActiveOperations.Get(operationId);
		if (callback)
		{
			callback.OnError("Operation cancelled");
			m_mActiveOperations.Remove(operationId);
		}
		
		m_aOperationQueue.RemoveItem(operationId);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get storage components with priority-based matching (from loadout manager insights)
	//! \param entity Entity to check
	//! \param storageManager Output storage manager
	//! \param universalStorage Output universal storage
	//! \return True if valid storage components found
	protected bool GetStorageComponents(IEntity entity, out InventoryStorageManagerComponent storageManager, out UniversalInventoryStorageComponent universalStorage)
	{
		if (!entity) return false;
		
		// Try to get storage manager component
		storageManager = InventoryStorageManagerComponent.Cast(entity.FindComponent(InventoryStorageManagerComponent));
		
		// Try to get universal storage component
		universalStorage = UniversalInventoryStorageComponent.Cast(entity.FindComponent(UniversalInventoryStorageComponent));
		
		return storageManager && universalStorage;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Enhanced item transfer with fallback mechanisms (inspired by loadout manager)
	//! \param item Item to transfer
	//! \param targetStorage Target storage component
	//! \param storageManager Storage manager for operations
	//! \return True if transfer successful
	protected bool TransferItemWithFallback(IEntity item, UniversalInventoryStorageComponent targetStorage, InventoryStorageManagerComponent storageManager)
	{
		if (!item || !targetStorage || !storageManager) return false;
		
		// Try to find specific suitable slot first
		InventoryStorageSlot itemSlot = targetStorage.FindSuitableSlotForItem(item);
		int slotID = -1;
		if (itemSlot) slotID = itemSlot.GetID();
		
		// Attempt primary transfer
		if (storageManager.TryMoveItemToStorage(item, targetStorage, slotID))
			return true;
		
		// Fallback 1: Try without specific slot
		if (storageManager.TryMoveItemToStorage(item, targetStorage, -1))
			return true;
		
		// Fallback 2: Check if item can be stacked with existing items
		array<IEntity> existingItems = {};
		storageManager.GetItems(existingItems);
		
		foreach (IEntity existingItem : existingItems)
		{
			if (CanStackItems(item, existingItem))
			{
				// Try to combine items (simplified - would need actual stacking logic)
				if (storageManager.TryMoveItemToStorage(item, targetStorage, -1))
					return true;
			}
		}
		
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Check if two items can be stacked (placeholder implementation)
	//! \param item1 First item
	//! \param item2 Second item
	//! \return True if items can stack
	protected bool CanStackItems(IEntity item1, IEntity item2)
	{
		if (!item1 || !item2) return false;
		
		// Simple implementation - check if they're the same prefab
		ResourceName prefab1 = item1.GetPrefabData().GetPrefabName();
		ResourceName prefab2 = item2.GetPrefabData().GetPrefabName();
		
		return prefab1 == prefab2;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Perform storage transfer operation
	protected void PerformStorageTransfer(string operationId, IEntity fromEntity, IEntity toEntity, OVT_StorageOperationConfig config)
	{
		OVT_StorageProgressCallback callback = m_mActiveOperations.Get(operationId);
		
		InventoryStorageManagerComponent fromManager, toManager;
		UniversalInventoryStorageComponent fromStorage, toStorage;
		
		// Get storage components for both entities
		bool fromValid = GetStorageComponents(fromEntity, fromManager, fromStorage);
		bool toValid = GetStorageComponents(toEntity, toManager, toStorage);
		
		if (!fromValid || !toValid)
		{
			if (callback) callback.OnError("Storage components not found");
			m_mActiveOperations.Remove(operationId);
			return;
		}
		
		// Get all items to transfer
		array<InventoryItemComponent> itemComps = {};
		fromStorage.GetOwnedItems(itemComps);
		
		if (itemComps.IsEmpty())
		{
			if (callback) callback.OnComplete(0, 0);
			m_mActiveOperations.Remove(operationId);
			return;
		}
		
		// Start item-by-item transfer with progress
		TransferItemsWithProgress(operationId, itemComps, toStorage, toManager, config, 0, 0, 0);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Transfer items with progress tracking using chunked processing to prevent server hitches
	protected void TransferItemsWithProgress(string operationId, array<InventoryItemComponent> items, UniversalInventoryStorageComponent targetStorage, InventoryStorageManagerComponent storageManager, OVT_StorageOperationConfig config, int currentIndex, int successCount, int skipCount)
	{
		OVT_StorageProgressCallback callback = m_mActiveOperations.Get(operationId);
		
		if (currentIndex >= items.Count())
		{
			// Transfer complete			
			if (callback) callback.OnComplete(successCount, skipCount);
			m_mActiveOperations.Remove(operationId);
			
			if (config.playSound)
				PlayTransferSound(targetStorage.GetOwner());
			
			return;
		}
		
		// Process items in chunks to prevent server hitches
		int itemsToProcess = Math.Min(config.itemsPerChunk, items.Count() - currentIndex);
		int chunkEnd = currentIndex + itemsToProcess;
		
		// Update progress before processing chunk
		if (callback)
		{
			float progress = (float)currentIndex / (float)items.Count();
			string operation = string.Format("Processing items %1-%2 of %3", 
				currentIndex + 1, chunkEnd, items.Count());
			callback.OnProgressUpdate(progress, currentIndex + 1, items.Count(), operation);
		}
		
		// Process chunk of items
		for (int i = currentIndex; i < chunkEnd; i++)
		{
			IEntity item = items[i].GetOwner();
			bool transferred = false;
			
			if (item)
			{
				transferred = TransferItemWithFallback(item, targetStorage, storageManager);
				
				if (transferred)
				{
					successCount++;
				}
				else
				{
					skipCount++;
					Print(string.Format("[Overthrow] Failed to transfer item: %1 (storage may be full)", 
						GetEntityDisplayName(item)), LogLevel.WARNING);
				}
			}
			else
			{
				skipCount++;
			}
		}
		
		// Continue with next chunk after delay
		float nextDelay = config.delayBetweenItems;
		// Add extra delay for larger chunks to spread server load
		if (itemsToProcess > 1)
			nextDelay += (itemsToProcess - 1) * 25;
		
		GetGame().GetCallqueue().CallLater(TransferItemsWithProgress, nextDelay, false, 
			operationId, items, targetStorage, storageManager, config, chunkEnd, successCount, skipCount);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Start container collection process
	protected void StartContainerCollection(string operationId, vector centerPos, IEntity targetVehicle, OVT_StorageOperationConfig config)
	{
		OVT_StorageProgressCallback callback = m_mActiveOperations.Get(operationId);
		
		// Find all containers in radius
		array<IEntity> containers = {};
		FindContainersInRadius(centerPos, config.searchRadius, containers);
		
		if (containers.IsEmpty())
		{
			if (callback) callback.OnComplete(0, 0);
			m_mActiveOperations.Remove(operationId);
			return;
		}
		
		// Start processing containers
		ProcessContainersSequentially(operationId, containers, targetVehicle, config, 0, 0, 0);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Find containers within radius using proper callback pattern
	protected void FindContainersInRadius(vector centerPos, float radius, out array<IEntity> containers)
	{
		// Clear containers array and store reference for callback
		containers.Clear();
		m_aContainerSearchResults = containers;
		
		// Use correct callback pattern - just a method name that takes IEntity and returns bool
		GetGame().GetWorld().QueryEntitiesBySphere(centerPos, radius, null, FilterContainerEntities, EQueryEntitiesFlags.STATIC | EQueryEntitiesFlags.DYNAMIC);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Container search results array for callback usage
	protected ref array<IEntity> m_aContainerSearchResults;
	
	//------------------------------------------------------------------------------------------------
	//! Filter callback for QueryEntitiesBySphere to find containers
	//! \param entity Entity being tested
	//! \return Always returns false to continue searching
	protected bool FilterContainerEntities(IEntity entity)
	{
		if (!entity || !m_aContainerSearchResults) return false;
		
		// Check if entity has storage and is a valid container
		if (HasValidStorageComponent(entity) && IsCollectableContainer(entity))
		{
			m_aContainerSearchResults.Insert(entity);
		}
		
		return false; // Continue searching
	}
	
	//------------------------------------------------------------------------------------------------
	//! Check if entity has valid storage component
	protected bool HasValidStorageComponent(IEntity entity)
	{
		if (!entity) return false;
		
		UniversalInventoryStorageComponent storage = UniversalInventoryStorageComponent.Cast(
			entity.FindComponent(UniversalInventoryStorageComponent));
		return storage != null;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Check if entity is a collectable container
	protected bool IsCollectableContainer(IEntity entity)
	{
		if (!entity) return false;
		
		// Check for placeable component (equipment boxes, etc.)
		OVT_PlaceableComponent placeable = OVT_PlaceableComponent.Cast(
			entity.FindComponent(OVT_PlaceableComponent));
		if (placeable) return true;
		
		// Check for buildable component (built structures with storage)
		OVT_BuildableComponent buildable = OVT_BuildableComponent.Cast(
			entity.FindComponent(OVT_BuildableComponent));
		if (buildable) return true;
		
		// Could add other container type checks here
		return false;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Process containers sequentially with progress
	protected void ProcessContainersSequentially(string operationId, array<IEntity> containers, IEntity targetVehicle, OVT_StorageOperationConfig config, int containerIndex, int totalTransferred, int totalSkipped)
	{
		OVT_StorageProgressCallback callback = m_mActiveOperations.Get(operationId);
		
		if (containerIndex >= containers.Count())
		{
			// All containers processed
			if (callback) 
			{
				callback.OnComplete(totalTransferred, totalSkipped);
			}
			m_mActiveOperations.Remove(operationId);
			
			if (config.playSound)
				PlayTransferSound(targetVehicle);
			
			return;
		}
		
		IEntity currentContainer = containers[containerIndex];
		if (!currentContainer)
		{
			// Container was deleted, skip to next
			GetGame().GetCallqueue().CallLater(ProcessContainersSequentially, config.delayBetweenItems, false,
				operationId, containers, targetVehicle, config, containerIndex + 1, totalTransferred, totalSkipped);
			return;
		}
		
		// Update progress
		if (callback)
		{
			float progress = (float)containerIndex / (float)containers.Count();
			string operation = string.Format("Processing container %1 of %2: %3", 
				containerIndex + 1, containers.Count(), GetEntityDisplayName(currentContainer));
			callback.OnProgressUpdate(progress, containerIndex + 1, containers.Count(), operation);
		}
		
		// Transfer items from this container to target vehicle
		string transferId = TransferStorage(currentContainer, targetVehicle, 
			new OVT_StorageOperationConfig(false, false, false, 10), null);
		
		// Remove container if configured to do so
		if (config.removeEmptyContainers)
		{
			GetGame().GetCallqueue().CallLater(RemoveEmptyContainer, config.delayBetweenItems + 50, false, currentContainer);
		}
		
		// Continue with next container with longer delay for container processing
		GetGame().GetCallqueue().CallLater(ProcessContainersSequentially, config.delayBetweenContainers, false,
			operationId, containers, targetVehicle, config, containerIndex + 1, totalTransferred + 1, totalSkipped);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Remove empty container after transfer
	protected void RemoveEmptyContainer(IEntity container)
	{
		if (!container) return;
		
		UniversalInventoryStorageComponent storage = UniversalInventoryStorageComponent.Cast(
			container.FindComponent(UniversalInventoryStorageComponent));
		
		if (storage)
		{
			array<InventoryItemComponent> items = {};
			storage.GetOwnedItems(items);
			
			// Only remove if truly empty
			if (items.IsEmpty())
			{
				SCR_EntityHelper.DeleteEntityAndChildren(container);
			}
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Perform warehouse transfer
	protected void PerformWarehouseTransfer(string operationId, IEntity fromEntity)
	{
		OVT_StorageProgressCallback callback = m_mActiveOperations.Get(operationId);
		
		// Implement warehouse transfer logic (similar to existing TransferToWarehouse)
		// This would integrate with the real estate manager's warehouse system
		
		if (callback) callback.OnComplete(0, 0);
		m_mActiveOperations.Remove(operationId);
	}
	
	//------------------------------------------------------------------------------------------------
	//! Play transfer completion sound
	protected void PlayTransferSound(IEntity entity)
	{
		if (!entity) return;
		
		SimpleSoundComponent soundComp = SimpleSoundComponent.Cast(entity.FindComponent(SimpleSoundComponent));
		if (soundComp)
		{
			vector mat[4];
			entity.GetWorldTransform(mat);
			soundComp.SetTransformation(mat);
			soundComp.PlayStr("SOUND_SUPPLIES_PARTIAL_LOAD");
		}
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get entity display name
	protected string GetEntityDisplayName(IEntity entity)
	{
		if (!entity) return "Unknown";
		
		// Try to get name from prefab data since entities don't have GetDisplayName
		EntityPrefabData prefabData = entity.GetPrefabData();
		if (prefabData)
		{
			ResourceName prefab = prefabData.GetPrefabName();
			if (!prefab.IsEmpty())
			{
				// Extract filename from path
				int lastSlash = prefab.LastIndexOf("/");
				if (lastSlash >= 0)
					return prefab.Substring(lastSlash + 1, prefab.Length() - lastSlash - 1);
				else
					return prefab;
			}
		}
		
		return "Container";
	}
	
	//------------------------------------------------------------------------------------------------
	//! Get entity from RplId
	protected IEntity GetEntityFromRplId(RplId rplId)
	{
		RplComponent rpl = RplComponent.Cast(Replication.FindItem(rplId));
		if (rpl) return rpl.GetEntity();
		return null;
	}
	
	//------------------------------------------------------------------------------------------------
	//! Generate unique operation ID
	protected string GenerateOperationId()
	{
		return string.Format("transfer_%1_%2", GetGame().GetWorld().GetWorldTime(), Math.RandomInt(0, 99999));
	}
}