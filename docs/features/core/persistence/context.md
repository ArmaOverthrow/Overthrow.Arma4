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

**This feature now has a machine-checkable definition of done** (delivered 2026-08-02 by `dev-ops/test-coverage` — see the session note below):

```bash
.scripts/reset_save.sh --profile OverthrowCI        # REQUIRED precondition — never without --profile
tools/run-tests.sh OVT_TEST_PersistenceRoundTripSuite
```

- **Exit 1 today** (9 of 9 cases), with the diagnostic `Persistence capability absent: SaveGame() produced no save (HasSaveGame() still false). The vanilla-persistence migration is not complete.`
- **Exit 0 = this migration is complete.** That is the acceptance criterion; nothing else needs to be agreed.
- The suite asserts only through Overthrow's **public manager API** (money, skills/XP, real-estate ownership, recruits, town control/support/population/stability) — no EPF type, no vanilla persistence type, no `SaveData` class appears in any assertion, so it survives the migration unchanged and cannot report it as a regression by construction.
- On green: delete the quarantine header, add the suite to the All group config, and update `tools/README.md` + this file.

**Blockers:**
- ⚠️ **Phase 1 work never compiled, and was written against an API that does not exist in Reforger 1.7.0.54** — see the 2026-08-01 session note below. The foundation must be re-done against the real vanilla persistence API before Phase 2 serializers can proceed.
- ⚠️ **This branch currently has NO working save path in *either* system** — measured, not inferred (2026-08-02 note below). Nothing saves, so nothing can be verified by restart testing either.

---

## Key Files

### Core Implementation
- `Scripts/Game/Persistence/Serializers/Components/` - Component serializers (to be created)
- `Scripts/Game/Persistence/Serializers/Entities/` - Entity serializers (to be created)
- `Scripts/Game/Persistence/Serializers/States/` - State serializers (to be created)
- `Scripts/Game/Components/OVT_Component.c` - Base component with Find<T>() helper
- `Scripts/Game/GameMode/Managers/OVT_PersistenceManagerComponent.c` - Main persistence manager

### Related Files
- `docs/features/core/persistence/prd.md` - Product requirements
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

### 2026-08-02 — Acceptance gate + branch findings from dev-ops/test-coverage (feature still paused; recorded for resume)

`dev-ops/test-coverage` wrote this migration's acceptance gate and, in doing so, measured two things about the branch that this feature needs on resume.

**1. The gate itself** — `Scripts/Game/Tests/TestSuites/Persistence/OVT_TEST_PersistenceRoundTripSuite.c`, 9 cases, quarantined (in **no** group config, never part of a default or CI run), red on purpose. Command, precondition and exit-code criterion are in Quick Status above; the full contract is `tools/README.md` → "Persistence acceptance gate". Its companion `OVT_TEST_PersistenceSuite` covers the same eight state kinds **within a single session** and is green today, so a regression in "writing state through the manager sticks" is already caught independently of saving.

Two anti-vacuous-pass closures are load-bearing and must not be weakened when the gate is being made green: the capability case asserts the whole **transition** (no save before → a save after — which is why the `reset_save.sh` precondition is mandatory), and every state-kind case **dirties** the value between saving and reloading, so a reload that restores nothing cannot pass. The second does not consult `HasSaveGame()` at all, so it survives a save layer that merely claims to have saved. Renaming the suite breaks the documented gate.

**2. There is no working save path on this branch, in either system.** Verified by execution, not by reading:

- `SaveGame()` and `AutoSave()` return **completely silently** — `m_PersistenceSystem` is null, so even the `TODO(vanilla-persistence)` warning is guarded away. `HasSaveGame()` is hardcoded `false`, `WipeSave()` is a no-op, and a `SaveGame()` call writes **zero bytes** anywhere under the profile directory.
- EPF is not a fallback: re-parenting `OVT_PersistenceManagerComponentClass` from `EPF_PersistenceManagerComponentClass` to `ScriptComponentClass` means `EPF_PersistenceManagerComponent.OnPostInit()` never runs, so EPF never reaches its SETUP state — no DB connection, no autosave tick, no world-load restore. The `SaveData` classes and 59 prefabs' `EPF_PersistenceComponent`s survive, driving nothing.
- Consequence worth fixing early: **a player pressing Save today is told it worked.** `OVT_MainMenuContext` shows `#OVT-Saved` unconditionally (`OVT_MainMenuContext.c:262-271`) and nothing anywhere logs that it did not save. Logged as the highest-severity item in `docs/features/dev-ops/test-coverage/findings.md` → "Bugs found (log only)"; deliberately not fixed there.

**3. The `.scripts/` save tools assume EPF's layout and will need updating when storage moves.** All three (`reset_save.sh`, `backup_save.sh`, `activate_save.sh`) are written against `<My Games>/<profile>/profile/.db/Overthrow` — EPF's `EDF_FileDbDriverBase` shape, with no named slots. Vanilla's `SaveGameManager` *does* offer slots (`RequestSavePoint`/`GetSaves`/`Load`/`Delete`) and is referenced nowhere in Overthrow, but it **exposes no path to script** (engine-sealed) and `$saves:` is not script-writable — so the replacement location can only be established **empirically**, by inspecting the profile directory after a run that actually saves. When it is known, three `DEFAULT_SAVE_DIR` values and `reset_save.sh`'s path guard (which refuses anything not ending in `.db/Overthrow`) must be updated together, plus the `.saves/` archive shape if the on-disk layout changes. Contract to keep in sync: `tools/README.md` → "Save-state control".

### 2026-08-01 — Compile-reality check from dev-ops/workbench-automation (feature paused; recorded for resume)

The new automated compile check (`dev-ops/workbench-automation`) revealed that this feature's Phase 1 "foundation" **never compiled** on Reforger 1.7.0.54 (engine 190965) and was written against an API that does not exist in the retail build. Minimal user-approved fixes were applied so the project compiles again; the substantive rework belongs to this feature when it resumes:

**What the real vanilla 1.7.0 persistence API looks like** (reference: `/mnt/n/Projects/Arma 4/ArmaReforger/scripts/Game/generated/Plugins/Persistence/` and `.../Game/Plugins/Persistence/System/SCR_PersistenceSystem.c`):
- There is **no `SCR_PersistenceSystem.TriggerSave()`**. Saving is per-entity/state: `PersistenceSystem.Save(notnull Managed entityOrState, ESaveGameType saveType)`.
- There is **no `PersistenceCollection.GetOrCreate()`** — `PersistenceCollection` is `sealed` with a private constructor, "only constructed through the internal system". Collections are **config-driven**, not created from script. Decision 1's 12 collections must be defined in the persistence system config instead.
- There is **no `DB_BASE_DIR`** constant (that was EPF's concept).
- These exist and work: `SCR_PersistenceSystem.GetScriptedInstance()`, `GetOnStateChanged()`, `GetOnBeforeSave()`, `GetOnAfterSave()`, `ESaveGameType`, `EPersistenceSystemState`, `ScriptedEntitySerializer` (base class for serializers).
- `ScriptComponent` has **no `OnGameEnd` engine event** — the shutdown hook must be wired explicitly (e.g. from the game mode).
- EnforceScript does **not support generic methods** (`static T Find<T>(...)` is rejected). Generic **classes** are the legal pattern (`class Foo<Class T>`, as EPF does).

**Changes applied on 2026-08-01 (minimal, to make the tree compile — review on resume):**
1. `Scripts/Game/Components/OVT_Component.c` — the illegal `OVT_Component.Find<T>()` generic method was replaced by a standalone generic class `OVT_ComponentFinder<Class T>` with `static T Find(IEntity entity)`. Call sites change from `OVT_Component.Find<X>(e)` to `OVT_ComponentFinder<X>.Find(e)`.
2. `Scripts/Game/GameMode/Managers/OVT_PersistenceManagerComponent.c` — all fictional API calls stubbed with `TODO(vanilla-persistence)` markers + warning Prints. The real event hooks (`GetOnStateChanged`/`GetOnBeforeSave`/`GetOnAfterSave`) are kept live. `OnGameEnd()` is now a plain method (no `override event`) that nothing calls yet. **`HasSaveGame()` currently always returns `false`** and `WipeSave()` is a no-op — the save/continue UI flow will reflect that until reimplemented.
3. The three `_OVT_*Template.c` serializer reference files were **moved out of the compiled tree** (they reference placeholder types like `OVT_MyEntityComponent` and can never compile) to `docs/features/core/persistence/templates/`. Their example calls were updated to the `OVT_ComponentFinder<T>.Find()` pattern.

**Good news for this feature:** the dev-ops epic delivered a working compile check (`-wbsilent -validate`, exit 0/255) and proved the autotest loop end-to-end (`-autotest "{GUID}"` → `junit.xml`). When this feature resumes, every API assumption can be compile-verified in ~4 seconds, and dev-ops/test-coverage will provide behaviour-level persistence round-trip tests as the acceptance gate.

### 2025-11-09 00:00
- Created dev docs structure
- Copied implementation plan to plan.md
- Ready to start Phase 1 implementation
- Next session should focus on OVT_Component helper and directory setup

---

*Update this file at the end of each work session. Run `/dev-docs-update` before compacting conversations.*
