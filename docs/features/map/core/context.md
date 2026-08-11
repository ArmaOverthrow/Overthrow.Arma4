# Map Core - Context & Decisions

**Last Updated:** 2026-08-11
**Current Phase:** Bugfix pass complete (canvas-layer contract extended by `map/territory-overlay` 2026-08-11; location-type contract extended by `map/map-layers` 2026-08-11)
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
| `m_fVisibilityZoom` / `GetVisibilityZoom()` | attribute | Zoom threshold below which this type's markers hide (and above which they draw the large icon). **Since BUG-138 (2026-08-11) this is the *fallback*, not the last word** — a record may override it per-instance (see the data key below). |
| **`OVT_MapDataKeys.VISIBILITY_ZOOM`** (`"visibilityZoom"`) | per-record data key | **Added by BUG-138 (2026-08-11).** Optional float written by `PopulateLocations` into `OVT_MapLocationData`. When present and `>= 0` it replaces `GetVisibilityZoom()` for that record alone, in **both** `OVT_MapLocationElement.ShouldUseSmallIcon` and `SetVisible`, via `GetEffectiveVisibilityZoom()`. Absent ⇒ type value, so every existing record is unchanged. **The sentinel is negative, not `0`** — `0` is a real threshold meaning "always visible" (Town/Base/RadioTower ship it). Only writer today is `OVT_MapLocationFOB`, which writes `0` for a priority FOB; that is what finally makes `isPriority` mean "enhanced map visibility". **Hot path** — the reader must stay a map lookup and a compare. |
| `m_InfoLayout` | attribute | Bespoke info-panel layout. When set, `UpdateInfoPanel` instantiates it and calls `OnSetupLocationInfo`. |
| `OnSetupLocationInfo(widget, location)` | virtual | Populate a bespoke `m_InfoLayout` panel. |
| **`m_SharedInfoLayout`** | attribute | **Added by `map/location-types` Phase 5 (2026-08-10).** Data-driven fallback panel, default `UI/Layouts/Map/Core/OVT_MapInfoRows.layout`. Consulted **only when `m_InfoLayout` is empty**. |
| **`BuildInfoRows(location, rowsContainer)`** | virtual | **Added by `map/location-types` Phase 5.** Fill the shared panel's `Rows` container. Called once per panel open. |
| **`AddInfoRow(rows, label, value)`** | helper | **Added by `map/location-types` Phase 5.** Append a label/value row (empty label ⇒ full-width line). |
| **`AddInfoIconRow(rows, label, value, imageset, icon)`** | helper | **Added by `map/location-types` Phase 5.** As above, with a leading glyph. |
| **`ClearInfoRows(rows)`** | helper | **Added by `map/location-types` Phase 5.** Empty the rows container. |
| **`CanRespawn(location, playerID, out reason)`** | virtual | **Added by `map/respawn` (2026-08-10).** Per-record respawn eligibility, defaulting to refuse (`reason = "#OVT-Respawn_NotEligible"`, returns false). Overridden on Base/FOB/Camp/House. **Hot path** — reached from `ShouldShowLocation` on every zoom change whenever `m_bRespawnOnly` is set. **Advisory only, like `CanFastTravel`:** it decides what this client *draws*; the server re-derives the eligible set from its own managers in `OVT_RespawnService.CollectEligiblePositions` and that is what decides where anybody actually spawns. |
| **`m_bRespawnOnly`** | attribute | **Added by `map/respawn` (2026-08-10).** `1` = this instance draws only records that pass `CanRespawn`. Set only in `Configs/Map/OverthrowMapRespawn.conf` (all four of its entries); `0` (default) leaves the living map byte-for-byte unchanged — `ShouldShowLocation` returns immediately after one boolean compare. |
| **`m_sCategoryName`** | attribute | **Added by `map/map-layers` (2026-08-11).** The **plural, localized, player-facing** category name shown on the map layer-filter row. Deliberately a **third** name field: `m_sName` stays the Workbench editor-tree label (`OVT_MapLocationTypeTitle._WB_GetCustomTitle`) and `m_sDisplayName` stays the **singular** type line on the info panel. Both are left doing exactly their existing jobs, untouched. Empty ⇒ falls back to `GetDisplayName()`, then `ClassName()`, with a one-time WARNING. Set on all 14 entries in `Configs/Map/OverthrowMap.conf`, so the fallback is unreachable for every shipped type. `Configs/Map/OverthrowMapRespawn.conf`'s four entries deliberately do **not** set it — that config carries no `OVT_MapLayersUI`, so `GetCategoryName()` is never called there. |
| **`GetCategoryName()`** | getter | **Added by `map/map-layers`.** Read only by `OVT_MapLayersUI` when building rows. **Not a hot path** — once per row per panel open. |
| **`m_bPlayerVisible` / `SetPlayerVisible(bool)` / `IsPlayerVisible()`** | runtime member + setter/getter | **Added by `map/map-layers`.** A **client-side presentation preference**, deliberately *not* an attribute — it is never authored in config, and is always applied from the persisted profile store at map open. Read as the **first** gate in `OVT_MapLocationElement.SetVisible`, immediately after the `!m_LocationType` guard, which **early-returns** — so a hidden type skips the `GetEffectiveVisibilityZoom()` lookup and the `ShouldShowLocation` manager reads entirely, and a hidden type costs *less* than a shown one. Default `true`, and it is **not** reset in `Init()` (which runs on every map open). 🔴 **This is not campaign visibility.** What the campaign chooses to *reveal* to a player belongs to the future intel epic and **must never share this field**: this one says "the player asked not to see it", not "the player does not know about it". |

**The Phase 5 change is purely additive and cannot alter the bespoke path.** `UpdateInfoPanel` now reads:
`m_InfoLayout` set → today's path verbatim, then `return`; else `m_SharedInfoLayout` set → create it,
`FindAnyWidget("Rows")`, `BuildInfoRows`; else return. Town, Base and RadioTower all set `m_InfoLayout`
in `Configs/Map/OverthrowMap.conf` (`:7`, `:20`, `:33`), so the new branch is unreachable for them.

**Layout ↔ code names introduced by that change** (the D1/D2 failure mode — audit these on any edit):

| Name | Layout | Read by |
|---|---|---|
| `Rows` | `UI/Layouts/Map/Core/OVT_MapInfoRows.layout` | `OVT_MapLocationType.UpdateInfoPanel` |
| `RowLabel` / `RowValue` / `RowIcon` | `UI/Layouts/Map/Core/OVT_MapInfoRow.layout` | `OVT_MapInfoRowHandler.HandlerAttached` |

**Layout ↔ code names introduced by `map/respawn` (2026-08-10).** Same failure mode, same discipline — each
name below was grepped in both the layout that defines it and the code that reads it.

| Name | Layout | Read by |
|---|---|---|
| `RespawnButton` | `UI/Layouts/Map/Core/OVT_MapInfoPanelRespawn.layout` | `OVT_RespawnMapUI.SetupTravelButton` (`FindAnyWidget` on `m_wInfoPanel`, then `FindHandler(SCR_InputButtonComponent)`) |
| `RespawnHomeButton` | `UI/Layouts/Respawn/OVT_RespawnScreen.layout` | `OVT_RespawnContext.WireRespawnHomeButton` (`FindAnyWidget` on `m_wRoot`, then `FindHandler(SCR_InputButtonComponent)`) |
| `StatusText` | `UI/Layouts/Respawn/OVT_RespawnScreen.layout` | `OVT_RespawnContext.SetStatusText` — the persistent refusal line; the hint `OVT_RespawnRequestComponent` raises is transient |
| `MapWidget` | vanilla `UI/layouts/Map/Map.layout`, inherited into `OVT_RespawnScreen.layout` | `SCR_MapEntity.OpenMap`, via `SCR_MapConstants.MAP_WIDGET_NAME`, as `config.RootWidgetRef.FindAnyWidget(...)`. **This is why the respawn screen must embed vanilla's `Map.layout` rather than author its own frame** — the name is not optional and the map entity resolves it out of whatever root widget it is handed. |
| `MapFrame` | `UI/Layouts/Respawn/OVT_RespawnScreen.layout` (the name given to the *inherited* `Map.layout` instance; vanilla's own root frame carries the same name) | **Nothing in Overthrow reads it.** `SCR_MapConstants.MAP_FRAME_NAME` has three vanilla consumers — `SCR_MapDrawingUI`, `SCR_MapMarkerBase` and `SCR_MapMarkerEntity` — and **none of those modules is carried by `Configs/Map/MapRespawn.conf`**, so on the respawn screen the name is defined and never looked up. Keep it correct anyway: adding drawing or markers to that config would make it load-bearing overnight. |

Pre-existing names the respawn info panel must keep, because they are read by the **inherited**
`OVT_OverthrowMapUI` code and not by anything respawn-specific: `LocationName`, `LocationType`,
`ContentSlot`, `CloseButton`. Names deliberately **absent** from that layout are a feature, not an
omission. `FastTravelButton` / `FastTravelReason` / `BringRecruitsButton` are never even *looked up* on this
screen — `OVT_RespawnMapUI.SetupTravelButton` overrides the only place the base class builds travel
affordances and deliberately does not call `super`. `Distance` **is** looked up (`SetupLocationInfoBase`
does it unconditionally, ignoring `m_bShowDistance`, and writes the literal `"Unknown"` on `-1`), and its
absence from the layout is what suppresses the row — a null `FindAnyWidget` skipping the whole block. Both
halves of "no travel affordances and no distance row on the respawn screen" are therefore enforced by the
layout, not by a runtime branch.

**Layout ↔ code names introduced by `map/map-layers` (2026-08-11).** Same failure mode, same discipline —
every row below was verified against both the `.layout` that defines the name and the `.c` that reads it,
not transcribed from the plan. Every lookup in the reading code is null-guarded and ERROR-logs the name it
could not find. ⚠️ **None of these has ever been resolved at runtime** — the feature is code-complete and
entirely unobserved, and `FindAnyWidget` returning null is invisible to `tools/compile-check.sh`.

| Name | Layout | Read by |
|---|---|---|
| `ToolFramesOverlay` | vanilla `UI/layouts/Map/MapMenu.layout` (**read-only — Overthrow does not override it**, K9) | `OVT_MapLayersUI.ResolveDockParent` — `m_RootWidget.FindAnyWidget(...)`; the panel is **created into** it via `WorkspaceWidget.CreateWidgets(layout, parent)` |
| `ToolMenuContainer` | vanilla `UI/layouts/Map/MapMenu.layout` | `OVT_MapLayersUI.ResolveDockParent` — first fallback when `ToolFramesOverlay` is absent (`FastTravelMapMenu.layout` has no such overlay and parents its frames here). Second fallback is `m_RootWidget` itself, and an ERROR names whichever was settled on |
| `LayersPanel` | `UI/Layouts/Map/Core/OVT_MapLayersPanel.layout` (the root widget's own `Name`) | **Nothing.** Held as `m_wPanel` from `CreateWidgets`; never looked up by name |
| `PanelTitle` | `OVT_MapLayersPanel.layout` | `OVT_MapLayersUI.BuildPanel` — `SetPanelText(..., "#OVT-Map_Layers_Title")` |
| `OverlaysHeader` | `OVT_MapLayersPanel.layout` | `OVT_MapLayersUI.BuildPanel` — `SetPanelText(..., "#OVT-Map_Layers_Overlays")` |
| `MarkersHeader` | `OVT_MapLayersPanel.layout` | `OVT_MapLayersUI.BuildPanel` — `SetPanelText(..., "#OVT-Map_Layers_Markers")` |
| `OverlayRows` | `OVT_MapLayersPanel.layout` | `OVT_MapLayersUI.BuildPanel` / `BuildRows` — parent container for the canvas-layer rows **and** the hand-built player row |
| `TypeRows` | `OVT_MapLayersPanel.layout` | `OVT_MapLayersUI.BuildPanel` / `BuildRows` — parent container for the 14 location-type rows |
| `FocusProxy` | `OVT_MapLayersPanel.layout` | `OVT_MapLayersUI.OnPanelBuilt` — `FindHandler(SCR_EventHandlerComponent)`, then `GetOnFocus()` bounces engine focus onto the first row (K10). Authored as vanilla's journal does it: a **full-stretch** `ButtonWidget`, `Opacity 0`, `style blank`, placed as the **first** overlay child so content renders on top and mouse clicks still reach the rows |
| `RowHighlight` | `UI/Layouts/Map/Core/OVT_MapLayerRow.layout` | `OVT_MapLayerRowComponent.Init` — the row-wide focus/mouse wash. **Not in the plan's §3.6 table; added during Phase 4** as the second of two independently-sufficient focus visuals (the first is vanilla's own `SCR_ButtonEffectColor` line highlight on `WLib_Checkbox`, which costs nothing to get) |
| `RowIcon` | `OVT_MapLayerRow.layout` | `OVT_MapLayerRowComponent.Init` — `LoadImageFromSet`; **hidden when the row has no imageset** (overlay and player rows have no icon source) |
| `RowLabel` | `OVT_MapLayerRow.layout` | `OVT_MapLayerRowComponent.Init` — `SetText` |
| `RowCheckbox` | `OVT_MapLayerRow.layout` (inherits `{5D5055E10FD00549}UI/layouts/WidgetLibrary/ToolBoxes/WLib_Checkbox.layout`) | `OVT_MapLayerRowComponent.Init` — `SCR_CheckboxComponent.GetCheckboxComponent("RowCheckbox", m_wRoot)`, then `m_OnChanged.Insert(...)`. 🔴 **The inherited `SCR_CheckboxComponent` override reuses the base layout's component GUID `{546A9B7B0A8AD927}`** — a fresh GUID adds a second, unconfigured component and the checkbox goes dead |

**Seven structural names in these two layouts are never resolved by script** and are listed so a future
reader does not go looking for the code that reads them: `PanelOverlay`, `PanelBackground`, `PanelContent`,
`TitleStripe`, `RowsScroll` and `RowsContent` in `OVT_MapLayersPanel.layout`, and `RowContent` in
`OVT_MapLayerRow.layout` (alongside the row root `LayerRow`, which is likewise only ever held as a widget
reference). They carry layout structure — the scroll container the 17 rows live in, the background, the
title rule — and renaming any of them is safe from script's point of view.

---

## The `OVT_MapCanvasLayer` Contract

The overlay half of the map — canvas draw commands rather than marker widgets (Important Decision 3). Like
the `OVT_MapLocationType` table above, **it is never changed silently**; every row names the feature that
added it. Everything below was added by **`map/territory-overlay` Phase 1 (2026-08-11)** and is **additive** —
`OVT_MapRestrictedAreas` and `OVT_MapThreatGrid` compile and behave as before without setting any of it.

⚠️ **None of this has been executed under an automated gate.** Canvas rendering is invisible to every test
tier; the evidence for the rows below is `tools/compile-check.sh` plus the user's Phase 2 probe session.

| Member | Kind | Purpose |
|---|---|---|
| `Draw()` | virtual | **Fill `m_Commands` for this frame. Override this, never `Update`.** `Update` calls it only when the layer is visible, then always submits. |
| **`m_iDrawOrder`** | attribute | **Added by `map/territory-overlay`.** Composite order. **Lower is drawn FIRST and therefore sits UNDERNEATH.** Shipped values: territory `100`, restricted areas `200`. Default `100`. |
| **`m_sLayerId`** | attribute | **Added by `map/territory-overlay`.** Stable, lowercase, no-spaces id the layer-toggle UI (`map/map-layers`) addresses this layer by. Shipped: `"territory"`, `"restricted"`. Default empty. |
| **`m_sDisplayName`** | attribute | **Added by `map/territory-overlay`.** Localization key for the layer's player-facing name. **Nothing renders it yet** — feature 7 does. Shipped: `#OVT-Map_Layer_Territory`, `#OVT-Map_Layer_Restricted`. Both ids exist in `Language/localization_Overthrow.st` **only**; they render as raw keys until the user regenerates the exports in Workbench. |
| **`m_bVisible` / `SetLayerVisible(bool)` / `IsLayerVisible()`** | member + getter/setter | **Added by `map/territory-overlay`. This is the toggle primitive — not `SetActive`.** Hidden means "clear the bucket and submit it empty", so the layer's commands leave the composite on the **next** frame and come back instantly. `SCR_MapModuleBase.SetActive(false)` calls `DeactivateModule` and there is **no script-reachable way back** (`ActivateModules` and `m_aActiveModules` are both `protected` in `SCR_MapEntity`), so it is one-way and unusable as a toggle. |
| **`CacheProjection()`** | method | **Added by `map/territory-overlay`.** Derives an affine world→screen basis with **three** `WorldToScreen` calls (origin + two axis probes). Call **once per frame** before any per-vertex work. |
| **`ProjectWorld(wx, wz, out sx, out sy)`** | method | **Added by `map/territory-overlay`.** Projects one world point through the cached basis with no engine call. Measured by the Phase 2 probe at **0 – 1.41 px** error against a direct `WorldToScreen` across four zooms and two pan positions — i.e. exactly 0, 1 or √2 px, which is `WorldToScreen`'s own integer truncation and **zero basis error**. Requires a prior `CacheProjection()`; it is inert without one. |
| `DrawCircle(center, range, color, n = 36, **tex = null, uvScale = 0**)` | method | **The last two params were added by `map/territory-overlay`.** `PolygonDrawCommand.m_pTexture` / `m_fUVScale` are only touched when `tex` is non-null, so every existing untextured call site produces a byte-identical command. `m_fUVScale`'s **units are still unknown** — the probe drew textures but never established how different scales tiled. |
| **Compositor registration lifecycle** | lifecycle | **Added by `map/territory-overlay`.** `OnMapOpen` registers with `OVT_MapCanvasCompositor`; `OnMapClose` **and** `SetActive(false)` unregister. `SetActive` unregisters **before** `super`, because super's `DeactivateModule` is what stops the layer ever updating again. **A subclass overriding any of the three MUST call `super`** — though `SubmitAndFlush` also adopts an unregistered layer defensively, so the symptom of forgetting is not a silent blank overlay. |
| **The `Count() > 0` guard is GONE** | removal | **Removed by `map/territory-overlay`.** `Update` used to skip `SetDrawCommands` when its list was empty, so a layer that cleared its bucket left its **last non-empty frame on the canvas indefinitely**. It was invisible while only one layer was live and became load-bearing the moment anything could be toggled off. The flush is now unconditional and lives outside every branch. ⚠️ Note `SetDrawCommands` takes a **pointer, not a copy** — the caller must keep the array alive — so the compositor's shared list is cleared and refilled in place and is **never reassigned or nulled**. |

**Why a compositor exists at all.** Every `OVT_MapCanvasLayer` resolves the *same* `CanvasWidget`
(`SCR_MapConstants.DRAWING_WIDGET_NAME`), and each used to call `m_Canvas.SetDrawCommands(m_Commands)` with
**its own** list — so with two live layers the last to run overwrote the first. That was invisible only
because `OVT_MapThreatGrid` ships disabled. `OVT_MapCanvasCompositor` (a static singleton,
`Scripts/Game/UI/Map/Core/OVT_MapCanvasCompositor.c`) holds one shared command list, stamps each layer's
bucket with the frame it was filled at, and on **every** submit concatenates the current-stamped buckets in
`m_iDrawOrder` order. `GetLayers()` returns the registered layers — the list `map/map-layers` builds its
toggle rows from.

**`map/map-layers` (2026-08-11) added NO rows to this table, and that is worth saying out loud.**
`territory-overlay` Phase 1 designed the layer half of feature 7's contract before feature 7 existed, and
when its first consumer actually arrived it needed **nothing added and nothing changed**: `m_sLayerId`,
`m_sDisplayName`, `SetLayerVisible` / `IsLayerVisible` and `GetLayers()` are consumed exactly as specified.
The overlay rows in the layers panel are built generically, one per `GetLayers()` entry, keyed
`"layer:" + GetLayerId()`. `GetLayers()` is **read, never mutated** by the panel. A contract that survives
its first real consumer untouched is a rare thing in this epic; the two `OVT_MapLocationType` extensions
above are what the normal case looks like.

**One documented exception — `OVT_MapPlayerLocation` (K5, `map/map-layers`).** It is a
`SCR_MapUIBaseComponent`, not an `OVT_MapCanvasLayer`, so it **never appears in `GetLayers()`** and its row
in the layers panel is **hand-built** — the only non-generic row of the seventeen. It gained
`m_bMarkersVisible` / `SetMarkersVisible(bool)` / `AreMarkersVisible()` (the setter loops `m_Widgets` and
uses `SetVisible`, **never** `SetOpacity`, so it cannot fight `Update()`'s own opacity handling) and
`IsAvailableThisSession()`, which is set true only past **all** of `OnMapOpen`'s early returns — no
controlled entity, `m_Difficulty.showPlayerOnMap` false, and a third the plan did not enumerate, an
unresolved player faction. An unavailable session presents **no row at all** rather than a dead toggle.
Three options existed — reparent it onto `OVT_MapCanvasLayer`, drop the row, or special-case it — and the
user chose the special case: reparenting a working, retained component (which `legacy-retirement`
deliberately kept) to satisfy a list builder buys nothing and risks a live feature, while dropping the row
leaves a visible marker with no control, which is exactly the inconsistency a filter panel exists to remove.
**One special case with a comment on it beat reparenting a working component.**

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
