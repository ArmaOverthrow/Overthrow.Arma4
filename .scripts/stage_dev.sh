#!/bin/bash

# stage_dev.sh - Stage development build
# This script prepares the development directory by clearing it (except addon.gproj) 
# and copying the latest changes from the main project

set -e  # Exit on any error

SOURCE_DIR="/mnt/n/Projects/Arma 4/Overthrow.Arma4"
DEST_DIR="/mnt/n/Projects/Arma 4/Overthrow.Dev"

echo "Staging development build..."

# Check if source directory exists
if [ ! -d "$SOURCE_DIR" ]; then
    echo "Error: Source directory '$SOURCE_DIR' does not exist!"
    exit 1
fi

# Check if destination directory exists
if [ ! -d "$DEST_DIR" ]; then
    echo "Error: Destination directory '$DEST_DIR' does not exist!"
    exit 1
fi

# Backup addon.gproj if it exists
ADDON_GPROJ="$DEST_DIR/addon.gproj"
TEMP_ADDON="/tmp/addon.gproj.backup"

if [ -f "$ADDON_GPROJ" ]; then
    echo "Backing up addon.gproj..."
    cp "$ADDON_GPROJ" "$TEMP_ADDON"
    ADDON_EXISTS=true
else
    echo "No addon.gproj found in destination"
    ADDON_EXISTS=false
fi

# Clear destination directory (except addon.gproj which we backed up)
echo "Clearing destination directory..."
find "$DEST_DIR" -mindepth 1 -delete

# Restore addon.gproj if it existed
if [ "$ADDON_EXISTS" = true ]; then
    echo "Restoring addon.gproj..."
    cp "$TEMP_ADDON" "$ADDON_GPROJ"
    rm "$TEMP_ADDON"
fi

# Copy files from source to destination, excluding:
# - Files/folders starting with "." (hidden files, .git, .scripts, etc.)
# - The "docs" folder
# - addon.gproj (we already have it in destination)
echo "Copying files from source to destination..."

cd "$SOURCE_DIR"
find . -mindepth 1 \
    -not -path './.*' \
    -not -path './docs' \
    -not -path './docs/*' \
    -not -name 'addon.gproj' \
    -print0 | rsync -a --files-from=- --from0 . "$DEST_DIR/"

echo "Development build staging complete!"
echo "Copied from: $SOURCE_DIR"
echo "Copied to: $DEST_DIR"
echo "Excluded: hidden files/folders (.*), docs folder, addon.gproj"