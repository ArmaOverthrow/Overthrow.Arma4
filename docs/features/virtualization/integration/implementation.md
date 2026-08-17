# Virtualization Integration — Implementation Plan

**Status:** Ready for Review
**Started:** 2026-08-17
**Target Completion:** TBD
**Last Updated:** 2026-08-17 16:05 (started via /autorun-feature; plan previously revised for the scope amendment: tower garrisons become a **deployment config**, not a second direct consumer)

**Epic:** `virtualization` (feature #4 of 5 — see `docs/features/virtualization/epic-overview.md`)
**Requirements:** `docs/features/virtualization/integration/requirements.md` — authoritative, **amended 2026-08-17** by five user decisions, all recorded as decisions below: in-place module rewrite ([D1](#d1--in-place-module-rewrite-the-config-surface-of-the-three-shipped-configs-is-frozen)), **one consumer seam — tower garrisons become a deployment config** ([D2](#d2--one-consumer-seam-tower-garrisons-become-a-deployment-config)), AI observers built here ([D14](#d14--the-observer-api-is-entity-keyed-not-handle-keyed)), the five inherited defects fixed as prerequisites ([D3](#d3--the-inherited-defects-are-fixed-first-in-their-own-phase)), and QRF map-wide garrison despawn **dropped** ([D12](#d12--qrf-no-longer-despawns-tower-garrisons--now-moot-by-construction)).
**Consumes:** `docs/features/virtualization/core/api.md` — 🔒 **FROZEN**. §10's `integration` table is the entire surface this feature programs against; §7 is the worked consumer example; §8 is Route B persistence. **One** additive core ask (the observer API, [§3.10](#310-the-one-additive-core-ask--ai-observers)), landed with the same ritual the three existing post-freeze additions used.
**Consumes:** `docs/features/virtualization/movement/context.md` → "For `integration`" — the auto-adoption seam. **The plan is the opt-in**; a moved group resumes waypoints from index 0; vehicle-borne groups stay materialised and are skipped.

> **Why this feature is fourth.** Core built the registry, movement made dormant groups believable, civilians proved the ambient seam. Nothing in the live campaign uses any of it: the epic kill switch (`OVT_VirtPlaytestKillSwitch.DISABLE_LEGACY_AI_SPAWNS = true`) currently silences the exact systems this feature migrates, so the campaign has **no** systemic AI at all. This is the feature that turns the layer on — and, after the 2026-08-17 amendment, it does so through **exactly one consumer seam**: the deployments framework. Radio-tower garrisons stop being bespoke manager code and become a shipped deployment config, which is precisely the shape `base-defense-migration` will use for its nine base-upgrade classes.

---

## 1. Executive Summary

Two live systems currently virtualize AI by hand, badly:

- **Deployments** toggle their whole force on a per-marker 10 s timer against `OVT_Global.PlayerInRange(pos, m_iMilitarySpawnDistance)` — `ActivateDeployment()` spawns every group from scratch, `DeactivateDeployment()` deletes every soldier. A patrol you shot down to one man comes back at full strength the moment you walk away and return. The proximity toggle even carries the comment *"will be replaced by virtualization system"* (`OVT_DeploymentComponent.c:264`).
- **Radio-tower garrisons** do the same thing inside one 9 s manager loop (`OVT_OccupyingFactionManager.CheckRadioTowers()`, `:542-629`) that fuses four unrelated concerns — sabotage countdown, garrison spawn, garrison despawn, and tower capture — into a single `foreach`, and additionally deletes **every tower garrison on the map** for the whole duration of any QRF anywhere.

This feature deletes both mechanisms. It does **not** replace them with two virtualization consumers: after the 2026-08-17 amendment, tower garrisons are re-expressed as a new shipped deployment config, `Deployment_TowerGarrison.conf`, so **deployments are the single tracked-group consumer this feature builds** ([D2](#d2--one-consumer-seam-tower-garrisons-become-a-deployment-config)). The deployment evaluator already enumerates radio towers as candidate positions (`GetRadioTowerPositions`, `OVT_DeploymentManager.c:364-381` — verified real and wired), so the garrison becomes a scored, costed, condition-gated deployment like every other.

Nothing about deployment *decision-making* moves: the 30 s evaluator, the candidate scoring, the resource pools, the marker entity and its serializer stay exactly where they are. **Only group lifecycle migrates** — one system per concern, no double bookkeeping.

Six things shape the work and are built into the phases rather than discovered later:

1. **The rewrite is in place, and the three shipped configs are frozen.** `Deployment_TownPatrol.conf`, `Deployment_VehiclePatrol_Light.conf` and `Deployment_VehiclePatrol_Heavy.conf` are not touched. `OVT_InfantrySpawningDeploymentModule` and `OVT_VehicleSpawningDeploymentModule` keep their class names, their attributes and their place in the module composition; only their bodies change ([D1](#d1--in-place-module-rewrite-the-config-surface-of-the-three-shipped-configs-is-frozen)). New authoring — one new `.conf`, one registry entry, two new module classes — is additive and is what carries the tower garrison.
2. **Waypoints become core's, so behaviour modules must answer *before* the group exists.** Today the spawning module spawns groups and the behaviour module bolts waypoints on afterwards. Core builds waypoint entities at registration from an `OVT_VirtualWaypointPlan` and forbids consumers from touching them (`api.md` §10, core D6). The module order therefore inverts: behaviour modules gain a `BuildVirtualPlan(vector)` that the spawning module calls **before** `RegisterGroup` ([D5](#d5--behaviour-modules-supply-plans-the-order-inverts)).
3. **Reclaim is a manager job, not a module job.** Deployment modules are cloned config objects with no stable identity and a lifetime the deployment controls; subscribing a `ScriptInvoker` to one is a dangling pointer waiting to happen. `OVT_DeploymentManagerComponent` subscribes `GetOnRecordsRestored()` and `GetOnGroupWiped()` **once** and fans out ([D7](#d7--the-manager-subscribes-the-modules-never-do)).
4. **Vehicle patrols were never proximity-toggled.** Both shipped vehicle configs already ship `m_bEnableProximityActivation 0`, so their crews spawn once and are never deactivated. Registering them with a huge `spawnDistanceOverride` (always materialised, movement skips them, live AI drives real roads) is therefore **parity**, not a regression — and it is bounded to `m_iMaxInstances 1` each, i.e. at most one 2-man and one 4-man crew campaign-wide ([D10](#d10--vehicle-crews-register-always-materialised)).
5. **Tower capture inverts.** Today the tower flips to the resistance as a side effect of despawn bookkeeping (`:590-615` — the garrison EntityID list happened to be empty this tick). New shape: the Tower Garrison deployment's **eliminated flag**, which is now driven by core's wipe bookkeeping, triggers `ChangeRadioTowerControl` through a small behaviour module. It must fire on **real wipes only** — core's mask semantics give that for free, and it is an acceptance criterion and a test, not a hope ([D18](#d18--tower-capture-is-driven-by-the-eliminated-flag-not-by-an-empty-list)).
6. **AI observers are built here, behind a gate.** An additive core API parks an engine `ObserversSystem` entity-following observer so a parked recruit squad pulls enemy AI awake with no player nearby. `InsertObserverSP` with a **null** entity hard-freezes the client (core `context.md` gotcha 0) and has zero vanilla callers, so the semantics are verified by an autotest case in Phase 1 **before** anything is built on them ([D15](#d15--the-observer-spike-is-a-gate-not-a-formality)).

The migration is expected to be **net-deleting**: `OVT_EntitySpawningAPI.c` becomes wholly unreferenced (all five of its live call sites are the deployment modules this feature rewrites), the manager's `DestroyDeployment` wrapper is already dead, `IsPlayerInRange` / `DeactivateDeployment` go, and `CheckRadioTowers` shrinks from ~88 lines to ~15.

---

## 2. Goals

### Primary

- **G1** All shipped deployment configs — the three existing ones **and** the new Tower Garrison — run on virtualization with **per-member dead-stay-dead** across despawn/respawn and save/load. A town patrol that lost 3 of 4 groups comes back with 1; a group that lost 2 of 4 men comes back with those 2 men in their own roles.
- **G2** Radio-tower garrisons are a **deployment config**. Tower **capture** is driven by the deployment's eliminated flag (core wipe bookkeeping), never by proximity despawn, and never by "the array happened to be empty this tick".
- **G3** The ad-hoc virtualization is **gone**, not bypassed: the proximity toggle, `IsPlayerInRange`, both module `OnDeactivate` delete-everything paths and the tower spawn/despawn/reap/capture blocks are deleted, grep-proven.
- **G4** **Existing saves survive.** A pre-feature save loads with its town patrols re-established from config and its tower garrisons **created by the evaluator like any other candidate** — never vanished, never duplicated, and **never resurrected for a deployment whose force was already wiped**.
- **G5** The **stolen-vehicle guarantee** holds, riding the engine's held-member protection rather than the old 40 m rule.
- **G6** The five inherited defects are fixed, and the reclaim/plan/key maths is world-free and Logic-tier pinned.
- **G7** **The observer API works and is used**: a parked recruit squad alone in a town makes the town's registered enemy groups materialise.
- **G8** **Core is only extended.** Exactly one additive change (the observer API), with the full ritual: `api.md` §3 + §10, a dated `core/context.md` entry naming `virtualization/integration`, nothing renamed or re-meant.

### Secondary

- **G9** External consumers are untouched and keep working: `OVT_PatrolHarassmentStabilityModifier` still applies town harassment; `OVT_GMSnapshotBuilder` still emits deployment records (with a **non-zero** `m_iResourcesInvested` for the first time); GM group icons still appear for migrated groups, including tower garrisons.
- **G10** Every `OVT-VIRT-PLAYTEST-ONLY` guard whose system this feature migrates is removed **in the phase that migrates it** — the tower one by **deleting the code it wraps**. Guards for un-migrated systems (base upgrades, QRF) stay until epic end.
- **G11** The migration is net-deleting across `Scripts/Game/GameMode/Deployments/` and the tower region of `OVT_OccupyingFactionManager.c`, new module classes and the new config notwithstanding.
- **G12** The Tower Garrison config is a **worked precedent**: `base-defense-migration` can read it as "how a bespoke garrison system becomes a deployment config" without re-deriving anything.

### 2.1 Quality Bar — the hard floor

| Bar | What it means concretely | How it is caught |
|---|---|---|
| **A wiped force never returns** | A deployment whose groups were all wiped does not re-register — not on the next evaluation, not on reload, not on an in-session re-apply. | §6 F4/F9; Persistence case T7.3 |
| **Proximity despawn is never mistaken for death** | A garrison that despawns because you drove away does **not** flip its tower to the resistance and does **not** set a deployment's eliminated flag. | §6 F6; Init case T4.8; acceptance criterion on Phase 4 |
| **No duplicate registrations, ever** | Every registration path is idempotent and checks `FindGroupsByOwner` first; every deployment creation passes the 250 m same-name dedup. A continued campaign, an in-session re-apply and a restart all converge on the same handle count. | §6 F8/F10; Init case T2.6 |
| **Core stays frozen** | One additive change (the observer API) and nothing else. `git diff Scripts/Game/GameMode/Virtualization/` shows only that. | Q/I criteria §6 |

---

## 3. Architecture Overview

### 3.1 Division of labour after the migration

```
SERVER ONLY. Nothing new replicates. Nothing new is persisted except ONE appended string field
(the deployment's virtual key) behind a serializer version bump.

CORE (virtualization/core — FROZEN, api.md §10 "integration")
├─ RegisterGroup(ownerSystem, ownerKey, factionKey, groupName, pos, plan, spawnDist, importance)
├─ UnregisterGroup / IsRegistered / GetGroup / GetAliveMemberCount / GetMemberCount
├─ FindGroupsByOwner(ownerSystem, ownerKey)          ← RECLAIM. Consumers never persist handles.
├─ GetOnRecordsRestored()  (fires once per load, after PostGameStart)
├─ GetOnGroupWiped()       (int handle; fires BEFORE the record is removed)
└─ [ADDED HERE] AddEntityObserver / RemoveEntityObserver / HasEntityObserver / GetEntityObserverCount

MOVEMENT (virtualization/movement — consumed, not touched)
└─ auto-adoption: any registered dormant group with a MOVE/PATROL plan is walked. THE PLAN IS THE OPT-IN.

DEPLOYMENTS — THE ONE CONSUMER SEAM   Scripts/Game/GameMode/Deployments/
├─ OVT_DeploymentManagerComponent      UNCHANGED decision-making (30 s evaluator, scoring, pools)
│    + subscribes GetOnRecordsRestored / GetOnGroupWiped ONCE and fans out (D7)
│    + CreateDeployment stamps resourcesInvested + threatLevel            (defect c)
│    + GetLocationTypeAtPosition ORs in RADIO_TOWER (surgical, D19)
│    - DestroyDeployment() wrapper deleted (already zero callers)
├─ OVT_DeploymentComponent             marker record — UNCHANGED role
│    + m_sVirtualKey (derived once, persisted, serializer v2)              (D6)
│    + EnsureGroups() fan-out over its spawning modules
│    - UpdateDeployment's proximity toggle, IsPlayerInRange, DeactivateDeployment   DELETED
├─ Modules/ (rewritten in place — class names, attributes and config surface preserved)
│    ├─ OVT_InfantrySpawningDeploymentModule    registers N groups; reinforcement re-registers
│    │                                          + m_eImportance attribute (default NORMAL)
│    ├─ OVT_VehicleSpawningDeploymentModule     1 crew group per vehicle, always-materialised
│    ├─ OVT_PatrolBehaviorDeploymentModule      + BuildVirtualPlan(); waypoint authoring DELETED
│    └─ OVT_MultiTownPatrolBehaviorDeploymentModule + BuildVirtualPlan(); authoring DELETED
├─ Modules/ (NEW — additive, carry the tower garrison)
│    ├─ OVT_RadioTowerControlConditionDeploymentModule   modelled on OVT_BaseControlCondition…
│    └─ OVT_RadioTowerCaptureBehaviorDeploymentModule    eliminated flag → ChangeRadioTowerControl
└─ OVT_EntitySpawningAPI.c              DELETED — zero remaining callers tree-wide (D13)

CONFIGS (additive only)
├─ Configs/Deployment/Deployment_TowerGarrison.conf     NEW
└─ Configs/Deployment/overthrowDeployments.conf         ONE registry entry appended

OCCUPYING FACTION MANAGER  Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c
├─ CheckRadioTowers()   SHRINKS to the sabotage countdown alone (:547-557)
├─ OVT_RadioTowerData.garrison            DELETED outright (no reader survives)
├─ ChangeRadioTowerControl / GetNearestRadioTower   UNCHANGED, now called from a deployment module
└─ tower discovery, OVT_PersistedRadioTower, RplSave/RplLoad   UNTOUCHED

WORLD-FREE STATICS (the Logic tier's whole subject)
├─ OVT_DeploymentVirtualKey     key derivation, module tags, sanitisation, disambiguation
└─ OVT_VirtualPlanFactory       perimeter geometry, defend plans, route→plan interleaving

NOT USED, DELIBERATELY
├─ a "tower_garrison" owner system                 dropped by the amendment — towers are deployments
├─ OVT_Global.PlayerInRange for AI lifecycle       proximity is the engine's observers now
├─ SCR_AIGroup lifecycle / waypoint calls          core owns both (api.md, core D6)
└─ any new prefab, RPC, RplProp or UI              none is needed
```

### 3.2 Owner system and owner keys

`ownerSystem`/`ownerKey` are free-form strings and are the *only* thing a consumer has to be able to re-derive after a load (`api.md` §3). After the amendment there is exactly **one** owner system.

| Consumer | `ownerSystem` | `ownerKey` | Handles per key |
|---|---|---|---|
| **Every** deployment spawning module — town patrol, vehicle patrol crew **and tower garrison** | `"deployment"` | `<deploymentKey>#<moduleTag>` | N (one per group) |

**`<deploymentKey>`** is `"<configName>@<round(x)>_<round(z)>"` of the marker entity's origin, derived **once** on first need and then **persisted** on the component (`m_sVirtualKey`, appended to `OVT_DeploymentComponentSerializer` behind version 2). Derive-and-store rather than derive-every-time, because derivation reads a position and the code that used to move that position is one of the defects being fixed here ([D6](#d6--the-deployment-key-is-derived-once-and-persisted)). Collisions (same config, same rounded spot) are disambiguated by the manager appending `#2`, `#3`… at derivation time.

For a tower garrison this reads `"Tower Garrison@4821_7093"` — naturally per-tower, stable across saves, and requiring **no** tower-specific key scheme at all. That is the amendment's whole point.

**`<moduleTag>`** is the module's authored `m_sModuleName` when non-empty, otherwise `"m" + <index within GetSpawningModules()>`. Both shipped vehicle modules leave `m_sModuleName` empty (`Deployment_VehiclePatrol_Light.conf:4-10`, `_Heavy.conf:4-9`) and the town-patrol infantry module authors `"Spawn Infantry"` — the fallback is not theoretical. The new Tower Garrison config authors a name.

### 3.3 The Tower Garrison config

`Configs/Deployment/Deployment_TowerGarrison.conf`, plus one appended entry in `overthrowDeployments.conf`. Four modules, three of which already exist:

| Module | Role | Key authored values |
|---|---|---|
| `OVT_InfantrySpawningDeploymentModule` | the garrison itself | `m_sModuleName "Tower Garrison"`, `m_sGroupType "light_patrol"` (the same prefab `m_aTowerDefensePatrolPrefab` names today — **verify per faction**, T4.2), `m_iMinGroupCount`/`m_iMaxGroupCount` tuned to today's `RandInt(patrolGroupsMin, patrolGroupsMax)` spread, `m_bScaleByTownSize 0`, `m_bSpawnAtNearestBase 0`, `m_fSpawnRadius` small (parity: today all groups spawn on one point), `m_eImportance HIGH` (new attribute, [§3.9](#39-registration-parameters)) |
| `OVT_PatrolBehaviorDeploymentModule` | **defend at the tower** | `m_ePatrolType 0` (`DEFEND`), `m_bUseNearestTownCenter 0` → the plan is a single `DEFEND` point at the deployment position, so **movement never walks a garrison** ([D4](#d4--the-plan-is-the-opt-in-inherited-verbatim-from-movement)) |
| `OVT_ReinforcementBehaviorDeploymentModule` | rebuy + condition-driven teardown | `m_bEnableReinforcement 1`, `m_bDeleteOnConditionFail 1`, `m_fInitialDelay` and `m_fCheckInterval` lowered from the class defaults (5 min / 60 s) so a captured tower's deployment is collected promptly |
| `OVT_RadioTowerControlConditionDeploymentModule` **(new)** | "does the controlling faction still hold this tower?" | `m_fMaxDistance 300`, `m_bRequireControl 1` — a byte-for-byte structural copy of `OVT_BaseControlConditionDeploymentModule` (`:1-105`) against `GetNearestRadioTower` instead of `GetNearestBase` |

Plus one more new behaviour module carried by the same config:

| Module | Role |
|---|---|
| `OVT_RadioTowerCaptureBehaviorDeploymentModule` **(new)** | On each `OnUpdate`, if `m_ParentDeployment.GetSpawnedUnitsEliminated()` and it has not already fired, resolve the tower within `m_fMaxDistance` and call `ChangeRadioTowerControl(tower, playerFactionIndex)` **once** (edge-latched). ≤ one 10 s `UpdateDeployment` tick of latency — better than today's 9 s poll ([D18](#d18--tower-capture-is-driven-by-the-eliminated-flag-not-by-an-empty-list)) |

Config-level: `m_iAllowedFactionTypes` = `OCCUPYING_FACTION`, `m_iAllowedLocationTypes` = `RADIO_TOWER`, `m_iPriority` **1** (ties with Town Patrol for the highest priority in the registry — towers should not lose the resource race), `m_fChance 100`, `m_iMaxInstances **-1**` (there are many towers; one-per-tower comes from the evaluator's 250 m same-name dedup, `HasExistingDeploymentOfType(..., radius = 250)` at `OVT_DeploymentManager.c:488`), and a **real, non-zero cost** ([D17](#d17--tower-garrisons-cost-resources-like-any-other-deployment)).

The loop closes without any bespoke code:

```
tower is occupying-held  →  GetRadioTowerPositions() offers it as a candidate (:364-381)
                         →  GetLocationTypeAtPosition ORs in RADIO_TOWER within 300 m (D19)
                         →  RadioTowerControlCondition.EvaluateStaticCondition passes
                         →  cost affordable, 250 m dedup clear, m_iPriority 1 wins
                         →  CreateDeployment  →  marker  →  EnsureGroups  →  RegisterGroup × N
garrison wiped           →  core GetOnGroupWiped × N  →  eliminated flag
                         →  RadioTowerCaptureBehavior fires ONCE  →  ChangeRadioTowerControl(resistance)
tower now resistance-held→  RadioTowerControlCondition.EvaluateCondition fails
                         →  ReinforcementBehavior (m_bDeleteOnConditionFail) → DeleteDeployment
occupying faction retakes→  next 30 s evaluation re-creates the deployment
```

### 3.4 What each retired path is replaced by

| Retired | File:line (2026-08-17 — re-grep) | Replaced by |
|---|---|---|
| Deployment proximity toggle | `OVT_DeploymentComponent.c:255-275` | Engine `ProximityDriven` lifecycle on each registered group. `UpdateDeployment` keeps the module `Update()` calls and activates **once**. |
| `IsPlayerInRange()` | `OVT_DeploymentComponent.c:278-281` | `ObserversSystem` (core's, inside the engine). Deleted. |
| `DeactivateDeployment()` | `OVT_DeploymentComponent.c:188-217` | Nothing — no caller survives. Deleted. |
| Infantry `OnDeactivate` delete-all | `OVT_InfantrySpawningDeploymentModule.c:76-89` | Nothing on deactivate; `OnCleanup` unregisters ([D8](#d8--teardown-funnels-through-oncleanup-not-ondeactivate--and-there-is-no-ondelete)). |
| Vehicle `OnDeactivate` delete-all + 40 m rule | `OVT_VehicleSpawningDeploymentModule.c:74-108` | `OnCleanup`: `UnregisterGroup` (respects held members) + a player-occupancy check before deleting the vehicle ([D11](#d11--the-stolen-vehicle-guarantee-is-verified-not-rebuilt)). |
| `OVT_EntitySpawningAPI.SpawnInfantryGroup` / `CleanupGroup` | `OVT_EntitySpawningAPI.c:47-91`, `:380-404` | `RegisterGroup` / `UnregisterGroup`. The whole file goes ([D13](#d13--ovt_entityspawningapic-is-deleted-outright)). |
| Patrol waypoint authoring | `OVT_PatrolBehaviorDeploymentModule.c:145-176` | `BuildVirtualPlan()` → core builds the waypoint entities at registration. |
| Multi-town waypoint authoring + double-insert | `OVT_MultiTownPatrolBehaviorDeploymentModule.c:211-285, :326-370` | `BuildVirtualPlan()`; `m_aWaypoints` and its double-delete disappear with it. |
| Tower garrison spawn | `OVT_OccupyingFactionManager.c:560-589` | **The Tower Garrison deployment config.** Deleted (the `OVT-VIRT-PLAYTEST-ONLY` guard at `:564` goes with the block, [D20](#d20--the-tower-kill-switch-guard-is-removed-by-deleting-the-code-it-wraps)). |
| Tower garrison reap + capture | `:590-615` | `OVT_RadioTowerCaptureBehaviorDeploymentModule`, driven by the eliminated flag. |
| Tower garrison despawn (+ the `!m_CurrentQRF` term) | `:559`, `:616-627` | Engine proximity lifecycle. The QRF term is **dropped and its code deleted** ([D12](#d12--qrf-no-longer-despawns-tower-garrisons--now-moot-by-construction)). |
| `OVT_RadioTowerData.garrison` | `:72-73` | Deleted. It is `[NonSerialized]`, absent from `OVT_PersistedRadioTower` and absent from the JIP stream, and **no reader exists outside `CheckRadioTowers`** (verified 2026-08-17). |

### 3.5 The registration/reclaim flow — one idempotent method, three callers

```
EnsureGroups()                                   // on each spawning module; ALWAYS safe to call
 ├ virt = OVT_Global.GetVirtualization(); null → return
 ├ handles = virt.FindGroupsByOwner("deployment", myOwnerKey)
 ├ prune handles that are no longer IsRegistered (paranoia; the finder already filters)
 ├ adopt: m_aHandles = handles;  re-tag each for the GM registry (§3.7)
 ├ if (AreSpawnedUnitsEliminated() || parent.GetSpawnedUnitsEliminated())  → return   // F4: wiped stays wiped
 ├ wanted = CalculateGroupCount(...)   (rolled once, then remembered in m_iActualGroupCount)
 └ for (i = m_aHandles.Count(); i < wanted; i++)   → register one more group, tag it, keep the handle
```

Called from exactly three places, all of which may run in any order, any number of times:

1. **`OnActivate()`** — the deployment's first `UpdateDeployment` tick (8–12 s jittered).
2. **`OVT_DeploymentManagerComponent`'s `GetOnRecordsRestored()` handler**, which walks `GetAllDeployments()` and fans out. This is the reclaim point `api.md` §3 mandates, and it also fires on an in-session re-apply.
3. **`Reinforce(n)`** — the rebuy path, which raises `wanted` and calls the same loop.

⚠️ **Ordering, stated once so nobody re-derives it.** On a continued campaign core's `ApplyPersistedRegistry()` runs **synchronously inside the deserialize pass**, and a deployment's own `InitializeDeployment` runs in that same pass, so the registry is already populated 8–12 s later when `OnActivate` first fires. If the two ever did race, `ApplyPersistedRegistry` unregisters live records the payload does not claim and `GetOnRecordsRestored` then re-adopts — the system converges either way, at the cost of one wasted registration. This is why `EnsureGroups` is written as *converge to `wanted`*, never as *spawn `wanted`*.

### 3.6 Wipe accounting — the eliminated flag becomes wipe-driven

`OVT_InfantrySpawningDeploymentModule.CheckIfUnitsEliminated()` currently polls `group.GetAgentsCount() > 0` — a test this project has already recorded as broken under 1.8 (a dormant or spawn-queued group reports zero agents; `OVT_GMGroupRegistry.c:78-82`). It is replaced by handle bookkeeping:

```
OnGroupWiped(handle)              // fanned out from the manager
 ├ not one of mine → return
 ├ m_aHandles.RemoveItem(handle)
 └ m_aHandles.IsEmpty() && m_iSpawnedEver > 0
      → m_bSpawnedUnitsEliminated = true
      → m_ParentDeployment.CheckAllSpawningModulesEliminated()
```

`CheckAllSpawningModulesEliminated()`, `m_bSpawnedUnitsEliminated`, `ApplyPersistedDeployment`'s flag-before-`InitializeDeployment` ordering and the serializer field all stay exactly as they are — which is what keeps `OVT_PatrolHarassmentStabilityModifier.c:34` working unchanged **and** what the tower-capture module reads.

**`CheckIfUnitsEliminated()` / `IsGroupAlive()` / `GetAliveGroupCount()` are deleted**; strength questions go to `virt.GetAliveMemberCount(handle)`, which is mask-first.

Because the flag is now set **only** from `GetOnGroupWiped`, and core fires that only when the survivor mask reports every slot dead, a proximity despawn can never set it. That single property is what makes [D18](#d18--tower-capture-is-driven-by-the-eliminated-flag-not-by-an-empty-list) safe, and it is asserted rather than assumed.

### 3.7 GM tagging — tag the durable group entity, and re-tag on reclaim

`OVT_GMGroupRegistry` keys on the **group entity's `EntityID`**, tags once at spawn, and never untags (`Sweep()` drops entries whose entity no longer resolves). Two consequences that make this easy:

- A registered group's entity is **durable** — it exists dormant, so one `Tag()` right after `RegisterGroup` covers the group's whole life. No `GetOnAgentAdded` subscription is needed.
- Under Route B the entity is **re-created on load with a new `EntityID`**, so reclaim must re-tag. Both happen inside `EnsureGroups()`, which is the one funnel.

`OVT_GMSnapshotBuilder.BuildGroups()` applies no agent-count filter and only requires an `RplComponent` on the group entity (which group prefabs carry), so dormant registered groups keep producing GM icons.

**Origins after the amendment.** All deployment-spawned groups tag `OVT_EGroupOrigin.DEPLOYMENT` with index `-1` and reason = the config name — so a tower garrison's icon reads `"Tower Garrison"` instead of `"RadioTower"`. `OVT_EGroupOrigin.RADIO_TOWER_GARRISON` (`OVT_GMGroupRegistry.c:19`) loses its only producer. **Leave the enum member in place** — the enum is not persisted but its integer *is* sent in the GM snapshot, its header warns that reordering would mislabel every group on a mismatched client, and `OVT_GMIconFormat.c:143` still maps it. Record the orphaning in `context.md`; do not touch the enum.

### 3.8 Plan translation — what each behaviour module hands the spawning module

| Source | Plan produced | Movement behaviour |
|---|---|---|
| `OVT_PatrolBehaviorDeploymentModule`, `m_ePatrolType = DEFEND` (**the shipped Tower Garrison**) | 1 point at the deployment position, `DEFEND`, cycle false | Never advanced — garrisons hold their post |
| `OVT_PatrolBehaviorDeploymentModule`, `m_ePatrolType = PERIMETER` (**the shipped Town Patrol**) | 8 points, cycle **true**: 4 road-snapped points at `radius` (200 m) around the centre, 90° apart, starting from the bearing of the group's own position, each immediately followed by a `WAIT` point at the same position with param `RandFloatXY(45, 75)` | Patrols virtually while dormant — this is the player-visible change |
| `OVT_MultiTownPatrolBehaviorDeploymentModule` (**both shipped vehicle configs**) | Per route town: a `MOVE` point + a `WAIT` point (`m_iWaitTimeAtTown` = 60 s); then, because `m_bReturnToStart` is true in both configs, a final `MOVE` point at the deployment origin. Cycle = `!m_bReturnToStart` → **false as shipped** | Skipped — the crew is always materialised ([D10](#d10--vehicle-crews-register-always-materialised)) |

This is a faithful translation of `OVT_OverthrowConfigComponent.GivePatrolWaypoints` (`:577-619`) — same 4-point/90° geometry, same road snap, same wait band, same single defend waypoint — expressed as a plan instead of as waypoint entities. Three consequences worth stating:

- **`WAIT` durations start working.** The live helper `SpawnWaitWaypoint(pos, time)` drops its duration (a known upstream wrinkle, `api.md` §2), but movement honours the plan's `m_aParams` directly, so a dormant patrol really does pause at each corner.
- **Neither shipped vehicle config produces an `AIWaypointCycle`.** `m_bReturnToStart` defaults true and neither config overrides it, so the cycle branch (`OVT_MultiTownPatrolBehaviorDeploymentModule.c:267-282`) is dead as shipped. That materially lowers the [BUG-175](#9-risks--mitigation) exposure for vehicle patrols; the Town Patrol's cycling plan carries exactly the exposure every Overthrow patrol already has today.
- **Tower garrisons are stationary by construction**, because the plan is the opt-in and their plan has nothing to advance.

### 3.9 Registration parameters

| Config | `groupName` | `spawnDistanceOverride` | `importance` |
|---|---|---|---|
| Town Patrol infantry | `m_sGroupType` = `"light_patrol"` | `-1` (global `virtualizationSpawnDistance`, 1750 — matches today's `m_iMilitarySpawnDistance`) | `NORMAL` |
| **Tower Garrison infantry** | `m_sGroupType` = `"light_patrol"` | `-1` | **`HIGH`** |
| Vehicle patrol crew | `m_sCrewGroupType` (`"light_patrol"` / `"light_fireteam"`) | `m_iSpawnDistanceOverride`, **default 100000** (always materialised) | `NORMAL` |

Never unstamped: an unstamped group inherits vanilla `LOW` (0.50 budget cap, evicted first), which is exactly how a garrison fails to appear on a busy server (`api.md` §5, §10). Because importance now varies per config, `OVT_InfantrySpawningDeploymentModule` gains an `m_eImportance` attribute defaulting to `NORMAL` — **additive**, so the three shipped configs that do not author it keep today's behaviour and their surface is unchanged. Copy it in `CloneModule` (the module class's standing trap, [D1](#d1--in-place-module-rewrite-the-config-surface-of-the-three-shipped-configs-is-frozen)).

⚠️ `m_aTowerDefensePatrolPrefab` is a **`ResourceName`**, not a registry name (`OVT_Faction.c:361-362`), pointing at `Group_USSR_SentryTeam.et` / `Group_US_SentryTeam.et`. `light_patrol` is believed to resolve to the same prefabs; T4.2 verifies it **for both shipped factions** before the config is authored. If they ever differ, add a `tower_garrison` registry entry to the faction configs rather than passing a raw prefab — core resolves `(factionKey, groupName)` and never a faction index (`api.md` §3).

### 3.10 The one additive core ask — AI observers

```c
//! Park an engine observer that FOLLOWS an entity, so dormant registered groups near it materialise
//! even with no player present. Server-only, idempotent per entity, keyed internally.
//! REFUSES a null entity: a null-entity InsertObserverSP has zero vanilla callers and hard-freezes
//! the client (context.md gotcha 0).
bool AddEntityObserver(IEntity entity);
bool RemoveEntityObserver(IEntity entity);
bool HasEntityObserver(IEntity entity);
int  GetEntityObserverCount();
```

Implementation shape (core's file, additive only): a `map<EntityID, int>` of entity → SP observer key, keys drawn from a namespaced monotonic counter; `InsertObserverSP(key, 0, 0, entity)` (zero offsets = follow the entity's own position); `RemoveObserverSP(key)` on removal, on a stale-entity sweep folded into core's existing 2 s ambient tick, and on `OnDelete`. **Zero vanilla script callers of `InsertObserverSP`/`RemoveObserverSP` exist in the 1.8 tree**, so the SP key space is entirely Overthrow's — but namespacing it costs nothing and is what the plan asks for.

This is the **only** core change in the feature. Landing ritual, copied from the three existing post-freeze additions: signatures into `api.md` §3 and §10's `integration` table; the header's "Additively extended" note extended; a dated entry in `core/context.md` under **"Additive changes after the freeze"** naming `virtualization/integration` as the requester, with the Phase 1 spike verdict quoted in it.

---

## 4. Implementation Phases

Eight phases. Each leaves the tree compiling and green; each removes the kill-switch guard for the system it migrates and no other.

**Test-run policy:** `tools/compile-check.sh` runs freely. `tools/run-tests.sh` launches a real Reforger client and is run **by the orchestrator only, once, after a phase completes** — never during planning, never inside a subagent. `.claude/test-policy.md` is the rule. Fast = `{6A6E29FF47ECB840}`, All = `{6A6E2A002F53A581}`.

---

### Phase 1 — Inherited defects + the observer spike **(GATE)**

**Agent:** `component-developer` — standard. Five small, local, well-understood fixes plus one autotest case. Nothing here registers a group.
**Estimate:** 4–6 h
**Suite after this phase:** **All** `{6A6E2A002F53A581}` (it touches the deployment manager's create path, which the persistence tier exercises)

**Tasks**

1. **T1.1 — Defect (a): patrol check interval, seconds vs milliseconds.** `OVT_PatrolBehaviorDeploymentModule.c:15-16` authors `m_fCheckInterval` at `60` and documents it as seconds; `:62` compares it against a `GetWorldTime()` delta, which is **milliseconds**. Keep the attribute in seconds (the shipped config surface is frozen) and fix the comparison to `>= m_fCheckInterval * 1000`. Comment the unit at both ends.
2. **T1.2 — Defect (b): town cache never caches.** `OVT_TownConditionalDeploymentModule.c:40` sets `m_fCacheTimeout = 30.0` ("30 seconds") and `:127` compares it against a millisecond delta, so the cache expires after 30 ms and `GetNearestTown()` re-scans on every evaluation. Change the constructor literal to `30000.0` and fix the comment. **Sweep the sibling modules for the same shape** — `OVT_ReinforcementBehaviorDeploymentModule.c:57` and `:273` use millisecond-authored attributes and are already correct; record that verdict in `context.md` rather than "fixing" them.
3. **T1.3 — Defect (c): invested resources and threat are never stamped.** `CreateDeployment` (`OVT_DeploymentManager.c:723-780`) never writes `m_iResourcesInvested` or `m_fThreatLevel`, so `RecoverResources()` refunds 0 (`OVT_MultiTownPatrolBehaviorDeploymentModule.c:466`) and the GM snapshot always shows 0 (`OVT_GMSnapshotBuilder.c:227`). Add two optional parameters (`int resourcesInvested = 0, float threatLevel = 0`) and stamp them immediately after `InitializeDeployment(...)` at `:773`; pass the values already in scope at the call site (`EvaluateFactionDeployments`, `:227-233` — `deploymentCost` and `candidate.threatLevel`). Update `ForceCreateDeployment` (`:1040-1043`) to forward them. Do **not** change `ApplyPersistedDeployment`'s ordering.
4. **T1.4 — Defect (d): the marker entity is teleported to the last vehicle.** Delete `m_ParentDeployment.GetOwner().SetOrigin(spawnPos);` (`OVT_VehicleSpawningDeploymentModule.c:195-196`). The marker is the persisted record, the basis of the deployment key ([D6](#d6--the-deployment-key-is-derived-once-and-persisted)) and what `GetDeploymentNearPosition` measures for the stability modifier — it must not move. The comment claiming it is needed "so we know when to clean it up" refers to the 40 m rule that goes in Phase 5.
5. **T1.5 — Defect (e): waypoint double-insert.** `OVT_MultiTownPatrolBehaviorDeploymentModule` inserts each town waypoint into `m_aWaypoints` **twice** (callee `:344`, caller `:240`) and each return waypoint twice (callee `:367`, caller `:263`); `OnDeactivate` (`:71-85`) then calls `DeleteEntityAndChildren` twice on the same entity, and the `if (waypoint)` guard does not protect against it because the array is unmanaged. Remove the **callee-side** inserts so creation and ownership stay in one place. (Phase 5 deletes this code entirely; fixing it here keeps the tree honest in between and keeps the phases independently revertable.)
6. **T1.6 — Defect (d-adjacent): `m_fMaxCruiseSpeed` is dropped by `CloneModule`.** Both shipped vehicle configs author it via the registry override (35 / 30) but `CloneModule` (`OVT_VehicleSpawningDeploymentModule.c:145-160`) never copies it, so the clone always holds the class default. Add the copy. **Leave the apply commented out** (`:206-212`) with its existing "bugged in Reforger" note plus a dated pointer — verify, do not rebuild.
7. **T1.7 — Retire the dead duplicate.** `OVT_DeploymentManagerComponent.DestroyDeployment(...)` (`:1045-1050`) is byte-identical to `DeleteDeployment(...)` (`:1052-1057`) and has **zero callers tree-wide**. Delete it; grep-prove.
8. **T1.8 — BUG-028 regression coverage (best effort).** The fix at `OVT_DeploymentManager.c:838-850` prunes dead ids out of `m_mFactionDeployments` (without it the per-faction cap of 100 silently halts all deploying) and has no test. Add a read-only diagnostic accessor (`int GetFactionDeploymentIdCount(int factionIndex)`) and an Init-tier case that drives `ForceCreateDeployment` → delete the marker entity → `CleanupDestroyedDeployments()` → assert the count drops. **If the Init world cannot drive the manager**, record that verdict in `context.md`, drop the case, and keep the accessor only if something else uses it. Do not invent a fixture to force it.
9. **T1.9 — THE GATE: server-side `InsertObserverSP` verification.** One Init-tier case in `OVT_TEST_InitSuite.c`:
   - pick a position far from the autotest camera; assert `ObserversSystem.HasObserverWithinRangeSq(x, z, r²)` is **false**;
   - spawn a throwaway marker **entity** at that position and `InsertObserverSP(key, 0, 0, thatEntity)` — **never null**, and never a fixed-position insert;
   - assert `HasObserverWithinRangeSq` is now **true**, and that `GetObserversSP()` grew by exactly one;
   - `RemoveObserverSP(key)`; assert it is false again and the count returned;
   - clean up the marker entity **before** reporting.
   Record in `context.md`: whether an SP insert is honoured at all, whether it survives across frames, and the observed key semantics. **Phase 6 must not start until this verdict is written down.** If SP inserts turn out not to be honoured on the authority, Phase 6 re-plans onto `InsertObserverMP` (the vanilla precedent, `SCR_SpawnRequestComponent.c:664`, which follows an entity) and the plan is amended in place.

**Acceptance criteria**

- `tools/compile-check.sh` exits **0**.
- `grep -rn "DestroyDeployment" Scripts/` shows only `OVT_DeploymentComponent.DestroyDeployment` and its two manager call sites.
- `grep -rn "GetOwner().SetOrigin" Scripts/Game/GameMode/Deployments/` → **empty**.
- `grep -rn "OVT-VIRT-PLAYTEST-ONLY" Scripts/ | wc -l` → **unchanged** (this phase removes none).
- `git diff Scripts/Game/GameMode/Virtualization/` → **empty** (core untouched this phase).
- `git diff Configs/` → **empty**.
- The T1.9 verdict is written into `docs/features/virtualization/integration/context.md`, with the exact numbers observed.
- Fast **and** All green; every new case carries a recorded can-fail proof; **no `maxAttempts`**.

---

### Phase 2 — The shared consumer scaffolding *(no behaviour change)*

**Agent:** `component-developer-advanced` — **advanced.** It appends a field to a **persisted binary payload** and adds the manager's invoker plumbing. The code is small; the persistence discipline is the phase.
**Estimate:** 5–7 h
**Suite after this phase:** **All** `{6A6E2A002F53A581}`

**Tasks**

1. **T2.1** Create `Scripts/Game/GameMode/Deployments/OVT_DeploymentVirtualKey.c` — world-free statics only. **No manager, game-mode, world, entity or registry identifier anywhere in the file, comments included**, so the Logic tier can assert it (movement's Phase 1 found the tier grep matches comments; its file header had to avoid the word "world"):
   - `string DeriveKey(string configName, float x, float z)` — `"<sanitised name>@<round(x)>_<round(z)>"`.
   - `string Sanitise(string name)` — collapse the characters that would make a key ambiguous (`@`, `#`, whitespace); empty name → `"unnamed"`.
   - `string Disambiguate(string baseKey, int ordinal)` — `baseKey` for ordinal 0, `baseKey + "#" + ordinal` above.
   - `string ModuleTag(string moduleName, int spawningIndex)` — the name when non-empty, else `"m" + index`.
   - `string OwnerKey(string deploymentKey, string moduleTag)`.
   - `int MissingCount(int wanted, int held)` — never negative.
2. **T2.2** Create `Scripts/Game/GameMode/Deployments/OVT_VirtualPlanFactory.c` — world-free statics that build an `OVT_VirtualWaypointPlan` from **geometry only**; the caller applies road snapping and surface clamps afterwards. Same tier-grep constraint as T2.1.
   - `OVT_VirtualWaypointPlan BuildDefendPlan(vector centre, float radius)`.
   - `OVT_VirtualWaypointPlan BuildPerimeterPlan(vector centre, vector fromPosition, float radius, array<float> waitSeconds)` — 4 points 90° apart starting from the bearing `fromPosition → centre` (matching `GivePatrolWaypoints:594-595`), each followed by a `WAIT` point at the same position; `m_bCycle = true`. Wait durations are passed **in** so the roll stays out of a world-free file and the test can pin the interleaving exactly.
   - `OVT_VirtualWaypointPlan BuildRoutePlan(array<vector> stops, float waitSeconds, bool returnToStart, vector startPosition)` — `MOVE`+`WAIT` per stop, optional closing `MOVE`, `m_bCycle = !returnToStart`.
   - All three keep the three arrays **parallel and equal-length** — a ragged plan is refused outright by `RegisterGroup` (`api.md` §2).
3. **T2.3** Create `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_DeploymentVirtualization.c` (Fast group, `suite: OVT_TEST_LogicSuite`), modelled on `OVT_TEST_Logic_VirtualMovement.c`. Cases per [§7](#7-testing-strategy). Recorded can-fail proof per case; **no `maxAttempts`**; ⚠ `vector.Distance` is +1 ULP off at 1000 m and 2000 m — assert with tolerances, never at an exact boundary; ⚠ `out` and `owned` are reserved local names.
4. **T2.4** `OVT_DeploymentComponent`: add `m_sVirtualKey` + `string EnsureVirtualKey()` (derive-once, ask the manager for a disambiguation ordinal, store) and `string GetVirtualKey()`. Add `void EnsureGroups()` that fans out over `GetSpawningModules()` — inert this phase, because no module implements it yet.
5. **T2.5** `OVT_DeploymentComponentSerializer`: bump to **version 2** and **append** `virtualKey` after `spawnedUnitsEliminated` — never insert, never reorder (binary contexts are positional; the file header already says so). On read, `version < 2` leaves the key empty and `EnsureVirtualKey()` derives it from the live marker position on first use, which is exactly the pre-feature-save migration path. Extend `ApplyPersistedDeployment`'s signature with the key, keeping its documented flag-before-`InitializeDeployment` ordering **unchanged**.
6. **T2.6** `OVT_DeploymentManagerComponent`: subscribe **once** to `virt.GetOnRecordsRestored()` and `virt.GetOnGroupWiped()` (in `PostGameStart`, guarded so a second campaign in the same session does not double-subscribe), and fan out over `GetAllDeployments()`. Add `int NextKeyOrdinal(string baseKey)` for T2.4's disambiguation. Both handlers are no-ops until Phase 3 gives the modules something to reclaim.
7. **T2.7** Add one `OVT_BaseBehaviorDeploymentModule` virtual: `OVT_VirtualWaypointPlan BuildVirtualPlan(vector groupPosition)`, defaulting to `null` ("I have no opinion"). Add the spawning-module helper that walks `m_ParentDeployment.GetBehaviorModules()` and takes the **first non-null** answer. No override exists yet.
8. **T2.8** Seed `docs/features/virtualization/integration/context.md`: the Phase 1 spike verdict, the key scheme, the ordering note of §3.5, the Tower Garrison config design of §3.3, and the phase-by-phase kill-switch removal ledger.

**Acceptance criteria**

- compile **0**; Fast and All green.
- `grep -rniE "Manager|World|Entity|OVT_Global|GetGame\(\)|GetInstance" Scripts/Game/GameMode/Deployments/OVT_DeploymentVirtualKey.c Scripts/Game/GameMode/Deployments/OVT_VirtualPlanFactory.c` → **empty**.
- `git diff Scripts/Game/GameMode/Virtualization/` → **empty**.
- `git diff Configs/` → **empty** (the serializer version bump is code, not config).
- **No behaviour change:** nothing registers a group in this phase; a save written before it still loads, and a save written by it still loads on the pre-phase build minus the appended field.

---

### Phase 3 — Town Patrol: infantry migration and the death of the proximity toggle

**Agent:** `component-developer-advanced` — **advanced.** It retires a live subsystem, changes what the stability modifier reads underneath it, and is the first code in the campaign to register groups.
**Estimate:** 10–14 h
**Suite after this phase:** **All** `{6A6E2A002F53A581}`

**Tasks**

1. **T3.1** `OVT_PatrolBehaviorDeploymentModule`: implement `BuildVirtualPlan(vector groupPosition)` per §3.8 — `DEFEND` → `BuildDefendPlan` at `GetPatrolCenter()`; `PERIMETER` → roll 4 wait durations (`RandFloatXY(45, 75)`), call `BuildPerimeterPlan`, then road-snap each patrol point with `OVT_WorldUtils.FindNearestRoad` (the wait point copies its patrol point's snapped position). **Delete** `ApplyPatrolBehaviorToGroup` (`:145-176`), `ApplyPatrolBehaviorToExistingGroups`, `CheckForNewGroups`, `IsGroupProcessed`, `m_aProcessedGroups`, `ForceReapplyPatrolBehavior` and the `OnUpdate` polling — waypoints are core's now, and a consumer must never create or delete them on a registered group (core D6). `GetPatrolCenter()` stays; it is what makes the `DEFEND` branch usable by the Tower Garrison config in Phase 4.
2. **T3.2** `OVT_InfantrySpawningDeploymentModule`: replace `m_aSpawnedGroups : array<SCR_AIGroup>` with `m_aHandles : array<int>` and implement `EnsureGroups()` per §3.5. Registration per group: position = `GetRandomSpawnPosition(baseSpawnPos)` (unchanged — the ring + road snap stays), `plan = <first behaviour module's BuildVirtualPlan(thatPosition)>`, `groupName = m_sGroupType`, `factionKey` resolved from the deployment's faction **index → key** (never pass the index), `spawnDistanceOverride = -1`, `importance = m_eImportance` (**new attribute, default `NORMAL`**, §3.9 — copy it in `CloneModule`). Tag the returned group for the GM registry immediately (§3.7).
3. **T3.3** Rewrite `Reinforce(int groupsNeeded)` to rebuy through the same API: charge `groupsNeeded * m_iReinforcementCost`, raise the wanted count, call the `EnsureGroups()` convergence loop, clear `m_bSpawnedUnitsEliminated` on success and re-run `CheckAllSpawningModulesEliminated()`. Keep `CanReinforce`, the cost arithmetic and the log lines. `GetMissingGroupCount()` answers from handles.
4. **T3.4** Delete the agent-count elimination machinery: `CheckIfUnitsEliminated()`, `CheckGroupStatus()`, `IsGroupAlive()`, `GetAliveGroupCount()`, and `m_iSpawnedCount`'s role as a liveness proxy (keep a simple `m_iSpawnedEver` latch). Implement `OnGroupWiped(int handle)` per §3.6.
5. **T3.5** Teardown: `OnDeactivate()` becomes a no-op; `OnCleanup()` unregisters every held handle and clears the list ([D8](#d8--teardown-funnels-through-oncleanup-not-ondeactivate--and-there-is-no-ondelete)). **Add no `OnDelete`** — a component `OnDelete` fires at quit-to-menu and would erase the records persistence exists to keep (core's own T3.11 deviation, `core/context.md`).
6. **T3.6** `OVT_DeploymentComponent.UpdateDeployment` (`:220-275`): keep the three module `Update()` loops; replace the whole proximity block (`:255-275`) with a single unconditional *activate-once*. **Delete** `IsPlayerInRange()` (`:278-281`) and `DeactivateDeployment()` (`:188-217`) once grep-proven to have no callers. Leave `OVT_DeploymentConfig.m_bEnableProximityActivation` and `m_fActivationRange` **declared** — the shipped config surface is frozen ([D1](#d1--in-place-module-rewrite-the-config-surface-of-the-three-shipped-configs-is-frozen)) — and mark both `desc` strings as no longer driving group lifecycle.
7. **T3.7** Wire the manager fan-outs from T2.6 to something real: `GetOnRecordsRestored()` → `foreach deployment : GetAllDeployments() → deployment.EnsureGroups()`; `GetOnGroupWiped(handle)` → fan out to each deployment's spawning modules.
8. **T3.8** **Remove the kill-switch guard** at `OVT_DeploymentManager.c:144` and confirm the evaluator's own guards still hold: the 0-players early return (`:150`) and the QRF early return (`:153-155`) are **kept** — this feature migrates group lifecycle, not the evaluation cadence ([D9](#d9--the-30-s-evaluator-and-its-guards-are-not-touched)).
9. **T3.9** Init-tier coverage per [§7](#7-testing-strategy): the registry resolves `"Town Patrol"` and its patrol module builds a non-empty **cycling** plan; a simulated `EnsureGroups` cycle registers, reclaims idempotently and unregisters, using `spawnDistanceOverride = 0` so the autotest camera cannot materialise anything (core's Manual-policy guard, `core/context.md` 2026-08-17).
10. **T3.10** Record in `context.md`: the retired-symbol list with its grep verdicts, and the `RegisterGroup(` fixture sweep (movement's D12 discipline — **any** new fixture registering a movable plan will be walked by the movement tick).

**Acceptance criteria**

- compile **0**; Fast and All green.
- `grep -rn "IsPlayerInRange\|DeactivateDeployment\|m_aProcessedGroups\|ApplyPatrolBehaviorToGroup\|CheckIfUnitsEliminated\|IsGroupAlive" Scripts/Game/GameMode/Deployments/` → **empty**.
- `grep -rn "OVT_EntitySpawningAPI" Scripts/Game/GameMode/Deployments/Modules/OVT_InfantrySpawningDeploymentModule.c` → **empty**.
- `grep -rn "AddWaypoint\|RemoveWaypoint\|GetWaypoints\|GivePatrolWaypoints" Scripts/Game/GameMode/Deployments/` → **empty** (core owns waypoints for registered groups).
- `grep -rn "OVT-VIRT-PLAYTEST-ONLY" Scripts/ | wc -l` → **one fewer** than before this phase; every remaining guard intact.
- `git diff Scripts/Game/GameMode/Virtualization/` → **empty**; `git diff Configs/` → **empty**.
- `git diff --stat Scripts/Game/GameMode/Deployments/` shows a **net deletion**.
- `OVT_PatrolHarassmentStabilityModifier` is **not edited** and still compiles against the same three calls.

---

### Phase 4 — Tower garrisons become a deployment config

**Agent:** `component-developer-advanced` — **advanced.** It authors new campaign content, adds two module classes, and edits `OVT_OccupyingFactionManager` — 1678 lines of replication-adjacent campaign state with an append-only persisted tower record and a JIP `RplSave`/`RplLoad` stream. None of that may move. The edit there is a **deletion**, which is the safest shape available, but the survey has to come first.
**Estimate:** 7–10 h
**Suite after this phase:** **All** `{6A6E2A002F53A581}`

**Tasks**

1. **T4.1 — Read-only survey first.** Confirm before editing: `OVT_RadioTowerData.garrison` (`:72-73`) has **no reader outside `CheckRadioTowers`** (verified 2026-08-17 — every other `garrison` hit in the tree belongs to base/FOB/camp data); it is `[NonSerialized]`; it is absent from `OVT_PersistedRadioTower` (`OVT_OccupyingFactionManagerSerializer.c:59-68`, field order `location, faction, disabledRemaining`) and absent from the JIP stream (`RplSave:1553-1561`). Record the verdicts. **Do not touch any persisted or streamed field order.**
2. **T4.2 — Verify the composition.** Confirm `light_patrol` resolves to the same prefab `m_aTowerDefensePatrolPrefab` names, **for both shipped factions** (`Configs/Factions/USSR_OverthrowData.conf:67` → `Group_USSR_SentryTeam.et`; `US_OverthrowData.conf:62` → `Group_US_SentryTeam.et`). If they match, the config uses `light_patrol`. If not, add a `tower_garrison` registry entry to both faction configs and use that name — **never** a raw prefab or a faction index.
3. **T4.3 — New module: `OVT_RadioTowerControlConditionDeploymentModule`.** A structural copy of `OVT_BaseControlConditionDeploymentModule` (`:1-105`) against `OVT_OccupyingFactionManager.GetNearestRadioTower(position)` instead of `GetNearestBase`: `EvaluateStaticCondition` (gates creation), `EvaluateCondition` (gates runtime, feeding `m_bDeleteOnConditionFail`), `m_fMaxDistance` (default 300), `m_bRequireControl` (default true), and a hand-written `CloneModule` copying all three attributes.
4. **T4.4 — New module: `OVT_RadioTowerCaptureBehaviorDeploymentModule`.** `OnUpdate`: if `m_ParentDeployment.GetSpawnedUnitsEliminated()` and an edge latch has not fired, resolve `GetNearestRadioTower(deploymentPos)` within `m_fMaxDistance`, verify the tower is still the deployment's faction (so a double-fire or a race cannot un-capture), and call `ChangeRadioTowerControl(tower, GetPlayerFactionIndex())` **once**. Notifications and the broadcast RPC are `ChangeRadioTowerControl`'s own (`:631-647`) and are not duplicated here. `CloneModule` copies the attributes and **not** the latch.
5. **T4.5 — Author `Configs/Deployment/Deployment_TowerGarrison.conf`** per §3.3, with a **fresh, repo-unique GUID**, and append its registry entry to `Configs/Deployment/overthrowDeployments.conf` in the shape the three existing entries use. Tune `m_iMinGroupCount`/`m_iMaxGroupCount` against today's `RandInt(config.m_Difficulty.patrolGroupsMin, patrolGroupsMax)` (`OVT_OccupyingFactionManager.c:578`), and set a **non-zero** `m_iBaseCost`/`m_iCostPerGroup` ([D17](#d17--tower-garrisons-cost-resources-like-any-other-deployment)) — flag the exact numbers as a play-test tuning question rather than pretending they are derived.
6. **T4.6 — Close the location-classification gap.** `GetLocationTypeAtPosition` (`OVT_DeploymentManager.c:661-720`) returns a **single** flag and checks TOWN then BASE (500 m) **before** RADIO_TOWER (300 m), so a tower inside a town's bounds or near a base would never classify as `RADIO_TOWER` and would never get a garrison. Fix **surgically**: compute the existing single value exactly as today, then **OR in `RADIO_TOWER`** when within 300 m of a tower. Nothing else in the precedence changes, so no existing config's candidate acceptance moves — `CanUseLocationType` is already a bitwise test (`OVT_DeploymentConfig.c:104-110`) ([D19](#d19--radio_tower-is-ored-in-not-promoted-in-the-precedence)).
7. **T4.7 — Delete the garrison halves of `CheckRadioTowers()`.** Keep the sabotage countdown alone (`:547-557`), byte-for-byte including its `Rpc(RpcDo_SetRadioTowerDisabled, ...)` and the repair notification. **Delete** the `inrange`/`!m_CurrentQRF` gate (`:559`), the spawn block (`:560-589` — the `OVT-VIRT-PLAYTEST-ONLY` guard at `:564` goes with it, [D20](#d20--the-tower-kill-switch-guard-is-removed-by-deleting-the-code-it-wraps)), the reap+capture block (`:590-615`) and the despawn block (`:616-627`). Delete `OVT_RadioTowerData.garrison` (`:72-73`). The `CallLater(CheckRadioTowers, 9000, true, GetOwner())` at `:321` stays — sabotage still needs a 9 s tick. Grep-note that `OVT_Faction.m_aTowerDefensePatrolPrefab` now has no reader; **leave the attribute declared** (it is authored in faction configs and prefabs) and hand the cleanup to feature 5.
8. **T4.8 — Coverage.** Logic tier: nothing new (the key statics already cover it). Init tier: the registry resolves `"Tower Garrison"` and its config is valid (`IsValidConfig`); its patrol module builds a **one-point `DEFEND`** plan with `m_bCycle` false (the "garrisons never wander" claim, asserted); `GetLocationTypeAtPosition` at a tower position includes the `RADIO_TOWER` bit **and** still includes whatever it returned before. Plus the phase's headline claim: **a proximity despawn does not capture a tower** — drive the capture module with a deployment whose groups are registered but *not* wiped and assert it does not fire; then report deaths through `ReportMemberKilled` until the last group wipes and assert it fires **exactly once**.
9. **T4.9 — Document the behaviour changes** in `context.md` and hand them to Phase 8: garrisons now materialise for **any engine observer** (a GM free camera included — `core/context.md` 0a); they no longer vanish map-wide during a QRF; garrison **creation** now costs resources and pauses while a QRF is active or no players are connected; GM icons for tower garrisons now read `DEPLOYMENT` / `"Tower Garrison"` rather than `RADIO_TOWER_GARRISON` / `"RadioTower"` (§3.7).

**Acceptance criteria**

- compile **0**; Fast and All green (`OVT_TEST_PersistenceRoundTrip_TowerSabotage_SurvivesSaveAndReload` must stay green — sabotage is deliberately untouched).
- `grep -n "garrison" Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c` → no hit on `OVT_RadioTowerData`; the file's remaining `garrison` hits belong to base/camp data only.
- `grep -n "m_CurrentQRF\|PlayerInRange" Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c` → no hit inside `CheckRadioTowers`.
- `git diff` on `OVT_PersistedRadioTower`, the serializer's write/read order, `RplSave`, `RplLoad` and `ApplyPersistedOccupyingFaction` → **empty**.
- `git diff --stat Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c` → **net deletion** of roughly 70 lines.
- `grep -rn "OVT-VIRT-PLAYTEST-ONLY" Scripts/ | wc -l` → **one fewer** again, and the removed line is gone **with its enclosing block**, not un-commented.
- `git diff Configs/` shows **exactly two** files: the new `Deployment_TowerGarrison.conf` (+ its `.meta`) and the one appended registry entry. **The three shipped deployment configs are unchanged.**
- `git diff Scripts/Game/GameMode/Virtualization/` → **empty**.

---

### Phase 5 — Vehicle patrols

**Agent:** `component-developer-advanced` — **advanced.** Crew seating, vehicle ownership and the stolen-vehicle guarantee all live here, and a mistake deletes a player's vehicle.
**Estimate:** 8–12 h
**Suite after this phase:** **All** `{6A6E2A002F53A581}`

**Tasks**

1. **T5.1** `OVT_MultiTownPatrolBehaviorDeploymentModule`: implement `BuildVirtualPlan(vector groupPosition)` from the existing nearest-neighbour route planning (`m_aPatrolRoute`), via `OVT_VirtualPlanFactory.BuildRoutePlan(stops, m_iWaitTimeAtTown, m_bReturnToStart, deploymentOrigin)`. **Delete** `CreatePatrolWaypoints`, `CreateWaypointsForVehicles`, `CreateTownWaypoint`, `CreateReturnWaypoint`, `m_aWaypoints` and the `OnDeactivate` waypoint teardown (`:71-85`) — the double-insert of T1.5 disappears with the array. Route **planning** stays; only waypoint **authoring** goes.
2. **T5.2** Re-express route completion without waypoint entities. Re-read `CheckPatrolComplete` (`:388-416`) first: if it already measures position, keep it and re-point it at the plan's final stop; if it inspects waypoints, replace it with a distance check against the final plan point (≤ ~100 m) plus the existing `m_bReturnToStart` gate. `OnPatrolComplete` (`:419-453`) — the refund (now non-zero thanks to T1.3) and `DeleteDeployment` (`:450`) — is otherwise unchanged. ⚠ It self-deletes the deployment from inside its own module's update frame; keep that shape rather than making it worse.
3. **T5.3** `OVT_VehicleSpawningDeploymentModule`: keep vehicle spawning and `GetRandomSpawnPosition` exactly as they are (base vehicle-patrol spawn point, then the road fallback). Register the **crew** through core: `groupName = m_sCrewGroupType`, position 5 m from the vehicle (unchanged), plan from the behaviour module, `importance = NORMAL`, `spawnDistanceOverride = m_iSpawnDistanceOverride` (**new attribute, default 100000** — always materialised, [D10](#d10--vehicle-crews-register-always-materialised); add it to `CloneModule` alongside T1.6's fix). Tag the group for the GM registry.
4. **T5.4** Replace crew seating's `GetOnInit()` subscription (`:269`, `OnCrewGroupInitialized` `:275-366`) with `GetOnAgentAdded()` per member. `api.md` §3 is explicit: in 1.8 `Event_OnInit` fires only on a **complete** group fill, which under budget pressure may never happen — and a core-registered group fills progressively through the engine's queue, so today's path would frequently never seat anyone. Seat each arriving agent into the next free compartment in the existing PILOT → TURRET → CARGO order; keep `FillCompartment` unchanged. Unsubscribe when the group is unregistered.
5. **T5.5** Teardown ([D11](#d11--the-stolen-vehicle-guarantee-is-verified-not-rebuilt)): `OnDeactivate()` → no-op; `OnCleanup()` → `UnregisterGroup(crewHandle)` for each crew (which **respects held members** and retires the group in place rather than deleting it out from under a vehicle), then delete each vehicle **only if** it is unoccupied by any player-controlled entity and is not player-owned. **Delete the 40 m rule** (`:85-99`). Before writing the occupancy check, verify `SCR_AIGroup.HasHeldMember` semantics against the engine source and record the verdict — the requirement says verify, not rebuild.
6. **T5.6** Delete the vehicle module's group-liveness machinery (`CheckIfUnitsEliminated`'s group half, `:433-440`) and route crew death through `OnGroupWiped`. Vehicle destruction accounting (`IsVehicleOperational`, `:466-477`) is **kept** — a vehicle is not a group and core does not own it.
7. **T5.7** **Delete `Scripts/Game/GameMode/Deployments/OVT_EntitySpawningAPI.c`** ([D13](#d13--ovt_entityspawningapic-is-deleted-outright)). Gate: `grep -rn "OVT_EntitySpawningAPI" Scripts/` must return **only comment references** first; rewrite those six comments to name what actually happens now. If a concurrent session has added a live caller, **keep the file**, delete only the kill-switch guard at `:49`, and record why in `context.md`.
8. **T5.8** Init-tier coverage: `BuildRoutePlan` shape for both `returnToStart` values (Logic, T2.3); the two vehicle configs resolve through the registry and their crew group names resolve to real compositions.

**Acceptance criteria**

- compile **0**; Fast and All green.
- `grep -rn "OVT_EntitySpawningAPI" Scripts/` → **empty**, or comment-only with the file retained and the reason recorded.
- `grep -rn "GetOnInit()" Scripts/Game/GameMode/Deployments/` → **empty**.
- `grep -rn "distance > 40\|m_aWaypoints" Scripts/Game/GameMode/Deployments/` → **empty**.
- `grep -rn "OVT-VIRT-PLAYTEST-ONLY" Scripts/ | wc -l` → **one fewer** (the `OVT_EntitySpawningAPI.c:49` backstop), unless T5.7's gate kept the file — then unchanged, with the reason recorded.
- `git diff Configs/Deployment/Deployment_TownPatrol.conf Configs/Deployment/Deployment_VehiclePatrol_Light.conf Configs/Deployment/Deployment_VehiclePatrol_Heavy.conf` → **empty**.
- `git diff Scripts/Game/GameMode/Virtualization/` → **empty**.

---

### Phase 6 — AI observers: the core API and the recruit wiring

**Agent:** `component-developer-advanced` — **advanced.** It edits the epic's **frozen** core and its contract documents, and a mistake here (a null-entity insert, a leaked observer) freezes a client or pins the whole map awake.
**Estimate:** 6–8 h
**Blocked on:** the **T1.9 verdict** being written into `context.md`.
**Suite after this phase:** **All** `{6A6E2A002F53A581}`

**Tasks**

1. **T6.1** Add the four methods of [§3.10](#310-the-one-additive-core-ask--ai-observers) to `OVT_VirtualizationManagerComponent`, beside the ambient section. Requirements, all of them load-bearing:
   - **refuse a null entity** with a WARNING and `false` — never call `InsertObserverSP` with null (`core/context.md` gotcha 0);
   - server guard + null-guard the `ObserversSystem` (reuse core's existing `GetObserversSystem()` at `:3236`);
   - idempotent per entity (re-adding returns true and reuses the key);
   - keys from a namespaced monotonic counter, never persisted, never replicated — the 1.8 tree has **zero** vanilla script callers of `InsertObserverSP`/`RemoveObserverSP`, so the SP key space is entirely Overthrow's, but namespacing costs nothing;
   - a stale-entity sweep folded into core's existing 2 s ambient tick (`FindEntityByID` fails → `RemoveObserverSP` + drop);
   - `OnDelete` removes **every** observer — a leaked SP observer across a campaign restart would pin content awake for the rest of the process.
2. **T6.2** `api.md`: add the four signatures to §3 (a new short "AI observers" block) and a row per method to §10's `integration` table; extend the header's "Additively extended" note to name this fourth additive change.
3. **T6.3** `core/context.md`: dated entry under **"Additive changes after the freeze"**, naming `virtualization/integration` Phase 6 as the requester, quoting the T1.9 spike verdict, and stating why it is not breaking (pure addition, no rename, no payload field, `CONFIG_STREAM_VERSION` unmoved, no existing call path changed).
4. **T6.4** Wire the consumer: `OVT_InactiveRecruitGroupComponent` — the marker component on a parked-recruit group, which already owns that group's defend waypoint and its `OnDelete` cleanup — adds an observer following **its own group entity** when the group is created on the server, and removes it in `OnDelete`. One component, one obvious lifetime, no manager plumbing. Gate the whole thing behind a manager attribute (`m_bRecruitGroupsAreObservers`, default **true**) so an operator can switch it off ([D16](#d16--observers-are-wired-to-parked-recruit-groups-with-an-off-switch)).
5. **T6.5** Init-tier coverage: `AddEntityObserver(null)` is a safe `false` (and the case survives it — this is the freeze guard, so it is the most valuable case in the phase); add/has/remove/count round-trip on a throwaway entity; adding twice keeps the count at 1; removing an unknown entity is a safe `false`. Clean up before reporting.
6. **T6.6** `context.md`: the cost note — an observer pins every registered group inside its spawn ring materialised for as long as it exists, so a squad parked in a town keeps that town's tower garrison and patrols awake. That is the feature; it is also the budget risk, and the off-switch is why it is acceptable.

**Acceptance criteria**

- compile **0**; Fast and All green.
- `git diff Scripts/Game/GameMode/Virtualization/` shows **only** the four added methods, their map/counter, the tick sweep and the `OnDelete` teardown — nothing renamed, nothing removed, no existing line re-meant.
- `grep -rn "InsertObserverSP" Scripts/` → **exactly one** call site, and the line above it is a null guard.
- `api.md` §3 and §10 both list the four methods; `core/context.md` carries the dated note naming `virtualization/integration`.
- `git diff Configs/` → **empty**.

---

### Phase 7 — Save compatibility and persistence-tier coverage

**Agent:** `component-developer-advanced` — **advanced.** It extends the shared All-group gate and asserts the epic's most load-bearing claim.
**Estimate:** 5–7 h
**Suite after this phase:** **All** `{6A6E2A002F53A581}`

**Tasks**

1. **T7.1 — The `RegisterGroup(` fixture sweep, before writing any case.** `grep -rn "RegisterGroup(" Scripts/Game/Tests/` and record a verdict per site, in `context.md`, in the table shape movement's `context.md` uses. A fixture is safe only if it registers a null/empty/DEFEND-only plan **or** registers and unregisters inside one frame — otherwise the movement tick walks it. Every new fixture this phase adds must satisfy one of the two.
2. **T7.2 — Case: a migrated deployment round-trips.** On the shared gate (`OVT_TEST_PersistenceRoundTripSuite`, All group): build a deployment component state with a config name, faction, threat, invested resources and a **virtual key**; save; dirty; re-apply; assert all five come back, that the key is the *same string* (not re-derived), and that `FindConfigByName` still resolves. Covers the serializer v2 append and the config-name resolution the requirements name.
3. **T7.3 — Case: an eliminated deployment does not resurrect its force.** Restore a deployment with `spawnedUnitsEliminated = true` and assert `EnsureGroups()` registers **zero** groups — the flag-before-`InitializeDeployment` ordering the existing serializer header documents, now with a consequence that can be asserted. This is G4's teeth.
4. **T7.4 — Case: a version-1 payload still loads.** Feed a v1-shaped payload (no key) and assert the deployment restores and derives a key on first use. This is literally the pre-feature-save migration path for town and vehicle patrols; tower garrisons need no migration at all, because a pre-feature save simply has none and the evaluator creates them ([D2](#d2--one-consumer-seam-tower-garrisons-become-a-deployment-config)).
5. **T7.5 — Member survival across the round trip** is already covered by core's `..._VirtualGroups_SurviveSaveAndReload` and `..._VirtualGroupsWiped_DoNotComeBack`. **Do not duplicate them.** Add instead one claim they do not make: a group registered under a *deployment* owner key is reclaimable by `FindGroupsByOwner` after the restore — the reclaim contract this whole feature rests on.
6. **T7.6 — Un-guard the Campaign-tier GM registry case.** `OVT_TEST_Campaign_GMGroupRegistry.c:74` trivial-passes with a loud WARNING while the kill switch is on, because its only observable producers were the gated paths. Deployments — now including tower garrisons — are producers again after Phases 3–4, so remove the guard and let the case assert for real. If base-upgrade producers turn out to be required for it to pass, restore the guard and record why. **Check whether the case asserts on `RADIO_TOWER_GARRISON` specifically**; if it does, re-point it at `DEPLOYMENT` (§3.7) rather than reviving the old origin.
7. **T7.7** Decode a real save (the project's save-decode path, `docs/features/persistence/`) and confirm by inspection: deployment records carry the new key field; a Tower Garrison deployment appears as an ordinary deployment record; **no core-owned AI record is ever self-spawned back** (`AIGroup`/`AIUnit`/`AIWaypoint` records **do** exist in the save, written with `SelfSpawn 0`, and nothing rebuilds an entity from them — *corrected 2026-08-17 by Phase 8; this task originally read "there is **no** vanilla `AIGroup`/`AIUnit`/`AIWaypoint` record for anything core owns", which the decode disproved: see `context.md` T7.7 finding 5*); the tower records are unchanged in field order. Record the findings.

**Acceptance criteria**

- compile **0**; **All** green, including the un-guarded Campaign case.
- Every new case has a recorded can-fail proof; **no `maxAttempts`**.
- The fixture-sweep table is in `context.md` with a verdict per site.
- `git diff Configs/Systems/Persistence/Overthrow.conf` → **empty** (the deployment binding already exists at `:189-203`; only the serializer's payload version moves).

---

### Phase 8 — Help & documentation sync

**Agent:** `help-docs-sync`
**Estimate:** 2–3 h
**Suite:** skipped — docs-only. Say so.

Players see genuinely new behaviour for the first time in this epic, so the closing sync is in scope. Movement deliberately deferred its player-facing duty to this feature; it arrives here.

1. **T8.1** Tutorial popups (`Configs/Tutorials/`) and the Field Manual (`Configs/FieldManual/`): fact-check **every** existing sentence about patrols, garrisons and radio towers against the shipped code, and cite a `file:line` or cut the sentence (the project has shipped invented mechanics twice; no gate catches a well-formed lie).
2. **T8.2** The four real player-visible changes to document:
   - **dead members stay dead** — a patrol you shot up comes back at the strength you left it, with the same men, across despawn *and* across save/load;
   - **patrols are not where you left them** — a town patrol keeps walking its route while nobody is watching, so it will not be at the corner you last saw it;
   - **towers are taken by clearing the garrison**, and clearing it now means actually killing them — walking away and coming back no longer resets the fight;
   - **towers may be found ungarrisoned** when the occupying faction is short of resources, and a garrison can appear later ([D17](#d17--tower-garrisons-cost-resources-like-any-other-deployment)).
3. **T8.3** Wiki: the same four points, plus the two operator-facing notes from Phase 4 — a GM free camera counts as an observer and keeps content near it spawned, and tower garrisons no longer disappear during a QRF.
4. **T8.4** Epic bookkeeping: update `epic-overview.md`'s `integration` row and rollup; note in the `base-defense-migration` row that the deployments↔virtualization seam is proven **and that the Tower Garrison config is its worked precedent**. Update the master `docs/overview.md` row. Update `api.md` §6's prose sentence about `m_iMilitarySpawnDistance` to name the systems that still read it (base upgrades, QRF) now that deployments and towers do not.
5. **T8.5** **The kill-switch ledger.** Confirm `grep -rn "OVT-VIRT-PLAYTEST-ONLY" Scripts/` returns exactly the guards for systems this feature did **not** migrate (base upgrades ×2, QRF queue, plus the switch file itself), and that the ledger in `context.md` accounts for every removed line — including that the tower guard was removed **with its enclosing block**. The switch itself is removed at **epic end**, not here.

**Acceptance criteria** — all three surfaces (tutorials, Field Manual, wiki) agree with the code; no invented mechanics; every claim carries a `file:line`; the guard ledger balances.

---

## 5. Key Technical Decisions

### D1 — In-place module rewrite; the config surface of the three shipped configs is frozen

*(user decision, 2026-08-17)*

`OVT_InfantrySpawningDeploymentModule` and `OVT_VehicleSpawningDeploymentModule` keep their class names, their existing `[Attribute]` sets and their position in the module composition; only their bodies change. `Deployment_TownPatrol.conf`, `Deployment_VehiclePatrol_Light.conf` and `Deployment_VehiclePatrol_Heavy.conf` are **not edited** — an empty `git diff` on those three is an acceptance criterion in three phases. New attributes (`m_eImportance`, `m_iSpawnDistanceOverride`) are additive with defaults matching today's behaviour, so an unauthored config is unaffected.

The rejected alternative was new `*Virtual*` module classes alongside the old ones with a config migration: it would have meant editing every shipped config, and a period where two spawning implementations coexisted and could both fire. The cost of the in-place choice is that **module clone-fragility survives** — `CloneModule` copies attributes by hand and silently drops any it forgets, which is exactly how `m_fMaxCruiseSpeed` got lost (T1.6) and is a trap every new attribute in this feature must dodge. That is feature 5's problem and is recorded, not fixed, here.

### D2 — One consumer seam: tower garrisons become a deployment config

*(user decision, 2026-08-17 — supersedes the original "second direct consumer" design)*

Tower garrisons do **not** migrate in place inside `OVT_OccupyingFactionManager` with a `"tower_garrison"` owner system. They become `Configs/Deployment/Deployment_TowerGarrison.conf` ([§3.3](#33-the-tower-garrison-config)), so this feature builds and proves **exactly one** tracked-group consumer.

Four reasons this is better:

1. **One seam to get right, one seam to review.** Registration, reclaim, wipe accounting, GM tagging, key derivation and save compatibility exist once, in `EnsureGroups`, instead of twice in two very different managers.
2. **The evaluator already does the work.** `GetRadioTowerPositions` (`OVT_DeploymentManager.c:364-381`) is real and wired; candidate scoring, threat, the 250 m same-name dedup and the resource pool all come free. The only gap is the location classification, and it is one `|=` ([D19](#d19--radio_tower-is-ored-in-not-promoted-in-the-precedence)).
3. **Save compatibility collapses to nothing.** A pre-feature save has no tower deployments; the 30 s evaluator creates them like any candidate. No bespoke reclaim code in the faction manager at all — the manager loses code rather than gaining it.
4. **It is the precedent `base-defense-migration` needs.** Feature 5's nine base-upgrade classes face exactly this question — "how does a bespoke garrison system become a virtualized one?" — and the answer is now a shipped, working, four-module config they can copy instead of a design discussion.

The cost is two new module classes and a new config file, and the behaviour changes in [D17](#d17--tower-garrisons-cost-resources-like-any-other-deployment) and [D18](#d18--tower-capture-is-driven-by-the-eliminated-flag-not-by-an-empty-list), both accepted deliberately.

### D3 — The inherited defects are fixed first, in their own phase

*(user decision, 2026-08-17)*

All five live in files this migration rewrites, and three of them (the two unit mismatches and the marker teleport) would change behaviour underneath the migration in ways that make a play-test unreadable. Fixing them in Phase 1, with the suites run before any registration exists, means a red gate in Phase 3 is attributable to Phase 3. It also means each fix is independently revertable — including T1.5, which fixes a double-delete in code Phase 5 deletes outright.

### D4 — The plan **is** the opt-in, inherited verbatim from movement

Garrisons register a one-point `DEFEND` plan and are never moved. Town patrols register a cycling `PATROL`/`WAIT` plan and patrol virtually the moment they go dormant. Vehicle crews are excluded by being always materialised. There is no flag to set, no registration argument, and no core field — `movement/context.md` → "For `integration`" states this as a contract and Phase 4 of that feature asserts it from both sides. The one obligation it puts on this feature is **not to author a movable plan for something that should stay put**, which the §3.8 table pins per config and T4.8 asserts for the Tower Garrison.

### D5 — Behaviour modules supply plans; the order inverts

Core builds waypoint entities at registration from the plan and forbids consumers from creating or deleting waypoints on a registered group (`api.md` §10, core D6). So the behaviour module can no longer bolt waypoints onto a spawned group — it has to answer **before** the group exists. `OVT_BaseBehaviorDeploymentModule` gains `BuildVirtualPlan(vector groupPosition)` defaulting to `null`, the spawning module takes the first non-null answer from `GetBehaviorModules()`, and every waypoint-authoring method in both behaviour modules is deleted.

A pleasant side effect: `OVT_PatrolBehaviorDeploymentModule`'s `DEFEND` branch — dead in the shipped configs today — becomes the Tower Garrison's whole behaviour, so the new config needs **no** new spawning or behaviour code beyond the two tower-specific modules.

Rejected: keeping the behaviour modules as they are and letting them re-author waypoints after materialisation. It breaks core's ownership rule, it would re-run on every spawn cycle, and on a **cycled** group it walks straight into [BUG-175](#9-risks--mitigation)'s recursive-`Invoke` shape.

### D6 — The deployment key is derived once and persisted

`"<configName>@<round(x)>_<round(z)>"`, derived on first need and stored in `m_sVirtualKey`, appended to the serializer behind version 2.

Derive-every-time was rejected because it depends on a position, and the code that moved that position (`SetOrigin` on the marker, T1.4) is one of the defects this feature fixes — a key that silently changed would duplicate a deployment's whole force on the next reclaim. A monotonic counter was rejected because the counter itself would have to be persisted and re-synchronised. Reading vanilla's persistent id was rejected because it is not assigned until the first save and the read path is unverified. Derive-and-store is self-contained, human-readable in a log, and immune to all three; a **version-1 payload with no key derives one on first use**, which is precisely the pre-feature-save migration path and is asserted by T7.4.

After [D2](#d2--one-consumer-seam-tower-garrisons-become-a-deployment-config) this scheme also carries tower garrisons for free: `"Tower Garrison@4821_7093"` is per-tower, stable, and needed no tower-specific design at all.

### D7 — The manager subscribes; the modules never do

`OVT_DeploymentManagerComponent` subscribes `GetOnRecordsRestored()` and `GetOnGroupWiped()` exactly once and fans out over `GetAllDeployments()`. Deployment modules are **cloned config objects** (`CloneModule`) with no stable identity, destroyed by `Cleanup()` whenever a deployment is deleted — and `OVT_MultiTownPatrolBehaviorDeploymentModule` deletes its own deployment from inside its own update frame (`:450`). A `ScriptInvoker` holding a method pointer into one of those is a dangling call waiting for the first route completion. The manager is a component with a world-scoped lifetime and already has the deployment list.

After [D2](#d2--one-consumer-seam-tower-garrisons-become-a-deployment-config) there is no second subscriber anywhere — `OVT_OccupyingFactionManager` never touches the virtualization API at all.

### D8 — Teardown funnels through `OnCleanup`, not `OnDeactivate` — and there is no `OnDelete`

Once the proximity toggle is gone, `Deactivate()` means "stop ticking" and only `Cleanup()` means "this deployment is over". Both spawning modules' `OnDeactivate` become no-ops and all unregistration moves to `OnCleanup` — one funnel, reached from exactly one place (`OVT_BaseDeploymentModule.Cleanup()` at `:46-53`, itself reached only from `OVT_DeploymentComponent.DestroyDeployment`).

**No `OnDelete` is added to any of these classes.** A component `OnDelete` fires at quit-to-menu, and unregistering there would delete the records persistence exists to keep — core hit exactly this and made its own `OnDelete` detach rather than destroy (`core/context.md`, T3.11 deviation). `civilians` records the same rule as "`OnDelete` destroys nothing — an explicit Deactivate path does".

### D9 — The 30 s evaluator and its guards are not touched

`EvaluateDeployments` keeps its 10 s one-shot + 30 s repeating install (both on the same method, so a single `Remove` kills both — worth knowing, not worth changing), its `GetPlayerCount() == 0` early return (`:150`) and its QRF early return (`:153-155`). This feature migrates **group lifecycle**, not deployment decision-making.

The consequence, stated so it is not read as a bug later: with no players connected no new deployments are created, and during a QRF none are created either — and after [D2](#d2--one-consumer-seam-tower-garrisons-become-a-deployment-config) that now includes **tower garrisons**. Existing deployments' groups live entirely on the engine's lifecycle and are unaffected by both, which is a strict improvement over the old behaviour where the whole force also stopped being maintained.

### D10 — Vehicle crews register always-materialised

`spawnDistanceOverride` defaults to 100000 for vehicle crews, so they never go dormant, movement skips them (`IsSpawned()` is true — movement's D2), and live AI drives real roads. Three reasons this is the right call and a cheap one:

1. **It is parity.** Both shipped vehicle configs already ship `m_bEnableProximityActivation 0`, so their crews spawn once at deployment activation and are never deactivated today.
2. **It is bounded.** Both configs carry `m_iMaxInstances 1`, so the campaign-wide ceiling is one 2-man crew (`light_patrol`) plus one 4-man crew (`light_fireteam`) — six AI — and both deployments delete themselves when their route completes.
3. **The alternative is broken.** A dormant vehicle crew with a MOVE plan would be **walked in a straight line by the movement tick** while its vehicle — a consumer-owned entity core knows nothing about — stayed parked. The crew would materialise kilometres from its own truck.

The override is exposed as a module attribute so an operator can dial it down, and the cost is recorded in `context.md`.

### D11 — The stolen-vehicle guarantee is verified, not rebuilt

The old 40 m rule (`OVT_VehicleSpawningDeploymentModule.c:85-99`) exists because deactivation deleted everything and a stolen vehicle had to be spared. After the migration there is no deactivation, so the rule has nothing to protect against and is deleted. What replaces it is two things that already exist: **`UnregisterGroup` respects held members** and retires the group in place rather than deleting it out from under whatever is using it (`api.md` §3), and a **player-occupancy / player-ownership check** before deleting the vehicle entity in `OnCleanup`. The requirement says *verify rather than rebuild* — so T5.5 requires reading `SCR_AIGroup.HasHeldMember`'s actual semantics against the engine source and recording the verdict before the check is written.

### D12 — QRF no longer despawns tower garrisons — now moot by construction

*(user decision, 2026-08-17)*

Today `CheckRadioTowers` ANDs `!m_CurrentQRF` into its range test (`:559`), so **any** QRF anywhere on the map routes **every** tower into the despawn branch and deletes all tower garrisons for the QRF's duration. After [D2](#d2--one-consumer-seam-tower-garrisons-become-a-deployment-config) the entire branch is deleted, so the term does not survive to be argued about: migrated garrisons follow the engine proximity lifecycle, full stop. This matches the decision `civilians` made when it turned QRF-driven civilian despawn into an opt-in, and it removes a global side effect no player could have predicted.

The one QRF interaction that **does** survive is `EvaluateDeployments`' own early return (`:153-155`): garrison *creation* pauses while a QRF is active. Existing garrisons are unaffected. Documented in Phase 8, not fixed here ([D9](#d9--the-30-s-evaluator-and-its-guards-are-not-touched)).

### D13 — `OVT_EntitySpawningAPI.c` is deleted outright

All five live call sites of that file are the two deployment modules this feature rewrites (`OVT_InfantrySpawningDeploymentModule.c:84, :176, :373`; `OVT_VehicleSpawningDeploymentModule.c:101, :252`). Its other seven statics (`SpawnVehicle`, `SpawnSoldier`, `SpawnStaticObject`, `SpawnEntityBatch`, `CleanupEntity`, `CleanupEntityArray`, `ValidateSpawnPosition`) already have **zero** call sites anywhere in the tree. QRF does not use it — `OVT_QRFControllerComponent` carries its own kill-switch guard at `:369` and its own spawning. So the file becomes wholly unreferenced and goes, taking one `OVT-VIRT-PLAYTEST-ONLY` guard with it.

It also takes a documented defect with it: `CleanupGroup` deletes a group's soldiers but never the group entity or its detached waypoints — a leak this project has cited in five separate places. Deleting the file is the cheapest possible fix for it. The task is gated on a fresh grep, because a concurrent session could add a caller between planning and implementation.

### D14 — The observer API is entity-keyed, not handle-keyed

*(user decision, 2026-08-17)*

`AddEntityObserver(IEntity)` rather than `AddGroupObserver(int handle)`, because the first consumer is a **parked recruit group**, which is not a registered group at all — it is a player-owned `SCR_AIGroup` with `OVT_InactiveRecruitGroupComponent` on it. An entity-keyed API serves that, serves a future high-command group, and serves a self-observing registered group (pass `virt.GetGroup(handle)`) with one signature. Keying the internal map on `EntityID` also gives the stale-entity sweep somewhere to stand.

The engine call is `InsertObserverSP(key, 0, 0, entity)` — zero offsets, so the observer *is* the entity's position. It is **never** called with a null entity: that has zero vanilla script callers in the 1.8 tree and hard-froze the client the one time a test case tried it (`core/context.md` gotcha 0).

### D15 — The observer spike is a gate, not a formality

The requirement asks for server-side `InsertObserverSP` verification **before** anything is built on it, and it is right to. `InsertObserverSP` is documented as a *local* observer, has no vanilla script caller to copy, and the only null-entity insert in the whole 1.8 tree is the **MP** variant (`SCR_SpawnRequestComponent.c:541`). Whether an SP insert made on the authority is honoured by `ChimeraAIGroup.HasObserverInRange` — the call the group lifecycle actually uses — is not answerable by reading script.

T1.9 answers it with an Init-tier case in Phase 1, five phases before Phase 6 needs the answer. If the verdict is negative, Phase 6 re-plans onto `InsertObserverMP(identity, 0, 0, entity)`, which has a vanilla entity-following precedent (`SCR_SpawnRequestComponent.c:664`), and this plan is amended in place rather than discovered mid-build.

### D16 — Observers are wired to parked recruit groups, with an off-switch

The wiring lives in `OVT_InactiveRecruitGroupComponent` — the marker component that already owns a parked-recruit group's defend waypoint and its `OnDelete` cleanup. One component, one obvious lifetime (the group is created when a recruit is parked and deleted when the last one leaves or the owner logs off), no manager plumbing and no new subscription into the recruit manager's transfer paths.

An *active* recruit's body is in its owner's slave group, and the owner is a player — already an observer. The case the requirement actually names ("recruit group left in a town alone causes enemy patrol to materialise") is the **parked** one, and that is where the wiring goes.

The off-switch (`m_bRecruitGroupsAreObservers`, default true) exists because an observer pins every registered group inside its spawn ring materialised for as long as it lives: a squad parked near a tower holds that garrison awake indefinitely. That is the intended effect and the budget risk in one sentence, which is why it is a knob.

### D17 — Tower garrisons cost resources like any other deployment

*(user decision, 2026-08-17)*

The Tower Garrison config carries a real, non-zero `m_iBaseCost`/`m_iCostPerGroup`, debited from the faction deployments pool by `EvaluateFactionDeployments` (`:227-233`) like every other config, and its reinforcement rebuy flows through the standard `OVT_ReinforcementBehaviorDeploymentModule`. Cost 0 was explicitly rejected.

Two deliberate behaviour changes follow, and both go into Phase 8's player-facing sync rather than being buried:

- **A resource-starved occupying faction can leave towers ungarrisoned.** Today a garrison spawns unconditionally whenever a player is within 1750 m. The mitigations are `m_iPriority 1` (towers do not lose the resource race) and a modest cost; the exact numbers are a **play-test tuning question**, flagged as such in T4.5 rather than presented as derived.
- **Garrison creation pauses while a QRF is active or no players are connected** ([D9](#d9--the-30-s-evaluator-and-its-guards-are-not-touched)). Existing garrisons are unaffected.

The gameplay consequence worth naming: an ungarrisoned tower cannot be taken by *clearing its garrison*, because there is none. That is not new — today's capture path is likewise only reachable once a garrison has spawned (`:590-615` sits in the else-branch) — but it becomes more visible, and it is [R15](#9-risks--mitigation).

### D18 — Tower capture is driven by the eliminated flag, not by an empty list

Today the tower flips to the resistance from inside the despawn/reap bookkeeping (`:590-615`): the code drops garrison ids whose entity is gone or whose agent count is 0, and if the list ends up empty it captures. That is a side effect masquerading as a rule, and under 1.8 the agent-count test is outright wrong (a dormant or spawn-queued group reports zero agents).

The new shape is a small behaviour module (`OVT_RadioTowerCaptureBehaviorDeploymentModule`, [§3.3](#33-the-tower-garrison-config)) that watches `m_ParentDeployment.GetSpawnedUnitsEliminated()` and fires `ChangeRadioTowerControl` **once**, edge-latched. Because that flag is now set **only** from core's `GetOnGroupWiped` — which fires only when the survivor mask reports every slot dead ([§3.6](#36-wipe-accounting--the-eliminated-flag-becomes-wipe-driven)) — a proximity despawn can never capture a tower. That is stated as an acceptance criterion and asserted by T4.8, not assumed.

The reverse direction is declarative and needs no code: a resistance-held tower fails `OVT_RadioTowerControlConditionDeploymentModule.EvaluateCondition()`, the reinforcement module's `m_bDeleteOnConditionFail` deletes the deployment, and if the occupying faction ever retakes the tower the evaluator creates a new one.

### D19 — `RADIO_TOWER` is OR-ed in, not promoted in the precedence

`GetLocationTypeAtPosition` (`OVT_DeploymentManager.c:661-720`) returns a **single** flag and tests TOWN → BASE (500 m) → PORT → AIRFIELD → RADIO_TOWER (300 m) → CHECKPOINT, so a tower inside a town's bounds or within 500 m of a base classifies as TOWN or BASE and would never get a garrison.

Three fixes were considered. **Moving the RADIO_TOWER test to the front** would steal town centres within 300 m of a tower away from the Town Patrol config. **Returning a full union of every matching flag** is the model the enum actually implies (`UIWidgets.Flags`, and `CanUseLocationType` is already a bitwise test) but would newly let a town centre within 500 m of a base satisfy the BASE-only vehicle-patrol configs — a behaviour change nobody asked for. **Chosen:** compute today's single value unchanged, then `|=` `RADIO_TOWER` when within 300 m of a tower. No existing config's candidate acceptance moves, the new config gets what it needs, and the change is one statement.

### D20 — The tower kill-switch guard is removed by deleting the code it wraps

*(user decision, 2026-08-17)*

`OVT_OccupyingFactionManager.c:564`'s `OVT-VIRT-PLAYTEST-ONLY` guard is not un-guarded — the whole spawn block that contains it is deleted (T4.7). This matters for the ledger: an un-guarded line would revive legacy tower spawning alongside the new deployment config and double every garrison. The acceptance criterion is deliberately worded as *"the removed line is gone with its enclosing block, not un-commented"*, and Phase 8's ledger re-checks it.

### D21 — Nothing new replicates, and only one string is added to a save

No `Rpc`, no `RplProp`, no config-stream field, no `CONFIG_STREAM_VERSION` move, no new prefab, no UI. The only persistence change in the whole feature is one appended string on `OVT_DeploymentComponentSerializer` behind a version bump ([D6](#d6--the-deployment-key-is-derived-once-and-persisted)); tower persistence and the tower JIP stream are byte-identical before and after. `Rpc()` arity is a compile-check blind spot in this tree (BUG-090), so a feature with zero remote calls is the safest shape and the acceptance greps enforce it. The one new RPC *path* — tower capture — is `ChangeRadioTowerControl`'s existing broadcast, called from new code but not changed.

---

## 6. Definition of Done

An evaluator with **no implementation context** should be able to verify every item below.

### Functional Criteria

- **F1 — Dead members stay dead (the headline).** Find a town patrol, kill 2 of its 4 men, drive 3 km away, come back. **Expect:** exactly 2 men, in their own roles, at the patrol's position — not 4, and not the same 2 slots vanilla would have picked.
- **F2 — Dead groups stay dead.** Wipe one of a town patrol's groups entirely. Leave and return. **Expect:** that group is gone for good; the others are unaffected; the deployment is still active.
- **F3 — Patrols move while you are away.** Note where a town patrol is, leave the area for several minutes, come back. **Expect:** it is somewhere else on its 200 m perimeter route, on the ground, on land — and it resumes patrolling from there (it may walk back toward the start of its route first; that is movement's documented F-B, not a defect).
- **F4 — A wiped deployment force does not resurrect.** Wipe every group of a town patrol. Leave, return, wait out a 30 s evaluation cycle, then save → quit → **Continue**. **Expect:** no groups come back; the deployment marker survives; `OVT_PatrolHarassmentStabilityModifier` drops the harassment modifier from that town.
- **F5 — Tower garrison survivor accuracy.** Approach a garrisoned tower. Kill one man in one garrison group, drive 2 km away and return. **Expect:** exactly one survivor at the same post in that group, and the other group intact. **The garrison never wanders off the tower** — the DEFEND plan is the opt-out from virtual movement.
- **F6 — Proximity despawn is not death.** Drive far enough from a tower that its garrison despawns, then return. **Expect:** the tower is **still occupying-faction** — it was not captured — and the garrison materialises again at its post with the same strength.
- **F7 — Wiping a tower garrison flips the tower.** Kill every member of every garrison group at one tower. **Expect:** the tower flips to the resistance within one 10 s deployment tick, the capture notification fires **once**, and the map updates. Within a few minutes the Tower Garrison deployment for that tower is deleted by its own failed condition.
- **F8 — A QRF no longer clears the map's garrisons.** Trigger a QRF anywhere. **Expect:** tower garrisons elsewhere on the map are unaffected — this is [D12](#d12--qrf-no-longer-despawns-tower-garrisons--now-moot-by-construction)'s deliberate change, verified by *nothing happening*.
- **F9 — Old saves load.** Load a campaign saved **before** this feature. **Expect:** its town patrols are re-established from config; **tower garrisons appear as new deployments** within a few evaluation cycles; nothing is duplicated; a deployment that was already eliminated in that save stays eliminated.
- **F10 — No duplication across a Continue.** Save with several deployments live, quit to menu, **Continue**. **Expect:** the same number of groups, at their saved positions and strengths — not double. Repeat with a second campaign started in the same session: still no doubling (the restart-hygiene case core's Phase 6 found four bugs in).
- **F11 — Vehicle patrols drive and are stealable.** A light vehicle patrol drives its multi-town route with a seated crew. Kill the crew and take the vehicle; drive it away. **Expect:** the vehicle is yours and survives; when the deployment finishes or is deleted, your vehicle is **not** deleted out from under you.
- **F12 — Vehicle patrols complete and refund.** Let a vehicle patrol finish its route. **Expect:** the deployment deletes itself, the crew group is unregistered, and the faction resource pool goes **up** by half the invested cost — a non-zero number for the first time (T1.3).
- **F13 — Observers work.** Park a recruit squad near a tower and take your player character 3 km away. **Expect:** the tower garrison materialises around the parked squad and fights it. Set `m_bRecruitGroupsAreObservers = false`, restart, repeat: nothing materialises.
- **F14 — GM icons survive.** Open the GM view. **Expect:** town patrol, vehicle crew and tower garrison groups all carry icons with the `DEPLOYMENT` origin and their config name as the reason — **including while dormant** — and after a save/reload.
- **F15 — Reinforcement works.** Reduce a town patrol below strength and wait out the reinforcement module's cooldown. **Expect:** the missing group count is rebought (resources deducted), the eliminated flag clears, and the harassment modifier comes back. The same mechanism rebuys a partially-cleared tower garrison.
- **F16 — Tower garrisons are ordinary deployments.** In the GM panel, a Tower Garrison deployment appears in the deployment list with a non-zero invested-resources figure, exactly like a Town Patrol. → [D2](#d2--one-consumer-seam-tower-garrisons-become-a-deployment-config), [D17](#d17--tower-garrisons-cost-resources-like-any-other-deployment)

### Quality Criteria

- **Q1** `tools/compile-check.sh` exits **0** with no output.
- **Q2** Fast `{6A6E29FF47ECB840}` and All `{6A6E2A002F53A581}` both exit **0** at the end of every phase.
- **Q3** Every new test case carries a recorded proof that it can fail — the exact edit, in a preamble comment. **No `maxAttempts` anywhere.**
- **Q4 Nothing new replicates:** `grep -rn "Rpc\|RplProp\|Replication.Bump" Scripts/Game/GameMode/Deployments/` → empty, and the tower manager's RPC set is unchanged from before the feature.
- **Q5 The ad-hoc virtualization is gone:** `grep -rn "IsPlayerInRange\|DeactivateDeployment\|OVT_EntitySpawningAPI\|m_aProcessedGroups\|m_aWaypoints\|distance > 40" Scripts/Game/GameMode/Deployments/` → empty; `grep -n "PlayerInRange\|m_CurrentQRF" Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c` → no hit inside `CheckRadioTowers`; `OVT_RadioTowerData` has no `garrison` field.
- **Q6 Waypoints are core's:** `grep -rn "AddWaypoint\|RemoveWaypoint\|GetWaypoints\|GivePatrolWaypoints\|AIWaypointCycle" Scripts/Game/GameMode/Deployments/` → empty.
- **Q7 Net deletion:** `git diff --stat` shows more lines removed than added across `Scripts/Game/GameMode/Deployments/` and `OVT_OccupyingFactionManager.c` combined, the two new module classes notwithstanding.
- **Q8 Logic-tier grep clean:** no manager, game-mode, world or entity identifier in `OVT_DeploymentVirtualKey.c`, `OVT_VirtualPlanFactory.c` or their Logic-tier test file, **comments included**.
- **Q9 The kill-switch ledger balances:** `grep -rn "OVT-VIRT-PLAYTEST-ONLY" Scripts/` returns exactly the guards for systems this feature did not migrate (base upgrades ×2, QRF, the switch file), `context.md` accounts for every removed line, and the tower one was removed **with its enclosing block**.
- **Q10 Core is only extended:** `git diff Scripts/Game/GameMode/Virtualization/` shows only the four observer methods and their bookkeeping.
- **Q11 Config authoring is additive only:** `git diff Configs/` shows the new `Deployment_TowerGarrison.conf` (+ `.meta`) and one appended registry entry, and **nothing else**.

### Integration Criteria

- **I1 `api.md` ritual complete.** The four observer methods are in §3 and §10's `integration` table; the header names the fourth additive change; `core/context.md` carries the dated entry naming `virtualization/integration` and quoting the T1.9 verdict.
- **I2 The stability modifier is untouched and still works.** `git diff Scripts/Game/GameMode/Systems/Modifiers/Stability/OVT_PatrolHarassmentStabilityModifier.c` → **empty**, and F4/F15 exercise it live.
- **I3 The GM snapshot is untouched and now truthful.** `git diff Scripts/Game/GameMode/GM/` → **empty**; `m_iResourcesInvested` is non-zero for a freshly created deployment for the first time; the `RADIO_TOWER_GARRISON` enum member is **still declared** even though it has no producer (§3.7).
- **I4 The three shipped deployment configs are unchanged.** `git diff Configs/Deployment/Deployment_TownPatrol.conf Configs/Deployment/Deployment_VehiclePatrol_Light.conf Configs/Deployment/Deployment_VehiclePatrol_Heavy.conf` → **empty**.
- **I5 Tower persistence and JIP untouched.** `git diff` on `OVT_PersistedRadioTower`, the serializer's write/read order, `RplSave` and `RplLoad` → **empty**.
- **I6 Movement and civilians unaffected.** `git diff Scripts/Game/GameMode/VirtualMovement/` and `Scripts/Game/GameMode/Civilians/` → **empty**.
- **I7 One consumer seam.** `grep -rn "RegisterGroup(\|FindGroupsByOwner(\|GetOnGroupWiped(\|GetOnRecordsRestored(" Scripts/Game/` outside the tests and core returns hits **only** under `Scripts/Game/GameMode/Deployments/`. → [D2](#d2--one-consumer-seam-tower-garrisons-become-a-deployment-config)

### Verification Method

**Automated — from the repo root, in order:**

1. `tools/compile-check.sh` → exit **0**.
2. `tools/run-tests.sh "{6A6E29FF47ECB840}"` → exit **0** (Fast).
3. `tools/run-tests.sh "{6A6E2A002F53A581}"` → exit **0** (All).
4. `git diff Scripts/Game/GameMode/Virtualization/` → the four observer methods only. → Q10, I1
5. `grep -rn "IsPlayerInRange\|DeactivateDeployment\|OVT_EntitySpawningAPI\|m_aWaypoints\|distance > 40" Scripts/Game/GameMode/Deployments/` → **empty**. → Q5
6. `grep -rn "AddWaypoint\|RemoveWaypoint\|GivePatrolWaypoints" Scripts/Game/GameMode/Deployments/` → **empty**. → Q6
7. `grep -rn "RegisterGroup(\|FindGroupsByOwner(" Scripts/Game/ --include=*.c | grep -v Tests | grep -v Virtualization` → only `Deployments/`. → I7
8. `grep -rn "InsertObserverSP" Scripts/` → **one** call site, null-guarded. → D14
9. `grep -rn "OVT-VIRT-PLAYTEST-ONLY" Scripts/` → base upgrades ×2, QRF, switch file. → Q9
10. `git diff --stat Configs/` → two new files + one appended registry entry. → Q11, I4

**Manual — solo play-test.** Debug affordances to flip on `Prefabs/GameMode/OVT_OverthrowGameMode.et`: core's `m_bDebugRegisterTestGroup = false` (a consumer exists now — the debug group is only noise), movement's `m_bDebugMovementLogging = true` for step 3, and a temporary raise of `m_fVirtualSpeedMs` to 10 to compress step 3's window. The epic kill switch is **off for the migrated systems by construction** (their guarded code is gone); leave the file in place for the un-migrated ones.

1. Start a fresh campaign. Wait out a few 30 s evaluation cycles, then approach a town and a radio tower. **Expect:** a town patrol and a tower garrison materialise; the GM view shows both with `DEPLOYMENT` origins and their config names; the GM deployment list shows non-zero invested resources for each. → F14, F16
2. Kill 2 of 4 men in one patrol group. Drive 3 km away, wait ~30 s, drive back. **Expect:** exactly those 2 survivors, same roles. → F1
3. Note a patrol's position, leave for 5 minutes with movement logging on, come back. **Expect:** it is elsewhere on its perimeter, on land. Check the tower garrison too: **it has not moved at all.** → F3, F5
4. Wipe every group of the town-patrol deployment. Leave, return, wait two evaluation cycles. **Expect:** nothing comes back; the town's harassment modifier is gone. → F2, F4
5. At a tower: kill one garrison member, leave 2 km, return. **Expect:** one survivor at the post. Then leave far enough for the garrison to despawn and return: **the tower is still enemy-held.** → F5, F6
6. Kill the whole garrison. **Expect:** capture + one notification within ~10 s; a few minutes later the Tower Garrison deployment for that tower is gone from the GM list. → F7
7. Trigger a QRF somewhere else and check a distant garrisoned tower. **Expect:** its garrison is unaffected. → F8
8. Find a vehicle patrol. Watch it drive between towns with a seated crew. Kill the crew, take the vehicle, drive it 1 km, then let the deployment complete or delete it. **Expect:** your vehicle survives; the faction pool goes up on completion. → F11, F12
9. Park a recruit squad near a tower; fly/drive 3 km away. **Expect:** the garrison materialises and fights them. Flip `m_bRecruitGroupsAreObservers` off, restart, repeat: nothing. → F13
10. Save → quit → **Continue**. **Expect:** every patrol and garrison back at its saved position and strength, nothing duplicated. → F10
11. Load a campaign saved **before** this feature (keep one). **Expect:** town patrols re-established from config; tower garrisons created as new deployments within a few cycles; an already-eliminated deployment stays empty. → F9
12. Start a **second campaign in the same session** without restarting the client. **Expect:** no doubled registrations, no doubled ticks, no observers left over from the first campaign. → F10, Q10
13. **Resource-starvation check (tuning).** Watch a fresh campaign's first few minutes and record how long every tower takes to get a garrison, and whether any tower is left ungarrisoned while the pool is low. Feed the numbers back into T4.5's cost/priority tuning. → [D17](#d17--tower-garrisons-cost-resources-like-any-other-deployment), R15

---

## 7. Testing Strategy

**The automated spine covers the maths, the seams and the round trips. Everything about survivor accuracy in the live game, materialisation feel, the observer effect, resource pacing and multi-campaign hygiene is a play-test** — the suites cannot see two men standing where four used to be.

### Logic tier — `TestSuites/Logic/OVT_TEST_Logic_DeploymentVirtualization.c` (Fast, new file)

World-free assertions on the two statics of Phase 2, which is the entire reason they are statics:

- **Keys:** `DeriveKey` is deterministic for the same inputs and differs for a 1 m difference that rounds differently; sanitisation removes `@`/`#`/whitespace; an empty config name yields a usable key; `Disambiguate(k, 0)` is `k` and `Disambiguate(k, 2)` is not; `ModuleTag` prefers the name and falls back to the index; `OwnerKey` composes without ambiguity between `("a#b", "c")` and `("a", "b#c")`.
- **Missing count:** `MissingCount` never goes negative and answers 0 when held ≥ wanted.
- **Defend plan:** one position, one `DEFEND` type, one param, `m_bCycle` false — and the three arrays are the **same length** (a ragged plan is refused by `RegisterGroup`). This is the tower garrison's plan, so it is also the "garrisons never wander" claim at its root.
- **Perimeter plan:** exactly 8 entries; types alternate `PATROL, WAIT, PATROL, WAIT…`; the four patrol points are 90° apart at the requested radius from the centre; the first point's bearing matches the `fromPosition → centre` direction; each `WAIT` sits at the same position as the point before it and carries the supplied duration; `m_bCycle` is true.
- **Route plan:** `MOVE, WAIT` per stop; `returnToStart = true` appends one closing `MOVE` and sets `m_bCycle` false; `returnToStart = false` sets `m_bCycle` true and appends nothing; an empty stop list yields an empty (legal) plan, not a ragged one.

⚠ `vector.Distance` is +1 ULP off at 1000 m and 2000 m — every distance claim carries a tolerance or is taken at a probed-safe magnitude. ⚠ `out` and `owned` are reserved local names.

### Init tier — additions to `TestSuites/Init/OVT_TEST_InitSuite.c` (Fast)

⚠ **Init worlds never run `PostGameStart`** (movement's Gotcha 6) — a case that needs a tick must install it itself. ⚠ **The autotest camera is an observer** — every registration in these cases uses `spawnDistanceOverride = 0` (Manual policy, enforced since core's 2026-08-17 spawn guard) so nothing materialises mid-case.

- **The observer spike (Phase 1, the gate):** `HasObserverWithinRangeSq` false → `InsertObserverSP` with a **real entity** → true → `RemoveObserverSP` → false, with `GetObserversSP()` counts on both sides. Never a null entity.
- **The observer API (Phase 6):** null is a safe `false`; add/has/remove/count round-trip; double-add keeps the count at 1; removing an unknown entity is a safe `false`.
- **Config resolution:** `FindConfigByName("Town Patrol")` resolves and its patrol module builds a non-empty **cycling** plan; `FindConfigByName("Tower Garrison")` resolves, is `IsValidConfig()`, and its patrol module builds a **one-point `DEFEND`, non-cycling** plan; both vehicle configs resolve and their crew group names resolve to real compositions.
- **Location classification:** at a tower position, `GetLocationTypeAtPosition` includes the `RADIO_TOWER` bit **and** still includes whatever it returned before the change ([D19](#d19--radio_tower-is-ored-in-not-promoted-in-the-precedence)).
- **The `EnsureGroups` contract:** register under a deployment owner key → `FindGroupsByOwner` returns them → a second `EnsureGroups` adds **nothing** (idempotence) → unregister → the finder is empty.
- **Capture, and only on a real wipe:** the capture module does **not** fire for a deployment whose groups are registered but not wiped; it fires **exactly once** after deaths reported through `ReportMemberKilled` wipe the last group. Deaths go through the public seam (`api.md` §3), so no world combat is needed.
- **BUG-028 (best effort, T1.8):** dead ids are pruned out of the per-faction list. If the Init world cannot drive the manager, the verdict is recorded and the case dropped.

Every case unregisters and removes its observers **before** reporting.

### Persistence tier — `TestSuites/Persistence/OVT_TEST_PersistenceRoundTripSuite.c` (All)

Three new cases (T7.2–T7.4) plus one added claim (T7.5), all on the shared gate:

- a migrated deployment's five scalars **and its virtual key** survive save → dirty → re-apply, and the key is the same string rather than a re-derivation;
- a restored **eliminated** deployment registers zero groups (the flag-before-`InitializeDeployment` ordering, now with teeth);
- a **version-1** payload restores and derives a key on first use — the pre-feature-save path;
- a group registered under a deployment owner key is reclaimable by `FindGroupsByOwner` after the restore.

Member survival across a round trip is already asserted by core's two existing virtual-group cases; **do not duplicate them.**

⚠ **Fixture discipline (movement D12).** Any fixture registering a group with a MOVE/PATROL plan **will be walked** by the movement tick. New fixtures use null/DEFEND plans, or register and unregister inside one frame. T7.1 re-sweeps `grep -rn "RegisterGroup(" Scripts/Game/Tests/` and records a verdict per site before any case is written.

### Campaign tier

`OVT_TEST_Campaign_GMGroupRegistry` is un-guarded in T7.6 and asserts for real again, because this feature restores its producers. If it asserts specifically on `RADIO_TOWER_GARRISON`, re-point it at `DEPLOYMENT` (§3.7).

### Not automatable, and why

| Area | Why manual |
|---|---|
| Survivor accuracy in the live game | Needs real combat, real deaths and eyes on which roles came back |
| Materialisation quality (where the men appear) | A judgement about a place, not an assertion |
| The observer effect | Needs a parked squad, a distant player and enemy AI actually walking over |
| Resource pacing / how long a tower waits for a garrison | A tuning judgement; T4.5's numbers are a starting point, not a claim |
| Save → quit → **Continue** | The autotest harness restarts the suite on a world transition (`api.md` §8) |
| Two campaigns in one session | Same reason; core's Phase 6 found four teardown bugs exactly here |
| Vehicle patrol driving / stolen-vehicle survival | Needs roads, a route and a player in the driver's seat |
| MP / JIP | Uncovered by the whole spine; a dedicated-server pass is the only check |

---

## 8. Dependencies

**Hard preconditions (all satisfied today):**

- **`virtualization/core` complete and frozen.** `RegisterGroup`, `UnregisterGroup`, `FindGroupsByOwner`, `GetOnRecordsRestored`, `GetOnGroupWiped`, `GetAliveMemberCount`, `GetMemberAlive`, `ForceSpawn`/`ForceDespawn` and the Route B registry persistence are all live as of Phase 6. `api.md` §10's `integration` table is the contract.
- **The Manual-policy spawn guard** (core, 2026-08-17) — `spawnDistanceOverride = 0` really means "never materialise", which every Init case in this feature depends on.
- **`virtualization/movement` complete and play-tested.** Auto-adoption, the plan-is-the-opt-in contract and `GetCurrentPlanIndex` are live; migrated patrols walk while dormant instead of freezing, and DEFEND-plan garrisons demonstrably do not.
- **The deployments framework**, including the parts this feature leans on but does not change: the 30 s evaluator, candidate scoring, `GetRadioTowerPositions` (`:364-381` — **verified real and wired**), the 250 m same-name dedup (`:488`), `m_iMaxInstances` / `m_fChance` / `m_iPriority` handling (`:596-630`), the marker prefab (`SelfSpawn 1`, priority 35000) and both serializers.
- **`OVT_OccupyingFactionManager.ChangeRadioTowerControl` and `GetNearestRadioTower`** — both already public, both already called from outside the manager (`OVT_BaseUpgradeSpecops.c:65`), so the new capture module needs no new surface.
- **The faction group registry** already resolves `light_patrol` (2 slots), `light_fireteam` (4 slots) and `rifle_squad` (6 slots). T4.2 confirms `light_patrol` is the tower sentry composition for both shipped factions.
- **The epic kill switch** — the guards for **this feature's** systems are removed phase by phase; the rest stay until epic end.

**Explicitly NOT depended on:**

- **`virtualization/civilians`** — ambient sources are not registered groups. Different files, different `api.md` section, separate `context.md` notes.
- **QRF migration** — an epic-level exclusion. `OVT_QRFControllerComponent` keeps its own guard and its own spawning.
- **New resistance deployments or any client-visible marker** — out of scope by the requirements. The one new config is occupying-faction only.

**Downstream (what this unblocks):** `base-defense-migration`. Its planner should read [§3.3](#33-the-tower-garrison-config) **first** — the Tower Garrison config is the worked precedent for turning a bespoke garrison system into a deployment config — then [§3.2](#32-owner-system-and-owner-keys) (the owner-key scheme), [§3.5](#35-the-registrationreclaim-flow--one-idempotent-method-three-callers) (the idempotent `EnsureGroups` shape) and [D7](#d7--the-manager-subscribes-the-modules-never-do).

**User-side (interactive):** the §6 play-test list including the step-13 resource-pacing tuning pass, and a **dedicated-server / MP pass** — the automated spine covers no multiplayer at all, and this feature changes what materialises around every player.

---

## 9. Risks & Mitigation

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| **R1** | **The observer spike comes back negative** — server-side `InsertObserverSP` is not honoured by `HasObserverInRange`, and Phase 6's whole premise is wrong. | Medium | Phase 6 re-plans | T1.9 is a **Phase 1 gate**, five phases early, with a written verdict ([D15](#d15--the-observer-spike-is-a-gate-not-a-formality)). The fallback is `InsertObserverMP(identity, 0, 0, entity)`, which has a vanilla entity-following precedent. Nothing else in the feature depends on the answer. |
| **R2** | **A null-entity observer insert freezes the client.** | Low, catastrophic | Total client hang, no log | Exactly one `InsertObserverSP` call site in the tree, with a null refusal on the line above and an acceptance grep proving both. `AddEntityObserver(null)` has its own Init case, so the guard is asserted, not assumed. |
| **R3** | **Duplicate registrations on a continued campaign** — the classic `api.md` §10 migration trap: `PostGameStart` runs on an already-populated registry. | High if not designed against | Every patrol and garrison doubles, every load | `EnsureGroups` is written as *converge to `wanted`*, never *spawn `wanted`* ([§3.5](#35-the-registrationreclaim-flow--one-idempotent-method-three-callers)); every path checks `FindGroupsByOwner` first; idempotence has its own Init case and its own DoD item (F10). |
| **R4** | **BUG-175 — assigning a GM waypoint to a cycled group crashes** with recursive `Invoke`, and every Overthrow perimeter patrol is cycled. | Medium (GM-only) | VM exception in the GM editor | **Not made worse by this feature and partly reduced by it.** Consumers never author waypoints on a registered group ([D5](#d5--behaviour-modules-supply-plans-the-order-inverts), core D6), so no script path re-enters `OnWaypointAdded`. Neither shipped vehicle config produces a cycle at all, and the **new Tower Garrison config's DEFEND plan produces none either**. The town patrol keeps exactly today's exposure; the fix is parked with gm epic Phase 3. |
| **R5** | **Budget starvation** — an unstamped or wrongly-tiered group is capped at 0.50 of the AI budget and evicted first, so a garrison silently never appears on a busy server. | Medium | The migration looks broken and is invisible in a log | Every registration stamps a tier explicitly ([§3.9](#39-registration-parameters)): garrisons `HIGH` via the new `m_eImportance` attribute, patrols `NORMAL`, never `-1`-by-accident. **The `m_eImportance` attribute must be copied in `CloneModule`** or the Tower Garrison silently ships at `NORMAL` — [D1](#d1--in-place-module-rewrite-the-config-surface-of-the-three-shipped-configs-is-frozen)'s standing trap, called out in T3.2. |
| **R6** | **Observers pin too much awake** — a parked recruit squad holds a whole town's AI materialised for the rest of the campaign, and a GM free camera does the same wherever it flies. | Medium | Server frame time | Both documented in `context.md` and in Phase 8's wiki sync. The recruit wiring has an off-switch ([D16](#d16--observers-are-wired-to-parked-recruit-groups-with-an-off-switch)); the GM camera is an engine fact core already recorded (0a) and is an operator-facing note, not a code problem. |
| **R7** | **Movement walks a group it should not** — a plan authored "for later", or a new test fixture with a movable plan. | Medium | A garrison wanders off its post; a shared gate goes red | The §3.8 table pins the plan per config, and only the town patrol gets a movable one. T4.8 asserts the Tower Garrison plan is one-point DEFEND and non-cycling. T7.1 re-sweeps every `RegisterGroup(` site under `Scripts/Game/Tests/` with a recorded verdict (movement's D12 discipline). |
| **R8** | **Concurrent sessions move the tree** — every `file:line` here was verified on 2026-08-17 and bugfix sessions commit into the same branch. | High | Stale references, failed edits | Re-grep before editing; no task depends on a line number for correctness. Three gates already assume drift: T4.1 re-verifies the tower survey, T5.7 re-greps before deleting `OVT_EntitySpawningAPI.c`, and T1.2 re-checks the sibling unit mismatches. |
| **R9** | **The evaluator's guards surprise a play-tester** — `EvaluateDeployments` returns early with 0 players connected and during any QRF, so "no garrison appeared" reads as a migration bug. | Medium | Wasted debugging | [D9](#d9--the-30-s-evaluator-and-its-guards-are-not-touched) and [D17](#d17--tower-garrisons-cost-resources-like-any-other-deployment) state both guards explicitly; §6 step 1 waits out evaluation cycles rather than expecting instant creation; Phase 8 documents it for players. |
| **R10** | **A pre-feature save behaves oddly** — its markers exist, its groups do not, and its eliminated flags must be honoured. | Medium | Duplicated or resurrected forces | The v1-payload path is an explicit task (T2.5) with its own Persistence case (T7.4), and "eliminated stays eliminated" has its own case (T7.3) and DoD item (F9). Tower garrisons need **no** migration at all after [D2](#d2--one-consumer-seam-tower-garrisons-become-a-deployment-config) — the evaluator simply creates them. |
| **R11** | **Always-materialised vehicle crews cost more than expected.** | Low | Server frame time | Bounded by `m_iMaxInstances 1` per config — six AI campaign-wide — and both deployments delete themselves on route completion ([D10](#d10--vehicle-crews-register-always-materialised)). The override is a module attribute. |
| **R12** | **Crew seating never happens** — the engine fills a registered group progressively, and the old `GetOnInit()` hook only fires on a complete fill. | High if not changed | Vehicle patrols with crews standing beside their trucks | T5.4 replaces it with per-member `GetOnAgentAdded()`, exactly as `api.md` §3's "do not gate on `Event_OnInit`" warning prescribes. F11 is the live check. |
| **R13** | **Deleting a player's vehicle.** `OnCleanup` deletes the deployment's vehicles, and a stolen one must be spared. | Low-Medium | A player loses a vehicle they own — unrecoverable and very visible | [D11](#d11--the-stolen-vehicle-guarantee-is-verified-not-rebuilt): `UnregisterGroup` respects held members, and the vehicle delete is gated on player occupancy **and** ownership. T5.5 requires reading `HasHeldMember`'s real semantics first. |
| **R14** | **Tower persistence or JIP field order is disturbed** while editing a 1678-line manager. | Low | Corrupt saves, broken JIP for every client | T4.1 is a **read-only survey task that runs before any edit**, and I5 makes an empty diff on `OVT_PersistedRadioTower`/`RplSave`/`RplLoad`/`ApplyPersistedOccupyingFaction` an acceptance criterion. The manager edit is a **deletion** of `[NonSerialized]`, unread state — the safest shape available. |
| **R15** | **Resource starvation leaves towers ungarrisoned**, and an ungarrisoned tower cannot be taken by clearing its garrison — a progression stall. | Medium | Players cannot advance on towers | Accepted as a deliberate change ([D17](#d17--tower-garrisons-cost-resources-like-any-other-deployment)) with three mitigations: `m_iPriority 1` so towers win the resource race, a deliberately modest cost, and **§6 step 13 is a tuning pass whose numbers feed back into T4.5**. Note that today's capture path is likewise unreachable at an ungarrisoned tower, so this is a visibility change more than a new wall. Raise it with the user after the first play-test. |
| **R16** | **The tower kill-switch guard is un-guarded instead of deleted**, reviving legacy tower spawning alongside the new config. | Low | Every tower garrison doubles | [D20](#d20--the-tower-kill-switch-guard-is-removed-by-deleting-the-code-it-wraps) makes the deletion explicit, Phase 4's acceptance criterion words it as *"gone with its enclosing block, not un-commented"*, and Phase 8's ledger re-checks it. |
| **R17** | **The Tower Garrison config never produces a deployment** because a tower classifies as TOWN/BASE, or because `IsValidConfig`/`m_iMaxInstances`/`m_fChance` are mis-authored. | Medium | Silent — no garrison anywhere, no error | T4.6 fixes the classification surgically ([D19](#d19--radio_tower-is-ored-in-not-promoted-in-the-precedence)) and T4.8 asserts both the config's validity and the `RADIO_TOWER` bit at a tower position. `m_iMaxInstances` must be **-1**, not 1 — one-per-tower comes from the 250 m dedup, and T4.5 must verify the shipped Eden tower spacing exceeds 250 m. |

---

## Agent Routing Summary

| Phase | Agent | Advanced? |
|---|---|---|
| 1 — Inherited defects + observer spike (gate) | `component-developer` | no — five local fixes and one autotest case; registers nothing |
| 2 — Shared scaffolding (keys, plans, manager plumbing, serializer v2) | `component-developer-advanced` | **yes** — appends a field to a persisted binary payload |
| 3 — Town Patrol migration + proximity toggle retirement | `component-developer-advanced` | **yes** — retires a live subsystem the stability modifier reads |
| 4 — Tower garrisons become a deployment config | `component-developer-advanced` | **yes** — new campaign content plus a deletion inside replication-adjacent manager state |
| 5 — Vehicle patrols | `component-developer-advanced` | **yes** — crew seating, vehicle ownership, the stolen-vehicle guarantee |
| 6 — AI observers (core additive + recruit wiring) | `component-developer-advanced` | **yes** — edits the epic's frozen core and its contract documents |
| 7 — Save compatibility + persistence coverage | `component-developer-advanced` | **yes** — extends the shared All-group gate |
| 8 — Help & documentation sync | `help-docs-sync` | — |

**Skills to activate:** `enforcescript-patterns` (all phases), `overthrow-architecture` (2–7), `workbench-workflow` (3–6 — the config authoring and the play-tests).

**Estimate:** 47–68 h across the eight phases, of which Phases 3–5 are roughly half.
