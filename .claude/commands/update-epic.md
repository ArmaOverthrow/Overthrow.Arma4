---
description: "Refresh an epic's overview and master rollup row with the latest progress from its child features. Usage: /update-epic [name]"
---

You have been asked to update an epic's documentation with the most recent changes from its child features.

**Epic name (optional):** `$ARGUMENTS`

## Epic awareness

This command is epic-aware. Before resolving `$ARGUMENTS` to a feature path,
read `.claude/epic-resolution.md` and apply its rules: detect epics by
`epic-overview.md`, resolve `<epic>/<feature>` references, fuzzy-fall-back into
epic folders for bare names, and the target is an epic; refresh its `epic-overview.md` rollup and the epic's single row in the master overview.
If no epics exist in `docs/features/`, behave exactly as before.

`.claude/epic-resolution.md` is the single source of truth for epic detection and resolution. This command's body is the update-epic workflow; it does **not** re-specify the detection/resolution rules.

## Purpose

`epic-overview.md` is the source of truth for an epic's aggregate status. It holds the Features table (per-child status + task counts), build order, integration notes, and the **Master Overview Rollup** that `/update-master` reads to produce the epic's single row in `docs/overview.md`. This command keeps all three layers in sync:

1. **Child features** — `tasks.md` + `context.md` (the source data, never modified here)
2. **`epic-overview.md`** — the epic's internal status document (updated here)
3. **`docs/overview.md`** (and optionally `docs/feature-status.md`) — the master row for this epic (updated here)

**TARGETED EDITS ONLY.** Never rewrite `epic-overview.md` or `docs/overview.md` whole-file. Use targeted `Edit` tool calls for every change. A whole-file `Write` risks clobbering content you have not read (tech-debt findings, integration notes, other features' rows). This discipline is explicit throughout the steps below.

---

## Process

### Step 1 — Determine the epic

#### 1a. Named epic (`$ARGUMENTS` provided)

If `$ARGUMENTS` is non-empty:

1. Verify `docs/features/$ARGUMENTS/epic-overview.md` exists (the marker rule).
   - If not found, check whether `$ARGUMENTS` uses a slash (`editor/base`) — that is a feature, not an epic. Report: "Argument `$ARGUMENTS` is a feature inside an epic, not an epic. To update the epic run `/update-epic editor`. To update the feature's own docs run `/update-feature editor/base`."
   - If no `epic-overview.md` and no slash, report the error and list all epics (dirs with `epic-overview.md`) so the user can pick the right one.
2. Use `$ARGUMENTS` as the epic name. Proceed to Step 2.

#### 1b. No argument — infer from session

If `$ARGUMENTS` is empty:

1. **Scan the session context** for recently touched file paths under `docs/features/`. Extract the parent directory name from any path of the form `docs/features/<dir>/...` (whether it is the epic directory itself or a child feature subfolder — both point to the same epic).
2. Collect every candidate. Then for each candidate, check `test -f docs/features/<candidate>/epic-overview.md` — the candidate must be an **epic** (has the marker). Discard plain features.
3. **One inferred epic** — confirm with the user before writing anything:

   ```
   Inferred epic from this session: "editor"
   About to refresh docs/features/editor/epic-overview.md and the editor row in docs/overview.md.
   Proceed? (yes / choose a different epic)
   ```

   Use `AskUserQuestion` for the confirmation. Only continue to Step 2 after explicit confirmation.

4. **Multiple candidates inferred** — list them and ask the user to pick one via `AskUserQuestion`. Do not guess.

5. **No inference possible** — fall through to Step 1c.

#### 1c. Cannot determine epic — ask

If neither a named argument nor session inference yields a clear single epic:

1. Run the following to list all epics in the project:

   ```bash
   for d in docs/features/*/; do
     [ -f "$d/epic-overview.md" ] && basename "$d"
   done
   ```

2. If there are zero epics, report: "No epics found in `docs/features/`. Use `/create-epic` or `/plan-epic` to create one."
3. If there is exactly one epic, confirm with the user (same prompt as 1b step 3) before proceeding.
4. If there are multiple epics, list them and ask via `AskUserQuestion` which one to update.

---

### Step 2 — Read the current epic state

Read `docs/features/<epic>/epic-overview.md` in full. Note:

- The **"Last Updated"** timestamp (used in Step 3 for the git-log window when the epic was named rather than session-inferred).
- The **Features table** — current rows (feature name, status, task counts, description).
- The **Build Order / Dependencies** section.
- The **Integration & Architecture** section.
- The **Tech Debt / Findings** section (do not modify this section — it is `/review-epic`'s domain).
- The **Master Overview Rollup** section (status and one-line summary to sync to `docs/overview.md`).

Also read `docs/overview.md` (Step 2b) if it exists:
- Note the "Last Updated" date.
- Identify the current row for this epic in the Feature Status table.

---

### Step 3 — Gather what changed

**Choose the change-gathering strategy based on how the epic was determined:**

#### 3a. Session-inferred epic (Step 1b)

Use **session context** as the primary source. You already know which features were worked on this session (from the file paths that led to the inference). Read each touched child feature's docs directly:

- `docs/features/<epic>/<feature>/tasks.md` — current task count and completion percentage.
- `docs/features/<epic>/<feature>/context.md` — latest session notes, decisions, quick status.

Also read all **other** child features (those not worked this session) to get their current status for the Features table — you need a complete, accurate table, not just a partial update.

#### 3b. Named epic (Step 1a) — git log approach

Use `git log` to find what changed since the epic was last updated (mirroring `/update-master`'s approach):

```bash
git log --oneline --since="<last-updated-timestamp-from-epic-overview>" -- docs/features/<epic>/
```

Use the "Last Updated" value read in Step 2 as the `--since` value (convert to a git-compatible date string, e.g. `"2026-06-04 07:30"`).

Filter the resulting commits for changes relevant to child features under the epic. Then:

- Read **every** child feature's `tasks.md` and `context.md` — git log shows *what files changed*, but current status must come from reading the actual docs (task counts can drift from commit messages).

**In both cases, extract per-feature:**
- Current task count (`X/Y`) and completion percentage (`Z%`)
- Current status (`Planned` / `In Progress` / `Complete`)
- Key changes or decisions from this session / since last updated

---

### Step 4 — Update `epic-overview.md`

All changes to `epic-overview.md` are **targeted `Edit`s** — never a whole-file `Write`. Preserve every section you are not explicitly updating (especially Tech Debt / Findings).

#### 4a. Refresh the Features table

For each child feature, update its row with the actual values from Step 3:

- **Status** — derived from the feature's `context.md` Quick Status and `tasks.md` progress. Use: `Planned` (not started), `In Progress` (has incomplete tasks), `Complete` (all tasks done or feature shipped).
- **Tasks** — exact count and percentage from the feature's `tasks.md` progress line (e.g. `12/18 (67%)`). **These must match the actual `tasks.md` file.** Cross-reference — do not guess.
- **Description** — only update if the one-line description has become inaccurate; otherwise leave it as-is to preserve intentional wording.

If any child feature **does not have a row** in the Features table (e.g. a newly created feature subfolder), add a row for it.

#### 4b. Update Build Order / Dependencies (if changed)

If the session surfaced a dependency change or a reordering of features:

- Update the numbered list in the Build Order / Dependencies section with targeted `Edit`s.
- If nothing changed, leave this section untouched.

#### 4c. Update Integration & Architecture notes (if changed)

If new cross-feature integration patterns emerged:

- Add a bullet or update an existing one in the Integration & Architecture section with a targeted `Edit`.
- If nothing changed, leave this section untouched.

#### 4d. Update Master Overview Rollup

Update the **Master Overview Rollup** section with the current aggregate:

- **Rollup status** — derive from the Features table (e.g. "In Progress (1/3 features complete, 45% tasks across the epic)"). Count features by status bucket.
- **One-line summary for master** — update to reflect the current phase of work (what the epic is doing right now, concisely). This is the string that appears in `docs/overview.md`'s Notes column.

#### 4e. Update "Last Updated"

Set the `**Last Updated:**` timestamp to today's date and time (e.g. `2026-06-04 09:45`).

---

### Step 5 — Update `docs/overview.md` (the master rollup row)

If `docs/overview.md` exists, update the epic's single row in the Feature Status table using a **targeted `Edit`** — change only this epic's row, not any other row or section.

The epic's row uses the values from Step 4d's Master Overview Rollup:
- **Status** column — the rollup status (e.g. "In Progress")
- **Tasks** column — aggregate task count/% (e.g. "45/100 (45%)")
- **Notes** column — the one-line summary from the Master Overview Rollup

**Do not add or remove other rows.** Do not rewrite the file. This is a surgical single-row update.

If `docs/overview.md` does not exist, note that the row will appear after the user runs `/update-master` (which creates the file if absent).

#### 5b. Update `docs/feature-status.md` (if present)

If `docs/feature-status.md` exists, find this epic's bucketed entry and update it with the aggregate % and current status bucket. As with `docs/overview.md`, use a targeted `Edit` — do not touch other entries.

---

### Step 6 — Show summary

```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  EPIC OVERVIEW UPDATED
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Epic: {epic-name}
Last Updated: {new timestamp}

Features:
{for each feature:}
  - {feature}: {old status/count} -> {new status/count}

epic-overview.md:
  - Refreshed Features table ({N} rows updated)
  - Updated Master Overview Rollup
  - Updated Last Updated timestamp
  {if build order changed:}
  - Updated Build Order / Dependencies
  {if integration notes changed:}
  - Updated Integration & Architecture

docs/overview.md:
  - Updated {epic-name} row: {old summary} -> {new summary}
  {if feature-status.md updated:}
  - Updated {epic-name} entry in feature-status.md

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

---

## Quality Checks

Before finishing, verify:
- [ ] Every child feature's task count in the Features table matches that feature's actual `tasks.md` progress line (cross-referenced, not guessed)
- [ ] "Last Updated" in `epic-overview.md` is set to today
- [ ] The Master Overview Rollup accurately reflects the aggregate state
- [ ] `docs/overview.md` shows exactly one row for this epic (not its child features)
- [ ] The Tech Debt / Findings section was not touched (it belongs to `/review-epic`)
- [ ] All edits were targeted `Edit`s — no whole-file `Write` of `epic-overview.md` or `docs/overview.md`

---

## Important Notes

- **TARGETED EDITS ONLY** — never `Write` the whole `epic-overview.md` or `docs/overview.md`. Use `Edit` for every change. A whole-file rewrite risks clobbering the Tech Debt / Findings section, integration notes, and other rows in the master overview.
- **TASK COUNTS MUST MATCH** — always read the actual `tasks.md` file; do not carry over stale counts from the existing Features table.
- **TECH DEBT IS READ-ONLY HERE** — the Tech Debt / Findings section in `epic-overview.md` is populated by `/review-epic`. Do not add to or remove from it in this command.
- **CONFIRM BEFORE WRITING (session-inferred only)** — when the epic is inferred from session context, always confirm with the user before making any changes. Never infer and write silently.
- **ONE ROW IN MASTER** — the epic appears as exactly one row in `docs/overview.md`. Its child features (`<epic>/<feature>`) are never listed in the master overview — they live inside `epic-overview.md`.
- **GIT LOG WINDOW** — for named epics, the git log `--since` date should match the "Last Updated" timestamp in `epic-overview.md`. If the timestamp is missing or unparseable, fall back to the session context approach.

---

## Error Handling

- **`$ARGUMENTS` is a feature path (`editor/base`):** Report that `editor/base` is a feature inside an epic. Suggest `/update-feature editor/base` to update the feature's own docs, and `/update-epic editor` to refresh the epic's rollup.
- **Named epic not found:** List the available epics (dirs with `epic-overview.md`); ask which one to update.
- **No child features found under the epic:** Report the issue and suggest running `/plan-epic <name>` or `/create-epic <name>` to add features.
- **Child feature is missing `tasks.md` or `context.md`:** Skip that feature's row update and note the missing file in the summary. Suggest `/start-feature <epic>/<feature>` to scaffold the docs.
- **`docs/overview.md` exists but has no row for this epic:** Add one using the Master Overview Rollup values. Note the addition in the summary.
- **`docs/overview.md` does not exist:** Skip the master row update; note that it will appear when the user runs `/update-master`.
- **Session inference ambiguous (multiple epics touched):** List the candidates and ask the user to pick one via `AskUserQuestion`. Do not update multiple epics in one run.
