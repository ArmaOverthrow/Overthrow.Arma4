# Virtualization - Epic Overview

**Epic:** virtualization
**Status:** 🟡 In Progress (4/5 features complete)
**Last Updated:** 2026-08-17 (integration Phases 1-8 complete)

> **2026-08-14 — Replanned on Reforger 1.8.** The 1.8 update shipped engine-native group virtualization (`ProximityDriven` lifecycle, importance-tiered budgeted spawn queue, dormancy with survivor counts, vanilla group persistence). Decision (user-approved): **adopt as a hybrid** — core becomes a thin Overthrow registry/config/API layer over the engine lifecycle, with **slot-accurate** dead-member truth (a core-owned per-slot mask enforced via an `ExpandOneMember` override on the already-modded `SCR_AIGroup`; revised from an initial count-based acceptance the same day). See `core/implementation.md` Revision 2 and `docs/reforger/1.8.0.10-changes.md`.

> **This file is the epic marker.** Its presence in `docs/features/virtualization/` is what tells every Beast Mode command (and future Web App / Discord clients) that this folder is an **epic**, not a plain feature. Keep it present and keep the required sections below filled in. It is the epic's equivalent of the project's `docs/overview.md`, scoped to this epic. The master `docs/overview.md` tracks this epic as a **single row**; the per-feature detail lives **here**.

---

## Purpose

The virtualization epic builds the single AI virtualization layer Overthrow has needed for years: a server-side system that owns AI group lifecycle — durable group records with slot-accurate survivor truth, spawn/despawn by player proximity, believable virtual movement while despawned — persisted through vanilla persistence. Since 2026-08-14 it is planned as a **thin layer over Reforger 1.8's engine-native lifecycle system** rather than a hand-rolled one. It keeps the general aim of GitHub issue #100 but **discards the EPF-based `virtualization` branch entirely** (264 commits behind main, persistence layer obsolete); only design lessons are salvaged.

Today three ad-hoc virtualization implementations coexist in the occupying epic (base-upgrades value banking, deployments proximity toggling, radio-tower garrison spawn/despawn), a fourth lives in the towns system (the ambient civilian spawner in `OVT_TownControllerComponent`), and the deployments framework's code references a "virtualization system" that doesn't exist. This epic builds that missing layer, converges the movable consumers onto it — including town civilians via a declarative, modder-extendable **ambient spawn-source** class for one-off, non-persisted spawning — and scopes the long-stalled base-upgrades→deployments migration as a visible final feature — deliberately last and deferrable, so its true cost is schedulable without blocking the rest. It also fixes the player-facing promise issue #100 left open: **dead group members stay dead** across despawn and load.

---

## Features

The constituent features of this epic, in build order. Each feature is a subfolder under `docs/features/virtualization/`.

| # | Feature | Status | Tasks | Description |
|---|---------|--------|-------|-------------|
| 1 | core | ✅ Complete (Ready for Review) | 50/50 (100%) | `OVT_VirtualizationManagerComponent` — thin registry/config/API layer over the 1.8 engine lifecycle: composition resolution, owner tagging/reclaim, server-configurable spawn distance, importance stamping, slot-accurate survivor masks, wipe bookkeeping, **Route B registry persistence** (the manager's serializer persists full re-creation state and rebuilds group entities on load — vanilla's `SCR_AIGroupSerializer` proved unusable for runtime groups, see core `context.md`), plus the **ambient spawn-source seam**. `api.md` frozen 2026-08-17; user play-tests tracked in core `context.md`. |
| 2 | civilians | ✅ Complete (Ready for Review) | 39/39 (100%) | Town civilians migrated onto core's ambient spawn-source seam: config-driven density with 3 runtime operator knobs, 6 civilian types with per-type clothing + per-town curation (13 Eden towns), 3 behaviour archetypes, doorway/POI placement, QRF despawn now **opt-in** (user amendment), ambient parked vehicles (kerb-first, save-safe untrack/claim). User play-test + Workbench passes owed (see its `context.md`). |
| 3 | movement | ✅ Complete (Ready for Review) | 22/22 (100%) | Infantry-only virtual movement: a server-side manager on the game mode with one 2 s round-robin tick that walks every dormant registered group along its own waypoint plan in straight lines at a fixed configurable speed, with stateless projection resume (no serializer, nothing replicated, nothing persisted) and ground-snapped, never-in-water writes; core was extended by **exactly one** method (`GetAllHandles()`). Vehicle groups are excluded by construction — they stay spawned via a huge `spawnDistanceOverride` and live AI drives real roads (the engine has no script route-finding API, and insertion/extraction makes vehicle transit live by design). The plan **is** the opt-in: an empty or DEFEND-only plan is never advanced. Handoff is largely native (waypoints never leave the group across dormancy; a moved group resumes its route from index 0 — documented, not engineered around). Automated coverage: Logic tier for the progression maths, Init tier for the seam, the tick and the no-leak claim. User play-test PASSED 2026-08-17 (it surfaced 3 fixes, all landed: waypoint surface-snap, `GetCurrentPlanIndex` live-handoff direction, and the modded-`SCR_AIGroup` Manual-policy spawn guard — Manual groups now really never materialise unrequested). Final gates Fast 190 / All 236. |
| 4 | integration | ✅ Complete (Ready for Review) | 62/62 (100%) | First tracked-group consumers, built: deployments' group lifecycle (town patrol + both vehicle patrols) **and radio-tower garrisons** run on the layer, and the ad-hoc proximity code is gone (the deployment proximity toggle, `IsPlayerInRange`, the 40 m vehicle rule and the whole 460-line `OVT_EntitySpawningAPI.c`). Tower garrisons became `Configs/Deployment/Deployment_TowerGarrison.conf` rather than migrating in place, so the epic has **exactly one** tracked-group consumer seam. Core was extended once more, additively: an **entity-observer API** (`AddEntityObserver`/`RemoveEntityObserver`, `m_bRecruitGroupsAreObservers` default ON) whose consumer is a parked recruit squad. Deployment serializer **v2** (persisted virtual key, v1 payloads migrate on first use). Player-visible: dead members stay dead across despawn *and* save/load, town patrols keep walking while unobserved, a tower flips only on a real wipe, and a tower may be found ungarrisoned when the occupier is short of resources. Phases 1–8 complete pending final review; **All suite 255 green 2026-08-17**. Play-test §6 steps 1–13 and an MP pass are owed (see its `context.md`). |
| 5 | base-defense-migration | Planned | — | Complete the stalled base-upgrades→deployments migration (design phases 3–4) on virtualization and retire base-upgrades — scoped for visibility, deferrable without blocking the earlier features. **The deployments↔virtualization seam it depends on is now proven** (`integration`, 2026-08-17), and `Configs/Deployment/Deployment_TowerGarrison.conf` is its worked precedent: a bespoke garrison system turned into a four-module deployment config, with the spawning/behaviour/capture/reinforcement split, the control-condition module, the `RADIO_TOWER` location-type OR-in and the legacy spawner deleted with its enclosing block. Copy that shape rather than re-deriving it; note the two traps it recorded (behaviour-module order in a `.conf` is update order, and `m_bEnableReinforcement` is what keeps a lost deployment collectable). The two remaining `OVT-VIRT-PLAYTEST-ONLY` production guards (base upgrades ×2, QRF queue) are this feature's to remove. |

> Reference any feature with the slash form `virtualization/[feature-name]` (e.g. `/continue-feature virtualization/core`). Task counts are pulled from each feature's own `tasks.md` and refreshed by `/update-epic`.

---

## Build Order / Dependencies

1. **core** — Foundational: every other feature registers groups with, or reads state from, the manager it introduces. Also carries the epic's persistence contract (Persistence-tier round-trip test from day one) and the ambient spawn-source seam civilians consumes.
2. **civilians** — First consumer, deliberately straight after core: exercises the ambient (one-off, non-persisted, untracked) class and the declarative config surface before any combat migration, hardening core's extensibility seam early. Depends only on core.
3. **movement** — Depends on core's registered dormant groups (writable position, owned waypoint entities). Must exist before tracked-group consumers migrate, or migrated patrols would freeze in place while despawned. Independent of civilians (ambient spawns have no despawned life to advance).
4. **integration** — The vertical slice that proves the tracked-group API: real campaign systems (deployments' three configs, tower garrisons) running on the layer, with dead-member persistence player-visible. De-risks base-defense-migration by exercising the API against the deployments framework first.
5. **base-defense-migration** — Largest feature, deliberately last: needs integration's proven deployments↔virtualization seam. **Deferrable** — the epic ships standalone value without it; if deferred, the base-upgrades/deployments coexistence remains (status quo) but on one fewer ad-hoc virtualization.

**Dependencies between features:**
- core → civilians (ambient spawn-source seam; no movement dependency)
- core → movement (virtual tick advances core's records)
- core + movement → integration (consumers need both lifecycle and believable positions)
- integration → base-defense-migration (proven seam + migration tooling)
- civilians ∥ movement is the only parallelizable pair; everything else is sequential.
- External: **vanilla persistence (shipped in 1.4.0)** and **Arma Reforger 1.8.0.10** (the engine lifecycle core adopts; migrated 2026-08-13) are the hard prerequisites for core. Integration is cleaner if deployments' known lifecycle bugs (BUG-028 faction-list leak, world-time unit bugs) are fixed in 1.4.x first — noted in its requirements. The 1.8 behavioral-hardening backlog (`docs/reforger/1.8.0.10-changes.md`) fixes today's systems and stays separate from this epic's build.

---

## Integration & Architecture

- **Within the epic:** core owns the registry and consumer API, delegating group lifecycle to the 1.8 engine — two registration classes: tracked persistent groups (engine-lifecycle), and ambient (one-off, non-persisted, config-declared) spawn sources; movement is a tick strategy over core's dormant group entities; civilians, integration and base-defense-migration are consumer migrations that shrink other systems rather than growing this one. The deployment marker entity remains the durable record for *deployment-level* state; virtualization owns *group-level* state (members, position, waypoints) — one system per concern, no double bookkeeping.
- **With other epics / features:** occupying/deployments becomes the primary tracked-group consumer (its spawning modules delegate group lifecycle); occupying/core's radio-tower garrison code is replaced; occupying/base-upgrades is ultimately retired (feature 5). Towns' `OVT_TownControllerComponent` loses its civilian spawner to feature 2 (town data — population, range, location — stays the density input; `OVT_PatrolHarassmentStabilityModifier` must keep working across the migration). Economy 2.0 (future epic) is a **design-for consumer only**: core's abstractions must not preclude non-combat virtual agents (virtual citizen purchases, deliveries), but nothing agent-related is built here — ambient civilians are scenery, not simulated agents.
- **Key architectural decisions for the epic as a whole:**
  - **Engine-native lifecycle (1.8):** proximity spawn/despawn, budget arbitration, frame-spread spawning, dormancy and group persistence are the engine's (`ProximityDriven` + `SCR_EAISpawnImportance` + `SCR_AIGroupSerializer`); Overthrow builds the registry, config, ambient seam and consumer API on top. The dormant `SCR_AIGroup` entity *is* the durable group record.
  - **Rebuild, don't rebase:** the old `virtualization` branch is reference material only. Its salvaged lessons are now largely engine-provided (held-member protection supersedes the 40 m stolen-vehicle rule; the spawn queue supersedes hand-rolled frame-spreading; eliminated-before-init ordering survives as the wipe-bookkeeping contract).
  - **Vanilla persistence native — revised to Route B (2026-08-17, core Phase 5):** vanilla's own AI serializers proved unusable for runtime-spawned groups (no safe self-spawn path; Overthrow's BUG-118 untrack + `SelfSpawn 0`; class-wide opt-in would duplicate lazily-registered orphans on every load). Core's own `OVT_VirtualizationManagerSerializer` persists **full re-creation state** (composition, live position, waypoint plan, slot mask) and the manager rebuilds group entities on load. Still vanilla persistence infrastructure (`ScriptedComponentSerializer`), still no EPF concepts — but the registry payload, not the group entity, is the durable record.
  - **Server-only, no client surface:** nothing Overthrow adds replicates; map/UI presence for virtual units is explicitly out of scope. (Group entities replicate as vanilla always has.)
  - **Slot-accurate survivor truth:** groups respawn with exactly their surviving *slots* (roles and loadouts preserved) and are removed when wiped. Core owns a per-slot alive mask, records deaths by slot, and enforces refill via an `ExpandOneMember` override on the already-modded `SCR_AIGroup` (user decision 2026-08-14, revising an initial count-based acceptance). The mask also corrects an engine hazard: budget-under-filled groups would otherwise have missing members counted dead at despawn.
  - **No virtual combat:** despawned groups never fight, take damage, or resolve engagements virtually.

---

## Tech Debt / Findings

Cross-feature tech debt and review findings. **Populated and updated by `/review-epic`** (which also writes feature-specific findings into the relevant child `context.md` files). Start empty.

- (none yet — `/review-epic` will surface cross-feature integration issues and per-feature tech debt here)

---

## Master Overview Rollup

How this epic is represented in the project's master `docs/overview.md` (one row, not its children). Kept in sync by `/update-epic` and `/update-master`.

- **Rollup status:** In Progress (4/5 features complete — core ✅ 50/50, civilians ✅ 39/39, movement ✅ 22/22 play-test passed, integration ✅ 62/62 pending final review)
- **One-line summary for master:** The unified AI virtualization layer (issue #100, a thin registry over Reforger 1.8's engine-native group lifecycle): **core, civilians, movement and integration built** — registry/API + survivor masks + frozen contract, town-civilian ambience, infantry virtual movement (play-test passed), and now the first real campaign consumers: deployments' three configs plus radio-tower garrisons (a new deployment config) run on the layer, `OVT_EntitySpawningAPI.c` and every ad-hoc proximity toggle are deleted, an entity-observer API lets a parked recruit squad hold content awake, and dead members stay dead across despawn AND save/load. All suite **255 green 2026-08-17**; only the deferrable base-upgrades→deployments migration remains.

---

*This is a required file — do not delete it; it marks the folder as an epic. Update it with `/update-epic virtualization` after working on the epic's features, and run `/review-epic virtualization` to refresh the Tech Debt / Findings section.*
