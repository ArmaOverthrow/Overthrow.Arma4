# Counter Attacks — Implementation Plan

**Status:** Planning
**Started:** 2026-08-18
**Target Completion:** TBD
**Last Updated:** 2026-08-18 (written by `solution-architect`; every `file:line` verified against the working tree on this date)

**Epic:** `occupying` — see `docs/features/occupying/epic-overview.md`
**Requirements:** `docs/features/occupying/counter-attacks/requirements.md` — **authoritative**. Its Out of Scope list is binding.
**Consumes:** the deployments framework (`docs/features/occupying/deployments/`), the QRF controller (`docs/features/occupying/qrf/`), the OF command layer (`docs/features/occupying/core/`).
**Consumes:** `docs/features/virtualization/core/api.md` — 🔒 **FROZEN**. **This feature asks core for nothing.** Every group it creates is created through a deployment module, exactly as `integration` and `base-defense-migration` established.
**Consumes:** `docs/features/virtualization/base-defense-migration/context.md` §"Gotchas & Learnings" — its inherited-binding list applies verbatim here and is restated in [§7](#7-testing-strategy).

> **What this feature is.** The occupying faction currently attacks by dice. Once per in-game hour, if its reserve is over 2 000 and a cooldown has expired, a 10 % roll picks a **uniformly random** base and drops a QRF on it (`OVT_OccupyingFactionManager.c:1418-1432`). A second trigger drops an instant QRF on any resistance town whose support fell below 25 % while a player stood within 300 m (`:1456-1469`). Neither has a build-up, a warning, or a reason.
>
> This feature deletes both and replaces them with a **single current objective** that the occupying faction works toward in three visible phases — harassment, then a forward operating base, then the counter-QRF. The resistance can read the ramp, predict the target, and spend on defence instead of only on offence.
>
> **It is additive to the deployment layer, not a new one.** Every soldier, truck, sabotage team and garrison this feature puts in the world is a deployment, funded from the one pool `base-defense-migration` established, registered with the virtualization core through the module seam. The new component decides *where* and *when*; the framework still decides *how*.

---

## Corrections to the requirements document

The requirements were written on 2026-08-17, before `virtualization/base-defense-migration` completed. Five of its statements are stale. **This plan supersedes them; the rest of the requirements stands.**

| # | Requirements says | Verified state, 2026-08-18 |
|---|---|---|
| C1 | Specops is *"being dropped by the base-defense-migration work currently in progress"* | **Already dropped.** `OVT_BaseUpgradeSpecops` and the whole `BaseUpgrades/` directory are gone. The OF has **no tower-recapture path at all today** — this feature restores it, and until it lands towers only ever flow one way. |
| C2 | Retirement targets at `CheckUpdate():1216-1229` and `:1232-1267` | The file is now 1 819 lines. The counter-attack roll is **`:1418-1432`**, the town-suppression QRF is **`:1456-1469`**, the timeout decrement is **`:1349-1352`**, the field is **`:176`**. |
| C3 | *"registries currently ship only `light_patrol` and `light_fireteam`"* | Both faction registries now ship **eight** group entries (`light_patrol`, `light_fireteam`, `rifle_squad`, `heavy_infantry`, `at_team`, `sniper`, `sniper_team`, `bunker_team`), four vehicle entries (`light_armed`, `heavy_armed`, `car`, `truck`) and five compositions. The registry work here is **much smaller than assumed**: a specops-grade team and a truck crew, and nothing else ([§3.6](#faction-registry-and-difficulty-additions)). |
| C4 | *"Wave LZ selection is biased toward the actual source's bearing (compute the preferred-direction fields from source geometry **instead of relying on authored cosmetic values**)"*, with the brief recommending a prerequisite fix to the LZ cache | **No prerequisite fix is needed, and the brief's recommendation is withdrawn.** The file-scope globals `Goodqrfpos`/`Goodqrfbasepos` **no longer exist** (`grep` is empty); each wave source already resolves its own LZ with the no-cache comment at `OVT_QRFControllerComponent.c:456-457`; the TraceBox no-op is fixed at `:492-494`; and the preferred-direction wrap bug is fixed at `:409-414`. All three landed in commit `d7e42362` under BUG-031. The bearing-bias change is now a **clean, self-contained edit** — see [D9](#d9--the-qrf-changes-are-exactly-two-and-need-no-prerequisite-repair). |
| C5 | The tower-recapture module *"inherit[s] the retired specops hold-timer mechanic"* from `OVT_RadioTowerCaptureBehaviorDeploymentModule` | That module has **no hold timer**. It fires a single edge when its own garrison is wiped (`EvaluateCapture`, `:85-130`). The hold timer is genuinely **new code** here; what is inherited from that module is its *shape* — the `EvaluateX(...)` pure-ish method split out of `OnUpdate` so it can be asserted without a live marker. |

---

## 1. Executive Summary

The occupying faction gains one piece of long-term intent: a **current objective** — a base, town or city the resistance holds that it has decided to take back — and a three-phase campaign to take it.

A new **server-only game-mode component, `OVT_ObjectiveDirectorComponent`**, owns that intent. Once per in-game minute (the OF manager's own cadence) it:

1. **Selects** an objective when it has none, scoring every resistance-held base, town and city by population, proximity to occupying-held bases, radio-tower coverage and — for towns — collapsed support. Selection is deliberately omniscient and deliberately *predictable*: an experienced player should be able to guess the target.
2. **Runs the phase machine.** Phase 1 sends harassment groups into towns, specops-grade teams to retake radio towers, and sabotage teams into bases to demolish what the resistance built. Phase 2 raises an OF forward operating base between its nearest held base and the objective. Phase 3 fires the counter-QRF through the existing QRF controller.
3. **Biases the deployment evaluator** toward the objective, by pushing a single nullable *anchor* (position + radius + weight) into `OVT_DeploymentManagerComponent`, which folds it into the candidate score it already sorts by.

Everything the director builds is a deployment. Three new behaviour modules (town harassment, tower recapture, base sabotage), one new condition module (objective scoping), one new general-purpose **live-truck insertion spawning module** and its FOB-raising subclass, six new configs, two faction registry entries and one authored map marker are the entire build surface. The director never spawns an entity itself except the FOB's structure, and never holds money — its "FOB budget" is a spend ceiling counted against the one deployment pool, so the conserved-total identity `base-defense-migration` established still holds after this feature.

Three legacy triggers are deleted **first**, in Phase 1, before anything replaces them: the hourly random counter-attack roll, the town-suppression QRF, and the `counterAttackTimeout` difficulty field with all four of its authored values. v1.5 is unreleased and the user has accepted temporary OF passivity in dev play-tests; nothing else in the campaign depends on either trigger, and both player-initiated battle paths (`OVT_CampaignRequestComponent.c:177` base capture, `OVT_UprisingRequestComponent.c:92` uprising) are untouched.

The QRF controller keeps its job and changes in exactly two places: the FOB joins its wave-source list, and each wave's landing zone is biased toward the bearing of the source that sent it, so an attack visibly comes from where the occupying faction actually is.

**Expected shape:** strongly net-adding (unlike its predecessor) — roughly 12 new script files, 8 new configs, 1 new prefab, 1 new entity class, 1 new serializer, ~2 500 lines added against ~60 deleted.

---

## 2. Goals

### Primary

- **G1 — One intent, three legible phases.** At any moment the occupying faction has at most one objective, in exactly one phase, and both are inspectable on the GM Overthrow panel. `grep` finds no second decision-maker: nothing outside `OVT_ObjectiveDirectorComponent` starts an offensive operation.
- **G2 — The legacy triggers are gone.** `grep -rn "counterAttackTimeout\|m_bCounterAttackTimeout" Scripts/ Configs/` returns **nothing**. `StartBaseQRF` and `StartTownQRF` keep exactly two callers each after this feature: the player-initiated one, and the director.
- **G3 — The ramp gives warning.** Between the first harassment group arriving at an objective and the counter-QRF landing there are, on Normal, tens of in-game minutes of visible activity: groups arriving by truck, a support slide, tower flips, structures demolished, and an enemy FOB standing in the open.
- **G4 — Everything is a deployment.** Every group this feature creates is created by a deployment module and registered with the virtualization core through it. `git diff Scripts/Game/GameMode/Virtualization/` is **empty** for the whole feature; `docs/features/virtualization/core/api.md` gains no signature.
- **G5 — The pool still balances.** The director never holds resources. Every resource it spends leaves `OVT_DeploymentManagerComponent.m_mFactionResources` exactly once, through `SubtractFactionResources`, at the moment the deployment is created. The FOB "budget" is a **ceiling counter**, not a wallet. No third funding path exists.
- **G6 — The whole objective survives a save.** Objective, phase, all timers, both success counters, the blacklist and the full FOB record round-trip through a **new, version-first serializer of the director's own**. The fragile positional `OVT_OccupyingFactionManagerSerializer` payload is **not** extended and does not move.
- **G7 — Insertion is real.** Phase 1+ groups arrive from a controlled base in a live truck that drives real roads, drops them at a road landing zone short of the target, and goes home — with a stuck fallback that dismounts and walks, and a hard cap on concurrent live insertions.
- **G8 — The QRF changes are two lines of intent, not a rewrite.** `git diff Scripts/Game/Controllers/OccupyingFaction/OVT_QRFControllerComponent.c` touches only `SendTroops`' source list, `SendWave`'s call into `GetLandingZone`, and `GetLandingZone`'s signature and preferred-direction derivation. Countdown, budgeting, scoring, waypoints and resolution are byte-identical.

### Secondary

- **G9 — The GM panel tells the truth.** The objective's name and phase appear as two new rows in the GM panel's `CampaignSection` (beside threat and resources — see [D13](#d13--the-gm-objective-fields-are-campaignsection-rows-not-the-detail-slot) for the deliberate divergence from the requirements' `DetailSection` pointer), delivered on the existing `OVT_GMSnapshotBuilder` → `OVT_GMCampaignState` fan. No record class is reordered; one RPC is appended.
- **G10 — The occupying faction can take a radio tower back again.** C1's regression is closed. A resistance-held tower within range of the objective draws a specops-grade team that must reach it and *hold* it, and only then does it flip.
- **G11 — Difficulty scales the whole ramp, and the sabotage gate scales inverted.** Easier difficulties demand **more** successful sabotage missions before the QRF is allowed, so a new player gets more warning; Insane demands two.
- **G12 — Nothing new replicates except one GM record.** No `RplProp`, no client-visible surface beyond the GM panel, one new user action and one new server-validated request. The FOB is deliberately **not** announced to the resistance.

### 2.1 Quality Bar — the hard floor

This is a **backend / AI-systems** feature. There is no UI polish axis; the bar is reliability, accounting integrity, deterministic tests and legibility of the ramp.

| Bar | What it means concretely | How it is caught |
|---|---|---|
| **Reliability — the machine never wedges** | The objective can always make progress or give up. Every phase has an exit: a timeout, a starvation rule, a blacklist-and-reselect on failure, and a hard reset after the QRF whatever its outcome. A director that has been running for six campaign hours with no objective, or one stuck in Phase 2 with no FOB, is a defect. | §6 F5/F9/F13; Init case "every phase has a reachable exit"; the WARNING log on every reset naming the reason |
| **Data integrity — the pool balances** | The sum of `m_iResources` + the deployment pool + what has been spent on live deployments is conserved across every director action. `m_iFOBSpent` is a counter that never holds money. This epic's history is broken bookkeeping (BUG-026/027/029) and the migration that just fixed it must not be undone. | §6 F10/Q6; Logic-tier ceiling maths; Init-tier "spend leaves the pool exactly once"; the `AddFactionResources` grep |
| **Data integrity — the save round-trips** | Objective, phase, both counters, every timer, the blacklist and the FOB record come back exactly as they were, on a **Continue** and on an in-session re-apply. A live QRF still deliberately rolls back. | §6 F11; the Persistence-tier case; the version-first serializer |
| **Determinism of tests** | Every new case is world-free or seam-driven, carries a recorded can-fail proof, and uses **no `maxAttempts`**. Nothing asserts on a live AI reaching a place. Randomness enters only through `s_AIRandomGenerator`, and no assertion depends on a roll. | §7; Q3 |
| **Legibility of the ramp** | A player who has never read this document can tell, from the game, that something is building: trucks arriving, support sliding, structures gone, a flag they did not plant. Each phase transition is either visible in the world or announced. **The FOB is the deliberate exception and stays silent.** | §6 F2/F3/F6/F7; play-test steps 3–9; Phase 9 |
| **The frozen neighbours stay frozen** | `git diff Scripts/Game/GameMode/Virtualization/ Scripts/Game/GameMode/VirtualMovement/` empty every phase; `Deployment_Base*.conf`, `Deployment_TownPatrol.conf`, `Deployment_TowerGarrison.conf` and both vehicle patrols byte-identical. | Acceptance criterion on all eight code phases |

---

## 3. Architecture Overview

### 3.1 Division of labour

```
SERVER ONLY, except one appended GM snapshot record. Core is NOT touched.

OVT_ObjectiveDirectorComponent                 Scripts/Game/GameMode/Objectives/   ← THE NEW BRAIN
  game-mode component, OVT_Component, server-only, ticked at the OF cadence
├─ m_Objective       : OVT_ObjectiveRecord      kind, position, name, phase, timers, counters
├─ m_FOB             : OVT_ObjectiveFOBRecord   position, source base, spent, starvation, deployment key
├─ m_aBlacklist      : array<ref OVT_ObjectiveBlacklistEntry>
├─ SELECTS   ─ scores every resistance-held base/town/city, picks one, or idles
├─ RUNS      ─ the phase machine; creates ops through the deployment manager
├─ BIASES    ─ pushes ONE anchor into OVT_DeploymentManagerComponent          (D5)
├─ TRIGGERS  ─ StartBaseQRF / StartTownQRF at Phase 3                          (D8)
├─ FREEZES   ─ every timer stops while OVT_OccupyingFactionManager.m_CurrentQRF is set   (D4)
└─ PERSISTS  ─ its OWN version-first serializer; the OF serializer is untouched (D6)

PURE STATICS (world-free, Logic-tier, the OVT_DeploymentSelection shape)
├─ OVT_ObjectiveSelection      scoring + pick + blacklist arithmetic
├─ OVT_ObjectivePhaseRules     phase gates, harassment ramp, starvation, difficulty scaling
├─ OVT_InsertionGeometry       walk-vs-drive, LZ point on the source→target line, stuck test
└─ OVT_QRFBearing              compass bearing from wave source to target        (Phase 8)

DEPLOYMENT FRAMEWORK — extended additively, never forked
  Scripts/Game/GameMode/Deployments/
├─ OVT_DeploymentManagerComponent
│    + SetObjectiveAnchor(faction, position, radius, weight) / ClearObjectiveAnchor(faction)   (D5)
│    + the anchor folded into EvaluateFactionDeployments' candidate score (:615-627)
│    + TryReserveInsertion(faction) / ReleaseInsertion(faction) + m_iMaxConcurrentInsertions   (D7)
├─ Modules/ (NEW)
│    ├─ OVT_InsertionSpawningDeploymentModule : OVT_InfantrySpawningDeploymentModule   (D7)
│    │     ├─ ref OVT_DeploymentSourceProvider m_Source          ← the modder seam
│    │     │    ├─ OVT_NearestControlledBaseSourceProvider   (default)
│    │     │    └─ OVT_ObjectiveAnchorSourceProvider         (FOB first, then base)
│    │     └─ virtual OnInsertionArrived(vector lz)              ← the FOB raiser's hook
│    ├─ OVT_FOBRaiseSpawningDeploymentModule : OVT_InsertionSpawning…   raises the FOB on arrival
│    ├─ OVT_TownHarassmentBehaviorDeploymentModule      reach + hold centre → stacking support debuff
│    ├─ OVT_TowerRecaptureBehaviorDeploymentModule      reach + hold tower → ChangeRadioTowerControl
│    ├─ OVT_BaseSabotageBehaviorDeploymentModule        hold base → demolish buildables smallest-first
│    └─ OVT_ObjectiveConditionDeploymentModule          "only at the current objective, only in phase N"
└─ (nothing else in the framework changes)

CONFIGS (all additive)
├─ Configs/Deployment/Deployment_ObjectiveHarassment.conf     town/city, Phase 1
├─ Configs/Deployment/Deployment_ObjectiveTowerRecapture.conf radio tower near objective, Phase 1
├─ Configs/Deployment/Deployment_ObjectiveSabotage.conf       base, Phase 1
├─ Configs/Deployment/Deployment_ObjectiveFOB.conf            the FOB itself, Phase 2
├─ Configs/Deployment/Deployment_ObjectiveFOBGarrison.conf    FOB garrison, Phase 2
├─ Configs/Deployment/overthrowDeployments.conf               five appended registry entries
├─ Configs/Factions/{USSR,US}_OverthrowData.conf              specops_team + truck_crew
├─ Configs/Modifiers/supportModifiers.conf                    ONE APPENDED entry (index is positional!)
└─ Configs/Difficulty/*.conf (6)                              counterAttackTimeout deleted, 11 fields added

OCCUPYING FACTION MANAGER                     (retirement + two small additions)
├─ m_bCounterAttackTimeout (:176), its decrement (:1349-1352), the roll (:1418-1432)   DELETED
├─ the town-suppression QRF (:1453-1469)                                               DELETED
│    ⚠ threat decay at :1443-1451 lives in the SAME block and STAYS
├─ + array<OVT_BaseData> GetBasesControlledBy(int factionIndex)      the director's enumerator
├─ + array<OVT_RadioTowerData> GetRadioTowersAffecting(vector pos)   de-duplicates the town manager's
│                                                                     inline loop; uses OVT_InfluenceRules
└─ StartBaseQRF / StartTownQRF / m_CurrentQRF / income / threat / JIP / serializer  UNTOUCHED

QRF CONTROLLER                                (exactly two changes)                    (D9)
├─ SendTroops()   :204-213  + the OF FOB position joins m_Bases as a wave source
└─ SendWave()     :275      GetLandingZone(sourcePos) — bearing derived from real geometry

GM                                            (one appended campaign record)          (G9, D13)
├─ OVT_GMRequestComponent  + SendCampaignObjective/RpcDo_CampaignObjective pair
│                          + CAMPAIGN_RECORD_COUNT 2 -> 3 (:93), WIRE_VERSION 1 -> 2 (:71)
├─ OVT_GMSnapshotBuilder   + one read-only call into the director's PURE getters (its :5-13 hard rule)
├─ OVT_GMCampaignState     + two fields, APPENDED — and CopyFrom (:172), CopyRecords (:196)
│                            and Clear (:226) all edited, which is the standing trap
└─ GMPanel.layout + OVT_GMPanelUIComponent   two new rows in CampaignSection      (D13)

NEW WORLD CONTENT
├─ Scripts/Game/Components/OVT_FOBPositionComponent.c + Scripts/Game/Entities/OVT_FOBPosition.c
│    + Prefabs/GameMode/OVT_FOBPosition.et         authored FOB sites (the OVT_SniperPosition shape)
├─ Prefabs/Bases/OVT_OccupyingFOB.et               the FOB structure, with the held action authored on it
└─ Scripts/Game/UserActions/OVT_DismantleEnemyFOBAction.c

NOT BUILT, DELIBERATELY (the requirements' Out of Scope, restated as a build constraint)
├─ OF counter-operations against resistance FOBs
├─ physical/interdictable resupply trucks (starvation stays abstract)
├─ QRF waves arriving by the insertion system
├─ resistance-side intel surfaces for enemy FOBs
└─ any client UI beyond the GM panel's two fields
```

### 3.2 The objective state machine

```
                    ┌──────────────────────────────────────────────────────────┐
                    │                         IDLE                             │
                    │  no objective. Entered at campaign start, after every     │
                    │  reset, and whenever selection finds no candidate         │
                    │  (early game: the resistance holds nothing).              │
                    └───────────────────────────┬──────────────────────────────┘
                        selection finds a candidate │
                                                    ▼
   ┌────────────────────────────────────────────────────────────────────────────────┐
   │ PHASE 1 — HARASSMENT                        (the only phase that may re-select) │
   │                                                                                │
   │  TOWN/CITY objective          BASE objective                                   │
   │  ├ harassment groups, size    ├ sabotage teams infiltrate, hold with no enemy   │
   │  │  ramping with each success │  near, then demolish buildables SMALLEST FIRST  │
   │  │  → hold the centre → a     └ each completed mission: m_iSabotageSuccesses++  │
   │  │  STACKING support debuff                                                    │
   │  └ tower-recapture teams for any resistance-held tower covering the objective  │
   │                                                                                │
   │  Every group arrives by live truck from a controlled base (or the FOB).        │
   │  Cadence, group size and concurrency are difficulty-scaled.                    │
   └───────────┬──────────────────────────────────────────────────┬────────────────┘
   town: support < 50 %  │                    base: sabotage ≥ 1  │
                         ▼                                        ▼
   ┌────────────────────────────────────────────────────────────────────────────────┐
   │ PHASE 2 — FOB                                       (objective is now LOCKED)   │
   │  site = authored OVT_FOBPosition in the band, else a generated clear/flat/      │
   │         elevated/near-road point, always clear of resistance FOBs, bases and    │
   │         resistance-held towns and villages.                                     │
   │  no site at all  → RESET + blacklist this objective for ONE selection round     │
   │  site found      → a live supply truck drives out and RAISES the FOB + one free │
   │                    garrison, then goes home. NO NOTIFICATION to the resistance. │
   │  the FOB becomes the insertion source for further Phase 1 operations, spending  │
   │  against a CEILING inside the deployment pool.                                  │
   │                                                                                │
   │  STARVED (source base lost / no garrison / strong resistance presence) for      │
   │  objectiveStarvationMinutes  → teardown, abandon, RESET                         │
   │  player clears the area and holds the flag action → teardown, pool penalty, RESET│
   └───────────┬────────────────────────────────────────────────────────────────────┘
   town: support < 25 %  |  base: sabotage ≥ objectiveSabotageMissionsRequired
   AND the FOB is up  AND m_iResources >= objectiveQRFResourceGate
                         ▼
   ┌────────────────────────────────────────────────────────────────────────────────┐
   │ PHASE 3 — COUNTER-QRF                                                          │
   │  StartBaseQRF(baseController) or StartTownQRF(town). The existing controller    │
   │  resolves the battle. The FOB is one of its wave sources; LZs favour the        │
   │  bearing of whichever source sent that wave.                                    │
   │  On m_OnFinished, WHATEVER the outcome: FOB torn down, objective RESET.         │
   └────────────────────────────────────────────────────────────────────────────────┘

RE-SELECTION TRIGGERS (Phase 1 and IDLE only — Phase 2+ is locked, per the requirements)
  • OVT_OccupyingFactionManager.m_OnBaseControlChanged  (:185, fired :1613)
  • OVT_TownManagerComponent.m_OnTownControlChange      (:147, fired :682)
  ⚠ BOTH handlers only RAISE A DIRTY FLAG. OnBaseControlChange fires BEFORE the faction is
    applied (OVT_BaseControllerComponent.c:174-179 invokes, THEN sets the affiliation), so a
    handler that reads ownership inline sees the OLD owner. Re-selection runs on the next
    director tick, never inside the handler.                                             (D3)
```

### 3.3 The director's tick, and what freezes

The director installs one repeating `CallLater` in its own `PostGameStart()`, at the **same cadence the OF manager uses** — `OF_UPDATE_FREQUENCY (60 000) / timeMul`, i.e. once per in-game minute (`OVT_OccupyingFactionManager.c:178, :400`). Reusing that cadence rather than inventing one means the phase machine and the resource tick advance in step, and both freeze together.

```
OnDirectorTick():
    if (!Replication.IsServer())                       return
    if (GetGame().GetPlayerManager().GetPlayerCount() == 0)  return   // parity with CheckUpdate:1361
    if (OVT_Global.GetOccupyingFaction().m_CurrentQRF)       return   // THE FREEZE            (D4)

    if (m_bReselectPending && phase is IDLE or HARASSMENT) SelectObjective()

    switch (phase)
        IDLE        → SelectObjective()
        HARASSMENT  → TickHarassment()   // op cadence, counters, gate check
        FOB         → TickFOB()          // raise progress, starvation, gate check
        COUNTER_QRF → TickCounterQRF()   // nothing to do; the QRF freeze covers it
```

⚠ **Every timer in the director is a tick counter, not a wall-clock deadline.** `m_iPhaseTicks`, `m_iNextOpTicks`, `m_iStarvationTicks` are integers decremented once per tick. An early return therefore freezes them *by construction* — there is no deadline to fall behind, and "phase progression pauses and all objective timers freeze" is a property of the shape rather than a rule someone has to remember. It is also what makes them trivially serializable and Logic-testable. ([D4](#d4--tick-counters-not-wall-clock-deadlines))

### 3.4 Objective selection

Candidates are **resistance-held bases, towns and cities only**. Villages (`OVT_TownSize.VILLAGE`, `size == 1`) are excluded, as are FOBs and radio towers — the requirements are explicit that towers are handled *within* an objective and villages fall as collateral.

| Input | Where it comes from | Weight intent |
|---|---|---|
| Population | `OVT_TownData.population` (towns); a fixed base weight for bases | The bigger the prize, the more it is worth |
| Distance to the nearest OF-held base | `OVT_OccupyingFactionManager.GetBasesControlledBy(occupying)` (new, [§3.1](#31-division-of-labour)) | Close targets are reachable; distant ones are not worth a supply line |
| Radio-tower coverage | `OVT_InfluenceRules.IsProximitySource(objective, tower.location, difficulty.radioTowerRange)`, honouring `tower.IsDisabled()` | An objective the OF can still broadcast over is easier to hold |
| Support (towns only) | `OVT_TownData.SupportPercentage()` | A support-collapsed town replaces the retired suppression trigger as a *high-weight objective*, exactly as the requirements ask |
| Threat (bases only) | `OVT_OccupyingFactionManager.GetThreatByLocation(pos)` | Where the resistance is active is where the fight is |
| Blacklist | The director's own record | A site-less objective sits out one selection round |

The arithmetic is a **pure static**, in the `OVT_DeploymentSelection` shape:

```
class OVT_ObjectiveSelection            // Scripts/Game/GameMode/Objectives/OVT_ObjectiveSelection.c
  static const int NOTHING_TO_SELECT = -1
  static float ScoreTown(int population, int supportPercentage, float distanceToNearestHeldBase,
                         float maxUsefulDistance, bool hasTowerCoverage)
  static float ScoreBase(int threat, float distanceToNearestHeldBase,
                         float maxUsefulDistance, bool hasTowerCoverage)
  static int   SelectBestIndex(array<float> scores, array<bool> blacklisted)
  static bool  IsBlacklisted(array<string> keys, array<int> roundsLeft, string key)
  static void  DecayBlacklist(inout array<int> roundsLeft)
```

⚠ **`ScoreTown`/`ScoreBase` take *numbers*, never a `OVT_TownData` or an `OVT_BaseData`** — the "already-present set is an argument" rule from `OVT_DeploymentSelection.c:19-22`. The director does every world query and hands the answers in. The Logic-tier grep matches comments too, so those class names may not appear in the file at all, including in prose.

**Predictability is a design constraint, not an accident.** Weights are named constants with their rationale in the header, the score is a plain weighted sum with no hidden randomness, and ties break on input order (registry/world order). There is **no jitter** in objective selection — the deployment evaluator's ±20 % jitter exists to spread routine work, but an objective that moves for no reason is exactly what this feature exists to end.

### 3.5 The objective anchor — how the bias plugs into evaluation scoring

The requirements ask that "all deployment systems prioritize the current objective (a scoring input + position anchor in the deployment manager's evaluate→score→create loop)". The seam is **one nullable anchor per faction, pushed by the director**:

```
OVT_DeploymentManagerComponent
  + void SetObjectiveAnchor(int factionIndex, vector position, float radius, float weight)
  + void ClearObjectiveAnchor(int factionIndex)
  protected ref map<int, ref OVT_DeploymentObjectiveAnchor> m_mObjectiveAnchors;
```

and in `EvaluateFactionDeployments` (`:615-627`), one line inside the loop that already builds `OVT_CandidatePosition`:

```
BEFORE                                                AFTER
base   = CalculateThreatLevel(position, faction)      base   = CalculateThreatLevel(position, faction)
jitter = RandFloatXY(-0.2, 0.2)                       jitter = RandFloatXY(-0.2, 0.2)
final  = base * (1.0 + jitter)                        final  = base * (1.0 + jitter)
                                                      final  = OVT_ObjectiveSelection.ApplyAnchorBias(
                                                                   final, distanceToAnchor, radius, weight)
candidatesWithThreat.Insert(new OVT_CandidatePosition(position, final))
```

with

```
static float ApplyAnchorBias(float score, float distanceToAnchor, float radius, float weight)
    // radius <= 0 or weight <= 0 or distance >= radius  → score unchanged (NO ANCHOR = TODAY'S BEHAVIOUR)
    // otherwise score + weight * (1 - distance / radius)      — linear falloff, never negative
```

Three properties this shape buys, and they are the reason for it:

1. **No anchor is byte-identical to today.** A campaign with no objective, a faction with no director, and every Init/Persistence world that never starts one all take the `radius <= 0` branch. The four shipped non-base configs and the nine base configs behave exactly as they did.
2. **The deployment manager does not know the director exists.** No `OVT_Global` call, no include, no null-check on a component that may not be there. The dependency points one way: director → manager.
3. **The bias is Logic-testable in isolation** — it is a pure function of four floats, so "an objective-adjacent candidate outsorts a distant higher-threat one, but only up to `weight`" is a Fast-tier assertion.

⚠ **The anchor biases ordering, not eligibility.** It never changes which configs are *suitable* at a position, never bypasses `m_iMaxDeploymentsPerFaction`, and never raises `MAX_DEPLOYMENTS_PER_EVALUATION`. A saturated map still cannot buy more; it just buys near the objective first.

**The director's own operations do not go through the evaluator at all.** Harassment, sabotage, tower-recapture and the FOB are created explicitly with `ForceCreateDeployment(config, position, faction, cost, threat)` (`:1612`), because the director owns their cadence and their ramp and the evaluator's threat sort would make both unpredictable. ⚠ **`ForceCreateDeployment` does not debit the pool** — it forwards to `CreateDeployment`, which only *stamps* `m_iResourcesInvested`. The director must call `SubtractFactionResources` itself, in the same method, immediately after a successful create. This is the single highest-value line in the feature for [G5](#primary) and it gets its own Init-tier assertion.

### 3.6 The new modules, configs, registry and difficulty additions

#### `OVT_InsertionSpawningDeploymentModule` — the reusable live-truck insertion

Subclasses `OVT_InfantrySpawningDeploymentModule` for the same decisive reason `base-defense-migration` recorded in its D6: `OVT_ReinforcementBehaviorDeploymentModule.GetMissingUnitsCount()` casts to that class and returns 0 for anything else, so a sibling class silently never rebuys.

```
[Attribute] ref OVT_DeploymentSourceProvider m_Source;     // the modder seam; default = nearest held base
[Attribute] float  m_fWalkThresholdDistance;   // 400  — below this, no truck: spawn at source and walk
[Attribute] string m_sTruckVehicleType;        // "truck"        (faction VEHICLE registry)
[Attribute] string m_sTruckCrewGroup;          // "truck_crew"   (faction GROUP registry)
[Attribute] float  m_fLZStandoffDistance;      // 300  — how far short of the target to drop
[Attribute] float  m_fStuckSpeedThreshold;     // 1.0 m/s
[Attribute] int    m_iStuckTicks;              // ticks below threshold before dismount
[Attribute] float  m_fArrivalRadius;           // 40
[Attribute] int    m_iTruckCostOverride;       // added to GetResourceCost()

protected virtual void OnInsertionArrived(vector lzPosition)   // ← the FOB raiser's hook; base = no-op
```

Behaviour, in the order the module runs it:

1. `EnsureGroups()` resolves the **source** through `m_Source.ResolveSource(deploymentPosition, factionIndex)`. No source → log a WARNING naming the config and do nothing (never spawn from thin air — that is the rule this module exists to enforce).
2. `OVT_InsertionGeometry.ShouldWalk(sourceToTargetDistance, m_fWalkThresholdDistance)` → walk: register the groups at the source with a plan that walks them in. This is the rare case and it is the *fallback for everything that goes wrong later*, so it is built first.
3. Otherwise ask the manager `TryReserveInsertion(factionIndex)`. Refused (cap reached) → fall back to walking **or** defer to the next tick, whichever the config says. Cap released on arrival, on abandonment and in `OnCleanup`. ⚠ Reservations are **runtime-only and must be zeroed on restore** — a leaked reservation permanently starves insertion.
4. Spawn the truck + crew exactly as `OVT_VehicleSpawningDeploymentModule` does — `SpawnEntity`, `RegisterGroup(..., spawnDistanceOverride = 100000, NORMAL)`, `PairCrew`, `SeatExistingAgents`. **`m_iSpawnDistanceOverride` stays at 100 000**: a dormant crew holding a route plan gets walked away from its truck by the movement tick (`OVT_VehicleSpawningDeploymentModule.c:12-18`). Register the *passenger* groups the same way and seat them in CARGO.
5. LZ = `OVT_InsertionGeometry.LZPointOnLine(source, target, m_fLZStandoffDistance)`, then snapped with `OVT_WorldUtils.FindNearestRoadSpawn(point, ROAD_SPAWN_MAX_DISTANCE, out pos, out angles)` (`Scripts/Game/Utilities/OVT_WorldUtils.c:259`, max 200 m). No road within range → use the raw line point.
6. Each `OnUpdate`, tick the convoy: distance to LZ, speed, `IsVehicleOperational`. On arrival → dismount, drop the passengers' `spawnDistanceOverride` back to the global default, hand them to the behaviour module's plan, call `OnInsertionArrived(lz)`, release the reservation, and send the truck home (a return waypoint, then release it on arrival or after a timeout).
7. `OVT_InsertionGeometry.IsStuck(...)` true, or the truck is destroyed/flipped → dismount **where they are** and walk. This is a state transition, not an error: the groups are already registered and simply lose their ride.

⚠ **`CloneModule()` must copy its own nine attributes *and* all thirteen inherited ones.** This is the standing trap that lost `m_fMaxCruiseSpeed` for a release; T4.8 asserts clone fidelity mechanically.

⚠ **`AIGroup.AddWaypoint()` does not take ownership.** A waypoint is an ordinary world entity (`OVT_InactiveRecruitGroupComponent.c:13`, `OVT_VirtualizationManagerComponent.c:737`). This module issues at least two per insertion — drive-out and return — and **every one must be deleted explicitly** when the convoy ends, is abandoned, or the module cleans up. A leaked waypoint per insertion is an entity leak that compounds over a campaign.

⚠ **There is no existing "drive from A to B, then despawn" behaviour anywhere in the tree.** The vehicle-patrol deployments patrol open-endedly; the civilian traffic system parks and discards but never drives. The parts exist (`SpawnVehicleMatrix`, `PairCrew`/`SeatAgent`, `OVT_OverthrowConfigComponent.SpawnMoveWaypoint(pos)` at `:489`, a poll on the module's own `OnUpdate`, `DeleteEntityAndChildren`) but the assembly is genuinely new — which is why this is its own phase and why the walk fallback is written first. ⚠ Note `OVT_OverthrowConfigComponent.SpawnGetInWaypoint(vector pos)` at `:507` appears to load `m_pGetOutWaypointBPrefab`; prefer the `SCR_EntityWaypoint SpawnGetInWaypoint(IEntity target)` overload at `:552` if a board-the-vehicle waypoint is needed, and record the suspected defect rather than fixing it here.

#### `OVT_FOBRaiseSpawningDeploymentModule : OVT_InsertionSpawningDeploymentModule`

One extra attribute (`ResourceName m_rFOBPrefab`) and one override:

```
override void OnInsertionArrived(vector lz)
    if (m_ParentDeployment.WasRestoredFromSave()) return          // D11 — never rebuild static content
    if (m_FOBEntity) return                                        // idempotent
    spawn m_rFOBPrefab at the deployment position (NOT the LZ — the LZ is where the truck stopped)
    OVT_PersistenceTracking.Track(entity)
    OVT_NavmeshRebuild.RebuildNow(entity)
    director.OnFOBRaised(m_ParentDeployment)
```

The FOB structure is therefore a **persistence-tracked world entity** that comes back from the save on its own, exactly like the base compositions — while the director's *record* of it comes back from the director's serializer. The two are re-linked lazily on the first tick after restore, never during deserialize ([D6](#d6--the-director-owns-its-own-version-first-serializer)).

#### The three behaviour modules

| Module | What it does | The testable split |
|---|---|---|
| `OVT_TownHarassmentBehaviorDeploymentModule` | While ≥1 registered member is alive within `m_fHoldRadius` of the town centre **and** no armed resistance is inside it, count down `m_iHoldTicks`. At zero: `OVT_Global.GetTowns().TryAddSupportModifierByName(townID, "ObjectiveHarassment")`, tell the director (`OnHarassmentSuccess`), and let the deployment be collected. | `bool EvaluateHold(int aliveInside, bool enemyPresent, inout int ticksLeft)` — pure, no world |
| `OVT_TowerRecaptureBehaviorDeploymentModule` | The **inverse** of `OVT_RadioTowerCaptureBehaviorDeploymentModule`. Nearest tower within `m_fMaxDistance` that the objective faction does **not** hold; while held by ≥1 alive member and no enemy near, count down `objectiveTowerRecaptureHoldSeconds` (default **600 s**, the retired specops timer); at zero call `ChangeRadioTowerControl(tower, occupyingIndex)` behind a `m_bCaptureFired` latch. | `bool EvaluateRecapture(bool holding, bool enemyPresent, int towerFaction, int myFaction, inout int ticksLeft)` |
| `OVT_BaseSabotageBehaviorDeploymentModule` | While holding the base with **no enemy within `m_fClearRadius`**, count down; each expiry demolishes **one** structure, smallest first, and restarts the timer. Structures are entities carrying `OVT_PlaceableComponent` or `OVT_BuildableComponent` within the base radius, ordered ascending by their config entry's `m_iCost` (resolved via `GetPlaceableType()`). After `m_iStructuresPerMission` or when nothing is left: `director.OnSabotageSuccess()`. | `int NextTargetIndex(array<int> costs, array<bool> alreadyDestroyed)` — pure |

⚠ The tower module's condition twin already exists and needs **no new code**: `OVT_RadioTowerControlConditionDeploymentModule` with `m_bRequireControl 0` means "deploy only while the faction does *not* control this tower", which is exactly the recapture gate, and paired with `m_bDeleteOnConditionFail 1` on the reinforcement module it collects the deployment the moment the tower flips. This is the closing loop that module's header (`:19-22`) documents, run backwards.

⚠ **Structure destruction must go through the same removal path the player's own dismantle uses**, not a raw `SCR_EntityHelper.DeleteEntityAndChildren` — otherwise the entity comes back on the next load because it is still persistence-tracked. Phase 6 opens with a read-only survey task to find and name that path before anything is deleted.

#### `OVT_ObjectiveConditionDeploymentModule`

A condition module carrying `m_iRequiredPhase` and `m_fMaxDistanceFromObjective`. `EvaluateStaticCondition` and `EvaluateCondition` both answer "is this position within range of the current objective, and is the director in the required phase". It exists for two reasons: it makes objective operations self-collecting when the objective resets (paired with `m_bDeleteOnConditionFail 1`), and it makes the configs safe if the evaluator ever picks one up on its own.

#### Faction registry and difficulty additions

**Registry — two entries per faction**, appended to `Configs/Factions/{USSR,US}_OverthrowData.conf` with a fresh, grep-verified-unused GUID prefix (the tree already uses `6AB1C7D4000000xx` and `6A9C4D1B0000000A`):

| Registry | Name | Why |
|---|---|---|
| Group | `specops_team` | Tower-recapture and base-sabotage teams. Prefab: the faction's special-forces group prefab (the one `m_aGroupSpecialPrefabSlots` used to name before D9 of the migration retired it — recover it from git history rather than guessing). |
| Group | `truck_crew` | Two men to drive the insertion truck. |

The harassment ramp needs **no new entries** — it walks the existing `light_patrol` → `light_fireteam` → `rifle_squad` → `heavy_infantry` ladder by success count, which is precisely the "groups of increasing size" the requirements ask for and costs nothing to author.

**Difficulty** — `counterAttackTimeout` (`OVT_DifficultySettings.c:50-51`) is **deleted along with all four authored values** (Normal 100, Hard 80, Extreme 60, Insane 45) in the same commit; an authored value with no attribute is a parse warning on every load. Eleven fields replace it, all in the `Occupying Faction` category:

| Field | Default | Easy | Normal | Hard | Extreme | Insane |
|---|---|---|---|---|---|---|
| `objectiveHarassmentIntervalMinutes` | 45 | 90 | 60 | 45 | 30 | 20 |
| `objectiveHarassmentMaxConcurrent` | 2 | 1 | 2 | 2 | 3 | 4 |
| `objectiveHarassmentHoldSeconds` | 180 | 240 | 180 | 150 | 120 | 90 |
| **`objectiveSabotageMissionsRequired`** ⬅ **inverted** | 4 | **6** | **5** | **4** | **3** | **2** |
| `objectiveSabotageHoldSeconds` | 120 | 180 | 120 | 100 | 80 | 60 |
| `objectiveSabotageStructuresPerMission` | 2 | 1 | 2 | 2 | 3 | 3 |
| `objectiveTowerRecaptureHoldSeconds` | 600 | 900 | 600 | 480 | 360 | 300 |
| `objectiveFOBGarrisonMax` | 3 | 1 | 2 | 3 | 5 | 6 |
| `objectiveFOBCost` | 400 | 400 | 400 | 400 | 400 | 400 |
| `objectiveMaxConcurrentInsertions` | 2 | 1 | 2 | 3 | 4 | 4 |
| `objectiveStarvationMinutes` | 30 | 45 | 30 | 25 | 20 | 15 |
| `objectiveQRFResourceGate` | 1500 | 2000 | 1500 | 1200 | 1000 | 800 |

(Twelve rows; `objectiveFOBBudgetCeiling` is deliberately **not** a field — it is `objectiveFOBCost × 3`, derived in `OVT_ObjectivePhaseRules`, because a separate knob nobody tunes is a knob that goes stale. `Difficulty_TestWorld.conf` authors none of these and inherits every default.)

⚠ **The inverted scaling is authored, not computed** — see [D10](#d10--the-inverted-sabotage-scaling-is-authored-and-the-invariant-is-asserted). The pure static consumes and clamps it; an **Init-tier** case asserts the ordering across the five shipped presets, because that is the only tier that can see loaded configs.

### 3.7 The FOB

| Concern | Decision |
|---|---|
| **Represented as** | A **deployment** (`Deployment_ObjectiveFOB.conf`) created by the director at the chosen site, whose `OVT_FOBRaiseSpawningDeploymentModule` puts the authored `Prefabs/Bases/OVT_OccupyingFOB.et` structure in the world and registers the free garrison. This buys pool accounting, virtualization, reinforcement, GM tagging and marker persistence for free. ([D11](#d11--the-fob-is-a-deployment-plus-a-tracked-structure-not-a-new-entity-system)) |
| **Site: authored** | `OVT_FOBPositionComponent` + `OVT_FOBPosition : GenericEntity` + `Prefabs/GameMode/OVT_FOBPosition.et`, following **`OVT_SniperPosition`** (`Scripts/Game/Components/OVT_SniperPositionComponent.c`, `Scripts/Game/Entities/OVT_SniperPosition.c`, `Prefabs/GameMode/OVT_SniperPosition.et`) rather than `OVT_VehiclePatrolSpawn`: the queryable thing is the **component**, so the marker can be attached to anything, and discovery is the `QueryEntitiesBySphere` + `FindComponent` filter that `OVT_SniperMarkerPlacementProvider.c:39-107` already proves. Preferred whenever one exists. ⚠ **No authored instances will exist on day one** — they live in world layers (`Worlds/MP/OVT_Campaign_Eden_Layers/misc.layer`) and placing them is a Workbench world-editing job, so [R17](#9-risks--mitigation) applies and the generated fallback must be good enough to ship alone. |
| **Site: generated** | Bounded-attempt sampling on the source-base→objective line within the band: ocean-rejected, `TraceBox` clearance (the QRF LZ pattern at `OVT_QRFControllerComponent.c:483-494`, which is now correct), a flatness check over four probe points, and a preference for elevation and for being within `ROAD_SPAWN_MAX_DISTANCE` of a road. |
| **Site: exclusions** | Never within `m_fClearanceRadius` of a resistance FOB or camp (`OVT_ResistanceFactionManager.m_FOBs`), a base of any faction (`GetBasesWithinDistance`), or a resistance-held town or village (`OVT_TownManagerComponent`). The clearance test is a **pure static** taking a list of exclusion positions and radii. |
| **No site found** | Reset, blacklist the objective for **one** selection round, reselect. Logged at WARNING with the objective name and the attempt count — "the OF never attacks town X" must have an explanation in the log, not a repro. |
| **Budget** | A **spend ceiling**, not a wallet. `m_iFOBSpent` counts what the director has spent from the pool on FOB-sourced operations; the ceiling is `objectiveFOBCost × 3`. Nothing is reserved, held or moved. |
| **Starvation** | Evaluated every tick from three inputs: the source base is still OF-held, it has ≥1 alive registered group, and no player is within `baseCloseRange`. Any failure increments `m_iStarvationTicks`; recovery zeroes it; `objectiveStarvationMinutes` reached tears the FOB down and abandons the objective with no penalty. |
| **Removal** | `OVT_DismantleEnemyFOBAction : ScriptedUserAction` on the FOB flag, shown only to the player faction, performable only when no OF AI is within the clear radius. ⚠ **The hold duration is authored on the prefab, not in script** — `Duration 15` inside the `additionalActions` block, exactly as `OVT_CaptureBaseAction` is wired on `Prefabs/Structures/Military/Flags/BaseFlag_US.et` and `OVT_SabotageTowerAction` on `TransmitterTower_01_base.et:51-59`. Routed through `OVT_CampaignRequestComponent` (which already owns the base-capture verb) and **re-validated server-side**, with refusals logged in the shape of `OVT_FOBRequestComponent.RejectFOBRequest` (`:593`). Tears the FOB down, debits the pool by `objectiveFOBRemovalPenalty` (= `objectiveFOBCost`, clamped at 0), and resets the objective. |
| **Notification** | **None**, deliberately, per the requirements. The Intel epic may surface it later. |

### 3.8 Persistence

A **new serializer of the director's own**, registered beside the others in the game-mode `ComponentSerializers` block of `Configs/Systems/Persistence/Overthrow.conf` (opens at `:23`, where `OVT_OccupyingFactionManagerSerializer` sits at `:36` and `OVT_DeploymentManagerSerializer` at `:40`), with a fresh GUID. The smallest complete model to copy is `OVT_DeploymentManagerSerializer.c` (105 lines) — its header states the contract this one inherits: *"Binary contexts are POSITIONAL: write order must equal read order. Version first."*, and *an absent payload must NOT clear live state* (`if (version < 1) return true;`).

```
class OVT_ObjectiveDirectorSerializer : ScriptedComponentSerializer
  override static typename GetTargetType()  → OVT_ObjectiveDirectorComponent

  WRITE (positional, version FIRST, append-only forever)
    1  int    version                  = 1
    2  int    objectiveKind            OVT_EObjectiveKind
    3  vector objectivePosition        the key — positions are what this epic already persists
    4  int    phase                    OVT_EObjectivePhase
    5  int    phaseTicks
    6  int    nextOpTicks
    7  int    harassmentSuccesses
    8  int    sabotageSuccesses
    9  array<vector> blacklistPositions
   10  array<int>    blacklistRounds
   11  bool   fobUp
   12  vector fobPosition
   13  vector fobSourceBasePosition
   14  int    fobSpent
   15  int    fobStarvationTicks
   16  string fobDeploymentName         re-link key, matched with GetDeploymentNearPosition

  READ   version-gated exactly as OVT_DeploymentComponentSerializer.c:136-142 does
         (`if (version < 1) return true;` then the fields in order), then ONE side-effecting call:
         director.ApplyPersistedObjective(...)
```

Three rules this shape enforces, each learned from a defect in this epic:

- **Deserialize is a pure codec.** It reads and calls one apply method. No world query, no deployment lookup, no pool arithmetic.
- **Nothing touches the resource pool during restore.** `OVT_OccupyingFactionManager.c:436-438` records that the deployment manager's restore **clears and refills the faction pool and runs after** the OF manager's — which is why the legacy refund is queued and credited in `PostGameStart()` at `:414`. The director follows the same pattern: any pool interaction is deferred to its first tick.
- **The FOB deployment is re-linked lazily.** `ApplyPersistedObjective` stores `fobDeploymentName` + `fobPosition`; the first director tick resolves it with `GetDeploymentNearPosition(name, position, radius)` (`OVT_DeploymentManager.c:1680`). If it does not resolve — the marker was destroyed while saved — the director tears the objective down and reselects, logging why. **Load order is never assumed.**

**A live QRF still rolls back**, exactly as today: nothing about `m_CurrentQRF` is persisted, and a director restored in `COUNTER_QRF` phase with no live QRF resets on its first tick.

⚠ `OVT_OccupyingFactionManagerSerializer` is **not** extended. It is version 2, fully positional, with no version field on its record classes; every field of `OVT_PersistedBase` is load-bearing by position. The epic's own tech-debt list names it as fragile, and this feature deliberately does not add to it.

---

## 4. Implementation Phases

Nine phases. Each leaves the tree compiling and the campaign playable. **No phase ships a module without the config that uses it, and no phase ships a config whose modules do not exist.**

**Test-run policy:** `tools/compile-check.sh` runs freely; `tools/run-tests.sh` launches a real Reforger client that steals desktop focus, so it is run **by the orchestrator only, once, after a phase completes** — never during planning, never inside a subagent. See `.claude/test-policy.md` for the full rules. Fast = `{6A6E29FF47ECB840}`, All = `{6A6E2A002F53A581}`.

---

### Phase 1 — Retirement and the difficulty rewire **(retire-first, per the user's decision)**

**Agent:** `component-developer` — standard. Contained deletions plus config authoring; nothing shared changes shape.
**Estimate:** 4–6 h
**Suite after this phase:** **All** `{6A6E2A002F53A581}` (it edits the OF economy tick and six difficulty configs).

**Tasks**

1. **T1.1 — Read-only survey first.** Re-verify and record in `context.md`: every reader of `m_bCounterAttackTimeout` (expected 3: `:176`, `:1351-1352`, `:1420`) and of `counterAttackTimeout` (expected 2: the attribute at `OVT_DifficultySettings.c:51` and `OVT_OccupyingFactionManager.c:1428`); every caller of `StartBaseQRF` (expected 2: `:1427` counter-attack, `OVT_CampaignRequestComponent.c:177` player capture) and `StartTownQRF` (expected 2: `:1466` suppression, `OVT_UprisingRequestComponent.c:92` uprising). If a concurrent session has added a caller, stop and re-check the design.
2. **T1.2 — Delete the counter-attack roll.** `OVT_OccupyingFactionManager.c:1418-1432`. ⚠ **`float rand = s_AIRandomGenerator.RandFloat01();` at `:1419` is computed unconditionally every tick and used only here** — it goes with the block. Delete `m_bCounterAttackTimeout` (`:176`) and its decrement (`:1351-1352`).
3. **T1.3 — Delete the town-suppression QRF.** `:1453-1469` **only**. ⚠ **Threat decay lives in the same `if(time.m_iMinutes == …)` block at `:1443-1451` and stays.** The two dead locals `playerFaction`/`occupyingFaction` at `:1453-1454` go with the loop. Leave the block shell intact.
4. **T1.4 — Retire `counterAttackTimeout`.** Delete `OVT_DifficultySettings.c:50-51` **and the four authored values in the same commit** (`Difficulty_Normal.conf:10`, `Hard.conf:15`, `Extreme.conf:15`, `Insane.conf:14`). An authored value with no attribute is a parse warning on every load.
5. **T1.5 — Author the eleven new difficulty fields** per [§3.6](#faction-registry-and-difficulty-additions), in the `Occupying Faction` category, each with a `desc:` a tuner can act on. Author the per-preset values in all five shipped presets; `Difficulty_TestWorld.conf` authors none.
6. **T1.6 — Re-word the two comments this invalidates.** `OVT_OccupyingFactionManager.c:1411-1412` says the remaining 20 % is "the QRF sizing **and counter-attack** reserve" — the counter-attack half is now gated by `objectiveQRFResourceGate` and belongs to the director. `:1394-1397` (`UpdateKnownTargets`) is still correct and stays.
7. **T1.7 — Logic-tier:** new `TestSuites/Logic/OVT_TEST_Logic_ObjectiveScaling.c` (Fast, `suite: OVT_TEST_LogicSuite`) covering the difficulty-consumption statics that exist at this point — `RequiredSabotageMissions(authored, fallback)` clamps a 0/negative/absurd authored value to the fallback and passes a sane one through; the harassment ramp maps success count → group-ladder index and saturates at the top rung. ⚠ World-free **including comments**.
8. **T1.8 — Init-tier:** the five shipped presets load, and `objectiveSabotageMissionsRequired` is **non-increasing** Easy → Normal → Hard → Extreme → Insane with at least one strict step ([G11](#secondary)). This is the only tier that can assert the inversion, because it needs loaded configs.
9. **T1.9 — `context.md`:** the T1.1 verdicts, and an explicit note that **the occupying faction has no offensive trigger at all between this phase and Phase 8** — accepted, v1.5 unreleased, per the user's retire-first decision.

**Acceptance criteria**

- `tools/compile-check.sh` exits **0**; All green.
- `grep -rn "counterAttackTimeout\|m_bCounterAttackTimeout" Scripts/ Configs/ Prefabs/` → **empty**.
- `grep -rn "StartBaseQRF\|StartTownQRF" Scripts/` → **exactly one caller each**, both player-initiated.
- `git diff` on `OVT_OccupyingFactionManagerSerializer.c`, `RplSave`, `RplLoad` → **empty**.
- Threat decay still runs: an Init case or the existing suite still observes `m_iThreat` falling.
- `ls Configs/Difficulty/*.conf | wc -l` → **6**, each parsing clean.

---

### Phase 2 — The director: state machine, selection, persistence **(GATE for phases 5–8)**

**Agent:** `component-developer-advanced` — **advanced.** A new persisted game-mode component with a load-order hazard, two event subscriptions with a documented ordering trap, and the state machine every later phase plugs into.
**Estimate:** 14–18 h
**Suite after this phase:** **All**.

**Tasks**

1. **T2.1 — The pure statics first**, so the phases below have something to call and the Logic tier has something to assert:
   - `OVT_ObjectiveSelection` — `ScoreTown`, `ScoreBase`, `SelectBestIndex`, `IsBlacklisted`, `DecayBlacklist`, `ApplyAnchorBias` (used in Phase 3 but authored here). Header carries the hard rule verbatim from `OVT_DeploymentSelection.c:4-6`.
   - `OVT_ObjectivePhaseRules` — `TownPhase2Gate(support)`, `TownPhase3Gate(support, fobUp, reserve, gate)`, `BasePhase3Gate(successes, required, fobUp, reserve, gate)`, `RequiredSabotageMissions(authored, fallback)`, `HarassmentLadderIndex(successes, rungs)`, `IsFOBStarved(sourceHeld, aliveGroups, playerPresent)`, `FOBBudgetCeiling(fobCost)`, `TickDown(value)`.
   ⚠ **No manager, world, entity or `OVT_Global` identifier in either file, comments included.** ⚠ `out` and `owned` are reserved local names. ⚠ `vector.Distance` is +1 ULP off at 1 000 m and 2 000 m — assert with tolerances.
2. **T2.2 — The records and enums.** `OVT_EObjectiveKind { NONE, TOWN, BASE }`, `OVT_EObjectivePhase { IDLE, HARASSMENT, FOB, COUNTER_QRF }`, `OVT_ObjectiveRecord`, `OVT_ObjectiveFOBRecord`, `OVT_ObjectiveBlacklistEntry` — plain `Managed` classes, the `OVT_FOBData` shape (`OVT_ResistanceFactionManager.c:22-38`). ⚠ **Enum integers travel in the GM snapshot from Phase 8 onward — never renumber them**, the rule `integration` recorded for `OVT_EGroupOrigin`.
3. **T2.3 — `OVT_ObjectiveDirectorComponent`.** `OVT_Component` subclass in `Scripts/Game/GameMode/Objectives/`. `OnPostInit` server-only + a static `GetInstance()` in the `OVT_DeploymentManagerComponent.GetInstance()` shape (`:86`). `Init(IEntity)` and `PostGameStart()` wired in `OVT_OverthrowGameMode.c` beside the others (`FindComponent` at `:1471-1490`, `PostGameStart` chain at `:353-390`) — **last in the chain**, so every manager it queries already exists. Add `OVT_Global.GetObjectiveDirector()` next to `GetDeploymentManager()` (`OVT_Global.c:242`). Declare the component on `Prefabs/GameMode/OVT_OverthrowGameMode.et`.
4. **T2.4 — The tick and the freeze.** `PostGameStart()` installs one repeating `CallLater` at `OF_UPDATE_FREQUENCY / timeMul`. The three early returns of [§3.3](#33-the-directors-tick-and-what-freezes), in that order, with the QRF freeze carrying a one-sentence comment naming the `m_CurrentQRF` singleton contract. **All timers are tick counters** ([D4](#d4--tick-counters-not-wall-clock-deadlines)).
5. **T2.5 — Selection.** `SelectObjective()`: enumerate resistance-held bases via the new `GetBasesControlledBy` and non-village resistance-held towns; per candidate gather the six inputs of [§3.4](#34-objective-selection); score through `OVT_ObjectiveSelection`; pick; set `HARASSMENT`; log the choice **and the runner-up with both scores** at INFO — predictability is a requirement and the log is how a tuner checks it. No candidates → `IDLE`, and say so once, not every tick.
6. **T2.6 — Two small additions to the OF manager**, both pure reads, both with headers: `array<OVT_BaseData> GetBasesControlledBy(int factionIndex)` beside `GetNearestOccupiedBase` (`:889`), and `array<OVT_RadioTowerData> GetRadioTowersAffecting(vector position)` using `OVT_InfluenceRules.IsProximitySource(...)` and honouring `IsDisabled()`. The second **de-duplicates the inline loop at `OVT_TownManagerComponent.c:275-285`** — re-point that loop at the helper in the same commit, or record why not.
7. **T2.7 — Re-selection triggers.** Subscribe `m_OnBaseControlChanged` (`:185`) and `m_OnTownControlChange` (`OVT_TownManagerComponent.c:147`). ⚠ **Both handlers set `m_bReselectPending = true` and return.** `OnBaseControlChange` fires *before* the affiliation is applied (`OVT_BaseControllerComponent.c:174-179`), so anything reading ownership inline reads the old owner. Re-selection happens on the next tick, and only in `IDLE`/`HARASSMENT`. ⚠ `ScriptInvoker.Insert` does not de-duplicate — `Remove` then `Insert`, and unsubscribe in the component's cleanup so a second campaign in one session does not double-fire.
8. **T2.8 — The reset path**, used by every failure and every ending: clear the anchor, delete every deployment the director created for this objective (tracked by config name + position through `GetDeploymentsInRadius`), clear the FOB record, optionally blacklist, go `IDLE`, log the reason. One method, one place, called by everything.
9. **T2.9 — `OVT_ObjectiveDirectorSerializer`** per [§3.8](#38-persistence) — version-first, positional, append-only, pure codec. Registered in the game-mode `ComponentSerializers` block of `Configs/Systems/Persistence/Overthrow.conf` (beside `:36`/`:40`) with a fresh GUID. `ApplyPersistedObjective(...)` is the single side-effecting entry point and **touches no pool and no deployment**; the lazy re-link is a tick-time job.
10. **T2.10 — Logic-tier** (extending T1.7's file): selection picks the highest score; ties go to input order; an all-blacklisted set selects nothing; an empty set selects nothing; `DecayBlacklist` never goes negative and drops entries at zero; every phase gate is asserted on both sides of its threshold; `IsFOBStarved` is true on each of its three inputs independently and false when all three are healthy.
11. **T2.11 — Init-tier:** the director resolves off the game mode via `GetInstance()` and `OVT_Global`; a fresh director is `IDLE` with a null objective; a driven `SelectObjective()` on a fixture set produces a deterministic pick; the tick early-returns while a QRF is live and **does not decrement any counter** (drive it by setting `m_CurrentQRF` on the OF manager and asserting the counters are unchanged). ⚠ **Init worlds never run `PostGameStart`** — the case installs the tick itself.
12. **T2.12 — Persistence-tier:** a full objective + FOB record round-trips on the shared gate (`OVT_TEST_PersistenceRoundTripSuite`, All group) — kind, position, phase, both counters, all three tick counters, a two-entry blacklist and every FOB field. Assert the **restore half** (`ApplyPersistedObjective`) with a real save taken alongside, exactly as `integration`'s T7.2–T7.4 and the migration's T4.8 do, and say so at the top of the fixture. 🔴 **Do not widen the reload seam** — `ReapplyLatestSaveData` builds its request with `Instances = {gameMode}` only.
13. **T2.13 — `context.md`:** the T2.1 hard-rule note, the two OF-manager helpers' verdicts, the invoker-ordering trap, and the load-order rule (§3.8) restated where the next implementer will read it.

**Acceptance criteria**

- compile **0**; All green.
- `git diff Scripts/Game/GameMode/Virtualization/ Scripts/Game/GameMode/VirtualMovement/` → **empty**.
- `git diff Scripts/Game/GameMode/Deployments/` → **empty** (the anchor lands in Phase 3).
- `git diff Configs/Deployment/` → **empty**.
- `grep -rn "OVT_Global\|GetGame()\|World\|Entity" Scripts/Game/GameMode/Objectives/OVT_ObjectiveSelection.c Scripts/Game/GameMode/Objectives/OVT_ObjectivePhaseRules.c` → **empty, comments included**.
- The director is declared exactly once on the game-mode prefab; `git diff Prefabs/` is that one addition.

---

### Phase 3 — The objective anchor in the deployment evaluator

**Agent:** `component-developer-advanced` — **advanced.** It edits the shared evaluator that thirteen shipped configs depend on. A mistake here changes where every deployment in the campaign is created.
**Estimate:** 4–6 h
**Suite after this phase:** **All**.

**Tasks**

1. **T3.1 — Read-only survey.** Record every writer of the candidate score (expected one: `EvaluateFactionDeployments:615-627`) and confirm `OVT_CandidatePosition.sortBy` still has exactly one producer. If a concurrent session added a second, re-check [§3.5](#35-the-objective-anchor--how-the-bias-plugs-into-evaluation-scoring).
2. **T3.2 — The anchor store and its API.** `OVT_DeploymentObjectiveAnchor` (position, radius, weight), `m_mObjectiveAnchors`, `SetObjectiveAnchor`, `ClearObjectiveAnchor`. Cleared on faction-list teardown so a second campaign in one session starts clean.
3. **T3.3 — One line in the scoring loop**, per [§3.5](#35-the-objective-anchor--how-the-bias-plugs-into-evaluation-scoring), calling `OVT_ObjectiveSelection.ApplyAnchorBias`. The method gets a doc block stating the two invariants: **no anchor is byte-identical to today**, and **the anchor biases ordering, never eligibility**.
4. **T3.4 — The director pushes.** `SetObjectiveAnchor` on selection and on every phase change (the radius grows with the phase: harassment ~600 m, FOB/QRF ~1 200 m so the FOB's own surroundings get garrisoned); `ClearObjectiveAnchor` on every reset. One place, in the reset method of T2.8.
5. **T3.5 — Logic-tier:** `ApplyAnchorBias` returns the input unchanged for `radius <= 0`, `weight <= 0`, and `distance >= radius`; rises monotonically as distance falls; never exceeds `score + weight`; never returns less than `score`; and a candidate at the anchor with a lower base score outranks a distant candidate whose base score is below `score + weight` but not one above it.
6. **T3.6 — Init-tier:** with no anchor set, `EvaluateFactionDeployments` produces the same candidate ordering as before (drive it twice and compare); with an anchor set, a candidate inside the radius sorts ahead of an equal-threat candidate outside it. ⚠ Scope every assertion to your own fixture positions — the Init/Persistence worlds run a live deployment wave.
7. **T3.7 — `context.md`:** the T3.1 verdict and the "ordering, not eligibility" invariant.

**Acceptance criteria**

- compile **0**; All green.
- `git diff Configs/Deployment/` → **empty**.
- Every existing deployment case still passes — the anchor defaults to absent.
- `grep -rn "OVT_ObjectiveDirector" Scripts/Game/GameMode/Deployments/` → **empty**. The dependency points one way.

---

### Phase 4 — The insertion module

**Agent:** `component-developer-advanced` — **advanced.** Live AI driving real roads, a vehicle+crew+passenger lifecycle, a shared concurrency counter on the deployment manager, and a fallback path that must work when everything else does not.
**Estimate:** 14–18 h
**Suite after this phase:** **All**.

**Tasks**

1. **T4.1 — `OVT_InsertionGeometry`** (pure): `ShouldWalk(distance, threshold)`; `LZPointOnLine(source, target, standoff)` — clamped so a standoff longer than the separation returns the source end, never a point beyond the target; `IsStuck(speed, speedThreshold, ticksBelow, ticksLimit, distanceToLZ, arrivalRadius)`; `HasArrived(distanceToLZ, arrivalRadius)`. Hard-rule header.
2. **T4.2 — `OVT_DeploymentSourceProvider`** — `[BaseContainerProps()]`, one virtual `bool ResolveSource(vector deploymentPosition, int factionIndex, out vector sourcePosition)` returning false rather than a zero vector, plus `GetProviderName()`. The `OVT_DeploymentPlacementProvider` shape (`Modules/OVT_DeploymentPlacementProvider.c:61-78`) exactly. Ship `OVT_NearestControlledBaseSourceProvider` (nearest base of the deployment's faction, via `GetBasesControlledBy`).
3. **T4.3 — Concurrency cap on the manager.** `[Attribute] int m_iMaxConcurrentInsertions` (default 2, authored from difficulty at `Init`), `TryReserveInsertion(int factionIndex)`, `ReleaseInsertion(int factionIndex)`, `ResetInsertionReservations()`. ⚠ **Reservations are runtime-only.** They are zeroed on restore and on faction-list teardown; a leaked reservation permanently starves insertion, so every exit path from the module releases, including `OnCleanup` and the abandon branch.
4. **T4.4 — `OVT_InsertionSpawningDeploymentModule`** per [§3.6](#36-the-new-modules-configs-registry-and-difficulty-additions). ⚠ `SCR_AIGroup.GetOnAgentAdded()` **passes ONE argument**, not two — the vanilla doc comment is wrong; recover the group with `agent.GetParentGroup()`. ⚠ `ScriptInvoker.Insert` does not de-duplicate — `Remove` then `Insert` on every reclaim. ⚠ Keep `spawnDistanceOverride = 100000` on the crew for the whole drive and drop the *passengers* back to the global default only after they dismount.
5. **T4.5 — The walk fallback is written first and is never optional.** Below the threshold, on a refused reservation, on a stuck truck, on a destroyed truck, and on a missing vehicle prefab, the groups still exist and still reach the objective on foot. A path that leaves men in a dead truck is a defect, not a degradation.
6. **T4.6 — `truck_crew` and `specops_team` registry entries**, both factions, **appended**, fresh grep-verified GUID prefix. Record in `context.md` which prefab each resolves to and where it came from (the special-forces prefab is recoverable from the pre-migration `m_aGroupSpecialPrefabSlots` values in git history).
7. **T4.7 — Logic-tier** (new `TestSuites/Logic/OVT_TEST_Logic_ObjectiveInsertion.c`): every `OVT_InsertionGeometry` function, including the degenerate cases — source == target, standoff ≥ separation, zero-length line, negative thresholds, and the stuck test's independence from distance when already inside the arrival radius.
8. **T4.8 — Init-tier:** both new registry names resolve to real prefabs for **both** factions (the case that catches a typo before five configs depend on it); the source provider returns false rather than null at a position with no friendly base; **the clone of the insertion module carries every one of its own and every inherited attribute** — the direct assertion against the `CloneModule` trap; the reservation counter rises on reserve, falls on release, and refuses past the cap.
9. **T4.9 — Fixture discipline.** ⚠ Any fixture that constructs this module is safe only if `SetSpawnedUnitsEliminated(true)` is set on the deployment **and every spawning module** before anything can tick — `InitializeDeployment` arms a repeating 8–12 s `UpdateDeployment` whose first tick would register real groups at a 100 000 m ring with the autotest camera inside it. Re-run `grep -rn "RegisterGroup(" Scripts/Game/Tests/` and record a verdict per site.
10. **T4.10 — `context.md`:** the prefab choices, the release-path audit (every exit that must release a reservation, listed), and the note that this module is **general-purpose and has no director dependency** — a future config can use it with the default provider.

**Acceptance criteria**

- compile **0**; All green.
- `git diff Configs/Deployment/` → **empty** (the module exists; nothing authors it yet).
- `grep -rn "OVT_ObjectiveDirector" Scripts/Game/GameMode/Deployments/Modules/OVT_InsertionSpawningDeploymentModule.c` → **empty**.
- The four shipped non-base configs and the nine base configs are **byte-identical**.

---

### Phase 5 — Phase 1 town operations: harassment and tower recapture

**Agent:** `component-developer` — standard. Two new behaviour modules, one condition module, two configs and one appended modifier entry. No shared system changes shape.
**Estimate:** 10–14 h
**Suite after this phase:** **All** (it appends a town-modifier entry, which is save-visible).

**Tasks**

1. **T5.1 — `OVT_ObjectiveConditionDeploymentModule`** per [§3.6](#36-the-new-modules-configs-registry-and-difficulty-additions). Both evaluations answer the same question; the module's header states that this is deliberate (unlike `OVT_NoPlayersNearbyConditionDeploymentModule`, whose asymmetry is the point) because an objective operation *should* be collected when the objective moves.
2. **T5.2 — The stacking support debuff.** ⚠ **`OVT_Modifier.m_iIndex` is the POSITIONAL index in `m_aModifiers`**, assigned in `OVT_TownModifierSystem.PostInit()`, and that index is what the replicated per-town modifier lists carry. **Append the new entry to the END of `Configs/Modifiers/supportModifiers.conf` and nowhere else** — inserting it anywhere earlier shifts every later index and corrupts every live save. Entry: `name "ObjectiveHarassment"`, a localized `title`, a hefty negative `baseEffect`, a long `timeout`, `stackLimit` matching the ramp cap, `flags` including `STACKABLE`, and a fresh GUID. ⚠ A handler is **required** even though this modifier has no behaviour — `PostInit` dereferences `config.handler` to assign the index — so ship `OVT_ObjectiveHarassmentSupportModifier : OVT_SupportModifier` with an empty `OnTick`, and say in its header that it exists to carry an index.
3. **T5.3 — `OVT_TownHarassmentBehaviorDeploymentModule`** per [§3.6](#36-the-new-modules-configs-registry-and-difficulty-additions), with `EvaluateHold` split out of `OnUpdate` so it is assertable without a live marker — the `OVT_RadioTowerCaptureBehaviorDeploymentModule.EvaluateCapture` pattern (`:85-130`), including its `m_bFired` latch and its rule that the latch is **not copied by `CloneModule`**.
4. **T5.4 — `OVT_TowerRecaptureBehaviorDeploymentModule`**, same shape, with the 600 s hold timer that `EvaluateCapture` does **not** have ([C5](#corrections-to-the-requirements-document)). Its flip calls `ChangeRadioTowerControl(tower, occupyingIndex)` (`OVT_OccupyingFactionManager.c:810`), which already sends both notifications.
5. **T5.5 — Author `Deployment_ObjectiveHarassment.conf`:** `OVT_InsertionSpawningDeploymentModule` (group type set by the director per ramp rung — author the baseline `light_patrol` and let the director create variant instances by config, see T5.7), harassment behaviour, reinforcement with `m_bDeleteOnConditionFail 1`, objective condition, `m_iAllowedLocationTypes TOWN`. ⚠ **Module order in a `.conf` is update order and `.conf` files cannot carry comments**: spawning first, then behaviour, reinforcement **last among behaviour modules**, then conditions.
6. **T5.6 — Author `Deployment_ObjectiveTowerRecapture.conf`:** insertion module with `specops_team`, recapture behaviour, reinforcement with `m_bDeleteOnConditionFail 1`, **`OVT_RadioTowerControlConditionDeploymentModule` with `m_bRequireControl 0`** (the existing inversion knob — no new condition code), objective condition, `m_iAllowedLocationTypes RADIO_TOWER`. ⚠ The recapture behaviour module must be authored **before** the reinforcement module, the same ordering constraint `OVT_RadioTowerCaptureBehaviorDeploymentModule`'s header (`:28-33`) records.
7. **T5.7 — The ramp.** The director escalates group size by creating the harassment config with a **group-type override per rung**. Decide and record which mechanism carries it: either four thin registry variants of the config (the `Deployment_BaseHeavyPatrol` precedent, simplest and consistent with the framework) or one config plus a director-side post-create override. **Prefer the variant configs** — the framework has no per-create override and inventing one is a parallel mechanism.
8. **T5.8 — Director wiring:** `TickHarassment()` creates an operation every `objectiveHarassmentIntervalMinutes` ticks up to `objectiveHarassmentMaxConcurrent`, choosing the rung from `HarassmentLadderIndex(m_iHarassmentSuccesses, rungs)`; creates a tower-recapture operation for each resistance-held tower returned by `GetRadioTowersAffecting(objective)`; and **debits the pool with `SubtractFactionResources` immediately after every successful `ForceCreateDeployment`** ([G5](#primary)). `OnHarassmentSuccess()` increments the counter and re-checks the Phase 2 gate.
9. **T5.9 — Init-tier:** both configs resolve and validate; the harassment config's plan is a movable one (the groups must reach the centre) while the recapture config's is whatever the tower module wants; `EvaluateHold` and `EvaluateRecapture` reach their fire condition from a driven sequence and **do not fire twice**; the clone of each new module carries every authored attribute; the new modifier resolves by name and its index is the **last** in the config.
10. **T5.10 — `context.md`:** the modifier-index append rule (restated where the next author will hit it), the T5.7 decision, and the ramp's rung table.

**Acceptance criteria**

- compile **0**; All green.
- `git diff Configs/Modifiers/supportModifiers.conf` shows **exactly one appended entry, at the end**.
- `grep -c "OVT_ModifierConfig" Configs/Modifiers/supportModifiers.conf` rises by exactly 1.
- `git diff Scripts/Game/GameMode/Virtualization/` → **empty**.
- Every existing town-modifier case still passes.

---

### Phase 6 — Phase 1 base operations: sabotage

**Agent:** `component-developer-advanced` — **advanced.** It destroys player property permanently, and it integrates two separate ownership registries whose removal path must be reused rather than reinvented.
**Estimate:** 10–14 h
**Suite after this phase:** **All**.

**Tasks**

1. **T6.1 — Read-only survey first, and it gates the rest of the phase.** Re-verify and record:
   - **The removal path.** `OVT_ResistanceFactionManager.RemovePlacedItem(RplId entityId, int playerId)` (`:870`) is the **only** removal API. Its two load-bearing lines, **in this order and no other** (the comment at `:896-898` explains why), are `OVT_NavmeshRebuild.Queue(entity)` then `SCR_EntityHelper.DeleteEntityAndChildren(entity)`. It sends no notification and performs no untracking — a deleted entity simply is not saved.
   - **🔴 The blocker: its owner-or-officer permission check at `:883-894` rejects a server-initiated call**, because a sabotage team has no `playerId`. The phase must therefore either add a `playerId == -1` server bypass to that method or extract the navmesh-queue-then-delete pair into a shared helper both paths call. **Decide and record which; do not copy the two lines into the sabotage module.**
   - **The cost join, which does not exist yet.** The live entity carries only a **type string** — `OVT_PlaceableComponent.GetPlaceableType()` / `OVT_BuildableComponent.GetBuildableType()` — and nothing in the tree joins that back to `OVT_Placeable.m_sName` / `OVT_Buildable.m_sName` to reach `m_iCost`. That lookup is new code and belongs in one place.
   - **The enumerator, which also does not exist.** There is **no registry of placed structures**; discovery is a sphere query. `OVT_ItemLimitChecker.CountItemsForLocation(locationId, baseType, searchCenter)` (`:216`, radius **500 for `EOVTBaseType.BASE`**) is the exact shape — with `FilterItemCallback` (`:244`, either component present) and `CountItemCallback` (`:279`, association match). The sabotage enumerator is that method **collecting instead of counting**.
   **Nothing is deleted until this table exists.**
2. **T6.2 — Reuse, do not reinvent.** Destruction goes through the single path T6.1 settled on. ⚠ Do not raw-delete: the navmesh carve must be captured **before** the entity goes, or the AI keeps pathing around a building that is not there.
3. **T6.3 — `OVT_BaseSabotageBehaviorDeploymentModule`** per [§3.6](#36-the-new-modules-configs-registry-and-difficulty-additions). Enumerate candidates in the T6.1 shape; filter to structures associated with this base and owned by the player faction; order **ascending by cost**; destroy one per `objectiveSabotageHoldSeconds` while the hold condition is met; stop at `objectiveSabotageStructuresPerMission` or when nothing is left, then report success. The shipped costs make the requirements' example come out exactly right: Bunkers 750 → Recruitment Tent / Medical Tent 1000 → Guard Tower 1200 → Vehicle Maintenance Ramp / Helipad 1500 → Fuel Depot 2000 → **Garage 8000** (`Configs/Resistance/buildables.conf`). ⚠ There is **no `m_iSize` attribute anywhere** — cost is the only ordering key that exists, and that is a deliberate choice, not an oversight.
4. **T6.4 — One notification per mission, not per structure.** The first destruction of a mission sends `SendTextNotification("ObjectiveSabotage", -1, baseName)` through `OVT_Global.GetNotify()` (`OVT_NotificationManagerComponent.SendTextNotification(tag, playerId = -1, p1, p2, p3)` at `:77`; `-1` is the broadcast default and there is **no faction-scoped send**). The preset goes in **`Configs/overthrowBroadcastMessages.conf`** — not a `Configs/Notifications` directory, which does not exist — with a fresh GUID and `Name`/`Description` keys in `Language/localization_Overthrow.st`. This is a **deliberate addition beyond the letter of the requirements**, justified by the Quality Bar's legibility row: a structure vanishing with no explanation is the opposite of a readable ramp. ⚠ It is **not** an intel surface for the FOB, which stays silent by explicit requirement. ⚠ `PrintFormat` takes at most 3 string params.
5. **T6.5 — Author `Deployment_ObjectiveSabotage.conf`:** insertion module with `specops_team`, sabotage behaviour, reinforcement with `m_bDeleteOnConditionFail 1`, `OVT_BaseControlConditionDeploymentModule` with `m_bRequireControl 0` (deploy only while the resistance holds it — the same inversion knob), objective condition, `m_iAllowedLocationTypes BASE`.
6. **T6.6 — Director wiring:** `TickHarassment()` for a BASE objective creates sabotage operations on the same cadence, with the same pool debit; `OnSabotageSuccess()` increments `m_iSabotageSuccesses` and re-checks the Phase 2 gate.
7. **T6.7 — Logic-tier:** `NextTargetIndex` returns the cheapest not-yet-destroyed index; returns `-1` on an empty list, an all-destroyed list and a ragged input; ties go to input order; a negative cost sorts first without crashing.
8. **T6.8 — Init-tier:** the config resolves and validates; the module's candidate filter excludes a structure associated with a *different* base and one owned by the occupying faction; the module destroys **nothing** while an enemy is inside the clear radius; `m_iStructuresPerMission` is respected.
9. **T6.9 — `context.md`:** the T6.1 table, the removal-path decision, and an explicit statement of what the player permanently loses so the Phase 9 documentation can be truthful.

**Acceptance criteria**

- compile **0**; All green.
- `grep -rn "DeleteEntityAndChildren" Scripts/Game/GameMode/Deployments/Modules/OVT_BaseSabotageBehaviorDeploymentModule.c` → **empty**. The module asks the shared path; it does not delete.
- `grep -rn "NavmeshRebuild" Scripts/Game/GameMode/Deployments/Modules/OVT_BaseSabotageBehaviorDeploymentModule.c` → **empty**, for the same reason.
- A destroyed structure does not reappear after save → reload (play-test criterion F5, and the reason T6.1 exists).
- The player's own `RemovePlacedItem` still refuses a non-owner non-officer — the server bypass must be keyed on `playerId == -1`, never on a widened permission.
- `git diff Scripts/Game/GameMode/Virtualization/` → **empty**.

---

### Phase 7 — Phase 2: the FOB

**Agent:** `component-developer-advanced` — **advanced, and the largest phase.** A new prefab and entity class, persistence-tracked world content raised by a driving truck, a spend ceiling inside the shared pool, a starvation rule, a held action with a new server-validated request, and a teardown that must not leak.
**Estimate:** 18–24 h
**Suite after this phase:** **All**.

**Tasks**

1. **T7.1 — The authored marker, in the `OVT_SniperPosition` shape.** `OVT_FOBPositionComponent : ScriptComponent` (the queryable thing, so the marker can be attached to anything) + `OVT_FOBPosition : GenericEntity` (which supplies only the Workbench bound box and facing arrow) + `Prefabs/GameMode/OVT_FOBPosition.et`. Copy `Scripts/Game/Components/OVT_SniperPositionComponent.c`, `Scripts/Game/Entities/OVT_SniperPosition.c` and `Prefabs/GameMode/OVT_SniperPosition.et` structurally. ⚠ **Per-frame Workbench visualisation in this repo uses copy-safe `Shape` calls only** — `Shape.CreateArrow` is proven; the `CreateLines` family crashed Workbench twice and is banned here. ⚠ **Author no world-layer instances in this phase** — see [R17](#9-risks--mitigation); the generated path must ship standing on its own.
2. **T7.2 — `Prefabs/Bases/OVT_OccupyingFOB.et`** — the FOB structure: a faction flag, a small camp composition, and the dismantle action's owner entity. Authored by hand; hand-authored GUIDs always resolve.
3. **T7.3 — Site selection, generated path first.** `OVT_FOBSiting` (pure): `IsInBand(distanceToObjective, min, max)`, `IsClearOfExclusions(candidate, array<vector> exclusions, array<float> radii)`, `ScoreSite(flatness, elevation, distanceToRoad)`. The director does the world work: **bounded-attempt generation** on the source→objective line with `OVT_WorldUtils.IsOceanAtPosition` and the `TraceBox` clearance pattern from `OVT_QRFControllerComponent.c:483-494` (which is correct at HEAD), plus `OVT_WorldUtils.FindNearestRoadSpawn` for the near-a-road preference; **then** authored markers by `QueryEntitiesBySphere` + `FindComponent(OVT_FOBPositionComponent)` filter, in the `OVT_SniperMarkerPlacementProvider.c:39-107` shape, which **win when they exist**. Build and tune the generated path first ([R17](#9-risks--mitigation)). ⚠ The attempt bound is production code and is fine, but it must be a **named constant with its reason**, not a bare literal — and the word `maxAttempts` still may not appear in any test.
4. **T7.4 — No site → blacklist and reselect.** One round, logged at WARNING with the objective name and the attempt count.
5. **T7.5 — `OVT_FOBRaiseSpawningDeploymentModule`** per [§3.6](#36-the-new-modules-configs-registry-and-difficulty-additions). ⚠ **`WasRestoredFromSave()` gates the raise** — without it a campaign grows one more FOB structure per load, which is [D11](#d11--the-fob-is-a-deployment-plus-a-tracked-structure-not-a-new-entity-system)'s whole reason for existing and R2's failure mode from the migration, restated.
6. **T7.6 — Author `Deployment_ObjectiveFOB.conf`** (the raise module + reinforcement + objective condition) and **`Deployment_ObjectiveFOBGarrison.conf`** (insertion module with `m_Source` = `OVT_ObjectiveAnchorSourceProvider`, group count capped by `objectiveFOBGarrisonMax`, DEFEND behaviour, reinforcement, objective condition). Ship `OVT_ObjectiveAnchorSourceProvider` here — it returns the FOB position when one is up and falls through to the nearest held base otherwise, which is also what makes later Phase 1 operations launch from the FOB.
7. **T7.7 — The budget ceiling.** `m_iFOBSpent` on the FOB record; every director create checks `GetFactionResources(faction) >= cost` **and** `OVT_ObjectivePhaseRules.WithinFOBCeiling(spent, cost, ceiling)` before `ForceCreateDeployment`, then debits and increments. ⚠ **The director never holds money.** State that in the record's header, because a future reader will otherwise "fix" it into a wallet.
8. **T7.8 — Starvation.** Evaluate the three inputs each tick, feed `IsFOBStarved`, tick `m_iStarvationTicks`, tear down at `objectiveStarvationMinutes`. Recovery zeroes the counter. Log every transition, both ways.
9. **T7.9 — Removal.** `OVT_DismantleEnemyFOBAction : ScriptedUserAction` — `CanBeShownScript` (player faction only), `CanBePerformedScript` (no OF AI within the clear radius; `SetCannotPerformReason` with a localized string when refused), `PerformAction` resolving `OVT_ControllerComponent<OVT_CampaignRequestComponent>.Get()` and calling one new verb, and `HasLocalEffectOnlyScript() { return true; }`. The whole shape is `OVT_CaptureBaseAction.c` (30 lines) plus `OVT_UndeployFOBAction.c`'s request hop. ⚠ **The hold duration is authored in the prefab, not in script** — `Duration 15` inside the `additionalActions` block of `Prefabs/Bases/OVT_OccupyingFOB.et`, exactly as `BaseFlag_US.et` wires `OVT_CaptureBaseAction`. ⚠ **The server re-validates everything** — this epic's headline debt is unvalidated capture RPCs (BUG-025) and this feature does not add a third; refusals log in the shape of `OVT_FOBRequestComponent.RejectFOBRequest` (`:593`). ⚠ **`Rpc()` arity is a compile-check blind spot** (BUG-090): a wrong argument count compiles clean and dies silently at the wire, so the new request gets an Init-tier seam case in the shape of `OVT_TEST_Init_CampaignRequestSeam.c`.
10. **T7.10 — Teardown, one path for all three exits** (starvation, removal, QRF resolution): delete the FOB deployment and the garrison deployment, delete the structure through the T6.2 removal path, zero `m_iFOBSpent`, release any insertion reservation, clear the anchor, apply the removal penalty **only** on the player-initiated exit, reset. Called by everything; asserted once.
11. **T7.11 — Logic-tier:** every `OVT_FOBSiting` function including the degenerate band (`min >= max`), an empty exclusion list, a ragged exclusion/radius pair, and `WithinFOBCeiling` at, below and above the ceiling.
12. **T7.12 — Init-tier:** both configs resolve and validate; the raise module on a restored deployment raises **nothing**; on a fresh one it raises once and, driven twice, raises nothing more; the anchor source provider prefers the FOB over the base when both exist; the teardown path leaves no deployment of either config within the radius.
13. **T7.13 — Persistence-tier:** extend T2.12's case with a raised FOB — after restore, the director re-links the FOB deployment by name+position on its first tick, and a payload naming a deployment that no longer exists resets the objective instead of stranding it.
14. **T7.14 — `context.md`:** the siting constants and why, the "never holds money" rule, the three teardown exits and which applies the penalty.

**Acceptance criteria**

- compile **0**; All green.
- `git diff Scripts/Game/GameMode/Virtualization/` → **empty**.
- `grep -rn "AddFactionResources" Scripts/Game/GameMode/Objectives/` → **empty**. The director only ever subtracts.
- A save taken with an FOB up and re-applied produces **exactly one** FOB structure, one FOB deployment and one garrison deployment.
- The new prefab and entity are referenced from exactly one config each.

---

### Phase 8 — Phase 3: the counter-QRF, and the GM panel

**Agent:** `component-developer-advanced` — **advanced.** It touches the battle layer and the GM wire, both of which have live client consumers and neither of which the automated spine covers well.
**Estimate:** 8–12 h
**Suite after this phase:** **All**.

**Tasks**

1. **T8.1 — Read-only survey.** Re-verify the two QRF edit sites (`SendTroops:204-213`, `SendWave:275`, `GetLandingZone:454`) and confirm [C4](#corrections-to-the-requirements-document) still holds — the globals gone, the trace fixed, the wrap fixed. Record the verdict; if a concurrent session has re-introduced any of them, this phase's design changes.
2. **T8.2 — The FOB as a wave source.** In `SendTroops()`, after the `m_Bases` loop (`:204-213`) and **before** the empty-list fallback (`:215-221`), insert the director's FOB position if one is up and it is farther than 20 m from the QRF target — the same guard the base loop uses. ⚠ `m_Bases` is populated once and never refreshed (`:204`); an FOB torn down mid-battle keeps sending troops. That matches the existing behaviour for captured source bases and is **left alone deliberately** — changing it is a QRF-systems job, and it gets a `context.md` note rather than a fix.
3. **T8.3 — Bearing bias.** `OVT_QRFBearing.PreferredDegreesFromSource(vector source, vector target)` (pure): the compass bearing **from the target toward the source**, in the convention `GetRandomDirection` documents at `:416-419` (0° = North = `-Z`, 90° = East = `+X`), because `GetLandingZone` computes `checkpos = qrfpos + (dir * distance)` and `dir` therefore points from the target *out to* the LZ. ⚠ **Getting this sign backwards puts every wave on the far side of the objective** and is the single most likely defect in the phase. Change `GetLandingZone()` to `GetLandingZone(vector sourcePos)`; when `sourcePos != vector.Zero` it derives the preferred direction from the bearing and keeps the authored `m_iDirectionVariance`; otherwise it uses `m_iPreferredDirection` exactly as today. `SendWave` passes the source it is iterating (`:270`, `:275`).
4. **T8.4 — Director wiring:** `TickCounterQRF` is entered by calling `StartBaseQRF(GetBase(...))` or `StartTownQRF(town)` when the Phase 3 gate passes; subscribe the QRF's `m_OnFinished` **through the OF manager's existing resolution path**, not by adding a second subscriber to the controller — the manager already deletes the controller entity in `OnQRFFinishedBase/Town`, and a director handler racing that deletion is a use-after-free. Prefer polling `m_CurrentQRF` going null on the director's own tick, and say why in the header.
5. **T8.5 — Reset whatever the outcome.** On the tick after the QRF clears: tear the FOB down, reset, reselect. Both a win and a loss take the same path.
6. **T8.6 — The GM record: a NEW additive pair, not a widened one.** Add `SendCampaignObjective(int playerId, int seq, string name, int phase)` beside `SendCampaignSchedule` (`:794`) and `RpcDo_CampaignObjective(int seq, string name, int phase)` beside `RpcDo_CampaignSchedule` (`:1013`), send it in the fan between Schedule and `SendRecordFan` (`:589-593`), and **bump `CAMPAIGN_RECORD_COUNT` 2 → 3 (`:93`)** because `SendSnapshotEnd` reports `CAMPAIGN_RECORD_COUNT + perEntityRecords` and an old client would then expect one record it never receives. **Bump `WIRE_VERSION` 1 → 2 (`:71`)** for the same reason — `RpcDo_SnapshotBegin(seq, wireVersion)` (`:967`) exists exactly so a mismatched client refuses to stage. ⚠ Every send site needs its `ShouldRespondLocally(playerId)` listen-server branch, in the shape at `:758-764`. ⚠ **`Rpc()` is an untyped variadic prototype: a wrong argument count compiles clean and dies silently at the wire (BUG-090)** — arity-diff the pair by eye, as the comment at `:744-749` instructs, and cover it in `OVT_TEST_Init_GMRequestSeam.c`.
7. **T8.7 — The client store, and its three-method trap.** Append `m_sObjectiveName` and `m_iObjectivePhase` to `OVT_GMCampaignState`. ⚠ **Every new scalar must be added to three methods, not one**: `CopyFrom` (`:172`), `CopyRecords` (`:196` — check whether it belongs there) and `Clear` (`:226`). A field added to the declaration and forgotten in `Clear` leaks the previous campaign's objective into the next one.
8. **T8.8 — The builder stays read-only.** `OVT_GMSnapshotBuilder`'s hard rule (`:5-13`) is that it mutates nothing; its only sanctioned mutation is `OVT_GMGroupRegistry.Sweep()`. The director therefore exposes **pure getters** (`GetObjectiveDisplayName()`, `GetPhase()`) that compute nothing and change nothing. If a number needs arithmetic, it goes in a pure static, not in a getter with a side effect.
9. **T8.9 — The panel: two rows in `CampaignSection`, not `DetailSection`** ([D13](#d13--the-gm-objective-fields-are-campaignsection-rows-not-the-detail-slot)). Author `Row_Objective` / `Label_Objective` / `Value_Objective` and `Row_Phase` / `Label_Phase` / `Value_Phase` in `UI/Layouts/GM/GMPanel.layout` under `CampaignSection` (`:84`), copying the `Row_Threat` pattern (`:91` / `:98` / `:106`); two `FindText` calls in `CacheWidgets` (`:155`); two `SetText` calls in `RenderAll` (`:401-416`); two `OVT-GMPanel_*` keys in `Language/localization_Overthrow.st` beside `:4293-4485`. ⚠ **No RPC, no `RplProp` and no replication receiver may appear in `OVT_GMPanelUIComponent`** — its header (`:15-19`) forbids it outright; a new number belongs on the snapshot. Formatting goes in `OVT_GMPanelFormat.c`, whose header (`:6-8`) bans any world/engine/manager reference because its Logic-tier test is a directory-wide text grep. **`.layout` authoring is a `ui-developer` slice** — hand it over rather than improvising it from a systems agent.
10. **T8.10 — Logic-tier:** `PreferredDegreesFromSource` for all four cardinal directions and both diagonals, with a tolerance (⚠ `vector.Distance` is +1 ULP off at 1 000 m and 2 000 m; bearings are worse); a source coincident with the target returns a defined value rather than NaN; the panel formatter for every phase and for no objective.
11. **T8.11 — Init-tier:** the GM seam accepts the new record and the state receives both fields; `Clear()` zeroes them; the director's Phase 3 gate fires `StartBaseQRF` exactly once (drive the gate twice and assert one call, using the existing `m_CurrentQRF` guard as the observable).
12. **T8.12 — `context.md`:** the T8.1 verdict, the bearing sign argument written out in full, the `m_Bases` staleness note, and the `CAMPAIGN_RECORD_COUNT` / `WIRE_VERSION` bumps with their reason.

**Acceptance criteria**

- compile **0**; All green.
- `git diff Scripts/Game/Controllers/OccupyingFaction/OVT_QRFControllerComponent.c` touches **only** `SendTroops`' source insertion, `SendWave`'s call, and `GetLandingZone`'s signature + preferred-direction derivation. Countdown, budgeting, scoring, waypoints and resolution are untouched.
- `git diff Scripts/Game/GameMode/GM/OVT_GMRecords.c` → **empty** (no per-entity record class changes).
- The GM state's existing fields are in their original order; the two new ones are last, and both appear in `CopyFrom` **and** `Clear`.
- `grep -n "CAMPAIGN_RECORD_COUNT\|WIRE_VERSION" Scripts/Game/Components/Controller/OVT_GMRequestComponent.c` → **3** and **2**.
- `grep -rn "Rpc\|RplProp" Scripts/Game/UI/GM/OVT_GMPanelUIComponent.c` → **empty**.
- A QRF triggered by the player still behaves exactly as before (its sources have no FOB and its LZ falls back to the authored direction).

---

### Phase 9 — Help & documentation sync

**Agent:** `help-docs-sync`
**Estimate:** 3–4 h
**Suite:** **skipped — docs-only.** Say so.

Players see substantially different enemy behaviour, so the closing sync is in scope.

1. **T9.1** Fact-check **every** existing sentence in `Configs/Tutorials/` and `Configs/FieldManual/` about enemy counter-attacks, QRFs, defending a town and defending a base against the shipped code, and cite a `file:line` or cut the sentence. The project has shipped invented mechanics twice; no gate catches a well-formed lie.
2. **T9.2** The player-visible changes to document:
   - **counter-attacks are no longer random.** The occupying faction picks one target and works toward it; you can see which one and prepare.
   - **the three phases and what each looks like** — trucks bringing groups in, support sliding in a harassed town, radio towers being retaken, structures at your base being demolished, an enemy FOB appearing between their nearest base and your town.
   - **an enemy FOB is not announced** — you find it, and pulling it down needs the area cleared and a held action on its flag.
   - **starving an FOB works**: take or empty the base supplying it, or keep a strong presence there, and it comes down on its own.
   - **the QRF now arrives from where they actually are**, including from the FOB.
   - **difficulty changes how much warning you get** — easier settings demand more sabotage missions before the assault.
   - **what you permanently lose to sabotage** (the T6.9 list), stated plainly.
3. **T9.3** Wiki: the same points plus the operator-facing notes — the eleven new difficulty fields and what each does, the removal of `counterAttackTimeout`, and the fact that objective operations spend the same deployment pool as everything else.
4. **T9.4** Epic bookkeeping: add `counter-attacks` to `docs/features/occupying/epic-overview.md`'s feature table, refresh its Tech Debt section (the QRF LZ half of BUG-031 is fixed; the "no OF tower-recapture path" regression from C1 is closed), and update the epic's row in the master `docs/overview.md`.

**Acceptance criteria** — all three surfaces agree with the code; no invented mechanics; every claim carries a `file:line`; the epic overview and master row reflect the new feature.

---

## 5. Key Technical Decisions

### D1 — A new server-only director component owns the objective

*(user decision, 2026-08-18 — binding, recorded verbatim)*

> **New director component:** a new server-only game-mode component (suggested name `OVT_ObjectiveDirectorComponent`, but pick a name fitting project conventions) owns objective selection, phase state machine, harassment/sabotage counters, blacklist, timers, and the FOB record, with its own vanilla-persistence serializer. It feeds the deployment manager a scoring input + position anchor (the requirements' "prioritize the current objective" hook in the evaluate→score→create loop) and calls `StartBaseQRF`/`StartTownQRF` for Phase 3. Selection scoring, phase-gate math, and the inverted difficulty scaling must be extracted as world-free pure statics for Logic-tier tests — the same pattern base-defense-migration just used (`OVT_DeploymentSelection`, `OVT_DeploymentPlacement`).

**The name stands as `OVT_ObjectiveDirectorComponent`.** The convention in this tree is `OVT_<Thing>ManagerComponent` for components that own a registry and answer questions about it — towns, vehicles, players, deployments, virtualization. This one owns no registry and answers no questions; it **makes a decision every minute**. Calling it a manager would put it in the wrong mental bucket, and "director" is the standard term for exactly this role. It lives in a new `Scripts/Game/GameMode/Objectives/` directory beside `Deployments/`, `Virtualization/` and `Civilians/`, and it gets an `OVT_Global.GetObjectiveDirector()` accessor like every other game-mode component.

The rejected alternative — putting the state machine on `OVT_OccupyingFactionManager` — was rejected on three counts: that class is already ~1 800 lines and the epic's own tech-debt list calls its serializer and JIP payloads fragile; the objective's payload would have to be appended to a version-2 positional format with no version field on its record classes; and a second decision-maker inside the class that already owns income, threat, bases, towers, QRF resolution and JIP is how that class got to 1 800 lines.

### D2 — Retire the legacy triggers first, in Phase 1

*(user decision, 2026-08-18 — binding, recorded verbatim)*

> **Retire-first sequencing:** the legacy triggers are deleted in an early phase, before the new system can attack. v1.5 is unreleased; temporary OF passivity in dev play-tests is accepted.

Phase 1 does exactly that. The cost is stated rather than hidden: **between Phase 1 and Phase 8 the occupying faction has no offensive trigger at all.** It still garrisons, patrols, fortifies and defends; it simply never attacks. Play-tests in that window cannot evaluate pressure and should not try.

The benefit is that no phase ever has two attack systems running at once. The alternative — build first, retire last — would have meant a window in which a random hourly QRF could fire *into* an objective the director was still building toward, producing exactly the unpredictability this feature exists to end, and making every mid-feature play-test uninterpretable.

### D3 — Re-selection is a flag, never inline

`m_OnBaseControlChanged` (`OVT_OccupyingFactionManager.c:185`) fires from `OVT_BaseControllerComponent.SetControllingFaction` **before** the affiliation is written (`:174-179`: invoke, then `SetAffiliatedFaction`). A handler that reads `GetControllingFaction()` inline reads the **old** owner, so a director that re-selected inside the handler would evaluate the world as it was one instruction ago and could pick the base the player just took as its own target.

Both handlers therefore set `m_bReselectPending` and return. Re-selection runs at the top of the next director tick, when every mutation has settled — and only in `IDLE` or `HARASSMENT`, because the requirements lock the objective from Phase 2 onward. The flag is **not persisted**: after a load, the first tick re-evaluates anyway.

### D4 — Tick counters, not wall-clock deadlines

Every duration the director tracks — phase age, next-operation cadence, starvation, hold timers inside its modules — is an integer decremented once per tick. Nothing stores a deadline.

That single choice satisfies four separate requirements at once:

1. **"All objective timers freeze while any QRF is live"** becomes a property of the early return rather than a rule someone has to remember to apply to each timer.
2. It matches the campaign's own time base: the director ticks at `OF_UPDATE_FREQUENCY / timeMul`, so a "45-minute" cadence is 45 in-game minutes regardless of the time multiplier, which is what a designer authoring `objectiveHarassmentIntervalMinutes` means.
3. Counters serialize as plain ints and restore exactly; a wall-clock deadline saved on one session and loaded on another is meaningless.
4. `TickDown(value)` is a one-line pure static, so the Logic tier can assert the machine's progression without a clock.

### D5 — The anchor is pushed, biases ordering only, and is absent by default

[§3.5](#35-the-objective-anchor--how-the-bias-plugs-into-evaluation-scoring) in full. The three properties that made push win over pull: no anchor is byte-identical to today's behaviour (so thirteen shipped configs and every existing test are unaffected); the deployment manager never learns the director exists (the dependency is one-way, and the manager stays testable with no director present); and the bias is a pure function of four floats, so the claim "objective-adjacent work is bought first, but only up to `weight`" is a Fast-tier assertion rather than a play-test impression.

Rejected: **a pull, with the manager calling `OVT_Global.GetObjectiveDirector()`** — adds a null-check on a component that may not exist, in the hottest loop in the framework, and inverts the dependency. **Raising `MAX_DEPLOYMENTS_PER_EVALUATION` or the per-faction ceiling near an objective** — the ceiling exists to bound AI cost and an objective is not a reason to exceed it. **Making objective operations ordinary configs the evaluator picks up** — the evaluator's threat sort and per-pass cap would make the ramp's cadence unpredictable, which is the one thing the requirements are most insistent about.

### D6 — The director owns its own version-first serializer

[§3.8](#38-persistence) in full. `OVT_OccupyingFactionManagerSerializer` is version 2, fully positional, and its record classes carry no version of their own — `OVT_PersistedBase`'s five fields are load-bearing by position, which is why `base-defense-migration` kept a dead `upgrades` array declared rather than remove it. Appending an objective payload there would put a new, actively-evolving structure inside the most fragile format in the epic.

A separate serializer costs one entry in `Configs/Systems/Persistence/Overthrow.conf` and buys a clean version-1 format that can grow by appending, a pure-codec `Deserialize`, and complete independence from the OF manager's load order.

The one genuine constraint it inherits is the **load-order hazard already documented at `OVT_OccupyingFactionManager.c:436-438`**: the deployment manager's restore clears and refills the faction resource pool and runs *after* the game-mode component serializers. The director therefore does nothing with money, and nothing with deployments, during restore — it stores what it read and does the work on its first tick. This is the same pattern the legacy refund uses (`QueueLegacyUpgradeRefund` → credited in `PostGameStart()` at `:414`) and it is copied, not re-derived.

### D7 — Insertion is one general-purpose module with a source-provider seam

The requirements ask for "a general-purpose deployment module any deployment config can use". That is taken literally: `OVT_InsertionSpawningDeploymentModule` has **no dependency on the director**, resolves its origin through a `OVT_DeploymentSourceProvider` in the shape the placement providers already established, and gets its concurrency cap from the **deployment manager**, not the director. A future config — a reinforcement convoy, a supply run, a resistance-side insertion — authors a provider and a `.conf` and changes nothing else.

Two consequences worth stating so they are not read as omissions:

- **The FOB raiser is a subclass, not a second module.** One truck delivers both the structure and the free garrison, which is what the requirements describe, and the subclass inherits the whole drive/stuck/dismount lifecycle rather than copying it.
- **The cap counter lives on the manager and is runtime-only.** A static on the module class would survive a campaign restart in the same client session — the "two campaigns in one session" trap that `virtualization/core`'s Phase 6 found four separate bugs in.

### D8 — The director calls the QRF starters; the QRF's resolution is polled, not subscribed

`StartBaseQRF` / `StartTownQRF` are called directly, exactly as the retired roll did. But the *end* of the battle is observed by **polling `m_CurrentQRF` on the director's own tick**, not by inserting a second handler into `m_OnFinished`.

The reason is a lifetime hazard: `OnQRFFinishedBase` (`:1129-1144`) and `OnQRFFinishedTown` (`:1168-1214`) both call `SCR_EntityHelper.DeleteEntityAndChildren(m_CurrentQRF.GetOwner())` from inside the invoker's own dispatch. A second subscriber ordered after the manager's would run with the controller entity already deleted. Polling costs one null-check per in-game minute and cannot be got wrong.

### D9 — The QRF changes are exactly two, and need no prerequisite repair

The brief asked for an explicit decision on whether fixing the LZ-cache globals is a prerequisite of bearing bias. **It is not, because they no longer exist** ([C4](#corrections-to-the-requirements-document)): commit `d7e42362` removed `Goodqrfpos`/`Goodqrfbasepos`, made each wave source resolve its own LZ, fixed the `TracePosition` no-op, and fixed the 0°/360° wrap. All three halves of BUG-031's LZ cluster are closed at HEAD.

So the scope is exactly what the requirements say and nothing more:

1. The FOB joins `m_Bases` as a wave source.
2. `GetLandingZone` takes the source position and derives the preferred direction from real geometry.

**Deliberately excluded, with reasons:** `m_Bases` is never refreshed mid-battle, so a captured source base — and now a torn-down FOB — keeps sending troops; fixing that changes player-initiated QRFs too and belongs to a QRF-systems feature. The unused `BaseWorld world` local at `:469` and the unreachable duplicate `case "DefendBase":` at `:340` are cosmetic and are left for whoever owns that file next. QRFs still do not debit `m_iResources` in `SendTroops` (they debit in `SendWave` at `:299-300`) — that is BUG-027 and it is not this feature's to fix.

### D10 — The inverted sabotage scaling is authored, and the invariant is asserted

The requirements say the Phase 3 sabotage count "scales **inverted**: easier difficulties require *more* successful sabotage missions". That is expressed as **authored per-preset values** (Easy 6 → Insane 2), not as a formula, for three reasons: `OVT_DifficultySettings` is a flat list of directly-authored numbers and every other scaling in the file works this way; a formula needs an input axis, and the file has no numeric "difficulty level" to invert (the presets are named, not ranked); and a tuner changing one preset should not have to reason about a curve.

The Logic tier therefore covers the **consumption** — `RequiredSabotageMissions(authored, fallback)` clamping an unset, zero, negative or absurd value — and the **Init** tier covers the **inversion invariant** across the five shipped presets, because that is the only tier that can see loaded configs. Both are required; neither alone is the claim.

Rejected: **a single `objectiveAggression` scalar** that every new field derives from. It would make the inversion a formula and satisfy the letter of "pure static", but it invents a tuning axis the game does not have and makes eleven fields un-tunable independently.

### D11 — The FOB is a deployment plus a tracked structure, not a new entity system

The FOB could have been a bespoke record with its own spawning, its own persistence and its own garrison logic — the resistance's `OVT_FOBData` is shaped that way. It is not, because expressing it as a deployment buys, at zero cost: pool accounting through the one funding path, virtualization of its garrison through the module seam, reinforcement when the garrison is thinned, GM tagging, marker persistence through the shipped deployment serializer, and self-collection when its condition module fails.

What the director keeps is only what a deployment cannot express: which objective it belongs to, which base supplies it, how much of the ceiling it has spent, and how long it has been starving.

⚠ The structure itself is a **persistence-tracked entity** and therefore **must not be re-raised on a restored deployment** — `WasRestoredFromSave()` gates it. Without that gate a long campaign grows one more FOB per load, in a slightly different place each time. This is R2 from `base-defense-migration`, restated for a bigger and more visible object, and it is the reason [D11](#d11--the-fob-is-a-deployment-plus-a-tracked-structure-not-a-new-entity-system) is a decision rather than an implementation detail.

### D12 — One notification for sabotage; none for the FOB

The requirements are explicit that the FOB is **not** announced, and that stays. Sabotage is different: a recruitment tent silently vanishing while the player is elsewhere is unreadable, and the Quality Bar's legibility row makes readability a hard floor. So one notification per **mission** (not per structure) is sent through the existing notification manager when the first structure of a mission falls.

This is a deliberate, small addition beyond the letter of the requirements, recorded here so a reviewer can veto it in one line rather than discover it. It creates no intel surface — it names the base, not the attacker, and says nothing about the FOB or the objective.

### D13 — The GM objective fields are `CampaignSection` rows, not the detail slot

The requirements name "the empty `DetailSection` in `OVT_GMPanelUIComponent`" as the insertion point. **This plan diverges, deliberately.**

`DetailSection` (`UI/Layouts/GM/GMPanel.layout:316`, authored `"Is Visible" 0`) is a *per-selection* sub-block: `GetDetailSlot()` (`OVT_GMPanelUIComponent.c:247`) hands a container to some other screen to parent a detail view into, `ShowDetail(bool)` (`:261`) toggles it, `ClearDetail()` (`:278`) empties it — and **nothing in the tree consumes it today**. The objective and its phase are not a detail of a selection; they are campaign-wide scalars of exactly the same kind as threat, occupying resources and the distribution countdown, every one of which lives as an authored `Row_*` pair in `CampaignSection` (`:84-110`).

So they become two more rows there, following `Row_Threat` verbatim. That is cheaper (two `FindText` calls and two `SetText` calls, in a render path that already runs), consistent (a GM reads every campaign scalar in one column), and it leaves `DetailSection` free for what it was designed for. The divergence is recorded here so a reviewer checking the plan against the requirements sees a decision rather than an omission.

---

## 6. Definition of Done

An evaluator with **no implementation context** should be able to verify every item below.

### Functional Criteria

- **F1 — The objective exists and is visible.** Open the GM Overthrow panel a few minutes into a campaign in which the resistance holds at least one town or base. **Expect:** a named objective and a phase in the detail section. With the resistance holding nothing, expect an explicit idle state, not a blank.
- **F2 — Selection is predictable.** Note the objective, then read the server log's selection line, which names the winner, the runner-up and both scores. **Expect:** the winner is a resistance-held town, city or base — never a village, an FOB or a radio tower — and its score is explicable from population, distance to the nearest enemy base, tower coverage and (for towns) support.
- **F3 — Harassment arrives by truck, and the town feels it.** Watch the objective town for a few in-game hours. **Expect:** a truck drives in from an occupying base, drops a group at a road short of the town, and leaves; the group makes for the centre; if it holds the centre unopposed the town's support drops by a visible step and drops **again** on the next successful mission. Group size grows with each success.
- **F4 — Towers are retaken.** Hold a radio tower covering the objective. **Expect:** a specops-grade team is delivered, must **hold** the tower for several minutes, and only then does it flip — killing them before the timer expires prevents it.
- **F5 — Bases are sabotaged, smallest first.** Make the objective a base you hold with several structures. **Expect:** a team infiltrates; while no defender is nearby, structures are demolished starting with the cheapest (recruitment tent, vehicle ramp) and escalating; one notification per mission; the destroyed structures **do not come back after a save and reload**.
- **F6 — Phase 2 raises a real FOB.** Let the ramp run. **Expect:** a supply truck drives out and an occupying-faction FOB appears between their nearest base and your town — with **no notification**. It has a garrison. Ambushing or stealing the truck prevents it.
- **F7 — The FOB can be starved.** Take or empty the base supplying the FOB, or keep a strong presence there. **Expect:** after the difficulty's starvation window the FOB comes down on its own and the occupying faction picks a new objective.
- **F8 — The FOB can be removed by hand.** Clear the area of occupying AI, then hold the action on the FOB's flag. **Expect:** the action is hidden to the wrong faction, refused with a reason while enemies are near, and on completion the FOB and its garrison are gone and the objective resets.
- **F9 — Phase 3 arrives from the right direction.** Let the gate pass. **Expect:** a QRF on the objective whose waves land **on the side the occupying faction actually holds**, including from the FOB — not on a uniformly random bearing. Whatever the outcome, the FOB comes down and a new objective is chosen.
- **F10 — Resource accounting is closed.** Watch the GM campaign panel across several director operations and one 6-hour resource tick. **Expect:** the deployment pool falls by exactly the cost of each operation created, the reserve behaves exactly as it did before this feature, and no number moves unexplained. **The occupying faction never gains resources from anything the director does.**
- **F11 — The whole objective survives a Continue.** Save mid-Phase-2 with an FOB up and some sabotage successes banked, quit, **Continue**. **Expect:** the same objective, the same phase, the same counters, **exactly one** FOB structure, and the ramp resumes where it stopped.
- **F12 — A QRF freezes everything.** Trigger a player-initiated QRF elsewhere on the map during Phase 1. **Expect:** the objective's operations stop being created and its timers do not advance for the duration; afterwards they resume from where they were.
- **F13 — The machine never wedges.** Play for an extended session. **Expect:** every objective either progresses, is abandoned for a stated reason, or is blacklisted and replaced — and every one of those transitions appears in the log with its reason. No objective sits in one phase indefinitely with nothing happening.
- **F14 — The retired triggers are gone.** Play for several in-game days without provoking anything. **Expect:** no QRF ever fires on a random base, and no QRF fires the instant a town's support crosses 25 %. Player-initiated capture and uprising still work exactly as before.

### Quality Criteria

- **Q1** `tools/compile-check.sh` exits **0** with no output.
- **Q2** Fast `{6A6E29FF47ECB840}` and All `{6A6E2A002F53A581}` both exit **0** at the end of every phase.
- **Q3** Every new test case carries a recorded proof that it can fail — the exact edit, in a preamble comment. **No `maxAttempts` anywhere.**
- **Q4 The legacy triggers are gone:** `grep -rn "counterAttackTimeout\|m_bCounterAttackTimeout" Scripts/ Configs/ Prefabs/` → **empty**; `grep -rn "StartBaseQRF\|StartTownQRF" Scripts/` → exactly the two player-initiated callers plus the director.
- **Q5 Core and movement are untouched:** `git diff Scripts/Game/GameMode/Virtualization/ Scripts/Game/GameMode/VirtualMovement/ docs/features/virtualization/core/api.md` → **empty** across the whole feature.
- **Q6 One funding path:** `grep -rn "AddFactionResources" Scripts/` → the OF manager's single credit point and the deployment framework's own refund, **and nothing in `Scripts/Game/GameMode/Objectives/`**. Every director spend goes through `SubtractFactionResources` immediately after a successful create.
- **Q7 Logic-tier grep clean:** no manager, game-mode, world, entity or `OVT_Global` identifier in any of the four pure-static files or their Logic-tier test files, **comments included**.
- **Q8 The dependency points one way:** `grep -rn "OVT_ObjectiveDirector" Scripts/Game/GameMode/Deployments/` → **empty**.
- **Q9 The save formats did not move:** `git diff` on `OVT_OccupyingFactionManagerSerializer.c`, `OVT_PersistedBase`, `OVT_PersistedRadioTower`, `RplSave`, `RplLoad`, `OVT_DeploymentComponentSerializer.c` and `OVT_DeploymentManagerSerializer.c` → **empty**. The director's own serializer is version 1 and append-only.
- **Q10 The modifier config appended:** `git diff Configs/Modifiers/supportModifiers.conf` shows exactly one entry, **at the end of `m_aModifiers`**.
- **Q11 The GM wire is append-only:** `git diff Scripts/Game/GameMode/GM/OVT_GMRecords.c` → **empty**; `OVT_GMCampaignState`'s pre-existing fields are in their original order.
- **Q12 Shipped configs are byte-identical:** `git diff Configs/Deployment/Deployment_TownPatrol.conf Configs/Deployment/Deployment_TowerGarrison.conf Configs/Deployment/Deployment_VehiclePatrol_*.conf Configs/Deployment/Deployment_Base*.conf` → **empty**.

### Integration Criteria

- **I1** `OVT_PatrolHarassmentStabilityModifier` is not edited and still works; its `GetDeploymentNearPosition` lookup is unaffected.
- **I2** Radio-tower sabotage, capture, persistence and the JIP stream are untouched; `OVT_TEST_PersistenceRoundTrip_TowerSabotage_SurvivesSaveAndReload` stays green.
- **I3** The nine base-defense configs and both vehicle patrols still create deployments and still fortify a base concern by concern — the anchor only reorders candidates.
- **I4** `OVT_EGroupOrigin` is **not** renumbered; objective groups carry `DEPLOYMENT` origin through the inherited `TagForGameMaster`, like every other deployment group.
- **I5** The resistance's own FOB system (`OVT_FOBData`, `OVT_DeployFOBAction`, `OVT_UndeployFOBAction`, `OVT_SetPriorityFOBAction`) is untouched; `git diff Scripts/Game/GameMode/Managers/Factions/OVT_ResistanceFactionManager.c` is empty except any shared removal helper T6.2 introduces.
- **I6** `UpdateKnownTargets()` and `GetThreatByLocation()` are untouched — the director has its own selection and does not repurpose the known-target list.
- **I7** `git diff Scripts/Game/GameMode/Civilians/` → **empty**.

### Verification Method

**Automated — from the repo root, in order:**

1. `tools/compile-check.sh` → exit **0**.
2. `tools/run-tests.sh "{6A6E29FF47ECB840}"` → exit **0** (Fast).
3. `tools/run-tests.sh "{6A6E2A002F53A581}"` → exit **0** (All).
4. `grep -rn "counterAttackTimeout\|m_bCounterAttackTimeout" Scripts/ Configs/ Prefabs/` → **empty**. → Q4
5. `grep -rn "StartBaseQRF\|StartTownQRF" Scripts/` → three callers total, all named. → Q4, F14
6. `git diff Scripts/Game/GameMode/Virtualization/ Scripts/Game/GameMode/VirtualMovement/ docs/features/virtualization/core/api.md` → **empty**. → Q5
7. `grep -rn "AddFactionResources" Scripts/Game/GameMode/Objectives/` → **empty**. → Q6, G5
8. `grep -rn "OVT_ObjectiveDirector" Scripts/Game/GameMode/Deployments/` → **empty**. → Q8
9. `git diff Scripts/Game/Persistence/Serializers/Components/OVT_OccupyingFactionManagerSerializer.c Scripts/Game/Persistence/Serializers/Components/OVT_DeploymentComponentSerializer.c` → **empty**. → Q9
10. `git diff Configs/Deployment/Deployment_TownPatrol.conf Configs/Deployment/Deployment_TowerGarrison.conf Configs/Deployment/Deployment_Base*.conf Configs/Deployment/Deployment_VehiclePatrol_*.conf` → **empty**. → Q12
11. `ls Configs/Deployment/Deployment_Objective*.conf | wc -l` → **5 or more** (five plus any harassment ramp variants from T5.7), each with a registry entry. → G1
12. `grep -n "OVT_ModifierConfig" Configs/Modifiers/supportModifiers.conf | tail -1` → the new entry is **last**. → Q10

**Manual — solo play-test.** Debug affordances: consider temporarily lowering `objectiveHarassmentIntervalMinutes` and `objectiveStarvationMinutes` on `Difficulty_Normal.conf` to compress steps 3–8, and note that the OF's time base is `timeMul`-scaled so a higher time multiplier compresses everything.

1. **Start a fresh campaign on Normal and take one town.** Watch the log and the GM panel. **Expect:** an objective is selected within a minute of the capture, with the selection line naming the winner, the runner-up and both scores. → F1, F2
2. **Confirm the retired triggers are gone.** Play several in-game days without provoking anything. **Expect:** no random-base QRF, no instant town QRF. Then capture a base by hand and start an uprising: both still trigger their QRFs. → F14
3. **Watch the first harassment operation end to end** from a distance: truck out, drop at a road short of the town, group walks in, truck goes home. **Expect** no group ever appears from thin air. → F3, G7
4. **Let a harassment mission succeed, then a second.** **Expect:** support steps down twice, and the second group is larger than the first. → F3
5. **Hold a radio tower covering the objective** and kill the recapture team mid-hold, then let a second team finish. **Expect:** no flip the first time, a flip the second. → F4, G10
6. **Make a base you hold the objective**, put four structures of different costs on it, and leave. **Expect:** demolition in ascending cost order, one notification per mission, and — after save/reload — the destroyed ones stay destroyed. → F5
7. **Let Phase 2 fire.** **Expect:** a supply truck, an FOB where you can see the logic of the site, a garrison, and **no notification**. Then reload the save and confirm there is still exactly **one** FOB. → F6, F11
8. **Starve it:** take the supplying base. **Expect:** the FOB comes down within the starvation window and a new objective is chosen. Then re-run to Phase 2 and **remove one by hand** instead: clear the area, hold the flag action, watch it and its garrison vanish and the objective reset. → F7, F8
9. **Let Phase 3 fire and watch where the waves land.** **Expect:** LZs on the occupying-held side, including from the FOB's bearing. Afterwards, win or lose, the FOB is gone and a new objective is selected. → F9
10. **Trigger a player QRF elsewhere during Phase 1** and watch the GM panel. **Expect:** the phase and its timers do not move for the duration. → F12
11. **Watch the GM campaign panel across several operations.** **Expect:** the deployment pool falls by each operation's cost and nothing else moves. → F10
12. **Play a long session (an hour or more) and read the log.** **Expect:** every objective transition — selected, abandoned, blacklisted, reset — carries a reason, and nothing sits still. → F13
13. **Start a second campaign without restarting the client.** **Expect:** the director starts clean, no doubled subscriptions, no stale insertion reservations. → F13
14. **Pacing/tuning pass.** Record: in-game time from selection to Phase 2 and from Phase 2 to Phase 3 on Normal; whether the pool is ever the binding constraint; how often insertion falls back to walking and why; the AI frame cost with two live insertions and an FOB garrison up. Feed the numbers back into the eleven difficulty fields. → [R6](#9-risks--mitigation)
15. **Dedicated-server / MP pass.** The automated spine covers MP not at all. Confirm: the GM panel's objective fields reach a joining client; the FOB dismantle action validates server-side when a client requests it; no client-side error from the new GM record on a JIP.

---

## 7. Testing Strategy

**The automated spine covers the maths, the seams and the round trips. Everything about whether a truck actually reaches a town, whether a site looks sensible, whether the ramp feels like a ramp, and whether any of it works in multiplayer is a play-test.**

**Binding constraints, inherited from `virtualization/base-defense-migration/context.md` §"Gotchas & Learnings" and restated because every one of them applies here:**

- **`CloneModule` copies attributes by hand and silently drops what it forgets** — six new modules here; every one asserts clone fidelity.
- **`SCR_AIGroup.GetOnAgentAdded()` passes ONE argument**; recover the group via `agent.GetParentGroup()`.
- **`ScriptInvoker.Insert` does not de-duplicate** — `Remove` then `Insert`, always.
- **`.conf` module order is update order and `.conf` files cannot carry comments** — spawning, behaviour, reinforcement last among behaviour, then conditions.
- **Init-tier worlds never run `PostGameStart`** — a case needing a tick installs it itself.
- **The autotest camera is an observer** — anything that registers uses `spawnDistanceOverride = 0` (Manual policy). The insertion module's 100 000 makes this **more** dangerous here, not less: any fixture touching it must be eliminated-marked first.
- **Deployment fixtures must be `SetSpawnedUnitsEliminated(true)`** on the deployment **and every spawning module** before anything ticks.
- **The Logic-tier rule is a reviewer grep over the whole directory and it does not distinguish code from comments** (`OVT_TEST_LogicSuite.c:4-24`). Concretely: the strings **`OVT_Global`** and **`GetGame().GetGameMode`** may not appear **anywhere** under `TestSuites/Logic/`, not even in prose. The same ban applies to the *subject* files those tests grep — the four pure-static classes — for the same reason `OVT_GMPanelFormat.c:6-8` carries it.
- **`new` does not apply `[Attribute()]` defvalues** (`OVT_TEST_LogicSuite.c:40-54`) — a hand-built subject must have every field set explicitly, or a test asserts against a zero the game never sees.
- **Logic cases run alphabetically by class name and must be independent**; floats compare with an epsilon (`OVT_TEST_LogicFixture.EPSILON = 0.0001`, `FloatEquals` at `:186`).
- **`RandInt` is max-exclusive; `out` and `owned` are reserved; `vector.Distance` is +1 ULP at 1 000/2 000 m; `PrintFormat` takes at most 3 string params.**
- **No `maxAttempts` anywhere** — the suites are deterministic and every new case carries a can-fail proof.
- **Scope every registry assertion to your own owner key** — the Init and Persistence worlds run a live deployment wave.

### Logic tier — Fast, three new files

`TestSuites/Logic/OVT_TEST_Logic_ObjectiveScaling.c` (Phases 1, 2, 6, 7)
- Difficulty consumption: `RequiredSabotageMissions` clamps unset/zero/negative/absurd to the fallback and passes a sane value through; `HarassmentLadderIndex` saturates at the top rung and never indexes past the array; `FOBBudgetCeiling` derives from `objectiveFOBCost` and `WithinFOBCeiling` is asserted at, below and above.
- Selection: highest score wins; ties go to input order; an all-blacklisted set selects nothing; an empty set selects nothing; `DecayBlacklist` never goes negative and drops at zero.
- Phase gates: `TownPhase2Gate`, `TownPhase3Gate`, `BasePhase3Gate` on both sides of every threshold, including the resource gate and the `fobUp` conjunct.
- Starvation: `IsFOBStarved` true on each of its three inputs independently, false when all three are healthy.
- Sabotage ordering: `NextTargetIndex` picks the cheapest remaining; `-1` on empty/all-destroyed/ragged; ties by input order.
- Siting: `IsInBand` including `min >= max`; `IsClearOfExclusions` with an empty list, a ragged pair and a candidate exactly on a boundary.
- `TickDown` never goes below zero.

`TestSuites/Logic/OVT_TEST_Logic_ObjectiveInsertion.c` (Phase 4)
- `ShouldWalk` on both sides of the threshold and for a negative threshold.
- `LZPointOnLine`: normal case; standoff ≥ separation clamps to the source end and never overshoots past the target; source == target; zero standoff.
- `IsStuck`: fires only when speed is below threshold **for** the tick limit **and** the truck is outside the arrival radius; never fires inside it.
- `HasArrived` on both sides, with tolerance.

`TestSuites/Logic/OVT_TEST_Logic_ObjectiveAnchorAndBearing.c` (Phases 3, 8)
- `ApplyAnchorBias`: unchanged for `radius <= 0`, `weight <= 0` and `distance >= radius`; monotonic in distance; bounded by `score + weight`; never below `score`; the ordering claim of [§3.5](#35-the-objective-anchor--how-the-bias-plugs-into-evaluation-scoring) expressed as two candidates.
- `PreferredDegreesFromSource`: all four cardinals and two diagonals with tolerance; source coincident with target returns a defined value; the **sign** claim asserted explicitly — a source due north of the target yields a bearing that puts the LZ north of the target.
- The GM panel objective formatter for every phase and for no objective.

### Init tier — additions to `TestSuites/Init/OVT_TEST_InitSuite.c` (Fast), plus one seam file

- **Phase 1:** the five shipped presets load and `objectiveSabotageMissionsRequired` is non-increasing Easy → Insane with at least one strict step.
- **Phase 2:** the director resolves via `GetInstance()` and `OVT_Global`; a fresh director is `IDLE`; a driven selection over a fixture set is deterministic; the tick early-returns while a QRF is live **and decrements nothing**.
- **Phase 3:** with no anchor, two consecutive evaluations produce the same candidate ordering; with an anchor, an in-radius candidate outsorts an equal-threat out-of-radius one.
- **Phase 4:** `truck_crew` and `specops_team` resolve for **both** factions; the source provider returns false rather than null with no friendly base; **the insertion module's clone carries every own and inherited attribute**; the reservation counter reserves, releases and refuses past the cap.
- **Phase 5:** both configs resolve and validate; `EvaluateHold` / `EvaluateRecapture` fire once from a driven sequence and never twice; every new module's clone is complete; the new support modifier resolves by name and is **last** in its config.
- **Phase 6:** the sabotage config resolves; the candidate filter excludes structures of another base and of the occupying faction; nothing is destroyed while an enemy is inside the clear radius; the per-mission cap holds.
- **Phase 7:** both FOB configs resolve; the raise module raises nothing on a restored deployment and exactly once on a fresh one; the anchor source provider prefers the FOB; teardown leaves no deployment of either config in the radius.
- **Phase 8:** the GM seam accepts the new record and the state receives all four fields (extend `OVT_TEST_Init_GMRequestSeam.c` — this is the only mechanical defence against the `Rpc()` arity blind spot); the Phase 3 gate fires the starter exactly once.

### Persistence tier — `TestSuites/Persistence/OVT_TEST_PersistenceRoundTripSuite.c` (All)

Two cases, both on the shared gate (`OVT_TEST_PersistenceSuite.RequiresStartedCampaign()` is `true`):

- **The objective round trip** (Phase 2): kind, position, phase, both counters, all three tick counters and a two-entry blacklist survive save → dirty → re-apply.
- **The FOB round trip and its re-link** (Phase 7): every FOB field survives; after restore the director re-links the FOB deployment by name + position on its first tick; a payload naming a deployment that no longer exists **resets the objective** rather than stranding it.

⚠ **The tier's assertion rule is narrow and binding** (`OVT_TEST_PersistenceSuite.c:4-23`): no persistence-framework type, no vanilla persistence type and no Overthrow save-data class may appear **anywhere** under `Scripts/Game/Tests/` except the single documented save-trigger line in `OVT_TEST_PersistenceRoundTripSuite.c`. So a case writes state through the director's **public API**, triggers the save, dirties the live state, re-applies, and reads back through the director's **public getters** — it never names the serializer, the payload or `ApplyPersistedObjective`. Assert **deltas, never absolutes** (`:77-78`), and use the shared resolvers in `OVT_TEST_PersistenceSubject.c` rather than resolving subjects inline.

🔴 **Structural limit, inherited and not to be worked around:** the suite's reload seam (`ReapplyLatestSaveData`) builds its request with `Instances = {gameMode}` only, so a deployment marker's `Deserialize` is never re-run by it. Both cases therefore exercise the game-mode-component half — which is where the director lives, so this is less limiting here than it was for `base-defense-migration` — and take a real save alongside so the write half runs over live state. Say so at the top of the fixture. **Do not widen the seam.**

### Not automatable, and why

| Area | Why manual |
|---|---|
| Whether a truck actually reaches a town over Eden's roads | Needs live AI driving real terrain for minutes; no harness has a road network with traffic |
| Whether the stuck fallback triggers when it should | Needs a truck to genuinely get stuck, which is not reproducible on demand |
| Whether an FOB site looks sensible | A judgement about a place |
| Ramp pacing and whether it *feels* like a build-up | In-game hours and a subjective verdict |
| Support sliding under stacked debuffs | Needs a live town economy over time |
| Sabotage against real player-built structures | Needs a player to have built them |
| Save → quit → **Continue** | The harness restarts the suite on a world transition |
| Two campaigns in one session | Same reason; `virtualization/core`'s Phase 6 found four teardown bugs exactly here |
| MP / JIP, including the GM record and the dismantle request | Uncovered by the whole spine; a dedicated-server pass is the only check |

---

## 8. Dependencies

**Hard preconditions (all satisfied today):**

- **The whole `virtualization` epic, complete (5/5)** — core (frozen `api.md`), movement (the §3.8 seam contract that keeps vehicle-borne groups live), integration (the consumer seam, owner keys, deployment serializer v2, the manager's single subscription), civilians, and base-defense-migration (the placement-provider seam, `OVT_DeploymentSelection`'s pure-static precedent, the single funding path, the 400 per-faction ceiling, `WasRestoredFromSave()`, `StationsGroupsDeliberately()`). **This feature asks core for nothing.**
- **The deployments framework** — the 30 s evaluator, `ForceCreateDeployment`, `GetDeploymentNearPosition`, `DeleteDeployment`, the faction resource pool API, the 250 m name-scoped dedup, `m_iMaxInstances` / `m_fChance` / `m_iPriority`, the marker prefab and both serializers.
- **The QRF controller**, reused as the battle layer, with the LZ cluster of BUG-031 already fixed at HEAD ([C4](#corrections-to-the-requirements-document)).
- **The OF command layer** — `m_Bases`, `m_RadioTowers`, `GetNearestBase`, `GetBasesWithinDistance`, `ChangeRadioTowerControl`, `StartBaseQRF`, `StartTownQRF`, `m_CurrentQRF`, `m_OnBaseControlChanged`.
- **The town system** — `OVT_TownData.SupportPercentage()`, `m_OnTownControlChange`, the modifier system's `TryAddSupportModifierByName` and its positional-index rule.
- **`OVT_InfluenceRules`** — the pure, world-free proximity rules the tower-coverage query reuses rather than re-deriving.
- **The placeable/buildable systems** — `OVT_PlaceableComponent` / `OVT_BuildableComponent` association and ownership, and their configs' `m_iCost`, for the sabotage target ordering.
- **The faction registries** — extended by two entries; the only resolution path since the migration retired the legacy prefab-slot arrays.

**Explicitly NOT depended on:**

- **`resistance/high-command`** — the requirements name it as the eventual measure of "strong resistance presence" at a starving FOB's source base. Until it lands, **player presence suffices**, and the starvation predicate takes that as a boolean argument so swapping the source is a one-line change with no test rewrite.
- **The Intel epic** — no resistance-side surface for the FOB is built here.
- **A QRF-systems upgrade** — QRF waves do not use the insertion module, by explicit exclusion.
- **The epic's open bug cluster** (BUG-025 unvalidated capture RPCs, BUG-027 free QRFs, BUG-028 deployment list leak) — none is this feature's to fix; the new request in Phase 7 must simply not add a fourth.

**Downstream (what this unblocks):** the Intel epic gets a real thing to surface. A QRF-systems feature gets a working insertion module to migrate its waves onto. OF counter-operations against resistance FOBs become a new objective *kind* rather than a new system.

**User-side (interactive):** the §6 play-test list, especially step 14's tuning pass and step 15's dedicated-server pass. The eleven difficulty values in [§3.6](#faction-registry-and-difficulty-additions) are **first guesses**; nothing in the automated spine can tell whether the ramp is too fast or too slow.

---

## 9. Risks & Mitigation

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| **R1** | **The director wedges.** An objective sits in one phase forever because a gate can never pass — a town whose support never reaches 50 %, a base with nothing left to sabotage, an FOB site that can never be found. | High if not designed against | The feature silently does nothing; the OF is passive forever after Phase 1 retired its triggers | Every phase has a **timeout exit** as well as a gate exit: a phase that has run `m_iPhaseTicks` past its budget resets and reselects, logging why. The blacklist stops the same dead objective being re-picked immediately. F13 is a player-visible criterion and play-test step 12 reads the log for it. Sabotage with nothing left to destroy reports success rather than stalling. |
| **R2** | **A second FOB on every load.** The classic static-content duplication — a restored deployment re-runs its raise. | High if not designed against | Save-breaking over a long campaign; visible and absurd | [D11](#d11--the-fob-is-a-deployment-plus-a-tracked-structure-not-a-new-entity-system): `WasRestoredFromSave()` gates the raise, the module is idempotent on its own entity handle, T7.12 asserts both halves, F11 is a player-visible criterion and play-test step 7 checks it explicitly. This is R2 from `base-defense-migration`, restated. |
| **R3** | **Pool accounting breaks.** `ForceCreateDeployment` does **not** debit; a director that forgets to call `SubtractFactionResources` creates free armies, which is exactly the bug class (BUG-026/027/029) the migration just closed. | Medium | The economy becomes decorative again | Every create goes through **one** director method that creates-then-debits, named and headed as such. Q6's grep proves the director never credits. An Init case drives a create and asserts the pool fell by exactly the cost. F10 is a play-test criterion. |
| **R4** | **The bearing sign is backwards** and every counter-QRF wave lands on the far side of the objective. | Medium | The headline Phase 3 promise is inverted, and it looks like a physics bug rather than a sign error | [§3.3 of the QRF task](#phase-8--phase-3-the-counter-qrf-and-the-gm-panel) spells the convention out (`dir` points target → LZ, so the bearing wanted is target → **source**); T8.8 asserts the sign directly as a named case; play-test step 9 is the live check. |
| **R5** | **Inserting the support modifier in the wrong place corrupts live saves.** `m_iIndex` is the positional index in `m_aModifiers` and travels in the replicated per-town modifier lists. | Medium | Every town's modifiers shift by one — silent, save-wide, and hard to diagnose | T5.2 states the append-only rule three times (task, class header, `context.md`), Q10 makes "the new entry is last" a grep-verifiable acceptance criterion, and an Init case asserts the new modifier's index is the final one. |
| **R6** | **The ramp is paced wrong.** Eleven new difficulty values are first guesses; too fast and the resistance is overwhelmed with no warning, too slow and nothing ever happens. | **High** | The feature's entire purpose is pacing | Every value is a difficulty field, not a constant, so tuning needs no code. Play-test step 14 is a dedicated pacing pass whose numbers feed straight back. The director logs every transition with its tick count, so the pass has data rather than impressions. |
| **R7** | **Live insertion is expensive or fails often.** Two always-materialised trucks with crews and passengers driving Eden's roads is more live AI than the campaign usually holds, and Reforger road AI is not reliable over long distances. | Medium-High | Frame cost; groups that never arrive | `objectiveMaxConcurrentInsertions` bounds it (default 2, Easy 1). The **walk fallback is written first and is never optional** (T4.5) — every failure path still delivers the men. `m_fWalkThresholdDistance` means short hops never spawn a truck at all. Play-test step 14 measures the cost and step 3 checks arrival. |
| **R8** | **`CloneModule` silently drops an attribute** on one of six new modules. The standing trap of the module system — it lost `m_fMaxCruiseSpeed` once already. | **High** | Silent wrong behaviour: an insertion with no truck type, a hold timer of zero, a sabotage cap of zero | Every new module hand-writes `CloneModule` copying its own **and all inherited** attributes, and every phase that ships a module asserts clone fidelity mechanically (T4.8, T5.9, T6.8, T7.12). It is the only defence that exists. |
| **R9** | **Sabotage deletes something that comes back**, because the entity was persistence-tracked and only the world copy was removed — the BUG-030 failure class. | Medium | Player property "destroyed" that reappears on load; or worse, an untracked orphan | T6.1 is a **gating read-only survey**: the removal path the player's own dismantle uses is found and named before anything is deleted; T6.2 reuses it or introduces one shared helper and re-points the player path at it. F5's save/reload check is the live proof. |
| **R10** | **The director races the QRF teardown.** A handler on `m_OnFinished` ordered after the OF manager's runs with the controller entity already deleted. | Medium | Use-after-free; a hard crash at the end of every counter-QRF | [D8](#d8--the-director-calls-the-qrf-starters-the-qrfs-resolution-is-polled-not-subscribed): the director **polls** `m_CurrentQRF` going null on its own tick and never subscribes. The reason is written into the method header so nobody "improves" it into a subscription. |
| **R11** | **The re-selection handler reads stale ownership**, because `m_OnBaseControlChanged` fires before the affiliation is applied. | Medium | The director targets the base the player just took, or refuses to re-target when it should | [D3](#d3--re-selection-is-a-flag-never-inline): both handlers set a flag and return; re-selection runs on the next tick. The trap is recorded in the handler's own comment. |
| **R12** | **Insertion reservations leak** and insertion permanently stops after a few campaign hours or one reload. | Medium | Every group walks; the feature quietly loses its most visible mechanic | T4.3 requires an explicit release on **every** exit path, listed in `context.md`; reservations are zeroed on restore and on faction-list teardown; an Init case reserves past the cap and releases back down. Worth a specific look during play-test step 14. |
| **R13** | **Temporary OF passivity is mistaken for a bug** during the window between Phase 1 and Phase 8. | Certain (it is the plan) | Wasted debugging; a misleading play-test report | [D2](#d2--retire-the-legacy-triggers-first-in-phase-1) states it, T1.9 records it in `context.md`, and every phase between 1 and 8 repeats it in its own notes. Play-tests in that window evaluate defence and deployments, not pressure. |
| **R14** | **Concurrent sessions move the tree.** Every `file:line` here was verified 2026-08-18 against the `v1.5` working tree, and bugfix sessions commit into the same branch. This plan has **already** been bitten once — five of the requirements' statements were stale by one day ([C1–C5](#corrections-to-the-requirements-document)). | **High** | Stale references, failed edits, designs built on facts that changed | Four phases open with a **read-only survey task** (T1.1, T3.1, T6.1, T8.1) whose only job is to re-verify before editing. No task depends on a line number for correctness — every one names the symbol as well. |
| **R15** | **The anchor changes where non-objective deployments are created**, and base defence or town patrols degrade across the map. | Low-Medium | A regression in the system that just shipped | [D5](#d5--the-anchor-is-pushed-biases-ordering-only-and-is-absent-by-default): no anchor is byte-identical to today, the anchor biases **ordering not eligibility**, and T3.6 asserts the no-anchor path is unchanged by driving two consecutive evaluations. I3 is an integration criterion. |
| **R16** | **The GM record breaks a JIP client**, because the fan is a wire and a mismatched client mis-parses. `Rpc()` arity is a compile-check blind spot in this tree (BUG-090), and `SendSnapshotEnd` reports a record count an old client would then wait for. | Medium | A client-side error storm during a GM session; a snapshot that never commits | T8.6 adds a **new** pair rather than widening an existing one, bumps both `CAMPAIGN_RECORD_COUNT` and `WIRE_VERSION`, leaves every record class untouched, and adds an Init seam case — the only mechanical check available for RPC arity. T8.7 names the three `OVT_GMCampaignState` methods a new scalar must touch. Play-test step 15 is the live MP check. |
| **R17** | **No authored FOB sites exist on day one.** `OVT_FOBPosition` instances live in world layers and placing them is a Workbench world-editing job nobody has done. If the generated fallback is weak, Phase 2 blacklists every objective in turn and the ramp never completes. | **Certain at first** | The feature's middle phase silently never fires | The **generated path is the primary path and is built and tuned as if the authored one will never exist**; authored markers are an optimisation for map authors, not a dependency. T7.3's siting is bounded-attempt with an ocean check, a clearance trace and a flatness probe, and T7.4 logs every failure with the attempt count so "the OF never builds an FOB" has a diagnosis in the log. Play-test step 7 checks the site quality; authoring a handful of Eden markers is a **follow-up**, not a blocker. |

---

## Agent Routing Summary

| Phase | Agent | Advanced? |
|---|---|---|
| 1 — Retirement + difficulty rewire | `component-developer` | no — contained deletions and config authoring |
| 2 — The director: state machine, selection, persistence **(gate)** | `component-developer-advanced` | **yes** — new persisted game-mode component, load-order hazard, event-ordering trap |
| 3 — The objective anchor in the evaluator | `component-developer-advanced` | **yes** — edits the shared evaluator thirteen shipped configs depend on |
| 4 — The insertion module | `component-developer-advanced` | **yes** — live AI driving, vehicle+crew+passenger lifecycle, shared concurrency counter |
| 5 — Town operations: harassment + tower recapture | `component-developer` | no — new modules and configs; one appended modifier entry under a stated rule |
| 6 — Base operations: sabotage | `component-developer-advanced` | **yes** — permanently destroys player property; integrates two ownership registries |
| 7 — Phase 2: the FOB | `component-developer-advanced` | **yes — the largest phase.** New prefab + entity + tracked structure + spend ceiling + held action + server-validated request |
| 8 — Phase 3: counter-QRF + GM panel | `component-developer-advanced` (hand the `.layout` slice, if any, to `ui-developer`) | **yes** — touches the battle layer and the GM wire, both with live client consumers |
| 9 — Help & documentation sync | `help-docs-sync` | — |

**Skills to activate:** `enforcescript-patterns` (all code phases), `overthrow-architecture` (1–8), `workbench-workflow` (4–8 — prefab and config authoring, and every play-test).

**Estimate:** 85–116 h across the nine phases, of which Phases 2, 4 and 7 are roughly half.
