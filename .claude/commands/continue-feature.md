---
description: "Resume work on an existing feature. Usage: /continue-feature [feature-name]"
---

You have been asked to continue work on an existing feature.

**Feature name:** `$ARGUMENTS`

## Epic awareness

This command is epic-aware. Before resolving `$ARGUMENTS` to a feature path,
read `.claude/epic-resolution.md` and apply its rules: detect epics by
`epic-overview.md`, resolve `<epic>/<feature>` references, fuzzy-fall-back into
epic folders for bare names, and load epic context (the `epic-overview.md` + all sibling features) when the target is a feature inside an epic.
Additionally, if the target is a bare epic name, switch to whole-epic summary mode instead of treating it as a single feature.
If no epics exist in `docs/features/`, behave exactly as before.

`.claude/epic-resolution.md` is the single source of truth for epic detection and resolution. Two epic cases change this command's behavior: a **feature inside an epic** (`<epic>/<feature>` or a fuzzy-resolved bare name) means every `docs/features/$ARGUMENTS/...` path below is the resolved nested `docs/features/<epic>/<feature>/...` location and you load epic context per §4 first; a **bare epic name** (a top-level dir that has `epic-overview.md`) switches to **whole-epic summary mode** (Step 1a below) instead of the single-feature flow. This command's body is the continue workflow; it does **not** re-specify the resolution rules.

## Process

1. **Determine Feature Name:**
   - If `$ARGUMENTS` is provided (not empty), use it as the feature name
   - If `$ARGUMENTS` is empty, list all features in `docs/features/` and ask user which one to continue
   - Verify the feature exists in `docs/features/$ARGUMENTS/`
   - **Resolve epics first (per `.claude/epic-resolution.md`):** the resolution determines which path this command takes:
     - **Bare epic name** (a top-level dir that has `epic-overview.md`) → go to **Step 1a (whole-epic summary mode)** instead of Steps 2–5. Do **not** treat the epic as a single feature.
     - **Feature inside an epic** (`<epic>/<feature>`, or a bare name fuzzy-resolved to one — announce the resolved path) → continue with Steps 2–5, where every `docs/features/$ARGUMENTS/...` path is the nested `docs/features/<epic>/<feature>/...` location, and **load epic context per §4 first** (see Step 1b).
     - **Plain feature** (top-level dir, no `epic-overview.md`) → continue with Steps 2–5 exactly as before; skip Steps 1a and 1b.

1a. **Whole-Epic Summary Mode (only when `$ARGUMENTS` resolved to a bare epic):**

   When the target is an epic rather than a single feature, summarize the **whole epic** and its features instead of running the single-feature status flow:
   - Read the epic's `docs/features/<epic>/epic-overview.md` (purpose, Features table, build order, integration notes, tech debt) and `epic-requirements.md` if present.
   - For **each child feature subfolder** under `docs/features/<epic>/`, read its `context.md` (status, "Next Steps", recent decisions) and `tasks.md` (progress %, current 🔄 task, blockers). For a still-planned feature with only a `requirements.md`, note it as "planned, not started".
   - Present an **epic-level summary** (not a single-feature view):
     ```
     📦 Epic: <epic>
     🎯 <one-line epic purpose from epic-overview.md>
     📊 Epic progress: <rolled-up tasks across features, e.g. 34/61 (56%)>

     Features (build order):
     1. <feature-a> — <status> — <X/Y tasks (Z%)> — <one-line current focus / blocker>
     2. <feature-b> — <status> — <X/Y tasks (Z%)> — ...
     3. <feature-c> — planned, not started — <one-line scope>

     🔗 Integration / cross-feature notes:
     [Key points from epic-overview.md Integration & Architecture]

     🚧 Blockers (any feature):
     [List, or "None"]

     ➡️  Suggested next step:
     [Per the epic's build order + each feature's status — name the specific feature to work
      next and the exact command, e.g. "Run /continue-feature <epic>/<feature> to resume <feature>"
      or "/plan-feature <epic>/<feature> to plan the next planned feature".]
     ```
   - Then ask which feature the user wants to work on (offer `/continue-feature <epic>/<feature>` for the recommended one). **Do not** proceed into a single feature's implementation from whole-epic mode without the user choosing one. Skip Steps 2–5.

1b. **Load Epic Context (only when `$ARGUMENTS` is a feature inside an epic):**

   Before Step 2, load the epic's context per `.claude/epic-resolution.md` §4 — read `docs/features/<epic>/epic-overview.md` (+ `epic-requirements.md` if present) and **every sibling feature's** `implementation.md` + `context.md` — so the status summary and "Next Steps" account for sibling progress and the epic's build order. (Plain features skip this.)

2. **Load Feature Docs:**
   Read all three files in order:
   - `docs/features/$ARGUMENTS/implementation.md`
   - `docs/features/$ARGUMENTS/context.md`
   - `docs/features/$ARGUMENTS/tasks.md`

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
   - You now have full context from the feature docs
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
       Use /update-feature $ARGUMENTS before compacting conversations!
```

## Error Handling

- If `$ARGUMENTS` is empty, list all features in `docs/features/` and ask user to select
- If `$ARGUMENTS` provided but feature doesn't exist in `docs/features/`:
  - List available features in `docs/features/`
  - Ask user to select from available features
- If no features in `docs/features/`:
  - Inform user no features found
  - Suggest using `/plan-feature` to create a new feature
- If feature docs files are missing (no context.md or tasks.md):
  - Offer to recreate them with `/start-feature`
- If context.md is very outdated (check "Last Updated"):
  - Warn user it might be stale
