# Shop UX — Requirements

**Epic:** economy
**Created:** 2026-08-04
**Addresses:** GitHub issue #145 (“Improve looting game loop by improving sell mechanic at shops”)

## Overview

Rework the shop menu (`OVT_ShopContext` + `ShopMenu.layout`) around two ideas: **category tabs** along the top of the browser, and a **split Buy/Sell mode** where Sell browses the *player's* unequipped inventory instead of the shop's stock. This implements issue #145's Option A (category grouping) plus Option B (alphabetical order within a category), and replaces the one-click-per-unit sell flow with per-item **Sell** / **Sell All** actions. A **“Sell All” trunk action on vehicles parked near a shop** (a returning Arma 3 Overthrow feature) rides on the same server-side bulk-sell routine. Options C/D from the issue (floor-drop / ammo-box selling) are deliberately not pursued — the sell tab solves the same pain without new world-state.

Categories are derived from data the economy manager already caches: every catalog entry carries `SCR_ArsenalItem` data with `SCR_EArsenalItemType` and `SCR_EArsenalItemMode` (`OVT_EconomyManagerComponent.m_aEntityCatalogEntries`). No price/shop config changes are required, and mod-added content categorizes itself.

## Requirements

### Category tabs (both modes)

- A row of **category tabs along the top** of the item browser, applied identically in Buy and Sell mode.
- Category is a **pure function of the item's catalog data** (`SCR_EArsenalItemType` + `SCR_EArsenalItemMode`), implemented in a UI-free helper class so the Logic test tier can cover the mapping. Proposed grouping (implementation may tune, but mags/scopes/weapons must land in distinct tabs — the core complaint of #145): Weapons; Ammunition (mode `AMMUNITION`); Attachments; Explosives & Throwables; Medical & Consumables; Clothing; Gear (backpacks/equipment); Other (no arsenal data — e.g. non-arsenal shop goods).
- An **“All” tab** is selected by default; **empty tabs are hidden** (a clothes shop shows no Weapons tab).
- **Items sort alphabetically by display name within a tab** (issue #145 Option B).
- Existing 15-card grid pagination is retained **per tab**; switching tab or mode resets to page 1 and clears the selection.
- Tabs are clickable buttons following the project's existing button/navigation pattern (mouse-wired buttons are gamepad-navigable per project findings); a dedicated controller tab-cycle binding is a nice-to-have, not a gate.

### Buy / Sell mode split

- The menu has explicit **Buy** and **Sell** modes with a visible toggle; Buy is the default and preserves current buy behavior (server-authoritative `RpcAsk_Buy` path unchanged).
- Sell mode is **unavailable (toggle hidden)** where selling is today: vehicle/procurement shops, and gun dealers when `gunDealerSellPriceMultiplier == 0` (current `ShouldShowSellButton` logic).

### Sell mode

- Sell mode lists the **player's own inventory**, not the shop's stock: every item held in containers — i.e. **not currently equipped** (no worn clothing, holstered/hands weapons, or other loadout-slot items; contents *inside* a worn backpack/vest count as containers and are sellable).
- Items are **aggregated by economy resource id** (each weapon variant is its own id), showing preview image, display name, **owned quantity**, and **unit sell price** (gun-dealer multiplier applied where relevant), under the same tabs/sort as Buy mode.
- **Items the shop does not buy still appear, grayed out**: selectable, details pane shows a localized “this shop doesn't buy this item” message, and the sell buttons are disabled. “Shop buys it” is decided by `OVT_EconomyManagerComponent.IsSoldAtShop(res, shopType)` (cached for UI use — it is a linear catalog scan), not by membership in the shop's current rolled stock. Items whose sell price resolves ≤ 0 are also grayed out.
- Selected sellable items expose **Sell** (one unit) and **Sell All** (every unequipped copy of the selected item).
- The list **refreshes after each transaction** (quantities drop, items disappearing at zero, selection preserved where possible).

### Vehicle “Sell All” trunk action (from Arma 3 Overthrow)

- A **“Sell All” user action on vehicle trunks** (cars/trucks), alongside the existing load/unload actions, **shown only when the vehicle is parked within selling range of a shop** that has a sell path (same shop-type rules as the menu's Sell mode: no vehicle/procurement shops, gun dealer only when the multiplier > 0).
- Performing it sells **everything in the vehicle's cargo that the shop buys** (`IsSoldAtShop` + sell price > 0 per item) in one transaction; items the shop doesn't buy stay in the trunk untouched. Crew/passenger personal inventories are never touched — cargo storage only.
- Respects **vehicle lock/ownership**: a locked vehicle can only be sold from by its owner (existing `OVT_PlayerOwnerComponent` pattern from `OVT_UnloadStorageAction`); driver must exit first (existing pattern).
- Result feedback via hint: items sold and total earned, plus a note when some items were skipped as not-bought (e.g. “Sold 34 items for $2,150 — 3 items not bought here”).
- Server-side this is **the same bulk-sell routine as the menu's Sell All** — a `RpcAsk_SellFromVehicle(vehicleRplId, shopRplId)` on the same `OVT_OverthrowController` transaction component that enumerates the trunk server-side, validates shop range/type/prices server-side, and credits only what was actually removed. Implement the routine once; both entry points consume it.
- Needs a small `GetNearestShop(pos)`/shop-in-range helper on the economy manager (only `GetNearestPort` exists today; shops are already registered in `m_aAllShops`).

### Server authority & correctness

- **All new client→server operations go on a specialized `OVT_OverthrowController` component** (e.g. a new `OVT_ShopTransactionComponent`, reached via `OVT_Global.GetController()`) — **not** `OVT_PlayerCommsComponent`, which is legacy and receives no new RPCs (project rule, see CLAUDE.md / `overthrow-controller.md`). The legacy `RpcAsk_Sell` in `OVT_PlayerCommsComponent` is **removed** once the new sell path replaces its callers; the buy path stays where it is, unchanged.
- Selling is **keyed by the player's actual item id**: the server enumerates the seller's containers and sells the requested resource id at its own server-computed price. The hardcoded variant remap block in the legacy `RpcAsk_Sell` (`OVT_PlayerCommsComponent.c:714-731`, AK74/RPG7 GUID swaps) dies with it — variants sell as themselves.
- The server **re-validates everything** the UI shows (BUG-020 pattern): shop exists and is in range, shop type buys this item (`IsSoldAtShop`), price recomputed server-side, and — new — **only unequipped items are eligible for deletion** (today `RpcAsk_Sell` uses `inventory.GetItems()` and can delete an equipped weapon; the containers-only rule must hold server-side, not just in the UI).
- **Sell All is a single RPC** (count validated server-side against what the player actually holds), not N spammed unit-sell RPCs.
- Client is credited only for items actually removed (existing `sold` counting pattern).

### Feedback & polish

- The shop component exposes an **inventory-changed invoker** fired on `RpcDo_SetInventory` so an open menu refreshes stock/quantities live (today the grid is stale until a page flip).
- **Transaction result feedback** for both modes: buy failures (insufficient funds, inventory full — finally wiring the localized-but-never-shown `PurchaseFailedInventoryFull`) and sell results surface as a message/toast in the menu.
- **Buy quantity** (e.g. a ×5 buy action alongside single buy) — nice-to-have; include only if the transaction plumbing makes it cheap.
- All new user-facing strings go through **`#OVT-` localization keys**.

### Testing

- Category mapping and sell-eligibility decisions (`category(type, mode)`, “shop buys item”, equipped-vs-container classification rules) live in **UI-free helpers with Logic-tier test coverage**, each new case proven able to fail.
- The menu itself (tabs, mode toggle, gray-out, Sell All flow, gamepad navigation) is **manual play-testing**, per project coverage rules: verify at a town shop and a gun dealer, including selling a weapon variant the shop's rolled stock doesn't contain, and confirming an equipped weapon cannot be sold. For the trunk action: verify the action only appears in shop range, mixed cargo sells only the accepted items, a locked non-owned vehicle refuses, and the hint reports sold/skipped counts.

## Companion QOL fixes (same PR, easy wins)

Small economy fixes that ride along because they touch the same files/patterns; each is independently droppable if it grows:

- ~~**BUG-081 — warehouse take buttons are dead wires**~~ — being fixed in the 1.4.0-bugfixes PR instead (2026-08-04); not part of this feature's scope anymore.
- **Money formatting helper**: every money readout is bare `"$" + amount` (shop, port, warehouse, real-estate menus, HUD `OVT_EconomyInfo.c:295`). Add one shared `FormatMoney(int)` helper (thousands separators, e.g. `$12,500`) in a UI-free class (Logic-tier testable) and use it everywhere the shop-ux work already touches.
- **Alphabetical sort for port and warehouse lists**: both build flat unsorted item lists (`OVT_PortContext.Refresh`, `OVT_WarehouseContext.Refresh`); reuse the shop-ux sort helper. (Category tabs for these menus stay out of scope.)
- **Warehouse "Take All"** for the selected item alongside 1/10/100 — mirrors Sell All; the take RPC already accepts a quantity. Nice-to-have.
- **Real-estate menu robustness**: guard the unchecked `m_mOwned[m_sPlayerID].Count()` lookups (`OVT_RealEstateContext.c:89,233` — NPE if a player owns zero buildings), and give the menu the same transaction-feedback treatment (silent returns on can't-afford today) using the toast built for shop mode. Refresh-after-ask is optimistic (asks are async) — acceptable to leave, but note it.
- **Stale warehouse refresh**: `OVT_WarehouseContext.Take()` refreshes immediately after the async server ask, showing stale quantities; refresh when the warehouse data update lands (same invoker pattern as the shop inventory-changed fix).
- **HUD money-delta ticker**: small text near the HUD cash readout (`OVT_EconomyInfo.c`) showing the running change whenever money moves — e.g. `-$150` after a buy, ticking to `-$300` on the next buy. **Accumulates across rapid changes and resets a few seconds after the money stops changing** (reset the timer on every change; then clear/fade). Green `+` for gains, red `−` for losses. Client-side only: the HUD already knows the player's money; track the last displayed amount and derive the delta from `m_OnPlayerMoneyChanged` — no new replication. Delta accumulation/reset logic goes in a UI-free helper for Logic-tier coverage; uses the shared `FormatMoney` helper.

## Dependencies

- `economy/market` — catalog cache, `IsSoldAtShop`, price APIs (all shipped).
- `economy/shops` — `OVT_ShopComponent`, `OVT_ShopContext`; BUG-020's server-authoritative sell validation is the *shape* to follow, but the new RPCs live on an `OVT_OverthrowController` component, not `OVT_PlayerCommsComponent`.
- `OVT_OverthrowController` infrastructure (shipped v1.3.0+) — specialized per-player components with built-in progress tracking (`OVT_ContainerTransferComponent` is the reference example).
- No dependency on `economy/real-estate` or on shop-stock persistence (still blocked on resource-id format; unaffected by this feature).

## Out of Scope

- Issue #145 Options C/D (sell-by-floor-drop, sell-via-ammo-box).
- Search/filter box (future enhancement; tabs + sort address the discoverability pain first).
- Sell-all-in-category / sell-entire-inventory actions.
- Shop stock persistence, restock behavior, or price model changes.
- Vehicle shop / procurement UI changes beyond hiding the Sell mode there.
- New tab UI for other menus (warehouse, port, loadouts) — pattern should be reusable, but migrating them is separate work.
