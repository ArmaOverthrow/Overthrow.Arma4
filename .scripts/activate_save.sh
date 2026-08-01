#!/bin/bash

# Script to activate (restore) one of the .saves/ backups over the save directory
#
# Usage: activate_save.sh [--profile <name>] [<name-or-file>]
#
#   <name-or-file>       Selects an archive without the menu: an exact path, an
#                        exact filename in .saves/, or otherwise the NEWEST
#                        .saves/*.tar.gz whose name matches. With no argument
#                        the numbered menu is used, unchanged.
#   --profile <name>     Restore into <My Games>/<name>/profile/.db/Overthrow
#                        instead of the Workbench profile (resolved through
#                        tools/lib/common.sh), e.g. --profile OverthrowCI
#   OVERTHROW_SAVE_DIR   Explicit save directory - always wins over --profile
#
# The current save is reset (see reset_save.sh and its path guard) before the
# archive is extracted. See tools/README.md (Save-state control).
#
# Exit codes: 0 = activated, or cancelled at the menu; 1 = usage error, no
# backups, unmatched name, invalid selection, failed reset or failed extract;
# 2 = --profile could not be resolved.

# Script directory and saves location
SCRIPT_DIR="$(dirname "$0")"
BACKUP_DIR="$SCRIPT_DIR/../.saves"
DEFAULT_SAVE_DIR="/mnt/c/Users/Aaron Static/OneDrive/Documents/My Games/ArmaReforgerWorkbench/profile/.db/Overthrow"
PROFILE_NAME=""
SAVE_ARG=""

usage() {
    echo "Usage: $(basename "$0") [--profile <name>] [<name-or-file>]"
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

# Print the available backups, numbered, in the same form as the menu
list_saves() {
    local i filename size
    for i in "${!saves[@]}"; do
        filename=$(basename "${saves[$i]}")
        size=$(du -h "${saves[$i]}" | cut -f1)
        echo "[$((i+1))] $filename ($size)"
    done
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
            if [ -n "$SAVE_ARG" ]; then
                echo "Too many arguments: $1"
                usage
                exit 1
            fi
            SAVE_ARG="$1"
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

# Check if .saves directory exists
if [ ! -d "$BACKUP_DIR" ]; then
    echo "No saved backups found in $BACKUP_DIR"
    exit 1
fi

# Find all tar.gz files (read line by line - the repo path can contain spaces
# when the script is invoked by absolute path)
saves=()
while IFS= read -r found_save; do
    saves+=("$found_save")
done < <(find "$BACKUP_DIR" -name "*.tar.gz" -type f | sort -r)

if [ -n "$SAVE_ARG" ]; then
    # Non-interactive selection: exact path, exact filename, then newest match
    if [ -f "$SAVE_ARG" ]; then
        selected_save="$SAVE_ARG"
    elif [ -f "$BACKUP_DIR/$SAVE_ARG" ]; then
        selected_save="$BACKUP_DIR/$SAVE_ARG"
    else
        selected_save=$(find "$BACKUP_DIR" -name "*${SAVE_ARG}*.tar.gz" -type f -printf '%T@ %p\n' | sort -rn | head -n 1 | cut -d' ' -f2-)
    fi

    if [ -z "$selected_save" ]; then
        echo "No backup matching '$SAVE_ARG' found in $BACKUP_DIR"
        echo ""
        if [ ${#saves[@]} -eq 0 ]; then
            echo "No backup files found"
        else
            echo "Available save backups:"
            echo ""
            list_saves
        fi
        exit 1
    fi
else
    # List available saves
    echo "Available save backups:"
    echo ""

    # Display them with numbers
    if [ ${#saves[@]} -eq 0 ]; then
        echo "No backup files found"
        exit 1
    fi

    list_saves

    echo ""
    read -p "Enter the number of the save to activate (or 'q' to quit): " choice

    # Check if user wants to quit
    if [ "$choice" = "q" ] || [ "$choice" = "Q" ]; then
        echo "Cancelled"
        exit 0
    fi

    # Validate choice
    if ! [[ "$choice" =~ ^[0-9]+$ ]] || [ "$choice" -lt 1 ] || [ "$choice" -gt ${#saves[@]} ]; then
        echo "Invalid selection"
        exit 1
    fi

    # Get selected save file
    selected_save="${saves[$((choice-1))]}"
fi

echo ""
echo "Activating save: $(basename "$selected_save")"

# First, reset the current save (path guard lives there)
echo "Resetting current save..."
if ! OVERTHROW_SAVE_DIR="$SAVE_DIR" "$SCRIPT_DIR/reset_save.sh"; then
    echo "Failed to reset the current save - nothing was extracted"
    exit 1
fi

# Extract the backup
echo "Extracting backup..."
mkdir -p "$(dirname "$SAVE_DIR")"
tar -xzf "$selected_save" -C "$(dirname "$SAVE_DIR")"

if [ $? -eq 0 ]; then
    echo ""
    echo "Save activated successfully!"
else
    echo "Failed to activate save"
    exit 1
fi
