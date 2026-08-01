---
name: workbench-workflow
description: Arma Reforger Workbench workflow, testing guidelines, and debugging patterns
version: 1.1.0
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
Compilation is verified automatically: run `tools/compile-check.sh` yourself after code changes. There is still no automated test system — all runtime testing is manual via Workbench play mode. Be specific about what the user should test and how.

**See:** `testing-guidelines.md` for manual test procedures

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
- ❌ **No unit tests** - Manual play-testing only
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
5. **User tests** in Workbench play mode
6. **User reports** bugs/runtime issues (debug prints, console errors)
7. **Fix issues**
8. **Repeat** until working

### Testing Cycle

1. **Define test procedure** - Specific steps to test feature
2. **User follows procedure** in Workbench play mode
3. **User reports results** - What worked, what didn't
4. **Fix issues** if needed
5. **Retest** until feature works

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

Also available: `tools/launch-game.sh` launches the game client with Overthrow loaded and reports the run's log directory — it exists for the dev-ops epic's autotest work (feature #2), not for this skill's manual-testing flow.

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
- **No test framework** - Manual testing only (dev-ops epic feature #2 is building one)
- **No debugger** - Print-based debugging
- **No hot reload** - Restart play mode to test changes
- **No CI/CD yet** - Compile check runs locally; dev-ops epic feature #4 will orchestrate it
- **User is QA** - User tests all runtime behaviour manually

---

**Pattern:** Start here for quick reference, dive into resource files for detailed procedures.
