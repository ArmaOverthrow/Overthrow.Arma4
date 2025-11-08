# Manager Components

Complete guide for Manager component pattern in Overthrow mod.

---

## Overview

Manager components are singletons placed on the OVT_OverthrowGameMode.et prefab. They manage entire systems or features within the game mode.

**Key characteristics:**
- Singleton pattern (one instance per game session)
- Server-side only (unless explicitly replicated)
- Manage multiple controller instances
- Coordinate global system state
- Registered on game mode prefab in Workbench

---

## Basic Manager Pattern

### Structure

```cpp
class OVT_SomeManagerComponentClass: OVT_ComponentClass {};

class OVT_SomeManagerComponent: OVT_Component
{
    // Editor-configurable parameters
    [Attribute("1", desc: "A parameter description")]
    int m_iSomeParameter;

    // Singleton instance
    static OVT_SomeManagerComponent s_Instance;

    // Singleton accessor
    static OVT_SomeManagerComponent GetInstance()
    {
        if (!s_Instance)
        {
            BaseGameMode pGameMode = GetGame().GetGameMode();
            if (pGameMode)
            {
                s_Instance = OVT_SomeManagerComponent.Cast(
                    pGameMode.FindComponent(OVT_SomeManagerComponent)
                );
            }
        }
        return s_Instance;
    }

    // Called manually by OVT_OverthrowGameMode for initialization
    void Init(IEntity owner)
    {
        // Initialize manager state
        // Set up collections
        // Register callbacks
    }

    // Called manually by OVT_OverthrowGameMode after new game starts
    void PostGameStart()
    {
        // Setup initial game state
        // Spawn initial entities
        // Initialize data structures
    }
}
```

---

## Lifecycle Methods

### Init(IEntity owner)

**When called:** During game mode initialization, before game starts

**Purpose:** Initialize manager state, collections, and callbacks

**Example:**
```cpp
void Init(IEntity owner)
{
    // Initialize collections
    m_aTowns = new array<ref OVT_TownData>();
    m_mTownControllers = new map<int, OVT_TownController>();

    // Register for events
    OVT_Global.GetServer().RegisterEventHandler(this);

    // Set up periodic updates
    GetGame().GetCallqueue().CallLater(
        UpdateManager,
        10000, // 10 seconds
        true   // Repeat
    );
}
```

### PostGameStart()

**When called:** After new game starts (not on load)

**Purpose:** Setup initial game state specific to new games

**Example:**
```cpp
void PostGameStart()
{
    // Spawn initial entities
    SpawnInitialBases();

    // Setup starting conditions
    InitializeFactionStates();

    // Distribute starting resources
    DistributeStartingResources();

    // Don't do this on save load - data restored via EPF
}
```

### Key Differences

- **Init():** Called both on new game AND save load
- **PostGameStart():** Called ONLY on new game start
- **Init():** Initialize data structures
- **PostGameStart():** Populate with initial game state

---

## Collection Management

### Registering Controllers

```cpp
class OVT_TownManagerComponent: OVT_Component
{
    // Collections of controller instances
    protected ref array<OVT_TownController> m_aTownControllers;
    protected ref map<int, OVT_TownController> m_mTownsById;

    static OVT_TownManagerComponent s_Instance;
    static OVT_TownManagerComponent GetInstance()
    {
        // ... singleton pattern ...
    }

    void Init(IEntity owner)
    {
        m_aTownControllers = new array<OVT_TownController>();
        m_mTownsById = new map<int, OVT_TownController>();
    }

    // Called by controller constructors
    void RegisterTown(OVT_TownController controller)
    {
        if (!controller) return;

        // Add to collections
        m_aTownControllers.Insert(controller);

        int townId = controller.GetTownId();
        m_mTownsById.Insert(townId, controller);

        // Initialize controller if needed
        controller.Init(controller.GetOwner());
    }

    // Called when controller destroyed
    void UnregisterTown(OVT_TownController controller)
    {
        if (!controller) return;

        // Remove from collections
        int idx = m_aTownControllers.Find(controller);
        if (idx >= 0)
        {
            m_aTownControllers.Remove(idx);
        }

        int townId = controller.GetTownId();
        if (m_mTownsById.Contains(townId))
        {
            m_mTownsById.Remove(townId);
        }
    }

    // Access controllers
    OVT_TownController GetTownById(int townId)
    {
        return m_mTownsById.Get(townId);
    }

    array<OVT_TownController> GetAllTowns()
    {
        return m_aTownControllers;
    }
}
```

---

## State Management

### Managing Global State

```cpp
class OVT_EconomyManagerComponent: OVT_Component
{
    // Global economy state
    [RplProp()]
    protected int m_iGlobalWealth;

    [RplProp()]
    protected float m_fInflationRate;

    protected ref map<string, int> m_mResourcePrices;

    void SetGlobalWealth(int wealth)
    {
        m_iGlobalWealth = wealth;
        Replication.BumpMe();
    }

    void UpdatePrices(map<string, int> prices)
    {
        m_mResourcePrices = prices;

        // Notify controllers of price changes
        foreach (OVT_MarketController market : m_aMarkets)
        {
            market.OnPricesUpdated(prices);
        }
    }
}
```

---

## Event Coordination

### Broadcasting Events

```cpp
class OVT_FactionManagerComponent: OVT_Component
{
    // Event invokers
    protected ref ScriptInvoker m_OnFactionChanged;
    protected ref ScriptInvoker m_OnWarDeclared;

    void Init(IEntity owner)
    {
        m_OnFactionChanged = new ScriptInvoker();
        m_OnWarDeclared = new ScriptInvoker();
    }

    void ChangeFaction(int townId, int newFactionId)
    {
        // Update faction
        OVT_TownController town = OVT_Global.GetTowns().GetTownById(townId);
        if (!town) return;

        int oldFactionId = town.GetFactionId();
        town.SetFactionId(newFactionId);

        // Broadcast event
        m_OnFactionChanged.Invoke(townId, oldFactionId, newFactionId);
    }

    void DeclareWar(int attackerFactionId, int defenderFactionId)
    {
        // Update war state
        // ...

        // Broadcast event
        m_OnWarDeclared.Invoke(attackerFactionId, defenderFactionId);
    }

    // Subscribe to events
    ScriptInvoker GetOnFactionChanged()
    {
        return m_OnFactionChanged;
    }
}

// Other components can subscribe:
void SubscribeToFactionEvents()
{
    OVT_FactionManagerComponent factions = OVT_Global.GetFactions();
    if (!factions) return;

    factions.GetOnFactionChanged().Insert(OnFactionChanged);
}

void OnFactionChanged(int townId, int oldFactionId, int newFactionId)
{
    // Handle faction change
}
```

---

## Periodic Updates

### Setting Up Periodic Tasks

```cpp
class OVT_EconomyManagerComponent: OVT_Component
{
    protected static const int UPDATE_INTERVAL = 60000; // 1 minute

    void Init(IEntity owner)
    {
        // Schedule periodic update
        GetGame().GetCallqueue().CallLater(
            PeriodicUpdate,
            UPDATE_INTERVAL,
            true // Repeat
        );
    }

    protected void PeriodicUpdate()
    {
        // Update economy state
        UpdatePrices();
        CalculateInflation();
        ProcessTransactions();

        // Notify controllers
        BroadcastEconomyUpdate();
    }
}
```

---

## OVT_Global Integration

### Adding Accessor to OVT_Global

After creating a manager, add accessor to OVT_Global for easy access:

```cpp
// In OVT_Global.c
class OVT_Global
{
    static OVT_TownManagerComponent GetTowns()
    {
        return OVT_TownManagerComponent.GetInstance();
    }

    static OVT_EconomyManagerComponent GetEconomy()
    {
        return OVT_EconomyManagerComponent.GetInstance();
    }

    // ... other accessors ...
}

// Usage anywhere in codebase:
void SomeMethod()
{
    OVT_TownManagerComponent towns = OVT_Global.GetTowns();
    if (!towns) return;

    // Use manager
}
```

---

## Persistence

### Manager Persistence Pattern

```cpp
#ifndef PLATFORM_CONSOLE

[EPF_ComponentSaveDataType(OVT_TownManagerComponent)]
class OVT_TownManagerSaveDataClass : EPF_ComponentSaveDataClass {};

class OVT_TownManagerSaveData : EPF_ComponentSaveData
{
    ref array<ref OVT_TownData> m_aTownData;
    int m_iNextTownId;

    override void ReadFrom(OVT_TownManagerComponent component)
    {
        // Extract manager state
        m_aTownData = new array<ref OVT_TownData>();
        component.GetTownData(m_aTownData);

        m_iNextTownId = component.GetNextTownId();
    }

    override void ApplyTo(OVT_TownManagerComponent component)
    {
        // Restore manager state
        component.SetTownData(m_aTownData);
        component.SetNextTownId(m_iNextTownId);

        // Rebuild runtime structures
        component.RebuildTownIndex();
    }
}

#endif
```

---

## Best Practices

### ✅ DO:

- **Use singleton pattern:** Static s_Instance and GetInstance()
- **Check for null:** GetInstance() may return null
- **Initialize collections:** In Init(), not in declaration
- **Clean up on destroy:** Override OnDelete() to clean up
- **Register with OVT_Global:** Add accessor for easy access
- **Document lifecycle:** When Init() and PostGameStart() are called
- **Manage controllers:** Provide Register/Unregister methods

### ❌ DON'T:

- **Create multiple instances:** Managers are singletons
- **Assume always exists:** Check GetInstance() result
- **Initialize in constructor:** Use Init() instead
- **Forget platform guards:** Wrap EPF in #ifndef PLATFORM_CONSOLE
- **Access directly:** Use OVT_Global accessors instead of GetInstance()
- **Replicate everything:** Most managers server-side only

---

## Testing Managers

### Manual Testing
1. Verify component placed on game mode prefab
2. Start game and check GetInstance() returns non-null
3. Verify Init() called during initialization
4. Test PostGameStart() called on new game only
5. Confirm singleton behavior (only one instance)
6. Test manager methods function correctly

### Persistence Testing
1. Create manager state
2. Save game
3. Load game
4. Verify state restored correctly
5. Test on PC and console (with guards)

---

## Common Issues

### Issue: GetInstance() returns null
**Cause:** Component not placed on game mode prefab
**Fix:** Add component to OVT_OverthrowGameMode.et in Workbench

### Issue: Init() never called
**Cause:** Game mode not calling Init()
**Fix:** Add Init() call to OVT_OverthrowGameMode initialization

### Issue: State lost after load
**Cause:** Missing persistence SaveData class
**Fix:** Implement EPF save/load pattern

---

## Related Resources

- See `controllers.md` for controller pattern
- See `global-access.md` for OVT_Global usage
- See `enforcescript-patterns/component-patterns.md` for base patterns
- See `enforcescript-patterns/persistence.md` for EPF patterns
- See main `SKILL.md` for overview
