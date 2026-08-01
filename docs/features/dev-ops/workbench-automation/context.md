# Workbench Automation - Context & Decisions

**Last Updated:** 2026-08-01 23:10
**Current Phase:** Complete (all 6 phases)
**Status:** ✅ Ready for Review
**Epic:** dev-ops (feature #1 of 5 — foundational; nothing else in the epic starts until this lands)

---

## Quick Status

**What's Done:**
- ✅ ALL 6 PHASES (56/56 tasks) — feature complete, 2026-08-01
- ✅ Phase 1: empirical CLI verification (`findings.md` — the epic's Workbench CLI reference)
- ✅ Phase 2: `tools/lib/common.sh` (path translation, process boundary, verified timeout kills, 29/29 self-test)
- ✅ Phase 3: `tools/compile-check.sh` (four-condition verdict, gcc-style errors, T13 check-on-the-check passes)
- ✅ Phase 4: `tools/launch-game.sh` (KEY=value contract; autotest smoke run produced `junit.xml`)
- ✅ Phase 5: signal traps, pidfile-scoped stale sweep, concurrency warn, `stage_dev.sh` exclusion, `tools/README.md` contract
- ✅ Phase 6: docs corrected (CLAUDE.md, technical-design §2/§10, mission-statement, workbench-workflow skill v1.1.0, epic-overview, agent definitions)
- ✅ Bonus: tree fixed to compile on 1.7.0.54 (user-approved; vanilla-persistence WIP issues — recorded in that feature's context.md)

**What's Next (for the epic, not this feature):**
- 📋 Feature #2 `autotest-foundation` — its whole dependency is proven (`launch-game.sh -- -autotest "{GUID}"` → `junit.xml` in `LOG_DIR`, ~8s)
- 📋 Changes are uncommitted on `vanilla-persistence` — user reviews & commits

**Blockers:**
- None

---

## Key Files

### Deliverables (to be created)
- `tools/lib/common.sh` - Shared library: path translation, process boundary (`ovt_run_win`), timeouts, log resolution
- `tools/compile-check.sh` - Headline deliverable: compile check with 0/1/2/124 exit codes and `file:line: message` errors
- `tools/launch-game.sh` - Game-client launcher for feature #2 (`-autotest`), emits `KEY=value` stdout contract
- `tools/README.md` - The contract document siblings read (Phase 5)
- `docs/features/dev-ops/workbench-automation/findings.md` - Phase 1 empirical record; the epic's CLI reference

### Related Files
- `docs/features/dev-ops/workbench-automation/implementation.md` - The plan (read Verified Ground Truth + Decisions before touching anything)
- `.scripts/*.sh` - Existing bash conventions (`OVERTHROW_*` env overrides)
- `.scripts/stage_dev.sh` - Needs `tools/` exclusion (task 5.5)

### Docs to update in Phase 6 (Definition of Done)
- `CLAUDE.md`, `docs/technical-design.md` §2+§10, `docs/mission-statement.md`, `.claude/skills/workbench-workflow/SKILL.md`

---

## Important Decisions

(12 key decisions live in `implementation.md` — summarised, the load-bearing ones:)

### Decision 1: `-validate` as compile mechanism, log-derived fallback
If Phase 1 shows `-validate` misbehaves, derive verdict from `console.log` — external contract stays identical.

### Decision 2: Success positively proven — three-condition rule
Exit 0 requires: acceptable exit code AND zero in-project errors AND affirmative evidence marker (`Module: X; loaded Nx files` / `PROFILING: Compiling`). Anything ambiguous → exit 2, never 0.

### Decision 5: Pinned dedicated `-profile` (e.g. `OverthrowCI`)
Logs resolve deterministically as newest-since-t0 under that profile. My Games dir is OneDrive-redirected — discover, never construct.

### Decision 12: Game client uses `-gproj <base ArmaReforger.gproj> -addonsDir -addons`, NOT Overthrow's addon.gproj
Confirmed from a real crash.log. Workbench, by contrast, DOES take Overthrow's `addon.gproj`.

---

## Gotchas & Learnings

### 1. Phases 2–6 are provisional until Phase 1 lands
**Problem:** No Workbench CLI flag has ever been executed on this machine — everything is strings/wiki-derived.
**Solution:** Phase 1 gate: re-read Phases 2–6 against `findings.md` and amend the plan before building.
**Lesson:** Building against an assumption Phase 1 already disproved is the main failure mode.

### 2. Verbatim error format (from GUI sessions, needs -validate confirmation)
- Errors: `SCRIPT (E): @"path,line": message` — comma not colon, addon-relative path
- Failure marker: `Can't compile "X" script module!` followed by an unprefixed restatement of the FIRST error only (don't double-count)
- Truncation sentinel: literal `Too many errors`
- NO success string, NO error-count summary anywhere — verdict must be assembled

---

## Testing Approach

No bash test framework — fixed manual checklist T1–T24 in `implementation.md` §Testing Strategy. Canary file `Scripts/Game/OVT_CompileCheckCanary.c` (ternary = guaranteed compile error), created and deleted by the test. Most important test: T13 (empty/unparseable log → exit 2, never 0).

---

## Next Steps

### Immediate
1. Phase 1 tasks 1.1–1.15 (advanced agent, one flag at a time, record everything in `findings.md`)
2. Gate: amend Phases 2–6 against findings
3. Phase 2: `tools/lib/common.sh`

### Future (After This Phase)
1. Phases 3–5: compile-check.sh, launch-game.sh, robustness + README
2. Phase 6: doc updates (Definition of Done — not optional)

---

## Open Questions

- [ ] **Q:** Does `-validate` accept `PC`, `workbench`, or both? (Decision 9 — default must be the shipping config)
- [ ] **Q:** Is `ArmaReforgerWorkbenchSteamDiag.exe` a detaching launcher shim? (task 1.3 — changes Phases 2–3 if yes)
- [ ] **Q:** Does bash `timeout` kill the Windows process or just the interop stub? (task 1.10)

---

## Session Notes

### 2026-08-01 21:45 — Phase 1 complete ✅ (+ tree fixed)
- Phase 1 (15/15 tasks) done by advanced-tier agent; full evidence in `findings.md`; plan gate applied — see "✅ GATE APPLIED" block in implementation.md Phase 1
- Headlines: `-wbsilent -validate` = the CI shape (exit 0/255 real, ~3.4s warm, no window); config arg ignored; `-addonsDir` mandatory (comma-separated; packed workshop EPF/EDF — source repos don't compile on 1.7.0.54); **false-pass trap** (unresolvable deps → silently validates base game → exit 0; parser must check `Loaded addons:` for Overthrow); bash `timeout` kills only the interop stub (need taskkill by PID); client exit code always 0; client needs cwd = game dir; `-autotest "{6AB9C8EEE9A651B5}"` wrote junit.xml (first autotest ever on this machine)
- **Tree at HEAD didn't compile on 1.7.0.54** (vanilla-persistence WIP). User approved minimal fixes via Discord:
  1. `OVT_Component.c` — illegal generic method → new `OVT_ComponentFinder<Class T>` class (EPF-style)
  2. `_OVT_*Template.c` reference files (placeholder types, can never compile) → moved to `docs/features/vanilla-persistence/templates/`
  3. `OVT_PersistenceManagerComponent.c` — fictional APIs (`TriggerSave`, `PersistenceCollection.GetOrCreate`, `DB_BASE_DIR`) stubbed with `TODO(vanilla-persistence)`; real vanilla event hooks kept live; `OnGameEnd` is now a plain method (no such ScriptComponent engine event) that must be called explicitly
- Canonical validate now **exit 0** — first green automated compile check on Overthrow
- ⚠️ For vanilla-persistence resume: draft was written against a nonexistent API; real API is per-entity `PersistenceSystem.Save()` + config-driven collections; `HasSaveGame()` stub returns false

### 2026-08-01 23:10 — Phases 2–6 complete, feature done ✅
- Phase 2: `tools/lib/common.sh` — deviation: `ovt_win_path` doesn't trust `wslpath -w` for `/mnt/<drive>` (it silently emits UNC for unmapped drives); exact string transform instead. `ovt_run_game` cd+restore (not subshell) to preserve `OVT_LAST_*` vars.
- Phase 3: `tools/compile-check.sh` — **caught a real false-pass during verification**: whole-log grep for the addon proof passes even on dropped-addon runs (path appears 3x in CLI echo); proof must be scoped to the `Loaded addons:` block (correction noted in findings.md). Verdict = four conditions (exit 0 + zero in-project errors + Game-module evidence + block-scoped addon proof). Dep-failure special case → exit 2 "Overthrow could not be verified". `--config` rejected (Workbench ignores the arg). Parse errors abort at first failing file (engine behaviour); semantic errors collect across files.
- Phase 4: `tools/launch-game.sh` — merge rule (caller-supplied flags drop defaults); absolute `-profile` rejected; EXIT_CODE documented as meaningless (client always exits 0); client CLI-params echo strips quoting (not a space-preservation oracle); timeout → no stdout contract. Stray test profiles `OtherProfile`, `Overthrow CI Test` left under My Games (harmless).
- Phase 5: INT/TERM traps scoped to `ovt_run_win` (caller traps saved/restored); pidfile registry `.tmp/ovt-pids/` + `--sweep-stale [--kill]` (registry-scoped + image-name match = can't kill dev's own session); `stage_dev.sh` excludes `tools/`; `tools/README.md` = the sibling contract; self-test 29/29.
- Phase 6: all doc targets updated; agent definitions (component-developer[-advanced], network-specialist-advanced, solution-architect) + vanilla-persistence requirements.md also corrected. No-tests/no-debugger claims intact.
- Final cross-phase sweep: compile-check OK exit 0 (6s), self-test 29/29, zero orphans, sweep clean.
- Needs human verification (see final report): one deliberate Q4 focus check during a compile run; optional no-Steam behaviour test.

### 2026-08-01 20:55
- Feature started via `/autorun-feature dev-ops/workbench-automation` (autonomous run from Discord)
- Scaffolded context.md + tasks.md from the existing implementation plan (56 tasks / 6 phases)
- Next: Phase 1 empirical verification with advanced agent
- **Agent-tier note:** the plan marks Phase 1 ADVANCED, but the project's `*-advanced` agents (component-developer-advanced, network-specialist-advanced) have no Bash tool and cannot run CLI experiments. Fell back to a full-toolset general-purpose agent on the strongest available model for Phases 1/2/3 (bash tooling domain — no matching advanced variant exists for it).

---

*Update this file at the end of each work session. Run `/dev-docs-update` before compacting conversations.*
