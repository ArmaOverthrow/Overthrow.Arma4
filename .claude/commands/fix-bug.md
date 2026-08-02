---
description: "Fix a specific bug by ID. Usage: /fix-bug [bug-ID]"
---

You have been asked to fix a specific bug.

**Bug ID:** `$ARGUMENTS`

## Epic awareness

This command is epic-aware. Before resolving the bug's `linkedFeature` to a feature path,
read `.claude/epic-resolution.md` and apply its rules: detect epics by
`epic-overview.md`, resolve `<epic>/<feature>` references, fuzzy-fall-back into
epic folders for bare names, and load epic context (the `epic-overview.md` + all sibling features) when the linked feature lives inside an epic.
If no epics exist in `docs/features/`, behave exactly as before.

`.claude/epic-resolution.md` is the single source of truth for epic detection and resolution. The bug's `linkedFeature` may be a `<epic>/<feature>` slash path or a bare feature name that lives inside an epic — resolve it before reading feature docs, so `docs/features/<linkedFeature>/...` becomes the resolved `docs/features/<epic>/<feature>/...` location. This command's body is the bug-fix workflow; it does **not** re-specify those rules.

## Process

1. **Parse Arguments:**
   - If `$ARGUMENTS` is empty, try to list available bugs (MCP `list_bugs` or glob `docs/bugs/BUG-*.md`) and ask the user which one to fix
   - If `$ARGUMENTS` is provided, validate it looks like a bug ID (e.g. `BUG-001`) before proceeding

2. **Load Bug (MCP-first with silent fallback):**
   - First, try the MCP tool `get_bug` with the provided ID
   - If MCP is unavailable or returns an error, silently fall back to reading `docs/bugs/$ARGUMENTS.md` directly
     - The bug file uses YAML frontmatter with fields: `id`, `title`, `status`, `priority`, `linkedFeature` (optional), `createdAt`, `updatedAt`
     - The body after the frontmatter contains the bug description
   - If the bug is not found by either method:
     - Try to list available bugs (MCP `list_bugs` or glob `docs/bugs/BUG-*.md`)
     - Show available bug IDs and ask the user to pick one
     - If no bugs exist at all, inform the user and stop
   - If the bug `status` is already `closed`, inform the user and ask if they want to reopen and fix it anyway before continuing

3. **Load Feature Context (if linked):**
   - If the bug has a `linkedFeature` field:
     - **Resolve `linkedFeature` per the Epic awareness rules above** — it may be a `<epic>/<feature>` slash path or a bare name that lives inside an epic (resolve via `.claude/epic-resolution.md`'s fuzzy fallback, announcing the resolved path). Use the resolved path for the reads below; do not assume the bare value is a top-level feature folder.
     - Read `docs/features/<resolved-feature>/implementation.md` — for understanding the feature architecture
     - Read `docs/features/<resolved-feature>/context.md` — for key files, decisions, and current state
     - If the linked feature lives inside an epic, also load epic context per `epic-resolution.md` §4 (the epic's `epic-overview.md` + sibling features) so the fix fits the epic's architecture
     - If either file doesn't exist, note it internally and continue without that context
   - If there is no `linkedFeature`, skip this step entirely

4. **Set Bug Status to In-Progress:**
   - First try MCP: `update_bug` with `id` and `status: "in-progress"`
   - If MCP fails, use the Edit tool to change the `status:` line in the bug file's YAML frontmatter from `open` to `in-progress`
   - Also update the `updatedAt` field to the current ISO 8601 timestamp
   - Only change these two lines — preserve everything else in the file

5. **Analyze and Fix the Bug:**
   - Read the bug description carefully before touching any code
   - Use the loaded feature context (if available) to understand the relevant codebase area
   - Identify the root cause by reading the relevant source files
   - Make the code fix using Edit/Write tools
   - Keep the fix minimal and focused — fix the bug, do not refactor surrounding code

6. **Report Changes and Ask User to Test:**
   - Summarize what was changed and why
   - List all files that were modified
   - Ask the user to test that the bug is fixed
   - Wait for the user's response before proceeding

7. **Close or Retry Based on User Feedback:**
   - If the user confirms the fix works:
     - Try MCP: `update_bug` with `id` and `status: "closed"`
     - If MCP fails, use the Edit tool to change `status: in-progress` to `status: closed` in the bug file frontmatter
     - Also update `updatedAt` to the current ISO 8601 timestamp
     - Report: "Bug $ARGUMENTS closed."
   - If the user reports the bug is still not fixed:
     - Leave status as `in-progress`
     - Ask the user for more details about what is still broken
     - Return to step 5 and attempt another fix

## Important Notes

- **DO NOT** modify any code until you have read and fully understood the bug description and relevant code files
- **DO NOT** close a bug without explicit confirmation from the user
- **DO** load feature context when available — it helps understand the codebase structure and prior decisions
- **DO** keep fixes minimal and focused on the specific bug
- **DO** update bug status at each transition: `open` → `in-progress` → `closed`
- The MCP fallback must be completely silent — never surface "MCP unavailable" or similar messages to the user
- When using the Edit tool to update YAML frontmatter, only change the `status:` and `updatedAt:` lines and preserve everything else exactly

## Error Handling

- If `$ARGUMENTS` is empty: list available bug IDs if possible, then ask the user which bug to fix
- If the bug is not found: list available bug IDs and ask the user to pick one
- If no bugs exist at all: inform the user and stop
- If the bug is already `closed`: inform the user and ask if they want to reopen it before fixing
- If feature context files don't exist: continue without them, do not error
- If MCP tools fail: silently fall back to file I/O, never surface the failure to the user
- If the code fix introduces new errors or fails: report the issue clearly to the user and wait for guidance, do not blindly retry
