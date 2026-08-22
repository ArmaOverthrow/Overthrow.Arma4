# Transfer UI (logistics/ui) - Context & Decisions

**Last Updated:** 2026-08-21
**Current Phase:** Complete — Ready for Review
**Status:** 🟢 Play-test GREEN (user, 2026-08-21) at both consumers; three polish tweaks applied after it and awaiting a short re-check.

---

## Quick Status

**What's Done:**
- ✅ Requirements formalized + implementation plan (2026-08-20); 8 user decisions recorded in requirements.md
- ✅ Phase 1: `OVT_TransferEntry` / `OVT_TransferListModel` / `OVT_TransferCartModel` + 9 Logic cases, all fail-proven. Fast suite 355/356 — all 9 new cases green; the one red is pre-existing (see Gotchas)
- ✅ Phase 2 (ADVANCED): 3 layouts + 2 row components + `OVT_TransferContext` base + 7 actions/1 ActionContext + 19 `.st` keys. Compile 0; conflict checker already at the end-of-feature baseline (0/0/3/0/1); `.st` braces 1834/1834

- ✅ Phase 3: `OVT_TabHostContext` + `OVT_ShopMenuTabComponent` retyped + shop reparented + `OVT_TransferContext` wired to the shared tab component. `ShopMenu_Tab.layout` byte-identical
  - **I3 overrun, recorded not hidden:** the `OVT_ShopContext` diff is **15 insertions + 3 deletions = 18 changed lines**, not the 15 first reported — `git diff --numstat HEAD` reads `15	3`. DoD I3 caps it at ≤ 15, so the feature misses I3 by three lines. Behaviour is unchanged (reparent, one `CreateTab` argument, `GetCategory` → `GetTabId`, two forwarding overrides) and `ShopMenu_Tab.layout` is byte-identical, so the diff is deliberately **not** shrunk to hit the number.

- ✅ Phase 4: `OVT_PortContext` is a thin `OVT_TransferContext` subclass; prefab block retargeted at `TransferMenu.layout` / `OverthrowTransferContext` (instance GUID `{5D5AD8A58B5B6AF8}` kept); old port screen, row layout, row handler, both `OverthrowPortBuy*` actions and `ActionContext OverthrowPortContext` deleted. Conflict checker 0/0/3/0/1; compile 0

- ✅ Phase 5: `OVT_WarehouseContext` is a 229-line `OVT_TransferContext` subclass (was 365 L of its own screen); prefab block retargeted (instance GUID `{5D48C36A6BAF83B3}` kept); old warehouse screen, row layout, row handler, all four `OverthrowWarehouseTake*` actions and `ActionContext OverthrowWarehouseContext` deleted; `OVT_TEST_Init_TransferContexts.c` added. Conflict checker 0/0/3/0/1; compile 0; `.st` braces 1836/1836

- ✅ Phase 6: conflict checker exit 0 at baseline (plain **and** `--warnings`); `.st` audit clean (23 runtime keys, two empty `Comment`s filled, braces 1836/1836, six dead keys retained); tutorials + Field Manual a genuine no-op (no Ports or Warehouses page exists) with one `retireval` typo fixed. **The wiki pass is blocked** — see Blockers.
- ✅ Cross-phase review + fix pass: 2 blockers (focus on open, focus after Accept) and 5 should-fixes applied, 2 nits, 2 findings deliberately declined with reasons. `OVT_TEST_LogicSuite` **203/203 green**; the new `CartRemoveKeepsOrder` case fail-proven; the Init case re-verified green.

**What's Next (all human / environment, no code owed):**
1. Workbench: open the three layouts and `Character_Player.et` (both context blocks), and confirm Accept/Close still render label + glyph after the `"no focus" 0` override.
2. Re-export the string table from Workbench — every new `#OVT-Transfer_*` key renders raw until then (expected, D11).
3. Play-test mouse (implementation.md §6 steps 9–12), then **gamepad-only** (13–17) — the focus fixes above are exactly what that session exists to check, plus the open R3 question and the cart-line focus-escape repro below.
4. Re-run the wiki pass in a session that has the `wikijs` MCP server attached.
5. A clean Fast-group verdict, once the unrelated virtualization/movement hang is dealt with.

**Blockers:**
- ⛔ **Wiki sync (task 6.4) could not run** — the `wikijs` MCP tools are not attached to this session (tool surface absent, not an auth failure). Nothing on the wiki was searched, read or written, so any wiki text naming the six deleted actions is still wrong. The verified binding table for the re-run is in this file.

---

## Key Files

### Created by this feature
- `Scripts/Game/Data/OVT_TransferEntry.c` - entry + image/value-kind enums (Phase 1)
- `Scripts/Game/Data/OVT_TransferListModel.c` - rows, sort, categories (Phase 1)
- `Scripts/Game/Data/OVT_TransferCartModel.c` - cart lines, arithmetic, Reconcile (Phase 1)
- `Scripts/Game/UI/OVT_TabHostContext.c` - tab-host intermediate class (Phase 3)
- `Scripts/Game/UI/Context/OVT_TransferContext.c` - the base screen (Phase 2)
- `UI/layouts/Menu/TransferMenu.layout` (+ Row/CartLine sublayouts) (Phase 2)
- `Scripts/Game/UI/Menu/TransferMenu/OVT_TransferRowComponent.c`, `OVT_TransferCartLineComponent.c` (Phase 2)

### Rewritten / touched
- `Scripts/Game/UI/Context/OVT_PortContext.c` - thin subclass (Phase 4)
- `Scripts/Game/UI/Context/OVT_WarehouseContext.c` - thin subclass (Phase 5)
- `Scripts/Game/UI/Context/OVT_ShopContext.c` - ~11-line reparent diff only (Phase 3)
- `Scripts/Game/UI/Menu/ShopMenu/OVT_ShopMenuTabComponent.c` - generalised types (Phase 3)
- `Configs/System/chimeraInputCommon.conf` - +7 actions +1 context, −6 actions −2 contexts
- `Prefabs/Characters/Factions/INDFOR/FIA/Character_Player.et` - two context blocks retargeted
- `Language/localization_Overthrow.st` - ~20 new keys (user must re-export the .conf)

### Deleted
- `PortMenu.layout` + row layout + `OVT_PortItemComponent.c` (Phase 4)
- `WarehouseMenu.layout` + row layout + `OVT_WarehouseInventoryItemComponent.c` (Phase 5)

---

## Important Decisions

Plan decisions D1–D11 live in `implementation.md` §5 — not restated here. Session-level decisions land below as they happen.

- **Reserved GUID series `6A8E2C1…`** (verified unused 2026-08-20): `…10` layouts, `…11` conf/input, `…12` widget-component instances.

---

## Gotchas & Learnings

- **Pre-existing suite red (not this feature):** `OVT_TEST_Init_CompositionSlotGate_AcceptedTypesMatchTheCompositions` — "config 'Base Fortifications' builds compositions but authors no OVT_CompositionSlotConditionDeploymentModule". Red on 2026-08-21 with only this feature's 4 new pure-data files in the tree; the v1.5 suite run was owed since the 08-20 merge. Belongs to base-defense/deployments, not logistics/ui.
- Phase 1 model judgement calls: `Add` re-syncs an existing line's `m_iMaxQuantity` from the entry it was handed (merge clamps against *current* stock); `Reconcile` only lowers caps (a raised cap leaves the line untouched) and drops a line clamped to 0.
- **⚠️ `WLib_NavigationButton` is NOT focusable — D5 could not work as planned.** `WLib_InputButton.layout` (its base) sets `"no focus" 1` at the widget level, so an empty-`m_sActionName` Accept button would have been mouse-only and unreachable on a pad. Fixed by overriding `"no focus" 0` on `AcceptButton` and `CloseButton` (vanilla precedent: `ContentBrowser_EnableAddonButton_Focusable.layout`). The other five nav buttons stay non-focusable — they have real actions. **This is the one edit that touches an inherited widget property; a mistake shows as a blank or mis-sized button, so it is on the Workbench check list.**
- Follow-on from that: `WLib_NavigationButton` has no `Background`/`Border` child, so `SCR_ButtonBaseComponent`'s focus visuals no-op on it. The context draws its own focus highlight by subscribing `m_OnFocus`/`m_OnFocusLost` on the Accept/Close input components and tinting the label accent orange — 4 extra subscriptions, all mirrored in `OnClose`.
- **Dead `.st` keys retained on purpose (7 as of 2026-08-21).** The six `OVT-Port_Buy_*` / `OVT-Warehouse_Take_*` keys, plus `OVT-Transfer_SummaryEmpty` — orphaned 2026-08-21 when `GetSummaryText()` began returning `""` on an empty cart so `CartEmptyLabel` is the screen's only empty-state message. Deleting a key is a structural `.st` edit with a data-loss failure mode (unbalanced braces = the next Workbench save eats entries), so they stay. Braces 1836/1836.
- `SCR_InputButtonComponent.IsKeybindAvailable` returns false on an empty `m_sActionName`, so `PortMenu.layout:341/:355`'s shape draws **no glyph**. `CloseButton`/`SelectHint` therefore follow `ShopMenu.layout` instead (real `m_sActionName`), which is a correction to §3.3's cited precedent. D5's trap does not apply to `SelectHint` because nothing subscribes to its `m_OnActivated`.
- `SCR_SpinBoxComponent.ClearAll()` → `SetInitialState()` reads `m_aElementNames.Count()` with **no null guard** (`SCR_SpinBoxComponent.c:135-139`, `:233-244`); `m_aElementNames` is only `new`-ed by the first `AddItem` (`SCR_SelectionWidgetComponent.c:14-15`). `RefreshDestinations` therefore returns before touching the box unless it previously held items (`OVT_TransferContext.c:625`), and both first consumers offer ≤ 1 destination so the box is never filled at all.
  - **Counter-evidence, so `storage` does not chase a ghost:** `m_aElementNames` is `[Attribute()]`-declared, and `HandlerAttached` runs the same `SetInitialState()` on *every* spin box in the game (`:91`), so a layout-instantiated picker evidently gets a non-null empty array from the container serializer. The guard at `:625` is belt-and-braces, not load-bearing. The path `storage` will actually hit first is different: a picker whose **first** fill has ≥ 2 destinations calls `ClearAll()` while `m_iDestinationItems == 0`, i.e. on an empty box. Verify that in the Workbench before designing around it.
- `tools/compile-check.sh` accepted `int x = "err"` and a missing semicolon before `}` in a throwaway probe, but does catch unknown types. **The gate proves symbol resolution, not full syntactic strictness.**

---

## Needs human verification

- **Workbench, possible now:** open all three new layouts. Confirm `TransferMenu.layout` resolves `DestinationSpin` (`{C9DF0E6590F6C388}WLib_SpinBox.layout`), `ListRows`/`CartLines` are empty containers, and `DetailsPreview`/`DetailsImage` are overlay siblings in `DetailsImageSlot`.
- **Workbench, highest value:** confirm Accept and Close still render label + glyph after the `"no focus" 0` override (the one inherited-property edit).
- Workbench: open `Character_Player.et` after Phase 4/5 retarget the two context blocks.
- Play-test mouse (implementation.md §6 steps 9–12) and gamepad-only (13–17).
- **The two focus fixes from the 2026-08-21 review pass — highest-value pad checks, both were blockers:**
  1. Open the port from the vehicle menu **on a pad only**. Something must be focused the instant the screen appears (row 0), and d-pad up/down must move immediately. Before the fix the first row was *tinted* but focus was nowhere and the d-pad did nothing.
  2. Build a cart, press `a` on Accept. Focus must land back on a list row. Before the fix it stayed on Accept, which `RefreshCheckout()` had just disabled.
- **R3, the open question:** from the last cart line, does d-pad **down** walk out of the `CartScroll` ScrollLayout to `AcceptButton`? If not, apply the plan's named fallback — an explicit `SetFocusedWidget` on `MenuDown` from the last cart line. Report what focus actually does.
- **Destroying the focused cart line may push focus out of both panes.** `AfterCartChanged` re-homes focus only `if(FocusIsInPanes())` (`OVT_TransferContext.c:1414`), and `ClearCartLines()` destroys the focused widget a few lines earlier. If the engine reparents focus to `DestinationSpin` or `AcceptButton` rather than leaving it null, focus silently leaves the cart while `m_ePane` still reads CART and the three buttons keep saying "Remove". **Repro:** focus a cart line and press `x` repeatedly until the line drops; report where focus ends up.
- **Pane swap:** watch for a *double* move on d-pad left/right (engine directional search + the context's own handler). The context only acts when focus is already inside one of the two columns, but the code cannot prove this.
- **D6 is not testable yet** — it needs a consumer offering two destinations, and neither first consumer does.
- `.st` re-export by the user (new keys render raw until then — expected, D11).
- **Warehouse invoker, the one thing only a play-test can show:** empty a stocked row with Accept and watch the row leave the list ~50 ms later WITHOUT a second Accept, with the cart line gone and focus still somewhere real. Then open and close the warehouse three times and Accept once — exactly one redraw per event proves `OnClose` removed the `m_OnWarehouseInventoryChanged` subscription.
- Workbench: `Character_Player.et`'s `OVT_WarehouseContext` block now carries `m_RowLayout` / `m_CartLineLayout` / `m_TabLayout` and no `m_ItemLayout`; confirm it opens without a dropped-attribute warning.

---

## Testing

- Logic tier: `OVT_TEST_Logic_TransferModels.c` — **10 cases** (9 from Phase 1 + 1 from the 2026-08-21 review pass). Mutation record (mutation → observed-expected failure):
  - ListSortAlphabetical: comparison `> 0`→`< 0` → "Sorted position 0 is 'cherry', expected 'apple_upper'"
  - ListCategoryPopulation: drop ALL-skip + de-dupe guards → "returned 4 ids, expected 2" *(observed against the Phase 1 fixture; the case was rewritten 2026-08-21 — see the review-pass block below)*
  - ListFilterByCategory: drop `CATEGORY_ALL` branch → "Filter by CATEGORY_ALL returned 0 rows, expected 3"
  - CartAddMerges: always-insert (`index = -1`) → line-count assertion fires first ("2 lines, expected 1")
  - CartClampsToMax: drop merge clamp → "clamped to 15, expected the cap of 10"
  - CartAddAll: `+= max` instead of delegating to clamping Add → "gave 14, expected exactly 10"
  - CartRemoveDropsLine: clamp-and-keep instead of drop → "left 1 lines, expected 0"
  - CartTotals: drop unit-value multiply → "TotalValue() is 5, expected 80"
  - CartReconcile: early `return` no-op → "cart holds 3 lines, expected 2" (note: unreachable-code mutation compiles with no warning) *(extended 2026-08-21)*
- **Review-pass cases (2026-08-21) — mutations DERIVED FROM THE FORMAT STRINGS, not yet observed.** Compile-check is green; the orchestrator runs the suite. Apply one mutation at a time:
  - `CartRemoveKeepsOrder` (**NEW**): revert any one of the four `m_aLines.RemoveOrdered(...)` calls in `OVT_TransferCartModel` to `m_aLines.Remove(...)`. Expected, per site:
    - `RemoveAll` (`:127`) → "RemoveAll on the second of four lines left the cart as [a,d,c], expected [a,c,d]"
    - `Remove` (`:115`) → "Removing the second of four lines to zero left the cart as [a,d,c], expected [a,c,d]"
    - `Reconcile` vanished branch (`:224`) → "Reconcile dropping a vanished second line left the cart as [a,d,c], expected [a,c,d]"
    - `Reconcile` clamp-to-zero branch (`:240`) → "Reconcile clamping the second line to zero left the cart as [a,d,c], expected [a,c,d]"
    - ⚠️ The fixture is FOUR lines with the **second** dropped on purpose. With three lines, swap-removal and ordered removal produce the same array and the mutation would pass.
  - `ListCategoryPopulation` (**changed**): delete the insertion-position `while` loop in `OVT_TransferListModel.GetPopulatedCategories` and `Insert` instead of `InsertAt` → "GetPopulatedCategories() returned [7, 3, 5], expected ascending [3, 5, 7]"
  - `CartAddMerges` (**changed**): delete the `if (line.m_iQuantity <= 0) return;` guard in `OVT_TransferCartModel.Add`'s insert branch → "Adding an entry whose cap is 0 inserted a zero-quantity line: 3 lines, out_of_stock sits at index 2"
  - `CartReconcile` (**changed**): delete `line.m_iUnitValue = entry.m_iValue;` from `Reconcile` → "Reconcile left the kept line's copied display fields stale: unit value 4, name 'Kept (repriced)'". Delete `line.m_sDisplayName = entry.m_sDisplayName;` instead → same message with "unit value 9, name 'Kept'". Delete both → "unit value 4, name 'Kept'". (The `TotalValue() != 43` assertion is the second net: it fires as "After Reconcile TotalValue() is 28, expected 43 (3 x 9 kept + 4 x 4 drained)" if the unit-value line alone is reverted *and* the field assertion is also removed.)
- Init tier: `OVT_TEST_Init_TransferContexts.c` — one case, `OVT_TEST_Init_TransferContexts_ShareOneScreen`, four claims: (1) both consumers resolve via `OVT_UIManagerComponent.GetContext`; (2) both `Cast` to `OVT_TransferContext`; (3) both carry the same `m_Layout` and the same `m_sContextName`; (4) `InputManager.ActivateContext("OverthrowTransferContext")` returns true — **guarded by a negative control**.
  - **`ActivateContext` on an unknown name could not be settled statically.** It is a native proto (`ArmaReforger/scripts/GameLib/generated/Input/ActionManager.c:20`, `proto external bool ActivateContext(string, int duration = 0)`) and no vanilla call site reads its return. So the case settles it at runtime: it activates `"OverthrowNoSuchContextExistsAnywhere"` first, and if THAT returns true it prints a note and **skips** claim 4 instead of asserting something vacuous. Either way the case says which happened.
  - **Runtime answer (orchestrator, 2026-08-21):** the negative control returned **false**, so claim 4 was really asserted, not skipped. `ActivateContext` does *not* return true for an undeclared context name.
  - **Fail-proven once (orchestrator, 2026-08-21).** Mutation: claim 3's `port.m_Layout != warehouse.m_Layout` → `port.m_Layout != "{DEADBEEF00000000}UI/Layouts/Menu/MutationProbe.layout"`. Observed: `⛔ OVT_TEST_Init_TransferContexts_ShareOneScreen: FAILURE — The two transfer consumers point at DIFFERENT layouts: port '{6A8E2C1000000001}UI/Layouts/Menu/TransferMenu.layout', warehouse '{6A8E2C1000000001}UI/Layouts/Menu/TransferMenu.layout'. …` (`run-tests: FAILED (1 of 1)`). Mutation reverted; the same single-case run is `run-tests: OK (1 tests, 14s)`.
  - The four per-claim mutations below are **derived from the format strings, not observed** — the case as a whole is proven able to fail, but each individual claim's message is not. Walk them if one ever needs to be trusted verbatim.
    - delete the `OVT_PortContext` block from `Character_Player.et` → "OVT_UIManagerComponent.GetContext(OVT_PortContext) returned null. The context is not in m_aContexts on Prefabs/Characters/Factions/INDFOR/FIA/Character_Player.et, so the port Import screen can never open - no compile error, no runtime error, no log line."
    - `OVT_TransferContext.Cast(port)` → `OVT_ShopContext.Cast(port)` (expected-value mutation) → "OVT_PortContext is not an OVT_TransferContext subclass (it is a OVT_PortContext). …"
    - revert the warehouse block's `m_Layout` to `{09449D49971A6A7A}UI/Layouts/Menu/WarehouseMenu.layout` → "The two transfer consumers point at DIFFERENT layouts: port '{6A8E2C1000000001}UI/Layouts/Menu/TransferMenu.layout', warehouse '{09449D49971A6A7A}UI/Layouts/Menu/WarehouseMenu.layout'. …"
    - revert the warehouse block's `m_sContextName` to `OverthrowWarehouseContext` → "The two transfer consumers declare DIFFERENT ActionContext names: port 'OverthrowTransferContext', warehouse 'OverthrowWarehouseContext'. …"
    - delete `ActionContext OverthrowTransferContext` from the conf → either the claim-4 failure ("InputManager.ActivateContext('OverthrowTransferContext') returned false while the same call for an undeclared name also returns false. …") or the skip note, which is itself the answer to the `ActivateContext` question.
- Suites: orchestrator-run, once per phase where they cover anything (Phase 1 and 5; layout/conf-only phases announced-skip).
- **Fast-group run after Phase 5 (2026-08-21, orchestrator):** 356 passed, 1 failed, then the client **hung** and the run timed out with no junit.xml (exit 124, i.e. *no verdict* for the group).
  - The failure is the pre-existing `OVT_TEST_Init_CompositionSlotGate_AcceptedTypesMatchTheCompositions` (base-defense/deployments).
  - The hang is `OVT_TEST_Init_VirtualMovement_StationaryPlanIsNeverAdvanced` (`autotest_current.log`), and `crash.log` also carries a `"Entity is already deleted"` VM exception in `OVT_DeploymentComponent.DestroyDeployment` from `OVT_TEST_Init_ObjectiveDirector.Teardown`. Both belong to virtualization/movement and objectives, not to logistics/ui — this feature touches no virtualization, deployment or director code.
  - This feature's own coverage was confirmed separately by single-case runs (above): the Init case is green and fail-proven. **A clean Fast-group verdict is still owed** once the movement hang is dealt with.
- **Logic suite after the review fix pass (2026-08-21, orchestrator): `run-tests: OK (203 tests, 2s)`, exit 0.**
  - `OVT_TEST_Logic_TransferModels_CartRemoveKeepsOrder` **fail-proven**: mutating `OVT_TransferCartModel.c:127` `RemoveOrdered` → `Remove` produced `run-tests: FAILED (1 of 203)` with exactly that one case red (`⛔ OVT_TEST_Logic_TransferModels_CartRemoveKeepsOrder: FAILURE`). Mutation reverted; the suite is green again. The other three `RemoveOrdered` sites and the changed `ListCategoryPopulation` / `CartAddMerges` / `CartReconcile` mutations remain **derived, not observed**.
  - `OVT_TEST_Init_TransferContexts_ShareOneScreen` re-run after the fix pass: `run-tests: OK (1 tests, 13s)`.
- **Transient `INDETERMINATE` (exit 2) seen once** on both `compile-check.sh` and `run-tests.sh` while a user Workbench instance (PID 23820) was open — the addon was absent from the `Loaded addons:` block. Both went green on the next attempt with no code change. Concurrent Workbench + CI runs contend on `<project>/resourceDatabase.rdb`; treat an isolated exit 2 during a Workbench session as contention, not as a verdict.

---

## Bindings — for the blocked wiki pass

Read out of `Configs/System/chimeraInputCommon.conf` (`ActionContext OverthrowTransferContext`, Priority 50, Flags 4) and cross-checked against both consumers, 2026-08-21. Whoever re-runs task 6.4 can write the wiki from this without re-deriving it.

| Action | Keyboard | Gamepad | Notes |
|---|---|---|---|
| `OverthrowTransferMode1` | `1` | `shoulder_right` | Inert on both current screens (one mode each) |
| `OverthrowTransferMode2` | `2` | `view` | Inert on both current screens |
| `OverthrowTransferPrevCategory` | `Q` | `thumb_left` | Port only; the warehouse has one category |
| `OverthrowTransferNextCategory` | `E` | `thumb_right` | Port only |
| `OverthrowTransferQtyOne` | `,` | `x` | Add 1 / Remove 1 |
| `OverthrowTransferQtyTen` | `.` | `y` | Add 10 / Remove 10 |
| `OverthrowTransferQtyAll` | `;` | `right_trigger` | Add all / Remove all; **retired at the port** (button hidden, which kills the keybind) |
| `OverthrowTransferAccept` | `F` | `left_trigger` | Added 2026-08-21; disabled (and so unpressable) whenever the cart is invalid |
| `MenuUp` / `MenuDown` | vanilla | d-pad up/down | Browse the focused column |
| `MenuLeft` / `MenuRight` | vanilla | d-pad left/right | Swap list ↔ cart; drives the destination picker when it holds focus (D6) |
| `MenuSelect` | vanilla | `a` | Activates the focused widget (row, tab, or Accept while Accept has focus) |
| `MenuBack` | vanilla | `b` | Close |

The six `Menu*` actions are vanilla and the mod does not redefine them. `shoulder_left` (VON) is untouched. Facts worth putting on the page: the port caps at 100 per item (`OVT_PortContext.c:9`, mirroring the server's protected `IMPORT_MAX_QUANTITY`), the warehouse clamps to stock, "Add all" is warehouse-only, and the refusal strings are `#OVT-Transfer_NoVehicle` (both) plus `#OVT-CannotAfford` (port), returned by `ValidateCart` before anything is sent.

The wiki must also stop naming the six deleted actions: `OverthrowPortBuyTen`, `OverthrowPortBuyHundred`, `OverthrowWarehouseTakeOne/Ten/Hundred/All`.

---

## Session Notes

- **2026-08-21:** Feature started via /autorun-feature (Discord). Scaffolded from the 2026-08-20 plan.
- **2026-08-21:** Phase 1 complete (`component-developer`): 4 files, compile OK, Fast 355/356 (the red is the pre-existing CompositionSlotGate case above). Phase 2 handed to `ui-developer-advanced`.
- **2026-08-21:** Phase 2 complete (`ui-developer-advanced`). Suites skipped per test policy — layouts/`.conf`/`.st`/UI scripts, which the suites do not cover. Repo path is `UI/Layouts/` (capital L); the plan's `UI/layouts/` was loose prose. Carry-forward notes for later phases:
  - **Phase 4 must call `ScheduleRefresh()` at the end of the port's `OnAccept`** — the base defines it but never calls it (D9 forbids an optimistic refresh, and only the port lacks an invoker). The warehouse must NOT call it.
  - Title comes from the active mode's label key (no title hook was added — the hook surface stays closed at eight), falling back to `#OVT-Transfer_Title`.
  - The "All" tab label is base-owned (`#OVT-Transfer_CategoryAll`); `GetCategoryLabelKey` is only ever asked about real ids.
  - Switching mode does not clear the cart — `Reconcile` drops what the new mode no longer offers. Unreachable with one mode, but it is the defined behaviour.
  - Two message modes: a `ValidateCart` reason is a *standing* condition shown without the fade timer; Accept's confirmation uses the shop's 4 s fade.
  - `#OVT-Transfer_NoVehicle` is defined but unused until Phase 4/5 implement `ValidateCart`.
  - Phase 4 found `#OVT-Transfer_DestinationVehicle` missing from the `.st` master (`BuildDestinations` labels its one destination with it). Added as `{6A8E2C1D00000014}`, "Vehicle"; braces 1836/1836.
  - The `OverthrowPortBuy*` deletion takes one of the two shared-`InputSource`-GUID pairs' *duplicate* half with it — `{5D48A6A7AE022164}` / `{5D48A6A672B025AF}` now belong solely to `OverthrowWarehouseTakeOne` / `TakeTen`, which Phase 5 deletes.
  - `OVT_TransferContext` is in no `m_aContexts` array, so **the screen cannot be opened until Phase 4 registers a consumer.**
- **2026-08-21:** Phase 3 complete (`component-developer`). Suites skipped — **no test in the tree references a UI context at all** (`grep` over `Scripts/Game/Tests/` for `UIManagerComponent|GetContext|UIContext|OVT_ShopContext` returns zero files), so a run could not have covered this phase. Shop regression stays a play-test item (§6 step 12). Notes:
  - **`ClearTabs` is asymmetric and stays that way.** `OVT_TransferContext.ClearTabs` (`:867-875`) destroys the tab widgets without calling `Cleanup()` on their components, so a queued `Activate` survives `OnClose`; the host cannot cancel it (the context's old `ClearTabs` used to `CallQueue.Remove(ApplyPendingTab)`). Harmless: a late `Activate` sets `m_iTab` then hits `Refresh()`'s `if(!m_bIsActive) return;` guard, and `OnShow` resets `m_iTab`. This is the shop's shipped exposure copied verbatim, so no cancel hook was added — reviewed again 2026-08-21 and deliberately left alone.
  - `OVT_ShopCategoryHelper.GetLabelKey` verified real at `Scripts/Game/Data/OVT_ShopCategory.c:104`.
  - The enum-through-a-local form (`OVT_ShopCategory cat = tabId; SelectTab(cat);`) was used pre-emptively per the plan's note — direct passing was never tested, so this is the conservative form.
  - No double-fire: `grep m_OnClicked OVT_TransferContext.c` returns nothing; the only live path is `SCR_ButtonComponent.m_OnClicked → OVT_ShopMenuTabComponent.OnTabClicked → CallLater(Activate, 0) → m_Host.SelectTabId`.
- **2026-08-21:** Phase 5 complete (`ui-developer`). Suites skipped per test policy (the orchestrator's Fast run covers the new Init case). Notes:
  - The warehouse's coalescer is its own method (`RefreshStock`, 50 ms) rather than the base's `ScheduleRefresh` (400 ms) — two separate `CallQueue` entries, so neither cancels the other, and the base constant stays untouched. `OnAccept` deliberately schedules **nothing** (D9): the invoker is the source of truth.
  - `RefreshSelected`'s selection-repair dance is gone. `Refresh()` → `BuildList()` already runs `m_Cart.Reconcile(m_Model)` and `RestoreFocus()`, which is strictly more than the old repair did.
  - The redundant `MenuBack → CloseLayout` `RegisterInputs`/`UnregisterInputs` override is gone (§3.5); `OVT_UIContext.RegisterInputs` binds `m_sCloseAction` from the prefab.
  - `BuildDestinations` / `GetOccupiedVehicle` are character-for-character identical to `OVT_PortContext`'s. Left duplicated on purpose — the hook surface and the base are closed this phase. If a third consumer needs them, that is the trigger to lift them into the base.
  - Deleting the four `OverthrowWarehouseTake*` actions retired `InputSourceSum` GUIDs `{5D48A6A7AE022164}` / `{5D48A6A672B025AF}`, closing the pre-existing shared-GUID defect whose other halves went with the port actions in Phase 4.
  - `#OVT-TakeFromWarehouse` (reused as the warehouse's mode label and screen title) had an empty `Comment`; filled in. Braces still 1836/1836. The six dead `OVT-Port_Buy_*` / `OVT-Warehouse_Take_*` keys are left in place for Phase 6 as planned.
  - The warehouse's one category id is a literal `0` and never `CATEGORY_ALL`, so `GetPopulatedCategories` returns one id and the tab row stays hidden with no consumer code.

- **2026-08-21: cross-phase review fix pass (`ui-developer-advanced`).** Seven review findings applied plus two nits; no hook was added (the surface stays closed at eight), no layout, conf, prefab, `.st` or server file was touched. Compile-check exit 0; conflict checker unchanged at 0/0/3/0/1.
  - **Two focus blockers, both gamepad-only and both invisible to every static check.** `Refresh()` decides `restoreFocus = FocusIsInPanes()` *before* the rebuild, which is right for an invoker-driven redraw (B1: a player parked on the picker or on Accept keeps that focus) and wrong for the two entry points where focus is legitimately outside the panes. Fixed with two explicit `RestoreFocus()` calls rather than a `Refresh(forceFocus)` parameter, so the warehouse's 50 ms coalesced redraw keeps its current behaviour exactly:
    - `OnShow` (`OVT_TransferContext.c:247`) — on arrival focus is still on the vehicle menu's button (`OVT_VehicleMenuContext.c:101`/`:86`), because `ShowContext()` → `ShowLayout()` → `OnShow()` is synchronous and runs *before* that menu's `CloseLayout()` (`:186-188`, `:165-167`). So `FocusIsInPanes()` was false, nothing was focused, and `PaintSelection()` still tinted row 0 — it looked selected while the d-pad did nothing. The deleted port screen focused unconditionally.
    - `Accept` (`:1454`) — Accept runs with `AcceptButton` focused, so `Refresh()` left focus there and `RefreshCheckout()` then `SetEnabled(false)`'d it over the now-empty cart.
  - `array.Remove()` is the **swap-with-last** removal (`ArmaReforger/scripts/Core/proto/Types.c:258-263`), so all four cart drop sites are now `RemoveOrdered`. A middle line was teleporting the last line into its slot while `m_iCartIndex` stayed put, so `AfterCartChanged` re-focused and re-detailed the wrong item.
  - `Reconcile` now re-copies `m_iUnitValue` and `m_sDisplayName` (`OVT_TransferCartModel.c:230-231`). D2 says lines copy display fields *and* `Reconcile` handles the staleness; it only handled disappearance and a fallen cap. At the port a tab or mode change re-reads prices into the model while the cart kept the old ones, so `TotalValue()`, the summary, the drawn line value and the affordability gate all ran on a stale price.
  - `GetPopulatedCategories` returns ids **ascending** (`OVT_TransferListModel.c:120-135`). First-seen order was driven by which item happens to sort first in each category; ascending reproduces `OVT_ShopCategoryHelper.GetDisplayOrder` exactly at the port (the enum is declared in display order) without the model touching a UI class. It also stops `TabOrderMatches` seeing a reordered list with unchanged membership and rebuilding the tab widgets, which is the D4 invariant.
  - `Accept` calls `RefreshDestinations()` before `GetSelectedDestination()` (`:1425`). `m_aDestinations` was only rebuilt by `Refresh()`, and a destroyed `IEntity` does not null its handle, so `ValidateCart`'s `!dest.m_Entity` check could not see it. Safe for the picker: `RefreshDestinations` is `m_bRebuildingDestinations`-guarded and restores the index with `SetCurrentItem(..., invokeOnChanged: false)`, and for both first consumers it returns before touching the spin box at all.
  - `Add` refuses to insert a zero-quantity line (`OVT_TransferCartModel.c:70-73`). `ClampQuantity` has no lower bound, so an entry with `m_iMaxQuantity <= 0` produced an "x0" line that only `Clear()` could remove and that enabled Accept. Unreachable for both first consumers; the base's `ChangeQuantity` only checks `m_bEnabled`, so a later consumer would have hit it immediately.
  - `RefreshList` clamps `m_iListIndex` **after** the row loop, against `m_aRowWidgets.Count()` (`:568-571`) — every downstream consumer indexes that array, which is shorter when a row fails to instantiate.
  - **Deliberately NOT changed:** `ClearTabs`'s missing `Cleanup()` (the shop's shipped exposure, recorded above); the `OVT_ShopContext` diff (18 changed lines, over I3 — corrected above rather than shrunk).
  - **Not covered by the review, found while applying it:** `Add`'s *merge* branch has the same zero-quantity hole as the insert branch — `existing.m_iQuantity = ClampQuantity(existing.m_iQuantity + qty, entry.m_iMaxQuantity)` with a cap that has since fallen to 0 leaves a zero-quantity line in place, violating `OVT_TransferCartLine.m_iQuantity`'s own "always in [1, m_iMaxQuantity]" doc. Left alone: dropping a line as a side effect of `Add` is surprising, and the UI cannot reach it (`RefreshActionButtons` disables the quantity buttons when `m_iMaxQuantity <= 0`, and the warehouse skips zero-stock rows). `Reconcile` is the intended repair path. `storage` should decide this explicitly.
  - **Also not covered:** `RefreshCart` clamps `m_iCartIndex` against `lines.Count()` before building `m_aCartWidgets`, the same shape as the list nit — but `m_iCartIndex` is read against *both* arrays (`GetCartLine` wants the line array, `GetCartComponent`/`GetPaneFocusTarget` want the widget array), so clamping it to one of them is not obviously right. Left as-is; both readers bounds-check.
  - **A doc claim in the review that did not hold:** the review read §3.7's "the same rule the shop already follows" as a claim about category *order*. That sentence is about *header visibility*, which really is the shop's rule. The plan's actual defect was silence about ordering — now stated in §3.2, §3.7's table and §7's case table.

- **2026-08-21 (autorun close-out, orchestrator):** Phases 4–6 run, then a cross-phase review and its fix pass. Verdicts, all mine in the main thread: `compile-check` exit 0; `check-input-conflicts.py` exit 0 both plain and `--warnings` at the `0/0/3/0/1` baseline; `OVT_TEST_LogicSuite` **203/203**; `OVT_TEST_Init_TransferContexts_ShareOneScreen` green and fail-proven; `.st` braces 1836/1836. The Phase-5 Fast-group run hung on `OVT_TEST_Init_VirtualMovement_StationaryPlanIsNeverAdvanced` (unrelated) so the group has no verdict — targeted runs stand in. Task 6.4 (wiki) is the one thing left undone, blocked on a missing MCP server. Nothing was committed; the whole change set is uncommitted in the working tree on `v1.5`.

- **2026-08-21 (play-test polish, `ui-developer`).** Three user-requested items after the green port + warehouse play-tests. Compile-check exit 0; conflict checker exit 0 plain **and** `--warnings`, unchanged at `0 error(s), 0 warning(s), 3 combo note(s), 0 pre-existing, 1 acknowledged.`; `.st` untouched, braces still 1836/1836; no duplicate widget GUIDs in `TransferMenu.layout`.
  1. **One empty-cart message.** `GetSummaryText()` returns `""` on an empty cart (`OVT_TransferContext.c:202`) and `SummaryText`'s seeded `Text` is `""` (`TransferMenu.layout:334`), so only `CartEmptyLabel` speaks. `RefreshCheckout` is the single consumer and just `SetText`s it; nothing branches on the summary being non-empty. `#OVT-Transfer_SummaryEmpty` is now a dead key — see the retention note above.
  2. **Cart has its own background.** `CartScroll` + `CartEmptyLabel` are wrapped in a new `CartPanel` `OverlayWidgetClass` holding `CartBackground` (black, Opacity **0.3** — deliberately lighter than `ListBackground`'s 0.5 so the two panels read as different) plus a `CartContent` `VerticalLayoutWidgetClass`. Copied from this layout's own `ListPanel`/`ListBackground` pair, which is also the `ShopMenu.layout` `Content`/`Background` shape. `CartPanel` takes the fill slot and the `Padding 8 0 8 0` that `CartScroll` used to carry, so the cart occupies exactly the space it did. No `ALWAYS_TOP`, nothing focusable added, no `AlignableSlot` padding involved (the two new slots are `OverlayWidgetSlot` with align 3, and the measured padding sits on the outer `LayoutSlot`). New GUIDs `{6A8E2C1000001033/34/35}`. `CartLines` / `CartEmptyLabel` are found with `FindAnyWidget` so the extra nesting is transparent; `CartScroll` is not referenced from script at all.
  3. **Accept has a real binding — D5 superseded, not reversed.** New action `OverthrowTransferAccept` (`keyboard:KC_F` / `gamepad0:left_trigger`, own GUIDs `{6A8E2C1100000080}`…`{…84}`), added to `ActionContext OverthrowTransferContext`, and set as `AcceptButton`'s `m_sActionName`. D5's trap was specific to *sharing* `MenuSelect`; a dedicated action cannot collide with it. **No context-side listener was added** — `SCR_InputButtonComponent.RegisterActionListeners` (`:602-613`) calls `AddActionListener(m_sActionName, DOWN, OnButtonPressed)` → `OnInput` → the same `m_OnActivated` that `WireWidgets` already subscribes `Accept` to, so a second path would double-fire. The disabled gate holds: `m_bCanBeDisabled` defaults to `1` (attribute default, not overridden by `WLib_NavigationButton` or `WLib_InputButton`) and `OnInput` bails on `!IsEnabledInHierarchy()` (`:731`), so `RefreshCheckout`'s `SetEnabled(canAccept)` really does block the key and the pad trigger, not just the mouse.
     - `KC_F` is the mod's established menu-confirm key (`OverthrowShopSell`, `OverthrowJobsAccept`, `OverthrowLoadoutsApply`); `left_trigger` is held by four actions on other screens, which the checker accepts because none share this context.
     - **`SelectHint` kept.** It advertises `MenuSelect`, not Accept, and `a` still activates whatever row/tab/button holds focus — that hint is still true and still the only thing telling a pad player so. It is no longer *needed* for Accept's glyph, and a pad player now sees both "Accept LT" and "Select A" in the footer. No double-fire: `a` on a focused `AcceptButton` reaches `Accept` once (engine button activation), and nothing subscribes to `SelectHint`'s own `m_OnActivated`. Flagged for the user rather than removed.
  - **Not verifiable here:** the layout, the `.conf` and the visual result are outside the automated spine. The background shade, the checkout panel's spacing with an empty summary line, and the new glyph all need eyes in the Workbench.

- **2026-08-21 — PLAY-TEST GREEN (user).** Confirmed working at both the port Import and warehouse Take consumers. Three polish items raised and applied the same session: (1) the checkout summary no longer prints "Nothing ordered yet" — the cart's own "Nothing added yet" is the single empty-state message, and `#OVT-Transfer_SummaryEmpty` joins the retained-dead-key list; (2) the cart region got its own `CartPanel`/`CartBackground` pair at opacity 0.3 against the list's 0.5, copied from this layout's own `ListPanel` shape; (3) **Accept gained a real binding** — `OverthrowTransferAccept` on `KC_F` + `gamepad0:left_trigger`, which **supersedes D5**. D5's trap was specific to reusing `MenuSelect`; a dedicated action cannot collide with it, needs no context-side listener (`SCR_InputButtonComponent.OnInput` already routes the named action to the `m_OnActivated` the context subscribes to — a second listener would double-fire), and makes the glyph render. `KC_F` matches the mod's existing menu-confirm convention (`OverthrowShopSell`, `OverthrowJobsAccept`, `OverthrowLoadoutsApply`). Conflict checker unchanged at `0/0/3/0/1`; compile 0.
  - **Open question for the user:** the footer now shows both "Accept LT" and "Select A", because `SelectHint` advertises `MenuSelect` generally and is still truthful. It is inert (nothing subscribes to it) and can be deleted (`TransferMenu.layout:455-473`) if the two hints read as clutter.
