# Virtualization - Epic Overview

**Epic:** virtualization
**Status:** 📋 Planned
**Last Updated:** 2026-08-04

> **This file is the epic marker.** Its presence in `docs/features/virtualization/` is what tells every Beast Mode command (and future Web App / Discord clients) that this folder is an **epic**, not a plain feature. Keep it present and keep the required sections below filled in. It is the epic's equivalent of the project's `docs/overview.md`, scoped to this epic. The master `docs/overview.md` tracks this epic as a **single row**; the per-feature detail lives **here**.

---

## Purpose

The virtualization epic builds the single AI virtualization layer Overthrow has needed for years: a server-side system that owns AI group lifecycle — virtual records with per-member state, spawn/despawn by player proximity, believable virtual movement while despawned — persisted through vanilla persistence. It keeps the general aim of GitHub issue #100 but **discards the EPF-based `virtualization` branch entirely** (264 commits behind main, persistence layer obsolete); only design lessons are salvaged.

Today three ad-hoc virtualization implementations coexist in the occupying epic (base-upgrades value banking, deployments proximity toggling, radio-tower garrison spawn/despawn), a fourth lives in the towns system (the ambient civilian spawner in `OVT_TownControllerComponent`), and the deployments framework's code references a "virtualization system" that doesn't exist. This epic builds that missing layer, converges the movable consumers onto it — including town civilians via a declarative, modder-extendable **ambient spawn-source** class for one-off, non-persisted spawning — and scopes the long-stalled base-upgrades→deployments migration as a visible final feature — deliberately last and deferrable, so its true cost is schedulable without blocking the rest. It also fixes the player-facing promise issue #100 left open: **dead group members stay dead** across despawn and load.

---

## Features

The constituent features of this epic, in build order. Each feature is a subfolder under `docs/features/virtualization/`.

| # | Feature | Status | Tasks | Description |
|---|---------|--------|-------|-------------|
| 1 | core | Planned | — | `OVT_VirtualizationManagerComponent` — virtual group records (faction, composition, per-member alive state, position, waypoint plan), proximity spawn/despawn with frame-spreading, server-configurable spawn distance, vanilla-persistence serializer, plus the **ambient spawn-source seam** (config-declared one-off/non-persisted spawning). |
| 2 | civilians | Planned | — | Town civilians migrate off `OVT_TownControllerComponent` onto the ambient spawn-source class: config-driven density with runtime operator tuning, prefab variety, behavior archetypes, believable placement; stretch: ambient parked civilian vehicles. |
| 3 | movement | Planned | — | Virtual movement while despawned — straight-line fixed speed for infantry, road-network-following for vehicle groups — plus valid spawn placement and live-AI waypoint handoff both directions. |
| 4 | integration | Planned | — | First tracked-group consumers: deployments' group lifecycle (town patrol + both vehicle patrols) and radio-tower garrisons migrate onto the layer; ad-hoc proximity code retired; dead members stay dead in the live game. |
| 5 | base-defense-migration | Planned | — | Complete the stalled base-upgrades→deployments migration (design phases 3–4) on virtualization and retire base-upgrades — scoped for visibility, deferrable without blocking the earlier features. |

> Reference any feature with the slash form `virtualization/[feature-name]` (e.g. `/continue-feature virtualization/core`). Task counts are pulled from each feature's own `tasks.md` and refreshed by `/update-epic`.

---

## Build Order / Dependencies

1. **core** — Foundational: every other feature registers groups with, or reads state from, the manager it introduces. Also carries the epic's persistence contract (Persistence-tier round-trip test from day one) and the ambient spawn-source seam civilians consumes.
2. **civilians** — First consumer, deliberately straight after core: exercises the ambient (one-off, non-persisted, untracked) class and the declarative config surface before any combat migration, hardening core's extensibility seam early. Depends only on core.
3. **movement** — Depends on core's records (position, waypoint plan). Must exist before tracked-group consumers migrate, or migrated patrols would freeze in place while despawned. Independent of civilians (ambient spawns have no despawned life to advance).
4. **integration** — The vertical slice that proves the tracked-group API: real campaign systems (deployments' three configs, tower garrisons) running on the layer, with dead-member persistence player-visible. De-risks base-defense-migration by exercising the API against the deployments framework first.
5. **base-defense-migration** — Largest feature, deliberately last: needs integration's proven deployments↔virtualization seam. **Deferrable** — the epic ships standalone value without it; if deferred, the base-upgrades/deployments coexistence remains (status quo) but on one fewer ad-hoc virtualization.

**Dependencies between features:**
- core → civilians (ambient spawn-source seam; no movement dependency)
- core → movement (virtual tick advances core's records)
- core + movement → integration (consumers need both lifecycle and believable positions)
- integration → base-defense-migration (proven seam + migration tooling)
- civilians ∥ movement is the only parallelizable pair; everything else is sequential.
- External: **vanilla persistence (shipped in 1.4.0)** is the hard prerequisite for core. Integration is cleaner if deployments' known lifecycle bugs (BUG-028 faction-list leak, world-time unit bugs) are fixed in 1.4.x first — noted in its requirements.

---

## Integration & Architecture

- **Within the epic:** core owns the record and lifecycle — two registration classes: tracked persistent group records, and ambient (one-off, non-persisted, config-declared) spawn sources; movement is a tick strategy over core's tracked records; civilians, integration and base-defense-migration are consumer migrations that shrink other systems rather than growing this one. The deployment marker entity remains the durable record for *deployment-level* state; virtualization owns *group-level* state (members, position, waypoints) — one system per concern, no double bookkeeping.
- **With other epics / features:** occupying/deployments becomes the primary tracked-group consumer (its spawning modules delegate group lifecycle); occupying/core's radio-tower garrison code is replaced; occupying/base-upgrades is ultimately retired (feature 5). Towns' `OVT_TownControllerComponent` loses its civilian spawner to feature 2 (town data — population, range, location — stays the density input; `OVT_PatrolHarassmentStabilityModifier` must keep working across the migration). Economy 2.0 (future epic) is a **design-for consumer only**: core's abstractions must not preclude non-combat virtual agents (virtual citizen purchases, deliveries), but nothing agent-related is built here — ambient civilians are scenery, not simulated agents.
- **Key architectural decisions for the epic as a whole:**
  - **Rebuild, don't rebase:** the old `virtualization` branch is reference material only. Salvaged lessons: eliminated-flag-before-module-init ordering (EPF-era resurrection bug), the 40 m stolen-vehicle rule, frame-spread spawning.
  - **Vanilla persistence native:** serializers follow the patterns in `docs/features/core/persistence/vanilla-api-reference.md`; no EPF concepts anywhere.
  - **Server-only, no client surface:** virtualized state never replicates; map/UI presence for virtual units is explicitly out of scope.
  - **Per-member truth:** the record tracks individual members alive/dead — groups respawn with exactly their survivors and are removed when wiped.
  - **No virtual combat:** despawned groups never fight, take damage, or resolve engagements virtually.

---

## Tech Debt / Findings

Cross-feature tech debt and review findings. **Populated and updated by `/review-epic`** (which also writes feature-specific findings into the relevant child `context.md` files). Start empty.

- (none yet — `/review-epic` will surface cross-feature integration issues and per-feature tech debt here)

---

## Master Overview Rollup

How this epic is represented in the project's master `docs/overview.md` (one row, not its children). Kept in sync by `/update-epic` and `/update-master`.

- **Rollup status:** Planned (0/5 features)
- **One-line summary for master:** The unified AI virtualization layer (issue #100 rebuilt on vanilla persistence): virtual group records with per-member state, config-driven ambient civilians with operator-tunable density, road-aware virtual movement, proximity spawn/despawn — converging four ad-hoc implementations (occupying's three + towns' civilian spawner) and scoping the stalled base-upgrades→deployments migration as a deferrable final feature.

---

*This is a required file — do not delete it; it marks the folder as an epic. Update it with `/update-epic virtualization` after working on the epic's features, and run `/review-epic virtualization` to refresh the Tech Debt / Findings section.*
