# Framework — Requirements

**Epic:** missions
**Created:** 2026-08-13

## Overview

The core mission runtime that every other feature in the missions epic consumes: a declarative, config-driven module system with branching, a server-authoritative mission instance state machine, co-op participant tracking, reward distribution, and its own replication + persistence. This is the ground-up replacement for the legacy jobs backend — do **not** model the architecture on `OVT_JobManagerComponent`; only the idea of config-as-data survives.

## Requirements

- Missions are defined entirely in config (Workbench-authorable, no code) as composable modules; entire missions definable from pieces, like jobs were but more capable.
- **Branching:** a mission can branch as many times as needed — player actions/choices route the mission instance down different paths. Linear `stage++` is not the model.
- **Stable identity:** every mission config carries a stable string `m_sId`; instances have their own runtime identity that works across the wire and in saves (no positional indices, no identity split across layers like jobs had).
- **Co-op first:** any number of players and/or groups can participate in one mission instance; participants can join mid-mission; participants are tracked by **persistent ID** (missions outlive connections).
- **Rewards at any point**, not only on completion: modules can grant cash, XP or items mid-mission. Distribution policies: split evenly among participants, closest player, triggering player, finding player, or a specific player. Add a persistent-id XP award helper (money already has `AddPlayerMoneyPersistentId`; XP has no offline-safe path).
- **Mission items:** mission-specific items (documents etc.) can be placed in the world for players to pick up and deliver to a location.
- **Spawning/offer conditions:** config-driven conditions decide when/where missions spawn (town/base scoped, support/stability/random gates — at least the expressive power the 7 legacy jobs' conditions needed).
- **Module library for parity:** enough condition/spawn/wait/placeable/waypoint modules to express all 7 legacy jobs (the 7 orphaned legacy framework classes are NOT ported).
- **Replication:** server drives all mission state; clients receive list + instance state incl. JIP (RplSave/RplLoad); the current waypoint per participant is derivable client-side from replicated state.
- **Persistence:** own `ScriptedComponentSerializer` registered in `Configs/Systems/Persistence/Overthrow.conf`, following the rules proven in `OVT_JobManagerSerializer.c` (version-first, positional order, frozen record classes, idempotent apply, no RPC in apply path). **Restore is state assignment, never module replay** — modules with spawn side effects must be explicitly non-restorable or re-derivable.
- **Intel hooks:** expose mission lifecycle ScriptInvokers and a module seam for changing/reading intel levels (future Intel epic, GH #11) — no intel implementation here.
- Client→server operations go on a new `OVT_MissionRequestComponent` on `OVT_OverthrowController` (never `OVT_PlayerCommsComponent`).
- Testable: logic-tier tests for branching/distribution maths, init-tier config-resolve + id-uniqueness guards (pattern: `OVT_TEST_Init_Jobs_StableIdsAreUniqueAndResolve`), persistence round-trip coverage.

## Dependencies

- Nothing within the epic — this is feature #1 and foundational for all siblings.
- Existing managers consumed via their current APIs: towns, occupying faction, economy, skills, players, placeables, notifications.

## Out of Scope

- All player-facing UI (mission list, menus, map waypoint rendering) — `missions/mission-ui`.
- Shipped mission content beyond a minimal test mission — `missions/mvp-missions`.
- Officer authoring, escrow, group assignment — `missions/resistance-missions`.
- Recruit-granting modules, dialog modules, Workbench tooling — later features.
- Jobs system removal — happens in `missions/mvp-missions`.
