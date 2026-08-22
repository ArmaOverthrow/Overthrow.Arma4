# Counter Attacks — Implementation Plan

**Status:** Ready for Review
**Started:** 2026-08-19 (build started; plan authored 2026-08-18)
**Target Completion:** TBD
**Last Updated:** 2026-08-19 — **counter-attack QRF mode addendum** (user-specified, this date): the counter-QRF is now a *silent siege* with its own mode on the QRF controller, and it only starts in daylight. New [§3.9](#39-the-counter-attack-qrf-mode--the-silent-siege), new [Phase 9](#phase-9--the-counter-attack-qrf-mode-the-silent-siege), new [D14](#d14--the-siege-is-a-mode-on-the-existing-controller-not-a-second-controller)–[D17](#d17--the-counter-attack-window-is-a-pure-hour-predicate-on-the-phase-3-gate); [G8](#2-goals) and [D9](#d9--the-qrf-changes-are-exactly-two-and-need-no-prerequisite-repair) amended, the docs phase renumbered 9 → 10. Original plan written 2026-08-18 by `solution-architect`; every `file:line` verified against the working tree on that date, and every `file:line` in the addendum verified on 2026-08-19.

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

The QRF controller keeps its job — it is still the only thing that resolves a battle — and gains **one new mode**. A player-initiated assault is unchanged: notification, 120 s countdown, waves, scoring. A counter-attack QRF is a **siege** ([§3.9](#39-the-counter-attack-qrf-mode--the-silent-siege)): it begins silently with no notification and no scoring UI, spends its **whole** budget in a single pass instead of trickling waves, walks those groups into an encirclement 100–150 m around the objective, and only when the last group is on the ground does it tell the resistance anything — at which point a **30-minute muster clock** starts during which nothing is scored and the resistance can gather, dig in, or run. Scoring begins when that clock expires, or early if the resistance kills the whole siege force first. It only ever starts in daylight (05:00–15:00), so a counter-attack is never a night ambush on a sleeping server. On top of the mode, the FOB joins the wave-source list and each wave's landing zone is biased toward the bearing of the source that sent it, so the attack visibly comes from where the occupying faction actually is.

**Expected shape:** strongly net-adding (unlike its predecessor) — roughly 14 new script files, 8 new configs, 1 new prefab, 1 new entity class, 1 new serializer, ~2 800 lines added against ~60 deleted.

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
- **G8 — A player-initiated QRF is byte-for-byte the battle it is today.** The counter-attack siege is a **mode**, entered only by the director. With `m_eMode == STANDARD` the controller takes exactly the paths it takes at HEAD: 120 s countdown, wave scheduling, `CheckUpdatePoints` gated on `m_iTimer <= 0`, the same notification at the same moment. The only unconditional changes to the file are Phase 8's three: `SendTroops`' source list, `SendWave`'s call into `GetLandingZone`, and `GetLandingZone`'s signature and preferred-direction derivation. Everything Phase 9 adds sits behind a mode branch, and F17 verifies the standard path by playing it.
- **G13 — A counter-attack is a siege, not a raid.** It starts unannounced, spends its whole budget in one pass, forms a visible ring at 100–150 m, and gives the resistance a **30-minute scored-nothing window** to react after the reveal. Killing the whole siege force during that window ends it early and wins the battle. It never begins outside 05:00–15:00.
- **G14 — "A QRF exists" and "a QRF is being fought" stop being the same question.** `m_bQRFActive` (a battle object exists → no second battle may start) and `IsQRFEngaged()` (the shooting has started → the world suppresses) are separate, and the client learns about the battle on a third flag, `m_bQRFRevealed`. Nothing suppresses civilians, garrisons, deployments or the economy during a siege the resistance has not been told about.

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
| **Legibility of the ramp** | A player who has never read this document can tell, from the game, that something is building: trucks arriving, support sliding, structures gone, a flag they did not plant. Each phase transition is either visible in the world or announced. **The FOB is the deliberate exception and stays silent.** | §6 F2/F3/F6/F7; play-test steps 3–9; Phase 10 |
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
   AND the world clock is inside 05:00–15:00                                    (D17)
                         ▼
   ┌────────────────────────────────────────────────────────────────────────────────┐
   │ PHASE 3 — COUNTER-QRF  (a SIEGE, not a raid — §3.9)                            │
   │  StartBaseQRF(baseController) or StartTownQRF(town), with mode COUNTER_ATTACK.  │
   │  The existing controller resolves the battle, in three stages:                  │
   │    SILENT_DEPLOY  whole budget in one pass, no notification, no HUD, no map     │
   │                   circle, no scoring, world NOT suppressed. Groups land at the  │
   │                   usual LZs and walk to an even ring 100–150 m out.             │
   │    MUSTER         last group spawned → notify + 30 REAL minutes, still unscored │
   │                   ends early if every siege group is neutralised                │
   │    BATTLE         scoring exactly as today; the world suppresses from here      │
   │  Throughout, m_bQRFActive is true, so the resistance cannot start a battle of   │
   │  its own. This is a deliberate, accepted tell.                                  │
   │  On m_OnFinished, WHATEVER the outcome: FOB torn down, objective RESET.         │
   │  Gate not met only because of the clock → the director WAITS in Phase 2 and     │
   │  keeps harassing; it never abandons an objective for being night.               │
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

> 🔴 **CORRECTED 2026-08-19, during the Phase 3 build. The original snippet here was wrong and would have broken D5's own invariant.** It biased the value passed to the `OVT_CandidatePosition` constructor — but that constructor writes its argument to **two** fields, and only one of them is a sort key:
> - `sortBy` — the ordering key. Biasing this is the whole intent.
> - `threatLevel` — read twice more in the same loop: `FindBestDeploymentConfig` → `CheckDeploymentConditions` uses it as a **hard eligibility gate** (`m_iMinimumThreatLevel`, authored above zero by `Deployment_BaseATSection` 50, `Deployment_BaseHeavyPatrol` 25, `Deployment_VehiclePatrol_Heavy` 1200), and `CreateDeployment` **persists** it via `SetThreatLevel()`, where four Persistence cases assert it.
>
> Biasing the constructor argument would therefore have let an objective-adjacent position **buy configs its real threat cannot afford** and written an inflated threat into the save — exactly the "changes eligibility" failure the ⚠ three paragraphs below forbids. The bias is applied to **`sortBy` only, after construction.** The design stands; only this example was wrong.

```
BEFORE                                                AFTER
base   = CalculateThreatLevel(position, faction)      base   = CalculateThreatLevel(position, faction)
jitter = RandFloatXY(-0.2, 0.2)                       jitter = RandFloatXY(-0.2, 0.2)
final  = base * (1.0 + jitter)                        final  = base * (1.0 + jitter)
candidate = new OVT_CandidatePosition(position, final)  candidate = new OVT_CandidatePosition(position, final)
                                                      ApplyObjectiveAnchorBias(candidate, anchor)
                                                      //  ^ mutates candidate.sortBy ONLY.
                                                      //    candidate.threatLevel is left alone: it gates
                                                      //    config eligibility and it is persisted.
candidatesWithThreat.Insert(candidate)                candidatesWithThreat.Insert(candidate)
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

### 3.9 The counter-attack QRF mode — the silent siege

> **Added 2026-08-19 at the user's direction, after the rest of this plan was written.** Every `file:line` below was verified against the working tree on that date. This subsection is the authority for Phase 9; where it and the requirements document disagree about Phase 3, this subsection wins (the requirements predate it).

A player assaulting a base and the occupying faction coming to take a town back are not the same event, and after this feature they no longer read the same. The QRF controller keeps one job — resolving a battle — and gains one **mode**.

**`STANDARD` (every battle the player starts) is unchanged at HEAD.** Notification the moment it starts, a 120 s countdown on the HUD, troops arriving in waves 4–8 minutes apart (`:342`), zone scoring the instant the countdown reaches zero (`:126`). Nothing in Phase 9 may alter that path; it is the branch `m_eMode == OVT_EQRFMode.STANDARD` skips into.

**`COUNTER_ATTACK` (only the director sets it) is a siege in three stages.**

| | `SILENT_DEPLOY` | `MUSTER` | `BATTLE` |
|---|---|---|---|
| **Entered when** | `Start()` | the spawn queue empties — i.e. the last group is on the ground | the muster clock hits 0, **or** every siege group is neutralised |
| **Notification** | none | **"the occupying faction is assaulting X"**, broadcast + external | — |
| **HUD QRF panel** | hidden | shown; "battle starts in **28 min**", both sliders at 0 | shown; "#OVT-BattleProgress" + the sliders, exactly as today |
| **Map restricted-area circle** | hidden | shown | shown |
| **Fast travel / respawn rules** | **not applied** | applied (today's rules, FOB exemption intact) | applied |
| **Zone scoring** | off | off | on — `CheckUpdatePoints` unchanged |
| **Spawning** | whole budget, one pass, no waves | none | none |
| **Group orders** | walk from the LZ to an assigned ring slot 100–150 m out, `Defend` there | hold the ring | `SearchAndDestroy` on the objective centre |
| **World suppression** (economy, deployments, that town's civilians) | **off — the world lives** | **off** | **on**, as today |
| **Resistance may start its own battle** | **no** | no | no |
| **Typical duration** | seconds to ~1 min (one group per second) | 30 real minutes | until a side reaches `QRFPointsToWin` |

#### Why the world keeps living until `BATTLE`

Three server-side gates currently read "does a QRF object exist" and mean "is a battle being fought": the occupying economy tick (`OVT_OccupyingFactionManager.c:1418`), deployment evaluation (`OVT_DeploymentManager.c:577`) and the objective town's civilian crowd (`m_OnQRFTownChanged.Invoke` at `OVT_OccupyingFactionManager.c:1162`). Under a 30-minute siege those three would empty the target town of civilians, freeze every deployment on the map and stall the faction's income for half an hour *before the resistance is told anything* — the loudest possible tell, and a dead world besides. All three move onto a new accessor:

```
bool IsQRFEngaged()   // OVT_OccupyingFactionManager
    return m_CurrentQRF && m_CurrentQRF.IsEngaged();
```

`IsEngaged()` is `true` for a `STANDARD` QRF from the moment it is created — **so standard battles behave exactly as they do today** — and `true` for a `COUNTER_ATTACK` QRF only in `BATTLE`. `m_CurrentQRF` keeps its other meaning untouched: a battle object exists, no second one may start, the director stands down.

⚠ The civilian invoke is a **transition**, not a poll: `StartTownQRF` fires it inline at `:1162` and `OnQRFFinishedTown` fires it again at `:1249`. In counter-attack mode the *first* invoke moves to the `BATTLE` transition. It must still be paired — a siege resolved during `MUSTER` (early end straight into `BATTLE`) still passes through `BATTLE`, so the pairing holds by construction; a siege that could ever skip `BATTLE` would leak a suppressed town, and nothing may introduce such a path.

#### Three flags, three questions

| Flag | Lives on | Replicated | Answers |
|---|---|---|---|
| `m_bQRFActive` | OF manager (`:169`) | yes, via `RpcDo_SetQRFActive/Inactive` (`:1957`, `:2008`) | *May a new battle start? May this player capture / rise up?* Set for the whole siege, from `SILENT_DEPLOY`. **Unchanged.** |
| `m_bQRFRevealed` | OF manager — **new** | yes, new `RpcDo_SetQRFRevealed(bool)` beside the others | *Does the client know?* Drives the HUD panel, the map circle and the travel/respawn rules. `true` from creation for `STANDARD`; `true` at the `MUSTER` transition for `COUNTER_ATTACK`. |
| `IsQRFEngaged()` | OF manager — **new**, server-only | no | *Is the shooting on?* Drives the three world-suppression gates. |

The client-side consumers each gain one conjunct and nothing else:

- `OVT_EconomyInfo.c:79` → `if(m_OccupyingFaction.m_bQRFActive && m_OccupyingFaction.m_bQRFRevealed)`
- `OVT_MapRestrictedAreas.c:327` → the same conjunct
- `OVT_FastTravelService.c:108` and `OVT_RespawnService.c:220` → the same conjunct (**this is the one behavioural change to a shipped rule**: during `SILENT_DEPLOY` a player may still fast-travel to and respawn in the objective, because nobody has told them not to. Without it, a fast-travel refusal is a free reveal.)
- `OVT_SleepService.c:227` and `OVT_GMPanelUIComponent.c:532` keep reading `m_bQRFActive` — a Game Master is *meant* to see the siege forming, and sleeping through an incoming assault should be refused from the moment it is incoming.

#### The spawn pass, and the ring

`SendTroops()` (`:245`) already computes the budget; in counter-attack mode only the **shape of the spend** changes:

- `SendWave()` (`:309`) loops until `m_iResourcesLeft <= 0` instead of allocating one bounded slice per source, and **does not** schedule a follow-up wave (`:342` is skipped). The 16-groups-per-source clamp still bounds a single source; the loop simply cycles the source list until the budget is gone. The debit at `:345-352` is unchanged and still runs once per pass — ⚠ it is the only place a QRF debits `m_iResources`, and a mode that spends in one pass must still pass through it exactly once.
- Spawn positions stay the existing `GetLandingZone()` points (250–750 m, bearing-biased by Phase 8), so nothing pops in beside a player standing in the objective.
- Each queued group is additionally assigned a **ring slot**: with `N` groups queued, slot `i` sits at bearing `360/N * i` from the objective centre at a radius rolled in `[SIEGE_RING_MIN (100), SIEGE_RING_MAX (150)]`, rejected and re-rolled inward if it lands in the ocean (`OVT_WorldUtils.IsOceanAtPosition`, the predicate `GetTargetZone` already uses at `:487`). The ring is computed **once, from the final queue length**, after the spawn pass fills the queue and before the first group spawns — assigning slots as groups spawn would clump them.
- `SpawnFromQueue()` (`:418`) keeps its one-per-second cadence (it is the frame-load spreader) and its `OVT_GMGroupRegistry.Tag(..., OVT_EGroupOrigin.QRF, -1, "QRF")` call at `:435`. Only the waypoint block at `:440-443` branches: counter-attack groups get a single `Defend` waypoint on their ring slot instead of the Scout/Scout/SaD/SaD ladder aimed at the centre.
- At the `BATTLE` transition every surviving group is issued `SearchAndDestroy` on `GetTargetZone(...)` through the existing `AddWaypoint` (`:403`) — the assault proper. ⚠ `SCR_AIGroup.AddWaypoint` appends; the `Defend` waypoint must be **removed** first or the group finishes defending before it attacks.

#### The muster clock, and ending early

`m_iTimer` (`:15`) is reused as-is — it is already a millisecond countdown ticked by `CheckUpdateTimer` (`:64-79`) and already gates scoring at `:126`. The mode changes when and how far it counts:

```
CheckUpdateTimer():                        // one second, real time, unchanged cadence
  STANDARD          → exactly as today: spawn below 105 000, decrement, publish
  SILENT_DEPLOY     → drain the queue; DO NOT decrement; publish nothing
                      queue empty → m_iTimer = MUSTER_TIME (1 800 000); stage = MUSTER;
                                    manager.RevealQRF()  (notification + m_bQRFRevealed)
  MUSTER            → decrement; publish (see the rate note); every 10 s check the early end
                      m_iTimer <= 0            → stage = BATTLE
                      whole force neutralised  → m_iTimer = 0, stage = BATTLE
  BATTLE            → nothing to do; CheckUpdatePoints owns it
```

**30 real minutes, not in-game minutes** (user decision, 2026-08-19): the window exists so players can physically drive there, so it must not shrink with `timeMul`. `MUSTER_TIME = 1 800 000` fits an `int` with three orders of magnitude to spare.

⚠ **Publication rate.** `UpdateQRFTimer` (`:1064`) broadcasts an RPC to every client on **every tick**; over a 30-minute muster that is 1 800 broadcasts where a standard QRF sends 120. Publish every **10 s** while more than 120 s remain, then every second — and render minutes while more than 120 s remain (`"#OVT-BattleStartsInMinutes"` + `Math.Ceil(ms / 60000)`), seconds below it via today's `"#OVT-BattleStartsIn"` string (`OVT_EconomyInfo.c:286-289`). A 10 s granularity is invisible on a minutes display and the final two minutes tick as they do today.

**Early end — "if all of the spawned enemy is neutralised, scoring begins."** Checked on the 10 s cadence during `MUSTER` only, over `m_Groups` (`:434`), reusing `IsFightingFit` (`:110`):

```
a group counts as NEUTRALISED when
    its entity is null / deleted                                   → dead
 OR it has ≥ 1 agent and none of them are IsFightingFit(...)        → dead
a group with ZERO agents but a live entity counts as ALIVE         → see the trap below
early end fires when every tracked group is neutralised AND at least one was ever tracked
```

🔴 **The trap this shape exists for:** "0 agents = dead" is a **known-bad** prune in this engine — the AI spawn queue and dormancy can legitimately report a group with no agents (recorded in `docs/features/.../reforger-1.8-update` and still unfixed at HEAD). Siege groups are spawned live and never virtualised, so the case should not arise, but a false positive here **hands the resistance an instant win**, which is the worst possible failure. The zero-agent case therefore resolves to ALIVE, deliberately.

Once `BATTLE` is entered by the early path, scoring runs unmodified: with no occupying AI left inside `QRF_RANGE` and any resistance inside `QRF_POINT_RANGE`, `CheckUpdatePoints` awards +5/tick and the resistance wins in `QRFPointsToWin / 5` ticks. That is the intended reward for wiping a siege before it lands, and it needs no new code.

#### Daylight only

The Phase 3 gate gains one conjunct: the world clock must be inside **05:00–15:00**. See [D17](#d17--the-counter-attack-window-is-a-pure-hour-predicate-on-the-phase-3-gate). A gate blocked only by the clock is **not** a failure — the director stays in Phase 2, keeps harassing, and fires on the next tick inside the window.

#### What does not change

- **Persistence: nothing.** No mode, no stage, no ring, no muster clock is serialized. A live QRF still rolls back cleanly on load (`§3.8`), and a director restored in `COUNTER_QRF` with no live QRF resets on its first tick — which now also covers a save taken mid-siege. Say so in `context.md`; it is the kind of omission a later reader files a bug against.
- **Resolution.** `OnQRFFinishedBase/Town` (`:1165`, `:1204`), `ChangeBaseControl`, the town outcome table and `m_OnFinished` are untouched. A siege that loses flips nothing, exactly as today.
- **`KillAll()`** (`:81`) is untouched and still unreferenced by this feature.
- **Survivors stay** after the battle, per commit `e115965`.

---

## 4. Implementation Phases

Ten phases. Each leaves the tree compiling and the campaign playable. (Phase 9 — the counter-attack QRF mode — was added on 2026-08-19; the documentation phase moved 9 → 10. Phases 1–8 are unchanged.) **No phase ships a module without the config that uses it, and no phase ships a config whose modules do not exist.**

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
5. **T1.5 — Author the twelve new difficulty fields** per [§3.6](#faction-registry-and-difficulty-additions), in the `Occupying Faction` category, each with a `desc:` a tuner can act on. Author the per-preset values in all five shipped presets; `Difficulty_TestWorld.conf` authors none.
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
   - **The cost join, which does not exist yet.** The live entity carries only a **type string** — `OVT_PlaceableComponent.GetPlaceableType()` / `OVT_BuildableComponent.GetBuildableType()` — and nothing in the tree joins that back to the config to reach `m_iCost`. That lookup is new code and belongs in one place.
     > 🔴 **CORRECTED 2026-08-19 during the Phase 6 build: do NOT join on `m_sName`.** The type string and the display name **disagree for seven of the eight shipped buildables** — `"GuardTower"`/`"Guard Tower"`, `"RecruitmentTent"`/`"Recruitment Tent"`, `"Bunker"`/`"Bunkers"`, `"VehicleGarage"`/`"Garage"`, `"FuelDepot"`/`"Fuel Depot"`, `"MedicalTent"`, `"VehicleMaintenanceRamp"`; only `"Helipad"` happens to line up. A name join would have priced almost everything at **zero** and so demolished the **garage first** — the exact inverse of the authored cheapest-first design, and a failure no compile or type check could catch. **Join on the prefab `ResourceName`** (`OVT_PrefabUtils.GetPrefabName(entity)` against `m_aPrefabs`): exact, needs no data re-authoring, and survives a restore. An unmatched prefab must sort **last**, never first.
   - **The enumerator, which also does not exist.** There is **no registry of placed structures**; discovery is a sphere query. `OVT_ItemLimitChecker.CountItemsForLocation(locationId, baseType, searchCenter)` (`:216`, radius **500 for `EOVTBaseType.BASE`**) is the exact shape — with `FilterItemCallback` (`:244`, either component present) and `CountItemCallback` (`:279`, association match). The sabotage enumerator is that method **collecting instead of counting**.
   **Nothing is deleted until this table exists.**
2. **T6.2 — Reuse, do not reinvent.** Destruction goes through the single path T6.1 settled on. ⚠ Do not raw-delete: the navmesh carve must be captured **before** the entity goes, or the AI keeps pathing around a building that is not there.
3. **T6.3 — `OVT_BaseSabotageBehaviorDeploymentModule`** per [§3.6](#36-the-new-modules-configs-registry-and-difficulty-additions). Enumerate candidates in the T6.1 shape; filter to structures associated with this base and owned by the player faction; order **ascending by cost**; destroy one per `objectiveSabotageHoldSeconds` while the hold condition is met; stop at `objectiveSabotageStructuresPerMission` or when nothing is left, then report success. The shipped costs make the requirements' example come out exactly right: Bunkers 750 → Recruitment Tent / Medical Tent 1000 → Guard Tower 1200 → Vehicle Maintenance Ramp / Helipad 1500 → Fuel Depot 2000 → **Garage 8000** (`Configs/Resistance/buildables.conf`). ⚠ There is **no `m_iSize` attribute anywhere** — cost is the only ordering key that exists, and that is a deliberate choice, not an oversight.
4. **T6.4 — One notification per mission, not per structure.** The first destruction of a mission sends `SendTextNotification("ObjectiveSabotage", -1, baseName)` through `OVT_Global.GetNotify()` (`OVT_NotificationManagerComponent.SendTextNotification(tag, playerId = -1, p1, p2, p3)` at `:77`; `-1` is the broadcast default and there is **no faction-scoped send**). The preset goes in **`Configs/overthrowBroadcastMessages.conf`** — not a `Configs/Notifications` directory, which does not exist — with a fresh GUID and `Name`/`Description` keys in `Language/localization_Overthrow.st`. This is a **deliberate addition beyond the letter of the requirements**, justified by the Quality Bar's legibility row: a structure vanishing with no explanation is the opposite of a readable ramp. ⚠ It is **not** an intel surface for the FOB, which stays silent by explicit requirement. ⚠ `PrintFormat` takes at most 3 string params.
5. **T6.5 — Author `Deployment_ObjectiveSabotage.conf`:** insertion module with `specops_team`, sabotage behaviour, reinforcement with `m_bDeleteOnConditionFail 1`, `OVT_BaseControlConditionDeploymentModule` with `m_bRequireControl 0` (deploy only while the resistance holds it — the same inversion knob), objective condition, `m_iAllowedLocationTypes BASE`.
6. **T6.6 — Director wiring:** `TickHarassment()` for a BASE objective creates sabotage operations on the same cadence, with the same pool debit; `OnSabotageSuccess()` increments `m_iSabotageSuccesses` and re-checks the Phase 2 gate.
7. **T6.7 — Logic-tier:** `NextTargetIndex` returns the cheapest not-yet-destroyed index; returns `-1` on an empty list, an all-destroyed list and a ragged input; ties go to input order; a negative cost sorts first without crashing.
8. **T6.8 — Init-tier:** the config resolves and validates; the module's candidate filter excludes a structure associated with a *different* base and one owned by the occupying faction; the module destroys **nothing** while an enemy is inside the clear radius; `m_iStructuresPerMission` is respected.
9. **T6.9 — `context.md`:** the T6.1 table, the removal-path decision, and an explicit statement of what the player permanently loses so the Phase 10 documentation can be truthful.

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

### Phase 8 — Phase 3: the counter-QRF, the daylight gate, and the GM panel

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
13. **T8.13 — The daylight conjunct** ([D17](#d17--the-counter-attack-window-is-a-pure-hour-predicate-on-the-phase-3-gate), added 2026-08-19). Add `static bool IsCounterAttackWindow(int hour, int startHour, int endHour)` to the objective-scaling pure static — a half-open `[start, end)` window that **handles wrap** (`start > end` means the window crosses midnight) even though the shipped values do not wrap, and add it as a conjunct to both Phase 3 gates. Read the hour the way the OF manager already does: `m_Time = world.GetTimeAndWeatherManager()` then `TimeContainer time = m_Time.GetTime()` (`OVT_OccupyingFactionManager.c:1389-1394, :1416`), guarding the null manager the same way. Consts `COUNTER_ATTACK_HOUR_START = 5` / `COUNTER_ATTACK_HOUR_END = 15` live on the director, **not** in `OVT_DifficultySettings` — the user asked for a fixed window, and promoting them to authored fields later is a two-line change. ⚠ A gate blocked **only** by the clock logs at most once per objective (not once per tick — it is a once-a-minute tick and the block can last in-game hours) and does **not** count toward any starvation or timeout counter.
14. **T8.14 — Logic-tier for the window:** inside, both boundaries (05:00 in, 15:00 out — half-open), outside, midnight, and a wrapping window (22 → 4) on both sides of both its edges.

**Acceptance criteria**

- compile **0**; All green.
- `git diff Scripts/Game/Controllers/OccupyingFaction/OVT_QRFControllerComponent.c` touches **only** `SendTroops`' source insertion, `SendWave`'s call, and `GetLandingZone`'s signature + preferred-direction derivation. Countdown, budgeting, scoring, waypoints and resolution are untouched. (**This criterion is scoped to Phase 8.** The siege mode lands in Phase 9 and widens this file deliberately; at the end of *this* phase the QRF still fires as a standard, loud, waved battle.)
- `grep -n "COUNTER_ATTACK_HOUR_START\|IsCounterAttackWindow" Scripts/` finds the consts on the director and the predicate in the pure static — and **nothing** in `OVT_DifficultySettings.c`.
- `git diff Scripts/Game/GameMode/GM/OVT_GMRecords.c` → **empty** (no per-entity record class changes).
- The GM state's existing fields are in their original order; the two new ones are last, and both appear in `CopyFrom` **and** `Clear`.
- `grep -n "CAMPAIGN_RECORD_COUNT\|WIRE_VERSION" Scripts/Game/Components/Controller/OVT_GMRequestComponent.c` → **3** and **2**.
- `grep -rn "Rpc\|RplProp" Scripts/Game/UI/GM/OVT_GMPanelUIComponent.c` → **empty**.
- A QRF triggered by the player still behaves exactly as before (its sources have no FOB and its LZ falls back to the authored direction).

---

### Phase 9 — The counter-attack QRF mode (the silent siege)

**Agent:** `component-developer-advanced` — **advanced.** It restructures the timer and spawn paths of a 561-line server-only component that every battle in the game runs through, and it touches four shipped client-facing rules (HUD, map, fast travel, respawn). A regression here breaks player-initiated battles, which are the feature's most-used path and the one nobody is testing while working on counter-attacks.
**Estimate:** 10–14 h
**Suite after this phase:** **All**.
**Authority:** [§3.9](#39-the-counter-attack-qrf-mode--the-silent-siege). Read it before writing anything; it carries the stage table, the flag table and the traps.

**Tasks**

1. **T9.1 — Read-only survey, and a baseline.** Re-verify the eleven `file:line` anchors §3.9 cites (`OVT_QRFControllerComponent.c` `:15`, `:64-79`, `:110`, `:126`, `:245`, `:309`, `:342`, `:345-352`, `:418`, `:434-443`; `OVT_OccupyingFactionManager.c` `:169`, `:1064`, `:1162`, `:1418`, `:1957`, `:2008`). Record the verdict in `context.md`. **Then write down, before changing anything, what a standard QRF does second by second** — it is the thing you must not break, and T9.11 checks you against this note.
2. **T9.2 — The enums and the mode field.** `OVT_EQRFMode { STANDARD, COUNTER_ATTACK }` and `OVT_EQRFStage { SILENT_DEPLOY, MUSTER, BATTLE }` in one new file under `Scripts/Game/Controllers/OccupyingFaction/`. On the controller: `m_eMode` (defaults `STANDARD`), `m_eStage`, `bool IsEngaged()`. ⚠ `m_eMode` must be set by the caller **before** `Start()` — `SpawnQRFController` (`OVT_OccupyingFactionManager.c:1278`) returns the component and both starters configure it before calling `Start()` (`:1091`, `:1143`), so follow that existing order rather than adding a parameter to `Start()`.
3. **T9.3 — `OVT_QRFSiege`, the pure static** (`Scripts/Game/Controllers/OccupyingFaction/OVT_QRFSiege.c`). No world, no entity, no manager, no `OVT_Global` — **including in comments** (the Logic-tier rule is a directory-wide grep). It owns: `RingSlotBearing(int index, int count)`; `RingSlotOffset(int index, int count, float radius)` returning a local offset in the same 0°=North=`-Z` convention `GetRandomDirection` documents at `:416-419`; `ShouldPublishTimer(int remainingMs, int lastPublishedMs)`; `FormatMusterRemaining(int ms)`'s numeric half (minutes above 120 s, seconds below); and `AllNeutralised(int tracked, int neutralised)`. Everything that can be decided without the world is decided here.
4. **T9.4 — The single-pass spend.** In `SendWave()` (`:309`), branch on the mode: cycle the source list until `m_iResourcesLeft <= 0`, and skip the follow-up `CallLater` at `:342`. ⚠ **The debit at `:345-352` must still run exactly once for the pass** — it is the only place a QRF debits `m_iResources`, and double-debiting or skipping it re-opens BUG-027 in a new place. ⚠ Bound the loop with an iteration counter as well as the budget: a source list that allocates 0 per pass (possible if `baseResourceCost` is misauthored) would otherwise spin forever on the server thread.
5. **T9.5 — The ring.** After the spawn pass fills the queue and **before** the first group spawns, compute one ring slot per queued group from the final queue length (§3.9 — computing them as groups spawn clumps them), rolling the radius in `[SIEGE_RING_MIN 100, SIEGE_RING_MAX 150]` and re-rolling inward when `OVT_WorldUtils.IsOceanAtPosition` rejects a slot (the predicate `GetTargetZone` already uses at `:487`). Store them alongside the existing parallel spawn arrays (`m_aSpawnQueue` / `m_aSpawnPositions` / `m_aSpawnTargets`, `:30-32`) — ⚠ these arrays are index-parallel and `SpawnFromQueue` removes index 0 from each (`:445-447`); a fourth array must be removed in the same place or every later group gets the wrong slot.
6. **T9.6 — Orders.** In `SpawnFromQueue` (`:418`), counter-attack groups get one `Defend` waypoint on their ring slot instead of the Scout/Scout/SaD/SaD ladder (`:440-443`). At the `BATTLE` transition, remove that waypoint from every surviving group and add `SearchAndDestroy` on the objective — ⚠ `AddWaypoint` appends (`:403-410`), so without the removal the group finishes defending before it attacks.
7. **T9.7 — The stage machine in `CheckUpdateTimer`** (`:64-79`), exactly as §3.9 spells it out. ⚠ The `m_iTimer < 105000` spawn condition (`:66`) is a *standard-mode* expression of "wait 15 s for the world to despawn"; in `SILENT_DEPLOY` the timer does not move at all, so the drain needs its own condition — and the 15 s wait is **not needed** in counter-attack mode because nothing is being suppressed yet ([§3.9](#39-the-counter-attack-qrf-mode--the-silent-siege) "Why the world keeps living"). Publish through `UpdateQRFTimer` only when `ShouldPublishTimer` says so.
8. **T9.8 — The early end.** On the existing 10 s `CheckUpdatePoints` cadence, in `MUSTER` only. Implement the neutralised test **exactly** as §3.9 states, 🔴 **including the zero-agent-means-ALIVE rule** — a false positive hands the resistance an instant win. Write the reason in a comment at the test, not just in the docs.
9. **T9.9 — The reveal, on the manager.** `m_bQRFRevealed` + `RpcDo_SetQRFRevealed(bool)` beside the existing pair (`:1957`, `:2008`), set `true` at creation for `STANDARD` (so nothing changes for player battles) and at the `MUSTER` transition for `COUNTER_ATTACK` via a new `RevealQRF()` that also sends the notification. ⚠ **`Rpc()` is an untyped variadic prototype — a wrong argument count compiles clean and dies silently at the wire (BUG-090).** Arity-diff the new pair by eye against `RpcDo_SetQRFTimer` (`:2002`). ⚠ Add the flag to the JIP payload the same way the others are carried, and note in `context.md` that `m_iCurrentQRFBase/Town` are *already* missing from it (a known defect, not this phase's to fix — do not quietly widen the payload contract beyond the one new flag).
10. **T9.10 — `IsQRFEngaged()` and the three world gates.** Add the accessor to the OF manager; move `OVT_OccupyingFactionManager.c:1418`, `OVT_DeploymentManager.c:577` and the first `m_OnQRFTownChanged.Invoke` (`:1162`) onto it per §3.9. ⚠ The civilian invoke is a paired transition (`:1162` / `:1249`) — moving the first one must not break the pairing; assert that a siege always passes through `BATTLE`.
11. **T9.11 — The client conjuncts, and the standard-path proof.** One conjunct each in `OVT_EconomyInfo.c:79`, `OVT_MapRestrictedAreas.c:327`, `OVT_FastTravelService.c:108`, `OVT_RespawnService.c:220`. Leave `OVT_SleepService.c:227` and `OVT_GMPanelUIComponent.c:532` on `m_bQRFActive` (§3.9). Then re-read the T9.1 baseline note and confirm line by line that a `STANDARD` QRF still does all of it.
12. **T9.12 — The HUD's minutes form.** `OVT_EconomyInfo.c:286-289`: minutes above 120 s via a new `#OVT-BattleStartsInMinutes`, today's seconds string below it. ⚠ **`Language/localization_Overthrow.st` is the only file you may edit** — the `.conf` exports are Workbench build output; add the new keys with a `Comment` each and **tell the orchestrator a re-export is owed**. Never hand-edit an export.
13. **T9.13 — Notifications.** New `CounterAttackBase` / `CounterAttackTown` tags in `Configs/overthrowBroadcastMessages.conf` (the `BaseBattle` entry at `:109-111` is the model) plus their `.st` entries, sent from `RevealQRF()` through `SendTextNotification` **and** `SendExternalNotifications`, matching the pairing at `OVT_OccupyingFactionManager.c:1102-1103`. Cities use the town tag; villages are never objectives.
14. **T9.14 — Logic-tier:** a new `TestSuites/Logic/OVT_TEST_Logic_QRFSiege.c` covering every `OVT_QRFSiege` static — ring bearings for 1, 2, 3 and 12 groups sum to a full circle and never repeat; slot 0 is due north of the centre; the offset convention matches `GetRandomDirection`'s (the **sign** asserted explicitly, as T8.10 does for bearing); the publish predicate on both sides of the 120 s boundary; the minutes/seconds crossover at exactly 120 000 ms; `AllNeutralised` for 0-of-0 (**false** — never fire with nothing tracked), 0-of-N, partial and N-of-N.
15. **T9.15 — Init-tier:** a controller created with `m_eMode = COUNTER_ATTACK` reports `IsEngaged() == false` in `SILENT_DEPLOY` and `MUSTER` and `true` in `BATTLE`; a `STANDARD` controller reports `true` immediately; the stage advances `SILENT_DEPLOY → MUSTER` on a driven empty queue and sets the muster clock to 1 800 000; the early-end predicate does not fire on a fixture whose one group reports zero agents. ⚠ Fixture groups must be eliminated-marked before anything ticks, per the binding constraints in [§7](#7-testing-strategy).
16. **T9.16 — `context.md`:** the T9.1 verdict and baseline note, the flag table, the zero-agent decision and why, the parallel-array trap, the fact that **no siege state is persisted** and a mid-siege save rolls the battle back, and the owed localization re-export.

**Acceptance criteria**

- compile **0**; All green.
- **A player-initiated QRF is unchanged, verified by playing one** — notification immediately, 120 s countdown, waves, scoring at zero. This is the phase's primary risk and a green suite does not cover it.
- `grep -n "m_eMode\|m_eStage" Scripts/Game/Controllers/OccupyingFaction/OVT_QRFControllerComponent.c` — every read is a branch that leaves the `STANDARD` side on today's code path.
- `grep -rn "m_CurrentQRF" Scripts/` → the OF manager's own uses, the GM flag at `OVT_GMRequestComponent.c:564`, and **nothing else**: the economy tick, the deployment evaluator and the civilian transition all read `IsQRFEngaged()`.
- `grep -rn "m_bQRFRevealed" Scripts/` → the manager, its RPC, and exactly four client consumers.
- `grep -rn "OVT_Global\|GetGame()" Scripts/Game/Controllers/OccupyingFaction/OVT_QRFSiege.c` → **empty**, comments included.
- `git diff Language/*.conf` → **empty** (only the `.st` master is edited; the re-export is owed and reported).
- `git diff Scripts/Game/GameMode/Virtualization/ Scripts/Game/GameMode/VirtualMovement/` → **empty**.

---

### Phase 10 — Help & documentation sync

**Agent:** `help-docs-sync`
**Estimate:** 3–4 h
**Suite:** **skipped — docs-only.** Say so.

Players see substantially different enemy behaviour, so the closing sync is in scope.

1. **T10.1** Fact-check **every** existing sentence in `Configs/Tutorials/` and `Configs/FieldManual/` about enemy counter-attacks, QRFs, defending a town and defending a base against the shipped code, and cite a `file:line` or cut the sentence. The project has shipped invented mechanics twice; no gate catches a well-formed lie.
2. **T10.2** The player-visible changes to document:
   - **counter-attacks are no longer random.** The occupying faction picks one target and works toward it; you can see which one and prepare.
   - **the three phases and what each looks like** — trucks bringing groups in, support sliding in a harassed town, radio towers being retaken, structures at your base being demolished, an enemy FOB appearing between their nearest base and your town.
   - **an enemy FOB is not announced** — you find it, and pulling it down needs the area cleared and a held action on its flag.
   - **starving an FOB works**: take or empty the base supplying it, or keep a strong presence there, and it comes down on its own.
   - **the QRF now arrives from where they actually are**, including from the FOB.
   - **a counter-attack surrounds you before it announces itself.** The first sign may be that you cannot start a battle of your own. When the encirclement is complete you are told, and you then have **30 real minutes** before anything is decided — that time is yours to gather, dig in, call recruits, or leave. Nothing is scored during it.
   - **the ring is about 100–150 m out** from the centre of the town or base, and it is worth scouting before the clock runs down.
   - **wipe them all before the 30 minutes are up and the battle ends there** — the assault never happens and the ground is yours.
   - **counter-attacks come in daylight**, roughly between 05:00 and 15:00. They will not start on you at night.
   - **difficulty changes how much warning you get** — easier settings demand more sabotage missions before the assault.
   - **what you permanently lose to sabotage** (the T6.9 list), stated plainly.
3. **T10.3** Wiki: the same points plus the operator-facing notes — the twelve new difficulty fields and what each does, the removal of `counterAttackTimeout`, and the fact that objective operations spend the same deployment pool as everything else.
4. **T10.4** Epic bookkeeping: add `counter-attacks` to `docs/features/occupying/epic-overview.md`'s feature table, refresh its Tech Debt section (the QRF LZ half of BUG-031 is fixed; the "no OF tower-recapture path" regression from C1 is closed), and update the epic's row in the master `docs/overview.md`.

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

> ⚠ **Amended 2026-08-19.** "Exactly two" is now **exactly two *unconditional* changes, plus a mode** ([§3.9](#39-the-counter-attack-qrf-mode--the-silent-siege), [D14](#d14--the-siege-is-a-mode-on-the-existing-controller-not-a-second-controller)). Everything below still stands for Phase 8 and for the `STANDARD` path, which is the path this decision was protecting; the siege additions land in Phase 9 and are all behind a mode branch. The "deliberately excluded" list at the end of this decision is **unchanged and still binding** — Phase 9 does not get to fix `m_Bases` staleness or the cosmetic dead code either.

The brief asked for an explicit decision on whether fixing the LZ-cache globals is a prerequisite of bearing bias. **It is not, because they no longer exist** ([C4](#corrections-to-the-requirements-document)): commit `d7e42362` removed `Goodqrfpos`/`Goodqrfbasepos`, made each wave source resolve its own LZ, fixed the `TracePosition` no-op, and fixed the 0°/360° wrap. All three halves of BUG-031's LZ cluster are closed at HEAD.

So the unconditional scope is exactly what the requirements say and nothing more:

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

> **D14–D17 were added 2026-08-19 with [§3.9](#39-the-counter-attack-qrf-mode--the-silent-siege).**

### D14 — The siege is a mode on the existing controller, not a second controller

The alternative was an `OVT_SiegeControllerComponent` beside the QRF's, leaving `OVT_QRFControllerComponent` untouched. Rejected on three counts.

**The battle layer is a singleton by design.** `m_CurrentQRF` is one slot; "one battle at a time" is a property the whole epic leans on — the director's freeze, the world suppression, the player-capture block, the map circle, the HUD panel all key off it. A second controller type means every one of those grows a second thing to ask, and the invariant stops being checkable by reading one field.

**The siege *is* a QRF for 90 % of its life.** Budgeting, source bases, landing zones, spawn pacing, the group registry tag, zone scoring, `QRFPointsToWin`, the win/loss handoff to `OnQRFFinishedBase/Town` — all of it is wanted verbatim. A separate class either duplicates ~400 lines or inherits from the one it was meant to leave alone.

**The differences are genuinely small and genuinely local.** They are: when the timer runs, how the budget is spent in one pass, where groups walk, and who has been told. Four branches on one enum, in a file that already branches on `m_iTimer`.

The cost is real and is accepted: a mistake in this file breaks player-initiated battles too. That is why Phase 9 is an advanced-agent phase, why T9.1 demands a written baseline of standard-mode behaviour before any edit, and why an explicit "play a normal QRF" acceptance criterion exists — the automated spine cannot see it.

### D15 — Three flags, because "a QRF exists", "a QRF is being fought" and "the players know" are three questions

Before this feature they were one question with three answers, and it worked because a QRF became all three at once. A siege breaks that: it exists for up to 31 minutes before it is fought, and it is fought-in-secret for a minute before anyone is told.

Conflating them is what would produce the two worst bugs available here — a town whose civilians vanish half an hour before the resistance is told anything (the tell that ruins the whole mechanic), and a resistance that can start a second battle because the siege has not "really" begun (the one thing the user explicitly required against). Splitting them is a new bool, a new server-only accessor, and one added conjunct at seven call sites, each verifiable by grep. See §3.9's flag table for which is which.

⚠ The default of every new flag is chosen so that **`STANDARD` behaves exactly as it does at HEAD**: revealed at creation, engaged at creation. A player battle should be incapable of taking a new code path.

### D16 — The muster window is real minutes, and the early end is biased toward "still alive"

**Real minutes** (user decision): the window exists so players can physically drive to the fight, and `timeMul` must not shrink it. In-game minutes at a typical 6× would leave five real minutes, which is not a mustering window, it is a warning shot.

**The early end resolves ambiguity toward ALIVE.** The check answers "is the whole siege force dead?", and it is asked of `m_Groups`, whose entries can be deleted, empty, or full of corpses. The engine's "zero agents" state is *not* reliable evidence of death — the spawn queue and dormancy both produce it, a known-unfixed hazard at HEAD. A false positive here does not cost a little accuracy; it ends the battle and hands the resistance the objective for free, silently, in a way no log would explain. So a group with a live entity and no agents counts as alive, and the early end additionally requires that at least one group was ever tracked (`AllNeutralised(0, 0)` is **false**). The failure mode this leaves is the benign one: a siege that should have ended early instead waits out its clock.

### D17 — The counter-attack window is a pure hour predicate on the Phase 3 gate

The user's rule is "no counter-attack at night — roughly 05:00 to 15:00". It belongs on the **gate**, not in the controller: a siege that has begun should finish whatever the clock does, and only the *decision to begin* is time-of-day sensitive. Starting no later than 15:00 also means the muster window and the battle both land in daylight without any second check.

It is a **pure static** (`IsCounterAttackWindow(hour, start, end)`) for the same reason every other gate in this feature is: it is a Logic-tier case with no world in it. It handles a wrapping window even though the shipped values do not wrap — the predicate is the natural place for that, and an operator who later authors 22 → 04 gets a correct answer instead of a silent never.

> 🔴 **CLARIFIED 2026-08-19 during the Phase 8 build — the original wording below was too broad and was read literally, correctly.** "No starvation tick" meant *waiting for daylight must not count as the objective failing*: no phase timeout, no reselect, no blacklist. It did **not** mean suspending starvation. The distinction is the difference between a clock the director runs against itself (freeze it) and **the player's counterplay** (never freeze it). Starvation responds to facts about the world — the supplying base taken or emptied, a strong resistance presence — and those are true at night too. Freezing it means a player who kills the supplying garrison at 22:00 watches the forward base stand frozen for hours and then launch a counter-attack from a base whose garrison is already dead, which contradicts [F7](#functional-criteria) outright and punishes a correct play.
>
> **The window gates firing the counter-QRF and nothing else.** During the wait the forward-base phase stays live: starvation is evaluated and its counter ticks, the garrison sender runs, and the FOB can be starved down or dismantled exactly as it can at midday. Only the **phase timeout** is held, and the wait still never reselects or blacklists. Gate the phase-timeout advance specifically — not the whole tick.

The bounds are **consts on the director, not `OVT_DifficultySettings` fields.** The user asked for a fixed window; the difficulty block already grows by eleven fields in this feature; and promoting two consts to authored fields later is a two-line change, while un-shipping two authored fields is a save-format conversation. ⚠ Being outside the window is **not** a failure of the objective: no starvation tick, no timeout, no reselect, no blacklist. The director simply keeps doing Phase 2 until morning, and logs the wait **once**, not once per tick.

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
- **F9 — Phase 3 arrives from the right direction.** Let the gate pass. **Expect:** a QRF on the objective whose troops land **on the side the occupying faction actually holds**, including from the FOB — not on a uniformly random bearing. Whatever the outcome, the FOB comes down and a new objective is chosen.
- **F10 — Resource accounting is closed.** Watch the GM campaign panel across several director operations and one 6-hour resource tick. **Expect:** the deployment pool falls by exactly the cost of each operation created, the reserve behaves exactly as it did before this feature, and no number moves unexplained. **The occupying faction never gains resources from anything the director does.**
- **F11 — The whole objective survives a Continue.** Save mid-Phase-2 with an FOB up and some sabotage successes banked, quit, **Continue**. **Expect:** the same objective, the same phase, the same counters, **exactly one** FOB structure, and the ramp resumes where it stopped.
- **F12 — A QRF freezes the director.** Trigger a player-initiated QRF elsewhere on the map during Phase 1. **Expect:** the objective's operations stop being created and its timers do not advance for the duration; afterwards they resume from where they were. (A *standard* QRF also freezes the economy, deployments and its own town's civilians from the moment it starts, exactly as it does today — see F15/F17 for how a siege differs.)
- **F15 — A counter-attack begins in silence, and the first tell is the one we accepted.** Let the Phase 3 gate pass with a player in the objective town. **Expect:** no notification, no QRF panel on the HUD, no circle on the map — but the capture and uprising actions are refused, and enemy groups are walking in from several directions. Fast travel and respawn into the town still work during this stage. Civilians are still going about their business.
- **F16 — The ring forms, then the clock starts.** Keep watching. **Expect:** groups take up positions in a rough circle 100–150 m out from the centre rather than driving straight in; when the last of them is on the ground the notification fires, the HUD shows a countdown in **minutes**, and both battle sliders sit at zero and stay there for the full **30 real minutes**. Nothing is won or lost during it. In the last two minutes the display switches to seconds.
- **F17 — A player-initiated battle is exactly the battle it was.** Capture a base yourself. **Expect:** the notification immediately, the 120 s countdown in seconds, troops arriving in waves over several minutes, scoring the moment the countdown reaches zero, and the town's civilians and the faction's economy freezing as they always did. **Nothing about the siege appears anywhere in this battle.**
- **F18 — Wiping the siege ends it early.** During the 30-minute window, kill every attacker. **Expect:** scoring starts as soon as the last one falls rather than at the end of the clock, and with no enemy left within 750 m the resistance takes the points quickly and wins. Conversely, leaving one group alive in a far corner does **not** end it early.
- **F19 — Counter-attacks come in daylight.** Play several in-game days with a ripe objective. **Expect:** every counter-attack begins between roughly 05:00 and 15:00; a gate that ripens at 22:00 waits until morning while harassment continues, and the log says so once. No objective is ever abandoned for being night.
- **F13 — The machine never wedges.** Play for an extended session. **Expect:** every objective either progresses, is abandoned for a stated reason, or is blacklisted and replaced — and every one of those transitions appears in the log with its reason. No objective sits in one phase indefinitely with nothing happening.
- **F14 — The retired triggers are gone.** Play for several in-game days without provoking anything. **Expect:** no QRF ever fires on a random base, and no QRF fires the instant a town's support crosses 25 %. Player-initiated capture and uprising still work exactly as before.

### Quality Criteria

- **Q1** `tools/compile-check.sh` exits **0** with no output.
- **Q2** Fast `{6A6E29FF47ECB840}` and All `{6A6E2A002F53A581}` both exit **0** at the end of every phase.
- **Q3** Every new test case carries a recorded proof that it can fail — the exact edit, in a preamble comment. **No `maxAttempts` anywhere.**
- **Q4 The legacy triggers are gone:** `grep -rn "counterAttackTimeout\|m_bCounterAttackTimeout" Scripts/ Configs/ Prefabs/` → **empty**; `grep -rn "StartBaseQRF\|StartTownQRF" Scripts/` → exactly the two player-initiated callers plus the director.
- **Q5 Core and movement are untouched:** `git diff Scripts/Game/GameMode/Virtualization/ Scripts/Game/GameMode/VirtualMovement/ docs/features/virtualization/core/api.md` → **empty** across the whole feature.
- **Q6 One funding path:** `grep -rn "AddFactionResources" Scripts/` → the OF manager's single credit point and the deployment framework's **two** refunds — `OVT_MultiTownPatrolBehaviorDeploymentModule.RecoverResources()` and `OVT_DeploymentManagerComponent.RecallDeployment()` — **and nothing in `Scripts/Game/GameMode/Objectives/`, comments included**. Every director spend goes through `SubtractFactionResources` immediately after a successful create.

  > ⚠ **AMENDED 2026-08-19, WITH THE REASON, EXACTLY AS THE CRITERION'S OWN WORDING INVITES.** It read "the deployment framework's own refund" (singular) until the idle-clock/recall fix. That fix had to make an abandoned objective hand back what it had spent on operations that never arrived — a play-test deleted a sabotage team mid-walk *and* its 100 resources, and the pool never saw them again. A credit in the director would have been a fourth kind of funding path and would have broken the **`Objectives/` clause**, which is the load-bearing half. So the credit went into the **framework**, beside the one that was already there: `RecallDeployment()` is "delete and refund what is recoverable", the director calls it and never touches a pool. **The `Objectives/` clause is unchanged and still empty**; what changed is only the enumeration of the framework's own refund points, from one to two.
  >
  > **[G5](#primary) is not weakened.** A refund is paid at most once per deployment and only for work bought and never delivered: `RecallDeployment()` zeroes `m_iResourcesInvested` **before** it credits, so a second call reads 0; the director's ledger is cleared by the same teardown, so a lookup cannot find the deployment twice; a force flagged eliminated pays nothing (a team the player killed is a loss, not a recall); and a *standing* forward base or garrison is never recalled at all, because its money bought exactly what it was for.
- **Q7 Logic-tier grep clean:** no manager, game-mode, world, entity or `OVT_Global` identifier in any of the four pure-static files or their Logic-tier test files, **comments included**.
- **Q8 The dependency points one way:** `grep -rn "OVT_ObjectiveDirector" Scripts/Game/GameMode/Deployments/` → **empty**.
- **Q9 The save formats did not move:** `git diff` on `OVT_OccupyingFactionManagerSerializer.c`, `OVT_PersistedBase`, `OVT_PersistedRadioTower`, `RplSave`, `RplLoad`, `OVT_DeploymentComponentSerializer.c` and `OVT_DeploymentManagerSerializer.c` → **empty**. The director's own serializer is version 1 and append-only.
- **Q10 The modifier config appended:** `git diff Configs/Modifiers/supportModifiers.conf` shows exactly one entry, **at the end of `m_aModifiers`**.
- **Q11 The GM wire is append-only:** `git diff Scripts/Game/GameMode/GM/OVT_GMRecords.c` → **empty**; `OVT_GMCampaignState`'s pre-existing fields are in their original order.
- **Q12 Shipped configs are byte-identical:** `git diff Configs/Deployment/Deployment_TownPatrol.conf Configs/Deployment/Deployment_TowerGarrison.conf Configs/Deployment/Deployment_VehiclePatrol_*.conf Configs/Deployment/Deployment_Base*.conf` → **empty**.
- **Q13 The siege never leaks into a standard battle:** every read of `m_eMode` / `m_eStage` in `OVT_QRFControllerComponent.c` is a branch whose `STANDARD` side is today's code path, and `grep -rn "m_bQRFRevealed" Scripts/` returns the manager, its RPC and exactly four client consumers (`OVT_EconomyInfo`, `OVT_MapRestrictedAreas`, `OVT_FastTravelService`, `OVT_RespawnService`).
- **Q14 The three world gates moved together:** `grep -rn "m_CurrentQRF" Scripts/` returns only the OF manager's own uses and `OVT_GMRequestComponent.c:564`; the economy tick, the deployment evaluator and the civilian transition all read `IsQRFEngaged()`.
- **Q15 Localization exports untouched:** `git diff Language/` shows **only** `localization_Overthrow.st`, every new key carries a `Comment`, and the owed re-export is stated in `context.md` and reported to the user.
- **Q16 The siege statics are pure:** `grep -rn "OVT_Global\|GetGame()\|GetWorld" Scripts/Game/Controllers/OccupyingFaction/OVT_QRFSiege.c` → **empty**, comments included.

### Integration Criteria

- **I1** `OVT_PatrolHarassmentStabilityModifier` is not edited and still works; its `GetDeploymentNearPosition` lookup is unaffected.
- **I2** Radio-tower sabotage, capture, persistence and the JIP stream are untouched; `OVT_TEST_PersistenceRoundTrip_TowerSabotage_SurvivesSaveAndReload` stays green.
- **I3** The nine base-defense configs and both vehicle patrols still create deployments and still fortify a base concern by concern — the anchor only reorders candidates.
- **I4** `OVT_EGroupOrigin` is **not** renumbered; objective groups carry `DEPLOYMENT` origin through the inherited `TagForGameMaster`, like every other deployment group.
- **I5** The resistance's own FOB system (`OVT_FOBData`, `OVT_DeployFOBAction`, `OVT_UndeployFOBAction`, `OVT_SetPriorityFOBAction`) is untouched; `git diff Scripts/Game/GameMode/Managers/Factions/OVT_ResistanceFactionManager.c` is empty except any shared removal helper T6.2 introduces.
- **I6** `UpdateKnownTargets()` and `GetThreatByLocation()` are untouched — the director has its own selection and does not repurpose the known-target list.
- **I7** `git diff Scripts/Game/GameMode/Civilians/` → **empty**. The civilian suppression change is a change to *when the OF manager fires* `m_OnQRFTownChanged` (`:1162`), not to the subscriber — `OVT_CivilianAmbienceManagerComponent` is not edited.
- **I8 (added 2026-08-19)** Every rule that currently keys off `m_bQRFActive` still fires for a standard QRF at the same moment it does today: capture and uprising actions and their server validators (`OVT_CampaignRequestComponent.c:148`, `OVT_UprisingRequestComponent.c:48`, `OVT_CaptureBaseAction.c:7,:25`, `OVT_StartUprisingAction.c:6,:21`) are **not edited at all** — they read "a battle exists", which is exactly what they should read during a siege.
- **I9 (added 2026-08-19)** `OVT_SleepService.c:227` and `OVT_GMPanelUIComponent.c:532` are **not edited**: sleep is refused and a Game Master can see the battle from the moment it exists, deliberately.

### Verification Method

**Automated — from the repo root, in order:**

1. `tools/compile-check.sh` → exit **0**.
2. `tools/run-tests.sh "{6A6E29FF47ECB840}"` → exit **0** (Fast).
3. `tools/run-tests.sh "{6A6E2A002F53A581}"` → exit **0** (All).
4. `grep -rn "counterAttackTimeout\|m_bCounterAttackTimeout" Scripts/ Configs/ Prefabs/` → **empty**. → Q4
5. `grep -rn "StartBaseQRF\|StartTownQRF" Scripts/` → three callers total, all named. → Q4, F14
6. `git diff Scripts/Game/GameMode/Virtualization/ Scripts/Game/GameMode/VirtualMovement/ docs/features/virtualization/core/api.md` → **empty**. → Q5
7. `grep -rn "AddFactionResources" Scripts/Game/GameMode/Objectives/` → **empty**, comments included. → Q6, G5
7a. `grep -rn "AddFactionResources" Scripts/ | grep -v Scripts/Game/Tests/` → **five** lines: the declaration, `RecallDeployment`'s call, the patrol module's recovery, and the OF manager's credit point plus its own header comment. → Q6, G5
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

### Logic tier — Fast, four new files

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
- `IsCounterAttackWindow` (Phase 8, T8.14): inside; **both boundaries** of the half-open window (05:00 in, 15:00 out); outside; midnight; and a wrapping window (22 → 04) on both sides of both its edges.

`TestSuites/Logic/OVT_TEST_Logic_QRFSiege.c` (Phase 9, added 2026-08-19)
- `RingSlotBearing` for 1, 2, 3 and 12 groups: bearings are evenly spaced, span a full circle, never repeat, and slot 0 is due north.
- `RingSlotOffset`: the **sign** convention asserted explicitly against `GetRandomDirection`'s documented 0° = North = `-Z` (a mirrored ring is the phase's most likely silent defect, exactly as the bearing sign is Phase 8's); magnitude equals the radius for every slot; radius 0 returns the zero vector.
- `ShouldPublishTimer` on both sides of the 120 000 ms boundary and at it; never publishes twice for the same value.
- `FormatMusterRemaining`'s numeric half: minutes above 120 s (rounding **up**, so a 30-minute clock reads "30" not "29"), seconds below, and the crossover asserted at exactly 120 000 ms.
- `AllNeutralised`: **`(0, 0)` is false** — the early end must never fire with nothing tracked; plus 0-of-N, partial, and N-of-N.

### Init tier — additions to `TestSuites/Init/OVT_TEST_InitSuite.c` (Fast), plus one seam file

- **Phase 1:** the five shipped presets load and `objectiveSabotageMissionsRequired` is non-increasing Easy → Insane with at least one strict step.
- **Phase 2:** the director resolves via `GetInstance()` and `OVT_Global`; a fresh director is `IDLE`; a driven selection over a fixture set is deterministic; the tick early-returns while a QRF is live **and decrements nothing**.
- **Phase 3:** with no anchor, two consecutive evaluations produce the same candidate ordering; with an anchor, an in-radius candidate outsorts an equal-threat out-of-radius one.
- **Phase 4:** `truck_crew` and `specops_team` resolve for **both** factions; the source provider returns false rather than null with no friendly base; **the insertion module's clone carries every own and inherited attribute**; the reservation counter reserves, releases and refuses past the cap.
- **Phase 5:** both configs resolve and validate; `EvaluateHold` / `EvaluateRecapture` fire once from a driven sequence and never twice; every new module's clone is complete; the new support modifier resolves by name and is **last** in its config.
- **Phase 6:** the sabotage config resolves; the candidate filter excludes structures of another base and of the occupying faction; nothing is destroyed while an enemy is inside the clear radius; the per-mission cap holds.
- **Phase 7:** both FOB configs resolve; the raise module raises nothing on a restored deployment and exactly once on a fresh one; the anchor source provider prefers the FOB; teardown leaves no deployment of either config in the radius.
- **Phase 8:** the GM seam accepts the new record and the state receives all four fields (extend `OVT_TEST_Init_GMRequestSeam.c` — this is the only mechanical defence against the `Rpc()` arity blind spot); the Phase 3 gate fires the starter exactly once; the gate **refuses at 22:00 and passes at 06:00** with every other input identical.
- **Phase 9 (added 2026-08-19):** a controller with `m_eMode = COUNTER_ATTACK` reports `IsEngaged() == false` in `SILENT_DEPLOY` and `MUSTER` and `true` in `BATTLE`, while a `STANDARD` controller reports `true` immediately; a driven empty spawn queue advances `SILENT_DEPLOY → MUSTER` and sets the muster clock to 1 800 000; the early-end predicate does **not** fire on a fixture group that reports zero agents with a live entity; `m_bQRFRevealed` is true at creation for `STANDARD` and false for `COUNTER_ATTACK`. ⚠ Fixture groups are eliminated-marked before anything ticks.

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
| Whether a player-initiated QRF still behaves as it always did (F17) | The suites assert seams, not a battle's second-by-second feel; this is the phase's primary regression risk and only a play-test sees it |
| Whether the ring reads as an encirclement rather than a clump | A judgement about a place, and it depends on live AI pathing from the LZ |
| Whether 30 minutes is the right muster window | Pacing; a subjective verdict over several battles |
| A JIP client arriving mid-siege (does it see the right thing at each stage?) | Needs a second machine; the JIP payload is uncovered by the spine |

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
| **R4** | **The bearing sign is backwards** and every counter-QRF wave lands on the far side of the objective. | Medium | The headline Phase 3 promise is inverted, and it looks like a physics bug rather than a sign error | [§3.3 of the QRF task](#phase-8--phase-3-the-counter-qrf-the-daylight-gate-and-the-gm-panel) spells the convention out (`dir` points target → LZ, so the bearing wanted is target → **source**); T8.10 asserts the sign directly as a named case; play-test step 9 is the live check. |
| **R5** | **Inserting the support modifier in the wrong place corrupts live saves.** `m_iIndex` is the positional index in `m_aModifiers` and travels in the replicated per-town modifier lists. | Medium | Every town's modifiers shift by one — silent, save-wide, and hard to diagnose | T5.2 states the append-only rule three times (task, class header, `context.md`), Q10 makes "the new entry is last" a grep-verifiable acceptance criterion, and an Init case asserts the new modifier's index is the final one. |
| **R6** | **The ramp is paced wrong.** Twelve new difficulty values are first guesses; too fast and the resistance is overwhelmed with no warning, too slow and nothing ever happens. | **High** | The feature's entire purpose is pacing | Every value is a difficulty field, not a constant, so tuning needs no code. Play-test step 14 is a dedicated pacing pass whose numbers feed straight back. The director logs every transition with its tick count, so the pass has data rather than impressions. |
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
| **R18** | **Phase 9 breaks player-initiated QRFs.** The siege restructures the timer and spawn paths of the one component every battle in the game runs through, and the most-used battle — a player capturing a base — is the one nobody is playing while building counter-attacks. | **High** | The feature's own path works and the shipped one silently regresses: no notification, no waves, a battle that never scores | [D14](#d14--the-siege-is-a-mode-on-the-existing-controller-not-a-second-controller): every siege behaviour is behind a mode branch whose `STANDARD` side is today's code, and every new flag defaults so a standard battle takes no new path. T9.1 requires a **written second-by-second baseline before any edit**, T9.11 checks the finished code against it, Q13 makes the branch structure grep-verifiable, and **F17 is a play-test criterion in its own right** — a green suite does not cover this. |
| **R19** | **The early end fires on a live siege force**, because a group momentarily reports zero agents. | Medium | The resistance is handed the objective for free, silently, with nothing in the log to explain it | [D16](#d16--the-muster-window-is-real-minutes-and-the-early-end-is-biased-toward-still-alive): the zero-agent state resolves to **ALIVE**, `AllNeutralised(0, 0)` is false, and both rules carry the reason in a comment at the test site and a Logic case each. The residual failure is the benign one — a siege that waits out its clock when it could have ended early. |
| **R20** | **The siege is not as silent as intended.** A tell the design did not account for — a suppressed civilian crowd, a fast-travel refusal, a map circle, an RPC storm — reveals the target before the reveal. | Medium | The mechanic's whole point is the reveal being a moment; a leaked target makes the silent stage merely an unexplained restriction | The three world-suppression gates move onto `IsQRFEngaged()` and the two travel rules onto `m_bQRFRevealed` ([§3.9](#39-the-counter-attack-qrf-mode--the-silent-siege)), each grep-verified by Q13/Q14. The **accepted** tell is stated up front and only one: a player who tries to start a battle of their own is refused. F15 is the live check for the rest. |

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
| 8 — Phase 3: counter-QRF + GM panel + daylight gate | `component-developer-advanced` (hand the `.layout` slice, if any, to `ui-developer`) | **yes** — touches the battle layer and the GM wire, both with live client consumers |
| 9 — The counter-attack QRF mode (the silent siege) | `component-developer-advanced` | **yes** — restructures the timer and spawn paths every battle in the game runs through, and moves four shipped client-facing rules onto a new flag |
| 10 — Help & documentation sync | `help-docs-sync` | — |

**Skills to activate:** `enforcescript-patterns` (all code phases), `overthrow-architecture` (1–9), `workbench-workflow` (4–9 — prefab and config authoring, and every play-test).

**Estimate:** 95–130 h across the ten phases, of which Phases 2, 4, 7 and 9 are roughly half.

**Owed to the user at the end:** a **localization re-export from Workbench** — Phase 9 adds `#OVT-BattleStartsInMinutes` and the two counter-attack broadcast messages to `Language/localization_Overthrow.st`, and the `.conf` exports are build output that must never be hand-edited. Raw `#OVT-` keys on screen after Phase 9 mean the re-export is outstanding, not that the strings are wrong.
