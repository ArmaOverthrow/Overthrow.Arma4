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

---

## Recorded planning decisions (2026-08-11)

`/plan-feature map/map-layers` was started on 2026-08-11 and **deliberately deferred** in favour of
planning `map/territory-overlay` first, per the epic's recorded build order (feature 7 follows feature 6
so the panel is built against the crowded map it exists to tune). Four decisions were settled before the
deferral and are recorded here so the next planning pass does not re-ask them:

- **Entry point: a vanilla tool-menu entry, not a new keybinding.** `Configs/Map/MapFullscreen.conf` is a
  same-GUID *delta* over vanilla's, so vanilla's `SCR_MapToolMenuUI` is already live on Overthrow's
  fullscreen map. `SCR_MapToolMenuUI.RegisterToolMenuEntry(imageset, icon, sortPriority, isExclusive)` is
  the pattern nine vanilla components use, and `SCR_MapJournalUI` (`:46-57`) is the exact precedent for
  "tool-menu entry → panel docked into `ToolFramesOverlay`" (a frame that already exists in vanilla's
  `UI/layouts/Map/MapMenu.layout:51-57`). **This closes the gamepad requirement for free**: tool-menu
  focus is `MapToolMenuFocus` = `gamepad0:pad_left` (single click), already bound, so no free key or pad
  button has to be found on `MapContext` — which is the single riskiest part of any new Overthrow map
  input (41 live actions, `KC_H` taken three times over, and the repo's conflict checker cannot see
  inline `ActionContext` actions).
- **Default preset: everything on.** The panel is purely additive — a player who never opens it sees
  exactly today's map. The existing per-type `m_fVisibilityZoom` thresholds already keep houses, shops
  and bus stops off the zoomed-out view, so there is no first-open clutter problem to pre-solve.
- **Legend shape: one list, each row is both the legend entry and the toggle** (icon + localized name +
  on/off). No separate read-only colour key. Rows are generated from the configured type list, so the
  panel stays correct as types are added to `Configs/Map/OverthrowMap.conf` with no code change.
- **Persistence mechanism: vanilla `ModuleGameSettings`.** A `ModuleGameSettings` subclass read/written
  via `GetGame().GetGameUserSettings().GetModule("<ClassName>")` +
  `BaseContainerTools.WriteToInstance` / `ReadFromInstance` + `GetGame().UserSettingsChanged()`
  (precedents: `SCR_ManualCameraSettings`, `SCR_EditorPersistentData`, `SCR_HintSettings`). Per-profile,
  console-safe, needs no `.conf` registration and no new file I/O. Overthrow does not use this mechanism
  anywhere yet — this would be its first. Note the consequence: preferences are **per profile, not per
  campaign**, so they follow the player across saves and servers.

**One finding that belongs to whoever builds the overlay toggles:** every `OVT_MapCanvasLayer` resolves
the *same* `CanvasWidget` (`SCR_MapConstants.DRAWING_WIDGET_NAME`, `OVT_MapCanvasLayer.c:89`) and each
one calls `m_Canvas.SetDrawCommands(m_Commands)` from its own `Update` (`:12-19`) — so with two enabled
layers the last to run overwrites the first. It is invisible today only because `OVT_MapThreatGrid` ships
disabled, leaving `OVT_MapRestrictedAreas` as the sole live layer. A second live overlay makes it a real
defect, so "toggle an overlay" cannot mean "SetActive a module" until the layers share one command list.

**The scope question left open:** whether overlay toggles are built against a generic registration API
(any canvas layer registers itself) or hardcoded to the layers that exist. That was not settled, because
the answer depends on what `map/territory-overlay` actually ships.
