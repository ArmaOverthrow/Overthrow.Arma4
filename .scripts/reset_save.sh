#!/bin/bash

# Script to reset Overthrow save data by deleting the save directory
#
# Usage: reset_save.sh [--profile <name>]
#
#   --profile <name>     Target <My Games>/<name>/profile/.db/Overthrow instead
#                        of the Workbench profile (resolved through
#                        tools/lib/common.sh), e.g. --profile OverthrowCI
#   OVERTHROW_SAVE_DIR   Explicit save directory - always wins over --profile
#
# This is an rm -rf, it is now called unattended from automation, and its
# default target is a real campaign save. So: the resolved path is always
# printed, and a path that does not look like a save DB is refused.
# See tools/README.md (Save-state control).
#
# Exit codes: 0 = deleted, or nothing to delete; 1 = usage error, refused
# path, or delete failed; 2 = --profile could not be resolved.

SCRIPT_DIR="$(dirname "$0")"
DEFAULT_SAVE_DIR="/mnt/c/Users/Aaron Static/OneDrive/Documents/My Games/ArmaReforgerWorkbench/profile/.db/Overthrow"
PROFILE_NAME=""

usage() {
    echo "Usage: $(basename "$0") [--profile <name>]"
}

# Resolve <My Games>/<name>/profile/.db/Overthrow through tools/lib/common.sh.
# Messages go to stderr - stdout is the resolved path only.
resolve_profile_save_dir() {
    local name="$1"
    local lib="$SCRIPT_DIR/../tools/lib/common.sh"
    if [ ! -f "$lib" ]; then
        echo "Cannot resolve --profile $name: $lib not found" >&2
        return 2
    fi
    # shellcheck source=/dev/null
    . "$lib" || return 2
    local root
    root="$(ovt_profile_dir "$name")" || return 2
    printf '%s/profile/.db/Overthrow\n' "$root"
}

# Parse arguments
while [ $# -gt 0 ]; do
    case "$1" in
        --profile)
            PROFILE_NAME="$2"
            if [ -z "$PROFILE_NAME" ]; then
                echo "--profile requires a profile name"
                usage
                exit 1
            fi
            shift 2
            ;;
        --profile=*)
            PROFILE_NAME="${1#*=}"
            if [ -z "$PROFILE_NAME" ]; then
                echo "--profile requires a profile name"
                usage
                exit 1
            fi
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "Unknown argument: $1"
            usage
            exit 1
            ;;
    esac
done

# Path to the save directory - environment variable, then --profile, then
# default. A set-but-empty OVERTHROW_SAVE_DIR is NOT silently replaced by the
# default: it is kept, and the guard below refuses it.
if [ -n "${OVERTHROW_SAVE_DIR+set}" ]; then
    SAVE_PATH="$OVERTHROW_SAVE_DIR"
elif [ -n "$PROFILE_NAME" ]; then
    SAVE_PATH="$(resolve_profile_save_dir "$PROFILE_NAME")" || exit 2
else
    SAVE_PATH="$DEFAULT_SAVE_DIR"
fi

echo "Resetting Overthrow save data..."

if [ -n "${OVERTHROW_SAVE_DIR+set}" ] && [ -n "$PROFILE_NAME" ]; then
    echo "OVERTHROW_SAVE_DIR is set - ignoring --profile $PROFILE_NAME"
fi

echo "Resolved save directory: $SAVE_PATH"

# Destructive-path guard - refuse anything that is not an absolute path
# ending in .db/Overthrow (empty and "/" included)
GUARD_PATH="$SAVE_PATH"
while [ "${GUARD_PATH%/}" != "$GUARD_PATH" ]; do
    GUARD_PATH="${GUARD_PATH%/}"
done

if [ -z "$GUARD_PATH" ]; then
    echo "REFUSING to delete: the save path is empty or '/'"
    echo "Nothing was deleted."
    exit 1
fi

case "$GUARD_PATH" in
    /*/.db/Overthrow)
        ;;
    *)
        echo "REFUSING to delete: '$SAVE_PATH'"
        echo "That is not a save DB path (expected an absolute path ending in .db/Overthrow)."
        echo "Nothing was deleted."
        exit 1
        ;;
esac

SAVE_PATH="$GUARD_PATH"

if [ -d "$SAVE_PATH" ]; then
    echo "Deleting save directory: $SAVE_PATH"
    if ! rm -rf "$SAVE_PATH"; then
        echo "Failed to delete save directory: $SAVE_PATH"
        exit 1
    fi
    echo "Save data deleted successfully!"
else
    echo "Save directory not found: $SAVE_PATH"
    echo "Nothing to delete."
fi

echo "Done."
