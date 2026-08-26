# Base Defense Migration — Implementation Plan

**Status:** Ready for Review
**Started:** 2026-08-17
**Completed:** 2026-08-18 (all 8 phases; All suite 278 green; kill-switch ledger empty)
**Last Updated:** 2026-08-18 (built via /autorun-feature; see context.md for the per-phase record, decision S1, and the play-test list owed)

**Epic:** `virtualization` (feature #5 of 5, the last — see `docs/features/virtualization/epic-overview.md`)
**Requirements:** `docs/features/virtualization/base-defense-migration/requirements.md` — authoritative.
**Consumes:** `docs/features/virtualization/core/api.md` — 🔒 **FROZEN**. §10's `integration` table is the entire surface this feature programs against. **This feature asks core for nothing new.**
**Consumes:** `docs/features/virtualization/integration/implementation.md` §3.3 (the worked precedent), §3.2 (owner keys), §3.5 (idempotent `EnsureGroups`), [D7](../integration/implementation.md#d7--the-manager-subscribes-the-modules-never-do) (the manager subscribes, modules never do).
**Consumes:** `docs/features/virtualization/integration/context.md` — its Phase 3/4/5/7 gotchas bind this plan and are cited inline.

> **Why this feature is last, and what it actually is.** Four features built the layer and put the campaign's *mobile* AI on it. Base defense is the one systemic force still running on its own ad-hoc virtualization — the "banked value proxying" in `OVT_BasePatrolUpgrade`, where a despawned patrol becomes an integer and comes back at full strength as a different set of men. It is also the second half of a **dual funding system**: `OVT_OccupyingFactionManager` splits 80 % of every resource tick across bases and calls `base.SpendResources()`, while separately dripping surplus into the deployments pool. This feature deletes the second spender, deletes the third virtualization, and re-expresses nine base-upgrade classes as **nine shipped deployment configs** on the layer — copying the shape `Deployment_TowerGarrison.conf` proved rather than re-deriving it.
>
> **It is also the epic's closing act.** The kill switch (`OVT_VirtPlaytestKillSwitch.c`) and its last three production guards leave with this feature. After it, `grep -rn "OVT-VIRT-PLAYTEST-ONLY" Scripts/` returns nothing.

---

## 1. Executive Summary

Ten base-upgrade classes (nine registered in `Configs/BaseUpgrades/overthrowBaseUpgrades.conf`, plus one dead) place and maintain every static defense the occupying faction owns: roaming garrison patrols, heavy defense emplacements, tower-top snipers, sniper-marker teams, road checkpoints, three slotted fortification compositions, and parked vehicles. They are funded by a spend path that exists nowhere else in the codebase — `OVT_OccupyingFactionManager.CheckUpdate()` hands each base a budget, `OVT_BaseControllerComponent.SpendResources()` walks upgrade priorities 1..19, and each upgrade decides for itself how to convert money into men.

That system has three structural problems this feature ends:

1. **It is a third virtualization.** `OVT_BasePatrolUpgrade.CheckUpdate()` runs its own ~10 s timer, deletes a base's groups when no player is within `m_iMilitarySpawnDistance`, and banks `agents × baseResourceCost` into an integer. The men who come back are new men. This is the BUG-029 drift family by construction, and it is **not** covered by the epic kill switch — that timer is installed from `PostInit` and still runs today.
2. **It is a second budget.** Two independent spenders draw on one economy through a hand-written bridge (`AllocateDeploymentResourcesIfNeeded`, which only tops the deployment pool up when it is below 500 *and* the reserve is above 1000), plus a **third** path nobody counts: `DistributeInitialResources()` conjures `startingResources × multiplier` per base at +5 s and spends it directly, never touching `m_iResources`.
3. **It resolves compositions through the legacy prefab-slot arrays.** `OVT_Faction.m_aGroupInfantryPrefabSlots` / `m_aHeavyInfantryPrefabSlots` / `m_aGroupATPrefabSlots` / `m_aGroupSpecialPrefabSlots` / `m_aGroupSniperPrefab` / `m_aGroupSniperTeamPrefab` and `GetRandomGroupByType()` — the path `api.md` §3 forbids consumers from using, because core resolves `(factionKey, groupName)` and never a raw prefab.

After this feature: base defense is **nine deployment configs**, its groups are core-registered (dead members stay dead, across despawn and across save/load), its money comes from the deployments pool and nowhere else, and the base-upgrades directory, its config, its test and its faction-prefab path are gone.

Seven things shape the work and are built into the phases rather than discovered later:

1. **The evaluator cannot currently put two deployments at one place, and per-concern configs need exactly that.** `FindDeploymentCandidates` filters every candidate through `IsPositionSuitableForDeployment`, which vetoes any position within `MIN_DEPLOYMENT_DISTANCE` (100 m) of **any** existing deployment (`OVT_DeploymentManager.c:860-862`); and `FindBestDeploymentConfig` returns exactly **one** config per position — the lowest `m_iPriority` — after which `EvaluateFactionDeployments` `continue`s when that one already exists nearby (`:614-621`), never falling through to the next-best. A base would get its priority-1 config and nothing else, forever. Phase 1 fixes both, surgically, and that fix **is** the per-base escalation mechanism ([D5](#d5--the-evaluator-escalates-by-priority-and-that-is-how-a-base-fortifies)).
2. **The map of concerns to configs is one-to-one with the legacy priorities.** Each new `.conf` carries the `m_iPriority` its legacy upgrade authored (1, 2, 2, 2, 3, 4, 10 …), so the evaluator's "buy the highest-priority config this place is still missing" loop reproduces `SpendResources`' priority sweep with no new scheduling code.
3. **Elevated placement is a proven mechanism and is generalized, not special-cased.** `OVT_BaseUpgradeTowerGuard` and `OVT_BaseUpgradeSniperPosition` spawn a group at ground level, hold the destination in a map keyed on the group id, subscribe `GetOnAgentAdded()`, and `character.Teleport(mat)` each arriving member onto its post. Ladder pathing was fixed in Reforger 1.8 and Overthrow has had **zero** issues with these placements since; no ground-level fallback is planned or wanted ([D2](#d2--preserve-elevated-placement-via-a-generalized-exact-placement-spawning-module)). One new module carries it for **any** deployment, through a pluggable placement provider — and it re-runs on every re-materialisation, which the legacy one-shot did not have to.
4. **Static content is not groups and must not double on load.** Checkpoints, the three compositions and parked vehicles put *entities* in the world, tracked by `OVT_PersistenceTracking` and restored by vanilla persistence. Their slot claims already round-trip independently of base upgrades (`OVT_OccupyingFactionManagerSerializer.c:270-278` writes `controller.m_aSlotsFilled`; `OVT_OccupyingFactionManager.c:735-742` restores it). The one new rule: a **restored** deployment never re-builds its static content — exactly what legacy `OVT_BaseUpgradeComposition.Deserialize` already did ([D7](#d7--a-restored-deployment-never-rebuilds-static-content)).
5. **One funding path means deleting a spender, not adding a broker.** `CheckUpdate`'s 80 %-across-bases loop, `DistributeInitialResources`' per-base spend and `AllocateDeploymentResourcesIfNeeded`'s conditional drip all go. In their place: the same 80 % of every tick goes **unconditionally** to `AddFactionResources`, and the opening budget those 11 Eden bases used to conjure is credited to the pool once, as a single sum. `m_iResources` survives as what it already also is — the QRF and counter-attack reserve, both epic-level exclusions.
6. **Legacy saves convert by value, not by identity.** Old `OVT_PersistedBaseUpgrade` records are read (the payload class stays declared — `upgrades` sits at field 4 of 5 in `OVT_PersistedBase` and removing it would reorder `garrison`), summed into a credit, and handed to the deployments pool. The evaluator re-establishes defense by threat. **No per-base re-establishment code exists** ([D4](#d4--legacy-save-conversion-is-a-value-refund-to-the-pool)).
7. **Specops is dropped, deliberately and with a written cost.** `OVT_BaseUpgradeSpecops` gets no deployment replacement. What is lost is listed in [D3](#d3--specops-is-dropped-with-no-replacement). What is *gained* is a live defect: at HEAD, `UpdateSpecops` still debits the reserve and still spawns groups, while the tick that would make them do anything is kill-switched off — money and men leaking into nothing.

The migration is expected to be **strongly net-deleting**: the whole `BaseUpgrades/` directory (12 files, ~1 800 lines), its config, its Campaign-tier test, ~120 lines of resource plumbing in `OVT_OccupyingFactionManager`, the base controller's upgrade half, and eight legacy faction attributes with their resolver.

---

## 2. Goals

### Primary

- **G1 — Behavioural parity, on the layer.** Every base-defense concern the occupying faction has today exists as a shipped deployment config, and every group belonging to one is core-registered: dead members stay dead across despawn **and** save/load, and a wiped force is not silently rebought.
- **G2 — One resource accounting path.** Base defense is bought out of `OVT_DeploymentManagerComponent.m_mFactionResources` and nowhere else. `OVT_BaseControllerComponent.SpendResources()` and every caller are deleted, grep-proven.
- **G3 — Banked-value proxying is gone.** No code anywhere converts a despawned group into an integer. `m_ProxiedGroups` / `m_iProxedResources` / `m_ProxiedPositions` do not exist.
- **G4 — The base-upgrades system is removed.** `Scripts/Game/Controllers/OccupyingFaction/BaseUpgrades/` is deleted, `Configs/BaseUpgrades/` is deleted, and the legacy prefab-slot resolution path (`GetRandomGroupByType` + the eight `Legacy Faction Groups` attributes) is retired.
- **G5 — Legacy saves convert.** A campaign saved on base upgrades loads, credits the occupying faction's deployment pool with the value it had invested, and re-establishes defense from the configs within a few evaluation cycles. Nothing duplicates; nothing vanishes into an unreadable payload.
- **G6 — Elevated placement is preserved and generalized.** Tower guards stand on tower walkways and sniper teams stand on their markers, **including after a despawn/respawn cycle** and after a reload — through one module usable by any future deployment that needs exact placement.
- **G7 — The epic's kill switch leaves.** All three remaining production guards are removed — two by **deleting** the legacy code they wrap, one (QRF) by **restoring** the code it disables — and `OVT_VirtPlaytestKillSwitch.c` is deleted. `grep -rn "OVT-VIRT-PLAYTEST-ONLY" Scripts/` returns **nothing**.
- **G8 — Core is not touched.** `git diff Scripts/Game/GameMode/Virtualization/` is **empty** for the whole feature. `api.md` gains no signature. This is the first consumer feature in the epic that asks core for nothing.

### Secondary

- **G9 — The GM panel stays truthful.** `OVT_GMSnapshotBuilder.BuildBases()` currently walks `controller.m_aBaseUpgrades`; it is re-pointed at the deployments anchored at each base rather than left emitting zeros. The GM record classes and their RPC are **not** changed.
- **G10 — Integration's four hand-offs are closed**, each named in the phase that closes it: the three kill-switch guards; `m_aTowerDefensePatrolPrefab`'s zero readers; the stale comment at `OVT_OccupyingFactionManager.c:440-443`; and `m_iMilitarySpawnDistance`'s last production reader.
- **G11 — The road-snap trap gets its opt-out.** Integration recorded that a garrison registers on the nearest road, which the shared snap can take up to 500 m away, and that "the fix is a module-level opt-out in a later phase" (`integration/context.md`, Phase 4 additions). This is that phase.
- **G12 — Nothing new replicates.** No `Rpc`, no `RplProp`, no new prefab, no UI. One appended field to one persisted payload, at most, and only if [D7](#d7--a-restored-deployment-never-rebuilds-static-content)'s cheaper option proves insufficient.

### 2.1 Quality Bar — the hard floor

This is a **backend / simulation** feature. There is no UI polish dimension; the bar is reliability, accounting integrity and save compatibility.

| Bar | What it means concretely | How it is caught |
|---|---|---|
| **No force doubling, ever** | A base never ends up with two of anything: not across a Continue, not across an in-session re-apply, not when a legacy save's value is refunded, not when a restored deployment re-activates. Every registration path converges (`FindGroupsByOwner` first); every static-content path refuses on a restored deployment. | §6 F8/F9/F12; Persistence cases T6.6/T4.9; the "no second composition" acceptance grep |
| **Resource accounting is closed** | Every resource that leaves the occupying reserve arrives somewhere countable. After Phase 6 exactly **one** code path spends on defense, exactly **one** credits the pool, and the sum of `m_iResources` + the pool + invested is conserved across a tick. | §6 F10; Logic-tier conversion maths; the `SpendResources` grep |
| **Dead members stay dead** | The headline player promise of the whole epic, now at bases: a defense position you shot down to one man comes back as one man, at the same post, after you drive away and after you reload. | §6 F1/F2/F6 — player-visible criteria |
| **A legacy save is never worse off** | Loading a pre-migration campaign neither strands its investment nor duplicates its defense. The refund is visible in the pool and the base re-fortifies. | §6 F12 — player-visible criterion; Persistence case T6.6 |
| **Core stays frozen** | `git diff Scripts/Game/GameMode/Virtualization/` empty, every phase. | Acceptance criterion on all eight phases |

---

## 3. Architecture Overview

### 3.1 Division of labour after the migration

```
SERVER ONLY. Nothing new replicates. Core is NOT touched.

CORE (virtualization/core — FROZEN, api.md §10 "integration")   ← consumed, unchanged
└─ RegisterGroup / UnregisterGroup / FindGroupsByOwner / GetOnRecordsRestored / GetOnGroupWiped /
   GetAliveMemberCount / GetGroup     — reached ONLY through the deployment framework, as today

DEPLOYMENTS — still the single tracked-group consumer seam
  Scripts/Game/GameMode/Deployments/
├─ OVT_DeploymentManagerComponent
│    + FindBestDeploymentConfig SKIPS configs already deployed here  → escalation      (D5)
│    - IsPositionSuitableForDeployment's blanket 100 m veto           → co-location     (D5)
│    + m_iMaxDeploymentsPerFaction raised on the game-mode prefab                       (D5)
├─ OVT_DeploymentComponent
│    + m_bRestoredFromSave (runtime only, set by ApplyPersistedDeployment)              (D7)
├─ Modules/ (edited, additively)
│    ├─ OVT_InfantrySpawningDeploymentModule
│    │     + m_bSnapToRoad attribute (default TRUE = today's behaviour)                 (G11)
│    │     + protected virtual ResolveSpawnPosition(anchor, index)
│    │     + protected virtual OnGroupRegistered(handle, pos) / OnGroupReclaimed(handle)
│    │     (all three copied in CloneModule — the standing trap)
│    └─ OVT_BaseConditionDeploymentModule                                    unchanged
├─ Modules/ (NEW — the whole build surface of this feature)
│    ├─ OVT_PlacedInfantrySpawningDeploymentModule : OVT_InfantrySpawningDeploymentModule
│    │     exact placement for N groups, re-applied on every materialisation      (D2)
│    │     ├─ ref OVT_DeploymentPlacementProvider m_Placement   ← the modder seam
│    │     ├─ OVT_TowerCoverPostPlacementProvider     MDT_TOWER walkway posts
│    │     ├─ OVT_SniperMarkerPlacementProvider       OVT_SniperPositionComponent markers
│    │     └─ OVT_BaseDefendPositionPlacementProvider nearest base's m_aDefendPositions
│    ├─ OVT_CompositionSpawningDeploymentModule : OVT_InfantrySpawningDeploymentModule
│    │     slotted composition + optional guard group anchored on it              (D6, D7)
│    ├─ OVT_ParkedVehicleSpawningDeploymentModule : OVT_BaseSpawningDeploymentModule
│    │     parking-spot vehicles; no groups, no virtualization                    (D7)
│    └─ OVT_NoPlayersNearbyConditionDeploymentModule : OVT_BaseConditionDeploymentModule
│          "don't fortify a base a player is standing in"      ← the baseCloseRange+100 rule
└─ (nothing else in the framework changes)

CONFIGS (additive, then one deletion)
├─ Configs/Deployment/Deployment_Base*.conf         NINE new files + nine registry entries
├─ Configs/Factions/{USSR,US}_OverthrowData.conf    new GROUP + VEHICLE + COMPOSITION entries
└─ Configs/BaseUpgrades/                            DELETED (Phase 7)

OCCUPYING FACTION MANAGER  Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c
├─ CheckUpdate()            base-spend loop DELETED; the same 80 % goes to the pool     (D8)
├─ DistributeInitialResources()  becomes ONE pool credit, not eleven base spends        (D8)
├─ AllocateDeploymentResourcesIfNeeded()  DELETED (its job is now unconditional)        (D8)
├─ UpdateSpecops() + both call sites   DELETED                                          (D3)
├─ InitBaseControllers()    upgrade-replay block DELETED; slotsFilled restore UNTOUCHED
├─ ApplyPersistedBaseUpgrades()  becomes the VALUE SUM, not a record copy               (D4)
├─ RecoverResources()       loses its only caller — deleted with it
└─ m_iResources, GainResources, threat, QRF, counter-attack, tower sabotage  UNTOUCHED

BASE CONTROLLER  Scripts/Game/Controllers/OccupyingFaction/OVT_BaseControllerComponent.c
├─ m_aBaseUpgrades / m_BaseUpgradesConfig / InitializeBase's copy loop  DELETED
├─ UpdateUpgrades() + its CallLater + FindUpgrade()                     DELETED  (guard :84)
├─ SpendResources()                                                    DELETED  (guard :298)
└─ SLOT REGISTRY SURVIVES AND IS NOW THE ONLY REASON THIS COMPONENT EXISTS BESIDES BASE DATA:
   m_AllSlots / m_AllCloseSlots / m_{Small,Medium,Large}Slots / m_{Medium,Large}RoadSlots /
   m_aSlotsFilled / m_Parking / m_aDefendPositions / m_aVehiclePatrolSpawns / GetNearestSlot

FACTION  Scripts/Game/Faction/OVT_Faction.c
├─ m_GroupRegistry / m_VehicleRegistry / m_aCompositionConfig    the ONLY resolution path
└─ Legacy Faction Groups arrays + GetRandomGroupByType           retired  (D9)

DELETED OUTRIGHT
├─ Scripts/Game/Controllers/OccupyingFaction/BaseUpgrades/    (12 files)
├─ Configs/BaseUpgrades/overthrowBaseUpgrades.conf (+ .meta)
├─ Scripts/Game/Configuration/OVT_BaseUpgradesConfig.c
├─ Scripts/Game/Tests/.../OVT_TEST_Campaign_BaseUpgrades_ConfigLoaded.c
└─ Scripts/Game/GameMode/Virtualization/OVT_VirtPlaytestKillSwitch.c     ← EPIC END

NOT USED, DELIBERATELY
├─ any new core API                          G8 — this feature asks core for nothing
├─ any change to QRF                         epic-level exclusion; its guard is UN-guarded
├─ resistance base defense                   out of scope by the requirements
└─ any client-visible base-defense surface   out of scope; the GM panel is re-pointed, not grown
```

### 3.2 The concern → config mapping

**Nine legacy classes, nine shipped configs, one documented drop, one straight delete.** Every config's `m_iPriority` is the priority its legacy upgrade authored in `overthrowBaseUpgrades.conf`, so the evaluator's escalation order **is** the old `SpendResources` sweep order.

| # | Legacy class | Legacy conf (prio / alloc) | Decision | New config | Spawning module(s) | Behaviour / condition modules |
|---|---|---|---|---|---|---|
| 1 | `OVT_BaseUpgradeDefensePatrol` (baseline) | :3-5 — prio **1**, alloc 6 | **Migrate** | `Deployment_BaseGarrisonPatrol.conf` | `OVT_InfantrySpawningDeploymentModule` `light_patrol`, `m_bSnapToRoad 0`, `m_fSpawnRadius 50` | `OVT_PatrolBehaviorDeploymentModule` PERIMETER r=280 (`baseRange`) + Reinforcement + BaseControlCondition + NoPlayersNearby |
| 2 | ″ (threat > 25 escalation) | same entry, in-class at `:36-38` | **Migrate as a variant config** | `Deployment_BaseHeavyPatrol.conf`, `m_iMinimumThreatLevel 25`, prio **5** | infantry `heavy_infantry` **(new registry entry)** | as above |
| 3 | ″ (threat > 50 / first-group AT) | same entry, `:34-40` | **Migrate as a variant config** | `Deployment_BaseATSection.conf`, `m_iMinimumThreatLevel 50`, prio **6** | infantry `at_team` **(new registry entry)** | as above |
| 4 | `OVT_BaseUpgradeDefensePosition` | :6-9 — prio **2**, alloc -1 | **Migrate** | `Deployment_BaseDefensePositions.conf` prio **2** | `OVT_PlacedInfantrySpawningDeploymentModule` + `OVT_BaseDefendPositionPlacementProvider`, `heavy_infantry` | `OVT_PatrolBehaviorDeploymentModule` **DEFEND** + Reinforcement + BaseControl + NoPlayersNearby |
| 5 | `OVT_BaseUpgradeTowerGuard` | :10-13 — prio **2**, alloc -1 | **Migrate** | `Deployment_BaseTowerGuards.conf` prio **2** | `OVT_PlacedInfantrySpawningDeploymentModule` + `OVT_TowerCoverPostPlacementProvider`, `sniper` **(new entry)**, `m_eImportance HIGH` | **no behaviour module at all** — legacy gives these guards no waypoint on purpose (`OVT_BaseUpgradeTowerGuard.c:151-159`) + Reinforcement + BaseControl + NoPlayersNearby |
| 6 | `OVT_BaseUpgradeSniperPosition` | :14-17 — prio **2**, alloc -1 | **Migrate** | `Deployment_BaseSniperPositions.conf` prio **2** | `OVT_PlacedInfantrySpawningDeploymentModule` + `OVT_SniperMarkerPlacementProvider`, `sniper_team` **(new entry)**, `m_eImportance HIGH` | none (same reason) + Reinforcement + BaseControl + NoPlayersNearby |
| 7 | `OVT_BaseUpgradeCheckpoints` | :24-27 — prio **3**, alloc -1 | **Migrate** | `Deployment_BaseCheckpoints.conf` prio **3** | `OVT_CompositionSpawningDeploymentModule` ×2 (`LargeCheckpoint` on `LARGE_ROAD`, `MediumCheckpoint` on `MEDIUM_ROAD`) each with a `light_patrol` guard group | `OVT_PatrolBehaviorDeploymentModule` DEFEND + Reinforcement + BaseControl + NoPlayersNearby |
| 8 | `OVT_BaseUpgradeComposition` ×3 (`SmallBunker` :28-32 prio 4, `AmmoCache` :33-37 prio 1, `MGNest` :38-42 prio 1) | three conf entries, one class | **Migrate — three modules in ONE config** | `Deployment_BaseFortifications.conf` prio **4** | `OVT_CompositionSpawningDeploymentModule` ×3 (`SmallBunker` +`bunker_team` guard, `AmmoCache`, `MGNest`), all `SMALL` slots | Reinforcement + BaseControl + NoPlayersNearby |
| 9 | `OVT_BaseUpgradeParkedVehicles` | :18-23 — prio **10**, alloc 9, `m_iNumberOfCars 0`, `m_iNumberOfTrucks 1` | **Migrate** | `Deployment_BaseParkedVehicles.conf` prio **10** | `OVT_ParkedVehicleSpawningDeploymentModule` (`truck` ×1; cars authored 0 today and stay 0) | BaseControl + NoPlayersNearby (**no** Reinforcement — nothing to rebuy) |
| 10 | `OVT_BaseUpgradeSpecops` | :43-46 — prio 3 | ❌ **DROPPED, documented** | — | — | see [D3](#d3--specops-is-dropped-with-no-replacement) |
| 11 | `OVT_BaseUpgradeTownPatrol` | **not in the conf, not in any `.et`** | ❌ **Straight delete** — dead code | — | — | Town patrols are `Deployment_TownPatrol.conf`, shipped since `integration` |

Notes that bind the authoring:

- **Grouping decisions.** Fortifications are three modules in one config (one purchase, priority 4 — the highest of the three legacy entries) because they are one player-visible concern and three configs would triple the deployment count for no behavioural gain. Checkpoints are two modules in one config for the same reason. Tower guards and sniper positions stay **separate** configs despite sharing a priority, because a base with towers but no sniper markers must not pay for both.
- **Escalation variants are configs, not module attributes**, because `m_iMinimumThreatLevel` is a config-level gate (`OVT_DeploymentComponent.c:496`). This is the only mechanism the framework offers, and it is exactly the escalation the user asked for: concern-by-concern with threat and resources.
- **Legacy threat gates**: no entry in `overthrowBaseUpgrades.conf` authors `m_iMinimumThreat`, so the controller's threat gate is dead today and all real escalation lives in class code (defense-patrol group type, sniper per-marker gate). The three thresholds that matter — 25, 50, and `OVT_SniperPositionComponent.m_iMinimumThreat` — are preserved: the first two as config `m_iMinimumThreatLevel`, the third **inside the sniper placement provider**, which filters markers by their own component the way `OVT_BaseUpgradeSniperPosition.c:98-99` does.
- **`m_iAllowedLocationTypes BASE` is already authored** by both shipped vehicle-patrol configs, so `GetBasePositions()` already produces candidates and `GetLocationTypeAtPosition` already returns `BASE` within 500 m of one. No classification change is needed (contrast integration's `RADIO_TOWER` OR-in).
- **`m_bFreeAtGameStart` is `0` on every base config.** Opening defense is *paid for* out of the pool, seeded from the legacy `startingResources` sum ([D8](#d8--one-funding-path-delete-the-second-spender-do-not-add-a-broker)). Marking them free would be the fallback if play-test pacing is unacceptable, and it is called out as such in [R6](#9-risks--mitigation).

### 3.3 The new modules

#### `OVT_PlacedInfantrySpawningDeploymentModule` — exact placement, generalized

Subclasses `OVT_InfantrySpawningDeploymentModule` deliberately: it inherits convergence, reclaim, wipe accounting, GM tagging, faction-key resolution, the two eliminated gates, and — crucially — **reinforcement**, because `OVT_ReinforcementBehaviorDeploymentModule.GetMissingUnitsCount()` casts to `OVT_InfantrySpawningDeploymentModule` and returns 0 for anything else (`:186-197`). A sibling class would silently never rebuy.

```
[Attribute] ref OVT_DeploymentPlacementProvider m_Placement;   // the modder seam
[Attribute] float m_fSearchRadius;                             // how far from the deployment to look

override CalculateGroupCount(pos)      → min(posts.Count(), m_iMaxGroupCount)
override ResolveSpawnPosition(anchor,i)→ posts[i]   (NO road snap, NO ring roll)
override OnGroupRegistered(handle,pos) → remember post; subscribe placement applier
override OnGroupReclaimed(handle)      → re-resolve post; re-subscribe (Route B gives a NEW entity)
override CloneModule()                 → all 12 inherited attributes + these two
```

The **placement applier** is the legacy mechanism, kept alive for the group's whole life instead of one shot:

```
group.GetOnAgentAdded().Insert(OnPlacedAgentAdded)     // ⚠ ONE argument, not two (integration Phase 5)
OnPlacedAgentAdded(AIAgent agent):
    group   = agent.GetParentGroup()
    slot    = next arrival index for this group
    target  = this group's post, offset by slot along the post's own right vector
    BaseGameEntity.Cast(agent.GetControlledEntity()).Teleport(mat)   // mat = post transform
```

Two differences from legacy, both required by virtualization and both load-bearing:

1. **The subscription is never removed until the group is unregistered.** Legacy removed it after placing (`OVT_BaseUpgradeTowerGuard.c:209`) because the group only ever spawned once. A registered group materialises and de-materialises repeatedly, and `Event_OnAgentAdded` fires again each time; keeping the subscription is what makes re-materialisation re-run the placement, which the requirements demand.
2. **The arrival counter resets per materialisation.** Subscribe to the group's own spawn/despawn notification (`api.md` §3, "Spawn/despawn notification — subscribe on the GROUP, not the manager") and zero the counter there. Fallback if that seam proves awkward: wrap the counter modulo the post count. Assignment is by arrival order, exactly as `m_PendingPlaced` did — posts within one concern are interchangeable, and core's slot mask (not the post index) is what preserves *who* is alive.

Providers (`[BaseContainerProps]`, one virtual `array<vector> ResolvePosts(vector deploymentPosition, float radius, int factionIndex)` returning **world transforms** as position+yaw pairs):

| Provider | Port of | How it finds posts |
|---|---|---|
| `OVT_TowerCoverPostPlacementProvider` | `OVT_BaseUpgradeTowerGuard.c:27-44, :131-146, :171-188` | `QueryEntitiesBySphere` for `SCR_DestructibleBuildingEntity` whose `SCR_MapDescriptorComponent.GetBaseType() == MDT_TOWER`; per tower, the first `SCR_AISmartActionSentinelComponent` tagged `"CoverPost"` and `IsActionAccessible()`; post = `towerTransform[3] + (GetActionOffset() + WALKWAY_OFFSET).Multiply3(towerTransform)` with `WALKWAY_OFFSET = "0 0 1.5"` — **the open-air walkway, not the glass cabin** (the reason is in the legacy comment at `:140-143` and must be carried into the provider's header) |
| `OVT_SniperMarkerPlacementProvider` | `OVT_BaseUpgradeSniperPosition.c:34-43, :131-158` | `QueryEntitiesBySphere` for `OVT_SniperPositionComponent`; **filters by that component's own `m_iMinimumThreat` against the current occupying threat**; post = the marker's full world transform (rotation preserved — snipers face where the marker points) |
| `OVT_BaseDefendPositionPlacementProvider` | `OVT_BaseUpgradeDefensePosition.c:45-105` + `OVT_BaseControllerComponent.c:246-255` | reads the nearest base controller's `m_aDefendPositions` (already discovered there from `SCR_AISmartActionSentinelComponent` entities within `baseRange`, towers excluded at `:239`) |

`ReleaseAction()` housekeeping (`OVT_BaseUpgradeTowerGuard.c:215-231`) is **not** ported: it existed to un-reserve smart-action posts that the despawn path had orphaned, and the new guards never take a smart action (they get no waypoint, exactly as legacy). Record that in `context.md` rather than porting dead machinery — but re-check for leaked `IsActionAccessible() == false` posts during the play-test.

#### `OVT_CompositionSpawningDeploymentModule` — slotted static content + its guard

Also subclasses the infantry module, for the same reinforcement reason and to get the guard group's whole lifecycle free.

```
[Attribute] string m_sCompositionTag;      // resolved via OVT_Faction.GetCompositionConfig(tag)
[Attribute] OVT_EDeploymentSlotType m_eSlotType;   // SMALL | MEDIUM | LARGE | ROAD_SMALL | ROAD_MEDIUM | ROAD_LARGE
[Attribute] bool m_bFillAmmoBoxes;         // port of OVT_BaseUpgradeComposition.FillAmmoboxes

EnsureGroups():
  if (!m_ParentDeployment.WasRestoredFromSave() && m_CompositionEntity == null)
      slot = nearest base controller's free slot of m_eSlotType (skip m_aSlotsFilled, ≤30 tries)
      spawn comp.m_aPrefabs.GetRandomElement() ON THE SLOT TRANSFORM
      OVT_PersistenceTracking.Track(entity)          ← how it survives a save, unchanged from legacy
      OVT_NavmeshRebuild.RebuildNow(entity)
      baseController.m_aSlotsFilled.Insert(slot.GetID())   ← the claim that already round-trips
      SpawnDefaultOccupants({TURRET}) within 7/15/23 m    ← mans MG nests and bunker turrets
      if (m_bFillAmmoBoxes) FillAmmoBoxes(entity)
  super.EnsureGroups()                                ← the guard group, anchored on the composition
override ResolveSpawnPosition(...)  → the composition's origin (no snap, no ring)
override GetResourceCost()          → m_iCostPerGroup × count + the composition's own m_iCost
```

The **checkpoint prefabs move into the composition config**: two new `OVT_FactionComposition` entries per faction, `MediumCheckpoint` and `LargeCheckpoint`, pointing at the prefabs `m_aMediumCheckpointPrefab` / `m_aLargeCheckpointPrefab` already name, with the legacy hardcoded costs (40 / 60) as `m_iCost`. One module then covers both concerns and two more faction attributes retire.

#### `OVT_ParkedVehicleSpawningDeploymentModule`

Subclasses `OVT_BaseSpawningDeploymentModule` (no groups, so nothing to inherit from the infantry module). Attributes: `m_sVehicleType` (faction **vehicle registry** name — two new entries, `car` and `truck`), `m_iVehicleCount`, `m_iCostPerVehicle`. Picks a random entry from the nearest base controller's `m_Parking`, calls `OVT_VehicleManagerComponent.GetParkingSpot(building, spot, type)` and `SpawnVehicleMatrix` — a direct port of `OVT_BaseUpgradeParkedVehicles.c:74-110`. Vehicles persist through the vehicle manager as they do today; **a restored deployment spawns none** ([D7](#d7--a-restored-deployment-never-rebuilds-static-content)).

#### `OVT_NoPlayersNearbyConditionDeploymentModule`

The one behavioural rule the funding rewrite would otherwise lose: `CheckUpdate` skips any base with a player within `baseCloseRange + 100` (`OVT_OccupyingFactionManager.c:1194`, comment *"Dont spawn stuff if a player is watching lol"*). Under the engine lifecycle, creating a deployment next to a player materialises its groups **in front of him**. So the rule becomes a creation gate:

```
[Attribute] float m_fMinPlayerDistance;     // default 320 = baseCloseRange(220) + 100
EvaluateStaticCondition(position, ...)  → GetNearestPlayerDistance(position) >= m_fMinPlayerDistance
EvaluateCondition()                     → true   ← runtime must NOT fail, or m_bDeleteOnConditionFail
                                                   would collect a base's defense whenever a player walks in
```

That asymmetry is the whole design of the module and belongs in its class header, because getting it backwards deletes a base's garrison the moment a player arrives.

### 3.4 The evaluator change — how a base fortifies concern by concern

Two surgical edits in `OVT_DeploymentManager.c`, and they are the reason per-concern configs work at all:

```
BEFORE                                          AFTER
FindDeploymentCandidates()                      FindDeploymentCandidates()
  └ IsPositionSuitableForDeployment(pos)           └ IsPositionSuitableForDeployment(pos)
      ├ reject if <100 m from ANY deployment           ├ (proximity veto REMOVED — the name-scoped
      └ ground trace                                   │  250 m dedup is the real anti-stack rule)
                                                       └ ground trace
FindBestDeploymentConfig(pos, ...)              FindBestDeploymentConfig(pos, ...)
  └ lowest m_iPriority among suitable               └ lowest m_iPriority among suitable
                                                       AND NOT already deployed within 250 m
                                                          (HasExistingDeploymentOfType, same call
                                                           the caller already makes)
EvaluateFactionDeployments()                    EvaluateFactionDeployments()
  └ if best already exists here → continue          └ unchanged (the check is now redundant but
    (the position is dead for this pass)               harmless and kept as a cheap guard)
```

The consequence, stated as the escalation contract:

```
pass 1 at a base:  missing everything → best = Garrison Patrol (prio 1) → bought
pass 2 at a base:  Garrison Patrol exists → best = Defense Positions (prio 2) → bought
pass 3:            → Tower Guards (prio 2, registry order breaks the tie)
pass 4:            → Sniper Positions (prio 2)
pass 5:            → Checkpoints (prio 3)
pass 6:            → Fortifications (prio 4)
pass 7 (threat>25):→ Heavy Patrol (prio 5)
pass 8 (threat>50):→ AT Section (prio 6)
pass 9:            → Parked Vehicles (prio 10)
… each gated by cost, by m_iMinimumThreatLevel, by the base-control condition and by
  "no player within 320 m", and paced by MAX_DEPLOYMENTS_PER_EVALUATION across the whole map.
```

This is `SpendResources`' 1..19 priority sweep, re-expressed. It also fixes a latent defect integration recorded but could not fix from inside its own scope: at a tower **inside a town**, Town Patrol and Tower Garrison are both priority 1 and the tie resolved to registry order, so the garrison was never chosen by the evaluator (only by the free-seed pass). After this change the loser is picked up on the next pass.

⚠ **Two ceilings become binding and must be raised deliberately.** Eden has ~11 bases (`Worlds/MP/OVT_Campaign_Eden_Layers/bases.layer`), 20 towns and 2 radio towers. A fully fortified map is ≈ 11 × 9 + 20 + 2 + 2 = **~123 deployments** against `m_iMaxDeploymentsPerFaction`'s class default of **100** (`OVT_DeploymentManager.c:34-35`; the game-mode prefab does not override it), and `MAX_DEPLOYMENTS_PER_EVALUATION = 10` per 30 s pass means ~6 minutes of real time to establish from cold. Phase 1 raises the per-faction ceiling on `Prefabs/GameMode/OVT_OverthrowGameMode.et` (recommended **400**) and leaves the per-pass cap alone unless the play-test says otherwise. The seed pass already logs a WARNING naming the config when the ceiling bites — that warning becomes the diagnostic.

### 3.5 The funding single-path cutover

```
TODAY (three paths, one economy)                     AFTER (one path)

NewGameStart:                                        NewGameStart:
  m_iResources = maxQRF                    (:276)      m_iResources = maxQRF            (unchanged)
  AddFactionResources(baseResourcesPerTick) (:307)     AddFactionResources(SEED)        ← ONE credit
+5 s DistributeInitialResources:                        where SEED = Σ over bases of
  per base: SpendResources(startingResources           startingResources × m_fStartingResourcesMultiplier
            × multiplier)                  (:765-767)  + baseResourcesPerTick
  UpdateSpecops()                          (:769)     (DistributeInitialResources deleted)

every 6 game hours, CheckUpdate:                     every 6 game hours, CheckUpdate:
  newResources = GainResources()          (:1168)      newResources = GainResources()   (unchanged)
  toSpend = 80 % of newResources          (:1170)      toSpend = 80 % of newResources   (unchanged)
  sort bases by threat                    (:1174-81)   AddFactionResources(toSpend)     ← UNCONDITIONAL
  per base: skip if player < baseCloseRange+100        m_iResources -= toSpend
            SpendResources(perBase)       (:1187-209)  (the base loop, the even split and the
  m_iResources -= spent                                 proximity skip are DELETED — threat ordering
  UpdateSpecops()                         (:1211)       is now the evaluator's candidate sort, and the
                                                        proximity skip is a condition module)
GainResources → AllocateDeploymentResourcesIfNeeded  (deleted — its 500/1000 conditional drip
  if pool < 500 && m_iResources > 1000:                exists only to arbitrate between two
     allocate min(newResources/2, m_iResources-1000)   spenders, and there is now one)
                                          (:1465-72)

QRF + counter-attack keep spending m_iResources.     unchanged — epic-level exclusion.
```

Three properties this preserves, and they are what "recognizable behavior" means here:

- **Bases near threat get defended first** — `EvaluateFactionDeployments` sorts candidates by `CalculateThreatLevel` descending with ±20 % jitter (`:593-606`), which is strictly better than today's *even* `perBase` split where threat only decided serving order.
- **Difficulty still scales defense** — through `baseResourcesPerTick` / `resourcesPerTick` (Easy 150 vs Normal 250) feeding the pool, and through `patrolGroupsMin/Max` feeding `CalculateGroupCount`. Deployment costs are **not** multiplied by `baseResourceCost`; `GetTotalResourceCost(difficultyMultiplier)` takes a multiplier that no caller passes, and wiring it would silently reprice town and vehicle patrols too. Left alone deliberately, recorded in [D8](#d8--one-funding-path-delete-the-second-spender-do-not-add-a-broker). ⚠ Note the compression integration already recorded: `CalculateGroupCount` halves the difficulty band then clamps to the module's own min/max.
- **A player watching a base doesn't see it fortify around him** — `OVT_NoPlayersNearbyConditionDeploymentModule`, [§3.3](#33-the-new-modules).

### 3.6 The save-conversion flow

```
A pre-migration save point contains, per occupying base:
  OVT_PersistedBase { location, faction, slotsFilled[], upgrades[], garrison[] }
  OVT_PersistedBaseUpgrade { type, resources, tag, pos, groups[] }

ApplyPersistedOccupyingFaction (:350-456)
 └ ApplyPersistedBaseUpgrades(base, record)      ← BECOMES THE CONVERSION, not a record copy
     value = 0
     foreach upgradeRecord:
         value += upgradeRecord.resources                       // banked + live value the class reported
         value += upgradeRecord.groups.Count() * LEGACY_GROUP_VALUE
     accumulate into a manager-level pending credit

after every base is read, ONCE:
     deploymentManager.AddFactionResources(occupyingIndex, totalValue)
     log the sum and the base count

InitBaseControllers (:702-757)
 ├ the upgrade replay block (:726-734)  DELETED  — nothing to replay into
 └ the slotsFilled restore (:735-742)   KEPT VERBATIM — it is what stops the composition
                                          module re-using a slot a legacy composition still occupies

Serialize side: WriteBase (:250-282) stops walking controller.m_aBaseUpgrades and writes an
EMPTY upgrades array. The FIELD STAYS DECLARED (OVT_PersistedBase.upgrades is field 4 of 5;
removing it reorders `garrison` and breaks every existing save). slotsFilled keeps being written.
```

`LEGACY_GROUP_VALUE` is `4 × m_Difficulty.baseResourceCost` — the average group size the legacy valuation used (`OVT_BasePatrolUpgrade.GetResources()` valued a live group at `agents × baseResourceCost`, and the shipped compositions run 2–6 men). It is an **approximation, by design**: the requirement says *value-parity, not entity-identity*. Pin it as a named constant with that sentence in its comment.

Three things deliberately convert to **zero**, and each has a reason:

- **Composition and checkpoint records** (`{type, pos, tag}`, `resources = 0`, no groups) — the composition *entities* are `OVT_PersistenceTracking`-tracked and come back from the save on their own, and their slots come back in `slotsFilled`. Refunding for them would pay twice.
- **`OVT_BaseUpgradeParkedVehicles`, `OVT_BaseUpgradeSpecops`, `OVT_BaseUpgradeTownPatrol`** — all three `Serialize()` to **null** and were never in a save payload at all.
- **A base the player already took** — the conversion runs per *occupying-faction* base record, matching where the money was.

**Idempotence is structural**, not guarded: after the first load the write path stores an empty array, so a second conversion sums zero. Say so in the method header rather than adding a flag.

### 3.7 Owner keys, plans and registration parameters

Unchanged from `integration` — this feature authors configs, it does not invent a scheme.

| Config | `ownerSystem` | `ownerKey` | plan | `spawnDistanceOverride` | `importance` |
|---|---|---|---|---|---|
| Base Garrison / Heavy / AT patrol | `"deployment"` | `<deploymentKey>#<moduleTag>` | PERIMETER, cycling, r = 280 | `-1` (global 1750) | `NORMAL` |
| Base Defense Positions | ″ | ″ | **DEFEND**, one point, non-cycling | `-1` | `NORMAL` |
| Base Tower Guards / Sniper Positions | ″ | ″ | **null** — no behaviour module (legacy parity) | `-1` | **`HIGH`** |
| Checkpoint / bunker guards | ″ | ″ | **DEFEND** | `-1` | `NORMAL` |

`<deploymentKey>` is `"<sanitised config name>@<round(x)>_<round(z)>"`, derived once and persisted (`m_sVirtualKey`, serializer v2). Nine configs at one base produce nine distinct keys because the config name differs — **no key-scheme change is needed**, which is the second time that decision has paid for itself.

⚠ **The plan is the opt-in.** A null or DEFEND-only plan is never walked by the movement tick, so tower guards, sniper teams, defense positions and checkpoint guards hold their posts by construction. Only the three roaming patrol configs get a movable plan — and their DEFEND-vs-PERIMETER choice is asserted, not assumed ([§7](#7-testing-strategy)).

⚠ **Module order in a `.conf` is update order, and `.conf` files cannot carry comments** (integration Phase 4). Every new config authors spawning modules first, then behaviour, then reinforcement **last among behaviour modules**, then conditions. Where the order matters for a specific module it goes in that module's class header, which is the only place a reader will find it.

---

## 4. Implementation Phases

Eight phases. Each leaves the tree compiling and the campaign playable: no phase deletes a legacy class without shipping the config that replaces it in the same phase, and no phase un-guards a kill switch whose legacy code still exists.

**Test-run policy:** `tools/compile-check.sh` runs freely. `tools/run-tests.sh` launches a real Reforger client and is run **by the orchestrator only, once, after a phase completes** — never during planning, never inside a subagent. `.claude/test-policy.md` is the rule. Fast = `{6A6E29FF47ECB840}`, All = `{6A6E2A002F53A581}`.

---

### Phase 1 — The evaluator learns to escalate **(GATE for every config phase)**

**Agent:** `component-developer-advanced` — **advanced.** It changes the shared deployment evaluator that four shipped configs already depend on, and a mistake here either floods the map with stacked deployments or silently stops creating any.
**Estimate:** 6–8 h
**Suite after this phase:** **All** `{6A6E2A002F53A581}`

**Tasks**

1. **T1.1 — Read-only survey first.** Re-grep and record in `context.md`, before editing: every caller of `IsPositionSuitableForDeployment` (expected: one, `FindDeploymentCandidates:692`), every caller of `FindBestDeploymentConfig` (expected: one, `:614`), and every authored `m_iAllowedLocationTypes` in `Configs/Deployment/` (expected: TOWN ×1, RADIO_TOWER ×1, **BASE ×2** — both vehicle patrols). If a concurrent session has added a caller, the design in [§3.4](#34-the-evaluator-change--how-a-base-fortifies-concern-by-concern) is re-checked before it is applied.
2. **T1.2 — Drop the blanket proximity veto.** Remove the `MIN_DEPLOYMENT_DISTANCE` rejection from `IsPositionSuitableForDeployment` (`:850-863`); keep the ground trace. Delete the now-unused constant **only if** nothing else reads it. Rewrite the method's comment to name what actually prevents stacking now: the name-scoped 250 m `HasExistingDeploymentOfType` dedup. ⚠ This is the change that lets two deployments share a position; it is also the change that would let a *runaway* config stack, so T1.4's dedup filter must land in the same commit.
3. **T1.3 — Raise the per-faction ceiling.** Author `m_iMaxDeploymentsPerFaction 400` on the `OVT_DeploymentManagerComponent` declaration in `Prefabs/GameMode/OVT_OverthrowGameMode.et`. Show the arithmetic in `context.md` (11 bases × 9 + 20 towns + 2 towers + 2 vehicle patrols ≈ 123 against a default of 100). Do **not** change the class default.
4. **T1.4 — Escalation: skip configs already deployed here.** In `FindBestDeploymentConfig`, add `HasExistingDeploymentOfType(position, factionIndex, config.m_sDeploymentName)` to the per-config filter, before the priority comparison. Leave the caller's identical check in place as a cheap guard and comment why. Add a doc block stating the escalation contract of [§3.4](#34-the-evaluator-change--how-a-base-fortifies-concern-by-concern) — this is the only place a reader will learn that priority now means "order of acquisition at one place", not just "who wins a tie".
5. **T1.5 — `OVT_NoPlayersNearbyConditionDeploymentModule`.** New condition module per [§3.3](#33-the-new-modules). `EvaluateStaticCondition` gates creation on `GetNearestPlayerDistance(position) >= m_fMinPlayerDistance` (default **320**); **`EvaluateCondition` returns true unconditionally** and its header says why in one sentence. Hand-written `CloneModule` copying both attributes.
6. **T1.6 — Logic-tier coverage** in a new `TestSuites/Logic/OVT_TEST_Logic_BaseDefenseEscalation.c` (Fast, `suite: OVT_TEST_LogicSuite`), world-free: the priority-selection maths (given a set of (name, priority) pairs and a set of already-present names, the next pick is the lowest-priority absent one; an all-present set picks nothing; ties resolve by input order). Extract the selection into a **world-free static helper** the evaluator calls, so the tier grep stays clean — no manager, world, entity or `OVT_Global` identifier in that file, **comments included** (movement's Phase 1 found the tier grep matches comments).
7. **T1.7 — Init-tier coverage:** at a base position, `GetLocationTypeAtPosition` includes the `BASE` bit; the new condition module refuses a position with a player on top of it and accepts one far away; and — the phase's headline claim — `FindBestDeploymentConfig` at a position that already holds config A returns config B rather than null. Use `spawnDistanceOverride = 0` on anything that registers (Manual policy), and scope every registry assertion to your own owner key (the Persistence/Campaign worlds run a live deployment wave).
8. **T1.8 — Record in `context.md`:** the T1.1 verdicts, the ceiling arithmetic, and an explicit note that **`m_iResourceAllocation` on `OVT_DeploymentConfig` has zero readers** (`:43-44` — the `m_iResourceAllocation` hits elsewhere belong to `OVT_BaseUpgrade`'s unrelated same-named field). Do not delete it in this phase; it is authored nowhere and dies in the Phase 7 sweep if still unread.

**Acceptance criteria**

- `tools/compile-check.sh` exits **0**; Fast and All green.
- `grep -rn "MIN_DEPLOYMENT_DISTANCE" Scripts/` → the constant is gone, or has a named surviving reader recorded in `context.md`.
- `git diff Scripts/Game/GameMode/Virtualization/` → **empty**.
- `git diff Configs/Deployment/` → **empty** (no config authoring in this phase).
- `git diff Prefabs/` → **exactly one** attribute added.
- The four shipped configs still create deployments in the Init/Campaign worlds — i.e. no case that passed before this phase now fails.

---

### Phase 2 — The new modules, the providers, and the faction registry entries

**Agent:** `component-developer-advanced` — **advanced.** It subclasses the shipped infantry module, adds three protected virtuals to it, and introduces the first code in the framework that calls `OVT_PersistenceTracking.Track`.
**Estimate:** 12–16 h
**Suite after this phase:** **All** `{6A6E2A002F53A581}`

**Tasks**

1. **T2.1 — Additive seams on `OVT_InfantrySpawningDeploymentModule`.** Three additions, all with today's behaviour as the default:
   - `[Attribute(defvalue:"1")] bool m_bSnapToRoad` — when false, `GetRandomSpawnPosition` returns the ring point without `OVT_WorldUtils.FindNearestRoad`. ⚠ **Default true**, so the four shipped configs are bit-identical. This closes integration's recorded 500 m road-snap trap ([G11](#secondary)).
   - `protected vector ResolveSpawnPosition(vector anchor, int index)` — defaults to `GetRandomSpawnPosition(anchor)`; `RegisterGroups` calls it instead of calling the roller directly (`:297`).
   - `protected void OnGroupRegistered(int handle, vector position)` and `protected void OnGroupReclaimed(int handle)` — empty; called right after `TagForGameMaster` in `RegisterGroups` (`:315`) and `ReclaimHandles` (`:251`) respectively.
   **Copy `m_bSnapToRoad` in `CloneModule`.** `CloneModule` copies attributes by hand and silently drops what it forgets — this is the trap that lost `m_fMaxCruiseSpeed`, and integration explicitly booked it as "feature 5's problem". Audit the existing 12-attribute copy list while you are in there and record the verdict.
2. **T2.2 — `OVT_DeploymentPlacementProvider` + the three shipped providers**, per [§3.3](#33-the-new-modules). Base class is `[BaseContainerProps]` with one virtual returning an `array<ref OVT_DeploymentPlacement>` (position + yaw). Each provider's header carries the *reason* its legacy original placed things where it did — especially the tower walkway note (`OVT_BaseUpgradeTowerGuard.c:140-143`: the cabin's window glass blinds AI perception and obstructs the muzzle) and the sniper marker's rotation preservation.
3. **T2.3 — `OVT_PlacedInfantrySpawningDeploymentModule`.** Per [§3.3](#33-the-new-modules). ⚠ `SCR_AIGroup.GetOnAgentAdded()` **passes ONE argument, not two** — the vanilla doc comment is wrong and a handler written to it compiles clean and misbehaves at the wire (integration Phase 5). Recover the group with `agent.GetParentGroup()`. ⚠ `ScriptInvoker.Insert` does not de-duplicate: `Remove` then `Insert` on every reclaim.
4. **T2.4 — `OVT_CompositionSpawningDeploymentModule`.** Per [§3.3](#33-the-new-modules), porting slot selection (30 random tries, skipping `m_aSlotsFilled`), `SpawnEntityPrefabMatrix` on the slot transform, `OVT_PersistenceTracking.Track`, `OVT_NavmeshRebuild.RebuildNow`, `SpawnDefaultOccupants({TURRET})` at 7/15/23 m by slot size, and the optional ammo-box fill. Add `OVT_EDeploymentSlotType` with the six values. **The slot is marked filled only after a successful spawn** — the legacy comment at `OVT_BaseUpgradeCheckpoints.c:16-17` says why, and it stays true.
5. **T2.5 — `OVT_ParkedVehicleSpawningDeploymentModule`.** Per [§3.3](#33-the-new-modules).
6. **T2.6 — `WasRestoredFromSave()` on `OVT_DeploymentComponent`.** A plain runtime bool set inside `ApplyPersistedDeployment` and never persisted, with a header stating [D7](#d7--a-restored-deployment-never-rebuilds-static-content). ⚠ **Do not touch the serializer, its version, or any field order** in this phase.
7. **T2.7 — Faction registry entries**, both shipped factions, all appended (never reordered):
   - **Group registry:** `heavy_infantry`, `at_team`, `sniper`, `sniper_team`, `bunker_team` — prefabs taken verbatim from the legacy arrays they replace (`m_aHeavyInfantryPrefabSlots`, `m_aGroupATPrefabSlots`, `m_aGroupSniperPrefab`, `m_aGroupSniperTeamPrefab`, and the `SmallBunker` composition's `m_aGroupPrefabs`). Where a legacy array held several prefabs, pick **one** and record which and why in `context.md` — the registry resolves one name to one composition. Author a plausible `m_iCost` on each.
   - **Vehicle registry:** `car`, `truck` from `m_aVehicleCarPrefabSlots[0]` / `m_aVehicleTruckPrefabSlots[0]`.
   - **Composition config:** `MediumCheckpoint` (`m_iCost 40`) and `LargeCheckpoint` (`m_iCost 60`) pointing at the existing checkpoint prefabs.
   ⚠ Hand-authored GUIDs are fine and always resolve; use a fresh repo-unique prefix, grep-verified unused before use.
8. **T2.8 — Init-tier coverage:** every new registry name resolves to a real prefab for **both** shipped factions (the case that would have caught a typo before nine configs depend on it); `m_bSnapToRoad 0` returns a position inside `m_fSpawnRadius` of the anchor while `1` does not necessarily; each provider returns an empty list rather than null at a position with nothing near it; the clone of each new module carries every authored attribute (a direct assertion against the `CloneModule` trap).
9. **T2.9 — `context.md`:** the `CloneModule` audit verdict, the prefab choices from T2.7, and the note that `ReleaseAction()` housekeeping was deliberately not ported.

**Acceptance criteria**

- compile **0**; Fast and All green.
- `git diff Configs/Deployment/` → **empty** (no deployment configs yet — the modules exist, nothing authors them).
- `git diff Scripts/Game/GameMode/Virtualization/` → **empty**.
- `git diff Scripts/Game/Controllers/OccupyingFaction/` → **empty** (no legacy class is touched in this phase).
- `grep -rn "OVT-VIRT-PLAYTEST-ONLY" Scripts/ | wc -l` → **unchanged at 5**.
- The four shipped deployment configs are byte-identical, and every existing case still passes — the three new virtuals and one new attribute are additive with today's defaults.

---

### Phase 3 — Garrison patrols: three configs, two classes deleted

**Agent:** `component-developer` — standard. Config authoring plus the deletion of two self-contained classes whose replacements ship in the same phase.
**Estimate:** 5–7 h
**Suite after this phase:** **All** `{6A6E2A002F53A581}`

**Tasks**

1. **T3.1** Author `Configs/Deployment/Deployment_BaseGarrisonPatrol.conf` per [§3.2](#32-the-concern--config-mapping): infantry `light_patrol`, `m_bSnapToRoad 0`, `m_fSpawnRadius 50`, `m_iMinGroupCount`/`m_iMaxGroupCount` matched to the legacy allocation (alloc 6 × `baseResourceCost` 15 = 90 on Normal, at `cost × 4` = 60 per group → **1–2 groups**); PERIMETER radius **280** (`m_Difficulty.baseRange`); Reinforcement (`m_bEnableReinforcement 1`, `m_bDeleteOnConditionFail 1`); `OVT_BaseControlConditionDeploymentModule` (`m_bRequireControl 1`, `m_fMaxDistance 500`); `OVT_NoPlayersNearbyConditionDeploymentModule`. Config level: `OCCUPYING_FACTION` / `BASE` / `m_iPriority 1` / `m_fChance 100` / `m_iMaxInstances -1` / `m_bFreeAtGameStart 0`.
2. **T3.2** Author `Deployment_BaseHeavyPatrol.conf` (`heavy_infantry`, `m_iMinimumThreatLevel 25`, `m_iPriority 5`) and `Deployment_BaseATSection.conf` (`at_team`, `m_iMinimumThreatLevel 50`, `m_iPriority 6`) as thin variants of T3.1. ⚠ The legacy AT branch also fired when `GetNumGroups() == 0` (`OVT_BaseUpgradeDefensePatrol.c:34`), i.e. the *first* group of every base was an AT group regardless of threat. That is **not** reproduced — it reads as a bug, not a feature; record the deliberate divergence in `context.md`.
3. **T3.3** Append three registry entries to `Configs/Deployment/overthrowDeployments.conf`, in the shape the four existing entries use (an inheritance line with an empty or minimal override body).
4. **T3.4** **Delete `OVT_BaseUpgradeDefensePatrol.c`** and remove its `overthrowBaseUpgrades.conf` entry (`:3-5`) in the same commit — a class deleted while its conf entry survives is a parse failure.
5. **T3.5** **Delete `OVT_BaseUpgradeTownPatrol.c`** outright. It is in no `.conf`, no `.et` and no other `.c`; its only non-self mention is a comment at `OVT_GMRecords.c:61`. Its deletion removes `OVT_OccupyingFactionManager.RecoverResources()`'s only caller (`:96`) — delete `RecoverResources` too, grep-proven, or record the reader that keeps it.
6. **T3.6** Init-tier: `FindConfigByName` resolves all three new names, each `IsValidConfig()`, and each patrol module builds a **non-empty cycling PERIMETER** plan (the three configs that *are* allowed to move).
7. **T3.7** `context.md`: the retired-symbol grep verdicts and the T3.2 divergence.

**Acceptance criteria**

- compile **0**; Fast and All green.
- `grep -rn "OVT_BaseUpgradeDefensePatrol\|OVT_BaseUpgradeTownPatrol\|RecoverResources" Scripts/` → empty (or a recorded, justified survivor).
- `git diff Configs/BaseUpgrades/overthrowBaseUpgrades.conf` shows **exactly one** entry removed.
- `git diff Scripts/Game/GameMode/Virtualization/` → **empty**.
- `grep -rn "OVT-VIRT-PLAYTEST-ONLY" Scripts/ | wc -l` → **unchanged at 5** (both guards' code still has other users).

---

### Phase 4 — Exact placement: defense positions, tower guards, sniper positions

**Agent:** `component-developer-advanced` — **advanced.** It ships the elevated-placement path and the first Persistence-tier case for a base-defense deployment; a mistake puts snipers on the ground or leaves them stranded after a respawn.
**Estimate:** 10–14 h
**Suite after this phase:** **All** `{6A6E2A002F53A581}`

**Tasks**

1. **T4.1** Author `Deployment_BaseDefensePositions.conf`: placed-infantry module with `OVT_BaseDefendPositionPlacementProvider`, `heavy_infantry`, `m_iMaxGroupCount` = `m_Difficulty.defenseGroupsBaseMax` (**5**), cost per group matched to the legacy `baseResourceCost × 4` (= 60 on Normal); `OVT_PatrolBehaviorDeploymentModule` **DEFEND**; Reinforcement; BaseControl; NoPlayersNearby. Priority **2**.
2. **T4.2** Author `Deployment_BaseTowerGuards.conf`: placed-infantry + `OVT_TowerCoverPostPlacementProvider`, `sniper`, `m_eImportance HIGH`, cost per group = `baseResourceCost` (15 on Normal), **no behaviour module** (legacy parity — the guard gets no waypoint on purpose). Priority **2**.
3. **T4.3** Author `Deployment_BaseSniperPositions.conf`: placed-infantry + `OVT_SniperMarkerPlacementProvider`, `sniper_team`, `m_eImportance HIGH`, cost per group = `baseResourceCost × 2` (30 on Normal), no behaviour module. Priority **2**. The per-marker threat gate lives in the provider.
4. **T4.4** Append three registry entries.
5. **T4.5** **Delete `OVT_BaseUpgradeDefensePosition.c`, `OVT_BaseUpgradeTowerGuard.c`, `OVT_BaseUpgradeSniperPosition.c`** and their three `overthrowBaseUpgrades.conf` entries in the same commit.
6. **T4.6 — The re-materialisation claim, made testable.** Expose the placement decision as a pure method on the module — `array<ref OVT_DeploymentPlacement> ResolvePlacements(vector position, float radius, int factionIndex, float threat)` — and a pure applier — `vector PlacementForArrival(int groupIndex, int arrivalIndex)` — so "the second materialisation places men on the same posts as the first" can be asserted without live combat. Integration used exactly this shape for `EvaluateCapture` and recorded why: the alternative needs a live deployment marker, which leaks a repeating 10 s `UpdateDeployment` into the shared test world.
7. **T4.7 — Init-tier coverage:** all three configs resolve and validate; **none** of them produces a movable plan (tower/sniper produce **null**, defense positions produce a **one-point DEFEND, non-cycling** plan) — this is the "garrisons never wander" claim at its root; `PlacementForArrival` is stable across two simulated materialisations and wraps correctly when more men arrive than posts; the sniper provider filters a marker whose `m_iMinimumThreat` exceeds the current threat.
8. **T4.8 — Persistence-tier coverage (the requirement's "base-defense deployment round trip").** On the shared gate (`OVT_TEST_PersistenceRoundTripSuite`, All group): build a base-defense deployment component state (config name, faction, threat, invested resources, virtual key), save, dirty, re-apply, and assert all five return and `FindConfigByName` still resolves the config name. ⚠ **The suite's reload seam cannot reload a deployment marker at all** (`ReapplyLatestSaveData` passes `Instances = {gameMode}` only) — assert the **restore half** (`ApplyPersistedDeployment`) with a real save taken alongside, exactly as integration's T7.2–T7.4 do, and say so at the top of the fixture. **Do not widen the seam.**
9. **T4.9 — Fixture discipline.** ⚠ A deployment fixture is safe only if it is marked `SetSpawnedUnitsEliminated(true)` on the **deployment and every spawning module** before anything can tick — "short" is not safe, because `InitializeDeployment` arms a repeating 8–12 s `UpdateDeployment` whose first tick registers real groups at the global 1750 m ring with the autotest camera inside it. Re-run `grep -rn "RegisterGroup(" Scripts/Game/Tests/` and record a verdict per site in the table shape integration's `context.md` uses.
10. **T4.10** `context.md`: the placement verdicts, and an explicit note that **`ReleaseAction()` was not ported** with the reason and the play-test check that replaces it.

**Acceptance criteria**

- compile **0**; Fast and All green.
- `grep -rn "OVT_BaseUpgradeDefensePosition\|OVT_BaseUpgradeTowerGuard\|OVT_BaseUpgradeSniperPosition" Scripts/ Configs/` → **empty**.
- `grep -rn "m_ProxiedGroups\|m_iProxedResources\|m_ProxiedPositions" Scripts/` → **only** `OVT_BasePatrolUpgrade.c` and its two remaining subclasses (Checkpoints, Composition, Specops) — the count strictly decreases.
- `git diff Scripts/Game/GameMode/Virtualization/` → **empty**.
- Every new case carries a recorded can-fail proof; **no `maxAttempts`**.

---

### Phase 5 — Static content: checkpoints, fortifications, parked vehicles

**Agent:** `component-developer-advanced` — **advanced.** It puts persistence-tracked entities into the world from a deployment module for the first time, and the failure mode is a base that grows a second bunker on every load.
**Estimate:** 8–12 h
**Suite after this phase:** **All** `{6A6E2A002F53A581}`

**Tasks**

1. **T5.1** Author `Deployment_BaseCheckpoints.conf`: two composition modules (`LargeCheckpoint` on `ROAD_LARGE`, `MediumCheckpoint` on `ROAD_MEDIUM`), each with a `light_patrol` guard group and `m_bSnapToRoad 0`; DEFEND behaviour; Reinforcement; BaseControl; NoPlayersNearby. Priority **3**, cost = the legacy hardcoded 60 + 40 plus guard cost.
2. **T5.2** Author `Deployment_BaseFortifications.conf`: three composition modules (`SmallBunker` with a `bunker_team` guard, `AmmoCache` with `m_bFillAmmoBoxes 1`, `MGNest`), all `SMALL` slots. Priority **4**, cost = 10 + 9 + 15 plus the guard.
3. **T5.3** Author `Deployment_BaseParkedVehicles.conf`: parked-vehicle module, `truck` ×1 (cars stay at 0 — `m_iNumberOfCars 0` is what ships today), cost `baseResourceCost × 6` (90 on Normal). Priority **10**. **No reinforcement module** — there is nothing to rebuy, and a config with no reinforcement module also has no `m_bDeleteOnConditionFail` path (integration recorded that the delete branch lives *inside* `CheckReinforcement()`), so this deployment is never collected. That is correct for parked trucks and must be stated in the config's own `context.md` note, because it is a trap for the next author.
4. **T5.4** Append three registry entries.
5. **T5.5** **Delete `OVT_BaseUpgradeCheckpoints.c`, `OVT_BaseUpgradeComposition.c`, `OVT_BaseUpgradeParkedVehicles.c`, `OVT_SlottedBaseUpgrade.c`** and their five `overthrowBaseUpgrades.conf` entries (Checkpoints, three Compositions, ParkedVehicles) in the same commit. `OVT_SlottedBaseUpgrade` has no other subclass — grep-prove.
6. **T5.6 — The no-double-build claim.** Assert it at the seam: a composition module on a deployment whose `WasRestoredFromSave()` is true spawns **nothing** and registers **nothing**; the same module on a fresh deployment builds once and, called a second time, builds nothing more (idempotence). Both are Init-tier and neither needs a live slot — resolve the module's decision through a pure predicate the way T4.6 does.
7. **T5.7 — Slot bookkeeping.** Assert that a composition module's slot claim lands in the nearest base controller's `m_aSlotsFilled`, and that a slot already in `m_aSlotsFilled` is never selected. This is the mechanism that makes a legacy save's compositions and a new deployment's compositions coexist without overlapping.
8. **T5.8** `context.md`: the parked-vehicles "never collected" note, and a decode of the composition path's persistence claim (which entity is tracked, which is not).

**Acceptance criteria**

- compile **0**; Fast and All green.
- `Configs/BaseUpgrades/overthrowBaseUpgrades.conf` now contains **exactly one** entry: `OVT_BaseUpgradeSpecops`.
- `grep -rn "OVT_SlottedBaseUpgrade\|OVT_BaseUpgradeComposition\|OVT_BaseUpgradeCheckpoints\|OVT_BaseUpgradeParkedVehicles" Scripts/ Configs/` → **empty**.
- `git diff Scripts/Game/GameMode/Virtualization/` → **empty**.

---

### Phase 6 — One funding path, the specops drop, and legacy save conversion

**Agent:** `component-developer-advanced` — **advanced, and the highest-risk phase in the feature.** It rewrites the occupying faction's economy loop, deletes the last live legacy spend path, and changes what a pre-migration save means. A mistake here is a campaign that never funds its defense, or one that double-pays.
**Estimate:** 10–14 h
**Suite after this phase:** **All** `{6A6E2A002F53A581}`

**Tasks**

1. **T6.1 — Read-only survey first.** Re-verify and record: every writer of `m_iResources` (expected `:276, :358, :689, :1198-1207, :1284-1292, :1429, :1465-1471`, plus `OVT_QRFControllerComponent.c:299-300`); every reader outside the manager (`OVT_GMRequestComponent.c:553`, the QRF controller, `OVT_BaseUpgradeSpecops.c:55`, the serializer at `:134`, two Persistence-suite pass-throughs); and every caller of `AddFactionResources` (expected one, `OVT_OccupyingFactionManager.c:1449`). **Do not touch the serializer's field order, `RplSave`/`RplLoad`, or `ApplyPersistedOccupyingFaction`'s structure.**
2. **T6.2 — Delete the base-spend loop.** In `CheckUpdate` (`:1168-1213`), keep `GainResources()` and the 80 % computation; replace the sorted-bases loop with a single unconditional `AllocateDeploymentResources(toSpend); m_iResources -= toSpend;`. Delete `UpdateKnownTargets()`'s call **only if** specops was its only consumer (check — the counter-attack path may read `m_aKnownTargets`). Keep the threat decay, the counter-attack block and both early returns (0 players, active QRF) exactly as they are.
3. **T6.3 — Fold the initial distribution into one pool credit.** Delete `DistributeInitialResources()` and its `CallLater` at `:325-326`; in `NewGameStart()`, replace the free `AllocateDeploymentResources(baseResourcesPerTick)` at `:307` with one credit of `baseResourcesPerTick + Σ(startingResources × base.m_fStartingResourcesMultiplier)` over the registered bases. ⚠ **Ordering:** bases must already be discovered when this runs; if they are not, keep a `CallLater` at the same +5 s the deleted method used and state why in the comment. `m_bDistributeInitial` (`:147`, cleared at `:360` on restore) keeps its meaning: **a continued campaign does not re-seed.**
4. **T6.4 — Delete `AllocateDeploymentResourcesIfNeeded()`** (`:1455-1474`) and its call from `GainResources()` (`:1434`). Its 500/1000 arbitration exists only because there were two spenders. Keep `AllocateDeploymentResources()` (`:1442-1452`) as the single credit point and give it a header saying so.
5. **T6.5 — The specops drop** ([D3](#d3--specops-is-dropped-with-no-replacement)). Delete `OVT_BaseUpgradeSpecops.c`, its `overthrowBaseUpgrades.conf` entry (the file is now empty of entries), `UpdateSpecops()` (`:1270-1296`) and both call sites (`:769` goes with T6.3, `:1211` with T6.2). Verify `StartBaseQRF`'s other two callers survive (`:1224` counter-attack, `OVT_CampaignRequestComponent.c:177` player capture) and that `m_aKnownTargets` / `UpdateKnownTargets` either keep a reader or go with it. Write the loss list from [D3](#d3--specops-is-dropped-with-no-replacement) into `context.md`.
6. **T6.6 — The save conversion** per [§3.6](#36-the-save-conversion-flow). Rewrite `ApplyPersistedBaseUpgrades` (`:462-499`) to sum rather than copy, accumulate across bases, and credit the pool **once** after every base record is read. Delete the upgrade-replay block in `InitBaseControllers` (`:726-734`) and **keep the `slotsFilled` restore (`:735-742`) verbatim**. In `OVT_OccupyingFactionManagerSerializer.WriteBase` (`:250-268`), stop walking `controller.m_aBaseUpgrades` and write an empty `upgrades` array; **keep `OVT_PersistedBaseUpgrade`, `OVT_PersistedBaseUpgradeGroup` and `OVT_PersistedBase.upgrades` declared and read** — the field sits at position 4 of 5 and removing it reorders `garrison`. Keep the `slotsFilled` write (`:270-278`).
7. **T6.7 — Logic-tier coverage for the conversion maths** (a world-free helper the manager calls): the sum of `resources + groups × LEGACY_GROUP_VALUE` over a set of records; an empty record set converts to 0; a record with only a tag and a position (a composition) converts to 0; the conversion of a set already converted (empty arrays) is 0, which is the idempotence claim.
8. **T6.8 — Persistence-tier coverage for the conversion path:** feed `ApplyPersistedOccupyingFaction` a base record carrying legacy upgrade records and assert the deployment pool rises by exactly the computed value and that `OVT_BaseData.upgrades` is left empty; feed it the same records twice and assert the second pass adds nothing on top of a payload that has been rewritten empty. Same structural limits as T4.8 — assert the restore half.
9. **T6.9 — Init-tier:** the 80 % transfer conserves the total (reserve down by exactly what the pool went up by), and the opening seed lands in the pool rather than in `m_iResources`.
10. **T6.10** `context.md`: the T6.1 verdicts, the conserved-total claim, the `LEGACY_GROUP_VALUE` derivation, and the specops loss list.

**Acceptance criteria**

- compile **0**; Fast and All green.
- `grep -rn "SpendResources\|DistributeInitialResources\|AllocateDeploymentResourcesIfNeeded\|UpdateSpecops\|OVT_BaseUpgradeSpecops" Scripts/` → **empty** except `OVT_BaseControllerComponent.SpendResources`, which dies in Phase 7 with its guard.
- `git diff` on `OVT_PersistedBase`, `OVT_PersistedBaseUpgrade`, `OVT_PersistedRadioTower`, the serializer's top-level write/read **order**, `RplSave` and `RplLoad` → **empty**. (`WriteBase`'s *body* changes; the payload shape does not.)
- `grep -rn "AddFactionResources" Scripts/` → the manager's single credit point, the deployment framework's own refund, and nothing else.
- `git diff Scripts/Game/GameMode/Virtualization/` → **empty**.

---

### Phase 7 — The retirement sweep

**Agent:** `component-developer-advanced` — **advanced.** It deletes a directory, a config, a test, a component's whole reason for existing, eight faction attributes authored in two `.conf` files and one 2 000-line prefab, and the epic's kill switch. Every one of those has a way of taking something else with it.
**Estimate:** 8–12 h
**Suite after this phase:** **All** `{6A6E2A002F53A581}`

**Tasks**

1. **T7.1 — Read-only sweep first.** Produce, in `context.md`, a table of every surviving reference to: `OVT_BaseUpgrade`, `m_aBaseUpgrades`, `m_BaseUpgradesConfig`, `FindUpgrade`, `OVT_BaseUpgradeData`, `OVT_BaseUpgradeGroupData`, `GetRandomGroupByType`, each of the eight legacy faction attributes, `m_iMilitarySpawnDistance`, and `baseResourceCost`. Every entry gets a disposition **before** anything is deleted.
2. **T7.2 — Delete the framework tier.** `OVT_BaseUpgrade.c`, `OVT_BasePatrolUpgrade.c`, the whole `Scripts/Game/Controllers/OccupyingFaction/BaseUpgrades/` directory, `Scripts/Game/Configuration/OVT_BaseUpgradesConfig.c`, `Configs/BaseUpgrades/overthrowBaseUpgrades.conf` **and its `.meta`**, and the `m_BaseUpgradesConfig` reference on `Prefabs/Controllers/OVT_BaseController.et:5`.
3. **T7.3 — Shrink the base controller.** Delete `m_aBaseUpgrades`, `m_BaseUpgradesConfig`, the copy loop in `InitializeBase` (`:176-188`), `UpdateUpgrades()` (`:82-92`) **with its `CallLater` at `:73`**, `FindUpgrade()` (`:192-211`) and `SpendResources()` (`:296-329`). **Both `[OVT-VIRT-PLAYTEST-ONLY]` guards (`:84`, `:298`) leave inside those deletions — never un-comment either.** An un-guarded `SpendResources` would revive the legacy spender beside the deployment pool and double every base's force; an un-guarded `UpdateUpgrades` would tick classes that no longer exist. **Keep** the entire slot registry, `FindSlots`/`FindParking`, `GetNearestSlot`, `GetRandomVehiclePatrolSpawn` and the faction/flag half.
4. **T7.4 — Restore the QRF spawn queue.** `OVT_QRFControllerComponent.c:369`'s guard is removed by **deleting the guard line only** and leaving `SpawnFromQueue()` intact. This is the *opposite* operation from T7.3 and the asymmetry is deliberate: QRF is an epic-level exclusion, was never migrated, and its spawner must come back exactly as it was. Assert the difference in `context.md` so a reviewer does not "fix" the inconsistency.
5. **T7.5 — Delete the kill switch.** `Scripts/Game/GameMode/Virtualization/OVT_VirtPlaytestKillSwitch.c`. ⚠ This is the epic's last file to go and it is the one place in the whole epic where core's directory is edited by this feature — a **deletion of a file core does not reference**, which is why [G8](#primary)'s empty-diff criterion is worded as "no change to `OVT_VirtualizationManagerComponent` or `api.md`" for this phase alone. `grep -rn "OVT-VIRT-PLAYTEST-ONLY\|DISABLE_LEGACY_AI_SPAWNS" Scripts/` must return **nothing**.
6. **T7.6 — Retire the legacy faction path** ([D9](#d9--the-legacy-prefab-slot-path-is-deleted-attributes-included)). Delete `GetRandomGroupByType()` (`OVT_Faction.c:471-486`) **only after** its last reader is gone: `OVT_SpawnGroupJobStage.c:30` is a **live, non-base-upgrade reader** and must be re-pointed at `m_GroupRegistry` first (its `OVT_GroupType` maps onto the registry names T2.7 authored). Then delete the eight attributes (`m_aGroupInfantryPrefabSlots`, `m_aHeavyInfantryPrefabSlots`, `m_aGroupATPrefabSlots`, `m_aGroupSpecialPrefabSlots`, `m_aGroupSniperPrefab`, `m_aGroupSniperTeamPrefab`, `m_aLightTownPatrolPrefab`, `m_aTowerDefensePatrolPrefab` — integration's hand-off) **and every authored value** in `Configs/Factions/USSR_OverthrowData.conf`, `US_OverthrowData.conf` and `Prefabs/GameMode/OVT_FactionManager.et`. ⚠ **Delete the attribute and its authored values in the same commit** — an authored value with no attribute is a parse warning, and an attribute with no reader is the debt integration handed over. Also retire `m_aMediumCheckpointPrefab` / `m_aLargeCheckpointPrefab` if T2.7 moved them into the composition config. Re-point the two Init-suite fixtures that read the arrays as fallbacks (`OVT_TEST_InitSuite.c:1603-1606, :1840-1843`).
7. **T7.7 — Delete `m_iMilitarySpawnDistance`** (`OVT_OverthrowConfigComponent.c:213`), integration's fourth hand-off. Its last production reader was `OVT_BasePatrolUpgrade.PlayerInRange()`, which died in T7.2; it is **not authored in any prefab, config or world** (verified 2026-08-17), and it is absent from the config component's `RplSave`/`RplLoad`. Rewrite the three comments that cite it and the one test diagnostic (`OVT_TEST_Campaign_GMGroupRegistry.c:374`). If any reader survives, leave the attribute and record why.
8. **T7.8 — Re-point the GM snapshot** ([G9](#secondary)). `OVT_GMSnapshotBuilder.BuildBases()` (`:143-193`) stops reading `controller.m_aBaseUpgrades` and instead enumerates the deployments within the base's radius: one `OVT_GMBaseUpgradeRecord` per deployment (`m_sType` = config name, `m_iResources` = `GetResourcesInvested()`, `m_iGroups` = registered group count), and `OVT_GMBaseRecord.m_iUpgrades` = that count. Add whatever minimal public accessor the group count needs (e.g. `int GetRegisteredGroupCount()` on `OVT_BaseSpawningDeploymentModule`, returning 0 in the base class). **Do not change the record classes, their field order, or the RPC** — they are on the GM wire.
9. **T7.9 — Tests and enums.** Delete `OVT_TEST_Campaign_BaseUpgrades_ConfigLoaded.c`. Update `OVT_TEST_Campaign_GMGroupRegistry.c`'s header, which documents the base-upgrade ledger and `BASE_PATROL` tagging as expected registry sources. **Leave `OVT_EGroupOrigin.BASE_PATROL`, `BASE_DEFENCE`, `BASE_SNIPER`, `TOWER_GUARD` and `TOWN_PATROL` declared** even though they lose their producers — the enum's integers travel in the GM snapshot stream and renumbering would mislabel every group on a mismatched client (the rule integration recorded for `RADIO_TOWER_GARRISON`). `BASE_GARRISON` keeps its producer (the resistance garrison restore). `OVT_TEST_Logic_GMIconFormat.c:53` asserts a pure string transform and needs no change — verify rather than assume.
10. **T7.10 — Comments.** Re-word `OVT_OccupyingFactionManager.c:440-443` (integration's third hand-off — the behaviour it justifies is still required, only its stated reason is stale) and `OVT_OccupyingFactionManagerSerializer.c:92` (a stale `:438-490` line citation).
11. **T7.11 — `context.md`:** the T7.1 disposition table with a grep verdict per entry, and the final kill-switch ledger.

**Acceptance criteria**

- compile **0**; Fast and All green.
- `grep -rn "OVT-VIRT-PLAYTEST-ONLY\|DISABLE_LEGACY_AI_SPAWNS\|OVT_VirtPlaytestKillSwitch" Scripts/` → **completely empty**. 🎉 **The epic's kill switch is gone.**
- `ls Scripts/Game/Controllers/OccupyingFaction/BaseUpgrades/ Configs/BaseUpgrades/` → both **absent**.
- `grep -rn "BaseUpgrade\|GetRandomGroupByType\|m_iMilitarySpawnDistance" Scripts/ Configs/ Prefabs/` → **empty**, or a recorded, justified survivor per entry.
- `git diff Scripts/Game/GameMode/GM/OVT_GMRecords.c Scripts/Game/Components/Controller/OVT_GMRequestComponent.c` → **empty** (records and RPC untouched; only the builder changed).
- `git diff Scripts/Game/GameMode/Virtualization/OVT_VirtualizationManagerComponent.c docs/features/virtualization/core/api.md` → **empty**.
- `git diff --stat` over the feature shows a **large net deletion**.

---

### Phase 8 — Help & documentation sync

**Agent:** `help-docs-sync`
**Estimate:** 2–3 h
**Suite:** skipped — docs-only. Say so.

Players see different base behaviour, so the closing sync is in scope.

1. **T8.1** Fact-check **every** existing sentence in `Configs/Tutorials/` and `Configs/FieldManual/` about bases, garrisons, fortifications and attacking a base against the shipped code, and cite a `file:line` or cut the sentence. The project has shipped invented mechanics twice; no gate catches a well-formed lie.
2. **T8.2** The player-visible changes to document:
   - **a base's defense is what you left it as** — men you killed stay dead across leaving, returning and reloading;
   - **bases fortify over time, concern by concern**, and a base you are standing in does not fortify while you watch;
   - **a base that has just been taken, or a resource-starved occupier, may be lightly defended**, and it will thicken up later;
   - **tower and sniper posts are manned again after a respawn** — clearing a post is not permanent unless you take the base;
   - **specops raids no longer happen** (they were the "enemy special forces are coming for your FOB" behaviour, [D3](#d3--specops-is-dropped-with-no-replacement)).
3. **T8.3** Wiki: the same points, plus the two operator-facing notes — base defense now spends the same pool as every other deployment, and `m_iMaxDeploymentsPerFaction` is the ceiling that decides whether a big map can fully fortify.
4. **T8.4** Epic bookkeeping: mark `base-defense-migration` complete in `epic-overview.md`, roll the epic to 5/5 in its rollup and in the master `docs/overview.md`, and record in `api.md` §6 that **no** system reads `m_iMilitarySpawnDistance` any more (integration's T8.4 left that sentence naming base upgrades and QRF).
5. **T8.5 — The epic's closing ledger.** State, with the grep output, that `OVT-VIRT-PLAYTEST-ONLY` returns nothing and that the switch file is deleted — the epic's final acceptance.

**Acceptance criteria** — all three surfaces agree with the code; no invented mechanics; every claim carries a `file:line`; the closing ledger is empty.

---

## 5. Key Technical Decisions

### D1 — Per-concern configs, each independently costed and priority-gated

*(user decision, 2026-08-17 — binding)*

> **Per-concern configs**: several `Configs/Deployment/Deployment_Base*.conf` files, each copying the `Deployment_TowerGarrison.conf` shape — e.g. garrison patrol, defend positions, tower/sniper guards, checkpoints, fortification compositions, parked vehicles. Each independently costed/priority-gated so bases escalate concern-by-concern with threat/resources, mirroring today's upgrade priorities.

Implemented as the nine configs of [§3.2](#32-the-concern--config-mapping), each carrying the `m_iPriority` its legacy upgrade authored. Two groupings were made inside that instruction and are called out rather than hidden: the three fortification compositions share one config (one concern, one purchase, priority 4), and the two checkpoint sizes share one config. Threat escalation of a single concern (the defense patrol's light → heavy → AT ladder) is expressed as **variant configs** with `m_iMinimumThreatLevel`, because that gate is config-level and the framework offers no per-module equivalent.

The rejected alternative was one `Deployment_BaseDefense.conf` with every module in it: one purchase, one cost, no escalation, and a base either fully fortified or not at all — the opposite of what the legacy priority sweep does.

### D2 — Preserve elevated placement, via a generalized exact-placement spawning module

*(user decision, 2026-08-17 — binding)*

> **Preserve elevated placement** for tower guards and sniper positions, via a NEW spawning deployment module that supports **exact placements** for any deployment that needs it (generalized, not tower-specific). Important correction: **ladder pathing was fixed in Reforger 1.8 and Overthrow has had zero issues with tower/sniper placements since** — do NOT treat elevated AI as hazardous or argue for ground-level fallbacks. The existing teleport-to-post spawn pattern in `OVT_BaseUpgradeTowerGuard` / `OVT_BaseUpgradeSniperPosition` is the proven mechanism; re-materialization after despawn must re-run the placement.

`OVT_PlacedInfantrySpawningDeploymentModule` + `OVT_DeploymentPlacementProvider` ([§3.3](#33-the-new-modules)). No ground-level fallback exists anywhere in this plan; no config authors one; no risk entry proposes one. The provider seam is what makes it general — a future config that wants men on a rooftop, a bridge or a bunker firing step authors a provider and changes nothing else.

The one genuinely new obligation over the legacy code is the **re-materialisation** requirement: legacy unsubscribed after placing because its groups spawned exactly once; a registered group materialises repeatedly, so the subscription lives as long as the registration and the arrival counter resets per materialisation.

### D3 — Specops is dropped, with no replacement

*(user decision, 2026-08-17 — binding, and the documented dropped-class decision the requirements demand)*

> **Drop specops for now**: `OVT_BaseUpgradeSpecops` is retired with no deployment replacement this feature.

**What is lost, explicitly:**

- **Target-driven special-forces squads.** `UpdateSpecops` walked `m_aKnownTargets` (player FOBs, captured bases, radio towers the resistance holds), found the nearest occupied base, and gave its specops upgrade a target; the upgrade bought a `SPECIAL_FORCES` group and waypointed it to the target. Nothing in the deployments framework enumerates player-held targets, so this has no cheap equivalent.
- **The `StartBaseQRF` hook.** On arriving within 20 m of a resistance-held **base**, the specops group triggered a QRF on it (`OVT_BaseUpgradeSpecops.c:45-51`). `StartBaseQRF` keeps its other two callers — the counter-attack roll (`OVT_OccupyingFactionManager.c:1224`) and player-initiated capture (`OVT_CampaignRequestComponent.c:177`) — so QRFs still happen; they just no longer arrive *because a specops team walked to your base*.
- **The 600 s radio-tower recapture timer.** On arriving at a resistance-held **broadcast tower**, the group sent a `"RadioTowerCapture"` notification and counted `m_iCaptureTimer = 600000` down to `ChangeRadioTowerControl` (`:52-71`). **The occupying faction loses its only way to take a radio tower back.**

**Why it is an acceptable drop, beyond the user's instruction:** QRF behaviour is an epic-level exclusion in the requirements, and specops is one third QRF trigger by construction. And the class is **already broken at HEAD in a way that costs the player nothing to remove**: `SetTarget` still debits `m_iResources` and still spawns groups (`UpdateSpecops` is not kill-switched), but its `OnUpdate` — the arrival check, the QRF hook and the capture timer — runs only from `OVT_BaseControllerComponent.UpdateUpgrades`, which **is** kill-switched. So today the occupying faction pays for specops squads that spawn, stand around and never do anything. Dropping the class fixes a live resource-and-AI leak.

If it is ever wanted back, the shape is a `Deployment_SpecopsRaid.conf` with a target-position condition module — which is a feature, not a migration, and belongs to the occupying epic.

### D4 — Legacy save conversion is a value refund to the pool

*(user decision, 2026-08-17 — binding)*

> **Legacy save conversion = value refund to pool**: on first load of a pre-migration save, sum each base's persisted upgrade value (banked `resources` + group value) from the old payload and credit the occupying faction's deployment resource pool; the evaluator then re-establishes defense naturally by threat. No per-base re-establishment code.

Flow in [§3.6](#36-the-save-conversion-flow). Three consequences worth stating so nobody reads them as bugs:

1. **A loaded legacy campaign is briefly under-defended.** The men are gone (they were never in the save as entities — the patrol upgrades persisted *prefab names and positions*, which the deleted `Deserialize` replayed), the money is in the pool, and the evaluator spends it over the following minutes. The requirement asks for *value-parity, not entity-identity*, and this is exactly that.
2. **Compositions and checkpoints do not vanish.** They are `OVT_PersistenceTracking`-tracked world entities and come back from the save on their own; their slots come back in `slotsFilled`. They convert to **zero** value on purpose, because refunding for them would pay for them twice.
3. **The conversion is idempotent by construction**, not by a flag: after the first load the write path stores an empty `upgrades` array.

The rejected alternative — replaying each base's upgrades into equivalent deployments — needs a class→config map, a position→slot map and a groups→handles map, all for a one-time migration, and would create deployments the evaluator's dedup then has to reconcile.

### D5 — The evaluator escalates by priority, and that **is** how a base fortifies

Two blocking facts, both verified 2026-08-17 and both invisible today because only one config class ever authored `BASE`:

- `FindDeploymentCandidates` filters through `IsPositionSuitableForDeployment`, which vetoes any candidate within **100 m** of any existing deployment (`:860-862`). A base with one deployment on it is permanently off the candidate list.
- `FindBestDeploymentConfig` returns **one** config per position, and the caller `continue`s when it already exists nearby (`:614-621`) rather than trying the next-best.

[§3.4](#34-the-evaluator-change--how-a-base-fortifies-concern-by-concern) fixes both with two small edits, and the result is a direct re-expression of `SpendResources`' 1..19 priority sweep — which is why the mapping table gives each config its legacy priority verbatim.

Rejected alternatives: **one config per base with many modules** (no escalation, no per-concern cost — fails D1); **a bespoke base-fortification scheduler outside the evaluator** (a second decision-maker, i.e. the exact architecture this feature exists to end); **marking every base config `m_bFreeAtGameStart`** (bypasses the pool, so base defense would be free while towns and towers are not — fails requirement 3, and is kept only as [R6](#9-risks--mitigation)'s fallback).

The one behaviour change outside base defense: **removing the 100 m veto lets any two different configs co-locate**, anywhere. Same-config stacking is still forbidden by the 250 m name-scoped dedup, which is the rule that actually mattered. This also un-sticks a latent defect integration recorded — a tower inside a town could never be chosen by the evaluator, because Town Patrol and Tower Garrison tie at priority 1 and the tie went to registry order.

### D6 — The new spawning modules subclass the infantry module, deliberately

`OVT_PlacedInfantrySpawningDeploymentModule` and `OVT_CompositionSpawningDeploymentModule` extend `OVT_InfantrySpawningDeploymentModule` rather than `OVT_BaseSpawningDeploymentModule`. The decisive reason is not code reuse — it is that `OVT_ReinforcementBehaviorDeploymentModule.GetMissingUnitsCount()` casts to `OVT_InfantrySpawningDeploymentModule` and returns **0** for every other module type (`:186-197`), so a sibling class would silently never be rebought and nobody would notice until a play-test.

They also inherit convergence, reclaim, the two eliminated gates, wipe accounting, GM re-tagging and faction-key resolution — all of which integration built and debugged once.

The cost is `CloneModule` fragility: each subclass must hand-copy **its own and all twelve inherited attributes**, and a forgotten one ships silently wrong. T2.8 asserts the clone of every new module carries every authored attribute, which is the only mechanical defence available.

`OVT_ParkedVehicleSpawningDeploymentModule` extends `OVT_BaseSpawningDeploymentModule` instead, because it registers no groups at all and inheriting group machinery it never uses would be worse than not having it.

### D7 — A restored deployment never rebuilds static content

Compositions, checkpoints and parked vehicles are *entities*, persisted by vanilla persistence (`OVT_PersistenceTracking.Track`, and the vehicle manager for vehicles) and restored by the engine before any deployment ticks. Their slot claims already round-trip through `controller.m_aSlotsFilled`, which the serializer writes and `InitBaseControllers` restores — a path this feature keeps **verbatim**.

So a restored deployment's static modules must do nothing. `OVT_DeploymentComponent.WasRestoredFromSave()` — a runtime bool set in `ApplyPersistedDeployment`, never persisted — is the gate. Without it, a base would grow one more bunker per load, in a different slot each time, forever.

This is exactly what legacy `OVT_BaseUpgradeComposition.Deserialize` already did: it restored `m_vPos` and re-ran `Setup()` (which re-mans turrets) and **deliberately did not rebuild**. Parity, not innovation.

The known gap, inherited: a composition destroyed while the campaign was saved is never rebuilt. Legacy had the same gap. If play-test shows bases visibly decaying, the upgrade path is to append the built slot positions to `OVT_DeploymentComponentSerializer` behind a **version 3** bump (append-only, last field) and have the module adopt them on restore — designed, costed and **not built** here on YAGNI grounds.

### D8 — One funding path: delete the second spender, do not add a broker

`CheckUpdate`'s per-base loop, `DistributeInitialResources`' per-base spend and `AllocateDeploymentResourcesIfNeeded`'s conditional drip are all deleted. The same 80 % of every resource tick goes unconditionally to `AddFactionResources`, and the opening budget is one credit ([§3.5](#35-the-funding-single-path-cutover)).

`m_iResources` is **not** deleted. It remains the occupying faction's reserve for QRF sizing (`OVT_QRFControllerComponent.c:223-250, :299-300`) and the counter-attack roll (`> 2000`, `:1217`) — both epic-level exclusions — and it remains what the GM campaign panel shows (`OVT_GMRequestComponent.c:553`, which already shows both pools side by side). "One accounting path" is a claim about **defense spending**, and after this phase there is exactly one.

Two things deliberately not done:

- **Difficulty is not wired into deployment costs.** `GetTotalResourceCost(int difficultyMultiplier = 1)` accepts a multiplier no caller passes; passing `baseResourceCost` would silently reprice town patrols and vehicle patrols too — a rebalance the requirements exclude. Difficulty keeps scaling defense through the pool's income (`baseResourcesPerTick`: Easy 150, Normal 250) and through `patrolGroupsMin/Max` in `CalculateGroupCount`. ⚠ Integration recorded that `CalculateGroupCount` halves the difficulty band and then clamps to the module's min/max, so difficulty scaling of *group counts* is compressed; the dial is each config's `m_iMaxGroupCount`.
- **The threat-weighted per-base split is not reproduced literally.** Today `perBase` is an **even** split and threat only decides serving order (`:1185-1187`). The evaluator's threat-sorted candidate list with ±20 % jitter is a closer match to the *intent* than the code was.

### D9 — The legacy prefab-slot path is deleted, attributes included

`GetRandomGroupByType()` and the eight `Legacy Faction Groups` attributes go, along with **every authored value** in the two faction configs and `Prefabs/GameMode/OVT_FactionManager.et`, in one commit. Deleting an attribute while its authored values survive produces a parse warning on every load; keeping the attribute with no reader is precisely the debt integration handed over for `m_aTowerDefensePatrolPrefab`.

Two readers must be dealt with **first**, and neither is a base upgrade:

- **`OVT_SpawnGroupJobStage.c:30`** — a live job stage calling `GetRandomGroupByType(m_GroupType)`. Re-point it at `m_GroupRegistry` using the names T2.7 authors. (Note its default branch falls through to `m_aGroupPrefabSlots`, which is built from the **vanilla GROUP entity catalog** at `OVT_Faction.Init()` and is *not* one of the legacy arrays — that field survives.)
- **`OVT_TEST_InitSuite.c:1603-1606, :1840-1843`** — two fixtures reading the arrays as prefab fallbacks. Re-point at the registry.

`m_aGroupMGPrefab`, `m_aGroupATPrefab`, `m_aGroupFRAGPrefab`, `m_aHeavyTownPatrolPrefab`, `m_aSpecOpsPatrolPrefab` are already unauthored and unread — sweep them in the same pass if T7.1 confirms it, and say so rather than leaving them for a future reader to wonder about.

### D10 — Nothing new replicates, and the save format does not move

No `Rpc`, no `RplProp`, no new prefab, no UI, no `CONFIG_STREAM_VERSION` change. The deployment serializer stays at **version 2**; the occupying-faction serializer keeps every payload class, every field and every field's position — `WriteBase`'s *body* changes to write an empty array, which is a value change, not a format change. `Rpc()` arity is a compile-check blind spot in this tree (BUG-090), so a feature with zero remote calls is the safest shape available and the acceptance greps enforce it.

The one wire-adjacent edit is `OVT_GMSnapshotBuilder.BuildBases()` (T7.8), which changes *what it puts in* the existing records — the record classes, their field order and the RPC that carries them are untouched, and an empty `git diff` on them is an acceptance criterion.

---

## 6. Definition of Done

An evaluator with **no implementation context** should be able to verify every item below.

### Functional Criteria

- **F1 — Dead members stay dead at a base (the headline).** Find a base defense position, kill 2 of its 4 men, drive 3 km away, come back. **Expect:** exactly 2 men, at the same post — not 4.
- **F2 — Dead members stay dead across a save.** Repeat F1, then save → quit → **Continue**. **Expect:** still 2 men, at the same post.
- **F3 — Tower guards are on the tower.** Approach a base with a tower. **Expect:** the guard is standing on the tower's **open-air walkway**, not on the ground and not inside the glass cabin, and he shoots at you from there.
- **F4 — Sniper teams are on their markers.** At a base with sniper positions, **expect** a two-man team at each marker within the threat gate, facing the direction the marker points, one step apart.
- **F5 — Placement survives re-materialisation.** Drive far enough for a tower guard to despawn, then come back. **Expect:** he is back **on the walkway**, not on the ground beneath it. Repeat after a save/reload.
- **F6 — A base fortifies concern by concern.** Watch a freshly-captured-by-the-occupier base (or a fresh campaign) from a distance over several minutes. **Expect:** it acquires a garrison patrol first, then defense positions/tower guards/sniper teams, then checkpoints, then fortifications, then a parked truck — in that order, one per evaluation cycle, each stopping when the pool runs dry.
- **F7 — A base does not fortify around a player standing in it.** Sit inside a base for several evaluation cycles. **Expect:** no new deployment is created there while you are within ~320 m. Walk 1 km away, wait, come back: it has grown.
- **F8 — No duplication across a Continue.** Save with several bases fortified, quit, **Continue**. **Expect:** the same number of groups, compositions and vehicles — not double. In particular, **no base has a second bunker, ammo cache, MG nest or checkpoint.**
- **F9 — No duplication across two campaigns in one session.** Start a second campaign without restarting the client. **Expect:** no doubled registrations, no doubled deployments.
- **F10 — Resource accounting is closed.** In the GM campaign panel, watch the occupying reserve and the deployment pool across one 6-hour resource tick. **Expect:** the reserve rises by the tick's gain and then falls by exactly 80 % of it, the pool rises by exactly that 80 %, and no other number moves unexplained.
- **F11 — A wiped base defense stays wiped until it is rebought.** Clear a base's garrison entirely and leave. **Expect:** it does not silently come back at full strength on your return; it is rebought only when the occupier can afford it, and only via reinforcement or a fresh deployment.
- **F12 — A legacy save converts (player-visible).** Keep a campaign saved **before** this feature with several fortified bases. Load it. **Expect:** the occupying faction's **deployment pool jumps** by a large amount at load (visible in the GM panel), the bases' compositions/checkpoints are **still standing** (they are tracked entities), the old patrols are gone, and over the following minutes the bases re-acquire garrisons from the new configs. Nothing is duplicated.
- **F13 — Specops is gone and nothing else broke.** No enemy special-forces squad walks to your FOB. QRFs still trigger from a counter-attack roll and from capturing a base. **Expect:** no errors, no orphaned targets.
- **F14 — GM icons and the GM base panel still work.** Open the GM view. **Expect:** base-defense groups carry `DEPLOYMENT` icons with their config names, including while dormant; the base panel lists the deployments at each base with non-zero invested resources instead of an empty upgrade list.
- **F15 — The kill switch is gone and QRF still spawns.** Trigger a QRF. **Expect:** it spawns waves as it did before the epic. `grep -rn "OVT-VIRT-PLAYTEST-ONLY" Scripts/` returns nothing.

### Quality Criteria

- **Q1** `tools/compile-check.sh` exits **0** with no output.
- **Q2** Fast `{6A6E29FF47ECB840}` and All `{6A6E2A002F53A581}` both exit **0** at the end of every phase.
- **Q3** Every new test case carries a recorded proof that it can fail — the exact edit, in a preamble comment. **No `maxAttempts` anywhere.**
- **Q4 The base-upgrades system is gone:** `ls Scripts/Game/Controllers/OccupyingFaction/BaseUpgrades/ Configs/BaseUpgrades/` → both absent; `grep -rn "BaseUpgrade" Scripts/ Configs/ Prefabs/` → empty except the persisted payload classes (`OVT_PersistedBaseUpgrade`, `OVT_BaseUpgradeData`) that legacy saves still need read.
- **Q5 Banked-value proxying is gone:** `grep -rn "m_ProxiedGroups\|m_iProxedResources\|m_ProxiedPositions\|BuyPatrol" Scripts/` → **empty**.
- **Q6 One spender:** `grep -rn "SpendResources\|DistributeInitialResources\|AllocateDeploymentResourcesIfNeeded\|UpdateSpecops" Scripts/` → **empty**.
- **Q7 The legacy faction path is gone:** `grep -rn "GetRandomGroupByType\|PrefabSlots" Scripts/ Configs/ Prefabs/` → empty except `m_aGroupPrefabSlots` (the vanilla-catalog-derived field, which survives) and `m_aVehicle*PrefabSlots` if T7.1 finds surviving readers.
- **Q8 The kill switch ledger is empty:** `grep -rn "OVT-VIRT-PLAYTEST-ONLY\|DISABLE_LEGACY_AI_SPAWNS" Scripts/` → **nothing**, and the switch file does not exist.
- **Q9 Core is untouched:** `git diff Scripts/Game/GameMode/Virtualization/OVT_VirtualizationManagerComponent.c docs/features/virtualization/core/api.md` → **empty** across the whole feature. (The only file removed from that directory is the kill switch, which core does not reference.)
- **Q10 Nothing new replicates:** `grep -rn "Rpc\|RplProp\|Replication.Bump" Scripts/Game/GameMode/Deployments/` → empty; the GM record classes and their RPC are unchanged.
- **Q11 The save format did not move:** `git diff` on `OVT_PersistedBase`, `OVT_PersistedBaseUpgrade`, `OVT_PersistedRadioTower`, the occupying serializer's write/read **order**, `RplSave`, `RplLoad` → **empty**.
- **Q12 Logic-tier grep clean:** no manager, game-mode, world or entity identifier in the two new world-free helpers or their Logic-tier test files, **comments included**.
- **Q13 Net deletion:** `git diff --stat` over the feature shows substantially more lines removed than added.

### Integration Criteria

- **I1** The four shipped deployment configs (`Deployment_TownPatrol`, both vehicle patrols, `Deployment_TowerGarrison`) are **byte-identical** — `git diff` on those four files is empty. Every new attribute on `OVT_InfantrySpawningDeploymentModule` defaults to today's behaviour.
- **I2** `OVT_PatrolHarassmentStabilityModifier` is not edited and still works.
- **I3** The resistance's FOB garrison path (`OVT_ResistanceFactionManager.c:1079-1092`, which reads `controller.m_AllCloseSlots`) still compiles and works — the base controller's slot registry survives the retirement.
- **I4** `OVT_EGroupOrigin` is **not** renumbered: `BASE_PATROL`, `BASE_DEFENCE`, `BASE_SNIPER`, `TOWER_GUARD`, `TOWN_PATROL` stay declared with no producer; `BASE_GARRISON` keeps its producer.
- **I5** Radio-tower sabotage, capture, persistence and the JIP stream are untouched; `OVT_TEST_PersistenceRoundTrip_TowerSabotage_SurvivesSaveAndReload` stays green.
- **I6** QRF is untouched apart from restoring its spawn queue — `git diff Scripts/Game/Controllers/OccupyingFaction/OVT_QRFControllerComponent.c` is **exactly one deleted line**.
- **I7** `git diff Scripts/Game/GameMode/VirtualMovement/ Scripts/Game/GameMode/Civilians/` → **empty**.

### Verification Method

**Automated — from the repo root, in order:**

1. `tools/compile-check.sh` → exit **0**.
2. `tools/run-tests.sh "{6A6E29FF47ECB840}"` → exit **0** (Fast).
3. `tools/run-tests.sh "{6A6E2A002F53A581}"` → exit **0** (All).
4. `ls Scripts/Game/Controllers/OccupyingFaction/BaseUpgrades/ Configs/BaseUpgrades/` → both **absent**. → Q4
5. `grep -rn "OVT-VIRT-PLAYTEST-ONLY\|DISABLE_LEGACY_AI_SPAWNS" Scripts/` → **empty**. → Q8, F15
6. `grep -rn "m_ProxiedGroups\|m_iProxedResources\|BuyPatrol" Scripts/` → **empty**. → Q5, G3
7. `grep -rn "SpendResources\|DistributeInitialResources\|AllocateDeploymentResourcesIfNeeded\|UpdateSpecops\|GetRandomGroupByType" Scripts/` → **empty**. → Q6, Q7
8. `git diff Scripts/Game/GameMode/Virtualization/OVT_VirtualizationManagerComponent.c docs/features/virtualization/core/api.md` → **empty**. → Q9
9. `git diff Configs/Deployment/Deployment_TownPatrol.conf Configs/Deployment/Deployment_VehiclePatrol_Light.conf Configs/Deployment/Deployment_VehiclePatrol_Heavy.conf Configs/Deployment/Deployment_TowerGarrison.conf` → **empty**. → I1
10. `git diff Scripts/Game/Controllers/OccupyingFaction/OVT_QRFControllerComponent.c` → exactly one deleted line. → I6
11. `ls Configs/Deployment/Deployment_Base*.conf | wc -l` → **9**, and `grep -c "Configs/Deployment/Deployment_Base" Configs/Deployment/overthrowDeployments.conf` → **9**. → G1
12. `git diff --stat` → net deletion. → Q13

**Manual — solo play-test.** Debug affordances on `Prefabs/GameMode/OVT_OverthrowGameMode.et`: core's `m_bDebugRegisterTestGroup = false`; consider temporarily lowering `m_iEvaluationInterval` from 30 000 to compress step 2.

1. **Start a fresh campaign on Normal.** Watch the log for the opening pool credit and the first evaluation. **Expect:** a large one-off credit naming the base count, then deployments appearing over the following minutes. → F6, F10
2. **Pick one base and watch it fortify from >400 m away** over ~6 minutes, checking the GM deployment list each cycle. **Expect:** the acquisition order of [§3.4](#34-the-evaluator-change--how-a-base-fortifies-concern-by-concern), one per cycle, each with a non-zero invested cost. → F6, F14
3. **Walk into that base and stay** for two cycles. **Expect:** nothing new is created while you are there. Leave 1 km, wait two cycles, return: it grew. → F7
4. **Look at the tower.** **Expect:** the guard on the walkway, not the ground, not the cabin — and he engages you. → F3
5. **Find the sniper markers.** **Expect:** a two-man team per marker, facing the marker's direction, one step apart. → F4
6. **Kill 2 of 4 men at a defense position, drive 3 km, return.** **Expect:** 2 men, same post. Then drive far enough for the tower guard to despawn and return: **he is back on the walkway.** → F1, F5
7. **Save → quit → Continue.** **Expect:** every base at the same strength and the same men; **no second bunker, cache, MG nest, checkpoint or truck anywhere**; tower guards back on their walkways. → F2, F5, F8
8. **Clear a base's garrison entirely and leave.** **Expect:** it does not return at full strength on your next visit; it is rebought only when affordable. → F11
9. **Watch the GM campaign panel across one 6-hour resource tick.** **Expect:** reserve up by the gain, then down by 80 %; pool up by exactly that. → F10
10. **Trigger a QRF.** **Expect:** waves spawn normally. → F15
11. **Play for 20 minutes and confirm no specops squad appears**, and that capturing a base still triggers a QRF. → F13
12. **Load a pre-migration save** (keep one). **Expect:** a big pool jump at load, compositions still standing, old patrols gone, bases re-garrisoning over the next minutes, nothing duplicated. → F12
13. **Start a second campaign in the same session.** **Expect:** no doubling of anything. → F9
14. **Pacing/tuning pass.** Record: how long a base takes to reach full fortification, whether the pool is ever the binding constraint, whether `m_iMaxDeploymentsPerFaction` ever logs its ceiling warning, and the AI frame cost of driving across a fully fortified map. Feed the numbers back into the config costs and the two ceilings. → [R6](#9-risks--mitigation), [R7](#9-risks--mitigation)

---

## 7. Testing Strategy

**The automated spine covers the maths, the seams and the round trips. Everything about placement quality, fortification pacing, survivor accuracy in the live game, resource feel and multi-campaign hygiene is a play-test** — the suites cannot see whether a sniper is standing on a walkway or under it.

### Logic tier — Fast, two new files

`TestSuites/Logic/OVT_TEST_Logic_BaseDefenseEscalation.c` (Phase 1) — world-free assertions on the **selection maths** the requirements name:

- given (name, priority) pairs and a set of already-present names, the next pick is the lowest-priority absent one; all-present picks nothing; ties resolve by input order; a set with one entry picks it exactly once.
- `MissingCount`-style clamping for placement: wanted vs available posts never goes negative and never exceeds the post count.

`TestSuites/Logic/OVT_TEST_Logic_BaseDefenseConversion.c` (Phase 6) — the **cost/conversion maths**:

- `Σ(resources) + Σ(groups) × LEGACY_GROUP_VALUE` over a record set; empty set → 0; composition-shaped records (tag + pos, no resources, no groups) → 0; a converted (emptied) set → 0 — the idempotence claim.
- The funding split: 80 % of a tick, floored, plus the conserved-total identity (what leaves the reserve equals what enters the pool).

⚠ Both files must be free of manager/world/entity/`OVT_Global` identifiers **including in comments** — the tier grep matches comments (movement's Phase 1 finding). ⚠ `vector.Distance` is +1 ULP off at 1000 m and 2000 m; assert with tolerances. ⚠ `out` and `owned` are reserved local names.

### Init tier — additions to `TestSuites/Init/OVT_TEST_InitSuite.c` (Fast)

⚠ **Init worlds never run `PostGameStart`** — a case needing a tick installs it itself. ⚠ **The autotest camera is an observer** — every registration uses `spawnDistanceOverride = 0` (Manual policy). ⚠ **Scope every registry assertion to your own owner key** — the Init/Persistence worlds run a live deployment wave.

- **Phase 1:** `GetLocationTypeAtPosition` at a base includes `BASE`; the no-players condition accepts far / refuses near; `FindBestDeploymentConfig` at a position already holding config A returns config B.
- **Phase 2:** every new faction registry name resolves for **both** shipped factions; `m_bSnapToRoad 0` keeps the position inside `m_fSpawnRadius`; each provider returns an empty (non-null) list where nothing qualifies; **every new module's clone carries every authored attribute**.
- **Phase 3:** the three patrol configs resolve, validate, and build **cycling PERIMETER** plans.
- **Phase 4:** the three placement configs resolve and validate; tower/sniper configs build **null** plans and the defense-positions config builds a **one-point DEFEND, non-cycling** plan (the "garrisons never wander" claim); `PlacementForArrival` is stable across two simulated materialisations and wraps past the post count; the sniper provider filters a marker whose own `m_iMinimumThreat` exceeds current threat.
- **Phase 5:** a composition module on a restored deployment builds nothing; on a fresh one it builds once and not twice; a slot already in `m_aSlotsFilled` is never selected.
- **Phase 6:** the 80 % transfer conserves the total; the opening seed lands in the pool.

### Persistence tier — `TestSuites/Persistence/OVT_TEST_PersistenceRoundTripSuite.c` (All)

Two new cases, both on the shared gate:

- **A base-defense deployment round trip** (Phase 4, the requirement's named case): config name, faction, threat, invested resources and virtual key survive save → dirty → re-apply, and `FindConfigByName` still resolves.
- **The legacy-payload conversion** (Phase 6): a base record carrying legacy upgrade records raises the deployment pool by exactly the computed value and leaves `OVT_BaseData.upgrades` empty; a second pass over an emptied payload adds nothing.

🔴 **Structural limit, inherited and not to be worked around:** the suite's reload seam (`ReapplyLatestSaveData`) builds its request with `Instances = {gameMode}` only, so a deployment marker's `Deserialize` is **never** re-run by it. Both cases assert the **restore half** (`ApplyPersistedDeployment` / `ApplyPersistedOccupyingFaction`) with a real save taken alongside so the write half runs over live state, and say so at the top of the fixture. **Do not widen the seam** — it means naming persistence-framework types inside `Scripts/Game/Tests/`, which the suite's assertion rule forbids.

⚠ **Fixture discipline:** a deployment fixture is safe only if it is marked `SetSpawnedUnitsEliminated(true)` on the deployment **and every spawning module** — "short" is not safe. Re-run the `RegisterGroup(` sweep and record a verdict per site (Phase 4, T4.9).

### Campaign tier

`OVT_TEST_Campaign_BaseUpgrades_ConfigLoaded` is **deleted** in Phase 7 — the config it guards ceases to exist. `OVT_TEST_Campaign_GMGroupRegistry` keeps asserting, with its header corrected: its producers are deployments, and base upgrades stop being one.

### Not automatable, and why

| Area | Why manual |
|---|---|
| Whether a sniper is on the walkway or under it | A judgement about a place, in a world the test harness has no towers in |
| Placement after re-materialisation, live | Needs a real despawn/respawn cycle driven by real distance |
| Fortification pacing and order | Minutes of campaign time and a subjective "does this feel right" |
| Resource feel / whether the pool is ever the constraint | A tuning judgement |
| Legacy-save conversion end to end | Needs a real pre-migration save and a real load |
| Save → quit → **Continue** | The harness restarts the suite on a world transition |
| Two campaigns in one session | Same reason; core's Phase 6 found four teardown bugs exactly here |
| AI budget on a fully fortified map | Needs ~120 deployments and a player driving across them |
| MP / JIP | Uncovered by the whole spine; a dedicated-server pass is the only check |

---

## 8. Dependencies

**Hard preconditions (all satisfied today):**

- **`virtualization/core` complete and frozen** — `RegisterGroup`, `FindGroupsByOwner`, `GetOnRecordsRestored`, `GetOnGroupWiped`, `GetAliveMemberCount`, the survivor mask and Route B persistence. This feature asks core for **nothing new**.
- **`virtualization/movement` complete** — the plan-is-the-opt-in contract, which is what keeps every static base garrison standing still with no flag to set.
- **`virtualization/integration` complete** — the entire consumer seam: `EnsureGroups` convergence, the owner-key scheme, deployment serializer v2, the manager's single subscription, GM re-tagging on reclaim, and `Deployment_TowerGarrison.conf` as the worked precedent. Its `context.md` gotchas are cited throughout this plan and should be read before Phase 2.
- **Amendment A1's free-at-game-start seeding** — not used by this feature's configs, but its `CollectSeedCandidates` / per-faction-list extraction is live code the evaluator changes sit beside.
- **The deployments framework's own decision-making** — the 30 s evaluator, candidate scoring, `GetBasePositions`, the 250 m dedup, `m_iMaxInstances` / `m_fChance` / `m_iPriority` handling, the marker prefab and both serializers.
- **The base controller's slot registry** — `FindSlots`/`FindParking` and the eight slot arrays, which this feature keeps and now depends on for compositions, checkpoints and parked vehicles.
- **The faction group/vehicle/composition registries** — extended by T2.7, and the only composition-resolution path after [D9](#d9--the-legacy-prefab-slot-path-is-deleted-attributes-included).

**Explicitly NOT depended on:**

- **QRF migration** — an epic-level exclusion. Its guard is *un-guarded*, its controller otherwise untouched.
- **Resistance base defense** — out of scope by the requirements.
- **`virtualization/civilians`** — different files, different seam.
- **The occupying epic's resource-economy bug cluster (BUG-026/027/029)** — BUG-029's drift class is eliminated *by construction* here; the others are not this feature's to fix, and it should not duplicate their work.

**Downstream (what this unblocks):** the epic closes. Whatever wants base-defense variety next (new fortification types, resistance base defense, a specops raid config) authors a `.conf` and a provider, and touches no manager code.

**User-side (interactive):** the §6 play-test list, especially step 14's pacing/tuning pass and step 12's legacy-save load — **keep a pre-migration save before starting Phase 6** — and a **dedicated-server / MP pass**, which the automated spine covers not at all.

---

## 9. Risks & Mitigation

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| **R1** | **The evaluator change floods or starves the map.** Removing the 100 m veto lets different configs co-locate; a mis-scoped dedup would let the *same* config stack, and the map fills with garrisons. | Medium | Severe — unplayable AI load | The two edits land in **one commit** ([D5](#d5--the-evaluator-escalates-by-priority-and-that-is-how-a-base-fortifies)): the veto is only removed alongside the name-scoped dedup inside `FindBestDeploymentConfig`. Phase 1 is a **gate** with its own Init case ("a position already holding A returns B") and its own read-only survey. The per-faction ceiling and its WARNING are the runtime backstop. |
| **R2** | **A base grows a second composition on every load.** The classic static-content duplication. | High if not designed against | Save-breaking over a long campaign | [D7](#d7--a-restored-deployment-never-rebuilds-static-content): `WasRestoredFromSave()` gates every static module, `m_aSlotsFilled` round-trips independently, and Phase 5 asserts both halves (T5.6/T5.7). It is also a **player-visible DoD criterion** (F8) and a play-test step (step 7). |
| **R3** | **`CloneModule` silently drops a new attribute.** The standing trap of the module system — it lost `m_fMaxCruiseSpeed` once already, and integration explicitly booked it as "feature 5's problem". Three new module classes and four new attributes are exactly the exposure. | High | Silent wrong behaviour: guards at NORMAL importance, road-snapped garrisons, wrong composition tags | T2.1 audits the existing 12-attribute list; every new module hand-writes `CloneModule` copying **its own and all inherited** attributes; **T2.8 asserts the clone of every new module carries every authored attribute**, which is the only mechanical defence that exists. |
| **R4** | **The teleport does not re-run after re-materialisation**, and tower guards end up on the ground — or worse, inside the tower's collision. | Medium | The feature's most visible promise breaks | The subscription lives as long as the registration (never removed after placing, unlike legacy), the arrival counter resets on the group's own spawn notification, and both are re-established in `OnGroupReclaimed` because Route B gives a restored group a **new entity**. T4.6 makes the decision a pure method so it is Init-testable; F5 and play-test step 6 are the live check. |
| **R5** | **A legacy save converts to the wrong number** — double-credited, zero-credited, or credited on every load. | Medium | Economy breaks silently | Idempotence is **structural** (the write path stores an empty array), the maths is Logic-tier pinned, the Persistence case asserts the pool delta exactly, and F12 makes the pool jump a player-visible criterion. ⚠ The one thing to get right is that the credit happens **once after all bases are read**, not once per base into a manager that might also be restoring. |
| **R6** | **Fortification pacing is wrong** — `MAX_DEPLOYMENTS_PER_EVALUATION = 10` and a threat-sorted candidate list mean a fresh campaign takes ~6 minutes to fortify ~11 bases, and a resource-poor occupier may never finish. | Medium-High | Bases feel empty; "the migration broke base defense" | Quantified up front ([§3.4](#34-the-evaluator-change--how-a-base-fortifies-concern-by-concern)), the opening pool is seeded with the legacy `startingResources` sum so the first wave is genuinely affordable, and **play-test step 14 is a tuning pass** whose numbers feed back into the config costs. The escape hatch, if pacing is unacceptable: mark the two baseline configs (`BaseGarrisonPatrol`, `BaseDefensePositions`) `m_bFreeAtGameStart 1`, exactly as the user already did for towns and towers. |
| **R7** | **AI budget on a fully fortified map.** ~120 deployments' worth of registered groups is far more than the campaign has ever held. | Medium | Server frame time; garrisons that never materialise | Every group is proximity-driven by the engine and dormant by default, so the cost is bounded by what a player can be near — but importance tiers now matter a lot: tower guards and sniper teams are `HIGH`, everything else `NORMAL`, nothing unstamped (an unstamped registration inherits vanilla `LOW`, capped at half the budget and evicted first). Play-test step 14 measures it. |
| **R8** | **Deleting a legacy faction attribute breaks a config parse**, because its authored values survive in two `.conf` files and a 2 000-line prefab. | Medium | Faction data fails to load — catastrophic and immediate | T7.6 deletes the attribute **and every authored value in the same commit**, and T7.1's disposition table is built before anything is removed. `OVT_SpawnGroupJobStage.c:30` — a live non-base-upgrade reader — is re-pointed **first**. |
| **R9** | **Un-guarding `SpendResources` instead of deleting it**, reviving the legacy spender beside the pool and doubling every base's force. | Low | Force doubling everywhere | [G7](#primary) and T7.3 word it as "the guards leave inside the deletions — never un-comment either", the QRF asymmetry is spelled out separately in T7.4, and the closing ledger (T8.5) re-checks. This is R16 from integration, restated for a bigger blast radius. |
| **R10** | **The GM base panel goes blank or misleads** once `m_aBaseUpgrades` is gone. | High if not handled | An operator-facing regression nobody planned | T7.8 re-points `BuildBases()` at the base's deployments rather than deleting the block, and an **empty diff on the record classes and the RPC** is an acceptance criterion — the wire does not move. |
| **R11** | **The no-players condition is written symmetrically** (runtime `EvaluateCondition` also returning the distance test), so `m_bDeleteOnConditionFail` collects a base's defense the moment a player walks in. | Medium | Bases empty out exactly when the player attacks them | The asymmetry is stated in [§3.3](#33-the-new-modules), required by T1.5, and belongs in the module's own class header. Worth an Init case: `EvaluateCondition()` returns true with a player standing on the position. |
| **R12** | **Concurrent sessions move the tree.** Every `file:line` here was verified 2026-08-17 against an uncommitted `v1.5` working tree, and bugfix sessions commit into the same branch. | High | Stale references, failed edits | Three phases open with a **read-only survey task** (T1.1, T6.1, T7.1) whose job is to re-verify before editing. No task depends on a line number for correctness. |
| **R13** | **Specops' removal takes something with it** — `m_aKnownTargets`, `UpdateKnownTargets`, or a `StartBaseQRF` caller. | Low-Medium | A QRF path silently dies | T6.5 requires verifying `StartBaseQRF`'s other two callers and deciding `m_aKnownTargets`' fate explicitly. F13 is the live check: capturing a base still triggers a QRF. |
| **R14** | **The occupying faction can no longer retake a radio tower**, because that was specops' 600 s capture timer. | Certain (it is the drop) | A one-way progression the player can never lose ground on | Accepted and documented ([D3](#d3--specops-is-dropped-with-no-replacement)); it is a *player-favourable* change, and the tower loop otherwise stays intact (garrison → wipe → flip → deployment collected). Raise it with the user after the play-test if towers feel too safe. |
| **R15** | **A base's defense is never collected** when the base flips to the resistance, because a config without a reinforcement module has no `m_bDeleteOnConditionFail` path (the branch lives *inside* `CheckReinforcement()`). | Medium | Enemy fortifications linger at a base the player owns | Every base config **except parked vehicles** authors `m_bEnableReinforcement 1` + `m_bDeleteOnConditionFail 1` + a base-control condition; the parked-vehicles exception is deliberate and recorded in T5.3. Worth confirming in the play-test: take a base and check its enemy deployments disappear. |
| **R16** | **The composition module picks a slot far from where a player would expect**, or fails to find one and silently costs money for nothing. | Medium | Invisible waste; odd-looking bases | The slot picker is a direct port (30 tries, skip filled) and the module registers nothing when it finds no slot; log a WARNING naming the config and the base when that happens, so "the base never got its bunker" has an explanation in the log rather than a repro. |

---

## Agent Routing Summary

| Phase | Agent | Advanced? |
|---|---|---|
| 1 — Evaluator escalation (gate) | `component-developer-advanced` | **yes** — changes the shared evaluator four shipped configs depend on |
| 2 — New modules, providers, registry entries | `component-developer-advanced` | **yes** — subclasses and edits the shipped infantry module; first framework code to call `OVT_PersistenceTracking.Track` |
| 3 — Garrison patrol configs (3) + 2 class deletions | `component-developer` | no — config authoring plus two self-contained deletions |
| 4 — Exact placement (3 configs) + 3 class deletions + Persistence case | `component-developer-advanced` | **yes** — elevated placement, re-materialisation, and the shared All-group gate |
| 5 — Static content (3 configs) + 4 class deletions | `component-developer-advanced` | **yes** — persistence-tracked entities from a deployment module; the double-build failure mode |
| 6 — Funding single-path + specops drop + save conversion | `component-developer-advanced` | **yes — the highest-risk phase.** Rewrites the occupying economy and changes what a legacy save means |
| 7 — Retirement sweep | `component-developer-advanced` | **yes** — deletes a directory, a config, a test, a component's core purpose, eight authored faction attributes and the epic kill switch |
| 8 — Help & documentation sync | `help-docs-sync` | — |

**Skills to activate:** `enforcescript-patterns` (all phases), `overthrow-architecture` (1–7), `workbench-workflow` (2–7 — config authoring and the play-tests).

**Estimate:** 61–86 h across the eight phases, of which Phases 2, 4, 6 and 7 are roughly three quarters.
