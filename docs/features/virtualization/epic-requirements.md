# Virtualization - Epic Requirements

**Created:** 2026-08-03
**Phase:** Planned (post-1.4.0 candidate — sequencing vs new-map to be decided after 1.4.0 ships)

> Epic-level requirements — the higher-level scope for the whole epic, mirroring a feature's `requirements.md` but one level up. `/plan-epic virtualization` reads this file (if it exists) to drive epic scoping; otherwise it uses the prompt. Each **child feature** below gets its own `requirements.md` (in the standard Overview / Requirements / Dependencies / Out of Scope shape) that `/plan-feature virtualization/[feature-name]` consumes.

## Overview

Build the single AI virtualization layer from scratch on vanilla persistence, keeping the general aim of GitHub issue #100 (virtualize AI groups, spawn/despawn by player proximity, virtual movement while despawned) while **throwing away the EPF-based `virtualization` branch**. The epic converges the occupying epic's three ad-hoc virtualization implementations onto one system, unblocks — and visibly scopes — the stalled base-upgrades→deployments migration, and lays design (not code) groundwork for the virtual agents Economy 2.0 will need.

## Requirements

- One server-side virtualization layer owns AI group lifecycle end-to-end: virtual record → proximity spawn → live AI → despawn back to record. The four ad-hoc implementations (base-upgrades value banking, deployments proximity toggling, radio-tower garrison spawn/despawn, and the towns system's ambient civilian spawner) converge onto it — via this epic's `civilians`, `integration` and `base-defense-migration` features — so no new ad-hoc virtualization is ever added again.
- Besides tracked persistent group records, the layer supports **ambient spawn sources**: declarative, config-defined, modder-extendable spawners for one-off, non-persisted, untracked entities (despawn discards, respawn re-rolls), with ownership transfer for entities players claim. Town civilians migrate onto this class with runtime operator-tunable density and richer town life (variety, behavior archetypes, believable placement; stretch: ambient parked vehicles).
- **Dead group members stay dead:** per-member alive state is tracked, survives despawn/respawn cycles and save/load, and a fully wiped group never returns.
- **Server owners control spawn distance** via configuration; setting a very large distance effectively disables despawning for beefy servers (issue #100's explicit ask).
- Despawned groups keep moving believably: straight-line fixed speed for infantry, road-network-following for vehicle groups, with valid (ground/road, never water) spawn placement when players approach.
- All virtualization state persists through **vanilla persistence** (native serializers per `docs/features/core/persistence/vanilla-api-reference.md`), with automated Persistence-tier round-trip coverage from feature 1 onward. No EPF concepts.
- The old `virtualization` branch is reference-only; salvage design lessons (eliminated-flag ordering, 40 m stolen-vehicle rule, frame-spread spawning), never code.
- Core abstractions must not preclude future non-combat virtual agents (Economy 2.0 virtual citizen purchases / virtual deliveries) — a documented extensibility seam, with nothing agent-related built in this epic.
- Existing campaigns survive each migration step: saves made before a feature lands load correctly after it (patrols/garrisons re-establish rather than vanish).

## Planned Features

The features that make up this epic, in intended **build order**. `/plan-epic` creates a subfolder + `requirements.md` for each, and records the order in `epic-overview.md`.

1. **core** — Virtualization manager: group records with per-member state, proximity spawn/despawn, configurable spawn distance, vanilla-persistence serializer, plus the ambient spawn-source seam — foundational; everything else registers with or reads from it.
2. **civilians** — Town civilians migrate off `OVT_TownControllerComponent` onto the ambient spawn-source class: config-driven, runtime operator-tunable density, prefab variety, behavior archetypes, believable placement (stretch: ambient parked vehicles) — first consumer; depends only on core.
3. **movement** — Virtual movement engine (infantry straight-line, vehicles road-aware) + spawn placement + live-AI waypoint handoff — depends on core's records; must precede tracked-group consumers so migrated groups don't freeze while despawned.
4. **integration** — Deployments' three configs and radio-tower garrisons migrate onto the layer; ad-hoc proximity code retired; dead-stay-dead becomes player-visible — the vertical slice that proves the tracked-group API before the big migration.
5. **base-defense-migration** — Complete the base-upgrades→deployments migration (design phases 3–4) on virtualization and retire base-upgrades — scoped last for schedule visibility; **deferrable** without blocking the epic's standalone value.

## Dependencies

- **Vanilla persistence** (core epic, shipped in v1.4.0) — hard prerequisite; this epic must not start before 1.4.0's persistence is stable in the wild.
- **occupying/deployments** — the framework integration builds on; its lifecycle bugs (BUG-028 faction-list leak, world-time unit mismatches) should be fixed in 1.4.x before feature 3.
- **occupying/core** — radio-tower garrison code (replaced by `integration`), faction resource income (funds deployments).
- **towns/core** — `OVT_TownControllerComponent` civilian spawner (replaced by `civilians`); town population/range/location data remains the density input.
- Vanilla road-network query API (feature 2) and `OVT_Faction` group registries (feature 1).
- Reference material: GitHub issue #100, the discarded `virtualization` branch, `docs/archive/ModularDeploymentSystem.md` (the never-executed migration strategy).

## Out of Scope

- **QRF migration in any form** (including a virtual travel phase) — QRFs stay live-spawned; their debt stays in the occupying epic.
- **Economy 2.0 virtual agents** (virtual purchases/deliveries) — design-for only; those simulation systems belong to the Economy 2.0 epic. (Ambient town civilians — one-off, non-persisted scenery — are **in** scope via the `civilians` feature; simulated civilian *agents* are not.)
- **Client-visible surface** — no replication, map markers, or UI for virtualized units.
- **Virtual combat** — despawned groups never fight, take damage, or resolve engagements.
- **Resistance/civilian-side deployments** — the deployments framework's faction-agnostic promise stays unexercised here.
- **AI driving improvements** (issue #71) — road-aware movement is virtual maths, not better live drivers.

---

*Consumed by `/plan-epic virtualization`. After planning, run `/plan-feature virtualization/[feature-name]` per feature in the recommended order.*
