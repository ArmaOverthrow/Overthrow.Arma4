# Mission UI — Requirements

**Epic:** missions
**Created:** 2026-08-13

## Overview

The player-facing loop for missions: a mission list/detail menu, accept/join/abandon flows, automatic map waypoints for participants, and notifications. This is the first playable vertical slice of the epic — one test mission driven end-to-end (list → accept → waypoint → progress → reward) proves the framework before content is authored at scale.

## Requirements

- Mission list + detail view (new UI context + layouts, replacing `OVT_JobsContext`/`JobsMenu.layout` patterns) showing title, location, rewards, participants, and **current objective/progress text** — the legacy UI gave zero in-progress guidance for multi-stage jobs; missions must always tell participants what to do now.
- Accept / join (mid-mission) / abandon flows via `OVT_MissionRequestComponent` on `OVT_OverthrowController`.
- **Reactive UI:** the open menu updates when mission state changes (invoker-driven), not rebuild-only-on-open like the jobs menu.
- **Automatic waypoints:** the current mission waypoint (when the active stage has a location) is always shown on the map for all participants — no manual "show on map". Implement as an `OVT_MapLocationType` subclass with `m_fRefreshInterval`, populated from the local player's replicated mission state, registered in `Configs/Map/OverthrowMap.conf`. Multiple concurrent missions must not overwrite each other's markers (the single-slot `m_vCurrentWaypoint` anti-pattern is retired).
- Notifications for mission lifecycle (offered, stage advanced, reward granted, completed, failed) via `OVT_NotificationManagerComponent` presets (`Configs/overthrowBroadcastMessages.conf`); menus subscribe to `m_OnNotification` since the HUD strip hides under UI contexts.
- Full gamepad/console usability (keybindings in `chimeraInputCommon.conf`, navigation per `overthrow-ui-patterns`).
- New strings added to `Language/localization_Overthrow.st` only (never the runtime exports).

## Dependencies

- `missions/framework` must exist (mission state, replication, `OVT_MissionRequestComponent`).
- Cannot be built in parallel with framework's core, but layout/context scaffolding can start once framework's client-side data model is stable.

## Out of Scope

- Officer mission-authoring UI — `missions/resistance-missions`.
- Mission content — `missions/mvp-missions`.
- Dialog UI — `missions/dialog`.
- Removing the jobs menu (stays until teardown in `missions/mvp-missions`).
