# Map - Epic Requirements

**Created:** 2026-08-10
**Phase:** Recovering and finishing the `new-map` branch rewrite

> Epic-level requirements — the higher-level scope for the whole epic, mirroring a feature's `requirements.md` but one level up. `/plan-epic map` reads this file (if it exists) to drive epic scoping; otherwise it uses the prompt. Each **child feature** below gets its own `requirements.md` (in the standard Overview / Requirements / Dependencies / Out of Scope shape) that `/plan-feature map/[feature-name]` consumes.

## Overview

This epic delivers Overthrow's interactive fullscreen map: config-driven location markers for every campaign point of interest, per-type info panels, and the two map-driven travel verbs (fast travel and bus travel). It owns the map UI layer end to end — the vanilla-derived widget system, the location-type extension point, and the travel service — and finishes the rewrite that has been unmerged on the `new-map` branch since 2025-08-02. Its completion condition is **full parity followed by deletion of the legacy map**: `OVT_MapIcons` and `OVT_MapContext`'s three modes come out only once everything they did exists in the new system.

## Requirements

- The map must present every campaign location the legacy system drew — towns, military bases, radio towers, FOBs, ports, gun dealers, shops, houses, warehouses, personal camps **and owned vehicles** — as interactive markers driven from `Configs/Map/OverthrowMap.conf`.
- Adding or reconfiguring a location type must not require changes to the core map code: a new type is a `OVT_MapLocationType` subclass plus a config entry, plus a layout only where a bespoke panel is wanted.
- The map must remain a **read-only projection of replicated campaign state**. It reads the manager singletons through `OVT_Global`; it must not introduce a second source of truth for town, base, shop, house or vehicle data.
- Selecting a location must show information appropriate to its type, and offer the travel verb only where travel is actually permitted — the displayed availability and cost must match what the server enforces.
- Fast travel and bus travel must both be driven from the map, with one shared rule and cost model, and must execute **server-side through the controller** (`OVT_Global.GetController()`) — **not** via the deprecated `OVT_PlayerCommsComponent`.
- Bus travel must survive the migration: the world action `OVT_CatchBusAction` must still let a player at a bus stop pick a destination bus stop on the map and be charged by distance.
- **Bus stops must become first-class Overthrow locations, not vanilla map descriptors.** Today they are discovered by proximity-querying vanilla `EMapDescriptorType.MDT_BUSSTOP` descriptors (`OVT_TownManagerComponent.GetNearestBusStop`, `:881`), which ties them to whatever the world author happened to place. They must migrate to an Overthrow marker component — the same pattern as ports (`OVT_PortControllerComponent`, an empty `OVT_Component` subclass found via `FindComponent`) — so a bus stop can be **attached to any entity**, registered in a manager, and rendered and selected like every other location type.
- The whole map — marker selection, info panels and both travel verbs — must be fully operable **on gamepad/console**, which is the reason the system is built on vanilla's `SCR_MapUIElement` widgets rather than a hand-rolled layer.
- Every feature must be verified in **multiplayer, including join-in-progress**, before it is considered complete. There is no separate verification feature; this is an acceptance gate on each one.
- Restriction rings drawn on the map (`OVT_MapRestrictedAreas`) must continue to match the radii the FOB deploy check actually enforces (the defect fixed as BUG-070 must not regress).
- The legacy map (`OVT_MapIcons.c`, the map-info/fast-travel/bus-travel modes in `OVT_MapContext`, and the duplicated main-menu entries) must be removed once, and only once, parity is demonstrated.

## Planned Features

The features that make up this epic, in intended **build order**. `/plan-epic` creates a subfolder + `requirements.md` for each, and records the order in `epic-overview.md`.

1. **core** — Discovery and hardening of the shipped map infrastructure (`OVT_OverthrowMapUI`, `OVT_MapLocationType`/`Element`/`Data`, `OVT_OverthrowMapConfig`, shared layouts, imageset, canvas-layer modules) — comes first because every other feature builds on its contract, and because it is written but undocumented and unverified.
2. **location-types** — Brings the ten shipped types to parity: adds the missing Vehicle type and bus-stop targets, and gives bespoke info panels to the seven types still using the generic fallback — depends on core's virtual-method contract and data payload.
3. **fast-travel** — Consolidates the travel rules, cost model and `OverthrowFastTravel` keybinding into `OVT_FastTravelService`, and migrates bus travel off `OVT_MapContext` — depends on core for selection delegation and on location-types for destination markers.
4. **legacy-retirement** — Deletes `OVT_MapIcons`, strips `OVT_MapContext`'s three modes, removes the duplicated main-menu entries and archives `towns/map-info` — last of the committed scope, gated on features 2 and 3 proving parity.

**Stretch goals** — additive capability built on the finished, legacy-free map. The epic is complete and coherent without them; they are sequenced after feature 4 so they are built once, against the final map, rather than maintained across the retirement.

5. **territory-overlay** — Voronoi territory shading over towns and bases, each cell clipped to an influence radius and border-smoothed, coloured by controlling faction — the highest-value new capability, and it also settles whether the disabled `OVT_MapThreatGrid` is revived or deleted.
6. **map-layers** — A legend plus per-overlay and per-location-type visibility toggles — follows the territory overlay because that is when the map becomes crowded enough to need tuning.
7. **shared-markers** — Networked player and squad markers by wiring up vanilla's existing marker stack — most self-contained; parallel-safe with 5 and 6 apart from registering in the legend.

## Dependencies

- **`towns/core`** — town records, and the current home of bus-stop discovery (`OVT_TownManagerComponent.GetNearestBusStop`, `:881`) that this epic replaces with an Overthrow marker component.
- **`occupying/core`** — base and radio-tower records, and the QRF state that gates fast travel.
- **`resistance/fob`** — FOB records, and the FOB-deploy radii that `OVT_MapRestrictedAreas` draws.
- **`economy/shops`, `economy/real-estate`** — shop, house, warehouse and gun-dealer records and their ownership state.
- **`core/controller-migration`** — the travel verbs must land on `OVT_OverthrowController`, not the deprecated `OVT_PlayerCommsComponent`.
- **`towns/map-info`** — documents the legacy system being replaced; superseded and archived by `map/legacy-retirement`.
- **`core/player-groups`** *(stretch)* — defines team scope for `map/shared-markers`.
- **Vanilla marker stack** *(stretch)* — `SCR_MapMarkerManagerComponent`, `SCR_MapMarkerSyncComponent`, squad leader/member marker components (`ArmaReforger/scripts/Game/Map/Markers/`); `map/shared-markers` is integration work whose value depends on what these provide for free.
- **External / process:** the `new-map` branch must stay merged up to date with `main`; the generated `Language/localization_Overthrow.<lang>.conf` exports must be regenerated in Workbench (the merge added 14 map string ids to the `.st` master only).

## Out of Scope

- **Fog of war, scouting and intel.** What the campaign chooses to *reveal* to a player — including surfacing the occupying faction's own knowledge of the player (`m_aKnownTargets` / `IsKnownTarget`, `OVT_OccupyingFactionManager.c:1361`) — is a **separate future epic**, not this one. This epic's canvas-overlay and layer-toggle machinery is what that epic will build on; neither should be designed assuming the map always shows everything.
- **The map's non-location canvas layers as a rewrite target.** `OVT_MapRestrictedAreas` and `OVT_MapPlayerLocation` stay as `SCR_MapConfig` modules; this epic keeps them working and consistent but does not port them into the location-type system. (`OVT_MapThreatGrid`, shipped disabled, is the one exception — `map/territory-overlay` decides whether it is revived or deleted.)
- **Changing the underlying campaign systems.** No changes to how towns, bases, FOBs, shops, houses or vehicles compute or replicate their state — the map only reads them. Defects found in those systems get filed as bugs against their own features.
- **The task/waypoint and spawn-point map surfaces.** `OVT_OverthrowMapUI` ships with `m_bShowSpawnPoints 0` and `m_bShowTasks 0`; deciding and enabling those is deferred unless parity with the legacy job-waypoint icons demands it.
- **Minimap / in-world map gadget UX** beyond what the fullscreen map requires.
- **New travel mechanics.** Fast travel and bus travel reach parity with what shipped; no new travel modes, vehicles-as-fast-travel-anchors, or route planning.
- **Freehand map drawing** (lines, shapes, annotations) — `map/shared-markers` covers point markers only.
- **Coastline-accurate territory borders** and weighted influence — `map/territory-overlay` clips cells to a uniform influence radius; following the shoreline or scaling reach by town strength is deferred.
- **Rewriting `OVT_MapContext` wholesale.** Only its three map modes are stripped in `legacy-retirement`; any remaining non-map responsibility of that context stays where it is.

---

*Consumed by `/plan-epic map`. After planning, run `/plan-feature map/[feature-name]` per feature in the recommended order.*
