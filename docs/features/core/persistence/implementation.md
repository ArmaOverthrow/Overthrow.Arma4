# Vanilla Persistence Migration - Implementation Plan (v2)

**Feature Name:** persistence (epic: core)
**Status:** Ready for Review — built and gate-verified 2026-08-02 (overnight autorun); awaiting manual play-test
**Priority:** High (dev-ops epic complete; this is the epic's next feature per build order)
**Plan v1:** written 2025-11-09 against a nonexistent API; superseded in full. Do **not** resurrect v1 code samples from git history — every API call in them is unverified or wrong.

---

## Executive Summary

Migrate Overthrow from EPF (Enfusion Persistence Framework) to Reforger's vanilla persistence system. Breaking change; no save migration; big-bang on the `vanilla-persistence` branch.

**The single source of API truth is `docs/features/core/persistence/vanilla-api-reference.md`** (verified against the retail 1.7.0.54 scripts, every claim has file:line). Field-level truth for *what* each system persists is the existing EPF SaveData classes in the repo — read them, don't trust any plan's summary.

**Acceptance criterion (machine-checkable, already exists):**
```bash
.scripts/reset_save.sh --profile OverthrowCI        # REQUIRED precondition
tools/run-tests.sh OVT_TEST_PersistenceRoundTripSuite
```
Exit 1 today → exit 0 = done. The suite asserts only through public manager API; do not rename it or weaken its anti-vacuous-pass closures (see context.md 2026-08-02 note).

### What changed from plan v1 (architecture deltas)

| v1 assumed | 1.7.0 reality |
|---|---|
| `SCR_PersistenceSystem.TriggerSave()` global save | Global save = `GetGame().GetSaveGameManager().RequestSavePoint(ESaveGameType, name, flags, callback)`. `PersistenceSystem.Save()` is per-instance only. |
| 12 collections created from script (`PersistenceCollection.GetOrCreate`) | Collections, storages, serializer bindings, persistent states are **all .conf-defined**. `PersistenceCollection` is sealed; script can only `FindCollection()`. |
| Serializers auto-discovered from `GetTargetType()` | Registration = script class **plus** a `.conf` entry (`EntitySerializer`/`ComponentSerializers{}`/`Serializer`) inside an `EntityPersistenceConfig`/`ScriptedStatePersistenceConfig` matched by a Rule. |
| `StartTracking()` opt-in from each manager's `OnPostInit` | The game-mode entity is tracked once via the native `Persistence` prefab component; component serializers ride along. `StartTracking()` is for script-spawned entities and scripted states. |
| `HasSaveGame()` checks a disk path | `SaveGameManager.GetSaves()` is async-only. Cache the answer (init query + `OnSaveCreated`/`OnSaveDeleted` events). Save location on disk is engine-managed and unknown until observed empirically. |
| `PersistentState` proxies instantiated + tracked from script | States are declared in the conf's `PersistentStates{}` block; engine constructs them. (Script-owned `StartTracking(instance)` exists but conf-declared is the vanilla idiom.) |
| 15 SaveData classes | **~22** exist. v1 missed: Deployments (3 classes), InventoryManager, and embedded `OVT_PlaceableData`/`OVT_PlayerOwnerData`/`OVT_BuildableData`, plus `Scripts/Game/Modded/EPF_PersistenceManager.c`. |
| Nothing about server lifecycle | **Dedicated servers purge the playthrough's saves at game-mode end** unless keep-session-data is configured (`SCR_BaseGameMode.c:723-747`). Must be handled or Overthrow's persistent campaign wipes itself. |
| Characters/vehicles/items each need Overthrow serializers | Vanilla `Common.conf` already persists characters, vehicles, items, doors, AI groups, storages. Inheriting it gives world-state persistence for free. |

### Key wiring facts (verified in this repo)

- `Configs/Systems/ChimeraSystemsConfig.conf` **overrides the vanilla resource by GUID `{86E953538A28A98D}`** — every Overthrow world's SystemSettings chain passes through it. This is where `SCR_PersistenceSystem` gets registered (copy the entry shape from `ArmaReforger/Configs/Systems/MissionSystems.conf:3-7`, point `Config` at our own persistence conf).
- `OVT_Campaign_Test.ent` is a SubScene of vanilla `MpTest/MpTest_Basic.ent` — the test world inherits the same chain, so the autotest world gets the persistence system too.
- `Missions/24_OVT_Eden.conf` is `SCR_MissionHeader` with `m_eSaveTypes` unset → default 15 (all save types enabled). No mission work needed to opt in.
- `Prefabs/GameMode/OVT_OverthrowGameMode.et` has **no vanilla ancestor** — the native `Persistence` component must be added to it by hand (vanilla reference: `GameMode_Base.et:4`).
- `OVT_OverthrowGameMode : SCR_BaseGameMode`, so vanilla's `GameMode.conf` rule (`EntityClassPersistenceConfigRule` on `SCR_BaseGameMode`) already matches our game mode — our conf should **inherit** `$CFG/Configuration/GameMode/GameMode.conf` and append `ComponentSerializers` (the `Tutorial.conf:3-9` pattern).
- Parent-conf GUIDs are discoverable from the vanilla conf headers themselves (each derived conf's first line contains `"{GUID}path"` of its parent).

---

## Target Architecture

### Config resources (new files)

1. **`Configs/Systems/Persistence/Overthrow.conf`** — `PersistenceSystemConfig` inheriting vanilla `Common.conf` (gets the 10 gameplay collections, WorldState bundle, Garbage/Door/Reconnect states, and all standard entity configs for free). Adds, via `+{}` merge:
   - Collection `Overthrow` on the vanilla session storage `{62488048AF9D8A55}` (managers + mod states; more collections only if a real need appears — placeables may warrant `OverthrowPlaceables` for `RequestSpawn` filtering).
   - `Configurations` entries for the Overthrow config files below.
   - `PersistentStates { }` entries if Phase 4 uses conf-declared states.
2. **`Configs/Systems/Persistence/Configuration/OverthrowGameMode.conf`** — `EntityPersistenceConfig` inheriting vanilla `GameMode.conf`, appending our ~10 manager `ComponentSerializers`. If rule priority collides with the inherited vanilla entry, set `Priority` above vanilla's and keep vanilla's component serializers in ours (one config wins per instance).
3. **`Configs/Systems/Persistence/Configuration/OverthrowPlaceables.conf`** (Phase 3) — `EntityPersistenceConfig` with `ComponentClassPersistenceConfigRule` on `OVT_PlaceableComponent`, `SelfSpawn 1`, collection assignment. Likewise for `OVT_BuildableComponent` and base upgrades as analysis dictates.
4. Every new `.conf` needs a `.meta` with a fresh unique GUID; internal object GUIDs are also freshly generated. Follow the exact textual shape of the vanilla confs.
5. **`Configs/Systems/ChimeraSystemsConfig.conf`** (existing, GUID-override) — add the `SCR_PersistenceSystem` entry (`SystemLocation Server`, `SystemPoints 0x2000 0x10`, `Config` → Overthrow.conf).

### Script layer

- **`OVT_PersistenceManagerComponent`** (rewrite in place) — the façade Overthrow code and tests already call:
  - `SaveGame()` → `RequestSavePoint(ESaveGameType.MANUAL, ..., callback)`; surface real success/failure (fix `OVT_MainMenuContext.c:262-271` telling players "#OVT-Saved" unconditionally — route through `GetOnAfterSave()` or the operation callback).
  - `AutoSave()` → `RequestSavePoint(ESaveGameType.AUTO)` (consider `RequestSavePointOverwrite` of the latest AUTO point to avoid unbounded save lists).
  - `HasSaveGame()` → cached bool: async `GetSaves(GetCurrentMissionResource())` at init + subscribe `OnSaveCreated`/`OnSaveDeleted`. Document the brief init window where the cache may lag.
  - `WipeSave()` → `GetSaves` → `Delete` each (or `Purge(mission)`); async with callback.
  - Load-on-continue: `GetSaves` → `Load(latest)` (menu flow), and keep the existing event hooks (`GetOnStateChanged` → ACTIVE as the "loaded" signal; there is no OnAfterLoad invoker).
  - `OnGameEnd()` stays explicit (no engine event on ScriptComponent); decide whether SHUTDOWN saves are engine-driven or manager-driven and record it.
- **Serializers** in `Scripts/Game/Persistence/Serializers/{Components,Entities,States}/` — one class per system, modeled exactly on `SCR_SpawnPointSerializer` / `SCR_GameMasterMetaSerializer` / `SCR_EditableEntityComponentSerializer` (see api-reference §3.4). Manual `WriteValue("version", 1)` first; write order == read order (binary context); `notnull` qualifiers must match base signatures exactly.
- **Dedicated purge guard** — override the game-mode-end purge path in `OVT_OverthrowGameMode` (or configure keep-session-data) so dedicated servers don't wipe the campaign. Verify by reading `SCR_BaseGameMode.c:723-747` against our subclass.
- **Field truth**: each serializer's field list comes from the corresponding EPF SaveData class (e.g. `Scripts/Game/GameMode/Persistence/Components/OVT_TownSaveData.c`) and the live manager members. Preserve the stored-vs-derived split (`[NonSerialized]` skill-effect outputs are rebuilt, not persisted) — the test suites depend on it.

### Manager serializer inventory (Phase 2 scope, from the game mode prefab's EPF block)

Economy, Player, OccupyingFaction, RealEstate, Town, Resistance, Config, DeploymentManager, Recruit, LoadoutManager — 10 component serializers targeting the manager components on `OVT_OverthrowGameMode`. (InventoryManager's EPF SaveData appears vestigial — verify whether it holds state worth migrating and record the verdict.) Player loadouts (`OVT_PlayerLoadoutSaveData`, an EPF scripted state) are handled in Phase 4.

---

## Phases

Run `tools/compile-check.sh` after every file-touching step; `tools/run-tests.sh "{6A6E29FF47ECB840}"` (Fast) after every phase; `"{6A6E2A002F53A581}"` (All) before declaring a phase done. New test cases must be proven able to fail.

### Phase 1 — Foundation & Wiring (system online, honest save façade)

1. Create `Overthrow.conf` persistence config (inherit vanilla `Common.conf` by GUID) + `.meta`.
2. Register `SCR_PersistenceSystem` in `Configs/Systems/ChimeraSystemsConfig.conf` pointing at it.
3. Add native `Persistence` component to `Prefabs/GameMode/OVT_OverthrowGameMode.et` (keep the EPF component until Phase 5; it drives nothing).
4. Rewrite `OVT_PersistenceManagerComponent` on the real API: RequestSavePoint-based `SaveGame`/`AutoSave`, cached `HasSaveGame`, async `WipeSave`, honest save feedback into `OVT_MainMenuContext`, load-on-continue seam.
5. Handle the dedicated-server end-of-game purge.
6. Extend the Init test tier: persistence system resolves in the test world (`SCR_PersistenceSystem.GetScriptedInstance()` non-null server-side, state reaches ACTIVE).
7. **Empirical checkpoint:** run the test world once with a manual save; find where the save landed under the profile dir; record it in context.md (feeds `.scripts/` tooling in Phase 6). If Workbench-only, flag for the user instead.

Exit: compile clean, Fast tier green, Init persistence case green, `SaveGame()` observably produces a save (or the empirical checkpoint is documented as user-blocked).

### Phase 2 — Manager Component Serializers (the gate's substance)

1. `OverthrowGameMode.conf` inheriting vanilla `GameMode.conf`, appending our serializers.
2. Ten serializers, simplest first to validate the pattern: Config, Economy → Town, OccupyingFaction, Resistance, RealEstate → Recruit, DeploymentManager, LoadoutManager → Player (most complex: identity keying, per-player maps, `m_OnPlayerDataLoaded` invocation timing).
3. Persistent player identity: string persistent-ID keying stays; UUIDs only if entity refs demand them.
4. Run the round-trip gate after each serializer lands; track which of the 9 cases flip green.

Exit: compile clean, All tier green, round-trip suite green or the specific red cases have a named blocking reason.

### Phase 3 — Entity Persistence (placeables, buildables, bases, world state)

1. Conf rules + `SelfSpawn 1` for `OVT_PlaceableComponent` / `OVT_BuildableComponent`; `StartTracking()` at placement time; UUID refs (`GetId`/`WriteDefault(NULL_UUID)`/`WhenAvailable`) for base-upgrade→base links.
2. Deployments: decide entity-serializer vs manager-state representation from what `OVT_DeploymentSaveData` actually stores.
3. Verify vanilla-inherited persistence (characters/vehicles/items via `Common.conf`) doesn't fight Overthrow systems (recruit characters, shop vehicles); scope down with rules if it does.

### Phase 4 — States & Player Edge Cases

1. Player loadouts (replace `OVT_PlayerLoadoutSaveData` EPF scripted state) — conf-declared `PersistentState` + `ScriptedStateSerializer`, or fold into the Player/Loadout manager serializers if simpler. Record the decision.
2. Recruit↔character linkage across save/load (UUIDs + `WhenAvailable` timeout handling).
3. Reconnect flow sanity vs vanilla `SCR_PlayerReconnectData` (inherited via Common.conf) — ensure no double-handling.

### Phase 5 — EPF Removal

1. Strip `EPF_PersistenceComponent` blocks from all 59 prefabs (scripted edit; verify with grep zero remaining).
2. Delete: `Scripts/Game/GameMode/Persistence/` (12 files), `Scripts/Game/GameMode/SaveData/` (2), `OVT_CharacterControllerComponentSaveData.c`, embedded SaveData in `OVT_PlaceableComponent.c`/`OVT_PlayerOwnerComponent.c`/`OVT_BuildableComponent.c`, Deployment SaveData blocks, `OVT_InventoryManagerComponent.c` SaveData block, `Scripts/Game/Modded/EPF_PersistenceManager.c`.
3. Migrate remaining `EPF_Component` base-class uses to `OVT_Component` (`OVT_ComponentFinder<T>.Find()` replaces `EPF_Component<T>.Find()`).
4. Remove EPF/EDF from `addon.gproj`; remove persistence `#ifdef PLATFORM_CONSOLE` guards.
5. Full compile + All tier + grep proves zero `EPF_`/`EDF_` references in `Scripts/`.

### Phase 6 — Gate Green + Tooling

1. Round-trip suite exit 0 under the documented command; then de-quarantine (delete quarantine header, add to All group config `{6A6E2A002F53A581}`), update `tools/README.md`.
2. Update `.scripts/reset_save.sh` / `backup_save.sh` / `activate_save.sh` (three `DEFAULT_SAVE_DIR`s + reset's path guard + `.saves/` archive shape) to the empirically discovered save location. Contract: `tools/README.md` → "Save-state control".
3. Update context.md, epic-overview.md, CHANGELOG/user-facing breaking-change note.

### Phase 7 — Manual Play-Test Handoff (user, morning)

Produce a concrete checklist: SP save→quit→continue; dedicated server restart cycle; JIP client after load; MP two-player data isolation; placeables/bases surviving restart; loadout availability after restart; wanted-system/economy spot checks. Everything automated stays automated — this list is only what the harness can't reach (JIP/MP/UI/real restart).

---

## Risks / Open Questions

- **Conf-file GUID hygiene** — hand-authored `.conf`/`.meta` GUID collisions or malformed conf shapes fail silently in Workbench. Mitigation: mirror vanilla shapes exactly; the Init-tier "system ACTIVE" test catches a dead system immediately.
- **Round-trip suite mechanics** — the suite was written against the manager façade; whether its reload half can pass without a process restart depends on `RequestLoad` re-applying state in-session. If a real restart is unavoidable for some cases, that's a suite/harness conversation to document, not a reason to weaken the suite.
- **Vanilla Common.conf side effects** — inheriting it turns on persistence for many vanilla entity types at once in Overthrow's world. Watch the first post-Phase-2 world run for surprises (doors, garbage, AI groups).
- **`ShouldKeepSessionData()` config source** — where the conf sets it isn't confirmed; the fallback is the `-keepSessionSave` CLI param and/or overriding the purge path in our game mode subclass.
- **Save location on disk** — unknown until first real save; Phase 6 tooling blocked on the Phase 1 empirical checkpoint.

## File Map (planned)

```
Configs/Systems/Persistence/Overthrow.conf (+.meta)
Configs/Systems/Persistence/Configuration/OverthrowGameMode.conf (+.meta)
Configs/Systems/Persistence/Configuration/OverthrowPlaceables.conf (+.meta)   [P3]
Configs/Systems/ChimeraSystemsConfig.conf                                      [edit]
Prefabs/GameMode/OVT_OverthrowGameMode.et                                      [edit]
Scripts/Game/GameMode/Managers/OVT_PersistenceManagerComponent.c               [rewrite]
Scripts/Game/UI/Context/OVT_MainMenuContext.c                                  [edit: honest save feedback]
Scripts/Game/Persistence/Serializers/Components/OVT_<System>Serializer.c × 10  [P2]
Scripts/Game/Persistence/Serializers/Entities/…                                [P3]
Scripts/Game/Persistence/Serializers/States/…                                  [P4]
Scripts/Game/Persistence/Rules/OVT_DeadCharacterPersistenceConfigRule.c        [P4, added]
Scripts/Game/Tests/TestSuites/Init/… (persistence-system case)                 [P1]
```

**Added after the planned map** (P4 revisions, see tasks.md): `Scripts/Game/Persistence/OVT_PersistenceTracking.c` (Track/IsTracked/GetPersistentId/Save/Untrack/**ReloadConfig** helpers, P5+P4), and the dead-character rule above, which is bound by a standalone `EntityPersistenceConfig` inside `Configs/Systems/Persistence/Overthrow.conf` rather than its own conf file.

Reference docs: `vanilla-api-reference.md` (API truth) · EPF SaveData classes in-repo (field truth) · `templates/` (v1 serializer sketches — shapes only, do not trust their API calls).
