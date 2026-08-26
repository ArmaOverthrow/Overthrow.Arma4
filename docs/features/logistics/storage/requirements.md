# Storage — Requirements

**Epic:** logistics (moved in from `core` on 2026-08-20 — same problem space and the same transfer screen as the rest of the epic)
**Created:** 2026-08-19 (draft) · **Formalized:** 2026-08-20 with the user. The earlier `core/storage` implementation plan was discarded by the user — too much changed; `/plan-feature logistics/storage` starts fresh. Code facts below are cited from the tree so the plan does not have to rediscover them.

## Overview

Arma Reforger's inventory tracks **spawned entities with full state**. Overthrow only ever cares about **type + quantity** for stockpiles, so a full ammo box or a looted truck is hundreds of replicated entities the engine must stream, simulate and serialize — the direct cause of network spikes, kicked clients and save-time FPS collapse. The warehouse already solved this once (pure data on the manager, not entities) but the solution is welded into `OVT_RealEstateManagerComponent` and cannot travel.

`storage` extracts that idea into a reusable **item ledger** on a **component** — placed on the warehouse, placed ammo boxes, trucks and civilian cars — with server-only **conversion** between the ledger and the vanilla inventory, and one screen: **Open Storage**, a `logistics/ui` consumer with a single **Take** mode whose only variable is the **destination** (this entity's own inventory, or any nearby storage holder). Every bulk-item flow in the mod (port import, warehouse take, box load/unload, FOB undeploy, truck loot) is converted onto it, and port **Export** is added because the ledger finally makes "what is in this truck" a cheap question.

This is the item half of the epic's "two ledgers, deliberately" decision: **count-keyed items**, unlimited where it makes sense. The resource half (m³-capped) is `logistics/resources` and must stay a separate ledger with its own capacity model and persistence record; nothing here may be widened "so resources can use it later".

## Requirements

### A. Ledger and component

- A pure, world-free **item ledger** class (prefab `ResourceName` → quantity): add (returns how many fitted), take (returns how many were taken), count, total (O(1) — action labels poll it every frame), free space, enumerate, clear, change invoker. Zero lines are removed. Logic-tier testable.
- Keys are **prefab `ResourceName` strings**, never the economy's integer ids — `GetInventoryId()` resolves an **unregistered** prefab to index `0`, i.e. some other item's identity (`OVT_EconomyManagerComponent.c` inventory-id lookup), and looted occupying-faction gear is routinely unregistered.
- An **`OVT_StorageComponent`** hosts a ledger on an entity with:
  - **capacity**: `-1` unlimited (warehouse, ammo boxes, trucks), `N` for smaller vehicles (civilian cars — value decided in planning), `0`/absent = no storage. Offensive/illegal vehicles get none.
  - a **display name** (player-settable, persisted, replicated) used wherever the holder is listed — in the destination picker, and in the action label. Anything carrying the component can be named.
  - hosts: the **warehouse** (replacing `OVT_WarehouseData`'s raw map — whether the component lives on a building entity or the manager keeps a ledger-bearing record for static world buildings is a planning decision; the ledger and the request seam must be identical either way), **placed ammo boxes** (`OVT_AmmoBox_Base.et` family), **trucks** (`M923A1_*`, `Ural4320_*`, the mobile FOB) and **civilian cars**. How "civilian" is decided (per-prefab attribute vs the economy's legality data) is settled in planning; carry one mechanism, not two.
- The warehouse code path has not been touched for a long time — audit it for accuracy and completeness as it is migrated (known: `isLinked` is dead code).

### B. Conversion engine (server-only)

- Server-only methods move items **between** the ledger and the vanilla inventory by spawning/despawning entities; the vanilla inventory is never removed or altered — it remains how a character accesses items.
- **Batched** with a **progress bar** wherever possible (the `OVT_BaseServerProgressComponent` pattern on the per-player `OVT_OverthrowController`; never the shared game-mode singleton `OVT_InventoryManagerComponent`, whose single-instance search/progress state is the defect this replaces). One operation at a time **per player**, not globally.
- **Data integrity per item**: delete the entity **before** crediting the ledger; debit the ledger only **after** a spawn succeeds. A crash mid-transfer loses or duplicates at most one item, never a stack. Source/destination dying or the player disconnecting mid-operation aborts cleanly at the next batch.
- **Vanilla → storage rules** ("Transfer all to storage"):
  - enumerate with the correct manager for the host (`SCR_VehicleInventoryStorageManagerComponent` sees truck-bed child storages; the root-only `GetOwnedItems()` path misses them — `OVT_SellableItemScanner.c` documents the trap);
  - **all attachments and the loaded magazine are removed from weapons first** so they go in as separate items (removal goes through the holder's inventory manager — a weapon entity has none);
  - **part-used magazines are ignored** — only a full magazine converts (`GetAmmoCount() == GetMaxAmmoCount()`, predicate precedent in `OVT_VehicleRearmUtils.c`); used clips stay in the vanilla inventory;
  - **unregistered prefabs are credited too** — the string came from an entity the server just deleted, so no registry gate; gating would delete the player's loot.
- **Storage → vanilla** stops early when the container refuses (full) and reports the shortfall.
- **Storage → storage** (between two holders) is pure ledger arithmetic: **zero spawns**, capacity-clamped at the destination.
- Take-out gate on client requests is **ledger membership** (`count(res) >= qty` on that specific ledger), which is strictly stronger than `IsRegisteredResource` (a line can only exist because the server deleted a real entity) — and must replace the registry check on the warehouse take path too, so converted loot is not trapped. Paths that **mint** stock from a client-chosen name (port import) keep the registry check.

### C. Actions and the screen

- **"Open Storage (1,123 items)"** on every holder — a `logistics/ui` consumer with **one mode, Take**; the list shows the holder's ledger with counts; the cart builds lines; the **destination picker** chooses where they go:
  - **Inventory** — converts into **this holder's own vanilla inventory**, spawning fresh entities (batched, progress bar), so the player can then open it normally;
  - **any nearby holder** with a storage component (another ammo box, a truck, a car, the warehouse if it is in range — the warehouse's eligibility as a destination depends on the host decision in A), listed by its display name, within a configurable radius; ledger-to-ledger, no spawns.
  - The action sits **immediately after the vanilla "Open"** action for discoverability, and the item count in its label is what answers "where did my stuff go".
- **"Transfer all to storage"** on every holder — converts the host's entire vanilla inventory into its ledger with the B rules. (This is why Open Storage needs no Put mode: putting *in* is this action; moving *between* holders is Take with a holder destination.)
- **Rename** — a way to set a holder's display name (action or UI affordance; who may rename is decided in planning — default: anyone who may open it).
- **Officer-only "Clear inventory"** on ammo boxes — empties the **vanilla** inventory (not the ledger) so junk such as part-spent magazines can be discarded. `OVT_ResistanceFactionManager.IsOfficer` is the gate precedent.
- Existing **ammo-box Load / Unload** actions (`OVT_LoadStorageAction` / `OVT_UnloadStorageAction`, nearest vehicle within 10–15 m, driver must exit) move **storage-only**: Unload first runs "Transfer all to storage" on the source so any vanilla contents are converted before the ledger-to-ledger move; action names mention **Storage** so it is clear what is moved. The common case — "Loot" a battlefield into a truck, then Unload at a box — therefore lands everything in storage.
- **Truck "Loot"** (`OVT_LootIntoVehicleAction` → `OVT_ContainerTransferComponent.LootBattlefield`) is reworked: migrated onto the progress/batch engine (no spike, visible progress), filter becomes **take everything except base clothing (shirt, pants, boots)** — helmets, vests, bags and the rest are taken — and the truck's vanilla inventory cap is raised so it fits roughly 20–30 soldiers' worth. Loot stays in the vanilla inventory and converts only on transfer to a holder.
- **Undeploy FOB** collects everything from nearby ammo boxes — **both** vanilla inventory and ledgers — into the mobile FOB's ledger; spent magazines may be deleted.
- **Port import** (`OVT_VehicleRequestComponent.RpcAsk_ImportToVehicle`, today up to 100 spawns in one frame) becomes **ledger credit into the vehicle's storage** — zero spawns. **Warehouse take** (`OVT_RealEstateManagerComponent.TakeFromWarehouseToVehicle`, today up to `qty` spawns in one frame) becomes ledger-to-ledger into the vehicle's storage — zero spawns. Both are the two worst spike sites in the mod and are deleted, not batched.
- **Port Export** — the vehicle's **ledger** contents are sellable at the port (a second mode on the `ui` port screen, categories as for import): price is a configurable ratio of the **import** price that must also sit **below every shop buy price** (no shop→port loop); **illegal items** are exportable under the shipped gate (`IllegalImports` permission / Trade L5, or `OVT_EconomyManagerComponent.ResistanceControlsNearestPort`). Money flows through `OVT_EconomyManagerComponent`.
- The `ui` feature's "Accept loops the existing per-line requests" is superseded here by **one request per checkout** carrying the cart lines (primitives only on the wire — see E), which is what makes the cart a single batched operation.

### D. Network

- **Contents are not replicated to every client.** Only the holder's **total item count** (and its display name) replicate as ordinary replicated properties — cheap, engine-batched, and free for join-in-progress. A client that opens a holder **requests the contents on open** and receives a snapshot (bounded by distinct item types, not item count) addressed to that player only; players nearby are unaffected — which is the whole point versus the vanilla inventory.
- Live refresh while the screen is open (another player loading the same box) re-pulls, coalesced.
- No `array<...>` on any RPC; every `Rpc()` call site hand-audited against its handler (BUG-090: arity mistakes compile clean and die on the wire). Listen-host owner responses use the `ShouldRespondLocally()` direct-call branch.
- All mutation server-authoritative through a component on `OVT_OverthrowController`.

### E. Persistence

- Ledger contents, capacity overrides and the display name persist through the vanilla persistence system (`ScriptedComponentSerializer`, version-first payload, rule in `Configs/Systems/Persistence/Overthrow.conf`); exact quantities survive save/load for boxes, vehicles and the warehouse (the warehouse's serializer bumps its version; old saves still load). Per-instance vehicle round-trip is the precedent.

### F. Cross-cutting

- **Hide/disable the base game's supply "Storage" system** where it would confuse players next to ours.
- New strings in `Language/localization_Overthrow.st` only; the user regenerates exports.
- Automated coverage (each proven able to fail): **Logic** — ledger add/take/clamp/total/free-space, full-magazine and base-clothing predicates, destination-radius and capacity rules, export price ratio; **Persistence** — a holder's ledger + name round-trip, the warehouse migration loading an old save; **Init/Campaign** — a truck and an ammo box resolve a storage component with the expected capacity. Everything involving real spawns, progress bars and the screen is a play-test gate (SP + dedicated, incl. JIP seeing the count/name without contents traffic).

## Decisions (2026-08-20, with the user)

1. **One mode (Take); only the destination varies** — Inventory (this holder's vanilla inventory) or any nearby storage holder. Putting in is "Transfer all to storage"; there is no Put mode and no per-item "put".
2. **Holders are nameable** — the name lives on the storage component, shows in the picker and the label, persists.
3. **Pull-on-open replication** — count (+ name) replicated, contents requested by the opening client only.
4. The old `core/storage` plan is **discarded**; plan fresh.
5. Port Export and the batched checkout live **here**, not in `ui`.

## Dependencies

- **`logistics/ui`** — the Open Storage / port / warehouse screens (must be complete).
- `core/game-mode` / `core/controller-migration` — `OVT_OverthrowController` components, `OVT_BaseServerProgressComponent`, `OVT_ContainerTransferComponent` (loot), `OVT_ComponentFinder`.
- `economy` epic — `OVT_EconomyManagerComponent` (prices, legality data, illegal gate), `OVT_VehicleRequestComponent` (import), `OVT_RealEstateManagerComponent` / `OVT_RealEstateRequestComponent` (warehouse), `OVT_ShopTransactionComponent` (bulk-sell routine for export).
- `resistance/fob` — the undeploy flow (`OVT_FOBRequestComponent.UndeployFOB`); `resistance` — `IsOfficer`.
- `core/persistence` — serializers and `Overthrow.conf` rules.
- Prefabs: `OVT_AmmoBox_Base.et`, `Vehicle_Base.et` and the truck/civilian prefabs; `OVT_CabinetMetal_01_grey_V1.et` also carries the old load/unload actions today.

**Downstream:** `logistics/resources` (the warehouse's item ledger it sits beside; resource Export shares the port mode), `core/damage` (hides storage actions on a ruined holder — a seam only).

## Out of Scope

- The resource ledger, volume/weight capacity, anything resource-shaped (`resources`).
- Per-item "put" into storage, partial conversion from vanilla inventory, or moving items between two holders' **vanilla** inventories.
- Any change to how the player's own character inventory works, or to the vanilla inventory UI.
- Warehouse linking (`isLinked` — dead code, delete), warehouse purchase/pricing/rent.
- Selling from anywhere except the port; trading between players.
- AI/recruits using storage.
