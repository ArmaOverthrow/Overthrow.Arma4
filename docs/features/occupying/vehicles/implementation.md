# Vehicles — Implementation Plan

**Status:** Ready for Review
**Started:** 2026-08-23
**Target Completion:** TBD
**Last Updated:** 2026-08-23

**Epic:** `occupying` — see `docs/features/occupying/epic-overview.md`. This is **feature 7**, after `objectives` (6).
**Approach:** **A — subclass the insertion module for the mounted spine; the QRF stands up deployments instead of spawning vehicles itself.** See [D1](#d1--approach-a-subclass-the-insertion-module-rejected-b-and-c).
**Consumes:** `objectives` (plan/phase authoring, `OVT_SendDeploymentObjectiveOperation`, reserve-floor rules), `deployments` (module framework, pool, `m_iMinimumThreatLevel`, battle suppression), `qrf` (waves, scoring, siege ring), `core` (the global threat scalar, `m_aKnownTargets`).
**Branch:** `v1.5`. Concurrent sessions commit into this tree — **re-baseline before every phase.** Every claim below carries a `file:line` so drift is detectable.

> **What this feature is.** The occupying faction owns exactly two vehicle configs — `Deployment_VehiclePatrol_Light` (`m_fChance 2`) and `Deployment_VehiclePatrol_Heavy` (`m_fChance 1`, `m_iMinimumThreatLevel 30`) — so in practice the player fights an army that walks everywhere. AT launchers are dead weight, armour is never a threat, and the campaign's escalation is invisible on the ground.
>
> This feature gives the faction a **threat-tiered vehicle ladder** authored in the faction registry, one **mounted-force module** that drives an armed vehicle down real roads with the same play-tested convoy machinery insertions already use, and four callers for it: mounted harassment at the current objective, a QRF echelon that really drives to the battle, armour parked at bases that crews up when the base is attacked, and an armoured sweep that answers a vehicle loss. Nothing new replicates; the map and HUD are untouched.

---

## Corrections and confirmations against the working tree (verified 2026-08-23, branch `v1.5`, clean)

| # | Statement | Verified state |
|---|---|---|
| C1 | The insertion module must be edited to add a subclass hook | **It already has one.** `OnInsertionArrived(vector lzPosition)` (`OVT_InsertionSpawningDeploymentModule.c:1893`) is documented as *"THE HOOK A SUBCLASS OVERRIDES to do something at the drop point"* and is empty. `CompleteInsertion()` (`:1861`) is `protected` and overridable. |
| C2 | `OnUpdate` switches on `m_eInsertionState`, so a new state needs a `case` | **It is an if-chain** (`:1053-1080`) with three tests and no `default`. Appending `HOLDING` to `OVT_EInsertionState` falls through all three as a no-op in the parent, which is exactly what the subclass wants. |
| C3 | A mounted force at a battle will be pinned dormant by `OVT_DeploymentBattleSuppression` | **It is already exempt while mounted.** `SuppressForcesAroundBattle` skips any handle whose spawn distance is *strictly wider* than the world's global ring (`OVT_DeploymentManager.c:565-568`), and riding crews/passengers register at `RIDING_SPAWN_DISTANCE = 100000`. **No whitelist is needed — but the mounted module must NOT call `DropPassengersToGlobalRing()` on arrival**, or the exemption is thrown away. See [D8](#d8--battle-suppression-needs-no-whitelist-the-riding-ring-already-exempts-a-mounted-force). |
| C4 | The vehicle ladder can be authored as one config per rung, like harassment | It can, but the user's requirement puts it **in the faction vehicle registry**, and that is the right place: three deployment configs would need the ladder duplicated in each. The registry-delta trick (`overthrowDeployments.conf:47-79`, four harassment rungs from one file) is still used — for the *harassment* rung, not for the vehicle. |
| C5 | The deployment evaluator can be pointed at an arbitrary threat hotspot | **It cannot.** `CollectSeedCandidates` (`OVT_DeploymentManager.c:927-949`) and `FindDeploymentCandidates` only ever produce **named-location** positions (town / base / port / airfield / radio tower / checkpoint). A hunter-killer at a hotspot must be `ForceCreateDeployment`d by a deliberate caller. See [Phase 6](#phase-6--hunter-killer-armoured-sweep). |
| C6 | `ForceCreateDeployment` + a resources-invested figure is the safe way to fund a QRF echelon | **No.** `RecallDeployment` (`:2715-2731`) and `CollectDeployment` (`:2663-2691`) both credit `AddFactionResources` — the **pool** — while a QRF wave debits `m_iResources`, the **reserve**. An echelon funded from the wave budget and later collected would create money across two ledgers. See [D6](#d6--the-qrf-echelon-is-created-with-zero-invested-and-torn-down-with-deletedeployment). |
| C7 | USSR `heavy_armed` authors no `m_iCost` | Confirmed (`USSR_OverthrowData.conf:68-71`) — it takes the `defvalue: "50"` of `OVT_Faction.c:66`, so a BTR-70 costs the same as a UAZ-PKM plus 25. It also authors no `m_iMaxCapacity`, so it defaults to **4** against a hull with **10** positions. Both are fixed in Phase 1. |
| C8 | The Overthrow `BTR70.et` delta is a normal child prefab | It is a **same-GUID delta**: `Prefabs/Vehicles/Wheeled/BTR70/BTR70.et.meta` carries `{C012BB3488BEA0C2}`, the vanilla `BTR70.et` GUID, so the vanilla file is the real inheritance source and the header's `BTR70_Base.et` parent line is not the whole story. Any BRDM-2 / LAV-25 driving delta follows the same pattern (and the `Ural4320_transport.et` tuning recipe). |
| C9 | There is a "battle started" event to hang crew-up on | **There is not.** `m_OnQRFTownChanged` (`OVT_OccupyingFactionManager.c:301`) is town-only and deliberately silent for base QRFs (decision D6 of `counter-attacks`). Phase 5 adds one invoker published from both `StartBaseQRF` (`:1231`) and `StartTownQRF` (`:1278`). |

---

## 1. Executive Summary

Six things ship, in one dependency order, and they all sit on one new resolver and one new module.

1. **A threat-tiered vehicle ladder.** `OVT_FactionVehicleEntry` gains two fields — `m_sLadderRole` and `m_iMinThreat` — and one new difficulty number, `vehicleThresholdScale`, stretches or compresses every threshold (Easy ×2 … Insane ×0.25). A pure static picks the highest rung whose scaled threshold is under the live threat **and** whose cost fits the caller's budget. USSR runs UAZ-PKM → BRDM-2 → BTR-70; US runs M151-M2HB → M1025-M2HB → LAV-25. Every vehicle module can ask for a role instead of naming a prefab.
2. **`OVT_MountedForceSpawningDeploymentModule`**, a subclass of the 3,368-line insertion module. It inherits the whole play-tested convoy path — the LOD hold, the convoy cap, crew materialisation budgets, the stuck test and the five-way walk fallback — and changes three things: the vehicle is ladder-picked and armed, the force **stays aboard** on arrival, and the return leg is replaced by a **HOLDING** state in which a behaviour module owns the vehicle. A dead vehicle still drops the force onto the march, which is the spine, not the error handling.
3. **Mounted harassment**, appended as the **fifth rung** of the harassment ladder in both shipped plans: a mobile checkpoint parked on the main road into the objective at 150–300 m, crew dismounted to a checkpoint posture with the gunner still up, relocating between approaches every few minutes. It is an AT target by design.
4. **A QRF mounted echelon.** The QRF controller does not spawn vehicles. Per wave source it stands up a director-only deployment through the deployment manager with a slice of the wave budget; the vehicles **really drive** from that base by road, so they can be ambushed and they arrive late. They stop at a standoff ring, their crews count in zone scoring like any other occupying agent, and in siege mode they become road-blocking anchors of the encirclement.
5. **Crew-up on alarm.** `Deployment_BaseParkedVehicles` gains an armour sibling. The BTR sits in the compound where a player can scout it; when that base's battle starts, the garrison crews it and it sorties — by handing the parked hull to a mounted deployment rather than by growing a second crewing path.
6. **A hunter-killer sweep.** Losing a vehicle reports the position as a known target; the occupying faction's existing 60 s tick picks the hottest hotspot, buys one bounded armoured sweep, and collects it when the clock runs out.

**Three commitments shape everything below.**

- **One vehicle code path.** Harassment, the QRF echelon, the sortie and the sweep are four `.conf` files over one module. A second mounted implementation is a plan defect.
- **The accounting stays conserved.** The pool is debited exactly once per creation at the caller's own choke point; the QRF's single `m_iResources` debit outside the mode branch (BUG-027's shape) does not move, and the echelon joins `spent` rather than growing a second debit.
- **Nothing about the shipped insertion path is refactored.** `OVT_InsertionSpawningDeploymentModule` gets one appended enum value and no behavioural edit. It was play-tested green two days ago and the seven faults that got it there are documented in `objectives/context.md`.

**Expected shape:** ~9 new script files, 4 new `.conf` deployment configs (one registry entry each), 2 new prefab deltas, 2 faction-config edits, 6 difficulty-config edits, 1 new invoker, ~90 new lines in the QRF controller. Server-only throughout; **no GM wire change, no map or HUD change, no new replicated property**.

---

## 2. Goals

### Primary

- **G1 — Escalation the player can see.** At low threat the occupying faction fields armed jeeps; by mid-campaign BRDM-2s / M1025s; late, BTR-70s and LAV-25s. The rung is a pure function of `GetThreatFloat()` and one difficulty scalar, and adding a fourth rung is a faction-config edit.
- **G2 — AT has a job.** Every mounted deployment puts an armed vehicle within a player's reach on the ground, standing still often enough to be stalked: the checkpoint parks, the echelon holds a standoff ring, the sortie drives one leg, the sweep loiters.
- **G3 — One mounted spine.** `grep -rln "OVT_MountedForceSpawningDeploymentModule" Configs/Deployment/` returns **four** configs; no second class spawns a crewed armed vehicle for a deployment.
- **G4 — The walk fallback survives subclassing.** A mounted force whose vehicle is destroyed, refused a convoy slot, stuck, uncrewed or unbuildable still exists, still holds a plan pointing at its objective, and still walks.
- **G5 — Accounting is conserved.** `grep -rn "AddFactionResources" Scripts/Game/GameMode/Objectives/` stays **empty**. The QRF's one debit stays one debit and stays outside the mode branch. An echelon is created with zero invested and deleted, never collected or recalled ([D6](#d6--the-qrf-echelon-is-created-with-zero-invested-and-torn-down-with-deletedeployment)).
- **G6 — The insertion module is not refactored.** `git diff Scripts/Game/GameMode/Deployments/Modules/OVT_InsertionSpawningDeploymentModule.c` is **one appended enum value and nothing else** for the whole feature.
- **G7 — Nothing new replicates and nothing new persists.** Live vehicles are not saved, exactly as they are not saved today; a restored mounted deployment walks. No serializer version bump anywhere.

### Secondary

- **G8 — The existing vehicle modules get the ladder for free.** `OVT_ParkedVehicleSpawningDeploymentModule` and `OVT_VehicleSpawningDeploymentModule` gain one optional `m_sVehicleRole` attribute each; leaving it empty is byte-identical behaviour.
- **G9 — Shipped vehicle names are not renamed.** `light_armed` / `heavy_armed` keep their prefabs and their meanings, so `Deployment_VehiclePatrol_*` and `Deployment_BaseHeavyPatrol` are untouched. The ladder is expressed with new entries and a role tag.
- **G10 — The reachability gap is stated, not papered over.** The QRF echelon skips land-isolated sources and sources with no road within a sane radius, and the limitation is written into the epic's Tech Debt section rather than being implied by a helper name.

### Explicitly out of scope

- **No escorted supply convoys, no checkpoint vehicles, no show-of-force, no air insertion.** All four are recorded in [§10](#10-deferred--follow-ups).
- **No persistence of live vehicles or live convoys.** BUG-030's class is not reopened.
- **No new map icon, HUD element, notification or GM panel row.** A mounted force is discovered by looking at it.
- **No change to QRF scoring, wave sizing, the siege ring geometry or `SpendWholeBudgetInOnePass`** beyond the one echelon call and its contribution to `spent`.
- **No new AI behaviour trees.** Postures are waypoints and existing vanilla activities.
- **No rebalancing of infantry deployments.** Only the harassment ladder gains a rung.

---

## 3. Architecture Overview

### 3.1 Where each piece lives

```
Scripts/Game/Data/
└── OVT_VehicleLadderRules.c                     NEW  pure statics: scaled thresholds, rung pick, budget fit

Scripts/Game/Faction/
└── OVT_Faction.c                                TOUCH + m_sLadderRole, m_iMinThreat on OVT_FactionVehicleEntry
                                                       + OVT_FactionVehicleRegistry.ResolveLadderRung(...)
                                                       + OVT_Faction.ResolveVehicleForRole(...)

Scripts/Game/Configuration/
└── OVT_DifficultySettings.c                     TOUCH + float vehicleThresholdScale ("Occupying Faction")

Scripts/Game/GameMode/Deployments/Modules/
├── OVT_MountedForceSpawningDeploymentModule.c   NEW  : OVT_InsertionSpawningDeploymentModule   (P2)
├── OVT_MobileCheckpointBehaviorDeploymentModule.c NEW : OVT_BaseBehaviorDeploymentModule       (P3)
├── OVT_ArmouredSweepBehaviorDeploymentModule.c  NEW  : OVT_BaseBehaviorDeploymentModule        (P6)
├── OVT_CrewUpOnAlarmBehaviorDeploymentModule.c  NEW  : OVT_BaseBehaviorDeploymentModule        (P5)
├── OVT_InsertionSpawningDeploymentModule.c      TOUCH + HOLDING appended to OVT_EInsertionState  (one line)
├── OVT_ParkedVehicleSpawningDeploymentModule.c  TOUCH + m_sVehicleRole, + ReleaseVehicleOwnership()
└── OVT_VehicleSpawningDeploymentModule.c        TOUCH + m_sVehicleRole (opt-in; empty = today)

Scripts/Game/GameMode/Deployments/
└── OVT_DeploymentManager.c                      TOUCH + ForceCreateDeploymentFrom(..., sourcePosition)

Scripts/Game/Controllers/OccupyingFaction/
└── OVT_QRFControllerComponent.c                 TOUCH + SendMountedEchelon(), + echelon teardown on finish

Scripts/Game/GameMode/Managers/Factions/
└── OVT_OccupyingFactionManager.c                TOUCH + m_OnBattleStarted invoker (P5)
                                                       + ReportVehicleLoss(vector) (P6)
                                                       + TickHunterKiller() on the existing 60 s tick (P6)

Configs/Deployment/
├── Deployment_ObjectiveHarassment_Mounted.conf  NEW  (P3, director-only)
├── Deployment_QRFMountedEchelon.conf            NEW  (P4, director-only)
├── Deployment_BaseArmourSortie.conf             NEW  (P5, director-only)
├── Deployment_HunterKillerSweep.conf            NEW  (P6, director-only)
├── Deployment_BaseParkedVehicles.conf           (untouched — the armour sibling is a registry delta)
└── overthrowDeployments.conf                    TOUCH + 5 entries (4 new files + 1 parked-armour delta)

Configs/Objective/
├── Objective_TownOffensive.conf                 TOUCH + one ladder rung in the Harassment phase
└── Objective_BaseOffensive.conf                 TOUCH + one ladder rung in the Harassment phase

Configs/Factions/{USSR,US}_OverthrowData.conf    TOUCH + role/threshold on 2 entries each, + 1 new entry each,
                                                       + explicit cost/capacity on heavy_armed, + vehicle_crew group
Configs/Difficulty/Difficulty_*.conf  (6 files)  TOUCH + vehicleThresholdScale

Prefabs/Vehicles/Wheeled/
├── BRDM2/BRDM2.et (+ .meta, GUID {254289B9C09904AB})   NEW  same-GUID driving delta (Ural recipe)
└── LAV25/LAV25.et (+ .meta, GUID {0FBF8F010F81A4E5})   NEW  same-GUID driving delta (Ural recipe)

Scripts/Game/Tests/TestSuites/
├── Logic/OVT_TEST_Logic_VehicleLadder.c                 NEW
├── Init/OVT_TEST_Init_MountedForce.c                    NEW
├── Init/OVT_TEST_Init_QRFMountedEchelon.c               NEW
├── Init/OVT_TEST_Init_DeploymentBattleSuppression.c     TOUCH + the riding-ring exemption case
└── Logic/OVT_TEST_Logic_DeploymentBattleSuppression.c   TOUCH + 1 case

Language/localization_Overthrow.st                TOUCH ~4 keys (P7). ⚠ NEVER edit Configs/Language/*.conf
docs/features/occupying/epic-overview.md          TOUCH row 7, build order, reachability debt, rollup
```

**Reserved GUID series: `{6BC1….}`** — verified **0 hits in both trees** on 2026-08-23 (`grep -rl 6BC10000` over `Configs/ Prefabs/ Scripts/ Worlds/` and over `/mnt/n/Projects/Arma 4/ArmaReforger`). Allocation: `6BC10…` deployment configs and their modules, `6BC11…` faction-registry entries, `6BC12…` objective-config ladder edits, `6BC13…` prefab component instances, `6BC14…` spare. **Re-grep before authoring** — another session may have claimed it. **Inherited component GUIDs are copied, never minted** (the duplicate-`ActionsManagerComponent` trap).

### 3.2 The ladder — two layers, one of them pure

**`OVT_VehicleLadderRules`** (statics, no world, no manager, no `OVT_Global` identifier anywhere in the file or its test — the Logic-tier grep does not distinguish code from prose):

| Signature | Contract |
|---|---|
| `static float ScaledThreshold(int minThreat, float scale)` | `scale <= 0` → treat as `1`. `Math.Max(0, minThreat * scale)`. |
| `static bool RungUnlocked(int minThreat, float scale, float threat)` | `threat >= ScaledThreshold(...)`. A rung of `0` is always unlocked, including at `threat == 0`. |
| `static bool RungAffordable(int cost, int budget)` | `budget < 0` → true (unbounded). Else `cost <= budget`. |
| `static int PickRung(notnull array<int> minThreats, notnull array<int> costs, float scale, float threat, int budget)` | The index of the **highest** `minThreat` that is both unlocked and affordable. Ties break to the **lowest index** (author order). `-1` when nothing qualifies — a real answer, not an error. |

**`OVT_FactionVehicleRegistry.ResolveLadderRung(string role, float threat, float scale, int budget, out OVT_FactionVehicleEntry entry)`** filters `m_aVehicleEntries` to `m_sLadderRole == role`, hands the two parallel arrays to `PickRung`, and returns false when the role is unauthored or nothing fits. `OVT_Faction.ResolveVehicleForRole(...)` is the null-safe wrapper every module calls, mirroring `GetVehiclePrefabByName` (`OVT_Faction.c:648`).

**The authored rungs** (all role `armed`; existing names are **not** renamed — G9):

| Faction | Entry | Prefab | `m_iMinThreat` | `m_iCost` | crew / capacity |
|---|---|---|---:|---:|---|
| USSR | `light_armed` | `{0B4DEA8078B78A9B}…UAZ469/UAZ469_PKM.et` | 0 | 25 | 2 / 3 |
| USSR | `medium_armed` **NEW** | `{254289B9C09904AB}…BRDM2/BRDM2.et` | 400 | 70 | 2 / 4 |
| USSR | `heavy_armed` | `{C012BB3488BEA0C2}…BTR70/BTR70.et` | 900 | **120** (was default 50) | 3 / **10** (was default 4) |
| US | `light_armed` | `{F6B23D17D5067C11}…M151A2/M151A2_M2HB.et` | 0 | 25 | 2 / 3 |
| US | `heavy_armed` | `{3EA6F47D95867114}…M998/M1025_armed_M2HB.et` | 400 | **70** (was default 50) | 3 / 4 |
| US | `heavy_armor` **NEW** | `{0FBF8F010F81A4E5}…LAV25/LAV25.et` | 900 | 120 | 3 / 9 |

⚠ The name asymmetry (`medium_armed` for USSR, `heavy_armor` for US) is deliberate and is the price of not renaming `heavy_armed`, which three shipped configs consume. **The ladder is resolved by role and threshold, never by name.**

`vehicleThresholdScale`: Easy **2.0**, Normal **1.0**, Hard **0.5**, Extreme **0.35**, Insane **0.25**, TestWorld **1.0** (explicit, for determinism). So a BTR-70 appears at threat 1800 on Easy and 225 on Insane.

### 3.3 The mounted module

`OVT_MountedForceSpawningDeploymentModule : OVT_InsertionSpawningDeploymentModule`. It inherits everything and overrides five things.

| Inherited behaviour | Mounted change |
|---|---|
| `GetVehiclePrefabFromFaction(factionIndex)` (`:3219`) | Overridden: ladder-resolve `m_sVehicleRole` against live threat, `vehicleThresholdScale` and the module's own vehicle budget (the inherited `m_iTruckCostOverride`). Falls back to `m_sTruckVehicleType` when the ladder answers nothing, so the walk-vs-drive decision never depends on the ladder succeeding. |
| `CompleteInsertion()` (`:1861`) | Overridden: **no disembark, no `DropPassengersToGlobalRing()`** (C3), release the convoy reservation, enter `HOLDING`, call `OnInsertionArrived(m_vLZ)`. The vehicle is not sent home. |
| `OnUpdate` (`:1053`) | Overridden: `super.OnUpdate(deltaTime)` first (the abandoned-truck sweep and UNDECIDED retry still run), then a `HOLDING` branch that watches for vehicle loss and crew death and falls back to `DismountAndWalk()`. |
| `OnCleanup` / `ReleaseConvoy` | Unchanged. The vehicle is still owned, still deleted at teardown unless a player claimed it, and the reservation is still released on every exit. |
| `GetSpawnedEntities()` (`:3115`) | Unchanged — the vehicle is already reported, so the GM and the deployment teardown already see it. |

**New authored attributes** (four; every one copied in `CloneModule()`, which repeats the parent's ten *and* the grandparent's thirteen — the shipped clone at `:3304-3337` is the template):

| Attribute | Default | Meaning |
|---|---|---|
| `string m_sVehicleRole` | `"armed"` | Ladder role in the faction VEHICLE registry. Empty falls back to `m_sTruckVehicleType`. |
| `bool m_bDismountOnArrival` | `0` | `1` reproduces the insertion module's behaviour exactly (drop and walk), for a config that wants a lift rather than a fighting vehicle. |
| `int m_iHoldTicks` | `0` | Update ticks in `HOLDING` before the deployment requests collection. `0` = indefinite (the checkpoint and the echelon); the sweep authors a bound. |
| `bool m_bAdoptExistingVehicle` | `0` | Phase 5 only: instead of spawning, take the vehicle handed in by `AdoptVehicle()` before the first convergence. |

**New public seams:** `Vehicle GetMountedVehicle()` (behaviour modules), `void SetSourceOverride(vector)` (the QRF and the sortie), `bool AdoptVehicle(Vehicle)` (the sortie), `OVT_EInsertionState GetInsertionState()` (already public at `:3262`).

⚠ **`m_iTruckCostOverride` is the vehicle budget, and it is still a budget and not a receipt.** A price is computed from the config *template* before any deployment exists, so the faction is unknown and the rung cannot be resolved at pricing time. The module therefore charges the authored figure and then refuses any rung dearer than it — which is what makes the escalation cost money: a config that wants a BTR-70 has to author ≥ 120.

### 3.4 Data flow

```
OVT_OccupyingFactionManager.GetThreatFloat()      OVT_DifficultySettings.vehicleThresholdScale
                    │                                            │
                    └──────────────┬─────────────────────────────┘
                                   ▼
                    OVT_VehicleLadderRules.PickRung()          ← pure, Logic tier
                                   ▲
              m_sLadderRole / m_iMinThreat / m_iCost
              Configs/Factions/{USSR,US}_OverthrowData.conf
                                   │
                                   ▼
                 OVT_Faction.ResolveVehicleForRole(role, threat, scale, budget)
                                   │
        ┌──────────────────────────┼──────────────────────────┐
        ▼                          ▼                          ▼
OVT_MountedForceSpawning…   OVT_ParkedVehicleSpawning…   OVT_VehicleSpawning…
   (P2 — drives, holds)        (P5 — telegraphed)          (opt-in role, else today)
        │
        │  GetMountedVehicle()
        ▼
  ┌─────┴──────────────┬─────────────────────┬──────────────────────┐
  ▼                    ▼                     ▼                      ▼
MobileCheckpoint    (echelon holds       ArmouredSweep         CrewUpOnAlarm
  Behavior (P3)      the standoff)        Behavior (P6)         Behavior (P5)
  ▲                    ▲                     ▲                      ▲
  │ authored in        │                     │                      │
Deployment_Objective  Deployment_QRF      Deployment_Hunter    Deployment_Base
Harassment_Mounted    MountedEchelon      KillerSweep          ParkedVehicles(armour)
  ▲                    ▲                     ▲                      │
  │ 5th ladder rung    │ SendWave() slice    │ OF 60 s tick         │ m_OnBattleStarted
objective director   OVT_QRFController    OVT_OccupyingFaction   ──▶ Deployment_BaseArmourSortie
                                          Manager (known targets)     (adopts the parked hull)
```

### 3.5 The QRF seam

`SendWave()` (`OVT_QRFControllerComponent.c:747`) keeps its shape exactly. Inside the STANDARD per-source loop and inside `SpendWholeBudgetInOnePass`'s inner loop, after the infantry allocation for that source, one new call:

```
allocated += SendMountedEchelon(base, qrfpos, allocate - allocated);
```

`SendMountedEchelon` returns **what it committed**, so it flows into `spent`, into `m_iResourcesLeft`, and into the single `m_OccupyingFaction.m_iResources` debit that already lives outside the mode branch. **No second debit, no second clamp.** It refuses (returning 0, silently) when: the source is land-isolated, no road is within `ECHELON_ROAD_SEARCH_M` of the source or of the standoff point, the ladder answers nothing inside the remaining slice, a per-battle echelon cap is reached, or the deployment manager refuses the creation.

**Reachability stance.** The epic records that no A→B land-reachability query exists anywhere (`epic-overview.md`, *Land reachability*). This feature does not invent one. It uses the two statements that do exist — the authored `landIsolated` flag on the base record (`OVT_OccupyingFactionManager.c:1553`) and `OVT_WorldUtils.FindNearestRoadSpawn(center, maxDistance, out position, out angles)` (`OVT_WorldUtils.c:259`) — and records in the epic's Tech Debt that a source with a road at both ends may still be unreachable across water, in which case the crew stalls, the stuck test fires and **the force walks**. That is a degradation, not a failure.

### 3.6 Persistence stance

**Nothing new is persisted.** Vehicles are not saved today; a convoy is never resumed across a load (`OVT_InsertionSpawningDeploymentModule.c` class header). A restored mounted deployment walks its force in, exactly as a restored insertion does. The QRF echelon and the armour sortie are both tied to a live battle, and a live battle is deliberately rolled back on load. No serializer changes, no version bumps, no new save records. The one durable piece of state — a base's parked armour — is already covered by the deployment component serializer, which restores the *deployment*, and the parked module re-buys its vehicle on the restored deployment exactly as it does now.

---

## 4. Implementation Phases

Seven phases. **Every phase leaves the tree compiling, the suites runnable and the campaign playable.** No phase ships a module without the config that authors it; no phase ships a config whose modules do not exist.

**Test-run policy:** `tools/compile-check.sh` runs freely. **`tools/run-tests.sh` is run by the orchestrator only, once, after a phase completes** — never during planning, never in a subagent. It launches a real Reforger client that steals desktop focus. See `.claude/test-policy.md`. Fast `{6A6E29FF47ECB840}`, All `{6A6E2A002F53A581}`.

---

### Phase 1 — The ladder: registry fields, difficulty scalar, pure resolver

**Agent:** `component-developer` — standard. Additive data plumbing with a pure spine; nothing changes shape.
**Estimate:** 6–9 h · **Suite after this phase:** **All** (it edits a difficulty preset that the Init tier reads).

**Tasks**

1. **T1.1 — `OVT_VehicleLadderRules`** in `Scripts/Game/Data/`, the four statics of [§3.2](#32-the-ladder--two-layers-one-of-them-pure). ⚠ No `OVT_Global`, `GetGame()`, `World` or `Entity` identifier anywhere in the file, **comments included**. ⚠ `out` and `owned` are reserved local names.
2. **T1.2 — Two fields on `OVT_FactionVehicleEntry`** (`OVT_Faction.c:58-80`): `string m_sLadderRole` (empty default — an unauthored entry is on no ladder) and `int m_iMinThreat` (0). Both with a `desc:` a tuner can act on.
3. **T1.3 — `OVT_FactionVehicleRegistry.ResolveLadderRung(...)` and `OVT_Faction.ResolveVehicleForRole(...)`**, in the shape of `GetVehiclePrefabByName` (`:648-656`) — null-safe, returning false rather than an empty `ResourceName` so "no rung" and "bad prefab" are distinguishable.
4. **T1.4 — `float vehicleThresholdScale`** on `OVT_DifficultySettings`, category `"Occupying Faction"`, `defvalue "1"`, with a `desc:` naming the Easy…Insane spread. Follow exactly how the twelve `objective*` fields were added (`:81-104`).
5. **T1.5 — Author the six difficulty presets** with the values in §3.2. ⚠ `Difficulty_TestWorld.conf` gets an explicit `1` so no test depends on a defvalue.
6. **T1.6 — Author both faction registries** per the §3.2 table: role + threshold on the four existing armed entries, one new entry each, **explicit `m_iCost` and `m_iMaxCapacity` on both `heavy_armed` entries** (C7). New entry GUIDs from `{6BC11…}`.
7. **T1.7 — Two prefab deltas**, `Prefabs/Vehicles/Wheeled/BRDM2/BRDM2.et` and `LAV25/LAV25.et`, each a **same-GUID delta** (`.meta` `Name` = the vanilla GUID, C8) carrying only the `AICarMovementComponent` tuning proven on the Ural: `FrictionCoefficient 0.1`, `MaxReverseTravelDistance 30`, `"Min Prediction Distance" 2`. Hand-authored GUIDs are fine; copy the inherited component GUIDs, never mint them.
8. **T1.8 — `OVT_TEST_Logic_VehicleLadder.c`:** `ScaledThreshold` for scale 0 / negative / 2.0 / 0.25; `RungUnlocked` at exactly the threshold (**unlocked** — `>=`) and one below; `RungAffordable` with `budget -1`; `PickRung` picking the top rung, dropping to the middle when the top is unaffordable, dropping to the bottom when threat is low, returning `-1` on an empty array and on all-locked, and breaking a tie to the lowest index. Floats compare with `OVT_TEST_LogicFixture.EPSILON`.
9. **T1.9 — Init case** (append to `OVT_TEST_Init_VehiclePriceSpecificity.c` or a new small file): both shipped faction registries resolve role `armed` to three distinct rungs, and `ResolveVehicleForRole` answers false for an unknown role.
10. **T1.10 — `context.md`:** the rung table as authored, the name-asymmetry rationale, and the two capacity/cost corrections with their before values.

**Acceptance criteria**

- `compile-check.sh` exit **0**; **All** green.
- `grep -rn "OVT_Global\|GetGame()\|World\|Entity" Scripts/Game/Data/OVT_VehicleLadderRules.c` → **empty, comments included**.
- `grep -c "vehicleThresholdScale" Configs/Difficulty/*.conf` → **1 in each of the six files**.
- `grep -c "m_sLadderRole \"armed\"" Configs/Factions/USSR_OverthrowData.conf Configs/Factions/US_OverthrowData.conf` → **3 each**.
- `git diff Configs/Deployment/ Scripts/Game/GameMode/` → **empty** (this phase touches neither).
- Workbench: open both faction `.conf`s — every vehicle entry expands with the two new fields resolved, and no attribute shows unresolved.

---

### Phase 2 — The mounted-force module

**Agent:** `component-developer-advanced` — **advanced.** It subclasses the tree's most intricate module, changes a lifecycle terminal state, and its correctness argument is about ownership and LOD rather than about arithmetic.
**Estimate:** 14–20 h · **Suite after this phase:** **All**.

**Tasks**

1. **T2.1 — Read-only survey.** Re-verify against the working tree: `OnInsertionArrived` (`:1893`), `CompleteInsertion` (`:1861`), `OnUpdate` (`:1053`), `GetVehiclePrefabFromFaction` (`:3219`), `CloneModule` (`:3304`), `ReleaseConvoy` (`:1956`), `DropPassengersToGlobalRing` (`:3052`), and the suppression skip at `OVT_DeploymentManager.c:565-568`. If a concurrent session moved any of them, stop and re-baseline.
2. **T2.2 — Append `HOLDING` to `OVT_EInsertionState`** with a one-line doc comment. ⚠ **This is the only permitted edit to the insertion module for the whole feature** (G6). It is safe because the enum is runtime-only — no convoy is resumed across a load, so no save carries a value.
3. **T2.3 — The module class** with the four attributes of [§3.3](#33-the-mounted-module) and a header that states, in the file's own idiom: what it owns (the vehicle, the crew registration, the waypoints, the convoy slot), what it borrows, that the walk fallback is still the spine, and that **the passengers must never be dropped to the global ring** because the riding ring is what exempts them from battle suppression.
4. **T2.4 — `GetVehiclePrefabFromFaction` override.** Ladder-resolve; on `false`, log once at NORMAL naming the role and the threat, and fall back to `m_sTruckVehicleType`.
5. **T2.5 — `CompleteInsertion` override** per §3.3. ⚠ `ReleaseReservation()` must still be called — a leaked convoy slot is permanent and eventually stops the faction driving anywhere.
6. **T2.6 — `OnUpdate` override** with the `HOLDING` branch: vehicle destroyed or crew dead → `DismountAndWalk("its vehicle was destroyed")`; `m_iHoldTicks` expired → `RequestDeploymentCollection`. `super.OnUpdate` is called first, unconditionally.
7. **T2.7 — `CloneModule()`** copying **all 27 fields** (13 grandparent + 10 parent + 4 own). ⚠ The parent's own header names what a dropped line costs; add the same sentence for the four new ones. This is the single highest-frequency defect in this module system.
8. **T2.8 — The three public seams** (`GetMountedVehicle`, `SetSourceOverride`, `AdoptVehicle`) plus `EnsureSourceResolved` preferring the override. `AdoptVehicle` is inert until Phase 5 authors `m_bAdoptExistingVehicle`; ship it now so Phase 5 is a config change plus a behaviour module.
9. **T2.9 — `ForceCreateDeploymentFrom(config, position, factionIndex, sourcePosition, …)`** on the deployment manager: create, then walk the created component's **runtime** modules and call `SetSourceOverride` on every mounted one **before the first convergence**. ⚠ Verify `CreateDeployment`'s activation ordering (`OVT_DeploymentManager.c:1868`) — if activation is synchronous with creation, the override must be applied inside `CreateDeployment` rather than after it. Record the answer in `context.md`.
10. **T2.10 — `OVT_TEST_Init_MountedForce.c`:** clone fidelity with 27 distinct non-default values and a consequence-naming `SetFailure`; the ladder override picks the expected rung for a planted threat and falls back with the role unauthored; `CompleteInsertion` leaves the state `HOLDING`, the passengers seated and the reservation released; a destroyed vehicle in `HOLDING` reaches `WALKING`; `SetSourceOverride` wins over the authored provider. ⚠ Deployment fixtures must be `SetSpawnedUnitsEliminated(true)` on the deployment **and every spawning module** before anything ticks — the autotest camera is an observer.
11. **T2.11 — Battle-suppression case** appended to `OVT_TEST_Init_DeploymentBattleSuppression.c`: a group registered at `RIDING_SPAWN_DISTANCE` inside the battle circle is **not** pinned. This is the mechanical guard on C3.
12. **T2.12 — `context.md`:** the ownership split for a mounted force, the reason `DropPassengersToGlobalRing` is not called, the activation-ordering answer from T2.9, and every can-fail proof.

**Acceptance criteria**

- compile **0**; **All** green.
- `git diff Scripts/Game/GameMode/Deployments/Modules/OVT_InsertionSpawningDeploymentModule.c` → **one appended enum value and its comment, nothing else**.
- `grep -n "DropPassengersToGlobalRing" Scripts/Game/GameMode/Deployments/Modules/OVT_MountedForceSpawningDeploymentModule.c` → **empty**.
- `grep -c "clone\." Scripts/Game/GameMode/Deployments/Modules/OVT_MountedForceSpawningDeploymentModule.c` → **≥ 27**.
- `git diff Configs/` → **empty** (no config authors this module yet).

---

### Phase 3 — Mounted harassment and the mobile checkpoint

**Agent:** `component-developer-advanced` — **advanced.** It edits both shipped objective plans and introduces the first behaviour module that owns a vehicle.
**Estimate:** 10–14 h · **Suite after this phase:** **All**.

**Tasks**

1. **T3.1 — `OVT_MobileCheckpointBehaviorDeploymentModule`** (`: OVT_BaseBehaviorDeploymentModule`). Attributes: `m_fApproachMinDistance` (150), `m_fApproachMaxDistance` (300), `m_fRoadSearchRadius` (120), `m_iRelocateMinutes` (8), `m_fCheckpointSpread` (25). On activate: find the mounted module through `m_ParentDeployment.GetSpawningModules()`, take `GetMountedVehicle()`, pick a road point on the approach to the objective with `OVT_WorldUtils.FindNearestRoadSpawn`, park the vehicle there facing along the road segment, dismount the **infantry** to a small perimeter and leave the **crew** aboard so the gun stays manned. On the relocate clock: pick a different approach bearing and drive to it. ⚠ Bearing selection must **exclude the last one used** so a two-road town does not oscillate.
2. **T3.2 — Approach selection.** Sample N bearings around the objective at the authored band, keep those with a road inside `m_fRoadSearchRadius`, and choose uniformly at random among them. ⚠ `RandInt` is **max-exclusive**. ⚠ `array.Remove()` is swap-with-last; use `RemoveOrdered` if bearing order matters.
3. **T3.3 — `Deployment_ObjectiveHarassment_Mounted.conf`** (`{6BC10…}`), modelled line-for-line on `Deployment_ObjectiveHarassment.conf`: the mounted module (role `armed`, `m_sGroupType "light_fireteam"`, `m_iTruckCostOverride 70`, `m_fLZStandoffDistance 250`, `m_Source OVT_ObjectiveAnchorSourceProvider`), the checkpoint behaviour, `OVT_ReinforcementBehaviorDeploymentModule` and `OVT_ObjectiveConditionDeploymentModule` with **`m_sFromPhase "Harassment"` / `m_sThroughPhase "ForwardBase"`** — the phase *range* is what stops the phase-3 deadlock and must not collapse to an equality. `m_bDirectorOnly 1`, `m_iBaseCost 45`, `m_iAllowedLocationTypes TOWN|BASE`.
4. **T3.4 — Register it** in `overthrowDeployments.conf` as `"Objective Harassment (Mounted)"`.
5. **T3.5 — Wire both plans.** Append the rung to the `m_aLadder` of the harassment operation in **four** places: the Harassment phase and the ForwardBase phase of `Objective_TownOffensive.conf`, and the same two in `Objective_BaseOffensive.conf`. ⚠ Ladder order **is** the ramp; the rung goes last so it saturates. ⚠ `.conf` files cannot carry comments — the ordering contract is documented in the module headers.
6. **T3.6 — Init cases** (extend `OVT_TEST_Init_ObjectiveOperations.c`): the ladder now has five rungs and rung 5 resolves to the mounted config; the checkpoint module clones completely; the approach chooser refuses a bearing with no road and never returns the previous bearing twice in a row.
7. **T3.7 — Logic case** for the pure part of approach selection if it can be extracted (bearing arithmetic, band clamping, previous-bearing exclusion) — no world identifiers.
8. **T3.8 — Play-test checklist** written into `context.md` before the phase closes, so the orchestrator has a script (see [§6](#6-definition-of-done) steps 3–5).

**Acceptance criteria**

- compile **0**; **All** green.
- `grep -c "Objective Harassment (Mounted)" Configs/Objective/*.conf` → **4** (two phases × two plans).
- `grep -n "m_sThroughPhase" Configs/Deployment/Deployment_ObjectiveHarassment_Mounted.conf` → `"ForwardBase"`.
- `git diff Configs/Deployment/Deployment_ObjectiveHarassment.conf` → **empty** (the infantry rungs are untouched).
- Workbench: `overthrowDeployments.conf` loads with **23** entries and the new one expands with its four modules.

---

### Phase 4 — QRF mounted echelon

**Agent:** `component-developer-advanced` — **advanced (max effort).** It edits the file that resolves every battle, touches the one debit that BUG-027 was about, adds a reachability gate the epic says does not exist, and has to behave in both QRF modes.
**Estimate:** 16–22 h · **Suite after this phase:** **All**.

**Tasks**

1. **T4.1 — Read-only survey** of `OVT_QRFControllerComponent.c`: `SendWave` (`:747`), `SpendWholeBudgetInOnePass` (`:831`), `SpawnTroops` (`:875`), `GetLandingZone` (`:1074`), `BuildSiegeRing` (`:707`), the four index-parallel spawn arrays (`:31-40`), the scoring loop (`:468-596`) and `m_OnFinished`. Confirm the single debit is still at `:800-806` and still outside the mode branch.
2. **T4.2 — `Deployment_QRFMountedEchelon.conf`** (`{6BC10…}`): the mounted module (role `armed`, `m_sGroupType "light_fireteam"`, `m_iTruckCostOverride 90`, `m_fLZStandoffDistance 0` — the deployment position **is** the standoff point), the mobile-checkpoint behaviour with `m_iRelocateMinutes 0` (park, do not roam), and **no** reinforcement module. `m_bDirectorOnly 1`, `m_iMaxInstances -1`, `m_iAllowedLocationTypes` all.
3. **T4.3 — `SendMountedEchelon(vector source, vector target, int budget)`** on the QRF controller. In order: cap check (`m_iEchelonsSent < ECHELON_CAP_PER_BATTLE`, 2); **reachability** — `GetNearestBase(source).landIsolated` false **and** `OVT_WorldUtils.FindNearestRoadSpawn(source, ECHELON_ROAD_SEARCH_M, …)` true; ladder resolve inside `budget`; compute the standoff point on the source→target line at `ECHELON_STANDOFF_M` (450) and road-snap it; `ForceCreateDeploymentFrom(config, standoff, factionIndex, source, 0 /* invested */)`; record the created component for teardown; **return the rung cost**. Every refusal returns 0 and logs at VERBOSE with the reason.
4. **T4.4 — Call it from both modes**, once per source, immediately after that source's infantry allocation, inside the existing `allocated` accumulation so it flows into `spent`. ⚠ **Do not add a second debit and do not move the existing one.** The mode branch still ends before the debit.
5. **T4.5 — Siege anchors.** In `COUNTER_ATTACK`, the standoff point is a **ring slot** rather than a line point: reuse `OVT_QRFSiege.RingSlotOffset` and the ocean walk-in of `BuildSiegeRing`, then road-snap. ⚠ `BuildSiegeRing` runs **after** `SendWave` returns and is computed from the final queue length — an echelon must not add to `m_aSpawnQueue` or the four index-parallel arrays. It is a deployment, not a queued group.
6. **T4.6 — Teardown on `m_OnFinished`.** Every echelon this battle created is torn down with **`DeleteDeployment`** (C6/[D6](#d6--the-qrf-echelon-is-created-with-zero-invested-and-torn-down-with-deletedeployment)), never `CollectDeployment` or `RecallDeployment`. ⚠ Also unsubscribe on component cleanup — `ScriptInvoker.Insert` does not de-duplicate.
7. **T4.7 — Scoring verification, not new code.** `CheckUpdatePoints` already counts every occupying-faction `AIAgent` inside `QRF_POINT_RANGE` that is `IsFightingFit`. Confirm by inspection and by an Init case that a mounted crew (a) registers under the occupying faction key and (b) is fighting-fit while seated. If a seated occupant is excluded by `IsFightingFit`, **record it and change nothing** — the fix belongs to `qrf`.
8. **T4.8 — `OVT_TEST_Init_QRFMountedEchelon.c`:** the reachability gate refuses a land-isolated source; the budget gate refuses when the slice is under the cheapest rung; the cap refuses the third echelon; the returned cost equals the rung cost; **the conserved-total case** — a wave with one echelon debits `m_iResources` exactly once, by exactly `spent`, and never touches `m_mFactionResources`.
9. **T4.9 — `context.md`:** the reachability limitation verbatim, the four-parallel-array hazard, the delete-not-collect rule and why, and the T4.7 verdict.
10. **T4.10 — Epic overview:** add the reachability limitation to the *Land reachability* section as a second bullet.

**Acceptance criteria**

- compile **0**; **All** green.
- `grep -c "m_OccupyingFaction.m_iResources -=" Scripts/Game/Controllers/OccupyingFaction/OVT_QRFControllerComponent.c` → **1**.
- `grep -n "CollectDeployment\|RecallDeployment" Scripts/Game/Controllers/OccupyingFaction/OVT_QRFControllerComponent.c` → **empty**.
- `grep -n "m_aSpawnQueue\|m_aSpawnRingSlots" ` within `SendMountedEchelon` → **empty**.
- `git diff Scripts/Game/GameMode/Deployments/Modules/OVT_InsertionSpawningDeploymentModule.c` → still only Phase 2's enum line.

---

### Phase 5 — Crew-up on alarm

**Agent:** `component-developer` — standard. Two small modules, one invoker, one registry delta; the hard part (mounting, driving, LOD) is Phase 2's.
**Estimate:** 8–11 h · **Suite after this phase:** **All**.

**Tasks**

1. **T5.1 — `ScriptInvoker<vector> m_OnBattleStarted`** on `OVT_OccupyingFactionManager`, published from `StartBaseQRF` (`:1231`) and `StartTownQRF` (`:1278`) at `m_vQRFLocation`, after the mode is configured and `Start()` has run. Document why it exists separately from `m_OnQRFTownChanged` (C9).
2. **T5.2 — `m_sVehicleRole` on `OVT_ParkedVehicleSpawningDeploymentModule`** (empty = today's `m_sVehicleType` path, G8) plus `ReleaseVehicleOwnership(Vehicle)` so a handed-over hull is not deleted twice. ⚠ Add it to that module's `CloneModule()`.
3. **T5.3 — Registry delta `"Base Parked Armour"`** on `Deployment_BaseParkedVehicles.conf` in `overthrowDeployments.conf`: role `armed`, `m_iVehicleCount 1`, `m_iCostPerVehicle 120`, `m_eParkingType PARKING_TRUCK`, plus the crew-up behaviour module, `m_iMinimumThreatLevel 400`, `m_fChance 60`, `m_bFreeAtGameStart 0`.
4. **T5.4 — `OVT_CrewUpOnAlarmBehaviorDeploymentModule`.** Attributes: `m_fAlarmRadius` (750, the QRF's own range), `m_sSortieConfigName` (`"Base Armour Sortie"`), `m_iSortieBudget` (60). On activate it subscribes to `m_OnBattleStarted`; on a battle inside the radius it resolves the parked vehicle, calls `ReleaseVehicleOwnership`, and force-creates the sortie **from** this base with the hull handed in. ⚠ Unsubscribe in `OnCleanup` and in `OnDeactivate`; `Insert` does not de-duplicate.
5. **T5.5 — `Deployment_BaseArmourSortie.conf`** (`{6BC10…}`): the mounted module with `m_bAdoptExistingVehicle 1`, a crew group, `m_fLZStandoffDistance 200`, and the mobile-checkpoint behaviour with `m_iRelocateMinutes 0` — the sortie parks in a defend posture short of the battle rather than driving into it. `m_bDirectorOnly 1`.
6. **T5.6 — `vehicle_crew` group entries** in both faction group registries (three men: driver, gunner, commander) so an armed hull's gun is manned. USSR and US each get one entry, cost 15. ⚠ Both `truck_crew` entries stay exactly as they are — the insertion path is untouched.
7. **T5.7 — Init cases:** the parked module clones the new field; the crew-up module fires exactly once for a battle inside the radius and not at all for one outside; ownership transfer leaves exactly one owner (assert the parked module no longer reports the vehicle in `GetSpawnedEntities()` and the mounted one does).
8. **T5.8 — `context.md`:** the ownership-transfer contract, the double-delete hazard, and the invoker's publication points.

**Acceptance criteria**

- compile **0**; **All** green.
- `git diff Configs/Deployment/Deployment_BaseParkedVehicles.conf` → **empty** (the armour variant is a registry delta).
- `grep -c "m_OnBattleStarted.Invoke" Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c` → **2**.
- `grep -n "m_OnBattleStarted.Remove" Scripts/Game/GameMode/Deployments/Modules/OVT_CrewUpOnAlarmBehaviorDeploymentModule.c` → present in both cleanup paths.

---

### Phase 6 — Hunter-killer armoured sweep

**Agent:** `component-developer` — standard. One dispatcher on an existing tick, one behaviour module, one config.
**Estimate:** 7–10 h · **Suite after this phase:** **All**.

**Tasks**

1. **T6.1 — `ReportVehicleLoss(vector position)`** on `OVT_OccupyingFactionManager`: insert an `OVT_TargetData` at the position through the same path the existing known-target inserts use (`:2110-2191`), deduplicating against `GetNearestKnownTarget`. Called from the mounted module's `HOLDING`/`DRIVING` vehicle-loss branch and from `ReleaseConvoy` when the vehicle was destroyed. ⚠ Not from teardown — a deployment collected normally has not lost anything.
2. **T6.2 — `OVT_ArmouredSweepBehaviorDeploymentModule`.** Attributes: `m_fSweepRadius` (400), `m_iSweepMinutes` (12), `m_fWaypointInterval` (90 s). It walks the vehicle around the hotspot on `Patrol`/`Defend` waypoints and calls `RequestDeploymentCollection("its sweep is over")` when the clock expires — `OVT_BaseBehaviorDeploymentModule` already owns collection, exfil holds and the player-watching veto (`:295-380`).
3. **T6.3 — `Deployment_HunterKillerSweep.conf`** (`{6BC10…}`): the mounted module (role `armed`, `m_iTruckCostOverride 90`, `m_iHoldTicks` matching the sweep clock, source `OVT_NearestControlledBaseSourceProvider`), the sweep behaviour, `m_bDirectorOnly 1`, `m_iMinimumThreatLevel 300`, `m_iBaseCost 60`.
4. **T6.4 — `TickHunterKiller()`** on the occupying faction manager's existing 60 s update (`OF_UPDATE_FREQUENCY`). It is the dispatcher the evaluator cannot be (C5): pick the highest-scoring known target by `GetThreatByLocation`, refuse when one sweep is already live, when the score is under a floor, when the deployment pool cannot afford it, or when a battle is engaged; otherwise `ForceCreateDeployment` **and debit the pool at that one call site** with `SubtractFactionResources`, exactly as the objective director does at `OVT_ObjectiveDirectorComponent.c:1225`.
5. **T6.5 — Init cases:** the dispatcher refuses at each of its four gates; it spends exactly once and the pool falls by exactly the config cost; `ReportVehicleLoss` deduplicates a second loss at the same spot; the sweep module clones completely.
6. **T6.6 — `context.md`:** why the evaluator could not be used (C5), and the create-then-debit choke point restated at the new call site.

**Acceptance criteria**

- compile **0**; **All** green.
- `grep -rn "AddFactionResources" Scripts/Game/GameMode/Objectives/` → still **empty**.
- `grep -c "SubtractFactionResources" Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c` → exactly the pre-existing count **+ 1**.
- One live sweep at a time: `grep -n "m_HunterKillerDeployment" …` shows a single-slot field, not a list.

---

### Phase 7 — Help, documentation and localization sync

**Agent:** `help-docs-sync`.
**Estimate:** 3–5 h · **Suite after this phase:** **Fast** (no code change expected).

Player-facing behaviour changes in this feature — armour appears, checkpoints block roads, battles arrive by vehicle — so the closing sync is in scope.

**Tasks**

1. **T7.1 — Fact-check first.** Every sentence written must cite a `file:line`. Do not invent thresholds: the ladder rungs and the difficulty spread are in §3.2 and in `Configs/Factions/*.conf`; the checkpoint band is `m_fApproachMinDistance`/`Max`; the echelon cap is a const in the QRF controller.
2. **T7.2 — Field Manual:** one page section on the occupying faction's armour — that it escalates with threat, that a parked BTR at a base is a warning and not decoration, that a checkpoint on the road into a contested town is deliberate and can be destroyed, and that AT is worth carrying from mid-campaign. `Configs/FieldManual/`.
3. **T7.3 — Tutorial popup:** at most one, fired the first time the player sees an occupying armed vehicle, pointing at the Field Manual page. `Configs/Tutorials/`.
4. **T7.4 — `.st` master only.** ~4 keys in `Language/localization_Overthrow.st`. ⚠ **Never edit `Configs/Language/*.conf`** — they are Workbench build output. Record the re-export as owed.
5. **T7.5 — Wiki draft** for the occupying-faction page, plus the epic-overview row 7, build order, and rollup update.

**Acceptance criteria**

- `git diff Configs/Language/` → **empty**.
- Every new sentence traceable to a `file:line` recorded in `context.md`.
- `docs/features/occupying/epic-overview.md` carries row 7 and the reachability follow-up.

---

## 5. Key Technical Decisions

### D1 — Approach A: subclass the insertion module (rejected B and C)

**A (chosen).** `OVT_MountedForceSpawningDeploymentModule : OVT_InsertionSpawningDeploymentModule`; the QRF stands deployments up through the manager instead of spawning vehicles itself. One vehicle code path for harassment, the echelon, the sortie and the sweep.
**Why.** The convoy path is three play-test rounds and seven diagnosed faults deep (`objectives/context.md`, *the insertion convoy, seven faults*): the LOD pin, event-driven seating, the retire-in-place fix, the `HIGH` crew importance, the split clocks, the stuck-truck collection rule. Copying any of it starts that clock again. Subclassing also keeps `OVT_ReinforcementBehaviorDeploymentModule.GetMissingUnitsCount()`'s cast to `OVT_InfantrySpawningDeploymentModule` working — a sibling class would silently never be rebought after a wipe, which is exactly why the insertion module subclasses the infantry module in the first place.
**B — extract a shared convoy driver first. Rejected:** a refactor of a 3,368-line file that was play-tested green two days ago, with no behavioural test that could prove parity. The regression risk is the whole feature's risk budget spent before any of it ships.
**C — extend `OVT_VehicleSpawningDeploymentModule`. Rejected:** it has no source provider, no landing zone, no convoy cap, no stuck test and no walk fallback. Adding them is reimplementing A's parent, badly.

### D2 — The ladder lives in the faction registry, with a role tag

Two new fields on `OVT_FactionVehicleEntry`, not one. `m_iMinThreat` alone would force the resolver to consider `car` and `truck` as rungs of the same ladder. **Rejected alternatives:** a name-prefix convention (`armed_0`, `armed_1`) — fragile and unauthorable; an array of vehicle names on each module (the `m_aLadder` shape) — it would duplicate the ladder in four configs and make adding a rung a four-file edit. The registry is where "what can this army field" belongs, and a config that names a role gets the whole ladder for free.

### D3 — Shipped vehicle names are not renamed

`heavy_armed` keeps its prefab in both factions because `Deployment_VehiclePatrol_Heavy.conf`, `Deployment_BaseHeavyPatrol.conf` and the registry deltas consume it by name. The consequence is an asymmetric top-rung name (`heavy_armor` for US, `heavy_armed` for USSR) and it is accepted: the ladder resolves by **role and threshold**, and no code path looks up a rung by name.

### D4 — The vehicle price is a budget, not a receipt

`GetResourceCost()` is computed from the config **template**, before a deployment exists and therefore before the faction is known — so the rung cannot be resolved at pricing time. The module charges the authored `m_iTruckCostOverride` and then **refuses any rung dearer than it**. This makes escalation cost money (a BTR-70 config has to author ≥ 120), keeps the create-then-debit choke point single, and reuses the parent's own documented stance verbatim rather than inventing a second pricing model.

### D5 — The QRF asks the deployment manager; it does not spawn vehicles

The alternative — teaching `SpawnTroops` to spawn a crewed vehicle — would put a second vehicle lifecycle inside a component whose four index-parallel spawn arrays are already documented as a trap, and would give the battle layer its own copy of the convoy, LOD and teardown problems. Standing up a deployment costs one method and inherits everything. It also makes the echelon **visible to the framework**: the GM sees it, teardown sweeps it, and the battle-suppression pass reasons about it correctly.

### D6 — The QRF echelon is created with zero invested and torn down with `DeleteDeployment`

`RecallDeployment` and `CollectDeployment` both credit `AddFactionResources` — the **pool** — while a QRF wave debits `m_iResources` — the **reserve** (C6). An echelon funded from the wave budget and later collected would move money between two ledgers and create it. So: `resourcesInvested = 0` at creation (nothing to recall), the true cost joins the wave's `spent` and leaves the reserve at the single existing debit, and every echelon is **deleted** when the battle finishes. The rule is stated in the config header, at the call site, and as an acceptance grep.

### D7 — A runtime source override, not a runtime source provider

The echelon and the sortie both need "come from *this* base", which is a runtime fact; providers are authored. Rather than invent a mutable provider, the mounted module carries a `SetSourceOverride(vector)` that `EnsureSourceResolved()` prefers, applied by a manager overload **before the first convergence** — because the source decides both the anchor the force is registered at and the ring it is registered on, and neither can be changed afterwards without throwing away the survivor mask. Every other caller keeps an authored provider and the override stays zero.

### D8 — Battle suppression needs no whitelist: the riding ring already exempts a mounted force

`SuppressForcesAroundBattle` skips any handle whose spawn distance is **strictly wider** than the world's global ring (`OVT_DeploymentManager.c:565-568`), a rule written for exactly this case — *"the insertion module's riding passengers and crew, which exist precisely so they can be seated in a truck driving through empty country. Pinning those arrives the convoy empty."* A mounted force never leaves that ring, because it never dismounts to the global one. **The whole decision is therefore a prohibition**: the mounted module must not call `DropPassengersToGlobalRing()`. An Init case pins it, because the failure mode — an echelon that materialises nothing at a battle — is silent.

### D9 — Nothing about live vehicles is persisted

A convoy is already never resumed across a load, and the walk fallback is the documented answer. Both battle-scoped deployments die with a battle that is itself deliberately rolled back on load. Persisting a live mounted force would mean persisting a vehicle, its compartment occupancy and a half-driven route — the BUG-030 class — for a force that is on the ground for minutes. **Restored mounted deployments walk**, and that is the feature working.

### D10 — Reachability is bounded, not solved

There is no A→B land-reachability query in the engine or the epic. This feature adds no invented one. It refuses land-isolated sources and sources with no road at either end, and accepts that a source with roads at both ends across a water gap will stall — at which point the stuck test fires and the force walks. The limitation is written into the epic's Tech Debt so the next doctrine inherits the statement rather than the assumption.

---

## 6. Definition of Done

### Functional

- **F1** — On a fresh Normal campaign at threat 0, an occupying armed vehicle deployment fields a **UAZ-PKM / M151-M2HB**. After threat passes 400 it fields a **BRDM-2 / M1025-M2HB**; after 900, a **BTR-70 / LAV-25**. On Easy the same rungs need 800 / 1800; on Insane 100 / 225.
- **F2** — A harassment ramp that reaches its fifth success at a town sends a **mounted** operation: an armed vehicle drives from a real occupying holding, parks on a road 150–300 m out on an approach to the town, its infantry dismounts to a perimeter, its gunner stays up, and it relocates to a different approach on the authored clock.
- **F3** — Destroying the checkpoint vehicle leaves the force alive on the ground holding its objective plan, not standing in a wreck.
- **F4** — A battle at a base with a road-reachable occupying source sees **one or two** mounted echelons drive in from that base's direction, arriving after the first infantry wave, stopping ~450 m out; their crews score in zone control; in a counter-attack siege they sit on ring bearings and block the road.
- **F5** — A battle's echelons are gone when the battle ends, and the faction's reserve fell by exactly what the waves committed, once.
- **F6** — At threat ≥ 400 some occupying bases have a **parked armed vehicle** a player can walk up to and scout. When that base's battle starts, it is crewed and moves out to a defend posture; when a battle starts elsewhere, it does not.
- **F7** — Destroying an occupying vehicle in the field is followed, within a few minutes, by an armoured sweep of that area that loiters for a bounded time and then leaves.
- **F8** — Every one of the above degrades to a walking force when the vehicle cannot be spawned, the convoy cap is spent, the crew never materialises, the vehicle is stuck or the vehicle is destroyed.

### Quality

- **Q1** — `git diff Scripts/Game/GameMode/Deployments/Modules/OVT_InsertionSpawningDeploymentModule.c` is one appended enum value.
- **Q2** — Every new clonable class has a dedicated Init clone case with distinct non-default values and a consequence-naming `SetFailure`.
- **Q3** — `OVT_VehicleLadderRules` contains no world/manager identifier, comments included, and its Logic case covers every boundary including "exactly at the threshold".
- **Q4** — The QRF's single reserve debit is still single and still outside the mode branch.
- **Q5** — `grep -rn "AddFactionResources" Scripts/Game/GameMode/Objectives/` is empty.
- **Q6** — No new `RplProp`, no new RPC, no GM wire version change. `grep -rn "RplProp\|Rpc(" ` across every file this feature adds → **empty**.
- **Q7** — No new persistence serializer, no version bump: `git diff Scripts/Game/Persistence/ Configs/Systems/Persistence/` → **empty**.
- **Q8** — `git diff Configs/Language/` → empty; only the `.st` master is edited.

### Integration

- **I1** — `git diff Scripts/Game/GameMode/Virtualization/ Scripts/Game/GameMode/VirtualMovement/ docs/features/virtualization/core/api.md` → **empty**.
- **I2** — `git diff Configs/Deployment/Deployment_TownPatrol.conf Configs/Deployment/Deployment_VehiclePatrol_*.conf Configs/Deployment/Deployment_Base*.conf Configs/Deployment/Deployment_Objective{FOB,FOBGarrison,Sabotage,TowerRecapture,Harassment}.conf` → **empty**.
- **I3** — `git diff UI/ Configs/Map/` → **empty**.
- **I4** — All four new configs author `m_bDirectorOnly 1`; the evaluator never picks one.

### Verification Method

**Automated — from the repo root, in order:**

1. `tools/compile-check.sh` → exit **0**.
2. `tools/run-tests.sh "{6A6E29FF47ECB840}"` → exit **0** (Fast).
3. `tools/run-tests.sh "{6A6E2A002F53A581}"` → exit **0** (All).
4. `grep -rn "OVT_Global\|GetGame()\|World\|Entity" Scripts/Game/Data/OVT_VehicleLadderRules.c` → **empty**. → Q3
5. `git diff Scripts/Game/GameMode/Deployments/Modules/OVT_InsertionSpawningDeploymentModule.c` → one enum value. → Q1
6. `grep -c "m_OccupyingFaction.m_iResources -=" Scripts/Game/Controllers/OccupyingFaction/OVT_QRFControllerComponent.c` → **1**. → Q4
7. `grep -n "CollectDeployment\|RecallDeployment" Scripts/Game/Controllers/OccupyingFaction/OVT_QRFControllerComponent.c` → **empty**. → D6
8. `grep -rn "AddFactionResources" Scripts/Game/GameMode/Objectives/` → **empty**. → Q5
9. `grep -rn "RplProp\|Rpc(" Scripts/Game/Data/OVT_VehicleLadderRules.c Scripts/Game/GameMode/Deployments/Modules/OVT_MountedForceSpawningDeploymentModule.c Scripts/Game/GameMode/Deployments/Modules/OVT_*Behavior*.c` → **empty**. → Q6
10. `git diff Scripts/Game/Persistence/ Configs/Systems/Persistence/ Configs/Language/ UI/ Configs/Map/` → **empty**. → Q7, Q8, I3
11. `grep -c "m_bDirectorOnly 1" Configs/Deployment/Deployment_{ObjectiveHarassment_Mounted,QRFMountedEchelon,BaseArmourSortie,HunterKillerSweep}.conf` → **1 each**. → I4
12. `grep -n "DropPassengersToGlobalRing" Scripts/Game/GameMode/Deployments/Modules/OVT_MountedForceSpawningDeploymentModule.c` → **empty**. → D8

**Workbench (`compile-check.sh` cannot see `.conf` or prefab faults):**

13. Open `Configs/Factions/USSR_OverthrowData.conf` and `US_OverthrowData.conf`. **Expect:** the vehicle registry lists five entries each, three tagged role `armed` with 0 / 400 / 900, and no unresolved attribute.
14. Open `Configs/Deployment/overthrowDeployments.conf`. **Expect:** **27** entries; the four new configs and the parked-armour delta all expand with their modules.
15. Open `Configs/Objective/Objective_TownOffensive.conf`. **Expect:** both harassment operations show a **five**-entry `m_aLadder` ending in `"Objective Harassment (Mounted)"`.
16. Open `Prefabs/Vehicles/Wheeled/BRDM2/BRDM2.et` and `LAV25/LAV25.et`. **Expect:** each resolves against its vanilla parent and shows the three `AICarMovementComponent` values; no duplicated `ActionsManagerComponent`.

**Manual — solo play-test.** Debug affordances: `/give-resources`, a raised time multiplier, and a **temporary** `vehicleThresholdScale 0.05` in `Difficulty_Normal.conf` to bring every rung into reach — **revert before committing** (Q-greps require a clean diff).

1. **Fresh campaign; raise threat by flipping a base.** Read the log for `[Overthrow]` ladder lines. **Expect:** the rung named, with the threat and the scaled threshold that chose it. → F1
2. **Take a town, let the harassment ramp run to its fifth success.** **Expect:** a mounted operation, a real drive from an occupying holding, and a checkpoint on a road into the town at 150–300 m with the gun manned. → F2
3. **Watch one relocation.** **Expect:** a different approach bearing, not the same one. → F2
4. **Destroy the checkpoint vehicle.** **Expect:** the surviving infantry stays in the fight at the objective; nothing is left standing in a wreck; the log says the force is on foot. → F3, F8
5. **Attack a base with another occupying base ~2 km away by road.** **Expect:** infantry arrives first; an armoured echelon **drives** in from that base's bearing and stops ~450 m out; ambushing it on the road works. → F4
6. **Watch the GM panel's pool and the log's reserve line across the battle.** **Expect:** one debit per wave, no pool movement attributable to the echelon. → F5, Q4
7. **Let the battle end.** **Expect:** no orphan armoured deployment on the map, no crew standing where the echelon was. → F5
8. **Trigger a counter-attack siege.** **Expect:** echelons on ring bearings, on roads, blocking approaches, and no distortion of the infantry ring's spacing. → F4
9. **Scout a base at threat ≥ 400.** **Expect:** a parked armed vehicle, uncrewed, approachable. Attack that base. **Expect:** it is crewed and moves out. Attack a *different* base. **Expect:** it does not. → F6
10. **Destroy an occupying vehicle in open country and wait.** **Expect:** an armoured sweep of that area within a few minutes; it loiters and then leaves; only one at a time. → F7
11. **Save mid-drive, quit, Continue.** **Expect:** the force is on the ground and walking; no ghost vehicle; no error. → D9
12. **Play an hour with `vehicleThresholdScale` back at 1.0.** **Expect:** the escalation reads as escalation — jeeps early, armour later — and the log explains every refusal.
13. **Dedicated-server / MP pass.** The automated spine covers MP not at all. Confirm a joining client sees the vehicles, that nothing new appears on the map, and that no client-side error storm follows a mounted deployment.

---

## 7. Testing Strategy

**The automated spine covers the ladder arithmetic, the module seams and the accounting. Everything about whether a vehicle actually drives, arrives, parks legibly and survives a player is a play-test** — and MP is untested throughout.

| Tier | What it can prove here | Files |
|---|---|---|
| **Logic** (Fast) | The whole ladder: scaled thresholds, boundary equality, budget fit, tie-breaking, empty input. Checkpoint bearing arithmetic if extractable. | `OVT_TEST_Logic_VehicleLadder.c` (new), `OVT_TEST_Logic_DeploymentBattleSuppression.c` (+1) |
| **Init** (Fast) | Clone fidelity on four new modules; the ladder resolving against the *shipped* faction configs; `CompleteInsertion` leaving `HOLDING`; the source override winning; the riding-ring suppression exemption; the QRF's reachability/budget/cap refusals; the conserved-total assertion; crew-up firing once; ownership transfer. | `OVT_TEST_Init_MountedForce.c`, `OVT_TEST_Init_QRFMountedEchelon.c` (both new), `OVT_TEST_Init_DeploymentBattleSuppression.c`, `OVT_TEST_Init_ObjectiveOperations.c` |
| **Campaign / Persistence** | **Nothing new.** No save format changes and no started-campaign state is added. A Persistence case here would assert an absence. |

**Binding constraints inherited from the epic's suites — every one applies:**

- **`CloneModule` copies by hand, silently drops what it forgets, and is NOT chained.** Four new clonable classes; **every one gets a dedicated case**. The mounted module's list is 27 fields.
- **Init-tier worlds never run `PostGameStart`** — a case needing a tick installs it itself.
- **Deployment fixtures must be `SetSpawnedUnitsEliminated(true)`** on the deployment **and every spawning module** before anything ticks; the autotest camera is an observer, and `m_iSpawnDistanceOverride 100000` means a mounted crew materialises near it.
- **The Logic-tier rule is a directory-wide grep that does not distinguish code from prose** — no `OVT_Global` or `GetGame().GetGameMode` under `TestSuites/Logic/`, comments included, nor in the pure files those tests grep.
- **`new` does not apply `[Attribute()]` defvalues** — a hand-built subject needs every field set, which is what makes the clone cases honest.
- **No `maxAttempts`. Nothing asserts on live AI reaching a place.** Randomness enters only through `s_AIRandomGenerator`.
- **`RandInt` is max-exclusive; `array.Remove` is swap-with-last; `out` and `owned` are reserved; `vector.Distance` is +1 ULP at 1 000 / 2 000 m; `PrintFormat` and `SetFailure` take at most 3 params after the format string.**
- **`Rpc()` arity is a compile-check blind spot (BUG-090)** — this feature adds no RPC, which is Q6's other purpose.
- **Never hand-edit `Language/*.conf`.**
- **Suites are not deterministic under load** — run the All group alone, and treat a concurrent Workbench session as an INDETERMINATE result rather than a red.

**Explicitly not automatable, and why:** whether a BRDM-2 gets down a Chotain back road; whether a checkpoint reads as a checkpoint; whether an echelon arrives late enough to matter and early enough to fight; whether the gunner engages; whether a crewed hull is fighting-fit for scoring; anything in multiplayer. All are play-test steps in [§6](#verification-method).

---

## 8. Dependencies

| Depends on | Why | Status |
|---|---|---|
| `occupying/deployments` | The module framework, the pool, `ForceCreateDeployment`, the convoy cap, battle suppression | Shipped; documented retrospectively |
| `occupying/objectives` | Plan/phase authoring, `OVT_SendDeploymentObjectiveOperation`'s ladder, the reserve-floor rules, `OVT_ObjectiveAnchorSourceProvider` | **Built 2026-08-21, Ready for Review — human verification still owed.** Phase 3 edits its two shipped plans; if a review changes the ladder mechanism, Phase 3 must re-baseline |
| `occupying/qrf` | `SendWave`, the single debit, zone scoring, the siege ring | Legacy/closed; consumed as-is. Phase 4 is the only feature in the epic currently allowed to edit it |
| `occupying/core` | `GetThreatFloat()`, `m_aKnownTargets`, `GetThreatByLocation`, `StartBaseQRF`/`StartTownQRF` | Shipped |
| `virtualization/core` | Registration, spawn rings, LOD; **frozen API** — this feature asks it for nothing new | 🔒 Frozen |
| Vanilla prefabs | BRDM-2 `{254289B9C09904AB}`, LAV-25 `{0FBF8F010F81A4E5}` | Verified present 2026-08-23 |

**Blocks:** nothing. **Blocked by:** nothing hard; Phase 3 is the only phase with a soft dependency on `objectives`' review outcome.

---

## 9. Risks & Mitigation

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| **R1** | 🔴 **AI driving.** Reforger vehicle AI strands itself, and heavier hulls on narrow roads are worse than the Ural the tuning was proven on. A stalled BTR is also the next convoy's roadblock. | **High** | Escalation that never arrives; visible piles of stuck armour | The stuck test, the walk fallback and `STUCK_TRUCK_TIMEOUT_TICKS = 1` collection are all inherited unchanged. T1.7 applies the proven Ural tuning to both new hulls. Play-test steps 2, 5 and 12 are the live check. **The force walking is a success path, not a bug report.** |
| **R2** | 🔴 **A crew with no behaviour tree.** The fault that cost `objectives` three play-test rounds: at max LOD an agent's tree is off, so a materialised driver never drives. | **High** if re-derived | Convoys that never move, with a perfectly alive crew | **Do not re-derive it.** `OVT_MountedGroupActivation.HoldGroupActive` / `ReleaseGroupActive` are inherited through `HoldRidersActive()`/`ReleaseRidersActive()` and are not touched. Any new code path that seats or unseats a rider must go through the parent's methods; T2.3's header states it. |
| **R3** | **Permanently-materialised crews cost frames.** Riding crews sit on a 100 km ring with an LOD pin; four mounted configs plus parked armour could put many on the map at once. | Medium-High | Server frame time on a long campaign | Every caller is bounded: the convoy cap (`objectiveMaxConcurrentInsertions`, 2) bounds live drives; `ECHELON_CAP_PER_BATTLE` is 2; one hunter-killer at a time; `m_iMaxInstances`/`m_fChance` bound parked armour; a checkpoint that arrives stops driving. **Instrument, do not assume** — play-test step 12 watches an hour with the log's activation lines. |
| **R4** | 🔴 **Accounting drift between the reserve and the pool.** The echelon is the first thing in the tree funded from one ledger and owned by the framework that credits the other. | Medium | Money creation on every battle — BUG-027's family in a new place | [D6](#d6--the-qrf-echelon-is-created-with-zero-invested-and-torn-down-with-deletedeployment): zero invested, delete never collect. T4.8's conserved-total Init case, and the two greps in the DoD (single debit, no collect/recall) |
| **R5** | **`CloneModule` drops an attribute.** The standing trap; the mounted module's list is 27 lines and three levels deep. | **High** | Silent wrong behaviour: a role that reverts to `truck`, a hold that never ends, an adopt flag that never fires | Every concrete class hand-writes the full list; every one gets a dedicated Init case with distinct non-default values; Q2 makes it countable; T2.7's header names what each dropped line costs |
| **R6** | **The riding-ring exemption is thrown away.** One call to `DropPassengersToGlobalRing()` in the mounted path makes every echelon materialise nothing at a battle. | Medium | A silent, battle-only failure that no unit test would notice without T2.11 | [D8](#d8--battle-suppression-needs-no-whitelist-the-riding-ring-already-exempts-a-mounted-force) is written as a prohibition, T2.11 pins it at the Init tier, and DoD step 12 greps for the call by name |
| **R7** | **Reachability.** A source with roads at both ends across water sends an echelon that can never arrive. | Medium | A wasted budget slice per wave, repeatedly | [D10](#d10--reachability-is-bounded-not-solved): isolated sources refused, road sanity at both ends, and the stall→walk fallback absorbs the rest. Recorded in the epic's Tech Debt rather than hidden in a helper |
| **R8** | **`objectives` is Ready for Review, not closed.** Phase 3 edits its two shipped plans and its harassment ladder. | Medium | Rework if the review changes the ladder or the phase names | Phase 3 opens with a re-baseline; the rung is **appended**, so a reordering upstream is a one-line fix; `Deployment_ObjectiveHarassment.conf` is untouched |
| **R9** | **MP is untested.** The automated spine covers it not at all, and mounted forces are the most entity-heavy thing this epic has shipped. | **High** | Client-visible desync or an error storm found only by players | Nothing new replicates (Q6) — the smallest possible MP surface. Play-test step 13 is a required DoD item, not an optional one |
| **R10** | **Concurrent sessions move the tree.** Every `file:line` here was verified 2026-08-23 on a clean `v1.5`; the epic has already had a line number drift 650 lines in a day. | **High** | Failed edits and designs built on stale facts | Phases 2, 4 open with a read-only survey task whose only job is re-verification. No task depends on a line number for correctness — every one names the symbol too. `sed` is the fallback when a string match fails |
| **R11** | **Capacity/cost corrections change shipped behaviour.** `heavy_armed`'s capacity goes 4 → 10 for USSR, which the heavy vehicle patrol reads. | Low-Medium | A patrol that suddenly carries six more men, unnoticed | Called out in C7 and T1.6 as a deliberate correction, gated on the **All** suite, and named in play-test step 12 |

---

## 10. Deferred / follow-ups

Recorded so they are not lost, and **deliberately not built**.

- 💡 **Escorted supply convoys.** Base→base and base→FOB supply runs with an armed escort, lootable when ambushed. Natural fit for the mounted module (`m_bDismountOnArrival 1`, a cargo behaviour), but it needs a supply *concept* the occupying faction does not have — today a base's stock is a number, not a thing that travels. Would also give the player a reason to interdict roads, which nothing currently does.
- 💡 **Checkpoint vehicles at road checkpoints.** `Deployment_BaseCheckpoints` already places compositions on road slots (`OVT_RoadSlotOverwatchPlacementProvider`); parking a ladder-picked armed vehicle at one is a config change plus a placement provider. Deferred because the checkpoint compositions have their own persistence debt (**BUG-030**) and adding a vehicle to a thing that vanishes on load would compound it.
- 💡 **Show-of-force in unstable towns.** `OVT_TownUnrestConditionDeploymentModule` already gates `Deployment_TowerRecaptureUnrest`; a vehicle patrol that circles an unstable town, suppressing support gains, would reuse it verbatim. Deferred as pure content once the ladder exists.
- 💡 **Air insertion at high threat.** The top of the ladder above armour. Needs a helicopter source, LZ selection, and a fast-rope or landing behaviour — none of which the insertion module has, and all of which would be a second convoy path. Revisit only after the ground ladder has been played.
- 💳 **A real reachability query.** Still absent epic-wide ([D10](#d10--reachability-is-bounded-not-solved)). The honest fix is a cached land-connectivity partition built once per world from navmesh tiles, which would serve objectives, insertions, forward bases and this feature at once.
- 💳 **A dedicated `vehicle_crew` per hull class.** Phase 5 adds one three-man group per faction; a LAV-25 with nine seats and a BRDM-2 with four want different crews. Deferred until a play-test shows an empty seat mattering.

---

## 11. Quality Bar — the hard floor

This is a **backend / AI-behaviour** feature. There is no UI polish axis. The bar is reliability under a hostile physics simulation, conserved resource accounting, and whether the escalation is legible to a player who is not reading the log.

| Bar | What it means concretely | How it is caught |
|---|---|---|
| **Reliability — every failure degrades to the march** | There is no state in which a mounted force is stranded, deleted mid-mission, or standing in a wreck. Vehicle destroyed, no convoy slot, crew never materialised, road impassable, source unreachable: five different causes, one outcome — men on the ground, holding the plan they were registered with, walking. | The inherited fallback is not re-derived (D1); T2.10's destroyed-vehicle case; play-test steps 4 and 11; F8 |
| **Resource conservation — one ledger, one debit** | Every creation debits exactly one pool at exactly one call site. The QRF's reserve debit stays single and outside the mode branch. Nothing this feature creates is ever recalled or collected into a pool it was not paid from. | D6; T4.8's conserved-total Init case; DoD greps 6, 7, 8; Q4, Q5 |
| **Legibility of the escalation** | A player who never opens a log can tell the campaign has escalated: they meet jeeps early and armour later, they can *see* a parked BTR before it is used against them, and a checkpoint on the road into their town is obviously deliberate. Every mounted force is approachable on the ground long enough to be attacked. | F1, F2, F6; play-test steps 1, 2, 9, 12; the Field Manual page in Phase 7 |
| **The play-tested spine is not disturbed** | The insertion module's seven-fault history is inherited, not reopened. One appended enum value is the whole diff. Any new seating, LOD or teardown code is a plan defect — raise it, do not widen the module. | Q1; DoD grep 5; T2.1's survey |
| **Deterministic, seam-driven tests** | Every new case is world-free (Logic) or driven through a public seam (Init). No `maxAttempts`, nothing asserting on live AI reaching a place, randomness only through `s_AIRandomGenerator`, and every case carries a recorded can-fail proof. | §7; Q2, Q3 |
| **Authorability** | A modder adds a rung by editing a faction `.conf`, and a fifth mounted doctrine by writing one deployment `.conf` — no script. Every attribute has a `desc:` a tuner can act on and names its difficulty field when it has one. | The `.conf` surface of §3.1; Workbench checks 13–16 |
| **The neighbours stay frozen** | Virtualization, VirtualMovement, `api.md`, every shipped deployment config, the map, the HUD, the GM wire, every serializer. | I1–I4; DoD greps 10, 11 |

---

## Agent Routing Summary

| Phase | Agent | Advanced? |
|---|---|---|
| 1 — Ladder: registry, difficulty, pure resolver | `component-developer` | no — additive data plumbing with a pure spine |
| 2 — The mounted-force module | `component-developer-advanced` | **yes** — subclasses the epic's most intricate module and changes a lifecycle terminal state |
| 3 — Mounted harassment + mobile checkpoint | `component-developer-advanced` | **yes** — edits both shipped objective plans; first behaviour module to own a vehicle |
| 4 — QRF mounted echelon | `component-developer-advanced` | **yes (max effort)** — the battle layer, the single debit, a reachability gate, two QRF modes |
| 5 — Crew-up on alarm | `component-developer` | no — small modules over Phase 2's machinery; the ownership transfer is the one hazard |
| 6 — Hunter-killer armoured sweep | `component-developer` | no — one dispatcher on an existing tick, one behaviour module, one config |
| 7 — Help & documentation sync | `help-docs-sync` | — |

**Skills to activate:** `enforcescript-patterns` (1–6), `overthrow-architecture` (1–6), `workbench-workflow` (1, 3–6 — every `.conf` and prefab task, and every play-test).

**Estimate:** 64–91 h across seven phases, of which Phases 2 and 4 are roughly half.

**Owed to the user at the end:** a **localization re-export from Workbench** (Phase 7 adds ~4 keys to `Language/localization_Overthrow.st`; the `Configs/Language/*.conf` exports are build output and must never be hand-edited), and the MP pass of play-test step 13, which no automated tier covers.
