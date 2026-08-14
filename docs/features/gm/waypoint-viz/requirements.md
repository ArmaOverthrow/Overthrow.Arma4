# Waypoint Viz — Requirements

**Epic:** gm
**Created:** 2026-08-14

## Overview

Read-only visualization of Overthrow-generated AI waypoints in the Game Master view, so a GM can see where a group was told to go. This is our own implementation — we do not use the base game's E_ waypoint set (it draws editable waypoints in GM; we had issues with it, and editability is unnecessary anyway).

## Requirements

- Overthrow-generated waypoints for AI groups are visualized in GM view (visible to authorized GMs only).
- Visualization is **read-only** — Overthrow waypoints are not editable through this feature. A GM who wants to redirect a group assigns a new base-game waypoint, which already grants more control than we could offer.
- Waypoint display must not interfere with the base game's own waypoint assignment/editing flow in GM.
- Works with virtualized/dormant groups to whatever extent their waypoint data is available (coordinate with the virtualization epic's lifecycle).

## Dependencies

- **gm/gm-state must be complete** — supplies group/waypoint data to GM clients.
- Can be built **in parallel with gm/hud-icons** (no dependency between them).
- Interacts with the virtualization epic's group lifecycle (dormant groups may hold waypoint intent without live AI agents).

## Out of Scope

- Editing, reordering or deleting Overthrow waypoints via this visualization.
- Adopting or wrapping the E_ waypoint set.
- Map-screen waypoint rendering — GM-view (3D world) only unless planning finds map display trivial via gm/gm-map's layers.
