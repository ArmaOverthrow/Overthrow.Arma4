# Persistence Patterns with EPF

Complete guide for save/load operations using Enfusion Persistence Framework (EPF).

---

## Overview

EPF (Enfusion Persistence Framework) handles save/load operations for Overthrow mod. Components define SaveData classes that extract and restore state.

**Critical:** Console platforms (Xbox/PlayStation) don't support disk access - all EPF operations must be wrapped in `#ifndef PLATFORM_CONSOLE` guards.

---

## Basic SaveData Pattern

### When to Use

Use EPF persistence when:
- Component state needs to survive game restarts
- Manager or Controller data must persist
- Complex state needs structured save/load

### Pattern Structure

```cpp
// SaveData class associated with component
[EPF_ComponentSaveDataType(OVT_SomeManagerComponent)]
class OVT_SomeSaveDataClass : EPF_ComponentSaveDataClass {};

class OVT_SomeSaveData : EPF_ComponentSaveData
{
    // Persisted values (automatically serialized)
    int m_iSomeValue;
    string m_sSomeName;
    vector m_vSomePosition;
    ref array<int> m_aIds; // Use ref for collections

    // Extract data from component
    override void ReadFrom(OVT_SomeManagerComponent component)
    {
        m_iSomeValue = component.GetSomeValue();
        m_sSomeName = component.GetSomeName();
        m_vSomePosition = component.GetSomePosition();

        // Copy collection data
        m_aIds = new array<int>();
        component.GetIds(m_aIds);
    }

    // Restore data to component
    override void ApplyTo(OVT_SomeManagerComponent component)
    {
        component.SetSomeValue(m_iSomeValue);
        component.SetSomeName(m_sSomeName);
        component.SetSomePosition(m_vSomePosition);
        component.SetIds(m_aIds);
    }
}
```

### Key Points

- **Attribute:** [EPF_ComponentSaveDataType(ComponentType)] links SaveData to Component
- **Inheritance:** SaveDataClass extends EPF_ComponentSaveDataClass
- **Inheritance:** SaveData extends EPF_ComponentSaveData
- **ReadFrom:** Extract data from component for saving
- **ApplyTo:** Restore data to component when loading
- **Auto-serialization:** EPF handles serialization of member variables
- **Strong refs:** Use `ref` for arrays, maps, Managed classes

---

## Persisting Complex Data

### Nested Data Structures

```cpp
// Data class for persisting
class OVT_TownData : Managed
{
    [NonSerialized()]
    int m_iTempValue; // Don't persist this

    // Persisted values
    vector m_vLocation;
    int m_iPopulation;
    string m_sName;
}

// SaveData with nested structures
class OVT_TownSaveData : EPF_ComponentSaveData
{
    ref array<ref OVT_TownData> m_aTowns; // Strong refs required

    override void ReadFrom(OVT_TownManagerComponent component)
    {
        m_aTowns = new array<ref OVT_TownData>();

        // Copy town data
        array<ref OVT_TownData> towns = component.GetTowns();
        foreach (OVT_TownData town : towns)
        {
            m_aTowns.Insert(town); // EPF handles deep copy
        }
    }

    override void ApplyTo(OVT_TownManagerComponent component)
    {
        component.SetTowns(m_aTowns);
    }
}
```

### Maps and Collections

```cpp
class OVT_ManagerSaveData : EPF_ComponentSaveData
{
    ref map<int, ref OVT_SomeData> m_mDataById;
    ref array<string> m_aPlayerIds;

    override void ReadFrom(OVT_ManagerComponent component)
    {
        // Save map
        m_mDataById = new map<int, ref OVT_SomeData>();
        map<int, ref OVT_SomeData> sourceMap = component.GetDataMap();
        foreach (int key, OVT_SomeData value : sourceMap)
        {
            m_mDataById.Insert(key, value);
        }

        // Save array
        m_aPlayerIds = new array<string>();
        component.GetPlayerIds(m_aPlayerIds);
    }

    override void ApplyTo(OVT_ManagerComponent component)
    {
        component.SetDataMap(m_mDataById);
        component.SetPlayerIds(m_aPlayerIds);
    }
}
```

---

## NonSerialized Attribute

### When to Use

Use [NonSerialized()] for:
- Temporary/cached values
- References to entities (use EntityID or RplId instead)
- Derivable data that can be recalculated
- Runtime-only state

### Example

```cpp
class OVT_SomeData : Managed
{
    // NOT persisted
    [NonSerialized()]
    protected int m_iCachedValue;

    [NonSerialized()]
    protected IEntity m_CachedEntity; // Never persist IEntity

    // Persisted (default)
    protected int m_iPersistentValue;
    protected EntityID m_SavedEntityId; // Save ID, not entity reference
}
```

### Key Points

- **Default behavior:** All member variables are serialized
- **Explicit opt-out:** Use [NonSerialized()] to skip
- **Entities:** Never persist IEntity directly - use EntityID
- **Cached data:** Don't persist values that can be recalculated

---

## Console Platform Handling

### The Problem

Xbox and PlayStation don't support FileIO or disk access. EPF operations will fail on consoles.

### The Solution

Wrap all EPF operations in platform guards:

```cpp
#ifndef PLATFORM_CONSOLE

[EPF_ComponentSaveDataType(OVT_SomeComponent)]
class OVT_SomeSaveDataClass : EPF_ComponentSaveDataClass {};

class OVT_SomeSaveData : EPF_ComponentSaveData
{
    int m_iValue;

    override void ReadFrom(OVT_SomeComponent component)
    {
        m_iValue = component.GetValue();
    }

    override void ApplyTo(OVT_SomeComponent component)
    {
        component.SetValue(m_iValue);
    }
}

#endif
```

### Usage in Component

```cpp
class OVT_SomeComponent : OVT_Component
{
    void SaveGame()
    {
        #ifndef PLATFORM_CONSOLE
        // EPF save operations
        EPF_PersistenceManager.GetInstance().Save();
        #endif
    }

    void LoadGame()
    {
        #ifndef PLATFORM_CONSOLE
        // EPF load operations
        EPF_PersistenceManager.GetInstance().Load();
        #endif
    }
}
```

### Key Points

- **Both guards:** Arma Reforger provides PLATFORM_CONSOLE for both Xbox and PlayStation
- **Full wrapping:** Wrap SaveData class AND any EPF calls
- **Graceful degradation:** Game works on console, just without persistence
- **Testing:** Test on PC with EPF, verify graceful failure on console

---

## Save/Load Triggers

### Manual Save

```cpp
void SaveGame()
{
    #ifndef PLATFORM_CONSOLE
    EPF_PersistenceManager pm = EPF_PersistenceManager.GetInstance();
    if (pm)
    {
        pm.Save();
    }
    #endif
}
```

### Manual Load

```cpp
void LoadGame()
{
    #ifndef PLATFORM_CONSOLE
    EPF_PersistenceManager pm = EPF_PersistenceManager.GetInstance();
    if (pm)
    {
        pm.Load();
    }
    #endif
}
```

### Auto-save

```cpp
// Set up periodic auto-save
protected void SetupAutoSave()
{
    #ifndef PLATFORM_CONSOLE
    GetGame().GetCallqueue().CallLater(
        AutoSave,
        300000, // 5 minutes in milliseconds
        true    // Repeat
    );
    #endif
}

protected void AutoSave()
{
    #ifndef PLATFORM_CONSOLE
    EPF_PersistenceManager pm = EPF_PersistenceManager.GetInstance();
    if (pm)
    {
        pm.Save();
    }
    #endif
}
```

---

## Persistence Lifecycle

### Save Flow
1. Game triggers save (manual or auto)
2. EPF calls ReadFrom() on all SaveData classes
3. SaveData extracts state from components
4. EPF serializes SaveData to disk
5. Save complete

### Load Flow
1. Game triggers load
2. EPF deserializes SaveData from disk
3. EPF calls ApplyTo() on all SaveData classes
4. SaveData restores state to components
5. Components rebuild runtime state
6. Load complete

---

## Gotchas and Best Practices

### ✅ DO:

- **Use strong refs:** Always `ref` for arrays/maps of Managed classes
- **Guard console:** Wrap all EPF code in #ifndef PLATFORM_CONSOLE
- **Save IDs, not entities:** Use EntityID or RplId, not IEntity
- **Minimal state:** Only save what can't be derived
- **Validate on load:** Check loaded data for corruption/invalid values
- **Version your data:** Consider version numbers for format changes

### ❌ DON'T:

- **Persist IEntity:** Will crash or corrupt save
- **Save temporary data:** Use [NonSerialized()]
- **Forget platform guards:** Console will crash without them
- **Save derivable data:** Waste of space, can recalculate
- **Trust loaded data:** Always validate before use

---

## Migration Between Versions

### Adding Fields

```cpp
class OVT_SomeSaveData : EPF_ComponentSaveData
{
    int m_iOldField;
    int m_iNewField; // Added in v2

    override void ApplyTo(OVT_SomeComponent component)
    {
        component.SetOldField(m_iOldField);

        // Provide default if loading old save
        if (m_iNewField == 0)
        {
            m_iNewField = 100; // Default value
        }
        component.SetNewField(m_iNewField);
    }
}
```

### Removing Fields

```cpp
class OVT_SomeSaveData : EPF_ComponentSaveData
{
    // m_iOldField removed

    int m_iNewField;

    override void ReadFrom(OVT_SomeComponent component)
    {
        // Don't write old field anymore
        m_iNewField = component.GetNewField();
    }

    override void ApplyTo(OVT_SomeComponent component)
    {
        // Old saves may have old field, ignore it
        component.SetNewField(m_iNewField);
    }
}
```

---

## Testing Persistence

### Save Testing
1. Create game state (spawn entities, change values)
2. Trigger save manually
3. Verify save file created on disk
4. Check save file size is reasonable
5. Verify no errors in console

### Load Testing
1. Start new game
2. Trigger load from saved game
3. Verify all state restored correctly
4. Test entity references work
5. Confirm collections populated
6. Check for any errors or warnings

### Console Testing
1. Build with PLATFORM_CONSOLE defined
2. Verify game runs without crashes
3. Confirm EPF code not executed
4. Test graceful degradation

---

## Example: Complete Manager Persistence

```cpp
#ifndef PLATFORM_CONSOLE

[EPF_ComponentSaveDataType(OVT_TownManagerComponent)]
class OVT_TownManagerSaveDataClass : EPF_ComponentSaveDataClass {};

class OVT_TownManagerSaveData : EPF_ComponentSaveData
{
    ref array<ref OVT_TownData> m_aTowns;
    ref map<int, string> m_mTownOwners;

    override void ReadFrom(OVT_TownManagerComponent component)
    {
        // Save towns array
        m_aTowns = new array<ref OVT_TownData>();
        array<ref OVT_TownData> towns = component.GetTowns();
        foreach (OVT_TownData town : towns)
        {
            m_aTowns.Insert(town);
        }

        // Save ownership map
        m_mTownOwners = new map<int, string>();
        map<int, string> owners = component.GetTownOwners();
        foreach (int townId, string ownerId : owners)
        {
            m_mTownOwners.Insert(townId, ownerId);
        }
    }

    override void ApplyTo(OVT_TownManagerComponent component)
    {
        // Restore towns
        component.SetTowns(m_aTowns);

        // Restore ownership
        component.SetTownOwners(m_mTownOwners);

        // Rebuild runtime state
        component.RebuildTownIndex();
    }
}

#endif
```

---

## Related Resources

- See `component-patterns.md` for component architecture
- See `memory-management.md` for strong ref usage
- See `networking.md` for replication with persistence
- See main `SKILL.md` for overview
