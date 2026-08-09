# Map Location Types - Task Checklist

**Last Updated:** 2026-08-10
**Progress:** Partially implemented — 10 types shipped, 4 parity gaps + 7 info panels outstanding

---

## Shipped (COMPLETED)

Built on the `new-map` branch, 2025-05 → 2025-08-02.

- [x] ✅ `OVT_MapLocationTown` (+ `OVT_MapInfoTown.layout`, modifier chips, size-based icons)
- [x] ✅ `OVT_MapLocationBase` (+ `OVT_MapInfoBase.layout`, faction colour, enemy-held travel gate)
- [x] ✅ `OVT_MapLocationRadioTower` (+ `OVT_MapInfoRadioTower.layout`)
- [x] ✅ `OVT_MapLocationHouse` (ownership/rent keys, owner-or-renter travel gate)
- [x] ✅ `OVT_MapLocationWarehouse`
- [x] ✅ `OVT_MapLocationShop` (+ `OVT_ShopTypeInfo` nested config for per-shop-type name/icon)
- [x] ✅ `OVT_MapLocationFOB`
- [x] ✅ `OVT_MapLocationCamp` (private/public travel gate)
- [x] ✅ `OVT_MapLocationPort`
- [x] ✅ `OVT_MapLocationGunDealer`
- [x] ✅ Retrospective documentation created (2026-08-10)
- [x] ✅ Parity checklist derived from `OVT_MapIcons.c` rather than the stale design note

---

## Parity gaps — must all close before `map/legacy-retirement`

- [ ] **G1 — Vehicle location type.** Legacy drew a `vehicle` icon. Source: `OVT_VehicleManagerComponent` (already cached as the unused `m_Vehicles`). **Blocked on BUG-136** (`map/core` D6) — vehicles move and markers currently never refresh mid-open.
- [ ] **G2 — Job waypoint marker.** Legacy drew a `waypoint` icon from `OVT_JobManagerComponent.m_vCurrentWaypoint` (`OVT_MapIcons.c:779-787`). Decide: an Overthrow marker, or enable vanilla's tasks (`m_bShowTasks`).
  - [ ] Do **not** inherit the legacy design flaw where `OVT_RecruitsContext.ShowOnMap` (`:479-491`) writes the recruit position into the same single `m_vCurrentWaypoint` slot, clobbering an active job's waypoint
- [ ] **G3 — POI registry equivalent.** Legacy `OVT_MapIcons.RegisterPOI(uiInfo, origin, mustOwnBase)`, fed by `OVT_MainMenuContextOverrideComponent.EOnFrame` (`:54`) on garages, vehicle maintenance ramps and military garages. **Hard dependency:** deleting the legacy file breaks that static call site.
- [ ] **G4 — Bus stops as an Overthrow marker component.** Replace `OVT_TownManagerComponent.GetNearestBusStop` (`:881`) descriptor proximity queries with a component attachable to any entity, plus a registry and a location type.
  - [ ] Ensure shipped world bus stops gain the component (or document the migration path)
  - [ ] State what happens to a save made before the migration
- [ ] **Design G3 and G4 together** — one "entity-attached map marker component" may serve both, and could also let `Port`/`GunDealer` stop querying the economy manager for static world placements

---

## Info panels for the seven types with none

`UpdateInfoPanel` no-ops when `m_InfoLayout` is empty, so these currently show only the shell header.

- [ ] `FOB` — garrison, ownership, upgrade state
- [ ] `Camp` — owner, private/public, garrison
- [ ] `House` — ownership/rent state, price
- [ ] `Warehouse` — ownership/rent state, contents summary
- [ ] `Shop` — shop type, stock summary
- [ ] `Port` — import/export function
- [ ] `GunDealer` — trade info

---

## Verification (NOT STARTED)

- [x] ✅ Regenerate localization exports in Workbench — done 2026-08-10, all 14 map string ids exported
- [ ] Single-player: confirm all ten types appear at correct positions with correct icons
- [ ] Zoom sweep per type — confirm each `m_fVisibilityZoom` / `m_fShowNameZoom` value is sensible on a full map
- [ ] Confirm faction-coloured types read correctly for **both** factions
- [ ] **MP/JIP — per-player ownership types are the main risk:** verify `House`, `Warehouse`, `Camp` (and future `Vehicle`) resolve `owner`/`renter` correctly on a JIP client, and that one player cannot see another's private ownership
- [ ] Confirm icon legibility at minimum zoom with a fully-populated campaign
- [ ] Gamepad/console: all types selectable, panels readable

---

## Technical debt (after parity)

- [ ] **T1** Unify the three manager-access idioms — inherited cache (5 types) vs own shadowing member + lazy resolve (`Base`, `RadioTower`) vs local `OVT_Global.GetEconomy()` (`Shop`, `Port`, `GunDealer`)
- [ ] **T2** Remove or use `m_Vehicles` — cached on every type, used by none (resolved by G1)
- [ ] **T3** Introduce shared constants for data keys; factor the duplicated `owner`/`renter`/`isOwned`/`isRented` (House/Warehouse) and `owner`/`persistentId`/`garrisonCount` (FOB/Camp) key sets
- [ ] **T5** Consider extracting `Town`'s inline modifier-chip construction (307 lines, 3× the next largest type)
- [ ] Prefer the `OVT_ShopTypeInfo` nested-config pattern over code-side attributes for any new intra-type variation

---

## Future Enhancements

See `implementation.md` and the epic's stretch features (`map/territory-overlay`, `map/map-layers`, `map/shared-markers`).

---

*This task file was created retrospectively. Add new tasks here when working on enhancements.*
