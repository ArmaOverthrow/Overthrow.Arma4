#!/bin/bash

# Path to the save directory - use environment variable or default
SAVE_DIR="${OVERTHROW_SAVE_DIR:-/mnt/c/Users/Aaron Static/OneDrive/Documents/My Games/ArmaReforgerWorkbench/profile/.db/Overthrow}"

# Script directory and saves location
SCRIPT_DIR="$(dirname "$0")"
BACKUP_DIR="$SCRIPT_DIR/../.saves"

# Check if .saves directory exists
if [ ! -d "$BACKUP_DIR" ]; then
    echo "No saved backups found in $BACKUP_DIR"
    exit 1
fi

# List available saves
echo "Available save backups:"
echo ""

# Find all tar.gz files and display them with numbers
saves=($(find "$BACKUP_DIR" -name "*.tar.gz" -type f | sort -r))

if [ ${#saves[@]} -eq 0 ]; then
    echo "No backup files found"
    exit 1
fi

for i in "${!saves[@]}"; do
    filename=$(basename "${saves[$i]}")
    size=$(du -h "${saves[$i]}" | cut -f1)
    echo "[$((i+1))] $filename ($size)"
done

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
echo ""
echo "Activating save: $(basename "$selected_save")"

# First, reset the current save
echo "Resetting current save..."
"$SCRIPT_DIR/reset_save.sh"

# Extract the backup
echo "Extracting backup..."
tar -xzf "$selected_save" -C "$(dirname "$SAVE_DIR")"

if [ $? -eq 0 ]; then
    echo ""
    echo "Save activated successfully!"
else
    echo "Failed to activate save"
    exit 1
fi