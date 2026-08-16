# Movement — Requirements

**Epic:** virtualization
**Created:** 2026-08-03
**Amended:** 2026-08-17 (user decision — vehicle groups are never virtually moved; see below)

> **2026-08-17 amendment (user).** Virtual **vehicle** movement is dropped from this feature.
> Two reasons: (1) the engine exposes **no script-callable route-finding API** — the entire road
> surface is `GetClosestRoad` / `GetRoadsInAABB` / `GetReachableWaypointInRoad` +
> `BaseRoad.GetWidth`/`GetPoints`, so road-following would have meant hand-building a road graph +
> A* in script; (2) the campaign needs **insertion/extraction** (QRF and deployments): a live
> vehicle delivers a group to its destination, and only then does the group enter normal
> virtualization — so vehicle transit is live-spawned by design anyway. Vehicle groups that must
> traverse the map **stay materialised** (registered with a huge `spawnDistanceOverride`, which core
> supports today) and real AI drives real roads. Movement's tick gate is `IsSpawned()`, so
> always-spawned groups are skipped by construction — no vehicle/infantry flag is needed.

## Overview

The virtual movement engine: advances despawned **infantry** groups along their waypoint plans in
straight lines at a fixed speed (issue #100's model) so the world keeps moving while nobody watches,
and hands off cleanly to live AI when players approach. Vehicle groups are out of scope — they stay
spawned and drive live (see the 2026-08-17 amendment above).

## Requirements

- **Infantry groups** advance along their waypoint plan in straight lines at a fixed, configurable
  speed while despawned. Groups whose plan has nothing to advance (empty plan, DEFEND-only garrison)
  are stationary and skipped.
- **Vehicle groups are never virtually moved** — they stay spawned (huge `spawnDistanceOverride`)
  and live AI drives them; movement must skip spawned groups by construction (`IsSpawned()` gate).
- The virtual tick is cheap: no per-frame per-group work; cost stays flat as group count grows
  (round-robin slicing per core's ambient-tick precedent).
- **Route progress is stateless across save/load:** core already persists the live group origin;
  after a load, movement re-derives the nearest plan leg by projecting the restored position onto
  the plan polyline and continues. No movement serializer, no payload (user decision 2026-08-17).
- **Spawn placement is always valid:** every position movement writes is snapped to ground and never
  in water, so a group mid-leg materialises at a valid interpolated position along its route, not at
  the last waypoint.
- **Handoff both directions:** largely native under the 1.8 model (2026-08-14 replan) — waypoint
  entities stay attached to the dormant group across despawn/respawn, so movement's job is advancing
  the dormant group entity's **origin** (via core's `SetPosition`) along its plan; on
  materialisation the engine spawns members at that origin and live AI resumes the same waypoints.
  Route progress bookkeeping (current leg, interpolation, wait timers) is movement's own transient
  state.
- **Insertion/extraction seam (design-for, built in `integration`):** a group registered into
  virtualization at its live delivery position is auto-adopted by movement's tick on the next pass —
  that auto-adoption *is* the handoff contract. This feature documents the seam; the delivery
  mechanics (convoy dispatch, arrival detection, extraction) belong to `integration`'s consumers.
- Route/progression maths that can be made world-free gets Logic-tier test coverage; ground/water
  placement is verified by Init-tier assertions where assertable, with movement *feel* left to
  play-testing (specific test steps documented).

## Dependencies

- `virtualization/core` must be complete — this feature is a tick strategy over core's registered
  dormant group entities (writable position, owned waypoints; frozen `api.md` §10 "movement" table).
  One **additive** core ask is expected: a way to enumerate all registered handles (only per-owner /
  per-system finders exist today).
- Must land before `integration` — migrated patrols would otherwise freeze in place while despawned.

## Out of Scope

- **Virtual vehicle movement / road-network routing in any form** (2026-08-17 amendment) — vehicle
  groups stay spawned; there is no script road-routing API and none is built here.
- **Insertion/extraction mechanics** (convoy dispatch, delivery detection, extraction) — designed
  for here as a seam, built in `integration`.
- Improving live AI driving (issue #71).
- Full pathfinding for infantry (terrain-aware virtual walking) — straight-line is the accepted
  fidelity.
- Virtual combat or detection while moving (virtual groups never fight or spot).
- Deciding *what* waypoint plans groups get — owners (deployments, garrisons) author plans; this
  feature only executes them.
