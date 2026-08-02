#!/bin/bash
#
# tools/run-tests.sh — run Overthrow's autotests in the real game client and
# return an HONEST verdict derived from junit.xml.
#
# The game client's exit code is ALWAYS 0, even on fatal errors (findings
# 1.14b), so a verdict must be POSITIVELY PROVEN from artifacts. Exit 0
# requires ALL of:
#   1. tools/launch-game.sh completed the run (not timeout, not tool failure),
#   2. the Overthrow addon is proven loaded (its addon.gproj listed INSIDE a
#      'Loaded addons:' block of this run's console.log — secondary guard),
#   3. junit.xml exists in THIS run's log dir and parses,
#   4. it contains at least one <testcase>,
#   5. it contains zero <failure>/<error> ELEMENTS.
# Anything ambiguous is exit 2 ("could not determine"), never 0 and never 1.
#
# CRITICAL parser fact (verified on 1.7.0.54): <testsuite> carries NO
# failures=/errors= attributes — failures MUST be counted as elements.
# CRITICAL false-green trap (verified): an INVALID -autotest target yields
# launcher rc 0 + client exit 0 + a clean self-exit and NO junit.xml — the
# missing-artifact check is the only thing that catches it (findings 2.7).
#
# Empirical ground truth: docs/features/dev-ops/autotest-foundation/findings.md
# Contract / plan:        docs/features/dev-ops/autotest-foundation/implementation.md
#
# stdout carries ONLY failing test names, one per line; everything else goes
# to stderr. Launching is done EXCLUSIVELY via tools/launch-game.sh (feature
# #1's process boundary) — this script names no executable path, kills no
# process, and resolves no log dir of its own.

set -euo pipefail

_RT_SCRIPT_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd -P)"
# shellcheck source=lib/common.sh
source "$_RT_SCRIPT_DIR/lib/common.sh"

OUT_DIR="$OVT_REPO_ROOT/.tmp/run-tests"
LAUNCHER="$_RT_SCRIPT_DIR/launch-game.sh"

usage() {
    cat <<EOF
usage: tools/run-tests.sh [--timeout <s>] [--keep-artifacts] [--verbose]
                          [-h|--help] [<target>]

Runs Overthrow's autotests by launching the Arma Reforger game client with
-autotest <target> (via tools/launch-game.sh), collects the run's artifacts
into .tmp/run-tests/, and derives an honest exit code from junit.xml.
A green run takes ~15-25s; a brief game window appears (no headless mode
exists).

<target> is any value the engine's -autotest parameter accepts:
    a suite class inheriting SCR_AutotestSuiteBase   (e.g. OVT_TEST_SmokeSuite)
    a case class inheriting SCR_AutotestCaseBase     (e.g. OVT_TEST_Smoke_HarnessRuns)
    a {GUID} of an SCR_AutotestGroup config          (e.g. "{6AB9C8EEE9A651B5}")
Default: OVT_TEST_SmokeSuite. NOTE there is NO "run everything" form — the
engine disables every suite not explicitly named; running more than one suite
in one launch requires an SCR_AutotestGroup config (needed at suite #2 —
feature #3's problem).

stdout carries ONLY the names of failing tests, one per line (empty on
success). The human-readable summary and all diagnostics go to stderr.

Artifacts (stale ones are DELETED before every launch; a previous run's
junit.xml can never satisfy this run's verdict):
    .tmp/run-tests/junit.xml             the verdict source (engine-fixed path)
    .tmp/run-tests/autotest.log          per-test framework log
    .tmp/run-tests/autotest_failed.log   failing test names (0 bytes when green)
    .tmp/run-tests/console.log           full client log
    .tmp/run-tests/crash.log             only present when the client crashed
                                         (e.g. invalid -autotest target)

Options:
  --timeout <s>     Kill the client after <s> seconds. Default:
                    \$OVERTHROW_TEST_TIMEOUT, else 300. The timeout is
                    MANDATORY protection, not advisory: the framework's
                    Setup_AwaitWorld has no timeout of its own, so a world
                    transition that never completes is an infinite hang.
  --keep-artifacts  Additionally copy the run's remaining logs (error.log,
                    script.log) into .tmp/run-tests/ and print the artifact
                    directory and source log dir on stderr.
  --verbose         Extra diagnostics on stderr (resolved target, log dir,
                    parse counts).
  -h, --help        This help.

Exit codes:
  0    All tests passed — POSITIVELY VERIFIED, never assumed. Requires ALL
       of: launcher completed, Overthrow addon proven loaded, junit.xml
       present and parseable, >=1 <testcase>, zero <failure>/<error>
       elements. "No failures found" in a file that does not exist is NOT
       a pass.
  1    At least one test failed: <failure>/<error> elements present in
       junit.xml. Failing test names on stdout.
  2    Tool/environment failure or INDETERMINATE result: launcher tool
       failure, missing/unparseable junit.xml (this is how an INVALID
       -autotest target manifests — the client exits cleanly, rc 0, writing
       nothing), zero test cases (e.g. an empty group), addon-not-loaded.
       Never means "pass" and never means "your tests are broken".
  124  Timed out (GNU timeout convention, propagated unchanged from
       launch-game.sh). The client is killed by PID and the kill verified.
  130/143  Interrupted (SIGINT/SIGTERM) — propagated from launch-game.sh,
       which kills the client (verified) first; no orphan remains.

Environment:
  OVERTHROW_TEST_TIMEOUT   Default --timeout in seconds (default 300).
  (Everything else — exe/profile/addon locations — belongs to
  tools/launch-game.sh and tools/lib/common.sh; see tools/README.md.)

Notes:
  * The always-failing OVT_TEST_MetaSuite exists solely to verify the red
    path: 'tools/run-tests.sh OVT_TEST_MetaSuite' must exit 1. It is never
    part of a default run.
  * The harness starts twice and loads the test world twice per launch —
    doubled 'CLI autotest'/'Requesting scenario change:' console lines are
    normal, not an anomaly (findings: "Harness runs twice").
  * Orphans from an uncleanly-killed wrapper can be reaped with:
        bash tools/lib/common.sh --sweep-stale [--kill]
    (strictly registry-scoped — never touches a session you launched
    yourself).
EOF
}

# --- argument parsing ---------------------------------------------------------

TIMEOUT_S="${OVERTHROW_TEST_TIMEOUT:-300}"
KEEP_ARTIFACTS=0
VERBOSE=0
TARGET=""

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
        --keep-artifacts) KEEP_ARTIFACTS=1; shift ;;
        --verbose)        VERBOSE=1; shift ;;
        -h|--help)        usage; exit 0 ;;
        -*)
            ovt_err "unknown argument '$1' (see --help; the target is a bare class name or {GUID}, not a flag)"
            exit 2
            ;;
        *)
            if [[ -n "$TARGET" ]]; then
                ovt_err "exactly one <target> is accepted, got '$TARGET' and '$1' (the engine takes ONE -autotest value; running several suites needs an SCR_AutotestGroup config — see --help)"
                exit 2
            fi
            TARGET="$1"
            shift
            ;;
    esac
done

TARGET="${TARGET:-OVT_TEST_SmokeSuite}"

if [[ ! "$TIMEOUT_S" =~ ^[0-9]+$ ]] || (( TIMEOUT_S < 1 )); then
    ovt_err "OVERTHROW_TEST_TIMEOUT must be a positive integer, got '$TIMEOUT_S'"
    exit 2
fi

# --- helpers ------------------------------------------------------------------

# indeterminate <reason> — the three-valued escape hatch. Never 0, never 1.
indeterminate() {
    printf 'run-tests: INDETERMINATE: %s (artifacts: %s)\n' "$1" "$OUT_DIR" >&2
    exit 2
}

vlog() {
    if (( VERBOSE )); then ovt_info "$@"; fi
}

# --- preflight ----------------------------------------------------------------

if [[ ! -x "$LAUNCHER" ]]; then
    ovt_err "tools/launch-game.sh not found or not executable at '$LAUNCHER'"
    exit 2
fi

mkdir -p "$OUT_DIR" || { ovt_err "cannot create '$OUT_DIR'"; exit 2; }
# Stale artifacts must be IMPOSSIBLE: a previous run's junit.xml satisfying
# this run's verdict is the false-green trap (Q2). Delete before launch.
rm -f "$OUT_DIR"/*

vlog "target:   $TARGET"
vlog "timeout:  ${TIMEOUT_S}s"

# --- save-state precondition ---------------------------------------------------
# The round-trip suite (in the All group since 2026-08-02) asserts the
# no-save -> save TRANSITION, which is only meaningful in a fresh session.
# Every run therefore starts from a clean CI save state — determinism for
# every other suite too. Skipped when the caller pins OVERTHROW_SAVE_DIR
# (fixture workflows manage their own state via activate_save.sh).
RESETTER="$_RT_SCRIPT_DIR/../.scripts/reset_save.sh"
if [[ -z "${OVERTHROW_SAVE_DIR+set}" ]]; then
    if [[ -x "$RESETTER" ]]; then
        "$RESETTER" --profile OverthrowCI >/dev/null || {
            ovt_err "save-state reset failed (.scripts/reset_save.sh --profile OverthrowCI)"
            exit 2
        }
        vlog "save state: reset (profile OverthrowCI)"
    else
        ovt_err "reset_save.sh not found/executable at '$RESETTER'"
        exit 2
    fi
fi

# --- launch (exclusively via feature #1's boundary) ----------------------------

RC=0
LAUNCH_OUT="$("$LAUNCHER" --timeout "$TIMEOUT_S" -- -autotest "$TARGET")" && RC=0 || RC=$?

if (( RC == 124 )); then
    # No stdout contract on timeout — nothing to collect; the launcher already
    # reported the partial-log location and verified the kill on stderr.
    printf 'run-tests: TIMEOUT after %ss (client killed and verified dead; result unknown)\n' "$TIMEOUT_S" >&2
    exit 124
fi
if (( RC == 130 || RC == 143 )); then
    exit "$RC"
fi
if (( RC != 0 )); then
    indeterminate "tools/launch-game.sh failed with exit code $RC (see its stderr above)"
fi

LOG_DIR="$(printf '%s\n' "$LAUNCH_OUT" | sed -n 's/^LOG_DIR=//p')"
DUR="$(printf '%s\n' "$LAUNCH_OUT" | sed -n 's/^DURATION_S=//p')"
DUR="${DUR:-?}"
if [[ -z "$LOG_DIR" || ! -d "$LOG_DIR" ]]; then
    indeterminate "launcher exited 0 but reported no usable LOG_DIR ('$LOG_DIR')"
fi

vlog "log dir:  $LOG_DIR"

# --- collect artifacts (before any verdict, so post-mortems always have them) --

for f in junit.xml autotest.log autotest_failed.log console.log crash.log; do
    if [[ -f "$LOG_DIR/$f" ]]; then
        cp -f "$LOG_DIR/$f" "$OUT_DIR/$f"
    fi
done
if (( KEEP_ARTIFACTS )); then
    for f in error.log script.log; do
        if [[ -f "$LOG_DIR/$f" ]]; then
            cp -f "$LOG_DIR/$f" "$OUT_DIR/$f"
        fi
    done
    ovt_info "artifacts kept: $OUT_DIR (source: $LOG_DIR)"
fi

# --- secondary guard: Overthrow proven loaded ----------------------------------
# Block-scoped, like compile-check.sh: the gproj path also appears in the
# CLI-params echo even when the addon is dropped, so a whole-log grep would
# produce false passes (feature #1's finding 1.8d).

if [[ ! -f "$OUT_DIR/console.log" ]]; then
    indeterminate "the run produced no console.log — nothing about it can be trusted"
fi
ADDON_PROOF="$(awk '
    {
        sub(/\r$/, "")
        if (in_addons) {
            if (index($0, "gproj:") > 0) {
                if ($0 ~ /Overthrow\.Arma4[\/\\]addon\.gproj/) { found = 1 }
                next
            }
            in_addons = 0
        }
        if ($0 ~ /Loaded addons:/) { in_addons = 1 }
    }
    END { print found + 0 }
' "$OUT_DIR/console.log")"
if [[ "$ADDON_PROOF" != 1 ]]; then
    indeterminate "no proof the Overthrow addon was loaded ('Overthrow.Arma4/addon.gproj' absent from every 'Loaded addons:' block of console.log) — the run's tests, if any ran, were not Overthrow's"
fi

# --- junit.xml presence (the invalid-target detector) --------------------------

if [[ ! -f "$OUT_DIR/junit.xml" ]]; then
    REASON="the run produced no junit.xml"
    if [[ -f "$OUT_DIR/crash.log" ]] && grep -q "Invalid -autotest parameter value" "$OUT_DIR/crash.log"; then
        REASON="'$TARGET' is not a valid -autotest target (crash.log: Invalid -autotest parameter value) and no junit.xml was produced"
    fi
    indeterminate "$REASON — a missing artifact is never a pass"
fi

# --- parse junit.xml -----------------------------------------------------------
# Shapes (verbatim in findings.md): a passing <testcase> is self-closing; a
# failing one wraps <failure type="Result">message</failure>. <testsuite> has
# NO failures=/errors= attributes on this build — elements MUST be counted.
# Output records: WELLFORMED, CASE, FAILELEM, ERRELEM, FAILCASE\t<name>.

PARSE_OUT="$(awk '
    {
        sub(/\r$/, "")
        line = $0
        if (line ~ /<testsuites[ >]/ || line ~ /<testsuites\/>/) { open_root = 1 }
        if (line ~ /<\/testsuites>/ && open_root)               { closed_root = 1 }
        if (match(line, /<testcase[ \t]/)) {
            print "CASE"
            in_case = 1
            case_failed = 0
            case_name = ""
            # NOTE the anchor: a bare /name="/ would match inside classname="
            if (match(line, /[ \t]name="[^"]*"/)) {
                case_name = substr(line, RSTART + 7, RLENGTH - 8)
            }
            # self-closing testcase on the same line = passing, closed
            if (line ~ /\/>[ \t]*$/) { in_case = 0 }
        }
        n = gsub(/<failure[ \t>]/, "&", line)
        if (n > 0) {
            for (i = 0; i < n; i++) print "FAILELEM"
            if (in_case && !case_failed) { case_failed = 1; printf "FAILCASE\t%s\n", case_name }
        }
        n = gsub(/<error[ \t>]/, "&", line)
        if (n > 0) {
            for (i = 0; i < n; i++) print "ERRELEM"
            if (in_case && !case_failed) { case_failed = 1; printf "FAILCASE\t%s\n", case_name }
        }
        if (line ~ /<\/testcase>/) { in_case = 0 }
    }
    END { if (open_root && closed_root) print "WELLFORMED" }
' "$OUT_DIR/junit.xml")"

WELLFORMED=0
N_CASES=0
N_FAIL_ELEMS=0
N_ERR_ELEMS=0
FAIL_NAMES=()
while IFS= read -r rec; do
    case "$rec" in
        WELLFORMED)        WELLFORMED=1 ;;
        CASE)              N_CASES=$(( N_CASES + 1 )) ;;
        FAILELEM)          N_FAIL_ELEMS=$(( N_FAIL_ELEMS + 1 )) ;;
        ERRELEM)           N_ERR_ELEMS=$(( N_ERR_ELEMS + 1 )) ;;
        FAILCASE$'\t'*)    FAIL_NAMES+=("${rec#FAILCASE$'\t'}") ;;
    esac
done <<< "$PARSE_OUT"

N_BAD=$(( N_FAIL_ELEMS + N_ERR_ELEMS ))
vlog "parsed junit.xml: wellformed=$WELLFORMED cases=$N_CASES failure-elements=$N_FAIL_ELEMS error-elements=$N_ERR_ELEMS"

if (( ! WELLFORMED )); then
    indeterminate "junit.xml is not parseable (no matching <testsuites>...</testsuites> root)"
fi

# --- verdict -------------------------------------------------------------------
# Failure ELEMENTS present -> exit 1. Zero cases -> exit 2. All passed -> 0.

if (( N_BAD > 0 )); then
    if (( ${#FAIL_NAMES[@]} == 0 )); then
        indeterminate "$N_BAD <failure>/<error> element(s) present but none attributable to a <testcase> — the parser may be out of date with this build's junit.xml shape"
    fi
    for name in "${FAIL_NAMES[@]}"; do
        printf '%s\n' "${name:-<unnamed testcase>}"
    done
    printf 'run-tests: FAILED (%d of %d) in %ss\n' "${#FAIL_NAMES[@]}" "$N_CASES" "$DUR" >&2
    exit 1
fi

if (( N_CASES == 0 )); then
    indeterminate "junit.xml contains zero <testcase> elements — nothing ran (empty group target, or the target selected no tests); zero tests can never be a pass"
fi

printf 'run-tests: OK (%d tests, %ss)\n' "$N_CASES" "$DUR" >&2
exit 0
