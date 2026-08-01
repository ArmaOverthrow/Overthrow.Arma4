# Dev-Ops — Epic Overview

**Epic:** dev-ops
**Status:** 🟡 In Progress
**Last Updated:** 2026-08-01 23:10

> **This file is the epic marker.** Its presence in `docs/features/dev-ops/` is what tells every Beast Mode command (and future Web App / Discord clients) that this folder is an **epic**, not a plain feature. Keep it present and keep the required sections below filled in. It is the epic's equivalent of the project's `docs/overview.md`, scoped to this epic. The master `docs/overview.md` tracks this epic as a **single row**; the per-feature detail lives **here**.

---

## Purpose

This epic replaces Overthrow's manual, human-in-the-loop quality gate with an automated one, using first-party tooling that shipped in Arma Reforger and was not previously available. Until now every change — however small — cost a human round-trip: open Workbench, press Build, read the console, host a session, join a second client, play through the change. That cost is the single largest brake on this project's iteration speed, and it is why the codebase has had no test suite at any point in its life.

Reforger 1.7.0 makes that obsolete. The game ships `scripts/Autotest/` — a complete script test framework with stages, async multi-tick steps, timeouts, retries and **native JUnit XML output** — wired into the base game's `game` script module, which Overthrow inherits automatically with no `addon.gproj` change. Separately, the Workbench binary exposes real automation flags (`-gproj`, `-wbmodule`, `-exitAfterInit`, `-wbsilent`, `-plugin`, plus a `-packAddon`/`-publishAddon*` family), and `WorkbenchPlugin` exposes a `RunCommandline()` hook. Together these make unattended compilation, unattended testing, and automated Workshop publishing possible for the first time.

The five features below build that capability bottom-up: first the ability to drive Workbench headlessly from WSL, then the test harness, then real coverage, then CI, then releases. The epic's completion condition is that a change can go from edit to verified without a human touching the Workbench GUI.

---

## Features

The constituent features of this epic, in build order. This is the epic's equivalent of `docs/overview.md`'s feature-status table, scoped to the epic. Each feature is a subfolder under `docs/features/dev-ops/`.

| # | Feature | Status | Tasks | Description |
|---|---------|--------|-------|-------------|
| 1 | workbench-automation | ✅ Complete | 56/56 | Drive Workbench headlessly from WSL; compile check with a real exit code and parsed errors |
| 2 | autotest-foundation | 📋 Planned | — | Wire `SCR_Autotest` into Overthrow; prove `-autotest` → `junit.xml` end-to-end |
| 3 | test-coverage | 📋 Planned | — | Behaviour-level suites: persistence round-trip, economy/town logic, manager init |
| 4 | ci-pipeline | 📋 Planned | — | Self-hosted Windows runner, GitHub Actions, JUnit results surfaced on PRs |
| 5 | release-automation | 📋 Planned | — | Workshop pack & publish via `-packAddon` / `-publishAddon*` |

> Reference any feature with the slash form `dev-ops/<feature-name>` (e.g. `/continue-feature dev-ops/workbench-automation`). Task counts are pulled from each feature's own `tasks.md` and refreshed by `/update-epic`.

---

## Build Order / Dependencies

1. **workbench-automation** — Foundational and unblocking. Nothing else in the epic can be automated until Workbench and the game client can be launched, driven and interpreted from a script. It also delivers the epic's single biggest standalone win: compile errors caught without a human. Build first, in isolation, and prove it before layering anything on top.

2. **autotest-foundation** — Depends on #1 for the ability to launch the game client with `-autotest` and retrieve artifacts. Establishes the test loop itself: the Overthrow-specific `SCR_AutotestHelper` override, a suite base pointed at the project's own world, one deliberately trivial smoke test, and confirmed `junit.xml` retrieval. Deliberately thin — its job is to prove the loop, not to provide coverage.

3. **test-coverage** — Depends on #2. This is where actual assertions get written. Largest feature in the epic and the one that keeps growing after the epic closes.

4. **ci-pipeline** — Depends on #1 and #2 (needs a working compile check and a working test loop to orchestrate). **Can be built in parallel with #3** — the pipeline does not care how many suites exist, only that the commands and artifacts are stable.

5. **release-automation** — Independent of #2–#4; needs only #1's Workbench-launching foundation. Sequenced last because it is the least urgent and carries outward-facing risk (it publishes to the Workshop). Small.

**Dependencies between features:**
- `workbench-automation` → `autotest-foundation` (launch + artifact retrieval)
- `workbench-automation` → `release-automation` (Workbench CLI invocation)
- `autotest-foundation` → `test-coverage` (suite base, helper override, assertion patterns)
- `autotest-foundation` + `workbench-automation` → `ci-pipeline` (stable commands and artifact paths to orchestrate)
- **Parallel:** `test-coverage` ∥ `ci-pipeline` once #2 lands
- **External:** requires a Windows host with a GPU. There is no headless rendering in Reforger — every "Headless" symbol in the binaries refers to headless *MP clients*, not offscreen rendering. CI therefore cannot run on GitHub-hosted Linux runners.

---

## Integration & Architecture

- **Within the epic:** Feature #1 owns the process boundary — one wrapper layer that everything else calls rather than each feature shelling out to `.exe` paths of its own. #2 owns the in-game test contract (`SCR_AutotestHelper` override, suite base). #3 consumes #2's contract and adds no new infrastructure. #4 orchestrates #1 and #2's commands and consumes their artifacts; it must not reimplement either. #5 reuses #1's launcher for a different Workbench verb.

- **With the rest of the project:** This epic touches almost no gameplay code. Its one intrusion into `Scripts/Game/` is a new test tree (suites and cases) plus an `OVT_AutotestHelper` modding of `SCR_AutotestHelper`. Test classes must **not** be wrapped in `#ifdef WORKBENCH` — the shipped BI example is, but the framework runs in the retail client and guarding them would make them Workbench-only.

- **Relationship to `vanilla-persistence`:** That feature is paused, not abandoned, and this epic supersedes it in priority. The two are deliberately coupled through feature #3: persistence tests are written at the **behaviour** level (does town control survive a save/reload?) with no EPF or vanilla API in the assertions. They therefore survive the migration and become its acceptance gate — reframing `vanilla-persistence` from "paused work" into "the first thing the new CI validates."

- **Key architectural decisions spanning the epic:**
  - **Windows-host CI is a hard constraint**, not a preference. Plan for a self-hosted runner from the start; do not design around GitHub-hosted runners.
  - **`junit.xml` path is fixed** at `$logs:/junit.xml`. The `-autotest-output-dir` flag that exists applies to the screenshot/perf autotest entities, **not** to the JUnit report. Artifact collection must resolve the profile log directory.
  - **The agent drives the toolchain from WSL** via `/mnt/n/...` paths and Windows binary interop. Path translation (WSL ↔ Windows) is a first-class concern owned by feature #1, not solved ad hoc five times.
  - **Docs are updated by the feature that invalidates them** — see below.

- **Documentation policy for this epic:** `CLAUDE.md`, `docs/technical-design.md` §2 and §10, `docs/mission-statement.md` ("Play-testing as the quality gate") and the `workbench-workflow` skill originally asserted that this project has no automated builds, no tests and no debugger. Each of those claims becomes false at a specific point in this epic — feature #1 has already corrected the "no automated builds" claims; the no-tests and no-debugger claims remain true and stay until their invalidating feature lands. **Every feature's Definition of Done includes updating the docs it invalidates** — there is no separate docs feature, and docs are never allowed to describe capability that does not yet exist.

---

## Tech Debt / Findings

Cross-feature tech debt and review findings. **Populated and updated by `/review-epic`** (which also writes feature-specific findings into the relevant child `context.md` files). Start empty.

- (none yet — `/review-epic dev-ops` will surface cross-feature integration issues and per-feature tech debt here)

---

## Master Overview Rollup

- **Rollup status:** In Progress (1/5 features complete)
- **One-line summary for master:** Automated compile, test and release pipeline for Overthrow, built on Reforger 1.7.0's shipped autotest framework and Workbench CLI — replaces the manual play-testing quality gate.

---

## Research Basis

Everything this epic depends on was verified by reading the Reforger 1.7.0 files installed on this machine, not from documentation. Key references for whoever plans the child features:

| Capability | Evidence |
|---|---|
| Test framework exists & is inherited | `ArmaReforger/scripts/Autotest/` (13 files); `ArmaReforger.gproj:927` adds `scripts/Autotest/Game` to the **`game`** module |
| Test authoring API | `GameLib/tests/TestingFramework.c` — `Test` / `TestStep` attributes, stages, timeouts, `maxAttempts` |
| Assertions & base class | `SCR_AutotestCaseBase.c` — `AssertTrue`, `SetResultSuccess`, `SetResultFailure` |
| Suite controls the world | `SCR_AutotestSuiteBase.c:11-21` — `GetWorldFile()` / `GetWorldSystemsConfigFile()` |
| CLI entry point | `SCR_AutotestRunner.c:4-8` — `-autotest <GUID \| SuiteClass \| CaseClass>` |
| JUnit output + auto-exit | `SCR_AutotestReport.c:4-5` (`$logs:/junit.xml`, `$logs:/autotest_failed.log`); `SCR_AutotestRunner.c:112-124` |
| Canonical launch command | `SCR_AutotestTool.c:90-101` + `SCR_AutotestFramework.c:23-26` (BI's own default launch params) |
| Workbench automation flags | `ArmaReforgerWorkbenchSteamDiag.exe` strings — `-exitAfterInit`, `-wbsilent`, `-wbmodule`, `-plugin`, `-packAddon*`, `-publishAddon*` |
| CLI automation hook | `GameLib/generated/WorkbenchAPI/Plugins/WorkbenchPlugin.c:20` — `RunCommandline()` |
| Working example to copy | `scripts/Game/Tests/TestSuites/Example/SCR_TEST_Example1TestSuite.c` |
| No headless rendering | Both binaries: every `Headless` symbol is `PlayOnHeadlessClient` / `SimulateOnHeadless` (MP clients); no `-noRender` |

**Empirically verified by feature #1** (2026-08-01, Reforger 1.7.0.54 / engine 190965, Tools stable branch): the toolchain in this table was executed end-to-end — exit codes, log formats and the compile-error surface are recorded in `workbench-automation/findings.md`, which supersedes this table where they differ. Two corrections: the shipped example suite classes are `#ifdef WORKBENCH`-guarded (and `SCR_TEST_Example1TestSuite.c` actually contains `SCR_TEST_Example1SubjectSuite`), so they are invalid in the retail client — use an `SCR_AutotestGroup` config GUID instead; and the Workbench ignores the optional `-validate [configName]` argument, so the gproj's own `workbench` script configuration always applies.

---

*This is a required file — do not delete it; it marks the folder as an epic. Update it with `/update-epic dev-ops` after working on the epic's features, and run `/review-epic dev-ops` to refresh the Tech Debt / Findings section.*
