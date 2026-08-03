# Workbench Automation - Task Checklist

**Last Updated:** 2026-08-04
**Progress:** 56/63 tasks complete (89%) — core feature 56/56 done 2026-08-01; Phase 7 (live-session agent debug bridge) added 2026-08-04, not started
**Advanced phases:** Phase 1 (REQUIRED — max-effort empirical work), Phase 3 (RECOMMENDED — verdict logic is where a false green is born)

---

## Phase 1: Empirical CLI Verification (15/15 complete) ✅ — ADVANCED AGENT REQUIRED

- [x] ✅ **1.1 Create findings.md with fixed table shape**
  - Description: Table: flag/command → observed exit code → wall time → window/focus → log location → notes. Record Reforger + Tools build numbers and Workbench branch (stable vs Experimental) at top.
  - File(s): `docs/features/dev-ops/workbench-automation/findings.md`
  - Estimate: 🟢 Small

- [x] ✅ **1.2 Baseline launch** — `-gproj` alone: GUI? blocking? meaningful `$?`? duration?
  - Estimate: 🟢 Small

- [x] ✅ **1.3 Detachment check (highest-risk unknown)** — shell wall time vs `tasklist.exe` process lifetime; if it detaches, `$?` is worthless (changes Phases 2–3)
  - Estimate: 🟡 Medium

- [x] ✅ **1.4 `-validate` matrix** — bare, `PC`, `workbench`: which accepted, what returned
  - Estimate: 🟡 Medium

- [x] ✅ **1.5 `-validate` against broken tree** — scratch ternary file; record raw exit code (does `-1` arrive as `255`?); clean run differs
  - Estimate: 🟡 Medium

- [x] ✅ **1.6 Confirm error format holds under `-validate`** — `@"path,line"` / `Can't compile` / `Too many errors` / success markers, same file
  - Estimate: 🟢 Small

- [x] ✅ **1.7 Non-disruption flags** — `-wbsilent`, `-exitAfterInit`, `-noFocus`, `-noThrow`: minimal flag set that validates with no visible window
  - Estimate: 🟡 Medium

- [x] ✅ **1.8 Profile pinning** — `-profile <Name>` creates `<My Games>/<Name>/logs/logs_<ts>/`? Nested names? Absolute paths? Both binaries?
  - Estimate: 🟡 Medium

- [x] ✅ **1.9 Timing** — cold run + three warm runs; propose default timeout
  - Estimate: 🟡 Medium

- [x] ✅ **1.10 Interop and process control** — does bash `timeout` kill the Windows process? `taskkill.exe /F /T`? PID capture? `tasklist.exe` reliable?
  - Estimate: 🟡 Medium

- [x] ✅ **1.11 Environmental prerequisites** — Steam required/logged in? dialogs? behaviour with Steam closed?
  - Estimate: 🟢 Small

- [x] ✅ **1.12 Concurrency** — `-validate` while a Workbench GUI is open: refusal, contention, or success?
  - Estimate: 🟢 Small

- [x] ✅ **1.13 Truncation threshold** — enough errors to trip `Too many errors`; roughly where the cap falls
  - Estimate: 🟢 Small

- [x] ✅ **1.14 Game client launch** — confirmed shape + `-autotest` accepted; BI's `SCR_TEST_Example1TestSuite` as free probe
  - Estimate: 🟡 Medium

- [x] ✅ **1.15 Write up divergences** — "Differs from assumptions" section in findings.md
  - Estimate: 🟢 Small

> **GATE:** Before Phase 2, re-read Phases 2–6 against `findings.md` and amend the plan.

---

## Phase 2: tools/lib/common.sh — shared library (9/9 complete) ✅

- [x] ✅ **2.1 Create `tools/`, `tools/lib/`; gitignore `tools/config.local.sh`**
  - File(s): `tools/`, `.gitignore`
  - Estimate: 🟢 Small

- [x] ✅ **2.2 Path translation** — `ovt_win_path` / `ovt_wsl_path` (spaces, trailing slashes, missing paths normalised)
  - File(s): `tools/lib/common.sh`
  - Estimate: 🟡 Medium

- [x] ✅ **2.3 Resolution + validation** — `ovt_workbench_exe`, `ovt_game_exe`, `ovt_project_gproj`, `ovt_game_gproj`, `ovt_mygames_dir` (discover OneDrive redirect), `ovt_profile_dir`; each honours `OVERTHROW_*` override, exit 2 with specific message when missing
  - Estimate: 🟡 Medium

- [x] ✅ **2.4 `ovt_run_win <timeout_s> <exe> [args...]`** — the single boundary crossing; exit-code normalisation; 124 on timeout; shaped by findings 1.3/1.10
  - Estimate: 🔴 Large

- [x] ✅ **2.5 Process control** — `ovt_is_running`, `ovt_kill_tree` (verified kill, exit 2 if cleanup fails)
  - Estimate: 🟡 Medium

- [x] ✅ **2.6 `ovt_run_workbench` / `ovt_run_game`** — thin verb-agnostic wrappers (feature #5's integration point)
  - Estimate: 🟢 Small

- [x] ✅ **2.7 `ovt_resolve_log_dir <profile> <since_epoch>`** — newest logs dir since t0; empty+non-zero = indeterminate, never success
  - Estimate: 🟡 Medium

- [x] ✅ **2.8 Output helpers** — `ovt_info`/`ovt_warn`/`ovt_err` to stderr only
  - Estimate: 🟢 Small

- [x] ✅ **2.9 Self-test path** — path round-tripping (spaces, drives, trailing slash), My Games discovery
  - Estimate: 🟡 Medium

---

## Phase 3: tools/compile-check.sh (10/10 complete) ✅ — ADVANCED AGENT RECOMMENDED

- [x] ✅ **3.1 CLI surface** — `--config --timeout --all --absolute --keep-log --verbose --allow-concurrent --help`; defaults from env vars from findings
  - File(s): `tools/compile-check.sh`
  - Estimate: 🟡 Medium

- [x] ✅ **3.2 Assemble argument list from Phase 1 findings; invoke `ovt_run_workbench`**
  - Estimate: 🟢 Small

- [x] ✅ **3.3 Copy run's console.log to `.tmp/compile-check/last.log`** (post-mortem + CI artifact)
  - Estimate: 🟢 Small

- [x] ✅ **3.4 Parser** — `@"path,line": message` → {file,line,message}; failure marker (module not hardcoded); no double-count of restatement line; `Too many errors` flag; unknown (E) lines surfaced never dropped
  - Estimate: 🔴 Large

- [x] ✅ **3.5 Evidence markers** — `Module: X; loaded Nx files` / `PROFILING: Compiling` required, else indeterminate
  - Estimate: 🟢 Small

- [x] ✅ **3.6 Verdict logic** — three-condition rule for exit 0; errors → 1; anything else → 2 with reason + log pointer
  - Estimate: 🟡 Medium

- [x] ✅ **3.7 Scope filtering** — out-of-project errors filtered by default, count always printed, `--all` disables
  - Estimate: 🟡 Medium

- [x] ✅ **3.8 Truncation reporting** — "22+ errors, engine stopped listing"
  - Estimate: 🟢 Small

- [x] ✅ **3.9 Output format** — gcc-style `file:line: error: message` on stdout, repo-relative; summary on stderr
  - Estimate: 🟢 Small

- [x] ✅ **3.10 Exit-code taxonomy 0/1/2/124** implemented + documented in `--help`
  - Estimate: 🟢 Small

---

## Phase 4: tools/launch-game.sh (8/8 complete) ✅

- [x] ✅ **4.1 CLI surface** — `[--timeout] [--profile] [--quiet] -- <args...>`; pass-through preserves quoting/spaces
  - File(s): `tools/launch-game.sh`
  - Estimate: 🟡 Medium

- [x] ✅ **4.2 Default client launch shape** — `-gproj <base> -addonsDir <parent> -addons EDF,EPF,Overthrow -profile <name>`; caller duplicates take precedence
  - Estimate: 🟡 Medium

- [x] ✅ **4.3 Non-disruptive defaults** — `-noFocus -noThrow -window -logLevel debug` (BI's own autotest set), overridable
  - Estimate: 🟢 Small

- [x] ✅ **4.4 Record t0, launch via `ovt_run_game` with timeout**
  - Estimate: 🟢 Small

- [x] ✅ **4.5 Resolve run's log dir; exit 2 if unresolvable**
  - Estimate: 🟢 Small

- [x] ✅ **4.6 `KEY=value` stdout contract** — PROFILE_DIR, LOG_DIR, LOG_DIR_WIN, EXIT_CODE, DURATION_S; everything else stderr
  - Estimate: 🟢 Small

- [x] ✅ **4.7 Exit-code propagation** — normalised, 124 on timeout
  - Estimate: 🟢 Small

- [x] ✅ **4.8 End-to-end smoke** — `-- -autotest SCR_TEST_Example1TestSuite`; confirm junit.xml under LOG_DIR (first autotest ever on this machine)
  - Estimate: 🟡 Medium

---

## Phase 5: Robustness, cleanup, contract doc (7/7 complete) ✅

- [x] ✅ **5.1 Concurrency guard** — refuse (exit 2) or warn per finding 1.12; `--allow-concurrent`
  - Estimate: 🟡 Medium

- [x] ✅ **5.2 Stale-process cleanup** — opt-in sweep, scoped so it can't kill a dev's open Workbench
  - Estimate: 🟡 Medium

- [x] ✅ **5.3 Signal handling** — trap INT/TERM, kill Windows process too
  - Estimate: 🟢 Small

- [x] ✅ **5.4 Timeout hardening** — verify kill, escalate, exit 2 if cleanup fails
  - Estimate: 🟢 Small

- [x] ✅ **5.5 `stage_dev.sh` exclusion** — add `tools` to exclusion list
  - File(s): `.scripts/stage_dev.sh`
  - Estimate: 🟢 Small

- [x] ✅ **5.6 Write `tools/README.md`** — full contract: commands, flags, exit codes, stdout formats, env vars, log resolution rule, feature-#5 verb extension guide
  - Estimate: 🟡 Medium

- [x] ✅ **5.7 Known-limitations section** — Steam required, GPU host, slow first run, `-validate` = compile only, build-specific findings
  - Estimate: 🟢 Small

---

## Phase 6: Documentation updates — Definition of Done (7/7 complete) ✅

- [x] ✅ **6.1 `CLAUDE.md`** — rewrite "cannot compile/build" bullet + "No automated builds" constraint; keep "no unit tests"/"no debugger"
- [x] ✅ **6.2 `docs/technical-design.md` §2** — "No automated builds" bullet no longer claims Claude/CI can never verify compilation
- [x] ✅ **6.3 `docs/technical-design.md` §10** — item 1: automated compile check is the first gate; GUI is the interactive path; do NOT touch "There is no test suite"
- [x] ✅ **6.4 `docs/mission-statement.md`** — "Automating the quality gate": first stage landed
- [x] ✅ **6.5 `.claude/skills/workbench-workflow/SKILL.md`** — constraints, dev cycle, "ask user for compile errors" → run compile check; no new dangling references
- [x] ✅ **6.6 `docs/features/dev-ops/epic-overview.md`** — flip feature #1 status, refresh rollup
- [x] ✅ **6.7 Cross-check grep** — no remaining "compilation cannot be automated" claims; test/debugger claims intact

---

## Phase 7: Live-session agent debug bridge (0/7) 📋 — added 2026-08-04

> **Goal:** replace the "agent writes `.tmp/group-debug.c`, human pastes into the Script Console" loop with zero-human-action code execution in a live play session. Research complete — plan basis is `live-session-debug-research.md` in this folder (verdict: possible via `ScriptModule.CompileScript` runtime eval; NetAPI TCP 5775 is the Workbench-only alternative transport). Spike 7.1 is the gate: its result decides the whole design.

- [ ] **7.1 Spike: `CompileScript` in a live session** — in Workbench play mode AND the diag client: does `ScriptModule.CompileScript(GetGame().GetScriptModule(), src, err, line)` + `Call` work mid-session; can compiled code reference Overthrow types (`OVT_Global.GetTowns()`); does repeated compiling leak. **GATE for 7.3-7.5.**
  - Estimate: 🟡 Medium
- [ ] **7.2 Spike: `$profile:` command-file round-trip** — existence-poll → read → write result → `DeleteFile` from a repeating `CallLater`; confirm agent-side profile-dir resolution via `tools/lib/common.sh`. (No mtime API exists — protocol is consume-and-delete or seq-numbered.)
  - Estimate: 🟢 Small
- [ ] **7.3 `OVT_AgentBridge` component** — dev-only (must never be active in retail; `CompileScript` self-limits to workbench/diag but gate the poller too): repeating `CallLater` ~250ms polls `$profile:agent/cmd.c`; on hit compile → `Call(null, "Main", ...)` → write `{result, errorText, errorLine}` to `$profile:agent/out.json` → delete input. Guards from `SCR_AutotestRunner.c:90-125` (`IsPreloadFinished`, transition-in-progress).
  - Estimate: 🔴 Large
- [ ] **7.4 Agent-side wrapper `tools/debug-exec.sh`** — write snippet to the bridge's command path, wait (bounded) for `out.json`, print result/compile errors; honest 0/1/2/124 taxonomy consistent with the other tools; `tools/README.md` section.
  - Estimate: 🟡 Medium
- [ ] **7.5 (Optional) NetAPI handler variant** — evaluate registering a Net Handler on the already-listening Workbench NetAPI port 5775 (study github.com/steffenbk/enfusion-mcp-BK) as a push transport replacing file polling. Workbench-only.
  - Estimate: 🟡 Medium
- [ ] **7.6 (Optional) External reload trigger** — manual test first: `Shift+F7` ("Validate and Reload Scripts" — renamed in current build) during play mode: enabled? drops to edit mode? Then, if useful, UIA Expand+Invoke on the Script Editor `Build` menu from PowerShell/WSL to kill the stale-scripts-after-WSL-edits trap. Note: hot reload is almost certainly a full VM teardown, not a live patch — this serves the edit→replay loop, not the bridge.
  - Estimate: 🟡 Medium
- [ ] **7.7 Docs** — `tools/README.md` bridge contract; `workbench-workflow` skill (debug-loop section + fix stale "Compile and Reload Scripts" label, also in `README.md:54`); update this feature's context.md + epic rollup.
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
- 2026-08-04: Phase 7 (7.1–7.7) — live-session agent debug bridge, from the investigation recorded in `live-session-debug-research.md`

### Blocked Items
- (none)

---

*Update this file as tasks are completed. Mark tasks with ✅ immediately when done. Add new tasks as they're discovered.*
