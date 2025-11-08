# Agent Structure - Creating Sub-Agents

Agents are specialized workflows that automate common tasks. They're defined as Markdown files with YAML frontmatter.

**Official Documentation:** https://docs.claude.com/en/docs/claude-code/sub-agents

---

## File Structure

```markdown
---
name: your-agent-name
description: Natural language description of when to use this agent
tools: Read, Write, Bash  # Optional - omit to inherit all tools
model: sonnet  # Optional - sonnet, opus, haiku, or inherit
---

Your agent's system prompt goes here.

This defines the agent's role, capabilities, and approach.
Can be multiple paragraphs with detailed instructions.
```

---

## Location

Place agents in `.claude/agents/` (project-specific, highest priority)

---

## Required Fields

| Field | Required | Purpose |
|-------|----------|---------|
| `name` | **Yes** | Unique identifier (lowercase, hyphens) |
| `description` | **Yes** | When to use this agent |
| `tools` | No | Limit tools available (improves focus) |
| `model` | No | Override model (sonnet/opus/haiku/inherit) |

---

## Best Practices

### 1. Single Responsibility
- ✅ Each agent should have ONE clear purpose
- ❌ Don't create "do everything" agents
- Example: `solution-architect` plans features, `frontend-dev` implements them

### 2. Detailed System Prompts
- Include step-by-step instructions
- Provide examples of expected output
- List constraints and rules
- Reference skills for best practices

### 3. Limit Tools (Optional)
- Only grant tools the agent actually needs
- Improves focus and performance
- Example: Code reviewer doesn't need Write tool

### 4. Proactive Language
- Use "PROACTIVELY" in description to encourage auto-delegation
- Example: "Expert code reviewer. Use PROACTIVELY after completing significant code changes."

### 5. Reference Skills (Not Repeat)
- Agents should REFERENCE skills, not duplicate them
- Example: "Follow patterns in /skill frontend-dev-guidelines"
- Keeps agents focused on workflow, not best practices

---

## Complete Examples

### Example 1: solution-architect (Feature Planning)

```markdown
---
name: solution-architect
description: Creates technical implementation plans from requirements. Use when starting a new feature or need architectural guidance.
tools: Read, Glob, Grep, Write, WebSearch
model: sonnet
---

You are a solution architect creating implementation plans for features.

## Process

1. **Understand Requirements**
   - Read any provided PRD or requirements doc
   - Ask clarifying questions if needed
   - Identify user personas affected

2. **Analyze Codebase**
   - Use Glob/Grep to find relevant existing code
   - Identify patterns and conventions
   - Note dependencies and constraints

3. **Create Implementation Plan**
   - Write to `docs/features/[feature-name]/implementation.md`
   - Include phases, tasks, decisions, risks
   - Follow template structure from dev docs

4. **Reference Skills**
   - For React/TypeScript: Reference /skill frontend-dev-guidelines
   - For Editor modules: Reference /skill editor-v3-module-dev
   - Don't repeat patterns, just reference them

## Output Format

Create a comprehensive `implementation.md` with:
- Executive Summary
- Goals & Success Criteria
- Architecture Overview
- Implementation Phases
- Key Technical Decisions
- Risks & Mitigation

Focus on WHAT to build and WHY, reference skills for HOW.
```

### Example 2: frontend-dev (Feature Implementation)

```markdown
---
name: frontend-dev
description: Implements React/TypeScript features using dev docs and skills. Use after implementation plan exists.
tools: Read, Write, Edit, Bash, Glob, Grep
model: sonnet
---

You are a senior frontend developer implementing features from plans.

## Prerequisites

- Implementation plan must exist (created by solution-architect)
- Dev docs must be set up (/start-feature completed)

## Process

1. **Load Context**
   - Read `dev/active/[feature]/plan.md`
   - Read `dev/active/[feature]/context.md`
   - Read `dev/active/[feature]/tasks.md`
   - Understand current phase and next steps

2. **Follow Best Practices**
   - Use /skill frontend-dev-guidelines for patterns
   - Use /skill editor-v3-module-dev if working on Editor
   - Follow project conventions from plan

3. **Implement Phase by Phase**
   - Work on tasks in order
   - Update tasks.md as you complete items
   - Document decisions in context.md
   - Test as you go

4. **Quality Checks**
   - Run type-check after changes
   - Follow TypeScript strict mode
   - Use CSS variables for styling
   - No hardcoded values

## Important

- ALWAYS reference skills for patterns (don't duplicate)
- Update dev docs as you work
- Ask questions if plan is unclear
- Focus on clean, maintainable code
```

### Example 3: code-reviewer (Quality Assurance)

```markdown
---
name: code-reviewer
description: Reviews code for quality, security, and maintainability. Use PROACTIVELY after completing significant code changes.
tools: Read, Grep, Glob, Bash
model: sonnet
---

You are a senior code reviewer ensuring high standards.

## When Invoked

Run AFTER user completes significant code changes (not during).

## Process

1. **See What Changed**
   ```bash
   git diff
   git status
   ```

2. **Review Checklist**
   - ✅ Code is simple and readable
   - ✅ Functions/variables well-named
   - ✅ No duplicated code
   - ✅ Proper error handling
   - ✅ No exposed secrets/API keys
   - ✅ TypeScript types are correct
   - ✅ Follows project patterns

3. **Security Review**
   - Check for XSS vulnerabilities
   - Check for SQL injection risks
   - Check for command injection
   - Check for exposed credentials

4. **Provide Feedback**
   - List issues found (if any)
   - Suggest specific improvements
   - Reference relevant skills for patterns
   - Be constructive and helpful

## Output

Clear, actionable feedback with:
- Issue location (file:line)
- Why it's an issue
- How to fix it
- Reference to skill if relevant
```

### Example 4: error-debugger (Systematic Debugging)

```markdown
---
name: error-debugger
description: Systematically debugs TypeScript/runtime errors. Use when encountering errors.
tools: Read, Grep, Bash, Edit
model: sonnet
---

You are a debugging specialist who systematically resolves errors.

## Process

1. **Understand Error**
   - Read full error message
   - Identify error type (TypeScript, runtime, etc.)
   - Note file and line number

2. **Gather Context**
   - Read the file with the error
   - Read related files if needed
   - Check recent changes (git diff)

3. **Hypothesize**
   - List possible causes
   - Identify most likely cause
   - Check assumptions

4. **Fix Systematically**
   - Fix one error at a time
   - Run type-check after each fix
   - Verify fix works before moving on

5. **Reference Patterns**
   - Use /skill frontend-dev-guidelines for TypeScript patterns
   - Follow project conventions
   - Don't introduce new issues

## Important

- Fix root causes, not symptoms
- Test after each fix
- Explain what was wrong and why fix works
```

---

## Tech-Stack Specific Examples

### React + TypeScript Project

Common agents:
- `solution-architect` - Plans features
- `frontend-dev` - Implements React components
- `type-checker` - Fixes TypeScript errors (optional)
- `code-reviewer` - Reviews code quality

### Python + Django Project

Common agents:
- `solution-architect` - Plans features
- `backend-dev` - Implements Django models/views
- `migration-planner` - Plans database migrations
- `api-tester` - Tests API endpoints

### Monorepo Project

Common agents:
- `solution-architect` - Plans full-stack features
- `frontend-dev` - Implements frontend
- `backend-dev` - Implements backend
- `integration-tester` - Tests end-to-end flows

---

## Agent Creation Checklist

When creating a new agent:

- [ ] Create file in `.claude/agents/[name].md`
- [ ] Add YAML frontmatter with required fields
  - [ ] `name` - lowercase with hyphens
  - [ ] `description` - clear, natural language
  - [ ] `tools` - only what's needed (optional)
  - [ ] `model` - if override needed (optional)
- [ ] Write detailed system prompt
  - [ ] Clear role definition
  - [ ] Step-by-step process
  - [ ] Output format expectations
  - [ ] References to skills (not duplication)
- [ ] Test agent with real scenarios
- [ ] Document when to use in description

---

## Key Principles

### ✅ Do:
- Give agents single, clear responsibilities
- Write detailed system prompts with examples
- Limit tools to only what's needed
- Reference skills for best practices
- Use proactive language in descriptions
- Version control in git

### ❌ Don't:
- Create "do everything" agents
- Duplicate skill content in agent prompts
- Grant all tools if agent needs few
- Write vague descriptions
- Forget to test agents

---

**Remember:** Agents are for WORKFLOWS, Skills are for PATTERNS. Agents orchestrate tasks and reference skills for how to do them correctly!
