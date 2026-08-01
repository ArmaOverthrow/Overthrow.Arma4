# Vanilla Persistence Migration - Context & Decisions

**Last Updated:** 2025-11-09 02:53
**Current Phase:** Phase 2 - Simple Component Serializers
**Status:** 🟡 In Progress

---

## Quick Status

**What's Done:**
- ✅ Dev docs structure created
- ✅ Implementation plan reviewed and ready
- ✅ Phase 1 Complete - Foundation Setup (6/6 tasks)
  - ✅ OVT_Component.Find<T>() helper added
  - ✅ Serializer directory structure created
  - ✅ Component, Entity, and State serializer templates created
  - ✅ OVT_PersistenceManagerComponent updated for vanilla system
  - ✅ 12 PersistenceCollections configured

**What's Next:**
- 📋 Create OVT_ConfigManagerComponentSerializer
- 📋 Create OVT_EconomyManagerComponentSerializer
- 📋 Validate OVT_Component pattern in 2-3 files

**Blockers:**
- None - Ready to compile and test Phase 1 work

---

## Key Files

### Core Implementation
- `Scripts/Game/Persistence/Serializers/Components/` - Component serializers (to be created)
- `Scripts/Game/Persistence/Serializers/Entities/` - Entity serializers (to be created)
- `Scripts/Game/Persistence/Serializers/States/` - State serializers (to be created)
- `Scripts/Game/Components/OVT_Component.c` - Base component with Find<T>() helper
- `Scripts/Game/GameMode/Managers/OVT_PersistenceManagerComponent.c` - Main persistence manager

### Related Files
- `docs/features/vanilla-persistence/prd.md` - Product requirements
- `dev/active/vanilla-persistence/plan.md` - Implementation plan

---

## Important Decisions

### Decision 1: 12 Separate PersistenceCollections
**Date:** 2025-11-09
**Context:** Need to organize save data for different systems
**Decision:** Use 12 separate collections (7 Manager, 4 Entity, 1 State) instead of a single monolithic collection
**Rationale:** Better organization, granular control, performance flexibility, clearer debugging, future-proof
**Impact:** More setup configuration but much better maintainability

**Collections:**
- **Manager Collections (7):** OverthrowTowns, OverthrowPlayers, OverthrowEconomy, OverthrowFactions, OverthrowRealEstate, OverthrowRecruits, OverthrowConfig
- **Entity Collections (4):** OverthrowPlaceables, OverthrowBuildings, OverthrowBases, OverthrowCharacters
- **State Collections (1):** OverthrowLoadouts

---

### Decision 2: Big Bang Migration Approach
**Date:** 2025-11-09
**Context:** EPF to vanilla migration affects 15 SaveData classes and 20+ files
**Decision:** Migrate all systems together in single branch (vanilla-persistence)
**Rationale:** Backward compatibility not required (breaking change), cleaner than incremental, avoids dual-system complexity
**Impact:** All existing save files will be incompatible, players must start fresh

---

## Gotchas & Learnings

### 1. Entity Reference Resolution
**Problem:** Vanilla persistence uses UUID-based references that resolve asynchronously
**Solution:** Use `WhenAvailable()` pattern for entity reference resolution
**Lesson:** Never assume entity references are immediately available after load

**Example:**
```enforcescript
// ❌ BAD - assumes immediate availability
IEntity entity = GetGame().GetWorld().FindEntityByID(entityId);

// ✅ GOOD - handles async resolution
context.ReadUuid(uuid);
PersistenceIdManager.WhenAvailable(uuid, this, "OnEntityAvailable");
```

---

### 2. No #ifdef PLATFORM_CONSOLE Guards Needed
**Problem:** EPF required platform guards for console compatibility
**Solution:** Vanilla persistence handles platform differences automatically
**Lesson:** Remove all `#ifdef PLATFORM_CONSOLE` guards - vanilla system is cross-platform by design

---

## Testing Approach

### Manual Testing Checklist
- [ ] Test scenario setup in Workbench
- [ ] Manual save trigger works
- [ ] Manual load trigger works
- [ ] Auto-save functionality
- [ ] Data persists correctly across sessions
- [ ] All collections save/load properly
- [ ] Entity references resolve correctly
- [ ] No console errors/warnings

---

## Performance Considerations

- Native C++ serialization significantly faster than EPF's script-based approach
- Automatic array/map serialization reduces overhead
- Per-collection save triggers allow performance tuning
- UUID-based entity references more efficient than EntityID lookups

---

## Next Steps

### Immediate (Today/This Session)
1. Update OVT_Component with Find<T>() helper
2. Create serializer directory structure
3. Create template/example serializers

### Short Term (This Week)
1. Update OVT_PersistenceManagerComponent
2. Configure 12 PersistenceCollections
3. Migrate simple components (Config, Economy)

### Future (After This Phase)
1. Migrate manager components with complex data
2. Migrate entity serializers
3. Complete EPF cleanup and removal

---

## Open Questions

- [ ] **Q:** Should we provide migration tool for existing saves?
      **A:** No - breaking change is acceptable, clean start is better

- [ ] **Q:** Should collections support different save types?
      **A:** All collections support MANUAL | AUTO | SHUTDOWN by default for flexibility

---

## Session Notes

### 2025-11-09 00:00
- Created dev docs structure
- Copied implementation plan to plan.md
- Ready to start Phase 1 implementation
- Next session should focus on OVT_Component helper and directory setup

---

*Update this file at the end of each work session. Run `/dev-docs-update` before compacting conversations.*
