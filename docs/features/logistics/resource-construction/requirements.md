# Resource Construction — Requirements

**Epic:** logistics
**Created:** 2026-08-11

## Overview

`resource-construction` is the sink that gives the sub-game its purpose: mid-game buildings stop appearing the instant you pay for them, and instead go up as construction sites that must be supplied by truck. Buildables gain an optional resource requirement list; placing one spawns a site with a requirements action, and a Build action unlocks only once the needed resources are sitting in crate piles nearby.

## Requirements

- **`OVT_Buildable` gains an optional resource requirement list** (resource id → quantity) in `Configs/Resistance/buildables.conf`. A buildable with an empty list behaves **exactly as it does today** — money cost only, instant placement. This is additive; the existing `m_iCost` money path is unchanged.
- **`OVT_Buildable` gains an optional construction-site prefab.** When a resource-requiring buildable is placed, that prefab is spawned instead of the finished building. When none is defined, a generic "under construction" marker/sign prefab is used as the fallback.
- The existing build flow (`OVT_BuildContext` → the resistance faction manager's build path) branches on whether the chosen buildable has requirements — it must not fork into a parallel placement system.
- **Requirements action** on a construction site shows, per required resource: how much is needed, and how much is currently **available nearby** (summed across all crate piles within a configurable radius of the site). The player must be able to tell at a glance what is still missing.
- **Build action** on the site is available only when every requirement is satisfied by nearby piles. When unavailable, the action's name or reason states why (which resource is short) rather than simply being hidden.
- **Performing Build** removes the required resources from the nearby piles (server-side, in a deterministic order), destroys the construction site, and spawns the finished building through the same path the instant-build flow uses today — so ownership, registration, XP reward (`m_iRewardXP`) and the existing build events all fire exactly as before.
- Piles emptied by a build are cleaned up; piles with leftover resources remain.
- **Construction sites persist**: a half-finished site survives save/load with its buildable identity and placement intact, and can be completed in a later session.
- **Money is still charged.** Decide during planning whether the money cost is taken at placement (site spawn) or at completion, and be explicit about what happens to it if a site is abandoned or removed. Whichever is chosen must be consistent and must not allow free buildings via place-then-remove.
- Removal: a construction site must be removable through the existing removal flow (`OVT_BuildableComponent` / removal mode) without leaving orphaned state.
- Server-authoritative: the Build action validates requirements and consumes resources on the server; client-side availability is a display of server state, never the authority.
- **The resource requirement is difficulty-scaled**, exactly as the money cost already is. Follow the shipped pattern:
  - Add a field to `OVT_DifficultySettings` (`Scripts/Game/Configuration/OVT_DifficultySettings.c`) in the `Economy` category, alongside `placeableCostMultiplier` / `buildableCostMultiplier` / `realEstateCostMultiplier` — e.g. `buildableResourceCostMultiplier`, `[Attribute(defvalue: "1", ...)]` so Normal is unscaled.
  - Add an accessor to `OVT_OverthrowConfigComponent` shaped exactly like `GetBuildableCost()` (`Math.Round(m_Difficulty.buildableCostMultiplier * buildable.m_iCost)`) that returns the scaled requirement, so no caller touches `m_Difficulty` directly. It returns a **per-resource quantity**, so decide and document the rounding rule — rounding must never turn a non-zero requirement into zero.
  - Difficulty `.conf` files only override what they change: `Difficulty_Normal.conf` and `Difficulty_TestWorld.conf` list no multipliers at all and inherit the attribute default, while `Difficulty_Easy.conf` sets 0.8 for the existing three. Add lines only to the tiers that want a non-default value.
  - The requirement **displayed** in the requirements action, and the amount **consumed** by the Build action, must both be the scaled figure — never the raw config number. A mismatch between what the UI asks for and what the server consumes is the obvious bug to guard against here.
  - `logistics/building-repair` adds a repair multiplier on the same pattern that stacks on top of this one.
- MVP requirement lists are applied to the **existing** mid-game buildables (Garage is the obvious first; Guard Tower, Helipad and the tents are candidates) — balanced against their current money costs. Do not add new buildings here.
- New strings go in `Language/localization_Overthrow.st`.
- Automated coverage: Logic-tier assertions for the requirements-satisfied predicate (exact match, short by one, surplus, multiple piles summing) and for the consumption order; Persistence-tier assertion for a construction site round-tripping.

## Dependencies

- **`logistics/resource-core`** — resource definitions and the ledger, for requirement lists and pile contents.
- **`logistics/resource-transport`** — crate piles are what the nearby-availability check reads and the Build action consumes.
- **`logistics/resource-trade`** — play-test dependency only: it is the MVP's sole resource source, so construction cannot be exercised end to end without it. No code dependency; the two can be implemented in parallel.
- `resistance` epic — the existing buildables/build path (`OVT_BuildContext`, `OVT_ResistanceFactionManager`, `OVT_BuildableComponent`).
- `core/persistence` — a `ScriptedComponentSerializer` for construction sites; `OVT_BuildableComponentSerializer` is the precedent, including its `version`-first payload and its `SelfSpawn` + `OVT_PersistenceTracking.Track()` requirement for spawned entities.

## Out of Scope

- Build **time** — no timers, no progress bars, no gradual construction. Requirements satisfied → Build action → building exists.
- Multi-stage construction (foundation → frame → finished) or partial resource delivery being "banked" into the site. Resources stay in the piles until the single Build action consumes them.
- Recruits or AI hauling to, or building at, a site.
- Consuming resources from a truck's cargo store directly. Resources must be unloaded into a pile first — the truck is transport, the pile is the site's supply.
- Consuming resources from a warehouse — `resource-storage` may add that later; it is not a requirement here.
- New buildings (barracks, high-command structures). This feature delivers the machinery and applies it to existing buildables.
- Upkeep, or demolition returning resources.
- Destruction and repair of built structures — that is `logistics/building-repair`, which re-enters this feature's site/requirements/Build
  flow rather than adding its own. **Plan accordingly:** that flow must be callable from a second entry point, not hard-wired to placement.
