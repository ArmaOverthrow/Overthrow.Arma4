# Transfer UI — Implementation Plan

**Status:** ✅ **Play-test GREEN 2026-08-21** — the user confirmed both consumers (port Import and warehouse Take) work correctly. Three polish items they raised are applied and re-gated: the duplicate empty-cart message removed, the cart panel given its own background, and Accept given a real binding (`KC_F` / `left_trigger`, superseding D5 — see the amendment in §5). **Owed:** a short re-check of those three tweaks, the `.st` re-export (Workbench, user), the wiki pass (blocked — no `wikijs` MCP server), and a clean Fast-group verdict (blocked on an unrelated virtualization/movement hang).
**Started:** 2026-08-21
**Target Completion:** TBD
**Last Updated:** 2026-08-21

**Epic:** `logistics` (feature #1 of 4 — see `docs/features/logistics/epic-overview.md`)
**Requirements:** `docs/features/logistics/ui/requirements.md` (authoritative for scope; 8 user decisions recorded there, none re-opened here)
**Approach:** abstract **List + Cart + Destination** context over two pure models, one shared layout and one shared `ActionContext`; the port Import and warehouse Take screens are rewritten as thin subclasses and their old layouts/handlers/conf blocks deleted. **No server change, no new RPC.**
**Branch:** `v1.5` (concurrent sessions exist on this tree — re-baseline before every phase)

---

## 1. Executive Summary

Overthrow has two transfer screens and they are the same screen written twice. `OVT_PortContext` (284 L) and `OVT_WarehouseContext` (365 L) each own a near-identical `Refresh()` that clears a container, collects a list, insertion-sorts it by display name through a private copy of the same algorithm, instantiates a private copy of the same row layout through a private copy of the same row handler, and focuses the first row. Each has its own display-name memo map, its own details pane, its own `SCR_CompartmentAccessComponent.GetVehicle()` dance, and its own pair of quantity buttons on two actions that **share their `InputSource` GUIDs with each other** (`{5D48A6A7AE022164}` / `{5D48A6A672B025AF}`, `chimeraInputCommon.conf:3` and `:37`). Between them they hold sixteen hand-authored placeholder rows that exist only to be deleted on the first refresh.

Both are also one-item-at-a-time screens. A player stocking a truck at the port presses "Buy 10" eleven times and sends eleven separate server requests, with no way to see what they are about to spend before spending it.

This feature replaces both with **one** screen: a categorized list on the left, a details panel over a **cart** on the right, a destination picker under the cart, and an Accept button. The list gains the shop's tool header (mode toggle + category tabs + steppers), so a 200-row import list becomes browsable instead of a wall. Nothing about the server contract changes — Accept walks the cart and issues the **existing** `ImportToVehicle` / `TakeFromWarehouseToVehicle` request once per line, exactly as the buttons do today. A batched checkout request is `logistics/storage`'s job and is deliberately not written here.

Three commitments shape everything below:

1. **Every decision that can be a pure function is one.** The row set, the alphabetical sort, the category population rule, and all cart arithmetic (add, remove, merge, clamp, drop-at-zero, totals, and reconciliation against a refreshed list) live in two UI-free classes under `Scripts/Game/Data/`, so the Logic tier can assert them without a widget. This is the `OVT_ShopBrowserModel` / `OVT_FuelChargeLedger` house pattern, and it is what makes a cart testable at all.
2. **The base owns the screen; consumers own the data.** `OVT_TransferContext` owns the layout, the widget lookups, the header, the tabs, focus, the input bindings and the cart. A consumer overrides eight small hooks and writes no widget code.
3. **The shop is not touched.** `OVT_ShopMenuTabComponent` and `ShopMenu_Tab.layout` are reused by generalising their two hardcoded types behind an intermediate class both hosts already inherit from. `OVT_ShopContext`'s visible behaviour is unchanged and its diff is ~11 lines.

Net line count is expected to fall: ~650 lines of duplicated context code plus two layouts (376 L + 356 L) and two row handlers become one base (~600 L), two consumers (~150 L each), one layout and two row handlers.

---

## 2. Goals

### Primary

1. **One transfer screen.** An abstract `OVT_UIContext` subclass with one shared layout, one shared `ActionContext` and a small hook surface, that both existing screens and every later `logistics` consumer extend.
2. **A cart.** Build a multi-line order with Add 1 / 10 / All, review it with a running total, remove from it with the same three buttons relabelled, then commit it with one Accept.
3. **A browsable list.** The shop's mode toggle + category tabs + prev/next steppers, hidden automatically when a consumer has fewer than two modes / fewer than two populated categories / neither.
4. **Fully operable on a gamepad.** d-pad up/down browses the focused column, left/right swaps columns, down past the cart lands on the destination picker, `a` accepts, `b` closes. Verbs on `x` / `y` / `RT` / `RB` / `view` / `L3` / `R3` — never on `a`, `b`, the d-pad or `shoulder_left`.
5. **Two consumers, replacement not parallel.** `OVT_PortContext` (Import only, price column, no "Add all") and `OVT_WarehouseContext` (Take only, stock column, "Add all") become thin subclasses; the old layouts, row layouts, row handlers and conf blocks are deleted in the same phases.
6. **Logic-tier coverage** for both models, every case proven able to fail once.

### Secondary

1. **Kill a pre-existing input defect for free.** The six deleted port/warehouse actions include the two GUID-sharing pairs; the seven new ones each get their own GUID triple.
2. **Turn two silent failures into messages.** `OVT_PortContext.Buy()` returns silently when the player is not in a vehicle (`:262-264`); `OVT_WarehouseContext.Take()` does the same (`:327-331`). The checkout area now says so.
3. **A reusable tab host.** After this feature a third menu can host category tabs by inheriting one class and writing two methods.

### Explicitly out of scope

Everything in the requirements' Out of Scope section, restated only where an implementer might reach for it anyway:

- **No Export, no vehicle-inventory reader** (`storage` owns it), **no batched checkout RPC**, **no server change of any kind**, **no additional destinations** beyond the occupied vehicle.
- **No resources, ledgers or m³ capacity.** The base must be *able* to draw a texture-image entry; it may not gain a resource-shaped hook, field or branch.
- **No change to the shop / gun-dealer screens** or to `OVT_ShopBrowserModel`'s paging.
- **No per-row quantity fields, no search, no sort orders beyond alphabetical.**
- **No speculative hooks "for later."** The hook list in §3.4 is closed; a later feature adds what it needs when it needs it.

---

## 3. Architecture Overview

### 3.1 Where each piece lives

```
Scripts/Game/Data/                                   PURE, Logic-tier testable, no widgets
├── OVT_TransferEntry.c          NEW   entry + EOVT_TransferImageKind + EOVT_TransferValueKind
├── OVT_TransferListModel.c      NEW   rows, alphabetical sort, category population/filter, find-by-id
└── OVT_TransferCartModel.c      NEW   OVT_TransferCartLine + cart arithmetic + reconcile

Scripts/Game/UI/
├── OVT_UIContext.c                    UNCHANGED
├── OVT_TabHostContext.c         NEW   OVT_UIContext subclass; the "tab host interface" (§3.6)
├── Context/
│   ├── OVT_TransferContext.c    NEW   the base screen: layout, header, tabs, focus, cart, inputs
│   ├── OVT_PortContext.c        REWRITE  → OVT_TransferContext subclass (~150 L)
│   ├── OVT_WarehouseContext.c   REWRITE  → OVT_TransferContext subclass (~150 L)
│   └── OVT_ShopContext.c        ~11-LINE TOUCH  → OVT_TabHostContext subclass (§3.6)
└── Menu/
    ├── ShopMenu/OVT_ShopMenuTabComponent.c   GENERALISED (int id + label key + OVT_TabHostContext)
    ├── TransferMenu/OVT_TransferRowComponent.c       NEW
    ├── TransferMenu/OVT_TransferCartLineComponent.c  NEW
    ├── PortMenu/OVT_PortItemComponent.c              DELETED
    └── WarehouseMenu/OVT_WarehouseInventoryItemComponent.c  DELETED

UI/layouts/Menu/
├── TransferMenu.layout (+ .meta)                     NEW
├── TransferMenu/TransferMenu_Row.layout (+ .meta)    NEW
├── TransferMenu/TransferMenu_CartLine.layout (+ .meta)  NEW
├── ShopMenu/ShopMenu_Tab.layout                      UNCHANGED
├── PortMenu.layout (+ .meta), PortMenu/              DELETED
└── WarehouseMenu.layout (+ .meta), WarehouseMenu/    DELETED

Configs/System/chimeraInputCommon.conf
├── + 7 Actions (Overthrow Transfer*)  + ActionContext OverthrowTransferContext
└── − 6 Actions (OverthrowPortBuy*, OverthrowWarehouseTake*)  − 2 ActionContexts

Prefabs/Characters/Factions/INDFOR/FIA/Character_Player.et
└── OVT_PortContext / OVT_WarehouseContext blocks retargeted at the shared layout + context name

Language/localization_Overthrow.st                    ~20 new #OVT-Transfer_* / consumer keys

Scripts/Game/Tests/TestSuites/
├── Logic/OVT_TEST_Logic_TransferModels.c   NEW   (registered on OVT_TEST_LogicSuite)
└── Init/OVT_TEST_Init_TransferContexts.c   NEW   (registered on OVT_TEST_InitSuite)
```

**Reserved GUID series: `6A8E2C1…`.** Verified unused — `grep -rl 6A8E2C1` returns 0 files in both `Overthrow.Arma4` and the extracted `ArmaReforger` reference tree (checked 2026-08-20). Allocation: `6A8E2C10…` layout widget/meta GUIDs, `6A8E2C11…` conf action + `InputSource` GUIDs, `6A8E2C12…` widget-component instance GUIDs. Inherited component GUIDs (e.g. `SCR_InputButtonComponent "{5D346C3DD81D95CD}"`) are **copied, never minted** — see `layouts.md` in the `overthrow-ui-patterns` skill.

### 3.2 The two pure models

Modelled directly on `Scripts/Game/Data/OVT_ShopBrowserModel.c` (same file, same tier, same doc style) minus paging.

**`OVT_TransferEntry : Managed`** — one browsable row. Deliberately dumb: what a row draws and what a cart line needs, nothing else.

| Field | Purpose |
|---|---|
| `m_sId` | Stable identity. `ResourceName` string for both first consumers; a resource key for `resources` later. The cart keys on this. |
| `m_sDisplayName` | Already-resolved name; what the sort orders by. |
| `m_eImageKind` | `PREFAB` (→ `ItemPreviewWidget`) or `TEXTURE` (→ `ImageWidget`). |
| `m_sImage` | `ResourceName` — the prefab, or the texture/imageset. |
| `m_iValue` | The one value column: unit price (PRICE) or stock held (QUANTITY). |
| `m_eValueKind` | `PRICE` or `QUANTITY`; drives formatting and the default summary. |
| `m_iMaxQuantity` | Cap the cart clamps to. Warehouse: current stock. Port: `IMPORT_MAX_QUANTITY` = 100 (`OVT_VehicleRequestComponent.c:64`). |
| `m_iCategoryId` | Consumer-defined; **never** `CATEGORY_ALL`. |
| `m_bEnabled` / `m_sDisabledReasonKey` | Dim the row and name why. |

**`OVT_TransferListModel : Managed`** — `Add` / `Clear` / `Count` / `GetEntries` / `FindById` / `SortByDisplayName` (stable, case-insensitive insertion sort — lift verbatim from `OVT_ShopBrowserModel.SortByDisplayName`, whose stability rationale applies here too) / `HasCategory` / `GetPopulatedCategories(out array<int>)` / `FilterByCategory(int, out array<ref OVT_TransferEntry>)`. `CATEGORY_ALL = -1` is a base-owned constant and is never a member of `GetPopulatedCategories`, exactly as `OVT_ShopCategory.ALL` is never returned by `GetPopulatedCategories()`. **`GetPopulatedCategories` returns ids ASCENDING** (corrected 2026-08-21; it originally shipped first-seen). Consumer category ids are declaration-ordered enums, so ascending reproduces `OVT_ShopCategoryHelper.GetDisplayOrder` exactly at the port without the model reaching for a UI class; and it is stable, whereas first-seen order changes whenever the alphabetically-first row of a category changes — which would make `TabOrderMatches` see a reordered list with unchanged membership and rebuild the tab widgets, destroying gamepad focus. **`-1` rather than `0`** because `OVT_ShopCategory.ALL` *is* `0` and the port maps its category ids straight onto that enum — a `0` sentinel would collide.

**`OVT_TransferCartModel : Managed`** — holds `OVT_TransferCartLine` (id, display name, unit value, value kind, quantity, max quantity). A line **copies** the display fields it needs rather than pointing into the list model, because the warehouse rebuilds its list model on every invoker refresh and a cart holding stale entry pointers is a class of bug this design should not have.

- `Add(OVT_TransferEntry entry, int qty)` — merges into an existing line by id, clamps the merged total to `m_iMaxQuantity`, ignores null and non-positive qty.
- `AddAll(entry)` — adds `m_iMaxQuantity` (equivalently, tops the line up to its cap).
- `Remove(string id, int qty)` / `RemoveAll(string id)` — decrement, and **drop the line entirely when it reaches zero**.
- `Clear()`, `Count()`, `GetLines()`, `GetQuantity(id)`, `FindLineIndex(id)`, `TotalQuantity()`, `TotalValue()` (Σ qty × unit value).
- `Reconcile(OVT_TransferListModel model)` — drops lines whose entry has vanished from the model and clamps lines whose `m_iMaxQuantity` fell. This is what stops "cart says take 50" surviving a refresh in which someone else emptied the warehouse to 3.

### 3.3 Layout — `UI/layouts/Menu/TransferMenu.layout`

Skeleton copied from `ShopMenu.layout` down to `ContentLayout` (`Window` anchored `0 0 1 1` with `SizeX -1820` / `SizeY -1050`, `Content` overlay with `Background` + `Blur`, `SpaceLayout` padding `0 72 0 72`, `Alignment` `SizeLayout` 1344 × 936). Widget names below are the script contract; every one is looked up by `FindAnyWidget` and must exist.

```
ContentLayout (VerticalLayout)
├── TitleRow (HorizontalLayout)      Title · Fill · PlayerMoney
├── HeaderRow (HorizontalLayout)     ← lifted from ShopMenu.layout:98-147
│    ├── Mode1Button      WLib_NavigationButton   OverthrowTransferMode1
│    ├── Mode2Button      WLib_NavigationButton   OverthrowTransferMode2
│    ├── PrevCategoryButton                       OverthrowTransferPrevCategory
│    ├── Tabs             HorizontalLayout, EMPTY (ShopMenu_Tab instances at runtime)
│    └── NextCategoryButton                       OverthrowTransferNextCategory
├── UpperStripe (Image, accent 0.761 0.392 0.08 1)
├── Columns (HorizontalLayout, SizeMode Fill)
│    ├── ListPanel (Overlay, FillWeight 0.55)
│    │    ├── ListBackground (Image, black, Opacity 0.5)
│    │    └── ListScroll (ScrollLayout)
│    │         └── ListRows (VerticalLayout)        ← EMPTY. No placeholder rows.
│    └── RightColumn (VerticalLayout, FillWeight 0.45)
│         ├── DetailsPanel (VerticalLayout)
│         │    ├── DetailsImageSlot (SizeLayout, HeightOverride 250 → Overlay)
│         │    │    ├── DetailsPreview  (ItemPreviewWidget)   PREFAB entries
│         │    │    └── DetailsImage    (ImageWidget)         TEXTURE entries
│         │    ├── DetailsName  (Text_Heading2, 28)
│         │    ├── DetailsValue (Text_Heading2, 25)
│         │    └── DetailsText  (Text_Body, Wrap 1)
│         ├── CartScroll (ScrollLayout)
│         │    └── CartLines (VerticalLayout)        ← EMPTY.
│         ├── CartEmptyLabel (Text)                  shown only when the cart is empty
│         ├── DestinationSpin (WLib_SpinBox + SCR_SpinBoxComponent)
│         └── CheckoutPanel (VerticalLayout)
│              ├── SummaryText (Text)                running total / line count
│              └── MessageText (Text)                errors + info, shop-style fade
└── Footer (HorizontalLayout)
     Qty1Button · Qty10Button · QtyAllButton · Fill · AcceptButton · SelectHint · CloseButton
```

Notes that are load-bearing:

- **Two image widgets, one visible.** `DetailsPreview`/`DetailsImage` and `RowPreview`/`RowImage` are siblings in an overlay; the base shows exactly one per `m_eImageKind`. This is the whole of the epic's "must display item prefabs *and* resources" requirement — one enum and a `SetVisible` pair, no resource-shaped API.
- **No placeholder rows.** `PortMenu.layout:138-173` and `WarehouseMenu.layout:92-127` author eight dead rows each that `Refresh()` deletes on first draw. Both containers here start empty.
- **`AcceptButton` is a `WLib_NavigationButton` with an EMPTY `m_sActionName`.** Verified: `SCR_InputButtonComponent.OnClick → OnInput → m_OnActivated.Invoke` (`SCR_InputButtonComponent.c:245-257`, `:726-743`) fires on mouse and on `MenuSelect`-while-focused, while `RegisterActionListeners` short-circuits on an empty action name (`:602`). So the button works, draws no glyph, and takes no input. Binding it to `MenuSelect` instead would be a bug: `OnInput` does **not** require focus, so `a` pressed on a *list row* would also fire Accept. `SelectHint` reproduces `PortMenu.layout:355`'s controller-only `#AR-Menu_Select` hint (`SCR_DeviceSpecificComponent m_bControllerOnly 1`) so pad players still see the `a` glyph. **Superseded 2026-08-21:** `AcceptButton` now carries `m_sActionName "OverthrowTransferAccept"` and draws its own glyph — see the D5 amendment in §5. `SelectHint` stays, advertising `MenuSelect` on the focused row.
- **`CloseButton`** copies `PortMenu.layout:341` — `SCR_InputButtonComponent` with `m_sLabel "#AR-Menu_Back"` plus `ButtonActionComponent m_bActionName "MenuBack"`, wired to `CloseLayout` via `m_OnActivated`.
- **Row layout** (`TransferMenu_Row.layout`) is `WarehouseInventoryItem.layout` restructured: `ButtonWidgetClass` + `SCR_ButtonComponent` (`m_bMouseOverToFocus 1`, `m_bShowBackgroundOnFocus 1`, `m_bShowBorderOnHover 1`) + `OVT_TransferRowComponent`, height 50, children `RowPreview`/`RowImage` (50 px), `RowName`, `Fill`, `RowValue` (right-aligned). **Cart-line layout** is the same shape with `LineName`, `Fill`, `LineQuantity`, `LineValue` and no image.
- `AlignableSlot` vertical padding is not measured — use the sibling `Slot` types the copied widgets already use, and do not add vertical padding to an `AlignableSlot` expecting it to show.

### 3.4 The hook surface — the closed list

`OVT_TransferContext` declares exactly these as overridable. A consumer that implements them writes no widget code.

| Hook | Signature | Default |
|---|---|---|
| Entries | `void BuildEntries(int mode, OVT_TransferListModel model)` | empty |
| Modes | `void BuildModes(out array<int> modes, out array<string> labelKeys)` | one unnamed mode |
| Category label | `string GetCategoryLabelKey(int categoryId)` | `""` |
| Destinations | `void BuildDestinations(out array<ref OVT_TransferDestination> dests)` | empty |
| Details | `void FillDetails(OVT_TransferEntry entry, out string name, out string value, out string body)` | name + formatted value |
| Accept | `void OnAccept(array<ref OVT_TransferCartLine> lines, OVT_TransferDestination dest)` | no-op |
| "Add all" allowed | `bool IsAddAllAllowed(int mode)` | `true` |
| Cart validity | `string ValidateCart(array<ref OVT_TransferCartLine> lines, OVT_TransferDestination dest)` | `""` = acceptable |

`ValidateCart` is the eighth hook and it is not a smuggled extra: the requirements' checkout area must show *"not enough room in the destination", "cannot afford", consumer-provided reason text*, and something has to produce that string. It returns an `#OVT-` key or `""`; a non-empty return greys Accept and prints into `MessageText`. The port implements affordability; both consumers implement "you are not in a vehicle" — which is exactly the two silent returns named in Goal S2.

`OVT_TransferDestination : Managed` = `{ string m_sId; string m_sLabel; IEntity m_Entity; }`. Both first consumers produce zero or one of these (the occupied vehicle), so the picker is hidden and `dest` is either the vehicle or null.

The running summary is **not** a hook. The base computes it from the cart's `m_eValueKind`: PRICE → total cost + line count; QUANTITY → total items + line count. A consumer that needs more overrides `GetSummaryText()`, which exists as a virtual with that default. (YAGNI: neither first consumer overrides it.)

### 3.5 Focus model

Two remembered indices and one pane enum:

```
EOVT_TransferPane { LIST, CART }
m_ePane, m_iListIndex, m_iCartIndex
```

- **Rows and cart lines are focusable buttons.** Engine directional focus already walks a `VerticalLayout` on `MenuUp`/`MenuDown`, so the base does not implement up/down at all; it only *tracks* where focus went, by subscribing each row/line's `SCR_ButtonBaseComponent.m_OnFocus` (`ScriptInvoker<Widget>`, `SCR_ButtonBaseComponent.c:56`) and recording the pane + index. That is also what makes a mouse click on a cart line switch the buttons to Remove, per the requirement.
- **`MenuLeft` / `MenuRight` swap panes.** The base adds its own listeners for both in `OnShow` and removes them in `OnClose`, and the handler bails unless `m_bIsActive`. **Both scopings are required:** `MenuLeft` is listed by a dozen contexts, so a listener registered for the character's lifetime (the way `OVT_PortContext.RegisterInputs:243` registers `MenuBack`) would fire while some *other* menu is open.
- **⚠️ The picker eats left/right when focused, and the base must not double-handle it.** `SCR_SpinBoxComponent` installs its **own** `MenuLeft`/`MenuRight` listeners on focus (`AddActionListeners`, `SCR_SpinBoxComponent.c:331-341`, hooked from `OnFocus`/`OnFocusLost` at `:95`/`:106`) and each gates on `GetFocusedWidget() == m_wRoot` (`:308-326`). The base's handler has no such gate, so without an explicit check one d-pad press would change the destination *and* jump the focus column. **The base's `MenuLeft`/`MenuRight` handler must return early when the focused widget is `DestinationSpin` or a descendant of it.** This is decision D6 and it is the single most likely gamepad bug in the feature.
- **Focus is never lost on refresh.** Refresh rebuilds rows and cart lines, so after a rebuild the base restores focus to the remembered index in the remembered pane, clamping to the new count (and falling back to the other pane, then to Accept, when a pane is empty). The list's initial focus follows `OVT_PortContext.c:96-97`'s `SetFocusedWidget(firstCard)` — without it a pad opens the menu with nothing focused and the list is mouse-only.
- **Down past the cart → picker → Accept** is left to the engine's directional search over the right column's natural widget order. Whether it lands cleanly is a play-test question, not a compile one; if it does not, the fallback is an explicit `SetFocusedWidget(DestinationSpin)` when `MenuDown` fires on the last cart line. Recorded as a risk (R3), not designed around in advance.
- The base does **not** re-register `MenuBack`. `OVT_UIContext.RegisterInputs:98-122` already binds `m_sCloseAction` from the prefab; the port and warehouse contexts each add a second, redundant `MenuBack → CloseLayout` listener today, and the rewrite drops it.

### 3.6 The tab host — `OVT_TabHostContext`

EnforceScript has no interfaces and no multiple inheritance, so the "interface both hosts implement" is realised as **an intermediate abstract class on the one ancestor they already share**:

```
OVT_UIContext                (unchanged)
└── OVT_TabHostContext       NEW: two virtuals, nothing else
    ├── OVT_ShopContext      parent changed; +2 overrides
    └── OVT_TransferContext  NEW base
```

```cpp
//! A context that can host a row of OVT_ShopMenuTabComponent tabs.
class OVT_TabHostContext : OVT_UIContext
{
	//! A tab was picked. \param[in] tabId The host's own id for that tab.
	void SelectTabId(int tabId) {}

	//! \return True when tabId is the host's active tab.
	bool IsTabIdActive(int tabId) { return false; }
}
```

`OVT_ShopMenuTabComponent` is retyped from `(OVT_ShopCategory, OVT_ShopContext)` to `(int tabId, string labelKey, OVT_TabHostContext host)`: `Init(int tabId, string labelKey, OVT_TabHostContext host, bool selected)`, `GetTabId()`, and `Activate()` calls `m_Host.SelectTabId(m_iTabId)`. **Its behaviour does not change** — the `m_OnClicked` subscription (`:96-106`), the one-call-queue-tick deferred `Activate` (`:116-120`) and the guarded `OnClick` fallback are all preserved verbatim, because the deferral is what keeps a tab from destroying itself inside its own click handler.

**The whole `OVT_ShopContext` diff, ~11 lines:**

1. `class OVT_ShopContext : OVT_TabHostContext` (1 line).
2. `CreateTab` (`:1001`): pass `OVT_ShopCategoryHelper.GetLabelKey(category)` as the new second argument (1 line).
3. `UpdateTabSelection` (`:1015`): `tab.GetCategory()` → `tab.GetTabId()` (1 line).
4. Two overrides forwarding to the existing `SelectTab(OVT_ShopCategory)` / `m_eTab` (~8 lines with doc comments).

> Implementation note: if `SelectTab(tabId)` is rejected because the parameter is typed `OVT_ShopCategory`, assign through a local (`OVT_ShopCategory cat = tabId; SelectTab(cat);`) — EnforceScript accepts the assignment conversion where it may reject the argument conversion. Do not change `SelectTab`'s signature.

**The shop's tab machinery is deliberately NOT lifted into `OVT_TabHostContext`.** `RefreshTabs` / `TabOrderMatches` / `RebuildTabs` / `CreateTab` / `ClearTabs` / `UpdateTabSelection` / `CycleTab` / `BuildTabOrder` are ~120 lines of *shipped, working* shop code (`OVT_ShopContext.c:515-537`, `:913-1027`); moving them would blow the user's ~10-line budget and put the shop's gamepad behaviour at risk for a DRY win. `OVT_TransferContext` writes its own id-based equivalent (~90 lines), preserving both invariants that matter: **tabs are rebuilt only when the tab *set* changes** (`TabOrderMatches`, `:942` — this is what keeps gamepad focus alive across a refresh), and **fewer than two populated categories means no tab row at all**. The duplication is logged as epic tech debt in `context.md`; a third host justifies the lift, two do not.

### 3.7 Header rules (from `RefreshHeader`, `OVT_ShopContext.c:887-905`)

| Condition | Result |
|---|---|
| consumer defines < 2 modes | `Mode1Button` / `Mode2Button` hidden → their keybinds die with them |
| < 2 populated categories | tab row hidden, steppers hidden → `Q`/`E`/`L3`/`R3` die with them |
| ≥ 2 populated categories | `CATEGORY_ALL` first, then the populated ids **ascending** (§3.2) |
| both of the above | whole `HeaderRow` hidden |
| active mode | accent `0xFFC26414` label + opacity 1.0; inactive 0.6, **never disabled** |

Both first consumers define exactly one mode and populate categories only at the port, so the warehouse ships with no header and the port ships with tabs and no mode buttons — which is the same "the header appears when it earns its place" rule the shop already follows.

### 3.8 Bindings

One shared `ActionContext` in `Configs/System/chimeraInputCommon.conf`:

```
ActionContext OverthrowTransferContext {
 Priority 50
 Flags 4
 ActionRefs {
  "OverthrowTransferMode1" "OverthrowTransferMode2"
  "OverthrowTransferPrevCategory" "OverthrowTransferNextCategory"
  "OverthrowTransferQtyOne" "OverthrowTransferQtyTen" "OverthrowTransferQtyAll"
  "OverthrowTransferAccept"
  "MenuUp" "MenuDown" "MenuLeft" "MenuRight" "MenuSelect" "MenuBack"
 }
}
```

| Verb | Widget | Action | Keyboard | Gamepad | Precedent |
|---|---|---|---|---|---|
| Mode 1 | `Mode1Button` | `OverthrowTransferMode1` | `KC_1` | `shoulder_right` | shop `ModeBuy` (`:144`) |
| Mode 2 | `Mode2Button` | `OverthrowTransferMode2` | `KC_2` | `view` | shop `ModeSell` (`:161`) |
| Prev category | `PrevCategoryButton` | `OverthrowTransferPrevCategory` | `KC_Q` | `thumb_left` | shop `PrevCategory` (`:178`) |
| Next category | `NextCategoryButton` | `OverthrowTransferNextCategory` | `KC_E` | `thumb_right` | shop `NextCategory` (`:195`) |
| Add/Remove 1 | `Qty1Button` | `OverthrowTransferQtyOne` | `KC_COMMA` | `x` | warehouse `TakeOne` (`:3`) |
| Add/Remove 10 | `Qty10Button` | `OverthrowTransferQtyTen` | `KC_PERIOD` | `y` | warehouse `TakeTen` (`:20`) |
| Add/Remove all | `QtyAllButton` | `OverthrowTransferQtyAll` | `KC_SEMICOLON` | `right_trigger` | warehouse `TakeAll` (`:404`) |
| Accept | `AcceptButton` | `OverthrowTransferAccept` *(2026-08-21; was none — see the D5 amendment)* | `KC_F` | `left_trigger` | shop `Sell` / jobs `Accept` / loadouts `Apply` (all `KC_F`) |
| Close | `CloseButton` | `MenuBack` | `ESC` | `b` | every screen |
| Browse / swap pane | — | `MenuUp/Down/Left/Right` | arrows + `WASD` | d-pad + left stick | — |

Rules this table already satisfies: `Q`/`E` are safe because the context does **not** list `MenuTabLeft`/`MenuTabRight`; `left_trigger` and `right_trigger` are free because the context does **not** list `MenuNavLeft`/`MenuNavRight` (and the requirement forbids listing them); nothing is on `shoulder_left` (VON @ 110, BUG-092); no verb is on `a`, `b` or the d-pad. **Each of the seven actions gets its own `InputSourceSum` + two `InputSourceValue` + two `InputFilterClick` GUIDs from the `6A8E2C11…` series** — no sharing, unlike the port/warehouse pairs being deleted.

**The relabel and the hidden "Add all" ride on visibility, not on rebinding.** Three widgets, three actions, two label sets:

| Focused pane | `Qty1Button` | `Qty10Button` | `QtyAllButton` |
|---|---|---|---|
| LIST, `IsAddAllAllowed(mode)` true | Add 1 | Add 10 | Add all |
| LIST, `IsAddAllAllowed(mode)` false (port Import) | Add 1 | Add 10 | **hidden** |
| CART | Remove 1 | Remove 10 | Remove all (**always visible**) |
| list and cart both empty | hidden | hidden | hidden |

"Add all" is hidden by `SetVisible(false)`, which also retires `right_trigger` (`SCR_InputButtonComponent.OnInput` bails on a non-`IsVisibleInHierarchy` widget — the same mechanism the shop's `x`-sharing waiver relies on, `OVT_ShopContext.c:1109-1118`). **"Remove all" stays visible even in Import mode**: removing a whole cart line is meaningful in every mode, and `IsAddAllAllowed` is a statement about *adding*. Labels are swapped with `SCR_InputButtonComponent.SetLabel()`, exactly as the shop relabels its action buttons.

### 3.9 Data flow

```
open  ─► OnShow ─► BuildModes ─► BuildEntries(mode) ─► SortByDisplayName
                              ─► RefreshHeader (modes / tabs / steppers)
                              ─► RefreshList ─► rows, focus first
                              ─► BuildDestinations ─► picker (hidden when ≤ 1)
select row ─► FillDetails ─► details panel
Qty button ─► cart.Add/Remove ─► RefreshCart ─► GetSummaryText + ValidateCart ─► summary / message
Accept     ─► ValidateCart; if "" ─► OnAccept(lines, dest) ─► cart.Clear ─► message ─► ScheduleRefresh
warehouse invoker (m_OnWarehouseInventoryChanged, coalesced 50 ms)
           ─► BuildEntries ─► cart.Reconcile(model) ─► RefreshList + RefreshCart ─► restore focus
```

Accept **leaves the menu open**. The warehouse's `m_OnWarehouseInventoryChanged` invoker remains the source of truth for its redraw (no optimistic refresh — that rule and its rationale are already in `OVT_WarehouseContext.c:315`); the port has no such invoker, so Accept schedules one coalesced refresh (~400 ms, the shop's `TRANSACTION_RECHECK_MS` precedent) to repaint money and prices.

---

## 4. Implementation Phases

Every phase: `tools/compile-check.sh` exit 0 before hand-back. `tools/run-tests.sh` is the orchestrator's, run once after a phase completes — never inside an agent, never during planning (`.claude/test-policy.md`). Phases 2–5 touch only layouts, `.conf`, `.st` and UI scripts, which the suites do not cover; the gate is skippable there and should be *announced* as skipped, not silently dropped. Phase 1 is the one phase the Logic suite genuinely covers.

### Phase 1 — Pure models + Logic-tier cases
**Estimate:** 3–4 h · **Agent:** `component-developer`

**Tasks**
1. `OVT_TransferEntry.c` — entry class + `EOVT_TransferImageKind` + `EOVT_TransferValueKind`; constructor sets every field explicitly (`new` applies no `[Attribute]` defvalues).
2. `OVT_TransferListModel.c` — `Add`/`Clear`/`Count`/`GetEntries`/`FindById`/`SortByDisplayName`/`HasCategory`/`GetPopulatedCategories`/`FilterByCategory`, `CATEGORY_ALL = -1`.
3. `OVT_TransferCartModel.c` — `OVT_TransferCartLine` + `Add`/`AddAll`/`Remove`/`RemoveAll`/`Clear`/`Count`/`GetLines`/`GetQuantity`/`FindLineIndex`/`TotalQuantity`/`TotalValue`/`Reconcile`.
4. `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_TransferModels.c`, registered on `OVT_TEST_LogicSuite`.

**Acceptance**
- No case references a manager, the game mode or the world — and **the identifiers for Overthrow's static manager accessor and the engine's game-mode getter appear nowhere under `TestSuites/Logic/`, not even in a comment** (the tier grep does not distinguish code from prose; `OVT_TEST_LogicSuite.c` says so and Phase 4 of the test-coverage feature tripped exactly this).
- Every case is proven able to fail once, with the mutation and the resulting message recorded in `context.md`.
- Cases named so they sort independently; floats compared with an epsilon; no `maxAttempts` anywhere.
- `tools/compile-check.sh` exit 0.

### Phase 2 — Layout, base context, shared `ActionContext`, focus & gamepad ⚠️ ADVANCED AGENT
**Estimate:** 8–12 h · **Agent:** `ui-developer-advanced`

**Tasks**
1. `TransferMenu.layout` + `.meta`, `TransferMenu_Row.layout` + `.meta`, `TransferMenu_CartLine.layout` + `.meta` — widget tree of §3.3, GUIDs from `6A8E2C10…`, inherited component GUIDs copied.
2. `OVT_TransferRowComponent.c`, `OVT_TransferCartLineComponent.c` — `Init(entry/line, index, context)`, `SetSelected`, `m_OnClicked` subscription + guarded `OnClick` fallback + `m_OnFocus` reporting. Same shape as `OVT_ShopMenuTabComponent`, including the `m_bWiredToButton` guard that stops a mouse click running twice.
3. `OVT_TransferContext.c` — attributes (`m_RowLayout`, `m_CartLineLayout`, `m_TabLayout`), state, `OnShow`/`OnClose`, header + tabs (§3.6), `RefreshList`/`RefreshCart`/`RefreshDetails`/`RefreshDestinations`/`RefreshActionButtons`, focus model (§3.5), the eight hooks with defaults, `ShowMessage`/`HideMessage` with the shop's fade.
4. 7 actions + `ActionContext OverthrowTransferContext` in `chimeraInputCommon.conf`, GUIDs from `6A8E2C11…`.
5. `#OVT-Transfer_*` entries in `Language/localization_Overthrow.st`.

**Acceptance**
- `OnClose` removes **exactly** what `OnShow` inserted: every `m_OnActivated`, every row/line `m_OnClicked` + `m_OnFocus`, both `Menu*` action listeners, every pending `CallLater`, and every cached widget/component reference nulled.
- The `MenuLeft`/`MenuRight` handler returns early when `DestinationSpin` (or a descendant) has focus (§3.5).
- Layout has no duplicate widget GUIDs (run the dedupe snippet in `layouts.md`); both new `.meta` files carry all five platform configurations.
- `python3 .claude/skills/overthrow-ui-patterns/scripts/check-input-conflicts.py` exit 0, and its summary line shows **0 errors / 0 warnings / 3 combo notes / 0 pre-existing / 1 acknowledged** — the exact 2026-08-20 baseline.
- `tools/compile-check.sh` exit 0.
- **Do not write `Configs/Language/*.conf`** — those are Workbench-generated exports.

### Phase 3 — Tab-host class + shop generalisation
**Estimate:** 2–3 h · **Agent:** `component-developer`

**Tasks**
1. `Scripts/Game/UI/OVT_TabHostContext.c` (§3.6).
2. `OVT_ShopMenuTabComponent` retyped to `(int, string, OVT_TabHostContext)`; deferred-activate, `m_OnClicked` wiring and `OnClick` fallback preserved verbatim.
3. `OVT_ShopContext` — the ~11-line diff of §3.6.
4. `OVT_TransferContext` reparented to `OVT_TabHostContext` and its tab row wired to the shared tab component + `ShopMenu_Tab.layout`.

**Acceptance**
- `git diff --stat Scripts/Game/UI/Context/OVT_ShopContext.c` shows ≤ 15 changed lines and no behavioural edit.
- `ShopMenu_Tab.layout` is byte-identical (`git diff --exit-code` on it).
- Shop tabs still rebuild only when the tab *set* changes.
- Compile-check exit 0. Shop play-test is Phase 6's checklist item (an unnoticed shop regression is the main risk of this phase).

### Phase 4 — Port consumer, and delete the old port screen
**Estimate:** 3–4 h · **Agent:** `ui-developer`

**Tasks**
1. Rewrite `OVT_PortContext` as `OVT_TransferContext`: one mode (`#OVT-Import`); `BuildEntries` keeps `CollectImportables` (`:104-163`) **verbatim**, including the illegal gate (`HasPermission("IllegalImports")` ∥ `m_Economy.ResistanceControlsNearestPort(...)`, `:112-114`) and the BUG-102 "no no-op rows" membership rules; entries get `m_eValueKind = PRICE`, `m_iValue = m_Economy.GetPrice(id)`, `m_iMaxQuantity = 100`, `m_eImageKind = PREFAB`, category from the shop mapping (`OVT_ShopCategoryHelper` via the economy's id→category cache).
2. `IsAddAllAllowed` → `false`. `BuildDestinations` → the occupied vehicle from `SCR_CompartmentAccessComponent.GetVehicle()`, or nothing.
3. `ValidateCart` → `"#OVT-Transfer_NoVehicle"` when there is no destination; `"#OVT-CannotAfford"` when `TotalValue()` exceeds `m_Economy.GetPlayerMoney(m_sPlayerID)`.
4. `OnAccept` → one `OVT_ControllerComponent<OVT_VehicleRequestComponent>.Get().ImportToVehicle(id, qty, entity)` per line. **No server change.**
5. Retarget the prefab block (`Character_Player.et:119-125`) at `TransferMenu.layout` + `m_sContextName "OverthrowTransferContext"` + the two row-layout attributes; keep the instance GUID.
6. **Delete** `PortMenu.layout` (+ `.meta`), `PortMenu/PortInventoryItem.layout` (+ `.meta`), `OVT_PortItemComponent.c`, actions `OverthrowPortBuyTen` / `OverthrowPortBuyHundred`, `ActionContext OverthrowPortContext`.

**Acceptance**
- `grep -rn "PortMenu\|OVT_PortItemComponent\|OverthrowPortBuy\|OverthrowPortContext" --include=*.c --include=*.et --include=*.conf --include=*.layout .` returns nothing.
- `OVT_VehicleMenuContext.Import()` (`:169-188`) is untouched and still opens `OVT_PortContext`.
- No file under `Scripts/Game/GameMode/` or `Scripts/Game/Components/Player/Request*` is modified.
- Compile-check exit 0; conflict checker exit 0.

### Phase 5 — Warehouse consumer, and delete the old warehouse screen
**Estimate:** 3–4 h · **Agent:** `ui-developer`

**Tasks**
1. Rewrite `OVT_WarehouseContext` as `OVT_TransferContext`: one mode (`#OVT-TakeFromWarehouse`); `BuildEntries` from `m_Warehouse.inventory` with `m_eValueKind = QUANTITY`, `m_iValue = m_iMaxQuantity = stock`, `m_eImageKind = PREFAB`, one category. `SetWarehouse()` keeps its signature — `OVT_VehicleMenuContext:159-167` is unchanged.
2. `IsAddAllAllowed` → `true`. Destinations + `ValidateCart` as in Phase 4, minus affordability.
3. `OnAccept` → one `TakeFromWarehouseToVehicle(m_Warehouse.id, id, qty, entity)` per line.
4. Keep the invoker path: subscribe `m_OnWarehouseInventoryChanged` in `OnShow`, unsubscribe from the **cached** manager instance in `OnClose`, coalesce at 50 ms, and drive `Reconcile` + redraw from it. **No optimistic refresh.**
5. Retarget the prefab block (`:100-106`); **delete** `WarehouseMenu.layout` (+ `.meta`), `WarehouseMenu/WarehouseInventoryItem.layout` (+ `.meta`), `OVT_WarehouseInventoryItemComponent.c`, the four `OverthrowWarehouseTake*` actions and `ActionContext OverthrowWarehouseContext`.
6. `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_TransferContexts.c` — both contexts resolve via `OVT_UIManagerComponent.GetContext`, both are `OVT_TransferContext` subclasses, both carry the **same** `m_Layout` and the **same** `m_sContextName` (this is the "one shared layout, one shared context" requirement, mechanically pinned), and `GetGame().GetInputManager().ActivateContext("OverthrowTransferContext")` returns true. Verify that last return's semantics when writing the case — if activation returns true for an unknown context name, drop the claim rather than assert something vacuous, and say so in the case comment.

**Acceptance**
- `grep -rn "WarehouseMenu\|OVT_WarehouseInventoryItemComponent\|OverthrowWarehouseTake\|OverthrowWarehouseContext" …` returns nothing.
- One handler per button, none stacked (BUG-081's rule survives the rewrite by construction — the base wires each button once).
- The four deleted warehouse actions and the two deleted port actions take **both** shared-GUID pairs with them.
- Compile-check exit 0; conflict checker exit 0.

### Phase 6 — Conflict check, localization, help & wiki sync
**Estimate:** 2–3 h · **Agents:** main thread + `help-docs-sync`

**Tasks**
1. Final `check-input-conflicts.py` (plain and `--warnings`) against the 2026-08-20 baseline.
2. `.st` audit: every runtime `SetLabel`/`SetText` key exists in `Language/localization_Overthrow.st`, each with a filled-in `Comment`; braces balanced; multi-line values use the trailing backslash. **Ask the user to re-export** — until they do, the new keys render raw on screen, which is expected and is the path the shop shipped on.
3. Leave the six now-dead keys (`OVT-Port_Buy_10`, `OVT-Port_Buy_100`, `OVT-Warehouse_Take_1/10/100/All`) in place; note them in `context.md`. Deleting them is a structural `.st` edit with a data-loss failure mode and no user-visible benefit.
4. `help-docs-sync`: this feature **does** change player-facing behaviour (two screens, new controls), so the phase runs. Grepped 2026-08-20: `Configs/Tutorials/` (16 configs) and `Configs/FieldManual/Categories/FM_Overthrow.conf` (16 entries) contain **no** "Buy 10" / "Take 100" / port / warehouse text, so the in-game half is likely a no-op — confirm, then focus on the **wiki**, which does document the port and warehouse screens. Wiki writes go through the `wikijs` MCP tools and can fail while reporting success; verify each page renders after writing.

**Acceptance**
- Conflict checker exit 0 with the baseline summary line unchanged.
- No `Configs/Language/*.conf` file modified.
- Tutorial / Field Manual grep for the old button text returns nothing (or the hits are fixed).
- Wiki port/warehouse pages describe the cart flow and the current bindings.

---

## 5. Key Technical Decisions

**D1 — Two pure models, not one "browser".** Splitting list from cart is what makes the cart independently testable and independently reusable: `storage`'s Open Storage screen has the same cart over a different list, and `resources` has the same cart over an m³-capped one. Both models are `Managed` with `new`-built state and no manager lookups, so they sit in the cheapest test tier (`OVT_TEST_LogicSuite`, no world, ~8 s). *Rejected:* one model holding both, which would have made every cart assertion drag a row set along with it.

**D2 — Cart lines copy display fields; they do not point at entries.** The warehouse rebuilds its list model on every invoker refresh. A line holding an `OVT_TransferEntry` ref would keep a strong reference to a row that no longer exists and silently draw stale stock. `Reconcile(model)` makes the staleness handling explicit, pure and testable.

**D3 — The tab "interface" is an intermediate class, not a delegate object.** EnforceScript has no interfaces and no multiple inheritance, but `OVT_ShopContext` and `OVT_TransferContext` already share `OVT_UIContext`, so an intermediate class *is* the interface, and it costs the shop one word in its class declaration. *Rejected:* (a) putting the two virtuals on `OVT_UIContext` — pollutes the base of all seventeen contexts with a concept only two of them have; (b) a `ScriptInvoker` or adapter-object callback — works, but leaves the tab component unable to ask "am I the active tab?" without a second channel.

**D4 — The shop's tab machinery stays in the shop.** ~90 lines of controlled duplication in `OVT_TransferContext` beats a 120-line lift out of shipped, gamepad-tuned code that the user capped at ~10 lines of touch. Logged as tech debt; a third host is the trigger to lift it. The two invariants — rebuild-only-on-set-change, and no tab row below two populated categories — are re-implemented deliberately, not by accident.

**D5 — Accept is focus-activated with an empty `m_sActionName`.** Requirement 43 puts Accept on `MenuSelect`/`a`. Binding the button's `m_sActionName` to `"MenuSelect"` would be a trap: `SCR_InputButtonComponent.OnInput()` does not check focus, so `a` pressed to select a *list row* would also fire Accept. An empty action name gives mouse + focus-`MenuSelect` activation via `m_OnActivated` with no listener registered (`SCR_InputButtonComponent.c:245-257`, `:602`, `:726-743`), and the controller-only `#AR-Menu_Select` hint from `PortMenu.layout:355` keeps the glyph on screen.

> **Amendment, 2026-08-21 (post-play-test).** D5's *reasoning* stands — Accept must never carry `"MenuSelect"` — but its *conclusion* (an empty `m_sActionName`) is superseded. Accept now has its own action, `OverthrowTransferAccept` (`keyboard:KC_F` / `gamepad0:left_trigger`, `InputSourceSum {6A8E2C1100000080}`), listed in `ActionContext OverthrowTransferContext`. A dedicated action cannot collide with `MenuSelect`, so the trap does not apply, and the cost D5 accepted — no pad binding and no glyph — is paid off: `IsKeybindAvailable` now returns true and the button draws its own chrome. `SCR_InputButtonComponent` registers the listener itself (`:602-613`), so the context adds no second path; `m_bCanBeDisabled` defaults to 1, so `RefreshCheckout`'s `SetEnabled(false)` still gates the press (`:731`). `KC_F` matches `OverthrowShopSell` / `OverthrowJobsAccept` / `OverthrowLoadoutsApply`. `SelectHint` is kept — it advertises a different action (`MenuSelect` activates the focused row), not Accept's.

**D6 — The base yields `MenuLeft`/`MenuRight` to the focused picker.** `SCR_SpinBoxComponent` installs its own listeners on focus and self-gates on focus; the base's do not, so without an explicit "is the picker focused?" early return, one d-pad press would both change the destination and jump the focus column. Named here because it is invisible in the conf and in the layout, and only a pad play-test would find it.

**D7 — Three quantity buttons, two label sets, no rebinding.** Relabelling with `SetLabel()` and retiring "Add all" with `SetVisible(false)` (which kills the keybind too) keeps the action set at three and the conf honest. "Remove all" stays visible in every mode — `IsAddAllAllowed` is a statement about adding, and removing a cart line is always meaningful.

**D8 — `CATEGORY_ALL = -1`.** `OVT_ShopCategory.ALL` is `0` and the port maps its category ids straight onto that enum, so a `0` sentinel would make "All" and "no filter" collide with a real category id.

**D9 — Accept leaves the menu open and never refreshes optimistically.** The warehouse's `m_OnWarehouseInventoryChanged` stays the source of truth (that is a shipped rule with a recorded rationale); the port, having no invoker, gets one coalesced `CallLater` refresh. Accept clears the cart and prints a summary rather than closing, because the common case at a port is several trips through the list.

**D10 — Per-line requests, unchanged server.** Requirement decision 3. A ten-line cart sends ten `ImportToVehicle` asks, exactly as ten button presses do today, so this feature cannot regress server validation, rate limits or the illegal-import gate. `storage` supersedes it with a batched checkout; `OnAccept` is the single seam that changes when it does.

**D11 — Localization keys in the layout from day one.** The requirement permits literal text until re-export; keys avoid a second edit that would otherwise be untracked. The cost is that new labels render as raw `#OVT-…` text in a play-test until the user re-exports — which is expected, is what raw keys on screen mean, and is the path the shop shipped on. If the user wants a readable play-test first, literals are the fallback and the follow-up edit gets tracked in `tasks.md`.

---

## 6. Definition of Done

### Functional

- **F1** Opening a port from the vehicle menu shows the transfer screen with an alphabetical, categorized import list; opening a warehouse shows an alphabetical Take list.
- **F2** Selecting a row fills the details panel (image, name, value, body) for both `PREFAB` and — asserted in a scratch case, not shipped — `TEXTURE` entries.
- **F3** Add 1 / Add 10 build cart lines; adding the same entry twice merges; a line never exceeds its `m_iMaxQuantity` (port 100, warehouse current stock).
- **F4** Focusing a cart line relabels the three buttons to Remove 1 / 10 / all; removing to zero drops the line; the buttons relabel back when focus returns to the list.
- **F5** "Add all" is absent at the port and present at the warehouse; "Remove all" is present at both.
- **F6** The destination picker is hidden (both consumers offer ≤ 1 destination) and Accept still targets the occupied vehicle.
- **F7** Accept issues one existing request per line, clears the cart, prints a summary, leaves the menu open; warehouse stock redraws when the server's value arrives.
- **F8** With no vehicle occupied, Accept refuses and the checkout area says why — replacing today's two silent returns.
- **F9** At the port, a cart the player cannot afford refuses with a reason before any request is sent.
- **F10** The header hides its mode buttons (both consumers have one mode) and shows tabs only at the port, only when ≥ 2 categories are populated.
- **F11** The old port and warehouse screens, row layouts, row handlers and conf blocks no longer exist anywhere in the tree.
- **F12** The shop and gun-dealer screens look and behave exactly as before.

### Quality

- **Q1** `tools/compile-check.sh` exit 0 at every phase boundary.
- **Q2** `check-input-conflicts.py` exit 0, summary line matching the 2026-08-20 baseline (0/0/3/0/1).
- **Q3** Logic cases for both models, each proven able to fail once, mutations recorded in `context.md`.
- **Q4** No `Configs/Language/*.conf` modified; `.st` braces balanced.
- **Q5** No file outside `Scripts/Game/UI/`, `Scripts/Game/Data/`, `Scripts/Game/Tests/`, `UI/layouts/`, `Configs/System/chimeraInputCommon.conf`, `Language/`, and the two prefab blocks is modified. In particular: no request component, no manager, no `core/damage` file (concurrent session).
- **Q6** `OnClose` removes exactly what `OnShow` inserted, verified by reading the two methods against each other.
- **Q7** Comments are sparse per `CLAUDE.md` — a line or two for a non-obvious constraint or a trap, never a rationale essay. Reasoning belongs in this document.

### Integration

- **I1** Both consumers are registered on `Character_Player.et` with the same `m_Layout` and the same `m_sContextName`, asserted by the Init case.
- **I2** `OVT_VehicleMenuContext.Import()` and the warehouse accessibility check are unmodified and still open the same two context types.
- **I3** `OVT_ShopContext`'s diff is ≤ 15 lines and `ShopMenu_Tab.layout` is byte-identical.
- **I4** Nothing in the base assumes a resource ledger, an m³ cap, an Export mode or a second destination.

### Verification method — an independent evaluator can follow this

**Static (no game):**
1. `cd "/mnt/n/Projects/Arma 4/Overthrow.Arma4" && tools/compile-check.sh` → exit 0.
2. `python3 .claude/skills/overthrow-ui-patterns/scripts/check-input-conflicts.py` → exit 0, and the summary reads `0 error(s), 0 warning(s), 3 combo note(s), 0 pre-existing, 1 acknowledged.`
3. `python3 .claude/skills/overthrow-ui-patterns/scripts/check-input-conflicts.py --warnings` → no new always-active overlap.
4. `grep -rn "PortMenu\|WarehouseMenu\|OVT_PortItemComponent\|OVT_WarehouseInventoryItemComponent\|OverthrowPortBuy\|OverthrowWarehouseTake\|OverthrowPortContext\|OverthrowWarehouseContext" --include=*.c --include=*.et --include=*.conf --include=*.layout .` → no hits.
5. `git diff --stat main -- Scripts/Game/UI/Context/OVT_ShopContext.c` → ≤ 15 lines; `git diff --exit-code main -- UI/layouts/Menu/ShopMenu/ShopMenu_Tab.layout` → clean.
6. `git status --porcelain Configs/Language/` → empty.
7. Duplicate-widget-GUID snippet from `layouts.md` on all three new layouts → "none".
8. Orchestrator only: `tools/run-tests.sh "{6A6E29FF47ECB840}"` (Fast) once after Phase 5 — the new Logic + Init cases are in it. Announce the focus steal first.

**Workbench (user-gated):** open `Character_Player.et` and confirm both context entries still list their attributes (`m_Layout`, `m_sContextName`, row layouts) after the Phase 3 reparent; open all three new layouts.

**Play-test, mouse (drive a truck to a port, own a warehouse):**
9. Port: open via vehicle menu → list populated, first row focused, tabs present. Click a category tab; the list filters and the tab set does not flicker. Add 1, Add 10, Add 10 again on the same row → one line, quantity 21. Click the cart line → buttons read Remove; Remove 10 → 11; Remove all → line gone. Confirm no "Add all" button. Build a cart you cannot afford → checkout says so and Accept refuses. Reduce it, Accept → items appear in the vehicle, cart clears, money drops, summary prints.
10. Port, on foot within 20 m: Accept refuses with the no-vehicle message (today it silently does nothing).
11. Warehouse: same walk-through with the stock column, "Add all" present, and a Take that empties a row — the row disappears from the list, the cart line disappears with it, and focus does not vanish.
12. Shop: open a shop and a gun dealer. Tabs, Buy/Sell, paging and the sell flow are unchanged.

**Play-test, gamepad only (no mouse touched):**
13. Open the port screen: something is focused on arrival. d-pad up/down walks the list; `R3`/`L3` step categories with wraparound; `RB`/`view` do nothing visible (one mode → buttons hidden and their inputs dead).
14. `x` / `y` add 1 / 10; `RT` does nothing at the port (Add all hidden) and adds all at the warehouse.
15. d-pad **right** moves focus into the cart; the three labels flip to Remove; `x`/`y`/`RT` remove. d-pad **left** returns to the list and the labels flip back.
16. d-pad **down** past the last cart line reaches the destination picker; d-pad left/right there changes the destination **and does not move the focus column** (D6). Down again reaches Accept; `a` accepts.
17. `b` closes. `LB` still opens VON and never touches the menu.

### Bug-report candidates for the orchestrator — do not file from this plan

- `OVT_UIContext.Init:54` contains `OVT_Global.GetConfig() = OVT_Global.GetConfig();`, a no-op that leaves `m_Config` null for every context. Not this feature's to fix and not depended on here.
- `OVT_WarehouseContext:57` wires `CloseButton` through `m_OnClicked` while every other button uses `m_OnActivated`. Dies with the rewrite.

---

## 7. Testing Strategy

**Logic tier — `OVT_TEST_Logic_TransferModels.c`** (world-free, `new`-built subjects, ~1 s):

| Case | Claim | Proof it can fail |
|---|---|---|
| `ListSortAlphabetical` | case-insensitive, **stable** for equal names | reverse the comparison |
| `ListCategoryPopulation` | `GetPopulatedCategories` omits empty categories, never returns `CATEGORY_ALL`, and returns ids **ascending** | drop the insertion sort (rows are added in descending category order, so first-seen fails) |
| `ListFilterByCategory` | `CATEGORY_ALL` returns everything; a real id returns only its own; order preserved | drop the `CATEGORY_ALL` branch |
| `CartAddMerges` | adding an id twice yields one line with the summed quantity | insert instead of merge |
| `CartClampsToMax` | a merged total above `m_iMaxQuantity` is clamped, not rejected | remove the clamp |
| `CartAddAll` | `AddAll` tops the line up to exactly `m_iMaxQuantity` | use `+= max` |
| `CartRemoveDropsLine` | removing to zero (and below) deletes the line; `Count()` falls | clamp the quantity at 0 and keep the line |
| `CartTotals` | `TotalQuantity` / `TotalValue` over mixed lines; empty cart is 0/0 | drop the unit-value multiply |
| `CartReconcile` | vanished entry → line dropped; lowered max → line clamped; unchanged → untouched | make `Reconcile` a no-op |

Each case sets **every** field it depends on explicitly (`new` applies no `[Attribute]` defvalues), and each fail-proof mutation + resulting message goes in `context.md`.

**Init tier — `OVT_TEST_Init_TransferContexts.c`** (live managers, local player): both contexts resolve, both are `OVT_TransferContext` subclasses, both carry the same `m_Layout` and `m_sContextName`, and the shared `ActionContext` exists in the conf. The poll for the local player's UI manager is a **precondition with a named failure on expiry**, not a retry — same shape as `OVT_TEST_Init_Controller_ComponentsResolve`. No `maxAttempts`.

**What the automated spine cannot reach** — everything in the Verification play-test steps, because the suites cover no UI at all:

- Whether any widget is where it should be, whether the scroll list scrolls, whether a row draws its preview.
- **All focus behaviour** — initial focus, focus survival across a refresh, pane swapping, the picker's left/right claim, whether d-pad down reaches the picker at all (R3).
- The Add/Remove relabel, and the hidden-button-kills-the-keybind mechanism.
- Every gamepad binding. The conflict checker proves no *declared* collision; it cannot prove a press arrives.
- Whether the shop still looks right after the reparent (I3 is a diff check, not a render check).
- Multi-player: two clients at one warehouse, the second one's cart reconciling when the first empties a row. Uncovered by the suites and by the local dedicated server harness in any automated sense — it is a two-client manual test.

---

## 8. Quality Bar

UI-specific bars, on top of §6's Quality criteria. Each is a thing a reviewer can check and a thing this feature can plausibly get wrong.

- **B1 — Focus is never lost.** After any refresh (tab change, mode change, Accept, a warehouse invoker tick), something is focused, and it is the remembered index in the remembered pane where that index still exists. A pad player must never end up in a live menu with nothing focused. This is the single most likely regression and it has no automated guard.
- **B2 — No dead rows, ever.** Containers start empty in the layout and are cleared before every rebuild; a hidden row is `SetVisible(false)`, never `SetOpacity(0)` (an opacity-0 widget still takes clicks). No hand-authored placeholder survives into the shipped layouts.
- **B3 — The relabel is correct in every state.** The label a button shows and the action it performs cannot disagree — including in the awkward states: cart empty, list empty, mode without "Add all", focus lost entirely, and the frame right after an Accept clears the cart.
- **B4 — Gamepad-only operability, end to end.** Every step of the port and warehouse flows is reachable with the d-pad, `x`, `y`, `RT`, `L3`, `R3`, `a` and `b` alone. Verified by walking steps 13–17 without touching the mouse — not by reading the conf.
- **B5 — One shared contract, provably.** One layout, one `ActionContext`, one set of widget names. The Init case asserts it so a future consumer cannot quietly fork a second layout.
- **B6 — Teardown is symmetric.** `OnShow` and `OnClose` read as mirror images. Invokers on managers outlive the menu; a missed `Remove` is one extra refresh per warehouse visit for the rest of the session.
- **B7 — Hover targets are not grown via the widget tree.** If a row is hard to hit, the fix is a cursor-proximity magnet, not a bigger container: the hover trace is clipped to parent bounds and ancestor size overrides squeeze grown containers (two failed attempts on record).
- **B8 — No `ALWAYS_TOP` focusable widget.** A focusable widget on that layer becomes a gamepad focus island. Nothing in this layout goes there.

---

## 9. Dependencies

**Consumed, unmodified:**
- `economy` — `OVT_EconomyManagerComponent` (prices, player money, `GetInventoryId`, `ResistanceControlsNearestPort`), `OVT_VehicleRequestComponent.ImportToVehicle`, `OVT_RealEstateRequestComponent.TakeFromWarehouseToVehicle`, `OVT_RealEstateManagerComponent.m_OnWarehouseInventoryChanged`, `OVT_WarehouseData`, `OVT_MoneyFormat`, `OVT_PrefabUtils.GetItemUIInfo`.
- `economy/shop-ux` — `OVT_ShopCategory` / `OVT_ShopCategoryHelper` (the port's categories), `OVT_ShopBrowserModel` (pattern only), `ShopMenu_Tab.layout`.
- `core/game-mode` — `OVT_UIContext` / `OVT_UIManagerComponent` lifecycle, `Character_Player.et` registration, `OVT_ControllerComponent<T>.Get()`.
- Vanilla — `SCR_SpinBoxComponent` / `WLib_SpinBox.layout`, `WLib_NavigationButton.layout`, `SCR_InputButtonComponent`, `SCR_ButtonComponent`, `ItemPreviewWidget` + `ItemPreviewManagerEntity`, `ScrollLayoutWidgetClass`.

**Modified:** `OVT_ShopContext` (~11 lines), `OVT_ShopMenuTabComponent` (types only), `chimeraInputCommon.conf`, `Character_Player.et` (two blocks), `localization_Overthrow.st`.

**Nothing in the `logistics` epic.** This feature is first in the build order precisely because it has no intra-epic dependency.

**Downstream, planned against this:** `logistics/storage` (Open Storage; adds Put, Export and a batched checkout that supersedes D10's per-line loop at exactly one seam, `OnAccept`), `logistics/resources` (resource entries with `TEXTURE` images, a Put mode, price-drift text in the details body, m³ messages in the checkout area). Neither may require a hook this feature did not ship — §3.4's list is closed.

**Concurrent work on this tree:** `core/damage` is In Progress in another session. Do not modify its files, and re-baseline (`git pull` / `git status`) before every phase.

---

## 10. Risks & Mitigation

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| R1 | **The `MenuLeft`/`MenuRight` double-handle** — a d-pad press both changes the destination and swaps panes | High if unguarded | Confusing, pad-only, invisible to every static check | D6's explicit focused-picker early return, written in Phase 2; play-test step 16 exists solely to catch it |
| R2 | **Focus lost on refresh**, especially the warehouse's coalesced invoker rebuild mid-interaction | Medium | Screen becomes mouse-only on a pad | Remembered pane + index, clamped and restored after every rebuild (B1); tab widgets rebuilt only when the tab *set* changes; play-test step 11 |
| R3 | **d-pad down never reaches the picker or Accept** — engine directional focus does not walk out of a `ScrollLayout` the way the tree suggests | Medium | Picker/Accept unreachable on a pad | Named as a play-test question, not designed around; fallback is an explicit `SetFocusedWidget` on `MenuDown` from the last cart line |
| R4 | **The shop regresses** from the reparent or the tab-component retype | Low | A shipped, heavily used screen breaks | Diff caps (≤ 15 lines, `ShopMenu_Tab.layout` byte-identical), the deferred-activate path preserved verbatim, and an explicit shop play-test in step 12 |
| R5 | **A layout/`.meta` mistake** — duplicate widget GUID, fresh GUID on an inherited component, missing `.meta` | Medium | Menu silently does not appear; compile-check cannot see it | GUID rules by construction from the reserved `6A8E2C1…` series, the dedupe snippet in Phase 2's acceptance, all five platform configs in each `.meta`, Workbench open of all three layouts |
| R6 | **Preview-widget cost on a long list** — an unbounded import list of `ItemPreviewWidget` rows | Low–Medium | Frame-rate dip when opening a port | Same construction and same row count as the screen being replaced, so no regression by definition; if it bites, the fix is `TEXTURE` fallbacks or row recycling, which is a follow-up, not this feature |
| R7 | **Deleting a referenced layout** — a prefab block still pointing at `PortMenu.layout` after deletion | Low | Menu fails to open at runtime, invisible to compile-check | The prefab retarget and the deletion are the same task in Phases 4/5, with a repo-wide grep in the acceptance criteria |
| R8 | **Reparenting `OVT_ShopContext` upsets container serialization** of `m_aContexts` | Low | Shop context stops resolving from the prefab | No `[BaseContainerProps]` is involved (contexts resolve by concrete class name); guarded by the Init case plus a Workbench open of `Character_Player.et` |
| R9 | **Raw `#OVT-` keys during the play-test** until the user re-exports | Certain | Cosmetic only | Stated up front (D11); the user is asked for a re-export in Phase 6 |
| R10 | **Concurrent sessions** change the tree between phases | Medium | Merge pain, stale line references | Re-baseline before every phase; every code citation in this plan carries a file:line so drift is detectable |

---

## Agent Routing Summary

| Phase | Agent | Why |
|---|---|---|
| 1 — models + Logic cases | `component-developer` | Pure classes and test cases; no widgets, no world, no networking |
| **2 — layout, base context, ActionContext, focus & gamepad** | **`ui-developer-advanced`** ⚠️ | **The one phase that needs it.** A three-layout widget tree, a custom focus model competing with the engine's directional search *and* with `SCR_SpinBoxComponent`'s own listeners, seven new input actions on a shared context, and a relabel scheme that leans on the hidden-button-kills-the-keybind mechanism. Nothing here is verifiable by compile-check, and a mistake surfaces only on a pad |
| 3 — tab host + shop generalisation | `component-developer` | Small, surgical, and the risk is *not* writing more than 15 lines. A UI agent would be tempted to improve the shop |
| 4 — port consumer + deletions | `ui-developer` | Straightforward consumer + a mechanical deletion sweep, on rails laid by Phase 2 |
| 5 — warehouse consumer + deletions + Init case | `ui-developer` | As Phase 4, plus the invoker/reconcile wiring |
| 6 — conflict check, loc, help & wiki | main thread + `help-docs-sync` | The wiki writes need the `wikijs` MCP tools and their known failure modes |

**Every implementation-agent prompt must carry, verbatim:**

> Do not run `tools/run-tests.sh`. Your gate is `tools/compile-check.sh` exit 0 — I run the test suites myself after the phase completes.

`ui-developer` and `ui-developer-advanced` both exist and both know the `overthrow-ui-patterns` skill; the `-advanced` variant is the one to spend on Phase 2. Phases 4 and 5 touch disjoint files after Phase 3 lands and **may run in parallel**, provided each takes its own `chimeraInputCommon.conf` edit window (both delete blocks from the same file).

**Total estimate: 21–30 h across 6 phases**, plus one user-gated Workbench session and two play-test sessions (mouse, then gamepad-only).
