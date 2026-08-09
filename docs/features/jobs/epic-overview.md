# Jobs - Epic Overview

**Epic:** jobs
**Status:** 📄 Documented (1/1 retrospective)
**Last Updated:** 2026-08-02 23:42

> **This file is the epic marker.** Its presence in `docs/features/jobs/` is what tells every Beast Mode command (and future Web App / Discord clients) that this folder is an **epic**, not a plain feature. Keep it present and keep the required sections below filled in. It is the epic's equivalent of the project's `docs/overview.md`, scoped to this epic. The master `docs/overview.md` tracks this epic as a **single row**; the per-feature detail lives **here**.

---

## Purpose

The job system — Overthrow's config-driven quest layer. Towns and bases offer missions (assassinations, recon, support-raising, and the five-job onboarding chain) composed from reusable condition and stage classes; a single manager polls, offers, ticks and rewards. This epic owns the framework, the manager, the jobs UI and the shipped job content. It was pre-declared by the core epic ("gameplay domains — towns, **jobs**, economy … will get their own epics later") and backfilled with retrospective docs via `/discover-feature` on 2026-08-02. Future job work (new content, MP distribution fixes, reactive UI) lands as features here.

---

## Features

The constituent features of this epic, in build order. This is the epic's equivalent of `docs/overview.md`'s feature-status table, scoped to the epic. Each feature is a subfolder under `docs/features/jobs/`.

| # | Feature | Status | Tasks | Description |
|---|---------|--------|-------|-------------|
| 1 | core | 📄 Documented (retrospective) | — | The whole shipped system: condition/stage framework, `OVT_JobManagerComponent` lifecycle, replication + JIP, vanilla persistence, jobs menu UI, 8-job catalog, authoring guide |

> Reference any feature with the slash form `jobs/core` (e.g. `/continue-feature jobs/core`). Task counts are pulled from each feature's own `tasks.md` and refreshed by `/update-epic`.

---

## Build Order / Dependencies

1. **core** — the entire existing system, documented as one feature: at ~1,400 lines across 24 files with a single manager there is no natural internal seam (a core/UI split would cut every MP-distribution finding in half). Future features (e.g. a job-content expansion or an MP-distribution overhaul) build on it.

**Dependencies between features:**
- (single feature today; new features added via `/plan-epic jobs` will slot in here)
- External: towns, occupying faction, economy, skills, recruits, placeables, persistence — see Integration below.

---

## Integration & Architecture

- **Within the epic:** one framework (`OVT_JobCondition`/`OVT_JobStage` composed by `.conf` assets), one manager, one thin UI reading the manager's board. `jobIndex` is the positional identity in `OVT_OverthrowGameMode.et`'s `m_aJobConfigs` — append-only, it is persisted.
- **With other epics / features:**
  - **core/persistence** — `OVT_JobManagerSerializer` + idempotent `ApplyPersistedJobs()`; the no-replay restore argument (waiting stages override `OnTick` only) is recorded in `core/persistence`'s context.md and is this epic's most load-bearing invariant.
  - **skills** — jobs pay XP via `GiveXP` (one of the four XP sources named in the skills epic).
  - **economy** — shop-registry reads; `AddPlayerMoney` rewards.
  - **occupying** — bases host base-only jobs; `SpawnGroupJobStage` spawns OF groups.
  - **resistance** (planned) — `HasRecruitJobStage` reads recruits; `PlaceableItemJobStage` reads placeables; `OVT_RecruitsContext` shares the `m_vCurrentWaypoint` map-waypoint slot.
  - **dev-ops** — 4 Logic-tier cases in `OVT_TEST_Logic_Jobs.c` (one deliberately pins BUG-005) + an Init resolve assert; the manager, stages and impure conditions are uncovered.
- **Key architectural decisions for the epic as a whole:** config-as-data composition (`ScriptAndConfig`); false-means-advance stage protocol; broadcast RPCs over RplProp; occupancy sets derived on restore, lifetime counters stored.

---

## Tech Debt / Findings

Cross-feature tech debt and review findings. **Populated and updated by `/review-epic`** (which also writes feature-specific findings into the relevant child `context.md` files). Seeded from the 2026-08-02 discovery:

- [ ] 💳 **MP distribution book-keeping is the weak layer** — core — update-RPC identity omits owner (BUG-038), decline never broadcasts (BUG-039), every private job + completion hint broadcasts to all clients (BUG-040); all fixable at the manager without touching the framework.
- [ ] 💳 **Global caps break the tutorial chain in MP** — core — the five onboarding jobs use global `m_iMaxTimes 1`, so the second player on a server never sees them (BUG-037; `m_iMaxTimesPlayer` exists but is inert under the global cap).
- [ ] 💳 **Positional `jobIndex` couples saves to prefab list order** — core — append-only discipline required on `m_aJobConfigs`; a stable identity (config resource name) would remove the trap.
- [ ] 💳 **Jobs bypass the notification manager** — core — raw `SCR_HintManagerComponent` (server-side call included), the only subsystem doing so; route through `OVT_NotificationManagerComponent`.
- [ ] 💳 **`m_vCurrentWaypoint` shared scratchpad** — core / resistance — one global vector on a server manager written by both jobs and recruits UIs; no clear; origin = unset.

---

## Master Overview Rollup

How this epic is represented in the project's master `docs/overview.md` (one row, not its children). Kept in sync by `/update-epic` and `/update-master`.

- **Rollup status:** 📄 Documented (1/1 retrospective)
- **One-line summary for master:** Config-driven quest layer (conditions/stages framework, job manager, menu UI, 8 shipped jobs) backfilled with retrospective docs; discovery catalogued ~20 issues headlined by the tutorial chain being dead for every player but the first (global caps), owner-less update-RPC identity, decline never broadcasting, and broadcast leakage of private jobs; top 5 filed as BUG-037…041.

---

*This is a required file — do not delete it; it marks the folder as an epic. Update it with `/update-epic jobs` after working on the epic's features, and run `/review-epic jobs` to refresh the Tech Debt / Findings section.*
