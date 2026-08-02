---
description: "Review an epic holistically -- cross-feature integration, architecture fit, and per-feature tech debt. Usage: /review-epic <name>"
---

You have been asked to review an **epic** holistically -- how its features integrate with each other and with the rest of the project, and what tech debt each feature carries.

**Epic name:** `$ARGUMENTS`

## Epic awareness

This command is epic-aware. Before resolving `$ARGUMENTS` to a feature path,
read `.claude/epic-resolution.md` and apply its rules: detect epics by
`epic-overview.md`, resolve `<epic>/<feature>` references, fuzzy-fall-back into
epic folders for bare names, and the target must be an epic; review it holistically across all its features (integration + per-feature tech debt) and write findings into `epic-overview.md` and child `context.md` files.
If no epics exist in `docs/features/`, behave exactly as before.

`.claude/epic-resolution.md` is the single source of truth for epic detection and resolution. This command's body is the review-epic workflow; it does **not** re-specify the detection/resolution rules.

## What this reviews (and how it differs from /review-feature)

`/review-epic` is the **architecture / integration / tech-debt** pass *across* an epic. `/review-feature <epic>/<feature>` (or `/review-feature <epic>`) is the **deep code** pass *within* a feature. They are complementary -- run `/review-epic` to see how the pieces fit, then `/review-feature` to scrutinise the code of a specific piece.

| | `/review-epic <epic>` | `/review-feature <epic>/<feature>` |
|---|---|---|
| **Asks** | Do the epic's features fit together, and what tech debt does each carry? | Is this feature's **code** well-written (Vercel React best practices)? |
| **Scope** | The whole epic -- cross-feature integration, architecture, contracts, per-feature debt | One feature's source files, line by line |
| **Reads** | `epic-overview.md` + every child feature's docs (not a line-by-line code pass) | One feature's `context.md` Key Files + their source |
| **Writes to** | `epic-overview.md` **Tech Debt / Findings** + each affected child `context.md` | `docs/features/<epic>/<feature>/code-review.md` (CR-N findings) |
| **Finding style** | Cross-feature integration issues + per-feature tech-debt items | CR-N code findings with concrete fix snippets |

**This is NOT a line-by-line code review of one feature.** Stay at the architecture/integration level: how features connect, where contracts drift, which feature carries debt that will hurt the others. For a deep code pass on a specific feature, point the user at `/review-feature <epic>/<feature>`.

## CRITICAL RULE: Report + record findings, do not fix

**NEVER implement fixes during this review.** This command writes findings into the epic's docs and reports a summary -- it does not change feature code. Fixing is a separate, user-approved step (`/proceed`, `/fix-feature`).

## Process

### Step 1 — Resolve the epic (must have `epic-overview.md`)

1. Resolve `$ARGUMENTS` per `.claude/epic-resolution.md`. The target **must** be an epic — a directory with `docs/features/$ARGUMENTS/epic-overview.md`.
2. **If `$ARGUMENTS` is empty:** list the project's epics and ask which to review:

   ```bash
   for d in docs/features/*/; do [ -f "$d/epic-overview.md" ] && basename "$d"; done
   ```

   If there are zero epics, report: "No epics found in `docs/features/`. `/review-epic` reviews an epic; for a single feature use `/review-feature <name>`." Then stop.
3. **If `$ARGUMENTS` is a plain feature** (`docs/features/$ARGUMENTS/` exists with `implementation.md` but **no** `epic-overview.md`): do not treat it as an epic. Report clearly and redirect:

   ```
   ✗ "$ARGUMENTS" is a plain feature, not an epic (no epic-overview.md).
     • To review its code:        /review-feature $ARGUMENTS
     • To review its UX / UI:      /review-ux $ARGUMENTS  ·  /review-ui $ARGUMENTS
     • To group features into an epic first: /create-epic <name> "<what it covers>"
   ```

   Then stop.
4. **If `$ARGUMENTS` is a slash form** (`editor/base`): that names a **feature inside** an epic, not the epic. Report and redirect: "`editor/base` is a feature inside the `editor` epic. To review the whole epic run `/review-epic editor`. To review that one feature's code run `/review-feature editor/base`." Then stop.
5. **If `$ARGUMENTS` is not found anywhere:** fall through to the existing not-found handling (list available epics + features).

### Step 2 — Load the epic + every child feature's context

Read the epic in full so the review is grounded in its intended architecture and current state. Per `.claude/epic-resolution.md` §4, read in this order:

1. `docs/features/<epic>/epic-overview.md` — Purpose, Features table (build order + status), **Build Order / Dependencies**, **Integration & Architecture**, the existing **Tech Debt / Findings** (so you append rather than duplicate), and the Master Overview Rollup.
2. `docs/features/<epic>/epic-requirements.md` if present — the epic's intended scope and planned feature set.
3. **Every** child feature subfolder under `docs/features/<epic>/`. For each feature read at least:
   - `implementation.md` — what was planned and the architecture it assumes.
   - `context.md` — Key Files, decisions, gotchas, current state.
   - `tasks.md` — what is built vs outstanding.
   - `requirements.md` (for not-yet-built features) — intended scope.

   Enumerate the children:

   ```bash
   for f in docs/features/<epic>/*/; do [ -f "$f/implementation.md" ] || [ -f "$f/requirements.md" ] && basename "$f"; done
   ```

   Skip `epic-overview.md`/`epic-requirements.md` (they are files, not feature subfolders). Read enough of each feature's **Key Files** (from its `context.md`) to judge integration and debt — but stay at the architecture level; this is not a per-line code pass.

### Step 3 — Review holistically (cross-feature + per-feature tech debt)

Produce two kinds of findings. Keep them distinct.

#### 3a. Cross-feature integration & architecture findings

How do the features fit together — and where don't they? Look for:

- **Contract drift between features** — feature B consumes a type/route/event/prop that feature A defines differently (or that A has since changed). Shared modules whose shape one feature assumes and another contradicts.
- **Duplicated or divergent implementations** — two features solving the same problem two ways (two HTTP clients, two state stores, two date utils) where one shared approach was intended.
- **Build-order / dependency violations** — a feature relies on something a *later* (per the build order) feature owns, or two features have a cyclic dependency.
- **Integration gaps vs the epic's intended architecture** — the `epic-overview.md` "Integration & Architecture" section says the features connect via X, but the code wires them via Y (or not at all).
- **Cross-epic / cross-project integration** — how this epic touches *other* epics or plain features: shared boundaries, leaked internals, missing seams the deferred work (e.g. Web/Discord) will need.
- **Epic-level architectural risks** — a decision made in one feature that constrains or contradicts the epic's stated key decisions.

Each cross-feature finding names **which features it spans** (e.g. `base ↔ realtime`) so it is actionable.

#### 3b. Per-feature tech debt

For **each** feature, identify the tech debt that will slow the epic down: shortcuts taken, TODOs that became load-bearing, missing error/loading/empty handling, untyped boundaries, tests skipped, a pattern that diverges from the rest of the epic. Attribute every item to its feature.

This is architecture/quality-level debt, not a CR-N code-line audit — note *what* the debt is and *why it matters to the epic*, and suggest `/review-feature <epic>/<feature>` when a finding warrants a deep code pass.

### Step 4 — Write findings into the epic's docs (TARGETED EDITS ONLY)

Record the findings so future work picks them up. **Use targeted `Edit` tool calls — never a whole-file `Write`.** A whole-file rewrite risks clobbering the Features table, build order, integration notes, and other content you did not author. Preserve every section you are not explicitly updating.

#### 4a. `epic-overview.md` → **Tech Debt / Findings** section

Append your findings to the existing **Tech Debt / Findings** section (do not replace the rest of the file). This section is `/review-epic`'s domain — `/update-epic` leaves it alone, so you own it. Use the template's bullet shape and keep per-feature attribution explicit:

```markdown
## Tech Debt / Findings

> Last reviewed: [date] by /review-epic.

**Cross-feature / integration**
- [ ] 🔗 **[Integration finding title]** — [feature-A ↔ feature-B] — [what's misaligned and why it matters; recommended follow-up].

**Per-feature tech debt**
- [ ] 💳 **[Debt title]** — [feature-name] — [what it is, why it matters to the epic; suggest `/review-feature <epic>/<feature>` for a deep code pass if warranted].
```

Keep any pre-existing items in this section (re-tick or update status if you re-confirmed them; don't silently drop them). If the section currently holds only the placeholder line (`(none yet — …)`), replace just that placeholder line with your findings.

#### 4b. Each affected child `context.md`

For **every** feature that has a finding against it, add the relevant item to that feature's own `docs/features/<epic>/<feature>/context.md` so per-feature work surfaces it — via a **targeted `Edit`** to the appropriate existing section:

- A **tech-debt / gotcha** finding → add to the feature's **Gotchas & Learnings** (or a **Technical Debt** subsection if one exists).
- An **integration** finding the feature must honour → add to its **Important Decisions** (as a constraint from the epic review) and/or its **Next Steps / What's Next**.
- A finding that should be the next thing tackled → add to **Next Steps** / Quick Status "What's Next".

Prefix each added item so its origin is clear, e.g. `**(epic review 2026-06-04):** …`. Only touch features that actually have a finding — do not edit untouched features' docs. Do **not** rewrite any `context.md` whole-file; append to the right section with `Edit`.

> Keep attribution consistent: every cross-feature finding lives in `epic-overview.md` (it spans features) **and** in each spanned feature's `context.md`. Every per-feature debt item lives in `epic-overview.md` **and** in that one feature's `context.md`. Nothing loses which feature(s) it belongs to.

### Step 5 — Report a summary + suggest follow-ups

```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
  EPIC REVIEW: {epic-name}
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Reviewed {N} features: {feature-1}, {feature-2}, {feature-3}

Cross-feature / integration findings: {C}
{for each:}
  🔗 [{feature-A} ↔ {feature-B}] {one-line finding}

Per-feature tech debt: {D}
{for each:}
  💳 [{feature}] {one-line finding}

What's already solid:
  ✅ {cross-feature thing that fits together well}

Written to:
  - docs/features/{epic}/epic-overview.md  (Tech Debt / Findings)
  - {for each affected feature:} docs/features/{epic}/{feature}/context.md

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Suggested follow-ups:
  • /review-feature {epic}/{feature}   — deep code pass on a specific feature
  • /fix-feature {epic}/{feature}      — address a finding
  • /proceed                            — work the next task with these findings in context
  • /update-epic {epic}                 — refresh the epic rollup after acting on findings
```

Note at least one thing the epic does well (the features that integrate cleanly) — a review that is all criticism is less useful and less trusted.

## Quality Checks

Before finishing, verify:
- [ ] The target was confirmed to be an epic (`epic-overview.md` present) — plain features / slash forms were redirected, not reviewed as epics
- [ ] **Every** child feature was read (implementation + context + tasks), not just a subset
- [ ] Findings stay at the architecture/integration level — this is not a line-by-line code review of one feature
- [ ] Every finding keeps per-feature attribution (cross-feature findings name the features they span)
- [ ] Findings were written into `epic-overview.md`'s **Tech Debt / Findings** section via targeted `Edit` (existing items preserved)
- [ ] Each affected feature's `context.md` got the relevant finding (and untouched features were not edited)
- [ ] No whole-file `Write` of `epic-overview.md` or any `context.md`
- [ ] No feature code was modified (findings only)

## Important Notes

- **ARCHITECTURE/INTEGRATION, NOT LINE-BY-LINE** — `/review-epic` reviews how the epic's features fit together and what debt they carry. For a deep code pass on one feature, use `/review-feature <epic>/<feature>`. State this distinction when reporting.
- **TARGETED EDITS ONLY** — never `Write` the whole `epic-overview.md` or any `context.md`. Use `Edit` for every change; preserve unread sections.
- **TECH DEBT / FINDINGS IS YOURS** — this section in `epic-overview.md` is owned by `/review-epic`. `/update-epic` reads but never writes it; you append to it here.
- **KEEP ATTRIBUTION** — every finding names the feature(s) it belongs to. Cross-feature findings name both/all sides; per-feature debt names the one feature.
- **REPORT, DON'T FIX** — record findings and report; do not implement fixes. Fixing is a separate user-approved step.
- **READ EVERY CHILD** — a partial read produces a misleading integration picture. Enumerate and read all feature subfolders.

## Error Handling

- **`$ARGUMENTS` empty:** list the project's epics (dirs with `epic-overview.md`) and ask which to review. If none exist, explain `/review-epic` is for epics and point at `/review-feature <name>` for a single feature.
- **`$ARGUMENTS` is a plain feature (no `epic-overview.md`):** do not review it as an epic. Redirect to `/review-feature` / `/review-ux` / `/review-ui`, or `/create-epic` to group features first (see Step 1.3).
- **`$ARGUMENTS` is a slash form (`editor/base`):** that is a feature inside an epic. Redirect to `/review-epic editor` (the epic) or `/review-feature editor/base` (the one feature).
- **Epic exists but has no child features:** report that there is nothing to review across; suggest `/plan-epic <name>` or `/create-epic <name>` to add features.
- **A child feature is missing `context.md`:** still review its `implementation.md`; note the missing `context.md` in the summary and skip writing per-feature findings there (record them only in `epic-overview.md`, noting the feature lacks a `context.md`).
- **`epic-overview.md` has no Tech Debt / Findings section** (older/hand-made epic): add the section (matching the `.claude/templates/epic-overview.md` shape) with your findings, via a targeted `Edit` that inserts it in the right place — do not rewrite the file.
