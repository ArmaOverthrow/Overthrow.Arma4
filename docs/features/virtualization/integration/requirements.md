# Integration — Requirements

**Epic:** virtualization
**Created:** 2026-08-03

## Overview

The vertical slice that proves the layer: the deployments framework's three shipped configs (Town Patrol, Light/Heavy Vehicle Patrol) and the occupying faction's radio-tower garrisons migrate onto the virtualization core, retiring two of the three ad-hoc virtualization implementations. This is where "dead group members stay dead" becomes player-visible in the live campaign, and where the core API is exercised against a real consumer before the much larger base-defense migration.

## Requirements

- **Deployments delegate group lifecycle:** deployment spawning modules create and own virtual groups through the core API instead of direct spawning + proximity toggling. The deployment marker entity remains the durable record for deployment-level state (config, faction, invested resources, eliminated flag); virtualization owns group-level state (members, position, waypoints) — one system per concern, no double bookkeeping.
- All three shipped deployment configs run on virtualization, including **per-member dead-stay-dead** across despawn/respawn and save/load (slot-accurate survivor truth per the 2026-08-14 core replan — a group that lost 3 comes back with exactly its survivors, roles preserved; wiped groups never return), and reinforcement rebuying through the same API.
- The **stolen-vehicle guarantee** is preserved: vehicles taken by players survive group despawn — in the 1.8 model this rides the engine's held-member protection (`SCR_AIGroup.HasHeldMember`), which supersedes the old branch's 40 m rule; verify rather than rebuild.
- **Radio-tower garrisons** become virtualization consumers; the ad-hoc spawn/despawn code in `OVT_OccupyingFactionManager` is removed.
- The towns epic's `OVT_PatrolHarassmentStabilityModifier` (the only external consumer of deployment state) keeps working throughout.
- **Existing campaign saves survive:** a save from before this feature loads with patrols and garrisons re-established on the new layer (re-created from config where per-group state didn't previously exist) rather than vanished or duplicated.
- Deployments' lifecycle bugs that this migration would otherwise inherit — BUG-028 (`m_mFactionDeployments` leak, the long-campaign kill switch) and the world-time unit mismatches — are verified fixed (in 1.4.x) or fixed here as a prerequisite task.
- Persistence-tier coverage extends to a migrated deployment round trip (config-name resolution, eliminated-flag ordering, member survival).

## Dependencies

- `virtualization/core` and `virtualization/movement` complete.
- occupying/deployments (the framework being integrated) and occupying/core (tower garrison code being replaced).
- BUG-028 and the deployments world-time bugs — ideally fixed in a 1.4.x patch before this feature starts.

## Out of Scope

- The nine base-upgrade classes and static base defense (feature: `base-defense-migration`).
- QRF spawning in any form (epic-level exclusion).
- New deployment configs or resistance-side deployments — migration at parity, no new content.
- Client-visible deployment/patrol markers.
