---
description: "Start a new feature with dev docs structure. Usage: /start-feature [feature-name]"
---

You have been asked to start a new feature with dev docs structure.

**Feature name:** `$ARGUMENTS`

## Epic awareness

This command is epic-aware. Before resolving `$ARGUMENTS` to a feature path,
read `.claude/epic-resolution.md` and apply its rules: detect epics by
`epic-overview.md`, resolve `<epic>/<feature>` references, fuzzy-fall-back into
epic folders for bare names, and load epic context (the `epic-overview.md` + all sibling features) when the target is a feature inside an epic.
If no epics exist in `docs/features/`, behave exactly as before.

`.claude/epic-resolution.md` is the single source of truth for epic detection and resolution. When `$ARGUMENTS` resolves to a feature inside an epic (via `<epic>/<feature>` or fuzzy fallback), every `docs/features/$ARGUMENTS/...` path below is the resolved nested `docs/features/<epic>/<feature>/...` location; load that epic's context per §4 before scaffolding (see Step 1). This command's body is the scaffolding workflow; it does **not** re-specify those rules.

## Process

1. **Determine Feature Name:**
   - If `$ARGUMENTS` is provided (not empty), use it as the feature name
   - If `$ARGUMENTS` is empty, list features in `docs/features/` and ask user which one to set up
   - Ensure kebab-case format (e.g., "layer-inspector", "undo-redo-system")
   - **If the resolved target is a feature inside an epic** (`<epic>/<feature>` per the Epic awareness block — including a fuzzy-resolved bare name), the nested path `docs/features/<epic>/<feature>/` is what every step below operates on (create/verify the docs at that nested path). First **load epic context per `.claude/epic-resolution.md` §4** — read the epic's `epic-overview.md` (+ `epic-requirements.md` if present) and each sibling feature's `implementation.md` + `context.md` — so the dev docs you scaffold reflect the epic's build order and siblings. (For a plain feature with no epic, skip this — nothing to load.)

2. **Verify Implementation Plan Exists:**
   - Check if `docs/features/$ARGUMENTS/implementation.md` exists
   - If not found, list available features in `docs/features/` and ask user to clarify
   - If no implementation plan exists, explain they need to create one first (using `/plan-feature` or solution-architect agent)

3. **Check if Already Started:**
   - Check if `docs/features/$ARGUMENTS/context.md` and `docs/features/$ARGUMENTS/tasks.md` exist
   - If they exist, the feature has already been started
   - Ask user if they want to:
     - Continue with existing docs (use `/continue-feature` instead)
     - Overwrite and start fresh
     - Cancel

4. **Update Implementation Plan Status:**
   - **IMPORTANT:** Use the `Edit` tool (NOT Write) to update ONLY these specific lines in `docs/features/$ARGUMENTS/implementation.md`:
     - Change `Status: Planning` (or whatever it is) to `Status: In Progress`
     - Update "Started" date to today's date (YYYY-MM-DD format)
     - Update "Last Updated" to current timestamp (YYYY-MM-DD HH:MM format)
   - **DO NOT** rewrite the entire file - only edit the status header lines
   - The Edit tool will preserve the rest of the file exactly as is

5. **Create context.md:**
   - Create `docs/features/$ARGUMENTS/context.md`
   - Use template structure (or read from `.claude/templates/context.md` if exists)
   - Fill in feature name from `$ARGUMENTS`
   - Set current timestamp
   - Set status to "🟡 In Progress"
   - Set current phase to "Phase 1" (or whatever first phase is called in implementation.md)
   - In "Quick Status" section, set "What's Next" to first task from implementation.md
   - Leave rest of template structure intact

6. **Create tasks.md from Plan:**
   - Create `docs/features/$ARGUMENTS/tasks.md`
   - Use template structure (or read from `.claude/templates/tasks.md` if exists)
   - Fill in feature name from `$ARGUMENTS`
   - Break down each phase from implementation.md into granular, actionable tasks
   - Each task should have:
     - Clear title describing what needs to be done
     - Specific file(s) that will be modified/created
     - Rough time estimate
   - Organize tasks by phase matching implementation.md
   - Calculate total task count
   - Set progress to "0/[total] tasks complete (0%)"

7. **Show Summary:**
   Display:
   ```
   ✅ Started feature: $ARGUMENTS

   📁 Location: docs/features/$ARGUMENTS/

   Files:
   - implementation.md (status updated to "In Progress")
   - context.md (created - ready for updates)
   - tasks.md (created - [X] tasks across [Y] phases)

   Next steps:
   1. Review implementation.md to understand the feature
   2. Start Phase 1 implementation
   3. Update context.md as you make decisions
   4. Mark tasks complete in tasks.md as you finish them
   5. Use /update-feature $ARGUMENTS before compacting conversations

   Ready to start implementation!
   ```

## Important Notes

- **DO NOT** start implementing the feature yet, just set up the dev docs structure
- **DO** break down tasks into granular, actionable items (not just copying phase descriptions)
- **DO** make sure task estimates are realistic
- **DO** organize tasks by phase matching the plan
- **DO** update timestamps to current date/time
- **NEW:** All docs now live in `docs/features/` (no separate `/dev` folder)

## Error Handling

- If `$ARGUMENTS` is empty, list available features and ask which one to set up
- If feature already started (context.md exists), ask if they want to continue or overwrite
- If implementation plan doesn't exist, guide them to create one with `/plan-feature` or solution-architect
- If templates are missing, use sensible defaults based on existing features
