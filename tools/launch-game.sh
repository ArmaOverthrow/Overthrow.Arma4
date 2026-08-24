#!/bin/bash
#
# tools/launch-game.sh — launch the Arma Reforger game client with Overthrow
# loaded, pass arbitrary caller arguments through unmangled, and report the
# run's log directory in a machine-readable stdout contract.
#
# This script knows NOTHING about tests. Feature #2 (autotest-foundation)
# builds on it by passing e.g. `-- -autotest "{GUID}"` and collecting
# $LOG_DIR/junit.xml afterwards. That is the whole integration surface.
#
# Empirical ground truth: docs/features/dev-ops/workbench-automation/findings.md
#   - The client's exit code is ALWAYS 0, even on fatal errors (finding 1.14b).
#   - The client must run with cwd = game install dir (ovt_run_game does this;
#     finding 1.14a).
#   - -addonsDir is ONE comma-separated value; repeating the flag = last wins
#     (finding 1.8e, confirmed for -addonsDir; assumed for other flags).
#
# stdout carries ONLY the KEY=value contract lines; all diagnostics go to
# stderr.

set -euo pipefail

_LG_SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
source "$_LG_SCRIPT_DIR/lib/common.sh"

OUT_DIR="$OVT_REPO_ROOT/.tmp/launch-game"
DIAG_FILE="$OUT_DIR/last-launch.txt"

usage() {
    cat <<EOF
usage: tools/launch-game.sh [--timeout <s>] [--profile <name>] [--quiet]
                            [--allow-concurrent] [-h|--help] [-- <client args...>]

Launches the Arma Reforger game client (cwd set to the game install dir, as
the client requires) with a default argument set that loads Overthrow from
source plus the packed EPF/EDF workshop dependencies, waits for the client to
exit, then resolves and reports the run's log directory.

Everything after '--' is passed to the client verbatim, quoting preserved
(arguments containing spaces survive intact).

Default client argument set (each individually overridable — see merge rule):

    -gproj <base game ArmaReforger.gproj>      (the client takes the BASE GAME
                                                project; Overthrow is loaded
                                                via -addonsDir/-addons)
    -addonsDir <addon farm>                    (a junction to THIS worktree
                                                and nothing else - see
                                                ovt_addon_farm(); Overthrow has
                                                no workshop dependencies)
    -addons Overthrow
    -profile <name>                            (default OverthrowCI)
    -noFocus -noThrow -window -logLevel debug  (BI's own autotest defaults)

Merge rule: if the pass-through arguments contain a flag with the same name
as a default (case-insensitive: -gproj, -addonsDir, -addons, -profile,
-noFocus, -noThrow, -window, -logLevel), the corresponding default is DROPPED
and only the caller's flag is sent — no duplicates. Pass-through arguments
are appended after the remaining defaults, so even for an unanticipated
duplicate the client's observed last-wins behaviour favours the caller
(verified for -addonsDir, assumed for other flags).

If the pass-through contains -profile, that profile (the last one, if
repeated) is also the one used for log-directory resolution and PROFILE_DIR.
Absolute-path -profile values are rejected (exit 2): log resolution supports
only My Games-relative profile names (nested names like A/b are fine).

stdout contract (the ONLY stdout; safe to source or grep), printed after a
completed run whose log directory was resolved:

    PROFILE_DIR=<wsl path>      profile root the run wrote to
    LOG_DIR=<wsl path>          this run's logs_<timestamp> directory
    LOG_DIR_WIN=<windows path>  same directory in Windows form
    EXIT_CODE=<n>               raw client exit code — see caveat below
    DURATION_S=<seconds>        wall time of the client run

*** CAVEAT: the game client's exit code is ALWAYS 0 — even on fatal errors
*** such as unresolvable addons or script compile failures (verified
*** empirically, findings 1.14). EXIT_CODE is reported for completeness only.
*** Read the run's outcome from artifacts under LOG_DIR (console.log,
*** junit.xml, crash.log), never from EXIT_CODE.

Options:
  --timeout <s>     Kill the client after <s> seconds. Default:
                    \$OVERTHROW_GAME_TIMEOUT, else 600 (client runs are long;
                    autotest world loads can take minutes).
  --profile <name>  Profile for the default -profile flag and for log
                    resolution. Default: \$OVERTHROW_PROFILE_NAME, else
                    OverthrowCI. A -profile in the pass-through overrides
                    this too.
  --quiet           Silence info-level diagnostics on stderr (warnings and
                    errors are always printed).
  --allow-concurrent  Suppress the warning when another game-client process
                    is already running (warn-only either way — the launcher
                    never refuses, and only ever kills processes it started
                    itself).
  -h, --help        This help.

Exit codes:
  0    The client ran to completion AND the run's log directory was resolved.
       (This says nothing about what happened inside the client — see the
       EXIT_CODE caveat above.)
  2    Tool/environment failure: missing binary/gproj/addons dir, bad
       arguments, or the run's log directory could not be resolved after the
       run (a launcher that reports the wrong log dir is worse than one that
       admits failure). Also used when a timeout kill could not be verified.
  124  Timed out (GNU timeout convention). The Windows client process is
       killed by PID and the kill verified. No stdout contract is printed —
       the run did not complete.
  130/143  Interrupted (SIGINT/SIGTERM). The launched client process is
       killed (verified) before the script exits — Ctrl-C leaves no orphan.

Environment (see tools/lib/common.sh for full resolution rules):
  OVERTHROW_GAME_TIMEOUT      Default --timeout in seconds (default 600).
  OVERTHROW_GAME_ADDONS_DIRS  Comma-separated WINDOWS-form -addonsDir value.
                              Default: <repo parent> (source Overthrow) +
                              <My Games>\\ArmaReforgerWorkbench\\addons
                              (packed EPF/EDF workshop deps).
  OVERTHROW_GAME_ADDONS       -addons value. Default:
                              Overthrow
  OVERTHROW_PROFILE_NAME      Default profile name (default OverthrowCI).
  OVERTHROW_GAME_EXE          WSL path of ArmaReforgerSteamDiag.exe.
  OVERTHROW_GAME_GPROJ        Base game ArmaReforger.gproj (default:
                              <game install>/addons/data/ArmaReforger.gproj).
  OVERTHROW_MYGAMES_DIR       WSL path of the 'My Games' folder (else
                              discovered; Documents may be OneDrive-redirected).

Notes:
  * A brief game window appears for the duration of the run — that is what
    -window -noFocus does; there is no headless mode.
  * The interop stub's own stdout/stderr noise is captured to
    .tmp/launch-game/last-launch.txt for post-mortems.
  * Smoke test (shipped empty autotest group; self-exits in ~7s):
        tools/launch-game.sh -- -autotest "{6AB9C8EEE9A651B5}"
    then check \$LOG_DIR/junit.xml exists.
  * Every launched Windows PID is registered in .tmp/ovt-pids/ while it runs
    (removed on verified exit/kill). Orphans from an uncleanly-killed wrapper
    can be reaped with: bash tools/lib/common.sh --sweep-stale [--kill]
    (strictly registry-scoped — never touches a client you launched yourself).
EOF
}

# --- argument parsing ---------------------------------------------------------

TIMEOUT_S="${OVERTHROW_GAME_TIMEOUT:-600}"
PROFILE="$(ovt_profile_name)"
QUIET=0
ALLOW_CONCURRENT=0
PASS_ARGS=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        --timeout)
            if [[ $# -lt 2 || ! "${2:-}" =~ ^[0-9]+$ || "${2}" -lt 1 ]]; then
                ovt_err "--timeout requires a positive integer number of seconds"
                exit 2
            fi
            TIMEOUT_S="$2"
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
        --quiet) QUIET=1; shift ;;
        --allow-concurrent) ALLOW_CONCURRENT=1; shift ;;
        -h|--help) usage; exit 0 ;;
        --)
            shift
            PASS_ARGS=("$@")
            break
            ;;
        *)
            ovt_err "unknown argument '$1' — client arguments go after '--' (see --help)"
            exit 2
            ;;
    esac
done

if [[ ! "$TIMEOUT_S" =~ ^[0-9]+$ ]] || (( TIMEOUT_S < 1 )); then
    ovt_err "OVERTHROW_GAME_TIMEOUT must be a positive integer, got '$TIMEOUT_S'"
    exit 2
fi

info() { if (( ! QUIET )); then ovt_info "$@"; fi; }

# --- helpers -------------------------------------------------------------------

# passthrough_has_flag <-flagname> — case-insensitive exact-token match in the
# pass-through args. Used to drop a default the caller replaces (merge rule).
passthrough_has_flag() {
    local want="${1,,}" a
    for a in ${PASS_ARGS[@]+"${PASS_ARGS[@]}"}; do
        if [[ "${a,,}" == "$want" ]]; then
            return 0
        fi
    done
    return 1
}

# game_addons_dir_arg — the client's default -addonsDir VALUE (Windows form):
# a farm holding a junction to THIS worktree and nothing else. See
# ovt_addon_farm() for why the repo parent is the wrong answer.
#
# ⚠ THE WORKSHOP ADDONS DIR IS NO LONGER PART OF THIS. Overthrow's addon.gproj
# declares exactly one dependency - 58D0FB3206B6F859, the base game - so the
# packed EPF/EDF workshop copies are not needed and including that directory
# only re-exposes a rival 'Overthrow' for '-addons' to pick (author, 2026-08-24:
# "overthrow has zero dependancies now"). The hard error this used to raise when
# that directory was absent would now fail a clean checkout for no reason.
# Overridable wholesale via OVERTHROW_GAME_ADDONS_DIRS.
game_addons_dir_arg() {
    if [[ -n "${OVERTHROW_GAME_ADDONS_DIRS:-}" ]]; then
        printf '%s\n' "$OVERTHROW_GAME_ADDONS_DIRS"
        return 0
    fi
    ovt_addon_farm
}


# --- pass-through sanity: OUR flags do not belong after '--' -------------------
# Everything after '--' goes to the client verbatim, so a launcher flag typed
# there is silently ignored - the client shrugs at an argument it does not know
# and the script keeps its default. Observed 2026-08-24: '-- -client ... --timeout
# 60000' ran with the default 600 s and the client was killed mid-session, which
# reads as "the timeout flag does not work".
for _arg in "${PASS_ARGS[@]}"; do
    case "$_arg" in
        --timeout|--profile|--quiet|--allow-concurrent|--scenario|--mode|--port|--max-players|--config|--admin-password)
            ovt_err "'$_arg' is a launcher flag but appears AFTER '--', so it was passed to the client and ignored by this script. Put launcher flags BEFORE '--'."
            exit 2
            ;;
    esac
done

# --- preflight (exit 2 with a specific message on any failure) ------------------

ovt_assert_unpacked || exit 2

mkdir -p "$OUT_DIR" || { ovt_err "cannot create '$OUT_DIR'"; exit 2; }

GAME_EXE="$(ovt_game_exe)" || exit 2
IMAGE="$(basename "$GAME_EXE")"
GAME_GPROJ="$(ovt_game_gproj)" || exit 2
GAME_GPROJ_WIN="$(ovt_win_path "$GAME_GPROJ")" || exit 2
ADDONS_DIR_ARG="$(game_addons_dir_arg)" || exit 2
ADDONS_LIST="${OVERTHROW_GAME_ADDONS:-Overthrow}"
ovt_mygames_dir >/dev/null || exit 2

# --- merge defaults with pass-through (drop a default the caller replaces) ------

MERGED=()
passthrough_has_flag -gproj     || MERGED+=( -gproj "$GAME_GPROJ_WIN" )
passthrough_has_flag -addonsDir || MERGED+=( -addonsDir "$ADDONS_DIR_ARG" )
passthrough_has_flag -addons    || MERGED+=( -addons "$ADDONS_LIST" )
passthrough_has_flag -profile   || MERGED+=( -profile "$PROFILE" )
passthrough_has_flag -noFocus   || MERGED+=( -noFocus )
passthrough_has_flag -noThrow   || MERGED+=( -noThrow )
passthrough_has_flag -window    || MERGED+=( -window )
passthrough_has_flag -logLevel  || MERGED+=( -logLevel debug )
MERGED+=( ${PASS_ARGS[@]+"${PASS_ARGS[@]}"} )

# The profile that actually receives the logs: a -profile in the pass-through
# supersedes --profile / the default (and last-wins if the caller repeats it).
EFFECTIVE_PROFILE="$PROFILE"
_i=0
while (( _i < ${#PASS_ARGS[@]} )); do
    if [[ "${PASS_ARGS[$_i],,}" == "-profile" ]] && (( _i + 1 < ${#PASS_ARGS[@]} )); then
        EFFECTIVE_PROFILE="${PASS_ARGS[$(( _i + 1 ))]}"
    fi
    _i=$(( _i + 1 ))
done
if [[ "$EFFECTIVE_PROFILE" =~ ^[A-Za-z]:[\\/] || "$EFFECTIVE_PROFILE" == /* ]]; then
    ovt_err "absolute-path -profile values ('$EFFECTIVE_PROFILE') are not supported: log resolution only handles My Games-relative profile names. Use a relative name (nested like Overthrow/ci is fine)."
    exit 2
fi

PROFILE_DIR="$(ovt_profile_dir "$EFFECTIVE_PROFILE")" || exit 2

# Concurrency: warn only — the launcher never refuses, and it only ever kills
# processes it started itself (equivalent of compile-check's Workbench guard).
if RUNNING_PIDS="$(ovt_pids_of "$IMAGE" 2>/dev/null)"; then
    if (( ALLOW_CONCURRENT )); then
        info "another $IMAGE is running (PID(s): ${RUNNING_PIDS//$'\n'/ }) — --allow-concurrent given"
    else
        ovt_warn "another $IMAGE is already running (PID(s): ${RUNNING_PIDS//$'\n'/ }). Proceeding — this launcher only ever kills processes it started itself, but two clients sharing a profile can contend for saves/logs. Pass --allow-concurrent to silence this warning."
    fi
fi

info "game exe:  $GAME_EXE"
info "profile:   $EFFECTIVE_PROFILE"
info "timeout:   ${TIMEOUT_S}s"
info "client args: ${MERGED[*]}"

# --- launch ----------------------------------------------------------------------

T0="$(date +%s)"
RC=0
# The interop stub prints Steam noise on stdout/stderr — keep stdout clean
# for the KEY=value contract, but keep the noise for post-mortems.
ovt_run_game "$TIMEOUT_S" "${MERGED[@]}" >"$DIAG_FILE" 2>&1 && RC=0 || RC=$?
DUR="${OVT_LAST_DURATION_S:-0}"

# --- timeout / tool-failure handling -----------------------------------------------

if (( RC == 124 )); then
    if [[ "${OVT_LAST_KILL_FAILED:-0}" == "1" ]]; then
        ORPHAN_PIDS="${OVT_LAST_WIN_PID:-unknown}"
        ovt_err "TIMEOUT after ${TIMEOUT_S}s AND the kill could NOT be verified — an ORPHANED $IMAGE process may still be running (Windows PID(s): $ORPHAN_PIDS). Kill it manually: taskkill.exe /F /T /PID ${ORPHAN_PIDS%% *} — or run: bash tools/lib/common.sh --sweep-stale --kill (registry-scoped, cannot touch a client you launched yourself). Verify with: tasklist.exe /FI \"IMAGENAME eq $IMAGE\"."
        exit 2
    fi
    if LOG_DIR="$(ovt_resolve_log_dir "$EFFECTIVE_PROFILE" "$T0")" 2>/dev/null; then
        ovt_warn "timed out after ${TIMEOUT_S}s (client killed and verified dead); partial logs: $LOG_DIR"
    else
        ovt_warn "timed out after ${TIMEOUT_S}s (client killed and verified dead); no log directory found for this run"
    fi
    exit 124
fi
if (( RC == 2 )); then
    ovt_err "tool failure launching the game client (see $DIAG_FILE)"
    exit 2
fi

info "client exited $RC after ${DUR}s (NOTE: client exit codes are meaningless — always 0, even on fatal errors; read $DIAG_FILE / the logs)"

# --- resolve this run's log directory ------------------------------------------------

LOG_DIR=""
if ! LOG_DIR="$(ovt_resolve_log_dir "$EFFECTIVE_PROFILE" "$T0")" || [[ -z "$LOG_DIR" ]]; then
    ovt_err "client exited $RC but this run's log directory could not be located under '$PROFILE_DIR/logs' (newest-since-launch). Reporting a wrong log dir would be worse than admitting failure — check the profile name reached the client (diag: $DIAG_FILE)."
    exit 2
fi
if [[ ! -f "$LOG_DIR/console.log" ]]; then
    ovt_err "resolved log dir '$LOG_DIR' contains no console.log — cannot attribute this run's output to it (diag: $DIAG_FILE)."
    exit 2
fi
LOG_DIR_WIN="$(ovt_win_path "$LOG_DIR")" || exit 2

# --- stdout contract (the ONLY stdout this script ever produces) ----------------------

printf 'PROFILE_DIR=%s\n' "$PROFILE_DIR"
printf 'LOG_DIR=%s\n'     "$LOG_DIR"
printf 'LOG_DIR_WIN=%s\n' "$LOG_DIR_WIN"
printf 'EXIT_CODE=%s\n'   "$RC"
printf 'DURATION_S=%s\n'  "$DUR"
exit 0
