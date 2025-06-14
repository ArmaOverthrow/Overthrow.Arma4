#!/bin/bash

# Script to reset Overthrow save data by deleting the save directory

SAVE_PATH="/mnt/c/Users/Aaron Static/OneDrive/Documents/My Games/ArmaReforgerWorkbench/profile/.db/Overthrow"

echo "Resetting Overthrow save data..."

if [ -d "$SAVE_PATH" ]; then
    echo "Deleting save directory: $SAVE_PATH"
    rm -rf "$SAVE_PATH"
    echo "Save data deleted successfully!"
else
    echo "Save directory not found: $SAVE_PATH"
    echo "Nothing to delete."
fi

echo "Done."