# Map Core - Task Checklist

**Last Updated:** 2026-08-10\
**Progress:** Implementation complete (100%) — verification and fixes outstanding

---

## Original Implementation (COMPLETED)

Built on the `new-map` branch, 2025-05 → 2025-08-02.

- \[x\] ✅ `OVT_OverthrowMapUI` container extending `SCR_MapUIElementContainer`
- \[x\] ✅ `OVT_MapLocationType` config-instantiated extension point
- \[x\] ✅ `OVT_MapLocationElement` per-marker widget handler
- \[x\] ✅ `OVT_MapLocationData` runtime record with typed key/value maps
- \[x\] ✅ `OVT_MapCanvasLayer` canvas overlay base
- \[x\] ✅ Config binding (`MapFullscreen.conf` delta + `OverthrowMap.conf`)
- \[x\] ✅ Shared element + info-panel layouts, icon imageset
- \[x\] ✅ Zoom-gated element and name visibility
- \[x\] ✅ Faction-aware icon colouring
- \[x\] ✅ Retrospective documentation created (2026-08-10)
- \[x\] ✅ BUG-069 regression check by code reading — all four defects avoided structurally

---

## Verification (NOT STARTED)

Nothing below has been observed at runtime. The branch has not been play-tested since 2025-08-02.

- [x] ✅ Regenerate `Language/localization_Overthrow.<lang>.conf` in Workbench — done 2026-08-10, all 14 map string ids exported to all six languages
- \[ \] Single-player play-test: open the map, confirm markers appear at correct positions
- \[ \] Confirm hover → panel, click → pin, click-empty → dismiss behaves as read
- \[ \] Zoom sweep: confirm `m_fVisibilityZoom` and `m_fShowNameZoom` gating at min and max zoom
- \[ \] **MP/JIP:** two clients against `tools/launch-server.sh`; verify a JIP client's markers match an established client's
- \[ \] **MP/JIP:** open the map immediately on join, before state has fully replicated
- \[ \] **Gamepad/console:** confirm vanilla synthesises hover from the map cursor, and that the panel can be reached and dismissed without a mouse
- \[ \] Confirm `OVT_MapRestrictedAreas` rings still match the radii `resistance/fob` enforces (BUG-070 must not regress)

---

## Filed bugs — confirm at runtime, then fix

All five were filed from code reading on 2026-08-10 and are **unverified**. Confirm each before fixing so the fix targets observed behaviour.

- [ ] **BUG-133** (medium) — D1: icon zoom-sizing inert. `SetupIconWidget` looks up `"IconLayout"`; the layout's `SizeLayoutWidget` is `"IconContainer"`. *One-line fix, high confidence.*
- [ ] **BUG-134** (medium) — D2+D3: info panel cannot be dismissed. `"CloseButton"` absent from `OVT_MapInfoPanel.layout`, and `HideLocationInfo` early-returns while pinned. Fix points at `ForceHideLocationInfo`.
- [ ] **BUG-135** (medium) — D4: `m_HoveredElement` strong ref never cleared on close; stale element can re-pin on the next map open.
- [ ] **BUG-136** (low) — D6: markers never refresh mid-open; `OnLocationDataChanged()` has no callers. **Blocks `map/location-types` G1 (Vehicle).**
- [ ] **BUG-137** (low) — D7: element click path dead; `OnLocationClicked` never fires, click sound never plays, no click-to-deselect.

**Not filed — recorded as debt instead:**
- D5 — `UpdateIcons` computes a `targetElement` (pinned-else-selected) that `UpdateInfoPanelPosition()` ignores in favour of `m_SelectedElement` (`:113-126`, `:551`). Harmless today because pinning also selects; intent and behaviour disagree. See Cleanup below.

---

## Cleanup / debt (after verification)

- \[ \] Remove or wire up dead code: `HandleSelection()`, `GetClickRadius()`, `OnLocationDataChanged()`
- \[ \] Replace `GetLocationTypeByName`'s linear `ClassName()` scan with a map built in `InitializeLocationTypes` (called 3× per panel show)
- \[ \] Hoist per-element `GetCurrentPlayerID()` out of `SetVisible` / `UpdateFastTravelIndicator` (runs per element per zoom change)
- \[ \] Audit every `FindAnyWidget` name in the map code against its layout — D1 and D2 are the same failure mode and the compiler cannot see it
- \[ \] Review hardcoded info-panel offsets (`x += 13; y -= 31`) and the partial screen-edge clamping

---

## Future Enhancements

See `implementation.md` and the epic's stretch features (`map/territory-overlay`, `map/map-layers`, `map/shared-markers`).

---

*This task file was created retrospectively. Add new tasks here when working on enhancements.*