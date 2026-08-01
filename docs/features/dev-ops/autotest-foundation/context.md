# Autotest Foundation - Context & Decisions

**Last Updated:** 2026-08-02 00:45
**Current Phase:** Complete (all 4 phases)
**Status:** ✅ Ready for Review
**Epic:** dev-ops (feature #2 of 5 — depends on #1 workbench-automation ✅; feeds #3 test-coverage and #4 ci-pipeline)

---

## Quick Status

**What's Done:**
- ✅ Plan created (implementation.md — 4 phases / 22 tasks, architect read the shipped framework source first)
- ✅ Dev docs scaffolded
- ✅ Phase 1 (6/6): four test-tree files created, compile-check exit 0 (8s), no ifdefs/ternaries. BI signatures matched verbatim (incl. dropped `protected` on `RequestScenarioChangeTransition`); `PrintFormat` on case base takes string params only (`.ToString()` on bools). World `{D87EF7EED4210569}` + GUID verified present.

- ✅ Phase 2 (10/10): loop PROVEN — all three `-autotest` forms work first try (suite/case/`{GUID}`); red path reports `<failure type="Result">`; inertness confirmed; ADDONS_OVT unchanged (EPF/EDF resolve transitively); findings.md (224 lines) is the record. GATE applied to Phases 3-4.

- ✅ Phase 3 (2/2): `tools/run-tests.sh` shipped + README section; verified by execution 0/0/1/0/2/124 all as expected. Five-condition verdict (launcher done + block-scoped addon proof + junit parseable + ≥1 testcase + zero failure/error ELEMENTS); awk parser (no python3); `--keep-artifacts`; `OVERTHROW_TEST_TIMEOUT` env (default 300s); artifacts under `.tmp/run-tests/`; none on 124.

- ✅ Phase 4 (4/4): all docs corrected (CLAUDE.md, technical-design preamble/§2/§3/§10, mission-statement, workbench-workflow skill v1.2.0 with Running/Writing Autotests sections, 3 agent definitions, epic rollup); stale-claim sweep done (historical records + debugger claims deliberately untouched)
- ✅ Final cross-phase gate: compile-check exit 0 (6s) + run-tests.sh exit 0 (16s, 4 artifacts)

**What's Next (for the epic, not this feature):**
- 📋 Feature #3 `test-coverage` (can run ∥ #4): inherits `OVT_TEST_SuiteBase`, the `OVT_TEST_` naming, findings.md's world facts (game mode present, campaign NOT auto-started); needs an `SCR_AutotestGroup` config at its second suite
- 📋 Feature #4 `ci-pipeline`: orchestrates `compile-check.sh` + `run-tests.sh` (same 0/1/2/124 taxonomy, artifacts `.tmp/run-tests/`)
- 📋 Changes are uncommitted on `vanilla-persistence` — user reviews & commits

**Blockers:**
- None

---

## Key Files

### Deliverables (to be created)
- `Scripts/Game/Tests/TestFramework/OVT_AutotestFramework.c` - modded `SCR_AutotestHelper` (project defaults; the entire integration point)
- `Scripts/Game/Tests/TestFramework/OVT_TEST_SuiteBase.c` - the single inheritance point for all Overthrow suites
- `Scripts/Game/Tests/TestSuites/Smoke/OVT_TEST_SmokeSuite.c` - always-green loop proof + log-only diagnostics
- `Scripts/Game/Tests/TestSuites/Meta/OVT_TEST_MetaSuite.c` - always-red suite (never in a default run)
- `tools/run-tests.sh` - launch + collect + honest verdict (0/1/2/124)
- `docs/features/dev-ops/autotest-foundation/findings.md` - Phase 2 empirical record

### Related Files
- `docs/features/dev-ops/autotest-foundation/implementation.md` - the plan (read Verified Ground Truth + Decisions 1-8 before touching anything)
- `docs/features/dev-ops/workbench-automation/findings.md` - feature #1's CLI ground truth (client exit code always 0; cwd=game dir; `-autotest "{GUID}"` proven)
- `tools/launch-game.sh` + `tools/lib/common.sh` + `tools/README.md` - feature #1's process boundary (consume, never reimplement)
- `/mnt/n/Projects/Arma 4/ArmaReforger/scripts/Autotest/Game/TestFramework/` - the framework API surface
- `/mnt/n/Projects/Arma 4/ArmaReforger/scripts/Game/Tests/TestFramework/SCR_AutotestFramework.c` - the override pattern being copied

---

## Important Decisions

(8 decisions live in implementation.md — the load-bearing ones:)

### Decision 1: Layout mirrors BI under `Scripts/Game/Tests/`; helper override lives there, NOT in `Modded/`
Whole test integration = one deletable directory. Naming `OVT_TEST_<Area>Suite` / `OVT_TEST_<Area>_<Subject>_<ExpectedBehaviour>`.

### Decision 2: Tests ship unguarded (no `#ifdef WORKBENCH`)
CI runs the retail client; BI's guarded example doesn't exist there (#1 proved it). Inertness is runtime (`SCR_AutotestRunnerCore.CanCreate()` false without `-autotest`), verified in task 2.6.

### Decision 3: Class names are the primary `-autotest` contract; no group config in this feature
There is NO "run everything" CLI form (default group is empty; `ConfigureTestSuites` disables unnamed suites) — feature #3 needs an `SCR_AutotestGroup` config at its second suite.

### Decision 4: If class forms fail → hand-authored `.conf` + `.meta` with generated GUID (16 uppercase hex, collision-checked), proven by execution (`CLI autotest config:` log line). GUI is the last resort.

### Decision 5: `tools/run-tests.sh` belongs to THIS feature
Client exit code is always 0 — something must turn `junit.xml` into a verdict, and that is test-contract knowledge (#2's remit, not #4's).

### Decision 6: The always-failing test is KEPT permanently, isolated in `OVT_TEST_MetaSuite` — standing red-path proof (`run-tests.sh OVT_TEST_MetaSuite` → exit 1 is a DoD criterion).

---

## Gotchas & Learnings

### 1. Framework gaps found by reading source (pre-implementation)
- **`Setup_AwaitWorld` has no timeout** (`SCR_AutotestSuiteBase.c:67-71`) — a world that never loads = infinite hang; `run-tests.sh`'s process timeout is mandatory, not advisory.
- **`OVT_Campaign_Test.ent` is a 3-line SubScene of `MpTest_Basic.ent` — no Overthrow game mode.** Fine for #2 (fast, nothing to init); real constraint for #3. Smoke test LOGS (never asserts) game-mode/manager presence.
- **`addonList` is comma-separated bare GUIDs** (no braces): `58D0FB3206B6F859,59B657D731E2A11D` (+ EPF `5D6EBC81EB1842EF`, EDF `5D6EA74A94173EDF` if transitive resolution doesn't hold).
- **`[Test]` classes are auto-registered at engine startup in every client** (`TestHarness.GetNSuites()` obsolete note) — inert but not free; task 2.6 verifies behaviour-neutrality vs baseline.
- **`OVT_TEST_SuiteBase` must NOT override `GetWorldFile()`** — inherited impl already routes to the modded helper; also keeps the cross-module method-sealing hazard (R1, `SCR_Hack_AutotestSuiteBase`) off the critical path.
- Suite/case `Print`/`PrintFormat` are shadowed to route into `autotest.log` — test code should use them, not global `Print`.

---

## Testing Approach

No bash test framework — manual checklist T1-T13 in implementation.md §Testing Strategy. Key: T6/T7 (missing/empty junit.xml → exit 2 never 0), T9/T10 (shipping inertness vs baseline).

---

## Next Steps

### Immediate
1. Phase 1 tasks 1.1-1.6 (standard agent; compile-check until exit 0)
2. Phase 2 empirical proof (ADVANCED full-toolset agent — project's *-advanced agents have no Bash)
3. GATE: amend Phases 3-4 against findings.md

### Future (After This Phase)
1. Phase 3: run-tests.sh + README contract
2. Phase 4: docs DoD (CLAUDE.md, technical-design §2/§3/§10, mission-statement, workbench-workflow skill, epic rollup)

---

## Open Questions

- [ ] **Q:** Do the class-name `-autotest` forms work in the retail client? (Only `{GUID}` proven; BI's class examples were `#ifdef WORKBENCH`-guarded so their failure proved nothing about the forms themselves.) → Phase 2 tasks 2.2/2.3; fallback Decision 4.
- [ ] **Q:** Does the scenario transition keep EPF/EDF loaded transitively, or must they be in `ADDONS_OVT`? → Phase 2 (R2 mitigation order: base+OVT → +EPF+EDF → derived list).

---

## Session Notes

### 2026-08-02 00:45 — Phases 3+4 complete, feature done ✅
- Phase 3: `tools/run-tests.sh` (awk junit parser, five-condition verdict incl. block-scoped addon proof; `--keep-artifacts`; `OVERTHROW_TEST_TIMEOUT` default 300s) + full README section. Verified live: 0/0/1/0/2/124 all as expected, no orphans. Agent found+fixed one parser bug offline (bare `name="` matched inside `classname="`) before any launch.
- Phase 4: docs DoD across CLAUDE.md (gitignored — won't show in git status), technical-design, mission-statement, skill v1.2.0, 3 agent definitions, epic-overview (also corrected its Purpose + doc-policy lines and superseded the GUID-only Research Basis note). `CLAUDE.md.example` left stale deliberately (frozen pre-epic snapshot, already wrong about compile too) — recommend regenerate-or-delete as a separate decision.
- Final sweep: compile-check exit 0, run-tests.sh exit 0 (16s), tree state = 8 modified + 6 new (all uncommitted; user commits).
- Needs human verification: none blocking — optionally eyeball the agent-definition/skill edits before committing (they steer future agents).

### 2026-08-01 23:55 — Phases 1+2 complete, gate applied
- Phase 1: four files, compile-check exit 0 (8s). Phase 2 (advanced full-toolset agent): all `-autotest` forms proven; Decision 4's .conf fallback never fired.
- Phase 3-relevant corrections (amended into the plan): junit.xml `<testsuite>` has NO `failures=`/`errors=` attributes on this build → count elements; invalid target = launcher rc 0 + client exit 0 + NO junit.xml → missing-artifact check is the only detector; default timeout 300s (worst green 22s); empty `<testsuites>` (BI empty group) = exit 2 shape captured.
- Corrects Decision 7/R6: `OVT_Campaign_Test.ent` DOES load `OVT_OverthrowGameMode` + managers via `OVT_Campaign_Test_Layers/default.layer` (campaign not auto-started; pre-existing `Failed to get SCR_PersistenceSystem instance!` per world load; navmesh missing). Feature #3 inherits this from findings.md.
- Harness quirk: runs twice per launch (menu + post-transition, 3 world loads/run) — doubled log lines are normal, not a bug.

### 2026-08-01 22:58
- Feature started via `/autorun-feature dev-ops/autotest-foundation` (autonomous run from Discord)
- Plan written by solution-architect after reading the shipped framework source; scaffolded context.md + tasks.md (22 tasks / 4 phases)
- Next: Phase 1 with standard-tier component developer agent

---

*Update this file at the end of each work session. Run `/dev-docs-update` before compacting conversations.*
