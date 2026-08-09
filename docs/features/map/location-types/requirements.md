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

### Shop info panel — relative price indicator

The `Shop` info panel must tell a player, at a glance, **what is worth buying here** — without printing prices.

- Show which of a shop's stocked items are priced **high** and which are priced **low**, using a caret icon set: **1, 2 or 3 carets up** for progressively more expensive, **1, 2 or 3 carets down** for progressively cheaper, and a neutral state for "about normal".
- **Never display actual prices or currency amounts** on the map panel. The indicator is a teaser; the shop menu remains the only place with real numbers. This is deliberate — it keeps the map readable and preserves a reason to visit the shop UI.
- The signal must be a **relative deviation**, not an absolute price ranking. Ranking a shop's items by raw price would only restate that a rifle costs more than a bandage, which tells the player nothing. What is useful is *"this shop is charging unusually much or little for this item."*
- **Grounding — how price actually varies by location.** `OVT_EconomyManagerComponent.GetSellPriceAtOffset` (`:553-576`) computes:
  `price = base + (1 − stock/maxStock) × base × 0.1 + base × distanceToPort × 0.0001`
  Two independent terms, and the distinction drives the whole design:
  - **Scarcity** — per item, varies within a shop, capped at **+10%**.
  - **Remoteness** — `distanceToPort`, **identical for every item at a given shop**, and unbounded in practice (5 km from a port ≈ +50%).
  Because remoteness is a shop-wide constant, per-item carets measured against base price would be dominated by one uniform offset — every item at a remote shop would show the same "up" reading. **Recommended split (settle during `/plan-feature`):** a single **shop-level** indicator for overall dearness (the remoteness term), plus **per-item carets measured against that shop's own norm** so the carets genuinely spread and mean "bargain here / rip-off here".
- **Vehicles are exempt from location pricing** — `GetSellPriceAtOffset` returns the flat base price for anything in `m_aAllVehicles` (`:561`). A vehicle shop would therefore show a uniformly neutral indicator. Either suppress the section for vehicle shops or state why it is flat; do not ship something that looks broken.
- Carets may be computed from **sell price**: the buy-side additions — `m_fShopProfitMargin` and the player's `priceMultiplier` (`GetBuyPrice`, `:582-597`) — are uniform multipliers that do not change relative ranking.
- **No new replication is required.** Stock is already client-readable: `OVT_ShopComponent` replicates its inventory through `RplSave`/`RplLoad` and a broadcast `RpcDo_SetInventory` (`:73-122`), and `GetTownStock` aggregates across a town's shops by `RplId` (`:601-611`). The indicator must be computed **client-side from replicated state** like every other part of the map.
- The caret icon set must be **created as an art asset** (Workbench) and must remain legible at panel size and distinguishable at a glance — up vs down must not rely on colour alone.
- The indicator must be correct on a **JIP client** and must not imply precision the data does not support (the scarcity term is a ±10% band; do not present three carets as though it were a large number).

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
