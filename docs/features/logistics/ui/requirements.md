# UI — Requirements

**Epic:** logistics
**Created:** 2026-08-20
**Formalized:** 2026-08-20 (from the user's draft; decisions recorded in the Decisions section)

## Overview

`ui` is the shared screen every transfer-style interaction in the epic will use: a **categorized List + Cart + Destination** interface, delivered as a base `OVT_UIContext` that consumers extend. It is built first because it depends on nothing else in the epic — its first two consumers are the **existing** port Import screen and the **existing** warehouse Take screen, which it replaces outright. Later features (`storage`, `resources`) then plug their new data (item storage, resources) into a screen that already exists, instead of each building its own.

This feature is about the **operation of the list and cart UI only**. It does not add a new server contract: Accept drives the existing per-line requests unchanged, and batching/export/new destinations are explicitly left to later features.

## Requirements

### The base context

- **One abstract base context** (`OVT_UIContext` subclass) that consumers extend. The base owns the layout, the widget lookups, the cart model, focus handling and the input bindings; a consumer supplies data and behaviour through a small set of overridable hooks:
  - the list of **entries** (id, display name, image, quantity-or-price column value, optional enabled flag + reason key, category id) for the current mode + category;
  - the set of **modes** (zero, one or two — e.g. Import/Export, Take/Put, Buy/Sell) and of **categories**, both optional;
  - the **destinations** for the picker (zero or more);
  - the **details-panel** content for the selected entry;
  - what happens on **Accept** for the cart lines and the chosen destination;
  - whether **"Add all"** is offered in the current mode.
- One **shared layout** and one **shared `ActionContext` block** in `Configs/System/chimeraInputCommon.conf` for every consumer, so bindings are identical on every screen built from the base. Consumers register on `Prefabs/Characters/Factions/INDFOR/FIA/Character_Player.et` as separate `OVT_UIManagerComponent` contexts (one per consumer) all pointing at the shared layout and the shared context name.
- Works for both kinds of thing the epic will put through it: **item prefabs** (preview rendered with an `ItemPreviewWidget`, as the port/warehouse screens do today) and **resources** (a plain icon/texture). The image slot must support both, chosen per entry by the consumer. Nothing in this feature may assume a resource ledger exists — the resource hooks are the same hooks the item consumers use, not a parallel API.

### Layout

Two columns under a tool header. No panel titles — the panels must be self-explanatory.

- **Tool header** — lifted from the Shop screen's `HeaderRow` (`UI/layouts/Menu/ShopMenu.layout:98-147`; `OVT_ShopContext.RefreshHeader/RefreshTabs/CycleTab`): mode toggle buttons on the left, category tabs with prev/next steppers in the middle. Modes and categories are consumer-defined; when a consumer defines fewer than two modes the mode buttons are hidden, when fewer than two populated categories the tab row is hidden, and when both are empty the whole header is hidden. Preserve the shop's rebuild-tabs-only-when-the-set-changes behaviour (gamepad focus survives a refresh) and the deferred-activate click path of the tab component. The shop's tab component is currently typed to `OVT_ShopContext`/`OVT_ShopCategory` and must be generalised for reuse — **the shop and gun-dealer screens themselves are not changed** and keep their current UI.
- **List (left column)** — a **scrolling row list** (port/warehouse style; no paging). Each row: image, name, and one value column (quantity for stock-type modes, price for purchase-type modes — consumer-defined). Sorted **alphabetically by name** only. No dead hand-authored placeholder rows in the layout.
- **Details (top right)** — larger image of the selected entry plus consumer-defined detail text.
- **Cart (under details)** — lines of (entry, quantity) built by selecting a list row and pressing **Add 1 / Add 10 / Add all**. When focus is on the cart (click a cart line, or d-pad right from the list), the same three buttons re-label to **Remove 1 / Remove 10 / Remove all** and act on the focused cart line. "Add all" is hidden by the consumer in modes where it has no meaning (Import).
- **Destination picker (under cart)** — a left/right picker (`SCR_SpinBoxComponent` / `WLib_SpinBox.layout`, already used by BaseMenu/FOBMenu/RealEstate) over the consumer-provided destinations. **Hidden when there is exactly one option** (or none).
- **Checkout (bottom right)** — an **Accept** button, plus an area for a running summary (e.g. total cost / line count) and for errors or information ("not enough room in the destination", "cannot afford", consumer-provided reason text).

### Input and gamepad

- Every verb has a keyboard key and a gamepad input; the screen must be fully operable without a mouse.
- **d-pad up/down** (`MenuUp/MenuDown`) browses whichever of list / cart is focused; **d-pad left/right** (`MenuLeft/MenuRight`) moves focus **between the list and the cart** — implemented as focus handling on the existing `Menu*` actions, not as new actions (these inputs are already claimed by every menu context).
- The destination picker takes `MenuLeft/MenuRight` **when it is focused** (d-pad down past the cart lands on it); no dedicated destination bindings.
- Accept is a focusable button (`MenuSelect` / `a`).
- Shared-context bindings, reusing shop precedents for consistency: modes on `shoulder_right` / `view` (shop's ModeBuy/ModeSell), categories on `thumb_left` / `thumb_right` (shop's Prev/NextCategory), Add/Remove 1 / 10 / All on `x` / `y` / `right_trigger` (warehouse's TakeAll precedent for RT). Keyboard defaults follow the shop (`1`/`2` modes, `Q`/`E` categories) and the warehouse (`,` `.` `;` for 1/10/All) unless the conflict checker objects.
- **Forbidden inputs:** `shoulder_left` (VON @ priority 110 — BUG-092), and `MenuNavLeft/Right` must **not** be listed (keeps both triggers free). Every new action gets its own unique `InputSource` GUID — do not copy the port/warehouse GUID collision.
- Retire a shortcut by `SetVisible(false)` on its button (an invisible `SCR_InputButtonComponent` does not fire) — this is how the Add/Remove relabel and the hidden "Add all" stay safe on shared inputs.
- Pass `python3 .claude/skills/overthrow-ui-patterns/scripts/check-input-conflicts.py` with no new collisions.

### First consumers (replacement, not parallel)

- **Port Import** — `OVT_PortContext` becomes a thin subclass of the base: **Import mode only** (no Export — see Out of Scope), categories reusing the shop's category mapping, the value column showing price, "Add all" hidden, destination = **the vehicle the player is sitting in** (picker hidden). Accept issues the **existing** `OVT_VehicleRequestComponent.ImportToVehicle` request once per cart line; the existing illegal-import gate (`IllegalImports` permission or `ResistanceControlsNearestPort`, client `OVT_PortContext.c:112` / server `OVT_VehicleRequestComponent.c:446`) and the BUG-102 "no no-op rows" rule are preserved.
- **Warehouse Take** — `OVT_WarehouseContext` becomes a thin subclass: **Take mode only** (Put arrives with `storage`), the value column showing stock, "Add all" available, destination = the occupied vehicle (picker hidden). Accept issues the **existing** `OVT_RealEstateRequestComponent.TakeFromWarehouseToVehicle` request once per cart line; the existing invoker-driven refresh (`m_OnWarehouseInventoryChanged`, coalesced) stays the source of truth and the one-handler-per-button rule (BUG-081) is preserved.
- The old `PortMenu.layout`, `WarehouseMenu.layout`, their row layouts/handlers and the `OverthrowPortContext` / `OverthrowWarehouseContext` conf blocks are **deleted**; the duplicated `SortByDisplayName`/display-name caches collapse into the base.
- Entry points are unchanged: `OVT_VehicleMenuContext.Import()` / the warehouse accessibility check continue to open the same two contexts.

### Quality

- Server-authoritative behaviour is unchanged because no server code changes; the cart is a **client-side model only**.
- New strings go in `Language/localization_Overthrow.st`; layouts referencing not-yet-exported keys use literal text until the user regenerates.
- Automated coverage: **Logic-tier** cases for the cart model (add/remove 1/10/all, clamping to available quantity, line merge/removal when a line reaches zero, totals) and the list model (alphabetical sort, category filter, mode/category reset rules) — written as pure classes so they are testable without widgets. The rest is a play-test gate: mouse and gamepad walk-through of both consumers.

## Decisions (2026-08-20, with the user)

1. Gamepad: focus-handling on `Menu*` for list↔cart and the picker; no dedicated destination bindings; shop/warehouse inputs reused as listed above.
2. Scrolling rows only; the shop/gun-dealer UI is untouched.
3. **No new server wiring.** Accept loops the existing per-line requests; a batched checkout RPC is a later feature's job.
4. **No Export** in this feature — `storage` adds it (so no throw-away vehicle-inventory reader is written here).
5. One destination for now (the occupied vehicle); the picker is built but hidden.
6. "Add all" hidden in Import.
7. Epic build order becomes: **ui → storage → resources → building-repair** (the five resource features were merged into `resources` later the same day).
8. Replacement, not parallel: old port/warehouse layouts, handlers and conf blocks are deleted.

## Dependencies

- `economy` epic — `OVT_PortContext` / `OVT_WarehouseContext` and their request components (`OVT_VehicleRequestComponent`, `OVT_RealEstateRequestComponent`), the shop's header/tab/category code (`OVT_ShopContext`, `OVT_ShopMenuTabComponent`, `OVT_ShopCategory`) that is generalised, `OVT_EconomyManagerComponent` price reads.
- `core/game-mode` — `OVT_UIContext` / `OVT_UIManagerComponent` lifecycle and prefab registration.
- Vanilla — `SCR_SpinBoxComponent`, `ItemPreviewWidget`, `SCR_InputButtonComponent`.
- **Nothing in the logistics epic.** This feature is first in the build order precisely because it has no intra-epic dependency.

**Downstream (consumers of this feature):** `logistics/storage` (Open Storage screen: Take/Put, adds Export to the port and Put to the warehouse), `logistics/resources` (Resources on the port screen with import/export and the drift indicator in the details panel; Resources + Put on the warehouse screen).

## Out of Scope

- **Export / selling items at the port** — `storage` owns it (needs a vehicle-inventory reader that would otherwise be rewritten).
- **A batched / cart-level server request**, any change to server validation, or any new RPC.
- Additional destinations (nearby vehicles, storage containers, warehouses) — the picker exists; populating it with more than one option is a later feature's call.
- Resources, ledgers, capacity (m³) checks and any resource data — the base only has to be *able* to display them.
- Put mode for warehouses; the "Open Storage" consumer.
- Changes to the shop / gun-dealer screens, or to `OVT_ShopBrowserModel`'s paging.
- Other sort orders / search / filtering beyond categories.
- Per-row quantity entry fields; quantity is driven by the 1/10/All buttons only.
