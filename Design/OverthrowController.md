# Overthrow Controller Architecture Design

## Overview

The Overthrow Controller system is a new architecture designed to replace the current `OVT_PlayerCommsComponent` approach. Instead of having all server communication through a single component on the player entity, we will use dedicated controller entities owned by each player with modular components for specific functionality.

## Current Architecture Problems

1. **Single Point of Congestion**: All server communication goes through `OVT_PlayerCommsComponent`
2. **Monolithic Design**: One component handles everything from economy to bases to inventory
3. **Limited Extensibility**: Adding new features requires modifying the same component
4. **Poor Separation of Concerns**: Mixing different domains in one place
5. **Difficult to Test**: Can't test individual features in isolation

## New Architecture Benefits

1. **Modular Components**: Each feature has its own dedicated component
2. **Better Organization**: Clear separation of concerns
3. **Easier Testing**: Components can be tested independently
4. **Network Efficiency**: Only relevant components need to replicate data
5. **Type Safety**: Each component can have strongly-typed interfaces
6. **Progress Tracking**: Built-in support for long-running operations

## Core Components

### OVT_OverthrowController

The base entity that each player owns. This is spawned by `OVT_PlayerManagerComponent` when a player joins.

```cpp
[EntityEditorProps(category: "Overthrow", description: "Controller entity for overthrow-specific client-server communication")]
class OVT_OverthrowControllerClass : GenericEntityClass
{
}

class OVT_OverthrowController : GenericEntity
{
    // Controller will have various components attached
}
```

### OVT_BaseServerProgressComponent

Base class for any server operation that takes time and needs progress reporting.

```cpp
class OVT_BaseServerProgressComponentClass : OVT_ComponentClass {};

class OVT_BaseServerProgressComponent : OVT_Component
{
    // Progress tracking
    protected float m_fProgress = 0.0;
    protected int m_iItemsProcessed = 0;
    protected int m_iTotalItems = 0;
    protected string m_sCurrentOperation = "";
    protected bool m_bIsRunning = false;
    
    // Client-side progress callbacks
    ref ScriptInvoker m_OnProgressUpdate = new ScriptInvoker();
    ref ScriptInvoker m_OnOperationComplete = new ScriptInvoker();
    ref ScriptInvoker m_OnOperationError = new ScriptInvoker();
    
    // Progress update methods (called from server)
    [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
    void RpcDo_UpdateProgress(float progress, int current, int total, string operation)
    {
        m_fProgress = progress;
        m_iItemsProcessed = current;
        m_iTotalItems = total;
        m_sCurrentOperation = operation;
        m_OnProgressUpdate.Invoke(progress, current, total, operation);
    }
    
    [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
    void RpcDo_OperationComplete(int itemsTransferred, int itemsSkipped)
    {
        m_bIsRunning = false;
        m_OnOperationComplete.Invoke(itemsTransferred, itemsSkipped);
    }
    
    [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
    void RpcDo_OperationError(string errorMessage)
    {
        m_bIsRunning = false;
        m_OnOperationError.Invoke(errorMessage);
    }
    
    // Getters for current state
    float GetProgress() { return m_fProgress; }
    bool IsRunning() { return m_bIsRunning; }
    string GetCurrentOperation() { return m_sCurrentOperation; }
}
```

### OVT_ContainerTransferComponent

Handles all container transfer operations with progress tracking.

```cpp
class OVT_ContainerTransferComponentClass : OVT_BaseServerProgressComponentClass {};

class OVT_ContainerTransferComponent : OVT_BaseServerProgressComponent
{
    // Transfer single container
    void TransferStorage(IEntity from, IEntity to, bool deleteEmpty = false)
    {
        RplComponent fromRpl = RplComponent.Cast(from.FindComponent(RplComponent));
        RplComponent toRpl = RplComponent.Cast(to.FindComponent(RplComponent));
        
        if (!fromRpl || !toRpl) return;
        
        Rpc(RpcAsk_TransferStorage, fromRpl.Id(), toRpl.Id(), deleteEmpty);
    }
    
    [RplRpc(RplChannel.Reliable, RplRcver.Server)]
    void RpcAsk_TransferStorage(RplId fromId, RplId toId, bool deleteEmpty)
    {
        // Use OVT_InventoryManagerComponent for the actual transfer
        OVT_StorageOperationConfig config = new OVT_StorageOperationConfig(
            false,      // showProgressBar (we handle UI ourselves)
            deleteEmpty,
            false,      // deleteEmptyContainers
            50,         // itemsPerBatch
            100,        // batchDelayMs
            -1,         // searchRadius (not needed for direct transfer)
            1           // maxBatchesPerFrame
        );
        
        // Create a callback wrapper to send progress to client
        OVT_ContainerTransferCallback callback = new OVT_ContainerTransferCallback(this);
        
        OVT_Global.GetInventory().TransferStorageByRplId(fromId, toId, config, callback);
    }
    
    // Collect containers in area
    void CollectContainers(vector pos, IEntity targetVehicle, float radius = 75.0)
    {
        RplComponent vehicleRpl = RplComponent.Cast(targetVehicle.FindComponent(RplComponent));
        if (!vehicleRpl) return;
        
        Rpc(RpcAsk_CollectContainers, pos, vehicleRpl.Id(), radius);
    }
    
    [RplRpc(RplChannel.Reliable, RplRcver.Server)]
    void RpcAsk_CollectContainers(vector pos, RplId vehicleId, float radius)
    {
        IEntity vehicle = GetEntityFromRplId(vehicleId);
        if (!vehicle) return;
        
        OVT_StorageOperationConfig config = new OVT_StorageOperationConfig(
            false,      // showProgressBar
            true,       // skipWeaponsOnGround
            true,       // deleteEmptyContainers
            100,        // itemsPerBatch
            300,        // batchDelayMs
            radius,     // searchRadius
            3           // maxBatchesPerFrame
        );
        
        OVT_ContainerTransferCallback callback = new OVT_ContainerTransferCallback(this);
        OVT_Global.GetInventory().CollectContainersToVehicle(pos, vehicle, config, callback);
    }
    
    // Transfer to warehouse
    void TransferToWarehouse(IEntity from)
    {
        RplComponent fromRpl = RplComponent.Cast(from.FindComponent(RplComponent));
        if (!fromRpl) return;
        
        Rpc(RpcAsk_TransferToWarehouse, fromRpl.Id());
    }
    
    [RplRpc(RplChannel.Reliable, RplRcver.Server)]
    void RpcAsk_TransferToWarehouse(RplId fromId)
    {
        // This will use the existing warehouse transfer logic
        OVT_Global.TransferToWarehouse(fromId);
        
        // Send completion immediately as warehouse transfers are instant
        Rpc(RpcDo_OperationComplete, 1, 0);
    }
}

// Callback class to bridge inventory manager callbacks to RPC updates
class OVT_ContainerTransferCallback : OVT_StorageProgressCallback
{
    protected OVT_ContainerTransferComponent m_Component;
    
    void OVT_ContainerTransferCallback(OVT_ContainerTransferComponent component)
    {
        m_Component = component;
    }
    
    override void OnProgressUpdate(float progress, int currentItem, int totalItems, string operation)
    {
        if (m_Component)
            m_Component.Rpc(m_Component.RpcDo_UpdateProgress, progress, currentItem, totalItems, operation);
    }
    
    override void OnComplete(int itemsTransferred, int itemsSkipped)
    {
        if (m_Component)
            m_Component.Rpc(m_Component.RpcDo_OperationComplete, itemsTransferred, itemsSkipped);
    }
    
    override void OnError(string errorMessage)
    {
        if (m_Component)
            m_Component.Rpc(m_Component.RpcDo_OperationError, errorMessage);
    }
}
```

## Client Access Pattern

Update `OVT_Global` to provide easy access to the controller:

```cpp
class OVT_Global : Managed
{
    // Get the local player's controller
    static OVT_OverthrowController GetController()
    {
        if (Replication.IsServer())
        {
            // Server doesn't have a controller
            return null;
        }
        
        IEntity player = SCR_PlayerController.GetLocalControlledEntity();
        if (!player) return null;
        
        int playerId = GetGame().GetPlayerManager().GetPlayerIdFromControlledEntity(player);
        return OVT_Global.GetPlayers().GetController(playerId);
    }
    
    // Get specific component from controller
    static T GetControllerComponent<T>()
    {
        OVT_OverthrowController controller = GetController();
        if (!controller) return null;
        
        return T.Cast(controller.FindComponent(T));
    }
    
    // Convenience method for container transfers
    static OVT_ContainerTransferComponent GetContainerTransfer()
    {
        return GetControllerComponent<OVT_ContainerTransferComponent>();
    }
}
```

## Migration Example

Current code:
```cpp
OVT_PlayerCommsComponent comms = OVT_Global.GetServer();
comms.TransferStorage(from, to);
```

New code:
```cpp
OVT_ContainerTransferComponent transfer = OVT_Global.GetContainerTransfer();
transfer.TransferStorage(from, to);
```

## Implementation Plan

### Phase 1: Foundation
1. ✅ Add controller prefab reference to PlayerManager
2. ✅ Implement controller spawning and cleanup
3. ✅ Add GetController methods to PlayerManager
4. Create OVT_OverthrowController prefab with RplComponent
5. Update OVT_Global with new access methods

### Phase 2: Base Progress Component
1. Implement OVT_BaseServerProgressComponent
2. Add progress tracking infrastructure
3. Create UI context for progress display
4. Test with simple operations

### Phase 3: Container Transfer Component
1. Implement OVT_ContainerTransferComponent
2. Integrate with OVT_InventoryManagerComponent
3. Add callback bridging
4. Update FOB deployment/undeployment to use new system

### Phase 4: Migration
1. Move other operations from PlayerCommsComponent to dedicated components:
   - OVT_EconomyControllerComponent (money, shops)
   - OVT_BaseControllerComponent (garrisons, base capture)
   - OVT_RealEstateControllerComponent (homes, building ownership)
   - OVT_JobControllerComponent (job management)
   - OVT_NotificationControllerComponent (notifications)
2. Update all UI contexts to use new components
3. Deprecate PlayerCommsComponent

## Future Components

The modular architecture allows easy addition of new components:
- OVT_CraftingComponent - Handle crafting operations
- OVT_TradeComponent - Player-to-player trading
- OVT_MissionComponent - Mission-specific operations
- OVT_StatisticsComponent - Track player statistics

## Benefits for Container Transfers

The new system specifically improves container transfers by:
1. **Progress Tracking**: Real-time updates on transfer progress
2. **Batch Processing**: Efficient handling of large transfers
3. **Error Handling**: Proper error reporting to clients
4. **Cancellation**: Ability to cancel long-running operations
5. **Multiple Operations**: Queue multiple transfers
6. **UI Integration**: Built-in support for progress dialogs

## Security Considerations

1. All operations validate player ownership of controller
2. Server-side validation of all requests
3. Rate limiting for expensive operations
4. Proper cleanup on player disconnect
5. No direct entity manipulation from client