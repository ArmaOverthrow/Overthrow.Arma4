---
name: workbench-workflow
description: Arma Reforger Workbench workflow, testing guidelines, and debugging patterns
version: 1.0.0
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
No automated build or test system available. All testing manual via Workbench. User compiles in Workbench and reports errors. Be specific about what to test and how.

**See:** `testing-guidelines.md` for manual test procedures

### Compile Errors
EnforceScript has specific error patterns. Common issues: missing semicolons, ternary operators, type mismatches, missing strong refs. Errors show in Workbench console.

**See:** `compile-errors.md` for common errors and fixes

### Debug Patterns
Use Print() for debug output, check Workbench console logs. No interactive debugger. Add debug prints strategically to trace execution and inspect values.

**See:** `debug-patterns.md` for debugging techniques

### Workbench Tips
Prefabs edited in Workbench, layouts in UI editor, configs in text editor. Save often. Workbench can crash. Always test changes in play mode.

**See:** `workbench-tips.md` for Workbench best practices

---

## Critical Constraints

- ❌ **No automated builds** - User compiles in Workbench
- ❌ **No unit tests** - Manual play-testing only
- ❌ **No interactive debugger** - Use Print() for debug output
- ✅ **Be specific** - Tell user exactly what to test
- ✅ **Test incrementally** - Small changes, test often
- ⚠️ **Workbench can crash** - Save frequently
- ✅ **Check console** - Errors show in Workbench console
- ⚠️ **Play mode testing** - Always test in play mode, not just edit mode

---

## Workflow Pattern

### Development Cycle

1. **Write code** in Claude Code
2. **User opens Workbench** and compiles
3. **User reports** compile errors
4. **Fix errors** based on report
5. **Repeat** until compiles
6. **User tests** in play mode
7. **User reports** bugs/issues
8. **Fix issues**
9. **Repeat** until working

### Testing Cycle

1. **Define test procedure** - Specific steps to test feature
2. **User follows procedure** in Workbench play mode
3. **User reports results** - What worked, what didn't
4. **Fix issues** if needed
5. **Retest** until feature works

---

## Common Workflow

### After Code Changes

Tell user:
```
Please test in Workbench:
1. Open Overthrow.Arma4 in Workbench
2. Let it compile (check console for errors)
3. If compile errors, report them to me
4. If compiles: Enter play mode
5. Test: [specific steps]
6. Report: [specific things to check]
```

### After Compile Errors

Ask user:
```
Please copy the exact error messages from the Workbench console
and send them to me. I'll fix the compile errors.
```

### After Runtime Errors

Ask user:
```
Please:
1. Check the Workbench console for any error messages
2. Try: [specific test step]
3. Report: What happened vs what should happen
```

---

## Resource Files

Detailed documentation organized by concern:

1. **testing-guidelines.md** - Manual test procedures for different component types
2. **compile-errors.md** - Common EnforceScript compile errors and solutions
3. **debug-patterns.md** - Using Print(), console logs, debugging techniques
4. **workbench-tips.md** - Working with prefabs, configs, layouts, best practices

---

## Key Differences from Other Dev Environments

- **No npm/build scripts** - Workbench handles compilation
- **No test framework** - Manual testing only
- **No debugger** - Print-based debugging
- **No hot reload** - Restart play mode to test changes
- **No CI/CD** - Local Workbench testing only
- **User is QA** - User tests everything manually

---

**Pattern:** Start here for quick reference, dive into resource files for detailed procedures.
