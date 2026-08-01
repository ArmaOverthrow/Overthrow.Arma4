---
name: workbench-workflow
description: Arma Reforger Workbench workflow, testing guidelines, and debugging patterns
version: 1.2.0
---

# Workbench Workflow

Quick reference for working with Arma Reforger Workbench. For detailed patterns, see resource files below.

---

## When to Use This Skill

Use this skill when:
- Testing changes in Workbench
- Debugging compile errors
- Understanding Workbench limitations
- Planning manual testing procedures
- Working with prefabs, configs, or layouts
- Troubleshooting runtime issues

---

## Quick Reference

### Testing Guidelines
Two automated gates you run yourself: `tools/compile-check.sh` (compiles all EnforceScript, ~5s) and `tools/run-tests.sh` (runs Overthrow's autotests in the real game client, ~15s). **Coverage is currently a single smoke test** that proves the harness runs and asserts nothing about Overthrow — real coverage is the dev-ops epic's feature #3. So runtime behaviour is still verified by manual Workbench play-testing; be specific about what the user should test and how.

**See:** `testing-guidelines.md` for manual test procedures · "Writing Autotests" below for automated ones

### Compile Errors
EnforceScript has specific error patterns. Common issues: missing semicolons, ternary operators, type mismatches, missing strong refs. `tools/compile-check.sh` surfaces them as `file:line: message` on stdout; the same errors also show in the Workbench console during interactive sessions.

**See:** `compile-errors.md` for common errors and fixes

### Debug Patterns
Use Print() for debug output, check Workbench console logs. No interactive debugger. Add debug prints strategically to trace execution and inspect values.

**See:** `debug-patterns.md` for debugging techniques

### Workbench Tips
Prefabs edited in Workbench, layouts in UI editor, configs in text editor. Save often. Workbench can crash. Always test changes in play mode.

**See:** `workbench-tips.md` for Workbench best practices

---

## Critical Constraints

- ✅ **Automated compile check** - Run `tools/compile-check.sh` after code changes; do not ask the user to compile for you (see `tools/README.md`)
- ✅ **Automated tests** - Run `tools/run-tests.sh` after code changes that touch tested areas (see `tools/README.md`)
- ⚠️ **Coverage is one smoke test** - Everything else is still manual play-testing; feature #3 adds real assertions
- ❌ **No interactive debugger** - Use Print() for debug output
- ✅ **Be specific** - Tell user exactly what to test
- ✅ **Test incrementally** - Small changes, compile-check often
- ⚠️ **Workbench can crash** - Save frequently
- ✅ **Check console** - Runtime errors show in Workbench console
- ⚠️ **Play mode testing** - Always test in play mode, not just edit mode

---

## Workflow Pattern

### Development Cycle

1. **Write code** in Claude Code
2. **Run `tools/compile-check.sh`** yourself (~5s warm)
3. **Fix errors** from the parsed `file:line: message` output
4. **Repeat** until exit 0
5. **Run `tools/run-tests.sh`** if an autotest suite covers the area you touched (today: only the smoke suite)
6. **User tests** in Workbench play mode
7. **User reports** bugs/runtime issues (debug prints, console errors)
8. **Fix issues**
9. **Repeat** until working

### Testing Cycle

1. **Automated where possible** - If the behaviour is assertable from a test world, write or extend a suite (see "Writing Autotests") and prove it with `tools/run-tests.sh`
2. **Define a manual test procedure** for everything the suites do not cover — which is currently almost everything
3. **User follows procedure** in Workbench play mode
4. **User reports results** - What worked, what didn't
5. **Fix issues** if needed
6. **Retest** until feature works

---

## Common Workflow

### After Code Changes

Run the compile check yourself — do not ask the user to compile:

```bash
tools/compile-check.sh
```

Once it exits 0, hand off runtime testing to the user:
```
Please test in Workbench:
1. Enter play mode
2. Test: [specific steps]
3. Report: [specific things to check]
```

### After Compile Errors

Read the compile check's stdout — each line is `file:line: message`. Fix the errors and re-run until exit 0. Do not ask the user for compile errors; the check is the source of truth. (Syntax errors abort at the first failing file, so fixing one may reveal more — just re-run.)

### After Runtime Errors

Runtime errors and debug prints still come from the user. Ask:
```
Please:
1. Check the Workbench console for any error messages
2. Try: [specific test step]
3. Report: What happened vs what should happen
```

---

## Running the Compile Check

```bash
tools/compile-check.sh          # compile all EnforceScript headlessly (~5s warm)
tools/compile-check.sh --help   # all flags and exit codes
```

**Exit codes:** 0 = verified clean · 1 = compile errors · 2 = indeterminate/tool failure (never means pass or fail) · 124 = timeout.

**Output:** errors on stdout, one per line, gcc-style and repo-relative:
```
Scripts/Game/Components/OVT_Foo.c:214: error: Broken expression (missing ';'?)
```
Summary on stderr; full engine log kept at `.tmp/compile-check/last.log`. Full contract (flags, env vars, limitations): `tools/README.md`.

Also available: `tools/launch-game.sh` launches the game client with Overthrow loaded and reports the run's log directory. `tools/run-tests.sh` is built on it — prefer `run-tests.sh` and do not drive the launcher directly for test runs.

---

## Running the Autotests

```bash
tools/run-tests.sh                            # default target: OVT_TEST_SmokeSuite
tools/run-tests.sh OVT_TEST_SmokeSuite        # one suite
tools/run-tests.sh OVT_TEST_Smoke_HarnessRuns # one case (runs inside its owning suite)
tools/run-tests.sh OVT_TEST_MetaSuite         # red-path proof — MUST exit 1
tools/run-tests.sh --help                     # all flags
```

**Exit codes:** 0 = all passed (positively verified from `junit.xml`) · 1 = test failures (names on stdout) · 2 = indeterminate/tool failure — *including a mistyped target*, which produces no `junit.xml` · 124 = timeout (default 300 s).

~15 s per run (full client boot + two test-world loads). Artifacts land in `.tmp/run-tests/`: `junit.xml`, `autotest.log`, `autotest_failed.log`, `console.log` (+ `crash.log` if the client crashed). **There is no "run everything" form** — one target per launch until someone authors an `SCR_AutotestGroup` config, which becomes necessary at the second suite.

Full contract: `tools/README.md`. Empirical ground truth (verbatim artifact shapes, timings, framework gaps, valid for Reforger 1.7.0.54): `docs/features/dev-ops/autotest-foundation/findings.md`.

---

## Writing Autotests

**Where files go**

```
Scripts/Game/Tests/
├── TestFramework/    glue — modded SCR_AutotestHelper (world + addon list), OVT_TEST_SuiteBase
└── TestSuites/<Area>/OVT_TEST_<Area>Suite.c    one file per area: the suite + its cases
```

**Naming:** suites `OVT_TEST_<Area>Suite`; cases `OVT_TEST_<Area>_<Subject>_<ExpectedBehaviour>`. The class name *is* the CLI argument, so keep it greppable.

**Skeleton**

```cpp
//! Suites inherit OVT_TEST_SuiteBase — never SCR_AutotestSuiteBase directly.
//! The world comes from the modded helper; do NOT override GetWorldFile().
class OVT_TEST_EconomySuite : OVT_TEST_SuiteBase {}

[Test(suite: OVT_TEST_EconomySuite, timeoutS: 30)]
class OVT_TEST_Economy_Prices_AreInitialised : SCR_AutotestCaseBase
{
    protected int m_iTicks;

    [Step(EStage.Setup)]
    void Setup()
    {
        Print("setting up");   // shadowed Print -> routes to autotest.log
    }

    [Step(EStage.Main)]
    bool Main()               // bool step: re-run EVERY tick until it returns true
    {
        m_iTicks++;
        if (m_iTicks < 5) return false;   // wait for managers to settle

        OVT_EconomyManagerComponent economy = OVT_Global.GetEconomy();
        if (!AssertTrue(economy != null, "economy manager missing")) return true;

        SetResultSuccess();
        return true;
    }

    [Step(EStage.TearDown)]
    void TearDown() {}
}
```

**Rules that bite**

- **Stages run Setup → Main → TearDown**, methods within a stage in definition order. A Setup failure **skips** TearDown; a Main failure still runs it.
- **`void` steps run once. `bool` steps are re-run every tick until they return `true`.** Returning `false` forever = a hang bounded only by the step timeout.
- **Timeouts** reset per step method: `[Test(timeoutS: N)]` / `[TestStep(stage, timeoutS: N)]`. `[Step(EStage.X)]` is the short alias of `[TestStep]`.
- **`maxAttempts:` on `[Test]`** re-runs a flaky test. It exists; prefer not to use it — a test that needs retries is usually a bug in the test.
- **Verdict API:** `AssertTrue(cond, msg)` (returns the bool, records a failure), `SetResultSuccess()`, `SetResultFailure("why")`. The failure string appears verbatim in `junit.xml` and `autotest.log`.
- **Use the case/suite-shadowed `Print` / `PrintFormat`**, not the global ones — the shadowed versions route through the autotest printer into `autotest.log`. Global `Print` only reaches `console.log`.
- **Never `#ifdef WORKBENCH` a test class.** Guarded tests do not exist in the retail client, which is what CI runs. Test code is inert without `-autotest` at runtime, which is the real safety guarantee.
- **Test world:** `Worlds/MP/OVT_Campaign_Test.ent` loads `OVT_OverthrowGameMode` plus managers via its `default.layer`, so `OVT_Global` accessors work. But the **campaign is not started** — anything needing running-campaign state (economy simulation, occupying faction) must start it explicitly in Setup.
- Compile tests like any other code: `tools/compile-check.sh` first, then `tools/run-tests.sh <YourSuite>`.

---

## Resource Files

Detailed documentation organized by concern:

1. **testing-guidelines.md** - Manual test procedures for different component types
2. **compile-errors.md** - Common EnforceScript compile errors and solutions
3. **debug-patterns.md** - Using Print(), console logs, debugging techniques
4. **workbench-tips.md** - Working with prefabs, configs, layouts, best practices

---

## Key Differences from Other Dev Environments

- **No npm/build scripts** - `tools/compile-check.sh` is the build-verification equivalent (compile only, no artifacts)
- **Test framework exists, coverage does not** - `tools/run-tests.sh` runs Reforger's shipped autotest framework against Overthrow, but there is one smoke test so far (feature #3 adds real coverage). Everything else is manual play-testing
- **Tests run in the real game client** - No mocks, no headless runner: each run boots the client and loads a world (~15s)
- **No debugger** - Print-based debugging
- **No hot reload** - Restart play mode to test changes
- **No CI/CD yet** - Compile check runs locally; dev-ops epic feature #4 will orchestrate it
- **User is QA** - User tests all runtime behaviour manually

---

**Pattern:** Start here for quick reference, dive into resource files for detailed procedures.
