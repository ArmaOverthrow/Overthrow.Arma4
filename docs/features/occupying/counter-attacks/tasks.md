# Counter Attacks - Task Checklist

**Last Updated:** 2026-08-20 (**CLOSED** — all 10 phases built + play-tested, **All 385/385 green**; superseded by `occupying/objectives`)
**Progress:** 137/137 complete (100%). **CLOSED 2026-08-20.** This feature is the proof of concept for `occupying/objectives`, which takes over every director script, config, test and serializer. The 12 items open at closure (T10.3 wiki sync, the Easy-interval tuning decision, and 10 human-verification play-test items) are ticked as **closed out, not done**: `objectives` re-runs the same F-criteria against its rewritten runner and owns any localization re-export or wiki sync it needs. Nothing else is owed here.

> **Agent routing:** phases **2, 3, 4, 6, 7, 8, 9** are **ADVANCED** (`component-developer-advanced`); phases **1** and **5** are STANDARD (`component-developer`); phase **10** is `help-docs-sync`. Phase 8's `.layout` slice goes to `ui-developer`.
> **Suite per phase:** 1–9 → **All** `{6A6E2A002F53A581}` (every one of them touches campaign/economy/persistence state); 10 → **skipped** (docs-only).
> **Source of truth:** `implementation.md` §4 and the Agent Routing Summary. §3.9 is the authority for Phase 9.
> **Frozen neighbours, every phase:** `git diff Scripts/Game/GameMode/Virtualization/ Scripts/Game/GameMode/VirtualMovement/ docs/features/virtualization/core/api.md` must stay **empty**.

---

## Phase 1: Retirement and the difficulty rewire (9/9 complete) — STANDARD

- [x] **T1.1 Read-only survey first (gates the phase)**
  - Description: Record in context.md every reader of `m_bCounterAttackTimeout` (expect 3) and `counterAttackTimeout` (expect 2); every caller of `StartBaseQRF` (expect 2) and `StartTownQRF` (expect 2). A new caller means the design is re-checked before editing.
  - File(s): `docs/features/occupying/counter-attacks/context.md`
  - Estimate: 0.5 h

- [x] **T1.2 Delete the hourly random counter-attack roll**
  - Description: `OVT_OccupyingFactionManager.c:1418-1432` including the unconditional `RandFloat01()` at `:1419`; delete `m_bCounterAttackTimeout` (`:176`) and its decrement (`:1351-1352`).
  - File(s): `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c`
  - Estimate: 0.5 h

- [x] **T1.3 Delete the town-suppression QRF**
  - Description: `:1453-1469` only. ⚠ Threat decay at `:1443-1451` lives in the same block and stays; the two dead locals go with the loop.
  - File(s): `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c`
  - Estimate: 0.5 h

- [x] **T1.4 Retire `counterAttackTimeout` and its four authored values**
  - Description: `OVT_DifficultySettings.c:50-51` plus `Difficulty_Normal.conf:10`, `Hard.conf:15`, `Extreme.conf:15`, `Insane.conf:14` — same commit, or every load warns.
  - File(s): `Scripts/Game/Configuration/OVT_DifficultySettings.c`, `Configs/Difficulty/*.conf`
  - Estimate: 0.5 h

- [x] **T1.5 Author the twelve new difficulty fields**
  - Description: Per §3.6, `Occupying Faction` category, actionable `desc:` on each; per-preset values in all five shipped presets; `Difficulty_TestWorld.conf` authors none.
  - File(s): `Scripts/Game/Configuration/OVT_DifficultySettings.c`, `Configs/Difficulty/*.conf`
  - Estimate: 1.5 h

- [x] **T1.6 Re-word the two comments this invalidates**
  - Description: `:1411-1412` (the 20 % reserve is no longer a counter-attack reserve). `:1394-1397` stays as-is.
  - File(s): `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c`
  - Estimate: 0.25 h

- [x] **T1.7 Logic-tier: `OVT_TEST_Logic_ObjectiveScaling.c` (new)**
  - Description: `RequiredSabotageMissions` clamps 0/negative/absurd to the fallback; the harassment ramp saturates at the top rung. World-free including comments.
  - File(s): `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_ObjectiveScaling.c` (new)
  - Estimate: 1 h

- [x] **T1.8 Init-tier: the difficulty inversion**
  - Description: Five shipped presets load and `objectiveSabotageMissionsRequired` is non-increasing Easy → Insane with at least one strict step (G11).
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c`
  - Estimate: 1 h

- [x] **T1.9 context.md: survey verdicts + the passivity note**
  - Description: T1.1 verdicts, and an explicit note that the OF has **no offensive trigger at all** between this phase and Phase 8 — accepted, v1.5 unreleased.
  - File(s): `docs/features/occupying/counter-attacks/context.md`
  - Estimate: 0.25 h

---

## Phase 2: The director — state machine, selection, persistence — GATE for phases 5–8 (13/13 complete) — ADVANCED

- [x] **T2.1 The pure statics first**
  - Description: `OVT_ObjectiveSelection` (Score/Select/Blacklist/DecayBlacklist/ApplyAnchorBias) and `OVT_ObjectivePhaseRules` (all eight predicates). Hard-rule headers; no manager/world/entity/`OVT_Global` identifier, comments included.
  - File(s): `Scripts/Game/GameMode/Objectives/OVT_ObjectiveSelection.c`, `.../OVT_ObjectivePhaseRules.c` (new)
  - Estimate: 3 h

- [x] **T2.2 Records and enums**
  - Description: `OVT_EObjectiveKind`, `OVT_EObjectivePhase`, `OVT_ObjectiveRecord`, `OVT_ObjectiveFOBRecord`, `OVT_ObjectiveBlacklistEntry`. ⚠ Enum integers ride the GM snapshot from Phase 8 — never renumber.
  - File(s): `Scripts/Game/GameMode/Objectives/` (new)
  - Estimate: 1 h

- [x] **T2.3 `OVT_ObjectiveDirectorComponent`**
  - Description: `OVT_Component`, server-only `OnPostInit`, static `GetInstance()`, `Init`/`PostGameStart` wired **last** in the game-mode chain, `OVT_Global.GetObjectiveDirector()`, declared on the game-mode prefab.
  - File(s): `Scripts/Game/GameMode/Objectives/OVT_ObjectiveDirectorComponent.c` (new), `OVT_OverthrowGameMode.c`, `OVT_Global.c`, `Prefabs/GameMode/OVT_OverthrowGameMode.et`
  - Estimate: 2.5 h

- [x] **T2.4 The tick and the freeze**
  - Description: One `CallLater` at `OF_UPDATE_FREQUENCY / timeMul`; the three early returns of §3.3 in order; the QRF freeze comment naming the singleton contract. All timers are tick counters (D4).
  - File(s): `Scripts/Game/GameMode/Objectives/OVT_ObjectiveDirectorComponent.c`
  - Estimate: 1.5 h

- [x] **T2.5 Selection**
  - Description: Enumerate resistance-held bases and non-village towns, gather the six inputs of §3.4, score, pick, enter HARASSMENT, log winner **and runner-up with both scores**. No candidates → IDLE, logged once.
  - File(s): `Scripts/Game/GameMode/Objectives/OVT_ObjectiveDirectorComponent.c`
  - Estimate: 2.5 h

- [x] **T2.6 Two pure-read helpers on the OF manager**
  - Description: `GetBasesControlledBy(int)` and `GetRadioTowersAffecting(vector)` (honouring `IsDisabled()`); re-point the duplicated inline loop at `OVT_TownManagerComponent.c:275-285` or record why not.
  - File(s): `OVT_OccupyingFactionManager.c`, `OVT_TownManagerComponent.c`
  - Estimate: 1.5 h

- [x] **T2.7 Re-selection triggers (flag only, never inline)**
  - Description: Subscribe `m_OnBaseControlChanged` and `m_OnTownControlChange`; both handlers set `m_bReselectPending` and return (D3 — the base invoker fires before the affiliation is applied). `Remove` then `Insert`; unsubscribe on cleanup.
  - File(s): `Scripts/Game/GameMode/Objectives/OVT_ObjectiveDirectorComponent.c`
  - Estimate: 1.5 h

- [x] **T2.8 The one reset path**
  - Description: Clear anchor, delete this objective's deployments, clear the FOB record, optionally blacklist, go IDLE, log the reason. One method called by every failure and every ending.
  - File(s): `Scripts/Game/GameMode/Objectives/OVT_ObjectiveDirectorComponent.c`
  - Estimate: 1.5 h

- [x] **T2.9 `OVT_ObjectiveDirectorSerializer`**
  - Description: Version-first, positional, append-only, pure codec per §3.8; registered in the game-mode `ComponentSerializers` block with a fresh GUID; `ApplyPersistedObjective` touches no pool and no deployment.
  - File(s): `Scripts/Game/Persistence/Serializers/Components/OVT_ObjectiveDirectorSerializer.c` (new), `Configs/Systems/Persistence/Overthrow.conf`
  - Estimate: 2.5 h

- [x] **T2.10 Logic-tier: selection, gates, blacklist, starvation**
  - Description: Extends T1.7's file — highest score wins, ties by input order, all-blacklisted and empty select nothing, `DecayBlacklist` floors at zero, every gate on both sides, `IsFOBStarved` on each input independently.
  - File(s): `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_ObjectiveScaling.c`
  - Estimate: 2 h

- [x] **T2.11 Init-tier: resolve, idle, deterministic pick, frozen tick**
  - Description: Resolves via `GetInstance()` and `OVT_Global`; fresh director is IDLE; driven selection is deterministic; the tick early-returns while `m_CurrentQRF` is set and decrements nothing. ⚠ Init worlds never run `PostGameStart` — install the tick in the case.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c`
  - Estimate: 2 h

- [x] **T2.12 Persistence-tier: the objective round trip**
  - Description: Kind, position, phase, both counters, all three tick counters and a two-entry blacklist survive save → dirty → re-apply, through the public API only. 🔴 Do not widen the reload seam.
  - File(s): `Scripts/Game/Tests/TestSuites/Persistence/OVT_TEST_PersistenceRoundTripSuite.c`
  - Estimate: 2 h

- [x] **T2.13 context.md: hard-rule note, helper verdicts, ordering trap, load order**
  - File(s): `docs/features/occupying/counter-attacks/context.md`
  - Estimate: 0.5 h

---

## Phase 3: The objective anchor in the deployment evaluator (7/7 complete) — ADVANCED

- [x] **T3.1 Read-only survey**
  - Description: Every writer of the candidate score (expect one at `EvaluateFactionDeployments:615-627`); confirm `OVT_CandidatePosition.sortBy` has exactly one producer.
  - File(s): `docs/features/occupying/counter-attacks/context.md`
  - Estimate: 0.5 h

- [x] **T3.2 The anchor store and its API**
  - Description: `OVT_DeploymentObjectiveAnchor` (position, radius, weight), `m_mObjectiveAnchors`, `SetObjectiveAnchor`, `ClearObjectiveAnchor`; cleared on faction-list teardown.
  - File(s): `Scripts/Game/GameMode/Deployments/OVT_DeploymentManager.c`
  - Estimate: 1 h

- [x] **T3.3 One line in the scoring loop**
  - Description: Call `OVT_ObjectiveSelection.ApplyAnchorBias`; doc block stating both invariants — no anchor is byte-identical to today, and the anchor biases **ordering, never eligibility**.
  - File(s): `Scripts/Game/GameMode/Deployments/OVT_DeploymentManager.c`
  - Estimate: 0.5 h

- [x] **T3.4 The director pushes the anchor**
  - Description: Set on selection and on every phase change (harassment ~600 m, FOB/QRF ~1 200 m); cleared in the T2.8 reset. One place.
  - File(s): `Scripts/Game/GameMode/Objectives/OVT_ObjectiveDirectorComponent.c`
  - Estimate: 1 h

- [x] **T3.5 Logic-tier: `ApplyAnchorBias`**
  - Description: Unchanged for `radius <= 0`, `weight <= 0`, `distance >= radius`; monotonic in distance; bounded by `score + weight`; never below `score`; the ordering claim as two candidates.
  - File(s): `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_ObjectiveAnchorAndBearing.c` (new)
  - Estimate: 1 h

- [x] **T3.6 Init-tier: no anchor is unchanged, an anchor reorders**
  - Description: Two consecutive no-anchor evaluations produce the same ordering; with an anchor, an in-radius candidate outsorts an equal-threat out-of-radius one. Scope assertions to your own fixture positions.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_ObjectiveAnchor.c` (new — the Phase 2 pattern, not the monolith)
  - Estimate: 1.5 h

- [x] **T3.7 context.md: T3.1 verdict + the "ordering, not eligibility" invariant**
  - File(s): `docs/features/occupying/counter-attacks/context.md`
  - Estimate: 0.25 h

---

## Phase 4: The insertion module (10/10 complete) — ADVANCED

- [x] **T4.1 `OVT_InsertionGeometry` (pure)**
  - Description: `ShouldWalk`, `LZPointOnLine` (clamped — never past the target), `IsStuck`, `HasArrived`. Hard-rule header.
  - File(s): `Scripts/Game/GameMode/Deployments/OVT_InsertionGeometry.c` (new)
  - Estimate: 1.5 h

- [x] **T4.2 `OVT_DeploymentSourceProvider` + nearest-base implementation**
  - Description: `[BaseContainerProps()]`, one virtual `ResolveSource(...)` returning **false** rather than a zero vector, plus `GetProviderName()`; the `OVT_DeploymentPlacementProvider` shape exactly.
  - File(s): `Scripts/Game/GameMode/Deployments/Modules/` (new)
  - Estimate: 1.5 h

- [x] **T4.3 Concurrency cap on the deployment manager**
  - Description: `m_iMaxConcurrentInsertions` (default 2, authored from difficulty), reserve/release/reset. ⚠ Runtime-only; zeroed on restore and teardown; **every** exit path releases.
  - File(s): `Scripts/Game/GameMode/Deployments/OVT_DeploymentManager.c`
  - Estimate: 2 h

- [x] **T4.4 `OVT_InsertionSpawningDeploymentModule`**
  - Description: Per §3.6. ⚠ `GetOnAgentAdded()` passes ONE arg (recover via `agent.GetParentGroup()`); `Insert` does not de-dup; crew keeps `spawnDistanceOverride = 100000` for the whole drive, passengers drop to default only after dismount.
  - File(s): `Scripts/Game/GameMode/Deployments/Modules/OVT_InsertionSpawningDeploymentModule.c` (new)
  - Estimate: 5 h

- [x] **T4.5 The walk fallback, written first and never optional**
  - Description: Below threshold, refused reservation, stuck truck, destroyed truck, missing prefab — the groups still exist and still arrive on foot.
  - File(s): `Scripts/Game/GameMode/Deployments/Modules/OVT_InsertionSpawningDeploymentModule.c`
  - Estimate: 2 h

- [x] **T4.6 `truck_crew` and `specops_team` registry entries, both factions**
  - Description: Appended, fresh grep-verified GUID prefix; record in context.md which prefab each resolves to and its provenance.
  - File(s): `Configs/Factions/*_OverthrowData.conf`
  - Estimate: 1 h

- [x] **T4.7 Logic-tier: `OVT_TEST_Logic_ObjectiveInsertion.c` (new)**
  - Description: Every geometry function including source == target, standoff ≥ separation, zero-length line, negative thresholds, and stuck-independence inside the arrival radius.
  - File(s): `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_ObjectiveInsertion.c` (new)
  - Estimate: 1.5 h

- [x] **T4.8 Init-tier: registry resolution, provider, clone fidelity, cap**
  - Description: Both registry names resolve for **both** factions; provider returns false with no friendly base; the module's clone carries every own **and inherited** attribute; the reservation counter refuses past the cap.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_ObjectiveInsertion.c` (new — kept out of the shared suite file, matching Phases 2 and 3)
  - Estimate: 2 h

- [x] **T4.9 Fixture discipline audit**
  - Description: Any fixture constructing this module must be `SetSpawnedUnitsEliminated(true)` on the deployment **and every spawning module** before it can tick. Re-run `grep -rn "RegisterGroup(" Scripts/Game/Tests/` and record a verdict per site.
  - File(s): `Scripts/Game/Tests/`
  - Estimate: 1 h

- [x] **T4.10 context.md: prefab choices, release-path audit, no-director-dependency note**
  - File(s): `docs/features/occupying/counter-attacks/context.md`
  - Estimate: 0.5 h

---

## Phase 5: Phase 1 town operations — harassment and tower recapture (10/10 complete) — STANDARD

- [x] **T5.1 `OVT_ObjectiveConditionDeploymentModule`**
  - Description: Both evaluations answer the same question, deliberately — header says why (an objective operation *should* be collected when the objective moves).
  - File(s): `Scripts/Game/GameMode/Deployments/Modules/OVT_ObjectiveConditionDeploymentModule.c` (new)
  - Estimate: 1 h

- [x] **T5.2 The stacking support debuff**
  - Description: ⚠ **Append to the END of `supportModifiers.conf`** — `m_iIndex` is positional and travels in replicated per-town lists. Ship `OVT_ObjectiveHarassmentSupportModifier` with an empty `OnTick` because `PostInit` dereferences `config.handler`.
  - File(s): `Configs/Modifiers/supportModifiers.conf`, `Scripts/Game/GameMode/Systems/Modifiers/` (new)
  - Estimate: 1.5 h

- [x] **T5.3 `OVT_TownHarassmentBehaviorDeploymentModule`**
  - Description: `EvaluateHold` split out of `OnUpdate` so it is assertable without a live marker; `m_bFired` latch **not** copied by `CloneModule`.
  - File(s): `Scripts/Game/GameMode/Deployments/Modules/` (new)
  - Estimate: 2.5 h

- [x] **T5.4 `OVT_TowerRecaptureBehaviorDeploymentModule`**
  - Description: Same shape plus the 600 s hold timer that `EvaluateCapture` does not have (C5); flips via `ChangeRadioTowerControl`, which already notifies.
  - File(s): `Scripts/Game/GameMode/Deployments/Modules/` (new)
  - Estimate: 2.5 h

- [x] **T5.5 Author `Deployment_ObjectiveHarassment.conf`**
  - Description: ⚠ Module order is update order and `.conf` files cannot carry comments: spawning, behaviour, reinforcement last among behaviour, then conditions. `m_iAllowedLocationTypes TOWN`.
  - File(s): `Configs/Deployment/Deployment_ObjectiveHarassment.conf` (new)
  - Estimate: 1 h

- [x] **T5.6 Author `Deployment_ObjectiveTowerRecapture.conf`**
  - Description: `specops_team` insertion, recapture behaviour **before** reinforcement, `OVT_RadioTowerControlConditionDeploymentModule` with `m_bRequireControl 0`, objective condition, `RADIO_TOWER`.
  - File(s): `Configs/Deployment/Deployment_ObjectiveTowerRecapture.conf` (new)
  - Estimate: 1 h

- [x] **T5.7 The ramp mechanism decision**
  - Description: Prefer **thin registry variant configs** per rung (the `Deployment_BaseHeavyPatrol` precedent) over inventing a per-create override. Decide, author, record.
  - File(s): `Configs/Deployment/`, `docs/features/occupying/counter-attacks/context.md`
  - Estimate: 1.5 h

- [x] **T5.8 Director wiring: `TickHarassment()`**
  - Description: One operation per `objectiveHarassmentIntervalMinutes` up to the concurrency cap, rung from `HarassmentLadderIndex`; a recapture operation per resistance-held affecting tower; **`SubtractFactionResources` immediately after every successful create** (G5). `OnHarassmentSuccess()` increments and re-checks the Phase 2 gate.
  - File(s): `Scripts/Game/GameMode/Objectives/OVT_ObjectiveDirectorComponent.c`
  - Estimate: 2.5 h

- [x] **T5.9 Init-tier: configs, plans, fire-once, clone fidelity, modifier index**
  - Description: Both configs resolve and validate; the harassment plan is movable; `EvaluateHold`/`EvaluateRecapture` fire once and never twice; every new module's clone is complete; the new modifier is **last** in its config.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c`
  - Estimate: 2 h

- [x] **T5.10 context.md: the append rule, the T5.7 decision, the rung table**
  - File(s): `docs/features/occupying/counter-attacks/context.md`
  - Estimate: 0.5 h

---

## Phase 6: Phase 1 base operations — sabotage (9/9 complete) — ADVANCED

- [x] **T6.1 Read-only survey — GATES the phase, nothing is deleted until it exists**
  - Description: The removal path and its two load-bearing lines in order; 🔴 the owner-or-officer check that rejects a server call (decide: `playerId == -1` bypass **or** a shared helper); the cost join that does not exist yet; the enumerator that does not exist yet (`CountItemsForLocation` shape, collecting instead of counting).
  - File(s): `docs/features/occupying/counter-attacks/context.md`
  - Estimate: 2 h

- [x] **T6.2 Reuse the single removal path**
  - Description: Navmesh carve captured **before** the entity goes. Do not copy the two lines into the module.
  - File(s): `Scripts/Game/GameMode/Managers/Factions/OVT_ResistanceFactionManager.c`
  - Estimate: 1.5 h

- [x] **T6.3 `OVT_BaseSabotageBehaviorDeploymentModule`**
  - Description: Enumerate → filter to this base + player faction → order **ascending by cost** → destroy one per hold interval → stop at the per-mission cap → report success. ⚠ Cost is the only ordering key that exists (there is no `m_iSize`).
  - File(s): `Scripts/Game/GameMode/Deployments/Modules/` (new)
  - Estimate: 3 h

- [x] **T6.4 One notification per mission, not per structure**
  - Description: `SendTextNotification("ObjectiveSabotage", -1, baseName)`; preset in `Configs/overthrowBroadcastMessages.conf` with a fresh GUID and `.st` keys. Deliberate addition beyond the letter of the requirements, justified by the legibility bar.
  - File(s): `Configs/overthrowBroadcastMessages.conf`, `Language/localization_Overthrow.st`
  - Estimate: 1 h

- [x] **T6.5 Author `Deployment_ObjectiveSabotage.conf`**
  - Description: `specops_team` insertion, sabotage behaviour, reinforcement with `m_bDeleteOnConditionFail 1`, `OVT_BaseControlConditionDeploymentModule` with `m_bRequireControl 0`, objective condition, `BASE`.
  - File(s): `Configs/Deployment/Deployment_ObjectiveSabotage.conf` (new)
  - Estimate: 1 h

- [x] **T6.6 Director wiring for a BASE objective**
  - Description: Same cadence and the same pool debit; `OnSabotageSuccess()` increments and re-checks the Phase 2 gate.
  - File(s): `Scripts/Game/GameMode/Objectives/OVT_ObjectiveDirectorComponent.c`
  - Estimate: 1.5 h

- [x] **T6.7 Logic-tier: `NextTargetIndex`**
  - Description: Cheapest remaining; `-1` on empty/all-destroyed/ragged; ties by input order; a negative cost sorts first without crashing.
  - File(s): `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_ObjectiveScaling.c`
  - Estimate: 1 h

- [x] **T6.8 Init-tier: config, filter, hold gate, per-mission cap**
  - Description: Excludes another base's structures and occupying-owned ones; destroys **nothing** while an enemy is inside the clear radius; the cap is respected.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c`
  - Estimate: 1.5 h

- [x] **T6.9 context.md: the T6.1 table, the removal decision, what the player permanently loses**
  - File(s): `docs/features/occupying/counter-attacks/context.md`
  - Estimate: 0.5 h

---

## Phase 7: Phase 2 — the FOB (14/14 complete) — ADVANCED (largest phase)

- [x] **T7.1 The authored marker, in the `OVT_SniperPosition` shape**
  - Description: Component + entity + prefab. ⚠ Per-frame Workbench viz is `Shape.CreateArrow` only — the `CreateLines` family is banned (it crashed Workbench twice). ⚠ Author **no** world-layer instances this phase.
  - File(s): `Scripts/Game/Components/`, `Scripts/Game/Entities/`, `Prefabs/GameMode/OVT_FOBPosition.et` (new)
  - Estimate: 2 h

- [x] **T7.2 `Prefabs/Bases/OVT_OccupyingFOB.et`**
  - Description: Faction flag, small camp composition, the dismantle action's owner entity. Hand-authored GUID.
  - File(s): `Prefabs/Bases/OVT_OccupyingFOB.et` (new)
  - Estimate: 2 h

- [x] **T7.3 Site selection — generated path first**
  - Description: `OVT_FOBSiting` (pure) + the director's world work: bounded-attempt generation on the source→objective line, ocean check, `TraceBox` clearance, road preference; **then** authored markers, which win when they exist. ⚠ The attempt bound is a named constant with its reason.
  - File(s): `Scripts/Game/GameMode/Objectives/OVT_FOBSiting.c` (new), `OVT_ObjectiveDirectorComponent.c`
  - Estimate: 4 h

- [x] **T7.4 No site → blacklist one round and reselect, logged at WARNING**
  - File(s): `Scripts/Game/GameMode/Objectives/OVT_ObjectiveDirectorComponent.c`
  - Estimate: 0.5 h

- [x] **T7.5 `OVT_FOBRaiseSpawningDeploymentModule`**
  - Description: ⚠ `WasRestoredFromSave()` gates the raise — without it a campaign grows one more FOB per load (D11 / R2).
  - File(s): `Scripts/Game/GameMode/Deployments/Modules/` (new)
  - Estimate: 3 h

- [x] **T7.6 Author both FOB configs + `OVT_ObjectiveAnchorSourceProvider`**
  - Description: `Deployment_ObjectiveFOB.conf` (raise + reinforcement + objective condition) and `Deployment_ObjectiveFOBGarrison.conf` (insertion from the anchor provider, count capped by `objectiveFOBGarrisonMax`, DEFEND). The provider prefers the FOB and falls through to the nearest held base.
  - File(s): `Configs/Deployment/` (new ×2), `Scripts/Game/GameMode/Deployments/Modules/` (new)
  - Estimate: 2.5 h

- [x] **T7.7 The budget ceiling**
  - Description: `m_iFOBSpent`; every create checks pool **and** `WithinFOBCeiling` before `ForceCreateDeployment`, then debits and increments. ⚠ The director never holds money — say so in the record header.
  - File(s): `Scripts/Game/GameMode/Objectives/`
  - Estimate: 1.5 h

- [x] **T7.8 Starvation**
  - Description: Three inputs each tick → `IsFOBStarved` → tick the counter → tear down at `objectiveStarvationMinutes`; recovery zeroes it; log both transitions.
  - File(s): `Scripts/Game/GameMode/Objectives/OVT_ObjectiveDirectorComponent.c`
  - Estimate: 1.5 h

- [x] **T7.9 Removal: `OVT_DismantleEnemyFOBAction` + one new server-validated verb**
  - Description: `OVT_CaptureBaseAction` shape plus `OVT_UndeployFOBAction`'s request hop. ⚠ Hold duration is authored in the prefab (`Duration 15`), not script. ⚠ The server re-validates everything (BUG-025 is this epic's headline debt). ⚠ `Rpc()` arity is a compile blind spot — add an Init seam case.
  - File(s): `Scripts/Game/UserActions/` (new), `OVT_CampaignRequestComponent.c`, `Prefabs/Bases/OVT_OccupyingFOB.et`
  - Estimate: 3 h

- [x] **T7.10 Teardown — one path for all three exits**
  - Description: Starvation, player removal, QRF resolution. Delete both deployments and the structure, zero `m_iFOBSpent`, release reservations, clear the anchor, apply the penalty **only** on the player-initiated exit, reset.
  - File(s): `Scripts/Game/GameMode/Objectives/OVT_ObjectiveDirectorComponent.c`
  - Estimate: 1.5 h

- [x] **T7.11 Logic-tier: `OVT_FOBSiting` + `WithinFOBCeiling`**
  - Description: Degenerate band (`min >= max`), empty exclusion list, ragged exclusion/radius pair, boundary candidate; ceiling at, below and above.
  - File(s): `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_ObjectiveScaling.c`
  - Estimate: 1 h

- [x] **T7.12 Init-tier: configs, restore-gates-raise, provider preference, clean teardown**
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c`
  - Estimate: 2 h

- [x] **T7.13 Persistence-tier: the FOB round trip and its lazy re-link**
  - Description: Every FOB field survives; the director re-links by name+position on its first tick; a payload naming a dead deployment **resets** the objective rather than stranding it.
  - File(s): `Scripts/Game/Tests/TestSuites/Persistence/OVT_TEST_PersistenceRoundTripSuite.c`
  - Estimate: 1.5 h

- [x] **T7.14 context.md: siting constants, the never-holds-money rule, the three exits**
  - File(s): `docs/features/occupying/counter-attacks/context.md`
  - Estimate: 0.5 h

---

## Phase 8: Phase 3 — the counter-QRF, the daylight gate, and the GM panel (13/14 complete) — ADVANCED

- [x] **T8.1 Read-only survey: re-verify C4 still holds**
  - Description: The LZ globals gone, the trace fixed, the wrap fixed. If a concurrent session re-introduced any, the phase's design changes.
  - File(s): `docs/features/occupying/counter-attacks/context.md`
  - Estimate: 0.5 h

- [x] **T8.2 The FOB as a wave source**
  - Description: Insert after the `m_Bases` loop and **before** the empty-list fallback, with the same >20 m guard. ⚠ `m_Bases` is never refreshed mid-battle — that is left alone deliberately and gets a context.md note.
  - File(s): `Scripts/Game/Controllers/OccupyingFaction/OVT_QRFControllerComponent.c`
  - Estimate: 1 h

- [x] **T8.3 Bearing bias**
  - Description: `PreferredDegreesFromSource(source, target)` (pure) in the 0° = North = `-Z` convention; `GetLandingZone(vector sourcePos)`. ⚠ **The sign backwards puts every wave on the far side** — the phase's most likely defect.
  - File(s): `Scripts/Game/Controllers/OccupyingFaction/`, `OVT_QRFControllerComponent.c`
  - Estimate: 2 h

- [x] **T8.4 Director wiring: fire the QRF, poll its end**
  - Description: `StartBaseQRF`/`StartTownQRF` on the gate; observe the end by **polling `m_CurrentQRF` going null** on the director's tick — never a second `m_OnFinished` subscriber (D8: the manager deletes the entity inside the dispatch).
  - File(s): `Scripts/Game/GameMode/Objectives/OVT_ObjectiveDirectorComponent.c`
  - Estimate: 1.5 h

- [x] **T8.5 Reset whatever the outcome**
  - Description: On the tick after the QRF clears: tear the FOB down, reset, reselect. Win and loss take the same path.
  - File(s): `Scripts/Game/GameMode/Objectives/OVT_ObjectiveDirectorComponent.c`
  - Estimate: 0.5 h

- [x] **T8.6 The GM record: a NEW additive pair**
  - Description: `SendCampaignObjective` + `RpcDo_CampaignObjective`; bump `CAMPAIGN_RECORD_COUNT` 2 → 3 **and** `WIRE_VERSION` 1 → 2. ⚠ Every send site needs its `ShouldRespondLocally` branch. ⚠ `Rpc()` arity is a compile blind spot (BUG-090) — arity-diff by eye and cover it in the seam test.
  - File(s): `Scripts/Game/Components/Controller/OVT_GMRequestComponent.c`
  - Estimate: 2 h

- [x] **T8.7 The client store and its three-method trap**
  - Description: `m_sObjectiveName` + `m_iObjectivePhase` on `OVT_GMCampaignState`. ⚠ Add to `CopyFrom`, `CopyRecords` (check) **and** `Clear` — a field missing from `Clear` leaks the previous campaign's objective into the next.
  - File(s): `Scripts/Game/GameMode/GM/`
  - Estimate: 1 h

- [x] **T8.8 The builder stays read-only**
  - Description: The director exposes **pure getters**; arithmetic goes in a pure static, never in a getter with a side effect.
  - File(s): `Scripts/Game/GameMode/GM/OVT_GMSnapshotBuilder.c`
  - Estimate: 0.5 h

- [x] **T8.9 The panel: two rows in `CampaignSection`** — built by the `ui-developer` slice 2026-08-19 (63 additive lines; `Row_Objective`/`Row_Phase` copied verbatim from `Row_Threat`). ⚠ Layouts are not parsed by any automated gate — **needs a human look in Workbench**, see the human-verification list
  - Description: `Row_Objective` / `Label_Objective` / `Value_Objective` and `Row_Phase` / `Label_Phase` / `Value_Phase` copying `Row_Threat`; two `FindText` in `CacheWidgets`, two `SetText` in `RenderAll`. ⚠ No RPC/`RplProp` may appear in `OVT_GMPanelUIComponent`. **`.layout` authoring is a `ui-developer` slice.**
  - ✅ **Already shipped by Phase 8 and ready to consume:** `OVT_GMCampaignState.m_sObjectiveName` / `.m_iObjectivePhase`, `OVT_GMPanelFormat.FormatObjectiveName(string)` / `.FormatObjectivePhase(int)`, and **all eight `.st` keys** (`OVT-GMPanel_Objective`, `OVT-GMPanel_ObjectivePhase`, `OVT-GMPanel_ObjectiveNone`, `…PhaseNone`, `…PhaseHarassment`, `…PhaseForwardBase`, `…PhaseCounterAttack`, `…PhaseUnknown`). ⚠ Use `SetText`, not `SetTextFormat`: a `#`-key is resolved by `SetText` and a town name must never go through a format string. **No `.st` editing is owed by this slice.**
  - File(s): `UI/Layouts/GM/GMPanel.layout`, `Scripts/Game/UI/GM/OVT_GMPanelUIComponent.c`
  - Estimate: 1 h

- [x] **T8.10 Logic-tier: bearing + panel formatter**
  - Description: All four cardinals and both diagonals with tolerance (⚠ `vector.Distance` is +1 ULP at 1 000/2 000 m); coincident source returns a defined value, not NaN; the formatter for every phase and for no objective.
  - File(s): `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_ObjectiveAnchorAndBearing.c`
  - Estimate: 1.5 h

- [x] **T8.11 Init-tier: GM seam, state fields, `Clear()`, gate fires once**
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_GMRequestSeam.c`, `OVT_TEST_InitSuite.c`
  - Estimate: 1.5 h

- [x] **T8.12 context.md: T8.1 verdict, the bearing sign argument in full, `m_Bases` staleness, the two wire bumps**
  - File(s): `docs/features/occupying/counter-attacks/context.md`
  - Estimate: 0.5 h

- [x] **T8.13 The daylight conjunct (D17)**
  - Description: `IsCounterAttackWindow(hour, start, end)` — half-open, **handles wrap** — as a conjunct on both Phase 3 gates; hour read the way the OF manager already reads it. Consts on the director, **not** in `OVT_DifficultySettings`. ⚠ A clock-only block logs **once per objective** and ticks no starvation/timeout counter.
  - File(s): `Scripts/Game/GameMode/Objectives/OVT_ObjectivePhaseRules.c`, `OVT_ObjectiveDirectorComponent.c`
  - Estimate: 1 h

- [x] **T8.14 Logic-tier: the window predicate**
  - Description: Inside, both boundaries (05:00 in, 15:00 out), outside, midnight, and a wrapping window (22 → 4) on both sides of both edges.
  - File(s): `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_ObjectiveScaling.c`
  - Estimate: 0.5 h

---

## Phase 9: The counter-attack QRF mode — the silent siege (16/16 complete) — ADVANCED

> Authority: `implementation.md` §3.9. Read it before writing anything.

- [x] **T9.1 Read-only survey + a written standard-mode baseline**
  - Description: Re-verify the eleven anchors §3.9 cites. **Then write down, before changing anything, what a standard QRF does second by second** — T9.11 checks the finished code against it. This is R18's primary defence.
  - File(s): `docs/features/occupying/counter-attacks/context.md`
  - Estimate: 1 h

- [x] **T9.2 The enums and the mode field**
  - Description: `OVT_EQRFMode { STANDARD, COUNTER_ATTACK }`, `OVT_EQRFStage { SILENT_DEPLOY, MUSTER, BATTLE }`, `m_eMode` (defaults STANDARD), `m_eStage`, `IsEngaged()`. ⚠ Mode is set by the caller **before** `Start()`, following the existing `SpawnQRFController` → configure → `Start()` order.
  - File(s): `Scripts/Game/Controllers/OccupyingFaction/` (new enum file), `OVT_QRFControllerComponent.c`
  - Estimate: 1 h

- [x] **T9.3 `OVT_QRFSiege`, the pure static**
  - Description: `RingSlotBearing`, `RingSlotOffset`, `ShouldPublishTimer`, `FormatMusterRemaining`'s numeric half, `AllNeutralised`. No world/entity/manager/`OVT_Global` — **comments included** (the Logic rule is a directory-wide grep).
  - File(s): `Scripts/Game/Controllers/OccupyingFaction/OVT_QRFSiege.c` (new)
  - Estimate: 1.5 h

- [x] **T9.4 The single-pass spend**
  - Description: Cycle the source list until the budget is gone; skip the follow-up wave `CallLater`. ⚠ The debit at `:345-352` must run **exactly once** for the pass. ⚠ Bound the loop with an iteration counter as well as the budget.
  - File(s): `Scripts/Game/Controllers/OccupyingFaction/OVT_QRFControllerComponent.c`
  - Estimate: 2 h

- [x] **T9.5 The ring**
  - Description: One slot per queued group, computed **once from the final queue length** before the first spawn; radius rolled in [100, 150]; ocean-rejected slots re-roll inward. ⚠ The spawn arrays are index-parallel and `SpawnFromQueue` removes index 0 from each — a fourth array must be removed in the same place.
  - File(s): `Scripts/Game/Controllers/OccupyingFaction/OVT_QRFControllerComponent.c`
  - Estimate: 2 h

- [x] **T9.6 Orders**
  - Description: Counter-attack groups get one `Defend` waypoint on their slot instead of the Scout/Scout/SaD/SaD ladder; at the BATTLE transition **remove it** and add `SearchAndDestroy` on the centre. ⚠ `AddWaypoint` appends.
  - File(s): `Scripts/Game/Controllers/OccupyingFaction/OVT_QRFControllerComponent.c`
  - Estimate: 1.5 h

- [x] **T9.7 The stage machine in `CheckUpdateTimer`**
  - Description: Exactly as §3.9 spells it out. ⚠ The `m_iTimer < 105000` spawn condition is a standard-mode expression of the 15 s despawn wait; SILENT_DEPLOY needs its own drain condition and **does not need the wait**.
  - File(s): `Scripts/Game/Controllers/OccupyingFaction/OVT_QRFControllerComponent.c`
  - Estimate: 2 h

- [x] **T9.8 The early end**
  - Description: On the 10 s cadence, MUSTER only. 🔴 **Zero agents with a live entity counts as ALIVE**; `AllNeutralised(0,0)` is false. Put the reason in a comment at the test site.
  - File(s): `Scripts/Game/Controllers/OccupyingFaction/OVT_QRFControllerComponent.c`
  - Estimate: 1.5 h

- [x] **T9.9 The reveal, on the manager**
  - Description: `m_bQRFRevealed` + `RpcDo_SetQRFRevealed(bool)`; true at creation for STANDARD, at the MUSTER transition for COUNTER_ATTACK via `RevealQRF()`. ⚠ Arity-diff the new RPC pair by eye (BUG-090). ⚠ Add the flag to the JIP payload; note that `m_iCurrentQRFBase/Town` are already missing from it and that is **not** this phase's to fix.
  - File(s): `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c`
  - Estimate: 2 h

- [x] **T9.10 `IsQRFEngaged()` and the three world gates**
  - Description: Move the economy tick (`:1418`), the deployment evaluator (`OVT_DeploymentManager.c:577`) and the first civilian invoke (`:1162`) onto it. ⚠ The civilian invoke is a **paired transition** — assert a siege always passes through BATTLE.
  - File(s): `OVT_OccupyingFactionManager.c`, `OVT_DeploymentManager.c`
  - Estimate: 1.5 h

- [x] **T9.11 The client conjuncts, and the standard-path proof**
  - Description: One conjunct each in `OVT_EconomyInfo.c:79`, `OVT_MapRestrictedAreas.c:327`, `OVT_FastTravelService.c:108`, `OVT_RespawnService.c:220`. Leave `OVT_SleepService` and `OVT_GMPanelUIComponent` on `m_bQRFActive`. Then re-read the T9.1 baseline and confirm line by line.
  - File(s): four client files
  - Estimate: 1.5 h

- [x] **T9.12 The HUD's minutes form**
  - Description: Minutes above 120 s via a new `#OVT-BattleStartsInMinutes`, seconds below. ⚠ **Only `Language/localization_Overthrow.st` may be edited** — the `.conf` exports are build output; a re-export is owed and must be reported.
  - File(s): `Scripts/Game/UI/HUD/OVT_EconomyInfo.c`, `Language/localization_Overthrow.st`
  - Estimate: 1 h

- [x] **T9.13 Notifications**
  - Description: `CounterAttackBase` / `CounterAttackTown` in `Configs/overthrowBroadcastMessages.conf` + `.st` entries, sent from `RevealQRF()` through both `SendTextNotification` and `SendExternalNotifications`. Cities use the town tag; villages are never objectives.
  - File(s): `Configs/overthrowBroadcastMessages.conf`, `Language/localization_Overthrow.st`, `OVT_OccupyingFactionManager.c`
  - Estimate: 1 h

- [x] **T9.14 Logic-tier: `OVT_TEST_Logic_QRFSiege.c` (new)**
  - Description: Ring bearings for 1/2/3/12; the **sign** convention asserted explicitly; publish predicate at the 120 s boundary; minutes/seconds crossover at exactly 120 000 ms (minutes round **up**); `AllNeutralised(0,0)` false plus the three other cases.
  - File(s): `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_QRFSiege.c` (new)
  - Estimate: 1.5 h

- [x] **T9.15 Init-tier: engagement, stage advance, early-end safety, reveal defaults**
  - Description: ⚠ Fixture groups eliminated-marked before anything ticks.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c`
  - Estimate: 1.5 h

- [x] **T9.16 context.md: baseline note, flag table, the zero-agent decision, the parallel-array trap, no-siege-persistence, owed re-export**
  - File(s): `docs/features/occupying/counter-attacks/context.md`
  - Estimate: 0.5 h

---

## Phase 10: Help & documentation sync (3/4 complete) — `help-docs-sync`

> Suite: **skipped — docs-only.**

- [x] **T10.1 Fact-check every existing sentence about counter-attacks, QRFs and defending**
  - Description: Cite a `file:line` or cut the sentence. The project has shipped invented mechanics twice; no gate catches a well-formed lie.
  - File(s): `Configs/Tutorials/`, `Configs/FieldManual/`
  - Estimate: 1 h

- [x] **T10.2 Document the player-visible changes**
  - Description: The full list in `implementation.md` Phase 10 T10.2 — including the siege, the 30-minute window, the 100–150 m ring, the early-wipe win and the daylight rule.
  - File(s): `Configs/Tutorials/`, `Configs/FieldManual/`, `Language/localization_Overthrow.st`
  - Estimate: 1.5 h

- [x] **[closed out 2026-08-20 — dropped at closure, superseded by `occupying/objectives`]** ⏸️ **T10.3 Wiki sync — BLOCKED, no wikijs MCP server attached to this session.** Ready-to-paste content for all four pages is in the Phase 10 session note of `context.md`
  - Description: The same points plus the operator notes — the twelve new difficulty fields, the removal of `counterAttackTimeout`, and the shared deployment pool.
  - File(s): wikijs MCP
  - Estimate: 1 h
  - ⚠ **BLOCKED 2026-08-19: the wikijs MCP tools were not available in the Phase 10 session** (no `mcp__wikijs__*` tool exposed at all, so not even `wikijs_connection_status` could be asked). Nothing was written to the wiki and nothing was faked. The ready-to-paste page content and the target paths are in the Phase 10 session note in `context.md`.

- [x] **T10.4 Epic bookkeeping**
  - Description: Add `counter-attacks` to the epic feature table, refresh the epic Tech Debt section, update the epic's row in `docs/overview.md`.
  - File(s): `docs/features/occupying/epic-overview.md`, `docs/overview.md`
  - Estimate: 0.5 h

---

## Play-test fixes (2026-08-19, after all 10 phases were built)

Defects found by the author play-testing, not by the suites. All fixed, all green at **All 383/383**.
These were not planned tasks — they are recorded here so the count reflects the work that exists.

- [x] ✅ **World Editor crash on world load** — six unguarded `GetGame().GetFactionManager()` dereferences in `OVT_OverthrowConfigComponent`; the director's `OnPostInit` was the first caller to reach one before the manager exists. Guarded all six; construction no longer does anchor work. **Lesson: `OnPostInit` runs in the World Editor, where the game's managers do not exist.**
- [x] ✅ **QRF waypoints buried in / floating above terrain** — every waypoint carried the objective entity's Y. Clamped at `CreateWaypoint()`, the one funnel all QRF waypoints are born through. Pre-existing.
- [x] ✅ **Deployments materialising inside a live battle** — 750 m suppression on `IsQRFEngaged()`, occupying faction only, Manual lifecycle policy rather than despawn.
- [x] ✅ **`/give-resources` admin command** + the Workbench-SP-only guard on the whole admin command class (`RplSession.Mode() == RplMode.None` inside `#ifndef WORKBENCH`)
- [x] ✅ **`/give-resources` did not distribute** — now runs `TransferDefenseShareToPool` immediately, through the sanctioned path
- [x] ✅ **`/tick-resources` admin command** — credits exactly one resource tick's worth, computed from the same `PredictResourceGain()` seam the GM panel's "Next Distribution" reads, then distributes it through the same path as `/give-resources`
- [x] ✅ **The insertion transport carried the flipped-vehicle bug** — `GetAngles()` into an `AnglesToMatrix` parameter; the same defect `main` was being fixed for the same day
- [x] ✅ **Authored `OVT_VehiclePatrolSpawn` markers now used for insertion spawns**, nearest free first, occupancy reusing the BUG-129 spot test
- [x] ✅ **The idle-timeout trio** — phase timeout became an idle clock, an in-flight operation holds it, a recalled operation is refunded through the framework (Q6 amended with its reason)
- [x] ✅ **The forward-base phase re-sited forever, silently** — affordability refusal masked by a per-objective latch; latch is now per (config, reason), and the 24-point lattice no longer runs before the pay check
- [x] ✅ **The evaluator was buying the director's operations** — new `m_bDirectorOnly` flag on all eight objective configs
- [x] ✅ **Ammoboxes spared from sabotage** (author's call, pending gear recovery)
- [x] ✅ **The forward base flew a US flag** — the prefab inherited the plain vanilla pole; the flag now resolves from the occupying-faction setting
- [x] ✅ **The objective RESERVE FLOOR (D18)** — routine garrisoning can no longer drain a credit and starve the director
- [x] ✅ **🔴 The Phase-3 deadlock** — Phase 1 operations now continue through the forward-base phase (inclusive phase range). Before this, **the counter-attack was unreachable for either objective kind.**
- [x] ✅ **🔴 The Phase-2 LIVELOCK (the deadlock fix's own side-effect)** — a forward base refused for money fell through to a cheaper Phase-1 operation, which spent the pool and overwrote the reserve floor with its own price. The base was never affordable, so it was never raised. The chain now stops on an affordability refusal of the FOB
- [x] ✅ **Intact groups are refunded on a successful operation** — new `CollectDeployment()` pays back `m_iCostPerGroup` per group that came through at FULL strength; the transport and the config's base cost are deliberately not refunded
- [x] ✅ **Forward-base siting widened** — lateral spread 250 → 400 m, lanes 3 → 5, with the offset mapping normalised so the outermost lane equals the spread whatever the lane count
- [x] ✅ **The forward base ignored its authored yaw** — the facing was never read at all; generated sites now face the objective rather than north
- [x] ✅ **Stranded insertion trucks accumulated** — bounded cleanup (~20 real min), player-proximity hold, steal-it-and-it's-yours veto intact
- [x] ✅ **Sabotage targets buildables only** — placeables (5–250) always undercut the 750 buildable floor, so cheapest-first could never run the designed ladder
- [x] ✅ **Insertion arrival gated on speed as well as distance** — hard braking was injuring passengers at the drop-off
- [x] ✅ **...and then the drop was ~20 s late** — the speed gate was reading a 10 s tick average, which cannot answer "has it stopped yet". Arrival now reads the transport's live physics velocity; the stall test keeps the average
- [x] ✅ **The second crewman rode in the back** — seating is per-materialisation, so passengers won the cab seats by arriving first. The co-driver's seat is now claimed explicitly for the crew (he is the man who dismounts to open gates) and the force fills the bed first
- [x] ✅ **The transport drove home to the base centre** — the return leg targeted `m_vSource` (the base) rather than the spawn it left from; it now returns to its own `OVT_VehiclePatrolSpawn` marker
- [x] ✅ **Balance: `objectiveQRFResourceGate` = maxQRF floored at 750; `FOB_CEILING_MULTIPLIER` 3 → 4** (author-authorised)
- [x] **[closed out 2026-08-20 — dropped at closure, superseded by `occupying/objectives`]** ❓ **OPEN: `objectiveHarassmentIntervalMinutes` on Easy** — 90 in-game minutes makes the full ramp 90+ real minutes. Recommended 20–30 for the testing period. **Author's call, not yet made.**

---

## Needs human verification

Populated as phases complete — items the automated spine cannot cover.

- [x] **[closed out 2026-08-20 — dropped at closure, superseded by `occupying/objectives`]** **F17 — a player-initiated QRF still behaves exactly as it does today** (Phase 9's primary regression risk; a green suite does not cover it). Notification immediately, a **seconds** countdown from 120, first groups landing ~17 s in one per second, waves 4–8 min apart, scoring the instant it hits zero, economy/deployments/civilians suppressed from the start
- [x] **[closed out 2026-08-20 — dropped at closure, superseded by `occupying/objectives`]** **A siege reads as an encirclement** — nothing at all during `SILENT_DEPLOY` (no notification, no HUD panel, no map circle, the town still full of civilians), then the announcement, then groups 100–150 m out on every side
- [x] **[closed out 2026-08-20 — dropped at closure, superseded by `occupying/objectives`]** **The world really is still alive during `SILENT_DEPLOY`** — deployments still being created, the six-hourly income still landing
- [x] **[closed out 2026-08-20 — dropped at closure, superseded by `occupying/objectives`]** **Fast travel and respawn still work during `SILENT_DEPLOY`** and start refusing the moment the announcement lands
- [x] **[closed out 2026-08-20 — dropped at closure, superseded by `occupying/objectives`]** **The HUD's minutes form** — "30 min" counting down and crossing to a bare seconds count at two minutes
- [x] **[closed out 2026-08-20 — dropped at closure, superseded by `occupying/objectives`]** **Wiping the ring before the clock runs out** resolves the battle to the resistance instead of waiting the window out
- [x] **[closed out 2026-08-20 — dropped at closure, superseded by `occupying/objectives`]** MP / dedicated-server pass: the GM objective record, the FOB dismantle request, and the counter-attack reveal over the wire — including a client joining **during** `SILENT_DEPLOY` (no panel, no circle) and one joining during `MUSTER` (both)
- [x] **[closed out 2026-08-20 — dropped at closure, superseded by `occupying/objectives`]** Ramp pacing: whether the twelve difficulty values feel right over several in-game days
- [x] **[closed out 2026-08-20 — dropped at closure, superseded by `occupying/objectives`]** Whether an FOB site looks sensible, and whether the siege ring reads as an encirclement
- [x] **[closed out 2026-08-20 — dropped at closure, superseded by `occupying/objectives`]** Save → quit → **Continue** with an objective and an FOB live

---

*Scaffolded by `/autorun-feature` on 2026-08-19 from `implementation.md` §4. Add new tasks here when the plan grows.*
