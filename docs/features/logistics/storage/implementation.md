# Storage — Implementation Plan

**Status:** Ready for Review
**Started:** 2026-08-21
**Target Completion:** TBD
**Last Updated:** 2026-08-21 (all 10 phases built + cross-phase review; gates green)

**Epic:** `logistics` (feature #2 of 4 — see `docs/features/logistics/epic-overview.md`)
**Requirements:** `docs/features/logistics/storage/requirements.md` (authoritative for scope; 5 decisions recorded there, none re-opened here)
**Approach:** one **item ledger** on one **`OVT_StorageComponent`**, authored on three shared prefab bases so it reaches every truck, car, ammo box and warehouse building; every holder addressed by **`RplId`**; one **`OVT_StorageRequestComponent`** on `OVT_OverthrowController` carrying a batched wire protocol over a per-player **job engine**; one `logistics/ui` consumer. `OVT_WarehouseData.inventory` and every warehouse RPC are **deleted**, not adapted.
**Branch:** `v1.5` (concurrent sessions exist on this tree — re-baseline before every phase)

---

## 1. Executive Summary

Overthrow stores stockpiles as **spawned entities**. A full ammo box is several hundred replicated entities the engine streams, simulates and serializes; a looted truck is worse. That is the direct cause of the network spikes, the kicked clients and the save-time frame collapse. The mod already knows the answer — the warehouse has been pure data (`map<string,int>`) since forever — but that answer is welded into `OVT_RealEstateManagerComponent` (:495-697), addressed by an array index that each client reconstructs independently (:243, :281, :1021, :1051), broadcast to **every** client (`RpcDo_SetWarehouseInventory`, :988-996) and JIP-streamed in full (:908-971). It cannot travel and it should not have travelled in that shape.

This feature extracts the idea properly. A pure `OVT_StorageLedger` (prefab `ResourceName` → count) lives on an `OVT_StorageComponent` that is authored on exactly **three** shared prefab bases — `Prefabs/Vehicles/Core/Wheeled_Base.et`, `Prefabs/Props/Military/AmmoBoxes/OVT_AmmoBox_Base.et`, and a same-GUID delta of vanilla `Warehouse_01_Base.et` — so every wheeled vehicle spawned by any system, every ammo box and all seven warehouse variants inherit one. Capacity resolves from data the economy already owns (truck → unlimited, legal car → 300, illegal/armed → none). Contents never leave the server except to the one player who asked; only a **count**, a **name** and a **capacity** replicate, as three ordinary `RplProp`s that are free for join-in-progress.

Everything that moves items in bulk is rerouted through one **per-player job engine** on the controller: port import stops spawning up to 100 entities in a frame (`OVT_VehicleRequestComponent.c:476-497`) and becomes a ledger credit; warehouse take stops spawning `qty` entities in a frame (`OVT_RealEstateManagerComponent.c:664-697`) and becomes map arithmetic; box Load/Unload, truck Loot and FOB undeploy move onto the engine with a real progress bar and stop going through `OVT_InventoryManagerComponent`, whose shared search accumulator (`:497`) and single-instance progress state are the defect this replaces. Because "what is in this truck" is finally a cheap question, the port gains an **Export** mode.

Three commitments shape everything below.

1. **One holder, one seam, one vocabulary.** Every holder — box, car, truck, mobile FOB, warehouse building — is an `RplId` with an `OVT_StorageComponent`. No `warehouseId`, no "is this the warehouse?" branch, no second code path. `OVT_WarehouseData` keeps ownership, privacy and location and loses everything else.
2. **The server owns the items; the client owns a snapshot.** Contents are pulled on open by the one player looking, in a `Begin…Line…End` fan framed by a client sequence number (`OVT_GMRequestComponent`'s shipped pattern). Nobody else pays for your open box.
3. **Nothing here is resource-shaped.** Counts, not m³. `logistics/resources` gets its own ledger, its own capacity model and its own record. The epic's "two ledgers, deliberately" is a wall, not a preference.

Net effect on shipped code: `OVT_RealEstateManagerComponent` loses ~200 lines, `OVT_RealEstateRequestComponent` loses ~180, `OVT_ContainerTransferComponent` loses its warehouse verb, `OVT_InventoryManagerComponent` loses its callers, and `OVT_WarehouseContext` is deleted outright.

---

## 2. Goals

### Primary

1. **A reusable item ledger** — pure, world-free, Logic-tier testable, with O(1) total (action labels poll it), keyed by prefab `ResourceName` and never by the economy's integer id.
2. **One component on every holder that should have one**, reached through three shared prefab bases and no runtime component creation (there is none in EnforceScript — `IEntity` exposes `FindComponent` only).
3. **A server-only conversion engine** that is batched, progress-bar-driven, one operation per player, and loses or duplicates at most **one** item if the server dies mid-transfer.
4. **Open Storage** — a `logistics/ui` consumer with one Take mode where the only variable is the destination (this holder's own vanilla inventory, or any nearby named holder), plus "Transfer all to storage", "Rename" and an officer-only "Clear inventory".
5. **The two worst spike sites deleted** — port import and warehouse take become zero-spawn ledger operations.
6. **Port Export** — sell a truck's ledger at the port, priced below every shop buy price so there is no shop→port loop.
7. **Contents off the wire** — count + name + capacity replicate; contents are pulled on open by the opening client only.
8. **Everything survives save/load** — ledger and name round-trip per instance; old saves' warehouse stock migrates once into the building's component.

### Secondary

1. **Delete the warehouse's second vocabulary.** Array-index ids reconstructed independently per client (`OVT_RealEstateManagerComponent.c:1003-1056`) are a latent desync; they go.
2. **Kill `isLinked`** — never set true anywhere, read in five places, occupying a slot in the v1 save payload.
3. **Retire `OVT_InventoryManagerComponent`'s bulk paths** and their shared-accumulator concurrency bug, without deleting the class (its `OVT_StorageProgressUIContext` cancel path and the vehicle-upgrade transfer stay until someone needs them gone).
4. **Hide vanilla's supply "Storage"** globally so two storage systems are not on screen at once.

### Explicitly out of scope

Everything in the requirements' Out of Scope section, restated only where an implementer might reach for it anyway:

- **No resource ledger, no volume, no weight, no m³.** Not a field, not a branch, not a "reserved" serializer slot. `resources` builds its own.
- **No Put mode and no per-item put.** Putting in is the "Transfer all to storage" action; moving between holders is Take with a holder destination.
- **No change to the player's own character inventory** or to the vanilla inventory UI. The vanilla inventory is never removed from any holder — it stays how a character physically accesses items.
- **No warehouse linking, purchase, pricing or rent changes.**
- **No selling anywhere except the port**; no player-to-player trading; no AI/recruit access to storage.
- **No new `OVT_TransferContext` hook.** §3.7 shows all four consumers landing on the shipped eight. If an implementer finds one that does not, that is a plan defect — raise it, do not widen the base.

---

## 3. Architecture Overview

### 3.1 Where each piece lives

```
Scripts/Game/Data/                                    PURE, Logic-tier testable, no world
├── OVT_StorageLedger.c              NEW   ledger + OVT_StorageLine
└── OVT_StorageRules.c               NEW   capacity resolution, item predicates, export pricing (statics)

Scripts/Game/Components/
├── OVT_StorageComponent.c           NEW   the holder: ledger + capacity + name + 3 RplProps
└── Controller/
    ├── OVT_StorageRequestComponent.c    NEW   : OVT_BaseServerProgressComponent — wire + engine
    ├── OVT_RealEstateRequestComponent.c REWRITE  −3 client methods, −3 RpcAsk, −2 validators
    └── OVT_ContainerTransferComponent.c REWRITE  −TransferToWarehouse, loot/undeploy rerouted

Scripts/Game/Utilities/
└── OVT_StorageUtils.c               NEW   RplId↔holder, radius query (per-call accumulator)

Scripts/Game/GameMode/Managers/
├── OVT_RealEstateManagerComponent.c REWRITE  warehouse inventory surface deleted; migration queue
└── OVT_InventoryManagerComponent.c  UNTOUCHED (callers removed in Phase 8; class survives)

Scripts/Game/Components/Controller/OVT_VehicleRequestComponent.c
└── RpcAsk_ImportToVehicle → ledger credit, zero spawns

Scripts/Game/UserActions/
├── OVT_OpenStorageMenuAction.c      NEW   "Storage (1,123 items)"
├── OVT_TransferAllToStorageAction.c NEW
├── OVT_RenameStorageAction.c        NEW
├── OVT_ClearVanillaInventoryAction.c NEW  officer-only
├── OVT_LoadStorageAction.c          REWRITE  (Load + Unload, both storage-only)
└── OVT_LootIntoVehicleAction.c      REWRITE  onto the job engine

Scripts/Game/UI/Context/
├── OVT_StorageContext.c             NEW   OVT_TransferContext consumer (Take, 8 hooks)
├── OVT_PortContext.c                TOUCH   + Export mode
├── OVT_WarehouseContext.c           DELETED (its entry point opens OVT_StorageContext)
└── OVT_VehicleMenuContext.c         REWRITE  warehouse buttons retargeted at the holder

Scripts/Game/UI/{HUD,Map}/
├── OVT_EconomyInfo.c                TOUCH   warehouse prompt reads the building's component
└── Map/LocationTypes/OVT_MapLocationWarehouse.c  REWRITE  contents rows → count + name only

Scripts/Game/GameMode/Services/OVT_RespawnService.c   TOUCH  (warehouse skip at :379)

Scripts/Game/Persistence/Serializers/Components/
├── OVT_StorageComponentSerializer.c NEW   version 1, one array record
└── OVT_RealEstateManagerSerializer.c REWRITE  version 2 + v1 inventory migration

Prefabs/
├── Vehicles/Core/Wheeled_Base.et            + OVT_StorageComponent (AUTO)
├── Vehicles/Core/Vehicle_Base.et            + 3 actions, SellCargo 101→102
├── Vehicles/Wheeled/M923A1/OverthrowMobileFOB.et  + OVT_StorageComponent (UNLIMITED)
├── Vehicles/Wheeled/{M923A1,Ural4320}/*_transport.et  vanilla cap raised
├── Props/Military/AmmoBoxes/OVT_AmmoBox_Base.et  + component, + 4 actions, renumbered
├── Structures/Industrial/Houses/Warehouse_01/Warehouse_01_Base.et  NEW same-GUID delta
├── GameMode/OVT_OverthrowGameMode.et        + m_aDisabledResourceTypes { 0 }
├── GameMode/OVT_OverthrowController.et      + OVT_StorageRequestComponent
└── Characters/.../Character_Player.et       + OVT_StorageContext, − OVT_WarehouseContext

Configs/Systems/Persistence/Overthrow.conf   3 serializer bindings (§3.9)
Language/localization_Overthrow.st           ~25 new keys + 2 value edits

Scripts/Game/Tests/TestSuites/
├── Logic/OVT_TEST_Logic_StorageLedger.c     NEW
├── Logic/OVT_TEST_Logic_StorageRules.c      NEW
├── Init/OVT_TEST_Init_StorageSeam.c         NEW   (controller component + holder capacities)
└── Persistence/OVT_TEST_PersistenceRoundTripSuite.c  + 3 cases
```

**Reserved GUID series: `6A8E2D…`.** Verified unused 2026-08-21 — `grep -rl 6A8E2D` returns **0 files** in `Overthrow.Arma4` and **0** in the extracted `ArmaReforger` tree. Allocation: `6A8E2D0…` prefab component/action instance GUIDs, `6A8E2D1…` `Character_Player.et` context block, `6A8E2D2…` spare. Persistence-config entries follow that file's own convention instead and take `6B0E7A6…` (also verified unused, 0 hits). `.st` entries get their own fresh GUIDs. **Inherited component GUIDs are copied, never minted** — `Vehicle_Base.et`'s `ActionsManagerComponent "{C97BE5489221AE18}"` is the inherited one already re-declared in the shipped delta, and the ammo box's is `"{5992E2EE5A9EEC18}"`.

### 3.2 The ledger — `OVT_StorageLedger : Managed`

Pure. No world, no manager, no engine type in the signature. Capacity is **passed in**, never held — the component owns capacity, which is what keeps this class Logic-testable and keeps a future resource ledger from inheriting an item-shaped cap.

| Method | Contract |
|---|---|
| `int Add(string res, int qty, int capacity)` | Returns how many **fitted**. `capacity < 0` = unlimited. Ignores empty `res` and `qty <= 0`. |
| `int Take(string res, int qty)` | Returns how many were **taken**. Clamps to what is held. **A line that reaches zero is removed** — the map never accumulates zero entries (today's `DoTakeFromWarehouse` floors at 0 and keeps the key, `:576-588`). |
| `int Count(string res)` | 0 when absent. |
| `int Total()` | **O(1)** — a maintained `m_iTotal`, because action labels poll it every frame. |
| `int FreeSpace(int capacity)` | `capacity < 0` → `int.MAX`; else `Math.Max(0, capacity - Total())`. |
| `int LineCount()` / `void GetLines(out array<string> res, out array<int> counts)` | Enumeration for the fan, the serializer and the UI. |
| `void Clear()` | |
| `ref ScriptInvoker m_OnChanged` | `(string res, int newQty)`. Lazily allocated. |

`OVT_StorageLine : Managed { string m_sRes; int m_iCount; }` is the enumeration/serialization record.

**Keys are prefab `ResourceName` strings, never economy ids.** `GetInventoryId()` resolves an unregistered prefab to index `0` — i.e. some other item's identity (`OVT_EconomyManagerComponent.c:1900-1913`, trap documented in place) — and looted occupying-faction gear is routinely unregistered.

⚠️ `array.Remove()` is swap-with-last. The ledger is map-backed so it is not exposed there, but the job engine's line arrays are — use `RemoveOrdered` anywhere order is observable.

### 3.3 The rules — `OVT_StorageRules` (statics, pure)

Everything that is a decision rather than a lookup, so the Logic tier can assert it without a world.

```
static int ResolveAutoCapacity(bool isVehicle, bool isRegistered, bool isLegalVehicle,
                               OVT_ParkingType parking, int defaultVehicleCapacity)
static bool MagazineIsFull(int ammoCount, int maxAmmoCount)
static bool IsBaseClothingArea(typename areaType)
static int  ExportUnitPrice(int importPrice, float ratio, int minShopBuyPrice)
static bool HolderIsInRange(vector holderPos, vector playerPos, float radius)
```

- `ResolveAutoCapacity`: not a vehicle → `-1` (unlimited: boxes and the warehouse building take AUTO and land here). Vehicle and **not registered** → `0` **and the caller logs an ERROR once per prefab** (silently granting capacity to an unknown prefab is the worse failure; a silent 0 is undiagnosable). Vehicle, registered, `!isLegalVehicle` → `0` (illegal/armed). `PARKING_TRUCK` → `-1`. Otherwise → `defaultVehicleCapacity` (**300**, decision 4). Backing data: `OVT_EconomyManagerComponent.IsVehicle(ResourceName)` `:1780`, `IsRegisteredResource` `:1913`, `IsLegalVehicle(int)` `:382`, `GetParkingType(int)` `:372`.
- `MagazineIsFull` is `ammoCount == maxAmmoCount` — the inverse of `OVT_VehicleRearmUtils.AnyAmmoMissing`'s test (`:129-141`). Part-used magazines never convert; they stay in the vanilla inventory.
- `IsBaseClothingArea` compares `typename` against `LoadoutJacketArea`, `LoadoutPantsArea`, `LoadoutBootsArea` — **there is no `LoadoutShirtArea`; the torso garment is `LoadoutJacketArea`**. Compare `cloth.GetAreaType().Type() == LoadoutJacketArea`, **not** `ClassName()` string equality: the shipped `LootBodyItems` (`OVT_InventoryManagerComponent.c:796-838`) compares class-name strings and two of its eight names do not exist in vanilla.
- `ExportUnitPrice` = `Math.Min(Math.Round(importPrice * ratio), minShopBuyPrice - 1)`, floored at 1. `minShopBuyPrice < 0` means "sold at no shop" (illegal items) — then the ratio stands alone. Caller supplies `economy.GetBuyPrice(id, portPosition, -1)` (`:734-746`, `playerId = -1` so no per-player multiplier leaks in) and `economy.GetPrice(id)` (`:352-356`, which is exactly what import charges — `OVT_VehicleRequestComponent.c:476`). Default ratio **0.5**.

### 3.4 The holder — `OVT_StorageComponent : OVT_Component`

```
[ComponentEditorProps(category: "Overthrow/Components")]
class OVT_StorageComponentClass : OVT_ComponentClass {};
class OVT_StorageComponent : OVT_Component
```

| Member | Kind | Notes |
|---|---|---|
| `m_eCapacityMode` | `[Attribute]` `EOVT_StorageCapacityMode { AUTO, UNLIMITED, FIXED, NONE }` | Default **AUTO**, so `Wheeled_Base.et` authors nothing but the component. |
| `m_iFixedCapacity` | `[Attribute]` int | Read only in `FIXED`. |
| `m_iAutoVehicleCapacity` | `[Attribute]` int, default **300** | Decision 4, tunable in the Workbench on `Wheeled_Base.et`. |
| `m_sDefaultNameKey` | `[Attribute]` string | Fallback when the prefab has no UIInfo (`"#OVT-Warehouse"` on the warehouse delta). |
| `m_Ledger` | `ref OVT_StorageLedger` | **Server-only content.** Non-null on clients but always empty; never read there. |
| `[RplProp(onRplName: "OnCountChanged")] m_iTotalCount` | int | The label's number and the live-refresh trigger. |
| `[RplProp()] m_sCustomName` | string | Empty = use the resolved default. |
| `[RplProp()] m_iCapacity` | int | The **resolved** capacity. Replicated so no client ever re-runs AUTO. |

Server API: `GetLedger()`, `GetCapacity()`, `SetCustomName(string)`, `PublishCount()`, `ApplyPersisted(...)`. Client-safe: `GetTotalCount()`, `GetCapacity()`, `GetDisplayName()`, `m_OnCountChanged` invoker.

- **`Replication.BumpMe()` is called at batch boundaries, never per item.** `PublishCount()` writes `m_iTotalCount` from `m_Ledger.Total()` and bumps once; the job engine calls it exactly once per holder per finished job. Bumping per item would replace one spike with another.
- **Capacity resolution is server-side and deferred.** `OnPostInit` schedules a resolve (the economy's vehicle catalogue is built during init and is not ready at component post-init); resolve sets `m_iCapacity` + one `BumpMe`. Retry once per second, ten times, then log an ERROR naming the prefab. Clients never resolve.
- **`GetDisplayName()` chain:** custom name → prefab UIInfo name (`OVT_PrefabUtils.GetItemUIInfo`, the memo pattern `OVT_WarehouseContext` already uses) → `m_sDefaultNameKey` → prefab file stem.
- **`OnPostInit` asserts `GetRpl()` is non-null** and logs an ERROR naming the prefab if not. Every planned host has one (`Building_Base.et` carries `RplComponent "{50A4E7C9B5728062}"`, `OVT_AmmoBox_Base.et:80`, vanilla `Vehicle_Base.et`), but BUG-193 is exactly this failure discovered late.
- **Rename permission is "anyone who may open the holder"** (requirements default) — enforced server-side by the same `MayUseHolder` gate as everything else (§3.6), never client-side only.

### 3.5 Holder addressing and lookup — `OVT_StorageUtils`

Every holder is an **`RplId`**. There is no second vocabulary.

```
static IEntity ResolveHolder(RplId id)                     // Replication.FindItem → RplComponent.GetEntity
static OVT_StorageComponent GetStorage(IEntity e)          // OVT_ComponentFinder<OVT_StorageComponent>.Find
static RplId GetHolderId(IEntity e)
class OVT_StorageHolderQuery : Managed                     // one instance per call
{
    ref array<IEntity> m_aResults;
    int Run(vector pos, float radius, out array<IEntity> results);
    protected bool FilterHolders(IEntity e);               // has component AND GetCapacity() != 0
}
```

⚠️ **The accumulator is per-instance, never static.** `OVT_InventoryManagerComponent.m_aContainerSearchResults` (`:497`) is a singleton member shared by every concurrent search — one of the defects this feature exists to stop repeating. `new` a query object per call, both on the server (validation) and on the client (destination picker).

### 3.6 The request component and the wire — `OVT_StorageRequestComponent`

`class OVT_StorageRequestComponent : OVT_BaseServerProgressComponent`, authored on `Prefabs/GameMode/OVT_OverthrowController.et` (append before the trailing `RplComponent`, following the file's one-entry-per-component shape).

**It is not an `OVT_ControllerRequestComponent`,** so it does not inherit `ResolveOwningPlayerId` / `ResolveEntity` / `ShouldRespondLocally`. Follow `OVT_ContainerTransferComponent`, the only other `OVT_BaseServerProgressComponent` subclass: caller identity via the **static** `OVT_ControllerRequestComponent.ResolveOwningPlayerIdFor(GetOwner())` (`:46-82`, `-1` = reject) and owner replies via the inherited `IsLocalPlayerOwner()` (`OVT_BaseServerProgressComponent.c:185-197`), which is the `ShouldRespondLocally` equivalent on this branch. Identity is **never** an RPC parameter.

**Attributes (authored on the controller prefab):** `m_fHolderRadius` (destination picker, default **25 m**), `m_fUseRadius` (caller must be within this of the holder, default **30 m** — `OVT_ContainerTransferComponent.TRANSFER_MAX_DISTANCE`), `m_fExportPriceRatio` (default **0.5**), `m_iMaxCartLines` (default **64**), `m_iItemsPerChunk` (default **5**), `m_iChunkDelayMs` (default **50**).

#### Client → server (`[RplRpc(RplChannel.Reliable, RplRcver.Server)] protected`)

| # | Signature | Arity | Meaning |
|---|---|---|---|
| 1 | `RpcAsk_OpenStorage(RplId holder, int seq)` | 2 | Pull-on-open. Server fans the contents to this player only. |
| 2 | `RpcAsk_BatchBegin(RplId source, RplId dest, int opKind, int seq, int lineCount)` | 5 | Opens a checkout. `opKind` = `EOVT_StorageOp`. `TO_INVENTORY` requires `dest == source`. |
| 3 | `RpcAsk_BatchLine(int seq, int index, string res, int qty)` | 4 | One cart line. |
| 4 | `RpcAsk_BatchCommit(int seq, int lineCount)` | 2 | `lineCount` repeated so the server can detect a short stream. |
| 5 | `RpcAsk_TransferAllToStorage(RplId holder)` | 1 | Vanilla inventory → this holder's ledger. |
| 6 | `RpcAsk_MoveAllToHolder(RplId source, RplId dest, bool sweepFirst)` | 3 | Whole-ledger move; `sweepFirst` runs #5 on the source first. |
| 7 | `RpcAsk_ClearVanillaInventory(RplId holder)` | 1 | Officer-only; empties the **vanilla** inventory, not the ledger. |
| 8 | `RpcAsk_RenameHolder(RplId holder, string name)` | 2 | 1–32 chars (recruit-rename precedent, `OVT_RecruitRequestComponent.c:279`). |

#### Server → owner (`[RplRpc(RplChannel.Reliable, RplRcver.Owner)] protected`)

| # | Signature | Arity |
|---|---|---|
| 9 | `RpcDo_ContentsBegin(RplId holder, int seq, int lineCount, int wireVersion)` | 4 |
| 10 | `RpcDo_ContentsLine(int seq, string res, int qty)` | 3 |
| 11 | `RpcDo_ContentsEnd(int seq)` | 1 |
| 12 | `RpcDo_StorageError(int seq, string messageKey)` | 2 |
| 13 | `RpcDo_BatchResult(int seq, int moved, int shortfall, int earned)` | 4 |

Progress reuses the inherited `RpcDo_UpdateProgress` / `OperationStart` / `OperationComplete` / `OperationError` — **no new progress RPCs**, so the shipped `OVT_ProgressInfo` HUD lights up for free.

**Rules that are not negotiable:**
- **No `array<...>` on any RPC.** Both directions are `Begin…Line…End` fans, exactly like `OVT_GMRequestComponent.RequestSnapshot` (`:400-414`) / `SendSnapshotBegin` (`:784`).
- **Every `Rpc()` call site is hand-audited against its handler.** `Rpc()` is an untyped variadic proto — a wrong arg count compiles clean and dies on the wire (BUG-090). Never wrap a `Rpc()` call in a helper.
- **Two independent sequence counters** on the client — one for the contents pull, one for the checkout. Independent fans get independent seqs (`OVT_GMRequestComponent.c:50-53`). A reply carrying a superseded seq is discarded; an empty answer is still an answer (`:55-59`).
- **Every `RpcDo_*` goes through the `IsLocalPlayerOwner()` direct-call branch first**, then `Rpc()` only when not the local owner. Owner RPCs on a listen host are silently dropped (BUG-090).
- `wireVersion` is a constant on both sides; a mismatch discards the fan and logs once.

#### `MayUseHolder(int playerId, IEntity holder)` — the single server gate

Called by every one of the eight asks. Dispatches on what the holder carries, then always checks distance:

1. `playerId <= 0` → reject.
2. Holder resolves, has an `OVT_StorageComponent`, `GetCapacity() != 0`.
3. Caller within `m_fUseRadius` of the holder (`OVT_ContainerTransferComponent.CallerIsWithin`, `:519`).
4. **Vehicle** (`Vehicle.Cast(holder)`) → `PlayerMayUseVehicle` (the gate `RpcAsk_TakeFromWarehouseToVehicle` uses, `:481-512`).
5. **`OVT_PlayerOwnerComponent` present** → locked ⇒ owner only (mirrors `OVT_OpenStorageAction.CanBePerformedScript:35-71` and `OVT_LoadStorageAction:98-109`).
6. **Real-estate warehouse** (`OVT_RealEstateManagerComponent.GetConfig(holder).m_IsWarehouse`) → the shipped accessibility rule `(!isPrivate && isOwned && !isRented) || (isPrivate && isOwner && !isRented) || isRented` (today inline at `OVT_VehicleMenuContext.c:65`) — **lift it to a method on the real-estate manager in Phase 7** so the client button and the server gate cannot drift.
7. **Ruined** → `OVT_StructureDamage.IsUsable(holder)` false ⇒ reject (the actions hide too, §3.10).

Rejections answer with `RpcDo_StorageError(seq, key)` — never a silent return. `RejectTransfer`'s log-only shape (`OVT_ContainerTransferComponent:577`) is the thing being improved on.

### 3.7 The job engine

One job per player at a time, on the per-player controller — never the shared `OVT_InventoryManagerComponent` singleton. `m_bIsRunning` (inherited) is the latch; a second request answers `#OVT-Storage_Busy`.

```
class OVT_StorageJob : Managed
{
    int m_iPlayerId, m_iSeq;
    EOVT_StorageOp m_eOp;            // TO_INVENTORY, TO_HOLDER, TO_STORAGE(sweep), EXPORT, CLEAR, LOOT
    RplId m_SourceId, m_DestId;
    ref array<string> m_aRes;  ref array<int> m_aQty;      // parallel; RemoveOrdered only
    ref array<IEntity> m_aPending;                          // entity work list (sweep / clear / loot)
    int m_iCursor, m_iMoved, m_iShortfall, m_iEarned, m_iTotalUnits;
    ref OVT_StorageJob m_NextJob;                           // chaining (sweepFirst, undeploy)
}
```

**State machine** — `VALIDATE → RUN → (STEP)* → FINISH | ABORT`.

- **VALIDATE** (synchronous, on Commit / on the single-shot asks): resolve both holders; `MayUseHolder` on both; clamp every line to `ledger.Count(res)` — **ledger membership is the take-out gate**, which is strictly stronger than `IsRegisteredResource` (a line can only exist because the server deleted a real entity) and **replaces the registry check on the warehouse take path** (`OVT_RealEstateRequestComponent.c:550`) so converted loot is not trapped; reject the whole batch on any failure. Paths that **mint** stock from a client-chosen name — only port import — keep `IsRegisteredResource`.
- **RUN**: `StartOperation(progressKey)`, then `CallLater(Step, m_iChunkDelayMs, true)`.
- **STEP** re-checks liveness first: both holders alive, player still connected. Either failing → **ABORT** at the batch boundary, leaving both ledgers consistent.

| Op | Per-unit order | Chunked? |
|---|---|---|
| `TO_HOLDER` | `src.Take(res, n)` → `dst.Add(res, n, cap)`; un-added remainder goes **back** to the source and counts as shortfall | No — pure map arithmetic, one step |
| `TO_INVENTORY` | `TrySpawnPrefabToStorage(res, null, -1, PURPOSE_ANY, null, 1)` → **on success** `ledger.Take(res,1)` (**spawn-then-debit**); first refusal ends that line and adds its remainder to shortfall | Yes, `m_iItemsPerChunk` |
| `TO_STORAGE` (sweep) | capacity check → `manager.TryDeleteItem(item)` → **on success** `ledger.Add(prefab,1,cap)` (**delete-then-credit**, and the capacity check precedes the delete so a full ledger can never eat an item) | Yes |
| `EXPORT` | `ledger.Take(res,n)` → `economy.DoAddPlayerMoney(playerId, n * unitPrice)` | No |
| `CLEAR` | `manager.TryDeleteItem(item)` | Yes |
| `LOOT` | body/weapon → items into the truck's **vanilla** inventory | Yes |

- `SendProgressUpdate(progress, processed, total, key)` once per chunk.
- **FINISH**: `PublishCount()` on each touched holder (**one BumpMe each**), `Track()` the holder if it is an untracked building (§3.9), `SendOperationComplete(moved, shortfall)`, `RpcDo_BatchResult(seq, moved, shortfall, earned)`, then start `m_NextJob` if present.

**Sweep enumeration rules (B):**
- Enumerate with the **right manager**: `SCR_VehicleInventoryStorageManagerComponent.GetItems` when present (it walks the truck-bed child storages), otherwise `SCR_InventoryStorageManagerComponent.GetItems`. The root-only `GetOwnedItems()` path misses truck beds — `OVT_SellableItemScanner.c:77-106` documents the trap and `OVT_InventoryManagerComponent.c:381-382` is the code that falls into it.
- **Weapons are stripped first**: current magazine (`BaseWeaponComponent.GetCurrentMagazine`) and every attachment (`GetAttachments` → `WeaponAttachmentsStorageComponent`) are removed **through the holder's inventory manager** (`TryRemoveItemFromStorage(attached, attachmentsStorage, null)`) — a weapon entity has no manager of its own — and appended to the work list as separate items.
- **Part-used magazines are skipped**, `OVT_StorageRules.MagazineIsFull`. They stay in the vanilla inventory; the officer "Clear inventory" action is how they are discarded.
- **Unregistered prefabs are credited**, no registry gate. The string came from an entity the server just deleted; gating would delete the player's loot.

### 3.8 The screen — `OVT_StorageContext` and the eight hooks

A `logistics/ui` consumer. `OVT_TransferContext`'s hook list is **closed** and all four consumers fit it. Opened by `SetHolder(IEntity)` then `ShowContext(OVT_StorageContext)`, exactly as `OVT_VehicleMenuContext:159-167` does with `SetWarehouse`; user actions open contexts this way today (`OVT_ShopAction.c:45`, `OVT_LoadLoadoutAction.c:20`).

| Hook | `OVT_StorageContext` (Open Storage) | `OVT_PortContext` Export mode (added) |
|---|---|---|
| `BuildEntries(mode, model)` | From the **staged snapshot** of the pull. If no snapshot for this holder yet, fire `RpcAsk_OpenStorage` (latched) and return empty. `QUANTITY`, `m_iValue = m_iMaxQuantity = count`, `PREFAB` image | Same, over the **occupied vehicle**'s snapshot; `PRICE`, value = `OVT_StorageRules.ExportUnitPrice(...)` |
| `BuildModes` | one mode, `#OVT-Storage_Take` | two: `#OVT-Import` (0), `#OVT-Export` (1) — the mode toggle appears for the first time |
| `GetCategoryLabelKey` | `""` (single category) | unchanged shop mapping |
| `BuildDestinations` | **This holder's inventory** (`m_sId "inventory"`, `m_Entity` = the holder) **+ every nearby holder** from `OVT_StorageHolderQuery(m_fHolderRadius)` minus self, labelled `GetDisplayName()`. **First consumer in the mod with ≥ 2 destinations** | Import: occupied vehicle. Export: occupied vehicle |
| `FillDetails` | name, count, `""` | name, unit price, `#OVT-Export_Body` |
| `OnAccept(lines, dest)` | `BatchBegin(holder, dest, opKind, seq, n)` → one `BatchLine` per line → `BatchCommit`. **One checkout, not one request per line** — this is what supersedes `ui` D10, and `OnAccept` was designed as its single seam | Import unchanged; Export → `BatchBegin(vehicle, vehicle, EXPORT, …)` |
| `IsAddAllAllowed` | `true` | Import `false`, Export `true` |
| `ValidateCart(lines, dest)` | empty cart / no destination → `#OVT-Storage_NoDestination`. **Fit is not validated client-side** — the server stops early and reports shortfall (requirement B) | Export: must be at a port and hold the illegal gate |

**Async open, zero base changes.** `BuildEntries` runs synchronously inside `Refresh()`, which is `public` (`OVT_TransferContext.c:430`), so the arrival of `RpcDo_ContentsEnd` simply calls `Refresh()` again. The first frame shows an empty list, covered by `ShowPersistentMessage("#OVT-Storage_Loading")` (`:1652`) cleared on the first snapshot. Firing the pull from inside `BuildEntries` is a deliberate side effect, latched and idempotent per (holder, seq) — the alternative, an `OnModeChanged` hook, would widen a list the epic declared closed for a one-line saving.

**Live refresh** is driven by the holder's replicated count: `m_OnCountChanged` → if this screen is open on that holder, coalesce a re-pull at **250 ms** (between the warehouse's 50 ms local invoker and the shop's 400 ms `TRANSACTION_RECHECK_MS`; a network round trip is in between). The cart survives via the base's `Reconcile`.

**`OVT_WarehouseContext` is deleted.** With the warehouse a holder like any other, a warehouse-only subclass would be `OVT_StorageContext` with a different way of finding the same component. `OVT_VehicleMenuContext.TakeFromWarehouse()` calls `SetHolder(building)` + `ShowContext(OVT_StorageContext)`; `PutInWarehouse()` becomes `MoveAllToHolder(vehicle, building, sweepFirst: true)` so the one-button "dump the truck into the warehouse" flow survives verbatim. Both buttons keep their existing visibility rules.

### 3.9 Persistence

**`OVT_StorageComponentSerializer : ScriptedComponentSerializer`**, `GetTargetType()` → `OVT_StorageComponent`, version-first positional payload (`OVT_PlayerOwnerComponentSerializer` / `OVT_BuildableComponentSerializer` are the templates).

```
Serialize:  context.WriteValue("version", 1);
            string customName = comp.GetCustomName();                  context.Write(customName);
            array<ref OVT_PersistedStorageLine> lines = <from ledger>;  context.Write(lines);
Deserialize: int version; context.ReadValue("version", version); if (version < 1) return true;
            string customName;                                          if(!context.Read(customName)) → abort+ERROR
            array<ref OVT_PersistedStorageLine> lines = new …;          if(!context.Read(lines))      → abort+ERROR
            comp.ApplyPersisted(customName, lines);
```

⚠️ **`SaveContext.Write(x)` / `LoadContext.Read(y)` key each property by the LOCAL VARIABLE'S NAME, not by position** (measured 2026-08-20; the note is written verbatim in `OVT_JobManagerSerializer.DeserializeVersion2`, `:410-412`, and `DeserializeVersion1`, `:437-441`). Consequences, all mandatory:
- Serialize and Deserialize locals **must be named identically**. A mismatch silently reads zeros and returns success.
- **A per-entry field loop is impossible** — writing `prefab` N times writes ONE property. The ledger is serialized as **one `array<ref OVT_PersistedStorageLine>`** (`{ string prefab; int count; }`), the `OVT_PersistedJobV2` / `OVT_PersistedLoadoutItem` idiom; the array carries its own count. The existing `OVT_PersistedWarehouse`'s parallel `itemIds` / `itemCounts` arrays are the shape being retired.
- **Every `Read()` return is checked.** A failed read leaves the destination non-null and empty; applying that over live state means "this box is empty now". Abort the payload, log ERROR, leave the component alone.
- **Never `array<bool>`** — use `array<int>` 0/1. (Not needed here; the rule applies to the v2 warehouse record too.)

**Capacity is deliberately not persisted.** It is prefab data resolved from economy data; re-deriving it on load means a retuned prefab or price config applies to old saves, whereas a persisted copy would freeze it forever. Requirement E's "capacity overrides" are the *prefab* overrides, which persist by being in the prefab. Flagged for the user to overrule (§5 D8).

**Three bindings in `Configs/Systems/Persistence/Overthrow.conf`, and never a new rule.** An entity gets exactly **one** `EntityPersistenceConfig`; a `ComponentClassPersistenceConfigRule "OVT_StorageComponent"` would hijack vehicles, boxes and buildings away from their existing configs. Append the serializer to the configs that already match each host:

| Host | Config | Edit |
|---|---|---|
| Wheeled vehicles | CAR `{64C6B4937723DA61}` (Overthrow.conf:97-120, matches vanilla `Wheeled_Base {62F416029692CE40}`) | append `OVT_StorageComponentSerializer "{6B0E7A60…}" { }` to `ComponentSerializers` |
| Placed ammo boxes | Placeable `{6B0E7A215A7FD39C}` (:157-173) | append `"{6B0E7A61…}"` |
| Warehouse buildings | vanilla Building `{65B682661F79DDBE}` | **new override block** under the existing `PersistenceConfigGroup Structures` (:147-153), restating `Collection "{65B4DD18C4F30AC9}"` and adding `ComponentSerializers { OVT_StorageComponentSerializer "{6B0E7A62…}" { } }`. Same-GUID conf overrides are deltas — the group's existing `{6968896F29096D64}` entry restates only its `Collection`, which is the shape to copy |

Helicopters get no storage, so the HELI config `{64EE8D74EB8192BA}` is deliberately **not** touched. Bare `OVT_AmmoBox_Base` / `_Cache` match vanilla's `StorageHolder.conf` and are not covered — accepted: only *placed* boxes are player stockpiles.

**Explicit `Track` of the warehouse building.** Intact buildings are **not tracked by default** — vanilla only registers one when it is destroyed (`SCR_DestructibleBuildingComponent.GoToDestroyedState:1336-1339`). So the first time a building holder's ledger becomes non-empty or it is renamed, the server calls `OVT_PersistenceTracking.Track(owner)` behind an `IsTracked` check (`OVT_VehicleManagerComponent.c:1138-1140` is the ask-first precedent). Never untrack.

**Warehouse migration — `OVT_RealEstateManagerSerializer` version 2.**

- v2 payload: `version 2` → `ownedRecords` → `rented` → `warehouses` as `array<ref OVT_PersistedWarehouseV2 { vector location; string owner; bool isPrivate; }>`. `isLinked` and the two inventory arrays are gone.
- Load: `version < 2` reads the **frozen** v1 `OVT_PersistedWarehouse` class into a local **named `warehouses`** — the v1 writer's local name is part of the format — and hands its `itemIds`/`itemCounts` to `OVT_RealEstateManagerComponent.QueueWarehouseMigration(location, itemIds, itemCounts)`.
- The migration queue drains server-side: for each entry, `GetNearestBuilding(location, 10)` (the manager's own 10 m matching tolerance), find its `OVT_StorageComponent`, `Add` every line at unlimited capacity, `PublishCount()`, `Track()`. Entries that find no building are retried on a 1 s `CallLater` up to ten times, then logged as ERROR with the location and dropped — **never silently**.
- The queue lives on the real-estate manager, not a new manager: it is the component that owned the data, and deleting it later is a one-file deletion.
- Old saves still load; new saves never write v1.

### 3.10 Actions

| Action | Class | Hosts | Sort Priority | Gate |
|---|---|---|---|---|
| Storage (N items) | `OVT_OpenStorageMenuAction` | box, warehouse, `Vehicle_Base` doors | box **1**, vehicle **101** | `MayUseHolder` client mirror + capacity ≠ 0 + `IsUsable` |
| Transfer all to storage | `OVT_TransferAllToStorageAction` | box, `Vehicle_Base` doors | box **2**, vehicle **103** | as above + holder has a vanilla inventory |
| Rename storage | `OVT_RenameStorageAction` | box, warehouse, vehicle doors | box **3**, vehicle **104** | as above |
| Clear inventory | `OVT_ClearVanillaInventoryAction` | box only | **4** | + `OVT_ResistanceFactionManager.IsOfficer` (`:484-490`) |
| Load Storage / Unload Storage | `OVT_LoadStorageAction` (rewritten) | box | **5**, **6** | unchanged nearest-vehicle + driver-must-exit |

- **Renumbering.** Ammo box today is OpenStorage 0 / Load 1 / Unload 2 (`OVT_AmmoBox_Base.et:39-79`); "immediately after the vanilla Open" needs an integer between 0 and 1, so Load/Unload move to 5/6. On vehicles the vanilla trunk action is sort 100 and `OVT_SellVehicleCargoAction` is 101 (`Vehicle_Base.et`), so Storage takes **101** and Sell moves to **102** — one changed number on a shipped action.
- **The vehicle actions go on `Vehicle_Base.et`, not `Wheeled_Base.et`**, even though the component goes on `Wheeled_Base.et`. `Vehicle_Base.et` already re-declares the inherited `ActionsManagerComponent "{C97BE5489221AE18}"` and owns the Overthrow `additionalActions` array; re-declaring that component on a *second* prefab in the same chain risks replacing that array rather than extending it. Helicopters are handled by the runtime gate (no component ⇒ hidden), not by prefab placement.
- **The warehouse building has no `ActionsManagerComponent`** (neither does `Building_Base.et`), so the delta adds a fresh one — the proven precedent is `Prefabs/Structures/Industrial/Garages/Garage_E_02/Garage_E_02.et`, which adds `ActionsManagerComponent "{6B70D0000000002C}"` with a `UserActionContext` at `Offset 0 1 0` on a `SCR_DestructibleBuildingEntity`. Warehouse_01 is ~40 m long, so the context uses **Radius 20** (Garage uses 8) — a play-test tuning item, §6 step 14.
- **Dynamic labels**: `GetActionNameScript` returns `"#OVT-Storage_Open (N items)"` from the component's replicated count, cached per state — the `OVT_FillFuelAction.c:178-196` pattern. `HasLocalEffectOnlyScript()` is `true`; the work goes over the request component.
- **All storage actions hide on a ruined holder** via `OVT_StructureDamage.IsUsable(GetOwner())` — the existing precedent in `OVT_OpenStorageAction.c:35-71` and `OVT_LoadStorageAction.c:76`. This is `core/damage`'s seam and nothing more.
- **Load stays ledger-only; Unload sweeps first.** Requirement C asks for the sweep on Unload (the truck has just been looted and its vanilla inventory is full). Load does **not** sweep: a box's vanilla inventory may hold a player's deliberately-parked rifle, and silently converting it would be a surprise. The asymmetry is intentional (§5 D6).
- ⚠️ The box now lists 7 actions including vanilla "Open". If the vanilla label and ours both read "Open Storage" in play-test, relabel the *vanilla* one on the box prefab to `#AR-Inventory_OpenAmmoBox` — a one-line prefab edit, §6 step 12.

### 3.11 Vanilla supply, hidden globally

`Prefabs/GameMode/OVT_OverthrowGameMode.et` gains `m_aDisabledResourceTypes { 0 }` (`SUPPLIES`). The attribute is `SCR_BaseGameMode.m_aDisabledResourceTypes` (`:262`), read through `SCR_ResourceComponent.IsResourceTypeEnabled` → `SCR_ResourceSystemHelper.IsGlobalResourceTypeEnabled` → `SCR_BaseGameMode.IsResourceTypeEnabled(:270)`; the vanilla precedent is `Prefabs/MP/Modes/Editor/GameMode_Editor.et:152`. One line disables all four supply user actions on every truck bed and civilian car at once, replacing what would otherwise be a per-prefab `ParentContextList { }` blanking exercise across dozens of files.

**Accepted side effect:** vanilla arsenal and support-station supply readouts vanish too (`SCR_InventoryStorageBaseUI.c:733,891`, `SCR_ArsenalComponent.c:169`, `SCR_BaseSupportStationComponent.c:740`). Overthrow does not run Conflict's supply economy, so those readouts show a number that means nothing today — but this is a **play-test check** (§6 step 16), not an assumption.

### 3.12 Replication summary

| What | Mechanism | JIP |
|---|---|---|
| Holder item count | `[RplProp(onRplName:"OnCountChanged")] int m_iTotalCount`, bumped once per finished job | Free — `RplProp` carries streamed-in state (`OVT_ReservationSyncComponent.c:16-25`) |
| Holder display name | `[RplProp()] string m_sCustomName` | Free |
| Holder capacity | `[RplProp()] int m_iCapacity`, bumped once at resolve | Free |
| Holder contents | **Never replicated.** Pull-on-open fan to one player | N/A — a joining client sees count/name/capacity and pulls if it opens one |
| Progress | Inherited owner RPCs on `OVT_BaseServerProgressComponent` | N/A |

`RplSave`/`RplLoad` are **not** implemented on `OVT_StorageComponent` — three `RplProp`s cover the whole replicated surface, and the real-estate manager's `RplSave`/`RplLoad` shed their inventory half in Phase 7 (which also fixes `RplLoad` inserting without clearing, `:968`).

---

## 4. Implementation Phases

Every phase: `tools/compile-check.sh` exit 0 before hand-back. **`tools/run-tests.sh` is the orchestrator's**, run once after a phase completes — never inside an agent, never during planning (`.claude/test-policy.md`; the script was fixed 2026-08-21 to test the correct worktree via a junction farm + file-identity guard — `-main` is still defective). The group named per phase is **Fast** `{6A6E29FF47ECB840}` or **All** `{6A6E2A002F53A581}`; All is required whenever serializers, `Configs/Systems/Persistence/`, or campaign/economy state are touched. Re-baseline (`git pull` / `git status`) before every phase — concurrent sessions share this tree.

### Phase 1 — Ledger, rules, Logic cases
**Estimate:** 4–5 h · **Agent:** `component-developer` · **Tests: Fast**

1. `Scripts/Game/Data/OVT_StorageLedger.c` — `OVT_StorageLine` + the ledger of §3.2.
2. `Scripts/Game/Data/OVT_StorageRules.c` — the five statics of §3.3.
3. `Tests/TestSuites/Logic/OVT_TEST_Logic_StorageLedger.c`, `…_StorageRules.c`, registered on `OVT_TEST_LogicSuite`.

**Acceptance**
- No case references a manager, the game mode or the world, and **the identifiers for Overthrow's static manager accessor and the engine's game-mode getter appear nowhere under `TestSuites/Logic/`, not even in a comment** — the tier grep does not distinguish code from prose.
- Every case proven able to fail once; the mutation and the resulting message recorded in `context.md`.
- `new` sets every field explicitly (`[Attribute]` defvalues do not apply to `new`); floats compared with an epsilon; no `maxAttempts`.
- `Total()` is O(1) by construction — no iteration in the getter.
- Compile-check exit 0.

### Phase 2 — `OVT_StorageComponent`, prefab deltas, Init cases ⚠️ ADVANCED AGENT
**Estimate:** 5–7 h · **Agent:** `component-developer-advanced` · **Tests: All**

1. `Scripts/Game/Components/OVT_StorageComponent.c` — §3.4, including the deferred AUTO resolve, the `GetRpl()` assertion, and `PublishCount()`.
2. `Scripts/Game/Utilities/OVT_StorageUtils.c` — §3.5, per-call query object.
3. Prefab deltas: `Wheeled_Base.et` (+ component, AUTO, `m_iAutoVehicleCapacity 300`); `OVT_AmmoBox_Base.et` (+ component UNLIMITED); **new same-GUID delta** `Prefabs/Structures/Industrial/Houses/Warehouse_01/Warehouse_01_Base.et` `{E35EA41864A3B0ED}` (+ component UNLIMITED, `m_sDefaultNameKey "#OVT-Warehouse"`) — the header must name vanilla's parent `Building_Base.et` `{A43A100E3C377DB2}` exactly as the vanilla file does; `OverthrowMobileFOB.et` (+ explicit UNLIMITED override — AUTO already yields it via `vehiclePrices.conf:87`, but the FOB's ledger is load-bearing for undeploy and must not depend on a pricing entry).
4. `OVT_OverthrowGameMode.et` — `m_aDisabledResourceTypes { 0 }` (§3.11).
5. `Tests/TestSuites/Init/OVT_TEST_Init_StorageSeam.c` — a spawned truck, a spawned civilian car, a spawned ammo box and the test world's warehouse (`Worlds/MP/OVT_Campaign_Test_Layers/default.layer:94`) each resolve a component with the expected capacity (−1 / 300 / −1 / −1).

**Acceptance**
- Fresh GUIDs come from `6A8E2D0…`; **inherited component GUIDs are copied, not minted**; `grep -rl` re-verifies the series is unused before authoring.
- The warehouse delta's GUID matches vanilla's byte-for-byte, and all seven `Warehouse_01*` variants inherit the component (verified by opening one child in the Workbench, and by the real-estate filter `"Warehouse_01"` still matching, `OVT_OverthrowGameMode.et:136-145`).
- Capacity resolution never runs on a client; a client reads `m_iCapacity` only.
- An armed/illegal wheeled vehicle resolves capacity **0** and its actions do not appear.
- Compile-check exit 0.

### Phase 3 — Persistence: serializer + three bindings ⚠️ ADVANCED AGENT
**Estimate:** 4–6 h · **Agent:** `component-developer-advanced` · **Tests: All**

1. `OVT_StorageComponentSerializer.c` + `OVT_PersistedStorageLine` — §3.9, **read `OVT_JobManagerSerializer.DeserializeVersion2` first**.
2. Three bindings in `Overthrow.conf` (CAR append, Placeable append, new Structures block for `{65B682661F79DDBE}`), GUIDs from `6B0E7A6…`.
3. `Track()`-on-first-content for building holders.
4. Three cases appended to `OVT_TEST_PersistenceRoundTripSuite.c`: a box's ledger + name round-trip (`RequestInstanceReload`), a vehicle's ledger round-trip, and the warehouse building's ledger round-trip incl. the explicit Track.

**Acceptance**
- Serialize/Deserialize locals are **identically named**; a deliberate rename is shown to fail during development and the observation is recorded in `context.md`.
- Every `Read()` return is checked; a forced failure leaves live state untouched and logs ERROR.
- No new `ComponentClassPersistenceConfigRule` anywhere.
- New saving cases sort **after** `..._Capability_...` (`OVT_TEST_PersistenceRoundTripSuite.c:542`).
- Compile-check exit 0.

### Phase 4 — Request component, wire protocol, pull-on-open ⚠️ ADVANCED AGENT
**Estimate:** 6–8 h · **Agent:** `network-specialist-advanced` · **Tests: All**

1. `OVT_StorageRequestComponent.c` — class, attributes, `MayUseHolder`, the two seq counters, RPCs 1, 7, 8 and 9–13 (§3.6). The batch verbs 2–6 are declared and answer `#OVT-Storage_Busy` until Phase 5.
2. `OVT_OverthrowController.et` — append the component before `RplComponent`.
3. `Tests/TestSuites/Init/OVT_TEST_Init_StorageSeam.c` — extend with the controller-component seam case (D11 requires one for every new controller component): poll `OVT_Global.GetController()`, assert `OVT_ControllerComponent<OVT_StorageRequestComponent>.Get()` is non-null **and** sits on this player's controller entity (`OVT_TEST_Init_VehicleRequestSeam.c:30-31` is the template).

**Acceptance**
- **Every `Rpc()` call site hand-audited against its handler's parameter list**, and the audit written into `context.md` as a table. No `Rpc()` call is wrapped in a helper.
- No `array<...>` on any RPC.
- Every `RpcDo_*` takes the `IsLocalPlayerOwner()` direct branch first.
- Every rejection answers `RpcDo_StorageError`; none returns silently.
- Rename enforces 1–32 characters server-side.
- Compile-check exit 0.

### Phase 5 — The job engine ⚠️ ADVANCED AGENT
**Estimate:** 8–10 h · **Agent:** `component-developer-advanced` · **Tests: All**

1. `OVT_StorageJob` + the state machine of §3.7 (VALIDATE / RUN / STEP / FINISH / ABORT), all six ops, chaining.
2. Verbs 2–6 wired: `BatchBegin/Line/Commit`, `TransferAllToStorage`, `MoveAllToHolder`.
3. Sweep enumeration rules: right manager, weapon stripping, full-magazine skip, no registry gate.
4. Progress keys + `.st` entries for them.

**Acceptance**
- **Ordering proven by reading:** delete-then-credit on the sweep with the capacity check **before** the delete; spawn-then-debit on `TO_INVENTORY`; the un-added remainder of a `TO_HOLDER` move goes back to the source.
- Holder death or player disconnect aborts at the next chunk with both ledgers consistent.
- `PublishCount()` (and therefore `BumpMe`) is called **once per holder per job**, asserted by reading every call site.
- One job per player; a second request answers Busy and does not queue.
- No call into `OVT_InventoryManagerComponent`.
- `RemoveOrdered` wherever a line array's order is observable.
- Compile-check exit 0.

### Phase 6 — Open Storage screen + actions
**Estimate:** 5–7 h · **Agent:** `ui-developer` · **Tests: Fast**

1. `Scripts/Game/UI/Context/OVT_StorageContext.c` — the eight hooks of §3.8, staged snapshot, latched pull, 250 ms coalesced live refresh.
2. `Character_Player.et` — an `OVT_StorageContext` block reusing `TransferMenu.layout` `{6A8E2C1000000001}` and `m_sContextName "OverthrowTransferContext"`, instance GUID from `6A8E2D1…`.
3. Four user actions (§3.10) + the ammo-box and `Vehicle_Base` prefab entries and renumbering.
4. Rename dialog — `SCR_ConfigurableDialogUi.CreateFromPreset("{272B6C4030554E27}…DialogPresets_Campaign.conf", "RENAME_RECRUIT")` + `SCR_EditBoxComponent`, the shipped recruit-rename flow (`OVT_RecruitsContext.c:641-716`).
5. `.st` keys for everything new.

**Acceptance**
- **No change to `OVT_TransferContext.c` or to either transfer model.** If one seems necessary, stop and raise it — the hook list is closed.
- The destination picker shows ≥ 2 entries (a first for the mod) and the base hides it only below 2.
- `OnClose` removes exactly what `OnShow` inserted, including the count invoker and every pending `CallLater`.
- Action labels read from the replicated count and update without a menu.
- `python3 .claude/skills/overthrow-ui-patterns/scripts/check-input-conflicts.py` exit 0 at the shipped baseline `0 error(s), 0 warning(s), 3 combo note(s), 0 pre-existing, 1 acknowledged`.
- **Do not write `Configs/Language/*.conf`** — Workbench-generated exports.
- Compile-check exit 0.

### Phase 7 — Warehouse migration + real-estate seam rewrite ⚠️ ADVANCED AGENT
**Estimate:** 6–8 h · **Agent:** `component-developer-advanced` · **Tests: All**

1. `OVT_WarehouseData` — delete `inventory` and `isLinked`; keep `id`, `location`, `owner`, `isPrivate`.
2. `OVT_RealEstateManagerComponent` — delete `GetWarehouseInventory`, `AddToWarehouse`, `DoAddToWarehouse`, `TakeFromWarehouse`, `DoTakeFromWarehouse`, `TransferToWarehouse`, `TakeFromWarehouseToVehicle`, `RpcDo_SetWarehouseInventory`, `m_OnWarehouseInventoryChanged`, `WarehouseHasStock`; strip the inventory half of `RplSave`/`RplLoad` (and fix `RplLoad` clearing before insert, `:968`); add `QueueWarehouseMigration` + the drain; add the public accessibility method the client button and `MayUseHolder` both call.
3. `OVT_RealEstateRequestComponent` — delete the three client methods, the three `RpcAsk_*`, `ValidateWarehouseRequest`, `RejectWarehouseRequest`.
4. `OVT_RealEstateManagerSerializer` — version 2 + the v1 migration read (§3.9).
5. `OVT_ContainerTransferComponent` — delete `TransferToWarehouse` + `RpcAsk_TransferToWarehouse`.
6. Callers: `OVT_VehicleMenuContext` (both warehouse buttons retargeted), `OVT_EconomyInfo` (prompt reads the building's component), `OVT_MapLocationWarehouse` (**contents rows become one count + name row** — its header comment "CONTENTS ARE GENUINELY CLIENT-READABLE" is now false and must be rewritten), `OVT_RespawnService:379`.
7. Delete `OVT_WarehouseContext.c` and its `Character_Player.et` block; update `OVT_TEST_Init_TransferContexts.c`.
8. Persistence case: a v1 save's warehouse stock lands in the building's component.

**Acceptance**
- `grep -rn "isLinked\|WarehouseInventory\|TakeFromWarehouseToVehicle\|OVT_WarehouseContext\|m_OnWarehouseInventoryChanged" --include=*.c --include=*.et --include=*.conf .` returns nothing.
- No `warehouseId` integer crosses any RPC anywhere.
- An old save loads: ownership, rentals and stock all present, stock now in components.
- The client's warehouse-button visibility and `MayUseHolder`'s warehouse branch call **the same method**.
- Compile-check exit 0.

### Phase 8 — Convert the remaining flows ⚠️ ADVANCED AGENT
**Estimate:** 6–8 h · **Agent:** `component-developer-advanced` · **Tests: All**

1. **Port import** — `OVT_VehicleRequestComponent.RpcAsk_ImportToVehicle`: keep every gate and the money maths (`:401-497`), replace the `TrySpawnPrefabToStorage` loop with a ledger `Add` clamped by capacity, charge for what fitted. **`IsRegisteredResource` stays** — this path mints from a client-chosen name.
2. **Load / Unload** — rewritten storage-only (§3.10); labels mention Storage (value edits on `#OVT-LoadStorage` / `#OVT-UnloadStorage`, no new keys).
3. **Truck Loot** — `OVT_LootIntoVehicleAction` → `LOOT` job; filter = everything except base clothing; the hard-coded English "Looting system busy, try again later" (`:257-319`) replaced with an `#OVT-` key.
4. **FOB undeploy** — `OVT_ResistanceFactionManager.UndeployFOB` / `OVT_ContainerTransferComponent.RpcAsk_UndeployFOB` chain per container: sweep then whole-ledger move into the mobile FOB. Spent magazines are skipped by the sweep and die with the container the existing cleanup deletes — no special case.
5. **Truck vanilla cap raised** to fit ~20–30 soldiers' loot: `M923A1_transport.et:4-7` and `Ural4320_transport.et:4-7` (`MaxCumulativeVolume` 200000, `m_fMaxWeight` 1000 → vanilla's own 1000000 / 4500-5000).
6. Remove the now-dead `OVT_InventoryManagerComponent` call sites (keep the class; `OVT_StorageProgressUIContext`'s cancel path and the vehicle-upgrade transfer still use it).

**Acceptance**
- Port import and warehouse take perform **zero** entity spawns — proven by reading, and by watching the frame in play-test step 9.
- The clothing filter compares `typename`, not `ClassName()` strings.
- FOB undeploy still deletes the emptied containers and reactivates physics exactly as today.
- Compile-check exit 0.

### Phase 9 — Port Export
**Estimate:** 3–4 h · **Agents:** `component-developer` then `ui-developer` · **Tests: All**

1. `EXPORT` job pricing wired to `OVT_StorageRules.ExportUnitPrice` with `m_fExportPriceRatio`; money via `OVT_EconomyManagerComponent.DoAddPlayerMoney`.
2. Illegal gate: exportable under `HasPermission("IllegalImports")` ∥ `ResistanceControlsNearestPort(...)` — the same expression `OVT_PortContext.CollectImportables` uses (`:112-114`).
3. `OVT_PortContext` — second mode, `BuildEntries(EXPORT)` over the occupied vehicle's snapshot, `IsAddAllAllowed` true in Export, `ValidateCart` port + gate checks, `OnAccept` → `EXPORT` batch.
4. `.st` keys.

**Acceptance**
- Export unit price is **below** `GetBuyPrice(id, portPos, -1)` for every priced item — asserted by a Logic case over the pure function and spot-checked in play-test step 13.
- Export never mints: the batch clamps to ledger membership.
- The mode toggle appears (two modes) and the shipped import path is byte-identical in behaviour.
- Compile-check exit 0.

### Phase 10 — Localization, conflict check, help & wiki sync
**Estimate:** 2–3 h · **Agents:** main thread + `help-docs-sync` · **Tests: none (announce skip)**

1. `.st` audit: every runtime key exists in `Language/localization_Overthrow.st` with a filled `Comment`; **count braces before and after** (an unbalanced `.st` means the next Workbench save eats entries); fresh GUIDs; Id order; multi-line values use the trailing backslash. **Ask the user to re-export** — keys render raw until then.
2. Final `check-input-conflicts.py`, plain and `--warnings`.
3. `help-docs-sync`: this feature changes player-facing behaviour substantially (a new screen, five new actions, warehouse take reshaped, port Export, vanilla supply gone), so the phase runs. Tutorials (`Configs/Tutorials/`) and the Field Manual (`Configs/FieldManual/Categories/FM_Overthrow.conf`) plus the wiki's port/warehouse/inventory pages.
4. Leave dead `.st` keys in place and note them — deleting one is a structural edit with a data-loss failure mode.

**Acceptance**
- No `Configs/Language/*.conf` modified; braces balanced (record the before/after counts).
- Conflict checker exit 0 at the baseline summary.
- **Every sentence of new help text cites a file:line or is cut** — two tutorial tips have shipped inventing mechanics; no gate catches a well-formed lie.
- Wiki writes verified by re-reading each page after writing (the MCP tools can report success and leave the render stale).

**Total estimate: 49–66 h across 10 phases**, plus one user-gated Workbench session and three play-test sessions (SP mouse, gamepad-only, dedicated + JIP). Phases 8 and 9 touch disjoint files and **may run in parallel** after Phase 7.

---

## 5. Key Technical Decisions

**D1 — One `OVT_StorageComponent` on `Wheeled_Base.et` reaches every wheeled vehicle.** (User decision, 2026-08-21.) There is **no runtime component creation in EnforceScript** — `IEntity` exposes `FindComponent`/`FindComponents` only, and `WorldEditorAPI.CreateComponent` is Workbench-only — so a component must be authored on a prefab. Overthrow already owns a same-GUID delta of vanilla `Prefabs/Vehicles/Core/Wheeled_Base.et` `{62F416029692CE40}` carrying `OVT_PlayerOwnerComponent` and `OVT_ReservationSyncComponent`, and every vanilla wheeled vehicle inherits it through `Wheeled_Car_Base`, `Wheeled_Truck_Base` and `Wheeled_APC_Base`. One line therefore reaches every wheeled vehicle spawned by **any** system — the vehicle manager, faction registries, deployment modules, civilian ambience, the world layers. Capacity then discriminates: AUTO resolves truck → unlimited, legal car → 300, illegal/armed → 0. *Rejected:* (a) per-truck prefab deltas — a dozen files, and it misses any vehicle a later config adds; (b) a manager-side map keyed by entity — no persistence per instance, no `RplProp`, and it reintroduces exactly the "welded into a manager" problem this feature is undoing. **This also settles `logistics/resources`' single largest open risk** (`epic-overview.md`, "how the cargo store reaches every truck"): the answer is the same seam, a second component on the same delta.

**D2 — The warehouse ledger lives on the building entity, not on a manager record.** (User decision, 2026-08-21.) A same-GUID delta of vanilla `Warehouse_01_Base.et` `{E35EA41864A3B0ED}` covers all seven variants in one file, and the real-estate filter `"Warehouse_01"` (`OVT_OverthrowGameMode.et:136-145`) already matches exactly that family. The building is a replicated entity (`Building_Base.et` carries `RplComponent "{50A4E7C9B5728062}"`) with a `Persistence` component, and Overthrow already adds components to vanilla buildings this way (`ShopHouse_E_2I01t_Base.et` adds `OVT_ShopComponent`; `Garage_E_02.et` adds five). The consequence that makes this the right call: the warehouse stops being special. It is an `RplId` with a ledger, so one screen, one gate, one serializer and one wire protocol serve it and every box and truck. *Rejected:* keeping a ledger-bearing record on the manager — it would have preserved `warehouseId`, the array-index identity reconstructed independently per client, and a second take path.

**D3 — Hide vanilla supplies globally, not per prefab.** (User decision, 2026-08-21.) `m_aDisabledResourceTypes { 0 }` on the game-mode prefab kills all four supply user actions everywhere through `SCR_BaseGameMode.IsResourceTypeEnabled` (`:270`), with a vanilla precedent (`GameMode_Editor.et:152`). The per-prefab alternative — blanking `ParentContextList` on every truck-bed and civilian-car prefab, which Overthrow already does on two of them — scales with the vehicle catalogue and silently misses anything added later. *Accepted cost:* vanilla arsenal and support-station supply readouts disappear. Overthrow does not run Conflict's supply economy, so they show a meaningless number today; **verified in play-test step 16, not assumed.**

**D4 — Civilian cars hold 300 items.** (User decision, 2026-08-21.) An `[Attribute]` on the component authored on `Wheeled_Base.et`, so it is one Workbench field to retune. Enough to make a car a real if inferior hauler next to an unlimited truck.

**D5 — One holder, one seam, one new batch engine.** (User decision, 2026-08-21.) Every holder is an `RplId`; no `warehouseId` vocabulary survives; the real-estate warehouse RPCs and the JIP inventory stream are deleted rather than adapted. The engine is a **per-player** job on `OVT_OverthrowController` extending `OVT_BaseServerProgressComponent` so it owns its own progress. *Rejected:* extending `OVT_InventoryManagerComponent` — its `m_aContainerSearchResults` accumulator (`:497`) is a singleton shared by concurrent searches and its progress state is single-instance, which is the defect being replaced, not a foundation. **`ui` D10 (one request per cart line) is superseded here** by the batched checkout, at exactly the seam `ui` designed for it: `OnAccept`.

**D6 — Unload sweeps the source; Load does not.** Requirement C asks for the sweep on Unload, and the reason is the common loop: Loot a battlefield into a truck (loot lands in the truck's *vanilla* inventory), drive to a box, Unload — everything must end up in storage. Load runs box → vehicle, and a box's vanilla inventory may hold something a player deliberately parked there; silently converting it would be a surprise with no undo. The asymmetry is a deliberate reading of the requirement, not an omission.

**D7 — Capacity is replicated, not re-derived on clients.** AUTO resolution reads the economy's vehicle catalogue (`m_mVehicleParking`, `m_aLegalVehicles`), which is built during server init; whether a client's copy is populated and populated *at the same time* is an assumption this feature would rather not make. One `RplProp` int, set once, bumped once, JIP-free, removes an entire class of client/server divergence for a one-time cost of one int per holder at spawn. *Rejected:* client-side resolution (divergence risk) and encoding "no storage" as a negative count (muddies the label).

**D8 — Capacity is not persisted.** Requirement E lists "capacity overrides" as persisted; the plan reads those as *prefab* overrides, which persist by being in the prefab. Writing a copy into the save would freeze a retuned prefab or price config out of every existing campaign — the wrong default for a mod that retunes constantly. Nothing in this feature mutates capacity at runtime, so there is nothing else to save. **Flagged for the user to overrule**; adding two ints is a version bump, which is cheap and normal.

**D9 — `OVT_WarehouseContext` is deleted rather than rewritten.** Once the warehouse is a holder, a warehouse-only subclass is `OVT_StorageContext` with a different way of finding the same component. Its entry point (`OVT_VehicleMenuContext.TakeFromWarehouse`) opens `OVT_StorageContext` with the building as the holder; the accessibility check that decides whether the button shows stays where it is and is lifted to a shared method so it and the server gate cannot drift. `PutInWarehouse` survives as `MoveAllToHolder(vehicle, building, sweepFirst: true)` — the one-button truck dump is not lost.

**D10 — No storage manager.** The only system-wide, server-only job in this feature is draining the one-shot warehouse migration, and that belongs to `OVT_RealEstateManagerComponent` — the component that owned the data and the component that will be deleted along with the migration. Radius queries are stateless (a `new`-ed query object), capacity comes from the existing economy manager, and all mutation is per-player by design. *Rejected:* an `OVT_StorageManagerComponent` — it would exist to hold three attributes and a queue, and would need an `OVT_Global` accessor and an Init case to justify itself.

**D11 — The eight `ui` hooks are enough; nothing is added to the base.** All four consumers (Open Storage, port Import, port Export, and the warehouse's take through Open Storage) land on the shipped list. The one thing storage seemed to need — "tell me when to rebuild after an async pull" — is covered because `OVT_TransferContext.Refresh()` is already public (`:430`); the pull is fired from inside `BuildEntries`, latched and idempotent. Adding an `OnModeChanged` hook would widen a list the epic declared closed to save one line. If an implementer finds a genuine gap, that is a plan defect to raise, not a base change to make quietly.

**D12 — Ledger membership replaces the registry check on take-out.** `count(res) >= qty` on that specific ledger is strictly stronger than `IsRegisteredResource` — a line can only exist because the server deleted a real entity. The registry gate on the warehouse take path (`OVT_RealEstateRequestComponent.c:550`) traps converted loot today, and it goes. The one path that **mints** stock from a client-chosen string, port import, keeps it.

---

## 6. Definition of Done

### Functional

- **F1** Every ammo box, wheeled legal vehicle and warehouse building shows a "Storage (N items)" action whose number is live and correct; illegal/armed vehicles and helicopters show none.
- **F2** Open Storage lists the holder's contents (pulled on open), with a destination picker offering **This container** plus every nearby named holder.
- **F3** Take → **This container** spawns items into the holder's own vanilla inventory with a progress bar, stops early when it is full, and reports the shortfall.
- **F4** Take → **another holder** moves ledger-to-ledger with **zero** spawns and clamps at the destination's capacity, returning the remainder to the source.
- **F5** "Transfer all to storage" converts the holder's entire vanilla inventory: attachments and loaded magazines go in as separate items, part-used magazines stay behind, unregistered prefabs are credited, truck-bed child storages are included.
- **F6** Rename sets a name that shows in the picker, in the action label and on the map, for every player, and survives a save.
- **F7** Officer-only "Clear inventory" empties an ammo box's **vanilla** inventory and leaves the ledger untouched; non-officers do not see it.
- **F8** Load/Unload move storage only; Unload sweeps the vehicle first, so "Loot a battlefield, drive to a box, Unload" ends with everything in the box's ledger.
- **F9** Port import credits the vehicle's ledger with zero spawns and charges only for what fitted; warehouse take is ledger-to-ledger with zero spawns.
- **F10** FOB undeploy collects both the vanilla inventories and the ledgers of nearby boxes into the mobile FOB's ledger, then cleans up as it does today.
- **F11** Port Export sells the occupied vehicle's ledger at a price below every shop buy price; illegal items export only under the shipped gate.
- **F12** A second player standing next to an open box sees no traffic; a joining client sees every holder's count and name immediately without any contents replication.
- **F13** Ledger and name survive save/continue for boxes, vehicles and the warehouse; an existing v1 save's warehouse stock appears in the building's storage.
- **F14** Vanilla supply Load/Unload/Store actions no longer appear on any truck bed or civilian car.

### Quality

- **Q1** `tools/compile-check.sh` exit 0 at every phase boundary.
- **Q2** Logic cases for the ledger and every rule, each proven able to fail once, mutations recorded in `context.md`.
- **Q3** Persistence round-trip cases for a box, a vehicle and the warehouse, plus the v1 migration; all sorted after `..._Capability_...`.
- **Q4** An RPC arity audit table in `context.md` covering all thirteen RPCs against their handlers.
- **Q5** No `array<...>` on any RPC; no `Rpc()` call wrapped in a helper.
- **Q6** `Replication.BumpMe()` is reachable at most once per holder per job.
- **Q7** No file under `Scripts/Game/GameMode/Managers/OVT_InventoryManagerComponent.c` is modified; no `core/damage` file is touched beyond calling `OVT_StructureDamage.IsUsable`.
- **Q8** No `Configs/Language/*.conf` modified; `.st` braces balanced with counts recorded.
- **Q9** Comments sparse per `CLAUDE.md` — a line or two for a trap or a load-bearing ordering, never a rationale essay. Reasoning belongs in this document.
- **Q10** `check-input-conflicts.py` exit 0 at the shipped baseline.

### Integration

- **I1** `OVT_TransferContext.c`, `OVT_TransferListModel.c` and `OVT_TransferCartModel.c` are **unmodified**.
- **I2** Nothing in this feature references m³, volume, weight or a resource type.
- **I3** `OVT_VehicleMenuContext`'s two warehouse buttons still appear under the same conditions and still work.
- **I4** `OVT_ShopTransactionComponent`, `OVT_ShopComponent` and the shop screens are untouched; port Export is a separate path.
- **I5** The client's warehouse accessibility check and the server's `MayUseHolder` warehouse branch call the same method.

### Verification method — an independent evaluator can follow this

**Static (no game):**
1. `cd "/mnt/n/Projects/Arma 4/Overthrow.Arma4" && tools/compile-check.sh` → exit 0.
2. `grep -rn "isLinked\|GetWarehouseInventory\|TakeFromWarehouseToVehicle\|OVT_WarehouseContext\|m_OnWarehouseInventoryChanged\|RpcDo_SetWarehouseInventory" --include=*.c --include=*.et --include=*.conf .` → no hits.
3. `git diff --exit-code -- Scripts/Game/UI/Context/OVT_TransferContext.c Scripts/Game/Data/OVT_TransferListModel.c Scripts/Game/Data/OVT_TransferCartModel.c` → clean.
4. `grep -rn "m3\|volume\|Volume\|weight\|EResourceType" Scripts/Game/Data/OVT_StorageLedger.c Scripts/Game/Components/OVT_StorageComponent.c` → no hits.
5. `grep -c "Rpc(" Scripts/Game/Components/Controller/OVT_StorageRequestComponent.c` matches the audit table in `context.md`, and each row's arity matches its handler.
6. `python3 .claude/skills/overthrow-ui-patterns/scripts/check-input-conflicts.py` → exit 0, summary `0 error(s), 0 warning(s), 3 combo note(s), 0 pre-existing, 1 acknowledged.`
7. `git status --porcelain Configs/Language/` → empty; `.st` brace count unchanged before/after by the number of added entries.
8. Orchestrator only: `tools/run-tests.sh "{6A6E2A002F53A581}"` (All) once after each of Phases 2, 3, 4, 5, 7, 8, 9. Announce the focus steal first.

**Workbench (user-gated):** open the new `Warehouse_01_Base.et` delta and one child variant (`Warehouse_01_Office.et`) and confirm the storage component and the action context are inherited; open `Wheeled_Base.et`, `OVT_AmmoBox_Base.et`, `OVT_OverthrowController.et` and `Character_Player.et` without dropped-attribute warnings.

**Play-test A — single player, mouse:**
9. Drive a truck to a port with `~$50,000`. Import 100 of something. **Watch the frame:** no hitch, no entities appear in the truck. Open Storage on the truck → 100 listed.
10. Take 50 → destination **This container** → progress bar runs, 50 entities appear in the truck's vanilla inventory, ledger drops to 50, the action label follows.
11. Place an ammo box next to the truck. Open Storage on the truck → the picker now lists the box by name → Take all 50 → **instant**, no progress bar needed, box count 50, truck 0.
12. On the box: "Transfer all to storage" after dropping in a loaded rifle with an optic and a half-empty magazine → rifle, optic and the *full* magazine become three ledger lines; the half-empty magazine stays in the vanilla inventory. Check the two "Open" actions are distinguishable; if both read "Open Storage", relabel the vanilla one (§3.10).
13. Rename the box "Depot North" → the name shows in the action, in the truck's picker and on the map. Officer "Clear inventory" removes the half-empty magazine.
14. Warehouse: walk up to a Warehouse_01 you own — the Storage action appears (note at what distance; tune `Radius` if it is awkward). Take to a vehicle. Then from the pilot seat use the vehicle menu's warehouse buttons — both still work.
15. Loot: kill a squad, drive a truck over, "Loot" → progress bar, no spike, helmets/vests/bags taken and base clothing left. Drive to a box, "Unload Storage" → everything, including the loot that was in the vanilla inventory, ends in the box's ledger.
16. Export: at a port with the L5 permission or a resistance-controlled port, switch the port screen to Export, sell the truck's ledger → money arrives, ledger empties, and the unit price is below what the same item costs at a shop.
17. **Vanilla supply check:** open a truck bed and a civilian car — no Load/Unload supply actions. Open an arsenal and a support station and confirm nothing is *broken* (a missing supply readout is expected; a non-functional arsenal is not).
18. Save (or Continue) → reload → every count, every name and the warehouse stock are exactly as left. Then load a **pre-feature** save and confirm the old warehouse stock appears in the building.

**Play-test B — gamepad only (no mouse touched):**
19. Open Storage on a box with two nearby holders. Something is focused on arrival; d-pad walks the list.
20. **D6, finally exercisable:** d-pad down to the destination picker; left/right changes the destination and **does not** move the focus column.
21. **The `ClearAll()`-on-empty path, finally exercisable:** open storage on an **empty** box that has ≥ 2 nearby destinations — the picker fills for the first time with an empty element list. It must not error (`SCR_SpinBoxComponent.SetInitialState` reads `m_aElementNames.Count()` with no null guard).
22. Build a cart, `a` on Accept → the batch runs, the cart clears, focus lands somewhere real.
23. `b` closes; `LB` still opens VON.

**Play-test C — dedicated server, two clients:**
24. Client 1 opens a box holding 500 items. **Client 2, standing next to it, sees no traffic** and its frame time is flat. Client 2 opens the same box → its own snapshot arrives.
25. Client 1 takes 200 while Client 2 has it open → Client 2's list re-pulls within ~250 ms and its cart reconciles.
26. **JIP:** Client 3 joins after all of the above → sees every holder's count and name on the action labels immediately, with no contents traffic until it opens one.
27. Two clients start a batch on the same holder simultaneously → both complete correctly or one is told Busy; the ledger total is exactly right afterwards.
28. Disconnect Client 1 mid-transfer → the job aborts, no item is duplicated, the count is consistent.

### Bug-report candidates for the orchestrator — do not file from this plan

- `OVT_RealEstateManagerComponent.RplLoad:968` inserts warehouse records without clearing first, so a re-stream duplicates. Fixed incidentally in Phase 7.
- `OVT_LootIntoVehicleAction:257-319` shows a hard-coded English string. Fixed in Phase 8.
- `OVT_StorageOperationConfig`'s constructor arguments do not match its own documentation (`OVT_InventoryManagerComponent.c:72`), so every caller passes 1 item per tick believing it passes 5. Not fixed here — the callers go away.

---

## 7. Testing Strategy

**Logic tier — `OVT_TEST_Logic_StorageLedger.c` / `…_StorageRules.c`** (world-free, `new`-built, ~1 s):

| Case | Claim | Proof it can fail |
|---|---|---|
| `LedgerAddClampsToCapacity` | `Add` returns what fitted; total never exceeds capacity | drop the clamp → returns the full qty |
| `LedgerAddUnlimited` | `capacity < 0` fits everything | treat −1 as a literal cap |
| `LedgerTakeClampsAndDropsLine` | taking ≥ held returns what was held and **removes the line**; `LineCount()` falls | clamp-and-keep → line count stays |
| `LedgerTotalIsMaintained` | total tracks add/take/clear across mixed keys | recompute from a stale field |
| `LedgerFreeSpace` | finite → `cap − total`; unlimited → huge; never negative | drop the `Math.Max(0, …)` |
| `LedgerIgnoresGarbage` | empty key and `qty <= 0` are no-ops | remove the guards |
| `RulesAutoCapacity` | non-vehicle → −1; unregistered vehicle → 0; illegal → 0; truck → −1; car → default | swap the truck and car branches |
| `RulesMagazineFull` | full only when `count == max`; `0/0` is "full" and `-1` inputs do not throw | use `>=` on a corrupt input |
| `RulesBaseClothing` | jacket/pants/boots true; vest, helmet, backpack, gloves false | add `LoadoutVestArea` to the set |
| `RulesExportPrice` | ratio applies; never ≥ `minShopBuyPrice`; `minShopBuyPrice < 0` means ratio alone; floor 1 | drop the `Math.Min` |
| `RulesHolderInRange` | inclusive at the boundary and correct at 1000 m | **`vector.Distance` is not correctly rounded** (+1 ULP at 1000 m) — assert with an epsilon, never on an exact boundary |

**Init tier — `OVT_TEST_Init_StorageSeam.c`:** the controller component resolves and sits on this player's controller (mandatory for a new controller component, D11); a spawned truck, civilian car, ammo box and the test world's warehouse (`default.layer:94`) each resolve a component with the expected capacity. Polls are **preconditions with a named failure on expiry**, never retries; no `maxAttempts`.

**Persistence tier — appended to `OVT_TEST_PersistenceRoundTripSuite.c`:** box ledger + name round-trip via `RequestInstanceReload`; vehicle ledger round-trip (the per-instance precedent is `..._VehicleReserveRelease_KeepsOwnerAndContents:2293`); warehouse building ledger round-trip including the explicit Track; a v1 real-estate payload migrating into a building component. Follow the 7-phase state-machine template at `:8479` and dirty state through the public facade before reloading. **All new saving cases must sort after `..._Capability_...`.**

**What the automated spine cannot reach** — and therefore what the play-test gates exist for:

- **Every real spawn and delete.** No suite spawns 100 entities into a truck bed or strips a weapon's attachments; the ordering guarantees (delete-then-credit, spawn-then-debit) are proven by reading and by step 28.
- **All multiplayer behaviour.** Pull-on-open addressing one player, JIP seeing count/name without contents, two clients on one holder, disconnect mid-batch. Steps 24–28.
- **All UI and focus behaviour**, including the two `ui` gotchas this feature is the first to exercise: the picker's d-pad claim (D6) and the first fill of a spin box that has never held items. Steps 20–21.
- **Progress bars** — the HUD path is inherited and untested by any suite.
- **Performance.** "No spike" is a frame-watch claim (steps 9, 15), not an assertion.
- **The vanilla-supply side effect** on arsenals and support stations (step 17).

---

## 8. Quality Bar

**Backend**

- **B1 — Data integrity per item, provable by reading.** The sweep checks capacity *before* deleting; `TO_INVENTORY` debits only after a spawn succeeds; a `TO_HOLDER` remainder returns to the source. The worst outcome of a mid-transfer crash is one item, never a stack.
- **B2 — One operation per player, never global.** Two players transferring into the same box at once both work. Every piece of engine state is per-job, and every sphere-query accumulator is per-call.
- **B3 — No new spikes.** Zero spawns on ledger-to-ledger and on port import; `BumpMe` once per holder per job; chunked work sized by attributes, not hard-coded.
- **B4 — Server-authoritative without exception.** Every mutation goes through `OVT_StorageRequestComponent` and `MayUseHolder`. No client writes a ledger, ever, including on a listen host.
- **B5 — Rejections are visible.** Every refused request answers with a key the player sees. Silent returns and log-only rejections are the shape being replaced.
- **B6 — Persistence never applies a failed read.** Every `Read()` return checked; abort + ERROR on failure; live state untouched.
- **B7 — The wire is auditable.** Thirteen RPCs, each with its arity written down and checked against its handler, because the compiler will not do it.

**UI**

- **B8 — Gamepad parity.** Every step of Open Storage is reachable with the d-pad, `x`, `y`, `RT`, `a` and `b` alone — including the destination picker, which no shipped consumer has ever populated.
- **B9 — Focus is never lost**, including across the async first snapshot and across a live re-pull while the cart is open.
- **B10 — The label never lies.** "Storage (N items)" reads from the replicated count and is correct within one replication tick of any change, on every client.
- **B11 — No `ALWAYS_TOP` focusable widget** and no hover target grown through the widget tree (the trace is clipped to parent bounds).

---

## 9. Dependencies

**Consumed, unmodified:**
- `logistics/ui` — `OVT_TransferContext` + both models + `TransferMenu.layout` + `ActionContext OverthrowTransferContext`. **Must not be modified** (I1).
- `core/game-mode`, `core/controller-migration` — `OVT_OverthrowController`, `OVT_BaseServerProgressComponent`, `OVT_ComponentFinder`, `OVT_ControllerComponent<T>.Get()`, `OVT_ProgressInfo` HUD.
- `economy` — `OVT_EconomyManagerComponent` (`GetPrice`, `GetBuyPrice`, `IsVehicle`, `IsLegalVehicle`, `GetParkingType`, `IsRegisteredResource`, `ResistanceControlsNearestPort`, `DoAddPlayerMoney`, `HasPermission("IllegalImports")`).
- `resistance` — `OVT_ResistanceFactionManager.IsOfficer`.
- `core/damage` — `OVT_StructureDamage.IsUsable` (a seam only; **do not modify any `core/damage` file**).
- `core/persistence` — `ScriptedComponentSerializer`, `OVT_PersistenceTracking`, `Overthrow.conf` rules.
- Vanilla — `InventoryStorageManagerComponent.TrySpawnPrefabToStorage` (note its `count` parameter), `TryDeleteItem`, `TryRemoveItemFromStorage`, `SCR_VehicleInventoryStorageManagerComponent`, `BaseWeaponComponent`, `BaseMagazineComponent`, `BaseLoadoutClothComponent`, `SCR_ConfigurableDialogUi`.

**Modified:** `OVT_RealEstateManagerComponent`, `OVT_RealEstateRequestComponent`, `OVT_RealEstateManagerSerializer`, `OVT_ContainerTransferComponent`, `OVT_VehicleRequestComponent`, `OVT_VehicleMenuContext`, `OVT_PortContext`, `OVT_EconomyInfo`, `OVT_MapLocationWarehouse`, `OVT_RespawnService`, `OVT_LoadStorageAction`, `OVT_LootIntoVehicleAction`, `OVT_ResistanceFactionManager` (undeploy chain), seven prefabs, `Overthrow.conf`, `localization_Overthrow.st`.

**Deleted:** `OVT_WarehouseContext.c` and its prefab block; `OVT_WarehouseData.inventory` / `.isLinked`; ten warehouse methods and four warehouse RPCs.

**Downstream, planned against this:**
- `logistics/resources` — a **second, separate** ledger beside this one on the same holders, reaching every truck through the same `Wheeled_Base.et` seam (D1 settles its largest open risk). Resource Export joins the port's Export mode as a third mode or a category within it — the mode is shaped so it can. It must **not** widen this ledger.
- `logistics/building-repair` — no dependency beyond the shared `IsUsable` seam.

**Concurrent work on this tree:** other sessions commit mid-feature. Re-baseline before every phase; every citation here carries a file:line so drift is detectable.

---

## 10. Risks & Mitigation

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| R1 | **RPC arity mistake** — `Rpc()` is untyped variadic, so a wrong count compiles clean and dies on the wire | Medium (13 RPCs) | Silent, intermittent, hard to trace | Hand-audit table in `context.md` (Phase 4 acceptance); never wrap a `Rpc()` call; owner replies always take the `IsLocalPlayerOwner()` branch |
| R2 | **Serializer property-name mismatch** — `Read`/`Write` key on the local variable's name, so a rename silently reads zeros and returns success | Medium | Save data lost with no error | Locals named identically by construction; every `Read()` return checked; a deliberate rename shown to fail during Phase 3 and recorded |
| R3 | **The warehouse migration finds no building** — record positions vs `GetNearestBuilding(location, 10)` | Medium | An old campaign's stock silently disappears | Retry on a 1 s `CallLater` ten times, then ERROR with the location; a Persistence case covers the happy path; step 18 covers a real save |
| R4 | **Same-GUID vanilla building delta not picked up** — GUID typo, wrong parent path, or a variant that does not inherit | Low–Medium | The warehouse has no storage and nothing says so | GUID copied byte-for-byte from the vanilla file; Init case resolves the component on the test world's warehouse; Workbench opens a child variant |
| R5 | **`BumpMe` storm** — a per-item count bump during a 500-item batch | Medium if unguarded | Exactly the spike this feature exists to remove | `PublishCount()` is the only writer, called once per holder per job; Phase 5 acceptance reads every call site |
| R6 | **Destination picker regressions** — this is the mod's first consumer with ≥ 2 destinations, so `ui` D6 and the empty-spin-box `ClearAll()` path are both unexercised code | High | Pad-only or empty-box-only breakage, invisible to every static check | Play-test steps 20–21 exist solely for these two; `ui/context.md` records both with line references |
| R7 | **Hidden vanilla supplies break something unexpected** — arsenals or support stations that quietly depend on the readout | Low–Medium | A shipped system stops working | Play-test step 17; the fallback is the per-prefab `ParentContextList { }` blanking Overthrow already uses on two cargo prefabs |
| R8 | **Action clutter** — an ammo box now lists 7 actions | Medium | Discoverability worse, not better | Sort priorities fixed in §3.10 so Storage sits right after Open; step 12 judges it; the fallback is folding Rename into the screen (a base change, so a last resort) |
| R9 | **Concurrent batches on one holder** from two players | Medium | Over-take or a negative count | Per-player latch plus a re-clamp against live ledger membership inside VALIDATE **and** at each step; step 27 |
| R10 | **Loot filter takes or leaves the wrong things** — the shipped filter compares class-name strings and two of its names do not exist | Certain today | Players lose gear or the truck fills with shirts | `typename` comparison via a pure, Logic-tested predicate; step 15 |
| R11 | **`OVT_TransferContext` turns out to need a change** after all | Low–Medium | An epic-level contract breaks, `resources` inherits the drift | D11 names the one candidate and closes it; any real gap is raised as a plan defect before code |
| R12 | **Scope pressure toward the resource ledger** — "just add a capacity unit" | Medium | The epic's two-ledger wall breaks | I2's grep is a Definition-of-Done item; §2's out-of-scope list is explicit |
| R13 | **Concurrent sessions** change the tree between phases | Medium | Merge pain, stale line references | Re-baseline before every phase; every claim here carries a file:line |

---

## Agent Routing Summary

| Phase | Agent | Why |
|---|---|---|
| 1 — ledger + rules + Logic cases | `component-developer` | Pure classes and test cases; no world, no networking |
| **2 — component + prefab deltas + Init cases** | **`component-developer-advanced`** ⚠️ | A same-GUID delta of a **vanilla** building, three shared prefab bases whose blast radius is every wheeled vehicle in the game, deferred capacity resolution, and three `RplProp`s. Compile-check sees none of it |
| **3 — serializer + three persistence bindings** | **`component-developer-advanced`** ⚠️ | Property-name-keyed serialization (R2), a config-binding rule where the wrong choice hijacks vehicles and buildings from their existing configs, and explicit tracking of an entity class the engine does not track |
| **4 — request component + wire protocol** | **`network-specialist-advanced`** ⚠️ | Thirteen RPCs on a base that is *not* `OVT_ControllerRequestComponent`, two sequence spaces, a fan protocol, listen-host owner replies, and BUG-090's compile blind spot |
| **5 — job engine** | **`component-developer-advanced`** ⚠️ | The data-integrity ordering rules, six op kinds, chunking, chaining, abort paths, and the vanilla inventory API's traps (right manager, weapon stripping, magazine predicate) |
| 6 — Open Storage screen + actions | `ui-developer` | A consumer on rails laid by `ui`, four user actions, one dialog. **Must not touch the base** |
| **7 — warehouse migration + real-estate rewrite** | **`component-developer-advanced`** ⚠️ | The biggest integration surface in the feature: two managers, a serializer version bump with a live migration, seven callers, one deleted context, one deleted prefab block |
| **8 — convert remaining flows** | **`component-developer-advanced`** ⚠️ | Five shipped flows rerouted at once (import, load/unload, loot, undeploy, truck caps) across managers, actions and prefabs |
| 9 — port Export | `component-developer` then `ui-developer` | Pricing + gate first, then a second mode on a shipped consumer |
| 10 — loc, conflict check, help & wiki | main thread + `help-docs-sync` | `.st` structural safety and the wiki's known write failure modes |

**Every implementation-agent prompt must carry, verbatim:**

> Do not run `tools/run-tests.sh`. Your gate is `tools/compile-check.sh` exit 0 — I run the test suites myself after the phase completes.

Phases 8 and 9 touch disjoint files after Phase 7 and **may run in parallel**. Phase 6 must land before Phase 7 (the storage screen has to exist before `OVT_WarehouseContext` is deleted).
