# Shop UX — Requirements

**Epic:** economy
**Created:** 2026-08-04
**Addresses:** GitHub issue #145 (“Improve looting game loop by improving sell mechanic at shops”)

## Overview

Rework the shop menu (`OVT_ShopContext` + `ShopMenu.layout`) around two ideas: **category tabs** along the top of the browser, and a **split Buy/Sell mode** where Sell browses the *player's* unequipped inventory instead of the shop's stock. This implements issue #145's Option A (category grouping) plus Option B (alphabetical order within a category), and replaces the one-click-per-unit sell flow with per-item **Sell** / **Sell All** actions. Options C/D from the issue (floor-drop / ammo-box selling) are deliberately not pursued — the sell tab solves the same pain without new world-state.

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

### Server authority & correctness

- Selling is **keyed by the player's actual item id**: the server enumerates the seller's containers, sells the requested resource id at its own server-computed price, and the hardcoded variant remap block in `RpcAsk_Sell` (`OVT_PlayerCommsComponent.c:714-731`, AK74/RPG7 GUID swaps) is **deleted** — variants sell as themselves.
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
- The menu itself (tabs, mode toggle, gray-out, Sell All flow, gamepad navigation) is **manual play-testing**, per project coverage rules: verify at a town shop and a gun dealer, including selling a weapon variant the shop's rolled stock doesn't contain, and confirming an equipped weapon cannot be sold.

## Dependencies

- `economy/market` — catalog cache, `IsSoldAtShop`, price APIs (all shipped).
- `economy/shops` — `OVT_ShopComponent`, `OVT_ShopContext`, server transaction block in `OVT_PlayerCommsComponent` (BUG-020's server-authoritative sell is the pattern to extend).
- No dependency on `economy/real-estate` or on shop-stock persistence (still blocked on resource-id format; unaffected by this feature).

## Out of Scope

- Issue #145 Options C/D (sell-by-floor-drop, sell-via-ammo-box).
- Search/filter box (future enhancement; tabs + sort address the discoverability pain first).
- Sell-all-in-category / sell-entire-inventory actions.
- Shop stock persistence, restock behavior, or price model changes.
- Vehicle shop / procurement UI changes beyond hiding the Sell mode there.
- New tab UI for other menus (warehouse, port, loadouts) — pattern should be reusable, but migrating them is separate work.
