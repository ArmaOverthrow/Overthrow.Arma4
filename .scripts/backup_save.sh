#!/bin/bash

# Script to back up the Overthrow save directory into .saves/
#
# Usage: backup_save.sh [--profile <name>] [<name>]
#
#   <name>               Archive name; skips the interactive prompt. Sanitised
#                        exactly like a typed name. With no <name> the prompt
#                        is used, unchanged.
#   --profile <name>     Back up <My Games>/<name>/profile/.db/Overthrow instead
#                        of the Workbench profile (resolved through
#                        tools/lib/common.sh), e.g. --profile OverthrowCI
#   OVERTHROW_SAVE_DIR   Explicit save directory - always wins over --profile
#
# See tools/README.md (Save-state control).
#
# Exit codes: 0 = archive created; 1 = usage error, save directory missing,
# invalid name, or tar failed; 2 = --profile could not be resolved.

SCRIPT_DIR="$(dirname "$0")"
DEFAULT_SAVE_DIR="/mnt/c/Users/Aaron Static/OneDrive/Documents/My Games/ArmaReforgerWorkbench/profile/.db/Overthrow"
PROFILE_NAME=""
SAVE_NAME_ARG=""

usage() {
    echo "Usage: $(basename "$0") [--profile <name>] [<name>]"
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
        -*)
            echo "Unknown argument: $1"
            usage
            exit 1
            ;;
        *)
            if [ -n "$SAVE_NAME_ARG" ]; then
                echo "Too many arguments: $1"
                usage
                exit 1
            fi
            SAVE_NAME_ARG="$1"
            shift
            ;;
    esac
done

# Path to the save directory - environment variable, then --profile, then
# default. A set-but-empty OVERTHROW_SAVE_DIR is an error, not a fallback.
if [ -n "${OVERTHROW_SAVE_DIR+set}" ]; then
    SAVE_DIR="$OVERTHROW_SAVE_DIR"
    if [ -n "$PROFILE_NAME" ]; then
        echo "OVERTHROW_SAVE_DIR is set - ignoring --profile $PROFILE_NAME"
    fi
    if [ -z "$SAVE_DIR" ]; then
        echo "OVERTHROW_SAVE_DIR is set but empty - refusing to guess a save directory"
        exit 1
    fi
elif [ -n "$PROFILE_NAME" ]; then
    SAVE_DIR="$(resolve_profile_save_dir "$PROFILE_NAME")" || exit 2
else
    SAVE_DIR="$DEFAULT_SAVE_DIR"
fi

# Create .saves directory if it doesn't exist
BACKUP_DIR="$SCRIPT_DIR/../.saves"
mkdir -p "$BACKUP_DIR"

# Check if save directory exists
if [ ! -d "$SAVE_DIR" ]; then
    echo "Save directory not found: $SAVE_DIR"
    exit 1
fi

if [ -n "$SAVE_NAME_ARG" ]; then
    # Name given on the command line - no prompt
    SAVE_NAME="$SAVE_NAME_ARG"
else
    echo "Suggested naming scheme: <testworld|everon>_<situation>_<MP|SP>"

    # Ask for backup name
    read -p "Enter a name for this save backup: " SAVE_NAME
fi

# Remove spaces and special characters from the name
SAVE_NAME=$(echo "$SAVE_NAME" | tr ' ' '_' | tr -cd '[:alnum:]._-')

if [ -z "$SAVE_NAME" ]; then
    echo "Invalid save name"
    exit 1
fi

# Create timestamp
TIMESTAMP=$(date +"%Y%m%d_%H%M%S")
BACKUP_FILE="$BACKUP_DIR/${SAVE_NAME}_${TIMESTAMP}.tar.gz"

# Create the backup
echo "Creating backup: $BACKUP_FILE"
tar -czf "$BACKUP_FILE" -C "$(dirname "$SAVE_DIR")" "$(basename "$SAVE_DIR")"

if [ $? -eq 0 ]; then
    echo "Backup created successfully: $BACKUP_FILE"
    echo "Size: $(du -h "$BACKUP_FILE" | cut -f1)"
else
    echo "Failed to create backup"
    exit 1
fi
