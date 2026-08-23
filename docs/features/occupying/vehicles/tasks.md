# Vehicles - Task Checklist

**Last Updated:** 2026-08-23
**Progress:** 59/59 tasks complete (100%)

> **Agent routing:** phases **2, 3, 4** are **ADVANCED** (`component-developer-advanced`; phase 4 at max effort); phases **1, 5, 6** are STANDARD (`component-developer`); phase **7** is `help-docs-sync`.
> **Suite per phase:** ⚠ **DEFERRED FOR THE WHOLE FEATURE.** The user is running Workbench/play-test sessions on `v1.5` for the duration of this build (instruction 2026-08-23), and `tools/run-tests.sh` launches a real Reforger client that steals desktop focus and returns INDETERMINATE under a concurrent Workbench session. Per-phase gate is therefore **`tools/compile-check.sh` exit 0 only**; the **All** group `{6A6E2A002F53A581}` is run **once, at the end of Phase 7**, as the feature's single regression gate. This is an announced skip, not a silent one — see `.claude/test-policy.md` §2 "Defer the gate".
> **Source of truth:** `implementation.md` §4, with §3 for the contracts and the Agent Routing Summary for tiers. C1–C9 in `implementation.md` are verified working-tree corrections and override any older statement.
> **Frozen neighbours, every phase:** `git diff Scripts/Game/GameMode/Virtualization/ Scripts/Game/GameMode/VirtualMovement/ docs/features/virtualization/core/api.md UI/ Configs/Map/ Scripts/Game/Persistence/ Configs/Systems/Persistence/ Configs/Language/` must stay **empty**.
> **The one-line rule (G6):** `git diff Scripts/Game/GameMode/Deployments/Modules/OVT_InsertionSpawningDeploymentModule.c` is **one appended enum value and its comment** for the entire feature. Re-check it at the end of every phase from 2 onward.
> **GUID prefix:** `{6BC1....}` — re-grep `grep -rl "6BC10000" Configs/ Prefabs/ Scripts/` before authoring; a concurrent session may have claimed it.

---

## Phase 1: The ladder — registry fields, difficulty scalar, pure resolver (10/10 complete) ✅ — STANDARD

- [x] **T1.1 `OVT_VehicleLadderRules` (pure statics)**
  - Description: `ScaledThreshold`, `RungUnlocked`, `RungAffordable`, `PickRung` per implementation.md §3.2. ⚠ No `OVT_Global`, `GetGame()`, `World` or `Entity` identifier anywhere in the file, **comments included** (the Logic-tier grep is directory-wide and does not distinguish code from prose). ⚠ `out` and `owned` are reserved local names.
  - File(s): `Scripts/Game/Data/OVT_VehicleLadderRules.c`
  - Estimate: 1.5 h

- [x] **T1.2 Two fields on `OVT_FactionVehicleEntry`**
  - Description: `string m_sLadderRole` (empty default = on no ladder) and `int m_iMinThreat` (0), both with a `desc:` a tuner can act on.
  - File(s): `Scripts/Game/Faction/OVT_Faction.c`
  - Estimate: 0.5 h

- [x] **T1.3 `ResolveLadderRung(...)` + `ResolveVehicleForRole(...)`**
  - Description: Registry filter by role → parallel arrays → `PickRung`; null-safe `OVT_Faction` wrapper in the shape of `GetVehiclePrefabByName`. Return **false** rather than an empty `ResourceName` so "no rung" and "bad prefab" stay distinguishable.
  - File(s): `Scripts/Game/Faction/OVT_Faction.c`
  - Estimate: 1 h

- [x] **T1.4 `float vehicleThresholdScale` on `OVT_DifficultySettings`**
  - Description: category `"Occupying Faction"`, `defvalue "1"`, `desc:` naming the Easy…Insane spread. Follow exactly how the twelve `objective*` fields were added.
  - File(s): `Scripts/Game/Configuration/OVT_DifficultySettings.c`
  - Estimate: 0.5 h

- [x] **T1.5 Author the six difficulty presets**
  - Description: Easy 2.0, Normal 1.0, Hard 0.5, Extreme 0.35, Insane 0.25, TestWorld **1.0 explicitly** (so no test depends on a defvalue).
  - File(s): `Configs/Difficulty/Difficulty_*.conf` (6 files)
  - Estimate: 0.5 h

- [x] **T1.6 Author both faction vehicle registries**
  - Description: role + threshold on the four existing armed entries; one new entry each (`medium_armed` BRDM-2 USSR, `heavy_armor` LAV-25 US); **explicit `m_iCost` and `m_iMaxCapacity` on both `heavy_armed` entries** (C7 — USSR BTR-70 was silently defaulting to cost 50 / capacity 4 against a 10-position hull). New entry GUIDs from `{6BC11…}`.
  - File(s): `Configs/Factions/USSR_OverthrowData.conf`, `Configs/Factions/US_OverthrowData.conf`
  - Estimate: 1 h

- [x] **T1.7 Two prefab deltas (BRDM-2, LAV-25)**
  - Description: **same-GUID deltas** (`.meta` `Name` = the vanilla GUID, C8) carrying only the Ural-proven `AICarMovementComponent` tuning: `FrictionCoefficient 0.1`, `MaxReverseTravelDistance 30`, `"Min Prediction Distance" 2`. ⚠ Copy inherited component GUIDs, never mint them (the duplicate-`ActionsManagerComponent` trap).
  - File(s): `Prefabs/Vehicles/Wheeled/BRDM2/BRDM2.et(.meta)`, `Prefabs/Vehicles/Wheeled/LAV25/LAV25.et(.meta)`
  - Estimate: 1 h

- [x] **T1.8 `OVT_TEST_Logic_VehicleLadder.c`**
  - Description: `ScaledThreshold` for scale 0 / negative / 2.0 / 0.25; `RungUnlocked` **at exactly** the threshold (unlocked, `>=`) and one below; `RungAffordable` with `budget -1`; `PickRung` top rung / top unaffordable → middle / low threat → bottom / `-1` on empty and on all-locked / tie breaks to lowest index. Floats via `OVT_TEST_LogicFixture.EPSILON`.
  - File(s): `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_VehicleLadder.c`
  - Estimate: 1.5 h

- [x] **T1.9 Init case against the shipped registries**
  - Description: Both shipped faction registries resolve role `armed` to three distinct rungs; `ResolveVehicleForRole` answers false for an unknown role. Append to `OVT_TEST_Init_VehiclePriceSpecificity.c` or a new small file.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/`
  - Estimate: 1 h

- [x] **T1.10 context.md — the rung table as authored**
  - Description: The table as it landed, the name-asymmetry rationale (G9/D3), and the two capacity/cost corrections **with their before values**.
  - File(s): `docs/features/occupying/vehicles/context.md`
  - Estimate: 0.5 h

**Acceptance:** compile 0 · `grep -rn "OVT_Global\|GetGame()\|World\|Entity" Scripts/Game/Data/OVT_VehicleLadderRules.c` empty · `vehicleThresholdScale` 1× in each of 6 difficulty confs · `m_sLadderRole "armed"` 3× in each faction conf · `git diff Configs/Deployment/ Scripts/Game/GameMode/` empty.

---

## Phase 2: The mounted-force module (12/12 complete) ✅ — **ADVANCED**

- [x] **T2.1 Read-only survey (gates the phase)**
  - Description: Re-verify `OnInsertionArrived` (:1893), `CompleteInsertion` (:1861), `OnUpdate` (:1053), `GetVehiclePrefabFromFaction` (:3219), `CloneModule` (:3304), `ReleaseConvoy` (:1956), `DropPassengersToGlobalRing` (:3052), and the suppression skip at `OVT_DeploymentManager.c:565-568`. A moved line means stop and re-baseline.
  - File(s): `docs/features/occupying/vehicles/context.md`
  - Estimate: 1 h

- [x] **T2.2 Append `HOLDING` to `OVT_EInsertionState`**
  - Description: One enum value + a one-line doc comment. ⚠ **The only permitted edit to the insertion module for the whole feature** (G6). Safe because the enum is runtime-only — no convoy resumes across a load.
  - File(s): `Scripts/Game/GameMode/Deployments/Modules/OVT_InsertionSpawningDeploymentModule.c`
  - Estimate: 0.25 h

- [x] **T2.3 The module class + header**
  - Description: `OVT_MountedForceSpawningDeploymentModule : OVT_InsertionSpawningDeploymentModule` with the four attributes of §3.3. Header states what it owns (vehicle, crew registration, waypoints, convoy slot), what it borrows, that the walk fallback is still the spine, and that **passengers must never be dropped to the global ring** — the riding ring at `RIDING_SPAWN_DISTANCE` is what exempts them from battle suppression (C3/D8).
  - File(s): `Scripts/Game/GameMode/Deployments/Modules/OVT_MountedForceSpawningDeploymentModule.c`
  - Estimate: 2 h

- [x] **T2.4 `GetVehiclePrefabFromFaction` override**
  - Description: Ladder-resolve `m_sVehicleRole` against live threat, `vehicleThresholdScale` and `m_iTruckCostOverride` as the budget. On false: log **once** at NORMAL naming the role and the threat, then fall back to `m_sTruckVehicleType` — the drive-vs-walk decision never depends on the ladder succeeding.
  - File(s): `…/OVT_MountedForceSpawningDeploymentModule.c`
  - Estimate: 1.5 h

- [x] **T2.5 `CompleteInsertion` override**
  - Description: No disembark, **no `DropPassengersToGlobalRing()`**, release the convoy reservation, enter `HOLDING`, call `OnInsertionArrived(m_vLZ)`. ⚠ `ReleaseReservation()` **must** still be called — a leaked convoy slot is permanent and eventually stops the faction driving anywhere.
  - File(s): `…/OVT_MountedForceSpawningDeploymentModule.c`
  - Estimate: 1.5 h

- [x] **T2.6 `OnUpdate` override with the `HOLDING` branch**
  - Description: `super.OnUpdate(deltaTime)` **first, unconditionally** (the abandoned-truck sweep and UNDECIDED retry still run), then: vehicle destroyed or crew dead → `DismountAndWalk("its vehicle was destroyed")`; `m_iHoldTicks` expired → **latch, not collect**. 🔴 **PLAN DEFECT, resolved:** `RequestDeploymentCollection` is `protected` on `OVT_BaseBehaviorDeploymentModule` and is **unreachable from a spawning module**; reaching around it would mean a second copy of the exfiltration rule. The hold clock latches instead and is read through the new public `IsHoldExpired()`. **Phase 6 must poll it from the sweep behaviour, or drop `m_iHoldTicks` from `Deployment_HunterKillerSweep.conf` and keep one clock.** See context.md → Phase 2.
  - File(s): `…/OVT_MountedForceSpawningDeploymentModule.c`
  - Estimate: 1.5 h

- [x] **T2.7 `CloneModule()` — all 27 fields**
  - Description: 13 grandparent + 10 parent + 4 own. The shipped clone at `:3304-3337` is the template. ⚠ Add the parent's own "what a dropped line costs" sentence for the four new ones. **This is the single highest-frequency defect in this module system.**
  - File(s): `…/OVT_MountedForceSpawningDeploymentModule.c`
  - Estimate: 1.5 h

- [x] **T2.8 The three public seams**
  - Description: `GetMountedVehicle()`, `SetSourceOverride(vector)`, `AdoptVehicle(Vehicle)`, plus `EnsureSourceResolved` preferring the override. `AdoptVehicle` is inert until Phase 5 authors `m_bAdoptExistingVehicle` — ship it now so Phase 5 is a config change plus a behaviour module.
  - File(s): `…/OVT_MountedForceSpawningDeploymentModule.c`
  - Estimate: 1.5 h

- [x] **T2.9 `ForceCreateDeploymentFrom(..., sourcePosition)`**
  - Description: Create, then walk the created component's **runtime** modules and `SetSourceOverride` every mounted one **before the first convergence**. ⚠ Verify `CreateDeployment`'s activation ordering (`OVT_DeploymentManager.c:1868`) — if activation is synchronous with creation the override must be applied **inside** `CreateDeployment`. Record the answer in context.md.
  - File(s): `Scripts/Game/GameMode/Deployments/OVT_DeploymentManager.c`
  - Estimate: 2 h

- [x] **T2.10 `OVT_TEST_Init_MountedForce.c`**
  - Description: clone fidelity with **27 distinct non-default values** and a consequence-naming `SetFailure`; ladder override picks the expected rung for a planted threat and falls back with the role unauthored; `CompleteInsertion` leaves state `HOLDING`, passengers seated, reservation released; a destroyed vehicle in `HOLDING` reaches `WALKING`; `SetSourceOverride` wins over the authored provider. ⚠ Deployment fixtures must be `SetSpawnedUnitsEliminated(true)` on the deployment **and every spawning module** before anything ticks.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_MountedForce.c`
  - Estimate: 3 h

- [x] **T2.11 Battle-suppression case (the mechanical guard on C3)**
  - Description: A group registered at `RIDING_SPAWN_DISTANCE` inside the battle circle is **not** pinned.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_DeploymentBattleSuppression.c`
  - Estimate: 1 h

- [x] **T2.12 context.md — ownership + activation ordering**
  - Description: The ownership split for a mounted force, why `DropPassengersToGlobalRing` is not called, the T2.9 activation-ordering answer, and every can-fail proof.
  - File(s): `docs/features/occupying/vehicles/context.md`
  - Estimate: 0.75 h

**Acceptance:** compile 0 · insertion-module diff = **one enum value** · `grep -n "DropPassengersToGlobalRing" …MountedForce….c` empty · `grep -c "clone\." …MountedForce….c` ≥ 27 · `git diff Configs/` empty.

---

## Phase 3: Mounted harassment and the mobile checkpoint (8/8 complete) ✅ — **ADVANCED**

- [x] **T3.1 `OVT_MobileCheckpointBehaviorDeploymentModule`**
  - Description: `: OVT_BaseBehaviorDeploymentModule`. Attributes `m_fApproachMinDistance` 150, `m_fApproachMaxDistance` 300, `m_fRoadSearchRadius` 120, `m_iRelocateMinutes` 8, `m_fCheckpointSpread` 25. On activate: find the mounted module via `m_ParentDeployment.GetSpawningModules()`, take `GetMountedVehicle()`, road-park it on an approach facing along the segment, dismount **infantry** to a perimeter, leave the **crew** aboard so the gun stays manned. Relocate clock → different bearing. ⚠ Bearing selection **excludes the last used** so a two-road town does not oscillate.
  - File(s): `Scripts/Game/GameMode/Deployments/Modules/OVT_MobileCheckpointBehaviorDeploymentModule.c`
  - Estimate: 3 h

- [x] **T3.2 Approach selection**
  - Description: Sample N bearings at the authored band, keep those with a road inside `m_fRoadSearchRadius`, choose uniformly at random. ⚠ `RandInt` is **max-exclusive**. ⚠ `array.Remove()` is swap-with-last — use `RemoveOrdered` if order matters.
  - File(s): `…/OVT_MobileCheckpointBehaviorDeploymentModule.c`
  - Estimate: 1.5 h

- [x] **T3.3 `Deployment_ObjectiveHarassment_Mounted.conf`**
  - Description: `{6BC10…}`, modelled line-for-line on `Deployment_ObjectiveHarassment.conf`: mounted module (role `armed`, `m_sGroupType "light_fireteam"`, `m_iTruckCostOverride 70`, `m_fLZStandoffDistance 250`, `m_Source OVT_ObjectiveAnchorSourceProvider`), the checkpoint behaviour, `OVT_ReinforcementBehaviorDeploymentModule`, and `OVT_ObjectiveConditionDeploymentModule` with **`m_sFromPhase "Harassment"` / `m_sThroughPhase "ForwardBase"`** — the phase **range** is what stops the phase-3 deadlock and must not collapse to an equality. `m_bDirectorOnly 1`, `m_iBaseCost 45`, `m_iAllowedLocationTypes TOWN|BASE`.
  - File(s): `Configs/Deployment/Deployment_ObjectiveHarassment_Mounted.conf(.meta)`
  - Estimate: 1.5 h

- [x] **T3.4 Register it**
  - Description: `overthrowDeployments.conf` entry `"Objective Harassment (Mounted)"`.
  - File(s): `Configs/Deployment/overthrowDeployments.conf`
  - Estimate: 0.25 h

- [x] **T3.5 Wire both plans (four places)**
  - Description: Append the rung to the harassment operation's `m_aLadder` in the **Harassment** phase and the **ForwardBase** phase of both `Objective_TownOffensive.conf` and `Objective_BaseOffensive.conf`. ⚠ Ladder order **is** the ramp — the rung goes **last** so it saturates. ⚠ `.conf` files cannot carry comments; the ordering contract lives in the module headers.
  - File(s): `Configs/Objective/Objective_TownOffensive.conf`, `Configs/Objective/Objective_BaseOffensive.conf`
  - Estimate: 1 h

- [x] **T3.6 Init cases**
  - Description: The ladder now has five rungs and rung 5 resolves to the mounted config; the checkpoint module clones completely; the approach chooser refuses a bearing with no road and never returns the previous bearing twice running.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_ObjectiveOperations.c`
  - Estimate: 2 h

- [x] **T3.7 Logic case for the pure part**
  - Description: Bearing arithmetic, band clamping, previous-bearing exclusion — extracted to a world-free static if it can be. No world identifiers, comments included.
  - File(s): `Scripts/Game/Tests/TestSuites/Logic/`
  - Estimate: 1 h

- [x] **T3.8 Play-test checklist into context.md**
  - Description: Written **before the phase closes** so the orchestrator has a script (implementation.md §6 steps 2–4).
  - File(s): `docs/features/occupying/vehicles/context.md`
  - Estimate: 0.5 h

**Acceptance:** compile 0 ✅ · `grep -c "Objective Harassment (Mounted)" Configs/Objective/*.conf` → **2 + 2 = 4** ✅ · `m_sThroughPhase "ForwardBase"` present ✅ · `git diff Configs/Deployment/Deployment_ObjectiveHarassment.conf` empty ✅.

**⚠ Landed differently from the plan in four places — full reasoning in `context.md` → Phase 3:**
1. **T3.5 has a FIFTH edit site.** `OVT_ObjectiveDirectorComponent.HARASSMENT_LADDER` is a code constant that `IsObjectiveOperationConfig()` and three shipped Init cases match against; the four `.conf` edits alone would have shipped a rung the director does not recognise.
2. **T3.3's module list was incomplete.** The config also authors `OVT_TownHarassmentBehaviorDeploymentModule` (`m_iHoldSeconds -1`), first among the behaviour modules. Without it the ramp DEADLOCKS at rung 5: the ladder index saturates at the top, and the forward-base gate opens on the town's support falling.
3. **`m_iAllowedLocationTypes` is authored numerically as `3`.** No multi-flag textual form exists anywhere in either tree; vanilla writes combined flags as integers. ⚠ Workbench-only verification.
4. **The config authors `m_iRelocateMinutes 4`, not the attribute default of 8.** A harassment operation is collected by its own hold (90-240 s plus the drive), so an eight-minute clock would never fire.

---

## Phase 4: QRF mounted echelon (10/10 complete) ✅ — **ADVANCED (max effort)**

- [x] **T4.1 Read-only survey (gates the phase)**
  - Description: `SendWave` (:747), `SpendWholeBudgetInOnePass` (:831), `SpawnTroops` (:875), `GetLandingZone` (:1074), `BuildSiegeRing` (:707), the four index-parallel spawn arrays (:31-40), the scoring loop (:468-596), `m_OnFinished`. **Confirm the single reserve debit is still at :800-806 and still outside the mode branch.**
  - File(s): `docs/features/occupying/vehicles/context.md`
  - Estimate: 1.5 h

- [x] **T4.2 `Deployment_QRFMountedEchelon.conf`**
  - Description: `{6BC10…}`; mounted module (role `armed`, `m_sGroupType "light_fireteam"`, `m_iTruckCostOverride 90`, `m_fLZStandoffDistance 0` — the deployment position **is** the standoff point), the mobile-checkpoint behaviour with `m_iRelocateMinutes 0` (park, do not roam), **no** reinforcement module. `m_bDirectorOnly 1`, `m_iMaxInstances -1`, all location types.
  - File(s): `Configs/Deployment/Deployment_QRFMountedEchelon.conf(.meta)`, `overthrowDeployments.conf`
  - Estimate: 1.5 h

- [x] **T4.3 `SendMountedEchelon(vector source, vector target, int budget)`**
  - Description: In order — cap check (`ECHELON_CAP_PER_BATTLE` 2); reachability (`GetNearestBase(source).landIsolated` false **and** `OVT_WorldUtils.FindNearestRoadSpawn(source, ECHELON_ROAD_SEARCH_M, …)` true); ladder resolve inside `budget`; standoff point on the source→target line at `ECHELON_STANDOFF_M` 450, road-snapped; `ForceCreateDeploymentFrom(config, standoff, factionIndex, source, 0 /* invested */)`; record the component for teardown; **return the rung cost**. Every refusal returns 0 and logs at VERBOSE with the reason.
  - File(s): `Scripts/Game/Controllers/OccupyingFaction/OVT_QRFControllerComponent.c`
  - Estimate: 4 h

- [x] **T4.4 Call it from both modes**
  - Description: Once per source, immediately after that source's infantry allocation, inside the existing `allocated` accumulation so it flows into `spent`. ⚠ **Do not add a second debit and do not move the existing one.** The mode branch still ends before the debit.
  - File(s): `…/OVT_QRFControllerComponent.c`
  - Estimate: 1.5 h

- [x] **T4.5 Siege anchors**
  - Description: In `COUNTER_ATTACK` the standoff point is a **ring slot**, not a line point — reuse `OVT_QRFSiege.RingSlotOffset` and the ocean walk-in of `BuildSiegeRing`, then road-snap. ⚠ `BuildSiegeRing` runs **after** `SendWave` returns and is computed from the final queue length — an echelon must **not** touch `m_aSpawnQueue` or the four index-parallel arrays. It is a deployment, not a queued group.
  - File(s): `…/OVT_QRFControllerComponent.c`
  - Estimate: 2.5 h

- [x] **T4.6 Teardown on `m_OnFinished`**
  - Description: Every echelon this battle created is torn down with **`DeleteDeployment`** — never `CollectDeployment` or `RecallDeployment` (C6/D6: both credit the **pool** while the wave debits the **reserve**, so collecting one creates money across two ledgers). ⚠ Unsubscribe on component cleanup; `ScriptInvoker.Insert` does not de-duplicate.
  - File(s): `…/OVT_QRFControllerComponent.c`
  - Estimate: 1.5 h

- [x] **T4.7 Scoring verification — inspection, not new code**
  - Description: Confirm `CheckUpdatePoints` counts a mounted crew: registered under the occupying-faction key **and** `IsFightingFit` while seated. If a seated occupant is excluded, **record it and change nothing** — that fix belongs to `qrf`.
  - File(s): `docs/features/occupying/vehicles/context.md`, Init case
  - Estimate: 1.5 h

- [x] **T4.8 `OVT_TEST_Init_QRFMountedEchelon.c`**
  - Description: reachability gate refuses a land-isolated source; budget gate refuses a slice under the cheapest rung; the cap refuses the third echelon; the returned cost equals the rung cost; **the conserved-total case** — a wave with one echelon debits `m_iResources` exactly once, by exactly `spent`, and never touches `m_mFactionResources`.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_QRFMountedEchelon.c`
  - Estimate: 3 h

- [x] **T4.9 context.md**
  - Description: The reachability limitation **verbatim**, the four-parallel-array hazard, the delete-not-collect rule and why, and the T4.7 verdict.
  - File(s): `docs/features/occupying/vehicles/context.md`
  - Estimate: 0.75 h

- [x] **T4.10 Epic overview — reachability debt**
  - Description: Add the limitation as a second bullet under *Land reachability*.
  - File(s): `docs/features/occupying/epic-overview.md`
  - Estimate: 0.25 h

**Acceptance:** compile 0 ✅ (6336 files) · `grep -c "m_OccupyingFaction.m_iResources -=" …QRFControllerComponent.c` → **1** ✅ · `grep -n "CollectDeployment\|RecallDeployment"` in that file → **empty** ✅ (the two names are deliberately not spelt out even in prose, so the grep is a mechanical guard) · no `m_aSpawnQueue`/ring-slot-array writes inside `SendMountedEchelon` ✅ · `grep -rn "AddFactionResources" Scripts/Game/GameMode/Objectives/` → **empty** ✅ · insertion-module diff ⚠ **carries the observer repair, not one enum line** — G6/Q1 was broken deliberately and is documented in `context.md` → *"✅ ROOT CAUSE FIXED 2026-08-23"*; **nothing in Phase 4 touched that file**.

**⚠ Landed differently from the plan in four places — full reasoning in `context.md` → Phase 4:**
1. **§3.5's budget expression `allocate - allocated` is ZERO OR NEGATIVE in ordinary play** (the infantry loop exits on `allocated >= allocate`), and `OVT_VehicleLadderRules.RungAffordable` reads a **negative budget as UNBOUNDED** — so as written it would have bought the dearest rung in the registry exactly when the wave ran out of money. The slice is `m_iResourcesLeft - allocated`, clamped at zero.
2. **The budget gate is the config's VEHICLE CEILING, not the cheapest rung.** The module re-resolves the ladder at spawn time against `m_iTruckCostOverride` alone (D4), so pricing here against a smaller number would charge for a UAZ and field a BRDM. Both sides now ask the ladder the identical question.
3. **`GetNearestBase(source).landIsolated` is only trusted within 50 m of the source.** Two of the QRF's wave sources are not bases at all (the forward operating base, and the no-bases-left placeholder), and `GetNearestBase` has no radius.
4. **No analogous "mission behaviour" module is needed** (the question Phase 3's deadlock raised): the mobile checkpoint never calls `RequestDeploymentCollection`, no reinforcement or condition module is authored, and the config is not in `HARASSMENT_LADDER` — so **the QRF is the only thing that can ever take an echelon down**, which is exactly what D6 requires.

**T4.7 verdict:** 🟢 **zone scoring DOES count a mounted crew, and nothing was changed.** `IsFightingFit` asks only about life state and unconsciousness; a seated character's `GetOrigin()` is a **world** position (vanilla's own `SCR_SeizingComponent.c:302` counts seated occupants by exactly this reading); the crew never leaves the riding ring so it is always a materialised agent under the occupying key. ⚠ One caveat recorded rather than hidden: at `ECHELON_STANDOFF_M` 450 the crew is inside `QRF_RANGE` (750) — which denies the resistance its uncontested `+5` push — but **outside** `QRF_POINT_RANGE` (220), so it does not contest the centre until it moves in. The only edit T4.7 justified was making `IsFightingFit` **public**, as `CheckUpdatePoints` and `CheckUpdateTimer` in the same file already are.

---

## Phase 5: Crew-up on alarm (8/8 complete) ✅ — STANDARD

- [x] **T5.1 `ScriptInvoker<vector> m_OnBattleStarted`**
  - Description: On `OVT_OccupyingFactionManager`, published from `StartBaseQRF` (:1231) **and** `StartTownQRF` (:1278) at `m_vQRFLocation`, after the mode is configured and `Start()` has run. Document why it exists separately from `m_OnQRFTownChanged` (C9: that one is town-only and deliberately silent for base QRFs).
  - File(s): `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c`
  - Estimate: 1 h

- [x] **T5.2 `m_sVehicleRole` + `ReleaseVehicleOwnership` on the parked module**
  - Description: Empty role = today's `m_sVehicleType` path exactly (G8). `ReleaseVehicleOwnership(Vehicle)` so a handed-over hull is not deleted twice. ⚠ Add the new field to that module's `CloneModule()`.
  - File(s): `Scripts/Game/GameMode/Deployments/Modules/OVT_ParkedVehicleSpawningDeploymentModule.c`
  - Estimate: 1.5 h

- [x] **T5.3 Registry delta `"Base Parked Armour"`**
  - Description: A delta on `Deployment_BaseParkedVehicles.conf` **in the registry** — role `armed`, `m_iVehicleCount 1`, `m_iCostPerVehicle 120`, `m_eParkingType PARKING_TRUCK`, the crew-up behaviour module, `m_iMinimumThreatLevel 400`, `m_fChance 60`, `m_bFreeAtGameStart 0`. ⚠ The base `.conf` file itself stays byte-identical.
  - File(s): `Configs/Deployment/overthrowDeployments.conf`
  - Estimate: 1 h

- [x] **T5.4 `OVT_CrewUpOnAlarmBehaviorDeploymentModule`**
  - Description: Attributes `m_fAlarmRadius` 750, `m_sSortieConfigName "Base Armour Sortie"`, `m_iSortieBudget` 60. Subscribes to `m_OnBattleStarted` on activate; on a battle inside the radius resolves the parked vehicle, calls `ReleaseVehicleOwnership`, force-creates the sortie **from** this base with the hull handed in. ⚠ Unsubscribe in **both** `OnCleanup` and `OnDeactivate`; `Insert` does not de-duplicate.
  - File(s): `Scripts/Game/GameMode/Deployments/Modules/OVT_CrewUpOnAlarmBehaviorDeploymentModule.c`
  - Estimate: 2.5 h

- [x] **T5.5 `Deployment_BaseArmourSortie.conf`**
  - Description: `{6BC10…}`; mounted module with `m_bAdoptExistingVehicle 1`, a crew group, `m_fLZStandoffDistance 200`; mobile-checkpoint behaviour with `m_iRelocateMinutes 0` — the sortie parks in a defend posture **short of** the battle rather than driving into it. `m_bDirectorOnly 1`.
  - File(s): `Configs/Deployment/Deployment_BaseArmourSortie.conf(.meta)`, `overthrowDeployments.conf`
  - Estimate: 1 h

- [x] **T5.6 `vehicle_crew` group entries**
  - Description: Three men (driver, gunner, commander), cost 15, one entry per faction group registry, so an armed hull's gun is manned. ⚠ Both `truck_crew` entries stay **exactly** as they are — the insertion path is untouched.
  - File(s): `Configs/Factions/USSR_OverthrowData.conf`, `Configs/Factions/US_OverthrowData.conf`
  - Estimate: 0.75 h

- [x] **T5.7 Init cases**
  - Description: The parked module clones the new field; the crew-up module fires **exactly once** for a battle inside the radius and **not at all** for one outside; ownership transfer leaves exactly one owner (the parked module no longer reports the vehicle in `GetSpawnedEntities()` and the mounted one does).
  - File(s): `Scripts/Game/Tests/TestSuites/Init/`
  - Estimate: 2 h

- [x] **T5.8 context.md**
  - Description: The ownership-transfer contract, the double-delete hazard, and the invoker's two publication points.
  - File(s): `docs/features/occupying/vehicles/context.md`
  - Estimate: 0.5 h

**Acceptance:** compile 0 ✅ (6338 files) · `git diff Configs/Deployment/Deployment_BaseParkedVehicles.conf` **empty** ✅ · `grep -c "m_OnBattleStarted.Invoke" …OccupyingFactionManager.c` → **2** ✅ · `m_OnBattleStarted.Remove` present in **both** cleanup paths ✅ (written inline in each, not through a shared helper - see context.md defect 1).

**⚠ Landed differently from the plan in three places — full reasoning in `context.md` → Phase 5:**
1. **The unsubscribe acceptance grep is literal**, so `OnDeactivate()`/`OnCleanup()` each carry their own inline `m_OnBattleStarted.Remove(...)` rather than a shared `UnsubscribeFromBattles()` helper, which a `grep -n` would only find once.
2. **The sortie needs no standoff arithmetic in the crew-up module.** `m_fLZStandoffDistance 200` is authored on the mounted module itself and the raw battle location is passed straight through - computing a pre-standoff point here (mirroring the QRF echelon) would have pulled back twice.
3. **The mobile-checkpoint's approach band is a third, novel value (150-250 m)**, centred on the 200 m standoff - neither the harassment ladder's 150-300 (objective-centred) nor the echelon's 0-50 (standoff-is-the-position) assumption holds here.

One can-fail proof did **not** go red: reordering `ReleaseVehicleOwnership`/`AdoptVehicle` compiles clean and produces the same end state, because releasing early does not invalidate the `Vehicle` entity. The ordering is a read-verified correctness argument, not a tier-guarded one - recorded in full in context.md.

---

## Phase 6: Hunter-killer armoured sweep (6/6 complete) ✅ — STANDARD

- [x] **T6.1 `ReportVehicleLoss(vector position)`**
  - Description: Insert an `OVT_TargetData` through the same path the existing known-target inserts use (:2110-2191), deduplicating against `GetNearestKnownTarget`. Called from the mounted module's `HOLDING`/`DRIVING` vehicle-loss branch and from `ReleaseConvoy` when the vehicle was destroyed. ⚠ **Not** from teardown — a deployment collected normally has not lost anything.
  - File(s): `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c`, `…/OVT_InsertionSpawningDeploymentModule.c`
  - Estimate: 1.5 h

- [x] **T6.2 `OVT_ArmouredSweepBehaviorDeploymentModule`**
  - Description: Attributes `m_fSweepRadius` 400, `m_iSweepMinutes` 12, `m_fWaypointInterval` 90 s. Walks the vehicle around the hotspot on `Patrol`/`Defend` waypoints and calls `RequestDeploymentCollection("its sweep is over")` when the clock expires — the base behaviour module already owns collection, exfil holds and the player-watching veto (:295-380).
  - File(s): `Scripts/Game/GameMode/Deployments/Modules/OVT_ArmouredSweepBehaviorDeploymentModule.c`
  - Estimate: 2 h

- [x] **T6.3 `Deployment_HunterKillerSweep.conf`**
  - Description: `{6BC10…}`; mounted module (role `armed`, `m_iTruckCostOverride 90`, `m_iHoldTicks` matching the sweep clock, source `OVT_NearestControlledBaseSourceProvider`), the sweep behaviour, `m_bDirectorOnly 1`, `m_iMinimumThreatLevel 300`, `m_iBaseCost 60`.
  - File(s): `Configs/Deployment/Deployment_HunterKillerSweep.conf(.meta)`, `overthrowDeployments.conf`
  - Estimate: 1 h

- [x] **T6.4 `TickHunterKiller()` on the existing 60 s tick**
  - Description: The dispatcher the evaluator cannot be (C5 — `CollectSeedCandidates` only ever produces named-location positions). Pick the highest-scoring known target by `GetThreatByLocation`; refuse when a sweep is already live, when the score is under a floor, when the pool cannot afford it, or when a battle is engaged; otherwise `ForceCreateDeployment` **and debit the pool at that one call site** with `SubtractFactionResources`, exactly as the objective director does at `OVT_ObjectiveDirectorComponent.c:1225`.
  - File(s): `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c`
  - Estimate: 2.5 h

- [x] **T6.5 Init cases**
  - Description: The dispatcher refuses at each of its four gates; it spends **exactly once** and the pool falls by exactly the config cost; `ReportVehicleLoss` deduplicates a second loss at the same spot; the sweep module clones completely.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/`
  - Estimate: 2 h

- [x] **T6.6 context.md**
  - Description: Why the evaluator could not be used (C5), and the create-then-debit choke point restated at the new call site.
  - File(s): `docs/features/occupying/vehicles/context.md`
  - Estimate: 0.5 h

**Acceptance:** compile 0 · `grep -rn "AddFactionResources" Scripts/Game/GameMode/Objectives/` still **empty** · `SubtractFactionResources` count in the OF manager = pre-existing **+1** · `m_HunterKillerDeployment` is a single-slot field, not a list.

---

## Phase 7: Help, documentation and localization sync (5/5 complete) ✅ — `help-docs-sync`

- [x] **T7.1 Fact-check first**
  - Description: Every sentence written cites a `file:line`. Do **not** invent thresholds — the rungs and the difficulty spread are in `Configs/Factions/*.conf` and `Configs/Difficulty/*.conf`; the checkpoint band is `m_fApproachMinDistance`/`Max`; the echelon cap is a const in the QRF controller. (Two tips have shipped inventing mechanics before; no gate catches a well-formed lie.)
  - File(s): `docs/features/occupying/vehicles/context.md`
  - Estimate: 1 h

- [x] **T7.2 Field Manual section**
  - Description: The occupying faction's armour — it escalates with threat; a parked BTR at a base is a warning, not decoration; a checkpoint on the road into a contested town is deliberate and can be destroyed; AT is worth carrying from mid-campaign.
  - File(s): `Configs/FieldManual/`
  - Estimate: 1 h

- [x] **T7.3 Tutorial popup (at most one)**
  - Description: Fires the first time the player sees an occupying armed vehicle, pointing at the Field Manual page.
  - File(s): `Configs/Tutorials/`
  - Estimate: 0.75 h

- [x] **T7.4 `.st` master only**
  - Description: ~4 keys in `Language/localization_Overthrow.st`. ⚠ **Never edit `Configs/Language/*.conf`** — Workbench build output. Record the re-export as owed to the user.
  - File(s): `Language/localization_Overthrow.st`
  - Estimate: 0.5 h

- [x] **T7.5 Wiki draft + epic rollup**
  - Description: Occupying-faction page draft, plus epic-overview row 7, build order and rollup.
  - File(s): `docs/features/occupying/epic-overview.md`, wiki
  - Estimate: 1 h

**Acceptance:** ⚠ `git diff Configs/Language/` is **vacuous - that path does not exist in this tree** (it errors with *"no such path in the working tree"*); the real runtime exports are `Language/localization_Overthrow.<lang>.conf` and **this phase did not touch them** ✅ · every new sentence traceable to a `file:line` in context.md → *Phase 7 → T7.1 citation list*, and repeated inline in each `.st` Comment ✅ · epic overview carries **row 7** ✅ and the reachability follow-up (added by Phase 4 T4.10, **not duplicated here**) ✅.

**⚠ T7.3 was deliberately SKIPPED, not done:** no tutorial trigger exists for "the player has seen an occupying armed vehicle" and authoring one is new tutorial-framework capability (`OVT_TutorialEvent` is a closed 14-value catalog, `OVT_TutorialTrigger.c:12-44`). Reported as a gap; the Field Manual section is the deliverable and stands without it.

**⚠ Owed to the user:** a **localization re-export from Workbench** for the 4 new `.st` keys (`OVT-FieldManual_OccupyingForces_Head5`, `_Text8`, `_Text9`, `_Text10`), and **publication of `wiki-draft.md`** — no `wikijs` MCP server was attached to the Phase 7 session.

---

## Feature-close gate (after Phase 7)

- [x] ✅ **G-1 — `tools/compile-check.sh` exit 0** — re-confirmed by the orchestrator after Phase 7: **0**, 6340 files.
- [ ] 🔴 **G-2 — `tools/run-tests.sh "{6A6E2A002F53A581}"` (All group), run ONCE, alone — STILL OWED, THE FEATURE'S ONLY UNRUN GATE.** Deferred for the whole feature at the user's instruction (2026-08-23: Workbench testing in progress). **Consequence, stated plainly: every Init and Logic case written across all seven phases has been COMPILED BUT NEVER EXECUTED.** Nothing in this feature has been observed going green *or* red. ⚠ Must not overlap a Workbench or play-test session — that returns INDETERMINATE (exit 2), which is *no verdict*, not a pass. The orchestrator asked the user for a clear window and the Discord transport failed to deliver the question; running it unprompted would have both wrecked the session and produced no verdict, so it was not run.
- [x] ✅ **G-3 — DoD greps 4–12** run by the orchestrator after Phase 7. All pass:
  - ladder file world-identifier count → **0**
  - QRF reserve debit → **1**; `CollectDeployment|RecallDeployment` → **0**
  - `AddFactionResources` in `Scripts/Game/GameMode/Objectives/` → **0**
  - `RplProp|Rpc(` across all six new script files → **0**
  - `git diff` on persistence, UI, map, Virtualization, VirtualMovement → **empty**
  - `m_bDirectorOnly 1` → **1 in each of the four new configs**
  - `DropPassengersToGlobalRing` in the mounted module → **0**
  - `Language/localization_Overthrow.st` brace-balanced (2439/2439), 4 new keys present
  - ⚠ **DoD grep 10's `Configs/Language/` path DOES NOT EXIST in this tree** — that criterion is vacuous and always passed. The real runtime exports are `Language/localization_Overthrow.<lang>.conf`, and they were **not** touched.
  - 🔴 **DoD Q1 / G6 FAILS BY DESIGN.** `OVT_InsertionSpawningDeploymentModule.c` is no longer a one-appended-enum-value diff: the observer root-cause repair landed there deliberately, plus Phase 6's 13-line `ReportVehicleLoss` hook. **Re-state this criterion; do not "repair" it.**

---

## Play-test round 1 fixes (2026-08-23, after the feature closed its phases)

- [x] **P1.1 Ladder thresholds raised** — `m_iMinThreat` 0/400/900 → **400/900/1500** in both faction
  registries. `baseThreat` is 100 on Normal, so the bottom rung at 0 meant armed vehicles from minute one
  (log: *"role 'armed' at threat 152 resolved to 'light_armed'"*).
- [x] **P1.2 `m_bWalkWhenNoLadderRung`** (new attribute, defvalue 1) — a ladder miss returns an empty
  `ResourceName` instead of substituting `m_sTruckVehicleType`. Authored 1 on harassment / echelon /
  hunter-killer, 0 on the base armour sortie. Without it, "no vehicles below 400" would have shipped an
  unarmed Ural parked as a mobile checkpoint instead.
- [x] **P1.3 Radio towers are no longer hunter-killer targets** — `PickHunterKillerTarget()` skips
  `BROADCAST_TOWER`. The director's specops tower recapture is untouched.
- [x] **P1.4 Mounted forces carry crew only** — all four mounted configs to `m_sGroupType ""` / 0 / 0 /
  cost 0; the two vehicle patrols re-crewed from `light_fireteam` / `light_patrol` to `vehicle_crew`.
  Truck and insertion configs (`truck_crew`) deliberately untouched.
- [x] **P1.5 A crew-only force marks itself eliminated when its crew dies** — override of
  `DismountAndWalk()`; the crew-lost test is taken before `super` (see context.md).
- [x] **P1.6 `CanFieldLadderVehicle()` gates all three dispatchers** — hunter-killer, QRF echelon, and the
  director's mounted harassment rung, so nothing marches a crew on foot below the bottom rung.
- [x] **P1.7 Init cases retuned** to the new thresholds (`OVT_TEST_Init_VehicleLadderResolution` now asks
  400/900/1500; the `CrewUpOnAlarm` and `QRFMountedEchelon` fixtures ask at 100000 so they are
  threshold-independent). `compile-check.sh` exit 0, 6341 files.
- [x] **P1.8 Patrol vehicles no longer vanish in front of the player who killed the crew** — a
  `deployments` defect surfaced by P1.4. `OVT_MultiTownPatrolBehaviorDeploymentModule` now arms its
  teardown and polls `TickPatrolTeardown()` against the base class's existing exfiltration rule instead of
  calling `DeleteDeployment()` inline. See context.md → *Play-test round 1* item 4.
- [x] **P1.9 The occupying reserve now has a ceiling** — `ReserveTarget` / `PoolTransferForWindow` /
  `ReserveOverflow` on `OVT_BaseDefenseConversion`, a new `reserveTargetMultiplier` difficulty field across
  all six presets, and `ArmDefenseShareDrip()` wired to them. One new Logic case
  (`OVT_TEST_Logic_BaseDefenseConversion_ReserveCeiling`, 6 claims). See context.md → item 5, including the
  double-pay fault the first draft had.
- [x] **P1.10 Daylight window on the deployment evaluator** — `IsInDaylightWindow()` +
  `m_iDaylightStartHour 5` / `m_iDaylightEndHour 17` on the manager, `m_bIgnoreDaylightWindow` on the
  config, one filter line in `FindBestDeploymentConfig()`. Seeding, director operations and reinforcement
  rebuys are outside it by design. Fixed the Init fixture the test world's 21:17 clock would have broken.
- [x] **P1.11 Reinforcement cap** — `m_iMaxReinforcements` on the reinforcement module (0 = no limit),
  authored 3 on the town patrol, cloned, counted per rebuy pass, not reset by reactivation. One new Init
  case. See context.md → item 7.
- [x] **P1.12 Finished the removal of the per-decision rebuy diagnostic** — it was a half-finished deletion,
  not a defect. `DescribeReinforcementDecision()`, `m_iLastDecision`, the `REBUY_DECISION_*` constants, both
  latch blocks and the unused `IsBattleSuppressed()` wrapper are gone. See context.md → item 8.
- [x] **P1.13 An empty dedicated server now halts** — player-count gates added to
  `OVT_VirtualMovementManagerComponent.MovementTick()` and `OVT_DeploymentComponent.UpdateDeployment()`,
  the two systems that kept running with nobody online. See context.md → item 9 for why resuming is safe
  and why the autotest world is unaffected.
- [x] **P2.1 The stuck horn** — a mounted force never cleared its outbound move order on arrival, so the
  crew kept trying to reach a point it was already at. Affected every mounted doctrine; only visible on the
  QRF echelon because the mobile checkpoint's `DetachForeignWaypoints()` was masking it wherever
  `m_iRelocateMinutes > 0`. See context.md → *Play-test round 2* A.
- [x] **P2.2 The QRF echelon pushes into the battle** — `ECHELON_STANDOFF_M` 450 → 0. `COUNTER_ATTACK`'s
  siege-ring anchor and the harassment config's 250 m are untouched. See *Play-test round 2* B.
- [ ] 🔴 **P2.3 OPEN — AI drivers hold the horn on clear road while MOVING.** Not the P2.1 fault (those
  were parked). Engine-side: nothing in Overthrow or vanilla script ever calls `SetVehicleHorn`. Lead is the
  `AICarMovementComponent` delta (Ural/BRDM-2/LAV-25); the decisive observation is whether an undelta'd hull
  does it too. No mitigation by author's decision. See context.md → *Play-test round 2*.
- [x] **P2.4 Per-deployment cooldown** — `m_fCooldownHours` on the config (0 = none), enforced and stamped
  in `CreateDeployment()`, seeding exempt, int-minute clock. Authored 6 h on tower recapture, and 3 h / 6 h
  on the light / heavy vehicle patrols (**my call** — the author said "might"). See context.md.
- [x] **P2.5 The horn lead is disproven** — a `UAZ469_PKM` carries none of Overthrow's
  `AICarMovementComponent` tuning. General Reforger AI driving, still open, still unmitigated.
- [ ] 🔴 **P1.14 No suite run and no play-test on any of the above.** Seventeen changes deep now, across
  `vehicles`, `deployments` and the resource economy. Same deferral as the rest of the
  feature — and the four config changes in P1.4 are the largest behavioural change in the list.

---

## Needs Human Verification

*(populated as phases close)*

- ⏸️ **Workbench loads** (implementation.md §6 checks 13–16): both faction `.conf`s resolve with the two new fields; `overthrowDeployments.conf` shows **27** entries; both objective plans show a **five**-rung harassment ladder; both prefab deltas resolve against their vanilla parents with no duplicated `ActionsManagerComponent`.
- ⏸️ **Solo play-test**, §6 steps 1–12. Debug affordances: `/give-resources`, a raised time multiplier, and a **temporary** `vehicleThresholdScale 0.05` in `Difficulty_Normal.conf` — ⚠ **revert before committing**, the DoD greps require a clean diff.
- ⏸️ **MP / dedicated-server pass**, §6 step 13. The automated spine covers MP not at all.
- ⏸️ **Localization re-export from Workbench** after Phase 7 - 4 new `.st` keys: `OVT-FieldManual_OccupyingForces_Head5`, `_Text8`, `_Text9`, `_Text10`. Until then they render as raw keys in game.
- ⏸️ **Wiki publication** of `docs/features/occupying/vehicles/wiki-draft.md` (proposed path `enemy-armour`). No `wikijs` MCP server was attached to the Phase 7 session. ⚠ Search before creating - a section on an existing occupying-faction page is preferred.
