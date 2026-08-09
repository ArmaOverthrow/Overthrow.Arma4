# [Epic Name] - Epic Overview

**Epic:** [epic-name]
**Status:** 🟡 In Progress
**Last Updated:** YYYY-MM-DD HH:MM

> **This file is the epic marker.** Its presence in `docs/features/[epic-name]/` is what tells every Beast Mode command (and future Web App / Discord clients) that this folder is an **epic**, not a plain feature. Keep it present and keep the required sections below filled in. It is the epic's equivalent of the project's `docs/overview.md`, scoped to this epic. The master `docs/overview.md` tracks this epic as a **single row**; the per-feature detail lives **here**.

---

## Purpose

[One-paragraph summary of what this epic covers — which part of the system it owns and why these features belong together. This is the high-level "what is this epic" that `/continue-feature [epic-name]` and `/review-epic [epic-name]` read first.]

---

## Features

The constituent features of this epic, in build order. This is the epic's equivalent of `docs/overview.md`'s feature-status table, scoped to the epic. Each feature is a subfolder under `docs/features/[epic-name]/`.

| # | Feature | Status | Tasks | Description |
|---|---------|--------|-------|-------------|
| 1 | [feature-name] | [Planned / In Progress / Complete] | [X/Y (Z%)] | [One-line description of what this feature delivers] |
| 2 | [feature-name] | [Planned / In Progress / Complete] | [X/Y (Z%)] | [One-line description] |
| 3 | [feature-name] | [Planned / In Progress / Complete] | [—] | [One-line description — not yet started, may be requirements-only] |

> Reference any feature with the slash form `[epic-name]/[feature-name]` (e.g. `/continue-feature [epic-name]/[feature-name]`). Task counts are pulled from each feature's own `tasks.md` and refreshed by `/update-epic`.

---

## Build Order / Dependencies

Which features come first, and why. This feeds planning and the next-step suggestions from `/continue-feature [epic-name]`.

1. **[feature-name]** — [Why it comes first — e.g. foundational, others depend on it, vertical slice that de-risks the rest.]
2. **[feature-name]** — [What it depends on from the feature(s) above and why it follows.]
3. **[feature-name]** — [Dependency / ordering rationale.]

**Dependencies between features:**
- [feature-A] → [feature-B] ([what B needs from A])
- [Note any feature that can be built in parallel, and any external dependency outside this epic.]

---

## Integration & Architecture

How the features fit together as one coherent system, and how this epic integrates with the rest of the project.

- **Within the epic:** [How the features connect — shared modules, data flow, contracts between features so they compose cleanly.]
- **With other epics / features:** [Cross-epic integration points and dependencies outside this epic.]
- **Key architectural decisions for the epic as a whole:** [Decisions that span multiple features and should constrain each feature's plan.]

---

## Tech Debt / Findings

Cross-feature tech debt and review findings. **Populated and updated by `/review-epic`** (which also writes feature-specific findings into the relevant child `context.md` files). Start empty.

- [ ] 💳 **[Debt / finding title]** — [Affected feature(s)] — [What it is and why it matters; recommended follow-up.]
- (none yet — `/review-epic` will surface cross-feature integration issues and per-feature tech debt here)

---

## Master Overview Rollup

How this epic is represented in the project's master `docs/overview.md` (one row, not its children). Kept in sync by `/update-epic` and `/update-master`.

- **Rollup status:** [Aggregate status of the epic — e.g. "In Progress (2/3 features complete)".]
- **One-line summary for master:** [The single-line notes string that appears in the master overview's Feature Status row for this epic.]

---

*This is a required file — do not delete it; it marks the folder as an epic. Update it with `/update-epic [epic-name]` after working on the epic's features, and run `/review-epic [epic-name]` to refresh the Tech Debt / Findings section.*
