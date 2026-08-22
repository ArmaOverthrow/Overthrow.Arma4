# OVT_OverthrowController Pattern

Complete guide for the modular controller architecture that replaced the legacy comms monolith.

**As of 2026-08-14 there is no other client→server seam.** `core/controller-migration` deleted
`OVT_PlayerCommsComponent` and `OVT_Global.GetServer()` outright and stripped the component from both
prefabs that carried it. If you are reading a doc, a comment or a memory that tells you to add an RPC to
the comms component, that text is stale - the class does not exist.

---

## Overview

**Status:** ✅ Fully Implemented - and since 2026-08-14, the ONLY client→server seam (17 components).

The OVT_OverthrowController is a dedicated controller entity owned by each player that houses specialized components for different features. It replaced the monolithic comms component, which is deleted.

**Key Benefits:**
- Modular components for each feature
- Better separation of concerns
- Built-in progress tracking support
- Easier testing and maintenance
- Cleaner network patterns

---

## Architecture

### Old Pattern (DELETED 2026-08-14 - shown so you recognise it in stale docs)

❌ **This does not compile any more. Neither identifier exists:**

```cpp
// OLD WAY - the class and the accessor were both deleted in P10 of core/controller-migration
OVT_PlayerCommsComponent comms = OVT_Global.GetServer();
comms.SomeOperation(); // Everything through one monolithic component
```

**Problems it had:**
- Single 2,001-line file with all client→server operations (55 `RpcAsk_` + 4 `RpcDo_`)
- **It sat on the player CHARACTER**, so the seam died with the body and `RplRcver.Owner` meant
  "whoever controls this body" rather than "this player"
- Every handler took a client-supplied `playerId`/`persistentId` and laundered it; nine endpoints had
  no validation at all (dismiss any recruit, possess any entity, delete any camp, ...)
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

**Or use the generic accessor (preferred - one line, no Cast):**

```cpp
// Even simpler - THE way to reach a controller component
OVT_ContainerTransferComponent transfer = OVT_ControllerComponent<OVT_ContainerTransferComponent>.Get();
if (!transfer) return;

transfer.TransferStorage(fromEntity, toEntity);
```

`OVT_ControllerComponent<Class T>.Get()`
(`Scripts/Game/Components/Controller/OVT_ControllerComponent.c`) resolves any
component on the **local** player's controller. It is a generic CLASS with a
static because EnforceScript has no generic methods - the same trick
`OVT_ComponentFinder<Class T>` uses, which it composes with.

**There is no `OVT_Global` getter for a controller component, and none is ever
added.** The six that used to exist (`GetContainerTransfer`, `GetShopTransactions`,
`GetTowerSabotage`, `GetTravelRequests`, `GetRespawnRequests`, `GetTutorials`)
were deleted in `core/controller-migration` Phase 1 and replaced by this accessor.
`OVT_Global` is a service locator for **managers**; a controller component is not
a manager.

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

### Migrating from the legacy comms monolith

**This migration is COMPLETE** (`core/controller-migration`, 2026-08-14). The guide is kept because the
shape is exactly what you follow when adding a NEW component - steps 1-7 are the live checklist.

**Before (Legacy - no longer compiles):**
```cpp
OVT_PlayerCommsComponent comms = OVT_Global.GetServer();
comms.Buy(shop, itemId, quantity, playerId);
```

**After (New Pattern):**
```cpp
// The component lives on OVT_OverthrowController and is reached generically
OVT_ShopTransactionComponent shop = OVT_ControllerComponent<OVT_ShopTransactionComponent>.Get();
if (!shop) return;

shop.PurchaseItem(shopEntity, itemId, quantity);
```

**Steps:**
1. Create new component in `Scripts/Game/Components/Controller/`, extending
   `OVT_ControllerRequestComponent` (server-authoritative requests - gives you
   `ResolveOwningPlayerId()`, `ResolveEntity()`, `GetEntityRpl()` and
   `ShouldRespondLocally()`) or `OVT_BaseServerProgressComponent` (long operations
   that report progress). Its `...ComponentClass` must extend the matching base
   `...ComponentClass`.
2. Place on `OVT_OverthrowController` prefab in Workbench (fresh GUID)
3. Implement RpcAsk/RpcDo methods. **Resolve the caller from
   `ResolveOwningPlayerId()`, never from a client-supplied player id** - migrated
   RPCs drop the identity parameter from the signature entirely
4. Reach it with `OVT_ControllerComponent<T>.Get()`. **Do NOT add an accessor to
   `OVT_Global`** - no controller component gets one; the generic accessor is the
   whole API, and its name is contract (`core/options`, `core/player-groups`)
5. Update call sites to use the new component - every one of them null-checks,
   because the accessor is null on a dedicated server and before ownership assignment
6. Add an Init-tier test case asserting the component resolves off a registered
   controller (`OVT_TEST_Init_Controller_ComponentsResolve` is the pattern). A
   component that was never added to the prefab produces no compile error and no
   runtime error - this case is the only thing that catches it
7. Test thoroughly. **`Rpc()` arity is a compile-check blind spot** - it is an untyped variadic
   prototype, so a wrong argument count compiles clean and dies silently at the wire. `grep -n
   "Rpc(Rpc" <yourfile>` and hand-check every call against its handler, including the
   `Replication.IsServer()` direct-call twin (only one half of that pair is type-checked)
8. **Branch every public entry point on `Replication.IsServer()`** - an `RplRcver.Server` RPC
   marshalled BY the authority is delivered to nobody, so an unconditional `Rpc()` is a silent no-op
   for a listen-server host. Owner responses use `ShouldRespondLocally()` for the same reason

---

## Best Practices

### ✅ DO:

- **Extend OVT_BaseServerProgressComponent** for long-running operations
- **Validate all client requests** on server (never trust client)
- **Use RplId for entity references** across network
- **Check Replication.IsServer()** before RPC to avoid unnecessary network calls on host
- **Reach components with `OVT_ControllerComponent<T>.Get()`** - never add an `OVT_Global` getter for one
- **Document what the component manages** in class header
- **Provide clear operation methods** with descriptive names
- **Handle errors gracefully** with RpcDo_OperationError

### ❌ DON'T:

- **Look for a legacy comms component to add an RPC to** - there isn't one; it was deleted 2026-08-14
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

### Implemented - all 17, in `Scripts/Game/Components/Controller/`

The twelve that carry the bulk of the request surface:

| Component | Handles |
|---|---|
| `OVT_ContainerTransferComponent` | container transfers with progress, FOB deploy/undeploy transfer, area collection, battlefield looting, warehouse transfers |
| `OVT_ShopTransactionComponent` | shop buying AND selling, vehicle-cargo selling (both halves share the 30 m gate and the price model on purpose) |
| `OVT_VehicleRequestComponent` | lock/unlock, claim unowned, upgrade, repair, import-to-vehicle, buy-vehicle |
| `OVT_RealEstateRequestComponent` | set-home, buy/sell/rent/stop-renting a building, the three warehouse movements |
| `OVT_EconomyRequestComponent` | drug selling, donate, send resistance funds, send money, take player money, resistance tax, buy skill |
| `OVT_ResistanceRequestComponent` | place, remove placed, build, add officer, add base garrison, convert supporter |
| `OVT_FOBRequestComponent` | camp/FOB garrison, deploy, undeploy, set priority, camp privacy, delete camp |
| `OVT_RecruitRequestComponent` | recruit civilian, recruit from tent, rename, dismiss |
| `OVT_LoadoutRequestComponent` | save a loadout, apply one from an equipment box, delete one |
| `OVT_PossessionRequestComponent` | possess a recruit and open its inventory, plus the whole close/restore lifecycle |
| `OVT_JobRequestComponent` | accept a job, decline a job |
| `OVT_CampaignRequestComponent` | start a base capture, deliver medical supplies, loot wanted check, request save |

Plus `OVT_TravelRequestComponent`, `OVT_RespawnRequestComponent`, `OVT_TowerSabotageComponent`,
`OVT_TutorialComponent` and `OVT_AdminCommandsComponent`; the shared bases
`OVT_ControllerRequestComponent` / `OVT_BaseServerProgressComponent`; and the generic accessor
`OVT_ControllerComponent<Class T>.Get()`.

### Planned for Migration

**Nothing.** The migration finished 2026-08-14 and the monolith is deleted.

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
