---
description: Resume work on an existing feature. Usage: /continue-feature [feature-name]
---

You have been asked to continue work on an existing feature.

**Feature name:** `$ARGUMENTS`

## Process

1. **Determine Feature Name:**
   - If `$ARGUMENTS` is provided (not empty), use it as the feature name
   - If `$ARGUMENTS` is empty, list all features in `dev/active/` and ask user which one to continue
   - Verify the feature exists in `dev/active/$ARGUMENTS/`

2. **Load Dev Docs:**
   Read all three files in order:
   - `dev/active/$ARGUMENTS/plan.md`
   - `dev/active/$ARGUMENTS/context.md`
   - `dev/active/$ARGUMENTS/tasks.md`

3. **Analyze Current State:**
   From the docs, determine:
   - Current phase
   - Completed tasks (✅)
   - In-progress task (🔄)
   - Next steps (from context.md "Next Steps")
   - Any blockers (⏸️ or mentioned in context.md)

4. **Provide Status Summary:**
   Display comprehensive summary:
   ```
   📋 Feature: $ARGUMENTS
   🎯 Current Phase: Phase [X] - [Phase Name]
   📊 Progress: [X]/[Y] tasks complete ([Z]%)

   ✅ What's Done:
   - [Major completed item 1]
   - [Major completed item 2]
   - [Major completed item 3]

   🔄 In Progress:
   - [Current task if any]

   📝 Next Steps:
   [Copy from context.md "Next Steps" → "Immediate" section]

   🚧 Blockers:
   [List any blockers, or "None"]

   💡 Key Files:
   [List 3-5 most important files from context.md]

   🔍 Recent Decisions:
   [List most recent 1-2 decisions from context.md]

   Ready to continue! What would you like to work on?
   ```

5. **Be Ready to Work:**
   - You now have full context from the dev docs
   - You know exactly where things stand
   - You know what to do next
   - Answer any questions the user has
   - Ready to continue implementation

## Helpful Behaviors

- **If asked "what should I do next?"** → Refer to "Next Steps" from context.md
- **If asked about a decision** → Check "Important Decisions" in context.md
- **If asked about a file** → Check "Key Files" in context.md
- **If asked about progress** → Show tasks.md progress percentage
- **If user wants to work on specific task** → Mark it as 🔄 in tasks.md

## Before Starting Work

Remind the user:
```
💡 Tip: As we work, I'll update tasks.md when tasks complete.
       Use /dev-docs-update $ARGUMENTS before compacting conversations!
```

## Error Handling

- If `$ARGUMENTS` is empty, list all features in `dev/active/` and ask user to select
- If `$ARGUMENTS` provided but feature doesn't exist in `dev/active/`:
  - Check `dev/completed/` and inform if it's there
  - List available features in `dev/active/`
  - Ask user to select from available features
- If no features in `dev/active/`:
  - Inform user no active features found
  - Suggest using `/start-feature` to begin
- If dev docs files are missing:
  - Offer to recreate them with `/start-feature`
- If context.md is very outdated (check "Last Updated"):
  - Warn user it might be stale
