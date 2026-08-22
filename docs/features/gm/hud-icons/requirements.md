# HUD Icons — Requirements

**Epic:** gm
**Created:** 2026-08-14

## Overview

Extend the Game Master view's existing HUD icon system (icons above towns and other locations) with Overthrow-specific icons and detail. Clicking an icon surfaces the Overthrow state behind it. This is the epic's riskiest base-game integration and, in Phase 2, these same icons gain popup menu management actions.

## Requirements

- Leverage the existing GM HUD icon layer: add new icons and change existing ones for anything Overthrow-related.
- Clicking a **town** icon shows its support, stability, population and related details.
- **Base** icons show resource information and total garrison.
- Existing **group** icons show which base/town the group came from and its reason for existing (deployment type, base upgrade type, etc.).
- Clicking a **player** shows their money, level, and related details.
- Detail that cannot be rendered on the HUD itself is shown in the Overthrow panel via the shared selected-icon contract (gm/overthrow-panel).
- Visible only in GM mode to authorized GMs; all data comes through the gm-state seam.

## Dependencies

- **gm/gm-state must be complete** — supplies all per-entity detail.
- **gm/overthrow-panel must be complete** — hosts overflow detail for selected icons.
- Can be built **in parallel with gm/waypoint-viz** (both are read-only consumers of gm-state).
- Reads the occupying epic's deployments/base-upgrade systems for group origin/purpose data.

## Out of Scope

- Popup menu actions on icons (give resources, spawn deployments, etc.) — Phase 2.
- Map-screen icons and info panels — gm/gm-map.
- Waypoint rendering — gm/waypoint-viz.
