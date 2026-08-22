# Resources — Requirements

**Epic:** logistics
**Created:** 2026-08-20 — merges the former `resource-core`, `resource-transport`, `resource-trade`, `resource-construction` and `resource-storage` features (planned 2026-08-11) into one feature. With `logistics/ui` owning the transfer screen and `logistics/storage` owning the item side, what is left of the resource system is one data spine plus components, prefabs and controller classes — not five features. `building-repair` stays separate.

## Overview

`resources` adds the multi-type resource economy the epic exists for: a config-driven set of resources (Timber, Cement, Steel, Hardware in the MVP) with a reusable m³-capped ledger; every truck gets a volume-capped resource cargo store; unloading drops merging crate piles that show on the map; the port imports and exports resources on the shared `ui` screen at a war-biased, drifting price; mid-game buildables gain resource requirements and go up as construction sites supplied from nearby piles; and warehouses hold resources alongside items and become buildable. All of it is server-authoritative, replicated, persisted and difficulty-scaled in the shipped patterns.

**Deliberately not the vanilla resource system.** `EResourceType` is a hard engine enum (`SUPPLIES`, `ELECTRICITY`) a mod cannot extend, so no `SCR_Resource*` type is in the data path; only Conflict's UX patterns (continuous load/unload actions, supply readouts) are borrowed.

**Deliberately not `storage`'s ledger.** `logistics/storage` owns a count-keyed **item** ledger with unlimited capacity where it makes sense; this feature owns an **m³-capped resource** ledger with its own capacity model and persistence record. They share the `ui` screen and may share ledger code as a starting point, but they do not merge.

## Requirements

### A. Engine and ledger (was `resource-core`)

- A resource config (e.g. `Configs/Resistance/resources.conf` + `OVT_ResourcesConfig` / `OVT_Resource`, following the `OVT_BuildablesConfig` shape) defines all resources. Adding, removing or re-pricing a resource requires **no** script change.
- Each definition carries at minimum: stable id, localized title and description, icon, **volume per unit (m³)**, **weight per unit (kg)**, `importable` flag, `illegal` flag, base price.
- MVP set: **Timber, Cement, Steel, Hardware/Tools** — all legal and importable, priced so a mid-game building costs a meaningful but achievable truckload. (Consequence: the illegal mechanic ships functional but unexercised until a config flag is flipped — do not remove it as dead code; prove it by temporarily flagging a resource in tests.)
- An `OVT_ResourceManagerComponent` singleton on the game mode owns the definitions (lookup by id and index, aggregate m³/kg for a set) and is reachable through `OVT_Global` in the manager pattern.
- A **reusable resource ledger type** (id → qty) is the one representation of "some resources", used identically by truck stores, crate piles and warehouses: query, add, remove, enumerate non-zero, total volume, total weight, "would this fit in N m³". **Capacity belongs to the holder, not the ledger** — piles and warehouses are unlimited simply by applying no cap.
- Replicable and persistable through vanilla persistence (`ScriptedComponentSerializer`, version-first positional payload, rule in `Configs/Systems/Persistence/Overthrow.conf`); **sparse** serialization keyed on stable id so adding a resource to config never shifts or corrupts saved stock.
- **Current price is per-resource state, not a constant**: initialised to base on a new campaign, **persisted** (there is no precedent — Overthrow prices are set once via `OVT_EconomyManagerComponent.SetPrice()` and never saved), replicated, server-authoritative, keyed on id so a later-added resource starts at base. Nothing may read the config base as if it were the live price.
- Weight is defined, aggregated and displayable but **never affects vehicle handling** (Conflict has no mass code either; `Physics.SetMass` on a live vehicle is an unproven MP/persistence risk). The field exists so a later feature can enable it.

### B. Transport (was `resource-transport`)

- **Every truck in the game can haul resources** — all truck-class vehicles including vanilla prefabs spawned by any system, not a bespoke supply truck. ⚠️ **The central architectural decision of this feature** — resolve during planning: same-GUID prefab delta overrides per truck prefab, runtime component attachment via `OVT_VehicleManagerComponent`, or a scripted prefab→capacity table with no per-prefab component. Their persistence and replication costs differ substantially.
- The cargo store is **separate from the vehicle's item inventory** (and from `storage`'s item ledger on the same truck); loading never touches trunk slots.
- **Volume capacity in m³ per truck class/prefab**, configurable, so a pickup and a flatbed differ.
- **Mixed loads**, constrained only by combined volume; a load that would exceed the cap is rejected (or clamped — decide once, apply the same rule at the port) with a clear reason.
- **Partial, chosen-quantity load and unload** — the player picks resource and amount. UX shape: Conflict's continuous user-action pattern (`SCR_ResourceContainerVehicleLoadAction`) for the in-world actions; the `ui` screen where a list/cart fits better (warehouse, port).
- **Unloading spawns a crate pile** holding the unloaded resources; piles have **no capacity limit**. **Piles merge**: unloading searches a configurable radius for an existing pile and adds to it (deterministic, server-side) before spawning a new one. Visuals: one generic stack prefab, or per-resource prefabs with a generic fallback — decide on available assets.
- **Inspect action** on a pile lists contents (resource, qty, total m³).
- **Cargo HUD** while in a truck with a load: contents and used/total m³ (weight may be shown).
- **Map markers** for piles via the `map` epic's marker plumbing.
- Truck contents and piles **persist** (incl. piles created and merged in the same session); a truck's load survives the vehicle despawn/respawn cycle (per-instance vehicle round-trip precedent in the Persistence tier).

### C. Port trade (was `resource-trade`)

- Resources are a **category/mode on the shared `ui` port screen** — no separate screen, layout or `ActionContext`. This feature supplies the resource entries, the Import/Export rules and the Accept behaviour for resource lines (item Export is `storage`'s).
- **Requires a truck**: available only to a player seated in a vehicle with a resource cargo store; buying deposits directly into it. If not in a suitable vehicle, show the reason rather than hiding the option.
- **Capacity-aware buying** — consistent with B's load rule (reject or clamp).
- **Import gating on `importable`** — non-importable resources are not listed at all (BUG-102: a row whose only outcome is a no-op click is a bug).
- **Export sells anything** in the truck's store, including non-importable, at the **live** price × a configurable sell ratio.
- **Illegal resources are control-gated** server-side: import/export only when the resistance controls the port's town — reuse the shipped gate `OVT_EconomyManagerComponent.ResistanceControlsNearestPort` / `IllegalImports` (Trade L5) permission from `5a0ab51f`.
- **Prices drift**: server-only, on `OVT_EconomyManagerComponent.CheckUpdate`'s hour-gated clock using the `m_iHourPaid*` guard idiom (cadence chosen in planning — per day or per 6-hour block); **bounded** to a config band around base (e.g. 0.5×–2.0×); **biased by the war** (random walk + pressure term from resistance control of the port's town and/or occupying threat — choose cheap server-side inputs and document the mapping); sell price follows via the ratio (no second walk). Speculation across a cycle is intended; the price shown must be the price charged.
- **Difficulty drives price two ways** — `OVT_DifficultySettings` fields in the `Economy` category via `OVT_OverthrowConfigComponent` accessors, like `buildableCostMultiplier`/`vehiclePriceMultiplier`: a **level multiplier** (e.g. `resourcePriceMultiplier`) applied to the *drifted* value, and a **volatility multiplier** (e.g. `resourcePriceVolatility`) scaling the step size, **not** the clamp band. State and assert the composition order. Difficulty `.conf` files override only non-default values (`Difficulty_Normal.conf`/`Difficulty_TestWorld.conf` list none).
- **Drift must be legible**: the `ui` details panel and/or value column show each resource's price relative to base (direction/degree at minimum).
- Money moves through `OVT_EconomyManagerComponent` so balances and money-changed events behave exactly as for items; the client requests through a component on `OVT_OverthrowController` and the server re-validates capacity, flags, town control, money before mutating.

### D. Construction (was `resource-construction`)

- **`OVT_Buildable` gains an optional resource requirement list** (id → qty) in `buildables.conf`; an empty list behaves **exactly as today** (money only, instant). Additive — `m_iCost` is unchanged.
- **`OVT_Buildable` gains an optional construction-site prefab**; a generic "under construction" sign is the fallback. The existing build flow (`OVT_BuildContext` → the resistance faction manager's build path) **branches** on requirements — no parallel placement system.
- **Requirements action** on a site shows per resource: needed vs **available nearby** (summed over piles within a configurable radius). **Build action** is available only when satisfied; when not, its name/reason states which resource is short rather than hiding. Build consumes from piles server-side in deterministic order, destroys the site, and spawns the finished building **through the same path as the instant build** (ownership, registration, `m_iRewardXP`, build events all fire). Emptied piles are cleaned up.
- **The requirements/consume machinery must be position-based, not site-bound.** Expose "nearby availability for (position, requirements)" and "consume (position, requirements)" as helpers callable against any entity — `building-repair` calls them on a `core/damage` ruin (the same entity in a ruin phase), which never becomes a site. The readout (needed vs available nearby, what is short) must likewise be reusable from the ruin's repair action.
- **Sites persist** with buildable identity and placement, and are completable in a later session; removable through the existing removal flow without orphaned state.
- **Money is still charged** — decide placement-vs-completion and what happens on abandonment; no free buildings via place-then-remove.
- **Difficulty-scaled resource requirement** — `buildableResourceCostMultiplier` (`defvalue: "1"`) in the `Economy` category + an accessor shaped like `GetBuildableCost()` returning a per-resource quantity with a rounding rule that never turns non-zero into zero; displayed and consumed amounts are **both** the scaled figure. `building-repair` stacks `core/damage`'s `repairCostMultiplier` on top of this one.
- MVP requirement lists go on **existing** mid-game buildables (Garage first; Guard Tower, Helipad, tents are candidates), balanced against their money costs. No new buildings.

### E. Warehouse resource storage (was `resource-storage`)

- **Warehouses hold resources as well as items**: a separate resource ledger beside the item inventory (`storage`'s `OVT_StorageLedger` by then). Both visible and usable from the shared `ui` warehouse screen — resources as entries in **Take and Put** modes (Put is the truck-deposit path). Preserve one-handler-per-button (BUG-081).
- Loading a truck from a warehouse is m³-checked exactly as loading from a pile.
- **Warehouse becomes a buildable** at bases and in resistance-controlled towns (`m_bBuildAtBase`, `m_bBuildInTown`; villages/FOBs decided in planning) and **costs resources** via D's mechanism.
- **A built warehouse registers as a real warehouse** — in `OVT_RealEstateManagerComponent`'s list, on the map, identical to a purchased one for every consumer. No parallel "built warehouse" concept. **Ownership of a built warehouse** must be settled in planning — the main integration risk.
- Warehouse resource contents **persist** alongside the item inventory with exact quantities.

### F. Cross-cutting

- **Server-authoritative** everywhere: all ledger mutation on the server; client requests via components on `OVT_OverthrowController` (never the deleted `OVT_PlayerCommsComponent`); results replicate to all clients **including JIP**.
- **Three new `OVT_DifficultySettings` multipliers in this feature** (`buildableResourceCostMultiplier`, `resourcePriceMultiplier`, `resourcePriceVolatility`) beside `core/damage`'s `repairCostMultiplier` (which `building-repair` reuses for the resource repair fraction) — keep naming `buildable*`/`resource*` and accessors uniform; `OVT_OverthrowConfigComponent`'s JIP difficulty stream is positional and versioned (`CONFIG_STREAM_VERSION`, damage bumps 4→5), so each new field appends and bumps again.
- New strings in `Language/localization_Overthrow.st` only; the user regenerates `.conf` exports in Workbench.
- Automated coverage (each new case proven able to fail): **Logic** — ledger maths (aggregation, fit, add/remove edges incl. over-remove), capacity rejection, merge-radius selection, drift step (both clamp edges, volatility scales step, multiplier composition order, war-pressure direction, one-drift-per-window guard), importable/illegal predicates, requirements-satisfied predicate (exact / short-by-one / surplus / multi-pile sum) and consumption order, deposit/withdraw capacity rules; **Init** — manager resolves, config populates; **Persistence** — truck load, crate pile, construction site, warehouse resource stock round-trips; **Campaign** (if the test world allows) — a port purchase moving money and resources; a built warehouse registered like a purchased one.

### Suggested phase map (for `/plan-feature`)

The former build order survives as phase order: **A engine+ledger → B transport → C trade → D construction → E warehouse** — C and D touch disjoint code and may be implemented in parallel after B; C must land before D is play-testable (import is the MVP's only resource source).

## Dependencies

- **`logistics/ui`** — the port and warehouse screens this feature adds resource entries/modes to (must be complete).
- **`logistics/storage`** — the warehouse's item inventory by then, port item Export (resource Export shares the mode), and the boundary the resource ledger stays distinct from.
- `economy` epic — `OVT_EconomyManagerComponent` (money, `SetPrice` precedent, `CheckUpdate`/`m_iHourPaid*`, `ResistanceControlsNearestPort`), `OVT_RealEstateManagerComponent` / `OVT_WarehouseData` / `OVT_RealEstateConfig.m_IsWarehouse` and the warehouse map location.
- `towns` epic — town control state (illegal gate, war pressure, where a warehouse may be built); occupying-faction threat if chosen as a pressure input.
- `resistance` epic — `OVT_BuildContext`, `OVT_ResistanceFactionManager`, `OVT_BuildableComponent`, `buildables.conf`, the removal flow; `OVT_VehicleManagerComponent` if runtime attachment is chosen.
- `map` epic — marker plumbing for piles.
- `core/persistence` — `ScriptedComponentSerializer`/`ScriptedEntitySerializer`, `OVT_PersistenceTracking.Track()` for spawned entities, `OVT_BuildableComponentSerializer` as the precedent.
- `core/controller-migration` — the `OVT_OverthrowController` component pattern.
- Vanilla (patterns only): `SCR_ResourceContainerVehicleLoad/UnloadAction`, Conflict supply HUD.

## Out of Scope

- Any transfer-screen UI of its own (that is `ui`), and item storage/export (that is `storage`).
- Production, salvage, passive town production — sourcing is **import-only**; buying resources directly into a warehouse or pile.
- Weight affecting handling; player supply-and-demand pricing; per-port price variation; drift for non-resource items; smuggling beyond the binary control gate; trading resources between players or a resistance-funds path.
- Build time, timers, progress bars, multi-stage construction, banking partial deliveries into a site; consuming from a truck store or a warehouse directly at a site (piles only); upkeep; demolition refunds; new buildings beyond the warehouse.
- Warehouse resource capacity limits, inter-warehouse routing, selling from a warehouse, changing how purchased warehouses are acquired/priced/rented, other storage building types.
- Piles lootable/destructible by the occupying faction, convoy interdiction, AI/recruit hauling, non-truck vehicles carrying resources.
- Destruction and repair — `core/damage` (ruin, gating, money repair) and `building-repair` (the resource gate).
