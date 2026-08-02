---
name: workbench-workflow
description: Arma Reforger Workbench workflow, testing guidelines, and debugging patterns
version: 1.3.0
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
Two automated gates you run yourself: `tools/compile-check.sh` (compiles all EnforceScript, ~5s) and `tools/run-tests.sh` (runs Overthrow's autotests in the real game client, ~15-19s). **Coverage is a spine, not the surface** — 30 assertions across four tiers (pure logic, manager init, started campaign, same-session persistence), reachable as one command. **Not covered at all: JIP/multiplayer (the most common regression class — it needs two client processes), UI, performance, AI movement, and the save/reload round-trip (written but gated behind the persistence migration).** Everything in that second list is still verified by manual Workbench play-testing; be specific about what the user should test and how.

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
- ✅ **Automated tests** - Run `tools/run-tests.sh "{6A6E29FF47ECB840}"` (Fast) after code changes that touch tested areas (see `tools/README.md`)
- ⚠️ **Coverage is a spine, not the surface** - 30 assertions over logic, init, campaign and same-session persistence. **JIP/multiplayer, UI, performance and save/reload are NOT covered** and are still manual play-testing
- ⚠️ **Extend the suites when you can** - If a change is assertable in the test world, add a case to the right tier rather than writing only a manual procedure
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
5. **Run `tools/run-tests.sh "{6A6E29FF47ECB840}"`** (Fast, ~16s) — or the All group if you touched campaign or persistence state
6. **User tests** in Workbench play mode
7. **User reports** bugs/runtime issues (debug prints, console errors)
8. **Fix issues**
9. **Repeat** until working

### Testing Cycle

1. **Automated where possible** - If the behaviour is assertable from a test world, add a case to the right tier (see "Writing Autotests") and prove it with `tools/run-tests.sh`. Prove it can fail, too — perturb the thing it covers, see exit 1, revert
2. **Define a manual test procedure** for everything the suites do not cover — JIP/multiplayer, UI, performance, AI movement, and anything needing a real save/reload
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
tools/run-tests.sh "{6A6E29FF47ECB840}"       # Fast group  — Logic + Init, 18 cases, ~16 s
tools/run-tests.sh "{6A6E2A002F53A581}"       # All group   — + Campaign + Persistence, 30 cases, ~19 s
tools/run-tests.sh OVT_TEST_LogicSuite        # one suite (debugging)
tools/run-tests.sh OVT_TEST_Logic_Town_SupportPercentage_Boundaries   # one case, inside its owning suite
tools/run-tests.sh OVT_TEST_MetaSuite         # red-path proof — MUST exit 1
tools/run-tests.sh                            # default target: OVT_TEST_SmokeSuite
tools/run-tests.sh --help                     # all flags
```

**The two group GUIDs are a stable contract** — quote them verbatim (the braces need the quotes) and never read the `.conf` files. Run **Fast** after most changes; run **All** before handing work over or when you touched campaign, economy or persistence state. Suite execution order inside a group is alphabetical by class name, so no case may depend on another.

**Exit codes:** 0 = all passed (positively verified from `junit.xml`) · 1 = test failures (names on stdout) · 2 = indeterminate/tool failure — *including a mistyped target*, which produces no `junit.xml` · 124 = timeout (default 300 s).

Artifacts land in `.tmp/run-tests/`: `junit.xml`, `autotest.log`, `autotest_failed.log`, `console.log` (+ `crash.log` if the client crashed). Verdicts come from `junit.xml` only — a green run still prints some `SCRIPT (E)` lines from gameplay code, and a campaign start emits 62 known VM exceptions, so **never judge a run by console error counts**.

**Save-state precondition.** Campaign- and persistence-tier runs assume a fresh save DB:

```bash
.scripts/reset_save.sh --profile OverthrowCI   # NEVER without --profile
```

Without `--profile OverthrowCI` (or an explicit `OVERTHROW_SAVE_DIR`) these tools target the user's **real Workbench campaign save** — an `rm -rf` on hours of play. The guard refuses implausible paths and prints what it resolved, but do not rely on it. Today the reset changes no verdict (nothing writes a save on this branch); it is load-bearing for the acceptance gate below. All three save tools: `tools/README.md` → Save-state control.

**Persistence acceptance gate.** `OVT_TEST_PersistenceRoundTripSuite` is quarantined, in no group, and **red on purpose**: `tools/run-tests.sh OVT_TEST_PersistenceRoundTripSuite` exits 1 with `Persistence capability absent…`. Exit 0 means the `core/persistence` migration is complete. Do not "fix" it, do not add it to a group, do not weaken it.

Full contract: `tools/README.md`. Empirical ground truth (verbatim artifact shapes, timings, framework gaps, valid for Reforger 1.7.0.54): `docs/features/dev-ops/autotest-foundation/findings.md` and `docs/features/dev-ops/test-coverage/findings.md`.

---

## Writing Autotests

**Pick the tier first.** Suites are organised by **setup cost**, not by subject — the world transition and the campaign start are paid per *suite*, so coverage grows by adding a case **file** to an existing tier, never by adding a suite. Choose the cheapest tier that can express the assertion:

| Tier | Suite | Available to a case | Put a case here when it… |
|---|---|---|---|
| **A** Logic | `OVT_TEST_LogicSuite` | nothing — no world, no game mode, no manager (engine-enforced: `GetWorldFile()` is empty, so the game-mode getter is null) | is pure computation on hand-built objects: record maths, modifier recalculation, a job condition, a skill effect, a level curve |
| **B** Init | `OVT_TEST_InitSuite` | world + managers, campaign **not** started | asserts something true at world load: a manager resolves, towns are populated, controllers are registered, a config-driven price seam round-trips |
| **C** Campaign | `OVT_TEST_CampaignSuite` | started campaign | needs campaign-start products: activated towns, stocked shops, income calculators, faction state |
| **D** Persistence | `OVT_TEST_PersistenceSuite` | started campaign + the one save seam | writes state through a manager's public mutator and reads it back through its public accessor |
| **D'** RoundTrip | `OVT_TEST_PersistenceRoundTripSuite` | — | **nothing new goes here.** Quarantined migration gate; red by design |

Prefer Tier A wherever the logic allows — those cases cost nothing (14 of them run in 6-9 s) and cannot flake.

**Where files go**

```
Scripts/Game/Tests/
├── TestFramework/    glue — modded SCR_AutotestHelper (world + addon list), OVT_TEST_SuiteBase
└── TestSuites/<Tier>/OVT_TEST_<Tier>_<Subject>.c    a case file in an existing tier directory
```

**Naming:** suites `OVT_TEST_<Tier>Suite`; cases `OVT_TEST_<Tier>_<Subject>_<ExpectedBehaviour>`. The class name *is* the CLI argument, so keep it greppable. Case execution order inside a suite is **alphabetical by class name** — no case may depend on another having run, or leave state a later one needs.

**Skeleton** (a new *case* in an existing tier is the normal job; the suite line is shown for context)

```cpp
//! Suites inherit OVT_TEST_SuiteBase — never SCR_AutotestSuiteBase directly.
//! [BaseContainerProps()] is MANDATORY or group configs silently instantiate nothing.
//! The world comes from the modded helper — only the pure-logic tier overrides GetWorldFile().
[BaseContainerProps()]
class OVT_TEST_CampaignSuite : OVT_TEST_SuiteBase
{
    //! Opt in to a started campaign. Default is false; the guarded start
    //! sequence itself lives in OVT_TEST_SuiteBase and is a TEST concern only.
    override bool RequiresStartedCampaign() { return true; }
}

[Test(suite: OVT_TEST_CampaignSuite, timeoutS: 30)]
class OVT_TEST_Campaign_Economy_PricesAreInitialised : SCR_AutotestCaseBase
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
- **`maxAttempts:` on `[Test]` is banned.** It exists in the framework and appears nowhere in Overthrow's test tree. A test that needs retries is a bug in the test, a race, or a real defect — fix the cause. Using it requires recorded evidence of genuine engine-timing non-determinism in `findings.md`.
- **Prove the case can fail, once, before you ship it.** Perturb the thing it covers (change a real input, not the comparison), run, observe **exit 1** and check the failure text names your case, then revert. A case that cannot go red is a defect, not a passing test. Record the perturbation and the observed text in `docs/features/dev-ops/test-coverage/findings.md` → "Can-fail proofs".
- **Determinism beats breadth.** Three consecutive runs must be identical — same exit code, same case count, same summary. A flaky case is removed or fixed before the work is done.
- **Verdict API:** `AssertTrue(cond, msg)` (returns the bool, records a failure), `SetResultSuccess()`, `SetResultFailure("why")`. The failure string appears verbatim in `junit.xml` and `autotest.log`.
- **Use the case/suite-shadowed `Print` / `PrintFormat`**, not the global ones — the shadowed versions route through the autotest printer into `autotest.log`. Global `Print` only reaches `console.log`.
- **Never `#ifdef WORKBENCH` a test class.** Guarded tests do not exist in the retail client, which is what CI runs. Test code is inert without `-autotest` at runtime, which is the real safety guarantee.
- **Test world:** `Worlds/MP/OVT_Campaign_Test.ent` loads `OVT_OverthrowGameMode` plus managers via its `default.layer`, so `OVT_Global` accessors work — but the **campaign is not started**. Do not start it by hand: override `RequiresStartedCampaign()` on the suite and let `OVT_TEST_SuiteBase` do it. That sequence is non-idempotent, must select the difficulty preset **by name** (`"Test World"` — index 0 is `Easy`), and has to close the start menu; one guarded implementation exists so every campaign-tier suite starts identically.
- **Manager resolution:** `OVT_Global` is fine — its statics were measured against the live game mode across the harness's three world loads and agreed 100% of the time (the engine nulls the weak statics on destruction). `OVT_TEST_SuiteBase.ResolveManager(typename)` finds a component on the live game mode and exists as defensive documentation if that ever changes. Tier A may use **neither** — it has no game mode at all.
- **Test-world scale:** exactly **one** town and **one** base controller, garrisons never populate, and the navmesh does not load (so no AI-movement assertions). Assert `>= 1`, never a magic count.
- **`new` does not apply `[Attribute()]` defvalues** — a hand-built config object starts fully zeroed, which silently breaks any field whose declared default is not zero (an "unset" sentinel of `-1`, a multiplier of `1`). Set every field a Tier A case depends on, explicitly.
- Compile tests like any other code: `tools/compile-check.sh` first, then `tools/run-tests.sh <YourSuite>`, then the Fast or All group.

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
- **Coverage is a spine, not a safety net** - `tools/run-tests.sh` runs Reforger's shipped autotest framework against Overthrow: 30 assertions over logic, manager init, started-campaign state and same-session persistence. JIP/multiplayer, UI, performance and save/reload are uncovered and stay manual
- **Tests run in the real game client** - No mocks, no headless runner: each run boots the client and loads a world (~15s)
- **No debugger** - Print-based debugging
- **No hot reload** - Restart play mode to test changes
- **No CI/CD yet** - Compile check runs locally; dev-ops epic feature #4 will orchestrate it
- **User is QA** - User tests all runtime behaviour manually

---

**Pattern:** Start here for quick reference, dive into resource files for detailed procedures.
