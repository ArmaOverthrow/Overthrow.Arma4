---
description: Update dev docs with current progress before compacting. Usage: /dev-docs-update [feature-name]
---

You have been asked to update the dev docs before compacting the conversation.

**⚠️ CRITICAL: This should ALWAYS be run before compacting a conversation!**

**Feature name:** `$ARGUMENTS`

## Process

1. **Determine Feature Name:**
   - If `$ARGUMENTS` is provided (not empty), use it as the feature name
   - If `$ARGUMENTS` is empty:
     - Try to detect from recent conversation (look for feature name mentions, file paths in `dev/active/`)
     - If detected, confirm with user: "Updating dev docs for [detected-name]?"
     - If unclear, list all features in `dev/active/` and ask user which one
   - Verify `dev/active/$ARGUMENTS/` exists

2. **Update tasks.md:**

   **Mark Completed Tasks:**
   - Review entire conversation for tasks that were completed
   - Change `- [ ]` to `- [x] ✅` for completed tasks
   - Add completion date: `- [x] ✅ **Task** - Completed 2025-10-30`
   - Remove any 🔄 (in progress) indicators from completed tasks

   **Update In-Progress Tasks:**
   - If currently working on a task, mark it: `- [ ] 🔄 **Task**`
   - Only ONE task should have 🔄 at a time

   **Add Discovered Tasks:**
   - Review conversation for new tasks discovered during implementation
   - Add them to appropriate phase in tasks.md
   - Mark as `- [ ]` (not started)

   **Update Progress:**
   - Count total tasks
   - Count completed tasks (✅)
   - Update progress line: `**Progress:** X/Y tasks complete (Z%)`
   - Update "Last Updated" timestamp

   **Add Session Notes:**
   - Under "Completed This Session" add today's completed tasks
   - Under "Discovered New Tasks" add newly found tasks

3. **Update context.md:**

   **Update Quick Status:**
   - **What's Done:** Add major items completed this session
   - **What's Next:** Update with next 2-3 immediate tasks
   - **Blockers:** Add any new blockers discovered

   **Add/Update Important Decisions:**
   - Review conversation for technical decisions made
   - Add new decisions with:
     - Date (2025-10-30)
     - Context (why decision was needed)
     - Decision (what was decided)
     - Rationale (why this approach)
     - Code example if applicable

   **Add Gotchas & Learnings:**
   - Review conversation for problems encountered
   - Add with:
     - Problem description
     - Solution that worked
     - Lesson/takeaway

   **Update Next Steps:**
   - **Immediate:** What to work on in next session (1-3 tasks)
   - **Short Term:** What's coming up this week
   - Clear and actionable

   **Add Session Notes:**
   - Add new session note at bottom with:
     - Current timestamp
     - Progress made this session
     - Decisions made
     - Any blockers
     - What next session should focus on

   **Update Timestamps:**
   - Update "Last Updated" at top of file
   - Update current phase if changed

4. **Verify plan.md:**
   - Rarely needs updates
   - Only update "Last Updated" timestamp if plan itself changed
   - If implementation significantly diverged from plan, add note in context.md (not plan.md)

5. **Show Summary:**
   Display what was updated:
   ```
   ✅ Updated dev docs for: $ARGUMENTS

   📝 tasks.md:
   - Marked [X] tasks complete
   - Added [Y] newly discovered tasks
   - Updated progress: [A]/[B] ([C]%)
   - Updated: [task names]

   📋 context.md:
   - Added [X] new decisions
   - Added [Y] gotchas/learnings
   - Updated "Next Steps"
   - Added session notes

   🎯 Next Session Focus:
   [Show "Immediate" items from "Next Steps"]

   ✅ Safe to compact conversation!

   💡 To resume: /continue-feature $ARGUMENTS
   ```

## Quality Checks

Before finishing, verify:
- [ ] All completed tasks marked with ✅
- [ ] Progress percentage is accurate
- [ ] "Next Steps" is clear and actionable
- [ ] Important decisions documented
- [ ] Timestamps updated
- [ ] Session notes added

## Important Notes

- **BE THOROUGH:** Review the entire conversation for completed work
- **BE SPECIFIC:** "Next Steps" should be concrete, not vague
- **BE HONEST:** If nothing was accomplished, note that
- **BE HELPFUL:** Make it easy to resume work

## Error Handling

- If `$ARGUMENTS` is empty, try to auto-detect from conversation or list available features
- If `$ARGUMENTS` provided but feature doesn't exist:
  - List available features in `dev/active/`
  - Ask which one to update
- If dev docs files are missing:
  - Offer to recreate with `/start-feature`
- If conversation is too short to have meaningful updates:
  - Explain and ask if user still wants to update
