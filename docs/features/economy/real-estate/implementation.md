# Real Estate - Implementation Plan (Retrospective)

**Status:** Implemented (Documented Retrospectively)
**Originally Implemented:** Grown over ~3 years (first commit `82b9df4 Real Estate`; EPF→vanilla persistence migration `28c121b`, 2026-08-02)
**Documented:** 2026-08-02
**Last Updated:** 2026-08-02 21:35

---

## Executive Summary

Real estate is the property-ownership layer of the economy epic: buying, selling and renting world buildings, the home/starting-house system that anchors player spawns, and warehouses (virtual item inventories attached to warehouse-type buildings). It is a game-mode manager singleton (`OVT_RealEstateManagerComponent`) built on a generic position-keyed ownership base (`OVT_OwnerManagerComponent`).

**Note:** This is a retrospective implementation plan created by analyzing the existing codebase. The feature has already been implemented.

---

## Goals

### Primary Goals
- Let players buy, sell, rent and rent out world buildings using either their own or resistance funds.
- Give every player a home (starting house or chosen property) that drives respawn, fast travel and the starting car.
- Provide warehouses: persistent, shared bulk storage for items, deposited from and withdrawn to vehicles.

### Success Criteria
- [x] Ownership/rental of buildings, keyed stably across sessions and network
- [x] Starting-home pool assignment for new players, honouring restored ownership on load
- [x] Warehouses with deposit/withdraw flows and JIP replication
- [x] Persistence via vanilla `ScriptedComponentSerializer` (idempotent re-apply)
- [ ] Server-side validation of real-estate mutations (currently client-trusted — see Known Issues)
- [ ] Warehouse privacy/linking UI (flags exist but have no setter anywhere)

---

## Current Architecture

### Key Components

| File | Role |
|---|---|
| `Scripts/Game/GameMode/Managers/OVT_RealEstateManagerComponent.c` (918 lines) | The manager: ownership, warehouses, homes, prices, JIP, RPCs |
| `Scripts/Game/GameMode/Managers/OVT_OwnerManagerComponent.c` (446 lines) | Generic position-keyed ownership/rental base + JIP |
| `Scripts/Game/GameMode/Managers/OVT_RplOwnerManagerComponent.c` | Parallel RplId-keyed sibling — used by the *vehicle* manager, NOT real estate |
| `Scripts/Game/Configuration/OVT_RealEstateConfig.c` | Building type/price/rent/warehouse config class |
| `Scripts/Game/Components/OVT_SpawnPointComponent.c` | `PointInfo` home-spawn offset on house prefabs |
| `Scripts/Game/Persistence/Serializers/Components/OVT_RealEstateManagerSerializer.c` | Vanilla persistence serializer + `OVT_PersistedOwnership`/`OVT_PersistedWarehouse` DTOs |
| `Scripts/Game/UI/Context/OVT_RealEstateContext.c` | Buy/Sell/Rent/StopRenting/SetAsHome menu with player/resistance account spinner |
| `Scripts/Game/UI/Context/OVT_WarehouseContext.c` + `UI/Menu/WarehouseMenu/` | Warehouse take-to-vehicle UI |
| `Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c:268-350, 864-899` | Client→server asks (home, owner, renter, warehouse) |
| `Scripts/Game/UserActions/OVT_SetHomeAction.c` | Set-home action at FOB/camp |

Config data: six `OVT_RealEstateConfig` instances on `Prefabs/GameMode/OVT_OverthrowGameMode.et:84-140` — Starting House (25k/1k), Small House (32k/1.5k), Medium House (48k/1.8k), Large House (60k/2.2k), Villa (120k/4.5k), Warehouse (150k/2.5k, `m_IsWarehouse 1`).

### Data Flow

**Building identity is a position string** — `building.GetOrigin().ToString(false)` — held in four hand-maintained maps on the base class (`OVT_OwnerManagerComponent.c:11-14`): `m_mOwned`/`m_mRented` (persistentId → posKeys) plus inverse indexes `m_mOwners`/`m_mRenters`. Position→entity resolution goes through `GetNearestBuilding(pos, range=40)` — a sphere query filtered to the literal class name `"SCR_DestructibleBuildingEntity"`.

**Buy flow (end to end):**
```
OVT_RealEstateContext.Buy()                 (client: price check + TakePlayerMoney!)
  → OVT_Global.GetServer().SetBuildingOwner(playerId, building)
    → RpcAsk_SetBuildingOwner(playerId, pos)          (PlayerComms — no validation)
      → [server] SetOwner → DoSetOwner + RpcDo_SetOwner broadcast
      → if warehouse-typed → create/find warehouse record + broadcast
```

**Three replication paths:** reliable broadcast RPCs for live deltas; `RplSave`/`RplLoad` for JIP (owners, renters, then warehouses); the serializer for disk (deliberately no RPC on load — clients get state via JIP).

**Warehouses** are virtual inventories (`ResourceName → count` maps in `OVT_WarehouseData`), not entity storage. Items are deleted from real storage on deposit and re-spawned on withdrawal. The warehouse `id` **is** its index into `m_aWarehouses` — the serializer deliberately does not persist it to defend that invariant. Warehouse records materialise implicitly the first time a warehouse-typed building gets an owner (matched by 10 m proximity).

**Home is not real-estate state** — it is `OVT_PlayerData.home` (a vector), persisted by the *player* manager serializer. The real-estate manager only mutates/reads it (`SetHome`/`SetHomePos`/`GetHome`/`IsHome`).

### Integration Points

- **market (economy manager):** all money movement (`TakePlayerMoney`, resistance-fund variants) — currently called client-side from `OVT_RealEstateContext`; `UpdateRents()` in `OVT_EconomyManagerComponent.c:223-270` runs at midnight (rent income to owner-renters, payments/eviction for tenants).
- **Towns:** price formula reads `town.population` and `town.stability`; `OVT_TownManagerComponent` uses `BuildingIsOwnable`/`IsOwned` for unowned-house queries. Circular-init risk between the two managers' `OnPostInit` caching (defensively re-fetched in `NewStartingTown()`).
- **Game mode:** `DoPostLoad` → `OnPostLoad` builds the starting-home pool map-wide *after* deserialization so restored ownership is honoured; `FinalizePlayerPreparation` assigns starting house + car or falls back to bus stop.
- **Spawning:** `OVT_PersistentRespawnLogic` spawns at `GetHome`; `OVT_SpawnLogic.FindSafeSpawnLocation` walks owned houses → safe town → hardcoded fallback.
- **Placement/limits:** place-near-owned-house rule (`MAX_HOUSE_PLACE_DIS` = 30 m) in `OVT_PlaceContext` and `OVT_ItemLimitChecker`.
- **Map/UI:** owned/rented icons, fast-travel-to-house rule (`OVT_MapContext.CanFastTravel`), warehouse HUD hint in vehicles.
- **Persistence:** serializer bound in `Configs/Systems/Persistence/Overthrow.conf:32`.

---

## Implementation Details

### Phase 1: Core Ownership (COMPLETED)
Position-keyed buy/sell/rent on `OVT_OwnerManagerComponent`, config-driven building typing by prefab-name substring, demand-scaled pricing:
```
price = base + base * (demandMultiplier * town.population * (town.stability / 100))
```
(warehouses bypass demand and use flat base price/rent).

### Phase 2: Homes & Starting Houses (COMPLETED)
Starting-house pool from `m_aStartingHouseFilters` (map-wide query at post-load, excluding furniture/ignored towns/owned buildings), random starting town + house assignment, home-driven respawn, fast travel, starting car spawn.

### Phase 3: Warehouses (COMPLETED)
Virtual inventories with deposit-from-vehicle (`OVT_Global.TransferToWarehouse`) and take-to-vehicle flows, JIP replication, linked-warehouse aggregation (`GetWarehouseInventory`).

### Phase 4: Vanilla Persistence Migration (COMPLETED, `28c121b`)
EPF `OVT_RealEstateSaveData` (stored vectors, re-derived keys by proximity) replaced with `OVT_RealEstateManagerSerializer` storing **position key strings verbatim** (avoids float round-tripping), with idempotent appliers (`ApplyPersistedOwner/Renter/Warehouses`) safe for live re-apply.

### Phase 5: Potential Improvements (NOT STARTED)
See Future Enhancements.

---

## Key Technical Decisions

### Decision 1: Position-string keys instead of EntityID/RplId
**Context:** Neither EntityID nor RplId is stable across sessions; commit history shows the RplId approach was tried, reverted, and retried (`10779db` → `00fd6f4` → `edb5094` → `31cbbc4 Use strings for owned houses`). `OVT_RplOwnerManagerComponent` is the surviving RplId variant, still used by vehicles.
**Implementation:** `vector.ToString(false)` as map key; `GetNearestBuilding(pos, 40)` to resolve back.
**Trade-offs:** Stable across save/network; but two hand-maintained inverse maps, a 40 m sphere query per reverse lookup, and ambiguity if two ownable buildings sit within range.

### Decision 2: Config-driven building typing by prefab-name substring
**Context:** Zero world edits needed — types live on the game-mode prefab.
**Trade-offs:** O(types × filters) string scan per lookup, including inside world-query callbacks.

### Decision 3: Warehouses as virtual inventories
**Context:** Solves persistence and entity-streaming for large caches.
**Trade-offs:** Items lose individual state (durability, attachments, magazine contents).

### Decision 4: Serializer stores key strings verbatim; no RPC on load
**Context:** The old EPF path re-derived keys from vectors by proximity, requiring buildings to exist and reintroducing float error. Load-order is handled by `DoPostLoad` running after deserialization.

---

## Current State

### What's Working
- Buy/sell/rent/rent-out with player or resistance funds; midnight rent processing
- Starting-home assignment, home respawn, fast travel, starting car
- Warehouse deposit/withdraw, linked aggregation, persistence round-trip
- Save/reload of ownership + warehouses (covered by the persistence gate suites)

### Known Issues (found during discovery, verified against source)

**Bugs:**
1. **JIP warehouse desync** — `RplSave` writes `inventory.Count()` but loops `m_aWarehouses.Count()` (`OVT_RealEstateManagerComponent.c:762-767`); `RplLoad` mirrors the mismatch (`:799-805`). Any server with ≠1 warehouse corrupts the joining client's whole RplLoad stream.
2. **`UpdateRents()` early-`return`** where it means `continue` (`OVT_EconomyManagerComponent.c:243-256`) — first resistance-owned/rented property aborts rent processing for every remaining renter that night.
3. **Unguarded map access** — `m_mOwned[m_sPlayerID].Count()` without `Contains` in `OVT_RealEstateContext.c:89, 235`; players who own nothing hit it on menu open.
4. **`IsHome()` inconsistent with `SetHome()`** — SetHome stores the spawn-point-offset position; IsHome compares building origin within 1 m (`:570-577`). Houses with `OVT_SpawnPointComponent` are never recognised as home (wrong button matrix; `Sell` can sell your home).
5. **`SetAsHome` ignores the building** — uses player's standing position (`OVT_RealEstateContext.c:349`); the correct `SetBuildingHome` path exists with zero callers.
6. **Starting-home filter double-insert** — `continue` targets the inner filter loop (`OVT_RealEstateManagerComponent.c:88`); prefabs matching two filters enter the pool twice.
7. **Missing null guards** — `IsRenter`/`IsOwned`/`IsRented` lack the `!building` guard `IsOwner` has (`OVT_OwnerManagerComponent.c:132-174`).
8. **`realEstateCostMultiplier` is dead config** — declared, set in all three difficulty confs, replicated — never read by any price function.
9. **`OVT_InventoryManagerComponent.PerformWarehouseTransfer` is a stub** (`:622-634`) — half-migrated duplicate of the working `OVT_Global.TransferToWarehouse` path.
10. **Warehouse take buttons miswired** — `take10`/`take100` handlers are all fetched from `take1` (`OVT_WarehouseContext.c:14-24`); only Take-1 works.

**Security/authority:**
- No server-side validation on any real-estate mutation ask (`RpcAsk_SetBuildingOwner` etc., `OVT_PlayerCommsComponent.c:295-350, 864-887`); money is debited **client-side** before the ask. The sibling shops feature validates server-side (`RpcAsk_Buy` recomputes cost + checks funds) — divergent patterns.
- `RpcAsk_SetBuildingOwner` takes `playerId` from the caller instead of deriving it from the RPC sender.

### Technical Debt
- Four hand-maintained ownership maps with no invariant enforcement; ~4 copies of "find-or-create warehouse within 10 m"; `ApplyPersistedOwner`/`Renter` are near-identical 30-line clones.
- `GetConfig` / `BuildingIsOwnable` duplicate the same loop (`BuildingIsOwnable` could be `GetConfig(e) != null`).
- `"SCR_DestructibleBuildingEntity"` string literal in 6 places — a vanilla rename silently kills the feature.
- Warehouse accessibility rule triplicated (`OVT_VehicleMenuContext.c:65, 155`, `OVT_EconomyInfo.c:169`).
- `isPrivate`/`isLinked` have no setter anywhere — persisted, replicated, read, never written (unfinished feature; camps have the fully-wired equivalent).
- Dead code: `TeleportHome` (no callers), `SetBuildingHome` (no callers), `s_Instance` never cleared on teardown.
- Two divergent owner-manager bases (~80% duplicated API, different bug-fix histories).
- Sell refunds 100% of current demand-scaled buy price — risk-free profit from stabilising a town after buying.
- Deprecated town attributes still consumed by `GetRandomUnownedHouse` paths.

---

## Future Enhancements

### High Priority
- [ ] Fix the JIP warehouse loop-bounds bug (#1) — multiplayer-corrupting
- [ ] Fix `UpdateRents()` early-return (#2)
- [ ] Move money debit + validation server-side for all real-estate asks (align with shops' pattern)

### Medium Priority
- [ ] Fix `IsHome`/`SetAsHome` inconsistency (#4/#5) via the existing `SetBuildingHome` path
- [ ] Wire warehouse privacy/linking UI (or remove the flags)
- [ ] Fix warehouse take-button wiring (#10) and unguarded map access (#3)
- [ ] Honour (or remove) `realEstateCostMultiplier`

### Low Priority / Nice to Have
- [ ] Consolidate the two owner-manager bases; dedupe warehouse find-or-create logic
- [ ] Sell-price depreciation
- [ ] Centralise the warehouse accessibility rule

---

## Testing

### Current Coverage
- `OVT_TEST_InitSuite.c:66` — manager resolves.
- `OVT_TEST_Persistence_RealEstateOwnership_RoundTrips` (`OVT_TEST_PersistenceSuite.c:340-427`) — all four maps agree after set/remove (designed to catch a migration rebuilding one map but not the others).
- `OVT_TEST_PersistenceRoundTrip_RealEstateOwnership_SurvivesSaveAndReload` (`OVT_TEST_PersistenceRoundTripSuite.c:850-1010`).
- `OVT_TEST_PersistenceSubject.ResolveUnownedBuilding()` helper avoids colliding with the randomized starting home.

### Testing Gaps
- **Warehouses: zero coverage** (add/take, id≡index invariant, linking, persistence round-trip, 10 m proximity matching).
- Renting (`SetRenter`/`IsRenter`/`UpdateRents` income/payment/eviction) — untested; bug #2 would be caught by a Logic-tier test.
- Pricing (`GetBuyPrice`/`GetRentPrice`/`GetConfig`/`BuildingIsOwnable`) — Logic-tier candidates.
- Home mechanics, starting-home pool, JIP `RplSave`/`RplLoad` (bug #1 lives here), idempotent double-apply.

---

## Documentation

### Current Documentation
- This retrospective; serializer header comments document the format rationale.

### Documentation Needs
- Warehouse invariants (id≡index, implicit creation) belong in the enforcescript-patterns skill if reworked.

---

## Dependencies

### External Dependencies
- Vanilla `SCR_DestructibleBuildingEntity` (by literal class-name string), `QueryEntitiesBySphere`, `SCR_PersistenceSystem` (via `core/persistence`).

### Internal Dependencies
- `economy/market` — money APIs, `UpdateRents` driver
- Towns manager — population/stability for pricing, house queries
- Player manager — persistent IDs, `OVT_PlayerData.home` storage/persistence
- `core/persistence` — serializer registration
- Vehicle manager — starting car at home (needs `OVT_ParkingComponent` on house prefabs)

---

## Notes

**Discovered Information:**
- Commit chronology: `82b9df4 Real Estate` → warehouses (`1f73cad`, `3dc287c`, `5505ba5`) → resistance-funds purchases (`6e53670`) → RplComponent removal saga → `31cbbc4 Use strings for owned houses` → `28c121b` EPF→vanilla serializer migration.
- Home deliberately decoupled from ownership — players can be homed where they don't own (bus-stop fallback, safe-spawn re-home).
- `DoPostLoad` ordering (starting-home pool built *after* deserialization) is load-bearing and documented in the serializer header.

**Retrospective Assessment:**
- The position-key design is battle-tested and the persistence migration around it is clean and idempotent.
- The weakest areas are multiplayer trust (client-side money, unvalidated asks) and warehouse code (JIP bug, no tests, unfinished privacy/linking).

---

*This retrospective plan was created by analyzing existing code. Use `/start-feature economy/real-estate` to begin making improvements.*
