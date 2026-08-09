# Autotest Foundation — Requirements

**Epic:** dev-ops
**Created:** 2026-08-01

## Overview

Wires Arma Reforger's shipped `SCR_Autotest` framework into Overthrow and proves the full loop works end-to-end: write a test → launch the game with `-autotest` → get a `junit.xml` back. Deliberately thin. Its job is to establish the contract and prove the plumbing, **not** to provide test coverage — that is feature #3.

The framework is inherited for free: `ArmaReforger.gproj:927` adds `scripts/Autotest/Game` to the base game's `game` script module, which Overthrow already extends. No `addon.gproj` change is needed.

## Requirements

- **An Overthrow-specific `SCR_AutotestHelper` override exists**, via `modded class`, supplying the project's own defaults — following the pattern in `ArmaReforger/scripts/Game/Tests/TestFramework/SCR_AutotestFramework.c`:
  - `GetDefaultWorld()` → an Overthrow test world (`Worlds/MP/OVT_Campaign_Test.ent` loads far faster than Eden and is the established dev world)
  - `GetDefaultSystemsConfig()`, `GetDefaultLaunchParams()`, `RequestScenarioChangeTransition()` — with Overthrow's addon GUID (`59B657D731E2A11D`) in the addon list, alongside the base game's
- **An Overthrow suite base class exists** for project tests to inherit, so world selection and any shared setup live in one place.
- **At least one deliberately trivial smoke test** that proves the loop — plus, ideally, one deliberately *failing* test used once to confirm failures are actually reported and not silently swallowed. A green run that cannot go red is worthless.
- **`-autotest` accepts a suite class name and a single case class name**, both confirmed working (per `SCR_AutotestRunner.c:4-8`).
- **`junit.xml` is retrieved reliably.** It is written to `$logs:/junit.xml` — a fixed path. Note that `-autotest-output-dir` does **not** apply to it (that flag serves the screenshot/perf autotest entities). The profile log directory must be resolved and the artifact collected.
- **Test classes must not be wrapped in `#ifdef WORKBENCH`.** BI's shipped example is, but the framework runs in the retail client; guarding Overthrow's tests would make them Workbench-only and useless to CI.
- **Test code must not affect the shipped mod's runtime behaviour** for players.
- **Establish and document the authoring patterns** other developers and agents will copy: stages (`Setup`/`Main`/`TearDown`), `bool`-returning steps that re-run each tick until true, timeouts (`timeoutS`/`timeoutMs`), `maxAttempts` for flakiness, and assertions (`AssertTrue`, `SetResultSuccess`, `SetResultFailure`).
- **Decide and document the test file layout** under `Scripts/Game/` (BI uses `scripts/Game/Tests/TestSuites/<Area>/`), and the `OVT_TEST_` naming convention.

## Definition of Done — documentation

This feature makes "no unit tests / no automated testing" false. Its completion **must** update:
- `CLAUDE.md` — the "No unit tests" constraint and the testing guidance
- `docs/technical-design.md` §10 (Testing Strategy) and §2
- `docs/mission-statement.md` — "Play-testing as the quality gate"
- The `workbench-workflow` skill — add the test-authoring patterns

## Dependencies

- **`dev-ops/workbench-automation` must be complete** — needs its game-client launcher and artifact/log-directory resolution.
- Arma Reforger 1.7.0+ (the framework does not exist in older versions)
- The Reforger reference tree at `/mnt/n/Projects/Arma 4/ArmaReforger` for the framework's API surface
- `Worlds/MP/OVT_Campaign_Test.ent` (exists)

## Out of Scope

- **Real test coverage** — one smoke test and one deliberate failure only. Assertions about economy, towns, persistence or managers belong to feature #3.
- CI orchestration — feature #4.
- Multiplayer / JIP test automation — needs two coordinated processes; deferred to a future epic. (`SCR_AutotestHelper.WORLD_MPTEST` = `worlds/MP/MpTest/MpTest.ent` noted as a starting point.)
- Performance/FPS/screenshot autotests (`AutotestGrid`, `Screenshot_Autotest`).
- Building a bespoke test harness. The shipped framework is used as-is; if it is missing something, document the gap rather than replacing it.
