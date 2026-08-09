# Map Location Types - Implementation Plan (Retrospective)

**Status:** Partially Implemented — 10 types shipped, **4 parity gaps outstanding**
**Epic:** map (feature 2 of 7)
**Originally Implemented:** 2025-05 → 2025-08-02, on the `new-map` branch
**Documented:** 2026-08-10
**Last Updated:** 2026-08-10

---

## Executive Summary

`map/location-types` is the **content** half of the map: ten `OVT_MapLocationType` subclasses that turn replicated campaign state into markers, each declared as an entry in `Configs/Map/OverthrowMap.conf`. Every type does the same three things — populate `OVT_MapLocationData` records in `PopulateLocations`, optionally vary its icon, and optionally fill an info panel — and the core container does the rest.

The set is substantial but **not at parity with the legacy `OVT_MapIcons` layer it replaces**. Reading the legacy component turned up four categories it drew that the new system has no equivalent for: **owned vehicles**, **job waypoints**, the **POI registry** (garages and maintenance ramps that self-register), and **bus stops** (which are not really a legacy map category at all — they are vanilla map descriptors the legacy bus flow queried by proximity). Until all four are covered, `OVT_MapIcons` cannot be deleted.

**Note:** This is a retrospective plan created by reading the code on the merged `new-map` branch. Claims are cited to `file:line`. Nothing has been observed at runtime — the branch has not been play-tested since 2025-08-02.

---

## Goals

### Primary Goals
- One subclass per campaign location category, declared in config, requiring no core map changes.
- Type-appropriate info and icons per category.
- Reach parity with `OVT_MapIcons` so the legacy layer can be deleted.

### Success Criteria
- [x] Ten types implemented and configured
- [x] Per-type icon, zoom and fast-travel configuration
- [x] Fast-travel eligibility delegated to one shared service
- [ ] **Vehicle** location type — legacy drew it, new system has none
- [ ] **Job waypoint** marker — legacy drew it, new system has none
- [ ] **POI registry** equivalent — legacy drew garages/ramps, new system has none
- [ ] **Bus stops** as first-class Overthrow locations (component migration)
- [ ] Bespoke info panels for the 7 types that have none
- [ ] MP/JIP verification, gamepad verification

---

## Current Architecture

### The ten shipped types

| Type | Lines | Source of truth | Overrides | Info panel | Fast travel |
|---|---:|---|---|---|---|
| `Town` | 307 | `m_TownManager` | `PopulateLocations`, `GetDisplayName`, `GetDisplayNameForLocation`, `GetIconColor`, `CanFastTravel`, `OnSetupLocationInfo`, **`OnSetupIconWidget`** | ✅ `OVT_MapInfoTown.layout` | ❌ explicitly refused (`:303-307`) |
| `Base` | 171 | own `m_OccupyingFactionManager` | `PopulateLocations`, `GetIconColor`, `CanFastTravel`, `OnSetupLocationInfo` | ✅ `OVT_MapInfoBase.layout` | ✅ resistance-held only |
| `RadioTower` | 105 | own `m_OccupyingFactionManager` | `PopulateLocations`, `GetIconColor`, `OnSetupLocationInfo` | ✅ `OVT_MapInfoRadioTower.layout` | ❌ |
| `House` | 136 | `m_RealEstate` | `PopulateLocations`, `GetIconColor`, `GetLocationDescription`, `CanFastTravel` | ❌ generic | ✅ owner/renter only |
| `Warehouse` | 105 | `m_RealEstate` | `PopulateLocations`, `GetIconColor`, `GetLocationDescription` | ❌ generic | ❌ |
| `Shop` | 96 | local `OVT_Global.GetEconomy()` | `PopulateLocations`, `GetLocationName`, `GetIconName` | ❌ generic | ❌ |
| `FOB` | 72 | `m_Resistance` | `PopulateLocations`, `GetIconName`, `CanFastTravel` | ❌ generic | ✅ global rules only |
| `Camp` | 63 | `m_Resistance` | `PopulateLocations`, `CanFastTravel` | ❌ generic | ✅ own/public only |
| `Port` | 34 | local `OVT_Global.GetEconomy()` | `PopulateLocations` | ❌ generic | ❌ |
| `GunDealer` | 34 | local `OVT_Global.GetEconomy()` | `PopulateLocations` | ❌ generic | ❌ |

Only **Base, RadioTower, Town** override `OnSetupLocationInfo`, exactly matching the three with `m_InfoLayout` configured. This is not a "generic fallback renderer" — `UpdateInfoPanel` returns early when `m_InfoLayout` is empty (`OVT_MapLocationType.c:126-127`), so the other seven contribute **nothing** to `ContentSlot` and show only the shell's header (name, type, distance, owner, fast-travel button).

**No type overrides `OnLocationClicked`** — which is fortunate, since `map/core` finding D7 established that virtual is currently unreachable (its only caller, `HandleSelection()`, is itself uncalled). Nothing is silently broken by it today, but it is a trap for whoever adds the next type.

### Data-key matrix

Keys each type writes into `OVT_MapLocationData`'s typed maps. These are **unregistered strings** — a typo silently yields the default.

| Type | Keys |
|---|---|
| `Town` | `faction`, `population`, `stability`, `support`, `townType` |
| `Base` | `faction`, `garrisonCount`, `isOccupying` |
| `RadioTower` | `faction` |
| `FOB` | `owner`, `persistentId`, `garrisonCount`, `isPriority`, `visibilityZoom` |
| `Camp` | `owner`, `persistentId`, `garrisonCount`, `isPrivate` |
| `House` | `houseID`, `owner`, `renter`, `isOwned`, `isRented`, `visibilityZoom` |
| `Warehouse` | `warehouseID`, `owner`, `renter`, `isOwned`, `isRented` |
| `Shop`, `Port`, `GunDealer` | *(none — position and name only)* |

`House`/`Warehouse` share an ownership key shape (`owner`/`renter`/`isOwned`/`isRented`); `FOB`/`Camp` share `owner`/`persistentId`/`garrisonCount`. Neither pairing is factored into a shared helper.

### Fast travel: one gate per type, one shared service

All four travel-capable types apply a **local eligibility rule** then delegate to the shared service:

- `Base` — refuses if `isOccupying` (`#OVT-CannotFastTravelEnemyBase`), else `OVT_FastTravelService.CanGlobalFastTravel` (`:153-171`)
- `Camp` — refuses if `isPrivate` and `owner != playerID`, else the service (`:43-63`)
- `House` — refuses unless player is `owner` or `renter`, else the service (`:119-136`)
- `FOB` — no local rule; straight to the service (`:61-72`)
- `Town` — unconditional refusal (`:303-307`)

This is the right shape: per-type policy on top of one global rule set. The global half belongs to `map/fast-travel`.

### Config-driven variation *within* a type

`Shop` demonstrates a sub-pattern worth reusing: a nested config array `m_aShopTypes` of `OVT_ShopTypeInfo` objects (`TypeInfo/OVT_ShopTypeInfo.c`), each mapping an `OVT_ShopType` enum value to a display name and icon name. `GetLocationName` and `GetIconName` look up the entry and fall back to a default (`Shop.c:56-96`). It means one type class serves five shop kinds without subclassing, and the icon/name mapping is authored in Workbench.

`Town` does the same thing in code rather than config — `m_sVillageIconName` / `m_sTownIconName` / `m_sCityIconName` attributes selected by town size.

---

## Parity gaps — what the legacy layer draws that this does not

Established by reading `Scripts/Game/UI/Map/OVT_MapIcons.c`. Its imageset icon names are `camp`, `gundealer`, `house`, `port`, `tower`, `vehicle`, `warehouse`, `waypoint`.

**G1 — Owned vehicles.** Legacy drew a `vehicle` icon. There is no `OVT_MapLocationVehicle`. Source of truth would be `OVT_VehicleManagerComponent` (already cached on the base class as `m_Vehicles` and currently unused by every shipped type). Vehicles move, which makes this the one type where `map/core`'s D6 finding — markers never refresh while the map is open — actually bites.

**G2 — Job waypoints.** Legacy drew a `waypoint` icon from `OVT_JobManagerComponent.m_vCurrentWaypoint` (`OVT_MapIcons.c:779-787`). The new container sets vanilla's `m_bShowTasks 0`, so neither Overthrow's nor vanilla's task markers are drawn. **This is a genuine parity gap, not an optional deferral** — the epic's requirements previously hedged on it; the legacy code settles it.
Related: `OVT_RecruitsContext.ShowOnMap` (`:479-491`) writes a recruit's position into that *same* `m_vCurrentWaypoint` field, so "show recruit on map" and job waypoints share one slot and overwrite each other. That is a pre-existing defect in the legacy design, and whatever replaces the waypoint marker should not inherit it.

**G3 — The POI registry.** Legacy exposed a static `OVT_MapIcons.RegisterPOI(uiInfo, origin, mustOwnBase)`; entities self-register through `OVT_MainMenuContextOverrideComponent.EOnFrame` (`:54`) when `m_bShowOnMap` is set, optionally gated on being within 220 m of an owned base (`m_bMustOwnBase`). Shipped on **garages, vehicle maintenance ramps and military garages** (`Prefabs/Structures/.../Garage_E_02.et`, `OVT_VehicleMaintenanceRamp.et`, `GarageMilitary_E_01_base.et`). Deleting `OVT_MapIcons` deletes the static registry these components call — so this is both a parity gap **and** a compile-level dependency on the legacy file.

**G4 — Bus stops.** Not a legacy *map icon* at all: bus stops are vanilla `EMapDescriptorType.MDT_BUSSTOP` descriptors found by proximity query in `OVT_TownManagerComponent.GetNearestBusStop` (`:881`, filter at `:1197-1203`). The epic's decision is to migrate them to an Overthrow marker component so they can be attached to any entity and rendered as a normal location type.

**Two existing precedents for that component pattern**, both already in the codebase:
- **Marker-component + manager lookup** — `OVT_PortControllerComponent` is an empty `OVT_Component` subclass that `OVT_EconomyManagerComponent` locates via `FindComponent` (`:1860`).
- **Self-registering component** — `OVT_MainMenuContextOverrideComponent` registers itself into a static registry on first frame, carrying an `SCR_UIInfo` and visibility rules.

The second is closer to what bus stops need (attachable to anything, carries its own display info); the first is closer to how the rest of Overthrow's location data is discovered. G3 and G4 should be designed together — a single "entity-attached map marker component" could serve both, and would let `Port`/`GunDealer` stop querying the economy manager for what are essentially static world placements.

---

## Current State

### What's working
- Ten types populate from replicated state and render with per-type icons, colours and zoom gating.
- Per-type fast-travel policy layered cleanly over one shared service.
- Faction colouring via the inherited `GetIconColor`/`GetFactionColor` path.
- `Shop`'s nested type-info config, and `Town`'s size-based icon selection.

### Technical debt

- **T1 — Three different manager-access idioms coexist.** The base class caches seven managers in `Init()` (`OVT_MapLocationType.c:72-78`), and only five types use them. `Base` and `RadioTower` declare their **own** `m_OccupyingFactionManager` member and lazily resolve it (`Base.c:15,21`; `RadioTower.c:9,15`), shadowing the inherited `m_OccupyingFaction`. `Shop`, `Port` and `GunDealer` ignore the cache entirely and call `OVT_Global.GetEconomy()` inside `PopulateLocations`. Pick one idiom.
- **T2 — `m_Vehicles` is cached for every type and used by none** — a vestige of the intended Vehicle type (G1).
- **T3 — Unregistered string keys.** No shared constants; `House`/`Warehouse` and `FOB`/`Camp` duplicate key sets by convention only.
- **T4 — Seven types have no info panel.** Selecting a shop, port, gun dealer, warehouse, FOB or camp shows only a name, type, distance and (sometimes) a travel button — strictly less than the legacy panel offered for several of these.
- **T5 — `Town` at 307 lines** is three times the next largest type; its info panel builds modifier chips inline. A candidate for extraction if it grows further.

---

## Testing

### Current coverage
None. No autotest suite touches map location types. The Fast (38) and All (76) groups cover logic, init, campaign and persistence only.

### Testing gaps
- Every type is unverified at runtime since 2025-08-02.
- **Per-player types are the MP risk**: `House`, `Warehouse`, `Camp` and (future) `Vehicle` carry `owner`/`renter` identity. Whether one player can see another's ownership — or whether a JIP client resolves persistent IDs correctly at map-open time — is untested and is exactly the kind of thing that works single-player and fails on a server.
- `CanFastTravel` runs per element on every zoom change (`map/core`, `OVT_MapLocationElement.c:302`); the per-type implementations are cheap map lookups today, but `House` and `Camp` do string comparisons against the player ID for every house on the map.

---

## Dependencies

### Internal
- `map/core` — the `OVT_MapLocationType` contract and `OVT_MapLocationData` payload model.
- `towns/core` (town records, and bus-stop discovery being migrated), `occupying/core` (bases, radio towers), `resistance/fob` (FOBs, camps), `economy/shops` + `economy/real-estate` (shops, ports, gun dealers, houses, warehouses), `OVT_VehicleManagerComponent` (for G1), `jobs/core` (for G2).
- `OVT_MapIcons.RegisterPOI` — a **static method on the legacy file**, called by `OVT_MainMenuContextOverrideComponent` (G3). This is a hard link between the legacy component and live prefabs.

---

## Notes

**Discovered information**
- The legacy icon set (`camp`, `gundealer`, `house`, `port`, `tower`, `vehicle`, `warehouse`, `waypoint`) is the definitive parity checklist, and it revealed two gaps the epic had not accounted for (G2, G3).
- `OVT_MainMenuContextOverrideComponent` is the closest existing thing to the "attach a bus stop to any entity" pattern the epic wants — it just registers into the legacy system instead of the new one.
- Every fast-travel-capable type routes through `OVT_FastTravelService.CanGlobalFastTravel`, so the global rule set already has exactly one implementation. That is a good starting position for `map/fast-travel`.

**Retrospective assessment**
- *What works well:* the config-driven type pattern delivered on its promise — ten categories, no core changes, and two independent mechanisms (Shop's type-info array, Town's size icons) for varying appearance inside a single type.
- *What could be improved:* the type set was built breadth-first — every type populates and shows an icon, but only three finish the job with a real info panel. The result reads as complete on the map and thin on selection.
- *Lesson:* discovering the legacy component's icon list first would have scoped this feature correctly from the start. The parity checklist should have come from the code being replaced, not from the design note (`docs/archive/OverthrowMapSystem.md`), which lists nine "next steps" and mentions neither waypoints nor POIs.

---

*This retrospective plan was created by analyzing existing code. Use `/start-feature map/location-types` to begin closing the parity gaps.*
