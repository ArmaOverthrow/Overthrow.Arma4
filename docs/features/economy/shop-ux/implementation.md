# Shop UX — Implementation Plan

**Epic:** economy (feature #4 of 4)
**Status:** Ready for Review (pending human play-test)
**Started:** 2026-08-04
**Target Completion:** 2026-08-04 (built; play-test outstanding)
**Last Updated:** 2026-08-04 08:00

---

## Executive Summary

The shop menu is a flat, unsorted, 15-card grid of whatever the shop happens to have rolled, with a one-click-per-unit sell button that deletes items **client-side** and then tells the server how much money to add. This feature rebuilds the browser around **category tabs** and an explicit **Buy/Sell mode split**, where Sell browses the *player's own unequipped inventory* rather than the shop's stock — and moves the whole sell path onto a new server-authoritative `OVT_ShopTransactionComponent` on `OVT_OverthrowController`, killing the legacy `OVT_PlayerCommsComponent.RpcAsk_Sell` (and its hardcoded AK74/RPG7 variant remap) in the process.

A returning Arma 3 Overthrow feature rides along: a **"Sell All" user action on vehicles parked in range of a shop**, sharing one server-side bulk-sell routine with the menu's Sell All button.

Three design commitments shape everything below:

1. **Every decision that can be a pure function is one.** Category mapping, alphabetical sort, per-tab pagination arithmetic, sell eligibility, money formatting and the HUD delta ticker all live in UI-free classes under `Scripts/Game/Data/`, so the Logic tier can assert them. The UI keeps only widget plumbing.
2. **One sell routine, two entry points.** The menu's Sell/Sell All and the trunk action are the same server function fed different candidate lists. Anything else guarantees the two drift.
3. **The server re-derives everything the client showed** — shop identity, range, shop-type eligibility, unit price (including the gun-dealer multiplier, today a UI-only decoration), the containers-only rule, and the quantity actually held. The client is credited only for items the server actually deleted.

Buy stays exactly where it is, on the legacy component, unchanged (one three-line addition to send the localized-but-never-sent `PurchaseFailedInventoryFull`).

---

## Goals

### Primary Goals

1. **Findable items.** Category tabs (mags, scopes and weapons in *different* tabs — the core complaint of issue #145), alphabetical within a tab, "All" default, empty tabs hidden.
2. **A sell flow that matches how players actually loot.** Sell mode lists what the player is carrying, aggregated by resource id, with per-item **Sell** and **Sell All**, and shows items the shop won't buy greyed out with a reason rather than hiding them.
3. **Server authority over selling.** All new client→server operations on `OVT_ShopTransactionComponent` (never `OVT_PlayerCommsComponent`); price recomputed server-side; only unequipped items deletable; Sell All is one RPC; honest crediting.
4. **Vehicle trunk "Sell All"** shown only in selling range of an eligible shop, sharing the same server routine, respecting lock/ownership and the driver-must-exit rule.
5. **Feedback that closes the loop** — live stock refresh via a shop inventory-changed invoker, buy/sell result toasts, all strings through `#OVT-` keys.
6. **Logic-tier coverage** for every pure decision, each new case proven able to fail once.

### Secondary Goals

1. **Fix BUG-024 by construction** — the new pagination lives in a tested model, not in `Refresh()`'s integer division.
2. **Companion economy QOL** (Phase 7, independently droppable): shared `FormatMoney`, sorted port/warehouse lists, warehouse "Take All", real-estate NPE guards + feedback, warehouse stale-refresh, HUD money-delta ticker.
3. **A reusable tab/model pattern** other Overthrow menus can adopt later — without migrating them now.

### Explicitly Out of Scope

- Issue #145 Options C/D (sell-by-floor-drop, sell-via-ammo-box).
- Search/filter box; sell-by-category; sell-entire-inventory.
- Shop stock persistence, restock behaviour (BUG-019), price-model changes.
- Vehicle/procurement shop UI beyond hiding Sell mode there.
- Tab UI for warehouse/port/loadout menus.
- BUG-081 (warehouse take buttons) — already fixed on `main`.
- Fixing the int-resource-id determinism assumption. Int ids stay the wire format.
- Hardening `RpcAsk_SellDrugs` (it has the same `GetItems(PURPOSE_ANY)` shape, but drugs only ever live in containers; separate work).

---

## Architecture Overview

### New pure-logic classes (`Scripts/Game/Data/`, Logic-tier testable)

| Class | Responsibility |
|---|---|
| `OVT_ShopCategory` (enum) | `ALL, WEAPONS, AMMUNITION, ATTACHMENTS, EXPLOSIVES, MEDICAL, CLOTHING, GEAR, OTHER` |
| `OVT_ShopCategoryHelper` | `static OVT_ShopCategory GetCategory(SCR_EArsenalItemType type, SCR_EArsenalItemMode mode)`, `GetCategoryForUncatalogued()`, `GetLabelKey(cat)`, `GetDisplayOrder(out array<OVT_ShopCategory>)` |
| `OVT_ShopBrowserItem` | One row: `m_iResourceId`, `m_sResource`, `m_sDisplayName`, `m_eCategory`, `m_iUnitPrice`, `m_iQuantity`, `m_bEnabled`, `m_sDisabledReasonKey` |
| `OVT_ShopBrowserModel` | `Add()`, `SortByDisplayName()`, `GetPopulatedCategories(out)`, `FilterByCategory(cat, out)`, `GetPageCount(count, perPage)`, `GetPageItems(cat, page, perPage, out)`. **All pagination arithmetic lives here** |
| `OVT_ShopSellRules` | `static bool ShopBuysFromPlayers(int shopType, bool isProcurement, float gunDealerMultiplier)`, `static float GetSellMultiplier(int shopType, float gunDealerMultiplier)`, `static bool CanSellItem(bool isEquipped, bool shopBuys, int unitPrice)`, `static string GetBlockReasonKey(bool isEquipped, bool shopBuys, int unitPrice)` |
| `OVT_MoneyFormat` | `static string FormatMoney(int amount)` → `$12,500` / `-$150`; `static string FormatDelta(int delta)` → `+$500` / `-$150` / `""` |
| `OVT_MoneyDeltaTracker` | `Update(int currentMoney, float timeSlice)`, `GetDelta()`, `IsVisible()`, `GetText()`. Accumulates across rapid changes, resets the timer on every change, clears after `RESET_SECONDS` |

**Category mapping is mode-first, then type** — this ordering *is* the feature. A rifle magazine is catalogued as type `RIFLE` with mode `AMMUNITION`; checking type first is exactly the bug issue #145 reports.

```
no arsenal data / unregistered ......................... OTHER
mode == AMMUNITION ..................................... AMMUNITION
mode == ATTACHMENT  || type == WEAPON_ATTACHMENT ....... ATTACHMENTS
mode == CONSUMABLE  || type == HEAL .................... MEDICAL
type in {LETHAL_THROWABLE, NON_LETHAL_THROWABLE,
         EXPLOSIVES, MORTARS} .......................... EXPLOSIVES
type in {RIFLE, PISTOL, MACHINE_GUN, SNIPER_RIFLE,
         ROCKET_LAUNCHER} .............................. WEAPONS
type in {HEADWEAR, TORSO, VEST_AND_WAIST, LEGS,
         FOOTWEAR, HANDWEAR} ........................... CLOTHING
type in {BACKPACK, RADIO_BACKPACK, EQUIPMENT} .......... GEAR
otherwise (VEHICLE, HELICOPTER, unknown) ............... OTHER
```

Vehicles deliberately fall into `OTHER` rather than earning a tab: at a vehicle shop every row lands in one category, so the model's "hide the tab row entirely when fewer than two categories are populated" rule leaves the vehicle browser looking exactly as it does today.

### Shared engine-touching scanner (`Scripts/Game/Components/Economy/OVT_SellableItemScanner.c`)

Not a component — a static utility, used **verbatim by both the client (display) and the server (authority)** so the two cannot disagree:

- `static void CollectUnequippedItems(IEntity character, out array<IEntity> items)`
- `static void CollectCargoItems(IEntity vehicle, out array<IEntity> items)`
- `static bool IsEquipped(IEntity item)` — per-item classification, for anything enumerated outside the two collectors

**The APIs are pinned (verified against the 1.7.0 vanilla tree during planning); this is not a spike.**

*Player containers* — the whole containers-only rule collapses into the `EStoragePurpose` bitmask on `GetItems`:

```cpp
// SCR_InventoryStorageManagerComponent (engine proto, InventoryStorageManagerComponent.c:47)
//   int GetItems(out notnull array<IEntity> outItems, EStoragePurpose purpose = PURPOSE_ANY)
inventory.GetItems(items, EStoragePurpose.PURPOSE_DEPOSIT);
```

`PURPOSE_DEPOSIT` is exactly "container contents" — backpack, vest, jacket pockets — and excludes `PURPOSE_LOADOUT_PROXY` (worn clothing), `PURPOSE_WEAPON_PROXY` (slung/holstered weapons), `PURPOSE_GADGET_PROXY` and `PURPOSE_ATTACHMENT_PROXY`. Vanilla precedent: `SCR_ArsenalManagerComponent.c:1219` uses `PURPOSE_DEPOSIT | PURPOSE_WEAPON_PROXY` for the same "what does this player have" question. **We take `PURPOSE_DEPOSIT` alone**, because the requirement is that a slung rifle is *not* sellable.

This is also the precise diagnosis of the legacy bug: `RpcAsk_Sell` calls `inventory.GetItems(items)` with the default `PURPOSE_ANY`, which is why it can delete the shirt off the player's back.

*Per-item equipped test* (needed only if a caller enumerates with `PURPOSE_ANY`):

```cpp
InventoryItemComponent itemComp = InventoryItemComponent.Cast(item.FindComponent(InventoryItemComponent));
InventoryStorageSlot slot = itemComp.GetParentSlot();          // null => loose in the world
// worn on the body:
if (LoadoutSlotInfo.Cast(slot)) ...
// gadget/equipment slot:
if (EquipmentStorageSlot.Cast(slot)) ...
// general case, bitmask test (precedent: OVT_SpawnLogic.c:991):
if (slot.GetStorage().GetPurpose() & EStoragePurpose.PURPOSE_DEPOSIT) ...
```
Plus `CharacterControllerComponent.GetCurrentItemInHands()` for items in hands. There is **no** `IsInLoadout()` helper in vanilla — this chain is the answer, and `SCR_BlockUnequipItemHintUIInfo.c:15` / `SCR_GadgetManagerComponent.c:723` are the one-line precedents.

*Vehicle cargo* — use the vehicle's own storage manager, **not** the container-transfer path:

```cpp
SCR_VehicleInventoryStorageManagerComponent vehicleStorage =
    SCR_VehicleInventoryStorageManagerComponent.Cast(vehicle.FindComponent(SCR_VehicleInventoryStorageManagerComponent));
vehicleStorage.GetItems(items);   // every registered cargo storage
```
This is what `OVT_UnloadStorageAction.c:61` already does. `OVT_InventoryManagerComponent.PerformStorageTransfer` instead reads `UniversalInventoryStorageComponent.GetOwnedItems()` off the vehicle **root only** (`OVT_InventoryManagerComponent.c:291-302, 381`), which misses vehicles whose cargo lives on attached child entities — do not copy that path here. Crew personal inventories are never touched because they are not registered cargo storages.

*Avoid:* `SCR_InventoryStorageManagerComponent.GetAllRootItems()` is marked `[Obsolete]` and is genuinely buggy (`.Copy()` in a loop overwrites instead of appending). `FindStorageForItem()` finds a *destination*, not an item's current home.

*Useful alternative for the single-resource case:* `FindItems(out items, InventorySearchPredicate predicate, EStoragePurpose purpose = PURPOSE_DEPOSIT)` and `CountItem(predicate, PURPOSE_DEPOSIT)` **already default to containers-only**, and `SCR_ResourceNamePredicate(ResourceName, IEntity excluded)` already exists (`SCR_InventoryStorageManagerComponent.c:276`). Either shape is acceptable; pick one and use it on both sides.

This class is **not** Logic-tier testable (it needs a spawned character). Its *decisions* are — `OVT_ShopSellRules.CanSellItem(isEquipped, shopBuys, unitPrice)` takes the classification as a boolean. The scanner's job is only to produce that boolean correctly, and that is verified by play-test (equipped weapon must not be sellable) plus the server-side re-scan.

### `OVT_ShopTransactionComponent` (`Scripts/Game/Components/Controller/`)

Modelled on `OVT_ContainerTransferComponent` (the reference example), but extends plain `OVT_Component` rather than `OVT_BaseServerProgressComponent` — sells are synchronous and complete in one frame, so a progress dialog would only flash. (If trunk sells of very large cargos measure badly in play-test, switching the base class is a contained change; see R6.)

```cpp
// ---- client wrappers (Replication.IsServer() ? direct call : Rpc) ----
void SellItems(OVT_ShopComponent shop, int resourceId, int quantity)
void SellVehicleCargo(IEntity vehicle, OVT_ShopComponent shop)

// ---- server entry points ----
[RplRpc(RplChannel.Reliable, RplRcver.Server)]
protected void RpcAsk_SellItems(RplId shopId, int resourceId, int quantity)

[RplRpc(RplChannel.Reliable, RplRcver.Server)]
protected void RpcAsk_SellVehicleCargo(RplId vehicleId, RplId shopId)

// ---- owner feedback ----
[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
protected void RpcDo_SellResult(int soldCount, int totalEarned, int skippedCount, int result)

ref ScriptInvoker m_OnSellResult;   // (soldCount, totalEarned, skippedCount, OVT_ShopSellResult)
```

`enum OVT_ShopSellResult { OK, SHOP_NOT_FOUND, OUT_OF_RANGE, SHOP_DOES_NOT_BUY, NOTHING_SOLD, NOT_VEHICLE_OWNER, VEHICLE_OCCUPIED }` — int over the wire.

`RpcDo_SellResult` is **display-only**. It must never mutate money or inventory on the client; those arrive through the existing `RpcDo_SetPlayerMoney` / `RpcDo_SetInventory` broadcasts.

The shared server routine both entry points call:

```cpp
//! \param filterResourceId -1 sells every eligible item (trunk), otherwise only that resource
//! \param maxItems clamp on how many to sell (menu quantity); -1 = unlimited
//! \return total money earned; soldCount/skippedCount out-params
protected int ExecuteSell(int playerId, OVT_ShopComponent shop,
                          array<IEntity> candidates,
                          InventoryStorageManagerComponent manager,
                          int filterResourceId, int maxItems,
                          out int soldCount, out int skippedCount)
```

Its inner loop follows the project's existing delete-then-count aggregation, lifted from `OVT_Global.TransferToWarehouse` (`Scripts/Game/Global/OVT_Global.c:279-326`): build a `map<int,int> collated` of resource id → count, increment **only** inside `if (manager.TryDeleteItem(item))`, then pay out and restock per key. That ordering is what makes partial failures credit honestly.

### Economy manager additions (`OVT_EconomyManagerComponent`)

| Method | Why |
|---|---|
| `OVT_ShopComponent GetNearestShop(vector pos, float maxDistance = -1)` | Only `GetNearestPort` exists. **Must scan `m_aAllShops` *and* `m_aGunDealers`** — `FilterShopEntities` deliberately excludes gun dealers from `m_aAllShops`, so a naive implementation would make gun dealers invisible to the trunk action |
| `bool IsRegisteredResource(ResourceName res)` | `GetInventoryId()` is a bare `m_aResourceIndex[res]` lookup; looted gear that never entered the resource DB would silently resolve to id 0 (i.e. *some other item's price*). Every scanned item is gated on this |
| `OVT_ShopCategory GetItemCategory(int id)` | Lazily-built `map<int, OVT_ShopCategory>` over `m_aEntityCatalogEntries`; without it every refresh re-walks the catalog per card |
| `bool IsSoldAtShopCached(int id, OVT_ShopType shopType)` | `IsSoldAtShop` runs `FindInventoryItems` (a full catalog scan) **per config rule per call**. Sell mode would call it once per held item per refresh. Lazily build `map<int, ref set<int>>` (shop type → resource ids) once |

### UI changes

**`UI/Layouts/Menu/ShopMenu.layout`** gains four widgets (hand-authored, fresh GUIDs):
- `Tabs` — a `HorizontalLayoutWidgetClass` in the header row, populated at runtime.
- `ModeBuyButton` / `ModeSellButton` — mode toggle in the header.
- `SellAllButton` — in the details footer beside the existing `BuyButton`/`SellButton`.
- `Message` — a `TextWidget` for the in-menu toast.

**`UI/Layouts/Menu/ShopMenu/ShopMenu_Tab.layout`** (new, small) — one tab button, instantiated per populated category at runtime via `workspace.CreateWidgets(m_TabLayout, tabsContainer)`, exactly the dynamic-list pattern `OVT_PortContext`/`OVT_WarehouseContext` already use. This keeps hand-authored layout GUIDs to a minimum (see R8).

**`OVT_ShopMenuTabComponent`** (new, `Scripts/Game/UI/Menu/ShopMenu/`) — `SCR_ScriptedWidgetComponent` with an `OnClick` that calls back into the context, mirroring `OVT_ShopMenuCardComponent`.

**`OVT_ShopMenuCardComponent.Init`** gains defaulted params `bool enabled = true, string reasonKey = ""` and dims/tints on `!enabled`, following the existing precedent in `OVT_PlaceMenuCardComponent.c:37-47`. Defaults keep `OVT_ManageVehicleContext` compiling untouched. **Also fixes a live bug found during planning:** the card's `Stock` widget is `"Is Visible" 0` in the layout and the code only ever calls `SetVisible(false)` — quantity has never rendered on a shop card. Sell mode needs it.

**`OVT_ShopContext`** is reworked in place around a mode + tab + model triple.

### Trunk action

`Scripts/Game/UserActions/OVT_SellVehicleCargoAction.c` : `ScriptedUserAction`
- `CanBeShownScript(user)` → nearest shop within selling range **and** `OVT_ShopSellRules.ShopBuysFromPlayers(...)` **and** the vehicle has cargo. Throttled (see R6).
- `CanBePerformedScript(user)` → `OVT_PlayerOwnerComponent` lock/ownership check + no pilot occupant, both lifted from `OVT_UnloadStorageAction.c:32-42,74-102`.
- `PerformAction` → `OVT_Global.GetShopTransactions().SellVehicleCargo(vehicle, shop)`.
- Registered in `Prefabs/Vehicles/Core/Vehicle_Base.et` — the same seam `OVT_LockVehicleAction`/`OVT_UnlockVehicleAction` use to reach every vehicle in the game (that file is Overthrow's override of the vanilla base vehicle prefab, GUID `{4085446E2B406849}`).

### Data flow

**Buy (unchanged path, new refresh signal):**
```
ShopContext[Buy] -> OVT_Global.GetServer().Buy(shop, id, 1, playerId)
  -> RpcAsk_Buy [server, legacy]: recompute price, check funds, spawn+insert,
     charge for delivered only, stock--, m_OnPlayerBuy + m_OnPlayerTransaction(true)
  -> shop.StreamInventory -> RpcDo_SetInventory [broadcast]
       -> NEW m_OnInventoryChanged.Invoke(id, amount) -> ShopContext.Refresh()
  -> RpcDo_SetPlayerMoney [broadcast] -> m_OnPlayerMoneyChanged -> header + HUD ticker
```

**Sell (new path):**
```
ShopContext[Sell]                                OVT_SellVehicleCargoAction
  builds display model from                        CanBeShownScript:
  OVT_SellableItemScanner (client)                   GetNearestShop + ShopBuysFromPlayers
        |                                                  |
        v                                                  v
 GetShopTransactions().SellItems(shop,resId,qty)   .SellVehicleCargo(vehicle,shop)
        |                                                  |
        +----------- Rpc(RpcAsk_*)  [Reliable, Server] ----+
                              |
                              v
   [SERVER] resolve player entity; resolve shop by RplId; distance <= SHOP_MAX_DISTANCE;
            OVT_ShopSellRules.ShopBuysFromPlayers(type, procurement, multiplier);
            (vehicle) lock/ownership + no pilot + cargo storage exists
                              |
                              v
   OVT_SellableItemScanner (server, authoritative re-scan) -> candidate entities
     player  : inventory.GetItems(items, EStoragePurpose.PURPOSE_DEPOSIT)
     vehicle : SCR_VehicleInventoryStorageManagerComponent.GetItems(items)
                              |
                              v
   ExecuteSell(): per item -> prefab -> IsRegisteredResource -> id
                  -> IsSoldAtShopCached(id, shopType)
                  -> unitPrice = GetSellPrice(id, shopPos) * GetSellMultiplier(...)
                  -> unitPrice > 0 -> manager.TryDeleteItem(item) -> collated[id]++
                  (skip -> skippedCount++)
                              |
                              v
   DoAddPlayerMoney(playerId, total); shop.AddToInventory(resId, collated[resId]);
   m_OnPlayerSell.Invoke(playerId, total);          // skills XP
   m_OnPlayerTransaction.Invoke(playerId, shop, false, total);   // stability/support modifiers
                              |
                              v
   Rpc(RpcDo_SellResult, sold, earned, skipped, OK)  [Owner]
                              |
              +---------------+----------------+
              v                                v
   ShopContext: toast + rebuild sell     SCR_HintManagerComponent.ShowCustom
   model, preserve selection             ("Sold %1 items for %2 - %3 not bought here")
```

**Tab / pagination interaction:**
```
model (all rows) --FilterByCategory(tab)--> rows --SortByDisplayName--> sorted
      --GetPageItems(page, 15)--> <=15 cards
tab change OR mode change => page = 0, selection cleared, Refresh()
page count = Math.Ceil(count / 15.0), clamped to >= 1; page clamped to [0, count-1]
```

### File structure

```
Scripts/Game/
├── Data/
│   ├── OVT_ShopCategory.c              (enum + OVT_ShopCategoryHelper)     NEW
│   ├── OVT_ShopBrowserModel.c          (+ OVT_ShopBrowserItem)             NEW
│   ├── OVT_ShopSellRules.c                                                 NEW
│   ├── OVT_MoneyFormat.c                                                   NEW
│   └── OVT_MoneyDeltaTracker.c                                             NEW
├── Components/
│   ├── Controller/
│   │   └── OVT_ShopTransactionComponent.c                                  NEW
│   └── Economy/
│       ├── OVT_SellableItemScanner.c                                       NEW
│       └── OVT_ShopComponent.c         (+ m_OnInventoryChanged)            EDIT
├── GameMode/Managers/
│   └── OVT_EconomyManagerComponent.c   (+ 4 seams above)                   EDIT
├── Global/
│   └── OVT_Global.c                    (+ GetShopTransactions())           EDIT
├── UI/
│   ├── Context/
│   │   ├── OVT_ShopContext.c           (major rework)                      EDIT
│   │   ├── OVT_PortContext.c           (sort + FormatMoney)          EDIT (Phase 7)
│   │   ├── OVT_WarehouseContext.c      (sort, Take All, refresh)     EDIT (Phase 7)
│   │   └── OVT_RealEstateContext.c     (NPE guards + feedback)       EDIT (Phase 7)
│   ├── HUD/
│   │   └── OVT_EconomyInfo.c           (delta ticker)                EDIT (Phase 7)
│   └── Menu/ShopMenu/
│       ├── OVT_ShopMenuCardComponent.c (enabled/reason + Stock fix)        EDIT
│       └── OVT_ShopMenuTabComponent.c                                      NEW
├── UserActions/
│   └── OVT_SellVehicleCargoAction.c                                        NEW
├── Components/Player/
│   └── OVT_PlayerCommsComponent.c      (DELETE Sell/RpcAsk_Sell;
│                                        send PurchaseFailedInventoryFull)  EDIT
└── Tests/TestSuites/Logic/
    └── OVT_TEST_Logic_ShopUX.c                                             NEW

UI/Layouts/Menu/
├── ShopMenu.layout                     (Tabs, mode toggle, SellAll, Message) EDIT
└── ShopMenu/ShopMenu_Tab.layout                                            NEW

Prefabs/
├── GameMode/OVT_OverthrowController.et (+ OVT_ShopTransactionComponent)    EDIT
└── Vehicles/Core/Vehicle_Base.et       (+ OVT_SellVehicleCargoAction)      EDIT

Configs/
├── System/chimeraInputCommon.conf      (+ OverthrowShopSellAll)            EDIT
└── overthrowBroadcastMessages.conf     (sell result tags, if used)         EDIT

Language/
├── localization_Overthrow.st           (new #OVT- keys)                    EDIT
└── localization_Overthrow.en-us.conf   (id index)                          EDIT
```

---

## Implementation Phases

Every phase ends with `tools/compile-check.sh` clean. Phases 1-3 additionally end with `tools/run-tests.sh "{6A6E2A002F53A581}"` (All) green.

### Phase 1 — Pure helpers + Logic-tier tests
*Standard dev agent. No engine types beyond the two arsenal enums; nothing here touches a manager, the game mode or the world.*

| # | Task |
|---|---|
| 1.1 | `OVT_ShopCategory` enum + `OVT_ShopCategoryHelper` (mapping table above, label keys, display order) |
| 1.2 | `OVT_ShopBrowserItem` + `OVT_ShopBrowserModel` (add / sort / populated-categories / filter / page-count / page-slice) |
| 1.3 | `OVT_ShopSellRules` (shop-buys predicate, sell multiplier, can-sell, block-reason key) |
| 1.4 | `OVT_MoneyFormat` (thousands separators, negatives, zero, signed delta form) |
| 1.5 | `OVT_MoneyDeltaTracker` (accumulate, reset-timer-on-change, clear after timeout, first-observation seeds baseline with no delta) |
| 1.6 | `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_ShopUX.c` — five cases (see Testing Strategy) |
| 1.7 | Prove each new case red once; record the method in the feature's notes |

**Acceptance:** compile-check exit 0. Fast group (`{6A6E29FF47ECB840}`) exit 0 with five additional cases. Each new case has a recorded way it was made to fail. No case reads state it did not itself write. `Scripts/Game/Tests/TestSuites/Logic/` still contains no reference to the static manager accessor or the engine game-mode getter (the Logic tier's grep rule).

### Phase 2 — Economy manager seams
*Standard dev agent.*

| # | Task |
|---|---|
| 2.1 | `GetNearestShop(pos, maxDistance)` over `m_aAllShops` **+ `m_aGunDealers`**, null-guarding `Replication.FindItem` and `rpl.GetEntity()`; returns null when nothing is in range |
| 2.2 | `IsRegisteredResource(ResourceName)` |
| 2.3 | `GetItemCategory(int id)` with lazy `m_mResourceCategory` cache, built from `m_aEntityCatalogEntries` on first use; unregistered/no-arsenal-data → `OTHER` |
| 2.4 | `IsSoldAtShopCached(int id, OVT_ShopType)` with lazy per-shop-type id set; keep `IsSoldAtShop(ResourceName, type)` as-is for existing callers |
| 2.5 | *(Recommended)* one Campaign-tier case: `GetNearestShop` finds a registered shop at its own origin and returns null 5 km away; the category cache is non-empty after campaign start. Must be proven red once |

**Acceptance:** compile-check clean; All group green; no behaviour change to any existing caller of `IsSoldAtShop`.

### Phase 3 — Server-authoritative sell ⚠️ ADVANCED AGENT
*Route to `network-specialist-advanced`. This is the integration-heavy, highest-risk phase: a new replicated component, a new prefab entry, deletion of a legacy RPC, and the one authority boundary in the feature.*

| # | Task |
|---|---|
| 3.1 | `OVT_SellableItemScanner` — `CollectUnequippedItems` via `GetItems(items, EStoragePurpose.PURPOSE_DEPOSIT)`; `CollectCargoItems` via `SCR_VehicleInventoryStorageManagerComponent.GetItems`; `IsEquipped` via the `GetParentSlot()` → `LoadoutSlotInfo`/`EquipmentStorageSlot`/`GetPurpose()` chain. One implementation, called from both client and server |
| 3.2 | `OVT_ShopTransactionComponent` — the RPC surface above, `ExecuteSell` with the `map<int,int> collated` delete-then-count loop, `OVT_ShopSellResult`, `m_OnSellResult` |
| 3.3 | Register on `Prefabs/GameMode/OVT_OverthrowController.et` (fresh GUID) + `OVT_Global.GetShopTransactions()` |
| 3.4 | Repoint `OVT_ShopContext.Sell` at the new component (interim: keep the current single-unit button working before Phase 4 lands) |
| 3.5 | **Delete** `OVT_PlayerCommsComponent.Sell()` and `RpcAsk_Sell()` including the AK74/RPG7 variant-remap block (`:714-731`). Confirm by grep that `OVT_SellDrugsAction` uses `SellDrugs`/`RpcAsk_SellDrugs` and is unaffected |
| 3.6 | Verify with a hand test that `PURPOSE_DEPOSIT` really excludes what we claim: worn vest/backpack/uniform and the slung rifle absent; backpack contents present |

**Acceptance:** compile-check clean; All group green; grep proves zero remaining callers of the removed methods; a sell in single-player credits exactly the recomputed server price; a gun-dealer sell is multiplied server-side (verify by selling the same item at a town shop and at a gun dealer and comparing credits); an equipped weapon cannot be sold even if a request names it.

### Phase 4 — Shop menu rework ⚠️ ADVANCED AGENT
*Route to `component-developer-advanced`. `OVT_ShopContext` is a 305-line file whose `Refresh()`/`SelectItem()` are rebuilt around a mode + tab + model; the layout gains four widgets and a new sub-layout. This is a major refactor and the phase most likely to produce dead buttons if rushed.*

| # | Task |
|---|---|
| 4.1 | Layout edits: `Tabs` container, `ModeBuyButton`/`ModeSellButton`, `SellAllButton`, `Message`. Fresh GUIDs; every new `FindAnyWidget` result null-guarded in script |
| 4.2 | `ShopMenu_Tab.layout` + `OVT_ShopMenuTabComponent` |
| 4.3 | `OVT_ShopMenuCardComponent.Init(res, cost, qty, context, bool enabled = true, string reasonKey = "")`; dim + tint when disabled; **make `Stock` visible when `qty != -1`** |
| 4.4 | `OVT_ShopContext`: `m_eMode` (BUY/SELL), `m_eTab`, `m_iPageNum`; `BuildBuyModel()` (shop stock / procurement vehicles) and `BuildSellModel()` (scanner → aggregate by resource id → price → `IsSoldAtShopCached` → enabled/reason); tab row rebuild (hidden when <2 populated categories); page reset + selection clear on tab/mode change; mode toggle hidden per `OVT_ShopSellRules.ShopBuysFromPlayers`; details pane shows owned qty + unit sell price + block reason in Sell mode; Sell / Sell All wiring; **handler teardown in `OnClose`** (fixes the existing insert-every-show leak) |
| 4.5 | Input: add `OverthrowShopSellAll` to `Configs/System/chimeraInputCommon.conf` and to the `OverthrowShopContext` action refs. *Nice-to-have, not a gate:* bind tab cycling to the already-present `MenuNavLeft`/`MenuNavRight` |
| 4.6 | All new strings as `#OVT-` keys in `localization_Overthrow.st` + the `en-us.conf` id index |

**Acceptance:** every tab, both mode buttons, Sell, Sell All, page prev/next and close respond to mouse **and** to gamepad focus navigation; no button is a dead wire; an empty category never shows a tab; switching tab or mode lands on page 1 with a valid selection; the buy flow behaves exactly as before the change (same prices, same stock decrements, same vehicle-shop close-on-buy).

### Phase 5 — Vehicle trunk "Sell All"
*Standard dev agent.*

| # | Task |
|---|---|
| 5.1 | `OVT_SellVehicleCargoAction` — `CanBeShownScript` (eligible shop in range + cargo non-empty, throttled), `CanBePerformedScript` (lock/ownership via `OVT_PlayerOwnerComponent`, `SetCannotPerformReason("#OVT-Locked")`, driver-must-exit), `PerformAction` → `SellVehicleCargo` |
| 5.2 | Prefab: add to `Prefabs/Vehicles/Core/Vehicle_Base.et` with UIInfo `#OVT-SellCargoHere`; verify in Workbench which door/trunk context makes it appear where players expect |
| 5.3 | Hint feedback from `m_OnSellResult` — sold count, total earned (`FormatMoney`), skipped count |

**Acceptance:** the action is invisible away from a shop, invisible at a vehicle/procurement shop and at a zero-multiplier gun dealer, visible at a town shop and a live gun dealer; mixed cargo sells only accepted items and leaves the rest in the trunk; a locked vehicle owned by someone else refuses with the locked reason; the hint reports sold/earned/skipped.

### Phase 6 — Feedback & polish
*Standard dev agent.*

| # | Task |
|---|---|
| 6.1 | `OVT_ShopComponent.m_OnInventoryChanged` invoker fired from `RpcDo_SetInventory`; `OVT_ShopContext` subscribes on show, **unsubscribes on close** |
| 6.2 | Send `PurchaseFailedInventoryFull` from `RpcAsk_Buy`'s complete-failure branch (funds were sufficient, zero items delivered). No other change to the buy path |
| 6.3 | In-menu toast: `m_OnSellResult` + buy notifications → `Message` widget with a fade timer; localized |
| 6.4 | *(YAGNI gate)* ×5 buy — include **only** if 6.3's plumbing makes it a two-line addition; drop it otherwise |

**Acceptance:** buying the last unit of an item updates the grid without a page flip; a full-inventory purchase produces a visible message instead of silence; every message is a `#OVT-` key, none is a raw string.

### Phase 7 — Companion QOL (independently droppable)
*Standard dev agent. Each item is separable; drop any that grows.*

| # | Task |
|---|---|
| 7.1 | `OVT_MoneyFormat.FormatMoney` applied everywhere this feature already touches: `OVT_ShopContext`, `OVT_ShopMenuCardComponent`, `OVT_PortContext`, `OVT_WarehouseContext`, `OVT_RealEstateContext`, `OVT_EconomyInfo:296` |
| 7.2 | Alphabetical sort in `OVT_PortContext.Refresh` and `OVT_WarehouseContext.Refresh` using the Phase 1 sort helper |
| 7.3 | Warehouse "Take All" button beside 1/10/100 (the take RPC already accepts a quantity) |
| 7.4 | Real-estate: guard `m_mOwned[m_sPlayerID]` at `OVT_RealEstateContext.c:89` and `:233` (a player owning zero buildings NPEs today); route can't-afford/not-owner silent returns through the Phase 6 toast; note in a comment that refresh-after-ask stays optimistic |
| 7.5 | `OVT_WarehouseContext.Take()` refreshes on the warehouse-data update rather than immediately after the async ask |
| 7.6 | HUD money-delta ticker in `OVT_EconomyInfo` driven by `OVT_MoneyDeltaTracker` (green `+`, red `−`, accumulate, clear after the reset timeout) — client-side only, no new replication |

**Acceptance:** every money readout in the touched menus shows thousands separators; port and warehouse lists are alphabetical; a player with zero owned buildings can open the real-estate menu; the HUD ticker accumulates across two rapid buys and clears a few seconds after the last change.

---

## Key Technical Decisions

**D1 — Category mapping is a pure function of `(type, mode)`, held in `Scripts/Game/Data/`.**
It is the one piece of this feature with a genuinely tricky rule (mode before type, because magazines are typed `RIFLE`), and it is trivially testable if — and only if — it never touches a widget or a manager. The economy manager owns the *cache* (id → category) because it owns the catalog; the *rule* lives in the helper.

**D2 — One bulk-sell routine, two candidate lists.**
The menu's Sell All and the trunk action differ only in which entities are candidates and whether a resource filter applies. Implementing them separately would guarantee that one of them eventually forgets the gun-dealer multiplier or the `IsSoldAtShop` check — precisely how the current sell path ended up client-authoritative in the first place.

**D3 — Sell mode enumerates the player's inventory twice: once on the client for display, once on the server for authority, through the same scanner class.**
The client needs a list to draw *now* (a round-trip per menu open would make the menu feel broken), and the server cannot trust that list. Sharing one static scanner — and, critically, one `EStoragePurpose` mask — means "what the UI offered" and "what the server will sell" are the same rule by construction; any divergence is then a state race (item moved between draw and click), which the server resolves by selling fewer items and crediting fewer.

**D4 — Sell All sends the client's observed count; the server clamps to what is actually held.**
One RPC, one code path, no sentinel value with special meaning. The requirement "count validated server-side" is satisfied by `min(requested, actually_deletable)`, and the client is credited only for successful `TryDeleteItem` calls.

**D5 — Tab state resets pagination; pagination never resets tab state.**
`(mode, tab)` selects the row set; `page` indexes into it. Changing either upper-level value invalidates `page`, so both set `page = 0` and clear the selection. This is the whole interaction contract, and it lives in the model, not in `Refresh()`.

**D6 — Pagination arithmetic moves into a tested model, which is how BUG-024 dies.**
`Math.Ceil(count / 15.0)` (float divisor), page count clamped to ≥1, page clamped to `[0, pageCount-1]`, slice bounds clamped to `count`. The current code's `Count()/15` integer division makes the tail of any 57-item shop permanently unreachable and lets `NextPage` compute `GetKey(-15)`. Not inheriting it is a requirement of the rework, and the Logic tier pins it.

**D7 — RPC payloads are int resource ids + RplIds.**
Int ids are already the wire format for shop stock; introducing `ResourceName` strings here would be a second, inconsistent format for no benefit and a larger payload. `RplId` for the shop and the vehicle, never `EntityID`. The determinism weakness of int ids is epic-level tech debt and explicitly out of scope — but `IsRegisteredResource` is added so an out-of-range id from a stale or malicious client cannot index the resource array.

**D8 — New RPCs live on a new controller component; the buy path stays on the legacy one.**
Project rule (`overthrow-controller.md`): `OVT_PlayerCommsComponent` receives no new RPCs. Moving *buy* as well would double this feature's blast radius for no user-visible gain; it is a separate migration. The legacy `RpcAsk_Sell` is deleted rather than deprecated because it is the exploit.

**D9 — `OVT_ShopTransactionComponent` extends `OVT_Component`, not `OVT_BaseServerProgressComponent`.**
Sells complete synchronously; the progress dialog would flash for a few frames on every transaction. Result feedback is a purpose-built owner RPC carrying money, which the progress base's `(itemsTransferred, itemsSkipped)` cannot express.

**D10 — Gray-out rather than hide for items the shop won't buy.**
Hiding them makes players think the item vanished from their pack. The card dims and the details pane names the reason, following `OVT_PlaceMenuCardComponent`'s existing can't-do-this treatment.

**D11 — The HUD ticker derives its delta by polling, not by subscribing.**
`OVT_EconomyInfo.UpdateMoney()` already polls `GetPlayerMoney` every frame and has a `timeSlice`. Feeding `(currentMoney, timeSlice)` into `OVT_MoneyDeltaTracker` needs no invoker subscription on a client HUD whose lifetime does not obviously match the manager's, and it keeps the accumulate/reset logic in a class the Logic tier can drive with a hand-written sequence.

**D12 — Tabs are ordinary buttons.**
Project finding: mouse-wired `m_OnClicked` buttons are already gamepad-navigable, so no controller-specific work is a gate. A `MenuNavLeft`/`MenuNavRight` tab-cycle binding is a nice-to-have on top.

**D13 — "Unequipped" is defined as `EStoragePurpose.PURPOSE_DEPOSIT`, not as a hand-rolled slot walk.**
The engine already partitions storages exactly the way the requirement words it, and the vanilla arsenal asks the same question the same way (`SCR_ArsenalManagerComponent.c:1219`). One mask constant is far harder to get subtly wrong than a per-item classification, and it makes the client and server rule literally the same token. Consequence, accepted deliberately: a **slung or holstered weapon is not sellable** (it lives in `PURPOSE_WEAPON_PROXY`) — the player unslings it into their pack first, which is what the requirement asks for. The per-item `IsEquipped` predicate is kept for callers that must enumerate more broadly.

---

## Definition of Done

An independent evaluator should be able to verify all of the following without having read the implementation.

### Functional Criteria

- [ ] **F1** Opening a town shop shows a row of category tabs with **All** selected; weapons, magazines and optics appear under *different* tabs.
- [ ] **F2** No tab is shown for a category with zero items (a clothes shop shows no Weapons tab); when fewer than two categories are populated, no tab row is shown at all.
- [ ] **F3** Items within a tab are ordered alphabetically by displayed name.
- [ ] **F4** Pagination is per tab: the page label reads `1/N` with `N ≥ 1`, the last page is reachable and shows the remainder, and `Next` on the last page / `Prev` on the first page does nothing bad. A shop with fewer than 15 items reads `1/1`.
- [ ] **F5** A visible **Buy / Sell** toggle exists; Buy is selected on open; switching resets to page 1 and clears the selection.
- [ ] **F6** The Sell toggle is **absent** at a vehicle shop, at a procurement (garage/helipad) shop, and at a gun dealer when `gunDealerSellPriceMultiplier == 0`.
- [ ] **F7** Sell mode lists items the player is carrying **in containers**, aggregated by resource id, with preview, name, owned quantity and unit sell price.
- [ ] **F8** Equipped items — worn clothing, worn vest/backpack, holstered or slung weapons, items in hands — are **not** listed and cannot be sold. Contents *inside* a worn backpack or vest **are** listed.
- [ ] **F9** Items the shop does not buy (per `IsSoldAtShop`), and items whose sell price resolves to ≤ 0, appear greyed out, are still selectable, show a localized reason, and have their sell buttons disabled.
- [ ] **F10** **Sell** sells one unit; **Sell All** sells every unequipped copy in a single request. The list refreshes after each transaction (quantities drop, zero-quantity rows disappear, selection preserved where possible).
- [ ] **F11** Selling a weapon **variant** the shop's rolled stock does not contain still works and credits that variant's own price (no AK74/RPG7 remap).
- [ ] **F12** A "Sell All" action appears on a vehicle **only** when it is parked within selling range of a shop with a sell path; it sells everything in cargo the shop buys, leaves the rest, never touches crew inventories, and reports sold/earned/skipped in a hint.
- [ ] **F13** The trunk action refuses on a locked vehicle the player does not own, and refuses while someone is in the driver's seat.
- [ ] **F14** Buying the last unit of an item updates the grid live, without a page flip.
- [ ] **F15** A purchase that fails because the inventory is full produces a visible message (`PurchaseFailedInventoryFull`), not silence.

### Quality Criteria

- [ ] **Q1 — No client-trusted money.** `OVT_PlayerCommsComponent.Sell` and `RpcAsk_Sell` no longer exist; `grep -rn "RpcAsk_Sell\b" Scripts/` returns nothing. No client code adds money or deletes an item as part of selling.
- [ ] **Q2 — Server re-validates.** The sell RPCs re-resolve the shop by `RplId`, re-check distance, re-check shop-type eligibility, recompute unit price (including the gun-dealer multiplier), re-derive the containers-only rule, and clamp quantity to what the player actually holds.
- [ ] **Q3 — Honest crediting.** Money added equals `unitPrice × items actually deleted`. Verified by selling a stack while a second sell request for the same stack is in flight (or by a Sell All of a quantity that partially fails).
- [ ] **Q4 — Single-RPC bulk.** Sell All and the trunk action each produce **one** client→server RPC regardless of item count.
- [ ] **Q5 — Buy path unchanged.** Same prices, same partial-purchase behaviour, same stock decrements, same vehicle-shop close-on-buy as before the feature. The only diff in `RpcAsk_Buy` is the added inventory-full notification.
- [ ] **Q6 — No dead buttons.** Every button in the reworked menu performs its labelled action, with mouse and with a controller.
- [ ] **Q7 — No new nulls.** Every `FindAnyWidget` result and every `Replication.FindItem`/`GetEntity()` result introduced by this feature is null-guarded. Opening the menu at every shop type, and opening the real-estate menu owning zero buildings, produces no script error.
- [ ] **Q8 — No ternaries, `ref` on Managed in containers, `RplId` not `EntityID` over the network, `OVT_`/`m_` naming** throughout the new code.
- [ ] **Q9 — Compile and tests clean:** `tools/compile-check.sh` exit 0; `tools/run-tests.sh "{6A6E2A002F53A581}"` exit 0.

### Integration Criteria

- [ ] **I1 — Skills XP.** A sell fires `m_OnPlayerSell.Invoke(playerId, total)`, so `OVT_SkillManagerComponent.OnPlayerSell` awards `1 + floor(total * 0.01)` XP — including in single-player, and once per bulk sell rather than once per unit.
- [ ] **I2 — Modifiers.** A sell fires `m_OnPlayerTransaction.Invoke(playerId, shop, false, total)`, so the black-market / strong-economy stability and support modifiers see sells (the `isBuying=false` parameter stops being dead).
- [ ] **I3 — Shop stock.** Sold items are added back to the shop's stock and broadcast, exactly as the legacy path did.
- [ ] **I4 — Localization.** Every new user-facing string is a `#OVT-` key present in `Language/localization_Overthrow.st` and in the `en-us.conf` id index. No raw English string is drawn by new code.
- [ ] **I5 — Controller pattern.** The new component sits on `OVT_OverthrowController` and is reachable via `OVT_Global.GetShopTransactions()`; nothing new was added to `OVT_PlayerCommsComponent`.

### Verification Method

**Automated (run these; record exit codes):**
```bash
tools/compile-check.sh                              # expect 0
tools/run-tests.sh "{6A6E29FF47ECB840}"             # Fast  - expect 0
tools/run-tests.sh "{6A6E2A002F53A581}"             # All   - expect 0
tools/run-tests.sh OVT_TEST_Logic_ShopUX_CategoryMapping   # single case, debugging
```

**Logic-tier cases that must exist and must each have been proven able to fail once** (record how, per project rule — no `maxAttempts`, ever):

| Case | Claim |
|---|---|
| `OVT_TEST_Logic_ShopUX_CategoryMapping` | Mode beats type: `(RIFLE, AMMUNITION)` → AMMUNITION, `(RIFLE, WEAPON)` → WEAPONS, `(RIFLE, ATTACHMENT)` → ATTACHMENTS; every enum value in the table maps as specified; an unrecognised type falls to OTHER |
| `OVT_TEST_Logic_ShopUX_BrowserModelSortAndFilter` | Alphabetical order is stable and case-insensitive; `FilterByCategory` returns only that category; `ALL` returns everything; `GetPopulatedCategories` omits empty ones |
| `OVT_TEST_Logic_ShopUX_Pagination` | **Pins BUG-024:** 57 items / 15 per page → 4 pages with a 12-item last page; 3 items → 1 page; 0 items → 1 page; page index clamps at both ends; no slice ever reads out of range |
| `OVT_TEST_Logic_ShopUX_SellRules` | Vehicle and procurement shops never buy; a gun dealer buys iff multiplier > 0 and the multiplier is applied; equipped → cannot sell; shop-doesn't-buy → cannot sell; price ≤ 0 → cannot sell; each blocked case names a distinct reason key |
| `OVT_TEST_Logic_ShopUX_MoneyFormatAndDelta` | `FormatMoney` groups thousands and handles 0/negative/large values; the tracker accumulates two changes inside the window, resets its timer on each change, and clears after the timeout; the first observation produces no delta |

**Manual play-test (the runtime gate — UI, MP/JIP and AI are not covered by the harness):**

*A. Town shop, buy side (regression check)*
1. Open a general shop. Confirm the tab row, `All` selected, alphabetical order, page label `1/N`.
2. Page to the last page — confirm the remainder items are reachable (this is the BUG-024 check).
3. Click each tab; confirm items are correctly grouped and that magazines are **not** in Weapons.
4. Buy an item: money drops by the displayed price, stock decrements **without a page flip**, and the details pane updates.
5. Fill your inventory and buy again: an inventory-full message appears.

*B. Town shop, sell side*
6. Switch to Sell. Confirm page resets to 1 and the list shows *your* items, aggregated with owned counts.
7. Confirm your worn vest, worn backpack, uniform and the rifle on your back are **absent** from the list; confirm items inside the worn backpack **are** present.
8. Select an item the shop doesn't buy (e.g. a weapon at a clothes shop): greyed, selectable, reason shown, Sell/Sell All disabled.
9. Sell one unit: money rises by the displayed unit price, quantity drops by one, selection is preserved.
10. Sell All on a stack: money rises by `unit × count` in **one** step, the row disappears, and the shop's stock for that item increases.
11. Switch back to Buy: page 1, sane selection, buy still works.

*C. Gun dealer*
12. At a gun dealer with `gunDealerSellPriceMultiplier > 0`: Sell toggle present; sell a weapon **variant** the dealer does not stock — it sells at its own price (no variant remap), and the credited amount reflects the multiplier (compare against the same item's price at a town shop).
13. Set the multiplier to 0 in difficulty config: the Sell toggle disappears; the trunk action disappears too.

*D. Vehicle / procurement shops*
14. At a fuel-station vehicle shop and at an owned garage: no Sell toggle; the vehicle browser looks and behaves as before.

*E. Trunk "Sell All"*
15. Load mixed cargo (sellable + not-sold-here) into a car. Away from any shop: **no** Sell All action on the vehicle.
16. Drive it next to a town shop: the action appears. Perform it — accepted items are gone, unaccepted items remain, money rises once, and the hint reports sold / earned / skipped.
17. With a second player (or a lock) — a locked vehicle you do not own refuses with the locked reason; with someone in the driver's seat it refuses with driver-must-exit.

*F. Equipped-item safety*
18. With a rifle equipped and no spare in your pack, confirm the rifle never appears in Sell mode and cannot be sold by any button.

*G. Multiplayer / JIP (two client processes — the harness cannot do this)*
19. Host + one client. Client sells from the menu: money and shop stock update on **both**; the client's own inventory updates once.
20. Client performs the trunk Sell All: same checks.
21. A third player joins after several sells (JIP): shop stock and money are correct on the joining client.
22. Two clients sell the same item type at the same shop simultaneously: no negative stock, no double credit.

*H. Companion QOL (if Phase 7 ships)*
23. Every money readout in shop / port / warehouse / real-estate / HUD shows thousands separators.
24. Port and warehouse lists are alphabetical; warehouse Take All empties the selected row; warehouse quantities are correct immediately after a take.
25. Open the real-estate menu as a player owning **zero** buildings — no script error.
26. Buy twice in quick succession: the HUD ticker shows an accumulating red delta, then clears a few seconds after the last change.

---

## Testing Strategy

**Logic tier (automated, world-free)** — the five cases in the table above cover: category mapping precedence, sort/filter, pagination arithmetic (BUG-024 pin), sell-eligibility rules including the gun-dealer multiplier, money formatting and delta accumulation. These are the only parts of the feature that *can* be asserted without a world, and they were deliberately factored so that they can be.

**Campaign tier (automated, optional but recommended)** — one case for `GetNearestShop` (found at a shop's own origin, null far away) and the category cache being populated after campaign start. Cheap, real, and it exercises the gun-dealer inclusion that a Logic case cannot.

**Not automatable, therefore manual and named explicitly:**
- The entire menu (tabs, mode toggle, grey-out, Sell All, controller navigation) — project rule: UI is play-test only.
- `OVT_SellableItemScanner`'s `PURPOSE_DEPOSIT` partition — needs a spawned character with a loadout. Steps B7 and F18 are its test.
- The trunk action's visibility gating and ownership rules — steps E15-E17.
- Multiplayer and JIP — steps G19-G22. **This is the most common regression class in this project and the harness cannot reach it.** Two client processes are required.
- Performance of the sell-mode refresh and the trunk action's visibility check — watch for hitching in steps B6 and E16.

**Fallibility rule:** every new automated case must be shown red once during development, by a recorded edit, before it ships. A case that has never failed is not evidence.

---

## Dependencies

**Internal — `economy/market` (shipped):**
`m_aEntityCatalogEntries` (category source), `IsSoldAtShop`, `GetSellPrice`/`GetBuyPrice`/`GetShopBuyPrice`, `GetInventoryId`/`GetResource`/`IsValidResourceId`, `DoAddPlayerMoney`, `m_OnPlayerBuy`/`m_OnPlayerSell`/`m_OnPlayerTransaction`/`m_OnPlayerMoneyChanged`, `m_aAllShops` + `m_aGunDealers`.

**Internal — `economy/shops` (shipped):**
`OVT_ShopComponent` (stock map, `AddToInventory`, `StreamInventory`, `RpcDo_SetInventory`, JIP), `OVT_ShopContext` + `ShopMenu.layout` + `ShopMenu_Card.layout` + `OVT_ShopMenuCardComponent`, `OVT_ShopAction`/`OVT_GunDealerAction` entry points, `RpcAsk_Buy` as the server-authority shape to copy.

**Internal — `OVT_OverthrowController` infrastructure (v1.3.0+):**
Per-player controller entity with lifecycle managed by `OVT_PlayerManagerComponent`; `OVT_ContainerTransferComponent` as the reference component; `OVT_Global.GetController()`.

**Internal — other:**
`OVT_PlayerOwnerComponent` (vehicle lock/ownership), `SCR_HintManagerComponent` (trunk feedback), `OVT_NotificationManagerComponent` (buy failures), `OVT_SkillManagerComponent` + the three economy modifiers (invoker consumers), `OVT_Global.GetPrefabName(item)` and the `map<ResourceName,int> collated` aggregation pattern in `OVT_Global.TransferToWarehouse`.

**External (vanilla, verified during planning):**
`SCR_EArsenalItemType` / `SCR_EArsenalItemMode` / `SCR_ArsenalItem` / `SCR_EntityCatalogEntry`; `InventoryStorageManagerComponent.GetItems(out, EStoragePurpose)` and `TryDeleteItem`; `EStoragePurpose` (bitmask: `PURPOSE_DEPOSIT` vs `PURPOSE_LOADOUT_PROXY` / `PURPOSE_WEAPON_PROXY`); `SCR_VehicleInventoryStorageManagerComponent.GetItems`; `InventoryItemComponent.GetParentSlot()` → `InventoryStorageSlot.GetStorage()/GetPurpose()`, `LoadoutSlotInfo`, `EquipmentStorageSlot`; optionally `FindItems`/`CountItem` + `SCR_ResourceNamePredicate`; `ScriptedUserAction` (`CanBeShownScript`/`CanBePerformedScript`/`SetCannotPerformReason`); `SCR_ScriptedWidgetComponent`, `ItemPreviewWidget`, `SCR_InputButtonComponent`.

**No dependency on** `economy/real-estate` (Phase 7 touches it, and Phase 7 is droppable) or on shop-stock persistence.

---

## Risks & Mitigation

**R1 — Removing `RpcAsk_Sell` breaks an unseen caller.** *(Low, verified)*
`grep` finds exactly one caller: `OVT_ShopContext.c:301`. `OVT_SellDrugsAction` goes through `SellDrugs`/`RpcAsk_SellDrugs`, a different RPC, and is unaffected. **Mitigation:** re-grep immediately before and after deletion; compile-check catches the rest.

**R2 — The `PURPOSE_DEPOSIT` partition does not match the requirement's wording exactly.** *(Low — API pinned, semantics to confirm by play-test)*
The mask is verified to exist and to be used this way by vanilla's arsenal, and it cleanly excludes worn clothing and equipped weapons. Two behaviours to confirm rather than assume: (a) contents of a **worn** backpack/vest are reported (they should be — those storages are `PURPOSE_DEPOSIT` and get registered with the manager when equipped); (b) a **spare** uniform carried inside the backpack is sellable (it should be — it is deposit content, not a loadout slot occupant). **Mitigation:** manual step B7 checks both directions explicitly. If (a) turns out false, fall back to walking `SCR_CharacterInventoryStorageComponent.GetStorages(out array<SCR_UniversalInventoryStorageComponent>)` (the worn-container set) and enumerating each. Either way, the same code runs on client and server, so a wrong answer is wrong *consistently* and under-sells rather than over-credits.

**R3 — Pagination rework regresses the vehicle/procurement browser.** *(Medium)*
The procurement branch has its own list-building path. **Mitigation:** both branches feed the same model; manual step D14 checks the vehicle browser explicitly; the Logic pagination case covers the arithmetic for both.

**R4 — Vehicle cargo enumeration misses storages.** *(Medium)*
`OVT_InventoryManagerComponent`'s transfer path reads `UniversalInventoryStorageComponent.GetOwnedItems()` off the vehicle root only and errors out on vehicles whose cargo lives on attached children. **Mitigation:** the trunk action uses `SCR_VehicleInventoryStorageManagerComponent.GetItems` (all registered cargo storages), the same call `OVT_UnloadStorageAction` already relies on; manual step E16 uses a truck with a separate cargo entity.

**R5 — Selling a weapon destroys its attachments and loaded magazine uncredited.** *(Low, pre-existing)*
Matches current behaviour. **Mitigation:** accept and document; do not silently change pricing. Worth a line in the sell details pane only if play-test shows players losing valuable optics by surprise.

**R6 — Performance.** *(Medium)*
`IsSoldAtShop` is a full catalog scan *per config rule per call*; sell mode would call it per held item per refresh. The trunk action's `CanBeShownScript` runs often and would otherwise do a linear shop scan with a `Replication.FindItem` each time. **Mitigation:** `IsSoldAtShopCached` + `GetItemCategory` caches (Phase 2); the action caches its last shop-lookup result with a short TTL. Watch for hitching in manual steps B6 and E16.

**R7 — Unregistered resources.** *(Medium)*
`GetInventoryId` is a raw map index; looted gear absent from the resource DB would resolve to id 0 — i.e. another item's price. **Mitigation:** `IsRegisteredResource` gates every scanned item on both sides; unregistered items simply do not appear and are never sold.

**R8 — Hand-authored layout/prefab GUIDs.** *(Medium)*
New widgets in `ShopMenu.layout`, the new tab layout, the controller-prefab component entry and the vehicle-prefab action entry all need unique GUIDs, and a missing widget surfaces as a null dereference at runtime, not a compile error. **Mitigation:** generate fresh 16-hex-digit GUIDs; null-guard every new `FindAnyWidget`; have the user open the affected prefabs/layouts once in Workbench to confirm they load. Note the Workbench-stale-scripts trap: after WSL edits, refocus/reload Workbench before re-testing or a "fix that didn't work" may just be a stale script.

**R9 — MP/JIP regressions are invisible to the harness.** *(High likelihood if skipped)*
This feature adds two client→server RPCs and one owner-targeted response. **Mitigation:** manual steps G19-G22 are a hard gate, not optional; the result RPC is display-only so a lost packet costs a toast, not money.

**R10 — Localization keys missing at runtime.** *(Low)*
A missing key renders as raw `#OVT-...`. **Mitigation:** add to `localization_Overthrow.st` (source of truth) and the `en-us.conf` id index; visually confirm every new string during play-test.

**R11 — Scope creep in Phase 7.** *(Medium)*
Six unrelated QOL items in one phase. **Mitigation:** each is independently droppable and none is a dependency of Phases 1-6; drop rather than grow.

**R12 — Grey-out may not read as "disabled" on a 3D `ItemPreviewWidget`.** *(Low)*
Tinting the root may not affect the item preview render. **Mitigation:** follow `OVT_PlaceMenuCardComponent`'s opacity + colour treatment and additionally dim the name/price text; confirm visually in manual step B8.

---

## Quality Bar

This feature is unusual in the project: it is **UI-heavy on the surface and authority-critical underneath**, and it must be held to both bars at once.

**Interaction feel (UI half):**
- **No dead wires.** Every button does its labelled thing, on mouse and on controller. The most recent economy bug in this codebase was three warehouse buttons stacked on one handler; that class of defect is unacceptable here.
- **Live, not stale.** After any transaction the panel a player is looking at reflects reality — quantities, stock, money — without a page flip or a reopen.
- **Predictable state.** Changing tab or mode always lands on page 1 with a valid selection. There is never an empty grid with a `1/0` page label.
- **Nothing disappears silently.** An item the shop won't buy is greyed with a reason, not hidden. A failed purchase says why.
- **Controller navigation keeps working.** Tabs, mode toggle and the new sell buttons are reachable and activatable without a mouse.
- **Every string is localized.** No raw English in new code.

**Server authority (backbone half):**
- **No client-trusted money path survives this feature.** The legacy client-authoritative sell is deleted, not left behind "just in case".
- **The server re-derives everything it acts on** — shop, range, eligibility, price, multiplier, equipped-state, quantity.
- **Bulk means one RPC.** Sell All and the trunk action never degrade into N unit requests.
- **Honest crediting.** Money added equals items actually removed × the server's own price. Partial success is normal and must be reported truthfully.
- **Feedback RPCs are display-only.** No client-side state mutation on the result path.

**Engineering hygiene:**
- Pure decisions live in UI-free classes with Logic-tier cases, and each new case has been proven able to fail once. A case that cannot go red does not ship, and `maxAttempts` is never used.
- New code follows the project's constraints without exception: no ternaries, `ref` for Managed in containers, `RplId` over the network, `OVT_`/`m_` naming, Doxygen `//!` on public methods.
- Every phase leaves `tools/compile-check.sh` clean and the All group green. The buy path's behaviour is bit-for-bit what it was, and the manual regression steps prove it.

---

*Plan created 2026-08-04 from `docs/features/economy/shop-ux/requirements.md` (GitHub issue #145) and a read of the existing shop, economy, controller, UI and user-action code, plus a verification sweep of the vanilla inventory APIs.*
