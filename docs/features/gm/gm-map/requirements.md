# GM Map — Requirements

**Epic:** gm
**Created:** 2026-08-14

## Overview

The Game Master map (`Configs/Map/MapOverthrow_GM.conf`) gains Overthrow-aware layers and info panels: a performant canvas-based threat grid, icon layers for deployments and base upgrades, GM-only info panel extensions, and a "Move Camera Here" action. This picks up the threat grid explicitly deferred by `map/territory-overlay` (decision D3) and builds on the canvas layer system that feature shipped.

## Requirements

- **Threat grid layer:** rewrite the old threat grid layer on the `OVT_MapCanvasCompositor` / `OVT_MapCanvasLayer` contract from map/territory-overlay so it is performant. Enabled in `MapOverthrow_GM.conf` **only** — not the main player map conf.
- **Deployments icon layer:** shows all current deployments, min zoom 0 so they always show.
- **Base upgrades icon layer(s):** visible when zoomed in to roughly shop level.
- Hovering a deployment/upgrade icon shows an info panel with its type. Use base-game group icons where possible to leverage NATO type indicators etc.
- **Base info panel (GM only):** extended to show resource information, garrison, etc.
- **All existing info panels** (shops, towns, etc.) gain a "Move Camera Here" action for GMs.
- All GM-only layers/panels/actions are gated to authorized GMs; data comes through the gm-state seam.

## Dependencies

- **gm/gm-state must be complete** — supplies threat, deployment, base and garrison data.
- **map/territory-overlay (✅ complete)** — supplies the canvas compositor/layer contract for the threat grid.
- Builds after gm/hud-icons to reuse its icon/detail conventions; reads the occupying epic's deployments and base-upgrade systems.

## Out of Scope

- Any change to the main player map conf (`MapOverthrow.conf`) or player-facing overlays.
- Popup actions that mutate state from map icons (spawn deployment, add upgrade) — Phase 2.
- GM-view (3D) HUD icons — gm/hud-icons.
