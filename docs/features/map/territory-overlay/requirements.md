# Map Territory Overlay — Requirements

**Epic:** map\
**Created:** 2026-08-10\
**Type:** Stretch goal (sequenced after `map/legacy-retirement`)

## Overview

This feature draws **who controls what** on the map: a coloured territory overlay computed as a Voronoi partition over towns and military bases, with each cell clipped to a maximum influence radius and its borders smoothed, so control reads as organic regions rather than a hard geometric partition. It turns the campaign's control state — which today the player can only infer by reading individual town and base markers one at a time — into a single glance.

It also settles the fate of `OVT_MapThreatGrid`, the threat-heatmap canvas layer that is fully written but shipped disabled (`Configs/Map/MapOverthrow.conf`: `m_bDisableModule 1`, `m_iGridSize 250`). It samples `GetThreatByLocation` over a grid and draws opacity-scaled cells — the same per-frame canvas budget and the same "sample the world, shade it" problem this feature solves. Either it is revived as a sibling overlay on this feature's machinery, or it is deleted; leaving written-but-disabled code in the tree indefinitely is the outcome to avoid.

## Requirements

- Render territory as **Voronoi cells over towns and military bases**, coloured by controlling faction, drawn beneath the location markers so it never obscures them. Each location type has a weight that drives the voronoi, so bases &gt; towns &gt; resistance FOBs in order of the effect they have on territory control
- Existing "Restricted Areas" drawing may conflict with territory now, offer choices to solve this during planning (different colour, or hatch shading etc)
- **Clip each cell to the coastline if possible.** Investigate if the base-game provides any tooling for this
- **Smooth the cell borders** (e.g. Chaikin corner-cutting or an equivalent subdivision) so boundaries read as organic frontiers rather than straight bisectors. Smoothing is applied after clipping. Territory border zones should be rendered with hatching or similar to denote "neutral" territory
- Reuse the **existing faction colour source** — `OVT_MapLocationType.GetIconColor` / `GetFactionColor(m_FactionType)` (`:238-252`) — so territory colour agrees with marker colour instead of introducing a second palette.
- Territory must be **legible without being loud**: low enough alpha that terrain, roads and markers stay readable, and contested/low-stability areas should be visually distinguishable from firmly-held ones.
- **Compute in world space once per map open, project per frame.** `OVT_MapCanvasLayer.Update` calls `Draw()` every frame (`:12-19`), and `WorldToScreen` runs per vertex. The Voronoi solve, clipping and smoothing must happen in `OnMapOpen`; only projection and command emission may run per frame. Smoothing resolution therefore directly costs frame time and must be tunable.
- **Verify how** `PolygonDrawCommand` **handles the polygons produced.** The canvas exposes `PolygonDrawCommand` with an arbitrary `m_Vertices` float array (`OVT_MapCanvasLayer.c:23,61`), but whether it fills non-convex polygons correctly is **unverified**. A Voronoi cell of a point site is convex, and clipping it to a circle and Chaikin-smoothing it both preserve convexity — so the safe design draws one polygon per cell and does **not** merge adjacent same-faction cells into a single region. If merging is wanted, non-convex fill must be proven first.
- Performance must be measured with a **fully-populated campaign** (all towns and bases present), not an early-game one, and the frame cost of opening the map recorded.
- Must be correct in **multiplayer including JIP** — a joining client's overlay must match an established client's, and must update as towns and bases change hands.
- **Decide** `OVT_MapThreatGrid`**'s fate** in this feature: revive it as a toggleable sibling overlay sharing this machinery, or delete it and its config block. Record the reason it was disabled if it can be determined.
- The overlay must be **toggleable** rather than always-on (see `map/map-layers`), and must degrade gracefully if toggled off before it has finished computing.

## Dependencies

- `map/core` — the `OVT_MapCanvasLayer` base (`DrawCircle`/`DrawRectangle` both already build `PolygonDrawCommand`) and the documented canvas-layer lifecycle.
- `map/legacy-retirement` — this is a stretch goal sequenced after the core epic is complete and the legacy map is gone; building it earlier means maintaining it across the retirement.
- `towns/core` — town records supply site positions plus `faction`, `support`, `stability` and `population`, all already replicated to clients and surfaced on the map (`OVT_MapLocationTown.c:39-43`).
- `occupying/core` — base records and `IsOccupyingFaction()` for control state, and `GetThreatByLocation` if the threat grid is revived.
- `map/map-layers` — supplies the toggle; if that feature is not built first, this one must ship its own temporary on/off.

## Out of Scope

- **Changing how control is computed.** The overlay visualises `faction` / `IsOccupyingFaction()` as they already exist; it does not introduce a new territory or influence model into the campaign simulation, and it must not become a second source of truth for who holds what.

- **Weighted / multiplicatively-weighted Voronoi.** Cell extent is distance-based with a uniform influence radius; making strongly-held cities project further than contested villages was considered and deferred as materially harder to compute and smooth.
- **Territory history or animation** — no time-lapse, no transition animation as regions change hands.
- **FOBs and radio towers as territory sites** — sites are towns and bases only.
