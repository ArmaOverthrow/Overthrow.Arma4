# Logistics - Epic Requirements

**Created:** 2026-08-11
**Phase:** Planning

> Epic-level requirements — the higher-level scope for the whole epic, mirroring a feature's `requirements.md` but one level up. `/plan-epic logistics` reads this file (if it exists) to drive epic scoping; otherwise it uses the prompt. Each **child feature** below gets its own `requirements.md` (in the standard Overview / Requirements / Dependencies / Out of Scope shape) that `/plan-feature logistics/<feature>` consumes.

## Overview

The logistics epic introduces a config-driven, multi-type resource economy aimed at the mid game, when the resistance holds a base or two and has trucks and cash. Resources are imported at the port, hauled in a dedicated volume-capped truck cargo store, dropped as merging crate piles, stored in warehouses, and consumed to construct — and to repair — the buildings that today cost only money. It owns resource definitions, the resource ledger and its persistence, plus the five systems that fill or drain that ledger — transport, port trade, construction, storage and repair.

## Requirements

- **Config-driven resource engine.** Resources are defined in a config (`Configs/.../resources.conf`), not hard-coded. Each definition carries at minimum: id/name, localized title and description, icon, volume per unit (m³), weight per unit (kg), `importable` flag, `illegal` flag and a base price. Adding a resource must require no script change.
- **MVP resource set:** Timber, Cement, Steel, Hardware/Tools — all legal and importable. (Consequence: the illegal mechanic ships functional but with no MVP member exercising it.)
- **A single reusable resource ledger.** One value type (resource id → quantity) used by every holder — truck cargo store, crate pile, warehouse — with shared volume/weight aggregate maths. Holders impose the capacity; the ledger itself does not.
- **Volume restricts, weight informs.** A truck's load is capped by total m³. Weight is stored, aggregated and displayed, but has **no** effect on vehicle handling in this epic.
- **Every truck can haul.** All trucks in the game gain a resource cargo store, not a bespoke supply-truck prefab. Mixed loads are supported — a truck may carry several resource types simultaneously up to its combined volume cap.
- **Load and unload are mixed and partial.** A player can load or unload chosen resources in chosen quantities, not one whole type at a time.
- **Unloading produces merging crate piles.** Unloading spawns a crate-stack entity. If a pile already exists nearby, the unloaded resources are added to it instead of spawning a second. Piles have no capacity limit.
- **Piles are inspectable and findable.** A user action on a pile lists its contents; piles are shown on the player map so a truckload is not lost in a field.
- **Buildings can require resources.** Buildables gain an optional resource requirement list. A buildable with no requirements behaves exactly as it does today (money only) — this is additive, not a replacement for the existing cost model.
- **Resource costs are difficulty-driven, like money already is.** Both the initial construction requirement and the repair fraction are `OVT_DifficultySettings` multipliers in the `Economy` category, applied through `OVT_OverthrowConfigComponent` accessors shaped like the existing `GetBuildableCost()`. Repair's multiplier stacks on top of construction's scaled figure.
- **Construction is a two-stage flow.** Placing a resource-requiring buildable spawns a **construction site** — a per-buildable prefab defined in `buildables.conf`, falling back to a generic "under construction" marker when none is defined. An action on the site shows what is required and what is available nearby (in surrounding crate piles). When requirements are satisfied, a **Build** action unlocks; performing it consumes the resources and replaces the site with the finished building.
- **Port trade in both directions.** A new "Import Resources" flow at the port, separate from the existing item Import screen, available to a player seated in a truck with resource capacity. Importable resources can be bought; **any** resource can be sold/exported, including ones that cannot be imported.
- **Resource prices drift slowly over time.** The config price is a base; the live price wanders around it in a bounded random walk, biased by war state (control of the port's town / occupying-faction threat) so a contested port is felt in the wallet. Drift ticks server-side on the existing economy clock and the drifted prices persist and replicate. Speculating across a drift cycle is an intended money loop, not an exploit to design out.
- **Difficulty drives resource prices two ways** — a level multiplier applied to the drifted price, and a volatility multiplier scaling the size of each drift step. Both are `OVT_DifficultySettings` fields in the `Economy` category applied through `OVT_OverthrowConfigComponent`, like the existing cost multipliers.
- **Illegal resources are control-gated.** Resources flagged illegal may only be imported or exported when the resistance controls the town the port belongs to.
- **Destroyed buildings are disabled and repairable.** An Overthrow-built structure that reaches `EDamageState.DESTROYED` stops providing its function, and its wreck becomes a construction site again — same requirements action, same nearby-pile check, same Build action, at a **difficulty-driven** fraction of the original cost (a new `OVT_DifficultySettings` multiplier following the shipped `buildableCostMultiplier` pattern). There is no separate repair mechanic and no in-place healing of engine destruction.
- **Warehouses become the base-side store.** Warehouses hold resources alongside their existing item-prefab inventory, and the warehouse becomes buildable at bases and in resistance-controlled towns rather than only purchasable as real estate.
- **Everything persists.** Resource stock in truck stores, crate piles, warehouses and half-finished construction sites survives save/load through the vanilla persistence system, as does a building's destroyed-pending-repair state (`OVT_BuildableComponentSerializer` version 2, backwards-compatible with version 1 saves).
- **Server-authoritative and MP-correct.** All ledger mutation happens on the server; client requests go through a component on `OVT_OverthrowController` (never `OVT_PlayerCommsComponent`). State must be correct for join-in-progress clients.

## Planned Features

1. **resource-core** — Config-driven resource engine: `resources.conf`, `OVT_ResourceManagerComponent`, the typed ledger with volume/weight maths, replication and persistence. — Foundational; the save format and replication contract must be settled before five consumers start writing to it.
2. **resource-transport** — Truck resource cargo store, mixed load/unload actions, crate-pile drop entity with nearby-merge, inspect action, cargo HUD, map markers. — First visible vertical slice, and it settles the epic's biggest unknown (how the cargo store reaches every truck prefab).
3. **resource-trade** — Port Import/Export Resources flow, importable + illegal flags, town-control gate, pricing. — Needs a truck store (#2) to buy into; it is the MVP's only source of resources, so it must land before construction is play-testable.
4. **resource-construction** — Resource requirements on buildables, construction-site prefabs with fallback, nearby-pile requirement check, Build action that consumes and swaps in the building. — The sink; needs #1 for requirements config, #2 for piles to consume and #3 for resources to exist.
5. **resource-storage** — Warehouses hold resources alongside items and become buildable at bases and in resistance-controlled towns. — Depends on the most: it is itself a resource-costed buildable (#4), loads from trucks (#2) and holds ledger contents (#1).
6. **building-repair** — Destroyed buildings are disabled and their ruins become construction sites, repairable from nearby piles at a difficulty-driven fraction of the original cost. — Last: it is a second entry point into #4's machinery, and going after #5 puts the buildable warehouse inside its per-building disabled-state gating pass.

## Dependencies

- **economy epic** — `OVT_EconomyManagerComponent` (player money, prices, the port purchase path), `OVT_RealEstateManagerComponent` / `OVT_WarehouseData` (the warehouse that `resource-storage` extends).
- **towns epic** — town control state, read by `resource-trade` for the illegal import/export gate and by `resource-storage` for where a warehouse may be built.
- **resistance epic** — the shipped buildables/build path (`OVT_BuildContext`, `OVT_ResistanceFactionManager`, `OVT_BuildableComponent`), extended by construction and repair.
- **Other epics' managers** — `building-repair` must gate each destroyed building's function where that function is offered (recruitment, vehicle services, storage access), which reaches into code owned by other epics.
- **map epic** — marker plumbing reused for crate piles in `resource-transport`.
- **core/persistence** — vanilla `ScriptedComponentSerializer` classes for the ledger, truck stores, crate piles and construction sites, each bound by a rule in `Configs/Systems/Persistence/Overthrow.conf`.
- **Vanilla reference (patterns only, not types):** `SCR_ResourceContainerVehicleLoad/UnloadAction` and the Conflict supply HUD, for UX shape.

## Out of Scope

- **The vanilla `SCR_Resource*` backend.** `EResourceType` is a hard engine enum (`SUPPLIES`, `ELECTRICITY`) that mods cannot extend, so it cannot back a multi-type engine. UX patterns are reused; the types are not.
- **Weight affecting vehicle handling.** Weight is config data and a display value only. Conflict does not do this either, and mutating a live vehicle's mass is an unproven MP/persistence risk. The config field exists so a later feature can enable it.
- **Production chains and factories.** The whole point of the `importable` flag is to leave room for resources that must be manufactured — but nothing produces resources in this epic. Sourcing is import-only.
- **Salvage / scavenging** as a resource source (dismantling wrecks, destroyed buildings, captured bases).
- **Passive town/industry resource production.**
- **Crate piles being lootable or destructible by the occupying faction**, and any convoy-interdiction gameplay around resource transport.
- **AI or recruit-driven hauling.** Trucks are driven by players.
- **New buildings other than the warehouse.** The barracks / high-command buildings mentioned as motivation are a later epic; this one delivers the machinery and applies it to existing buildables.
- **Resource-consuming upkeep.** Buildings cost resources to *construct* and to *repair after destruction* only; nothing drains resources over time.
- **Reversing engine destruction in place.** No healing a ruin back to an intact building via `GoToDamagePhase`/`SetHealthScaled`. Repair replaces, it does not heal.
- **Partial damage states.** Only destroyed-versus-not matters; there is no degraded mode and no repair of undestroyed damage.
- **Making non-destructible buildables destructible.** Destructibility is inherited from each buildable's vanilla base prefab and is left as it is.

---

*Consumed by `/plan-epic logistics`. After planning, run `/plan-feature logistics/<feature-name>` per feature in the recommended order.*
