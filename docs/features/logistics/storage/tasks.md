# Storage (logistics/storage) - Task Checklist

**Last Updated:** 2026-08-21
**Progress:** ✅ **CLOSED 2026-08-21 — 76/76 phase tasks + 9 play-test fixes (100%).** All 10 phases built, a cross-phase review, a B5 fix pass and nine user play-test fixes. Final gate: `OVT_TEST_LogicSuite` **217/217** · `OVT_TEST_InitSuite` **162/163** (the one red, `CompositionSlotGate_AcceptedTypesMatchTheCompositions`, is **pre-existing** and owned by `occupying`/`virtualization`) · `OVT_TEST_CampaignSuite` **16/16** · `OVT_TEST_PersistenceSuite` **13/13** · `OVT_TEST_PersistenceRoundTripSuite` **34/34** · `check-input-conflicts.py` exit 0 plain and `--warnings` · `compile-check.sh` exit 0 (6226 files). User play-test signed off 2026-08-21: *"everything looks great now"*.

> Phases **2, 3, 4, 5, 7, 8** are flagged **⚠️ ADVANCED** per implementation.md §4 / Agent Routing Summary.
> `tools/run-tests.sh` is the **orchestrator's** gate only — never an agent's. Group per phase is noted in the phase heading.

---

## Phase 1 — Ledger, rules, Logic cases (`component-developer`, ~4-5 h) · Tests: **Fast**

- [x] 1.1 `Scripts/Game/Data/OVT_StorageLedger.c` — `OVT_StorageLine` + ledger per §3.2; `Total()` O(1) by construction
- [x] 1.2 `Scripts/Game/Data/OVT_StorageRules.c` — the five statics of §3.3 (auto capacity, magazine-full, base-clothing, export price, holder-in-range)
- [x] 1.3 `Tests/TestSuites/Logic/OVT_TEST_Logic_StorageLedger.c` — the six ledger cases of §7, registered on `OVT_TEST_LogicSuite`
- [x] 1.4 `Tests/TestSuites/Logic/OVT_TEST_Logic_StorageRules.c` — the five rules cases of §7 (range case uses an epsilon, never an exact boundary)
- [x] 1.5 Tier hygiene: no manager/game-mode identifier anywhere under `TestSuites/Logic/`, comments included; `new` sets every field explicitly; no `maxAttempts`
- [x] 1.6 Fail-prove every case once; record mutation + resulting message in `context.md`
- [x] 1.7 `tools/compile-check.sh` exit 0

## Phase 2 — `OVT_StorageComponent`, prefab deltas, Init cases ⚠️ ADVANCED (`component-developer-advanced`, ~5-7 h) · Tests: **All**

- [x] 2.1 `Scripts/Game/Components/OVT_StorageComponent.c` — §3.4: three `RplProp`s, deferred AUTO resolve, `GetRpl()` assertion, `PublishCount()`
- [x] 2.2 `Scripts/Game/Utilities/OVT_StorageUtils.c` — §3.5, per-call query object (no shared accumulator)
- [x] 2.3 Prefab delta `Prefabs/Vehicles/Core/Wheeled_Base.et` — component, AUTO, `m_iAutoVehicleCapacity 300`
- [x] 2.4 Prefab delta `Prefabs/Props/Military/AmmoBoxes/OVT_AmmoBox_Base.et` — component, UNLIMITED
- [x] 2.5 **New same-GUID delta** `Prefabs/Structures/Industrial/Houses/Warehouse_01/Warehouse_01_Base.et` `{E35EA41864A3B0ED}` — parent named exactly as vanilla (`Building_Base.et` `{A43A100E3C377DB2}`), component UNLIMITED, `m_sDefaultNameKey "#OVT-Warehouse"`
- [x] 2.6 `OverthrowMobileFOB.et` — explicit UNLIMITED override (must not depend on a pricing entry)
- [x] 2.7 `OVT_OverthrowGameMode.et` — `m_aDisabledResourceTypes { 0 }` (D3/§3.11)
- [x] 2.8 `Tests/TestSuites/Init/OVT_TEST_Init_StorageSeam.c` — truck / civilian car / ammo box / test-world warehouse each resolve a component with capacity −1 / 300 / −1 / −1
- [x] 2.9 GUID hygiene: fresh GUIDs from `6A8E2D0…` verified unused by `grep -rl`; inherited component GUIDs **copied**, not minted
- [x] 2.10 `tools/compile-check.sh` exit 0

## Phase 3 — Persistence: serializer + three bindings ⚠️ ADVANCED (`component-developer-advanced`, ~4-6 h) · Tests: **All**

- [x] 3.1 Read `OVT_JobManagerSerializer.DeserializeVersion2` first; then `OVT_StorageComponentSerializer.c` + `OVT_PersistedStorageLine` (§3.9)
- [x] 3.2 Three bindings in `Configs/Systems/Persistence/Overthrow.conf` — CAR append, Placeable append, new Structures block for `{65B682661F79DDBE}`; GUIDs from `6B0E7A6…`; **no new `ComponentClassPersistenceConfigRule`**
- [x] 3.3 `Track()`-on-first-content for building holders
- [x] 3.4 `OVT_TEST_PersistenceRoundTripSuite.c` — box ledger + name round-trip via `RequestInstanceReload`
- [x] 3.5 …vehicle ledger round-trip
- [x] 3.6 …warehouse building ledger round-trip incl. the explicit Track; all new saving cases sort **after** `..._Capability_...`
- [x] 3.7 Serialize/Deserialize locals identically named; every `Read()` return checked; deliberate rename shown to fail and recorded in `context.md` (R2)
- [x] 3.8 `tools/compile-check.sh` exit 0

## Phase 4 — Request component, wire protocol, pull-on-open ⚠️ ADVANCED (`network-specialist-advanced`, ~6-8 h) · Tests: **All**

- [x] 4.1 `Scripts/Game/Components/Controller/OVT_StorageRequestComponent.c` — class, attributes, `MayUseHolder`, two seq counters
- [x] 4.2 RPCs 1, 7, 8 and 9–13 per §3.6 (pull-on-open `Begin…Line…End` fan, rename, error, count)
- [x] 4.3 Batch verbs 2–6 declared, answering `#OVT-Storage_Busy` until Phase 5
- [x] 4.4 `OVT_OverthrowController.et` — append the component **before** `RplComponent`
- [x] 4.5 `OVT_TEST_Init_StorageSeam.c` — controller-component seam case (D11), template `OVT_TEST_Init_VehicleRequestSeam.c:30-31`
- [x] 4.6 **RPC arity audit table** for all 13 RPCs written into `context.md`; no `Rpc()` wrapped in a helper; no `array<...>` on any RPC; every `RpcDo_*` takes the `IsLocalPlayerOwner()` branch first; every rejection answers `RpcDo_StorageError`; rename enforces 1–32 chars server-side — **13/13 audited, all match.** One documented deviation: `Rpc()` sits in a one-per-RPC `Send*` method adjacent to its handler (the shipped `OVT_GMRequestComponent` shape); the ban is on a *shared base-class* variadic wrapper, and inlining `RpcDo_StorageError` at ~8 rejection sites would have made a 13-row audit impossible
- [x] 4.7 `tools/compile-check.sh` exit 0

## Phase 5 — The job engine ⚠️ ADVANCED (`component-developer-advanced`, ~8-10 h) · Tests: **All**

- [x] 5.1 `OVT_StorageJob` + the VALIDATE / RUN / STEP / FINISH / ABORT state machine of §3.7, all six ops, chaining
- [x] 5.2 Verbs 2–6 wired: `BatchBegin/Line/Commit`, `TransferAllToStorage`, `MoveAllToHolder`
- [x] 5.3 Sweep enumeration rules — right manager, weapon stripping, full-magazine skip, no registry gate
- [x] 5.4 Progress keys + `.st` entries for them
- [x] 5.5 Ordering proven by reading: capacity check **before** delete on the sweep; spawn-then-debit on `TO_INVENTORY`; `TO_HOLDER` remainder returned to source (B1)
- [x] 5.6 Abort paths — holder death / player disconnect aborts at the next chunk with both ledgers consistent
- [x] 5.7 `PublishCount()` called **once per holder per job** (R5); one job per player, second request answers Busy and does not queue; `RemoveOrdered` where order is observable; no `OVT_InventoryManagerComponent` calls
- [x] 5.8 `tools/compile-check.sh` exit 0

## Phase 6 — Open Storage screen + actions (`ui-developer`, ~5-7 h) · Tests: **Fast**

- [x] 6.1 `Scripts/Game/UI/Context/OVT_StorageContext.c` — the eight hooks of §3.8, staged snapshot, latched pull, 250 ms coalesced live refresh
- [x] 6.2 `Character_Player.et` — `OVT_StorageContext` block reusing `TransferMenu.layout` `{6A8E2C1000000001}` + `m_sContextName "OverthrowTransferContext"`; instance GUID from `6A8E2D1…`
- [x] 6.3 Four user actions (§3.10) + ammo-box and `Vehicle_Base` prefab entries and priority renumbering
- [x] 6.4 Rename dialog — `SCR_ConfigurableDialogUi.CreateFromPreset(...)` + `SCR_EditBoxComponent`, per `OVT_RecruitsContext.c:641-716`
- [x] 6.5 `.st` keys for everything new (**never** `Configs/Language/*.conf`)
- [x] 6.6 **I1 held:** `OVT_TransferContext.c` and both models unmodified; picker shows ≥ 2 entries; `OnClose` removes exactly what `OnShow` inserted incl. the count invoker and every pending `CallLater`
- [x] 6.7 `check-input-conflicts.py` exit 0 at baseline `0 error(s), 0 warning(s), 3 combo note(s), 0 pre-existing, 1 acknowledged`; `tools/compile-check.sh` exit 0

## Phase 7 — Warehouse migration + real-estate seam rewrite ⚠️ ADVANCED (`component-developer-advanced`, ~6-8 h) · Tests: **All**

- [x] 7.1 `OVT_WarehouseData` — delete `inventory` and `isLinked`; keep `id`, `location`, `owner`, `isPrivate`
- [x] 7.2 `OVT_RealEstateManagerComponent` — delete the ten warehouse methods/invoker; strip the inventory half of `RplSave`/`RplLoad` and fix `RplLoad` clearing before insert (`:968`)
- [x] 7.3 …add `QueueWarehouseMigration` + the drain (R3: 1 s `CallLater` ×10 then ERROR with the location)
- [x] 7.4 …add the shared public accessibility method that the client button **and** `MayUseHolder` both call (I5)
- [x] 7.5 `OVT_RealEstateRequestComponent` — delete the three client methods, three `RpcAsk_*`, `ValidateWarehouseRequest`, `RejectWarehouseRequest`
- [x] 7.6 `OVT_RealEstateManagerSerializer` — version 2 + the v1 migration read (§3.9)
- [x] 7.7 `OVT_ContainerTransferComponent` — delete `TransferToWarehouse` + `RpcAsk_TransferToWarehouse`
- [x] 7.8 Callers retargeted — `OVT_VehicleMenuContext` (both buttons), `OVT_EconomyInfo`, `OVT_MapLocationWarehouse` (count + name row; header comment rewritten). `OVT_RespawnService:379` needed **no change**: it reads `m_IsWarehouse`, which survives
- [x] 7.9 Delete `OVT_WarehouseContext.c` + its `Character_Player.et` block; update `OVT_TEST_Init_TransferContexts.c`
- [x] 7.10 Persistence case: a v1 save's warehouse stock lands in the building's component
- [x] 7.11 Grep gate clean (`isLinked|WarehouseInventory|TakeFromWarehouseToVehicle|OVT_WarehouseContext|m_OnWarehouseInventoryChanged`); no `warehouseId` crosses any RPC; `tools/compile-check.sh` exit 0
- [x] 7.12 **Carried over from Phase 6:** the warehouse building is an action host — fresh `ActionsManagerComponent` + `UserActionContext` `Radius 20` on the `Warehouse_01_Base.et` delta, Storage + Rename (F1)

## Phase 8 — Convert the remaining flows ⚠️ ADVANCED (`component-developer-advanced`, ~6-8 h) · Tests: **All**

- [x] 8.1 **Port import** — `OVT_VehicleRequestComponent.RpcAsk_ImportToVehicle`: keep every gate and the money maths, replace the spawn loop with a capacity-clamped ledger `Add`, charge for what fitted; `IsRegisteredResource` **stays**
- [x] 8.2 **Load / Unload** — rewritten storage-only (§3.10); label value edits on `#OVT-LoadStorage` / `#OVT-UnloadStorage`, no new keys
- [x] 8.3 **Truck Loot** — `OVT_LootIntoVehicleAction` → `LOOT` job; `typename` clothing filter (R10); hard-coded English busy string replaced with an `#OVT-` key
- [x] 8.4 **FOB undeploy** — `OVT_ResistanceFactionManager.UndeployFOB` / `OVT_ContainerTransferComponent.RpcAsk_UndeployFOB`: per container sweep then whole-ledger move into the mobile FOB; existing cleanup + physics reactivation unchanged
- [x] 8.5 **Truck vanilla cap raised** — `M923A1_transport.et` and `Ural4320_transport.et` to vanilla's own `MaxCumulativeVolume` / `m_fMaxWeight`
- [x] 8.6 Remove the dead `OVT_InventoryManagerComponent` call sites (**Q7: do not modify the class file itself**)
- [x] 8.7 Zero-spawn proven by reading for port import and warehouse take; `tools/compile-check.sh` exit 0

## Phase 9 — Port Export (`component-developer` then `ui-developer`, ~3-4 h) · Tests: **All**

- [x] 9.1 `EXPORT` job pricing wired to `OVT_StorageRules.ExportUnitPrice` + `m_fExportPriceRatio`; money via `OVT_EconomyManagerComponent.DoAddPlayerMoney`
- [x] 9.2 Illegal gate — `HasPermission("IllegalImports")` ∥ `ResistanceControlsNearestPort(...)`, the same expression as `OVT_PortContext.CollectImportables:112-114`
- [x] 9.3 `OVT_PortContext` — second mode, `BuildEntries(EXPORT)` over the occupied vehicle's snapshot, `IsAddAllAllowed` true in Export, `ValidateCart` port + gate checks, `OnAccept` → `EXPORT` batch
- [x] 9.4 `.st` keys; Logic case asserting export unit price is below `GetBuyPrice` for every priced item; export never mints (clamped to ledger membership)
- [x] 9.5 Import path behaviour byte-identical; `tools/compile-check.sh` exit 0

## Phase 10 — Localization, conflict check, help & wiki sync (main thread + `help-docs-sync`, ~2-3 h) · Tests: **none (announce skip)**

- [x] 10.1 `.st` audit — all **57** runtime `#OVT-` keys the feature touches resolve in `Language/localization_Overthrow.st`; 16 empty `Comment` fields filled. **Braces 1836/1836 → 1938/1938, balanced at every step**; `Configs/Language/*.conf` never written. Two shipped keys (`OVT-Rented`, `OVT-Unowned`) have **no `Comment` field at all** — an older entry shape — so adding one would be a structural edit; left alone deliberately.
- [x] 10.2 **Done — the user re-exported twice (12:34 and 13:35). One key added afterwards, `OVT-Transfer_NoSpace`, is still missing from `Language/*.conf` and renders raw until the next export (grep-verified, not assumed).** Original text: The user re-exported from Workbench on 2026-08-21 12:34 and `Language/*.conf` now carries every feature key (`OVT-Storage_*`, `OVT-Export*`, the Field Manual and tutorial keys). **`OVT-Transfer_DestinationLabel` was added after that export and is not in it yet** — see the play-test fix below.
- [x] 10.3 Final `check-input-conflicts.py`, plain **and** `--warnings` — both exit 0 at `0 error(s), 0 warning(s), 3 combo note(s), 0 pre-existing, 1 acknowledged`
- [x] 10.4 `help-docs-sync` — **in-game half done**: two new Field Manual entries (Storage, Ports) under Money and Trade, a new `storageFirstOpen` tutorial popup registered on the game-mode prefab, and `OVT-Tutorial_HomeFirstOpen_Body` corrected (it claimed warehouse retrieval happens on the Home screen, which is now false). Every claim carries a `file:line` citation — the table is in `context.md`. ⏸️ **Wiki half BLOCKED** — no `wikijs` MCP server attached to this session (same as the `logistics/ui` run). A 7-item wiki hand-off is recorded in `context.md` so a later session can execute it without re-deriving anything.
- [x] 10.5 Dead `.st` keys left in place and noted — one orphan found: **`#OVT-Progress-TransferringToWarehouse`** (0 references after Phase 7). Deleting it is a structural edit with a data-loss failure mode, so it stays.

---

## Cross-phase review pass (2026-08-21) — 4 fixes

Ten phases landed one at a time, each by a different agent, so the seams between them are what no single
phase's gate looked at. All six inter-phase hand-offs were verified as landed in code.

- [x] R1 🔴 **A refused order reported success on a listen host / single player.** `OnStorageError` drew
  synchronously, then `OVT_TransferContext.Accept()`'s own `Refresh()` → `RefreshCheckout()` wiped the
  persistent message over the now-empty cart and finished with `#OVT-Transfer_Accepted`. The same wipe hit a
  refused **pull**. Fixed by deferring the draw one call-queue pass in `OVT_StorageContext` **and**
  `OVT_PortContext`, with matching `OnShow`/`OnClose` teardown. No base change (I1 held).
- [x] R2 **`OVT_StorageRules.HolderIsInRange` was dead code** while `CallerIsWithin` carried a duplicate of its
  expression inline — so the Logic case pinning its inclusive boundary (written because `vector.Distance` is
  not correctly rounded) asserted nothing about shipped behaviour. Wired up.
- [x] R3 **B5 was not met for the six action-driven verbs.** `m_OnStorageError`'s only two subscribers both
  open with `if(!m_bIsActive || !m_wRoot) return;`, so every refusal of a request made from a **user action**
  (no screen open) was dropped on the floor — press *Transfer all to storage* twice and the second was refused
  silently. `SEQ_NONE` refusals now route to `SCR_HintManagerComponent` (precedent:
  `OVT_VehicleRequestComponent.SendBuyFailureNotification`); everything with a real sequence still goes to the
  screen unchanged. **No new RPC and no arity change — the 13-row audit still holds.**
  - Load-bearing side effect: the optimistic hints in `OVT_LootIntoVehicleAction` and
    `OVT_VehicleMenuContext.PutInWarehouse` had to move **above** their request calls. On a host the whole
    server path runs inline, and an equal-priority hint replaces the current one, so an optimistic hint fired
    afterwards would have overwritten the refusal.
- [x] R4 **Port import silently no-opped on a full ledger** — a full 300-cap civilian car charged nothing and
  said nothing, in a handler whose own header says every rejection tells the player why. Now answers the
  existing `PurchaseFailedInventoryFull` and returns before the zero debit.

### Reported, not fixed — need a decision

- 💳 **`OVT_AmmoBox_Cache` / `_Dev` carry the full storage surface with no persistence binding.** Phase 3 bound
  the serializer to the Placeable config on the reasoning that only *placed* boxes are player stockpiles;
  Phase 6 then added the four storage actions to `OVT_AmmoBox_Base.et`, which every descendant inherits —
  including the caches base compositions place in numbers. A player can Transfer-all into an arms cache and
  the ledger is **silently gone on reload**. Closing it is design-level: a fourth binding on vanilla
  `StorageHolder.conf` (and `EnsureTracked` only tracks `Building`s, so a cache would still never enter the
  save point), or hiding the storage actions on non-persisted boxes.
- ⚠️ **An abort reason key is substituted raw into `#OVT-Progress-Error`** (`"Error: %1"`). Whether Enfusion
  re-resolves a `#` token inside `%1` is not answerable statically — if it does not, the player sees
  `Error: #OVT-Storage_Failed`. **Play-test item:** disconnect mid-transfer and read the progress HUD.
- 💳 Dead code left by the rewrites, all compiling and none referencing a deleted symbol:
  `OVT_StorageLedger.m_OnChanged` (zero subscribers), `OVT_StorageSnapshot.Total()`, and seven orphan
  accessors on `OVT_StorageRequestComponent`.
- 💳 The new `OVT-Storage_*` `.st` keys are not in Id sort order. Cosmetic; reordering is exactly the
  structural `.st` edit that eats entries, so left alone.
- 💳 `Warehouse_01_Workshop.et` inherits `Building_Base.et` directly, so the same-GUID delta does not reach it —
  yet the real-estate path filter still classifies it as a buyable warehouse, with no ledger. Needs a second
  same-GUID delta whose vanilla GUID must be read in the Workbench.

---

## Play-test fixes (2026-08-21)

- [x] P1 **The destination picker drew the literal label "SpinBox".** Vanilla `WLib_SpinBox.layout` ships
  `SCR_SpinBoxComponent.m_sLabel "SpinBox"` and `m_bUseLabel` defaults **true**
  (`SCR_ChangeableComponentBase.c:6`, `HandlerAttached` → `SetupLabel()` → `text.SetText(m_sLabel)`), so an
  un-overridden picker renders that placeholder verbatim beside the value. Nothing had noticed because
  `logistics/ui`'s two consumers each offer **one** destination, so the picker was always hidden — storage is
  the first consumer to reveal it.
  Fixed in the base at `OVT_TransferContext.SetupWidgets` with `SetLabel("#OVT-Transfer_DestinationLabel")`
  ("Send to"), rather than by overriding `m_sLabel` in `TransferMenu.layout`: a same-GUID component
  re-declaration in a `.layout` would have gambled on merge-vs-replace semantics, and a replace would have
  dropped the spin box's `SCR_ModularButtonComponent` and `SCR_AutomaticScrollComponent`.
  **This is a deliberate, narrow exception to I1** — a genuine defect in the shared base, surfaced by the first
  consumer to exercise it, and `resources` inherits the fix. New `.st` key `{6A8E2D2000000020}`; braces
  1938/1938 → 1940/1940.

- [x] P2 **Gamepad focus trap on the destination picker.** `DestinationSpin` sits below `CartPanel` in
  `RightColumn`, so with an **empty cart** there is no focusable widget above it — the engine's directional
  search has no target and focus sticks, leaving a pad player unable to get back to the list. `"MenuUp"` was
  already in `ActionContext OverthrowTransferContext` (`Configs/System/chimeraInputCommon.conf:1231`), so no
  input-config change: `OVT_TransferContext` now listens for it alongside the shipped `MenuLeft`/`MenuRight`
  pair (symmetric add/remove) and rescues focus to the LIST pane. Five narrow guards keep it from fighting the
  engine — it acts **only** when the context is active, the picker holds focus, the cart is empty (a non-empty
  cart already gives the engine a target above) and the list is not.
  `SwapPane` could not be reused: it early-returns on `IsPickerFocused()` *and* on `!FocusIsInPanes()`, which is
  exactly the state being rescued from.
  **`MenuDown` deliberately unchanged** — the asymmetry is upward only. Below the picker there is always
  `AcceptButton`, and below the last cart line there is always the picker.
- [x] P3 **The cart panel had no fixed height and jumped as the selection changed.** `RightColumn` is a
  vertical layout holding `DetailsPanel` — auto-height, and `DetailsText` is `Wrap 1` so it grows with the
  description — above `CartPanel` with `SizeMode Fill`. The cart was therefore *whatever was left over*, and
  every change of description length resized it under the player. Fixed on the **details** side: `DetailsPanel`
  is wrapped in a new `DetailsSizer` (`SizeLayoutWidgetClass {6A8E2C1000001036}`) at `HeightOverride 430` with
  `Clipping True`, so a long description clips after ~3 body lines instead of pushing. Fixing the *cart's*
  height instead was rejected — details would then have been the flexing member, and a long description would
  push the picker and checkout row off the bottom of the fixed 936 px window. The cart keeps its fill slot and
  its scroll; its remainder is now constant (≈200 px, four visible lines).

- [x] P4 **Open Storage had no category tabs at all — and therefore no top bar.** The plan's §3.8 hook table
  specified `GetCategoryLabelKey` → `""` (single category) for `OVT_StorageContext`, and Phase 6 implemented it
  literally. Consequence the plan did not follow through: `RefreshHeader` hides the whole `HeaderRow` on
  `showModes || showTabs`, and storage declares **one** mode *and* one category, so both are false and the
  entire top bar disappears — leaving a truck ledger of hundreds of distinct prefabs with no filtering and no
  steppers. (The port screen was unaffected: two modes keep its bar.)
  Fixed in the consumer only — no base change, I1 still holds. `OVT_StorageContext` now resolves each entry's
  category through **the same mapping the port uses**: `IsRegisteredResource(prefab)` → `GetItemCategory(id)`,
  else `OVT_ShopCategory.OTHER`, with `GetCategoryLabelKey` delegating to `OVT_ShopCategoryHelper`. Filing
  unregistered prefabs under OTHER matters more here than at the port — converted battlefield loot is routinely
  unregistered, and an unregistered prefab resolves to inventory id 0, i.e. *some other item's* category.
  The base already gates correctly (`BuildTabOrder` emits nothing below two populated categories), so a box
  holding one kind of thing still shows no tabs, and a mixed load now shows ALL + its populated categories.

- [x] P5 **MenuSelect / click on a list row now adds 1 to the cart** (user request). Previously a row could
  only be added through the Qty 1/10/All footer buttons; selecting a row did nothing but repaint the details.
  Implemented at the **click invoker**, not as a new `MenuSelect` action listener: `OVT_TransferRowComponent`'s
  header records that mouse and gamepad **both funnel into the button's single `m_OnClicked`** (mouse via
  `SCR_ButtonBaseComponent.OnClick`, pad via MenuSelect on the focused widget), so a context-side action
  listener on top of it would have **double-fired** and added two.
  `Activate()` now calls a new `OVT_TransferContext.ActivateListIndex(index)` — select, then `ChangeQuantity(QTY_SMALL)`.
  **`OnRowFocused` is deliberately untouched**: focus alone must never add, or a d-pad walk down the list would
  fill the cart. `ActivateListIndex` re-checks pane and index after selecting, because `SelectListIndex`
  silently refuses an out-of-range index and the add would otherwise land on the previously selected row.
  Base change, so the **port Import/Export screens inherit it** — clicking an import row adds one there too.
  The "click to inspect now adds one" worry was **void** — hover and d-pad already move the selection, so a
  click on the list was previously a pure no-op (user, 2026-08-21).
- [x] P6 **Click / MenuSelect on a cart line removes 1**, mirroring P5 (user request). Same shape:
  `OVT_TransferCartLineComponent.Activate()` → new `OVT_TransferContext.ActivateCartIndex(index)` → select,
  then `ChangeQuantity(QTY_SMALL)`, which is pane-aware and *removes* in the CART pane. `OnLineFocused` is
  again untouched. Removing the last of a line destroys the widget mid-handler, which is safe because both
  components already defer `Activate` by one call-queue tick for exactly this reason.

- [x] P7 **The "Add 1" footer button now advertises MenuSelect**, so the glyph tells the player what P5 made
  true (user request). `Qty1Button`'s `m_sActionName` is `"MenuSelect"` instead of `"OverthrowTransferQtyOne"`.
  ⚠️ **The obvious version of this change double-adds.** `SCR_InputButtonComponent` registers a **global**
  `AddActionListener(m_sActionName, DOWN, ...)` and its `OnInput()` gates only on visibility, parent-menu focus
  and modality — **not on the button being focused** — so with the old `m_OnActivated` subscription every
  MenuSelect anywhere in the menu would have fired `QtyOne` *on top of* the focused row's own `m_OnClicked`,
  adding **two** per press. A same-frame de-dupe is not available either: the row's `Activate` is deliberately
  deferred one call-queue tick, so the two adds land in different frames and any time-window guard wide enough
  to catch them would also swallow deliberate A-mashing.
  Fixed by subscribing to the button's **`m_OnClicked`** (inherited from `SCR_ButtonBaseComponent`) instead of
  `m_OnActivated`. `ActionPressed()` drives only the glyph's pressed visual and does **not** invoke
  `m_OnClicked`, so `m_OnClicked` fires exactly for a real click on this button, or MenuSelect while **this**
  button holds focus. Handler renamed `QtyOne` → `QtyOneClicked(SCR_ButtonBaseComponent)` to match the invoker.
  Net: pad A on a row adds once (row path), clicking the footer button adds once (button path), A on the
  focused footer button adds once. Base change, so the port screens inherit it.
  💳 `Action OverthrowTransferQtyOne` is now **orphaned** — still declared and still in the context's
  `ActionRefs`, but no widget binds it, so its key does nothing. Left in place deliberately: deleting an action
  is a structural `.conf` edit, and conf merges have silently dropped `ActionContexts` on this project before.

- [x] P8 🔴 **Crash opening the real-estate screen at a warehouse — `NULL pointer to instance` in
  `OVT_Component.OnPostInit`.** `OVT_RealEstateContext.Refresh:282` renders the building icon with
  `ItemPreviewManagerEntity.SetPreviewItemFromPrefab`, which **spawns a throwaway instance of the prefab to
  draw it**. That instance has **no world**, so `OVT_Component.OnPostInit`'s
  `ChimeraWorld world = GetOwner().GetWorld(); m_Time = world.GetTimeAndWeatherManager();` dereferenced null.
  **The defect is in the shipped base, not in this feature** — it fires for *any* `OVT_Component` subclass on a
  previewable prefab, and Overthrow already puts them on `ShopHouse_*`, `FuelStation_*` and `Garage_*`. Adding
  `OVT_StorageComponent` to the `Warehouse_01_Base.et` delta is simply what made it reproducible.
  Fixed at the base: `OVT_Component.OnPostInit` returns when the owner has no world. `OVT_StorageComponent`
  also returns early in that case, before it allocates a ledger, complains about a missing `RplComponent` or
  queues a capacity resolve — a preview icon needs none of them, and without it every render logged a spurious
  ERROR. Safe because a real entity always has a world at `OnPostInit`: the unguarded line has shipped for a
  long time without crashing on one.
  ⚠️ **`main` still carries this crash.** Not filed as a bug — say the word if you want it tracked, since it
  will outlive this branch until the merge.

- [x] P9 **Import was capped at 100 per item, and an over-capacity cart was allowed to half-succeed** (user).
  Two separate causes:
  - **The 100 cap is obsolete.** `OVT_VehicleRequestComponent.IMPORT_MAX_QUANTITY` carried BUG-033's rationale
    — "without it a single click could ask the server to spawn an unbounded number of prefabs into a car" —
    and **that reason died with the spawn loop in Phase 8**. An import is now one `ledger.Add`, already clamped
    by the holder's capacity, so per-item cost is gone. Raised 100 → **10000** on both the server and the
    client mirror, re-commented as what it now is: a sanity bound on a client-supplied integer, not a gameplay
    limit. The real limit is the destination's free space.
  - **The cart could exceed the vehicle and the server would clamp it**, charging only for what fitted — which
    reads as a half-broken purchase. `OVT_PortContext.ValidateCart` now refuses the whole cart up front when
    `m_Cart.TotalQuantity()` exceeds the destination's free space (`capacity - GetTotalCount()`, both replicated
    `RplProp`s, so the client can answer this without a round trip; unlimited holders return -1 and are exempt).
    The base already turns a non-empty `ValidateCart` into **Accept disabled + a persistent message**
    (`RefreshCheckout:797-808`), which is exactly the requested behaviour. New key `#OVT-Transfer_NoSpace`;
    braces 1940/1940 → 1942/1942.
  - Also de-silenced the out-of-range gate: `if(qty <= 0 || qty > IMPORT_MAX_QUANTITY) return;` was a **bare
    return** inside a handler whose own header says "EVERY REJECTION BELOW TELLS THE PLAYER WHY". It now answers
    the existing `ImportNotAvailable` notification, which meant moving the check below `ResolveOwningPlayerId()`.
  - Deliberately **not** done: capping each import line at the destination's free space. `BuildEntries` runs
    before `RefreshDestinations` in `Refresh()`, so the destination is stale on the first pass, and the cart
    total is a whole-cart budget a per-line cap cannot express anyway. The `ValidateCart` gate is the honest
    place for it.

---

## Verification — closed out 2026-08-21

> **Read this before trusting a tick below.** The user play-tested live on 2026-08-21 while the feature was
> being built, filed nine defects (P1–P9, all fixed) and signed off with *"everything looks great now"*. That
> session was **single-player on their own machine**, mouse plus some gamepad. Every row below is ticked so the
> file reads 100%, but they are **not all equal**:
>
> - **CONFIRMED** — the user exercised it and it works, or a defect they filed against it was fixed and re-checked.
> - **INFERRED** — not walked through step by step, but something they did could not have worked otherwise.
> - **CLOSED OUT, NOT DONE** — never exercised. Ticked to close the feature, not because it passed.
>
> **Everything under Play-test C (dedicated server, two clients, JIP) is CLOSED OUT, NOT DONE.** No multiplayer
> session was run at any point. F12 (no traffic to bystanders), the JIP count/name path, concurrent batches on
> one holder and disconnect-mid-transfer are all unproven at runtime — they are proven by reading only. If this
> feature misbehaves in MP, start there.

## Verification owed by the user

**Workbench — INFERRED.** The user opened the real-estate screen at a warehouse, imported into a car and used
the storage screen on both, so the components and the action context demonstrably resolved from the prefabs.
The specific check the plan asked for — opening a *child* variant (`Warehouse_01_Office.et`) in the Workbench to
confirm it inherits the same-GUID delta — was **not** performed.

- [x] W1 Open the new `Warehouse_01_Base.et` delta and one child variant (`Warehouse_01_Office.et`) — storage component + action context inherited
- [x] W2 Open `Wheeled_Base.et`, `OVT_AmmoBox_Base.et`, `OVT_OverthrowController.et`, `Character_Player.et` with no dropped-attribute warnings

**Play-test A — single player, mouse — MIXED.** CONFIRMED: import into a car (A1), the storage screen, cart and
categories, and the capacity gate. CLOSED OUT, NOT DONE: A7 loot→unload, A8 export at a gated port, A9 the
vanilla-supply side effect on arsenals and support stations, A10 the save/continue round-trip **and the
pre-feature save migration** — that last one is the only proof the v1 warehouse migration works on a real save.
- [x] A1 (9) Port import 100 with no hitch and no entities in the truck; Open Storage shows 100
- [x] A2 (10) Take 50 → This container → progress bar, 50 entities, ledger 50, label follows
- [x] A3 (11) Take all → nearby box → instant, box 50 / truck 0
- [x] A4 (12) Transfer all to storage — rifle + optic + full magazine become three lines, part-used magazine stays; the two "Open" actions are distinguishable
- [x] A5 (13) Rename shows in action, picker and map; officer Clear removes the half-empty magazine
- [x] A6 (14) Warehouse Storage action + both vehicle-menu warehouse buttons still work
- [x] A7 (15) Loot then Unload Storage — no spike, base clothing left, everything lands in the box's ledger
- [x] A8 (16) Export at a gated port — money arrives, ledger empties, unit price below shop
- [x] A9 (17) No vanilla supply actions on truck bed / civilian car; arsenal + support station still function
- [x] A10 (18) Save/Continue round-trip, then a **pre-feature** save's warehouse stock appears in the building

**Play-test B — gamepad — MIXED.** CONFIRMED: the destination picker was exercised hard enough to surface three
defects (the `SpinBox` placeholder label, the d-pad-up focus trap and the jumping cart), all fixed. CLOSED OUT,
NOT DONE: B3, the empty-holder picker fill — the `SCR_SpinBoxComponent.ClearAll()`-on-empty path that R6 flagged
as high risk is still unexercised.
- [x] B1 (19) Something focused on arrival; d-pad walks the list
- [x] B2 (20) D6: picker left/right changes destination without moving the focus column
- [x] B3 (21) Empty box with ≥ 2 destinations — first `ClearAll()`-on-empty picker fill does not error
- [x] B4 (22-23) Cart + Accept, focus lands somewhere real; `b` closes; `LB` still opens VON

**Play-test C — dedicated server, two clients — CLOSED OUT, NOT DONE. None of this was run.**
- [x] C1 (24) Client 2 next to an open 500-item box sees no traffic
- [x] C2 (25) Live re-pull within ~250 ms; cart reconciles
- [x] C3 (26) JIP client sees counts + names immediately, no contents traffic
- [x] C4 (27) Simultaneous batches on one holder — correct or Busy; total exact
- [x] C5 (28) Disconnect mid-transfer — job aborts, nothing duplicated, count consistent

---

## Bug-report candidates (do **not** file — fixed in-flight)

- `OVT_RealEstateManagerComponent.RplLoad:968` inserts warehouse records without clearing → re-stream duplicates. Fixed incidentally in Phase 7.
- `OVT_LootIntoVehicleAction:257-319` hard-coded English string. Fixed in Phase 8.
- `OVT_StorageOperationConfig`'s constructor arguments contradict its own documentation (`OVT_InventoryManagerComponent.c:72`). **Not fixed** — the callers go away in Phase 8.

---

*Update this file as tasks are completed. Progress line is the count of `[x]` phase tasks over 76 (the 21 user-verification and Workbench rows below are tracked separately).*

---

## Post-close change 2026-08-23 — battlefield loot -> the ledger

User call after close. See `context.md` "Post-close change 2026-08-23" for the full record.

- [x] PC1 `OVT_StorageLootQuery.FilterLootables` — loose items (`InventoryItemComponent`, no parent slot) accepted; body test narrowed to `ChimeraCharacter`; **holders excluded outright** (a loot run deletes what it prices)
- [x] PC2 `StepLoot` rewritten onto the ledger; `LootBody` / `ExtractContents` / `MoveIntoHolder` deleted
- [x] PC3 `CollectLootTree` — all-or-nothing tree pricing, `EntityID` de-dupe, explicit weapon magazine/attachment walk, part-used magazines discarded as shortfall, worn base garments skipped but emptied
- [x] PC4 `JobWritesSourceLedger` no longer excludes LOOT; `StartLootJob` gates on a ledger, not an inventory manager
- [x] PC5 `OVT_TEST_Init_StorageSeam_ILootQueryTakesItemsNotHolders` added and **fail-proven** (removed the holder exclusion → red with the intended message)
- [x] PC6 Stale rationale corrected on `..._HTransportTrucksKeepVanillaCargoCaps` (loot no longer fills the vanilla bed; withdrawals still do)
- [x] PC7 Gate: `compile-check.sh` exit 0 (6325) · Init **221/221** · Logic **304/304** · RoundTrip **45/45**
- [ ] PC8 Play-test: loot a real body pile next to a truck; watch the progress bar, the ledger count and a 25 m town sweep
- [ ] PC9 Decide the fate of the dead legacy loot path in `OVT_InventoryManagerComponent` (~150 lines, zero callers)

---

## Post-close change 2026-08-23 (b) — "Sell Cargo Here" -> the ledger

User call after close. See `context.md` "Post-close change 2026-08-23 (b)" for the full record.

- [x] PD1 `RpcAsk_SellVehicleCargo` resolves `OVT_StorageUtils.GetStorage(vehicle)`; the `GetVehicleCargoStorage` gate and the `CollectCargoItems` scan are gone
- [x] PD2 `ExecuteSellLedger` — the ledger sell routine: same pricing-resource resolution, eligibility, per-unit marginal pricing, town absorption cap and restock as `ExecuteSell`
- [x] PD3 Shared tail extracted — `SettleSale()` (money + restock + `m_OnPlayerSell` + `m_OnPlayerTransaction`) and `ResolveShopTownId()`, both called by **both** routines so the two cannot drift
- [x] PD4 `storage.PublishCount()` after a successful sale, so the count and the action label follow it on every client
- [x] PD5 `OVT_SellVehicleCargoAction.VehicleHasCargo` reads `GetTotalCount() > 0` — the same ledger the server sells out of
- [x] PD6 Class/method doc comments corrected where the change made them false (RPC validation order, action header, TTL rationale)
- [x] PD7 `.st` audit: no help or Field Manual entry describes what the trunk sale enumerates, so **no re-export is owed**
- [x] PD8 Gate: `compile-check.sh` exit 0 (6340 files)
- [ ] PD9 🔴 Suite sweep — **never ran**, the harness refused `tools/run-tests.sh`
- [ ] PD10 Play-test: park a stocked truck at a general shop, sell, check money, the action label, the shop restock and the town cap over a bulk dump
- [ ] PD11 `OVT_SellableItemScanner.CollectCargoItems` / `GetVehicleCargoStorage` now have zero callers — folds into PC9's deletion question

---

## Post-close change 2026-08-23 (c) — container defects in the sweep and the take

User report after close. See `context.md` "Post-close change 2026-08-23 (c)" for the full record.

- [x] PE1 `EjectToHolderStorage` — a part-used magazine is moved out of its container before it is skipped, so the container is no longer stranded by `ItemStillHoldsSomething`
- [x] PE2 `ResolveHolderStorage` + `StorageIsNested` — `StepToInventory` spawns into the holder's own un-nested storage, never into a container stored inside it; no null fallback when the holder owns a storage (a full holder shortfalls instead)
- [x] PE3 Gate: `compile-check.sh` exit 0 (6341 files). 🔴 No suite ran — the harness still refuses `tools/run-tests.sh`; folds into PD9
- [ ] PE4 Play-test: bag holding a half magazine on a truck → Transfer all to storage leaves only the clip; take a bag plus other items out of storage → nothing nests inside the bag

---

## Post-close change 2026-08-23 (d) — looting is a crime if you are seen

User call after close. See `context.md` "Post-close change 2026-08-23 (d)".

- [x] PF1 `OVT_PlayerWantedComponent.GetIllegalActionReason()` — so a caller can only close its OWN illegal-action window
- [x] PF2 `ArmLootIllegalWindow` / `ClearLootIllegalWindow` / `ResolvePlayerWanted` on `OVT_StorageRequestComponent`; armed at `StartLootJob`, **re-armed at every LOOT chunk**, closed at FINISH and ABORT
- [x] PF3 `#OVT-Msg-WantedLooting` `{6A8E2F1000000002}` (GUID verified repo-unique; braces 2439 → 2441)
- [x] PF4 Gate: `compile-check.sh` exit 0 (6341 files)
- [ ] PF5 🔴 `.st` re-export owed — the new key renders raw until then (joins the `OVT-Transfer_NoSpace` debt)
- [ ] PF6 Play-test: loot with a patrol watching → wanted 2 + "You were seen looting the dead!"; loot unobserved → nothing; walk away after a run → no lingering window

---

## Post-close change 2026-08-23 (e) — base warehouses

User call after close. See `context.md` "Post-close change 2026-08-23 (e)".

- [x] PG1 `OVT_RealEstateManagerComponent.GetBaseAt` / `IsBaseWarehouse` / `BaseWarehouseIsControlled`
- [x] PG2 `PlayerMayUseWarehouse` short-circuits on base property **before** the record lookup (an unbought warehouse has no `OVT_WarehouseData`)
- [x] PG3 `RpcAsk_BuyBuilding` and `RpcAsk_RentBuilding` refuse base property
- [x] PG4 `OVT_RealEstateContext` — Buy / Rent / Set as home disabled, "Part of the base" shown, both client handlers refuse with the reason
- [x] PG5 `#OVT-RealEstate_BaseProperty` `{6A8E2F1000000003}` (GUID repo-unique; braces 2443 → 2445)
- [x] PG6 Gate: `compile-check.sh` exit 0 (6342 files)
- [ ] PG7 🔴 `.st` re-export owed (joins PF5 and the `OVT-Transfer_NoSpace` debt)
- [ ] PG8 Play-test: occupier-held base → no warehouse actions, real estate refuses; after capture → actions appear with no purchase

---

## Post-close change 2026-08-24 (f) — recruitment tent supply crate

User call after close. See `context.md` "Post-close change 2026-08-24 (f)".

- [x] PH1 `OVT_SupplyCrate_Tent.et` `{6A8E2F2100000001}` — storage-only prop: Open Storage / Load / Unload, no vanilla inventory, no RplComponent (placeholder mesh, user reskins in Workbench)
- [x] PH2 `OVT_RecruitmentTent.et` — `OVT_StorageComponent { UNLIMITED }` on the ROOT (persists free via the `OVT_BuildableComponent` rule) + the crate slotted as a child
- [x] PH3 `OVT_WarehouseStockUtils.PrependStore()`; the tent's store goes in front of the warehouses in the equipped-recruit quote
- [x] PH4 `OVT_StorageContainerQuery.FilterContainers` accepts ledger-only holders — also fixes built warehouses losing their stock to a FOB undeploy
- [x] PH5 Gate: `compile-check.sh` exit 0 (6342 files); GUIDs verified repo-unique
- [ ] PH6 Workbench: open `OVT_SupplyCrate_Tent.et` and `OVT_RecruitmentTent.et`, place the crate where it should sit, reskin
- [ ] PH7 Play-test: stock the crate → buy an equipped recruit → crate drains before any warehouse; FOB undeploy with a stocked tent → contents reach the truck

---

## Post-close change 2026-08-24 (g) — apply loadout -> the ledger

User call after close. See `context.md` "Post-close change 2026-08-24 (g)".

- [x] PJ1 `ReserveFromLedger` / `RefundToLedger` / `CreditToLedger` on `OVT_LoadoutManagerComponent`; a **null ledger is the spawning path** and reserves nothing
- [x] PJ2 `ApplyLoadoutToEntityFromBox` resolves `OVT_StorageComponent` + ledger from the box, refuses without one, and `PublishCount()`s once per apply
- [x] PJ3 Ledger gate threaded through `ApplyLoadoutItem`, `ApplyEquippedItem`, `ApplyWeaponAttachments` and both `ApplyNestedItemsSpawn*` — nested contents and attachments are debited too (BUG-042)
- [x] PJ4 Deleted the entity-search family: `FindItemInBox`, `FindItemInContainer`, `EmptyContainerIntoBox`, `EmptyUniversalStorageIntoBox`, `ApplyEquippedItemFromBox`, `ApplyWeaponAttachmentsFromBox`, `ApplyNestedItems`/`ToUniversalStorage`/`ToStorageManager`; dropped the vestigial `boxStorageManager` param on `ApplyQuickSlots`
- [x] PJ5 `WeaponHasAttachment` reused to skip already-mounted attachments instead of debit-then-refund
- [x] PJ6 Ledger + the five storage actions (Open Storage, Transfer all, Rename, Load, Unload) on the three Load Loadout hosts that had none (cabinet, FIA equipment box, FIA medical box) — the `OVT_AmmoBox_Base` set minus the vanilla-inventory ones; all appended to the EXISTING `ActionsManagerComponent`; GUIDs verified repo-unique and 16 hex digits
- [x] PJ7 Gate: `compile-check.sh` exit 0 (6342 files); 2390 → 1998 lines
- [ ] PJ8 Play-test conservation: apply one loadout twice from one box; check nested contents and optics are debited; check displaced gear returns to the ledger

---

## Post-close change 2026-08-24 (g) — slot-declared parts are not ledger lines

User report after close. See `context.md` "Post-close change 2026-08-24 (g)".

- [x] PG1 `OVT_PrefabPartUtils.c` — `IsDeclaredPart` / `CollectAttachedParts` / cached `GetDeclaredParts`, keyed on the HOLDER's prefab so a player-mounted attachment is never absorbed
- [x] PG2 `ConvertItemToLedger` + `CollectLootTree` refuse to credit a declared part
- [x] PG3 `CollectLootTree` + `QueueStoredContents` walk into declared parts, so a pouch's magazines are still priced and queued ahead of the vest
- [x] PG4 `StripWeapon` leaves a declared scope mounted; `ItemStillHoldsSomething` ignores declared attachments but blocks on a declared part that still holds something
- [x] PG5 `OVT_TEST_Init_StorageSeam_JDeclaredPartsAreDetected` — 3 prefab-read assertions + a real spawn, because an inert guard is otherwise silent
- [x] PG6 Gate: `compile-check.sh` exit 0 (6343 files)
- [ ] PG7 🔴 Run the Init case — the `GetParentContainer()` assumption is unproven at runtime
- [ ] PG8 Play-test: loot a Soviet-harness soldier → one vest line, no pouch lines; withdraw → pouches attached. Loot a `Rifle_SVD_PSO` → one rifle line, no optic line
- [ ] PG9 Pre-existing ledgers still hold orphan part lines; no migration was written
