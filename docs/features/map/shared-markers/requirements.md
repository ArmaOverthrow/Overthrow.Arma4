# Shared Map Markers — Requirements

**Epic:** map
**Created:** 2026-08-10
**Type:** Stretch goal (sequenced after `map/legacy-retirement`)

## Overview

This feature lets players **mark things on the map for each other**: place a networked marker, see teammates' markers, and see where teammates are. Overthrow is a co-op campaign whose players currently have no in-game way to point at a location — coordination happens entirely out of band over voice.

The important property of this feature is that it is mostly **integration, not construction**. Arma Reforger already ships the whole stack: `SCR_MapMarkerManagerComponent`, `SCR_MapMarkerSyncComponent` (networked marker replication), `SCR_MapMarkerSquadLeaderComponent` / `SCR_MapMarkerSquadMemberComponent`, `SCR_MapMarkersUI`, and a config-driven marker entry system (`SCR_MapMarkerConfig`). Overthrow touches this API in exactly one place today — and that one place does not actually use it (see below).

## Requirements

- Let a player **place, move and remove a map marker** that other players can see, using vanilla's marker stack rather than a bespoke Overthrow implementation.
- Marker visibility must respect Overthrow's own grouping — markers should be shared with the player's group/team as defined by `core/player-groups`, not broadcast indiscriminately, unless a deliberate "everyone" scope is chosen and documented.
- **Squad/teammate positions** should be visible on the map where the vanilla squad marker components support it, so players can see each other without placing markers manually.
- Markers must **survive join-in-progress**: a player joining an established campaign must receive the markers already placed. Verify whether `SCR_MapMarkerSyncComponent` handles this natively or whether Overthrow must drive it.
- Decide and document whether markers **persist across a server restart**. If they should, they go through the persistence system; if not, say so explicitly so their disappearance is not later filed as a bug.
- **Fix the existing dead marker call.** `OVT_RecruitsContext.ShowOnMap` (`:479-491`) fetches `SCR_MapMarkerManagerComponent.GetInstance()`, null-checks it, and then never uses it — instead it writes the recruit's position into `OVT_JobManagerComponent.m_vCurrentWaypoint`. That is a stub where a real marker was intended, and reusing the job waypoint slot appears to **clobber an active job's waypoint**. "Show recruit on map" should place a real marker. Confirm the clobbering behaviour and file it as a bug if it reproduces.
- Marker placement and selection must be **fully operable on gamepad/console**, using vanilla's marker menu where possible rather than a mouse-only affordance.
- Markers must integrate with `map/map-layers` as a toggleable category if that feature exists.
- Must not conflict with Overthrow's own map elements — a placed marker must be distinguishable from a location marker and must not break `OVT_MapLocationElement` selection or the info panel.
- Verify behaviour with **two clients against a local dedicated server** (`tools/launch-server.sh` plus two `tools/launch-game.sh --profile` clients) — this feature is meaningless if it is only tested single-player, and marker sync is precisely the class of thing that works locally and fails over the wire.

## Dependencies

- **`map/core`** — the map UI the markers coexist with; marker input must not fight `OVT_MapLocationElement` selection.
- **`map/legacy-retirement`** — stretch goal, sequenced after the core epic completes.
- **`core/player-groups`** — defines who "my team" is for marker scoping and squad markers.
- **Vanilla marker stack** — `SCR_MapMarkerManagerComponent`, `SCR_MapMarkerSyncComponent`, `SCR_MapMarkerSquadLeaderComponent`/`MemberComponent`, `SCR_MapMarkersUI`, `SCR_MapMarkerConfig` (reference tree: `ArmaReforger/scripts/Game/Map/Markers/`). Establish what vanilla gives for free **before** designing anything; the value of this feature depends on it.
- **`jobs/core`** — the job waypoint slot currently misused by `OVT_RecruitsContext.ShowOnMap`.
- **`dev-ops/mp-testing`** — the two-client harness this feature must be verified on.
- Buildable in parallel with `map/territory-overlay`; only its legend/toggle registration depends on `map/map-layers`.

## Out of Scope

- **Freehand map drawing.** Lines, shapes and annotations beyond point markers are deferred.
- **A bespoke Overthrow marker system.** If vanilla's stack cannot do something, prefer dropping that capability over reimplementing the stack — the whole premise of this feature is leverage.
- **Voice or text chat integration**, ping wheels, or any communication feature beyond the markers themselves.
- **AI/recruit reaction to markers** — markers are player-to-player communication; making recruits move to a marker is a `resistance/recruits` concern, not this one.
- **Enemy or intel markers** — what the campaign reveals to the player belongs to the future intel epic.
