# Map Core - Context & Decisions

**Last Updated:** 2026-08-10
**Current Phase:** Bugfix pass complete
**Status:** ✅ Documented (Existing Feature) — ✅ BUG-133…137 fixed and play-tested 2026-08-10

---

## Quick Status

**What's Done:**
- ✅ Feature implemented on the `new-map` branch (2025-05 → 2025-08-02)
- ✅ Branch merged up to date with `main` (2026-08-10, commit `6b0169e4`) — compiles clean, All group 76/76
- ✅ Retrospective documentation created
- ✅ BUG-069 regression check completed by code reading — **all four legacy lifecycle defects avoided structurally**

- ✅ **BUG-133 … BUG-137 fixed and play-tested 2026-08-10** (findings D1–D7; D5 kept as debt). BUG-135 turned out to have been fixed already by `map/fast-travel` (`008293c2`). Zoom sizing, panel dismissal, live marker refresh and the click contract all verified in game

**What's Next:**
- 🔴 The `FindAnyWidget` name sweep across `Scripts/Game/UI/Map/**` — D1/D2 were fixed one at a time, the exhaustive audit is still owed
- 🔴 Swap the close button's literal `"Close"` label for `#OVT-Map_ClosePanel` once the localization exports are regenerated in Workbench (the `.st` master entry exists)
- 🔴 MP/JIP verification; broader gamepad/console verification

**Blockers:**
- None. (The localization export blocker was cleared on 2026-08-10 — all 14 map string ids were regenerated into the six `localization_Overthrow.<lang>.conf` files in Workbench, +28 lines each. Visual verification is unblocked.)

---

## Key Files

| File | Role |
|---|---|
| `Scripts/Game/UI/Map/OVT_OverthrowMapUI.c` | Container; owns locations, elements, selection/pin, info panel |
| `Scripts/Game/UI/Map/Core/OVT_MapLocationType.c` | Config-instantiated type; the extension contract |
| `Scripts/Game/UI/Map/Core/OVT_MapLocationElement.c` | Per-marker widget handler |
| `Scripts/Game/UI/Map/Core/OVT_MapLocationData.c` | Runtime record + four typed key/value maps |
| `Scripts/Game/UI/Map/Core/OVT_MapCanvasLayer.c` | Canvas overlay base (`PolygonDrawCommand` wrapper) |
| `Configs/Map/MapFullscreen.conf` | Same-GUID delta over vanilla; registers modules + UI components |
| `Configs/Map/OverthrowMap.conf` | `OVT_OverthrowMapConfig` — all ten location-type entries |
| `UI/Layouts/Map/Core/OVT_MapLocationElement.layout` | Marker widget (attaches the handler at `:14`) |
| `UI/Layouts/Map/Core/OVT_MapInfoPanel.layout` | Shared info-panel shell with `ContentSlot` |
| `Scripts/Game/UI/Map/Visualization/` | `OVT_MapRestrictedAreas`, `OVT_MapThreatGrid` (disabled), `OVT_MapPlayerLocation` |

---

## The `OVT_MapLocationType` Contract

This is the extension surface every location type builds on. **It is never changed silently** — any
change here is recorded in this table with the feature that made it.

| Member | Kind | Purpose |
|---|---|---|
| `PopulateLocations(array<ref OVT_MapLocationData>)` | virtual | Emit this type's records. Runs once per map open. |
| `PostInit()` | virtual | Per-map-open setup. Note `Init()` runs on **every** open, not once. |
| `CanFastTravel(location, playerID, out reason)` | virtual | Per-type fast-travel policy. **Hot path** — runs per element on every zoom change. |
| `ShouldShowLocation(location, playerID)` | virtual | Per-record visibility. **Hot path**, same as above. |
| `OnLocationSelected(location, element)` | virtual | Selection hook. |
| `OnLocationClicked(location, element)` | virtual | Fires when a click **pins** a location. Invoked by `OVT_OverthrowMapUI.NotifyLocationClicked` from `OnMapSelection` (BUG-137, 2026-08-10). Default body is empty — the container has already built the panel by then. Was unreachable until then. |
| `GetLocationName` / `GetLocationDescription` / `GetDisplayNameForLocation` | virtual | Header text for the info panel. |
| `GetIconName(location)` / `GetIconColor(location)` | virtual | Per-record icon and tint. |
| `OnSetupIconWidget(iconWidget, location, isSmall)` | virtual | Per-record icon customisation. Re-runs on zoom change — keep it cheap. |
| **`m_fRefreshInterval`** | attribute | **Added by BUG-136 (2026-08-10).** Seconds between live re-populations of this type while the map is open. `0` (default) = populate once per map open, exactly as before. |
| **`GetRefreshInterval()`** | getter | **Added by BUG-136.** Read by `OVT_OverthrowMapUI.TickRefresh`. |
| `m_InfoLayout` | attribute | Bespoke info-panel layout. When set, `UpdateInfoPanel` instantiates it and calls `OnSetupLocationInfo`. |
| `OnSetupLocationInfo(widget, location)` | virtual | Populate a bespoke `m_InfoLayout` panel. |
| **`m_SharedInfoLayout`** | attribute | **Added by `map/location-types` Phase 5 (2026-08-10).** Data-driven fallback panel, default `UI/Layouts/Map/Core/OVT_MapInfoRows.layout`. Consulted **only when `m_InfoLayout` is empty**. |
| **`BuildInfoRows(location, rowsContainer)`** | virtual | **Added by `map/location-types` Phase 5.** Fill the shared panel's `Rows` container. Called once per panel open. |
| **`AddInfoRow(rows, label, value)`** | helper | **Added by `map/location-types` Phase 5.** Append a label/value row (empty label ⇒ full-width line). |
| **`AddInfoIconRow(rows, label, value, imageset, icon)`** | helper | **Added by `map/location-types` Phase 5.** As above, with a leading glyph. |
| **`ClearInfoRows(rows)`** | helper | **Added by `map/location-types` Phase 5.** Empty the rows container. |

**The Phase 5 change is purely additive and cannot alter the bespoke path.** `UpdateInfoPanel` now reads:
`m_InfoLayout` set → today's path verbatim, then `return`; else `m_SharedInfoLayout` set → create it,
`FindAnyWidget("Rows")`, `BuildInfoRows`; else return. Town, Base and RadioTower all set `m_InfoLayout`
in `Configs/Map/OverthrowMap.conf` (`:7`, `:20`, `:33`), so the new branch is unreachable for them.

**Layout ↔ code names introduced by that change** (the D1/D2 failure mode — audit these on any edit):

| Name | Layout | Read by |
|---|---|---|
| `Rows` | `UI/Layouts/Map/Core/OVT_MapInfoRows.layout` | `OVT_MapLocationType.UpdateInfoPanel` |
| `RowLabel` / `RowValue` / `RowIcon` | `UI/Layouts/Map/Core/OVT_MapInfoRow.layout` | `OVT_MapInfoRowHandler.HandlerAttached` |

---

## Important Decisions

**1. Extend vanilla's map widgets rather than hand-roll an icon layer.**
`OVT_OverthrowMapUI : SCR_MapUIElementContainer` and `OVT_MapLocationElement : SCR_MapUIElement`. This buys zoom, pan, cursor handling, element containment and console navigation. It also — unintentionally but decisively — eliminated the legacy system's whole lifecycle bug class: because the map module's open/close *is* the engine's own, there is exactly one teardown path, so BUG-069's asymmetric-close, armed-mode, panel-stacking and static-listener-leak defects cannot occur. This is the strongest justification for the rewrite.

**2. Config-driven types, not code-driven registration.**
Types are `ScriptAndConfig`-style objects instantiated from `OverthrowMap.conf`. A new type = subclass + config entry (+ optional layout). This is why `map/location-types` can proceed without touching core.

**3. Two rendering paths, deliberately kept separate.**
Markers are widgets under `m_aUIComponents`; overlays are canvas draw commands under `m_aModules`. They share only the map entity. Anything drawn as a region (the territory stretch goal) belongs on the canvas path, which exposes `PolygonDrawCommand` with an arbitrary vertex array.

**4. Untyped string-keyed payloads on `OVT_MapLocationData`.**
Four maps (string/int/float/bool) let each type carry whatever it needs without subclassing the record. The cost is that keys are unregistered strings — a typo silently returns the default.

**5. Interaction model: hover shows, click pins, click-empty dismisses — plus an explicit close.**
Not the conventional click-to-open. The "close" affordance was evidently designed for a different model and did not work at all (D2/D3); BUG-134 resolved it in favour of the shipped model rather than changing the model. The panel now carries a `CloseButton` (a `WLib_NavigationButtonSmall` bound to the `OverthrowCloseInfoPanel` action, `KC_C` / gamepad `b`) that is **visible only while the selection is pinned**, and it calls `ForceHideLocationInfo` — not `HideLocationInfo`, whose pinned guard would make it a no-op in exactly the state the button exists for. Hiding it on hover panels also retires its keybind, which is the intent.

---

## Gotchas & Learnings

- **`FindAnyWidget` returning null is a silent no-op, and the compiler cannot see it.** Two configured features were dead purely from widget-name mismatches: `IconLayout` (D1 — zoom icon sizing) and `CloseButton` (D2 — panel dismissal). **Both fixed 2026-08-10** (BUG-133 / BUG-134): the icon lookup now uses the name the layout actually defines, `IconContainer`, hoisted to `OVT_MapLocationType.ICON_CONTAINER`; the info panel now really has a `CloseButton`. Any layout↔code name contract needs a runtime pass or an explicit audit; `tools/compile-check.sh` will never catch this class.
- **`FrameSlot.SetSize` cannot size a widget that is not in a `FrameSlot`.** The icon resize target `IconContainer` sits in a `LayoutSlot` (its parent is a `VerticalLayoutWidget`), so even under the right name `FrameSlot.SetSize` would have done nothing. `SizeLayoutWidget.SetWidthOverride`/`SetHeightOverride` is the supported route — BUG-133 was two bugs stacked, and fixing only the name would have looked like a fix and changed nothing.
- **A `SizeLayoutWidget` override sets the DESIRED size; a stretching parent still wins — and the cross axis stretches by default.** BUG-133's third layer, found only by play-testing: with correct, equal width and height overrides, icons still came out flattened (32×24 zoomed in, 32×12 zoomed out). In a `VerticalLayoutWidget` the vertical axis is the main axis, so `SizeMode Auto` honours the height override; the horizontal axis is the cross axis and stretches unless the slot says otherwise. `IconContainer`'s slot authored only `VerticalAlign`, so its width was pinned to the parent's 32px. Fixed by authoring `HorizontalAlign 1` (Center) on the slot **and** asserting it from `SetupIconWidget` via `AlignableSlot.SetHorizontalAlign` — the script-side call exists because a Workbench re-save can re-emit slot defaults, and a layout regression there is invisible to `tools/compile-check.sh`. **Lesson: when a runtime size override appears to apply on one axis only, look at the parent layout's alignment before doubting the override.**
- **`OVT_MapLocationType.Init()` runs on every map open, not once.** It re-caches all seven manager singletons each time. Don't write `PostInit` code that assumes single execution.
- ~~**`UpdateInfoPanel` no-ops entirely when `m_InfoLayout` is empty**~~ — **superseded 2026-08-10 by `map/location-types` Phase 5.** It used to return early when `m_InfoLayout` was empty, which is why seven of ten types showed only the panel header: they were not falling back to a generic renderer, they were contributing nothing to `ContentSlot`. There is now a second branch (`m_SharedInfoLayout` → `BuildInfoRows`) — see the contract table above. The bespoke path is byte-for-byte unchanged. A type that supplies neither layout, or whose `BuildInfoRows` adds no rows, still contributes nothing (the empty container is removed again).
- **`Widget.GetScreenSize` returns PHYSICAL pixels; `WorkspaceWidget.DPIUnscale` converts to the reference units `FrameSlot.SetPos` wants.** Mixing them silently works at DPI scale 1.0 and misplaces everywhere else — `UpdateInfoPanelPosition`'s screen-edge clamp did exactly that for its whole life. If a widget position looks right on your machine and wrong on someone else's, suspect this before suspecting the layout.
- **`CanFastTravel` is on a hot path.** It runs per element inside `UpdateFastTravelIndicator`, which runs for every element on every zoom change. Keep implementations cheap.
- **`m_bShowSpawnPoints` / `m_bShowTasks` are vanilla attributes**, not Overthrow ones (`SCR_MapUIElementContainer.c:23,26`). Setting them `0` suppresses vanilla's own icons — correct config, not dead keys.
- **The type's `m_sName` is a Workbench editor label only** (`OVT_MapLocationTypeTitle._WB_GetCustomTitle`). Never shown to players; not localized.
- **`OnMapSelection` used to unpin the info panel on any click that missed a map element — including clicks landing on the panel itself.** Its `else` branch runs `m_bSelectionPinned = false; m_PinnedElement = null; ForceHideLocationInfo();` whenever `m_HoveredElement` is null, which is what a press on one of the panel's own buttons looks like from the map's point of view. It was masked while every panel button closed the map. `map/fast-travel` Phase 3 added a recruit toggle that must leave the panel open, and on a controller the toggle's only input path is the map cursor plus `MapSelect` — so the hazard became load-bearing. **Change made (map/fast-travel Phase 3, 2026-08-10):** `OVT_OverthrowMapUI.IsSelectionOnInfoPanel(selectionPos)` converts the selected world position back to screen space via `m_MapEntity.WorldToScreen` and compares it against the panel's `GetScreenPos`/`GetScreenSize` rect; `OnMapSelection` returns early when it is inside. **Unverified at runtime:** whether the button widget consumes the click before the map's selection handler sees it at all. If it does, the guard is inert — it was applied regardless because it is cheap.
- ~~**`OnLocationClicked` is currently unreachable**~~ — **fixed 2026-08-10 (BUG-137).** `HandleSelection()` and `GetClickRadius()` were dead code and are deleted; the container now calls the virtual from `OnMapSelection` when a click pins a location, and plays the element's click sound (`PlayClickSound`) at the same point. There is deliberately **no click-to-deselect**: hover already shows the panel, so unpinning under a stationary cursor would leave it on screen anyway. Explicit dismissal is the panel's close button.
- **Markers refresh on a per-type opt-in timer, not on events** (BUG-136, 2026-08-10). `OVT_MapLocationType.m_fRefreshInterval` (seconds, `0` = never) makes `OVT_OverthrowMapUI.TickRefresh` re-run that type's `PopulateLocations` and reconcile its elements — matched records are re-pointed via `OVT_MapLocationElement.SetLocationData` (which is what finally gives `OnLocationDataChanged()` a caller), gone ones are destroyed, new ones get a marker. Enabled in `Configs/Map/OverthrowMap.conf` for Town/Base/RadioTower/FOB/Camp at 5s and Vehicle at 2s; every other type stays at 0 and costs nothing. **Reconciliation destroys elements while the map is open** — `DestroyLocationElement` must clear `m_HoveredElement`, `m_PinnedElement`, `m_SelectedElement` and the base class's static `s_SelectedElement`, or it re-creates BUG-135 on a timer.

---

*This context file was created retrospectively by analyzing existing code.*
