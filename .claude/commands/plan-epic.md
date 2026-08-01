---
description: "Plan an epic from scratch - define the set of constituent features, their build order, and scaffold the epic folder with requirements. Usage: /plan-epic <epic-name> [initial requirements prompt]"
---

You have been asked to plan an **epic** — a group of related features that together deliver a coherent part of the system.

**Epic name:** First argument from `$ARGUMENTS` (kebab-case, e.g. `checkout`)
**Initial requirements prompt:** Remaining arguments from `$ARGUMENTS` (may be empty)

> This command is the **from-scratch planning path**: it works with you at the epic level (scope, the set of features, boundaries, build order), then creates `epic-overview.md`, `epic-requirements.md`, and ordered feature subfolders each with a `requirements.md` that `/plan-feature <epic>/<feature>` consumes. It does **not** create `implementation.md` / `context.md` / `tasks.md` for child features — those come from running `/plan-feature <epic>/<feature>` per feature in the recommended order.
>
> To group **existing** prefixed features into an epic, use `/create-epic` instead.

## Epic awareness

This command is epic-aware. Before resolving `$ARGUMENTS` to a feature path,
read `.claude/epic-resolution.md` and apply its rules: detect epics by
`epic-overview.md`, resolve `<epic>/<feature>` references, fuzzy-fall-back into
epic folders for bare names, and the target is the epic itself; if the target already has an `epic-overview.md`, read the existing docs and continue planning (this command is re-runnable and additive — it refines an existing epic rather than erroring).
If no epics exist in `docs/features/`, behave exactly as before.

`.claude/epic-resolution.md` is the single source of truth for epic detection and resolution rules. This command's body is the planning workflow; it does **not** re-specify those rules.

---

## Process

### 1. Parse Arguments

Parse `$ARGUMENTS` to extract:
- **Epic name** (required): first word/phrase, normalized to kebab-case. This becomes `docs/features/<epic-name>/`.
- **Initial requirements prompt** (optional): everything after the epic name.

Examples:
- `/plan-epic checkout` → name: `checkout`, prompt: empty
- `/plan-epic checkout "buy flow with cart, payments, confirmation"` → name: `checkout`, prompt: "buy flow with cart, payments, confirmation"

If `$ARGUMENTS` is completely empty, ask the user for the epic name and a brief description before continuing.

### 2. Source Requirements

**If `docs/features/<epic-name>/epic-requirements.md` exists:**
- Read it and use it as the base requirements — inform the user you found existing epic requirements and summarize them.
- Ask clarifying questions to fill gaps or refine scope, but do not ask the user to re-describe what they already wrote.
- This is the **re-run path**: `/plan-epic` is designed to be run again on an existing epic to add features or refine the plan. Read the current `epic-overview.md` and existing feature subfolders too (check what has already been scaffolded vs what is new).

**If `docs/features/<epic-name>/epic-requirements.md` does not exist but an initial prompt was given:**
- Use the prompt as the starting requirements.
- Ask clarifying questions to understand scope, feature boundaries, and the "why".

**If neither exists:**
- Ask the user: "What is this epic about? Describe the part of the system it covers and why these features belong together."
- Wait for response, then ask clarifying questions.

**Clarifying questions to ask (via `AskUserQuestion` for concrete choices):**
- What is the primary outcome this epic delivers to users?
- Are there features that are clearly in scope vs. clearly deferred?
- Are there existing features in `docs/features/` that belong in this epic, or is this entirely greenfield?
- What are the hardest/riskiest things to get right — what should be de-risked first?

Apply YAGNI: if something sounds speculative or "nice to have for later", push back. Ask "Is this essential for the epic's first version?"

### 3. Scope the Feature Set

Work with the user at the **epic level** — this is a scoping and decomposition conversation, not a deep architecture discussion. Only drop into architecture where genuinely necessary to decide feature boundaries.

**Your goal in this step:** produce an agreed, named list of constituent features with:
- A one-line description for each
- A sense of relative scope (roughly small / medium / large)
- A first draft of why they are in scope (vs. deferred)

**Use `AskUserQuestion` for scoping choices** such as:
- "Should X be its own feature, or part of Y?"
- "Is this epic covering A and B, or just A?"
- "Do any of these feel too broad? I can split them."

Keep the feature count realistic: 2–5 features per epic is healthy; more than 6–8 usually means the epic is too wide or a feature is too fine-grained.

Do **not** dive into implementation approaches or data models per feature at this stage — that is `/plan-feature`'s job.

### 4. Decide Build Order

Once the feature set is agreed, decide the build order and record **why**:

- Which features are foundational (others depend on their data, APIs, or UI shells)?
- Which features deliver the earliest vertical slice of value (de-risks the epic, proves the concept)?
- Which features can be built in parallel (no dependency between them)?
- What external dependencies or integrations must exist before certain features can start?

Produce an ordered list (1, 2, 3, …) with a one-sentence rationale per feature explaining its position. Where features can be built in parallel, note that explicitly.

**If re-running on an existing epic:** review the current `epic-overview.md` build order — keep, adjust, or extend it based on what has changed.

### 5. Create the Epic Scaffold

Create all files in a single pass after reaching agreement with the user. Do not create files one by one mid-conversation.

#### 5a. Create the epic folder (if new)

```bash
mkdir -p docs/features/<epic-name>
```

If the folder already exists (re-run path), leave its existing contents intact; only create or update the files listed below.

#### 5b. Write `epic-overview.md`

Create (or update) `docs/features/<epic-name>/epic-overview.md` from the template at **`.claude/templates/epic-overview.md`**.

Fill in:
- **Epic:** `<epic-name>`
- **Status:** `Planned` (for a new epic); keep the existing status on re-run.
- **Purpose:** 1–2 paragraphs describing what this epic covers and why these features belong together (from the scoping conversation).
- **Features table:** one row per agreed feature, in build order (build order = row order). Status = `Planned` for new features. Task counts = `—` (not yet started). One-line description per feature.
- **Build Order / Dependencies:** the ordered list from Step 4, with rationale. Note any parallel-build pairs and any external dependencies.
- **Integration & Architecture:** high-level notes on how the features compose and how the epic integrates with the rest of the project. Keep this proportional to what emerged in scoping — a placeholder is fine if architecture was not discussed.
- **Tech Debt / Findings:** leave empty (populated by `/review-epic`).
- **Master Overview Rollup:** rollup status = `Planned (0/<N> features)`; one-line master summary = the epic's elevator-pitch sentence for `docs/overview.md`.
- **Last Updated:** current timestamp.

#### 5c. Write `epic-requirements.md`

If `docs/features/<epic-name>/epic-requirements.md` does **not** already exist, create it from the template at **`.claude/templates/epic-requirements.md`**.

Fill in:
- **Overview:** 2–3 sentences describing what the epic delivers, which part of the system it owns, and why these features belong together.
- **Requirements:** the agreed epic-level requirements from the scoping conversation (what the epic as a whole must achieve — not per-feature detail).
- **Planned Features:** the ordered feature list from Step 4, each with its one-line description and ordering rationale.
- **Dependencies:** any epics, features, or external systems that must exist before this epic (or before specific features within it).
- **Out of Scope:** things explicitly excluded from this epic.

If `epic-requirements.md` already exists (re-run path), do **not** overwrite it. If the scoping conversation changed the scope, update it with targeted edits.

#### 5d. Create each feature subfolder with a `requirements.md`

For **each** feature in the agreed feature set:

1. Create the subfolder: `docs/features/<epic-name>/<feature-name>/`
2. Create `docs/features/<epic-name>/<feature-name>/requirements.md` in exactly the shape `/plan-feature` reads in its Step 4 requirements lookup:

```markdown
# <Feature Name> — Requirements

**Epic:** <epic-name>
**Created:** <today's date>

## Overview

[2–3 sentences: what this feature delivers, which part of the system it owns,
and how it fits within the <epic-name> epic.]

## Requirements

- [Concrete, testable requirement — what the feature must achieve.]
- [Keep it focused on *what*, not *how*. Implementation detail goes in implementation.md.]
- [Include the key user-facing behaviors and any non-negotiable constraints.]

## Dependencies

- [What must exist before this feature can be built — from within the epic (e.g. "<feature-X>
  must be complete") or from outside it (e.g. "requires the auth system").]
- [Note if this feature can be built in parallel with any sibling feature.]

## Out of Scope

- [Capabilities explicitly deferred from this feature to a later feature or epic.]
- [Keep scope tight — document the boundary explicitly so /plan-feature does not gold-plate.]
```

Do **not** create `implementation.md`, `context.md`, or `tasks.md` for child features at this stage. Those files are created by `/plan-feature <epic-name>/<feature-name>` and `/start-feature <epic-name>/<feature-name>` when the user is ready to implement each feature.

If a feature subfolder already exists (re-run path), check whether it has a `requirements.md`. If yes, leave it intact (the user can run `/plan-feature <epic-name>/<feature-name>` to refine it). If no, create the `requirements.md` as above.

### 6. Report and Hand Off to `/plan-feature`

Show a summary and the exact next-step commands:

```
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
EPIC PLANNED: <epic-name>
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

Epic:  docs/features/<epic-name>/
Files: epic-overview.md ✓   epic-requirements.md ✓

Features scaffolded (<N> total, in build order):
  1. <feature-name>   — <one-line description>
  2. <feature-name>   — <one-line description>
  3. <feature-name>   — <one-line description>

Next steps — plan each feature in order:
  /plan-feature <epic-name>/<feature-1>
  /plan-feature <epic-name>/<feature-2>
  /plan-feature <epic-name>/<feature-3>

When ready to implement, run /start-feature <epic-name>/<feature-name>
after each feature is planned.
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

List the `/plan-feature` commands in the recommended build order so the user can copy-paste them one by one.

---

## Important Notes

- **DO NOT start implementing** any feature — this command creates the epic scaffold and requirements only.
- **DO work at the epic level** — scope, feature set, boundaries, build order. Architecture per feature is `/plan-feature`'s job.
- **DO apply YAGNI** — push back on speculative features. A tight, well-ordered epic of 2–5 features is better than a sprawling one.
- **DO use `AskUserQuestion`** for scoping choices (feature boundaries, what's in vs. deferred, build order). Open-ended questions are fine for the initial description; concrete choices get concrete options.
- **DO create files only after agreement** — do not create partial scaffolds mid-conversation.
- **DO keep child feature `requirements.md` minimal** — Overview / Requirements / Dependencies / Out of Scope. The depth comes from `/plan-feature`, not from this command.
- **DO NOT create `implementation.md` / `context.md` / `tasks.md`** for child features — those come from `/plan-feature` and `/start-feature`.
- **This command is re-runnable.** If `epic-overview.md` already exists, read it and the existing subfolders, then continue planning — add features, refine the build order, fill in missing `requirements.md` files.
- **DRY:** detection/resolution rules live in `.claude/epic-resolution.md`. This command is the planning workflow and points at that file.

## Error Handling

- **Empty `$ARGUMENTS`:** ask for the epic name and a brief description before proceeding.
- **Epic name contains spaces or special characters:** suggest a kebab-case alternative.
- **`docs/features/<epic-name>/` exists and is a plain feature** (has `implementation.md` but no `epic-overview.md`): surface a clear conflict — the epic name collides with an existing feature. Ask the user to choose a different epic name or run `/create-epic` to promote that feature into an epic.
- **Feature subfolder name collides with the epic name itself:** flag and ask the user to rename the feature.
- **User wants to add a feature to an existing epic during re-run:** treat it as a new entry in the feature set; create its subfolder + `requirements.md`; update the `epic-overview.md` Features table and build order.
- **User cancels mid-conversation:** if the epic folder was not created, there is nothing to clean up. If partial files were written, inform the user of what exists and leave them in place (no harm — they're not the marker until `epic-overview.md` is present).

## Example Workflow

```
User: /plan-epic checkout "buy flow with cart, payments, confirmation"

Claude: I'll help plan the checkout epic. Let me ask a few scoping questions
        before we lock in the feature set...

        [AskUserQuestion: scoping choices]

        Agreed feature set (build order):
          1. cart        — item management, quantities, persistence
          2. payments    — Stripe integration, payment methods, error handling
          3. confirmation — order summary, email receipt, order history

        Creating epic scaffold...

━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
EPIC PLANNED: checkout
...
Next steps:
  /plan-feature checkout/cart
  /plan-feature checkout/payments
  /plan-feature checkout/confirmation
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
```

---

**Remember:** This command owns the epic-level decomposition (what features, in what order, with what boundaries). The deep planning work per feature belongs to `/plan-feature <epic-name>/<feature-name>`, which loads the epic context (including this `epic-overview.md` and all sibling requirements) before planning each feature.
