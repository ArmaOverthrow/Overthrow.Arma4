#!/usr/bin/env node
/**
 * Edit Tracker Hook
 * Tracks which files were edited during the session
 * Hook type: PostToolUse (Edit, Write, MultiEdit)
 */
import { readFileSync, writeFileSync, existsSync } from 'fs';
import { join, dirname } from 'path';
import { fileURLToPath } from 'url';
const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);
const TRACK_FILE = join(__dirname, '../session-edits.json');
async function main() {
    try {
        // Read JSON input from stdin
        const input = await readStdin();
        const hookInput = JSON.parse(input);
        const toolName = hookInput.tool_name || 'unknown';
        const filePath = hookInput.tool_input?.file_path || hookInput.tool_input?.path;
        // Skip if no file path
        if (!filePath || filePath === 'unknown') {
            process.exit(0);
        }
        // Load or initialize tracker
        let tracker;
        if (existsSync(TRACK_FILE)) {
            tracker = JSON.parse(readFileSync(TRACK_FILE, 'utf-8'));
        }
        else {
            tracker = {
                sessionId: new Date().toISOString().replace(/[:.]/g, '-').split('T')[0],
                edits: [],
            };
        }
        // Add new edit entry
        tracker.edits.push({
            file: filePath,
            tool: toolName,
            timestamp: new Date().toISOString(),
        });
        // Save tracker
        writeFileSync(TRACK_FILE, JSON.stringify(tracker, null, 2));
        // Silent exit - this hook just tracks
        process.exit(0);
    }
    catch (error) {
        // Silent failure - don't interrupt workflow
        process.exit(0);
    }
}
function readStdin() {
    return new Promise((resolve) => {
        let data = '';
        process.stdin.on('data', (chunk) => {
            data += chunk;
        });
        process.stdin.on('end', () => {
            resolve(data);
        });
    });
}
main();
