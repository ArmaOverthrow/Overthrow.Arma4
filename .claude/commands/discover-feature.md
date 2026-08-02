---
description: "Discover and document an existing feature in the codebase, creating implementation docs for legacy code. Usage: /discover-feature <feature-name> [description]"
---

You have been asked to discover and document an existing feature in the codebase.

**Feature name:** First argument from `$ARGUMENTS`
**Initial description:** Remaining arguments from `$ARGUMENTS` (may be empty)

## Epic awareness

This command is epic-aware. Before resolving the feature name to a path,
read `.claude/epic-resolution.md` and apply its rules: detect epics by
`epic-overview.md`, resolve `<epic>/<feature>` references, and fuzzy-fall-back into
epic folders for bare names.
If the feature name is given in `<epic>/<feature>` slash form, discovery targets the nested path — all `docs/features/<feature-name>/...` reads and writes below become `docs/features/<epic>/<feature>/...`, and load the epic's `epic-overview.md` so the retrospective fits the epic.
If no epics exist in `docs/features/`, behave exactly as before.

`.claude/epic-resolution.md` is the single source of truth for epic detection and resolution. This command's body is the discovery workflow; it does **not** re-specify those rules.

## Process

### 1. Parse Arguments

Parse `$ARGUMENTS` to extract:
- **Feature name** (required): First word/phrase (use kebab-case format)
  - If the feature name is in `<epic>/<feature>` slash form (or a bare name that resolves into an epic per the Epic awareness rules), use the nested `docs/features/<epic>/<feature>/` location for the existence check in step 2 and all file creation in step 7.
- **Initial description** (optional): Everything after the feature name

Examples:
- `/discover-feature layer-inspector` → name: "layer-inspector", description: empty
- `/discover-feature layer-inspector The tree view in the editor sidebar` → name: "layer-inspector", description: "The tree view in the editor sidebar"

If `$ARGUMENTS` is completely empty:
- Ask user for feature name
- Ask for brief description of what the feature does

### 2. Check if Implementation Plan Already Exists

**CRITICAL CHECK:** Look for existing implementation plan at `docs/features/<feature-name>/implementation.md`

**If implementation.md EXISTS:**
- Read it to verify it's a proper implementation plan (not just placeholder)
- Show summary to user:
  ```
  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  📋 EXISTING PLAN FOUND
  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

  Feature: <feature-name>
  Location: docs/features/<feature-name>/implementation.md
  Status: [Read from file]

  [Show first 20-30 lines of plan]

  ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  ```
- Ask user: "An implementation plan already exists. Would you like to:"
  - **Option 1:** Start working on it → Run `/start-feature <feature-name>`
  - **Option 2:** Review what's been done → Investigate code and compare to plan
  - **Option 3:** Replace it → Continue with discovery process to create new plan
  - **Option 4:** Cancel → Do nothing

**If user chooses Option 1 (Start):**
- Run `/start-feature <feature-name>` using SlashCommand tool
- Check if `docs/features/<feature-name>/context.md` and `docs/features/<feature-name>/tasks.md` already exist
- If exist, investigate what work has been done:
  - Read context.md and tasks.md to see progress
  - Show summary of completed vs remaining work
  - Ask if they want to continue from where it left off

**If user chooses Option 2 (Review):**
- Proceed to step 3 (Investigation) but focus on comparing actual implementation to plan
- Highlight what matches, what's different, what's missing

**If user chooses Option 3 (Replace):**
- Warn user that existing plan will be overwritten
- Ask for confirmation
- Proceed to step 3

**If user chooses Option 4 (Cancel):**
- Exit command

**If NO implementation.md exists:**
- Proceed to step 3

### 3. Gather Context from User

Ask the user to guide you to the feature:

"I'll help document the <feature-name> feature. To understand how it works, please tell me:

1. **Where is the code?** (file paths, directories, or patterns like `src/apps/Editor/modules/LayerInspector/**`)
2. **What does it do?** (user-facing functionality)
3. **How is it used?** (user workflow or API usage)
4. **Any known issues or improvement areas?**

You can provide as much or as little detail as you'd like - I'll investigate the code to fill in the gaps."

Wait for user response.

### 4. Investigate the Codebase

Based on user guidance, use the **Task tool with subagent_type="Explore"** to investigate:

**Agent prompt should include:**
```
Investigate the <feature-name> feature in this codebase.

**User Guidance:**
<user's description and file paths>

**Your Task:**
1. Find all files related to this feature
2. Understand the architecture and how components interact
3. Identify key technical decisions and patterns used
4. Note dependencies and integration points
5. Document the current implementation approach
6. Identify potential improvements or technical debt

**Focus Areas:**
- Main components/modules
- Data flow and state management
- API integrations
- UI components (if applicable)
- Testing coverage
- Performance considerations

Provide a thorough analysis that can be used to create implementation documentation.
```

**Set thoroughness to "very thorough"** for comprehensive analysis.

### 5. Synthesize Findings

After the Explore agent completes, synthesize the findings into a structured summary:

**Show user:**
```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
🔍 DISCOVERY COMPLETE
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Feature: <feature-name>

📁 Key Files Found:
- [List main files]
- [List supporting files]

🏗️ Architecture:
[Brief summary of how it's structured]

⚙️ How It Works:
[Brief summary of functionality]

📊 Technical Approach:
[Key patterns and decisions used]

🔗 Dependencies:
[What it depends on]

💡 Improvement Opportunities:
[Potential enhancements or issues noted]

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

### 6. Ask Clarifying Questions

Based on the investigation, ask user 2-4 clarifying questions:
- Confirm understanding of purpose
- Clarify ambiguous technical decisions
- Understand historical context (why built this way)
- Identify what should be documented vs what's working as-is

Use **AskUserQuestion** for structured choices where appropriate.

### 7. Create Feature Documentation

Create `docs/features/<feature-name>/` directory structure:
```bash
mkdir -p docs/features/<feature-name>
```

#### 7a. Create Retrospective Implementation Plan

Create a **retrospective implementation plan** - what the plan might have looked like if this feature was planned using Beast Mode.

**Use Write tool** to create `docs/features/<feature-name>/implementation.md` with this structure:

```markdown
# <Feature Name> - Implementation Plan (Retrospective)

**Status:** Implemented (Documented Retrospectively)
**Originally Implemented:** [Approximate date if known, or "Unknown"]
**Documented:** [Today's date]
**Last Updated:** [Today's timestamp]

---

## Executive Summary

[What the feature does and why it exists]

**Note:** This is a retrospective implementation plan created by analyzing the existing codebase. The feature has already been implemented.

---

## Goals

### Primary Goals
[What the feature accomplishes]

### Success Criteria
- [x] [Key functionality that exists]
- [x] [Key functionality that exists]
- [ ] [Potential improvements identified]

---

## Current Architecture

[Based on code investigation]

### Key Components
[List main files and their roles]

### Data Flow
[How data moves through the feature]

### Integration Points
[How it connects to other systems]

---

## Implementation Details

[Describe the actual implementation found in code]

### Phase 1: Core Functionality (COMPLETED)
[What was built first - reverse engineer from code]

### Phase 2: Enhancements (COMPLETED)
[Additional features added - reverse engineer from code]

### Phase 3: Potential Improvements (NOT STARTED)
[Based on investigation, what could be enhanced]

---

## Key Technical Decisions

[Document the architectural decisions observed in code]

### Decision 1: [Pattern/Approach Used]
**Context:** [Why this was likely chosen]
**Implementation:** [How it's done in code]
**Trade-offs:** [Benefits and drawbacks observed]

---

## Current State

### What's Working
- [List functioning aspects]

### Known Issues
- [List any issues identified]

### Technical Debt
- [List areas for improvement]

---

## Future Enhancements

[Based on investigation and user input]

### High Priority
- [ ] [Improvement opportunity]

### Medium Priority
- [ ] [Enhancement possibility]

### Low Priority / Nice to Have
- [ ] [Optional improvement]

---

## Testing

### Current Coverage
[What tests exist, if any]

### Testing Gaps
[What should be tested but isn't]

---

## Documentation

### Current Documentation
[What docs exist]

### Documentation Needs
[What should be documented]

---

## Dependencies

### External Dependencies
[Libraries, services, etc.]

### Internal Dependencies
[Other features/modules this depends on]

---

## Notes

**Discovered Information:**
- [Key insights from investigation]
- [Historical context if known]
- [Architectural rationale]

**Retrospective Assessment:**
- [What works well]
- [What could be improved]
- [Lessons learned]

---

*This retrospective plan was created by analyzing existing code. Use `/start-feature <feature-name>` to begin making improvements.*
```

#### 7b. Create Context File (Stub)

**Use Write tool** to create `docs/features/<feature-name>/context.md`:

```markdown
# <Feature Name> - Context & Decisions

**Last Updated:** [Today's date]
**Current Phase:** Retrospective Documentation
**Status:** ✅ Documented (Existing Feature)

---

## Quick Status

**What's Done:**
- ✅ Feature fully implemented (existing code)
- ✅ Retrospective documentation created

**What's Next:**
- 📋 Review for potential improvements (see implementation.md)

**Blockers:**
- None

---

## Key Files

[List key files identified during discovery]

---

## Important Decisions

[Document any architectural decisions discovered during investigation]

---

## Gotchas & Learnings

[Document any gotchas or learnings identified during discovery]

---

*This context file was created retrospectively by analyzing existing code.*
```

#### 7c. Create Tasks File (Stub)

**Use Write tool** to create `docs/features/<feature-name>/tasks.md`:

```markdown
# <Feature Name> - Task Checklist

**Last Updated:** [Today's date]
**Progress:** Complete (Existing Feature)

---

## Original Implementation (COMPLETED)

All original implementation tasks have been completed. This feature was documented retrospectively.

- [x] ✅ Core functionality implemented
- [x] ✅ Integration with existing systems
- [x] ✅ Retrospective documentation created

---

## Future Enhancements

See `implementation.md` Phase 3 / Future Enhancements section for potential improvements.

---

*This task file was created retrospectively. Add new tasks here when working on enhancements.*
```

### 8. Present Plan to User

Show summary:
```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
✅ RETROSPECTIVE DOCUMENTATION CREATED
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

📋 Feature: <feature-name>
📁 Location: docs/features/<feature-name>/

📄 Files Created:
- implementation.md - Retrospective implementation plan
- context.md - Context and decisions (stub)
- tasks.md - Task checklist (marked complete)

📊 Summary:
- Current State: Implemented
- Key Files: [X files]
- Improvement Opportunities: [X identified]

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

**If reusable patterns were identified during discovery**, also show:

```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
📚 REUSABLE PATTERNS IDENTIFIED
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

The following patterns/systems might be useful for other features:

- [Pattern/system 1] - [Brief description]
- [Pattern/system 2] - [Brief description]
- [Gotcha/learning] - [Brief description]

💡 Consider running `/document-feature <feature-name>` to add these
   to the project's skill system for future agents.

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

**Criteria for identifying reusable patterns:**
- Systems or frameworks other features may need to use
- Component patterns, API patterns, state management approaches
- Integration patterns with existing systems
- Gotchas that would help future agents avoid issues
- Utilities or helpers that could benefit other features

**DO NOT suggest /document-feature if:**
- All patterns are specific to this feature only
- No generalizable systems were discovered
- Patterns are already well-documented in existing skills

Show first ~30 lines of the implementation plan.

### 9. Ask for Next Steps

Ask user:

"Would you like to:

1. **Start working on improvements** → Run `/start-feature <feature-name>` to begin enhancements
2. **Just document for now** → Keep the plan as reference, make no changes
3. **Request adjustments** → Tell me what to change in the plan
4. **Cancel** → Discard this documentation
[If reusable patterns identified]
5. **Document reusable patterns** → Run `/document-feature <feature-name>` to add patterns to skill system

What would you like to do?"

**If user chooses option 1 (Start improvements):**
- Run `/start-feature <feature-name>` using SlashCommand tool
- The dev docs will include the retrospective plan as reference
- User can work on the "Future Enhancements" section

**If user chooses option 2 (Document only):**
- Confirm plan is saved
- Suggest they can use `/start-feature <feature-name>` later when ready
- If reusable patterns were identified, remind them they can run `/document-feature <feature-name>` later

**If user chooses option 3 (Adjustments):**
- Ask what to change
- Use Edit tool to update the plan
- Show updated summary
- Ask again (repeat step 9)

**If user chooses option 4 (Cancel):**
- Ask if they want to delete the plan files
- If yes, remove all created files (implementation.md, context.md, tasks.md)
- Confirm cancellation

**If user chooses option 5 (Document patterns) - only shown if reusable patterns identified:**
- Run `/document-feature <feature-name>` using SlashCommand tool
- This will extract patterns to `.claude/skills/` for future agents

## Important Notes

- **DO NOT** make any code changes - this command only documents existing features
- **DO** use the Explore agent for thorough investigation (set thoroughness="very thorough")
- **DO** involve user in understanding the feature's purpose and context
- **DO** create a realistic retrospective plan based on actual code
- **DO** create context.md and tasks.md stubs alongside implementation.md
- **DO** identify improvement opportunities without being critical
- **DO** identify reusable patterns that could benefit other features
- **DO** wait for user approval before running /start-feature or /document-feature

## Special Cases

### Case 1: Feature Partially Documented
If some docs exist but not complete (e.g., has implementation.md but missing context.md or tasks.md):
- Note what exists
- Only create missing files (don't overwrite existing ones)
- Merge with existing documentation
- Skip creating stubs for files that already have real content

### Case 2: Feature Has Existing Plan
Already handled in step 2 - offer to start working on it or review progress.

### Case 3: Feature Spans Multiple Areas
- Document all areas in one plan
- Use clear section headers for each area
- Note how they interact

### Case 4: No Clear Feature Boundaries
- Ask user to clarify scope
- Suggest breaking into multiple features if too broad
- Or create umbrella plan with sub-components

## Error Handling

- If Explore agent can't find the feature, ask user for more specific file paths
- If feature is too complex, offer to break into smaller features
- If insufficient information, ask more questions before creating plan
- If user cancels mid-investigation, clean up partial work

## Example Workflow

```
User: /discover-feature layer-inspector

Claude: I'll help document the layer-inspector feature.

Checking for existing plans...
No existing plan found.

To understand how it works, please tell me:
1. Where is the code?
2. What does it do?
3. Any known issues?

User: It's in src/apps/Editor/modules/LayerInspector/, shows a tree view of scene layers

Claude: Perfect! Investigating the codebase...

[Uses Explore agent]

🔍 Found layer-inspector in 12 files...
[Shows discovery summary]

Creating retrospective documentation...

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
✅ RETROSPECTIVE DOCUMENTATION CREATED
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
📄 Files Created:
- implementation.md
- context.md
- tasks.md

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
📚 REUSABLE PATTERNS IDENTIFIED
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
- Tree component pattern - Recursive rendering with drag/drop
- Selection state management - Multi-select with shift/ctrl modifiers

💡 Consider running `/document-feature layer-inspector`

Would you like to:
1. Start working on improvements
2. Just document for now
3. Request adjustments
4. Cancel
5. Document reusable patterns

User: 5

Claude: Running /document-feature layer-inspector...
✅ Patterns documented to .claude/skills/!
```

---

**Remember:** This command is for legacy features that weren't developed with Beast Mode. It creates documentation by investigating existing code, making it easier to enhance those features using the dev docs workflow.
