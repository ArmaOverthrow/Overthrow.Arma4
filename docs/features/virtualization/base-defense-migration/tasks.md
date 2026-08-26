# Base Defense Migration - Task Checklist

**Last Updated:** 2026-08-18 (Phase 8 complete; post-completion amendment A1 built)
**Progress:** 69/69 + A1 tasks complete (100%)

> Agent routing: phases 1, 2, 4, 5, 6, 7 are **ADVANCED** (`component-developer-advanced`); phase 3 is STANDARD (`component-developer`); phase 8 is `help-docs-sync`. Suite per phase: 1–7 → **All** `{6A6E2A002F53A581}`, 8 → skipped (docs-only). Source of truth: `implementation.md` §4 and the Agent Routing Summary. Core is FROZEN — `git diff Scripts/Game/GameMode/Virtualization/` must stay empty every phase (Phase 7's kill-switch file deletion is the sole sanctioned exception).

---

## Phase 1: The evaluator learns to escalate — GATE (8/8 complete) — ADVANCED

- [x] **T1.1 Read-only survey first**
  - Description: Record in context.md before editing: callers of `IsPositionSuitableForDeployment` (expect 1), callers of `FindBestDeploymentConfig` (expect 1), every authored `m_iAllowedLocationTypes` in Configs/Deployment/ (expect TOWN ×1, RADIO_TOWER ×1, BASE ×2).
  - File(s): `Scripts/Game/GameMode/Deployments/OVT_DeploymentManager.c`, `Configs/Deployment/`
  - Estimate: 0.5 h

- [x] **T1.2 Drop the blanket 100 m proximity veto**
  - Description: Remove `MIN_DEPLOYMENT_DISTANCE` rejection from `IsPositionSuitableForDeployment` (keep ground trace); delete constant if unread; rewrite comment naming the name-scoped 250 m dedup as the anti-stack rule. Must land with T1.4.
  - File(s): `Scripts/Game/GameMode/Deployments/OVT_DeploymentManager.c`
  - Estimate: 1 h

- [x] **T1.3 Raise per-faction deployment ceiling to 400 on the game-mode prefab**
  - Description: Author `m_iMaxDeploymentsPerFaction 400` on `OVT_DeploymentManagerComponent` in the prefab; class default unchanged; arithmetic in context.md (~123 needed vs 100 default).
  - File(s): `Prefabs/GameMode/OVT_OverthrowGameMode.et`
  - Estimate: 0.5 h

- [x] **T1.4 Escalation: FindBestDeploymentConfig skips configs already deployed here**
  - Description: Add `HasExistingDeploymentOfType(position, factionIndex, config.m_sDeploymentName)` to the per-config filter before priority comparison; keep caller's guard; doc block stating the escalation contract (§3.4).
  - File(s): `Scripts/Game/GameMode/Deployments/OVT_DeploymentManager.c`
  - Estimate: 1.5 h

- [x] **T1.5 New `OVT_NoPlayersNearbyConditionDeploymentModule`**
  - Description: `EvaluateStaticCondition` → `GetNearestPlayerDistance(position) >= m_fMinPlayerDistance` (default 320); `EvaluateCondition` returns true unconditionally (header says why — asymmetry prevents `m_bDeleteOnConditionFail` collecting a base when a player walks in); hand-written `CloneModule`.
  - File(s): `Scripts/Game/GameMode/Deployments/Modules/OVT_NoPlayersNearbyConditionDeploymentModule.c` (new)
  - Estimate: 1.5 h

- [x] **T1.6 Logic-tier coverage: escalation selection maths**
  - Description: New world-free Fast file (suite `OVT_TEST_LogicSuite`): lowest-priority absent pick; all-present picks nothing; ties by input order. Extract selection into a world-free static helper. No manager/world/entity/`OVT_Global` identifiers, comments included.
  - File(s): `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_BaseDefenseEscalation.c` (new)
  - Estimate: 1.5 h

- [x] **T1.7 Init-tier coverage: BASE location bit, condition module, escalation**
  - Description: `GetLocationTypeAtPosition` includes BASE at a base; condition module refuses near/accepts far; `FindBestDeploymentConfig` at a position holding config A returns config B. `spawnDistanceOverride = 0` on anything registering; owner-key-scoped assertions.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c`
  - Estimate: 2 h

- [x] **T1.8 Record survey verdicts + ceiling arithmetic + `m_iResourceAllocation` zero-reader note in context.md**
  - Description: Per T1.8 of the plan; do not delete `m_iResourceAllocation` this phase.
  - File(s): `docs/features/virtualization/base-defense-migration/context.md`
  - Estimate: 0.5 h

---

## Phase 2: New modules, providers, faction registry entries (9/9 complete) — ADVANCED

- [x] **T2.1 Additive seams on `OVT_InfantrySpawningDeploymentModule`**
  - Description: `m_bSnapToRoad` (default 1, bit-identical shipped behaviour), `protected ResolveSpawnPosition(anchor, index)`, `protected OnGroupRegistered(handle, pos)` / `OnGroupReclaimed(handle)` hooks; copy `m_bSnapToRoad` in `CloneModule`; audit the existing 12-attribute copy list and record verdict.
  - File(s): `Scripts/Game/GameMode/Deployments/Modules/OVT_InfantrySpawningDeploymentModule.c`
  - Estimate: 2 h

- [x] **T2.2 `OVT_DeploymentPlacementProvider` + three shipped providers**
  - Description: Base `[BaseContainerProps]` class returning `array<ref OVT_DeploymentPlacement>` (position+yaw); `OVT_TowerCoverPostPlacementProvider` (MDT_TOWER walkway posts, WALKWAY_OFFSET "0 0 1.5", glass-cabin reason in header), `OVT_SniperMarkerPlacementProvider` (per-marker `m_iMinimumThreat` filter, rotation preserved), `OVT_BaseDefendPositionPlacementProvider` (nearest base's `m_aDefendPositions`).
  - File(s): `Scripts/Game/GameMode/Deployments/Modules/` (new files)
  - Estimate: 3 h

- [x] **T2.3 `OVT_PlacedInfantrySpawningDeploymentModule`**
  - Description: Exact placement for N groups, re-applied on every materialisation; `GetOnAgentAdded` handler takes ONE arg (`agent.GetParentGroup()`); `Remove` then `Insert` on every reclaim; arrival counter resets per materialisation (group's own spawn notification, or modulo fallback).
  - File(s): `Scripts/Game/GameMode/Deployments/Modules/OVT_PlacedInfantrySpawningDeploymentModule.c` (new)
  - Estimate: 3 h

- [x] **T2.4 `OVT_CompositionSpawningDeploymentModule` + `OVT_EDeploymentSlotType`**
  - Description: Slot pick (30 tries, skip `m_aSlotsFilled`), `SpawnEntityPrefabMatrix` on slot transform, `OVT_PersistenceTracking.Track`, `OVT_NavmeshRebuild.RebuildNow`, `SpawnDefaultOccupants({TURRET})` 7/15/23 m, optional ammo-box fill; slot marked filled only after successful spawn; `WasRestoredFromSave()` gate.
  - File(s): `Scripts/Game/GameMode/Deployments/Modules/OVT_CompositionSpawningDeploymentModule.c` (new)
  - Estimate: 3 h

- [x] **T2.5 `OVT_ParkedVehicleSpawningDeploymentModule`**
  - Description: Port of `OVT_BaseUpgradeParkedVehicles.c:74-110` on `OVT_BaseSpawningDeploymentModule`; vehicle registry names; `m_Parking` + `GetParkingSpot` + `SpawnVehicleMatrix`; restored deployment spawns none.
  - File(s): `Scripts/Game/GameMode/Deployments/Modules/OVT_ParkedVehicleSpawningDeploymentModule.c` (new)
  - Estimate: 2 h

- [x] **T2.6 `WasRestoredFromSave()` on `OVT_DeploymentComponent`**
  - Description: Runtime-only bool set in `ApplyPersistedDeployment`, never persisted; header cites D7. Do NOT touch serializer/version/field order.
  - File(s): `Scripts/Game/GameMode/Deployments/OVT_DeploymentComponent.c`
  - Estimate: 0.5 h

- [x] **T2.7 Faction registry entries (both factions, appended)**
  - Description: Groups `heavy_infantry`, `at_team`, `sniper`, `sniper_team`, `bunker_team` (prefabs verbatim from legacy arrays; record picks); vehicles `car`, `truck`; compositions `MediumCheckpoint` (cost 40), `LargeCheckpoint` (cost 60). Fresh repo-unique GUID prefix, grep-verified.
  - File(s): `Configs/Factions/USSR_OverthrowData.conf`, `Configs/Factions/US_OverthrowData.conf`
  - Estimate: 2 h

- [x] **T2.8 Init-tier coverage: registry resolution, snap-to-road, providers, clone fidelity**
  - Description: Every new registry name resolves for both factions; `m_bSnapToRoad 0` stays within `m_fSpawnRadius`; providers return empty (non-null) lists where nothing qualifies; every new module's clone carries every authored attribute.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c`
  - Estimate: 2 h

- [x] **T2.9 context.md: CloneModule audit verdict, prefab picks, ReleaseAction not-ported note**
  - File(s): `docs/features/virtualization/base-defense-migration/context.md`
  - Estimate: 0.5 h

---

## Phase 3: Garrison patrols — three configs, two classes deleted (8/8 complete) — STANDARD

- [x] **T3.0 (added by the orchestrator — decision S1) OR the `BASE` bit in within 250 m of a base centre**
  - Description: Towns are tested first with a 500 m radius and shadowed 4 of Eden's 10 bases (and the Init world's only base). `GetLocationTypeAtPosition()` now ORs `BASE` in via the new `IsNearBaseCentre()` / `BASE_CLASSIFICATION_RADIUS = 250`, deliberately equal to `HasExistingDeploymentOfType()`'s dedup radius so force doubling is geometrically impossible. TOWN/RADIO_TOWER logic untouched.
  - File(s): `Scripts/Game/GameMode/Deployments/OVT_DeploymentManager.c`
  - Estimate: 1 h

- [x] **T3.1 Author `Deployment_BaseGarrisonPatrol.conf`**
  - Description: `light_patrol`, `m_bSnapToRoad 0`, `m_fSpawnRadius 50`, 1–2 groups, PERIMETER r=280, Reinforcement (`m_bDeleteOnConditionFail 1`), BaseControl (`m_fMaxDistance 500`), NoPlayersNearby; OCCUPYING_FACTION / BASE / prio 1 / chance 100 / maxInstances -1 / free 0.
  - File(s): `Configs/Deployment/Deployment_BaseGarrisonPatrol.conf` (new)
  - Estimate: 1.5 h

- [x] **T3.2 Author `Deployment_BaseHeavyPatrol.conf` + `Deployment_BaseATSection.conf`**
  - Description: `heavy_infantry` threat≥25 prio 5; `at_team` threat≥50 prio 6. Legacy "first group is AT" quirk deliberately NOT reproduced — record divergence.
  - File(s): `Configs/Deployment/Deployment_BaseHeavyPatrol.conf`, `Configs/Deployment/Deployment_BaseATSection.conf` (new)
  - Estimate: 1 h

- [x] **T3.3 Append three registry entries to `overthrowDeployments.conf`**
  - File(s): `Configs/Deployment/overthrowDeployments.conf`
  - Estimate: 0.5 h

- [x] **T3.4 Delete `OVT_BaseUpgradeDefensePatrol.c` + its conf entry (same commit)**
  - File(s): `Scripts/Game/Controllers/OccupyingFaction/BaseUpgrades/OVT_BaseUpgradeDefensePatrol.c`, `Configs/BaseUpgrades/overthrowBaseUpgrades.conf`
  - Estimate: 0.5 h

- [x] **T3.5 Delete `OVT_BaseUpgradeTownPatrol.c` + `RecoverResources()`**
  - Description: Dead code; `RecoverResources`' only caller dies with it — grep-prove or record survivor.
  - File(s): `Scripts/Game/Controllers/OccupyingFaction/BaseUpgrades/OVT_BaseUpgradeTownPatrol.c`, `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c`
  - Estimate: 0.5 h

- [x] **T3.6 Init-tier: three configs resolve, validate, build non-empty cycling PERIMETER plans**
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c`
  - Estimate: 1 h

- [x] **T3.7 context.md: retired-symbol grep verdicts + T3.2 divergence**
  - File(s): `docs/features/virtualization/base-defense-migration/context.md`
  - Estimate: 0.5 h

---

## Phase 4: Exact placement — defense positions, tower guards, sniper positions (10/10 complete) — ADVANCED

- [x] **T4.1 Author `Deployment_BaseDefensePositions.conf`**
  - Description: Placed-infantry + `OVT_BaseDefendPositionPlacementProvider`, `heavy_infantry`, max 5 groups, cost 60/group, DEFEND behaviour, Reinforcement, BaseControl, NoPlayersNearby, prio 2.
  - File(s): `Configs/Deployment/Deployment_BaseDefensePositions.conf` (new)
  - Estimate: 1 h

- [x] **T4.2 Author `Deployment_BaseTowerGuards.conf`**
  - Description: Placed-infantry + tower provider, `sniper`, HIGH importance, cost 15/group, NO behaviour module (legacy parity), prio 2.
  - File(s): `Configs/Deployment/Deployment_BaseTowerGuards.conf` (new)
  - Estimate: 1 h

- [x] **T4.3 Author `Deployment_BaseSniperPositions.conf`**
  - Description: Placed-infantry + sniper-marker provider, `sniper_team`, HIGH, cost 30/group, no behaviour module, prio 2; per-marker threat gate in provider.
  - File(s): `Configs/Deployment/Deployment_BaseSniperPositions.conf` (new)
  - Estimate: 1 h

- [x] **T4.4 Append three registry entries**
  - File(s): `Configs/Deployment/overthrowDeployments.conf`
  - Estimate: 0.25 h

- [x] **T4.5 Delete `OVT_BaseUpgradeDefensePosition.c`, `OVT_BaseUpgradeTowerGuard.c`, `OVT_BaseUpgradeSniperPosition.c` + their 3 conf entries (same commit)**
  - File(s): `Scripts/Game/Controllers/OccupyingFaction/BaseUpgrades/`, `Configs/BaseUpgrades/overthrowBaseUpgrades.conf`
  - Estimate: 0.5 h

- [x] **T4.6 Testable placement seam: `ResolvePlacements` + `PlacementForArrival` pure methods**
  - Description: Expose placement decision as pure methods so re-materialisation stability is Init-assertable without a live deployment marker.
  - File(s): `Scripts/Game/GameMode/Deployments/Modules/OVT_PlacedInfantrySpawningDeploymentModule.c`
  - Estimate: 1.5 h

- [x] **T4.7 Init-tier: configs resolve/validate; plan shapes; arrival stability; sniper threat filter**
  - Description: Tower/sniper → null plans; defense positions → one-point DEFEND non-cycling; `PlacementForArrival` stable across two materialisations and wraps; provider filters over-threat marker.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c`
  - Estimate: 2 h

- [x] **T4.8 Persistence-tier: base-defense deployment round trip**
  - Description: Config name, faction, threat, invested resources, virtual key survive save → dirty → re-apply; assert the restore half (`ApplyPersistedDeployment`) — the seam cannot reload a marker; say so at fixture top. Do not widen the seam.
  - File(s): `Scripts/Game/Tests/TestSuites/Persistence/OVT_TEST_PersistenceRoundTripSuite.c`
  - Estimate: 2 h

- [x] **T4.9 Fixture discipline: `SetSpawnedUnitsEliminated(true)` everywhere + `RegisterGroup(` sweep**
  - Description: Deployment + every spawning module marked eliminated before anything ticks; re-grep `RegisterGroup(` in Tests and record per-site verdict table.
  - File(s): `Scripts/Game/Tests/`
  - Estimate: 1 h

- [x] **T4.10 context.md: placement verdicts + ReleaseAction note with play-test replacement check**
  - File(s): `docs/features/virtualization/base-defense-migration/context.md`
  - Estimate: 0.5 h

---

## Phase 5: Static content — checkpoints, fortifications, parked vehicles (8/8 complete) ✅ — ADVANCED

- [x] **T5.1 Author `Deployment_BaseCheckpoints.conf`**
  - Description: Two composition modules (Large on ROAD_LARGE, Medium on ROAD_MEDIUM), each with `light_patrol` guard, `m_bSnapToRoad 0`; DEFEND; Reinforcement; BaseControl; NoPlayersNearby; prio 3.
  - File(s): `Configs/Deployment/Deployment_BaseCheckpoints.conf` (new)
  - Estimate: 1.5 h

- [x] **T5.2 Author `Deployment_BaseFortifications.conf`**
  - Description: Three composition modules (SmallBunker + `bunker_team` guard, AmmoCache + `m_bFillAmmoBoxes 1`, MGNest), all SMALL slots; prio 4.
  - File(s): `Configs/Deployment/Deployment_BaseFortifications.conf` (new)
  - Estimate: 1.5 h

- [x] **T5.3 Author `Deployment_BaseParkedVehicles.conf`**
  - Description: Parked-vehicle module, `truck` ×1, cost 90, prio 10; NO reinforcement (never collected — deliberate, note the trap).
  - File(s): `Configs/Deployment/Deployment_BaseParkedVehicles.conf` (new)
  - Estimate: 1 h

- [x] **T5.4 Append three registry entries**
  - File(s): `Configs/Deployment/overthrowDeployments.conf`
  - Estimate: 0.25 h

- [x] **T5.5 Delete `OVT_BaseUpgradeCheckpoints.c`, `OVT_BaseUpgradeComposition.c`, `OVT_BaseUpgradeParkedVehicles.c`, `OVT_SlottedBaseUpgrade.c` + their 5 conf entries (same commit)**
  - Description: `OVT_SlottedBaseUpgrade` has no other subclass — grep-prove. Conf then holds only Specops.
  - File(s): `Scripts/Game/Controllers/OccupyingFaction/BaseUpgrades/`, `Configs/BaseUpgrades/overthrowBaseUpgrades.conf`
  - Estimate: 0.75 h

- [x] **T5.6 Init-tier: no-double-build claim at the seam**
  - Description: Restored deployment → composition module spawns/registers nothing; fresh deployment → builds once, second call builds nothing (pure predicate, T4.6 shape).
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c`
  - Estimate: 1.5 h

- [x] **T5.7 Init-tier: slot bookkeeping**
  - Description: Claim lands in nearest controller's `m_aSlotsFilled`; a filled slot is never selected.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c`
  - Estimate: 1 h

- [x] **T5.8 context.md: parked-vehicles never-collected note + composition persistence decode**
  - File(s): `docs/features/virtualization/base-defense-migration/context.md`
  - Estimate: 0.5 h

---

## Phase 6: One funding path, specops drop, legacy save conversion (10/10 complete) ✅ — ADVANCED (highest risk)

- [x] **T6.1 Read-only survey: `m_iResources` writers/readers, `AddFactionResources` callers**
  - Description: Record verdicts in context.md. Do NOT touch serializer field order, `RplSave`/`RplLoad`, or `ApplyPersistedOccupyingFaction` structure.
  - File(s): `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c` (+readers)
  - Estimate: 1 h

- [x] **T6.2 Delete the base-spend loop in `CheckUpdate`**
  - Description: Keep `GainResources()` + 80 % computation; replace sorted-bases loop with unconditional `AllocateDeploymentResources(toSpend); m_iResources -= toSpend;`. Keep threat decay, counter-attack, both early returns. `UpdateKnownTargets` only goes if specops was its sole consumer.
  - File(s): `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c`
  - Estimate: 2 h

- [x] **T6.3 Fold initial distribution into one pool credit**
  - Description: Delete `DistributeInitialResources()` + its CallLater; `NewGameStart` credits `baseResourcesPerTick + Σ(startingResources × multiplier)` once; keep +5 s CallLater if base discovery ordering requires (comment why); `m_bDistributeInitial` keeps meaning (Continue does not re-seed).
  - File(s): `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c`
  - Estimate: 2 h

- [x] **T6.4 Delete `AllocateDeploymentResourcesIfNeeded()`; keep `AllocateDeploymentResources()` as the single credit point (header)**
  - File(s): `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c`
  - Estimate: 0.5 h

- [x] **T6.5 The specops drop (D3)**
  - Description: Delete `OVT_BaseUpgradeSpecops.c`, its conf entry (file now empty of entries), `UpdateSpecops()` + both call sites; verify `StartBaseQRF`'s other two callers survive; decide `m_aKnownTargets`/`UpdateKnownTargets` fate explicitly; loss list into context.md.
  - File(s): `Scripts/Game/Controllers/OccupyingFaction/BaseUpgrades/OVT_BaseUpgradeSpecops.c`, `Configs/BaseUpgrades/overthrowBaseUpgrades.conf`, `OVT_OccupyingFactionManager.c`
  - Estimate: 1.5 h

- [x] **T6.6 The save conversion (§3.6)**
  - Description: `ApplyPersistedBaseUpgrades` sums (resources + groups × `LEGACY_GROUP_VALUE`) and credits pool ONCE after all bases read; delete upgrade-replay block in `InitBaseControllers`, keep `slotsFilled` restore verbatim; `WriteBase` writes empty `upgrades` array; payload classes stay declared and read.
  - File(s): `OVT_OccupyingFactionManager.c`, `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManagerSerializer.c`
  - Estimate: 2.5 h

- [x] **T6.7 Logic-tier: conversion maths**
  - Description: World-free helper: sum over records; empty → 0; composition-shaped → 0; converted set → 0 (idempotence).
  - File(s): `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_BaseDefenseConversion.c` (new)
  - Estimate: 1.5 h

- [x] **T6.8 Persistence-tier: conversion path**
  - Description: Legacy records → pool rises by exactly the computed value, `upgrades` left empty; second pass adds nothing. Restore-half assertions only.
  - File(s): `Scripts/Game/Tests/TestSuites/Persistence/OVT_TEST_PersistenceRoundTripSuite.c`
  - Estimate: 2 h

- [x] **T6.9 Init-tier: 80 % transfer conserves total; opening seed lands in pool**
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c`
  - Estimate: 1 h

- [x] **T6.10 context.md: T6.1 verdicts, conserved-total claim, `LEGACY_GROUP_VALUE` derivation, specops loss list**
  - File(s): `docs/features/virtualization/base-defense-migration/context.md`
  - Estimate: 0.5 h

---

## Phase 7: The retirement sweep (11/11 complete) — ADVANCED

- [x] **T7.1 Read-only sweep: disposition table for every surviving legacy reference**
  - Description: `OVT_BaseUpgrade*`, `m_aBaseUpgrades`, `m_BaseUpgradesConfig`, `FindUpgrade`, `GetRandomGroupByType`, eight legacy faction attributes, `m_iMilitarySpawnDistance`, `baseResourceCost` — a disposition per entry BEFORE deleting anything.
  - File(s): repo-wide grep → `docs/features/virtualization/base-defense-migration/context.md`
  - Estimate: 1.5 h

- [x] **T7.2 Delete the framework tier**
  - Description: `OVT_BaseUpgrade.c`, `OVT_BasePatrolUpgrade.c`, whole `BaseUpgrades/` dir, `OVT_BaseUpgradesConfig.c`, `Configs/BaseUpgrades/` (+ .meta), `m_BaseUpgradesConfig` ref in `OVT_BaseController.et`.
  - File(s): `Scripts/Game/Controllers/OccupyingFaction/BaseUpgrades/`, `Scripts/Game/Configuration/OVT_BaseUpgradesConfig.c`, `Configs/BaseUpgrades/`, `Prefabs/Controllers/OVT_BaseController.et`
  - Estimate: 1.5 h

- [x] **T7.3 Shrink the base controller (guards leave INSIDE deletions — never un-comment)**
  - Description: Delete `m_aBaseUpgrades`, `m_BaseUpgradesConfig`, InitializeBase copy loop, `UpdateUpgrades()` + CallLater, `FindUpgrade()`, `SpendResources()`. KEEP slot registry, `FindSlots`/`FindParking`, `GetNearestSlot`, `GetRandomVehiclePatrolSpawn`, faction/flag half.
  - File(s): `Scripts/Game/Controllers/OccupyingFaction/OVT_BaseControllerComponent.c`
  - Estimate: 2 h

- [x] **T7.4 Restore the QRF spawn queue (delete the guard line ONLY)**
  - Description: Opposite operation from T7.3 — deliberate asymmetry, assert in context.md. Diff must be exactly one deleted line.
  - File(s): `Scripts/Game/Controllers/OccupyingFaction/OVT_QRFControllerComponent.c`
  - Estimate: 0.5 h

- [x] **T7.5 Delete the kill switch — EPIC END**
  - Description: `OVT_VirtPlaytestKillSwitch.c` deleted; `grep -rn "OVT-VIRT-PLAYTEST-ONLY\|DISABLE_LEGACY_AI_SPAWNS" Scripts/` → nothing.
  - File(s): `Scripts/Game/GameMode/Virtualization/OVT_VirtPlaytestKillSwitch.c`
  - Estimate: 0.5 h

- [x] **T7.6 Retire the legacy faction path (D9)**
  - Description: Re-point `OVT_SpawnGroupJobStage.c:30` at `m_GroupRegistry` FIRST; then delete `GetRandomGroupByType()`, the eight attributes AND every authored value (both faction confs + `OVT_FactionManager.et`) in one commit; retire checkpoint prefab attributes if moved; re-point the two Init-suite fixture fallbacks; sweep the five unauthored/unread arrays if T7.1 confirms.
  - File(s): `Scripts/Game/Faction/OVT_Faction.c`, `Scripts/Game/Jobs/…OVT_SpawnGroupJobStage.c`, `Configs/Factions/*.conf`, `Prefabs/GameMode/OVT_FactionManager.et`, `OVT_TEST_InitSuite.c`
  - Estimate: 2.5 h

- [x] **T7.7 Delete `m_iMilitarySpawnDistance`**
  - Description: Last production reader died in T7.2; not authored anywhere; absent from RplSave/RplLoad. Rewrite 3 citing comments + 1 test diagnostic. If a reader survives, keep and record why.
  - File(s): `Scripts/Game/Components/OVT_OverthrowConfigComponent.c` (+comment sites)
  - Estimate: 1 h

- [x] **T7.8 Re-point the GM snapshot (G9)**
  - Description: `BuildBases()` enumerates deployments near the base (`m_sType` = config name, `m_iResources` = invested, `m_iGroups` = registered count); add minimal accessor (e.g. `GetRegisteredGroupCount()`); record classes / field order / RPC untouched.
  - File(s): `Scripts/Game/GameMode/GM/OVT_GMSnapshotBuilder.c`, `Scripts/Game/GameMode/Deployments/Modules/OVT_BaseSpawningDeploymentModule.c`
  - Estimate: 1.5 h

- [x] **T7.9 Tests and enums**
  - Description: Delete `OVT_TEST_Campaign_BaseUpgrades_ConfigLoaded.c`; fix `OVT_TEST_Campaign_GMGroupRegistry.c` header; LEAVE `OVT_EGroupOrigin` values declared (wire integers); verify `OVT_TEST_Logic_GMIconFormat.c:53` needs no change.
  - File(s): `Scripts/Game/Tests/`
  - Estimate: 1 h

- [x] **T7.10 Comment re-words**
  - Description: `OVT_OccupyingFactionManager.c:440-443` (stale reason) and `OVT_OccupyingFactionManagerSerializer.c:92` (stale line citation).
  - File(s): as above
  - Estimate: 0.5 h

- [x] **T7.11 context.md: T7.1 disposition table + final kill-switch ledger**
  - File(s): `docs/features/virtualization/base-defense-migration/context.md`
  - Estimate: 0.5 h

---

## Phase 8: Help & documentation sync (5/5 complete) ✅ — help-docs-sync

- [x] **T8.1 Fact-check every base/garrison/fortification sentence in Tutorials + FieldManual against shipped code (cite file:line or cut)**
  - File(s): `Configs/Tutorials/`, `Configs/FieldManual/`
  - Estimate: 1 h

- [x] **T8.2 Document the player-visible changes**
  - Description: Defense persists as you left it; concern-by-concern fortification (not while watched); light defense after capture / when starved; posts re-manned after respawn; specops raids gone.
  - File(s): `Configs/Tutorials/`, `Configs/FieldManual/`
  - Estimate: 1 h

- [x] **T8.3 Wiki sync: same points + two operator notes (single pool; `m_iMaxDeploymentsPerFaction` ceiling)**
  - File(s): wiki via wikijs MCP
  - Estimate: 0.5 h
  - 🔴 **NOT PUBLISHED — the `mcp__wikijs__*` tools were unavailable to the Phase 8 agent (tool-availability gap, not a wiki outage; nothing was written and no page was left half-edited). The exact content to publish, page-selection guidance and the known MCP write hazards are written out in `context.md` under "T8.3". Owed to a follow-up session.**

- [x] **T8.4 Epic bookkeeping: epic-overview 5/5, master overview, api.md §6 `m_iMilitarySpawnDistance` note**
  - File(s): `docs/features/virtualization/epic-overview.md`, `docs/overview.md`, `docs/features/virtualization/core/api.md`
  - Estimate: 0.5 h

- [x] **T8.5 The epic's closing ledger: grep output showing OVT-VIRT-PLAYTEST-ONLY returns nothing, switch file deleted**
  - File(s): `docs/features/virtualization/base-defense-migration/context.md`, epic docs
  - Estimate: 0.25 h

---

## Amendment A1: authored base perimeters, free garrisons, AT road overwatch (8/8 complete) ✅ — post-completion, 2026-08-18 — ADVANCED

> Raised by the user from the §6 play-test (all groups spawned in the right places; the *waypoints*
> were the problem). Listed separately from the 69: the feature was already Ready for Review when this
> landed. Suite run owed to the orchestrator. **Superseded mid-task by a user design change**: the
> original "gate PERIMETER on base proximity" became two explicit enum members.

- [x] **A1.1 Garrison patrol and tower guards are free at game start**
  - Description: `m_bFreeAtGameStart 0` → `1`. Verdict recorded in context.md: the free-seed pass DOES ask `NoPlayersNearby`'s static gate, which passes with nobody connected (dedicated server) and otherwise defers that one base to the evaluator, which asks the same gate.
  - File(s): `Configs/Deployment/Deployment_BaseGarrisonPatrol.conf`, `Configs/Deployment/Deployment_BaseTowerGuards.conf`
  - Estimate: 0.25 h

- [x] **A1.2 Authored patrol square on the base controller**
  - Description: `m_fPerimeterRadius` (280 = `baseRange`) and `m_fPerimeterRotation` (0°) beside the QRF attack-direction attributes, plus a shared `FindNearestBaseControllerWithin()` and a pure `PerimeterCorner()` the viz and the plan both use.
  - File(s): `Scripts/Game/Controllers/OccupyingFaction/OVT_BaseControllerComponent.c`
  - Estimate: 0.5 h

- [x] **A1.3 `BuildSquarePerimeterPlan()` on the plan factory**
  - Description: world-free; 4 corners at `rotationDeg + k*90`, the START corner still following the walker's bearing; `StartCornerIndex` + `NormalizeDegrees` as pure protected helpers.
  - File(s): `Scripts/Game/GameMode/Deployments/OVT_VirtualPlanFactory.c`
  - Estimate: 1 h

- [x] **A1.4 `OVT_PatrolType.PERIMETER_BASE` (APPENDED) walks the authored square**
  - Description: new enum member; `BuildAuthoredSquarePlan()` looks the base up at `BASE_CLASSIFICATION_RADIUS`, applies `PERIMETER_ROTATION_JITTER_DEG = 10` fresh per plan, ground-snaps and **never road-snaps**; WARNING + un-authored fallback when no base is in range. Plain `PERIMETER` untouched. Every `OVT_PatrolType.` comparison site given a verdict in context.md.
  - File(s): `Scripts/Game/GameMode/Managers/OVT_OverthrowConfigComponent.c`, `Scripts/Game/GameMode/Deployments/Modules/OVT_PatrolBehaviorDeploymentModule.c`, `Configs/Deployment/Deployment_BaseGarrisonPatrol.conf`, `Configs/Deployment/Deployment_BaseHeavyPatrol.conf`
  - Estimate: 2 h

- [x] **A1.5 Workbench debug viz for the square**
  - Description: cyan `CreateLinesLoop` square at the authored rotation + two faint ±10° squares + a start arrow at corner 0, inside the existing `#ifdef WORKBENCH` block and on the same `CALL_WHEN_ENTITY_SELECTED` condition as the QRF arrows.
  - File(s): `Scripts/Game/Controllers/OccupyingFaction/OVT_BaseControllerComponent.c`
  - Estimate: 1 h
  - 🔧 **CRASHED WORKBENCH ON FIRST TEST, FIXED SAME DAY.** The vertex buffers were method LOCALS and the `Shape.CreateLines` family REFERENCES the caller's array rather than copying it, so the render thread read a dead stack frame (jittering vertices, then a crash). Now class members sized exactly to `num`, drawn with plain `CreateLines` + a 5-point closed strip, copying `SCR_PowerLineJointEntity.c:22,163`. Full record + the standing rule in context.md.
  - 🔴 **NEEDS A SECOND WORKBENCH LOOK after the fix — not automatable.** See "Needs Human Verification" below.

- [x] **A1.6 AT sections become placed road overwatch**
  - Description: new `OVT_RoadSlotOverwatchPlacementProvider` (large + medium road slots, `m_fSideOffset` 15 m across the slot's own right vector, side a pure parity of the slot position, post faces back at the slot, `m_aSlotsFilled` deliberately not consulted); `Deployment_BaseATSection.conf` rewritten to placed + DEFEND. No `CloneModule` on the provider — `m_Placement` is shared by reference, by design.
  - File(s): `Scripts/Game/GameMode/Deployments/Modules/OVT_RoadSlotOverwatchPlacementProvider.c` (new), `Configs/Deployment/Deployment_BaseATSection.conf`
  - Estimate: 2 h

- [x] **A1.7 Tests: 1 new Logic case, 1 new Init case, 3 Init cases extended**
  - Description: Logic `..._SquarePerimeterPlan` (corner maths, rotation obeyed, negative rotations fold, only the start corner follows the walker); Init `..._RoadSlotOverwatchIsOffsetAndStable` (offset across the slot, facing, order-independent side, authored 15, repeatable live resolve); Init `..._BasePatrolConfigsCyclePerimeter` rewritten (PERIMETER_BASE by name + live authored-square geometry + the AT section's new shape); `..._TownPatrolPlanCycles`, `..._PlacementProvidersAnswerEmptyNotNull`, `..._FreeAtGameStartIsAuthored` extended. **Persistence suite untouched.**
  - File(s): `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_DeploymentVirtualization.c`, `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c`
  - Estimate: 2 h

- [x] **A1.8 Docs: the A1 record in context.md and this section**
  - File(s): `docs/features/virtualization/base-defense-migration/context.md`, `docs/features/virtualization/base-defense-migration/tasks.md`
  - Estimate: 0.5 h

---

## Bugs & Issues

**Active Bugs:**
- (none — in-dev defects go here, not create_bug)

**Fixed Bugs:**
- (none yet)

---

## Technical Debt

- [ ] 💳 **Composition destroyed-while-saved is never rebuilt** — Priority: Low
  - Description: Inherited from legacy (D7). Upgrade path designed (serializer v3 append-only slot positions) and deliberately not built (YAGNI).
  - Effort: 2-3 h if play-test shows visible base decay

---

## Needs Human Verification

- [ ] §6 Manual play-test steps 1–14 (fortification pacing, walkway placement, legacy-save load, two campaigns, QRF, GM panel) — keep a **pre-migration save before Phase 6**
- [ ] Dedicated-server / MP pass (uncovered by the automated spine)
- [ ] Play-test check replacing `ReleaseAction()` housekeeping: leaked `IsActionAccessible() == false` posts
- [ ] 🔴 **A1.5 — RE-TEST the Workbench perimeter viz after the crash fix.** It is `#ifdef WORKBENCH` and draws only while the entity is selected, so no suite can reach it. Open a world layer in Workbench, select a base marker, and confirm: one solid cyan square at `m_fPerimeterRadius`, two fainter cyan squares at ±10°, a short cyan arrow to corner 0 — **and no crash, and no vertex jitter**. Jitter returning would mean a buffer is being copied on the way to the call; an open "C" instead of a square would mean the closing repeat point was lost.
- [ ] 🟡 **Pre-existing, found by the A1 crash sweep, deliberately not touched:** `Scripts/Game/Entities/OVT_StartCameraPos.c:32,54` passes a method-LOCAL `vector points[12]` to `Shape.CreateTris` from `_WB_AfterWorldUpdate` — the same buffer-lifetime hazard that crashed the perimeter viz. Three-line fix (move to a member). ⚠ Selecting a start-camera-pos entity during the A1 re-test could crash for this reason and look like the perimeter fix failing.
- [ ] A1 — authoring pass: set `m_fPerimeterRadius` / `m_fPerimeterRotation` per base. Every base ships at the class defaults (280 m / 0°), which is parity with the old `baseRange` patrol radius, so nothing regresses if this is never done.
- [ ] A1 — play-test the AT posts: beside a road slot rather than in it, looking at the road, and back on the same side after a despawn/re-materialise cycle.

---

*Update this file as tasks are completed. Mark tasks with ✅ immediately when done. Add new tasks as they're discovered.*
