# Map Location Types — Implementation Plan

**Status:** ✅ **Built and verified** — Phases 1–6 built and gated 2026-08-10; **Phase 7 runtime verification discharged 2026-08-11, all green including MP.** One documentation task (7b, filing the deferred findings) remains.
**Epic:** map (feature 2 of 7)
**Started:** 2026-08-10
**Completed:** 2026-08-11 (build 2026-08-10; verification gate 2026-08-11)
**Last Updated:** 2026-08-11 (build gates: compile exit 0 / 5956 files, Fast 43, All 78 — historical counts, re-baseline by running the scripts)

> This file replaces the retrospective discovery document produced by `/discover-feature`. The
> discovered architecture is preserved verbatim-in-substance under **Current State / Current
> Architecture**; everything after it is forward-looking. All `file:line` citations are load-bearing —
> keep them when editing.

---

## 1. Executive Summary

`map/location-types` is the **content** half of Overthrow's map: `OVT_MapLocationType` subclasses that
turn replicated campaign state into markers, each declared as an entry in `Configs/Map/OverthrowMap.conf`.
Ten types ship on the `new-map` branch. They are **not at parity** with the legacy `OVT_MapIcons` layer
they replace, and until they are, `map/legacy-retirement` cannot delete a single line of the old map.

This feature closes that gap. It:

- adds four new location types — **Vehicle** (G1), **Waypoint** (G2), **POI** (G3) and **Bus Stop** (G4);
- introduces **one generic entity-attached marker component and one registry** that serves both POIs and
  bus stops, replacing the legacy static `OVT_MapIcons.RegisterPOI` and the vanilla `MDT_BUSSTOP`
  proximity query;
- gives the **seven header-only types real info panels** via a shared, data-driven row mechanism plus two
  bespoke layouts;
- adds the **shop relative-price indicator** — per-item scarcity carets and a shop-level remoteness badge,
  computed entirely client-side from already-replicated state, showing **no prices at all**;
- fixes a **privacy regression** discovered while planning: the current House type shows *every* player's
  property to *every* player, where the legacy layer showed only your own (N1);
- folds tech debt T1/T2/T3 in opportunistically as each type is touched.

Everything here is **read-only client presentation**. No new client→server RPC is required, and nothing
in this feature touches `OVT_PlayerCommsComponent`. The one deliberate write into a non-map system — the
epic's stated single exception — is the bus-stop marker component and its registry.

Completion is gated on a two-client MP/JIP play-test and a gamepad pass, because the epic has no separate
verification feature.

---

## 2. Goals

### Primary

1. **Parity with `OVT_MapIcons`.** Every icon the legacy layer drew (`camp`, `gundealer`, `house`, `port`,
   `tower`, `vehicle`, `warehouse`, `waypoint`) plus its POI registry exists in the new system, and the
   `RegisterPOI` compile-level dependency is gone. This is the completion bar `requirements.md:23` sets.
2. **Bus stops as first-class Overthrow locations**, discovered through a registry rather than a vanilla
   world descriptor query — unblocking `map/fast-travel` F5.
3. **Type-appropriate info panels for all types**, with the three existing bespoke panels unchanged.
4. **A shop price indicator** that tells a player what is worth buying without printing a number.
5. **Correct in multiplayer and on JIP**, and **operable on gamepad/console**.

### Secondary

6. No leakage of another player's private ownership (N1).
7. Tech debt T1/T2/T3 reduced where a type is being touched anyway — not as a separate refactor phase.
8. The config-driven property preserved: a new type stays "a `.c` subclass + a conf entry + optionally a
   layout", with **one** deliberate, additive extension to `map/core`'s contract (K5), flagged as such.

### Explicit non-goals

- **Live marker refresh while the map is open.** Legacy sampled vehicle positions once at build time
  (`OVT_MapIcons.c:709-731`) and its `Update()` only re-projected the stored world position. **Sampling at
  map open IS parity.** `map/core`'s D6 does not block this feature (correction to the previous doc).
- Fixing `map/core` D1 (`IconLayout` name mismatch), D2/D3 (panel close) or D7 (`OnLocationClicked`
  unreachable). Those are `map/core`'s — and all three were fixed there on 2026-08-10 as BUG-133,
  BUG-134 and BUG-137. `OnLocationClicked` now runs, so the "do not override" rule is lifted.
- Changing what towns, bases, FOBs, shops, houses or vehicles store or replicate. Defects found there are
  filed as bugs against those features (N5, N6).
- The travel verbs themselves (`map/fast-travel`), and deleting anything legacy (`map/legacy-retirement`).
- New location categories the legacy system never drew.

---

## 3. Current State / Current Architecture

*(Discovered by reading the merged `new-map` branch. Nothing below has been observed at runtime — the
branch has not been play-tested since 2025-08-02. Corrections made during planning are marked.)*

### 3.1 The ten shipped types

| Type | Lines | Source of truth | Overrides | Info panel | Fast travel |
|---|---:|---|---|---|---|
| `Town` | 307 | `m_TownManager` | `PopulateLocations`, `GetDisplayName`, `GetDisplayNameForLocation`, `GetIconColor`, `CanFastTravel`, `OnSetupLocationInfo`, **`OnSetupIconWidget`** | ✅ `OVT_MapInfoTown.layout` | ❌ explicitly refused (`:303-307`) |
| `Base` | 171 | own `m_OccupyingFactionManager` | `PopulateLocations`, `GetIconColor`, `CanFastTravel`, `OnSetupLocationInfo` | ✅ `OVT_MapInfoBase.layout` | ✅ resistance-held only |
| `RadioTower` | 105 | own `m_OccupyingFactionManager` | `PopulateLocations`, `GetIconColor`, `OnSetupLocationInfo` | ✅ `OVT_MapInfoRadioTower.layout` | ❌ |
| `House` | 136 | `m_RealEstate` | `PopulateLocations`, `GetIconColor`, `GetLocationDescription`, `CanFastTravel` | ❌ header only | ✅ owner/renter only |
| `Warehouse` | 105 | `m_RealEstate` | `PopulateLocations`, `GetIconColor`, `GetLocationDescription` | ❌ header only | ❌ |
| `Shop` | 96 | local `OVT_Global.GetEconomy()` | `PopulateLocations`, `GetLocationName`, `GetIconName` | ❌ header only | ❌ |
| `FOB` | 72 | `m_Resistance` | `PopulateLocations`, `GetIconName`, `CanFastTravel` | ❌ header only | ✅ global rules only |
| `Camp` | 63 | `m_Resistance` | `PopulateLocations`, `CanFastTravel` | ❌ header only | ✅ own/public only |
| `Port` | 34 | local `OVT_Global.GetEconomy()` | `PopulateLocations` | ❌ header only | ❌ |
| `GunDealer` | 34 | local `OVT_Global.GetEconomy()` | `PopulateLocations` | ❌ header only | ❌ |

Only **Base, RadioTower, Town** override `OnSetupLocationInfo`, exactly matching the three with
`m_InfoLayout` configured. This is **not** a "generic fallback renderer": `UpdateInfoPanel` returns early
when `m_InfoLayout` is empty (`OVT_MapLocationType.c:126-127`), so the other seven contribute *nothing* to
`ContentSlot` and show only the shell header (name, type, distance, owner, fast-travel button).

**No type overrides `OnLocationClicked`.** That was fortunate while `map/core` D7 stood — the virtual was
unreachable. BUG-137 wired it (the container calls it from `OnMapSelection` when a click pins a location),
so overriding it is now supported; the default body is empty.

### 3.2 Data-key matrix

Keys each type writes into `OVT_MapLocationData`'s typed maps. **Unregistered strings** — a typo silently
yields the default (T3).

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

`House`/`Warehouse` share an ownership key shape; `FOB`/`Camp` share `owner`/`persistentId`/`garrisonCount`.
Neither pairing is factored into a helper. The shell's `Owner` row is special-cased on the `"owner"` key
(`OVT_OverthrowMapUI.c:419-445`) — a location that stores only `renter` shows no owner line.

### 3.3 Fast travel: one gate per type, one shared service

- `Base` — refuses if `isOccupying` (`#OVT-CannotFastTravelEnemyBase`), else `OVT_FastTravelService.CanGlobalFastTravel` (`:153-171`)
- `Camp` — refuses if `isPrivate` and `owner != playerID`, else the service (`:43-63`)
- `House` — refuses unless player is `owner` or `renter`, else the service (`:119-136`)
- `FOB` — no local rule; straight to the service (`:61-72`)
- `Town` — unconditional refusal (`:303-307`)

Per-type policy on top of one global rule set — the right shape. The global half belongs to `map/fast-travel`.

### 3.4 Config-driven variation *within* a type

`Shop` demonstrates the sub-pattern to reuse: a nested config array `m_aShopTypes` of `OVT_ShopTypeInfo`
(`TypeInfo/OVT_ShopTypeInfo.c`), each mapping an `OVT_ShopType` value to a display name and icon name;
`GetLocationName`/`GetIconName` look up and fall back (`Shop.c:56-96`). One class serves five shop kinds,
authored in Workbench. `Town` does the same in code (`m_sVillageIconName`/`m_sTownIconName`/`m_sCityIconName`).
**Prefer the Shop pattern for new variation.**

### 3.5 Parity gaps — what the legacy layer draws that this does not

Established from `Scripts/Game/UI/Map/OVT_MapIcons.c`. Its icon names are the definitive checklist:
`camp`, `gundealer`, `house`, `port`, `tower`, `vehicle`, `warehouse`, `waypoint`.

**G1 — Owned vehicles.** Legacy drew a `vehicle` icon from `vehicles.GetOwned(persId)`
(`OVT_MapIcons.c:710`), rotated by the vehicle's yaw (`ent.GetYawPitchRoll()[0]`). No `OVT_MapLocationVehicle`
exists. Source of truth is `OVT_VehicleManagerComponent : OVT_RplOwnerManagerComponent`;
`GetOwned(string playerId)` returns `set<EntityID>` (`OVT_RplOwnerManagerComponent.c:185`), and `m_mOwned`
is JIP-replicated (`RplSave` `:221`).
**CORRECTED:** this is **not** blocked on `map/core` D6. Legacy inserted vehicle positions into `m_Centers`
once at build time (`:709-731`) and its `Update()` only re-projected them. Position-at-map-open *is* parity.

**G2 — Job waypoints.** Legacy drew a `waypoint` icon from `OVT_JobManagerComponent.m_vCurrentWaypoint`
(`OVT_MapIcons.c:779-787`). The new container sets `m_bShowTasks 0`, so nothing draws it.
`OVT_RecruitsContext.ShowOnMap` (`:489`) writes a recruit's position into that **same single field** that
`OVT_JobsContext` (`:109`) uses — so "show job on map" and "show recruit on map" clobber each other. Both
writes are client-local (plain member on a game-mode component, written from UI contexts). Dropping G2
would silently kill "show recruit on map" as well.

**G3 — The POI registry.** Legacy exposed a **static** `OVT_MapIcons.RegisterPOI(uiInfo, origin, mustOwnBase)`
(`:46-54`), fed from `OVT_MainMenuContextOverrideComponent.EOnFrame` (`:54`) when `m_bShowOnMap` is set,
gated on being within 220 m of a base when `m_bMustOwnBase`. Display-time it additionally required the
nearest base to be **resistance-held** (`OVT_MapIcons.c:495-498`). At zoom < 1 it swapped to a generic
12×12 icon; at zoom ≥ 1 it used the POI's own icon at 44×44 (`:320-334`).
Ships on three live prefabs:

| Prefab | `m_UiInfo` | `m_bMustOwnBase` |
|---|---|---|
| `Prefabs/Structures/Industrial/Garages/Garage_E_02/Garage_E_02.et:7-16` | `#OVT-Procurement`, `overthrow_mapicons`/`vehicles` | **1** |
| `Prefabs/Structures/Military/Houses/GarageMilitary_E_01/GarageMilitary_E_01_base.et:4-12` | `#OVT-Procurement`, `overthrow_mapicons`/`vehicles` | 0 |
| `Prefabs/Structures/Military/FOB/OVT_VehicleMaintenanceRamp.et:7-14` | `#OVT-Build_VehicleMaintenanceRamp`, `icons_wrapperUI-32`/`repair` | 0 |

`m_bShowOnMap` defaults to `1` and none of the three override it, so all three register.
**The maintenance ramp is a player-buildable `PLACEABLE`** (`:15-21`) — POIs are therefore not only
world-static; the registry must accept runtime-spawned markers too.
Deleting `OVT_MapIcons.c` breaks this static call site: a **compile-level** dependency, not just visual.

**G4 — Bus stops.** Not a legacy *map icon* at all: bus stops are vanilla `EMapDescriptorType.MDT_BUSSTOP`
descriptors found by proximity query in `OVT_TownManagerComponent.GetNearestBusStop` (`:881`, 15 m sphere;
filter `FindBusStop` at `:1197-1203`). Sole consumer: `OVT_MapContext.c:453`.

### 3.6 Precedents already in the codebase

- **Marker component + manager lookup (the port pattern).** `OVT_PortControllerComponent`
  (`Scripts/Game/Controllers/OVT_PortController.c:5`) is a **completely empty** `OVT_Component` subclass.
  `OVT_EconomyManagerComponent.InitializePorts()` (`:1649-1656`) finds them with
  `QueryEntitiesBySphere("0 0 0", 99999999, CheckPortInit, FilterPortEntities)` and `FilterPortEntities`
  (`:1857-1864`) just tests for the component.
- **That scan is client-safe and needs no replication.** It runs from `AfterInit()` (`:1288-1294`)
  **before** the `if(!Replication.IsServer()) return;` guard, so it executes on **every machine**, and
  `m_aAllPorts` is deliberately *not* in the economy manager's `RplSave`/`RplLoad`. Static world entities
  are identical everywhere. **This is the precedent for the new registry.**
- **Self-registering component (the anti-pattern).** `OVT_MainMenuContextOverrideComponent` registers from
  `EOnFrame`. A frame event may never fire for a streamed-out static — which is exactly why the world scan
  is preferred.
- **Row-widget + handler.** `OVT_TownModifierWidgetHandler : SCR_ScriptedWidgetComponent`
  (`Scripts/Game/UI/Components/OVT_TownModifierWidgetHandler.c:3`) caches `ModifierText`/`ModifierBackground`
  in `HandlerAttached` (`:30-44`) and exposes `Init(...)` (`:20`); the layout attaches it in a
  `components { }` block (`OVT_TownModifierWidget.layout:8-13`); `OVT_MapLocationTown.SetupModifierContainer`
  (`:243-300`) clears the container, `CreateWidgets`, `FindHandler`, `Init`. **This is the template for the
  shared info row.**
- **No shared label/value row layout exists.** The pattern is inlined twice inside `OVT_MapInfoTown.layout`
  (`SupportLabel`/`Support` at `:99-117`, `StabilityLabel`/`Stability` at `:176-194`).

### 3.7 Icons already present

`UI/Imagesets/overthrow_mapicons.imageset` contains: `camp, city, clothes, electronics, fob, fob_priority,
gundealer, house, pharmacy, port, shop, tower, town, vehicle, vehicles, village, warehouse, waypoint`.
**CORRECTED:** `vehicle` and `waypoint` already exist — **G1 and G2 need no new art.** G3 POI icons come
from each prefab's already-authored `SCR_UIInfo`. Only **bus stop** and the **caret set** are new art.

### 3.8 Technical debt (carried forward)

- **T1 — Three manager-access idioms coexist.** The base class caches seven managers in `Init()`
  (`OVT_MapLocationType.c:72-78`); five types use them; `Base`/`RadioTower` declare their **own**
  `m_OccupyingFactionManager` and lazily resolve it (`Base.c:15,21`; `RadioTower.c:9,15`), shadowing the
  inherited field; `Shop`/`Port`/`GunDealer` call `OVT_Global.GetEconomy()` per call.
- **T2 — `m_Vehicles` is cached on every type and used by none** — a vestige of the missing Vehicle type.
  **Resolved by G1.**
- **T3 — Unregistered string keys**, duplicated by convention between House/Warehouse and FOB/Camp.
- **T4 — Seven types have no info panel.** Closed by this feature.
- **T5 — `Town` at 307 lines**, three times the next largest; modifier chips built inline. Candidate for
  extraction only if it grows.

### 3.9 New findings from planning (2026-08-10)

These change the shape of the work and are not in the previous document.

- **N1 — 🔴 House markers leak every player's property.** Legacy drew only the **local** player's owned
  (`realEstate.GetOwned(persId)`, `OVT_MapIcons.c:472`) and rented (`:543`) houses.
  `OVT_MapLocationHouse.PopulateLocations` iterates `m_RealEstate.m_mOwned` and `m_mRented` for **all**
  players (`:25`, `:59`). This violates `requirements.md:21` and is a regression against the system being
  replaced. **In scope: fix here.**
- **N2 — Warehouses are deliberately public.** Legacy's block is literally commented
  `//Public Owned Warehouses` and iterates `realEstate.m_mOwners` (`OVT_MapIcons.c:573+`), exactly as
  `OVT_MapLocationWarehouse` does. **No change** — and this asymmetry with houses is intentional, not a bug.
- **N3 — Legacy distinguished the player's home.** `realEstate.IsHome(persId, id)` set the icon's range to
  the always-visible value (`OVT_MapIcons.c:525-528`), and rented houses were drawn `Color.Gray25` (`:562`).
  The new House type colours owned/rented but has no home distinction. Cheap parity item.
  (`OVT_RealEstateManagerComponent.IsHome` at `:595`, `GetHome` at `:750`.)
- **N4 — The per-location `visibilityZoom` data key is never read.**
  `OVT_MapLocationElement.SetVisible` (`:498`) and `ShouldUseSmallIcon` (`:315`) both use the **type-level**
  `m_LocationType.GetVisibilityZoom()`. FOB writes `0` for priority FOBs (`OVT_MapLocationFOB.c:39`) and
  House writes the type value (`:45`,`:79`) — both inert. **Priority FOBs are therefore not always
  visible.** This is a `map/core` contract gap; file it as **core D8** and see K9.
- **N5 — FOB/Camp `garrison` is not replicated.** `OVT_ResistanceFactionManager.RplSave`/`RplLoad`
  (`:1231-1300`) carries `persistentId, name, location, owner, isPriority|isPrivate` only; the broadcast
  `RpcDo_RegisterFOB`/`RpcDo_RegisterCamp` (`:1135`, `:1102`) carry no garrison either. So
  `fob.garrison.Count()` is **0 on every remote client**, and the existing `garrisonCount` key is
  meaningless off a listen-server host. Panels must not lead with garrison. File against `resistance/fob`.
  There is also **no FOB upgrade/build state** to show — `OVT_FOBData` is five fields (`:22-37`).
- **N6 — `GetTownStock` can null-deref on a client.** `OVT_EconomyManagerComponent.c:601-611` calls
  `GetShopByRplId(shopId)` — which returns null when `Replication.FindItem` misses (`:435-440`) — then
  `shop.GetStock(id)` with no guard. The shop indicator must **not** call it; compute town stock with a
  null-safe local loop and file the missing guard against `economy/shops`.
- **N7 — `DistanceToNearestPort` returns −1 when no ports are registered** (`:846-858`). The shop badge must
  treat a negative distance as "unknown" and hide, not render a tiny discount.
- **N8 — Item display-name resolution is expensive.** `OVT_ShopContext.ResolveDisplayName` (`:855-883`)
  documents that UIInfo resolution "loads and scans a prefab container, which is far too expensive to repeat
  per row per refresh" and caches per id. Cap the number of shop rows and cache resolved names.
- **N9 — Overthrow already ships a same-GUID delta of the bus-stop prefab.**
  `Prefabs/Structures/Signs/Traffic/SignBusStop_01.et` overrides vanilla's (which carries
  `SCR_MapDescriptorComponent { MainType "Bus Stop" }`) and currently adds only `EnableDamage 0` plus an
  `ActionsManagerComponent` with `OVT_CatchBusAction`. **Vanilla has exactly one bus-stop prefab**, so the
  migration touches one file and covers every world-placed stop in every world, with no world editing.
  (`Prefabs/Structures/Signs/Signs_Base.et` is also already an Overthrow delta.)
- **N10 — `OVT_MapLocationBase.GetIconNameForBase` (`:139-150`) has no callers.** Base never overrides
  `GetIconName`/`OnSetupIconWidget`, and its config sets both icon names to `Character_Bcg`. Dead code;
  delete opportunistically if Base is touched, otherwise leave.
- **N11 — Warehouse contents *are* client-readable.** `OVT_WarehouseData.inventory` (ResourceName→qty) is
  JIP-replicated (`OVT_RealEstateManagerComponent.RplSave` `:771`, `:786-792`), with
  `GetWarehouseInventory(OVT_WarehouseData)` (`:473`), `GetNearestWarehouse(vector,int)` (`:454`), the public
  `m_aWarehouses` (`:30`) and a `m_OnWarehouseInventoryChanged` invoker (`:42`). A contents summary is
  feasible.
- **N13 — A gun dealer's stock is mostly identical everywhere; only FOUR ids are per-dealer picks.**
  Generation is **not** in the economy manager — it is `OVT_TownController.SpawnGunDealer()` (`:233`, called
  from `ActivateTown()` `:98-103`, skipped for `OVT_TownSize.VILLAGE`). The selector is
  `OVT_ShopInventoryItem.m_bSingleRandomItem` (`OVT_EconomyManagerComponent.c:54-55`): when set, one entry is
  rolled from the matching catalog (`OVT_TownController.c:303-325`); when unset, **every** matching entry is
  added (`:326-336`).
  Decoding `Configs/System/GunDealerConfig.conf`, exactly four rows are single picks:
  **RIFLE** (rows 21-25), **ROCKET_LAUNCHER** (34-39), **SNIPER_RIFLE** (45-50), **MACHINE_GUN** (55-60).
  Everything else — *all* pistols (incl. occupying-faction ones), all rifle/launcher/MG ammunition, all
  attachments, throwables, explosives, and a fixed weed entry (`:287-293`) — is **the same at every dealer**.
  ⚠️ **The user's premise "one of each gun type" is right for four heavy-weapon categories and wrong for
  pistols.** Town size gates only whether a dealer *exists* (`:102`) and quantity via `GetTownMaxStock`; it
  never changes *which* items are picked. Categorisation is vanilla `SCR_EArsenalItemType`/`Mode`, and note
  the type attribute defaults to `2` = RIFLE (`OVT_EconomyManagerComponent.c:26-27`) while
  `FindInventoryItems` never wildcards type (`:1511`) — so a config row specifying only a mode is implicitly
  a RIFLE row.

- **N14 — 🔴 The four signature weapons RE-ROLL on every campaign load.**
  `s_AIRandomGenerator.RandInt(0, entries.Count())` (`OVT_TownController.c:310`) is the global **unseeded**
  generator — there is no per-town or per-dealer seed anywhere. Nothing persists the result: the persistence
  conf's `ComponentSerializers` list (`Configs/Systems/Persistence/Overthrow.conf:23-62`) has no shop entry,
  `OVT_EconomyManagerSerializer` saves only resistance money and tax (`:41-56`), and the dealer entity is
  explicitly `UntrackTransient`-ed with the comment "respawned by this controller every session"
  (`OVT_TownController.c:273-276`). Only the **position** survives, via `OVT_TownData.gunDealerPosition`.
  **Within** a session the roll is stable — restock tops up existing ids and never re-rolls
  (`OVT_EconomyManagerComponent.c:344-368`).
  **Design consequence:** the panel is accurate when read and must not imply durability. Do not write copy
  like "always stocks" — this dealer stocks it *this session*.

- **N15 — Sold-out ids remain in `m_aInventory` at stock 0** (deliberate, so per-town heavy weapons restock
  rather than re-roll — `OVT_EconomyManagerComponent.c:344-347`). Any panel iterating a shop's inventory
  **must filter `stock > 0`** or it will list items the dealer does not currently have.

- **N16 — `OVT_ShopCategory` cannot distinguish weapon kinds.** `OVT_ShopCategoryHelper.GetCategory`
  (`Scripts/Game/Data/OVT_ShopCategory.c:38-90`) collapses rifle/pistol/sniper/MG/launcher into a single
  `WEAPONS` bucket, and checks **mode before type** (a rifle magazine is type RIFLE / mode AMMUNITION). Only
  vanilla `SCR_EArsenalItemType` separates the kinds, and **no id → arsenal-type cache is exposed** — the
  economy manager caches only the coarser id → `OVT_ShopCategory` (`GetItemCategory`, `:1789`). The gun-dealer
  panel must resolve arsenal type itself over the small `WEAPONS`-with-stock set and cache it.

- **N17 — 🐛 `GunDealerConfig.conf:51-54` duplicates the RIFLE+AMMUNITION rule from `:26-29`.** By position and
  by the pattern of every other weapon pair it was plainly meant to be **SNIPER_RIFLE + AMMUNITION**. Effect:
  rifle magazines are added twice, and **sniper ammunition may never be stocked at all** — a dealer can roll a
  sniper rifle the player then cannot feed. File against `economy/shops`; **do not fix it in this feature**
  (it is a config/economy defect, not a map defect), but the panel must tolerate it.

- **N18 — 🐛 `RegisterGunDealer` has no broadcast RPC.** `OVT_EconomyManagerComponent.c:1257-1263` only inserts
  into `m_aGunDealers`; the list reaches clients solely through the economy manager's JIP `RplSave`
  (`:1906-1913`). A dealer registered **after** a client has joined therefore never appears on that client's
  map. File against `economy/shops`. Also note `m_aInventory` exists on a client only while the dealer entity
  is streamed in — legacy maintained an `m_aFailedGunDealers` retry list for exactly this
  (`OVT_MapIcons.c:357-380`), and the current type already guards it (`OVT_MapLocationGunDealer.c:19-25`).

- **N12 — House prices are pure client-side computations.** `GetBuyPrice(IEntity)` (`:712`) and
  `GetRentPrice(IEntity)` (`:731`) derive from `OVT_RealEstateConfig` plus replicated town population and
  stability. Safe to call from a panel.

### 3.10 Save-game impact of the bus-stop migration

`requirements.md:15` asks what happens to a save made before the migration. **Answer: nothing.**
Bus stops are **world statics discovered at runtime and never persisted** — the current implementation is a
live `QueryEntitiesBySphere` (`OVT_TownManagerComponent.c:884`), and the new implementation is a live world
scan plus component self-registration. No bus-stop state is written to or read from a save, so no migration
path and no save-format change is required. **This must be verified, not assumed** — see DoD V-7.

---

## 4. Architecture Overview

### 4.1 Component hierarchy

```
OVT_OverthrowGameMode (prefab: Prefabs/GameMode/OVT_OverthrowGameMode.et)
└── OVT_MapMarkerManagerComponent          NEW — registry, runs on EVERY machine, no replication
        m_aMarkers : array<OVT_MapMarkerComponent>          (all categories, one flat list)
        GetMarkers(OVT_MapMarkerCategory) -> array<...>
        GetNearestMarker(vector pos, OVT_MapMarkerCategory, float maxDist) -> OVT_MapMarkerComponent
        RegisterMarker(c) / UnregisterMarker(c)             idempotent

World / prefab entities
└── OVT_MapMarkerComponent : OVT_Component  NEW — generic, attachable to ANY entity
        [Attribute] OVT_MapMarkerCategory m_eCategory       BUS_STOP | POI
        [Attribute] ref SCR_UIInfo m_UiInfo                 name + imageset + icon (POI)
        [Attribute] bool m_bMustOwnBase                     preserves legacy POI gating
        OnPostInit -> CallLater(Register, 0)
        OnDelete   -> UnregisterMarker

OVT_MapLocationType (config-instantiated, Configs/Map/OverthrowMap.conf)
├── existing: Town, Base, RadioTower, House, Warehouse, Shop, FOB, Camp, Port, GunDealer
├── OVT_MapLocationBusStop   NEW — registry query, category BUS_STOP
├── OVT_MapLocationPOI       NEW — registry query, category POI, mustOwnBase filter
├── OVT_MapLocationVehicle   NEW — m_Vehicles.GetOwned(localPlayer)
└── OVT_MapLocationWaypoint  NEW — job waypoint + recruit waypoint (two slots)
```

`OVT_Global` gains `static OVT_MapMarkerManagerComponent GetMapMarkers()` alongside the existing accessors
(`Scripts/Game/Global/OVT_Global.c:141-226`), resolving through a `GetInstance()` singleton exactly like
every other manager.

### 4.2 Discovery: world scan + self-registration, both idempotent

Two mechanisms, because two genuinely different cases exist:

| Case | Mechanism | Why |
|---|---|---|
| World-placed statics (bus-stop signs, garages) | **One world scan** at manager init: `QueryEntitiesBySphere("0 0 0", 99999999, Check, Filter, EQueryEntitiesFlags.STATIC)` | Proven by `InitializePorts` (`:1649-1656`); reliably finds streamed-out statics where a frame event would not |
| Runtime-spawned placeables (`OVT_VehicleMaintenanceRamp`) | **Self-registration** from `OnPostInit` via `CallLater(Register, 0)` | The world scan has already run by the time a player builds a ramp |

`RegisterMarker` is set-based and idempotent, so running both is harmless. `OnDelete` unregisters (a
destroyed maintenance ramp must lose its marker). The scan runs on **every machine, before any server
guard** — these are static, identical world entities, so **no replication is added**.

### 4.3 Two thin location types over one registry

```
OVT_MapLocationBusStop : OVT_MapLocationType
    PopulateLocations -> GetMapMarkers().GetMarkers(BUS_STOP), one record per marker
    icon: "busstop" (new art; fallback "port" until it lands — K8)

OVT_MapLocationPOI : OVT_MapLocationType
    PopulateLocations -> GetMarkers(POI); per marker:
        if m_bMustOwnBase:
            base = GetOccupyingFaction().GetNearestBase(pos)
            skip unless base && dist(base.location,pos) <= 220 && !base.IsOccupyingFaction()
        record name/icon from m_UiInfo (SCR_UIInfo.GetName(), GetIconPath()/imageset+icon)
    m_fVisibilityZoom ~1.0 (legacy showed the real icon only at zoom >= 1)
```

The 220 m rule and the resistance-held rule are the **two halves of legacy's gating**
(`OVT_MainMenuContextOverrideComponent.c:45-53` at registration; `OVT_MapIcons.c:495-498` at display). Both
are preserved, both evaluated at populate time — cheap, since base locations are static and populate runs
once per map open. **Neither goes in `CanFastTravel`/`ShouldShowLocation`** (hot path).

`GetNearestBusStop` moves from a world query to a registry query. `OVT_TownManagerComponent.GetNearestBusStop`
(`:881`) and its `FindBusStop` filter (`:1197-1203`) are **deleted**, and the single caller
(`OVT_MapContext.c:453`) is re-pointed at
`OVT_Global.GetMapMarkers().GetNearestMarker(pos, BUS_STOP, 15)`. This satisfies `requirements.md:47`
("coordinate so the town manager is not left with a dead method") and is what `map/fast-travel` F5 will
build on.

### 4.4 Vehicle and Waypoint types

```
OVT_MapLocationVehicle : OVT_MapLocationType
    PopulateLocations:
        persId = GetCurrentPlayerID(); if empty -> return
        foreach EntityID in m_Vehicles.GetOwned(persId):     // local player only — matches legacy :710
            ent = world.FindEntityByID(id); skip if null
            record(pos=ent.GetOrigin(), name=vehicle UIInfo name, m_EntityID=id)
    OnSetupIconWidget: image.SetRotation(ent.GetYawPitchRoll()[0])    // legacy parity, :727
    icon "vehicle" (exists). Uses the inherited m_Vehicles cache -> resolves T2.
```

```
OVT_MapLocationWaypoint : OVT_MapLocationType
    PopulateLocations emits UP TO TWO records:
        jobs.m_vCurrentWaypoint     != "0 0 0"  -> record(name "#OVT-Job", colour A)
        jobs.m_vRecruitWaypoint     != "0 0 0"  -> record(name "#OVT-Recruit", colour B)
    icon "waypoint" (exists), m_fVisibilityZoom 0 (legacy always-visible), m_bShowName 1
```

The shared-slot defect is fixed with the **smallest possible additive change** in the other features'
territory: one new field `vector m_vRecruitWaypoint` next to `m_vCurrentWaypoint`
(`OVT_JobManagerComponent.c:64`), and `OVT_RecruitsContext.ShowOnMap:489` writes the new field instead of
the job field. Both fields stay plain, client-local, non-replicated, non-persisted members — which is what
they already are (both writes happen in client UI contexts). No RPC, no save impact.

### 4.5 Info panels: one shared row mechanism, two bespoke layouts

**Contract extension (additive, backwards compatible):**

```
OVT_MapLocationType
  + [Attribute] ResourceName m_SharedInfoLayout      default UI/Layouts/Map/Core/OVT_MapInfoRows.layout
  + protected void BuildInfoRows(OVT_MapLocationData location, Widget rowsContainer)   // new virtual
  + protected void AddInfoRow(Widget rows, string label, string value)
  + protected void AddInfoIconRow(Widget rows, string label, string value, ResourceName imageset, string icon)

  UpdateInfoPanel(location, contentSlot):
      clear contentSlot                                  (unchanged)
      if (!m_InfoLayout.IsEmpty())                       -> today's path: CreateWidgets + OnSetupLocationInfo
      else if (!m_SharedInfoLayout.IsEmpty())            -> CreateWidgets(m_SharedInfoLayout)
                                                            rows = FindAnyWidget("Rows")
                                                            if (rows) BuildInfoRows(location, rows)
      else                                               -> return (today's behaviour)
```

Town, Base and RadioTower keep `m_InfoLayout` and are **untouched**. This is the one deliberate change to
`map/core`'s contract — see K5 for the rationale and cross-feature note.

`OVT_MapInfoRow.layout` + `OVT_MapInfoRowHandler : SCR_ScriptedWidgetComponent` are modelled **exactly** on
`OVT_TownModifierWidget.layout` + `OVT_TownModifierWidgetHandler`: widget names `RowLabel` (TextWidget),
`RowValue` (TextWidget), `RowIcon` (ImageWidget, hidden unless set); handler caches them in
`HandlerAttached` and exposes `Init(label, value, imageset, icon)`.

**Row content per type — only data that actually exists client-side:**

| Type | Panel | Rows |
|---|---|---|
| `FOB` | shared | Priority (Yes/No); Garrison **only when > 0** (N5) |
| `Camp` | shared | Access (Private/Public); Garrison **only when > 0** (N5) |
| `House` | shared | Status (Home / Owned / Rented); Renter name when `renter` set (the shell only shows `owner`); Rent when rented (`GetRentPrice`, N12) |
| `Warehouse` | shared | Owner/Renter status; Contents — top 3 item lines from `GetWarehouseInventory` (N11), or "Empty" |
| `Port` | shared | Static explanatory lines: imports arrive here; goods are cheaper near ports (grounded in `GetSellPriceAtOffset:566` and `IMPORT_MAX_PORT_DISTANCE = 30`, `OVT_PlayerCommsComponent.c:773`) |
| `BusStop` | shared | One line: travel to another stop by bus |
| `POI` | shared | One line from `m_UiInfo.GetDescription()` when set |
| `Vehicle` | shared | Vehicle name (`OVT_Global.GetVehicleUIInfo`) |
| `Waypoint` | shared | Which marker this is (job / recruit) |
| `Shop` | **bespoke** `OVT_MapInfoShop.layout` | shop type + price indicator (§4.6) — composes the same `Rows` container plus a `Badge` and a `ScarcityRows` section |
| `GunDealer` | **bespoke** — reuses `OVT_MapInfoShop.layout` | **NOT the same as Shop — see §4.6b.** The four signature weapons this dealer rolled, each with a price caret. A gun dealer *is* an `OVT_ShopComponent` with `m_ShopType == SHOP_GUNDEALER`, but its useful content is *what it stocks*, not *what is dear* |

Shop composes rather than being fully bespoke: `OVT_MapInfoShop.layout` contains a `Rows` vertical layout
(so `AddInfoRow` works unchanged) plus a `Badge` overlay and a `ScarcityRows` container. The caret maths
lives in one shared helper, `OVT_MapShopPriceIndicator`, called by both Shop and GunDealer.

### 4.6 Shop relative-price indicator

**The model.** Working from `OVT_EconomyManagerComponent.GetSellPriceAtOffset` (`:553-573`) and normalising
by base price:

```
price_i / base_i  =  1  +  0.1 × (1 − townStock_i / townMaxStock_i)  +  0.0001 × distToPort
                            └── per-item, bounded ±10% ──┘             └─ shop-wide, unbounded ─┘
```

**Per-item carets read the scarcity term ONLY.**

```
scarcityPct_i = 100 × 0.1 × (1 − townStock_i / max(1, townMaxStock_i))     // range ≈ [−10, +10]
```

| Band | Display |
|---|---|
| `> +7.5` | ▲▲▲ |
| `+5.0 … +7.5` | ▲▲ |
| `+2.5 … +5.0` | ▲ |
| `−2.5 … +2.5` | neutral (row omitted) |
| `−5.0 … −2.5` | ▼ |
| `−7.5 … −5.0` | ▼▼ |
| `< −7.5` | ▼▼▼ |

Because the remoteness term is *exactly* constant across a shop, excluding it already solves the "every item
reads up at a remote shop" problem that `requirements.md:37` worried about — **no statistical normalisation
against the shop mean is needed**, and avoiding it preserves real information: a genuinely scarce town
honestly reads all-up instead of being flattened to neutral. Stock can overshoot to **2× max**
(`TOWN_STOCK_BUY_CAP_MULTIPLIER`, `:628`), so the term genuinely goes negative — real bargains exist.

**Shop-level badge reads the remoteness term only.**

```
remotenessPct = 100 × 0.0001 × distToPort = 0.01 × distToPort     // metres
```

| Band | Display |
|---|---|
| `< 5` (under 500 m) | neutral — badge hidden |
| `5 … 15` | ▲ |
| `15 … 30` | ▲▲ |
| `> 30` | ▲▲▲ |
| `distToPort < 0` | hidden (N7 — no ports registered) |

**It is a TOWN signal on a shop panel — say so.** `GetTownStock(townId, id)` sums `GetStock(id)` across
**every** shop in `m_mTownShops[townId]` (`:601-611`), so **two shops in the same town produce identical
per-item carets**. The section header must label it as local supply, not this shop's own pricing whim.
Suggested copy: "Local supply" / "Scarce in this town — dearer here" rather than "This shop overcharges".

**Which items are listed.** From the shop's own `m_aInventory` keys with stock > 0 (canonical client loop:
`OVT_ShopContext.c:697-711` — `GetKey(i)` → `IsValidResourceId` → `GetResource`), excluding anything in
`m_aAllVehicles`. Sort by `scarcityPct`; take **up to 3 most negative (best bargains)** and **up to 3 most
positive (worst rip-offs)**, skipping the neutral band, de-duplicating when the shop stocks ≤ 6 items.
**Empty shop, or no item outside the neutral band → one line ("Nothing unusual here" / "No stock") and no
rows.** Six rows is also the cap that keeps N8's per-item prefab load acceptable; names are cached in a
member map on the type, mirroring `OVT_ShopContext.m_mDisplayNames` (`:73`).

**Vehicle shops are suppressed entirely.** `GetSellPriceAtOffset` returns the flat base price when
`m_aAllVehicles.Contains(id)` (`:561`) — it returns *before* both adjustment terms. So a `SHOP_VEHICLE` shop
would show a uniformly neutral list **and** a meaningless badge. Suppress both and show a single
explanatory line (`requirements.md:38`).

**Client-computability — verified, add no replication.**

| Input | Source | Client availability |
|---|---|---|
| Shop list / town→shops / gun dealers | `m_aAllShops`, `m_mTownShops`, `m_aGunDealers` | JIP-replicated, `RplSave` `:1888-1911` / `RplLoad` `:1926-1953` |
| Shop stock | `OVT_ShopComponent.m_aInventory`, `GetStock(id)` (`:61`) | `RplSave`/`RplLoad` (`:73-104`) + broadcast `RpcDo_SetInventory` (`:122`) |
| Town max stock | `GetTownMaxStock` (`:619-624`) = `round(1 + pop × m_fNPCBuyRate × GetDemand(id) × stability/100)` | population/stability replicated; demand from config |
| Item cost / demand / vehicle set / resource index | `m_mItemCosts`, `m_mItemDemand`, `m_aAllVehicles`, `m_aResources` | built by `BuildResourceDatabase()` in `AfterInit()` (`:1288-1294`) on **every** machine |
| Distance to port | `DistanceToNearestPort(pos)` (`:846-858`) over `m_aAllPorts` | `m_aAllPorts` built by the client-side `InitializePorts` scan |

**Town resolution must match the server.** `GetSellPriceAtOffset` uses `GetNearestTown(pos)` then
`GetTownID` (`:563-565`) — **not** `OVT_ShopComponent.m_iTownId`. Use the same path so the panel agrees with
the price the player will actually be charged.

**Do not call `GetTownStock`** (N6). Compute town stock with a local null-safe loop over
`m_mTownShops[townId]`, skipping shops whose `GetShopByRplId` returns null.

Carets may be computed from **sell** price: `GetBuyPrice` (`:582-597`) adds `m_fShopProfitMargin` and the
player's `priceMultiplier`, both uniform multipliers that do not change relative ranking.
Minor edge: `GetSellPriceAtOffset` skips all adjustment when `pos[0] == 0`.

**Computed once, at panel-open time** — inside `OnSetupLocationInfo`, never in `CanFastTravel`,
`ShouldShowLocation` or anything the element calls per zoom change (`OVT_MapLocationType.c:368-379`;
`OVT_MapLocationElement.c:300`, `:506`).

**Never display prices, currency amounts, percentages or stock counts** on the shop panel. Up vs down must
not rely on colour alone — the carets are directional glyphs first, colour second.

### 4.6b Gun dealer panel — the four signature weapons (added 2026-08-10, user directive)

**The requirement.** A gun dealer's panel must answer *"is it worth walking to this one?"*. Scarcity carets
cannot answer that. **What this dealer stocks** can.

**What the code actually supports (N13).** Of everything a dealer carries, exactly **four ids are rolled
per-dealer** — one **RIFLE**, one **ROCKET_LAUNCHER**, one **SNIPER_RIFLE**, one **MACHINE_GUN**. Every
other id (all pistols, all ammunition, attachments, throwables, explosives, weed) is **identical at every
dealer** and is therefore worthless as a differentiator. So the panel is **at most four rows**, which is also
exactly the right size for a map panel.

> The originating request said "one of each gun type". That is **true for those four categories and false
> for pistols** — every dealer stocks every pistol, including occupying-faction ones. Listing pistols would
> pad the panel with identical rows at every dealer and actively obscure the four that differ.

**Row content.** Per row: the weapon's display name, its category label (Rifle / Sniper / Machine Gun /
Launcher), and a **price caret** from the §4.6 scarcity model. **No prices, no stock counts** — unchanged.

**Selecting the four.** `OVT_ShopCategory` cannot do this (N16 — it collapses every weapon into `WEAPONS`
and checks mode before type). The panel must:
1. iterate the dealer's `m_aInventory`, **filtering `stock > 0`** (N15 — sold-out ids remain at 0);
2. keep ids whose `OVT_ShopCategory` is `WEAPONS` (cheap, already cached by `GetItemCategory`, `:1789`);
3. resolve **`SCR_EArsenalItemType`** for that small residue and keep only `RIFLE`, `SNIPER_RIFLE`,
   `MACHINE_GUN`, `ROCKET_LAUNCHER` — discarding `PISTOL`;
4. cache id → arsenal type in a member map, mirroring `OVT_ShopContext.m_mDisplayNames` (`:73`, keyed by the
   int resource id). The residue is small (four rolled weapons plus the pistol set), so this is bounded.

If a category yields nothing, **omit that row** — do not print "None". A dealer that rolled no sniper simply
has three rows.

**Honesty constraint (N14).** The four weapons **re-roll on every campaign load** — the generator is unseeded
and nothing persists the result. The panel is accurate when read, so copy must be present-tense
("Available here") and must never imply permanence ("always stocks"). Within a session the set is stable, so
the panel does not lie to a player acting on it now.

**Reuse, not divergence.** `OVT_MapInfoShop.layout` already provides `Rows` + `Badge` + `ScarcityRows`.
GunDealer reuses it wholesale and populates the item section with these four rows instead of top-N carets.
The **remoteness badge stays meaningful** for a gun dealer — `GetSellPriceAtOffset` applies the
distance-to-port term to any non-vehicle item — so it renders unchanged.

**Tolerate the two economy defects, do not fix them.** N17 (sniper ammunition likely never stocked, from a
duplicated config rule) and N18 (`RegisterGunDealer` has no broadcast RPC, so a dealer registered after a
client joined is missing from that client's list) are both filed against `economy/shops` in Phase 7. The
panel must render correctly regardless of either.

---

## 5. Implementation Phases

Sequenced per the epic: **the bus-stop migration goes first**, because it is the riskiest item (it changes
how stops are authored and discovered) and because `map/fast-travel` F5 is blocked on it.

Effort is expressed as **S / M / L** relative to a single focused session. "Agent" is the routing hint for
`/proceed`.

---

### Phase 1 — Marker component, registry, and the bus-stop migration (G4) — **L — `component-developer-advanced`**

> **Advanced.** New manager on the game mode, a new component class, a responsibility moved out of
> `towns/core`, a live prefab delta, and the seam `map/fast-travel` F5 depends on. Integration-heavy.

**Tasks**

1. `Scripts/Game/Components/Map/OVT_MapMarkerComponent.c` — `OVT_Component` subclass with
   `OVT_MapMarkerCategory m_eCategory` (enum: `BUS_STOP`, `POI`), `ref SCR_UIInfo m_UiInfo`,
   `bool m_bMustOwnBase`. `OnPostInit` → `CallLater(Register, 0)`; `OnDelete` → unregister. Guard with
   `if (SCR_Global.IsEditMode()) return;` like `OVT_Component` does (`:31`).
2. `Scripts/Game/GameMode/Managers/OVT_MapMarkerManagerComponent.c` — `GetInstance()` singleton;
   `RegisterMarker`/`UnregisterMarker` (idempotent); `GetMarkers(category)`;
   `GetNearestMarker(pos, category, maxDist)`; one `QueryEntitiesBySphere("0 0 0", 99999999, Check, Filter,
   EQueryEntitiesFlags.STATIC)` at init, **outside any `Replication.IsServer()` guard**, following
   `InitializePorts` (`:1649-1656`).
3. `Prefabs/GameMode/OVT_OverthrowGameMode.et` — add the manager component block with a fresh unique GUID
   (the file is plain text, 352 lines, one GUID-keyed block per manager; `OVT_TownManagerComponent` at
   `:212` is the shape to copy).
4. `Scripts/Game/Global/OVT_Global.c` — add `static OVT_MapMarkerManagerComponent GetMapMarkers()`.
5. `Prefabs/Structures/Signs/Traffic/SignBusStop_01.et` — add `OVT_MapMarkerComponent { m_eCategory BUS_STOP }`
   to the **existing** Overthrow delta (N9). One file; covers every world-placed stop.
6. `Scripts/Game/UI/Map/LocationTypes/OVT_MapLocationBusStop.c` + a `Configs/Map/OverthrowMap.conf` entry
   with a fresh GUID. Icon: **`bus`** — the art landed 2026-08-10, so the K8 `port` fallback is unused.
7. Delete `OVT_TownManagerComponent.GetNearestBusStop` (`:881`) and `FindBusStop` (`:1197-1203`); re-point
   `OVT_MapContext.c:453` at `GetMapMarkers().GetNearestMarker(pos, BUS_STOP, 15)`. Keep the 15 m radius —
   changing it changes bus-travel behaviour, which is `map/fast-travel`'s call.
8. Check for **duplicate icons**: vanilla's `SCR_MapDescriptorComponent { MainType "Bus Stop" }` still merges
   in from the base prefab. If vanilla draws it on the fullscreen map, suppress it in the Overthrow delta;
   if it does not, record that and move on.

**Acceptance**
- `tools/compile-check.sh` exit 0.
- Every world bus stop shows an Overthrow bus-stop marker, and exactly one icon per stop.
- `OVT_CatchBusAction` → map → click a stop still works end to end (unchanged behaviour, new discovery path).
- No `GetNearestBusStop`/`MDT_BUSSTOP` reference remains outside vanilla.
- A save made before this phase loads and behaves identically (§3.10 / DoD V-7).

---

### Phase 2 — POI migration (G3) and removal of the `RegisterPOI` dependency — **M — `component-developer`**

**Tasks**

1. `Scripts/Game/UI/Map/LocationTypes/OVT_MapLocationPOI.c` + conf entry (fresh GUID). Name/icon from the
   marker's `SCR_UIInfo`; `m_fVisibilityZoom ≈ 1.0`.
2. Preserve legacy gating exactly: `m_bMustOwnBase` ⇒ nearest base exists **and** within 220 m **and**
   `!base.IsOccupyingFaction()` (`OVT_MainMenuContextOverrideComponent.c:45-53`; `OVT_MapIcons.c:495-498`).
3. Add `OVT_MapMarkerComponent { m_eCategory POI; m_UiInfo <copy of the existing SCR_UIInfo>; m_bMustOwnBase <same> }`
   to the three prefabs in §3.5. **Copy the existing `SCR_UIInfo` values; do not invent new ones.**
4. `OVT_MainMenuContextOverrideComponent` — delete the `EOnFrame` map-registration branch, the
   `m_bRegistered` flag, `m_bShowOnMap`, and the now-unneeded `EntityEvent.FRAME` in the event mask.
   **Leave `m_ContextName`, `m_fRange`, `m_bMustBeDriving` and `CanShow` untouched** — the in-world menu
   role is not this feature's.
5. Grep for any other `OVT_MapIcons.` reference outside `OVT_MapIcons.c` itself; there must be none left.

**Acceptance**
- `grep -rn "OVT_MapIcons\." --include=*.c Scripts/ | grep -v "UI/Map/OVT_MapIcons.c"` returns nothing.
- Garages and maintenance ramps appear as POI markers with their authored icons and names.
- `Garage_E_02` markers appear only near a **resistance-held** base and disappear when that base flips.
- A newly built `OVT_VehicleMaintenanceRamp` appears on the **next** map open without a restart.
- `tools/compile-check.sh` exit 0.

---

### Phase 3 — Vehicle type (G1), house privacy fix (N1), home marker (N3), T2 — **M — `component-developer`**

**Tasks**

1. `OVT_MapLocationVehicle` + conf entry. Local player's owned vehicles only; icon `vehicle` (exists);
   yaw rotation via `OnSetupIconWidget` (legacy parity, `OVT_MapIcons.c:727`). Uses the inherited
   `m_Vehicles` — **closes T2**.
2. **N1:** rewrite `OVT_MapLocationHouse.PopulateLocations` to use
   `m_RealEstate.GetOwned(localPersId)` / `GetRented(localPersId)`
   (`OVT_OwnerManagerComponent.c:224`, `:243` — both return `set<EntityID>`, and both are exactly what
   legacy called at `OVT_MapIcons.c:472`/`:543`) instead of iterating every player's `m_mOwned`/`m_mRented`.
   **Leave `OVT_MapLocationWarehouse` alone** — public is intentional (N2).
3. **N3:** add an `isHome` key via `m_RealEstate.IsHome(persId, entityId)` (`:595`) and a distinct home
   colour/icon; keep rented houses visually distinct as today.
4. Remove the inert per-location `visibilityZoom` writes from House and FOB (N4) so no one reads them as
   working behaviour. Do **not** change `map/core` here — see K9.
5. **T3, opportunistic:** introduce `Scripts/Game/UI/Map/Core/OVT_MapDataKeys.c` with `static const string`
   constants for the shared key sets, and use them in every type this feature touches. Do not rewrite types
   this feature does not otherwise touch.
6. **T1, opportunistic:** any type touched in this feature switches to the inherited manager cache. Do not
   refactor `Base`/`RadioTower` unless touched.

**Acceptance**
- Your own vehicles appear at their positions with correct facing; **another player's do not**.
- Only your own owned/rented houses appear; your home is visually distinguished.
- `tools/compile-check.sh` exit 0; `tools/run-tests.sh "{6A6E2A002F53A581}"` still green.

---

### Phase 4 — Waypoint markers (G2) — **S — `component-developer`**

**Tasks**

1. `OVT_JobManagerComponent.c:64` — add `vector m_vRecruitWaypoint;` next to `m_vCurrentWaypoint`, with a
   comment recording that both are client-local, non-replicated, non-persisted.
2. `OVT_RecruitsContext.c:489` — write `m_vRecruitWaypoint` instead of `m_vCurrentWaypoint`. Nothing else in
   that context changes.
3. `OVT_MapLocationWaypoint` + conf entry; up to two records; icon `waypoint` (exists);
   `m_fVisibilityZoom 0`; distinct colour and display name per slot; skip a slot when it is `"0 0 0"`
   (the same emptiness test legacy used, `OVT_MapIcons.c:780`).

**Acceptance**
- "Show on map" from the Jobs menu places a job waypoint; "Show on map" from the Recruits menu places a
  recruit waypoint; **both are visible at the same time** and neither clears the other.
- `tools/compile-check.sh` exit 0.

---

### Phase 5 — Shared info-row mechanism and the non-shop panels — **L — `ui-developer-advanced`**

> **Advanced.** New layouts, a new widget-name contract, an additive change to `map/core`'s contract, and
> the gamepad/console readability surface. `FindAnyWidget` returning null is a silent no-op the compiler
> cannot catch — this is exactly how `map/core` D1 and D2 shipped dead.

**Tasks**

1. `UI/Layouts/Map/Core/OVT_MapInfoRow.layout` (`RowLabel`, `RowValue`, `RowIcon`) +
   `Scripts/Game/UI/Map/Core/OVT_MapInfoRowHandler.c`, modelled on the town-modifier chip pair.
2. `UI/Layouts/Map/Core/OVT_MapInfoRows.layout` — container with a vertical layout named **`Rows`**.
3. `OVT_MapLocationType` — add `m_SharedInfoLayout`, the `BuildInfoRows` virtual and the `AddInfoRow` /
   `AddInfoIconRow` / `ClearInfoRows` helpers, and the `UpdateInfoPanel` branch (§4.5). **Verify Town, Base
   and RadioTower render byte-identically afterwards.**
4. Implement `BuildInfoRows` for `FOB`, `Camp`, `House`, `Warehouse`, `Port`, `BusStop`, `POI`, `Vehicle`,
   `Waypoint` using the row table in §4.5. Show garrison **only when > 0** (N5).
5. Add the new string ids to `Language/localization_Overthrow.st` (master only — see §10).
6. **Widget-name audit:** for every `FindAnyWidget` added, confirm the name exists in the layout. Record the
   layout↔code name contract in a comment at the top of each new layout's handler.

**Acceptance**
- Selecting any of the nine shared-panel types shows a populated panel, not a bare header.
- Town, Base and RadioTower panels are visually unchanged.
- Every row label resolves to real text (no raw `#OVT-` keys) once the exports are regenerated; before that,
  literal text is acceptable and must be listed for the user.
- Panels are readable at 1080p and legible on a controller with a gamepad cursor.

---

### Phase 6 — Shop / GunDealer price indicator — **M — `component-developer` + `ui-developer`**

**Tasks**

1. `Scripts/Game/UI/Map/LocationTypes/Shop/OVT_MapShopPriceIndicator.c` — the calculator from §4.6:
   scarcity per item, remoteness for the shop, banding, top-N selection, vehicle suppression, null-safe
   town-stock loop (N6), negative-distance handling (N7), display-name cache (N8).
2. `UI/Layouts/Map/LocationTypes/OVT_MapInfoShop.layout` — `Rows` + `Badge` + `ScarcityRows`.
3. `OVT_MapLocationShop` — set `m_InfoLayout`, implement `OnSetupLocationInfo` (shop type row, badge,
   caret rows).
3b. **`OVT_MapLocationGunDealer` — the four signature weapons (§4.6b), NOT the shop's top-N carets.**
   Same layout and same caret helper, different population: filter `stock > 0` (N15) → `WEAPONS` category →
   resolve `SCR_EArsenalItemType` and keep only RIFLE / SNIPER_RIFLE / MACHINE_GUN / ROCKET_LAUNCHER,
   discarding PISTOL (N13/N16). Cache id → arsenal type. Omit a missing category rather than printing
   "None". Copy must be present-tense — the set re-rolls every campaign load (N14). Badge unchanged.
4. Caret rendering with a **dual affordance** so the code path is testable before the art exists (K8): the
   row carries both an `ImageWidget` (`CaretIcon`) and a `TextWidget` (`CaretText`); until the imageset has
   the icons, set the text and hide the image.
5. New `.st` ids for the section header, the badge, the vehicle-shop line and the empty-shop line.

**Acceptance**
- At a shop with mixed stock, at least one up row and one down row appear, and no numbers appear anywhere.
- A shop with only neutral items shows the "nothing unusual" line, not an empty section.
- A `SHOP_VEHICLE` shop shows the flat-price explanation and **no** carets and **no** badge.
- Two shops in the **same** town show identical carets (expected — it is a town signal) and identical badges.
- A shop far from any port shows a badge; a shop next to a port shows none.
- **Gun dealer:** the panel lists **at most four** weapons — one each of rifle / sniper / machine gun /
  launcher — with **no pistols**, no ammunition and no attachments. Two different dealers show **different**
  weapons in at least one slot. A category the dealer did not roll is **absent**, not "None".
- **Gun dealer:** no item with stock 0 appears (N15).
- Opening and closing the panel 20 times does not degrade frame time (names are cached; nothing runs per
  frame or per zoom change).

---

### Phase 7 — Verification, parity sign-off and bug filing — **M — user-driven, no agent**

**Tasks**

1. Single-player sweep (DoD V-1 … V-4).
2. Two-client MP/JIP gate (DoD V-5).
3. Gamepad/console gate (DoD V-6).
4. Save-compatibility check (DoD V-7).
5. Tick off the legacy icon enumeration (DoD F-1) and hand the parity statement to `map/legacy-retirement`.
6. File the bugs this feature deliberately does **not** fix: **core D8** (N4, per-location visibility zoom),
   **FOB/Camp garrison not replicated** (N5, against `resistance/fob`), **`GetTownStock` missing null guard**
   (N6, against `economy/shops`).

---

## 6. Key Technical Decisions

**K1 — G2 (job waypoints) is IN, as a location type.**
`OVT_RecruitsContext.ShowOnMap` (`:489`) writes into the *same* field `OVT_JobsContext` (`:109`) uses, and
only legacy `OVT_MapIcons` renders it (`:779-787`). Dropping G2 would silently kill "show recruit on map"
too. The plan does **not** inherit the shared-slot defect: one extra `vector` field and a one-line change in
the recruits context give both markers concurrent slots, with no change to job or recruit logic. Additive,
minimal, in someone else's territory — deliberately so.

**K2 — G3 and G4 share ONE generic marker component and ONE registry.**
Both are "an entity that should appear on the map and be findable by proximity". A single
`OVT_MapMarkerComponent` (category + `SCR_UIInfo` + a visibility rule) plus one
`OVT_MapMarkerManagerComponent` serves both, with two thin `OVT_MapLocationType` subclasses filtering by
category. Cheaper than two mechanisms, and it gives `map/fast-travel` a single `GetNearestMarker` API. The
component shape is the proven `OVT_PortControllerComponent` pattern (`OVT_PortController.c:5`,
`FilterPortEntities` `:1857`); the discovery mechanism is the proven client-safe world scan
(`InitializePorts` `:1649-1656`) rather than legacy's `EOnFrame` self-registration, because a frame event
may never fire for a streamed-out static. Self-registration is retained **only** for runtime-spawned
placeables, which the world scan cannot see.

**K3 — No replication is added for markers.** The scan runs on every machine over identical static world
entities, exactly as `m_aAllPorts` does (and `m_aAllPorts` is deliberately absent from the economy manager's
`RplSave`). Adding replication would be strictly worse: more bandwidth, a JIP ordering hazard, and a second
source of truth for something the client can already see.

**K4 — Info panels are data-driven rows, with bespoke layouts only where warranted.**
One reusable content layout plus one row widget gives nine types real panels for roughly the cost of two,
and keeps them visually consistent. The existing bespoke path (`m_InfoLayout` → `OnSetupLocationInfo`) is
untouched, so Town/Base/RadioTower cannot regress. Shop needs a bespoke element for the badge and the caret
section — it **composes** the shared `Rows` container plus its own section rather than going fully bespoke,
so `AddInfoRow` works there too and GunDealer reuses the whole thing.

**K5 — The one change to `map/core`'s contract is additive and must be flagged.**
`OVT_MapLocationType` gains `m_SharedInfoLayout`, a `BuildInfoRows` virtual and row helpers, and
`UpdateInfoPanel` gains an `else if` branch. **Existing behaviour is unchanged when `m_InfoLayout` is set,
and unchanged when both are empty.** Cross-feature impact: `map/core`'s documented contract table must gain
the new virtual; `map/map-layers` is unaffected. The epic's rule "do not change core's contract without
saying so explicitly" is satisfied by this paragraph and by a matching note in `map/core`'s `context.md`
when the phase lands.

**K6 — Shop indicator: split by TERM, on fixed absolute scales.**
`requirements.md:37` proposed per-item carets measured against the shop's own norm and deferred the decision
to planning. Rejected: normalising against the shop mean destroys real information (a genuinely scarce town
would be flattened to all-neutral). Because the remoteness term is *exactly* uniform across a shop,
excluding it from the per-item signal already solves the problem that motivated normalisation, with no
statistics and no hidden state. Fixed absolute bands are also stable across shops, so a player learns one
scale, not one per shop.

**K7 — The scarcity signal is town-wide, and the panel says so.**
`GetTownStock` sums across every shop in the town (`:601-611`), so two shops in one town read identically.
This is a property of Overthrow's economy, not a bug in the indicator. Labelling it "local supply" is honest
and is more useful to a player than a per-shop claim the data cannot support.

**K8 — Art has a stated fallback so the code path is testable before it lands.**
Bus stop: ~~use the existing `port` icon until `busstop` exists~~ — **superseded 2026-08-10**: the user added
a `bus` icon to `overthrow_mapicons.imageset` (`:101`) before Phase 1 landed, so the bus-stop half of this
decision was never exercised. Carets: the row carries both an `ImageWidget` and a `TextWidget`, and the code
renders text until the imageset has the glyphs — **also superseded 2026-08-10**: the caret set landed as
`overthrow_priceicons.imageset` before Phase 6 started. **Neither fallback was ever exercised**; both art
items arrived ahead of the phase that needed them, so Phase 6 renders `ImageWidget` carets directly.

The **dual-affordance requirement survives the fallback becoming moot**: the row still carries both an
`ImageWidget` and a `TextWidget`, because up-vs-down must not rely on colour alone (Q/F-6). The `TextWidget`
is now an accessibility affordance, not a placeholder.

⚠️ **Open art question:** `overthrow_priceicons.imageset` declares `size 1 1` in its `ImageSetTextureClass`
while `RefSize` is `200 134`. The working `overthrow_mapicons.imageset` declares `size 784 522`, matching its
own `RefSize`. If the carets fail to draw in Phase 6, a Workbench re-import of the atlas is the first thing
to check — not the panel code.

**K9 — N4 (per-location visibility zoom) is filed, not fixed here.**
The element reads only the type-level `GetVisibilityZoom()` (`OVT_MapLocationElement.c:315`, `:498`), so
FOB's "priority FOBs are always visible" does not work. The fix belongs in `map/core` (it is a change to the
element, not to a type). This feature **removes the inert writes** so nobody mistakes them for working
behaviour, files the bug, and does **not** claim priority-FOB visibility in its Definition of Done.

**K10 — This feature adds no client→server RPC.** Every new type reads replicated state and renders. The
prohibition on adding to `OVT_PlayerCommsComponent` is therefore satisfied trivially, and
`OVT_OverthrowController` is not touched. If a future "clear this waypoint" affordance is wanted, it is
client-local state and still needs no RPC.

**K11 — Tech debt is folded in opportunistically, not as a phase.** T2 resolves itself when Vehicle lands.
T3 gets constants introduced and applied only to types this feature already edits. T1 is applied only where
a type is being touched. T5 is not addressed — `Town` is not touched.

---

## 7. Definition of Done

Written so an evaluator with no implementation context can verify each item.

### Functional

**F-1 — Legacy icon-set parity checklist.** Open the map on a running campaign and confirm each of the
following renders:

- [ ] `camp` — a personal camp (public, or your own private one)
- [ ] `gundealer` — a town gun dealer
- [ ] `house` — a house **you** own or rent
- [ ] `port` — a port
- [ ] `tower` — a radio tower
- [ ] `vehicle` — a vehicle **you** own, rotated to its heading
- [ ] `warehouse` — any player's warehouse (public by design, N2)
- [ ] `waypoint` — a job waypoint **and**, simultaneously, a recruit waypoint
- [ ] **POI registry** — a garage and a vehicle maintenance ramp, with their authored icons
- [ ] **Bus stops** — every world bus stop, as an Overthrow marker

**F-2** — `grep -rn "OVT_MapIcons\." --include=*.c Scripts/` matches nothing outside
`Scripts/Game/UI/Map/OVT_MapIcons.c` itself. `OVT_MainMenuContextOverrideComponent` no longer references the
legacy static, and no longer sets `EntityEvent.FRAME` for map registration.

**F-3** — `GetNearestBusStop` and `FindBusStop` no longer exist in `OVT_TownManagerComponent`, and no script
outside vanilla references `EMapDescriptorType.MDT_BUSSTOP`. `OVT_CatchBusAction` still opens the map and a
bus stop is still selectable.

**F-4** — Exactly one icon is drawn per bus stop (no vanilla-descriptor duplicate).

**F-5** — Every location type has a populated info panel: selecting a FOB, camp, house, warehouse, shop,
port, gun dealer, bus stop, POI, vehicle or waypoint shows at least one content row below the header.

**F-6** — Shop panel: at a shop with mixed stock, at least one ▲ row and one ▼ row appear; **no price, no
currency symbol, no percentage and no stock count appears anywhere on the panel**; up and down are
distinguishable with colour removed (verified by describing the glyphs, not the colours).

**F-7** — A vehicle shop shows a single explanatory line and **no** carets and **no** remoteness badge.

**F-8** — Two shops in the same town show identical carets; a shop >3 km from the nearest port shows a
▲▲▲ badge; a shop within 500 m of a port shows none.

**F-9** — Job and recruit "show on map" markers coexist: triggering one does not remove the other.

**F-10** — A newly built vehicle maintenance ramp appears as a POI on the next map open, and disappears when
the ramp is destroyed.

### Quality

**Q-1** — `tools/compile-check.sh` exits 0.

**Q-2** — `tools/run-tests.sh "{6A6E2A002F53A581}"` exits 0 (regression guard only; no map UI test is added
— UI is not automatable here).

**Q-3 — Null/partial-replication tolerance.** Every new `PopulateLocations` returns cleanly when its manager
is null, when a `Replication.FindItem` lookup misses, or when a referenced entity is null. Verified by
opening the map **within 5 seconds of joining a server**, before state has settled: no script error is
printed and the map renders whatever has arrived.

**Q-4** — The shop indicator never calls `OVT_EconomyManagerComponent.GetTownStock` (N6). Verified by grep.

**Q-5 — Icon legibility at minimum zoom.** On a fully-populated campaign, zoom fully out and confirm the map
is readable: no unreadable clump of overlapping markers in any town, and each type's `m_fVisibilityZoom` is
set so that low-value markers (houses, shops, warehouses, POIs, bus stops) are hidden at that zoom.

**Q-6 — No regression in the three existing bespoke panels.** Town, Base and RadioTower panels show the same
content as before this feature (side-by-side against a pre-change build or a screenshot).

**Q-7** — No new work is performed inside `CanFastTravel` or `ShouldShowLocation`. Verified by reading the
new types: the shop indicator is computed only in `OnSetupLocationInfo`.

**Q-8** — Every `FindAnyWidget(...)` name added in this feature exists in the corresponding `.layout`.
Verified by an explicit name-by-name audit (this is the D1/D2 failure mode and the compiler cannot catch it).

### Integration

**I-1 — `map/core` contract.** The only change to `OVT_MapLocationType` is the additive
`m_SharedInfoLayout` + `BuildInfoRows` + row helpers described in K5, and it is recorded in `map/core`'s
`context.md`. No change to `OVT_MapLocationElement`, `OVT_MapLocationData` or `OVT_OverthrowMapUI`.

**I-2 — `map/fast-travel` F5 unblocked.** `OVT_Global.GetMapMarkers().GetNearestMarker(pos, BUS_STOP, r)`
exists, is client-callable, and bus stops are selectable map locations. F5 needs no further work from this
feature.

**I-3 — `map/legacy-retirement` parity gate satisfied.** F-1 is fully ticked and F-2/F-3 hold, so deleting
`OVT_MapIcons.c` would break no call site and remove no player-visible capability.

**I-4 — Bugs filed, not silently absorbed:** core D8 (N4), FOB/Camp garrison replication (N5),
`GetTownStock` null guard (N6). Each has a bug id recorded in this feature's `context.md`.

**I-5 — No new client→server RPC**, and `OVT_PlayerCommsComponent` is untouched (K10). Verified by diff.

### Verification Method

Run in order. Stop and fix at the first failure.

**V-1 — Compile.** `tools/compile-check.sh`. Expect exit 0 and no `file:line:` output.

**V-2 — Regression tests.** `tools/run-tests.sh "{6A6E29FF47ECB840}"` (Fast) then
`tools/run-tests.sh "{6A6E2A002F53A581}"` (All). Expect exit 0. Artifacts in `.tmp/run-tests/`.

**V-3 — Single-player marker sweep.** Start a campaign, buy a house, buy a vehicle, place a camp, deploy a
FOB, build a maintenance ramp, accept a job and "show on map", then mark a recruit on the map. Open the map
and walk the F-1 checklist. Confirm F-9 and F-10.

**V-4 — Zoom sweep.** From maximum zoom-out to maximum zoom-in, step through the zoom range and confirm
Q-5: each type appears at a sensible zoom, names do not overlap into illegibility, and no type is visible at
a zoom where it is pure noise.

**V-5 — Two-client MP/JIP gate (highest risk; do not skip).**

> ⚠️ Client launches open a real window on the user's desktop and can orphan. Always pass a long
> `--timeout` — the default is 600 s and will kill the client mid-test.

1. `tools/launch-server.sh` (headless dedicated server on this working tree).
2. Client A: `tools/launch-game.sh --timeout 3600 --profile OverthrowClient1 --allow-concurrent -- -client 127.0.0.1:2001`
3. As A: buy a house, buy a vehicle, place a **private** camp, deploy a FOB, and note a shop's caret rows.
4. Client B (**JIP — join after step 3**):
   `tools/launch-game.sh --timeout 3600 --profile OverthrowClient2 --allow-concurrent -- -client 127.0.0.1:2001`
5. On **B**, open the map and verify:
   - [ ] Bus stops, POIs, ports, towns, bases, radio towers and gun dealers are all present — the same set A sees.
   - [ ] **B does NOT see A's house.** (N1 — this is the specific regression being fixed.)
   - [ ] **B does NOT see A's vehicle.**
   - [ ] **B does NOT see A's private camp.**
   - [ ] B **does** see A's warehouse (public by design, N2) and A's FOB.
   - [ ] The FOB panel on B shows owner correctly; garrison is either absent or ≥ 0 with no error (N5).
   - [ ] The shop panel on B shows the **same** caret rows and the **same** badge as A saw.
6. On **B**, buy a house and a vehicle; confirm they appear for B and **not** for A.
7. On **A**, open the map immediately after B joins (Q-3): no script errors in the log.
8. Record whether any marker set differs between A and B; any difference that is not an intentional
   per-player type is a failure.

**V-6 — Gamepad/console gate.** With a controller only (no mouse):
- [ ] Move the map cursor onto each new marker type and confirm the info panel appears.
- [ ] Confirm every panel's rows are readable without scrolling at 1080p, and that the shop caret column is
      distinguishable at panel size.
- [ ] Confirm the fast-travel button is still reachable on the types that offer it.
- [ ] Note (do not fix) any dismissal awkwardness — `map/core` D2/D3 own the close affordance.

**V-7 — Save compatibility.** Load a save created **before** Phase 1, open the map, and confirm bus stops,
POIs and all pre-existing markers render; then save, reload, and confirm the same. Expected result: no
difference, because bus stops are world statics and are not persisted (§3.10). If any difference appears,
that assumption is wrong and this plan must be revised.

---

## 8. Quality Bar

This is a **UI-heavy, multiplayer-sensitive** feature. Two failure modes dominate, and both are invisible to
the compiler and to the test suites.

**Visual and interaction quality**
- A fully-populated campaign map must stay **readable**. Eleven-plus marker types is enough to turn a town
  into a smear. Every new type must set `m_fVisibilityZoom` / `m_fShowNameZoom` / `m_bShowName` /
  `m_bShowDistance` deliberately, and the zoom sweep (V-4) is the gate, not a code review.
- Icons must be **distinguishable at minimum zoom**, not merely present. Bus stops and POIs are the
  densest new types; if they clutter, raise their visibility zoom rather than shrinking the icon.
- Panels must be **readable, not just populated**. A row that says "Garrison: 0" on every client because the
  data does not replicate (N5) is worse than no row.
- The shop carets must read at panel size **without colour**. Test by describing the glyphs.
- Gamepad/console is a first-class target — it is the entire reason the rewrite sits on vanilla's
  `SCR_MapUIElement`. Anything that bypasses the vanilla widget layer forfeits it.

**Multiplayer correctness**
- **Per-player ownership types are the highest-risk surface in this feature.** House, Warehouse, Camp and
  Vehicle all carry identity. The two things that must hold are: a JIP client resolves `owner`/`renter`
  the same as a client present from the start, and **one player never sees another's private ownership**.
  N1 proves this class of bug is already live on the branch.
- Everything here reads replicated state at map-open time. Every read must tolerate **partial replication**:
  a null manager, a missed `Replication.FindItem`, an entity that has not streamed in. `PopulateLocations`
  returning early is always better than a script error.
- **No new replication and no new RPC.** If a panel appears to need one, the answer is to show less.

**Discipline**
- `FindAnyWidget` returning null is a silent no-op. Every new layout↔code name pair gets an explicit audit
  (Q-8). This is exactly how `map/core` D1 and D2 shipped dead.
- Do not invent data. If a fact is not in the code, it does not go on a panel — the project has already
  shipped two tutorial tips describing mechanics that did not exist.

---

## 9. Testing Strategy

**Automated — regression guard only.** No autotest suite covers map UI, and UI is not automatable here. Do
**not** invent UI test cases. Run `tools/run-tests.sh "{6A6E29FF47ECB840}"` (Fast, 38 cases ~15 s) after
each phase and `"{6A6E2A002F53A581}"` (All, 66 cases ~19 s) before sign-off, purely to prove nothing else
broke.

There is one place where a test **would** be honest: the caret banding function is pure maths over
`(townStock, townMaxStock)` and `(distToPort)`. If it is written as a static with no world dependency, it
belongs in the **Logic** tier (`Scripts/Game/Tests/TestSuites/…`), and the case must be proven able to fail
before shipping. This is optional but recommended — it is the only part of this feature that can be
asserted world-free.

**Manual — the real gate.**

| # | Scenario | Expected |
|---|---|---|
| 1 | Fresh campaign, map open before anything is built | Towns, bases, radio towers, ports, gun dealers, bus stops, POIs present; no errors |
| 2 | Buy house → map | Only your house; home distinguished from a second owned house |
| 3 | Buy vehicle, park it, drive elsewhere, reopen map | Marker at the position **as of map open**, rotated to heading (refresh-while-open is a non-goal) |
| 4 | Place private camp, then public camp | Private visible only to you; public visible to all |
| 5 | Deploy FOB, set priority | Both render; priority uses `fob_priority`. **Do not** expect always-visible (K9/N4) |
| 6 | Build maintenance ramp, reopen map | POI appears; destroy it, reopen, POI gone |
| 7 | Job "show on map" then recruit "show on map" | Two distinct waypoints, both visible |
| 8 | Shop with full stock vs shop with depleted stock in the same town | Identical carets (town-wide signal, K7) |
| 9 | Coastal shop vs inland shop | Inland badge higher; coastal shop may have no badge |
| 10 | Vehicle shop | Explanatory line, no carets, no badge |
| 11 | Empty shop | "Nothing unusual" line, no empty section |
| 12 | Map open within 5 s of joining a server | No script errors; markers fill in on the next open |
| 13 | Two clients, JIP (V-5) | Ownership isolation holds in both directions |
| 14 | Controller only (V-6) | Every marker selectable, every panel readable |

**Debugging.** No debugger — `Print()` only. When a marker does not appear, the three usual causes are: the
registry never saw the entity (log the scan count at init), the record was skipped by a null guard, or the
element is zoom-gated invisible. Log each separately; do not guess.

---

## 10. Dependencies

### Internal (code)

- **`map/core`** — the `OVT_MapLocationType` contract, `OVT_MapLocationData`, `OVT_MapLocationElement`, the
  shared panel shell. Extended additively (K5). Its D1/D2/D3/D7 were fixed there on 2026-08-10 (BUG-133,
  BUG-134, BUG-137); `OnLocationClicked` is reachable and safe to override.
- **`towns/core`** — loses `GetNearestBusStop`/`FindBusStop`; gains nothing.
- **`resistance/fob`** — FOB/camp records; N5 filed against it.
- **`economy/shops` / `economy/real-estate`** — shop, gun-dealer, house and warehouse records and the
  pricing API; N6 filed against `economy/shops`.
- **`OVT_VehicleManagerComponent`** — Vehicle source of truth.
- **`jobs/core` + `resistance/recruits`** — one new field and one changed write (Phase 4).
- **`map/fast-travel`** — consumes `GetNearestMarker` for F5. Do not implement bus travel here.
- **`map/legacy-retirement`** — hard-gated on F-1/F-2/F-3.

### External — user / Workbench work

| Item | Blocking? | Fallback |
|---|---|---|
| ~~**Bus-stop icon**~~ — **LANDED 2026-08-10** as `bus` (`overthrow_mapicons.imageset:101`), atlas rebuilt | No | *(fallback no longer needed)* |
| ~~**Caret icon set**~~ — **LANDED 2026-08-10.** `{A5EA4C81F9A25690}UI/Imagesets/overthrow_priceicons.imageset` — `up_1`,`up_2`,`up_3`,`down_1`,`down_2`,`down_3` (64×64). No neutral icon, which is **correct**: §4.6 omits the row entirely in the neutral band | No | *(fallback no longer needed — render images)* |
| **Localization export regeneration** for the new `.st` ids | No | Layouts/rows use literal English until regenerated; list the affected ids for the user |
| Workbench verification of the new prefab/conf blocks | No | Blocks are plain text and hand-editable with fresh GUIDs; the user confirms they load |

**Localization is master-only.** New ids go in `Language/localization_Overthrow.st` **only** — never edit
`Language/localization_Overthrow.<lang>.conf` (Workbench-generated; hand-editing has corrupted all six files
before). Proposed ids (final copy is the user's):

```
OVT-Map_BusStop, OVT-Map_BusStop_Desc
OVT-Map_POI_Desc
OVT-Map_Waypoint_Job, OVT-Map_Waypoint_Recruit
OVT-Map_Vehicle
OVT-Map_Row_Priority, OVT-Map_Row_Garrison, OVT-Map_Row_Access,
OVT-Map_Row_Private, OVT-Map_Row_Public, OVT-Map_Row_Status, OVT-Map_Row_Contents
OVT-Map_House_Home
OVT-Map_Port_Desc
OVT-Map_Shop_LocalSupply, OVT-Map_Shop_Remote, OVT-Map_Shop_VehicleFlat, OVT-Map_Shop_NoStock,
OVT-Map_Shop_NothingUnusual
```

### New and changed files

```
Scripts/Game/
├── Components/Map/
│   └── OVT_MapMarkerComponent.c                        NEW
├── GameMode/Managers/
│   └── OVT_MapMarkerManagerComponent.c                 NEW
├── Global/OVT_Global.c                                 + GetMapMarkers()
├── UI/Map/Core/
│   ├── OVT_MapLocationType.c                           + m_SharedInfoLayout, BuildInfoRows, row helpers
│   ├── OVT_MapInfoRowHandler.c                         NEW
│   └── OVT_MapDataKeys.c                               NEW (T3)
├── UI/Map/LocationTypes/
│   ├── OVT_MapLocationBusStop.c                        NEW
│   ├── OVT_MapLocationPOI.c                            NEW
│   ├── OVT_MapLocationVehicle.c                        NEW
│   ├── OVT_MapLocationWaypoint.c                       NEW
│   ├── Shop/OVT_MapShopPriceIndicator.c                NEW
│   ├── OVT_MapLocationHouse.c                          privacy fix (N1), home flag (N3), BuildInfoRows
│   ├── OVT_MapLocationWarehouse.c                      BuildInfoRows (contents, N11)
│   ├── OVT_MapLocationFOB.c                            BuildInfoRows; drop inert visibilityZoom (N4)
│   ├── OVT_MapLocationCamp.c                           BuildInfoRows
│   ├── OVT_MapLocationPort.c                           BuildInfoRows
│   ├── OVT_MapLocationShop.c                           m_InfoLayout + OnSetupLocationInfo
│   └── OVT_MapLocationGunDealer.c                      reuses the shop panel
├── Components/OVT_MainMenuContextOverrideComponent.c   remove the RegisterPOI branch
├── GameMode/Managers/OVT_TownManagerComponent.c        delete GetNearestBusStop + FindBusStop
├── GameMode/Managers/OVT_JobManagerComponent.c         + m_vRecruitWaypoint
├── UI/Context/OVT_RecruitsContext.c                    write the recruit slot (:489)
└── UI/Context/OVT_MapContext.c                         re-point :453 at the registry

UI/Layouts/Map/
├── Core/OVT_MapInfoRow.layout                          NEW
├── Core/OVT_MapInfoRows.layout                         NEW
└── LocationTypes/OVT_MapInfoShop.layout                NEW

Configs/Map/OverthrowMap.conf                           + 4 type entries (fresh GUIDs), + Shop m_InfoLayout

Prefabs/
├── GameMode/OVT_OverthrowGameMode.et                   + OVT_MapMarkerManagerComponent
├── Structures/Signs/Traffic/SignBusStop_01.et          + OVT_MapMarkerComponent (BUS_STOP)
├── Structures/Industrial/Garages/Garage_E_02/Garage_E_02.et                    + (POI, mustOwnBase 1)
├── Structures/Military/Houses/GarageMilitary_E_01/GarageMilitary_E_01_base.et  + (POI)
└── Structures/Military/FOB/OVT_VehicleMaintenanceRamp.et                       + (POI)

Language/localization_Overthrow.st                      + new ids (master only)
```

---

## 11. Risks & Mitigation

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| R1 | **The world scan misses streamed-out statics**, so some bus stops or garages never register | Low | High — silent partial parity | The scan is the *proven* mechanism (`InitializePorts` finds every port map-wide). Log the marker count at init and compare against a known world count during V-3. Self-registration is the second net. |
| R2 | **Duplicate bus-stop icons** — vanilla's `SCR_MapDescriptorComponent` still merges in from the base prefab | Medium | Low | Explicit Phase 1 task 8; suppress in the delta if vanilla draws it. Same-GUID overrides are **deltas**, so the descriptor is inherited unless overridden. |
| R3 | **Widget-name mismatch ships dead** (the D1/D2 failure mode) | Medium | Medium | Q-8 name audit; every new handler documents its layout contract; V-3 visually confirms every panel populates. |
| R4 | **`GetTownStock` null-deref** taken by accident on a client | Medium | High — script error on a client | Q-4 greps for the call; the indicator uses its own null-safe loop; bug filed against `economy/shops`. |
| R5 | **Caret bands never produce all three levels** in a real campaign — everything reads neutral | Medium | Medium | Bands are derived from the ±10 % analytic range, not guessed. V-3 records the observed spread; if all-neutral, tighten the inner band before adding statistics. Do **not** reintroduce shop-mean normalisation (K6). |
| R6 | **Map clutter** — eleven-plus types make towns unreadable | High | Medium | V-4 zoom sweep is a gate. Bus stops and POIs default to high visibility zoom. `map/map-layers` will add toggles later; do not pre-build them (YAGNI). |
| R7 | **Per-player leakage in MP** beyond the House case | Medium | High | V-5 checks all four ownership types in both directions. Every ownership type must filter on `GetCurrentPlayerID()` at populate time, not at display time. |
| R8 | **Garrison rows read 0 on clients** and look broken (N5) | High (it is already true) | Low if handled | Show garrison only when > 0; file against `resistance/fob`; do not fix replication here. |
| R9 | **`m_vRecruitWaypoint` edit conflicts** with concurrent work in `jobs`/`recruits` | Low | Low | Two lines, additive; re-check git state before editing (parallel sessions commit mid-work in this tree). |
| R10 | **Art does not arrive**, blocking the shop panel | Medium | Low | K8 fallbacks make both art items non-blocking; the code path ships and the art is a one-line switch. |
| R11 | **Core contract extension breaks Town/Base/RadioTower** | Low | High | The new branch only runs when `m_InfoLayout` is empty; Q-6 compares the three panels before and after. |
| R12 | **The registry becomes a second source of truth** for something non-map | Low | Medium | The component carries only presentation data (category, `SCR_UIInfo`, one visibility rule). Anything else belongs to the owning system. This is the epic's single sanctioned write; keep it tight. |

---

*Plan created 2026-08-10 by `/plan-feature map/location-types`, replacing the retrospective discovery
document. Discovered architecture preserved in §3. Use `/proceed` to execute the phases in order — Phase 1
and Phase 5 are flagged for the advanced (max-effort) agents.*
