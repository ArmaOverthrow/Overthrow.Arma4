#!/bin/bash
#
# tools/compile-check.sh — compile all of Overthrow's EnforceScript via the
# Workbench CLI (-wbsilent -validate) and return an HONEST verdict.
#
# Exit 0 is only produced when success is POSITIVELY PROVEN — all four of:
#   1. the Workbench process exited 0,
#   2. zero in-project errors were parsed from the run's console.log,
#   3. the Game-module compile evidence marker is present in the log
#      ("Module: Game; loaded Nx files" / "PROFILING : Compiling Game ..."),
#   4. the log proves the Overthrow addon itself was loaded
#      ("Overthrow.Arma4/addon.gproj") — guards the FALSE-PASS TRAP where
#      unresolvable deps make the Workbench silently validate the BASE GAME
#      and exit 0 on a broken Overthrow tree (findings 1.8d).
# Anything ambiguous is exit 2 ("could not determine"), never 0 and never 1.
#
# Empirical ground truth: docs/features/dev-ops/workbench-automation/findings.md
# Contract / plan:        docs/features/dev-ops/workbench-automation/implementation.md
#
# stdout carries ONLY gcc-style error lines; everything else goes to stderr.

set -euo pipefail

_CC_SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
source "$_CC_SCRIPT_DIR/lib/common.sh"

OUT_DIR="$OVT_REPO_ROOT/.tmp/compile-check"
LAST_LOG="$OUT_DIR/last.log"
DIAG_FILE="$OUT_DIR/last-launch.txt"

usage() {
    cat <<EOF
usage: tools/compile-check.sh [--timeout <s>] [--all] [--absolute]
                              [--keep-log] [--verbose] [--allow-concurrent]
                              [-h|--help]

Compiles all of Overthrow's EnforceScript by launching the Arma Reforger
Workbench headlessly (-wbsilent -validate) and parsing the run's console.log.
No GUI appears; a warm run takes ~4s (the first run ever on a machine pays a
one-off ~60s project resource-DB scan).

stdout carries ONLY error lines, one per line, gcc-style:

    Scripts/Game/Some/File.c:214: error: <message>

Paths are repo-relative by default. All diagnostics and the summary line go
to stderr. The run's console.log is always copied to .tmp/compile-check/last.log
for post-mortems.

Options:
  --timeout <s>       Kill the Workbench after <s> seconds. Default:
                      \$OVERTHROW_COMPILE_TIMEOUT, else 120.
  --all               Also print errors originating OUTSIDE this repo (base
                      game / EPF / EDF dependencies), prefixed '[dep] '.
                      Without --all they are hidden from stdout but ALWAYS
                      counted and reported in the stderr summary — never
                      silently dropped.
  --absolute          Print absolute WSL paths for in-project errors instead
                      of repo-relative ones. (Dependency paths stay as the
                      engine emitted them — they do not exist in this repo.)
  --keep-log          Additionally print the kept log's path on stderr.
  --verbose           Extra diagnostics on stderr (resolved paths, exit code,
                      parse counts, evidence markers).
  --allow-concurrent  Suppress the warning when another Workbench process is
                      already running (observed benign — the check uses its
                      own pinned profile).
  -h, --help          This help.

Exit codes:
  0    Compiled clean — POSITIVELY VERIFIED, never assumed. Requires ALL of:
       process exit 0, zero in-project errors, Game-module compile evidence
       in the log, and proof the Overthrow addon was actually loaded (guards
       the base-game false-pass trap: with unresolvable dependencies the
       Workbench silently validates the BASE GAME and exits 0).
  1    Compilation failed: at least one in-project error, printed on stdout.
  2    Tool/environment failure or INDETERMINATE result: missing binary or
       project, no log found, unparseable log, dependencies failed to
       compile (not Overthrow's fault, but Overthrow was not verified),
       Overthrow addon not loaded, unverified timeout cleanup. Never means
       "pass" and never means "your code is broken".
  124  Timed out (GNU timeout convention). The Windows process is killed and
       the kill is verified; if the kill cannot be verified the exit is 2
       with a loud orphan warning naming the PID and the exact taskkill
       command.
  130/143  Interrupted (SIGINT/SIGTERM). The launched Workbench process is
       killed (verified) before the script exits — Ctrl-C leaves no orphan.

Environment (see tools/lib/common.sh for full resolution rules):
  OVERTHROW_COMPILE_TIMEOUT  Default --timeout in seconds (default 120).
  OVERTHROW_WORKBENCH_EXE    WSL path of ArmaReforgerWorkbenchSteamDiag.exe.
  OVERTHROW_GPROJ            Overthrow addon.gproj (default <repo>/addon.gproj).
  OVERTHROW_ADDONS_DIRS      Comma-separated WINDOWS-form -addonsDir value.
                             Default: <My Games>\\ArmaReforgerWorkbench\\addons
                             (packed workshop EPF/EDF) + <game install>\\addons.
  OVERTHROW_PROFILE_NAME     Pinned automation profile (default OverthrowCI).
  OVERTHROW_MYGAMES_DIR      WSL path of the 'My Games' folder (else discovered;
                             Documents may be OneDrive-redirected).

Notes:
  * There is NO --config flag: the Workbench IGNORES the optional
    '-validate [configName]' argument (verified empirically — PC, workbench
    and a bogus name behave identically). The gproj's own script
    configuration ('workbench': defines WORKBENCH, PERSISTENCE_DEBUG, ...)
    applies automatically.
  * PARSE (syntax) errors abort compilation at the FIRST failing file — only
    that file's errors are listed, so fixing it may reveal more. SEMANTIC
    errors (unknown types, bad attributes, ...) are collected across files,
    capped at ~21 before the engine emits 'Too many errors'; the summary
    reports such counts as incomplete ("N+ errors, engine stopped listing").
  * Requires: Windows host with WSL interop, Arma Reforger Tools installed
    via Steam (Steam running), and the packed EPF/EDF workshop dependencies
    downloaded once in the Workbench.
  * Every launched Windows PID is registered in .tmp/ovt-pids/ while it runs
    (removed on verified exit/kill). Orphans from an uncleanly-killed wrapper
    (e.g. SIGKILL, which bypasses the traps) can be reaped with:
        bash tools/lib/common.sh --sweep-stale [--kill]
    The sweep is strictly registry-scoped — it can never kill a Workbench
    you opened yourself.
EOF
}

# --- argument parsing --------------------------------------------------------

TIMEOUT_S="${OVERTHROW_COMPILE_TIMEOUT:-120}"
SHOW_ALL=0
ABSOLUTE=0
KEEP_LOG=0
VERBOSE=0
ALLOW_CONCURRENT=0

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
        --all)              SHOW_ALL=1; shift ;;
        --absolute)         ABSOLUTE=1; shift ;;
        --keep-log)         KEEP_LOG=1; shift ;;
        --verbose)          VERBOSE=1; shift ;;
        --allow-concurrent) ALLOW_CONCURRENT=1; shift ;;
        -h|--help)          usage; exit 0 ;;
        --config)
            ovt_err "--config is not supported: the Workbench ignores the '-validate [configName]' argument (see findings.md 1.4e); the gproj's script configuration applies automatically."
            exit 2
            ;;
        *)
            ovt_err "unknown argument '$1' (see --help)"
            exit 2
            ;;
    esac
done

if [[ ! "$TIMEOUT_S" =~ ^[0-9]+$ ]] || (( TIMEOUT_S < 1 )); then
    ovt_err "OVERTHROW_COMPILE_TIMEOUT must be a positive integer, got '$TIMEOUT_S'"
    exit 2
fi

# --- helpers ------------------------------------------------------------------

# indeterminate <reason> — the three-valued escape hatch. Never 0, never 1.
indeterminate() {
    local where="$DIAG_FILE"
    [[ -f "$LAST_LOG" ]] && where="$LAST_LOG"
    printf 'compile-check: INDETERMINATE: %s (log: %s)\n' "$1" "$where" >&2
    exit 2
}

vlog() {
    if (( VERBOSE )); then ovt_info "$@"; fi
}

# --- preflight (exit 2 with a specific message on any failure) ----------------

mkdir -p "$OUT_DIR" || { ovt_err "cannot create '$OUT_DIR'"; exit 2; }
rm -f "$LAST_LOG"   # never let a previous run's log satisfy this run's verdict

WB_EXE="$(ovt_workbench_exe)" || exit 2
GPROJ="$(ovt_project_gproj)" || exit 2
GPROJ_WIN="$(ovt_win_path "$GPROJ")" || exit 2
ADDONS_ARG="$(ovt_workbench_addons_dir_arg)" || exit 2
PROFILE="$(ovt_profile_name)"
ovt_mygames_dir >/dev/null || exit 2
IMAGE="$(basename "$WB_EXE")"

vlog "workbench:  $WB_EXE"
vlog "gproj:      $GPROJ_WIN"
vlog "addonsDir:  $ADDONS_ARG"
vlog "profile:    $PROFILE"
vlog "timeout:    ${TIMEOUT_S}s"

# Concurrency: benign in observation (findings 1.12) — warn, don't refuse.
if RUNNING_PIDS="$(ovt_pids_of "$IMAGE" 2>/dev/null)"; then
    if (( ALLOW_CONCURRENT )); then
        vlog "another $IMAGE is running (PID(s): ${RUNNING_PIDS//$'\n'/ }) — --allow-concurrent given"
    else
        ovt_warn "another $IMAGE is already running (PID(s): ${RUNNING_PIDS//$'\n'/ }). Proceeding — observed safe (the check uses its own '$PROFILE' profile), but both processes write <project>/resourceDatabase.rdb. Pass --allow-concurrent to silence this warning."
    fi
fi

# --- launch --------------------------------------------------------------------

T0="$(date +%s)"
RC=0
# The interop stub prints Steam noise on stdout/stderr — keep it out of the
# user's face but in a diagnostic file for post-mortems.
ovt_run_workbench "$TIMEOUT_S" \
    -gproj "$GPROJ_WIN" \
    -addonsDir "$ADDONS_ARG" \
    -wbsilent -validate \
    -profile "$PROFILE" \
    >"$DIAG_FILE" 2>&1 && RC=0 || RC=$?
DUR="${OVT_LAST_DURATION_S:-0}"

vlog "workbench exit code: $RC (${DUR}s)"

# --- copy the run's console.log (post-mortem artifact, and the parse target) ---

LOG_DIR=""
if LOG_DIR="$(ovt_resolve_log_dir "$PROFILE" "$T0")" && [[ -f "$LOG_DIR/console.log" ]]; then
    cp -f "$LOG_DIR/console.log" "$LAST_LOG"
    vlog "log dir:    $LOG_DIR"
    if (( KEEP_LOG )); then
        ovt_info "log kept: $LAST_LOG (source: $LOG_DIR/console.log)"
    fi
else
    LOG_DIR=""
fi

# --- timeout / launch-failure handling ------------------------------------------

if (( RC == 124 )); then
    if [[ "${OVT_LAST_KILL_FAILED:-0}" == "1" ]]; then
        ORPHAN_PIDS="${OVT_LAST_WIN_PID:-unknown}"
        ovt_err "TIMEOUT after ${TIMEOUT_S}s AND the kill could NOT be verified — an ORPHANED $IMAGE process may still be running (Windows PID(s): $ORPHAN_PIDS). Kill it manually: taskkill.exe /F /T /PID ${ORPHAN_PIDS%% *} — or run: bash tools/lib/common.sh --sweep-stale --kill (registry-scoped, cannot touch your own Workbench). Verify with: tasklist.exe /FI \"IMAGENAME eq $IMAGE\"."
        indeterminate "timed out after ${TIMEOUT_S}s and process cleanup was not verified"
    fi
    printf 'compile-check: TIMEOUT after %ss (Workbench killed and verified dead; result unknown)\n' "$TIMEOUT_S" >&2
    exit 124
fi
if (( RC == 2 )); then
    # ovt_run_win's own usage/environment failure, not a Workbench verdict.
    indeterminate "tool failure launching the Workbench (see $DIAG_FILE)"
fi
if [[ -z "$LOG_DIR" ]]; then
    indeterminate "Workbench exited $RC but this run's console.log could not be located under <My Games>/$PROFILE/logs (newest-since-launch). The compile result CANNOT be trusted without the log."
fi

# --- parse the log ---------------------------------------------------------------
# Shapes (verbatim samples in findings.md):
#   error:      <ts>  SCRIPT  (E): @"Scripts/Game/File.c,214": <message>
#   sentinel:   ... @"<file>,<line>": Too many errors        -> truncation flag
#   failure:    <ts>  SCRIPT  (E): Can't compile "Game" script module!
#   restatement (NOT counted, no SCRIPT prefix):  path(line): message
#   evidence:   Module: Game; loaded 5633x files; ...
#               PROFILING    : Compiling Game scripts took: 1502.17 ms
#   addon proof: the Overthrow addon.gproj listed INSIDE the 'Loaded addons:'
#               block. Scoping to the block is essential: the gproj path also
#               appears in the CLI-params echo and registration lines even
#               when the addon is silently DROPPED (the 1.8d false-pass trap),
#               so a whole-log grep would defeat the fourth verdict condition.
# Identical (W)-severity duplicates of (E) lines are excluded by matching (E)
# only; identical repeated (E) lines are deduped on file|line|message.

parse_log() {   # $1 = log file, $2 = expected gproj path (forward-slash form)
    awk -v expected="$2" '
    {
        sub(/\r$/, "")
        line = $0
        # "Loaded addons:" block: consecutive gproj listing lines follow it.
        if (in_addons) {
            if (index(line, "gproj:") > 0) {
                if ((expected != "" && index(line, expected) > 0) \
                    || line ~ /Overthrow\.Arma4[\/\\]addon\.gproj/) {
                    print "ADDON_PROOF"
                }
                next
            }
            in_addons = 0
        }
        if (line ~ /Loaded addons:/) { in_addons = 1; next }
        if (match(line, /^[^ \t]+[ \t]+SCRIPT[ \t]+\(E\): /)) {
            rest = substr(line, RSTART + RLENGTH)
            if (substr(rest, 1, 2) == "@\"") {
                body = substr(rest, 3)
                p = index(body, "\": ")
                if (p > 0) {
                    fl  = substr(body, 1, p - 1)
                    msg = substr(body, p + 3)
                    if (match(fl, /,[0-9]+$/)) {
                        f  = substr(fl, 1, RSTART - 1)
                        ln = substr(fl, RSTART + 1)
                        if (msg == "Too many errors") { print "TRUNC"; next }
                        key = f "|" ln "|" msg
                        if (!(key in seen)) {
                            seen[key] = 1
                            printf "ERR\t%s\t%s\t%s\n", f, ln, msg
                        }
                        next
                    }
                }
                printf "UNPARSED\t%s\n", line
                next
            }
            if (match(rest, /^Can'\''t compile "[^"]*" script module!/)) {
                mod = rest
                sub(/^Can'\''t compile "/, "", mod)
                sub(/" script module!.*$/, "", mod)
                printf "MODULEFAIL\t%s\n", mod
                next
            }
            printf "UNPARSED\t%s\n", line
            next
        }
        if (match(line, /Module: Game; loaded [0-9]+x files/)) {
            m = substr(line, RSTART, RLENGTH)
            gsub(/[^0-9]/, "", m)
            printf "EVIDENCE_FILES\t%s\n", m
            next
        }
        if (line ~ /PROFILING[ \t]*: Compiling Game scripts took:/) {
            print "EVIDENCE_PROF"
            next
        }
    }
    ' "$1"
}

PROJ_OUT=()          # gcc-style lines for in-project errors (stdout on failure)
DEP_OUT=()           # '[dep] '-prefixed lines for out-of-project errors
UNPARSED=()          # verbatim unrecognised SCRIPT (E) lines
FAILED_MODULES=""    # space-joined module names from failure markers
TRUNC=0              # 'Too many errors' sentinel seen
EVIDENCE_FILES=""    # N from 'Module: Game; loaded Nx files'
EVIDENCE_PROF=0      # PROFILING Game line seen
ADDON_PROOF=0        # our gproj listed inside the 'Loaded addons:' block

PATH_PREFIX=""
if (( ABSOLUTE )); then PATH_PREFIX="$OVT_REPO_ROOT/"; fi

# The engine echoes gproj paths with forward slashes in 'Loaded addons:'.
GPROJ_FWD="${GPROJ_WIN//\\//}"

while IFS= read -r rec; do
    case "$rec" in
        ERR$'\t'*)
            rest="${rec#ERR$'\t'}"
            f="${rest%%$'\t'*}"; rest="${rest#*$'\t'}"
            l="${rest%%$'\t'*}"; msg="${rest#*$'\t'}"
            if [[ -e "$OVT_REPO_ROOT/$f" ]]; then
                PROJ_OUT+=("${PATH_PREFIX}${f}:${l}: error: ${msg}")
            else
                DEP_OUT+=("[dep] ${f}:${l}: error: ${msg}")
            fi
            ;;
        TRUNC)              TRUNC=1 ;;
        MODULEFAIL$'\t'*)   FAILED_MODULES="${FAILED_MODULES:+$FAILED_MODULES }${rec#MODULEFAIL$'\t'}" ;;
        EVIDENCE_FILES$'\t'*) EVIDENCE_FILES="${rec#EVIDENCE_FILES$'\t'}" ;;
        EVIDENCE_PROF)      EVIDENCE_PROF=1 ;;
        ADDON_PROOF)        ADDON_PROOF=$(( ADDON_PROOF + 1 )) ;;
        UNPARSED$'\t'*)     UNPARSED+=("${rec#UNPARSED$'\t'}") ;;
    esac
done < <(parse_log "$LAST_LOG" "$GPROJ_FWD")

N_PROJ=${#PROJ_OUT[@]}
N_DEP=${#DEP_OUT[@]}
N_UNPARSED=${#UNPARSED[@]}
HAVE_EVIDENCE=0
if [[ -n "$EVIDENCE_FILES" ]] || (( EVIDENCE_PROF )); then HAVE_EVIDENCE=1; fi

vlog "parsed: $N_PROJ in-project error(s), $N_DEP dependency error(s), $N_UNPARSED unparsed error line(s)"
vlog "truncated: $TRUNC; failed module(s): ${FAILED_MODULES:-none}; Game evidence: files=${EVIDENCE_FILES:-?} profiling=$EVIDENCE_PROF; addon proof: ${ADDON_PROOF}x"

# Unparsed SCRIPT (E) lines are surfaced verbatim, never dropped.
for u in ${UNPARSED[@]+"${UNPARSED[@]}"}; do
    ovt_warn "unparsed error line: $u"
done

# --- verdict ---------------------------------------------------------------------
# Exit 0 requires ALL FOUR: RC==0, N_PROJ==0, HAVE_EVIDENCE, ADDON_PROOF>0.
# Errors in this repo -> exit 1. Everything else -> exit 2 (indeterminate).

print_error_lines() {   # stdout — the ONLY thing ever printed to stdout
    local e
    for e in ${PROJ_OUT[@]+"${PROJ_OUT[@]}"}; do printf '%s\n' "$e"; done
    if (( SHOW_ALL )); then
        for e in ${DEP_OUT[@]+"${DEP_OUT[@]}"}; do printf '%s\n' "$e"; done
    fi
}

dep_note() {            # stderr — dependency errors are counted, never hidden silently
    if (( N_DEP > 0 )); then
        if (( SHOW_ALL )); then
            printf 'compile-check: (%d errors outside the project, shown with [dep] prefix)\n' "$N_DEP" >&2
        else
            printf 'compile-check: (%d errors outside the project; re-run with --all to see them)\n' "$N_DEP" >&2
        fi
    fi
}

# 4th condition first: without addon-loaded proof NOTHING else can be trusted —
# with unresolvable deps the Workbench validates the BASE GAME and exits 0, and
# any parsed errors may belong to a different addon whose paths shadow ours.
if (( ADDON_PROOF == 0 )); then
    indeterminate "no proof the Overthrow addon was loaded ('Overthrow.Arma4/addon.gproj' absent from the log, exit code $RC). The Workbench silently falls back to validating the BASE GAME when a dependency (packed EPF/EDF) cannot be resolved — a passing exit code here would be a FALSE PASS. Check -addonsDir/OVERTHROW_ADDONS_DIRS and that the workshop dependencies are downloaded."
fi

if (( N_PROJ > 0 )); then
    print_error_lines
    if (( TRUNC )); then
        printf 'compile-check: FAILED (%d+ errors, engine stopped listing) in %ss\n' "$N_PROJ" "$DUR" >&2
    else
        printf 'compile-check: FAILED (%d errors) in %ss\n' "$N_PROJ" "$DUR" >&2
    fi
    dep_note
    exit 1
fi

# Zero in-project errors from here on.
if (( RC == 0 )); then
    if (( N_DEP > 0 )); then
        print_error_lines
        dep_note
        indeterminate "exit code 0 but $N_DEP dependency error line(s) present — contradictory evidence"
    fi
    if [[ -n "$FAILED_MODULES" ]]; then
        indeterminate "exit code 0 but the log reports failed script module(s): $FAILED_MODULES — contradictory evidence"
    fi
    if (( N_UNPARSED > 0 )); then
        indeterminate "exit code 0 but $N_UNPARSED unrecognised SCRIPT error line(s) in the log (shown above) — cannot rule out real errors"
    fi
    if (( TRUNC )); then
        indeterminate "exit code 0 but the 'Too many errors' sentinel is present — contradictory evidence"
    fi
    if (( ! HAVE_EVIDENCE )); then
        indeterminate "exit code 0 but no Game-module compile evidence in the log ('Module: Game; loaded Nx files' / 'PROFILING : Compiling Game scripts took:' both absent) — compilation cannot be confirmed to have run"
    fi
    # All four conditions positively verified.
    if [[ -n "$EVIDENCE_FILES" ]]; then
        printf 'compile-check: OK (%s files, Game module, %ss)\n' "$EVIDENCE_FILES" "$DUR" >&2
    else
        printf 'compile-check: OK (Game module compiled, %ss)\n' "$DUR" >&2
    fi
    exit 0
fi

if (( RC == 255 )); then
    if (( N_DEP > 0 )); then
        print_error_lines
        dep_note
        local_trunc=""
        if (( TRUNC )); then local_trunc="+"; fi
        indeterminate "dependencies failed to compile (${N_DEP}${local_trunc} dep errors); Overthrow could not be verified — this is not an Overthrow code failure"
    fi
    if (( N_UNPARSED > 0 )); then
        indeterminate "compile failed (module(s): ${FAILED_MODULES:-unknown}) but only unrecognised error lines were found (shown above) — the parser may be out of date with this Workbench build"
    fi
    indeterminate "Workbench exited 255 (compile failure) but no error lines could be parsed from the log"
fi

indeterminate "unexpected Workbench exit code $RC"
