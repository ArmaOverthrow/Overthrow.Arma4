# Autotest Loop Empirical Findings (Phase 2)

**Date:** 2026-08-01
**Reforger build:** game version **1.7.0.54** built 2026-06-14 12:57:43 UTC, engine version **190965** (from `console.log`: `INIT : Creating game instance(ArmaReforgerScripted), version 1.7.0.54 built 2026-06-14 12:57:43 UTC.` / `ENGINE : Initializing engine, version 190965`).
**Validity:** these findings are valid ONLY for this build. Re-run the experiment set after any Reforger update (see `tools/README.md` Maintenance).
**Launcher:** every run used `tools/launch-game.sh --timeout 120 -- -autotest <target>` (feature #1's boundary; no `.exe` was ever invoked directly). Default addon set: EDF + EPF (packed workshop) + Overthrow source; profile `OverthrowCI`.
**Steam:** running and logged in throughout. GPU: NVIDIA GeForce RTX 4070.
**Tree state:** Phase 1 output (4 files under `Scripts/Game/Tests/`), compile-check exit 0, no other modifications. `ADDONS_OVT` was **never edited** — the shipped two-GUID list worked first try (see 2.2 and "Addon survival" below).

## Experiment table

| # | Command (`tools/launch-game.sh --timeout 120 -- ...`) | Observed outcome | Wall time | Artifacts in `$LOG_DIR` | Notes |
|---|---|---|---|---|---|
| 2.2 | `-autotest OVT_TEST_SmokeSuite` (suite-class form) | **WORKS.** `console.log`: `CLI autotest suite: OVT_TEST_SmokeSuite` (twice — see "Harness runs twice"); `Creating: SCR_TestRunner`; `Requesting scenario change:` (twice); `SCR_TestRunner has finished running`; `Autotest JUnit XML saved to: $logs:/junit.xml`. Client self-exited. Launcher rc 0. | **22 s** | `junit.xml` (1 passing case), `autotest.log`, `autotest_failed.log` (0 bytes), `console.log`, `error.log`, `script.log` | 3 world loads total: main menu → test world → test world again (framework re-requests the transition after reload). Smoke test ticked 5 Main-step frames then succeeded. |
| 2.3 | `-autotest OVT_TEST_Smoke_HarnessRuns` (case-class form) | **WORKS.** `console.log`: `CLI autotest case: OVT_TEST_Smoke_HarnessRuns` (twice); same runner/finish/save lines as 2.2. junit.xml contains exactly one case. Launcher rc 0. | **14 s** | same set as 2.2 | The case runs inside its owning suite (`testsuite name="OVT_TEST_SmokeSuite"` in junit.xml). Faster than 2.2 — likely warm caches; treat 14–22 s as the normal green-run band. |
| 2.4 | `-autotest OVT_TEST_MetaSuite` (red path) | **FAILURE IS SURFACED, not swallowed.** junit.xml contains a `<failure type="Result">` element; `autotest_failed.log` names the test; `autotest.log` shows `⛔ OVT_TEST_Meta_AlwaysFails: FAILURE`. Client still self-exits cleanly, launcher rc 0, client exit code 0 (meaningless as always). | **15 s** | same set as 2.2; `autotest_failed.log` is 26 bytes | Verbatim shapes below. Red is only visible in the artifacts — nothing about the process differs from a green run. |
| 2.5 | `-autotest "{6AB9C8EEE9A651B5}"` (BI's shipped empty group) | **NO LEAK.** `console.log`: `CLI autotest config: SCR_AutotestGroup<0x...>`; junit.xml is an empty `<testsuites>` element. The harness's `Tests to run:` debug listing shows the Overthrow suites registered but with enable-count **0** — they do NOT run. **No world transition happened** (no `Requesting scenario change:`). | **8 s** | `junit.xml` (empty), `autotest.log` (header only), `autotest_failed.log` (0 bytes), logs | Confirms "no run-everything form": an unnamed suite is disabled, period. Also confirms the T7 indeterminate input for Phase 3 (junit.xml with zero `<testcase>`). |
| 2.6 | *(no `-autotest` at all)* | **INERT.** Client boots to main menu and **stays there** — no `Creating: SCR_TestRunner`, no `CLI autotest` line, no `Requesting scenario change:`, no `junit.xml` / `autotest.log` / `autotest_failed.log`, zero mentions of any `OVT_TEST_` class in `console.log`. Run ended by the launcher's 120 s timeout (rc **124**, kill verified by the tool; partial logs still written). | 120 s (timeout by design — a menu session never self-exits) | `console.log`, `error.log`, `script.log` only | Errors in `console.log`: 3 total — `GUI (E): Unknown class 'SCR_WidgetExportRuleRoot'` (1x) and `SCRIPT (E): Can't instantiate class 'SCR_FilterCategory', constructor is not public` (2x). All three are pre-existing base-game/menu noise that also appears in the main-menu phase of every autotest run (e.g. run 2.2 at 23:05:43.166) and in feature #1's runs. **No error is attributable to the test classes. Shipping safety confirmed.** |
| 2.7 | `-autotest OVT_TEST_DoesNotExist` (invalid target) | **Clean self-exit with crash.log**, exactly per feature #1's finding 1.14c. `crash.log`: `Virtual Machine Exception / Reason: Invalid -autotest parameter value: OVT_TEST_DoesNotExist` (stack: `SCR_AutotestRunner.c:85 HandleCommandLineArguments`). **No junit.xml**, no autotest.log. Launcher rc 0, client exit code 0. | **7 s** | `console.log`, `crash.log`, `error.log`, `script.log` — **no junit.xml** | Phase 3 MUST classify this as exit **2** (indeterminate): the only distinguishing evidence is missing `junit.xml` (+ optionally `crash.log` presence). Exit code and launcher rc look identical to a green run. |
| 2.9 | *(Decision 4 fallback: hand-authored group config)* | **DID NOT FIRE.** Both class-name forms work (2.2, 2.3); no `Configs/Tests/` files were created. | n/a | n/a | Decision 3 stands: class names are the primary contract. |

## Verbatim artifacts

### Passing junit.xml — suite form (run 2.2, `logs_2026-08-01_23-05-35`)

```xml
<testsuites time="10.133684" timestamp="2026-08-01T13:05:46.399Z">
	<testsuite name="OVT_TEST_SmokeSuite" tests="1" time="8.024225" timestamp="2026-08-01T13:05:48.508Z">
		<testcase classname="OVT_TEST_SmokeSuite" name="OVT_TEST_Smoke_HarnessRuns" time="2.153866" />
	</testsuite>
</testsuites>
```

### Passing junit.xml — case form (run 2.3, `logs_2026-08-01_23-09-04`)

```xml
<testsuites time="3.302894" timestamp="2026-08-01T13:09:14.381Z">
	<testsuite name="OVT_TEST_SmokeSuite" tests="1" time="1.745700" timestamp="2026-08-01T13:09:15.938Z">
		<testcase classname="OVT_TEST_SmokeSuite" name="OVT_TEST_Smoke_HarnessRuns" time="0.245239" />
	</testsuite>
</testsuites>
```

### Failing junit.xml (run 2.4, `logs_2026-08-01_23-09-49`)

```xml
<testsuites time="2.999306" timestamp="2026-08-01T13:09:59.559Z">
	<testsuite name="OVT_TEST_MetaSuite" tests="1" time="1.472870" timestamp="2026-08-01T13:10:01.085Z">
		<testcase classname="OVT_TEST_MetaSuite" name="OVT_TEST_Meta_AlwaysFails" time="0.000232">
			<failure type="Result">Deliberate failure: proves the harness reports red</failure>
		</testcase>
	</testsuite>
</testsuites>
```

`autotest_failed.log` (complete content — one test name per line, no decoration):

```
OVT_TEST_Meta_AlwaysFails
```

Notes for Phase 3's parser and feature #4's annotations:

- A passing `<testcase>` is **self-closing** with no children; a failing one wraps a `<failure type="Result">message</failure>` element whose text is the `SetResultFailure()` string verbatim.
- `<testsuite>` carries **no** `failures=`/`errors=` attribute on this build — only `tests=`, `time=`, `timestamp=`. Failure counting must count `<failure>` elements, not read an attribute.
- On a green run `autotest_failed.log` exists but is **0 bytes**.
- The XML uses tabs for indentation and no XML declaration line.

### Empty-group junit.xml (run 2.5, `logs_2026-08-01_23-10-21`)

```xml
<testsuites time="1.591631" timestamp="2026-08-01T13:10:26.626Z">
</testsuites>
```

Identical shape to feature #1's 1.14d observation. This is Phase 3's "zero test cases → exit 2" input.

### Key console.log lines (quoted)

```
23:05:40.997 SCRIPT       : CLI autotest suite: OVT_TEST_SmokeSuite
23:09:09.458 SCRIPT       : CLI autotest case: OVT_TEST_Smoke_HarnessRuns
23:10:26.626 SCRIPT       : CLI autotest config: SCR_AutotestGroup<0x000001C193D75008>
23:05:40.998 SCRIPT       : Creating: SCR_TestRunner
23:05:43.075 SCRIPT    (D): Requesting scenario change:
	{D87EF7EED4210569}Worlds/MP/OVT_Campaign_Test.ent
	{1C60D2EDA2B468B8}Configs/Systems/BaseGameModeSystems.conf
23:05:56.533 SCRIPT       : SCR_TestRunner has finished running
23:05:56.534 SCRIPT       : Autotest JUnit XML saved to: $logs:/junit.xml
23:05:56.534 SCRIPT       : Autotest failed list saved to: $logs:/autotest_failed.log
```

The harness's target-selection debug listing (run 2.2; `1` = enabled, `0` = disabled):

```
23:05:40.998 SCRIPT    (D): (SCR_AutotestHarness) Tests to run:
23:05:40.998 SCRIPT    (D): 	OVT_TEST_MetaSuite: 0
23:05:40.998 SCRIPT    (D): 		OVT_TEST_Meta_AlwaysFails: 1
23:05:40.998 SCRIPT    (D): 	OVT_TEST_SmokeSuite: 1
23:05:40.998 SCRIPT    (D): 		OVT_TEST_Smoke_HarnessRuns: 1
23:05:40.998 SCRIPT    (D): 	OVT_TEST_SuiteBase: 0
23:05:40.998 SCRIPT    (D): 	SCR_AutotestSuiteBase: 0
```

(Note: base classes `OVT_TEST_SuiteBase`/`SCR_AutotestSuiteBase` are themselves auto-registered as suites — always disabled, harmless, but they will appear in every listing. The per-case `1` under a disabled suite does not run it; suite enablement gates the run, as run 2.5 proves.)

### Invalid-target crash.log (run 2.7, `logs_2026-08-01_23-13-37`)

```
Virtual Machine Exception

Reason: Invalid -autotest parameter value: OVT_TEST_DoesNotExist

Class:      'SCR_AutotestRunnerCore'
Function: 'HandleCommandLineArguments'
Stack trace:
scripts/Autotest/Game/TestFramework/SCR_AutotestRunner.c:85 Function HandleCommandLineArguments
scripts/Autotest/Game/TestFramework/SCR_AutotestRunner.c:19 Function ShouldCreate
Scripts/Game/Tests/TestFramework/SCR_AutotestRunnerCore.c:13 Function CanCreate
```

### autotest.log shape (run 2.2; timestamps `HH:MM:SS`, content also mirrored into console.log/script.log)

```
23:05:48 
23:05:48 ############################################/
23:05:48 TestSuite #OVT_TEST_SmokeSuite started
23:05:48 Requesting scenario change:
	{D87EF7EED4210569}Worlds/MP/OVT_Campaign_Test.ent
	{1C60D2EDA2B468B8}Configs/Systems/BaseGameModeSystems.conf
23:05:56 	✅ OVT_TEST_Smoke_HarnessRuns: SUCCESS
23:05:56 	Smoke test teardown complete
23:05:56 /############################################
23:05:56 
```

(Status glyphs are UTF-8 characters written by the framework itself — recorded verbatim; parsers must expect non-ASCII in autotest.log.)

Red variant (run 2.4):

```
23:10:02 	⛔ OVT_TEST_Meta_AlwaysFails: FAILURE
23:10:02 		Failure reason: Deliberate failure: proves the harness reports red
23:10:02 	 Output: <none>
```

## Test-world evidence for feature #3 (log-only diagnostics)

The smoke test's deliberate log-only diagnostics printed, in the loaded test world (run 2.2, `console.log` lines 1319-1320; also present in run 2.3):

```
23:05:56.533 SCRIPT       : 	Diagnostic (log only): OVT_OverthrowGameMode present = true
23:05:56.533 SCRIPT       : 	Diagnostic (log only): OVT_Global.GetTowns() non-null = true
```

**Both true.** The plan's assumption that `OVT_Campaign_Test.ent` is a bare MpTest world with no Overthrow game mode is **wrong**: the `.ent` file itself is a three-line `SubScene`, but its sibling layer directory `Worlds/MP/OVT_Campaign_Test_Layers/default.layer` (354 lines) adds a full Overthrow setup — `OVT_OverthrowGameMode` (`{4DC10DCB8BE06D32}Prefabs/GameMode/OVT_OverthrowGameMode.et`), `OVT_OverthrowFactionManager`, `OVT_BaseController`, `OVT_TownController`, `SCR_MapEntity` (Arland), `SCR_AIWorld`, `PerceptionManager`, buildings, slots, and an `OverthrowMobileFOB` vehicle. Reading only the `.ent` misses the layers.

Consequence for feature #3: manager-backed assertions are possible **today** in the default test world — no new world or `GetWorldFile()` override is needed for tests that only need the game mode and managers present.

Caveats observed in the test world (candidates for #3 to investigate, not caused by test code):

- `SCRIPT (E): [Overthrow] Failed to get SCR_PersistenceSystem instance!` fires once per test-world load during `OVT_OverthrowGameMode` entity creation (runs 2.2/2.3/2.4; e.g. 23:05:47.496 and 23:05:53.972 in run 2.2), followed later by a successful `[Overthrow] Initializing Persistence`. Pre-existing `vanilla-persistence`-branch init-order issue surfacing in this world; NOT introduced by the test tree (absent from run 2.6 only because no world with the game mode is ever loaded there).
- The Overthrow start-game menu logic runs in the test world (`[Overthrow] Showing start menu for single player` after the first transition) — the game mode is present but the campaign is **not started** (`Game started: false, Has save: false`). Tests needing a *running* campaign still need explicit start/setup; presence of managers != initialized game state.
- `PATHFINDING(E): Failed to load Navmesh from file! Will initialize empty navmesh world` — despite `Worlds/MP/.NavData/OVT_Campaign_Test/navData_0.ntile` existing in the repo. AI-movement tests will need this resolved.

## Addon survival across the scenario transition (Decision 8 check)

`ADDONS_OVT = "58D0FB3206B6F859,59B657D731E2A11D"` (base + Overthrow only) was sufficient. The post-transition `Loaded addons:` block (run 2.2, 23:05:44.415) lists all five addons — `core`, `ArmaReforger`, `EnfusionDatabaseFramework` (packed), `EnfusionPersistenceFramework` (packed), `Overthrow` — identical to the pre-transition block. EPF/EDF were pulled transitively via Overthrow's `addon.gproj` dependency declarations, as Decision 8 hoped. **No edit to `ADDONS_OVT` was needed; R2's mitigation ladder was never climbed.**

## Harness runs twice per launch (observed mechanics)

Every class-form run shows the full harness startup **twice**:

1. Client boots into the main menu world (`Entered main menu`, `No GameMode present in the world, using fallback logic!`). `SCR_TestRunner` is created, the suite starts, `Setup_OpenWorld` requests the scenario change (`TransitionRequest { state: MainMenu, request: WorldChange }`).
2. The test world loads; scripts and game instance are re-created (second `Loaded addons:` block, second `CLI autotest suite:` line, second `Creating: SCR_TestRunner`). The suite starts again and **requests the same scenario change again** (`TransitionRequest { state: OfflineGame, request: WorldChange }`), causing a second load of the already-loaded test world (run 2.2: `LoadEntities` at 23:05:48 [1089 ms] and again at 23:05:54 [145 ms]).
3. Only this third world instance runs the test cases; junit.xml carries the timestamp of the second harness creation.

So a green run = 3 world loads (menu, test world, test world again). `Setup_OpenWorld` does not detect "already in the target world". Cost is a few seconds; harmless, but it explains the doubled log lines and why `junit.xml`'s `time=` (~10 s in 2.2) exceeds the visible test time (~2 s). Do not treat doubled `CLI autotest`/`Requesting scenario change:` lines as an anomaly.

## Timing and timeout proposal (task 2.8)

| Run | Wall time (launcher `DURATION_S`) |
|---|---|
| 2.2 suite form (cold-ish) | 22 s |
| 2.3 case form | 14 s |
| 2.4 red path | 15 s |
| 2.5 empty group (no transition) | 8 s |
| 2.7 invalid target | 7 s |

**Proposed `tools/run-tests.sh` default timeout: 300 s.** Worst observed complete run is 22 s; 300 s is >13x headroom, covering cold shader/resource caches, OneDrive stalls, and future suites that load heavier worlds, while still failing a hung CI job in 5 minutes instead of feature #1's 600 s launcher default. The timeout is **mandatory, not advisory**: `Setup_AwaitWorld` ticks `!IsTransitionRequestedOrInProgress()` forever with no timeout of its own (`SCR_AutotestSuiteBase.c:67-71`), so a world transition that never completes (bad addon list, unresolvable world) is an **infinite hang** whose only backstop is the process timeout — run 2.6 demonstrates the mechanics: a client that never self-exits ends only when `launch-game.sh` kills it (rc 124, kill verified).

Determinism note: two green runs (2.2, 2.3) and one red run all produced well-formed junit.xml with consistent shapes; no flakiness observed in this session.

## Differs from assumptions

Contradictions or corrections to `implementation.md`'s ground-truth table:

1. **The default test world DOES have the Overthrow game mode.** The ground-truth table and Decision 7/R6 claim `OVT_Campaign_Test.ent`'s "entire content is `SubScene { Parent MpTest_Basic }`" and that it has "no Overthrow game mode". The `.ent` is indeed that SubScene, but `OVT_Campaign_Test_Layers/default.layer` adds `OVT_OverthrowGameMode` + faction manager + town/base controllers (see diagnostics section). Both smoke diagnostics returned **true**. R6 ("Certain") did not materialize. Feature #3's constraint is softer than planned: managers exist; a *started campaign* does not.
2. **The harness runs twice and loads the test world twice per launch** (see mechanics section). Not mentioned anywhere in the plan; harmless but observable in every log.
3. **`<testsuite>` has no `failures=`/`errors=` attributes** on this build — Phase 3's parser must count `<failure>` (and `<error>`, none observed) elements rather than reading attributes.
4. Everything else held: both class-name forms accepted; junit at `$logs:/junit.xml`; `autotest.log`/`autotest_failed.log` alongside; invalid value → VM exception + crash.log + clean self-exit; empty group → empty `<testsuites>`; inertness without `-autotest`; client exit code always 0 and meaningless.

## Framework gaps

Documented, not worked around (per requirements):

1. **No run-everything CLI form.** The default group is empty and `ConfigureTestSuites` disables every suite not explicitly named; run 2.5 proves Overthrow suites do not leak into a group run. Running >1 suite in one launch requires an `SCR_AutotestGroup` config (`.conf` + `.meta` with GUID) — needed the moment feature #3 adds a second suite (Decision 4 documents the hand-authoring procedure; unexercised, since 2.9 never fired).
2. **`Setup_AwaitWorld` has no timeout** — an unresolvable world transition is an infinite hang. Only the wrapper's process timeout bounds it. (`SCR_AutotestSuiteBase.c:67-71`.)
3. **A run without a started campaign:** the default test world boots `OVT_OverthrowGameMode` to the start-game menu; nothing starts the campaign. Tests requiring economy/occupying-faction state need explicit game-start setup (feature #3's problem, now with evidence).
4. **Invalid `-autotest` is indistinguishable from success by process signals** — exit code 0, launcher rc 0, clean exit; only missing `junit.xml` (+ `crash.log`) reveals it. Any wrapper that does not positively require `junit.xml` will false-green on a typo'd suite name.
5. **Redundant double world-load** per run (~5-7 s waste): `Setup_OpenWorld` re-requests the transition even when already in the target world.
6. **Deprecation noise is normal:** every harness startup emits ~12 `'GetNSuites'/'GetSuite' is obsolete` script warnings and `Calling TestHarness.Begin() without arguments is deprecated...`. Baseline noise on 1.7.0.54; do not alarm on it, but a future Reforger release may remove the deprecated path (re-verify after updates).
7. **Base suite classes are auto-registered:** `OVT_TEST_SuiteBase` (and BI's `SCR_AutotestSuiteBase`) appear as (disabled, empty) suites in the harness listing. Cosmetic; means "suite class named on CLI" would even accept the base class (untested — nothing would run).

## Run index (log directories under `My Games/OverthrowCI/logs/`)

| Run | Log dir |
|---|---|
| 2.2 | `logs_2026-08-01_23-05-35` |
| 2.3 | `logs_2026-08-01_23-09-04` |
| 2.4 | `logs_2026-08-01_23-09-49` |
| 2.5 | `logs_2026-08-01_23-10-21` |
| 2.6 | `logs_2026-08-01_23-11-00` (partial — timeout kill) |
| 2.7 | `logs_2026-08-01_23-13-37` (has `crash.log`) |
