# Vanilla Persistence Migration - Implementation Plan

**Feature Name:** vanilla-persistence
**Status:** Planning Complete - Ready for Implementation
**Priority:** High - Start ASAP
**Started:** 2025-11-09
**Target Completion:** 5-7 days (estimated 30-40 hours total)
**Last Updated:** 2025-11-09

---

## Executive Summary

This plan outlines the complete migration from EPF (Enfusion Persistence Framework) to Arma Reforger's vanilla persistence system. This is a **breaking change** - all existing save files will become incompatible and players will need to start fresh.

The migration affects **15 SaveData classes** (8 component, 5 entity, 2 state) and requires updating **20+ files** that use EPF helper utilities. We'll replace EPF's script-based serialization with vanilla's native C++ serialization system, which offers better performance, cleaner code, and automatic platform handling.

**Key Benefits:**
- **Performance:** Native C++ serialization vs script-based
- **Simplicity:** Automatic array/map serialization, no manual loops
- **Robustness:** Built-in UUID-based entity references with async resolution
- **Maintainability:** Less boilerplate, cleaner patterns
- **Platform Support:** No more `#ifdef PLATFORM_CONSOLE` guards needed

**Approach:** Big bang migration - all systems migrate together in a single branch (`vanilla-persistence`). This is acceptable because backward compatibility is not required.

---

## Goals

### Primary Goals
1. **Complete EPF Removal** - Replace all EPF/EDF dependencies with vanilla persistence
2. **Maintain Feature Parity** - All currently persisted data continues to save/load correctly
3. **Improve Performance** - Leverage native C++ for faster save/load operations
4. **Simplify Codebase** - Reduce boilerplate with cleaner serialization patterns
5. **Enable Console Support** - Remove platform-specific guards

### Secondary Goals
1. **Modernize Architecture** - Align with current Arma Reforger best practices
2. **Better Async Handling** - Use `WhenAvailable` pattern for entity reference resolution
3. **Improve Maintainability** - Create clear examples for future Overthrow development
4. **Proper Versioning** - All serializers support forward versioning from day one

---

## Architecture Overview

### Architectural Decisions

#### Decision 1: PersistenceCollection Organization

**Decision:** **12 separate collections** - one per major system/entity type.

**Rationale:**
- **Better organization** - Each system has its own collection for clarity
- **Granular control** - Can save/load individual systems if needed
- **Performance flexibility** - Can tune save frequencies per collection
- **Clearer debugging** - Issues isolated to specific collections
- **Future-proof** - Easy to add new systems without affecting existing collections
- **Logical grouping** - Collections match Overthrow's architectural boundaries

**Collection Structure:**

**Manager/Component Collections (7):**
1. `"OverthrowTowns"` - Town management data (OVT_TownManagerComponent)
2. `"OverthrowPlayers"` - Player management data (OVT_PlayerManagerComponent)
3. `"OverthrowEconomy"` - Economy/resistance money (OVT_EconomyManagerComponent)
4. `"OverthrowFactions"` - Faction state (OVT_OccupyingFactionManager, OVT_ResistanceManagerComponent)
5. `"OverthrowRealEstate"` - Owned properties (OVT_RealEstateManagerComponent)
6. `"OverthrowRecruits"` - Recruited NPCs (OVT_RecruitManagerComponent)
7. `"OverthrowConfig"` - Mod configuration (OVT_ConfigManagerComponent)

**Entity Collections (4):**
8. `"OverthrowPlaceables"` - Player-placed objects (OVT_PlaceableSaveData)
9. `"OverthrowBuildings"` - Building state (OVT_BuildingSaveData)
10. `"OverthrowBases"` - Base upgrades (OVT_BaseUpgradeSaveData)
11. `"OverthrowCharacters"` - Character state (OVT_CharacterControllerComponentSaveData)

**State Collections (1):**
12. `"OverthrowLoadouts"` - Loadout templates and player loadouts

**Trade-offs:**
- ✅ Better organization and isolation
- ✅ Granular control over save timing
- ✅ Easier debugging and testing
- ✅ Flexible performance tuning
- ❌ More configuration setup
- ❌ Slightly more complex global save triggers (must iterate collections or use SaveType masks)

**Configuration Example:**
```enforcescript
// All collections support all save types by default
void ConfigureCollections()
{
    // Manager collections
    PersistenceCollection.GetOrCreate("OverthrowTowns").SetSaveTypes(ESaveGameType.MANUAL | ESaveGameType.AUTO | ESaveGameType.SHUTDOWN);
    PersistenceCollection.GetOrCreate("OverthrowPlayers").SetSaveTypes(ESaveGameType.MANUAL | ESaveGameType.AUTO | ESaveGameType.SHUTDOWN);
    PersistenceCollection.GetOrCreate("OverthrowEconomy").SetSaveTypes(ESaveGameType.MANUAL | ESaveGameType.AUTO | ESaveGameType.SHUTDOWN);
    PersistenceCollection.GetOrCreate("OverthrowFactions").SetSaveTypes(ESaveGameType.MANUAL | ESaveGameType.AUTO | ESaveGameType.SHUTDOWN);
    PersistenceCollection.GetOrCreate("OverthrowRealEstate").SetSaveTypes(ESaveGameType.MANUAL | ESaveGameType.AUTO | ESaveGameType.SHUTDOWN);
    PersistenceCollection.GetOrCreate("OverthrowRecruits").SetSaveTypes(ESaveGameType.MANUAL | ESaveGameType.AUTO | ESaveGameType.SHUTDOWN);
    PersistenceCollection.GetOrCreate("OverthrowConfig").SetSaveTypes(ESaveGameType.MANUAL | ESaveGameType.AUTO | ESaveGameType.SHUTDOWN);

    // Entity collections
    PersistenceCollection.GetOrCreate("OverthrowPlaceables").SetSaveTypes(ESaveGameType.MANUAL | ESaveGameType.AUTO | ESaveGameType.SHUTDOWN);
    PersistenceCollection.GetOrCreate("OverthrowBuildings").SetSaveTypes(ESaveGameType.MANUAL | ESaveGameType.AUTO | ESaveGameType.SHUTDOWN);
    PersistenceCollection.GetOrCreate("OverthrowBases").SetSaveTypes(ESaveGameType.MANUAL | ESaveGameType.AUTO | ESaveGameType.SHUTDOWN);
    PersistenceCollection.GetOrCreate("OverthrowCharacters").SetSaveTypes(ESaveGameType.MANUAL | ESaveGameType.AUTO | ESaveGameType.SHUTDOWN);

    // State collections
    PersistenceCollection.GetOrCreate("OverthrowLoadouts").SetSaveTypes(ESaveGameType.MANUAL | ESaveGameType.AUTO | ESaveGameType.SHUTDOWN);
}
```

**Serializer-to-Collection Mapping:**

| Serializer | Collection | Phase |
|------------|------------|-------|
| OVT_ConfigManagerComponentSerializer | OverthrowConfig | Phase 2 |
| OVT_EconomyManagerComponentSerializer | OverthrowEconomy | Phase 2 |
| OVT_TownManagerComponentSerializer | OverthrowTowns | Phase 3 |
| OVT_OccupyingFactionManagerSerializer | OverthrowFactions | Phase 3 |
| OVT_ResistanceManagerComponentSerializer | OverthrowFactions | Phase 3 |
| OVT_RealEstateManagerComponentSerializer | OverthrowRealEstate | Phase 3 |
| OVT_RecruitManagerComponentSerializer | OverthrowRecruits | Phase 3 |
| OVT_PlayerManagerComponentSerializer | OverthrowPlayers | Phase 4 |
| OVT_PlaceableSerializer | OverthrowPlaceables | Phase 5 |
| OVT_BuildingSerializer | OverthrowBuildings | Phase 5 |
| OVT_BaseUpgradeSerializer | OverthrowBases | Phase 5 |
| OVT_CharacterControllerComponentSerializer | OverthrowCharacters | Phase 5 |
| OVT_LoadoutManagerSerializer | OverthrowLoadouts | Phase 6 |
| OVT_PlayerLoadoutSerializer | OverthrowLoadouts | Phase 6 |

**Note:** Collection assignment is typically done via `PersistenceConfig` on the component/entity, or programmatically when calling `StartTracking()`. Each serializer implementation should reference its assigned collection.

---

#### Decision 2: EPF_Component Helper Replacement

**Decision:** Add static `Find<T>()` method to existing `OVT_Component` base class.

**Rationale:**
- **Minimal code churn** - One-to-one replacement: `EPF_Component<T>.Find()` → `OVT_Component.Find<T>()`
- **Uses existing infrastructure** - `OVT_Component` already exists as base component class
- **Familiar location** - Already has utility methods like `GetRpl()`, `GetGUID()`
- **Easier migration** - Simple search-and-replace across 20+ files
- **Maintains readability** - `OVT_Component.Find<DamageManagerComponent>(entity)` is clear
- **Centralized** - Future improvements to component finding logic in one place
- **No new files** - Extends existing architecture

**Trade-offs:**
- ✅ Minimal migration effort (find-replace operation)
- ✅ Uses existing Overthrow base class
- ✅ Familiar pattern for existing developers
- ✅ No new files to create
- ❌ Slight indirection vs direct `entity.FindComponent()` calls

**Alternative Considered:** Direct `entity.FindComponent(T)` calls
- More explicit but requires more code changes
- Less consistent with existing Overthrow patterns

**Implementation:**
```enforcescript
// Add to Scripts/Game/Components/OVT_Component.c
class OVT_Component: ScriptComponent
{
    // ... existing methods (GetRpl, GetGUID, etc) ...

    //! Find a component on an entity (replacement for EPF_Component<T>.Find)
    //! @param entity Entity to search
    //! @return Component instance or null if not found
    static T Find<Class T>(IEntity entity)
    {
        if (!entity)
            return null;

        return T.Cast(entity.FindComponent(T));
    }
}
```

**Migration Pattern:**
```enforcescript
// OLD (EPF)
DamageManagerComponent dmg = EPF_Component<DamageManagerComponent>.Find(entity);

// NEW (Vanilla)
DamageManagerComponent dmg = OVT_Component.Find<DamageManagerComponent>(entity);
```

---

#### Decision 3: Serializer Naming Convention

**Decision:** `OVT_<ComponentName>Serializer` pattern (matches component/entity being serialized).

**Examples:**
- Component: `OVT_TownManagerComponent` → `OVT_TownManagerComponentSerializer`
- Component: `OVT_PlayerManagerComponent` → `OVT_PlayerManagerComponentSerializer`
- Entity: `OVT_PlaceableComponent` → `OVT_PlaceableSerializer` (entity serializer)
- State: `OVT_LoadoutManagerData` → `OVT_LoadoutManagerSerializer` (state serializer)

**Rationale:**
- **Clear association** - Immediately obvious which component/entity the serializer handles
- **Follows vanilla patterns** - Arma Reforger uses similar naming (e.g., `SCR_GadgetManagerComponentSerializer`)
- **IDE/search friendly** - Easy to find serializer for a given component
- **Consistency** - One rule applies to all serializers

**Trade-offs:**
- ✅ Crystal clear which component/entity is being serialized
- ✅ Matches vanilla Arma Reforger patterns
- ✅ Search-friendly (`OVT_TownManagerComponent` → find `OVT_TownManagerComponentSerializer`)
- ❌ Slightly longer names (acceptable given clarity benefit)

---

#### Decision 4: Loadout System Architecture

**Decision:** Migrate to `ScriptedStateSerializer` with proxy `PersistentState` classes (minimal refactoring).

**Rationale:**
- **Matches EPF pattern** - Loadouts currently use `EPF_PersistentScriptedState` (non-component state)
- **Minimal refactoring** - Closest 1:1 migration from EPF pattern
- **Appropriate pattern** - Loadouts are global data not tied to specific entity instances
- **Faster migration** - Don't need to refactor loadout architecture during persistence migration
- **Future-proof** - Can refactor to component-based later if desired (separate concern)

**Trade-offs:**
- ✅ Minimal refactoring during migration
- ✅ Appropriate pattern for global non-entity state
- ✅ Matches existing EPF architecture
- ❌ Less "standard" than component-based (but valid pattern per vanilla docs)

**Implementation Approach:**
```enforcescript
// 1. Create proxy state classes
class OVT_LoadoutManagerData : PersistentState {}
class OVT_PlayerLoadoutData : PersistentState {}

// 2. Create state serializers
class OVT_LoadoutManagerSerializer : ScriptedStateSerializer
{
    override static typename GetTargetType() { return OVT_LoadoutManagerData; }

    override protected ESerializeResult Serialize(/* ... */)
    {
        // Access actual manager component
        OVT_LoadoutManagerComponent loadoutMgr = OVT_Global.GetLoadouts();

        // Serialize loadout data
        // ...
    }
}
```

---

#### Decision 5: System Migration Order

**Decision:** Migrate in order of complexity (simple → complex) to build confidence and patterns.

**Phase Order:**
1. **Foundation** - Helpers, utilities, infrastructure
2. **Simple Components** - Config, Economy (minimal data, no entity references)
3. **Manager Components** - Town, Occupying Faction, Resistance, RealEstate, Recruit (arrays/maps, no entity refs)
4. **Player System** - Player manager (complex maps, player identity integration)
5. **Entity Serializers** - Placeable, Building, Base, Character (spawn data + entity references)
6. **State Serializers** - Loadout system (global state)
7. **EPF Cleanup** - Remove all EPF dependencies, platform guards
8. **Integration & Testing** - End-to-end validation

**Rationale:**
- **Build confidence** - Start with simplest to validate patterns
- **Incremental learning** - Each phase teaches lessons for next
- **Early feedback** - Compile and test after each phase
- **Risk mitigation** - Complex entity references come later when patterns established
- **Dependency management** - Foundation before usage

**Trade-offs:**
- ✅ Lower risk - validate patterns early
- ✅ Incremental progress - user can compile/test after each phase
- ✅ Build expertise - simple systems inform complex ones
- ❌ Slightly more phases (but better for learning/validation)

---

### System Architecture

```
Vanilla Persistence Architecture (Target State)

┌─────────────────────────────────────────────────────────────────┐
│ PersistenceSystem (Native C++ Singleton)                        │
│ - StartTracking() / StopTracking()                              │
│ - GetId() / SetId() / FindById()                                │
│ - WhenAvailable() (async entity resolution)                     │
│ - TriggerSave() / Save()                                        │
└─────────────────────────────────────────────────────────────────┘
                              │
                              │ manages
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│ PersistenceCollection: "Overthrow"                              │
│ - Save types: MANUAL | AUTO | SHUTDOWN                          │
│ - Contains all Overthrow serializers                            │
└─────────────────────────────────────────────────────────────────┘
                              │
                              │ contains
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│ Serializers (15 total)                                          │
│                                                                 │
│ Component Serializers (8):                                     │
│   - OVT_TownManagerComponentSerializer                          │
│   - OVT_PlayerManagerComponentSerializer                        │
│   - OVT_EconomyManagerComponentSerializer                       │
│   - OVT_OccupyingFactionManagerSerializer                       │
│   - OVT_RealEstateManagerComponentSerializer                    │
│   - OVT_RecruitManagerComponentSerializer                       │
│   - OVT_ResistanceManagerComponentSerializer                    │
│   - OVT_ConfigManagerComponentSerializer                        │
│                                                                 │
│ Entity Serializers (5):                                        │
│   - OVT_PlaceableSerializer                                     │
│   - OVT_BuildingSerializer                                      │
│   - OVT_BaseUpgradeSerializer                                   │
│   - OVT_CharacterControllerComponentSerializer                  │
│   - OVT_OverthrowGameModeSerializer (placeholder)               │
│                                                                 │
│ State Serializers (2):                                         │
│   - OVT_LoadoutManagerSerializer                                │
│   - OVT_PlayerLoadoutSerializer                                 │
└─────────────────────────────────────────────────────────────────┘
                              │
                              │ serializes
                              ↓
┌─────────────────────────────────────────────────────────────────┐
│ Game State (Components, Entities, Data)                         │
│                                                                 │
│ OVT_OverthrowGameMode                                           │
│ ├── Manager Components (on game mode entity)                   │
│ │   ├── OVT_TownManagerComponent (singleton)                   │
│ │   ├── OVT_PlayerManagerComponent (singleton)                 │
│ │   ├── OVT_EconomyManagerComponent (singleton)                │
│ │   └── ... (other managers)                                   │
│ │                                                               │
│ ├── World Entities (spawned in world)                          │
│ │   ├── Placeables (player-placed objects)                     │
│ │   ├── Buildings (owned/rented buildings)                     │
│ │   ├── Bases (resistance bases with upgrades)                 │
│ │   └── Characters (recruits, NPCs)                            │
│ │                                                               │
│ └── Global State (non-entity data)                             │
│     ├── Loadout Manager State (saved loadouts)                 │
│     └── Player Loadout State (per-player loadouts)             │
└─────────────────────────────────────────────────────────────────┘
```

**Data Flow:**

```
SAVE FLOW:
1. User triggers save (manual command, auto-save, shutdown)
2. PersistenceSystem.TriggerSave(ESaveGameType)
3. For each tracked entity/component:
   a. Find matching Serializer via GetTargetType()
   b. Call Serializer.Serialize(entity, context)
   c. Write data to context (context.Write(...))
4. Native system writes to disk

LOAD FLOW:
1. Server starts, loads save file
2. PersistenceSystem restores data
3. For each saved record:
   a. Find matching Serializer via GetTargetType()
   b. For entity serializers:
      - Call DeserializeSpawnData() → spawn entity
      - Call Deserialize() → restore state
   c. For component/state serializers:
      - Call Deserialize() → restore state to existing component
4. Entity references resolved via WhenAvailable()
```

---

## Implementation Phases

### Phase 1: Foundation Setup (2-3 hours)

**Objective:** Create infrastructure, helpers, and base patterns before migrating data.

**Tasks:**

1.1. **Update OVT_Component with Find<T>() helper** (15 min)
   - File: `Scripts/Game/Components/OVT_Component.c`
   - Add `Find<T>(IEntity)` static method to existing class
   - Add documentation/comments
   - Compile and verify syntax

1.2. **Create serializer directory structure** (10 min)
   - Create: `Scripts/Game/Persistence/Serializers/Components/`
   - Create: `Scripts/Game/Persistence/Serializers/Entities/`
   - Create: `Scripts/Game/Persistence/Serializers/States/`
   - Create: `Scripts/Game/Persistence/States/` (for proxy classes)

1.3. **Create template/example serializers** (1 hour)
   - Create: `Scripts/Game/Persistence/Serializers/Components/_OVT_ComponentSerializerTemplate.c` (reference template)
   - Create: `Scripts/Game/Persistence/Serializers/Entities/_OVT_EntitySerializerTemplate.c`
   - Create: `Scripts/Game/Persistence/Serializers/States/_OVT_StateSerializerTemplate.c`
   - Include inline documentation and examples
   - Reference research.md patterns

1.4. **Update OVT_PersistenceManagerComponent** (1 hour)
   - File: `Scripts/Game/GameMode/Managers/OVT_PersistenceManagerComponent.c`
   - Replace EPF inheritance with vanilla system integration
   - Remove EPF-specific event handlers
   - Hook into `SCR_PersistenceSystem` events:
     - `GetOnBeforeSave()` - pre-save logic
     - `GetOnAfterSave()` - post-save feedback/logging
     - `GetOnStateChanged()` - system lifecycle
   - Implement save/load triggers using `PersistenceSystem.TriggerSave()`
   - Remove `SaveRecruits()` manual save logic (will be automatic)
   - Remove EPF-specific methods (`HasSaveGame()`, `WipeSave()` - implement vanilla versions later)

1.5. **Configure 12 PersistenceCollections** (30 min)
   - In `OVT_PersistenceManagerComponent` or initialization code
   - Create all 12 collections using `PersistenceCollection.GetOrCreate()`:
     - **Manager Collections (7):** OverthrowTowns, OverthrowPlayers, OverthrowEconomy, OverthrowFactions, OverthrowRealEstate, OverthrowRecruits, OverthrowConfig
     - **Entity Collections (4):** OverthrowPlaceables, OverthrowBuildings, OverthrowBases, OverthrowCharacters
     - **State Collections (1):** OverthrowLoadouts
   - Set `ESaveGameType` masks for each (all support MANUAL | AUTO | SHUTDOWN by default)
   - Add helper method `GetCollectionForComponent()` or similar for serializers to reference
   - Document collection purpose in comments

1.6. **Create test mission/scenario** (30 min)
   - Set up test scenario for incremental testing
   - Configure mission header with persistence settings
   - Document test procedure for user

**Files Created:**
- `Scripts/Game/Persistence/Serializers/Components/_OVT_ComponentSerializerTemplate.c`
- `Scripts/Game/Persistence/Serializers/Entities/_OVT_EntitySerializerTemplate.c`
- `Scripts/Game/Persistence/Serializers/States/_OVT_StateSerializerTemplate.c`

**Files Modified:**
- `Scripts/Game/Components/OVT_Component.c` (add Find<T>() method)
- `Scripts/Game/GameMode/Managers/OVT_PersistenceManagerComponent.c`

**Acceptance Criteria:**
- ✅ Project compiles in Workbench with no errors
- ✅ `OVT_Component.Find<T>()` is callable and compiles
- ✅ Directory structure created
- ✅ Template files provide clear examples

**Estimated Effort:** 2-3 hours

---

### Phase 2: Simple Component Serializers (3-4 hours)

**Objective:** Migrate simplest components with no entity references to validate basic patterns.

**Systems:** Config, Economy

#### 2.1. OVT_ConfigManagerComponent (1 hour)

**Current EPF Implementation:** `Scripts/Game/GameMode/Persistence/Components/OVT_ConfigSaveData.c`
- Saves: `OVT_DifficultySettings m_Difficulty`
- Complexity: Very simple - single ref object

**Migration Steps:**

1. Create `Scripts/Game/Persistence/Serializers/Components/OVT_ConfigManagerComponentSerializer.c`
2. Implement `ScriptedComponentSerializer`:
   ```enforcescript
   class OVT_ConfigManagerComponentSerializer : ScriptedComponentSerializer
   {
       override static typename GetTargetType()
       {
           return OVT_OverthrowConfigComponent;
       }

       override protected ESerializeResult Serialize(
           IEntity owner,
           GenericComponent component,
           BaseSerializationSaveContext context)
       {
           OVT_OverthrowConfigComponent config = OVT_OverthrowConfigComponent.Cast(component);

           context.WriteValue("version", 1);
           context.Write(config.m_Difficulty);

           return ESerializeResult.OK;
       }

       override protected bool Deserialize(
           IEntity owner,
           GenericComponent component,
           BaseSerializationLoadContext context)
       {
           OVT_OverthrowConfigComponent config = OVT_OverthrowConfigComponent.Cast(component);

           int version;
           context.Read(version);
           context.Read(config.m_Difficulty);

           return true;
       }
   }
   ```

3. Update `OVT_OverthrowConfigComponent` to start tracking:
   ```enforcescript
   override void OnPostInit(IEntity owner)
   {
       super.OnPostInit(owner);

       if (Replication.IsServer())
           PersistenceSystem.GetInstance().StartTracking(GetOwner());
   }
   ```

4. Compile and test

**Files Created:**
- `Scripts/Game/Persistence/Serializers/Components/OVT_ConfigManagerComponentSerializer.c`

**Files Modified:**
- `Scripts/Game/Components/OVT_OverthrowConfigComponent.c` (add tracking)

---

#### 2.2. OVT_EconomyManagerComponent (1.5 hours)

**Current EPF Implementation:** `Scripts/Game/GameMode/Persistence/Components/OVT_EconomySaveData.c`
- Saves: `int m_iResistanceMoney`, `float m_fTaxRate`
- Complexity: Simple primitives

**Migration Steps:**

1. Create `Scripts/Game/Persistence/Serializers/Components/OVT_EconomyManagerComponentSerializer.c`
2. Implement serializer (similar pattern to Config)
3. Update `OVT_EconomyManagerComponent` to start tracking in `OnPostInit()`
4. Compile and test

**Files Created:**
- `Scripts/Game/Persistence/Serializers/Components/OVT_EconomyManagerComponentSerializer.c`

**Files Modified:**
- `Scripts/Game/GameMode/Managers/OVT_EconomyManagerComponent.c` (add tracking)

---

#### 2.3. First EPF_Component Helper Migration Pass (30 min)

Update 2-3 files to validate `OVT_Component` pattern:
- `Scripts/Game/Global/OVT_Global.c` (lines 425, 432 - `FilterDeadBodiesAndWeapons()`)
- `Scripts/Game/UserActions/OVT_ManageBaseAction.c`
- One other simple file

**Pattern:**
```enforcescript
// OLD
DamageManagerComponent dmg = EPF_Component<DamageManagerComponent>.Find(entity);

// NEW
DamageManagerComponent dmg = OVT_Component.Find<DamageManagerComponent>(entity);
```

---

**Phase 2 Acceptance Criteria:**
- ✅ Config and Economy serialize/deserialize correctly
- ✅ Manual save/load tested in Workbench
- ✅ `OVT_Component` pattern validated in 2-3 files
- ✅ No compile errors
- ✅ Print statements confirm data is saving/loading

**Estimated Effort:** 3-4 hours

---

### Phase 3: Manager Component Serializers (6-8 hours)

**Objective:** Migrate manager components with arrays/maps but no entity references.

**Systems:** Town, Occupying Faction, Resistance, RealEstate, Recruit

#### 3.1. OVT_TownManagerComponent (1.5 hours)

**Current EPF Implementation:** `Scripts/Game/GameMode/Persistence/Components/OVT_TownSaveData.c`
- Saves: `array<ref OVT_TownData> m_aTowns`
- Complexity: Array of ref objects
- Special logic: Matches saved towns to existing world towns by location

**Migration Steps:**

1. Create `Scripts/Game/Persistence/Serializers/Components/OVT_TownManagerComponentSerializer.c`
2. Serialize array using `context.Write(m_aTowns)` (vanilla handles ref arrays automatically)
3. Deserialize with custom matching logic:
   ```enforcescript
   override protected bool Deserialize(/* ... */)
   {
       OVT_TownManagerComponent towns = OVT_TownManagerComponent.Cast(component);

       int version;
       context.Read(version);

       array<ref OVT_TownData> savedTowns();
       context.Read(savedTowns);

       // Match saved data to existing towns by location
       foreach(OVT_TownData savedTown : savedTowns)
       {
           OVT_TownData existing = towns.GetNearestTown(savedTown.location);
           if (!existing) continue;

           existing.CopyFrom(savedTown);
       }

       return true;
   }
   ```
4. Update `OVT_TownManagerComponent` to start tracking
5. Compile and test

**Files Created:**
- `Scripts/Game/Persistence/Serializers/Components/OVT_TownManagerComponentSerializer.c`

**Files Modified:**
- `Scripts/Game/GameMode/Managers/OVT_TownManagerComponent.c` (add tracking)

---

#### 3.2. OVT_OccupyingFactionManager (1 hour)

**Current EPF Implementation:** `Scripts/Game/GameMode/Persistence/Components/OVT_OccupyingFactionSaveData.c`
- Saves: Occupying faction state
- Complexity: Simple data

**Migration Steps:**
1. Create `Scripts/Game/Persistence/Serializers/Components/OVT_OccupyingFactionManagerSerializer.c`
2. Implement serializer
3. Add tracking to manager
4. Compile and test

**Files Created:**
- `Scripts/Game/Persistence/Serializers/Components/OVT_OccupyingFactionManagerSerializer.c`

**Files Modified:**
- `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c`

---

#### 3.3. OVT_ResistanceManagerComponent (1 hour)

**Current EPF Implementation:** `Scripts/Game/GameMode/Persistence/Components/OVT_ResistanceSaveData.c`
- Saves: Resistance faction state
- Complexity: Simple data

**Migration Steps:**
1. Create `Scripts/Game/Persistence/Serializers/Components/OVT_ResistanceManagerComponentSerializer.c`
2. Implement serializer
3. Add tracking
4. Compile and test

**Files Created:**
- `Scripts/Game/Persistence/Serializers/Components/OVT_ResistanceManagerComponentSerializer.c`

**Files Modified:**
- `Scripts/Game/GameMode/Managers/Factions/OVT_ResistanceFactionManager.c`

---

#### 3.4. OVT_RealEstateManagerComponent (2 hours)

**Current EPF Implementation:** `Scripts/Game/GameMode/Persistence/Components/OVT_RealEstateSaveData.c`
- Saves:
  - `array<ref OVT_WarehouseData> m_aWarehouses`
  - `map<string, ref array<vector>> m_mOwned` (player ID → building positions)
  - `map<string, ref array<vector>> m_mRented`
- Complexity: Maps with nested arrays, position-based entity finding on load
- Special logic: Converts position strings to vectors, finds buildings by position on load

**Migration Steps:**

1. Create `Scripts/Game/Persistence/Serializers/Components/OVT_RealEstateManagerComponentSerializer.c`
2. **Simplify serialization** - vanilla handles maps/arrays automatically:
   ```enforcescript
   override protected ESerializeResult Serialize(/* ... */)
   {
       OVT_RealEstateManagerComponent re = OVT_RealEstateManagerComponent.Cast(component);

       context.WriteValue("version", 1);
       context.Write(re.m_aWarehouses);

       // Convert string-based owned/rented to vector-based for clean serialization
       map<string, ref array<vector>> owned();
       for (int i = 0; i < re.m_mOwned.Count(); i++)
       {
           array<string> ownedArray = re.m_mOwned.GetElement(i);
           string playerId = re.m_mOwned.GetKey(i);
           owned[playerId] = new array<vector>();

           foreach (string posString : ownedArray)
               owned[playerId].Insert(posString.ToVector());
       }
       context.Write(owned);

       // Same for rented
       // ...

       return ESerializeResult.OK;
   }
   ```
3. Deserialize with position-based entity finding (preserve EPF logic)
4. Add tracking
5. Compile and test

**Files Created:**
- `Scripts/Game/Persistence/Serializers/Components/OVT_RealEstateManagerComponentSerializer.c`

**Files Modified:**
- `Scripts/Game/GameMode/Managers/OVT_RealEstateManagerComponent.c`

---

#### 3.5. OVT_RecruitManagerComponent (1.5 hours)

**Current EPF Implementation:** `Scripts/Game/GameMode/Persistence/Components/OVT_RecruitSaveData.c`
- Saves: Recruit state data
- Complexity: May have entity references to recruit characters (needs investigation)

**Migration Steps:**
1. Analyze current recruit persistence (may need entity references)
2. Create `Scripts/Game/Persistence/Serializers/Components/OVT_RecruitManagerComponentSerializer.c`
3. If entity references needed, use UUID + WhenAvailable pattern (see Phase 5)
4. Add tracking
5. Compile and test

**Note:** If recruit characters need entity persistence, this might move to Phase 5.

**Files Created:**
- `Scripts/Game/Persistence/Serializers/Components/OVT_RecruitManagerComponentSerializer.c`

**Files Modified:**
- `Scripts/Game/GameMode/Managers/OVT_RecruitManagerComponent.c`

---

**Phase 3 Acceptance Criteria:**
- ✅ All manager components serialize/deserialize correctly
- ✅ Arrays and maps handled cleanly
- ✅ Town matching logic preserved
- ✅ RealEstate position-based finding works
- ✅ Manual save/load tested for each system
- ✅ No compile errors
- ✅ Print statements confirm data integrity

**Estimated Effort:** 6-8 hours

---

### Phase 4: Player System (3-4 hours)

**Objective:** Migrate player manager with complex maps and player identity integration.

#### 4.1. OVT_PlayerManagerComponent (3-4 hours)

**Current EPF Implementation:** `Scripts/Game/GameMode/Persistence/Components/OVT_PlayerSaveData.c`
- Saves: `map<string, ref OVT_PlayerData> m_mPlayers` (persistent player ID → player data)
- Complexity: HIGH
  - Map with ref values
  - Extensive validation logic
  - Player identity integration (reconnection)
  - Invoker for player data loaded event
  - Nested OVT_PlayerData contains loadouts, money, faction standing, progression

**Key Challenges:**
- Player identity UUID integration
- Reconnection logic (existing player rejoins server)
- Validation during save/load
- Event invokers after load

**Migration Steps:**

1. Create `Scripts/Game/Persistence/Serializers/Components/OVT_PlayerManagerComponentSerializer.c`

2. Serialize with validation (preserve EPF logic):
   ```enforcescript
   override protected ESerializeResult Serialize(/* ... */)
   {
       OVT_PlayerManagerComponent players = OVT_PlayerManagerComponent.Cast(component);

       context.WriteValue("version", 1);

       // Validate and filter players before saving
       map<string, ref OVT_PlayerData> validPlayers();

       for (int i = 0; i < players.m_mPlayers.Count(); i++)
       {
           string persId = players.m_mPlayers.GetKey(i);
           OVT_PlayerData player = players.m_mPlayers.GetElement(i);

           // Skip invalid entries
           if (!persId || persId.IsEmpty() || !player)
               continue;

           // Skip uninitialized players
           if (player.name.IsEmpty() && !player.initialized && player.money == 0)
               continue;

           validPlayers[persId] = player;
       }

       context.Write(validPlayers);
       return ESerializeResult.OK;
   }
   ```

3. Deserialize with validation and event invocation:
   ```enforcescript
   override protected bool Deserialize(/* ... */)
   {
       OVT_PlayerManagerComponent players = OVT_PlayerManagerComponent.Cast(component);

       int version;
       context.Read(version);

       map<string, ref OVT_PlayerData> savedPlayers();
       context.Read(savedPlayers);

       if (!players.m_mPlayers)
           players.m_mPlayers = new map<string, ref OVT_PlayerData>();

       // Validate and load
       for (int i = 0; i < savedPlayers.Count(); i++)
       {
           string persId = savedPlayers.GetKey(i);
           OVT_PlayerData player = savedPlayers.GetElement(i);

           if (!persId || persId.IsEmpty() || !player)
               continue;

           players.m_mPlayers[persId] = player;
           players.m_OnPlayerDataLoaded.Invoke(player, persId);
       }

       return true;
   }
   ```

4. **Player Identity Integration:**
   - Existing player reconnection uses persistent player ID (string)
   - Vanilla persistence can use player identity UUID
   - Keep existing string-based mapping (works with vanilla)
   - No changes needed to player identity logic

5. Add tracking to `OVT_PlayerManagerComponent`

6. Test extensively:
   - New player join
   - Player disconnect/reconnect
   - Player data persistence across sessions
   - Multiple players concurrently

**Files Created:**
- `Scripts/Game/Persistence/Serializers/Components/OVT_PlayerManagerComponentSerializer.c`

**Files Modified:**
- `Scripts/Game/GameMode/Managers/OVT_PlayerManagerComponent.c`

**Testing Focus:**
- Player joins, data saved
- Player leaves, rejoins → data restored
- Multiple players → all data correct
- Player progression persists

**Acceptance Criteria:**
- ✅ Player data saves/loads correctly
- ✅ Player reconnection works
- ✅ Validation logic preserved
- ✅ Events fire correctly
- ✅ Multiple players tested
- ✅ No compile errors

**Estimated Effort:** 3-4 hours

---

### Phase 5: Entity Serializers (6-8 hours)

**Objective:** Migrate entity serializers with spawn data and entity references.

**Complexity:** HIGH - Requires UUID-based entity references with async WhenAvailable pattern.

#### 5.1. OVT_PlaceableSerializer (2-3 hours)

**Current EPF Implementation:** `Scripts/Game/GameMode/Persistence/OVT_PlaceableSaveData.c`
- Currently: Empty placeholder extending `EPF_ItemSaveData`
- EPF automatically saves spawn data (prefab, transform)

**Migration Steps:**

1. Create `Scripts/Game/Persistence/Serializers/Entities/OVT_PlaceableSerializer.c`

2. Implement `ScriptedEntitySerializer`:
   ```enforcescript
   class OVT_PlaceableSerializer : ScriptedEntitySerializer
   {
       override static typename GetTargetType()
       {
           // Return the placeable component type or entity type
           return OVT_PlaceableComponent; // Or appropriate type
       }

       // Serialize spawn data (prefab + transform)
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

       // Serialize entity state (if needed beyond spawn data)
       override protected ESerializeResult Serialize(
           IEntity entity,
           BaseSerializationSaveContext context)
       {
           // Add any additional state beyond spawn data
           context.WriteValue("version", 1);

           // Example: owner player ID, custom properties, etc.

           return ESerializeResult.OK;
       }

       // Deserialize entity state
       override protected bool Deserialize(
           IEntity entity,
           BaseSerializationLoadContext context)
       {
           int version;
           context.Read(version);

           // Restore additional state

           return true;
       }
   }
   ```

3. Configure entity for automatic tracking (or manual tracking on placement)

4. Test spawn/despawn cycle

**Files Created:**
- `Scripts/Game/Persistence/Serializers/Entities/OVT_PlaceableSerializer.c`

**Files Modified:**
- Placeable component/entity (add tracking on creation)

---

#### 5.2. OVT_BuildingSerializer (2 hours)

**Current EPF Implementation:** `Scripts/Game/GameMode/Persistence/OVT_BuildingSaveData.c`
- Saves: Building state (damage, ownership, etc.)
- Note: Buildings might not need spawn serialization (world-placed), just state

**Migration Steps:**

1. Determine if buildings are world-placed (no spawn needed) or dynamically spawned
2. If world-placed: Use `ScriptedComponentSerializer` for building component state
3. If spawned: Use `ScriptedEntitySerializer` with spawn data
4. Create appropriate serializer
5. Handle ownership/damage state
6. Add tracking
7. Test

**Files Created:**
- `Scripts/Game/Persistence/Serializers/Entities/OVT_BuildingSerializer.c` OR
- `Scripts/Game/Persistence/Serializers/Components/OVT_BuildingComponentSerializer.c`

---

#### 5.3. OVT_BaseUpgradeSerializer (2 hours)

**Current EPF Implementation:** `Scripts/Game/GameMode/Persistence/OVT_BaseUpgradeSaveData.c`
- Saves: Base upgrade progress and state
- May have entity references to base structures

**Migration Steps:**

1. Analyze base upgrade system for entity dependencies
2. Create `Scripts/Game/Persistence/Serializers/Entities/OVT_BaseUpgradeSerializer.c`
3. If entity references exist, use UUID + WhenAvailable:
   ```enforcescript
   // Serialize entity reference
   UUID baseEntityId = GetSystem().GetId(m_BaseEntity);
   context.Write(baseEntityId);

   // Deserialize with async resolution
   UUID baseEntityId;
   context.Read(baseEntityId);

   if (!baseEntityId.IsNull())
   {
       Tuple1<OVT_BaseUpgradeComponent> ctx(this);
       PersistenceWhenAvailableTask task(OnBaseAvailable, ctx);
       GetSystem().WhenAvailable(baseEntityId, task, 60.0);
   }

   // Callback
   protected static void OnBaseAvailable(
       Managed instance,
       PersistenceDeferredDeserializeTask task,
       bool expired,
       Managed context)
   {
       IEntity baseEntity = IEntity.Cast(instance);
       if (!baseEntity || expired)
           return;

       auto ctx = Tuple1<OVT_BaseUpgradeComponent>.Cast(context);
       if (ctx && ctx.param1)
           ctx.param1.SetBaseEntity(baseEntity);
   }
   ```

4. Add tracking
5. Test base creation, upgrade, save/load

**Files Created:**
- `Scripts/Game/Persistence/Serializers/Entities/OVT_BaseUpgradeSerializer.c`

---

#### 5.4. OVT_CharacterControllerComponentSerializer (1.5 hours)

**Current EPF Implementation:** `Scripts/Game/Entities/Character/OVT_CharacterControllerComponentSaveData.c`
- Saves: Character-specific controller state

**Migration Steps:**
1. Create `Scripts/Game/Persistence/Serializers/Components/OVT_CharacterControllerComponentSerializer.c`
2. Implement component serializer (likely no entity refs, just state)
3. Add tracking
4. Test with recruit characters

**Files Created:**
- `Scripts/Game/Persistence/Serializers/Components/OVT_CharacterControllerComponentSerializer.c`

---

#### 5.5. OVT_OverthrowGameModeSerializer (30 min)

**Current EPF Implementation:** `Scripts/Game/GameMode/Persistence/OVT_OverthrowSaveData.c`
- Currently: Empty placeholder

**Migration Steps:**
1. Create `Scripts/Game/Persistence/Serializers/Entities/OVT_OverthrowGameModeSerializer.c`
2. Leave as placeholder unless game mode needs entity-level persistence
3. Most data is in manager components, so this may remain empty

**Files Created:**
- `Scripts/Game/Persistence/Serializers/Entities/OVT_OverthrowGameModeSerializer.c`

---

**Phase 5 Acceptance Criteria:**
- ✅ All entity serializers implemented
- ✅ Spawn data serialization works (placeables respawn correctly)
- ✅ Entity references resolve via WhenAvailable
- ✅ Timeout handling for missing entities
- ✅ Buildings persist state
- ✅ Bases persist upgrades
- ✅ Characters persist controller state
- ✅ Manual save/load tested for each entity type
- ✅ No compile errors

**Estimated Effort:** 6-8 hours

---

### Phase 6: State Serializers (Loadout System) (2-3 hours)

**Objective:** Migrate loadout system from `EPF_PersistentScriptedState` to `ScriptedStateSerializer`.

**Systems:** Loadout Manager, Player Loadouts

#### 6.1. OVT_LoadoutManagerSerializer (1.5 hours)

**Current EPF Implementation:** `Scripts/Game/GameMode/SaveData/OVT_LoadoutManagerSaveData.c`
- Uses: `EPF_ComponentSaveData` (minimal save data)
- Saves: `m_iCachedLoadoutsCount`, `m_mLoadoutIdMapping`

**Migration Steps:**

1. Create proxy state class:
   ```enforcescript
   // Scripts/Game/Persistence/States/OVT_LoadoutManagerData.c
   class OVT_LoadoutManagerData : PersistentState {}
   ```

2. Create state serializer:
   ```enforcescript
   // Scripts/Game/Persistence/Serializers/States/OVT_LoadoutManagerSerializer.c
   class OVT_LoadoutManagerSerializer : ScriptedStateSerializer
   {
       override static typename GetTargetType()
       {
           return OVT_LoadoutManagerData;
       }

       override protected ESerializeResult Serialize(
           Managed instance,
           BaseSerializationSaveContext context)
       {
           OVT_LoadoutManagerComponent loadoutMgr = OVT_Global.GetLoadouts();
           if (!loadoutMgr)
               return ESerializeResult.DEFAULT;

           context.WriteValue("version", 1);
           context.Write(loadoutMgr.GetCachedLoadoutCount());
           context.Write(loadoutMgr.GetLoadoutIdMapping());

           return ESerializeResult.OK;
       }

       override protected bool Deserialize(
           Managed instance,
           BaseSerializationLoadContext context)
       {
           OVT_LoadoutManagerComponent loadoutMgr = OVT_Global.GetLoadouts();
           if (!loadoutMgr)
               return false;

           int version;
           context.Read(version);

           int cachedCount;
           context.Read(cachedCount);

           map<string, string> loadoutIdMapping();
           context.Read(loadoutIdMapping);

           if (loadoutIdMapping)
               loadoutMgr.RestoreLoadoutIdMapping(loadoutIdMapping);

           return true;
       }
   }
   ```

3. Instantiate and track proxy state:
   ```enforcescript
   // In OVT_LoadoutManagerComponent or game mode initialization
   OVT_LoadoutManagerData proxyState();
   PersistenceSystem.GetInstance().StartTracking(proxyState);
   ```

4. Test loadout saving/loading

**Files Created:**
- `Scripts/Game/Persistence/States/OVT_LoadoutManagerData.c`
- `Scripts/Game/Persistence/Serializers/States/OVT_LoadoutManagerSerializer.c`

**Files Modified:**
- `Scripts/Game/GameMode/Managers/OVT_LoadoutManagerComponent.c` (track proxy state)

---

#### 6.2. OVT_PlayerLoadoutSerializer (1.5 hours)

**Current EPF Implementation:** `Scripts/Game/GameMode/SaveData/OVT_PlayerLoadoutSaveData.c`
- Uses: `EPF_PersistentScriptedState`
- Saves: Per-player loadout data

**Migration Steps:**
1. Create proxy state class: `OVT_PlayerLoadoutData`
2. Create state serializer: `OVT_PlayerLoadoutSerializer`
3. Similar pattern to LoadoutManager
4. Track proxy state instances
5. Test player-specific loadouts

**Files Created:**
- `Scripts/Game/Persistence/States/OVT_PlayerLoadoutData.c`
- `Scripts/Game/Persistence/Serializers/States/OVT_PlayerLoadoutSerializer.c`

**Files Modified:**
- Loadout system components (track proxy states)

---

**Phase 6 Acceptance Criteria:**
- ✅ Loadout manager state persists
- ✅ Player loadouts persist
- ✅ Loadout ID mapping restored correctly
- ✅ Test: save loadout, restart, load loadout
- ✅ No compile errors

**Estimated Effort:** 2-3 hours

---

### Phase 7: EPF Cleanup (3-4 hours)

**Objective:** Remove ALL EPF/EDF dependencies and platform guards.

#### 7.1. Complete EPF_Component Helper Migration (1 hour)

Finish migrating all remaining files (17+ remaining):
- All files in `Scripts/Game/UserActions/`
- `Scripts/Game/UI/HUD/OVT_EconomyInfo.c`
- `Scripts/Game/Respawn/Logic/OVT_SpawnLogic.c`
- `Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c`
- `Scripts/Game/Controllers/OccupyingFaction/OVT_BaseControllerComponent.c`
- `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c`
- Any other files found via grep

**Process:**
1. Grep for all `EPF_Component` usage
2. For each file:
   - Replace `EPF_Component<T>.Find(entity)` with `OVT_Component.Find<T>(entity)`
   - Remove `#ifdef PLATFORM_CONSOLE` guards around EPF calls
   - Compile and verify
3. Verify no EPF imports remain

---

#### 7.2. Delete Old EPF SaveData Files (15 min)

Delete entire directories:
- `Scripts/Game/GameMode/Persistence/` (all EPF SaveData classes)
- `Scripts/Game/GameMode/SaveData/` (EPF state SaveData)

**Files Deleted (15 total):**

Component SaveData:
- `Scripts/Game/GameMode/Persistence/Components/OVT_TownSaveData.c`
- `Scripts/Game/GameMode/Persistence/Components/OVT_PlayerSaveData.c`
- `Scripts/Game/GameMode/Persistence/Components/OVT_EconomySaveData.c`
- `Scripts/Game/GameMode/Persistence/Components/OVT_OccupyingFactionSaveData.c`
- `Scripts/Game/GameMode/Persistence/Components/OVT_RealEstateSaveData.c`
- `Scripts/Game/GameMode/Persistence/Components/OVT_RecruitSaveData.c`
- `Scripts/Game/GameMode/Persistence/Components/OVT_ResistanceSaveData.c`
- `Scripts/Game/GameMode/Persistence/Components/OVT_ConfigSaveData.c`

Entity SaveData:
- `Scripts/Game/GameMode/Persistence/OVT_OverthrowSaveData.c`
- `Scripts/Game/GameMode/Persistence/OVT_PlaceableSaveData.c`
- `Scripts/Game/GameMode/Persistence/OVT_BuildingSaveData.c`
- `Scripts/Game/GameMode/Persistence/OVT_BaseUpgradeSaveData.c`
- `Scripts/Game/Entities/Character/OVT_CharacterControllerComponentSaveData.c`

State SaveData:
- `Scripts/Game/GameMode/SaveData/OVT_LoadoutManagerSaveData.c`
- `Scripts/Game/GameMode/SaveData/OVT_PlayerLoadoutSaveData.c`

---

#### 7.3. Remove EPF/EDF from Mod Dependencies (15 min)

Update mod configuration to remove EPF/EDF dependencies:
- Remove from `addon.gproj` or equivalent
- Remove from mission headers if referenced
- Verify mod loads without EPF/EDF present

---

#### 7.4. Remove Platform Guards (30 min)

Search and remove all `#ifdef PLATFORM_CONSOLE` guards related to persistence:

```bash
# Search for platform guards
grep -r "PLATFORM_CONSOLE" Scripts/Game/
```

Remove guards that were protecting EPF calls. Keep any guards unrelated to persistence.

---

#### 7.5. Update OVT_PersistenceManagerComponent (1 hour)

Final cleanup of persistence manager:
- Remove EPF inheritance completely
- Remove EPF-specific methods
- Implement vanilla equivalents:
  - `HasSaveGame()` → Check vanilla save file existence
  - `WipeSave()` → Delete vanilla save files
- Clean up logging/debugging
- Remove manual recruit saving (now automatic)

---

#### 7.6. Final Compile and Verification (30 min)

1. Full compile in Workbench
2. Resolve any remaining errors
3. Verify no EPF imports anywhere
4. Verify no platform guards related to persistence
5. Clean up any commented-out EPF code

---

**Phase 7 Acceptance Criteria:**
- ✅ ZERO EPF/EDF references in codebase
- ✅ ZERO `#ifdef PLATFORM_CONSOLE` guards for persistence
- ✅ All old SaveData files deleted
- ✅ EPF/EDF removed from mod dependencies
- ✅ Full compile with no errors
- ✅ No warnings about missing EPF classes
- ✅ `OVT_Component` used everywhere instead of EPF_Component

**Estimated Effort:** 3-4 hours

---

### Phase 8: Integration & Testing (4-6 hours)

**Objective:** Comprehensive end-to-end validation of entire persistence system.

#### 8.1. System-by-System Testing (2-3 hours)

Test each major system individually:

**Config System:**
1. Start server, modify difficulty settings
2. Trigger manual save
3. Restart server
4. Verify difficulty settings restored
5. Print debug: "Config loaded: <difficulty values>"

**Economy System:**
1. Change resistance money and tax rate
2. Save
3. Restart
4. Verify values restored

**Town System:**
1. Capture town, change stability
2. Save
3. Restart
4. Verify town ownership/stability

**Player System:**
1. Player A joins, earns money, changes faction standing
2. Player A disconnects
3. Save
4. Restart server
5. Player A reconnects → verify money/faction standing restored
6. Player B joins (new player) → verify new player created

**RealEstate System:**
1. Player purchases building
2. Save
3. Restart
4. Verify ownership restored

**Entity Systems (Placeables, Buildings, Bases):**
1. Place object/build base
2. Save
3. Restart
4. Verify entity respawns at correct location with correct state

**Loadout System:**
1. Save custom loadout
2. Restart
3. Verify loadout available

---

#### 8.2. Integration Testing (1-2 hours)

Test cross-system integration:

**Scenario 1: Full Game Session**
1. Fresh server start
2. Player joins, plays for 30 min:
   - Earn money
   - Capture town
   - Build base
   - Place objects
   - Save loadout
3. Trigger auto-save
4. Continue playing
5. Manual save
6. Shutdown server
7. Restart server
8. Player reconnects
9. Verify ALL state restored correctly

**Scenario 2: Multiple Players**
1. 2-3 players join
2. Each player:
   - Earns money separately
   - Captures different towns
   - Builds separate bases
3. Save
4. Restart
5. All players reconnect
6. Verify each player's data independent and correct

**Scenario 3: Player Reconnection**
1. Player joins, plays, disconnects
2. Save
3. Different player joins
4. First player reconnects
5. Verify first player's data restored (not mixed with second player)

---

#### 8.3. Edge Case Testing (1 hour)

**Test Corrupted/Missing Data:**
1. Delete save file mid-game → verify graceful handling
2. Corrupt save file → verify error handling
3. Missing entity (entity deleted, but UUID reference exists) → verify WhenAvailable timeout

**Test Empty States:**
1. Save immediately after server start (before anything happens)
2. Restart
3. Verify no crashes

**Test Large Data:**
1. Spawn many entities (100+ placeables)
2. Capture all towns
3. Multiple players with lots of data
4. Save/load → verify performance acceptable

**Test Rapid Save/Load:**
1. Trigger multiple saves quickly
2. Restart during save (if possible)
3. Verify data integrity

---

#### 8.4. Performance Validation (30 min)

Compare EPF vs vanilla performance:
1. Create large game state (many towns, players, entities)
2. Time save operation
3. Time load operation
4. Compare to EPF baseline (if available)
5. Verify vanilla is faster or comparable

---

#### 8.5. Documentation and User Instructions (1 hour)

Create user-facing documentation:

**Breaking Change Announcement:**
```markdown
# BREAKING CHANGE: Vanilla Persistence Migration

**Version:** X.X.X
**Date:** 2025-XX-XX

## What Changed
Overthrow has migrated from EPF (Enfusion Persistence Framework) to Arma Reforger's native persistence system.

## Impact
**ALL EXISTING SAVE FILES ARE INCOMPATIBLE**

- You MUST start a fresh game after this update
- Old saves cannot be migrated
- Servers must wipe their databases

## Why?
- Better performance (native C++ vs script-based)
- Cleaner code and easier maintenance
- Better platform support
- Aligns with Arma Reforger's official persistence system

## What to Do
1. Backup your current save if you want to keep it for the old version
2. Update Overthrow to version X.X.X
3. Start a new game
4. Enjoy improved performance and stability!
```

**Server Admin Guide:**
```markdown
# Server Admin Guide: Vanilla Persistence

## Save File Location
Saves are now stored in the standard Arma Reforger save location (managed by game).

## Wiping Saves
To wipe a save and start fresh:
1. Stop server
2. Delete save files via game interface or file system
3. Restart server

## Save Configuration
Configure save types in mission header:
- MANUAL - Player-triggered saves
- AUTO - Automatic periodic saves
- SHUTDOWN - Save on server shutdown

## Troubleshooting
- Check server logs for persistence errors
- "Save failed" errors indicate disk/permission issues
- Entity reference timeouts indicate missing entities (usually safe to ignore)
```

---

**Phase 8 Acceptance Criteria:**
- ✅ All systems tested individually - data persists correctly
- ✅ Integration scenarios pass - cross-system state preserved
- ✅ Edge cases handled gracefully
- ✅ Performance acceptable (save/load times reasonable)
- ✅ Multiple players tested concurrently
- ✅ Player reconnection works reliably
- ✅ Documentation created for users and server admins
- ✅ No data loss or corruption
- ✅ No crashes during save/load

**Estimated Effort:** 4-6 hours

---

## Key Technical Decisions

### Decision Summary

| Decision | Choice | Rationale |
|----------|--------|-----------|
| **Collection Organization** | Single "Overthrow" collection | Simpler, cohesive mod, all data typically saved together |
| **EPF_Component Replacement** | `OVT_Component.Find<T>()` | Minimal code churn, familiar pattern, one-to-one replacement |
| **Serializer Naming** | `OVT_<ComponentName>Serializer` | Clear association, matches vanilla patterns, search-friendly |
| **Loadout System** | `ScriptedStateSerializer` with proxy classes | Matches EPF pattern, minimal refactoring, appropriate for global state |
| **Migration Order** | Simple → Complex (8 phases) | Build confidence, incremental learning, validate patterns early |

### Entity Reference Resolution Strategy

**Pattern:** UUID + WhenAvailable for all entity references

**Why:**
- Entities may load in any order
- Referenced entity might not exist yet when deserializing
- Async resolution handles late-joiners, delayed spawns
- More robust than EPF's synchronous RplId pattern

**Template:**
```enforcescript
// Save
UUID entityId = GetSystem().GetId(m_Entity);
context.Write(entityId);

// Load
UUID entityId;
context.Read(entityId);

if (!entityId.IsNull())
{
    Tuple1<MyComponent> ctx(this);
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

    auto ctx = Tuple1<MyComponent>.Cast(context);
    if (ctx && ctx.param1)
        ctx.param1.SetEntity(entity);
}
```

### Array/Map Serialization Strategy

**Pattern:** Use native `context.Write()` for arrays/maps directly

**Why:**
- Vanilla system handles ref arrays/maps automatically
- No need for manual count tracking
- No need for manual loops
- Simpler, cleaner code

**Example:**
```enforcescript
// OLD EPF
serializer.Write(m_aItems.Count(), 16);
foreach (string item : m_aItems)
    serializer.WriteString(item);

// NEW Vanilla
context.Write(m_aItems); // That's it!
```

### Versioning Strategy

**Pattern:** ALL serializers MUST include version number from day one

**Why:**
- Future-proofs save data
- Allows graceful handling of format changes
- Standard best practice

**Template:**
```enforcescript
// Serialize
context.WriteValue("version", 1);

// Deserialize
int version;
if (!context.ReadValue("version", version))
    version = 1; // Old data without version

// Handle version differences
if (version >= 2)
    context.Read(m_NewField);
else
    m_NewField = default;
```

### Error Handling Approach

**Strategy:** Defensive programming with graceful degradation

**Patterns:**
- Validate data before saving (skip invalid entries)
- Validate data during loading (skip corrupt entries)
- Handle missing entities gracefully (timeout callbacks)
- Log warnings but don't crash
- Return success even if partial data loaded

**Example:**
```enforcescript
// Validate before saving
if (!data || data.IsEmpty())
{
    Print("[Overthrow] WARNING: Skipping invalid data", LogLevel.WARNING);
    return ESerializeResult.DEFAULT;
}

// Handle WhenAvailable timeout
protected static void OnEntityAvailable(/* ... */, bool expired, /* ... */)
{
    if (expired)
    {
        Print("[Overthrow] Entity reference timeout - entity not found", LogLevel.WARNING);
        return; // Gracefully handle missing entity
    }
}
```

---

## Testing Strategy

### Testing Philosophy

**Manual Play-Testing Focus:**
- Arma Reforger Workbench has no automated testing
- All testing is manual via play-testing
- Focus on realistic gameplay scenarios
- Test incremental (per phase) and comprehensive (end-to-end)

**Defensive Programming:**
- Extensive validation during save/load
- Print statements for debugging
- Graceful error handling
- User reports compile errors/test results

### Key Validation Areas per System

#### Config System
- Difficulty settings persist
- Server restart restores config
- Manual save/load works

#### Economy System
- Resistance money persists
- Tax rate persists
- Values correct after restart

#### Town System
- Town ownership persists
- Stability values persist
- Town matching by location works
- Multiple towns handled correctly

#### Player System
- New player joins → data created
- Player disconnects/reconnects → data restored
- Multiple players → independent data
- Player identity integration works
- Money, faction standing, loadouts persist

#### RealEstate System
- Building ownership persists
- Warehouse inventory persists
- Position-based building finding works
- Rented vs owned properties distinguished

#### Recruit System
- Recruit state persists
- Recruit entities respawn correctly

#### Entity Systems
- Placeables: Spawn location/orientation correct
- Buildings: State persists (damage, ownership)
- Bases: Upgrades persist
- Characters: Controller state persists

#### Loadout System
- Saved loadouts persist
- Player-specific loadouts persist
- Loadout ID mapping restored

### Edge Cases to Consider

**Data Integrity:**
- Empty collections (no players, no towns)
- Null/invalid data (corrupted saves)
- Missing entity references (entity deleted)
- Duplicate player IDs (shouldn't happen, but validate)

**Timing Issues:**
- Save during entity spawn
- Load before world fully initialized
- Player reconnects during load
- Multiple saves triggered rapidly

**Network/Multiplayer:**
- Multiple players join/leave during session
- Player reconnects with same identity
- Player reconnects with different identity (shouldn't happen)
- Server restarts mid-session

**Performance:**
- Large data sets (100+ entities, 10+ players)
- Rapid save/load cycles
- Save file size reasonable

### Performance Validation Approach

**Baseline Metrics:**
- Small game: <1s save, <2s load (10 entities, 1 player)
- Medium game: <3s save, <5s load (50 entities, 3 players)
- Large game: <10s save, <15s load (200 entities, 10 players)

**Comparison:**
- Compare to EPF performance (if baseline available)
- Vanilla should be faster (native C++)

**Monitoring:**
- Log save/load times
- Print debug statements
- User reports performance issues

### Player Reconnection Testing

**Critical Path:**
1. Player joins, earns money, captures town
2. Player disconnects
3. Server continues running
4. Different player joins
5. First player reconnects
6. Verify first player's data restored correctly (not mixed with other player)

**Variations:**
- Reconnect immediately
- Reconnect after delay
- Reconnect after server save
- Reconnect after server restart

### Server Restart Validation

**Standard Cycle:**
1. Play session (perform various actions)
2. Trigger manual save
3. Stop server
4. Start server
5. Verify all state restored:
   - Towns captured
   - Players data intact
   - Buildings owned
   - Entities spawned
   - Economy values correct

**Variations:**
- Shutdown save (no manual save)
- Auto-save (periodic)
- Restart immediately after save
- Restart after delay

---

## Dependencies

### External Dependencies

**Remove:**
- ❌ EPF (Enfusion Persistence Framework) - Completely removed
- ❌ EDF (Enfusion Database Framework) - Completely removed

**Add:**
- ✅ None - Vanilla persistence is built into Arma Reforger

### Internal Dependencies

**Component Manager Dependencies:**
- All manager components depend on persistence system
- `OVT_PersistenceManagerComponent` coordinates saves
- Managers start tracking in `OnPostInit()`

**Player System Dependencies:**
- Player manager depends on player identity UUID (already used for reconnection)
- Controller entities depend on player manager for data

**Entity Dependencies:**
- Placeables depend on prefab resources
- Buildings depend on position-based finding
- Bases depend on base manager (if exists)
- Recruits depend on recruit manager

**Data Dependencies:**
- Town data depends on world-placed town entities (matching by position)
- RealEstate depends on buildings (position-based finding)
- Player data depends on persistent player ID

### Phase Dependencies

**Dependency Chain:**

```
Phase 1 (Foundation)
  ↓
Phase 2 (Simple Components) ← validates basic patterns
  ↓
Phase 3 (Manager Components) ← uses patterns from Phase 2
  ↓
Phase 4 (Player System) ← uses patterns from Phase 2-3
  ↓
Phase 5 (Entity Serializers) ← uses async patterns + WhenAvailable
  ↓
Phase 6 (State Serializers) ← uses state serializer pattern
  ↓
Phase 7 (EPF Cleanup) ← requires all migrations complete
  ↓
Phase 8 (Integration Testing) ← validates entire system
```

**Critical Path:**
- Phase 1 MUST complete before any other phase
- Phase 2-4 can partially overlap (simple components independent)
- Phase 5 depends on understanding from Phase 2-4
- Phase 7 requires Phase 2-6 complete
- Phase 8 requires ALL phases complete

---

## Risks & Mitigation

### Risk 1: Breaking Change for Players

**Risk:** All existing save files become invalid.

**Impact:** HIGH - Players lose progress, servers must wipe.

**Likelihood:** Certain (guaranteed by design)

**Mitigation:**
- ✅ Clearly communicate breaking change in release notes
- ✅ Provide advance warning (announce before release)
- ✅ Time release appropriately (avoid mid-campaign)
- ✅ Consider this a "fresh start" for major version bump
- ✅ Document why change was necessary (performance, maintainability)

**Status:** Accepted - User confirmed fresh start is acceptable.

---

### Risk 2: Complex Entity References

**Risk:** Async entity resolution via `WhenAvailable` is more complex than EPF's synchronous pattern.

**Impact:** MEDIUM - Potential for race conditions, timing bugs, references not resolving.

**Likelihood:** Medium (new pattern for Overthrow developers)

**Mitigation:**
- ✅ Study vanilla examples thoroughly (see research.md)
- ✅ Use proven Tuple context pattern
- ✅ Test entity reference resolution extensively
- ✅ Handle timeout cases gracefully (60s default)
- ✅ Start with simple references in Phase 5, build complexity
- ✅ Print debug statements for entity resolution
- ✅ Document pattern clearly in templates

**Status:** Manageable with careful implementation and testing.

---

### Risk 3: Big Bang Migration Risk

**Risk:** Migrating all systems at once increases risk of bugs and testing complexity.

**Impact:** HIGH - One bug could break entire persistence, difficult to isolate issues.

**Likelihood:** Medium (large scope)

**Mitigation:**
- ✅ Implement in 8 logical phases (simple → complex)
- ✅ Test each phase thoroughly before moving to next
- ✅ Keep EPF code temporarily for reference (delete in Phase 7)
- ✅ Use version control branch (`vanilla-persistence`) for rollback
- ✅ Extensive play-testing after each phase
- ✅ User compiles and tests incrementally
- ✅ Print debug statements throughout
- ✅ Can pause between phases if issues arise

**Status:** Mitigated by phased implementation with incremental testing.

---

### Risk 4: Missing Edge Cases

**Risk:** EPF may handle edge cases that vanilla system doesn't (or handles differently).

**Impact:** MEDIUM - Data loss or corruption in edge cases (null data, empty collections, missing entities).

**Likelihood:** Medium (unknown unknowns)

**Mitigation:**
- ✅ Review all EPF SaveData classes for special handling (preserve validation logic)
- ✅ Test edge cases explicitly (null data, empty collections, missing entities)
- ✅ Add defensive checks in deserializers
- ✅ Log warnings for unexpected data states
- ✅ Validate data before saving (skip invalid entries)
- ✅ Graceful degradation (load partial data if some corrupt)
- ✅ WhenAvailable timeout handling (missing entities)

**Status:** Mitigated by defensive programming and explicit edge case testing.

---

### Risk 5: Performance Regression

**Risk:** Vanilla system might perform differently than EPF in practice.

**Impact:** LOW to MEDIUM - Potential save/load time changes affecting gameplay.

**Likelihood:** Low (native C++ should be faster)

**Mitigation:**
- ✅ Native C++ should be faster than EPF's script-based approach
- ✅ Monitor save/load times during testing
- ✅ Use `WriteDefault()` to skip unchanged data
- ✅ Profile if issues arise
- ✅ Establish baseline metrics
- ✅ Compare to EPF performance (if available)
- ✅ Test with large data sets (Phase 8)

**Status:** Low risk - vanilla should be faster or comparable.

---

### Risk 6: Incomplete Documentation

**Risk:** Vanilla system is newer, may have less community documentation than EPF.

**Impact:** LOW - Slower development due to research needs, trial and error.

**Likelihood:** Low (research complete)

**Mitigation:**
- ✅ research.md provides comprehensive documentation
- ✅ Vanilla examples in Arma Reforger source code (20+ serializers)
- ✅ Can reference EPF author's design intent (same author)
- ✅ Template serializers with inline documentation
- ✅ Patterns validated in research phase

**Status:** Mitigated by thorough research phase and vanilla examples.

---

### Risk 7: Network Synchronization Confusion

**Risk:** Persistence (UUID) vs replication (RplId) confusion.

**Impact:** MEDIUM - Bugs from mixing persistence and networking concerns.

**Likelihood:** Medium (common confusion point)

**Mitigation:**
- ✅ Clear documentation on persistence vs replication separation
- ✅ Use UUID ONLY for persistence
- ✅ Use RplId ONLY for runtime network references
- ✅ Code comments explaining difference
- ✅ Templates demonstrate correct patterns
- ✅ Code reviews to catch mixing

**Status:** Mitigated by clear separation of concerns and documentation.

---

### Risk 8: WhenAvailable Callback Complexity

**Risk:** Static callbacks with Tuple context passing are less intuitive than direct access.

**Impact:** MEDIUM - Increased complexity, potential for context mismanagement.

**Likelihood:** Medium (new pattern)

**Mitigation:**
- ✅ Clear templates with documented examples
- ✅ Use Tuple classes consistently (Tuple1, Tuple2, Tuple3)
- ✅ Validate context in callbacks (`if (!ctx || !ctx.param1) return;`)
- ✅ Test callbacks thoroughly
- ✅ Document callback lifecycle
- ✅ Reference vanilla examples (research.md)

**Status:** Manageable with clear templates and testing.

---

### Risk 9: Loadout System Refactoring

**Risk:** Loadout system using StateSerializer might be less standard than component-based.

**Impact:** LOW - Works correctly but might require future refactoring.

**Likelihood:** Low (valid pattern per vanilla docs)

**Mitigation:**
- ✅ StateSerializer is valid pattern for global non-entity state
- ✅ Matches EPF's PersistentScriptedState approach (minimal refactoring)
- ✅ Can refactor to component-based later if desired (separate concern)
- ✅ Focus on migration, not architecture changes
- ✅ Validate pattern with vanilla examples (SCR_TaskSystemSerializer)

**Status:** Low risk - valid pattern, future refactoring optional.

---

### Risk 10: User Testing Availability

**Risk:** User must be available to compile and test after each phase.

**Impact:** MEDIUM - Migration blocked if user unavailable.

**Likelihood:** Low to Medium (depends on user schedule)

**Mitigation:**
- ✅ Phases are self-contained (can pause between)
- ✅ Clear acceptance criteria for each phase
- ✅ Detailed testing instructions
- ✅ User can test asynchronously (no real-time interaction needed)
- ✅ Print debug statements reduce need for interactive debugging

**Status:** Manageable - phases allow flexible scheduling.

---

## Migration Checklist

High-level milestones for tracking progress:

### Foundation
- [ ] `OVT_Component.Find<T>()` method added and compiling
- [ ] Serializer directory structure created
- [ ] Template serializers created with documentation
- [ ] `OVT_PersistenceManagerComponent` updated for vanilla system
- [ ] Test scenario configured

### Component Serializers
- [ ] Config serializer implemented and tested
- [ ] Economy serializer implemented and tested
- [ ] Town serializer implemented and tested
- [ ] Occupying Faction serializer implemented and tested
- [ ] Resistance serializer implemented and tested
- [ ] RealEstate serializer implemented and tested
- [ ] Recruit serializer implemented and tested

### Player System
- [ ] Player manager serializer implemented
- [ ] Player identity integration working
- [ ] Player reconnection tested
- [ ] Multiple players tested

### Entity Serializers
- [ ] Placeable serializer implemented with spawn data
- [ ] Building serializer implemented
- [ ] Base upgrade serializer implemented
- [ ] Character controller serializer implemented
- [ ] Game mode serializer placeholder created
- [ ] Entity reference resolution (WhenAvailable) working
- [ ] Entity respawning tested

### State Serializers
- [ ] Loadout manager state serializer implemented
- [ ] Player loadout state serializer implemented
- [ ] Proxy state classes created and tracked
- [ ] Loadout persistence tested

### EPF Cleanup
- [ ] All `EPF_Component` usage replaced with `OVT_Component`
- [ ] All EPF SaveData files deleted
- [ ] EPF/EDF removed from mod dependencies
- [ ] All `#ifdef PLATFORM_CONSOLE` guards removed
- [ ] Full compile with no EPF references
- [ ] `OVT_PersistenceManagerComponent` fully cleaned up

### Integration & Testing
- [ ] All systems tested individually
- [ ] Integration scenarios tested (full game session)
- [ ] Multiple players tested concurrently
- [ ] Player reconnection tested extensively
- [ ] Server restart tested
- [ ] Edge cases tested (empty data, corrupted saves, missing entities)
- [ ] Performance validated (save/load times acceptable)
- [ ] User documentation created
- [ ] Breaking change announcement drafted

### Release Preparation
- [ ] All phases complete
- [ ] No known bugs
- [ ] Performance acceptable
- [ ] Documentation complete
- [ ] Breaking change clearly communicated
- [ ] Release notes prepared

---

## Appendices

### Appendix A: File Changes Summary

#### Files to CREATE (22 files)

**Utilities (1):**
- `Scripts/Game/Components/OVT_Component.c`

**Component Serializers (8):**
- `Scripts/Game/Persistence/Serializers/Components/OVT_ConfigManagerComponentSerializer.c`
- `Scripts/Game/Persistence/Serializers/Components/OVT_EconomyManagerComponentSerializer.c`
- `Scripts/Game/Persistence/Serializers/Components/OVT_TownManagerComponentSerializer.c`
- `Scripts/Game/Persistence/Serializers/Components/OVT_OccupyingFactionManagerSerializer.c`
- `Scripts/Game/Persistence/Serializers/Components/OVT_RealEstateManagerComponentSerializer.c`
- `Scripts/Game/Persistence/Serializers/Components/OVT_RecruitManagerComponentSerializer.c`
- `Scripts/Game/Persistence/Serializers/Components/OVT_ResistanceManagerComponentSerializer.c`
- `Scripts/Game/Persistence/Serializers/Components/OVT_CharacterControllerComponentSerializer.c`

**Entity Serializers (5):**
- `Scripts/Game/Persistence/Serializers/Entities/OVT_PlaceableSerializer.c`
- `Scripts/Game/Persistence/Serializers/Entities/OVT_BuildingSerializer.c`
- `Scripts/Game/Persistence/Serializers/Entities/OVT_BaseUpgradeSerializer.c`
- `Scripts/Game/Persistence/Serializers/Entities/OVT_OverthrowGameModeSerializer.c`

**State Serializers (2):**
- `Scripts/Game/Persistence/Serializers/States/OVT_LoadoutManagerSerializer.c`
- `Scripts/Game/Persistence/Serializers/States/OVT_PlayerLoadoutSerializer.c`

**Proxy State Classes (2):**
- `Scripts/Game/Persistence/States/OVT_LoadoutManagerData.c`
- `Scripts/Game/Persistence/States/OVT_PlayerLoadoutData.c`

**Templates (3):**
- `Scripts/Game/Persistence/Serializers/Components/_OVT_ComponentSerializerTemplate.c`
- `Scripts/Game/Persistence/Serializers/Entities/_OVT_EntitySerializerTemplate.c`
- `Scripts/Game/Persistence/Serializers/States/_OVT_StateSerializerTemplate.c`

**Documentation (1):**
- `docs/features/vanilla-persistence/BREAKING_CHANGE.md`

---

#### Files to MODIFY (25+ files)

**Manager Components (add tracking, remove EPF):**
- `Scripts/Game/GameMode/Managers/OVT_PersistenceManagerComponent.c` (major refactor)
- `Scripts/Game/Components/OVT_OverthrowConfigComponent.c`
- `Scripts/Game/GameMode/Managers/OVT_EconomyManagerComponent.c`
- `Scripts/Game/GameMode/Managers/OVT_TownManagerComponent.c`
- `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c`
- `Scripts/Game/GameMode/Managers/OVT_RealEstateManagerComponent.c`
- `Scripts/Game/GameMode/Managers/OVT_RecruitManagerComponent.c`
- `Scripts/Game/GameMode/Managers/Factions/OVT_ResistanceFactionManager.c`
- `Scripts/Game/GameMode/Managers/OVT_PlayerManagerComponent.c`
- `Scripts/Game/GameMode/Managers/OVT_LoadoutManagerComponent.c`

**EPF_Component Helper Migration (20+ files):**
- `Scripts/Game/Global/OVT_Global.c`
- `Scripts/Game/UserActions/OVT_ManageBaseAction.c`
- `Scripts/Game/UserActions/OVT_CaptureBaseAction.c`
- `Scripts/Game/UserActions/OVT_UnlockVehicleAction.c`
- `Scripts/Game/UserActions/OVT_UnloadStorageAction.c`
- `Scripts/Game/UI/HUD/OVT_EconomyInfo.c`
- `Scripts/Game/Respawn/Logic/OVT_SpawnLogic.c`
- `Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c`
- `Scripts/Game/Controllers/OccupyingFaction/OVT_BaseControllerComponent.c`
- Plus 10+ other UserAction files

**Entity/Component Classes (add tracking):**
- Placeable component/entity files
- Building component/entity files
- Base upgrade component files
- Character controller component files

**Mod Configuration:**
- `addon.gproj` or equivalent (remove EPF/EDF dependencies)
- Mission headers (remove EPF references, configure vanilla persistence)

---

#### Files to DELETE (15 files)

**Component SaveData (8):**
- `Scripts/Game/GameMode/Persistence/Components/OVT_TownSaveData.c`
- `Scripts/Game/GameMode/Persistence/Components/OVT_PlayerSaveData.c`
- `Scripts/Game/GameMode/Persistence/Components/OVT_EconomySaveData.c`
- `Scripts/Game/GameMode/Persistence/Components/OVT_OccupyingFactionSaveData.c`
- `Scripts/Game/GameMode/Persistence/Components/OVT_RealEstateSaveData.c`
- `Scripts/Game/GameMode/Persistence/Components/OVT_RecruitSaveData.c`
- `Scripts/Game/GameMode/Persistence/Components/OVT_ResistanceSaveData.c`
- `Scripts/Game/GameMode/Persistence/Components/OVT_ConfigSaveData.c`

**Entity SaveData (5):**
- `Scripts/Game/GameMode/Persistence/OVT_OverthrowSaveData.c`
- `Scripts/Game/GameMode/Persistence/OVT_PlaceableSaveData.c`
- `Scripts/Game/GameMode/Persistence/OVT_BuildingSaveData.c`
- `Scripts/Game/GameMode/Persistence/OVT_BaseUpgradeSaveData.c`
- `Scripts/Game/Entities/Character/OVT_CharacterControllerComponentSaveData.c`

**State SaveData (2):**
- `Scripts/Game/GameMode/SaveData/OVT_LoadoutManagerSaveData.c`
- `Scripts/Game/GameMode/SaveData/OVT_PlayerLoadoutSaveData.c`

**Directories to DELETE:**
- `Scripts/Game/GameMode/Persistence/` (entire directory)
- `Scripts/Game/GameMode/SaveData/` (entire directory)

---

### Appendix B: Code Templates

#### B.1. Component Serializer Template

```enforcescript
//! Vanilla persistence serializer for <ComponentName>
//! Replaces EPF SaveData class
class OVT_<ComponentName>Serializer : ScriptedComponentSerializer
{
    //! Specify which component this serializer handles
    override static typename GetTargetType()
    {
        return OVT_<ComponentName>;
    }

    //! Optional: Control when deserialization happens
    //! Options: BEFORE_POSTINIT, BEFORE_EONINIT, AFTER_ENTITY_FINALIZE (default)
    override static EComponentDeserializeEvent GetDeserializeEvent()
    {
        return EComponentDeserializeEvent.AFTER_ENTITY_FINALIZE;
    }

    //! Serialize component data to save context
    override protected ESerializeResult Serialize(
        IEntity owner,
        GenericComponent component,
        BaseSerializationSaveContext context)
    {
        OVT_<ComponentName> comp = OVT_<ComponentName>.Cast(component);
        if (!comp)
            return ESerializeResult.ERROR;

        // Always version your data
        context.WriteValue("version", 1);

        // Write data
        context.Write(comp.m_iSomeValue);
        context.Write(comp.m_sSomeName);
        context.Write(comp.m_aSomeArray);
        context.Write(comp.m_mSomeMap);

        // Optional: Skip default values
        // context.WriteDefault(comp.m_iOptional, 0);

        return ESerializeResult.OK;
    }

    //! Deserialize component data from load context
    override protected bool Deserialize(
        IEntity owner,
        GenericComponent component,
        BaseSerializationLoadContext context)
    {
        OVT_<ComponentName> comp = OVT_<ComponentName>.Cast(component);
        if (!comp)
            return false;

        // Read version
        int version;
        if (!context.ReadValue("version", version))
            version = 1; // Old data without version

        // Read data
        context.Read(comp.m_iSomeValue);
        context.Read(comp.m_sSomeName);
        context.Read(comp.m_aSomeArray);
        context.Read(comp.m_mSomeMap);

        // Handle version differences
        if (version >= 2)
            context.Read(comp.m_iNewField);
        else
            comp.m_iNewField = 0; // Default for old data

        return true;
    }
}
```

**Component Setup (add to component class):**
```enforcescript
class OVT_<ComponentName> : ScriptComponent
{
    // ... component code ...

    override void OnPostInit(IEntity owner)
    {
        super.OnPostInit(owner);

        // Start tracking for persistence (server only)
        if (Replication.IsServer())
            PersistenceSystem.GetInstance().StartTracking(owner);
    }
}
```

---

#### B.2. Entity Serializer Template with Spawn Data

```enforcescript
//! Vanilla persistence serializer for <EntityName>
//! Handles both spawn data (prefab + transform) and entity state
class OVT_<EntityName>Serializer : ScriptedEntitySerializer
{
    //! Specify which entity type this serializer handles
    override static typename GetTargetType()
    {
        return OVT_<EntityName>; // Or component type
    }

    //! Serialize spawn data (prefab + spawn parameters)
    override protected ESerializeResult SerializeSpawnData(
        IEntity entity,
        BaseSerializationSaveContext context,
        out EntitySpawnParams defaultData)
    {
        // Get prefab name
        ResourceName prefab = entity.GetPrefabData().GetPrefabName();

        // Get transform
        EntitySpawnParams params();
        entity.GetTransform(params.Transform);

        // Write spawn data
        context.Write(prefab);
        context.Write(params);

        return ESerializeResult.OK;
    }

    //! Deserialize spawn data (called before entity spawned)
    override protected bool DeserializeSpawnData(
        out ResourceName prefab,
        out EntitySpawnParams params,
        BaseSerializationLoadContext context)
    {
        context.Read(prefab);
        context.Read(params);
        return true;
    }

    //! Serialize entity state (called after spawn data)
    override protected ESerializeResult Serialize(
        IEntity entity,
        BaseSerializationSaveContext context)
    {
        context.WriteValue("version", 1);

        // Write entity state (beyond spawn data)
        // Example: owner, damage, custom properties

        return ESerializeResult.OK;
    }

    //! Deserialize entity state (called after entity spawned)
    override protected bool Deserialize(
        IEntity entity,
        BaseSerializationLoadContext context)
    {
        int version;
        context.Read(version);

        // Read entity state

        return true;
    }
}
```

---

#### B.3. State Serializer Template (Non-Entity)

```enforcescript
//! Proxy state class for persistence
//! This is a lightweight placeholder - actual data lives in manager/system
class OVT_<SystemName>Data : PersistentState {}

//! State serializer for global non-entity data
class OVT_<SystemName>Serializer : ScriptedStateSerializer
{
    //! Specify proxy state type
    override static typename GetTargetType()
    {
        return OVT_<SystemName>Data;
    }

    //! Serialize system state
    override protected ESerializeResult Serialize(
        Managed instance,
        BaseSerializationSaveContext context)
    {
        // Access actual system/manager
        OVT_<SystemName>Component system = OVT_Global.Get<SystemName>();
        if (!system)
            return ESerializeResult.DEFAULT;

        context.WriteValue("version", 1);

        // Write system data
        context.Write(system.m_SomeData);

        return ESerializeResult.OK;
    }

    //! Deserialize system state
    override protected bool Deserialize(
        Managed instance,
        BaseSerializationLoadContext context)
    {
        // Access actual system/manager
        OVT_<SystemName>Component system = OVT_Global.Get<SystemName>();
        if (!system)
            return false;

        int version;
        context.Read(version);

        // Read and restore system data
        context.Read(system.m_SomeData);

        return true;
    }
}
```

**System Setup (create and track proxy state):**
```enforcescript
class OVT_<SystemName>Component : ScriptComponent
{
    // ... component code ...

    override void OnPostInit(IEntity owner)
    {
        super.OnPostInit(owner);

        if (Replication.IsServer())
        {
            // Create proxy state for persistence
            OVT_<SystemName>Data proxyState();
            PersistenceSystem.GetInstance().StartTracking(proxyState);
        }
    }
}
```

---

#### B.4. UUID Entity Reference Pattern

**Single Entity Reference:**
```enforcescript
class OVT_MyComponent : ScriptComponent
{
    private IEntity m_ReferencedEntity;

    void SetReferencedEntity(IEntity entity)
    {
        m_ReferencedEntity = entity;
    }
}

class OVT_MyComponentSerializer : ScriptedComponentSerializer
{
    override static typename GetTargetType()
    {
        return OVT_MyComponent;
    }

    override protected ESerializeResult Serialize(/* ... */)
    {
        OVT_MyComponent comp = OVT_MyComponent.Cast(component);

        context.WriteValue("version", 1);

        // Get UUID of referenced entity
        UUID entityId = GetSystem().GetId(comp.m_ReferencedEntity);

        // Write UUID (or NULL if not tracked)
        if (!entityId.IsNull())
            context.Write(entityId);
        else
            context.Write(UUID.NULL_UUID);

        return ESerializeResult.OK;
    }

    override protected bool Deserialize(/* ... */)
    {
        OVT_MyComponent comp = OVT_MyComponent.Cast(component);

        int version;
        context.Read(version);

        UUID entityId;
        context.Read(entityId);

        // If valid UUID, request async resolution
        if (!entityId.IsNull())
        {
            // Create context for callback (captures component reference)
            Tuple1<OVT_MyComponent> ctx(comp);

            // Create async task
            PersistenceWhenAvailableTask task(OnEntityAvailable, ctx);

            // Wait for entity (max 60 seconds)
            GetSystem().WhenAvailable(entityId, task, 60.0);
        }

        return true;
    }

    //! Static callback when entity becomes available
    protected static void OnEntityAvailable(
        Managed instance,
        PersistenceDeferredDeserializeTask task,
        bool expired,
        Managed context)
    {
        // Cast to actual entity type
        IEntity entity = IEntity.Cast(instance);
        if (!entity || expired)
        {
            if (expired)
                Print("[Overthrow] Entity reference timeout", LogLevel.WARNING);
            return; // Entity not found or timeout
        }

        // Retrieve component from context
        Tuple1<OVT_MyComponent> ctx = Tuple1<OVT_MyComponent>.Cast(context);
        if (!ctx || !ctx.param1)
            return;

        // Restore reference
        ctx.param1.SetReferencedEntity(entity);
    }
}
```

**Array of Entity References:**
```enforcescript
class OVT_MyComponent : ScriptComponent
{
    private array<IEntity> m_aEntities = new array<IEntity>();

    void AddEntity(IEntity entity)
    {
        if (!m_aEntities.Contains(entity))
            m_aEntities.Insert(entity);
    }
}

class OVT_MyComponentSerializer : ScriptedComponentSerializer
{
    override protected ESerializeResult Serialize(/* ... */)
    {
        OVT_MyComponent comp = OVT_MyComponent.Cast(component);

        context.WriteValue("version", 1);

        // Convert entity array to UUID array
        array<UUID> entityIds();
        foreach (IEntity entity : comp.m_aEntities)
        {
            UUID id = GetSystem().GetId(entity);
            if (!id.IsNull())
                entityIds.Insert(id);
        }

        // Write UUID array
        context.Write(entityIds);

        return ESerializeResult.OK;
    }

    override protected bool Deserialize(/* ... */)
    {
        OVT_MyComponent comp = OVT_MyComponent.Cast(component);

        int version;
        context.Read(version);

        // Read UUID array
        array<UUID> entityIds();
        context.Read(entityIds);

        // Request async resolution for each UUID
        foreach (UUID id : entityIds)
        {
            if (!id.IsNull())
            {
                // Pass component + UUID in context
                Tuple2<OVT_MyComponent, UUID> ctx(comp, id);
                PersistenceWhenAvailableTask task(OnEntityAvailable, ctx);
                GetSystem().WhenAvailable(id, task, 60.0);
            }
        }

        return true;
    }

    protected static void OnEntityAvailable(
        Managed instance,
        PersistenceDeferredDeserializeTask task,
        bool expired,
        Managed context)
    {
        IEntity entity = IEntity.Cast(instance);
        if (!entity || expired)
            return;

        Tuple2<OVT_MyComponent, UUID> ctx = Tuple2<OVT_MyComponent, UUID>.Cast(context);
        if (ctx && ctx.param1)
            ctx.param1.AddEntity(entity);
    }
}
```

---

#### B.5. OVT_Component.Find<T>() Method

```enforcescript
// Add to existing Scripts/Game/Components/OVT_Component.c
class OVT_Component: ScriptComponent
{
    // ... existing methods (GetRpl, GetGUID, etc.) ...

    //! Find a component on an entity (replacement for EPF_Component<T>.Find)
    //! \param entity The entity to search
    //! \return Component instance or null if not found
    static T Find<Class T>(IEntity entity)
    {
        if (!entity)
            return null;

        return T.Cast(entity.FindComponent(T));
    }
}

// USAGE EXAMPLE:
// OLD EPF:
// DamageManagerComponent dmg = EPF_Component<DamageManagerComponent>.Find(entity);

// NEW Vanilla:
// DamageManagerComponent dmg = OVT_Component.Find<DamageManagerComponent>(entity);
```

---

### Appendix C: Migration Patterns (OLD vs NEW)

#### C.1. Basic Component Serialization

**OLD (EPF):**
```enforcescript
[EPF_ComponentSaveDataType(OVT_ConfigComponent), BaseContainerProps()]
class OVT_ConfigSaveDataClass : EPF_ComponentSaveDataClass {}

[EDF_DbName.Automatic()]
class OVT_ConfigSaveData : EPF_ComponentSaveData
{
    ref OVT_DifficultySettings m_Difficulty;

    override EPF_EReadResult ReadFrom(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
    {
        OVT_ConfigComponent config = OVT_ConfigComponent.Cast(component);
        m_Difficulty = config.m_Difficulty;
        return EPF_EReadResult.OK;
    }

    override EPF_EApplyResult ApplyTo(IEntity owner, GenericComponent component, EPF_ComponentSaveDataClass attributes)
    {
        OVT_ConfigComponent config = OVT_ConfigComponent.Cast(component);
        config.m_Difficulty = m_Difficulty;
        return EPF_EApplyResult.OK;
    }
}
```

**NEW (Vanilla):**
```enforcescript
class OVT_ConfigComponentSerializer : ScriptedComponentSerializer
{
    override static typename GetTargetType()
    {
        return OVT_ConfigComponent;
    }

    override protected ESerializeResult Serialize(
        IEntity owner,
        GenericComponent component,
        BaseSerializationSaveContext context)
    {
        OVT_ConfigComponent config = OVT_ConfigComponent.Cast(component);

        context.WriteValue("version", 1);
        context.Write(config.m_Difficulty);

        return ESerializeResult.OK;
    }

    override protected bool Deserialize(
        IEntity owner,
        GenericComponent component,
        BaseSerializationLoadContext context)
    {
        OVT_ConfigComponent config = OVT_ConfigComponent.Cast(component);

        int version;
        context.Read(version);
        context.Read(config.m_Difficulty);

        return true;
    }
}
```

**Changes:**
- ❌ No separate SaveData class needed
- ❌ No EDF attributes
- ✅ Direct serialization in Serializer class
- ✅ Versioning added
- ✅ Cleaner, less boilerplate

---

#### C.2. Array Serialization

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

**Changes:**
- ❌ No manual count tracking
- ❌ No manual loops
- ✅ Automatic serialization
- ✅ Much simpler

---

#### C.3. Map Serialization

**OLD (EPF):**
```enforcescript
// Save
serializer.Write(m_mData.Count(), 16);
for (int i = 0; i < m_mData.Count(); i++)
{
    string key = m_mData.GetKey(i);
    SomeData value = m_mData.GetElement(i);

    serializer.WriteString(key);
    // ... serialize value ...
}

// Load
int count;
serializer.Read(count, 16);
m_mData.Clear();
for (int i = 0; i < count; i++)
{
    string key;
    serializer.ReadString(key);
    // ... deserialize value ...
    m_mData[key] = value;
}
```

**NEW (Vanilla):**
```enforcescript
// Save
context.Write(m_mData);

// Load
context.Read(m_mData);
```

**Changes:**
- ❌ No manual iteration
- ✅ Automatic map serialization
- ✅ Handles ref values automatically

---

#### C.4. Entity Reference

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

**Changes:**
- ❌ No RplId (network ID)
- ✅ UUID (persistence ID)
- ✅ Async resolution via WhenAvailable
- ✅ More robust (handles load order)
- ⚠️ More complex (static callback pattern)

---

#### C.5. Tracking

**OLD (EPF):**
```enforcescript
#ifndef PLATFORM_CONSOLE
    EPF_PersistenceManager.GetInstance().Register(entity);
#endif
```

**NEW (Vanilla):**
```enforcescript
// No platform guard needed
PersistenceSystem.GetInstance().StartTracking(entity);
```

**Changes:**
- ❌ No platform guards
- ✅ Simpler API
- ✅ Platform handled internally

---

#### C.6. Component Helper

**OLD (EPF):**
```enforcescript
DamageManagerComponent dmg = EPF_Component<DamageManagerComponent>.Find(entity);
```

**NEW (Vanilla):**
```enforcescript
DamageManagerComponent dmg = OVT_Component.Find<DamageManagerComponent>(entity);
```

**Changes:**
- ✅ Drop-in replacement
- ✅ No EPF dependency
- ✅ Same pattern, different helper

---

#### C.7. State Serialization (Non-Entity)

**OLD (EPF):**
```enforcescript
[EPF_PersistentScriptedStateSettings(OVT_LoadoutManagerData)]
class OVT_LoadoutManagerSaveData : EPF_PersistentScriptedState
{
    // Save data fields

    override void OnAfterLoad()
    {
        // Restore to manager
        OVT_LoadoutManagerComponent mgr = OVT_Global.GetLoadouts();
        // ... apply data ...
    }
}
```

**NEW (Vanilla):**
```enforcescript
// 1. Proxy state class
class OVT_LoadoutManagerData : PersistentState {}

// 2. State serializer
class OVT_LoadoutManagerSerializer : ScriptedStateSerializer
{
    override static typename GetTargetType()
    {
        return OVT_LoadoutManagerData;
    }

    override protected ESerializeResult Serialize(/* ... */)
    {
        OVT_LoadoutManagerComponent mgr = OVT_Global.GetLoadouts();
        // ... write data ...
    }

    override protected bool Deserialize(/* ... */)
    {
        OVT_LoadoutManagerComponent mgr = OVT_Global.GetLoadouts();
        // ... read data ...
    }
}

// 3. Track proxy state
OVT_LoadoutManagerData proxyState();
PersistenceSystem.GetInstance().StartTracking(proxyState);
```

**Changes:**
- ✅ Proxy state class pattern
- ✅ Serializer accesses actual manager
- ✅ Cleaner separation

---

## Summary

**Total Estimated Effort:** 30-40 hours over 5-7 days

**Phase Breakdown:**
1. Foundation: 2-3 hours
2. Simple Components: 3-4 hours
3. Manager Components: 6-8 hours
4. Player System: 3-4 hours
5. Entity Serializers: 6-8 hours
6. State Serializers: 2-3 hours
7. EPF Cleanup: 3-4 hours
8. Integration & Testing: 4-6 hours

**Files Affected:**
- **Create:** 22 files (serializers, utilities, templates, docs)
- **Modify:** 25+ files (managers, components, helpers)
- **Delete:** 15 files (all EPF SaveData classes)

**Key Success Factors:**
- Phased approach with incremental testing
- Clear architectural decisions made upfront
- Comprehensive templates and patterns
- Defensive programming with validation
- Extensive manual play-testing

**Risk Mitigation:**
- Breaking change accepted by user
- Async patterns studied from vanilla examples
- Edge cases explicitly tested
- Performance validated
- User tests after each phase

This migration will modernize Overthrow's persistence layer, improve performance, and reduce technical debt while maintaining feature parity.

---

**END OF IMPLEMENTATION PLAN**
