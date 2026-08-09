# Map - Epic Overview

**Epic:** map
**Status:** 🟡 In Progress
**Last Updated:** 2026-08-10 00:00

> **This file is the epic marker.** Its presence in `docs/features/map/` is what tells every Beast Mode command (and future Web App / Discord clients) that this folder is an **epic**, not a plain feature. Keep it present and keep the required sections below filled in. It is the epic's equivalent of the project's `docs/overview.md`, scoped to this epic. The master `docs/overview.md` tracks this epic as a **single row**; the per-feature detail lives **here**.

---

## Purpose

This epic owns **everything the player sees and does on the fullscreen map**: the interactive location markers, their info panels, and the map-driven travel verbs (fast travel and bus travel). It exists to land and finish the map rewrite that has been sitting unmerged on the long-dormant `new-map` branch since 2025-08-02 — a config-driven system built on vanilla's own `SCR_MapUIElementContainer`/`SCR_MapUIElement` widgets, replacing the hand-rolled `OVT_MapIcons` parallel-array icon layer and the three flag-based modes inside `OVT_MapContext`.

The branch is **substantially built, not started-from-scratch**: `OVT_OverthrowMapUI` is already wired into `Configs/Map/MapFullscreen.conf` with ten configured location types, and the legacy `OVT_MapIcons` component is already switched off there (`Enabled 0`, `m_bDisableComponent 1`). What remains is to document what was built, close the parity gaps that still keep the legacy code alive, verify the whole thing in multiplayer and on gamepad, and then delete the old system. These features belong together because they share one config (`Configs/Map/OverthrowMap.conf`), one element/info-panel layout pair, and one retirement: none of the legacy map code can be deleted until *all* of its capabilities exist in the new system.

---

## Features

The constituent features of this epic, in build order. This is the epic's equivalent of `docs/overview.md`'s feature-status table, scoped to the epic. Each feature is a subfolder under `docs/features/map/`.

| # | Feature | Status | Tasks | Description |
|---|---------|--------|-------|-------------|
| 1 | core | 📄 Discovered — built, unverified | — | Discovery + hardening of the shipped map infrastructure: `OVT_OverthrowMapUI`, `OVT_MapLocationType`/`Element`/`Data`, `OVT_OverthrowMapConfig`, the shared layouts, imageset and canvas-layer modules. **BUG-069's four lifecycle defects are avoided structurally.** **5 bugs filed: BUG-133 … BUG-137** (from findings D1–D7), all awaiting runtime confirmation |
| 2 | location-types | 📄 Discovered — partial | — | Ten types shipped; **4 parity gaps** vs the legacy layer: Vehicle (G1), job waypoints (G2), the POI registry (G3), bus stops as a marker component (G4). 7 of 10 types have no info panel |
| 3 | fast-travel | 📄 Discovered — partial | — | Rule set and cost model consolidated and healthy; **execution path is not**: on-foot travel teleports and debits money client-side (F1/F2), routes through the deprecated comms component (F3), recruits regressed (F4), bus travel not migrated (F5) |
| 4 | legacy-retirement | Planned | — | Delete `OVT_MapIcons`, strip the map-info/fast-travel/bus-travel modes from `OVT_MapContext`, remove the duplicated main-menu entries, and archive `towns/map-info` |
| 5 | territory-overlay | Planned (stretch) | — | Voronoi territory shading over towns + bases, clipped to an influence radius and border-smoothed; also settles the disabled `OVT_MapThreatGrid` |
| 6 | map-layers | Planned (stretch) | — | Legend plus per-overlay and per-location-type visibility toggles, config-driven and gamepad-operable |
| 7 | shared-markers | Planned (stretch) | — | Networked player/squad map markers by wiring up vanilla's existing marker stack |

> **Features 5–7 are stretch goals**, sequenced after `legacy-retirement`. Features 1–4 are the epic's committed scope: they land the rewrite and delete the legacy map. The stretch features add new capability on top of a finished, legacy-free map — none of them is a prerequisite for shipping.

> Reference any feature with the slash form `map/[feature-name]` (e.g. `/continue-feature map/core`). Task counts are pulled from each feature's own `tasks.md` and refreshed by `/update-epic`.

---

## Build Order / Dependencies

Which features come first, and why. This feeds planning and the next-step suggestions from `/continue-feature map`.

1. **core** — Foundational and mostly *discovery*. Every other feature extends `OVT_MapLocationType` or consumes `OVT_MapLocationData`/`OVT_MapLocationElement`, so the base contract (virtual method set, visibility/zoom rules, selection and info-panel lifecycle, config binding) must be documented and known-correct before anything is added on top. It is also where the element lifecycle is verified in MP/JIP — a defect here reappears in all ten types.
2. **location-types** — Depends on core's contract. Closes the *content* half of parity: adds the one missing type (Vehicle), migrates bus stops from vanilla `MDT_BUSSTOP` descriptors onto an Overthrow marker component so they can be attached to any entity and selected like any other location, and gives the seven types currently rendering through the generic `OVT_MapInfoPanel.layout` their own panels. Must land before retirement because `OVT_MapIcons` drew vehicles and the legacy bus flow needs a destination marker. The bus-stop migration is the riskiest item here — it changes how stops are authored and discovered, so it should be sequenced first within the feature.
3. **fast-travel** — Depends on core (selection/click delegation) and on location-types (a target must exist to travel to, including bus stops). Closes the *verb* half of parity: consolidates the fast-travel rule set and cost model into `OVT_FastTravelService` and moves bus travel out of `OVT_MapContext` so the map is the single travel surface.
4. **legacy-retirement** — Last of the committed scope. Deleting `OVT_MapIcons` and stripping `OVT_MapContext`'s three modes is only safe once features 2 and 3 have proven parity; doing it earlier removes shipped player capability. Also archives the superseded `towns/map-info` feature doc.
5. **territory-overlay** *(stretch)* — First stretch goal because it is the highest-value new capability and reuses machinery feature 1 already documents (`OVT_MapCanvasLayer` + `PolygonDrawCommand`). Deliberately after retirement so the overlay is built once, against the final map, rather than maintained across the deletion.
6. **map-layers** *(stretch)* — Follows territory because that is the point at which the map becomes genuinely crowded: eleven-plus marker types plus territory, restriction rings and possibly the threat grid, all drawn at once. It absorbs whatever temporary on/off the territory overlay shipped with.
7. **shared-markers** *(stretch)* — Last because it is the most self-contained and the most dependent on what vanilla gives for free. Buildable in parallel with 5 and 6; only its legend/toggle registration depends on 6.

**Stretch-goal note:** features 5–7 are additive. If the epic must stop after feature 4, it stops in a complete, coherent state — the rewrite landed and the legacy map is gone.

**Dependencies between features:**
- core → location-types (the `OVT_MapLocationType` virtual contract and `OVT_MapLocationData` payload shape)
- core → fast-travel (element selection/click delegation and the info-panel hook the travel button lives on)
- location-types → fast-travel (fast travel and bus travel both need selectable destination markers, incl. bus stops)
- location-types + fast-travel → legacy-retirement (**hard gate** — nothing legacy is deleted until parity is demonstrated)
- legacy-retirement → territory-overlay, map-layers, shared-markers (stretch goals build on the finished, legacy-free map)
- territory-overlay → map-layers (the overlay is the main thing the panel toggles); map-layers → shared-markers (markers register as a toggleable category)
- **External:** `resistance/fob` (FOB markers), `economy/shops` + `economy/real-estate` (shop/house markers and ownership), `towns/core` (town records, and the bus-stop discovery this epic replaces), `occupying/core` (bases, radio towers, QRF state). This epic *reads* those systems; it must not change their state models — the **one deliberate exception** is bus stops, which migrate from vanilla map descriptors to an Overthrow marker component (see below).
- location-types and fast-travel are **not** parallel-safe as written (fast travel depends on bus-stop markers); everything else is strictly sequential.

---

## Integration & Architecture

How the features fit together as one coherent system, and how this epic integrates with the rest of the project.

- **Within the epic:** One extension point does the work. `OVT_MapLocationType` (a `ScriptAndConfig` subclass) is instantiated *from config*, not from code — `Configs/Map/OverthrowMap.conf` holds an `OVT_OverthrowMapConfig` with an `m_aLocationTypes` array, and `Configs/Map/MapFullscreen.conf` binds that config into `OVT_OverthrowMapUI`. Each type supplies its own icon (imageset + icon name), visibility zoom, optional bespoke info layout, and fast-travel eligibility. Adding a location type is therefore a new `.c` subclass plus a config entry plus (optionally) a layout — no change to the core map code. All types share `UI/Layouts/Map/Core/OVT_MapLocationElement.layout` for the marker and fall back to `OVT_MapInfoPanel.layout` for the panel.
- **With other epics / features:** The map is a **read-only projection of replicated campaign state** — it populates markers from the manager singletons via `OVT_Global` (towns, bases, radio towers, FOBs, shops, houses, vehicles) and must not become a second source of truth. The one exception is the travel verbs, which are actions: those must go server-side through the controller, **not** through the deprecated `OVT_PlayerCommsComponent` (see `core/controller-migration`). Canvas-layer modules (`OVT_MapRestrictedAreas`, `OVT_MapThreatGrid`, `OVT_MapPlayerLocation`) stay as `SCR_MapConfig` modules and are *not* being replaced by the location-type system — `OVT_MapRestrictedAreas` in particular now draws the FOB-deploy restriction rings that `resistance/fob` enforces, and the two must stay in agreement (this was BUG-070).
- **Key architectural decisions for the epic as a whole:**
  - **Build on vanilla, don't reimplement.** The system extends `SCR_MapUIElementContainer`/`SCR_MapUIElement` so vanilla zoom, pan, cursor handling, decluttering and **gamepad/console navigation** come for free. This is the main reason the rewrite exists; any change that bypasses the vanilla widget layer forfeits it.
  - **Config-driven, not code-driven.** Location types are declared in `OverthrowMap.conf`. Note that Overthrow's `Configs/Map/MapFullscreen.conf` is a **same-GUID delta over vanilla's**, not a replacement — edits there merge with vanilla's config rather than overriding it.
  - **No dedicated verification feature.** The user chose a four-feature epic, so **MP/JIP and gamepad/console verification are acceptance criteria inside each feature**, not a separate phase. Nothing in this epic has been play-tested since 2025-08-02, and JIP/multiplayer is this project's most common regression class — every feature plan must carry an explicit MP + gamepad gate. `tools/launch-server.sh` plus two `tools/launch-game.sh --profile` clients is the intended harness.
  - **Bus stops become an Overthrow component, not a vanilla descriptor.** Bus stops are currently found by proximity-querying vanilla `EMapDescriptorType.MDT_BUSSTOP` descriptors (`OVT_TownManagerComponent.c:881`). They migrate to an Overthrow marker component following the **port pattern** — `OVT_PortControllerComponent` is an empty `OVT_Component` subclass that a manager locates with `FindComponent` (`OVT_EconomyManagerComponent.c:1860`) — so a bus stop can be attached to **any** entity rather than depending on world-authored descriptors, and so it becomes a normal location type with an icon, an info panel and a registry like every other. This is the epic's one intentional write into a non-map system.
  - **Two independent rendering paths, and the stretch goals use the second one.** Markers are *widgets* (`SCR_MapUIElement` under `OVT_OverthrowMapUI`); overlays are *canvas draw commands* (`OVT_MapCanvasLayer` under `SCR_MapConfig.m_aModules`). The canvas path exposes `PolygonDrawCommand` with an arbitrary vertex array (`OVT_MapCanvasLayer.c:23,61`) — which is what makes a real Voronoi territory overlay feasible rather than a grid approximation — but `Draw()` runs **every frame** via `Update` (`:12-19`), so any overlay must precompute in world space at map open and only project per frame.
  - **A future intel epic is anticipated and is not this epic.** What the campaign chooses to *reveal* to a player — fog of war, scouting, and surfacing the occupying faction's own knowledge of the player (`m_aKnownTargets` / `IsKnownTarget`, `OVT_OccupyingFactionManager.c:1361`) — is deliberately excluded here. This epic's canvas-overlay and layer-toggle machinery is what that epic will build on, so neither should be designed in a way that assumes the map always shows everything.
  - **Localization.** The merge added 14 map string ids to `Language/localization_Overthrow.st` but the generated `localization_Overthrow.<lang>.conf` exports were taken from `main` verbatim — **the exports must be regenerated in Workbench** before any of these strings render.

---

## Tech Debt / Findings

Cross-feature tech debt and review findings. **Populated and updated by `/review-epic`** (which also writes feature-specific findings into the relevant child `context.md` files). Start empty.

Seeded by discovery on 2026-08-10 (`/discover-feature` over `core`, `location-types`, `fast-travel`). All are code-reading findings — **none has been observed at runtime**.

- [ ] 🔴 **Fast travel is not server-authoritative** — `map/fast-travel` — `ExecuteFastTravel` uses two authority models: the in-vehicle branch asks the server, the on-foot branch calls `SCR_Global.TeleportPlayer` and `TakePlayerMoneyPersistentId` **on the client**. Highest-value MP verification in the epic.
- [ ] 🔴 **Recruit accompaniment regressed** — `map/fast-travel` — legacy called `RequestFastTravelWithRecruits`; the new service never does. Silent player-visible behaviour loss.
- [ ] 💳 **Deprecated comms dependency** — `map/fast-travel` — the one server call goes through `OVT_PlayerCommsComponent`; migrate to `OVT_OverthrowController`.
- [ ] 💳 **Legacy static is a hard dependency** — `map/location-types` + `map/legacy-retirement` — `OVT_MainMenuContextOverrideComponent` calls `OVT_MapIcons.RegisterPOI` from live prefabs (garages, maintenance ramps). Deleting the legacy file breaks that call site, not just the visuals.
- [ ] 💳 **Silent widget-name mismatches** — `map/core` — **BUG-133** `IconLayout` (zoom icon sizing) and **BUG-134** `CloseButton` (panel dismissal) are looked up but absent from the layouts. `FindAnyWidget` returning null is a silent no-op the compiler cannot catch; a layout↔code name audit is warranted across the whole map UI.
- [ ] 💳 **Markers never refresh while the map is open** — **BUG-136** (`map/core`) + `map/location-types` (G1) — `OnLocationDataChanged()` has no callers. Blocks a working Vehicle marker, since vehicles move.
- [ ] 💳 **Three manager-access idioms across ten types** — `map/location-types` (T1) — inherited cache vs own shadowing member vs per-call `OVT_Global` lookup.

---

## Master Overview Rollup

How this epic is represented in the project's master `docs/overview.md` (one row, not its children). Kept in sync by `/update-epic` and `/update-master`.

- **Rollup status:** In Progress (0/7 complete — 3 discovered, 1 planned, 3 stretch). Features 1–3 are built but **unverified since 2025-08-02**; 4 parity gaps and 2 server-authority defects block retirement.
- **One-line summary for master:** Config-driven interactive map built on vanilla's map widgets — location markers, info panels and the fast/bus travel verbs — replacing the legacy `OVT_MapIcons` layer and `OVT_MapContext` modes.

---

*This is a required file — do not delete it; it marks the folder as an epic. Update it with `/update-epic map` after working on the epic's features, and run `/review-epic map` to refresh the Tech Debt / Findings section.*
