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

- **Within the epic:** one framework (`OVT_JobCondition`/`OVT_JobStage` composed by `.conf` assets), one manager, one thin UI reading the manager's board. `jobIndex` is the positional identity in `OVT_OverthrowGameMode.et`'s `m_aJobConfigs`, and it is still the in-session and on-the-wire handle. ⚠️ **Updated 2026-08-09: it is no longer what gets persisted.** Saves are keyed on the stable `OVT_JobConfig.m_sId` from job payload version 2 onward, so `m_aJobConfigs` no longer has to be append-only — see the discharged tech-debt item below.
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

- [x] ✅ **RESOLVED — MP distribution book-keeping was the weak layer** — core — update-RPC identity omitted owner (BUG-038), decline never broadcast (BUG-039), every private job + completion hint broadcast to all clients (BUG-040). **All three are `status: closed`** (`docs/bugs/BUG-038.md:4`, `BUG-039.md:4`, `BUG-040.md:4`), fixed at the manager on 2026-08-03 without touching the framework, exactly as this item predicted. ⚠️ **Closed is not observed:** two-client behaviour has never been play-tested, which the `new-player-experience` epic also records as its own biggest outstanding question.
- [x] ✅ **RESOLVED — Global caps break the tutorial chain in MP** — core — **fixed in place on 2026-08-03** (BUG-037, `closed`): `OVT_JobManagerComponent.c:638-639` skips the global `m_iMaxTimes` gate for player-allocated configs. The five onboarding jobs it described were then **removed** on 2026-08-09 by `new-player-experience/starter-jobs-retirement` — a redundancy decision that closed nothing; the fix predates it and remains live for any future player-allocated job. ⚠️ Note that **no shipped config is player-allocated any more** (`m_bPublic` defaults to `1`, `OVT_JobConfig.c:31-32`, and no survivor overrides it), so that branch is currently exercised only by tests.
- [x] ✅ **DISCHARGED 2026-08-09 — Positional `jobIndex` no longer couples saves to prefab list order** — core — discharged by `new-player-experience/starter-jobs-retirement`, which had to remove five interleaved configs and could not do so safely while the save format named jobs by position. What replaced it:
  - **`OVT_JobConfig.m_sId`** (`Scripts/Game/Configuration/OVT_JobConfig.c`) — a stable, immutable kebab-case identity authored into every `Configs/Jobs/*.conf`. The **config resource name** this item originally suggested was rejected (decision D1): it is stable but couples saves to a file path, so moving `Configs/Jobs/` would break them.
  - **Save format version 2** (`Scripts/Game/Persistence/Serializers/Components/OVT_JobManagerSerializer.c`) — the board and **both** lifetime counter maps are persisted keyed on `m_sId`. Reordering or trimming `m_aJobConfigs` is no longer a save-breaking act.
  - **The frozen `LEGACY_V1_JOB_IDS` table** (`OVT_JobManagerSerializer.c:194-207`, with `RETIRED_V1_JOB_IDS` at `:218-224`) — the twelve-entry version 1 index→id history, marked immutable. It is what lets a pre-migration save name the job in each record it drops instead of silently re-attaching it to whatever now sits at that index.
  - **Exactly two translation points, both server-side and both on the save/load path** (decision D2): `Serialize()` converts index→id, and the manager's restore path converts id→index inside `FindRestorableJobConfig()`. **In-session and on-the-wire identity is still the positional int** — no `[RplRpc]` signature and no `RplSave`/`RplLoad` field changed, proven by diff.
  - Covered by three assertions proven able to fail: the frozen table (Logic tier), the id uniqueness/resolve guard (Init tier), and a job-board save→re-apply round trip (PersistenceRoundTrip tier).

- [ ] 💳 **Six stage/condition classes are orphaned but deliberately KEPT** — core — the five starter jobs retired on 2026-08-09 were the only shipped configs composing these six classes, so **no shipped `.conf` references any of them today** (verified by grep over `Configs/Jobs/` on 2026-08-09): `OVT_GetShopLocationJobStage`, `OVT_GetDealerLocationJobStage`, `OVT_HasRecruitJobStage`, `OVT_IsNearestTownWithShopJobCondition`, `OVT_IsNearestTownWithDealerJobCondition`, `OVT_IsNearestJobCondition`. They were **not** deleted (decision D8, user decision): they are valid config-composable primitives documented in `jobs/core`'s authoring guide, an unreferenced `ScriptAndConfig` class costs nothing but a symbol, and deleting them is a modder-facing break with no runtime gain. Recorded here so the next reader knows they are **unexercised by shipped content** — a change to one of them is untested by any live job, and `OVT_GetRadioTowerLocationJobStage.c:4-6` carries the same note at the one site that used to cite a job instead of a class.
- [ ] 💳 **Jobs bypass the notification manager** — core — raw `SCR_HintManagerComponent` (server-side call included), the only subsystem doing so; route through `OVT_NotificationManagerComponent`.
- [ ] 💳 **`m_vCurrentWaypoint` shared scratchpad** — core / resistance — one global vector on a server manager written by both jobs and recruits UIs; no clear; origin = unset.

---

## Master Overview Rollup

How this epic is represented in the project's master `docs/overview.md` (one row, not its children). Kept in sync by `/update-epic` and `/update-master`.

- **Rollup status:** 📄 Documented (1/1 retrospective)
- **One-line summary for master:** Config-driven quest layer (conditions/stages framework, job manager, menu UI, 8 shipped jobs) backfilled with retrospective docs; discovery catalogued ~20 issues headlined by the tutorial chain being dead for every player but the first (global caps), owner-less update-RPC identity, decline never broadcasting, and broadcast leakage of private jobs; top 5 filed as BUG-037…041.

---

*This is a required file — do not delete it; it marks the folder as an epic. Update it with `/update-epic jobs` after working on the epic's features, and run `/review-epic jobs` to refresh the Tech Debt / Findings section.*
