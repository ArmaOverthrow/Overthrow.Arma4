# Workbench Automation - Implementation Plan

**Epic:** dev-ops (feature #1 of 5)
**Status:** Ready for Review
**Started:** 2026-08-01
**Completed:** 2026-08-01
**Last Updated:** 2026-08-01 23:10

---

## Quality Bar

**Target quality:** Infrastructure-grade. This is backend tooling that every other feature in the epic — and eventually CI — will trust blindly. Nobody reads its output carefully; they read its exit code. That sets the bar.

- **Honest exit codes above all.** A compile check that returns 0 when compilation actually failed is the single worst outcome this feature can produce: it converts a broken build into a silently-shipped regression and destroys trust in the whole epic. **Success must be positively proven, never assumed.** Absence of detected errors is not evidence of success — the wrapper must find affirmative evidence that compilation ran and completed before it may exit 0.
- **Three-valued thinking, not two.** Pass / Fail / *Could not determine*. Anything ambiguous — missing log, unparseable output, unexpected exit code, no evidence of compilation — is a distinct non-zero result (exit 2), never folded into either pass or fail.
- **Determinism.** The same tree produces the same verdict every run. No dependence on interactive shell state, no dependence on which log directory happens to be newest, no dependence on the user's window focus.
- **Robust termination.** Every launch is bounded by a timeout. A timeout is a reported failure, never a hang. No run may leave an orphaned Windows process behind — CI runners die of accumulated zombies.
- **Output readable by human and agent alike.** Errors as `file:line: message` on stdout (universally parseable, editor-clickable, trivially convertible to CI annotations); diagnostics on stderr; never a wall of undifferentiated log text.
- **Truncation must be visible.** The engine caps its error list with a literal `Too many errors`. A tool that reports "22 errors" when the engine gave up counting is lying by omission — say so explicitly.
- **Non-disruptive by default.** Runs on a machine someone is actively working on. No stolen focus, no flashing windows, no clobbering an already-open Workbench session.
- **Stable public contract.** Command names, flags, exit codes and output format are the API that features #2, #4 and #5 build on. Changing them later is a breaking change; get them right and write them down.

---

## Executive Summary

This feature establishes the **process boundary** for the entire dev-ops epic: the ability to launch and drive the Arma Reforger Workbench and game client from a WSL bash shell, unattended, and interpret the result programmatically.

Its headline deliverable is a **compile check** — `tools/compile-check.sh` — a single command runnable from the repo root that compiles all of Overthrow's EnforceScript and returns a real exit code plus structured `file:line: message` errors, with no GUI interaction and no human pressing Build. Today every compile error costs a full human round-trip; this removes that cost entirely.

Alongside it ships `tools/launch-game.sh` (arbitrary-argument game-client launcher with log-directory resolution, required by feature #2 for `-autotest`) and `tools/lib/common.sh` (the shared library owning WSL↔Windows path translation, process launching, timeouts and log resolution — solved once here, reused by every downstream feature).

The approach is deliberately thin: **bash wrappers around documented Workbench CLI flags**, primarily `-validate`. No WorkbenchPlugin, no C++ tooling, no bespoke build system. But the design is **evidence-first**: no Workbench CLI flag has ever been executed against this project, so Phase 1 is empirical verification, recorded in `findings.md`, and every later phase is explicitly subject to revision by what Phase 1 finds.

---

## Goals

### Primary Goals

1. **A working compile check.** One command, from a WSL shell, that compiles Overthrow and exits 0 on success / non-zero on failure — with errors parsed into `file:line: message`.
2. **Empirically-established ground truth** about the Workbench CLI: what each flag actually does, what exit codes come back, where compile errors surface under `-validate`, how long runs take, and what appears on screen. Written down in `findings.md` as the epic's reference.
3. **WSL↔Windows path translation solved once**, in a shared library, never reimplemented per-feature.
4. **A game-client launcher** accepting arbitrary pass-through arguments, resolving the run's profile/log directory afterwards — the contract feature #2 needs for `-autotest` and `junit.xml`.
5. **Bounded, cleanly-terminating runs.** Timeouts that actually kill the Windows process and report failure.

### Secondary Goals

1. **A stable, documented contract** (`tools/README.md`) that features #2, #4 and #5 consume without reading the scripts.
2. **A generic Workbench verb runner** so feature #5 can add `-packAddon` / `-publishAddon*` without writing new launch plumbing.
3. **Docs that stop asserting capability this feature invalidates** — CLAUDE.md, `docs/technical-design.md` §2 and §10, `docs/mission-statement.md`, and the `workbench-workflow` skill.
4. **Portability hooks** — machine-specific paths behind environment variables, so feature #4's runner is a config change rather than a rewrite.

### Explicitly Out of Scope

Restating `requirements.md` so nothing creeps in:

- Running tests (feature #2). This feature only *launches* the client with arbitrary arguments.
- CI orchestration, GitHub Actions, runner setup (feature #4).
- Implementing packing/publishing (feature #5) — only leaving the door open for it.
- Any change to gameplay code under `Scripts/Game/`.
- Solving the lack of a debugger. `Print()` remains the runtime tool.
- A WorkbenchPlugin / `RunCommandline()` implementation. **Explicitly rejected as overkill** — see Decision 8.

---

## Verified Ground Truth (established 2026-08-01, before Phase 1)

Pre-planning investigation of this machine's on-disk logs already settled several things that were previously assumptions. These reduce Phase 1's scope but **do not eliminate it** — every sample below came from an *interactive GUI Workbench session*, not from a `-validate` run. Phase 1 must confirm the same shapes appear under `-validate`.

### Log locations (confirmed)

`Documents` on this machine is **redirected into OneDrive**. The real profile roots are:

| Profile root | Written by |
|---|---|
| `/mnt/c/Users/<user>/OneDrive/Documents/My Games/ArmaReforgerWorkbench/` | Workbench (stable branch) |
| `.../My Games/ArmaReforgerWorkbenchExp/` | Workbench (Experimental branch) |
| `.../My Games/ArmaReforger/` | Game client |
| `.../My Games/<ProfileName>/` | Any run launched with `-profile <ProfileName>` |

Each contains `logs/logs_YYYY-MM-DD_HH-MM-SS/` holding `console.log`, `script.log`, `error.log`, sometimes `crash.log`, plus `.backend/`. `script.log` is a strict subset of `console.log` (SCRIPT-tagged lines only). **`console.log` is the parse target** — it is the superset and the only one carrying the `PROFILING` timing line.

Existing profile names on this machine from past manual MP testing: `OverthrowDedi`, `OverthrowServer`, `PeerPlugin1`, `PeerTool1`, `PeerToolExp1`. This confirms `-profile` takes a **relative profile name**, not a path — see Decision 5.

### Compile-error format (verbatim, real samples)

Per-error line:

```
15:36:14.740    SCRIPT    (E): @"Scripts/Game/Components/EPF_BaseInventoryStorageComponentSaveData.c,6": Expected attribute call
00:43:08.593    SCRIPT    (E): @"Scripts/Game/GameMode/Managers/OVT_RespawnSystemComponent.c,2": Unknown type 'EPF_BaseRespawnSystemComponentClass'
```

Note the `@"<path>,<line>"` form — **comma, not colon**, inside the quotes; path is relative to the addon root. Whitespace between `SCRIPT` and the severity tag varies and must be treated as flexible.

Failure block (always this exact shape):

```
15:36:14.743    SCRIPT    (E): Can't compile "Game" script module!
<blank line>
Scripts/Game/Components/EPF_BaseInventoryStorageComponentSaveData.c(6): Expected attribute call
15:36:14.791    SCRIPT    (W): Failed to load
15:36:14.791   SCRIPT    (W): Initializing default GameLib's game class.
```

The unprefixed `path(line): message` line is a **restatement of the first error only** — the parser must not double-count it. The module name in `Can't compile "Game" script module!` varies by module; do not hardcode `Game`.

**Error truncation:** the engine caps its list and emits a final entry whose message is literally `Too many errors`:

```
15:36:14.741    SCRIPT    (E): @"Scripts/Game/Entities/EPF_TimeAndWeatherSaveData.c,7": Too many errors
```

**There is no "Compilation failed" string and no error-count summary anywhere in any log on this machine.** Greps for `N error(s)` / `warning(s)` returned zero matches. The verdict must be assembled, not read off a summary line.

### Success evidence markers (confirmed)

There is likewise no explicit success string. Compilation succeeding is indicated by the absence of `(E)` lines **plus** these affirmative markers:

```
15:37:55.087    SCRIPT       : Module: Game; loaded 5630x files; 10894x classes; ...
                PROFILING    : Compiling Game scripts took: 2129.348000 ms
```

These are Decision 2's third condition — the positive proof that compilation actually ran. The `PROFILING` line is `console.log`-only.

Workbench `console.log` also confirms which project was loaded, useful as a sanity assertion:

```
00:43:03.275    ENGINE       : dir: 'N:/Projects/Arma 4/Overthrow.Arma4/'
```

### Game-client launch pattern (confirmed from a real historical command line)

Recovered verbatim from a `crash.log` on this machine:

```
gproj N:/.../Arma Reforger/addons/data/ArmaReforger.gproj
addonsDir N:/Projects/Arma 4/Overthrow.Arma4/
addons EnfusionDatabaseFramework,EnfusionPersistenceFramework,Overthrow
window forceupdate nofocus client rpl-reconnect rpl-timeout-disable profile PeerTool1
```

Critically: the **game client's `-gproj` points at the base game's `ArmaReforger.gproj`**, and Overthrow is loaded via `-addonsDir` + `-addons`. Any assumption that the client takes Overthrow's own `addon.gproj` is wrong. (The Workbench's `-gproj`, by contrast, does take the addon project.)

BI's own default autotest launch parameters, from `SCR_AutotestFramework.c:23-26`:

```
-profile ArmaReforger/autotest -logLevel debug -noFocus -forceUpdate -noThrow -window
```

and `SCR_AutotestTool.c:87-104` builds `"<exe>" -gproj "<gproj>" -addonsDir <dirs> -addons <addons> -autotest <arg> <user args>` — the same shape. `-noThrow` and `-noFocus` are directly relevant to unattended, non-disruptive runs.

`SCR_AutotestReport.c:4-5` confirms the fixed artifact paths `$logs:/junit.xml` and `$logs:/autotest_failed.log`. **No autotest has ever run on this machine** — no `junit.xml` and no `ArmaReforger/autotest` profile exist — so feature #2's loop is entirely unproven and this feature's launcher is its only foothold.

---

## Architecture Overview

### The `tools/` layer

```
tools/
├── README.md              # The contract. Read by features #2, #4, #5 and by humans.
├── lib/
│   └── common.sh          # Sourced library. Owns the process boundary.
├── compile-check.sh       # Headline deliverable. Thin caller of common.sh.
└── launch-game.sh         # Game-client launcher for feature #2.

docs/features/dev-ops/workbench-automation/
└── findings.md            # Phase 1 empirical record. The epic's CLI reference.
```

Everything crosses the WSL→Windows boundary in exactly **one place** (`ovt_run_win` in `common.sh`). The two entry-point scripts are argument marshalling plus result interpretation; they never invoke an `.exe` path directly.

### Layer responsibilities

| Layer | Owns | Must not |
|---|---|---|
| `tools/lib/common.sh` | Path translation, binary/project resolution, env-var overrides, launching, timeout + kill, stale-process detection, profile/log-directory resolution, diagnostic output helpers | Know anything about compiling or testing |
| `tools/compile-check.sh` | Building the `-validate` argument list, capturing the log, parsing errors, the pass/fail/indeterminate verdict, exit-code contract | Reimplement launching or path translation |
| `tools/launch-game.sh` | Pass-through argument handling, profile pinning, resolving and reporting the run's log directory | Know anything about autotest, JUnit, or test semantics |
| `findings.md` | The observed truth about the CLI on a named Reforger build | Contain aspirations or untested claims |

### Data flow — compile check

```
WSL shell (repo root)
  └─ tools/compile-check.sh [--config X] [--timeout N]
       ├─ source tools/lib/common.sh
       ├─ resolve + validate: workbench exe, addon.gproj, profile name   ──► exit 2 if missing
       ├─ guard: is a Workbench already running?                         ──► exit 2 (unless --allow-concurrent)
       ├─ translate paths  /mnt/n/...  ──►  N:\...
       ├─ record t0, launch via ovt_run_win with timeout
       │     ArmaReforgerWorkbenchSteamDiag.exe
       │       -gproj "N:\...\addon.gproj" -validate <config>
       │       -wbsilent [-nofocus] -profile <pinned profile name>
       ├─ on timeout ──► kill process tree, verify dead ─────────────────► exit 124
       ├─ locate this run's console.log (pinned profile, newest-since-t0) ► exit 2 if not found
       ├─ parse:  @"path,line": message   ──► [{file, line, message}]
       │          "Too many errors"       ──► truncation flag
       │          Can't compile "X" module ──► failure marker
       │          Module: X; loaded Nx files / PROFILING: Compiling ──► evidence marker
       └─ verdict:
             exit code OK AND 0 in-project errors AND evidence found ──► summary, exit 0
             any parsed errors                                       ──► file:line: message, exit 1
             anything else (no evidence / unparseable / odd code)    ──► exit 2 "indeterminate"
```

### Data flow — game launcher

```
tools/launch-game.sh [--timeout N] [--profile NAME] -- <arbitrary args...>
  ├─ same resolution + translation + timeout machinery from common.sh
  ├─ launch ArmaReforgerSteamDiag.exe with pass-through args (quoting preserved)
  ├─ resolve the run's log directory under the pinned profile
  └─ stdout (KEY=value, source-able):
        PROFILE_DIR=/mnt/c/.../My Games/OverthrowCI
        LOG_DIR=/mnt/c/.../My Games/OverthrowCI/logs/logs_2026-08-01_12-30-00
        LOG_DIR_WIN=C:\...\My Games\OverthrowCI\logs\logs_2026-08-01_12-30-00
        EXIT_CODE=0
        DURATION_S=118
```

Feature #2 reads `LOG_DIR` and collects `$LOG_DIR/junit.xml`. That is the whole integration surface — no shared code, no coupling to test semantics.

### Contracts exposed to sibling features

| Consumer | Contract it depends on |
|---|---|
| #2 `autotest-foundation` | `tools/launch-game.sh -- <args>`; `LOG_DIR=` on stdout; timeout → 124; arbitrary args passed through unmangled; the `-gproj`/`-addonsDir`/`-addons` client launch shape handled for it |
| #4 `ci-pipeline` | Stable command names `tools/compile-check.sh` / `tools/launch-game.sh`; exit codes 0/1/2/124; `file:line: message` on stdout; `--timeout`; stale-process cleanup; `OVERTHROW_*` env overrides so no machine paths land in workflow files; captured log as an attachable artifact |
| #5 `release-automation` | `source tools/lib/common.sh` then `ovt_run_workbench <timeout> -gproj ... -packAddon ...`; `ovt_win_path`; profile-dir resolution (pack output lands under the Workbench profile's `publish/`) |

### Environment / configuration contract

Follows the existing `.scripts/*.sh` precedent (`${OVERTHROW_SAVE_DIR:-<default>}`):

| Variable | Default (this machine) |
|---|---|
| `OVERTHROW_WORKBENCH_EXE` | `/mnt/n/Program Files (x86)/Steam/steamapps/common/Arma Reforger Tools/Workbench/ArmaReforgerWorkbenchSteamDiag.exe` |
| `OVERTHROW_GAME_EXE` | `/mnt/n/Program Files (x86)/Steam/steamapps/common/Arma Reforger/ArmaReforgerSteamDiag.exe` |
| `OVERTHROW_GAME_GPROJ` | `/mnt/n/.../Arma Reforger/addons/data/ArmaReforger.gproj` (client only — see Decision 12) |
| `OVERTHROW_GPROJ` | `<repo root>/addon.gproj` (Workbench only) |
| `OVERTHROW_MYGAMES_DIR` | `/mnt/c/Users/<user>/OneDrive/Documents/My Games` |
| `OVERTHROW_PROFILE_NAME` | e.g. `OverthrowCI` — the pinned profile (Decision 5) |
| `OVERTHROW_COMPILE_TIMEOUT` | Set in Phase 1 from measured cold/warm timings |
| `OVERTHROW_SCRIPT_CONFIG` | Set in Phase 1 (see Decision 9) |

Plus an optional, gitignored `tools/config.local.sh` sourced if present, for per-machine overrides without env fiddling.

### Naming conventions

- Shell functions: `ovt_` prefix (mirrors the codebase's `OVT_`, short enough for shell).
- Environment variables: `OVERTHROW_` prefix (matches the existing `.scripts/` precedent).
- `#!/bin/bash`, `set -euo pipefail`, explicit `exit N`, quoted paths everywhere (every real path on this machine contains spaces).

---

## Implementation Phases

### Phase 1: Empirical CLI Verification (Foundation) — **REQUIRES ADVANCED (MAX-EFFORT) AGENT**

**Goal:** Replace every remaining assumption about the Workbench CLI with an observed fact, recorded in `findings.md`. Nothing is built until this is done.

> **This phase is the crux of the feature.** Every design decision in Phases 2-5 hangs off it, and several will change if observations differ from expectation. It is also the phase most likely to surface an unpleasant surprise (a launcher shim that detaches, a Steam login prompt, an exit code that is always 0, or `-validate` writing its output somewhere entirely different). It needs an agent working slowly, one flag at a time, writing down exactly what happened — including the boring parts and especially the inconvenient ones.

**Tasks:**

- [ ] 1.1 Create `docs/features/dev-ops/workbench-automation/findings.md` with a fixed table shape: *flag/command → observed exit code (raw `$?`) → wall time → window/focus behaviour → log location → notes*. Record the exact Reforger and Tools build numbers at the top, and **which Workbench branch** (stable vs Experimental — they use different profile roots). Findings are only valid for a named build.
- [ ] 1.2 **Baseline launch.** `ArmaReforgerWorkbenchSteamDiag.exe -gproj "<win path>/addon.gproj"` alone. Does it open a GUI? Does the shell block until exit? Does `$?` come back meaningfully? How long? Kill it manually if needed.
- [ ] 1.3 **Detachment check (highest-risk unknown).** Determine whether `*SteamDiag.exe` is a launcher shim that spawns a child and returns immediately. Compare the shell's wall time against process lifetime in `tasklist.exe`. If it detaches, `$?` is worthless and the wrapper must poll for process exit and read the verdict from the log — record this explicitly; it changes Phases 2 and 3.
- [ ] 1.4 **`-validate` matrix.** Run bare `-validate`, `-validate PC`, and `-validate workbench` (Overthrow's `addon.gproj` declares `GameProjectConfig PC` containing `ScriptConfigurationClass workbench`; the wiki's wording does not disambiguate). Record which are accepted, which error, and what each returns.
- [ ] 1.5 **`-validate` against a broken tree.** Introduce a guaranteed EnforceScript compile error in a scratch file (a ternary operator is the cheapest reliable one) and re-run the accepted variants. Record the raw exit code — **explicitly note whether the documented `-1` arrives as `255`** through WSL interop. Remove the scratch file and confirm the clean run differs.
- [ ] 1.6 **Confirm the known error format holds under `-validate`.** The `@"path,line": message` / `Can't compile "X" script module!` / `Too many errors` shapes and the `Module: X; loaded Nx files` + `PROFILING: Compiling X scripts took:` success markers are already captured verbatim (see Verified Ground Truth) — **but from interactive GUI sessions, not from `-validate`**. Confirm they appear identically, in the same file, under `-validate`. If `-validate` emits to stdout, to a different file, or in a different shape, that supersedes the samples above and Phase 3's parser is written against the new evidence.
- [ ] 1.7 **Non-disruption flags.** Test `-wbsilent` (documented: initialises engine + Workbench modules and exits without opening any windows), `-exitAfterInit`, `-noFocus`/`-nofocus`, and `-noThrow` (BI uses it in their own autotest defaults; presumably suppresses exception dialogs). For each: does a window appear, is focus stolen, does validation still happen, what is the exit code. Determine the **minimal flag set that validates with no visible window**.
- [ ] 1.8 **Profile pinning.** Confirm `-profile <Name>` creates/uses `<My Games>/<Name>/` and that `logs/logs_<timestamp>/` lands there (strongly implied by the existing `PeerTool1`, `OverthrowDedi` profiles and BI's `-profile ArmaReforger/autotest`). Test whether a nested name (`Overthrow/ci`) and whether an absolute path are accepted. Confirm behaviour for **both** binaries. This determines whether log resolution is deterministic (Decision 5).
- [ ] 1.9 **Timing.** Measure a cold run (first after a Tools update, or after clearing the resource cache if safe) and three warm runs. These set the default timeout. Record whether a resource-database rebuild happens and its cost.
- [ ] 1.10 **Interop and process control.** Verify: does bash `timeout` terminate the *Windows* process or only the Linux-side interop stub? Does `taskkill.exe /F /T /PID` work from WSL? Can the launched process's Windows PID be captured? Does `tasklist.exe` reliably report a running Workbench?
- [ ] 1.11 **Environmental prerequisites.** Does the run require Steam running/logged in? Any dialog? What happens with Steam closed? (Note: **only one Workbench binary exists** — `ArmaReforgerWorkbenchSteamDiag.exe`, plus `CrashReporter.exe`; there is no non-Steam variant, so this dependency cannot be engineered away.)
- [ ] 1.12 **Concurrency.** Run `-validate` while a Workbench GUI session is open on the same project. Record what happens — refusal, resource-database contention, or apparent success. Determines whether Phase 5's guard is a hard refusal.
- [ ] 1.13 **Truncation threshold.** Introduce enough errors to trigger `Too many errors` and record roughly where the cap falls, so Phase 3 can report truncation meaningfully ("at least N errors; the engine stopped listing").
- [ ] 1.14 **Game client.** Launch `ArmaReforgerSteamDiag.exe` using the confirmed shape — `-gproj <base game ArmaReforger.gproj> -addonsDir <repo parent> -addons EnfusionDatabaseFramework,EnfusionPersistenceFramework,Overthrow -profile <name> -noFocus -window` — and confirm it starts, that the pinned profile's log directory appears, and that `-autotest` is at least *accepted*. Running a real suite is feature #2's job, but BI's shipped `SCR_TEST_Example1TestSuite` is a free end-to-end probe if it works.
- [ ] 1.15 **Write up divergences.** A dedicated "Differs from assumptions" section in `findings.md`, naming anything that contradicts the wiki, the epic overview's research table, the Verified Ground Truth section above, or this plan.

**Estimated Time:** 4-6 hours (dominated by launch wall-time and careful recording, not typing)

**Acceptance Criteria:**
- [ ] `findings.md` exists and every flag in tasks 1.2-1.14 has an observed result — including those that did nothing or failed.
- [ ] The exact command line producing a clean compile check with no visible window is written down and reproducible.
- [ ] Raw exit codes for both a passing and a failing compile are recorded as literal integers as seen from bash.
- [ ] It is settled whether the documented error/success log shapes hold under `-validate`, and in which file.
- [ ] Profile-pinning behaviour is settled for both binaries.
- [ ] Cold and warm timings are recorded and a default timeout proposed from them.
- [ ] A "Differs from assumptions" section exists (even if it says "nothing").

> **Gate:** Before Phase 2 begins, re-read Phases 2-6 against `findings.md` and amend them. If `-validate` proves unusable, Decision 1's fallback (log-derived verdict) is activated here, not improvised later.

> **✅ GATE APPLIED 2026-08-01 — Phase 1 complete. Amendments to Phases 2–6 (full evidence in `findings.md`):**
> 1. **`-wbsilent -validate` is the shape** — without `-wbsilent` the GUI opens and never exits; `-exitAfterInit` returns 0 on a broken tree (unusable). Exit codes ARE real: 0 pass / 255 fail (wiki's -1 as 255). Workbench does NOT detach; `$?` is trustworthy.
> 2. **The `-validate [configName]` argument is ignored** (PC/workbench/bogus identical) — Decision 9 amended: no `--config` plumbing needed; plain `-validate`. The gproj's `workbench` defines apply automatically, i.e. the check always validates WITH `WORKBENCH`/`PERSISTENCE_DEBUG` defined (R12 blind spot stands, documented).
> 3. **`-addonsDir` is mandatory** (base addons NOT auto-discovered): comma-separated single flag, one level deep. Canonical deps: packed workshop EPF/EDF in `My Games/ArmaReforgerWorkbench/addons` + `<game>/addons`. EPF/EDF SOURCE repos do not compile on 1.7.0.54 — use packed.
> 4. **FALSE-PASS TRAP (new hard requirement for Phase 3):** unresolvable deps make Workbench silently validate the base game and exit 0. The parser MUST require `Overthrow.Arma4/addon.gproj` in the `Loaded addons:` block as a fourth verdict condition (Decision 2 extended to four conditions).
> 5. **bash `timeout` kills only the interop stub** — the Windows process survives. `ovt_run_win` must taskkill by PID (`tasklist.exe /FI ... /FO CSV /NH` → `taskkill.exe /F /T /PID`) and verify death (confirms tasks 2.4/2.5 as designed).
> 6. **Game client exit code is ALWAYS 0** (even on fatal errors) — task 4.7 amended: report `EXIT_CODE` but document it as meaningless; outcomes come from logs/artifacts. **Client requires cwd = game install dir** (finds `core` via `./addons`) — task 4.2 amended.
> 7. **`-autotest` works** (first autotest ever run on this machine): `{GUID}` group form produced `junit.xml`; `SCR_TEST_Example1TestSuite` is `#ifdef WORKBENCH`-guarded and invalid in retail — task 4.8 uses `-autotest "{6AB9C8EEE9A651B5}"` (shipped empty group) as the smoke test.
> 8. **Timings:** warm ~3.4s; one-off first-ever project scan ~56s. Default `OVERTHROW_COMPILE_TIMEOUT`: 120s.
> 9. **Tree fixed to compile (user-approved scope addition):** `OVT_Component.c` generic method → `OVT_ComponentFinder<Class T>` class; `_OVT_*Template.c` reference files moved to `docs/features/vanilla-persistence/templates/`; `OVT_PersistenceManagerComponent.c` fictional API calls (TriggerSave/GetOrCreate/DB_BASE_DIR) stubbed with `TODO(vanilla-persistence)`. Canonical command now returns **exit 0** on the clean tree.

---

### Phase 2: `tools/lib/common.sh` — the shared library

**Goal:** One place that crosses the process boundary, and one place that knows where anything lives.

**Tasks:**

- [ ] 2.1 Create `tools/` and `tools/lib/`; add `tools/config.local.sh` and the tool's `.tmp/` outputs to `.gitignore` (`.tmp/` is already ignored).
- [ ] 2.2 Path translation: `ovt_win_path` / `ovt_wsl_path` wrapping `wslpath -w` / `wslpath -u`. Must handle spaces, trailing slashes, and non-existent paths (`wslpath` behaviour on missing paths is inconsistent — normalise it). Reject silently-wrong output rather than passing it on.
- [ ] 2.3 Resolution + validation: `ovt_workbench_exe`, `ovt_game_exe`, `ovt_project_gproj`, `ovt_game_gproj`, `ovt_mygames_dir`, `ovt_profile_dir <name>` — each honouring its `OVERTHROW_*` override, falling back to the documented default, failing with a specific actionable message (exit 2) if the target does not exist. `ovt_mygames_dir` must **discover** the OneDrive-redirected Documents path rather than constructing `%USERPROFILE%\Documents`.
- [ ] 2.4 `ovt_run_win <timeout_s> <exe> [args...]` — the single boundary crossing. Launches, enforces the timeout, normalises the exit code (any non-zero is failure; never compare against `-1`), returns 124 on timeout. Shaped by findings 1.3 and 1.10 — if the exe detaches, this polls for process exit rather than trusting `$?`.
- [ ] 2.5 Process control: `ovt_is_running <image.exe>` (via `tasklist.exe`) and `ovt_kill_tree` (via `taskkill.exe /F /T`, PID-scoped where possible, image-scoped as fallback). After killing, **verify** the process is gone and report exit 2 if not.
- [ ] 2.6 `ovt_run_workbench <timeout> [args...]` and `ovt_run_game <timeout> [args...]` — trivial verb-agnostic wrappers over 2.4. This is feature #5's entire integration point: a new Workbench verb is a new argument list, not new plumbing.
- [ ] 2.7 Log-directory resolution: `ovt_resolve_log_dir <profile_name> <since_epoch>` — newest `logs/logs_*` directory under the pinned profile modified after `since_epoch`. Returns empty and non-zero if nothing matches; callers treat that as indeterminate, never as success.
- [ ] 2.8 Output helpers `ovt_info` / `ovt_warn` / `ovt_err` — all to **stderr**, so stdout stays clean for the machine-readable contract.
- [ ] 2.9 A small self-check path (`tools/lib/common.sh --self-test` or equivalent) asserting path round-tripping on tricky inputs (spaces, `N:` vs `C:`, trailing slash) and that the My Games directory resolves. Path bugs are silent; this is cheap insurance.

**Estimated Time:** 2-3 hours

**Acceptance Criteria:**
- [ ] `source tools/lib/common.sh` works from any working directory; the library resolves the repo root itself.
- [ ] Self-test passes for path translation including paths with spaces, and for OneDrive-redirected My Games discovery.
- [ ] Every resolver fails with a specific message and exit 2 when its target is missing — never a bare "command not found".
- [ ] `ovt_run_win` returns 124 and leaves no surviving Windows process when a timeout fires (verified with `tasklist.exe`).

---

### Phase 3: `tools/compile-check.sh` — verdict and error parsing — **ADVANCED AGENT RECOMMENDED**

**Goal:** The headline deliverable. Correct verdicts, honest exit codes, structured errors.

> The pass/fail/indeterminate logic here is where a false green would be born. Worth extra care even though the code is short.

**Tasks:**

- [ ] 3.1 CLI surface: `tools/compile-check.sh [--config <name>] [--timeout <s>] [--all] [--absolute] [--keep-log] [--verbose] [--allow-concurrent] [-h|--help]`. Defaults come from env vars, which come from Phase 1's findings.
- [ ] 3.2 Assemble the argument list established in Phase 1 (`-gproj`, `-validate <config>`, `-profile <pinned>`, plus the minimal non-disruptive flag set) and invoke `ovt_run_workbench`.
- [ ] 3.3 Copy the run's `console.log` to `.tmp/compile-check/last.log` (gitignored) so a failed run is always post-mortem-able and feature #4 has an artifact to attach.
- [ ] 3.4 **Parser**, written against the verbatim samples (Verified Ground Truth, re-confirmed by finding 1.6):
  - Match `SCRIPT` + `(E):` + `@"<path>,<line>": <message>` → `{file, line, message}`. Whitespace-tolerant between tag and severity.
  - Detect the failure marker `Can't compile "<Module>" script module!` — module name not hardcoded.
  - **Do not double-count** the unprefixed `path(line): message` restatement that follows the failure marker; it repeats the first error only.
  - Detect the `Too many errors` sentinel and set a truncation flag.
  - Any `(E):` line that looks like an error but matches no known shape is surfaced as an unparsed warning, never dropped.
- [ ] 3.5 **Evidence markers.** Require `Module: <X>; loaded Nx files` and/or `PROFILING : Compiling <X> scripts took:` to conclude compilation actually ran. Absence means indeterminate.
- [ ] 3.6 **Verdict logic** — all three must hold for exit 0: acceptable exit code, zero in-project errors parsed, evidence marker present. Otherwise: errors present → exit 1; anything else → exit 2 with a one-line reason and a pointer to the captured log.
- [ ] 3.7 **Scope filtering.** Errors originating outside the project (base game, EPF) are filtered by default so they cannot make the check permanently red — but **never silently**: always print `(N errors outside the project; re-run with --all to see them)`. `--all` disables filtering.
- [ ] 3.8 **Truncation reporting.** When the `Too many errors` sentinel is present, the summary must say so explicitly — e.g. `compile-check: FAILED (22+ errors, engine stopped listing) in 47s`. Never present a truncated count as complete.
- [ ] 3.9 **Output format.** Errors to stdout, one per line, gcc-style: `Scripts/Game/Components/OVT_Foo.c:214: error: unexpected token '?'`. Repo-relative by default (the engine already emits addon-relative paths, so this is close to a straight transform), absolute WSL paths with `--absolute`. Summary line to stderr.
- [ ] 3.10 Exit-code taxonomy implemented and documented in `--help`:

| Code | Meaning |
|---|---|
| 0 | Compiled clean — verified, not assumed |
| 1 | Compilation failed — at least one parsed in-project error |
| 2 | Tool/environment failure or indeterminate result |
| 124 | Timed out (GNU `timeout` convention) |

**Estimated Time:** 3-4 hours

**Acceptance Criteria:**
- [ ] Clean tree → exit 0 with no stdout error lines.
- [ ] Deliberately broken tree → exit 1 with the correct `file:line: message` for every introduced error, each reported exactly once.
- [ ] Missing binary / missing project → exit 2 with a specific message, never 0 or 1.
- [ ] Empty or truncated log → exit 2 "indeterminate", never 0.
- [ ] A run that trips `Too many errors` says so in the summary.
- [ ] `--help` documents every flag and every exit code.

---

### Phase 4: `tools/launch-game.sh` — the game-client launcher

**Goal:** Feature #2's dependency, delivered without knowing anything about tests.

**Tasks:**

- [ ] 4.1 CLI surface: `tools/launch-game.sh [--timeout <s>] [--profile <name>] [--quiet] [-h|--help] -- <args...>`. Everything after `--` is passed through verbatim, quoting preserved (arguments containing spaces must survive the WSL→Windows boundary intact — test explicitly).
- [ ] 4.2 Supply the confirmed client launch shape by default — `-gproj <base game ArmaReforger.gproj> -addonsDir <repo parent dir> -addons EnfusionDatabaseFramework,EnfusionPersistenceFramework,Overthrow -profile <name>` — so callers only pass what is specific to their run (e.g. `-autotest X`). Every default must be overridable; caller-supplied duplicates take precedence.
- [ ] 4.3 Adopt the non-disruptive subset of BI's own autotest defaults (`-noFocus`, `-noThrow`, `-window`, `-logLevel debug`) as defaults, overridable — they are the parameters BI ships for exactly this purpose.
- [ ] 4.4 Record `t0`, launch via `ovt_run_game` with the timeout.
- [ ] 4.5 Resolve the run's log directory via `ovt_resolve_log_dir` under the pinned profile. If it cannot be resolved, exit 2 — a launcher that reports the wrong log directory is worse than one that admits it does not know.
- [ ] 4.6 Emit the `KEY=value` stdout contract (`PROFILE_DIR`, `LOG_DIR`, `LOG_DIR_WIN`, `EXIT_CODE`, `DURATION_S`); all other output to stderr.
- [ ] 4.7 Propagate the client's exit code (same normalisation, 124 on timeout), except where the contract requires a distinct tool-failure code.
- [ ] 4.8 End-to-end smoke using BI's shipped example suite if Phase 1 showed `-autotest` is accepted: `tools/launch-game.sh -- -autotest SCR_TEST_Example1TestSuite`, then confirm `junit.xml` exists under the reported `LOG_DIR`. This validates feature #2's entire dependency without writing a single Overthrow test — and would be the **first autotest ever run on this machine**.

**Estimated Time:** 2-3 hours

**Acceptance Criteria:**
- [ ] Arbitrary arguments, including ones containing spaces, reach the client unmangled.
- [ ] `LOG_DIR` on stdout points at the directory the run actually wrote to, verified by inspecting a file inside it.
- [ ] Timeout → exit 124, no surviving client process.
- [ ] stdout contains only `KEY=value` lines and is safe to `source` or `grep`.

---

### Phase 5: Robustness, cleanup and the contract document

**Goal:** Make it safe to run on a machine someone is using, and on an unattended runner.

**Tasks:**

- [ ] 5.1 **Concurrency guard.** Detect an already-running Workbench before launching. Default behaviour follows finding 1.12: refuse with exit 2 and an actionable message if concurrent runs are harmful; warn only if benign. `--allow-concurrent` overrides.
- [ ] 5.2 **Stale-process cleanup.** Optional pre-run sweep for orphans from a previous run (feature #4 explicitly needs this). Opt-in, or scoped tightly enough that it cannot kill a developer's open Workbench without asking.
- [ ] 5.3 **Signal handling.** `trap` on INT/TERM so Ctrl-C kills the Windows process too rather than orphaning it.
- [ ] 5.4 **Timeout hardening.** Verify the kill worked; escalate to `/F`; report exit 2 if cleanup fails so a runner surfaces the problem instead of silently accumulating zombies.
- [ ] 5.5 **`stage_dev.sh` exclusion.** `.scripts/stage_dev.sh` copies everything not beginning with `.` into `Overthrow.Dev`, so a visible `tools/` would be staged into — and potentially published with — the addon. Add `tools` to its exclusion list, and note it in `tools/README.md` for feature #5's packing step.
- [ ] 5.6 **Write `tools/README.md`** — the contract: every command, flag and exit code, the stdout formats, the `OVERTHROW_*` variables, the log-directory resolution rule, and a "for feature #5: add a Workbench verb like this" section. This is what siblings read instead of the source.
- [ ] 5.7 Add a "known limitations" section to `tools/README.md`: Steam must be running; a Windows host with a GPU is required; the first run is slow; `-validate` covers compilation only, not runtime; findings are branch- and build-specific (stable vs Experimental Workbench use different profile roots).

**Estimated Time:** 2-3 hours

**Acceptance Criteria:**
- [ ] Running a compile check with the Workbench GUI open produces the documented behaviour, not a corrupted state or a confusing failure.
- [ ] Ctrl-C during a run leaves no Windows process behind.
- [ ] `.scripts/stage_dev.sh` no longer stages `tools/`.
- [ ] `tools/README.md` is complete enough that features #2/#4/#5 never need to open a `.sh` file.

---

### Phase 6: Documentation updates (Definition of Done)

**Goal:** Stop the project's docs asserting a constraint this feature just removed. Per the epic's documentation policy this is not optional and not deferred.

**Tasks:**

- [ ] 6.1 **`CLAUDE.md`** — rewrite the Development Workflow bullet ("You cannot compile/build the project…") and the Workbench constraint block ("No automated builds - User compiles in Workbench"). Replace with the real capability: the agent runs `tools/compile-check.sh` itself and reads the parsed errors. Keep "no unit tests" and "no debugger" — those are feature #2 and out of scope respectively.
- [ ] 6.2 **`docs/technical-design.md` §2 "What We Don't Have"** — rewrite the "**No automated builds.**" bullet (line ~82), which currently reads "Claude and CI can never verify that a change compiles." Now false. Follow the section's existing convention for entries invalidated by the epic.
- [ ] 6.3 **`docs/technical-design.md` §10 "Testing Strategy"** — amend item 1 of "What we do instead" ("**Compile in Workbench** — Build → Compile and Reload Scripts. The user does this; the assistant cannot.") to make the automated compile check the first gate, with the GUI as the interactive path. Do **not** touch "There is no test suite" — that belongs to feature #2.
- [ ] 6.4 **`docs/mission-statement.md` §"Automating the quality gate"** — update the closing paragraph ("Until that pipeline lands, the discipline is unchanged…") to record that the first stage has landed. Compile correctness is now machine work.
- [ ] 6.5 **`.claude/skills/workbench-workflow/SKILL.md`** — the largest change. Update "Testing Guidelines", "Critical Constraints" ("No automated builds"), the "Development Cycle" (steps 1-5 currently describe a human compile round-trip), "After Code Changes" / "After Compile Errors" (currently instruct the agent to *ask the user* for compile errors), and "Key Differences" ("No npm/build scripts"). Add a short section on running `tools/compile-check.sh` and reading its output. Note that this SKILL.md references four resource files that do not exist — do not create them, just do not add more dangling references.
- [ ] 6.6 **`docs/features/dev-ops/epic-overview.md`** — flip feature #1's status and refresh the rollup line.
- [ ] 6.7 Cross-check: grep the repo for any remaining assertion that compilation cannot be automated, and fix or flag what turns up.

**Estimated Time:** 1-2 hours

**Acceptance Criteria:**
- [ ] No file in the repo still claims the assistant/CI cannot verify that a change compiles.
- [ ] The `workbench-workflow` skill instructs the agent to run the compile check rather than ask the user to.
- [ ] Claims about *tests* and *debugging* are untouched — this feature did not deliver those, and docs must never describe capability that does not exist.

---

**Total estimated effort:** 14-21 hours, front-loaded into Phase 1.

---

## Key Technical Decisions

### Decision 1: `-validate` as the compile mechanism, with a log-derived fallback

**Context:** Something has to make the Workbench compile scripts and tell us whether it worked.
**Decision:** Use the documented `-validate [scriptConfigName]` flag as the primary mechanism. If Phase 1 shows it misbehaves (unrecognised, always-zero exit code, or no compilation actually performed), fall back to deriving the verdict entirely from `console.log` while keeping the wrapper's external contract identical.
**Rationale:** `-validate` is documented on the BI Community Wiki (*Arma Reforger: Startup Parameters*) as "checks if the game scripts are compilable and returns Workbench application return code of -1 when compilation failed and 0 when compilation was successful", with an optional script configuration name. That is exactly this feature's requirement, first-party and purpose-built. Building anything more elaborate before testing the thing designed for the job would be wasted work. The fallback is cheap because the log format is already known verbatim.
**Alternatives considered:** Log-parsing a plain `-gproj` launch (kept as the fallback); a WorkbenchPlugin (rejected, Decision 8); driving the GUI (rejected outright).

### Decision 2: Success must be positively proven — the three-condition rule

**Context:** The worst possible failure mode is a green compile check on a broken build. Compounding this: **the logs contain no "compilation succeeded" string and no error-count summary** — verified by grepping every log on this machine. A verdict has to be assembled from several weak signals.
**Decision:** `compile-check.sh` exits 0 only when **all three** hold: the process returned an acceptable exit code, **and** zero in-project errors were parsed, **and** an affirmative evidence marker was found (`Module: <X>; loaded Nx files` and/or `PROFILING : Compiling <X> scripts took:`). Anything else is exit 1 (errors found) or exit 2 (indeterminate).
**Rationale:** Each signal can fail independently and silently — an exit code can be swallowed by a launcher shim, a log can be written somewhere unexpected, a parser can stop matching after a Reforger update. Requiring agreement between an exit code and positive log evidence means any single-point failure degrades to a loud "could not determine" rather than a quiet pass. Distinguishing exit 2 from exit 1 also matters to feature #4: a broken runner and a broken build need different human responses.
**Alternatives considered:** Trusting the documented exit code alone (one silent failure away from a false green); trusting "no errors in log" alone (indistinguishable from "no compilation happened", which is exactly the false-green case).

### Decision 3: Pure bash + WSL interop, not PowerShell

**Context:** The toolchain is Windows; the agent and developer work in WSL.
**Decision:** Bash scripts in `tools/`, invoking the Windows `.exe` directly through WSL binfmt interop. PowerShell only if a specific step proves impossible otherwise.
**Rationale:** Interop is confirmed working; bash is where the caller already is; the repo already has four committed bash scripts in `.scripts/` establishing the conventions — `#!/bin/bash`, `${OVERTHROW_*:-default}` env overrides, explicit exits. A PowerShell layer would add a second language and a second quoting model at the exact boundary where quoting bugs live. `update-arma-scripts.ps1` remains the precedent if one step genuinely needs it — notably it uses native Windows paths with no WSL translation at all, which is precisely the ad-hoc approach this feature exists to replace.
**Alternatives considered:** PowerShell throughout (extra boundary, no benefit); Python (a dependency the project does not otherwise have in this path).

### Decision 4: One process boundary, in one function

**Context:** Four features will eventually launch Windows binaries from WSL.
**Decision:** All launching goes through `ovt_run_win` in `tools/lib/common.sh`; `ovt_run_workbench` and `ovt_run_game` are thin verb-agnostic wrappers. No entry-point script ever names an `.exe` path.
**Rationale:** The epic explicitly assigns the process boundary to this feature and forbids per-feature reimplementation. Concentrating it fixes timeout handling, kill semantics, exit-code normalisation and quoting once. It also makes feature #5 nearly free: `-packAddon` is a different argument list to `ovt_run_workbench`, not new plumbing.
**Alternatives considered:** Each script launching directly (four copies of the timeout/kill bug).

### Decision 5: Pin a dedicated `-profile` name rather than guess the newest log

**Context:** Artifacts (the compile log, and later `junit.xml` at the fixed path `$logs:/junit.xml`) must be attributed to a specific run.
**Decision:** Launch with `-profile <DedicatedName>` (e.g. `OverthrowCI`), so logs land under `<My Games>/<DedicatedName>/logs/logs_<timestamp>/`, isolated from every interactive session. Resolve the run's directory as the newest under that profile modified after a timestamp captured immediately before launch, and fail loudly if none appears.
**Rationale:** `-profile` taking a **relative profile name** (not a path) is confirmed by existing profiles on this machine (`PeerTool1`, `OverthrowDedi`, `OverthrowServer`) and by BI's own autotest default `-profile ArmaReforger/autotest`. A dedicated profile makes attribution near-deterministic: nothing else writes there, so "newest" cannot pick up an interactive Workbench session's log. It also sidesteps a real local hazard — the profile root lives under a **OneDrive-redirected** Documents folder (`/mnt/c/Users/<user>/OneDrive/Documents/My Games/...`), so any code constructing `%USERPROFILE%\Documents\...` would be wrong here and differently wrong on a CI runner. Additional benefit: a dedicated profile keeps automation runs from polluting the developer's own Workbench settings and save database.
**Alternatives considered:** Newest-across-all-profiles (racy, and would collide with the stable/Experimental/client profile roots); parsing the log for its own path (circular — you need the log to find the log).

### Decision 6: gcc-style `file:line: message` as the error format

**Context:** Errors must be readable by a human and by an agent, and eventually by GitHub Actions.
**Decision:** One error per line on stdout, `path/relative/to/repo.c:214: error: message`. Repo-relative by default. No JSON output in this feature.
**Rationale:** It is the most widely-understood machine-readable error format in existence: editors make it clickable, agents parse it without a schema, and a GitHub Actions problem-matcher or `::error file=,line=::` annotation is a one-line transform belonging to feature #4. It is also barely a transform at all — the engine already emits addon-relative paths in `@"path,line"` form, so the conversion is `,` → `:`. Adding a JSON mode now would be speculative — YAGNI.
**Alternatives considered:** JSON (no consumer yet); raw log passthrough (explicitly forbidden by the requirements).

### Decision 7: Exit-code taxonomy 0 / 1 / 2 / 124

**Context:** Callers must distinguish "your code is broken" from "the tool is broken".
**Decision:** 0 = verified clean, 1 = compile errors, 2 = tool/environment failure or indeterminate, 124 = timeout (GNU `timeout` convention). Any negative or unexpected code from Windows is normalised to a failure — never compared against `-1`.
**Rationale:** Feature #4 must fail a PR differently for "the author broke the build" versus "the runner lost Steam". 124 matching GNU `timeout` means existing tooling and human intuition already know what it means. The normalisation rule exists because a documented `-1` will most likely surface as `255` through WSL interop, and an equality check against `-1` would silently treat a failure as an unrecognised code.
**Alternatives considered:** Binary 0/1 (conflates broken build with broken runner — the exact ambiguity #4 must not have).

### Decision 8: No WorkbenchPlugin

**Context:** `WorkbenchPlugin.RunCommandline()` exists and could drive the Workbench from inside the engine.
**Decision:** Rejected. Not built, not prototyped, not kept as a fallback.
**Rationale:** It requires writing and maintaining EnforceScript that lives in the shipped addon purely to serve tooling, adds a compile-time dependency between the tooling and the very scripts it is meant to validate (a plugin that fails to compile cannot report that compilation failed), and solves a problem the documented `-validate` flag already solves. Straightforward YAGNI. Recorded here so it is not re-litigated mid-implementation.
**Alternatives considered:** Keeping it as a tertiary fallback — rejected; the log-parse fallback in Decision 1 is sufficient and far cheaper.

### Decision 9: Script configuration is explicit, and defaults to the shipping build

**Context:** `-validate` takes an optional script configuration name. Overthrow's `addon.gproj` declares `GameProjectConfig PC` containing a `ScriptConfigurationClass workbench` whose defines are `DEBUG_NAVMESH_REBUILD_AREAS`, `PLATFORM_WINDOWS`, `ENF_WB`, `WORKBENCH`, `PERSISTENCE_DEBUG`. The wiki describes the parameter as "a script configuration name … (Configurations can be found in project settings, usually PC, XBOX_SERIES, etc)" — which does not disambiguate between the two levels.
**Decision:** Phase 1 determines which name `-validate` accepts. The wrapper exposes `--config` / `OVERTHROW_SCRIPT_CONFIG` and defaults to whichever configuration corresponds to the **shipping** build, not the developer configuration.
**Rationale:** The `workbench` configuration defines `WORKBENCH` and `PERSISTENCE_DEBUG`. Validating only under it would compile code paths that never ship while skipping `#ifndef WORKBENCH` paths that do — a green check that does not reflect what players run. `technical-design.md` §1 already warns that code behind these defines never runs on a shipped server; the compile check must not inherit that blind spot. Validating both configurations is a possible later refinement for feature #4, deliberately not built now.
**Alternatives considered:** Hardcoding one name (brittle and possibly wrong); always validating both (doubles runtime for a benefit nobody has asked for yet).

### Decision 10: `-wbsilent` for non-disruptive launching

**Context:** The requirement forbids stealing focus or leaving GUI windows on a machine someone is working on.
**Decision:** Use `-wbsilent` as the primary non-disruption flag for the Workbench, with `-noFocus`, `-noThrow` and `-exitAfterInit` as verified additions if Phase 1 shows they help. For the game client, adopt BI's own autotest defaults (`-noFocus -noThrow -window -logLevel debug`).
**Rationale:** `-wbsilent` is documented as "initialises the engine, workbench modules and exits without opening any windows; can be used to validate engine/game initialisation and script compilation" — precisely the described use case. `-exitAfterInit` ("makes Workbench automatically exit once it is completely initialised and all startup parameters are executed") is the termination guarantee if `-wbsilent` alone does not exit. `nofocus` is corroborated both as a string in the installed binary and in a real historical Overthrow launch command line recovered from a local crash log. The client-side set is not invented — it is what BI ships as `SCR_AutotestHelper.GetDefaultLaunchParams()`.
**Alternatives considered:** Launching minimised or on a virtual desktop (fragile, platform-specific, unnecessary if the documented flag works).

### Decision 11: `tools/` at the repo root, alongside the existing `.scripts/`

**Context:** The repo already has four committed bash scripts under `.scripts/` (save management, dev staging).
**Decision:** New, visible `tools/` directory. `.scripts/` is left untouched.
**Rationale:** The two have different audiences. `.scripts/` holds personal developer conveniences; `tools/` is the automation contract surface referenced by CI workflow files, by `tools/README.md`, and by three sibling features — it should be discoverable, not hidden behind a dot. **Consequence worth knowing:** `.scripts/stage_dev.sh` copies everything *not* starting with `.` into the staged dev build, so a visible `tools/` would be staged (and potentially published) with the addon unless excluded — handled in task 5.5, and flagged for feature #5's packing step.
**Alternatives considered:** Putting the scripts in `.scripts/` (hidden from the people and systems that need them, and mixes throwaway conveniences with a stable contract); `.tools/` (hidden, same problem).

### Decision 12: The game client is launched via `-addonsDir`/`-addons`, not via Overthrow's `addon.gproj`

**Context:** It would be natural to assume both binaries take `-gproj <Overthrow addon.gproj>`. They do not.
**Decision:** `launch-game.sh` defaults to `-gproj <base game ArmaReforger.gproj> -addonsDir <repo parent dir> -addons EnfusionDatabaseFramework,EnfusionPersistenceFramework,Overthrow`. `compile-check.sh` keeps `-gproj <Overthrow addon.gproj>` for the Workbench.
**Rationale:** Confirmed from a real historical Overthrow client launch recovered from a local `crash.log`, and corroborated by `SCR_AutotestTool.c:87-104`, which builds exactly this shape (`-gproj`, `-addonsDir`, `-addons`, then `-autotest`). Getting this wrong would produce a client that launches without the mod loaded — and an autotest run in feature #2 that fails for reasons having nothing to do with the tests. Encoding it as a default here means feature #2 never has to discover it.
**Alternatives considered:** Requiring the caller to supply the full argument set (pushes this trap onto feature #2, which is exactly what this feature exists to prevent).

---

## Definition of Done

All criteria below must pass. They are written to be verifiable by an evaluator with no implementation context.

### Functional Criteria

- [ ] **F1.** From a WSL shell at `/mnt/n/Projects/Arma 4/Overthrow.Arma4`, `tools/compile-check.sh` runs to completion with no GUI interaction and no prompts.
- [ ] **F2.** On an unmodified tree it exits **0** and prints no error lines on stdout.
- [ ] **F3.** With a deliberately introduced compile error (e.g. a ternary operator, which EnforceScript rejects), it exits **non-zero (1)** and prints that error on stdout in the form `path/to/file.c:LINE: message`, naming the correct file and the correct line.
- [ ] **F4.** With errors in **two different files**, both are reported, each on its own line, each exactly once.
- [ ] **F5.** After removing the deliberate error, a re-run exits **0** again — the check is not sticky.
- [ ] **F6.** `tools/compile-check.sh --help` documents every flag and every exit code.
- [ ] **F7.** `tools/launch-game.sh -- -autotest <SuiteName>` launches the game client with Overthrow loaded and prints a `LOG_DIR=` line on stdout naming a directory that exists and contains that run's log files.
- [ ] **F8.** `tools/launch-game.sh` passes arbitrary arguments through unmangled, including arguments containing spaces.
- [ ] **F9.** `docs/features/dev-ops/workbench-automation/findings.md` exists and records, for each flag tested, the observed exit code, wall time, window/focus behaviour and log location — including a "Differs from assumptions" section.

### Quality Criteria

- [ ] **Q1. No false greens.** With the log made unreadable or emptied, the tool exits **2** with an "indeterminate" message. It never exits 0 without positive evidence that compilation ran.
- [ ] **Q2. Tool failure is distinguishable from build failure.** `OVERTHROW_WORKBENCH_EXE=/nonexistent tools/compile-check.sh` exits **2** with a specific message naming the missing path — not 0 and not 1.
- [ ] **Q3. Timeouts work.** `tools/compile-check.sh --timeout 5` on a run known to take longer exits **124** within a few seconds of the deadline, and `tasklist.exe` afterwards shows **no** surviving `ArmaReforgerWorkbenchSteamDiag.exe` started by that run.
- [ ] **Q4. No GUI disruption.** During a normal compile check, no Workbench window appears and keyboard focus is not taken from whatever the evaluator is typing in.
- [ ] **Q5. No stale processes.** Three consecutive compile checks leave zero orphaned Workbench processes; Ctrl-C during a run also leaves none.
- [ ] **Q6. Determinism.** Three consecutive runs on the same unmodified tree produce identical verdicts and identical stdout.
- [ ] **Q7. Clean-environment safe.** `env -i /bin/bash -lc 'cd "/mnt/n/Projects/Arma 4/Overthrow.Arma4" && tools/compile-check.sh'` behaves identically to a run from an interactive shell.
- [ ] **Q8. Concurrency handled.** With a Workbench GUI already open, the check produces the behaviour documented in `tools/README.md` — never a confusing failure or a corrupted project state.
- [ ] **Q9. Nothing is silently hidden.** When dependency (base game / EPF) errors exist, the summary states how many were filtered and how to see them. When the engine emits `Too many errors`, the summary says the count is incomplete.
- [ ] **Q10. Automation does not pollute the developer's environment.** Runs write to a dedicated profile, not the interactive Workbench profile; the developer's settings and save database are untouched.

### Integration Criteria

- [ ] **I1.** `tools/README.md` documents, for each command: name, flags, exit codes, stdout format, stderr behaviour, and the `OVERTHROW_*` environment variables.
- [ ] **I2.** The log-directory resolution rule is documented well enough that feature #2 can collect `$LOG_DIR/junit.xml` without reading any script source.
- [ ] **I3.** `tools/README.md` shows how to add a new Workbench verb (feature #5's `-packAddon` / `-publishAddon*`) via `ovt_run_workbench` without new launch plumbing.
- [ ] **I4.** All machine-specific paths are overridable by environment variable, so feature #4's runner needs no committed absolute paths in workflow files.
- [ ] **I5.** `.scripts/stage_dev.sh` excludes `tools/` from the staged dev build.
- [ ] **I6.** `tools/lib/common.sh` can be sourced standalone and its path-translation helpers used without side effects.

### Documentation Criteria

- [ ] **D1. `CLAUDE.md`** — the Development Workflow bullet and the "No automated builds" constraint are corrected; the agent is told it can run the compile check itself.
- [ ] **D2. `docs/technical-design.md` §2** — the "No automated builds" entry no longer claims "Claude and CI can never verify that a change compiles."
- [ ] **D3. `docs/technical-design.md` §10** — "What we do instead" item 1 no longer says "The user does this; the assistant cannot."
- [ ] **D4. `docs/mission-statement.md`** — "Automating the quality gate" reflects that the compile stage has landed.
- [ ] **D5. `.claude/skills/workbench-workflow/SKILL.md`** — constraints, development cycle and the "ask the user for compile errors" guidance are updated to run the compile check instead.
- [ ] **D6.** Claims about **tests** and the **debugger** are left intact — this feature delivered neither, and no doc may describe capability that does not exist.

### Verification Method

An independent evaluator should be able to follow these steps on the dev machine with Steam running:

1. Open a WSL shell and `cd "/mnt/n/Projects/Arma 4/Overthrow.Arma4"`.
2. Confirm the tree is clean (`git status`). Run `time tools/compile-check.sh`; check the exit code with `echo $?`. **Expect 0**, no stdout error lines, and a summary on stderr. → F1, F2
3. Run it twice more. **Expect identical output and exit code each time.** → Q6
4. While step 3 runs, type in another window. **Expect no window to appear and no focus change.** → Q4
5. Create `Scripts/Game/OVT_CompileCheckCanary.c` containing a class with one method whose body uses a ternary (`int x = 1 > 0 ? 1 : 0;`). Run `tools/compile-check.sh; echo $?`. **Expect exit 1** and a stdout line naming `Scripts/Game/OVT_CompileCheckCanary.c` and the correct line number, appearing exactly once. → F3
6. Add a second broken file and re-run. **Expect both files reported.** → F4
7. Delete both files and re-run. **Expect exit 0.** → F5
8. Run `OVERTHROW_WORKBENCH_EXE=/nonexistent tools/compile-check.sh; echo $?`. **Expect exit 2** and a message naming the missing path. → Q2
9. Run `tools/compile-check.sh --timeout 5; echo $?`. **Expect exit 124** promptly. Then run `tasklist.exe | grep -i armareforgerworkbench`. **Expect no matching process.** → Q3, Q5
10. Start a run and press Ctrl-C after ~10 seconds. Check `tasklist.exe` again. **Expect no orphaned process.** → Q5
11. Open the Workbench GUI on the project, then run `tools/compile-check.sh; echo $?`. **Expect the behaviour documented in `tools/README.md`** — most likely a clear refusal with exit 2. → Q8
12. Run `env -i /bin/bash -lc 'cd "/mnt/n/Projects/Arma 4/Overthrow.Arma4" && tools/compile-check.sh'; echo $?`. **Expect the same result as step 2.** → Q7
13. Confirm the runs wrote to a dedicated profile directory under `.../My Games/` and that the interactive `ArmaReforgerWorkbench/profile/` was not modified. → Q10
14. Run `tools/launch-game.sh -- -autotest SCR_TEST_Example1TestSuite`. **Expect** the client to launch with Overthrow loaded, the command to return, and a `LOG_DIR=` line on stdout. `ls` that directory; **expect** run log files (and, if the suite ran, `junit.xml`). → F7
15. Open `findings.md`. **Expect** a per-flag table of observed behaviour and a "Differs from assumptions" section. → F9
16. Open `tools/README.md`. **Expect** every command, flag, exit code and environment variable documented, plus the feature-#5 verb-extension note. → I1, I2, I3
17. Grep the repo for "cannot compile", "No automated builds", "the assistant cannot". **Expect** no remaining assertion that compilation cannot be automated; **expect** claims about tests and the debugger still present. → D1-D6

---

## Testing Strategy

There is no bash test framework in this repo and building one is out of scope. Validation is a fixed, written-down manual checklist — the same discipline the project already applies to gameplay changes.

### Compile-check correctness

| ID | Scenario | Expected |
|---|---|---|
| T1 | Clean tree | exit 0, no stdout errors |
| T2 | One deliberate ternary in a scratch file | exit 1, correct `file:line: message`, reported once |
| T3 | Errors in two files | exit 1, both reported |
| T4 | Error removed | exit 0 (not sticky) |
| T5 | Error inside a `#ifdef WORKBENCH` block | Behaviour matches the selected `--config`; documented either way (guards against Decision 9's blind spot) |
| T6 | `--all` with a dependency error present | Out-of-project errors shown; without `--all`, hidden but **counted** |
| T7 | Enough errors to trip `Too many errors` | Summary states the count is incomplete |
| T8 | The unprefixed `path(line): message` restatement line | First error not double-counted |

The canary file (`Scripts/Game/OVT_CompileCheckCanary.c`) is created and deleted by the test — never an edit to a real source file, so a forgotten revert cannot ship.

### Failure-mode and robustness

| ID | Scenario | Expected |
|---|---|---|
| T9 | `--timeout 5` on a longer run | exit 124, no surviving process |
| T10 | Ctrl-C mid-run | no orphaned process |
| T11 | Missing/incorrect binary path | exit 2, specific message |
| T12 | Missing `addon.gproj` | exit 2, specific message |
| T13 | Empty/unparseable log | exit 2 "indeterminate" — **never 0** |
| T14 | Steam not running | exit 2 with an actionable message, not a hang |
| T15 | Workbench GUI already open | documented behaviour, no corruption |
| T16 | Three runs back to back | identical verdicts, no zombie accumulation |

### Environment independence

| ID | Scenario | Expected |
|---|---|---|
| T17 | `env -i` clean shell | identical result |
| T18 | Invoked from a different cwd | identical result (library resolves repo root itself) |
| T19 | `OVERTHROW_*` overrides set | honoured; defaults used when unset |
| T20 | Path round-trip self-test | spaces, drive letters and trailing slashes survive `wslpath` both ways; OneDrive-redirected My Games resolves |

### Launcher

| ID | Scenario | Expected |
|---|---|---|
| T21 | `-- -autotest SCR_TEST_Example1TestSuite` | client launches **with Overthrow loaded**, `LOG_DIR=` printed, directory contains that run's logs |
| T22 | Argument containing a space | reaches the client intact |
| T23 | `--timeout` shorter than launch | exit 124, client killed |
| T24 | stdout piped to `grep '^LOG_DIR='` | works; no diagnostic noise on stdout |

### The check on the check

The most important single test is **T13**. A compile check that can only ever say "pass" is worse than no compile check, because it is trusted. Before this feature is called done, someone must deliberately break the *checking* mechanism (not the code) and confirm the tool says "I could not determine" rather than "OK".

---

## Dependencies

### Internal

- **None.** This is feature #1 of the epic and is built in isolation, by design. Nothing in `Scripts/Game/` is touched.
- Existing repo conventions it builds on: `.scripts/*.sh` (bash style, `OVERTHROW_*` env-override pattern), `.gitignore` (`.tmp/` already ignored), `.scripts/stage_dev.sh` (needs a one-line exclusion).

### External

- **Arma Reforger Tools (Workbench)** — `/mnt/n/Program Files (x86)/Steam/steamapps/common/Arma Reforger Tools/Workbench/ArmaReforgerWorkbenchSteamDiag.exe`. Verified present. **This is the only Workbench binary installed** (alongside `CrashReporter.exe`) — there is no non-Steam variant, so the Steam dependency cannot be engineered away.
- **Arma Reforger 1.7.0+** — `/mnt/n/Program Files (x86)/Steam/steamapps/common/Arma Reforger/ArmaReforgerSteamDiag.exe` (preferred over `ArmaReforgerSteam.exe`; `ArmaReforger_BE.exe` is the BattlEye launcher and is not used). Its `addons/data/ArmaReforger.gproj` is the client's `-gproj` target.
- **Profile roots** under `/mnt/c/Users/<user>/OneDrive/Documents/My Games/` — `ArmaReforgerWorkbench`, `ArmaReforgerWorkbenchExp`, `ArmaReforger`, plus any `-profile`-created directory. Note the OneDrive redirection.
- **Steam** installed, running and logged in.
- **A Windows host with a GPU.** Reforger has no headless rendering mode.
- **WSL↔Windows interop** (`binfmt_misc/WSLInterop`) — confirmed working.
- **`wslpath`**, `tasklist.exe`, `taskkill.exe`, GNU `timeout` (coreutils).
- **Overthrow's `addon.gproj`** (GUID `59B657D731E2A11D`) and its declared dependencies `58D0FB3206B6F859` (base data) and `5D6EBC81EB1842EF` (EPF), both resolvable by the Workbench.
- **Reference material:** BI Community Wiki, *Arma Reforger: Startup Parameters* — the authoritative external source for `-validate`, `-wbsilent`, `-exitAfterInit` and the `-packAddon` family. The reference script tree at `/mnt/n/Projects/Arma 4/ArmaReforger` — specifically `scripts/Autotest/WorkbenchGame/TestFramework/SCR_AutotestTool.c`, `scripts/Game/Tests/TestFramework/SCR_AutotestFramework.c`, and `scripts/Autotest/Game/TestFramework/SCR_AutotestReport.c`.

### Dependents (do not break these later)

- `dev-ops/autotest-foundation` — needs `launch-game.sh`, the client launch shape, and `LOG_DIR` resolution.
- `dev-ops/ci-pipeline` — needs stable command names, exit codes, error format, timeouts, cleanup, env-var configurability.
- `dev-ops/release-automation` — needs `ovt_run_workbench` and path translation.

---

## Risks & Mitigation

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| R1 | **`-validate` does not behave as documented** (unrecognised flag, always-zero exit code, or does not actually compile) | Medium | High | Phase 1 gates all design on observation. Decision 1's fallback — derive the verdict entirely from `console.log`, whose format is already known verbatim — keeps the external contract identical. Callers never see the difference. |
| R2 | **`ArmaReforgerWorkbenchSteamDiag.exe` is a launcher shim** that spawns a child and returns immediately, making `$?` meaningless | Medium | High | Explicit Phase 1 task (1.3) comparing shell wall time to `tasklist.exe` process lifetime. If confirmed, `ovt_run_win` polls for process exit and the verdict comes from the log. Decision 2's evidence rule already prevents a spurious 0 in this case. |
| R3 | **False green** — check passes while compilation actually failed. Aggravated by there being **no success string and no error-count summary** in the logs | Low (by design) | Critical | The three-condition rule (Decision 2), using the confirmed `Module: X; loaded Nx files` / `PROFILING: Compiling` markers as positive evidence. Test T13 deliberately breaks the checking mechanism and requires "indeterminate". Exit 2 exists precisely so ambiguity is never rounded down to success. |
| R4 | **Exit-code mangling through WSL interop** — documented `-1` arriving as `255` or `4294967295` | High | Low | Never compare against `-1`. Normalise: acceptable-code list for success, everything else is failure. Raw values recorded verbatim in `findings.md`. |
| R5 | **`-validate` output differs from the GUI-session samples** — different file, different shape, or straight to stdout | Medium | Medium | Phase 1 task 1.6 exists solely to confirm this before the parser is written. The known samples are a strong starting point, not a specification, and the plan says so. |
| R6 | **Error-list truncation** — the engine emits `Too many errors` and stops, so a "fix all 22" run reveals 40 more | High (on badly broken trees) | Medium | Parser detects the sentinel; summary reports the count as incomplete (`22+`). Phase 1 task 1.13 establishes roughly where the cap falls. |
| R7 | **Steam not running / login or DRM prompt** blocks an unattended run | Medium | Medium | Only one (Steam) Workbench binary exists, so this is a documented prerequisite, not a bug to fix. Detect and fail fast with exit 2 and an actionable message rather than hanging; the timeout is the backstop. Documented in `tools/README.md` and relevant to feature #4's runner setup. |
| R8 | **A Workbench GUI is already open**, causing resource-database contention or refusal | Medium | High | Phase 1 task 1.12 establishes real behaviour; Phase 5 adds a pre-launch guard defaulting to refusal with exit 2. `--allow-concurrent` for the case where it proves benign. |
| R9 | **First-run cost** — resource-database rebuild or shader compilation makes an initial run take many minutes and trip the timeout | High | Medium | Phase 1 measures cold vs warm. Default timeout set generously from measurement, overridable via `--timeout` / `OVERTHROW_COMPILE_TIMEOUT`. A one-off warm-up run documented in `tools/README.md` and in feature #4's runner-setup notes. Note a dedicated `-profile` starts cold the first time by definition. |
| R10 | **Timeout kills only the Linux-side interop stub**, leaving the Windows process alive and the runner accumulating zombies | Medium | High | Phase 1 task 1.10 verifies. `ovt_kill_tree` uses `taskkill.exe /F /T`, PID-scoped where possible; the kill is **verified** afterwards and a failed cleanup is itself reported as exit 2. |
| R11 | **Wrong log directory attributed to a run** — silently wrong artifacts, and later the wrong `junit.xml` | Low (mitigated by Decision 5) | High | Dedicated `-profile` isolates automation logs from interactive sessions; newest-since-t0 within that profile; fail loudly when nothing matches. Never fall back to "any log directory". |
| R12 | **Script-config mismatch** — validating under `workbench` (which defines `WORKBENCH`, `PERSISTENCE_DEBUG`) gives a green that does not reflect the shipping build | Medium | High | Decision 9: explicit `--config`, defaulting to the shipping configuration; the difference documented; test T5 exercises a guarded code path. Validating both configurations is a possible later refinement for feature #4. |
| R13 | **Dependency errors** (base game, EPF) make the check permanently red and train people to ignore it | Medium | Medium | Filter to in-project paths by default, but **always print the filtered count** and offer `--all`. Never hide silently. |
| R14 | **OneDrive-redirected Documents folder** breaks any constructed profile path; OneDrive sync can also lock or delay files | Medium | Medium | Never construct the path — discover it, or accept it via `OVERTHROW_MYGAMES_DIR`. If sync-induced file locking proves to be a problem, `findings.md` records it and a non-synced profile location is considered. |
| R15 | **Wrong client launch shape** — assuming the client takes Overthrow's `addon.gproj` produces a client with no mod loaded, and later a feature-#2 autotest that fails for unrelated reasons | Low (now known) | High | Decision 12 encodes the confirmed `-gproj`/`-addonsDir`/`-addons` shape as `launch-game.sh`'s default, corroborated by `SCR_AutotestTool.c`. |
| R16 | **Workbench stable vs Experimental** write to different profile roots; testing one and running the other silently reads the wrong logs | Low | Medium | `findings.md` records which branch was used; the binary path and profile name are both configurable and validated together. |
| R17 | **`tools/` gets staged/published with the addon** via `.scripts/stage_dev.sh`'s copy-everything-not-dotted rule | Medium | Low | Task 5.5 adds the exclusion; noted in `tools/README.md` for feature #5's packing step. |
| R18 | **A Reforger update changes flags or log format**, silently breaking the parser | Low | Medium | `findings.md` is stamped with the build it was verified against. The parser fails loudly (exit 2, unparsed-line warnings) rather than passing. Re-verification after a Tools update is a documented maintenance step. |
| R19 | **Quoting bugs at the WSL→Windows boundary** with paths containing spaces (every real path here does) | Medium | Medium | All boundary crossing in one function; path self-test in task 2.9; explicit pass-through test T22. |
| R20 | **Scope creep into feature #4** — adding CI orchestration, JSON output, matrix configs "while we're here" | Medium | Medium | Decision 6 and the Out of Scope list. `--format` and multi-config validation are explicitly deferred to their owning feature. |

---

## Notes

- **`findings.md` is a first-class deliverable**, not scratch work. It is the epic's reference for the Workbench CLI and the reason features #2 and #5 will not have to rediscover any of this. It should be committed.
- **The "Verified Ground Truth" section above is pre-Phase-1 evidence, not a substitute for Phase 1.** Every log sample in it came from an interactive GUI Workbench session. If `-validate` behaves differently, `findings.md` supersedes that section and this plan is amended accordingly.
- **Phases 2-6 are provisional until Phase 1 lands.** If findings contradict this plan — particularly Decisions 1, 5, 9, 10 or 12 — amend the plan *before* implementing, and record what changed and why. Building against an assumption Phase 1 already disproved is the main way this feature goes wrong.
- **Phase 1 warrants an advanced (max-effort) agent.** It is slow, empirical, easy to do superficially, and everything downstream depends on it being done honestly — including recording the inconvenient results. Phase 3's verdict logic is short but is where a false green would originate; advanced treatment is recommended there too.
- **This feature touches no gameplay code.** No EnforceScript is written or modified. The only incidental repo changes outside `tools/` and `docs/` are the `stage_dev.sh` exclusion (task 5.5) and `.gitignore` entries.
- **A free bonus if Phase 4 succeeds:** task 4.8 would be the first autotest ever executed on this machine, de-risking feature #2 before it starts.
- **Branch policy:** work stays on `vanilla-persistence`; `main` is under a bugfix-only freeze.

---

## Related Documentation

- **Requirements:** `docs/features/dev-ops/workbench-automation/requirements.md`
- **Epic overview:** `docs/features/dev-ops/epic-overview.md`
- **Epic requirements:** `docs/features/dev-ops/epic-requirements.md`
- **Findings (Phase 1 output):** `docs/features/dev-ops/workbench-automation/findings.md`
- **Contract for siblings (Phase 5 output):** `tools/README.md`
- **Sibling requirements:** `docs/features/dev-ops/autotest-foundation/requirements.md`, `.../ci-pipeline/requirements.md`, `.../release-automation/requirements.md`
- **Docs to update (DoD):** `CLAUDE.md`, `docs/technical-design.md` §2 and §10, `docs/mission-statement.md`, `.claude/skills/workbench-workflow/SKILL.md`
- **Reforger reference:** `scripts/Autotest/WorkbenchGame/TestFramework/SCR_AutotestTool.c`, `scripts/Game/Tests/TestFramework/SCR_AutotestFramework.c`, `scripts/Autotest/Game/TestFramework/SCR_AutotestReport.c`
- **External:** BI Community Wiki — *Arma Reforger: Startup Parameters* (`-validate`, `-wbsilent`, `-exitAfterInit`, `-packAddon`)

---

*This plan will be updated as implementation progresses. Phase 1's findings may revise Phases 2-6 — that is by design, not a failure of planning. See `context.md` and `tasks.md` for current status.*
