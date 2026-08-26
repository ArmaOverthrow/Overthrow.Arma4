# Overthrow Panel — Requirements

**Epic:** gm
**Created:** 2026-08-14

## Overview

A new panel in the Game Master UI, positioned above the settings panel at the bottom-left, showing Overthrow campaign-wide information at a glance. It is the epic's first visible vertical slice and proves the gm-state seam end-to-end.

## Requirements

- New GM UI panel above the settings panel (bottom-left) with the Overthrow logo at the top.
- Shows campaign-wide information:
  - Current threat levels
  - OF resources
  - Next OF resource distribution — amount and countdown
  - Next resistance payout — amount and countdown
  - Resistance funds
  - Any other campaign-wide information that makes sense (decide during planning)
- Shows Overthrow detail for the currently selected HUD icon when that detail cannot be shown on the HUD itself (contract shared with the hud-icons feature).
- Values update live (countdowns tick locally; state changes propagate via gm-state).
- Panel is visible only in GM mode, to authorized GMs.

## Dependencies

- **gm/gm-state must be complete** — all displayed values come from the gm-state seam.
- The selected-icon detail section integrates with gm/hud-icons (built after this feature); the panel should ship with the campaign-wide section working and a seam for selected-icon detail.

## Out of Scope

- Any actions/buttons that change campaign state — Phase 2.
- HUD icon rendering or selection behaviour itself — gm/hud-icons.
- Map-related display — gm/gm-map.
