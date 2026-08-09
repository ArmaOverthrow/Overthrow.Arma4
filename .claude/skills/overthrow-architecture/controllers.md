# Controller Components

Complete guide for Controller component pattern in Overthrow mod.

---

## Overview

Controller components manage individual entity instances (bases, towns, camps, etc.). Unlike managers (singletons), controllers are instantiated multiple times - one per entity.

**Key characteristics:**
- Multiple instances (not singletons)
- Manage specific entity state
- Register with manager in constructor
- Replicate state to clients as needed
- Persist entity-specific data via EPF

---

## Basic Controller Pattern

### Structure

```cpp
class OVT_TownControllerComponentClass: OVT_ComponentClass {};

class OVT_TownControllerComponent: OVT_Component
{
    // Protected parameters (editor-configurable)
    [Attribute("0", desc: "Town ID")]
    protected int m_iTownId;

    [Attribute("", desc: "Town name")]
    protected string m_sTownName;

    // Runtime state
    protected int m_iFactionId;
    protected int m_iPopulation;

    // Constructor - register with manager
    void OVT_TownControllerComponent(IEntityComponentSource src, IEntity ent, IEntity parent)
    {
        // Register on both client and server
        OVT_TownManagerComponent manager = OVT_Global.GetTowns();
        if (manager)
        {
            manager.RegisterTown(this);
        }
    }

    // Init may be called by manager if needed
    void Init(IEntity owner)
    {
        // Controller initialization
        // Not always called - don't rely on it
    }

    // Cleanup on destroy
    void ~OVT_TownControllerComponent()
    {
        // Unregister from manager
        OVT_TownManagerComponent manager = OVT_Global.GetTowns();
        if (manager)
        {
            manager.UnregisterTown(this);
        }
    }

    // Getters and setters for parameters
    int GetTownId() { return m_iTownId; }
    void SetTownId(int townId) { m_iTownId = townId; }

    string GetTownName() { return m_sTownName; }
    void SetTownName(string name) { m_sTownName = name; }

    int GetFactionId() { return m_iFactionId; }
    void SetFactionId(int factionId)
    {
        m_iFactionId = factionId;
        OnFactionChanged();
    }

    protected void OnFactionChanged()
    {
        // Handle faction change
        // Update visuals, notify other systems
    }
}
```

---

## Registration Pattern

### Constructor Registration (Recommended)

**Modern pattern:** Register in constructor on both client and server

```cpp
void OVT_SomeControllerComponent(IEntityComponentSource src, IEntity ent, IEntity parent)
{
    // Runs on both client and server
    OVT_SomeManagerComponent manager = OVT_Global.GetSomeManager();
    if (manager)
    {
        manager.RegisterController(this);
    }
}
```

### Legacy Registration (Deprecated)

**Old pattern:** QueryEntitiesBySphere on startup

❌ **Don't use for new controllers:**
```cpp
// OLD WAY - Don't do this anymore
void FindControllers()
{
    array<IEntity> entities = new array<IEntity>();
    GetGame().GetWorld().QueryEntitiesBySphere(
        vector.Zero,
        10000,
        null,
        null,
        entities,
        EQueryEntitiesFlags.DYNAMIC | EQueryEntitiesFlags.STATIC
    );

    foreach (IEntity entity : entities)
    {
        OVT_SomeController controller = OVT_SomeController.Cast(
            entity.FindComponent(OVT_SomeController)
        );
        if (controller)
        {
            RegisterController(controller);
        }
    }
}
```

---

## State Replication

### Basic Replication

```cpp
class OVT_BaseControllerComponent: OVT_Component
{
    // Replicated values
    [RplProp(onRplName: "OnFactionChanged")]
    protected int m_iFactionId;

    [RplProp(onRplName: "OnSuppliesChanged")]
    protected int m_iSupplies;

    // Non-replicated (server-only)
    protected float m_fProductionRate;

    // Server method to update faction
    void SetFactionId(int factionId)
    {
        if (!Replication.IsServer()) return; // Server only

        m_iFactionId = factionId;
        Replication.BumpMe(); // Broadcast to clients
    }

    // Client callback when faction changes
    protected void OnFactionChanged()
    {
        // Update client-side visuals
        UpdateFactionFlag();
        UpdateBaseColor();
    }

    // Server method to update supplies
    void AddSupplies(int amount)
    {
        if (!Replication.IsServer()) return;

        m_iSupplies += amount;
        Replication.BumpMe();
    }

    // Client callback when supplies change
    protected void OnSuppliesChanged()
    {
        // Update UI
        UpdateSupplyDisplay();
    }
}
```

### Collection Replication via RPC

```cpp
class OVT_BaseControllerComponent: OVT_Component
{
    protected ref array<string> m_aDefenders;

    // Server updates defenders
    void UpdateDefenders(array<string> defenders)
    {
        if (!Replication.IsServer()) return;

        m_aDefenders = defenders;

        // Broadcast to clients via RPC
        RpcDo_UpdateDefenders(defenders);
    }

    // Call directly on host, RPC on server
    void RpcDo_UpdateDefenders(array<string> defenders)
    {
        Rpc(RpcDo_UpdateDefendersImpl, defenders);
    }

    [RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]
    protected void RpcDo_UpdateDefendersImpl(array<string> defenders)
    {
        m_aDefenders = defenders;
        OnDefendersUpdated();
    }

    protected void OnDefendersUpdated()
    {
        // Update client UI
    }
}
```

---

## Persistence

### Controller Persistence Pattern

```cpp
#ifndef PLATFORM_CONSOLE

[EPF_ComponentSaveDataType(OVT_TownControllerComponent)]
class OVT_TownControllerSaveDataClass : EPF_ComponentSaveDataClass {};

class OVT_TownControllerSaveData : EPF_ComponentSaveData
{
    int m_iTownId;
    string m_sTownName;
    int m_iFactionId;
    int m_iPopulation;
    vector m_vPosition;

    override void ReadFrom(OVT_TownControllerComponent component)
    {
        // Extract controller state
        m_iTownId = component.GetTownId();
        m_sTownName = component.GetTownName();
        m_iFactionId = component.GetFactionId();
        m_iPopulation = component.GetPopulation();

        IEntity owner = component.GetOwner();
        if (owner)
        {
            m_vPosition = owner.GetOrigin();
        }
    }

    override void ApplyTo(OVT_TownControllerComponent component)
    {
        // Restore controller state
        component.SetTownId(m_iTownId);
        component.SetTownName(m_sTownName);
        component.SetFactionId(m_iFactionId);
        component.SetPopulation(m_iPopulation);

        // Position handled by entity transform
    }
}

#endif
```

---

## Lifecycle

### Controller Lifecycle Flow

1. **Creation:**
   - Entity spawned or loaded from prefab
   - Constructor called (both client/server)
   - Constructor registers with manager
   - Manager may call Init() if needed

2. **Runtime:**
   - Controller manages entity state
   - Replicates changes to clients
   - Persists state via EPF

3. **Destruction:**
   - Destructor called
   - Unregister from manager
   - Clean up resources

### Example with Full Lifecycle

```cpp
class OVT_CampControllerComponent: OVT_Component
{
    protected int m_iCampId;
    protected ref array<EntityID> m_aOccupants;

    // 1. Construction
    void OVT_CampControllerComponent(IEntityComponentSource src, IEntity ent, IEntity parent)
    {
        Print("Camp controller constructed");

        // Register with manager
        OVT_CampManagerComponent manager = OVT_Global.GetCamps();
        if (manager)
        {
            m_iCampId = manager.RegisterCamp(this);
        }

        // Initialize collections
        m_aOccupants = new array<EntityID>();
    }

    // 2. Initialization (if manager calls it)
    void Init(IEntity owner)
    {
        Print("Camp controller initialized");

        // Additional setup if needed
        SetupTriggers();
    }

    // 3. Runtime methods
    void AddOccupant(IEntity occupant)
    {
        if (!occupant) return;

        m_aOccupants.Insert(occupant.GetID());
        OnOccupantsChanged();
    }

    // 4. Destruction
    void ~OVT_CampControllerComponent()
    {
        Print("Camp controller destroyed");

        // Unregister from manager
        OVT_CampManagerComponent manager = OVT_Global.GetCamps();
        if (manager)
        {
            manager.UnregisterCamp(this);
        }

        // Clean up
        m_aOccupants = null;
    }
}
```

---

## Parameter Access Patterns

### Protected Members with Getters/Setters

**Best practice:** Keep members protected, expose via methods

```cpp
class OVT_SomeControllerComponent: OVT_Component
{
    // Protected - not directly accessible
    [Attribute("0")]
    protected int m_iValue;

    [Attribute("")]
    protected string m_sName;

    // Public getters
    int GetValue() { return m_iValue; }
    string GetName() { return m_sName; }

    // Public setters with validation
    void SetValue(int value)
    {
        if (value < 0) return; // Validation

        m_iValue = value;
        OnValueChanged();
    }

    void SetName(string name)
    {
        if (name.IsEmpty()) return;

        m_sName = name;
    }

    protected void OnValueChanged()
    {
        // Handle value change
        // Update derived values
        // Notify other systems
    }
}
```

---

## Multiple Controllers on Same Entity

### Composition Pattern

Controllers can be composed - multiple controllers on same entity managing different aspects:

```cpp
// Base management controller
class OVT_BaseControllerComponent: OVT_Component
{
    // Manages base ownership, faction, etc.
}

// Defense controller on same entity
class OVT_DefenseControllerComponent: OVT_Component
{
    // Manages defense turrets, guards, etc.

    void Init(IEntity owner)
    {
        // Get base controller on same entity
        OVT_BaseControllerComponent baseController = OVT_BaseControllerComponent.Cast(
            owner.FindComponent(OVT_BaseControllerComponent)
        );

        if (baseController)
        {
            // Coordinate with base controller
        }
    }
}

// Economy controller on same entity
class OVT_BaseEconomyComponent: OVT_Component
{
    // Manages resource production, storage, etc.
}
```

---

## Best Practices

### ✅ DO:

- **Register in constructor:** On both client and server
- **Use protected members:** Expose via getters/setters
- **Unregister in destructor:** Clean up manager references
- **Replicate essential state:** Use RplProp for simple values
- **Validate setters:** Check parameters before setting
- **Check manager exists:** Manager may not be loaded yet
- **Document lifecycle:** When constructor/Init/destructor are called
- **Persist controller state:** Use EPF for save/load

### ❌ DON'T:

- **Use QueryEntitiesBySphere:** Deprecated, register in constructor instead
- **Assume Init() called:** May not be called by manager
- **Store IEntity long-term:** Use EntityID instead
- **Replicate arrays with RplProp:** Use RPC or JIP
- **Forget to unregister:** Memory leak if manager keeps reference
- **Public member variables:** Use getters/setters
- **Skip validation:** Always validate in setters

---

## Testing Controllers

### Manual Testing
1. Spawn entity with controller
2. Verify constructor called and registered
3. Test getter/setter methods
4. Verify state replication to clients
5. Test persistence (save/load)
6. Delete entity and verify unregistration

### Multiple Instance Testing
1. Spawn multiple entities
2. Verify each has separate controller
3. Test state changes don't affect others
4. Verify all registered with manager

---

## Common Issues

### Issue: Controller not registered
**Cause:** Manager doesn't exist when constructor runs
**Fix:** Check manager exists before registering, or use delayed registration

### Issue: Init() never called
**Cause:** Manager not calling Init()
**Fix:** Don't rely on Init(), initialize in constructor or first method call

### Issue: State not replicating
**Cause:** Forgot Replication.BumpMe()
**Fix:** Call Replication.BumpMe() after changing RplProp values

### Issue: Memory leak
**Cause:** Not unregistering from manager
**Fix:** Add unregister call in destructor

---

## Related Resources

- See `managers.md` for manager pattern
- See `global-access.md` for accessing managers
- See `enforcescript-patterns/component-patterns.md` for base patterns
- See `enforcescript-patterns/networking.md` for replication
- See `enforcescript-patterns/persistence.md` for EPF
- See main `SKILL.md` for overview
