---
description: "Suggest the next feature to develop based on project state, mission statement, and technical design. Usage: /suggest-feature"
---

You have been asked to suggest what feature should be developed next.

## Epic awareness

This command is epic-aware. It takes no feature argument, but when surveying the project it must understand epics: read `.claude/epic-resolution.md` for the rules, and detect epics by `epic-overview.md`.
When reading existing features, treat each **epic** as a unit — an epic directory has no `implementation.md` of its own, so read its `epic-overview.md` (purpose, Features table, build order, status) and descend into its child feature subfolders. A suggestion may be a **new feature inside an existing epic** — present it as `<epic>/<feature>` and, if chosen, scaffold `docs/features/<epic>/<feature>/requirements.md`.
If no epics exist in `docs/features/`, behave exactly as before.

`.claude/epic-resolution.md` is the single source of truth for epic detection and resolution. This command's body is the suggest-feature workflow; it does **not** re-specify those rules.

## Process

### 1. Gather Project Context

Read the following to understand the current state of the project:

- `docs/overview.md` — master overview with feature status table, integration points, and changelog (if exists — this is the best starting point as it tracks all features and their progress)
- `CLAUDE.md` — project overview, architecture, development phases
- `docs/mission-statement.md` — product vision and goals (if exists)
- `docs/technical-design.md` — comprehensive technical specification (if exists)
- All existing feature docs in `docs/features/*/implementation.md` — what has been planned or built so far. **For epics** (a `docs/features/<dir>/` containing `epic-overview.md`), read its `epic-overview.md` instead (it rolls up the epic's features and status) and descend into the child feature subfolders' `implementation.md`.
- Scan the main source directories to see what code actually exists (not just planned)

If `docs/overview.md` exists, use it as your primary source for feature status and completion — it aggregates all feature progress in one place. Fall back to reading individual feature docs for details not captured in the overview.

If `docs/mission-statement.md` or `docs/technical-design.md` do not exist, rely on `CLAUDE.md` and the codebase itself. You can still make useful suggestions without these files, but note their absence and suggest the user create them for better results.

### 2. Determine Current Phase

From `CLAUDE.md` and/or `docs/technical-design.md`, identify if the project defines development phases or milestones.

- If phases are defined, determine which phase the project is currently in by cross-referencing with existing features and actual code
- If no phases are defined, assess project maturity by examining what exists: early (core infrastructure), mid (key features being built), or late (polish/optimization)

Cross-reference feature docs with actual code to determine what is truly complete vs. only planned.

### 3. Identify Candidates

Based on the current phase, what has been built, and any dependency chains from the technical design, identify the next logical features. Consider:

- **Dependencies:** What must exist before other things can be built?
- **Vertical slices:** Prefer features that deliver something end-to-end over horizontal expansion
- **Phase boundaries:** Stay within the current phase unless it is complete
- **Technical design guidance:** Follow the architecture and system dependencies described in the tech design doc (if available)
- **Mission alignment:** Prioritize features that advance the core product vision

### 3b. When the Path Isn't Clear: Offer Deep Research

Usually the next features follow naturally from the phase, dependencies, and mission. But sometimes there's **no clear path** — the project is between phases, the mission is broad, or deciding what's next genuinely depends on **external context** like what competitors ship, current industry standards, or emerging best practices.

When that's the case, **ask the user before researching** — don't do it silently:

```
I don't have a strong next-feature signal from the project state alone — choosing well here
depends on outside context (e.g. what comparable products offer, current standards for <X>).
Do you want me to use deep research to look this up? It's slower and uses more tokens but
gives better-grounded suggestions.

  • Yes, run deep research
  • No, suggest based on what we have
```

If the user says yes, use the **`/deep-research`** command with a focused question (competitor landscape, industry standards, etc.), then fold the verified findings into your candidate list in Step 4. If no, proceed with project-state-based suggestions as normal.

Skip this step entirely when the next steps are already clear from the project.

### 4. Present Suggestions

Present **2-4 feature suggestions** using this format:

```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  SUGGESTED NEXT FEATURES
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Current Phase: [Phase name or maturity assessment]
Completed: [Brief list of what exists]

1. **feature-name** — One-sentence description of what it delivers.

2. **feature-name** — One-sentence description of what it delivers.

3. **feature-name** — One-sentence description of what it delivers.

Recommended: #N — [Brief reason why this should come first]

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

Keep each suggestion to **one line** — a kebab-case name and a short description. No implementation details, no technical breakdown. The point is to help the user decide direction, not to pre-plan the feature.

If there is a clear dependency order (A must come before B), say so briefly.

### 5. Offer Next Steps

After presenting suggestions, use **AskUserQuestion** to let the user choose:

- Each suggestion as an option
- Options should include the feature name as the label and description as the description

Based on the user's choice:

**If the user picks a feature:**
1. Create the feature directory — `docs/features/<feature-name>/`, or `docs/features/<epic>/<feature-name>/` if the suggestion belongs inside an existing epic (use the `<epic>/<feature>` name you presented).
2. Write `requirements.md` in that directory with a concise requirements summary:

```markdown
# <Feature Name> - Requirements

**Created:** [Today's date]
**Phase:** [Current development phase]

## Overview

[2-3 sentence description of what this feature delivers and why it matters at this stage]

## Requirements

- [Bullet list of concrete requirements derived from the technical design and mission statement]
- [Keep it focused — what must this feature do, not how]

## Dependencies

- [Features or systems that must exist first]

## Out of Scope

- [Things explicitly NOT included in this feature to keep scope tight]
```

3. Tell the user they can now run `/plan-feature <feature-name>` to create the full implementation plan, and that the requirements.md will provide context to the planning process.

**If the user wants more detail on a suggestion:**
- Give a brief (3-5 sentence) expansion of what the feature covers, still without a full plan
- Then ask again if they want to proceed

## Important Notes

- **DO NOT** create implementation plans — that is `/plan-feature`'s job
- **DO NOT** write lengthy descriptions — keep suggestions concise
- **DO** read actual code, not just feature docs, to understand true project state
- **DO** follow the dependency chain from the technical design (if available)
- **DO** stay within the current development phase unless it is complete
- **DO** consider what delivers the most value as a vertical slice
