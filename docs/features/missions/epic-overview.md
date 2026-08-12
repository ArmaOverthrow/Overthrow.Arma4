# Missions - Epic Overview

**Epic:** missions
**Status:** 🟡 Planned
**Last Updated:** 2026-08-13

> **This file is the epic marker.** Its presence in `docs/features/missions/` is what tells every Beast Mode command (and future Web App / Discord clients) that this folder is an **epic**, not a plain feature. Keep it present and keep the required sections below filled in. It is the epic's equivalent of the project's `docs/overview.md`, scoped to this epic. The master `docs/overview.md` tracks this epic as a **single row**; the per-feature detail lives **here**.

---

## Purpose

Missions completely replaces the legacy Jobs system — the most problematic system in Overthrow since inception. Only the *idea* of jobs survives: declarative, config-driven, modular missions authorable with no code. Everything else is redesigned: missions branch on player choices, support any number of players and groups co-operating (with reward distribution policies instead of a single owner), can grant rewards or world-placed mission items at any point, auto-show their current waypoint on the map, and sit on a new, solid backend + persistence framework. The epic also delivers resistance missions (officer-created, escrow-funded, group-assignable) and feeds the future Intel epic (GH issue #11) through explicit hook seams. The jobs system is nuked when missions release — parity and teardown ship together in `mvp-missions`.

---

## Features

The constituent features of this epic, in build order. Each feature is a subfolder under `docs/features/missions/`.

| # | Feature | Status | Tasks | Description |
|---|---------|--------|-------|-------------|
| 1 | framework | Planned | — | Core mission runtime: branching config schema, server-authoritative instance state machine, participant tracking, reward modules + distribution policies, world-placed mission items, replication + persistence |
| 2 | mission-ui | Planned | — | Mission list/detail menu with per-stage progress text, accept/join/abandon via a new controller comms component, automatic participant map waypoints, notifications |
| 3 | mvp-missions | Planned | — | Job-parity mission configs (7) + showcase branching/co-op missions, and full teardown of the legacy Jobs system |
| 4 | resistance-missions | Planned | — | Officer-created missions: authoring UI from config templates, resistance-fund escrow, group/open assignment, per-kill rewards, MP-only |
| 5 | recruit-missions | Planned | — | Post-MVP: missions that grant temporary or permanent recruits (rescue missions) |
| 6 | dialog | Planned | — | Stretch: simple text-only Q&A dialog module for mission NPCs |
| 7 | authoring-tools | Planned | — | Stretch: Workbench tooling for mission authoring (possibly leveraging the behavior-tree node editor) |

> Reference any feature with the slash form `missions/<feature-name>` (e.g. `/plan-feature missions/framework`). Task counts are pulled from each feature's own `tasks.md` and refreshed by `/update-epic`.

---

## Build Order / Dependencies

1. **framework** — Foundational: every other feature consumes its config schema, runtime, participant model and persistence. Rewards/distribution are core co-op semantics and live here, not in a follow-up.
2. **mission-ui** — First playable vertical slice: one test mission end-to-end (list → accept → waypoint → progress → reward) proves the framework before content is authored at scale.
3. **mvp-missions** — The release gate: parity with the 7 shipped legacy jobs plus showcase missions, and the jobs teardown in the same feature so parity and removal are verified together.
4. **resistance-missions** — Builds on a proven module system; adds the officer/escrow/assignment layer and its own authoring UI.
5. **recruit-missions** — Post-MVP; depends only on framework + `OVT_RecruitManagerComponent`.
6. **dialog** — Stretch; a new module type, independent of 5 and 7.
7. **authoring-tools** — Stretch; zero runtime impact, can be built any time after the config schema stabilises.

**Dependencies between features:**
- framework → everything (config schema, runtime, participant/reward model, persistence)
- mission-ui → mvp-missions (content needs the player-facing loop to be testable)
- framework + mission-ui + mvp-missions → resistance-missions (officer layer assumes a stable module system)
- recruit-missions, dialog, authoring-tools are mutually independent and parallelisable once their prerequisites exist
- External: the future Intel epic (GH #11) consumes framework's hook seams but is **not** a dependency of anything here

---

## Integration & Architecture

Epic-wide decisions every `/plan-feature` must respect (grounded in exploration of the existing code):

- **Within the epic:** a new manager `OVT_MissionManagerComponent` on the game mode owns all mission state; `OVT_Global.GetMissions()` replaces the `GetJobs()` slot (`OVT_Global.c:269`). Every mission config carries a stable string `m_sId` from day one — the jobs v1→v2 save migration exists because positional indices don't survive config reorders.
- **Client→server:** never `OVT_PlayerCommsComponent` (deprecated). New `OVT_MissionRequestComponent` (player: accept/join/abandon) and `OVT_MissionAuthoringComponent` (officer: create/fund/assign/cancel) on `OVT_OverthrowController` — template: `OVT_TravelRequestComponent.c` (identity resolved server-side from controller ownership; minimal distinct RPC signatures per BUG-090).
- **Participants tracked by persistent ID**, not player ID — co-op missions outlive individual connections. Money already has an offline-safe path (`AddPlayerMoneyPersistentId`); XP does not — framework adds a persistent-id XP helper alongside `OVT_SkillManagerComponent.GiveXP`.
- **Escrow lives inside the missions manager**, not the economy manager: debit the treasury via `DoTakeResistanceMoney` at mission creation, refund on cancel; the escrowed balance persists in the missions serializer (the economy serializer stays at v1). `DoTakeResistanceMoney` does not clamp at zero — guard before per-kill payouts.
- **Group assignment identity** = the leader's persistent ID ("the leader's current group"). Vanilla `SCR_AIGroup.GetGroupID()` is runtime-scoped and group membership is deliberately never persisted (`OVT_PlayerGroupManagerComponent` decision D2).
- **MP-only gate for resistance missions:** check `RplSession.Mode()` (precedent `OVT_OverthrowGameMode.c:1048`) — SP/listen-host player 1 is always auto-officer, so `IsOfficer` alone cannot exclude single-player.
- **Map waypoints:** subclass `OVT_MapLocationType` with `m_fRefreshInterval`, populated from the local player's replicated mission state, registered in `Configs/Map/OverthrowMap.conf` — automatic, no click-to-show, no new network traffic (replaces the client-local `m_vCurrentWaypoint` anti-pattern).
- **Notifications** go through `OVT_NotificationManagerComponent` (jobs bypassed it — the only subsystem that did); mission menus subscribe to `m_OnNotification` since the HUD strip hides under UI contexts.
- **Persistence:** own `ScriptedComponentSerializer` registered in `Configs/Systems/Persistence/Overthrow.conf`, following the hard rules encoded in `OVT_JobManagerSerializer.c` — version written first, positional field order, frozen record classes, stable ids not indices, idempotent apply, no RPC in the apply path, and **restore is state assignment, never replay**. Branching makes the replay hazard sharper: any module with spawn side effects must be explicitly non-restorable/re-derivable.
- **With other epics/features:** framework exposes mission lifecycle ScriptInvokers + an intel-level-change module seam for the future Intel epic (GH #11) — intertwined but independent. Missions consume towns (support/stability), occupying-faction (bases, towers), economy, skills, recruits and placeables through their existing manager APIs, same surface the jobs system used.

---

## Tech Debt / Findings

Cross-feature tech debt and review findings. **Populated and updated by `/review-epic`.** Start empty.

- (none yet — `/review-epic` will surface cross-feature integration issues and per-feature tech debt here)

---

## Master Overview Rollup

- **Rollup status:** Planned (0/7 features)
- **One-line summary for master:** Config-driven branching co-op mission system replacing the legacy Jobs system, including officer-created escrow-funded resistance missions.

---

*This is a required file — do not delete it; it marks the folder as an epic. Update it with `/update-epic missions` after working on the epic's features, and run `/review-epic missions` to refresh the Tech Debt / Findings section.*
