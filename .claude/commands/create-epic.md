---
description: "Group existing prefixed features into an epic (move + un-prefix + rewrite references) with a mandatory preview/confirm. Usage: /create-epic <epic-name> <prompt>"
---

You have been asked to create an **epic** by migrating existing related features into a single epic folder.

**Epic name:** First argument from `$ARGUMENTS` (kebab-case epic folder, e.g. `editor`)
**Prompt:** Remaining arguments from `$ARGUMENTS` — what part of the system this epic covers (e.g. "the rich-text editor system")

> This command is the **migration path**: it moves existing feature folders into `docs/features/<epic-name>/`, un-prefixes them (`editor-base` → `base`), rewrites every reference to the old names, creates the epic's marker docs, and folds the migrated features' master-overview rows into a single epic row. To create an epic **from scratch** (no existing features to absorb), use `/plan-epic` instead.

> **⚠ This is a destructive, hard-to-reverse operation** (folders move, names change, references are rewritten across the repo, `docs/overview.md` is edited). It is **safe by construction**: it computes and previews the ENTIRE plan and modifies **nothing** until you explicitly confirm via `AskUserQuestion` (Step 4). Read Step 4 before anything is written.

## Epic awareness

This command is epic-aware. Before resolving `$ARGUMENTS` to a feature path,
read `.claude/epic-resolution.md` and apply its rules: detect epics by
`epic-overview.md`, resolve `<epic>/<feature>` references, fuzzy-fall-back into
epic folders for bare names, and the target is the epic itself; if a target that is already an epic is passed to `/create-epic`, error clearly instead of nesting.
If no epics exist in `docs/features/`, behave exactly as before.

`.claude/epic-resolution.md` is the single source of truth for epic detection and the reference-rewrite convention — **read it now** and apply **§5 (the `/create-epic` reference-rewrite convention)** throughout this command (word-boundary-aware grep, preview every hit, rewrite to `<epic>/<feature>` or bare, surface collisions / over-generic names, apply only after confirm). This command's body is the migration *workflow* (discover → preview → confirm → apply); it does **not** re-specify those rules.

---

## Process

### 1. Parse Arguments & Verify the Target Is Not Already an Epic

Parse `$ARGUMENTS`:
- **Epic name** (required): the first word/phrase, normalized to kebab-case (e.g. `editor`). This becomes `docs/features/<epic-name>/`.
- **Prompt** (recommended): everything after the name — a short description of what the epic covers. Used to seed `epic-requirements.md` and to help judge candidate relevance.

If `$ARGUMENTS` is empty, ask the user for the epic name and the prompt before continuing.

**Guard against nesting / clobbering** (per `.claude/epic-resolution.md` §1):

```bash
test -f "docs/features/<epic-name>/epic-overview.md"   # true → ALREADY an epic
```

- If `docs/features/<epic-name>/epic-overview.md` **already exists**, the target is **already an epic**. **Stop with a clear error** — do not nest, do not re-migrate:

  ```
  ✗ "<epic-name>" is already an epic (docs/features/<epic-name>/epic-overview.md exists).
    To add more features to it, use /plan-epic <epic-name> or move/plan a feature under it.
    To refresh its rollup, use /update-epic <epic-name>.
  ```

- If `docs/features/<epic-name>/` exists but is a **plain feature** (a directory with `implementation.md`/`tasks.md` and **no** `epic-overview.md`), that name collides with the epic folder you'd create. Surface it as a **collision** (Step 3) — the user must rename the epic or fold that feature in deliberately. Do not silently overwrite it.

### 2. Discover Candidate Features to Absorb

Scan `docs/features/*` for features that should become children of this epic. Cast a wide net, then confirm relevance — **never auto-absorb**.

**Find candidates by prefix AND by theme:**

```bash
# Prefix-sharing candidates (the common case: editor-base, editor-ux → epic "editor")
ls -d docs/features/<epic-name>-*/ 2>/dev/null

# Full roster, to also catch theme matches that don't share the literal prefix
ls -d docs/features/*/
```

1. **Prefix match** — directories named `<epic-name>-*` (e.g. `editor-base`, `editor-ux` for epic `editor`). These are the primary candidates.
2. **Theme match** — features whose purpose matches the `<prompt>` even if the name doesn't share the prefix (e.g. `rich-text-toolbar` for an "editor" epic). Include these as candidates to *propose*, clearly marked as theme (not prefix) matches.
3. **Skip non-candidates** — directories that are **already epics** (have their own `epic-overview.md`) and the epic folder itself. Epics do not nest (`.claude/epic-resolution.md` §1).

**Confirm relevance — read each candidate's docs.** For every candidate, read at least its `context.md` (and skim `implementation.md`) to confirm it genuinely belongs in this epic. Do **not** absorb a feature just because its name matches a pattern — a `editor-config-loader` that's actually a shared utility may not belong. Note your relevance judgment for each; the user makes the final call in the Step 4 preview.

Build the candidate list as `<old-feature-name>` entries, each tagged **prefix** or **theme** and with a one-line relevance note. If **no** candidates are found, tell the user there's nothing to migrate and suggest `/plan-epic <epic-name> "<prompt>"` to create the epic from scratch instead, then stop.

### 3. Compute the Migration Plan (do NOT apply anything)

Compute the full plan **in memory only**. Nothing is written in this step.

#### 3a. Proposed folder moves

For each confirmed candidate, the move is:

```
docs/features/<old-feature-name>/   →   docs/features/<epic-name>/<new-feature-name>/
```

#### 3b. Proposed un-prefixing — with collision & over-generic detection

Compute each `<new-feature-name>` by **stripping the shared prefix**:
- `editor-base` → `base`, `editor-ux` → `ux` (strip the leading `<epic-name>-`).
- A **theme** candidate that doesn't share the prefix keeps its name unless the user chooses to rename it.

Then **detect and flag** the two failure modes from `.claude/epic-resolution.md` §5 — these **block silent application** until resolved:

- **Rename collision** — two or more candidates un-prefix to the **same** `<new-feature-name>` (e.g. `editor-base` **and** `cms-base` both → `base`), **or** the target `<new-feature-name>` would collide with a subfolder that already exists under `docs/features/<epic-name>/`. Flag each collision and require the user to pick distinct names (or keep a prefix) in Step 4.
- **Over-generic result** — a bare un-prefixed name that is too generic to stand alone: `base`, `core`, `ux`, `api`, `common`, `shared`, `utils`, `main`, and similar. Flag it; the user may **keep** it, **qualify** it (e.g. `editor-base` → `editor-core`), or **rename** it in Step 4.

Record, per candidate: old name → proposed new name, plus any **COLLISION** or **OVER-GENERIC** flag. Do **not** drop flagged candidates — surface them so the user resolves them.

#### 3c. Reference-rewrite discovery (grep, word-boundary-aware)

Apply `.claude/epic-resolution.md` §5. For **each** old feature name, find every reference across the repo with file + line + context:

```bash
grep -rn "<old-feature-name>" . --exclude-dir=.git    # repeat per old name; review EVERY hit
```

- **Prefer word-boundary-aware matching.** A naive replace of `editor-base` would also corrupt `editor-base-extras`. Treat the old name as a whole token (bounded by start/end, `/`, whitespace, quotes, or punctuation — **not** extended by `-` or alphanumerics). When a hit is an overlapping/partial name, mark it **SKIP (partial overlap)** by default and let the user opt in.
- **Determine each hit's rewrite target:**
  - A reference that names the feature as a **path** → the new nested path `<epic-name>/<new-feature-name>` (e.g. `docs/features/editor-base/...` → `docs/features/editor/base/...`, and bare path references `editor-base` → `editor/base`).
  - A reference that names the feature **on its own** where the bare name is unambiguous in context → the bare `<new-feature-name>`. **When unsure, prefer the fully-qualified `<epic-name>/<new-feature-name>` form** — it is never ambiguous.
- **Count hits per old name** so the user can sanity-check scope.
- Note that the candidate's **own moved docs** (files inside the folder being moved) may contain self-references; those move with the folder and are rewritten in place during apply.

#### 3d. Master-overview migration (compute the targeted edits)

Determine exactly which lines of `docs/overview.md` change — **as targeted `Edit`s, never a whole-file rewrite**:

- **Rows to remove:** the individual Feature Status row for **each** absorbed feature (e.g. the `editor-base` and `editor-ux` rows). Also note their description blocks / integration-point mentions, if present.
- **Row to add:** **one** epic row for `<epic-name>`. Status = **rollup** of its features (e.g. "In Progress (1/2 features complete)"); Notes = a one-line summary of the epic (from the `<prompt>` + what the absorbed features do). This single row replaces all the removed child rows.
- **Flat index (if present):** if `docs/feature-status.md` exists, plan to bucket the epic as **one** entry with an aggregate % (derived from the children's `tasks.md`), removing the children's individual entries. If it doesn't exist, skip it.

Read `docs/overview.md` now (if it exists) to identify the exact rows. If `docs/overview.md` does not exist, note that there are no master rows to migrate (the epic row can be added when the user next runs `/update-master`).

### 4. PREVIEW + CONFIRM (MANDATORY — nothing has been modified yet)

**This gate is non-negotiable. Up to this point you have written NOTHING. Do not move a folder, rename anything, rewrite a reference, or edit `docs/overview.md` until the user explicitly confirms below.**

Present the **complete** plan in one preview so the user sees the full blast radius at once:

```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  /create-epic <epic-name> — MIGRATION PREVIEW  (nothing applied yet)
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Epic:    <epic-name>
Covers:  <prompt>

📁 FOLDER MOVES + RENAMES (un-prefixing)
  docs/features/editor-base/  →  docs/features/<epic-name>/base
  docs/features/editor-ux/    →  docs/features/<epic-name>/ux
  [theme] docs/features/rich-text-toolbar/ → docs/features/<epic-name>/rich-text-toolbar  (name kept)

⚠ NEEDS RESOLUTION (blocks apply until fixed):
  ✗ COLLISION    — editor-base AND cms-base both un-prefix to "base" → pick distinct names
  ⚠ OVER-GENERIC — "base" is generic; keep / qualify (e.g. "editor-core") / rename?

🔁 REFERENCE REWRITES  (grep hits — deselect any you don't want)
  editor-base  (N hits)
    docs/overview.md:42        | editor-base | In Progress | ...   →  editor/base
    README.md:18               see docs/features/editor-base/...   →  docs/features/editor/base/...
    SKIP (partial overlap)     editor-base-extras                  →  (left unchanged)
  editor-ux  (M hits)
    ...

📊 MASTER OVERVIEW (docs/overview.md — targeted Edits only)
  REMOVE row:  | editor-base | ... |
  REMOVE row:  | editor-ux   | ... |
  ADD    row:  | <epic-name> | In Progress (1/2) | <aggregate> | <one-line epic summary> |
  (docs/feature-status.md present → bucket <epic-name> as one entry; else skipped)

Totals: <X> folders to move, <Y> renames, <Z> reference rewrites, <C> collisions, <G> over-generic, master rows -<R>/+1
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

Then use **`AskUserQuestion`** to get an explicit decision. If there are **any** unresolved collisions or over-generic names, the confirm path is **disabled** until they're resolved — make that clear:

```
Review the migration plan above. Nothing has been changed yet.

  • Confirm & apply — execute the moves, renames, reference rewrites, and master migration exactly as previewed
       (only offered once all COLLISION / OVER-GENERIC items are resolved)
  • Adjust names — change one or more un-prefixed names (resolve a collision, qualify an over-generic name, keep a prefix)
  • Adjust scope — add/remove a candidate feature, or deselect specific reference-rewrite hits
  • Cancel — change nothing and stop
```

- **Adjust names / scope** → apply the change, **recompute Step 3**, and re-present the preview. Loop until the plan is clean and the user confirms. (A collision or over-generic flag must be resolved — there is no "apply anyway" for an unresolved collision.)
- **Cancel** → stop immediately; nothing was modified.
- **Confirm & apply** → and only then proceed to Step 5.

### 5. Apply the Migration (only after explicit confirm)

Execute the confirmed plan in this order. Work surgically; preserve everything not in the plan.

1. **Create the epic folder:** `mkdir -p docs/features/<epic-name>`.
2. **Move + un-prefix each feature:** for each candidate, `git mv docs/features/<old-feature-name> docs/features/<epic-name>/<new-feature-name>` (use `git mv` when the repo is a git repo so history is preserved; fall back to `mv` otherwise). This relocates `implementation.md` / `context.md` / `tasks.md` (and any `requirements.md`) intact.
3. **Rewrite references** (the confirmed, non-deselected hits from Step 3c), including self-references now living inside the moved folders. Use targeted `Edit`s per hit (word-boundary-aware) — rewrite to `<epic-name>/<new-feature-name>` (path form) or the bare new name as decided. Do **not** touch deselected or partial-overlap hits.
4. **Create `epic-overview.md`** from the template at **`.claude/templates/epic-overview.md`** at `docs/features/<epic-name>/epic-overview.md`:
   - Fill the **Purpose** from the `<prompt>`.
   - Populate the **Features** table with one row per absorbed feature — fold in each feature's migrated master-overview entry/summary and its real status + task counts (read each child's `tasks.md`). Order them sensibly (foundational first); the user can refine via `/plan-epic` or `/update-epic`.
   - Seed **Build Order / Dependencies** and **Integration & Architecture** from what the candidates' docs revealed (leave a clear placeholder where unknown).
   - Populate the **Master Overview Rollup** field with the rollup status + the one-line master summary you'll write into `docs/overview.md` (Step 6) so the two stay in sync.
   - Set **Last Updated** to the current timestamp.
5. **Create `epic-requirements.md`** from the template at **`.claude/templates/epic-requirements.md`** at `docs/features/<epic-name>/epic-requirements.md`, seeded from the `<prompt>` and the absorbed features (list them under **Planned Features** / Overview). This is the epic-level requirements doc; it's lightweight — `/plan-epic` can flesh it out later.
6. **Migrate `docs/overview.md`** using **targeted `Edit`s ONLY — never `Write` the whole file** (data-loss risk; see `.claude/epic-resolution.md` §5 and the Architecture's "How the master overview changes"):
   - Remove each absorbed feature's individual row (and its description-block/integration mention, if present), one targeted `Edit` per removal.
   - Add the single `<epic-name>` epic row (rollup status + one-line summary) — matching the table's existing column format.
   - If `docs/overview.md` doesn't exist, skip (note it; the epic row appears when the user next runs `/update-master`).
7. **Refresh `docs/feature-status.md`** *if it exists*: remove the children's entries and add one `<epic-name>` entry with an aggregate % (targeted `Edit`s). If it doesn't exist, skip.

### 6. Report Summary & Suggest Next Steps

Show what was actually applied (counts, not guesses — derive from what you did):

```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
✅ EPIC CREATED: <epic-name>
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

📁 Epic: docs/features/<epic-name>/
   Marker: epic-overview.md ✓   Requirements: epic-requirements.md ✓

🔀 Folders moved + un-prefixed (<X>):
   editor-base → <epic-name>/base
   editor-ux   → <epic-name>/ux

🔁 References rewritten: <Z> hits across <F> files

📊 Master overview: removed <R> child rows, added 1 epic row (<epic-name>)
   (docs/feature-status.md: <updated / not present>)

Next steps:
  • /update-epic <epic-name>            — refresh the epic rollup any time after working its features
  • /plan-epic <epic-name>              — add more features to the epic (plans + scaffolds new ones)
  • /continue-feature <epic-name>       — see a whole-epic summary
  • /continue-feature <epic-name>/base  — resume a specific feature inside the epic
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

---

## Important Notes

- **NOTHING IS MODIFIED BEFORE STEP 4'S EXPLICIT CONFIRM.** Steps 1–3 only read and compute. Folder moves, renames, reference rewrites, and master edits happen **only** in Step 5, after the user confirms.
- **COLLISIONS & OVER-GENERIC NAMES BLOCK SILENT APPLICATION.** A rename collision (two features → same name, or a name that already exists) or an over-generic result (`base`, `core`, `ux`, …) is surfaced in the preview and must be resolved before the "Confirm & apply" path is offered. There is no "apply anyway".
- **MASTER EDITS ARE SURGICAL.** Use targeted `Edit`s on `docs/overview.md` (remove child rows, add one epic row) — **never `Write` the whole file**. Preview the master diff in Step 4; only the planned rows change.
- **PRESERVE HISTORY.** Prefer `git mv` over `mv` in a git repo so the moved feature folders keep their history.
- **WORD-BOUNDARY-AWARE REWRITES.** Never blind-replace a substring — `editor-base` must not corrupt `editor-base-extras`. Preview every hit; default partial overlaps to SKIP.
- **READ CANDIDATES BEFORE ABSORBING.** A name/prefix match is a *candidate*, not a decision — confirm relevance from each feature's docs; the user makes the final call.
- **DON'T NEST.** Epics contain features only. A target that is already an epic errors in Step 1; candidate scanning skips dirs that are themselves epics.
- **DRY:** the detection + rewrite *rules* live only in `.claude/epic-resolution.md`. This command is the workflow; it points at that file rather than re-specifying the rules.

## Error Handling

- **Empty `$ARGUMENTS`:** ask for the epic name and prompt before proceeding.
- **Target already an epic** (`epic-overview.md` present): stop with the clear "already an epic" error (Step 1) — never nest or re-migrate.
- **Epic name collides with an existing plain feature** (`docs/features/<epic-name>/` exists without `epic-overview.md`): surface as a collision in the preview; the user renames the epic or folds that feature in deliberately. Never overwrite it.
- **No candidate features found:** tell the user there's nothing to migrate; suggest `/plan-epic <epic-name> "<prompt>"` to build the epic from scratch, then stop.
- **Unresolved collision / over-generic at confirm time:** do not offer "Confirm & apply"; require the user to adjust names first (loop back to the preview).
- **`docs/overview.md` missing:** skip the master migration; note that the epic row will appear on the next `/update-master`. Still create the epic folder + marker docs and move/rewrite the features.
- **A `git mv` fails** (not a git repo, or path issue): fall back to `mv`; if a move fails mid-apply, stop and report exactly what moved and what didn't so the user can recover — do not continue rewriting references for a folder that didn't move.
- **A candidate's docs are missing** (`context.md`/`implementation.md` absent): still allow migrating the folder, but warn that its `epic-overview.md` Features-table row will be sparse; suggest `/update-epic <epic-name>` afterward.
