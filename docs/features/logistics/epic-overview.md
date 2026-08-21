# Logistics - Epic Overview

**Status:** 🟡 In Progress (2/4 features **closed**) — `ui` and `storage` both play-tested green and closed 2026-08-21; `resources` and `building-repair` still at requirements
**Last Updated:** 2026-08-21

> **This file is the epic marker.** Its presence in `docs/features/logistics/` is what tells every Beast Mode command (and future Web App / Discord clients) that this folder is an **epic**, not a plain feature. Keep it present and keep the required sections below filled in. It is the epic's equivalent of the project's `docs/overview.md`, scoped to this epic. The master `docs/overview.md` tracks this epic as a **single row**; the per-feature detail lives **here**.

---

## Purpose

The logistics epic adds a mid-game supply sub-game: a config-driven, multi-type **resource** economy that sits alongside money. Resources are bought at the port, hauled by truck in a dedicated volume-capped cargo store, dropped as crate piles on the ground, stored in warehouses, and consumed to construct the mid-game buildings that today cost only cash. It targets the point where the resistance holds a base or two, has armed vehicles and some money — turning "click Build, pay $1500" into "drive a truck to the port, buy 40 m³ of cement and steel, convoy it back, and raise the garage".

These four features belong together because they share one transfer screen and two data spines. `ui` is the shared List + Cart + Destination screen every transfer flows through; `storage` owns the type+quantity **item** ledger (moved in from `core` on 2026-08-20 because it is the same problem space and the same screen); `resources` owns the resource definitions, the typed **resource** ledger, its persistence and every way it is filled or drained (truck, pile, port, construction site, warehouse), and `building-repair` is the one consumer different enough to stay separate. Splitting them across epics would fragment one UI contract and two save-formats across many planning documents.

Repair closes the loop the other way: a destroyed building stops working and becomes a construction site again, so the war can undo the resistance's infrastructure and resources are what put it back.

**Deliberately not the vanilla resource system.** `EResourceType` (`ArmaReforger/scripts/Game/Sandbox/Resources/Container/SCR_ResourceContainer.c:1`) is a hard engine enum with exactly `SUPPLIES` and `ELECTRICITY`; a mod cannot add values to it, so `SCR_ResourceComponent`/`Container`/`Generator`/`Consumer` cannot back a multi-type resource engine. What *is* reused from Conflict is UX shape — the continuous load/unload user-action pattern (`SCR_ResourceContainerVehicleLoadAction`), the supply readout widgets, and the "virtual cargo store attached to a vehicle" idea.

---

## Features

The constituent features of this epic, in build order. This is the epic's equivalent of `docs/overview.md`'s feature-status table, scoped to the epic. Each feature is a subfolder under `docs/features/logistics/`.

| # | Feature | Status | Tasks | Description |
|---|---------|--------|-------|-------------|
| 1 | ui | ✅ **Play-test green 2026-08-21** (built same day) | 43/44 (98%) | Shared **List + Cart + Destination** base context (`OVT_UIContext` subclass) with the shop's tool header (modes + categories), scrolling rows, details, cart with Add/Remove 1/10/All, spin-box destination picker and Accept. Replaces the port Import and warehouse Take screens outright (they become thin subclasses); **no new server wiring** — Accept loops the existing per-line requests. First in the order because it depends on nothing in the epic |
| 2 | storage | ✅ **CLOSED 2026-08-21** — 10 phases + a cross-phase review + 9 user play-test fixes; user signed off. Residual: one `.st` re-export, the wiki pass, and **MP is unproven at runtime** | 76/76 + 9 fixes (100%) | Type+quantity item ledger on a nameable `OVT_StorageComponent` (warehouse, ammo boxes, trucks, civilian cars), replacing spawned-entity stockpiles; server-only batched conversion to/from vanilla inventory (full-magazine / attachment rules, delete-then-credit); **Open Storage** as a `ui` consumer with one Take mode and the destination picker doing the work (this holder's inventory or any nearby named holder); **Transfer all to storage**, officer Clear, Rename; port import / warehouse take become zero-spawn ledger moves; box Load/Unload, FOB undeploy and truck Loot converted; port **Export** (ratio below import *and* shop prices, illegal under the L5-or-controlled gate); contents **pulled on open**, only count + name replicated |
| 3 | resources | 📋 Requirements formalized 2026-08-20 (merges the former resource-core / -transport / -trade / -construction / -storage) | 0/0 (—) | The resource economy as one feature: `resources.conf` + `OVT_ResourceManagerComponent` + the m³-capped ledger (replicated, persisted, live drifting prices); a cargo store on **every** truck, merging crate piles with inspect/HUD/map markers; resource Import/Export on the shared `ui` port screen with war-biased drift and two difficulty multipliers; resource requirements + construction sites + Build-from-piles on buildables; warehouses hold resources (Take/Put on the shared screen) and become a resource-costed buildable |
| 4 | building-repair | 📋 Requirements rewritten 2026-08-20 against `core/damage` | 0/0 (—) | A **resource gate on `core/damage`'s repair seam**: `OVT_ResistanceFactionManager.RepairStructure()` also requires (and consumes from nearby crate piles) the buildable's resource requirement × `repairCostMultiplier`, with the ruin showing needed-vs-nearby. Destruction, the in-place ruin entity, disabled-state gating, serializer v2, the held money-repair action and the occupying repair module are all `core/damage`'s (In Progress, 16/70) |

> Reference any feature with the slash form `logistics/<feature>` (e.g. `/plan-feature logistics/resources`). Task counts are pulled from each feature's own `tasks.md` and refreshed by `/update-epic`.

---

## Build Order / Dependencies

1. **ui** — First, because it has **no** intra-epic dependency: it only needs today's port/warehouse screens, which it replaces. Every later feature that shows a list of things to move becomes a consumer of the base context instead of building its own screen. Decided 2026-08-20.
2. **storage** — The item-side ledger (moved in from `core` 2026-08-20). Lands before `resources` so the warehouse/port/ammobox conversions are settled and the resource ledger is designed *beside* a finished item ledger. Also owns port Export (the vehicle-inventory reader `ui` deliberately did not write).
3. **resources** — The whole resource economy in one feature (merged 2026-08-20: with the screen and the item side owned elsewhere, what remains is one config + manager + ledger, a truck component, a pile prefab, construction-site prefabs and controller request classes). Internal phase order keeps the old build order — engine/ledger → transport → trade → construction → warehouse — and still carries the epic's biggest unknown (how the cargo store reaches every truck prefab), settled in its `/plan-feature`.
4. **building-repair** — Last and separate, and now small: `core/damage` (In Progress) ships the in-place ruin, the gating, the persistence and a money-only held repair; this feature adds the resource requirement on that one seam, calling `resources`' position-based availability/consume helpers. Needs both `core/damage` and `resources` complete.

**Dependencies between features:**
- ui → storage, resources (consumers of the shared List + Cart + Destination screen)
- storage → resources (the warehouse's item inventory becomes `OVT_StorageLedger`; the resource ledger sits beside it and must stay distinct — count-keyed items vs m³-capped resources; item Export precedes resource Export on the same port mode)
- resources → building-repair (repair reuses the requirements/site/Build machinery wholesale — it must be enterable from a second caller)
- `core/damage` (outside the epic, In Progress) → building-repair (the ruin entity, `OVT_StructureDamage`, disabled-state gating, serializer v2, `RepairStructure()` seam, `repairCostMultiplier`); `resources` → `core/damage`'s retrofit (the buildable warehouse prefab must carry the destruction component and join the ruin gate)
- **Parallel opportunity inside `resources`:** trade and construction phases touch disjoint code and can be implemented in parallel once engine + transport land; only construction's play-test is blocked on trade.

**External dependencies (outside this epic):**
- `economy` epic — `OVT_EconomyManagerComponent` for player money, prices and the port purchase path (`ImportToVehicle`); `OVT_RealEstateManagerComponent` + `OVT_WarehouseData` for the warehouse that `resources` extends.
- `towns` epic — town control/support state, read by `resources` to gate illegal import/export at the port's town and to decide where a warehouse may be built.
- `resistance` epic — the shipped buildables/build path (`OVT_BuildContext`, `OVT_ResistanceFactionManager`, `OVT_BuildableComponent`), extended by `resources` and `building-repair`.
- `map` epic — marker plumbing reused by `resources` for crate piles.
- `core/persistence` — vanilla `ScriptedComponentSerializer` classes (bound by rules in `Configs/Systems/Persistence/Overthrow.conf`) for the ledger, truck stores, crate piles and half-finished construction sites, (`OVT_BuildableComponentSerializer` v2 with the ruin phase is `core/damage`'s; this epic adds no buildable serializer change).

---

## Integration & Architecture

- **Within the epic:** One `OVT_ResourceManagerComponent` singleton on the game mode owns the resource *definitions* (loaded from `resources.conf`). It does **not** own the stock — stock lives in a reusable ledger value type held by each thing that can hold resources: a truck's cargo component, a crate-pile entity, a warehouse record. Every feature therefore speaks the same three verbs (`CanFit` / `Add` / `Remove`) against the same type, and the volume cap is a property of the *holder*, not the ledger. Crate piles are declared unlimited by holding a ledger with no cap, which is what makes "unload anything into a pile" work without a special case.

- **With other epics / features:** This epic sits on top of the economy epic rather than beside it. Resources are a second currency-like axis, but they are never fungible with money except at the port, which is the single conversion point in both directions. Construction changes the *precondition* of an existing shipped flow (`OVT_BuildContext` → `OVT_ResistanceFactionManager` build path) rather than replacing it: money cost stays, resource cost is additive and optional per buildable, so any buildable with no requirements behaves exactly as it does today. `building-repair` reaches furthest outside the epic: disabling a destroyed building's function means gating capabilities that live in other epics' managers (recruitment, vehicle services, storage access), one gate per functional buildable.

- **Key architectural decisions for the epic as a whole:**
  - **One transfer screen, many consumers.** `ui` ships an abstract List + Cart + Destination base context with one shared layout and one shared `ActionContext` block; the port, warehouse, storage containers and (later) resource flows are thin subclasses supplying entries/modes/categories/destinations/Accept. No feature in this epic builds its own transfer screen. Decided 2026-08-20.
    - **Shipped 2026-08-21.** The base is `OVT_TransferContext` (`Scripts/Game/UI/Context/`), over two pure models in `Scripts/Game/Data/` (`OVT_TransferListModel`, `OVT_TransferCartModel` — Logic-tier tested) and one tab-host intermediate class (`OVT_TabHostContext`, shared with the shop). A consumer overrides **eight** hooks and writes no widget code; that hook list is **closed** — a later feature adds what it needs when it needs it, and must not assume a resource/volume-shaped hook exists. `storage` and `resources` inherit it. Read `docs/features/logistics/ui/implementation.md` §3.4 before writing a consumer, and its `context.md` for the gamepad traps (`WLib_NavigationButton` is not focusable without an override; the picker eats d-pad left/right; `array.Remove` is swap-with-last).
  - **Two ledgers, deliberately.** `storage` owns a **count-keyed item** ledger (unlimited where it makes sense); the resource features own an **m³-capped resource** ledger. They share the UI and may share ledger code as a starting point, but not a capacity model or a persistence record.
  - **Own backend, borrowed UX.** No `SCR_Resource*` types in the data path (see Purpose). Reuse Conflict's *patterns* and widget layouts only.
  - **Server-authoritative ledger.** All mutation happens server-side; clients get replicated snapshots and drive changes through a component on `OVT_OverthrowController` — never `OVT_PlayerCommsComponent` (legacy/deprecated).
  - **Volume caps, weight is data only.** Volume (m³/unit) restricts what a truck can carry. Weight (kg/unit) is defined in config and displayed, but does **not** affect vehicle handling in the MVP — Conflict does not do this either (there is no mass code anywhere in `scripts/Game/Sandbox/Resources/`), and mutating a live vehicle's `Physics.SetMass` is an unproven MP/persistence risk. Deferred, not designed out: the config field exists so a later feature can turn it on.
  - **⚠️ Open risk — how the cargo store reaches every truck.** The requirement is that *all* trucks in the game can haul resources, including vanilla prefabs spawned by any system. The three candidate approaches (same-GUID prefab delta overrides per truck prefab; runtime component attachment via the vehicle manager; a scripted prefab→capacity lookup with no component at all) have very different costs for persistence and replication. This must be settled in `/plan-feature logistics/resources` — it is the single largest unknown in the epic.
  - **A ruin borrows the construction site's machinery; it does not become one.** (Revised 2026-08-20.) The original rationale — engine destruction cannot be reversed, so treat the wreck as a new site — is gone: `core/damage` keeps the destroyed buildable as the **same entity** in a ruin phase and restores the intact mesh on repair (`OVT_StructureDestructionComponent`, `OVT_StructureDamage.Repair`). `building-repair` therefore adds a resource check to `core/damage`'s existing `RepairStructure()` seam and reuses `resources`' *position-based* nearby-availability and consume helpers on the ruin. Consequence for `resources`: those helpers must take (position, requirements), not a site entity.
  - **Prices are state, not constants.** Resource prices drift over time, so `resources` stores, replicates and persists a *current* price per resource and owns the tick that moves it. Nothing may read the config base price as if it were the live price — a UI that quotes the base while the server charges the drifted value is the predictable bug. Note this is genuinely new ground: Overthrow prices today are set once from config at init (`OVT_EconomyManagerComponent.SetPrice()`) and are never persisted.
  - **Resource costs scale with difficulty.** Money costs already do (`OVT_DifficultySettings.buildableCostMultiplier` via `OVT_OverthrowConfigComponent.GetBuildableCost()`), so resource requirements follow the identical pattern: a multiplier field in the `Economy` category, an accessor on the config component, and difficulty `.conf` files overriding only where they differ from the attribute default. `resources` owns the construction multiplier and the two price multipliers; `building-repair` adds a repair multiplier that stacks on top of the already-scaled figure — a price level multiplier and a price volatility multiplier. Displayed requirement and consumed amount must both use the scaled value. Four new multipliers land in the same `Economy` category across two features, so keep the naming consistent (`buildable*` / `resource*`) and the accessors uniform.
  - **Illegal-resource gate ships inert.** The `m_bIllegal` flag and the resistance-town-control gate at the port are built in `resources`, but the agreed MVP resource set (Timber, Cement, Steel, Hardware) contains no illegal member. The mechanic will therefore be **unexercised in the MVP** until a config flag is flipped. Accepted deliberately; noted here so no one later reads working-but-unused code as dead.

---

## Tech Debt / Findings

Cross-feature tech debt and review findings. **Populated and updated by `/review-epic`** (which also writes feature-specific findings into the relevant child `context.md` files). Start empty.

- (none yet — `/review-epic` will surface cross-feature integration issues and per-feature tech debt here)

---

## Master Overview Rollup

- **Rollup status:** In Progress (2/4 features **closed**, 119/120 tasks across the epic) — `ui` play-tested green 2026-08-21 and `storage` built the same day with every automated gate green; `resources` and `building-repair` are formalized requirements awaiting `/plan-feature`
- **One-line summary for master:** Mid-game supply sub-game. Two of four features are built: the shared List + Cart + Destination transfer screen (play-tested green), and the **item ledger** that replaces spawned-entity stockpiles outright — one `OVT_StorageComponent` on three shared prefab bases reaches every wheeled vehicle, ammo box and warehouse building, contents never leave the server except to the one player who opened a holder, and the two worst spike sites (port import, warehouse take) are now zero-spawn ledger arithmetic. Port Export is new; `OVT_WarehouseData.inventory`, the warehouse RPCs and `OVT_WarehouseContext` are deleted. Owed on `storage`: the string-table re-export, Workbench checks and three play-test sessions. Next: the multi-resource economy hauled by truck, traded at the port and consumed to construct, store and repair buildings.

---

*This is a required file — do not delete it; it marks the folder as an epic. Update it with `/update-epic logistics` after working on the epic's features, and run `/review-epic logistics` to refresh the Tech Debt / Findings section.*
