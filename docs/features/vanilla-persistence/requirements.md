# Vanilla Persistence Migration - Requirements

**Feature Name:** vanilla-persistence
**Status:** Requirements Gathering
**Priority:** High - Start ASAP
**Last Updated:** 2025-11-09
**Author:** Planning based on user input and codebase analysis

---

## Executive Summary

Migrate Overthrow mod from EPF (Enfusion Persistence Framework) to Arma Reforger's new vanilla persistence system. This is a comprehensive system-wide change affecting all persistent game state.

---

## Goals

### Primary Goals
1. **Complete EPF Removal** - Replace all EPF dependencies with vanilla persistence system
2. **Maintain Feature Parity** - All currently persisted data continues to be saved/loaded
3. **Improve Performance** - Leverage native C++ persistence for better save/load times
4. **Simplify Codebase** - Reduce boilerplate and complexity of persistence code
5. **Enable Console Support** - Remove platform-specific guards that vanilla system handles internally

### Secondary Goals
1. **Modernize Architecture** - Align with current Arma Reforger patterns and best practices
2. **Better Async Handling** - Use `WhenAvailable` pattern for entity references
3. **Improve Maintainability** - Cleaner serialization code with less custom logic
4. **Documentation** - Create comprehensive examples for future Overthrow development

---

## User Requirements

### User Input Summary
- **Priority:** High - Start ASAP
- **Backward Compatibility:** No - Fresh start acceptable (breaking change)
- **Migration Approach:** Big bang - All at once
- **Scope:** All persistence systems

### Interpretation
- This is a **breaking change** - existing save files will NOT be migrated
- All systems using EPF will be migrated simultaneously
- No need to maintain dual EPF/vanilla persistence during transition
- Players/servers will need to start fresh saves after this update
- High priority indicates this should be primary focus once approved

---

## Scope

### In Scope - Component SaveData Classes (8 files)

These EPF component SaveData classes will be converted to vanilla Serializers:

1. **OVT_TownSaveData** (`Scripts/Game/GameMode/Persistence/Components/OVT_TownSaveData.c`)
   - Persists array of town data (control, stability, population, etc.)
   - Used by: `OVT_TownManagerComponent`

2. **OVT_PlayerSaveData** (`Scripts/Game/GameMode/Persistence/Components/OVT_PlayerSaveData.c`)
   - Persists map of player data by persistent ID
   - Includes: money, faction standing, loadouts, progression
   - Used by: `OVT_PlayerManagerComponent`

3. **OVT_EconomySaveData** (`Scripts/Game/GameMode/Persistence/Components/OVT_EconomySaveData.c`)
   - Persists resistance money and tax rate
   - Used by: `OVT_EconomyManagerComponent`

4. **OVT_OccupyingFactionSaveData** (`Scripts/Game/GameMode/Persistence/Components/OVT_OccupyingFactionSaveData.c`)
   - Persists occupying faction state
   - Used by: `OVT_OccupyingFactionManager`

5. **OVT_RealEstateSaveData** (`Scripts/Game/GameMode/Persistence/Components/OVT_RealEstateSaveData.c`)
   - Persists owned properties and buildings
   - Used by: `OVT_RealEstateManagerComponent`

6. **OVT_RecruitSaveData** (`Scripts/Game/GameMode/Persistence/Components/OVT_RecruitSaveData.c`)
   - Persists recruited NPC/AI state
   - Used by: `OVT_RecruitManagerComponent`

7. **OVT_ResistanceSaveData** (`Scripts/Game/GameMode/Persistence/Components/OVT_ResistanceSaveData.c`)
   - Persists resistance faction state
   - Used by: `OVT_ResistanceManagerComponent`

8. **OVT_ConfigSaveData** (`Scripts/Game/GameMode/Persistence/Components/OVT_ConfigSaveData.c`)
   - Persists mod configuration settings
   - Used by: `OVT_ConfigManagerComponent`

### In Scope - Entity SaveData Classes (5 files)

These EPF entity SaveData classes will be converted:

1. **OVT_OverthrowSaveData** (`Scripts/Game/GameMode/Persistence/OVT_OverthrowSaveData.c`)
   - Currently empty placeholder - may need entity-level persistence
   - Used by: Game mode entity

2. **OVT_PlaceableSaveData** (`Scripts/Game/GameMode/Persistence/OVT_PlaceableSaveData.c`)
   - Persists player-placed objects and structures
   - Entity spawn data and placement info

3. **OVT_BuildingSaveData** (`Scripts/Game/GameMode/Persistence/OVT_BuildingSaveData.c`)
   - Persists building state (damage, ownership, etc.)

4. **OVT_BaseUpgradeSaveData** (`Scripts/Game/GameMode/Persistence/OVT_BaseUpgradeSaveData.c`)
   - Persists base upgrade progress and state

5. **OVT_CharacterControllerComponentSaveData** (`Scripts/Game/Entities/Character/OVT_CharacterControllerComponentSaveData.c`)
   - Persists character-specific state

### In Scope - Loadout SaveData (2 files)

1. **OVT_LoadoutManagerSaveData** (`Scripts/Game/GameMode/SaveData/OVT_LoadoutManagerSaveData.c`)
   - Uses `EPF_PersistentScriptedState` (non-component state)
   - Persists saved loadout templates

2. **OVT_PlayerLoadoutSaveData** (`Scripts/Game/GameMode/SaveData/OVT_PlayerLoadoutSaveData.c`)
   - Uses `EPF_PersistentScriptedState`
   - Persists player-specific loadouts

### In Scope - EPF Component Helper Usage

**20+ files** use `EPF_Component<T>.Find()` helper pattern:
- All UserAction classes
- UI components
- Various manager/controller components

**Migration:** Replace with vanilla component finding pattern or custom helper.

### In Scope - EPF Dependencies

**Complete removal of:**
- EPF (Enfusion Persistence Framework) mod dependency
- EDF (Enfusion Database Framework) mod dependency
- All `#ifdef PLATFORM_CONSOLE` guards related to EPF
- All EPF imports and using statements

### Out of Scope

**NOT migrating (out of scope for this feature):**
- Network replication (RplProp/RPC) - separate concern
- UI/HUD elements - not persistence-related
- Game logic changes - only persistence layer affected
- New features - this is migration only, feature parity maintained

---

## Technical Requirements

### TR-1: Serializer Architecture

**Requirement:** Create vanilla Serializers for all EPF SaveData classes.

**Details:**
- Replace `EPF_ComponentSaveDataClass` with `ScriptedComponentSerializer`
- Replace `EPF_EntitySaveDataClass` with `ScriptedEntitySerializer`
- Replace `EPF_PersistentScriptedState` with `ScriptedStateSerializer` + proxy `PersistentState` class
- All serializers must implement:
  - `GetTargetType()` static event
  - `Serialize()` method
  - `Deserialize()` method
  - Versioning via `context.WriteValue("version", N)`

**Acceptance Criteria:**
- All 15 SaveData files converted to Serializers
- Compile with no errors in Workbench
- All serializers follow vanilla patterns (see research.md examples)

### TR-2: Entity Reference Migration

**Requirement:** Convert all entity references from RplId to UUID with async resolution.

**Details:**
- Replace `RplId` persistence with `UUID`
- Use `PersistenceSystem.GetId(entity)` for save
- Use `WhenAvailable` pattern for load
- Implement static callbacks with Tuple context passing
- Handle timeouts (60s default for player entities)

**Acceptance Criteria:**
- No more `Replication.FindId()` calls in persistence code
- All entity references use `WhenAvailable` pattern
- Callbacks handle `expired` parameter properly

### TR-3: Collection Management

**Requirement:** Define and configure PersistenceCollection(s) for Overthrow data.

**Details:**
- Create at least one `PersistenceCollection` (e.g., "Overthrow")
- Configure which SaveTypes each collection supports (MANUAL/AUTO/SHUTDOWN)
- Organize related data into logical collections

**Acceptance Criteria:**
- Collections defined in configuration
- All serializers assigned to appropriate collection
- Collections accessible via `PersistenceSystem.FindCollection()`

### TR-4: Tracking Lifecycle

**Requirement:** Properly manage tracking lifecycle for all persistent entities/components.

**Details:**
- Replace `EPF_PersistenceManager.Register()` with `PersistenceSystem.StartTracking()`
- Replace `EPF_PersistenceManager.Unregister()` with `PersistenceSystem.StopTracking()`
- Use `SetId()` for player identity persistence
- Use `FindById()` for player reconnection

**Acceptance Criteria:**
- All manager components tracked on server
- Player data properly keyed by persistent identity
- Entities spawn/despawn tracked correctly

### TR-5: Array/Map Serialization

**Requirement:** Simplify array/map serialization using native context.Write/Read.

**Details:**
- Remove manual loops for array serialization
- Remove count tracking for collections
- Use `context.Write(array)` and `context.Read(array)` directly
- Leverage native support for `ref` arrays and maps

**Acceptance Criteria:**
- No more manual count writes
- No more manual foreach loops for serialization
- Code is simpler and more readable

### TR-6: Platform Guard Removal

**Requirement:** Remove all `#ifdef PLATFORM_CONSOLE` guards related to persistence.

**Details:**
- Delete all `#ifndef PLATFORM_CONSOLE` blocks wrapping EPF calls
- Rely on vanilla system's internal platform handling
- Check `OnAfterSave()` success parameter for failures instead

**Acceptance Criteria:**
- Zero `#ifdef PLATFORM_CONSOLE` guards in persistence code
- System works on all platforms without guards

### TR-7: EPF Component Helper Replacement

**Requirement:** Replace `EPF_Component<T>.Find()` usage with vanilla pattern.

**Details:**
- Option A: Create `OVT_Component<T>.Find()` helper mimicking EPF pattern
- Option B: Use direct `GetOwner().FindComponent(T)` calls
- Update all 20+ files using the helper

**Acceptance Criteria:**
- No more EPF_Component imports
- All component finding works identically
- Compile with no errors

### TR-8: Save/Load Triggers

**Requirement:** Implement save/load triggers using vanilla system.

**Details:**
- Use `PersistenceSystem.TriggerSave(ESaveGameType)` for manual saves
- Hook into `SCR_PersistenceSystem.GetOnBeforeSave()` for pre-save logic
- Hook into `SCR_PersistenceSystem.GetOnAfterSave()` for post-save feedback
- Configure auto-save intervals via mission header settings

**Acceptance Criteria:**
- Manual saves work via admin commands/UI
- Auto-saves trigger at configured intervals
- Shutdown saves happen on server stop
- UI provides feedback on save success/failure

### TR-9: Versioning Strategy

**Requirement:** All serializers must support forward versioning.

**Details:**
- Use `context.WriteValue("version", N)` in all Serialize methods
- Start at version 1 for initial migration
- Read version and handle missing fields gracefully
- Document version changes in comments

**Acceptance Criteria:**
- All serializers write version number
- All serializers read version and handle gracefully
- Future version changes won't break old saves (within reason)

### TR-10: Testing Plan

**Requirement:** Define comprehensive test plan for migration validation.

**Details:**
- Test save/load for each major system (towns, players, economy, etc.)
- Test entity reference resolution (bases, placeables, vehicles)
- Test player reconnection with persistent ID
- Test server restart with save/load
- Test multiple players with concurrent data
- Test edge cases (missing entities, corrupted data)

**Acceptance Criteria:**
- Documented test scenarios
- Each system validated through play-testing
- Edge cases identified and handled

---

## Data Model

### Current EPF Architecture

```
OVT_OverthrowGameMode (Entity)
├── Components (Manager Singletons)
│   ├── OVT_TownManagerComponent
│   │   └── OVT_TownSaveData (EPF_ComponentSaveData)
│   ├── OVT_PlayerManagerComponent
│   │   └── OVT_PlayerSaveData
│   ├── OVT_EconomyManagerComponent
│   │   └── OVT_EconomySaveData
│   ├── OVT_OccupyingFactionManager
│   │   └── OVT_OccupyingFactionSaveData
│   ├── OVT_RealEstateManagerComponent
│   │   └── OVT_RealEstateSaveData
│   ├── OVT_RecruitManagerComponent
│   │   └── OVT_RecruitSaveData
│   ├── OVT_ResistanceManagerComponent
│   │   └── OVT_ResistanceSaveData
│   └── OVT_ConfigManagerComponent
│       └── OVT_ConfigSaveData
│
├── Non-Component States
│   ├── OVT_LoadoutManagerSaveData (EPF_PersistentScriptedState)
│   └── OVT_PlayerLoadoutSaveData
│
└── World Entities
    ├── Placeables (OVT_PlaceableSaveData)
    ├── Buildings (OVT_BuildingSaveData)
    ├── Bases (OVT_BaseUpgradeSaveData)
    └── Characters (OVT_CharacterControllerComponentSaveData)
```

### Target Vanilla Architecture

```
OVT_OverthrowGameMode (Entity)
├── Components (Manager Singletons)
│   ├── OVT_TownManagerComponent
│   │   └── OVT_TownManagerComponentSerializer (ScriptedComponentSerializer)
│   ├── OVT_PlayerManagerComponent
│   │   └── OVT_PlayerManagerComponentSerializer
│   ├── OVT_EconomyManagerComponent
│   │   └── OVT_EconomyManagerComponentSerializer
│   ├── OVT_OccupyingFactionManager
│   │   └── OVT_OccupyingFactionManagerSerializer
│   ├── OVT_RealEstateManagerComponent
│   │   └── OVT_RealEstateManagerComponentSerializer
│   ├── OVT_RecruitManagerComponent
│   │   └── OVT_RecruitManagerComponentSerializer
│   ├── OVT_ResistanceManagerComponent
│   │   └── OVT_ResistanceManagerComponentSerializer
│   └── OVT_ConfigManagerComponent
│       └── OVT_ConfigManagerComponentSerializer
│
├── Non-Component States (via proxy classes)
│   ├── OVT_LoadoutManagerData : PersistentState
│   │   └── OVT_LoadoutManagerSerializer (ScriptedStateSerializer)
│   └── OVT_PlayerLoadoutData : PersistentState
│       └── OVT_PlayerLoadoutSerializer
│
└── World Entities
    ├── Placeables → OVT_PlaceableSerializer (ScriptedEntitySerializer)
    ├── Buildings → OVT_BuildingSerializer
    ├── Bases → OVT_BaseUpgradeSerializer
    └── Characters → OVT_CharacterControllerComponentSerializer
```

---

## Dependencies

### Remove (EPF/EDF)
- **EPF (Enfusion Persistence Framework)** - Completely removed
- **EDF (Enfusion Database Framework)** - EPF dependency, removed

### Add (None - Vanilla System Built-in)
- Vanilla persistence is part of Arma Reforger - no external dependencies

### Internal Overthrow Dependencies
- All manager components depend on persistence
- Player systems depend on persistent identity
- Base/town systems depend on entity reference persistence
- Economy depends on persistent state

**Migration Impact:** This is a **foundational change** affecting nearly all Overthrow systems.

---

## File Organization

### New Directory Structure

```
Scripts/Game/
├── Persistence/                         # NEW directory
│   ├── Serializers/                     # All serializers
│   │   ├── Components/                  # Component serializers
│   │   │   ├── OVT_TownManagerComponentSerializer.c
│   │   │   ├── OVT_PlayerManagerComponentSerializer.c
│   │   │   ├── OVT_EconomyManagerComponentSerializer.c
│   │   │   ├── OVT_OccupyingFactionManagerSerializer.c
│   │   │   ├── OVT_RealEstateManagerComponentSerializer.c
│   │   │   ├── OVT_RecruitManagerComponentSerializer.c
│   │   │   ├── OVT_ResistanceManagerComponentSerializer.c
│   │   │   └── OVT_ConfigManagerComponentSerializer.c
│   │   │
│   │   ├── Entities/                    # Entity serializers
│   │   │   ├── OVT_PlaceableSerializer.c
│   │   │   ├── OVT_BuildingSerializer.c
│   │   │   ├── OVT_BaseUpgradeSerializer.c
│   │   │   └── OVT_CharacterControllerComponentSerializer.c
│   │   │
│   │   └── States/                      # State serializers (non-entity)
│   │       ├── OVT_LoadoutManagerSerializer.c
│   │       └── OVT_PlayerLoadoutSerializer.c
│   │
│   └── States/                          # Proxy state classes
│       ├── OVT_LoadoutManagerData.c
│       └── OVT_PlayerLoadoutData.c
│
└── GameMode/
    ├── Persistence/                     # DELETE entire directory
    └── SaveData/                        # DELETE entire directory
```

### Migration Changes
- **DELETE:** `Scripts/Game/GameMode/Persistence/` (all EPF SaveData files)
- **DELETE:** `Scripts/Game/GameMode/SaveData/` (EPF state SaveData)
- **CREATE:** `Scripts/Game/Persistence/` (new vanilla serializers)
- **UPDATE:** All 20+ files using `EPF_Component<T>.Find()`

---

## Risks & Mitigation

### Risk 1: Breaking Change for Players
**Risk:** All existing save files become invalid.

**Impact:** High - Players lose progress, servers must wipe.

**Mitigation:**
- Clearly communicate breaking change in release notes
- Provide advance warning before update
- Time release appropriately (avoid mid-campaign)
- Consider this a "fresh start" for major version

**Status:** Accepted - User confirmed fresh start is acceptable.

### Risk 2: Complex Entity References
**Risk:** Async entity resolution via `WhenAvailable` is more complex than EPF's synchronous pattern.

**Impact:** Medium - Potential for race conditions, timing bugs.

**Mitigation:**
- Study vanilla examples thoroughly (see research.md)
- Use proven Tuple context pattern
- Test entity reference resolution extensively
- Handle timeout cases gracefully
- Start with simple references, then complex ones

**Status:** Manageable with careful implementation.

### Risk 3: Big Bang Migration Risk
**Risk:** Migrating all systems at once increases risk of bugs and testing complexity.

**Impact:** High - One bug could break entire persistence.

**Mitigation:**
- Implement in logical phases (see implementation plan)
- Test each phase thoroughly before moving to next
- Keep EPF code temporarily for reference
- Use version control branches for rollback
- Extensive play-testing after each phase

**Status:** Mitigated by phased implementation within big bang approach.

### Risk 4: Missing Edge Cases
**Risk:** EPF may handle edge cases that vanilla system doesn't (or handles differently).

**Impact:** Medium - Data loss or corruption in edge cases.

**Mitigation:**
- Review all EPF SaveData classes for special handling
- Test edge cases explicitly (null data, empty collections, missing entities)
- Add defensive checks in deserializers
- Log warnings for unexpected data states

**Status:** Mitigated by thorough code review and testing.

### Risk 5: Performance Regression
**Risk:** Vanilla system might perform differently than EPF in practice.

**Impact:** Low to Medium - Potential save/load time changes.

**Mitigation:**
- Native C++ should be faster than EPF's script-based approach
- Monitor save/load times during testing
- Use `WriteDefault()` to skip unchanged data
- Profile if issues arise

**Status:** Low risk - vanilla should be faster.

### Risk 6: Incomplete Documentation
**Risk:** Vanilla system is newer, may have less community documentation than EPF.

**Impact:** Low - Slower development due to research needs.

**Mitigation:**
- research.md provides comprehensive documentation
- Vanilla examples in Arma Reforger source code
- Can reference EPF author's design intent

**Status:** Mitigated by thorough research phase.

### Risk 7: Network Synchronization Confusion
**Risk:** Persistence (UUID) vs replication (RplId) confusion.

**Impact:** Medium - Bugs from mixing persistence and networking concerns.

**Mitigation:**
- Clear documentation on persistence vs replication
- Use UUID only for persistence
- Use RplId only for runtime network references
- Code reviews to catch mixing

**Status:** Mitigated by clear separation of concerns.

---

## Constraints

### Technical Constraints
1. **No Ternary Operators** - EnforceScript limitation
2. **Strong Refs Required** - Use `ref` keyword for Managed classes in arrays/maps
3. **No Automated Builds** - User compiles in Workbench
4. **No Unit Tests** - Manual play-testing only
5. **Server Authority** - Only server persists, clients receive state via replication

### Project Constraints
1. **Breaking Change Acceptable** - No backward compatibility required
2. **Big Bang Approach** - All systems migrate together
3. **High Priority** - Start ASAP, primary focus
4. **Workbench Workflow** - User reports compile errors, test results

### Timeline Constraints
1. **No Hard Deadline** - But high priority suggests urgency
2. **User Availability** - User must compile and test in Workbench
3. **Testing Dependent** - Manual play-testing required for validation

---

## Success Criteria

### Functional Success
- [ ] All 15 SaveData classes converted to Serializers
- [ ] All EPF dependencies removed from mod
- [ ] Save/load works for all major systems (towns, players, economy, bases)
- [ ] Entity references resolve correctly
- [ ] Player reconnection works with persistent identity
- [ ] Server restart preserves game state
- [ ] Multiple players can save/load concurrently

### Technical Success
- [ ] Zero compile errors in Workbench
- [ ] Zero runtime errors during save/load
- [ ] Code follows vanilla persistence patterns (research.md)
- [ ] All entity references use UUID + WhenAvailable
- [ ] All serializers support versioning
- [ ] No platform-specific guards needed

### Code Quality Success
- [ ] Code is simpler than EPF version (less boilerplate)
- [ ] Serializers follow consistent pattern
- [ ] Comments explain non-obvious logic
- [ ] Examples documented for future development
- [ ] Follows Overthrow coding conventions (OVT_ prefix, m_ members, etc.)

### Testing Success
- [ ] All test scenarios pass (see TR-10)
- [ ] Edge cases handled gracefully
- [ ] Performance is acceptable (save/load times reasonable)
- [ ] No data loss or corruption
- [ ] User-reported bugs addressed

---

## Open Questions

### Q1: PersistenceCollection Organization
**Question:** Should Overthrow use a single collection or multiple?

**Options:**
- Single "Overthrow" collection for all data
- Multiple collections: "OverthrowWorld", "OverthrowPlayers", "OverthrowEconomy"

**Recommendation:** Start with single collection, split later if needed for performance.

**Status:** To be decided during implementation planning.

### Q2: EPF_Component Helper Replacement
**Question:** Should we create OVT_Component helper or use direct calls?

**Options:**
- Create `OVT_Component<T>.Find()` helper (easier migration, one-to-one replacement)
- Use direct `entity.FindComponent(T)` calls (more explicit, less magic)

**Recommendation:** Create helper for easier migration, less code churn.

**Status:** To be decided during implementation planning.

### Q3: Serializer Naming Convention
**Question:** Should serializers be named after component or SaveData?

**Options:**
- `OVT_TownManagerComponentSerializer` (matches component name)
- `OVT_TownSerializer` (shorter, matches data domain)

**Recommendation:** Match component name for clarity (e.g., `OVT_TownManagerComponentSerializer`).

**Status:** To be decided during implementation planning.

### Q4: Loadout System Architecture
**Question:** Should loadouts use StateSerializer or ComponentSerializer?

**Current:** Uses `EPF_PersistentScriptedState` (non-component state)

**Options:**
- Migrate to `ScriptedStateSerializer` with proxy state classes (maintains current architecture)
- Refactor to `ScriptedComponentSerializer` on a manager component (more standard pattern)

**Recommendation:** Use StateSerializer to minimize refactoring (closer to EPF pattern).

**Status:** To be decided during implementation planning.

### Q5: Migration Phases
**Question:** What order should systems be migrated in?

**Recommendation:** See implementation plan (to be created).

**Status:** To be decided by solution-architect agent.

---

## Assumptions

1. **Vanilla System Stable** - Arma Reforger's vanilla persistence is production-ready
2. **EPF Author Expertise** - Same author designed both, patterns should translate well
3. **User Testing Availability** - User can compile and test in Workbench throughout migration
4. **No Hidden EPF Dependencies** - All EPF usage is in obvious SaveData classes
5. **Network Layer Separate** - Persistence migration won't affect RplProp/RPC networking
6. **Configuration Via Code** - PersistenceConfig can be set via script (not just prefab attributes)
7. **WhenAvailable Reliable** - Async entity resolution works as documented
8. **Performance Acceptable** - Native persistence won't introduce performance regressions

---

## Next Steps

1. **Review Requirements** - User reviews and approves this document
2. **Create Implementation Plan** - solution-architect agent creates phased implementation plan
3. **User Approves Plan** - User reviews and approves implementation approach
4. **Begin Implementation** - Start migration in planned phases
5. **Iterative Testing** - User compiles and tests after each phase
6. **Final Validation** - Comprehensive testing of all systems
7. **Release** - Deploy with breaking change announcement

---

## Appendix A: Current EPF File Inventory

**Component SaveData (8 files):**
1. `Scripts/Game/GameMode/Persistence/Components/OVT_TownSaveData.c`
2. `Scripts/Game/GameMode/Persistence/Components/OVT_PlayerSaveData.c`
3. `Scripts/Game/GameMode/Persistence/Components/OVT_EconomySaveData.c`
4. `Scripts/Game/GameMode/Persistence/Components/OVT_OccupyingFactionSaveData.c`
5. `Scripts/Game/GameMode/Persistence/Components/OVT_RealEstateSaveData.c`
6. `Scripts/Game/GameMode/Persistence/Components/OVT_RecruitSaveData.c`
7. `Scripts/Game/GameMode/Persistence/Components/OVT_ResistanceSaveData.c`
8. `Scripts/Game/GameMode/Persistence/Components/OVT_ConfigSaveData.c`

**Entity SaveData (5 files):**
1. `Scripts/Game/GameMode/Persistence/OVT_OverthrowSaveData.c`
2. `Scripts/Game/GameMode/Persistence/OVT_PlaceableSaveData.c`
3. `Scripts/Game/GameMode/Persistence/OVT_BuildingSaveData.c`
4. `Scripts/Game/GameMode/Persistence/OVT_BaseUpgradeSaveData.c`
5. `Scripts/Game/Entities/Character/OVT_CharacterControllerComponentSaveData.c`

**State SaveData (2 files):**
1. `Scripts/Game/GameMode/SaveData/OVT_LoadoutManagerSaveData.c`
2. `Scripts/Game/GameMode/SaveData/OVT_PlayerLoadoutSaveData.c`

**Total: 15 SaveData files to migrate**

**EPF_Component Usage (20+ files):**
- All files in `Scripts/Game/UserActions/` directory
- `Scripts/Game/UI/HUD/OVT_EconomyInfo.c`
- `Scripts/Game/Respawn/Logic/OVT_SpawnLogic.c`
- `Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c`
- `Scripts/Game/Controllers/OccupyingFaction/OVT_BaseControllerComponent.c`
- `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c`
- And potentially more...

---

## Appendix B: Reference Documents

1. **Research Document:** `docs/features/vanilla-persistence/research.md`
   - Comprehensive vanilla system documentation
   - Code examples and patterns
   - Migration comparison table

2. **Arma Reforger Source Examples:**
   - Component Serializers: `ArmaReforger/Scripts/Game/Systems/Persistence/Serializers/Components/`
   - Entity Serializers: `ArmaReforger/Scripts/Game/Systems/Persistence/Serializers/Entities/`
   - State Serializers: `ArmaReforger/Scripts/Game/Systems/Persistence/Serializers/States/`

3. **Project Documentation:**
   - `CLAUDE.md` - Project overview and patterns
   - Skills: `enforcescript-patterns`, `overthrow-architecture`

---

**Requirements Status:** Ready for Review
**Next Action:** User approval to proceed to implementation planning
