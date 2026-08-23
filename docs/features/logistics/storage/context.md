# Storage (logistics/storage) — Context & Decisions

**Last Updated:** 2026-08-23
**Current Phase:** ✅ **CLOSED 2026-08-21** — three post-close changes 2026-08-23 (loot → ledger; trunk sale → ledger; two container defects, §§ below)
**Was:** ✅ **CLOSED 2026-08-21** — 10 phases, a cross-phase review, a B5 fix pass and 9 user play-test fixes
**Status:** ✅ **Closed** — user play-test signed off 2026-08-21 ("everything looks great now"). Residuals below.

---

## Quick Status

**What's Done:**
- ✅ Requirements formalized 2026-08-20 (5 decisions, none re-opened)
- ✅ Implementation plan written 2026-08-21 — 10 phases, 12 technical decisions (D1–D12), 13 risks
- ✅ Feature scaffolded (`tasks.md`, this file) 2026-08-21
- ✅ **Phase 1** — `OVT_StorageLedger` + `OVT_StorageRules` + 11 Logic cases; `OVT_TEST_LogicSuite` **214/214** (was 203)
- ✅ **Phase 2** — `OVT_StorageComponent` + `OVT_StorageUtils` + four prefab deltas + `m_aDisabledResourceTypes { 0 }` + 5 Init cases; `compile-check.sh` exit 0 (6216 files); `OVT_TEST_InitSuite` **159/160** (the one red is pre-existing — see Suite baselines).
- ✅ **Phase 3** — `OVT_StorageComponentSerializer` + `OVT_PersistedStorageLine` + three `Overthrow.conf` bindings + `EnsureTracked()` + 3 Persistence cases; `compile-check.sh` exit 0 (6217 files). Suite run owed to the orchestrator.
- ✅ **Phase 4** — `OVT_StorageRequestComponent` (13 RPCs, verbs 1/7/8 live, 2–6 declared), the controller prefab entry, 10 `.st` refusal keys and the D11 seam case; `compile-check.sh` exit 0 (6218 files). Audit table below. Suite run owed to the orchestrator.
- ✅ **Phase 5** — `OVT_StorageJob` + the VALIDATE/RUN/STEP/FINISH/ABORT engine, all six ops, verbs 2–6 wired, the sweep enumeration rules, `OVT_StorageRules.TransferLedgerLine` + 2 Logic cases and 8 `.st` keys; `compile-check.sh` exit 0 (6219 files). Suite run owed to the orchestrator.

- ✅ **Phase 7** — the warehouse world deleted and repointed: `OVT_WarehouseData` reduced to id/location/owner/isPrivate,
  ten warehouse methods and four warehouse RPCs gone, `OVT_RealEstateManagerSerializer` at version 2 with a v1 stock
  migration, the shared `PlayerMayUseWarehouse` rule (I5, `isRented` hole closed), `OVT_WarehouseContext` deleted, the
  warehouse building made an action host, and one Persistence case. `compile-check.sh` exit 0 (6224 files).

- ✅ **Phase 8** — the five shipped flows rerouted: port import is a ledger credit with zero spawns,
  Load/Unload are storage-only whole-ledger moves, truck Loot runs the LOOT job, FOB undeploy runs a new
  COLLECT job, and both transport trucks got vanilla's cargo caps back. `compile-check.sh` exit 0 (6225
  files); 2 Init cases added.

- ✅ **Phase 6** — `OVT_StorageContext` on the closed eight-hook surface (**I1 clean**), the `Character_Player.et`
  block, four user actions + the sort renumbering, the rename dialog and 10 `.st` keys. `check-input-conflicts.py`
  exit 0 at the shipped baseline.
- ✅ **Phase 9** — Port Export: a second mode on `OVT_PortContext` over the occupied vehicle's snapshot, a public
  price accessor onto the *same* body the server bills with, and a 16,800-combination Logic case proving the
  export price sits under every shop buy price. The import path's behaviour is unchanged.
- ✅ **Phase 10** — localization audit (all 57 runtime keys resolve; braces balanced at every step; no
  `Configs/Language/*.conf` written), both conflict-checker modes green, and the in-game help sync: two new Field
  Manual entries (Storage, Ports), a new `storageFirstOpen` tutorial popup, and `OVT-Tutorial_HomeFirstOpen_Body`
  corrected where this feature made it false.
- ✅ **Cross-phase review + B5 fix pass** — 4 defects fixed (see `tasks.md`), all six inter-phase hand-offs verified
  as landed, full DoD audit recorded.

**What's Next:**
- ⏸️ The user's localization re-export (10.2)
- ⏸️ Workbench checks W1/W2, then play-tests A (mouse), B (gamepad-only) and C (dedicated + JIP) — 21 rows in `tasks.md`
- ⏸️ The wiki pass, when a `wikijs` MCP server is attached

**Residuals at close (none blocking):**
- ⏸️ **One more localization re-export, for exactly one key.** The user re-exported twice (12:34 and 13:35);
  the 13:35 pass picked up everything including `OVT-Transfer_DestinationLabel`. **`OVT-Transfer_NoSpace`** — the
  cart-exceeds-capacity refusal added at ~14:00 — is the only key missing from `Language/*.conf`, and it renders
  **raw** until the next export. Verified by grepping the exports, not assumed.
- ⏸️ **Wiki sync** — no `wikijs` MCP server was ever attached. A 7-item hand-off is at the bottom of this file.
- ⏸️ **Multiplayer is entirely unproven at runtime.** No dedicated-server session was run. F12, JIP, concurrent
  batches on one holder and disconnect-mid-transfer are proven by reading only. See `tasks.md` Play-test C.
- ⏸️ **The v1 warehouse-save migration has never met a real pre-feature save.** The Persistence case covers the
  queue and delivery; loading an actual old campaign (§6 step 18) was not done.
- 💳 `OVT_AmmoBox_Cache` / `_Dev` carry the storage actions with no persistence binding — a Transfer-all into an
  arms cache is silently lost on reload. Design-level, recorded in `tasks.md`.
- 💳 `main` still carries the `OVT_Component` null-world crash fixed here as P8.

---

## Post-close change 2026-08-23 — battlefield loot lands in the LEDGER

User call, made after close: "shift all our Loot functions to use storage now … remove all the filters
… the truck gets every last item apart from their base clothes (pants, shirt, but still grab what's
stored in them)", and it must use the controller's progress system and spread the work over time.

**The progress half was already done.** `StartLootJob` has run LOOT on the job engine since Phase 8 —
chunked over the call queue at `m_iItemsPerChunk` / `m_iChunkDelayMs`, reported on
`OVT_BaseServerProgressComponent`'s bar under `#OVT-Progress-LootingBattlefield`, aborting on
disconnect. Nothing there changed. What changed is the DESTINATION and the FILTERS.

**What changed**

1. **`StepLoot` writes the ledger, not the vanilla inventory.** `LootBody` / `ExtractContents` /
   `MoveIntoHolder` are gone, and with them the last `TryInsertItem` in the loot path. A truck's bed
   volume no longer decides how much of a battlefield fits.
2. **`CollectLootTree` replaces them — one pending entity is one ALL-OR-NOTHING tree.** It prices the
   whole tree into a list of prefab names *without moving, detaching or deleting anything*, the caller
   checks `FreeSpace` once, then deletes the root once (`DeleteEntityAndChildren`) and credits every
   line. The sweep's per-item delete-then-credit could not be reused: half of what loot collects is
   lying on the ground, and **the ground has no inventory manager to delete an item through**. Recorded
   as the fourth ordering rule in the job engine's header block.
   - It de-dupes by `EntityID`. `InventoryStorageManagerComponent.GetItems` is a native proto and the
     shipped code was already unsure whether it recurses (`CollectSweepItems` tolerates duplicates
     because a repeat visit resolves to a deleted entity). Here a repeat visit would **credit twice**,
     so the `seen` list is load-bearing, not defensive.
   - Weapon magazines and attachments are walked explicitly: a `WeaponAttachmentsStorageComponent` is
     not a universal storage and would otherwise be destroyed with the weapon.
3. **The query is widened to every loose item** (`OVT_StorageLootQuery.FilterLootables`) — anything with
   an `InventoryItemComponent` and **no parent slot**. The parent-slot test is what stops an item that
   is still in a body/container/vehicle being priced twice: once by the query, once by its owner's tree.
4. **🔴 The body test is now `ChimeraCharacter`, not "destroyed damage manager", and holders are excluded
   outright.** A loot run DELETES what it prices. The old filter accepted *anything* whose damage
   manager reported destroyed — which a **ruined Overthrow building** and a wrecked truck both do. It
   only ever survived because `StepLoot` skipped what had no `InventoryStorageManagerComponent`; the
   new tree walk has no such accident protecting it. Guarded by
   `OVT_TEST_Init_StorageSeam_ILootQueryTakesItemsNotHolders`, fail-proven.
   - **Behaviour narrowed on purpose:** a destroyed *vehicle* is no longer looted-and-deleted by a loot
     run. It is a holder; it is emptied through the transfer screen.
5. **Part-used magazines are DESTROYED, not left behind** (user's call, "delete them"). A ledger line is
   a count with nowhere to record "27 of 30". They go with the tree and are tallied as **shortfall**, so
   the completion report still accounts for them.
6. **Base clothing unchanged — jacket + pants + boots** (`OVT_StorageRules.IsBaseClothingArea`, already
   Logic-tested). A garment **worn on a body** contributes no line of its own but its pockets are still
   emptied; a garment **lying on the ground** is ordinary loot, because it is then the tree root. That
   `isRoot` distinction is the whole rule.
7. **`JobWritesSourceLedger` no longer excludes LOOT**, so a loot run republishes the holder's count.
   Missing this would have left every client reading the pre-loot number until something else bumped it.
8. `StartLootJob` now gates on the holder having a **ledger** rather than an inventory manager.

**Gate:** `compile-check.sh` exit 0 (6325 files) · `OVT_TEST_InitSuite` **221/221** ·
`OVT_TEST_LogicSuite` **304/304** · `OVT_TEST_PersistenceRoundTripSuite` **45/45**. The one Init red
recorded at close (`CompositionSlotGate_AcceptedTypesMatchTheCompositions`) is no longer failing.

**Owed:** a play-test. Nothing here has been run with a real corpse in front of a real truck, and two
things can only be judged there — how much a widened 25 m sweep actually hoovers up in a town, and
whether deleting a *player's* dead body (the query accepts any destroyed character) collides with
`OVT_PersistenceReservation`. The pre-change code deleted bodies too, so this is not a new risk, but it
is an unproven one.

**Left alone, still dead:** `OVT_InventoryManagerComponent.LootBattlefieldIntoVehicle` /
`StartBattlefieldLooting` / `ProcessBattlefieldLoot` / `LootBodyItems` / `ExtractItemsFromClothing`
(~150 lines, zero callers since Phase 8, filters by class-name string). A deletion candidate.

---

## Post-close change 2026-08-23 (b) — Sell Cargo Here sells the LEDGER

User call: *"the 'sell all' action from vehicles is still using base game inventory, needs to sell
from storage now"*. `OVT_SellVehicleCargoAction` was the last shipped flow still reading vanilla
cargo entities — Phase 8 rerouted Load/Unload/Loot/import but left the trunk sale alone.

**What changed**

1. **`RpcAsk_SellVehicleCargo` sells out of `OVT_StorageComponent`'s ledger.** The
   `GetVehicleCargoStorage` null-gate and the `CollectCargoItems` scan are gone; the RPC resolves
   `OVT_StorageUtils.GetStorage(vehicle)` and calls a new `ExecuteSellLedger`. Every gate ahead of it
   (range, shop buys, lock/ownership, driver seat) is unchanged.
2. **`ExecuteSellLedger` is a second routine, not a branch inside `ExecuteSell`.** The two disagree
   about what a *unit* is: `ExecuteSell` prices entities and destroys them one `TryDeleteItem` at a
   time, so it needs the delete-then-count discipline and the `HasStoredContents` container guard. A
   ledger line is `(prefab, count)` with no entity to delete and no container to walk — `ledger.Take`
   is the commit and cannot partially fail, so both guards have nothing to act on. Folding them
   together would have meant a routine where half the body is inapplicable on either path.
3. **Everything that decides MONEY is shared and identical.** Same `ResolvePricingResource` → id
   (R7), same `ResourceIsAccepted`, same per-unit marginal pricing down the scarcity curve
   (BUG-117), same `CanTownAbsorbStock` cap, same restock, same `m_OnPlayerSell` /
   `m_OnPlayerTransaction`. A truckload sold out of a ledger earns exactly what the same items earn
   out of a trunk. The shared tail is now `SettleSale()` and the town lookup is now
   `ResolveShopTownId()`, both called by *both* routines so they cannot drift.
4. **The ledger loop still prices unit by unit.** It looks like it could sell a line in one step, but
   the price *falls* with each unit already sold this call, so a per-line price would overpay a bulk
   dump. Both break conditions (the town cap refusing a unit, the price reaching zero) end the line —
   past that point every further unit of that id gives the same answer.
5. **`storage.PublishCount()` after a successful sale**, so every client's action label and count
   follow the sale. Missing this leaves the pre-sale number on screen until something else bumps it.
6. **The action's `VehicleHasCargo` reads `GetTotalCount() > 0`** instead of scanning entities — the
   same ledger the server sells out of, so the offer and the sale agree.

**Behaviour narrowed, deliberately:** items lying loose in the vanilla cargo bed are **no longer
sold** by this action. They are moved into storage through the transfer screen (Transfer all to
storage) first. This is the same narrowing the loot change made and follows from the same rule — the
bed is where withdrawals land, the ledger is what the mod owns.

**No `.st` change, so no re-export owed.** Grepped: no Field Manual or tutorial entry describes what
the trunk sale enumerates, only `#OVT-SellCargoHere` (the label) and the three result hints, all
still accurate.

**Gate:** `compile-check.sh` exit 0 (6340 files). 🔴 **No suite ran** — the harness refused
`tools/run-tests.sh` (auto-mode classifier), the same block `resource-production` hit. Nothing here
has automated coverage either way: the sell path has never had a test above `OVT_ShopSellRules`'
pure-rule cases, and `ExecuteSellLedger` needs an economy manager, so it is not Logic-tier testable.

**Owed:** a play-test — park a stocked truck at a general shop, sell, and check the money, the
action label, the shop's restocked stock and the town's absorption cap over a bulk dump.

**Now dead:** `OVT_SellableItemScanner.CollectCargoItems` and `GetVehicleCargoStorage` have zero
callers (verified by grep). Left in place — the scanner is still live for the character path — but
they join PC9's deletion question.

---

## Post-close change 2026-08-23 (c) — two container defects in the sweep and the take

User report: *"when doing 'move all items to storage' if there are half clips in a container such as
a bag etc the whole container gets left, not just the clips"* and *"when taking stuff out of storage
into an inventory if im taking a container such as a bag, it will put the other items into that
container"*. Both are in `OVT_StorageRequestComponent.c`; neither touches the ledger or the wire.

**1. One half-magazine stranded its whole container.** `ConvertItemToLedger` skips a part-used
magazine (a ledger line is a count, so "27 of 30" has nowhere to go), and `ItemStillHoldsSomething`
then skips the bag *because the skipped magazine is still in it* — the guard that stops a delete
cascading over uncredited contents cannot tell "failed" from "deliberately left". Fixed by
**ejecting** the magazine first: `EjectToHolderStorage` moves it out of the container and into one of
the holder's own storages, so the container converts and only the clips are left loose. The sweep
queues contents ahead of their container, so by the time the container is examined every eject has
already run. A failed eject leaves the old behaviour exactly as it was — the container is skipped.

**2. A withdrawn bag swallowed everything withdrawn after it.** `StepToInventory` called
`TrySpawnPrefabToStorage(res, null, …)`, and the engine's own words for a null storage are *"most
suitable storage would be chosen from owned storages"* — which includes the storage inside a bag that
landed in the bed one item earlier. Fixed with `ResolveHolderStorage`: the first of the holder's
**un-nested** storages that will take the item, and **no null fallback** when the holder has one —
a full bed is FULL, not an invitation to nest. Verified against the vanilla layout: the truck's
`SCR_UniversalInventoryStorageComponent` sits on the vehicle's own root entity
(`Ural4320.et:594`), so it is never classified as nested.

**`StorageIsNested`** is the shared predicate: a storage is nested when it, or its owner entity's
`InventoryItemComponent`, has a parent slot. Both are checked because a container item's item
component and its storage component may or may not be the same object.

**Gate:** `compile-check.sh` exit 0 (6341 files). No suite run — see PE3.

**Owed:** a play-test. Two things can only be judged in front of a real truck: whether the eject
target the engine picks is somewhere sensible (it is "first storage that will take it", not "nearest
the bag"), and whether the removed null fallback ever refuses a withdrawal that used to succeed on a
holder whose only storage this predicate classifies as nested.

---

## What this feature is

One **item ledger** (prefab `ResourceName` → count) on one **`OVT_StorageComponent`**, authored on three shared
prefab bases so it reaches every wheeled vehicle, ammo box and warehouse building. Every holder is addressed by
**`RplId`** — there is no `warehouseId` vocabulary and no "is this the warehouse?" branch. Contents never leave the
server except to the one player who opened the holder; only **count + name + capacity** replicate.

Read `implementation.md` §3 before writing anything. §5 (D1–D12) is the decision record and is not re-opened here —
this file records what was *learned while building*, not what was decided while planning.

---

## Key Files

### Created by this feature
- `Scripts/Game/Data/OVT_StorageLedger.c` — the pure ledger (Phase 1)
- `Scripts/Game/Data/OVT_StorageRules.c` — five pure statics (Phase 1)
- `Scripts/Game/Components/OVT_StorageComponent.c` — the holder component (Phase 2)
- `Scripts/Game/Utilities/OVT_StorageUtils.c` — per-call radius query (Phase 2)
- `Scripts/Game/Components/Controller/OVT_StorageRequestComponent.c` — 13 RPCs (Phase 4) + the job engine (Phase 5)
- `Scripts/Game/Components/Controller/OVT_StorageJob.c` — the per-player job record (Phase 5)
- `Scripts/Game/UI/Context/OVT_StorageContext.c` — the `logistics/ui` consumer (Phase 6)
- `Scripts/Game/Persistence/Serializers/Components/OVT_StorageComponentSerializer.c` — `OVT_PersistedStorageLine` + the serializer (Phase 3)

### Consumed, must not be modified (I1 / Q7)
- `Scripts/Game/UI/Context/OVT_TransferContext.c`, `Scripts/Game/Data/OVT_TransferListModel.c`, `…/OVT_TransferCartModel.c` — `logistics/ui`'s base; the eight-hook list is **closed** (D11)
- `Scripts/Game/GameMode/Managers/OVT_InventoryManagerComponent.c` — call sites go away in Phase 8; the class file itself is not edited
- Any `core/damage` file — `OVT_StructureDamage.IsUsable` is a seam only

### Rewritten
- `OVT_RealEstateManagerComponent` / `OVT_RealEstateRequestComponent` / `OVT_RealEstateManagerSerializer` (Phase 7)
- `OVT_VehicleRequestComponent`, `OVT_LootIntoVehicleAction`, `OVT_LoadStorageAction`, `OVT_ContainerTransferComponent`, `OVT_ResistanceFactionManager` undeploy chain (Phase 8)
- `OVT_PortContext` (Phase 9)

### Deleted
- `OVT_WarehouseContext.c` + its `Character_Player.et` block; `OVT_WarehouseData.inventory` / `.isLinked`; ten warehouse methods and four warehouse RPCs

---

## Standing constraints for every phase

1. **`tools/run-tests.sh` is the orchestrator's.** Agents gate on `tools/compile-check.sh` exit 0 and nothing else.
2. **Never write `Configs/Language/*.conf`** — Workbench-generated exports. Edit `Language/localization_Overthrow.st` only, and count braces before and after (an unbalanced `.st` is eaten on the next Workbench save).
3. **`Rpc()` arity is a compile blind spot** (BUG-090) — the audit table below is the only check that exists.
4. **Save contexts key on the local variable's name** — a renamed local in Deserialize silently reads zeros and reports success.
5. **`array.Remove()` is swap-with-last** — use `RemoveOrdered` wherever order is observable.
6. **Comments sparse** (`CLAUDE.md`) — a line or two for a trap or load-bearing ordering. Reasoning belongs in `implementation.md`.
7. **Concurrent sessions share this tree** — re-baseline before every phase; every plan citation carries a file:line so drift is detectable.

---

## Fail-proof record (Q2)

**⚠️ Derived, not observed in-engine.** The no-run rule (`.claude/test-policy.md` §3) forbids an agent running
`run-tests.sh`, so the Phase 1 agent transliterated both production classes and all eleven case bodies into a
throwaway simulator, verified every case passes against the faithful implementation, then applied each §7 mutation
and captured the first `SetFailure` that fires. The message strings are the real ones from the source; the `%1`
substitutions come from the simulation. The orchestrator's suite run confirms the cases **pass**; it does not
re-confirm they can fail. Every mutation fails on a *different* assertion from every other case, so none is a
coincidental catch.

| Case | Mutation | Message |
|---|---|---|
| `LedgerAddClampsToCapacity` | drop the `capacity >= 0` clamp in `Add` | `Add(80) with 60 of room fitted 80, expected 60` |
| `LedgerAddUnlimited` | treat `-1` as a literal cap | `Add(5000) at capacity -1 fitted 0, expected all 5000` |
| `LedgerTakeClampsAndDropsLine` | clamp-and-keep: `Set(res, 0)` instead of `Remove(res)` | `After draining a line LineCount() is 2, expected 1 - a drained line must be removed, not kept at zero` |
| `LedgerTotalIsMaintained` | stale field: `Take` no longer decrements `m_iTotal` | `Total() is 20 after taking 4, expected 16` |
| `LedgerFreeSpace` | drop the `Math.Max(0, …)` | `FreeSpace(50) with 120 held is -70, expected 0 - free space is never negative` |
| `LedgerIgnoresGarbage` | remove the empty-key / `qty <= 0` guards | `Add() with an empty key fitted 5, expected 0` |
| `RulesAutoCapacity` | swap the truck and car branches | `A registered legal truck resolved to 300, expected -1 (unlimited)` |
| `RulesMagazineFull` | `>=` instead of `==` | `A 31/30 magazine is reported full - an over-count is corrupt, not full` |
| `RulesBaseClothing` | add `LoadoutVestArea` to the set | `LoadoutVestArea is treated as base clothing - a vest is lootable gear` |
| `RulesExportPrice` | drop the min/ceiling clamp | `Export capped by a 40 shop buy price is 50, expected 39 - it must be strictly under` |
| `RulesHolderInRange` | `<` instead of `<=` | `A holder at the player's own position is out of range at radius 0 - the test must be inclusive` |
| `MoveReturnsRemainder` | drop `source.Add(res, remainder, -1)` in `TransferLedgerLine` | `After a move that only half fitted the source holds 0, expected 18 - the un-added remainder must go BACK to the source` |
| `MoveClampsToMembership` | `dest.Add(res, qty, cap)` instead of `dest.Add(res, taken, cap)` | `The destination gained 10 from a source holding 3 - a move must never mint` |

---

## RPC arity audit (Q4 / R1)

All thirteen RPCs on `Scripts/Game/Components/Controller/OVT_StorageRequestComponent.c`, hand-diffed
2026-08-21 (Phase 4). Every `Rpc()` call site sits **immediately before** the handler it targets and
appears **exactly once** — 13 `Rpc(` calls, 13 `[RplRpc]` handlers, one-to-one, in the same order.
Nothing is wrapped in a generic variadic helper: the five `Send*` methods each contain exactly one
`Rpc()` naming its own handler, which is the shipped `OVT_GMRequestComponent.SendSnapshotBegin`
shape. Call-site args exclude the leading handler reference.

| # | Verb | `Rpc()` call site args | Handler signature | Match |
|---|---|---|---|---|
| 1 | `RpcAsk_OpenStorage` | `holder, m_iContentsSeq` (2: RplId, int) | `(RplId holder, int seq)` (2) | ✅ |
| 2 | `RpcAsk_BatchBegin` | `source, dest, opKind, m_iBatchSeq, lineCount` (5: RplId, RplId, int, int, int) | `(RplId source, RplId dest, int opKind, int seq, int lineCount)` (5) | ✅ |
| 3 | `RpcAsk_BatchLine` | `seq, index, res, qty` (4: int, int, string, int) | `(int seq, int index, string res, int qty)` (4) | ✅ |
| 4 | `RpcAsk_BatchCommit` | `seq, lineCount` (2: int, int) | `(int seq, int lineCount)` (2) | ✅ |
| 5 | `RpcAsk_TransferAllToStorage` | `holder` (1: RplId) | `(RplId holder)` (1) | ✅ |
| 6 | `RpcAsk_MoveAllToHolder` | `source, dest, sweepFirst` (3: RplId, RplId, bool) | `(RplId source, RplId dest, bool sweepFirst)` (3) | ✅ |
| 7 | `RpcAsk_ClearVanillaInventory` | `holder` (1: RplId) | `(RplId holder)` (1) | ✅ |
| 8 | `RpcAsk_RenameHolder` | `holder, name` (2: RplId, string) | `(RplId holder, string name)` (2) | ✅ |
| 9 | `RpcDo_ContentsBegin` | `holder, seq, lineCount, wireVersion` (4: RplId, int, int, int) | `(RplId holder, int seq, int lineCount, int wireVersion)` (4) | ✅ |
| 10 | `RpcDo_ContentsLine` | `seq, res, qty` (3: int, string, int) | `(int seq, string res, int qty)` (3) | ✅ |
| 11 | `RpcDo_ContentsEnd` | `seq` (1: int) | `(int seq)` (1) | ✅ |
| 12 | `RpcDo_StorageError` | `seq, messageKey` (2: int, string) | `(int seq, string messageKey)` (2) | ✅ |
| 13 | `RpcDo_BatchResult` | `seq, moved, shortfall, earned` (4: int, int, int, int) | `(int seq, int moved, int shortfall, int earned)` (4) | ✅ |

Every RPC also has a **direct-call** twin — `RpcAsk_*` behind `Replication.IsServer()` on the client
verbs, `RpcDo_*` behind `IsLocalPlayerOwner()` on the owner replies. Those twins ARE compile-checked
(they are ordinary method calls), so an arity drift between a call site and its twin is caught by
`compile-check.sh`; only the `Rpc()` half above is invisible to it.

Re-run the mechanical half of the audit with:

```
grep -n "^\t\t*Rpc(" Scripts/Game/Components/Controller/OVT_StorageRequestComponent.c
grep -n "RplRpc" -A 1 Scripts/Game/Components/Controller/OVT_StorageRequestComponent.c | grep "protected void Rpc"
```

Both must print 13 lines, pairwise in the same order.

---

## Suite baselines for this feature

Run suites **by class name**, not by group. Established 2026-08-21 during Phase 2:

| Suite | Baseline | Note |
|---|---|---|
| `OVT_TEST_LogicSuite` | **216/216** green, ~9 s | 203 before Phase 1 → 214 (Phase 1) → 216 (Phase 5's two `TransferLedgerLine` cases) |
| `OVT_TEST_InitSuite` | **162/163**, ~55 s (159/160 at Phase 2 → +1 seam case Phase 4 → +2 cases Phase 8) | ⚠️ one **pre-existing** red — `OVT_TEST_Init_CompositionSlotGate_AcceptedTypesMatchTheCompositions` ("config 'Base Fortifications' builds compositions but authors no `OVT_CompositionSlotConditionDeploymentModule`"). A deployment-config authoring defect owned by `occupying`/`virtualization`, not by storage — nothing in this feature touches deployment configs. **Any Init run showing a second failure is this feature's.** |
| `OVT_TEST_PersistenceRoundTripSuite` | **34/34** green, ~20 s (33 at Phase 3, +1 warehouse-migration case in Phase 7) | baselined 2026-08-21 after Phase 3; includes the three new storage round-trip cases (box ledger+name, vehicle ledger, warehouse ledger with explicit Track). `ReapplyEntitySaveData` on a **vehicle** had no shipped precedent before this — it works. |

| `OVT_TEST_CampaignSuite` | **16/16** green, ~31 s | baselined 2026-08-21 after Phase 7 |
| `OVT_TEST_PersistenceSuite` | **13/13** green, ~15 s | baselined 2026-08-21 after Phase 7 |

Together these five are the **All group's** contents, so running all five by class name is an equivalent gate.

⚠️ **The suites are NOT fully deterministic under load.** On 2026-08-21 a back-to-back five-suite sequence
produced `OVT_TEST_InitSuite` **6 of 163** in 102 s; an isolated re-run of the same commit produced the usual
**1 of 163** in 54 s. The five extra failures were `VirtualMovement_StationaryPlanIsNeverAdvanced`,
`CompositionSlotGate_FreeSlotScanIsExhaustive` and four `Virtualization_*` cases — **all** of them
`virtualization`/`occupying` cases that wait on world time or on a spawn settling, and the same family as the
case that used to hang the Fast group. **Zero storage cases failed in either run.** Practical rule: run the
Init suite **on its own**, not as the tail of a five-suite sequence, and treat a burst of `Virtualization_*` /
`VirtualMovement_*` reds after a long sequence as a load artefact until an isolated run confirms it.
(Note also that this suite's junit attaches `<failure>` elements one case out of step with the `name`
attributes — trust `run-tests.sh`'s stdout for the failing case name, not the junit pairing.)

An **exit 2 (INDETERMINATE)** from either script is `resourceDatabase.rdb` contention, not a verdict — it happened once
on `OVT_TEST_PersistenceRoundTripSuite` after Phase 8 and went green on an immediate retry with no code change. Never
report an exit 2 as a pass or a fail; re-run it.

**The Init suite does *not* hang standalone** (160 cases, 53 s) — the `VirtualMovement_StationaryPlanIsNeverAdvanced`
hang recorded after `logistics/ui` was a **group**-run symptom. Class-name runs are therefore the working gate; the
Fast/All *groups* remain unused for this feature.

---

## Serializer rename proof (R2)

**What was renamed.** In `OVT_StorageComponentSerializer.Deserialize()`, the local `customName` — the
one `Serialize()` writes under that exact spelling — was renamed to `readCustomName` (declaration,
`context.Read()` argument and the `ApplyPersisted()` argument, all three), leaving the writer's
`customName` untouched. Reverted immediately afterwards from a pristine copy; `grep` confirms only the
matched spelling is in the tree.

**What it produced: absolutely nothing.** `tools/compile-check.sh` → **exit 0, 6217 files, zero
diagnostics** — byte-identical to the run before the rename and to the run after the revert. There is
no compile-time link between the two methods at all: the property key is derived at runtime from the
variable's own debug name, so the entire static gate this project has is blind to the fault. That is
the finding: **the rename is not caught anywhere an agent can look.**

**What it would have produced at runtime**, from the two measurements already on the tree (not
re-measured here — the no-run rule forbids an agent running the suites, and the binary contexts are
native, so there is no honest way to simulate them):

- A renamed **simple-typed** local (`string`, `int`, `vector`) finds no such property, leaves the
  destination at its zero value and **returns success** — the 2026-08-20 `occupying/objectives`
  measurement, where a version-2 reader with `read`-prefixed locals restored every field as zero and
  took its "nothing was saved" branch.
- A renamed **array-of-records** local returns **false** and reads nothing — the 2026-08-09
  measurement written verbatim in `OVT_JobManagerSerializer.DeserializeVersion2` (`:410-412`).

**The consequence specific to this serializer, and why the `Read()` check is not the whole answer.**
Only the second of those is caught by checking `Read()`: renaming `lines` trips
`AbortUnreadablePayload` and the holder is left alone. Renaming `customName` does **not** — the read
succeeds, `customName` is `""`, and `ApplyPersisted("", lines)` writes an empty name over the live
one. Every box, truck and warehouse a player renamed would silently lose its name on the next
continue while its ledger came back perfectly. Identical spelling **by construction** is therefore the
primary defence and the `Read()` check is the backstop, not the other way round.

**Cheap review test.** `grep -n "context.Write(\|context.Read(" Scripts/Game/Persistence/Serializers/Components/OVT_StorageComponentSerializer.c`
— the argument spellings must pair up exactly.

---

## Gotchas & Learnings

### Phase 1

**`IsBaseClothingArea(null)` does not compile.** `error: Cannot convert 'Class' to 'typename' for argument '0'` —
the null branch is exercised through an uninitialised `typename unset;` local instead.

**`Clear()` deliberately does not fire `m_OnChanged`.** There is no per-line event to fire and firing once with an
empty key would be a lie. Consequence for Phase 2: **`PublishCount()` must be called explicitly after a `Clear()`.**

**Test registration is the attribute, not a list.** `[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]` is the whole
registration — `OVT_TEST_Logic_TransferModels.c` is referenced by no config or suite file, so there is nothing to edit.

### Phase 2

**`Warehouse_01_Workshop.et` does NOT inherit `Warehouse_01_Base.et`.** It inherits `Building_Base.et`
`{A43A100E3C377DB2}` directly, so the same-GUID delta does not reach it — while the real-estate filter
`"Warehouse_01"` matches on the FULL prefab path (`GetConfig`, `OVT_RealEstateManagerComponent.c:834-841`)
and therefore *does* classify it as a warehouse. A Workshop placed on an official world would be a
buyable warehouse with no ledger. The plan's "all seven variants" count is satisfied (base + 6 children);
the Workshop is the eighth file in that folder and is outside it. **Closing it needs a second same-GUID
delta**, whose GUID is not resolvable from the extracted tree (nothing under `Prefabs/` references it) and
must be read in the Workbench.

**`OVT_PrefabUtils.GetItemUIInfo` cannot name any storage holder.** It scans for an
`InventoryItemComponent`, and a vehicle, an ammo box and a building all have none — so §3.4's display-name
chain would have fallen straight through to `m_sDefaultNameKey`/the prefab stem for every holder. The chain
gained a step before it: `SCR_EditableEntityComponent.GetInfo()` on the live entity, which is where vehicles
(`SCR_EditableVehicleComponent.m_UIInfo`) and placed boxes keep their names.

**`isVehicle` must come from `Vehicle.Cast(owner)`, never from the economy.** `IsVehicle(ResourceName)`
routes through `GetInventoryId`, which resolves an unregistered prefab to id 0 — so an unknown truck would
answer "not a vehicle" and take `ResolveAutoCapacity`'s unlimited branch. The entity class is the only
trustworthy input.

**The mobile FOB is the first registered `PARKING_TRUCK` in the economy catalogue** (it is a `prefab`-keyed
entry in `vehiclePrices.conf`, which `BuildResourceDatabase` processes before any faction catalogue) **and**
the one wheeled prefab that overrides the mode to UNLIMITED. A truck case that picks its subject from the
catalogue therefore asserts the override instead of the AUTO branch unless it skips it — `OVT_TEST_Init_StorageSeam`
skips it by path fragment and additionally asserts `GetCapacityMode()`.

**`ApplyPersisted` adds at UNLIMITED capacity, deliberately.** Capacity is not persisted (D8) and is not
necessarily resolved when a save is applied, so clamping on load would silently eat stock after any prefab
or price retune. Over-cap on load is the strictly better failure.

### Phase 4

**`RplId` has never been passed through a `ScriptInvoker` anywhere.** Not in this project and not in
the extracted vanilla tree — `grep` for `Invoke(...rplId` returns nothing in either. `RplId` is a
`sealed class` with a private constructor, i.e. an engine handle, and `ScriptInvoker.Invoke`'s
variadic parameters are untyped. `m_OnContentsUpdated` therefore fires with **no payload** and the
consumer reads `GetSnapshot().m_HolderId`, which is also the shipped `OVT_GMRequestComponent` shape
(`m_OnSnapshotUpdated.Invoke()`, consumer reads the store). The two invokers that *do* carry payloads
carry `string` and `int`, both of which the base progress component already puts through invokers.

**⚠ ON A LISTEN HOST THE WHOLE CONTENTS FAN ARRIVES INSIDE `RequestOpenStorage()`.** The ask is
invoked directly rather than sent, so `RpcDo_ContentsBegin`, every line, `RpcDo_ContentsEnd` and the
`m_OnContentsUpdated` invoke all run before the method returns. §3.8 has `OVT_StorageContext` firing
the pull from **inside `BuildEntries`** and re-entering `Refresh()` when the fan lands — on a host
that is `BuildEntries` re-entering itself synchronously. **Phase 6 must set its per-(holder, seq)
latch BEFORE calling `RequestOpenStorage`, not after**; the latch is what terminates the recursion.
The seam is deliberately left synchronous because that is exactly what the shipped
`OVT_GMRequestComponent.RequestSnapshot` does, and diverging here would have been a private
invention in the one place a consumer cannot see it.

**The two sequence spaces are disjoint by PARITY, not merely independent.** Contents sequences are
even and start at 2; checkout sequences are odd and start at 1; `SEQ_NONE` is 0. The plan fixes
`RpcDo_StorageError` at arity 2 `(int seq, string messageKey)`, so it is the one reply both spaces
share and it has no room for a space discriminator — parity is how it tells a refused pull from a
refused checkout without one. Two independent counters alone would not have been enough: seq 3 could
have been live in both spaces at once.

**Plan steps 4 and 5 of `MayUseHolder` are ONE call.** `OVT_ControllerRequestComponent.PlayerMayUseVehicleFor`
works on **any** entity — a locked vehicle and a locked crate answer to the same rule, and an entity
with no `OVT_PlayerOwnerComponent` passes it — so the "vehicle" branch and the "`OVT_PlayerOwnerComponent`
present" branch collapse exactly as `OVT_ContainerTransferComponent.CallerMayReach` already collapses
them. Two calls would have been two copies of one rule.

**The warehouse gate is strictly stricter than what ships today, and still carries a hole.**
`OVT_RealEstateRequestComponent.ValidateWarehouseRequest` (`:533-557`) checks quantity, warehouse-id
range and `IsRegisteredResource` and **nothing about who is asking** — any client can take from any
warehouse today. `MayUseHolder` adds the shipped client-button expression. That expression's third
clause is a bare `|| isRented`, and `OVT_OwnerManagerComponent.IsRented(EntityID)` means "rented by
*anybody*" — so a warehouse rented by one player is open to every player. Carried verbatim because
I5 requires the button and the gate to be one rule; **Phase 7 lifts it to the real-estate manager and
that is the place to fix it**, not here, where fixing it would silently diverge from the button.

**`RpcAsk_BatchLine` and `RpcAsk_BatchCommit` are the only handlers that answer nothing, deliberately.**
A checkout is refused once, at `Begin`. The client streams Begin, every line and Commit back to back
before any reply can arrive, so answering per line would send the player up to 66 refusals for one
order. Phase 5 must keep that property: the refusal belongs to the checkout, not to the line.

### Phase 5

**`TryDeleteItem` CASCADES INTO A CONTAINER'S CONTENTS, and the plan does not mention it.** Proven at
runtime during BUG-083 and written into `OVT_ShopTransactionComponent.HasStoredContents` (`:713`).
Without a guard, "Transfer all to storage" on a truck holding a full backpack would have deleted the
pack, taken the bandages inside it with it, and credited **one** ledger line. The sweep therefore
does two things the plan's §3.7 does not list: it queues a container's contents **ahead of** the
container (`QueueStoredContents`), and it **skips** any item that still holds something when its turn
comes (`ItemStillHoldsSomething`). Both orders of `GetItems` are then safe — either the contents
convert first and the container follows in the same pass, or the container is left for the next
"Transfer all", which is the same under-convert-rather-than-destroy direction the shop path took.

**A weapon's attachment storage is NOT a universal storage,** so the shop's shipped container test
cannot see a loaded magazine or a mounted optic. `ItemStillHoldsSomething` needs its explicit weapon
half (`GetCurrentMagazine` + `GetAttachments`) or a rifle whose strip failed would be deleted with its
optic inside it. That is also why the strip is explicit rather than falling out of the generic
container rule.

**`OVT_WorldUtils.GetNearbyBodiesAndWeapons` is the SAME defect as
`OVT_InventoryManagerComponent:497`** — its accumulator is a `protected static ref array<IEntity>`
(`OVT_WorldUtils.c:601`), so two players looting at once fill each other's lists. It is not reused;
`OVT_StorageLootQuery` in `OVT_StorageUtils.c` is a per-call re-declaration (B2).

**A vanished work item is a SILENT SKIP, not a shortfall.** That is what makes a duplicate id in the
work list harmless — the second visit finds a deleted entity — and a duplicate is exactly what
happens when `GetItems` already returned a container's contents that `QueueStoredContents` then
queued again. Counting them would have inflated every sweep's shortfall by the size of every pack.

**`Processed()` counts the CURSOR for the entity ops, not moved + shortfall.** A deliberately-skipped
work item (a part-used magazine, a container that still holds something) is neither, so the progress
bar would stall short of the end on any box holding one.

**`m_aPending` is `array<EntityID>`, not the plan's `array<IEntity>`.** A sweep deletes the entities
it enumerated and anything else may delete them between two chunks; an id that no longer resolves is
a skip, a dangling `IEntity` is a crash. This is the standing project rule (EntityID local, RplId
network) winning over a plan sketch.

**EXPORT's port and illegal gates were brought forward from Phase 9,** deliberately. `opKind` arrives
from the client, so an EXPORT that only Phase 9's *screen* gated would have been a one-packet money
faucet for a modified client. `AtAPort` uses port import's own 30 m on both ends and
`ResolveExportUnitPrice` carries import's illegal expression verbatim. Phase 9 still owns the screen,
the mode toggle and `IsAddAllAllowed`.

**The chained job is re-gated at FINISH.** `sweepFirst` can spend seconds inside the sweep, so the
`TO_HOLDER` job it chains to re-runs `CheckoutHoldersUsable` rather than trusting the permission and
distance check the original request made. Its lines are also built **then**, not at request time — the
sweep is precisely what changes them.

**`OVT_StorageRules` gained a sixth static.** `TransferLedgerLine` is the whole ledger-to-ledger move
(clamp to live membership → take → capped add → **remainder back to the source**), lifted out so the
one B1 ordering rule that is pure enough to assert without a world actually is asserted, by two Logic
cases. The plan's §3.3 lists five statics; this is an addition, not a change.

**⚠ ON A LISTEN HOST A WHOLE CHECKOUT RUNS INSIDE `OnAccept`.** Same synchronicity as Phase 4's
contents fan: `RequestBatchBegin`, every `RequestBatchLine` and `RequestBatchCommit` invoke their
handlers directly, so VALIDATE, `StartOperation` and — for `TO_HOLDER` and `EXPORT`, which are one
step — the entire move, `FinishJob`, `RpcDo_BatchResult` and the `m_OnBatchResult` invoke all run
before `OnAccept` returns. **Phase 6 must not assume the cart is still valid after `OnAccept`'s own
call to Accept**, and must not re-enter itself from `m_OnBatchResult`.

**A refusal answers at most once per checkout, and Commit is allowed to be the one.** Phase 4's rule
is preserved by construction: `Begin` nulls `m_Checkout` on every refusal, so the trailing lines and
commit find nothing and stay silent; a malformed line sets `m_bMalformed` instead of answering; and
`Commit` takes the checkout before anything can refuse, so a duplicate commit is silent too.

### Phase 6

**The latch is cleared BEFORE `RequestOpenStorage`, and that ordering is the whole termination
argument.** On a listen host the fan runs inside the ask, so `m_OnContentsUpdated` re-enters
`BuildEntries` from within itself; the second visit finds `m_bWantPull` already false and does
nothing. A second guard, `m_bBuildingEntries`, makes the invoker skip its `Refresh()` in that case -
without it the inner Refresh draws the list and the outer one immediately overdraws it from a model
the outer `BuildEntries` never got to fill.

**`ShowPersistentMessage` cannot be called from `BuildEntries`.** `RefreshCheckout()` runs at the end
of every `Refresh()` and clears a persistent message whenever the cart validates - and an empty cart
always validates. The loading message is therefore set in an **override of `Refresh()`**, after
`super.Refresh()` returns. No base change: `Refresh()` was already public and virtual.

**The batch result must NOT refresh inline.** On a listen host the whole checkout runs inside
`OnAccept`, so `m_OnBatchResult` fires while the base's `Accept()` is still mid-flight (it clears the
cart, refreshes and restores focus *after* `OnAccept` returns). The handler schedules the 250 ms
coalesced re-pull instead.

**Nearby destinations are sorted nearest-first, not query order.** `QueryEntitiesBySphere` order is
spatially arbitrary and can differ between two calls that found the same holders, which would
reshuffle the picker under the player's selection index between refreshes.

**"This container" is skipped when the holder has no vanilla inventory** - a warehouse building has
none, so the row would be a destination that can never receive anything.

**The warehouse building is NOT an action host yet.** §3.10 lists it for Storage and Rename, but the
delta has no `ActionsManagerComponent` and Phase 6's task list names only the ammo box and
`Vehicle_Base`. The recipe is unchanged (fresh `ActionsManagerComponent` + `UserActionContext`
`Radius 20`, `Garage_E_02.et` precedent); the warehouse is reached through
`OVT_VehicleMenuContext.TakeFromWarehouse()`, which Phase 7 retargets.

**The rename dialog reuses the `RENAME_RECRUIT` preset but overrides its title and message**
(`SetTitle` / `SetMessage`), because the preset's own strings say "recruit". No new preset, no
`.conf` edit.

### Phase 9

**Phases 1 and 5 had already built the whole EXPORT backend.** `RunExport`, `ResolveExportUnitPrice`,
`AtAPort`, the `CheckoutHoldersUsable` EXPORT branch, `#OVT-Progress-StorageExporting` and
`OVT_StorageRules.ExportUnitPrice` were all shipped and correct; the only backend addition this phase
made is a **public** `GetExportUnitPrice(player, pos, res)` wrapper so the screen prices its rows
through the same method the server bills with, rather than growing a second pricing expression.

**The client prices Export from replicated/config state only.** `GetPrice`, `GetBuyPrice`,
`IsSoldAtAnyNonVehicleShop` and `ResistanceControlsNearestPort` all read shop config, town/base
records and `m_aAllPorts`, none of which is server-only, and `m_fExportPriceRatio` is a prefab
attribute every client's own controller carries. The row price a player sees is therefore the price
the server will pay.

**A refused line is listed DISABLED, not hidden.** An unregistered or ungated-illegal item priced 0
stays on the list with `#OVT-Export_NotSellable`, so a truck carrying unsellable loot says so instead
of silently showing a shorter list. `FillDetails` leaves the body empty for a disabled row - the base
puts the reason there itself (`OVT_TransferContext:684`), so setting `#OVT-Export_Body` unconditionally
would have hidden every reason.

**`ValidateCart` is only ever called on a NON-EMPTY cart** (`RefreshCheckout:786`), so the port-range
check cannot nag a player who has selected nothing. The loading message still has to be set in the
`Refresh()` override after `super.Refresh()`, for Phase 6's reason.

**The mode toggle finally appears, and it costs no focus.** `Mode1Button`/`Mode2Button` were authored
in `TransferMenu.layout` from the start with `OverthrowTransferMode1/2` already in
`OverthrowTransferContext`; `RefreshHeader` was showing them only above one mode. They are
`WLib_NavigationButton`s in the header row, outside the list and cart panes, so `FocusIsInPanes()` is
false for them and `RestoreFocus()`/`FocusCurrentPane()` never target them - a pad reaches them by
pressing RB / View, which is what the glyphs on their chrome say.

**Import stayed byte-identical.** The only removed lines in `OVT_PortContext.c` are two header comment
lines, one comment line, and `modes.Insert(0)` becoming `modes.Insert(MODE_IMPORT)` where
`MODE_IMPORT == 0`.

### Phase 7

**The frozen v1 record could not simply LOSE its link flag, so the flag was RENAMED in place.** The
acceptance grep forbids the string `isLinked` anywhere under `*.c`, but `OVT_PersistedWarehouse` is the
class every existing save names in its `$type` discriminator and it has to keep reading. Whether the
binary container keys a record's members by NAME or by DECLARATION ORDER decides whether deleting a
field is safe - deleting is safe only under name keying, and `OVT_PersistedJobV2`'s header asserts order
keying. **Renaming is safe under both**: a bool of the same type in the same slot consumes the same
bytes under order keying, and under name keying it simply finds nothing and stays false, which is what
the migration wants anyway. The field is now `legacyLinkFlag` and its header says exactly this.

**`WarehouseHasStock` was on the REQUEST component, not the manager** (the phase brief lists it under
the manager). It went with `ValidateWarehouseRequest` and `RejectWarehouseRequest`.

**`OVT_RespawnService:379` needed no change at all.** The plan lists it as a caller; it reads
`OVT_RealEstateConfig.m_IsWarehouse`, which survives untouched, and never sees stock, `isLinked` or a
warehouse id. Left byte-identical.

**A resistance RENTAL had to be carved out of the `isRented` fix.** "Rented by this player" would
otherwise have locked every officer out of a warehouse rented on the resistance account
(`RpcAsk_RentBuilding(useResistanceFunds: true)` sets the renter to the literal `"resistance"`), which
is a collective, not a person. `GetRenterID(building) == "resistance"` opens it to everyone, mirroring
the shipped `RpcAsk_StopRentingBuilding` convention.

**An OWNER now loses access while somebody else is renting their warehouse.** That falls straight out
of the fix and is deliberate: an owner who wants exclusive occupancy rents their own building, which
the shipped rent path explicitly allows and charges nothing for.

**The client action mirror gained the warehouse branch too.** `OVT_StorageActionBase` calls itself a
mirror of `MayUseHolder`; with the warehouse building now an action host, a Storage action on a
warehouse nobody owns would have shown and then been refused. It refuses in `CanBePerformedScript` with
`#OVT-Storage_NoAccess` rather than hiding in `CanBeShownScript`, so the player gets a reason and the
cost is paid only when the action is a candidate.

**The vehicle menu's 40 m button window is wider than the server's 30 m `m_fUseRadius`.** The buttons
appear at up to 40 m from the warehouse RECORD (unchanged, I3) while `MayUseHolder` refuses beyond 30 m
from the building. The record location IS the building origin and Warehouse_01 is ~40 m long, so
parking anywhere at the building puts you ~20 m from the origin; the seam only opens 10-20 m outside the
building, and it answers `#OVT-Storage_TooFar` rather than failing silently. Play-test item, alongside
the `Radius 20` tuning §6 step 14 already lists.

**Opening a warehouse from the vehicle menu has exactly ONE destination.** A building has no vanilla
inventory, so Phase 6's "This container" row is skipped, and the only destination is the vehicle - which
has to be inside `m_fHolderRadius` (25 m) of the building. Park close.

### Phase 3

**The persisted record is NOT `OVT_StorageLine`.** The binary container writes the concrete class name
into every save as a `$type` discriminator and creates the instance from that name on load, so
whichever class is written is frozen — name and field order — for the life of every save file. Binding
that freeze to `OVT_StorageLine`, the live record Phases 4–9 still have to shape for the wire fan and
the UI, would have been a trap with a silent, total-loss failure mode. `OVT_PersistedStorageLine
{ string prefab; int count; }` is a dedicated frozen record and the serializer maps between the two;
the mapping loop is four lines each way and is the one place a shape change is forced to be noticed.

**`ApplyPersisted()` now routes through `PublishCount()`** instead of duplicating its three lines.
Same behaviour, one BumpMe, and it makes "`PublishCount()` after any load that changes the total"
structurally true rather than a convention two call sites have to remember — `Clear()` fires no change
event, so a load that rebuilt the ledger without republishing would leave every client's action label
reading the pre-load number.

**Tracking is gated on `Building.Cast(owner)`, not on `!IsTracked()`.** An unconditional track would
also register bare `OVT_AmmoBox_Base` / `_Cache` instances — which base compositions place in numbers
and which match vanilla's `StorageHolder.conf`, i.e. a config this serializer is deliberately not
bound to. The vanilla `Persistence` component on `Building_Base.et` carries `Flags 0 0x1` and vanilla
registers a building only from `GoToDestroyedState` ("as they are otherwise not tracked by default"),
whereas `Vehicle_Base.et` carries a bare `Persistence { }` and the placed box is tracked by
`PlaceItem()` — so vehicles and boxes latch out on the class test and never reach the lookup.

**A placed ammo box has no native `Persistence` component**, so a Persistence-tier case cannot spawn
one with `SpawnEntityPrefab` — it would never be tracked and the reload would fail for a reason that
has nothing to do with storage. `OVT_ResistanceFactionManager.PlaceItem(index, 0, pos, vector.Zero, -1)`
is the path that tracks it, and `-1` waives the funds, distance and item-limit checks exactly as
`BuildItem(-1)` does for the `StructureDamage` cases.

**The three `.conf` bindings are appends to configs that already match, never a new rule.** The CAR
block in `Overthrow.conf` already restates all nine inherited vanilla `ComponentSerializers` — proof
that declaring that array in a same-GUID delta REPLACES it rather than merging — so a new entry has to
go at the end of the restated list. The new `{65B682661F79DDBE}` block is the opposite case:
`Building.conf` declares no `ComponentSerializers` at all, so there is nothing there to clobber.

### Phase 8

**THE PLAN NEVER MENTIONS FOB *DEPLOY*, AND UNDEPLOY MAKES IT A DATA-LOSS PATH.** Undeploy now leaves
everything in the mobile FOB's **ledger** (F10), and `DeployFOB` deletes that truck after moving only
its **vanilla inventory** (`transfer.TransferStorage`). Deploying a truck that had just been undeployed
would therefore have destroyed the entire stockpile. Closed with `OVT_StorageUtils.MoveWholeLedger`,
called synchronously in `DeployFOB` before the vanilla transfer. `OVT_VehicleManagerComponent.UpgradeVehicle`
is the same shape (spawn replacement → transfer → delete original) and got the same call.

**`OverthrowMobileFOBDeployed.et` was not in Phase 2's prefab list and resolved capacity 0.** It is not
in `vehiclePrices.conf`, so AUTO calls it an unregistered vehicle — which also meant one ERROR per
session naming the prefab. Given an explicit `UNLIMITED` override (inherited component GUID
`{6A8E2D0000000001}` re-declared) so the undeploy can collect its cargo instead of deleting it with the
entity. Init case G asserts it.

**The `m_NextJob` chain CANNOT carry a per-container undeploy.** Three separate blockers, all in
`FinishJob`: it fires `SendOperationComplete` per job (so the resistance manager's FOB cleanup would run
after the first container), it re-gates a chained job through `CheckoutHoldersUsable` (which returns
`#OVT-Storage_BadRequest` for `TO_STORAGE` and applies `MayUseHolder`'s 30 m rule to containers spread
over a 75 m footprint), and it drops the rest of the chain when a container turns out to be empty.
Hence a seventh op, `COLLECT`: one job, a container queue (`m_aHolders` / `m_iHolderCursor` /
`m_bHolderOpened` on `OVT_StorageJob`), per-**item** chunking and per-**container** progress.

**A holder spawned this frame reads capacity 0.** `OnPostInit` defers the resolve with
`CallLater(TryResolveCapacity, 0, false)` even for the non-AUTO modes, so the mobile FOB the undeploy
spawns is `NO_CAPACITY` for at least one call-queue hop. `MayUseHolder` would refuse it and
`FreeSpace(0)` would report a full ledger on an empty truck. `StartCollectionJob` therefore gates the
destination on `PlayerMayUseVehicleFor` rather than `MayUseHolder`, and `StepCollect` treats an
unresolved destination as unlimited. Resolving non-AUTO modes inside `OnPostInit` would be the real fix
but it would call `Replication.BumpMe()` before the entity is registered — not attempted.

**`StartCollectionJob` refusals are LOG-ONLY, on purpose.** The resistance manager latches FOB state
before calling and clears it from `m_OnOperationError`, so emitting a refusal there would fire a
completion callback for a job that never started — and the "engine is busy" refusal would additionally
clear the RUNNING job's `m_bIsRunning` (the hazard `RejectTransfer` guards with `if (!m_bIsRunning)`).
`UndeployFOB` unwinds by calling `OnFOBCollectionError` itself on a `false` return.

**Loot kept its wire and changed its work.** `OVT_ContainerTransferComponent.RpcAsk_LootBattlefield` is
still the client→server hop (identity, radius cap, `CallerMayReach` all unchanged); it now forwards to
`StartLootJob` instead of calling the inventory manager. That is what keeps the RPC count at 13 and the
`context.md` audit table valid — a fourteenth RPC on `OVT_StorageRequestComponent` would have invalidated it.

**`OVT_ContainerTransferComponent` is down to two verbs.** `TransferStorageForDeployment` and
`CollectContainers` were already caller-less (the 2026-08-14 audit flagged both as deletion candidates)
and went; `UndeployFOBWithCollection` / `RpcAsk_UndeployFOB` went because the manager's `UndeployFOB` is
server-side and can call `StartCollectionJob` directly. What is left is the FOB **deploy** transfer and
the loot wire — and the deploy transfer is the last `OVT_Global.GetInventory()` call site outside the
two the plan named (the vehicle upgrade and `OVT_StorageProgressUIContext`'s cancel).

**The undeploy container filter is the SHIPPED one, not `OVT_StorageHolderQuery`.**
`OVT_StorageContainerQuery` requires `UniversalInventoryStorageComponent` **plus**
`OVT_PlaceableComponent` or `OVT_BuildableComponent`, exactly like
`OVT_InventoryManagerComponent.IsCollectableContainer`. The holder filter would have swept a bystander's
parked car into the truck, because a legal car is a holder with capacity 300.

**`StepSweep`'s per-item body is now `ConvertItemToLedger`,** shared with `StepCollect`, so the
check-capacity → delete → credit ordering (B1) exists in exactly one place. Outcomes are four named
constants rather than a bool, because "full" has to stop the sweep and "failed" only costs a shortfall.

*Filled in as they are hit. Inherited traps already known from `logistics/ui` (see `docs/features/logistics/ui/context.md`):*
- `WLib_NavigationButton` is not focusable without an override
- the destination picker eats d-pad left/right (D6) — this feature is the **first** consumer with ≥ 2 destinations, so that guard is exercised for the first time
- `SCR_SpinBoxComponent.SetInitialState` reads `m_aElementNames.Count()` with no null guard — the empty-holder picker fill is likewise a first

---

## Open Questions

- [ ] **Q:** **`ExportUnitPrice`'s floor beats its ceiling at `minShopBuyPrice == 1`.** The ceiling is then 0 and the
      floor is 1, so the result *equals* the shop price rather than sitting under it. Unavoidable given "floored at 1".
      If the no-shop→port-loop guarantee must hold absolutely for $1 items, the rule needs a "not exportable" answer —
      a plan change, not an implementation one.
      **A:** _shipped as planned; asserted as-is in `RulesExportPrice`. Flagged to the user._

- [ ] **Q:** **§7's negative clothing list names `LoadoutHandwearArea`, which does not exist.** The real vanilla
      typename is `LoadoutHandwearSlotArea`. The test uses the real one. This also confirms the plan's claim about
      the shipped `LootBodyItems` filter: of its eight strings, `LoadoutArmoredVestSlotArea` and `LoadoutHandwearArea`
      are not vanilla classes — which is R10, and why Phase 8 moves to `typename` comparison.
      **A:** _corrected in code; no plan edit needed._

- [ ] **Q:** D8 — capacity is deliberately **not** persisted (a persisted copy would freeze a retuned prefab out of every existing campaign). The plan flags this for the user to overrule; adding two ints is a cheap version bump.
      **A:** _proceeding as planned unless the user says otherwise_

---

## Session Notes

### 2026-08-21 — Phase 8 complete

- **Port import** (`OVT_VehicleRequestComponent.RpcAsk_ImportToVehicle`): every gate and the money maths
  untouched; the `qty`-iteration `TrySpawnPrefabToStorage` loop replaced by one
  `ledger.Add(res, qty, storage.GetCapacity())` + `PublishCount()`, and the debit still pays for what
  fitted. The vanilla-inventory precondition became a storage-component precondition and now answers
  `ImportNotAvailable` instead of returning silently. `IsValidResourceId` (the registry gate D12 keeps
  for the one minting path) is unchanged.
- **Load / Unload**: both rewritten onto `OVT_StorageVehicleActionBase` (new file) over
  `RequestMoveAllToHolder`. Load = box → vehicle, `sweepFirst false`; Unload = vehicle → box,
  `sweepFirst true` (D6). Shipped gates preserved: 10 m sphere query, nearest within 15 m,
  driver-must-exit, the lock rule (now the `OVT_StorageActionBase` mirror), `#OVT-NoVehiclesNearby`,
  `#OVT-DriverMustExit`, `#OVT-StorageEmpty`, `#OVT-VehicleEmpty`. Both label values edited, no new keys.
- **Truck Loot**: `OVT_LootIntoVehicleAction` rewritten; both hard-coded English strings replaced
  (`#OVT-Storage_Busy`, `#OVT-Storage_NoCapacity`), the client-side body query moved off
  `OVT_WorldUtils.GetNearbyBodiesAndWeapons`'s static accumulator onto a per-call `OVT_StorageLootQuery`,
  and the work now runs as the LOOT job (typename clothing filter, R10).
- **FOB undeploy**: new `COLLECT` op + `StartCollectionJob` on `OVT_StorageRequestComponent`;
  `OVT_ResistanceFactionManager.UndeployFOB` calls it directly and its completion handlers moved to that
  component. `CleanupFOBArea` and the physics reactivation in `OnFOBCollectionComplete` are byte-identical.
- **FOB deploy and vehicle upgrade** gained `OVT_StorageUtils.MoveWholeLedger` — see the gotcha; without
  it undeploy→deploy destroyed the whole stockpile.
- **Truck caps**: `M923A1_transport.et` and `Ural4320_transport.et` to vanilla's own
  `MaxCumulativeVolume 1000000` and `m_fMaxWeight` 4500 / 5000.
- **Dead call sites removed**: `RpcAsk_TransferStorageForDeployment`, `RpcAsk_CollectContainers`,
  `RpcAsk_UndeployFOB` and `RpcAsk_LootBattlefield`'s body.
  `git diff --exit-code -- Scripts/Game/GameMode/Managers/OVT_InventoryManagerComponent.c` is clean (Q7).
- **2 Init cases** appended to `OVT_TEST_Init_StorageSeam.c` (G: the deployed FOB resolves UNLIMITED;
  H: both transport trucks meet vanilla's cargo floors).
- Gate: `compile-check.sh` exit 0 (6225 files). 13 `Rpc()` / 13 `[RplRpc]` — the audit table is unchanged.
  `.st` braces 1892 → 1892, two `Target_en_us` values and two `Comment`s edited, no new keys, no
  `Configs/Language/*.conf` written. **Not run:** `run-tests.sh` (orchestrator's).

### 2026-08-21 — Phase 7 complete

- **Deleted:** `OVT_WarehouseData.inventory` / `.isLinked`; `GetWarehouseInventory`, `AddToWarehouse`,
  `DoAddToWarehouse`, `TakeFromWarehouse`, `DoTakeFromWarehouse`, `TransferToWarehouse`,
  `TakeFromWarehouseToVehicle`, `RpcDo_SetWarehouseInventory`, `m_OnWarehouseInventoryChanged` on the
  manager; the three client verbs, three `RpcAsk_*`, `ValidateWarehouseRequest`, `WarehouseHasStock`,
  `RejectWarehouseRequest` and `VEHICLE_MAX_DISTANCE` on `OVT_RealEstateRequestComponent` (589 → 351);
  `TransferToWarehouse` + `RpcAsk_TransferToWarehouse` on `OVT_ContainerTransferComponent` (−61);
  `OVT_WarehouseContext.c` and its `Character_Player.et` block.
- **`OVT_RealEstateManagerComponent`** 1083 → 1019 lines: −268/+205. Gained `WAREHOUSE_MATCH_RANGE`,
  `PlayerMayUseWarehouse`, `QueueWarehouseMigration` / `DrainWarehouseMigration` /
  `DeliverWarehouseMigration`, and an `RplLoad` that CLEARS before inserting (the duplicate-on-re-stream
  bug the plan flagged at `:968`).
- **`OVT_RealEstateManagerSerializer`** at version 2. `OVT_PersistedWarehouse` frozen (link flag renamed
  in place, see the gotcha); new `OVT_PersistedWarehouseV2 { location, owner, isPrivate }`;
  `DeserializeVersion1` / `DeserializeVersion2` / `AbortUnreadablePayload`, every `Read()` checked.
  Locals `ownedRecords` / `rented` / `warehouses` are identical in the writer and both readers.
- **`Warehouse_01_Base.et`** gained the action host §3.10 wanted and Phase 6 deferred: a FRESH
  `ActionsManagerComponent "{6A8E2D0000000030}"` (nothing in the chain declares one - vanilla
  `Warehouse_01_Base.et` and `Building_Base.et` both checked) with a `UserActionContext` at `Radius 20`,
  carrying `OVT_OpenStorageMenuAction` (1) and `OVT_RenameStorageAction` (2). GUIDs `…0030..0036`.
  **F1 is now met.**
- 1 Persistence case, `OVT_TEST_PersistenceRoundTrip_WarehouseMigration_Version1StockLandsInTheBuilding`
  — drives the queue directly (a binary v1 payload cannot be produced from script), asserts the deferral,
  the delivery, the line values and the REPUBLISHED count.
- Gate: `compile-check.sh` exit 0 (6224 files). Acceptance grep clean; no `warehouseId` anywhere in the
  tree; `check-input-conflicts.py` exit 0 at the shipped baseline; `.st` untouched (braces 1892/1892
  unchanged); no `Configs/Language/*.conf` written. **Not run:** `run-tests.sh` (orchestrator's).

### 2026-08-21 — Phase 5 complete

- `OVT_StorageJob` (new file, `Scripts/Game/Components/Controller/`) + the VALIDATE / RUN / STEP /
  FINISH / ABORT engine on `OVT_StorageRequestComponent`. **All six ops**: `TO_HOLDER` and `EXPORT`
  are one synchronous step (map arithmetic bounded by `m_iMaxCartLines`), `TO_INVENTORY`,
  `TO_STORAGE`, `CLEAR` and `LOOT` are chunked at `m_iItemsPerChunk` every `m_iChunkDelayMs` with a
  re-armed one-shot `CallLater` rather than a repeat.
- Verbs 2–6 wired. `RpcAsk_BatchBegin` refuses everything line-independent, `RpcAsk_BatchLine`
  answers nothing and records `m_bMalformed`, `RpcAsk_BatchCommit` runs VALIDATE and may refuse once.
  `RpcAsk_ClearVanillaInventory` starts the `CLEAR` job at the line Phase 4 left for it.
  `StartLootJob(playerId, holder, radius)` is a **server-side, non-RPC** entry point for Phase 8.
- **The three orderings.** Sweep: `FreeSpace(capacity) <= 0` → stop, else `TryDeleteItem` → *then*
  `ledger.Add`. `TO_INVENTORY`: `TrySpawnPrefabToStorage(...)` → *then* `ledger.Take(res, 1)`.
  `TO_HOLDER`: `OVT_StorageRules.TransferLedgerLine` clamps, takes, adds capped, and adds the
  remainder **back to the source** at unlimited capacity.
- **R5 holds by construction:** `PublishCount()` is reached from exactly one method
  (`PublishTouchedHolders`), which is reached from `FinishJob` and `AbortJob` and never both, and
  publishes the source once and the destination once. `CLEAR` and `LOOT` write no ledger and publish
  nothing.
- **R9** is enforced twice: `ClampLinesToLedger` at VALIDATE (with a per-resource claimed-so-far map,
  so a cart naming one resource twice cannot inflate), and again per unit/line inside every step.
- 8 `.st` keys, `{6A8E2D200000000B..12}` — five `OVT-Progress-Storage*` captions and three refusals
  (`_BadRequest`, `_NotAtPort`, `_NothingToMove`). Braces 1856 → 1872, balanced both sides.
- 2 Logic cases appended to `OVT_TEST_Logic_StorageRules.c` over the new pure `TransferLedgerLine`;
  both assert conservation across the two ledgers.
- Gate: `compile-check.sh` exit 0 (6219 files). 13 `Rpc()` call sites / 13 `[RplRpc]` handlers,
  unchanged from Phase 4's audit; no `array<...>` on any RPC; no call into
  `OVT_InventoryManagerComponent` (four mentions, all comments); no ternaries; no `maxAttempts`; I2
  grep clean. **Not run:** `run-tests.sh` (orchestrator's).

### 2026-08-21 — Phase 4 complete

- `OVT_StorageRequestComponent : OVT_BaseServerProgressComponent` — 13 RPCs, `MayUseHolder`, two
  parity-disjoint sequence counters, a client-side `OVT_StorageSnapshot` store and the
  `EOVT_StorageOp` enum (append-only; the ordinal crosses the wire). Verbs **1, 7, 8** complete;
  **2–6** declared and refusing `#OVT-Storage_Busy`; `RpcAsk_ClearVanillaInventory` validates in full
  (including the officer gate) and refuses at the point Phase 5 replaces with a CLEAR job.
- `OVT_OverthrowController.et` — `OVT_StorageRequestComponent "{6A8E2D0000000004}"` appended before
  the trailing `RplComponent`.
- **10** `.st` refusal keys, `{6A8E2D2000000001..A}`, inserted in sort order before `OVT-StorageEmpty`
  (`_` sorts before letters in this file). Braces 1836 → 1856, balanced both sides (each entry adds
  two of each: the block and its GUID string).
- **1** Init case appended, `OVT_TEST_Init_StorageSeam_FRequestComponentResolves` (D11). The five
  existing cases A–E are byte-identical; the file was appended to, never rewritten.
- Gate: `compile-check.sh` exit 0 (6218 files). I2 grep clean; no `array<...>` on any RPC; no
  ternaries; no `maxAttempts`. **Not run:** `run-tests.sh` (orchestrator's).

### 2026-08-21 — Phase 3 complete

- `OVT_StorageComponentSerializer` + `OVT_PersistedStorageLine` (version 1: `version` → `customName`
  → one `array<ref OVT_PersistedStorageLine>`). Both `Read()` returns checked; both reads happen
  before anything is applied, so a fault can never half-apply a name without a ledger;
  `AbortUnreadablePayload()` logs at ERROR and consumes the payload with live state untouched.
  Capacity deliberately not written (D8).
- Three `Overthrow.conf` bindings, **no new rule** (`ComponentClassPersistenceConfigRule` count
  unchanged at 3): CAR `{64C6B4937723DA61}` append `{6B0E7A60C1D2E3F4}`; Placeable
  `{6B0E7A215A7FD39C}` append `{6B0E7A61D2E3F405}`; new Structures block for vanilla's Building
  `{65B682661F79DDBE}` restating `Collection "{65B4DD18C4F30AC9}"` and adding `{6B0E7A62E3F40516}`.
  Each GUID verified unused by `grep -rl` in both trees before authoring. HELICOPTER
  `{64EE8D74EB8192BA}` deliberately untouched.
- `OVT_StorageComponent.EnsureTracked()` — latched, server-only, `Building.Cast` gated, ask-first,
  never untracks; fired from `PublishCount()` when the total is non-zero and from `SetCustomName()`
  when the name is non-empty.
- **3** Persistence cases appended, one per binding, with different saved counts per case so a
  serializer reading one holder's record onto another is visible rather than passing three times over.
- Gate: `compile-check.sh` exit 0 (6217 files). I2 grep clean; no `maxAttempts`; no ternaries; no new
  persistence type name anywhere under `Scripts/Game/Tests/`. **Not run:** `run-tests.sh`
  (orchestrator's) — the Persistence suite has never been baselined and now has three more cases.

### 2026-08-21 — Phase 2 complete
- `OVT_StorageComponent` (3 `RplProp`s, deferred server-only AUTO resolve with a 10 x 1 s retry budget,
  `GetRpl()` assertion, `PublishCount()`, `ApplyPersisted`, memoised `GetDisplayName()`) +
  `OVT_StorageUtils` / `OVT_StorageHolderQuery` (accumulator on the instance, re-created per `Run()`).
- Prefab deltas: `Wheeled_Base.et` (AUTO, 300) · `OVT_AmmoBox_Base.et` (UNLIMITED) · **new same-GUID delta**
  `Warehouse_01_Base.et` `{E35EA41864A3B0ED}` (UNLIMITED, `#OVT-Warehouse`) · `OverthrowMobileFOB.et`
  (inherited component GUID re-declared, UNLIMITED) · `OVT_OverthrowGameMode.et` `m_aDisabledResourceTypes { 0 }`.
- Fresh GUIDs `{6A8E2D0000000001..3}`; the FOB re-declares `…0001` because inherited GUIDs are copied.
- **5** Init cases in `OVT_TEST_Init_StorageSeam.c` — the plan's four (truck / car / box / warehouse) plus
  an illegal-vehicle case, because "an armed/illegal wheeled vehicle resolves 0" is a Phase 2 acceptance
  criterion with no other automated cover; it also pins `OVT_StorageHolderQuery`'s `GetCapacity() != 0`
  clause. Every case additionally asserts `GetCapacityMode()`, so a dropped attribute cannot pass by
  giving the right number for the wrong reason.
- Gate: `compile-check.sh` exit 0 (6216 files). I2 grep clean. **Not run:** `run-tests.sh` (orchestrator's).

### 2026-08-21 — Phase 1 complete
- `OVT_StorageLedger` (map-backed, maintained `m_iTotal`, `Total()` O(1), drained lines `map.Remove`d) +
  `OVT_StorageRules` (5 statics) + 11 Logic cases across two suites.
- Gate: `compile-check.sh` exit 0 (6213 files) · `tools/run-tests.sh OVT_TEST_LogicSuite` **214/214, 10 s**.
  The Fast *group* is still unusable (hangs on `OVT_TEST_Init_VirtualMovement_StationaryPlanIsNeverAdvanced`),
  so single-suite-by-class-name is the gate for Logic-only phases.
- I2 grep clean; Logic tier purity grep clean; no `maxAttempts`.

### 2026-08-21 — scaffold
- `/autorun-feature` invoked with no argument; resolved to `logistics/storage` (only feature planned-but-not-started, and next in the epic's build order after `ui`).
- Plan read end-to-end; `implementation.md` status flipped to In Progress; `tasks.md` (75 phase tasks across 10 phases) and this file created.
- Next: Phase 1 via `component-developer`.


---

## Wiki hand-off (blocked — no `wikijs` MCP server in this session)

Search before creating; the paths below are the expected flat paths.

1. **`warehouses`** (search `warehouse`; may be folded into `real-estate`/`houses`) — rewrite "how stock gets in
   and out". Kill any mention of a warehouse-only screen or of a warehouse being linked to a vehicle
   (`isLinked` is deleted). New: the building carries **Storage** and **Rename storage** actions within 20 m;
   the vehicle menu's **Take from warehouse** / **Put in warehouse** still work, with Put now sweeping the
   vehicle's loose inventory into its ledger first. Access: unowned → nobody, owned public → everybody,
   owned private → owner only, rented → renter only (a `"resistance"` rental opens it to everybody).
2. **`ports`** (search `port`, `import`) — add Export, correct import. Import no longer spawns items; it credits
   the vehicle's ledger and charges only for what fitted (civilian car 300, trucks uncapped, illegal/armed
   vehicles and helicopters cannot trade at all). Player **and** vehicle within 30 m. Export: same screen,
   second mode, unit price = import price × 0.5 clamped to one dollar under the port-local buy price, so
   shop→port arbitrage does not pay; illegal goods need Trade L5 or a resistance-controlled port; unpriced
   items cannot be exported.
3. **New page `storage`** (check for an existing `inventory`/`cargo` page first) — player-language version of the
   new Field Manual Storage page. Link from `warehouses`, `ports` and any vehicles page. No class names, no
   `OVT_` prefixes, no GUIDs.
4. **`vehicles`** — remove any claim that vanilla supply Load/Unload/Store actions appear on truck beds or
   civilian cars; they no longer do. Truck vanilla inventory limits raised to vanilla's own values
   (M923A1 4500 kg, Ural 5000 kg, 1,000,000 volume).
5. **`fobs`** — undeploy now collects nearby containers' loose inventories **and** their ledgers, within 75 m.
6. **Looting** (search `loot`) — truck Loot now takes vests, backpacks and helmets as items in their own right;
   jackets, trousers and boots are still left on the body with their pockets emptied. Radius 25 m.
7. Screenshots needed for the new `storage` page and the port Export mode. The two new Field Manual entries use
   the generic `default_ui.edds` tile; dedicated tiles are an art task.

**MCP hazards when this is finally run:** search returns wrong pageIds; `update` needs `tags` and can report
failure while still writing; a failed update leaves the render stale — so **re-read every page after writing**.
