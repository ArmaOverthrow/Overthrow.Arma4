---
description: "Fix all open bugs linked to a feature. Usage: /fix-feature [feature-name]"
---

You have been asked to fix all open bugs linked to a feature.

**Feature name:** `$ARGUMENTS`

## Epic awareness

This command is epic-aware. Before resolving `$ARGUMENTS` (and each bug's `linkedFeature`) to a feature path,
read `.claude/epic-resolution.md` and apply its rules: detect epics by
`epic-overview.md`, resolve `<epic>/<feature>` references, fuzzy-fall-back into
epic folders for bare names, and load epic context (the `epic-overview.md` + all sibling features) when a feature lives inside an epic.
Additionally, if `$ARGUMENTS` is a bare epic name, operate across all of the epic's features — fix every open bug linked to any of its child features.
If no epics exist in `docs/features/`, behave exactly as before.

`.claude/epic-resolution.md` is the single source of truth for epic detection and resolution. Both `$ARGUMENTS` and each bug's `linkedFeature` may be a `<epic>/<feature>` slash path or a bare name that lives inside an epic — resolve them before reading feature docs, so `docs/features/<feature-name>/...` becomes the resolved `docs/features/<epic>/<feature>/...` location. This command's body is the fix-feature workflow; it does **not** re-specify those rules.

## Process

1. **Parse Arguments:**
   - If `$ARGUMENTS` is provided: **resolve it per the Epic awareness rules above** before using it as the feature name.
     - If it resolves to a **feature inside an epic** (`<epic>/<feature>`, directly or via fuzzy fallback — announce the resolved path), use that nested path as the feature; the bug filter matches that nested `linkedFeature` value.
     - If it resolves to a **bare epic name** (a top-level dir with `epic-overview.md`), operate in **whole-epic mode**: target every open bug linked to any of the epic's child features (`<epic>/<child>`), grouped per child feature.
     - Otherwise check whether `docs/features/<feature-name>/` exists — if not found, warn the user but still proceed to search `docs/bugs/` for bugs that reference it.
   - If `$ARGUMENTS` is empty: operate in "all" mode — process every open bug across all features and unlinked bugs.

2. **Load Bugs (MCP-first with silent fallback):**
   - If a feature name was provided (use the resolved path from Step 1 — e.g. `<epic>/<feature>` for an epic-nested feature):
     - Try MCP `list_bugs` with filters `linkedFeature: <resolved-feature>` and `status: "open"`
     - If MCP fails, glob `docs/bugs/BUG-*.md`, read each file's YAML frontmatter, and filter where `linkedFeature` matches the resolved name and `status` is `open`
   - If `$ARGUMENTS` resolved to a **bare epic name** (whole-epic mode): gather bugs for **each** child feature — call MCP `list_bugs` once per child with `linkedFeature: <epic>/<child>` and `status: "open"`, or (on MCP failure) glob `docs/bugs/BUG-*.md` and keep those whose `linkedFeature` matches any of the epic's child features. Each child feature becomes a group in Step 3.
   - If in "all" mode:
     - Try MCP `list_bugs` with filter `status: "open"`
     - If MCP fails, glob `docs/bugs/BUG-*.md`, read each file's YAML frontmatter, and filter where `status` is `open`
   - If no open bugs are found: inform the user and stop

3. **Prioritize and Group:**
   - Group bugs by their `linkedFeature` value — each unique value forms one group
   - Bugs with no `linkedFeature` (unlinked) form their own group, always processed last
   - Within each group, sort by priority: critical → high → medium → low
   - If a specific feature was requested, there will be only one group
   - Show the user a summary before starting:
     ```
     Found X open bugs:
     - feature-name-1: N bugs (critical: 1, high: 2, ...)
     - feature-name-2: N bugs (...)
     - Unlinked: N bugs

     Starting with feature-name-1...
     ```

4. **For Each Feature Group (sequential — complete one group before starting the next):**

   4a. **Load Feature Context:**
      - **Resolve the group's `linkedFeature` per the Epic awareness rules** — it may be a `<epic>/<feature>` path or a bare name inside an epic. Use the resolved path for the reads below.
      - Read `docs/features/<resolved-feature>/implementation.md` — for understanding the feature architecture
      - Read `docs/features/<resolved-feature>/context.md` — for key files, decisions, and current state
      - If the feature lives inside an epic, also load epic context per `epic-resolution.md` §4 (the epic's `epic-overview.md` + siblings)
      - If either file doesn't exist, note it internally and continue without that context

   4b. **Set All Bugs in Group to In-Progress:**
      - For each bug in the group:
        - Try MCP: `update_bug` with `id` and `status: "in-progress"`
        - If MCP fails, use the Edit tool to change `status: open` to `status: in-progress` in the bug file's YAML frontmatter
        - Update `updatedAt` to the current ISO 8601 timestamp
        - Only change these two lines — preserve everything else in the file

   4c. **Fix All Bugs in Group Sequentially:**
      - Process bugs in priority order (critical first)
      - For each bug:
        - Read the bug description carefully before touching any code
        - Identify the root cause by reading the relevant source files
        - Make the code fix using Edit/Write tools
        - Keep the fix minimal and focused — fix the bug, do not refactor surrounding code
      - Complete all bugs in the group before moving to step 4d

   4d. **Report Changes and Ask User to Test:**
      - Print a summary of all changes made for this feature group:
        ```
        Fixed X bugs in [feature-name]:
        - BUG-001: [title] — [brief description of fix]
        - BUG-002: [title] — [brief description of fix]

        Files modified:
        - path/to/file1.ts
        - path/to/file2.ts

        Please test these fixes. Are they all working correctly?
        ```
      - Wait for the user's response before proceeding

   4e. **Close or Handle Partial Rejection:**
      - If the user confirms all fixes work:
        - For each bug in the group:
          - Try MCP: `update_bug` with `id` and `status: "closed"`
          - If MCP fails, use the Edit tool to change `status: in-progress` to `status: closed` in the bug file frontmatter
          - Update `updatedAt` to the current ISO 8601 timestamp
      - If the user says some bugs are still broken:
        - Ask which specific bugs are still broken
        - Close the confirmed bugs (MCP `update_bug` or Edit tool as above)
        - Leave the broken bugs as `in-progress`
        - Note which ones still need fixing
      - **DO NOT** proceed to the next feature group until the user has responded

5. **Process Next Feature Group:**
   - After the user confirms or rejects the current group, move to the next feature group
   - Repeat steps 4a–4e for each group
   - Unlinked bugs are always the last group

6. **Print Final Summary:**
   ```
   Bug Fix Summary:
   - Closed: X bugs
   - Still in-progress: Y bugs
   - Errors: Z bugs

   [List any bugs that are still open with their IDs and titles]
   ```

## Important Notes

- **DO NOT** modify any code until you have read the bug description and relevant code
- **DO NOT** close bugs without explicit user confirmation
- **DO NOT** proceed to the next feature group until the user has confirmed the current one
- **DO** load feature context for each group — it helps understand the codebase and prior decisions
- **DO** keep fixes minimal and focused on each individual bug
- **DO** update bug status at each transition: `open` → `in-progress` → `closed`
- **DO** process bugs within each group in priority order (critical first)
- The MCP fallback must be completely silent — never surface "MCP unavailable" or similar messages to the user
- When using the Edit tool to update YAML frontmatter, only change the `status:` and `updatedAt:` lines and preserve everything else exactly

## Error Handling

- If no open bugs are found for the specified feature: inform the user "No open bugs found for [feature-name]" and stop
- If no open bugs are found at all (all mode): inform the user "No open bugs found" and stop
- If `$ARGUMENTS` names a feature not found in `docs/features/`: warn the user but still search `docs/bugs/` for bugs that reference it by name
- If feature context files don't exist: continue without them, do not error
- If MCP tools fail: silently fall back to file I/O, never surface the failure to the user
- If a code fix introduces new errors: report the issue clearly to the user and wait for guidance, do not blindly retry
