#!/usr/bin/env tsx
/**
 * Build Check Hook (Stop)
 *
 * Runs after Claude's response to catch build/type errors before continuing.
 * This helps maintain code quality by catching issues immediately.
 *
 * CUSTOMIZATION REQUIRED:
 * - Update BUILD_COMMAND with your project's check command
 * - Update COMMAND_NAME for display purposes
 */

import { fileURLToPath } from 'url';
import { dirname, join } from 'path';
import { execSync } from 'child_process';
import { readFileSync } from 'fs';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);

// TODO: Customize this command for your project
// Examples:
//   - 'npm run type-check' (TypeScript)
//   - 'npm run lint' (ESLint)
//   - 'npm test' (Run tests)
//   - 'python manage.py check' (Django)
//   - 'cargo check' (Rust)
const BUILD_COMMAND = 'npm run type-check'; // ← CUSTOMIZE THIS

// TODO: Customize the display name
const COMMAND_NAME = 'type-check'; // ← CUSTOMIZE THIS

const SESSION_EDITS_FILE = join(__dirname, '..', 'session-edits.json');

interface SessionEdits {
  files: string[];
}

function getEditedFiles(): string[] {
  try {
    const data = readFileSync(SESSION_EDITS_FILE, 'utf-8');
    const session: SessionEdits = JSON.parse(data);
    return session.files || [];
  } catch {
    return [];
  }
}

function runBuildCheck(): void {
  const editedFiles = getEditedFiles();

  // Only run if files were edited
  if (editedFiles.length === 0) {
    return;
  }

  console.log(`\n${'━'.repeat(60)}`);
  console.log(`🔍 Running ${COMMAND_NAME}...`);
  console.log(`${'━'.repeat(60)}\n`);

  try {
    execSync(BUILD_COMMAND, {
      cwd: process.cwd(),
      stdio: 'inherit',
      encoding: 'utf-8'
    });

    console.log(`\n${'━'.repeat(60)}`);
    console.log(`✅ ${COMMAND_NAME} passed!`);
    console.log(`${'━'.repeat(60)}\n`);
  } catch (error) {
    console.log(`\n${'━'.repeat(60)}`);
    console.log(`❌ ${COMMAND_NAME} failed! Please fix errors above.`);
    console.log(`${'━'.repeat(60)}\n`);
  }
}

runBuildCheck();
