# Map Core - Implementation Plan (Retrospective)

**Status:** Implemented (Documented Retrospectively) — shipped but never play-tested or MP-verified
**Epic:** map (feature 1 of 7)
**Originally Implemented:** 2025-05 → 2025-08-02, on the `new-map` branch
**Documented:** 2026-08-10
**Last Updated:** 2026-08-10

---

## Executive Summary

`map/core` is the infrastructure half of Overthrow's map rewrite: a **config-driven marker system built on vanilla's own map widgets**. `OVT_OverthrowMapUI` extends `SCR_MapUIElementContainer` and is instantiated by the engine from `Configs/Map/MapFullscreen.conf`; it reads an `OVT_OverthrowMapConfig` (`Configs/Map/OverthrowMap.conf`) holding an array of `OVT_MapLocationType` objects, asks each to populate `OVT_MapLocationData` records, and spawns one `OVT_MapLocationElement` widget per record. Adding a location type is a subclass plus a config entry — no core code changes.

The rewrite's central bet is **leverage vanilla instead of reimplementing it**: zoom, pan, cursor handling, element containers and console navigation all come from `SCR_MapUIElement`/`SCR_MapUIElementContainer`. That bet also fixed the legacy system's worst structural defect for free — see "BUG-069 verdict" below.

**Note:** This is a retrospective plan created by reading the code on the merged `new-map` branch. Every claim below is cited to `file:line`. Nothing here has been observed at runtime — the branch has not been play-tested since 2025-08-02 and has never been verified in multiplayer. Findings marked **(unverified)** are code-reading inferences that need a play-test to confirm.

---

## Goals

### Primary Goals
- Provide one extension point (`OVT_MapLocationType`) that turns replicated campaign state into interactive map markers, declared in config rather than code.
- Inherit vanilla's map interaction model — including gamepad/console navigation — rather than hand-rolling it.
- Replace the legacy `OVT_MapIcons` parallel-array icon layer and its per-frame retry subsystem.

### Success Criteria
- [x] Config-driven location type registry (`OVT_OverthrowMapConfig.m_aLocationTypes`)
- [x] Element lifecycle bound to vanilla's map open/close, with symmetric listener add/remove
- [x] Single info panel enforced — no panel stacking
- [x] Zoom-gated element visibility and name display
- [x] Faction-aware icon colouring from campaign config
- [ ] Zoom-gated **icon sizing** — configured but inert (see D1)
- [ ] A working way to dismiss the info panel other than clicking empty map (see D2/D3)
- [ ] Markers that refresh while the map is open (see D6)
- [ ] MP/JIP verification
- [ ] Gamepad/console verification

---

## Current Architecture

### Key Components

| File | Role |
|---|---|
| `Scripts/Game/UI/Map/OVT_OverthrowMapUI.c` | The container (`SCR_MapUIElementContainer`). Owns the location list, element creation, selection/pin state and the info panel. Also declares `OVT_OverthrowMapConfig` (`:1-6`). |
| `Scripts/Game/UI/Map/Core/OVT_MapLocationType.c` | Abstract, config-instantiated type. Attributes for icon/layout/zoom/fast-travel; the virtual contract subclasses implement. |
| `Scripts/Game/UI/Map/Core/OVT_MapLocationElement.c` | Per-marker widget handler (`SCR_MapUIElement`). Hover/selection state, icon/name/distance rendering, zoom response. |
| `Scripts/Game/UI/Map/Core/OVT_MapLocationData.c` | Plain `Managed` runtime record: position, name, type name, ids, and four typed key→value maps. |
| `Scripts/Game/UI/Map/Core/OVT_MapCanvasLayer.c` | Base for canvas overlays (`SCR_MapModuleBase`). Wraps `PolygonDrawCommand`/`ImageDrawCommand` as `DrawCircle`/`DrawRectangle`/`DrawImage`. |
| `Configs/Map/MapFullscreen.conf` | **Same-GUID delta over vanilla's** map config. Registers the canvas modules and UI components. |
| `Configs/Map/OverthrowMap.conf` | The `OVT_OverthrowMapConfig` holding all ten location-type entries. |
| `UI/Layouts/Map/Core/OVT_MapLocationElement.layout` | Marker widget; attaches the `OVT_MapLocationElement` handler (`:14`). |
| `UI/Layouts/Map/Core/OVT_MapInfoPanel.layout` | Shared info panel shell with a `ContentSlot` the type fills. |
| `UI/Imagesets/overthrow_mapicons.imageset` | Icon atlas (`UI/Textures/Map/overthrow_mapicons_atlas.edds`). |
| `Scripts/Game/UI/Map/Visualization/` | Canvas layers: `OVT_MapRestrictedAreas`, `OVT_MapThreatGrid` (shipped disabled), `OVT_MapPlayerLocation`. |

### Two independent rendering paths

This is the single most important architectural fact about the map, and it is easy to miss:

1. **Markers are widgets.** `OVT_OverthrowMapUI` (a `m_aUIComponents` entry) creates one widget per location into `m_wIconsContainer` and registers it in vanilla's `m_mIcons` map (`OVT_OverthrowMapUI.c:183-197`).
2. **Overlays are canvas draw commands.** `OVT_MapCanvasLayer` subclasses (`m_aModules` entries) push `CanvasWidgetCommand` arrays into a single `CanvasWidget` every frame (`OVT_MapCanvasLayer.c:12-19`).

They share nothing but the map entity. The stretch-goal territory overlay lives on path 2; every location type lives on path 1.

### Data flow

```
Engine opens map
  └─ OVT_OverthrowMapUI.OnMapOpen                         (:71)
       ├─ super.OnMapOpen  → vanilla builds its own icons
       ├─ InitializeLocationTypes → type.Init(this)        (:142-152)
       │     └─ caches 7 manager singletons via OVT_Global (LocationType.c:67-81)
       ├─ PopulateAllLocations → type.PopulateLocations()  (:155-165)
       │     └─ each type appends OVT_MapLocationData      (client-side, from replicated state)
       ├─ CreateLocationElements                           (:168-204)
       │     ├─ workspace.CreateWidgets(m_LocationElementLayout, m_wIconsContainer)
       │     ├─ widget.FindHandler(OVT_MapLocationElement)
       │     ├─ element.Init(location, type, this)         → UpdateDisplay()
       │     └─ m_mIcons.Set(widget, element)
       └─ subscribes OnMapZoom + OnMapSelection            (:88-89)
```

### The `OVT_MapLocationType` contract

Everything a subclass may override, and when the container calls it:

| Member | Called from | Contract |
|---|---|---|
| `Init(mapUI)` | `OnMapOpen` → `InitializeLocationTypes` (`:150`) | **Every map open.** Caches 7 manager singletons, then calls `PostInit()`. Do not assume it runs once. |
| `PostInit()` | `Init` (`LocationType.c:80`) | Subclass setup hook. |
| `PopulateLocations(array<ref OVT_MapLocationData>)` | `OnMapOpen` → `PopulateAllLocations` (`:163`) | **The main override.** Append one record per location. Runs client-side against replicated state; must tolerate managers being null or partially replicated. |
| `CanFastTravel(location, playerID, out reason)` | Element `UpdateFastTravelIndicator` (`Element.c:302`) **and** container `SetupFastTravelButton` (`:485`) | Called per element on every `UpdateDisplay`, so it must be **cheap**. Default returns `m_bCanFastTravel`. |
| `OnLocationClicked(location, element)` | Element `HandleSelection` (`Element.c:134`) — **currently unreachable, see D7** | Default shows the info panel (`LocationType.c:119-120`). |
| `OnLocationSelected(location, element)` | Element `Select(true)` (`Element.c:227`) | Fires on selection, including hover-driven selection. |
| `UpdateInfoPanel(location, infoPanel)` | Container `ShowLocationInfo` (`:265`), passed the `ContentSlot` widget | Clears the slot, instantiates `m_InfoLayout`, then calls `OnSetupLocationInfo`. **No-ops entirely if `m_InfoLayout` is empty** (`LocationType.c:126-127`) — this is why the 7 types without a bespoke layout show only the panel's generic header. |
| `OnSetupLocationInfo(widget, location)` | `UpdateInfoPanel` (`:144`) | Where subclasses fill their panel. |
| `GetLocationName/Description(location)` | Container `SetupLocationInfoBase` (`:366`,`:379`), element `UpdateLocationName` (`Element.c:384`) | Name defaults to `location.m_sName`; description defaults to the type's `m_sDisplayName`. |
| `ShouldShowLocation(location, playerID)` | Element `SetVisible` (`Element.c:507`) | Per-element visibility filter. Default returns `location.m_bVisible`. |
| `GetIconName(location)` / `GetIconColor(location)` / `OnSetupIconWidget(...)` | `SetupIconWidget` (`LocationType.c:297-359`) | Per-location icon overrides. `GetIconColor` resolves faction colour via `GetFactionColor` → `OVT_OverthrowConfigComponent` (`:256-282`). |

**Client/server:** the entire map layer is **client-side presentation**. Types read manager singletons through `OVT_Global` and never write campaign state. The one action seam is the fast-travel button, which belongs to `map/fast-travel`.

### The `OVT_MapLocationData` payload model

A `Managed` record (`OVT_MapLocationData.c`) with fixed fields (`m_vPosition`, `m_sName`, `m_sTypeName`, `m_EntityID`, `m_RplID`, `m_iID`, `m_bVisible`, `m_bCanFastTravel`, `m_ShopType`, `m_pEntity`) plus **four typed maps** — string/int/float/bool — accessed through `GetDataX(key, default)` / `SetDataX(key, value)` (`:52-109`). Types stuff arbitrary per-location state in at populate time and read it back when filling the info panel.

Two consequences worth knowing:
- **Keys are untyped strings with no registry.** A typo in a key silently yields the default. `map/location-types` should document each type's key set.
- **`m_sTypeName` is the link back to the type**, resolved by `GetLocationTypeByName` comparing `ClassName()` (`:208-220`).

`GetEntity()` (`:112-129`) resolves `m_EntityID` first, then falls back to `m_RplID` via `Replication.FindItem` — the correct networked path.

### Element lifecycle

- **Creation** — one widget per record, `SetSizeToContent` + centre alignment (`:200-201`).
- **Positioning** — vanilla's `UpdateIcons`/`GetPos`; the element returns its world position from location data (`Element.c:67-72`).
- **Zoom gating** — `SetVisible` ANDs three conditions: caller intent, `currentZoom >= m_fVisibilityZoom`, and `ShouldShowLocation` (`Element.c:485-511`). Names/distance gate separately on `m_fShowNameZoom` **and** "not selected" (`Element.c:379`, `:403`).
- **Hover** — `OnMouseEnter` unpins any pin, sets hovered, selects, and **shows the info panel** (`Element.c:139-162`); `OnMouseLeave` clears hover and hides it (`:165-183`).
- **Pin** — a map click routes to `OnMapSelection` (`:44-69`): clicking a hovered element pins it; clicking empty space unpins and force-hides.
- **Teardown** — `OnMapClose` (`:92-106`) removes both listeners, force-hides the panel, and nulls `m_aLocations`/`m_SelectedElement`. Vanilla's `super.OnMapClose` destroys the icon widgets and clears `m_mIcons` (`SCR_MapUIElementContainer.c:205`).

**Interaction model in one line: hover shows, click pins, click-empty dismisses.**

---

## BUG-069 verdict — the legacy lifecycle defects were NOT reproduced

The legacy `OVT_MapContext` had four lifecycle defects (BUG-069, now closed). The rewrite avoids all four **structurally**, because it is a map *module* driven by the engine's own open/close rather than a UI context driven by an input handler:

| BUG-069 defect | Status in the rewrite | Evidence |
|---|---|---|
| 1. Two unequal close paths; engine-side close leaks the panel | **Avoided.** There is one close path — `OnMapClose` — and it calls `ForceHideLocationInfo()` unconditionally. | `:92-106` |
| 2. Bus mode stays armed after close; next click charges a fare | **Not applicable.** No persistent mode flags. Travel is a button on the panel, not a map mode. | `:449-539` |
| 3. Panels stack to N orphans | **Avoided.** `ShowLocationInfo` removes any existing panel before creating one — "Always force hide any existing panel first to ensure only one exists". | `:241-246` |
| 4. Static `SCR_MapEntity.GetOnMapClose()` listener accumulates per respawn | **Avoided.** No static subscription. Listeners are added in `OnMapOpen` and removed in `OnMapClose`, symmetrically. | `:88-89`, `:97-98` |

This is the strongest argument that the rewrite is the right direction, and it should be stated explicitly when the branch is reviewed.

---

## Current State

### What's working (by construction)
- Config-driven type registration and per-type icon/zoom/layout configuration.
- Symmetric, leak-free map open/close lifecycle (see above).
- Single-panel invariant.
- Zoom-gated element and name visibility.
- Faction-aware icon colouring sourced from `OVT_OverthrowConfigComponent`.
- `m_bShowSpawnPoints 0` / `m_bShowTasks 0` in `MapFullscreen.conf` are **vanilla** `SCR_MapUIElementContainer` attributes (`SCR_MapUIElementContainer.c:23,26`), deliberately suppressing vanilla's own spawn-point and task icons so Overthrow's markers are the only ones drawn. These are correct config, not stale keys.

### Defects found (code reading — all **unverified** at runtime)

- **D1 — Zoom-based icon sizing is inert (widget name mismatch).** `SetupIconWidget` looks up `"IconLayout"` (`LocationType.c:321`) to apply `m_iIconSizeSmall`/`m_iIconSizeLarge`. `OVT_MapLocationElement.layout` contains no `IconLayout`; its `SizeLayoutWidgetClass` is named **`IconContainer`** (layout `:53-54`) — which the element itself caches under that name (`Element.c:83`). So `ShouldUseSmallIcon()` is computed, threaded through, and discarded: **icons never resize with zoom**, and the two size attributes on all ten types do nothing. Likely a rename that was not propagated.
- **D2 — The info panel has no working close button.** `ShowLocationInfo` wires a `CloseButton` (`:272-278`), but `OVT_MapInfoPanel.layout` defines no widget of that name. The lookup returns null and the wiring is dead. The only dismissal is clicking empty map space.
- **D3 — Even with a close button it would no-op while pinned.** `HideLocationInfo` early-returns when `m_bSelectionPinned` (`:291-297`), and any click-opened panel is pinned by `OnMapSelection` (`:55`). D2 and D3 compound: there is no close affordance, and the wiring that was meant to provide one would not have worked for the pinned case anyway.
- **D4 — `m_HoveredElement` is never cleared on map close.** It is a strong `ref` (`:37`); `OnMapClose` clears `m_aLocations` and `m_SelectedElement` but not it, and `ForceHideLocationInfo` clears only `m_PinnedElement` (`:315-316`). Closing the map while the cursor rests on a marker (no `OnMouseLeave`) keeps a dead element alive across the close; on the next open, `OnMapSelection`'s `if (m_HoveredElement)` branch (`:47`) treats a click on **empty space** as a click on the stale element and re-pins it.
- **D5 — Dead local in `UpdateIcons`.** It picks `targetElement` = pinned-else-selected (`:116-121`) then calls `UpdateInfoPanelPosition()`, which ignores the argumentless choice and positions from `m_SelectedElement` only (`:551`). Harmless today because pinning also selects, but the intent and the behaviour disagree.
- **D6 — Markers never refresh while the map is open.** Population happens once in `OnMapOpen`; `OnLocationDataChanged()` (`Element.c:443`) is the refresh hook and **has no callers**. Campaign state changing while the map is open is not reflected until it is reopened.

### Dead code
- `OVT_MapLocationElement.HandleSelection()` (`Element.c:105-136`) — **no callers**. It is the only consumer of `m_sSoundClick` (`:38`) and the only caller of `OnLocationClicked`, meaning **D7: the `OnLocationClicked` virtual is currently unreachable** — the panel appears via hover instead.
- `OVT_MapLocationElement.GetClickRadius()` (`:468-482`) — no callers.
- `OVT_MapLocationElement.OnLocationDataChanged()` (`:443-446`) — no callers (see D6).

### Technical debt / performance notes
- `GetLocationTypeByName` is a linear scan comparing `ClassName()` (`:208-220`), called once per location during creation and **three times** per info-panel show (`:260`, `:363`, `:376`, `:461`). A `map<string, OVT_MapLocationType>` built once in `InitializeLocationTypes` would remove it.
- `GetCurrentPlayerID()` — a player-manager lookup plus persistent-ID resolution — runs **per element** inside `SetVisible` (`Element.c:506`) and `UpdateFastTravelIndicator` (`Element.c:300`), both of which run for every element on every zoom change.
- `CanFastTravel` is therefore also evaluated per element per zoom change; any type with an expensive implementation pays it repeatedly.
- Info-panel positioning offsets are hardcoded pixels (`x += 13; y -= 31`, `:562-563`) and clamped to screen only on the right and top edges (`:572-575`).

---

## Testing

### Current coverage
**None specific to the map.** The Fast (38) and All (76) autotest groups cover logic, init, campaign and persistence; no suite touches map UI. This is consistent with the project's stated position that UI is not automatable here.

### Testing gaps
- The whole feature is unverified at runtime since 2025-08-02.
- D1–D6 are all code-reading inferences and need a play-test to confirm or dismiss.
- MP/JIP: markers populate from replicated state at map open; a JIP client opening the map before state has replicated is exactly the untested case.
- Gamepad/console: the interaction model depends on vanilla synthesising hover from the map cursor. `OnMouseEnter` is commented "works for both mouse and controller" (`Element.c:138`) but this is **unverified**, and with D2 the only dismissal is clicking empty map — an awkward gesture on a controller.

---

## Dependencies

### External
- Vanilla `SCR_MapUIElementContainer`, `SCR_MapUIElement`, `SCR_MapModuleBase`, `SCR_MapEntity`, `SCR_MapGadgetComponent`, `PolygonDrawCommand`/`ImageDrawCommand`.

### Internal
- `OVT_Global` manager accessors: towns, real estate, resistance, occupying faction, economy, vehicles, players (`LocationType.c:72-78`).
- `OVT_OverthrowConfigComponent` for faction colours and faction data.
- `OVT_FastTravelService` — referenced from the container (`:538`, `:545`) but owned by `map/fast-travel`.

---

## Notes

**Discovered information**
- The config key `m_sName` on each location type is read **only** by `OVT_MapLocationTypeTitle._WB_GetCustomTitle` (`LocationType.c:383-400`), a Workbench editor-tree label. Players never see it. It is the natural label source for the `map/map-layers` stretch feature.
- `MapFullscreen.conf` currently registers **both** map systems: `OVT_MapIcons` with `Enabled 0` / `m_bDisableComponent 1`, and `OVT_OverthrowMapUI` live. The new system is already the active one.
- `OVT_MapThreatGrid` is fully implemented (samples `GetThreatByLocation` into opacity-scaled cells) but shipped with `m_bDisableModule 1`. Its fate is assigned to `map/territory-overlay`.

**Retrospective assessment**
- *What works well:* choosing to extend vanilla's widget system paid for itself — it eliminated the legacy system's entire class of lifecycle bugs (BUG-069) without anyone having to fix them, and it is the only credible route to console support.
- *What could be improved:* the widget-name mismatches (D1, D2) are the signature of a UI built without a runtime pass — both are invisible to the compiler, and both disable a configured feature silently. A layout↔code name audit belongs in this feature's verification.
- *Lesson:* in Enfusion, `FindAnyWidget` returning null is a silent no-op. Code that looks up widget names needs either a runtime assert or a documented layout contract; `tools/compile-check.sh` cannot catch any of it.

---

*This retrospective plan was created by analyzing existing code. Use `/start-feature map/core` to begin verification and fixes.*
