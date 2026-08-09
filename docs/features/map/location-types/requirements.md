# Map Location Types — Requirements

**Epic:** map
**Created:** 2026-08-10

## Overview

This feature owns the **content** of the map: the set of `OVT_MapLocationType` subclasses that turn campaign state into interactive markers, their config entries in `Configs/Map/OverthrowMap.conf`, their icons, and their info panels. Ten types already ship on the branch (Town, Base, RadioTower, FOB, Port, GunDealer, Shop, House, Camp, Warehouse); this feature brings that set to **parity with the legacy `OVT_MapIcons` layer** so the old system can be deleted.

Three gaps stand between the current state and parity: there is no Vehicle location type (the legacy layer drew owned vehicles), bus stops are not Overthrow locations at all, and seven of the ten types fall back to the generic `OVT_MapInfoPanel.layout` instead of showing type-appropriate information.

## Requirements

- **Migrate bus stops to an Overthrow marker component.** Replace vanilla-descriptor discovery (`OVT_TownManagerComponent.GetNearestBusStop` at `:881`, proximity-querying `EMapDescriptorType.MDT_BUSSTOP`) with a marker component following the **port pattern** — cf. `OVT_PortControllerComponent`, an empty `OVT_Component` subclass located via `FindComponent` (`OVT_EconomyManagerComponent.c:1860`). A bus stop must be attachable to **any entity** via prefab composition, discoverable through a manager registry rather than a world query, and renderable as a normal location type with its own icon and info panel.
- Existing world-placed bus stops must not silently disappear: either the shipped bus-stop prefabs/compositions gain the component, or a documented migration path covers them. State what happens to a save made before the migration.
- **Add the missing Vehicle location type** so owned/registered vehicles appear on the map, matching what `OVT_MapIcons` drew, including position freshness for vehicles that move.
- **Give each remaining type a type-appropriate info panel.** Only Base, RadioTower and Town have bespoke layouts (`UI/Layouts/Map/LocationTypes/`); FOB, Port, GunDealer, Shop, House, Camp and Warehouse currently render through the generic fallback. Each panel must show what a player actually needs for that location (e.g. ownership and rent state for houses, stock/type for shops, garrison and upgrade state for FOBs).
- Every type must declare sensible **visibility and decluttering** behaviour — `m_fVisibilityZoom`, `m_fShowNameZoom`, `m_bShowDistance`, `m_bShowName` — so a fully-populated map stays readable at every zoom level.
- Icons must be legible and distinguishable at minimum zoom, and faction-coloured types (`m_bUseFactionColor`) must read correctly for both factions.
- Marker sets must be correct in **multiplayer and on join-in-progress** — a JIP client must see the same locations as a client that was present from the start, including ownership-dependent types (houses, vehicles, camps) which are per-player.
- Types that expose player-specific state must not leak another player's information (e.g. one player's owned house or vehicle must not be visible to others unless it should be).
- All ten-plus types must be selectable and their panels readable **on gamepad/console**.
- Achieving parity is the completion bar: at the end of this feature every location the legacy `OVT_MapIcons` drew must exist in the new system. Enumerate the legacy icon set explicitly and check it off.

## Dependencies

- **`map/core`** — the `OVT_MapLocationType` virtual contract, `OVT_MapLocationData` payload model and element lifecycle must be documented and verified first.
- **`towns/core`** — currently owns bus-stop discovery; the migration moves that responsibility. Coordinate so the town manager is not left with a dead method.
- **`economy/shops`, `economy/real-estate`** — shop, warehouse, gun-dealer and house records and their ownership/rent state.
- **`resistance/fob`**, **`occupying/core`** — FOB, base and radio-tower records.
- **Vehicle registration** (`OVT_VehicleManagerComponent`) — for the Vehicle type's source of truth.
- **Workbench** — new icons in `UI/Imagesets/overthrow_mapicons.imageset`, new `.layout` files and any new prefab/config authoring are Workbench-side work the user must perform or verify.
- Blocks `map/fast-travel` (bus stops must exist as destination markers) and `map/legacy-retirement` (parity gate).

## Out of Scope

- **Changing what the underlying systems store or replicate.** The map reads town, base, FOB, shop, house and vehicle state; defects there are filed against those features. The sole intended write is the new bus-stop component and its registry.
- **The travel verbs themselves.** This feature makes bus stops *exist and be selectable*; the bus-travel rules, cost and execution are `map/fast-travel`.
- **Deleting `OVT_MapIcons`** — parity is demonstrated here, deletion happens in `map/legacy-retirement`.
- **Job/task waypoint markers and spawn points** — deferred with `m_bShowTasks 0` / `m_bShowSpawnPoints 0` unless the legacy parity checklist proves job waypoints were load-bearing.
- **New location categories** that the legacy system never drew — no speculative types.
