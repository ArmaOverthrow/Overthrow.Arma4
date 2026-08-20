# Objectives — Requirements

**Epic:** occupying
**Created:** 2026-08-20 (design discussion with the author; decisions D1–D6 below are settled, not proposals)
**Proof of concept:** `occupying/counter-attacks` (built and play-tested 2026-08-19/20). Every rule it shipped is kept; this feature changes *where the rules live*, not what they are.

## Overview

`occupying/counter-attacks` proved the strategic AI: a single current objective, a legible three-phase ramp (harassment → forward base → silent siege), every group bought and tracked through the deployment system. It also grew into a monolith — `OVT_ObjectiveDirectorComponent` is ~5,200 lines and **all of its doctrine is hard-coded**: which targets score how, which phases exist and in what order, which deployment configs each phase may send and at what cadence, the gates between phases, and ~1,500 lines of forward-base handling.

This feature splits the director into a **modular, declarative, config-driven** brain in the same shape as the deployment framework it sits on: an **objective registry** of **objective configs** (campaign plans), each an ordered list of **phases**, each phase a bag of **condition / operation / abort modules** authored in Workbench `.conf` files. The director itself becomes a thin runner. Server owners and modders customise the faction's decision-making by editing or adding `.conf`s — **no script needed** for a new doctrine built from shipped modules, a new module for genuinely new behaviour.

The deployment system remains the **only** mechanism that buys and tracks AI. Objectives decide *what to buy, where, and when*; deployments remain how.

## Predecessor closure (2026-08-20)

`occupying/counter-attacks` was **closed** the day these requirements were written, with `base-upgrades` (retired) and `qrf` (legacy). Its open items — QRF-phase play-test criteria, the Easy interval decision, wiki sync, the localization re-export, tests for the last day's fixes — were **dropped, not transferred**. This feature owes only what its own parity requirement (R4) and its own strings need.

## Settled decisions (author, 2026-08-20)

- **D1 — Ordered phases, not a behaviour tree.** A plan is a list of phases; a phase is a bag of modules. Easier to author in `.conf`, to persist (a phase index + a state bag), and to test than a tree. A BT is out of scope.
- **D2 — Plan selects target (Option A).** The director scores every eligible plan's best candidate and runs the top one (score × plan priority). The plan owns its target selector; adding a doctrine ("retake lost forward base", "punitive raid on a high-support town", "ambush a supply route") is a new config, possibly a new selector module — never a director edit.
- **D3 — Assets are operation modules, not a parallel concept.** The forward base (siting, budget ceiling, starvation, teardown, dismantle) becomes `OVT_RaiseForwardBaseObjectiveModule`. Future assets — **checkpoints are near-term** — follow the same shape: each is a module with its own specific handling, not a generic "prefab + config" asset type.
- **D4 — Difficulty fallback (b).** Module attributes that today come from a difficulty field default to `-1` = *"use the difficulty setting"*; an explicit value in the `.conf` overrides it. All twelve `objective*` difficulty fields and every authored preset survive unchanged, including the inverted sabotage gate; a modder who wants a hard number sets one.
- **D5 — Design for N objectives, configure 1.** The director holds a list of objective instances with a cap attribute defaulting to 1. The single-battle slot (`m_CurrentQRF`) and the single reserve become explicit exclusion points rather than implicit assumptions.
- **D6 — This feature takes over everything the director owned.** `counter-attacks` keeps its docs as the proof-of-concept record; its scripts, configs, tests and serializer move under this feature. Authoring is **Workbench `.conf` only** — no runtime JSON/registry swap.

## Requirements

### R1 — The objective model

- **`OVT_ObjectiveRegistry`** (`.conf`, `configRoot`) lists `OVT_ObjectiveConfig`s, referenced from the game mode like `overthrowDeployments.conf` is, with the same inherit-and-delta authoring so a modded registry can adjust one plan without copying it. Same conflict rule as deployments: a mod ships its own registry `.conf` and points the prefab at it.
- **`OVT_ObjectiveConfig`** = a plan: `m_sObjectiveName`, faction type, `m_iPriority`, `m_fChance`, `m_iMaxInstances`, one **target selector module**, `m_aPhases[]`.
- **`OVT_ObjectivePhase`**: `m_sPhaseName` (stable, used by deployment-side conditions and the GM panel), `m_aModules[]`, a per-phase operation cadence (difficulty-fallback), and an optional **evaluator anchor radius** (the value `AnchorRadiusForPhase` hard-codes today).
- **Module roles** (one base class, three subclasses, mirroring `OVT_BaseCondition/Spawning/BehaviorDeploymentModule`):
  - **Condition modules** — AND'd; when all are true the objective **advances** to the next phase. Shipped: `SupportBelow`, `ProgressAtLeast(key, n)`, `ReserveAtLeast`, `AssetUp(key)`, `DaylightWindow(start,end)`, `PhaseTicksAtLeast`, `TargetKindIs`.
  - **Operation modules** — ticked on the phase cadence in authored order; the first that acts consumes the cadence (today's `tower || harassment || sabotage` chain, declared). Shipped: `SendDeployment { configName, targetResolver, maxConcurrent, ladder[] }`, `RaiseForwardBase {…}`, `StartBattle { mode }`.
  - **Abort modules** — OR'd; any true **resets** the objective (with the blacklist rounds today's reset takes). Shipped: `IdleFor(minutes)` (the idle clock — operations in flight hold it, refunds on teardown), `AssetStarved(key)`, `TargetLost` (ownership flipped under us).
  - Module lifecycle is the deployment contract: `OnEnter / OnTick / OnExit / Serialize / Deserialize / Clone`, and every module's `Clone` is covered by the existing "dropped line" Init-tier pattern.
- **Target resolvers** are a small provider family (like `OVT_DeploymentSourceProvider`): *the objective itself*, *enemy radio towers affecting the objective*, *the forward base*, *nearest controlled base*. A `SendDeployment` module names one; the FOB garrison, tower recapture and sabotage operations are then all the same module with different resolvers.
- The last phase of a plan completing (e.g. `StartBattle`'s battle resolving) ends the objective: **win** → re-select; **loss** → reset with blacklist. Both outcomes are reported through the same path so a plan can end on any module, not only a battle.

### R2 — The director becomes a runner

- Keeps, unchanged in behaviour: the one-minute tick, the `m_CurrentQRF` early return, the restore-pending resolve, the reselect-is-a-flag rule (D3 of counter-attacks), the **reserve floor** push/drop per tick, the **evaluator anchor** push/drop on phase change, the **blacklist**, the "no players → no tick" rule, and the **idle-clock semantics** exactly as play-tested.
- Loses: every `Send*Operation`, every `*Gate`, every FOB method, `FireCounterAttack`, the phase `switch`, and every doctrine constant.
- **Selection** becomes: for each plan in the registry whose faction/instances/chance allow, ask its selector for `(bestCandidate, score)`; apply blacklist and priority; commit the top. The pure scoring in `OVT_ObjectiveSelection` survives as the math the shipped selectors call, with its weights lifted to selector attributes (defaults = today's constants).
- **Public read API is preserved by name** where the GM panel, HUD, `OVT_QRFControllerComponent`, `OVT_CampaignRequestComponent`, `OVT_FOBPositionComponent` and the deployment modules consume it (`HasObjective`, `GetObjectivePosition/Kind/Name`, `GetPhase` → now phase *name* + index, `IsFOBUp/GetFOBPosition` → generic `IsAssetUp(key)/GetAssetPosition(key)` with the old names kept as thin wrappers until the consumers are moved). The **write API used by tests** (`CommitObjective`, `EnterPhase`, `SetOperationCountdown`, `SetPhaseTimeout`, `ResetObjective`) is preserved against the instance.

### R3 — The objective instance and its state bag

- An `OVT_ObjectiveInstance` = config ref + target (kind, position, name) + phase index + phase ticks + next-op ticks + a generic `map<string,int>` **state bag** (+ a `map<string,vector>` for positions). Module state (`sabotage.successes`, `harassment.successes`, `fob.up`, `fob.spent`, `fob.starvationTicks`, `fob.position`, `fob.source`) lives in the bag under the module's declared key prefix — **modules never add fields to the director**.
- Deployment-side modules report progress through the instance — `objective.Report("sabotage", +1)` — not through named director methods; conditions read the bag. `OVT_BaseSabotageBehaviorDeploymentModule`, `OVT_TownHarassmentBehaviorDeploymentModule`, `OVT_FOBRaiseSpawningDeploymentModule`, `OVT_ObjectiveConditionDeploymentModule` and `OVT_ObjectiveAnchorSourceProvider` are re-pointed at the instance handle and stop referencing the director class.
- `OVT_ObjectiveConditionDeploymentModule`'s `m_iRequiredPhase/m_iThroughPhase` integers become **phase names** (`m_sFromPhase/m_sThroughPhase`), with the existing "dropped line / empty through = single phase" protections carried over.

### R4 — The two shipped plans

- `Configs/Objective/Objective_TownOffensive.conf` and `Objective_BaseOffensive.conf`, in `Configs/Objective/overthrowObjectives.conf`, reproducing the play-tested machine exactly: the selectors' weights; phase **Harassment** (tower recapture → harassment ladder → [base: sabotage], advance on `SupportBelow 50` / `ProgressAtLeast sabotage N`, abort on `IdleFor`); phase **ForwardBase** (RaiseForwardBase + FOB garrison + Phase-1 operations continuing — the deadlock fix — advance on `AssetUp fob` ∧ `ReserveAtLeast gate` ∧ `DaylightWindow 5–15`, abort on `IdleFor` / `AssetStarved fob`); phase **CounterAttack** (`StartBattle SIEGE`).
- **Behavioural parity is a hard requirement**: with the shipped registry, the objective director must make the same decisions on the same inputs as the current code. The Init-tier cases that drive the machine (`OVT_TEST_Init_ObjectiveDirector/Operations/FOB/Sabotage/Reserve/Anchor`) are rewritten against the runner + shipped plans and must assert the same outcomes.
- The remaining `Deployment_Objective*.conf`s and `Deployment_TowerRecaptureUnrest.conf` keep working; `m_bDirectorOnly` keeps its meaning (only an objective operation may create it).

### R5 — Difficulty

- Every `objective*` difficulty field keeps its name, preset values and `desc:`. Each module attribute that maps to one documents the field in its `desc:` and reads it when authored `-1`. The mapping is per module type in code (no string lookups of difficulty fields).
- The twelve fields are the **only** difficulty surface; no new ones unless a shipped module genuinely needs a knob the presets should scale.

### R6 — Persistence

- One serializer: config name + target + phase name + phase/op ticks + the state bag, version-first. A phase or config **missing on load** (mod removed, plan renamed) abandons the objective cleanly (log + re-select), never crashes.
- **Migration** from the counter-attacks record (fixed fields, integer phase enum) into the new record on first load: kind/position/name/phase map onto the matching shipped plan; counters and the FOB record land in the bag. Covered by the Persistence tier. The "enum values are frozen in saves" constraint dies with this; the migration is the one place that still reads them.
- A live battle still rolls back on load (qrf's rule); a forward base is restored as today (`WasRestoredFromSave()` gate kept).

### R7 — Presentation

- GM panel: plan name and **phase name** (from the config) replace the enum→string table. `OVT_GMCampaignState` carries the strings; `FormatObjectivePhase(int)` is retired with its keys or re-pointed.
- Battle HUD header/clock, the broadcast presets, Field Manual "Counter Attacks" page: no change in content; re-check every sentence that names a phase or number still matches the shipped plan (the fact-checking rule).

### R8 — Validation & tooling

- `ValidateAllConfigs()`-style startup validation for the objective registry: unknown deployment config names in `SendDeployment`, unknown resolver, a phase with no advance condition and no terminal operation, duplicate phase names, cadence `-1` with no difficulty mapping. Log once, loud, at world start; a broken plan is skipped, not run.
- Refusal logging (`LogOperationRefusal` dedup) and the `/give-resources`, `/tick-resources` admin verbs keep working against the instance.

## What moves (ownership)

From `occupying/counter-attacks` to this feature: `Scripts/Game/GameMode/Objectives/**` (the director, records, serializer, pure statics, all six deployment modules), `Configs/Deployment/Deployment_Objective*.conf`, the Objective Init/Logic/Persistence test files, and the GM-panel objective rows. `counter-attacks` stays as the historical feature and its `context.md` remains the authoritative record of *why* each rule exists.

**Frozen, as before:** `Scripts/Game/GameMode/Virtualization/`, `VirtualMovement/`, `docs/features/virtualization/core/api.md`, and the thirteen non-objective deployment configs. The deployment framework itself changes only at the objective-facing seam (condition module phase names, the instance handle).

## Dependencies

- **occupying/deployments** — the purchase/track mechanism; `OVT_DeploymentRegistry`/`OVT_DeploymentConfig` are the pattern being mirrored.
- **occupying/qrf** — `StartBattle` fires `StartBaseQRF/StartTownQRF` in SIEGE mode and polls `m_CurrentQRF` going null (never a second `m_OnFinished` subscriber).
- **occupying/core** — ownership/threat/reserve reads; `OnBaseControlChange` semantics (D3).
- **core/persistence** — the serializer family and the Persistence test tier.
- **core/difficulty** — the twelve `objective*` fields.
- **core/damage** (in progress) — its sabotage adoption edits `OVT_BaseSabotageBehaviorDeploymentModule` in place; this feature moves that file, so sequence the two or merge the edit first.

## Out of Scope

- New doctrines beyond the two shipped plans (a third plan is the first thing a *modder* should be able to write, and is the acceptance test for the design — but it is not shipped here). ⚠ One known **doctrine gap** to keep in view for that third plan: since `OVT_BaseUpgradeSpecops` was deleted, `UpdateKnownTargets()` still marks discovered resistance camps/FOBs as `ATTACK` targets but **nothing reacts** (BUG-109, closed as obsolete). A "raid a discovered camp/FOB" plan with a known-targets selector is the natural first non-shipped doctrine.
- The checkpoint asset (near-term, its own feature; this feature only guarantees the module shape it will use).
- A behaviour tree, runtime JSON/registry swapping, an in-game editor for plans.
- Any change to QRF combat, the siege mode, insertion driving, FOB siting math, or the deployment evaluator beyond the seam above.
- Multiple simultaneous objectives being *tuned* — designed for N, shipped as 1.

## Testing expectations

- Logic tier: the pure statics unchanged; new pure cases for plan scoring/priority resolution and condition/abort evaluation over a bag.
- Init tier: the rewritten director cases (parity), module `Clone` drop-a-line cases, registry validation cases, a "phase missing on load" case.
- Persistence tier: new-record round trip; **migration** from a counter-attacks-format save with an objective + FOB live.
- Play-test: the same F-criteria as counter-attacks (the ramp reads identically), plus one **modder exercise** — author a third plan from shipped modules only, in Workbench, and watch it run.

## Open questions for planning (not blocking)

- Whether `OVT_ObjectiveInstance` is a `Managed` object inside the director or its own component/entity (matters only for N>1 and replication of the read API; today nothing replicates).
- Whether the selector's per-candidate scoring runs every tick in IDLE or on the reselect flag only (today: flag + IDLE); with N plans the cost multiplies and should be measured.
- How a phase hands a *position* to the next (e.g. FOB site → siege wave source) — bag vector keys are the proposal; confirm no resolver needs more than one.
