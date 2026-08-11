# Resource Storage — Requirements

**Epic:** logistics
**Created:** 2026-08-11

## Overview

`resource-storage` closes the mid-game loop by giving the resistance a proper base-side depot. Warehouses — today purchasable real estate holding item prefabs — gain a resource inventory alongside their existing item inventory, and become **buildable** at bases and in resistance-controlled towns rather than only being bought. A player can then haul a load home, bank it, and draw on it later.

## Requirements

- **Warehouses hold resources as well as items.** The existing `OVT_WarehouseData` item-prefab inventory keeps working unchanged; a resource ledger is added alongside it. Both are visible and usable from the warehouse menu.
- The existing warehouse menu (`OVT_WarehouseContext`, with its Take 1/10/100/All into-your-vehicle flow) is extended to cover resources — and must also support **depositing** resources from a truck, since resources arrive by truck rather than being bought into the warehouse. Preserve the one-handler-per-button rule the context documents (BUG-081).
- **Warehouse becomes a buildable**, placeable at bases and in resistance-controlled towns, with the location predicates set accordingly in `buildables.conf` (`m_bBuildAtBase`, `m_bBuildInTown`, and whether villages/FOBs qualify decided during planning).
- **A built warehouse registers as a real warehouse** — it must appear in `OVT_RealEstateManagerComponent`'s warehouse list, on the map as a warehouse location, and behave identically to a purchased one for every existing consumer of warehouse state. A second, parallel "built warehouse" concept is explicitly not acceptable.
- Ownership of a built warehouse must be well-defined (who owns it, and how that interacts with the real-estate ownership model for a building nobody bought). Settle this during planning — it is the feature's main integration risk.
- **The warehouse buildable costs resources**, using `logistics/resource-construction`'s requirement mechanism — the depot itself is something you must haul for.
- Warehouse resource contents **persist** alongside the existing warehouse item inventory, and survive save/load with exact quantities.
- Loading a truck from a warehouse is capacity-checked against the truck's m³ exactly as loading from a crate pile is.
- Server-authoritative: deposits and withdrawals are validated and applied on the server, with client requests going through an `OVT_OverthrowController` component (never `OVT_PlayerCommsComponent`), and results replicating to all clients including join-in-progress.
- New strings go in `Language/localization_Overthrow.st`.
- Automated coverage: Persistence-tier assertions for warehouse resource stock round-tripping; Logic-tier assertions for deposit/withdraw capacity rules; Init- or Campaign-tier assertion that a built warehouse is registered like a purchased one.

## Dependencies

- **`logistics/resource-core`** — the ledger held by the warehouse.
- **`logistics/resource-transport`** — the truck cargo store that deposits into and withdraws from a warehouse.
- **`logistics/resource-construction`** — the resource-requirement and construction-site machinery the warehouse buildable uses.
- `economy/real-estate` — `OVT_RealEstateManagerComponent`, `OVT_WarehouseData`, `OVT_RealEstateConfig.m_IsWarehouse`, `OVT_WarehouseContext` and the warehouse map location. This is the most integration-heavy dependency in the epic.
- `towns` epic — town control state, to decide where a warehouse may be built.
- Last feature in the epic; nothing depends on it.

## Out of Scope

- Construction sites consuming resources directly out of a warehouse. Building still reads from nearby crate piles — a warehouse near a site may be added later, but is not required here.
- Warehouse capacity limits for resources. Like crate piles, warehouse resource storage is unlimited in the MVP.
- Automatic transfer, resource sharing or logistics routing between warehouses.
- Selling resources from a warehouse. The port (with a truck) is still the only conversion point to money.
- Changing how purchased/real-estate warehouses are acquired, priced or rented.
- New storage building types beyond the warehouse (depots, silos, fuel tanks).
- Recruits or AI moving resources into or out of storage.
