# Storage (logistics/storage) - Task Checklist

**Last Updated:** 2026-08-21
**Progress:** 79/80 tasks complete (99%) — **all 10 phases built and gated, plus a cross-phase review pass (2 findings fixed) and a B5 fix pass (2 more).** The only open task is **10.2, the Workbench localization re-export, which is the user's to run.** Final gate 2026-08-21: `OVT_TEST_LogicSuite` **217/217** · `OVT_TEST_InitSuite` **162/163** (the one red, `CompositionSlotGate_AcceptedTypesMatchTheCompositions`, is **pre-existing** and unrelated) · `OVT_TEST_CampaignSuite` **16/16** · `OVT_TEST_PersistenceSuite` **13/13** · `OVT_TEST_PersistenceRoundTripSuite` **34/34** · `check-input-conflicts.py` exit 0 plain **and** `--warnings` · `compile-check.sh` exit 0 (6225 files).

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
- [ ] 10.2 ⏸️ **Nearly done — one more re-export owed.** The user re-exported from Workbench on 2026-08-21 12:34 and `Language/*.conf` now carries every feature key (`OVT-Storage_*`, `OVT-Export*`, the Field Manual and tutorial keys). **`OVT-Transfer_DestinationLabel` was added after that export and is not in it yet** — see the play-test fix below.
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

---

## Verification owed by the user

**Workbench (user-gated):**
- [ ] W1 Open the new `Warehouse_01_Base.et` delta and one child variant (`Warehouse_01_Office.et`) — storage component + action context inherited
- [ ] W2 Open `Wheeled_Base.et`, `OVT_AmmoBox_Base.et`, `OVT_OverthrowController.et`, `Character_Player.et` with no dropped-attribute warnings

**Play-test A — single player, mouse** (implementation.md §6 steps 9–18):
- [ ] A1 (9) Port import 100 with no hitch and no entities in the truck; Open Storage shows 100
- [ ] A2 (10) Take 50 → This container → progress bar, 50 entities, ledger 50, label follows
- [ ] A3 (11) Take all → nearby box → instant, box 50 / truck 0
- [ ] A4 (12) Transfer all to storage — rifle + optic + full magazine become three lines, part-used magazine stays; the two "Open" actions are distinguishable
- [ ] A5 (13) Rename shows in action, picker and map; officer Clear removes the half-empty magazine
- [ ] A6 (14) Warehouse Storage action + both vehicle-menu warehouse buttons still work
- [ ] A7 (15) Loot then Unload Storage — no spike, base clothing left, everything lands in the box's ledger
- [ ] A8 (16) Export at a gated port — money arrives, ledger empties, unit price below shop
- [ ] A9 (17) No vanilla supply actions on truck bed / civilian car; arsenal + support station still function
- [ ] A10 (18) Save/Continue round-trip, then a **pre-feature** save's warehouse stock appears in the building

**Play-test B — gamepad only** (steps 19–23):
- [ ] B1 (19) Something focused on arrival; d-pad walks the list
- [ ] B2 (20) D6: picker left/right changes destination without moving the focus column
- [ ] B3 (21) Empty box with ≥ 2 destinations — first `ClearAll()`-on-empty picker fill does not error
- [ ] B4 (22-23) Cart + Accept, focus lands somewhere real; `b` closes; `LB` still opens VON

**Play-test C — dedicated server, two clients** (steps 24–28):
- [ ] C1 (24) Client 2 next to an open 500-item box sees no traffic
- [ ] C2 (25) Live re-pull within ~250 ms; cart reconciles
- [ ] C3 (26) JIP client sees counts + names immediately, no contents traffic
- [ ] C4 (27) Simultaneous batches on one holder — correct or Busy; total exact
- [ ] C5 (28) Disconnect mid-transfer — job aborts, nothing duplicated, count consistent

---

## Bug-report candidates (do **not** file — fixed in-flight)

- `OVT_RealEstateManagerComponent.RplLoad:968` inserts warehouse records without clearing → re-stream duplicates. Fixed incidentally in Phase 7.
- `OVT_LootIntoVehicleAction:257-319` hard-coded English string. Fixed in Phase 8.
- `OVT_StorageOperationConfig`'s constructor arguments contradict its own documentation (`OVT_InventoryManagerComponent.c:72`). **Not fixed** — the callers go away in Phase 8.

---

*Update this file as tasks are completed. Progress line is the count of `[x]` phase tasks over 76 (the 21 user-verification and Workbench rows below are tracked separately).*
