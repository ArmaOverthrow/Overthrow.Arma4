#!/bin/bash
#
# tools/launch-server.sh — run a local Arma Reforger dedicated server with
# Overthrow loaded FROM THIS WORKING TREE, so multiplayer behaviour can be
# play-tested without publishing anything.
#
# This is the MP counterpart to tools/launch-game.sh. It exists because the
# single-client autotest harness (tools/run-tests.sh) cannot cover the
# project's most common regression class — JIP and multiplayer — which needs
# a real server and real remote clients.
#
# Two modes, both verified on 2026-08-06 (Reforger 1.7.0.54, server app
# 1874900). They differ in ways that matter; pick deliberately:
#
#   --mode local (default)   -server <scenario>
#       Loads ONLY core + ArmaReforger + this working tree. No workshop
#       lookup, no download, works offline, listening ~5s after launch.
#       No backend registration, so no Direct Join code and no RCON —
#       clients join by IP:port. This is the faithful "test exactly what is
#       in my checkout" mode and the right default for play-testing.
#
#   --mode dedicated         -config <server.json>
#       The full dedicated-server path: JSON config (persistence, AI limits,
#       view distance...), backend registration with a Direct Join code, and
#       RCON. The working tree still WINS over the published build for the
#       Overthrow addon itself (verified: the mounted addon is this repo, not
#       the downloaded package) — BUT resolving the mod through the workshop
#       also pulls in the published build's dependencies, currently EPF and
#       EDF, and those DO get mounted. This tree no longer uses EPF, so this
#       mode runs with two addons that mode 'local' does not have. Slower to
#       start (~38s) and requires the backend to be reachable.
#
# The dedicated server binary is a SEPARATE Steam app (1874900, "Arma
# Reforger Server") — it is not part of the game client install.
#
# stdout carries ONLY the KEY=value contract lines; all diagnostics go to
# stderr.

set -euo pipefail

_LS_SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
source "$_LS_SCRIPT_DIR/lib/common.sh"

OUT_DIR="$OVT_REPO_ROOT/.tmp/launch-server"
DIAG_FILE="$OUT_DIR/last-launch.txt"
GEN_CONFIG="$OUT_DIR/serverconfig.json"

# Scenario shorthands → the mission header resource names. A mission header
# (.conf) is required: pointing the server straight at a .ent world skips the
# header the loading flow needs.
SCENARIO_TESTWORLD='{6B0E7A50D1E2F3A4}Missions/25_OVT_TestWorld.conf'
SCENARIO_EDEN='{3DAD390C31623F04}Missions/24_OVT_Eden.conf'

# Opts the session in to synthesised DEV_<playerId> UIDs. Must match
# OVT_Global.DEV_UID_CLI_PARAM.
DEV_UID_PARAM="ovtDevUid"

usage() {
    cat <<EOF
usage: tools/launch-server.sh [--scenario testworld|eden|<resource>]
                              [--mode local|dedicated] [--config <file>]
                              [--port <n>] [--max-players <n>]
                              [--profile <name>] [--admin-password <pw>]
                              [--timeout <s>] [--quiet] [--allow-concurrent]
                              [-h|--help] [-- <server args...>]

Runs a dedicated server with Overthrow loaded from this working tree and
blocks until it exits. A server does not stop on its own — press Ctrl-C to
shut it down (the Windows process is killed and verified, no orphan left).

Options:
  --scenario <s>    testworld (default — 1 town, small, fast), eden (the real
                    campaign), or a full resource name '{GUID}Missions/x.conf'.
  --mode <m>        local (default) or dedicated. See the header of this file
                    for the difference; it is not cosmetic.
                    🔴 REFUSED without --config: dedicated mode declares the mod
                    in the config's mods[], and a mods[] entry pointing at a
                    SOURCE project makes the server PACK this repo into
                    data.pak and DELETE non-addon files from the repo root
                    (verified 2026-08-24: it removed README.md, LICENSE.md,
                    CHANGES.md, .gitignore, update-arma-scripts.ps1 and
                    CLAUDE.md). The pak then silently overrides the sources for
                    every later launch. Use local mode.
  --config <file>   dedicated mode only: use this server config verbatim
                    instead of the generated one. --port/--max-players/
                    --scenario/--admin-password are then NOT applied.
  --port <n>        RPL bind port. Default 2001. DEDICATED MODE ONLY — the
                    -server route has no bind-port flag and the engine always
                    falls back to 2001, so local mode rejects any other value
                    rather than lie about where it is listening.
  --max-players <n> Default 8.
  --profile <name>  Profile dir + log resolution. Default:
                    \$OVERTHROW_SERVER_PROFILE, else OverthrowDS. Deliberately
                    NOT the OverthrowCI profile the test harness uses.
  --admin-password <pw>  Server admin password. Default: devadmin.
  --timeout <s>     Kill the server after this many seconds. Default:
                    \$OVERTHROW_SERVER_TIMEOUT, else 86400 (i.e. "until
                    Ctrl-C"). Short values are for automation, where exit 124
                    means the server stayed up for the whole window.
  --quiet           Silence info-level diagnostics on stderr.
  --allow-concurrent  Suppress the warning when another server process is
                    already running (a second server on the same port will
                    fail to bind).
  --difficulty <n>  Difficulty preset NAME to write into
                    \$profile:Overthrow_Config.json (matched by name against the
                    game mode's preset list). Defaults to 'Test World' for the
                    testworld scenario — that preset ships with the test world
                    only and gives plenty of starting cash — and to leaving the
                    file's existing value alone for every other scenario.
                    Presets in the base game mode: Easy, Normal, Hard, Extreme,
                    Insane.
  --no-config       Do not touch Overthrow_Config.json at all.
  --no-dev-uid      Do NOT pass -$DEV_UID_PARAM in local mode. Local mode has no
                    backend, so nobody is authenticated and every player's
                    identity id is empty — which stalls Overthrow's spawn path
                    forever. The flag makes the game synthesise DEV_<playerId>
                    UIDs instead, and without it players connect but never
                    spawn. Only pass this if you are deliberately testing the
                    no-identity path.
  -h, --help        This help.

Everything after '--' is passed to the server verbatim, after the defaults.

Joining the server:
  Same machine  — Reforger → Multiplayer → Direct Join → 127.0.0.1:<port>
  Another PC    — Direct Join → <this machine's LAN IP>:<port>
  In 'dedicated' mode a Direct Join Code is also printed once the server
  registers with the backend.

  A client joining a source-built server must load the SAME source build, or
  the connection is refused on a checksum mismatch ("remote script source
  code checksum does not match"). Launch the client from this tree with
  tools/launch-game.sh, or from the Workbench.

Exit codes:
  0    The server exited on its own AND its log directory was resolved. For a
       server this usually means it FAILED to stay up — check the log.
  2    Tool/environment failure: missing server binary, bad arguments, or the
       log directory could not be resolved.
  124  Timed out — i.e. the server ran for the whole --timeout window. For a
       server this is the SUCCESS shape when a timeout was set deliberately.
  130/143  Interrupted (Ctrl-C / SIGTERM). The server process is killed and
       the kill verified before this script exits.

Environment:
  OVERTHROW_SERVER_EXE       WSL path of ArmaReforgerServerDiag.exe.
  OVERTHROW_SERVER_PROFILE   Default profile name (default OverthrowDS).
  OVERTHROW_SERVER_TIMEOUT   Default --timeout in seconds (default 86400).
  OVERTHROW_SERVER_ADDONS_DIRS  Comma-separated WINDOWS-form -addonsDir value.
                             Default: <repo parent>, which is what makes the
                             working tree visible to the server.
  OVERTHROW_MYGAMES_DIR      WSL path of 'My Games' (else discovered).

Notes:
  * Do NOT pass -gproj: the server resolves the base game from its own
    ./addons/data, and the launcher sets cwd accordingly.
  * -config and -addons are mutually exclusive (the engine treats it as a
    fatal error), which is why dedicated mode declares the mod in the config's
    mods[] rather than on the command line.
  * -addonsDir exposes every sibling project under <repo parent> as a
    candidate addon; only the ones selected actually load.
  * Every launched Windows PID is registered in .tmp/ovt-pids/ while it runs.
    Reap orphans with: bash tools/lib/common.sh --sweep-stale [--kill]
EOF
}

# --- argument parsing ---------------------------------------------------------

SCENARIO="$SCENARIO_TESTWORLD"
MODE="local"
CONFIG_FILE=""
PORT=2001
MAX_PLAYERS=8
PROFILE="${OVERTHROW_SERVER_PROFILE:-OverthrowDS}"
ADMIN_PASSWORD="devadmin"
TIMEOUT_S="${OVERTHROW_SERVER_TIMEOUT:-86400}"
QUIET=0
ALLOW_CONCURRENT=0
DEV_UID=1
CONFIG_DIFFICULTY_ENABLED=1
# Empty = leave whatever is in Overthrow_Config.json. Set from the scenario
# after parsing unless the caller named one explicitly.
DIFFICULTY=""
DIFFICULTY_EXPLICIT=0
PASS_ARGS=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --scenario)
            if [[ $# -lt 2 || -z "${2:-}" ]]; then
                ovt_err "--scenario requires a value (testworld, eden, or a full resource name)"
                exit 2
            fi
            case "${2,,}" in
                testworld) SCENARIO="$SCENARIO_TESTWORLD" ;;
                eden)      SCENARIO="$SCENARIO_EDEN" ;;
                *)
                    if [[ "$2" != \{*\}*.conf ]]; then
                        ovt_err "--scenario '$2' is neither a shorthand (testworld, eden) nor a '{GUID}Missions/x.conf' resource name"
                        exit 2
                    fi
                    SCENARIO="$2"
                    ;;
            esac
            shift 2
            ;;
        --mode)
            case "${2:-}" in
                local|dedicated) MODE="$2" ;;
                *) ovt_err "--mode must be 'local' or 'dedicated'"; exit 2 ;;
            esac
            shift 2
            ;;
        --config)
            if [[ $# -lt 2 || ! -f "${2:-}" ]]; then
                ovt_err "--config requires a path to an existing server config JSON"
                exit 2
            fi
            CONFIG_FILE="$2"
            shift 2
            ;;
        --port)
            if [[ $# -lt 2 || ! "${2:-}" =~ ^[0-9]+$ ]] || (( $2 < 1 || $2 > 65535 )); then
                ovt_err "--port requires a port number between 1 and 65535"
                exit 2
            fi
            PORT="$2"
            shift 2
            ;;
        --max-players)
            if [[ $# -lt 2 || ! "${2:-}" =~ ^[0-9]+$ ]] || (( $2 < 1 )); then
                ovt_err "--max-players requires a positive integer"
                exit 2
            fi
            MAX_PLAYERS="$2"
            shift 2
            ;;
        --profile)
            if [[ $# -lt 2 || -z "${2:-}" ]]; then
                ovt_err "--profile requires a profile name"
                exit 2
            fi
            PROFILE="$2"
            shift 2
            ;;
        --admin-password)
            if [[ $# -lt 2 || -z "${2:-}" ]]; then
                ovt_err "--admin-password requires a value"
                exit 2
            fi
            ADMIN_PASSWORD="$2"
            shift 2
            ;;
        --timeout)
            if [[ $# -lt 2 || ! "${2:-}" =~ ^[0-9]+$ || "${2}" -lt 1 ]]; then
                ovt_err "--timeout requires a positive integer number of seconds"
                exit 2
            fi
            TIMEOUT_S="$2"
            shift 2
            ;;
        --quiet) QUIET=1; shift ;;
        --allow-concurrent) ALLOW_CONCURRENT=1; shift ;;
        --no-dev-uid) DEV_UID=0; shift ;;
        --difficulty)
            if [[ $# -lt 2 || -z "${2:-}" ]]; then
                ovt_err "--difficulty requires a preset NAME as it appears in the game mode's preset list (e.g. 'Test World', Easy, Normal, Hard, Extreme, Insane)"
                exit 2
            fi
            DIFFICULTY="$2"
            DIFFICULTY_EXPLICIT=1
            shift 2
            ;;
        --no-config) CONFIG_DIFFICULTY_ENABLED=0; shift ;;
        -h|--help) usage; exit 0 ;;
        --)
            shift
            PASS_ARGS=("$@")
            break
            ;;
        *)
            ovt_err "unknown argument '$1' — server arguments go after '--' (see --help)"
            exit 2
            ;;
    esac
done

if [[ ! "$TIMEOUT_S" =~ ^[0-9]+$ ]] || (( TIMEOUT_S < 1 )); then
    ovt_err "OVERTHROW_SERVER_TIMEOUT must be a positive integer, got '$TIMEOUT_S'"
    exit 2
fi
if [[ -n "$CONFIG_FILE" && "$MODE" != "dedicated" ]]; then
    ovt_err "--config only applies to --mode dedicated (mode 'local' takes the scenario on the command line)"
    exit 2
fi
# 🔴 DEDICATED MODE PACKS THE WORKING TREE AND DELETES FILES FROM ITS ROOT.
#
# Verified destructively 2026-08-24. In dedicated mode the mod cannot be named
# with -addons (mutually exclusive with -config), so it is declared in the
# config's mods[] by GUID. The dedicated server treats a mods[] entry that
# resolves to a SOURCE project as something to BUILD: it runs the workshop pack
# pipeline in place, writing data.pak, *_manifest.json, ServerData.json, meta/
# and thumbnail.png into the project root - and PRUNING that root of everything
# that is not addon content.
#
# What it removed from this repo on 2026-08-24, in one run:
#     .gitignore  CHANGES.md  CLAUDE.md  CLAUDE.md.example  LICENSE.md
#     README.md   update-arma-scripts.ps1
# Six were tracked and came back with `git checkout --`. CLAUDE.md is
# gitignored and could NOT be recovered from git.
#
# It is also silently self-perpetuating: once data.pak exists in the tree, every
# later launch of ANY tool loads the PACKED build instead of the sources, and
# the log's 'Loaded addons:' block still names the source path. That cost a full
# session of debugging "why is my code not running" before the pak was found.
#
# So: refuse. --mode local uses -server + -addons, never touches the tree, and
# is what the verified join recipe in tools/README.md uses.
if [[ "$MODE" == "dedicated" && -z "${CONFIG_FILE:-}" ]]; then
    ovt_err "--mode dedicated is REFUSED against the working tree: it packs this repo into data.pak and DELETES non-addon files from the repo root (on 2026-08-24 it removed README.md, LICENSE.md, CHANGES.md, .gitignore, update-arma-scripts.ps1 and CLAUDE.md — the last unrecoverable from git). Use --mode local, which is the default and what the verified client-join recipe uses. If you genuinely need a dedicated server, pass --config <file> naming a mods[] entry that points at a PUBLISHED addon, never at this project."
    exit 2
fi

if [[ "$MODE" == "local" ]] && (( PORT != 2001 )); then
    # Verified 2026-08-06: the -server route has no bind-port flag. -bindPort
    # is accepted on the command line and ignored; the engine logs "RPL listen
    # address not specified. Using default fallback." and binds 2001. Failing
    # here beats reporting a BIND_PORT the server is not listening on.
    ovt_err "--port is not supported in --mode local — the engine has no bind-port flag for the -server route and always falls back to 2001. Use --mode dedicated, where the port comes from the server config."
    exit 2
fi

# The test world ships its own 'Test World' difficulty preset (generous starting
# cash, sized for play-testing). It is only in the preset list for that
# scenario, so it is only selected for that scenario.
if (( ! DIFFICULTY_EXPLICIT )) && [[ "$SCENARIO" == "$SCENARIO_TESTWORLD" ]]; then
    DIFFICULTY="Test World"
fi

info() { if (( ! QUIET )); then ovt_info "$@"; fi; }

# passthrough_has_flag <-flagname> — case-insensitive exact-token match in the
# pass-through args, so a caller-supplied flag is never duplicated. Same helper
# and same contract as tools/launch-game.sh.
passthrough_has_flag() {
    local want="${1,,}" a
    for a in ${PASS_ARGS[@]+"${PASS_ARGS[@]}"}; do
        if [[ "${a,,}" == "$want" ]]; then
            return 0
        fi
    done
    return 1
}

# --- resolution ---------------------------------------------------------------


# --- pass-through sanity: OUR flags do not belong after '--' -------------------
# Everything after '--' goes to the client verbatim, so a launcher flag typed
# there is silently ignored - the client shrugs at an argument it does not know
# and the script keeps its default. Observed 2026-08-24: '-- -client ... --timeout
# 60000' ran with the default 600 s and the client was killed mid-session, which
# reads as "the timeout flag does not work".
for _arg in "${PASS_ARGS[@]}"; do
    case "$_arg" in
        --timeout|--profile|--quiet|--allow-concurrent|--scenario|--mode|--port|--max-players|--config|--admin-password)
            ovt_err "'$_arg' is a launcher flag but appears AFTER '--', so it was passed to the server and ignored by this script. Put launcher flags BEFORE '--'."
            exit 2
            ;;
    esac
done

ovt_assert_unpacked || exit 2

SERVER_EXE="$(ovt_server_exe)" || exit 2

ADDONS_DIR_ARG="${OVERTHROW_SERVER_ADDONS_DIRS:-}"
if [[ -z "$ADDONS_DIR_ARG" ]]; then
    # 🔴 A FARM CONTAINING ONLY THIS WORKTREE - NOT THE REPO PARENT. The parent
    # holds every sibling checkout, they all share addon ID 'Overthrow' and GUID
    # 59B657D731E2A11D, and '-addons Overthrow' then resolves to whichever the
    # engine picks. That is not hypothetical: on 2026-08-24 a server launched
    # from v1.5 ran Overthrow.Arma4-main for a whole session, and the log's
    # 'Loaded addons:' block still named the v1.5 path. See ovt_addon_farm().
    ADDONS_DIR_ARG="$(ovt_addon_farm)" || exit 2
fi

mkdir -p "$OUT_DIR"

# --- server config (dedicated mode) -------------------------------------------

# write_server_config <path> — generate the dedicated-server JSON. Kept
# minimal and readable rather than exhaustive: every field here is either
# required, or something a local play-test actually wants to control. The
# full schema is at <server install>/addons/core/data/backend/DSConfigSchema.json.
write_server_config() {
    local path="$1"
    cat > "$path" <<EOF
{
	"bindAddress": "0.0.0.0",
	"bindPort": $PORT,
	"publicAddress": "127.0.0.1",
	"publicPort": $PORT,
	"a2s": {
		"address": "0.0.0.0",
		"port": $(( PORT + 15776 ))
	},
	"rcon": {
		"address": "127.0.0.1",
		"port": $(( PORT + 17998 )),
		"password": "$ADMIN_PASSWORD",
		"permission": "admin",
		"maxClients": 4
	},
	"game": {
		"name": "Overthrow DEV (local)",
		"password": "",
		"passwordAdmin": "$ADMIN_PASSWORD",
		"scenarioId": "$SCENARIO",
		"maxPlayers": $MAX_PLAYERS,
		"visible": false,
		"crossPlatform": false,
		"supportedPlatforms": ["PLATFORM_PC"],
		"modsRequiredByDefault": true,
		"mods": [
			{ "modId": "59B657D731E2A11D", "name": "Overthrow" }
		],
		"gameProperties": {
			"serverMaxViewDistance": 1600,
			"networkViewDistance": 1000,
			"serverMinGrassDistance": 50,
			"fastValidation": true,
			"battlEye": false
		}
	},
	"operating": {
		"lobbyPlayerSynchronise": true,
		"disableCrashReporter": true
	}
}
EOF
}

# --- Overthrow_Config.json ----------------------------------------------------

# Overthrow reads $profile:Overthrow_Config.json at startup and, when the file
# is absent, WRITES ONE ITSELF with defaults (difficulty ""). So "create it if
# missing" is useless after the first launch ever - by then the game has
# already made an empty one. This patches the difficulty field in place
# instead, leaving every other field (officers, item limits, webhook...) alone.
#
# The difficulty is matched BY NAME against the game mode's preset list
# (OVT_OverthrowGameMode.DoStartGame). 'Test World' is only in that list for
# the test world scenario, which is why it is not applied to eden.
ensure_overthrow_config() {
    local want="$1" profile_root cfg_dir cfg current

    profile_root="$(ovt_profile_dir "$PROFILE")" || return 2
    cfg_dir="$profile_root/profile"
    cfg="$cfg_dir/Overthrow_Config.json"

    if ! mkdir -p "$cfg_dir" 2>/dev/null; then
        ovt_warn "could not create '$cfg_dir' — leaving Overthrow_Config.json alone"
        return 0
    fi

    if [[ ! -f "$cfg" ]]; then
        # No file yet: write a minimal one. The game fills in every field it
        # does not find, so only the difficulty needs stating.
        printf '{"difficulty":"%s"}\n' "$want" > "$cfg"
        info "Overthrow_Config.json created with difficulty '$want'"
        return 0
    fi

    if ! grep -q '"difficulty"' "$cfg"; then
        ovt_warn "Overthrow_Config.json has no 'difficulty' field — not editing it. Add '\"difficulty\": \"$want\"' by hand, or delete the file and relaunch."
        return 0
    fi

    current="$(sed -n 's/.*"difficulty"[[:space:]]*:[[:space:]]*"\([^"]*\)".*/\1/p' "$cfg" | head -1)"
    if [[ "$current" == "$want" ]]; then
        info "Overthrow_Config.json difficulty already '$want'"
        return 0
    fi

    if sed -i "s/\"difficulty\"[[:space:]]*:[[:space:]]*\"[^\"]*\"/\"difficulty\":\"$want\"/" "$cfg"; then
        info "Overthrow_Config.json difficulty '${current:-<empty>}' -> '$want' (other settings untouched)"
    else
        ovt_warn "could not patch difficulty in '$cfg'"
    fi
}

if (( CONFIG_DIFFICULTY_ENABLED )); then
    if [[ -n "$DIFFICULTY" ]]; then
        ensure_overthrow_config "$DIFFICULTY" || exit 2
    fi
fi

# --- argument assembly --------------------------------------------------------

ARGS=( -addonsDir "$ADDONS_DIR_ARG" -profile "$PROFILE" -logLevel normal -nothrow -maxFPS 60 )

if [[ "$MODE" == "local" ]]; then
    ARGS=( -server "$SCENARIO" -addons Overthrow "${ARGS[@]}"
           -maxPlayers "$MAX_PLAYERS" -adminPassword "$ADMIN_PASSWORD" )
    # Local mode never contacts the backend, so nobody is authenticated and every
    # player's identity id stays empty - which stalls Overthrow's spawn path
    # forever (OVT_Global.GetPlayerUID). Opt this session in to synthesised
    # DEV_<playerId> UIDs. Deliberately NOT passed in dedicated mode: there the
    # backend does issue real identities, and an empty one merely means "still
    # authenticating", which must be waited for rather than papered over.
    if (( ! DEV_UID )); then
        info "dev UIDs disabled (--no-dev-uid) — players will not spawn unless the backend issues identities"
    elif passthrough_has_flag "-$DEV_UID_PARAM"; then
        info "dev UIDs: -$DEV_UID_PARAM already supplied in the pass-through args"
    else
        ARGS+=( "-$DEV_UID_PARAM" )
    fi
else
    if [[ -z "$CONFIG_FILE" ]]; then
        write_server_config "$GEN_CONFIG"
        CONFIG_FILE="$GEN_CONFIG"
        info "generated server config: $GEN_CONFIG"
    fi
    CONFIG_WIN="$(ovt_win_path "$CONFIG_FILE")" || exit 2
    ARGS=( -config "$CONFIG_WIN" "${ARGS[@]}" )
fi

ARGS+=( ${PASS_ARGS[@]+"${PASS_ARGS[@]}"} )

# --- concurrency warning ------------------------------------------------------

if (( ! ALLOW_CONCURRENT )); then
    existing="$(ovt_pids_of "$(basename "$SERVER_EXE")" 2>/dev/null)" || existing=""
    if [[ -n "$existing" ]]; then
        ovt_warn "another server process is already running (PID(s): ${existing//$'\n'/ }). A second server on port $PORT will fail to bind. Use --port to run both, or stop the other one."
    fi
fi

# --- readiness watcher --------------------------------------------------------

# The server prints one unambiguous line when it is accepting connections.
# Watch for it in the background so the operator is told when to join,
# instead of guessing from a wall of entity spam.
READY_MARKER='Starting RPL server, listening on address'
WATCHER_PID=""

start_ready_watcher() {
    local since="$1"
    (
        local waited=0 log_dir="" line=""
        while (( waited < 300 )); do
            if [[ -z "$log_dir" ]]; then
                log_dir="$(ovt_resolve_log_dir "$PROFILE" "$since" 2>/dev/null)" || log_dir=""
            fi
            if [[ -n "$log_dir" && -f "$log_dir/console.log" ]]; then
                line="$(grep -m1 -F "$READY_MARKER" "$log_dir/console.log" 2>/dev/null)" || line=""
                if [[ -n "$line" ]]; then
                    ovt_info "SERVER READY — ${line##*NETWORK      : }"
                    ovt_info "join with Direct Join: 127.0.0.1:$PORT (or <LAN IP>:$PORT from another PC)"
                    if [[ "$MODE" == "dedicated" ]]; then
                        # Backend registration completes a few seconds AFTER
                        # the RPL listener is up, so the code is not in the
                        # log yet at this point — wait a bounded while.
                        local code="" waited_code=0
                        while (( waited_code < 30 )); do
                            code="$(grep -m1 -F 'Direct Join Code:' "$log_dir/console.log" 2>/dev/null)" || code=""
                            if [[ -n "$code" ]]; then
                                ovt_info "${code##*BACKEND      : }"
                                break
                            fi
                            sleep 1
                            waited_code=$(( waited_code + 1 ))
                        done
                        if [[ -z "$code" ]]; then
                            ovt_warn "server is listening but did not register with the backend within 30s — no Direct Join Code. Joining by IP still works."
                        fi
                    fi
                    ovt_info "log: $log_dir"
                    exit 0
                fi
            fi
            sleep 1
            waited=$(( waited + 1 ))
        done
        ovt_warn "server did not report '$READY_MARKER' within 300s — check the log directory"
    ) &
    WATCHER_PID=$!
}

stop_ready_watcher() {
    if [[ -n "$WATCHER_PID" ]]; then
        kill "$WATCHER_PID" 2>/dev/null || true
        wait "$WATCHER_PID" 2>/dev/null || true
        WATCHER_PID=""
    fi
}

# --- run ----------------------------------------------------------------------

info "server exe : $SERVER_EXE"
info "mode       : $MODE"
info "scenario   : $SCENARIO"
info "addonsDir  : $ADDONS_DIR_ARG"
info "profile    : $PROFILE  (port $PORT, max $MAX_PLAYERS players)"
if (( TIMEOUT_S >= 86400 )); then
    info "stop with Ctrl-C"
else
    info "timeout    : ${TIMEOUT_S}s (exit 124 = the server stayed up, which is success)"
fi

T0="$(date +%s)"
start_ready_watcher "$T0"

RC=0
other=""
ovt_run_server "$TIMEOUT_S" "$SERVER_EXE" "${ARGS[@]}" >"$DIAG_FILE" 2>&1 || RC=$?
stop_ready_watcher

LOG_DIR="$(ovt_resolve_log_dir "$PROFILE" "$T0")" || LOG_DIR=""

case "$RC" in
    124)
        info "server ran for the full ${TIMEOUT_S}s window and was stopped"
        ;;
    130|143)
        info "interrupted — server stopped"
        ;;
    0)
        # A server that exits on its own has failed. The overwhelmingly most
        # common cause is a port clash with another server (verified
        # 2026-08-06 against a second server already holding 2001), and the
        # engine's signature for it is unhelpful unless you know it: the
        # listener line prints first, THEN replication fails to start.
        if [[ -n "$LOG_DIR" ]] && grep -qF 'Unable to start replication' "$LOG_DIR/console.log" 2>/dev/null; then
            ovt_err "the server could not start replication — port $PORT is almost certainly already in use by another server. Stop it, or (dedicated mode only) pass --port with a free port."
            other="$(ovt_pids_of ArmaReforgerServerDiag.exe 2>/dev/null; ovt_pids_of ArmaReforgerServer.exe 2>/dev/null)" || other=""
            other="$(printf '%s' "$other" | tr '\n' ' ')"
            if [[ -n "${other// /}" ]]; then
                ovt_err "server processes currently running: ${other% }"
            fi
        else
            ovt_warn "the server exited on its own after $(( $(date +%s) - T0 ))s — for a server this usually means it failed to start. Check ${LOG_DIR:-the profile log directory}."
        fi
        ;;
esac

if [[ -z "$LOG_DIR" ]]; then
    ovt_err "could not resolve the run's log directory under profile '$PROFILE' — cannot report where the server logged. Interop stub output: $DIAG_FILE"
    exit 2
fi

printf 'MODE=%s\n' "$MODE"
printf 'SCENARIO=%s\n' "$SCENARIO"
printf 'PROFILE=%s\n' "$PROFILE"
printf 'BIND_PORT=%s\n' "$PORT"
printf 'LOG_DIR=%s\n' "$LOG_DIR"
printf 'EXIT_CODE=%s\n' "$RC"

exit "$RC"
