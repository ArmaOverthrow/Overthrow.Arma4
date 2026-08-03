# Jobs - Epic Requirements

**Created:** 2026-08-02
**Phase:** Backfill (retrospective documentation of a shipped system)

> Epic-level requirements — the higher-level scope for the whole epic, mirroring a feature's `requirements.md` but one level up. `/plan-epic jobs` reads this file (if it exists) to drive epic scoping; otherwise it uses the prompt. Each **child feature** below gets its own `requirements.md` (in the standard Overview / Requirements / Dependencies / Out of Scope shape) that `/plan-feature jobs/[feature-name]` consumes.

## Overview

The job system is Overthrow's quest layer: data-driven missions offered per town, per base or per player, composed from reusable condition and stage classes and paid out in money, XP and items. This epic owns the framework, the manager, the jobs UI and the job content — everything a developer touches to add or fix a job.

## Requirements

- Jobs are authored as data (`.conf` + prefab list entry); new mechanics are one small condition/stage class — no manager changes.
- Both distribution models work correctly in multiplayer: public (one instance per town/base) and player-allocated (per player, per-player caps).
- The job board is consistent across server, connected clients and JIP joiners.
- In-progress jobs survive save/continue at their current stage with no double payout (the `core/persistence` no-replay invariant).
- The onboarding chain (find dealer/shop, place box, recruit, place camp) works for **every** player on a server, not just the first (BUG-037 — currently violated).

## Planned Features

The features that make up this epic, in intended **build order**. `/plan-epic` creates a subfolder + `requirements.md` for each, and records the order in `epic-overview.md`.

1. **core** — the shipped system, documented retrospectively (2026-08-02) — foundation; everything else builds on it.
2. *(future — via `/plan-epic jobs`)* candidates from discovery: an MP-distribution fix pass (BUG-037…041), a reactive jobs menu, per-stage guidance for multi-stage jobs, new job content.

## Dependencies

- Towns, occupying faction, economy, skills, recruits, placeables managers (all shipped)
- `core/persistence` — serializer registered in `Configs/Systems/Persistence/Overthrow.conf`; restore rules documented there

## Out of Scope

- Recruit management itself (resistance epic — jobs only read recruit counts)
- The notification system rework (jobs should *adopt* `OVT_NotificationManagerComponent`, not redesign it)
- Victory/defeat conditions and campaign-level mission arcs

---

*Consumed by `/plan-epic jobs`. After planning, run `/plan-feature jobs/[feature-name]` per feature in the recommended order.*
