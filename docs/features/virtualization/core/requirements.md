# Core — Requirements

**Epic:** virtualization
**Created:** 2026-08-03

## Overview

The foundational virtualization manager: a server-only singleton (working name `OVT_VirtualizationManagerComponent`, on the game mode per the Manager pattern) that owns virtual group records and their spawn/despawn lifecycle. Every other feature in the virtualization epic registers groups with it, ticks through it, or migrates consumers onto it — and it carries the epic's persistence contract from day one.

## Requirements

- A **virtual group record** holds: owning faction, composition source (faction group registry key / prefab), **per-member alive state**, current virtual position, a waypoint plan, and an owner/purpose tag identifying which system created it (deployment, tower garrison, …).
- A registration API lets other systems create, destroy, query, and reclaim virtual groups without touching record internals — the seam features 3–4 and future consumers program against.
- Groups **spawn when any player is within the configured spawn distance** of their virtual position and **despawn when no player is**, with spawn/despawn work spread across frames (no hitch when a player fast-travels into a dense area).
- Spawning materializes **only surviving members**; members killed while spawned are recorded on despawn (or death); a fully wiped group's record is removed and never respawns.
- **Spawn distance is server-configurable** (Overthrow config/difficulty settings, not a code constant); a very large value effectively keeps all AI spawned, per issue #100's server-owner ask.
- All records persist through a **vanilla-persistence serializer** (patterns per `docs/features/core/persistence/vanilla-api-reference.md`), covered by a Persistence-tier round-trip test proven able to fail — shipping in the same feature, not later.
- Server-only: no state replicates to clients; nothing in the design assumes a client-side view.
- The record/lifecycle abstraction must not preclude future **non-combat virtual agents** (Economy 2.0 citizens/deliveries) — document the extensibility seam; build nothing agent-specific.

## Dependencies

- Vanilla persistence (core epic, shipped v1.4.0) — hard prerequisite.
- `OVT_Faction` group registries for composition resolution.
- First feature in the epic — no sibling dependencies; nothing else can start before it.

## Out of Scope

- Virtual movement of despawned groups (feature: `movement`) — core records hold position/waypoints but do not advance them.
- Migrating any existing system onto the layer (features: `integration`, `base-defense-migration`).
- Virtual civilians/economy agents, client-visible UI, virtual combat resolution (epic-level exclusions).
- Salvaging code from the old `virtualization` branch — design lessons only (eliminated-flag ordering, frame-spread spawning).
