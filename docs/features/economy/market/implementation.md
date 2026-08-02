# Market - Implementation Plan (Retrospective)

**Status:** Implemented (Documented Retrospectively)
**Originally Implemented:** Grown over the project's life; EPF→vanilla persistence migration `28c121b` (2026-08-02)
**Documented:** 2026-08-02
**Last Updated:** 2026-08-02 21:40

---

## Executive Summary

The market is Overthrow's central economy: the resource/price database, dynamic pricing, the hourly economic simulation (income, NPC purchases, restocking, rents), and player/resistance money. It all lives in one manager — `OVT_EconomyManagerComponent` (1724 lines) on the game mode — with `OVT_PlayerCommsComponent` as the client→server RPC gateway for every mutation.

**Note:** This is a retrospective implementation plan created by analyzing the existing codebase. The feature has already been implemented.

---

## Goals

### Primary Goals
- One persistent currency: per-player wallets plus a shared resistance treasury with a configurable tax split.
- Config-driven prices for every item and vehicle, modulated by town stock, port distance, shop margin and player skills.
- A living simulation: towns generate income and NPC demand on a game-time clock.

### Success Criteria
- [x] Player and resistance money with server authority and broadcast streaming
- [x] Resource database + price cascade built from configs and faction catalogs
- [x] Income/tax/donation simulation driven by town population, stability and support
- [x] Resistance treasury persists via vanilla serializer; player money via player manager
- [ ] Shop restocking actually works (`ReplenishStock` is dead code — see Known Issues)
- [ ] Server-side validation of the sell/money paths (currently client-trusted)

---

## Current Architecture

### Key Components

| File | Role |
|---|---|
| `Scripts/Game/GameMode/Managers/OVT_EconomyManagerComponent.c` (1724 lines) | Resource DB, prices, demand, stock loops, income, wallets, port & shop registry, JIP |
| `Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c` (1475 lines) | **The RPC gateway** — every client→server economy mutation (`Buy` :354, `ImportToVehicle` :464, `BuyVehicle` :502, money :596-673) |
| `Scripts/Game/Configuration/OVT_PricesConfig.c` / `OVT_VehiclePricesConfig.c` | Price config classes (type/mode/find-string/cost/demand/hidden; vehicles add prefab/illegal/parking) |
| `Scripts/Game/Configuration/OVT_DifficultySettings.c:51-77` | The Economy attribute block (startingCash, respawnCost, taxIncome, donationIncome, multipliers…) |
| `Configs/Pricing/itemPrices.conf` / `vehiclePrices.conf` | The price tables (cascade, broad→narrow) |
| `Scripts/Game/Persistence/Serializers/Components/OVT_EconomyManagerSerializer.c` | Persists **only** `m_iResistanceMoney` + `m_fResistanceTax` |
| `Scripts/Game/Persistence/Serializers/Components/OVT_PlayerManagerSerializer.c` | Where player money actually persists (`OVT_PersistedPlayer.money`) |
| `Scripts/Game/Data/OVT_PlayerData.c:14,45` | The wallet storage (`money`) + `[NonSerialized]` `priceMultiplier` (skills) |
| `Scripts/Game/UI/HUD/OVT_EconomyInfo.c` | HUD: money, town panel, QRF, notifications |
| `Scripts/Game/Controllers/OVT_PortController.c` | Empty marker component — the only thing that makes an entity a port |
| `Scripts/Game/UI/Context/OVT_ResistanceMenuContext.c` | Treasury/tax/donation UI |

Configs are bound to the manager on `Prefabs/GameMode/OVT_OverthrowGameMode.et:10-19`.

### Data Flow

**The resource-ID system (the single most important design decision):** `BuildResourceDatabase()` (`:1061-1240`) builds `m_aResources`; **a resource's ID is its index in that array.** Everything — prices, demand, shop stock, RPC payloads, JIP — is keyed by this int. Build order (= ID assignment) is load-bearing: explicit-prefab vehicles, then per-faction inventory items and vehicles, then gun-dealer prefabs. Price resolution walks `m_aPrices` sequentially with **later entries overriding earlier ones** (why `itemPrices.conf` reads broad→narrow); `hidden` drops the item. The DB is built **independently on every machine** — client and server must produce identical ordering or every int in every shop RPC means a different item. Nothing validates this.

**Price model:**
```
GetSellPrice(id, pos)  = price + (1 - stock/maxStock)*price*0.1        (scarcity, ≤+10%)
                               + price * distToNearestPort * 0.0001    (+1%/100m)
                         (vehicles: flat price)
GetBuyPrice            = Round(sell * (1 + m_fShopProfitMargin=0.25)) * player.priceMultiplier
GetShopBuyPrice        = procurement ? price * procurementMult * vehicleMult : GetBuyPrice [* vehicleMult]
GetTownMaxStock        = Round(1 + population * m_fNPCBuyRate(0.1) * demand * stability/100)
```
Note: EnforceScript picks division mode from the *target* type — `stability/100` inside `Math.Round(...)` is float division. Measured, not assumed (`docs/features/dev-ops/test-coverage/findings.md:1120-1180`); the suspected truncation bugs were proven not to exist.

**The economic clock — `CheckUpdate()` (`:140-190`):** server-only, every ~10s real time, **frozen when `GetPlayerCount() == 0`**. Game hours 0/6/12/18 → `CalculateIncome()` + `UpdateShops()` (NPC demand simulation); hour 7 → `ReplenishStock()`; hour 0 → `UpdateRents()`. Idempotent via hour sentinels. Income = donations (`donationIncome * support`, doubled above 75 stability, all towns) + taxes (`taxIncome * population * stability/100`, occupied towns exempt); taxed share to the resistance, remainder split equally across online players.

**Money:** player money lives in `OVT_PlayerData.money` (player manager) — the economy manager is only the API + streaming layer. Every mutator follows `public wrapper → if IsServer: Do*() else GetServer().X()`; no `RplProp` anywhere — state moves by `RplSave`/`RplLoad` (JIP: treasury, tax, shop registry, gun dealers) plus explicit reliable broadcast RPCs (`RpcDo_SetPlayerMoney` sends every balance to every client).

**Events (`ScriptInvoker` seams):** `m_OnPlayerMoneyChanged` (HUD/menus), `m_OnResistanceMoneyChanged`, `m_OnPlayerBuy`/`m_OnPlayerSell` (skill XP), `m_OnPlayerTransaction` (stability/support modifiers — buy only).

### Integration Points

- **shops:** registration/discovery (whole-world query for `SCR_DestructibleBuildingEntity` + `OVT_ShopComponent`, excluding procurement/gun dealers), `InitShopInventory`, `GetShopBuyPrice`, NPC sales.
- **real-estate:** `UpdateRents()` midnight pass; property prices via towns.
- **Towns:** population/stability/support feed income, stock caps and pricing; gun dealers spawned by `OVT_TownController`.
- **Skills:** `OVT_TradeDiscountSkillEffect` sets `player.priceMultiplier`; buy/sell XP.
- **Modifiers:** strong-economy (>$50, non-gun-dealer) and black-market (>$1000, gun dealer) subscribe to `m_OnPlayerTransaction`.
- **Everything with a cost** calls into here: respawn charge, fast travel, bus fares, recruiting, placeables/buildables, vehicle upgrades, job rewards, port imports.
- **Persistence:** serializer registered in `Configs/Systems/Persistence/Overthrow.conf:23-26`; deliberately no RPC on load (JIP covers clients).

---

## Implementation Details

### Phase 1: Resource DB & Pricing (COMPLETED)
Config-cascade pricing over faction arsenal catalogs, int-ID resource database, Workbench custom titles for config readability.

### Phase 2: Money & Replication (COMPLETED)
Player/resistance wallets, wrapper→Do* server-authority pattern, comms-component RPC gateway with anti-double-spend latches, JIP payload.

### Phase 3: Economic Simulation (COMPLETED)
Game-time clock, income/tax/donation calculation, NPC purchase simulation, rent processing, (intended) restocking.

### Phase 4: Vanilla Persistence Migration (COMPLETED, `28c121b`)
EPF `OVT_EconomySaveData` deleted; `OVT_EconomyManagerSerializer` persists treasury + tax with a version guard and an exemplary header comment. Player money rides `OVT_PlayerManagerSerializer` (v2).

### Phase 5: Potential Improvements (NOT STARTED)
See Future Enhancements.

---

## Key Technical Decisions

### Decision 1: Int resource IDs on the wire
**Context:** Massive bandwidth win over `ResourceName` strings.
**Trade-offs:** IDs are array indices with no stability guarantee — client/server must build identical DBs (unvalidated), and shop stock can never be persisted without a name-keyed format.

### Decision 2: All client→server mutation through `OVT_PlayerCommsComponent`
**Context:** The comms component is per-player-controller, so `RplRcver.Server` is well-defined; the manager itself only broadcasts. `OVT_Global.GetServer()` returns the game-mode copy on the server, the local player's copy on clients — which is why server code can call the same wrappers.

### Decision 3: No `RplProp`; JIP snapshot + explicit broadcast deltas
**Context:** Consistent with the rest of Overthrow; positional binary `RplSave`/`RplLoad`.

### Decision 4: Game-time-driven simulation, frozen on empty servers
**Context:** `CheckUpdate` returns immediately at zero players — world state is player-presence-coupled. Deliberate, previously undocumented.

### Decision 5: Config cascade with later-overrides-earlier semantics
**Context:** Lets one broad rule hide/misc-price whole categories and narrow rules refine; unmatched items fall back to Conflict's `GetSupplyCost` with a log line (the source of BUG-009).

---

## Current State

### What's Working
- Wallets, treasury, tax split, income simulation (well-tested: `IncomeMatchesTownState` is the strongest case in the tree)
- Price cascade + buy-price seams (Init-tier tested); persistence round-trips for player money
- NPC demand draining stock; JIP payload; ports registry; gun-dealer registration

### Known Issues (found during discovery, verified against source)
1. **`ReplenishStock` never restocks regular shops — dead code.** `half = Round(stock*0.5); if(stock < half)` is never true (`OVT_EconomyManagerComponent.c:304-308`); almost certainly meant `max*0.5`. Town shop stock only ever decreases after the initial roll. Gun-dealer weed restocking works; weapons don't restock.
2. **`ReplenishStock` iterates all shops once per town** (`:292`) with the wrong town's `GetTownMaxStock`; computed `max` never used.
3. **`UpdateRents` early-`return`** aborts the whole rent pass on the first resistance-owned/rented property (`:243-256`) — shared with real-estate.
4. **`InitShopInventory` drops all items when `m_bIncludeOtherFactionItems` is false** (`:1482`, duplicated in `OVT_TownController.c:310,326`) — flag semantics inverted; masked because shipped configs use the default `true`.
5. **`GetInventoryId` has no `Contains` guard** (`:1418`) — unknown ResourceName returns 0, a *valid wrong* item; downstream guard `if(id > -1)` can never fail.
6. **Vehicle default-price warning unreachable** — default 500000, diagnostic checks `== 50000` (`:1181`/`:1212`); unmatched vehicles silently cost 500k.
7. **`doEvent` dropped on the server path** (`AddPlayerMoney:794-802`) — no selling XP in single-player/listen-host.
8. **Sell is client-authoritative** — `OVT_ShopContext.Sell` deletes locally then asks for money with no server verification (also `OVT_SellDrugsAction`); a modified client can mint money. Buy is correctly server-validated.
9. **`m_OnPlayerTransaction` never fires for sells** (`PlayerComms:453`, always `isBuying=true`) — the modifiers' `isBuying` param is dead.
10. **Anti-glitch latches can deadlock** — `addingMoney`/`takingMoney` cleared only by an owner round-trip RPC; one dropped RPC permanently blocks that client's money ops.
11. **`RpcDo_SetPlayerMoney` broadcasts every balance to every client**; combined with BUG-016 (stale runtime IDs), a departed player's ID can resolve to a different live player.
12. **`ChargeRespawn` hardcodes `money > 500`** while the cost is `difficulty.respawnCost` (default 5) (`:1577`).
13. **`GetTownStock` dereferences shop without null check** (`:550-560`), reachable from `GetSellPrice` on every priced lookup.
14. **Port imports bypass all pricing** — raw `GetPrice`, no margin/distance/multiplier (`OVT_PortContext`, `RpcAsk_ImportToVehicle`) — always the cheapest source, by design or accident.
15. Invoker doc-comments don't match reality (documented 4 args, invoked with 2 — `e7978e5` Gemini-generated docs drifted); `Init()` has two no-op self-assignments (`:1023-1024`); `OVT_EconomyInfoWidgets.c` is dead code (widget names don't exist in the layout); stale refactor comment `:1237-1239`; `GetAllNonOccupyingFactionVehiclesByParking` has zero call sites.
16. **BUG-013 (filed):** `QRFPointsToWin` read client-side at `OVT_EconomyInfo.c:273` but never replicated — HUD QRF bar wrong for non-host clients on non-Normal difficulty.

### Technical Debt
- **1724-line god object.** Natural seams: `OVT_ResourceCatalog` (DB/IDs/faction buckets), `OVT_PriceCalculator` (pure functions, trivially testable), `OVT_MarketSimulation` (hourly loops), `OVT_Treasury` (wallets).
- Pricing logic duplicated in three places (`GetShopBuyPrice`, `RpcAsk_BuyVehicle`, `OVT_ShopContext`) and already drifted (gun-dealer sell multiplier is UI-only).
- No client/server resource-DB checksum in the JIP payload.
- Shop stock and prices/demand not persisted (blocks market-manipulation persistence and dynamic price drift).
- Magic numbers (`0.1`, `0.0001`, `500`) belong in difficulty settings.

---

## Future Enhancements

### High Priority
- [ ] Fix `ReplenishStock` (dead code + wrong loop) — restocking has never worked
- [ ] Server-side validation for sell/money paths (kill the money-minting exploit)
- [ ] Resource-DB checksum in JIP; refuse/log on mismatch

### Medium Priority
- [ ] Fix `UpdateRents` early-return; fix `m_bIncludeOtherFactionItems` semantics
- [ ] Persist shop stock via a resource-name-keyed serializer
- [ ] Fix `doEvent` drop (SP sell XP); fire `m_OnPlayerTransaction` on sells or remove the param
- [ ] Unify port pricing with the shop model (or document it as intended)

### Low Priority / Nice to Have
- [ ] Split the god object along the four seams above
- [ ] Replace anti-glitch latches with server-side idempotency
- [ ] Delete dead code (`OVT_EconomyInfoWidgets`, unused APIs, `SHOP_FOOD`-adjacent leftovers)

---

## Testing

### Current Coverage
- `OVT_TEST_Campaign_Economy_ShopsInitialise` — ≥1 registered shop stocked (content-robust assertion).
- `OVT_TEST_Campaign_Economy_IncomeMatchesTownState` — the strongest case in the tree: drives town 0 through occupied/liberated/supporters/lowered-stability and derives expectations from live town records + difficulty config; proved the stability factor is fractional.
- `OVT_TEST_Init_Economy_PriceAndDemandSeams` — unknown-ID defaults, price/demand round-trips, buy-price formula from config.
- Persistence: `PlayerMoney_RoundTrips` + `PlayerMoney_SurvivesSaveAndReload` (mutate → save → dirty → reload).

### Testing Gaps
- `GetSellPrice` dynamic terms (scarcity, port distance) — zero assertions (recorded in test-coverage findings as "reassessed, not proven").
- `GetShopBuyPrice` procurement/vehicle branches; `ReplenishStock`; `UpdateShops`; `UpdateRents`; the tax split & per-player distribution; resistance treasury round-trip; `BuildResourceDatabase` cascade/hidden/faction bucketing; JIP payload & client/server ID agreement; `ChargeRespawn`/`ImportToVehicle`/`BuyVehicle`/gun-dealer restock.

---

## Documentation

### Current Documentation
- This retrospective; `OVT_EconomyManagerSerializer.c` and `OVT_PlayerManagerSerializer.c` headers are the quality bar.

### Documentation Needs
- The resource-ID determinism requirement and the "economy frozen when empty" decision deserve a note in the enforcescript-patterns or overthrow-architecture skill.

---

## Dependencies

### External Dependencies
- Faction arsenal catalogs (`SCR_ArsenalItem`, `SCR_NonArsenalItemCostCatalogData`), Conflict `GetSupplyCost` fallback, `TimeAndWeatherManagerEntity`.

### Internal Dependencies
- Towns manager (population/stability/support), player manager (wallet storage + persistent IDs), `core/config` (difficulty settings), `core/persistence` (serializers), time handler (`GetDayTimeMultiplier`).

---

## Notes

**Discovered Information:**
- The EPF save data (`OVT_EconomySaveData`) persisted exactly the same two fields as today's serializer — the scope was always treasury + tax.
- `GetDonationIncome` counts occupied towns while `GetTaxIncome` exempts them — possibly intentional (underground support), definitely undocumented.
- Ports are identified solely by an empty marker component.

**Retrospective Assessment:**
- Income simulation and its test are excellent; the wrapper→Do*→Stream pattern is consistent and MP-safe for buys.
- The weakest areas: restocking has silently never worked, the sell path trusts clients, and the int-ID determinism assumption is unguarded.

---

*This retrospective plan was created by analyzing existing code. Use `/start-feature economy/market` to begin making improvements.*
