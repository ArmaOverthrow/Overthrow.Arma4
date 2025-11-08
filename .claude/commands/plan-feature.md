---
description: Plan a new feature by gathering requirements and creating an implementation plan. Usage: /plan-feature <feature-name> [initial requirements]
---

You have been asked to plan a new feature with implementation documentation.

**Feature name:** First argument from `$ARGUMENTS`
**Initial requirements:** Remaining arguments from `$ARGUMENTS` (may be empty)

## Process

### 1. Parse Arguments

Parse `$ARGUMENTS` to extract:
- **Feature name** (required): First word/phrase (use kebab-case format)
- **Initial prompt** (optional): Everything after the feature name

Examples:
- `/plan-feature user-authentication` → name: "user-authentication", prompt: empty
- `/plan-feature user-authentication Add OAuth support and 2FA` → name: "user-authentication", prompt: "Add OAuth support and 2FA"

If `$ARGUMENTS` is completely empty:
- Ask user for feature name
- Ask for initial requirements

### 2. Verify Feature Doesn't Already Exist

Check if `docs/features/<feature-name>/` already exists:
- If it exists with `implementation.md`, inform user and ask if they want to:
  - Overwrite the existing plan
  - Use `/start-feature` to work on it as-is
  - Use `/discover-feature` if they want to investigate it first
- If directory exists but no `implementation.md`, proceed with planning

### 3. Gather Requirements

**If initial prompt was provided:**
- Start with that as the base requirements
- Ask 2-3 clarifying questions to understand:
  - Key use cases
  - User personas affected (if applicable)
  - Technical constraints
  - Integration points with existing systems

**If no initial prompt:**
- Ask user: "What is this feature about? Describe what you want to build and why."
- Wait for response
- Then ask clarifying questions as above

**Use AskUserQuestion tool** for structured choices when appropriate:
- Priority level (High/Medium/Low)
- Target completion timeline
- Dependencies on other features

### 4. Create Feature Directory Structure

Create the directory structure:
```bash
mkdir -p docs/features/<feature-name>
```

### 5. Launch Solution-Architect Agent

Use the `Task` tool with `subagent_type="solution-architect"` to create the implementation plan.

**Agent prompt should include:**
```
Create an implementation plan for the following feature:

**Feature Name:** <feature-name>

**Requirements:**
<gathered requirements from steps 3>

**Context:**
- Project: [Detected from CLAUDE.md or project structure]
- Tech Stack: [Detected from project files]
- Existing Architecture: [Brief summary if relevant]

**Output:**
Create a detailed implementation plan in `/docs/features/<feature-name>/implementation.md` following this structure:

1. **Executive Summary** - Brief overview
2. **Goals** - Primary and secondary goals
3. **Architecture Overview** - High-level design
4. **Implementation Phases** - Broken down with tasks, estimates, acceptance criteria
5. **Key Technical Decisions** - Architecture decisions with rationale
6. **Testing Strategy** - How to validate the feature
7. **Dependencies** - What this feature depends on
8. **Risks & Mitigation** - Potential issues and how to address them

**Status:** Planning
**Started:** [Today's date]
**Target Completion:** [Based on user input or TBD]
**Last Updated:** [Today's date with timestamp]

Use the existing implementation plans in this project as reference for format and style.
```

**IMPORTANT:** Tell the agent to:
- Use the `Write` tool to create the implementation plan
- Be thorough but concise
- Include specific technical details relevant to this project
- Break phases into granular tasks
- Provide realistic time estimates

### 6. Verify Plan Was Created

After agent completes:
- Check that `docs/features/<feature-name>/implementation.md` exists
- Read the first 50 lines to verify it has proper structure

If plan creation failed:
- Show error to user
- Offer to retry with more context

### 7. Present Plan to User

Show a summary:
```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
✅ IMPLEMENTATION PLAN CREATED
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

📋 Feature: <feature-name>
📁 Location: docs/features/<feature-name>/implementation.md

📊 Plan Summary:
- Phases: [X phases]
- Estimated Time: [X hours/days]
- Key Technologies: [List]

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

Then display the first ~30 lines of the plan so user can see the executive summary and goals.

### 8. Wait for Approval

Ask the user:

"Please review the implementation plan at `docs/features/<feature-name>/implementation.md`.

Would you like to:
1. **Start working on it** - I'll run `/start-feature <feature-name>` to set up dev docs
2. **Request changes** - Tell me what to adjust and I'll update the plan
3. **Cancel** - Discard this plan

What would you like to do?"

**If user chooses option 1 (Start):**
- Run `/start-feature <feature-name>` using the SlashCommand tool
- Wait for it to complete
- Show success message

**If user chooses option 2 (Changes):**
- Ask what changes they want
- Use Edit tool to update the plan OR re-run solution-architect with adjusted requirements
- Show updated summary
- Ask for approval again (repeat step 8)

**If user chooses option 3 (Cancel):**
- Ask if they want to delete the plan file
- If yes, remove it
- Confirm cancellation

## Important Notes

- **DO NOT** start implementing the feature - this command only creates the plan
- **DO** involve the user in requirements gathering
- **DO** use the solution-architect agent for plan creation (don't write it yourself)
- **DO** wait for explicit user approval before running /start-feature
- **DO** make sure the plan is thorough and actionable

## Error Handling

- If feature name is invalid (special characters, spaces), suggest kebab-case alternative
- If solution-architect agent fails, offer to retry or let user create plan manually
- If /start-feature fails after approval, show error and let user fix manually
- If user cancels mid-process, clean up any partial files created

## Example Workflow

```
User: /plan-feature user-authentication Add OAuth and 2FA
Claude: I'll help plan the user-authentication feature.

Gathering requirements...
[Asks clarifying questions]

Creating implementation plan with solution-architect agent...

✅ Plan created at docs/features/user-authentication/implementation.md

Would you like to start working on it?

User: Yes, start it

Claude: Running /start-feature user-authentication...
✅ Dev docs created! Ready to implement.
```

---

**Remember:** This command orchestrates the planning process but delegates the actual plan writing to the solution-architect agent. Your role is to facilitate, gather requirements, and coordinate the workflow.
