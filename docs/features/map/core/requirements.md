# Map Core — Requirements

**Epic:** map
**Created:** 2026-08-10

## Overview

This feature owns the map's **infrastructure**: the `OVT_OverthrowMapUI` container that vanilla instantiates from `Configs/Map/MapFullscreen.conf`, the `OVT_MapLocationType` / `OVT_MapLocationElement` / `OVT_MapLocationData` triple that every location type builds on, the `OVT_OverthrowMapConfig` binding, the shared element and info-panel layouts, the map icon imageset, and the `SCR_MapConfig` canvas-layer modules. It is the extension point the rest of the epic plugs into.

This is primarily a **discovery** feature, not greenfield work. The code already exists and is already live on the `new-map` branch — it was written between 2025-05 and 2025-08-02 and has never been documented, reviewed, or verified in multiplayer. The job is to establish what was actually built, write it down accurately, and prove it works before three more features are stacked on top of it.

## Requirements

- Produce an accurate retrospective implementation record of the shipped core: `OVT_OverthrowMapUI`, `Core/OVT_MapLocationType.c`, `Core/OVT_MapLocationElement.c`, `Core/OVT_MapLocationData.c`, `Core/OVT_MapCanvasLayer.c`, `Configs/Map/OverthrowMap.conf`, `Configs/Map/MapFullscreen.conf`, `UI/Layouts/Map/Core/*`, and `UI/Imagesets/overthrow_mapicons.imageset`.
- Document the **`OVT_MapLocationType` extension contract** precisely enough that a new type can be added without reading the core source: every virtual method, when the container calls it, what it may assume about client vs server, and what it must not do.
- Document the **`OVT_MapLocationData` payload model** — the typed `GetDataInt`/`SetDataBool`-style accessors, who populates them, and their lifetime relative to map open/close.
- Document the **element lifecycle**: creation, positioning, visibility/zoom gating (`m_fVisibilityZoom`, `m_fShowNameZoom`), selection and hover state, info-panel show/update/hide, and teardown on map close.
- Verify the lifecycle is leak-free and symmetric across **every** close path, not just the input-driven one — the legacy system's equivalent defect (asymmetric close leaking panels, stacking orphans, and accumulating a static listener per respawn) was BUG-069 and must not have been reproduced in the rewrite.
- Confirm `OVT_MapRestrictedAreas` still draws restriction rings matching the radii `resistance/fob` actually enforces (BUG-070's fix survived the branch merge and the removal of the faction-flag drawing).
- Establish the MP/JIP behaviour of the core: markers must populate correctly for a client that joins a campaign already in progress, and for a client that opens the map before all state has replicated.
- Verify full **gamepad/console** operability of marker selection and the info panel through vanilla's `SCR_MapUIElement` navigation.
- Record the known gaps deliberately left to later features so they are not mistaken for defects: no Vehicle type, seven types on the generic info panel, `m_bShowSpawnPoints 0` / `m_bShowTasks 0`, and the still-live legacy `OVT_MapContext` modes.

## Dependencies

- **Merged `new-map` branch** — done (merged up to date with `main` on 2026-08-10; compiles clean, All test group 76/76).
- **Workbench localization regenerate** — the 14 map string ids exist only in `Language/localization_Overthrow.st`; the generated `<lang>.conf` exports were taken from `main` and must be rebuilt before map strings render. Blocks visual verification, not code discovery.
- Reads campaign state from the manager singletons via `OVT_Global` (`towns/core`, `occupying/core`, `resistance/fob`, `economy/*`) — no changes to those systems.
- Blocks **all** other features in this epic: `location-types` and `fast-travel` extend this contract, and `legacy-retirement` cannot be scoped until it is known.

## Out of Scope

- **Adding or completing location types** — the ten shipped types are surveyed here only to the extent needed to document the base contract; fixing, extending or adding types belongs to `map/location-types`.
- **The travel verbs** — `OVT_FastTravelService`, the `OverthrowFastTravel` keybinding and bus travel belong to `map/fast-travel`.
- **Deleting anything legacy** — `OVT_MapIcons` and `OVT_MapContext` stay untouched; retirement is `map/legacy-retirement`.
- **Porting the canvas layers into the location-type system** — `OVT_MapRestrictedAreas`, `OVT_MapThreatGrid` and `OVT_MapPlayerLocation` remain `SCR_MapConfig` modules; this feature documents and verifies them, it does not rewrite them.
- **Enabling spawn points or tasks on the map** — both ship off; the decision is deferred.
