# Autotest Foundation — Implementation Plan

**Epic:** dev-ops (feature #2 of 5)
**Status:** Ready for Review
**Started:** 2026-08-01
**Target Completion:** 2026-08-02 (complete)
**Last Updated:** 2026-08-02 00:45

---

## Executive Summary

This feature turns "Overthrow has no automated tests" from a fact into a false statement, by wiring Arma Reforger 1.7.0's **shipped** `SCR_Autotest` framework into the project and proving the whole loop end-to-end: *write a test class → launch the client with `-autotest` → read a `junit.xml` verdict*.

It is deliberately thin. Four small EnforceScript files, one bash wrapper, and a written-down contract. **No coverage is delivered here** — one trivially-passing smoke test and one deliberately-failing test, and nothing else. Assertions about towns, economy, persistence and managers are feature #3's entire job, and every hour spent writing them before the loop is proven is an hour spent on a foundation nobody has stood on yet.

The framework is inherited for free: the base game's `ArmaReforger.gproj` (line 927) puts `scripts/Autotest/Game` inside the `game` script module, which Overthrow's `Scripts/Game/` already extends. **No `addon.gproj` change is required.** What is required is the project-specific glue BI left as empty static stubs: which world to load, which systems config, and — the part unique to a mod — a scenario transition that keeps Overthrow's addon loaded across the world change.

The output is a **stable command contract**: `tools/run-tests.sh` returns an honest exit code derived from `junit.xml`, and feature #4 orchestrates it without ever learning what a `SCR_AutotestSuiteBase` is.

### Quality bar (inherited from feature #1)

- **No false greens.** The game client's exit code is *always 0*, even on fatal errors. A verdict must be **positively proven** from `junit.xml`: present, parseable, at least one test case executed, zero failures. "No failures found" in a file that does not exist is not a pass.
- **Three-valued.** Pass / Fail / *Could not determine*. Missing artifact, zero test cases, unparseable XML, launch failure — all exit 2, never folded into pass or fail.
- **Test code is inert for players.** It ships (it must — see Decision 2) but does nothing without `-autotest`.
- **Docs describe only what exists.** The documentation phase runs *last*, after the loop is proven, never before.

---

## Goals

### Primary Goals

1. **A proven loop.** `tools/run-tests.sh` → game client boots → Overthrow test world loads → an Overthrow test class runs → `junit.xml` is retrieved → an honest exit code comes back.
2. **The Overthrow `SCR_AutotestHelper` override** — the project's defaults for world, systems config, launch params, and a scenario transition carrying Overthrow's addon GUID `59B657D731E2A11D`.
3. **An Overthrow suite base class** (`OVT_TEST_SuiteBase`) so world selection and shared setup live in exactly one place, inherited by every suite feature #3 writes.
4. **Proof the harness can go red.** A green run that cannot go red is worthless; a deliberately failing test is exercised and its `junit.xml` failure shape recorded verbatim.
5. **Both class-name `-autotest` forms confirmed** — suite class and single case class — since only the `{GUID}` group form has ever been observed working on this machine.
6. **Documented authoring patterns** — stages, bool-returning multi-tick steps, timeouts, `maxAttempts`, assertions, file layout, `OVT_TEST_` naming — so feature #3 and every future agent copy a settled pattern instead of inventing one.

### Secondary Goals

1. **A stable artifact contract** for feature #4: fixed local paths for `junit.xml`, `autotest.log`, `autotest_failed.log`, `console.log`.
2. **Gaps in the shipped framework written down** rather than worked around (the requirement is explicit: document, do not replace).
3. **Docs corrected** — CLAUDE.md, `docs/technical-design.md` §2 + §10, `docs/mission-statement.md`, and the `workbench-workflow` skill all currently assert there is no test framework.

### Explicitly Out of Scope

- **Real coverage.** Feature #3. No assertion about any Overthrow system is written here.
- **CI orchestration**, GitHub Actions, runners — feature #4.
- **Multiplayer / JIP test automation** — needs two coordinated processes. (`SCR_AutotestHelper.WORLD_MPTEST` = `{96A8AF57260A7392}worlds/MP/MpTest/MpTest.ent` is noted as the future starting point.)
- **Performance / FPS / screenshot autotests** (`AutotestGrid`, `Screenshot_Autotest`, `-autotest-output-dir`).
- **A bespoke harness.** The shipped framework is used as-is.
- **Reimplementing feature #1's process boundary.** `tools/run-tests.sh` calls `tools/launch-game.sh`; it never names an `.exe`.

---

## Architecture Overview

### Verified ground truth (read from the shipped framework, 2026-08-01)

Established by reading `/mnt/n/Projects/Arma 4/ArmaReforger/scripts/Autotest/Game/TestFramework/*` and `scripts/GameLib/tests/TestingFramework.c`. These are API facts, not assumptions — but they are *read*, not *executed*; Phase 2 executes them.

| Fact | Source | Consequence for this plan |
|---|---|---|
| `-autotest` accepts three forms: `{GUID}` of an `SCR_AutotestGroup` config, a class inheriting `SCR_AutotestSuiteBase`, a class inheriting `SCR_AutotestCaseBase`. Anything else → `Debug.Error("Invalid -autotest parameter value")` + `RequestClose()`. | `SCR_AutotestRunner.c:33-87` | Class names are a first-class contract, not a hack. Phase 2 confirms both class forms in the retail client. |
| JUnit path is the hardcoded constant `$logs:/junit.xml`; failed list is `$logs:/autotest_failed.log`. | `SCR_AutotestReport.c:4-5` | Collected at `$LOG_DIR/junit.xml` using feature #1's log-dir resolution. `-autotest-output-dir` is irrelevant. |
| Test log is `$logs:autotest.log` (note: no slash — same directory). | `SCR_AutotestPrinter.c:8` | Second artifact to collect. |
| The default test group is **empty** (`return new SCR_AutotestGroup()`), and `ConfigureTestSuites` **disables every suite** not named. | `SCR_AutotestHarness.c:184-208, 274-281` | **There is no "run everything" CLI form.** Running >1 suite in one launch requires an `SCR_AutotestGroup` config. Feature #3 will need one; #2 does not (one suite). This must be written down. |
| `addonList` for `GameStateTransitions.RequestScenarioChangeTransition` is a **comma-separated string of bare GUIDs** (no braces). | `SCR_GameOverScreenUIComponent.c:347-363` | Overthrow's override passes `58D0FB3206B6F859,59B657D731E2A11D` (+ EPF `5D6EBC81EB1842EF`, EDF `5D6EA74A94173EDF` if needed). |
| `GameProject.GetLoadedAddons()` + `GameProject.IsVanillaAddon()` exist. | same | Documented fallback if a hardcoded list proves insufficient. |
| `[Test(suite:, timeoutS:, timeoutMs:, maxAttempts:)]` and `[TestStep(stage, timeoutS, timeoutMs)]` (alias `[Step(EStage.X)]`). Stages run Setup → Main → TearDown, methods in definition order. `void` steps run once; `bool` steps re-run every tick until true. | `TestingFramework.c:79-131` | This is the authoring pattern to be documented. |
| Setup failure skips TearDown; Main failure runs TearDown; timeouts reset per step method. | `TestingFramework.c:54-67` | Documented in the skill. |
| `SCR_AutotestSuiteBase` already implements `Setup_OpenWorld` + `Setup_AwaitWorld` and routes to `SCR_AutotestHelper.GetDefaultWorld()`. | `SCR_AutotestSuiteBase.c:51-71` | `OVT_TEST_SuiteBase` does **not** need to override `GetWorldFile()` — modding the helper is sufficient. Keeps the sealing risk (R1) off the critical path. |
| **`Setup_AwaitWorld` has no timeout.** It returns `!IsTransitionRequestedOrInProgress()` every tick, forever. | `SCR_AutotestSuiteBase.c:67-71` | **Framework gap.** A world that never loads = a run that never ends. Bounded only by the wrapper's process timeout. Document; mitigate in `run-tests.sh`. |
| The harness self-closes the game after the run (`s_bCloseGameAfterRun = true` → `GetGame().RequestClose()`). | `SCR_AutotestHarness.c:33`, `SCR_AutotestRunner.c:120-124` | Confirmed empirically by #1 (7s round trip). No kill needed on the happy path. |
| `SCR_AutotestRunnerCore.CanCreate()` → `SCR_TestRunner.ShouldCreate()` → false unless `-autotest` is present. | `SCR_AutotestRunnerCore.c:11-14` | The inertness guarantee for shipped builds. |
| `TestHarness.GetNSuites()` is marked `[Obsolete("Switch to instantiating tests and suites manually")]` — i.e. `[Test]`-annotated classes are **auto-registered by the engine at startup**. | `TestHarness.c:16-19` | Test classes are instantiated in every client, `-autotest` or not. Cost is trivial but must be verified as behaviour-neutral (Phase 2). |
| `Worlds/MP/OVT_Campaign_Test.ent` = `{D87EF7EED4210569}`; its entire content is `SubScene { Parent "{A701424D70022078}worlds/MP/MpTest/MpTest_Basic.ent" }`. | repo | The default test world is a **bare MpTest world with no Overthrow game mode**. Ideal here (fast, no economy sim). **A real constraint for feature #3** — behaviour tests needing managers will need a world/mission that starts `OVT_OverthrowGameMode`. Phase 2 records this by *logging* (never asserting) whether the game mode is present. |

### Component layout

```
Scripts/Game/Tests/                                   <- new tree, mirrors BI's scripts/Game/Tests/
├── TestFramework/
│   ├── OVT_AutotestFramework.c        modded class SCR_AutotestHelper  (project defaults)
│   └── OVT_TEST_SuiteBase.c           class OVT_TEST_SuiteBase : SCR_AutotestSuiteBase
└── TestSuites/
    ├── Smoke/OVT_TEST_SmokeSuite.c    suite + OVT_TEST_Smoke_HarnessRuns   (always green)
    └── Meta/OVT_TEST_MetaSuite.c      suite + OVT_TEST_Meta_AlwaysFails    (always red, never in a default run)

tools/
└── run-tests.sh                       NEW - launch + collect + honest verdict (calls launch-game.sh)

docs/features/dev-ops/autotest-foundation/
├── implementation.md                  this file
└── findings.md                        NEW - empirical record from Phase 2
```

### Data flow

```
tools/run-tests.sh [target]
  |
  |-- rm stale .tmp/run-tests/*                     (a previous run's junit can never satisfy this one)
  |-- tools/launch-game.sh -- -autotest <target>    (feature #1 owns this boundary)
  |     |
  |     +--> client boots -> Overthrow addon loaded -> scripts compiled
  |            |
  |            +-- SCR_TestRunner.ShouldCreate() sees -autotest -> harness begins
  |                  |
  |                  +-- OVT_TEST_SuiteBase (Setup): SCR_AutotestHelper.WorldOpenFile(...)
  |                  |     -> modded RequestScenarioChangeTransition(world, systemsCfg,
  |                  |          "58D0FB3206B6F859,59B657D731E2A11D")
  |                  |     -> Setup_AwaitWorld ticks until the transition completes
  |                  +-- test cases run (Setup -> Main -> TearDown, per tick)
  |                  +-- report.WriteJUnitXML()   -> $logs:/junit.xml
  |                  +-- report.WriteFailedList() -> $logs:/autotest_failed.log
  |                  +-- GetGame().RequestClose()
  |
  |-- read LOG_DIR= from launch-game.sh stdout
  |-- copy junit.xml / autotest.log / autotest_failed.log / console.log -> .tmp/run-tests/
  |-- parse junit.xml -> verdict
  +-- exit 0 (all passed, >=1 case ran) | 1 (failures) | 2 (indeterminate) | 124 (timeout)
```

### Ownership boundary (epic rule)

| Layer | Owner | This feature |
|---|---|---|
| Windows process boundary, timeouts, PID kill, log-dir resolution | #1 `workbench-automation` | **consumed, never reimplemented** |
| In-game test contract: helper override, suite base, naming, `-autotest` value | **#2 (this)** | built here |
| Assertions about Overthrow behaviour | #3 `test-coverage` | out of scope |
| Job orchestration, PR annotations, artifact upload | #4 `ci-pipeline` | consumes `tools/run-tests.sh`'s exit codes and `.tmp/run-tests/*` |

---

## Implementation Phases

### Phase 1: Framework integration + the two test classes

**Agent tier: STANDARD (high)** — EnforceScript authoring against a well-understood API; `tools/compile-check.sh` catches signature mistakes in ~5s. No Bash requirement beyond running the compile check.

**Goal:** Every file that has to exist, exists and compiles. Nothing is launched yet.

**Tasks:**

- [ ] 1.1 Create `Scripts/Game/Tests/TestFramework/OVT_AutotestFramework.c` — `modded class SCR_AutotestHelper` mirroring BI's `SCR_AutotestFramework.c` **signature for signature** (note: BI drops `protected` on `RequestScenarioChangeTransition`; match BI, not the base declaration):
  - `static const ResourceName WORLD_OVT_TEST = "{D87EF7EED4210569}Worlds/MP/OVT_Campaign_Test.ent";`
  - `static const string ADDONS_OVT = "58D0FB3206B6F859,59B657D731E2A11D";` (base game + Overthrow; comma-separated bare GUIDs — see ground truth)
  - `override static ResourceName GetDefaultWorld()` → `WORLD_OVT_TEST`
  - `override static ResourceName GetDefaultSystemsConfig()` → `GetGame().GetSystemsConfig()` (BI's approach: keep what is loaded). Overthrow's own `{86E953538A28A98D}Configs/Systems/ChimeraSystemsConfig.conf` is the documented alternative if Phase 2 shows the world needs it.
  - `override static string GetDefaultLaunchParams()` → `"-profile OverthrowCI\n-logLevel debug\n-noFocus\n-noThrow\n-window\n"` (match feature #1's profile so Workbench-launched runs land where the tooling looks; drop BI's `-forceUpdate`)
  - `override static bool RequestScenarioChangeTransition(...)` → `GameStateTransitions.RequestScenarioChangeTransition(mapResource.GetPath(), worldSystemsConfigResourceName, addonList: ADDONS_OVT)`
  - Doxygen `//!` header explaining that this file is the project's entire integration point with the shipped framework.
- [ ] 1.2 Create `Scripts/Game/Tests/TestFramework/OVT_TEST_SuiteBase.c` — `class OVT_TEST_SuiteBase : SCR_AutotestSuiteBase`. Deliberately near-empty: **do not** override `GetWorldFile()` / `GetWorldSystemsConfigFile()` (the inherited implementations already route to the modded helper — this also keeps the cross-module sealing hazard, R1, off the critical path). Its job is to be the single inheritance point and the place feature #3 adds shared setup. Document that in the file header.
- [ ] 1.3 Create `Scripts/Game/Tests/TestSuites/Smoke/OVT_TEST_SmokeSuite.c`:
  - `class OVT_TEST_SmokeSuite : OVT_TEST_SuiteBase` (empty body)
  - `[Test(suite: OVT_TEST_SmokeSuite, timeoutS: 30)] class OVT_TEST_Smoke_HarnessRuns : SCR_AutotestCaseBase` with a `[Step(EStage.Setup)]` void, a `[Step(EStage.Main)]` bool that ticks a counter for a few frames then `SetResultSuccess()` and returns true, and a `[Step(EStage.TearDown)]` void. It exercises every element of the pattern being documented while asserting nothing about Overthrow.
  - **Log-only diagnostics, never assertions:** print whether `GetGame().GetGameMode()` is an `OVT_OverthrowGameMode` and whether `OVT_Global` returns a manager. This answers feature #3's biggest open question from the run's `autotest.log` at zero risk of a red default run. A comment must state that these are logs on purpose.
- [ ] 1.4 Create `Scripts/Game/Tests/TestSuites/Meta/OVT_TEST_MetaSuite.c` — `OVT_TEST_MetaSuite` + `[Test(suite: OVT_TEST_MetaSuite)] class OVT_TEST_Meta_AlwaysFails` whose Main step calls `SetResultFailure("Deliberate failure: proves the harness reports red")` and returns true. **File header must state, in capitals, that this suite is never part of a default or CI run and exists solely to verify the red path.**
- [ ] 1.5 Confirm no `#ifdef WORKBENCH` anywhere in the new tree (BI's example is guarded; ours must not be — that is what makes the retail client able to run them), and no ternary operators.
- [ ] 1.6 Run `tools/compile-check.sh` until exit 0. If the `override static` chain on an already-modded class fails to compile, stop and apply R1's fallback before proceeding — do not improvise.

**Estimated Time:** 1.5-2.5 hours

**Acceptance Criteria:**
- [ ] `tools/compile-check.sh` exits 0 with the four new files present.
- [ ] `grep -r "ifdef WORKBENCH" Scripts/Game/Tests/` returns nothing.
- [ ] The modded helper's four overrides match BI's signatures exactly.
- [ ] `OVT_TEST_SuiteBase` overrides no world-selection method (world comes from the helper).
- [ ] The always-failing suite is in its own file, in its own suite, with the capitalised warning header.

---

### Phase 2: Prove the loop (empirical) — **REQUIRES ADVANCED (opus) AGENT + BASH**

**Agent tier: ADVANCED (opus).** **Requires a Bash-capable agent** — this phase is CLI experimentation and log forensics, so the project's `*-advanced` EnforceScript agents (no Bash) cannot run it. Use a full-toolset agent.

**Goal:** Replace every remaining assumption with an observed fact, recorded in `findings.md`. This is the crux of the feature; Phase 3's verdict logic is written against what this phase observes. Nothing in Phase 3 or 4 starts until the gate below is applied.

> Every run here is a ~10-40s game-client launch. Work one variable at a time and write down what happened, including the boring parts.

**Tasks:**

- [ ] 2.1 Create `docs/features/dev-ops/autotest-foundation/findings.md` with the same table shape #1 used: *command → observed outcome → wall time → artifacts produced → notes*. Record the Reforger build at the top; findings are only valid for a named build.
- [ ] 2.2 **Suite-class form.** `eval "$(tools/launch-game.sh -- -autotest OVT_TEST_SmokeSuite | grep '^LOG_DIR=')"` then inspect `$LOG_DIR`. Record: does `console.log` contain `CLI autotest suite: OVT_TEST_SmokeSuite`? Does the world transition happen (the framework logs `Requesting scenario change:` with `forceFileWrite`)? Does `junit.xml` exist, and **what does it contain, verbatim**? Wall time?
- [ ] 2.3 **Case-class form.** Same with `-autotest OVT_TEST_Smoke_HarnessRuns`. Expect `CLI autotest case:` and a `junit.xml` containing exactly one test case. Record both.
- [ ] 2.4 **Red path.** `-autotest OVT_TEST_MetaSuite`. Record the **verbatim** `junit.xml` failure element and the contents of `autotest_failed.log`. Feature #4 needs this exact shape to write PR annotations; Phase 3's parser needs it to classify failures. Confirm failures are not silently swallowed.
- [ ] 2.5 **Scoping / no-leak check.** Re-run BI's shipped empty group `-autotest "{6AB9C8EEE9A651B5}"`. Expect the same empty `<testsuites>` as #1 observed, i.e. Overthrow's new suites do **not** leak into unrelated runs — and confirm the "no run-everything form" ground-truth claim.
- [ ] 2.6 **Inertness check (shipping safety).** `tools/launch-game.sh` with **no** `-autotest`. Confirm: no `Creating: SCR_TestRunner` line, no world transition, no `junit.xml`, and no new errors in `console.log` versus a pre-Phase-1 baseline run. This is the evidence that test code does not affect players.
- [ ] 2.7 **Invalid-target behaviour.** `-autotest OVT_TEST_DoesNotExist`. Record the failure mode (expected: `Invalid -autotest parameter value` + `crash.log` + clean self-exit, per #1's finding 1.14c) — Phase 3 must classify this as indeterminate (exit 2), never as pass.
- [ ] 2.8 **Timing + timeout basis.** Record wall time for tasks 2.2-2.4 (boot + world load + run). Propose `run-tests.sh`'s default timeout from the worst observed time with generous headroom, and note the unbounded `Setup_AwaitWorld` gap as the reason a timeout is mandatory rather than advisory.
- [ ] 2.9 **Conditional — only if 2.2 or 2.3 fails.** Activate Decision 4's fallback: hand-author `Configs/Tests/OVT_TestGroup.conf` + `.conf.meta` with a generated GUID and prove the `{GUID}` form works with an Overthrow-authored config. Full procedure and verification in Decision 4.
- [ ] 2.10 **Write up.** A "Differs from assumptions" section in `findings.md` naming anything that contradicts this plan's ground-truth table, plus a **"Framework gaps"** section (the requirement is to document gaps, not replace the framework). Seed it with: no run-everything CLI form; `Setup_AwaitWorld` has no timeout; the default test world has no Overthrow game mode; anything else observed.

**Estimated Time:** 2-4 hours (dominated by launch wall-time and careful recording)

**Acceptance Criteria:**
- [ ] `findings.md` exists; every task 2.2-2.8 has an observed result, including the ones that did nothing.
- [ ] Both class-name forms are settled as working or not working, with the log lines quoted.
- [ ] A passing `junit.xml` and a failing `junit.xml` are both recorded **verbatim**.
- [ ] The no-`-autotest` run is confirmed behaviour-neutral.
- [ ] A default timeout is proposed from measured times.
- [ ] "Differs from assumptions" and "Framework gaps" sections exist (even if one says "nothing").

> **Gate:** before Phase 3 begins, re-read Phases 3-4 against `findings.md` and amend them in place (as #1 did). If neither class-name form works, Decision 4's fallback becomes the primary contract *here*, and Phase 3's default target changes from a class name to a `{GUID}` — decided at the gate, not improvised inside Phase 3.

---

### Phase 3: `tools/run-tests.sh` + the command contract

**Agent tier: STANDARD (high).** **Requires a Bash-capable agent.**

> **Evidence-first, like `compile-check.sh`.** The client's exit code is meaningless. Exit 0 requires *positive proof*: `junit.xml` exists in **this run's** log dir, parses, contains at least one executed test case, and reports zero failures and zero errors. Absence of evidence is exit 2. A stale artifact must be impossible — delete before launch, never reuse.

**Goal:** One command that feature #4 can call forever without knowing anything about EnforceScript.

**Tasks:**

- [ ] 3.1 `tools/run-tests.sh [--timeout <s>] [--keep-artifacts] [--verbose] [-h|--help] [<target>]`, where `<target>` is any accepted `-autotest` value (suite class, case class, or `{GUID}`) and defaults to `OVT_TEST_SmokeSuite`. One target argument, no `--suite`/`--case`/`--group` triplet — the engine already accepts all three through one parameter.
- [ ] 3.2 Source `tools/lib/common.sh` for path/dir helpers only. Launch **exclusively** via `tools/launch-game.sh -- -autotest <target>`, consuming its documented `KEY=value` stdout (`LOG_DIR`). Never name an `.exe`, never re-derive the log directory.
- [ ] 3.3 Delete `.tmp/run-tests/*` before launch; after the run copy `junit.xml`, `autotest.log`, `autotest_failed.log`, `console.log` (and `crash.log` if present) from `$LOG_DIR` into `.tmp/run-tests/` under those exact names. Missing `junit.xml` is a hard indeterminate, never a pass.
- [ ] 3.4 Verdict logic — **all** must hold for exit 0: launcher completed (not 124/2); `junit.xml` present and parseable; at least one `<testcase>` present; zero `<failure>`/`<error>` elements. Failures present → exit 1. Anything else → exit 2 with a one-line reason and the artifact path. **⚠️ GATE (Phase 2, verified):** `<testsuite>` has NO `failures=`/`errors=` attributes on this build — the verdict MUST count `<failure>`/`<error>` *elements*, never read attributes. An invalid target produces launcher rc 0 + client exit 0 + NO junit.xml (process-indistinguishable from success) — the missing-artifact check is the only thing that catches it. Include the `Loaded addons:` block-scoped check on `console.log` if `lib/common.sh` exposes it (secondary guard; #1 proved a whole-log grep produces false passes).
- [ ] 3.5 Human-readable summary on **stderr** (`run-tests: OK (N tests, Ds)` / `run-tests: FAILED (N of M) in Ds` / `run-tests: INDETERMINATE: <reason> (artifacts: <path>)`); failing test names, one per line, on **stdout**. Same stdout/stderr split as `compile-check.sh` so #4 sees a uniform interface.
- [ ] 3.6 Pass `--timeout` through to `launch-game.sh`; propagate 124 unchanged. **Default 300 s** (GATE: Phase 2 measured green runs at 14-22 s, red 15 s, degenerate 7-8 s; 300 s is >13x worst observed — an unbounded `Setup_AwaitWorld` means the timeout is the only thing standing between a broken world transition and a hung CI job).
- [ ] 3.7 Add a `tools/run-tests.sh` section to `tools/README.md` in the existing style: synopsis, flags, exit-code table, stdout/stderr contract, artifact table, the `-autotest` target forms, the "no run-everything form" note, and the framework gaps from `findings.md`.
- [ ] 3.8 Verify by execution: default target → exit 0; `OVT_TEST_MetaSuite` → exit 1; a bogus target → exit 2; `--timeout 5` → exit 124 with no orphaned client process (`tasklist.exe`).

**Estimated Time:** 2-3 hours

**Acceptance Criteria:**
- [ ] `tools/run-tests.sh` exits 0 on the smoke suite, 1 on the meta suite, 2 on a bogus target, 124 on a forced timeout.
- [ ] Deleting `$LOG_DIR/junit.xml` mid-flight (or pointing at a run that produced none) yields exit 2, never 0.
- [ ] `.tmp/run-tests/` holds the four artifacts with stable names after every run.
- [ ] `tools/README.md` documents the command completely enough that feature #4 never reads the script source.
- [ ] The script contains no `.exe` path and no log-directory globbing of its own.

---

### Phase 4: Documentation (Definition of Done)

**Agent tier: STANDARD (high).**

**Goal:** Stop four documents asserting a constraint this feature just removed — and do it *after* the loop is proven, never before. Per the epic's policy this is mandatory, not deferred.

> **⚠️ GATE (Phase 2, verified — corrects Decision 7/R6):** the default test world **DOES load `OVT_OverthrowGameMode`** — the `.ent` is a bare SubScene, but `Worlds/MP/OVT_Campaign_Test_Layers/default.layer` adds the game mode, faction manager and town/base controllers. Smoke diagnostics: game mode present = true, `OVT_Global.GetTowns()` non-null = true. Caveats: the campaign is not *started* (start menu shows), a pre-existing `Failed to get SCR_PersistenceSystem instance!` error fires per test-world load, and navmesh fails to load. Docs written in this phase must state the corrected fact (managers reachable; campaign-start state is feature #3's problem), not the plan's original assumption.

**Tasks:**

- [ ] 4.1 **`CLAUDE.md`** — the "No unit tests - Manual play-testing only" constraint and the Development Workflow bullet ("There is still no test suite — the dev-ops epic (feature #2 …) is building one"). Replace with the real capability: `tools/run-tests.sh` runs Overthrow's autotests; state plainly that coverage is currently one smoke test (feature #3 adds real coverage) so nobody infers more than exists. Keep "No debugger" — untouched by this feature.
- [ ] 4.2 **`docs/technical-design.md` §2 "What We Don't Have"** — strike through the "**No unit or integration tests.**" bullet (line ~83) following the section's existing convention for entries invalidated by this epic, naming `dev-ops/autotest-foundation` and the date. Note in the same entry that coverage is minimal pending #3.
- [ ] 4.3 **`docs/technical-design.md` §10 "Testing Strategy"** — replace "### There is no test suite / No unit tests, no integration tests, no CI. This is a property of the platform, not a backlog item." That sentence is now false in its first clause and its justification. Add automated tests as a numbered gate in "What we do instead", between the compile check and hosted play-testing. Keep "Every change ships with test steps" — manual play-testing remains the runtime gate for everything the suites do not cover, which is currently everything.
- [ ] 4.4 **`docs/technical-design.md` §3 project structure** — add `Scripts/Game/Tests/` to the tree and a one-line directory-responsibility entry, including the note that `Scripts/Game/Tests/TestFramework/OVT_AutotestFramework.c` holds a `modded class` that by convention would live under `Modded/` (see Decision 1).
- [ ] 4.5 **`docs/mission-statement.md`** — update the "Automating the quality gate" closing paragraph (~line 111), which currently says "Until the autotest features land, the runtime discipline is unchanged". The first autotest stage has landed; say exactly what it does and does not yet prove.
- [ ] 4.6 **`.claude/skills/workbench-workflow/SKILL.md`** — the largest change. Update "Testing Guidelines", "Critical Constraints" ("No unit tests"), "Key Differences" ("No test framework - Manual testing only (dev-ops epic feature #2 is building one)"), and the Development/Testing Cycles. Add a **test-authoring section** covering: file layout under `Scripts/Game/Tests/TestSuites/<Area>/`, the `OVT_TEST_` naming convention, inheriting `OVT_TEST_SuiteBase`, `[Test]`/`[Step]` attributes, stages and their unwind rules, `void` vs `bool` steps, `timeoutS`/`timeoutMs`/`maxAttempts`, `AssertTrue`/`SetResultSuccess`/`SetResultFailure`, and how to run one suite or one case. Keep it short and link `tools/README.md` + `findings.md` rather than restating them. Do not create the four dangling resource files this skill already references.
- [ ] 4.7 **`docs/features/dev-ops/epic-overview.md`** — flip feature #2's status and refresh the rollup.
- [ ] 4.8 Cross-check: `grep -ri "no unit test\|no automated test\|no test framework\|no test suite"` across the repo; fix or explicitly flag whatever turns up.

**Estimated Time:** 1-1.5 hours

**Acceptance Criteria:**
- [ ] No file in the repo still claims Overthrow has no automated tests or no test framework.
- [ ] No file overstates the position either — every updated doc says coverage is a single smoke test today.
- [ ] The `workbench-workflow` skill contains a test-authoring section a new agent can write a suite from without reading the base game.
- [ ] Claims about the **debugger** are untouched.

---

**Total estimated effort:** 7-11 hours, weighted toward Phase 2.

---

## Key Technical Decisions

### Decision 1: Test file layout mirrors BI's, under `Scripts/Game/Tests/`

**Context:** The requirement asks for a decided, documented layout. BI uses `scripts/Game/Tests/{TestFramework,TestSuites/<Area>}`. Overthrow's convention puts `modded class` overrides in `Scripts/Game/Modded/`.
**Decision:** `Scripts/Game/Tests/TestFramework/` (glue + suite base) and `Scripts/Game/Tests/TestSuites/<Area>/` (suites + their cases, one file per area, as BI does). The `modded class SCR_AutotestHelper` lives in `Tests/TestFramework/OVT_AutotestFramework.c`, **not** in `Modded/`. Naming: `OVT_TEST_<Area>Suite` for suites, `OVT_TEST_<Area>_<Subject>_<ExpectedBehaviour>` for cases (BI's `SCR_TEST_Example1Subject_GetFive_ReturnsFive` shape).
**Rationale:** Mirroring BI makes every pattern directly greppable against the reference tree — the single highest-value property for a codebase whose only documentation is the base game's source. `OVT_TEST_` makes test classes trivially distinguishable from gameplay classes in grep, in the `-autotest` argument, and in a future decision to exclude them from a packed release. Keeping the helper override next to the suite base means the entire test integration is one deletable directory; splitting it across `Modded/` would put two files that only make sense together in different trees. The deviation is deliberate and gets documented in §3 of the technical design (task 4.4).
**Alternatives considered:** `Scripts/Game/Tests/` flat (breaks down the moment #3 adds areas); helper in `Modded/` (cohesion loss for a file that is not fragile in the way `Modded/` exists to flag — it overrides empty stubs that BI designed to be overridden).

### Decision 2: Tests ship in the addon, unguarded

**Context:** BI's example suite is wrapped in `#ifdef WORKBENCH` and therefore does not exist in the retail client — #1 proved this the hard way (`-autotest SCR_TEST_Example1TestSuite` → `Invalid -autotest parameter value`). The requirement forbids repeating that mistake.
**Decision:** No preprocessor guard on any Overthrow test class. They compile into, and ship with, the addon. Inertness is guaranteed *at runtime*, not at compile time: `SCR_AutotestRunnerCore.CanCreate()` returns false without `-autotest`, so the runner is never created, no world transition is requested, and no artifact is written.
**Rationale:** CI runs the retail client, not the Workbench. A `WORKBENCH`-guarded test is a test CI cannot run — which is the entire point of the feature. The cost is a handful of small classes registered by `TestHarness` at startup in every player's client; task 2.6 measures and confirms that this is behaviour-neutral rather than assuming it.
**Alternatives considered:** A dedicated `AUTOTEST` define added to `addon.gproj` (would need to be defined in the shipped build to be useful, i.e. exactly the same outcome with an extra moving part); stripping tests at pack time in feature #5 (real option later, but it would break CI running against a packed build — revisit only if the cost ever becomes measurable).

### Decision 3: Class names are the primary `-autotest` contract; no group config yet

**Context:** Three forms exist. Only `{GUID}` has been empirically proven, because BI's class-name examples do not exist in retail. A group config normally needs the Workbench GUI to mint a GUID.
**Decision:** `-autotest <SuiteClassName>` is the contract, with `<CaseClassName>` for single-test debugging. No `SCR_AutotestGroup` config is created in this feature. `run-tests.sh`'s default target is `OVT_TEST_SmokeSuite`.
**Rationale:** A class name needs no asset, no GUID, no GUI, and no resource-database registration; it is git-diffable, greppable, and renamed by the same edit that renames the class. The group form's only advantage — running several suites in one launch — is worth nothing while exactly one suite exists (YAGNI). The ground-truth finding that **there is no run-everything CLI form** is written down so feature #3 knows a group config becomes necessary the moment it adds a second suite, and can create it then with real requirements.
**Alternatives considered:** Creating the group config now (speculative, and pays the GUID problem for no current benefit); one launch per suite forever (fine at N=1, becomes #3's problem to weigh at N=10 with measured boot cost in hand).

### Decision 4: Fallback if class-name forms do not work — hand-authored `.conf` + `.meta`

**Context:** The one genuinely open question. If Phase 2 shows the retail client rejects class names, the `{GUID}` group form is the only path — and creating a `.conf` normally means using the Workbench GUI to assign a GUID.
**Decision:** Do not touch the GUI first. Hand-author both files, because a `.conf`'s GUID lives in its sibling `.meta`, in plain text, in a format the repo already contains dozens of examples of:

1. `Configs/Tests/OVT_TestGroup.conf` — root `SCR_AutotestGroup` (it is `[BaseContainerProps(configRoot: true)]`), with `m_aSuites` listing the suite class(es). Copy the container syntax from an existing Overthrow config such as `Configs/Deployment/overthrowDeployments.conf`, including the per-entry instance GUIDs it uses.
2. `Configs/Tests/OVT_TestGroup.conf.meta` — copy the shape of `Configs/Systems/ChimeraSystemsConfig.conf.meta`, setting `Name "{<NEW_GUID>}Configs/Tests/OVT_TestGroup.conf"` and keeping the same per-platform `CONFResourceClass` blocks.
3. **GUID generation:** 16 uppercase hex characters. Enfusion's are timestamp-derived, but uniqueness is the only actual requirement. Generate one, then prove it is unused: `grep -r "{<GUID>}"` across this repo, the reference tree at `/mnt/n/Projects/Arma 4/ArmaReforger`, and the game install's `addons/` — regenerate on any hit.
4. **Prove it loads** (do not assume): `tools/launch-game.sh -- -autotest "{<GUID>}"` must log `CLI autotest config: SCR_AutotestGroup` (`SCR_AutotestRunner.c:56`) and **not** `Invalid resource path for autotest config`. Then confirm `junit.xml` contains the suite's cases.
5. If the suite classes cannot be instantiated from the container, add `[BaseContainerProps()]` to them (the base class carries `[BaseContainerProps(category: "Autotest")]`; BI's example has it commented out, so inheritance may already suffice — determine empirically, do not guess).
6. **Only if all of that fails:** ask the user to create the config once through the Workbench Resource Browser and commit it. This is the single step in the feature that would need the GUI, and it is the last resort, not the first move.

**Rationale:** Every Enfusion resource GUID in this repo is plain text in a `.meta` file; there is no registry the GUI writes to that a text editor cannot. The plausible failure mode is not GUID *validity* but *resource-database registration* — hence step 4 verifies by execution rather than by inspection. Writing the whole procedure down now means Phase 2 does not have to design under pressure after a failed run.
**Alternatives considered:** GUI-first (needs a human round-trip for something that is very likely a text edit); reusing BI's `{6AB9C8EEE9A651B5}` (it is a base-game asset — cannot be edited to reference Overthrow suites).

### Decision 5: `tools/run-tests.sh` exists, and it belongs to this feature

**Context:** The alternative is documenting a two-line invocation (`eval "$(tools/launch-game.sh -- -autotest X | grep ^LOG_DIR=)"` then read `$LOG_DIR/junit.xml`) and letting feature #4 do the rest.
**Decision:** Ship `tools/run-tests.sh`: launch → collect artifacts → parse `junit.xml` → honest exit code. Thin, no process handling of its own.
**Rationale:** Something must turn `junit.xml` into an exit code, because **the client's exit code is always 0** — a verified fact from #1. If that logic is not written here it gets written in #4, which the epic's integration rule forbids ("#2 owns the in-game test contract; #4 orchestrates the commands #2 stabilises"). It is also the exact place the false-green risk lives: a missing or empty `junit.xml` must never read as success, and that judgement is test-contract knowledge, not CI knowledge. Uniformity matters too — #4 gets two commands with the same exit taxonomy (0/1/2/124), same stdout/stderr split, same artifact discipline, instead of one script and one paragraph of prose. Cost is roughly 150 lines over documenting the invocation.
**Alternatives considered:** Documented invocation only (pushes the honest-verdict problem downstream and splits ownership); a full test-selection/reporting CLI (speculative — one optional target argument covers every form the engine accepts).

### Decision 6: The always-failing test is kept, not deleted

**Context:** The requirement asks for a failing test "used once" to confirm failures surface.
**Decision:** Keep `OVT_TEST_Meta_AlwaysFails` permanently, in its own `OVT_TEST_MetaSuite`, never in a default or CI run. `tools/run-tests.sh OVT_TEST_MetaSuite` must exit 1 — a standing DoD criterion.
**Rationale:** The red path needs re-verification exactly when it is least convenient: after a Reforger update, when #4 wires up PR annotations, when a parser change lands. Deleting it means the next person to need that proof re-invents it under time pressure. Kept and isolated it costs ~15 lines of shipped dead code and gives every downstream feature a one-command way to prove its failure handling works. The isolation is what makes it safe: it is in no group, it is not the default target, and its file says so in capitals.
**Alternatives considered:** Delete after Phase 2 (cheapest, loses the permanent capability); keep in the smoke suite behind a flag (would make the default run red — unacceptable).

### Decision 7: `OVT_Campaign_Test.ent` as the default world, with its limitation recorded

**Context:** The requirement names this world because it loads far faster than Eden. Reading it revealed it is a three-line `SubScene` of BI's `MpTest_Basic.ent` — a bare world with no Overthrow game mode.
**Decision:** Use it, and record the limitation prominently in `findings.md` and the skill: **the default autotest world does not start `OVT_OverthrowGameMode`.**
**Rationale:** For this feature it is ideal — nothing to initialise, nothing to desync, fastest possible loop, and the smoke test asserts nothing that needs a manager. For feature #3 it is a design constraint discovered now instead of on day three: behaviour tests needing managers will need either a suite that overrides `GetWorldFile()` to point at a mission/world that starts the game mode, or a new dedicated autotest world. Task 1.3's log-only diagnostics capture the evidence for that decision without creating an assertion that can go red.
**Alternatives considered:** Building an Overthrow autotest world now (out of scope, and #3 should specify it against real needs); using the Eden campaign (minutes of load time per run — fatal for a feedback loop).

### Decision 8: Addon list is an explicit constant, not derived

**Context:** The scenario transition must keep Overthrow loaded across the world change. `GameProject.GetLoadedAddons()` + `IsVanillaAddon()` would derive the list at runtime.
**Decision:** A named constant `"58D0FB3206B6F859,59B657D731E2A11D"` (base game + Overthrow), with EPF `5D6EBC81EB1842EF` and EDF `5D6EA74A94173EDF` added only if Phase 2 shows the transition drops them. The derived form is documented in `findings.md` as the fallback.
**Rationale:** Explicit is greppable and deterministic: when a world transition mysteriously loses the mod, a constant is one line to read and one line to change, whereas a derived list depends on how the client happened to be launched. It also matches the requirement's wording and BI's own pattern. Overthrow declares EPF as a dependency in `addon.gproj`, so transitive resolution is the expected behaviour — but expected, not verified, which is why Phase 2 checks rather than assumes.
**Alternatives considered:** Derived list (more robust in theory, less debuggable, and hides a broken dependency declaration behind runtime luck).

---

## Definition of Done

All criteria must pass. Written to be verifiable by an evaluator with no implementation context.

### Functional Criteria

- [ ] **F1.** `Scripts/Game/Tests/TestFramework/OVT_AutotestFramework.c` contains a `modded class SCR_AutotestHelper` overriding `GetDefaultWorld()`, `GetDefaultSystemsConfig()`, `GetDefaultLaunchParams()` and `RequestScenarioChangeTransition()`, with `59B657D731E2A11D` in the addon list.
- [ ] **F2.** `OVT_TEST_SuiteBase` exists and every Overthrow suite inherits from it.
- [ ] **F3.** `tools/compile-check.sh` exits 0 with all new files present.
- [ ] **F4.** `tools/run-tests.sh` exits **0**, and `.tmp/run-tests/junit.xml` shows at least one executed test case with zero failures.
- [ ] **F5.** `tools/run-tests.sh OVT_TEST_MetaSuite` exits **1** and names the failing test on stdout; `.tmp/run-tests/junit.xml` contains a `<failure>` element and `autotest_failed.log` names the test.
- [ ] **F6.** `-autotest` is confirmed working with **both** a suite class name and a single case class name, each evidenced by a quoted `console.log` line in `findings.md`. (If either form is proven impossible, Decision 4's fallback is implemented and proven instead, and the gap is documented.)
- [ ] **F7.** `junit.xml` is retrieved reliably from `$LOG_DIR/junit.xml` on every run, without any dependence on `-autotest-output-dir`.
- [ ] **F8.** No file under `Scripts/Game/Tests/` contains `#ifdef WORKBENCH`.
- [ ] **F9.** `findings.md` exists and records, per command, the observed outcome, wall time and artifacts — with "Differs from assumptions" and "Framework gaps" sections.

### Quality Criteria

- [ ] **Q1. No false greens.** With `junit.xml` absent, empty, or containing zero test cases, `run-tests.sh` exits **2** with an indeterminate message — never 0.
- [ ] **Q2. Stale artifacts cannot pass.** After a green run, a subsequent run whose client fails to produce `junit.xml` exits 2; it does not re-report the previous run's result.
- [ ] **Q3. Tool failure is distinguishable from test failure.** A bogus `-autotest` target exits **2**, not 1 and not 0.
- [ ] **Q4. Timeouts work.** `tools/run-tests.sh --timeout 5` exits **124**, and `tasklist.exe` afterwards shows no surviving `ArmaReforgerSteamDiag.exe` from that run.
- [ ] **Q5. Shipping-neutral.** A client run **without** `-autotest` produces no test runner, no world transition, no `junit.xml`, and no new `console.log` errors versus baseline (evidence in `findings.md`).
- [ ] **Q6. Determinism.** Three consecutive `tools/run-tests.sh` runs on the same tree give the same exit code and the same summary.
- [ ] **Q7. No reimplementation of feature #1.** `tools/run-tests.sh` contains no `.exe` path, no `taskkill`, and no log-directory globbing — it calls `tools/launch-game.sh` and consumes its `KEY=value` stdout.
- [ ] **Q8. The always-failing test cannot pollute a default run.** It is in its own suite, referenced by no group config, and is not `run-tests.sh`'s default target.

### Integration Criteria

- [ ] **I1.** `tools/README.md` documents `run-tests.sh`: synopsis, flags, exit codes, stdout/stderr contract, artifact paths, accepted target forms.
- [ ] **I2.** Feature #4 can orchestrate the tests using only `tools/README.md` — one command, four exit codes, four artifact paths — without reading any script or EnforceScript source.
- [ ] **I3.** Feature #3 can add a suite using only the `workbench-workflow` skill: where the file goes, what to name it, what to inherit, which attributes to use.
- [ ] **I4.** The "no run-everything CLI form" constraint is documented, so #3 knows a group config is required at its second suite.
- [ ] **I5.** Framework gaps are documented rather than worked around: unbounded `Setup_AwaitWorld`, no game mode in the default test world, plus anything Phase 2 finds.

### Documentation Criteria

- [ ] **D1. `CLAUDE.md`** — "No unit tests" is gone; `tools/run-tests.sh` is named; the single-smoke-test reality is stated.
- [ ] **D2. `docs/technical-design.md` §2** — the "No unit or integration tests" entry is struck through in the section's existing convention.
- [ ] **D3. `docs/technical-design.md` §10** — "There is no test suite" is replaced; automated tests appear as a gate; manual play-testing remains for everything uncovered.
- [ ] **D4. `docs/technical-design.md` §3** — `Scripts/Game/Tests/` appears in the structure with a responsibility line.
- [ ] **D5. `docs/mission-statement.md`** — "Automating the quality gate" reflects that the autotest stage has landed and what it does not yet prove.
- [ ] **D6. `workbench-workflow` skill** — constraints updated and a test-authoring section added (layout, naming, attributes, stages, timeouts, assertions, how to run one suite or one case).
- [ ] **D7.** No document overstates the capability: every one of them says coverage is currently a single smoke test.
- [ ] **D8.** Claims about the **debugger** are untouched.

### Verification Method

An independent evaluator, on the dev machine with Steam running, from `/mnt/n/Projects/Arma 4/Overthrow.Arma4`:

1. `tools/compile-check.sh; echo $?` → **expect 0**. → F3
2. `grep -rn "ifdef WORKBENCH" Scripts/Game/Tests/` → **expect no output**. → F8
3. `tools/run-tests.sh; echo $?` → **expect 0**, a `run-tests: OK (...)` line on stderr, empty stdout. → F4
4. `cat .tmp/run-tests/junit.xml` → **expect** at least one `<testcase>`, no `<failure>`. `ls .tmp/run-tests/` → **expect** `junit.xml`, `autotest.log`, `autotest_failed.log`, `console.log`. → F4, F7
5. `tools/run-tests.sh OVT_TEST_MetaSuite; echo $?` → **expect 1**, the failing test named on stdout, a `<failure>` in the collected `junit.xml`. → F5
6. `tools/run-tests.sh; echo $?` again → **expect 0** (the red run left nothing sticky). → Q2, Q6
7. `tools/run-tests.sh OVT_TEST_NoSuchThing; echo $?` → **expect 2** with a clear reason. → Q3
8. `tools/run-tests.sh --timeout 5; echo $?` → **expect 124**; then `tasklist.exe | grep -i armareforger` → **expect nothing**. → Q4
9. `tools/launch-game.sh` with no `-autotest`, then inspect its `LOG_DIR` → **expect** no `junit.xml`, no `Creating: SCR_TestRunner`. → Q5
10. `grep -c "exe\|taskkill" tools/run-tests.sh` → **expect** no `.exe` path and no `taskkill`. → Q7
11. Open `findings.md` → **expect** a per-command table, verbatim passing and failing `junit.xml`, quoted `CLI autotest suite:` / `CLI autotest case:` lines, "Differs from assumptions", "Framework gaps". → F6, F9, I5
12. Open `tools/README.md` → **expect** a complete `run-tests.sh` section. → I1, I2
13. Open the `workbench-workflow` skill → **expect** a test-authoring section sufficient to write a new suite. → I3, D6
14. `grep -ri "no unit test\|no automated test\|no test framework\|no test suite"` across the repo → **expect** no stale claim; **expect** debugger claims still present. → D1-D8

---

## Testing Strategy

There is no bash test framework here and building one is out of scope. Validation is a written-down checklist, run by hand, exactly as feature #1 did.

### The loop itself

| ID | Scenario | Expected |
|---|---|---|
| T1 | `run-tests.sh` (default) | exit 0, `junit.xml` with >=1 passing case |
| T2 | `run-tests.sh OVT_TEST_Smoke_HarnessRuns` (case form) | exit 0, `junit.xml` with exactly one case |
| T3 | `run-tests.sh OVT_TEST_MetaSuite` | exit 1, `<failure>` present, name on stdout |
| T4 | `run-tests.sh OVT_TEST_NoSuchThing` | exit 2, indeterminate reason |
| T5 | `run-tests.sh --timeout 5` | exit 124, no orphan process |
| T6 | `junit.xml` deleted before parsing | exit 2 |
| T7 | `junit.xml` present but zero `<testcase>` | exit 2 (proven with `-autotest "{6AB9C8EEE9A651B5}"`, BI's empty group) |
| T8 | Three consecutive default runs | identical exit code and summary |

### Shipping safety

| ID | Scenario | Expected |
|---|---|---|
| T9 | Client launched with no `-autotest` | no runner, no world transition, no artifacts, no new errors |
| T10 | `console.log` diff vs a pre-Phase-1 baseline run | no new error/warning lines attributable to test classes |

### Framework behaviour (recorded, not asserted)

| ID | Scenario | Recorded in `findings.md` |
|---|---|---|
| T11 | World transition | the `Requesting scenario change:` line and whether Overthrow stays loaded |
| T12 | Game mode presence in the test world | log-only output from the smoke test's diagnostics |
| T13 | Wall time per run | boot + world load + execution, for #3/#4 planning |

**Not tested here:** anything about Overthrow's behaviour. That is feature #3. If a proposed test in this feature would fail because Overthrow has a bug, it does not belong in this feature.

---

## Dependencies

### Internal

- **`dev-ops/workbench-automation` (complete)** — `tools/launch-game.sh` (client launch, `LOG_DIR` resolution, timeout + verified kill), `tools/lib/common.sh`, `tools/compile-check.sh`, and the contract in `tools/README.md`. Consumed, never reimplemented.
- **`docs/features/dev-ops/workbench-automation/findings.md`** — the CLI ground truth this plan is built on (client exit codes meaningless, cwd must be the game dir, `-autotest "{GUID}"` proven, `junit.xml` at `$logs:/junit.xml`).
- **`Worlds/MP/OVT_Campaign_Test.ent`** `{D87EF7EED4210569}` — exists.
- **`addon.gproj`** — unchanged. Overthrow's GUID `59B657D731E2A11D`; deps ArmaReforger `58D0FB3206B6F859`, EPF `5D6EBC81EB1842EF` (EPF → EDF `5D6EA74A94173EDF`).

### External

- **Arma Reforger 1.7.0+** (verified build 1.7.0.54 / engine 190965). The framework does not exist in older versions; `findings.md` is only valid for a named build.
- **Reference tree** `/mnt/n/Projects/Arma 4/ArmaReforger` — `scripts/Autotest/Game/TestFramework/` (10 files), `scripts/Game/Tests/TestFramework/SCR_AutotestFramework.c` (the override pattern), `scripts/GameLib/tests/TestingFramework.c` (attribute API), `scripts/GameLib/generated/ScriptTestingFramework/` (TestBase/TestSuite/TestHarness bindings).
- **`ArmaReforger.gproj:927`** puts `scripts/Autotest/Game` in the `game` module — verified in the installed game data. If a future update moves it, this feature breaks at compile time (loudly, which is correct).
- **Windows host with a GPU, Steam running.** Inherited from #1 and unfixable — Reforger has no headless rendering.
- **Packed workshop EPF/EDF** under `My Games/ArmaReforgerWorkbench/addons` (the source repos do not compile on 1.7.0.54).

### Dependents (do not break these later)

- **#3 test-coverage** inherits `OVT_TEST_SuiteBase`, the naming convention, the layout, and the documented authoring patterns. Renaming any of them later is a breaking change to #3.
- **#4 ci-pipeline** consumes `tools/run-tests.sh`'s exit taxonomy (0/1/2/124) and `.tmp/run-tests/*` paths. Same rule.

---

## Risks & Mitigation

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| **R1** | **`override static` on an already-modded class fails to compile.** Overthrow mods `SCR_AutotestHelper`, which BI already mods; `SCR_AutotestSuiteBase.c:90-97` carries a `MODULE_AUTOTEST` "hack" class specifically to stop the compiler sealing these methods across script modules. | Low-Med | High — blocks Phase 1 | Overthrow's scripts join the **same** `game` module as `scripts/Autotest/Game`, and BI's own `scripts/Game/Tests` override works, so the sealing case should not apply. Caught in 5s by `tools/compile-check.sh`. **Fallback:** move world/systems selection into `OVT_TEST_SuiteBase` by overriding the *virtual instance* methods `GetWorldFile()`/`GetWorldSystemsConfigFile()`, keeping the modded helper for `RequestScenarioChangeTransition` only (the sole injection point for the addon list). If even that fails, document the gap and use the group-config path. |
| **R2** | **World transition fails or hangs** — wrong addon list, world unresolvable in the client, or the transition never completes. `Setup_AwaitWorld` has **no timeout**, so this is an infinite hang, not an error. | Med | High | The framework logs the transition with `forceFileWrite`, so `autotest.log` shows the attempted world/config even on a hang. Phase 2 varies the addon list in a fixed order: base+OVT → base+OVT+EPF+EDF → `GetLoadedAddons()` union. `run-tests.sh`'s timeout converts a hang into exit 124 rather than a stuck CI job. The gap is documented rather than patched (requirement). |
| **R3** | **Class-name `-autotest` forms rejected by the retail client**, as BI's example was. | Low-Med | Med | Decision 4's fallback is fully specified in advance: hand-authored `.conf` + `.meta` with a generated GUID, collision-checked, proven by the `CLI autotest config:` log line. Decided at the Phase 2 gate, not improvised. |
| **R4** | **False green** — `run-tests.sh` exits 0 without tests having run (missing/stale/empty `junit.xml`). | Med | Critical | Four-condition verdict (artifact present, parses, >=1 case, zero failures); stale artifacts deleted before launch; `LOG_DIR` resolved fresh per run by #1's rule; T6/T7 verify the empty and missing cases explicitly. This is the same discipline that caught #1's base-game false-pass trap. |
| **R5** | **Test classes change shipped runtime behaviour** — they are unguarded and auto-registered by `TestHarness` at startup. | Low | High (players) | Task 2.6 compares a no-`-autotest` run against a pre-Phase-1 baseline: no runner created, no transition, no artifacts, no new log errors. If anything differs, the feature does not ship until it is explained. |
| **R6** | **The default test world has no Overthrow game mode**, so the loop proves less about Overthrow than it appears to. | Certain | Med (for #3) | Acknowledged and documented, not hidden (Decision 7). The smoke test *logs* game-mode presence so #3 inherits the evidence. #2's remit is the plumbing; conflating the two would smuggle coverage work into a feature that explicitly excludes it. |
| **R7** | **A Reforger update moves or changes the framework** (`scripts/Autotest/Game` out of the `game` module, `$logs:/junit.xml` relocated, attribute API changed). | Low per update | Med | `findings.md` is stamped with the engine/game build, mirroring #1's maintenance note. Module changes fail at compile time; path changes fail as exit 2 (indeterminate), never as a silent pass. `tools/README.md` gains a re-verify note. |
| **R8** | **Scope creep into real coverage** — "while we're here, let's assert the economy initialises." | Med | Med | The rule is written into the Testing Strategy: if a proposed test could fail because Overthrow has a bug, it belongs to feature #3. The smoke test asserts nothing about Overthrow; diagnostics are log-only by design. |
| **R9** | **Hand-authored `.conf` GUID not registered in the client's resource database**, so `Resource.Load("{GUID}")` fails even though the file is well-formed. | Low (only if R3 fires) | Med | Decision 4 step 4 verifies by execution, not inspection; step 6 falls back to one Workbench GUI round-trip with the user. |

---

## Notes

- **`-autotest-output-dir` is a decoy** for this feature. `junit.xml` is written to the hardcoded `$logs:/junit.xml` (`SCR_AutotestReport.c:4`); the output-dir flag serves the screenshot/perf autotest entities, which are out of scope.
- **`SCR_TEST_Example1TestSuite.c` is a decoy file name** — the class inside is `SCR_TEST_Example1SubjectSuite`, and the whole file is `#ifdef WORKBENCH`. Never use it as a probe (#1 already burned a run on it).
- **The Workbench GUI path exists** (`SCR_AutotestTool`, `SCR_AutotestPlugin`, `SCR_AutotestDebugMenu`) and `GetDefaultLaunchParams()` feeds it. It is not part of this feature's contract, but pointing it at the `OverthrowCI` profile costs nothing and keeps GUI-launched runs landing where the tooling looks.
- **Suite `Print`/`PrintFormat` are shadowed** in `SCR_AutotestSuiteBase`/`SCR_AutotestCaseBase` to route through `SCR_AutotestPrinter` into `autotest.log`. Test code should use them rather than global `Print` — worth a line in the skill.
- **`maxAttempts`** on `[Test]` re-runs a flaky test. Document it, but note the project position: a test that needs retries is usually a bug in the test. #3 decides its own policy.

---

## Related Documentation

- `docs/features/dev-ops/epic-overview.md` — epic scope and build order
- `docs/features/dev-ops/autotest-foundation/requirements.md` — this feature's requirements
- `docs/features/dev-ops/workbench-automation/findings.md` — CLI ground truth (client behaviour, exit codes, `-autotest "{GUID}"` proof)
- `docs/features/dev-ops/workbench-automation/implementation.md` — the plan this one mirrors
- `tools/README.md` — the automation contract this feature extends
- `/mnt/n/Projects/Arma 4/ArmaReforger/scripts/Autotest/Game/TestFramework/` — the framework's API surface
- `/mnt/n/Projects/Arma 4/ArmaReforger/scripts/Game/Tests/TestFramework/SCR_AutotestFramework.c` — the override pattern being copied
- `/mnt/n/Projects/Arma 4/ArmaReforger/scripts/GameLib/tests/TestingFramework.c` — `Test`/`TestStep` attribute API and stage semantics
