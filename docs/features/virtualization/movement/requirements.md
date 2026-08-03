# Movement — Requirements

**Epic:** virtualization
**Created:** 2026-08-03

## Overview

The virtual movement engine: advances despawned groups along their waypoint plans so the world keeps moving while nobody watches, and hands off cleanly to live AI when players approach. Infantry moves in straight lines at fixed speed (issue #100's model); vehicle groups follow the road network so they spawn where vehicles believably are — the decided upgrade over the old branch's straight-line-only movement.

## Requirements

- **Infantry groups** advance along their waypoint plan in straight lines at a fixed, configurable speed while despawned.
- **Vehicle groups** advance along a road-network route between waypoints at a fixed cruise speed, falling back to straight-line where no road route exists.
- The virtual tick is cheap: route legs are computed once and cached per leg — no per-tick pathfinding, no per-frame per-group work; cost stays flat as group count grows.
- **Spawn placement is always valid:** snapped to ground, vehicles on or beside a road, never in water; a group mid-leg spawns at its interpolated position along the route, not at the last waypoint.
- **Handoff both directions:** on spawn, remaining waypoints transfer to live AI waypoints so the group continues naturally; on despawn, current position and route progress are captured back into the core record.
- Route/progression maths that can be made world-free gets Logic-tier test coverage; road queries and placement are verified by Init-tier assertions where assertable, with movement *feel* left to play-testing (specific test steps documented).

## Dependencies

- `virtualization/core` must be complete — this feature is a tick strategy over core's records.
- Vanilla road-network query API (as already used by the deployments framework's road queries).
- Must land before `integration` — migrated patrols would otherwise freeze in place while despawned.

## Out of Scope

- Improving live AI driving (issue #71) — this is virtual maths, not driver behavior.
- Full pathfinding for infantry (terrain-aware virtual walking) — straight-line is the accepted fidelity.
- Virtual combat or detection while moving (virtual groups never fight or spot).
- Deciding *what* waypoint plans groups get — owners (deployments, garrisons) author plans; this feature only executes them.
