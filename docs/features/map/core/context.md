# Map Core - Context & Decisions

**Last Updated:** 2026-08-10
**Current Phase:** Retrospective Documentation
**Status:** ✅ Documented (Existing Feature) — ⚠️ unverified at runtime

---

## Quick Status

**What's Done:**
- ✅ Feature implemented on the `new-map` branch (2025-05 → 2025-08-02)
- ✅ Branch merged up to date with `main` (2026-08-10, commit `6b0169e4`) — compiles clean, All group 76/76
- ✅ Retrospective documentation created
- ✅ BUG-069 regression check completed by code reading — **all four legacy lifecycle defects avoided structurally**

**What's Next:**
- 🔴 Runtime verification — nothing here has been play-tested since 2025-08-02
- 🔴 Confirm then fix **BUG-133 … BUG-137** (filed 2026-08-10 from findings D1–D7; D5 kept as debt). BUG-133/134 are high-confidence and cheap
- 🔴 MP/JIP verification; gamepad/console verification

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
| `OnLocationClicked(location, element)` | virtual | ⚠️ **Unreachable** — `HandleSelection()` has no callers (D7). Do not override. |
| `GetLocationName` / `GetLocationDescription` / `GetDisplayNameForLocation` | virtual | Header text for the info panel. |
| `GetIconName(location)` / `GetIconColor(location)` | virtual | Per-record icon and tint. |
| `OnSetupIconWidget(iconWidget, location, isSmall)` | virtual | Per-record icon customisation. Re-runs on zoom change — keep it cheap. |
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

**5. Interaction model: hover shows, click pins, click-empty dismisses.**
Not the conventional click-to-open. It matters because the "close" affordance was evidently designed for a different model and does not currently work (D2/D3).

---

## Gotchas & Learnings

- **`FindAnyWidget` returning null is a silent no-op, and the compiler cannot see it.** Two configured features are dead purely from widget-name mismatches: `IconLayout` (D1 — zoom icon sizing) and `CloseButton` (D2 — panel dismissal). Any layout↔code name contract needs a runtime pass or an explicit audit; `tools/compile-check.sh` will never catch this class.
- **`OVT_MapLocationType.Init()` runs on every map open, not once.** It re-caches all seven manager singletons each time. Don't write `PostInit` code that assumes single execution.
- ~~**`UpdateInfoPanel` no-ops entirely when `m_InfoLayout` is empty**~~ — **superseded 2026-08-10 by `map/location-types` Phase 5.** It used to return early when `m_InfoLayout` was empty, which is why seven of ten types showed only the panel header: they were not falling back to a generic renderer, they were contributing nothing to `ContentSlot`. There is now a second branch (`m_SharedInfoLayout` → `BuildInfoRows`) — see the contract table above. The bespoke path is byte-for-byte unchanged. A type that supplies neither layout, or whose `BuildInfoRows` adds no rows, still contributes nothing (the empty container is removed again).
- **`CanFastTravel` is on a hot path.** It runs per element inside `UpdateFastTravelIndicator`, which runs for every element on every zoom change. Keep implementations cheap.
- **`m_bShowSpawnPoints` / `m_bShowTasks` are vanilla attributes**, not Overthrow ones (`SCR_MapUIElementContainer.c:23,26`). Setting them `0` suppresses vanilla's own icons — correct config, not dead keys.
- **The type's `m_sName` is a Workbench editor label only** (`OVT_MapLocationTypeTitle._WB_GetCustomTitle`). Never shown to players; not localized.
- **`OnLocationClicked` is currently unreachable** because its only caller, `HandleSelection()`, has no callers itself. Subclasses overriding it are writing code that does not run.

---

*This context file was created retrospectively by analyzing existing code.*
