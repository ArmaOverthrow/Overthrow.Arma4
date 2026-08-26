# GM State — Requirements

**Epic:** gm
**Created:** 2026-08-14

## Overview

A shared, read-only replication seam that streams Overthrow campaign state to authorized Game Master clients. It is the data spine of the gm epic's Phase 1: the Overthrow panel, HUD icons, waypoint visualization and GM map layers are all pure consumers of this seam, and Phase 2 will later add its write-side (management actions) to the same home.

## Requirements

- Deliver campaign-wide state to GM clients: current threat levels, OF resources, next OF resource distribution (amount + time remaining), next resistance payout (amount + time remaining), and resistance funds.
- Deliver per-entity detail on demand (for a selected/hovered entity): town support/stability/population, base resources and garrison totals, group origin (base/town) and reason for existing (deployment type, base upgrade type, etc.), and player money/level.
- Data is **gated to authorized GM clients only** — player money/levels and OF internals must not be replicated to regular clients.
- **Strictly read-only** — this feature exposes no mutation path; Phase 2 owns writes.
- Countdown values must stay accurate on clients without per-second network traffic (e.g. replicate a timestamp/deadline, tick locally).
- Follows the OVT_OverthrowController specialized-component pattern for any client→server requests (never OVT_PlayerCommsComponent).

## Dependencies

- Reads existing managers: towns (support/stability/population), occupying faction (resources, distribution schedule, deployments, base upgrades, garrisons), economy (resistance funds, payout schedule), player managers (money, level). No new game systems required.
- No sibling dependencies — this is the epic's first feature; all siblings depend on it.

## Out of Scope

- Any UI — panels, icons and map layers are the sibling features.
- Any state mutation (give resources/funds/money/XP) — Phase 2.
- Streaming data to non-GM players or extending the player-facing map/HUD.
