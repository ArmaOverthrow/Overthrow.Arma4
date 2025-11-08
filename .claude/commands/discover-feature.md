---
description: Discover and document an existing feature in the codebase, creating implementation docs for legacy code. Usage: /discover-feature <feature-name> [description]
---

You have been asked to discover and document an existing feature in the codebase.

**Feature name:** First argument from `$ARGUMENTS`
**Initial description:** Remaining arguments from `$ARGUMENTS` (may be empty)

## Process

### 1. Parse Arguments

Parse `$ARGUMENTS` to extract:
- **Feature name** (required): First word/phrase (use kebab-case format)
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
- Check if `dev/active/<feature-name>/` already exists
- If exists, investigate what work has been done:
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

### 7. Create Retrospective Implementation Plan

Create `docs/features/<feature-name>/` directory structure:
```bash
mkdir -p docs/features/<feature-name>
```

Now create a **retrospective implementation plan** - what the plan might have looked like if this feature was planned using Beast Mode.

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

### 8. Present Plan to User

Show summary:
```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
✅ RETROSPECTIVE PLAN CREATED
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

📋 Feature: <feature-name>
📁 Location: docs/features/<feature-name>/implementation.md

📊 Summary:
- Current State: Implemented
- Key Files: [X files]
- Improvement Opportunities: [X identified]

The plan documents the existing implementation and suggests potential enhancements.

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

Show first ~30 lines of the plan.

### 9. Ask for Next Steps

Ask user:

"Would you like to:

1. **Start working on improvements** → Run `/start-feature <feature-name>` to begin enhancements
2. **Just document for now** → Keep the plan as reference, make no changes
3. **Request adjustments** → Tell me what to change in the plan
4. **Cancel** → Discard this documentation

What would you like to do?"

**If user chooses option 1 (Start improvements):**
- Run `/start-feature <feature-name>` using SlashCommand tool
- The dev docs will include the retrospective plan as reference
- User can work on the "Future Enhancements" section

**If user chooses option 2 (Document only):**
- Confirm plan is saved
- Suggest they can use `/start-feature <feature-name>` later when ready

**If user chooses option 3 (Adjustments):**
- Ask what to change
- Use Edit tool to update the plan
- Show updated summary
- Ask again (repeat step 9)

**If user chooses option 4 (Cancel):**
- Ask if they want to delete the plan file
- If yes, remove it
- Confirm cancellation

## Important Notes

- **DO NOT** make any code changes - this command only documents existing features
- **DO** use the Explore agent for thorough investigation (set thoroughness="very thorough")
- **DO** involve user in understanding the feature's purpose and context
- **DO** create a realistic retrospective plan based on actual code
- **DO** identify improvement opportunities without being critical
- **DO** wait for user approval before running /start-feature

## Special Cases

### Case 1: Feature Partially Documented
If some docs exist but not a complete implementation.md:
- Note what exists
- Fill in the gaps
- Merge with existing documentation

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

Creating retrospective implementation plan...

✅ Plan created! Would you like to start working on improvements?

User: Yes, start it

Claude: Running /start-feature layer-inspector...
✅ Dev docs created! You can now work on enhancements.
```

---

**Remember:** This command is for legacy features that weren't developed with Beast Mode. It creates documentation by investigating existing code, making it easier to enhance those features using the dev docs workflow.
