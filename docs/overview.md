# Overthrow - Project Overview

**Last Updated:** 2026-08-01
**Version:** v1.0

## Project Summary

Overthrow is a dynamic, persistent revolution mod for Arma Reforger, built on EnforceScript and the Enfusion engine's entity-component architecture. Towns have populations, stability and support that shift over time; money, gear, vehicles, houses and recruits persist across sessions. Current engineering priority is the **dev-ops epic** — replacing the manual Workbench/play-testing quality gate with automated compile checks, an autotest suite and CI, built on Reforger 1.7.0's shipped tooling.

## Feature Status

| Feature | Status | Tasks | Notes |
|---------|--------|-------|-------|
| dev-ops (epic) | 🟡 In Progress (1/5 features) | 56/56 (feature #1) | Automated compile, test and release pipeline built on Reforger 1.7.0's autotest framework and Workbench CLI — replaces the manual play-testing quality gate. Feature #1 `workbench-automation` ✅ complete: `tools/compile-check.sh` + `tools/launch-game.sh` + `tools/README.md` contract. Next: `autotest-foundation`. |
| vanilla-persistence | ⏸️ Paused | 6/67 (9%) | Migration from EPF to vanilla persistence. Superseded in priority by the dev-ops epic. ⚠️ Its Phase 1 "foundation" was found (2026-08-01) to target a nonexistent API and never compiled — must be re-done against the real vanilla API on resume (see its context.md). |

> Epics are tracked as a single row; per-feature detail lives in `docs/features/dev-ops/epic-overview.md`.

## Integration Points

- **dev-ops → vanilla-persistence:** the epic's future `test-coverage` feature writes behaviour-level persistence tests (no EPF/vanilla API in assertions) that become vanilla-persistence's acceptance gate on resume. The compile check already guards its API assumptions (~5s feedback).
- **dev-ops feature #1 → #2/#4/#5:** `tools/README.md` is the stable contract (commands, exit codes 0/1/2/124, `KEY=value` launcher output, `OVERTHROW_*` env overrides). Sibling features consume it without reading script source.

## Changelog

- v1.0 (2026-08-01): **dev-ops/workbench-automation complete (56/56)** — First automated compile check for Overthrow: `tools/compile-check.sh` (Workbench `-wbsilent -validate`, honest 0/1/2/124 exit codes, gcc-style `file:line:` errors, false-pass guard) and `tools/launch-game.sh` (game-client launcher; proved the `-autotest` → `junit.xml` loop end-to-end). Empirical CLI reference in `findings.md`. Also fixed the tree to compile on Reforger 1.7.0.54 (vanilla-persistence WIP had never compiled: illegal generic method, fictional persistence API, template files in the compiled tree). Docs corrected across CLAUDE.md, technical-design, mission-statement, workbench-workflow skill and agent definitions. Created this master overview.
