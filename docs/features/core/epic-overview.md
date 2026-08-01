# Core - Epic Overview

**Epic:** core
**Status:** ⏸️ Paused (0/1 features complete)
**Last Updated:** 2026-08-02

> **This file is the epic marker.** Its presence in `docs/features/core/` is what tells every Beast Mode command (and future Web App / Discord clients) that this folder is an **epic**, not a plain feature. Keep it present and keep the required sections below filled in. It is the epic's equivalent of the project's `docs/overview.md`, scoped to this epic. The master `docs/overview.md` tracks this epic as a **single row**; the per-feature detail lives **here**.

---

## Purpose

The `core` epic owns Overthrow's foundational systems — the skeleton every other system stands on: the game-mode/manager lifecycle (`OVT_OverthrowGameMode`, `OVT_Global`, `OVT_OverthrowController`), configuration, player identity, and the persistence layer. Gameplay domains (towns, jobs, economy, occupying faction, etc.) will get their own epics later; this epic holds only the very core. It was created by migrating the pre-existing `vanilla-persistence` feature (now `core/persistence`) and will be backfilled with `/discover-feature` docs for the legacy core systems.

---

## Features

The constituent features of this epic, in build order. This is the epic's equivalent of `docs/overview.md`'s feature-status table, scoped to the epic. Each feature is a subfolder under `docs/features/core/`.

| # | Feature | Status | Tasks | Description |
|---|---------|--------|-------|-------------|
| 1 | persistence | ⏸️ Paused | 6/67 (9%) | Migration from EPF to Reforger's vanilla persistence system (big-bang, breaking, no save migration). Superseded in priority by the dev-ops epic. ⚠️ Phase 1 "foundation" targeted a nonexistent API and never compiled — must be re-done against the real vanilla API on resume (see its `context.md`). Machine-checkable definition of done exists: `OVT_TEST_PersistenceRoundTripSuite` flipping from exit 1 to exit 0. |

> Reference any feature with the slash form `core/[feature-name]` (e.g. `/continue-feature core/persistence`). Task counts are pulled from each feature's own `tasks.md` and refreshed by `/update-epic`.
>
> Discovery backfill planned (2026-08-02): `core/game-mode` (game mode, manager lifecycle, `OVT_Global`, `OVT_OverthrowController`), `core/config` (`OVT_OverthrowConfigComponent`, difficulty/faction config), `core/player-manager` (persistent player identity and data). These document existing legacy systems via `/discover-feature`; rows appear as each is discovered.

---

## Build Order / Dependencies

Which features come first, and why. This feeds planning and the next-step suggestions from `/continue-feature core`.

1. **game-mode** *(discovery pending)* — foundational: manager registration/lifecycle and global access underpin everything else in the mod.
2. **config** *(discovery pending)* — loaded by the game mode at init; other systems read it.
3. **player-manager** *(discovery pending)* — persistent player identity; consumed by ownership, skills, and persistence.
4. **persistence** — spans all of the above (it serializes their state). Paused; resumes after the dev-ops epic lands, validated by the quarantined round-trip gate.

**Dependencies between features:**
- game-mode → config, player-manager, persistence (manager lifecycle hosts all of them)
- persistence → everything (serializes the state the other core systems own)
- External: dev-ops epic supersedes `core/persistence` in priority and supplies its acceptance gate (`dev-ops/test-coverage`).

---

## Integration & Architecture

How the features fit together as one coherent system, and how this epic integrates with the rest of the project.

- **Within the epic:** `OVT_OverthrowGameMode` instantiates and initializes the manager singletons; `OVT_Global` exposes them statically; `OVT_OverthrowController` is the client→server seam; `OVT_PersistenceManagerComponent` (feature #1) saves/loads the state those managers own.
- **With other epics / features:** `dev-ops/test-coverage` provides the behaviour-level persistence suites; `OVT_TEST_PersistenceRoundTripSuite` (quarantined, red by design) is `core/persistence`'s acceptance gate — exit 1 → 0 is the migration's definition of done. Future domain epics (towns, jobs, economy…) all build on the manager lifecycle this epic documents.
- **Key architectural decisions for the epic as a whole:** Manager (gamemode singleton) vs Controller (per-entity) split; server authority with RplProp/RPC/JIP replication; big-bang EPF → vanilla persistence migration on the `vanilla-persistence` branch with `main` frozen until it lands.

---

## Tech Debt / Findings

Cross-feature tech debt and review findings. **Populated and updated by `/review-epic`** (which also writes feature-specific findings into the relevant child `context.md` files). Start empty.

- [ ] 💳 **No working save path in either system** — persistence — measured 2026-08-02 by `dev-ops/test-coverage`: `SaveGame()` is a stub and the EPF re-parenting means EPF never initialises either. Tracked as BUG-002 / BUG-006 (`linkedFeature: core/persistence`).
- (further findings: `/review-epic core` will surface cross-feature integration issues here)

---

## Master Overview Rollup

How this epic is represented in the project's master `docs/overview.md` (one row, not its children). Kept in sync by `/update-epic` and `/update-master`.

- **Rollup status:** ⏸️ Paused (0/1 features complete)
- **One-line summary for master:** Overthrow's core systems. #1 `persistence` — migration from EPF to vanilla persistence, paused, superseded in priority by the dev-ops epic. ⚠️ Its Phase 1 "foundation" was found (2026-08-01) to target a nonexistent API and never compiled — must be re-done against the real vanilla API on resume (see its context.md). Further core-system features to be backfilled via discovery.

---

*This is a required file — do not delete it; it marks the folder as an epic. Update it with `/update-epic core` after working on the epic's features, and run `/review-epic core` to refresh the Tech Debt / Findings section.*
