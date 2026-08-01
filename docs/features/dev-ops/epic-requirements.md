# Dev-Ops — Epic Requirements

**Created:** 2026-08-01
**Phase:** Active — supersedes `vanilla-persistence` in priority

> Epic-level requirements — the higher-level scope for the whole epic, mirroring a feature's `requirements.md` but one level up. `/plan-epic dev-ops` reads this file (if it exists) to drive epic scoping; otherwise it uses the prompt. Each **child feature** below gets its own `requirements.md` (in the standard Overview / Requirements / Dependencies / Out of Scope shape) that `/plan-feature dev-ops/<feature-name>` consumes.

## Overview

This epic gives Overthrow an automated quality gate for the first time in its life, using first-party tooling that shipped in Arma Reforger 1.7.0: the `SCR_Autotest` script test framework (with native JUnit XML output) and the Workbench's command-line automation flags. It owns everything between "a developer or agent edits a script" and "we know whether that edit compiles, passes its tests, and can ship" — compile checking, test authoring and execution, CI orchestration, and Workshop release publishing.

These features belong together because they form one dependency chain around a single scarce resource: the ability to drive the Enfusion toolchain without a human. Once that exists, compile checking, testing, CI and publishing are all just different verbs against the same launcher.

## Requirements

- **A script change can be compile-checked with no human interaction** — invoked from a WSL shell, returning a meaningful exit code and machine-readable errors, without the Workbench GUI being touched.
- **Claude Code can drive the toolchain unattended** from Ubuntu WSL, via `/mnt/n/...` paths and Windows binary interop, on this same machine.
- **Overthrow-specific test suites can be written, run and reported on**, using the shipped `SCR_Autotest` framework rather than a bespoke harness.
- **Test results are emitted in a CI-consumable format** (the framework's native `junit.xml`) and collected reliably from the profile log directory.
- **Persistence behaviour is covered by tests that survive the EPF → vanilla migration** — assertions at the behaviour level, not against any persistence API — so they act as the migration's acceptance gate.
- **CI runs automatically on push/PR** against the repository, on a self-hosted Windows runner, surfacing pass/fail per test on the PR.
- **Workshop releases can be packed and published from the command line**, with version and changelog supplied non-interactively.
- **No documentation is left asserting capability that has changed.** Every feature updates the docs its completion invalidates, as part of its Definition of Done.
- **Nothing in this epic may weaken the shipped mod.** Test code must not alter runtime behaviour for players; release automation must never publish without an explicit, deliberate trigger.

## Planned Features

The features that make up this epic, in intended **build order**. `/plan-epic` creates a subfolder + `requirements.md` for each, and records the order in `epic-overview.md`.

1. **workbench-automation** — Headless Workbench/game invocation from WSL, plus a compile check with real exit codes and parsed errors — Foundational; unblocks every other feature and is the single biggest standalone iteration-speed win.
2. **autotest-foundation** — Wire `SCR_Autotest` into Overthrow and prove `-autotest` → `junit.xml` end-to-end with one trivial smoke test — Depends on #1 for launching and artifact retrieval; establishes the test contract everything else builds on.
3. **test-coverage** — Behaviour-level suites for persistence round-trip, economy/town logic and manager initialisation — Depends on #2's contract; largest feature, and the one that keeps growing after the epic closes.
4. **ci-pipeline** — Self-hosted Windows runner, GitHub Actions workflow, JUnit results on PRs — Depends on #1 and #2; **can be built in parallel with #3**.
5. **release-automation** — Workshop pack & publish via `-packAddon` / `-publishAddon*` — Depends only on #1; sequenced last as least urgent and outward-facing.

## Dependencies

- **Arma Reforger 1.7.0+** installed at `N:\Program Files (x86)\Steam\steamapps\common\Arma Reforger`, and **Arma Reforger Tools** (Workbench) alongside it. The autotest framework and the Workbench automation flags do not exist in older versions.
- **A Windows host with a GPU.** Reforger has no headless rendering mode — every `Headless` symbol in the binaries refers to headless *MP clients*, not offscreen rendering. CI cannot run on GitHub-hosted Linux runners; a self-hosted Windows runner is required.
- **WSL ↔ Windows interop** available on the dev machine (confirmed working: `binfmt_misc/WSLInterop`, project reachable at `/mnt/n/Projects/Arma 4/Overthrow.Arma4`).
- **The Reforger reference tree** at `/mnt/n/Projects/Arma 4/ArmaReforger`, kept current by `update-arma-scripts.ps1` — this is the source of truth for the framework's API surface.
- **Workshop publishing credentials** for feature #5 only.
- No dependency on `vanilla-persistence` completing. That feature is paused; this epic proceeds independently and will validate it when it resumes.

## Out of Scope

- **Migrating `vanilla-persistence` itself.** This epic tests persistence behaviour; it does not perform or complete the EPF → vanilla migration.
- **Multiplayer / join-in-progress test automation.** JIP is the project's most common regression class but needs two coordinated processes. Out of scope here; `SCR_AutotestHelper.WORLD_MPTEST` (`worlds/MP/MpTest/MpTest.ent`) is noted as a starting point for a future epic.
- **Performance, FPS and screenshot autotests.** The `AutotestGrid` / `Screenshot_Autotest` entities and `-autotest-output-dir` exist but target a different problem.
- **Replacing manual play-testing.** Automation covers compile correctness, logic and persistence round-trips. Feel, balance and emergent behaviour still require a human, and this epic does not pretend otherwise.
- **Retrofitting tests across the whole codebase.** Feature #3 establishes patterns and covers the highest-risk systems; broad coverage accrues over time as features are touched.
- **A hosted/cloud CI runner.** Self-hosted only, given the GPU requirement.
- **Rewriting the Beast Mode workflow docs** absorbed from the old `dev-ops` feature — that feature was outdated (it described the removed v1 `/dev` structure) and was deleted rather than migrated. Recoverable from commit `1ee1cb2` and `.claude/.beast-mode-backup-20260801-171542/` if ever needed.

---

*Consumed by `/plan-epic dev-ops`. After planning, run `/plan-feature dev-ops/<feature-name>` per feature in the recommended order.*
