---
description: "Update project master overview with latest feature progress, status, and changelog. Usage: /update-master [feature-name]"
---

You have been asked to update the project's master overview documentation with the most recent changes.

**Feature name (optional):** `$ARGUMENTS`

## Epic awareness

This command is epic-aware. Before resolving `$ARGUMENTS` to a feature path,
read `.claude/epic-resolution.md` and apply its rules: detect epics by
`epic-overview.md`, resolve `<epic>/<feature>` references, fuzzy-fall-back into
epic folders for bare names, and roll an epic up to a single row (read its `epic-overview.md`; never list the epic's child features individually).
If no epics exist in `docs/features/`, behave exactly as before.

`.claude/epic-resolution.md` is the single source of truth for epic detection and resolution. The master overview tracks each **epic as one row** (a rollup); its child features live in the epic's `epic-overview.md`, never as separate master rows. See the **Epic rollup rule** below for how this changes Steps 1, 3, 4, and 4b. This command's body is the master-update workflow; it does **not** re-specify the detection/resolution rules.

### Epic rollup rule (applies throughout this command when epics are present)

A directory under `docs/features/` that contains `epic-overview.md` is an **epic**, and the master overview represents it as **exactly one row** — never its child features:

- **Detecting epics while scanning:** in any scan of `docs/features/*/`, treat a dir as an epic iff it has `epic-overview.md`. An epic's child feature subfolders (`docs/features/<epic>/<feature>/`) are **not** master-overview features — do **not** add a row for `<epic>/<feature>`; they are tracked inside the epic's `epic-overview.md` (and updated by `/update-epic`).
- **The epic's row** = a rollup. Read the epic's `docs/features/<epic>/epic-overview.md` — specifically its **Master Overview Rollup** section and Features table — to derive the row's **Status** (rolled up across the epic's features), **Tasks** (aggregate count/%, e.g. summed across child features), and **Notes** (a one-line summary of the epic). Do not re-derive these by reading each child's `tasks.md` from the master pass; the epic owns that detail.
- **`$ARGUMENTS` resolution:** if `$ARGUMENTS` names an epic (bare name with the marker), update that **epic's single row**. If `$ARGUMENTS` is `<epic>/<feature>`, the master overview itself does not change (that feature is internal to the epic) — point the user at `/update-epic <epic>` to refresh the epic's rollup, then re-run `/update-master` to reflect the new rollup in the epic row.
- **Plain features are unchanged** — a top-level dir with no `epic-overview.md` stays an individual row exactly as before.
- **If an epic has no `epic-overview.md` rollup data yet** (freshly created), fall back to summarizing from its Features table / child statuses, and suggest running `/update-epic <epic>` to populate the rollup.

## Purpose

The master overview tracks the project as a whole — interconnected features, integrations, and overall progress. Individual feature docs (`docs/features/*/`) are updated per-session via `/update-feature`, but the master overview can fall behind. This command syncs it.

**Master doc:** `docs/overview.md`

If `docs/overview.md` does not exist, this command will create it (see Step 4a).

---

## Process

### 1. Determine Scope

- If `$ARGUMENTS` is provided, focus on that specific feature
- If `$ARGUMENTS` is empty, scan ALL features for recent changes:
  - List all `docs/features/*/` directories
  - Compare each feature's `context.md` "Last Updated" timestamp against overview.md's "Last Updated"
  - Report which features have changed since the last master update
  - **Epics (see the Epic rollup rule):** treat a dir with `epic-overview.md` as **one** epic unit — compare the epic's `epic-overview.md` "Last Updated" (and its children, via the epic) rather than listing each `<epic>/<feature>` separately. Never scan an epic's child subfolders as top-level master features.

### 2. Read Current Master State

Read `docs/overview.md` if it exists:
- Note the "Last Updated" date
- Note the version number (if versioned)
- Note existing feature status table entries

If it doesn't exist, note that it will be created.

### 3. Gather Feature Updates

For each changed feature (or the specified `$ARGUMENTS` feature):

**If the changed item is an epic** (dir with `epic-overview.md`) — per the Epic rollup rule — read its `docs/features/<epic>/epic-overview.md` instead (Master Overview Rollup + Features table) to get the epic's rolled-up status, aggregate task count/%, and one-line summary. Use that for the epic's single master row in Step 4. Do **not** read or list its child feature subfolders as separate master entries. Then skip the plain-feature reads below for that epic.

**Read the feature's docs (plain features):**
- `docs/features/<feature>/context.md` — latest decisions, session notes, status
- `docs/features/<feature>/tasks.md` — task counts, completion percentage
- `docs/features/<feature>/implementation.md` — phase status (if exists)

**Extract key information:**
- Current task count (X/Y tasks)
- Completion percentage
- Recent session notes (what changed)
- Important decisions made
- New integrations or cross-feature impacts
- Any schema changes or migrations

**Also check git log for recent changes:**
- Run `git log --oneline --since="<master-last-updated>"` to find commits since last master update
- Filter for commits relevant to the features being updated
- Identify key commits that affect the master overview

### 4. Update overview.md

If `docs/overview.md` exists, update it in place using the Edit tool. If it does not exist, create it using step 4a.

**Update "Last Updated" date:**
- Set to today's date

**Update Feature Status Table:**
- Update task counts for changed features
- Update status column if feature moved between states (e.g., In Progress → Complete)
- Update Notes column with key additions
- Add rows for any new features not yet in the table
- **Epics = one row each** (Epic rollup rule): an epic's row uses the rollup from its `epic-overview.md` (status, aggregate tasks/%, one-line summary). Never add or keep separate rows for an epic's child features (`<epic>/<feature>`). If the table still has individual rows for features that have since been absorbed into an epic, that migration is `/create-epic`'s job — do not add them here.

**Update Feature Descriptions (if present):**
- For each changed feature, update its description block with:
  - New capabilities added
  - Updated task counts
  - Updated implementation references

**Update Integration Points (if applicable):**
- Add any new cross-feature integrations discovered
- Update integration descriptions

**Add Changelog Entry:**
- Increment version (e.g., v1.3 → v1.4)
- Add new entry at the TOP of the changelog (newest first)
- Format: `- v{version} ({date}): **{Feature Name} {summary}** - {details}`
- Include: task counts, key changes, notable additions
- Keep it concise but comprehensive (1-3 lines)

#### 4a. Create overview.md (if it doesn't exist)

If no master overview exists, create `docs/overview.md` with this structure:

```markdown
# [Project Name] - Overview

**Last Updated:** [today's date]
**Version:** v1.0

## Project Summary

[Read CLAUDE.md and docs/mission-statement.md (if exists) to write a 2-3 sentence project summary]

## Feature Status

| Feature | Status | Tasks | Notes |
|---------|--------|-------|-------|
| [feature-name] | [Complete/In Progress/Planned] | [X/Y (Z%)] | [Key notes] |
| ... | | | |

## Integration Points

[Cross-feature dependencies and connections discovered from reading feature docs]

## Changelog

- v1.0 ([date]): **Initial overview** - Created master overview tracking [N] features
```

Populate it by reading all `docs/features/*/tasks.md` and `context.md` files.

### 4b. (Optional but recommended) Sync a lightweight flat index doc

`overview.md` grows verbose fast on projects with many features. Maintaining a single flat companion doc alongside it gives the user a scannable "how done is everything" view they can glance at without reading the full master. If `docs/feature-status.md` exists, refresh it in the same pass (treat the two as a set); on large projects, consider creating it.

**`docs/feature-status.md`** — a flat roster of every feature dir, bucketed by completeness tier (e.g. Done/Live, Shippable 80-99%, In progress, Planning), each row showing `Tasks X/Y (%)` pulled from that feature's own `tasks.md`. **An epic is one bucketed entry** (Epic rollup rule): list the epic once with its **aggregate** % (from its `epic-overview.md` rollup / summed child tasks), bucketed by the epic's overall state — **do not** list the epic's child features as separate rows. Plain features stay individual rows. A quick way to regenerate the raw data:

```bash
for d in docs/features/*/; do f=$(basename "$d")
  if [ -f "$d/epic-overview.md" ]; then        # EPIC → one aggregate entry, not its children
    prog=$(grep -m1 -iE "progress|rollup" "$d"epic-overview.md 2>/dev/null | sed -E 's/\*\*//g; s/(Progress|Rollup):?[[:space:]]*//I')
    echo "$f (epic) :: ${prog:-see epic-overview.md}"
  else                                          # plain feature → its own tasks.md
    prog=$(grep -m1 -iE "progress" "$d"tasks.md 2>/dev/null | sed -E 's/\*\*//g; s/Progress:?[[:space:]]*//I')
    echo "$f :: ${prog:-n/a}"
  fi
done
```

Gotcha: **checkbox drift** — some features ship but their `tasks.md` boxes are never ticked, so the raw % understates reality. Bucket those by their true (shipped) state and mark them, rather than trusting a stale 0%.

### 5. Show Summary

Display what was updated:
```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  MASTER OVERVIEW UPDATED
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Version: v{old} -> v{new}
Last Updated: {date}

Features Updated:
- {feature-1}: {old-count} -> {new-count} tasks ({percentage}%)
- {feature-2}: {summary of changes}

overview.md:
- Updated feature status table
- Updated {N} feature sections
- Added changelog entry v{new}

Key Changes:
- {Notable change 1}
- {Notable change 2}

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

---

## Quality Checks

Before finishing, verify:
- [ ] Version number incremented correctly
- [ ] "Last Updated" timestamp set to today
- [ ] Task counts match feature docs (cross-reference tasks.md)
- [ ] No features missing from status table
- [ ] Changelog entry accurately describes changes
- [ ] Cross-feature integrations noted where applicable
- [ ] Each epic appears as exactly **one** row (rollup from its `epic-overview.md`); no `<epic>/<feature>` child rows in the table or the flat index
- [ ] If a flat index doc (`docs/feature-status.md`) exists, it was refreshed alongside overview.md (see Step 4b)

## Important Notes

- **BE ACCURATE:** Cross-reference feature task counts — don't guess, read the actual tasks.md files
- **BE CONSISTENT:** Use the same format as existing entries in tables and changelog
- **PRESERVE STRUCTURE:** Use Edit tool for targeted updates — don't rewrite entire files
- **CROSS-CUTTING AWARENESS:** Some features touch multiple other features — note integrations
- **CHANGELOG IS CRITICAL:** This is the audit trail — include key changes and component names
- **GIT LOG IS USEFUL:** Check recent commits for changes that may not be reflected in feature docs yet

## Error Handling

- If `$ARGUMENTS` is provided but feature doesn't exist:
  - List available features in `docs/features/`
  - Ask which one to update for
- If no features have changed since last master update:
  - Report "Master overview is up to date" with last updated date
  - Ask if user wants to force an update anyway
- If feature docs are missing (no context.md or tasks.md):
  - Skip that feature with a warning
  - Suggest running `/start-feature` or `/discover-feature` for it
- If overview.md doesn't exist:
  - Create it automatically (step 4a)
- If overview.md has structural issues:
  - Fix formatting while updating
  - Note fixes in changelog
