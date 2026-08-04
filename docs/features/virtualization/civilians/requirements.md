# Civilians — Requirements

**Epic:** virtualization
**Created:** 2026-08-04

## Overview

Migrate ambient town civilians off the towns system's ad-hoc spawner and onto the virtualization layer — the layer's **first consumer**, and the proving ground for its **ambient spawn-source** class: declarative, config-driven, one-off spawning with **no persistence and no per-member tracking**. Ambient civilians are scenery that re-rolls on each approach, not records — the opposite end of the spectrum from the tracked combat groups core was designed around, which is exactly why doing this second hardens the layer's extensibility seam before the combat migrations start. On top of the migration, the feature raises the liveliness ceiling (appearance variety, behavior archetypes, believable placement) and gives players and server operators a runtime knob for civilian density for the first time. Stretch goal: ambient parked civilian vehicles that make towns look inhabited.

## Current State (what's being replaced)

All ambient civilian spawning lives in `OVT_TownControllerComponent` (`Scripts/Game/Controllers/OVT_TownController.c`): a 10 s `CallLater` loop per town spawns `population × m_fCivilianSpawnRate` (default 0.1) single-character `Group_CIV.et` groups when any player is within `m_iCivilianSpawnDistance` (default 1000 m) of the town centre, each ping-ponging forever between two random road points. Known defects the migration must not inherit:

- **All-or-nothing, single-frame spawn/despawn** — no cap, no frame-spreading (St. Phillipe: 28 groups + 140 waypoint entities in one frame), no hysteresis.
- **Waypoint entity leak** — the per-civilian waypoints are free-standing world entities, never deleted on despawn.
- **Global QRF suppression** — a QRF anywhere on the map despawns civilians in *every* town (`!m_CurrentQRF` is one global field).
- **Stale tracking** — dead civilians are never pruned from `m_aCivilians`; recruited civilians stay in it and get deleted by the next despawn pass.
- **Wait-time bug** — `SpawnWaitWaypoint` discards its time argument, so the randomised 15–50 s pauses never apply.
- **Build-time-only tuning** — `m_fCivilianSpawnRate` / `m_iCivilianSpawnDistance` are prefab attributes; `Overthrow_Config.json` and `OVT_DifficultySettings` have zero civilian fields.
- **Zero variety** — one hardcoded group prefab (`m_pCivilianPrefab`), clothes randomised via `Configs/Civilians/CivilianClothes.conf`; spawn points are road snaps, never houses or POIs.

Civilians are deliberately unpersisted today (`docs/features/towns/core/implementation.md`) — the migration keeps that property by design.

## Requirements

- **First ambient consumer:** town civilians spawn through core's ambient spawn-source class — one-off, non-persisted, untracked (no per-member records, no serializer coverage); despawn discards, respawn re-rolls. The civilian code in `OVT_TownControllerComponent` (`ActivateTown` loop, `CheckSpawnCivilian`, `SpawnCivilians`, `SpawnCivilian`, `DespawnCivilians`) is retired.
- **Declarative and modder-extendable:** civilian ambience is defined in config (`.conf`) — density inputs, prefab pools, behavior archetypes, placement rules — so modders can add or override ambient spawn types without script changes, consistent with Overthrow's config-driven systems (deployments, jobs, modifiers).
- **Runtime operator tuning:** a civilian density multiplier and a hard per-town cap are exposed through `OVT_OverthrowConfigStruct` (`$profile:Overthrow_Config.json`) — the file server operators actually edit — not only build-time prefab attributes. Spawn distance rides core's server-configurable spawn distance.
- **Documented density model with clamps:** per-town count derives from town population with explicit min/max clamps and the operator multiplier; the formula gets Logic-tier (world-free) test coverage.
- **More life in towns:**
  - Appearance variety via a configurable civilian prefab pool (seeded from the CIV entity catalog rather than one hardcoded prefab), keeping `CivilianClothes.conf` randomisation.
  - At least two behavior archetypes beyond today's two-point ping-pong (e.g. wanderer and loiterer-at-POI), declared per spawn source in config.
  - Placement that prefers believable spots (near buildings/doorways/POIs, falling back to road points), never water.
  - The wait-time bug is fixed so civilians actually pause instead of pacing continuously.
- **Lifecycle correctness by construction:** spawn/despawn is frame-spread through core (no single-frame hitches), top-up is incremental rather than all-or-nothing, waypoint entities are cleaned up with their group, dead civilians are pruned, and QRF suppression becomes town-local (only the town under QRF loses its civilians).
- **Player interactions preserved:** recruit, convert-supporter and sell-drugs actions, the wanted-system disable on spawn, and the civilian-death stability/support modifiers all keep working. A **recruited civilian is handed off out of ambient ownership** — the layer must support ownership transfer so despawn never deletes a recruit.
- **Stretch — ambient parked vehicles:** a second ambient spawn-source config places civilian vehicles in towns, parked realistically (using `OVT_ParkingComponent` spots and the currently-dead `m_CivilianVehicleEntityCatalog`), scaled by town size and despawned with the same proximity rules. A vehicle a player takes leaves ambient management and survives (the epic's stolen-vehicle rule applied to ambience).
- **Testing:** Logic-tier for the density maths; Init-tier for config resolution (ambient sources registered, prefab pools/catalogs resolve); documented play-test steps for spawn feel, interaction regression (recruit/convert/sell), and the QRF-locality change.

## Dependencies

- `virtualization/core` complete — specifically its **ambient spawn-source seam** (added to core's requirements alongside this feature; core must land it or civilians cannot start).
- Towns system: `OVT_TownControllerComponent` / `OVT_TownManagerComponent` (code being replaced; town population, range and location data remain the density inputs — `OVT_TownData` stays the persisted numeric truth).
- `Configs/Civilians/CivilianClothes.conf` and the CIV entity catalogs (`Configs/EntityCatalog/CIV/*`) for variety.
- Stretch: `OVT_ParkingComponent` and `Vehicles_EntityCatalog_Factionless.conf`.
- Does **not** depend on `virtualization/movement` — ambient civilians have no despawned life to advance.

## Out of Scope

- **Persisting or tracking individual civilians** — ambience re-rolls each visit by design; only town population (already persisted) survives a save.
- **Virtual movement while despawned** — nothing to move; the ambient class has no despawned state.
- **Civilian vehicle traffic** (civilians driving) — parked ambience only; AI driving is its own problem (issue #71).
- **Economy 2.0 virtual agents** (virtual purchases/deliveries) — this feature is ambience, not simulation; the agent systems stay in the Economy 2.0 epic.
- **Day/night schedules or deep routine simulation** — archetypes are simple behavior variants, not a life sim.
- **Client-visible surface** beyond the spawned entities themselves (no map markers/UI).
