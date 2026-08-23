# Objectives - Task Checklist

**Last Updated:** 2026-08-23
**Progress:** 74/74 tasks complete (100%)

> **Agent routing:** phases **2, 3, 4, 5, 6** are **ADVANCED** (`component-developer-advanced`); phases **1** and **7** are STANDARD (`component-developer`, with any `.layout` slice to `ui-developer`); phase **8** is `help-docs-sync`.
> **Suite per phase:** 1–7 → **All** `{6A6E2A002F53A581}` (every one touches campaign/persistence state); 8 → **skipped** (docs + localization only).
> **Source of truth:** `implementation.md` §4, with §3 for the contracts and the Agent Routing Summary for tiers. `requirements.md` stays authoritative on scope; C1–C8 in `implementation.md` override it where they differ.
> **Strangler rule:** every phase from 2 onward leaves the machine running. A director method is deleted **in the phase that replaces it**, never in a later cleanup phase.
> **Frozen neighbours, every phase:** `git diff Scripts/Game/GameMode/Virtualization/ Scripts/Game/GameMode/VirtualMovement/ docs/features/virtualization/core/api.md Configs/Difficulty/` must stay **empty**. `Scripts/Game/Controllers/OccupyingFaction/` is frozen **behaviourally** — its only permitted diff for the whole feature is the two keyed-API call sites in `OVT_QRFControllerComponent.c` that T1.3 required (`IsFOBUp`→`IsAssetUp(ASSET_FOB)`). ⚠ `implementation.md` §6 item 10 lists that folder as byte-frozen; that is an internal inconsistency with T1.3 and this carve-out is the resolution.
> **GUID prefix:** `{6BA1....}` — re-grep `grep -rl "6BA10000" Configs/ Prefabs/ Scripts/` before authoring.

---

## Phase 1: Relocation and the generic asset API (8/8 complete) ✅ — STANDARD

> **Gate: GREEN.** `compile-check.sh` 0; **All 416/417**. The single red (`CompositionSlotGate_AcceptedTypesMatchTheCompositions`) is a pre-existing v1.5 leftover, unrelated. Workbench check + play-test still owed — see "Needs Human Verification".

- [x] ✅ **T1.1 Read-only survey (gates the phase)**
  - Description: Re-verify every `IsFOBUp`/`GetFOBPosition` call site and the wider director read-API consumer list from implementation.md §4 Phase 1. A new caller means stop and re-check. Record the verdict.
  - File(s): `docs/features/occupying/objectives/context.md`
  - Estimate: 1 h

- [x] ✅ **T1.2 The asset record and the generic API**
  - Description: `OVT_ObjectiveAssetRecord` + `map<string, ref OVT_ObjectiveAssetRecord>` on the director; ship `IsAssetUp(string)` / `GetAssetPosition(string)` server-authoritative; **delete** `IsFOBUp()` / `GetFOBPosition()`. Keep the "forward-base state does not replicate" header note.
  - File(s): `Scripts/Game/GameMode/Objectives/OVT_ObjectiveDirectorComponent.c`, `OVT_ObjectiveRecords.c`
  - Estimate: 1.5 h

- [x] ✅ **T1.3 Move every production consumer to the keyed API**
  - Description: `ASSET_FOB` key constant so the literal appears once; QRF controller, anchor source provider, serializer, game mode, `OVT_Global`, GM request/panel. `OVT_DismantleEnemyFOBAction`'s two-entry-point rule unchanged.
  - File(s): `OVT_QRFControllerComponent.c`, `OVT_ObjectiveAnchorSourceProvider.c`, `OVT_ObjectiveDirectorSerializer.c`, `OVT_OverthrowGameMode.c`, `OVT_Global.c`, GM files
  - Estimate: 1.5 h

- [x] ✅ **T1.4 Move the test consumers (renames only)**
  - Description: `OVT_TEST_Init_CampaignRequestSeam.c`, `OVT_TEST_Init_ObjectiveDirector.c`, `OVT_TEST_Init_ObjectiveFOB.c`, `OVT_TEST_PersistenceRoundTripSuite.c`, `OVT_TEST_Logic_GMPanelFormat.c` prose. No assertion changes meaning.
  - File(s): `Scripts/Game/Tests/TestSuites/**`
  - Estimate: 1 h

- [x] ✅ **T1.5 `git mv` the repair module out to Deployments**
  - Description: `OVT_BaseRepairBehaviorDeploymentModule.c` → `Scripts/Game/GameMode/Deployments/Modules/`. **No content change.**
  - File(s): `Scripts/Game/GameMode/{Objectives,Deployments}/Modules/OVT_BaseRepairBehaviorDeploymentModule.c`
  - Estimate: 0.25 h

- [x] ✅ **T1.6 Rename the repair config file, never its config name**
  - Description: `Deployment_ObjectiveRepair.conf` → `Deployment_BaseRepair.conf` **with its `.conf.meta` GUID byte-identical**; edit the path half of the registry reference. ⚠ `m_sDeploymentName "Base Repair Detail"` is the persistence key (C8) and is NOT touched.
  - File(s): `Configs/Deployment/Deployment_BaseRepair.conf(.meta)`, `Configs/Deployment/overthrowDeployments.conf`
  - Estimate: 0.5 h

- [x] ✅ **T1.7 Re-point `OVT_TEST_Init_ObjectiveRepair.c`**
  - Description: 4 cases, 535 L, at the new paths; assertions unchanged (they are the proof the move was behaviour-free). Consider renaming to `OVT_TEST_Init_BaseRepair.c`.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_ObjectiveRepair.c`
  - Estimate: 0.75 h

- [x] ✅ **T1.8 context.md: T1.1 verdicts + the rename rule**
  - Description: File name is a hint, GUID is identity, `m_sDeploymentName` is fatal. Record explicitly that this phase's correctness is **not** covered by the Init tier and name the three substitute checks.
  - File(s): `docs/features/occupying/objectives/context.md`
  - Estimate: 0.5 h

---

## Phase 2: The objective framework and the strangler seam (15/15 complete) ✅ — **ADVANCED — GATE for phases 3–7**

> **Gate: GREEN.** `compile-check.sh` 0 (6216 files); **All `{6A6E2A002F53A581}` 417/418**, re-run after the fix.
> The single red, `OVT_TEST_Init_CompositionSlotGate_AcceptedTypesMatchTheCompositions`, is the pre-existing
> `core/damage` leftover — Phase 2 touched no composition or slot-gate code.
>
> 🔴 **This is the first run in the feature that actually tested this worktree.** Every earlier verdict, Phase 1's
> included, loaded the sibling `Overthrow.Arma4` checkout — see the two 2026-08-21 sections in `context.md` for the
> cause, the tooling fix, and the junction + `OVERTHROW_GAME_ADDONS_DIRS` recipe every future run depends on.
>
> The first honest run found two real regressions, both one bug: **a save context keys properties by the LOCAL
> VARIABLE'S NAME, not by position**, so version 2 read into `readTargetKind` what it had written as `targetKind`
> and restored every field as zero. Fixed — the serializer now writes one `array<ref OVT_PersistedObjective>` and
> checks every `Read()`. ⚠ This invalidates the positional premise of `implementation.md` §3.8; the record's
> content is unchanged.
>
> The registry `.conf` and the prefab wire are proven to load in the real client (case A), and the 44 existing
> objective Init cases passed **unedited** — the parity gate, at the Init tier, on real evidence. Only the parity
> play-test remains owed.

- [x] ✅ **T2.1 `OVT_ObjectivePlanRules` pure statics first**
  - Description: `ResolveWithDifficulty`, `ResolvePlanScore`, `SelectBestPlanIndex`, `AllConditionsMet`, `AnyAbort`, `PhaseIndexOf` (→ `-1`). ⚠ No manager/world/entity/`OVT_Global` identifier, **comments included**. ⚠ `out`/`owned` are reserved local names.
  - File(s): `Scripts/Game/GameMode/Objectives/OVT_ObjectivePlanRules.c`
  - Estimate: 2 h

- [x] ✅ **T2.2 The data classes**
  - Description: `OVT_ObjectiveRegistry : ScriptAndConfig` (`configRoot: true`, custom title field, `FindConfigByName`) in the `OVT_DeploymentRegistry.c:1-28` shape; `OVT_ObjectiveConfig`; `OVT_ObjectivePhase`. Every attribute carries an actionable `desc:`.
  - File(s): `Scripts/Game/GameMode/Objectives/OVT_ObjectiveRegistry.c`, `OVT_ObjectiveConfig.c`, `OVT_ObjectivePhase.c`
  - Estimate: 2.5 h

- [x] ✅ **T2.3 `OVT_ObjectiveInstance : Managed`**
  - Description: bag, vector bag, asset map, runtime module array; `Report/Get/Set/SetPos/GetPos/GetAsset`. ⚠ Strong `ref` on every Managed member; `array<ref …>`.
  - File(s): `Scripts/Game/GameMode/Objectives/OVT_ObjectiveInstance.c`
  - Estimate: 2 h

- [x] ✅ **T2.4 The four module base classes**
  - Description: Per §3.3, each mirroring `OVT_BaseDeploymentModule`. `CloneModule()` per concrete class; `CopyTo` is **not** the pattern (C7). Header states the dropped-line hazard verbatim and names the Init case that catches it.
  - File(s): `Scripts/Game/GameMode/Objectives/Modules/OVT_BaseObjective{Condition,Operation,Abort}Module.c` + base
  - Estimate: 2.5 h

- [x] ✅ **T2.5 `OVT_ObjectiveTargetResolver` base**
  - Description: `GetResolverName()`, in the `OVT_DeploymentSourceProvider.c:28-37` shape; empty array is the refusal, nothing cached across calls. No concrete resolvers yet.
  - File(s): `Scripts/Game/GameMode/Objectives/Resolvers/OVT_ObjectiveTargetResolver.c`
  - Estimate: 1 h

- [x] ✅ **T2.6 The two legacy shim classes**
  - Description: Per §3.7. Headers state in CAPITALS that they are temporary, which phase deletes each, and that **no new code may call them**.
  - File(s): `Scripts/Game/GameMode/Objectives/Modules/OVT_LegacyPhase*.c`
  - Estimate: 2 h

- [x] ✅ **T2.7 The runner rework**
  - Description: `m_aInstances` + `m_iMaxConcurrentObjectives` (default 1). `DirectorTick()` keeps its three early returns and restore-resolve **verbatim**; the phase `switch` becomes the instance loop of §3.2. `EnterPhase(name)` clones modules, `Exit()` outgoing / `Initialize()` incoming, re-arms via `SetPhaseTimeout()` (the progress re-baseline half is load-bearing), pushes the anchor.
  - File(s): `Scripts/Game/GameMode/Objectives/OVT_ObjectiveDirectorComponent.c`
  - Estimate: 5 h

- [x] ✅ **T2.8 The registry on the game-mode prefab**
  - Description: `[Attribute] ref OVT_ObjectiveRegistry m_Registry` on the director; author it on `Prefabs/GameMode/OVT_OverthrowGameMode.et:51` in exactly the deployment-registry form at `:8-11`.
  - File(s): `Prefabs/GameMode/OVT_OverthrowGameMode.et`
  - Estimate: 0.5 h

- [x] ✅ **T2.9 The two plans, authored in full with shim phases**
  - Description: `Configs/Objective/{overthrowObjectives,Objective_TownOffensive,Objective_BaseOffensive}.conf`. Real names; phases `Harassment`, `ForwardBase`, `CounterAttack`; real cadences and anchor radii as `-1`; bags = the shim pair. ⚠ `.conf` cannot carry comments; **module order is evaluation order**.
  - File(s): `Configs/Objective/*.conf` (+ `.meta`)
  - Estimate: 2 h

- [x] ✅ **T2.10 The validator and its call site**
  - Description: Phase-2 rules per §3.9 **plus** a real `PostGameStart()` call site (C6 — the deployment one is dead code). A skipped plan is named once at ERROR and never selected.
  - File(s): `Scripts/Game/GameMode/Objectives/OVT_ObjectiveRegistry.c`, `OVT_ObjectiveDirectorComponent.c`
  - Estimate: 2 h

- [x] ✅ **T2.11 Serializer v2**
  - Description: Per §3.8 — version-first, positional, append-only, three version outcomes each with its own log line. ⚠ Rewrite the "never renumber the enum" header on `OVT_ObjectiveRecords.c:1-13`; with names in the payload the constraint is **dead** (D2).
  - File(s): `Scripts/Game/GameMode/Objectives/OVT_ObjectiveDirectorSerializer.c`, `OVT_ObjectiveRecords.c`
  - Estimate: 3 h

- [x] ✅ **T2.12 Logic-tier: `OVT_TEST_Logic_ObjectivePlanRules.c` (new)**
  - Description: `ResolveWithDifficulty` (-1/0/sane/absurd); `SelectBestPlanIndex` (highest, ties by input order, all-ineligible, empty); `AllConditionsMet([])` = **true**; `AnyAbort([])` = **false**; `PhaseIndexOf` → -1 twice. Floats via `OVT_TEST_LogicFixture.EPSILON`.
  - File(s): `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_ObjectivePlanRules.c`
  - Estimate: 2.5 h

- [x] ✅ **T2.13 Init-tier: registry, validator, shims, runner**
  - Description: registry resolves off director and prefab; validate passes on the shipped registry; duplicate phase name / empty phase list / empty name skipped **and named**; both shims clone completely; runner enters phase 0 on commit and advances on the shim condition; the tick still early-returns on a live QRF and decrements nothing. ⚠ Init worlds never run `PostGameStart` — install the tick in-case.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_Objective*.c`
  - Estimate: 4 h

- [x] ✅ **T2.14 Persistence-tier: the v2 round trip**
  - Description: Rewrite `…ObjectiveDirector_SurvivesSaveAndReapply` (`:9107`) — plan name, target, phase **name**, both tick counters, two-key int bag, one-key vector bag, the `fob` asset record, a two-entry blacklist. Assert **deltas**; public API in, public getters out. 🔴 Do not widen the reload seam.
  - File(s): `Scripts/Game/Tests/TestSuites/Persistence/OVT_TEST_PersistenceRoundTripSuite.c`
  - Estimate: 3 h

- [x] ✅ **T2.15 context.md: the shim contract and its deletion schedule**
  - Description: Plus the bag-key table, the "public mutator may never change phase" rule restated at the new call sites, and the load-order rule the serializer inherits.
  - File(s): `docs/features/occupying/objectives/context.md`
  - Estimate: 0.75 h

---

## Phase 3: Plan-driven selection (9/9 complete) ✅ — **ADVANCED**

> **Gate: GREEN.** `compile-check.sh` 0 (6220 files); **All `{6A6E2A002F53A581}` 419/420** — the only red is the
> pre-existing `CompositionSlotGate` leftover. 🔴 **T3.8's parity case
> `OVT_TEST_Init_ObjectiveDirector_PlanDrivenSelectionReproducesTheSingleListPick` PASSED in the real client** —
> the fork from one candidate list into two plans is proved, and this was the last phase in which it could be,
> because both paths only coexist here. Deleted from the director: `CollectTownCandidates`, `CollectBaseCandidates`,
> `DistanceToNearestHeldBase`, `HasOccupyingTowerCoverage`, `ResolveBaseName`, `ResolveTownName`,
> `ResolveTownNameAt`, **plus** the strangler's `ResolveLegacyPlan()` and `LEGACY_TOWN_PLAN` /
> `LEGACY_BASE_PLAN` (context.md's deletion schedule puts them in this phase). See `context.md` §Phase 3
> for the equal-priority parity argument, the candidate-source flag table and the 12 can-fail proofs.

- [x] ✅ **T3.1 `OVT_ObjectiveTargetSelector` base**
  - Description: `[BaseContainerProps()]`, `int GetCandidateSources()` flag set, `bool ScoreCandidates(notnull OVT_ObjectiveCandidateSet, notnull array<float> outScores)`, `GetSelectorName()`.
  - File(s): `Scripts/Game/GameMode/Objectives/Selectors/OVT_ObjectiveTargetSelector.c`
  - Estimate: 1 h

- [x] ✅ **T3.2 `OVT_ObjectiveCandidateSet`, collected once per round**
  - Description: Parallel arrays of kind/position/name/population/support/threat/distance-to-nearest-held-base/tower-coverage. **The world queries happen here and nowhere else** — N plans cost one pass (D6).
  - File(s): `Scripts/Game/GameMode/Objectives/Selectors/OVT_ObjectiveCandidateSet.c`
  - Estimate: 2 h

- [x] ✅ **T3.3 The two shipped selectors with weights as attributes**
  - Description: Today's eight constants as defaults (`OVT_ObjectiveSelection.c:35-65`). ⚠ The statics keep taking **numbers**, never an `OVT_TownData`. Villages excluded; FOBs and radio towers non-candidates.
  - File(s): `Scripts/Game/GameMode/Objectives/Selectors/OVT_ResistanceTownObjectiveSelector.c`, `OVT_ResistanceBaseObjectiveSelector.c`
  - Estimate: 3 h
  - Built: weights are `protected` attributes authored explicitly in both plan `.conf`s; `ApplyShippedWeights()` ties the defaults to the pure statics' constants in ONE place. `OVT_ObjectiveSelection.c` is byte-unchanged.

- [x] ✅ **T3.4 Plan-driven `SelectObjective()`**
  - Description: Per plan, faction/cap/`m_fChance` permitting: score the selector's best candidate, apply the blacklist, multiply by `m_fPriority`, commit the top. **No jitter.** `LogSelection` keeps naming winner, runner-up and both scores, and gains the plan name.
  - File(s): `Scripts/Game/GameMode/Objectives/OVT_ObjectiveDirectorComponent.c`
  - Estimate: 3 h

- [x] ✅ **T3.5 Selection cadence**
  - Description: Run on the reselect flag, or when a slot is free and `m_iSelectionCooldownTicks` (registry attribute, **default 1** = today's behaviour) expired. Behind the debug flag, log candidates × plans and elapsed ms.
  - File(s): `OVT_ObjectiveDirectorComponent.c`, `OVT_ObjectiveRegistry.c`
  - Estimate: 1 h

- [x] ✅ **T3.6 The validator's Phase-3 rules**
  - Description: A plan with no selector; a selector whose candidate sources are empty.
  - File(s): `Scripts/Game/GameMode/Objectives/OVT_ObjectiveRegistry.c`
  - Estimate: 0.5 h
  - Both rules are exercised by `…Framework_BValidatorNamesAndSkipsABrokenPlan` (plans "Zeta" and "Eta"); its skipped-plan count moved 4 → 6.

- [x] ✅ **T3.7 Logic-tier: plan resolution**
  - Description: Extend `OVT_TEST_Logic_ObjectiveScaling.c` — equal priority picks higher score; ×2 lets a lower raw score win; ×0 excludes; all-blacklisted selects nothing; ties by input order.
  - File(s): `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_ObjectiveScaling.c`
  - Estimate: 1.5 h
  - Built as `…_PlanResolution_PriorityMultipliesAndTiesByRegistryOrder`, 13 rows, 4 can-fail proofs (M27–M30).

- [x] ✅ **T3.8 🔴 Init-tier: the two plans reproduce the single-list pick**
  - Description: Deterministic fixture towns+bases; drive the **old** selection path (still reachable — shims are still authored) and the **new** plan-driven path; assert the same position. Only possible while both exist. Plus: empty set, blacklist exclusion, cooldown default = every-tick.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_ObjectiveDirector.c`
  - Estimate: 3 h
  - Built as `…_PlanDrivenSelectionReproducesTheSingleListPick`: half A is three hand-built candidate sets (base wins / town wins / an EXACT float tie) decided both ways against the `.conf`-loaded selectors, half B drives the real `SelectObjective()` on the live world against a single-list reference. Can-fail executed: `m_fBasePrizeWeight` 45→5 in `Objective_BaseOffensive.conf`, compile 0, the pick inverts from index 2 to index 0.

- [x] ✅ **T3.9 context.md: the equal-priority parity argument + the source-flag table**
  - File(s): `docs/features/occupying/objectives/context.md`
  - Estimate: 0.5 h

---

## Phase 4: The harassment phase in config (11/11 complete) ✅ — **ADVANCED**

> **Gate: GREEN.** `compile-check.sh` 0 (6230 files); **All `{6A6E2A002F53A581}` 430/431** after the repair pass —
> the only red is the pre-existing `CompositionSlotGate` leftover. The first run was 427/431: three reds, **all**
> stale Phase-2 assertions or an under-authored Phase-2 fixture in `OVT_TEST_Init_ObjectiveFramework.c`, and
> **no Phase-4 code defect**. Each was repaired by STRENGTHENING the case, never by relaxing it.
>
> 🔴 The most valuable of the three: case B's "wholly valid" control plan was built from a phase with an **empty
> module bag** — genuinely a wedge that can neither advance nor end. T4.9's new wedge rule was right and the
> fixture had been wrong since Phase 2. The rule was not touched. Case E's repair also added the first assertion
> anywhere that **the runtime set is clones, never the config's own template objects** — Phases 5 and 6 both
> rebuild a module set and had no cover for that.
>
> Every §4 acceptance grep and diff green; 15 can-fail faults injected, compiled and restored. Workbench +
> play-test owed — see "Needs Human Verification". ⚠ Two recorded deviations: `OVT_TowerRecaptureBehaviorDeploymentModule` is
> EXCLUDED from the difficulty flip (it is shared with the frozen `Deployment_TowerRecaptureUnrest.conf`,
> so a flip there could not be behaviour-neutral), and a new temporary bridge `SendRampOperation()`
> carries the ramp continuation into the still-hard-coded forward-base phase until Phase 5 deletes it.

- [x] ✅ **T4.1 The four target resolvers**
  - Description: §3.4, each with the dedup-then-next-candidate contract in its header. `OVT_EnemyTowersAffectingTargetResolver` ports `:944-973` **including** the occupying-faction skip.
  - File(s): `Scripts/Game/GameMode/Objectives/Resolvers/*.c`
  - Estimate: 3 h

- [x] ✅ **T4.2 `OVT_SendDeploymentObjectiveOperation`**
  - Description: `m_sConfigName`, `m_Resolver`, `m_iMaxConcurrent` (-1 → `objectiveHarassmentMaxConcurrent`), `m_aLadder[]`, `m_sLadderProgressKey`, `m_fDedupRadius`. `TryAct()`: resolve → dedup → `HarassmentLadderIndex` → `CreateObjectiveDeployment` (**unchanged**). ⚠ The ladder names four registry entries; it does not build them.
  - File(s): `Scripts/Game/GameMode/Objectives/Modules/OVT_SendDeploymentObjectiveOperation.c`
  - Estimate: 4 h

- [x] ✅ **T4.3 The three conditions + the `IdleFor` abort**
  - Description: `SupportBelow`, `ProgressAtLeast`, `TargetKindIs`. 🔴 `SupportBelow` carries the **world-fact conjunct**: port `ObjectiveTownCarriesHarassmentDebuff()` into the condition as `m_sRequiredTownModifier`, **not** into `TownPhase2Gate`. Without it the town gate fires on its own entry tick.
  - File(s): `Scripts/Game/GameMode/Objectives/Modules/OVT_*ObjectiveCondition.c`, `OVT_IdleForObjectiveAbort.c`
  - Estimate: 3 h

- [x] ✅ **T4.4 Re-point the deployment-side reporters**
  - Description: Town harassment and base sabotage behavior modules call `objective.Report("harassment.successes"/"sabotage.successes", +1)`. 🔴 **The success signal is still PULLED by the tick** — `ConsumeReportedOperations()` compares against the progress marks; a `Report` that re-armed a timer reintroduces two red cases.
  - File(s): `OVT_TownHarassmentBehaviorDeploymentModule.c`, `OVT_BaseSabotageBehaviorDeploymentModule.c`
  - Estimate: 1.5 h

- [x] ✅ **T4.5 `OVT_ObjectiveConditionDeploymentModule` phase names**
  - Description: `m_iRequiredPhase`/`m_iThroughPhase` → `m_sFromPhase`/`m_sThroughPhase`; `PhaseInRange` becomes an index comparison via `PhaseIndexOf`. ⚠ Carry `EffectiveLastPhase`'s empty-through protection and the dropped-line consequences verbatim. Re-author the affected `Deployment_Objective*.conf`s — the config **name** does not move, so nothing orphans.
  - File(s): `Scripts/Game/GameMode/Objectives/Modules/OVT_ObjectiveConditionDeploymentModule.c`, `Configs/Deployment/Deployment_Objective*.conf`
  - Estimate: 2 h

- [x] ✅ **T4.6 The difficulty convention flip**
  - Description: The three objective-side modules read `-1` → difficulty per §3.10, and their configs are re-authored to `-1` in the same commit so shipped behaviour is unchanged. **`OVT_BaseRepairBehaviorDeploymentModule` is excluded.**
  - File(s): `Scripts/Game/GameMode/Objectives/Modules/*.c`, `Configs/Deployment/Deployment_Objective*.conf`
  - Estimate: 1.5 h

- [x] ✅ **T4.7 Author the Harassment phase in both plans**
  - Description: `[SendDeployment(tower, EnemyTowersAffecting)] [SendDeployment(harassment ladder, ObjectiveSelf)] [SendDeployment(sabotage, ObjectiveSelf) + TargetKindIs BASE]` then conditions then abort. ⚠ `tower || harassment || sabotage` is the shipped chain and the authored order is what reproduces it.
  - File(s): `Configs/Objective/Objective_*.conf`
  - Estimate: 1.5 h

- [x] ✅ **T4.8 Delete the Phase-1 shim authoring and its director methods**
  - Description: The 13 methods in implementation.md §4 Phase 4's deletes list, plus the `m_iLegacyPhase 1` authoring.
  - File(s): `Scripts/Game/GameMode/Objectives/OVT_ObjectiveDirectorComponent.c`, `Configs/Objective/Objective_*.conf`
  - Estimate: 1.5 h

- [x] ✅ **T4.9 The validator's Phase-4 rules**
  - Description: Unresolvable deployment config / ladder entry / resolver; a phase with no advance condition and no terminal operation; `-1` with no mapping.
  - File(s): `Scripts/Game/GameMode/Objectives/OVT_ObjectiveRegistry.c`
  - Estimate: 1 h

- [x] ✅ **T4.10 Init-tier: clones, resolvers, ladder, gate, refusal log**
  - Description: One dedicated clone case per new class (the `OVT_TEST_Init_TowerUnrestRecapture.c:214-259` shape — distinct non-default values, per-field `SetFailure`); tower resolver skips an occupying-held tower; dedup returns the next candidate; ladder saturates; the town gate refuses without the modifier and passes with it; a refusal logs once. ⚠ `SetSpawnedUnitsEliminated(true)` on the deployment **and every spawning module**.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_Objective{Operations,Sabotage,Insertion}.c`
  - Estimate: 4 h

- [x] ✅ **T4.11 context.md: order contract, world-fact conjunct, difficulty flip**
  - File(s): `docs/features/occupying/objectives/context.md`
  - Estimate: 0.75 h

---

## Phase 5: The forward base as an operation module (11/11 complete) ✅ — **ADVANCED — largest phase**

> **Gate: GREEN — All `{6A6E2A002F53A581}` 437/438 on the re-run** (the only red is the pre-existing
> `CompositionSlotGate` leftover). The first run was 435/438: one red was this phase's, one was that same
> leftover, and one was `…BaseRepair_AConfigResolvesAndIsOrdered` timing out as the **first case in the Init
> suite** with `Output: <none>` — a harness artifact that passed on the re-run, not a code fault. This phase's red —
> `…ObjectiveFramework_ARegistryResolvesAndValidates` — `…ObjectiveFramework_ARegistryResolvesAndValidates`
> reporting the real twelve-module ForwardBase set as a broken shim pair — was Phase 4's ⚠ TEMPORARY
> `AssertShimPair()` still wired to `ForwardBase`; **repaired by moving that caller line and writing
> `AssertForwardBasePhase()`/`AssertForwardBaseChain()`/`AssertRampSpansIntoTheForwardBase()` with real
> teeth** (order, both halves of the T5.7 deadlock fix, the four gate conjuncts, the idle abort, and the
> abort ORDER, which nothing had pinned before). The other two reds are not this feature's — the
> pre-existing `CompositionSlotGate` leftover and a first-case-in-suite harness timeout on
> `…BaseRepair_AConfigResolvesAndIsOrdered`.
>
> **Built 2026-08-21.** `compile-check.sh` **0** (6236 files). Every §4 acceptance grep is empty
> (`TickFOB|SendFOBOperation|SendFOBGarrisonOperation|ResolveFOBSite|TearDownFOB|IsPlayerAtFOB|WithinFOBBudget`,
> `SendRampOperation`, `m_iLegacyPhase 2`); `git diff` on the frozen neighbours is empty;
> `OVT_CampaignRequestComponent.c` has **no diff at all** and `OVT_DismantleEnemyFOBAction.c` carries only
> Phase 1's prose rename. **Director 6,225 → 4,926 lines (-1,299; 1,752 removed gross).**
>
> ⚠ **TWO ACCEPTANCE CRITERIA COULD NOT BE MET AS WRITTEN, and both are recorded in full in `context.md`:**
> `wc -l` **below 3,000** was written against a 5,201-line director that Phases 2–4 had already grown to 6,225,
> and the **before/after parity check** is impossible here because the shim it names STARTS A BATTLE and
> ADVANCES THE PHASE (it was never a predicate) and because the acceptance grep requires it deleted in the same
> phase. Four named substitutes were executed instead — see "THE §4 PARITY CHECK AS WRITTEN IS NOT AVAILABLE".
>
> 🔴 New this phase and binding: `OVT_BaseObjectiveAssetModule` (an asset outlives the phase that built it, so
> the director holds its module by key until the objective ends); `ClaimOperationInterval()` (the forward base's
> first claim on the pool, which an operation module returning false cannot express); and the runner's
> **"EVERY false condition holds the clock"** rule, without which the daylight hold would stop the backstop
> every night for a phase wedged for some other reason. Suite run + Workbench + play-test owed.

- [x] ✅ **T5.1 Read-only survey (gates the phase)**
  - Description: Re-verify the FOB block's boundaries and every external caller before moving a line. Record the verdict.
  - File(s): `docs/features/occupying/objectives/context.md`
  - Estimate: 1.5 h
  - Verdict: boundaries confirmed; external surface is 4 facade methods + `OVT_FOBRaiseSpawningDeploymentModule`'s
    reporter. 🔴 One caller the plan did not predict: three Init fixtures called `RecordFOB()` on an **idle**
    director, which the keyed reporter (which keeps `OnFOBRaised`'s guard) refuses - so those fixtures now commit
    an objective, and the teardown case enters the phase that REGISTERS the raise module.

- [x] ✅ **T5.2 `OVT_RaiseForwardBaseObjectiveOperation`**
  - Description: The whole ~1,500-line block; `OVT_FOBSiting` (457 L, pure) moves unchanged. Attributes: `m_sAssetKey` ("fob"), deployment/garrison config names, `m_iGarrisonMax`/`m_iBudgetCost` (-1 → difficulty), band/spread/lane siting attributes with today's tuned values (250→400 m spread, 5 lanes).
  - File(s): `Scripts/Game/GameMode/Objectives/Modules/OVT_RaiseForwardBaseObjectiveOperation.c`
  - Estimate: 8 h

- [x] ✅ **T5.3 The three conditions + the starvation abort**
  - Description: `AssetUp`, `ReserveAtLeast`, `DaylightWindow`, `AssetStarved`. 🔴 **`DaylightWindow` holds the phase timeout only** — starvation and the operation cadence keep running while waiting for dawn.
  - File(s): `Scripts/Game/GameMode/Objectives/Modules/OVT_*ObjectiveCondition.c`, `OVT_AssetStarvedObjectiveAbort.c`
  - Estimate: 2.5 h

- [x] ✅ **T5.4 Starvation ports verbatim, including C4**
  - Description: `IsFOBStarved(sourceHeld, aliveGroups, playerPresent)` with `playerPresent` measured **at the forward base** via `PlayerInRange(assetPosition, difficulty.baseCloseRange)`. ⚠ Do not "fix" it to the source base. Count the force through **handles** + `GetAliveMemberCount`, never `GetSpawnedEntities()`.
  - File(s): `OVT_RaiseForwardBaseObjectiveOperation.c`, `OVT_AssetStarvedObjectiveAbort.c`
  - Estimate: 1.5 h

- [x] ✅ **T5.5 The budget ceiling stays a counter**
  - Description: `fob.spent` in the asset record; `FOBBudgetCeiling`/`WithinFOBCeiling` unchanged; arms when the forward base's own deployment is **sent**, disarms on reset. ⚠ A spent ceiling deliberately does **not** set `m_bBlockedOnAffordability`.
  - File(s): `OVT_RaiseForwardBaseObjectiveOperation.c`
  - Estimate: 1 h

- [x] ✅ **T5.6 Restore**
  - Description: `WasRestoredFromSave()` and the `alreadyAttempted` latch gate the raise **independently**. The structure is found for teardown by **prefab resource name off the config**, never a runtime `EntityID`. The re-link is a first-tick job, never deserialization.
  - File(s): `OVT_RaiseForwardBaseObjectiveOperation.c`, `OVT_ObjectiveDirectorSerializer.c`
  - Estimate: 2.5 h

- [x] ✅ **T5.7 Author the ForwardBase phase in both plans**
  - Description: `[RaiseForwardBase] [SendDeployment(FOB garrison, ForwardBaseResolver)]` **plus the Harassment phase's operations, repeated** — the deadlock fix, expressed twice (this phase's bag **and** the deployment-side `m_sFromPhase`/`m_sThroughPhase` span). 🔴 Both halves or the deadlock returns.
  - File(s): `Configs/Objective/Objective_*.conf`, `Configs/Deployment/Deployment_Objective*.conf`
  - Estimate: 2 h

- [x] ✅ **T5.8 Delete the Phase-2 shim authoring and 36 director methods**
  - Description: Everything in implementation.md §4 Phase 5's deletes list. ⚠ `CanDismantleFOB`, `CanDismantleFOBAt`, `CountOccupyingDefendersNear`, `OnFOBDismantledByPlayer` **stay** as a thin facade; `OVT_DismantleEnemyFOBAction.c` and `OVT_CampaignRequestComponent.c` are **not edited**.
  - File(s): `Scripts/Game/GameMode/Objectives/OVT_ObjectiveDirectorComponent.c`
  - Estimate: 3 h

- [x] ✅ **T5.9 Init-tier: raise, restore, latch, teardown, anchor, starvation, daylight**
  - Description: Clone completeness; a restored deployment raises nothing and a fresh one raises exactly once; the latch blocks a second raise after a rebuy; teardown leaves no deployment of either config in the radius; the anchor provider still prefers the FOB and falls through; the starvation predicate true on each input independently; the daylight wait does **not** freeze starvation. ⚠ Stop a give-up fixture **at** the grace window.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_Objective{FOB,Anchor}.c`
  - Estimate: 4 h

- [x] ✅ **T5.10 Persistence-tier: the asset-record relink**
  - Description: Rewrite `…ObjectiveFOB_RelinksItsDeployment` (`:9566`); every asset field survives; a payload naming a dead deployment **resets** the objective rather than stranding it. Drive exactly `FOB_RELINK_ATTEMPTS` ticks and plant a countdown afterwards.
  - File(s): `Scripts/Game/Tests/TestSuites/Persistence/OVT_TEST_PersistenceRoundTripSuite.c`
  - Estimate: 2.5 h

- [x] ✅ **T5.11 context.md: C4 at the new call site, two latches, deadlock halves**
  - File(s): `docs/features/occupying/objectives/context.md`
  - Estimate: 0.75 h

---

## Phase 6: The battle as a terminal operation, and the last shim dies (7/7 complete) ✅ — **ADVANCED**

> **Built 2026-08-21.** `compile-check.sh` 0 (6235 files). Director **4,926 → 4,383 lines**. Both shim classes,
> the promoted entry point and all three temporary paths are gone; `grep -rn "OVT_LegacyPhase" Scripts/ Configs/`
> is empty and so is every other acceptance grep. **Suite run owed** (orchestrator).

- [x] ✅ **T6.1 `OVT_StartBattleObjectiveOperation`**
  - Description: `m_eMode` (`SIEGE`), `IsTerminal()` true; calls `StartBaseQRF`/`StartTownQRF` and returns; resolution is **polled** by the runner watching `m_CurrentQRF` go null. 🔴 Never add a second `m_OnFinished` subscriber — the manager deletes the controller inside the invoker's own dispatch. The reason goes in the method header.
  - File(s): `Scripts/Game/GameMode/Objectives/Modules/OVT_StartBattleObjectiveOperation.c`
  - Estimate: 2.5 h

- [x] ✅ **T6.2 Terminal completion through one path**
  - Description: The last phase completing ends the objective through `ResetObjective(reason, blacklist)` (**unchanged**): win → re-select, loss → reset with blacklist, both reported the same way, so a plan may end on any module.
  - File(s): `Scripts/Game/GameMode/Objectives/OVT_ObjectiveDirectorComponent.c`
  - Estimate: 1.5 h

- [x] ✅ **T6.3 `OVT_DaylightWindowObjectiveCondition` finishes**
  - Description: `IsCounterAttackWindow(hour, startHour, endHour)` with 05:00/15:00 as attribute defaults; the world hour off `OVT_Component`'s **inherited** `m_Time`, re-resolved lazily — ⚠ re-declaring `m_Time` shadows the base copy with one nothing fills. Log the wait **once**.
  - File(s): `Scripts/Game/GameMode/Objectives/Modules/OVT_DaylightWindowObjectiveCondition.c`
  - Estimate: 1.5 h

- [x] ✅ **T6.4 `AnchorRadiusForPhase` → `m_fAnchorRadius` on the phase**
  - Description: `-1` means today's per-phase value; the runner pushes at phase entry as it always did.
  - File(s): `OVT_ObjectivePhase.c`, `OVT_ObjectiveDirectorComponent.c`, `Configs/Objective/*.conf`
  - Estimate: 1 h
  - **Built:** the per-phase numbers are authored in both plans (600 / 1200 / 1200) and `-1` now means one flat
    `DEFAULT_ANCHOR_RADIUS` (600) rather than "the ported value for this phase index" — the positional meaning
    died with the enum lookup it needed. The registry's `PORTED_ANCHOR_PHASES` rule went with it.

- [x] ✅ **T6.5 Delete both shim classes and the `m_iLegacyPhase 3` authoring**
  - File(s): `Scripts/Game/GameMode/Objectives/Modules/OVT_LegacyPhase*.c`, `Configs/Objective/Objective_*.conf`
  - Estimate: 1 h

- [x] ✅ **T6.6 Init-tier: gate, daylight, terminal reset, reserve floor**
  - Description: The gate fires the starter **exactly once**; refuses at 22:00 and passes at 06:00 with every other input identical; the daylight refusal does not tick starvation and does not reselect; a terminal operation's completion resets on the reset path and blacklists only on the loss branch; the reserve floor is pushed on a refused ask and dropped on a satisfied one.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_Objective{Director,Reserve,Anchor,Framework}.c`
  - Estimate: 3 h
  - **Built:** `…GateWaitsForDaylightThenFiresOnce` re-pointed off the three deleted director readers onto the
    phase's own authored conditions, and extended with the RESOLUTION half (the battle slot empties, the next
    tick ends the objective, nothing is blacklisted). New `…ObjectiveDirector_TerminalPhaseEndsTheObjectiveOnOnePath`
    drives the failure half. `…ObjectiveFramework` gained `AssertCounterAttackPhase()` (type and position, never a
    count) and swapped its two shim clone cases for the battle module's. `…ObjectiveAnchor` re-pointed at the
    running plan's authored radii. **`…ObjectiveReserve` needed no edit** — it names no deleted symbol and
    already asserts the floor being pushed on a refused ask and lapsing on a satisfied one.

- [x] ✅ **T6.7 context.md: poll-not-subscribe restated, shim deletion recorded**
  - File(s): `docs/features/occupying/objectives/context.md`
  - Estimate: 0.5 h

---

## Phase 7: Validation, presentation and the admin surface (8/8 complete) ✅ — STANDARD

> **Built 2026-08-21.** `compile-check.sh` 0 (6235 files). Gate (All) owed - orchestrator.

- [x] ✅ **T7.1 The GM wire**
  - Description: `OVT_GMCampaignState.m_iObjectivePhase` (int) → `m_sObjectivePhaseName` (string) + a plan-name field. ⚠ **THREE edits** — the declaration, `CopyFrom` (`:208`) and `Clear` (`:267`); the one with no symptom is `Clear`. `CopyRecords` is for the four per-entity arrays only.
  - File(s): `Scripts/Game/GameMode/GM/OVT_GMRecords.c`
  - Estimate: 1.5 h

- [x] ✅ **T7.2 `SendCampaignObjective` signature + `WIRE_VERSION` bump**
  - Description: ⚠ `Rpc()` arity is a compile-check blind spot (BUG-090); the Init seam case is the only mechanical defence. `CAMPAIGN_RECORD_COUNT` does **not** change.
  - File(s): `Scripts/Game/Components/Controller/OVT_GMRequestComponent.c`
  - Estimate: 1 h

- [x] ✅ **T7.3 Retire `FormatObjectivePhase(int)` and its four keys**
  - Description: The phase row shows the authored `m_sPhaseName`; the "no objective" and "unknown" keys stay. ⚠ A `#`-prefixed key handed to `SetText` is resolved, so one formatter answers either a key or a proper noun with no branch at the call site.
  - File(s): `Scripts/Game/UI/GM/OVT_GMPanelFormat.c`, `OVT_GMPanelUIComponent.c`, `Language/localization_Overthrow.st`
  - Estimate: 1.5 h

- [x] ✅ **T7.4 The admin verbs: verify, do not rewire**
  - Description: ⚠ `/give-resources` and `/tick-resources` are **not** in the director — `git diff Scripts/Game/Components/Controller/OVT_AdminCommandsComponent.c` must be **empty**. What needs checking is the refusal-log dedup (`LogOperationRefusal`), whose call sites are now inside modules — assert it still de-duplicates across them.
  - File(s): `Scripts/Game/GameMode/Objectives/OVT_ObjectiveDirectorComponent.c` (verification only)
  - Estimate: 1 h

- [x] ✅ **T7.4b Delete or test the two zero-caller getters**
  - Description: `GetLoggedRefusalCount` / `HasLoggedRefusal` have no callers anywhere. Preferred: an Init case asserting the dedup. Do not leave them as they are.
  - File(s): `Scripts/Game/GameMode/Objectives/OVT_ObjectiveDirectorComponent.c`
  - Estimate: 0.75 h

- [x] ✅ **T7.5 The validator's error-text pass**
  - Description: Every rule names the plan, the phase and the attribute, in that order, and says what a modder should do. The authorability bar's only mechanical support.
  - File(s): `Scripts/Game/GameMode/Objectives/OVT_ObjectiveRegistry.c`
  - Estimate: 1.5 h

- [x] ✅ **T7.6 Logic-tier: the phase-name and plan-name rows**
  - Description: Authored name, empty name, `#`-key name.
  - File(s): `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_GMPanelFormat.c`
  - Estimate: 1 h

- [x] ✅ **T7.7 Init-tier: GM seam + validator rules**
  - Description: The seam accepts the changed record and the state receives every field; each validator rule fails the plan it should and names it; a failing plan is **never selected** while the others still run.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_GMRequestSeam.c`, `OVT_TEST_Init_Objective*.c`
  - Estimate: 2 h

---

## Phase 8: Help & documentation sync (5/5 complete) ✅ — `help-docs-sync` — **suite skipped (docs only)**

> **T8.1–T8.4 built 2026-08-21.** `compile-check.sh` **0 (6235 files)** after the one `desc:` string touched
> in `OVT_StartBattleObjectiveOperation.c`. No suite run, by policy. Player-facing behaviour did not change in
> this feature and almost no player-facing text needed to: **one** Field Manual body sentence was corrected
> ("stay in strength on the forward base" overstated a rule a SINGLE player satisfies) and seven translator
> `Comment`s were re-cited off deleted methods and constants. 🔴 **The C5 hit was in a fact-check note, not a
> body:** `#OVT-FieldManual_CounterAttacks_Text6`'s Comment claimed "MUSTER_TIME_MS = 1800000 ms. THIRTY REAL
> MINUTES" where the shipped constant is 900000 = fifteen, which is what the body has always said. ⚠ The
> **`wikijs` MCP was not attached**, so the modder page was written to `wiki-draft.md` and its publication is
> owed. T8.5 was run by the orchestrator on 2026-08-21.

- [x] ✅ **T8.1 Fact-check the tutorial popups**
  - Description: Every sentence naming a phase or quoting a number, against the shipped plans and difficulty presets. Cite a `file:line` or cut the sentence.
  - File(s): `Configs/Tutorials/*`
  - Estimate: 0.75 h

- [x] ✅ **T8.2 Fact-check the Field Manual's Counter Attacks page**
  - Description: ⚠ C5 is exactly the failure mode — a doc quoting "45 minutes on Normal" when Normal authors 60.
  - File(s): `Configs/FieldManual/*`
  - Estimate: 0.75 h

- [x] ✅ **T8.3 Wiki: describe the registry as the authoring surface**
  - Description: A modder-facing page is the natural new content. If the wikijs MCP is not connected, write the draft to `wiki-draft.md` and record the owed publication.
  - File(s): `docs/features/occupying/objectives/wiki-draft.md` / wiki
  - Estimate: 1.5 h

- [x] ✅ **T8.4 Record what is owed**
  - Description: The localization re-export from Workbench (Phase 7 removes four `#OVT-GMPanel_ObjectivePhase*` keys; `Language/*.conf` are build output), plus any unpublished wiki page.
  - File(s): `docs/features/occupying/objectives/context.md`
  - Estimate: 0.25 h

- [x] ✅ **T8.5 Epic + master rollup** - Completed 2026-08-21
  - Description: `/update-feature`, `/update-epic`, `/update-master`.
  - File(s): `docs/features/occupying/epic-overview.md`, `docs/overview.md`
  - Estimate: 0.5 h

---

## Post-build enhancements

- [x] ✅ **Land-isolated targets are never objectives (2026-08-23)** — author-reported: Erquy Harbour sits on an island, and nothing stopped the faction picking it. `ProximityScore` is straight-line distance and Erquy's nearest occupying holding is ~4.7 km, INSIDE the director's 5 km `m_fMaxUsefulDistance`, so it scored as an ordinary slightly-distant prize. Once picked, the harassment insertion drives a truck at a strait, strands it, and the stranded-transport path opens the doors and marches the passengers at open water; the forward base is then sited between the nearest holding and the target, which across a strait is sea. **Every `IsOceanAtPosition` call in the tree asks whether a POINT is wet; none asks whether a point can be WALKED TO.**
  - **Authored, not derived** — the engine has no A→B reachability query (`NavmeshWorldComponent` has only tile predicates + `GetReachablePoint(origin, distance, out)`, which answers "some reachable point" not "is THAT one reachable"; `AIPathfindingComponent.RayTrace` is a straight line that would also reject any target behind a hill with a good road around it). New `m_bLandIsolated` on `OVT_BaseControllerComponent` and `OVT_TownControllerComponent`, copied to `OVT_BaseData`/`OVT_TownData` at discovery as `[NonSerialized()]` (world-derived, re-read every Init — persisting it would let a save outvote a corrected map).
  - **Gated in `OVT_ObjectiveCandidateSet`, not in a scorer** — a zero score excludes nothing (the director picks the best candidate it has, so on a quiet map a penalised island still wins), and a scorer gate would have to be repeated in every selector including a modder's. "This target cannot be walked to" is a fact about the map that no doctrine may opt out of.
  - ⚠ **Only objective targeting is gated.** The base still defends itself (its deployments are created and spawned AT the base, never sent to it), still fights a QRF, and can still be captured. QRF waves were already safe — they `SpawnEntityPrefab` at a water-rejected landing zone near the target.
  - Authored on `OVT_Base_Erquy` + `Town_Erquy`; the town half is belt-and-braces (a village is already excluded by size).
  - Coverage: `OVT_TEST_Init_Objectives_LandIsolatedTargetsAreNeverCandidates` (plants the flag on a live record and collects twice — reachable IS collected, isolated is NOT, so the absence proves the flag and not an unrelated ineligibility) plus **`tools/check-land-isolated.py`**, because no test tier loads Eden and a Workbench re-save drops an authored attribute silently. Checker proven can-fail. **All 602/602.**
  - ⏸️ **Play-test owed** — confirm the faction now picks a mainland objective instead and that Erquy still defends/captures normally.

- [x] ✅ **The forward base is restored on load (2026-08-23)** — user play-test: *"a FOB has disappeared with its garrison still standing there."* Nothing removed it. `OVT_PersistenceTracking.Track()` only makes an entity SAVEABLE; it comes BACK only when the `PersistenceConfig` it matches carries `SelfSpawn`, and matching is by the `ComponentClassPersistenceConfigRule` entries in `Configs/Systems/Persistence/Overthrow.conf`. `Prefabs/Bases/OVT_OccupyingFOB.et` matched none of the four, proven by decoding the savepoint blob (zero records for it, while every deployment and buildable was present). Fixed with a fifth rule on `OVT_OccupyingFlagComponent` plus `OVT_OccupyingFlagComponentSerializer`, whose real job is re-queuing the navmesh a restored structure does not carve on its own. Coverage: `OVT_TEST_Init_ObjectiveFOB_MStructureConfigSelfSpawns`. `compile-check.sh` exit 0.
  - ⏸️ **Suite run + play-test owed** — raise a forward base, save, load, and see it standing with its garrison; walk an AI past it for the navmesh.

- [x] ✅ **`objectiveFirstOperationDelayMinutes` (2026-08-23)** — user: the faction sent teams the instant a place became the objective. New difficulty field; `ArmFirstOperationDelay()` at the commit funnel, **after** the phase entry (which zeroes the cadence). Easy 240 / Normal 150 / Hard 100 / Extreme 60 / Insane 30, class default 0 = pre-setting behaviour. Coverage: `OVT_TEST_Init_ObjectiveDirector_ANewObjectiveHoldsFireBeforeItsFirstTeam`. `compile-check.sh` exit 0.
  - ⏸️ **Play-test owed** — a new objective must log "holding fire for N in-game minute(s)" and send nothing until it elapses.

## Needs Human Verification

Filled in as phases complete. Seeded from implementation.md §6 "Verification Method":

- [ ] Workbench: `Configs/Objective/overthrowObjectives.conf` — both plans expand, no unresolved attribute (Phase 2+)
- [ ] Workbench: `Prefabs/GameMode/OVT_OverthrowGameMode.et` — `m_Registry` resolves (Phase 2)
- [ ] Workbench: `Configs/Deployment/overthrowDeployments.conf` — the repair entry still resolves with its four modules and `m_bDirectorOnly 0` (Phase 1)
- [ ] Workbench: both plans' **Harassment** phase — six modules each, two polymorphic resolver sub-objects, and `m_aLadder`'s four quoted rungs all present (Phase 4). ⚠ `m_aLadder` is the first array-of-strings this mod authors in its own configs
- [ ] Workbench: both plans' **ForwardBase** phase — **twelve** modules each (5 operations, 5 conditions counting the kind guard, 2 aborts), two polymorphic resolver sub-objects, and the repeated ladder's four quoted rungs (Phase 5)
- [x] ✅ **PARTIAL 2026-08-21** — the forward base was raised in a live play-test and the author reports **no early-raise issues**, which closes the Phase-5 raise-gating fix. ⚠ Still unverified within this item: the supply party driving, the structure facing, the garrison cap, and the ramp continuing to send through the phase. Play-test: **a real forward base, end to end** (Phase 5) — the site is chosen once and logged with a score, the supply party drives, the structure goes up facing the objective, the garrison arrives and is capped at `objectiveFOBGarrisonMax`, and the ramp KEEPS SENDING through the phase. Nothing in the Init tier drives a real raise
- [ ] Play-test: **the forward base's first claim on the pool** (Phase 5) — with the pool between the sabotage price and the forward-base price, the faction must SAVE UP for the base rather than buying a sabotage mission every time it passes 100. The reserve-floor line must name the forward base, not the last operation asked
- [ ] Play-test: **starve one out** (Phase 5) — take or empty the supplying base, or stand on the flag, and the objective must be abandoned after `objectiveStarvationMinutes`, with both transition lines in the log — **including at night**, mid daylight-wait
- [ ] Play-test: **dismantle one** (Phase 5) — the action must refuse at range and while defended, say why, and on success cost the occupying faction the forward base's price
- [ ] Save/Continue **in the forward-base phase** (Phase 5) — the base must be re-linked rather than rebuilt or abandoned, its spend must survive, and no SECOND structure may appear on any load
- [ ] Play-test: **an objective covered by TWO resistance-held towers** must send a recapture team to the second one on a later interval, not stop after the first (Phase 4 — the dedup-then-next-candidate walk is the one part of the send module the Init tier cannot drive without spending real resources)
- [x] ✅ **PARTIAL 2026-08-21** — repeated sabotage insertions ran "without a hitch as designed", the objective advanced Harassment → ForwardBase in the log, and a co-driver was observed opening a gate. ⚠ Still unverified: the escalation ladder rung-by-rung, the town gate firing only after the debuff lands, and sabotage at a base objective. Play-test: the harassment ramp end to end — one operation per difficulty interval, escalating a rung per completed operation, the town gate firing only AFTER the debuff has landed, sabotage at a base objective, and the ramp continuing into the forward-base phase (Phase 4)
- [ ] Save/Continue mid-ramp — the deployment-side phase span is resolved by NAME through the running plan now, so a restored objective must KEEP its ramp deployments rather than collecting them (Phase 4). ⚠ A `[Overthrow.ObjectiveCondition] '…' spans phases … which the running plan … does not carry` line is the symptom if it does not
- [ ] Play-test F2–F13 — the thirteen solo steps of §6
- [ ] 🔬 The **modder exercise** (§6 step 9) — the acceptance test for the whole design
- [ ] 🔴 Localization re-export from Workbench after Phases 7 **and 8** - THREE KEYS WERE REMOVED from the `.st` master (`#OVT-GMPanel_ObjectivePhaseHarassment`, `…ForwardBase`, `…CounterAttack`) and ONE ENGLISH BODY CHANGED (`#OVT-FieldManual_CounterAttacks_Text4`); `Language/*.conf` are build output that still carry both. One export covers both
- [ ] 🔴 Publish the modder wiki page (Phase 8 T8.3) - `docs/features/occupying/objectives/wiki-draft.md` to `development-documentation/objective-plans`, **searching for an existing objective page first**. The `wikijs` MCP was not attached to the Phase 8 session
- [ ] 🔴 Fix two wrong numbers in the counter-attacks wiki brief before publishing it (`docs/features/occupying/counter-attacks/context.md` T10.3): the muster window is **fifteen** real minutes (900000 ms), not thirty, and `objectiveQRFResourceGate` is 750/750/1200/2000/3000, not 2000/1500/1200/1000/800
- [ ] Re-translate `#OVT-FieldManual_CounterAttacks_Text4`; `#OVT-FieldManual_BaseCapture_Text5` and `#OVT-Tutorial_BasesFirstCapture_Body` still carry RETIRED-MECHANIC text in `Target_de_de` and `Target_uk_ua` and are factually wrong in those languages today
- [ ] Comment sweep OUTSIDE this feature's freeze: four stale "30-minute muster" comments in `Scripts/Game/Controllers/OccupyingFaction/` (`OVT_QRFModes.c:36,:61`, `OVT_QRFSiege.c:69`, `OVT_QRFControllerComponent.c:142,:245`), three "one tick in forty-five" comments (Normal authors 60; 45 is Hard), and the `OVT_EObjectivePhase` mention at `OVT_QRFModes.c:21`
- [ ] 🔴 MP/host pass on the GM snapshot wire (Phase 7) - `WIRE_VERSION` 2 -> 3 and `SendCampaignObjective` changed from `(name, int phase)` to `(name, planName, phaseName)`. Arity was hand-counted (4 payload arguments at all three sites) because `Rpc()` arity is a compile-check blind spot (BUG-090); only a real client/server pair proves it
- [ ] Play-test: `/give-resources` and `/tick-resources` still unblock a poverty-stalled objective (Phase 7 T7.4 - verification only, `OVT_AdminCommandsComponent.c` is unchanged)
- [ ] Play-test: the Game Master panel's phase row reads "Town Offensive: Harassment" and follows the ramp (Phase 7); it renders raw keys until the localization re-export above
- [ ] 🔴 **Run All `{6A6E2A002F53A581}` against the 2026-08-21 play-test fix round** (4 files under `Deployments/` + `Modded/SCR_AIGroup.c`, uncommitted). `compile-check.sh` is the only gate it has had, and almost none of it is Logic-tier assertable. **This is the last gate before that round is committed.**
- [ ] ⚠ Re-running the feature's DoD greps will now FAIL on `git diff Scripts/Game/GameMode/Deployments/` — the freeze was build-time discipline and was legitimately broken by play-test fixes to shipped code. Do not "restore" it; see `context.md`.
- [ ] Dedicated-server / MP pass — the automated spine covers MP not at all

---

## Task Status Legend

- [ ] Not started · 🔄 In progress · ⏸️ Blocked · [x] ✅ Completed · [x] ❌ Cancelled

---

*Update this file as tasks are completed. Mark tasks ✅ immediately when done.*
