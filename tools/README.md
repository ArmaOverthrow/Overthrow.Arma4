# `tools/` — Workbench & Game Automation Contract

This directory is the **automation contract surface** for the dev-ops epic. The
commands, flags, exit codes, stdout formats and environment variables below are
a stable API: sibling features — #2 `autotest-foundation`, #4 `ci-pipeline`,
#5 `release-automation` — build against **this document**, not the script
source. Changing anything documented here is a breaking change.

Everything here is grounded in empirical findings against a named build
(Reforger **1.7.0.54**, engine **190965**, Tools stable branch):
`docs/features/dev-ops/workbench-automation/findings.md`.

## Architecture

```
tools/
├── README.md          # this contract
├── lib/common.sh      # sourced library — owns the WSL->Windows process boundary
├── compile-check.sh   # compile all EnforceScript, honest verdict + parsed errors
├── launch-game.sh     # game-client launcher with log-dir resolution
└── run-tests.sh       # run autotests, honest verdict from junit.xml
```

There is exactly **one process boundary**: `ovt_run_win` in `lib/common.sh`.
It launches the Windows exe through WSL interop, identifies the Windows PID,
registers it in a pidfile registry, enforces a real timeout (bash `timeout`
only kills the interop stub — the Windows process survives it), kills by PID
via `taskkill.exe /F /T` with **verified** death, and handles SIGINT/SIGTERM.
The two entry scripts are argument marshalling plus result interpretation; they
never name an `.exe` path. New tooling (feature #5) reuses the same boundary —
see [Adding a Workbench verb](#for-feature-5--adding-a-workbench-verb).

---

## `tools/compile-check.sh`

Compiles all of Overthrow's EnforceScript by launching the Workbench headlessly
(`-wbsilent -validate`) against the pinned automation profile, then parses the
run's `console.log`. No GUI, no focus stealing, no prompts.

```
tools/compile-check.sh [--timeout <s>] [--all] [--absolute]
                       [--keep-log] [--verbose] [--allow-concurrent]
                       [-h|--help]
```

| Flag | Meaning |
|---|---|
| `--timeout <s>` | Kill the Workbench after `<s>` seconds. Default `$OVERTHROW_COMPILE_TIMEOUT`, else 120. |
| `--all` | Also print errors originating outside this repo (base game / EPF / EDF), prefixed `[dep] `. Without it they are hidden from stdout but **always counted** in the stderr summary. |
| `--absolute` | Absolute WSL paths for in-project errors instead of repo-relative. |
| `--keep-log` | Additionally print the kept log's path on stderr. |
| `--verbose` | Extra diagnostics on stderr (resolved paths, exit code, parse counts). |
| `--allow-concurrent` | Silence the warning when another Workbench process is already running. |
| `-h`, `--help` | Help. |

There is **no `--config` flag** (passing it exits 2): the Workbench ignores the
optional `-validate [configName]` argument — verified empirically; the gproj's
own `workbench` script configuration applies automatically. Consequence: the
check always validates **with `WORKBENCH` and `PERSISTENCE_DEBUG` defined**
(see Known limitations).

### Exit codes

| Code | Meaning |
|---|---|
| **0** | Compiled clean — **positively verified, never assumed**. Requires ALL FOUR: (1) Workbench process exited 0, (2) zero in-project errors parsed from the log, (3) Game-module compile evidence present (`Module: Game; loaded Nx files` / `PROFILING : Compiling Game scripts took:`), (4) proof the Overthrow addon was actually loaded — its `addon.gproj` listed **inside the `Loaded addons:` block** of the log. Condition 4 guards the false-pass trap: with an unresolvable dependency the Workbench silently validates the **base game** and exits 0 on a broken Overthrow tree. |
| **1** | Compilation failed — at least one in-project error, printed on stdout. |
| **2** | Tool/environment failure or **indeterminate**: missing binary/project, no log found, unparseable log, addon-not-loaded, unexpected exit code, unverified timeout cleanup — and notably **dependency compile failure**: if the deps (base game / EPF / EDF) fail to compile, Overthrow was never verified, so the result is 2 ("not an Overthrow code failure"), never 1. Exit 2 never means "pass" and never means "your code is broken". |
| **124** | Timed out (GNU `timeout` convention). The Windows process is killed by PID and the kill verified; if the kill cannot be verified the exit is **2** with an orphan warning naming the PID and the exact `taskkill.exe` command. |
| **130/143** | Interrupted (SIGINT/SIGTERM). The launched Workbench process is killed (verified) first — Ctrl-C leaves no orphan. |

### stdout format (the only stdout)

One error per line, gcc-style, repo-relative by default:

```
Scripts/Game/Components/OVT_Foo.c:214: error: Broken expression (missing ';'?)
```

With `--all`, out-of-project lines follow, prefixed `[dep] ` (paths as the
engine emitted them). With `--absolute`, in-project paths are absolute WSL
paths. On exit 0 stdout is empty.

### stderr summary lines (machine-greppable)

```
compile-check: OK (<N> files, Game module, <D>s)
compile-check: FAILED (<N> errors) in <D>s
compile-check: FAILED (<N>+ errors, engine stopped listing) in <D>s   # 'Too many errors' sentinel seen — count is INCOMPLETE
compile-check: (<N> errors outside the project; re-run with --all to see them)
compile-check: INDETERMINATE: <reason> (log: <path>)
compile-check: TIMEOUT after <s>s (Workbench killed and verified dead; result unknown)
```

Parse-error caveat: EnforceScript **syntax** errors abort compilation at the
first failing file — only that file's errors are listed, so fixing it may
reveal more. **Semantic** errors are collected across files, capped at ~21
before the engine emits `Too many errors` (reported as `N+`).

### Artifacts

| Path | Content |
|---|---|
| `.tmp/compile-check/last.log` | This run's full `console.log` (copied; the parse target; attach in CI). |
| `.tmp/compile-check/last-launch.txt` | Interop stub stdout/stderr (Steam noise) for post-mortems. |

Both are overwritten per run; a stale `last.log` is deleted before launch so a
previous run's log can never satisfy this run's verdict.

### Determinism and timings

Same tree ⇒ same verdict and same stdout, independent of cwd, shell state or
window focus. Warm run: **~4–7 s**. One-off exception: the **first Workbench
CLI run ever on a machine** pays a ~60 s project resource-DB scan (writes
`<repo>/resourceDatabase.rdb`, gitignored); all later runs are warm, even from
brand-new profiles. Default timeout 120 s covers both.

### Concurrency

Running while a Workbench GUI is open is **observed benign** (warn-only, not a
refusal): the check uses its own pinned profile, produces correct verdicts,
and the GUI is unaffected. The warning names the running PID(s); silence it
with `--allow-concurrent`. Residual caveat: both processes write
`<repo>/resourceDatabase.rdb` — no failure was ever observed, but a
theoretical write race exists on that one shared file.

---

## `tools/launch-game.sh`

Launches the Arma Reforger **game client** with Overthrow loaded, passes
arbitrary caller arguments through unmangled (spaces survive), waits for exit,
and reports the run's log directory. Knows nothing about tests — feature #2
passes `-- -autotest ...` and collects `$LOG_DIR/junit.xml`.

```
tools/launch-game.sh [--timeout <s>] [--profile <name>] [--quiet]
                     [--allow-concurrent] [-h|--help] [-- <client args...>]
```

| Flag | Meaning |
|---|---|
| `--timeout <s>` | Kill the client after `<s>` seconds. Default `$OVERTHROW_GAME_TIMEOUT`, else 600. |
| `--profile <name>` | Profile for the default `-profile` flag and log resolution. Default `$OVERTHROW_PROFILE_NAME`, else `OverthrowCI`. |
| `--quiet` | Silence info-level stderr diagnostics (warnings/errors always print). |
| `--allow-concurrent` | Silence the warning when another game-client process is already running (warn-only either way). |
| `--` | Everything after it goes to the client verbatim. |

Default client argument set (the client takes the **base game's**
`ArmaReforger.gproj`; Overthrow is loaded via `-addonsDir`/`-addons`; cwd is
set to the game install dir, which the client requires):

```
-gproj <base ArmaReforger.gproj>
-addonsDir <repo parent>,<My Games>\ArmaReforgerWorkbench\addons
-addons EnfusionDatabaseFramework,EnfusionPersistenceFramework,Overthrow
-profile <name>
-noFocus -noThrow -window -logLevel debug
```

**Merge rule:** if the pass-through contains a flag with the same name as a
default (case-insensitive), the default is **dropped** and only the caller's
flag is sent — no duplicates are ever emitted. Pass-through args are appended
after the remaining defaults, so even an unanticipated duplicate favours the
caller under the client's observed last-wins behaviour — **note: last-wins is
only empirically confirmed for `-addonsDir`**; for other flags it is assumed.
A `-profile` in the pass-through also becomes the profile used for log
resolution (last one wins if repeated). **Absolute-path `-profile` values are
rejected (exit 2)** — log resolution supports only My Games-relative names
(nested names like `Overthrow/ci` are fine).

### stdout contract

Exactly five `KEY=value` lines (safe to `source` or `grep`), printed only
after a completed run whose log directory was resolved:

```
PROFILE_DIR=/mnt/c/.../My Games/OverthrowCI
LOG_DIR=/mnt/c/.../My Games/OverthrowCI/logs/logs_2026-08-01_12-30-00
LOG_DIR_WIN=C:\...\My Games\OverthrowCI\logs\logs_2026-08-01_12-30-00
EXIT_CODE=0
DURATION_S=8
```

**`EXIT_CODE` is meaningless.** The client exits 0 even on fatal errors
(unresolvable addons, script compile failures — verified empirically). It is
reported for completeness only; read outcomes from artifacts under `LOG_DIR`
(`console.log`, `junit.xml`, `crash.log`), never from `EXIT_CODE`.

### Exit codes

| Code | Meaning |
|---|---|
| **0** | Client ran to completion AND the log directory was resolved (says nothing about what happened in-game — see above). |
| **2** | Tool/environment failure: missing binary/gproj/addons dir, bad arguments, absolute `-profile`, log dir not resolvable after the run, or unverified timeout kill. |
| **124** | Timed out; client killed by PID, kill verified. **No stdout contract is printed** — the run did not complete. (Partial-log location, if any, goes to stderr.) |
| **130/143** | Interrupted; launched client killed (verified) first. |

### Artifacts

`.tmp/launch-game/last-launch.txt` — interop stub stdout/stderr per run.

### Working autotest example (verified end-to-end)

```bash
eval "$(tools/launch-game.sh -- -autotest "{6AB9C8EEE9A651B5}" | grep '^LOG_DIR=')"
ls "$LOG_DIR/junit.xml"
```

`{6AB9C8EEE9A651B5}` is BI's shipped (empty) `SCR_AutotestGroup` config; the
client runs the harness, writes `junit.xml`, `autotest.log` and
`autotest_failed.log` into the run's log dir, and self-exits in ~8 s.
Accepted `-autotest` values: a `{GUID}` of an `SCR_AutotestGroup` config, a
class inheriting `SCR_AutotestSuiteBase`, or one inheriting
`SCR_AutotestCaseBase`. **Do not use `SCR_TEST_Example1TestSuite`** — that
file name is a decoy; the class inside is `SCR_TEST_Example1SubjectSuite` and
the whole example is `#ifdef WORKBENCH`-guarded, so it does not exist in the
retail client (the run crashes out with `Invalid -autotest parameter value`).

---

## `tools/run-tests.sh`

Runs Overthrow's autotests in the real game client and returns an honest
exit code derived from `junit.xml`. Launches **exclusively** through
`tools/launch-game.sh` (it names no `.exe`, uses no `taskkill`, and resolves
no log directory of its own — feature #1's boundary is consumed, never
reimplemented). Feature #4 orchestrates this command; feature #3 writes the
suites it runs. Empirical ground truth:
`docs/features/dev-ops/autotest-foundation/findings.md` (valid for Reforger
**1.7.0.54** / engine **190965** — re-verify after updates).

```
tools/run-tests.sh [--timeout <s>] [--keep-artifacts] [--verbose]
                   [-h|--help] [<target>]
```

| Flag | Meaning |
|---|---|
| `--timeout <s>` | Kill the client after `<s>` seconds (passed through to `launch-game.sh`). Default `$OVERTHROW_TEST_TIMEOUT`, else **300**. Mandatory protection, not advisory — see Framework gaps. |
| `--keep-artifacts` | Additionally copy the run's remaining logs (`error.log`, `script.log`) into `.tmp/run-tests/` and print the artifact dir + source log dir on stderr. |
| `--verbose` | Extra diagnostics on stderr (target, log dir, parse counts). |
| `-h`, `--help` | Help. |

### Accepted `<target>` forms

One optional positional argument — any value the engine's `-autotest`
parameter accepts (there is deliberately no `--suite`/`--case`/`--group`
triplet):

| Form | Example |
|---|---|
| Suite class (inherits `SCR_AutotestSuiteBase`) | `OVT_TEST_SmokeSuite` *(the default)* |
| Case class (inherits `SCR_AutotestCaseBase`) — runs that one test inside its owning suite | `OVT_TEST_Smoke_HarnessRuns` |
| `{GUID}` of an `SCR_AutotestGroup` config | `"{6A6E29FF47ECB840}"` — see Group targets |

**There is no "run everything" form.** The engine's default group is empty
and its suite configuration disables every suite not explicitly named
(verified: Overthrow suites do not leak into a group run). Running more than
one suite in a single launch requires an `SCR_AutotestGroup` config
(`.conf` + `.meta` with a GUID). Overthrow ships two — below.

### Group targets (the fast/slow contract)

Two hand-authored `SCR_AutotestGroup` configs in `Configs/Tests/`. **These two
GUIDs are the stable contract**: quote them verbatim in CI and never read the
`.conf` files. The braces are part of the argument — always quote it, so no
shell ever gets a chance to interpret them.

| Target | GUID | Config | Suites (execution order is alphabetical, not config order) | Cases | Typical |
|---|---|---|---|---|---|
| **Fast** | `"{6A6E29FF47ECB840}"` | `Configs/Tests/OVT_TestGroup_Fast.conf` | `OVT_TEST_InitSuite`, `OVT_TEST_LogicSuite` | **18** | exit 0, **13–16 s** |
| **All** | `"{6A6E2A002F53A581}"` | `Configs/Tests/OVT_TestGroup_All.conf` | `OVT_TEST_CampaignSuite`, `OVT_TEST_InitSuite`, `OVT_TEST_LogicSuite`, `OVT_TEST_PersistenceSuite` | **30** | exit 0, **16–19 s** |

```bash
tools/run-tests.sh "{6A6E29FF47ECB840}"   # Fast — 18 cases
tools/run-tests.sh "{6A6E2A002F53A581}"   # All  — 30 cases
```

Deliberately in **neither** group: `OVT_TEST_MetaSuite` (always red by design)
and `OVT_TEST_PersistenceRoundTripSuite` (quarantined acceptance gate — see
below). Verified, not assumed: in an All run the harness listing shows
`OVT_TEST_MetaSuite: 0` and `OVT_TEST_PersistenceRoundTripSuite: 0`, and
neither contributes a `<testcase>` to `junit.xml`. `OVT_TEST_SmokeSuite` is
also excluded — it asserts nothing the tier suites do not.

**Recommended CI usage:**

```bash
# reset the save DB first — never without --profile, see the warning below
.scripts/reset_save.sh --profile OverthrowCI

tools/run-tests.sh "{6A6E29FF47ECB840}"   # every push
tools/run-tests.sh "{6A6E2A002F53A581}"   # nightly / pre-merge / release
```

The split buys scope, not wall time: All is only ~3 s slower than Fast, but
Fast cannot be reddened by campaign or persistence state. Neither group needs
`OVERTHROW_TEST_TIMEOUT` — both finish inside 20 s against a 300 s default.

**What a green All run does not prove.** It is one client process, so
join-in-progress and everything else multiplayer is untested — that is the most
common regression class in this project. Nor does it cover UI, performance, AI
movement (the navmesh does not load in the test world), or the save/reload
round-trip, which is written but gated (below). Treat exit 0 as a floor, not a
release decision.

**Adding a suite to a group:** append an entry to the `.conf`'s `m_aSuites`
with a fresh 16-uppercase-hex instance GUID, and make sure the suite class
carries `[BaseContainerProps()]`. Without that attribute the group loads but
instantiates nothing — `Unknown class '<Suite>'`, an empty `<testsuites>`, and
`run-tests.sh` exit 2. Cost of one extra suite: ~+1.5 s and one world
transition (a world-free suite adds no transition).

`tools/run-tests.sh OVT_TEST_MetaSuite` is the standing red-path check: that
suite always fails by design (proving failures surface) and is never part of
a default or CI run.

### Exit codes

| Code | Meaning |
|---|---|
| **0** | All tests passed — **positively verified, never assumed**. Requires ALL of: (1) `launch-game.sh` completed the run, (2) the Overthrow addon proven loaded (its `addon.gproj` inside a `Loaded addons:` block of this run's `console.log`), (3) `junit.xml` present in **this run's** log dir and parseable, (4) at least one `<testcase>`, (5) zero `<failure>`/`<error>` **elements**. |
| **1** | At least one test failed: `<failure>`/`<error>` elements present. Failing test names on stdout. |
| **2** | Tool failure or **indeterminate**: launcher tool failure, missing or unparseable `junit.xml` — which is exactly how an **invalid target** manifests (see Framework gaps) — zero test cases (e.g. an empty group), or addon-not-loaded. Never means "pass", never means "your tests are broken". |
| **124** | Timed out (propagated unchanged from `launch-game.sh`; client killed by PID, kill verified). No artifacts are collected — the run did not complete. |
| **130/143** | Interrupted (SIGINT/SIGTERM), propagated; the client is killed (verified) first. |

### stdout format (the only stdout)

Failing test names, one per line, nothing else:

```
OVT_TEST_Meta_AlwaysFails
```

On exit 0, 2 and 124 stdout is **empty**. Same split as `compile-check.sh`:
all diagnostics and the summary go to stderr.

### stderr summary lines (machine-greppable)

```
run-tests: OK (<N> tests, <D>s)
run-tests: FAILED (<N> of <M>) in <D>s
run-tests: INDETERMINATE: <reason> (artifacts: <path>)
run-tests: TIMEOUT after <s>s (client killed and verified dead; result unknown)
```

### Artifacts

Collected into `.tmp/run-tests/` under stable names after every completed
run (including red and indeterminate ones). **Stale artifacts are deleted
before every launch** — a previous run's `junit.xml` can never satisfy this
run's verdict.

| Path | Content |
|---|---|
| `.tmp/run-tests/junit.xml` | The verdict source (engine-fixed `$logs:/junit.xml`; `-autotest-output-dir` does not apply). Absent when the run produced none — which is itself the exit-2 evidence. |
| `.tmp/run-tests/autotest.log` | Per-test framework log (UTF-8 status glyphs — expect non-ASCII). |
| `.tmp/run-tests/autotest_failed.log` | Failing test names, one per line. Exists but **0 bytes** on a green run. |
| `.tmp/run-tests/console.log` | Full client log for the run. |
| `.tmp/run-tests/crash.log` | Only present when the client crashed — e.g. an invalid `-autotest` target. |

### junit.xml shape (this build)

A passing `<testcase>` is self-closing; a failing one wraps
`<failure type="Result">message</failure>` with the `SetResultFailure()`
string verbatim. **`<testsuite>` carries no `failures=`/`errors=`
attributes** — any consumer (PR annotations in feature #4 included) must
count `<failure>`/`<error>` *elements*, never read attributes. Verbatim
samples: `docs/features/dev-ops/autotest-foundation/findings.md`.

### Framework gaps (documented, not worked around)

- **An invalid `-autotest` target is process-indistinguishable from
  success**: the client raises a VM exception, writes `crash.log`, self-exits
  cleanly — client exit 0, launcher rc 0, **no `junit.xml`**. The missing
  artifact is the only detector; `run-tests.sh` classifies it as exit 2 and
  quotes the crash reason.
- **`Setup_AwaitWorld` has no timeout**: a world transition that never
  completes is an infinite hang inside the harness. The `--timeout`
  (default 300 s, >13x the worst observed 22 s green run) is the only
  backstop — mandatory, not advisory.
- **The harness runs twice per launch** and loads the test world twice
  (menu → test world → test world again): doubled `CLI autotest ...` /
  `Requesting scenario change:` console lines and a `junit.xml` `time=`
  larger than the visible test time are **normal**, not an anomaly.
- **No run-everything CLI form** — see Accepted target forms above.
- The default test world boots `OVT_OverthrowGameMode` but the campaign is
  **not started** (start menu shows); tests needing running-campaign state
  need explicit setup (feature #3).

### Timings

Green run ~14–22 s, red ~13–15 s, degenerate (no world transition) ~7–8 s —
each including full client boot and two test-world loads. Determinism:
same tree ⇒ same exit code and same summary.

### Persistence acceptance gate (`core/persistence`)

`OVT_TEST_PersistenceRoundTripSuite` is **quarantined and red on purpose**. It
is in no group config, is never part of a default or CI run, and **its exit
code is the `core/persistence` migration's acceptance criterion**: the
branch has no working save path in either persistence system, so a save +
reload round-trip cannot pass yet.

```bash
# 1. establish the save-state precondition (REQUIRED — see below)
.scripts/reset_save.sh --profile OverthrowCI          # fresh campaign
# …or, for a known saved state once fixtures exist:
.scripts/activate_save.sh <name> --profile OverthrowCI

# 2. run the gate
tools/run-tests.sh OVT_TEST_PersistenceRoundTripSuite
```

| Exit | Meaning |
|---|---|
| **1** | Expected **today**. `.tmp/run-tests/junit.xml` carries `Persistence capability absent: SaveGame() produced no save (HasSaveGame() still false). The vanilla-persistence migration is not complete.` on every case. |
| **0** | **The migration is complete.** Delete the quarantine header, add the suite to the All group, and update `docs/features/core/persistence/`. |

The reset is not optional decoration: the suite's first case asserts that no
save exists *before* it saves and one exists *after*, which is what makes a
save layer that merely claims to have saved detectable. Running the gate
against a stale save reports a precondition violation rather than a pass.

**Never run the `.scripts/` save tools without `--profile OverthrowCI` (or an
explicit `OVERTHROW_SAVE_DIR`)** — their default target is the user's real
Workbench campaign save. `reset_save.sh` prints the path it resolved before it
deletes anything and refuses anything that is not an absolute path ending in
`.db/Overthrow`. Full documentation of all three: [Save-state
control](#save-state-control-scripts).

The green companion suite, `OVT_TEST_PersistenceSuite`, covers the same state
kinds within a single session (write through a public manager mutator, read
back through a public manager accessor) and exits **0** today.

---

## Save-state control (`.scripts/`)

Three save-state tools live in **`.scripts/`**, not `tools/` — they predate this
epic, the user runs them by hand, and relocating them would break existing
habits for no functional gain. Feature #3 added non-interactive argument forms
so automation can call them; **every interactive path is unchanged**. This
section is their canonical documentation.

Save state is a **precondition of a run**, established before launch. The save
DB is read during world init, long before a test step executes, so no test case
can arrange it from inside EnforceScript.

```
.scripts/reset_save.sh    [--profile <name>] [-h|--help]
.scripts/backup_save.sh   [--profile <name>] [<name>] [-h|--help]
.scripts/activate_save.sh [--profile <name>] [<name-or-file>] [-h|--help]
```

| Script | Does | With an argument (automation) | Without one (interactive) |
|---|---|---|---|
| `reset_save.sh` | Deletes the save DB — next boot starts a fresh campaign | always non-interactive | same |
| `backup_save.sh` | Archives the save DB into `.saves/<name>_<timestamp>.tar.gz` | `<name>` skips the suggestion line and the prompt; same sanitisation as a typed name | prompts for the name, as before |
| `activate_save.sh` | Resets, then extracts a `.saves/` archive over the save DB | `<name-or-file>` selects: an exact path, else an exact filename in `.saves/`, else the **newest** `*<arg>*.tar.gz` by mtime | numbered menu, as before |

### Which directory they act on

Precedence: **`OVERTHROW_SAVE_DIR` > `--profile <name>` > built-in default.**

| Source | Resolves to |
|---|---|
| `OVERTHROW_SAVE_DIR` | used verbatim; wins over `--profile`, and the script says so |
| `--profile <name>` | `<My Games>/<name>/profile/.db/Overthrow`, via `tools/lib/common.sh`'s `ovt_profile_dir` — note the `profile/` level between the profile root and `.db` |
| neither | `<My Games>/ArmaReforgerWorkbench/profile/.db/Overthrow` — **the user's real Workbench campaign save** |

> ⚠️ **The default is the trap.** Tests run under the game client's
> `OverthrowCI` profile; the scripts default to the **Workbench** profile; the
> two are different directories. **Every automated invocation must pass
> `--profile OverthrowCI` or set `OVERTHROW_SAVE_DIR`.** A forgotten profile
> flag points an `rm -rf` at hours of the user's play.

A **set-but-empty** `OVERTHROW_SAVE_DIR` is an error, not a fallback — `VAR=
.scripts/reset_save.sh` used to silently fall through to the Workbench default.

### The destructive-path guard

`reset_save.sh` is an `rm -rf`, so it:

- prints `Resolved save directory: <path>` on **every** run, before touching
  anything;
- **refuses** — exit 1, nothing deleted — a path that is empty, is `/`, is
  relative, or does not end in `.db/Overthrow`;
- treats a missing directory as "nothing to delete" (exit 0), not an error.

`activate_save.sh` resets through the same guard before extracting.

### Exit codes

| Code | Meaning |
|---|---|
| **0** | Succeeded — including `reset_save.sh` finding nothing to delete, and `activate_save.sh` cancelled at the menu. |
| **1** | Usage error, refused path, missing save directory, invalid or unmatched archive name, or a failed extract/delete. An unmatched name prints the available archives and resets nothing. |
| **2** | `--profile` could not be resolved (propagated from `tools/lib/common.sh`). |

No failure path exits 0, and nothing succeeds silently.

### `.saves/` layout and naming

`.saves/` (repo root, gitignored) is the **user's** fixture library, not this
epic's — six `testworld_*` archives today. Anything an automated run creates
there must be deleted afterwards.

```
.saves/<world>_<situation>_<MP|SP>_<YYYYmmdd>_<HHMMSS>.tar.gz
.saves/testworld_baserecruit_SP_20250708_054422.tar.gz
```

`backup_save.sh` appends `_<timestamp>` itself — pass only the
`<world>_<situation>_<MP|SP>` part.

### Use in automation

```bash
# fresh campaign, before a campaign/persistence-tier run or the acceptance gate
.scripts/reset_save.sh --profile OverthrowCI

# known state from a local fixture (needs a working load path — see below)
.scripts/activate_save.sh --profile OverthrowCI testworld_baserecruit_SP
```

For the tier suites this is belt-and-braces today: nothing writes a save on this
branch (`HasSaveGame()` is hardcoded `false`), and the verdicts were verified
identical with and without the reset. It is **load-bearing for the acceptance
gate**, whose first case asserts that no save exists before it saves and one
exists after.

**Migration note:** all three scripts are written against **EPF's**
`.db/Overthrow` layout. Vanilla persistence stores saves elsewhere and its
`SaveGameManager` exposes no path to script, so the replacement location can
only be established empirically once the migration lands — at which point the
guard's suffix and the three `DEFAULT_SAVE_DIR` values must be updated together.
Recorded as migration work in `docs/features/core/persistence/context.md`.

---

## Log-directory resolution rule

How `LOG_DIR` is determined — precise enough to rely on without reading source:

1. The profile root is `<My Games>/<profile>` where `<My Games>` is discovered
   (Documents may be OneDrive-redirected) or taken from
   `OVERTHROW_MYGAMES_DIR`, and `<profile>` is the effective profile name
   (default `OverthrowCI`; nested names allowed).
2. A timestamp `t0` is captured immediately **before** launch.
3. After the run, `LOG_DIR` is the **newest** directory matching
   `<profile root>/logs/logs_*` whose mtime ≥ `t0` (ties broken by
   lexically-greater name — `logs_YYYY-MM-DD_HH-MM-SS` names sort by time).
4. If no directory matches, or the resolved directory has no `console.log`,
   the script **exits 2** and prints nothing on stdout — it never guesses.

Because the automation profile is dedicated, nothing else writes there, so
newest-since-t0 cannot pick up an interactive session's log. Feature #2
collects `junit.xml` at exactly `$LOG_DIR/junit.xml` (the engine's fixed
`$logs:/junit.xml` path; `-autotest-output-dir` does **not** apply to it).
A run's log dir also contains `console.log`, `script.log`, `error.log`, and
on crashes `crash.log`.

---

## Environment variables

All machine-specific paths are overridable — CI workflow files need no
committed absolute paths. Precedence: environment variable >
`tools/config.local.sh` > built-in default.

| Variable | Consumed by | Default |
|---|---|---|
| `OVERTHROW_WORKBENCH_EXE` | compile-check (via lib) | `/mnt/n/Program Files (x86)/Steam/steamapps/common/Arma Reforger Tools/Workbench/ArmaReforgerWorkbenchSteamDiag.exe` |
| `OVERTHROW_GAME_EXE` | launch-game (via lib) | `/mnt/n/Program Files (x86)/Steam/steamapps/common/Arma Reforger/ArmaReforgerSteamDiag.exe` |
| `OVERTHROW_GPROJ` | compile-check | `<repo root>/addon.gproj` |
| `OVERTHROW_GAME_GPROJ` | launch-game | `<game install>/addons/data/ArmaReforger.gproj` |
| `OVERTHROW_ADDONS_DIRS` | compile-check | `<My Games>\ArmaReforgerWorkbench\addons,<game install>\addons` (Windows form, comma-separated; packed workshop EPF/EDF + base game) |
| `OVERTHROW_GAME_ADDONS_DIRS` | launch-game | `<repo parent>,<My Games>\ArmaReforgerWorkbench\addons` (Windows form; source Overthrow + packed EPF/EDF) |
| `OVERTHROW_GAME_ADDONS` | launch-game | `EnfusionDatabaseFramework,EnfusionPersistenceFramework,Overthrow` |
| `OVERTHROW_PROFILE_NAME` | both | `OverthrowCI` |
| `OVERTHROW_MYGAMES_DIR` | both | discovered: `<win user profile>/OneDrive/Documents/My Games`, else `<win user profile>/Documents/My Games` |
| `OVERTHROW_COMPILE_TIMEOUT` | compile-check | `120` (seconds) |
| `OVERTHROW_GAME_TIMEOUT` | launch-game | `600` (seconds) |
| `OVERTHROW_TEST_TIMEOUT` | run-tests | `300` (seconds) |
| `OVT_PID_REGISTRY` | lib (pidfile registry) | `<repo root>/.tmp/ovt-pids` |

**`tools/config.local.sh`** (gitignored): if present at that exact path it is
sourced by `lib/common.sh` on every load. Put per-machine `OVERTHROW_*`
assignments there instead of exporting them in your shell:

```bash
# tools/config.local.sh — this machine only, never committed
OVERTHROW_WORKBENCH_EXE="/mnt/d/Steam/steamapps/common/Arma Reforger Tools/Workbench/ArmaReforgerWorkbenchSteamDiag.exe"
OVERTHROW_PROFILE_NAME="OverthrowCI"
```

Important dependency note: the default addon dirs use the **packed workshop
EPF/EDF** under `My Games/ArmaReforgerWorkbench/addons` — download them once
in the Workbench. The EPF/EDF **source repos do not compile** on 1.7.0.54 and
must not be substituted. If the packed deps are missing, compile-check fails
preflight with exit 2 (this is deliberate: an unresolvable dependency is what
triggers the base-game false-pass trap).

---

## For feature #5 — adding a Workbench verb

`ovt_run_workbench <timeout_s> [args...]` is the whole integration point: a
new Workbench verb (`-packAddon`, `-publishAddon*`, …) is a new argument list,
not new plumbing. You inherit the timeout, PID-scoped kill + verify, pidfile
registry, signal handling and exit-code passthrough for free.

```bash
#!/bin/bash
set -euo pipefail
source "$(dirname "$0")/lib/common.sh"   # any cwd works; resolves repo root itself

GPROJ_WIN="$(ovt_win_path "$(ovt_project_gproj)")" || exit 2
ADDONS="$(ovt_workbench_addons_dir_arg)" || exit 2
PROFILE="$(ovt_profile_name)"

ovt_run_workbench 300 \
    -gproj "$GPROJ_WIN" \
    -addonsDir "$ADDONS" \
    -packAddon \
    -wbsilent -profile "$PROFILE"
rc=$?   # 0 / workbench code / 124 timeout / 2 tool failure

# Pack/publish output lands under the run's PROFILE directory — resolve it:
PROFILE_DIR="$(ovt_profile_dir "$PROFILE")"    # <My Games>/<profile>
# (expect output under e.g. "$PROFILE_DIR/publish/" — verify empirically,
# none of the -packAddon/-publishAddon* flags has been executed yet)
```

Also available: `ovt_run_game <timeout> [client args...]` (sets cwd to the
game install dir, as the client requires) and raw
`ovt_run_win <timeout> <exe_wsl_path> [args...]`. After any run:
`OVT_LAST_DURATION_S` (wall seconds), `OVT_LAST_WIN_PID` (identified Windows
PID(s)), `OVT_LAST_KILL_FAILED` (1 if a timeout-kill could not be verified —
surface this loudly and exit 2).

---

## Utilities

```bash
bash tools/lib/common.sh --self-test              # 29 assertions: path round-trips,
                                                  # resolvers, sweep, trap save/restore
bash tools/lib/common.sh --sweep-stale            # LIST registered orphans (STALE\t<pid>\t<image>)
bash tools/lib/common.sh --sweep-stale --kill     # kill them (verified) + clean registry
```

`source tools/lib/common.sh` is side-effect-free (no `set -e`, no traps, no
launches) and works from any cwd — the library resolves the repo root itself.
Path helpers are usable standalone:

```bash
source tools/lib/common.sh
ovt_win_path "/mnt/n/Projects/Arma 4/Overthrow.Arma4"   # -> N:\Projects\Arma 4\Overthrow.Arma4
ovt_wsl_path 'C:\Users\Foo\Documents'                    # -> /mnt/c/Users/Foo/Documents
```

`ovt_win_path` refuses untranslatable input (non-zero, no output) rather than
emitting a silently-wrong `\\wsl.localhost\...` UNC path; both helpers handle
spaces, trailing slashes and not-yet-existing paths under mounted drives.

### Process hygiene (pidfile registry, signals, sweep)

- Every Windows PID launched by `ovt_run_win` is registered as
  `.tmp/ovt-pids/<pid>.pid` (line 1: image name, line 2: launch epoch). The
  record is removed only when the process is **verified dead** (normal exit,
  timeout kill, or signal cleanup). A failed kill keeps its pidfile so the
  sweep can retry.
- **SIGINT/SIGTERM** during a run: the Windows process tree is killed
  (verified), the registry cleaned, the caller's own traps restored, and the
  signal re-raised — the script exits 130/143 with no orphan. Traps are
  installed only for the duration of the run, never globally.
- **`--sweep-stale`** reaps orphans left by an *uncleanly* killed wrapper
  (SIGKILL bypasses traps). It considers **only** PIDs from the registry,
  and only kills one whose image name still matches the recorded name
  (guards PID recycling). It can **never** kill a developer's own
  Workbench/game session — those were never registered. Dead/recycled
  records are cleaned without killing. Returns 0 (including "nothing
  stale"); 2 if a kill could not be verified.
- **Feature #4 (ci-pipeline) should run
  `bash tools/lib/common.sh --sweep-stale --kill` before each job** to
  guarantee a clean slate.

---

## Known limitations

- **Steam must be installed, running and logged in.** Only a Steam Workbench
  binary exists (`ArmaReforgerWorkbenchSteamDiag.exe`) — the dependency cannot
  be engineered away. The no-Steam case is **untested** (both Diag exes log
  `[API loaded no]`, suggesting weak coupling, but do not rely on it).
- **Windows host with a GPU required.** Reforger has no headless rendering
  mode ("headless" in its codebase means headless MP *clients*). GPU-less /
  Linux CI runners are not an option.
- **First-ever run is slow**: one-off ~60 s project resource-DB scan (writes
  `<repo>/resourceDatabase.rdb`); everything after is warm (~4–7 s), even
  from new profiles.
- **`-validate` covers compilation only** — no runtime behaviour, no resource
  validation (resource errors do not affect the exit code). And it always
  compiles with the gproj's `workbench` defines active: **`WORKBENCH` and
  `PERSISTENCE_DEBUG` are defined during validation**, so `#ifndef WORKBENCH`
  shipping-only paths are not compiled. A green check does not exercise the
  exact shipping define set.
- **Findings are build-specific**: verified on Reforger **1.7.0.54** / engine
  **190965**, Tools **stable** branch only (stable and Experimental Workbench
  use different profile roots). Re-verify after any Tools/game update.
- **OneDrive-redirected `My Games`** (the case on this machine): sync can in
  theory lock or delay log files. Never observed, but if it bites, move the
  profile root (set `OVERTHROW_MYGAMES_DIR`) or pause sync.
- **Concurrent GUI Workbench is benign but shares one file**: both processes
  write `<repo>/resourceDatabase.rdb`; a theoretical write race remains
  (warn-only guard, `--allow-concurrent` to silence).
- **The client's CLI-params echo strips quoting**: `console.log` shows the
  passed parameters without their original quoting, so the echo is not
  evidence about how arguments were quoted — do not use it as a quoting
  oracle. (Pass-through quoting itself is verified: arguments with spaces
  reach the client intact.)
- **`tools/` is excluded from dev staging**: `.scripts/stage_dev.sh` skips it
  (like `docs/`), so it is never staged into `Overthrow.Dev`. Feature #5's
  packing step must likewise keep `tools/` out of the published addon.

## Maintenance

After a Reforger/Tools update, re-run the verification set in
`docs/features/dev-ops/workbench-automation/findings.md` (exit codes, log
shapes, `Loaded addons:` block, timings) before trusting the tools again. The
parser is deliberately brittle-loud: unknown `SCRIPT (E)` shapes are surfaced
as warnings and force exit 2 (indeterminate) rather than being dropped — if
the log format drifts, the check fails loudly instead of going silently green.
