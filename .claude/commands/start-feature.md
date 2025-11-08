---
description: Start a new feature with dev docs structure. Usage: /start-feature [feature-name]
---

You have been asked to start a new feature with dev docs structure.

**Feature name:** `$ARGUMENTS`

## Process

1. **Determine Feature Name:**
   - If `$ARGUMENTS` is provided (not empty), use it as the feature name
   - If `$ARGUMENTS` is empty, list features in `docs/features/` and ask user which one to set up
   - Ensure kebab-case format (e.g., "layer-inspector", "undo-redo-system")

2. **Verify Implementation Plan Exists:**
   - Check if `docs/features/$ARGUMENTS/implementation.md` exists
   - If not found, list available features in `docs/features/` and ask user to clarify
   - If no implementation plan exists, explain they need to create one first (using solution-architect agent)

3. **Create Dev Docs Directory:**
   - Create `dev/active/$ARGUMENTS/` directory
   - Verify it doesn't already exist (ask user if they want to overwrite)

4. **Copy Implementation Plan to plan.md:**
   - Use `cp` command to copy `docs/features/$ARGUMENTS/implementation.md` to `dev/active/$ARGUMENTS/plan.md`
   - **IMPORTANT:** Use the `Edit` tool (NOT Write) to update ONLY these specific lines:
     - Change `Status: Planning` (or whatever it is) to `Status: In Progress`
     - Update "Started" date to today's date (YYYY-MM-DD format)
     - Update "Last Updated" to current timestamp (YYYY-MM-DD HH:MM format)
   - **DO NOT** rewrite the entire file - only edit the status header lines
   - The Edit tool will preserve the rest of the file exactly as is

5. **Create context.md:**
   - Use template from `dev/templates/context.md`
   - Fill in feature name from `$ARGUMENTS`
   - Set current timestamp
   - Set status to "🟡 In Progress"
   - Set current phase to "Phase 1" (or whatever first phase is called in plan.md)
   - In "Quick Status" section, set "What's Next" to first task from plan
   - Leave rest of template structure intact

6. **Create tasks.md from Plan:**
   - Use template from `dev/templates/tasks.md`
   - Fill in feature name from `$ARGUMENTS`
   - Break down each phase from plan.md into granular, actionable tasks
   - Each task should have:
     - Clear title describing what needs to be done
     - Specific file(s) that will be modified/created
     - Rough time estimate
   - Organize tasks by phase matching plan.md
   - Calculate total task count
   - Set progress to "0/[total] tasks complete (0%)"

7. **Show Summary:**
   Display:
   ```
   ✅ Created dev docs for: $ARGUMENTS

   📁 Location: dev/active/$ARGUMENTS/

   Files created:
   - plan.md (copied from implementation plan)
   - context.md (ready for updates)
   - tasks.md ([X] tasks across [Y] phases)

   Next steps:
   1. Review plan.md to understand the feature
   2. Start Phase 1 implementation
   3. Update context.md as you make decisions
   4. Mark tasks complete in tasks.md as you finish them
   5. Use /dev-docs-update $ARGUMENTS before compacting conversations

   Ready to start implementation!
   ```

## Important Notes

- **DO NOT** start implementing the feature yet, just set up the dev docs structure
- **DO** break down tasks into granular, actionable items (not just copying phase descriptions)
- **DO** make sure task estimates are realistic
- **DO** organize tasks by phase matching the plan
- **DO** update timestamps to current date/time

## Error Handling

- If `$ARGUMENTS` is empty, list available features and ask which one to set up
- If feature already exists in `dev/active/`, ask if they want to overwrite
- If implementation plan doesn't exist, guide them to create one with solution-architect first
- If templates are missing, inform user they need to set up the dev docs system first
