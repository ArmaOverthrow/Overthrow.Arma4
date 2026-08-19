# Storage — Implementation Plan

**Status:** Planning
**Started:** 2026-08-19
**Target Completion:** TBD
**Last Updated:** 2026-08-19 17:55 (initial plan; codebase survey verified against the tree at `2c07a624`)

**Epic:** `core` (feature #8 — see `docs/features/core/epic-overview.md`)
**Requirements:** `docs/features/core/storage/requirements.md` (authoritative for scope)
**Approach:** shared ledger class + per-entity component + one generalized UI; full scope in 7 ordered phases (user-approved 2026-08-19)
**Branch:** `v1.5` (concurrent sessions exist on this tree — re-baseline before every phase)

---

## 1. Executive Summary

Arma Reforger's inventory tracks **spawned entities with full state**. Overthrow only ever cares about **type + quantity** for stockpiles, so a full ammo box or a looted truck is a pile of hundreds of replicated entities that the engine must stream, simulate and serialize. That is the direct cause of the network spikes, the kicked clients and the save-time FPS collapse this feature exists to remove.

Overthrow already solved this once — a **warehouse** holds `map<string,int>` on the manager, not entities — but the solution is welded into `OVT_RealEstateManagerComponent` and cannot travel. This feature **extracts that idea into a reusable ledger** and puts it on ammo boxes, trucks and civilian cars, then converts every bulk-item flow in the mod (port import, warehouse take, box load/unload, FOB undeploy, truck loot) onto it.

Four things make it more than "a map on a component":

1. **A shared `OVT_StorageLedger` serves two very different owners.** Placed ammo boxes and vehicles are prefab instances and get a real `OVT_StorageComponent`. Warehouses are **static world buildings** — a prefab component cannot reach them — so the warehouse keeps its manager-side record and swaps its raw map for the same ledger. One data class, one request seam, one UI, two hosts.
2. **The two worst spike sites are deleted, not batched.** `OVT_RealEstateManagerComponent.TakeFromWarehouseToVehicle` (`:642`, up to `qty` `TrySpawnPrefabToStorage` calls in one frame) and `OVT_VehicleRequestComponent.RpcAsk_ImportToVehicle` (`:484-490`, up to 100 spawns in one frame) both become **pure ledger-to-ledger arithmetic with zero entity spawns**, because their destination is now storage rather than cargo.
3. **The remaining spawn/despawn work gets a fresh batcher on the per-player controller.** `OVT_InventoryManagerComponent` is not repaired — see D5. The new batcher extends `OVT_BaseServerProgressComponent`, so "one operation at a time" is scoped **per player**, not globally, and the existing progress HUD lights up for free.
4. **Data integrity is the quality bar, not throughput.** Every conversion commits **per item**: an entity is deleted *before* the ledger is credited, and the ledger is debited only *after* a spawn succeeds. A crash or a disconnect mid-transfer can therefore lose or duplicate at most one item, never a stack.

**Explicitly not in scope:** the `logistics` epic's resource ledger (see §3.7), volume/weight-based storage capacity, warehouse linking (`isLinked` is dead code and is deleted), and any change to how the player's own character inventory works.

---

## 2. Goals

### Primary

- **G1** A reusable, Logic-tier-testable `OVT_StorageLedger` (ResourceName → count, plus capacity) exists and is the **single** representation of pure-data item stock in the mod. The warehouse's raw `map<string,int>` is gone.
- **G2** `OVT_StorageComponent` is wired onto placed ammo boxes, trucks and civilian cars; offensive/illegal vehicles do not get usable storage. Capacity is a per-prefab attribute, `-1` = unlimited, and is **enforced server-side**.
- **G3** Items convert **both ways** between our storage and the vanilla inventory, batched across frames, with the existing progress bar, and **without item duplication or loss** across normal completion, capacity refusal, crash, and disconnect.
- **G4** "Open Storage (N items)" sits immediately after the vanilla "Open" action and opens a categorized, gamepad-navigable menu with Transfer 1 / 10 / All.
- **G5** "Transfer all to storage" converts a container's vanilla inventory into storage, **ignoring part-used magazines** and **stripping attachments and loaded magazines first** so they convert as separate lines.
- **G6** Port import and warehouse withdrawal go **straight to storage** — zero entity spawns on those paths.
- **G7** Ammo box Load/Unload operate on storage only (Unload converts the source first), and their labels say "Storage".
- **G8** FOB undeploy collects from **both** the vanilla inventory and the storage of nearby ammo boxes; spent magazines are deleted rather than hauled.
- **G9** The truck "Loot" action keeps only the corpse's **shirt, pants and boots** out of the haul, runs batched through the progress component, and the truck holds ~20-30 soldiers' worth of gear.
- **G10** An officer-only ammo box action empties the box's **vanilla** inventory (junk removal), never the storage.

### Secondary

- **G11** No unbatched entity spawn/despawn loop remains on any player-reachable path. No per-line RPC broadcast storm: bulk mutation replicates as **one replicated integer**, and detail is pulled on demand by the one client that has the menu open.
- **G12** Every dead path this feature supersedes is **deleted**, not left as attack surface: `OVT_StorageProgressUIContext`, three caller-less `OVT_InventoryManagerComponent` entry points, two caller-less `OVT_ContainerTransferComponent` RPCs, `OVT_WarehouseData.isLinked` + `GetWarehouseInventory`.
- **G13** The ~28-line lock-check block copy-pasted verbatim into three ammo box actions (and absent from a fourth) becomes one shared base class, and the fourth action gains it.
- **G14** The vanilla supplies "Storage" surface is hidden everywhere it is still reachable.
- **G15** Saves round-trip storage exactly: an ammo box, a truck and a warehouse all come back with the same lines and quantities after save → quit → Continue.

---

## 3. Architecture Overview

### 3.1 Where each piece lives

```
DATA (world-free, Logic-tier testable)
  Scripts/Game/Data/OVT_StorageLedger.c        ResourceName -> int, capacity, O(1) total, change invoker
  Scripts/Game/Data/OVT_StorageRules.c         pure predicates: IsFullMagazine, IsBaseClothing,
                                               CapacityAllows, category-for-resource

ENTITY (server authority + one replicated scalar)
  Scripts/Game/Components/OVT_StorageComponent.c
     [Attribute] m_iCapacity            -1 unlimited / 0 disabled / N item cap
     [RplProp]   m_iReplicatedCount     the ONLY per-mutation replication
     ref OVT_StorageLedger m_Ledger     full detail; authoritative copy on the server,
                                        client copy filled by an on-demand snapshot pull

MANAGER-SIDE HOST (warehouses are static buildings; no prefab component can reach them)
  OVT_RealEstateManagerComponent.m_aWarehouses[i].m_Ledger   <- replaces map<string,int> inventory

CLIENT -> SERVER SEAM (exactly one)
  Scripts/Game/Components/Controller/OVT_StorageRequestComponent.c
     : OVT_BaseServerProgressComponent      (progress plumbing; cannot also extend
                                             OVT_ControllerRequestComponent - no multiple
                                             inheritance - so it reuses the static rule bodies,
                                             exactly as OVT_ContainerTransferComponent documents)
     owns the batched conversion engine and the per-player one-operation-at-a-time latch

UI (one context, two sources)
  Scripts/Game/UI/Context/OVT_StorageContext.c
  Scripts/Game/UI/Menu/StorageMenu/OVT_StorageMenuTabComponent.c
  UI/Layouts/Menu/StorageMenu.layout  +  StorageMenu/StorageInventoryItem.layout

USER ACTIONS
  Scripts/Game/UserActions/OVT_BaseStorageUserAction.c   shared lock check + storage resolution
  OVT_OpenItemStorageAction / OVT_TransferAllToStorageAction / OVT_ClearVanillaInventoryAction
  (existing OVT_LoadStorageAction / OVT_UnloadStorageAction / OVT_LootIntoVehicleAction rebased)

PERSISTENCE
  Scripts/Game/Persistence/Serializers/Components/OVT_StorageComponentSerializer.c
  Configs/Systems/Persistence/Overthrow.conf   two new ComponentSerializers entries
  OVT_RealEstateManagerSerializer               version 1 -> 2
```

**No new manager.** Storage is per-entity, there is no registry to coordinate, and the one piece of shared state (the in-flight operation latch) is naturally per player on the controller component. Per the epic rule, `OVT_Global` gains **no** accessor; `OVT_StorageComponent` is reached with `OVT_ComponentFinder<OVT_StorageComponent>.Find(entity)` and `OVT_StorageRequestComponent` with `OVT_ControllerComponent<OVT_StorageRequestComponent>.Get()`.

### 3.2 The ledger

`OVT_StorageLedger : Managed` — pure data, no engine types, no world. Modelled on `OVT_FuelChargeLedger` (`Scripts/Game/Data/OVT_FuelChargeLedger.c`), which is the house precedent for a Logic-tier ledger.

| Member | Purpose |
|---|---|
| `protected ref map<string,int> m_mItems` | ResourceName string → quantity. Zero-quantity lines are removed, never kept. |
| `protected int m_iCapacity` | `-1` unlimited, `0` disabled, `N` maximum total item count. |
| `protected int m_iTotal` | Running total, maintained incrementally. **Must be O(1)** — `GetActionNameScript` polls it every frame. |
| `ref ScriptInvoker m_OnChanged` | `(string res, int newQty)`. Fires once per changed line, on whichever machine mutated. |

API (all pure; every one of these is a Logic-tier case):

```
int  Add(string res, int qty)     // returns how many ACTUALLY fitted (capacity-clamped, never negative)
int  Take(string res, int qty)    // returns how many were ACTUALLY taken (stock-clamped)
void Set(string res, int qty)     // absolute setter, for the replication/snapshot path only
int  GetCount(string res)
int  GetTotalCount()              // O(1)
int  GetFreeSpace()               // -1 when unlimited
bool IsFull()
int  Count() / string KeyAt(int) / int QtyAt(int)
void Clear()
```

**Serialization is deliberately NOT on the ledger.** `RplSave`/`RplLoad` and `SaveContext`/`LoadContext` loops live on the component and the manager, so the ledger stays world-free and Logic-tier reachable, and the two hosts keep independent wire formats. (`ScriptBitWriter` also cannot be exercised from a test at all — it hard-crashes on first use from script.)

### 3.3 Replication shape — one integer broadcast, detail pulled

The label "Open Storage (2874 items)" is produced by `GetActionNameScript`, which the engine polls **every frame** for every visible action. Anything expensive there is a frame-rate bug. And a box holding 3,000 items must never put 3,000 anything on the wire.

| What | Mechanism | Why |
|---|---|---|
| Total item count | `[RplProp(onRplName: "OnCountChanged")] int m_iReplicatedCount` + `Replication.BumpMe()` | Engine-batched, no RPC, and **JIP is free** — a streamed-in entity carries the current value. Precedent: `OVT_ReservationSyncComponent` (`Scripts/Game/Components/OVT_ReservationSyncComponent.c:33`). |
| Per-line detail | **Owner-pulled snapshot** on the requesting player's controller: `RpcAsk_RequestSnapshot(RplId)` → `RpcDo_SnapshotBegin(RplId, int lines)` / `RpcDo_SnapshotLine(RplId, string res, int qty)` / `RpcDo_SnapshotEnd(RplId)`, all `RplRcver.Owner` | Only the one player with the menu open needs detail. Bounded by *distinct item types* (tens), not item count (thousands). No broadcast, no array on the wire. |
| Live refresh while the menu is open | `OnCountChanged` → coalesced re-pull (250 ms, remove-before-`CallLater`) | Covers "another player is loading this box while I watch", reuses `OVT_WarehouseContext`'s coalescing idiom (`:132-136`). |

**No `array<...>` is ever put on an RPC.** Nothing in the codebase does it today (grep confirms zero instances), and `Rpc()` is an untyped variadic whose arity mistakes compile clean and die silently on the wire (BUG-090). Every RPC parameter list in this feature is primitives only, and **every `Rpc()` call site must be hand-audited against its handler signature.**

Owner responses use the established `ShouldRespondLocally()` direct-call branch so a listen-server host is not silently dropped (the engine never loops an RPC back to the sender — see `OVT_BaseServerProgressComponent.IsLocalPlayerOwner`, `:185-197`).

**The warehouse keeps its existing replication unchanged in this feature** — manager `RplSave`/`RplLoad` (`OVT_RealEstateManagerComponent.c:886/:917`) plus `RpcDo_SetWarehouseInventory` per-line broadcast (`:966`). It is not migrated to the pull model because its two storm sources are deleted outright in Phases 4-5 (a warehouse withdrawal now touches one line, and the container dump that used to stream ~40 lines is gone), which removes the storm without touching a replication path that works.

### 3.4 The conversion engine

Lives on `OVT_StorageRequestComponent`, one instance per player's `OVT_OverthrowController`. Per-operation state is protected members on that component; `m_bIsRunning` (inherited) is the latch.

> This is the structural fix for `OVT_InventoryManagerComponent`'s central defect: it kept its search results and progress state on a **single game-mode singleton shared by every player** (`m_aContainerSearchResults`), so two concurrent operations corrupted each other. On a per-player component the latch is per player by construction.

Two directions, both driven by `GetGame().GetCallqueue().CallLater` with a fixed batch size and inter-batch delay (start at **25 items / 100 ms**, tune during the play-test):

**A. Vanilla inventory → storage (`ConvertToStorage`)**

1. Enumerate the source with the **correct** enumerator for its kind — `SCR_VehicleInventoryStorageManagerComponent.GetItems()` for vehicles (registers storages on attached child entities, i.e. truck beds), `SCR_InventoryStorageManagerComponent.GetItems()` for boxes. **Never** the root-only `GetOwnedItems()` path, which misses truck beds — `OVT_SellableItemScanner.c:80-83` documents exactly this trap and names `OVT_InventoryManagerComponent` as the code that falls into it.
2. **Pre-pass per weapon**: enumerate the weapon's `SCR_WeaponAttachmentsStorageComponent` and move every child out into the container, so optics, muzzle devices **and the loaded magazine** become independent items. Discriminate the magazine with `item.FindComponent(BaseMagazineComponent)` (vanilla's own test, `SCR_InventoryMenuUI.c:3678`); attachments and the loaded mag live in the *same* storage (`SCR_DetachMagazineUserAction.c:26-33`). **Removal must go through the holder's `InventoryStorageManagerComponent`, not the weapon's — a weapon entity has none** (this is an existing latent bug at `OVT_LoadoutManagerComponent.c:2129`; do not copy it).
3. **Per item**, in order: skip if `OVT_StorageRules.IsFullMagazine()` says it is a part-used magazine (`MagazineComponent.GetAmmoCount() < GetMaxAmmoCount()` — the predicate already exists at `OVT_VehicleRearmUtils.c:133`); ask the ledger whether it fits; `TryDeleteItem(item)`; **only if the delete returned true**, `ledger.Add(prefab, 1)`.
4. On completion, `SendOperationComplete(converted, skipped)` and one `Replication.BumpMe()`.

**B. Storage → vanilla inventory (`ConvertFromStorage`)**

Per item: `TrySpawnPrefabToStorage(res)` → **only if it returned true**, `ledger.Take(res, 1)`. Stop early when the container refuses (full), report the shortfall as `itemsSkipped`.

**Delete-then-credit / spawn-then-debit is the whole data-integrity story** and is the rule `OVT_RealEstateManagerComponent.TransferToWarehouse` already documents (`:576-577`). Never batch the commit: per-item commit bounds any crash to one item.

**Cancellation.** If the source or destination entity dies mid-operation, or the requesting player disconnects, the next batch tick finds a null and aborts cleanly — leaving the ledger and the world consistent, because every completed item is already committed on both sides.

### 3.5 Identity: `ResourceName`, and the take gate is ledger membership

Ledger keys are **prefab `ResourceName` strings** (`OVT_PrefabUtils.GetPrefabName(entity)`), matching the warehouse and the loadout system.

The economy's integer ids are rejected because `GetInventoryId()` is a bare map index: an **unregistered** prefab silently resolves to id `0`, i.e. *some other item's identity* (`OVT_EconomyManagerComponent.c:1874-1892`). Looted occupying-faction gear is routinely unregistered, so an int-keyed ledger would silently turn a Soviet helmet into whatever item 0 is.

That forces a second decision, and it is the most important one in the feature:

> **Server-side conversion credits unregistered prefabs too. `IsRegisteredResource` must NOT gate a conversion — gating it would delete the player's loot.**

The registry check exists because `RpcAsk_TakeFromWarehouse*` feeds a **client-chosen string** to `TrySpawnPrefabToStorage`, i.e. an arbitrary prefab spawn (`OVT_RealEstateRequestComponent.c:518-557`). The new storage paths have a **strictly stronger** gate available: *the string must already be a line in that specific ledger with at least that quantity*, and a line can only get there by the server deleting a real entity. So:

| Path | Gate |
|---|---|
| Server-side conversion (vanilla → storage) | none needed — the string came from an entity the server just deleted |
| Client take-out of storage / warehouse | `ledger.GetCount(res) >= qty` (replaces `IsRegisteredResource` on the warehouse take paths too, so converted loot is not trapped) |
| Client *add* by name (warehouse `RpcAsk_AddToWarehouse`, port import) | `IsRegisteredResource` **retained** — those mint stock from a name, not from an entity |

### 3.6 Which vehicles get storage

`Prefabs/Vehicles/Core/Vehicle_Base.et` is Overthrow's override of the vanilla base and is inherited by **every vehicle in the game**, civilian and military alike (`Wheeled_Base.et:1`, `Helicopter_Base.et:1`). There is no civilian/military split in the prefab tree — the distinction is **data only**.

Design: wire `OVT_StorageComponent` once on `Vehicle_Base.et` with a civilian-car default capacity, then **disable it at runtime on anything that is not a civilian vehicle**, using the same data that already defines "civilian" — `OVT_EconomyManagerComponent.IsLegalVehicle(id)`, backed by `vehiclePrices.conf`'s `illegal` flag (default `1`) plus the `CIV` faction force-legalisation (`OVT_EconomyManagerComponent.c:1635-1637`).

```
authority, lazily on first query (the economy database is not ready at OnPostInit):
   if capacity was authored on this prefab (truck override) -> use it
   else if !economy.IsRegisteredResource(prefab)            -> 0   (fail safe: no storage)
   else if !economy.IsLegalVehicle(economy.GetInventoryId(prefab)) -> 0
   else                                                     -> the authored default
```

Capacity table:

| Host | Capacity | Where set |
|---|---|---|
| Placed ammo box (`OVT_AmmoBox_Base.et`) | `-1` | prefab attribute |
| `M923A1_transport.et`, `Ural4320_transport.et`, `OverthrowMobileFOB.et` | `-1` | prefab attribute (overrides the legality gate) |
| Civilian cars (UAZ469, S105, S1203, M998/M1025 unarmed, …) | `100` | `Vehicle_Base.et` default, allowed through by the legality gate |
| Armed HMMWV, BTR70, helicopters, everything `illegal 1` | `0` (disabled) | legality gate |
| Warehouse | `-1` | manager-side, ledger constructed unlimited |

Capacity `0` means the storage actions do not appear at all (`CanBeShownScript` returns false).

**Fallback if the legality gate proves unusable** (economy not ready when the first action label is drawn, or `IsLegalVehicle` turns out to be too narrow): author `m_iCapacity` explicitly on the ~8 civilian prefabs and the 3 trucks and set the `Vehicle_Base.et` default to `0`. Decide this during Phase 1 with a Workbench check; do not carry both mechanisms.

### 3.7 Boundary: the `logistics` epic is a different system

`docs/features/logistics/resource-storage/requirements.md` states plainly that warehouses gain "a resource ledger **alongside** [the] existing item inventory ... Both are visible and usable from the warehouse menu." That is the contract:

- **This feature owns ITEMS** — prefab-keyed, count-capped, unlimited where it makes sense (warehouses, boxes, trucks).
- **`logistics` owns RESOURCES** — `EResourceType`-keyed (a sealed engine enum), **m³-capacity-checked**, hauled by truck, spent on construction.

They do not merge. `logistics` **may** reuse `OVT_StorageLedger` as a starting point if a count-keyed ledger fits, but it gets its own capacity model (volume, not count) and its own persistence record. Nothing in this feature may assume a resource ledger exists, and nothing here may be widened "so logistics can use it later" — that is precisely the speculative generality this plan forbids.

---

## 4. Implementation Phases

Every phase must leave `tools/compile-check.sh` at exit 0 and the suites green. **Do not run `tools/run-tests.sh` from an implementation agent** — the orchestrator runs it once per completed phase (`.claude/test-policy.md`).

**Reserved GUID series for this feature: `6B62A0000000xxxx`** (verified unused repo-wide, 2026-08-19). Every new `.conf`/`.et`/`.meta`/layout object takes the next value in the series. A GUID collision fails **silently**.

---

### Phase 1 — Ledger, component, prefab wiring, persistence

*Agent: `component-developer`.*

| # | Task | Acceptance |
|---|---|---|
| 1.1 | `Scripts/Game/Data/OVT_StorageLedger.c` per §3.2. `m_iTotal` maintained incrementally; zero-quantity lines removed on write. | Compile 0. |
| 1.2 | `Scripts/Game/Data/OVT_StorageRules.c` — start with `CapacityAllows(int total, int capacity, int qty)` and `ResolveCategory(...)`. (The magazine and clothing predicates land in Phases 2 and 6 with their consumers.) | Compile 0. |
| 1.3 | `Scripts/Game/Components/OVT_StorageComponent.c` — `[Attribute] m_iCapacity` (default `100`), `[RplProp(onRplName:"OnCountChanged")] m_iReplicatedCount`, lazy capacity resolution per §3.6, authority-only `AddItem`/`TakeItem`/`SetLine`/`ClearAll`, `GetLedger()`, `GetItemCount()`, `IsEnabled()`. Every writer bumps the scalar exactly once and calls `Replication.BumpMe()`. | Compile 0; `OVT_ComponentFinder<OVT_StorageComponent>.Find()` resolves on a spawned box. |
| 1.4 | Prefab wiring — `Prefabs/Props/Military/AmmoBoxes/OVT_AmmoBox_Base.et` (capacity `-1`, new component entry after `SCR_InventoryStorageManagerComponent` at `:37-38`); `Prefabs/Vehicles/Core/Vehicle_Base.et` (default capacity, new entry in `components { }` alongside the existing `ActionsManagerComponent`); `M923A1_transport.et`, `Ural4320_transport.et`, `OverthrowMobileFOB.et` (capacity `-1`). Fresh GUIDs from the reserved series. | Workbench loads all five prefabs clean (**user-gated**). |
| 1.5 | `Scripts/Game/Persistence/Serializers/Components/OVT_StorageComponentSerializer.c` — `WriteValue("version", 1)`, capacity, line count, then `(string, int)` pairs. Read order == write order. Guard `version < 1` → return true (no payload). Model on `OVT_PlayerOwnerComponentSerializer.c`. | Compile 0. |
| 1.6 | Bind it in `Configs/Systems/Persistence/Overthrow.conf` **twice**, with two distinct fresh GUIDs (the pattern `OVT_PlayerOwnerComponentSerializer` uses for its three bindings): under the Vehicles group's car config `{64C6B4937723DA61}` (`:97-121`) and under the Overthrow group's `OVT_PlaceableComponent` config `{6B0E7A215A7FD39C}` (`:156-174`). | An entity gets exactly one `EntityPersistenceConfig`; both hosts must list the serializer or one of them silently loses stock. |
| 1.7 | Logic suite `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_StorageLedger.c` — Add clamps to capacity and returns the actual; Add to an unlimited ledger never clamps; Take clamps to stock; a line hitting zero disappears; `GetTotalCount()` tracks Add/Take/Set/Clear; `Set` replaces rather than accumulates. | Each case carries a recorded proof-it-can-fail preamble. **No `maxAttempts`.** |
| 1.8 | Persistence round-trip case in `Scripts/Game/Tests/TestSuites/Persistence/OVT_TEST_PersistenceRoundTripSuite.c` — seed a placed ammo box's ledger via the component API, save, reload, assert the same lines and quantities. | Suite still exits 0. |

**Phase acceptance:** compile 0; new Logic cases pass; the round-trip case passes; five prefabs load in Workbench.

---

### Phase 2 — Controller seam + batched conversion engine

*Agent: **`component-developer-advanced`** — new client→server seam, new RPC surface, the data-integrity core of the feature, and unfamiliar vanilla weapon/magazine APIs.*

| # | Task | Acceptance |
|---|---|---|
| 2.1 | `Scripts/Game/Components/Controller/OVT_StorageRequestComponent.c : OVT_BaseServerProgressComponent`. Reuse the static rule bodies (`OVT_ControllerRequestComponent.ResolveOwningPlayerIdFor`, `PlayerMayUseVehicleFor`) and copy `OVT_ContainerTransferComponent`'s validation ladder verbatim: `ResolveCallerPlayerId` → `CallerIsWithin` → `CallerMayReach` → `RejectStorageRequest` (log line **always**, `SendOperationError` when not already running). `TRANSFER_MAX_DISTANCE = 30`. | No handler takes a `playerId` parameter — `grep -n "RpcAsk_.*playerId" Scripts/Game/Components/Controller/OVT_StorageRequestComponent.c` returns nothing. |
| 2.2 | Register on `Prefabs/GameMode/OVT_OverthrowController.et` (fresh GUID, alongside the 21 existing components). | Workbench loads the prefab clean (**user-gated**). |
| 2.3 | Snapshot pull: `RpcAsk_RequestSnapshot(RplId)` + `RpcDo_SnapshotBegin/Line/End` (`RplRcver.Owner`, with the `ShouldRespondLocally()` direct-call branch). The client writes lines into its own copy of the component's ledger via `SetLine`. | Hand-audited `Rpc()` arity, argument by argument, against each handler signature. |
| 2.4 | `OVT_StorageRules.IsFullMagazine(IEntity)` — `MagazineComponent.Cast(item.FindComponent(MagazineComponent))`; no magazine component → true (not a magazine, convert it); otherwise `GetAmmoCount() >= GetMaxAmmoCount()`. Keep the entity lookup in a thin wrapper and the comparison in a pure `IsFullByCount(int ammo, int maxAmmo)` so the Logic tier can reach it. | Logic cases: full → convert; part-used → skip; empty → skip; zero-max (defensive) → convert; non-magazine → convert. |
| 2.5 | Weapon strip pass per §3.4 step 2. Enumerate `SCR_WeaponAttachmentsStorageComponent` with `GetOwnedItems(items, false)` (vanilla's idiom, `SCR_PlayerArsenalLoadout.c:318`); move each child into the **container's** storage through the **container's** `InventoryStorageManagerComponent`. Re-attachment ordering is irrelevant here (nothing is re-attached) — do not copy `SCR_WeaponAttachmentsStorageComponentSerializer`'s sort. | A rifle with an optic and a full mag converts as **three** ledger lines. |
| 2.6 | `ConvertToStorage(...)` batched engine per §3.4A, including the **delete-then-credit** rule and the correct per-kind enumerator. `StartOperation("#OVT-Progress-ConvertingToStorage")`. | A 300-item box converts with the progress bar advancing and no frame spike. |
| 2.7 | `ConvertFromStorage(...)` batched engine per §3.4B, **spawn-then-debit**. `StartOperation("#OVT-Progress-ConvertingFromStorage")`. | A full-container refusal leaves the un-spawned remainder in the ledger. |
| 2.8 | `RpcAsk_TransferAllToStorage(RplId)` and `RpcAsk_TakeFromStorage(RplId, string res, int qty)` handlers wired to 2.6/2.7 behind the ladder, with the §3.5 gates. | Every rejection produces a `LogLevel.WARNING` line naming the player, the request and the reason. |
| 2.9 | Init case `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_StorageRequestSeam.c` — the component resolves off `OVT_OverthrowController` (mirror `OVT_TEST_Init_RealEstateRequestSeam.c`). | Case passes with a recorded proof-it-can-fail. |
| 2.10 | New loc keys in `Language/localization_Overthrow.st` **only** (never the `.conf` exports — those are Workbench build output): `#OVT-Progress-ConvertingToStorage`, `#OVT-Progress-ConvertingFromStorage`, `#OVT-Storage-Full`, `#OVT-Storage-Empty`, `#OVT-Storage-NothingToConvert`. Every consumer has an English code fallback. | `.st` braces balanced (an unbalanced brace loses data on the next Workbench save). |

**Phase acceptance:** compile 0; Logic + Init cases pass; a manual conversion of a stocked box moves items in and back out with the progress bar and no duplication.

---

### Phase 3 — Actions and the Storage UI

*Agent: `component-developer` for the actions, `ui-developer` for the menu. Run the action work first.*

| # | Task | Acceptance |
|---|---|---|
| 3.1 | `Scripts/Game/UserActions/OVT_BaseStorageUserAction.c : SCR_InventoryAction` — hoist the ~28-line lock-check block that is copy-pasted verbatim into `OVT_OpenStorageAction.c:29-57`, `OVT_LoadStorageAction.c:70-98` and `OVT_UnloadStorageAction.c:74-102`. Add: storage-component resolution walking `GetOwner()` → `GetParent()` (needed because `door_rear` lives on the truck's cargo **child** while the storage component lives on the root — the same reason `OVT_LootIntoVehicleAction.c:35-49` does it), and a cached `RefreshCache()` for label data. Rebase all three existing actions onto it. | `grep -c "OVT_OverthrowGameMode ot = OVT_OverthrowGameMode.Cast" Scripts/Game/UserActions/` drops from 3 to 1. |
| 3.2 | `OVT_OpenItemStorageAction` — label `"#OVT-OpenStorage (" + n + " items)"`, or `"(n/cap items)"` when capacity is finite, read from the **replicated scalar** (never by walking the ledger). Hidden when `!IsEnabled()`. Opens `OVT_StorageContext`. `HasLocalEffectOnlyScript()` true. | The label updates within a frame of the server's count changing, and costs one integer read per poll. |
| 3.3 | `OVT_TransferAllToStorageAction` — calls `RpcAsk_TransferAllToStorage`. Disabled with `SetCannotPerformReason("#OVT-Storage-NothingToConvert")` when the vanilla inventory is empty, and `"#OVT-Storage-Full"` when the ledger is full. | Rejections are visible, never silent. |
| 3.4 | Ammo box prefab wiring — add both actions to `OVT_AmmoBox_Base.et`'s `additionalActions` block (`:49-78`) on context `"default"`. **Renumber Sort Priority** so the new Open sits immediately after the vanilla Open: `OVT_OpenStorageAction` (unset/0) → `OVT_OpenItemStorageAction` 1 → `OVT_TransferAllToStorageAction` 2 → `OVT_LoadStorageAction` 3 (was 1) → `OVT_UnloadStorageAction` 4 (was 2). Bump `OVT_AmmoBox_Placed.et`'s Lock/Unlock from 3 to 6 so they do not collide. **Verify in Workbench that ascending priority = display order**; if it is inverted, invert the whole table. | The action list reads Open, Open Storage, Transfer All, Load, Unload, Lock/Unlock, loadouts. |
| 3.5 | Vehicle prefab wiring — add both actions to `Vehicle_Base.et`'s `additionalActions` block with `ParentContextList { "door_r01" "door_l01" "door_rear" }`, priorities 1 and 2 (`OVT_SellVehicleCargoAction` is 101, `OVT_FillFuelAction` is on `fuel_cap`). | Actions appear on a civilian car and on both trucks; absent on a BTR70. |
| 3.6 | `OVT_StorageContext` + `StorageMenu.layout` + `StorageMenu/StorageInventoryItem.layout` + `OVT_StorageMenuTabComponent`. Copy `WarehouseMenu.layout` as the base and add a category tab strip (reuse `UI/Layouts/Menu/ShopMenu/ShopMenu_Tab.layout` as the tab widget; `OVT_ShopMenuTabComponent` is bound to `OVT_ShopContext`, so write a sibling rather than generalizing it). Buttons: Transfer 1 / Transfer 10 / Transfer All / Close. **One handler per button, each on its OWN widget** (BUG-081) with matching removal in `OnClose`. Alphabetical sort by display name within a category, memoised via `OVT_PrefabUtils.GetItemUIInfo`. Categories from `OVT_EconomyManagerComponent.GetItemCategory(id)` guarded by `IsRegisteredResource` — an unregistered prefab must fall to `OVT_ShopCategoryHelper.GetCategoryForUncatalogued()`, never to `GetInventoryId()`'s id-0 default. | Fully navigable on a gamepad; a Take redraws when the server's number lands, not optimistically. |
| 3.7 | Register the context on `Prefabs/Characters/Factions/INDFOR/FIA/Character_Player.et` (`m_sContextName "OverthrowStorageContext"`, `m_sCloseAction "MenuBack"`, `m_bOpenActionCloses 0`, `m_ItemLayout`), and add the `ActionContext OverthrowStorageContext` block to `Configs/System/chimeraInputCommon.conf` modelled on `OverthrowWarehouseContext` (`:1079`). | Context opens and closes from both mouse and gamepad. |
| 3.8 | Loc keys in the `.st` master: `#OVT-OpenStorage`, `#OVT-OpenStorageCapped`, `#OVT-TransferAllToStorage`, `#OVT-Storage-MenuTitle`, `#OVT-Storage-Transfer1/10/All`. | English fallbacks present in code. |

**Phase acceptance:** compile 0; suites still green; the menu opens on a box and a car, lists categorized stock, and Transfer 1/10/All spawn the right items into the container.

---

### Phase 4 — Warehouse migration onto the ledger

*Agent: **`component-developer-advanced`** — touches a manager, a serializer, a replication path, a controller component and a UI context at once, and changes the save format.*

| # | Task | Acceptance |
|---|---|---|
| 4.1 | Replace `OVT_WarehouseData.inventory` (`map<string,int>`, `OVT_RealEstateManagerComponent.c:13`) with `ref OVT_StorageLedger m_Ledger`, constructed unlimited. Update every reader/writer: `DoAddToWarehouse` `:517`, `DoTakeFromWarehouse` `:554`, `RpcDo_SetWarehouseInventory` `:967`, `RplSave` `:902-907`, `RplLoad` `:937-945`, `SetOwner` `:220`, `RpcDo_SetWarehouseOwner` `:998`. | `grep -n "warehouse.inventory\|\.inventory\[" Scripts/` returns nothing. |
| 4.2 | **Delete** `GetWarehouseInventory` (`:473-492`) and the `isLinked` field. Both are dead — `GetWarehouseInventory` has no callers, and `isLinked`/`isPrivate` are never set to true anywhere. `isPrivate` is kept (it is persisted and cheap); `isLinked` goes. | Zero references remain. |
| 4.3 | `OVT_RealEstateManagerSerializer` version **1 → 2**. v2 writes the ledger's capacity alongside the existing parallel `itemIds[]`/`itemCounts[]` arrays (the record shape is otherwise unchanged). Deserialize accepts **both**: v1 → load the arrays, capacity `-1`; v2 → load arrays + capacity. Use `CanSeekMembers()` for the optional tail. | An existing save loads with its warehouse stock intact; a new save round-trips capacity. |
| 4.4 | `OVT_RealEstateRequestComponent.WarehouseHasStock` (`:566-573`) reads the ledger. Replace `IsRegisteredResource` with **ledger membership** on the two *take* paths (`RpcAsk_TakeFromWarehouse` `:448`, `RpcAsk_TakeFromWarehouseToVehicle` `:481`) per §3.5, keeping it on `RpcAsk_AddToWarehouse` `:426`. Update the class comment — the reasoning it records is the thing being changed. | Loot converted into a warehouse can be withdrawn again. |
| 4.5 | **Rewrite `TakeFromWarehouseToVehicle` (`:642-675`) as pure ledger arithmetic**: warehouse ledger → the vehicle's `OVT_StorageComponent` ledger, capacity-clamped, zero spawns, no progress bar. This is one of the two headline spike deletions (G6). Refuse with a visible reason if the vehicle has no enabled storage. | `grep -n "TrySpawnPrefabToStorage" Scripts/Game/GameMode/Managers/OVT_RealEstateManagerComponent.c` returns nothing. |
| 4.6 | `OVT_StorageContext` gains a **warehouse source mode** — `SetWarehouse(OVT_WarehouseData)` alongside `SetStorage(OVT_StorageComponent)`. Destination differs by mode: warehouse mode transfers into the **vehicle's storage ledger** (per the requirement); component mode spawns into the container's vanilla inventory. Subscribe to `m_OnWarehouseInventoryChanged` in warehouse mode, to `OnCountChanged` in component mode. | One context class, two sources, one layout. |
| 4.7 | **Delete `OVT_WarehouseContext.c`** and repoint `OVT_VehicleMenuContext.c:160-165` at `OVT_StorageContext`. Remove the `OVT_WarehouseContext` registration from `Character_Player.et:100-106`. Delete `UI/Layouts/Menu/WarehouseMenu.layout` (+ `.meta`) and `WarehouseMenu/WarehouseInventoryItem.layout` (+ `.meta`) once nothing references them; keep `OverthrowWarehouseContext` in `chimeraInputCommon.conf` **only** if something still names it, otherwise remove it too. | Workbench loads clean; the vehicle-menu warehouse entry opens the new menu (**user-gated**). |
| 4.8 | Logic cases for the v1→v2 record mapping (a pure "old arrays → ledger" helper, so the branch is testable without a world). | Cases pass with proof-it-can-fail. |

**Phase acceptance:** compile 0; the full suite green including the existing `RealEstateOwnership` round-trip; an **existing pre-migration save** loads with warehouse stock intact (explicit manual check).

---

### Phase 5 — Load / Unload, port import, FOB undeploy

*Agent: **`component-developer-advanced`** — three separate integration flows, one of which (FOB undeploy) has a single-in-flight latch that wedges the whole session if an error path is missed.*

| # | Task | Acceptance |
|---|---|---|
| 5.1 | `OVT_LoadStorageAction` — box **storage** → vehicle **storage**, pure ledger arithmetic. Rename the label to `#OVT-LoadStorage` ⇒ "Load Storage into Vehicle". Fix the **unguarded null `nearestVeh` dereference at `:33-34`** (the sphere query is 10 m but the acceptance radius is 15 m, so `nearestVeh` can be null when every hit is out of range). | Compile 0; no null deref reachable. |
| 5.2 | `OVT_UnloadStorageAction` — **first** run `ConvertToStorage` on the source vehicle, **then** on completion move the vehicle's ledger into the box's ledger. Chain on the progress component's server-side `m_OnOperationComplete`, matching how `OVT_ResistanceFactionManager` chains its FOB continuations (`:562-563`). Same null fix as 5.1 (`OVT_UnloadStorageAction.c:33-34`). Rename the label to mention Storage. | A truck with loose loot unloads into a box in one action, converting on the way. |
| 5.3 | **Rewrite `OVT_VehicleRequestComponent.RpcAsk_ImportToVehicle` (`:401-494`) to credit the vehicle's storage ledger** instead of the 100-iteration `TrySpawnPrefabToStorage` loop at `:484-490`. Keep **every** existing gate verbatim (BUG-033/BUG-102 family: validity, occupying-faction, `Import` permission, extended-catalogue/`IllegalImports`, port distance ×2, affordability) and keep "pay for what actually fitted" — now clamped by ledger capacity rather than cargo volume. Refuse visibly when the vehicle has no enabled storage. | The second headline spike deletion (G6). Zero spawns on the import path. |
| 5.4 | FOB undeploy — extend the collection so it drains **both** the vanilla inventory and the `OVT_StorageComponent` ledger of every collectable container in radius, into the mobile FOB truck's ledger. Entry chain is unchanged: `OVT_UndeployFOBAction` → `OVT_FOBRequestComponent.RpcAsk_UndeployFOB` `:346` → `OVT_ResistanceFactionManager.UndeployFOB` `:569` → the transfer component. | Ammo box stock survives an undeploy/redeploy cycle. |
| 5.5 | Spent magazines are **deleted** during FOB collection (`!OVT_StorageRules.IsFullMagazine(item)` → `TryDeleteItem`), per the requirement. | A FOB full of part-used mags undeploys without hauling them. |
| 5.6 | Audit the FOB latch: `m_pCurrentDeploymentSource/Target/UndeployedFOB/MobileFOB` must be cleared on **every** exit including the new refusal paths, or FOB operations wedge for the whole session (`OVT_ResistanceFactionManager.c:590-594`). Every new early return must route through `SendOperationError`. | Deliberately fail a collection and confirm the next undeploy still starts. |

**Phase acceptance:** compile 0; suites green; import, warehouse take, box load/unload and FOB undeploy all work end to end with no visible spawn burst.

---

### Phase 6 — Truck Loot rework, officer clear, vanilla-supplies hiding, dead-code deletion

*Agent: `component-developer` (advanced if 6.1-6.3 are taken together with 6.7).*

| # | Task | Acceptance |
|---|---|---|
| 6.1 | `OVT_StorageRules.IsBaseClothing(typename areaType)` — keep out **only** `LoadoutJacketArea`, `LoadoutPantsArea`, `LoadoutBootsArea`. Vest, armoured vest, backpack, head cover and handwear are now **taken**. Use `typename`s, not strings: the current string list at `OVT_InventoryManagerComponent.c:812-820` contains `"LoadoutHandwearArea"`, which **is not a real class** (it is `LoadoutHandwearSlotArea`) and has therefore never matched — `OVT_LoadoutSwap.c:395-400` already documents the typo and demonstrates the typename fix. | Logic cases: each of the three base garments excluded; vest/backpack/helmet/handwear included; unknown area included. |
| 6.2 | Migrate the loot walk off `OVT_InventoryManagerComponent.ProcessBattlefieldLoot` (`:714-792`, **100 ms per entity**, ignores `OVT_StorageOperationConfig` entirely, re-resolves storage components every iteration) onto the Phase 2 batcher on `OVT_StorageRequestComponent`. Items still land in the truck's **vanilla** inventory (loot only converts when transferred to an ammo box — requirement). Keep extracting the contents of skipped garments before dropping them. | 30 corpses complete in seconds, not 3-6 s of serialized `CallLater`s, with the progress bar moving. |
| 6.3 | Do not delete a body that still holds gear. Today `SCR_EntityHelper.DeleteEntityAndChildren` fires whenever `bodyItemsLooted > 0` (`:771-782`), destroying whatever did not fit. Delete only when the body is actually empty of lootable items. | No silent item destruction on a full truck. |
| 6.4 | Rebase `OVT_LootIntoVehicleAction` onto `OVT_BaseStorageUserAction` — it currently returns `true` unconditionally from `CanBePerformedScript` (`:73-76`), i.e. **no lock or ownership check at all**. Replace the two hard-coded English strings (`:21-26`, `:51-55`) with loc keys, and move the `#OVT-BodiesLooted` confirmation (`:68`, fired before the server has done anything) onto the operation-complete callback. | Looting someone else's locked truck is refused with a reason. |
| 6.5 | Raise truck inventory capacity for ~20-30 soldiers of loot: `M923A1_transport.et:4-7` and `Ural4320_transport.et:4-7` from `MaxCumulativeVolume 200000` / `m_fMaxWeight 1000` to `3000000` / `15000` (the ammo box's volume, half its weight — `OVT_AmmoBox_Base.et:28-30`). Also **investigate** whether `UseCapacityCoefficient 0` belongs on trucks: it is set on all four Overthrow boxes and on neither truck. | Play-test: loot 25 AI into one truck with nothing refused. Tune the numbers from the measurement — do not ship the guess unmeasured. |
| 6.6 | `OVT_ClearVanillaInventoryAction` — officer-only (`OVT_ResistanceFactionManager.IsOfficer(playerId)`, checked **server-side** in the handler, not only in `CanBePerformedScript`), deletes the ammo box's **vanilla** inventory and never touches the ledger. Batched through the same engine. Add to `OVT_AmmoBox_Base.et` at Sort Priority 5. | A non-officer's request is logged and refused. |
| 6.7 | **Dead-code deletion.** `Scripts/Game/UI/Context/OVT_StorageProgressUIContext.c` (its layout does not exist). `OVT_InventoryManagerComponent`: `TransferToWarehouse` (`:247`), `PerformWarehouseTransfer` (`:624`, a stub), `UndeployFOBWithCollection` (`:212`), and `LootBattlefieldIntoVehicle` + `StartBattlefieldLooting` + `ProcessBattlefieldLoot` + `LootBodyItems` + `ExtractItemsFromClothing` once 6.2 lands. `OVT_ContainerTransferComponent`: `RpcAsk_TransferStorageForDeployment` and `RpcAsk_CollectContainers` (both documented caller-less at `:175-178` and `:247-248`). **Keep** `TransferStorageByRplId` — `OVT_VehicleManagerComponent.c:390` (vehicle upgrade) still uses it. | `grep -rn` for each deleted symbol returns nothing. |
| 6.8 | Vanilla supplies hiding audit. The two cargo trucks already neutralise it by emptying `ParentContextList` (`M923A1_cargo.et:6-13`, `Ural4320_cargo.et:6-13`). Grep `Prefabs/` and `Configs/` for any other `SCR_ResourceContainer*Action`, `SCR_ResourceComponent` UI surface or supplies HUD element that is still player-visible and neutralise it the same way. | No "Supplies"/"Storage" vanilla surface reachable in a normal session. |

**Phase acceptance:** compile 0; suites green; loot, officer clear and the deletions all verified; every `grep` in 6.7 empty.

---

### Phase 7 — Help & documentation sync

*Agent: `help-docs-sync`. Required — this feature changes what players see and do.*

- Tutorial popups (`Configs/Tutorials/`): a first-storage-encounter tip explaining that Load/Unload now move **storage**, and that the item count in the action label is where their gear went.
- Field Manual (`Configs/FieldManual/`): a Storage entry — what it is, why it exists (performance), capacity by vehicle class, the half-magazine rule, and the officer clear action.
- Public wiki: same content.
- **Every sentence must cite a `file:line` or be cut.** Two shipped tips have previously described mechanics that did not exist; no gate catches a well-formed lie.
- Any new tutorial/manual key goes in `Language/localization_Overthrow.st` only, and the user is asked for a Workbench re-export of the `.conf` files.

---

## 5. Key Technical Decisions

**D1 — Ledger keys are `ResourceName` strings, not economy integer ids.**
`GetInventoryId()` is a bare map index; an unregistered prefab silently resolves to id `0`, i.e. another item's identity (`OVT_EconomyManagerComponent.c:1874-1892`). Looted occupying-faction gear is routinely unregistered. Strings also match the warehouse and the loadout system, so no translation layer is needed anywhere. Cost: a string key per line, which is what the warehouse has always paid.

**D2 — Conversion credits unregistered prefabs; the take-out gate is ledger membership, not `IsRegisteredResource`.**
The registry check exists to stop a **client-chosen string** reaching `TrySpawnPrefabToStorage`. A ledger line is not a client-chosen string — it can only exist because the server deleted a real entity — so "this ledger holds ≥ qty of this key" is a strictly stronger gate *and* it does not delete the player's loot. Applying the registry check to a conversion would silently destroy every unregistered item a player transfers. This is the single highest-consequence decision in the feature.

**D3 — One replicated integer + an owner-pulled snapshot, instead of per-line broadcast.**
The action label needs a number every frame; only the one player with the menu open needs detail. `RplProp` gives the number to everyone (JIP included) for free, and the snapshot is bounded by *distinct types*, not item count. Rejected alternatives: (a) per-line broadcast — the storm the requirements name; (b) `RplSave`/`RplLoad` of the full ledger on every streamer — pays the full cost for every nearby client whether or not they look; (c) arrays over RPC — **zero** precedent in this codebase and `Rpc()` arity errors compile clean and fail silently (BUG-090).

**D4 — Warehouses keep their manager-side record; only the container changes.**
A warehouse is a static world building with no Overthrow prefab. Its record must stay on `OVT_RealEstateManagerComponent`, keyed by location. Swapping the raw map for `OVT_StorageLedger` is therefore the *entire* migration, and it lets one UI and one set of rules serve both hosts.

**D5 — A fresh batcher, not a repair of `OVT_InventoryManagerComponent`.**
The 908-line manager is not salvageable within this feature's risk budget: caller-less public entry points (`TransferToWarehouse` `:247`, `UndeployFOBWithCollection` `:212`), a stub (`PerformWarehouseTransfer` `:624`), fabricated per-item counts in `ProcessContainersSequentially` (`:549` — it counts *containers* and passes null nested callbacks), a **non-reentrant singleton side channel** shared by every player, a root-only enumeration that misses truck beds, a loot walk that ignores its own config object and costs 100 ms per entity, and positional constructor args mislabelled at their call sites (`OVT_ContainerTransferComponent.c:159-166` labels `deleteEmpty` as `skipWeaponsOnGround`). The new engine lives on a **per-player** component, which makes the concurrency defect structurally impossible. The two live entry points nothing else replaces (`TransferStorageByRplId`, used by the vehicle upgrade) are left alone.

**D6 — Serializer version bump 1 → 2, carrying capacity.**
The persisted warehouse record shape (parallel `itemIds[]`/`itemCounts[]`) does not need to change, so a bump for its own sake would be hollow. It carries the ledger's **capacity** instead, which makes the branch honest and leaves the door open for a finite-capacity warehouse without a second format change. Both versions load; v1 defaults capacity to `-1`.

**D7 — Per-item commit, in the safe order for each direction.**
Delete-then-credit inbound, spawn-then-debit outbound. A crash, a disconnect or a destroyed container can lose or duplicate **at most one item**. Batching the commit would trade that bound for a whole batch. This is the rule `TransferToWarehouse` already documents (`:576-577`) and `TakeFromWarehouseToVehicle` already follows (`:636-637`); it is generalized, not invented.

**D8 — Capacity is an item count, enforced server-side, and `0` means "no storage".**
Volume/weight is what the vanilla inventory already does badly for our purposes; a count is trivially testable, trivially displayable in the action label, and is what the requirements ask for. The `0` sentinel is what lets one `Vehicle_Base.et` wiring cover every vehicle while offensive vehicles simply have no storage actions.

**D9 — `Vehicle_Base.et` + a runtime legality gate, not per-prefab enumeration.**
There is **no** civilian/military split in the prefab tree — `Vehicle_Base.et` reaches BTR70s and helicopters as well as UAZs. The civilian/offensive distinction is data (`vehiclePrices.conf` `illegal`, default `1`, plus the `CIV` faction override), so the gate reads that same data and follows config changes automatically. Fail-safe: an unregistered vehicle gets no storage. Documented fallback in §3.6 if the economy is not ready in time.

**D10 — `OVT_WarehouseContext` is deleted, not kept as a second implementation.**
Two menus over the same ledger would drift. One `OVT_StorageContext` with two sources and two destination modes is the whole point of extracting the ledger.

---

## 6. Definition of Done

An independent evaluator with no implementation context can verify every item below.

### Functional criteria

- **F1** Standing at a placed ammo box, the action list reads: `Open`, `Open Storage (N items)`, `Transfer All to Storage`, `Load Storage into Vehicle`, `Unload Storage from Vehicle`, `Clear Inventory` *(officer only)*, then Lock/Unlock and the loadout actions — in that order.
- **F2** `Open Storage (N items)` shows the true count and updates within a second of another player changing it.
- **F3** `Transfer All to Storage` on a box holding 200 mixed items: the box's vanilla inventory ends **empty except** part-used magazines; the ledger gains one line per distinct prefab; a rifle with an optic and a full magazine produces **three** separate lines; the progress bar runs and no frame hitch is visible.
- **F4** `Open Storage` → select a line → `Transfer 1` / `Transfer 10` / `Transfer All` spawns exactly that many of exactly that prefab into the container's vanilla inventory and decrements the line by the same amount. Transferring into a full container leaves the shortfall in storage.
- **F5** A civilian car shows the storage actions and refuses transfers past its capacity with a visible reason. A BTR70, an armed HMMWV and a helicopter show **no** storage actions.
- **F6** Buying at the port with a truck credits the truck's **storage**; nothing spawns in the cargo bay; the money taken matches what actually fitted.
- **F7** Taking from a warehouse into a vehicle credits the vehicle's **storage**; nothing spawns; the warehouse line drops by the same amount.
- **F8** `Unload Storage from Vehicle` on a truck holding loose loot converts the loot first and then moves the whole ledger into the box, in one action.
- **F9** Undeploying a FOB with two stocked ammo boxes nearby returns **both** their vanilla inventory and their storage into the mobile FOB truck; part-used magazines are gone.
- **F10** The truck `Loot` action takes vests, armoured vests, backpacks, helmets and gloves off corpses and leaves **only** jackets, pants and boots; the contents of those three are still extracted; the progress bar runs; bodies that still hold gear are not deleted.
- **F11** The officer `Clear Inventory` action empties the box's vanilla inventory and leaves the ledger untouched. A non-officer is refused.

### Quality criteria

- **Q1 — No duplication, no loss.** Count the items in a box, transfer all to storage, transfer all back: the multiset is identical apart from part-used magazines, which never left. Repeat across a save/reload and a JIP join.
- **Q2 — Server-authoritative.** Every `RpcAsk_` in `OVT_StorageRequestComponent` starts `if(!Replication.IsServer()) return;` and resolves the caller from its own controller entity. `grep -n "RpcAsk_.*int playerId\|RpcAsk_.*string persId" Scripts/Game/Components/Controller/OVT_StorageRequestComponent.c` returns **nothing**.
- **Q3 — No silent rejection.** Every refusal path emits a `LogLevel.WARNING` naming player, request and reason, and — where the player initiated it — a visible message.
- **Q4 — No unbatched loops.** `grep -rn "TrySpawnPrefabToStorage" Scripts/` shows no call inside a bare `for`/`foreach` over a client-supplied quantity.
- **Q5 — No regressions.** Buying items, selling cargo, refuelling, FOB deploy, vehicle upgrade, loadout save/load and the shop all behave exactly as before.
- **Q6 — Dead code gone.** Each symbol listed in task 6.7 greps to zero.
- **Q7 — Persistence.** Save → quit → **Continue** restores box, truck and warehouse ledgers exactly, and an **existing pre-migration save** still loads its warehouse stock.
- **Q8 — UI standard.** The storage menu is fully operable on a gamepad; one handler per button; every handler inserted in `OnShow` is removed in `OnClose`.

### Verification method

```bash
# 1. Static gate (free, headless, run after every edit)
cd "/mnt/n/Projects/Arma 4/Overthrow.Arma4" && tools/compile-check.sh      # expect exit 0

# 2. Regression gate — ORCHESTRATOR ONLY, once per completed phase
tools/run-tests.sh --group All                                            # expect exit 0

# 3. Dead-code gate
grep -rn "OVT_StorageProgressUIContext\|PerformWarehouseTransfer\|UndeployFOBWithCollection\|RpcAsk_TransferStorageForDeployment\|RpcAsk_CollectContainers\|isLinked\|GetWarehouseInventory" Scripts/ Prefabs/ UI/
# expect: no matches

# 4. Identity gate
grep -rn "RpcAsk_[A-Za-z]*(.*playerId\|RpcAsk_[A-Za-z]*(.*persId" Scripts/Game/Components/Controller/OVT_StorageRequestComponent.c
# expect: no matches
```

**Manual (single player, listen host):** F1-F5, F11, Q1 (in-session), Q7 (save → quit → Continue).
**Manual (dedicated server, 2 clients):** F6-F10, Q1 across JIP, plus the §7 MP list.

---

## 7. Testing Strategy

### Automated coverage — where each behaviour lands

| Tier | Suite | What it pins |
|---|---|---|
| **Logic** | `OVT_TEST_Logic_StorageLedger.c` (P1) | Add/Take clamping and return values; capacity arithmetic including `-1` and `0`; zero-lines removed; `GetTotalCount()` correctness across Add/Take/Set/Clear. |
| **Logic** | `OVT_TEST_Logic_StorageRules.c` (P2, P6) | `IsFullByCount` (full / part-used / empty / zero-max / non-magazine); `IsBaseClothing` over the three excluded and five included area typenames; the v1→v2 warehouse record mapping (P4). |
| **Init** | `OVT_TEST_Init_StorageRequestSeam.c` (P2) | `OVT_StorageRequestComponent` resolves off `OVT_OverthrowController` in a started campaign. Mirrors `OVT_TEST_Init_RealEstateRequestSeam.c`. |
| **Persistence** | `OVT_TEST_PersistenceRoundTripSuite.c` (P1, P4) | A placed ammo box's ledger survives save/reload; warehouse stock still survives after the v2 bump. |

House rules: every new case carries a **recorded proof-it-can-fail** preamble comment (the deliberate fault, the observed failure, the revert); **no `maxAttempts` anywhere**; no case reads state another wrote; no float comparisons.

### What the automated spine cannot reach — manual MP play-test

Run on a **dedicated server with two clients** (a listen host cannot prove the owner-routing branches, and `tools/launch-server.sh` runs this working tree).

1. **`Rpc()` arity on the wire.** Every new request and response actually arrives. A wrong arity compiles clean and dies silently.
2. **Owner-routed progress.** Player A converts a box; **only A** sees the progress bar. Player B, standing at the same box, sees the count change but no bar.
3. **Snapshot isolation.** A and B both open the same box's storage; each sees correct stock; A's Transfer All redraws B's list.
4. **Listen-host direct-call branch.** Repeat 2-3 with the host as the actor (the engine never loops an RPC back to the sender — BUG-090).
5. **Rejection paths.** From a modified/edge client: take more than the ledger holds; take from a box 200 m away; transfer into a locked truck you do not own; a non-officer clearing an ammo box. Each is refused with a log line.
6. **Disconnect mid-transfer.** A starts a 300-item conversion and disconnects at ~50 %. Server does not error-spam; the box's item count plus its ledger equals the starting count ±1.
7. **JIP.** C joins mid-session and sees correct counts on every nearby box and truck, then opens one and sees correct detail.
8. **Continue.** Save on the dedicated server, restart, reconnect: every ledger intact (BUG-104 — a Continue replaces the world without anyone connecting; nothing here may depend on a per-connect rebuild).
9. **Capacity.** Fill a civilian car to its cap; the next transfer is refused with a reason, not silently dropped.
10. **Truck loot volume.** Loot 25 AI into one truck; nothing is refused; **record the actual fill** and tune 6.5's numbers from it.
11. **FOB undeploy/redeploy** with two stocked boxes; then deliberately fail a collection and confirm the next FOB operation still starts (the latch cleared).
12. **Existing save.** Load a pre-migration save; warehouse stock intact; boxes/vehicles start with empty storage and their old real contents still present as entities.
13. **UI on a gamepad** — tabs, list, all four buttons, close.
14. **Performance.** Watch server FPS during a 300-item conversion and during a save with three full boxes. This is the reason the feature exists; if it does not improve, the feature has failed.

---

## 8. Quality Bar

This is a backend/gameplay-heavy feature with one significant UI surface. In priority order:

1. **Data integrity above everything.** No item duplication and no item loss — across conversion, capacity refusal, save/load, JIP, a destroyed container, and a disconnect mid-transfer. Every conversion commits per item, in the safe order for its direction (D7). **`IsRegisteredResource` must never gate a conversion** (D2). If a change makes an item's fate ambiguous, it is wrong.
2. **Network hygiene.** No unbatched entity spawn/despawn loop on any player-reachable path. No per-line broadcast storm. Bulk mutation costs **one replicated integer**; detail is pulled by the one client that asked. No arrays on the wire. Every `Rpc()` arity hand-audited.
3. **Server-authoritative validation.** Every request resolves the caller from its own controller entity, never from a parameter; re-checks proximity, ownership/lock and permission server-side; and logs every rejection with a reason. Storage is a *stockpile* — an unvalidated endpoint here is an item-duplication exploit, not a cosmetic bug.
4. **Deletion over accumulation.** Every path this feature supersedes is removed in the same phase that supersedes it. An endpoint with no callers is attack surface with no purpose.
5. **UI standard inherited from the warehouse menu.** One handler per button on its own widget (BUG-081), matching removal in `OnClose`, coalesced refresh (50 ms, remove-before-`CallLater`), memoised display names, redraw on the **server's** number rather than optimistically, and full gamepad navigability.
6. **Documented reasoning at every non-obvious decision.** The codebase's existing comments are load-bearing — several of them are the only record of a bug's root cause. Where this feature changes behaviour those comments describe (notably `OVT_RealEstateRequestComponent`'s `IsRegisteredResource` rationale), **update the comment in the same edit**.

---

## 9. Dependencies

**Internal (all present, none blocking):**

- `OVT_BaseServerProgressComponent` + `OVT_ProgressEventHandler` + `OVT_ProgressInfo` — the progress plumbing, healthy and listen-server-correct. Always use the `Send*` wrappers.
- `OVT_ControllerRequestComponent` static rule bodies (`ResolveOwningPlayerIdFor`, `PlayerMayUseVehicleFor`) and `OVT_ControllerComponent<T>.Get()`.
- `OVT_ContainerTransferComponent`'s post-BUG-166 validation ladder — copied, not re-derived.
- `OVT_SellableItemScanner` — the only correct full-vehicle enumerator, and the documented warning against the root-only path.
- `OVT_PrefabUtils.GetPrefabName` / `GetItemUIInfo`; `OVT_ShopCategoryHelper` + `OVT_EconomyManagerComponent.GetItemCategory`; `OVT_VehicleRearmUtils`' magazine-fill idiom.
- `modded SCR_InventoryStorageManagerComponent` — the mandatory null guard for inserts into non-character storage. Every new insert path relies on it.

**External:** none. Independent of the `logistics` epic (§3.7), the `occupying` epic and the virtualization work.

**Scheduling:** the tree is `v1.5` with concurrent sessions. Re-check `git status` and the highest BUG id before each phase; expect the tree to have moved.

---

## 10. Risks & Mitigation

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| **R1** | **Item duplication or loss during conversion** — a crash, disconnect or destroyed container between the entity and the ledger. | Medium | **Critical** — the one failure this feature cannot ship with. | Per-item commit in the safe order (D7); every batch tick re-null-checks both ends; play-test item **6** disconnects mid-transfer deliberately and counts. |
| **R2** | **`IsRegisteredResource` applied to a conversion silently deletes loot.** The existing code and its comments push hard toward applying it everywhere. | Medium | **Critical** | D2 is stated in the plan, in §8, and must be repeated as a comment at every gate site. Play-test converts occupying-faction gear specifically and takes it back out. |
| **R3** | **Prefab wiring is only confirmed in Workbench.** Nine prefabs are text-edited (`Vehicle_Base`, 3 trucks, 2 ammo boxes, the controller, the player character) plus two `.conf` files. A GUID collision fails silently. | Medium | High | Reserved GUID series `6B62A0000000xxxx`, verified unused; a `grep` for the new GUID before each insertion; **user-gated Workbench load** at the end of Phases 1, 2, 3 and 4. |
| **R4** | **Sort Priority ordering convention is assumed** (ascending = display order). If inverted, the new Open lands in the wrong place and the renumbering makes it worse. | Medium | Low | Task 3.4 verifies it in Workbench before renumbering, and the table is inverted wholesale if needed. |
| **R5** | **Magazine / weapon-attachment API surprises.** The strip pass is entirely new to Overthrow, and the weapon entity has **no** storage manager of its own (an existing latent bug at `OVT_LoadoutManagerComponent.c:2129` proves the trap is easy to fall into). | Medium | Medium | The exact classes are cited in §3.4 and Phase 2 (`MagazineComponent.GetAmmoCount/GetMaxAmmoCount`, `SCR_WeaponAttachmentsStorageComponent.GetOwnedItems`, `item.FindComponent(BaseMagazineComponent)`); re-`grep` the vanilla reference tree at `/mnt/n/Projects/Arma 4/ArmaReforger/scripts/` during implementation before writing against them. |
| **R6** | **Warehouse migration breaks existing saves.** The most integration-heavy phase touches manager, serializer, replication, controller and UI at once. | Medium | High | Phase 4 runs on an **advanced** agent; the persisted record shape is deliberately unchanged; v1 and v2 both load; an explicit "load a pre-migration save" check is a phase gate, not a nice-to-have. |
| **R7** | **The civilian/offensive vehicle gate misfires** — the economy database is not ready when the first action label is drawn, or `IsLegalVehicle` excludes a vehicle players expect to have storage. | Medium | Medium | Lazy resolution with caching (never at `OnPostInit`); fail-safe to "no storage"; a documented per-prefab-authoring fallback in §3.6 that must be chosen in Phase 1, not carried alongside. |
| **R8** | **FOB latch wedges the session.** `OVT_ResistanceFactionManager` holds a single in-flight latch cleared only from the progress component's complete/error invokers; a new early return that skips `SendOperationError` disables FOB operations for everyone until restart. | Medium | High | Task 5.6 is an explicit audit task with a deliberate-failure verification step. |
| **R9** | **Concurrent sessions on `v1.5`.** Other agents are editing `Configs/`, `Language/` and `Scripts/` on this same tree. | High | Medium | Re-baseline (`git status`, highest BUG id) before each phase; never run the suites while another session is writing `Configs/`/`Language/`; never touch the localization `.conf` exports (they are Workbench build output — `.st` master only). |
| **R10** | **Logistics boundary erosion** — a later reader "unifies" the item ledger with the resource ledger. | Low | Medium | §3.7 states the boundary; `OVT_StorageLedger`'s class comment must state it too; nothing in this feature may be widened for a hypothetical resource consumer. |
| **R11** | **Truck capacity numbers are a guess.** `3000000 / 15000` is derived from the ammo box, not measured. | High | Low | Task 6.5 and play-test item **10** require a measurement; the numbers are tuned from it before the feature closes. |
| **R12** | **Performance does not actually improve.** The feature's entire justification is server FPS and network spikes; none of it is measured by the test spine. | Low | **Critical** | Play-test item **14** measures server FPS during a 300-item conversion and during a save with three full boxes, before and after. If it does not improve, the feature has failed and the plan must be revisited rather than shipped.
