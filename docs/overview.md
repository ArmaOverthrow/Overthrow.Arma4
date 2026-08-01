# Overthrow - Project Overview

**Last Updated:** 2026-08-02
**Version:** v1.2

## Project Summary

Overthrow is a dynamic, persistent revolution mod for Arma Reforger, built on EnforceScript and the Enfusion engine's entity-component architecture. Towns have populations, stability and support that shift over time; money, gear, vehicles, houses and recruits persist across sessions. Current engineering priority is the **dev-ops epic** — replacing the manual Workbench/play-testing quality gate with automated compile checks, an autotest suite and CI, built on Reforger 1.7.0's shipped tooling.

## Feature Status

| Feature | Status | Tasks | Notes |
|---------|--------|-------|-------|
| dev-ops (epic) | 🟡 In Progress (3/5 features) | 139/140 (features #1-#3; 1 optional deferred) | Automated compile, test and release pipeline built on Reforger 1.7.0's autotest framework and Workbench CLI — replaces the manual play-testing quality gate. #1 `workbench-automation` ✅; #2 `autotest-foundation` ✅; #3 `test-coverage` ✅ (30 assertions in four tiers, two group targets `{Fast}`/`{All}`, quarantined `core/persistence` acceptance gate, save-state tooling). Next: `ci-pipeline`, then `release-automation`. |
| core (epic) | 🟡 3/4 documented; persistence ⏸️ | 6/67 (persistence) | Overthrow's core systems. `game-mode`, `config`, `player-manager` 📄 documented retrospectively (2026-08-02, `/discover-feature` — legacy code, notable debt recorded in each `implementation.md`). `persistence` — EPF → vanilla migration, paused, superseded by dev-ops. ⚠️ Its Phase 1 "foundation" targeted a nonexistent API and never compiled — must be re-done against the real vanilla API on resume. Gate: `OVT_TEST_PersistenceRoundTripSuite` exit 1 → 0. |

> Epics are tracked as a single row; per-feature detail lives in each epic's `epic-overview.md` (`docs/features/dev-ops/`, `docs/features/core/`).

## Integration Points

- **dev-ops → core/persistence:** the epic's future `test-coverage` feature writes behaviour-level persistence tests (no EPF/vanilla API in assertions) that become core/persistence's acceptance gate on resume. The compile check already guards its API assumptions (~5s feedback).
- **dev-ops feature #1 → #2/#4/#5:** `tools/README.md` is the stable contract (commands, exit codes 0/1/2/124, `KEY=value` launcher output, `OVERTHROW_*` env overrides). Sibling features consume it without reading script source.

## Changelog

- v1.2 (2026-08-02): **dev-ops/test-coverage complete (61/62; 1 optional deferred)** — Overthrow's first real coverage: 30 behaviour-level assertions across four setup-cost tiers (Logic 14 world-free ~8s, Init 4, Campaign 4, Persistence 8 same-session), every case with a recorded can-fail proof, no `maxAttempts` anywhere. Two stable CI targets shipped as `SCR_AutotestGroup` configs: Fast `{6A6E29FF47ECB840}` (18 cases ~16s) and All `{6A6E2A002F53A581}` (30 cases ~19s). The quarantined `OVT_TEST_PersistenceRoundTripSuite` (9 cases, red by design) is now `core/persistence`'s machine-checkable acceptance gate (exit 1 → 0 = migration done). Save-state tooling made automation-safe (`.scripts/` argument forms, `--profile`, destructive-path guard). 11 gameplay/content bugs found and filed (BUG-001…011, incl. FillAmmoboxes 62 VM exceptions/campaign start and the unconditional save-success UI); the plan's suspected integer-division defects proven not to exist. Docs corrected repo-wide (technical-design §10, mission-statement, workbench-workflow skill v1.3.0, CLAUDE.md). **dev-ops/autotest-foundation complete (22/22)** — Overthrow's first automated test loop: `tools/run-tests.sh` (launch → `junit.xml` → honest 0/1/2/124 verdict, ~15s/run) on the shipped `SCR_Autotest` framework. `Scripts/Game/Tests/` tree: modded `SCR_AutotestHelper`, `OVT_TEST_SuiteBase`, one smoke suite + a kept always-red `OVT_TEST_MetaSuite` proving the failure path. All three `-autotest` forms (suite/case/`{GUID}`) verified in the retail client; test code proven inert without `-autotest`. Coverage is deliberately one smoke test — real coverage is feature #3. Docs corrected (CLAUDE.md, technical-design, mission-statement, workbench-workflow skill v1.2.0, agent definitions).
- v1.0 (2026-08-01): **dev-ops/workbench-automation complete (56/56)** — First automated compile check for Overthrow: `tools/compile-check.sh` (Workbench `-wbsilent -validate`, honest 0/1/2/124 exit codes, gcc-style `file:line:` errors, false-pass guard) and `tools/launch-game.sh` (game-client launcher; proved the `-autotest` → `junit.xml` loop end-to-end). Empirical CLI reference in `findings.md`. Also fixed the tree to compile on Reforger 1.7.0.54 (vanilla-persistence WIP had never compiled: illegal generic method, fictional persistence API, template files in the compiled tree). Docs corrected across CLAUDE.md, technical-design, mission-statement, workbench-workflow skill and agent definitions. Created this master overview.
