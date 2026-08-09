# Map Layers & Legend — Requirements

**Epic:** map
**Created:** 2026-08-10
**Type:** Stretch goal (sequenced after `map/legacy-retirement`)

## Overview

This feature gives the player **control over what the map shows**: a legend that names what each icon and colour means, and toggles for each overlay and each location type. By the time the epic reaches this point the map carries eleven-plus marker types, a territory overlay, restricted-area rings, player location and possibly a threat heatmap — all drawn at once. Without a control panel that is an unreadable map; with one it is a map the player tunes to the question they are asking.

There is a natural label source already in place. Every location type declares an `m_sName` in `Configs/Map/OverthrowMap.conf` — "Towns", "Bases", "Radio Towers", "FOBs", "Ports" — but at runtime that field is read **only** by `OVT_MapLocationTypeTitle._WB_GetCustomTitle` (`OVT_MapLocationType.c:7, 384-390`), a Workbench editor-tree label. Players never see those names. This feature is where that grouping becomes visible.

## Requirements

- Provide a **legend** on the fullscreen map identifying each visible location type by icon and name, and each active overlay by its colour meaning (faction territory colours, restriction rings, threat shading).
- Provide **per-location-type visibility toggles**, driven from the configured type list rather than a hardcoded menu — adding a location type in config must add its toggle automatically, with no code change. This is the same config-driven principle the location-type system already follows.
- Provide **per-overlay toggles** for the canvas layers: territory, restricted areas, threat grid (if revived), player location.
- Label toggles from a **player-facing, localized** string. The existing `m_sName` is an editor label and is not localized; either promote it properly (with localization ids added to `Language/localization_Overthrow.st`) or introduce a separate display field. Do not ship raw editor strings to players.
- **Persist the player's choices** across map open/close and across sessions, so a player who turns off houses does not have to turn them off every time they open the map.
- Toggling must be **cheap and immediate** — hiding a type should stop its elements being drawn without tearing down and rebuilding the whole map UI.
- The panel must be **fully operable on gamepad/console**. This is the feature where console usability is most at risk: a filter panel that only works with a mouse is worse than no filter panel, because the console player still gets the crowded map. Navigation must work through the vanilla map input context.
- The panel must not **occlude the map** it is filtering — collapsible, edge-docked, or dismissible.
- Must behave correctly in **multiplayer**, including that toggles are purely client-side presentation and never affect what the server sends or what other players see.
- Consider a **default preset** that is readable out of the box: the map's first-open state should already be sensible rather than requiring the player to configure it.

## Dependencies

- **`map/core`** — the location-type registry and element visibility mechanism the toggles drive.
- **`map/location-types`** — the full type set must exist so the panel is built against the real list.
- **`map/legacy-retirement`** — stretch goal, sequenced after the core epic completes.
- **`map/territory-overlay`** — supplies the main overlay this panel toggles; if territory is built first, this feature absorbs its temporary on/off control.
- **`map/shared-markers`** — if built, player-placed and squad markers should register as a toggleable category here too.
- **Workbench** — the panel `.layout`, and localization ids added to `Language/localization_Overthrow.st` (with exports regenerated).
- **Client settings storage** — for persisting toggle state; identify the existing mechanism rather than inventing one.

## Out of Scope

- **Search by name.** Typing to find a location is a different interaction from filtering categories; deferred unless filtering proves insufficient. It is also poor on console.
- **Per-marker (individual) hiding** — toggles operate on categories, not on single locations.
- **Rearranging or restyling the map itself** beyond adding the panel and legend.
- **Server-side or admin control of what players may see.** Toggles are player presentation preferences only; any notion of the campaign hiding information from a player belongs to the future intel epic, not here.
- **Changing any location type's default visibility zoom values** — `m_fVisibilityZoom` / `m_fShowNameZoom` tuning stays with `map/location-types`.
