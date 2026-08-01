# Vanilla Arma Reforger Persistence System - Research

**Last Updated:** 2025-11-09
**Research Status:** Complete
**Author:** Research conducted via automated codebase exploration

---

## Executive Summary

Arma Reforger now includes a native C++ persistence system with EnforceScript bindings, developed by the same author who created EPF (Enfusion Persistence Framework). This system offers automatic serialization, configuration-driven persistence rules, and better integration with the Enfusion engine.

**Key Advantages over EPF:**
- Native C++ performance with script bindings
- Automatic tracking via configuration rules
- Built-in UUID-based entity references with async resolution
- No console platform guards required (handled internally)
- Native support for collections, arrays, maps
- Cleaner API with less boilerplate

---

## Architecture Overview

### Core System Classes

#### **PersistenceSystem** (Singleton)
**Location:** `Scripts/Game/generated/Systems/Persistence/PersistenceSystem.c`

The main system interface accessed via `PersistenceSystem.GetInstance()`.

**Key Operations:**
- `TriggerSave(ESaveGameType saveType)` - Trigger global save
- `Save(Managed entityOrState, ESaveGameType saveType)` - Save specific instance
- `StartTracking(Managed entityOrState)` - Register for persistence
- `StopTracking(Managed entityOrState)` - Unregister from persistence
- `PauseTracking()/ResumeTracking()` - Temporarily pause/resume tracking
- `ReleaseTracking()` - Memory optimization - release tracker but keep data
- `GetId(Managed entityOrState)` - Get UUID of tracked instance
- `SetId(Managed entityOrState, UUID id)` - Assign UUID
- `FindById(UUID id)` - Find tracked instance by UUID
- `WhenAvailable(UUID uuid, task, maxWaitSeconds)` - Async dependency resolution
- `GetConfig()/SetConfig()/ReloadConfig()` - Dynamic configuration management

#### **SCR_PersistenceSystem** (Scripted Extension)
**Location:** `Scripts/Game/Systems/Persistence/SCR_PersistenceSystem.c`

Adds script invokers for lifecycle events:
- `GetOnStateChanged()` - System state lifecycle
- `GetOnBeforeSave()` - Pre-save hook
- `GetOnAfterSave()` - Post-save hook with success status
- `IsLoadInProgress()` - Suppress UI/sound during load

---

### Serializer Architecture

All serializers inherit from `PersistenceSerializerBase` with type-specific implementations:

#### **ScriptedComponentSerializer** (for GenericComponent)

**Required Implementation:**
```enforcescript
class MyComponentSerializer : ScriptedComponentSerializer
{
    // REQUIRED: Declare what type this serializer handles
    override static typename GetTargetType()
    {
        return MyComponent;
    }

    // Optional: Control when deserialization happens
    override static EComponentDeserializeEvent GetDeserializeEvent()
    {
        return EComponentDeserializeEvent.AFTER_ENTITY_FINALIZE; // Default
    }

    // Serialize component data
    override protected ESerializeResult Serialize(
        IEntity owner,
        GenericComponent component,
        BaseSerializationSaveContext context)
    {
        // Write data
        return ESerializeResult.OK;
    }

    // Deserialize component data
    override protected bool Deserialize(
        IEntity owner,
        GenericComponent component,
        BaseSerializationLoadContext context)
    {
        // Read data
        return true;
    }
}
```

**Deserialize Timing Options:**
- `BEFORE_POSTINIT` - Apply before OnPostInit
- `BEFORE_EONINIT` - Apply before EOnInit
- `AFTER_ENTITY_FINALIZE` - Default, after entity fully constructed

#### **ScriptedEntitySerializer** (for IEntity)

**Required Implementation:**
```enforcescript
class MyEntitySerializer : ScriptedEntitySerializer
{
    override static typename GetTargetType()
    {
        return MyEntity;
    }

    // Serialize entity spawn data (prefab + spawn params)
    override protected ESerializeResult SerializeSpawnData(
        IEntity entity,
        BaseSerializationSaveContext context,
        out EntitySpawnParams defaultData)
    {
        ResourceName prefab = entity.GetPrefabData().GetPrefabName();
        EntitySpawnParams params();
        entity.GetTransform(params.Transform);

        context.Write(prefab);
        context.Write(params);
        return ESerializeResult.OK;
    }

    // Deserialize spawn data
    override protected bool DeserializeSpawnData(
        out ResourceName prefab,
        out EntitySpawnParams params,
        BaseSerializationLoadContext context)
    {
        context.Read(prefab);
        context.Read(params);
        return true;
    }

    // Serialize entity state (after spawn)
    override protected ESerializeResult Serialize(
        IEntity entity,
        BaseSerializationSaveContext context)
    {
        // Write state data
        return ESerializeResult.OK;
    }

    // Deserialize entity state
    override protected bool Deserialize(
        IEntity entity,
        BaseSerializationLoadContext context)
    {
        // Read state data
        return true;
    }
}
```

#### **ScriptedStateSerializer** (for PersistentState - Non-Entity Global State)

**Usage Pattern:**
```enforcescript
// 1. Define a proxy state class
class MySystemData : PersistentState {}

// 2. Create serializer for it
class MySystemSerializer : ScriptedStateSerializer
{
    override static typename GetTargetType()
    {
        return MySystemData;
    }

    override protected ESerializeResult Serialize(
        Managed instance,
        BaseSerializationSaveContext context)
    {
        // Access actual system
        MySystem system = MySystem.GetInstance();

        // Write system state
        context.WriteValue("version", 1);
        context.Write(system.GetData());

        return ESerializeResult.OK;
    }

    override protected bool Deserialize(
        Managed instance,
        BaseSerializationLoadContext context)
    {
        int version;
        context.Read(version);

        // Restore system state
        MySystem system = MySystem.GetInstance();
        // ... read and apply

        return true;
    }
}
```

---

### Configuration System

#### **PersistenceConfig**
Controls how instances are persisted.

**Properties:**
- `PersistenceCollection m_Collection` - Which collection to store in
- `ESaveGameType m_eSaveMask` - Which save types to activate for (MANUAL/AUTO/SHUTDOWN)
- `bool m_bSelfDelete` - Delete database record when instance destroyed

#### **EntityPersistenceConfig**
Extended config for entities.

**Additional Properties:**
- `bool m_bSelfSpawn` - Auto-spawn on load
- `bool m_bStorageRoot` - Can be root record (vs child-only)
- `EPersistenceParentHandling m_eParentHandling` - How to handle parent entities:
  - `ACCEPT` - No root record if parent present
  - `IGNORE_UNTRACKED` - Ignore parents not tracked
  - `IGNORE_LOADED` - Ignore world-loaded parents
  - `IGNORE` - Always create root record

#### **PersistenceCollection**
Groups related persistent data (e.g., "Player", "Character", "Vehicle").

**Access:**
```enforcescript
PersistenceCollection collection = PersistenceSystem.GetInstance().FindCollection("MyCollection");
```

---

### Save Types

**ESaveGameType** (enum flags):
- `MANUAL` - Player-initiated saves
- `AUTO` - Automatic periodic saves
- `SHUTDOWN` - Server shutdown saves

Mission headers configure which types are enabled via `m_eSaveTypes` flags.

---

## Serialization Patterns

### Basic Serialization

#### **Write Data:**
```enforcescript
override protected ESerializeResult Serialize(/* ... */ context)
{
    // Simple values
    context.Write(m_iValue);
    context.Write(m_sName);
    context.Write(m_bEnabled);

    // Arrays/Maps (automatic)
    context.Write(m_aItems);
    context.Write(m_mData);

    return ESerializeResult.OK;
}
```

#### **Read Data:**
```enforcescript
override protected bool Deserialize(/* ... */ context)
{
    context.Read(m_iValue);
    context.Read(m_sName);
    context.Read(m_bEnabled);
    context.Read(m_aItems);
    context.Read(m_mData);

    return true;
}
```

### Versioning

**ALWAYS version your data for future compatibility:**
```enforcescript
override protected ESerializeResult Serialize(/* ... */ context)
{
    context.WriteValue("version", 2); // Named value

    context.Write(m_iValue);
    context.Write(m_sNewField); // Added in v2

    return ESerializeResult.OK;
}

override protected bool Deserialize(/* ... */ context)
{
    int version;
    if (context.ReadValue("version", version))
    {
        // Version found
    }
    else
    {
        version = 1; // Old data
    }

    context.Read(m_iValue);

    if (version >= 2)
        context.Read(m_sNewField);
    else
        m_sNewField = "default"; // Handle old data

    return true;
}
```

### Writing Defaults Only

**Skip writing values that are at their default (saves space):**
```enforcescript
override protected ESerializeResult Serialize(/* ... */ context)
{
    // Only write if different from default
    context.WriteDefault(m_iCount, 0);
    context.WriteDefault(m_bEnabled, false);
    context.WriteDefault(m_fMultiplier, 1.0);

    // Check if all values were default
    if (m_iCount == 0 && !m_bEnabled && m_fMultiplier == 1.0)
        return ESerializeResult.DEFAULT; // No custom data, use native

    return ESerializeResult.OK;
}
```

### Entity References (UUID Pattern)

**The most important pattern for Overthrow migration.**

#### **Save Entity Reference:**
```enforcescript
override protected ESerializeResult Serialize(/* ... */ context)
{
    // Get UUID of referenced entity
    UUID entityId = GetSystem().GetId(m_ReferencedEntity);

    // Only write if entity is tracked
    if (!entityId.IsNull())
        context.Write(entityId);
    else
        context.Write(UUID.NULL_UUID);

    return ESerializeResult.OK;
}
```

#### **Load Entity Reference (with async resolution):**
```enforcescript
override protected bool Deserialize(/* ... */ context)
{
    UUID entityId;
    context.Read(entityId);

    if (!entityId.IsNull())
    {
        // Create context for callback (captures 'this' component)
        Tuple1<MyComponent> ctx(this);

        // Create async task
        PersistenceWhenAvailableTask task(OnEntityAvailable, ctx);

        // Wait for entity (max 60 seconds)
        GetSystem().WhenAvailable(entityId, task, 60.0);
    }

    return true;
}

// Static callback when entity becomes available
protected static void OnEntityAvailable(
    Managed instance,
    PersistenceDeferredDeserializeTask task,
    bool expired,
    Managed context)
{
    // Cast to actual entity type
    IEntity entity = IEntity.Cast(instance);
    if (!entity || expired)
        return; // Entity not found or timeout

    // Retrieve component from context
    Tuple1<MyComponent> ctx = Tuple1<MyComponent>.Cast(context);
    if (!ctx || !ctx.param1)
        return;

    // Restore reference
    ctx.param1.SetReferencedEntity(entity);
}
```

**Why async resolution?**
- Entities may load in any order
- Referenced entity might not exist yet when deserializing
- `WhenAvailable` queues callback for when UUID becomes available
- Handles late-joiners, delayed spawns, etc.

### Array of Entity References

```enforcescript
override protected ESerializeResult Serialize(/* ... */ context)
{
    array<UUID> entityIds();

    foreach (IEntity entity : m_aEntities)
    {
        UUID id = GetSystem().GetId(entity);
        if (!id.IsNull())
            entityIds.Insert(id);
    }

    context.Write(entityIds);
    return ESerializeResult.OK;
}

override protected bool Deserialize(/* ... */ context)
{
    array<UUID> entityIds();
    context.Read(entityIds);

    foreach (UUID id : entityIds)
    {
        Tuple2<MyComponent, UUID> ctx(this, id);
        PersistenceWhenAvailableTask task(OnEntityAvailable, ctx);
        GetSystem().WhenAvailable(id, task, 60.0);
    }

    return true;
}

protected static void OnEntityAvailable(/* ... */)
{
    IEntity entity = IEntity.Cast(instance);
    if (!entity || expired)
        return;

    Tuple2<MyComponent, UUID> ctx = Tuple2<MyComponent, UUID>.Cast(context);
    if (ctx && ctx.param1)
        ctx.param1.AddEntity(entity);
}
```

### Using Tuple for Complex Context

**Tuple classes** allow passing multiple values to callbacks:
```enforcescript
// Single value
Tuple1<MyComponent> ctx(this);

// Two values
Tuple2<MyComponent, int> ctx(this, m_iSomeValue);

// Three values
Tuple3<MyComponent, int, string> ctx(this, m_iValue, m_sName);

// Access in callback
auto ctx = Tuple2<MyComponent, int>.Cast(context);
MyComponent comp = ctx.param1;
int value = ctx.param2;
```

---

## Tracking Lifecycle

### Automatic Tracking

Entities/components can be automatically tracked based on configuration rules (prefab, class type, etc.).

### Manual Tracking

```enforcescript
// Start tracking an entity
PersistenceSystem.GetInstance().StartTracking(myEntity);

// Assign specific UUID (e.g., player identity)
UUID playerId = SCR_PlayerIdentityUtils.GetPlayerIdentityId(playerId);
PersistenceSystem.GetInstance().SetId(playerController, playerId);

// Save immediately (transient until next global save)
PersistenceSystem.GetInstance().Save(myEntity);

// Find tracked instance
IEntity entity = IEntity.Cast(PersistenceSystem.GetInstance().FindById(entityId));

// Stop tracking (deletes data on next save)
PersistenceSystem.GetInstance().StopTracking(myEntity);

// Release tracking (keeps data but releases tracker for memory)
PersistenceSystem.GetInstance().ReleaseTracking(myEntity);
```

---

## Network Synchronization

**Key Difference from EPF:**
- Persistence system is **server-authority only**
- Uses **UUID** for persistent references (not RplId)
- No automatic network sync - persistence handles save/load only
- Components use **RplProp/RPC** for runtime replication separately
- Player identity UUID used to reconnect players to their saved data

**Pattern:**
1. Server saves entity state to persistence
2. Server replicates runtime state via RplProp/RPC
3. On load, server restores from persistence
4. Server replicates restored state to clients

---

## Console Platform Handling

**No explicit console guards needed!**

Unlike EPF, the vanilla system handles platform differences internally:
- ❌ No `#ifdef PLATFORM_CONSOLE` guards in serializers
- ✅ System automatically adapts to platform capabilities
- ✅ Check `OnAfterSave(saveType, success)` for failures

---

## Migration from EPF to Vanilla Persistence

### Comparison Table

| Feature | EPF | Vanilla Persistence |
|---------|-----|-------------------|
| **System Type** | Script-based | Native C++ with script bindings |
| **Configuration** | Code-based registration | Config-driven rules |
| **Tracking** | Manual registration | Automatic + manual |
| **Collections** | EPF_PersistentScriptedStateSettings | PersistenceCollection |
| **Save Classes** | EPF_ComponentSaveDataClass | Custom classes with context.Write/Read |
| **Serialization** | ScriptBitSerializer | BaseSerializationSaveContext |
| **Entity Refs** | EPF_EntityIdComponentHelper | GetSystem().GetId() + WhenAvailable |
| **Arrays/Maps** | Manual serialization loops | context.Write(array/map) auto-handles |
| **Spawn Handling** | Manual SpawnEntity | RequestSpawn with PersistenceSpawnRequest |
| **Versioning** | Manual flags | context.WriteValue("version", N) |
| **Platform Guards** | Required (#ifdef PLATFORM_CONSOLE) | Not needed |
| **Performance** | Slower (all script) | Faster (native serialization) |

### Migration Steps

#### 1. Replace SaveData Classes with Serializers

**OLD (EPF):**
```enforcescript
class OVT_MyComponentSaveData : EPF_ComponentSaveDataClass
{
    int m_iValue;
    string m_sName;

    override void WriteData(ScriptBitSerializer serializer)
    {
        serializer.Write(m_iValue, 32);
        serializer.WriteString(m_sName);
    }

    override bool ReadData(ScriptBitSerializer serializer)
    {
        serializer.Read(m_iValue, 32);
        serializer.ReadString(m_sName);
        return true;
    }
}
```

**NEW (Vanilla):**
```enforcescript
class OVT_MyComponentSerializer : ScriptedComponentSerializer
{
    override static typename GetTargetType()
    {
        return OVT_MyComponent;
    }

    override protected ESerializeResult Serialize(
        IEntity owner,
        GenericComponent component,
        BaseSerializationSaveContext context)
    {
        OVT_MyComponent comp = OVT_MyComponent.Cast(component);

        context.WriteValue("version", 1);
        context.Write(comp.m_iValue);
        context.Write(comp.m_sName);

        return ESerializeResult.OK;
    }

    override protected bool Deserialize(
        IEntity owner,
        GenericComponent component,
        BaseSerializationLoadContext context)
    {
        OVT_MyComponent comp = OVT_MyComponent.Cast(component);

        int version;
        context.Read(version);
        context.Read(comp.m_iValue);
        context.Read(comp.m_sName);

        return true;
    }
}
```

#### 2. Update Entity References

**OLD (EPF):**
```enforcescript
// Save
RplId entityId = Replication.FindId(m_Entity);
saveData.m_iEntityId = entityId.Id();

// Load
RplId entityId = RplId.Invalid();
entityId.SetId(saveData.m_iEntityId);
m_Entity = IEntity.Cast(Replication.FindItem(entityId));
```

**NEW (Vanilla):**
```enforcescript
// Save
UUID entityId = GetSystem().GetId(m_Entity);
context.Write(entityId);

// Load
UUID entityId;
context.Read(entityId);

if (!entityId.IsNull())
{
    Tuple1<OVT_MyComponent> ctx(this);
    PersistenceWhenAvailableTask task(OnEntityAvailable, ctx);
    GetSystem().WhenAvailable(entityId, task, 60.0);
}

// Callback
protected static void OnEntityAvailable(
    Managed instance,
    PersistenceDeferredDeserializeTask task,
    bool expired,
    Managed context)
{
    IEntity entity = IEntity.Cast(instance);
    if (!entity || expired)
        return;

    auto ctx = Tuple1<OVT_MyComponent>.Cast(context);
    ctx.param1.m_Entity = entity;
}
```

#### 3. Update Collections

**OLD (EPF):**
```enforcescript
[EPF_PersistentScriptedStateSettings(OVT_WorldData)]
class OVT_WorldDataSave : EPF_PersistentScriptedState
{
    // ...
}
```

**NEW (Vanilla):**
```enforcescript
// 1. Create proxy state
class OVT_WorldData : PersistentState {}

// 2. Create serializer
class OVT_WorldDataSerializer : ScriptedStateSerializer
{
    override static typename GetTargetType()
    {
        return OVT_WorldData;
    }

    override protected ESerializeResult Serialize(/* ... */)
    {
        // Access actual system data
        OVT_GameModeManager gameMode = OVT_GameModeManager.GetInstance();

        // Write state
        context.WriteValue("version", 1);
        // ...

        return ESerializeResult.OK;
    }
}
```

#### 4. Update Tracking Calls

**OLD (EPF):**
```enforcescript
EPF_PersistenceManager.GetInstance().Register(myEntity);
EPF_PersistenceManager.GetInstance().Unregister(myEntity);
```

**NEW (Vanilla):**
```enforcescript
PersistenceSystem.GetInstance().StartTracking(myEntity);
PersistenceSystem.GetInstance().StopTracking(myEntity);
```

#### 5. Remove Console Platform Guards

**OLD (EPF):**
```enforcescript
#ifndef PLATFORM_CONSOLE
    EPF_PersistenceManager.GetInstance().Register(this);
#endif
```

**NEW (Vanilla):**
```enforcescript
// Just remove the guards - system handles it
PersistenceSystem.GetInstance().StartTracking(this);
```

#### 6. Update Array/Map Serialization

**OLD (EPF):**
```enforcescript
// Save
serializer.Write(m_aItems.Count(), 16);
foreach (string item : m_aItems)
{
    serializer.WriteString(item);
}

// Load
int count;
serializer.Read(count, 16);
m_aItems.Clear();
for (int i = 0; i < count; i++)
{
    string item;
    serializer.ReadString(item);
    m_aItems.Insert(item);
}
```

**NEW (Vanilla):**
```enforcescript
// Save
context.Write(m_aItems);

// Load
context.Read(m_aItems);
```

---

## Best Practices

### 1. Always Version Your Data
```enforcescript
context.WriteValue("version", 1);
```

### 2. Use WriteDefault for Optional Values
```enforcescript
context.WriteDefault(m_iValue, 0); // Only writes if != 0
```

### 3. Check for NULL UUIDs
```enforcescript
UUID entityId = GetSystem().GetId(entity);
if (!entityId.IsNull())
    context.Write(entityId);
else
    context.Write(UUID.NULL_UUID);
```

### 4. Return DEFAULT When No Custom Data
```enforcescript
if (noCustomData)
    return ESerializeResult.DEFAULT; // Uses native serialization
```

### 5. Handle WhenAvailable Timeouts
```enforcescript
protected static void OnAvailable(/* ... */, bool expired, /* ... */)
{
    if (expired || !instance)
        return; // Entity didn't load in time
}
```

### 6. Use Tuple for Context Passing
```enforcescript
Tuple2<MyComponent, int> ctx(this, someValue);
PersistenceWhenAvailableTask task(OnAvailable, ctx);
```

### 7. Consider CanSeekMembers for Format Compatibility
```enforcescript
if (!myArray.IsEmpty() || !context.CanSeekMembers())
    context.Write(myArray); // Always write for non-seekable formats
```

---

## Code Examples from Arma Reforger

### Example 1: Component with Entity Reference
**File:** `Scripts/Game/Systems/Persistence/Serializers/Components/Character/SCR_GadgetManagerComponentSerializer.c`

```enforcescript
class SCR_GadgetManagerComponentSerializer : ScriptedComponentSerializer
{
    override static typename GetTargetType()
    {
        return SCR_GadgetManagerComponent;
    }

    override protected ESerializeResult Serialize(
        notnull IEntity owner,
        notnull GenericComponent component,
        notnull BaseSerializationSaveContext context)
    {
        const SCR_GadgetManagerComponent gadgetManager = SCR_GadgetManagerComponent.Cast(component);
        const UUID gadgetId = GetSystem().GetId(gadgetManager.GetHeldGadget());

        if (gadgetId.IsNull())
            return ESerializeResult.DEFAULT;

        context.Write(gadgetId);
        return ESerializeResult.OK;
    }

    override protected bool Deserialize(
        notnull IEntity owner,
        notnull GenericComponent component,
        notnull BaseSerializationLoadContext context)
    {
        auto gadgetManager = SCR_GadgetManagerComponent.Cast(component);

        UUID gadgetId;
        if (context.Read(gadgetId) && !gadgetId.IsNull())
        {
            Tuple1<SCR_GadgetManagerComponent> ctx(gadgetManager);
            PersistenceWhenAvailableTask task(OnGadgetAvailable, ctx);
            GetSystem().WhenAvailable(gadgetId, task);
        }
        return true;
    }

    protected static void OnGadgetAvailable(
        Managed instance,
        PersistenceDeferredDeserializeTask task,
        bool expired,
        Managed context)
    {
        auto gadget = IEntity.Cast(instance);
        if (!gadget)
            return;

        auto ctx = Tuple1<SCR_GadgetManagerComponent>.Cast(context);
        if (ctx.param1)
            ctx.param1.HandleInput(gadget, 1);
    }
}
```

### Example 2: Complex Component with Version + Collections
**File:** `Scripts/Game/Systems/Persistence/Serializers/Components/GameMode/Conflict/SCR_CampaignMilitaryBaseComponentSerializer.c`

```enforcescript
override protected ESerializeResult Serialize(
    notnull IEntity owner,
    notnull GenericComponent component,
    notnull BaseSerializationSaveContext context)
{
    const SCR_CampaignMilitaryBaseComponent militaryBase = SCR_CampaignMilitaryBaseComponent.Cast(component);

    const bool isHQ = militaryBase.IsHQ();
    const UUID buildingId = GetSystem().GetId(militaryBase.GetBaseBuildingComposition());
    const int callsign = militaryBase.GetCallsign();

    // Skip if all defaults
    if (!isHQ && buildingId.IsNull() && callsign == INVALID_CALLSIGN)
        return ESerializeResult.DEFAULT;

    context.WriteValue("version", 1);
    context.WriteDefault(isHQ, false);
    context.WriteDefault(buildingId, UUID.NULL_UUID);
    context.WriteDefault(callsign, INVALID_CALLSIGN);

    return ESerializeResult.OK;
}
```

### Example 3: State Serializer (Non-Entity)
**File:** `Scripts/Game/Systems/Persistence/Serializers/States/SCR_TaskSystemSerializer.c`

```enforcescript
class SCR_TaskSystemData : PersistentState {} // Proxy state

class SCR_TaskSystemSerializer : ScriptedStateSerializer
{
    override static typename GetTargetType()
    {
        return SCR_TaskSystemData;
    }

    override ESerializeResult Serialize(
        notnull Managed instance,
        notnull BaseSerializationSaveContext context)
    {
        const SCR_TaskSystem taskSystem = SCR_TaskSystem.GetInstance();
        if (!taskSystem)
            return ESerializeResult.DEFAULT;

        array<SCR_Task> outTasks();
        taskSystem.GetTasks(outTasks);

        array<ref SCR_TaskSave> tasks();
        foreach (auto task : outTasks)
        {
            auto save = SCR_TaskSave.GetTaskTypeSave(task);
            save.Save(task, GetSystem());
            if (!save.IsDefault())
                tasks.Insert(save);
        }

        if (tasks.IsEmpty())
            return ESerializeResult.DEFAULT;

        context.WriteValue("version", 1);
        context.Write(tasks);
        return ESerializeResult.OK;
    }
}
```

---

## Key Files for Reference

**Core System:**
- `/mnt/n/Projects/Arma 4/ArmaReforger/Scripts/Game/generated/Systems/Persistence/PersistenceSystem.c`
- `/mnt/n/Projects/Arma 4/ArmaReforger/Scripts/Game/Systems/Persistence/SCR_PersistenceSystem.c`

**Serializer Bases:**
- `/mnt/n/Projects/Arma 4/ArmaReforger/Scripts/Game/generated/Systems/Persistence/Serializers/ScriptedComponentSerializer.c`
- `/mnt/n/Projects/Arma 4/ArmaReforger/Scripts/Game/generated/Systems/Persistence/Serializers/ScriptedEntitySerializer.c`
- `/mnt/n/Projects/Arma 4/ArmaReforger/Scripts/Game/generated/Systems/Persistence/Serializers/ScriptedStateSerializer.c`

**Real Examples:**
- `/mnt/n/Projects/Arma 4/ArmaReforger/Scripts/Game/Systems/Persistence/Serializers/Components/Character/SCR_GadgetManagerComponentSerializer.c`
- `/mnt/n/Projects/Arma 4/ArmaReforger/Scripts/Game/Systems/Persistence/Serializers/Components/GameMode/Conflict/SCR_CampaignMilitaryBaseComponentSerializer.c`
- `/mnt/n/Projects/Arma 4/ArmaReforger/Scripts/Game/Systems/Persistence/Serializers/Entities/SCR_AIGroupSerializer.c`
- `/mnt/n/Projects/Arma 4/ArmaReforger/Scripts/Game/Systems/Persistence/Serializers/States/SCR_TaskSystemSerializer.c`

---

## Conclusion

The vanilla persistence system represents a significant improvement over EPF:
- **Cleaner API** with less boilerplate
- **Better performance** via native C++ implementation
- **Automatic handling** of collections, references, platform differences
- **More robust** async entity reference resolution
- **Easier maintenance** with configuration-driven rules

Migration will require rewriting all EPF SaveData classes as Serializers, but the resulting code will be more maintainable and performant.

---

**Next Steps:**
1. Review this research document
2. Create requirements.md detailing migration scope
3. Create implementation.md with phased migration plan
4. Begin implementation after approval
