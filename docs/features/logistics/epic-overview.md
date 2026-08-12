# Logistics - Epic Overview

**Epic:** logistics
**Status:** 📋 Planned (0/6 features)
**Last Updated:** 2026-08-11 00:00

> **This file is the epic marker.** Its presence in `docs/features/logistics/` is what tells every Beast Mode command (and future Web App / Discord clients) that this folder is an **epic**, not a plain feature. Keep it present and keep the required sections below filled in. It is the epic's equivalent of the project's `docs/overview.md`, scoped to this epic. The master `docs/overview.md` tracks this epic as a **single row**; the per-feature detail lives **here**.

---

## Purpose

The logistics epic adds a mid-game supply sub-game: a config-driven, multi-type **resource** economy that sits alongside money. Resources are bought at the port, hauled by truck in a dedicated volume-capped cargo store, dropped as crate piles on the ground, stored in warehouses, and consumed to construct the mid-game buildings that today cost only cash. It targets the point where the resistance holds a base or two, has armed vehicles and some money — turning "click Build, pay $1500" into "drive a truck to the port, buy 40 m³ of cement and steel, convoy it back, and raise the garage".

These six features belong together because they are one data spine with five consumers. `resource-core` owns the resource definitions, the typed ledger and its persistence; transport, trade, construction, storage and repair are each a different way that ledger is filled or drained. Splitting them across epics would fragment one save-format and one replication contract across five planning documents.

Repair closes the loop the other way: a destroyed building stops working and becomes a construction site again, so the war can undo the resistance's infrastructure and resources are what put it back.

**Deliberately not the vanilla resource system.** `EResourceType` (`ArmaReforger/scripts/Game/Sandbox/Resources/Container/SCR_ResourceContainer.c:1`) is a hard engine enum with exactly `SUPPLIES` and `ELECTRICITY`; a mod cannot add values to it, so `SCR_ResourceComponent`/`Container`/`Generator`/`Consumer` cannot back a multi-type resource engine. What *is* reused from Conflict is UX shape — the continuous load/unload user-action pattern (`SCR_ResourceContainerVehicleLoadAction`), the supply readout widgets, and the "virtual cargo store attached to a vehicle" idea.

---

## Features

The constituent features of this epic, in build order. This is the epic's equivalent of `docs/overview.md`'s feature-status table, scoped to the epic. Each feature is a subfolder under `docs/features/logistics/`.

| # | Feature | Status | Tasks | Description |
|---|---------|--------|-------|-------------|
| 1 | resource-core | 📋 Planned | — | Config-driven resource engine: `resources.conf`, `OVT_ResourceManagerComponent`, the typed ledger (id→qty) with volume/weight maths, replication and persistence |
| 2 | resource-transport | 📋 Planned | — | Truck resource cargo (volume-capped), mixed load/unload actions, crate-pile drop entity that merges into nearby piles, inspect action, cargo HUD, map markers for piles |
| 3 | resource-trade | 📋 Planned | — | Port "Import Resources" / "Export Resources" flow for a player in a truck: importable + illegal flags, resistance-town-control gate, and a war-biased price drift with difficulty-driven level and volatility |
| 4 | resource-construction | 📋 Planned | — | Buildables gain resource requirements; placement spawns a construction site (config prefab, fallback sign) with a requirements action and a Build action that consumes nearby piles |
| 5 | resource-storage | 📋 Planned | — | Warehouses hold resources alongside item prefabs, and the warehouse becomes a **buildable** at bases and in resistance-controlled towns |
| 6 | building-repair | 📋 Planned | — | Destroyed Overthrow buildings are disabled and become construction sites again, repairable from nearby crate piles at a difficulty-driven fraction of the original cost |

> Reference any feature with the slash form `logistics/<feature>` (e.g. `/plan-feature logistics/resource-core`). Task counts are pulled from each feature's own `tasks.md` and refreshed by `/update-epic`.

---

## Build Order / Dependencies

1. **resource-core** — Foundational and unavoidably first. Every sibling reads resource definitions and mutates the same ledger; the ledger's save format and replication contract must be settled before five features start writing to it.
2. **resource-transport** — The earliest vertical slice with something to *see*: a truck that carries a mixed load, drops a pile, and shows it on the map. It also de-risks the epic's biggest unknown early (how a resource cargo store attaches to *every* truck in the game — see Key architectural decisions).
3. **resource-trade** — The only **source** of resources in the MVP (sourcing is import-only). Nothing downstream is play-testable until a player can actually buy timber, so trade lands before construction even though the two are code-independent.
4. **resource-construction** — The **sink**, and the reason the sub-game exists. Needs core (requirements config), transport (piles to consume) and trade (resources in existence) all present to be testable end to end.
5. **resource-storage** — Depends on the most: it is itself a buildable with a resource cost (#4), it must accept loads from a truck (#2), and it holds ledger contents (#1) alongside the existing item-prefab warehouse inventory.
6. **building-repair** — Last. It is a second entry point into #4's requirements/site/Build machinery, so that must be proven first; and going after #5 means the buildable warehouse is included in repair's per-building disabled-state gating pass rather than retrofitted into it.

**Dependencies between features:**
- resource-core → all five siblings (definitions, ledger type, persistence, volume maths)
- resource-transport → resource-trade (a truck cargo store to buy *into*)
- resource-transport → resource-construction (crate piles are what the Build action consumes)
- resource-trade → resource-construction (play-test dependency only — trade is the MVP's sole resource source; the code paths do not touch)
- resource-construction + resource-transport + resource-core → resource-storage
- resource-construction → building-repair (repair reuses the requirements/site/Build machinery wholesale)
- resource-storage → building-repair (ordering only — no code dependency; it puts the buildable warehouse inside repair's disabled-state gating pass)
- **Parallel opportunity:** resource-trade and resource-construction touch disjoint code (port UI/economy vs. buildables/user actions) and can be *implemented* in parallel once core and transport land; only the play-test of construction is blocked on trade.

**External dependencies (outside this epic):**
- `economy` epic — `OVT_EconomyManagerComponent` for player money, prices and the port purchase path (`ImportToVehicle`); `OVT_RealEstateManagerComponent` + `OVT_WarehouseData` for the warehouse that `resource-storage` extends.
- `towns` epic — town control/support state, read by `resource-trade` to gate illegal import/export at the port's town, and by `resource-storage` to decide where a warehouse may be built.
- `resistance` epic — the shipped buildables/build path (`OVT_BuildContext`, `OVT_ResistanceFactionManager`, `OVT_BuildableComponent`), extended by `resource-construction` and `building-repair`.
- `map` epic — marker plumbing reused by `resource-transport` for crate piles.
- `core/persistence` — vanilla `ScriptedComponentSerializer` classes (bound by rules in `Configs/Systems/Persistence/Overthrow.conf`) for the ledger, truck stores, crate piles and half-finished construction sites, plus a **version 2** bump of `OVT_BuildableComponentSerializer` for `building-repair`'s destroyed flag.

---

## Integration & Architecture

- **Within the epic:** One `OVT_ResourceManagerComponent` singleton on the game mode owns the resource *definitions* (loaded from `resources.conf`). It does **not** own the stock — stock lives in a reusable ledger value type held by each thing that can hold resources: a truck's cargo component, a crate-pile entity, a warehouse record. Every feature therefore speaks the same three verbs (`CanFit` / `Add` / `Remove`) against the same type, and the volume cap is a property of the *holder*, not the ledger. Crate piles are declared unlimited by holding a ledger with no cap, which is what makes "unload anything into a pile" work without a special case.

- **With other epics / features:** This epic sits on top of the economy epic rather than beside it. Resources are a second currency-like axis, but they are never fungible with money except at the port, which is the single conversion point in both directions. Construction changes the *precondition* of an existing shipped flow (`OVT_BuildContext` → `OVT_ResistanceFactionManager` build path) rather than replacing it: money cost stays, resource cost is additive and optional per buildable, so any buildable with no requirements behaves exactly as it does today. `building-repair` reaches furthest outside the epic: disabling a destroyed building's function means gating capabilities that live in other epics' managers (recruitment, vehicle services, storage access), one gate per functional buildable.

- **Key architectural decisions for the epic as a whole:**
  - **Own backend, borrowed UX.** No `SCR_Resource*` types in the data path (see Purpose). Reuse Conflict's *patterns* and widget layouts only.
  - **Server-authoritative ledger.** All mutation happens server-side; clients get replicated snapshots and drive changes through a component on `OVT_OverthrowController` — never `OVT_PlayerCommsComponent` (legacy/deprecated).
  - **Volume caps, weight is data only.** Volume (m³/unit) restricts what a truck can carry. Weight (kg/unit) is defined in config and displayed, but does **not** affect vehicle handling in the MVP — Conflict does not do this either (there is no mass code anywhere in `scripts/Game/Sandbox/Resources/`), and mutating a live vehicle's `Physics.SetMass` is an unproven MP/persistence risk. Deferred, not designed out: the config field exists so a later feature can turn it on.
  - **⚠️ Open risk — how the cargo store reaches every truck.** The requirement is that *all* trucks in the game can haul resources, including vanilla prefabs spawned by any system. The three candidate approaches (same-GUID prefab delta overrides per truck prefab; runtime component attachment via the vehicle manager; a scripted prefab→capacity lookup with no component at all) have very different costs for persistence and replication. This must be settled in `/plan-feature logistics/resource-transport` — it is the single largest unknown in the epic.
  - **A ruin is a construction site, not a repair mechanic.** `building-repair` deliberately has no separate repair path: a destroyed building presents the same requirements action, nearby-pile check and Build action as an unbuilt one, at a difficulty-driven fraction of the original cost (a new `OVT_DifficultySettings` multiplier in the `Economy` category, applied through an `OVT_OverthrowConfigComponent` accessor shaped like `GetBuildableCost()`). This avoids depending on reversing the engine's destruction (`GoToDamagePhase` is public but one-directional in practice, and `m_bDeleteAfterFinalPhase` prefabs leave no entity to heal), and it means construction's machinery is exercised twice rather than duplicated. Consequence: `resource-construction` must be planned so its site/requirements/Build flow can be entered from a second caller, not hard-wired to the placement path.
  - **Prices are state, not constants.** Resource prices drift over time, so `resource-core` stores, replicates and persists a *current* price per resource while `resource-trade` owns the tick that moves it. Nothing may read the config base price as if it were the live price — a UI that quotes the base while the server charges the drifted value is the predictable bug. Note this is genuinely new ground: Overthrow prices today are set once from config at init (`OVT_EconomyManagerComponent.SetPrice()`) and are never persisted.
  - **Resource costs scale with difficulty.** Money costs already do (`OVT_DifficultySettings.buildableCostMultiplier` via `OVT_OverthrowConfigComponent.GetBuildableCost()`), so resource requirements follow the identical pattern: a multiplier field in the `Economy` category, an accessor on the config component, and difficulty `.conf` files overriding only where they differ from the attribute default. `resource-construction` owns the construction multiplier; `building-repair` adds a repair multiplier that stacks on top of the already-scaled figure; `resource-trade` adds two more — a price level multiplier and a price volatility multiplier. Displayed requirement and consumed amount must both use the scaled value. Four new multipliers land in the same `Economy` category across three features, so keep the naming consistent (`buildable*` / `resource*`) and the accessors uniform.
  - **Illegal-resource gate ships inert.** The `m_bIllegal` flag and the resistance-town-control gate at the port are built in `resource-trade`, but the agreed MVP resource set (Timber, Cement, Steel, Hardware) contains no illegal member. The mechanic will therefore be **unexercised in the MVP** until a config flag is flipped. Accepted deliberately; noted here so no one later reads working-but-unused code as dead.

---

## Tech Debt / Findings

Cross-feature tech debt and review findings. **Populated and updated by `/review-epic`** (which also writes feature-specific findings into the relevant child `context.md` files). Start empty.

- (none yet — `/review-epic` will surface cross-feature integration issues and per-feature tech debt here)

---

## Master Overview Rollup

- **Rollup status:** Planned (0/6 features)
- **One-line summary for master:** Mid-game supply sub-game — a config-driven multi-resource economy hauled by truck, dropped as crate piles, traded at the port and consumed to construct, store and repair buildings.

---

*This is a required file — do not delete it; it marks the folder as an epic. Update it with `/update-epic logistics` after working on the epic's features, and run `/review-epic logistics` to refresh the Tech Debt / Findings section.*
