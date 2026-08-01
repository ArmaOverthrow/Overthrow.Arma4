# [Epic Name] - Epic Requirements

**Created:** [Today's date]
**Phase:** [Current development phase, if applicable]

> Epic-level requirements — the higher-level scope for the whole epic, mirroring a feature's `requirements.md` but one level up. `/plan-epic [epic-name]` reads this file (if it exists) to drive epic scoping; otherwise it uses the prompt. Each **child feature** below gets its own `requirements.md` (in the standard Overview / Requirements / Dependencies / Out of Scope shape) that `/plan-feature [epic-name]/[feature-name]` consumes.

## Overview

[2-3 sentence description of what this epic delivers as a whole, which part of the system it owns, and why these features belong together.]

## Requirements

- [Bullet list of concrete, epic-level requirements — what the epic as a whole must achieve.]
- [Keep it focused on *what*, not *how*. Per-feature detail goes in each child feature's own `requirements.md`.]

## Planned Features

The features that make up this epic, in intended **build order**. `/plan-epic` creates a subfolder + `requirements.md` for each, and records the order in `epic-overview.md`.

1. **[feature-name]** — [One-line description] — [Why it comes first / what it unblocks.]
2. **[feature-name]** — [One-line description] — [What it depends on from #1.]
3. **[feature-name]** — [One-line description] — [Ordering / dependency rationale.]

## Dependencies

- [Epics, features, or systems that must exist before this epic — or before specific features within it.]

## Out of Scope

- [Things explicitly NOT included in this epic, to keep scope tight.]
- [Capabilities deferred to a later epic or feature.]

---

*Consumed by `/plan-epic [epic-name]`. After planning, run `/plan-feature [epic-name]/[feature-name]` per feature in the recommended order.*
