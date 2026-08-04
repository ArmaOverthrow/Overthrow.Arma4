# Shop UX - Task Checklist

**Epic:** economy
**Last Updated:** 2026-08-04 08:00
**Progress:** 37/37 tasks complete (100%)

> Phase tiers (from implementation.md): Phases 1, 2, 5, 6, 7 = STANDARD; **Phases 3 & 4 = ⚠️ ADVANCED** (`network-specialist-advanced` / `component-developer-advanced`).

---

## Phase 1: Pure helpers + Logic-tier tests (7/7 complete) ✅

- [x] ✅ **1.1 `OVT_ShopCategory` enum + `OVT_ShopCategoryHelper`**
  - Description: Enum (ALL…OTHER) + static mapping `GetCategory(type, mode)` (mode-first!), `GetLabelKey`, `GetDisplayOrder`
  - File(s): `Scripts/Game/Data/OVT_ShopCategory.c` (NEW)
  - Estimate: 🟡 Medium

- [x] ✅ **1.2 `OVT_ShopBrowserItem` + `OVT_ShopBrowserModel`**
  - Description: Add / SortByDisplayName / GetPopulatedCategories / FilterByCategory / GetPageCount / GetPageItems — all pagination arithmetic here (kills BUG-024)
  - File(s): `Scripts/Game/Data/OVT_ShopBrowserModel.c` (NEW)
  - Estimate: 🟡 Medium

- [x] ✅ **1.3 `OVT_ShopSellRules`**
  - Description: ShopBuysFromPlayers, GetSellMultiplier, CanSellItem, GetBlockReasonKey (all static, pure)
  - File(s): `Scripts/Game/Data/OVT_ShopSellRules.c` (NEW)
  - Estimate: 🟢 Small

- [x] ✅ **1.4 `OVT_MoneyFormat`**
  - Description: FormatMoney (thousands separators, negatives, zero) + FormatDelta (signed)
  - File(s): `Scripts/Game/Data/OVT_MoneyFormat.c` (NEW)
  - Estimate: 🟢 Small

- [x] ✅ **1.5 `OVT_MoneyDeltaTracker`**
  - Description: Update(currentMoney, timeSlice) / GetDelta / IsVisible / GetText — accumulate, reset-timer-on-change, clear after timeout, first observation seeds baseline
  - File(s): `Scripts/Game/Data/OVT_MoneyDeltaTracker.c` (NEW)
  - Estimate: 🟡 Medium

- [x] ✅ **1.6 Logic-tier test suite (5 cases)**
  - Description: CategoryMapping, BrowserModelSortAndFilter, Pagination (BUG-024 pin), SellRules, MoneyFormatAndDelta
  - File(s): `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_ShopUX.c` (NEW)
  - Estimate: 🟡 Medium

- [x] ✅ **1.7 Prove each new case red once**
  - Description: Deliberate edit per case, record the method in context.md; no maxAttempts
  - File(s): context.md notes
  - Estimate: 🟢 Small

---

## Phase 2: Economy manager seams (5/5 complete) ✅

- [x] ✅ **2.1 `GetNearestShop(pos, maxDistance)`**
  - Description: Scan `m_aAllShops` **and** `m_aGunDealers`; null-guard Replication.FindItem/GetEntity; null when out of range
  - File(s): `Scripts/Game/GameMode/Managers/OVT_EconomyManagerComponent.c`
  - Estimate: 🟢 Small

- [x] ✅ **2.2 `IsRegisteredResource(ResourceName)`**
  - Description: Guard against unregistered loot resolving to id 0
  - File(s): `Scripts/Game/GameMode/Managers/OVT_EconomyManagerComponent.c`
  - Estimate: 🟢 Small

- [x] ✅ **2.3 `GetItemCategory(int id)` lazy cache**
  - Description: `m_mResourceCategory` built from `m_aEntityCatalogEntries` on first use; unregistered → OTHER
  - File(s): `Scripts/Game/GameMode/Managers/OVT_EconomyManagerComponent.c`
  - Estimate: 🟡 Medium

- [x] ✅ **2.4 `IsSoldAtShopCached(int id, OVT_ShopType)`**
  - Description: Lazy per-shop-type id set; existing `IsSoldAtShop` untouched for current callers
  - File(s): `Scripts/Game/GameMode/Managers/OVT_EconomyManagerComponent.c`
  - Estimate: 🟡 Medium

- [x] ✅ **2.5 Campaign-tier test case**
  - Description: GetNearestShop found at shop origin / null 5km away; category cache non-empty after campaign start; proven red once
  - File(s): `Scripts/Game/Tests/TestSuites/Campaign/` (extend)
  - Estimate: 🟡 Medium

---

## Phase 3: Server-authoritative sell ⚠️ ADVANCED (6/6 complete) ✅

- [x] ✅ **3.1 `OVT_SellableItemScanner`**
  - Description: CollectUnequippedItems (PURPOSE_DEPOSIT), CollectCargoItems (SCR_VehicleInventoryStorageManagerComponent), IsEquipped chain — one impl for client + server
  - File(s): `Scripts/Game/Components/Economy/OVT_SellableItemScanner.c` (NEW)
  - Estimate: 🟡 Medium

- [x] ✅ **3.2 `OVT_ShopTransactionComponent`**
  - Description: SellItems/SellVehicleCargo wrappers, RpcAsk_SellItems/RpcAsk_SellVehicleCargo, RpcDo_SellResult (display-only), ExecuteSell with delete-then-count collated map, OVT_ShopSellResult enum, m_OnSellResult
  - File(s): `Scripts/Game/Components/Controller/OVT_ShopTransactionComponent.c` (NEW)
  - Estimate: 🔴 Large

- [x] ✅ **3.3 Prefab + accessor registration**
  - Description: Add component to `OVT_OverthrowController.et` (fresh GUID) + `OVT_Global.GetShopTransactions()`
  - File(s): `Prefabs/GameMode/OVT_OverthrowController.et`, `Scripts/Game/Global/OVT_Global.c`
  - Estimate: 🟢 Small

- [x] ✅ **3.4 Repoint `OVT_ShopContext.Sell`**
  - Description: Interim: current single-unit Sell button uses the new component (Phase 4 reworks the menu)
  - File(s): `Scripts/Game/UI/Context/OVT_ShopContext.c`
  - Estimate: 🟢 Small

- [x] ✅ **3.5 Delete legacy sell path**
  - Description: Remove `OVT_PlayerCommsComponent.Sell()` + `RpcAsk_Sell()` incl. AK74/RPG7 remap block; grep proves zero callers; SellDrugs unaffected
  - File(s): `Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c`
  - Estimate: 🟢 Small

- [x] ✅ **3.6 PURPOSE_DEPOSIT semantics check**
  - Description: Verify worn vest/backpack/uniform + slung rifle excluded; backpack contents included (compile + code-path review now; runtime confirm is manual step B7)
  - File(s): —
  - Estimate: 🟢 Small

---

## Phase 4: Shop menu rework ⚠️ ADVANCED (6/6 complete) ✅

- [x] ✅ **4.1 `ShopMenu.layout` edits**
  - Description: `Tabs` container, `ModeBuyButton`/`ModeSellButton`, `SellAllButton`, `Message` text — fresh GUIDs, all FindAnyWidget results null-guarded
  - File(s): `UI/Layouts/Menu/ShopMenu.layout`
  - Estimate: 🟡 Medium

- [x] ✅ **4.2 Tab sub-layout + component**
  - Description: `ShopMenu_Tab.layout` + `OVT_ShopMenuTabComponent` (SCR_ScriptedWidgetComponent, OnClick → context)
  - File(s): `UI/Layouts/Menu/ShopMenu/ShopMenu_Tab.layout` (NEW), `Scripts/Game/UI/Menu/ShopMenu/OVT_ShopMenuTabComponent.c` (NEW)
  - Estimate: 🟡 Medium

- [x] ✅ **4.3 Card component enabled/reason + Stock fix**
  - Description: `Init(..., bool enabled = true, string reasonKey = "")`, dim/tint when disabled, make `Stock` visible when qty != -1 (fixes never-rendering stock)
  - File(s): `Scripts/Game/UI/Menu/ShopMenu/OVT_ShopMenuCardComponent.c`
  - Estimate: 🟡 Medium

- [x] ✅ **4.4 `OVT_ShopContext` rework**
  - Description: mode/tab/page state, BuildBuyModel + BuildSellModel (scanner→aggregate→price→IsSoldAtShopCached→enabled/reason), tab row rebuild (<2 categories hidden), page/selection reset on tab/mode change, Sell/Sell All wiring, mode toggle hidden per ShopBuysFromPlayers, handler teardown in OnClose
  - File(s): `Scripts/Game/UI/Context/OVT_ShopContext.c`
  - Estimate: 🔴 Large

- [x] ✅ **4.5 Input action `OverthrowShopSellAll`**
  - Description: Add to chimeraInputCommon.conf + OverthrowShopContext action refs (tab-cycle on MenuNavLeft/Right = nice-to-have)
  - File(s): `Configs/System/chimeraInputCommon.conf`
  - Estimate: 🟢 Small

- [x] ✅ **4.6 Localization keys**
  - Description: All new strings as `#OVT-` keys in .st + en-us.conf id index
  - File(s): `Language/localization_Overthrow.st`, `Language/localization_Overthrow.en-us.conf`
  - Estimate: 🟢 Small

---

## Phase 5: Vehicle trunk "Sell All" (3/3 complete) ✅

- [x] ✅ **5.1 `OVT_SellVehicleCargoAction`**
  - Description: CanBeShownScript (eligible shop in range + cargo, throttled TTL), CanBePerformedScript (lock/ownership + driver-must-exit), PerformAction → SellVehicleCargo
  - File(s): `Scripts/Game/UserActions/OVT_SellVehicleCargoAction.c` (NEW)
  - Estimate: 🟡 Medium

- [x] ✅ **5.2 Vehicle prefab registration**
  - Description: Add action to `Vehicle_Base.et` with `#OVT-SellCargoHere` UIInfo
  - File(s): `Prefabs/Vehicles/Core/Vehicle_Base.et`
  - Estimate: 🟢 Small

- [x] ✅ **5.3 Hint feedback**
  - Description: m_OnSellResult → SCR_HintManagerComponent hint: sold count, FormatMoney earnings, skipped count
  - File(s): `Scripts/Game/UserActions/OVT_SellVehicleCargoAction.c`
  - Estimate: 🟢 Small

---

## Phase 6: Feedback & polish (4/4 complete) ✅

- [x] ✅ **6.1 Shop inventory-changed invoker**
  - Description: `m_OnInventoryChanged` fired from RpcDo_SetInventory; ShopContext subscribes on show, unsubscribes on close
  - File(s): `Scripts/Game/Components/Economy/OVT_ShopComponent.c`, `Scripts/Game/UI/Context/OVT_ShopContext.c`
  - Estimate: 🟢 Small

- [x] ✅ **6.2 Send `PurchaseFailedInventoryFull`**
  - Description: From RpcAsk_Buy complete-failure branch (funds ok, zero delivered); no other buy change
  - File(s): `Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c`
  - Estimate: 🟢 Small

- [x] ✅ **6.3 In-menu toast**
  - Description: m_OnSellResult + buy notifications → Message widget with fade timer; localized
  - File(s): `Scripts/Game/UI/Context/OVT_ShopContext.c`
  - Estimate: 🟡 Medium

- [x] ❌ **6.4 ×5 buy (YAGNI gate)** — DROPPED per gate: server side free, but no gamepad input remains on this screen; would be a screen redesign, not polish
  - Description: Only if 6.3 plumbing makes it a two-line addition; drop otherwise
  - File(s): `Scripts/Game/UI/Context/OVT_ShopContext.c`
  - Estimate: 🟢 Small

---

## Phase 7: Companion QOL — independently droppable (6/6 complete) ✅

- [x] ✅ **7.1 `FormatMoney` everywhere touched**
  - Description: ShopContext, ShopMenuCardComponent, PortContext, WarehouseContext, RealEstateContext, OVT_EconomyInfo:296
  - File(s): see description
  - Estimate: 🟡 Medium

- [x] ✅ **7.2 Alphabetical sort for port + warehouse**
  - Description: Reuse Phase 1 sort helper in both Refresh() methods
  - File(s): `Scripts/Game/UI/Context/OVT_PortContext.c`, `OVT_WarehouseContext.c`
  - Estimate: 🟢 Small

- [x] ✅ **7.3 Warehouse "Take All"**
  - Description: Button beside 1/10/100; take RPC already accepts quantity
  - File(s): `Scripts/Game/UI/Context/OVT_WarehouseContext.c` + layout
  - Estimate: 🟢 Small

- [x] ✅ **7.4 Real-estate NPE guards + feedback**
  - Description: Guard `m_mOwned[m_sPlayerID]` at :89/:233; route silent returns through the Phase 6 toast; comment on optimistic refresh
  - File(s): `Scripts/Game/UI/Context/OVT_RealEstateContext.c`
  - Estimate: 🟡 Medium

- [x] ✅ **7.5 Warehouse stale-refresh fix**
  - Description: Refresh on warehouse-data update (invoker pattern), not immediately after async ask
  - File(s): `Scripts/Game/UI/Context/OVT_WarehouseContext.c`
  - Estimate: 🟢 Small

- [x] ✅ **7.6 HUD money-delta ticker**
  - Description: OVT_EconomyInfo drives OVT_MoneyDeltaTracker (green +, red −, accumulate, clear after timeout); client-side only
  - File(s): `Scripts/Game/UI/HUD/OVT_EconomyInfo.c` + layout if needed
  - Estimate: 🟡 Medium

---

## Bugs & Issues

**Active Bugs:** (none — BUG-083 fixed, pending play-test confirm)

**Fixed Bugs:**
- [x] ✅ 🐛 **BUG-083 — equipped items sellable** - Fixed 2026-08-04 (pending runtime confirm)
  - Fix: root cause = character loadout storage declares StoragePurpose 0x9 (DEPOSIT|LOADOUT_PROXY, Character_Base.et:176) so the PURPOSE_DEPOSIT query legitimately returns worn gear. Scanner now post-filters via class-cast slot/storage classification (fail-safe → equipped); mask kept as first-pass optimisation only. Client+server share the classifier.

**Fixed by construction (when phases land):**
- [x] ✅ 🐛 **BUG-024 shop pagination arithmetic** — model adopted by menu 2026-08-04 (incl. procurement branch)
- [x] ✅ 🐛 **BUG-020 client-authoritative sell (shop half)** — legacy RpcAsk_Sell deleted 2026-08-04; grep proves zero callers
- [x] ✅ 🐛 **Card `Stock` widget never renders** — fixed 2026-08-04 (visible when qty != -1)

---

## Technical Debt

- [ ] 💳 **Int resource ids stay the wire format** — epic-level debt, explicitly out of scope (D7); `IsRegisteredResource` mitigates
- [ ] 💳 **`RpcAsk_SellDrugs` has the same GetItems(PURPOSE_ANY) shape** — out of scope, separate work

---

## Testing Tasks

- [x] ✅ **Logic tier:** 5 new cases in `OVT_TEST_Logic_ShopUX.c`, each proven red once (2026-08-04)
- [x] ✅ **Campaign tier:** OVT_TEST_Campaign_ShopUXSeams (4 claims), proven red once (2026-08-04)
- [ ] **Manual play-test checklist** — implementation.md "Verification Method" steps A1–H26 (UI, MP/JIP, trunk action, equipped-item safety) — **human gate, not automatable**

---

## Progress Tracking

### Completed This Session (2026-08-04)
- ✅ Phase 1 complete: 5 pure helper classes + OVT_TEST_Logic_ShopUX.c (5 cases); compile clean; Fast 27/27; all 5 cases proven red once via recorded single-line mutations
- ✅ Phase 2 complete: 4 economy manager seams + OVT_TEST_Campaign_ShopUXSeams; compile clean; All 52/52; Campaign case proven red once
- ✅ Phase 3 complete (ADVANCED): OVT_SellableItemScanner + OVT_ShopTransactionComponent + prefab/accessor; legacy Sell/RpcAsk_Sell deleted (AK74/RPG7 remap gone); compile clean; All 52/52
- ✅ Phase 4 complete (ADVANCED): menu rework (tabs/mode split/model pagination/Stock fix/toast early); compile clean; All 52/52; static dead-wire audit passed (widgets incl. vanilla PagingButtons import, actions, 22 loc keys all resolve)
- ✅ Phase 5 complete: OVT_SellVehicleCargoAction (1s TTL visibility cache, ownership/pilot gates, localized hint) + Vehicle_Base.et registration; compile clean; All 52/52; zero new loc keys needed
- ✅ Phase 7 complete (ui-developer): all 6 QOL items (FormatMoney sweep incl. port cards, port/warehouse sort, Take All button KC_SEMICOLON/LB, real-estate NPE guards + 8-key message line, warehouse invoker refresh, HUD delta ticker); compile/tests/conflict checker green
- ✅ Phase 6 complete (ui-developer): m_OnInventoryChanged invoker (host+client symmetric), PurchaseFailedInventoryFull wired, notification invoker + in-menu buy-result toast with fade, KC_S→KC_F rebind; ×5 buy dropped per YAGNI gate; compile/tests/conflict-checker all green (independently confirmed)

### Discovered New Tasks
- [x] ✅ **Review finding 1 (MAJOR): selling a loaded container destroyed contents uncredited** — fixed 2026-08-04: ExecuteSell skips non-empty universal-inventory containers (skippedCount++); weapons/assembled vests unaffected (different storage class)
- [x] ✅ **Review finding 2 (minor): 50ms stock refresh cancelled 400ms post-sell recheck** — fixed: separate trampolines (RefreshPostSell / RefreshFromStockEvent), OnClose cancels both
- [x] ✅ **Rebind `OverthrowShopSell` off `KC_S`** — moved to KC_F 2026-08-04; baseline entry removed; checker 0 errors, 12 pre-existing (was 13) (collides with reserved MenuDown — baselined bug on the shop screen; skill rule: fix it since we're reworking this screen, and delete its BASELINE line in `check-input-conflicts.py` baseline) — assign to Phase 6 UI agent
- [x] ✅ Phase 4 MUST use `OVT_ShopTransactionComponent.ShopBuysResource()`/`GetSellUnitPrice()` for grey-out, NOT `IsSoldAtShopCached` directly (no-rules shop types: gun dealer/drugstore accept anything priced — server/menu drift otherwise)

### Blocked Items
- (none)

---

*Update this file as tasks are completed. Mark tasks with ✅ immediately when done. Add new tasks as they're discovered.*
