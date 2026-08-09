# Workbench Automation - Context & Decisions

**Last Updated:** 2026-08-04
**Current Phase:** Phase 7 (live-session agent debug bridge) — planned, not started; core (Phases 1-6) complete
**Status:** 🟡 Extension planned (core ✅ shipped 2026-08-01)
**Epic:** dev-ops (feature #1 of 5 — foundational; nothing else in the epic starts until this lands)

---

## Quick Status

**What's Done:**
- ✅ ALL 6 CORE PHASES (56/56 tasks) — feature complete, 2026-08-01
- ✅ 2026-08-04: Live-session debug research complete (4 parallel research agents) — verdict: agent-driven code exec in a live play session IS possible. Full evidence in `live-session-debug-research.md` (this folder). Phase 7 added to tasks.md (7 tasks, spike-gated)
- ✅ Phase 1: empirical CLI verification (`findings.md` — the epic's Workbench CLI reference)
- ✅ Phase 2: `tools/lib/common.sh` (path translation, process boundary, verified timeout kills, 29/29 self-test)
- ✅ Phase 3: `tools/compile-check.sh` (four-condition verdict, gcc-style errors, T13 check-on-the-check passes)
- ✅ Phase 4: `tools/launch-game.sh` (KEY=value contract; autotest smoke run produced `junit.xml`)
- ✅ Phase 5: signal traps, pidfile-scoped stale sweep, concurrency warn, `stage_dev.sh` exclusion, `tools/README.md` contract
- ✅ Phase 6: docs corrected (CLAUDE.md, technical-design §2/§10, mission-statement, workbench-workflow skill v1.1.0, epic-overview, agent definitions)
- ✅ Bonus: tree fixed to compile on 1.7.0.54 (user-approved; vanilla-persistence WIP issues — recorded in that feature's context.md)

**What's Next:**
- 📋 **Task 7.1 (GATE):** spike `ScriptModule.CompileScript` in a live session — Workbench play mode + diag client, referencing OVT types. ~30 min; its result decides the Phase 7 design
- 📋 Task 7.2: `$profile:` command-file round-trip spike
- 📋 Then 7.3/7.4: `OVT_AgentBridge` component + `tools/debug-exec.sh` wrapper

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

### Decision 13 (2026-08-04): Debug bridge = CompileScript eval over a $profile: file drop-box, NOT the debugger protocol or hot reload
**Context:** Wanted agent-driven code execution in a live play session (today: agent writes `.tmp/group-debug.c`, human pastes into Script Console).
**Decision:** Build `OVT_AgentBridge` (Phase 7): dev-only poller → `ScriptModule.CompileScript(GetGame().GetScriptModule(), src, err, line)` → `Call` → results to `$profile:agent/out.json`. NetAPI (TCP 5775, already enabled+listening on this machine, script-side Net Handlers, see enfusion-mcp) is the optional Workbench-only push transport. Rejected: speaking the script-debugger port directly (private, checksum-gated protocol); relying on hot reload for injection (three independent signals say reload = full VM teardown, statics/ScriptInvokers die — see research doc).
**Rationale:** `CompileScript` is first-party, documented "workbench or diag" availability (our diag client qualifies), returns compile errors as strings, and BI's own doc comment is an eval recipe. Every transport primitive is proven in shipped code (Overthrow already does runtime `$profile:` JSON I/O).
**Full evidence:** `live-session-debug-research.md` (this folder). Recompile hotkey fact for 7.6: current build renamed the action — `Shift+F7` = "Validate and Reload Scripts"; `README.md:54` label is stale.

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
1. **Task 7.1 (GATE, ~30 min):** spike `CompileScript` mid-session — Workbench play mode + diag client, compiled code must reach `OVT_Global.GetTowns()`; check repeat-compile leak
2. Task 7.2: `$profile:agent/` file round-trip from a repeating `CallLater`
3. Re-read Phase 7 tasks against the spike results before building 7.3

### Future (After This Phase)
1. Tasks 7.3–7.4: `OVT_AgentBridge` + `tools/debug-exec.sh`
2. Optional 7.5 (NetAPI handler) / 7.6 (external `Shift+F7` reload trigger + hot-reload-during-play manual test)
3. 7.7 docs, then `/update-epic dev-ops`

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
  2. `_OVT_*Template.c` reference files (placeholder types, can never compile) → moved to `docs/features/core/persistence/templates/`
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

### 2026-08-04 — Live-session debug research done; Phase 7 added 📋
- User asked whether agents can connect to a running Workbench and execute code in an active play session (current loop: agent writes `.tmp/group-debug.c`, human pastes into Script Console); follow-up: can the agent trigger a recompile in an open Workbench
- 4 parallel research agents (WorkbenchAPI surface, binary IPC probe, script-side bridge feasibility, WSL keystroke/UIA injection). **Verdict: YES.** Written up in `live-session-debug-research.md` (this folder) + memory `live-session-code-execution`
- Headlines: `ScriptModule.CompileScript`/`LoadScript` = runtime eval (workbench/diag/headless); **NetAPI TCP 5775 already enabled and listening** on this machine ("controlled from external applications", script-side Net Handlers, working community proof enfusion-mcp); no pipes/HTTP-server/instance-forwarding; recompile hotkey = `Shift+F7` (action renamed to "Validate and Reload Scripts"); hot reload ≈ full VM teardown, not live patch; UIA menu-invoke from PowerShell/WSL is the robust external reload trigger
- User decided: fold into THIS feature (Phase 7, 7 tasks) rather than a new feature — small enough to implement here later. Docs updated; nothing built yet; 7.1 is the gate
- Interim zero-code win available any time: `clip.exe < snippet.c` so the human step is just paste+Enter

### 2026-08-01 20:55
- Feature started via `/autorun-feature dev-ops/workbench-automation` (autonomous run from Discord)
- Scaffolded context.md + tasks.md from the existing implementation plan (56 tasks / 6 phases)
- Next: Phase 1 empirical verification with advanced agent
- **Agent-tier note:** the plan marks Phase 1 ADVANCED, but the project's `*-advanced` agents (component-developer-advanced, network-specialist-advanced) have no Bash tool and cannot run CLI experiments. Fell back to a full-toolset general-purpose agent on the strongest available model for Phases 1/2/3 (bash tooling domain — no matching advanced variant exists for it).

---

*Update this file at the end of each work session. Run `/dev-docs-update` before compacting conversations.*
