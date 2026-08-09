# Shops - Implementation Plan (Retrospective)

**Status:** Implemented (Documented Retrospectively)
**Originally Implemented:** Grown over the project's life
**Documented:** 2026-08-02
**Last Updated:** 2026-08-02 21:40

---

## Executive Summary

Shops are the physical, entity-level retail layer of the economy epic. A shop is any entity carrying `OVT_ShopComponent` (a 108-line stock map); it is discovered/registered at world load by the economy manager, opened via a user action on a cashier prop (or the main-menu proximity override for garages), and mutates money + stock through server RPCs on `OVT_PlayerCommsComponent`. Seven shop types collapse into three behavioural families: town item shops, the per-town spawned gun dealer, and vehicle/procurement shops. Shop stock is **not persisted** — it is re-rolled from config every campaign start.

**Note:** This is a retrospective implementation plan created by analyzing the existing codebase. The feature has already been implemented.

---

## Goals

### Primary Goals
- Physical shops in towns where players buy and sell items, with stock that NPCs also consume.
- Gun dealers: one per (non-village) town, spawned at runtime with randomized black-market stock.
- Vehicle purchase at fuel stations and cost-discounted procurement at owned garages/helipads.

### Success Criteria
- [x] Config-driven shop inventories as declarative catalog queries (not explicit item lists)
- [x] Server-authoritative buy flow (server recomputes price, checks funds, delivers items)
- [x] JIP replication of stock and registry
- [ ] Server-authoritative sell flow (currently client-trusted — exploitable)
- [ ] Shop stock persistence (blocked on resource-name-keyed format; see market feature)
- [ ] Shop ownership revenue (`HandleNPCSale` has a To-do stub)

---

## Current Architecture

### Key Components

| File | Role |
|---|---|
| `Scripts/Game/Components/Economy/OVT_ShopComponent.c` (108 lines) | The per-shop model: type, procurement flag, town id, `map<int,int>` stock, JIP, per-item broadcast |
| `Scripts/Game/Configuration/OVT_ShopConfig.c` | Per-shop-type inventory rules (`OVT_ShopInventoryItem` itself lives in the economy manager file) |
| `Scripts/Game/Configuration/OVT_GunDealerConfig.c` | Gun dealer stock: catalog rules + explicit prefabs |
| `Scripts/Game/UI/Context/OVT_ShopContext.c` | The shop menu (buy/sell/pagination), gun-dealer sell multiplier, procurement vehicle list |
| `Scripts/Game/UI/Menu/ShopMenu/OVT_ShopMenuCardComponent.c` | One grid card (reused by `OVT_ManageVehicleContext`) |
| `Scripts/Game/UserActions/OVT_ShopAction.c` / `OVT_GunDealerAction.c` | Entry points (~95% duplicated; parent-vs-self `GetShop()`) |
| `Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c:352-593` | Server transaction block (`RpcAsk_Buy`, `RpcAsk_BuyVehicle`, inventory asks) |
| `Scripts/Game/Controllers/OVT_TownController.c:203-351` | Gun dealer spawn/stock/registration |
| `Configs/System/ShopConfig.conf` / `GunDealerConfig.conf` | The inventory rule tables |

**Shop types:** `SHOP_GENERAL`, `SHOP_DRUG`, `SHOP_CLOTHES`, `SHOP_ELECTRONIC` (town shops, world-query registered); `SHOP_GUNDEALER` (spawned per town); `SHOP_VEHICLE` ± `m_bProcurement` (fuel stations register; procurement garages/helipads deliberately don't). `SHOP_FOOD` is a dead enum value — no config, no prefab.

### Data Flow

**Registration (server):** whole-world query filtered to `entity.ClassName() == "SCR_DestructibleBuildingEntity"` with a non-procurement, non-gun-dealer `OVT_ShopComponent` → `m_aAllShops` (RplIds) + `m_mTownShops[townID]`, back-writing `shop.m_iTownId`. Consequence: procurement shops and gun dealers get no restock pass, no NPC sales, and don't count toward the town-stock price curve.

**Buy (server-authoritative):**
```
OVT_ShopAction (cashier prop, local-only) → OVT_ShopContext.SetShop + ShowContext
→ Buy: client guards (stock, funds) → GetServer().Buy/BuyVehicle
→ RpcAsk_Buy [server]: re-resolve shop by RplId, RECOMPUTE GetShopBuyPrice,
   check funds, spawn items (inventory → right-hand equip fallback → delete+break),
   charge only for delivered items, decrement stock, fire m_OnPlayerBuy/m_OnPlayerTransaction
→ stock delta broadcast via shop.StreamInventory → RpcDo_SetInventory
→ money broadcast via RpcDo_SetPlayerMoney → HUD/menu refresh
```

**Sell (client-authoritative — the weak path):** `OVT_ShopContext.Sell` computes the price locally, deletes the item locally, then `AddPlayerMoney` + `AddToInventory` asks with **no server verification** of item, shop, amount or proximity. The only mitigation is a client-side double-fire latch. The gun-dealer sell multiplier exists *only* in the UI.

**Gun dealer lifecycle:** `OVT_TownController.ActivateTown` → `SpawnGunDealer` (reuses persisted `town.gunDealerPosition`, else picks a `SHOP_GUNDEALER`-marked house or any unowned house) → spawns the dealer character prefab, stocks from explicit prefabs + catalog rules (`m_bSingleRandomItem` gives each town a different random weapon), neutralizes faction, `RegisterGunDealer`.

**Procurement entry:** no user action — `OVT_MainMenuContext.ShowLayout` does a 50 m query for `OVT_MainMenuContextOverrideComponent` and special-cases `m_ContextName == "OVT_ShopContext"` (string-typed coupling).

**NPC economy:** hourly-ish `UpdateShops` in the market feature drains stock per town (`population * NPCBuyRate * stability`), feeding scarcity pricing.

### Integration Points

- **market:** all pricing (`GetShopBuyPrice`/`GetBuyPrice`/`GetSellPrice`), registration, `InitShopInventory` stocking, NPC sales, `IsSoldAtShop` (used by the medical-supplies delivery action), `m_OnPlayerTransaction` → stability/support modifiers.
- **Towns:** `m_mTownShops` keyed by town ID; gun dealer owned by `OVT_TownController`; occupier patrols route past shops (`OVT_BaseUpgradeTownPatrol`).
- **Jobs:** `OVT_IsNearestTownWithShopJobCondition` reads `m_mTownShops`; `OVT_GetShopLocationJobStage` takes `[townID][0]` with no bounds check (guarded only by the paired condition).
- **Vehicles:** `OVT_ParkingComponent` gates stocking and spawn spots; `SpawnVehicleMatrix` sets the buyer's persistent ID as owner.
- **Map:** shop-type → icon mapping with failed-icon retry and cached positions for unstreamed entities.
- **Persistence:** only `town.gunDealerPosition` persists (via the town serializer) — dealer respawns in place with fresh random stock. No shop stock serializer exists (and never did, even under EPF).

---

## Implementation Details

### Phase 1: Town Shops (COMPLETED)
Shop component + prefab inheritance (base shop house, per-type overrides, furniture layer placing the cashier prop with the user action), config-driven stocking, buy/sell UI.

### Phase 2: Server-Authoritative Buy (COMPLETED)
`RpcAsk_Buy` with server price recomputation, partial-purchase handling, equip fallback for weapons, per-item stock broadcast.

### Phase 3: Gun Dealers & Vehicles (COMPLETED)
Per-town dealer spawn/stock/registration; fuel-station vehicle shops; procurement garages/helipads with difficulty-discounted pricing and base-ownership gate.

### Phase 4: Potential Improvements (NOT STARTED)
See Future Enhancements.

---

## Key Technical Decisions

### Decision 1: Shops never own mutation RPCs
**Context:** Every write funnels through `OVT_PlayerCommsComponent` (duplicated on game mode + player character; `OVT_Global.GetServer()` returns the right copy per side), so server code can call the same wrappers. Only the broadcast-only `RpcDo_SetInventory` lives on the shop.

### Decision 2: Declarative catalog-query inventories
**Context:** `OVT_ShopInventoryItem` rules (arsenal type/mode + prefab substring + faction flags) instead of explicit item lists — content updates flow through without config edits.
**Trade-offs:** Stock is keyed by the market's unstable int resource IDs, which is why it cannot be persisted as-is.

### Decision 3: Interaction decoupled from the shop entity
**Context:** The user action sits on a cashier furniture prop that must be a *child* of the shop building; `OVT_GunDealerAction` reads the component off the dealer itself.

### Decision 4: Gun dealers spawned, not placed
**Context:** Randomized per-town stock and location reuse via persisted `gunDealerPosition`.

---

## Current State

### What's Working
- Buy flow end-to-end (server-validated, partial purchases handled, money only for delivered items)
- Registration, stocking, JIP, NPC consumption, map icons, gun-dealer lifecycle
- Procurement discount path and base-ownership gating

### Known Issues (found during discovery, verified against source)
1. **Sell is client-authoritative** (`OVT_ShopContext.c:297-348`) — a modified client can mint money; `RpcAsk_AddToInventory`/`RpcAsk_TakeFromInventory` are likewise unvalidated. (Shared root cause with market issue #8.)
2. **No proximity/identity validation on shop RPCs** — `RpcAsk_Buy` trusts client-supplied `playerId` and never checks distance to the shop.
3. **Pagination is broken** (`OVT_ShopContext.c:146,170`): `Math.Ceil(Count()/15)` is integer division — 57 items → 12 items permanently unreachable; <15 items → `m_iNumPages == 0`, page label "1/0", and `NextPage` can drive an index of `GetKey(-15)`.
4. **Shops never restock** — market's `ReplenishStock` is dead code (market issue #1); after the initial roll, town stock only decreases.
5. **`m_bIncludeOtherFactionItems` semantics inverted** in `InitShopInventory` and both `OVT_TownController` stock loops — masked by default-true configs.
6. **`PurchaseFailedInventoryFull` is defined, localized and never sent** — `PlayerComms:389-394` is an empty `if` with a comment.
7. **Item IDs typed as `RplId`** in `TakeFromShopInventory`/`RpcAsk_TakeFromInventory`/`RplLoad` — works only because both are ints.
8. **Hardcoded prefab-GUID aliasing** in the sell path for weapon variants (AK74/GP25, RPG7/PGO7), self-flagged `//Chris - Make this work better for variants`.
9. **UI handler hygiene** — `OnShow` inserts button handlers every show with no teardown; `m_OnPlayerMoneyChanged.Insert` never removed; `Refresh` dereferences `OVT_ParkingComponent` without a null guard.
10. **`OVT_GetShopLocationJobStage`** indexes `m_mTownShops[townID][0]` with no bounds check.
11. **Dead code/data:** `SHOP_FOOD` enum value; `m_aAllVehicleShops` allocated and never used; `OVT_VehicleShopConfig` empty and unreferenced.
12. **Shop ownership is a stub** — `HandleNPCSale`'s `//To-do: Give player money if they own the shop`; no revenue path despite real-estate ownership existing.
13. **Brittle registration filter** — literal `"SCR_DestructibleBuildingEntity"` class-name compare; a vanilla refactor silently removes every shop from the economy (shared with real-estate).

### Technical Debt
- `OVT_ShopAction`/`OVT_GunDealerAction` ~95% duplicated (~60 lines).
- String-typed `m_ContextName` coupling in the main-menu override special-case.
- Client/server price duplication — gun-dealer sell multiplier UI-only means nothing enforces it if sell is ever hardened.

---

## Future Enhancements

### High Priority
- [ ] Server-authoritative sell (validate item, recompute price server-side including the gun-dealer multiplier)
- [ ] Fix pagination (float division + clamp) — items are currently unreachable in big shops
- [ ] Proximity/identity validation on all shop RPCs

### Medium Priority
- [ ] Persist shop stock (needs resource-name-keyed format; coordinate with market)
- [ ] Send `PurchaseFailedInventoryFull`; fix `m_bIncludeOtherFactionItems`
- [ ] Move weapon-variant aliasing into config/catalog data

### Low Priority / Nice to Have
- [ ] Shop ownership revenue (the `HandleNPCSale` To-do)
- [ ] Merge the two shop user actions; UI handler teardown; remove dead enum/values

---

## Testing

### Current Coverage
- `OVT_TEST_Campaign_Economy_ShopsInitialise` — registration + stocking end-to-end (≥1 stocked shop; measured 5 shops/286 entries/604 ms on 1.7.0.54).
- `OVT_TEST_Init_Economy_PriceAndDemandSeams` — buy-price seam from config.
- Gun-dealer *position* touched by campaign/town/jobs logic tests.

### Testing Gaps
- No test of `RpcAsk_Buy` (money deduction, stock decrement, partial path, equip fallback), the sell path, procurement/vehicle price branches, `UpdateShops` NPC consumption, shop JIP round-trip, registration filtering (procurement/dealer exclusion, fuel-station inclusion), or pagination arithmetic (issue #3 is a perfect Logic-tier candidate).

---

## Documentation

### Current Documentation
- This retrospective.

### Documentation Needs
- The comms-gateway pattern (Decision 1) is reusable knowledge for any new transactional feature — candidate for the enforcescript-patterns skill.

---

## Dependencies

### External Dependencies
- `SCR_DestructibleBuildingEntity` (literal class-name filter), arsenal catalogs, `SCR_InputButtonComponent`, `ItemPreviewWidget`.

### Internal Dependencies
- `economy/market` — all pricing, registration, stocking, NPC sales
- Towns manager + `OVT_TownController` — town scoping, gun-dealer lifecycle
- Vehicle manager + `OVT_ParkingComponent` — vehicle delivery
- `core/persistence` — only via the town serializer (`gunDealerPosition`)

---

## Notes

**Discovered Information:**
- The three-family collapse (town shops / gun dealer / vehicle-procurement) is the real architecture; the seven-value enum overstates it.
- Fuel stations are the only `SHOP_VEHICLE` that registers — they get 100 of every parking-compatible vehicle and participate in town stock; garages/helipads deliberately don't.
- No shop save data ever existed, even under EPF (`git log -S "ShopSaveData"` is empty).

**Retrospective Assessment:**
- The buy path is the pattern to copy: server recomputation, partial handling, honest charging.
- The sell path, pagination and never-working restock are the three user-visible weaknesses; stock persistence is the big campaign-feel gap.

---

*This retrospective plan was created by analyzing existing code. Use `/start-feature economy/shops` to begin making improvements.*
