# Epic Resolution — Shared Reference

**This file is the single source of truth for epic detection, reference resolution, context loading, and the `/create-epic` rewrite convention.** It is installed to **`.claude/epic-resolution.md`** and is consumed by every epic-aware command via a small "Epic awareness" pointer block (see the bottom of this file). The resolution logic lives here and **only** here — commands never duplicate it, they point at it.

> **Backwards-compatibility contract:** if there are **no epics** in `docs/features/` (no directory contains an `epic-overview.md`), every rule below is a no-op and commands behave exactly as they did before epics existed. Epic behavior activates **only** when an `epic-overview.md` marker is found.

---

## 1. Epic detection (the marker rule)

A directory under `docs/features/` is an **epic** if — and **only if** — it contains an `epic-overview.md` file:

```bash
test -f "docs/features/<dir>/epic-overview.md"   # true → <dir> is an EPIC, not a plain feature
```

This marker is **load-bearing**. Do **not** infer "epic-ness" from heuristics like "has subfolders" or "name has a prefix" — those break on ordinary features that happen to have nested directories. The `epic-overview.md` file is the sole, machine-detectable signal, and it is the same contract future Web App / Discord clients will rely on.

- An epic contains **feature subfolders only** — epics do **not** nest. A feature subfolder is never itself an epic, so `<epic>/<feature>` is the deepest path you will ever resolve.
- A plain feature is a directory with the usual `implementation.md` / `context.md` / `tasks.md` and **no** `epic-overview.md`.

**Cheap project-wide check** — "does this project use epics at all?":

```bash
# Lists every epic folder (empty output → no epics → behave exactly as before)
for d in docs/features/*/; do [ -f "$d/epic-overview.md" ] && basename "$d"; done
```

---

## 2. Reference resolution for `$ARGUMENTS`

When a command receives an argument that names a feature or epic, resolve it with the following rules **before** doing any of the command's normal work. Let `NAME` be the raw argument.

### 2.1 Resolution summary table

| `$ARGUMENTS` | Resolves to | Behavior |
|---|---|---|
| `editor/base` (slash) | `docs/features/editor/base/` | feature **inside** an epic — resolve directly; **load epic context** (see §4) for plan/start/continue |
| `editor` (has `epic-overview.md`) | `docs/features/editor/` | **whole-epic mode** — operate on the epic as a unit (summary / review across all its features) |
| `user-auth` (plain, no marker) | `docs/features/user-auth/` | **unchanged** — today's plain-feature behavior |
| `base` (only under `editor/`) | `docs/features/editor/base/` | **fuzzy fallback** — single match → resolve **and announce** the `editor/base` path |
| `base` (under `editor/` **and** `cms/`) | ambiguous | **disambiguate** — list the matches, ask the user which one |
| `nonesuch` (nowhere) | — | fall through to the command's existing **"not found"** handling |

### 2.2 Resolution algorithm (step by step)

1. **Slash form `<epic>/<feature>`** (NAME contains a `/`):
   - Resolve directly to `docs/features/<epic>/<feature>/`.
   - Confirm the epic is real: `test -f docs/features/<epic>/epic-overview.md`. If the marker is missing, the `<epic>` segment is not actually an epic — fall through to the command's "not found" handling and tell the user `<epic>` has no `epic-overview.md`.
   - Confirm the feature subfolder exists (`test -d docs/features/<epic>/<feature>`). If not, list the epic's actual feature subfolders and ask.
   - This is a **feature inside an epic** → for plan / start / continue commands, **load epic context** per §4.

2. **Bare name found at top level** (`test -d docs/features/<NAME>`):
   - If `docs/features/<NAME>/epic-overview.md` exists → **it is an epic**. Enter **whole-epic mode**: the command operates on the epic as a unit (e.g. `/continue-feature` summarizes the whole epic; review commands review across all its features). Do **not** treat it as a single feature.
   - Else → **it is a plain feature**. Behave exactly as before (today's behavior). No epic context, no announcement.

3. **Bare name NOT found at top level** (`docs/features/<NAME>` does not exist):
   - Run the **fuzzy fallback** in §3.

### 2.3 Worked examples (concrete paths)

- `editor/base` → `docs/features/editor/base/` (verify `docs/features/editor/epic-overview.md` exists; load epic context for plan/start/continue).
- `editor` with `docs/features/editor/epic-overview.md` present → whole-epic mode over `docs/features/editor/` (children: `docs/features/editor/base/`, `docs/features/editor/ux/`, …).
- `user-auth` with `docs/features/user-auth/` and **no** `epic-overview.md` → plain feature, `docs/features/user-auth/`, unchanged.
- `base` where only `docs/features/editor/base/` exists → fuzzy fallback resolves to `editor/base` and announces it.
- `base` where both `docs/features/editor/base/` and `docs/features/cms/base/` exist → ambiguous; ask the user to pick `editor/base` or `cms/base`.
- `nonesuch` matching nothing top-level or nested → fall through to the command's existing not-found handling.

---

## 3. Fuzzy-fallback algorithm (bare name not at top level)

Users will type `base` when they mean `editor/base`. When a **bare** name (no slash) is **not** a top-level directory under `docs/features/`, search for it **one level deep inside epic folders** before giving up:

1. **Search** for the name as a feature subfolder inside any epic:

   ```bash
   # Every epic-nested feature matching <NAME>, printed as "<epic>/<NAME>"
   for d in docs/features/*/; do
     epic="$(basename "$d")"
     [ -f "$d/epic-overview.md" ] || continue          # only descend into real epics
     [ -d "$d/$NAME" ] && echo "$epic/$NAME"
   done
   ```

   (Only descend into directories that are themselves epics — i.e. have an `epic-overview.md`. Because epics are flat, you never search deeper than one level.)

2. **Exactly one match** → resolve to `docs/features/<epic>/<NAME>/` **and announce the resolution** so the user stays oriented, e.g.:

   ```
   ℹ Resolved "base" → editor/base (found inside the "editor" epic).
   ```

   Then continue as if the user had typed `editor/base` (including loading epic context per §4 for plan/start/continue).

3. **Multiple matches** across different epics → **do not guess**. List the matches and ask the user to disambiguate (prefer `AskUserQuestion` when the command already uses it), e.g.:

   ```
   "base" exists in more than one epic. Which did you mean?
     • editor/base
     • cms/base
   ```

4. **No matches** → **fall through** to the command's existing "feature not found" handling (list available features, suggest `/plan-feature`, etc.). Do not invent a feature.

**Never** act on a silent first-match. Single match → announce; multiple → ask; none → fall through.

---

## 4. Epic-context-loading rule

When **planning** or **working** a feature that lives **inside** an epic (resolved via slash form or fuzzy fallback), the feature must be understood in the context of the whole epic — what came before it and what comes after — so the features fit together. Before doing the command's normal work, read, in this order:

1. The epic's marker/rollup: `docs/features/<epic>/epic-overview.md` (purpose, Features table, build order, integration notes, known tech debt).
2. The epic's requirements, if present: `docs/features/<epic>/epic-requirements.md`.
3. **All sibling features'** docs — for **every** other feature subfolder under `docs/features/<epic>/`, read at least its `implementation.md` and `context.md` (and `requirements.md` if the feature is still only planned). This is what lets the new work integrate with siblings instead of contradicting them.

Apply this rule in:
- **`/plan-feature <epic>/<feature>`** — load the context above **before** invoking the solution-architect, and pass it into the architect's prompt so the plan respects the epic's architecture, build order, and siblings.
- **`/start-feature <epic>/<feature>`** — load it before scaffolding/verifying the nested docs.
- **`/continue-feature <epic>/<feature>`** — load it before summarizing, so "next steps" account for sibling progress and the epic's build order.

For **whole-epic mode** (a bare epic name), the command reads `epic-overview.md` plus each child feature's status rather than loading a single feature — see the per-command clause variants in §6.

---

## 5. The `/create-epic` reference-rewrite convention

When `/create-epic` absorbs existing prefixed features (e.g. `editor-base`, `editor-ux`) into an epic, it **un-prefixes** them (`editor-base` → `base`) and **rewrites every reference** to the old names. To keep rewrites consistent and safe, follow this convention (the full preview/confirm flow lives in `/create-epic` itself; this section defines the shared rules it applies):

1. **Discover every reference** to each old feature name across the repo, with file + line + context:

   ```bash
   grep -rn "editor-base" .   # repeat per old name; review every hit before changing anything
   ```

2. **Prefer word-boundary-aware matching.** Guard against partial / overlapping names: a naive replace of `editor-base` would also corrupt `editor-base-extras`. Match the old name as a whole token (bounded by start/end, `/`, whitespace, quotes, or punctuation — not by `-` or alphanumerics that would extend the token). When in doubt, show the surrounding context and let the user deselect a hit.

3. **Preview every hit** — never rewrite blind. Show each `file:line` with the current text and the proposed replacement. Counts per old name help the user sanity-check scope.

4. **Rewrite target:**
   - A reference that names the feature as a path becomes the new nested path: `editor-base` → `editor/base` (i.e. `<epic>/<feature>`).
   - A reference that names the feature on its own, where the new bare name is unambiguous in context, may become the bare name (`base`). When unsure, prefer the fully-qualified `<epic>/<feature>` form — it is never ambiguous.

5. **Surface collisions and over-generic results before applying:**
   - **Rename collision** — two candidates that un-prefix to the same name (`editor-base` + `cms-base` → `base`), or a target name that already exists. Stop and ask the user to choose a name or keep a prefix.
   - **Over-generic result** — a bare un-prefixed name like `base`, `core`, `ux`, `api`. Flag it; let the user keep it, qualify it, or rename.

6. **Nothing is modified before explicit confirmation.** Moves, un-prefix renames, reference rewrites, and master-overview edits are all previewed together and applied only after the user confirms (see `/create-epic`).

---

## 6. The canonical "Epic awareness" command block

Every epic-aware command pastes the block below near where it parses `$ARGUMENTS`. **This is the only thing copied into commands** — the body logic above is never duplicated. Update the wording **here**; commands inherit it by pointing back at this file.

### Base block (paste verbatim)

```markdown
## Epic awareness

This command is epic-aware. Before resolving `$ARGUMENTS` to a feature path,
read `.claude/epic-resolution.md` and apply its rules: detect epics by
`epic-overview.md`, resolve `<epic>/<feature>` references, fuzzy-fall-back into
epic folders for bare names, and (for plan/start/continue) load epic context.
If no epics exist in `docs/features/`, behave exactly as before.
```

### Per-command trailing-clause variants

Keep the block identical across commands except for one trailing clause naming which rules apply to that command. In the base block, replace the line `and (for plan/start/continue) load epic context.` with the matching clause below. (Clauses are plain prose so they paste cleanly; the inline-code tokens like `<epic>/<feature>` stay as written.)

| Command(s) | Trailing clause (replaces the `and (for plan/start/continue) load epic context.` line) |
|---|---|
| `plan-feature`, `start-feature`, `continue-feature` | and load epic context (the `epic-overview.md` + all sibling features) when the target is a feature inside an epic. |
| `continue-feature` (additional) | additionally, if the target is a bare epic name, switch to whole-epic summary mode instead of treating it as a single feature. |
| `proceed`, `proceed-advanced` | and resolve the current feature's nested `<epic>/<feature>` path when it lives inside an epic. |
| `update-feature` | and resolve `<epic>/<feature>` (auto-detect may land on a nested feature). |
| `update-master` | and roll an epic up to a single row (read its `epic-overview.md`; never list the epic's child features individually). |
| `review-feature`, `review-ux`, `review-ui`, `review-mobile` | and operate across all of the epic's features when the target is an epic (aggregate findings, keep per-feature attribution). |
| `review-epic` | the target must be an epic; review it holistically across all its features (integration + per-feature tech debt) and write findings into `epic-overview.md` and child `context.md` files. |
| `update-epic` | the target is an epic; refresh its `epic-overview.md` rollup and the epic's single row in the master overview. |
| `create-epic`, `plan-epic` | the target is the epic itself; if a target that is already an epic is passed to `/create-epic`, error clearly instead of nesting. |
| `fix-bug` | and resolve the bug's `linkedFeature` (not `$ARGUMENTS`, which is the bug ID) to its `<epic>/<feature>` path, loading epic context, when the linked feature lives inside an epic. |
| `fix-feature` | and resolve `$ARGUMENTS` and each bug's `linkedFeature`; additionally, if `$ARGUMENTS` is a bare epic name, fix open bugs across all of the epic's child features. |
| `audit-feature` | and resolve `<epic>/<feature>` (loading epic context) when the target is a feature inside an epic; descend into epic folders during the cross-feature scan; a bare epic name → ask which child feature to audit. |
| `discover-feature` | and resolve `<epic>/<feature>` so discovery creates docs at the nested path when documenting a feature inside an epic. |
| `document-feature` | and resolve `<epic>/<feature>` when documenting a feature inside an epic; a bare epic name → ask which child feature to document. |
| `evaluate-feature` | and resolve `<epic>/<feature>` (passing epic context to the evaluator) when the target is a feature inside an epic; a bare epic name → ask which child feature to evaluate. |
| `review-pr` | `$ARGUMENTS` is a PR number; when mapping changed files to feature docs (pr-notes, branch name) descend into epic folders and use the nested `<epic>/<feature>` path. |
| `suggest-feature` | takes no argument; when surveying features, treat each epic as a unit (read `epic-overview.md`, descend into children) and a suggestion may be a new feature inside an existing epic (`<epic>/<feature>`). |

> The clause only **names which rules apply** for that command — the rules themselves (detection, resolution, fuzzy fallback, context loading, rewrite convention) stay in this file. If you change a rule, change it here once and every command follows.
>
> **`continue-feature` uses two clauses** (the base feature-inside-epic clause **and** the additional whole-epic line) because it handles both `<epic>/<feature>` and a bare epic name.

---

## 7. Reference summary (quick recall)

- **Marker:** epic ⇔ `docs/features/<dir>/epic-overview.md` exists (`test -f`). No heuristics.
- **Structure:** flat — epic → feature subfolders only; `<epic>/<feature>` is the deepest path.
- **Resolve:** slash → direct; bare-with-marker → whole-epic; bare-plain → unchanged; bare-not-top-level → fuzzy fallback (one → announce, many → ask, none → fall through).
- **Context:** working a feature inside an epic → read `epic-overview.md` + `epic-requirements.md` + **all** siblings' `implementation.md` + `context.md` first.
- **Rewrite (`/create-epic`):** `grep -rn` each old name, word-boundary-aware, preview every hit, rewrite to `<epic>/<feature>` (or bare), surface collisions / over-generic names, apply only after confirm.
- **Master overview:** an epic is **one** row; its children live in the epic's `epic-overview.md`.
- **Backwards-compat:** no epics present → every rule is a no-op.
