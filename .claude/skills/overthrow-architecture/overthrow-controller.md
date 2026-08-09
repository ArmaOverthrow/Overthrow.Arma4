# OVT_OverthrowController Pattern

Complete guide for the new modular controller architecture replacing the legacy OVT_PlayerCommsComponent.

---

## Overview

**Status:** ✅ Fully Implemented (v1.3.0+)

The OVT_OverthrowController is a dedicated controller entity owned by each player that houses specialized components for different features. This replaces the monolithic `OVT_PlayerCommsComponent` pattern.

**Key Benefits:**
- Modular components for each feature
- Better separation of concerns
- Built-in progress tracking support
- Easier testing and maintenance
- Cleaner network patterns

---

## Architecture

### Old Pattern (Deprecated)

❌ **Don't use OVT_PlayerCommsComponent for new features:**

```cpp
// OLD WAY - Deprecated
OVT_PlayerCommsComponent comms = OVT_Global.GetServer();
comms.SomeOperation(); // Everything through one monolithic component
```

**Problems:**
- Single 1431-line file with all client→server operations
- Mixed concerns (economy, bases, inventory, real estate, jobs, etc.)
- Difficult to test features in isolation
- No built-in progress tracking
- Poor extensibility

### New Pattern (Recommended)

✅ **Use specialized components on OVT_OverthrowController:**

```cpp
// NEW WAY - Recommended
OVT_OverthrowController controller = OVT_Global.GetController();
if (!controller) return;

OVT_ContainerTransferComponent transfer = OVT_ContainerTransferComponent.Cast(
    controller.FindComponent(OVT_ContainerTransferComponent)
);
if (!transfer) return;

transfer.TransferStorage(fromEntity, toEntity);
```

**Or use convenience methods:**

```cpp
// Even simpler
OVT_ContainerTransferComponent transfer = OVT_Global.GetContainerTransfer();
if (!transfer) return;

transfer.TransferStorage(fromEntity, toEntity);
```

**Benefits:**
- Clear separation of concerns
- Dedicated component per feature
- Built-in progress tracking
- Type-safe interfaces
- Easy to extend

---

## Controller Entity Lifecycle

### 1. Player Joins Server

**OVT_PlayerManagerComponent** spawns controller entity:

```cpp
// In OVT_PlayerManagerComponent (lines 260-287)
void OnPlayerConnected(int playerId)
{
    // Spawn controller entity for this player
    IEntity controller = SpawnControllerEntity(playerId);

    // Assign ownership
    AssignControllerOwnership(controller, playerId);

    // Register controller
    m_mControllers.Insert(playerId, controller);
}
```

### 2. Ownership Assignment

Controller entity is assigned to player via RplComponent:

```cpp
// In OVT_PlayerManagerComponent (lines 290-322)
void AssignControllerOwnership(IEntity controller, int playerId)
{
    RplComponent rpl = RplComponent.Cast(controller.FindComponent(RplComponent));
    if (!rpl) return;

    // Assign player as owner
    rpl.Give(playerId);

    // Notify client
    Rpc(RpcDo_NotifyControllerAssigned, playerId);
}
```

### 3. Player Uses Controller

Client accesses owned controller:

```cpp
// Client-side
OVT_OverthrowController controller = OVT_Global.GetController();
if (!controller) return; // Not available or we're on server

// Access components
OVT_SomeComponent component = OVT_SomeComponent.Cast(
    controller.FindComponent(OVT_SomeComponent)
);
```

### 4. Player Disconnects

**OVT_PlayerManagerComponent** cleans up:

```cpp
// In OVT_PlayerManagerComponent (lines 328-372)
void OnPlayerDisconnected(int playerId)
{
    // Get controller
    IEntity controller = m_mControllers.Get(playerId);
    if (!controller) return;

    // Cleanup and delete
    SCR_EntityHelper.DeleteEntityAndChildren(controller);
    m_mControllers.Remove(playerId);
}
```

---

## Creating Controller Components

### Base Pattern

```cpp
//! Component on OVT_OverthrowController for [feature description]
class OVT_FeatureComponentClass: OVT_ComponentClass {};

class OVT_FeatureComponent: OVT_Component
{
    //-----------------------------------------------------------------------
    // CLIENT→SERVER REQUESTS
    //-----------------------------------------------------------------------

    //! Client requests an operation
    void RequestOperation(int param)
    {
        // Check if we're already server (host scenario)
        if (Replication.IsServer())
        {
            RpcAsk_Operation(param);
        }
        else
        {
            Rpc(RpcAsk_Operation, param);
        }
    }

    [RplRpc(RplChannel.Reliable, RplRcver.Server)]
    protected void RpcAsk_Operation(int param)
    {
        // Validate request (never trust client)
        if (!ValidateRequest(param)) return;

        // Process on server
        ProcessOperation(param);

        // Optionally send result back
        Rpc(RpcDo_OperationResult, true);
    }

    [RplRpc(RplChannel.Reliable, RplRcver.Owner)]
    protected void RpcDo_OperationResult(bool success)
    {
        // Client receives result
        if (success)
        {
            // Handle success
        }
    }

    //-----------------------------------------------------------------------
    // SERVER LOGIC
    //-----------------------------------------------------------------------

    protected bool ValidateRequest(int param)
    {
        // Server-side validation
        return param >= 0 && param <= 100;
    }

    protected void ProcessOperation(int param)
    {
        // Server-side processing
    }
}
```

### Client Access Pattern

```cpp
// In user action or UI context
void SomeClientMethod()
{
    // Get local controller
    OVT_OverthrowController controller = OVT_Global.GetController();
    if (!controller) return; // We're on dedicated server

    // Get component
    OVT_FeatureComponent component = OVT_FeatureComponent.Cast(
        controller.FindComponent(OVT_FeatureComponent)
    );
    if (!component) return; // Component not registered

    // Call method (will RPC to server)
    component.RequestOperation(someValue);
}
```

### OVT_Global Convenience Method

Add to `OVT_Global.c`:

```cpp
static OVT_FeatureComponent GetFeature()
{
    OVT_OverthrowController controller = GetController();
    if (!controller) return null;

    return OVT_FeatureComponent.Cast(
        controller.FindComponent(OVT_FeatureComponent)
    );
}
```

**Usage:**

```cpp
OVT_FeatureComponent feature = OVT_Global.GetFeature();
if (!feature) return;

feature.RequestOperation(value);
```

---

## Progress Tracking System

### OVT_BaseServerProgressComponent

Base class for operations that need progress tracking.

**Features:**
- Automatic progress UI integration
- Client callbacks for progress updates
- Error handling and reporting
- Operation state management

**Example: Extending for New Feature**

```cpp
//! Component for [feature] with progress tracking
class OVT_NewFeatureComponentClass: OVT_ComponentClass {};

class OVT_NewFeatureComponent: OVT_BaseServerProgressComponent
{
    //! Start a long-running operation
    void StartOperation(IEntity entity)
    {
        if (Replication.IsServer())
        {
            RpcAsk_StartOperation(Replication.FindId(entity));
        }
        else
        {
            Rpc(RpcAsk_StartOperation, Replication.FindId(entity));
        }
    }

    [RplRpc(RplChannel.Reliable, RplRcver.Server)]
    protected void RpcAsk_StartOperation(RplId entityId)
    {
        // Notify start
        Rpc(RpcDo_OperationStart, "Processing Items");

        // Get entity
        IEntity entity = GetEntityFromRplId(entityId);
        if (!entity) return;

        // Process with progress updates
        ProcessWithProgress(entity);
    }

    protected void ProcessWithProgress(IEntity entity)
    {
        int totalItems = GetItemCount(entity);
        int processed = 0;

        // Process items
        for (int i = 0; i < totalItems; i++)
        {
            // Do work
            ProcessItem(i);

            // Update progress
            processed++;
            float progress = (processed / (float)totalItems) * 100.0;

            // Send progress update to client
            Rpc(RpcDo_UpdateProgress, progress, processed, totalItems);
        }

        // Complete
        Rpc(RpcDo_OperationComplete, processed, 0);
    }
}
```

### Progress Event Integration

**OVT_ProgressEventHandler** on controller provides global progress events:

```cpp
class OVT_OverthrowController : GenericEntity
{
    protected ref OVT_ProgressEventHandler m_ProgressEvents;

    OVT_ProgressEventHandler GetProgressEvents()
    {
        return m_ProgressEvents;
    }
}
```

**Progress events automatically trigger UI:**

1. Client calls operation on controller component
2. Component extends `OVT_BaseServerProgressComponent`
3. Server calls `Rpc(RpcDo_OperationStart, "Operation Name")`
4. Base class invokes `controller.GetProgressEvents().InvokeStart("Operation Name")`
5. `OVT_ProgressInfo` UI widget subscribes to progress events
6. Progress dialog shows automatically
7. Updates display on `RpcDo_UpdateProgress`
8. Hides on `RpcDo_OperationComplete` or `RpcDo_OperationError`

**No UI code needed in your component!**

---

## Reference Implementation: Container Transfer

**OVT_ContainerTransferComponent** is the reference implementation.

**Location:** `Scripts/Game/Components/Controller/OVT_ContainerTransferComponent.c`

**Operations implemented:**
1. `TransferStorage` - Basic container transfer
2. `TransferStorageForDeployment` - FOB deployment
3. `CollectContainers` - Area collection
4. `TransferToWarehouse` - Warehouse transfers
5. `UndeployFOBWithCollection` - FOB undeployment
6. `LootBattlefield` - Battlefield looting

**Key patterns demonstrated:**

### 1. Client Request Pattern

```cpp
void TransferStorage(IEntity from, IEntity to, bool deleteEmpty = false)
{
    // Convert to RplIds
    RplId fromId = Replication.FindId(from);
    RplId toId = Replication.FindId(to);

    // Check if server (host scenario)
    if (Replication.IsServer())
    {
        RpcAsk_TransferStorage(fromId, toId, deleteEmpty);
    }
    else
    {
        Rpc(RpcAsk_TransferStorage, fromId, toId, deleteEmpty);
    }
}
```

### 2. Server Processing with Progress

```cpp
[RplRpc(RplChannel.Reliable, RplRcver.Server)]
protected void RpcAsk_TransferStorage(RplId fromId, RplId toId, bool deleteEmpty)
{
    // Notify client of start
    Rpc(RpcDo_OperationStart, "Transferring Items");

    // Get entities from RplIds
    IEntity from = GetEntityFromRplId(fromId);
    IEntity to = GetEntityFromRplId(toId);

    // Create callback bridge
    OVT_ContainerTransferCallback callback = new OVT_ContainerTransferCallback(this);

    // Perform operation (inventory manager calls back with progress)
    OVT_Global.GetInventory().TransferStorage(from, to, callback, deleteEmpty);
}
```

### 3. Callback Bridge Pattern

```cpp
class OVT_ContainerTransferCallback : OVT_InventoryOperationCallback
{
    protected OVT_ContainerTransferComponent m_Component;

    void OVT_ContainerTransferCallback(OVT_ContainerTransferComponent component)
    {
        m_Component = component;
    }

    override void OnProgress(float progress, int current, int total)
    {
        // Forward to progress component
        m_Component.Rpc(
            m_Component.RpcDo_UpdateProgress,
            progress,
            current,
            total
        );
    }

    override void OnComplete(int transferred, int skipped)
    {
        m_Component.Rpc(
            m_Component.RpcDo_OperationComplete,
            transferred,
            skipped
        );
    }

    override void OnError(string error)
    {
        m_Component.Rpc(
            m_Component.RpcDo_OperationError,
            error
        );
    }
}
```

### 4. Busy State Management

```cpp
protected bool m_bOperationInProgress = false;

bool IsAvailable()
{
    return !m_bOperationInProgress;
}

protected void StartOperation()
{
    m_bOperationInProgress = true;
}

protected void EndOperation()
{
    m_bOperationInProgress = false;
}
```

---

## Migration Guide

### Migrating from OVT_PlayerCommsComponent

**Before (Legacy):**
```cpp
OVT_PlayerCommsComponent comms = OVT_Global.GetServer();
comms.Buy(shop, itemId, quantity, playerId);
```

**After (New Pattern):**
```cpp
// 1. Create OVT_ShopComponent on controller
OVT_ShopComponent shops = OVT_Global.GetShops();
if (!shops) return;

shops.PurchaseItem(shop, itemId, quantity);
```

**Steps:**
1. Create new component extending `OVT_Component` or `OVT_BaseServerProgressComponent`
2. Place on `OVT_OverthrowController` prefab in Workbench
3. Implement RpcAsk/RpcDo methods
4. Add convenience accessor to `OVT_Global`
5. Update call sites to use new component
6. Test thoroughly
7. Remove old method from `OVT_PlayerCommsComponent`

---

## Best Practices

### ✅ DO:

- **Extend OVT_BaseServerProgressComponent** for long-running operations
- **Validate all client requests** on server (never trust client)
- **Use RplId for entity references** across network
- **Check Replication.IsServer()** before RPC to avoid unnecessary network calls on host
- **Add convenience methods to OVT_Global** for frequently used components
- **Document what the component manages** in class header
- **Provide clear operation methods** with descriptive names
- **Handle errors gracefully** with RpcDo_OperationError

### ❌ DON'T:

- **Add new methods to OVT_PlayerCommsComponent** - it's deprecated
- **Skip server-side validation** - always validate client requests
- **Use EntityID across network** - use RplId instead
- **Forget null checks** - controller may not exist or component may not be registered
- **Block the server** - long operations should use callbacks or CallLater
- **Skip progress updates** for operations > 1 second
- **Assume component exists** - always check FindComponent() result

---

## Testing Controller Components

### Manual Testing Checklist

1. **Compile in Workbench** - Check for errors
2. **Start as server** (play mode)
3. **Second player joins**
4. **Server triggers operation** via component
5. **Client sees progress** UI automatically
6. **Operation completes** successfully
7. **Client triggers operation** via component
8. **Server processes** and validates
9. **Progress updates** display correctly
10. **Error handling** works (invalid params, etc.)
11. **Player disconnects** mid-operation - no errors
12. **Controller cleanup** on disconnect verified

### Test Scenarios

**Scenario 1: Basic Operation**
```
1. Client calls: component.RequestOperation(value)
2. Verify: RPC sent to server
3. Verify: Server validates request
4. Verify: Server processes
5. Verify: Result sent back to client
```

**Scenario 2: Progress Operation**
```
1. Client calls: component.TransferItems(from, to)
2. Verify: Progress dialog appears
3. Verify: Progress updates (0%, 25%, 50%, 75%, 100%)
4. Verify: Item counts shown
5. Verify: Dialog hides on complete
```

**Scenario 3: Error Handling**
```
1. Client calls: component.RequestOperation(invalidValue)
2. Verify: Server validates and rejects
3. Verify: Error shown to client
4. Verify: No server error messages
```

---

## Current Controller Components

### Implemented

1. **OVT_ContainerTransferComponent** ✅
   - Container transfers with progress
   - FOB deployment/undeployment
   - Area container collection
   - Battlefield looting
   - Warehouse transfers

### Planned for Migration

From `OVT_PlayerCommsComponent` (legacy):
- Economy operations (buy/sell, money management)
- Base management (garrison, capture)
- Real estate (homes, buildings, rent)
- Job management (accept/decline)
- Notifications
- Shop operations
- Placement/building
- Fast travel
- Loadout management

---

## Summary

The OVT_OverthrowController pattern provides a clean, modular architecture for client-server communication:

- ✅ **One controller per player** - automatic lifecycle management
- ✅ **Specialized components** - clear separation of concerns
- ✅ **Built-in progress tracking** - automatic UI integration
- ✅ **Type-safe interfaces** - FindComponent with casting
- ✅ **Easy testing** - isolated component testing
- ✅ **Better organization** - features in dedicated files

**Use this pattern for all new client→server operations.**

---

## Related Resources

- See `component-patterns.md` for base component patterns
- See `global-access.md` for OVT_Global accessor patterns
- See `networking.md` in enforcescript-patterns for RPC details
- See `docs/OverthrowController.md` for architecture overview
- See `Scripts/Game/Components/Controller/OVT_ContainerTransferComponent.c` for reference implementation
