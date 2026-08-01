# Vanilla Persistence Migration - Task Checklist

**Last Updated:** 2025-11-09 02:53
**Progress:** 6/67 tasks complete (9%)

---

## Phase 1: Foundation Setup (6/6 complete) ✅

- [x] ✅ **Update OVT_Component with Find<T>() helper**
  - Description: Add Find<T>(IEntity) static method to OVT_Component base class
  - File(s): `Scripts/Game/Components/OVT_Component.c`
  - Estimate: 🟢 15 min

- [x] ✅ **Create serializer directory structure**
  - Description: Create three serializer directories: Components/, Entities/, States/
  - File(s): `Scripts/Game/Persistence/Serializers/`, `Scripts/Game/Persistence/States/`
  - Estimate: 🟢 10 min

- [x] ✅ **Create component serializer template**
  - Description: Create reference template with inline documentation for component serializers
  - File(s): `Scripts/Game/Persistence/Serializers/Components/_OVT_ComponentSerializerTemplate.c`
  - Estimate: 🟡 30 min

- [x] ✅ **Create entity and state serializer templates**
  - Description: Create reference templates for entity and state serializers
  - File(s): `Scripts/Game/Persistence/Serializers/Entities/_OVT_EntitySerializerTemplate.c`, `Scripts/Game/Persistence/Serializers/States/_OVT_StateSerializerTemplate.c`
  - Estimate: 🟡 30 min

- [x] ✅ **Update OVT_PersistenceManagerComponent**
  - Description: Replace EPF inheritance with vanilla system integration, hook into SCR_PersistenceSystem events
  - File(s): `Scripts/Game/GameMode/Managers/OVT_PersistenceManagerComponent.c`
  - Estimate: 🟡 1 hour

- [x] ✅ **Configure 12 PersistenceCollections**
  - Description: Create all 12 collections (7 Manager, 4 Entity, 1 State) with proper save type masks
  - File(s): `Scripts/Game/GameMode/Managers/OVT_PersistenceManagerComponent.c`
  - Estimate: 🟡 30 min

---

## Phase 2: Simple Component Serializers (0/4 complete)

- [ ] **Create OVT_ConfigManagerComponentSerializer**
  - Description: Implement serializer for config manager (difficulty settings)
  - File(s): `Scripts/Game/Persistence/Serializers/Components/OVT_ConfigManagerComponentSerializer.c`
  - Estimate: 🟡 45 min

- [ ] **Add tracking to OVT_OverthrowConfigComponent**
  - Description: Call StartTracking() in OnPostInit() to enable persistence
  - File(s): `Scripts/Game/Components/OVT_OverthrowConfigComponent.c`
  - Estimate: 🟢 15 min

- [ ] **Create OVT_EconomyManagerComponentSerializer**
  - Description: Implement serializer for economy (resistance money, tax rate)
  - File(s): `Scripts/Game/Persistence/Serializers/Components/OVT_EconomyManagerComponentSerializer.c`
  - Estimate: 🟡 1 hour

- [ ] **Migrate 2-3 files from EPF_Component to OVT_Component**
  - Description: Validate OVT_Component pattern in OVT_Global.c, OVT_ManageBaseAction.c
  - File(s): `Scripts/Game/Global/OVT_Global.c`, `Scripts/Game/UserActions/OVT_ManageBaseAction.c`
  - Estimate: 🟢 30 min

---

## Phase 3: Manager Component Serializers (0/10 complete)

- [ ] **Create OVT_TownManagerComponentSerializer**
  - Description: Implement serializer with custom town matching logic by location
  - File(s): `Scripts/Game/Persistence/Serializers/Components/OVT_TownManagerComponentSerializer.c`
  - Estimate: 🟡 1.5 hours

- [ ] **Add tracking to OVT_TownManagerComponent**
  - Description: Enable persistence tracking for town manager
  - File(s): `Scripts/Game/GameMode/Managers/OVT_TownManagerComponent.c`
  - Estimate: 🟢 10 min

- [ ] **Create OVT_OccupyingFactionManagerSerializer**
  - Description: Implement serializer for occupying faction state
  - File(s): `Scripts/Game/Persistence/Serializers/Components/OVT_OccupyingFactionManagerSerializer.c`
  - Estimate: 🟡 45 min

- [ ] **Add tracking to OVT_OccupyingFactionManager**
  - Description: Enable persistence tracking for occupying faction
  - File(s): `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c`
  - Estimate: 🟢 15 min

- [ ] **Create OVT_ResistanceManagerComponentSerializer**
  - Description: Implement serializer for resistance faction state
  - File(s): `Scripts/Game/Persistence/Serializers/Components/OVT_ResistanceManagerComponentSerializer.c`
  - Estimate: 🟡 45 min

- [ ] **Add tracking to OVT_ResistanceFactionManager**
  - Description: Enable persistence tracking for resistance faction
  - File(s): `Scripts/Game/GameMode/Managers/Factions/OVT_ResistanceFactionManager.c`
  - Estimate: 🟢 15 min

- [ ] **Create OVT_RealEstateManagerComponentSerializer**
  - Description: Implement serializer with maps/arrays (warehouses, owned/rented buildings)
  - File(s): `Scripts/Game/Persistence/Serializers/Components/OVT_RealEstateManagerComponentSerializer.c`
  - Estimate: 🔴 2 hours

- [ ] **Add tracking to OVT_RealEstateManagerComponent**
  - Description: Enable persistence tracking for real estate manager
  - File(s): `Scripts/Game/GameMode/Managers/OVT_RealEstateManagerComponent.c`
  - Estimate: 🟢 10 min

- [ ] **Create OVT_RecruitManagerComponentSerializer**
  - Description: Implement serializer for recruit state (analyze if entity refs needed)
  - File(s): `Scripts/Game/Persistence/Serializers/Components/OVT_RecruitManagerComponentSerializer.c`
  - Estimate: 🟡 1.5 hours

- [ ] **Add tracking to OVT_RecruitManagerComponent**
  - Description: Enable persistence tracking for recruit manager
  - File(s): `Scripts/Game/GameMode/Managers/OVT_RecruitManagerComponent.c`
  - Estimate: 🟢 10 min

---

## Phase 4: Player System (0/2 complete)

- [ ] **Create OVT_PlayerManagerComponentSerializer**
  - Description: Implement complex serializer with validation, player identity, event invokers
  - File(s): `Scripts/Game/Persistence/Serializers/Components/OVT_PlayerManagerComponentSerializer.c`
  - Estimate: 🔴 3 hours

- [ ] **Add tracking and test player reconnection**
  - Description: Enable tracking, test new player join, disconnect/reconnect, multiple players
  - File(s): `Scripts/Game/GameMode/Managers/OVT_PlayerManagerComponent.c`
  - Estimate: 🟡 1 hour

---

## Phase 5: Entity Serializers (0/10 complete)

- [ ] **Create OVT_PlaceableSerializer**
  - Description: Implement ScriptedEntitySerializer with spawn data (prefab + transform)
  - File(s): `Scripts/Game/Persistence/Serializers/Entities/OVT_PlaceableSerializer.c`
  - Estimate: 🔴 2 hours

- [ ] **Add tracking to placeable entities**
  - Description: Configure automatic or manual tracking on placement
  - File(s): Placeable component/entity
  - Estimate: 🟡 30 min

- [ ] **Test placeable spawn/despawn cycle**
  - Description: Verify placeables respawn correctly after save/load
  - File(s): N/A (manual testing)
  - Estimate: 🟢 30 min

- [ ] **Analyze building persistence needs**
  - Description: Determine if buildings are world-placed (state only) or spawned (spawn + state)
  - File(s): Building component/entity
  - Estimate: 🟢 30 min

- [ ] **Create OVT_BuildingSerializer**
  - Description: Implement appropriate serializer based on analysis (component or entity)
  - File(s): `Scripts/Game/Persistence/Serializers/Entities/OVT_BuildingSerializer.c` OR `Scripts/Game/Persistence/Serializers/Components/OVT_BuildingComponentSerializer.c`
  - Estimate: 🟡 1.5 hours

- [ ] **Create OVT_BaseUpgradeSerializer**
  - Description: Implement serializer with UUID-based entity references and WhenAvailable pattern
  - File(s): `Scripts/Game/Persistence/Serializers/Entities/OVT_BaseUpgradeSerializer.c`
  - Estimate: 🔴 2 hours

- [ ] **Test base upgrade persistence**
  - Description: Verify base creation, upgrade, save/load with entity reference resolution
  - File(s): N/A (manual testing)
  - Estimate: 🟢 30 min

- [ ] **Create OVT_CharacterControllerComponentSerializer**
  - Description: Implement serializer for character controller state
  - File(s): `Scripts/Game/Persistence/Serializers/Components/OVT_CharacterControllerComponentSerializer.c`
  - Estimate: 🟡 1 hour

- [ ] **Test character persistence with recruits**
  - Description: Verify recruit characters persist state correctly
  - File(s): N/A (manual testing)
  - Estimate: 🟢 30 min

- [ ] **Create OVT_OverthrowGameModeSerializer**
  - Description: Create placeholder serializer (likely empty unless game mode needs entity-level persistence)
  - File(s): `Scripts/Game/Persistence/Serializers/Entities/OVT_OverthrowGameModeSerializer.c`
  - Estimate: 🟢 30 min

---

## Phase 6: State Serializers (Loadout System) (0/6 complete)

- [ ] **Create OVT_LoadoutManagerData proxy class**
  - Description: Create PersistentState proxy for loadout manager
  - File(s): `Scripts/Game/Persistence/States/OVT_LoadoutManagerData.c`
  - Estimate: 🟢 15 min

- [ ] **Create OVT_LoadoutManagerSerializer**
  - Description: Implement ScriptedStateSerializer for loadout manager data
  - File(s): `Scripts/Game/Persistence/Serializers/States/OVT_LoadoutManagerSerializer.c`
  - Estimate: 🟡 1 hour

- [ ] **Track loadout manager proxy state**
  - Description: Instantiate and track proxy state in loadout manager
  - File(s): `Scripts/Game/GameMode/Managers/OVT_LoadoutManagerComponent.c`
  - Estimate: 🟢 15 min

- [ ] **Create OVT_PlayerLoadoutData proxy class**
  - Description: Create PersistentState proxy for player loadouts
  - File(s): `Scripts/Game/Persistence/States/OVT_PlayerLoadoutData.c`
  - Estimate: 🟢 15 min

- [ ] **Create OVT_PlayerLoadoutSerializer**
  - Description: Implement ScriptedStateSerializer for player-specific loadouts
  - File(s): `Scripts/Game/Persistence/Serializers/States/OVT_PlayerLoadoutSerializer.c`
  - Estimate: 🟡 1 hour

- [ ] **Test loadout persistence**
  - Description: Save loadout, restart, verify loadout available
  - File(s): N/A (manual testing)
  - Estimate: 🟢 30 min

---

## Phase 7: EPF Cleanup (0/8 complete)

- [ ] **Find all EPF_Component usage**
  - Description: Grep codebase for all EPF_Component references
  - File(s): N/A (grep command)
  - Estimate: 🟢 10 min

- [ ] **Migrate UserActions files**
  - Description: Replace EPF_Component with OVT_Component in all user action files
  - File(s): `Scripts/Game/UserActions/*.c`
  - Estimate: 🟡 30 min

- [ ] **Migrate remaining component files**
  - Description: Replace EPF_Component in UI, respawn, player components, controllers
  - File(s): `Scripts/Game/UI/HUD/OVT_EconomyInfo.c`, `Scripts/Game/Respawn/Logic/OVT_SpawnLogic.c`, `Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c`, `Scripts/Game/Controllers/OccupyingFaction/OVT_BaseControllerComponent.c`, etc.
  - Estimate: 🟡 30 min

- [ ] **Delete old EPF SaveData directories**
  - Description: Delete Scripts/Game/GameMode/Persistence/ and Scripts/Game/GameMode/SaveData/ (15 files total)
  - File(s): EPF SaveData directories
  - Estimate: 🟢 15 min

- [ ] **Remove EPF/EDF from mod dependencies**
  - Description: Update addon.gproj and mission headers to remove EPF/EDF
  - File(s): `addon.gproj`, mission headers
  - Estimate: 🟢 15 min

- [ ] **Remove persistence-related platform guards**
  - Description: Search and remove #ifdef PLATFORM_CONSOLE guards for persistence
  - File(s): Various files with platform guards
  - Estimate: 🟢 30 min

- [ ] **Final OVT_PersistenceManagerComponent cleanup**
  - Description: Remove EPF methods, implement vanilla equivalents (HasSaveGame, WipeSave)
  - File(s): `Scripts/Game/GameMode/Managers/OVT_PersistenceManagerComponent.c`
  - Estimate: 🟡 1 hour

- [ ] **Final compile and verification**
  - Description: Full Workbench compile, resolve errors, verify no EPF references
  - File(s): N/A (compilation)
  - Estimate: 🟢 30 min

---

## Phase 8: Integration & Testing (0/21 complete)

### System-by-System Testing (0/7 complete)

- [ ] **Test config system persistence**
  - Description: Modify difficulty, save, restart, verify settings restored
  - File(s): N/A (manual testing)
  - Estimate: 🟢 15 min

- [ ] **Test economy system persistence**
  - Description: Change money/tax, save, restart, verify values
  - File(s): N/A (manual testing)
  - Estimate: 🟢 15 min

- [ ] **Test town system persistence**
  - Description: Capture town, save, restart, verify ownership/stability
  - File(s): N/A (manual testing)
  - Estimate: 🟢 20 min

- [ ] **Test player system persistence**
  - Description: Player join/leave/rejoin, verify money/faction standing, test new players
  - File(s): N/A (manual testing)
  - Estimate: 🟡 30 min

- [ ] **Test real estate system persistence**
  - Description: Purchase building, save, restart, verify ownership
  - File(s): N/A (manual testing)
  - Estimate: 🟢 15 min

- [ ] **Test entity systems persistence**
  - Description: Place objects/build base, save, restart, verify respawn
  - File(s): N/A (manual testing)
  - Estimate: 🟡 30 min

- [ ] **Test loadout system persistence**
  - Description: Save loadout, restart, verify loadout available
  - File(s): N/A (manual testing)
  - Estimate: 🟢 15 min

### Integration Testing (0/3 complete)

- [ ] **Full game session scenario**
  - Description: 30min session with money, towns, bases, objects, loadouts → save/restart → verify all
  - File(s): N/A (manual testing)
  - Estimate: 🟡 1 hour

- [ ] **Multiple players scenario**
  - Description: 2-3 players with independent data, save/restart, verify isolation
  - File(s): N/A (manual testing)
  - Estimate: 🟡 1 hour

- [ ] **Player reconnection scenario**
  - Description: Player A plays, disconnects, Player B joins, Player A rejoins → verify data separation
  - File(s): N/A (manual testing)
  - Estimate: 🟢 30 min

### Edge Case Testing (0/6 complete)

- [ ] **Test missing save file handling**
  - Description: Delete save mid-game, verify graceful handling
  - File(s): N/A (manual testing)
  - Estimate: 🟢 15 min

- [ ] **Test corrupted save file handling**
  - Description: Corrupt save file, verify error handling
  - File(s): N/A (manual testing)
  - Estimate: 🟢 15 min

- [ ] **Test missing entity reference timeout**
  - Description: Delete entity with UUID reference, verify WhenAvailable timeout
  - File(s): N/A (manual testing)
  - Estimate: 🟢 20 min

- [ ] **Test save during high load**
  - Description: Trigger save during intense gameplay, verify stability
  - File(s): N/A (manual testing)
  - Estimate: 🟢 20 min

- [ ] **Test auto-save functionality**
  - Description: Verify auto-save triggers at correct intervals
  - File(s): N/A (manual testing)
  - Estimate: 🟢 15 min

- [ ] **Test shutdown save**
  - Description: Verify save on server shutdown works correctly
  - File(s): N/A (manual testing)
  - Estimate: 🟢 15 min

### Performance & Polish (0/5 complete)

- [ ] **Performance baseline measurement**
  - Description: Measure save/load times for vanilla vs EPF comparison
  - File(s): N/A (performance testing)
  - Estimate: 🟡 30 min

- [ ] **Add debug logging**
  - Description: Add Print statements for debugging persistence issues
  - File(s): All serializers
  - Estimate: 🟡 30 min

- [ ] **Review and clean up code**
  - Description: Remove commented code, clean up formatting
  - File(s): All modified files
  - Estimate: 🟡 30 min

- [ ] **Update user documentation**
  - Description: Document save system for users (breaking change notice)
  - File(s): User docs
  - Estimate: 🟡 30 min

- [ ] **Create migration announcement**
  - Description: Prepare changelog entry and player communication
  - File(s): CHANGELOG, announcement text
  - Estimate: 🟢 20 min

---

## Bugs & Issues

**Active Bugs:**
- None yet

**Fixed Bugs:**
- None yet

---

## Technical Debt

- [ ] 💳 **Migration tool for existing saves** - Priority: Low
  - Description: Tool to convert EPF saves to vanilla (if feasible)
  - Reason: Breaking change accepted, but nice-to-have for early adopters
  - Effort: 🔴 4-6 hours

---

## Documentation Tasks

- [ ] **Add serializer pattern documentation**
  - Description: Document component/entity/state serializer patterns for future devs
  - File(s): dev/docs/
  - Estimate: 🟡 1 hour

- [ ] **Update architecture docs**
  - Description: Update persistence section in architecture docs
  - File(s): docs/features/core/persistence/
  - Estimate: 🟢 30 min

- [ ] **Create troubleshooting guide**
  - Description: Document common persistence issues and solutions
  - File(s): dev/docs/
  - Estimate: 🟢 30 min

---

## Task Status Legend

- [ ] Not started
- [ ] 🔄 In progress
- [ ] ⏸️ Blocked (waiting on something)
- [x] ✅ Completed
- [x] ❌ Cancelled/Won't do

---

## Progress Tracking

### Completed This Session (2025-11-09)
- ✅ Dev docs structure created
- ✅ Implementation plan reviewed

### Discovered New Tasks
- None yet

### Blocked Items
- None yet

---

## Notes

### Task Estimation
- 🟢 Small (< 1 hour)
- 🟡 Medium (1-3 hours)
- 🔴 Large (> 3 hours)

### Priority Guidelines
- **High:** Critical for feature to work
- **Medium:** Important but not blocking
- **Low:** Nice to have, can defer

### Total Effort Estimate
- **Phase 1:** 2-3 hours (6 tasks)
- **Phase 2:** 3-4 hours (4 tasks)
- **Phase 3:** 6-8 hours (10 tasks)
- **Phase 4:** 3-4 hours (2 tasks)
- **Phase 5:** 6-8 hours (10 tasks)
- **Phase 6:** 2-3 hours (6 tasks)
- **Phase 7:** 3-4 hours (8 tasks)
- **Phase 8:** 4-6 hours (21 tasks)
- **Total:** 30-40 hours (67 tasks)

---

*Update this file as tasks are completed. Mark tasks with ✅ immediately when done. Add new tasks as they're discovered.*
