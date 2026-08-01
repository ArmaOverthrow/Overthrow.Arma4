# Autotest Foundation - Task Checklist

**Last Updated:** 2026-08-02 00:40
**Progress:** 22/22 tasks complete (100%) ✅
**Epic:** dev-ops (feature #2 of 5)
**Advanced phases:** Phase 2 (ADVANCED opus + requires Bash-capable agent). Phases 1/3/4 STANDARD (high); Phase 3 also requires a Bash-capable agent.

---

## Phase 1: Framework integration + the two test classes (6/6 complete) ✅ — STANDARD (high)

- [x] ✅ **1.1 `OVT_AutotestFramework.c` — modded `SCR_AutotestHelper`**
  - Description: `modded class SCR_AutotestHelper` mirroring BI's `SCR_AutotestFramework.c` signature-for-signature: `WORLD_OVT_TEST` (`{D87EF7EED4210569}Worlds/MP/OVT_Campaign_Test.ent`), `ADDONS_OVT` (`58D0FB3206B6F859,59B657D731E2A11D`), `GetDefaultWorld()`, `GetDefaultSystemsConfig()`, `GetDefaultLaunchParams()` (profile `OverthrowCI`), `RequestScenarioChangeTransition()` with the addon list
  - File(s): `Scripts/Game/Tests/TestFramework/OVT_AutotestFramework.c`
  - Estimate: 🟢 Small

- [x] ✅ **1.2 `OVT_TEST_SuiteBase.c` — the single inheritance point**
  - Description: `class OVT_TEST_SuiteBase : SCR_AutotestSuiteBase`, deliberately near-empty; do NOT override `GetWorldFile()`/`GetWorldSystemsConfigFile()` (inherited impls route through the modded helper). Header documents its job.
  - File(s): `Scripts/Game/Tests/TestFramework/OVT_TEST_SuiteBase.c`
  - Estimate: 🟢 Small

- [x] ✅ **1.3 Smoke suite + always-green case**
  - Description: `OVT_TEST_SmokeSuite` + `[Test(suite: OVT_TEST_SmokeSuite, timeoutS: 30)] OVT_TEST_Smoke_HarnessRuns` exercising Setup/Main(bool, multi-tick)/TearDown; log-only diagnostics for game-mode/manager presence (never assertions — comment says so)
  - File(s): `Scripts/Game/Tests/TestSuites/Smoke/OVT_TEST_SmokeSuite.c`
  - Estimate: 🟢 Small

- [x] ✅ **1.4 Meta suite + always-failing case**
  - Description: `OVT_TEST_MetaSuite` + `OVT_TEST_Meta_AlwaysFails` calling `SetResultFailure(...)`; capitalised header warning: never part of a default/CI run, exists to prove the red path
  - File(s): `Scripts/Game/Tests/TestSuites/Meta/OVT_TEST_MetaSuite.c`
  - Estimate: 🟢 Small

- [x] ✅ **1.5 Guard sweep**
  - Description: confirm no `#ifdef WORKBENCH` and no ternaries anywhere in `Scripts/Game/Tests/`
  - File(s): `Scripts/Game/Tests/**`
  - Estimate: 🟢 Small

- [x] ✅ **1.6 Compile clean**
  - Description: `tools/compile-check.sh` exit 0; if `override static` on the already-modded class fails, apply R1's fallback (world selection into suite base virtuals) — do not improvise
  - File(s): n/a (verification)
  - Estimate: 🟢 Small

---

## Phase 2: Prove the loop — empirical (10/10 complete) ✅ — **ADVANCED (opus), requires Bash-capable agent**

- [x] ✅ **2.1 Create `findings.md`** (same table shape as feature #1; Reforger build stamped at top)
  - File(s): `docs/features/dev-ops/autotest-foundation/findings.md`
  - Estimate: 🟢 Small

- [x] ✅ **2.2 Suite-class form** — `launch-game.sh -- -autotest OVT_TEST_SmokeSuite`; record `CLI autotest suite:` line, world transition, verbatim `junit.xml`, wall time
  - Estimate: 🟡 Medium

- [x] ✅ **2.3 Case-class form** — `-autotest OVT_TEST_Smoke_HarnessRuns`; expect `CLI autotest case:` + one-case `junit.xml`
  - Estimate: 🟢 Small

- [x] ✅ **2.4 Red path** — `-autotest OVT_TEST_MetaSuite`; record verbatim failure `junit.xml` + `autotest_failed.log`
  - Estimate: 🟢 Small

- [x] ✅ **2.5 Scoping / no-leak check** — BI's empty group `{6AB9C8EEE9A651B5}` still yields empty `<testsuites>`
  - Estimate: 🟢 Small

- [x] ✅ **2.6 Inertness check** — no `-autotest`: no `SCR_TestRunner`, no transition, no `junit.xml`, no new console errors vs baseline
  - Estimate: 🟡 Medium

- [x] ✅ **2.7 Invalid-target behaviour** — `-autotest OVT_TEST_DoesNotExist`; record failure mode for Phase 3's exit-2 classification
  - Estimate: 🟢 Small

- [x] ✅ **2.8 Timing + timeout proposal** — wall times from 2.2-2.4; propose `run-tests.sh` default timeout; note unbounded `Setup_AwaitWorld` as the reason
  - Estimate: 🟢 Small

- [x] ✅ **2.9 Conditional fallback** — NOT NEEDED (both class forms worked first try). Was: ONLY if 2.2/2.3 fail: hand-authored `Configs/Tests/OVT_TestGroup.conf` + `.meta` per Decision 4
  - File(s): `Configs/Tests/OVT_TestGroup.conf`, `Configs/Tests/OVT_TestGroup.conf.meta`
  - Estimate: 🟡 Medium (only if needed)

- [x] ✅ **2.10 Write-up** — "Differs from assumptions" + "Framework gaps" sections in `findings.md`
  - Estimate: 🟢 Small

> **GATE: ✅ APPLIED 2026-08-01 23:55.** All three -autotest forms work; class names stay the contract. Amendments written into Phases 3-4: verdict counts <failure>/<error> ELEMENTS (no failures= attributes on this build); invalid target = rc 0 + no junit.xml (missing-artifact check is load-bearing); default --timeout 300s; test world DOES load OVT_OverthrowGameMode via default.layer (corrects Decision 7/R6 — docs must state the corrected fact).

---

## Phase 3: `tools/run-tests.sh` + command contract (2/2 complete) ✅ — STANDARD (high), requires Bash-capable agent

- [x] ✅ **3.1-3.7 `tools/run-tests.sh` + `tools/README.md` section**
  - Description: `run-tests.sh [--timeout <s>] [--keep-artifacts] [--verbose] [<target>]` (default `OVT_TEST_SmokeSuite`); launches ONLY via `launch-game.sh`; deletes `.tmp/run-tests/*` pre-launch; collects `junit.xml`/`autotest.log`/`autotest_failed.log`/`console.log` (+`crash.log`); four-condition verdict → exit 0/1/2/124; summary on stderr, failing test names on stdout; `--timeout` passthrough; README section (synopsis, flags, exit codes, artifacts, target forms, "no run-everything form", framework gaps)
  - File(s): `tools/run-tests.sh`, `tools/README.md`
  - Estimate: 🔴 Large

- [x] ✅ **3.8 Verify by execution** (actual: 0/0/1/0/2/124 — all as expected; no orphans)
  - Description: default → 0; `OVT_TEST_MetaSuite` → 1; bogus target → 2; `--timeout 5` → 124 with no orphan (`tasklist.exe`)
  - File(s): n/a (verification)
  - Estimate: 🟡 Medium

---

## Phase 4: Documentation — Definition of Done (4/4 complete) ✅ — STANDARD (high)

- [x] ✅ **4.1-4.2 `CLAUDE.md` + technical-design §2**
  - Description: replace "No unit tests" constraint with real capability (state single-smoke-test reality; keep "No debugger"); strike through §2's "No unit or integration tests" per section convention
  - File(s): `CLAUDE.md`, `docs/technical-design.md`
  - Estimate: 🟢 Small

- [x] ✅ **4.3-4.4 technical-design §10 + §3**
  - Description: §10 replace "There is no test suite", add automated tests as a gate between compile check and play-testing; §3 add `Scripts/Game/Tests/` to the tree + Modded-convention deviation note
  - File(s): `docs/technical-design.md`
  - Estimate: 🟢 Small

- [x] ✅ **4.5-4.6 mission-statement + workbench-workflow skill**
  - Description: update "Automating the quality gate" paragraph; skill: constraints updated + full test-authoring section (layout, `OVT_TEST_` naming, `OVT_TEST_SuiteBase`, `[Test]`/`[Step]`, stages, void/bool steps, timeouts, `maxAttempts`, assertions, how to run one suite/case); link tools/README.md + findings.md
  - File(s): `docs/mission-statement.md`, `.claude/skills/workbench-workflow/SKILL.md`
  - Estimate: 🟡 Medium

- [x] ✅ **4.7-4.8 Epic rollup + stale-claim sweep**
  - Description: flip feature #2 status in `epic-overview.md`; `grep -ri "no unit test|no automated test|no test framework|no test suite"` repo-wide, fix or flag every hit (debugger claims untouched)
  - File(s): `docs/features/dev-ops/epic-overview.md`, various
  - Estimate: 🟢 Small

---

## Bugs & Issues

**Active Bugs:**
- (none)

**Fixed Bugs:**
- (none)

---

## Technical Debt

- (none yet)

---

## Progress Tracking

### Discovered New Tasks
- (none yet)

### Blocked Items
- (none)

---

*Update this file as tasks are completed. Mark tasks with ✅ immediately when done. Add new tasks as they're discovered.*
