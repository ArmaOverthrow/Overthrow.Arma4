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
├── launch-server.sh   # local dedicated server running THIS working tree (MP play-testing)
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

## `tools/launch-server.sh`

Runs a local dedicated server with Overthrow loaded **from this working tree**,
so multiplayer and JIP behaviour can be play-tested without publishing
anything. This is the only tool here that addresses the project's most common
regression class; `run-tests.sh` is single-client and structurally cannot.

```
tools/launch-server.sh [--scenario testworld|eden|<resource>]
                       [--mode local|dedicated] [--config <file>]
                       [--port <n>] [--max-players <n>] [--profile <name>]
                       [--admin-password <pw>] [--timeout <s>]
                       [--quiet] [--allow-concurrent] [-h|--help]
                       [-- <server args...>]
```

**Requires a separate Steam app**: *Arma Reforger Server* (1874900). It is not
part of the game client install, and `ovt_server_exe` fails with exit 2 and
that instruction if it is missing.

### The two modes are not interchangeable

| | `--mode local` (default) | `--mode dedicated` |
|---|---|---|
| Engine path | `-server <scenario>` | `-config <server.json>` |
| Addons mounted | `core`, `ArmaReforger`, **this tree** — nothing else | the same three **plus EPF + EDF** |
| Workshop | not contacted; works offline | mod resolved through the workshop API |
| Time to listening | ~5 s | ~38 s (download/verify pass) |
| Backend registration | no | yes — prints a **Direct Join Code** |
| RCON | no | yes, on `--port + 17998` |
| `--port` | **rejected** (see below) | honoured |
| Server config knobs | `--max-players`, `--admin-password` only | full JSON: persistence, AI limits, view distance… |

### Local mode has no player identities — and Overthrow needs them

Local mode never contacts the backend, so it authenticates nobody
(`ServerImpl event: authenticating (identity=0x00000000)`) and every player's
identity id is empty (`### Updating player: … IdentityId=`). Overthrow keys
every player record on that string, so with no identity
`OVT_SpawnLogic.DoSpawn_S` can never register the player: it logs
`WARNING: Persistent UID not available yet for playerId: N, retrying...` once
a second **forever**, and the player sits at the spawn camera, connected, in a
started campaign, never entering the world.

The launcher therefore passes **`-ovtDevUid`** in local mode, which opts the
session in to synthesised `DEV_<playerId>` UIDs
(`OVT_Global.GetPlayerUID`). Distinct per connected client, computed
identically on server and clients, so no extra replication:

```
[Overthrow] Setting up player data for: DEV_1
[Overthrow] Player assigned home at <226.19, 1.655, 193.901>
[Overthrow] Setting up player data for: DEV_2
[Overthrow] Player assigned home at <232.689, 1, 27.62>
```

Two clients from the *same Steam account* still get distinct UIDs and distinct
homes, which is why the id is derived from the runtime player id rather than
from anything account- or name-shaped.

`--no-dev-uid` suppresses the flag if you are deliberately testing the
no-identity path. **Dedicated mode does not pass it**, on purpose: there the
backend issues real identities, and an empty one means "still authenticating",
which must be waited for rather than papered over. The fallback is gated on
the CLI parameter and not merely on "the identity is blank" for that same
reason — a production server never passes it, so it cannot fire there.

### Difficulty (`Overthrow_Config.json`)

The test world ships its own **`Test World`** difficulty preset — generous
starting cash, sized for play-testing. The launcher selects it automatically
for the testworld scenario by writing the difficulty into
`$profile:Overthrow_Config.json`, which the game matches **by name** against
the game mode's preset list at `DoStartGame`:

```
[Overthrow] Overthrow_Config.json - setting difficulty to Test World
```

Two details worth knowing:

- **The file is patched in place, not regenerated.** Overthrow *writes this
  file itself* with defaults (`difficulty: ""`) when it is missing, so
  "create it if absent" would be a no-op after the very first launch. Only the
  `difficulty` value is rewritten; `officers`, item limits, the webhook URL and
  everything else survive. The launcher prints what it changed:
  `Overthrow_Config.json difficulty '<empty>' -> 'Test World'`.
- **It is only applied to the test world.** The `Test World` preset is in the
  preset list for that scenario only, so `--scenario eden` leaves the file's
  existing value alone rather than naming a preset that would not resolve.

`--difficulty <name>` overrides the choice for any scenario (base game mode
presets: `Easy`, `Normal`, `Hard`, `Extreme`, `Insane`); `--no-config` leaves
the file untouched entirely.

Both modes mount **the working tree, not the published build** — verified by
the `Loaded addons:` block naming `<repo>/addon.gproj`. The difference that
matters is the extra pair: resolving Overthrow through the workshop also pulls
the *published* build's dependency manifest, which still lists EPF and EDF, and
those get mounted. This tree no longer uses EPF, so **dedicated mode runs with
two addons that local mode does not have**. Prefer `local` unless you
specifically need a Direct Join Code, RCON, or a config-only setting.

`--port` is rejected in local mode rather than silently ignored: the `-server`
route has no bind-port flag, `-bindPort` is accepted and discarded, and the
engine logs `RPL listen address not specified. Using default fallback.` before
binding 2001 regardless. Reporting a `BIND_PORT` the server is not listening on
would be worse than refusing.

### Joining (verified end-to-end 2026-08-06)

**One command, no menu navigation** — `-client <ip:port>` auto-joins:

```bash
# terminal 1
tools/launch-server.sh --scenario eden

# terminal 2 — joins automatically once the server reports SERVER READY
tools/launch-game.sh --profile OverthrowClient1 -- -client 127.0.0.1:2001
```

For a **second** player on the same machine, give it its own profile so the
two clients do not share settings or save data — verified working, two clients
into one campaign:

```bash
tools/launch-game.sh --profile OverthrowClient1 --allow-concurrent -- -client 127.0.0.1:2001
tools/launch-game.sh --profile OverthrowClient2 --allow-concurrent -- -client 127.0.0.1:2001
```

`--allow-concurrent` only silences the "another client is already running"
warning; the launcher never refuses and only ever kills processes it started.
A client on another PC uses this machine's LAN IP.

**Give the clients a long `--timeout`.** It defaults to 600 s, and the
launcher kills the client when it expires — mid-play-test, with no warning
beyond the log line. `--timeout 3600` for a real session.

Verified on both sides of the wire:

```
CLIENT  Starting multiplayer client using command line args.
        Starting RPL client, number of addresses to try connecting to: 1
        Endpoint 127.0.0.1:2001 state 0: send ConnectionRequest, size 512.
        ClientImpl event: connected (identity=0x00000000)
SERVER  Player connected: connectionID=0
        ### Creating player: PlayerId=1, Name=<steam name>
        Players connected: 1 / 1
```

**No `rpl-validation-*` switches were needed.** A client launched from this
tree and a server launched from this tree produce matching script, addon and
rdb checksums, so validation passes natively — the two ends run identical
code. That is the whole point of joining this way rather than disabling
validation.

A Steam-launched retail client running the *published* Overthrow will **not**
connect: the engine refuses with `RplConnection::ValidationError remote script
source code checksum does not match!`.

Manual join also works — Reforger → Multiplayer → **Direct Join** →
`127.0.0.1:<port>`; dedicated mode additionally prints a
`Direct Join Code: <10 digits>`.

### The Workbench cannot be the client

Tested and ruled out on 2026-08-06, because it is the obvious thing to reach
for and it does not work:

- **Workbench Play mode is local-only.** Its play modes are `Play inside
  Viewport`, `Play in Fullscreen`, `Play from Camera Position` — there is no
  join-a-server play mode, and clicking Play on a world starts a local session
  with the start-game menu.
- **The Workbench binary ignores `-client`.** Launched with
  `-client 127.0.0.1:2001` it never emits `Starting multiplayer client using
  command line args.`, and the server records no incoming connection. It
  starts a local session instead.
- The `rpl-validation-*-disable` switches below *are* accepted by the
  Workbench (all four warnings printed) — they just do not create a client.

Use `tools/launch-game.sh` for the client and keep the Workbench for editing
and `Shift+F7` script reloads.

### Dev-only validation switches (documented, not needed)

Both the Workbench and the server binaries — but **not** the retail client —
accept a family of developer switches, verified accepted in plain `-flag`
form (each logs `RplSession::CheckWarning: <x> validation disabled. This
option is DEVELOPER ONLY!`):

```
-rpl-validation-devbin-disable    -rpl-validation-scr-disable
-rpl-validation-addons-disable    -rpl-validation-rdb-disable
-rpl-validation-version-disable   -rpl-timeout-disable / -rpl-timeout-ms / -rpl-reconnect
```

Validation is **server-side** (`Rpc_Validation_S received` → `ValidationError`
→ `Rpc_ValidationPassed_O sent`), so a switch has to be on the server to
matter. **You should not need any of them**, and reaching for
`-rpl-validation-scr-disable` in particular is a smell: it does not reconcile
mismatched code, it only silences the check, leaving client and server running
different builds.

### stdout contract

```
MODE=local
SCENARIO={6B0E7A50D1E2F3A4}Missions/25_OVT_TestWorld.conf
PROFILE=OverthrowDS
BIND_PORT=2001
LOG_DIR=/mnt/c/.../My Games/OverthrowDS/logs/logs_2026-08-06_22-32-01
EXIT_CODE=124
```

### Exit codes

| Code | Meaning |
|---|---|
| 0 | The server **exited on its own** — for a server this almost always means it failed to start. The launcher warns and points at the log. |
| 2 | Tool/environment failure: server binary missing, bad arguments, or the log dir could not be resolved. |
| 124 | Ran for the whole `--timeout` window. **This is the success shape** when a timeout was set deliberately. |
| 130/143 | Ctrl-C / SIGTERM. The Windows process is killed and the kill verified. |

The default timeout is 86400 s — i.e. "runs until you press Ctrl-C". Short
timeouts are for automation.

A readiness watcher greps the live log and prints `SERVER READY` with the join
address the moment the RPL listener is up (~5 s local, ~38 s dedicated), so
there is no need to guess from the entity-spawn spam.

### Profile separation

Defaults to profile **`OverthrowDS`**, deliberately *not* the `OverthrowCI`
profile the test harness uses — a server session writes saves and settings,
and must not perturb the state `run-tests.sh` asserts against.

### Still not verified

Verified as of 2026-08-06: single client joins, **two concurrent clients**,
each registering a distinct player with its own home, and the second client
joining a campaign that was already running (**JIP at the connection level**).
These are not:

- **A second machine.** LAN join, firewall, inbound 2001/17777 — everything so
  far is loopback on one box.
- **JIP into a campaign with real accumulated state.** The second client
  joined seconds after the first, into a fresh campaign. Joining hours in,
  with towns taken and vehicles owned, is a different test.
- **That any given Overthrow system replicates correctly.** Connection,
  registration and home assignment work. Groups, recruits, economy and
  persistence over the wire are exactly what you are launching this to find
  out.
- **Dedicated mode with a real client.** The verified two-client run was local
  mode. Dedicated mode's identity path (real backend UIDs instead of `DEV_n`)
  has never had a client attached.
- **RCON.** Initialisation observed; no command has ever been sent.

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
| **Fast** | `"{6A6E29FF47ECB840}"` | `Configs/Tests/OVT_TestGroup_Fast.conf` | `OVT_TEST_InitSuite`, `OVT_TEST_LogicSuite` | **20** | exit 0, **13–16 s** |
| **All** | `"{6A6E2A002F53A581}"` | `Configs/Tests/OVT_TestGroup_All.conf` | `OVT_TEST_CampaignSuite`, `OVT_TEST_InitSuite`, `OVT_TEST_LogicSuite`, `OVT_TEST_PersistenceSuite`, `OVT_TEST_PersistenceRoundTripSuite` | **42** | exit 0, **~30 s** |

```bash
tools/run-tests.sh "{6A6E29FF47ECB840}"   # Fast — 20 cases
tools/run-tests.sh "{6A6E2A002F53A581}"   # All  — 42 cases
```

Deliberately in **neither** group: `OVT_TEST_MetaSuite` (always red by design)
and `OVT_TEST_SmokeSuite` (asserts nothing the tier suites do not).
`OVT_TEST_PersistenceRoundTripSuite` joined the All group on 2026-08-02, when
its exit-code flip (1 → 0) discharged the `core/persistence` acceptance
criterion and its quarantine was lifted per its own written procedure.

**Recommended CI usage:**

```bash
tools/run-tests.sh "{6A6E29FF47ECB840}"   # every push
tools/run-tests.sh "{6A6E2A002F53A581}"   # nightly / pre-merge / release
```

(`run-tests.sh` resets the OverthrowCI save state itself before every run —
the round-trip suite's fresh-session precondition — unless `OVERTHROW_SAVE_DIR`
pins a fixture.)

The split buys scope, not wall time: All is only ~3 s slower than Fast, but
Fast cannot be reddened by campaign or persistence state. Neither group needs
`OVERTHROW_TEST_TIMEOUT` — both finish inside 20 s against a 300 s default.

**What a green All run does not prove.** It is one client process, so
join-in-progress and everything else multiplayer is untested — that is the most
common regression class in this project. Nor does it cover UI, performance, AI
movement (the navmesh does not load in the test world), or the true
quit-and-continue restart path (the round-trip suite exercises save → dirty →
in-session re-apply through the persistence storage; `SaveGameManager.Load`'s
world transition restarts the autotest harness and cannot be covered here — it
is on the manual play-test checklist). Treat exit 0 as a floor, not a
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

### Persistence round-trip suite (`core/persistence`)

`OVT_TEST_PersistenceRoundTripSuite` was authored quarantined and red on
purpose: its exit-code flip from 1 to 0 **was** the `core/persistence`
migration's acceptance criterion. The flip happened on 2026-08-02 (vanilla
persistence landed) and the suite now runs green as part of the **All** group.

```bash
tools/run-tests.sh OVT_TEST_PersistenceRoundTripSuite   # just the round trip
tools/run-tests.sh "{6A6E2A002F53A581}"                 # whole All group
```

Its capability case asserts the no-save → save **transition**, which needs a
fresh session — `run-tests.sh` establishes that itself by running
`reset_save.sh --profile OverthrowCI` before every launch. The reload half is
an **in-session re-apply** through the persistence storage (save → dirty →
re-apply → assert); the true savepoint → `SaveGameManager.Load` → restored
campaign path restarts the autotest harness and is manual-play-test territory.

**10 cases** since 2026-08-02: the capability gate, eight save→dirty→re-apply
state kinds, and one **per-instance** round trip
(`..._VehicleReserveRelease_KeepsOwnerAndContents`, BUG-086) that takes no
save point at all — it hides one owned vehicle in place, releases it again, and
asserts it is the same entity, still tracked, still owned, locked, placed and
fuelled. That case
uses neither of the suite's two persistence-layer seams, so it cannot disturb
the fresh-session precondition above.

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

`run-tests.sh` performs the reset itself before every launch, so the manual
step above is only needed outside the harness. The reset is **load-bearing for
the round-trip suite**, whose first case asserts that no save exists before it
saves and one exists after.

**Save layout (established empirically 2026-08-02):** vanilla persistence
writes save points to
`<My Games>/<profile>/profile/.save/app<appid>_user<uid>/game/<mission>/playthrough<NNN>/savepoint<NNN>/`
(`meta-info.json` + `WorldState/*.blob`); the test world's mission dir is
`OVT-Campaign-Test`. `reset_save.sh` deletes both the legacy EPF `.db/Overthrow`
tree and every `profile/.save/*/game` dir (never `settings/`).
`backup_save.sh`/`activate_save.sh` still speak only EPF and are pending the
fixture rework (`docs/features/core/persistence/tasks.md`, Phase 6).

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
| `OVERTHROW_SERVER_EXE` | launch-server | `<steam>/common/Arma Reforger Server/ArmaReforgerServerDiag.exe` (separate Steam app 1874900) |
| `OVERTHROW_SERVER_PROFILE` | launch-server | `OverthrowDS` (deliberately not the CI profile) |
| `OVERTHROW_SERVER_TIMEOUT` | launch-server | `86400` (seconds — i.e. until Ctrl-C) |
| `OVERTHROW_SERVER_ADDONS_DIRS` | launch-server | `<repo parent>` in Windows form — what makes the working tree visible |
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

## `tools/check-placeables.py`

Static asset check, no Workbench and no game: verifies that every prefab listed
in **`placeables.conf` and `buildables.conf`** really ends up carrying its
Overthrow component (`OVT_PlaceableComponent` / `OVT_BuildableComponent`) **and
an `RplComponent`**, by walking each prefab's **inheritance chain** across both
the mod and the base-game reference tree.

```
tools/check-placeables.py [--reforger DIR] [--conf FILE] [--quiet] [--verbose] [--strict]
```

| Flag | Meaning |
|---|---|
| `--reforger <dir>` | Base-game reference tree. Default `../ArmaReforger`, or `$OVERTHROW_REFORGER_DIR`. |
| `--conf <file>` | Check only one config — `Configs/Resistance/placeables.conf` or `Configs/Resistance/buildables.conf`. Default: both. |
| `--quiet` | Only problems and the summary. |
| `--verbose` | Print each prefab's resolved type, where its `RplComponent` came from, and the full chain (`*` marks a mod file that shadows a game path). |
| `--strict` | Also fail when replication could not be **proven** (chain truncated outside both trees), not only when it is proven missing. |

| Exit | Meaning |
|---|---|
| `0` | Every prefab resolves to a component and to replication — verified clean (warnings may still print). |
| `1` | At least one problem; details on stdout. |
| `2` | Indeterminate — a config or the reference tree could not be read. |

**The replication half.** `PlaceItem()`/`BuildItem()` spawn the real object on
the **server** (`OVT_Global.SpawnEntityPrefabMatrix`). A runtime-spawned entity
with no `RplComponent` is server-local — on a dedicated server nobody sees the
thing they just paid for. The placement *ghost* is spawned client-side by
`OVT_PlaceContext` and looks perfectly normal, and a listen-server host (server
and client in one process) cannot reproduce the fault at all, which is what makes
it so easy to ship. The trap is inheritance: several placeables are built on
static base-game props that never needed replication because vanilla only ever
places them at design time.

Three verdicts, deliberately distinct:

- **proven present** — an `RplComponent` was found in the chain and nothing
  disables it (an explicit `Enabled 0` at the most derived level that states one
  is a failure, same as absent).
- **proven missing** — the whole chain resolved and no level declares one. Fails.
- **unproven** — the chain stops at a file in neither tree, so the tool cannot
  see whether the base carries one. Warns by default (`--strict` fails). This is
  not paranoia: base-game prefabs declare `RplComponent` all the time, some
  recorded paths are stale (`Prefabs/Props/Core/Destructible_Props_Base.et` no
  longer exists under that name), and the reference tree publishes no `.meta`
  files, so a GUID cannot be re-resolved to the file's real path.

**Same-path overrides are deltas.** Where the mod shadows a game path, the script
reads **both** files and unions their components — mod first, so the mod wins on
"most derived". `Prefabs/Structures/Signs/Signs_Base.et` in the mod contains
nothing but `OVT_PlaceableComponent`, while the game's file at that path supplies
the mesh, the rigid body and the `RplComponent`; both are live at runtime.
Reading only the mod side reports all 16 signs as unreplicated — backwards.

**Why it is not a grep.** A placed or built object is saved only because the
persistence rule matches `ComponentClass "OVT_PlaceableComponent"` (and the same
rule again for `OVT_BuildableComponent`), is ownable only because
`PlaceItem()`/`BuildItem()` find that component, and is visible to
`OVT_PlaceableItemJobStage` / `OVT_TownPlaceableCountJobCondition` only through
its `m_sPlaceableType` — which is *only* ever a prefab attribute. None of that is
visible to `compile-check.sh` (it compiles scripts, not prefabs) or to the
autotests, and the failure is silent: the object places, looks correct, and
evaporates on continue. `OVT_RecruitmentTent.et` shipped in exactly that state
until 2026-08-06, when this check found it.

The component is usually **not** on the prefab named in the config. Overthrow
overrides shared base prefabs (`FurnitureMilitary_base.et`, `Signs_Base.et`,
`LampKerosene_01_base.et`) by shipping a file at the **same path with the same
GUID** as the game's, adding nothing but the component — so dozens of untouched
base-game leaves inherit it. Answering "is this placeable wired up?" therefore
means resolving the whole chain, which is what this does.

**Problems** (exit 1): no component anywhere in a chain; a head prefab in neither
tree; a config GUID that disagrees with the file's own `.meta` (an override that
is silently *not* an override); the component declared at two levels under
different GUIDs (a duplicate rather than an override); and, when
`m_bRandomizePrefab` is set, variants resolving to different types.

**Warnings** (exit 0): a component with no type attribute (saved and ownable, but
invisible to type-aware features); a type string that differs from the config's
`m_sName`; and variants with mixed types where the *player* picks the variant.
The six standing warnings are all buildables whose `m_sName` is a display name
with spaces (`"Guard Tower"`) against a CamelCase type (`"GuardTower"`) — that is
the convention, not a defect, and the warning exists so a job author uses the
type string rather than the display name.

Run it after touching either config or any placeable/buildable prefab, and before
shipping a job or feature that matches on type.

Requires `python3` (WSL default). A truncated chain is not a fault: base-game
paths recorded in a `.et` can be stale (the engine resolves by GUID) and the
reference tree is a partial extract — and since resolution checks the mod first,
anything unresolvable is by definition not a mod file and cannot declare an
Overthrow component.

---

## `tools/check-shop-coverage.py`

Static config check, no Workbench and no game: reports every base-game catalogue
item that **no shop rule can ever stock**. Shop inventories are catalogue
*queries* (`OVT_ShopInventoryItem`: type + mode + `m_sFind`), so new vanilla
items flow onto the shelves for free — until Bohemia adds a new
`SCR_EArsenalItemType` (HANDWEAR, RADIO_BACKPACK) or files items under a mode
no rule asks for (WEAPON_VARIANTS). Then the item is priced, registered and
lootable but unbuyable, and nobody notices until a player asks where the gloves
are (the 2026-08-23 sweep found 33 such items).

```
tools/check-shop-coverage.py [--reforger DIR] [--mode missing|summary|all]
                             [--ignore PATTERN]... [--no-default-ignores] [--quiet]
```

| Flag | Meaning |
|---|---|
| `--reforger <dir>` | Base-game reference tree. Default `../ArmaReforger`, or `$OVERTHROW_REFORGER_DIR`. |
| `--mode missing` | (default) Unreachable items grouped by type/mode. |
| `--mode summary` | Reachable/missing counts per type/mode. |
| `--mode all` | Every sellable item with the shops whose rules match it (`(rnd)` = `m_bSingleRandomItem`). |
| `--ignore <pattern>` | Prefab-path substring to treat as deliberately unsold; repeatable, adds to the built-in list. |
| `--no-default-ignores` | Drop the built-in design-exclusion list (crew-served/mortar parts, sandbags, barbed tape, mortar/heli/vehicle ammo, ballistic tables, the FIA tent). |
| `--quiet` | Summary line only (stderr). |

| Exit | Meaning |
|---|---|
| `0` | Every non-ignored item is reachable by at least one rule in `ShopConfig.conf` / `GunDealerConfig.conf`. |
| `1` | Unreachable items found; details on stdout. |
| `2` | Indeterminate — a config, the faction manager prefab or the reference tree could not be read. |

It mirrors the runtime: the universe is the ITEM catalogue of every faction
`OVT_OverthrowFactionManager.et` loads (resolving same-GUID mod deltas over the
vanilla confs), minus disabled entries, `SCR_NonArsenalItemCostCatalogData`
entries and anything a `hidden 1` `OVT_PriceConfig` matches
(`BuildResourceDatabase`); a rule matches under `FindInventoryItems` semantics
(type equal, `m_sFind` substring, mode `DEFAULT` = any,
`m_bIncludeSupportStationItems 0` drops SUPPORT_STATION). Faction include flags
and `m_bSingleRandomItem` are stocking choices, not reachability, and are
ignored. **Run it after every Reforger update** (`/update-reforger`) and after
editing either shop config.

---

## `tools/decode-savepoint.py`

Reads a Reforger persistence save point — what is in it, what changed between
two of them, and where a given persistence id lives. **Offline; it never
launches the game.**

This exists because four persistence bugs in a row (BUG-086, BUG-104, BUG-116,
BUG-118) could only be settled by reading the blob. Logs report what the engine
*said*; the blob is what is actually stored, and when they disagree the blob
wins. BUG-118's headline figure (+492 records per restart, 0 removed) is one
`diff` invocation.

```bash
tools/decode-savepoint.py summary <path>...          # size, records, per-collection counts
tools/decode-savepoint.py diff <old> <new>           # deltas, added/removed, mint-time buckets
tools/decode-savepoint.py ids <path> [--since T]     # every record id with its creation time
tools/decode-savepoint.py find <path> <uuid>         # locate an id, all bit alignments
tools/decode-savepoint.py prefab <path> <GUID>...    # count records per prefab GUID
tools/decode-savepoint.py strings <path> [--min N]   # readable text incl. bit-packed payloads
```

`<path>` is a `.blob`, a `savepoint<NNN>` directory, or a `playthrough<NNN>`
directory (every save point inside it, oldest first). Save points live at
`<profile>/.save/[app<id>_user<id>/]game/<mission>/playthrough<NNN>/savepoint<NNN>/`.

### Exit codes

| code | meaning |
|---|---|
| 0 | ok |
| 1 | bad usage, unreadable path, or `find` did not locate the id |

### What you need to know to read the output

- **A save point is a FULL snapshot, not a delta** — verified across three
  campaigns; two *consecutive* save points held 946 and 944 records. If a
  record is not in the loaded save point, it was not loaded.
- **An absent `Item` section is normal**, not missing data: vanilla's
  `Item.conf` leaves `StorageRoot` false, so an item in a container is written
  as a nested child of its parent's record, never as a root record.
- **An id appearing twice is usually legitimate** — cross-collection pairs are
  references (AIGroup→Character, System→Vehicle). Only two occurrences in the
  *same* collection indicate a genuine duplicate record.
- Ids carry their creation time (UUID v8, 48-bit ms prefix), which is what
  distinguishes "written by the session that broke" from "inherited from last
  week".

The blob format, its bit-packed scripted sub-streams and the false-positive
rules for id detection are documented in the script's own header. For the wider
investigation workflow — which logs to pull, which traps to avoid, how to
reproduce locally — see the `persistence-forensics` skill.

---

## Utilities

```bash
tools/check-placeables.py --quiet                 # placeable/buildable prefabs: component + RplComponent reachable?
tools/check-shop-coverage.py --quiet             # every base-game catalogue item reachable by some shop rule?
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
- **MP testing is manual.** `launch-server.sh` + `launch-game.sh -- -client
  <ip:port>` gives a verified one-command server and a verified one-command
  join, but nothing drives them: **two concurrent clients, JIP and automated
  MP testing do not exist.** The single-client handshake is proven; everything
  built on top of it is not.

## Maintenance

After a Reforger/Tools update, re-run the verification set in
`docs/features/dev-ops/workbench-automation/findings.md` (exit codes, log
shapes, `Loaded addons:` block, timings) before trusting the tools again. The
parser is deliberately brittle-loud: unknown `SCRIPT (E)` shapes are surfaced
as warnings and force exit 2 (indeterminate) rather than being dropped — if
the log format drifts, the check fails loudly instead of going silently green.
