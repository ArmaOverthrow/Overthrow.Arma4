# Beast Mode Implementation Plan - Overthrow Mod

**Project:** Overthrow.Arma4
**Type:** Arma Reforger Game Mod (EnforceScript)
**Date:** 2025-11-08
**Status:** Installation Complete → Configuration Pending

---

## Executive Summary

Beast Mode has been installed for the Overthrow mod project. This setup is customized for:

- **Language:** EnforceScript (C++ variant for Enfusion engine)
- **Architecture:** Entity-Component system with Managers/Controllers
- **Build Process:** External (Arma Reforger Workbench)
- **Testing:** Manual play-testing
- **Team:** Solo developer
- **Focus Areas:** Component patterns, network replication, persistence

### What Was Installed Automatically

The universal templates have been copied to your project:

✅ **Slash Commands** (`.claude/commands/`)
- `start-feature.md` - Start tracking a new feature
- `continue-feature.md` - Resume work on an active feature
- `dev-docs-update.md` - Update dev docs after changes

✅ **Dev Docs Templates** (`dev/templates/`)
- `plan.md` - Feature planning template
- `context.md` - Context capture template
- `tasks.md` - Task tracking template
- `README.md` - Workflow guide

✅ **Hook Templates** (`.claude/hooks/`)
- `edit-tracker.ts` - Tracks edited files (disabled by user choice)
- `skill-reminder.ts` - Suggests relevant skills (**enabled**)
- `build-check.ts` - Reminds to test in Workbench (disabled by user choice)

✅ **Format References** (`.claude/`)
- `skill-structure-example.md` - Guide for creating skills
- `agent-structure-example.md` - Guide for creating agents
- `skill-rules.template.json` - Template for skill auto-suggestions
- `settings.hooks.json` - Template for hook registration

✅ **Directory Structure**
- `dev/active/` - Active feature dev docs
- `dev/completed/` - Archived feature dev docs
- `docs/features/dev-ops/` - This implementation plan

---

## Phase 1: Configure and Test Hooks

**⚠️ CRITICAL:** Hooks must be configured FIRST before creating skills, as the skill-reminder hook needs to know which skills exist.

### Step 1.1: Install tsx globally

TypeScript hooks require `tsx` to execute directly with the `#!/usr/bin/env tsx` shebang.

```bash
npm install -g tsx
```

**Verify installation:**
```bash
tsx --version
```

### Step 1.2: Register hooks in settings

You selected: **skill-reminder** hook only

Add this to `.claude/settings.local.json`:

```json
{
  "hooks": {
    "UserPromptSubmit": [
      {
        "matcher": "",
        "hooks": [
          {
            "type": "command",
            "command": "./.claude/hooks/skill-reminder.ts"
          }
        ]
      }
    ]
  }
}
```

**Why settings.local.json?**
- Not committed to git (personal preferences)
- Won't conflict with team settings (if you expand the team later)
- Can be easily toggled without affecting the repo

A template is available at `.claude/settings.hooks.json` with all hooks enabled. You can copy and modify it.

### Step 1.3: Make hooks executable

```bash
chmod +x .claude/hooks/*.ts
```

### Step 1.4: Test skill-reminder hook

The skill-reminder hook triggers when your prompt contains keywords that match skill rules.

**Test it:**
1. After creating skills in Phase 2, use keywords in a prompt like:
   - "I need to create a new manager component"
   - "Help me implement RPC replication"
   - "How do I persist this data with EPF?"

2. The hook should suggest relevant skills

**If it doesn't work:**
- Check `.claude/session-edits.json` was created
- Verify tsx is in PATH: `which tsx`
- Check hook permissions: `ls -la .claude/hooks/`
- Review `.claude/skills/skill-rules.json` exists and has valid patterns

---

## Phase 2: Create Project-Specific Skills

**⚠️ CRITICAL:** Skills use **progressive disclosure pattern**:
- Main file MUST be named `SKILL.md` (ALL CAPS)
- Keep SKILL.md to ~100-150 lines as quick reference
- Put detailed content in separate resource .md files
- See `.claude/skill-structure-example.md` for complete pattern

### Recommended Skills for Overthrow

Based on your EnforceScript/Arma Reforger architecture:

#### Skill 1: `enforcescript-patterns`

**Location:** `.claude/skills/enforcescript-patterns/`

**SKILL.md** (~120 lines) - Quick reference covering:
- No ternary operators allowed
- Strong refs for Managed classes
- Entity-component architecture basics
- Quick reference to resource files

**Resource files** (detailed content):
- `component-patterns.md` - Manager, Controller, and Component patterns
- `networking.md` - RplProp, RPC patterns, JIP replication
- `persistence.md` - EPF save/load patterns
- `memory-management.md` - Strong refs, garbage collection, EntityID handling
- `ui-patterns.md` - OVT_UIContext patterns, layout activation
- `common-pitfalls.md` - Ternary operators, weak refs, EntityID vs RplId

**Keyword triggers:**
- "component", "manager", "controller"
- "replication", "RPC", "network"
- "persistence", "EPF", "save"
- "strong ref", "garbage collection"

#### Skill 2: `overthrow-architecture`

**Location:** `.claude/skills/overthrow-architecture/`

**SKILL.md** (~110 lines) - Quick reference covering:
- OVT_ naming conventions
- Manager vs Controller vs Component
- OVT_Global static accessors
- Quick reference to resource files

**Resource files:**
- `managers.md` - Singleton manager pattern, GetInstance, Init, PostGameStart
- `controllers.md` - Instance controllers, registration, lifecycle
- `global-access.md` - OVT_Global patterns, GetController/GetServer/GetUI
- `file-structure.md` - Where to put Scripts, Configs, Prefabs, UI
- `coding-standards.md` - Naming conventions, member prefixes, documentation

**Keyword triggers:**
- "OVT_", "Overthrow"
- "manager", "controller"
- "singleton", "GetInstance"
- "naming convention"

#### Skill 3: `workbench-workflow`

**Location:** `.claude/skills/workbench-workflow/`

**SKILL.md** (~90 lines) - Quick reference covering:
- No automated build/test available
- How to test changes
- Common compile error patterns
- Quick reference to resource files

**Resource files:**
- `testing-guidelines.md` - Manual test procedures, what to test for different component types
- `compile-errors.md` - Common EnforceScript compile errors and fixes
- `debug-patterns.md` - Using debug prints, what to look for in logs
- `workbench-tips.md` - Working with prefabs, configs, layouts in Workbench

**Keyword triggers:**
- "test", "testing", "compile"
- "workbench", "build"
- "debug", "error"

### Creating Your First Skill

**Step-by-step example for `enforcescript-patterns`:**

1. **Create directory:**
   ```bash
   mkdir -p .claude/skills/enforcescript-patterns
   ```

2. **Create SKILL.md** (use template from `.claude/skill-structure-example.md`)

3. **Create resource files:**
   ```bash
   cd .claude/skills/enforcescript-patterns
   touch component-patterns.md networking.md persistence.md memory-management.md ui-patterns.md common-pitfalls.md
   ```

4. **Populate SKILL.md** with:
   - Title and description
   - Quick reference sections
   - Links to resource files using `resource://` protocol
   - When to use this skill

5. **Populate resource files** with detailed content extracted from CLAUDE.md

6. **Test the skill:**
   ```
   Use the Skill tool in a conversation to activate it:
   "Use the enforcescript-patterns skill"
   ```

---

## Phase 3: Create skill-rules.json for Auto-Suggestions

**⚠️ IMPORTANT:** `skill-rules.json` is for keyword matching in user prompts ONLY. The skill-reminder hook runs BEFORE edits (UserPromptSubmit hook) and matches keywords in your prompt text, NOT file patterns.

**Location:** `.claude/skills/skill-rules.json`

**Template structure:**

```json
{
  "rules": [
    {
      "skill": "enforcescript-patterns",
      "triggers": {
        "keywords": [
          "component",
          "manager",
          "controller",
          "replication",
          "RPC",
          "RplProp",
          "persistence",
          "EPF",
          "strong ref",
          "garbage collection",
          "EntityID",
          "RplId"
        ],
        "patterns": []
      },
      "priority": 10,
      "description": "EnforceScript component patterns, networking, and memory management"
    },
    {
      "skill": "overthrow-architecture",
      "triggers": {
        "keywords": [
          "OVT_",
          "Overthrow",
          "singleton",
          "GetInstance",
          "OVT_Global",
          "naming convention"
        ],
        "patterns": []
      },
      "priority": 9,
      "description": "Overthrow mod architecture patterns and conventions"
    },
    {
      "skill": "workbench-workflow",
      "triggers": {
        "keywords": [
          "test",
          "testing",
          "compile",
          "workbench",
          "build",
          "debug",
          "error"
        ],
        "patterns": []
      },
      "priority": 5,
      "description": "Arma Reforger Workbench workflow and testing guidelines"
    }
  ]
}
```

**Testing:**
1. Save `skill-rules.json`
2. Use keywords in a prompt: "I need to create a new manager component with RPC replication"
3. The skill-reminder hook should suggest `enforcescript-patterns` and `overthrow-architecture`

---

## Phase 4: Create Project-Specific Agents

**⚠️ IMPORTANT:**
- Agents are Markdown files with YAML frontmatter
- Official docs: https://docs.claude.com/en/docs/claude-code/sub-agents
- Template: See `.claude/agent-structure-example.md`
- Agents **reference** skills (DON'T duplicate content)

### Recommended Agents for Overthrow

#### Agent 1: `solution-architect`

**Location:** `.claude/agents/solution-architect.md`

**Purpose:** Plan features, design component architecture, make high-level decisions

**Frontmatter:**
```yaml
---
name: solution-architect
description: Plans features and designs component architecture for Overthrow mod
model: sonnet
tools: [Read, Glob, Grep, Task]
---
```

**Content:**
- Reference `overthrow-architecture` skill
- Reference `enforcescript-patterns` skill
- Focus on: Planning component hierarchies, choosing Manager vs Controller patterns
- Decision criteria: When to create new managers, how to split responsibilities
- Output: Architecture diagrams, component lists, dependency maps

#### Agent 2: `component-developer`

**Location:** `.claude/agents/component-developer.md`

**Purpose:** Implement components following Overthrow patterns

**Frontmatter:**
```yaml
---
name: component-developer
description: Implements EnforceScript components following Overthrow patterns
model: sonnet
tools: [Read, Write, Edit, Grep, Glob]
---
```

**Content:**
- Reference `enforcescript-patterns` skill
- Reference `overthrow-architecture` skill
- Focus on: Writing Manager/Controller components, implementing RPC/replication
- Patterns to follow: Naming conventions, strong refs, network optimization
- Output: Component code, integration points, test procedures

#### Agent 3: `network-specialist`

**Location:** `.claude/agents/network-specialist.md`

**Purpose:** Implement network replication, RPC calls, JIP handling

**Frontmatter:**
```yaml
---
name: network-specialist
description: Implements network replication and RPC patterns for multiplayer
model: sonnet
tools: [Read, Write, Edit, Grep]
---
```

**Content:**
- Reference `enforcescript-patterns` skill (networking.md resource)
- Focus on: RplProp attributes, RPC patterns, JIP replication, optimization
- Decision criteria: When to use RplProp vs RPC, broadcast vs owner
- Common issues: RplId vs EntityID, replication flooding, JIP state sync
- Output: Network code, replication tests, performance notes

### Creating Your First Agent

**Step-by-step example for `solution-architect`:**

1. **Create file:**
   ```bash
   touch .claude/agents/solution-architect.md
   ```

2. **Add frontmatter:**
   ```yaml
   ---
   name: solution-architect
   description: Plans features and designs component architecture for Overthrow mod
   model: sonnet
   tools: [Read, Glob, Grep, Task]
   ---
   ```

3. **Add instructions:**
   ```markdown
   # Solution Architect Agent

   You are the solution architect for the Overthrow mod project.

   ## Skills Available
   Activate these skills to access detailed patterns:
   - enforcescript-patterns
   - overthrow-architecture

   ## Your Role
   - Plan feature implementations
   - Design component architectures
   - Make high-level technical decisions
   - Recommend Manager vs Controller patterns

   [... rest of instructions ...]
   ```

4. **Test the agent:**
   ```
   Launch the agent using the Task tool:
   "Launch solution-architect agent to plan a new faction reputation system"
   ```

---

## Phase 5: Test Complete Workflow

Now that everything is set up, test the full Beast Mode workflow with a small feature.

### Test Feature: "dev-ops"

This very implementation is tracked as a feature! Let's use it to test:

1. **Start the feature:**
   ```
   /start-feature dev-ops
   ```
   This creates `dev/active/dev-ops/` with plan.md, context.md, tasks.md

2. **Verify hook works:**
   - Type a prompt with keywords: "Create a manager component"
   - skill-reminder should suggest `enforcescript-patterns`

3. **Test skill activation:**
   ```
   Activate the enforcescript-patterns skill
   ```
   Should expand the skill content

4. **Test agent:**
   ```
   Launch solution-architect agent to review the Beast Mode setup
   ```

5. **Update dev docs:**
   ```
   /dev-docs-update
   ```
   Should prompt you to update plan/context/tasks

6. **Complete the feature:**
   When dev-ops setup is done, move it to completed:
   ```bash
   mv dev/active/dev-ops dev/completed/
   ```

---

## Phase 6: Documentation & Onboarding

### Update Project Documentation

Add Beast Mode reference to `CLAUDE.md`:

```markdown
## Beast Mode Workflow

This project uses the Beast Mode workflow system for feature development.

### Quick Start

1. **Start a feature:** `/start-feature <name>`
2. **Resume work:** `/continue-feature`
3. **Update docs:** `/dev-docs-update`

### Skills Available

- `enforcescript-patterns` - Component patterns, networking, persistence
- `overthrow-architecture` - OVT architecture, naming conventions
- `workbench-workflow` - Testing and debugging guidelines

### Agents Available

- `solution-architect` - Feature planning and architecture design
- `component-developer` - Component implementation
- `network-specialist` - Replication and RPC patterns

See `dev/README.md` for complete workflow documentation.
```

### Optional: Create Team Documentation

If you expand the team later, create `dev/ONBOARDING.md`:

```markdown
# Beast Mode Onboarding

Welcome to the Overthrow development workflow!

## Setup

1. Install tsx: `npm install -g tsx`
2. Copy `.claude/settings.hooks.json` to `.claude/settings.local.json`
3. Review available skills in `.claude/skills/`
4. Review available agents in `.claude/agents/`

## Daily Workflow

1. Start your feature: `/start-feature <name>`
2. Work on tasks, Claude tracks context automatically
3. Update dev docs: `/dev-docs-update`
4. Test in Workbench
5. Complete feature: move to `dev/completed/`

## Best Practices

- Always test changes in Workbench
- Use agents for complex planning
- Activate skills when working on specific patterns
- Keep dev docs updated
```

---

## Next Steps Checklist

Use this checklist to track your implementation progress:

- [ ] **Phase 1: Configure Hooks**
  - [ ] Install tsx globally
  - [ ] Register skill-reminder hook in settings.local.json
  - [ ] Make hooks executable
  - [ ] Test skill-reminder (after Phase 3)

- [ ] **Phase 2: Create Skills**
  - [ ] Create `enforcescript-patterns` skill + 6 resources
  - [ ] Create `overthrow-architecture` skill + 5 resources
  - [ ] Create `workbench-workflow` skill + 4 resources
  - [ ] Test each skill activates correctly

- [ ] **Phase 3: Create skill-rules.json**
  - [ ] Create `.claude/skills/skill-rules.json`
  - [ ] Add rules for all 3 skills
  - [ ] Test keyword matching

- [ ] **Phase 4: Create Agents**
  - [ ] Create `solution-architect` agent
  - [ ] Create `component-developer` agent
  - [ ] Create `network-specialist` agent
  - [ ] Test each agent launches

- [ ] **Phase 5: Test Workflow**
  - [ ] Run `/start-feature dev-ops`
  - [ ] Verify hooks work
  - [ ] Test skill activation
  - [ ] Test agent launch
  - [ ] Run `/dev-docs-update`
  - [ ] Complete feature (move to dev/completed/)

- [ ] **Phase 6: Documentation**
  - [ ] Update CLAUDE.md with Beast Mode reference
  - [ ] (Optional) Create dev/ONBOARDING.md

---

## Troubleshooting

### Hook doesn't trigger

**Problem:** skill-reminder hook doesn't suggest skills

**Solutions:**
1. Verify tsx is installed: `tsx --version`
2. Check hook is executable: `ls -la .claude/hooks/skill-reminder.ts`
3. Verify settings.local.json has correct hook registration
4. Check skill-rules.json exists and has valid JSON
5. Try using exact keyword from skill-rules.json in prompt

### Skill doesn't activate

**Problem:** Skill tool doesn't find the skill

**Solutions:**
1. Verify SKILL.md exists (ALL CAPS): `ls .claude/skills/*/SKILL.md`
2. Check file has correct frontmatter with `name` field
3. Restart Claude Code session to reload skills

### Agent doesn't launch

**Problem:** Task tool doesn't find the agent

**Solutions:**
1. Verify agent file exists: `ls .claude/agents/*.md`
2. Check frontmatter has `name` and `description` fields
3. Verify agent name matches filename (without .md)
4. Restart Claude Code session to reload agents

### Build check hook fails

**Problem:** build-check hook shows error (if you enable it later)

**Solutions:**
1. Edit `.claude/hooks/build-check.ts`
2. Update `BUILD_COMMAND` to match your needs
3. For Workbench workflow, change to just show reminder instead of running command

---

## Maintenance

### Adding New Skills

1. Create new directory in `.claude/skills/<skill-name>/`
2. Create `SKILL.md` (ALL CAPS) with frontmatter
3. Create resource .md files for detailed content
4. Add rule to `skill-rules.json`
5. Test keyword matching

### Adding New Agents

1. Create `.claude/agents/<agent-name>.md`
2. Add YAML frontmatter with name, description, model, tools
3. Reference existing skills in agent instructions
4. Test agent launch with Task tool

### Updating Hooks

1. Edit `.claude/hooks/<hook-name>.ts`
2. Make hooks executable: `chmod +x .claude/hooks/*.ts`
3. Restart Claude Code session to reload hooks

---

## Success Metrics

You'll know Beast Mode is working when:

✅ **Hooks work:**
- skill-reminder suggests relevant skills based on your prompts
- Suggestions match your actual workflow needs

✅ **Skills are useful:**
- You activate skills when working on specific patterns
- Skills provide quick access to detailed reference material
- Resource files contain the right level of detail

✅ **Agents are effective:**
- solution-architect plans features with proper architecture
- component-developer implements following Overthrow patterns
- Agents reference skills instead of duplicating content

✅ **Dev docs track context:**
- You can resume work on features after breaks
- Context files capture important decisions
- Tasks track progress through implementation

✅ **Workflow feels natural:**
- You use `/start-feature` for new work
- `/continue-feature` helps you resume
- `/dev-docs-update` captures progress
- The system helps instead of getting in the way

---

## Support

For questions or issues:

1. Review this implementation plan
2. Check `.claude/skill-structure-example.md` for skill patterns
3. Check `.claude/agent-structure-example.md` for agent patterns
4. Review `dev/README.md` for workflow guide
5. Reference original Beast Mode post for concepts

**Remember:** Beast Mode is a workflow system. Customize it to fit your needs. If something doesn't work for your Overthrow development process, adjust it!

---

**Implementation Status:** Ready to start Phase 1

**Next Command:** `/start-feature dev-ops`
