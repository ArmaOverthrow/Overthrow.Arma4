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

## Filed bugs — ALL CLOSED 2026-08-10

All five were filed from code reading on 2026-08-10, fixed and play-tested the same day.

- [x] **BUG-133** (medium) — D1: icon zoom-sizing inert. **Two stacked faults**, not the one-liner the bug predicted: the lookup name was wrong (`"IconLayout"` → `"IconContainer"`, now the `ICON_CONTAINER` constant) *and* `FrameSlot.SetSize` could never have sized it, because `IconContainer` is in a `LayoutSlot`. Now uses `SizeLayoutWidget.SetWidthOverride`/`SetHeightOverride`.
- [x] **BUG-134** (medium) — D2+D3: info panel cannot be dismissed. `OVT_MapInfoPanel.layout` gained a real `CloseButton` (a `WLib_NavigationButtonSmall` bound to the new `OverthrowCloseInfoPanel` action, `KC_C` / gamepad `b`), pointed at `ForceHideLocationInfo`. Visible only while pinned, which also keeps its keybind off hover panels.
- [x] **BUG-135** (medium) — D4: `m_HoveredElement` never cleared on close. **Already fixed before the fix pass** — `map/fast-travel` (commit `008293c2`) added the clear to `OnMapClose`. Verified by `git log -S`; no further change made.
- [x] **BUG-136** (low) — D6: markers never refresh mid-open. Per-type opt-in polling (`m_fRefreshInterval`, default `0`) drives `OVT_OverthrowMapUI.TickRefresh` → `RefreshLocationType`, which reconciles markers by identity key; survivors are re-pointed through the new `OVT_MapLocationElement.SetLocationData`, giving `OnLocationDataChanged()` its first caller. Enabled at 5 s for Town/Base/RadioTower/FOB/Camp, 2 s for Vehicle. **Unblocks `map/location-types` G1.**
- [x] **BUG-137** (low) — D7: element click path dead. `HandleSelection()` and `GetClickRadius()` deleted; `OVT_OverthrowMapUI.NotifyLocationClicked` now fires `OnLocationClicked` (and the click sound, via the new `PlayClickSound`) when a click pins a location. Default body of the virtual is now empty. **No click-to-deselect** — hover already shows the panel, so BUG-134's close button is the explicit dismissal.

**Filed later — one open bug:**
- [ ] **BUG-138** (low, filed 2026-08-11 by `map/location-types` as finding "core D8") — the per-location visibility-zoom concept does not exist: `OVT_MapLocationElement` reads only the **type-level** `GetVisibilityZoom()` in both `ShouldUseSmallIcon` (`:298`) and `SetVisible` (`:482`), and no per-record key has a reader. So **a priority FOB is not always visible**, and no individual location can differ from its type — a type author writing `SetDataFloat("visibilityZoom", …)` gets a silent no-op. The fix is a per-record override with a sentinel (`0` is a legitimate "always visible"), in both call sites, plus a `map/core` contract-table entry.

**Not filed — recorded as debt instead:**
- D5 — `UpdateIcons` computes a `targetElement` (pinned-else-selected) that `UpdateInfoPanelPosition()` ignores in favour of `m_SelectedElement` (`:113-126`, `:551`). Harmless today because pinning also selects; intent and behaviour disagree. See Cleanup below.

---

## Cleanup / debt (after verification)

- \[x\] ~~Remove or wire up dead code: `HandleSelection()`, `GetClickRadius()`, `OnLocationDataChanged()`~~ — done 2026-08-10. First two deleted (BUG-137), third given a caller (BUG-136).
- \[ \] Replace `GetLocationTypeByName`'s linear `ClassName()` scan with a map built in `InitializeLocationTypes` (called 3× per panel show)
- \[ \] Hoist per-element `GetCurrentPlayerID()` out of `SetVisible` / `UpdateFastTravelIndicator` (runs per element per zoom change)
- \[ \] Audit every `FindAnyWidget` name in the map code against its layout — **still owed.** D1 and D2 were fixed individually (BUG-133/134), but they were found by spot-checking, not by the sweep. Same failure mode, still invisible to the compiler.
- \[x\] ~~Review the partial screen-edge clamping~~ — done 2026-08-10. `UpdateInfoPanelPosition` clamped the right edge and the top but **never the bottom**, so a marker low on the screen got a panel that ran off the display and lost its last rows (reported against shops/gun dealers, whose panels are tallest). Bottom clamp added, and the comparison units fixed: `Widget.GetScreenSize` answers in **physical** pixels while `x`/`y` are DPI-unscaled reference units, so the existing right-edge clamp was only correct at DPI scale 1.0.
- \[ \] Review the hardcoded info-panel offsets (`x += 13; y -= 31`) — still owed, and now joined by `SizeLayout0`'s `Alignment 0.5 0.255` pivot, which was tuned for a fixed 32px icon and no longer tracks the zoom-varying icon size (BUG-133)

---

## Future Enhancements

See `implementation.md` and the epic's stretch features (`map/territory-overlay`, `map/map-layers`, `map/shared-markers`).

---

*This task file was created retrospectively. Add new tasks here when working on enhancements.*