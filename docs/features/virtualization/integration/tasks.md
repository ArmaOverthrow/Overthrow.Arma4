# Virtualization Integration - Task Checklist

**Last Updated:** 2026-08-17 (Phase 8 complete; post-completion amendment A1 built)
**Progress:** 62/62 + A1 tasks complete (100%)

> Phase 1 is **STANDARD** (`component-developer`); phases 2–7 are **ADVANCED** (`component-developer-advanced`); phase 8 is `help-docs-sync`. Suite per phase: 1–7 → **All** `{6A6E2A002F53A581}` (every phase touches the deployment create path, serializers or the shared persistence gate), 8 → skipped (docs-only). Source of truth: `implementation.md` §4 and the Agent Routing Summary.

---

## Phase 1: Inherited defects + observer spike GATE (9/9 complete) — STANDARD ✅ (All 237 green 2026-08-17)

- [x] **T1.1 Defect (a): patrol check interval seconds vs ms**
  - Description: `m_fCheckInterval` authored in seconds (`:15-16`) but compared against a `GetWorldTime()` ms delta (`:62`). Keep attribute in seconds (config surface frozen); compare `>= m_fCheckInterval * 1000`; comment the unit at both ends.
  - File(s): `Scripts/Game/GameMode/Deployments/Modules/OVT_PatrolBehaviorDeploymentModule.c`
  - Estimate: 0.5 h

- [x] **T1.2 Defect (b): town cache never caches**
  - Description: `m_fCacheTimeout = 30.0` vs ms delta at `:127` → cache expires in 30 ms. Constructor literal → `30000.0`, fix comment. Sweep sibling modules for the same shape (`OVT_ReinforcementBehaviorDeploymentModule.c:57`/`:273` believed already correct) and record the verdict in context.md.
  - File(s): `Scripts/Game/GameMode/Deployments/Modules/OVT_TownConditionalDeploymentModule.c`
  - Estimate: 0.5 h

- [x] **T1.3 Defect (c): invested resources / threat never stamped**
  - Description: `CreateDeployment` gains optional `resourcesInvested`/`threatLevel` params, stamped right after `InitializeDeployment` (`:773`); pass values in scope at `EvaluateFactionDeployments` (`:227-233`); `ForceCreateDeployment` forwards them. Do NOT change `ApplyPersistedDeployment` ordering.
  - File(s): `Scripts/Game/GameMode/Deployments/OVT_DeploymentManager.c`
  - Estimate: 1 h

- [x] **T1.4 Defect (d): marker teleported to last vehicle**
  - Description: Delete `m_ParentDeployment.GetOwner().SetOrigin(spawnPos);` (`OVT_VehicleSpawningDeploymentModule.c:195-196`). The marker is the persisted record and the key basis (D6) — it must not move.
  - File(s): `Scripts/Game/GameMode/Deployments/Modules/OVT_VehicleSpawningDeploymentModule.c`
  - Estimate: 0.25 h

- [x] **T1.5 Defect (e): waypoint double-insert**
  - Description: Remove the **callee-side** inserts (`:344`, `:367`) so each waypoint is inserted once and `OnDeactivate`'s double `DeleteEntityAndChildren` has nothing to double-delete. (Phase 5 deletes this code entirely; fix keeps phases independently revertable.)
  - File(s): `Scripts/Game/GameMode/Deployments/Modules/OVT_MultiTownPatrolBehaviorDeploymentModule.c`
  - Estimate: 0.5 h

- [x] **T1.6 Defect (d-adjacent): `m_fMaxCruiseSpeed` dropped by `CloneModule`**
  - Description: Add the copy in `CloneModule` (`:145-160`). Leave the apply commented out (`:206-212`) with its "bugged in Reforger" note + dated pointer.
  - File(s): `Scripts/Game/GameMode/Deployments/Modules/OVT_VehicleSpawningDeploymentModule.c`
  - Estimate: 0.25 h

- [x] **T1.7 Retire dead duplicate `DestroyDeployment`**
  - Description: Manager's `DestroyDeployment(...)` (`:1045-1050`) is byte-identical to `DeleteDeployment` and has zero callers. Delete; grep-prove only `OVT_DeploymentComponent.DestroyDeployment` + its two manager call sites remain.
  - File(s): `Scripts/Game/GameMode/Deployments/OVT_DeploymentManager.c`
  - Estimate: 0.25 h

- [x] **T1.8 BUG-028 regression coverage (best effort)** — case DROPPED, accessor not added (verdict in context.md; Init world cannot drive the evaluator)
  - Description: Add `int GetFactionDeploymentIdCount(int factionIndex)` accessor + Init-tier case: `ForceCreateDeployment` → delete marker → `CleanupDestroyedDeployments()` → count drops. If the Init world cannot drive the manager, record the verdict, drop the case, keep the accessor only if used.
  - File(s): `Scripts/Game/GameMode/Deployments/OVT_DeploymentManager.c`, `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c`
  - Estimate: 1 h

- [x] **T1.9 THE GATE: server-side `InsertObserverSP` verification** — VERDICT: HONOURED, application deferred 1 frame (context.md)
  - Description: Init-tier case — far position: `HasObserverWithinRangeSq` false → spawn throwaway marker **entity**, `InsertObserverSP(key, 0, 0, entity)` (never null, never fixed-position) → assert true + `GetObserversSP()` grew by 1 → `RemoveObserverSP(key)` → false + count returned → clean up entity before reporting. Write verdict (honoured? survives frames? key semantics) into context.md. **Phase 6 must not start without it.**
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c`
  - Estimate: 1.5 h

---

## Phase 2: Shared consumer scaffolding — no behaviour change (8/8 complete) — ADVANCED ✅ (All 243 green 2026-08-17)

- [x] **T2.1 Create `OVT_DeploymentVirtualKey.c`** — world-free statics only
  - Description: `DeriveKey`, `Sanitise`, `Disambiguate`, `ModuleTag`, `OwnerKey`, `MissingCount`. No manager/world/entity/registry identifier anywhere in the file, comments included (Logic-tier grep rule).
  - File(s): `Scripts/Game/GameMode/Deployments/OVT_DeploymentVirtualKey.c` (new)
  - Estimate: 1 h

- [x] **T2.2 Create `OVT_VirtualPlanFactory.c`** — world-free plan builders
  - Description: `BuildDefendPlan`, `BuildPerimeterPlan` (4 points 90° apart from `fromPosition→centre` bearing, WAIT interleaved, cycle true, wait durations passed in), `BuildRoutePlan` (MOVE+WAIT per stop, optional closing MOVE, `m_bCycle = !returnToStart`). Arrays parallel and equal-length. Same grep constraint as T2.1.
  - File(s): `Scripts/Game/GameMode/Deployments/OVT_VirtualPlanFactory.c` (new)
  - Estimate: 1.5 h

- [x] **T2.3 Logic-tier test file** — Fast group
  - Description: `OVT_TEST_Logic_DeploymentVirtualization.c` (suite `OVT_TEST_LogicSuite`), cases per §7: keys, missing count, defend/perimeter/route plan shapes. Can-fail proof per case; no `maxAttempts`; distance tolerances (+1 ULP trap); `out`/`owned` reserved.
  - File(s): `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_DeploymentVirtualization.c` (new)
  - Estimate: 2 h

- [x] **T2.4 `OVT_DeploymentComponent`: virtual key + `EnsureGroups()` fan-out**
  - Description: `m_sVirtualKey`, `EnsureVirtualKey()` (derive-once via manager disambiguation ordinal, store), `GetVirtualKey()`, `EnsureGroups()` fanning out over `GetSpawningModules()` (inert this phase).
  - File(s): `Scripts/Game/GameMode/Deployments/OVT_DeploymentComponent.c`
  - Estimate: 1 h

- [x] **T2.5 Serializer v2: append `virtualKey`**
  - Description: Bump to version 2, **append** after `spawnedUnitsEliminated` (never insert/reorder — positional binary context). `version < 2` → empty key → derived on first use (pre-feature-save migration). Extend `ApplyPersistedDeployment` signature; flag-before-`InitializeDeployment` ordering unchanged.
  - File(s): `Scripts/Game/GameMode/Deployments/OVT_DeploymentComponentSerializer.c` (or wherever the serializer lives — re-grep)
  - Estimate: 1.5 h

- [x] **T2.6 Manager: subscribe once + fan out + `NextKeyOrdinal`**
  - Description: In `PostGameStart` subscribe `GetOnRecordsRestored()`/`GetOnGroupWiped()` (guarded against double-subscribe on second campaign); handlers fan out over `GetAllDeployments()` (no-ops until Phase 3). Add `NextKeyOrdinal(string baseKey)`.
  - File(s): `Scripts/Game/GameMode/Deployments/OVT_DeploymentManager.c`
  - Estimate: 1 h

- [x] **T2.7 `BuildVirtualPlan` virtual on the behaviour base**
  - Description: `OVT_BaseBehaviorDeploymentModule.BuildVirtualPlan(vector)` default `null`; spawning-module helper takes first non-null answer from `GetBehaviorModules()`. No override yet.
  - File(s): `Scripts/Game/GameMode/Deployments/Modules/OVT_BaseBehaviorDeploymentModule.c` (+ spawning base)
  - Estimate: 0.5 h

- [x] **T2.8 Seed context.md**
  - Description: Phase 1 spike verdict, key scheme, §3.5 ordering note, Tower Garrison config design (§3.3), kill-switch removal ledger.
  - File(s): `docs/features/virtualization/integration/context.md`
  - Estimate: 0.5 h

---

## Phase 3: Town Patrol migration + proximity-toggle retirement (10/10 complete) — ADVANCED ✅ (All 245 green 2026-08-17)

- [x] **T3.1 `OVT_PatrolBehaviorDeploymentModule.BuildVirtualPlan` + delete waypoint authoring**
  - Description: DEFEND → `BuildDefendPlan(GetPatrolCenter())`; PERIMETER → roll 4 waits `RandFloatXY(45,75)`, `BuildPerimeterPlan`, road-snap patrol points (`OVT_WorldUtils.FindNearestRoad`; WAIT copies snapped position). Delete `ApplyPatrolBehaviorToGroup`, `ApplyPatrolBehaviorToExistingGroups`, `CheckForNewGroups`, `IsGroupProcessed`, `m_aProcessedGroups`, `ForceReapplyPatrolBehavior`, OnUpdate polling. Keep `GetPatrolCenter()`.
  - File(s): `Scripts/Game/GameMode/Deployments/Modules/OVT_PatrolBehaviorDeploymentModule.c`
  - Estimate: 2 h

- [x] **T3.2 Infantry module: handles + `EnsureGroups()` per §3.5**
  - Description: `m_aSpawnedGroups` → `m_aHandles : array<int>`; converge-to-wanted registration (position `GetRandomSpawnPosition`, plan from behaviour module, `groupName = m_sGroupType`, faction **key** not index, `spawnDistanceOverride -1`, `importance = m_eImportance` new attribute default NORMAL — copy in `CloneModule`). GM-tag each group immediately (§3.7).
  - File(s): `Scripts/Game/GameMode/Deployments/Modules/OVT_InfantrySpawningDeploymentModule.c`
  - Estimate: 2.5 h

- [x] **T3.3 `Reinforce(n)` rebuys through the API**
  - Description: Charge cost, raise wanted, run `EnsureGroups` convergence, clear `m_bSpawnedUnitsEliminated` on success, re-run `CheckAllSpawningModulesEliminated()`. Keep `CanReinforce`/cost math/logs. `GetMissingGroupCount()` from handles.
  - File(s): `Scripts/Game/GameMode/Deployments/Modules/OVT_InfantrySpawningDeploymentModule.c`
  - Estimate: 1 h

- [x] **T3.4 Delete agent-count elimination machinery; `OnGroupWiped` per §3.6**
  - Description: Delete `CheckIfUnitsEliminated`, `CheckGroupStatus`, `IsGroupAlive`, `GetAliveGroupCount`; keep `m_iSpawnedEver` latch. `OnGroupWiped(handle)`: remove handle; empty + spawned-ever → eliminated flag + `CheckAllSpawningModulesEliminated()`.
  - File(s): `Scripts/Game/GameMode/Deployments/Modules/OVT_InfantrySpawningDeploymentModule.c`
  - Estimate: 1 h

- [x] **T3.5 Teardown: `OnDeactivate` no-op, `OnCleanup` unregisters, NO `OnDelete`**
  - Description: Per D8 — quit-to-menu must not destroy records (core T3.11 precedent).
  - File(s): `Scripts/Game/GameMode/Deployments/Modules/OVT_InfantrySpawningDeploymentModule.c`
  - Estimate: 0.5 h

- [x] **T3.6 `UpdateDeployment`: activate-once; delete `IsPlayerInRange`/`DeactivateDeployment`**
  - Description: Keep module `Update()` loops; replace proximity block (`:255-275`) with unconditional activate-once. Delete `IsPlayerInRange` (`:278-281`) + `DeactivateDeployment` (`:188-217`) grep-proven. Leave `m_bEnableProximityActivation`/`m_fActivationRange` declared; mark descs as no longer driving lifecycle.
  - File(s): `Scripts/Game/GameMode/Deployments/OVT_DeploymentComponent.c`
  - Estimate: 1.5 h

- [x] **T3.7 Wire manager fan-outs to real handlers**
  - Description: `GetOnRecordsRestored` → each deployment's `EnsureGroups()`; `GetOnGroupWiped(handle)` → each deployment's spawning modules.
  - File(s): `Scripts/Game/GameMode/Deployments/OVT_DeploymentManager.c`
  - Estimate: 0.5 h

- [x] **T3.8 Remove the deployments kill-switch guard**
  - Description: `OVT_DeploymentManager.c:144` guard removed; evaluator's own guards (0-players `:150`, QRF `:153-155`) **kept** (D9).
  - File(s): `Scripts/Game/GameMode/Deployments/OVT_DeploymentManager.c`
  - Estimate: 0.25 h

- [x] **T3.9 Init-tier coverage**
  - Description: Registry resolves `"Town Patrol"` + patrol module builds non-empty **cycling** plan; simulated `EnsureGroups` cycle registers/reclaims idempotently/unregisters with `spawnDistanceOverride = 0` (Manual policy — autotest camera must not materialise anything).
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c`
  - Estimate: 1.5 h

- [x] **T3.10 Record retired symbols + fixture sweep in context.md**
  - Description: Retired-symbol list with grep verdicts; `RegisterGroup(` fixture sweep (movement D12 discipline).
  - File(s): `docs/features/virtualization/integration/context.md`
  - Estimate: 0.5 h

---

## Phase 4: Tower garrisons become a deployment config (9/9 complete) — ADVANCED ✅ (All 248 green 2026-08-17)

- [x] **T4.1 Read-only survey FIRST**
  - Description: Confirm `OVT_RadioTowerData.garrison` has no reader outside `CheckRadioTowers`, is `[NonSerialized]`, absent from `OVT_PersistedRadioTower` (field order `location, faction, disabledRemaining`) and from the JIP stream (`RplSave:1553-1561`). Record verdicts. Touch no persisted/streamed field order.
  - File(s): `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c` (read-only), context.md
  - Estimate: 0.5 h

- [x] **T4.2 Verify the composition per faction**
  - Description: `light_patrol` vs `m_aTowerDefensePatrolPrefab` for USSR (`Group_USSR_SentryTeam.et`) and US (`Group_US_SentryTeam.et`). Match → config uses `light_patrol`; mismatch → add `tower_garrison` registry entry to both faction configs. Never a raw prefab or faction index.
  - File(s): `Configs/Factions/USSR_OverthrowData.conf`, `Configs/Factions/US_OverthrowData.conf` (read; edit only on mismatch)
  - Estimate: 0.5 h

- [x] **T4.3 New module: `OVT_RadioTowerControlConditionDeploymentModule`**
  - Description: Structural copy of `OVT_BaseControlConditionDeploymentModule` against `GetNearestRadioTower`; `EvaluateStaticCondition`/`EvaluateCondition`; `m_fMaxDistance` 300, `m_bRequireControl` true; hand-written `CloneModule` copying all attributes.
  - File(s): `Scripts/Game/GameMode/Deployments/Modules/OVT_RadioTowerControlConditionDeploymentModule.c` (new)
  - Estimate: 1 h

- [x] **T4.4 New module: `OVT_RadioTowerCaptureBehaviorDeploymentModule`**
  - Description: OnUpdate — eliminated + not-yet-fired → resolve tower within `m_fMaxDistance`, verify still deployment's faction, `ChangeRadioTowerControl(tower, playerFactionIndex)` **once** (edge-latched). `CloneModule` copies attributes, not the latch.
  - File(s): `Scripts/Game/GameMode/Deployments/Modules/OVT_RadioTowerCaptureBehaviorDeploymentModule.c` (new)
  - Estimate: 1 h

- [x] **T4.5 Author `Deployment_TowerGarrison.conf` + registry entry**
  - Description: Per §3.3 — 5 modules, fresh repo-unique GUID, `m_iPriority 1`, `m_fChance 100`, `m_iMaxInstances -1` (dedup is the one-per-tower mechanism — verify Eden tower spacing > 250 m), non-zero cost (flag numbers as play-test tuning), min/max groups tuned to today's `RandInt(patrolGroupsMin, patrolGroupsMax)`.
  - File(s): `Configs/Deployment/Deployment_TowerGarrison.conf` (new + .meta), `Configs/Deployment/overthrowDeployments.conf`
  - Estimate: 1.5 h

- [x] **T4.6 Location classification: OR in `RADIO_TOWER`**
  - Description: `GetLocationTypeAtPosition` — compute today's single value unchanged, then `|= RADIO_TOWER` within 300 m of a tower (D19). Nothing else in the precedence moves.
  - File(s): `Scripts/Game/GameMode/Deployments/OVT_DeploymentManager.c`
  - Estimate: 0.5 h

- [x] **T4.7 Delete the garrison halves of `CheckRadioTowers()`**
  - Description: Keep sabotage countdown (`:547-557`) byte-for-byte. Delete `inrange`/`!m_CurrentQRF` gate, spawn block (guard goes **with its block**, D20), reap+capture block, despawn block, `OVT_RadioTowerData.garrison`. `CallLater(...9000...)` stays. Note `m_aTowerDefensePatrolPrefab` now reader-less; leave declared (feature 5 cleans up).
  - File(s): `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c`
  - Estimate: 1.5 h

- [x] **T4.8 Coverage: config validity, DEFEND plan, classification, capture-only-on-real-wipe**
  - Description: Init tier — `"Tower Garrison"` resolves + `IsValidConfig`; one-point DEFEND non-cycling plan; tower position includes `RADIO_TOWER` bit AND previous value; capture module does NOT fire on registered-not-wiped; fires **exactly once** after `ReportMemberKilled` wipes the last group.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c`
  - Estimate: 2 h

- [x] **T4.9 Document behaviour changes in context.md → Phase 8**
  - Description: Garrisons materialise for any engine observer (GM camera incl.); no map-wide QRF despawn; creation costs resources + pauses during QRF/0 players; GM icons now `DEPLOYMENT`/`"Tower Garrison"`.
  - File(s): `docs/features/virtualization/integration/context.md`
  - Estimate: 0.5 h

---

## Phase 5: Vehicle patrols (8/8 complete) — ADVANCED ✅ (All 249 green 2026-08-17) ✅ (code complete 2026-08-17, compile 0; All suite owed)

- [x] **T5.1 Multi-town module: `BuildVirtualPlan` from route; delete waypoint authoring**
  - Description: `BuildRoutePlan(stops, m_iWaitTimeAtTown, m_bReturnToStart, deploymentOrigin)` from `m_aPatrolRoute` planning. Delete `CreatePatrolWaypoints`, `CreateWaypointsForVehicles`, `CreateTownWaypoint`, `CreateReturnWaypoint`, `m_aWaypoints`, OnDeactivate waypoint teardown. Route planning stays.
  - File(s): `Scripts/Game/GameMode/Deployments/Modules/OVT_MultiTownPatrolBehaviorDeploymentModule.c`
  - Estimate: 2 h

- [x] **T5.2 Route completion without waypoint entities**
  - Description: Re-read `CheckPatrolComplete` (`:388-416`); keep if position-based (re-point at final plan stop), else distance check ≤ ~100 m + `m_bReturnToStart` gate. `OnPatrolComplete` (refund + self-`DeleteDeployment`) unchanged in shape.
  - File(s): `Scripts/Game/GameMode/Deployments/Modules/OVT_MultiTownPatrolBehaviorDeploymentModule.c`
  - Estimate: 1 h

- [x] **T5.3 Vehicle module: register crew via core, always materialised**
  - Description: Vehicle spawning + `GetRandomSpawnPosition` unchanged. Crew: `groupName = m_sCrewGroupType`, 5 m from vehicle, plan from behaviour module, importance NORMAL, `spawnDistanceOverride = m_iSpawnDistanceOverride` (new attribute, default 100000; add to `CloneModule` beside T1.6's fix). GM-tag.
  - File(s): `Scripts/Game/GameMode/Deployments/Modules/OVT_VehicleSpawningDeploymentModule.c`
  - Estimate: 2 h

- [x] **T5.4 Crew seating via per-member `GetOnAgentAdded`**
  - Description: Replace `GetOnInit()` subscription (fires only on complete fill — may never under budget pressure). Seat each arriving agent PILOT → TURRET → CARGO; `FillCompartment` unchanged; unsubscribe on unregister.
  - File(s): `Scripts/Game/GameMode/Deployments/Modules/OVT_VehicleSpawningDeploymentModule.c`
  - Estimate: 1.5 h

- [x] **T5.5 Teardown: stolen-vehicle guarantee verified, not rebuilt**
  - Description: `OnDeactivate` no-op; `OnCleanup` → `UnregisterGroup` per crew (respects held members), delete vehicle only if unoccupied by player-controlled entity and not player-owned. Delete the 40 m rule (`:85-99`). FIRST verify `SCR_AIGroup.HasHeldMember` semantics against engine source; record verdict.
  - File(s): `Scripts/Game/GameMode/Deployments/Modules/OVT_VehicleSpawningDeploymentModule.c`, context.md
  - Estimate: 1.5 h

- [x] **T5.6 Delete vehicle module's group-liveness machinery**
  - Description: `CheckIfUnitsEliminated`'s group half (`:433-440`) → `OnGroupWiped`. Keep `IsVehicleOperational` (a vehicle is not a group).
  - File(s): `Scripts/Game/GameMode/Deployments/Modules/OVT_VehicleSpawningDeploymentModule.c`
  - Estimate: 0.5 h

- [x] **T5.7 Delete `OVT_EntitySpawningAPI.c` (gated on fresh grep)**
  - Description: `grep -rn "OVT_EntitySpawningAPI" Scripts/` must be comment-only first; rewrite those comments. If a concurrent session added a live caller: keep file, delete only the `:49` guard, record why.
  - File(s): `Scripts/Game/GameMode/Deployments/OVT_EntitySpawningAPI.c` (delete)
  - Estimate: 0.5 h

- [x] **T5.8 Init-tier coverage**
  - Description: Both vehicle configs resolve through the registry; crew group names resolve to real compositions. (BuildRoutePlan shape already Logic-tier, T2.3.)
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c`
  - Estimate: 1 h

---

## Phase 6: AI observers — core additive + recruit wiring (6/6 complete) — ADVANCED ✅ (code complete 2026-08-17; All owed)

- [x] **T6.1 The four observer methods on core**
  - Description: `AddEntityObserver`/`RemoveEntityObserver`/`HasEntityObserver`/`GetEntityObserverCount` beside the ambient section. Null refusal (WARNING + false); server guard + null-guard `GetObserversSystem()`; idempotent per entity; namespaced monotonic keys, never persisted/replicated; stale-entity sweep in the existing 2 s ambient tick; `OnDelete` removes every observer.
  - File(s): `Scripts/Game/GameMode/Virtualization/OVT_VirtualizationManagerComponent.c`
  - Estimate: 2 h

- [x] **T6.2 `api.md` ritual**
  - Description: Four signatures into §3 (new "AI observers" block) + §10 `integration` table rows; header's "Additively extended" note names the fourth additive change.
  - File(s): `docs/features/virtualization/core/api.md`
  - Estimate: 0.5 h

- [x] **T6.3 Core `context.md` dated additive entry**
  - Description: Under "Additive changes after the freeze": names `virtualization/integration` Phase 6, quotes T1.9 verdict, states why non-breaking.
  - File(s): `docs/features/virtualization/core/context.md`
  - Estimate: 0.25 h

- [x] **T6.4 Recruit wiring + off-switch**
  - Description: `OVT_InactiveRecruitGroupComponent` adds observer following its own group entity on server-side creation; removes in `OnDelete`. Gate behind manager attribute `m_bRecruitGroupsAreObservers` default true (D16).
  - File(s): `Scripts/Game/Components/OVT_InactiveRecruitGroupComponent.c` (path confirmed), `Scripts/Game/GameMode/Virtualization/OVT_VirtualizationManagerComponent.c` (attribute + `GetRecruitGroupsAreObservers()`)
  - Estimate: 1.5 h

- [x] **T6.5 Init-tier coverage**
  - Description: `AddEntityObserver(null)` safe false (freeze guard — most valuable case); add/has/remove/count round-trip; double-add stays 1; remove-unknown safe false. Clean up before reporting.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c`
  - Estimate: 1 h

- [x] **T6.6 Cost note in context.md**
  - Description: An observer pins every registered group in its ring materialised — feature + budget risk + off-switch rationale.
  - File(s): `docs/features/virtualization/integration/context.md`
  - Estimate: 0.25 h

---

## Phase 7: Save compatibility + persistence coverage (7/7 complete) — ADVANCED ✅ (code complete — All owed)

- [x] **T7.1 `RegisterGroup(` fixture sweep BEFORE any case** ✅
  - Description: `grep -rn "RegisterGroup(" Scripts/Game/Tests/`; verdict per site in context.md (movement's table shape). Safe = null/empty/DEFEND plan OR register+unregister in one frame.
  - File(s): context.md
  - Result: full re-sweep run before any case was written. **18 real call sites + 1 comment, every one safe, nothing changed.** Table in context.md. The three new marker fixtures register nothing at all — inert by construction (eliminated flags), not by finishing first.
  - Estimate: 0.5 h

- [x] **T7.2 Case: migrated deployment round-trips** ✅
  - Description: Config name, faction, threat, invested resources, virtual key → save/dirty/re-apply → all five back; key is the *same string* (not re-derived); `FindConfigByName` resolves.
  - File(s): `Scripts/Game/Tests/TestSuites/Persistence/OVT_TEST_PersistenceRoundTripSuite.c`
  - Result: `..._DeploymentRecord_SurvivesSaveAndReapply`. Planted key uses coordinates no marker stands at and the case asserts that precondition first, so a re-derivation anywhere is visible; `EnsureVirtualKey()` is asserted separately from the field. ⚠ The re-apply is `ApplyPersistedDeployment`, not the gate's reload seam — see the structural finding in context.md.
  - Estimate: 1.5 h

- [x] **T7.3 Case: eliminated deployment does not resurrect (G4's teeth)** ✅
  - Description: Restore with `spawnedUnitsEliminated = true` → `EnsureGroups()` registers **zero** groups.
  - File(s): `Scripts/Game/Tests/TestSuites/Persistence/OVT_TEST_PersistenceRoundTripSuite.c`
  - Result: `..._DeploymentEliminated_RegistersNoGroups`. Asserts the ordering at the **module** level (only `InitializeDeployment`'s flag-before block sets it on a fresh restore), zero registrations under the module's own owner key, and — after the reload — a re-applied payload re-marking live modules.
  - Estimate: 1 h

- [x] **T7.4 Case: version-1 payload still loads** ✅
  - Description: v1-shaped payload (no key) → restores, derives key on first use — the pre-feature-save path.
  - File(s): `Scripts/Game/Tests/TestSuites/Persistence/OVT_TEST_PersistenceRoundTripSuite.c`
  - Result: `..._DeploymentVersion1Payload_StillLoads`. Two claims: derive-once from the marker's own position, and a later keyless re-apply does **not** clobber the key the session holds. T7.7 found a real pre-feature save with 23 such records, so this path is not hypothetical.
  - Estimate: 1 h

- [x] **T7.5 Claim: deployment-owned group reclaimable after restore** ✅
  - Description: Group registered under a deployment owner key → `FindGroupsByOwner` finds it after the restore. Do NOT duplicate core's member-survival cases.
  - File(s): `Scripts/Game/Tests/TestSuites/Persistence/OVT_TEST_PersistenceRoundTripSuite.c`
  - Result: `..._DeploymentOwnedGroups_ReclaimAfterReload`. Two composed keys differing only in module tag survive verbatim (`@` and `#` included) and never cross-contaminate; also the only place the `"m" + index` fallback tag is stored and read back. A genuine storage round trip (the registry lives on the game mode). No member-survival claim duplicated.
  - Estimate: 0.75 h

- [x] **T7.6 Un-guard the Campaign-tier GM registry case** ✅
  - Description: `OVT_TEST_Campaign_GMGroupRegistry.c:74` — remove the kill-switch trivial-pass guard; if it asserts on `RADIO_TOWER_GARRISON`, re-point at `DEPLOYMENT` (§3.7). If base-upgrade producers required, restore guard + record why.
  - File(s): `Scripts/Game/Tests/TestSuites/Campaign/OVT_TEST_Campaign_GMGroupRegistry.c`
  - Result: **guard removed; nothing re-pointed** — the case never asserted on `RADIO_TOWER_GARRISON`. Producer moved to the deployment wave (~18–22 s, or ~48–52 s on the second evaluation), so `MAX_WAIT_MS` 25 s → 55 s and `timeoutS` 60 → 90, and the failure text now prints a deployment ledger. Kill-switch ledger **6 → 5**; `Scripts/Game/Tests/` is clean of the tag. Verdict + the "read the ledger before restoring the guard" rule in context.md.
  - Estimate: 0.75 h

- [x] **T7.7 Decode a real save + inspect** ✅ (one claim inspection-owed)
  - Description: Deployment records carry the key; Tower Garrison appears as ordinary deployment record; no vanilla AIGroup/AIUnit/AIWaypoint record for core-owned content; tower records unchanged in field order. Record findings.
  - File(s): context.md (findings)
  - Result: 40 blobs across 5 worlds decoded read-only. Found a **pre-feature save with 23 version-1 deployment records** (field for field, `virtualKey` count 0); the save context is **name-keyed**; tower records untouched. Two corrections: the "no `AIWaypoint` record" claim is **wrong** (records exist deliberately, `SelfSpawn 0` — the right claim is "never self-spawned back"), and **no v2 deployment payload exists in any blob yet** (the CI world saves ~1 s into the campaign, before the first evaluation), so "a stored `virtualKey` appears" is owed after the next play-test. Full findings in context.md.
  - Estimate: 1 h

---

## Phase 8: Help & documentation sync (5/5 complete) — help-docs-sync — suite skipped (docs-only) ✅ (docs-only; no suite run, per plan)

- [x] **T8.1 Fact-check tutorials + Field Manual**
  - Description: Every existing sentence about patrols/garrisons/radio towers: cite a `file:line` or cut it.
  - File(s): `Configs/Tutorials/`, `Configs/FieldManual/`
  - Result: Swept both surfaces. **No tutorial popup makes any patrol/garrison/tower claim** (`OVT-Tutorial_BasesFirstCapture_Body` is about the base QRF, an un-migrated system, and stands as written). Field Manual: **1 sentence corrected** — `OVT-FieldManual_BaseCapture_Text5`'s "Towns and radio towers are fought over in much the same way" is no longer true of towers and was rewritten. **6 stale `file:line` citations re-cited** in fact-check Comments (the dead `OVT_OccupyingFactionManager.c:508` read site and the moved `OVT_InfantrySpawningDeploymentModule.c:224` → `:416`, `OVT_BaseUpgradeDefensePosition.c:48` → `:47`, `OVT_OccupyingFactionManager:1178-1194` → `:1217-1226`) across the three difficulty descriptions, both faction descriptions and the base-capture tutorial comment; every underlying claim re-verified TRUE. Nothing had to be cut.
  - Estimate: 1 h

- [x] **T8.2 Document the four player-visible changes**
  - Description: Dead members stay dead; patrols move while away; towers taken by really clearing the garrison; towers may be ungarrisoned under resource starvation.
  - File(s): tutorials/Field Manual
  - Result: One new Field Manual entry, **"Patrols and Garrisons"** (`Configs/FieldManual/Categories/FM_Overthrow.conf`, The Resistance category, GUIDs `{6B5A11C0000000 01..09}`), carrying all four points across 8 pieces + 9 new `#OVT-FieldManual_OccupyingForces_*` keys, each with a `file:line` fact-check Comment. **No new tutorial popup**: no trigger exists that could fire one (`OVT_TutorialTrigger.c:13-44` has no tower/patrol event), recorded as a gap for the tutorial-system feature. Placeholder tile art (`default_ui.edds`) — a bespoke tile is owed.
  - Estimate: 0.75 h

- [x] **T8.3 Wiki sync**
  - Description: Same four points + operator notes (GM camera is an observer; garrisons no longer vanish during QRF).
  - File(s): wiki via wikijs MCP
  - Result: **NOT DONE — OWED.** The `wikijs` MCP tools were not exposed to this session at all (no `wikijs_connection_status` / `get_page` / `update_page` available), so nothing could be fetched or written and the crash-recovery re-fetch could not be performed either. The wiki state from the crashed session remains **unknown and unverified**. Copy for the six points is drafted in `context.md`'s Phase 8 session note, ready to paste.
  - Estimate: 0.5 h

- [x] **T8.4 Epic + master bookkeeping**
  - Description: epic-overview `integration` row + rollup; `base-defense-migration` row notes the proven seam + worked precedent; master `docs/overview.md` row; `api.md` §6 prose re `m_iMilitarySpawnDistance` readers.
  - File(s): `docs/features/virtualization/epic-overview.md`, `docs/overview.md`, `docs/features/virtualization/core/api.md`
  - Result: Epic status 3/5 → 4/5, `integration` row rewritten (62/62, Phases 1–8, All 255 green), `base-defense-migration` row now names the proven seam + the Tower Garrison config as its worked precedent (with its two recorded traps), rollup + one-line summary refreshed. Master `docs/overview.md`: the epic's **single** row updated (173/173). `api.md` §6 re-written: **one** production reader of `m_iMilitarySpawnDistance` survives (`OVT_BasePatrolUpgrade.c:96-99`, used by the defence-position / sniper-position / tower-guard upgrades); the QRF spawner reads it **not at all** — deviation from the task wording, recorded below. `api.md` §8 and `implementation.md` T7.7's "no vanilla `AIGroup`/`AIUnit`/`AIWaypoint` record" claim corrected to "no core-owned AI record is ever self-spawned back". Zero script files touched.
  - Estimate: 0.5 h

- [x] **T8.5 Kill-switch ledger**
  - Description: `grep -rn "OVT-VIRT-PLAYTEST-ONLY" Scripts/` returns exactly base upgrades ×2, QRF, switch file; ledger accounts for every removed line; tower guard removed with its block. Switch file itself stays until epic end.
  - File(s): context.md ledger
  - Result: **BALANCES.** Grep returns exactly **5** hits, and exactly the expected ones: `OVT_BaseControllerComponent.c:84`, `:298`; `OVT_QRFControllerComponent.c:369`; `OVT_VirtPlaytestKillSwitch.c:1`, `:5`. Ledger accounts for all four removals (9 → 5) and the tower guard is confirmed gone **with its enclosing block**, not un-commented. Switch file stays until epic end. Verdict recorded in `context.md`.
  - Estimate: 0.25 h

---

## Amendments (post-completion)

> Work agreed with the user **after** the feature closed at 62/62. Numbered `A<n>` on purpose: the
> phase count stays 62/62 so the build record is not retroactively rewritten, and an amendment is
> visibly a different kind of thing from a planned task.

- [x] **A1 Free-at-game-start deployments** — 2026-08-17 — ADVANCED (`component-developer-advanced`)
  - Trigger: user play-test on **Easy** found many radio towers ungarrisoned. Cause chain: garrisons are deployments bought from the faction pool ([D17](implementation.md#d17--tower-garrisons-cost-resources-like-any-other-deployment)), Easy allocates **150/tick** (`Configs/Difficulty/Difficulty_Easy.conf`), a Tower Garrison costs **50** (`Deployment_TowerGarrison.conf`), and `MAX_DEPLOYMENTS_PER_EVALUATION` (10) paces each pass. User decision: some configs are marked **free at game start**, and for now that set is **Town Patrol + Tower Garrison**.
  - Description: new `OVT_DeploymentConfig.m_bFreeAtGameStart` (bool, defvalue `"0"`); new `OVT_DeploymentManagerComponent.SeedFreeDeployments()` scheduled once from `PostGameStart()` at **+9 s** (before the existing one-shot evaluation at +10 s); the flag authored on the two shipped configs. Seeding bypasses the resource pool, the player-count guard and the QRF guard; it honours the same-name 250 m dedup, `m_iMaxInstances`, `m_iMaxDeploymentsPerFaction` and the configs' own condition modules, and ignores `m_fChance`, `m_iMinimumThreatLevel` and `MAX_DEPLOYMENTS_PER_EVALUATION`.
  - File(s): `Scripts/Game/GameMode/Deployments/OVT_DeploymentConfig.c`, `Scripts/Game/GameMode/Deployments/OVT_DeploymentManager.c`, `Configs/Deployment/Deployment_TownPatrol.conf`, `Configs/Deployment/Deployment_TowerGarrison.conf`, `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c`
  - Tests: 2 new Init-tier cases — `…_Deployments_FreeAtGameStartIsAuthored` (pure read, nothing created) and `…_Deployments_FreeSeedingIsFreeAndIdempotent` (creates real deployments; safe on **both** T7.1 grounds — inert by `SetSpawnedUnitsEliminated` **and** torn down inside one frame).
  - ⚠ Supersedes **D1's freeze** on `Deployment_TownPatrol.conf` for this one additive attribute. Recorded in `context.md`.
  - Result: Compile **0 errors** (`tools/compile-check.sh`, 6132 files). **Suite run owed** — it belongs to the orchestrator, not to the amendment.
  - Estimate: 2 h
