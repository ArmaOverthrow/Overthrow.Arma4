# Shop UX - Context & Decisions

**Epic:** economy (feature #4 of 4)
**Last Updated:** 2026-08-04 08:00
**Current Phase:** Complete — pending human play-test
**Status:** 🟡 In Progress

---

## Quick Status

**What's Done:**
- ✅ Requirements (issue #145) + full implementation plan (planned 2026-08-04 with vanilla API verification sweep)
- ✅ Dev docs scaffolded, feature started
- ✅ Phase 1: 5 pure helper classes in `Scripts/Game/Data/` + `OVT_TEST_Logic_ShopUX.c` (5 cases). Compile clean, Fast 27/27, every case proven red once
- ✅ Phase 2: 4 manager seams (GetNearestShop scans shops+dealers; IsRegisteredResource; GetItemCategory + IsSoldAtShopCached lazy caches with anti-poisoning guards) + `OVT_TEST_Campaign_ShopUXSeams`. All 52/52
- ✅ Phase 3 (ADVANCED): scanner + `OVT_ShopTransactionComponent` (RpcAsk_SellItems / RpcAsk_SellVehicleCargo / RpcDo_SellResult display-only), registered on controller prefab (GUID {6A7A1F3C4B29D0E5}) + `OVT_Global.GetShopTransactions()`; legacy Sell/RpcAsk_Sell deleted. All 52/52
- ✅ Phase 4 (ADVANCED): ShopMenu.layout HeaderRow (mode toggle + tab strip + Q/E–LB/RB cycle buttons), ShopMenu_Tab.layout + OVT_ShopMenuTabComponent, card enabled/reason + Stock fix, OVT_ShopContext rebuilt around mode+tab+model, 5 new input actions, 22 loc keys. Static dead-wire audit green
- ✅ Phase 5: trunk "Sell Cargo Here" action on Vehicle_Base.et (contexts door_r01/door_l01/door_rear, GUIDs {6A7D5C1E4B93F210}/{...11}); HasLocalEffectOnlyScript; subscribe-before-call for listen-server; at most one result handler per instance
- ✅ Phase 6 (ui-developer): live stock via m_OnInventoryChanged (StreamInventory=host side, RpcDo_SetInventory=clients; 50ms coalesced refresh), PurchaseFailedInventoryFull sent from RpcAsk_Buy else-branch, OVT_NotificationManagerComponent.m_OnNotification + in-menu buy-result toast (buy tags only) with AnimateWidget fade, Sell key KC_S→KC_F
- ✅ Phase 7 (ui-developer): FormatMoney sweep (7 shop readouts, port incl. cards, warehouse, real-estate, HUD), port/warehouse alphabetical sort, warehouse Take All (`OverthrowWarehouseTakeAll` KC_SEMICOLON/LB), real-estate zero-buildings NPE fixed + 8 localized feedback messages, `m_OnWarehouseInventoryChanged` invoker (host/client symmetric, 50ms coalesced), HUD delta ticker (poll-driven per D11)

**What's Next:**
- 📋 Human play-test (see Needs Human Verification + BUG-083/review-fix re-test steps), then user handles git/PR

**Blockers:**
- None

---

## Key Files

### To create (Phases 1–5)
- `Scripts/Game/Data/OVT_ShopCategory.c`, `OVT_ShopBrowserModel.c`, `OVT_ShopSellRules.c`, `OVT_MoneyFormat.c`, `OVT_MoneyDeltaTracker.c` — pure logic, Logic-tier testable
- `Scripts/Game/Components/Economy/OVT_SellableItemScanner.c` — shared client/server item enumeration
- `Scripts/Game/Components/Controller/OVT_ShopTransactionComponent.c` — server-authoritative sell RPCs
- `Scripts/Game/UI/Menu/ShopMenu/OVT_ShopMenuTabComponent.c`, `UI/Layouts/Menu/ShopMenu/ShopMenu_Tab.layout`
- `Scripts/Game/UserActions/OVT_SellVehicleCargoAction.c`
- `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_ShopUX.c`

### To edit
- `Scripts/Game/GameMode/Managers/OVT_EconomyManagerComponent.c` — 4 new seams (Phase 2)
- `Scripts/Game/UI/Context/OVT_ShopContext.c` — major rework (Phase 4)
- `Scripts/Game/UI/Menu/ShopMenu/OVT_ShopMenuCardComponent.c` — enabled/reason params + Stock-visibility fix
- `Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c` — DELETE Sell/RpcAsk_Sell; add PurchaseFailedInventoryFull send
- `Scripts/Game/Components/Economy/OVT_ShopComponent.c` — m_OnInventoryChanged invoker
- `Scripts/Game/Global/OVT_Global.c` — GetShopTransactions()
- `Prefabs/GameMode/OVT_OverthrowController.et`, `Prefabs/Vehicles/Core/Vehicle_Base.et`
- `UI/Layouts/Menu/ShopMenu.layout`, `Language/localization_Overthrow.st` + `.en-us.conf`

---

## Important Decisions

See implementation.md **D1–D13** (authoritative). Highlights that shape every phase:
- **D1/D6:** category mapping + pagination are pure functions in `Scripts/Game/Data/` (mode-before-type; BUG-024 dies in the model)
- **D2/D3:** one server sell routine, two entry points; one shared scanner class on both sides — same `EStoragePurpose.PURPOSE_DEPOSIT` mask
- **D8:** new RPCs on `OVT_ShopTransactionComponent` (OVT_OverthrowController); buy stays on legacy comms; legacy `RpcAsk_Sell` deleted, not deprecated
- **D13:** "unequipped" ≡ `PURPOSE_DEPOSIT` — slung/holstered weapons deliberately not sellable

---

## Gotchas & Learnings

(from planning; add build-time findings here)
- Arsenal enums are power-of-two valued but every codebase consumer compares with `==` on single catalogued values — Phase 1 follows that; composite bitmask values fall to OTHER (safe direction)
- There is no `WEAPON` arsenal *type* — "weapon" is a mode; the category table only names real type members
- `OVT_ShopType` is compared as raw `int` throughout the codebase (`m_ShopType` is declared int) — sell rules take `int shopType`
- Reason keys + category label keys are named constants on the helpers: `#OVT-SellBlocked_Equipped/_NotBoughtHere/_NoValue`, `#OVT-ShopCategory_*` (12 keys total — Phase 4 must add them to the string table)
- Model has extra Phase 4 conveniences: `Clear()/Count()/GetItems()/HasCategory()`; tracker: `Reset()/Set|GetResetSeconds()/GetTimeRemaining()`
- `m_aAllShops`/`m_aGunDealers` are `ref array<RplId>`; don't reuse `GetShopByRplId` in loops (unguarded GetEntity) — `FindNearestShopIn` guards everything and skips unresolvable entries
- `IsSoldAtShopCached` replicates IsSoldAtShop semantics: only type/mode/m_sFind matter; DEFAULT mode matches any; no-rule shop types sell nothing (avoids pre-existing null-foreach in IsSoldAtShop); unregistered prefabs excluded by construction (R7)
- Eligibility (IsSoldAtShop) is broader than rolled stock — exactly what "sell a variant the dealer doesn't stock" needs (Phase 3)
- Caches never write while `m_aEntityCatalogEntries` is empty (pre-BuildResourceDatabase calls answer correctly but cache nothing)
- **Phase 3 deviation (load-bearing for Phase 4):** shop types with NO config rules (SHOP_GUNDEALER, SHOP_DRUG — only GENERAL/ELECTRONIC/CLOTHES/VEHICLE have ShopConfig rules) accept ANY priced item via `ResourceIsAccepted`; strict `IsSoldAtShopCached` would make gun dealers buy nothing. **Menu grey-out must call `OVT_ShopTransactionComponent.ShopBuysResource(shop, id)` + `GetSellUnitPrice(shop, id)`, never IsSoldAtShopCached directly**
- `SellItems` rejects resourceId < 0 (collides with m_SelectedResource's -1 = no selection); the -1 wildcard is internal to the trunk path
- Sell result delivery: invoker fired directly for the local player (listen-server host gets no double toast), RPC otherwise
- Range constants: SHOP_MAX_DISTANCE=30 (matches buy), VEHICLE_MAX_DISTANCE=15 (player→vehicle on trunk path)
- Restock mutates shop.m_aInventory + StreamInventory directly (AddToInventory is a client→server forwarder — same approach as legacy)
- Shop paging widgets (PrevButton/NextButton/Pages) come from VANILLA `PagingButtons.layout` imported at ShopMenu.layout:345 — grep Overthrow's UI tree alone and they look missing
- Phase 4: gamepad `x` shared by OverthrowShopBuy and OverthrowShopSellAll — safe ONLY because BuyButton/SellAllButton are mode-exclusive (enforced in RefreshActionButtons); don't break that invariant
- Phase 4: tab row rebuilt only when the tab SET changes (a tab click must not destroy its own widget mid-event); tab activation deferred one call-queue tick
- Phase 4 deviations: procurement cards pass qty -1 (Stock stays hidden there); mode change resets tab to ALL; display-name sort via memoised WidgetManager.Translate; minimal toast + 400ms ScheduleRefresh shipped early (Phase 6 polish still owed)
- OVT_ShopContext is configured on `Prefabs/Characters/.../FIA/Character_Player.et` — m_TabLayout attribute set there AND as script default
- Phase 5: SHOP_MAX_DISTANCE=30 duplicated as local const in the action (component's is protected) — keep in step
- Phase 5: GetNearestShop returns nearest shop, not nearest *eligible* — a vehicle parked closer to a vehicle shop than the general shop shows no action (documented; would need a new manager seam)
- User created ui-developer/-advanced agents + overthrow-ui-patterns skill mid-run (post-Phase 4); Phases 6–7 route to ui-developer; conflict checker: `python3 .claude/skills/overthrow-ui-patterns/scripts/check-input-conflicts.py`
- Phase 6: notifications render ONLY via the HUD strip, and ShowLayout hides the HUD — menus can't see notifications without the new m_OnNotification invoker; OnSellResult keeps a scheduled refresh (sell result RPC can beat the item-removal replication; sell model is a local inventory scan no shop invoker announces)
- Phase 6: ×5 buy dropped — no free gamepad input left on the shop screen (a/b/x/y, shoulders, thumbs, d-pad all spent)
- `GetNearestShop` must scan `m_aAllShops` **and** `m_aGunDealers` — FilterShopEntities excludes dealers from m_aAllShops
- `IsSoldAtShop` is a full catalog scan per rule per call — never call it per-item per-refresh without the Phase 2 cache
- Card `Stock` widget is `Is Visible 0` in the layout and code only ever calls `SetVisible(false)` — stock count has never rendered
- `GetAllRootItems()` is `[Obsolete]` and buggy; use `GetItems(out, purpose)` / `FindItems`
- Workbench-stale-scripts trap: after WSL edits, refocus/reload Workbench before play-testing
- EnforceScript: no ternaries; `ref` for Managed in containers; RplId over the wire

---

## Testing Approach

- **Logic tier (automated):** 5 cases in `OVT_TEST_Logic_ShopUX.c` — category mapping, sort/filter, pagination (BUG-024 pin), sell rules, money format/delta. Each proven red once; method recorded below.
- **Campaign tier (automated, recommended):** GetNearestShop + category-cache case.
- **Manual (human gate):** implementation.md Verification Method steps A–H — menu UI, controller nav, PURPOSE_DEPOSIT semantics (B7/F18), trunk action (E15–17), MP/JIP (G19–22), QOL (H23–26).
- Run `tools/compile-check.sh` every phase; `tools/run-tests.sh "{6A6E2A002F53A581}"` (All) after Phases 1–3.

### Proven-red record (2026-08-04, single-line mutation → run single case → exit 1 → revert → Fast 27/27 green)
- `CategoryMapping` — `OVT_ShopCategory.c:42` mode check `AMMUNITION` → `PYLON` (magazines fell to WEAPONS, the exact issue #145 defect)
- `BrowserModelSortAndFilter` — `OVT_ShopBrowserModel.c:102` sort `Compare(..., false)` → `Compare(..., true)` (case-sensitive order wrong)
- `Pagination` — `OVT_ShopBrowserModel.c:188` `Math.Ceil(float/float)` → `count / perPage` (reintroduces BUG-024: 57/15→3 pages)
- `SellRules` — `OVT_ShopSellRules.c:39` `return gunDealerMultiplier > 0;` → `return true;` (zero-multiplier dealer claims to buy)
- `MoneyFormatAndDelta` — `OVT_MoneyDeltaTracker.c:55` `m_fTimer = m_fResetSeconds;` → `m_fTimer = 0.5;` (timer no longer restarts to window)
- `Campaign_ShopUXSeams` — `OVT_EconomyManagerComponent.c:771` cache write → `GetCategoryForUncatalogued()` (all ids answered OTHER)

---

## Needs Human Verification

(running list — items only a play-test can confirm)
- BUG-083 re-test (11 steps in fix report): worn/slung/held/gadget items absent from Sell; backpack contents + spare uniform inside pack sellable; Sell All leaves loadout intact; MP client parity
- Review-fix re-test: Sell All on a LOADED backpack row → skipped (toast counts it), empty backpack sells; trunk sell leaves loaded packs (second pass sells emptied pack); rifle with mag+optic still sells (R5); assembled ALICE vest still sells; remote-client Sell rows settle post-sale (finding 2 is invisible on a host); close menu within 400ms of selling → no errors
- PURPOSE_DEPOSIT runtime semantics (B7/F18): worn vest/backpack/uniform + slung rifle absent from Sell; backpack contents present; spare uniform inside backpack sellable (enum values not readable in generated dump — engine-side filter)
- Gun dealer sell path (C12) — kept alive only by the Phase 3 no-rules deviation; sell a variant the dealer doesn't stock, credit must reflect multiplier
- Drugstore (SHOP_DRUG) sell — shares the no-rules fallback
- MP/JIP steps G19–G22 — esp. two clients selling the same stack simultaneously (honest-crediting proof)
- ~~Interim behaviour change (Phase 3): Buy-grid sell of variants~~ superseded — Phase 4 Sell mode lists the player's own items; variants are first-class rows
- Phase 4 UI (F1–F11, Q6): tab row/grouping at a general shop, last page reachable (BUG-024 check), Sell toggle absent at vehicle/procurement/zero-multiplier dealer, grey-out + reason at clothes shop, Sell/Sell All quantities, gamepad pass over every new button (Q/E + LB/RB cycle, 1/2 mode, X sell-all), buy-side regression A1–A5
- Workbench GUID check (R8): open ShopMenu.layout, ShopMenu_Tab.layout and Character_Player.et once in Workbench — if tabs never appear but the menu works, the tab-layout GUID didn't resolve
- Trunk action (E15–E17 + agent's 8-step procedure): Vehicle_Base.et loads with 3 actions, no `door_rear` context warning (drop door_rear if it warns); action appears/disappears within ~1s of parking/emptying; locked/driver-seat refusals; only performer sees hint (MP)

---

## Session Notes

### 2026-08-04 01:20
- Feature started via /autorun-feature (autonomous). Plan pre-existed; docs scaffolded; Phase 1 next.
- Orchestration: Phases 3 & 4 route to advanced agents per plan; all work stays uncommitted on branch `feat/economy/shop-ux` (user owns git).

### 2026-08-04 02:05
- Phase 1 done (component-developer agent). compile-check 0; Fast group 27/27 (was 22). All five Logic cases proven red once — record above.
- DEFAULT_RESET_SECONDS on the delta tracker = 4.0s, settable via SetResetSeconds().

### 2026-08-04 08:00 (wrap-up)
- BUG-083 fixed (network-specialist-advanced): root cause was Character_Base.et:176 loadout storage StoragePurpose 0x9 (DEPOSIT|LOADOUT_PROXY) — the purpose mask never was a safe-to-delete partition. IsEquipped now classifies by slot/storage class casts, fail-safe → equipped. NOTE: EStoragePurpose ordinals vs flag bits diverge from GADGET_PROXY on — `& PURPOSE_DEPOSIT` idioms elsewhere (OVT_SpawnLogic) work by luck.
- Cross-phase review (fresh agent): Q1–Q9/I1–I5 verified clean except 1 major + 2 minor. Major (ExecuteSell deleting non-empty containers → contents destroyed uncredited) fixed: HasStoredContents gate via FindComponents(BaseUniversalInventoryStorageComponent)+GetOwnedItems (polymorphism proven by OVT_SpawnLogic precedent); empty containers still sell; weapons (SCR_WeaponAttachmentsStorageComponent, not universal) still sell loaded per R5; assembled vests (ClothNodeStorageComponent) unaffected. Minor refresh-scheduling collision fixed via split trampolines.
- Stray working-tree change NOT ours: Configs/Factions/CIV.conf `m_sFactionRadioEncryptionKey` → `"Radio encryption key"` (Workbench/engine resave after 1.7.0 moved the property to gamecode). Left untouched — user decides before PR.
- Final gates: compile 0; Fast 27/27; All 52/52; conflict checker 0 errors / 12 pre-existing.

### 2026-08-04 06:30
- Phase 7 done (ui-developer, self-verified): all six QOL items, none dropped. compile 0; All 52/52 (one flake explained by concurrent Workbench persistence timeout, two clean re-runs); conflict checker 0/12 baseline.
- Play-test: agent's 27-step checklist covering warehouse (incl. BUG-081 regression + teardown x10), real-estate (zero-buildings NPE, 8 messages), port/shop formatting, HUD ticker accumulate/clear.
- BUG-083 filed (critical): play-test proved GetItems(PURPOSE_DEPOSIT) does NOT exclude worn clothing on 1.7.0 (risk R2). Fix in flight in scanner only.

### 2026-08-04 05:40
- Phase 6 done (ui-developer, self-verified + independently confirmed): compile 0; All 52/52; conflict checker 0 errors, 12 pre-existing (shop KC_S baseline removed).
- Play-test additions for Phase 6: agent's 11-step checklist (live stock, toast fade/overlap, F vs S key, notification scoping, teardown x10, MP).

### 2026-08-04 05:00
- Phase 5 done (component-developer). compile 0; All 52/52. No new loc keys (all six needed already existed).
- Ran overthrow-ui-patterns conflict checker on Phase 4 work: 0 new errors; discovered pre-existing OverthrowShopSell/KC_S vs MenuDown baselined bug on our screen → queued for Phase 6.

### 2026-08-04 04:30
- Phase 4 done (component-developer-advanced). compile 0; All 52/52. Static dead-wire audit: every FindAnyWidget name resolves (paging ones live in vanilla PagingButtons.layout import), every layout ActionName exists in chimeraInputCommon.conf, every #OVT- key used by code exists in the .st + en-us.conf.
- New actions: SellAll KC_X/pad-x, ModeBuy KC_1/thumb_left, ModeSell KC_2/thumb_right, Prev/NextCategory KC_Q/KC_E + shoulders.

### 2026-08-04 03:15
- Phase 3 done (network-specialist-advanced). compile 0; All 52/52; grep: RpcAsk_Sell survives only in one doc comment; controller GUID unique.
- Identity from RPC arrival entity (controller ownership), no playerId in payloads. JIP: component holds no replicated state. Old client ↔ new server = normal version mismatch (prefab component added), not save-compat.

### 2026-08-04 02:35
- Phase 2 done (component-developer agent). compile-check 0; All group 52/52 (was 46). Campaign case proven red once.
- MP note for play-test: gun-dealer half of GetNearestShop's union is only incidentally covered — trunk action at a gun dealer on a client (steps E15/E16 + C13) is the real proof.

---

*Update this file at the end of each work session. Run `/update-feature economy/shop-ux` before compacting conversations.*
