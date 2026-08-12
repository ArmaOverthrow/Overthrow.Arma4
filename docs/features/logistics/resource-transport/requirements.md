# Resource Transport — Requirements

**Epic:** logistics
**Created:** 2026-08-11

## Overview

`resource-transport` is the epic's first playable slice: trucks gain a dedicated, volume-capped resource cargo store separate from their normal inventory, players load and unload mixed quantities through user actions, and unloaded resources land as crate piles that merge with any pile already nearby. It also carries the readouts that make an invisible cargo legible — a HUD while driving, an inspect action on a pile, and markers on the player map.

## Requirements

- **Every truck in the game can haul resources.** A resource cargo store applies to all truck-class vehicles, including vanilla prefabs spawned by any system — not a bespoke Overthrow supply truck. ⚠️ **This is the feature's central architectural decision** and must be resolved during planning; candidates include same-GUID prefab delta overrides per truck prefab, runtime component attachment via `OVT_VehicleManagerComponent`, or a scripted prefab→capacity table with no per-prefab component. Their persistence and replication costs differ substantially.
- The cargo store is **separate from the vehicle's item inventory** — loading resources must not consume or interact with the trunk's normal item slots (mirroring how Conflict supplies are separate from cargo).
- Each truck has a **volume capacity in m³**, configurable per truck class/prefab so a small pickup and a large flatbed differ meaningfully.
- **Mixed loads.** A truck may hold several resource types at once, constrained only by combined volume. Loading is rejected (with a clear reason shown to the player) when it would exceed the cap.
- **Partial, chosen-quantity load and unload.** The player picks which resource and how much, not "transfer everything of one type". Reuse Conflict's continuous user-action pattern (`SCR_ResourceContainerVehicleLoadAction`) as the UX shape.
- **Unloading spawns a crate pile.** A crate-stack entity appears at the unload point holding the unloaded resources. Piles have **no capacity limit**.
- **Piles merge.** Before spawning, unloading searches for an existing pile within a configurable radius and adds to it instead of creating a second one. The search radius and the merge rule must be deterministic and server-side.
- **Pile visuals** may be a single generic stack prefab, or per-resource prefabs with a generic fallback — decide during planning based on available assets; content variety is not a requirement.
- **Inspect action** on a pile lists its contents (resource, quantity, and total m³) to the player.
- **Cargo HUD** while the player is in a truck with a resource load: current contents and used/total m³. Weight may be displayed but must not affect handling.
- **Map markers** for crate piles, using the existing `map` epic marker plumbing, so a dropped truckload is findable.
- Truck cargo contents and crate piles **persist** across save/load, including piles created and merged in the same session. A truck's load must survive the vehicle's despawn/respawn cycle (see the per-instance vehicle round-trip precedent in the Persistence tier).
- Server-authoritative: load/unload requests originate on the client and are validated and applied on the server; results replicate to all clients including join-in-progress.
- New user-action and HUD strings go in `Language/localization_Overthrow.st`.
- Automated coverage where the test world allows: Logic-tier assertions for capacity rejection and merge-radius selection; Persistence-tier assertions for a truck load and a crate pile round-tripping.

## Dependencies

- **`logistics/resource-core`** — must be complete. Supplies the resource definitions, the ledger type and the volume maths.
- `core/persistence` — `ScriptedComponentSerializer` / `ScriptedEntitySerializer` for the truck store and the pile entity, plus the `OVT_PersistenceTracking.Track()` requirement for spawned entities; the per-instance vehicle despawn/respawn round trip is the pattern to follow.
- `map` epic — marker registration and the map layer this pile marker belongs on.
- Vanilla reference (pattern only): `SCR_ResourceContainerVehicleLoadAction` / `...UnloadAction` and the Conflict supply HUD layouts.

## Out of Scope

- Buying or selling resources — that is `resource-trade`.
- Consuming resources to build anything — that is `resource-construction`.
- Warehouses of any kind — that is `resource-storage`.
- Weight affecting vehicle handling, top speed or acceleration. Volume is the only constraint.
- Piles being lootable or destructible by the occupying faction, and any convoy-ambush gameplay.
- AI or recruits driving, loading or unloading trucks.
- Non-truck vehicles (helicopters, boats, cars) carrying resources.
