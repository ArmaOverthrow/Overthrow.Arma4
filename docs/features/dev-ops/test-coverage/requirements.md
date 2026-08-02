# Test Coverage — Requirements

**Epic:** dev-ops
**Created:** 2026-08-01

## Overview

Writes Overthrow's first real test suites on top of the foundation from feature #2. Targets the three areas where regressions actually happen and where automation can genuinely help: **persistence round-trips**, **campaign logic** (economy, towns, jobs, skills), and **manager/controller initialisation**.

The persistence suites carry extra weight. They are written at the **behaviour** level — no EPF and no vanilla-persistence API in any assertion — so they survive the EPF → vanilla migration and become its acceptance gate. This is what reframes the paused `core/persistence` feature from "unverifiable big-bang rewrite" into "a migration with a test suite waiting to validate it."

## Requirements

### Persistence round-trip (highest priority)

- **Assertions are behaviour-level only.** "Town control survives a save/reload cycle" — never "`OVT_TownSaveData` serialized N fields" and never a call into `EPF_*` or a vanilla persistence type. If the migration would break the test, the test is written wrong.
- Cover the state that matters to players: **town control/stability/population, player money and skills, real estate ownership, vehicles, container and player inventories, loadouts, recruits, placed structures.**
- Each suite establishes state → triggers a save → reloads → asserts the state matches. The multi-tick `bool`-returning step pattern is what makes the async save/load waits expressible.
- These suites must pass against the **current EPF implementation** before the migration, so a failure afterwards is unambiguously a migration regression.

### Campaign logic

- Cover the deterministic, high-value systems: **economy/pricing, town stability and support maths, job selection, skill effects, town modifiers.**
- Prefer tests that need no world load where the logic allows it — they are dramatically faster and belong in a separate fast suite.

### Initialisation / integration

- Boot `OVT_Campaign_Test.ent` with the real game mode and assert the managers initialise: all managers reachable via `OVT_Global`, towns populated, controllers registered.
- This is the cheapest possible guard against the "everything is null on startup" class of breakage.

### Cross-cutting

- **Suites are organised so fast tests can run without a world load** and slow integration tests can be run separately. CI (feature #4) needs to be able to run a quick subset on every push and the full set less often.
- **Every test must be able to fail.** Any test that cannot be made to go red by breaking the thing it covers is not providing value — verify this deliberately when writing each suite.
- **No flaky tests are accepted into the suite.** Use `maxAttempts` only where genuine engine-timing non-determinism is proven, never to paper over a real race.
- **Test code must not alter shipped runtime behaviour.**
- Follow the patterns and naming (`OVT_TEST_*`) established by feature #2 rather than inventing new ones.
- **Save-state management scripts exist and must be documented and used** (user directive, 2026-08-02): `.scripts/reset_save.sh` deletes the active Overthrow save DB (fresh-campaign state), `.scripts/backup_save.sh` archives it to `.saves/`, `.scripts/activate_save.sh` restores an archive. Tests that require a fresh campaign (or a known saved state) use these as needed. Caveats to resolve when adopting them: all three default to the **Workbench** profile save dir (`ArmaReforgerWorkbench/profile/.db/Overthrow`) and honour `OVERTHROW_SAVE_DIR` — test runs use the game client's `OverthrowCI` profile, so the variable must point at that profile's `.db/Overthrow`; `backup_save.sh`/`activate_save.sh` are interactive (`read -p`) and need non-interactive argument forms before agents can call them.

## Definition of Done — documentation

- `docs/technical-design.md` §10 — replace the "three dimensions that break" guidance with what is now automated vs. still manual
- `docs/mission-statement.md` — "Play-testing as the quality gate" reflects the reduced (not eliminated) manual surface
- The `workbench-workflow` skill — testing guidelines
- `docs/features/core/persistence/` — record that behaviour-level persistence tests now exist and act as the migration's acceptance gate

## Dependencies

- **`dev-ops/autotest-foundation` must be complete** — needs the suite base, helper override, working `-autotest` loop and authoring patterns.
- Transitively depends on `dev-ops/workbench-automation`.
- **Can be built in parallel with `dev-ops/ci-pipeline`** — the pipeline does not care how many suites exist.
- Independent of `core/persistence`'s progress; deliberately written to outlive it.

## Out of Scope

- **Comprehensive coverage of the whole codebase.** This feature establishes patterns and covers the highest-risk systems. Broad coverage accrues over time as features are touched.
- **Multiplayer / JIP tests** — the project's most common regression class, but it needs two coordinated processes. Deferred to a future epic.
- **UI tests.** The UI layer is client-side and viewport-dependent; out of scope here.
- **Performance/FPS regression tests.**
- **Testing `modded class` overrides against future Reforger patches** — a real risk, but it needs a different mechanism (upgrade-time diffing) than a test suite.
- **Fixing any bugs the new tests uncover.** Findings get logged as separate work; this feature delivers the tests, not the fixes.
