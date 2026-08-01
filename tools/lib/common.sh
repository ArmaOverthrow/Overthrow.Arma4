#!/bin/bash
#
# tools/lib/common.sh — shared library for Overthrow's Workbench/game automation.
#
# Owns the WSL -> Windows process boundary: path translation, binary/project
# resolution, launching with a real timeout, process kill/verify, and profile
# log-directory resolution. Every entry-point script (compile-check.sh,
# launch-game.sh, release tooling) sources this file and never names an .exe
# path or calls wslpath directly.
#
# Usage:
#   source tools/lib/common.sh          # from any cwd; side-effect-free
#   bash tools/lib/common.sh --self-test
#   bash tools/lib/common.sh --sweep-stale [--kill]   # reap registered orphans
#
# Conventions:
#   - Functions are prefixed ovt_; internal helpers _ovt_.
#   - Env overrides are prefixed OVERTHROW_ (see resolver functions below).
#   - Diagnostics go to stderr (ovt_info/ovt_warn/ovt_err); stdout is reserved
#     for machine-readable results.
#   - When SOURCED this file does NOT touch shell options (no set -e etc).
#     When EXECUTED (--self-test) it runs under set -euo pipefail.
#
# Exit/return code contract:
#   0    success
#   1    recoverable "not found / no" answers (e.g. untranslatable path,
#        no matching log dir, image not running)
#   2    tool/environment failure: missing binary, unresolvable dir,
#        failed cleanup — always with a specific message on stderr
#   124  ovt_run_win timeout (GNU timeout convention); Windows process killed
#   other: the launched Windows process's real exit code, passed through
#        (Workbench -wbsilent -validate: 0 = compile pass, 255 = compile fail)
#   On SIGINT/SIGTERM during ovt_run_win the Windows process tree is killed
#   (verified), the pidfile registry cleaned, the caller's traps restored,
#   and the signal re-raised — the shell exits 130/143 conventionally.
#
# Empirical ground truth for everything here:
#   docs/features/dev-ops/workbench-automation/findings.md
# In particular:
#   - Workbench does NOT detach; $? is the real exit code (finding 1.3).
#   - bash `timeout` kills only the WSL interop stub; the Windows process
#     survives and must be killed by PID via taskkill.exe (findings 1.10a/b).
#   - The game client's exit code is ALWAYS 0, even on fatal errors (1.14).
#   - The game client requires cwd = game install dir (finds core via
#     ./addons) (finding 1.14a).
#   - -addonsDir is ONE comma-separated flag, last one wins (finding 1.8e).

# ---------------------------------------------------------------------------
# Location + per-machine config
# ---------------------------------------------------------------------------

# Resolve our own location so the repo root is known from any cwd.
OVT_LIB_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
OVT_REPO_ROOT="$(cd -- "$OVT_LIB_DIR/../.." && pwd -P)"

# Per-process caches / last-run info (globals, not exported).
OVT_CACHED_MYGAMES_DIR="${OVT_CACHED_MYGAMES_DIR:-}"
OVT_LAST_DURATION_S="${OVT_LAST_DURATION_S:-}"
OVT_LAST_WIN_PID="${OVT_LAST_WIN_PID:-}"
OVT_LAST_KILL_FAILED="${OVT_LAST_KILL_FAILED:-0}"

# Pidfile registry (task 5.2): every Windows PID that ovt_run_win launches is
# recorded as .tmp/ovt-pids/<pid>.pid (line 1: image name, line 2: launch
# epoch); the record is removed only once the process is VERIFIED dead.
# ovt_sweep_stale reaps orphans strictly from this registry — it can never
# touch a process these tools did not launch (e.g. the developer's own open
# Workbench, which was never registered).
OVT_PID_REGISTRY="${OVT_PID_REGISTRY:-$OVT_REPO_ROOT/.tmp/ovt-pids}"

# ovt_run_win in-flight state — globals so the INT/TERM signal handler can
# see them (task 5.3). Cleared by _ovt_run_clear_state on every exit path.
_OVT_RUN_STUB_PID=""
_OVT_RUN_WIN_PIDS=""
_OVT_RUN_IMAGE=""
_OVT_RUN_BEFORE_PIDS=""
_OVT_RUN_SAVED_TRAP_INT=""
_OVT_RUN_SAVED_TRAP_TERM=""
_OVT_RUN_TRAPS_INSTALLED=0

# Optional, gitignored per-machine overrides (OVERTHROW_* assignments only).
if [[ -f "$OVT_REPO_ROOT/tools/config.local.sh" ]]; then
    # shellcheck source=/dev/null
    source "$OVT_REPO_ROOT/tools/config.local.sh"
fi

# ---------------------------------------------------------------------------
# 2.8 Output helpers — stderr ONLY; stdout belongs to machine-readable output
# ---------------------------------------------------------------------------

ovt_info() { printf 'ovt: %s\n' "$*" >&2; }
ovt_warn() { printf 'ovt: WARNING: %s\n' "$*" >&2; }
ovt_err()  { printf 'ovt: ERROR: %s\n' "$*" >&2; }

# ---------------------------------------------------------------------------
# 2.2 Path translation
# ---------------------------------------------------------------------------

# ovt_win_path <wsl_path>
# Print the Windows form of a WSL path. Handles spaces, trailing slashes,
# relative paths and non-existent paths.
#
# IMPORTANT: `wslpath -w` never fails — it silently emits a
# \\wsl.localhost\... UNC path for anything it cannot map to a drive
# (including /mnt/z/... when Z: is not mounted), which is exactly the
# silently-wrong output this function must never produce. Therefore:
#   - /mnt/<letter>/... is transformed to <LETTER>:\... directly (exact match
#     for what wslpath produces on mounted drives, and correct even when the
#     drive is not mounted in WSL or the path does not exist yet).
#   - other paths that EXIST are given to wslpath (legitimate UNC results for
#     files living in the WSL filesystem).
#   - other paths that do NOT exist are refused: return 1, nothing on stdout.
ovt_win_path() {
    local in="${1:-}"
    if [[ -z "$in" ]]; then
        ovt_err "ovt_win_path: no path given"
        return 1
    fi
    local p
    # Absolutise, collapse '..' and trailing slashes; does not require existence.
    if ! p="$(realpath -m -- "$in" 2>/dev/null)" || [[ -z "$p" ]]; then
        ovt_err "ovt_win_path: cannot normalise '$in'"
        return 1
    fi

    if [[ "$p" =~ ^/mnt/([a-zA-Z])(/.*)?$ ]]; then
        local drive="${BASH_REMATCH[1]}" rest="${BASH_REMATCH[2]:-}"
        drive="${drive^^}"
        rest="${rest//\//\\}"
        if [[ -z "$rest" ]]; then
            printf '%s\n' "${drive}:\\"
        else
            printf '%s\n' "${drive}:${rest}"
        fi
        return 0
    fi

    # Not a drvfs path: only translate if it exists (UNC \\wsl.localhost form).
    if [[ -e "$p" ]]; then
        local out
        if out="$(wslpath -w "$p" 2>/dev/null)" && [[ -n "$out" ]]; then
            printf '%s\n' "$out"
            return 0
        fi
    fi
    ovt_err "ovt_win_path: cannot translate '$in' to a Windows path (not under /mnt/<drive> and does not exist in the WSL filesystem)"
    return 1
}

# ovt_wsl_path <windows_path>
# Print the WSL form of a Windows path. Accepts X:\..., X:/... and
# \\wsl.localhost\... UNC forms; does not require the path to exist.
ovt_wsl_path() {
    local in="${1:-}"
    if [[ -z "$in" ]]; then
        ovt_err "ovt_wsl_path: no path given"
        return 1
    fi

    if [[ "$in" =~ ^([A-Za-z]):([\\/].*)?$ ]]; then
        local drive="${BASH_REMATCH[1]}" rest="${BASH_REMATCH[2]:-}"
        drive="${drive,,}"
        rest="${rest//\\//}"
        local p="/mnt/${drive}${rest}"
        # strip trailing slashes (but keep the drive root itself)
        while [[ "$p" == */ && "$p" != "/mnt/${drive}/" ]]; do p="${p%/}"; done
        [[ "$p" == "/mnt/${drive}/" ]] && p="/mnt/${drive}"
        printf '%s\n' "$p"
        return 0
    fi

    if [[ "$in" == \\\\* ]]; then
        local out
        if out="$(wslpath -u "$in" 2>/dev/null)" && [[ -n "$out" ]]; then
            printf '%s\n' "$out"
            return 0
        fi
    fi
    ovt_err "ovt_wsl_path: cannot translate '$in' to a WSL path (expected X:\\... or \\\\wsl UNC form)"
    return 1
}

# ---------------------------------------------------------------------------
# 2.3 Resolution + validation
# Each resolver honours its OVERTHROW_* override, falls back to the documented
# default, and returns 2 with a specific actionable message if missing.
# ---------------------------------------------------------------------------

ovt_workbench_exe() {
    local exe="${OVERTHROW_WORKBENCH_EXE:-/mnt/n/Program Files (x86)/Steam/steamapps/common/Arma Reforger Tools/Workbench/ArmaReforgerWorkbenchSteamDiag.exe}"
    if [[ ! -f "$exe" ]]; then
        ovt_err "Workbench executable not found: '$exe'. Install Arma Reforger Tools via Steam, or set OVERTHROW_WORKBENCH_EXE to the WSL path of ArmaReforgerWorkbenchSteamDiag.exe."
        return 2
    fi
    printf '%s\n' "$exe"
}

ovt_game_exe() {
    local exe="${OVERTHROW_GAME_EXE:-/mnt/n/Program Files (x86)/Steam/steamapps/common/Arma Reforger/ArmaReforgerSteamDiag.exe}"
    if [[ ! -f "$exe" ]]; then
        ovt_err "Game executable not found: '$exe'. Install Arma Reforger via Steam, or set OVERTHROW_GAME_EXE to the WSL path of ArmaReforgerSteamDiag.exe."
        return 2
    fi
    printf '%s\n' "$exe"
}

# The Overthrow addon project (what the WORKBENCH takes as -gproj).
ovt_project_gproj() {
    local gproj="${OVERTHROW_GPROJ:-$OVT_REPO_ROOT/addon.gproj}"
    if [[ ! -f "$gproj" ]]; then
        ovt_err "Project gproj not found: '$gproj'. Expected the Overthrow addon.gproj at the repo root, or set OVERTHROW_GPROJ."
        return 2
    fi
    printf '%s\n' "$gproj"
}

# The BASE GAME project (what the GAME CLIENT takes as -gproj; Overthrow is
# loaded via -addonsDir/-addons — see findings 1.14).
ovt_game_gproj() {
    local gproj="${OVERTHROW_GAME_GPROJ:-}"
    if [[ -z "$gproj" ]]; then
        local exe
        exe="$(ovt_game_exe)" || return 2
        gproj="$(dirname "$exe")/addons/data/ArmaReforger.gproj"
    fi
    if [[ ! -f "$gproj" ]]; then
        ovt_err "Base game gproj not found: '$gproj'. Expected <game install>/addons/data/ArmaReforger.gproj, or set OVERTHROW_GAME_GPROJ."
        return 2
    fi
    printf '%s\n' "$gproj"
}

# Print the Windows user profile dir (C:\Users\<name>) via cmd.exe.
_ovt_win_userprofile() {
    if ! command -v cmd.exe >/dev/null 2>&1; then
        ovt_err "cmd.exe not on PATH — Windows interop appears disabled (check /etc/wsl.conf [interop] appendWindowsPath)."
        return 2
    fi
    local up
    # cd to a Windows-backed dir first so cmd.exe does not whine about UNC cwds.
    up="$( (cd /mnt/c 2>/dev/null || true; cmd.exe /c 'echo %USERPROFILE%' 2>/dev/null) | tr -d '\r' | tail -n 1 )"
    if [[ ! "$up" =~ ^[A-Za-z]: ]]; then
        ovt_err "Could not resolve %USERPROFILE% via cmd.exe (got '$up')."
        return 2
    fi
    printf '%s\n' "$up"
}

# ovt_mygames_dir — discover the 'My Games' root. Documents may be
# OneDrive-redirected (it is on this machine), so the path is DISCOVERED:
#   1. OVERTHROW_MYGAMES_DIR override (must exist)
#   2. <profile>/OneDrive/Documents/My Games
#   3. <profile>/Documents/My Games
# The discovery is cached in OVT_CACHED_MYGAMES_DIR for this process (note:
# a $(...) capture runs in a subshell, so the cache only primes the parent
# shell when the function is first called outside a command substitution;
# either way, correctness is unaffected — only the ~0.2s cmd.exe hop).
ovt_mygames_dir() {
    if [[ -n "${OVERTHROW_MYGAMES_DIR:-}" ]]; then
        if [[ ! -d "$OVERTHROW_MYGAMES_DIR" ]]; then
            ovt_err "OVERTHROW_MYGAMES_DIR is set but not a directory: '$OVERTHROW_MYGAMES_DIR'"
            return 2
        fi
        printf '%s\n' "$OVERTHROW_MYGAMES_DIR"
        return 0
    fi
    if [[ -n "$OVT_CACHED_MYGAMES_DIR" && -d "$OVT_CACHED_MYGAMES_DIR" ]]; then
        printf '%s\n' "$OVT_CACHED_MYGAMES_DIR"
        return 0
    fi
    local up_win up_wsl cand
    up_win="$(_ovt_win_userprofile)" || return 2
    up_wsl="$(ovt_wsl_path "$up_win")" || return 2
    for cand in "$up_wsl/OneDrive/Documents/My Games" "$up_wsl/Documents/My Games"; do
        if [[ -d "$cand" ]]; then
            OVT_CACHED_MYGAMES_DIR="$cand"
            printf '%s\n' "$cand"
            return 0
        fi
    done
    ovt_err "Could not find 'My Games' under '$up_wsl' (tried OneDrive/Documents/My Games and Documents/My Games). Set OVERTHROW_MYGAMES_DIR to the WSL path of your My Games folder."
    return 2
}

# ovt_profile_name — the pinned automation profile (default OverthrowCI).
ovt_profile_name() {
    printf '%s\n' "${OVERTHROW_PROFILE_NAME:-OverthrowCI}"
}

# ovt_profile_dir <name> — <My Games>/<name>. NOTE: intentionally does not
# require the directory to exist — the engine creates it on the profile's
# first run. Use ovt_resolve_log_dir to attribute a run's output.
ovt_profile_dir() {
    local name="${1:-}"
    if [[ -z "$name" ]]; then
        ovt_err "ovt_profile_dir: profile name required"
        return 2
    fi
    local mg
    mg="$(ovt_mygames_dir)" || return 2
    printf '%s/%s\n' "$mg" "$name"
}

# ovt_workbench_addons_dir_arg — build the canonical -addonsDir VALUE for
# Workbench runs, in Windows form:
#   <My Games>\ArmaReforgerWorkbench\addons,<game install>\addons
# (packed workshop EPF/EDF first, then base game addons — finding 1.8g).
# Overridable wholesale via OVERTHROW_ADDONS_DIRS (already-Windows-form,
# comma-separated). Both default directories must exist: a missing packed
# EPF/EDF dir triggers the FALSE-PASS TRAP (finding 1.8d) where Workbench
# silently validates the base game and exits 0 on a broken Overthrow tree.
ovt_workbench_addons_dir_arg() {
    if [[ -n "${OVERTHROW_ADDONS_DIRS:-}" ]]; then
        printf '%s\n' "$OVERTHROW_ADDONS_DIRS"
        return 0
    fi
    local mg exe game_dir wb_addons game_addons w1 w2
    mg="$(ovt_mygames_dir)" || return 2
    exe="$(ovt_game_exe)" || return 2
    game_dir="$(dirname "$exe")"
    wb_addons="$mg/ArmaReforgerWorkbench/addons"
    game_addons="$game_dir/addons"
    if [[ ! -d "$wb_addons" ]]; then
        ovt_err "Workbench workshop addons dir not found: '$wb_addons'. Packed EPF/EDF live there; without them Workbench silently validates the base game (false-pass trap). Download the dependencies in Workbench once, or set OVERTHROW_ADDONS_DIRS."
        return 2
    fi
    if [[ ! -d "$game_addons" ]]; then
        ovt_err "Base game addons dir not found: '$game_addons'. Check the Arma Reforger install, or set OVERTHROW_ADDONS_DIRS."
        return 2
    fi
    w1="$(ovt_win_path "$wb_addons")" || return 2
    w2="$(ovt_win_path "$game_addons")" || return 2
    printf '%s,%s\n' "$w1" "$w2"
}

# ---------------------------------------------------------------------------
# 2.5 Process control (tasklist.exe / taskkill.exe)
# ---------------------------------------------------------------------------

_ovt_require_interop() {
    if ! command -v tasklist.exe >/dev/null 2>&1 || ! command -v taskkill.exe >/dev/null 2>&1; then
        ovt_err "tasklist.exe/taskkill.exe not on PATH — Windows interop appears disabled (check /etc/wsl.conf [interop] appendWindowsPath)."
        return 2
    fi
}

# ovt_pids_of <image.exe> — Windows PIDs of all processes with that image
# name, one per line. Returns 1 (empty output) when none are running.
ovt_pids_of() {
    local image="${1:-}"
    if [[ -z "$image" ]]; then
        ovt_err "ovt_pids_of: image name required (e.g. ArmaReforgerWorkbenchSteamDiag.exe)"
        return 2
    fi
    _ovt_require_interop || return 2
    local out
    out="$(tasklist.exe /FI "IMAGENAME eq $image" /FO CSV /NH 2>/dev/null | tr -d '\r' | awk -F'","' '/^"/ {print $2}')" || true
    if [[ -z "$out" ]]; then
        return 1
    fi
    printf '%s\n' "$out"
}

# ovt_is_running <image.exe> — 0 if at least one such process exists.
ovt_is_running() {
    ovt_pids_of "$1" >/dev/null
}

# _ovt_win_pid_alive <pid> — 0 while the Windows PID exists.
_ovt_win_pid_alive() {
    tasklist.exe /FI "PID eq $1" /FO CSV /NH 2>/dev/null | tr -d '\r' | grep -q '^"'
}

# ovt_kill_tree <win_pid> — taskkill /F /T the PID, then VERIFY it is gone
# (finding 1.10b). Returns 2 if the process is still alive afterwards.
# Never kills by image name: that could hit the user's own GUI session.
ovt_kill_tree() {
    local pid="${1:-}"
    if [[ ! "$pid" =~ ^[0-9]+$ ]]; then
        ovt_err "ovt_kill_tree: numeric Windows PID required, got '$pid'"
        return 2
    fi
    _ovt_require_interop || return 2
    taskkill.exe /F /T /PID "$pid" >/dev/null 2>&1 || true
    local i
    for i in 1 2 3 4 5 6 7 8 9 10; do
        if ! _ovt_win_pid_alive "$pid"; then
            return 0
        fi
        sleep 0.5
    done
    ovt_err "ovt_kill_tree: Windows PID $pid still alive after taskkill /F /T (tried for ~5s). Kill it manually: taskkill.exe /F /T /PID $pid"
    return 2
}

# _ovt_list_diff <before> <after> — lines present in <after> but not <before>.
_ovt_list_diff() {
    comm -13 <(printf '%s\n' "${1:-}" | sort -u) <(printf '%s\n' "${2:-}" | sort -u) | sed '/^$/d'
}

# ---------------------------------------------------------------------------
# 5.2 Pidfile registry + stale-process sweep
# ---------------------------------------------------------------------------

_ovt_pidfile_write() {   # <win_pid> <image>
    mkdir -p "$OVT_PID_REGISTRY" 2>/dev/null || return 0
    printf '%s\n%s\n' "$2" "$(date +%s)" > "$OVT_PID_REGISTRY/$1.pid" 2>/dev/null || true
}

_ovt_pidfile_remove() {  # <win_pid>
    rm -f "$OVT_PID_REGISTRY/$1.pid" 2>/dev/null || true
}

# _ovt_win_pid_matches_image <pid> <image> — 0 iff the Windows PID is alive
# AND its image name matches <image>. The image match guards against Windows
# PID recycling: a recycled PID belongs to an unrelated process and must
# never be killed.
_ovt_win_pid_matches_image() {
    local pid="$1" image="$2" row img
    [[ -n "$image" ]] || return 1
    row="$(tasklist.exe /FI "PID eq $pid" /FO CSV /NH 2>/dev/null | tr -d '\r' | grep '^"' | head -n 1)" || return 1
    [[ -n "$row" ]] || return 1
    img="${row#\"}"
    img="${img%%\"*}"
    [[ "${img,,}" == "${image,,}" ]]
}

# ovt_sweep_stale [--kill]
# Sweep the pidfile registry for orphaned Windows processes left behind by an
# uncleanly-killed wrapper (SIGKILL bypasses the traps; a failed timeout-kill
# also keeps its pidfile so the sweep can retry).
#   default: LIST stale entries on stdout, one per line: STALE\t<pid>\t<image>
#   --kill:  taskkill /F /T each stale entry, VERIFY it died, then remove
#            its pidfile. A record whose PID is dead — or recycled by a
#            different image — is cleaned up without killing anything.
# STRICTLY registry-scoped: only PIDs recorded by ovt_run_win are considered.
# A developer's own Workbench/game session was never registered and can never
# be touched. Feature #4 (ci-pipeline) calls '--kill' before each run.
# Returns 0 on success (including "nothing stale"); 2 if a kill could not be
# verified (the pidfile is kept) or on usage/interop failure.
ovt_sweep_stale() {
    local do_kill=0
    if (( $# > 1 )); then
        ovt_err "ovt_sweep_stale: too many arguments (usage: ovt_sweep_stale [--kill])"
        return 2
    fi
    case "${1:-}" in
        "")     ;;
        --kill) do_kill=1 ;;
        *)
            ovt_err "ovt_sweep_stale: unknown argument '$1' (usage: ovt_sweep_stale [--kill])"
            return 2
            ;;
    esac
    _ovt_require_interop || return 2
    if [[ ! -d "$OVT_PID_REGISTRY" ]]; then
        ovt_info "sweep: no pidfile registry at '$OVT_PID_REGISTRY' — nothing to sweep"
        return 0
    fi
    local f pid image n_stale=0 n_cleaned=0 n_killfail=0
    for f in "$OVT_PID_REGISTRY"/*.pid; do
        [[ -e "$f" ]] || continue
        pid="$(basename "$f" .pid)"
        image="$(head -n 1 "$f" 2>/dev/null | tr -d '\r')"
        if [[ ! "$pid" =~ ^[0-9]+$ ]]; then
            rm -f "$f"
            n_cleaned=$(( n_cleaned + 1 ))
            continue
        fi
        if _ovt_win_pid_matches_image "$pid" "$image"; then
            n_stale=$(( n_stale + 1 ))
            if (( do_kill )); then
                if ovt_kill_tree "$pid"; then
                    _ovt_pidfile_remove "$pid"
                    ovt_info "sweep: killed stale $image (Windows PID $pid), verified dead"
                else
                    n_killfail=$(( n_killfail + 1 ))
                    ovt_err "sweep: could not verify the death of $image PID $pid — pidfile kept for a later retry"
                fi
            else
                printf 'STALE\t%s\t%s\n' "$pid" "$image"
            fi
        else
            # Dead, or the PID was recycled by an unrelated process — either
            # way the record is obsolete. NEVER kill on an image mismatch.
            rm -f "$f"
            n_cleaned=$(( n_cleaned + 1 ))
        fi
    done
    if (( n_stale == 0 )); then
        ovt_info "sweep: no stale processes (${n_cleaned} obsolete record(s) cleaned)"
    elif (( do_kill )); then
        ovt_info "sweep: ${n_stale} stale process(es) found, $(( n_stale - n_killfail )) killed, ${n_cleaned} obsolete record(s) cleaned"
    else
        ovt_info "sweep: ${n_stale} stale process(es) listed (re-run with --kill to reap), ${n_cleaned} obsolete record(s) cleaned"
    fi
    if (( n_killfail > 0 )); then
        return 2
    fi
    return 0
}

# ---------------------------------------------------------------------------
# 5.3 Signal handling around a run
# common.sh is SOURCED by callers, so traps are installed only for the
# duration of ovt_run_win, and the caller's own INT/TERM traps are saved
# beforehand and restored on every exit path — never globally clobbered.
# ---------------------------------------------------------------------------

_ovt_install_run_traps() {
    _OVT_RUN_SAVED_TRAP_INT="$(trap -p INT)"
    _OVT_RUN_SAVED_TRAP_TERM="$(trap -p TERM)"
    trap '_ovt_on_run_signal INT' INT
    trap '_ovt_on_run_signal TERM' TERM
    _OVT_RUN_TRAPS_INSTALLED=1
}

_ovt_restore_run_traps() {
    (( _OVT_RUN_TRAPS_INSTALLED )) || return 0
    if [[ -n "$_OVT_RUN_SAVED_TRAP_INT" ]]; then
        eval "$_OVT_RUN_SAVED_TRAP_INT"
    else
        trap - INT
    fi
    if [[ -n "$_OVT_RUN_SAVED_TRAP_TERM" ]]; then
        eval "$_OVT_RUN_SAVED_TRAP_TERM"
    else
        trap - TERM
    fi
    _OVT_RUN_TRAPS_INSTALLED=0
}

_ovt_run_clear_state() {
    _ovt_restore_run_traps
    _OVT_RUN_STUB_PID=""
    _OVT_RUN_WIN_PIDS=""
    _OVT_RUN_IMAGE=""
    _OVT_RUN_BEFORE_PIDS=""
}

# _ovt_on_run_signal <INT|TERM> — kill the Windows process tree (verified),
# clean the pidfile registry, reap the interop stub, restore the caller's
# traps, then re-raise the signal so the shell exits with the conventional
# code (130/143) — or the caller's own saved trap runs, if it had one.
_ovt_on_run_signal() {
    local sig="$1" p after
    ovt_warn "ovt_run_win: caught SIG$sig — killing the Windows process before exiting"
    if [[ -z "$_OVT_RUN_WIN_PIDS" && -n "$_OVT_RUN_IMAGE" ]]; then
        # Signal arrived before the PID was identified — try once more now.
        after="$(ovt_pids_of "$_OVT_RUN_IMAGE" 2>/dev/null)" || after=""
        _OVT_RUN_WIN_PIDS="$(_ovt_list_diff "$_OVT_RUN_BEFORE_PIDS" "$after")"
        _OVT_RUN_WIN_PIDS="${_OVT_RUN_WIN_PIDS//$'\n'/ }"
    fi
    for p in $_OVT_RUN_WIN_PIDS; do
        if ovt_kill_tree "$p"; then
            _ovt_pidfile_remove "$p"
        else
            ovt_err "ovt_run_win: SIG$sig cleanup could NOT verify the death of Windows PID $p — pidfile kept. Reap later with: bash tools/lib/common.sh --sweep-stale --kill  (or: taskkill.exe /F /T /PID $p)"
        fi
    done
    if [[ -n "$_OVT_RUN_STUB_PID" ]]; then
        kill "$_OVT_RUN_STUB_PID" 2>/dev/null || true
        wait "$_OVT_RUN_STUB_PID" 2>/dev/null || true
    fi
    _ovt_run_clear_state
    kill -s "$sig" $$ 2>/dev/null || true
    # If the re-raised signal does not terminate this shell (interactive
    # sourced use, or a caller trap that returns), execution resumes and
    # ovt_run_win unwinds on the now-dead stub.
}

# ---------------------------------------------------------------------------
# 2.4 ovt_run_win — THE single WSL -> Windows boundary crossing
# ---------------------------------------------------------------------------

# ovt_run_win <timeout_s> <exe_wsl_path> [args...]
#
# Launches the Windows exe, enforces a REAL timeout, and returns the process's
# real exit code. Why not `timeout <n> exe`? Because bash `timeout` kills only
# the WSL interop stub — the Windows process survives (finding 1.10a). So:
#
#   1. snapshot the PIDs of this image name (tasklist.exe)
#   2. launch in the background; the interop stub blocks for the process's
#      lifetime and its exit status is the real one (finding 1.3)
#   3. diff tasklist to identify OUR Windows PID (never touch pre-existing
#      processes — they may be the user's own session)
#   4. watchdog-wait; on deadline: ovt_kill_tree our PID(s), reap the stub,
#      return 124
#   5. normal exit: return the real exit code, set OVT_LAST_DURATION_S
#
# Globals set: OVT_LAST_DURATION_S (wall seconds), OVT_LAST_WIN_PID
# (space-separated PIDs identified, possibly empty for very fast runs),
# OVT_LAST_KILL_FAILED (1 if a timeout-kill could not be verified — the
# caller should surface this loudly; return code is still 124).
#
# Exit codes: 124 timeout; 2 usage/environment error; otherwise the process's
# own code (0-255; Workbench -wbsilent -validate uses 255 = compile failure,
# the documented -1 as seen through WSL interop).
ovt_run_win() {
    local timeout_s="${1:-}" exe="${2:-}"
    if [[ ! "$timeout_s" =~ ^[0-9]+$ ]] || (( timeout_s < 1 )); then
        ovt_err "ovt_run_win: first argument must be a positive integer timeout in seconds, got '$timeout_s'"
        return 2
    fi
    if [[ -z "$exe" || ! -f "$exe" ]]; then
        ovt_err "ovt_run_win: executable not found: '$exe'"
        return 2
    fi
    shift 2
    _ovt_require_interop || return 2

    local image before_pids after_pids new_pids
    image="$(basename "$exe")"
    before_pids="$(ovt_pids_of "$image" 2>/dev/null)" || before_pids=""

    local start deadline now
    start="$(date +%s)"
    deadline=$(( start + timeout_s ))
    OVT_LAST_DURATION_S=""
    OVT_LAST_WIN_PID=""
    OVT_LAST_KILL_FAILED=0

    # Signal-handler state + INT/TERM traps, installed for the duration of
    # this run only (the caller's own traps are saved and restored).
    _OVT_RUN_IMAGE="$image"
    _OVT_RUN_BEFORE_PIDS="$before_pids"
    _OVT_RUN_WIN_PIDS=""
    _ovt_install_run_traps

    "$exe" "$@" &
    local stub=$!
    _OVT_RUN_STUB_PID=$stub

    # Identify the Windows PID(s) we just created and record them in the
    # pidfile registry (.tmp/ovt-pids/<pid>.pid) so ovt_sweep_stale can find
    # them if this shell dies uncleanly. Fast runs may finish before we
    # catch one — that's fine.
    new_pids=""
    while kill -0 "$stub" 2>/dev/null; do
        after_pids="$(ovt_pids_of "$image" 2>/dev/null)" || after_pids=""
        new_pids="$(_ovt_list_diff "$before_pids" "$after_pids")"
        if [[ -n "$new_pids" ]]; then
            break
        fi
        now="$(date +%s)"
        if (( now >= deadline )); then
            break
        fi
        sleep 0.2
    done
    OVT_LAST_WIN_PID="${new_pids//$'\n'/ }"
    _OVT_RUN_WIN_PIDS="$OVT_LAST_WIN_PID"
    local p
    for p in $_OVT_RUN_WIN_PIDS; do
        _ovt_pidfile_write "$p" "$image"
    done

    # Watchdog.
    while kill -0 "$stub" 2>/dev/null; do
        now="$(date +%s)"
        if (( now >= deadline )); then
            ovt_warn "ovt_run_win: '$image' exceeded ${timeout_s}s — killing the Windows process"
            if [[ -z "$new_pids" ]]; then
                # Last resort: re-diff now.
                after_pids="$(ovt_pids_of "$image" 2>/dev/null)" || after_pids=""
                new_pids="$(_ovt_list_diff "$before_pids" "$after_pids")"
                OVT_LAST_WIN_PID="${new_pids//$'\n'/ }"
                _OVT_RUN_WIN_PIDS="$OVT_LAST_WIN_PID"
            fi
            if [[ -n "$new_pids" ]]; then
                while IFS= read -r p; do
                    [[ -n "$p" ]] || continue
                    if ovt_kill_tree "$p"; then
                        _ovt_pidfile_remove "$p"
                    else
                        # Keep the pidfile: ovt_sweep_stale can retry later.
                        OVT_LAST_KILL_FAILED=1
                    fi
                done <<< "$new_pids"
            else
                ovt_err "ovt_run_win: timed out but could not identify the Windows PID of '$image' — a process may survive; check: tasklist.exe /FI \"IMAGENAME eq $image\""
                OVT_LAST_KILL_FAILED=1
            fi
            kill "$stub" 2>/dev/null || true
            wait "$stub" 2>/dev/null || true
            OVT_LAST_DURATION_S=$(( $(date +%s) - start ))
            if (( OVT_LAST_KILL_FAILED )); then
                ovt_err "ovt_run_win: timeout cleanup NOT verified — an orphaned '$image' process may remain (PID(s): ${OVT_LAST_WIN_PID:-unknown})"
            fi
            _ovt_run_clear_state
            return 124
        fi
        sleep 0.25
    done

    local rc=0
    wait "$stub" && rc=0 || rc=$?
    OVT_LAST_DURATION_S=$(( $(date +%s) - start ))
    # Clean exit: the Workbench/client does not detach (finding 1.3), so the
    # Windows process is gone — drop its registry entries.
    for p in $_OVT_RUN_WIN_PIDS; do
        _ovt_pidfile_remove "$p"
    done
    _ovt_run_clear_state
    return "$rc"
}

# ---------------------------------------------------------------------------
# 2.6 Verb-agnostic wrappers
# ---------------------------------------------------------------------------

# ovt_run_workbench <timeout_s> [workbench args...]
# Feature #5's integration point: a new Workbench verb (-packAddon, ...) is a
# new argument list here, not new plumbing.
ovt_run_workbench() {
    local timeout_s="${1:-}"
    shift || true
    local exe
    exe="$(ovt_workbench_exe)" || return 2
    ovt_run_win "$timeout_s" "$exe" "$@"
}

# ovt_run_game <timeout_s> [client args...]
# The client MUST run with cwd = the game install dir — it finds the 'core'
# addon via ./addons relative to cwd (finding 1.14a). cwd is restored
# afterwards (no subshell, so OVT_LAST_* survive for the caller).
# NOTE: the client's exit code is ALWAYS 0, even on fatal errors (finding
# 1.14b) — read outcomes from logs/artifacts, never from this return value
# (except 124 = timeout and 2 = tool failure, which are ours).
ovt_run_game() {
    local timeout_s="${1:-}"
    shift || true
    local exe rc prev_dir
    exe="$(ovt_game_exe)" || return 2
    prev_dir="$PWD"
    if ! cd "$(dirname "$exe")"; then
        ovt_err "ovt_run_game: cannot cd to game install dir '$(dirname "$exe")'"
        return 2
    fi
    ovt_run_win "$timeout_s" "$exe" "$@" && rc=0 || rc=$?
    cd "$prev_dir" 2>/dev/null || true
    return "$rc"
}

# ---------------------------------------------------------------------------
# 2.7 Log-directory resolution
# ---------------------------------------------------------------------------

# ovt_resolve_log_dir <profile_name> <since_epoch>
# Print the WSL path of the newest logs/logs_* directory under
# <My Games>/<profile>/logs/ whose mtime >= since_epoch. Empty output and
# return 1 when nothing matches — callers MUST treat that as indeterminate,
# never as success. Capture <since_epoch> with `date +%s` BEFORE launching.
ovt_resolve_log_dir() {
    local profile="${1:-}" since="${2:-}"
    if [[ -z "$profile" ]]; then
        ovt_err "ovt_resolve_log_dir: profile name required"
        return 2
    fi
    if [[ ! "$since" =~ ^[0-9]+$ ]]; then
        ovt_err "ovt_resolve_log_dir: since_epoch must be an integer epoch, got '$since'"
        return 2
    fi
    local mg logs_root
    mg="$(ovt_mygames_dir)" || return 2
    logs_root="$mg/$profile/logs"
    if [[ ! -d "$logs_root" ]]; then
        return 1
    fi
    local d mt best="" best_mt=-1
    for d in "$logs_root"/logs_*; do
        [[ -d "$d" ]] || continue
        mt="$(stat -c %Y "$d" 2>/dev/null)" || continue
        if (( mt >= since )); then
            # Prefer newest mtime; on a tie the lexically-greater name wins
            # (logs_ dir names are timestamp-sortable).
            if (( mt > best_mt )) || { (( mt == best_mt )) && [[ "$d" > "$best" ]]; }; then
                best="$d"
                best_mt="$mt"
            fi
        fi
    done
    if [[ -z "$best" ]]; then
        return 1
    fi
    printf '%s\n' "$best"
}

# ---------------------------------------------------------------------------
# 2.9 Self-test (only reachable when EXECUTED, never when sourced)
# ---------------------------------------------------------------------------

_ovt_self_test() {
    local pass=0 fail=0

    _st_ok()   { printf 'PASS: %s\n' "$1"; pass=$(( pass + 1 )); }
    _st_bad()  { printf 'FAIL: %s%s\n' "$1" "${2:+ — $2}"; fail=$(( fail + 1 )); }
    _st_eq()   { # <desc> <expected> <actual>
        if [[ "$2" == "$3" ]]; then _st_ok "$1"; else _st_bad "$1" "expected [$2] got [$3]"; fi
    }

    local out rc expected

    # --- path translation -------------------------------------------------
    out="$(ovt_win_path "$OVT_REPO_ROOT" 2>/dev/null)" && rc=0 || rc=$?
    if (( rc == 0 )) && [[ "$out" =~ ^[A-Za-z]:\\ ]]; then
        _st_ok "ovt_win_path repo root (spaces) -> drive path: $out"
    else
        _st_bad "ovt_win_path repo root" "rc=$rc out=[$out]"
    fi
    local win_root="$out"

    out="$(ovt_win_path "$OVT_REPO_ROOT/" 2>/dev/null)" && rc=0 || rc=$?
    _st_eq "ovt_win_path trailing slash normalised" "$win_root" "$out"

    out="$(ovt_wsl_path "$win_root" 2>/dev/null)" && rc=0 || rc=$?
    _st_eq "round-trip wsl -> win -> wsl (repo root)" "$OVT_REPO_ROOT" "$out"

    out="$(ovt_win_path "/mnt/c/Program Files" 2>/dev/null)" && rc=0 || rc=$?
    _st_eq "ovt_win_path C: drive with spaces" 'C:\Program Files' "$out"

    out="$(ovt_wsl_path 'N:\Projects\Arma 4' 2>/dev/null)" && rc=0 || rc=$?
    _st_eq "ovt_wsl_path N: backslashes + spaces" '/mnt/n/Projects/Arma 4' "$out"

    out="$(ovt_wsl_path 'C:/Users/Nobody/' 2>/dev/null)" && rc=0 || rc=$?
    _st_eq "ovt_wsl_path forward slashes + trailing slash" '/mnt/c/Users/Nobody' "$out"

    # Non-existent path under a mounted drive: pure transform, still correct.
    expected="${win_root}\\tools\\__ovt_selftest_missing__\\deep.txt"
    out="$(ovt_win_path "$OVT_REPO_ROOT/tools/__ovt_selftest_missing__/deep.txt" 2>/dev/null)" && rc=0 || rc=$?
    _st_eq "ovt_win_path non-existent path under drive" "$expected" "$out"

    # Untranslatable input must FAIL, never emit a silently-wrong UNC path.
    out="$(ovt_win_path "/nonexistent_ovt_selftest/zzz" 2>/dev/null)" && rc=0 || rc=$?
    if (( rc != 0 )) && [[ -z "$out" ]]; then
        _st_ok "ovt_win_path untranslatable path returns non-zero, no output"
    else
        _st_bad "ovt_win_path untranslatable path" "rc=$rc out=[$out]"
    fi

    out="$(ovt_wsl_path 'not-a-windows-path' 2>/dev/null)" && rc=0 || rc=$?
    if (( rc != 0 )) && [[ -z "$out" ]]; then
        _st_ok "ovt_wsl_path garbage input returns non-zero, no output"
    else
        _st_bad "ovt_wsl_path garbage input" "rc=$rc out=[$out]"
    fi

    # --- repo root / resolvers --------------------------------------------
    if [[ -f "$OVT_REPO_ROOT/addon.gproj" ]]; then
        _st_ok "repo root resolved from library location (cwd=$PWD): $OVT_REPO_ROOT"
    else
        _st_bad "repo root resolution" "no addon.gproj under $OVT_REPO_ROOT"
    fi

    out="$(ovt_mygames_dir 2>/dev/null)" && rc=0 || rc=$?
    if (( rc == 0 )) && [[ -d "$out" ]]; then
        _st_ok "ovt_mygames_dir discovered existing dir: $out"
    else
        _st_bad "ovt_mygames_dir" "rc=$rc out=[$out]"
    fi

    out="$(OVERTHROW_WORKBENCH_EXE=/nonexistent/wb.exe ovt_workbench_exe 2>&1)" && rc=0 || rc=$?
    if (( rc == 2 )) && [[ "$out" == *OVERTHROW_WORKBENCH_EXE* ]]; then
        _st_ok "ovt_workbench_exe bogus override -> rc 2 + actionable message"
    else
        _st_bad "ovt_workbench_exe bogus override" "rc=$rc out=[$out]"
    fi

    out="$(OVERTHROW_GPROJ=/nonexistent/x.gproj ovt_project_gproj 2>&1)" && rc=0 || rc=$?
    if (( rc == 2 )) && [[ "$out" == *OVERTHROW_GPROJ* ]]; then
        _st_ok "ovt_project_gproj bogus override -> rc 2 + actionable message"
    else
        _st_bad "ovt_project_gproj bogus override" "rc=$rc out=[$out]"
    fi

    out="$(OVERTHROW_MYGAMES_DIR=/nonexistent/mg ovt_mygames_dir 2>&1)" && rc=0 || rc=$?
    if (( rc == 2 )); then
        _st_ok "ovt_mygames_dir bogus override -> rc 2"
    else
        _st_bad "ovt_mygames_dir bogus override" "rc=$rc out=[$out]"
    fi

    out="$(unset OVERTHROW_PROFILE_NAME; ovt_profile_name)" && rc=0 || rc=$?
    _st_eq "ovt_profile_name default" "OverthrowCI" "$out"

    out="$(ovt_profile_dir SomeProfile 2>/dev/null)" && rc=0 || rc=$?
    expected="$(ovt_mygames_dir 2>/dev/null)/SomeProfile"
    _st_eq "ovt_profile_dir composes <mygames>/<name>" "$expected" "$out"

    out="$(ovt_workbench_addons_dir_arg 2>/dev/null)" && rc=0 || rc=$?
    if (( rc == 0 )) && [[ "$out" == *,* && "$out" =~ ^[A-Za-z]:\\ ]]; then
        _st_ok "ovt_workbench_addons_dir_arg -> comma-separated Windows dirs: $out"
    else
        _st_bad "ovt_workbench_addons_dir_arg" "rc=$rc out=[$out]"
    fi

    out="$(OVERTHROW_ADDONS_DIRS='X:\a,Y:\b' ovt_workbench_addons_dir_arg 2>/dev/null)" && rc=0 || rc=$?
    _st_eq "ovt_workbench_addons_dir_arg honours OVERTHROW_ADDONS_DIRS" 'X:\a,Y:\b' "$out"

    # --- process control (no launches!) ------------------------------------
    if ovt_is_running "OvtSelfTestNoSuchImage__.exe" 2>/dev/null; then
        _st_bad "ovt_is_running bogus image" "returned 0 for a non-existent image"
    else
        _st_ok "ovt_is_running bogus image returns non-zero"
    fi

    out="$(ovt_kill_tree "not-a-pid" 2>&1)" && rc=0 || rc=$?
    if (( rc == 2 )); then
        _st_ok "ovt_kill_tree rejects non-numeric PID with rc 2"
    else
        _st_bad "ovt_kill_tree non-numeric PID" "rc=$rc"
    fi

    # --- log resolution -----------------------------------------------------
    out="$(ovt_resolve_log_dir "$(ovt_profile_name)" "$(( $(date +%s) + 999999 ))" 2>/dev/null)" && rc=0 || rc=$?
    if (( rc != 0 )) && [[ -z "$out" ]]; then
        _st_ok "ovt_resolve_log_dir future since -> empty + non-zero"
    else
        _st_bad "ovt_resolve_log_dir future since" "rc=$rc out=[$out]"
    fi

    out="$(ovt_resolve_log_dir "" 5 2>/dev/null)" && rc=0 || rc=$?
    if (( rc == 2 )); then
        _st_ok "ovt_resolve_log_dir missing profile arg -> rc 2"
    else
        _st_bad "ovt_resolve_log_dir missing profile arg" "rc=$rc"
    fi

    # --- run_win argument validation (no launches!) -------------------------
    out="$(ovt_run_win 0 /bin/true 2>&1)" && rc=0 || rc=$?
    if (( rc == 2 )); then
        _st_ok "ovt_run_win rejects timeout 0 with rc 2"
    else
        _st_bad "ovt_run_win timeout 0" "rc=$rc"
    fi

    out="$(ovt_run_win 5 /nonexistent/foo.exe 2>&1)" && rc=0 || rc=$?
    if (( rc == 2 )); then
        _st_ok "ovt_run_win rejects missing exe with rc 2"
    else
        _st_bad "ovt_run_win missing exe" "rc=$rc"
    fi

    # --- pidfile registry + sweep (isolated registry; no launches, no kills) --
    local saved_registry="$OVT_PID_REGISTRY"
    OVT_PID_REGISTRY="$(mktemp -d)"

    printf 'OvtSelfTestNoSuchImage__.exe\n0\n' > "$OVT_PID_REGISTRY/4009999997.pid"
    out="$(ovt_sweep_stale 2>/dev/null)" && rc=0 || rc=$?
    if (( rc == 0 )) && [[ -z "$out" && ! -e "$OVT_PID_REGISTRY/4009999997.pid" ]]; then
        _st_ok "ovt_sweep_stale cleans a dead-PID record: lists nothing, kills nothing"
    else
        _st_bad "ovt_sweep_stale dead-PID record" "rc=$rc out=[$out]"
    fi

    out="$(ovt_sweep_stale --bogus 2>&1)" && rc=0 || rc=$?
    if (( rc == 2 )); then
        _st_ok "ovt_sweep_stale rejects unknown argument with rc 2"
    else
        _st_bad "ovt_sweep_stale unknown argument" "rc=$rc"
    fi

    rm -rf "$OVT_PID_REGISTRY"
    OVT_PID_REGISTRY="$saved_registry"

    # --- run-trap save/restore (no signals raised; side-effect-free) ---------
    local orig_trap cur_trap
    trap 'echo __ovt_selftest_trap__' INT
    orig_trap="$(trap -p INT)"
    _ovt_install_run_traps
    cur_trap="$(trap -p INT)"
    if [[ "$cur_trap" == *_ovt_on_run_signal* ]]; then
        _st_ok "_ovt_install_run_traps installs the run INT trap"
    else
        _st_bad "_ovt_install_run_traps" "INT trap now [$cur_trap]"
    fi
    _ovt_restore_run_traps
    cur_trap="$(trap -p INT)"
    _st_eq "_ovt_restore_run_traps restores the caller's previous INT trap" "$orig_trap" "$cur_trap"
    trap - INT
    _ovt_restore_run_traps   # idempotent no-op when nothing is installed
    _st_ok "_ovt_restore_run_traps is a no-op when no run traps are installed"

    printf '\nself-test: %d passed, %d failed\n' "$pass" "$fail"
    (( fail == 0 ))
}

# ---------------------------------------------------------------------------
# Entry point (executed, not sourced)
# ---------------------------------------------------------------------------

if [[ "${BASH_SOURCE[0]}" == "$0" ]]; then
    set -euo pipefail
    case "${1:-}" in
        --self-test)
            if _ovt_self_test; then
                exit 0
            else
                exit 1
            fi
            ;;
        --sweep-stale)
            shift
            _sweep_rc=0
            ovt_sweep_stale "$@" || _sweep_rc=$?
            exit "$_sweep_rc"
            ;;
        *)
            {
                echo "usage: bash ${BASH_SOURCE[0]} --self-test"
                echo "       bash ${BASH_SOURCE[0]} --sweep-stale [--kill]"
                echo "       (or: source ${BASH_SOURCE[0]})"
            } >&2
            exit 2
            ;;
    esac
fi
