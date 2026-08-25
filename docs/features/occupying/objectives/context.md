# Objectives - Context & Decisions

**Last Updated:** 2026-08-23
**Current Phase:** Complete — all 8 phases built, Ready for Review
**Status:** 🟢 Built, awaiting human verification

---

## Quick Status

**What's Done:**
- ✅ **ALL 8 PHASES BUILT 2026-08-21**, 74/74 tasks. Every code phase gated on the **All** group and green:
  P2 417/418 · P3 419/420 · P4 430/431 · P5 437/438 · P6 437/438 · P7 439/440. P8 skipped the gate (docs only).
  The single red throughout is `OVT_TEST_Init_CompositionSlotGate_AcceptedTypesMatchTheCompositions`, a
  **pre-existing `core/damage` leftover** — Phase 2 touched no composition or slot-gate code and neither did any
  phase after it.
- ✅ The director is a **runner**: 5,201 L at feature start → 4,383 L, with the harassment, forward-base and
  counter-attack doctrine blocks all gone into authored modules.
- ✅ **The strangler seam is fully dismantled** — both shim classes, the `m_iLegacyPhase` authoring, the phase enum,
  and all three temporary fallback paths.

**Post-build (2026-08-23):**
- ✅ **The forward base now comes back on load.** It never did: `Track()` makes an entity saveable, but only a `.conf` rule with `SelfSpawn` makes it RETURN, and the FOB prefab matched none of Overthrow's four rules. Fixed with a fifth rule on `OVT_OccupyingFlagComponent` + a serializer that re-queues the navmesh. See the 2026-08-23 section at the end of this file.
- ✅ **Land-isolated targets are gated out of objective selection.** Erquy Harbour is on an island and was selectable (~4.7 km to the nearest holding, inside the 5 km cutoff). New authored `m_bLandIsolated` flag → `OVT_BaseData.landIsolated`/`OVT_TownData.landIsolated` → a `continue` in `OVT_ObjectiveCandidateSet.AddResistanceBases/AddResistanceTowns`. ⚠ **The general lesson: every water check in the tree is a POINT test (`IsOceanAtPosition`, 9 call sites); there is no reachability test anywhere, and the engine offers no A→B query to build one from.** Any future doctrine that moves a force overland inherits this and must respect the flag.

**What's Next:**
- ⏸️ **Nothing automated.** The remaining work is the "Needs Human Verification" list in `tasks.md` — Workbench
  loads, play-tests, the MP/host pass on the changed GM wire, the localization re-export, and the wiki publication.
- ⏸️ The **modder exercise** (§6 step 9) is the acceptance test for the whole design and has not been run.

**Blockers:**
- None for the build. The wiki publication is blocked on the `wikijs` MCP server not being attached.

---

## ⚠ Worktree relocated 2026-08-21 — the tree moved to a Windows-native path

This worktree was moved from `/home/aaronstatic/.herdr/worktrees/Overthrow.Arma4/1-5-objectives` (WSL ext4) to
**`/mnt/n/Projects/Arma 4/Overthrow.Arma4-objectives`** (`N:\Projects\Arma 4\Overthrow.Arma4-objectives`) so the
tree can be registered as a Workbench project for play-testing, alongside the existing `Overthrow.Arma4-main`
worktree.

- `git worktree move` **fails cross-device** (ext4 → drvfs: "Invalid cross-device link"). The move was done as
  `cp -a` + `git worktree repair`, which preserved the staged renames and every uncommitted edit exactly.
- **`tools/run-tests.sh`'s addon-load guard was hardcoded to `Overthrow.Arma4/addon.gproj`** and therefore
  reported a **false INDETERMINATE (exit 2)** for *any* worktree whose directory is not literally named
  `Overthrow.Arma4` — including the old WSL path and this one. It now derives the expected gproj from
  `$OVT_REPO_ROOT` in Windows forward-slash form, exactly as `compile-check.sh:347` already did, keeping the old
  pattern as a fallback. ⚠ This is a shared-tool fix, outside the feature's scope, made because it blocked the
  gate; `Overthrow.Arma4-main` had the same latent problem.
- Side benefit: the native path is roughly twice as fast (All suite 88 s vs 161 s; compile-check 6 s vs 12 s),
  and the `VehicleReserveRelease` 500 ms I/O timeout seen on the WSL run did not recur.

**Blockers:**
- None

---

## Key Files

### Owned by this feature (taken over from `counter-attacks`, CLOSED 2026-08-20)
- `Scripts/Game/GameMode/Objectives/OVT_ObjectiveDirectorComponent.c` — 5,201 L at start; becomes the **runner** (~2,000–2,200 L target)
- `Scripts/Game/GameMode/Objectives/OVT_ObjectivePhaseRules.c`, `OVT_ObjectiveSelection.c`, `OVT_FOBSiting.c` — the pure statics
- `Scripts/Game/GameMode/Objectives/OVT_ObjectiveRecords.c`, `OVT_ObjectiveDirectorSerializer.c` — persistence
- `Scripts/Game/GameMode/Objectives/Modules/` — the objective-side deployment modules
- `Configs/Deployment/Deployment_Objective*.conf` — the five objective deployment configs

### New in this feature
- ✅ **Phase 2:** `Scripts/Game/GameMode/Objectives/OVT_ObjectivePlanRules.c`, `OVT_ObjectiveRegistry.c`,
  `OVT_ObjectiveConfig.c`, `OVT_ObjectivePhase.c`, `OVT_ObjectiveInstance.c`
- ✅ **Phase 2:** `Objectives/Modules/OVT_BaseObjectiveModule.c` + `…{Condition,Operation,Abort}Module.c`,
  `OVT_LegacyPhaseObjectiveOperation.c`, `OVT_LegacyPhaseObjectiveCondition.c`
- ✅ **Phase 2:** `Objectives/Resolvers/OVT_ObjectiveTargetResolver.c` (the seam only; the four concrete
  resolvers arrive with the send-deployment operation)
- ✅ **Phase 2:** `Configs/Objective/overthrowObjectives.conf`, `Objective_TownOffensive.conf`,
  `Objective_BaseOffensive.conf` (+ `.meta`), wired on `Prefabs/GameMode/OVT_OverthrowGameMode.et:51`
- ✅ **Phase 3:** `Objectives/Selectors/OVT_ObjectiveTargetSelector.c`, `OVT_ObjectiveCandidateSet.c`
  (+ the `OVT_EObjectiveCandidateSource` flag enum), `OVT_ResistanceTownObjectiveSelector.c`,
  `OVT_ResistanceBaseObjectiveSelector.c`; the `m_Selector` block in both plan `.conf`s
- ✅ **Phase 4:** `Objectives/Resolvers/OVT_ObjectiveSelfTargetResolver.c`,
  `OVT_EnemyTowersAffectingTargetResolver.c`, `OVT_ForwardBaseTargetResolver.c`,
  `OVT_NearestControlledBaseTargetResolver.c`; `Objectives/Modules/OVT_SendDeploymentObjectiveOperation.c`,
  `OVT_SupportBelowObjectiveCondition.c`, `OVT_ProgressAtLeastObjectiveCondition.c`,
  `OVT_TargetKindIsObjectiveCondition.c`, `OVT_IdleForObjectiveAbort.c`;
  `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_ObjectiveModules.c` (9 cases)
- ✅ **Phase 6:** `Objectives/Modules/OVT_StartBattleObjectiveOperation.c`. **DELETED:**
  `OVT_LegacyPhaseObjectiveOperation.c`, `OVT_LegacyPhaseObjectiveCondition.c`
- ✅ **Phase 7:** no new files. The GM wire changed shape, the phase enum was deleted and the
  validator's every message was rewritten - see the Phase 7 record at the end of this file

### Reference, not edited
- `docs/features/occupying/counter-attacks/context.md` (5,478 L) — the authoritative record of *why* each shipped rule exists. **The shipped code is the parity reference, not the prose.**
- `docs/features/occupying/deployments/` — the pattern being mirrored
- `docs/features/virtualization/core/api.md` — 🔒 FROZEN; this feature asks core for nothing

---

## Important Decisions

The eight plan-time decisions (D1–D11) live in `implementation.md` §5 and are not restated here. The four that
most affect day-to-day work:

- **D1 — Strangler, not rewrite.** Phase 2 authors both plans in full behind *legacy shim modules* that call
  `TickHarassment()`/`TickFOB()`/the CA gate verbatim; phases 4–6 swap one shim for real modules and delete
  what it wrapped **in the same phase**. Parity becomes a comparison, not a reading.
- **D2 — No save migration.** v1.5 is unreleased: an unrecognised record is logged, discarded and re-selected.
  The "phase enum frozen in saves" constraint (`OVT_ObjectiveRecords.c:1-13`) is **dead** and its header must be
  rewritten, not obeyed.
- **D3 — The repair module moves out** to the deployments framework. Safe because a deployment's persistence
  key is `m_sDeploymentName`, **not** the `.conf` file name (C8).
- **D4 — No `IsFOBUp`/`GetFOBPosition` wrappers.** All 11 consumers move to `IsAssetUp(key)` /
  `GetAssetPosition(key)` in Phase 1. 🔴 This is the one change the Init tier structurally cannot catch: a case
  reading `IsAssetUp("fob")` is green whether or not the key resolves. Grep + Workbench + play-test are the
  required substitutes.

---

## Gotchas & Learnings

Inherited and binding, from `counter-attacks/context.md` — every one applies here:

1. **`CloneModule` copies attributes by hand, silently drops what it forgets, and is NOT chained.** A subclass
   repeats its parent's whole list. ~19 new clonable classes here; **every one gets a dedicated Init case**.
2. **`.conf` module order is evaluation order, and `.conf` files cannot carry comments.** The authored order is
   the contract; document it in the module headers instead.
3. **Init-tier worlds never run `PostGameStart`** — a case needing a tick installs it itself, and the validator's
   call site is therefore not exercised by the tier (a case calls the validator directly).
4. **Deployment fixtures must be `SetSpawnedUnitsEliminated(true)`** on the deployment **and every spawning
   module** before anything ticks; the autotest camera is an observer.
5. **The Logic-tier rule is a directory-wide grep that does not distinguish code from comments.**
6. **`new` does not apply `[Attribute()]` defvalues** — a hand-built subject needs every field set explicitly.
7. **The Persistence tier's assertion rule is narrow:** public API in, public getters out; assert deltas, never
   absolutes; 🔴 do not widen the reload seam (`Instances = {gameMode}`).
8. **A public counter/mutator may NEVER change phase** — only the tick moves the machine. This cost two red
   cases in two suites once already.
9. **`out` and `owned` are reserved EnforceScript identifiers.**
10. **`Rpc()` arity is a compile-check blind spot** (BUG-090) — a wrong argument count compiles clean and dies
    at the wire. Relevant in Phase 7.

**New, learned in this feature, and binding from here on:**

11. 🔴 **A module can swap the runtime module set from inside its own method.** The legacy operation shim does
    exactly that — the hard-coded tick it calls advances the phase, which re-clones the set. The runner
    therefore snapshots the phase's modules into a **strong-ref** local (`array<ref …>`) before running any of
    them: iterating the live array walks a mutating collection, and iterating a *weak* copy frees the very
    object whose method is on the stack. Both are crashes, and neither is a compile error.
12. **A `.conf` field with no reader is worse than a missing one.** A server owner tunes it, nothing happens,
    and the whole authored surface loses credibility. Attributes land in the build phase that reads them.
13. **`EnterPhase()` must stay THE ONE FUNNEL.** It is what lets the hard-coded gates and the plan-driven
    advance both keep the instance in step without either knowing about the other. The legacy condition shim's
    out-of-step WARNING is the tripwire if it ever stops being true.
14. 🔴 **A SAVE CONTEXT'S PROPERTIES ARE KEYED BY THE LOCAL VARIABLE'S NAME, NOT BY POSITION.**
    `SaveContext.Write(x)` writes under the property `"x"`; `LoadContext.Read(y)` reads the property `"y"`.
    Writing `objectives` and reading into `readObjectives` does not read the wrong field — **it reads nothing,
    silently**, leaving the destination at its default. `OVT_JobManagerSerializer` measured this on 2026-08-09;
    **this feature hit it again in Phase 2** (see below). Two consequences, both binding from here on:
    - **Name every read local exactly what the write local was called.** `compile-check.sh` cannot see this and
      neither can any reviewer who is reading the two halves separately.
    - **A per-record field loop is impossible.** Writing `configName` once per record writes ONE property N
      times. N records travel as an **array of record objects under one name** — the shape `OVT_PersistedJobV2`,
      `OVT_PersistedBase` and `OVT_PersistedLoadoutItem` already use.
    - **Check every `Read()` return.** A failed read leaves its destination non-null and empty, which applies as
      "this campaign has nothing" — indistinguishable in play from the system working.
15. **`array<bool>` has no precedent in any payload in this tree.** A single `bool` field does, and `array<int>`
    does. A save format is not the place to be the first caller of an untested shape — flatten to 0/1.
16. 🔴 **`reference` IS A RESERVED EnforceScript IDENTIFIER**, alongside `out` and `owned` (#9). A parameter or
    a local called `reference` fails with `error: Expected name, not a keyword 'reference'`; a local called
    `reference` also produces the *unrelated-looking* `Broken expression (missing ';'?)`. Both were hit in
    Phase 3 — once in a selector's saturation helper, once in the parity case. Suspect the identifier before
    the expression. ⚠ And when renaming one, do NOT run a bare `\breference\b` substitution over the file:
    "reference" is also an ordinary English word and it appears in prose comments, including comments this
    feature did not write.
17. **A `.conf` CAN author a `protected` `[Attribute]` field.** There is no precedent for it under
    `Objectives/` or `Deployments/` (every module attribute there is public), which made it look unsafe —
    but `Configs/Map/OverthrowMap.conf` authors `OVT_MapInfluenceLayer.m_fRefreshInterval`, which is
    `protected` on a `[BaseContainerProps()]` class. The selectors keep their weights protected.
18. **The Logic tier's grep is for `OVT_Global` and `GetGame(`, and nothing else.** The tier rule is stated
    in `OVT_TEST_LogicSuite.c`'s header: those two identifiers may not appear anywhere under
    `TestSuites/Logic/`, prose included. Words like "world" and "entity" are fine in prose, and are
    unavoidable when describing what the tier excludes.
19. 🔴 **A DELETION ACCEPTANCE GREP IS A SUBSTRING MATCH AND IT READS COMMENTS.** Every phase of this
    feature deletes named methods and gates on `grep -rn "<name>" Scripts/` being empty — so the
    historical prose that explains WHY a rule exists has to be rewritten to describe the thing rather
    than to name the method. Phase 4 rewrote ~30 such lines across the director, six modules and five
    test files. **Do the prose pass as part of the deletion, not after the grep fails**, and prefer a
    description ("the hard-coded forward-base gate", "the tower-recapture sender") to a bare removal:
    the reasoning is the reason the comment is there.
20. **A deployment `.conf` may only be re-authored if this feature OWNS it.** The difficulty-convention
    flip is behaviour-neutral only when the config is re-authored in the same pass, so a module shared
    between an objective config and a frozen non-objective one **cannot be flipped at all**. Check
    `grep -rl "<ModuleClass>" Configs/` before flipping any convention.
21. **`array<string>` authors in a container file one quoted entry per line**, e.g. vanilla
    `ExplosiveCharge_base.et`'s `m_aIgnoredComponents`. There was no precedent under `Configs/` in this
    mod before Phase 4's harassment ladder; the vanilla one is the shape to copy, and an Init case that
    asserts the array is non-empty is what turns a block that failed to parse into a red test.
22. 🔴 **NEVER SUBSCRIBE TO THE BATTLE'S END — POLL IT.** Both of `OVT_OccupyingFactionManager`'s finish
    handlers DELETE the battle controller's entity from inside the invoker's own dispatch, so a second
    subscriber ordered after them runs against a deleted entity and the crash is in the engine's dispatch
    rather than anywhere a stack trace would point. `DirectorTick()`'s third early return already IS the
    poll: while `m_CurrentQRF` is set no module is reached at all, and the first tick that reaches one
    again is the first tick after the battle resolved. `grep -rn "m_OnFinished"
    Scripts/Game/GameMode/Objectives/` being empty is a Definition-of-Done criterion.
23. **A phase whose operation WAITS rather than SPENDS must author `m_iOperationCadence 0`.** The runner
    only asks operation modules once the cadence has elapsed, so a polling phase left on the difficulty
    interval is asked again up to a whole interval after the thing it was waiting for happened - sixty
    in-game minutes on Normal for the shipped battle phase, during which a finished objective holds the
    machine's one objective slot and the deployment bias. Zero means "every in-game minute" and is a
    supported authoring gesture; the Init tier pins the cadence PER PHASE for exactly this reason.

---

## Testing Approach

Three tiers, per `implementation.md` §7:

- **Logic (Fast)** — pure statics only; world-free; `OVT_TEST_Logic_ObjectivePlanRules.c` (new) and
  `OVT_TEST_Logic_ObjectiveScaling.c` (extended).
- **Init (Fast)** — the 44 existing objective cases re-pointed at the instance, plus one clone case per new
  clonable class, the validator rules, and the T3.8 old-path-vs-new-path parity case (landed 2026-08-21 as
  `…_PlanDrivenSelectionReproducesTheSingleListPick`).
  ⚠ **The selectors are NOT clonable and deliberately have no `CloneModule()`**, so they carry no
  dropped-line hazard and need no clone-fidelity case. A phase's *modules* are cloned per objective because
  they latch state; a selector is arithmetic over its arguments and the plan's one authored instance is read
  by every round of every campaign in the session.
- **Persistence (All)** — the v2 record round trip and the asset-record relink.

**Test-run policy:** `tools/compile-check.sh` freely; `tools/run-tests.sh` **once per completed phase, by the
orchestrator only** (`.claude/test-policy.md`). Fast `{6A6E29FF47ECB840}`, All `{6A6E2A002F53A581}`. Phases 1–7
run **All**; Phase 8 skips the gate.

---

## Phase 1 — the asset-API rename and the repair relocation (2026-08-21)

### T1.1 survey verdict — the plan's list was complete, with two comment-only extras

Every `IsFOBUp` / `GetFOBPosition` call site the plan predicted was found exactly where it predicted, and no
concurrent session had added one.

| Site | Expected | Found |
|---|---|---|
| `OVT_QRFControllerComponent.c` | `:634, :636` | ✅ exact |
| `OVT_ObjectiveAnchorSourceProvider.c` | `:78, :81, :119, :122, :157, :160` | ✅ exact |
| `OVT_ObjectiveDirectorSerializer.c` | `:120, :123` | ✅ exact |
| `OVT_ObjectiveDirectorComponent.c` | the two declarations, `:5135`, `:5138` | ✅ exact — both **deleted** |
| `OVT_TEST_Init_CampaignRequestSeam.c` | `:161, :174` | ✅ exact |
| `OVT_TEST_Init_ObjectiveDirector.c` | `:91` | ✅ exact |
| `OVT_TEST_Init_ObjectiveFOB.c` | `:449, :515, :531, :678, :793, :807` | ✅ exact (`:449` is a **comment** quoting a deleted line) |
| `OVT_TEST_PersistenceRoundTripSuite.c` | `:9346, :9411, :9414, :9544, :9819, :9822, :9857` | ✅ exact (`:9544` is a **comment**) |
| `OVT_DismantleEnemyFOBAction.c` | `:12` prose | ✅ comment only — the action never called either method (the two-entry-point rule stands unchanged) |
| `OVT_TEST_Logic_GMPanelFormat.c` | "if it names either method in prose" | **It does not.** Zero hits; no edit needed |

**Two director consumers the plan's wider list did not name, both harmless:**
`OVT_QRFModes.c:36` and `OVT_OccupyingFactionManager.c:1737` name `OVT_ObjectiveDirectorComponent` in **comments
only**. Neither calls the read API. Recorded, not edited.

The wider read-API consumers the plan listed (`OVT_CampaignRequestComponent`, `OVT_GMRequestComponent`,
`OVT_OverthrowGameMode`, `OVT_Global`, `OVT_FOBPositionComponent`, the GM panel pair) were re-checked and none
of them touched the deleted pair — they read `HasObjective` / `GetObjective*` / the FOB **detail** getters
(`GetFOBSourceBasePosition`, `GetFOBSpent`, `GetFOBStarvationTicks`, `GetFOBDeploymentName`, `GetFOBSite`),
which are FOB-specific and are deliberately **not** part of the generic API. They keep their names.

### How the key was wired so it cannot silently resolve to nothing

`OVT_ObjectiveFOBRecord` now **extends** a new `OVT_ObjectiveAssetRecord` (which owns `up` and `position`), and
the director inserts `m_FOB` itself into `m_mAssets` under `ASSET_FOB` in `OnPostInit()`. **The map entry IS
`m_FOB`, not a copy**, so the keyed API and every FOB write path read the same two fields by construction —
there is no synchronisation step that can be forgotten and no second source of truth to drift. That is the
structural answer to D4's "a green test proves nothing" problem: the only way `IsAssetUp(ASSET_FOB)` can
disagree with the old `IsFOBUp()` is if the `Insert` line is deleted, which makes the *whole* key vanish rather
than go subtly stale.

`ASSET_FOB` is a `static const string` on the director, so the literal `"fob"` appears exactly once in the tree
and every consumer spells the key as `OVT_ObjectiveDirectorComponent.ASSET_FOB` — a compile error when mistyped.

### The rename rule, stated once (C8, D3)

- **A deployment `.conf`'s FILE NAME IS A HINT.** The registry references it by GUID with the path attached for
  readability (`overthrowDeployments.conf:47`), so renaming the file costs exactly one line there plus the
  `Name` line inside its own `.conf.meta`.
- **THE `.conf.meta` GUID IS THE IDENTITY.** `{6B70D00000000038}` was preserved byte-identically through the
  rename; only the path half of its `Name` field changed, because a `.meta` embeds its own resource path and
  Workbench would otherwise rewrite it.
- 🔴 **`m_sDeploymentName` IS FATAL AND WAS NOT TOUCHED.** `"Base Repair Detail"` is the persistence key
  (`OVT_DeploymentComponentSerializer.c:74-78` writes it, `OVT_DeploymentRegistry.FindConfigByName` resolves
  it) and the string every test and every re-link matches on. Renaming it would orphan every persisted repair
  instance. It is unchanged, and `OVT_TEST_Init_BaseRepair` case A asserts it.

### 🔴 This phase's correctness is NOT covered by the Init tier

A case that reads `IsAssetUp(ASSET_FOB)` where it read `IsFOBUp()` is green **whether or not the key resolves to
the same record** — the tier can only prove the method exists and answers, never that it answers about the FOB.
Three substitutes stand in for it and **all three are required before Phase 1 is called done**:

1. ✅ **Compile + comment-inclusive grep.** `bash tools/compile-check.sh` → 0 (6202 files);
   `grep -rn "IsFOBUp\|GetFOBPosition" Scripts/` → **empty**, comments included.
2. ⏳ **Workbench** — open `Configs/Deployment/overthrowDeployments.conf` and confirm the repair entry still
   resolves to a config with its four modules. `compile-check.sh` cannot see `.conf` faults.
3. ⏳ **Play-test** — run the forward base to standing and confirm (a) the QRF wave-source list still includes
   it (`OVT_QRFControllerComponent.c:633-636`), (b) the dismantle action still appears and still refuses with
   enemies near, and (c) the FOB survives a save/Continue.

### Deviations and notes for the orchestrator

- **The acceptance criterion "two renames at 100 % similarity" is not literally achievable** for the `.conf.meta`:
  it embeds its own path in its `Name` field, so it is a rename **plus one changed line**. The `.conf` itself is a
  clean 100 % rename. The GUID is byte-identical, which is what the criterion is protecting.
- **The criterion `grep -rn "IsFOBUp\|GetFOBPosition" Scripts/ docs/features/occupying/objectives/` → empty was
  met for `Scripts/` only.** `implementation.md`, `tasks.md` and `requirements.md` still name the old pair, and
  must: they are the plan that says "delete `IsFOBUp()`" and the decision record that explains why. Editing them
  would destroy their meaning. The hard form of the criterion — §8's Q9, `grep … Scripts/` → empty — passes.
- **`Language/localization_Overthrow.st:3800`** carries a translator `Comment` that fact-checks a Field Manual
  page against `Configs/Deployment/Deployment_ObjectiveRepair.conf` by its **old** path. It is a stale reference
  inside a comment, harmless to the build, and the `.st` master was deliberately **not** touched (an unbalanced
  brace there is data loss on the next Workbench save, and any edit owes a re-export). Fix it in Phase 8.
- **`tasks.md`'s frozen-neighbour list includes `Scripts/Game/Controllers/OccupyingFaction/`, which T1.3 requires
  editing** (`OVT_QRFControllerComponent.c`, 2 lines). The two rules contradict; T1.3 wins, and the diff there is
  exactly the two keyed-API call sites. Worth correcting in `tasks.md` before a later phase trips on it.
- `OVT_TEST_Init_ObjectiveRepair.c` **was** renamed to `OVT_TEST_Init_BaseRepair.c` (the plan's "consider", and
  §7's table already records it as RENAMED), with its four case classes renamed to match. Cases are discovered
  by their `[Test(suite:)]` annotation, not by a registry, so the rename carries no registration cost; the one
  cross-reference in `OVT_TEST_Logic_ObjectiveRepair.c:11` was updated. **No assertion changed meaning.**

---

## Phase 2 — the objective framework and the strangler seam (2026-08-21)

### The one structural decision everything else follows from: THE RECORD IS SHARED, NOT COPIED

`OVT_ObjectiveInstance` owns `m_Record` (the existing `OVT_ObjectiveRecord`) and `m_mAssets`, and the
director's `m_Objective` and `m_mAssets` fields are **the same objects, not copies**. That is the same
structural trick Phase 1 used to wire the keyed asset API to the forward-base record, used again for the same
reason: with one object there is no synchronisation step that can be forgotten and no second source of truth
to drift. The ~4,000 lines of runner that read `m_Objective.phase` are therefore reading the instance's own
state, unedited — which is what let the whole framework land with byte-identical behaviour instead of a
4,000-line diff.

The record is folded **into** the instance in Phase 6, when the legacy phase enum has no readers left.

### The shim contract, and exactly when each piece dies

| Piece | What it is | Deleted in |
|---|---|---|
| `OVT_LegacyPhaseObjectiveOperation` (`m_iLegacyPhase 1`) | calls `RunLegacyPhaseTick(1)` → `TickHarassment()` verbatim | **Phase 4** (authoring + the 13 methods) |
| ~~`OVT_LegacyPhaseObjectiveOperation` (`m_iLegacyPhase 2`)~~ | ~~`RunLegacyPhaseTick(2)` → `TickFOB()`~~ | ✅ **DELETED, Phase 5.** The authoring is gone from both plans and the `FOB` case is gone from the switch |
| ~~`OVT_LegacyPhaseObjectiveOperation` (`m_iLegacyPhase 3`)~~ | ~~`RunLegacyPhaseTick(3)` → `TickCounterQRF()`~~ | ✅ **DELETED, Phase 6.** The class file is gone |
| ~~`OVT_LegacyPhaseObjectiveCondition` (all three)~~ | ~~**answers false, always** — see below~~ | ✅ **DELETED, Phase 6.** The class file is gone |
| ~~`OVT_ObjectiveDirectorComponent.RunLegacyPhaseTick(int)`~~ | ~~the one promoted entry point~~ | ✅ **DELETED, Phase 6** |
| ~~`RunUnplannedObjectiveTick()` — the strangler fallback~~ | ~~runs the hard-coded tick when an objective has no plan behind it~~ | ✅ **DELETED, Phase 6.** Replaced by `LogObjectiveWithNoPlan()`, which logs and does nothing else |
| `PlanIndexForLegacyPhase()` / `LegacyPhaseForPlanIndex()` / `LegacyPhaseForSavedName()` | enum ↔ plan-index mapping | **Phase 7**, with the enum — it outlived the strangler, see below |
| ~~`ResolveLegacyPlan()` + `LEGACY_TOWN_PLAN` / `LEGACY_BASE_PLAN`~~ | ~~picks a plan by objective KIND~~ | ✅ **DELETED, Phase 3** |
| `LEGACY_PHASE_HARASSMENT` / `_FORWARD_BASE` / `_COUNTER_ATTACK` | the three shipped phase names as constants | **Phase 7**, with the enum |

`grep -rn "OVT_LegacyPhase\|RunLegacyPhaseTick\|RunUnplannedObjectiveTick\|ResolveLegacyPlan" Scripts/ Configs/`
returns **nothing** as of Phase 6, and that is the Definition-of-Done grep for the strangler.

⚠ **The wider form this line originally carried (`LegacyPhaseFor`, `PlanIndexForLegacy`, `LEGACY_`) is NOT empty,
and was never going to be.** Those three methods and three constants map the *enum* to plan indices, and
`OVT_EObjectivePhase` OUTLIVED the strangler: the Game Master wire still carries `m_iObjectivePhase` as an
integer and the objective record still stores one. They go in **Phase 7**, with T7.1.

### 🔴 The condition shim answers FALSE, always, and that is not an oversight

**Every one of the three hard-coded phases owns its own transition.** `TickHarassment()` calls
`CheckHarassmentGate()` which calls `EnterPhase(FOB)` itself; `TickFOB()` calls `FireCounterAttack()` which
calls `EnterPhase(COUNTER_QRF)` itself; `TickCounterQRF()` calls `ResetObjective()` itself. **There is no gate
left over for a condition module to own.** If the shim condition answered true the runner would advance a
*second* time in the same tick, straight past the phase the legacy gate had just entered, and both shipped
plans would skip a phase the first time they advanced.

It is authored anyway, for three reasons: the phase's module bag already has the shape its replacement will
have (Phases 4–6 swap a condition for a condition rather than restructuring); the runner's condition path —
clone, `Initialize`/`Exit`, the AND-fold, and the "a tick that advances runs no operation" rule — runs once per
in-game minute in every live campaign from day one; and it is the place a reader looks for "who owns the
transition during the strangler".

**Deviation from `implementation.md` §3.7, recorded deliberately.** The plan's sketch had the condition shim
calling `CheckXGate()`. That cannot work: the gate is not a predicate, it transitions. The operation shim
therefore carries the whole legacy tick (which is what makes parity exact by construction) and the condition
shim is a truthful no-op with a loud header. `T2.13`'s "advances on the shim condition" is covered instead by
`…_GPhaseAdvanceArithmeticAndModuleLifecycle` (the advance arithmetic and the module lifecycle, hand-built) plus
`…_ECommitEntersPhaseZeroAndEveryEntrySyncsTheInstance` (every real transition re-syncs index, name and module
set). The end-to-end runner advance becomes drivable in **Phase 4**, when the first real condition exists.

### The bag: keys, owners and the two rules that govern them

`OVT_ObjectiveInstance` carries **two generic maps and one asset registry**, and nothing else:

| Key | Map | Owner | Was |
|---|---|---|---|
| `harassment.successes` | int | town harassment behaviour module → instance | `m_Objective.harassmentSuccesses` — **field deleted in Phase 2** |
| `sabotage.successes` | int | base sabotage behaviour module → instance | `m_Objective.sabotageSuccesses` — **field deleted in Phase 2** |
| `fob.up` / `fob.position` / `fob.source` / `fob.spent` / `fob.starvationTicks` / `fob.deploymentName` | **asset record** under key `"fob"` | the forward base | `m_FOB` — unchanged object, now registered on the instance |

⚠ **The two success counters moved into the bag in Phase 2, not in Phase 4.** They were never properties of the
target; they were state a module accumulated about it. Moving them now means Phase 4's
`objective.Report("harassment.successes", +1)` is a one-line swap against storage that already round-trips, and
it gives the bag a real job — and a real save-format assertion — from the moment it ships.
`GetHarassmentSuccesses()` / `GetSabotageSuccesses()` kept their names and their meanings, which is why every
case that asserts on them is unchanged.

⚠ **One string does not fit two maps.** `fob.deploymentName` is the re-link key, and it lives on the **asset
record** rather than in a third typed map. The asset registry is `map<string, ref OVT_ObjectiveAssetRecord>` on
the instance, and the entry under `"fob"` **is** the director's `m_FOB` object — the Phase-1 wiring, moved.

⚠ **The int bag is integers.** A module wanting a metre-scale float scales it and documents the unit in the key
(`fob.facingDeg`); a third typed map is a save-format change.

### 🔴 "A public mutator may NEVER change phase", restated at the new call sites

The rule cost two red cases in two suites once already. It now applies to **five** public writers, not two:

| Writer | What it may do | What it may never do |
|---|---|---|
| `OnHarassmentSuccess()` / `OnSabotageSuccess()` | `m_Instance.Report(key, 1)` | advance, reset, re-arm a timer |
| `SetObjectiveBagValue(key, value)` / `SetObjectiveBagPosition(key, v)` | write the bag | anything else at all |
| `OVT_ObjectiveInstance.Set` / `Report` / `SetPos` | write the map | anything else at all |
| `OVT_ObjectiveInstance.RecordPhase(index, name)` | **record** which phase is running | clone modules, arm the clock, push the anchor — those are `EnterObjectivePhase()`'s |
| `SetOperationCountdown` / `SetPhaseTimeout` | plant a countdown | change phase |

Every transition still happens on `DirectorTick()`, behind its three early returns, and **`EnterPhase()` is
still the one funnel** — which is exactly what makes the strangler safe, because the legacy gates and the
plan-driven advance both re-sync the instance through it without either knowing about the other. The legacy
condition shim's out-of-step WARNING is the tripwire if that ever stops being true.

### The load-order rule the version-2 serializer inherits, unchanged

`OVT_ObjectiveDirectorSerializer` still obeys both structural rules it was written with, and both are now
restated at more call sites because the record grew:

1. **NOTHING IN THE SERIALIZER OR IN `ApplyPersistedObjective()` TOUCHES THE RESOURCE POOL OR ANY DEPLOYMENT.**
   The deployment manager's restore **clears and refills** the faction resource pool and runs *after* the
   game-mode component serializers, so anything credited or debited here would be overwritten moments later;
   deployment entities are separately-tracked instances whose restore order relative to this payload is
   undefined. Everything that needs either is deferred to the director's **first tick**
   (`ResolveRestoredObjective()`), which is also where the forward base is re-linked by name+position.
   ⚠ This is why `DiscardPersistedObjective()` clears the FOB **record** and does **not** tear the deployment
   down, and why `AdoptPersistedPhase()` rebuilds the runtime module set but never enters the phase.
2. **`Deserialize` is a pure codec** making exactly one side-effecting call — into `ApplyPersistedObjective()`
   or `DiscardPersistedObjective()`. It reads *every* instance in the stream even when it can only adopt the
   first, because the format is positional and a skipped read would mis-align the blacklist that follows.

**Adopting a phase is not entering one.** `EnterPhase()` re-arms the idle clock and re-baselines the progress
marks; a restore has just put real values in both. `AdoptPersistedPhase()` therefore assigns the phase and
rebuilds the module set, and nothing else. A restored objective with **no** modules would silently fall through
to the strangler fallback on every tick, which is why the round-trip case asserts the module count.

### Version 2: three outcomes, and no migration (D2)

| Read | Meaning | Action |
|---|---|---|
| `version == 0` | absent payload — a pre-serializer save, or a read that failed | **keep live state, silently.** Unchanged contract |
| `version == 1` or anything unrecognised | a counter-attacks-format record, or a future one | **ERROR naming the version, discard, re-select.** ⚠ **Not one field is read** — the stream is positional and this build has never seen that format |
| `version == 2`, unknown plan or phase NAME | renamed or removed by a mod or a later build | **ERROR naming the missing plan/phase, abandon that objective, keep the blacklist** |

⚠ **The blacklist is restored first and survives every abandon path.** It is a fact about *places*, not about
the objective that was running; forgetting it would make one bad save also forget every place the campaign had
already decided to leave alone.

⚠ **`OVT_ObjectiveRecords.c`'s "NEVER RENUMBER THE ENUM" header is dead and was rewritten, not obeyed.** With
names in the payload the constraint has no consumer, and leaving it would have told the next reader to preserve
a format nothing reads — which was the only thing keeping the phase enum alive past Phase 6.

### 🔴 The Phase-2 gate found two reds, and both were ONE bug in the version-2 serializer

**Reported:** `…ObjectiveDirector_SurvivesSaveAndReapply` → "the restored objective is kind 0, not the town
that was saved"; `…ObjectiveFOB_RelinksItsDeployment` → "the forward base did not survive the round trip".

**Root cause: a save context's properties are keyed by the LOCAL VARIABLE'S NAME, not by position.** The
version-2 `Deserialize` read each instance's fields into `read`-prefixed locals — `readConfigName`,
`readTargetKind`, `readTargetPosition`, … — while `Serialize` had written them as `configName`, `targetKind`,
`targetPosition`. Every one of those reads found no such property and left its destination at zero. `targetKind`
came back as `0` = `NONE`, `ApplyPersistedObjective()` took its "no objective was saved" branch, cleared the
record **and the forward-base record with it**, and both symptoms follow from that single line. The one local
that *did* match — `instanceCount` — is why the payload looked present rather than absent.

**It is a production bug in T2.11, not a test fault.** No assertion was weakened: both cases were asserting
exactly the right thing and the format was wrong. The evidence chain, for the record: the run's `console.log`
carries **no** `DISCARDING the persisted objective` line and no `ended:` line, so the discard path never fired
and nothing tore the objective down — the payload simply restored as "nothing". The prefix was the only
difference between the working version-1 reader and the broken version-2 one; v1's locals matched its writer's
by accident of naming, which is why the trap survived unnoticed until a rewrite renamed them.

**It had already been measured once in this repository.** `OVT_JobManagerSerializer.DeserializeVersion2()`
carries the note verbatim: *"The local names ARE the property names… writing `jobRecords` and reading the same
payload into a local called `readJobs` FAILS"* (2026-08-09). The rule is now hoisted into this file's Gotchas as
#14 so the next serializer author meets it before writing one, rather than after.

**The fix, and why it is a format change rather than a rename.** A per-instance field loop is *unfixable* by
renaming: writing `configName` once per objective writes ONE property N times, and nothing on the way back in
could tell two objectives apart. Version 2 therefore carries **one array of `OVT_PersistedObjective` records**
under the name `objectives`, exactly as every other N-record payload in this epic already does
(`OVT_PersistedJobV2`, `OVT_PersistedBase`, `OVT_PersistedLoadoutItem`). Three further hardenings landed with
it:

- **Every `Read()` return is checked.** A failed read leaves its destination non-null and *empty*, and applying
  that would replace a running campaign's objective with nothing. `AbortUnreadablePayload()` now leaves the live
  objective untouched and says so at ERROR — the same rule and the same reasoning as the job serializer's, and
  it is what turns this whole class of fault from silent into diagnosable.
- **`assetUp` is `array<int>` of 0/1, never `array<bool>`.** Not one payload anywhere in this tree writes an
  array of booleans, so nothing establishes that the form round-trips. A save format is not the place to be the
  first caller of an untested shape.
- **`instanceCount` is gone.** The array carries its own count, so there is no second thing to keep in step.

⚠ **This corrects `implementation.md` §3.8**, whose field table describes a positional per-instance sequence
with an explicit `instanceCount`. That premise does not hold for this engine's save context. The *content* of
the record is unchanged — plan name, kind, position, phase name, both tick counters, both bags, the asset
arrays, then the blacklist pair — only its framing.

### Deviations from the plan, and why

- **No operation-cadence gate in the runner yet.** §3.2's sketch has `TickOperationCadence(instance)` gating
  operations on `nextOpTicks`. While a phase's only operation is the legacy shim, the cadence is owned by the
  hard-coded tick the shim calls — it advances the countdown, decides whether to spend, and re-arms, all of it
  verbatim. A runner-side gate on top would decrement the same counter twice **and** skip the very ticks the
  hard-coded gate check needs. The gate lands in **Phase 4**, with the first operation module that does not own
  one. Marked in `RunObjectivePhaseModules()`'s header.
- **`liveAtTickStart` is measured before the instance loop, not after.** An objective that ends *during* a tick
  must not be replaced in the same in-game minute: the phase switch this loop replaced could only take one
  branch, and the IDLE branch was not it. Reading the count afterwards would select on the tick the last
  objective died, and a first phase arms the operation countdown to **zero** — so the next tick would buy a
  real deployment with real resources, one in-game minute early, every time an objective ended.
- **The strangler fallback (`RunUnplannedObjectiveTick`) is not in the plan.** It exists because **no compiler
  reads a `.conf`**: a mistyped path or a prefab line that never saved would otherwise stop the occupying
  faction attacking with nothing but a log line to show for it. It costs one ERROR per objective and dies with
  the last shim. `OVT_TEST_Init_ObjectiveFramework_ARegistryResolvesAndValidates` is what turns that silent
  degradation into a red test.
- **`m_iSelectionCooldownTicks` and `m_iBlacklistRounds` were NOT added to the registry.** §3.10 puts them
  there and Phase 3 gives them readers; authoring a `.conf` field nothing consults is worse than a missing one,
  because a server owner tunes it, nothing changes, and the whole authored surface loses credibility. They
  arrive with their reader.
- **`m_iMaxConcurrentObjectives` is floored at 1** in `MaxConcurrentObjectives()`. Zero would stop the
  occupying faction ever choosing an objective again, silently. "Turn the director off" is not a supported
  authoring gesture.
- **The phase gains `m_iIdleTimeoutTicks`** (`-1` = the director's own `m_iPhaseTimeoutTicks`, which is what
  every phase shared before plans existed). §3.10 predicted this as a small capability gain; both shipped plans
  author `-1`, so pacing is unchanged.

### GUIDs authored (prefix `{6BA1…}`, verified free)

| GUID | What |
|---|---|
| `{6BA1000000000001}` | `Configs/Objective/overthrowObjectives.conf` |
| `{6BA1000000000002}` | `Configs/Objective/Objective_TownOffensive.conf` |
| `{6BA1000000000003}` | `Configs/Objective/Objective_BaseOffensive.conf` |
| `{6BA1000000000010}` | the registry instance on `Prefabs/GameMode/OVT_OverthrowGameMode.et:51` |
| `{6BA1000000000011}`–`{6BA1000000000012}` | the two plan entries inside the registry `.conf` |
| `{6BA1000000000020}`–`{6BA1000000000028}` | Town Offensive's phases and modules |
| `{6BA1000000000030}`–`{6BA1000000000038}` | Base Offensive's phases and modules |

### 🔴 What the Init tier CANNOT prove about this phase

The same shape of hole Phase 1 had, in a different place:

1. ✅ **Compile** — `tools/compile-check.sh` → 0 (6216 files), and every can-fail injection below also exited 0.
2. ✅ **The registry demonstrably loads** — `…_ARegistryResolvesAndValidates` **passed in the real client** on
   the Phase-2 gate run (2026-08-21), which is a stronger statement than the Workbench inspection it was
   standing in for: it proves the `.conf` resolves *and* that both plans carry their three named phases and
   their shim pairs *and* that the whole registry validates. A Workbench look is still worth having for the
   authoring experience, but the load itself is no longer an open risk.
3. ⏳ **Play-test** — the parity claim itself. Run a campaign far enough to watch the ramp: an objective is
   selected, harassment operations are sent on the difficulty cadence, the town gate fires, the forward base
   goes up, the counter-attack fires in daylight. The log must read exactly as it did before this phase, plus
   nothing. ⚠ **A `[Overthrow.ObjectiveDirector] Objective '…' is running with NO PLAN behind it` line means the
   registry did not load** — the campaign will still play, on the hard-coded fallback, so the log line is the
   only symptom.
4. ⏳ **Save/Continue** — take a save mid-ramp and continue: the objective must come back in the same phase, on
   the same plan, with the same counters. A version-1 dev save must produce one ERROR line and a fresh
   objective, not a crash.

### Can-fail proofs recorded this phase (all injected one at a time; every one exited `compile-check.sh` 0)

| # | Fault injected | Case it reddens |
|---|---|---|
| P1 | `authored > USE_DIFFICULTY` → `authored > 0` | `…PlanRules_DifficultyFallback…` |
| P2 | `ResolveWithDifficulty` returns `authored` unconditionally | same |
| P3 | drop the priority floor | `…PlanRules_PlanScore…` |
| P4 | `score * priority` → `score + priority` | same |
| P5 | drop the eligibility skip | `…PlanRules_SelectBestPlanIndex…` |
| P6 | tie comparison `<=` → `<` | same |
| P7 | `AllConditionsMet([])` → false | `…PlanRules_EmptyConditions…` |
| P8 | `AnyAbort([])` → true | same |
| P9 | `PhaseIndexOf` unknown → 0 | `…PlanRules_PhaseIndexOf…` |
| P10 | drop the empty-name guard | same |
| A | remove `m_Registry` from the game-mode prefab | `…Framework_ARegistryResolvesAndValidates` |
| B | remove the duplicate-phase-name rule | `…Framework_BValidatorNamesAndSkips…` |
| C | drop `clone.m_iLegacyPhase` (operation shim) | `…Framework_CLegacyOperationShimClones…` |
| D | drop `clone.m_iLegacyPhase` (condition shim) | `…Framework_DLegacyConditionShimClones…` |
| E | drop `instance.EnterRuntimePhase(authored)` | `…Framework_ECommitEntersPhaseZero…` |
| F | point `GetHarassmentSuccesses()` at the sabotage key | `…Framework_FCountersAreBagKeys…` |
| G | drop the next-phase upper bound | `…Framework_GPhaseAdvanceArithmetic…` |

### Notes for the orchestrator

- **`git diff Configs/Deployment/` is NOT empty in the working tree, and that is Phase 1's diff, not Phase 2's.**
  Phase 1's two lines (the repair `.conf.meta` `Name` and the registry path in `overthrowDeployments.conf`) are
  still uncommitted. Phase 2 added nothing to that folder — verified line by line.
- Same for `Scripts/Game/Controllers/OccupyingFaction/`: still exactly T1.3's two keyed-API call sites.
- **Nothing was committed.** All Phase-2 work is uncommitted in the working tree.
- The 44 existing objective Init cases were **not edited**. Their subjects still drive `CommitObjective`,
  `EnterPhase`, `SetPhaseTimeout`, `SetOperationCountdown`, `OnHarassmentSuccess`, `OnSabotageSuccess` and
  `DirectorTick`, all of which kept their signatures and their meanings — which is the parity gate's own claim,
  made structurally rather than by re-pointing 44 files.


---

## Phase 3 — plan-driven selection (2026-08-21)

### 🔴 THE EQUAL-PRIORITY PARITY ARGUMENT, WRITTEN OUT (T3.9)

This is the argument the whole phase rests on, and it is the least mechanically obvious claim in the
plan. **Before:** one list — every resistance-held town, then every resistance-held base — scored by
`OVT_ObjectiveSelection.ScoreTown` / `ScoreBase`, highest wins, ties to the earlier entry.
**After:** each plan's selector picks its own best candidate, that score is multiplied by the plan's
`m_fPriority`, and the plans are compared. The two give the same answer on the same map **because of
four properties, every one of which is now asserted rather than assumed**:

| # | Property | Where it is pinned |
|---|---|---|
| 1 | **One candidate collection, in the same order — every town, then every base**, each in its registry's own order. The collection is the only thing that looks at the world, and both forms read the same set. | `OVT_TEST_Init_ObjectiveDirector_PlanDrivenSelectionReproducesTheSingleListPick.AssertTownsBeforeBases()` — asserted where the order is *produced*, not where it is handed in |
| 2 | **The shipped selectors ARE those scorers, term for term and in the same order of addition.** The eight weights became attributes whose defaults are the constants; `OVT_ObjectiveSelection.c` was **not edited**. ⚠ Float addition is not associative, so `size + collapse + reach + coverage` and `prize + threat + reach + coverage` are reproduced in exactly that sequence. | Same case, half A: it compares the `.conf`-loaded selector's score against the untouched static's **per candidate**, not merely the argmax |
| 3 | **Equal priorities multiply by one, and `score * 1.0` is exact in binary floating point.** Both shipped plans author `m_fPriority 1`, so a plan's rank *is* its selector's score and every comparison is the comparison the single list made. | `OVT_TEST_Logic_ObjectiveScaling_PlanResolution_…`, first row |
| 4 | **The tie-breaks agree.** Within a plan `SelectBestIndex` keeps the FIRST candidate at a given score; between plans `SelectBestPlanIndex` keeps the FIRST plan at a given rank; and **the town plan is authored first in `overthrowObjectives.conf` exactly as towns were collected first in the single list.** So a town and a base of identical score still resolve to the town. | Same Init case: `AssertRegistryOrder()` plus arrangement 3, an **exact** float tie at 70.0 built from sixteenths and halves |

⚠ **The equivalence holds because the two shipped plans claim DISJOINT sources.** The town plan
claims towns, the base plan claims bases, so "best plan by its best candidate" and "best candidate
over one list" are the same argmax. **A registry whose plans OVERLAP is a supported thing to author
and is exactly where the two forms diverge** — and there the plan form is the intended one, because a
doctrine's priority is meant to be able to out-rank a slightly better target it has no doctrine for.
That divergence is a design decision, not a parity failure, and it is stated in `SelectObjective()`'s
own header so a future reader does not "fix" it back.

### The candidate-source flag table (T3.9)

`OVT_EObjectiveCandidateSource`, declared beside `OVT_ObjectiveCandidateSet`. **Append only, powers of
two** — the values travel in an authored `.conf` as a flag field, exactly as `OVT_FactionTypeFlag`
does, so renumbering a member silently re-points every authored selector at the wrong source.

| Flag | Value | What it collects | Deliberately NOT included | Declared by |
|---|---:|---|---|---|
| `RESISTANCE_TOWNS` | 1 | Every town and city held by the resistance faction | **Villages** — they fall as collateral and are never worth a campaign of their own | `OVT_ResistanceTownObjectiveSelector` |
| `RESISTANCE_BASES` | 2 | Every military base held by the resistance faction | **Forward bases** (the occupying faction's own) and **radio towers** (handled *within* an objective) | `OVT_ResistanceBaseObjectiveSelector` |

Three things the flag set does, and only the first is obvious:

1. **It is the collection budget.** The runner walks the **union** of every *eligible* plan's flags,
   once. A registry with no base doctrine never touches the base registry; a registry with ten town
   doctrines walks the town registry once. That is the whole economy of D6.
2. 🔴 **It is the per-plan selection MASK, and that is what keeps "scored zero" and "not a candidate"
   apart.** Every selector writes a score for *every* index so the parallel arrays stay aligned, and
   zero is a perfectly legal score — a distant base with no threat scores exactly `BASE_PRIZE_WEIGHT`,
   and a candidate could legitimately score 0. Without the mask a town selector's zero for a base
   would be *selectable*, and the town plan could commit to a base it has no phases for.
   `OVT_ObjectiveCandidateSet.BuildSelectionMask()` folds the blacklist and the source check together
   for exactly this reason.
3. **It is what `ResolvePlanForKind()` asks.** A commit that did not come from a selection round — a
   test fixture, a scripted scenario, a restore — resolves its plan by asking which plan's selector
   *declares it can describe that kind*, rather than by looking two plan names up. See the deletion
   note below.

**The exclusions are in the COLLECTION, not in a selector**, and that placement is deliberate: "the
world offers no villages as objectives" is a statement about the campaign, not about what a doctrine
values. A mod that wants a village doctrine changes the collection, and every plan sees it.

### What moved, and the one deviation from the plan's wording

The plan's Phase-3 text says the seven deleted director methods "all move into the two selectors".
**Five of them went to `OVT_ObjectiveCandidateSet` instead, and one rule beat the other:** T3.2 says
*the world queries happen in the candidate set and nowhere else*, which is the more specific
instruction and the one that buys D6's single pass.

| Deleted from the director | Now | Why there |
|---|---|---|
| `CollectTownCandidates` | `OVT_ObjectiveCandidateSet.AddResistanceTowns` (protected) | It is a world walk |
| `CollectBaseCandidates` | `…AddResistanceBases` | Same |
| `DistanceToNearestHeldBase` | `…ReachFromHeldGround` | Same. ⚠ Renamed because the acceptance grep is a substring match on the old name |
| `HasOccupyingTowerCoverage` | `…IsUnderOccupyingBroadcast` | Same, same reason |
| `ResolveBaseName` / `ResolveTownName` / `ResolveTownNameAt` | `OVT_ObjectiveCandidateSet` **statics**, names unchanged | 🔴 **A base with no marker name falls back to the nearest TOWN's name**, so name resolution is not town-doctrine work or base-doctrine work — it is shared. Duplicating `ResolveTownName`'s marker guard into two selectors would have been two copies of one crash-avoidance check, drifting apart. `ResolveObjectiveName()` (the restore path) calls the statics directly |
| **`ScoreTown` / `ScoreBase`'s weights** | The two selectors, as attributes | This *is* the selector's job |

**Also deleted this phase, per the strangler schedule in the Phase-2 section:**
`ResolveLegacyPlan(kind)` and the constants `LEGACY_TOWN_PLAN` / `LEGACY_BASE_PLAN`. They existed so a
freshly committed objective could look its plan up by *kind* while selection was one list. Selection
knows the plan now, so `CommitObjective()` **takes it** (a fourth parameter defaulting to `null`, so
all 44 existing Init call sites are untouched), and a commit that arrives from outside selection gets
`ResolvePlanForKind()` — which asks the registry rather than knowing two names.
⚠ `OVT_TEST_Init_ObjectiveFramework` read those two constants in six places and now asserts **literals**,
which is what its own header already demanded for the phase names: *a constant naming a constant passes
even when both sides are renamed together, which is the exact change that abandons every save on disk.*

### The runner's shape after this phase

```
SelectObjective():
    re-arm the selection cooldown                          (D6; shipped value 1 = no-op)
    collect the ELIGIBLE plans   validation -> faction -> instance cap -> m_fChance
    collect the candidate set ONCE, over the UNION of their sources
    apply the blacklist mask once, for every plan
    per plan:  selector.ScoreCandidates -> BuildSelectionMask -> SelectBestIndex -> ResolvePlanScore
    SelectBestPlanIndex                                     -> the winning plan and its candidate
    ServeBlacklistRound()                                   AFTER the pick, exactly as before
    LogSelectionRound()  (behind the campaign debug flag)
    LogSelection(plan, ...)  -> winner, runner-up, both scores, and now the PLAN NAME
    CommitObjective(kind, position, name, plan)
```

Three details worth keeping:

- **`m_fChance` is rolled LAST**, after the cheap gates. A plan that was never going to be eligible
  must not consume a random draw: the moment selection's answer depends on how many dice were thrown
  earlier, the path stops being reproducible from the map. Both shipped plans author `100`, and the
  roll short-circuits at 100 exactly as `OVT_DeploymentManager.c:1778` does, so **no shipped round
  touches the generator at all**.
- 🔴 **The instance cap excludes the instance the round would replace, and that is the difference
  between a cap and a deadlock.** A re-selection request runs a full round while an objective is still
  live — that is what "the map changed under us" means — and commits over the top of `m_Instance`.
  Counting that instance would make a plan at its cap ineligible for the very round that was about to
  free the slot, so a town changing hands would silently stop the town doctrine ever being chosen
  again. At the shipped concurrency of one objective the count is therefore always zero, which is the
  honest answer.
- **The fallback only fires when there is NO plan at all.** A registry that authors only a town
  doctrine, on a map where the resistance holds only bases, must select **nothing** — falling back
  would attack a base with a doctrine the author deliberately did not ship, which is precisely the
  "it quietly did something else" failure the authored surface exists to end. `plans.IsEmpty()` means
  the registry did not load, or every plan in it failed validation and was named at ERROR.

### The selection cadence (D6), and the one coupling it has

`OVT_ObjectiveRegistry.m_iSelectionCooldownTicks`, **default 1 = every idle in-game minute**, which is
what the campaign has always done. `SelectObjective()` re-arms the counter itself and
`IsSelectionDue()` serves it, so:

- **every** path that runs a round pays the cooldown for the ticks that follow — including a reselect,
  which is answered immediately (a map change is never made to wait) but does not then allow a second
  round on the next minute;
- at the shipped value the counter **never leaves zero**, which is what makes the whole mechanism a
  no-op by default. The Init case asserts exactly that, immediately after running a round itself.
- ⚠ **A blacklist round is served per SELECTION round, not per tick**, so raising the cooldown also
  slows how fast a failed objective works off its cooldown. That coupling is the shipped meaning of
  "round" and was left alone rather than given a second clock; the attribute's `desc:` says so.

`LogSelectionRound()` prints `N candidate(s) x M plan(s) in T ms` **behind the campaign's existing
`OVT_OverthrowConfigComponent.m_bDebugMode`** — no new authored flag. It runs once per in-game minute
in every live campaign, and the whole argument for collecting candidates once is that the cost is
invisible; a line per minute in a normal server's log would cost more than the thing it measures.
⚠ `System.GetTickCount()` is integer milliseconds, so a fast round honestly reads as `0 ms`. Tune the
cooldown from a play-test, never from a guess.

### GUIDs authored (prefix `{6BA1…}`, verified free before authoring)

| GUID | What |
|---|---|
| `{6BA1000000000029}` | the `OVT_ResistanceTownObjectiveSelector` inside `Objective_TownOffensive.conf` |
| `{6BA1000000000039}` | the `OVT_ResistanceBaseObjectiveSelector` inside `Objective_BaseOffensive.conf` |

### Can-fail proofs recorded this phase

**The phase's required proof (T3.8), executed:** `m_fBasePrizeWeight` was changed from **45 to 5** in
`Configs/Objective/Objective_BaseOffensive.conf`. `tools/compile-check.sh` exited **0** — *no compiler
reads a `.conf`*, which is the whole reason the case exists. The first arrangement then inverts, and
the inversion was computed independently in IEEE binary32 rather than assumed:

| Candidate | Single list (untouched statics) | Plan-driven, prize 45 | Plan-driven, prize 5 |
|---|---:|---:|---:|
| fixture town A | 47.5 | 47.5 | 47.5 |
| fixture town B | 21.0 | 21.0 | 21.0 |
| **fixture base A** | **60.0** | **60.0** | 20.0 |
| fixture base B | 45.0 | 45.0 | 5.0 |
| **pick** | **index 2** | **index 2** ✅ | **index 0** 🔴 |

The case then reports *"the plan-driven pick and the single-list pick chose DIFFERENT places -
'fixture town A' at rank 47.5 against 'fixture base A' at score 60"*, and the per-candidate score
comparison fires as well. The value was restored and the tree recompiled clean (exit 0).

⚠ **"It went red" here means arithmetic proof plus a compile-clean injection, not an observed suite
run** — running the suites is the orchestrator's job (`.claude/test-policy.md`), which is the same
standard Phase 2's seventeen proofs were recorded to.

Eleven further faults were injected one at a time, each compiled (**all exit 0**), each restored:

| # | Fault injected | Case it reddens |
|---|---|---|
| V2 | drop the "plan has no selector" validator rule | `…Framework_BValidatorNamesAndSkips…` ("Zeta", and the count falls to 5) |
| V3 | drop the "selector declares no sources" rule | same case, "Eta" |
| MASK | drop the candidate-source half of `BuildSelectionMask` | `…PlanDrivenSelectionReproducesTheSingleListPick` — "the town doctrine's selection mask left a BASE selectable" |
| BL | drop the blacklist half of `BuildSelectionMask` | same case — "with the best town sitting out its blacklist round the town doctrine must fall back to the next one" |
| M27 | `score + priority` in `ResolvePlanScore` | `…PlanResolution_PriorityMultipliesAndTiesByRegistryOrder`, first row (73.5 ≠ 72.5) |
| M28 | drop the priority floor | same case — "a negative multiplier is floored to zero, not honoured: got -60" |
| M29 | ties to the LAST plan in `SelectBestPlanIndex` | same case — "a tie between doctrines goes to the FIRST in registry order: got 2" |
| M30 | ignore eligibility in `SelectBestPlanIndex` | same case — the row is deliberately built with the INELIGIBLE plan scoring higher, or it would pass with the guard gone |
| COOL | `m_iSelectionCooldown = 7` in place of the re-arm | `…PlanDrivenSelection…` — "after one selection round the cooldown counter is at 7" |
| ORDER | collect bases before towns | same case — "the candidate collection listed a base before the town …" |
| PLAN | `CommitObjective` re-derives the plan by kind instead of taking the winner | 🔴 **NOT CAUGHT — see below** |

🔴 **The PLAN injection is an admitted coverage gap, recorded rather than hidden.** On the shipped
registry `ResolvePlanForKind(TOWN)` and "the plan that won" are the same object, so dropping the
hand-off changes nothing. It can only bite when **two plans claim one source** — which is exactly the
overlapping-registry case the parity argument above says is where the two forms legitimately diverge.
A third plan is out of scope for this feature (it is the modder exercise, not shipped content), so
there is nothing in-tree to author the fixture from. **Whoever ships a second town doctrine owes this
case.**

### 🔴 What the Init tier CANNOT prove about this phase

1. ✅ **Compile** — `tools/compile-check.sh` → 0 (6220 files), and every injection above also exited 0.
2. ⏳ **The two selector blocks actually load.** `Objective_TownOffensive.conf` and
   `Objective_BaseOffensive.conf` now author a polymorphic `m_Selector` sub-object with six weights
   each. If the block fails to resolve, the plan has **no selector**, the validator skips it, and the
   occupying faction falls through to the strangler fallback — which still plays, so the only symptom
   is a log line. `…Framework_ARegistryResolvesAndValidates` now asserts the selector is present *and*
   claims the right source, which converts that into a red test — but it still wants a Workbench look
   at the authoring experience.
3. ⏳ **Play-test the ramp.** The selection line changed shape: it now reads
   `Objective: 'Town Offensive' on town 'X' at score N, ahead of 'Y' at M`. A line reading
   `Objective: NO PLAN (the registry did not load) on …` means the registry is not resolving.
4. ⏳ **The debug line.** With `m_bDebugMode` on, every idle minute should print
   `Selection round: N candidate(s) x 2 plan(s) in T ms`. That is the number D6 says to tune the
   cooldown from, and nobody has seen it yet.

### Notes for the orchestrator

- **Nothing was committed.** All Phase-3 work is uncommitted, on top of Phases 1 and 2.
- `git diff Configs/Deployment/` is **still Phase 1's two lines and nothing else** — verified line by
  line. `Scripts/Game/GameMode/Deployments/`, `Configs/Difficulty/`, `Virtualization/`,
  `VirtualMovement/` and `api.md` are all **empty**. `Scripts/Game/Controllers/OccupyingFaction/` is
  still exactly T1.3's two keyed-API call sites.
- `Scripts/Game/GameMode/Objectives/OVT_ObjectiveSelection.c` is **byte-unchanged** — the strongest
  form of the "the eight constants keep their values" acceptance check.
- The 44 existing objective Init cases were again **not edited**, bar one comment: the determinism
  case's can-fail preamble named `CollectTownCandidates`, which no longer exists, and now names
  `OVT_ObjectiveCandidateSet.AddResistanceTowns`. **No assertion changed meaning.**
- ⚠ **THE DIRECTOR GREW THIS PHASE: 6,181 → 6,451 lines (+270), and that is worth stating rather than
  glossing.** ~225 lines of collection and name resolution left; ~495 arrived. What arrived is not
  doctrine — "what a town is worth" is now entirely in the two selectors and the `.conf` — it is
  **runner** work, and §3.2 is explicit that comparing candidates and committing one is the runner's
  job. Roughly 150 of those lines are the parity argument written out in `SelectObjective()`'s own
  header (D11: the reasoning moves with the code), and ~75 are `SelectWithoutAPlan()`, which is
  strangler scaffolding that **dies in Phase 6** with `RunUnplannedObjectiveTick()`. The plan's
  2,000–2,200-line end state is a Phase-6 measurement and the large deletions — 1,580 lines of forward
  base, 803 of operations — are all still ahead, so this is not a drift on that target.
- **New EnforceScript facts learned here, hoisted to the Gotchas list:** `reference` is a **reserved
  identifier** (#16, and it produces two *different* and equally misleading compile errors), and a
  `.conf` **can** author a `protected` `[Attribute]` field (#17, precedent found in
  `Configs/Map/OverthrowMap.conf`).

---

---

## Phase 4 — the harassment phase in config (2026-08-21)

### 🔴 THE OPERATION-ORDER CONTRACT, AND WHERE IT IS WRITTEN DOWN (T4.7, T4.11)

**`.conf` module order IS evaluation order, and a `.conf` cannot carry a comment saying so.** Both
shipped plans author their harassment phase as three `OVT_SendDeploymentObjectiveOperation`s in this
order and nothing else may reorder them:

```
1. tower recapture        m_sConfigName "Objective Tower Recapture"     resolver: EnemyTowersAffecting
2. the harassment ladder  m_aLadder[4]                                  resolver: ObjectiveSelf
3. sabotage               m_sConfigName "Objective Sabotage"            resolver: ObjectiveSelf (+ base guard)
```

That is `SendTowerRecaptureOperation() || SendHarassmentOperation() || SendSabotageOperation()` — the
hard-coded chain — term for term. **The first module that acts consumes the cadence**, so the order is
not cosmetic: it decides which operation gets the interval on a tick when more than one could act.

**Why tower recapture is first**, ported from the sender's own header: it is the most urgent and the
most bounded of the three — a tower is a discrete thing that is either being worked on or is not, the
module deduplicates against the live deployment, and a tower left in resistance hands keeps the
objective easier for the resistance to hold. The other two have no ceiling and will still be there next
interval.

**Why authoring all three in BOTH plans is exact parity, not padding.** `SendNextOperation()`'s own
header says it: *"HARASSMENT AND SABOTAGE ARE MUTUALLY EXCLUSIVE BY OBJECTIVE KIND, not by priority…
so the order they are asked in here is arbitrary and only one of them can ever answer."* The kind fork
lived on each sender's FIRST LINE, and it is now `m_iRequiredTargetKind` on the module (TOWN on the
ladder, BASE on sabotage, 0 = any on tower recapture). Both plans therefore carry the identical chain
and differ only in their **conditions**, which is the honest difference.

⚠ **The contract is stated in three places and nowhere else**: `OVT_ObjectivePhase`'s class header,
`OVT_SendDeploymentObjectiveOperation`'s class header, and
`OVT_TEST_Init_ObjectiveModules_HarassmentPhaseAuthorsTheShippedChain`, which is the only thing that
turns a reordered `.conf` into a red test.

### 🔴 THE WORLD-FACT CONJUNCT, AND WHY IT LIVES IN THE MODULE (T4.3, T4.11)

`OVT_SupportBelowObjectiveCondition` is **two conjuncts**, not one:

1. the town's support is strictly below the threshold (`m_iSupportThreshold`, `-1` = the ported
   `OVT_ObjectivePhaseRules.TownPhase2Gate`, 50 %), and
2. **the town is currently carrying the modifier THIS ramp applies** (`m_sRequiredTownModifier`,
   authored `"ObjectiveHarassment"`).

**Without (2) the gate fires on the phase's own entry tick** for any town already under half, and the
entire harassment ramp is skipped. A collapsed town is already rewarded by SELECTION, which scores low
support heavily — the prize for being collapsed is being *chosen*, not being allowed to skip a phase.

**Three reasons it is in the module and not in `OVT_ObjectivePhaseRules.TownPhase2Gate`:**

- **The pure tier may not ask it.** `TownPhase2Gate` is a function of its arguments and nothing else;
  "did this ramp do it" is a question about the world (the town manager, the modifier system, the
  town's replicated modifier list). Folding it in would break the tier's one rule.
- **Its signature is pinned by Logic cases** on both sides of the threshold, and changing a settled
  contract to fix a different layer's mistake is how a shared static becomes unmaintainable.
- **It is authored data now.** A modder writing a pure support gate authors an empty
  `m_sRequiredTownModifier`; a modder writing a doctrine driven by some other debuff authors that
  name. Neither is expressible in a static.

⚠ **The success COUNTER cannot do this job and was tried.** A counter is a plain integer any caller may
raise, so it records that operations were *reported*, not that anything happened in the world — a
fixture bumping it three times satisfies a counter test while the town it names has never been touched.
The modifier is the causal link itself, and it also survives a save correctly (town modifiers are
persisted) where a session-local "have I sent one yet" flag would not.

⚠ **Two things now have to author the name, and both are asserted.**
`OVT_TEST_Init_ObjectiveModules_TownGateNeedsTheRampsOwnDebuff` drives the module (refuses without,
passes with, on a real town whose support it plants and restores), and
`…_HarassmentPhaseAuthorsTheShippedChain.CheckGate()` asserts the shipped `.conf` still says
`"ObjectiveHarassment"` — because clearing that one line in `Objective_TownOffensive.conf` disables the
conjunct, compiles clean, and has no other symptom.

### The difficulty-convention flip, and the TWO modules it deliberately excludes (T4.6, T4.11)

**Flipped:** `OVT_TownHarassmentBehaviorDeploymentModule.m_iHoldSeconds`,
`OVT_BaseSabotageBehaviorDeploymentModule.m_iHoldSeconds` and `.m_iStructuresPerMission`.

| | Before | After |
|---|---|---|
| Meaning of an authored positive number | **IGNORED** whenever difficulty settings are loaded | **HONOURED** — it overrides difficulty |
| Meaning of `-1` | (not a thing) | "use the campaign's difficulty setting" |
| What the shipped `.conf` authors | the difficulty value, restated | **`-1`**, re-authored in the same change |

The old convention meant a server owner could tune a `.conf` field and nothing would happen — which is
worse than a missing field, because the whole authored surface loses its credibility with it. The flip
is **behaviour-neutral only because the configs were re-authored to `-1` in the same pass**, and that
is now a red test: `OVT_TEST_Init_ObjectiveSabotage_CCloneFidelityAndDifficultyPrecedence` gained a
third half asserting the shipped sabotage config authors the sentinel, and
`OVT_TEST_Init_ObjectiveModules_HarassmentConfigAsksForDifficulty` asserts it for **all four** ladder
rungs (each is a separate registry entry and a delta could re-author one alone).

🔴 **`OVT_TowerRecaptureBehaviorDeploymentModule` was NOT flipped, and that is a deviation from
implementation.md §3.10's "three objective-side modules" — recorded, not glossed.** It is authored in
**two** configs and only one of them is objective doctrine:

- `Deployment_ObjectiveTowerRecapture.conf` — the ramp's recapture operation (this feature's), and
- `Deployment_TowerRecaptureUnrest.conf` — the standalone unrest response, which the phase's frozen-file
  rule requires to stay **byte-identical**.

Flipping the convention while leaving the second config holding its authored `600` would **honour** that
600 instead of overriding it: the unrest recapture would take 600 s on Easy (the campaign says 900) and
600 s on Insane (it says 300). Re-authoring that config instead is the same statement made in a file
this phase may not touch. So the exclusion is forced, it is the *same* exclusion and the *same* reason
that already applies to `OVT_BaseRepairBehaviorDeploymentModule` (a pure relocation whose behaviour must
not change), and it is written out at the attribute itself so nobody "tidies" it later. **Whoever
separates the two configs can flip it in one commit with both.**

### The deployment phase-span is two NAMES now, not two enum integers (T4.5)

`OVT_ObjectiveConditionDeploymentModule.m_iRequiredPhase` / `m_iThroughPhase` →
**`m_sFromPhase` / `m_sThroughPhase`**, resolved to plan-phase indices through
`OVT_ObjectiveConfig.IndexOfPhase()` and compared by a new pure static
`OVT_ObjectivePlanRules.PhaseIndexInRange()`.

Both protections were carried across **verbatim**, in string space:

- **`EffectiveThroughPhase()`** replaces `ResolveThroughPhase()`. An **empty** `m_sThroughPhase` — which
  is both the attribute default and what an unauthored field holds — means "the first phase only",
  exactly as `0` did. The collapse is stated in ONE place and the live predicate reads it from there,
  so a reader and the machine cannot disagree.
- **The range is not an equality test**, and regressing it to one restores the 2026-08-19 deadlock: a
  base objective is promoted on its FIRST sabotage mission and needs up to six, so a ramp scoped to the
  ramp phase alone can never reach the counter-attack.

🔴 **An unresolvable span now REFUSES, and it says so once, at ERROR.** Three things make it
unresolvable — no plan behind the objective, a phase name the plan does not carry (a rename made in one
file and not the other), or an objective that has not entered a phase. All three mean the deployment
cannot say whether it belongs, and a condition that cannot answer must refuse. **The cost is real**:
every deployment of that config is collected on its next reinforcement check. That is exactly why it
logs, once per module, naming the plan and the missing phase — the alternative is a campaign that
quietly stops garrisoning its objective with nothing in the log at all.

⚠ Re-authored: all five `Deployment_Objective*.conf`. The config **name** does not move, so nothing
orphans. `Deployment_TowerRecaptureUnrest.conf` and the thirteen non-objective configs are untouched.

### The runner grew the cadence gate and the idle clock, and the abort fold runs TWICE

`RunObjectivePhaseModules()` now brackets the modules:

```
if (the phase is NOT a legacy shim):
    m_bBlockedOnAffordability = false
    AdvanceOperationCadence()
Tick() every module
aborts   (OR)   -> ResetObjective, return
conditions (AND) -> advance, return              <- a tick that advances runs no operation
if (the phase IS a legacy shim):  operations, return   <- the shim owns its own cadence and clock
cadence = ResolveOperationCadence(instance)
if (cadence >= 0 && nextOpTicks == 0):  created = operations
if (created):   SetOperationCountdown(cadence)
if (TickObjectiveIdleClock(created)):  aborts (OR) AGAIN
```

🔴 **The second abort fold is what keeps the timing EXACT, and it is not belt-and-braces.** The
hard-coded handlers ended with `if (TickObjectiveIdleClock(created)) ResetObjective(...)` — the give-up
verdict was read AFTER the operations, on the same tick the clock ran out. An abort module asked only at
the top of the tick can only ever see the PREVIOUS tick's clock, so the objective would be abandoned one
in-game minute late **and would get one more operation attempt it never used to get**. Asking again
after the clock is served costs nothing, because `ShouldAbort()` is side-effect free by contract.

⚠ **A legacy-shim phase is detected by a type cast (`PhaseOwnsItsOwnClocks`), not by a virtual on the
module base.** Declaring "do you own the clocks?" on `OVT_BaseObjectiveOperationModule` would put a
permanent question into the modder-facing contract to describe a temporary condition, and every module
anyone ever writes would have to answer it. One cast dies with one grep in Phase 6.

⚠ **An UNRESOLVABLE cadence stops the phase spending, and that is the shipped behaviour.**
`SendNextOperation()` opened with `if (!difficulty) return false;`. `NO_CADENCE` (-1) reproduces it.
An authored **zero** is a legal, if aggressive, gesture ("one operation every in-game minute") and is
honoured; only the sentinel with no difficulty behind it refuses.

🔴 **A phase with no `OVT_IdleForObjectiveAbort` can no longer time out at all.** Before the doctrine
was authored data, EVERY phase timed out because the give-up was hard-coded into each handler. The price
of making it authorable is that an author can leave it out — supported for a TERMINAL phase, a mistake
anywhere else, and **the validator deliberately does not catch it** because it cannot tell the two
apart. The shipped plans are pinned by an Init case instead. ⚠ Phase 5 and Phase 6 must each author one
when they replace their shim.

### `SendRampOperation()` — a NEW temporary bridge, not in the plan, and why it had to exist

🔴 **`SendNextFOBOperation()` chained the three deleted senders.** "Phase 1 operations continue into the
forward-base phase" is the 2026-08-19 deadlock fix and is load-bearing: a base objective is promoted on
its first sabotage mission and needs up to six, and a town's stacking debuff is applied by harassment
operations. Deleting the senders in Phase 4 while the forward-base phase is still hard-coded would have
restored that deadlock exactly.

`SendRampOperation()` **asks the plan** — it borrows the objective's FIRST phase's operation modules,
clones them, initialises them against the live instance, tries them in authored order, exits them, and
answers whether one acted. It is deleted in **Phase 5**, with `SendNextFOBOperation()` itself.

⚠ **It asks the plan rather than re-deriving anything, and that is the whole point.** The three senders
it replaced *were* the methods the harassment phase used, so the continuation is only faithful if it
runs the very modules the harassment phase now uses — same order, same configs, same resolvers, same
caps, same ladder rung. It also means the forward-base phase now exercises the new modules in play,
which is free parity evidence.

⚠ The clones are per-call and dropped again: a phase's authored modules are TEMPLATES and are never run,
and these are being *borrowed* by a phase that is not theirs. Three clones once per in-game minute, in
one phase, only on a tick whose cadence has already elapsed.

### `ReportObjectiveProgress(key, delta)` replaces `OnHarassmentSuccess()` / `OnSabotageSuccess()`

One public counter instead of two named ones, carrying the same header and the same rule: **it counts,
it does not decide.** The signal is still PULLED by the tick — `ConsumeReportedOperations()` compares the
bag against `m_iProgress*Mark` — and a report that advanced a phase or re-armed a timer would put a
transition somewhere other than behind the tick's three early returns. That cost two red cases in two
suites once already and the generalisation states the rule at exactly one public writer instead of two.

The two deployment-side reporters now call
`director.ReportObjectiveProgress(OVT_ObjectiveInstance.BAG_HARASSMENT_SUCCESSES, 1)` and its sabotage
twin; the six test call sites were re-pointed mechanically and **no assertion changed meaning**.

### The constants that did NOT move, and why

`HARASSMENT_LADDER`, `TOWER_RECAPTURE_CONFIG`, `SABOTAGE_CONFIG`, `FOB_CONFIG` and `HARASSMENT_MODIFIER`
are still on the director, even though the modules now author the same names in `.conf`.

🔴 **Because `IsObjectiveOperationConfig()` reads them, and it is a KEPT method.** It answers two
questions at once — "is this operation still in flight?" (which holds the idle clock for a team that is
still walking) and "is tearing this down a RECALL rather than a write-off?" (which decides the refund) —
and it does so for deployments this director created in an EARLIER phase. Re-pointing it at the running
phase's modules would stop a harassment team sent in phase 1 counting while the objective is in phase 2,
which is precisely the case the ramp continuation exists for.

⚠ **So there are two lists of the same names, and drift between them is silent.** A plan sending a
config the director does not recognise would buy men whose walk does not hold the clock and whose recall
pays nothing back. `…_HarassmentPhaseAuthorsTheShippedChain` compares the authored ladder against the
director's constant **rung for rung and in order**, and the tower and sabotage names against theirs.
The duplication ends in Phase 6, when the enum and the last shim go.

### GUIDs authored (prefix `{6BA1…}`, verified free before authoring)

| GUID range | What |
|---|---|
| `{6BA1000000000040}`–`{6BA1000000000048}` | Town Offensive's harassment-phase modules and their two resolvers |
| `{6BA1000000000050}`–`{6BA1000000000058}` | Base Offensive's, same shape |

The two shim GUIDs each plan's harassment phase used (`…0021`/`…0022`, `…0031`/`…0032`) are retired
rather than reused, so a reader diffing the file never sees a GUID change meaning.

### Can-fail proofs recorded this phase (injected one at a time; every one exited `compile-check.sh` 0, every one restored)

| # | Fault injected | Case it reddens |
|---|---|---|
| P1 | drop `clone.m_sRequiredTownModifier` | `…ObjectiveModules_SupportBelowCloneCarriesEveryAttribute` |
| P2 | `m_sRequiredTownModifier ""` in `Objective_TownOffensive.conf` | `…ObjectiveModules_HarassmentPhaseAuthorsTheShippedChain` (CheckGate) |
| P3 | move tower recapture out of first place in the `.conf` | same case — "does not send tower recapture FIRST" |
| P4 | rename one ladder rung in `Objective_BaseOffensive.conf` only | same case — rung mismatch **and** the registry lookup |
| P5 | drop `clone.m_iMaxConcurrent` | `…ObjectiveModules_SendDeploymentCloneCarriesEveryAttribute` |
| P6 | `m_iStructuresPerMission 2` back in `Deployment_ObjectiveSabotage.conf` | `…ObjectiveSabotage_CCloneFidelityAndDifficultyPrecedence` (third half) |
| P7 | drop the own-faction skip in the tower resolver | `…ObjectiveModules_TowerResolverSkipsOursAndOffersTheRest` |
| P8 | the ladder ignores the bag counter | `…ObjectiveModules_LadderRungFollowsTheBagAndSaturates` |
| P9 | drop the runner's `nextOpTicks == 0` cadence gate | `OVT_TEST_Init_ObjectiveReserve` — "the floor must lapse on the first tick that does not ask" |
| P10 | a successful create no longer re-arms the cadence | `OVT_TEST_Init_ObjectiveDirector_RecallsWhatItSent` (**assertion added this phase**) |

⚠ **"It went red" here means a reasoned trace plus a compile-clean injection, not an observed suite
run** — running the suites is the orchestrator's job (`.claude/test-policy.md`), the same standard
Phases 2 and 3 recorded their proofs to.

### 🔴 What the Init tier CANNOT prove about this phase

1. ✅ **Compile** — `tools/compile-check.sh` → 0 (6230 files), and every injection above also exited 0.
2. ⏳ **The two plan `.conf`s still load.** Each harassment phase now authors six modules and two
   polymorphic resolver sub-objects, and `m_aLadder` is **the first array-of-strings this mod authors in
   its own configs** (the shape was verified against vanilla `ExplosiveCharge_base.et`'s
   `m_aIgnoredComponents`, which authors one the same way). If a block fails to resolve, the phase runs
   with fewer modules and the ramp goes quiet.
   `…_HarassmentPhaseAuthorsTheShippedChain` turns every part of that into a red test **except** a total
   failure to load the plan, which `…Framework_ARegistryResolvesAndValidates` already covers.
3. ⏳ **THE DEDUP-THEN-NEXT-CANDIDATE WALK IS NOT DRIVEN.** The resolver's many-answer and its order ARE
   asserted, and the tower module's dedup radius is asserted non-zero — but "a second call at a position
   that already carries a team returns the NEXT candidate" needs a live deployment standing at a tower,
   which means creating one with real resources in the shared world. **Play-test it**: an objective
   covered by two resistance-held towers must send a recapture team to the second one on a later
   interval, not stop after the first.
4. ⏳ **Play-test the ramp end to end.** Harassment operations on the difficulty cadence, one per
   interval, escalating a rung per completed operation; the town gate firing only after the debuff has
   landed; sabotage at a base objective; the ramp continuing into the forward-base phase. The log must
   read exactly as it did before this phase, plus nothing.
5. ⏳ **Save/Continue mid-ramp.** The bag counters and the phase name round-trip (Phase 2's format is
   unchanged), but the deployment-side phase span is now resolved by NAME through the running plan — a
   restored objective whose plan resolved must still keep its ramp deployments rather than collecting
   them.

### Deviations from the plan, and why

- 🔴 **`OVT_TowerRecaptureBehaviorDeploymentModule` is excluded from the difficulty flip** — see above.
  §3.10's "three objective-side modules" becomes two modules and three attributes.
- **`OVT_SendDeploymentObjectiveOperation` gained `m_iRequiredTargetKind`, `m_fConcurrencyRadius` and a
  `-1`/`0` split on `m_iMaxConcurrent`**, none of which §4's T4.2 attribute list names. Each is the
  verbatim port of something that was inline in a sender: the "towns only" / "bases only" first line,
  the two different concurrency radii (800 m harassment / 300 m sabotage), and tower recapture having
  **no** concurrency cap at all (it bounds itself by deduplicating per tower). Without them the three
  senders could not be one module.
- **`OVT_ObjectiveSelfTargetResolver` gained `m_fRequireEnemyHeldBaseWithin`** rather than a fifth
  resolver class. It carries the sabotage sender's other two refusals — "the objective position must
  still be a real base" (100 m) and "somebody took it back" — which are statements about the
  DESTINATION, which is this seam's own question. It also answers the BASE's position, as the sender
  created at `base.location`.
- **`SendRampOperation()` is a new temporary bridge** — see above. It is scheduled to die in Phase 5;
  if Phase 5 deletes `SendNextFOBOperation()` and not this, `grep -rn "SendRampOperation" Scripts/`
  must not be empty by accident.
- **`ReportObjectiveProgress()` replaces the two named counters** rather than the modules reaching the
  instance directly. The deployment-side modules only ever hold a director handle, and the director's
  method is where the "counts, does not decide" contract is written down.
- **`ResolveObjectiveSupportPercentage()`'s body was INLINED into `MeetsCounterAttackRamp()`**, its one
  remaining caller, rather than kept as a helper. The forward-base gate's copy became
  `OVT_SupportBelowObjectiveCondition`; this half follows it into a module in Phase 6.
- **One assertion was ADDED to an existing parity case** (`…_RecallsWhatItSent`): a tick that created and
  paid for an operation must re-arm the operation cadence. Nothing was weakened — the re-arm moved from
  inside a sender to the runner this phase and had no case at all.
- **`OVT_TEST_Init_ObjectiveInsertion.c` needed no edit.** §4 lists it as "4 cases, subject only"; none
  of its four references anything this phase changed, and it compiles unedited.

### Notes for the orchestrator

- **Nothing was committed.** All Phase-4 work is uncommitted, on top of Phases 1–3.
- `git diff Configs/Deployment/` is Phase 1's two lines **plus** the five `Deployment_Objective*.conf`
  files T4.5/T4.6 re-authored, and nothing else — verified file by file.
  `Deployment_TowerRecaptureUnrest.conf` and the thirteen non-objective configs are **byte-identical**.
- `Configs/Difficulty/`, `Scripts/Game/GameMode/Deployments/`, `Virtualization/`, `VirtualMovement/` and
  `api.md` are all **empty of diff**. `Scripts/Game/Controllers/OccupyingFaction/` is still exactly
  T1.3's two keyed-API call sites. `tools/` carries only the orchestrator's own `run-tests.sh` change.
- **The director SHRANK this phase: 6,451 → 6,212 lines (-239).** 495 lines of doctrine deleted, ~256
  added — of which ~90 is `SendRampOperation()` and `PhaseOwnsItsOwnClocks()` (both die in Phases 5–6)
  and the rest is the runner's cadence/idle-clock bracket and its reasoning. The plan's 2,000–2,200-line
  end state is still a Phase-6 measurement; the two big deletions (1,580 lines of forward base, the
  battle block) are ahead.
- **`grep -rn "OVT_LegacyPhase" Configs/`** now returns only the FOB and counter-attack phases —
  `m_iLegacyPhase 1` is gone from both plans, as T4.8 required.


---

## Phase 4 gate — All `{6A6E2A002F53A581}`: **427 / 431**, 3 real reds, all in Phase 2's framework case file (2026-08-21)

`compile-check.sh` 0 and every static acceptance grep and diff verified clean on both sides. The fourth
red is the pre-existing `CompositionSlotGate` leftover. **All three reds were in
`OVT_TEST_Init_ObjectiveFramework.c` and none of them was a Phase-4 defect** — two were assertions that
encoded the Phase-2 shim shape T4.7 deliberately replaced, and one was a fixture that a new T4.9 rule
correctly rejected. Each was repaired by making the assertion *stronger*, never by relaxing it.

### 🔴 RED 3 (the one that looked like a real defect) — `…_ECommitEntersPhaseZeroAndEveryEntrySyncsTheInstance`

> *committing an objective left 6 runtime module(s) from the phase before it*

**Verdict: STALE TEST, not a defect.** Six is exactly the harassment phase's new module count, and the
failure was on the **first commit** — which has no phase before it. The case carried a hard-coded
`modules = 2` (the shim pair) and passed it to an `AssertState()` that phrased *every* count mismatch as
"leftovers".

**Verified by inspection, not by adjusting the number.** `OVT_ObjectiveInstance.EnterRuntimePhase()`
calls `ExitRuntimePhase()` first — which `Exit()`s every outgoing module and `Clear()`s the array — and
`OVT_ObjectivePhase.CloneModules()` `Clear()`s the destination again before inserting. The
"`Exit()` outgoing, clone incoming, `Initialize()` incoming" contract of §3.3/T2.7 is intact, which
matters because Phases 5 and 6 both depend on it.

**The repair makes the assertion strictly stronger.** The expectation is now DERIVED from the plan the
objective is running and compared **module by module, in order**:

| Now asserted | Why the old count could not |
|---|---|
| runtime count == the authored phase's count | — |
| runtime module *type* == authored type, per position | A count cannot catch a failed swap between two phases of the **same size**, which `ForwardBase` → `CounterAttack` (2 and 2) exactly is |
| runtime `m_sModuleName` == authored name, per position | same |
| runtime module is **not** the config's own template object | Two objectives entering one phase would share one module and one set of latches |
| every runtime module has been `Initialize()`d | An un-initialised module refuses everything it is asked |

⚠ **It can never go stale again**, because re-authoring a doctrine moves both sides together — and the
ORDER half is now a second enforcement point for the `tower || harassment || sabotage` contract.

### RED 1 — `…_ARegistryResolvesAndValidates`

> *Plan 'Town Offensive' phase 'Harassment' carries the wrong number of modules - the strangler authors
> exactly one condition shim and one operation shim per phase*

**Verdict: STALE TEST.** `EXPECTED_MODULES = 2` and the shim-pair assertion were applied to all three
phases; T4.7/T4.8 replaced the harassment phase's pair with six real modules.

**Repaired by splitting the assertion along the strangler's own seam**, which is what the file is
documenting anyway:

- `AssertPhaseHeader()` — name, position, and the two tuning sentinels. True of **every** shipped phase,
  doctrine or shim, and it now also refuses an empty module bag outright.
- `AssertHarassmentPhase()` — the real doctrine: **exactly three** send-deployment operations, the
  **ladder second** (the shipped chain is tower recapture → ladder → sabotage, and `.conf` order is
  evaluation order), at least one advance condition, at least one abort, and **no legacy shim** (a
  harassment phase that still carried one would call a switch case that has not existed since T4.8, so
  the phase would do nothing at all behind one ERROR line).
- `AssertShimPair()` — ⚠⚠ TEMPORARY. `ForwardBase` and `CounterAttack` only. **One line of the caller
  moves from the shim list to the real list per build phase**, and the method dies with the last shim.

### RED 2 — `…_BValidatorNamesAndSkipsABrokenPlan`

> *The one wholly valid plan was skipped: one broken plan in a mod must never stop the rest of the
> registry running*

**Which rule fired: T4.9's WEDGE RULE** — "a phase with no advance condition and no terminal operation".
`MakePhase()` built a phase with an **empty `m_aModules` array**, and "Alpha", the fixture's *valid
control*, was built from it.

🔴 **Verdict: THE RULE IS RIGHT AND THE FIXTURE WAS UNDER-AUTHORED.** A phase with no modules genuinely
can neither advance (nothing gates it) nor end (nothing terminal acts) — it is a plan that can be
committed to and then never do anything, which is the exact failure mode with no symptom a player could
report. Alpha was not "wholly valid" under the Phase-4 rule set; it was a wedge that had never been
noticed because nothing checked. **The rule was not touched.** `MakePhase()` now builds the minimum
legal bag — one advance condition and one idle abort — which is also the smallest thing a modder could
honestly ship.

**And the case grew four rows, because its own header says the rules grow with the feature:**

| Fixture plan | Rule it must trip | Manager needed? |
|---|---|---|
| `Theta` | the **wedge** — an operation, but no condition and nothing terminal | no |
| `Iota` | a send-deployment operation with **no `m_Resolver`** | no |
| `Kappa` | a send authoring **neither `m_sConfigName` nor `m_aLadder`** | no |
| `Lambda` | a send naming a deployment config **the registry does not carry** | **yes** |

⚠ Each broken fixture is broken in EXACTLY ONE WAY — `Theta`'s operation names a real registered config
and has a resolver, and `Iota`/`Kappa`/`Lambda` all carry an advance condition — so the intended rule is
the only one that can fire and the case cannot pass for the wrong reason.

⚠ **`Lambda`'s expectation is conditional on the deployment framework resolving**, because the
name-resolution rule deliberately SKIPS when there is no framework to ask (a validator that turned "I
could not check" into "this plan is broken" would skip both shipped doctrines in a world without one).
The skipped count is therefore 9 without a manager and 10 with one, and the case computes which.

### Can-fail proofs for the three repairs (each injected alone, each compiled exit 0, each restored)

| # | Fault injected | Case it reddens |
|---|---|---|
| P11 | `EnterObjectivePhase()` never calls `instance.EnterRuntimePhase(authored)` | `…Framework_E` — 0 runtime modules against 6 authored, then 0 against 2 |
| P12 | `CloneModules()` inserts the config's own `module` instead of `clone` | `…Framework_E` — "put the CONFIG'S OWN TEMPLATE object into the runtime set" |
| P13 | the wedge rule removed from `ValidatePhase()` | `…Framework_B` — "a phase that can neither ADVANCE nor END must be skipped" |
| P14 | the no-resolver rule removed from `ValidateSendOperation()` | `…Framework_B` — "an operation with NO RESOLVER must be skipped" |
| P15 | the ladder moved out of second place in `Objective_TownOffensive.conf` | `…Framework_A` (`AssertHarassmentPhase`) **and** `…ObjectiveModules_HarassmentPhaseAuthorsTheShippedChain` |

⚠ **P12 is the proof that matters for the strangler.** Phases 5 and 6 both rebuild a phase's module set,
and "the runtime set is clones, never the config's own objects" had no assertion anywhere before this
repair — a count could never have seen it.


---

## Session Notes

### 2026-08-21 — Phase 6 built
- T6.1–T6.7. `OVT_StartBattleObjectiveOperation` (terminal, polled, `m_eMode COUNTER_ATTACK`), the battle phase
  authored in both plans with an `OVT_IdleForObjectiveAbort` beside it, the per-phase anchor radius moved into the
  plans, **both shim classes deleted** with `RunLegacyPhaseTick()`, `PhaseOwnsItsOwnClocks()`, the whole
  counter-attack block and all three remaining temporary paths. `AssertShimPair()` replaced by
  `AssertCounterAttackPhase()`; the two shim clone cases replaced by one for the battle module; a new
  `…ObjectiveDirector_TerminalPhaseEndsTheObjectiveOnOnePath`; `…GateWaitsForDaylightThenFiresOnce` re-pointed and
  extended with the resolution half; `…ObjectiveAnchor` re-pointed at the plans' authored radii.
- `compile-check.sh` 0 (6235 files). Director **4,926 → 4,383 lines**. Every acceptance grep empty, ledger closed.
- **Suite run owed.** Nothing committed.

### 2026-08-21 — feature started
- `/start-feature occupying/objectives` — `implementation.md` flipped to In Progress, `tasks.md` (74 tasks
  across 8 phases) and this file scaffolded.
- Running autonomously via `/autorun-feature`. Phases 2–6 route to `component-developer-advanced`; 1 and 7 to
  `component-developer`; 8 to `help-docs-sync`.

---

### 2026-08-21 — Phase 1 built
- T1.1–T1.8 complete. `OVT_ObjectiveAssetRecord` + `m_mAssets` on the director, `IsAssetUp(key)` /
  `GetAssetPosition(key)` shipped, `IsFOBUp()` / `GetFOBPosition()` deleted with no wrappers (D4), all 11
  production and test consumers moved, the repair module and its config relocated out to the deployments
  framework (D3). `compile-check.sh` 0. Suite, Workbench config load and play-test owed.

---

### 2026-08-21 — Phase 2 gate: two reds, one root cause, fixed
- The version-2 save record did not round-trip: `Deserialize` read into `read`-prefixed locals while a save
  context keys its properties by the **local variable's name**. `targetKind` came back 0, the restore took its
  "no objective" branch and cleared the forward base with it. Rewritten around a single
  `array<ref OVT_PersistedObjective> objectives`, every `Read()` return checked, `array<bool>` avoided.
  `compile-check.sh` 0. Full write-up in the Phase 2 section; the rule is now Gotcha #14.
- Also recorded: the suites had never loaded this worktree before this run, so every earlier green in this
  feature's record was another checkout's.

---

### 2026-08-21 — Phase 2 built
- T2.1–T2.15 complete. `OVT_ObjectivePlanRules` (pure statics), `OVT_ObjectiveRegistry` / `OVT_ObjectiveConfig`
  / `OVT_ObjectivePhase` / `OVT_ObjectiveInstance`, the four module base classes, the target-resolver seam, the
  two legacy shims, the runner rework (instance loop, one phase-entry funnel, strangler fallback), the two
  shipped plans on the game-mode prefab, the validator **with a real call site**, and the version-2 save record
  with names instead of enum integers. The two success counters moved into the instance bag and their record
  fields were deleted.
- 17 can-fail proofs injected and compiled (all exit 0). `compile-check.sh` 0 (6216 files).
- **Owed:** the All-suite run (orchestrator), the Workbench load of `Configs/Objective/overthrowObjectives.conf`
  and the prefab's `m_Registry` line, and the parity play-test — the ramp must read in the log exactly as it did
  before this phase.

---

### 2026-08-21 — Phase 4 gate: 3 reds, all in the Phase-2 framework case file, all repaired
- All 427/431. Two stale assertions (case A's `EXPECTED_MODULES = 2` shim-pair shape, case E's
  hard-coded runtime module count) and one under-authored fixture (case B's valid control plan built a
  phase with an EMPTY module bag, which T4.9's wedge rule correctly rejects). **No Phase-4 code defect.**
- Case E's count became a module-by-module comparison against the AUTHORED phase — type, name, order,
  clone-not-template, and initialised — which is strictly stronger and cannot go stale. Case A split
  along the strangler seam (real doctrine vs shim pair). Case B's fixture was made legal and grew four
  rows for the four new module-bag rules.
- 5 further can-fail faults injected and compiled (all exit 0), each restored. `compile-check.sh` 0.

---

### 2026-08-21 — Phase 4 built
- T4.1–T4.11 complete. Four target resolvers, the unified `OVT_SendDeploymentObjectiveOperation`, the
  `SupportBelow` / `ProgressAtLeast` / `TargetKindIs` conditions and the `IdleFor` abort, the two
  deployment-side reporters re-pointed at the bag through `ReportObjectiveProgress()`, the deployment
  phase span moved from enum integers to phase NAMES, the difficulty convention flipped on the two
  objective-exclusive behaviour modules (with their configs re-authored to `-1` in the same pass), the
  harassment phase authored in both plans, three validator rules, and nine new Init cases.
- **Deleted from the director:** `TickHarassment`, `SendNextOperation`, `SendHarassmentOperation`,
  `SendTowerRecaptureOperation`, `SendSabotageOperation`, `CountLiveHarassmentOperations`,
  `CountLiveSabotageOperations`, `CheckHarassmentGate`, `CheckBaseHarassmentGate`,
  `ObjectiveTownCarriesHarassmentDebuff`, `ResolveObjectiveSupportPercentage`, `OnHarassmentSuccess`,
  `OnSabotageSuccess` — and the `m_iLegacyPhase 1` shim authoring in both plans.
- 10 can-fail faults injected and compiled (all exit 0), each restored. `compile-check.sh` 0
  (6230 files). Director 6,451 → 6,212 lines.
- **Owed:** the All-suite run (orchestrator); a Workbench look at the two rewritten harassment phases;
  the two-tower dedup play-test; and a mid-ramp save/Continue.

---

### 2026-08-21 — Phase 3 built
- T3.1–T3.9 complete. `OVT_ObjectiveTargetSelector` (the seam), `OVT_ObjectiveCandidateSet` + the
  `OVT_EObjectiveCandidateSource` flag set (the one place the world is walked), the two shipped selectors with
  the eight weights lifted to attributes whose defaults are the constants, plan-driven `SelectObjective()`,
  the `m_iSelectionCooldownTicks` cadence with its debug measurement, the two new validator rules, one Logic
  case and the T3.8 parity case.
- **Deleted from the director:** `CollectTownCandidates`, `CollectBaseCandidates`,
  `DistanceToNearestHeldBase`, `HasOccupyingTowerCoverage`, `ResolveBaseName`, `ResolveTownName`,
  `ResolveTownNameAt` — plus the strangler's `ResolveLegacyPlan()` and `LEGACY_TOWN_PLAN` /
  `LEGACY_BASE_PLAN`, which context.md's own deletion schedule put in this phase.
- 12 can-fail faults injected and compiled (all exit 0), including the phase's required one — a `.conf`
  weight flip whose inversion was computed in IEEE binary32. `compile-check.sh` 0 (6220 files).
- **Owed:** the All-suite run (orchestrator); a Workbench look at the two new `m_Selector` blocks; and a
  play-test of the ramp, whose selection line now names the plan.

---

*Update this file at the end of each work session.*

---

## 🔴 2026-08-21 — every suite verdict on this worktree so far was taken against the WRONG TREE

**`tools/run-tests.sh` has never tested this worktree.** The Phase 2 gate run loaded
`N:/Projects/Arma 4/Overthrow.Arma4/addon.gproj` — the sibling `v1.5` checkout — not
`Overthrow.Arma4-objectives`. Proof, from `.tmp/run-tests/autotest.log`: the run executed
`OVT_TEST_Init_ObjectiveRepair_*`, the class name **T1.7 renamed away**. This tree contains only
`OVT_TEST_Init_BaseRepair.c`; the sibling still has the old file. None of Phase 2's 12 new cases
(`OVT_TEST_Logic_ObjectivePlanRules_*` ×5, `OVT_TEST_Init_ObjectiveFramework_*` ×7) appear anywhere in
the log, and the total was 417 — unchanged from before Phase 2 added them.

### Mechanism

`run-tests.sh` delegates to `launch-game.sh`, which launches the **vanilla** gproj plus
`-addonsDir "N:\Projects\Arma 4",<My Games>\addons -addons Overthrow`. Three sibling directories under
`N:\Projects\Arma 4` — `Overthrow.Arma4`, `Overthrow.Arma4-main`, `Overthrow.Arma4-objectives` — all
declare **`ID "Overthrow"` AND the identical `GUID 59B657D731E2A11D`** in their `addon.gproj`. The engine
resolves the ambiguity to `Overthrow.Arma4`. Nothing about the invocation can express "this checkout".

### Why the addon-load guard passed anyway

`run-tests.sh:271-287` builds `EXPECTED_GPROJ_FWD` from `$OVT_REPO_ROOT/addon.gproj` (the Phase-1 fix) but
**ORs it with a legacy fallback** `$0 ~ /Overthrow\.Arma4[\/\\]addon\.gproj/` at `:277`. That fallback matches
the *wrong* tree's path. The fix that was made in Phase 1 to prevent a false INDETERMINATE is precisely what
converted a wrong-tree run into a false GREEN.

### Verdicts this invalidates

- **Phase 1's "All `{6A6E2A002F53A581}` 416/417" is VOID.** Same wrong tree; the count is identical because it
  is literally the same set of cases. The "pre-existing v1.5 leftover" red
  (`CompositionSlotGate_AcceptedTypesMatchTheCompositions`) is a fact about `Overthrow.Arma4`, not about here.
- **Phase 2 has no suite verdict.**
- Any run taken from `Overthrow.Arma4-main` is suspect for the same reason.

### NOT affected — `compile-check.sh` is trustworthy

`compile-check.sh:213` passes `-gproj "$GPROJ_WIN"` = **this worktree's own `addon.gproj`**, so it compiles this
tree directly and never resolves an addon ID. Its file count tracking 6202 → 6216 across Phase 2's ~14 new files
is the confirmation. Every compile verdict recorded in this feature stands.
⚠ But `compile-check.sh:410`'s *proof-of-load* message still hardcodes the `Overthrow.Arma4/addon.gproj` string;
harmless today (the `-gproj` form cannot load the wrong tree) but it should not be copied into a new check.

### Owed before Phase 3

Phases 1 and 2 both need a **real** suite run. Until the addon-resolution problem is fixed, the only trustworthy
gates on this worktree are `compile-check.sh` and the static acceptance greps/diffs.

---

## ✅ 2026-08-21 — the tooling is fixed, and Phase 2's gate is GREEN on a run that provably tested THIS tree

### How to run the suites on this worktree (do not lose this)

`tools/run-tests.sh` alone still resolves `-addons Overthrow` to a sibling checkout. Two things make it correct
here, and **both** are needed:

1. **A junction farm** — `N:\Projects\ArmaAutotestAddons\Overthrow.Arma4-objectives` is a junction to this tree
   (`cmd.exe /c mklink /J`). It lives OUTSIDE `N:\Projects\Arma 4` deliberately, so the default `-addonsDir`
   scan of the repo parent cannot see it and no other tool's behaviour changes.
2. **`OVERTHROW_GAME_ADDONS_DIRS`** pointing at that farm plus the My Games workshop addons dir (the packed
   EPF/EDF dependencies still have to come from there). It is now set in `.claude/settings.local.json` `env`
   (gitignored) so a session gets it automatically:
   `N:\Projects\ArmaAutotestAddons,C:\Users\Aaron Static\OneDrive\Documents\My Games\ArmaReforgerWorkbench\addons`

**The guard now fails loudly instead of silently.** `tools/run-tests.sh`'s addon proof no longer string-matches
a path; it converts every gproj the engine listed back through `wslpath -u` + `readlink -f` and compares **file
identity** against `$OVT_REPO_ROOT/addon.gproj`. A junction resolves to the same file and matches; a sibling
checkout is a different file and does not. So if the junction or the env var goes missing, the next run reports
**INDETERMINATE**, never a false green. ⚠ Do not "fix" that by re-adding a path fallback — see the capitalised
comment in the script.

⚠ These are **shared-tool and outside-the-repo changes** made because they blocked the gate, in the same
category as Phase 1's `$OVT_REPO_ROOT` guard fix. `tools/run-tests.sh` is modified in the working tree; the
junction is a filesystem object under `N:\Projects\ArmaAutotestAddons`. `Overthrow.Arma4-main` still has the
original defect and will silently test `Overthrow.Arma4` until it gets the same treatment.

### Phase 2 gate — All `{6A6E2A002F53A581}`: **417 / 418**

The one red is `OVT_TEST_Init_CompositionSlotGate_AcceptedTypesMatchTheCompositions` ('Base Fortifications'
authors no `OVT_CompositionSlotConditionDeploymentModule`) — the **pre-existing `core/damage` leftover**, and
this is the first run that can honestly call it that, because it is the first run of this tree. Phase 2 touched
no composition or slot-gate code.

Confirmed in the run's `console.log` / `autotest.log`:
- the loaded gproj was `N:/Projects/ArmaAutotestAddons/Overthrow.Arma4-objectives/addon.gproj`
- `OVT_TEST_Init_BaseRepair_*` ran and `OVT_TEST_Init_ObjectiveRepair_*` did **not** — i.e. T1.7's rename is live,
  which is the single cheapest proof of tree identity available and is worth re-using
- all 12 new Phase-2 cases ran and passed
- **the 44 existing objective Init cases passed unedited** — the parity gate, on real evidence

**Phase 1 is retro-covered.** Its changes live in this tree, so this run is also the first real verdict on
Phase 1. The "All 416/417 GREEN" recorded for it earlier remains void as *evidence*; the conclusion happens to
hold.

### One non-reproducing timeout, recorded rather than dismissed

The first junction-based run reported `OVT_TEST_Logic_BaseDefenseConversion_FundingSplitConservesTheTotal:
FAILURE / timeout`, with `Output: <none>`, **3 s** into the run rather than at its 30 s budget. The case is pure
integer arithmetic with no loop, no world and no I/O, and it is the alphabetically first case the Logic suite
executes. It passed on the next run with no change to it or to anything it touches. Most likely a first-case
startup artifact, possibly aggravated by the extra drvfs indirection of loading through a junction. **It is not
a code defect and it is not a flaky assertion** — but if it recurs, suspect the junction mechanism before the
case, and prefer a fix that makes the client load this tree without one.

### T2.11's real root cause — hoisted to Gotchas #14

The serializer round trip failed on both Persistence cases because **an Enfusion save context keys each property
by the LOCAL VARIABLE'S NAME, not by position**: `SaveContext.Write(x)` writes property `"x"` and
`LoadContext.Read(y)` reads property `"y"`. Version 2's `Deserialize` read into `readTargetKind` etc. while
`Serialize` wrote `targetKind` etc., so every field came back zero, `targetKind == 0` = `NONE`, and
`ApplyPersistedObjective()` took its "nothing was saved" branch — clearing the objective **and** the forward-base
record. Neither assertion was wrong; both were weakened by nothing.

🔴 **This invalidates the premise of `implementation.md` §3.8's field table**, which describes a positional
per-instance sequence with an explicit `instanceCount`. A per-instance field loop cannot work at all under
name-keyed properties — writing `configName` once per objective writes ONE property N times. The format is now
one `array<ref OVT_PersistedObjective>` plus the blacklist pair, matching the `OVT_PersistedJobV2` idiom already
in this repo. The record's **content** is unchanged. `OVT_JobManagerSerializer.DeserializeVersion2()` had already
recorded this trap on 2026-08-09; it cost us a full gate cycle to rediscover.

---

## Phase 3 gate — All `{6A6E2A002F53A581}`: **419 / 420** (2026-08-21)

Green. The single red is the same pre-existing `OVT_TEST_Init_CompositionSlotGate_AcceptedTypesMatchTheCompositions`
leftover; Phase 3 touched no composition or slot-gate code. Loaded gproj confirmed as
`N:/Projects/ArmaAutotestAddons/Overthrow.Arma4-objectives/addon.gproj` — this tree.

🔴 **T3.8's parity case passed in the real client**:
`OVT_TEST_Init_ObjectiveDirector_PlanDrivenSelectionReproducesTheSingleListPick`. That is the claim the whole
phase rests on — that two plans scoring one shared candidate set reproduce the old single-list pick — and this
was the **last** phase in which it could ever be proved, because it needs both paths to exist at once. Phase 4
onward, the old path is gone.

Case count moved 418 → 420 (T3.7's Logic extension + T3.8). The two selection cases that were re-pointed
(`DeterministicSelectionPicksTheSameCandidate`) also passed unedited in meaning.

**Scope note the orchestrator accepted:** the agent added `SelectWithoutAPlan()`, a second strangler half not in
the plan, so that a registry that fails to load leaves the occupying faction still attacking rather than passive.
It fires **only** when the plan list is empty (registry absent or every plan skipped), never when plans merely
decline, and it is scheduled to die in Phase 6 alongside `RunUnplannedObjectiveTick()`. This preserves Phase 2's
fallback promise rather than extending scope, but it is two temporary code paths now owed to Phase 6 — if Phase 6
deletes only one, `grep -rn "SelectWithoutAPlan\|RunUnplannedObjectiveTick" Scripts/` must not be empty by
accident.

---

## Phase 4 gate — All `{6A6E2A002F53A581}`: **430 / 431** (2026-08-21)

Green after one repair pass. The only red is the pre-existing `CompositionSlotGate` leftover.

The first run was **427/431**. All three reds were in Phase 2's `OVT_TEST_Init_ObjectiveFramework.c`, and
**none was a Phase-4 code defect** — which is exactly what the strangler seam is for: Phase 2's cases are the
tripwire on Phase 4's replacement, and they fired.

| Case | Root cause | Resolution |
|---|---|---|
| `…_ARegistryResolvesAndValidates` | `EXPECTED_MODULES = 2` (the shim pair) applied to all three phases; T4.7/T4.8 legitimately replaced Harassment's pair with six real modules | Split along the strangler's seam: a per-phase header assert, a Harassment-specific assert (three sends, **ladder second**, ≥1 condition, ≥1 abort, **no shim**), and a ⚠ TEMPORARY shim-pair assert that loses one caller line per build phase and dies with the last shim |
| `…_BValidatorNamesAndSkipsABrokenPlan` | 🔴 **the fixture was wrong and the rule was right.** T4.9's wedge rule fired on "Alpha", the fixture's *valid control*, because `MakePhase()` built it with an **empty `m_aModules`** — a phase that can neither advance nor end | Rule untouched. `MakePhase()` now builds the minimum legal bag. Four rows added, one per new module-bag rule, each fixture broken in exactly one way |
| `…_ECommitEntersPhaseZeroAndEveryEntrySyncsTheInstance` | Stale hard-coded `modules = 2`, and an `AssertState()` that phrased *every* count mismatch as "left N from the phase before" — the red was on the **first** commit, which has no previous phase; the 6 were the freshly-cloned Harassment set | Expectation now **derived from the plan the objective is running** and compared module-by-module in order: type, name, not-the-template, `IsInitialized()`. A count could never distinguish `ForwardBase` from `CounterAttack` — both are 2 |

**The `EnterPhase` contract was verified intact by inspection, not by tuning a number:**
`OVT_ObjectiveInstance.EnterRuntimePhase()` calls `ExitRuntimePhase()` first (`Exit()` each outgoing, then
`Clear()`), and `OVT_ObjectivePhase.CloneModules()` clears the destination again before inserting. §3.3/T2.7's
"`Exit()` outgoing → clone incoming → `Initialize()` incoming" holds.

🔴 **New assertion worth knowing about:** until this repair, *nothing anywhere* asserted that the runtime module
set contains **clones and never the config's own template objects**. Can-fail proof P12 (`CloneModules()` inserts
the template) now reddens case E. Phases 5 and 6 both rebuild a module set and were relying on that being true
without any cover.

### Deviations accepted this phase

1. 🔴 **`OVT_TowerRecaptureBehaviorDeploymentModule` is excluded from T4.6's difficulty flip.** It is authored in
   **two** configs, and one of them — `Deployment_TowerRecaptureUnrest.conf` — is required to show no diff.
   Flipping the convention while that config keeps its authored `600` would make the engine *honour* 600 s on
   Easy (where the campaign says 900) and on Insane (where it says 300) — a real behaviour change to a frozen
   shipped deployment. §3.10's own precondition ("behaviour-neutral **only if** the configs are re-authored in
   the same commit") cannot be met, so the exclusion is forced. Two modules / three attributes flipped, not three.
2. **`SendRampOperation()` — a third temporary bridge.** `SendNextFOBOperation()` chained three now-deleted
   senders (the 2026-08-19 ramp-continuation deadlock fix). The bridge borrows the objective's *first* phase's
   operation modules. **Dies in Phase 5** with `SendNextFOBOperation()`.

⚠ **The temporary-path ledger now has THREE entries owed to later phases**, and a partial cleanup would leave a
grep non-empty by accident rather than by intent:
`RunUnplannedObjectiveTick()` (P2 → dies P6) · `SelectWithoutAPlan()` (P3 → dies P6) · `SendRampOperation()`
(P4 → dies **P5**).

> **LEDGER UPDATED, 2026-08-21 (Phase 5).** `SendRampOperation()` is **RETIRED** — deleted with
> `SendNextFOBOperation()` as scheduled; `grep -rn "SendRampOperation" Scripts/` is empty. The ramp continuation
> it carried is authored data now (the three ramp operations repeated in the ForwardBase phase). **THREE entries
> remain, all owed to Phase 6:** `RunUnplannedObjectiveTick()` · `SelectWithoutAPlan()` · **NEW —**
> `StartCounterAttackNow()` / `m_bCounterAttackStarted`, the battle starter the forward-base handler used to own
> (see Phase 5's deviation 2). All three die with `RunLegacyPhaseTick()`, both shim classes and the phase enum.

> ## ✅ **LEDGER CLOSED, 2026-08-21 (Phase 6). IT IS EMPTY.**
>
> All four temporary paths this feature ever opened are gone, and each was verified **BY NAME** rather than by
> the Definition-of-Done grep — which only looks for `OVT_LegacyPhase` and would have missed a stranded one:
>
> | Path | Opened | Closed | Verified by |
> |---|---|---|---|
> | `SendRampOperation()` | Phase 4 | Phase 5 | `grep -rn "SendRampOperation" Scripts/` → empty |
> | `RunUnplannedObjectiveTick()` | Phase 2 | **Phase 6** | `grep -rn "RunUnplannedObjectiveTick" Scripts/` → empty |
> | `SelectWithoutAPlan()` | Phase 3 | **Phase 6** | `grep -rn "SelectWithoutAPlan" Scripts/` → empty |
> | `StartCounterAttackNow()` + `m_bCounterAttackStarted` | Phase 5 | **Phase 6** | `grep -rn "StartCounterAttackNow\|m_bCounterAttackStarted" Scripts/` → empty |
>
> 🔴 **AND WHAT CLOSING IT COSTS, STATED ONCE.** Both fallbacks existed because **no compiler reads a `.conf`**.
> There is no fallback now, so a registry that fails to load means **the occupying faction stops attacking**:
> selection finds no eligible plan and picks nothing, and an objective committed from outside a selection round
> finds no modules and does nothing at all — it cannot act, cannot advance, and cannot even be given up, because
> an empty module set carries no abort module either. Two ERROR lines are the ONLY symptom:
> `SelectObjective()`'s round line and `LogObjectiveWithNoPlan()`.
> `OVT_TEST_Init_ObjectiveFramework_ARegistryResolvesAndValidates` is what turns that into a red test instead.

🔴 **Phases 5 and 6 must each author an `OVT_IdleForObjectiveAbort` when they replace their shim** — a phase with
no abort module can no longer time out at all. The shipped plans are pinned by an Init case, so forgetting this
is a red, not a silent stall. ✅ **Both did:** ForwardBase in Phase 5, CounterAttack in Phase 6, and
`AssertForwardBasePhase()` / `AssertCounterAttackPhase()` each require it by TYPE.

---

## Phase 5 — the forward base as an operation module (2026-08-21)

### T5.1 survey verdict — the plan's boundaries held; ONE caller it did not predict, and one it did

**Re-verified against the tree before a line moved.** The 36 methods in `implementation.md` §4 Phase 5's
deletes list all existed, at line numbers within a few of the ones quoted, and the block's boundaries are
exactly as described: `TickFOB` at `:1666`, then the counter-attack sub-block (Phase 6's, untouched), then
`SendNextFOBOperation` at `:2016` through `CountFOBSpend` at `:3560`, plus `RecordFOB`/`AddFOBSpend`/
`SetFOBStarvationTicks` at `:5096-5122` and `ClearFOBRecord` at `:5853`.

**External callers of the 36, found by grep over `Scripts/ Configs/ Prefabs/`:**

| Method | Caller outside the director | Disposition |
|---|---|---|
| `OnFOBRaised` | `OVT_FOBRaiseSpawningDeploymentModule.NotifyDirector()` (`:278`) | **Re-pointed** at the keyed `ReportAssetRaised(ASSET_FOB, …)`. That module is deployment-side and this feature owns it - the same re-point T4.4 made for the two success reporters |
| `CanDismantleFOBAt` | `OVT_DismantleEnemyFOBAction.c:126` | **Facade kept.** No diff for this phase |
| `CanDismantleFOB`, `OnFOBDismantledByPlayer` | `OVT_CampaignRequestComponent.c:245` (+ prose) | **Facade kept.** No diff at all |
| `RecordFOB` | 4 Init call sites, 3 Persistence call sites | Re-pointed at `ReportAssetRaised()` |
| `AddFOBSpend`, `SetFOBStarvationTicks` | 3 + 1 Persistence call sites | Re-pointed at `AddAssetSpend(key, …)` / `SetAssetStarvationTicks(key, …)` |
| the other 29 | **none** | Pure internals |

🔴 **THE ONE THE PLAN DID NOT PREDICT, AND IT CHANGED THE DESIGN.** Three Init fixtures called `RecordFOB()`
on an **idle** director - no objective committed at all - because the old writer had no guard. The keyed
reporter that replaces it keeps `OnFOBRaised()`'s guard instead (a supply party can outlive the objective that
sent it, and a base recorded onto an idle machine makes the anchor provider prefer a phantom for the rest of
the campaign), so those three fixtures now commit an objective first. **`OVT_TEST_Init_ObjectiveFOB_F` needed
more than that**: the teardown is the raise module's now and the raise module is only reachable because it
registered itself on **phase entry**, so a fixture that never entered the forward-base phase would have swept
nothing and gone green against a machine that cannot happen. It enters the phase. That is a stronger case than
the one it replaced, not a weaker one.

**Verdict: proceed.** Boundaries confirmed, external surface is four methods and one deployment-side reporter,
and the facade carve-out is exactly the right shape - both of its callers are untouched.

### 🔴 C4 RESTATED AT THE NEW CALL SITE: PRESENCE IS MEASURED AT THE FORWARD BASE (T5.4, T5.11)

`OVT_AssetStarvedObjectiveAbort.IsPlayerAtAsset()` reads
`OVT_WorldUtils.PlayerInRange(asset.position, difficulty.baseCloseRange)` — **the asset's own position**, which
is where the flag stands, and **not** the base that supplies it. The requirements-era prose says "at its source
base" and is wrong; the shipped code is the parity reference and this is a verbatim port of it.

**Why it has to be the asset.** "The resistance is standing on it" is the third of the three starvation inputs
and it means *a player is interdicting the thing itself*. Measured at the source base instead, a player could
sit on the forward base indefinitely with no effect at all, and clearing a base a kilometre away would starve
one that was perfectly supplied. Both halves of that are wrong in play and neither would look like a bug.

The correction is now written out **three times, on purpose**: in the abort module's class header, at the
method, and here. It has been "fixed" once already.

⚠ **The other two inputs ported verbatim too.** The supplying base is matched within 100 m of the recorded
position (a recorded position with no base near it is a base that has gone), and the garrison is counted
through `CollectRegisteredHandles()` + `IsRegistered()` + `GetAliveMemberCount()` — **never** through spawned
entities. A dormant group reports zero agents while being perfectly alive, so a body count would starve every
forward base on the map the moment the last player drove away, which is exactly when one is supposed to be
quietly doing its job. That is the **opposite** of what the dismantle rule counts, deliberately: that one asks
"is anybody shooting at me", which is a question about materialised bodies.

⚠ **The counter is served in `OnTick()` and only READ in `ShouldAbort()`.** The runner folds the abort modules
**twice** on a tick that reaches the end, so a `ShouldAbort()` that incremented would count every such tick
twice and starve a base in half the authored time.

### 🔴 THE TWO INDEPENDENT RAISE LATCHES, AND WHY NEITHER SUBSTITUTES FOR THE OTHER (T5.6, T5.11)

They are in different objects and they answer different questions.

| Latch | Where | Question | What removing it costs |
|---|---|---|---|
| `WasRestoredFromSave()` | `OVT_DeploymentComponent`, read by `OVT_FOBRaiseSpawningDeploymentModule.DecideRaise()` | "did this deployment come back from a save point?" | The structure is a **persistence-tracked world entity**: vanilla persistence puts it back before any deployment ticks, so a restored deployment that raised anyway gives the campaign a SECOND forward base **on every single load**, in a slightly different place each time. Ten loads is ten flagpoles in a field, every one persisted and dismantleable |
| `m_bRaiseAttempted` | the raise module itself | "have I already had my one attempt?" | A **reinforcement rebuy CLEARS the eliminated flags and re-runs the convergence**, so a second arrival raises a second structure beside the first — in the same session, with no save involved |

`DecideRaise(hasDeployment, alreadyAttempted, restoredFromSave, eliminated)` is a pure static precisely so both
claims are assertable without a save, a truck or a world (`OVT_TEST_Init_ObjectiveFOB_C`). **Neither latch is
reachable from the other's failure mode.**

⚠ **A THIRD LATCH JOINS THEM THIS PHASE, ON THE OBJECTIVE SIDE:**
`OVT_RaiseForwardBaseObjectiveOperation.m_bDeploymentSent`. It answers "has a supply party been SENT", it is
what **arms the spend ceiling**, and it is **runtime-only and never persisted**. A save taken while the truck
was on the road comes back with no truck, no convoy and a deployment that can never raise anything, so the
honest restored state is "nothing was sent" — and the restored module re-discovers the stranded marker,
collects it and sites again. It is also why the module **adopts** a base that is already standing on entry
(`OnEnter()` sets it when `record.up`): a restored objective gets a fresh clone with no memory, and without the
adoption the ceiling would be disarmed and the teardown would have nothing to take down.

⚠ **THE STRUCTURE IS FOUND FOR TEARDOWN BY PREFAB RESOURCE NAME OFF THE CONFIG, NEVER BY A RUNTIME `EntityID`.**
The raise module holds an id, but that link dies with the session; the join is
`FindConfigByName(m_sDeploymentConfigName)` → its `OVT_FOBRaiseSpawningDeploymentModule` → `GetFOBPrefab()` →
`QueryEntitiesBySphere` matching `OVT_PrefabUtils.GetPrefabName()`. It survives a restore because persistence
respawns from the same prefab, and a modded config that raises a different structure is still torn down.

⚠ **THE RE-LINK IS A FIRST-TICK JOB, NEVER DESERIALIZATION**, and that is unchanged: the structure, the
deployment marker and the director's record are three independently restored things with no ordering between
them, so `ResolveRestoredObjective()` matches name + position on the first tick and gives up only after
`FOB_RELINK_ATTEMPTS`.

### 🔴 THE DEADLOCK FIX HAS TWO HALVES AND BOTH ARE AUTHORED DATA NOW (T5.7, T5.11)

"Phase 1 operations continue into the forward-base phase" is the 2026-08-19 deadlock fix. A base objective is
promoted on its **first** completed sabotage mission and its counter-attack gate demands up to **six** on Easy,
so a promotion that stopped the ramp made the remaining five unsendable and the battle unreachable. Towns
deadlock identically: the stacking support debuff that drives support under 25 % is applied **by** harassment
operations, so it stopped stacking, timed out, and support recovered.

In the config world that one rule is expressed **twice**, in two different files, and either half alone leaves
the ramp dead:

| Half | Where | What it says | Symptom if omitted |
|---|---|---|---|
| **The operations** | `Configs/Objective/Objective_*.conf`, the ForwardBase phase's `m_aModules` | tower recapture, the harassment ladder and sabotage are authored **again**, after the raise and the garrison | Nothing is ever SENT: the ramp simply stops at the promotion |
| **The spans** | `Configs/Deployment/Deployment_Objective{Harassment,Sabotage,TowerRecapture}.conf`, `m_sFromPhase "Harassment"` / `m_sThroughPhase "ForwardBase"` | a ramp deployment still BELONGS while the objective is in the forward-base phase | Everything sent is **collected** on its next reinforcement check the moment the objective is promoted |

Both halves are pinned by `OVT_TEST_Init_ObjectiveFOB_QForwardBasePhaseAuthorsTheShippedChain`, which walks the
plans for the first and the deployment registry for the second. The span half was already correct from Phase 4;
this phase added the operations half and the assertion that covers **both**.

⚠ **The authored order is the contract and the `.conf` cannot say so.** Both plans author
`[raise] [garrison] [tower] [ladder] [sabotage]`, which is the hard-coded forward-base spender's five-way `&&`
chain term for term. The first module that acts consumes the cadence, so the order decides who gets the
interval on a tick where more than one could act. It is written down in
`OVT_RaiseForwardBaseObjectiveOperation`'s class header and in the Init case, and nowhere else.

### The interval CLAIM: how "the forward base has first claim" survives becoming a module

The hard-coded chain had two `return false`s that **stopped** it rather than falling through, and an operation
module returning false cannot express that. `OVT_ObjectiveDirectorComponent.ClaimOperationInterval()` is the
seam: a module that did not act may say "not this tick, and not by anyone else", and
`RunObjectiveOperationModules()` ends the walk. It is **per-walk, not a latch**, it does **not** re-arm the
cadence and it does **not** re-arm the idle clock — it is neither a refusal nor a success.

Two play-tests put it there and both were livelocks:

- **MONEY (2026-08-20).** Refused at 120 against a pool of 56, the walk fell through to sabotage at 100 and the
  faction bought a sabotage mission every time the pool passed 100 and never reached 120. The reserve floor
  named the LAST refused operation because each refusal overwrote the previous one's floor, and the floor alone
  would not have helped anyway because it deliberately does not govern this component's own spending.
- **HOUSEKEEPING (2026-08-20).** A tick spent collecting a stale marker handed its interval to a sabotage
  mission and pushed the forward base a full cadence away — 60 in-game minutes on Normal.

⚠ **ONLY AN AFFORDABILITY REFUSAL CLAIMS.** An unregistered config name is a fault to be fixed and a spent
ceiling is a decision the machine made about itself; the module asks `director.IsBlockedOnAffordability()` to
tell them apart, exactly as the hard-coded `m_bBlockedOnAffordability && !up && !sent` condition did.

⚠ **NOTHING IN THE HARASSMENT PHASE CLAIMS**, which is why that ramp still falls through from tower recapture to
harassment to sabotage exactly as it always has. The claim is authored doctrine's, not the runner's.

### The spend ceiling stayed a counter, and it moved onto the asset module (T5.5)

`fob.spent` lives on the **asset record**; `OVT_ObjectivePhaseRules.FOBBudgetCeiling()` /
`WithinFOBCeiling()` are **byte-unchanged**; the director's one create-then-debit choke point now asks every
**registered asset module** instead of one hard-coded pair of methods (`IsAssetCeilingArmed()`,
`WithinAssetCeilings()`, `CountAssetSpend()`).

- It is **inactive during harassment** — not by a phase test, but because the raise module is not in that
  phase's module set and therefore has not registered.
- It **arms when the forward base's own deployment is SENT**, before the pre-flight, so the structure's own cost
  is inside the budget.
- It **disarms when the objective's record is cleared**, through the same one funnel that drops the anchor and
  the reserve floor.
- 🔴 **A spent ceiling still does NOT set `m_bBlockedOnAffordability`.** Being broke is a fact about the FACTION
  and holds the idle clock; a spent ceiling is a decision the machine made about ITSELF, and a phase that can
  only ever hit that SHOULD run its clock down and be abandoned.

### 🔴 THE DAYLIGHT HOLD IS "THE ONLY THING IN THE WAY", NOT "ONE OF THE THINGS IN THE WAY"

`OVT_BaseObjectiveConditionModule.HoldsIdleClock()` is new, and the RUNNER's fold is where the D17 correction
actually lives: `holdsClock = at least one condition is false AND EVERY false condition holds the clock`.

**Holding on the first holding condition alone would have been a silent regression** — the hard-coded gate
answered `NOT_READY` (clock runs) whenever the RAMP was unfinished and `WAIT_FOR_DAYLIGHT` (clock held) only
when the clock was the sole blocker. With a naive fold, a forward-base phase that was wedged for some entirely
different reason would stop spending its 240-minute backstop every night, so the give-up would take roughly
**two and a half times longer** in in-game minutes and a wedged objective would sit far past its budget.

⚠ **THE WAIT IS ANNOUNCED BY THE RUNNER, NOT BY THE CONDITION** (`LogIdleClockHold()`, latched once per **phase
entry**). A condition cannot see the others, so a wait logged from inside the daylight module would be said the
first time night fell on a phase whose ramp was nowhere near finished. The runner is the only thing that knows
the hold applies; it quotes the module's authored `m_sModuleName`, which is why that name is a sentence.

⚠ **AND THE HOLD IS THE CLOCK AND ONLY THE CLOCK.** Starvation, every other abort, the operation cadence and
every operation all ran before the hold branch is reached, so a forward base cut off at 22:00 still comes down
at 22:00 and the garrison sender still runs. Freezing them was D17's original, over-broad wording and was
corrected by the author of the decision on 2026-08-19; it contradicts F7 outright and punishes a correct play.

### `OVT_BaseObjectiveAssetModule` — a class the plan did not name, and why it had to exist

An ASSET is a thing an objective puts in the world that **outlives the phase that built it**. That single fact
is what a plain operation module cannot express, and three separate things depend on it:

1. **The teardown.** `ResetObjective()` is reached from every phase — a timeout during harassment, a battle
   resolving in the counter-attack phase, a player pulling the flag down. On none of those ticks is the raise
   module in the running phase's set.
2. **The spend ceiling.** The create-then-debit choke point asks about it for EVERY operation, including the
   ramp operations that continue into a later phase.
3. **The player-facing dismantle rule**, which is asked **on a client**, where no objective and no module set
   exist at all.

So an asset module registers itself with the director on entry and the director holds it, by key, until the
objective's record is cleared. The director asks the **base class** three questions — is your ceiling armed,
what is it, take yourself down — and never names a concrete module, so the checkpoint asset that follows this
feature is a key and a module and no director change at all. `implementation.md` §4 Phase 5's own facade note
("forwards to the asset's module") already assumed this lookup existed; this is it.

⚠ **A REGISTERED MODULE MUST KEEP WORKING AFTER `Exit()`.** The base nulls `m_Objective` at phase end, so the
record and the director are cached on entry (`m_Asset`, `m_OwnerDirector`) and nothing in the teardown path goes
through `GetObjective()`.

### The four asset-record fields moved onto the BASE record

`OVT_ObjectiveAssetRecord` now carries all six of §3.5's fields (`up`, `position`, `sourceBasePosition`,
`spent`, `starvationTicks`, `deploymentName`) and `OVT_ObjectiveFOBRecord` survives as a **named type with no
fields of its own**. While the four were on the subclass, every generic reader — the keyed spend counter, the
keyed starvation writer, the save payload's six parallel arrays — had to cast to the FOB type to see them,
which is the "one asset API" the Phase-1 rename removed. **The save format did not change**: the same six
parallel arrays, written and read the same way, now with no cast in the middle.

### GUIDs authored (prefix `{6BA1…}`, verified free before authoring)

| GUID range | What |
|---|---|
| `{6BA1000000000060}`–`{6BA1000000000070}` | Town Offensive's ForwardBase phase: 5 operations, 2 resolvers, 4 conditions, 2 aborts |
| `{6BA1000000000080}`–`{6BA1000000000090}` | Base Offensive's, same shape |

The four shim GUIDs the two ForwardBase phases used (`…0024`/`…0025`, `…0034`/`…0035`) are **retired rather than
reused**, so a reader diffing the file never sees a GUID change meaning. The two phase GUIDs themselves
(`…0023`, `…0033`) are unchanged — it is the same phase, re-authored.

### Can-fail proofs recorded this phase (injected one at a time; every one exited `compile-check.sh` 0, every one restored)

| # | Fault injected | Case it reddens |
|---|---|---|
| P1 | drop `clone.m_iBudgetCost` | `…ObjectiveFOB_KRaiseOperationCloneCarriesEveryAttribute` |
| P2 | drop `clone.m_fAreaRadius` | `…ObjectiveFOB_PAssetStarvedCloneCarriesEveryAttribute` |
| P3 | `HoldsIdleClock()` answers false | `…ObjectiveFOB_NDaylightWindowCloneCarriesEveryAttribute` **and** `…ObjectiveDirector_GateWaitsForDaylightThenFiresOnce` ("must not serve a round off the phase timeout") |
| P4 | the Base plan's ForwardBase sabotage renamed to a harassment rung | `…ObjectiveFOB_QForwardBasePhaseAuthorsTheShippedChain` (the ramp-repeat half) |
| P5 | `Deployment_ObjectiveSabotage.conf` spans `Harassment`→`Harassment` | same case, the **span** half — the other side of the same deadlock |
| P7 | `m_bConcurrencyAtResolvedPosition 0` on the garrison | same case, the garrison half |
| P8 | the raise module never calls `RegisterAsAssetOwner()` | `…ObjectiveFOB_RCeilingArmsWithTheAssetAndTheSetIsClones` **and** the Persistence relink case's new owner assertion |
| P10 | `CloneModule()` hands back `this` | `…ObjectiveFOB_R`'s clones half (and `…ObjectiveFOB_K`'s runtime-state half) |

⚠ **"It went red" here means a reasoned trace plus a compile-clean injection, not an observed suite run** —
running the suites is the orchestrator's job (`.claude/test-policy.md`), the same standard Phases 2, 3 and 4
recorded their proofs to.

### 🔴 THE §4 PARITY CHECK AS WRITTEN IS NOT AVAILABLE FOR THIS PHASE, AND HERE IS THE SUBSTITUTE

§4 Phase 5 asks for "the shim for phase 2 and the module set produce the same gate answer and the same
operation on the phase's Init fixture, driven before and after the swap in the same session". **That cannot be
done**, for two independent reasons, and both are properties of the thing being replaced rather than of the
work:

1. **They cannot coexist.** The acceptance grep requires `TickFOB` deleted in the same phase that authors the
   module set. Phase 3's equivalent parity case worked precisely because both selection paths were allowed to
   coexist for one phase; the strangler rule forbids that here.
2. 🔴 **The shim was never a predicate.** `RunLegacyPhaseTick(2)` → `TickFOB()` **starts a real battle and
   advances the phase** (through `FireCounterAttack()`), spends real resources and can reset the objective.
   "Drive it on a fixture and compare the answer" was never available for this phase in the shape it was for
   selection.

**The four substitutes actually executed, named so they can be argued with:**

- **The gate answer**, driven end to end through the module set on the same fixture the shim used:
  `…ObjectiveDirector_GateWaitsForDaylightThenFiresOnce` still plants a complete ramp, a cut-off forward base
  and a night clock, and still asserts the phase timeout is untouched, the cadence served exactly one round,
  the starvation counter served exactly one round, no battle at night, and one battle in daylight. Its
  assertions were **not weakened** - one tick was added, because the advance and the battle are now one in-game
  minute apart by design.
- **The operation set**, pinned against the director's own constants and the live deployment registry:
  `…ObjectiveFOB_Q` (order, names, the ladder rung for rung, the two spans).
- **The starvation predicate**, unchanged and still pinned in the Logic tier
  (`OVT_TEST_Logic_ObjectiveScaling`, three inputs independently).
- **A line-by-line port.** Every moved body is verbatim apart from the receiver (`m_FOB` → the cached asset
  record, `m_Objective.position` → `objective.GetTargetPosition()`) and the constants that became attributes,
  whose defaults are the constants' values.

### Deviations from the plan, and why

1. 🔴 **`OVT_BaseObjectiveAssetModule` is a new class §3.3 does not list.** See above - the lifetime of a
   standing asset is not expressible on a plain operation module, and §4's own facade note presumes the lookup
   it provides.
2. 🔴 **`TickCounterQRF()` grew a TEMPORARY battle STARTER, and that is a fourth temporary path.** The
   counter-attack gate is four authored conditions now, and a condition can only ADVANCE a phase - it cannot
   start a battle. The starter the forward-base handler used to own therefore had to go somewhere for one build
   phase, and the honest place is the shim that already owns the battle phase.
   `StartCounterAttackNow()` + `m_bCounterAttackStarted` are marked ⚠⚠ TEMPORARY and **die in Phase 6** with
   `TickCounterQRF()` itself, when `OVT_StartBattleObjectiveOperation` becomes that phase's authored operation.
   `FireCounterAttack()` and `LogDaylightWait()` were **deleted early** (Phase 6's list) because this phase
   removed their only callers and leaving a method whose contract had become wrong is worse than deleting it.
3. **The advance and the battle are now one in-game minute apart.** The forward-base phase's conditions advance
   to `CounterAttack`, and the battle starts on that phase's first tick. That is the end state §3.1 and T6.1
   describe, reached one phase early because the gate had to be decomposed before the starter could move.
   ⚠ **A refused start no longer keeps the objective in the forward-base phase.** It sits in the battle phase
   and is abandoned with the blacklist when the phase times out - the same outcome the hard-coded refusal
   reached (it "sat out the rest of the forward-base phase and was abandoned when that timed out"), one phase
   later. `LogCounterAttackRefusal()` still says why, once.
4. 🔴 **The abort fold now runs BEFORE the advance conditions in the forward-base phase, and the hard-coded
   handler asked the gate first.** On the single tick where a starved forward base's gate would ALSO fire, the
   module world abandons the objective where the monolith would have started the battle. That is the runner's
   §3.2 order, it applies to every phase, and the state is a coincidence of two rare conditions in one in-game
   minute. Recorded rather than special-cased: a starvation abort that asked "unless the gate is about to fire"
   would stop being independent of the other modules, which is the contract that makes the fold free.
5. **`OVT_SendDeploymentObjectiveOperation` gained `m_iMaxConcurrentDifficulty` and
   `m_bConcurrencyAtResolvedPosition`** (plus the two-member `OVT_EObjectiveConcurrencyLimit` enum). The
   forward-base garrison's cap is `objectiveFOBGarrisonMax` (1..6 across the presets, where the ramp's is 1..4)
   and is measured around the FORWARD BASE, not around the objective. Without both, the garrison could not be
   the same module - which is §3.4's whole claim - or its cap would silently change on every difficulty preset.
   ⚠ The six existing harassment blocks are **not** re-authored: both attributes default to the value that
   reproduces today's behaviour, and every `.conf` edit is a risk no compiler can catch.
6. **The two dismantle radii are `static const` on the raise module, not attributes.** T5.2 asks for the siting
   knobs as attributes and they are; these two are different. The dismantle rule is asked on a CLIENT, where
   there is no plan to read an authored value from, so an attribute would be visible to the server and not to
   the client and the prompt a player reads would disagree with the rule the server enforces.
7. **`IsFOBStarving()` is derived from the counter** (`starvationTicks > 0`) instead of from a second boolean.
   The two are equivalent by construction - the abort zeroes the counter on every supplied tick and increments
   it on every cut-off one - and the record is the half that survives a save.
8. 🔴 **`wc -l` on the director is 4,926, not "below 3,000", and the criterion cannot be met by this phase.**
   The number in §4 was written against a **5,201-line** file; Phases 2-4 added ~1,000 lines of runner, plan
   registry and plan-driven selection before this phase started, so the file was **6,225** when it began.
   **1,752 lines were removed gross** (1,572 of methods, 145 of siting constants, ~35 of state) and ~450 added
   (the keyed asset API, the facade, the interval claim, the battle bridge and their reasoning), for a net
   **-1,299**. The forward-base block itself was fully moved: every one of the 36 methods is gone and the greps
   are empty. Phase 6's deletions (the ~470-line counter-attack block, both shims, `SelectWithoutAPlan()`,
   `RunUnplannedObjectiveTick()`, the enum) take it to roughly 4,300 - so §1's "2,000-2,200 end state" is a
   plan-time estimate that the built runner does not support, and saying so is better than stripping the
   reasoning §1 explicitly forbids stripping to hit a number.

### 🔴 What the Init tier CANNOT prove about this phase

1. ✅ **Compile** — `tools/compile-check.sh` → 0 (6236 files), and every can-fail injection above also exited 0.
2. ⏳ **The two plan `.conf`s still load.** Each ForwardBase phase now authors **twelve** modules and two
   polymorphic resolver sub-objects. `…ObjectiveFOB_Q` turns every part of that into a red test **except** a
   total failure to load the plan, which `…Framework_ARegistryResolvesAndValidates` already covers.
3. ⏳ **NOTHING DRIVES A REAL RAISE.** The siting lattice, the supply-truck insertion, the structure spawn and
   the teardown of a real structure all need real resources and a persisted flagpole in the shared test world.
   The Init tier covers the DECISION (`DecideRaise`, pure), the registration, the ceiling latch, the teardown of
   fixture DEPLOYMENTS and the dismantle rule - not the raise itself. **Play-test it.**
4. ⏳ **THE AFFORDABILITY FIRST CLAIM IS NOT DRIVEN.** Proving it needs a pool that can afford sabotage and not
   a forward base, which means real resources in the shared world. The reasoning is in
   `ClaimOperationInterval()`'s header and the injection that would break it (deleting the walk's
   `m_bOperationIntervalClaimed` check) has **no covering case** - recorded as a known gap.
5. ⏳ **Save/Continue in the forward-base phase.** The Persistence relink case covers the record, the re-link
   and (new this phase) the module re-adoption, but only through the reload seam; a real Continue is the only
   thing that exercises the structure and the deployment marker coming back independently.
6. ⏳ **The dismantle penalty now comes off the asset module** rather than from a second reading of the
   difficulty setting. Same number for the shipped config, and no case drives the subtraction.

### Notes for the orchestrator

- **Nothing was committed.** All Phase-5 work is uncommitted, on top of Phases 1-4.
- `git diff` on `Virtualization/`, `VirtualMovement/`, `api.md`, `Configs/Difficulty/` and
  `Scripts/Game/GameMode/Deployments/` is **empty**. `Configs/Deployment/` is byte-identical to what Phase 4
  left. `Scripts/Game/Controllers/OccupyingFaction/` is still exactly T1.3's two call sites.
  `OVT_CampaignRequestComponent.c` has **no diff at all** and `OVT_DismantleEnemyFOBAction.c` carries only
  Phase 1's prose rename.
- `tools/` carries only the orchestrator's own `run-tests.sh` change; `.claude/` is untouched.
- **The director shrank 6,225 → 4,926 lines (-1,299).** See deviation 8 for the arithmetic and for why §4's
  "below 3,000" is not reachable from where the file actually was.
- New files: `Objectives/Modules/OVT_BaseObjectiveAssetModule.c`, `OVT_RaiseForwardBaseObjectiveOperation.c`,
  `OVT_AssetUpObjectiveCondition.c`, `OVT_ReserveAtLeastObjectiveCondition.c`,
  `OVT_DaylightWindowObjectiveCondition.c`, `OVT_AssetStarvedObjectiveAbort.c`.
- 8 new Init cases (5 clone-fidelity, the authored-chain pin, the ceiling/clones case) and 1 strengthened
  Persistence assertion. No assertion anywhere was weakened.

---

## Phase 5 gate — All `{6A6E2A002F53A581}`: **435 / 438** (2026-08-21), one red mine and expected

| Red | Whose | Disposition |
|---|---|---|
| `…ObjectiveFramework_ARegistryResolvesAndValidates` — *"phase 'ForwardBase' carries 12 module(s); while it is still a strangler shim it authors exactly one condition shim and one operation shim"* | 🔴 **Mine** | Phase 4's ⚠ TEMPORARY `AssertShimPair()` was still wired to `ForwardBase`. **Repaired** — see below |
| `…CompositionSlotGate_AcceptedTypesMatchTheCompositions` | Not this feature's | Pre-existing `core/damage` leftover, red since before Phase 1 |
| `…BaseRepair_AConfigResolvesAndIsOrdered` | Not this feature's | Timed out as the **first case in the Init suite**, `Output: <none>`, 2 s after the suite's scenario change - the same harness artifact that hit the first Logic case earlier in this feature and then passed on re-run |

### 🔴 THE MISS, AND THE RULE IT PROVES: MOVING THE SHIM ASSERT IS PART OF THE BUILD PHASE

`AssertShimPair()` is the ⚠ TEMPORARY method Phase 4 wrote with "loses one caller line per build phase
and dies with the last shim" in its own header. **Phase 5 is the phase that moves the `ForwardBase` line,
and it was missed** — so the case reported the real, correct twelve-module set as a broken shim pair. A red
that says nothing about the product.

**It is the third time a count in this one case has gone stale** (Phase 4's gate found two of them), and the
reason is always the same: *a module count can neither tell two phases apart nor say which rule went
missing.* The repair therefore replaces the line rather than re-tuning a number.

- `AssertShimPair()` now has **exactly one caller**, `CounterAttack`, and its header says so. Build phase 6
  deletes the method with the last shim.
- **`AssertForwardBasePhase()` + `AssertForwardBaseChain()` are new, and every assertion names a TYPE and a
  POSITION.** The five-operation chain in order (raise → garrison → tower → ladder → sabotage), the garrison's
  forward-base resolver, the ladder at position four and not five, all four gate conjuncts (asset-up, the
  per-doctrine ramp measure, the reserve gate, the daylight window), **the `OVT_IdleForObjectiveAbort`**, the
  starvation abort, and a refusal if the phase ever carries a shim again.
- 🔴 **THE ABORT ORDER IS NOW A PINNED CONTRACT**, and it was not asserted anywhere before. The abort fold
  takes its reason and its blacklist flag from the FIRST module that fires, and a forward base cut off for
  its whole budget has almost certainly also stopped making progress — so both can fire on the same tick.
  Starvation first is what puts *"its forward base was cut off"* in the campaign log instead of *"the
  forward-base phase did nothing at all"*: the difference between telling a player their counterplay worked
  and telling them nothing happened.
- **`AssertRampSpansIntoTheForwardBase()` is new and is asserted ONCE**, not per plan, because it is a fact
  about the DEPLOYMENT configs. It checks `m_sFromPhase "Harassment"` **and** `m_sThroughPhase "ForwardBase"`
  on tower recapture, sabotage and all four ladder rungs. With half one (the repeated operations) in
  `AssertForwardBasePhase()`, **both halves of the T5.7 deadlock fix are now asserted in the one case** — and
  either half alone leaves the ramp dead.

⚠ **Correction to this phase's own write-up:** each ForwardBase phase authors **twelve** modules, not
thirteen (5 operations, 5 conditions counting the kind guard, 2 aborts). The Workbench check in `tasks.md`
was corrected with it.

⚠ **No assertion anywhere was weakened to reach green.** The repair adds ten claims to a case that
previously made one, and the one it made was about a count.

---

## Phase 6 — the battle as a terminal operation, and the last shim dies (2026-08-21)

### 🔴 POLL, DO NOT SUBSCRIBE — THE RULE RESTATED WHERE IT NOW LIVES (T6.1, T6.7)

`OVT_StartBattleObjectiveOperation.TryAct()` calls `StartBaseQRF` / `StartTownQRF` and **returns**. It never
subscribes to anything, and the reason is in the class header so that nobody "improves" it into a callback:

> **The occupying faction manager's own finish handlers DELETE THE BATTLE CONTROLLER'S ENTITY from inside the
> invoker's own dispatch.** A second subscriber ordered after them runs against a deleted entity, and the crash
> is in the engine's dispatch rather than anywhere a stack trace would point.

**The poll costs no code at all, and that is the point.** It is `DirectorTick()`'s *third early return*: while
`m_CurrentQRF` is set the tick never reaches any module, so the phase is frozen rather than spinning, and the
FIRST tick on which the slot is empty again is the first tick that reaches `TryAct()`. Finding the module's own
"a battle was started" latch set and the slot empty is therefore exactly "the battle this objective started has
resolved". One null check per in-game minute cannot be got wrong.

⚠ **`grep -rn "m_OnFinished" Scripts/Game/GameMode/Objectives/` being empty is a Definition-of-Done criterion**,
and it is the mechanical half of this rule.

### 🔴 THE BATTLE PHASE AUTHORS A CADENCE OF **ZERO**, AND IT IS NOT A TYPO

This is the one deviation from "both shipped plans author `-1` on every tuning field", and it is forced by the
poll above. Every other phase spends on an interval; this one spends nothing and is *waiting*.

| Cadence | What happens after the battle resolves |
|---|---|
| `-1` (the difficulty interval) | The runner's cadence gate refuses to ask the operation again until the countdown elapses — **up to 60 in-game minutes on Normal**. The finished objective stands there holding the machine's one objective slot, the deployment bias and the reserve seam for all of it |
| `0` ("every in-game minute") | The very next tick asks, sees the empty battle slot, and ends the objective — the same one-tick gap every other ending in this machine has |

`ResolveOperationCadence()`'s header already called an authored zero "a legal, if aggressive, authoring gesture";
this is what it was for. `AssertPhaseHeader()` now pins the cadence **per phase** (`-1`, `-1`, `0`) rather than
asserting one sentinel for all three, so a revert to `-1` is a red test with that reasoning in its message.

⚠ **The alternative — teaching the runner to ask terminal operations every tick — was rejected.** It would put a
second meaning on `IsTerminal()` (today: "this phase can end by acting", read by the validator) and add a rule to
the modder-facing contract to describe one shipped phase. The authored zero says the same thing in the data.

### T6.2 — one completion path, and where each arm of it lives (R1)

`ResetObjective(reason, blacklist)` is **byte-unchanged**. What Phase 6 did was make both arms of the battle
phase reach it from authored modules:

| Arm | Module | Blacklists? | Why |
|---|---|---|---|
| The battle **resolved** | `OVT_StartBattleObjectiveOperation`, on the poll | **No** | A resolved battle is not a failure of the objective. The occupying faction spent its ramp; whether it took the place or not, THIS objective is finished, and the place is re-evaluated on its merits next round |
| **No battle could be started at all** | the phase's `OVT_IdleForObjectiveAbort`, when the clock runs out | **Yes** | The place just failed. Picking it again immediately would fail the same way |

🔴 **A WIN AND A LOSS TAKE THE SAME PATH BECAUSE THE DIRECTOR IS NEVER TOLD WHICH IT WAS.** The campaign's battle
slot reports no outcome to this component and is not asked for one — `m_CurrentQRF` simply goes null. So R1's
"win → re-select, loss → reset with blacklist" is satisfied by the two arms above rather than by inspecting an
outcome that does not exist at this seam: the *resolution* re-selects on the next tick, and the *failure to start*
blacklists. Both are reported the same way, through one method, which is what lets a plan end on **any** module
rather than only on a battle — `AdvanceObjectivePhase()` already ends a plan that runs out of phases through the
same call, and any abort module can too.

### T6.3 — the daylight condition was already whole, and the `m_Time` trap is recorded where it can still bite

`OVT_DaylightWindowObjectiveCondition` landed complete in **Phase 5** (the gate had to be decomposed before the
starter could move), so T6.3 was a verification task: `IsCounterAttackWindow(hour, m_iStartHour, m_iEndHour)` with
05:00/15:00 as attribute defaults, the clock handle resolved lazily and cached, and the wait announced **once per
phase entry** by the runner's `LogIdleClockHold()` rather than by the module.

🔴 **THE MODULE DELIBERATELY DOES NOT USE `m_Time`, AND THAT IS NOT AN OVERSIGHT.** `m_Time` is a field on
`OVT_Component`, filled by that class's own `OnPostInit`. **A MODULE IS NOT A COMPONENT**: it is cloned per phase
entry and has no `OnPostInit`, so it carries `m_Clock` and resolves it on the first tick that asks.

⚠ **And the trap the plan warned about is now recorded in the director itself, where the temptation lives.** The
director no longer reads the world clock at all — `ResolveWorldHour()` went with the gate — so the comment that
used to explain the inherited handle now explains what would happen if anyone re-added one: **re-declaring
`m_Time` on a subclass shadows the base's copy with one nothing ever fills**, and every reader in this feature
treats an unreadable clock as "no restriction", so the daylight gate would silently stop existing.

### T6.4 — the per-phase anchor radius became authored data, and the sentinel stopped being positional

`AnchorRadiusForPhase(phase)` was a hard-coded lookup keyed by the legacy enum: 600 m for harassment, 1200 m from
the forward base onward. Both plans now author their own three (**600 / 1200 / 1200**) and the method is gone.

🔴 **`-1` HAD TO CHANGE MEANING, AND THE PLAN'S WORDING FOR IT ("today's per-phase value") IS NOT AVAILABLE ONCE
THE LOOKUP IS DELETED.** "Today's value" was a function of the phase INDEX, which is exactly the enum coupling
this phase removes. It now means one flat `DEFAULT_ANCHOR_RADIUS` (600 — the tight one, because a bias nobody
chose the reach of should lean on as little of the map as possible). Two consequences, both landed:

- **The registry's `PORTED_ANCHOR_PHASES` rule is gone.** It existed to refuse a fourth phase authoring `-1`,
  because such a phase would have silently inherited the third's radius. With a flat default there is nothing to
  inherit wrongly, and the rule would have been a lie.
- **`AssertPhaseHeader()` now requires a POSITIVE authored radius per phase** instead of requiring the sentinel,
  and a new `AssertAnchorReachWidens()` asserts the SHAPE the two deleted constants' comments carried: tight while
  the doctrine may still re-select, wider once it is committed, and the battle sharing the forward base's figure
  because the ground being fought over does not shrink when the fighting starts. ⚠ **The numbers themselves are
  not asserted anywhere** - a tuner may move 600 and 1200 without reddening a case, which is the same rule
  `…ObjectiveAnchor` already stated for itself.

### T6.5 — what the last shim took with it

| Deleted | Note |
|---|---|
| `OVT_LegacyPhaseObjectiveOperation.c`, `OVT_LegacyPhaseObjectiveCondition.c` | the class FILES, so a reverted `.conf` cannot author one — it fails to resolve the class instead |
| `RunLegacyPhaseTick(int)` | the one promoted entry point. Nothing on the director is package-visible for the strangler's sake any more |
| `PhaseOwnsItsOwnClocks()` and the `shimOwnsTheClocks` branch of the runner | the cadence and the idle clock are the runner's for EVERY phase now, with no carve-outs |
| `TickCounterQRF()`, `StartCounterAttackNow()`, `m_bCounterAttackStarted` | the battle phase's hard-coded tick and Phase 5's temporary starter |
| `EvaluateCounterAttackGate()`, `MeetsCounterAttackRamp()`, `IsCounterAttackDaylight()`, `ResolveWorldHour()`, `LogCounterAttackRefusal()`, `StartCounterAttackOnBase()`, `StartCounterAttackOnTown()` | the three-answer gate and both starters |
| `IsWaitingForCounterAttackDaylight()`, `IsCounterAttackReady()` | ⚠ **two PUBLIC readers with no production caller.** They wrapped the deleted gate; the one place that read them was an Init case, which now asks the phase's own conditions — a stronger claim, because it asserts the decomposition rather than a second implementation of the same arithmetic |
| `COUNTER_ATTACK_HOUR_START` / `_HOUR_END` / `_NOT_READY` / `_WAIT_FOR_DAYLIGHT` / `_FIRE`, `BASE_OP_RESOLVE_RADIUS`, `HARASSMENT_ANCHOR_RADIUS`, `FORWARD_ANCHOR_RADIUS` | eight constants whose values are attribute defaults or authored numbers now. Their REASONING moved with them, into the module headers and the attribute `desc:` strings |
| `AdvanceObjectiveTimers()` | ⚠ **not in the plan's deletes list**, but its only caller was `TickCounterQRF()`. `AdvancePhaseTimeout()` and `AdvanceOperationCadence()` both keep live callers and stay |
| `RunUnplannedObjectiveTick()`, `SelectWithoutAPlan()` | the two strangler fallbacks — see the closed ledger above |

### ⚠ `AssertShimPair()` DIED HERE, AND WHAT REPLACED IT ASSERTS TEN THINGS INSTEAD OF ONE

Phase 4 wrote `AssertShimPair()` with "loses one caller line per build phase and dies with the last shim" in its
own header; Phase 5's gate proved what happens when the line is not moved (the real twelve-module ForwardBase set
was reported as a broken shim pair — a red that says nothing about the product). Phase 6 is the phase that
deletes the method, and `AssertCounterAttackPhase()` is what stands in its place.

**Every claim it makes names a TYPE and a POSITION. There is no module count anywhere in the file any more** —
`EXPECTED_SHIM_MODULES` went with the method, and the case header now says why a count may never come back: a
number can neither tell two phases apart (ForwardBase and CounterAttack were BOTH "2" while both were shims) nor
say which rule went missing. Three separate stale-count reds in this one case is the evidence.

What it asserts:

1. exactly **one** operation, and it is `OVT_StartBattleObjectiveOperation`;
2. it declares **`IsTerminal()`** — without which the registry's wedge rule skips **both shipped plans** at world
   start, because this phase authors no advance condition by design;
3. its mode is **`COUNTER_ATTACK`**, not the announced `STANDARD` battle a captured base raises;
4. its base resolve radius is positive (at zero, every base doctrine refuses its own battle);
5. the **`OVT_IdleForObjectiveAbort`** is present — since Phase 4 a phase with no abort module cannot time out at
   all — and that it **blacklists**, because this is the phase's failure arm;
6. 🔴 **that the phase carries NO condition module at all**, which is the rule a modder is most likely to break.

**Point 6 is where the plan's "assert the four gate conjuncts including the daylight window" landed, and the
shape it landed in is deliberate.** The four conjuncts (asset-up, the per-doctrine ramp measure, the reserve gate,
the daylight window) live on the phase BEFORE this one — Phase 5 decomposed the gate onto `ForwardBase`, where
being satisfied ADMITS the objective to the battle phase — and `AssertForwardBasePhase()` already asserts all
four by type. Asserting them a second time here would produce two failure messages for one fault. What is genuinely
new, and what a `.conf` author would get wrong, is the inverse: **a condition on the LAST phase does not gate the
battle.** Conditions ADVANCE, and advancing off the end of a plan ENDS the objective without blacklisting it — so
authoring the daylight window here, which reads like the obvious way to say "fight in daylight", would end the
objective at 05:00 with no battle and no failure recorded anywhere.

### GUIDs authored (prefix `{6BA1…}`, verified free before authoring)

| GUID | What |
|---|---|
| `{6BA10000000000A0}` / `{6BA10000000000A1}` | Town Offensive's battle operation and idle abort |
| `{6BA10000000000B0}` / `{6BA10000000000B1}` | Base Offensive's, same shape |

The four shim GUIDs (`…0027`/`…0028`, `…0037`/`…0038`) are **retired rather than reused**, as Phase 5 retired
its four, so a reader diffing the file never sees a GUID change meaning. Both CounterAttack phase GUIDs
(`…0026`, `…0036`) are unchanged — it is the same phase, re-authored.

### Can-fail proofs recorded this phase (injected one at a time; every one exited `compile-check.sh` 0, every one restored)

| # | Fault injected | Case it reddens |
|---|---|---|
| P1 | `clone.m_eMode` dropped from `CloneModule()` | `…ObjectiveFramework_CBattleOperationClonesEveryAttribute` |
| P2 | `IsTerminal()` answers false | `…ObjectiveFramework_ARegistryResolvesAndValidates` (the terminal row) **and** `…_BValidatorNamesAndSkips…`'s wedge rule fires on both shipped plans |
| P3 | the battle phase's `m_bBlacklist` authored `0` in both plans | `…ObjectiveDirector_TerminalPhaseEndsTheObjectiveOnOnePath` (the blacklist delta) |
| P4 | the poll branch removed — `TryAct()` returns false instead of resetting when the latch is set and the slot is empty | `…ObjectiveDirector_GateWaitsForDaylightThenFiresOnce` ("the battle resolved and the very next tick did not end the objective") |
| P5 | the battle phase re-authored `m_iOperationCadence -1` | `…ObjectiveFramework_A` (the per-phase cadence row) |
| P6 | the battle phase re-authored `m_fAnchorRadius -1` | `…ObjectiveFramework_A` (the "authors no reach at all" row) **and** `…ObjectiveAnchor_DirectorPushesPerPhaseAndDropsOnEveryExit`'s precondition |

⚠ **"It went red" here means a reasoned trace plus a compile-clean injection, not an observed suite run** —
running the suites is the orchestrator's job (`.claude/test-policy.md`), the same standard Phases 2–5 recorded
their proofs to.

### 🔴 What the Init tier CANNOT prove about this phase

1. ✅ **Compile** — `tools/compile-check.sh` → 0 (6235 files), and every can-fail injection above also exited 0.
2. ⏳ **A REAL BATTLE, FOUGHT TO A RESULT.** The Init tier starts one battle and then *empties the campaign's
   battle slot by hand* to simulate the end of it. What it cannot drive is the real thing: the silent deploy, the
   30-real-minute muster, the fight, and the manager's own finish handler deleting the controller entity — which
   is the very dispatch the poll-not-subscribe rule exists for. **Play-test it**, and watch for the objective
   ending on the first in-game minute after the battle resolves rather than an hour later.
3. ⏳ **A SECOND BATTLE ON THE SAME OBJECTIVE.** The "exactly once" claim is asserted against the campaign's
   single-battle handle over four synchronous ticks; a live campaign has a 30-minute muster in the middle of it.
4. ⏳ **THE ANCHOR RADII IN PLAY.** Both `.conf`s now carry three numbers where they carried three sentinels, and
   a mistyped one is a wider or narrower deployment bias with no symptom but a faction garrisoning the wrong
   ground. The Init case asserts the relationship (600 < 1200, the two committed phases equal, all positive) and
   that the pushed reach IS the authored one — but only the authored one, not that 1200 is right.
5. ⏳ **Save/Continue in the BATTLE phase.** The battle module's two latches are runtime-only and deliberately not
   persisted, so a restored objective sitting in the battle phase starts a NEW battle on its first tick — which is
   exactly what the hard-coded machine did (`m_bCounterAttackStarted` was cleared with the record and never
   saved). Nothing drives that path.

### Notes for the orchestrator

- **Nothing was committed.** All Phase-6 work is uncommitted, on top of Phases 1–5.
- `git diff` on `Virtualization/`, `VirtualMovement/`, `api.md`, `Configs/Difficulty/` and
  `Scripts/Game/GameMode/Deployments/` is **empty**. `Configs/Deployment/` is byte-identical to what Phase 5
  left. `Scripts/Game/Controllers/OccupyingFaction/` is still exactly T1.3's two call sites.
- **The director shrank 4,926 → 4,383 lines (-543).** §4's "below 3,000" was written against the 5,201-line file
  and remains unreachable for the reason Phase 5 recorded: Phases 2–4 added ~1,000 lines of runner, plan registry
  and plan-driven selection before the deletions started. The remaining bulk is the runner's kept methods and the
  reasoning in their headers, which §1 explicitly forbids stripping to hit a number.
- **`OVT_TEST_Init_ObjectiveReserve.c` was NOT edited.** §4 Phase 6's "re-points" line predicted two, but the file
  names no deleted symbol and its `…DirectorFloorsOnlyWhileBrokeAndLapsesWhenItStopsAsking` already asserts T6.6's
  reserve claim exactly — floored on a refused ask, lapsed on the next tick that does not ask. Editing it would
  have been churn.
- **`OVT_TEST_Init_ObjectiveAnchor.c` WAS re-pointed and §4 did not predict it** — it was the only other caller of
  `AnchorRadiusForPhase()`, in four places.
- The two shim clone-fidelity cases (`…Framework_C…`, `…Framework_D…`) were **replaced by one** for the battle
  module, which keeps the file's one-clone-case-per-clonable-class rule intact. Case letters are now A, B, C, E,
  F, G — the gap is deliberate: re-lettering E, F and G would move them in the run order for no reason.

---

## ⚠ 2026-08-21 — the first case of a suite can report a bogus `timeout`

**Signature, seen twice, in two different suites:** the **alphabetically first** case of a suite reports
`FAILURE / Failure reason: timeout` with `Output: <none>`, **seconds** after the suite requests its scenario
change rather than at its declared 30 s budget — and passes on the next run with nothing about it changed.

| Run | Case | Suite | Outcome next run |
|---|---|---|---|
| Phase 2 gate, 1st | `OVT_TEST_Logic_BaseDefenseConversion_FundingSplitConservesTheTotal` | Logic | ✅ passed |
| Phase 5 gate, 1st | `OVT_TEST_Init_BaseRepair_AConfigResolvesAndIsOrdered` | Init | ✅ passed |

Both cases are incapable of hanging — the first is pure integer arithmetic, the second resolves a config. Neither
had been touched by the phase whose gate reported it. The harness appears to lose the first case while the world /
scenario is still coming up.

🔴 **This is the one place where `maxAttempts` being banned bites us**: the suites are supposed to be
deterministic, so a red is meant to be information. A red with THIS EXACT signature — first case of its suite,
`Output: <none>`, failing at suite startup — is the known exception. **Check the signature before believing it,
and never widen the exception**: any other timeout, or this one in a case that is not first, is real.

Suspected aggravator: the addon now loads through a **junction** (see the tooling section above), and the extra
drvfs indirection slows world load. If it becomes frequent, prefer a fix that lets the client load this tree
without a junction over absorbing the flake.

---

## Phase 6 gate — All `{6A6E2A002F53A581}`: **437 / 438**, green on the first run (2026-08-21)

The only red is the pre-existing `CompositionSlotGate` leftover. **The strangler seam is gone**, and the
orchestrator verified the teardown by name rather than trusting the Definition of Done's single grep:

| Grep | Result |
|---|---|
| `OVT_LegacyPhase` in `Scripts/ Configs/` | empty |
| `m_iLegacyPhase` in `Configs/` | empty |
| `FireCounterAttack\|EvaluateCounterAttackGate\|AnchorRadiusForPhase\|TickCounterQRF` | empty |
| `m_OnFinished` under `Objectives/` | empty — **poll-not-subscribe holds** |
| **the temporary-path ledger**: `RunUnplannedObjectiveTick\|SelectWithoutAPlan\|StartCounterAttackNow\|m_bCounterAttackStarted` | **empty — closed** |
| `Scripts/Game/Controllers/OccupyingFaction/` | exactly the pre-existing 2 lines; the QRF layer was consumed as-is |

**Why the ledger was checked separately:** §6's Definition of Done greps only for `OVT_LegacyPhase`. That would
have passed with any of the three fallback paths stranded in the tree — they were added one per phase (P2, P3, P5)
to keep the machine running behind the seam, and nothing in the plan tracked them. All three are now gone.

**Director line count: 4,383** (5,201 at feature start; 6,225 at peak after Phases 2–4; −543 this phase).
§4's "below 3,000" and §1's "2,000–2,200 end state" are **not met and will not be** — see the Phase 5 record for
the arithmetic. The forward-base, harassment and counter-attack blocks are all genuinely gone; the runner simply
grew more machinery than the plan's estimate assumed.

### Deviations accepted

1. **The battle phase authors `m_iOperationCadence 0`.** The runner only asks operation modules once the cadence
   has elapsed, so a polling phase on the difficulty interval could be re-asked up to 60 in-game minutes after its
   battle ended — leaving a finished objective holding the slot and the anchor bias. `AssertPhaseHeader()` now
   pins cadence per phase (`-1`, `-1`, `0`).
2. **T6.4's `-1` had to change meaning.** "Today's per-phase value" was a function of the phase *index* — the very
   enum coupling being deleted. Both plans now author 600/1200/1200 and `-1` means one flat
   `DEFAULT_ANCHOR_RADIUS` (600). The registry's now-false `PORTED_ANCHOR_PHASES` rule went with it.
3. **`AssertCounterAttackPhase()` asserts the inverse of the gate, not the gate.** Phase 5 decomposed the gate onto
   the *ForwardBase* phase where it is already asserted by type; a second copy would give two messages for one
   fault. What it asserts instead is genuinely new and is exactly how a modder would break this: **the battle phase
   must carry no condition module at all** — a condition on the last phase advances off the end and ends the
   objective *without a battle*.
4. `AdvanceObjectiveTimers()` deleted (not on the plan's list) — its only caller was `TickCounterQRF()`.
   `AdvancePhaseTimeout` / `AdvanceOperationCadence` keep live callers and stay. Two zero-caller public readers,
   `IsWaitingForCounterAttackDaylight()` and `IsCounterAttackReady()`, went with the gate they wrapped.

### 🔴 Owed to Phase 7 — the enum is still alive

`OVT_EObjectivePhase`, the three `LEGACY_PHASE_*` constants and their three mapping methods are **still in the
tree**, because the GM wire still carries `m_iObjectivePhase` as an int (T7.1 is what replaces it). The Phase-2
ledger's wider grep is therefore **not** empty yet, and that is expected rather than a miss.


---

## Phase 7 — validation, presentation and the admin surface (2026-08-21)

### 🔴 THE ENUM IS GONE. `grep -rn "OVT_EObjectivePhase\|LEGACY_PHASE_" Scripts/ Configs/` returns ONE comment

`OVT_EObjectivePhase`, `LEGACY_PHASE_HARASSMENT` / `_FORWARD_BASE` / `_COUNTER_ATTACK`,
`PlanIndexForLegacyPhase()`, `LegacyPhaseForPlanIndex()`, `LegacyPhaseForSavedName()` and
`OVT_ObjectiveRecord.phase` are all deleted. T7.1 removed the last consumer (the Game Master wire) and
the rest followed in the same pass, which is what the Phase-6 record said was owed here.

**What replaced each reader, because none of them was a one-line swap:**

| Was | Now | Why not a rename |
|---|---|---|
| `EnterPhase(OVT_EObjectivePhase)` | `EnterPhase(string phaseName)` | The plan is the only thing that knows what phases exist. An unknown name is REFUSED with an ERROR naming the plan, never silently entered as phase 0 - phase 0 is a real phase and is the first one |
| `GetPhase()` | `GetObjectivePhaseName()` / `GetObjectivePhaseIndex()` (both already existed) | ~40 test sites moved to the name; an idle director answers `""` where it answered `IDLE` |
| `ConsumeReselectRequest`'s `phase != IDLE && phase != HARASSMENT` | `GetObjectivePhaseIndex() > 0` | The rule was always "past the first phase, the objective is locked". No objective answers -1, the first phase answers 0, so the boundary is identical for the shipped plans and CORRECT for a modded one |
| `ResolveRestoredObjective`'s `phase == COUNTER_QRF` | `RestoredPhaseIsTerminal()` - any runtime module that is an operation with `IsTerminal()` | "Was it saved mid-battle" was never about the number 3; it is about a phase that ENDS the objective and whose battle is not persisted. A plan whose terminal phase is its second gets the same roll-back |
| `AdoptPersistedPhase`'s no-registry `LegacyPhaseForSavedName(name) == IDLE` discard | discard only when the saved phase NAME is empty | With no registry there is nothing to check a name against, and refusing every name this build has not shipped would abandon a modded campaign's objective on every load. ⚠ **This is a deliberate behaviour change** in the registry-less restore path only, and nothing covers it either way |
| `OVT_ObjectiveRecord.phase` | nothing - the instance's index + authored name is the single source of truth | Every writer of the field already called `RecordPhase()` beside it; the field was a second copy of the one fact the whole machine steps on |

⚠ **ONE HIT SURVIVES AND IT IS A COMMENT IN A FROZEN FILE.** `OVT_QRFModes.c:21` names the enum in prose
("unlike `OVT_EObjectiveKind`/`OVT_EObjectivePhase` these two MAY be renumbered"). `Scripts/Game/
Controllers/OccupyingFaction/` is behaviourally frozen to T1.3's two lines, and this phase did not spend
that budget on a comment. **The one-line fix, for whoever unfreezes it:** delete `/OVT_EObjectivePhase`
from that sentence.

### THE GM WIRE: THREE EDITS ON THE STATE, FOUR ARGUMENTS ON THE RECORD

🔴 **A NEW OR CHANGED SCALAR ON `OVT_GMCampaignState` NEEDS THREE EDITS AND THE ONE WITH NO SYMPTOM IS
`Clear()`.** The declaration, `CopyFrom()` and `Clear()`. Missing from `CopyFrom` the row never fills and
somebody notices in a minute; missing from `Clear` a second campaign in the same client session opens
showing the PREVIOUS campaign's value, which is wrong-looking rather than empty and has no other symptom.
`CopyRecords()` is for the four per-entity arrays only and neither field belongs there. Two fields landed
this phase (`m_sObjectivePlanName`, `m_sObjectivePhaseName`, replacing `m_iObjectivePhase`), so six edits,
and `OVT_TEST_Init_GMCampaignState_CarriesAndClearsEveryScalar` asserts all six.

⚠ **The file is `OVT_GMCampaignState.c`, NOT `OVT_GMRecords.c`.** §4's acceptance criterion
"`git diff …/OVT_GMRecords.c` → empty" and T7.1's "three edits at `:97-102`, `:208`, `:267`" only LOOK
contradictory: every one of those line numbers is in `OVT_GMCampaignState.c`, and `OVT_GMRecords.c`
carries the four per-entity record classes, none of which mentions the objective. **Both hold as written**
and `git diff OVT_GMRecords.c` really is empty.

**`WIRE_VERSION` 2 -> 3, and the arity hand-count (BUG-090).** The record COUNT did not change
(`CAMPAIGN_RECORD_COUNT` is still 3), so nothing about a mismatched client's arithmetic would have looked
wrong - it would simply have read a string off an integer slot. The version bump is the only thing that
makes that loud. Arity, counted by eye at all three sites and recorded in the method header:
`Rpc(RpcDo_CampaignObjective, seq, name, planName, phaseName)` = **4** payload arguments; the local
short-circuit `RpcDo_CampaignObjective(seq, name, planName, phaseName)` = **4**; the handler
`RpcDo_CampaignObjective(int, string, string, string)` = **4**.

### THE PANEL HAS TWO ROWS AND THREE THINGS WORTH SAYING

`FormatObjectivePhase(int)` and its three shipped-phase keys are gone. Its successor is
`OVT_GMPanelFormat.FormatPhaseRow(planName, phaseName)` - **renamed, not merely re-signatured**, because
the acceptance grep is a substring match and `FormatObjectivePhase` had to stop appearing anywhere.

- The phase row now reads **"Town Offensive: Harassment"** - the plan and the phase, both authored, both
  verbatim. No `.layout` change: the plan rides in the existing phase row rather than a third widget.
- 🔴 **A '#'-KEY NAME IS NEVER CONCATENATED.** A `#`-prefixed string is resolved by the widget only when
  it is the WHOLE string it was handed, so a mod authoring its phase names as localization keys gets the
  key alone rather than a plan label with a raw key beside it. That branch is the one thing in the
  formatter that is not obvious, and the Logic case has two rows for it.
- **Both surviving keys kept their meaning and gained a distinction:** empty plan AND empty phase is
  `PhaseNone` ("there is no objective"); a plan with NO phase name is `PhaseUnknown` (client and server
  builds differ, or the plan did not resolve). Reporting the second as "none" tells a Game Master the
  campaign has no target while it is being attacked.

### 🔴 LOCALIZATION: THE `.st` IS EDITED, THE `.conf`s ARE NOT, AND A RE-EXPORT IS OWED

Three `CustomStringTableItem` blocks were deleted from `Language/localization_Overthrow.st`
(`…PhaseHarassment`, `…PhaseForwardBase`, `…PhaseCounterAttack`), and two surviving Comments were
re-written to describe the authored-name row. **Brace balance verified before and after: 1796/1796 ->
1790/1790**, three blocks x two braces.

⚠ **`Language/*.conf` ARE WORKBENCH BUILD OUTPUT AND WERE NOT TOUCHED**, so `grep` for the three removed
keys still finds them in all seven `.conf`s. That is expected and is the ONLY part of §4's key grep that
does not come back empty - **the re-export from Workbench is owed** and is on the human-verification list.
`git diff Language/` shows `localization_Overthrow.st` and nothing else.

**Also fixed while in the file:** `:3800`'s translator Comment fact-checked a Field Manual page against
`Configs/Deployment/Deployment_ObjectiveRepair.conf`, a path Phase 1 renamed. It now names
`Deployment_BaseRepair.conf`.

### T7.4 — the admin verbs were VERIFIED, not rewired

`git diff Scripts/Game/Components/Controller/OVT_AdminCommandsComponent.c` is **empty**. `/give-resources`
and `/tick-resources` were never in the director: they credit `OVT_OccupyingFactionManager` and reach the
objective machine only through the shared pool, so R8's "keep working against the instance" needed no
edit. The play-test confirmation is on the human-verification list.

**What DID need checking is the refusal-log dedup**, whose callers now live inside modules. It still
de-duplicates on the `(config, reason)` pair across both doors - the internal choke point and the public
`LogObjectiveRefusal()` the raise module uses - and that is now asserted rather than argued.

### T7.4b — the two zero-caller getters got a case instead of a deletion

`GetLoggedRefusalCount()` and `HasLoggedRefusal()` had no caller anywhere in the repo, tests included.
They are the only way to interrogate a RUNNING campaign about why its ramp is quiet without reading the
log back, so `OVT_TEST_Init_ObjectiveDirector_TheRefusalLedgerDedupsAcrossItsCallers` gives them their
first callers and asserts the dedup end to end: a repeat is silent, a new reason speaks, a different
operation speaks, a pair that was never recorded is not claimed, and ending the objective forgets all of
it. ⚠ **The ledger is emptied by ENDING an objective, not by starting one**, so the case resets before it
commits - a refusal latched by an earlier case in the same world would otherwise be counted as one of ours.

### T7.5 — the validator's message contract, stated in the code

Every rule's line now names, **in this order**: the PLAN (added by `ValidateAllConfigs`), the PHASE and
the module slot (added by `ValidateConfig` / `ValidatePhase` as they descend), the ATTRIBUTE by its
**authored field name**, and **what to do about it**. The field name is the load-bearing part: it is the
one piece of the sentence that is identical in the log and in the author's own `.conf`, and it is what
they can search for. The contract is written into `ValidateAllConfigs()`'s header so a rule added later
has to meet it.

### Tests

| Tier | Case | Claim |
|---|---|---|
| Logic | `…GMPanelFormat_ObjectiveRows` (rewritten) | the authored pair, an authored phase alone, both empty states, and TWO `#`-key rows |
| Init | `…GMCampaignState_CarriesAndClearsEveryScalar` (extended) | the objective TRIO survives `CopyFrom` and is dropped by `Clear` |
| Init | `…ObjectiveFramework_HASkippedPlanIsNeverSelected` (new) | a wedged plan authored at priority 1000 - which would out-rank every shipped doctrine - is skipped by selection, and the valid plans still commit |
| Init | `…ObjectiveDirector_TheRefusalLedgerDedupsAcrossItsCallers` (new) | the ledger, through the public door, plus both zero-caller getters |
| Init | `…ObjectiveFramework_BValidatorNamesAndSkipsABrokenPlan` (unchanged) | each validator rule fires on the plan it should - T7.7's first half was already covered |

⚠ **Case H mutates the LIVE registry** (`registry.m_aObjectiveConfigs` is a public attribute) and removes
its fixture plan plus re-validates BEFORE it asserts anything, so a red case cannot leave a wedged
doctrine in the registry for whatever runs next.

### 🔴 What the Init tier CANNOT prove about this phase

1. ✅ **Compile** - `tools/compile-check.sh` -> 0 (6235 files).
2. ⏳ **THE WIRE ITSELF.** `SendCampaignObjective` and its handler are `protected`, so no case can drive
   the send path; the arity is defended by the hand-count above and by `WIRE_VERSION`, not by a test.
   **A host and a client on different sides of this change is the only real proof.**
3. ⏳ **THE PANEL.** Nothing draws a widget in any tier. The row renders raw keys until the localization
   re-export.
4. ⏳ **The admin verbs**, which are a play-test by construction (they are chat commands).
5. ⏳ **A registry-less restore**, whose discard rule changed shape (see the enum table above).

### Notes for the orchestrator

- **Nothing was committed.** All Phase-7 work is uncommitted, on top of Phases 1-6.
- `git diff` on `Virtualization/`, `VirtualMovement/`, `api.md`, `Configs/Difficulty/`,
  `Scripts/Game/GameMode/Deployments/`, `OVT_AdminCommandsComponent.c` and `OVT_GMRecords.c` is **empty**.
  `Scripts/Game/Controllers/OccupyingFaction/` is still exactly T1.3's two lines.
- **No `.layout` work was needed**, as predicted: the phase row is a text field and the plan name rides
  in it.

---

## Phase 7 gate — All `{6A6E2A002F53A581}`: **439 / 440**, green on the first run (2026-08-21)

Only the pre-existing `CompositionSlotGate` red. Case count 438 → 440.

**A framing error by the orchestrator, corrected by the build:** the Phase-7 prompt claimed §4's
`git diff Scripts/Game/GameMode/GM/OVT_GMRecords.c` → empty criterion contradicted T7.1. **It does not.** T7.1's
three edit points (`:97-102`, `:208`, `:267`) are all in **`OVT_GMCampaignState.c`**; `OVT_GMRecords.c` holds only
the four per-entity record classes and genuinely diffs empty. Both hold as written. `tasks.md`'s T7.1 File(s) line
names `OVT_GMRecords.c` and is the thing that is wrong.

**The enum is dead.** `OVT_EObjectivePhase`, the three `LEGACY_PHASE_*` constants, all three mapping methods and
`OVT_ObjectiveRecord.phase` are gone. `EnterPhase(string)` refuses an unknown name with an ERROR rather than
silently entering phase 0; the reselect lock is `GetObjectivePhaseIndex() > 0`; the restore-mid-battle roll-back is
`RestoredPhaseIsTerminal()` (any runtime operation whose `IsTerminal()` is true).

⚠ **One deliberate behaviour change in restore:** in a **registry-less** restore the "unrecognised phase" discard
became "empty phase name". Refusing every phase name this build has not shipped would abandon a *modded*
campaign's objective on every load — the opposite of D2's intent.

### 🔶 One acceptance grep is not literally empty, by choice

`grep -rn "OVT_EObjectivePhase\|LEGACY_PHASE_" Scripts/ Configs/` returns **one** hit:

```
Scripts/Game/Controllers/OccupyingFaction/OVT_QRFModes.c:21  //! ... unlike OVT_EObjectiveKind/OVT_EObjectivePhase these two MAY be renumbered ...
```

It is a **comment**, in a **hard-frozen** folder whose only permitted diff for the whole feature is T1.3's two
keyed-API lines. The freeze is phase-failing; the grep is a verification item. **The freeze wins**, and this
follows the precedent Phase 1 set explicitly: `OVT_QRFModes.c:36` and `OVT_OccupyingFactionManager.c:1737` name
the director in comments only and were "Recorded, not edited."

It is a stale reference to a now-deleted type, so it is worth one line of cleanup **outside this feature's
freeze** — the fix is to delete `/OVT_EObjectivePhase` from that sentence, zero behaviour. Surfaced to the user
rather than done silently.

### Owed

- 🔴 **Localization re-export.** Three `#OVT-GMPanel_ObjectivePhase*` keys were removed from
  `localization_Overthrow.st` (brace balance verified 1790/1790) but the seven `Language/*.conf` exports **still
  list them** — they are Workbench build output and were correctly not hand-edited. The GM phase row renders raw
  until the user re-exports.
- **MP/host pass on the changed snapshot record.** `WIRE_VERSION` 2 → 3; `Rpc()` arity hand-counted as **4** at
  the call site, the local short-circuit and the handler (BUG-090 is a compile-check blind spot). Both methods are
  `protected`, so no test can drive the wire — this is the only defence besides the hand count.

---

## Phase 8 - help & documentation sync (2026-08-21)

**T8.1-T8.4 only.** T8.5 (epic + master rollup) is the orchestrator's. No suite run: this phase is docs and
localization, and the suites assert nothing here. `tools/compile-check.sh` **0 (6235 files)** after the one
`.c` touch below.

### The verdict: PLAYER-FACING BEHAVIOUR DID NOT CHANGE, AND ALMOST NO PLAYER-FACING TEXT NEEDED TO

Phases 1-7 were a strangler with a parity claim, so the honest answer for most sentences was "still true".
The audit was therefore mostly a **citation** pass: the bodies survived, and the translator `Comment`s that
fact-check them named methods, constants and file:lines that no longer exist. That matters more than it
sounds - the Comment is the only thing a future editor has to check a sentence against, and a Comment that
cites deleted code is how a true sentence becomes an invented one at the next rewrite.

**Three in-game surfaces name this machine, and only three:**

| Surface | Verdict |
|---|---|
| `OVT-Tutorial_BasesFirstCapture_Body` (the only tutorial that names it) | Body **unchanged**, every clause re-verified. Comment updated |
| `#OVT-FieldManual_CounterAttacks_*` (6 bodies) | **One body sentence corrected** (below). Five Comments updated |
| `#OVT-FieldManual_BaseCapture_Text5` | Body **unchanged**. Comment updated: three dead citations |

No tutorial anywhere quotes a phase name or a director number, which is now a **requirement rather than a
style choice**: phase names are authored data and a mod may rename them, so a phase name in a popup could
not be kept true.

### The one body correction, and the one C5 hit

1. **`#OVT-FieldManual_CounterAttacks_Text4`** read *"or stay in strength on the forward base itself"*.
   The rule is `OVT_WorldUtils.PlayerInRange(asset.position, difficulty.baseCloseRange)` via
   `OVT_AssetStarvedObjectiveAbort.IsPlayerAtAsset:255-262`: **one** player inside `baseCloseRange`
   (220 m default, `OVT_DifficultySettings.c:70`) starves it. "In strength" overstates the requirement.
   Now reads *"or stay close to the forward base itself"*. ⚠ Every translation of this string still carries
   the old clause.

2. 🔴 **C5, found in a fact-check note rather than in a body.** `#OVT-FieldManual_CounterAttacks_Text6`'s
   Comment asserted in capitals *"MUSTER_TIME_MS = 1800000 ms. THIRTY REAL MINUTES"*. The shipped constant
   is **900000 ms = fifteen** (`OVT_QRFSiege.c:49`, halved 30 -> 15 on 2026-08-20 by the author after the
   first counter-attack play-test). The **body has always said "fifteen real minutes" and was right**; the
   note that exists to protect it was the thing that was wrong, and it would have talked the next editor
   into "correcting" a true sentence. The Comment now says so explicitly.

### The `.c` edit, and the ones deliberately not made

`OVT_StartBattleObjectiveOperation.c:56` - the `m_eMode` attribute `desc:` said COUNTER_ATTACK gives
*"a 30-real-minute muster"*. That string is **modder-facing help** (it is what Workbench shows beside the
field), it is in this feature's own folder, and it carried the same wrong number. Changed to
`15-real-minute`. `compile-check.sh` 0. **Nothing else in any script was touched.**

Left alone and surfaced instead:

- **`Scripts/Game/Controllers/OccupyingFaction/` carries four more stale "30-minute muster" comments**
  (`OVT_QRFModes.c:36,:61`, `OVT_QRFSiege.c:69`, `OVT_QRFControllerComponent.c:142,:245`). The folder is
  hard-frozen for this feature (T1.3's two lines only). They are code comments, not a help surface, and the
  arithmetic examples around them ("1 800 broadcasts") would need recomputing, so they want a one-commit
  sweep outside this feature.
- **"one tick in forty-five"** appears in three module/director comments
  (`OVT_ObjectiveDirectorComponent.c:799`, `OVT_BaseObjectiveOperationModule.c:14`,
  `OVT_SendDeploymentObjectiveOperation.c:58`) as a stand-in for the operation interval. Normal authors
  **60**; 45 is Hard. Internal prose, no help surface reads it, flagged not edited.

### 🔴 THE WIKI INSTRUCTIONS LEFT BY occupying/counter-attacks PHASE 10 CONTAIN TWO WRONG NUMBERS

`docs/features/occupying/counter-attacks/context.md` (T10.3, around `:985-1000`) is the still-unpublished
brief for the wiki pass, and **whoever publishes it will publish two false figures**:

| It says | Shipped |
|---|---|
| the muster window is "1 800 000 ms" / "a thirty **real**-minute unscored muster window" | **900 000 ms, fifteen real minutes** (`OVT_QRFSiege.c:49`) |
| `objectiveQRFResourceGate` "2000/1500/1200/1000/800" | **Easy 750, Normal 750, Hard 1200, Extreme 2000, Insane 3000** (`Configs/Difficulty/Difficulty_*.conf`) |

Its other figures were re-checked and are right (`objectiveStarvationMinutes` 45/30/25/20/15, the ring
100-150, the daylight hours 5 and 15). Two of its structural claims are now stale as well: the daylight
window and the ring are no longer "constants on the director" for the daylight half (it is the authored
`OVT_DaylightWindowObjectiveCondition`, `m_iStartHour 5` / `m_iEndHour 15` on both plans), and the town
support thresholds are authored too (`m_iSupportThreshold 25` on the town plan's ForwardBase phase).
**That file was not edited** - it is a closed feature's record and rewriting another feature's history is
not this phase's job. It is called out here and in the hand-off so the wiki pass corrects it at publication.

### T8.3 - the wiki page was WRITTEN BUT NOT PUBLISHED

The `wikijs` MCP server was **not attached to this session** (confirmed by the orchestrator before the phase
started), so `wikijs_connection_status`, `wikijs_search_pages` and every write were unavailable. Nothing on
https://wiki.armaoverthrow.com was read, created or changed.

The draft is **`docs/features/occupying/objectives/wiki-draft.md`** (229 lines): modder-facing, proposed at
`development-documentation/objective-plans`, covering the registry -> plan -> phase -> module object model,
the selector and candidate-source model, the full module/resolver catalogue with the shipped attribute
values, the `-1` difficulty convention with the five presets tabulated, the per-tick evaluation order, what
the validator catches, and the renaming hazards (`m_sObjectiveName` and `m_sPhaseName` are save keys). Its
header carries the three steps the publisher must take first, including **search before creating**.

### Files changed this phase

- `Language/localization_Overthrow.st` - 1 body sentence, 7 Comments. **Brace balance re-verified 1790/1790**,
  line count unchanged at 16909. No key was added, renamed or removed.
- `Scripts/Game/GameMode/Objectives/Modules/OVT_StartBattleObjectiveOperation.c` - one `desc:` string.
- `docs/features/occupying/objectives/wiki-draft.md` (new), `context.md`, `tasks.md`.
- **`Language/*.conf` untouched**, `Configs/Tutorials/` and `Configs/FieldManual/` untouched (no key changed,
  so no `.conf` needed editing).

### Owed after Phase 8

1. 🔴 **Localization re-export from Workbench.** This is the *same* debt Phase 7 opened, now with a second
   reason: Phase 7 **removed three keys** (`#OVT-GMPanel_ObjectivePhaseHarassment`, `…ForwardBase`,
   `…CounterAttack`) that the seven `Language/*.conf` build outputs still list, and Phase 8 **changed one
   English body** (`#OVT-FieldManual_CounterAttacks_Text4`). Until the re-export, the Game Master phase row
   renders raw keys and the Field Manual shows the old "in strength" sentence. **One export covers both.**
2. 🔴 **Publish the wiki page.** `wiki-draft.md` -> `development-documentation/objective-plans`, after
   searching for an existing objective/counter-attack page and updating that instead if one exists.
3. 🔴 **The counter-attacks wiki brief needs its two numbers fixed at publication** - see the table above.
   That pass was already owed before this phase and is still owed.
4. **Re-translate two strings.** `#OVT-FieldManual_CounterAttacks_Text4` (de/uk/etc. carry the corrected
   clause's predecessor) joins the existing backlog on `#OVT-FieldManual_BaseCapture_Text5` and
   `#OVT-Tutorial_BasesFirstCapture_Body`, both of which still carry **retired-mechanic** text in
   `Target_de_de` and `Target_uk_ua`. Those two are factually wrong in those languages today, not merely
   out of date.
5. **A comment sweep outside this feature's freeze**: the four "30-minute muster" comments in
   `Scripts/Game/Controllers/OccupyingFaction/`, the three "one tick in forty-five" comments, and the
   `OVT_EObjectivePhase` mention at `OVT_QRFModes.c:21` recorded in the Phase 7 section above.
6. **No screenshots exist for the wiki page.** The Game Master panel's new phase row
   ("Town Offensive: Harassment") is the obvious one, and it cannot be captured until the re-export in
   item 1 lands.

---

## Session Notes — 2026-08-21, the autonomous build

All eight phases were built in one `/autorun-feature` run, one agent per phase, with the orchestrator running the
**All** suite once per completed code phase.

**Three phases needed a repair pass**, and every one of them was caused by a Phase-2 assertion doing its job:

| Phase | First run | What the red actually was |
|---|---|---|
| 2 | 416/418 | 🔴 **A real production bug** — the serializer read into differently-named locals than it wrote (Gotcha #14). Both Persistence assertions were correct |
| 4 | 427/431 | Three stale Phase-2 assertions, **no** Phase-4 defect. One exposed that the validator fixture's "wholly valid" control plan had been a wedge since Phase 2 |
| 5 | 435/438 | The ⚠ TEMPORARY shim-pair assert still pointed at a phase that had just been replaced |

**The single most important thing that happened this session was not a phase.** Before Phase 2's gate could be
believed, it emerged that **`run-tests.sh` had never tested this worktree** — see the 🔴 section above. Every suite
verdict recorded for Phase 1 was another checkout's. The tooling fix (junction farm, `OVERTHROW_GAME_ADDONS_DIRS`,
and a file-identity guard replacing a path-matching one) is what made every number in this document mean anything.

**What the build proved, and what it did not.** The Init and Persistence tiers now cover the framework, the
selection fork, the module contracts, the clone fidelity of ~19 classes, the runtime-set-is-clones rule and the
v2 save round trip. They cover **no** MP, **no** real battle fought to a result, **no** real forward base raised,
and **no** `.conf` authoring experience. Those are the human list, and they are not a formality: Phase 5's own
report names one injected fault with no covering case (the affordability first-claim walk).

---

## 🔶 Design decision 2026-08-21 — transport crews KEEP vanilla combat behaviour (do not add HOLD_FIRE)

**Author's call, during play-test.** A transport crew that perceives a dangerous target dismounts and fights
(`SCR_AIVehicleCombatActivity` — a group in a vehicle with `!HasWeapon()`, i.e. a truck, bails the whole crew and
cargo). This became reachable for the first time only when crews were LOD-pinned; at max LOD an agent has no
behaviour tree and perceives nothing, so it never happened before 2026-08-21.

**It was considered and deliberately NOT suppressed.** The lever exists — `SetCombatMode(HOLD_FIRE)` on the crew
group while mounted makes `CustomEvaluate` return 0 — and it is **not to be applied**:

> "I think players may expect a transport crew to fight and it also opens up truck theft, as long as it's not
> binary and too easy. i.e. in Arma 3 you would fire 1 shot at a vehicle and the entire team would jump out
> instantly to fight you so you just steal their tank and run away. For now I'm happy with whatever is the most
> vanilla option and let them deal with it until we start digging into custom Overthrow behaviour trees."

So the objection is to a **binary, instant** bail — not to bailing. A crew that fights is desirable: it meets player
expectation and it makes truck theft a real tactic. The Arma 3 failure mode is the thing to avoid, and fixing it
properly means tuning *when* they bail, which belongs in custom Overthrow behaviour trees, not in a blanket
combat-mode override.

🔴 **Do not "fix" a bailing crew in a future round.** It is intended. The thing that made it dangerous — a bailed
crew leaking as two registered, LOD-pinned men walking home forever — is already handled independently:
`TickReturn` now runs the same uncrewed test as the drive leg (~60 s), stands the crew down and arms the truck's
collection countdown. The vanilla behaviour is safe *because* of that fix, not in spite of it.

Revisit when custom behaviour trees land. Until then: most vanilla option, and let players deal with it.

---

## Play-test round 2026-08-21 (post-commit `1490e65f`) — the insertion convoy, seven faults

The feature was committed as `1490e65f feat: occupying/objectives`. Everything below is a **separate,
uncommitted fix round** driven by the author's live play-test, and it lives almost entirely in
`Scripts/Game/GameMode/Deployments/` and `Scripts/Game/Modded/` — see the freeze note at the end.

**Outcome: the author is satisfied with insertion.** "The last few have gone off without a hitch as designed
(apart from getting stuck but that's just Arma, we have actually reduced the instances of getting stuck a lot
from base game)." Testing has moved on to later phases.

### The root cause nobody would have guessed, and the cascade it started

🔴 **AI agents at max LOD have no behaviour tree.** The per-agent LOD system fires `EOnDeactivate` →
`IsAIActivated = false` at LOD max (**default ≥1000 m**, `SCR_AIGroup.c:118-123`; `SCR_AIToggleMaxLOD.c:90`:
*"prevents AIAgent to change to MAX lod - that disactivates AI"*). It is **not** distance-configurable from any
spawn ring. So a transport crew registered on a 100 km ring was **materialised but inert** — the driver sat in
the cab holding a move waypoint he never executed. Every convoy failed the moment the player was more than a
kilometre away, which is most of the time.

Vanilla solves this for its own long-range convoy: `SCR_ResupplyTaskSolver` calls `PreventMaxLOD()` at boarding
(`:159`) and `AllowMaxLOD()` on **both** completion and failure (`:216`, `:243`), lifting an agent already at max
with `SetLOD(maxLOD-1)` first (`:266-297`). `SCR_AIStaticArtilleryBehavior:93/100` does the same for a crew that
must keep firing unobserved. **Overthrow called neither, anywhere.**

**Fixing it exposed three more faults in sequence**, each invisible while the crews were inert:

1. **Gates.** `SCR_AISelectDoorOperatorAgent:88-92` has a telekinesis branch — at max LOD a gate opens with
   nobody getting out. Once crews were awake they walked to gates like anyone else, and our per-tick
   `SeatEveryone()` sweep teleported the operator back into the cab mid-task, so the gate never opened.
2. **Retire-in-place leak.** `UnregisterGroup` retires a group *in place* when any member is `IsAIActivated()`.
   That rule was written when a distant crew was inert; the pin made it **permanently true**, so every teardown
   left two materialised men standing where their truck had been. This is what the author kept finding on foot.
3. **Crews bailing to fight.** `SCR_AIVehicleCombatActivity` dismounts a whole crew from an **unarmed** vehicle
   that perceives danger, and `SCR_AILeaveStaticVehicles:104-109` dismounts from a vehicle failing `CanMove()`.
   Both were unreachable before the pin. See the design decision above — this one is **intended** and stays.

### The seven faults, their causes and their fixes

| # | Symptom | Root cause | Fix |
|---|---|---|---|
| 1 | Convoys never moved | Crew at max LOD, behaviour tree off | LOD pin for the whole mounted lifetime, out **and** home |
| 2 | Gate never opened | Per-tick `SeatEveryone()` fought the door operator | Seating became an **event** (spawn / adopt / board), never a clock; narrow `EvictHijackers()` retained on the tick |
| 3 | Crews found walking, truck vanished | Deployment collection deleted a **returning** transport with its crew aboard; `TruckDeletionVeto()` protected a *player's* vehicle but not our own crew | Stand down + evacuate AI before deletion; a collected deployment's modules never tick again, so "let it drive home" was never possible |
| 4 | Successful team loitered at the objective | Collection refunded but never removed the force (retire-in-place again) | `StandDownDeploymentForce()`, on the three **mission** callers only |
| 5 | FOB raised while its party was a km away | The 80 m gate measured `GetPosition(handle)` = the `SCR_AIGroup` **marker entity**, which never follows its walking members | `OVT_VirtualGroupGeometry.IsGroupWithin()` — real member origins when materialised, record position when dormant. **Verified live.** |
| 6 | Same authored FOB site every time | `FindAuthoredSite` was strictly highest-score | Uniform random among corridor+band eligible. ⚠ `RandInt` is **max-exclusive** |
| 7 | Every insertion after the first failed to crew | **Importance-ordered spawn-queue starvation** — `CREW_IMPORTANCE` was NORMAL, so a two-man crew queued level with every garrison rifleman and never got its turn inside a 60 s window | `CREW_IMPORTANCE` → **HIGH**; and the clocks were split so async spawn latency is no longer charged to a "he won't get in the truck" budget |

**Fault 7's diagnosis cost three rounds and two disproved hypotheses** (an owner-key reclaim collision, then a
vanilla pop-in clause). What finally settled it: the author pointing out that **group member spawning is
asynchronous** — members arrive over many frames, so `0 materialised` shortly after registration is normal and
was never evidence of anything. The orchestrator had built an "asymmetry" argument on a line printed **8 seconds**
into a drive; it was withdrawn. The lesson is in the instrumentation now: **measure the trend, not the snapshot.**

### Reverted deliberately

A disembark fan-out (arc placement so the force stops blocking its own transport) was built, then **removed at the
author's instruction**. Correct placement requires knowing the men are actually out of their compartments, and
`GetOutVehicle(TELEPORT)` is **not synchronous** — vanilla defers by a call-queue tick with the comment *"because
person needs to be out of vehicle for sure"* (`GameCode/Vehicle/BaseCompartmentSlot.c:212-214`). Teleporting an
occupant **drags the vehicle**, which put a truck across the road. The deferred version worked but rearranged the
men visibly seconds later, which reads worse than the brief blocking it solved. `OVT_InsertionGeometry.c` and
`OVT_TEST_Logic_ObjectiveInsertion.c` are byte-identical to HEAD again.

### Stuck-transport cleanup (author's rule)

> "If an insertion gets stuck and there's no players around anywhere we can just delete the truck and its crew to
> minimize pile ups on that route."

The reasoning is compounding: a stuck truck is an obstacle the **next** convoy must path around. Implemented by
shortening the existing machinery, not adding a parallel one — `STUCK_TRUCK_TIMEOUT_TICKS = 1` replaces the
120-tick (~20 min) budget handed to the *same* predicate under the *same* 320 m hold. Poll, never one-shot.
**The force still walks**; only the transport and the men whose sole job was driving it are removed.

⚠ **Accepted regression, stated so it is not rediscovered as a bug:** a player who watches a convoy stall from a
kilometre away and drives out to loot it will find the truck already gone. The knob is
`ABANDONED_TRUCK_PLAYER_RADIUS_M` (320) — the question is "who counts as watching", not "how long do we wait".
⚠ An orchestrator suggestion to keep the 20-minute timer as an OR-backstop was **correctly refused** by the
implementer: it would have deleted a truck with a player standing beside it, contradicting two shipped assertions
in `…_CollectsAbandonedTransportsOnlyWhenDue` and the predicate's documented absolute hold.

### 🔴 The `Deployments/` freeze is broken, legitimately, and the DoD grep will now fail

`git diff Scripts/Game/GameMode/Deployments/` was an **acceptance criterion of Phases 2–7 and is no longer empty** —
this round changed `OVT_InsertionSpawningDeploymentModule.c`, `OVT_BaseBehaviorDeploymentModule.c`,
`OVT_MountedGroupActivation.c` and `Scripts/Game/Modded/SCR_AIGroup.c`. That freeze was a **build-time discipline**
to stop the feature drifting into its neighbours; it does not bind play-test fixes to shipped code found by
running the game. Anyone re-running the feature's DoD greps against the working tree should expect this and not
"restore" it.

### Owed

- 🔴 **The suites have not run against ANY of this round.** `compile-check.sh` exit 0 is the only gate it has had,
  and almost none of it is Logic-tier assertable — it is engine compartment / agent / AI-world state. Run **All**
  `{6A6E2A002F53A581}` before this is committed.
- The one stuck insertion the author saw has **no diagnosis** — the walk fallback absorbed it and nobody watched
  it happen. If it recurs, the liveness line now distinguishes a pathing stall from a crew that bailed to fight.
- `ABANDONED_TRUCK_TIMEOUT_TICKS` has **no live caller**; it survives only as the Logic case's realistic limit
  value. Delete it and give the test a literal, or leave it — recorded either way.
- MP/JIP untouched and untested throughout.

---

## 🔴 Play-test 2026-08-23 (v1.5 Workbench testing) — an insertion transport STALLED WITH A LIVE, PINNED, SEATED CREW

Reported by the user during `v1.5` testing, mid-way through building `occupying/vehicles` (which subclasses
this module). Recorded here rather than as a BUG because the insertion module is dev-branch code that has
never shipped — see the no-bugs-for-in-dev-features rule.

```
Insertion 'Objective Sabotage/Sabotage Team': its transport stalled at <7479.61, 5.44885, 6244.23>
  - crew handle 88: 2 of 2 alive in the mask, 2 materialised, 2 AI-active, worst LOD 9 of 10;
    lifecycle ProximityDriven on a 100000 m ring, nearest player 1931 m
Insertion ... its transport is left standing ... stopped making progress 1592 m short of the landing zone
Insertion ... is on foot ...
Insertion ... its abandoned transport ... was collected after 0 update(s)
```

### What the fallback did: exactly the right thing

Stall → `DismountAndWalk` → force on foot holding its plan → abandoned transport collected on the next update
because nobody was within 320 m. **That is the spine working**, and it is the behaviour `occupying/vehicles`
G4/F8 depends on inheriting. Nothing about the fallback is implicated.

### Two hypotheses KILLED by the log itself — do not re-open them

1. **It is not the crew/spawn gate.** `2 of 2 alive, 2 materialised, 2 AI-active`. Neither the
   "crew never materialised" nor the "nobody took the wheel" branch fired, so a driver was in the seat with a
   running behaviour tree for the whole stall window.
2. **🔴 It is not the LOD pin, and "LOD 9 is too slow to drive" is NOT an available explanation.**
   `worst LOD 9 of 10` is precisely what `OVT_MountedGroupActivation.HoldAgentActive` sets — `maxLod - 1`,
   the vanilla `SCR_ResupplyTaskSolver` recipe. That is the pin **working**.
   And the tempting follow-up — "yes, but a driver thinking at LOD 9 steers too rarely to hold a road" —
   is disproved by vanilla's own table:

   `SCR_AIDecideBehavior.s_aUpdateIntervals` (`ArmaReforger/Scripts/Game/AI/ScriptedNodes/Soldier/SCR_AIDecideBehavior.c:6-16`)
   is `{0.55, 1.3, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0, 2.0}` — it **saturates at 2.0 s from LOD 2 upward**.
   A driver at LOD 9 re-decides on exactly the same 2 s interval as one at LOD 2, and only LOD 0 (0.55 s) and
   LOD 1 (1.3 s) are faster. **Pinning deeper than `maxLod - 1` would buy nothing** and would cost simulation
   on every convoy. Do not "fix" this by lowering the pin.

### What is therefore left

The transport had a driver, a running tree, an operational hull, and it sat **below 1 m/s for 6 consecutive
deployment ticks ≈ 60 s** (`m_fStuckSpeedThreshold` 1, `m_iStuckTicks` 6) at **y = 5.45 m — essentially sea
level**, 1592 m short. That is an **AI vehicle driving / pathing** failure, the same class the Ural prefab
tuning addressed on 2026-08-19 (`FrictionCoefficient 0.055 → 0.1`, `MaxReverseTravelDistance 30`,
`Min Prediction Distance 2`), not a virtualization, LOD or spawn failure.

`Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_transport.et` **does** carry that tuning today, so if the stalled
transport was the USSR `truck` entry the tuning was already applied and was not sufficient at that spot. The
near-sea-level Y strongly suggests a coastal road, causeway or bridge approach.

### 🔴 The diagnostic gap that makes this expensive, and why it was NOT closed here

The stall line names the crew in forensic detail and says **nothing about the vehicle**: not which prefab, and
not **how far it had already travelled**. Those two facts separate two completely different bugs —

- *never moved at all* → the waypoint/path was never issued or the driver never took the order, versus
- *drove several km and wedged* → a specific piece of terrain beat `AICarMovementComponent`.

— and without them every occurrence needs a fresh repro. **The fix is one `DescribeTransport()` line beside
the existing `DescribeCrewLiveness()` one.**

⚠ It was deliberately **not** written during `occupying/vehicles`: that feature's G6 freezes this file to
**one appended enum value** for its entire duration, and quietly widening the diff would destroy the guarantee
that the mounted module inherited a play-tested spine unchanged. It is queued as the **first** edit to make
after `occupying/vehicles` closes.

### ⛔ CORRECTION (same day, after the user's second report) — it is NOT pathing, and the section above's conclusion is SUPERSEDED

The user: *"its not pathing. the spawn position is a proven good position at chotain base that ive seen them
drive away from a hundred times. the only difference here is that I wasnt watching them in GM or nearby."*

So the truck **never moved at all**, from a spawn point that works reliably **when observed**, and the single
variable is **observation**. That kills the terrain/tuning explanation outright — the `FrictionCoefficient` /
`MaxReverseTravelDistance` / `Min Prediction Distance` recipe is about a truck that drives *badly*, and this
truck did not drive at all.

**A third hypothesis also dies here, before anyone spends a day on it.** "At LOD ≠ 0 the driver cannot be
*told* to drive, because AI speech is gated" is wrong: `SCR_AICommsHandler.CanBypass()`
(`ArmaReforger/Scripts/Game/AI/Talk/SCR_AICommsHandler.c:120-123`) returns **true** — i.e. *bypass the
transmission and complete the request successfully* — precisely when `GetLOD() != 0`. The voice line is
skipped; the order is not. Comms is not a blocker.

### 🔴 The hypothesis that fits every data point: THE VEHICLE'S OWN SIMULATION IS ASLEEP

Everything the stall line measures is about the **men**. Nothing anywhere in this module measures the
**vehicle**. And the engine has a distance-gated vehicle simulation:

- **`VehicleWheeledSimulation.ForceEnableSimulation()`** exists as an engine proto
  (`ArmaReforger/Scripts/Game/generated/Vehicle/VehicleWheeledSimulation.c:17`, and the same on
  `VehicleTrackedSimulation` and `VehicleWheeledSimulation_SA_B`), documented verbatim as *"Forcibly enables
  simulation of vehicle, only meant for cinematics, not to be used in any game logic!"*
- **It has ZERO callers in the entire vanilla script tree.** A proto that exists only to force vehicle
  simulation on is proof that vehicle simulation is **conditionally off**, and BI's own warning is that script
  is not supposed to be the thing that overrides it.
- The governing distance is **`DYNAMICSIM_LASTLOD_DISTANCE`** — the same per-prefab dynamic-simulation budget
  the AI LOD system steps over (`SCR_AIGroup.c:118-123`), defaulting to max LOD at **≥ 1000 m**.
- The stall was at **nearest player 1931 m** — nearly twice that.

That reads: the driver had a running behaviour tree and was holding the throttle down on a hull the engine was
not simulating. Every crew gate reports perfect health, the truck reports 0 m/s, and the two are consistent.

It also explains the discriminator exactly. **A Game Master free camera is an observer**, and so is a nearby
player: watching the convoy pulls the truck inside dynamic-simulation range and it drives away — "a hundred
times".

⚠ `DYNAMICSIM_LASTLOD_DISTANCE` does **not** appear in any `.et` as text (0 hits across both trees). It is a
prefab/entity property, so it is a **Workbench** inspection, not a grep.

### The decisive experiment (cheap, one session)

Put a GM camera ~900 m from a driving convoy — it should drive. Pull back to ~1200 m and watch. **If the truck
stops within a second or two of crossing ~1 km and resumes as the camera closes, it is dynamic simulation, and
nothing about AI, LOD, pathing or prefab tuning is involved.**

If confirmed, the lever is `DYNAMICSIM_LASTLOD_DISTANCE` on the vehicle prefabs — Overthrow already owns
same-GUID deltas for `Ural4320_transport`, `BTR70`, and (as of `occupying/vehicles` Phase 1) `BRDM2` and
`LAV25`, so it is authorable in the place the friction tuning already lives.

### 🔴 What this means for `occupying/vehicles`, which is being built right now

**Every mounted deployment in that feature is designed to drive unobserved**, and the QRF mounted echelon's
entire premise (F4) is *"the vehicles really drive from that base, so they can be ambushed and they arrive
late."* If vehicle simulation is observer-gated at ~1 km then the echelon will routinely fail to cover the
last stretch, the harassment checkpoint will not relocate, and the hunter-killer sweep will not sweep — all of
them degrading silently to the walk fallback, which is *correct behaviour* and therefore **invisible**.

This is a risk to the feature's premise, not to its correctness. It is recorded here and in
`docs/features/occupying/vehicles/context.md` so it is settled before the play-test rather than during it.

### ✅ THE LEVER, FOUND — and the `DYNAMICSIM_LASTLOD_DISTANCE` recommendation above is WRONG, ignore it

`DYNAMICSIM_LASTLOD_DISTANCE` is documented (`SCR_AIGroup.c:118-123`) as a property set **on character
prefabs**, governing the **AI agent LOD** stepping — the axis already proved irrelevant here. It is **not** the
vehicle-physics gate, and it has **0 text hits** across both trees, so the "Workbench edit on the vehicle
prefabs" suggestion above is unfounded. Struck.

**The engine documents the real mechanism in one sentence.** `ObserversSystem.InsertObserverSP`
(`ArmaReforger/Scripts/Game/generated/System/ObserversSystem.c:14-22`):

> `\warning Temporary observers can keep distant entities simulated, so be mindful of their lifetime.`

That is BI stating both halves at once: **distant entities are not simulated by default**, and **an observer is
the supported way to keep one simulated**. The identical warning is repeated on `InsertObserverMP`. It explains
the discriminator exactly — a Game Master camera is an observer, so watching the convoy is what was making it
drive.

**Overthrow already owns the wrapper**, built for exactly this shape of problem by `virtualization/core`:

| API | Contract |
|---|---|
| `OVT_VirtualizationManagerComponent.AddEntityObserver(IEntity)` (`:3375`) | Parks an SP observer that **follows** the entity (zero offsets, so it moves with it). Server-only, idempotent per entity, reuses the key when one already exists. **Refuses null** (a null insert hard-freezes the client) and refuses an invalid `EntityID`. |
| `RemoveEntityObserver(IEntity)` (`:3450`) | The pair. |
| `HasEntityObserver` / `GetEntityObserverCount` | Answer from core's own map, **never** the engine — application is deferred one frame both ways, so an engine query straight after an add reads a false negative. |

Keys count up from `ENTITY_OBSERVER_KEY_BASE = 771000`; session-local, never persisted, never replicated.
Two production callers already pair add/remove correctly: `OVT_InactiveRecruitGroupComponent.c:128/197` and
`OVT_HighCommandGroupComponent.c:66/171`. `OVT_TEST_InitSuite.c:6713` already guards the null-freeze.

**The fix:** `AddEntityObserver(m_Truck)` while the transport is driving; `RemoveEntityObserver(m_Truck)` on
every exit path.

⚠ **The hazard is the lifetime, and it is the one BI's warning names.** This module has ~17 release paths
(its own `TickAbandonedTruck` header counts them). A missed removal leaks an observer that keeps a chunk of the
map simulated **for the rest of the session** — a real server-cost regression, and worse than the bug it fixes.
The change is two lines; the **audit of every exit path, plus a case asserting the observer is gone on each,
is the actual work.**

⚠ **It breaks G6** (`occupying/vehicles` freezes this file to one appended enum value). That is a deliberate,
documented break to be taken **after** that feature's Phase 2 stops editing the file — not a quiet widening.
Raised with the user 2026-08-23; awaiting their call on timing.

### 🔴 THE MODULE'S OWN "TWO STAMPS" MODEL IS INCOMPLETE — there is a THIRD, and it is on the HULL

User observation, 2026-08-23: *"the insertion module is a monolith, much of its code may have been trying to
code around this very problem."* The file's own header settles it. Its central diagnosis reads:

> *"A live convoy needs TWO stamps, not one. The crew has to EXIST wherever the players are (the 100 km
> RIDING_SPAWN_DISTANCE ring) and it also has to be RUNNING, which no ring can deliver... A crew with only the
> first stamp is a materialised driver asleep at the wheel — a convoy that 'never left its spawn point' with a
> perfectly alive crew. OVT_MountedGroupActivation is the second stamp."*

**The 2026-08-23 log carries BOTH stamps and the truck still never moved.** `2 materialised` (stamp 1),
`2 AI-active, worst LOD 9 of 10` (stamp 2, the pin working exactly as designed), 0 m/s for six ticks.

**Stamp 3: the VEHICLE must be simulated.** Every gate in this file measures *people* — alive, materialised,
AI-active, seated, driving. Nothing anywhere measures the *hull*. That is why the diagnostics read perfect
while the thing that was not running went unnamed.

Consequences for how the file's history should be read:

- **The LOD pin is necessary but NOT sufficient**, and was credited with a fix it can only have half-made. A
  driver with no behaviour tree certainly cannot drive — but supplying one does not make an unsimulated hull
  move. The play-tests that "confirmed" the pin were almost certainly run with an observer present.
- **`NudgeCrewMaterialisation` already admits it is not a fix** in its own header: *"measured over three
  failing insertions it FIRED on all three crews and NONE of them materialised, so it is not the fix and is not
  claimed to be."* Symptom-chasing, honestly labelled.
- **The stuck test, the uncrewed grace, the abandoned-truck countdown and the walk fallback are all "the truck
  is not moving" machinery.** Some of it is legitimate design (a truck really can be destroyed or blocked, and
  the header is explicit that the march is the spine by intent). Some of it may be scaffolding around a root
  cause nobody had named. **Which is which cannot be decided from the code — it needs one play-test with the
  observer fix in.**

⚠ **Delete nothing on this basis yet.** The fallback is what currently makes these failures safe-but-invisible;
removing it on a hunch would trade an invisible degradation for a visible defect.

### ✅ CORRECTION: the release audit is ONE path, not seventeen

An earlier note here worried about ~17 release paths. Wrong, and the file says so: every exit funnels through
**`ReleaseConvoy(reason, deleteTruck)`** (`:1959`), described in `ReleaseRidersActive`'s header as *"this
file's single audited teardown, so there is exactly one release to keep correct rather than five."*
`ReleaseRidersActive()` already sits there. The observer removal goes beside it.

### The shape of the fix

1. `AddEntityObserver(m_Truck)` where the transport is spawned / the drive begins (`m_Truck` is assigned at
   exactly one place, `:746`).
2. `RemoveEntityObserver(m_Truck)` in `ReleaseConvoy()`, beside `ReleaseRidersActive()`.
3. **An authored off-switch attribute**, following the established convention. This is mandatory, not polish:
   core's own header warns that an entity observer means *"registered groups near it materialise even with no
   player anywhere near. That is the feature and it is the budget risk in one sentence, which is why the
   consumer is expected to carry an off-switch"* (`m_bRecruitGroupsAreObservers` is the precedent). A truck
   crossing 2 km will wake ambient groups and other deployments along its route.
4. An Init case asserting the observer is present while DRIVING and **gone after teardown** — the leak is the
   expensive failure mode, per BI's own `\warning` about observer lifetime.
5. Correct the class header's "TWO stamps" to three; mark the pin's play-test claim and
   `NudgeCrewMaterialisation` as superseded.

### ✅ FIXED 2026-08-23 — the third stamp is parked and dropped; `compile-check.sh` exit 0, suites deferred

Landed on `v1.5`, uncommitted. Five files of production code and test, no config, no prefab, no persistence.

**What landed, and where:**

| | |
|---|---|
| The off-switch | `OVT_InsertionSpawningDeploymentModule.m_bTransportIsObserver`, `[Attribute(defvalue: "1")]` — **ON as shipped**, because the stall is the common case. Read **at the consumer** in `HoldTruckSimulated()`, never inside `AddEntityObserver` — the `m_bRecruitGroupsAreObservers` convention. |
| The add | `HoldTruckSimulated()` (beside `HoldRidersActive`/`ReleaseRidersActive`), called from **`EnsureConvoy()`**, on the line after the transport is secured. |
| The drop | Two lines in **`ReleaseConvoy()`**, immediately after `ReleaseRidersActive()` and before `UnsubscribeRiders()` / `ReleaseTruck()` — the same ordering rule the line above it carries. |
| The header | "TWO stamps" → **THREE**, with the third named as being on the hull and the note that nothing else in the file measures the vehicle. The LOD pin's "never left its spawn point" fix is marked **superseded — necessary but not sufficient**. One line added to `NudgeCrewMaterialisation` pointing at the real cause. Nothing was deleted: the stuck test, the uncrewed grace, the nudge and the walk fallback all stand until a play-test says otherwise. |

⚠ **The add is in `EnsureConvoy()`, NOT at the `m_Truck = ...` line the diagnosis pointed at (`:746`), and the
reason is load-bearing.** `OVT_MountedForceSpawningDeploymentModule` **overrides `SpawnTruck()`** and assigns
`m_Truck` itself on its adopted-hull branch, so a call inside the parent's spawn would have missed exactly the
vehicles `occupying/vehicles` cares about. `EnsureConvoy()` is `SpawnTruck()`'s only caller, runs only in
`DRIVING`, and both assignment routes pass through it. It re-asserts on every convergence, which is free
(idempotent per entity).

#### The leak audit — verdict: CLEAN on every module-driven path, with one named residual

- **`m_Truck` is nulled at exactly one function, `ReleaseTruck()`, on both of its branches** (the deletion and
  the ownership veto). Its three callers are `ReleaseConvoy(deleteTruck: true)`, `TickAbandonedTruck()` and
  `ArmAbandonedTruck()` — and the **last two are only reachable downstream of `ReleaseConvoy`** (the abandoned
  countdown is armed nowhere else). So no path nulls the field before the removal runs.
- **The truck is never re-assigned over a live one.** `SpawnTruck()` is called only behind `if (!m_Truck && ...)`.
- **Arrival does NOT release**, and must not: `CompleteInsertion()` moves to `RETURNING` and the transport
  drives home, so the observer has to survive it. All four `TickReturn` exits go through `ReleaseConvoy`.
- **An entity handle nulls itself if something else deletes the vehicle** (not a destroyed one — a wreck is
  still an entity), which would orphan the map entry. That case is covered by core's own 2 s
  `SweepEntityObservers()`, whose documented job is exactly "an entity destroyed by something that never runs
  script". Session-long leak avoided.
- **World teardown** is covered by `RemoveAllEntityObservers()` on the manager's `OnDelete`.
- 🔴 **Residual, not fixed:** a module instance discarded *without* `OnCleanup` while its truck still exists
  would leak the observer. That path would equally leak the convoy reservation and the crew registration, both
  of which already depend on the same single teardown — so it is not a new class of leak, and closing it means
  auditing module destruction generally, which is out of this repair's scope.
- ⚠ **Not a leak but worth knowing:** for a `HOLDING` mounted force (`occupying/vehicles`), `ReleaseConvoy` is
  not reached until the deployment ends, so **the observer's lifetime becomes the deployment's** rather than
  the drive's. Defensible (a parked echelon's gunner is manned and it may be ordered to relocate), but it is a
  much longer-lived observer than a convoy's and that feature should decide it deliberately.

#### The clone counts, all three of them

`CloneModule` is not chained; three classes hand-copy this module's fields and **all three were updated**:

| Class | Was | Now |
|---|---|---|
| `OVT_InsertionSpawningDeploymentModule` | 13 inherited + **10** own | 13 + **11** |
| `OVT_MountedForceSpawningDeploymentModule` | **27** (13 + 10 + 4) | **28** (13 + **11** + 4) |
| `OVT_FOBRaiseSpawningDeploymentModule` | **25** (13 + 10 + 2) | **26** (13 + **11** + 2) |

Their three clone cases were extended with the new field (probe value `true`, so a dropped line is caught —
`new` gives `false`), and every count in their prose was corrected.

#### The test, and what it cannot prove

`OVT_TEST_Init_ObjectiveInsertion_ObserverParkedWhileDrivingAndGoneAfterTeardown` (Init tier), driven through
a test-local `OVT_TEST_InsertionObserverProbe : OVT_InsertionSpawningDeploymentModule` — the
`OVT_TEST_MountedForceProbe` pattern, no production API widened. It spawns one real transport near the fixture
town, waits for a valid `EntityID`, and asserts three things in **one synchronous frame**:

1. switch **on** + a planted transport → `HasEntityObserver` true and the count moves by exactly one;
2. `ReleaseConvoy(reason, **deleteTruck false**)` → `HasEntityObserver` false and the count comes back;
3. switch **off** → nothing is parked and the count does not move.

⚠ `deleteTruck` is **false on purpose**: against a deleted truck the handle nulls itself and
`HasEntityObserver(null)` is false for free, so the leak assertion would pass whether or not the removal was
ever written. The count assertion is the leak check that does not depend on the entity at all.

⚠ Presence is read from **core's own map**, never the engine — engine application is deferred one frame in
both directions. All three claims in one frame is also what makes the case safe in the shared init world: the
engine never applies the observer, so nothing near the fixture is pulled awake.

**It cannot prove that a convoy drives.** Everything asserted is core bookkeeping. That the engine then
simulates the hull, that the truck covers ground with nobody near it, and what the observer costs along a 2 km
route are all **play-test** questions, and the decisive experiment described above is still the thing to run.
The suites were **not** run for this change (user in Workbench); `tools/compile-check.sh` exit 0 is the only
gate it has had.

#### ⚠ This DELIBERATELY BREAKS `occupying/vehicles` G6

That feature froze `OVT_InsertionSpawningDeploymentModule.c` to **one appended enum value** for its whole
duration, and this repair widens that diff by an attribute, a method, two call sites and a header rewrite. The
user was consulted 2026-08-23 and left the call to the implementer; the break is **recorded rather than
quiet**. Q1 of that feature's DoD (the `git diff` grep) will now legitimately fail and must be **re-stated**,
not "repaired" — see `docs/features/occupying/vehicles/context.md`.

## 🔴 FIXED 2026-08-23 — THE FORWARD BASE WAS NEVER RESTORED ON LOAD: tracking is not self-spawning

User report during `v1.5` Workbench testing: *"a FOB has disappeared with its garrison still standing there. was it pulled down?"* It was not. Nothing removed it — it was never brought back.

**What the logs show** (`logs_2026-08-23_10-45-12`, then `_11-35-39`):

- 11:12:21 `Forward base 'Objective Forward Base/Forward Base Party' raised at <7415.72, 113.914, 5298.08>`, garrisoned 11:18 and 11:28.
- No dismantle / teardown / abandon line in either session.
- Saves at 11:27:06 and 11:31:59, load at 11:40:37. The garrison and the objective came back (`Objective Forward Base Garrison … came back from a save point`; objective still in `ForwardBase` at `#OVT-Base_Levie`). The structure did not, and D11's gate correctly stopped the restored deployment re-raising one.
- The savepoint blob was decoded directly (`playthrough009/savepoint001`): **zero records for the FOB prefab**, while `OVT_DeploymentComponent`, `OVT_BuildableComponent`, `OVT_PlaceableComponent` and `OVT_ResourcePileComponent` records were all present.

**Root cause.** `OVT_FOBRaiseSpawningDeploymentModule` calls `OVT_PersistenceTracking.Track(structure)` and its header claimed *"Vanilla persistence saves it and puts it back."* **Tracking only WRITES the record.** An entity comes BACK only when the `PersistenceConfig` it MATCHES carries `SelfSpawn 1`, and matching is done by the `ComponentClassPersistenceConfigRule` entries in `Configs/Systems/Persistence/Overthrow.conf` — which were `OVT_PlaceableComponent`, `OVT_BuildableComponent`, `OVT_DeploymentComponent`, `OVT_ResourcePileComponent`. `Prefabs/Bases/OVT_OccupyingFOB.et` carries **none** of them (it is a `FlagPole_02_V1` delta with `OVT_OccupyingFlagComponent` + `ActionsManagerComponent` + `RplComponent`), so it was tracked, saved and never spawned back.

⚠ **The generalisation, and it applies to anything this epic spawns and wants back:** `Track()` makes an entity SAVEABLE; a matching `.conf` rule with `SelfSpawn` makes it RETURN. The two are separate and nothing in the tree pairs them. BUG-018's finding is the neighbour of this one: a *scripted* rule is never consulted either, so the `.conf` is the only place this can be declared.

**The symptom compound.** The director's `OVT_PersistedObjective` record restores intact, so `IsAssetUp(ASSET_FOB)` keeps answering true against a structure that is gone; the garrison stands on an empty site; and the dismantle action rides on the flagpole, so **the player can no longer end the objective the intended way** — it can only time out.

### What landed (uncommitted on `v1.5`, `compile-check.sh` exit 0)

| | |
|---|---|
| The rule | A fifth `EntityPersistenceConfig` in `Configs/Systems/Persistence/Overthrow.conf` matching `ComponentClass "OVT_OccupyingFlagComponent"`, `SelfSpawn 1`, `Priority 35000`, `ParentHandling "Ignore always"`, `Collection {6B0E7A11D0F5A34B}` — the `OVT_ResourcePileComponent` block's shape verbatim. GUIDs `{6B0E7A7A3B4C5D6E}` / `{6B0E7A7B4C5D6E7F}` / `{6B0E7A7C5D6E7F80}` / `{6B0E7A7D6E7F8091}`. |
| The serializer | `OVT_OccupyingFlagComponentSerializer` (`Scripts/Game/Persistence/Serializers/Components/`). **It carries no component state on purpose** — the flag component's one field, the faction index of the material on the pole, is re-derived by its own 10 s re-check on every machine, and storing it would let a save outvote a campaign started against the other occupier. Its real job is `OVT_NavmeshRebuild.Queue(owner)` on load: the world's navmesh is the BAKED one and the raise path's `RebuildNow()` never runs for a restored structure. That is the `OVT_PlaceableComponentSerializer` / `OVT_BuildableComponentSerializer` precedent verbatim. |
| The header | The raise module's "vanilla persistence puts it back" claim corrected in two places, with the rule named at the `Track()` call site so a modded structure prefab is told it must carry `OVT_OccupyingFlagComponent` or bring its own rule. Same correction in `OVT_TEST_Init_ObjectiveFOB.c`'s file header. |
| The test | `OVT_TEST_Init_ObjectiveFOB_MStructureConfigSelfSpawns`. Reads the prefab **off the config** (not a constant), stands it near the fixture town as a probe, asserts it carries `OVT_OccupyingFlagComponent` and that its matched `EntityPersistenceConfig.m_bSelfSpawn` is true, then `Untrack(keepData: false)` **before** deleting on every exit path — the file's standing rule is that no case may leave a persisted flagpole in the shared init world. |

**What did NOT need changing, and why that is worth knowing:** the teardown was already correct. `OVT_RaiseForwardBaseObjectiveOperation.RemoveStructure()` finds the structure **by prefab resource name inside a sphere**, not by the module's runtime `EntityID`, precisely because that link does not survive a load. So a restored FOB is torn down and dismantled correctly the moment it exists again. And deleting a tracked entity drops its record with it (`SelfDelete` defaults on), so a dismantled base does not come back on the next load.

⏸️ **Owed:** the **All** suite has not been run against this (it is one config rule, one new serializer, one new case), and the play-test that actually settles it is *raise a forward base, save, load, and see it standing with its garrison* — plus one AI walk past it to confirm the queued navmesh rebuild took.

## 2026-08-23 — `objectiveFirstOperationDelayMinutes`: a newly chosen objective holds fire

User play-test: *"they are relentlessly sending specops and the player has almost no time to settle, build, repair."* The cadence had **no starting offset** — the tick that committed to a target could spend on the very next in-game minute, so the moment a place became the objective the first team was already driving. Measured from the logs: at 6× time acceleration Normal's 60 in-game-minute interval is exactly 10 real minutes, and the director fired on it like a metronome (10:47, 10:57, 11:08, 11:18, 11:28) because it never skips an interval it can afford.

**The setting.** `OVT_DifficultySettings.objectiveFirstOperationDelayMinutes` — in-game minutes a newly committed objective waits before its first operation. Authored Easy 240 / Normal 150 / Hard 100 / Extreme 60 / Insane 30; class default **0**, which is the pre-setting behaviour, so a difficulty file that never authors it behaves exactly as before.

**Where it is armed, and why that is load-bearing.** `ArmFirstOperationDelay()` is called from `CommitObjective()` **after** `EnterObjectivePhaseIndex(m_Instance, 0)`. `EnterObjectivePhase()` zeroes `nextOpTicks` on every entry, so arming it beside the other record fields — the obvious place — is silently wiped by the entry it was armed for. The Init case reads zero at its first claim if anybody moves it back.

**Three deliberate scope decisions:**
- **Commit only, not phase entry.** A phase transition is the same objective escalating, and the ramp's own cadence governs it. The grace is what the player gets when the faction picks a NEW target.
- **It is spent whether or not the faction could have afforded anything.** It arms the same countdown a successful operation arms, and the tick serves that countdown unconditionally. A faction that was broke through the whole grace does not get it back.
- **A restored objective keeps its saved countdown.** `ApplyPersistedObjective()` writes `nextOpTicks` directly rather than committing, so a save taken mid-grace comes back mid-grace and one taken after it does not serve it again.

Coverage: `OVT_TEST_Init_ObjectiveDirector_ANewObjectiveHoldsFireBeforeItsFirstTeam` — asserts against the **difficulty**, never a hard-coded number, and reports a skip rather than a pass in a world that authors zero. `compile-check.sh` exit 0; suites owed.

⚠ Unrelated but noticed while editing the difficulty files: `Difficulty_Extreme.conf` has **lost** its `objectiveHarassmentMaxConcurrent 3` and `objectiveMaxConcurrentInsertions 4` lines, so both now fall back to the class default of 2. If that was a Workbench re-save rather than an intentional edit, Extreme is quietly weaker than it was.

---

## 2026-08-24 — Director logging gated

The director and its modules were part of the log-spam sweep. 16 informational `Print` sites here now go through `OVT_DeploymentLog.Debug` (off unless `m_bDebugMode`); `LogOperationRefusal`/`LogObjectiveRefusal` use `OVT_DeploymentLog.Log(msg, level)` so the `ERROR`/`WARNING` levels their callers pass are still honoured. Full write-up, including why `LogLevel.VERBOSE` is not a sink, is in `../deployments/context.md` (2026-08-24).

---

## 2026-08-24 — Tower recapture approach warning re-arms

**Author, on the test server:** *"spec ops are currently at my radio tower trying to recapture (its from the objective director) and I was not notified. I believe it only notifies in the unrest recapture deployment."*

**The diagnosis in the report is not what the code says, and that is worth recording.** `Deployment_ObjectiveTowerRecapture.conf` and `Deployment_TowerRecaptureUnrest.conf` both use `OVT_TowerRecaptureBehaviorDeploymentModule` and both author `m_fApproachWarningRadius 300`; diffing them shows the two module blocks are identical apart from their GUIDs. The `RadioTowerCapture` preset is registered in `overthrowBroadcastMessages.conf:132`. Nothing about the warning is unrest-only.

**The gap that is real:** `m_bApproachAnnounced` was a **one-way latch for the life of the deployment**, while `OVT_ReinforcementBehaviorDeploymentModule` ("Rebuy losses, or collect the team once the tower is ours") **rebuys losses into that same deployment**. So the first team announced itself and every replacement team afterwards arrived in silence — for a deployment with `m_iHoldSeconds 600` and `m_iMaxInstances -1`, that is most of the teams a player will actually meet.

**Fix:** the latch re-arms. `WarnOnApproach` now clears it whenever no living registered member of the force is within the warning radius, so a fresh approach is a fresh warning; the ten-second repeat it was written to prevent still cannot happen, because the latch is only cleared once the force is *gone* from the ring.

⚠ **Not proven to be the whole story.** This is the one mechanism that survives reading the code, and it fits the report; the session's own log was not available to confirm (the test server writes elsewhere, and this session's logs predate it). If a *first* recapture team is ever seen arriving unannounced, the warning is not the suspect — the suspect is `CountAliveRegisteredMembersWithin`, which counts only groups the spawning module registered with the virtualization core.

⚠ **Unrelated but adjacent:** `OVT_RadioTowerCaptureBehaviorDeploymentModule` (used by `Deployment_TowerGarrison.conf`, a different class from the recapture module) sends **no notification at all**. It only flips a tower when its own garrison is wiped, which is news in the player's favour, so it was left alone.

`tools/compile-check.sh` exit 0 (6346 files). Suite not run; play-test owed.

### Reinforcement is defensive-only; offensive operations are paid for in full (2026-08-24)

**Author, correcting the design:** *"that's not what reinforcement is supposed to be for though. reinforcement should only ever be for defensive deployments, these are offensive and should be paid for in full every time"* — and, on the same thread: *"it also means they will keep trying to retake the same tower even if they keep failing, never picking a different tower to go for."*

**Both of the player-visible symptoms were the same root cause**, and neither was a targeting bug. `OVT_SendDeploymentObjectiveOperation` already walks every candidate tower and skips the ones inside `m_fDedupRadius` of a live deployment (`m_iMaxConcurrent 0` on tower recapture is *no cap* — it bounds itself per tower). What blocked it was that the operation at the first tower **never ended**: the reinforcement module rebought its force forever, so the deployment stayed live, that tower stayed deduplicated, and from outside it read as the faction fixating on one tower. It is also why replacement teams arrived unannounced (the entry above).

🔴 **`m_bEnableReinforcement 0` alone would have been a serious bug.** `OVT_ReinforcementBehaviorDeploymentModule` owns **two unrelated jobs** — "buy the force back" and "take this deployment away when it is over" — and `OnUpdate` opened with `if (!m_bEnableReinforcement) return;`, so the flag disabled **both**. Turning reinforcement off on an offensive config would have left its marker in the world forever once the force died, and the director's own dedup would then have read that dead marker as "already sent" and never bought another — the exact fixation the change is meant to end, made permanent.

**What shipped:**
- `TickCollection()` lifted out of `CheckReinforcement()` and runs **regardless of the flag**. It keeps the existing condition-fail teardown verbatim, and adds one rule: *reinforcement off + every spawning module eliminated → delete the deployment*. That deletion is what makes "paid in full" true — the dedup stops seeing an operation there, and the next send is a fresh purchase at full price.
- `m_bEnableReinforcement 0` on the five offensive configs: `Deployment_ObjectiveTowerRecapture`, `Deployment_TowerRecaptureUnrest`, `Deployment_ObjectiveHarassment`, `Deployment_ObjectiveHarassment_Mounted`, `Deployment_ObjectiveSabotage`. Their `m_sModuleName`s now say what the module does there.
- `SampleCasualties()` is now called only when reinforcement is enabled — it exists solely to feed the rebuy's contact cooldown. Defensive configs are unaffected in every respect.

⚠ **Two configs deliberately left reinforcing, and they need a ruling:** `Deployment_ObjectiveFOB` ("Rebuy the free garrison…") and `Deployment_ObjectiveFOBGarrison`. Both *garrison* a forward base rather than strike at one, which reads as defensive under the new rule — but the first is bought as an objective operation, so it is genuinely ambiguous and was not changed on a guess.

⚠ Re-picking the **same** tower after a collection is now possible and is not fixation: it costs the faction a full purchase, gated by the operation cadence and the pool.

`tools/compile-check.sh` exit 0 (6346 files). Suite not run; play-test owed.

### The enemy FOB left its props standing when it was pulled down (2026-08-24)

**Author, on the test server:** *"I just pulled down an enemy FOB and the camo net disappeared but not the prefab's children."*

**Two independent faults, and the camo net is the tell for the second.**

**1. `OVT_WorldUtils.DeleteEntityTree` was ONE LEVEL DEEP despite its name.** It walked the root's *direct* children and handed each to `SCR_EntityHelper.DeleteEntityAndChildren` — which `OVT_ResistanceFactionManager` already documents as a misnomer: its whole body is `RplComponent.DeleteRplEntity(entity, false)`, so it removes the entity it is given and leaves that entity's own children in the world. Anything two levels down survived. Now genuinely recursive, deepest-first, re-reading each level's children rather than pre-collecting the tree (a pre-collected list hands back handles an ancestor's delete already freed). `MAX_TREE_DEPTH` 16 guards a cycle, and the "never delete a character" rule is now applied at **every** level instead of only the first.

**2. Only one of the FOB's five props could ever be a hierarchy child.** Walking each child's prefab chain:

| Child | Chain root | `Hierarchy`? |
|---|---|---|
| CamoNet_Tent_Soviet | `CamoNet_Base.et` | ✅ **yes** |
| CrateWooden_01_base | `Props_Base.et` | ❌ |
| CzechHedgehog_01_base | `Fortifications_Base.et` | ❌ |
| BarbedTape_Coil | `DestructibleMultiPhase_Props_Base.et` | ❌ |
| ShellContainerstack_01_pile_big | `ShellContainerstack_01_base.et` | ❌ |
| **the FOB root itself** | `FlagPole_Base.et` | ❌ |

The camo net is the only one whose chain carries a `Hierarchy` component, and it is the only prop that came down — an exact correlation with the report. `Hierarchy` components authored in `OVT_OccupyingFOB.et` on the **root** (without one, nothing under it is a child at all) and on each child that does not inherit one.

⚠ **The four Czech hedgehogs were a `$grp StaticModelEntity` block**, which shares one prefab reference across instances and has no place to hang a per-instance component. They are now four individual `StaticModelEntity` entries carrying the same IDs, coords and angles, each with its own `Hierarchy`. `$grp` is vanilla's baked-static form, so **this is the one edit here that a Workbench load must confirm** — `compile-check.sh` does not parse `.et` files.

⚠ **The recursion change touches five other callers** — town vehicle cleanup, the insertion transport, vehicle-patrol teardown, high command, and the virtualization core. For all of them it fixes the same latent leak (a vehicle's turret sub-entities are exactly the two-levels-down case), and the deepest-first order means a parent is never freed before its children are read. It is still a behaviour change on paths this report did not cover, and it deserves a look on the next vehicle play-test.

`tools/compile-check.sh` exit 0 (6346 files). Workbench prefab load owed; suite not run.

---

## Change 2026-08-25 — two objective announcements reach the Discord webhook

User ask: post to the webhook when a deployment is trying to recapture a radio tower, and when spec-ops
have sabotaged a buildable; and confirm the counter-attack announcement already does.

Both were sending the in-game text notification only. `SendExternalNotifications` is a separate call —
`SendTextNotification` does not imply it — so anything that wants both has to make both calls.

- `OVT_TowerRecaptureBehaviorDeploymentModule.WarnOnApproach()` → `RadioTowerCapture`
- `OVT_BaseSabotageBehaviorDeploymentModule.NotifyOnce()` → `ObjectiveSabotage`

Both are already once-per-mission behind their own latch (`m_bApproachAnnounced`, `m_bNotified`), so the
webhook inherits that rate limiting rather than needing its own — the tower one re-arms only when no
living team member is left near the tower, which is the behaviour a player sees in chat too.

Both tags already exist as `SCR_SimpleMessagePreset`s in `Configs/overthrowBroadcastMessages.conf`
(`RadioTowerCapture` :133, `ObjectiveSabotage` :574) with a `Description`, which is what
`SendExternalNotifications` localizes — a tag with no preset returns early and posts nothing, silently.
**No `.st` change and no re-export owed.**

**Counter-attack: already correct, no change made.** `OVT_OccupyingFactionManager.RevealQRF()` is the
single announcement path (SILENT_DEPLOY → MUSTER, idempotent) and it already sends both the text and the
webhook for `CounterAttackTown` and `CounterAttackBase`. Grepped for a second announcement path; there
is none.

`tools/compile-check.sh` exit 0 (6347 files). Suites not run — the user was play-testing. Owed: confirm
the two posts actually land in the channel on a server with `discordWebHookURL` set.

---

## Change 2026-08-25 (b) — player join/leave on the Discord webhook

User ask: post to the webhook when players join and disconnect.

`OVT_OverthrowGameMode` gained an `OnPlayerConnected` override and one line at the top of
`OnPlayerDisconnected`, both routed through a new `AnnouncePlayerToWebhook(tag, playerId)`.

**Two placement decisions, both load-bearing:**

1. **NOT `m_PlayerManager.m_OnPlayerConnected`.** That invoker fires from `FinalizePlayerPreparation`,
   which early-returns for anyone already in `m_aInitializedPlayers` — the RECONNECT path. Hooking it
   would have announced first joins only and silently missed every reconnect. The vanilla
   `OnPlayerConnected` override is the one hook that fires on every arrival, and it is server-only by
   vanilla's own contract.
2. **The leave post runs BEFORE `super.OnPlayerDisconnected`**, and before the id mappings are cleared,
   because that teardown is what makes `GetPlayerName(playerId)` stop resolving. It does not branch on
   `KickCauseCode`, so a quit, a timeout and a kick all announce identically.

**External channel ONLY — no `SendTextNotification` pairing.** Vanilla already prints its own
connect/disconnect lines in game; a second set would be duplicate spam. `SendExternalNotifications` is
a separate call, so "webhook only" is simply the one call.

**New content:** presets `PlayerJoined` / `PlayerLeft` in `Configs/overthrowBroadcastMessages.conf`
(GUIDs from a verified-free `6A8E2F50…` series) and `#OVT-Msg-PlayerJoined` / `#OVT-Msg-PlayerLeft` in
`Language/localization_Overthrow.st` — braces counted 2445 → 2449, balanced at both ends.

🔴 **A `.st` re-export is owed.** Both keys render as raw `#OVT-Msg-…` in the webhook post until
`Configs/Language/*.conf` is regenerated in Workbench. Those exports were NOT written by hand.

`tools/compile-check.sh` exit 0 (6347 files). Suites not run — the user was play-testing. Owed: confirm
the posts land on a server with `discordWebHookURL` set, including that a reconnect announces.

### 2026-08-25 — Sabotage completed with the team dead and high command holding the base

**Author:** *"a specops team just sabotaged a buildable at our base after they were dead and there is resistance everywhere (high command groups)"*; *"I wasnt there myself, but it looks like they didnt even make it to the flag, their bodies are like 40m away"*; and, correcting a wrong diagnosis: *"a high command group cannot be dormant, they are an observer, and besides I was only about 300m away and could hear the firefight."*

**UNRESOLVED. Instrumented, not explained.** Two source-read explanations were offered and both were wrong; this entry records what has been RULED OUT so the next reader does not re-walk them.

❌ **Ruled out — "the HC groups were dormant so the entity query could not see them."** Wrong, and the author said so. `OVT_HighCommandGroupComponent.InstallObserver` makes every HC group an **AI observer**, and its header forbids `SetLifecyclePolicy` precisely so the group stays Manual/always-live: *"Manual (the engine default) is what 'always live' means; ProximityDriven would delete its bodies at 800 m."* Their men are materialised, `OVT_ResistancePresence`'s sphere query finds them, and the author was 300 m away with the firefight audible regardless.

❌ **Ruled out — a surviving transport crew standing in for the dead team.** The insertion module registers its crew under a separate `crewKey` and `CollectRegisteredHandles` returns only `m_aHandles`, so the crew is never counted by `CountAliveRegisteredMembersWithin`.

❌ **Ruled out — HC groups being the wrong faction for the query.** `Group_FIA_*` → `Character_FIA_Rifleman` → `Character_FIA_Base` authors `"faction affiliation" "FIA"`, which is `m_sPlayerFaction`, so the affiliation branch matches them directly.

**What is left.** `EvaluateDemolition` fires only on `aliveInside >= 1 && !enemyPresent && ticks exhausted`. With the team dead `aliveInside` should be 0 (the survivor mask), and with HC groups at the base `enemyPresent` should be true. Both should have blocked, and one of them demonstrably did not. Which one cannot be settled from source — so it is logged instead, on the update that actually takes a structure: `aliveInside`, the clear radius, `enemyPresent`, and the **measured distance to the nearest resistance**. A positive count beside a dead team indicts the survivor mask; `enemyPresent=false` with a small distance indicts `OVT_ResistancePresence`.

⚠ One note on timing the author raised implicitly: `ticksLeft` **pauses** rather than resets when contested, so a mission can bank progress while the base is quiet and finish the instant it goes quiet again. That is by design, but it means "the demolition happened after they died" and "the ticks were earned before they died" are not mutually exclusive.

**Kept from the wrong diagnosis:** `OVT_HighCommandManagerComponent.HasLivingGroupWithin(position, radius)` and its use as `IsGroundHeld`'s first question. It is no longer justified as a dormancy fix - it is a cheap early answer (a walk over a handful of records instead of a world sphere query) and a fallback for a server running with `GetHighCommandGroupsAreObservers` off. Harmless and useful; not a fix for this report.

**Next step is the author's:** reproduce with `m_bDebugMode` on and send the `Sabotage demolishing at` line.

`tools/compile-check.sh` exit 0 (6349 files). Suite not run.

### 2026-08-25 — An interrupted hold now RESETS instead of pausing

**Author:** *"it should reset though"* — on learning that `ticksLeft` paused rather than reset when a hold was contested.

**Why it matters:** a paused clock banks progress. A sabotage team could be driven off five times and still finish on the sixth visit, because every quiet minute counted towards the same interval. Resetting is what makes "they have to hold the ground" true, and it is a large behaviour change in the player's favour on every timed occupying operation.

**All four hold decisions changed together**, because they are the same rule copied four times:

| Module | Decision |
|---|---|
| `OVT_BaseSabotageBehaviorDeploymentModule` | `EvaluateDemolition` |
| `OVT_TownHarassmentBehaviorDeploymentModule` | `EvaluateHold` |
| `OVT_TowerRecaptureBehaviorDeploymentModule` | `EvaluateRecapture` |
| `OVT_BaseRepairBehaviorDeploymentModule` | `EvaluateRepair` |

⚠ **The reset value is passed IN, as a new `int fullTicks` parameter, rather than read from difficulty inside the decision.** These four methods are deliberately pure — that is the whole point of the split, and the Logic/Init tiers pin them by calling them directly on a bare module with no deployment and no world. Reaching for `ResolveIntervalTicks()` inside them would have made them depend on `OVT_Global.GetDifficulty()` and broken every one of those cases. The callers, which already hold the module, pass `ResolveIntervalTicks()` / `ResolveHoldTicks()`.

⚠ **The tests were rewritten, not mechanically re-signatured.** Their assertions *were* the old rule — three separate suites asserted "an interruption must PAUSE the clock, not reset it" in as many words. Each now asserts the reset and then serves the whole interval from the top, which is the claim that actually distinguishes the two designs; a test that only checked the tick count after an interruption would pass under either.

⚠ **Interaction with the unresolved sabotage report above:** this removes the "banked ticks" explanation for it entirely. If a demolition still completes with defenders present, the cause is one of the two gates, not accumulated progress.

`tools/compile-check.sh` exit 0 (6349 files). ⚠ **Suite not run** — 3 rewritten case bodies across `OVT_TEST_Init_ObjectiveSabotage`, `OVT_TEST_Init_ObjectiveOperations` and `OVT_TEST_Logic_ObjectiveRepair` are unproven until the **All** group runs.
