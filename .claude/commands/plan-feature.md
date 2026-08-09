---
description: "Plan a new feature by gathering requirements and creating an implementation plan. Usage: /plan-feature <feature-name> [initial requirements]"
---

You have been asked to plan a new feature with implementation documentation.

**Feature name:** First argument from `$ARGUMENTS`
**Initial requirements:** Remaining arguments from `$ARGUMENTS` (may be empty)

## Epic awareness

This command is epic-aware. Before resolving `$ARGUMENTS` to a feature path,
read `.claude/epic-resolution.md` and apply its rules: detect epics by
`epic-overview.md`, resolve `<epic>/<feature>` references, fuzzy-fall-back into
epic folders for bare names, and load epic context (the `epic-overview.md` + all sibling features) when the target is a feature inside an epic.
If no epics exist in `docs/features/`, behave exactly as before.

`.claude/epic-resolution.md` is the single source of truth for epic detection and resolution. When the target is a feature inside an epic (resolved via `<epic>/<feature>` or fuzzy fallback), load that epic's context per §4 before planning and pass it into the solution-architect prompt (see Step 3 and Step 7). This command's body is the planning workflow; it does **not** re-specify those rules.

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

### 3. Explore Project Context

Before asking questions, understand what you're working with:
- Read `CLAUDE.md` and any project docs to understand the tech stack and architecture
- Check recent git commits to understand current development momentum
- Scan the codebase structure (key directories, existing patterns)
- Look at existing features in `docs/features/` for conventions and style

This context informs smarter questions and better approach recommendations.

**If the target is a feature inside an epic** (resolved to `<epic>/<feature>` per the Epic awareness block), also **load epic context per `.claude/epic-resolution.md` §4** before continuing — read the epic's `docs/features/<epic>/epic-overview.md` and `epic-requirements.md` (if present) and **every sibling feature's** `implementation.md` + `context.md` (and `requirements.md` if the sibling is only planned). This tells you the epic's purpose, build order, integration notes, and what the sibling features already do, so the plan fits what comes before and after instead of contradicting it. Keep this epic context — it is passed into the solution-architect prompt in Step 7. (For a plain feature with no epic, skip this — there is nothing to load.)

### 4. Gather Requirements

**If initial prompt was provided:**
- Start with that as the base requirements
- Ask clarifying questions to understand:
  - Key use cases
  - User personas affected (if applicable)
  - Technical constraints
  - Integration points with existing systems

**If no initial prompt:**
- Check if `docs/features/<feature-name>/requirements.md` exists (created by `/suggest-feature`)
- If it exists, read it and use it as the base requirements — inform the user you found existing requirements and summarize them
- Then ask clarifying questions as above to fill any gaps

**If no initial prompt AND no requirements.md:**
- Ask user: "What is this feature about? Describe what you want to build and why."
- Wait for response
- Then ask clarifying questions as above

**Prefer multiple-choice questions using AskUserQuestion** wherever possible. Open-ended questions are fine for initial description, but for constraints, scope, and priorities, offer concrete options based on what you learned from project context.

**Apply YAGNI ruthlessly** - if a requirement sounds like a "nice to have" or "future consideration", push back. Ask: "Is this essential for the first version?" Keep scope tight.

### 4b. Assess Whether Research Is Needed

Before proposing approaches, judge whether this feature depends on **external knowledge you can't reliably answer from the codebase or general knowledge** — for example:

- Comparing competitor products or industry-standard approaches
- Choosing between unfamiliar libraries/services, or relying on current API/pricing/version details
- Regulatory, security, or compliance specifics that must be accurate
- Anything where a wrong external fact would send the architecture down the wrong path

Two constraints shape how you handle this:

- **All research happens here, before handoff.** The solution-architect agent has no web or research access — it can only work from what you pass it. Anything external must be gathered now, not delegated to the architect.
- **Match the tool to the need — don't over-reach.** Most questions are answered by a quick web search; reserve deep research for genuinely high-stakes decisions where a wrong external fact would be costly to unwind. Don't reach for deep research when a simple search would do.

If you identify a research need, **ask the user first** — do not run research silently. Use `AskUserQuestion`:

```
This feature would benefit from some research on <topic> before I lock in scope/approach
(e.g. comparing <X> vs <Y>, current best practice for <Z>).

  • Quick web search now — fast, usually enough                        (recommended)
  • Deep research now — slower / more tokens, for high-stakes calls where a wrong fact is costly
  • No — proceed with what we know
```

- **"Quick web search now"** → use the **`WebSearch`** tool with focused queries, cross-check a couple of sources, and summarize the findings for the user.
- **"Deep research now"** → use the **`/deep-research`** command with a focused question and summarize the verified findings for the user.
- **"No"** → continue normally.

Either way, **capture the findings** and: (a) let them inform the approach options in Step 5, and (b) pass them into the solution-architect prompt in Step 7 as a **Research Findings** block. Never ask the architect to research — it can't.

If no external research is needed (most features), skip this step silently.

### 5. Propose Approaches

Before jumping to a full plan, present **2-3 different approaches** to the user:

- Lead with your **recommended approach** and explain why
- Include **trade-offs** for each (complexity, performance, maintainability, time)
- Ground approaches in what you learned from the project context (existing patterns, tech stack)
- Scale this to complexity: a config change might get a single recommended approach with a brief rationale, while a major feature gets 2-3 detailed options

**Use AskUserQuestion** to let the user pick an approach (or suggest their own).

Wait for the user to choose before proceeding to plan creation.

### 6. Create Feature Directory Structure

Create the directory structure:
```bash
mkdir -p docs/features/<feature-name>
```

### 7. Launch Solution-Architect Agent

Use the `Task` tool with `subagent_type="solution-architect"` to create the implementation plan.

**Agent prompt should include:**
```
Create an implementation plan for the following feature:

**Feature Name:** <feature-name>

**Requirements:**
<gathered requirements from steps 3>

**Chosen Approach:**
<selected approach from step 5, with rationale>

**Research Findings (include ONLY if research was done in Step 4b; omit this whole block otherwise):**
<summary of the verified findings from the quick web search or deep research, with source URLs>
Treat these as authoritative external context — cite them in Key Technical Decisions. Do NOT run further research; you have no web/research access.

**Context:**
- Project: [Detected from CLAUDE.md or project structure]
- Tech Stack: [Detected from project files]
- Existing Architecture: [Brief summary if relevant]
- Existing Patterns: [Relevant patterns found in project context exploration]

**Epic Context (include ONLY if this feature lives inside an epic — omit this whole block for a plain feature):**
This feature is part of the `<epic>` epic. Plan it to fit the epic's architecture, build order, and sibling features. From the epic context loaded in Step 3:
- Epic purpose & scope: [from epic-overview.md / epic-requirements.md]
- Build order & where this feature sits in it: [from epic-overview.md Build Order — what comes before/after]
- Sibling features and what they already do: [one line per sibling from its implementation.md / context.md]
- Integration points & shared conventions to respect: [from epic-overview.md Integration & Architecture]
- Known epic-level tech debt or constraints: [from epic-overview.md, if any]
Write the plan so it integrates with these siblings (reuse their patterns, honor the build order, don't duplicate or contradict their work).

**Output:**
Create a detailed implementation plan in `/docs/features/<feature-name>/implementation.md` following this structure:

1. **Executive Summary** - Brief overview
2. **Goals** - Primary and secondary goals
3. **Architecture Overview** - High-level design
4. **Implementation Phases** - Broken down with tasks, estimates, acceptance criteria
5. **Key Technical Decisions** - Architecture decisions with rationale
6. **Definition of Done** - Feature-level acceptance criteria (see below)
7. **Testing Strategy** - How to validate the feature
8. **Dependencies** - What this feature depends on
9. **Risks & Mitigation** - Potential issues and how to address them

**Definition of Done (IMPORTANT):**
The plan MUST include a "Definition of Done" section with concrete, testable acceptance criteria. These criteria will be used by a separate evaluator agent to verify the feature works correctly. Write them as specific, observable behaviors — not vague quality statements. Include:
- **Functional Criteria:** What the user can do and what should happen (e.g., "User can click 'Save' and see a success toast within 2 seconds")
- **Quality Criteria:** Error handling, loading states, no regressions
- **Integration Criteria:** How this feature connects with existing systems
- **Verification Method:** Step-by-step instructions for how an evaluator should test the feature (manual steps, API calls, or test commands)

**Status:** Planning
**Started:** [Today's date]
**Target Completion:** [Based on user input or TBD]
**Last Updated:** [Today's date with timestamp]

Use the existing implementation plans in this project as reference for format and style.
```

**IMPORTANT:** Tell the agent to:
- Use the `Write` tool to create the implementation plan
- Be thorough but concise — scale detail to the feature's complexity
- Include specific technical details relevant to this project
- Break phases into granular tasks
- Apply YAGNI — only include what's needed for the chosen approach, no speculative features
- Build on existing project patterns rather than introducing new ones unnecessarily
- Set the **Quality Bar** section — tailor the quality descriptors to the feature type (UI-heavy features emphasize visual polish and interaction feel; backend features emphasize reliability and data integrity; full-stack features cover both)
- Write the **Definition of Done** with criteria specific enough that an independent evaluator with no implementation context could verify them
- **If Step 4b produced research findings:** fill in the **Research Findings** block above with the summarized, sourced findings so the architect can cite them in its Key Technical Decisions. The architect has no web or research access — never instruct it to research; all findings must be gathered before handoff. Omit the block entirely if no research was done.
- **If this feature lives inside an epic:** fill in the **Epic Context** block above from the epic docs loaded in Step 3 (epic purpose, build order/position, sibling summaries, integration points, known tech debt) so the architect plans the feature to fit the epic. Omit that block entirely for a plain feature.
- Flag any phases that are major refactors or touch many integration points as needing an **advanced (max-effort) dev agent**, so `/proceed` can route them correctly

### 8. Verify Plan Was Created

After agent completes:
- Check that `docs/features/<feature-name>/implementation.md` exists
- Read the first 50 lines to verify it has proper structure

If plan creation failed:
- Show error to user
- Offer to retry with more context

### 9. Present Plan to User

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

### 10. Wait for Approval

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
- **DO** explore project context before asking questions
- **DO** involve the user in requirements gathering
- **DO** propose 2-3 approaches and get user buy-in before creating the plan
- **DO** use the solution-architect agent for plan creation (don't write it yourself)
- **DO** wait for explicit user approval before running /start-feature
- **DO** apply YAGNI - keep scope tight, push back on speculative requirements
- **DO** scale the process to complexity - a simple feature gets a brief plan, not a novel

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
