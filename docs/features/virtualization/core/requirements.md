# Core — Requirements

**Epic:** virtualization
**Created:** 2026-08-03
**Amended:** 2026-08-14 — replanned around Reforger 1.8's native AI group lifecycle system (user-approved: hybrid adoption, slot-accurate member truth via a core-owned mask + `ExpandOneMember` override). See `docs/reforger/1.8.0.10-changes.md`.

## Overview

The foundational virtualization manager: a server-only singleton (working name `OVT_VirtualizationManagerComponent`, on the game mode per the Manager pattern) that owns virtual group registration and configuration **on top of Reforger 1.8's engine-native group lifecycle** (`SCR_EAIGroupLifecyclePolicy.ProximityDriven`, the importance-tiered budgeted spawn queue, engine dormancy with survivor counts, and vanilla group persistence). Every other feature in the virtualization epic registers groups with it or migrates consumers onto it — and it carries the epic's persistence contract from day one.

## Requirements

- A **virtual group registration** holds: owning faction, composition source (faction group registry key / prefab), **per-slot alive state** (the authoritative survivor mask), an owner/purpose tag identifying which system created it (deployment, tower garrison, …), and per-registration overrides (spawn distance, spawn importance). Engine-side lifecycle state — survivor counts, position, waypoints — lives on the dormant `SCR_AIGroup` entity, which 1.8 keeps alive through despawn precisely to be that record; the mask refines the engine's counts to identities.
- A registration API lets other systems create, destroy, query, and reclaim virtual groups without calling engine lifecycle APIs directly — the seam the epic's consumer features (`civilians`, `integration`, `base-defense-migration`) and future consumers program against.
- Groups **spawn when any observer is within the configured spawn distance** of their position and **despawn when none is**, with spawn work frame-spread and budget-arbitrated (no hitch when a player fast-travels into a dense area) — delegated to the engine's `ProximityDriven` policy and spawn queue, verified in Overthrow's world rather than hand-rolled.
- Spawning materializes **exactly the surviving slots** (slot-accurate truth — roles and loadouts preserved, enforced by an `ExpandOneMember` override on the already-modded `SCR_AIGroup`); members killed while spawned are recorded by slot the moment they die; a fully wiped group's record is removed and never respawns.
- Every registration carries an explicit **spawn importance** (`SCR_EAISpawnImportance`) so campaign AI is not starved at vanilla's default LOW budget tier.
- **Spawn distance is server-configurable** (Overthrow config/difficulty settings, not a code constant); a very large value effectively keeps all AI spawned, per issue #100's server-owner ask.
- Group state persists through **vanilla's `SCR_AIGroupSerializer`** (already connected via `Common.conf`); the manager's registry bookkeeping (handles, owner tags, overrides) persists through a small **Overthrow serializer** that relinks groups by persistence UUID — both covered by Persistence-tier round-trip tests proven able to fail, shipping in the same feature, not later.
- Alongside tracked records, the layer supports a second registration class: **ambient spawn sources** — declarative, config-defined spawners that materialize one-off, **non-persisted, untracked** entities by the same proximity/frame-spread lifecycle (no per-member records, no serializer coverage; despawn discards, respawn re-rolls from config). Sources are declared via config classes modders can extend without script changes, and support **ownership transfer** (an entity a player claims — e.g. a recruited civilian, a taken vehicle — leaves ambient management instead of being deleted). First consumer: `virtualization/civilians`; core ships the seam plus an Init-tier assertion that a registered source resolves, not any civilian content.
- Server-only: no state replicates to clients; nothing in the design assumes a client-side view.
- The record/lifecycle abstraction must not preclude future **non-combat virtual agents** (Economy 2.0 citizens/deliveries) — document the extensibility seam; build nothing agent-specific.

## Dependencies

- **Arma Reforger 1.8.0.10** — the engine lifecycle/budget/dormancy system this feature adopts (migrated 2026-08-13; `ObserversSystem` presence in Overthrow's world is a Phase 1 spike verification).
- Vanilla persistence (core epic, shipped v1.4.0) — hard prerequisite; 1.8 extends it to AI groups/waypoints via `Common.conf`.
- `OVT_Faction` group registries for composition resolution.
- First feature in the epic — no sibling dependencies; nothing else can start before it.

## Out of Scope

- Virtual movement of despawned groups (feature: `movement`) — core records hold position/waypoints but do not advance them.
- Migrating any existing system onto the layer (features: `civilians`, `integration`, `base-defense-migration`) — core ships seams, not consumers; no civilian configs, prefab pools, or town wiring here.
- Economy 2.0 virtual agents, client-visible UI, virtual combat resolution (epic-level exclusions).
- Salvaging code from the old `virtualization` branch — design lessons only (eliminated-flag ordering, frame-spread spawning).
