# Test Coverage — Implementation Plan

**Epic:** dev-ops (feature #3 of 5)
**Status:** ✅ Ready for Review
**Started:** 2026-08-02
**Completed:** 2026-08-02 (61/62 tasks; 4.10 optional-deferred)
**Last Updated:** 2026-08-02 04:45

---

## Executive Summary

Feature #2 built a loop with nothing in it. This feature puts Overthrow's first real assertions inside that loop, in the three places regressions actually happen: **persistence round-trips**, **campaign logic** (economy, towns, jobs, skills, modifiers), and **manager/controller initialisation**.

It adds **no new tooling**. `tools/run-tests.sh`, `OVT_TEST_SuiteBase`, the `OVT_TEST_` naming and the `-autotest <target>` contract are inherited unchanged from #2. Save-state control reuses the project's existing `.scripts/reset_save.sh`, `backup_save.sh` and `activate_save.sh` rather than inventing a mechanism. The only genuinely new artefacts are two hand-authored `SCR_AutotestGroup` configs — required, not optional, because there is no run-everything CLI form and this feature adds the second suite (`autotest-foundation/findings.md`, Framework gaps #1).

Three facts discovered during planning shape everything below.

**1. Starting a campaign programmatically is easy.** `OVT_OverthrowGameMode.DoStartNewGame()` and `DoStartGame()` are plain public methods with no server guard, no RPC, no UI and no player dependency — the start-menu button calls exactly those two, in that order (`Scripts/Game/UI/Context/OVT_StartGameContext.c:157`). `HasGameStarted()` / `IsInitialized()` flip synchronously. The biggest unknown feature #2 handed over turns out to be a five-line Setup step.

**2. There is no working save path on this branch, in either system.** `OVT_PersistenceManagerComponent.SaveGame()`, `AutoSave()` and `OnGameEnd()` are `TODO(vanilla-persistence)` stubs that only print a warning; `HasSaveGame()` is hardcoded `return false`; `WipeSave()` is a no-op. And the migration re-parented `OVT_PersistenceManagerComponentClass` from `EPF_PersistenceManagerComponentClass` to `ScriptComponentClass`, so `EPF_PersistenceManagerComponent.OnPostInit()` never runs — EPF never reaches its SETUP state, its autosave tick never registers, and its world-load restore never happens. The EPF `SaveData` classes and 59 prefabs' `EPF_PersistenceComponent`s survive, driving nothing. **The requirement "these suites must pass against the current EPF implementation" is not satisfiable on this branch as it stands**, and pretending otherwise would produce exactly the kind of test that cannot fail.

**3. Save-state control already exists and is nearly usable.** `.scripts/reset_save.sh` (non-interactive) deletes the active save DB; `backup_save.sh` and `activate_save.sh` archive and restore it, but prompt interactively. All three default to the **Workbench** profile, while tests run under the game client's `OverthrowCI` profile. Small, well-defined work — and one sharp edge: `reset_save.sh` is an `rm -rf` on a path whose default is the user's real campaign save.

The plan confronts (2) head-on rather than routing around it. Persistence is split into two suites: a **green** suite of same-session state round-trips through Overthrow's public API (real coverage, ships in CI today) and a **quarantined round-trip suite** that adds save+reload, is excluded from every group, and whose exit code flipping from 1 to 0 becomes the literal, machine-checkable acceptance criterion for `vanilla-persistence`. That is a stronger artefact than a persistence test that silently passes because nothing was ever saved.

Everything unknown is settled by a short empirical spike (Phase 1) whose findings gate the later phases, mirroring feature #2's structure.

---

## Goals

### Primary Goals

1. **Four green suites** covering pure campaign maths, manager/controller initialisation, started-campaign integration, and same-session persistence behaviour — all passing, all provably able to fail.
2. **A behaviour-level persistence contract.** No `EPF_*` type, no vanilla persistence type, and no `OVT_*SaveData` class appears in any assertion. State is written and read exclusively through Overthrow's public manager API, so the suites survive the migration unchanged.
3. **A machine-checkable acceptance gate for `vanilla-persistence`**: a quarantined round-trip suite whose verdict is the migration's definition of done.
4. **A fast/slow split delivered as stable named targets** — two `SCR_AutotestGroup` configs — so feature #4 can run a quick subset on every push and the full set less often, without knowing what a suite is.
5. **Deterministic save state on demand**, via the existing `.scripts/` tools made callable from automation, so "fresh campaign" and "known saved state" are reproducible preconditions rather than hopes.
6. **Proven fallibility.** Every case is deliberately made red once during development, and the method is recorded. A test that cannot go red is a defect and does not ship.

### Secondary Goals

1. **A settled authoring pattern for tiers** — where a new case goes, whether it needs the campaign, how it resolves managers — written into the `workbench-workflow` skill so future coverage accrues without re-deciding.
2. **The `SCR_AutotestGroup` hand-authoring procedure proven** (feature #2 pre-specified it in Decision 4 but never had to fire it).
3. **Bugs found are logged, not fixed** — a findings record that feeds the backlog. Several are already visible from reading (see Risks R7).
4. **Docs corrected** — technical-design §10, mission-statement, the skill, `tools/README.md`, and `docs/features/core/persistence/`.

### Explicitly Out of Scope

- **Comprehensive coverage.** This establishes tiers and covers the highest-risk spine. Breadth accrues as features are touched.
- **Multiplayer / JIP tests** — needs two coordinated processes. Future epic.
- **UI tests**, performance/FPS tests, `modded class`-vs-patch regression detection.
- **Fixing any bug a test uncovers.** Logged as separate work, never silently patched to make a suite green.
- **Repairing the persistence implementation.** This feature writes the gate, not the thing the gate measures.
- **Rewriting or relocating the `.scripts/` save tools.** They gain non-interactive argument forms and documentation; their location, defaults and interactive behaviour for the user stay.

---

## Architecture Overview

### Verified ground truth (read from source, 2026-08-01)

| Fact | Source | Consequence |
|---|---|---|
| `DoStartNewGame()` (`:103`) and `DoStartGame()` (`:173`) are plain public methods; no `Replication.IsServer()` guard, no RPC, no player dependency. `PrepareConnectedPlayers()` no-ops on an empty player list. | `Scripts/Game/GameMode/OVT_OverthrowGameMode.c` | A test can start the campaign from a Setup step. This is the whole of Phase 3's mechanism. |
| `m_bGameStarted` set at `DoStartGame:180`; `m_bGameInitialized` at `:269`. `HasGameStarted()` / `IsInitialized()` expose them. | same | `IsInitialized()` is the stronger post-start assertion; both are true synchronously on return. |
| Neither method is idempotent or re-entrancy guarded; a double call re-runs every `PostGameStart()` and re-registers repeating timers. | same | Every start step must be guarded by `if (!mode.HasGameStarted())`. |
| `DoStartNewGame()` → `OVT_OccupyingFactionManager.NewGameStart()` reads `m_Difficulty.baseThreat` **before** `DoStartGame()`'s null-default. The test world's layer overrides `m_aDifficultyPresets` only, so a direct call runs on **Normal**, not TestWorld difficulty. | `OVT_OccupyingFactionManager.c:206-209`, `OVT_Campaign_Test_Layers/default.layer:37` | The Setup step sets `m_Difficulty = m_aDifficultyPresets[0]` first, exactly as `OVT_StartGameContext.OnShow()` does. Deterministic starting cash 100000 / resources 200 from `Difficulty_TestWorld.conf`. |
| Post-start deferred work: shops/bases/garrisons ≈1 frame; occupying-faction resource distribution **5 s**; deployment evaluation **10 s**. | `OVT_EconomyManagerComponent.c:1370`, `OVT_OccupyingFactionManager.c:253-260`, `OVT_DeploymentManager.c:92` | Campaign-tier cases poll with bool steps and generous `timeoutS`; assertions needing deployments are deferred (out of scope). |
| Every manager caches `static s_Instance` and **nothing ever nulls it**; the harness loads the world **three times per launch**. | `OVT_TownManagerComponent.c:146-160` (pattern repeats), `autotest-foundation/findings.md` §"Harness runs twice" | Stale singletons pointing at a destroyed game mode are a first-class flake source. Suites resolve managers via `FindComponent` on the live game mode, not blindly via `OVT_Global`. |
| `Setup_OpenWorld` skips the transition entirely when `GetWorldFile()` is empty (`if (world && ...)`). | `SCR_AutotestSuiteBase.c:52-63` | A genuinely world-free fast tier is architecturally possible — **if** overriding `GetWorldFile()` compiles across the module boundary (feature #2 deliberately never tested this; R1/`SCR_Hack_AutotestSuiteBase`). Cheap to settle: one compile-check. |
| A group config instantiates suites and `ConfigureTestSuites` enables them by `ClassName()`; each enabled suite runs its own `Setup_OpenWorld`. | `SCR_AutotestHarness.c:184-208`, `SCR_AutotestGroup.c:1-26` | N suites in one launch = N world-transition requests. Suite count, not case count, is the runtime driver. |
| `run-tests.sh` accepts `{GUID}` as a positional target with no new flag; artifacts and the 0/1/2/124 taxonomy are unchanged. | `tools/README.md` | The fast/slow split needs zero tooling change. |
| `SaveGame()`/`AutoSave()`/`OnGameEnd()` print a WARNING and return; `HasSaveGame()` returns `false` unconditionally; `WipeSave()` is a no-op; `PreShutdownPersist()` is empty. | `OVT_PersistenceManagerComponent.c:19-85`, `OVT_OverthrowGameMode.c:419-422` | No save path. Drives the persistence ladder below. |
| `OVT_PersistenceManagerComponentClass` now extends `ScriptComponentClass`; pre-migration it extended `EPF_PersistenceManagerComponentClass` (still visible in `generated-docs/html/hierarchy.js:52-54`). No `EPF_PersistenceManagerComponent` exists in the repo. | repo | EPF never reaches SETUP: no DB connection, no autosave tick, no world-load restore. The game-mode prefab entry is unchanged — only the script class was re-parented. |
| The player-facing save path is `OVT_MainMenuContext` → `OVT_Global.GetServer().RequestSave()` → `RpcAsk_RequestSave()` → `persistence.SaveGame()`. The UI shows `#OVT-Saved` **regardless of outcome**. | `OVT_MainMenuContext.c:262-271`, `OVT_PlayerCommsComponent.c:10-25` | `SaveGame()` is the seam tests use (server-side, no player, no RPC). The UI's unconditional success message is a bug to **log**, not fix. |
| Historical EPF save location was `$profile:/.db/Overthrow` with **no named slots**. Vanilla's `SaveGameManager` does offer slots (`RequestSavePoint`/`GetSaves`/`Load`/`Delete`) and is referenced **nowhere** in Overthrow. | `EDF_FileDbDriverBase.c:19`, `SaveGameManager.c` | The `.scripts/` tools are written against the EPF `.db` shape. The migration will move the location; the scripts will need updating then, which this plan records. |
| `OVT_InventoryManagerSaveData.ReadFrom`/`ApplyTo` are empty stubs, and character held-item persistence is deliberately disabled pending an EPF bug. | `OVT_InventoryManagerSaveData`, `OVT_CharacterControllerComponentSaveData.c:16` | Inventory coverage is deferred with cause, not from laziness. |
| Test world has exactly **one** `OVT_TownController` and one `OVT_BaseController`. | `OVT_Campaign_Test_Layers/default.layer:209,223` | Assert `>= 1`, never a magic count. Eden-sized expectations would be red on arrival. |
| Navmesh fails to load in the test world; a pre-existing `Failed to get SCR_PersistenceSystem instance!` error fires once per world load. | `autotest-foundation/findings.md` §Caveats | No AI-movement assertions. The error is inherited noise, not a regression signal. |
| `ovt_profile_dir <name>` resolves to `<My Games>/<name>`, so the client's `-profile OverthrowCI` gives `<My Games>/OverthrowCI` — **not** the Workbench's `<My Games>/ArmaReforgerWorkbench/profile`, which is what `.scripts/*` default to. | `tools/lib/common.sh:284-296`, `.scripts/reset_save.sh:5` | Every automated call to the save scripts must set `OVERTHROW_SAVE_DIR`. Forgetting it deletes the user's real save. |

### Suite tiers

Suites are organised by **setup cost**, not by subject area, because the suite — not the case — is the unit at which the world transition and the campaign start happen.

```
Tier A  OVT_TEST_LogicSuite        no campaign, no manager, ideally no world  (~8-15 s)
        pure maths: OVT_TownData, modifier Recalculate, pure job conditions,
        skill effects, OVT_PlayerData levelling

Tier B  OVT_TEST_InitSuite         world + managers, campaign NOT started      (~15 s)
        OVT_Global sweep, towns populated, controllers registered,
        economy price/demand seams (SetPrice/SetDemand)

Tier C  OVT_TEST_CampaignSuite     campaign started in suite Setup             (~20-30 s)
        post-start manager state, town activation, shop init, economy income

Tier D  OVT_TEST_PersistenceSuite  campaign started; same-session round-trips  (~25-35 s)
        write state via public API -> read back via public API

Tier D' OVT_TEST_PersistenceRoundTripSuite   QUARANTINED - save + reload
        excluded from every group; red today by design; green = migration done

Meta    OVT_TEST_MetaSuite         inherited from #2, unchanged, always red
Smoke   OVT_TEST_SmokeSuite        inherited from #2, unchanged, always green
```

Case names keep #2's convention with the tier as the `<Area>` token: `OVT_TEST_Logic_TownSupport_IsPercentageOfPopulation`, `OVT_TEST_Init_Towns_ArePopulated`, `OVT_TEST_Campaign_Economy_ShopsInitialise`, `OVT_TEST_Persistence_TownControl_SurvivesRoundTrip`.

### Save-state control

Deterministic starting state is a **precondition of the run**, established outside the game before launch — not something a test case can arrange from inside EnforceScript, since the save DB lives on disk and is read during world init. The project already has the three tools for this and they are reused as-is, gaining only argument forms:

| Script | Today | After Phase 2 |
|---|---|---|
| `.scripts/reset_save.sh` | Non-interactive `rm -rf` of `$OVERTHROW_SAVE_DIR` (defaults to the **Workbench** profile) | Unchanged behaviour, plus a refusal to delete a path that does not look like a save DB, and a `--profile <name>` convenience that resolves the game-client profile via `tools/lib/common.sh` |
| `.scripts/backup_save.sh` | Interactive `read -p` for the archive name | `backup_save.sh [<name>]` — name as `$1` skips the prompt; no argument keeps today's prompt |
| `.scripts/activate_save.sh` | Interactive numbered menu | `activate_save.sh [<name-or-file>]` — argument selects (exact file, or newest archive whose name matches); no argument keeps today's menu |

Usage in this feature:

- **Fresh campaign** (Tiers B, C, D): reset the `OverthrowCI` save DB before the run, so first-boot assertions are not influenced by a stray save. Harmless today (`HasSaveGame()` is hardcoded false) and *required* the moment persistence works — which is exactly why it is wired now rather than retrofitted.
- **Known saved state** (fixture mechanism, Tier D'): `activate_save.sh <name>` restores a committed-by-hand archive before a run, letting the quarantined round-trip suite eventually be split into "load a known save and assert its contents" without the write half. Not exercised while no load path exists; the mechanism and its naming convention (`testworld_<situation>_SP`) are documented so it is available the day it works.

The canonical documentation lives in `tools/README.md` alongside every other automation contract, with pointers from the scripts' own headers and from the `workbench-workflow` skill. The scripts stay in `.scripts/` — moving them would break the user's existing habits for no benefit.

### File structure

```
Scripts/Game/Tests/
├── TestFramework/
│   ├── OVT_AutotestFramework.c              (unchanged from #2)
│   └── OVT_TEST_SuiteBase.c                 EXTENDED: campaign-start opt-in + manager resolution
└── TestSuites/
    ├── Smoke/OVT_TEST_SmokeSuite.c          (unchanged)
    ├── Meta/OVT_TEST_MetaSuite.c            (unchanged)
    ├── Logic/
    │   ├── OVT_TEST_LogicSuite.c            suite class + tier header
    │   ├── OVT_TEST_Logic_Town.c            town maths + modifier recalculation
    │   ├── OVT_TEST_Logic_Jobs.c            pure job conditions
    │   └── OVT_TEST_Logic_Skills.c          skill effects + player levelling
    ├── Init/OVT_TEST_InitSuite.c            suite + cases (single file; small)
    ├── Campaign/
    │   ├── OVT_TEST_CampaignSuite.c
    │   └── OVT_TEST_Campaign_Economy.c
    └── Persistence/
        ├── OVT_TEST_PersistenceSuite.c      green: same-session round-trips
        └── OVT_TEST_PersistenceRoundTripSuite.c   QUARANTINED

Configs/Tests/                                NEW directory
├── OVT_TestGroup_Fast.conf(+.meta)          Logic + Init
└── OVT_TestGroup_All.conf(+.meta)           Logic + Init + Campaign + Persistence

.scripts/                                     EXISTING - argument forms added, not rewritten
├── reset_save.sh
├── backup_save.sh
└── activate_save.sh

docs/features/dev-ops/test-coverage/
├── implementation.md                         this file
└── findings.md                               NEW - Phase 1 empirical record + bug log + can-fail table
```

One suite class per file; cases may live in sibling files referencing the suite typename. That is the growth path — new coverage adds a case file, not a suite, so launch cost does not grow with coverage.

### `OVT_TEST_SuiteBase` extension

Two additions, both no-ops for the inherited Smoke and Meta suites:

- `bool RequiresStartedCampaign()` — virtual, default `false`. Overridden `true` by the Campaign and Persistence suites.
- A `[Step(EStage.Setup)]` bool step that, when the virtual returns true, sets the TestWorld difficulty preset, calls `DoStartNewGame()` + `DoStartGame()` **guarded by `!HasGameStarted()`**, closes the start-menu layout if it opened, and then polls `IsInitialized()` before returning true.
- A manager-resolution helper that finds components on the **live** game mode rather than trusting `OVT_Global`'s never-invalidated statics, plus a documented rule about when each is appropriate.

Base-class steps run before derived-suite steps (BI's own `Setup_OpenWorld`/`Setup_AwaitWorld` rely on this), so the world is loaded before the campaign start attempt. Verified in Phase 1.

### Persistence round-trip mechanics — the ladder

Assertions are behaviour-level under all rungs: state is written through a public manager method and read back through a public manager method. No persistence type of any flavour appears in an assertion. The *trigger* is the single permitted seam — `OVT_Global.GetOverthrow().GetPersistence().SaveGame()` — chosen over the player-facing `RequestSave()` RPC because it is server-side, needs no player entity, and is the method the RPC itself ends up calling.

| Rung | Condition | Mechanism |
|---|---|---|
| **L1** | A save path exists and an in-session reload preserves the script VM | mutate → `SaveGame()` → request scenario change → re-resolve managers → assert. Cheapest and preferred. |
| **L2** | A save path exists but reload needs a fresh process | Two suites, `..._Write` then `..._Verify`, run as an ordered pair of `run-tests.sh` invocations ANDed by the caller, with `reset_save.sh` before the pair. The verify suite must fail loudly when no save is present, so it can never pass vacuously. |
| **L3** | **No save path (expected — see ground truth)** | `OVT_TEST_PersistenceSuite` ships green with same-session write→read-back coverage. `OVT_TEST_PersistenceRoundTripSuite` ships with the full round-trip, is excluded from both groups, and is documented as red-until-migrated. |
| **L4** | Optional, needs user approval | Validate the round-trip suite against a `main` worktree (where `OVT_PersistenceManagerComponentClass` still extends EPF's) using `OVERTHROW_GAME_ADDONS_DIRS`. Cost: a second addon tree plus porting the test tree onto that worktree. Not on the critical path. |

Phase 1 determines the rung. The plan is written so that L3 — the expected outcome — still delivers real, green, behaviour-level coverage rather than a folder of skipped tests.

### Data flow

```
[optional] OVERTHROW_SAVE_DIR=<OverthrowCI .db> .scripts/reset_save.sh       fresh-campaign precondition
[optional] OVERTHROW_SAVE_DIR=<...> .scripts/activate_save.sh <name>         known-state fixture
  |
tools/run-tests.sh <target>        target = suite class | case class | {GROUP GUID}
  └─ tools/launch-game.sh -- -autotest <target>
       └─ SCR_TestRunner → harness → suite Setup (open world, await, close menus,
                                     [OVT] start campaign if required)
                                   → cases (Setup → Main bool steps → TearDown)
                                   → junit.xml → RequestClose()
  └─ artifacts to .tmp/run-tests/, verdict 0/1/2/124
```

---

## Implementation Phases

### Phase 1: Empirical spike — settle the unknowns

**Agent tier: ADVANCED (opus). Requires a Bash-capable agent** (CLI runs + log forensics; the project's `*-advanced` EnforceScript agents have no Bash).

**Goal:** replace every assumption below with an observed fact in `findings.md`, then amend Phases 2-7 in place. Nothing downstream starts until the gate is applied. Throwaway probe code is fine and is deleted at the end of the phase; only findings survive.

**Tasks:**

- [ ] 1.1 Create `docs/features/dev-ops/test-coverage/findings.md` using #2's shape: Reforger build stamp at the top, per-experiment table (command → observed outcome → wall time → artifacts → notes), "Differs from assumptions" and "Bugs found (log only)" sections.
- [ ] 1.2 **World-free tier feasibility (compile only, ~5 s).** Add a throwaway suite overriding `GetWorldFile()` to return an empty `ResourceName` and run `tools/compile-check.sh`. Records whether Tier A can skip the world load, or whether the cross-module sealing hazard (`SCR_Hack_AutotestSuiteBase`, `#ifdef MODULE_AUTOTEST`) blocks it. If it compiles, run it and record whether `Requesting scenario change:` is absent and the wall time.
- [ ] 1.3 **Campaign start.** In a throwaway case: set `m_Difficulty = m_aDifficultyPresets[0]`, call `DoStartNewGame()` + `DoStartGame()`, poll `IsInitialized()`. Record: does it work; how many ticks/seconds to `IsInitialized()`; what appears in `console.log` (expect `Starting Economy/Towns/Occupying Faction/…`); whether the start menu interferes; whether `GetStartGameContext().CloseLayout()` is needed; any errors.
- [ ] 1.4 **Post-start settling budget.** From the same run, record when shops, base controllers and garrisons actually exist, and confirm the 5 s (occupying-faction resources) / 10 s (deployments) deferrals. Produces the `timeoutS` values Phases 4-5 use.
- [ ] 1.5 **Stale-singleton check.** Log, across the run's three world loads, whether `OVT_Global.GetTowns()` returns the same object as `FindComponent` on the live game mode. This decides whether the suite base's manager-resolution helper is mandatory or merely defensive. Record verbatim.
- [ ] 1.6 **Base-class step ordering.** Confirm empirically that a `[Step(EStage.Setup)]` added to `OVT_TEST_SuiteBase` runs *after* the inherited `Setup_AwaitWorld` and before derived steps. If not, record the real order — the campaign-start step's placement depends on it.
- [ ] 1.7 **Persistence reality check.** Call `GetPersistence().SaveGame()` from a case with the campaign started, and record: what is printed, whether anything is written to disk anywhere under the profile dir, and what `HasSaveGame()` returns. Also grep `console.log` for any EPF initialisation at all. **This selects the ladder rung.** Expected: L3.
- [ ] 1.8 **Save-directory determination.** Establish empirically where the `OverthrowCI` profile's save DB would live — i.e. what `$profile:/.db/Overthrow` resolves to for a `-profile OverthrowCI` client run — by inspecting the profile dir created by a real run (expected `<My Games>/OverthrowCI/.db/Overthrow`, per `ovt_profile_dir`). Record the exact WSL path, whether the directory exists after a run, and how it differs from `.scripts/*`'s Workbench default. **Also record what a vanilla-persistence save would use instead** (`SaveGameManager` storage, unused today) so the scripts' eventual migration is on record.
- [ ] 1.9 **In-session reload survivability (only if 1.7 finds a save path).** Request a scenario change from inside a Main step and record whether the script VM, the harness statics and the test-case instance survive, i.e. whether the step resumes after the transition. Decides L1 vs L2.
- [ ] 1.10 **Group config proof.** Hand-author `Configs/Tests/OVT_TestGroup_Probe.conf` + `.conf.meta` per feature #2's Decision 4 (root `SCR_AutotestGroup`, `m_aSuites` listing `OVT_TEST_SmokeSuite`; 16 uppercase hex GUID, collision-checked against this repo, the reference tree and the game install). Run `tools/run-tests.sh "{GUID}"`; require `CLI autotest config: SCR_AutotestGroup` in `console.log` and the smoke case in `junit.xml`. Then add a second suite and record **whether both run and what the extra world transition costs**. If suite classes cannot be instantiated from the container, try `[BaseContainerProps()]` on the suite class and record the result.
- [ ] 1.11 **Timing baseline.** Wall times for: one Tier-A-shaped suite, one Tier-B-shaped suite, one campaign-start suite, and a two-suite group. Feeds the group composition decision and #4's CI budget.
- [ ] 1.12 Delete all throwaway probe code. Run `tools/compile-check.sh` → exit 0. Write up "Differs from assumptions".

**Estimated time:** 3-5 hours (dominated by launch wall-time and careful recording).

**Acceptance criteria:**
- [ ] `findings.md` exists with an observed result for every task 1.2-1.11, including the ones that changed nothing.
- [ ] The persistence ladder rung is **named and justified with quoted evidence**.
- [ ] Campaign start is proven working (or proven impossible, with the failure quoted).
- [ ] The `OverthrowCI` save-DB path is recorded as an exact WSL path.
- [ ] A group config has been proven to load and run ≥2 suites, with its GUID recorded.
- [ ] Tier A's world-free status is settled either way.
- [ ] No probe code remains; compile-check exits 0.

> **Gate: APPLIED 2026-08-02** against `findings.md`. Rulings now baked into Phases 2-7 below:
> - **Tier A is world-free** (1.2): `GetWorldFile()` → `ResourceName.Empty` compiles and runs; no scenario change, harness runs ONCE, 8 s wall. `GetGameMode()` is null there — Tier A purity is engine-enforced.
> - **Persistence rung is L3** (1.7): `SaveGame()`/`AutoSave()` return **silently** (the TODO warning is itself guarded by a null `m_PersistenceSystem`); `HasSaveGame()` false always; zero disk writes; EPF mounts but never initialises. 1.9 N/A (script VM does survive an in-session transition, so L1 is the likely rung post-migration).
> - **Save path** (1.8): `<My Games>/OverthrowCI/profile/.db/Overthrow` — note the extra `profile/` level vs. the original prediction. `$saves:` is not script-writable. Vanilla `SaveGameManager` exposes no path (engine-sealed) — the scripts' replacement location can only be found empirically after the migration.
> - **Campaign start works** (1.3): same-frame `HasGameStarted()`/`IsInitialized()`, 71 ms. **`m_aDifficultyPresets[0]` is 'Easy'** — the world layer's override APPENDS (5 presets at runtime, 'Test World' at index 4). **Select by `preset.name == "Test World"`**, never by index. Keep `CloseLayout()` (menu opens ~4 ms after Setup — races any multi-frame step).
> - **Stale singletons do NOT manifest** (1.5): the engine nulls the weak statics on destruction; `OVT_Global` == `FindComponent` in 100% of samples. Decision 6's helper is defensive documentation, not a flake fix; R3 downgraded.
> - **Group configs work** (1.10) with one mandatory prerequisite: **`[BaseContainerProps()]` on every concrete suite class**, or the group silently instantiates nothing (`Unknown class` error, empty `<testsuites>`, exit 2 — looks like a bad GUID). Execution order is **alphabetical by class name**, not `m_aSuites` order. +1 transition ≈ +1 s per extra suite.
> - **Settling/`timeoutS`** (1.4): managers/shops/bases ≤ 604 ms; resource distribution ≈ 6.5 s; deployments ≈ 12 s (real — the autotest client spawns a local player). Budgets: **30** (init/towns/bases/shops), **45** (resource distribution), **60** (deployments, out of scope). Garrisons never populate in this world — never assert on them.
> - **CI caveat for #4**: a campaign start emits **62 deterministic VM exceptions** (`FillAmmoboxes` null deref via `DistributeInitialResources`, logged as Bug #1) — `console.log` error counts are NOT a usable CI signal until fixed.

---

### Phase 2: Save-state control

**Agent tier: STANDARD (high). Bash only — no EnforceScript.** Small, self-contained, and safely parallelisable with Phase 3.

**Goal:** make the three existing save tools callable from automation without changing what they do for the user, and make it impossible for an automated call to destroy the user's real save.

**Tasks:**

- [ ] 2.1 `.scripts/backup_save.sh [<name>]` — name from `$1` when present (same sanitisation as today), interactive prompt when absent. Same archive naming, same `.saves/` location, same output.
- [ ] 2.2 `.scripts/activate_save.sh [<name-or-file>]` — argument selects an archive: an exact path or filename first, otherwise the newest `.saves/*.tar.gz` whose name matches. Unmatched argument exits non-zero with the available list; no argument keeps today's numbered menu. Same reset-then-extract behaviour.
- [ ] 2.3 **Destructive-path guard in `reset_save.sh`.** Refuse to `rm -rf` a path that does not end in a save-DB-shaped suffix (e.g. `.db/Overthrow`), is `/`, or is empty. Print the resolved path before deleting. This is the single highest-value change in the phase: the default target is the user's real campaign save, and a forgotten `OVERTHROW_SAVE_DIR` in a script or CI job is otherwise silently destructive.
- [ ] 2.4 **Profile convenience:** accept `--profile <name>` on all three (default: keep the current `OVERTHROW_SAVE_DIR`-or-Workbench behaviour), resolving **`<My Games>/<name>/profile/.db/Overthrow`** (note the `profile/` level — finding 1.8; e.g. `--profile OverthrowCI` → `/mnt/c/Users/Aaron Static/OneDrive/Documents/My Games/OverthrowCI/profile/.db/Overthrow`) via `tools/lib/common.sh`'s `ovt_profile_dir`. Explicit `OVERTHROW_SAVE_DIR` continues to win.
- [ ] 2.5 Exit codes: 0 success, non-zero with a message on every failure path (missing save dir, unmatched name, refused path). No silent success.
- [ ] 2.6 Verify by execution against a **throwaway** directory (never the user's real save): reset on a missing dir, reset on a present dir, backup with and without an argument, activate by name and by filename, activate with an unmatched name, reset with a guard-violating path. Record the matrix in `findings.md`.
- [ ] 2.7 Confirm the interactive paths still behave exactly as before when called with no arguments.

**Estimated time:** 1.5-2 hours.

**Acceptance criteria:**
- [ ] All three scripts work non-interactively with arguments and interactively without them.
- [ ] `reset_save.sh` refuses an implausible path and prints what it resolved before deleting.
- [ ] The verification matrix is recorded, run against a throwaway directory.
- [ ] No change to archive format, `.saves/` location or naming convention.

---

### Phase 3: Suite base extension + Tier B (initialisation / integration)

**Agent tier: STANDARD (high).** EnforceScript authoring against a now-known API; compile-check catches signature errors in ~5 s. Needs Bash to run `tools/run-tests.sh`.

**Goal:** the cheapest possible guard against the "everything is null on startup" class of breakage, plus the shared campaign-start machinery that Phases 4-5 depend on.

**Tasks:**

- [ ] 3.1 Extend `OVT_TEST_SuiteBase`: `RequiresStartedCampaign()` virtual (default `false`); the guarded campaign-start Setup step (**select the difficulty preset by `preset.name == "Test World"` — NEVER by index; index 0 is 'Easy'** (finding 1.3) → `DoStartNewGame()` → `DoStartGame()` → `CloseLayout()` on the start-game context (kept: the menu opens ~4 ms after Setup and races multi-frame steps) → check `IsInitialized()`, same-frame per finding 1.3); a manager-resolution helper per finding 1.5 (defensive only — stale statics proven NOT to manifest; document that). Header documents each and states plainly that the campaign start is a *test* concern and must never be relied on by shipped code.
- [ ] 3.2 Verify Smoke and Meta suites still behave exactly as before (`run-tests.sh` → 0, `run-tests.sh OVT_TEST_MetaSuite` → 1). The base class is shared; a regression here breaks #2's standing proofs.
- [ ] 3.3 Create `OVT_TEST_InitSuite` (`RequiresStartedCampaign()` = false).
- [ ] 3.4 Case: every `OVT_Global` manager getter that does not need a local player returns non-null — Config, Difficulty, Economy, Players, RealEstate, Vehicles, Towns, OccupyingFaction, ResistanceFaction, Jobs, Notify, Factions, Skills, Inventory, DeploymentManager, Recruits, Loadouts, plus `GetOverthrow()`. One case, one failure message naming the first null getter. Explicitly **excludes** `GetServer`/`GetUI`/`GetController`/`GetContainerTransfer` (they dereference a local controlled entity that does not exist here).
- [ ] 3.5 Case: towns are populated — `GetTowns().m_Towns.Count() >= 1` and the first town has non-zero population and a location. Never a magic count (the test world defines exactly one town).
- [ ] 3.6 Case: controllers are registered — at least one town controller and at least one base controller are reachable through their managers, and the town controller resolves to a town in `m_Towns`.
- [ ] 3.7 Case: economy is seeded — the price/demand maps respond to the public seams (`SetPrice`/`SetDemand` then `GetPrice`/`GetDemand` round-trips), and `GetBuyPrice(id, "0 0 0", -1)` applies `m_fShopProfitMargin` as specified.
- [ ] 3.8 Establish the fresh-campaign precondition: run this suite after `OVERTHROW_SAVE_DIR=<OverthrowCI path> .scripts/reset_save.sh` and record that the verdict is unchanged with and without it (it should be, today). This proves the wiring works before persistence makes it load-bearing.
- [ ] 3.9 **Can-fail proof for every case in this suite.** Break the covered thing (e.g. seed a different price, assert against a deliberately wrong expected value), run, observe exit 1, revert. Record method + observed failure text per case in `findings.md`.
- [ ] 3.10 `tools/compile-check.sh` → 0; `tools/run-tests.sh OVT_TEST_InitSuite` → 0, three consecutive runs identical.

**Estimated time:** 2-3 hours.

**Acceptance criteria:**
- [ ] `run-tests.sh OVT_TEST_InitSuite` exits 0, deterministically over 3 runs.
- [ ] `run-tests.sh` (default, Smoke) still exits 0 and `OVT_TEST_MetaSuite` still exits 1.
- [ ] Every case has a recorded can-fail proof.
- [ ] The suite base's campaign-start step is present and demonstrably inert when `RequiresStartedCampaign()` is false.
- [ ] The reset-before-run precondition is exercised and recorded.

---

### Phase 4: Persistence — Tier D (green) and Tier D' (quarantined gate)

**Agent tier: ADVANCED (opus).** Highest-priority requirement, load-bearing for `vanilla-persistence`, and the phase most likely to meet a surprise. Needs Bash.

**Goal:** behaviour-level persistence coverage that ships green today, plus the round-trip suite that becomes the migration's acceptance gate.

> **Assertion rule, non-negotiable:** no `EPF_*`, no vanilla persistence type, and no `OVT_*SaveData` class may appear anywhere in these files except the single documented save-trigger call. Every assertion reads state back through the same public manager API that wrote it. A reviewer must be able to `grep -rn "EPF_\|SCR_Persistence\|SaveData" Scripts/Game/Tests/` and find at most the one annotated trigger line.

**Tasks:**

- [ ] 4.1 Create `OVT_TEST_PersistenceSuite` with `RequiresStartedCampaign()` = true. File header states the assertion rule verbatim.
- [ ] 4.2 Same-session round-trips through the public API, one case per state kind. Each: mutate via the manager's public mutator → read back via the manager's public accessor → assert. Cover the spine:
  - town control / stability / population
  - player money
  - player skills / XP / level
  - real estate ownership
  - recruits
- [ ] 4.3 Document, in the suite header and in `findings.md`, the state kinds deliberately deferred and why: vehicles and placed structures persist through per-entity components on spawned prefabs; container inventories are not saved by `OVT_InventoryManagerSaveData` at all (its `ReadFrom`/`ApplyTo` are empty stubs); character held items are deliberately disabled pending an EPF bug; loadouts live in a separate scripted-state path whose repository methods are unimplemented stubs. These are the growth path, not this feature's spine.
- [ ] 4.4 Create `OVT_TEST_PersistenceRoundTripSuite` — same cases as 4.2, with `SaveGame()` + reload inserted between mutate and assert, using the ladder rung Phase 1 selected. File header carries a capitalised warning in #2's `OVT_TEST_MetaSuite` style: **quarantined, in no group, red by design until `vanilla-persistence` lands**.
- [ ] 4.5 Make the quarantined suite's failure *diagnostic*: when no save path exists it must fail with a message that names the missing capability, not with a null dereference or a timeout. A reviewer reading `junit.xml` should learn why. **Load-bearing, not a nicety** (finding 1.7): `SaveGame()` returns silently — no print, no disk write, `HasSaveGame()` stays false — so without an explicit capability assertion (e.g. `HasSaveGame()` true after `SaveGame()`, or persistence-system-resolved check) the failure surfaces as a confusing value mismatch far downstream.
- [ ] 4.6 **Anti-vacuous-pass guard:** the round-trip suite fails if the save it expects does not exist. Under L2 (two-launch) the verify suite must fail loudly on a missing save; under L3 it fails on the stubbed trigger. It must be impossible for this suite to go green without persistence actually working.
- [ ] 4.7 Document the run recipe for the round-trip suite — `reset_save.sh` (fresh) or `activate_save.sh <name>` (known state) with `OVERTHROW_SAVE_DIR` set to the Phase 1 path, then `run-tests.sh <suite>` — in the suite header and in `tools/README.md`, so that whoever completes the migration has the acceptance procedure written down rather than reconstructed.
- [ ] 4.8 Can-fail proof for every green case in 4.2; record method + observed text.
- [ ] 4.9 `run-tests.sh OVT_TEST_PersistenceSuite` → 0 (3 consecutive runs identical); `run-tests.sh OVT_TEST_PersistenceRoundTripSuite` → 1 with the diagnostic message, recorded verbatim in `findings.md`.
- [ ] 4.10 *(Optional, requires explicit user approval — do not start unassisted.)* L4: validate the round-trip suite against a `main` worktree via `OVERTHROW_GAME_ADDONS_DIRS`, to satisfy "passes against EPF" literally. Record the outcome either way; abandoning it is an acceptable, documented result.

**Estimated time:** 4-6 hours.

**Acceptance criteria:**
- [ ] `grep -rn "EPF_\|SCR_Persistence\|SaveData" Scripts/Game/Tests/` returns at most the single annotated trigger line.
- [ ] `run-tests.sh OVT_TEST_PersistenceSuite` exits 0 deterministically.
- [ ] `run-tests.sh OVT_TEST_PersistenceRoundTripSuite` exits 1 with a message that names the missing capability (or exits 0, if Phase 1 found a working path — in which case it joins the All group and the quarantine is deleted).
- [ ] The round-trip suite appears in **no** group config.
- [ ] The acceptance procedure (save-state precondition + command + expected exit codes) is written down.
- [ ] Every green case has a can-fail proof.

---

### Phase 5: Campaign logic — Tier A (pure) and Tier C (started campaign)

**Agent tier: STANDARD (high).** Routine authoring against patterns Phases 3-4 have settled; the risky decisions are already made. Needs Bash.

**Goal:** breadth over the deterministic, high-value systems. Prefer the pure tier wherever the logic allows — those cases cost nothing and never flake.

**Tasks:**

- [ ] 5.1 Create `OVT_TEST_LogicSuite` **world-free** (finding 1.2: `override ResourceName GetWorldFile() { return ResourceName.Empty; }` — no scenario change, harness runs once, ~8 s). Header states the tier rule: **nothing in this suite may touch a manager, the game mode, or the world** (`GetGameMode()` is null here anyway — engine-enforced).
- [ ] 5.2 `OVT_TEST_Logic_Town.c`:
  - `OVT_TownData.SupportPercentage()` boundary table (0 population, support < population, support == population, support > population). Construct `OVT_TownData` directly.
  - `OVT_TownModifierSystem.Recalculate()` — summing, clamping at `min`/`max`, empty-modifier identity. Hand-built `m_Config`, no `Init()`.
  - `OVT_TownSupportModifierSystem.Recalculate()` — the deterministic branches only (`> 75`, `< -75`, `max == 0`, clamps). **The RNG-gated branch is deliberately not tested** and a comment says so; `maxAttempts` is not an acceptable substitute.
  - `OVT_TownData.IsWithinTownBounds()`, `GetAreaHeat`/`SetAreaHeat` clamping, `CopyFrom`.
- [ ] 5.3 `OVT_TEST_Logic_Jobs.c`: `OVT_TownSupportJobCondition` (min/max/unset combinations against a hand-built town), `OVT_TownHasDealerJobCondition` (set and unset dealer position), `OVT_RandomJobCondition` at its deterministic edges (`m_fChance` 0 → never, 100 → always).
- [ ] 5.4 `OVT_TEST_Logic_Skills.c`: each `OVT_SkillEffect.OnPlayerData()` writes exactly the field it claims on a fresh `OVT_PlayerData` (trade discount → `priceMultiplier`, stealth → `stealthMultiplier`, support → `diplomacy`, permission → `HasPermission`); `OVT_GivePermissionSkillEffect` idempotency; `OVT_PlayerData.GetRawLevel`/`GetLevel`/`GetLevelXP`/`CountSkills`. `OVT_StaminaSkillEffect` is documented as intentionally inert, not asserted.
- [ ] 5.5 Create `OVT_TEST_CampaignSuite` (`RequiresStartedCampaign()` = true) with `OVT_TEST_Campaign_Economy.c`. Cases: `HasGameStarted()` and `IsInitialized()` both true after Setup; towns are activated post-start; shop inventory initialises; `GetTaxIncome()`/`GetDonationIncome()` return a value consistent with the town state the suite set up. Use the settling budget from finding 1.4 for every `timeoutS`.
- [ ] 5.6 **Bugs found are logged, never fixed and never papered over.** Reading already suggests several integer-division defects (`OVT_TownData.SupportPercentage`, `OVT_EconomyManagerComponent.GetTaxIncome`, `GetSellPrice`'s stock term, `OVT_PlayerData.GetLevelProgress`/`GetNextLevelXP`) plus a UI that reports a successful save unconditionally. Where behaviour is clearly unintended, **pin the current behaviour with a case whose name and comment say it is pinning a suspected bug**, and record it in `findings.md` under "Bugs found (log only)". Do not change gameplay code, and do not weaken the case to hide the finding.
- [ ] 5.7 Can-fail proof for every case; record method + observed text.
- [ ] 5.8 `run-tests.sh OVT_TEST_LogicSuite` → 0 and `run-tests.sh OVT_TEST_CampaignSuite` → 0, each three times identically.

**Estimated time:** 4-6 hours.

**Acceptance criteria:**
- [ ] Both suites exit 0, deterministically over 3 runs.
- [ ] No case in `OVT_TEST_LogicSuite` references a manager, the game mode or the world.
- [ ] No `maxAttempts` anywhere without a `findings.md` entry proving engine-timing non-determinism.
- [ ] Suspected bugs are pinned-and-logged, not fixed and not hidden.
- [ ] Every case has a can-fail proof.

---

### Phase 6: Group configs and the fast/slow contract

**Agent tier: STANDARD (high).** Mechanically fiddly (hand-authored GUIDs) but fully pre-specified by feature #2's Decision 4 and de-risked by Phase 1's probe. Needs Bash.

**Goal:** two stable named targets feature #4 can hard-code forever.

**Tasks:**

- [ ] 6.1 `Configs/Tests/OVT_TestGroup_Fast.conf` + `.conf.meta` — `OVT_TEST_LogicSuite` + `OVT_TEST_InitSuite`. Fresh 16-hex GUID (do NOT reuse the Phase 1 probe's `6A6E04103558938B`), collision-checked against this repo, the reference tree and the game install. **`[BaseContainerProps()]` on every concrete suite class is MANDATORY** (finding 1.10 — without it the group instantiates nothing: `Unknown class` error, empty `<testsuites>`, exit 2). **Execution order is alphabetical by class name**, not `m_aSuites` order — no group may depend on order.
- [ ] 6.2 `Configs/Tests/OVT_TestGroup_All.conf` + `.conf.meta` — Logic + Init + Campaign + Persistence. **Must not include** `OVT_TEST_MetaSuite` or `OVT_TEST_PersistenceRoundTripSuite`.
- [ ] 6.3 Verify by execution: `run-tests.sh "{FAST_GUID}"` → 0 with every expected case present in `junit.xml`; `run-tests.sh "{ALL_GUID}"` → 0 likewise. Record both wall times.
- [ ] 6.4 Leak check: confirm the quarantined and meta suites appear in the harness listing as **disabled** during a group run and contribute no `<testcase>` to `junit.xml`.
- [ ] 6.5 Determinism: three consecutive `{FAST_GUID}` runs and three `{ALL_GUID}` runs — identical exit codes, identical case counts, identical summaries.
- [ ] 6.6 Document both GUIDs in `tools/README.md` (target table only — **no change to `run-tests.sh` itself**) and in the skill, alongside the recommended CI usage: reset the save DB, run Fast on every push, All less often, and the `OVERTHROW_TEST_TIMEOUT` guidance if the All group's measured time approaches the 300 s default.
- [ ] 6.7 Record in `findings.md` the measured per-suite cost of group membership (the extra world transition per suite), so a future decision to merge suites has numbers behind it.

**Estimated time:** 2-3 hours.

**Acceptance criteria:**
- [ ] Both group targets exit 0 and run exactly the intended suites.
- [ ] Neither group can ever include the meta or quarantined suites (verified, not assumed).
- [ ] `tools/README.md` documents both targets well enough that feature #4 never reads a `.conf`.
- [ ] `tools/run-tests.sh` is byte-identical to its state at the start of this feature.

---

### Phase 7: Documentation (Definition of Done)

**Agent tier: STANDARD (high).**

**Goal:** stop several documents describing a coverage position this feature just changed — and do it last, after the suites are green, never before.

**Tasks:**

- [ ] 7.1 **`docs/technical-design.md` §10** — replace "The test suite is real but tiny" with what is now automated vs still manual; update gate #2 in "What we do instead" with the tiers and the two group targets; **rewrite "The three dimensions that break"** — persistence round-trip is now partly automated (same-session covered; save/reload gated behind the migration), JIP remains entirely manual, `modded class` breakage remains entirely manual. Keep "Every change ships with test steps". Also update §7 Persistence to note the acceptance gate.
- [ ] 7.2 **`docs/mission-statement.md`** — the "Automating the quality gate" closing paragraph (~line 113) currently says the loop "does not yet prove anything about Overthrow itself". Replace with exactly what it now proves and what it still does not.
- [ ] 7.3 **`tools/README.md`** — the canonical home for the automation contract, so it gains: the two group GUIDs as targets; a new **Save-state control** section documenting `.scripts/reset_save.sh`, `backup_save.sh` and `activate_save.sh` (synopsis, arguments, `OVERTHROW_SAVE_DIR` and `--profile`, the Workbench-vs-`OverthrowCI` default trap, exit codes, `.saves/` layout and naming convention, and the destructive-path guard); and the round-trip suite's acceptance procedure.
- [ ] 7.4 **`.claude/skills/workbench-workflow/SKILL.md`** — Testing Guidelines (line 28) and Critical Constraints (line 53) both assert "coverage is currently a single smoke test"; the Development and Testing Cycles say the same. Update all four. Extend "Running the Autotests" with the two group targets and the save-state preconditions, and "Writing Autotests" with: the tier table, which tier a new case belongs in, the campaign-start opt-in, the manager-resolution rule (stale statics), the can-fail requirement, and the no-flake / no-`maxAttempts` policy.
- [ ] 7.5 **`docs/features/core/persistence/context.md`** — record that behaviour-level persistence tests now exist and act as the migration's acceptance gate; name `OVT_TEST_PersistenceRoundTripSuite`, the exact command and its save-state precondition; state the acceptance criterion as "exit 1 today, exit 0 when the migration is complete". Also record this feature's findings that (a) the branch currently has no working save path in either system, and (b) the `.scripts/` save tools are written against EPF's `.db/Overthrow` layout and will need updating when the storage location moves.
- [ ] 7.6 **`CLAUDE.md`** — the "Coverage today is a single smoke test" bullets (Development Workflow and Critical Constraints) are now false. State the real position and keep the "no debugger" claim untouched.
- [ ] 7.7 **`docs/features/dev-ops/epic-overview.md`** — flip feature #3's status and task count; refresh the rollup; note in Integration & Architecture that the persistence acceptance gate now exists.
- [ ] 7.8 Sweep: `grep -ri "single smoke test\|smoke test only\|no real coverage"` across the repo; fix or deliberately flag every hit (historical records in `findings.md` files stay as-is — they are dated records, not claims).

**Estimated time:** 2-2.5 hours.

**Acceptance criteria:**
- [ ] No document still says coverage is a single smoke test.
- [ ] No document overstates it either — each says what is covered and that JIP, UI, MP and the save/reload round-trip remain manual or gated.
- [ ] The skill contains a tier table a new agent can place a case with, without reading this plan.
- [ ] `tools/README.md` documents all three save scripts, including the profile trap.
- [ ] `docs/features/core/persistence/` names the acceptance-gate command, its precondition and its expected exit codes.

---

**Total estimated effort:** 19-28 hours, weighted toward Phases 1, 4 and 5.

---

## Key Technical Decisions

### Decision 1: Suites are organised by setup cost, not by subject area

**Context:** #2's convention is `OVT_TEST_<Area>Suite`, one file per area. The natural reading gives ~8 suites (Economy, Towns, Jobs, Skills, Modifiers, Init, Persistence…).
**Decision:** four suites named for their **tier** — Logic, Init, Campaign, Persistence — with subject expressed in case names and in sibling case files inside the tier directory.
**Rationale:** the world transition and the campaign start happen **per suite**, and a group run re-requests the transition for every enabled suite (`SCR_AutotestHarness.ConfigureTestSuites`, and `Setup_OpenWorld` does not detect "already there" — findings.md). Eight subject suites would pay eight world loads per full run to express the same assertions; four tiers pay four. It also maps 1:1 onto the fast/slow split CI needs, instead of requiring a per-suite speed annotation maintained by hand. Coverage grows by adding a **case file** to an existing tier, so launch cost stays flat as coverage rises.
**Alternatives considered:** one suite per subject (loses the tier mapping, multiplies launch cost); one suite total (loses per-tier setup and the fast subset entirely).

### Decision 2: The fast/slow split ships as two `SCR_AutotestGroup` configs

**Context:** there is no run-everything CLI form; `run-tests.sh` takes exactly one target. #4 needs "quick subset on push, full set less often".
**Decision:** `OVT_TestGroup_Fast` (Logic + Init) and `OVT_TestGroup_All` (Logic + Init + Campaign + Persistence), hand-authored `.conf` + `.meta` in `Configs/Tests/`, with both GUIDs documented in `tools/README.md`. Class-name targets remain the debugging contract for a single suite or case.
**Rationale:** it is the only mechanism the engine offers for >1 suite per launch, and it needs zero tooling change — `run-tests.sh` already accepts `{GUID}` positionally. Encoding the split in config rather than in a shell flag keeps feature #2's tool untouched (the epic's ownership rule) and lets the split change without touching Bash. The GUIDs are ugly in CI config; documented constants are the accepted cost.
**Alternatives considered:** a `--fast` flag on `run-tests.sh` (new infrastructure in someone else's feature); one giant suite with all cases (kills the fast subset, and one slow Setup would be paid by every case).

### Decision 3: Campaign start lives in `OVT_TEST_SuiteBase` behind an opt-in virtual

**Context:** `DoStartNewGame()` + `DoStartGame()` are callable, but the sequence has three sharp edges — non-idempotent, difficulty-preset mismatch, and a start menu that may already be open.
**Decision:** one implementation in the suite base, opted into per suite via `RequiresStartedCampaign()`, guarded by `!HasGameStarted()`.
**Rationale:** copy-pasting a three-edged sequence into every campaign suite guarantees one copy eventually diverges — and this one is fragile in ways (`m_Difficulty` sourced from the prefab's Normal preset rather than the world layer's TestWorld preset) that a reader would not spot. Centralising also makes the *behaviour* uniform, which is what "no flaky tests" actually requires: every campaign-tier suite starts from the identical state. Placing it in the base rather than a helper class means it composes with the framework's Setup stage, so a start failure fails the suite before any case runs, with TearDown correctly skipped.
**Alternatives considered:** a static helper called from each suite's Setup (loses stage integration and the fail-fast unwind); a dedicated "campaign" suite base class between `OVT_TEST_SuiteBase` and the suites (an extra type for one boolean).

### Decision 4: Persistence assertions may only touch Overthrow's public manager API

**Context:** the requirement is that these tests survive the EPF → vanilla migration and become its acceptance gate.
**Decision:** state is written and read exclusively through public manager methods. The **only** permitted persistence-layer reference in the entire test tree is the single save trigger, `GetPersistence().SaveGame()`, annotated as the deliberate seam. Enforced by a grep in the DoD.
**Rationale:** a test that names `OVT_TownSaveData` or `EPF_PersistenceManager` breaks when the migration lands, which inverts its purpose — it would report the migration as a regression by construction. Routing through Overthrow's own API also encodes the real contract: the migration is allowed to change *how* state is stored and forbidden to change *what* survives. `SaveGame()` is preferred over the player-facing `RequestSave()` RPC because it is server-side, needs no player entity, and is what the RPC calls anyway. A greppable rule is enforceable by a reviewer with no context.
**Alternatives considered:** allowing persistence types in setup but not assertions (the boundary is unenforceable by grep and will erode); adding a purpose-built test facade to gameplay code (touches shipped code, which the epic forbids).

### Decision 5: The round-trip suite ships quarantined and red, as the migration's gate

**Context:** the branch has no working save path — `SaveGame()` is a stub, and re-parenting `OVT_PersistenceManagerComponentClass` away from EPF means EPF never reaches SETUP either. "Must pass against the current EPF implementation" is not satisfiable here as-is.
**Decision:** split persistence into a green same-session suite (in the All group) and a quarantined round-trip suite (in no group), whose transition from exit 1 to exit 0 is `vanilla-persistence`'s acceptance criterion. Record this, with its save-state precondition, in the vanilla-persistence docs.
**Rationale:** the alternatives are worse. Writing round-trip tests that pass because nothing was saved is precisely the "test that cannot fail" the requirements forbid. Deleting them leaves the migration unverifiable, which is the whole reason this coupling exists. Quarantining is honest, costs one suite class, and produces a *better* artefact than the original plan: a single command that says whether the migration is done. The green same-session suite is not a consolation prize — it independently catches "setting town control doesn't stick", which is a real regression class.
**Alternatives considered:** restoring EPF's manager component to make the tests pass (changes shipped behaviour and takes over the paused feature's work — out of scope); marking the round-trip tests with `maxAttempts` or a skip flag (dishonest, and the framework has no skip concept); validating against a `main` worktree (kept as optional task 4.10, not the critical path).

### Decision 6: Managers are resolved from the live game mode, not from `OVT_Global`'s statics

**Context:** every manager caches `static s_Instance` and nothing ever nulls it. The harness loads the world three times per launch.
**Decision:** the suite base exposes a resolution helper that finds components on the current game mode; suites use it in Setup and cache the result for their cases. `OVT_Global` remains what the Init suite *asserts about*, rather than what other suites *depend on*.
**GATE AMENDMENT (finding 1.5):** stale singletons do **not** manifest on 1.7.0.54 — the engine nulls the weak statics on destruction and `OVT_Global` agreed with `FindComponent` in 100% of samples. The helper ships as *defensive documentation* (cheap, and future-proof against an engine change), but it is not a flake fix and suites may trust `OVT_Global` where convenient. R3 downgraded accordingly.
**Original rationale (premise now disproven):** a static pointing at a component of a destroyed game mode fails in the worst possible way — not with a null dereference, but with plausible readings of dead state. That is the textbook flaky test, and it would be blamed on the framework for a long time before anyone suspected a singleton. Distinguishing "the thing under test" from "the thing we use to reach the thing under test" also keeps the Init suite meaningful: it can legitimately assert that `OVT_Global` returns live managers, because it is not itself relying on that.
**Alternatives considered:** trusting `OVT_Global` everywhere (cheapest until the first unexplained red); nulling the statics from test code (mutates shipped state and would mask a real bug in production paths).

### Decision 7: Save-state control reuses `.scripts/*`, extended rather than replaced

**Context:** some tests need a fresh campaign or a known saved state. The project already has three tools for this; they default to the Workbench profile and two of them prompt interactively.
**Decision:** add optional argument forms (`backup_save.sh <name>`, `activate_save.sh <name-or-file>`) and a `--profile` convenience, keep every interactive path working, keep the location, archive format and `.saves/` naming convention, and document all three canonically in `tools/README.md`. Save state is established **before** the launch, never from inside a test case.
**Rationale:** the requirement is explicit that these must be used rather than replaced, and it is right: a second mechanism would diverge from whatever the user actually runs, and the fixtures in `.saves/` are theirs. Argument forms are strictly additive — no existing invocation changes behaviour. Establishing state before launch rather than inside a case is not a preference either: the save DB is read during world init, long before a test step runs, so an in-case reset could not affect the boot it is trying to control. Documenting in `tools/README.md` rather than a new file puts it where every other automation contract lives and where feature #4 will look, at the cost of one cross-directory pointer.
**Alternatives considered:** a new `tools/save-state.sh` wrapper (a second mechanism to keep in sync, and the user's muscle memory is on `.scripts/`); moving the scripts into `tools/` (churn with no functional gain, and it would break every existing habit and doc reference).

### Decision 8: `reset_save.sh` gets a destructive-path guard

**Context:** `reset_save.sh` is `rm -rf "$SAVE_PATH"`, where `SAVE_PATH` defaults to the user's real Workbench campaign save. This feature is about to start calling it from tooling, and CI will call it unattended.
**Decision:** refuse to delete a path that is empty, is `/`, or does not end in a save-DB-shaped suffix; print the resolved path before deleting.
**Rationale:** the failure mode is silent, instant and unrecoverable — a mistyped `OVERTHROW_SAVE_DIR`, an unset variable in a CI job, or a `cd` in the wrong place destroys hours of the user's play. A guard costs three lines and turns the worst outcome into an error message. This is the one place in the feature where automation touches something the user cares about and cannot get back, so it is the one place worth being paranoid about.
**Alternatives considered:** requiring `OVERTHROW_SAVE_DIR` to be explicitly set (breaks the user's existing bare invocations); doing nothing (the default target is a real save — not acceptable once unattended callers exist).

### Decision 9: Every case is proven red once, and the method is recorded

**Context:** the requirement is explicit — a test that cannot be made to fail provides no value.
**Decision:** during development, each case is made to fail by perturbing the thing it covers; the perturbation and the observed failure text are recorded in a `findings.md` table. Reverting the perturbation and observing green again completes the proof.
**Rationale:** the failure modes this catches are common and invisible: an assertion after an early `return true`, an `AssertTrue` whose result is discarded, a case that silently never runs because its suite was not enabled. #2 already institutionalised the idea with `OVT_TEST_MetaSuite`; this extends it from "the harness can go red" to "each specific assertion can go red". Recording the method makes it re-runnable after a Reforger update rather than a one-off ritual.
**Alternatives considered:** mutation testing (no tooling on this platform); trusting review (this is exactly the class of defect review misses).

### Decision 10: Bugs found are pinned and logged, never fixed and never hidden

**Context:** reading the code already surfaces several likely integer-division defects in exactly the maths this feature covers, plus a save UI that reports success unconditionally. Fixing bugs is explicitly out of scope.
**Decision:** where current behaviour is clearly unintended, write a case that **pins the current behaviour**, name and comment it as pinning a suspected bug, and log it under "Bugs found (log only)" in `findings.md`. Never weaken a case to hide a finding; never add `maxAttempts` to a case that fails for a real reason.
**Rationale:** the alternatives both destroy information. Asserting the *intended* behaviour lands a red suite that blocks the feature and drags bug-fixing into scope. Omitting the case leaves the defect uncovered and un-noticed — and worse, invisible to whoever fixes it later, who then has no signal that they changed anything. A pinned case with an honest comment turns the eventual fix into a deliberate, visible test change. It also gives the maintainer a list.
**Alternatives considered:** fix as you go (scope creep into gameplay code, in an epic whose rule is to touch none); skip the area (loses the highest-value fast tests in the codebase).

---

## Quality Bar

Test-infrastructure work fails differently from feature work: it fails by being reassuring.

- **A test that cannot fail is a defect**, not a passing test. Fallibility is proven per case, not assumed.
- **Reliability and determinism over breadth.** Twelve cases that are identical across three runs beat forty that are usually green. A flaky suite trains everyone to ignore red, which is worse than having no suite.
- **`maxAttempts` is a last resort with a burden of proof.** It may only be used where `findings.md` records evidence of genuine engine-timing non-determinism. It is never a fix for a race, a settling delay or a real bug.
- **Honest verdicts.** A suite that is red for a real reason stays red and is quarantined out of the default targets with a documented explanation. Nothing is weakened to produce green.
- **Zero impact on shipped runtime behaviour.** No gameplay code is modified. Test classes remain unguarded and inert without `-autotest` (#2's Decision 2, verified in its task 2.6). The campaign-start sequence lives in test code and must never become something shipped code relies on.
- **Automation must not be able to destroy the user's data.** The one destructive tool in reach gets a guard and prints what it resolved before acting.
- **No new tooling.** `tools/run-tests.sh` must be byte-identical at the end of this feature; the `.scripts/` tools are extended additively, never rewritten. The epic's ownership boundary is not negotiable.
- **Assertions describe behaviour, not implementation.** Especially in persistence, where naming an implementation type would invert the test's purpose.

---

## Definition of Done

All criteria must pass. Written to be verifiable by an evaluator with no implementation context.

### Functional criteria

- [ ] **F1.** `OVT_TEST_LogicSuite`, `OVT_TEST_InitSuite`, `OVT_TEST_CampaignSuite`, `OVT_TEST_PersistenceSuite` exist under `Scripts/Game/Tests/TestSuites/`, each inheriting `OVT_TEST_SuiteBase`.
- [ ] **F2.** `OVT_TEST_SuiteBase` provides `RequiresStartedCampaign()` and a guarded campaign-start Setup step; Smoke and Meta suites are unaffected.
- [ ] **F3.** `OVT_TEST_PersistenceRoundTripSuite` exists, is in no group config, and its header carries a capitalised quarantine warning plus the acceptance procedure.
- [ ] **F4.** `Configs/Tests/OVT_TestGroup_Fast.conf` and `OVT_TestGroup_All.conf` exist with `.meta` files and collision-checked GUIDs, and both are documented in `tools/README.md`.
- [ ] **F5.** `.scripts/backup_save.sh <name>` and `.scripts/activate_save.sh <name-or-file>` work non-interactively; both still prompt when called with no argument; `reset_save.sh` refuses an implausible path.
- [ ] **F6.** `tools/compile-check.sh` exits 0 with everything present.
- [ ] **F7.** `docs/features/dev-ops/test-coverage/findings.md` exists and records: the Reforger build; the per-experiment table from Phase 1; the `OverthrowCI` save-DB path; the selected persistence ladder rung with evidence; the save-script verification matrix; the can-fail table (case → perturbation → observed failure text); "Bugs found (log only)"; "Differs from assumptions".
- [ ] **F8.** `tools/run-tests.sh` is unchanged from its state at the start of this feature.

### Quality criteria

- [ ] **Q1. Every case can fail.** `findings.md` has a can-fail entry for every case in every green suite.
- [ ] **Q2. Determinism.** Three consecutive runs of `{FAST_GUID}` and three of `{ALL_GUID}` produce identical exit codes, case counts and summaries.
- [ ] **Q3. No unjustified retries.** `grep -rn "maxAttempts" Scripts/Game/Tests/` returns nothing, or only entries with a matching `findings.md` justification.
- [ ] **Q4. Behaviour-level persistence.** `grep -rn "EPF_\|SCR_Persistence\|SaveData" Scripts/Game/Tests/` returns at most the single annotated save-trigger line.
- [ ] **Q5. Tier A purity.** No file under `TestSuites/Logic/` references `OVT_Global`, `GetGame().GetGameMode()` or any manager type.
- [ ] **Q6. No shipped-code change.** `git diff --stat` for this feature touches only `Scripts/Game/Tests/`, `Configs/Tests/`, `.scripts/`, `docs/`, `.claude/skills/`, `CLAUDE.md` and `tools/README.md`. No file under `Scripts/Game/` outside `Tests/` is modified.
- [ ] **Q7. Inertness preserved.** No `#ifdef WORKBENCH` anywhere in `Scripts/Game/Tests/`; a client run without `-autotest` still produces no test runner and no new `console.log` errors.
- [ ] **Q8. Quarantine holds.** A `{ALL_GUID}` run's harness listing shows `OVT_TEST_MetaSuite` and `OVT_TEST_PersistenceRoundTripSuite` disabled, and `junit.xml` contains none of their cases.
- [ ] **Q9. No magic counts.** No assertion depends on a town/base count larger than the test world provides.
- [ ] **Q10. Save tooling is non-destructive by accident.** `reset_save.sh` with an empty, `/` or non-save-shaped `OVERTHROW_SAVE_DIR` exits non-zero without deleting anything, and prints the path it resolved.

### Integration criteria

- [ ] **I1.** Feature #4 can run the fast subset and the full set using only `tools/README.md` — two targets, four exit codes, four artifact paths — plus the documented save-reset precondition.
- [ ] **I2.** The `workbench-workflow` skill lets a new agent place and write a case in the right tier without reading this plan.
- [ ] **I3.** `docs/features/core/persistence/` names the acceptance-gate suite, its save-state precondition, the exact command, and the expected exit code before and after the migration.
- [ ] **I4.** The per-suite cost of group membership is recorded, so a future merge decision has numbers.
- [ ] **I5.** All three `.scripts/` save tools are documented in one canonical place, including the Workbench-vs-`OverthrowCI` profile trap.

### Documentation criteria

- [ ] **D1. `docs/technical-design.md` §10** — coverage position updated; "The three dimensions that break" rewritten to say which dimensions are now automated. §7 notes the acceptance gate.
- [ ] **D2. `docs/mission-statement.md`** — "Automating the quality gate" reflects real coverage and names what is still manual.
- [ ] **D3. `tools/README.md`** — group targets, a Save-state control section for all three scripts, and the round-trip acceptance procedure.
- [ ] **D4. `workbench-workflow` skill** — Testing Guidelines, Critical Constraints, both cycles, "Running the Autotests" (group targets + save preconditions) and "Writing Autotests" (tier table, campaign opt-in, manager resolution, can-fail, no-flake) all updated.
- [ ] **D5. `docs/features/core/persistence/context.md`** — acceptance gate recorded, plus the findings that neither persistence system currently works on the branch and that the save scripts assume EPF's `.db` layout.
- [ ] **D6. `CLAUDE.md`** — the single-smoke-test claims replaced; the "no debugger" claim untouched.
- [ ] **D7. `docs/features/dev-ops/epic-overview.md`** — feature #3 status and rollup updated.
- [ ] **D8.** No document overstates coverage: each names JIP, UI, MP and save/reload as still uncovered.

### Verification Method

An independent evaluator, on the dev machine with Steam running, from `/mnt/n/Projects/Arma 4/Overthrow.Arma4`. Substitute the recorded GUIDs and save path from `tools/README.md` / `findings.md`. **Never run the save scripts without `OVERTHROW_SAVE_DIR` pointed at the `OverthrowCI` profile.**

1. `tools/compile-check.sh; echo $?` → **0**. → F6
2. `tools/run-tests.sh OVT_TEST_LogicSuite; echo $?` → **0**. → F1
3. `tools/run-tests.sh OVT_TEST_InitSuite; echo $?` → **0**. → F1
4. `tools/run-tests.sh OVT_TEST_CampaignSuite; echo $?` → **0**. → F1, F2
5. `tools/run-tests.sh OVT_TEST_PersistenceSuite; echo $?` → **0**. → F1
6. `tools/run-tests.sh OVT_TEST_PersistenceRoundTripSuite; echo $?` → **1**, and `.tmp/run-tests/junit.xml` contains a `<failure>` naming the missing capability. *(Expected to become 0 once `vanilla-persistence` lands — that is the gate.)* → F3, I3
7. `tools/run-tests.sh "{FAST_GUID}"; echo $?` → **0**; `grep -c "<testcase" .tmp/run-tests/junit.xml` equals the Logic + Init case count. → F4
8. `tools/run-tests.sh "{ALL_GUID}"; echo $?` → **0**; case count equals Logic + Init + Campaign + Persistence; `grep -c "PersistenceRoundTrip\|MetaSuite" .tmp/run-tests/junit.xml` → **0**. → F4, Q8
9. Repeat step 7 three times → identical exit code, case count and stderr summary each time. → Q2
10. `tools/run-tests.sh OVT_TEST_MetaSuite; echo $?` → **1** (feature #2's standing red-path proof still holds). → F2
11. `tools/run-tests.sh OVT_TEST_NoSuchThing; echo $?` → **2**. → inherited contract intact
12. `OVERTHROW_SAVE_DIR=/tmp/ovt-save-probe/.db/Overthrow .scripts/backup_save.sh probe; echo $?` after creating that directory → **0**, and a `probe_<timestamp>.tar.gz` appears in `.saves/`. → F5
13. `OVERTHROW_SAVE_DIR=/tmp/ovt-save-probe/.db/Overthrow .scripts/activate_save.sh probe; echo $?` → **0**, no prompt. `… activate_save.sh no-such-save; echo $?` → **non-zero** with the available list. → F5
14. `OVERTHROW_SAVE_DIR=/tmp/definitely-not-a-save .scripts/reset_save.sh; echo $?` → **non-zero**, nothing deleted, resolved path printed. `OVERTHROW_SAVE_DIR= .scripts/reset_save.sh; echo $?` → **non-zero**. → Q10
15. `.scripts/backup_save.sh` with no arguments → still prompts. → F5
16. `grep -rn "EPF_\|SCR_Persistence\|SaveData" Scripts/Game/Tests/` → at most one annotated line. → Q4
17. `grep -rln "OVT_Global\|GetGameMode" Scripts/Game/Tests/TestSuites/Logic/` → **no output**. → Q5
18. `grep -rn "ifdef WORKBENCH\|maxAttempts" Scripts/Game/Tests/` → no `ifdef`; any `maxAttempts` has a `findings.md` justification. → Q3, Q7
19. `git diff --stat <base>..HEAD -- Scripts/Game/ ':!Scripts/Game/Tests/'` → **no output**. → Q6
20. `git diff --stat <base>..HEAD -- tools/run-tests.sh` → **no output**. → F8
21. Open `findings.md` → per-experiment table, save-DB path, ladder rung with evidence, save-script matrix, can-fail table covering every case, "Bugs found (log only)", "Differs from assumptions". → F7, Q1
22. Open `tools/README.md` → both group GUIDs; a Save-state control section covering all three scripts, `OVERTHROW_SAVE_DIR`, `--profile`, the Workbench-default trap, `.saves/` naming, exit codes; the round-trip acceptance procedure. → F4, I1, I5, D3
23. Open the `workbench-workflow` skill → tier table, campaign opt-in, manager-resolution rule, can-fail and no-flake policies, save preconditions. → I2, D4
24. `grep -ri "single smoke test"` across the repo → only dated historical records in `findings.md` files. → D1-D8

---

## Testing Strategy

The tests are the deliverable, so "testing the tests" is the real work.

### Validating each case

| ID | Check | Method | Expected |
|---|---|---|---|
| V1 | The case can fail | Perturb the covered value or invert the expectation; run the suite | exit 1, failure text names the case |
| V2 | The case actually ran | After V1's revert, confirm the case appears in `junit.xml` | self-closing `<testcase>` present |
| V3 | The assertion is reached | Confirm no `return true` precedes the assertion in a bool step | reviewed per case |
| V4 | The case is deterministic | 3 consecutive suite runs | identical verdict and timing band |
| V5 | The case is tier-correct | Logic-tier cases reference no manager or world | grep-verified (Q5) |

### Validating the suites

| ID | Scenario | Expected |
|---|---|---|
| S1 | Each suite run alone by class name | exit 0 (except the quarantined suite: exit 1 with a diagnostic message) |
| S2 | Fast group | exit 0, exactly the Logic + Init cases |
| S3 | All group | exit 0, exactly Logic + Init + Campaign + Persistence |
| S4 | Meta suite | exit 1 — #2's standing red-path proof, unchanged |
| S5 | Bogus target | exit 2 — inherited contract intact |
| S6 | Three consecutive Fast-group runs | identical exit code, case count, summary |
| S7 | Campaign suite Setup failure (simulated by breaking the start call) | suite fails in Setup, TearDown skipped, message names the start failure |
| S8 | Fast group with and without a preceding `reset_save.sh` | identical verdict today; the recipe is proven working for when it matters |

### Validating the save tooling

| ID | Scenario | Expected |
|---|---|---|
| W1 | `reset_save.sh` on a missing directory | exit 0, "nothing to delete" |
| W2 | `reset_save.sh` on a present throwaway directory | exit 0, directory gone, resolved path printed |
| W3 | `reset_save.sh` with an empty / `/` / non-save-shaped path | non-zero, nothing deleted |
| W4 | `backup_save.sh <name>` / no argument | archive created without prompting / prompts as before |
| W5 | `activate_save.sh <name>` / `<file>` / unmatched / no argument | selects by name / by file / non-zero with list / menu as before |
| W6 | Every case above | run against a throwaway directory, never the user's real save |

### Policies

- **No flakes.** Any case that is not identical across three runs is fixed or removed before the feature closes. `maxAttempts` is not a fix; it requires recorded evidence of engine-timing non-determinism and a `findings.md` entry.
- **Compile gate.** `tools/compile-check.sh` exits 0 before any launch, always.
- **Red-on-arrival handling.** A case that fails because Overthrow has a bug is pinned to current behaviour and logged (Decision 10), or quarantined out of the groups with an explanation. It is never weakened silently and the bug is never fixed here.
- **Not tested here:** JIP/MP, UI, performance, AI movement (navmesh does not load in the test world), and anything requiring a second client process.

---

## Dependencies

### Internal

- **`dev-ops/autotest-foundation` (complete)** — `OVT_TEST_SuiteBase`, the modded `SCR_AutotestHelper`, the `OVT_TEST_` naming, `tools/run-tests.sh` and its 0/1/2/124 taxonomy. Consumed; `run-tests.sh` is not modified.
- **`docs/features/dev-ops/autotest-foundation/findings.md`** — the empirical ground truth this plan is built on. Supersedes assumptions.
- **`dev-ops/workbench-automation` (complete)** — `tools/lib/common.sh` (`ovt_profile_dir`, `ovt_mygames_dir`) is used to resolve the `OverthrowCI` save path; `launch-game.sh` is used only via `run-tests.sh`.
- **`.scripts/reset_save.sh`, `backup_save.sh`, `activate_save.sh`** — existing save-state tooling, extended additively in Phase 2.
- **`Worlds/MP/OVT_Campaign_Test.ent`** + `OVT_Campaign_Test_Layers/default.layer` — supplies the game mode, faction manager, one town controller, one base controller and the TestWorld difficulty preset.
- **Overthrow gameplay code as it exists on `vanilla-persistence`** — specifically `OVT_OverthrowGameMode.DoStartNewGame`/`DoStartGame`/`HasGameStarted`/`IsInitialized`/`GetPersistence`, `OVT_Global`'s manager getters, and the public mutators/accessors of the town, economy, player, real-estate, skill and recruit managers. Renaming any of these breaks the suites, which is the intended coupling.

### External

- **Arma Reforger 1.7.0.54 / engine 190965.** Findings are build-specific; re-verify after any update.
- **Windows host with a GPU, Steam running.** Inherited constraint; no headless rendering exists.
- **Packed workshop EPF/EDF** — still loaded transitively via `addon.gproj`, still required to compile, even though EPF is dormant at runtime on this branch.

### Dependents

- **#4 ci-pipeline** consumes the two group GUIDs, the unchanged exit taxonomy, and the documented save-reset precondition. Renaming or re-GUIDing a group later is a breaking change to #4.
- **`vanilla-persistence`** gains a machine-checkable acceptance criterion. Renaming `OVT_TEST_PersistenceRoundTripSuite` breaks the documented gate. It also inherits the note that the `.scripts/` tools assume EPF's `.db/Overthrow` layout and must be updated when the storage location moves.

---

## Risks & Mitigation

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| **R1** | **No working save path on this branch** — `SaveGame()` stubbed, EPF never reaches SETUP — so "passes against EPF first" cannot be satisfied literally. | **Certain** (evidence in ground truth) | High for the requirement, Medium for the feature | Decision 5's split: green same-session suite plus a quarantined round-trip suite that becomes the migration's gate. Phase 1 task 1.7 confirms empirically before anything is written. Optional task 4.10 offers the literal satisfaction via a `main` worktree, off the critical path and requiring user approval. |
| **R2** | **Campaign start proves impossible or unstable** in the test world (missing player, half-initialised managers, errors). | Low (source strongly suggests it works) | High — Tiers C and D collapse | Phase 1 task 1.3 proves it before any suite is written. If it fails, the gate shrinks the feature to Tiers A and B and documents the blocker; Tier A and B still deliver real coverage. |
| **R3** | ~~Stale manager singletons~~ **RESOLVED at gate (finding 1.5): does not manifest** — engine nulls weak statics; helper is defensive only. | ~~Medium~~ Nil | ~~High~~ Nil | Decision 6's resolution helper; Phase 1 task 1.5 measures whether it actually manifests; V4's three-run determinism check catches what analysis misses. |
| **R4** | **In-session reload destroys the harness**, making L1 impossible and forcing the clumsier L2. | Medium (only bites if a save path exists) | Medium | Task 1.9 tests it directly before any round-trip case is written. L2 is pre-specified with its anti-vacuous-pass guard so it can be adopted without redesign. Under the expected L3 the question is moot. |
| **R5** | ~~Group config does not load~~ **RESOLVED at gate (finding 1.10): works first try** — but `[BaseContainerProps()]` on each concrete suite class is mandatory, and execution order is alphabetical. | ~~Low-Med~~ Nil | ~~Medium~~ Nil | Phase 1 task 1.10 proves the mechanism with a throwaway probe **before** Phase 6 depends on it; `[BaseContainerProps()]` on the suite class is the pre-specified next step; the last resort is one Workbench GUI round-trip with the user (#2's Decision 4 step 6). Fallback if all fails: CI runs the fast suites as separate sequential `run-tests.sh` calls — more launches, same verdicts. |
| **R6** | **Suite runtime grows** until the All group approaches `run-tests.sh`'s 300 s default and CI feedback degrades. | Medium over time | Medium | Tier organisation caps world transitions at one per suite (Decision 1); coverage grows by adding case files, not suites. Task 6.7 records the measured per-suite cost so a merge decision has numbers. `OVERTHROW_TEST_TIMEOUT` documented for #4. |
| **R7** | **Tests are red on arrival** because the covered maths is genuinely buggy — several integer-division defects are already visible by inspection (`SupportPercentage`, `GetTaxIncome`, `GetSellPrice`'s stock term, `GetLevelProgress`), plus a save UI that reports success unconditionally. | High | Medium | Decision 10: pin current behaviour, name the case honestly, log it. Fixing is explicitly out of scope; hiding is explicitly forbidden. The log becomes backlog input. |
| **R8** | ~~`GetWorldFile()` cannot be overridden~~ **RESOLVED at gate (finding 1.2): compiles and runs world-free**, harness runs once, 8 s. | ~~Medium~~ Nil | ~~Low~~ Nil | Task 1.2 settles it in one 5 s compile check. Cost of the fallback is ~7 s per fast run — annoying, not blocking. |
| **R9** | **Assertions drift into implementation detail** in persistence, silently coupling the tests to EPF. | Medium | High — destroys the migration gate | Decision 4's grep-enforceable rule, checked in DoD Q4 and in the Verification Method. The rule is stated verbatim in the suite file headers so the next author meets it before writing a line. |
| **R10** | **Test code accidentally alters shipped behaviour** — the campaign-start sequence is non-idempotent and the difficulty preset is mutated. | Low | High (players) | The start step is guarded by `!HasGameStarted()`, lives only in the test tree, and mutates only in-session state in a process that exits seconds later. DoD Q6 proves no file outside `Scripts/Game/Tests/` changed; Q7 re-confirms inertness without `-autotest`. |
| **R11** | **An automated `reset_save.sh` call destroys the user's real campaign save** — the default target is the Workbench profile, and an unset or mistyped `OVERTHROW_SAVE_DIR` hits it silently. | Medium if unguarded | **Critical and unrecoverable** | Decision 8's path guard plus printing the resolved path; every documented invocation in this feature sets `OVERTHROW_SAVE_DIR` explicitly; Phase 2's verification runs only against a throwaway directory; DoD Q10 tests the guard directly. |
| **R12** | **The `OverthrowCI` save path is wrong or moves** — `$profile:` resolution differs from the assumption, or vanilla persistence stores saves somewhere else entirely (`SaveGameManager`, not `.db/Overthrow`). | Medium | Medium | Task 1.8 determines it empirically rather than by inference, and records what a vanilla save would use instead. The scripts read the path from a variable, so a move is a one-line change. The mismatch is recorded in the vanilla-persistence docs as migration work. |
| **R13** | **A Reforger update moves the framework or changes junit/step semantics.** | Low per update | Medium | `findings.md` is build-stamped like #1's and #2's. Framework changes fail at compile time or as exit 2, never as a silent pass. Re-run Phase 1's experiment set after any update. |
| **R14** | **Scope creep into comprehensive coverage** — "while we're here, let's cover vehicles and inventories too." | Medium | Medium | The spine is named in Phase 4 task 4.2 and the deferred list, with reasons, in 4.3. Growth is a later, cheap activity: add a case file to an existing tier. |

---

## Notes

- **The harness runs twice and loads the world three times per launch.** Doubled `CLI autotest`/`Requesting scenario change:` lines are normal (findings.md), not a symptom.
- **`autotest.log` contains UTF-8 status glyphs**; `junit.xml`'s `<testsuite>` has **no** `failures=` attribute on this build. Both are `run-tests.sh`'s problem, already solved — do not re-derive them.
- **Use the suite/case-shadowed `Print`/`PrintFormat`**, not the globals: only the shadowed versions reach `autotest.log`. `PrintFormat` on the case base takes string params only — call `.ToString()` on numbers and bools.
- **`OVT_Global.GetServer()`/`GetUI()` dereference the local controlled entity without a null check** off the server path. Harmless here (tests run as server) but a trap for anyone writing a client-side case later.
- **`OVT_StaminaSkillEffect` is intentionally empty** (BI does not expose stamina params). Assert nothing about it; note it.
- **The pre-existing `Failed to get SCR_PersistenceSystem instance!` error** fires once per test-world load and is inherited noise from the paused migration. It is not a test failure and must not be treated as one.
- **`Difficulty_TestWorld.conf`** gives deterministic starting values (cash 100000, resources 200) — useful anchors for economy assertions, but read them from the config rather than hardcoding, so a config change does not silently invalidate a test.
- **`.saves/` naming convention** is already established (`<testworld|everon>_<situation>_<MP|SP>`); fixtures added later should follow it, and the archives themselves are the user's, not this feature's, to curate.

---

## Related Documentation

- `docs/features/dev-ops/epic-overview.md` — epic scope, build order, ownership boundaries
- `docs/features/dev-ops/test-coverage/requirements.md` — this feature's requirements
- `docs/features/dev-ops/autotest-foundation/implementation.md` — Decisions 1-8, the inherited contract
- `docs/features/dev-ops/autotest-foundation/findings.md` — **empirical ground truth; supersedes assumptions**
- `docs/features/dev-ops/workbench-automation/findings.md` — CLI ground truth (exit codes, log dirs, addon proof)
- `tools/README.md` — `run-tests.sh` contract, exit codes, artifact paths, target forms; gains the save-state section
- `.claude/skills/workbench-workflow/SKILL.md` — "Running the Autotests" / "Writing Autotests"
- `docs/features/core/persistence/` — the migration this feature gates
- `/mnt/n/Projects/Arma 4/ArmaReforger/scripts/Autotest/Game/TestFramework/` — framework API surface
- `/mnt/n/Projects/Arma 4/ArmaReforger/scripts/GameLib/tests/TestingFramework.c` — `Test`/`TestStep` attributes, stages, timeouts
