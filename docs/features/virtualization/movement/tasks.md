# Virtualization Movement - Task Checklist

**Last Updated:** 2026-08-17 15:30
**Progress:** 22/22 tasks complete (100%) — ALL DONE. Gates green (final: Fast 190 / All 236 after the three play-test fixes); user play-test passed 2026-08-17 ("all play-test items are green").

> Phases 2 and 3 are **ADVANCED** (`component-developer-advanced`); phases 1 and 4 are standard (`component-developer`). Suite per phase: 1 → Fast, 2 → Fast, 3 → **All**, 4 → Fast (All if a persistence-tier file was touched). Source of truth: `implementation.md` §4.

---

## Phase 1: Progression maths + Logic tier (2/2 complete) — STANDARD ✅

- [x] ✅ **T1.1 Create `OVT_VirtualMovementMath.c`** — world-free statics only
  - Description: `DistanceXZ`, `DistancePointToSegmentXZ`, `ProjectOntoPlan`, `IsStationaryPlan`, `NextTargetIndex`, `ResolveStepDistance`, `AdvanceTowardsXZ`, `DrainWait`, `SortHandlesAscending` (hand-rolled), `IsExternalMove`, constants `ARRIVAL_RADIUS_M`/`MAX_STEP_SECONDS`/`EXTERNAL_MOVE_EPSILON_M`. No manager/world/entity/registry identifier anywhere in the file (Logic-tier grep rule).
  - File(s): `Scripts/Game/GameMode/VirtualMovement/OVT_VirtualMovementMath.c` (new)
  - Estimate: 2 h

- [x] ✅ **T1.2 Create Logic-tier test file** — Fast group (7 cases, Fast 186 green 2026-08-17)
  - Description: `OVT_TEST_Logic_VirtualMovement.c` (suite `OVT_TEST_LogicSuite`), cases per implementation.md §7: projection, plan classification, index advance, stepping, wait drain, ordering, external-move predicate. Recorded can-fail proof per case; no `maxAttempts`; no exact distance boundaries (vector.Distance +1 ULP trap).
  - File(s): `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_VirtualMovement.c` (new)
  - Estimate: 2 h

---

## Phase 2: The additive core seam (4/4 complete) — ADVANCED ✅

- [x] ✅ **T2.1 Add `GetAllHandles()` to the frozen core manager** (+23/-0, beside `FindGroupsBySystem`)
  - Description: Beside `FindGroupsBySystem` (~`:1578`); same finder shape; null-guard `m_mRecords`; doc comment states order is unstable, callers sort. The ONLY code change to `Scripts/Game/GameMode/Virtualization/` in this feature.
  - File(s): `Scripts/Game/GameMode/Virtualization/OVT_VirtualizationManagerComponent.c`
  - Estimate: 0.5 h

- [x] ✅ **T2.2 Update frozen `api.md`** (§3 + §10 movement row + header note)
  - Description: Add signature to §3 "Queries and reclaim"; add row to §10 `movement` table; extend header's "Additively extended" note.
  - File(s): `docs/features/virtualization/core/api.md`
  - Estimate: 0.5 h

- [x] ✅ **T2.3 Dated additive note in core `context.md`** (2026-08-17 entry)
  - Description: "Additive changes after the freeze" entry naming `virtualization/movement` (Phase 2, plan §3.7): what was added, why `FindGroupsBySystem` is insufficient, why non-breaking.
  - File(s): `docs/features/virtualization/core/context.md`
  - Estimate: 0.25 h

- [x] ✅ **T2.4 Init-tier case for `GetAllHandles()`** (`OVT_TEST_Init_Virtualization_GetAllHandlesEnumeratesRegistry`; Fast 187 green)
  - Description: Register two groups via `OVT_TEST_VirtualizationFixture`, assert both handles present and count matches `GetGroupCount()`, unregister (cleanup before reporting), assert gone. Can-fail proof in preamble.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c`
  - Estimate: 0.75 h

---

## Phase 3: The manager, the tick and the writes (10/10 complete) — ADVANCED ✅ (All 233 green 2026-08-17)

- [x] **T3.1 FIRST: make shared test fixtures stationary (F-A)**
  - Description: In `OVT_TEST_PersistenceRoundTripSuite.c` `BuildPlan()` (`:4587-4601`) change waypoint **types** to `DEFEND` (keep two distinct positions/params, `m_bCycle = true` — payload claims unchanged); extend case preamble with the why. Sweep `grep -rn "RegisterGroup(" Scripts/Game/Tests/` and record a verdict per site.
  - File(s): `Scripts/Game/Tests/TestSuites/Persistence/OVT_TEST_PersistenceRoundTripSuite.c`
  - Estimate: 1 h

- [x] **T3.2 Moved-position persistence claim**
  - Description: In the same round-trip case, `SetPosition(handle, registrationPosition + (0,0,42))` before save; assert restore at the moved position. Timing-free direct write.
  - File(s): `Scripts/Game/Tests/TestSuites/Persistence/OVT_TEST_PersistenceRoundTripSuite.c`
  - Estimate: 0.75 h

- [x] **T3.3 Create `OVT_VirtualMovementState.c`**
  - Description: Plain `Managed` transient state class per plan §3.3 — no attributes, no serialization.
  - File(s): `Scripts/Game/GameMode/VirtualMovement/OVT_VirtualMovementState.c` (new)
  - Estimate: 0.25 h

- [x] **T3.4 Create manager component skeleton**
  - Description: `OVT_VirtualMovementManagerComponent(Class)`, attributes per §3.9, `s_Instance` + world-re-resolving `GetInstance()`, server guard in `OnPostInit` before allocation, `Init(IEntity)`, idempotent `PostGameStart()` tick install (`m_bTickRunning` latch), full `OnDelete` teardown (remove CallLater, clear map, clear `s_Instance`).
  - File(s): `Scripts/Game/GameMode/VirtualMovement/OVT_VirtualMovementManagerComponent.c` (new)
  - Estimate: 2 h

- [x] **T3.5 Implement the tick (§3.4)**
  - Description: dead-world self-cancel first; enumerate via `GetAllHandles` + `SortHandlesAscending` + count-guarded purge; slice via `OVT_VirtualizationMath.SliceIndices`/`AdvanceCursor`; per-handle body with `IsSpawned` gate FIRST.
  - File(s): `OVT_VirtualMovementManagerComponent.c`
  - Estimate: 2 h

- [x] **T3.6 Implement `DeriveState` + external-move re-derivation**
  - Description: Lazy projection resume (§3.3), external-move epsilon check (D9), stationary latch, wait drain, arrival handling, §3.5 type semantics (MOVE/PATROL/WAIT/DEFEND/CYCLE, wrap vs ping-pong).
  - File(s): `OVT_VirtualMovementManagerComponent.c`
  - Estimate: 2 h

- [x] **T3.7 Implement the write**
  - Description: Ground snap `GetSurfaceY`, water veto `OVT_WorldUtils.IsOceanAtPosition` (advance accumulator, write only land), then `SetPosition` — the feature's ONLY core mutation.
  - File(s): `OVT_VirtualMovementManagerComponent.c`
  - Estimate: 1 h

- [x] **T3.8 Implement `m_bDebugMovementLogging`**
  - Description: One line per advance: handle, from→to, target index, step m, wait remaining, land/water, adopted/resumed. Off by default; NORMAL level when switched on.
  - File(s): `OVT_VirtualMovementManagerComponent.c`
  - Estimate: 0.5 h

- [x] **T3.9 Wire up: OVT_Global + game mode + prefab**
  - Description: `OVT_Global.GetVirtualMovement()` (beside `GetVirtualization()` ~`:252`); game-mode field + `EOnInit` FindComponent/Init + `PostGameStart()` call, all AFTER Virtualization's; text-wire component onto `Prefabs/GameMode/OVT_OverthrowGameMode.et` with fresh repo-unique GUID. Flag Workbench verification as user task.
  - File(s): `Scripts/Game/Global/OVT_Global.c`, `Scripts/Game/GameMode/OVT_OverthrowGameMode.c`, `Prefabs/GameMode/OVT_OverthrowGameMode.et`
  - Estimate: 1.5 h

- [x] **T3.10 Seed movement `context.md` handoff content**
  - Description: Findings F-A/F-B, seam contract §3.8, play-test list §6, "lost progress is by design" note.
  - File(s): `docs/features/virtualization/movement/context.md`
  - Estimate: 0.5 h

---

## Phase 4: Init-tier coverage, seam documentation, scale check (6/6 complete) — STANDARD ✅

- [x] ✅ **T4.1 Init case: tick advances a dormant group** (`OVT_TEST_Init_VirtualMovement_TickAdvancesDormantGroup`)
  - Description: Two-point PATROL plan, 200 m, leg picked all-land (the water rule writes nothing over a bay and would read like a dead tick); bounded wall-clock poll of 10 s (~5 passes — the FIRST pass over a handle derives state with dt 0 and cannot move it); asserts displacement > 1 m, < the whole leg, and closer to the target than the start. Cleanup BEFORE reporting. Tolerance-based, no `maxAttempts`.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c`
  - Estimate: 1 h

- [x] ✅ **T4.2 Init case: stationary plan never advances** (`OVT_TEST_Init_VirtualMovement_StationaryPlanIsNeverAdvanced`)
  - Description: Same shape and the same picked leg as T4.1 with ONE variable changed — the waypoint type; asserts XZ drift ≤ 0.5 m over the whole 10 s window (D10 opt-in contract). Cleanup BEFORE reporting.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c`
  - Estimate: 0.5 h

- [x] ✅ **T4.3 Init case: manager resolves, no leaks** (`OVT_TEST_Init_VirtualMovement_ManagerResolvesAndDoesNotLeak` + `GetTrackedCount()`)
  - Description: `OVT_Global.GetVirtualMovement()` non-null; bounded wait for the tracked count to return to 0 after the cases above unregister (the map is emptied by the tick, not by `UnregisterGroup`); then a registered DEFEND-only group contributes 0 while registered (plan-stationary groups hold no entry — context.md Gotcha #4). Read via the diagnostic `GetTrackedCount()` getter — the phase's ONLY production change.
  - File(s): `OVT_TEST_InitSuite.c`, `OVT_VirtualMovementManagerComponent.c` (getter only)
  - Estimate: 0.5 h

- [x] ✅ **T4.4 Extend context.md: "For `integration`" section**
  - Description: The Phase 3 seam-contract section retitled and addressed to `integration`'s planner, with §3.8's three consequences now restated **verbatim** and a note on which of them the Init cases assert. Phase 3's play-test results are still owed by the user (checklist unchanged).
  - File(s): `docs/features/virtualization/movement/context.md`
  - Estimate: 0.5 h

- [x] ✅ **T4.5 Scale check — user play-test PASSED 2026-08-17** ("all play-test items are green"; specific step/period numbers not recorded)
  - Description: Needs a live campaign, so Phase 4 documented the exact recipe instead of running it: `m_iDebugTestGroupCount = 40`, `m_bDebugMovementLogging = true`, expect all 40 handles logged within one round-robin period (40/8 × 2 s = 10 s) and a per-advance step ≈ speed × elapsed ≈ 15 m (D5), no hitch. Entry added to context.md's Manual Testing Checklist; **numbers to be recorded back there**.
  - File(s): `docs/features/virtualization/movement/context.md`
  - Estimate: 1 h

- [x] ✅ **T4.6 Update epic overview**
  - Description: `movement` row → 🟡 Code Complete (play-test owed) with a one-line summary and a task count; `integration` row notes the auto-adoption seam is now live. Table not renumbered or restructured.
  - File(s): `docs/features/virtualization/epic-overview.md`
  - Estimate: 0.25 h

---

## Bugs & Issues

**Active Bugs:**
- (none)

**Fixed Bugs** (play-test findings, fixed 2026-08-17 — NOT tracked in the bug system per user instruction: in-development features don't file bugs, the IDs collide with main):
- [x] ✅ 🐛 **Waypoints buried in terrain / floating in air** (user play-test report). Root cause in CORE: plan positions carry arbitrary Y and `CreatePlannedWaypoint` spawned entities verbatim; a completion radius smaller than the Y error could stall live AI at waypoint 0 forever. Fix: surface-snap the waypoint ENTITY spawn (plan payload untouched) + the debug affordance's authored positions.
- [x] ✅ 🐛 **Despawned groups resume AWAY from their live waypoint** (user play-test report). Coincident legs on a 2-point cycling route make direction unrecoverable by position-only projection. User-approved fix: additive core `GetCurrentPlanIndex(handle)` (entity-identity match of the live current waypoint; cycle entity → 0; -1 = fall back to projection); movement consults it on adoption via a `liveIndex` param on `DeriveState`. Movement's waypoint-identifier greps still 0.
- [x] ✅ 🐛 **Manual-policy (`spawnDistanceOverride = 0`) groups materialised anyway** (found by the movement Init cases — the first tests to watch a registered group across seconds). Vanilla's `m_bSpawnImmediately` queues a full spawn at entity init and the queue re-checks only OBSERVER presence (GM/autotest cameras qualify), ignoring the stamped policy. Fix in the modded `SCR_AIGroup`: masked Manual groups refuse `ExpandOneMember`/`SpawnMembers` dispatches unless `ForceSpawn` armed `ArmOVTManualSpawn()`; `IsExpandComplete` answers true so the queue drops rather than retries. The three movement Init cases now register with override 0 (dormant by construction).

⚠️ Movement's I1 criterion "core diff = exactly one method" now reads: one method (`GetAllHandles`) + `GetCurrentPlanIndex` (additive #3) + the waypoint-snap and ForceSpawn-arm fixes, plus the modded `SCR_AIGroup` guard. All documented in core `context.md` with dated entries.

**Final gates after all three fixes:** Fast **190/190**, All **236/236** (2026-08-17 ~15:10).

---

## Technical Debt

- (none yet)

---

## Needs human verification

- [x] ✅ Workbench: `OVT_VirtualMovementManagerComponent` confirmed resolving on the prefab — user verified 2026-08-17 ("it always does, you can hand author GUIDs, it's never a problem")
- [x] ✅ Manual play-test passes of implementation.md §6 — user confirmed green 2026-08-17 (surfaced and drove the three play-test fixes above first)
- [x] ✅ T4.5 scale check — user confirmed green 2026-08-17 (numbers not recorded)

---

## Progress Tracking

### Completed This Session (2026-08-17)
- Phases 1–4 built via /autorun-feature. Phase 4: three Init cases + one diagnostic getter + the `For integration` section + the T4.5 recipe + the epic row. Compile 0.
- All gates closed by the orchestrator: Fast 186 (P1) → Fast 187 (P2) → All 233 (P3) → Fast 190 (P4, after the tick-install fix — context.md Gotcha 6 — and one unrelated Setup_Checkpoint I/O flake, Gotcha 7).

### Discovered New Tasks
- (none — the tick-install fix was folded into the three Phase 4 cases rather than tracked separately)

### Blocked Items
- (none)

---

*Update this file as tasks are completed. Mark tasks with ✅ immediately when done. Add new tasks as they're discovered.*
