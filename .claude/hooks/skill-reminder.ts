#!/usr/bin/env tsx
/**
 * Skill Reminder Hook
 * Suggests relevant skills based on user prompt keywords
 * Hook type: UserPromptSubmit
 */

import { readFileSync, existsSync } from 'fs';
import { join, dirname } from 'path';
import { fileURLToPath } from 'url';

const __filename = fileURLToPath(import.meta.url);
const __dirname = dirname(__filename);

interface HookInput {
  prompt?: string;
}

interface SkillConfig {
  type: string;
  enforcement: string;
  priority: string;
  promptTriggers: {
    keywords: string[];
    intentPatterns: string[];
  };
  fileTriggers?: {
    pathPatterns: string[];
    contentPatterns: string[];
  };
}

interface SkillRules {
  skills: {
    [skillName: string]: SkillConfig;
  };
}

const SKILL_RULES_FILE = join(__dirname, '../skills/skill-rules.json');

const SKILL_DESCRIPTIONS: Record<string, { icon: string; title: string; description: string }> = {
  'enforcescript-patterns': {
    icon: '⚙️',
    title: 'enforcescript-patterns',
    description: 'EnforceScript component patterns, networking, persistence, and memory management',
  },
  'overthrow-architecture': {
    icon: '🏗️',
    title: 'overthrow-architecture',
    description: 'Overthrow mod architecture patterns, naming conventions, and project structure',
  },
  'workbench-workflow': {
    icon: '🔨',
    title: 'workbench-workflow',
    description: 'Arma Reforger Workbench workflow, testing guidelines, and debugging patterns',
  },
};

async function main() {
  try {
    // Read user prompt from stdin
    const input = await readStdin();
    const hookInput: HookInput = JSON.parse(input);
    const prompt = hookInput.prompt?.toLowerCase() || '';

    if (!prompt) {
      process.exit(0);
    }

    // Load skill rules
    if (!existsSync(SKILL_RULES_FILE)) {
      process.exit(0);
    }

    const skillRules: SkillRules = JSON.parse(readFileSync(SKILL_RULES_FILE, 'utf-8'));
    const relevantSkills: string[] = [];

    // Check each skill for relevance
    for (const [skillName, config] of Object.entries(skillRules.skills)) {
      // Check keywords
      const hasKeyword = config.promptTriggers.keywords.some((keyword) =>
        prompt.includes(keyword.toLowerCase())
      );

      // Check intent patterns
      const matchesIntent = config.promptTriggers.intentPatterns.some((pattern) => {
        try {
          const regex = new RegExp(pattern, 'i');
          return regex.test(prompt);
        } catch {
          return false;
        }
      });

      if (hasKeyword || matchesIntent) {
        relevantSkills.push(skillName);
      }
    }

    // Only show reminder if relevant skills found
    if (relevantSkills.length === 0) {
      process.exit(0);
    }

    // Output skill reminder
    console.log('');
    console.log('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━');
    console.log('🎯 RELEVANT SKILLS DETECTED');
    console.log('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━');
    console.log('');

    for (const skillName of relevantSkills) {
      const info = SKILL_DESCRIPTIONS[skillName];
      if (info) {
        console.log(`${info.icon} ${info.title}`);
        console.log(`   ${info.description}`);
        console.log(`   Use: /skill ${skillName}`);
        console.log('');
      }
    }

    console.log('━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━');
    console.log('');

    process.exit(0);
  } catch (error) {
    // Silent failure - don't interrupt workflow
    process.exit(0);
  }
}

function readStdin(): Promise<string> {
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
