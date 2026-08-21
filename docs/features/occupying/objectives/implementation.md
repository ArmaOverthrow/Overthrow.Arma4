# Objectives — Implementation Plan

**Status:** Ready for Review
**Started:** 2026-08-21
**Target Completion:** TBD
**Last Updated:** 2026-08-21 (all 8 phases built; All 439/440 green)

**Epic:** `occupying` — see `docs/features/occupying/epic-overview.md`
**Requirements:** `docs/features/occupying/objectives/requirements.md` — **authoritative**. D1–D6 are settled; its Out of Scope list is binding. R1–R8 are the requirement set and are not restated here.
**Proof of concept / parity reference:** `docs/features/occupying/counter-attacks/` — **CLOSED 2026-08-20**. Its `context.md` (5,478 L) is the authoritative record of *why* each shipped rule exists. **The shipped code is the parity reference, not the prose.**
**Pattern being mirrored:** the deployment framework — `docs/features/occupying/deployments/implementation.md` + `context.md`.
**Consumes:** `docs/features/virtualization/core/api.md` — 🔒 **FROZEN**. This feature asks core for nothing; every group is created through a deployment module.

> **What this feature is.** `OVT_ObjectiveDirectorComponent` is 5,201 lines and every rule the occupying faction follows is hard-coded inside it: which targets score how, which phases exist and in what order, which deployment configs each phase may send and how often, the gates between phases, and ~1,500 lines of forward-base handling. It works — it was play-tested to closure on 2026-08-20 — and it cannot be extended without editing it.
>
> This feature keeps every rule and moves it into authored data. An **objective registry** of **plans**; each plan an ordered list of **phases**; each phase a bag of **condition / operation / abort modules** authored in Workbench `.conf`, in exactly the shape `OVT_DeploymentRegistry` / `OVT_DeploymentConfig` / `OVT_Base*DeploymentModule` already established one layer down. The director becomes a runner. A modder adds a doctrine by writing a `.conf`.
>
> **It is a refactor with a parity requirement, and that shapes the whole plan.** It is built as a **strangler**, not a rewrite: the framework lands first, the two shipped plans run through *legacy shim modules* that call the existing methods verbatim, and then one phase of doctrine at a time is replaced by real modules and the methods it wrapped are deleted in the same implementation phase. Every phase compiles, tests and plays; every phase can be diffed against a still-present reference.

---

## Corrections and confirmations against the working tree (verified 2026-08-21, branch `1.5-objectives`, clean)

| # | Statement | Verified state |
|---|---|---|
| C1 | The requirements' R6 asks for **migration** from the counter-attacks save record | **Dropped by the author, 2026-08-21.** v1.5 is unreleased. Ship the version-first record only; an unrecognised record is detected, logged loudly, discarded, and the objective re-selected — the same clean-abandon path R6 already requires for "phase or config missing on load". No field mapping, no Persistence-tier migration case. **The "enum values are frozen in saves" constraint dies with it** (`OVT_ObjectiveRecords.c:1-13` carries that warning and must be rewritten, not obeyed). See [D2](#d2--no-migration-the-old-record-is-detected-discarded-and-logged). |
| C2 | The requirements' R2 keeps `IsFOBUp`/`GetFOBPosition` as **thin wrappers** over `IsAssetUp(key)`/`GetAssetPosition(key)` until consumers move | **Do not ship the wrappers.** The consumers move inside this feature, in Phase 1, and the old names are deleted. See [D4](#d4--the-asset-api-rename-happens-in-phase-1-with-no-wrappers) — and note the verification problem it creates, because this is the one change the Init tier will not catch behaviourally. |
| C3 | `OVT_BaseRepairBehaviorDeploymentModule` + `Deployment_ObjectiveRepair.conf` are objective doctrine | **They are not.** The config authors `m_bDirectorOnly 0` (last line of `Configs/Deployment/Deployment_ObjectiveRepair.conf`), carries **no** `OVT_ObjectiveConditionDeploymentModule`, and the module's own header states "there is no objective director anywhere in the file (plan D16) — it is evaluator-selectable maintenance, not a director operation". `grep -rn "OVT_ObjectiveDirector" Scripts/Game/GameMode/Objectives/Modules/OVT_BaseRepairBehaviorDeploymentModule.c` is empty. It moves **out**, to the deployments framework. See [D3](#d3--the-repair-module-moves-out-rename-the-file-never-the-config-name). |
| C4 | The epic overview cites the FOB starvation presence check at `OVT_ObjectiveDirectorComponent.c:2325-2332` | **Stale — it is now `:2982-2990`** (`IsPlayerAtFOB()`), and it measures presence **at the forward base itself** via `OVT_WorldUtils.PlayerInRange(m_FOB.position, difficulty.baseCloseRange)`. The requirements-era prose that says "at its source base" is wrong. **Port the code.** (The drift in the line number in one day is itself the argument for [R7](#9-risks--mitigation).) |
| C5 | `context.md`'s T6.9 block quotes the sabotage/harassment operation interval as "45 in-game minutes on Normal" | **45 is Hard.** `Configs/Difficulty/Difficulty_Normal.conf:10` authors `objectiveHarassmentIntervalMinutes 60`; Easy 90, Hard 45, Extreme 30, Insane 20. The attribute's own `defvalue` is 45 (`OVT_DifficultySettings.c:81`), which is where the confusion came from. Ported modules read the **difficulty field**, never the defvalue. |
| C6 | `OVT_DeploymentRegistry.ValidateAllConfigs()` is the model for R8's startup validation | It is the right *shape* but it is **dead code** — `grep -rn "ValidateAllConfigs" Scripts/` returns only its own declaration at `OVT_DeploymentRegistry.c:77`. The objective validator must ship **with a call site** (`PostGameStart`), or R8 is decorative. Wiring the deployment one is out of scope. |
| C7 | The deployment module clone contract is `CloneModule()` → `CopyTo()` | The base declares both (`OVT_BaseDeploymentModule.c:114-132`) but **no concrete module in the tree overrides `CopyTo`** — every one overrides `CloneModule()` and hand-builds `new ConcreteType()`. Mirror what is used, not what is declared. |
| C8 | A deployment config's persistence key is its file name | **It is `m_sDeploymentName`** (`OVT_DeploymentComponentSerializer.c:74-78` writes it, `OVT_DeploymentRegistry.FindConfigByName` at `:20-28` resolves it). The `.conf` **file** name is not a key — the registry references it by GUID with the path as a hint (`Configs/Deployment/overthrowDeployments.conf:47`). This is what makes C3's rename safe. |

---

## 1. Executive Summary

`OVT_ObjectiveDirectorComponent` currently answers five questions in one file: *what should we attack*, *what phase are we in*, *what may we buy right now*, *how do we run a forward base*, and *when do we start the battle*. This feature keeps every answer and moves four of the five into authored `.conf` data.

**The object model** is the deployment framework's, one level up:

| Deployments | Objectives |
|---|---|
| `OVT_DeploymentRegistry` (`configRoot`, on the game-mode prefab) | `OVT_ObjectiveRegistry` |
| `OVT_DeploymentConfig` — a thing that can exist | `OVT_ObjectiveConfig` — a **plan**: a selector + an ordered `m_aPhases[]` |
| — | `OVT_ObjectivePhase` — a stable name, a module bag, a cadence, an anchor radius |
| `OVT_Base{Condition,Spawning,Behavior}DeploymentModule` | `OVT_BaseObjective{Condition,Operation,Abort}Module` |
| `OVT_DeploymentSourceProvider` (*where does the force come from*) | `OVT_ObjectiveTargetResolver` (*where does this operation go*) |
| `m_sDeploymentName` is the persistence key | `m_sObjectiveName` + `m_sPhaseName` are the persistence keys |

**The director keeps its runner.** The one-minute tick, the `m_CurrentQRF` early return, the "no players → no tick" rule, the restore-pending resolve, reselect-is-a-flag, the reserve-floor push/drop, the evaluator-anchor push/drop, the blacklist, the idle-clock semantics, the refusal-log dedup and the single create-then-debit spend choke point are **kept in place and not rewritten** — they are the play-tested parts and they are not doctrine.

**It is built as a strangler.** Phase 2 lands the whole framework with the two shipped plans authored **in full**, but each of their three phases carries a *legacy shim module pair* that calls `TickHarassment()` / `TickFOB()` / the counter-attack gate verbatim. Behaviour at the end of Phase 2 is byte-identical because it is the same code, reached through one more indirection. Phases 3–6 then replace one shim at a time with real modules, deleting the director methods that shim wrapped **in the same phase that replaces them**. The last shim dies in Phase 6 and the classes go with it.

**Expected shape:** strongly net-*reducing* in the director and net-adding in modules and configs. Roughly **24 new script files** (registry, config, phase, instance, 4 module base classes, ~11 concrete modules, 4 resolvers, 2 selectors), **3 new configs** (`overthrowObjectives.conf` + two plans), **1 file relocation + 1 rename**, one serializer format bump, and one GM wire field changing type.

**The move budget, measured** (methods plus their own doc-comment blocks — ~40 % of this file is design-rationale prose attached to individual methods, and a comment moves with its method):

| Concern | Lines | Destination |
|---|---:|---|
| Forward base (39 methods) | **1,580** | `OVT_RaiseForwardBaseObjectiveOperation` + `OVT_AssetStarvedObjectiveAbort` (Phase 5) |
| Operations (22) | 803 | ~300 to `SendDeployment` + conditions (Phase 4); **~500 stays** (create-then-debit, refusal log, idle clock) |
| Phase machinery (19) | 742 | ~340 to conditions and the phase attributes; **~400 stays** (timers, `EnterPhase`, `ResetObjective`) |
| Selection (17) | 461 | ~210 to the two selectors (Phase 3); **~250 stays** (commit, blacklist, control-change flags) |
| Battle (9) | 288 | `OVT_StartBattleObjectiveOperation` + `DaylightWindow` (Phase 6) |
| Lifecycle (9) / public read API (32) / persistence (3) / reserve (2) / anchor (2) | 748 | **stays** |
| Header, fields, 4 attributes and **49 doctrine constants** (lines 1-578) | 578 | ~380 of the constants become module and phase attributes |

**Realistic end state: the director around 2,000–2,200 lines** (from 5,201), with ~2,700 moved into modules and ~300 deleted as scaffolding the config replaces. ⚠ **Do not set a lower bar than this** — the runner's kept methods carry the play-tested reasoning in their headers ([D11](#d11--the-runners-play-tested-semantics-are-moved-not-re-derived)) and stripping comments to hit a number would delete the reason each rule exists.

---

## 2. Goals

### Primary

- **G1 — A plan is data.** `Configs/Objective/overthrowObjectives.conf` lists two `OVT_ObjectiveConfig`s. Adding a third doctrine built from shipped modules requires **no EnforceScript change** — the modder exercise in [§7](#7-testing-strategy) is the acceptance test for this goal and the only one that can be.
- **G2 — Behavioural parity.** With the shipped registry, the machine makes the same decisions on the same inputs as the code at HEAD. Same selection, same operation order (`tower || harassment || sabotage`), same ladder rungs, same gates on both sides, same starvation rule (**measured at the forward base**, C4), same daylight window, same blacklist, same idle-clock behaviour, same refusals in the log.
- **G3 — The director loses its doctrine and keeps its runner.** After Phase 6, `grep -rn "SendHarassmentOperation\|SendTowerRecaptureOperation\|SendSabotageOperation\|SendFOBOperation\|SendFOBGarrisonOperation\|FireCounterAttack\|CheckHarassmentGate\|CheckBaseHarassmentGate\|EvaluateCounterAttackGate\|TickHarassment\|TickFOB\|AnchorRadiusForPhase" Scripts/` is **empty**. `DirectorTick()`'s three early returns are unchanged.
- **G4 — Modules never add fields to the director.** All module state is bag keys under a declared prefix. `OVT_ObjectiveInstance` gains no per-module field; `OVT_ObjectiveDirectorComponent` gains no counter.
- **G5 — The pool still balances.** The director never holds money. Every spend leaves `OVT_DeploymentManagerComponent.m_mFactionResources` exactly once through `SubtractFactionResources`, at the one create-then-debit choke point (`CreateObjectiveDeployment`, `OVT_ObjectiveDirectorComponent.c:1060`). `grep -rn "AddFactionResources" Scripts/Game/GameMode/Objectives/` stays **empty**, comments included.
- **G6 — The save is version-first and self-healing.** The new record round-trips. An unrecognised version, an unknown plan name or an unknown phase name is **logged loudly, discarded, and re-selected** — never a crash, never a half-restored objective.
- **G7 — Validation fails loud and skips.** `ValidateObjectiveRegistry()` runs once at world start with a real call site (C6). A plan with an unknown deployment config, an unknown resolver, a duplicate phase name, a phase with neither an advance condition nor a terminal operation, or a cadence of `-1` with no difficulty mapping is **named in an ERROR line and skipped**. The rest of the registry runs.
- **G8 — The frozen neighbours stay frozen.** `git diff Scripts/Game/GameMode/Virtualization/ Scripts/Game/GameMode/VirtualMovement/ docs/features/virtualization/core/api.md` empty for the whole feature. The thirteen non-objective deployment configs and `Deployment_TowerRecaptureUnrest.conf` byte-identical. Deployments remain the only mechanism that buys and tracks AI.

### Secondary

- **G9 — The objectives folder contains only what the registry drives.** `OVT_BaseRepairBehaviorDeploymentModule` and its config leave for `Deployments/`; nothing under `Scripts/Game/GameMode/Objectives/` is unreachable from a plan.
- **G10 — One asset API.** `IsAssetUp(key)` / `GetAssetPosition(key)` replace `IsFOBUp()` / `GetFOBPosition()` everywhere, with no wrappers, so the checkpoint asset that follows this feature adds a key rather than a method pair.
- **G11 — Difficulty is unchanged and now overridable.** All twelve `objective*` fields keep their names, preset values and `desc:`. Module attributes default to `-1` = "use the difficulty setting", mapped per module type in code, no string lookups. The inverted sabotage gate survives. `git diff Configs/Difficulty/` is **empty**.
- **G12 — The GM panel tells the truth in the plan's own words.** Plan name and authored **phase name** replace the enum→string table; `FormatObjectivePhase(int)` is retired with its four keys.
- **G13 — Nothing new replicates.** Server-only, bar the one existing GM snapshot record, whose objective field changes type from `int` to `string`.

### 2.1 Quality Bar — the hard floor

This is a **backend / AI-systems + authored-data** feature. There is no UI polish axis. The bar is parity, accounting, deterministic seam-driven tests, save round-trip, authorability, and validation that fails loud.

| Bar | What it means concretely | How it is caught |
|---|---|---|
| **Behavioural parity with the play-tested machine** | On the same inputs the runner takes the same decision the monolith took: the same objective selected, the same operation sent in the same order at the same cadence, the same gate answer on both sides of every threshold, the same abort for the same reason. Parity is asserted **per phase, against a reference that is still present**, never at the end against a monolith that no longer exists. | The rewritten Init cases (44 existing objective cases) driven against the runner; every doctrine phase's acceptance criteria include "the shim and the module produce the same answer on the phase's own fixture set"; play-test steps 2–8 |
| **Accounting integrity — the pool is conserved** | The director never holds money and never credits. Every spend leaves the one pool once, at one method. Refunds stay in the framework (`RecallDeployment`), never in `Objectives/`. The forward base's `spent` stays a counter. | `grep -rn "AddFactionResources" Scripts/Game/GameMode/Objectives/` empty; the create-then-debit choke point survives verbatim; Init "spend leaves the pool exactly once"; F9 |
| **Deterministic, seam-driven tests** | Every new case is world-free (Logic) or driven through a public seam (Init). No `maxAttempts`. Nothing asserts on live AI reaching a place. Randomness enters only through `s_AIRandomGenerator` and no assertion depends on a roll. Every new case carries a recorded can-fail proof in a preamble comment. | [§7](#7-testing-strategy); Q3 |
| **The save round-trips, and a broken save is survivable** | Plan name, target, phase **name**, both tick counters and the full bag come back exactly. An unknown version / plan / phase logs an ERROR naming the missing thing and leaves the machine idle and re-selecting, on a live campaign as well as a cold load. | The Persistence round-trip case; the Init "phase missing on load" case; F10/F11 |
| **Authorability of the `.conf` surface** | Every module attribute has a `desc:` a tuner can act on and names its difficulty field when it has one. A third plan can be assembled in Workbench from shipped modules alone, with no script. Module order in a phase is documented as evaluation order. | The **modder exercise** (§7, play-test step 9) — the only test of this bar that exists; the validator's error text naming the exact attribute |
| **Startup validation fails loud** | A broken plan produces one ERROR line naming the plan, the phase and the fault, and is skipped. It never wedges the machine and never silently does nothing. | G7's Init cases (one per validator rule); the call site in `PostGameStart` |
| **The frozen neighbours stay frozen** | Virtualization, VirtualMovement, `api.md`, the thirteen non-objective deployment configs, `Deployment_TowerRecaptureUnrest.conf`, and every `Configs/Difficulty/*.conf`. | Acceptance criterion on all seven code phases; the greps in [§6](#6-definition-of-done) |

---

## 3. Architecture Overview

### 3.1 The object model

```
SERVER ONLY. Nothing new replicates. Core is NOT touched. Every group is still a deployment.

Configs/Objective/overthrowObjectives.conf                      ← THE AUTHORED SURFACE (D6)
  OVT_ObjectiveRegistry                    (configRoot, on the game-mode prefab beside the
    m_sRegistryName                         deployment registry, same inherit-and-delta authoring)
    m_aObjectiveConfigs[]  ────────────┐
                                        │
  OVT_ObjectiveConfig  ◄────────────────┘     A PLAN
    m_sObjectiveName        stable — the persistence key and the GM panel's plan row
    m_iAllowedFactionTypes  OVT_FactionTypeFlag, as OVT_DeploymentConfig authors it
    m_fPriority             MULTIPLIER on the selector's score. Higher wins.  (D8)
    m_fChance               roll before a plan may be committed
    m_iMaxInstances         default 1
    m_Selector              ref OVT_ObjectiveTargetSelector          ← one per plan (D2 of reqs)
    m_aPhases[]             ordered; index 0 is entered on commit

  OVT_ObjectivePhase                          A PHASE
    m_sPhaseName            stable — persisted, read by deployment-side conditions, GM panel
    m_iOperationCadence     in-game minutes between operations. -1 = the difficulty field
    m_fAnchorRadius         evaluator anchor radius while in this phase. -1 = the ported default
    m_aModules[]            authored order IS evaluation order

  OVT_BaseObjectiveModule                     ONE BASE, THREE ROLES  (§3.3)
    ├─ OVT_BaseObjectiveConditionModule    AND'd → advance to the next phase
    ├─ OVT_BaseObjectiveOperationModule    ticked on cadence, in order; first to act consumes it
    └─ OVT_BaseObjectiveAbortModule        OR'd → reset the objective (blacklist as authored)

Scripts/Game/GameMode/Objectives/
  OVT_ObjectiveDirectorComponent            ← THE RUNNER, and nothing else  (§3.2)
    m_Registry            ref OVT_ObjectiveRegistry     (attribute, configRoot)
    m_aInstances          array<ref OVT_ObjectiveInstance>   cap m_iMaxConcurrentObjectives = 1 (D5)
    m_aBlacklist          array<ref OVT_ObjectiveBlacklistEntry>       — unchanged
    m_bReselectPending    — unchanged
    (the refusal-log dedup, the reserve-floor and anchor push/drop, the teardown ledger — unchanged)

  OVT_ObjectiveInstance : Managed            ← ONE OBJECTIVE IN FLIGHT  (D5)
    m_Config              OVT_ObjectiveConfig   (weak — the registry owns the ref)
    m_eTargetKind         OVT_EObjectiveKind    TOWN / BASE / … (extensible; no longer a wire format)
    m_vTargetPosition     THE KEY, as today
    m_sTargetName         a label; deliberately NOT persisted
    m_iPhaseIndex / m_sPhaseName
    m_iPhaseTicks / m_iNextOpTicks            tick counters, D4 of counter-attacks — unchanged
    m_mBag   : map<string,int>                 ← ALL module state  (§3.5)
    m_mBagV  : map<string,vector>              ← ALL module positions
    m_aRuntimeModules  : array<ref OVT_BaseObjectiveModule>   cloned on phase entry, dropped on exit

PURE STATICS — kept, extended, never world-aware  (Logic tier)
  OVT_ObjectiveSelection    scoring + pick + blacklist + anchor bias   (368 L, survives)
  OVT_ObjectivePhaseRules   gates, ladder, starvation, ceiling, tick-down, PhaseInRange (422 L)
  OVT_FOBSiting             siting maths                               (457 L, moves with the module)
  + OVT_ObjectivePlanRules  NEW — plan scoring/priority resolution, condition/abort evaluation
                                  over a bag, cadence and difficulty-fallback resolution

DEPLOYMENT FRAMEWORK — the seam only, and it still points one way
  OVT_DeploymentManagerComponent      SetObjectiveAnchor / ClearObjectiveAnchor        UNCHANGED
                                      SetObjectiveReserve / ClearObjectiveReserve      UNCHANGED
                                      (:2505, :2536, :2546 — the reserve floor)
  Modules/  OVT_BaseRepairBehaviorDeploymentModule       ← MOVES IN from Objectives/ (D3)
  Objectives/Modules/  the five that stay, re-pointed at the instance handle (R3)

GM  OVT_GMCampaignState.m_iObjectivePhase (int, :97-102)  →  m_sObjectivePhaseName (string)
    OVT_GMRequestComponent.SendCampaignObjective (:614)   →  signature change + WIRE_VERSION bump
    OVT_GMPanelFormat.FormatObjectivePhase(int) (:78)     →  RETIRED with its four keys

NOT BUILT, DELIBERATELY (the requirements' Out of Scope, restated as a build constraint)
├─ a third shipped plan (the modder exercise authors one; it is not committed as content)
├─ the checkpoint asset (only the module SHAPE it will use is guaranteed)
├─ a behaviour tree, runtime registry swapping, an in-game plan editor
├─ any change to QRF combat, siege mode, insertion driving, FOB siting maths, or the evaluator
└─ tuning for N>1 objectives — designed for N, shipped and tested at 1
```

### 3.2 What the runner still does

`DirectorTick()` (`OVT_ObjectiveDirectorComponent.c:725`) keeps its shape and its three early returns **verbatim**. Only the body of the phase switch changes:

```
OnDirectorTick():
    if (!Replication.IsServer())                          return      KEPT
    if (player count == 0)                                return      KEPT
    if (OVT_Global.GetOccupyingFaction().m_CurrentQRF)    return      KEPT — THE FREEZE
    ResolveRestoredObjective()                                        KEPT  (:4886)
    if (ConsumeReselectRequest())  SelectObjective()                  KEPT  (:795, :3771)

    foreach instance in m_aInstances:                                 NEW — the only structural change
        RunAbortModules(instance)      any true → ResetObjective(reason, blacklist)
        RunConditionModules(instance)  all true → EnterNextPhase(instance)
        TickOperationCadence(instance) → first operation module that acts consumes the cadence
        TickObjectiveIdleClock(instance, created)                     KEPT  (:3537)
        PushObjectiveReserve / DropObjectiveReserve                   KEPT  (:4238, :4275)

    if (m_aInstances.Count() < m_iMaxConcurrentObjectives) SelectObjective()
```

Everything in this list survives **unedited except for taking an instance argument**, and each keeps its existing header comment because the reasoning in it is still true:

| Kept | Line at HEAD | Why it is not doctrine |
|---|---|---|
| `GetInstance` / `OnPostInit` / `Init` / `PostGameStart` / `InstallTick` / `OnDelete` | `:579, :613, :634, :647, :662, :684` | Lifecycle. ⚠ `OnPostInit` runs in the **World Editor** where managers do not exist — allocate only. |
| `DirectorTick` and its early returns | `:725` | The freeze contract with `m_CurrentQRF` |
| `ConsumeReselectRequest`, `HookControlChanges`, `OnBaseControlChanged`, `OnTownControlChanged` | `:795, :4682, :4734, :4746` | Reselect-is-a-flag (D3 of counter-attacks): the invoker fires *before* the affiliation is applied |
| `CreateObjectiveDeployment`, `CanSendObjectiveDeployment` | `:1060, :1113` | **The one spend choke point.** Create-then-debit; sets `m_bBlockedOnAffordability` on the pool test and nothing else |
| `IsSameRefusal`, `LogOperationRefusal`, `ForgetOperationRefusals`, `GetLoggedRefusalCount`, `HasLoggedRefusal` | `:1194–:1282` | The refusal-log dedup (R8) |
| `AdvanceObjectiveTimers`, `AdvancePhaseTimeout`, `AdvanceOperationCadence`, `TickObjectiveIdleClock`, `ConsumeReportedOperations`, `RearmObjectiveIdleClock`, `SyncProgressMarks`, `HasOperationInFlight`, `IsObjectiveOperationConfig`, `LogAffordabilityBlock` | `:3461–:3741` | **The idle-clock semantics, exactly as play-tested.** The success signal is *pulled by the tick, never pushed by the counter*; an operation in flight holds the clock; affordability holds it; a spent ceiling does not |
| `PushObjectiveAnchor`, `DropObjectiveAnchor`, `PushObjectiveReserve`, `DropObjectiveReserve` | `:4134, :4173, :4238, :4275` | Reserve floor + evaluator anchor. The dependency still points one way |
| `ResetObjective`, `TearDownObjectiveDeployments`, `TrackObjectiveDeployment`, `ClearObjectiveRecord`, `ClearObjectiveRecordFields`, `EnterIdle` | `:4337, :4391, :4443, :4980, :5003, :4032` | The one teardown funnel both runtime paths share |
| `BlacklistPosition`, `ReadBlacklist`, `ServeBlacklistRound` | `:4594, :4628, :4642` | The blacklist |
| `SetOperationCountdown`, `SetPhaseTimeout`, `CommitObjective`, `EnterPhase` | `:4553, :4576, :3993, :4094` | The **write API the tests drive**. ⚠ A public mutator may never change phase — only the tick moves the machine |
| `ResolveRestoredObjective`, `ApplyPersistedObjective` | `:4886, :4777` | Restore-pending resolve; signature changes, the rule does not |

`AnchorRadiusForPhase` (`:4303`) is the one exception: its body is doctrine (a hard-coded radius per phase) and becomes `OVT_ObjectivePhase.m_fAnchorRadius`, with `-1` meaning "the value it hard-codes today". The *method* dies in Phase 6.

### 3.3 The module contract

Mirrors `OVT_BaseDeploymentModule` (`Scripts/Game/GameMode/Deployments/Modules/OVT_BaseDeploymentModule.c:1-132`) — public wrapper, protected virtual hook, hand-written `CloneModule()` per concrete class (C7):

```
[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sModuleName")]
class OVT_BaseObjectiveModule
{
    [Attribute(desc: "Name of this module")] string m_sModuleName;
    protected OVT_ObjectiveInstance m_Objective;      // weak; set by Initialize()

    void Initialize(OVT_ObjectiveInstance objective)  { m_Objective = objective; OnEnter(); }
    void Tick()                                       { OnTick(); }
    void Exit()                                       { OnExit(); }

    protected void OnEnter() {}
    protected void OnTick()  {}
    protected void OnExit()  {}

    // R1's Serialize/Deserialize: DECLARED, base is empty, NO SHIPPED MODULE OVERRIDES THEM. (D6)
    void Serialize(notnull OVT_ObjectiveInstance objective) {}
    void Deserialize(notnull OVT_ObjectiveInstance objective) {}

    OVT_BaseObjectiveModule CloneModule();             // overridden per concrete class
    string GetBagPrefix();                             // declared key prefix, e.g. "fob"
}

OVT_BaseObjectiveConditionModule : OVT_BaseObjectiveModule
    bool Evaluate()                       // AND'd across the phase; all true → advance
OVT_BaseObjectiveOperationModule : OVT_BaseObjectiveModule
    bool TryAct()                         // authored order; the FIRST that returns true consumes
                                          // the cadence. Ports `tower || harassment || sabotage`
    bool IsTerminal()                     // a phase whose last operation ended the objective
OVT_BaseObjectiveAbortModule : OVT_BaseObjectiveModule
    bool ShouldAbort(out string reason, out bool blacklist)   // OR'd; first true wins
```

**Shipped concrete modules** (the whole build surface; nothing else is needed for the two plans):

| Role | Class | Ports |
|---|---|---|
| Condition | `OVT_SupportBelowObjectiveCondition` | `TownPhase2Gate` / `TownPhase3Gate` support half **plus the world-fact conjunct** — `ObjectiveTownCarriesHarassmentDebuff()` (`:1473`). ⚠ Both halves or the town gate fires on its own entry tick |
| Condition | `OVT_ProgressAtLeastObjectiveCondition` | `BasePhase2Gate` / the sabotage half of `BasePhase3Gate`, over a bag key |
| Condition | `OVT_ReserveAtLeastObjectiveCondition` | `MeetsResourceGate(reserve, gate)` (`OVT_ObjectivePhaseRules.c:339`) |
| Condition | `OVT_AssetUpObjectiveCondition` | the `fobUp` conjunct |
| Condition | `OVT_DaylightWindowObjectiveCondition` | `IsCounterAttackWindow(hour, start, end)` (`:315`) + `LogDaylightWait` once |
| Condition | `OVT_PhaseTicksAtLeastObjectiveCondition` | (new; no shipped plan uses it — R1 asks for it and the modder exercise wants it) |
| Condition | `OVT_TargetKindIsObjectiveCondition` | the `kind == TOWN` / `== BASE` branches that today fork inside the send methods |
| Operation | `OVT_SendDeploymentObjectiveOperation` | `SendHarassmentOperation` + `SendTowerRecaptureOperation` + `SendSabotageOperation` + `SendFOBGarrisonOperation`, unified: `{ m_sConfigName, m_Resolver, m_iMaxConcurrent, m_aLadder[], m_fDedupRadius }` |
| Operation | `OVT_RaiseForwardBaseObjectiveOperation` | the whole `:1937–:3442` block (D3 of the requirements) |
| Operation | `OVT_StartBattleObjectiveOperation` | `FireCounterAttack` + `StartCounterAttackOnBase/Town` + `TickCounterQRF`; **terminal** |
| Abort | `OVT_IdleForObjectiveAbort` | the phase-timeout/idle-clock give-up, with the blacklist |
| Abort | `OVT_AssetStarvedObjectiveAbort` | `TickFOBStarvation` / `IsFOBStarved` (`:2848`, `:361`) |
| Abort | `OVT_TargetLostObjectiveAbort` | ownership flipped under us |
| Selector | `OVT_ResistanceTownObjectiveSelector` | `CollectTownCandidates` (`:3830`) + `ScoreTown`, weights lifted to attributes |
| Selector | `OVT_ResistanceBaseObjectiveSelector` | `CollectBaseCandidates` (`:3865`) + `ScoreBase`, same |
| Shim (temporary) | `OVT_LegacyPhaseObjectiveOperation` / `OVT_LegacyPhaseObjectiveCondition` | [§3.7](#37-the-strangler-seam) — **deleted in Phase 6** |

### 3.4 Target resolvers — the provider family

Shaped exactly on `OVT_DeploymentSourceProvider` (`Scripts/Game/GameMode/Deployments/Modules/OVT_DeploymentSourceProvider.c:28-37`), which a config names polymorphically under a base-typed `ref` attribute (`OVT_InsertionSpawningDeploymentModule.c:85-86` is the precedent; `Deployment_ObjectiveSabotage.conf:18` is the authored form).

```
[BaseContainerProps()]
class OVT_ObjectiveTargetResolver
    //! Answers ZERO, ONE or MANY positions, in preference order. Never null, never a zero vector
    //! standing in for "none" — an empty array is the refusal.
    bool Resolve(notnull OVT_ObjectiveInstance objective, int factionIndex,
                 notnull array<vector> outPositions);
    string GetResolverName();
```

Four shipped:

| Resolver | Answers | Replaces |
|---|---|---|
| `OVT_ObjectiveSelfTargetResolver` | the objective position, one entry | the implicit `m_Objective.position` in the harassment and sabotage senders |
| `OVT_EnemyTowersAffectingTargetResolver` | every resistance-held tower from `GetRadioTowersAffecting(objective)`, in list order | the whole loop at `:944-973`, including the `tower.faction == occupyingIndex` skip |
| `OVT_ForwardBaseTargetResolver` | `GetAssetPosition("fob")` when up, else empty | the FOB-garrison send's position |
| `OVT_NearestControlledBaseTargetResolver` | the faction's nearest held base to the objective | `ResolveNearestControlledBaseTo` (`:2273`) |

`OVT_SendDeploymentObjectiveOperation.TryAct()` walks the resolved positions in order, skips any that already carries a live instance of its config within `m_fDedupRadius` (`GetDeploymentNearPosition`, exactly as `:967` does today), and creates at the first free one. **This is what makes tower recapture, harassment, sabotage and the FOB garrison the same module.**

⚠ **The multi-position answer is load-bearing and is not "return one and iterate outside".** The tower operation's dedup-then-next-candidate walk is the shipped behaviour and must live inside one module or a phase with two towers sends nothing.

### 3.5 The state bag

```
OVT_ObjectiveInstance
    map<string,int>    m_mBag      Report(key, delta) / Get(key) / Set(key, value)
    map<string,vector> m_mBagV     SetPos(key, v) / GetPos(key)
```

**Two maps, and only two.** The shipped keys, with their owners:

| Key | Owner | Was |
|---|---|---|
| `harassment.successes` | `OVT_TownHarassmentBehaviorDeploymentModule` → instance | `m_Objective.harassmentSuccesses` |
| `sabotage.successes` | `OVT_BaseSabotageBehaviorDeploymentModule` → instance | `m_Objective.sabotageSuccesses` |
| `fob.up` (0/1) | `OVT_RaiseForwardBaseObjectiveOperation` | `m_FOB.up` |
| `fob.spent` | same | `m_FOB.spent` — **still a counter, never a wallet** |
| `fob.starvationTicks` | same | `m_FOB.starvationTicks` (the one counter that counts up) |
| `fob.deploymentName` | same | `m_FOB.deploymentName` — **a string, so it is the one exception**: see below |
| `fob.position` | same | `m_FOB.position` (vector map) |
| `fob.source` | same | `m_FOB.sourceBasePosition` (vector map) |

⚠ **One string does not fit two maps.** `fob.deploymentName` is the re-link key. Rather than add a third map for one value, the **asset registry** on the instance carries it: `map<string, ref OVT_ObjectiveAssetRecord>` keyed by asset key (`"fob"`), holding `up`, `position`, `source`, `spent`, `starvationTicks` and `deploymentName`. This is what `IsAssetUp(key)` / `GetAssetPosition(key)` read (G10), it is what the checkpoint asset will add a second entry to, and it means the two generic maps carry only numbers and positions. **The asset record is not a bypass of "modules never add fields":** it is a single generic container the module writes through, exactly like the bag.

**Position hand-off between phases** is bag-vector keys and nothing more — confirmed sufficient: the only cross-phase positions in the shipped machine are `fob.position` and `fob.source`, and the FOB **facing yaw** never crosses a phase boundary (it is consumed inside `ResolveFOBSite` → the raise, and is absent from today's save payload). See [D7](#d7--a-phase-hands-a-position-through-the-vector-bag-and-one-key-is-enough).

### 3.6 The deployment-framework seam

Exactly what R3 asks and nothing more. The dependency still points one way: `grep -rn "OVT_ObjectiveDirector\|OVT_ObjectiveInstance\|OVT_ObjectiveConfig" Scripts/Game/GameMode/Deployments/` stays **empty**.

- The five deployment-side modules that stay under `Objectives/Modules/` (`OVT_BaseSabotageBehaviorDeploymentModule`, `OVT_TownHarassmentBehaviorDeploymentModule`, `OVT_FOBRaiseSpawningDeploymentModule`, `OVT_ObjectiveConditionDeploymentModule`, `OVT_ObjectiveAnchorSourceProvider`) stop calling named director methods and call `objective.Report("sabotage.successes", +1)` / read `IsAssetUp("fob")` through a resolved instance handle.
- `OVT_ObjectiveConditionDeploymentModule.m_iRequiredPhase` / `m_iThroughPhase` (`:59`, `:75`) become **`m_sFromPhase` / `m_sThroughPhase`** strings. ⚠ Carry both existing protections over verbatim: `EffectiveLastPhase` (`:139`) — an **empty** `m_sThroughPhase` means "this phase only", never "everything" — and the dropped-line consequences documented at `:144-155`. The range is what fixed the phase-3 deadlock; **do not regress it to an equality test**.
- `m_bDirectorOnly` keeps its meaning and its two read sites (`OVT_DeploymentConfig.c:123`, `:242`) and its three indirect gates (`OVT_DeploymentManager.c:900, :1311, :1746`). `ForceCreateDeployment` never consults it — that is the deliberate-caller door.
- `SetObjectiveAnchor` / `ClearObjectiveAnchor` and `SetObjectiveReserve` / `ClearObjectiveReserve` (`OVT_DeploymentManager.c:2505, :2536, :2546`) are unchanged; the runner keeps pushing them.

### 3.7 The strangler seam

**The single most important structural decision in this plan.** ([D1](#d1--strangler-with-legacy-shim-modules-not-a-big-bang-rewrite))

Phase 2 authors both shipped plans **in full** — real names, real phase names, real cadences — but each phase's module bag contains one pair:

```
OVT_LegacyPhaseObjectiveOperation  { m_iLegacyPhase }     TryAct()      → the director's TickX() work
OVT_LegacyPhaseObjectiveCondition  { m_iLegacyPhase }     Evaluate()    → the director's CheckXGate()
```

Both switch on one authored integer (1 = HARASSMENT, 2 = FOB, 3 = COUNTER_QRF) and call the existing protected methods, promoted to package visibility for the duration. Behaviour at the end of Phase 2 is the same code reached through one more call.

Each doctrine phase then:
1. builds the real modules for **one** legacy phase,
2. re-authors that phase's `m_aModules[]` in the two plans,
3. **deletes** the director methods that shim wrapped, in the same phase,
4. re-points the Init cases that drove those methods.

The two shim classes are deleted in Phase 6 when the last one is unused. `grep -rn "OVT_LegacyPhase" Scripts/ Configs/` empty is a Definition-of-Done criterion.

**Why this and not a big-bang rewrite:** parity is a hard requirement (R2 of the goals) and parity drift is invisible. Catching it per-phase against a reference that is still in the file — the shim and the module can be driven on the same fixture and compared — is a mechanical check. Catching it at the end, against a monolith that has been deleted, is a reading exercise.

### 3.8 Persistence

One serializer, version **2**, replacing the version-1 record wholesale. `OVT_ObjectiveDirectorSerializer` keeps its GUID and its config entry (`Configs/Systems/Persistence/Overthrow.conf:42`) and its two structural rules: **nothing here touches the resource pool or any deployment** (the deployment manager's restore clears and refills the pool *after* game-mode component serializers), and **`Deserialize` is a pure codec** making exactly one side-effecting call.

```
 1  int    version = 2                     ⚠ VERSION FIRST, and the format is positional
 2  int    instanceCount
 for each instance:
 3  string  configName                     the plan. Unknown on load → ABANDON + log      (G6)
 4  int     targetKind
 5  vector  targetPosition                 THE KEY — unchanged
 6  string  phaseName                      Unknown on load → ABANDON + log                (G6)
 7  int     phaseTicks
 8  int     nextOpTicks
 9  array<string> bagKeys    / 10 array<int>    bagValues
11  array<string> bagVecKeys / 12 array<vector> bagVecValues
13  array<string> assetKeys  / 14 …the asset record fields, one parallel array each
 then:
15  array<vector> blacklistPositions   / 16 array<int> blacklistRounds
```

**Version handling, and the one distinction that matters** ([D2](#d2--no-migration-the-old-record-is-detected-discarded-and-logged)):

| Read | Meaning | Action |
|---|---|---|
| `version == 0` (absent payload) | No record was written — a save from before this serializer, or a read that failed | **Keep live state, silently.** This is the existing contract at `OVT_ObjectiveDirectorSerializer.c:157-159` and it stays |
| `version == 1` or any other unrecognised value | A counter-attacks-format record, or a future format | **`Print(ERROR)` naming the version, discard, clear the instance list, flag reselect.** No field reading, no mapping |
| `version == 2`, unknown `configName` or `phaseName` | The plan or phase was renamed or removed | **`Print(ERROR)` naming the missing plan/phase, abandon that instance cleanly, continue with the rest** |

The name is still **deliberately not persisted** — a label, not an identifier — and `ResolveRestoredObjective()` (`:4886`) still re-resolves it on the first tick. A live battle still rolls back. The forward base is still restored through `WasRestoredFromSave()`.

### 3.9 Validation (R8)

`OVT_ObjectiveRegistry.ValidateAllConfigs()` in the `OVT_DeploymentRegistry.c:77-91` shape — **but with a call site**, from the director's `PostGameStart()`, once, server-only. Every rule names the plan, the phase and the attribute; a plan that fails **any** rule is added to `m_aSkippedConfigs` and never selected.

| Rule | Added in |
|---|---|
| Empty `m_sObjectiveName`; duplicate plan name; duplicate phase name within a plan; empty `m_aPhases` | Phase 2 |
| No selector, or a selector whose `GetResolverName()` is unrecognised | Phase 3 |
| `SendDeployment` naming a config `OVT_DeploymentRegistry.FindConfigByName` cannot resolve; a ladder entry that cannot resolve; an unresolvable resolver | Phase 4 |
| A phase with **no** advance condition and **no** terminal operation (the wedge) | Phase 4 (rule), Phase 6 (terminal flag exists) |
| Cadence or anchor radius `-1` on a phase whose module set has no difficulty mapping | Phase 4 |

### 3.10 Difficulty (R5, D4 of the requirements)

All twelve fields (`OVT_DifficultySettings.c:81-104`) keep their names, preset values and `desc:`. `git diff Configs/Difficulty/` is empty for the whole feature.

New objective-module attributes default to **`-1` = "use the difficulty setting"**, resolved per module type in code — `OVT_ObjectivePlanRules.ResolveWithDifficulty(authored, difficultyValue)`, a pure static, no string lookup of a field name. Each attribute's `desc:` names its field.

**The mapping work is smaller than it looks — the director only reads seven of the twelve.** Verified 2026-08-21:

| Field | Read in the director at | Becomes an attribute on |
|---|---|---|
| `objectiveHarassmentIntervalMinutes` | `:896, :1945, :2011` | `OVT_ObjectivePhase.m_iOperationCadence` |
| `objectiveHarassmentMaxConcurrent` | `:919, :1025` (**shared by harassment and sabotage**) | `OVT_SendDeploymentObjectiveOperation.m_iMaxConcurrent` |
| `objectiveSabotageMissionsRequired` | `:1689` | `OVT_ProgressAtLeastObjectiveCondition.m_iRequired` (**the inverted one** — Init case unchanged) |
| `objectiveQRFResourceGate` | `:1691, :1701` | `OVT_ReserveAtLeastObjectiveCondition.m_iGate` |
| `objectiveFOBGarrisonMax` | `:2187` | `OVT_RaiseForwardBaseObjectiveOperation.m_iGarrisonMax` |
| `objectiveFOBCost` | `:3344, :3345, :3399` | `…m_iBudgetCost` |
| `objectiveStarvationMinutes` | `:2877, :2880, :2883` | `OVT_AssetStarvedObjectiveAbort.m_iStarvationMinutes` |
| `objectiveHarassmentHoldSeconds`, `objectiveSabotageHoldSeconds`, `objectiveSabotageStructuresPerMission`, `objectiveTowerRecaptureHoldSeconds` | **not read in the director** — read inside the deployment-side modules | already attributes there; the convention flips (below) |
| `objectiveMaxConcurrentInsertions` | **not read in the director** — the deployment manager's insertion cap | unchanged |

⚠ **One non-`objective*` coupling.** `IsPlayerAtFOB()` reads `difficulty.baseCloseRange` at `:2988`. It moves with the starvation code and keeps reading that field — it is what the rest of the campaign already means by "at a base", and inventing an objective-specific radius would be a new difficulty knob R5 forbids.

**The four director attributes become config attributes**, since they are per-plan or per-phase tuning rather than runner state: `m_fMaxUsefulDistance` (`:47`) → the selector; `m_iPhaseTimeoutTicks` (`:56`) → `OVT_ObjectivePhase` (per-phase, which is a small **capability gain**: today every phase shares one budget); `m_iBlacklistRounds` (`:59`) → the registry; `m_fObjectiveAnchorWeight` (`:62`) → the plan. ⚠ Author each with today's value as the default, or the ramp's pacing changes silently.

**~380 of the 49 doctrine constants move too**, and Phases 4–6 each lift their own: the anchor radii (600 m harassment / 1 200 m forward, `:105, :114`) → `m_fAnchorRadius`; the four-entry harassment ladder (`:135-140`) → `m_aLadder[]`; the three config-name constants (`:143, :152, :188, :191`) → `m_sConfigName`; the op radii and dedup radii (`:157, :161, :166, :177`) → `m_fDedupRadius`; the daylight window (`:243, :251`) → the condition's attributes. The **seventeen FOB siting constants** (`:283-:398`) stay as constants inside the raise module — they are the tuned output of a play-test, not a tuning surface, and R-scope forbids changing the siting maths.

⚠ **The five deployment-side modules use the opposite convention today** ("FALLBACK ONLY — the campaign's *X* is used whenever difficulty settings are loaded", e.g. `OVT_BaseSabotageBehaviorDeploymentModule.c:82,85`), meaning an authored value is *ignored* when difficulty is loaded. Flipping them to `-1` semantics is behaviour-neutral **only if their configs are re-authored to `-1` in the same commit**. That is planned for Phase 4 for the three objective-side modules; **`OVT_BaseRepairBehaviorDeploymentModule` is explicitly excluded** — it is leaving for the deployments framework as a pure relocation and its behaviour must not change (C3). The residual inconsistency is accepted and recorded.

---

## 4. Implementation Phases

Eight phases. **Every phase leaves the tree compiling, the suites runnable and the campaign playable**, and every phase from 2 onward leaves the objective machine *running*. No phase ships a module without the config that authors it; no phase ships a config whose modules do not exist.

**Test-run policy:** `tools/compile-check.sh` runs freely. `tools/run-tests.sh` launches a real Reforger client that steals desktop focus — it is run **by the orchestrator only, once, after a phase completes**. Never during planning, never in a subagent. See `.claude/test-policy.md`. Fast `{6A6E29FF47ECB840}`, All `{6A6E2A002F53A581}`.

**GUID prefix:** `{6BA1....}` — verified free on 2026-08-21 (`grep -rl "6BA10000" Configs/ Prefabs/ Scripts/` → 0). Re-grep before authoring.

---

### Phase 1 — Relocation and the generic asset API

**Agent:** `component-developer` — standard. Two mechanical, wide, behaviour-free changes; no shared system changes shape.
**Estimate:** 5–8 h
**Suite after this phase:** **All** (it edits the persistence round-trip suite and the GM-panel Logic case).
**Deletes from the director:** `IsFOBUp()` (`:5135`), `GetFOBPosition()` (`:5138`). **Re-points:** every Init case listed in T1.4.

**Tasks**

1. **T1.1 — Read-only survey.** Re-verify the consumer list before editing. Expected call sites of `IsFOBUp`/`GetFOBPosition`: `OVT_QRFControllerComponent.c:634,636`; `OVT_ObjectiveAnchorSourceProvider.c:78,81,119,122,157,160`; `OVT_ObjectiveDirectorSerializer.c:120,123`; plus tests. Expected consumers of the director's read API more broadly: `OVT_CampaignRequestComponent.c:238`, `OVT_GMRequestComponent.c:609-614`, `OVT_DismantleEnemyFOBAction.c:116`, `OVT_OverthrowGameMode.c:64,1535`, `OVT_Global.c:324-326`, `OVT_FOBPositionComponent.c` and `OVT_GMPanelFormat.c` / `OVT_GMPanelUIComponent.c` (comment/format references). If a concurrent session added a caller, stop and re-check.
2. **T1.2 — The asset record and the generic API.** Add `OVT_ObjectiveAssetRecord` and `map<string, ref OVT_ObjectiveAssetRecord>` to the director (it moves to the instance in Phase 2). Ship `bool IsAssetUp(string key)` and `vector GetAssetPosition(string key)`, both server-authoritative and both answering false/zero on a client exactly as the old pair did. **Delete `IsFOBUp()` and `GetFOBPosition()`.** Keep the header note that the forward-base state does **not** replicate, so no client-side code may ask it where the base is.
3. **T1.3 — Move every consumer** to `IsAssetUp("fob")` / `GetAssetPosition("fob")`, including the `ASSET_FOB` key constant so the string literal appears once. `OVT_DismantleEnemyFOBAction`'s two-entry-point rule (`CanDismantleFOBAt(callerPos, ownerEntityPos, …)` on the client, the record overload on the server) is unchanged.
4. **T1.4 — Move the test consumers**: `OVT_TEST_Init_CampaignRequestSeam.c:161,174`, `OVT_TEST_Init_ObjectiveDirector.c:91`, `OVT_TEST_Init_ObjectiveFOB.c:449,515,531,678,793,807`, `OVT_TEST_PersistenceRoundTripSuite.c:9346,9411,9414,9544,9819,9822,9857`, and `OVT_TEST_Logic_GMPanelFormat.c` if it names either method in prose. **These are renames only — no assertion changes meaning.**
5. **T1.5 — `git mv` the repair module** to `Scripts/Game/GameMode/Deployments/Modules/OVT_BaseRepairBehaviorDeploymentModule.c`. **No content change.**
6. **T1.6 — Rename the repair config file**, not its name. `git mv Configs/Deployment/Deployment_ObjectiveRepair.conf Configs/Deployment/Deployment_BaseRepair.conf` **and its `.conf.meta`, preserving the meta GUID byte-identically**, then edit the path half of the registry reference at `Configs/Deployment/overthrowDeployments.conf:47`. ⚠ **`m_sDeploymentName "Base Repair Detail"` is NOT touched** — it is the persistence key (C8) and renaming it would orphan every persisted repair instance. Record in `context.md` that the file name is a hint and the GUID is the identity.
7. **T1.7 — Re-point `OVT_TEST_Init_ObjectiveRepair.c`** (4 cases, 535 L) at the new paths. Its assertions (`m_bRequireControl 1`, `m_bDirectorOnly 0`, clone fidelity, difficulty precedence) are unchanged and are the proof the move was behaviour-free. Consider renaming the file to `OVT_TEST_Init_BaseRepair.c` in the same commit.
8. **T1.8 — `context.md`:** the T1.1 verdicts; the rename rule (file safe, `m_sDeploymentName` fatal); and the explicit note that **this phase's correctness is not covered by the Init tier** — see the acceptance criteria.

**Acceptance criteria**

- `tools/compile-check.sh` exits **0**; All green.
- `grep -rn "IsFOBUp\|GetFOBPosition" Scripts/ docs/features/occupying/objectives/` → **empty**.
- `git diff -M --stat Configs/Deployment/` shows exactly **two renames at 100 % similarity** (the `.conf` and its `.meta`) plus **one changed line** in `overthrowDeployments.conf`.
- `grep -n "m_sDeploymentName" Configs/Deployment/Deployment_BaseRepair.conf` → `"Base Repair Detail"`, unchanged.
- `ls Scripts/Game/GameMode/Objectives/Modules/` → **six** files (the repair module is gone).
- `git diff Configs/Difficulty/ Scripts/Game/GameMode/Virtualization/ Scripts/Game/GameMode/VirtualMovement/` → **empty**.
- 🔴 **The rename is the one change the Init tier cannot catch behaviourally.** A test that reads `IsAssetUp("fob")` where it used to read `IsFOBUp()` passes whether or not the key is wired to the same record. Three checks stand in for it, and all three are required before the phase is called done:
  1. **Compile + grep** — no old name survives anywhere, including comments.
  2. **Workbench** — open `Configs/Deployment/overthrowDeployments.conf` and confirm the repair entry still resolves to a config with its four modules (`compile-check.sh` cannot see `.conf` faults).
  3. **Play-test** — run the forward base to standing and confirm (a) the QRF wave-source list still includes it (`OVT_QRFControllerComponent.c:633-636`), (b) the dismantle action still appears and still refuses with enemies near, and (c) the FOB survives a save/Continue.

---

### Phase 2 — The objective framework and the strangler seam **(GATE for phases 3–7)**

**Agent:** `component-developer-advanced` — **advanced.** A new authored-data framework on the game-mode prefab, a save-format replacement, an instance model the whole feature plugs into, and a shim seam whose whole job is to be behaviour-identical.
**Estimate:** 20–26 h
**Suite after this phase:** **All**.
**Deletes from the director:** nothing yet — this is the phase that makes deletion safe. **Re-points:** `OVT_TEST_Init_ObjectiveDirector.c` (8 cases) onto the instance write API; both Persistence cases onto the v2 record.

**Tasks**

1. **T2.1 — `OVT_ObjectivePlanRules`**, the new pure static, first, so the tier has something to assert. `ResolveWithDifficulty(authored, difficultyValue)`; `ResolvePlanScore(selectorScore, priorityMultiplier)`; `SelectBestPlanIndex(array<float> scores, array<bool> eligible)`; `AllConditionsMet(array<bool>)`; `AnyAbort(array<bool>)`; `PhaseIndexOf(array<string> names, string name)` returning `-1`. ⚠ **No manager, world, entity or `OVT_Global` identifier, comments included** — the Logic-tier rule is a directory-wide grep. ⚠ `out` and `owned` are reserved local names.
2. **T2.2 — The data classes.** `OVT_ObjectiveRegistry : ScriptAndConfig` with `[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sRegistryName")]` and `FindConfigByName`, in the `OVT_DeploymentRegistry.c:1-28` shape. `OVT_ObjectiveConfig`, `OVT_ObjectivePhase`. Every attribute carries a `desc:` a tuner can act on.
3. **T2.3 — `OVT_ObjectiveInstance : Managed`** ([D5](#d5--the-instance-is-a-managed-object-inside-the-director)) with the bag, the vector bag, the asset map and the runtime module array. `Report(key, delta)`, `Get`, `Set`, `SetPos`, `GetPos`, `GetAsset(key)`. ⚠ Strong `ref` on every Managed member; the array is `array<ref …>`.
4. **T2.4 — The four module base classes** per [§3.3](#33-the-module-contract), each mirroring `OVT_BaseDeploymentModule` exactly. `CloneModule()` overridden per concrete class; `CopyTo` is **not** the pattern (C7). The base header states the dropped-line hazard verbatim and names the Init case that catches it.
5. **T2.5 — `OVT_ObjectiveTargetResolver`** base + `GetResolverName()`, in the `OVT_DeploymentSourceProvider.c:28-37` shape (contract: empty array is the refusal, nothing cached across calls). No concrete resolvers yet — Phase 4 ships them.
6. **T2.6 — The two shim classes** per [§3.7](#37-the-strangler-seam). Their headers state, in capitals, that they are temporary, which phase deletes each, and that **no new code may call them**.
7. **T2.7 — The runner rework.** `m_aInstances` + `m_iMaxConcurrentObjectives` (default 1, D5 of the requirements). `DirectorTick()` keeps its three early returns and its restore-resolve verbatim; the phase `switch` becomes the instance loop of [§3.2](#32-what-the-runner-still-does). Every kept method takes an instance argument and keeps its header. `EnterPhase(name)` clones the phase's modules, calls `Exit()` on the outgoing set, `Initialize()` on the incoming, re-arms the phase timeout through `SetPhaseTimeout()` (which also re-baselines the progress marks — that second half is load-bearing) and pushes the anchor at the phase's radius.
8. **T2.8 — The registry on the prefab.** `[Attribute] ref OVT_ObjectiveRegistry m_Registry` on the director; author it on `Prefabs/GameMode/OVT_OverthrowGameMode.et:51` pointing at `Configs/Objective/overthrowObjectives.conf`, in exactly the form the deployment registry uses at `:8-11`.
9. **T2.9 — The two plans, authored in full, with shim phases.** `Configs/Objective/Objective_TownOffensive.conf`, `Objective_BaseOffensive.conf`, `overthrowObjectives.conf`. Real `m_sObjectiveName`s; phase names `Harassment`, `ForwardBase`, `CounterAttack`; real cadences (`-1`); real anchor radii (`-1`); module bag = the shim pair. ⚠ **`.conf` files cannot carry comments** and **module order is evaluation order** — the authored order is the contract.
10. **T2.10 — The validator** with its Phase-2 rules ([§3.9](#39-validation-r8)) **and its call site** in `PostGameStart()` (C6). A skipped plan is named once at ERROR and never selected.
11. **T2.11 — Serializer v2** per [§3.8](#38-persistence). Version-first, positional, append-only. The three version outcomes, each with its own log line. ⚠ Rewrite the "never renumber the enum" header on `OVT_ObjectiveRecords.c:1-13`: with names in the payload the constraint is **dead**, and leaving the warning would mislead the next reader into preserving a format nothing reads.
12. **T2.12 — Logic-tier**, new `TestSuites/Logic/OVT_TEST_Logic_ObjectivePlanRules.c`: `ResolveWithDifficulty` for authored `-1`, `0`, a sane value and an absurd value; `SelectBestPlanIndex` on highest score, ties by input order, all-ineligible, empty; `AllConditionsMet` on the empty array (**true** — a phase with no conditions is terminal-or-timeout, not blocked) and `AnyAbort` on the empty array (**false**); `PhaseIndexOf` returning `-1` for an unknown name and for an empty array. Floats compare with `OVT_TEST_LogicFixture.EPSILON`.
13. **T2.13 — Init-tier:** the registry resolves off the director and off the prefab; `ValidateAllConfigs()` passes on the shipped registry; a hand-built plan with a duplicate phase name / empty phase list / empty name is **skipped and named**; both shim classes clone completely (the dropped-line pattern); the runner enters phase index 0 on commit and advances on the shim condition; **the tick still early-returns while a QRF is live and decrements nothing**. ⚠ Init worlds never run `PostGameStart` — a case needing a tick installs it itself.
14. **T2.14 — Persistence-tier:** rewrite `OVT_TEST_PersistenceRoundTrip_ObjectiveDirector_SurvivesSaveAndReapply` (`:9107`) against the v2 record — plan name, target, phase **name**, both tick counters, a two-key int bag, a one-key vector bag, the `fob` asset record and a two-entry blacklist. Assert **deltas, never absolutes**; write through the public API and read back through public getters — the tier's rule forbids naming the serializer or the payload anywhere under `Scripts/Game/Tests/`. 🔴 **Do not widen the reload seam** (`Instances = {gameMode}` only).
15. **T2.15 — `context.md`:** the shim contract and its deletion schedule; the bag key table; the "public mutator may never change phase" rule restated at the new call sites; the load-order rule the serializer inherits.

**Acceptance criteria**

- compile **0**; All green.
- **Parity gate:** with only shim phases authored, the machine's behaviour is the pre-phase behaviour. Concretely: every one of the 44 existing objective Init cases passes with **assertion bodies unchanged**, only their subject re-pointed from the director's fields to the instance's.
- `git diff Scripts/Game/GameMode/Deployments/` → **empty**.
- `git diff Configs/Deployment/ Configs/Difficulty/` → **empty**.
- `grep -rn "OVT_ObjectiveDirector\|OVT_ObjectiveInstance\|OVT_ObjectiveConfig" Scripts/Game/GameMode/Deployments/` → **empty**.
- `grep -rn "OVT_Global\|GetGame()\|World\|Entity" Scripts/Game/GameMode/Objectives/OVT_ObjectivePlanRules.c` → **empty, comments included**.
- The director is declared once on the game-mode prefab; `git diff Prefabs/` is that one attribute addition.
- The validator's call site exists: `grep -rn "ValidateAllConfigs\|ValidateObjectiveRegistry" Scripts/Game/GameMode/Objectives/` returns a declaration **and a call**.

---

### Phase 3 — Plan-driven selection

**Agent:** `component-developer-advanced` — **advanced.** It replaces the one decision the whole feature exists to make, and its parity claim ("two plans reproduce one list") is the least mechanically obvious in the plan.
**Estimate:** 10–14 h
**Suite after this phase:** **All**.
**Deletes from the director:** `CollectTownCandidates` (`:3830`), `CollectBaseCandidates` (`:3865`), `DistanceToNearestHeldBase` (`:3892`), `HasOccupyingTowerCoverage` (`:3906`), `ResolveBaseName` (`:3926`), `ResolveTownName` (`:3953`), `ResolveTownNameAt` (`:3976`) — all move into the two selectors. **Re-points:** the selection cases in `OVT_TEST_Init_ObjectiveDirector.c`.

**Tasks**

1. **T3.1 — `OVT_ObjectiveTargetSelector`** base: `[BaseContainerProps()]`, `int GetCandidateSources()` (a flag set: `RESISTANCE_TOWNS`, `RESISTANCE_BASES`, …), `bool ScoreCandidates(notnull OVT_ObjectiveCandidateSet set, notnull array<float> outScores)`, `GetSelectorName()`.
2. **T3.2 — `OVT_ObjectiveCandidateSet`**, collected **once per selection round by the runner** and shared across every plan ([D6](#d6--selection-runs-on-the-reselect-flag-or-a-free-slot-with-one-shared-candidate-collection)). Parallel arrays of kind / position / name / population / support / threat / distance-to-nearest-held-base / tower-coverage — the six inputs the shipped scorer already takes, plus the two the base scorer takes. **The world queries happen here and nowhere else**, so N plans cost one pass.
3. **T3.3 — The two shipped selectors**, carrying `OVT_ObjectiveSelection`'s weights as attributes with today's constants as defaults: `TOWN_POPULATION_WEIGHT 40`, `POPULATION_REFERENCE 400`, `TOWN_SUPPORT_COLLAPSE_WEIGHT 30`, `BASE_PRIZE_WEIGHT 45`, `BASE_THREAT_WEIGHT 25`, `THREAT_REFERENCE 40`, `PROXIMITY_WEIGHT 25`, `TOWER_COVERAGE_WEIGHT 10` (`OVT_ObjectiveSelection.c:35-65`). ⚠ **The statics keep taking numbers, never a `OVT_TownData`** — the hard rule at `OVT_ObjectiveSelection.c:4-6`. Villages stay excluded; FOBs and radio towers stay non-candidates.
4. **T3.4 — Plan-driven `SelectObjective()`.** For each plan whose faction / instance cap / `m_fChance` allow: score its selector's best candidate; apply the blacklist; multiply by `m_fPriority`; commit the top. **No jitter** — predictability is a design constraint. `LogSelection` (`:4055`) keeps naming the winner, the runner-up and both scores, and gains the plan name.
5. **T3.5 — Selection cadence** per [D6](#d6--selection-runs-on-the-reselect-flag-or-a-free-slot-with-one-shared-candidate-collection): run on the reselect flag, or when a slot is free and `m_iSelectionCooldownTicks` (registry attribute, **default 1** = today's every-idle-tick behaviour) has expired. Behind the existing debug flag, log candidate count × plan count and the elapsed ms.
6. **T3.6 — The validator's Phase-3 rules**: a plan with no selector, or a selector whose candidate sources are empty.
7. **T3.7 — Logic-tier** (extending `OVT_TEST_Logic_ObjectiveScaling.c`, 12 cases): plan resolution with two plans at equal priority picks the higher score; a priority multiplier of 2 lets a lower raw score win and 0 excludes; an all-blacklisted candidate set selects nothing; ties by input order.
8. **T3.8 — Init-tier — the parity case that matters.** 🔴 **"The two shipped plans reproduce the single-list pick."** Build a deterministic fixture set of towns and bases, drive the *old* selection path (still reachable at this point, because the shim phases are still authored) and the *new* plan-driven path, and assert they choose the **same position**. This is the only place the fork from one list into two plans can be proved, and it is only possible while both paths exist. Plus: the selector returns nothing on an empty set; blacklisted candidates are excluded; the cooldown default reproduces every-tick selection.
9. **T3.9 — `context.md`:** the equal-priority parity argument written out, and the candidate-source flag table.

**Acceptance criteria**

- compile **0**; All green.
- The T3.8 parity case is green **and carries its can-fail proof** (flip one selector weight; it must go red).
- `grep -rn "CollectTownCandidates\|CollectBaseCandidates\|DistanceToNearestHeldBase\|HasOccupyingTowerCoverage" Scripts/` → **empty**.
- `git diff Scripts/Game/GameMode/Deployments/ Configs/Deployment/ Configs/Difficulty/` → **empty**.
- The shipped weights are unchanged: `grep -n "WEIGHT\|REFERENCE" Scripts/Game/GameMode/Objectives/OVT_ObjectiveSelection.c` shows the same eight constants with the same values.

---

### Phase 4 — The harassment phase in config

**Agent:** `component-developer-advanced` — **advanced.** The `SendDeployment` module unifies four senders with different resolvers, different caps and a ladder, and it is the module the modder exercise will lean on hardest.
**Estimate:** 16–22 h
**Suite after this phase:** **All**.
**Deletes from the director:** `TickHarassment` (`:843`), `SendNextOperation` (`:887`), `SendHarassmentOperation` (`:904`), `SendTowerRecaptureOperation` (`:944`), `SendSabotageOperation` (`:993`), `CountLiveHarassmentOperations` (`:1307`), `CountLiveSabotageOperations` (`:1340`), `CheckHarassmentGate` (`:1410`), `CheckBaseHarassmentGate` (`:1453`), `ObjectiveTownCarriesHarassmentDebuff` (`:1473`), `ResolveObjectiveSupportPercentage` (`:1503`), `OnHarassmentSuccess` (`:4479`), `OnSabotageSuccess` (`:4503`), and the **`m_iLegacyPhase 1`** shim authoring. **Re-points:** `OVT_TEST_Init_ObjectiveOperations.c` (7 cases), `OVT_TEST_Init_ObjectiveSabotage.c` (6 cases), `OVT_TEST_Init_ObjectiveInsertion.c` (4 cases, subject only).

**Tasks**

1. **T4.1 — The four resolvers** of [§3.4](#34-target-resolvers--the-provider-family), each with the dedup-then-next-candidate contract in its header. `OVT_EnemyTowersAffectingTargetResolver` ports the loop at `:944-973` **including** the occupying-faction skip.
2. **T4.2 — `OVT_SendDeploymentObjectiveOperation`**: `m_sConfigName`, `ref OVT_ObjectiveTargetResolver m_Resolver`, `m_iMaxConcurrent` (**-1** → `objectiveHarassmentMaxConcurrent`), `m_aLadder[]` (config names by rung), `m_sLadderProgressKey`, `m_fDedupRadius`. `TryAct()` resolves positions → dedups → picks the ladder rung via `HarassmentLadderIndex(successes, rungs)` (`OVT_ObjectivePhaseRules.c:117`) → calls `CreateObjectiveDeployment` (**unchanged**). ⚠ The ladder is **four** registry entries derived from one `.conf` (`overthrowDeployments.conf:49-83`); the module names them, it does not build them.
3. **T4.3 — The three conditions** `SupportBelow`, `ProgressAtLeast`, `TargetKindIs`, and the `IdleFor` abort. 🔴 **`SupportBelow` carries the world-fact conjunct.** `CheckHarassmentGate()`'s header is explicit: a town can already be under its threshold when it is *chosen*, so the gate must also require the town to be carrying the `ObjectiveHarassment` modifier this ramp applies. Port `ObjectiveTownCarriesHarassmentDebuff()` **into the condition module**, as an attribute-gated conjunct (`m_sRequiredTownModifier`), not into `OVT_ObjectivePhaseRules.TownPhase2Gate` — that static's signature is pinned by Logic cases and the conjunct is not its question. Without it the town gate fires on its own entry tick.
4. **T4.4 — Re-point the deployment-side reporters.** `OVT_TownHarassmentBehaviorDeploymentModule` and `OVT_BaseSabotageBehaviorDeploymentModule` call `objective.Report("harassment.successes", +1)` / `("sabotage.successes", +1)` instead of the director's named methods. 🔴 **The success signal is still PULLED by the tick, never PUSHED.** `ConsumeReportedOperations()` (`:3591`) compares the bag against `m_iProgress*Mark`; a `Report` that re-armed a timer would reintroduce the two red cases Phase 5 of counter-attacks paid for.
5. **T4.5 — `OVT_ObjectiveConditionDeploymentModule` phase names.** `m_iRequiredPhase`/`m_iThroughPhase` → `m_sFromPhase`/`m_sThroughPhase`; `PhaseInRange` becomes an index comparison through `PhaseIndexOf`. ⚠ Carry `EffectiveLastPhase`'s empty-through protection (`:139`) and the dropped-line consequences (`:144-155`) verbatim. Re-author the affected `Deployment_Objective*.conf`s. ⚠ **This is a `.conf` schema change on a persisted-adjacent file** — the config *name* does not move, so nothing orphans.
6. **T4.6 — The difficulty convention flip** for the three objective-side modules per [§3.10](#310-difficulty-r5-d4-of-the-requirements): attributes read `-1` → difficulty, and the configs are re-authored to `-1` in the same commit so the shipped behaviour is unchanged. **`OVT_BaseRepairBehaviorDeploymentModule` is excluded.**
7. **T4.7 — Author the Harassment phase** in both plans: `[SendDeployment(tower, EnemyTowersAffecting)] [SendDeployment(harassment ladder, ObjectiveSelf)] [SendDeployment(sabotage, ObjectiveSelf) + TargetKindIs BASE]` then the conditions then the abort. ⚠ **The order `tower || harassment || sabotage` is the shipped chain and the authored order is what reproduces it.**
8. **T4.8 — Delete the Phase-1 shim authoring** and the director methods listed above.
9. **T4.9 — The validator's Phase-4 rules** ([§3.9](#39-validation-r8)): unresolvable deployment config, unresolvable ladder entry, unresolvable resolver, a phase with no advance condition and no terminal operation, `-1` with no mapping.
10. **T4.10 — Init-tier:** every new module's **clone carries every own and inherited attribute** (one dedicated case per class, the `OVT_TEST_Init_TowerUnrestRecapture.c:214-259` shape — hand-built template with distinct non-default values, per-field `SetFailure` naming the consequence); the tower resolver skips an occupying-held tower and returns the next; a second call at a deduped position returns the next candidate; the ladder rung saturates; the town gate **refuses without the modifier and passes with it**; a `SendDeployment` refusal is logged once, not every tick. ⚠ Deployment fixtures must be `SetSpawnedUnitsEliminated(true)` on the deployment **and every spawning module** before anything ticks.
11. **T4.11 — `context.md`:** the operation-order contract; the world-fact conjunct and why it is in the module; the difficulty-convention flip and its exclusion.

**Acceptance criteria**

- compile **0**; All green.
- `grep -rn "SendHarassmentOperation\|SendTowerRecaptureOperation\|SendSabotageOperation\|CheckHarassmentGate\|CheckBaseHarassmentGate\|TickHarassment\|OnHarassmentSuccess\|OnSabotageSuccess" Scripts/` → **empty**.
- `grep -rn "m_iRequiredPhase\|m_iThroughPhase" Scripts/ Configs/` → **empty**.
- `grep -rn "AddFactionResources" Scripts/Game/GameMode/Objectives/` → **empty**.
- `git diff Configs/Difficulty/` → **empty**; `git diff` on the thirteen non-objective deployment configs and `Deployment_TowerRecaptureUnrest.conf` → **empty**.
- One clone case exists per new module: `grep -c "CloneCarriesEveryAttribute\|CloneFidelity" Scripts/Game/Tests/TestSuites/Init/` rises by the number of modules shipped.

---

### Phase 5 — The forward base as an operation module

**Agent:** `component-developer-advanced` — **advanced. The largest phase.** ~1,500 lines of siting, budget, starvation, teardown, dismantle, restore and a server-validated player request move into one module, and the restore path has a documented duplication hazard.
**Estimate:** 20–26 h
**Suite after this phase:** **All**.
**Deletes from the director:** `TickFOB` (`:1595`), `SendNextFOBOperation` (`:1937`), `SendFOBOperation` (`:2034`), `SendFOBGarrisonOperation` (`:2173`), `CountLiveFOBGarrisons` (`:2198`), `FindLiveFOBDeployment` (`:2231`), `ResolveFOBSourceBase` (`:2248`), `ResolveNearestControlledBaseTo` (`:2273`), `ResolveFOBSite` (`:2340`), `CollectFOBExclusions` (`:2399`), `SampleGeneratedFOBSite` (`:2487`), `FindAuthoredFOBSite` (`:2580`), `EvaluateFOBCandidate` (`:2664`), `MeasureGroundSpread` (`:2703`), `IsSiteClearOfObstructions` (`:2744`), `MeasureRoadDistance` (`:2763`), `FilterFOBMarker` (`:2776`), `AddFOBMarker` (`:2787`), `OnFOBRaised` (`:2812`), `TickFOBStarvation` (`:2848`), `IsFOBSourceBaseHeld` (`:2896`), `CountAliveFOBGroups` (`:2927`), `IsPlayerAtFOB` (`:2982`), `TearDownFOB` (`:3016`), `ClearFOBRuntimeState` (`:3081`), `RemoveFOBStructure` (`:3114`), `ResolveFOBStructurePrefab` (`:3149`), `FilterFOBStructure` (`:3172`), `AddFOBStructure` (`:3183`), `IsFOBBudgetActive` (`:3369`), `WithinFOBBudget` (`:3390`), `CountFOBSpend` (`:3412`), `RecordFOB` (`:4516`), `AddFOBSpend` (`:4531`), `SetFOBStarvationTicks` (`:4542`), `ClearFOBRecord` (`:5047`), and the **`m_iLegacyPhase 2`** shim authoring. **Re-points:** `OVT_TEST_Init_ObjectiveFOB.c` (8 cases), `OVT_TEST_Init_ObjectiveAnchor.c` (4 cases), the FOB Persistence case (`:9566`).

⚠ **`CanDismantleFOB` (`:3203`), `CanDismantleFOBAt` (`:3234`), `CountOccupyingDefendersNear` (`:3269`) and `OnFOBDismantledByPlayer` (`:3331`) stay on the director as a thin facade** that forwards to the asset's module. `OVT_DismantleEnemyFOBAction.c:116` and `OVT_CampaignRequestComponent.c:238` are **not edited**: a user action and a server validator must not have to know which module owns an asset, and the "one body, two entry points" rule (client asks about the entity it is attached to, server asks the record) is unchanged.

**Tasks**

1. **T5.1 — Read-only survey.** Re-verify the FOB block's boundaries and every external caller before moving a line. Record the verdict.
2. **T5.2 — `OVT_RaiseForwardBaseObjectiveOperation`**, carrying the whole block. `OVT_FOBSiting` (457 L, pure) moves with it unchanged. Attributes: `m_sAssetKey` (default `"fob"`), `m_sDeploymentConfigName`, `m_sGarrisonConfigName`, `m_iGarrisonMax` (**-1** → `objectiveFOBGarrisonMax`), `m_iBudgetCost` (**-1** → `objectiveFOBCost`), band/spread/lane siting attributes with today's tuned values (250→400 m spread, 5 lanes) as defaults.
3. **T5.3 — `OVT_AssetUpObjectiveCondition`, `OVT_ReserveAtLeastObjectiveCondition`, `OVT_DaylightWindowObjectiveCondition`, `OVT_AssetStarvedObjectiveAbort`.** 🔴 **`DaylightWindow` holds the phase timeout only.** Starvation and the operation cadence keep running while waiting for dawn — freeze them and a forward base cut off at 22:00 survives until morning and attacks anyway. This corrected D17 once already; the correction is the shipped behaviour.
4. **T5.4 — Starvation ports verbatim, including C4.** `IsFOBStarved(sourceHeld, aliveGroups, playerPresent)` (`OVT_ObjectivePhaseRules.c:361`) with `playerPresent` supplied by presence **at the forward base**, `OVT_WorldUtils.PlayerInRange(assetPosition, difficulty.baseCloseRange)`. ⚠ **Do not "fix" this to the source base** on the strength of the requirements-era prose. Count the force through **handles** (`CollectRegisteredHandles` + `GetAliveMemberCount`), never `GetSpawnedEntities()` — a dormant force reads as dead.
5. **T5.5 — The budget ceiling stays a counter.** `fob.spent` in the asset record; `FOBBudgetCeiling` / `WithinFOBCeiling` (`:381`, `:399`) unchanged; the ceiling **arms when the forward base's own deployment is SENT** and disarms on reset. ⚠ A spent ceiling deliberately does **not** set `m_bBlockedOnAffordability` — that is a decision the machine made about itself and the phase should time out.
6. **T5.6 — Restore.** `WasRestoredFromSave()` gates the raise **and** the `alreadyAttempted` latch gates it separately — neither substitutes for the other (a reinforcement rebuy clears the eliminated flags and re-runs the convergence). The structure is found for teardown by **prefab resource name off the config**, never by a runtime `EntityID`. The re-link is a first-tick job, never deserialization.
7. **T5.7 — Author the ForwardBase phase** in both plans: `[RaiseForwardBase] [SendDeployment(FOB garrison, ForwardBaseResolver)]` **plus the Harassment phase's operations, repeated** — the phase-3 deadlock fix. In the config world that is expressed twice: the operations are authored in this phase's bag *and* the deployment-side `OVT_ObjectiveConditionDeploymentModule`s span `Harassment`→`ForwardBase` via `m_sFromPhase`/`m_sThroughPhase`. 🔴 **Both halves or the deadlock returns**: a base objective is promoted by its *first* sabotage mission and needs up to six, and a town's support only falls while harassment keeps landing.
8. **T5.8 — Delete the Phase-2 shim authoring** and every method in the deletes list.
9. **T5.9 — Init-tier:** the raise module clones completely; a **restored** deployment raises nothing and a fresh one raises exactly once; the `alreadyAttempted` latch blocks a second raise after a rebuy; teardown leaves no deployment of either config in the radius; the anchor source provider still prefers the forward base and falls through to the nearest base rather than failing; the starvation predicate is true on each of its three inputs independently; the daylight wait does **not** freeze starvation. ⚠ A fixture driving the tick past a give-up must stop **at** the grace window — the IDLE branch of the very next tick buys a real deployment with real resources.
10. **T5.10 — Persistence-tier:** rewrite `OVT_TEST_PersistenceRoundTrip_ObjectiveFOB_RelinksItsDeployment` (`:9566`) against the asset record; every asset field survives; a payload naming a deployment that no longer exists **resets the objective** rather than stranding it. Drive exactly `FOB_RELINK_ATTEMPTS` ticks and plant a countdown afterwards.
11. **T5.11 — `context.md`:** the C4 correction restated at the new call site; the two independent raise latches; the deadlock fix's two halves.

**Acceptance criteria**

- compile **0**; All green.
- `grep -rn "TickFOB\|SendFOBOperation\|SendFOBGarrisonOperation\|ResolveFOBSite\|TearDownFOB\|IsPlayerAtFOB\|WithinFOBBudget" Scripts/` → **empty**.
- `OVT_DismantleEnemyFOBAction.c` and `OVT_CampaignRequestComponent.c` show **no diff** for this phase.
- `git diff Scripts/Game/GameMode/Deployments/ Configs/Difficulty/` → **empty**.
- `wc -l Scripts/Game/GameMode/Objectives/OVT_ObjectiveDirectorComponent.c` → **below 3,000** (the forward-base block is 1,580 lines of the 5,201).
- **Parity check:** the shim for phase 2 and the module set produce the same gate answer and the same operation on the phase's Init fixture, driven before and after the swap in the same session.

---

### Phase 6 — The battle as a terminal operation, and the last shim dies

**Agent:** `component-developer-advanced` — **advanced.** It touches the battle layer's only entry point and the QRF polling contract, and it deletes the strangler seam.
**Estimate:** 10–14 h
**Suite after this phase:** **All**.
**Deletes from the director:** `EvaluateCounterAttackGate` (`:1656`), `MeetsCounterAttackRamp` (`:1685`), `IsCounterAttackDaylight` (`:1712`), `ResolveWorldHour` (`:1728`), `LogDaylightWait` (`:1757`), `LogCounterAttackRefusal` (`:1776`), `FireCounterAttack` (`:1799`), `StartCounterAttackOnBase` (`:1839`), `StartCounterAttackOnTown` (`:1879`), `TickCounterQRF` (`:3444`), `AnchorRadiusForPhase` (`:4303`), **and both shim classes**. **Re-points:** the remaining cases in `OVT_TEST_Init_ObjectiveDirector.c` (8) and `OVT_TEST_Init_ObjectiveReserve.c` (2).

**Tasks**

1. **T6.1 — `OVT_StartBattleObjectiveOperation`**: `m_eMode` (`SIEGE`), `IsTerminal()` true. It calls `StartBaseQRF` / `StartTownQRF` and returns; the resolution is **polled** by the runner watching `m_CurrentQRF` go null. 🔴 **Never add a second `m_OnFinished` subscriber** — the manager deletes the controller entity inside the invoker's own dispatch. The reason goes in the method header so nobody "improves" it into a subscription.
2. **T6.2 — Terminal completion.** The last phase completing ends the objective through **one** path (R1): win → re-select, loss → reset with blacklist, both reported the same way, so a plan can end on any module rather than only on a battle. `ResetObjective(reason, blacklist)` (`:4337`) is that path and is unchanged.
3. **T6.3 — `OVT_DaylightWindowObjectiveCondition` finishes.** `IsCounterAttackWindow(hour, startHour, endHour)` (`:315`) with 05:00/15:00 as attribute defaults; the world hour is read off `OVT_Component`'s **inherited** `m_Time`, re-resolved lazily — ⚠ a component that re-declares `m_Time` shadows the base's copy with one nothing fills in. Log the wait **once**, not every tick.
4. **T6.4 — `AnchorRadiusForPhase` → `m_fAnchorRadius`** on the phase, with `-1` meaning today's per-phase value; the runner pushes at phase entry as it always did.
5. **T6.5 — Delete both shim classes** and the `m_iLegacyPhase 3` authoring.
6. **T6.6 — Init-tier:** the gate fires the starter **exactly once**; it refuses at 22:00 and passes at 06:00 with every other input identical; the daylight refusal does not tick starvation and does not reselect; a terminal operation's completion resets the objective on the reset path and blacklists only on the loss branch; the reserve floor is pushed on a refused ask and dropped on a satisfied one.
7. **T6.7 — `context.md`:** the poll-not-subscribe rule restated; the shim deletion recorded.

**Acceptance criteria**

- compile **0**; All green.
- `grep -rn "OVT_LegacyPhase" Scripts/ Configs/` → **empty**.
- `grep -rn "FireCounterAttack\|EvaluateCounterAttackGate\|AnchorRadiusForPhase\|TickCounterQRF" Scripts/` → **empty**.
- `grep -rn "m_OnFinished" Scripts/Game/GameMode/Objectives/` → **empty**.
- `grep -rn "StartBaseQRF\|StartTownQRF" Scripts/` → the two player-initiated callers plus the `StartBattle` module, and nothing else.
- `git diff Scripts/Game/Controllers/OccupyingFaction/` → **empty** (the QRF layer is consumed as-is).

---

### Phase 7 — Validation, presentation and the admin surface

**Agent:** `component-developer` — standard, but hand the `.layout` slice, if any, to `ui-developer`. Contained; the only risk is the GM wire, which has a documented recipe.
**Estimate:** 8–12 h
**Suite after this phase:** **All**.
**Deletes from the director:** nothing. **Re-points:** `OVT_TEST_Logic_GMPanelFormat.c`, `OVT_TEST_Init_GMRequestSeam.c`.

**Tasks**

1. **T7.1 — The GM wire.** `OVT_GMCampaignState.m_iObjectivePhase` (int, `:97-102`) → `m_sObjectivePhaseName` (string), plus a plan-name field. ⚠ **A new/changed scalar on `OVT_GMCampaignState` needs THREE edits and the one with no symptom is `Clear()`** (`:267`) — missing from `CopyFrom` (`:208`) the row never fills; missing from `Clear` the row shows the *previous* campaign's value after a second campaign in the same client session. `CopyRecords` is for the four per-entity arrays only.
2. **T7.2 — `SendCampaignObjective`** (`OVT_GMRequestComponent.c:614`) changes signature; bump **`WIRE_VERSION`**. ⚠ `Rpc()` arity is a compile-check blind spot (BUG-090) — a wrong argument count compiles clean and dies at the wire. The Init seam case is the only mechanical defence. `CAMPAIGN_RECORD_COUNT` does **not** change (no record is added).
3. **T7.3 — Retire `FormatObjectivePhase(int)`** (`OVT_GMPanelFormat.c:78-90`) and its four `#OVT-GMPanel_ObjectivePhase*` keys. `FormatObjectiveName` (`:61`) is unchanged. The phase row now shows the authored `m_sPhaseName`; the "no objective" and "unknown" keys stay for the empty and unresolvable cases. ⚠ A `#`-prefixed key handed to `TextWidget.SetText` is resolved, so one formatter may answer either a key or a proper noun with no branch at the call site (`OVT_GMPanelUIComponent.c:410-417`).
4. **T7.4 — The admin verbs: verify, do not rewire.** ⚠ **`/give-resources` and `/tick-resources` are not in the director at all** — they live in `Scripts/Game/Components/Controller/OVT_AdminCommandsComponent.c` and touch `OVT_OccupyingFactionManager`. R8's "keep working against the instance" is therefore a *confirmation* task, not an edit: `git diff Scripts/Game/Components/Controller/OVT_AdminCommandsComponent.c` must be **empty**, and a play-test confirms both verbs still unblock a poverty-stalled objective. What does need checking is the **refusal-log dedup** (`LogOperationRefusal`, `:1220`), which now sees call sites inside modules rather than inside the director — assert it still de-duplicates across them.
5. **T7.4b — Delete the two zero-caller getters.** `GetLoggedRefusalCount` (`:1268`) and `HasLoggedRefusal` (`:1282`) have **no callers anywhere in the repo, tests included**. Either give them an Init case that asserts the dedup (preferred — the dedup is otherwise untested) or delete them. Do not leave them as they are.
5. **T7.5 — The validator's error text pass.** Every rule's message names the plan, the phase and the attribute, in that order, and says what a modder should do. This is the authorability bar's only mechanical support.
6. **T7.6 — Logic-tier:** the phase-name formatter for an authored name, an empty name and a `#`-key name; the plan-name row.
7. **T7.7 — Init-tier:** the GM seam accepts the changed record and the state receives every field; each validator rule fails the plan it should and names it; a plan failing validation is **never selected** while the others still run.

**Acceptance criteria**

- compile **0**; All green.
- `grep -rn "FormatObjectivePhase\|OVT-GMPanel_ObjectivePhaseHarassment\|OVT-GMPanel_ObjectivePhaseForwardBase\|OVT-GMPanel_ObjectivePhaseCounterAttack" Scripts/ Language/ UI/` → **empty**.
- `git diff Language/` shows **only** `localization_Overthrow.st`; every removed key is removed there and **nowhere else** — ⚠ `Language/*.conf` are Workbench build output and must never be hand-edited. Report the owed re-export.
- `git diff Scripts/Game/GameMode/GM/OVT_GMRecords.c` → **empty**; `OVT_GMCampaignState`'s other fields keep their order.

---

### Phase 8 — Help & documentation sync

**Agent:** `help-docs-sync`
**Estimate:** 3–5 h
**Suite after this phase:** **skipped** — docs and localization only. Say so.

Player-facing *behaviour* is unchanged by design, but two things players and Game Masters see do change: the GM panel's phase row now shows the plan's authored phase name, and the Field Manual's "Counter Attacks" page describes a ramp whose phases are now named in data.

**Tasks:** re-check every sentence in the tutorial popups (`Configs/Tutorials/`), the Field Manual (`Configs/FieldManual/`) and the public wiki that names a phase or quotes a number against the **shipped plans and the shipped difficulty presets** — the fact-checking rule: cite a `file:line` or cut the sentence. ⚠ **C5 is exactly the failure mode**: a doc quoting "45 minutes on Normal" when Normal authors 60. Update the wiki to describe the registry as the authoring surface (a modder-facing page is the natural new content). Record what is owed.

---

## 5. Key Technical Decisions

### D1 — Strangler with legacy shim modules, not a big-bang rewrite

**Context:** parity with a 5,201-line play-tested machine is a hard requirement, and parity drift is silent — nothing errors, nothing warns, the faction just decides slightly differently.
**Decision:** land the framework first with both plans authored in full but their module bags filled by a shim pair that calls the existing methods; replace one legacy phase per implementation phase; delete each method **in the phase that replaces it**, never in a later cleanup phase.
**Rationale:** it makes parity a *comparison* rather than a *reading*. While a shim and its replacement both exist, an Init case can drive the same fixture through both and assert the same answer (T3.8 is the sharpest example). It also keeps every intermediate state playable, which matters because this feature's real verification is a play-test.
**Trade-offs:** two extra classes and a little churn in the plan `.conf`s; a reader mid-feature sees an indirection with no purpose. Both are paid off at Phase 6, and the "no `OVT_LegacyPhase` survives" grep is a Definition-of-Done criterion.

### D2 — No migration: the old record is detected, discarded and logged

**Context:** R6 asked for migration from the counter-attacks record. The author dropped it on 2026-08-21 — v1.5 is unreleased.
**Decision:** version 2 only. `version == 0` (absent) keeps live state silently, as today. Any other unrecognised version logs an ERROR naming it, discards, and re-selects — the same clean-abandon path R6 already requires for a missing plan or phase.
**Rationale:** the migration would have been the *only* consumer of the frozen enum integers, so dropping it deletes a whole constraint (`OVT_ObjectiveRecords.c:1-13`) rather than carrying it forever for one code path. A discarded objective costs a player nothing they will notice: the machine picks a new one on the next tick.
**Trade-offs:** anyone carrying a dev save from the counter-attacks build loses their in-flight objective. Accepted, and it is one log line.

### D3 — The repair module moves out; rename the file, never the config name

**Context:** `OVT_BaseRepairBehaviorDeploymentModule` + `Deployment_ObjectiveRepair.conf` arrived with `core/damage` (`d3ce53ef`) under `Objectives/Modules/`, but the config is `m_bDirectorOnly 0`, carries no objective condition module, and the director never references it (C3).
**Decision:** `git mv` the module to `Deployments/Modules/`; `git mv` the config to `Deployment_BaseRepair.conf` with its `.meta` GUID **byte-identical**, and edit only the path half of the registry reference (`overthrowDeployments.conf:47`). **`m_sDeploymentName "Base Repair Detail"` is not touched.**
**Rationale:** the persistence key is `m_sDeploymentName` (C8), not the file name — the registry resolves the file by GUID and the path is a readable hint. So the file rename costs one line and orphans nothing, while a name rename would orphan every persisted repair instance *and* is unnecessary because the name never carried the `Objective` prefix in the first place. **Recommendation: rename the file, keep the name.** The alternative — leaving `Deployment_ObjectiveRepair.conf` where it is — was rejected because "the objectives folder contains only what the registry drives" is the point of the move, and a file called `Objective*` that no objective drives is exactly the confusion this feature exists to remove.
**Trade-offs:** the module keeps reading two `objective*` difficulty fields after the move. Accepted and noted — R5 freezes the field names and inventing `baseRepair*` twins would be a difficulty-surface change for cosmetics.

### D4 — The asset API rename happens in Phase 1, with no wrappers

**Context:** R2 proposed thin wrappers until consumers moved. The author closed that on 2026-08-21: move them now.
**Decision:** `IsAssetUp(key)` / `GetAssetPosition(key)` ship in Phase 1 and the old pair is deleted in the same commit.
**Rationale:** a wrapper that is never removed is the normal outcome, and the checkpoint asset (near-term, its own feature) would then inherit a method pair per asset. Doing it first, alone, in a phase that changes nothing else, also makes the diff readable.
**⚠ It is the one change in this feature the Init tier will not catch behaviourally** — a case that reads `IsAssetUp("fob")` where it read `IsFOBUp()` passes whether or not the key resolves to the same record. The substitutes are stated as Phase 1 acceptance criteria and all three are required: compile + a zero-hit grep for the old names *including comments*; a **Workbench** load of `overthrowDeployments.conf` confirming the repair entry still resolves (compile-check cannot see `.conf` faults); and a **play-test** proving the three live consumers — the QRF wave-source list, the dismantle action's appear/refuse behaviour, and the FOB surviving a Continue.

### D5 — The instance is a `Managed` object inside the director

**(Requirements open question (a).)**
**Decision:** `OVT_ObjectiveInstance : Managed`, held in `array<ref OVT_ObjectiveInstance>` on the director. **Not** its own component, **not** its own entity.
**Rationale:** the only two arguments for a component/entity are replication and independent persistence, and this feature has neither — nothing about the objective replicates (the GM panel is a server-built snapshot, not a replicated record), and the director already owns a game-mode component serializer that can write N instances as a count plus N records. A component would need a prefab, an `RplComponent`, persistence tracking, a load-order story against the deployment manager's pool refill, and a teardown story for a second campaign in one session — all for zero benefit at N=1. The precedent is right next door: deployments made the opposite call **because** each instance needed a durable, independently-persisted world marker, and objectives have no world presence at all.
**Migration path if N>1 with replication is ever wanted:** hand the instance a marker entity, exactly as deployments did. Nothing in the module contract assumes the instance is not entity-backed.
**Trade-offs:** the director's serializer grows a loop, and every module holds a **weak** back-reference to the instance (strong would cycle). The instance's own members are `ref`.

### D6 — Selection runs on the reselect flag or a free slot, with one shared candidate collection

**(Requirements open question (b).)**
**Decision:** keep today's trigger semantics exactly — the reselect flag, or a free objective slot — and add `m_iSelectionCooldownTicks` on the registry with **default 1**, which reproduces today's every-idle-tick behaviour byte-for-byte. Separately, and this is the part that actually matters: **collect the candidate set once per selection round** in the runner and hand each plan's selector its slice, rather than letting each selector walk the world.
**Rationale:** the cost that multiplies with N is the *world queries* (walking the base list, the town list, tower coverage per candidate), not the arithmetic — `ScoreTown`/`ScoreBase` are a handful of multiplies on numbers the caller already has. Collecting once makes N plans cost one pass instead of N, which removes the reason to change the cadence at all; the cooldown attribute is there so a server with ten plans can back off without a code change, and at its default it is a no-op. Selectors declare which candidate sources they want (a flag set), so a future "raid a discovered camp" plan adds a source rather than a second collection pass.
**Measurement rather than assumption:** behind the existing debug flag, log candidate count × plan count and elapsed ms per round (T3.5). Tune the default only with numbers from a play-test.
**Trade-offs:** the shared set is the union of every enabled plan's sources, so one exotic plan can make the collection wider for everyone. Acceptable — the flag set makes the cost legible, and a plan that fails validation contributes nothing.

### D7 — A phase hands a position through the vector bag, and one key is enough

**(Requirements open question (c).)**
**Decision:** `map<string,vector>` on the instance, keyed by the writing module's declared prefix. **Confirmed sufficient** for the shipped machine and for the four shipped resolvers.
**Rationale:** the only positions that cross a phase boundary today are `fob.position` and `fob.source`, both consumed by `OVT_ForwardBaseTargetResolver` and `OVT_ObjectiveAnchorSourceProvider`. The forward base's **facing yaw** — the one non-position scalar the siting produces — never crosses a boundary: it is consumed inside the raise and is absent from today's save payload, so no third map is needed for it. If a future module does need a cross-phase scalar, it goes in the **int** bag with a documented unit (`fob.facingDeg`), not in a new map: a third typed map is a save-format change and the int bag is already versioned.
**Multi-position answers are the resolver's job, not the bag's** — `OVT_EnemyTowersAffectingTargetResolver` computes its list on demand from the world, so no phase ever needs to hand forward a *set*.
**Trade-offs:** a module that wanted a float would truncate or scale. Accepted, and stated in the bag's header so nobody stores a metre-scale float as an int by accident.

### D8 — Plan priority is a float multiplier where higher wins, and it is named `m_fPriority`

**Context:** `OVT_DeploymentConfig.m_iPriority` is an **int** where **lower** wins (`GetConfigsByPriority` filters `<=` then sorts ascending). D2 of the requirements says an objective plan's rank is "score × plan priority", which is the opposite convention.
**Decision:** name the objective field **`m_fPriority`** — a float multiplier, default `1.0`, **higher wins** — and say why in its `desc:` and its header.
**Rationale:** silently inverting a convention under the same field name is the kind of thing that produces a plan that never runs and a bug report nobody can reproduce. A different type and a different prefix makes the difference visible in the `.conf` itself.
**Trade-offs:** two priority conventions in one tree. Mitigated by the name and by the validator, which rejects a negative multiplier.

### D9 — Modules do not serialize; the bag is the format

**Context:** R1 lists `Serialize`/`Deserialize` in the module contract, and deployment modules do not serialize at all (their state is reconstructed by re-cloning from the config).
**Decision:** declare `Serialize`/`Deserialize` on the module base as required, implement them as **empty**, and ship **zero** overrides. All persisted module state is bag keys, written by the runner's one serializer.
**Rationale:** one save format, one version number, one place to append. A per-module serializer would reproduce the `CloneModule` trap in the save layer — a module that forgets to write a field loses it silently on every load, and there is no equivalent of the dropped-line test for a format nobody can enumerate. The bag is enumerable by construction, so the round-trip case asserts *the whole thing* rather than a list somebody maintained.
**Trade-offs:** a module needing a structure the two maps cannot express has to add a typed array to the record — a deliberate, reviewed format bump, which is the right friction. This is a small deviation from R1's literal wording and is flagged for the author to overrule.

### D10 — The pure statics survive, with their weights lifted to attributes

`OVT_ObjectiveSelection` (368 L), `OVT_ObjectivePhaseRules` (422 L) and `OVT_FOBSiting` (457 L) are kept and extended, not rewritten. Their hard rule — *"every function is a function of its arguments and nothing else"* (`OVT_ObjectiveSelection.c:4-6`) — is what makes the whole progression assertable in the cheapest tier, and it is why the Logic tier can carry the parity burden that the Init tier carries for seams. The eight selection weights become selector attributes with today's constants as defaults; the statics keep taking numbers.

### D11 — The runner's play-tested semantics are moved, not re-derived

Everything in the [§3.2](#32-what-the-runner-still-does) table keeps its body and its header comment. In particular the idle clock's three rules survive verbatim: the success signal is **pulled by the tick**, an operation in flight **holds** the clock (scoped by `IsObjectiveOperationConfig`, so a *standing* forward base does not hold it open forever), and an affordability block holds it while a spent ceiling does not. Each of these was a play-test fix with a written reason; re-deriving them from the requirements would lose all three.

---

## 6. Definition of Done

An evaluator with **no implementation context** should be able to verify every item below.

### Functional Criteria

- **F1 — The registry drives the machine.** Open `Configs/Objective/overthrowObjectives.conf`. **Expect:** exactly two `OVT_ObjectiveConfig` entries, each with a name, a selector, a priority and an ordered `m_aPhases[]` of three named phases, each phase listing condition, operation and abort modules. No phase list is empty; no module name appears that is not a class in `Scripts/Game/GameMode/Objectives/Modules/`.
- **F2 — Selection is plan-driven and still predictable.** Start a fresh campaign on Normal and take one town. **Expect:** within a minute an objective is selected, and the log line names the winning **plan**, the winning candidate, the runner-up and both scores. The winner is a resistance-held town, city or base — never a village, an FOB or a radio tower.
- **F3 — The ramp is the ramp.** Watch an objective through to a battle. **Expect, in this order and no other:** harassment operations arriving (tower recapture attempted first when a resistance-held tower covers the objective, then harassment, then — at a base — sabotage); an unannounced forward base once the phase-1 gate passes; and a counter-attack once the forward base is up, the reserve gate is met and the clock is inside 05:00–15:00. Group size grows with each harassment success.
- **F4 — Phase 1 operations continue into the forward-base phase.** Make a base you hold the objective and let it promote on its first sabotage mission. **Expect:** further sabotage teams keep arriving during the forward-base phase, and the counter is able to reach the difficulty's required count. (A machine that stops sending at promotion is the shipped deadlock, reintroduced.)
- **F5 — The forward base still starves at the forward base.** Keep a strong presence **at the forward base itself** (not at its source base). **Expect:** the starvation clock runs and the base comes down. Then re-run and take the source base instead: it also starves. Both inputs work independently.
- **F6 — A daylight wait is a wait, not a failure.** Let the counter-attack gate ripen at night. **Expect:** the objective waits, harassment continues, **starvation keeps ticking**, the log says so once, and the battle starts after 05:00. No objective is abandoned for being night.
- **F7 — The machine never wedges.** Play an extended session. **Expect:** every objective progresses, is abandoned for a stated reason, or is blacklisted and replaced, and every transition carries its reason in the log. No objective sits in one phase indefinitely with nothing happening.
- **F8 — A battle still freezes everything.** Trigger a player-initiated QRF elsewhere during phase 1. **Expect:** operations stop being created and no objective timer advances; afterwards they resume from where they were.
- **F9 — Resource accounting is closed.** Watch the GM campaign panel across several operations and one 6-hour tick. **Expect:** the deployment pool falls by exactly the cost of each operation created and nothing moves unexplained. **The occupying faction never gains resources from anything the machine does.**
- **F10 — The objective survives a Continue.** Save mid-forward-base with a base standing and some progress banked, quit, **Continue**. **Expect:** the same plan, the same phase **by name**, the same counters, exactly **one** forward base structure, and the ramp resuming where it stopped.
- **F11 — A broken save abandons cleanly.** Rename a shipped plan in `overthrowObjectives.conf` and load a save that used it. **Expect:** one ERROR line naming the missing plan, no crash, the machine idle and selecting a new objective on its next tick. Repeat with a renamed **phase**: same outcome, naming the phase.
- **F12 — A broken plan is skipped, not run.** Author a plan with a duplicate phase name (or a `SendDeployment` naming a config that does not exist) and start a world. **Expect:** one ERROR line at world start naming the plan, the phase and the fault; that plan is never selected; **the other plan still runs**.
- **F13 — The GM panel speaks the plan's language.** Open the GM Overthrow panel during each phase. **Expect:** the objective row shows the place, and the phase row shows the **authored phase name** from the `.conf` — change `m_sPhaseName` in the config, restart, and the panel shows the new string with no code change.
- **F14 — A modder can add a doctrine with no script.** See the modder exercise in [§7](#7-testing-strategy). **Expect:** a third plan assembled in Workbench from shipped modules only, passing validation and visibly running.

### Quality Criteria

- **Q1** `tools/compile-check.sh` exits **0** with no output.
- **Q2** Fast `{6A6E29FF47ECB840}` and All `{6A6E2A002F53A581}` both exit **0** at the end of every phase.
- **Q3** Every new test case carries a recorded proof that it can fail — the exact edit, in a preamble comment. **No `maxAttempts` anywhere.**
- **Q4 The doctrine left the director:** `grep -rn "SendHarassmentOperation\|SendTowerRecaptureOperation\|SendSabotageOperation\|SendFOBOperation\|SendFOBGarrisonOperation\|FireCounterAttack\|CheckHarassmentGate\|CheckBaseHarassmentGate\|EvaluateCounterAttackGate\|TickHarassment\|TickFOB\|AnchorRadiusForPhase\|ResolveFOBSite\|TearDownFOB" Scripts/` → **empty**.
- **Q5 The strangler seam is gone:** `grep -rn "OVT_LegacyPhase" Scripts/ Configs/` → **empty**.
- **Q6 The director is a runner:** `wc -l Scripts/Game/GameMode/Objectives/OVT_ObjectiveDirectorComponent.c` → **under 2,400** (from 5,201), and the one `switch (m_Objective.phase)` at `:753` is gone. ⚠ The bar is 2,400, not lower — see the move budget in [§1](#1-executive-summary); the kept methods carry their play-tested rationale in their headers and must not be stripped to hit a number.
- **Q7 One funding path:** `grep -rn "AddFactionResources" Scripts/Game/GameMode/Objectives/` → **empty**, comments included. Every spend goes through `SubtractFactionResources` immediately after a create that returned non-null.
- **Q8 The dependency points one way:** `grep -rn "OVT_ObjectiveDirector\|OVT_ObjectiveInstance\|OVT_ObjectiveConfig\|OVT_ObjectiveRegistry" Scripts/Game/GameMode/Deployments/` → **empty**.
- **Q9 The old asset API is gone:** `grep -rn "IsFOBUp\|GetFOBPosition" Scripts/` → **empty**.
- **Q10 The old phase formatter is gone:** `grep -rn "FormatObjectivePhase" Scripts/ UI/` → **empty**, and its four localization keys are removed from `Language/localization_Overthrow.st` and nowhere else.
- **Q11 The frozen neighbours are frozen:** `git diff Scripts/Game/GameMode/Virtualization/ Scripts/Game/GameMode/VirtualMovement/ docs/features/virtualization/core/api.md Configs/Difficulty/` → **empty**.
- **Q12 The non-objective deployment configs are byte-identical:** `git diff Configs/Deployment/Deployment_TownPatrol.conf Configs/Deployment/Deployment_TowerGarrison.conf Configs/Deployment/Deployment_VehiclePatrol_*.conf Configs/Deployment/Deployment_Base*.conf Configs/Deployment/Deployment_TowerRecaptureUnrest.conf` → **empty**.
- **Q13 The repair move is a pure rename:** `git diff -M --stat Configs/Deployment/ Scripts/Game/GameMode/Objectives/Modules/ Scripts/Game/GameMode/Deployments/Modules/` shows the module and the config (with its `.meta`) as **100 % similarity renames**; `grep -n "m_sDeploymentName" Configs/Deployment/Deployment_BaseRepair.conf` → `"Base Repair Detail"`.
- **Q14 Every module's clone is covered:** every concrete `OVT_*ObjectiveModule`, resolver and selector class has a dedicated Init case asserting field-by-field clone fidelity with distinct non-default values.
- **Q15 Logic-tier grep clean:** no manager, game-mode, world, entity or `OVT_Global` identifier in any pure-static file or its Logic-tier test file, **comments included**.
- **Q16 The save format is version-first and append-only:** `OVT_ObjectiveDirectorSerializer` writes `version` first, reads it first, and has exactly three documented version outcomes. `git diff` on `OVT_OccupyingFactionManagerSerializer.c`, `OVT_DeploymentComponentSerializer.c` and `OVT_DeploymentManagerSerializer.c` → **empty**.
- **Q17 Validation has a call site:** `grep -rn "ValidateAllConfigs\|ValidateObjectiveRegistry" Scripts/Game/GameMode/Objectives/` returns a declaration **and at least one call**.

### Integration Criteria

- **I1 — Deployments is still the only thing that buys AI.** Every group the machine creates is created by a deployment module; `git diff Scripts/Game/GameMode/Virtualization/` is empty and `api.md` gains no signature.
- **I2 — The deployment framework did not learn about objectives.** Q8's grep. The seam is the instance handle inside `Objectives/Modules/` and the two manager APIs (`SetObjectiveAnchor`/`ClearObjectiveAnchor`, `SetObjectiveReserve`/`ClearObjectiveReserve`) that already existed.
- **I3 — `m_bDirectorOnly` keeps its meaning.** Its two read sites (`OVT_DeploymentConfig.c:123, :242`) and three indirect gates (`OVT_DeploymentManager.c:900, :1311, :1746`) are unedited; `ForceCreateDeployment` still never consults it.
- **I4 — QRF is consumed as-is.** `git diff Scripts/Game/Controllers/OccupyingFaction/` → **empty**. `grep -rn "m_OnFinished" Scripts/Game/GameMode/Objectives/` → **empty** (poll, never subscribe). `StartBaseQRF`/`StartTownQRF` have exactly three callers: base capture, uprising, and the `StartBattle` module.
- **I5 — Core is untouched.** `git diff Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c` shows no change beyond a comment, if any. `UpdateKnownTargets()` and `GetThreatByLocation()` are untouched.
- **I6 — The player's FOB systems are untouched.** `OVT_DismantleEnemyFOBAction.c` and `OVT_CampaignRequestComponent.c` show no diff after Phase 1; the resistance's own `OVT_FOBData` / deploy / undeploy path is not edited at all.
- **I7 — The GM wire is a type change, not a record change.** `git diff Scripts/Game/GameMode/GM/OVT_GMRecords.c` → **empty**; `CAMPAIGN_RECORD_COUNT` unchanged; `WIRE_VERSION` bumped once.

### Verification Method

**Automated — from the repo root, in order:**

1. `tools/compile-check.sh` → exit **0**.
2. `tools/run-tests.sh "{6A6E29FF47ECB840}"` → exit **0** (Fast).
3. `tools/run-tests.sh "{6A6E2A002F53A581}"` → exit **0** (All).
4. `wc -l Scripts/Game/GameMode/Objectives/OVT_ObjectiveDirectorComponent.c` → **< 2400**; `grep -n "switch (m_Objective.phase)" Scripts/Game/GameMode/Objectives/OVT_ObjectiveDirectorComponent.c` → **empty**. → Q6
5. `grep -rn "SendHarassmentOperation\|SendTowerRecaptureOperation\|SendSabotageOperation\|SendFOBOperation\|SendFOBGarrisonOperation\|FireCounterAttack\|CheckHarassmentGate\|CheckBaseHarassmentGate\|EvaluateCounterAttackGate\|TickHarassment\|TickFOB\|AnchorRadiusForPhase\|ResolveFOBSite\|TearDownFOB" Scripts/` → **empty**. → Q4
6. `grep -rn "OVT_LegacyPhase" Scripts/ Configs/` → **empty**. → Q5
7. `grep -rn "IsFOBUp\|GetFOBPosition" Scripts/` → **empty**. → Q9
8. `grep -rn "AddFactionResources" Scripts/Game/GameMode/Objectives/` → **empty**. → Q7
9. `grep -rn "OVT_ObjectiveDirector\|OVT_ObjectiveInstance\|OVT_ObjectiveConfig\|OVT_ObjectiveRegistry" Scripts/Game/GameMode/Deployments/` → **empty**. → Q8
10. `git diff Scripts/Game/GameMode/Virtualization/ Scripts/Game/GameMode/VirtualMovement/ docs/features/virtualization/core/api.md Configs/Difficulty/` → **empty**. → Q11, I4
    ⚠ **Corrected 2026-08-21 (Phase 1):** `Scripts/Game/Controllers/OccupyingFaction/` was listed here as byte-frozen, which **contradicts T1.3** — `OVT_QRFControllerComponent.c` lives there and is one of the eleven consumers the asset-API rename must move. It is frozen **behaviourally**: its only permitted diff for the whole feature is the two keyed-API call sites (`:634`, `:636`). Phase 6's own acceptance criterion is read the same way.
11. `git diff Configs/Deployment/Deployment_TownPatrol.conf Configs/Deployment/Deployment_TowerGarrison.conf Configs/Deployment/Deployment_VehiclePatrol_*.conf Configs/Deployment/Deployment_Base*.conf Configs/Deployment/Deployment_TowerRecaptureUnrest.conf` → **empty**. → Q12
12. `git diff -M --stat Configs/Deployment/` → the repair `.conf` and `.meta` as **100 % renames**, plus one changed line in `overthrowDeployments.conf`. → Q13
13. `grep -n "m_sDeploymentName" Configs/Deployment/Deployment_BaseRepair.conf` → `"Base Repair Detail"`. → Q13
14. `grep -c "OVT_ObjectiveConfig " Configs/Objective/overthrowObjectives.conf` → **2**. → F1
15. `grep -rn "FormatObjectivePhase" Scripts/ UI/` → **empty**; `git diff Language/` shows only `localization_Overthrow.st`. → Q10
16. `grep -rn "ValidateAllConfigs\|ValidateObjectiveRegistry" Scripts/Game/GameMode/Objectives/` → a declaration **and** a call. → Q17
17. `ls Scripts/Game/GameMode/Objectives/Modules/` → the repair module is **absent**; `ls Scripts/Game/GameMode/Deployments/Modules/` → it is **present**. → G9

**Workbench (`compile-check.sh` cannot see `.conf` or prefab faults):**

18. Open `Configs/Objective/overthrowObjectives.conf`. **Expect:** both plans expand, every phase shows its module list, and no attribute shows as unresolved.
19. Open `Prefabs/GameMode/OVT_OverthrowGameMode.et`. **Expect:** `OVT_ObjectiveDirectorComponent` carries `m_Registry` pointing at the objectives registry, in the same form the deployment registry uses at `:8-11`.
20. Open `Configs/Deployment/overthrowDeployments.conf`. **Expect:** the repair entry still resolves, with its four modules and `m_bDirectorOnly 0`.

**Manual — solo play-test.** Debug affordances: lower `objectiveHarassmentIntervalMinutes` and `objectiveStarvationMinutes` on `Difficulty_Normal.conf` **temporarily and revert before committing** (Q11 requires an empty diff); a higher time multiplier compresses everything; `/give-resources` and `/tick-resources` remove poverty as a variable.

1. **Start a fresh campaign on Normal and take one town.** Read the log's selection line. **Expect:** a plan name, a candidate, a runner-up and both scores. → F2
2. **Watch phase 1 end to end.** **Expect:** tower recapture attempted first where a covered tower is resistance-held, then harassment, then (at a base) sabotage; group size growing with each success; support stepping down twice. → F3
3. **Make a base the objective and let its first sabotage mission promote it.** Keep watching. **Expect:** further sabotage teams still arrive during the forward-base phase and the counter reaches the difficulty's required count. → F4
4. **Let the forward base go up. Stand on it with a squad.** **Expect:** the starvation clock runs and the base comes down. Re-run and take its **source base** instead: it also starves. → F5
5. **Let the counter-attack gate ripen at night.** **Expect:** a single "waiting for daylight" line, harassment continuing, starvation still ticking, and the battle starting after 05:00. → F6
6. **Trigger a player QRF elsewhere during phase 1.** **Expect:** the GM panel's phase and timers do not move for the duration. → F8
7. **Watch the GM panel across several operations and one 6-hour tick.** **Expect:** the pool falls by each operation's cost and nothing else moves. → F9
8. **Save mid-forward-base, quit, Continue.** **Expect:** the same plan and phase name, the same counters, exactly one forward base. Then edit `m_sPhaseName` for that phase in the `.conf`, restart, and load the same save. **Expect:** one ERROR naming the phase, no crash, a new objective on the next tick. → F10, F11
9. 🔬 **THE MODDER EXERCISE — the acceptance test for the whole design.** In **Workbench only**, with **no script change**, author a third plan: e.g. *"punitive raid on a high-support town"* — the town selector with different weights, one phase with `SendDeployment(harassment ladder, ObjectiveSelf)`, an advance condition of `ProgressAtLeast(harassment.successes, 2)`, a terminal `StartBattle`, and an `IdleFor` abort. Give it `m_fPriority 2` so it wins. Start a campaign and watch it run. **Expect:** it validates, it is selected, its phases advance, its operations arrive, and its battle starts. **Record how long it took and every place the `.conf` surface was unclear** — that record is the deliverable, not the plan. ⚠ **Do not commit the plan.** → F14
10. **Break it deliberately, three ways:** a duplicate phase name; a `SendDeployment` naming a config that does not exist; a phase with no advance condition and no terminal operation. **Expect:** three ERROR lines at world start, each naming the plan, the phase and the fault; the broken plan never selected; **the two shipped plans still running**. → F12
11. **Play a long session (an hour or more) and read the log.** **Expect:** every transition carries a reason; nothing sits still. → F7
12. **Start a second campaign without restarting the client.** **Expect:** the machine starts clean — no doubled subscriptions, no stale anchor or reserve, no previous campaign's phase name on the GM panel. → F7, F13
13. **Dedicated-server / MP pass.** The automated spine covers MP not at all. Confirm the GM panel's objective and phase rows reach a joining client, and the dismantle request still validates server-side.

---

## 7. Testing Strategy

**The automated spine covers the maths, the seams and the round trips. Everything about whether the machine still *feels* like the play-tested one, whether the `.conf` surface is authorable, and whether any of it works in multiplayer is a play-test.**

**Binding constraints, inherited from `counter-attacks/context.md` §"Gotchas & Learnings" — every one applies here:**

- **`CloneModule` copies attributes by hand and silently drops what it forgets, and it is NOT chained** — a subclass repeats its parent's whole list. ~19 new clonable classes here; **every one gets a dedicated Init case**.
- **`.conf` module order is evaluation order, and `.conf` files cannot carry comments** — the authored order is the contract and it must be documented in the module headers instead.
- **Init-tier worlds never run `PostGameStart`** — a case needing a tick installs it itself. (This also means the validator's call site is **not** exercised by the Init tier; a case calls the validator directly.)
- **Deployment fixtures must be `SetSpawnedUnitsEliminated(true)`** on the deployment **and every spawning module** before anything ticks; the autotest camera is an observer.
- **The Logic-tier rule is a directory-wide grep that does not distinguish code from comments** — `OVT_Global` and `GetGame().GetGameMode` may not appear anywhere under `TestSuites/Logic/`, prose included, nor in the pure-static files those tests grep.
- **`new` does not apply `[Attribute()]` defvalues** — a hand-built subject needs every field set explicitly, which is exactly what makes the clone cases honest.
- **The Persistence tier's assertion rule is narrow:** no persistence-framework type, no vanilla persistence type and no Overthrow save-data class anywhere under `Scripts/Game/Tests/` except the one documented save-trigger line. A case writes through the **public API**, saves, dirties, re-applies, and reads back through **public getters**. Assert **deltas, never absolutes**. 🔴 **Do not widen the reload seam** (`Instances = {gameMode}`).
- **A public counter/mutator may NEVER change phase** — only the tick moves the machine. A phase entry re-arms the phase timeout, so a transition from a public method overwrites planted timers and can save a phase nobody asked for. This cost two red cases in two suites once already.
- **A fixture driving the tick must plant a non-zero operation countdown**, and one driving past a give-up must stop **at** the grace window — the IDLE branch of the very next tick buys a real deployment with real resources.
- **`RandInt` is max-exclusive; `out` and `owned` are reserved; `vector.Distance` is +1 ULP at 1 000/2 000 m; `PrintFormat` and `SetFailure` take at most 3 params after the format string.**
- **`Rpc()` arity is a compile-check blind spot (BUG-090)** — the Init seam case is the only mechanical defence for Phase 7's wire change.
- **Never hand-edit `Language/*.conf`** — Workbench build output. Edit the `.st` master and report the owed re-export.
- **No `maxAttempts` anywhere.**

### Logic tier — Fast

| File | Status | Content |
|---|---|---|
| `OVT_TEST_Logic_ObjectivePlanRules.c` | **NEW** (Phase 2) | `ResolveWithDifficulty` for `-1`/`0`/sane/absurd; `SelectBestPlanIndex` highest-wins, ties-by-input-order, all-ineligible, empty; `AllConditionsMet([])` is **true**; `AnyAbort([])` is **false**; `PhaseIndexOf` returns `-1` for unknown and for empty |
| `OVT_TEST_Logic_ObjectiveScaling.c` (12 cases, 2 368 L) | **EXTENDED** (Phase 3) | Keep every existing case — the gates, ladder, starvation, ceiling, siting and tick-down statics are unchanged. Add: plan resolution with two plans; a priority multiplier of 2 letting a lower raw score win; 0 excluding |
| `OVT_TEST_Logic_ObjectiveAnchorAndBearing.c` (5) | **RE-POINTED** (Phase 7) | The GM formatter cases move from `FormatObjectivePhase(int)` to the phase-name row |
| `OVT_TEST_Logic_ObjectiveInsertion.c` (5), `OVT_TEST_Logic_ObjectiveReserveFloor.c` (3), `OVT_TEST_Logic_ObjectiveRepair.c` (2) | **UNCHANGED** | Insertion geometry, the reserve floor arithmetic and the repair statics are untouched. Repair's file may be renamed with its subject in Phase 1 |

### Init tier — Fast

| File | Status | Content |
|---|---|---|
| `OVT_TEST_Init_ObjectiveDirector.c` (8 cases) | **REWRITTEN** (P2, P3, P6) | Resolution, fresh-is-idle, deterministic selection, tick-freezes-under-QRF — **assertion bodies unchanged, subject re-pointed to the instance**. Plus the registry resolving off the prefab |
| `OVT_TEST_Init_ObjectiveOperations.c` (7) | **REWRITTEN** (P4) | The operation chain order, the concurrency cap, `m_bDirectorOnly`, refusal-log dedup — now driven through `SendDeployment` + resolvers |
| `OVT_TEST_Init_ObjectiveSabotage.c` (6) | **REWRITTEN** (P4) | Target filtering, the clear-radius rule, the per-mission cap, clone fidelity, difficulty precedence — subject re-pointed at the instance reporter |
| `OVT_TEST_Init_ObjectiveFOB.c` (8) | **REWRITTEN** (P5) | Siting, the raise latches, teardown, the budget ceiling, clone fidelity — driven through the raise operation module |
| `OVT_TEST_Init_ObjectiveAnchor.c` (4) | **RE-POINTED** (P5) | The anchor radius now comes from the phase attribute |
| `OVT_TEST_Init_ObjectiveReserve.c` (2) | **RE-POINTED** (P6) | Push on a refused ask, drop on a satisfied one — through the runner |
| `OVT_TEST_Init_ObjectiveInsertion.c` (4) | **SUBJECT ONLY** (P4) | The insertion module is unchanged; only its objective-side reporter moves |
| `OVT_TEST_Init_ObjectiveRepair.c` (4) | **RE-POINTED, RENAMED** (P1) | Proof the relocation was behaviour-free |
| `OVT_TEST_Init_DifficultyObjectiveSabotageInversion.c` (1) | **UNCHANGED** | The inverted gate survives; `Configs/Difficulty/` has an empty diff |
| `OVT_TEST_Init_CampaignRequestSeam.c` | **RE-POINTED** (P1) | Asset API rename only |
| `OVT_TEST_Init_TowerUnrestRecapture.c` | **UNCHANGED** | `Deployment_TowerRecaptureUnrest.conf` is byte-identical |
| **NEW: `OVT_TEST_Init_ObjectiveRegistry.c`** | (P2, P3, P4, P7) | **The validator, one case per rule**: empty name; duplicate plan name; duplicate phase name; empty phase list; missing selector; unresolvable deployment config; unresolvable ladder entry; unresolvable resolver; a phase with no advance condition and no terminal operation; `-1` with no difficulty mapping. Each asserts the plan is **named and skipped** and that **the other plans still run** |
| **NEW: `OVT_TEST_Init_ObjectiveModuleClones.c`** | (P2, P4, P5, P6) | **One case per clonable class** — ~19 of them (4 base-derived condition/operation/abort sets, 4 resolvers, 2 selectors, 2 shims while they exist). The `OVT_TEST_Init_TowerUnrestRecapture.c:214-259` shape: hand-built template with **distinct non-default** values → `CloneModule()` → per-field equality → a `SetFailure` naming the field **and its real-world consequence** |
| **NEW: `OVT_TEST_Init_ObjectiveRestore.c`** | (P2, P5) | **"Phase missing on load"**: apply a payload naming a phase the plan does not have → the instance is abandoned, an ERROR names the phase, the machine is idle and flagged for reselect, **and nothing crashed**. Same for an unknown plan name and for an unrecognised version. Also: `version == 0` (absent) **keeps live state** |
| **NEW: parity case (P3)** | 🔴 | **"The two shipped plans reproduce the single-list pick."** Drive the old and new selection paths on one deterministic fixture set while both exist and assert the same chosen position. This case is **deleted with Phase 3's cleanup** — record that in its header, with what it proved |

### Persistence tier — All

Both existing cases are rewritten against the v2 record; **no migration case is added** ([D2](#d2--no-migration-the-old-record-is-detected-discarded-and-logged)).

- **`…_ObjectiveDirector_SurvivesSaveAndReapply`** (`:9107`, Phase 2) — plan name, target kind and position, phase **name**, both tick counters, a two-key int bag, a one-key vector bag and a two-entry blacklist survive save → dirty → re-apply.
- **`…_ObjectiveFOB_RelinksItsDeployment`** (`:9566`, Phase 5) — every asset-record field survives; the re-link happens on the first tick by name + position; a payload naming a deployment that no longer exists **resets the objective** rather than stranding it.

### Not automatable, and why

| Area | Why manual |
|---|---|
| **Whether the `.conf` surface is authorable** | The modder exercise is a human judgement about clarity; no assertion can express it |
| **Whether the ramp still feels like the play-tested ramp** | In-game hours and a subjective verdict; parity of *decisions* is asserted, parity of *feel* is not |
| Whether a truck reaches a town over real roads; whether a forward-base site looks sensible | Live AI over minutes; a judgement about a place |
| Save → quit → **Continue**; two campaigns in one session | The harness restarts the suite on a world transition |
| **The Phase-1 asset-API rename** | The Init tier passes either way ([D4](#d4--the-asset-api-rename-happens-in-phase-1-with-no-wrappers)); compile + grep + Workbench + play-test stand in |
| `.conf` and prefab faults | `compile-check.sh` cannot see them; Workbench and the Init tier are the only checks |
| MP / JIP, including the changed GM wire | Uncovered by the whole spine; a dedicated-server pass is the only check |

---

## 8. Dependencies

**Hard preconditions (all satisfied today):**

- **`occupying/counter-attacks`, CLOSED 2026-08-20** — the machine being ported, and its `context.md` as the record of *why*. **The shipped code is the parity reference; two of its documented pointers disagree with it (C4, C5) and the code wins.**
- **`occupying/deployments`** — the pattern being mirrored (`OVT_DeploymentRegistry.c:1-91`, `OVT_DeploymentConfig.c`, `OVT_BaseDeploymentModule.c:114-132`, `OVT_DeploymentComponent.c:68-76`, `OVT_DeploymentSourceProvider.c:28-37`), and the purchase/track mechanism itself: `ForceCreateDeployment`, `GetDeploymentNearPosition`, `DeleteDeployment`, `RecallDeployment`, the faction pool API, `SetObjectiveAnchor`/`SetObjectiveReserve`, `m_bDirectorOnly`.
- **`occupying/qrf`** — consumed as-is. `StartBaseQRF`/`StartTownQRF` in SIEGE mode, polled through `m_CurrentQRF` going null. Never a second `m_OnFinished` subscriber.
- **`occupying/core`** — ownership/threat/reserve reads, `GetBasesControlledBy`, `GetRadioTowersAffecting`, `ChangeRadioTowerControl`, `m_OnBaseControlChanged`, and its "fires before the affiliation is applied" ordering.
- **`core/persistence`** — the serializer family, the `ComponentSerializers` block (`Configs/Systems/Persistence/Overthrow.conf:42`), and the Persistence test tier with its narrow assertion rule.
- **`core/difficulty`** — the twelve `objective*` fields (`OVT_DifficultySettings.c:81-104`), frozen.
- **`core/damage`, committed `d3ce53ef`** — the sequencing hazard in the requirements is **resolved**: its sabotage adoption is in the tree, and its repair module is moved (not edited) by Phase 1. Merge order is no longer a constraint.
- **The whole `virtualization` epic (5/5)** — the frozen `api.md`, the module seam, `WasRestoredFromSave()`, `StationsGroupsDeliberately()`, the owner-key discipline. **This feature asks core for nothing.**

**Explicitly NOT depended on:**

- **A save migration** — dropped ([D2](#d2--no-migration-the-old-record-is-detected-discarded-and-logged)).
- **The checkpoint asset** — this feature only guarantees the module shape it will use.
- **`resistance/high-command`** — the starvation predicate keeps taking player presence as a boolean argument, so swapping the source stays a one-line change.
- **The epic's open bugs** — **BUG-025** (unvalidated capture RPCs) and **BUG-028** (deployment faction-list leak) are out of scope. This feature must simply not add to either.

**Downstream (what this unblocks):** the **checkpoint asset** becomes a module plus a config rather than a system. The BUG-109 doctrine gap (nothing reacts to a discovered camp/FOB) becomes a `.conf` with a known-targets selector. Server owners and modders get a customisation surface that did not exist.

**User-side (interactive):** the [§6](#6-definition-of-done) play-test list, especially **step 9 (the modder exercise)** — it is the only test of the design's central claim, it needs Workbench and a human, and its **deliverable is the record of where the `.conf` surface was unclear**, not the plan.

---

## 9. Risks & Mitigation

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| **R1** | 🔴 **Parity drift.** The machine ends up making *slightly* different decisions — a gate off by a conjunct, an operation order swapped, a cadence that re-arms in the wrong place. Nothing errors. Nothing warns. It is found weeks later in a play-test, if at all. | **High** | The feature's one hard requirement fails, invisibly, and the reference it drifted from has been deleted | [D1](#d1--strangler-with-legacy-shim-modules-not-a-big-bang-rewrite) is the whole answer: **one legacy phase is replaced at a time, and the reference is still in the file while it happens.** Every doctrine phase's acceptance criteria include driving the shim and the replacement on the same fixture. Phase 3 carries a dedicated parity case (T3.8) for the selection fork, which is the least mechanically obvious claim in the plan. The 44 existing Init cases are re-pointed with **assertion bodies unchanged** — a rewritten assertion is a rewritten contract |
| **R2** | **A gate loses its second conjunct.** The town phase-1 gate needs *both* "support is low" **and** "this ramp's modifier is on the town"; a `SupportBelow` condition that ports only the first fires on the phase's own entry tick and skips harassment entirely. | **High** | The most visible phase of the ramp silently stops happening | T4.3 names it as a 🔴 task with the reason. `CheckHarassmentGate()`'s own header states it. An Init case asserts refuse-without-modifier / pass-with-modifier. Play-test step 2 is the live check |
| **R3** | **The phase-3 deadlock returns.** The condition module's phase *range* is what lets phase-1 operations continue into the forward-base phase; a config or a port that collapses it back to an equality test re-creates the deadlock the shipped code fixed. | Medium-High | A base objective freezes at one sabotage mission and can never reach its required count; towns deadlock one step later | T4.5 and T5.7 both state it; `EffectiveLastPhase`'s empty-through protection (`OVT_ObjectiveConditionDeploymentModule.c:139`) is ported verbatim; F4 is a player-visible criterion and play-test step 3 is the live check |
| **R4** | **`CloneModule` silently drops an attribute** on one of ~19 new classes. The standing trap of this module system — it lost `m_fMaxCruiseSpeed` once and `m_iMinTownSize` in a deliberate fault injection. | **High** | Silent wrong behaviour: a resolver with no radius, a cap of zero, a ladder that never climbs | Every concrete class hand-writes `CloneModule()` copying its own **and all inherited** attributes, and **every one gets a dedicated Init case** in the `OVT_TEST_Init_TowerUnrestRecapture.c:214-259` shape with distinct non-default values and a consequence-naming `SetFailure`. Q14 makes it countable. It is the only defence that exists |
| **R5** | **The idle clock or the reserve floor is re-derived instead of moved.** They are three play-test fixes deep (pull-not-push, in-flight holds, affordability holds but a spent ceiling does not) and each rule looks arbitrary out of context. | Medium | An objective that never times out, or one that times out during a poverty spell it cannot control — both were shipped defects once | [D11](#d11--the-runners-play-tested-semantics-are-moved-not-re-derived): the §3.2 table is a **move list**, method by method with line numbers, and each keeps its header comment. Q4's grep proves the doctrine left; the absence of these names from that grep proves the runner did not |
| **R6** | **The `.conf` surface is not actually authorable.** Everything compiles, both plans run, and a modder cannot write a third one because a module needs a companion nobody documented or an attribute whose meaning is only in code. | **Medium-High** | G1 — the reason the feature exists — fails, and no automated test can tell | The **modder exercise** (play-test step 9) is a Definition-of-Done criterion in its own right, and its deliverable is the list of unclear places. Every attribute carries a `desc:`; the validator's messages name the plan, the phase and the attribute (T7.5); module headers document evaluation order, because `.conf` files cannot carry comments |
| **R7** | **Concurrent sessions move the tree.** Every `file:line` here was verified 2026-08-21 against a clean `1.5-objectives`, and bugfix sessions commit into the same branch. The epic overview already carries a line number that drifted by ~650 lines in one day (C4). | **High** | Stale references, failed edits, designs built on facts that changed | Three phases open with a **read-only survey task** (T1.1, T3.x, T5.1) whose only job is to re-verify. No task depends on a line number for correctness — every one names the symbol as well. `sed` is the fallback when a string match fails |
| **R8** | **The asset-API rename is verified by tests that cannot fail.** A case reading `IsAssetUp("fob")` where it read `IsFOBUp()` is green whether or not the key is wired to the same record. | **Certain** (it is structural) | A silently dead FOB API: the QRF loses a wave source, the dismantle action never appears, and everything still passes | [D4](#d4--the-asset-api-rename-happens-in-phase-1-with-no-wrappers) states it explicitly and Phase 1's acceptance criteria list **three** required substitutes: compile + comment-inclusive grep, a Workbench config load, and a play-test of the three live consumers. The phase is deliberately small and alone so those checks are cheap |
| **R9** | **A save that cannot be read wedges instead of abandoning.** The version guard has three outcomes and the wrong one on the wrong input either clears live state that should have been kept (`version == 0`) or half-restores an objective (unknown plan). | Medium | A campaign that loads into a broken objective, or one whose objective is silently wiped on every re-apply | [§3.8](#38-persistence) tabulates the three outcomes; the existing `version < 1 → return true` contract at `OVT_ObjectiveDirectorSerializer.c:157-159` is preserved verbatim for the absent case; `OVT_TEST_Init_ObjectiveRestore.c` carries a case per outcome; F11 is a play-test criterion with a specific procedure |
| **R10** | **The GM wire breaks a JIP client.** The objective phase field changes type on a fan that is a wire, and `Rpc()` arity is a compile-check blind spot (BUG-090). | Medium | A client-side error storm during a GM session; a snapshot that never commits | T7.2 bumps `WIRE_VERSION` (so a mismatched client refuses to stage rather than showing a truncated snapshot), leaves `CAMPAIGN_RECORD_COUNT` and every record class untouched, and extends the Init seam case — the only mechanical check for arity. T7.1 names the **three** `OVT_GMCampaignState` edits and flags `Clear()` as the one with no symptom. Play-test step 13 is the live MP check |
| **R11** | **The validator ships without a call site**, exactly as `OVT_DeploymentRegistry.ValidateAllConfigs()` did (C6) — a complete validator that nothing runs. | Medium | R8 is decorative; a broken plan does nothing and says nothing | T2.10 makes the call site part of the task, Q17's grep requires **a declaration and a call**, and F12 is a play-test criterion that can only pass if it runs at world start. ⚠ Init worlds never run `PostGameStart`, so the Init cases call the validator directly — the call site itself is only proved by the play-test |
| **R12** | **The instance model leaks across a second campaign in one session.** A stale anchor, a stale reserve, a doubled control-change subscription, or a previous campaign's phase name on the GM panel. `virtualization/core`'s Phase 6 found four teardown bugs in exactly this shape. | Medium | Wrong behaviour that only appears on the second campaign, which nobody tests | `ClearObjectiveRecordFields()` stays the construction-time path and `ClearObjectiveRecord()` stays the runtime funnel both live paths share; `ScriptInvoker.Insert` does not de-duplicate, so `Remove` then `Insert` and unsubscribe in cleanup; `OVT_GMCampaignState.Clear()` is called out by name in T7.1. Play-test step 12 is the check |
| **R13** | **The repair rename orphans persisted instances**, if `m_sDeploymentName` is changed along with the file. | Low-Medium | Every live repair detail in every save silently drops on load | [D3](#d3--the-repair-module-moves-out-rename-the-file-never-the-config-name) states the rule three times (decision, task T1.6, `context.md`); Q13 makes "the name is unchanged" a grep-verifiable criterion and "the rename is 100 % similarity" a `git diff -M` criterion |
| **R14** | **The bag becomes a second, untyped director.** Modules start storing coordination state in it, keys collide, and "modules never add fields to the director" is satisfied in letter while the coupling moves into strings. | Medium | The design's central discipline erodes and nothing catches it | Every module declares a `GetBagPrefix()` and the validator rejects two modules in one phase declaring the same prefix. The bag's header states that a key is *state a module owns*, never a channel between modules — modules communicate through the **phase structure** (a condition reads what an operation wrote) and never by convention |
| **R15** | **Scope creep into the shipped surface.** "While we are in here" edits to QRF, the evaluator, the insertion module or FOB siting maths. | Medium | The parity claim becomes untestable and the blast radius doubles | The requirements' Out of Scope is binding and is restated as build constraints in [§3.1](#31-the-object-model). `git diff Scripts/Game/Controllers/OccupyingFaction/` empty and `git diff Scripts/Game/GameMode/Deployments/` empty are per-phase acceptance criteria, not just end-state ones |

---

## Agent Routing Summary

| Phase | Agent | Advanced? |
|---|---|---|
| 1 — Relocation + the generic asset API | `component-developer` | no — mechanical, wide, behaviour-free; but its **acceptance criteria include a Workbench check and a play-test** |
| 2 — The objective framework and the strangler seam **(gate)** | `component-developer-advanced` | **yes** — new authored-data framework on the game-mode prefab, save-format replacement, the instance model everything plugs into |
| 3 — Plan-driven selection | `component-developer-advanced` | **yes** — replaces the machine's central decision; the "two plans reproduce one list" parity claim is subtle |
| 4 — The harassment phase in config | `component-developer-advanced` | **yes** — unifies four senders behind one module, changes a deployment-side `.conf` schema, flips a difficulty convention |
| 5 — The forward base as an operation module | `component-developer-advanced` | **yes — the largest phase.** ~1,500 lines including siting, budget, starvation, teardown, a player request and a restore path with a duplication hazard |
| 6 — The battle as a terminal operation, and the last shim dies | `component-developer-advanced` | **yes** — the battle layer's only entry point, the poll-not-subscribe contract, and the seam deletion |
| 7 — Validation, presentation and the admin surface | `component-developer` (hand any `.layout` slice to `ui-developer`) | no — contained; the GM wire has a documented recipe and a seam case |
| 8 — Help & documentation sync | `help-docs-sync` | — |

**Skills to activate:** `enforcescript-patterns` (all code phases), `overthrow-architecture` (1–7), `workbench-workflow` (1, 2, 4–7 — every `.conf` and prefab authoring task, and every play-test).

**Estimate:** 92–127 h across the eight phases, of which Phases 2, 4 and 5 are roughly half.

**Owed to the user at the end:** a **localization re-export from Workbench** — Phase 7 removes four `#OVT-GMPanel_ObjectivePhase*` keys from `Language/localization_Overthrow.st`, and the `Language/*.conf` exports are build output that must never be hand-edited. Raw `#OVT-` keys on the GM panel after Phase 7 mean the re-export is outstanding, not that the strings are wrong.
