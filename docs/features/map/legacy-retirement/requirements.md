# Map Legacy Retirement — Requirements

**Epic:** map
**Created:** 2026-08-10

## Overview

This feature **deletes the old map**. Once `map/location-types` and `map/fast-travel` have demonstrated parity, the hand-rolled `OVT_MapIcons` icon layer and the three map modes inside `OVT_MapContext` have no remaining job, and the duplicated main-menu entries that reach them become dead UI. This feature removes them, cleans up the config and menu surfaces they leave behind, and archives the superseded `towns/map-info` feature documentation.

It is deliberately last and deliberately narrow. Its value is entirely in _removal_ — every day both systems coexist is a day of double maintenance, two sources of map truth, and a menu that offers the player two ways to do the same thing.

## Requirements

- **Delete `Scripts/Game/UI/Map/OVT_MapIcons.c`** and its now-unused support: the `MapIcon.layout` and legacy `MapInfo` layouts (`UI/Layouts/Map/MapIcon.layout`, `UI/Layouts/Map/MapInfo.layout`, `UI/Layouts/Map/MapInfo/Modifier.layout`) where nothing else references them, and the `OVT_MapIcons` block in `Configs/Map/MapOverthrow.conf` (currently present but already switched off with `Enabled 0` / `m_bDisableComponent 1`).
- **Strip the three map modes from `OVT_MapContext`**: `EnableMapInfo`/`DisableMapInfo`, `EnableFastTravel`/`DisableFastTravel`, `EnableBusTravel`/`DisableBusTravel`, their `m_bMapInfoActive`/`m_bFastTravelActive`/`m_bBusTravelActive` flags, the `MapClick` branching that dispatches on them, `ShowTownInfo`, and the `MapExit`/`OnMapExit` teardown for those modes.
- **Remove the static `SCR_MapEntity.GetOnMapClose()` subscription leak** if any part of `OVT_MapContext` survives — `PostInit` (`:34`) inserts a listener that is never removed and accumulates one dead binding per respawn (BUG-069, part 4). If the whole context goes away, confirm no other context reintroduces the pattern.
- **Remove the duplicated main-menu entries** — the "Map Info" (`OVT_MainMenuContext.c:212`) and "Fast Travel" (`:218`) options that call into the retired modes — along with any localization ids and layout rows that exist only to serve them.
- **Re-point `OVT_CatchBusAction`** (`:10`) at the new map bus-travel flow; the world action must keep working.
- **Do not remove the canvas-layer modules.** `OVT_MapRestrictedAreas`, `OVT_MapThreatGrid` (disabled) and `OVT_MapPlayerLocation` are still live `SCR_MapConfig` modules and are explicitly retained.
- **Verify nothing else referenced the deleted code** — a compile-clean tree is necessary but not sufficient; check configs, prefabs and layouts, which the compiler does not check. Note that `Configs/Map/MapOverthrow.conf` is a **same-GUID delta over vanilla's**, so removing a block changes what merges with vanilla rather than replacing a whole file.
- **Archive `docs/features/towns/map-info`** to `docs/archive/`, leaving a pointer to this epic, and update `docs/features/towns/epic-overview.md` and `docs/overview.md` so the towns epic no longer claims to own the map.
- After removal: `tools/compile-check.sh` clean, `tools/run-tests.sh "{6A6E2A002F53A581}"` green, and a **multiplayer play-test** confirming the map, both travel verbs and the bus action all still work with the legacy code gone.

## Dependencies

- **`map/location-types` — hard gate.** Every location the legacy `OVT_MapIcons` drew (including vehicles and bus stops) must exist in the new system first.
- **`map/fast-travel` — hard gate.** Fast travel and bus travel must be fully working from the new map before the `OVT_MapContext` modes are stripped.
- **`map/core`** — the documented contract that justifies calling parity.
- **`towns/map-info`** — the feature doc being archived; read it as the authoritative list of what the legacy system did, so nothing is deleted that was never replaced.
- **Workbench** — config and layout deletions need Workbench-side verification; deleted `.layout`/`.conf` GUID references cannot be caught by the script compiler.

## Out of Scope

- **Any new map capability.** This feature only removes; if a gap is discovered during retirement, it goes back to `map/location-types` or `map/fast-travel` rather than being fixed here.
- **Refactoring the rest of `OVT_MapContext`.** Only its three map modes and their supporting members are stripped; any unrelated responsibility left in that context stays where it is (and if nothing remains, deleting the context itself is fine — but that is a consequence, not a goal).
- **Touching the canvas-layer modules** beyond confirming they still work.
- **Reworking the main menu** beyond removing the two dead entries.
- **Deleting `docs/archive/OverthrowMapSystem.md`** — the original 2025 design note stays archived as history, even though it is stale (it predates nine of the ten shipped location types).
