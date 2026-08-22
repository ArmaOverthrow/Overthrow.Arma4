# Transfer UI (logistics/ui) - Task Checklist

**Last Updated:** 2026-08-21
**Progress:** 43/44 tasks complete (98%) — 32/33 phase tasks + 11/11 review-pass fixes. The only open task is **6.4, the wiki pass, blocked on the missing `wikijs` MCP server**.

> Phase 2 is flagged **ADVANCED** (`ui-developer-advanced`) per implementation.md §4/Agent Routing.

---

## Phase 1 — Pure models + Logic-tier cases (`component-developer`, ~3-4 h)

- [x] 1.1 `Scripts/Game/Data/OVT_TransferEntry.c` — entry class + `EOVT_TransferImageKind` + `EOVT_TransferValueKind`; explicit constructor
- [x] 1.2 `Scripts/Game/Data/OVT_TransferListModel.c` — Add/Clear/Count/GetEntries/FindById/SortByDisplayName/HasCategory/GetPopulatedCategories/FilterByCategory, `CATEGORY_ALL = -1`
- [x] 1.3 `Scripts/Game/Data/OVT_TransferCartModel.c` — `OVT_TransferCartLine` + Add/AddAll/Remove/RemoveAll/Clear/Count/GetLines/GetQuantity/FindLineIndex/TotalQuantity/TotalValue/Reconcile
- [x] 1.4 `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_TransferModels.c` — 9 cases per §7, registered on `OVT_TEST_LogicSuite`; no manager/game-mode identifiers anywhere in the file
- [x] 1.5 Fail-prove every case once; record mutation + message in context.md
- [x] 1.6 `tools/compile-check.sh` exit 0

## Phase 2 — Layout, base context, ActionContext, focus & gamepad ⚠️ ADVANCED (`ui-developer-advanced`, ~8-12 h)

- [x] 2.1 `UI/layouts/Menu/TransferMenu.layout` + `.meta` — §3.3 tree, GUIDs `6A8E2C10…`, inherited component GUIDs copied
- [x] 2.2 `TransferMenu/TransferMenu_Row.layout` + `TransferMenu_CartLine.layout` + `.meta`s
- [x] 2.3 `OVT_TransferRowComponent.c` + `OVT_TransferCartLineComponent.c` — Init/SetSelected, `m_OnClicked` + guarded `OnClick` fallback + `m_OnFocus` reporting
- [x] 2.4 `OVT_TransferContext.c` — attributes, OnShow/OnClose (symmetric teardown), header/tabs, Refresh* methods, focus model §3.5 incl. D6 picker guard, 8 hooks with defaults, message fade
- [x] 2.5 `chimeraInputCommon.conf` — 7 actions + `ActionContext OverthrowTransferContext`, GUIDs `6A8E2C11…`, no shared InputSources
- [x] 2.6 `Language/localization_Overthrow.st` — `#OVT-Transfer_*` keys (no `Configs/Language/*.conf` writes)
- [x] 2.7 Gates: duplicate-GUID dedupe clean; `check-input-conflicts.py` exit 0 at 2026-08-20 baseline (0/0/3/0/1); compile-check exit 0

## Phase 3 — Tab-host class + shop generalisation (`component-developer`, ~2-3 h)

- [x] 3.1 `Scripts/Game/UI/OVT_TabHostContext.c` — two virtuals only
- [x] 3.2 `OVT_ShopMenuTabComponent` retyped `(int tabId, string labelKey, OVT_TabHostContext)`; deferred-activate + wiring preserved verbatim
- [x] 3.3 `OVT_ShopContext` ~11-line diff (reparent, CreateTab arg, GetTabId, two overrides)
- [x] 3.4 `OVT_TransferContext` reparented to `OVT_TabHostContext`; tab row wired to shared component + `ShopMenu_Tab.layout`
- [x] 3.5 Gates: ShopContext diff ≤ 15 lines; `ShopMenu_Tab.layout` byte-identical; compile-check exit 0

## Phase 4 — Port consumer + delete old port screen (`ui-developer`, ~3-4 h)

- [x] 4.1 Rewrite `OVT_PortContext` as `OVT_TransferContext` subclass — `CollectImportables` verbatim (illegal gate + BUG-102 rules), PRICE column, max 100, shop category mapping, one mode
- [x] 4.2 `IsAddAllAllowed`=false; `BuildDestinations`=occupied vehicle; `ValidateCart` no-vehicle + cannot-afford; `OnAccept` loops `ImportToVehicle` per line
- [x] 4.3 Retarget `Character_Player.et` port block at `TransferMenu.layout` + shared context name (keep instance GUID)
- [x] 4.4 Delete `PortMenu.layout`(+meta), `PortMenu/PortInventoryItem.layout`(+meta), `OVT_PortItemComponent.c`, `OverthrowPortBuy*` actions, `ActionContext OverthrowPortContext`
- [x] 4.5 Gates: repo grep for old port symbols = 0 hits; `OVT_VehicleMenuContext.Import()` untouched; compile-check + conflict checker exit 0

## Phase 5 — Warehouse consumer + deletions + Init case (`ui-developer`, ~3-4 h)

- [x] 5.1 Rewrite `OVT_WarehouseContext` as subclass — QUANTITY column, stock=max, `SetWarehouse()` signature kept, one mode, `IsAddAllAllowed`=true
- [x] 5.2 Invoker path: `m_OnWarehouseInventoryChanged` subscribe/unsubscribe (cached manager), 50 ms coalesce, `Reconcile` + redraw; no optimistic refresh; `OnAccept` loops `TakeFromWarehouseToVehicle`
- [x] 5.3 Retarget prefab block; delete `WarehouseMenu.layout`(+meta), row layout(+meta), `OVT_WarehouseInventoryItemComponent.c`, 4 `OverthrowWarehouseTake*` actions, `ActionContext OverthrowWarehouseContext`
- [x] 5.4 `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_TransferContexts.c` — both contexts resolve, same `m_Layout` + `m_sContextName`, ActivateContext claim verified-or-dropped
- [x] 5.5 Gates: repo grep for old warehouse symbols = 0 hits; compile-check + conflict checker exit 0

## Review fix pass — cross-phase findings (`ui-developer-advanced`, 2026-08-21)

Applied between Phase 5 and Phase 6. Review findings, not new scope; no hook added, no layout / conf / prefab / `.st` / server change.

- [x] R.1 `OnShow` and `Accept` each call `RestoreFocus()` after `Refresh()` — the two entry points where focus is legitimately outside both panes (gamepad blockers)
- [x] R.2 All four cart drop sites use `RemoveOrdered`; `array.Remove()` is swap-with-last
- [x] R.3 `Add` refuses to insert a zero-quantity line
- [x] R.4 `GetPopulatedCategories` returns ids ascending (tab order + `TabOrderMatches` stability)
- [x] R.5 `Reconcile` re-copies `m_iUnitValue` / `m_sDisplayName` (D2's stated purpose)
- [x] R.6 `Accept` re-resolves destinations before validating
- [x] R.7 `RefreshList` clamps `m_iListIndex` after the row loop, against the widget count
- [x] R.8 Four comment blocks trimmed to the sparse-comment rule (`OVT_TEST_Init_TransferContexts.c` header, three in `OVT_PortContext.c`)
- [x] R.9 Logic case `CartRemoveKeepsOrder` added; `ListCategoryPopulation`, `CartAddMerges`, `CartReconcile` extended — mutations recorded in context.md, **suite run owed to the orchestrator**
- [x] R.10 Docs: I3 overrun recorded honestly (18 changed lines, not 15); category-order rule stated in implementation.md §3.2/§3.7/§7; cart-line focus-escape added to "Needs human verification"; `ClearTabs` asymmetry re-pinned
- [x] R.11 Gates: `tools/compile-check.sh` exit 0; `check-input-conflicts.py` 0/0/3/0/1 exit 0

## Phase 6 — Conflict check, localization, help & wiki (main thread + `help-docs-sync`, ~2-3 h)

- [x] 6.1 Final `check-input-conflicts.py` (plain + `--warnings`) at baseline — both exit 0, `0 error(s), 0 warning(s), 3 combo note(s), 0 pre-existing, 1 acknowledged.`
- [x] 6.2 `.st` audit — all 23 runtime keys present; filled the two empty `Comment`s (`OVT-CannotAfford`, `OVT-Import`); braces 1836/1836; the six dead port/warehouse keys left in place and noted
- [x] 6.3 Tutorial/Field Manual grep — genuine no-op (no Ports or Warehouses page exists; the four surviving port/warehouse mentions describe economics, not the replaced buttons). One `retireval` typo fixed in passing
- [ ] 6.4 ⛔ **BLOCKED** — `help-docs-sync` wiki pass. The `wikijs` MCP server is not attached to this session, so nothing on the wiki was searched, read or written. Needs a session with the server attached; the verified binding table is in context.md so the re-run is cheap
- [x] 6.5 `.st` re-export asked for (Workbench-only, user action — new keys render raw until then, per D11)

---

## Orchestrator gates (per phase, main thread)

- compile-check after every phase; conflict checker after conf-touching phases
- `tools/run-tests.sh` Fast once after Phase 1 and once after Phase 5 (new Logic/Init cases); phases 2-4, 6 are UI/conf/doc-only — announced skip per test policy
- Actual: Phase 5's Fast run hung on an unrelated virtualization case (no group verdict). Coverage confirmed instead by targeted runs — `OVT_TEST_LogicSuite` **203/203 green**, `OVT_TEST_Init_TransferContexts_ShareOneScreen` green, both fail-proven. A clean Fast-group verdict is still owed
