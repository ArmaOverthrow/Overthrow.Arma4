#!/bin/bash

# Path to the save directory - use environment variable or default
SAVE_DIR="${OVERTHROW_SAVE_DIR:-/mnt/c/Users/Aaron Static/OneDrive/Documents/My Games/ArmaReforgerWorkbench/profile/.db/Overthrow}"

# Create .saves directory if it doesn't exist
BACKUP_DIR="$(dirname "$0")/../.saves"
mkdir -p "$BACKUP_DIR"

# Check if save directory exists
if [ ! -d "$SAVE_DIR" ]; then
    echo "Save directory not found: $SAVE_DIR"
    exit 1
fi

# Ask for backup name
read -p "Enter a name for this save backup: " SAVE_NAME

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