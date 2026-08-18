# Fuel — Requirements

**Epic:** economy
**Created:** 2026-08-18

> **Deviation note (built 2026-08-18):** the "flat fee per unit time" below was superseded by a
> **per-litre-delivered** charge (user decision D1 in `implementation.md`) — the difficulty knob is
> `float fuelPricePerLitre`, charged server-side in `OnFuelAddedToVehicleServer`, not in
> `LoopActionUpdate` (which runs client-side and moves no fuel).

## Overview

Wire the base game's fuel systems into Overthrow's economy: refueling at world fuel stations costs
money, and the resistance can build a fuel depot at captured bases that — once physically filled by
a fuel truck — provides free fuel. This is a prerequisite for `resistance/high-command` (whose
vehicle groups auto-refuel at these sources and charge their owner), and a general money sink that
makes fuel logistics a real consideration.

Everything rides the vanilla fuel stack — `SCR_RefuelAtSupportStationAction` (a looped duration
action), `SCR_FuelSupportStationComponent` + `SCR_FuelManagerComponent` (stations have real storage
and a `NO_FUEL_TO_GIVE` state) — we add charging, a buildable, and a fill path; we do not build a
parallel fuel system.

## Requirements

### Paid refuel at fuel stations

- The existing refuel action at **world fuel stations** charges the performing player a **flat fee
  per unit time**, difficulty-scaled (new `OVT_DifficultySettings` field, charged server-side on
  the action's loop tick — `LoopActionUpdate` is the natural seam).
- The action refuses/stops when the player cannot pay (partial refuels are fine — you get what you
  paid for).
- Charging is **server-authoritative**: the money mutation happens on the server through the
  economy manager; the client only sees cost feedback. Show the rate in the action UI where
  feasible.
- **What charges and what doesn't:** static world fuel stations charge. Vehicle-to-vehicle fuel
  transfer (e.g. from a player's own fuel truck) stays free — that fuel was already paid for at
  the pump. Overthrow-flagged free sources (the depot, below) don't charge.

### Fuel depot (new buildable)

- Buildable at **captured bases only** (`m_bBuildAtBase` only in `buildables.conf`).
- **Starts empty.** The depot prefab carries vanilla fuel storage
  (`SCR_FuelManagerComponent` + `SCR_FuelSupportStationComponent`), so once filled it dispenses
  through the **same base-game refuel systems** players already use — and reports
  `NO_FUEL_TO_GIVE` while empty, all for free from the vanilla stack.
- **Filling:** from a fuel truck — a fill action on the depot transfers fuel truck→depot using the
  vanilla fuel-flow machinery. The truck itself is filled at any base-game fuel station **for
  money** (the paid-refuel rule above covers its cargo tanks), so depot fuel is pre-paid, hauled
  fuel.
- Once fueled, the depot offers **free fuel** to players in vehicles.
- **Persistence:** the depot's fuel level persists via the vanilla
  `SCR_FuelManagerComponentSerializer` (entries already exist in `Overthrow.conf`); the buildable
  itself persists like every other buildable.

### High-command integration

- `resistance/high-command` vehicle groups auto-refuel in range of **any** of these sources:
  at fuel stations the **owning player** is charged with the same per-time math; at a fueled depot
  it is free (and drains the depot's storage like any other consumer).
- This feature only has to make the sources uniform (a "fuel source" is discoverable and answers
  cost-or-free); the auto-refuel tick itself is high-command's work.

## Difficulty / config knobs

- Fuel fee per unit time (difficulty presets, like `baseRecruitCost` / `vehiclePriceMultiplier`).
- Depot storage capacity (prefab/config).

## Dependencies

- Vanilla support-station stack (1.8): `SCR_RefuelAtSupportStationAction`,
  `SCR_FuelSupportStationComponent`, `SCR_FuelManagerComponent`, and the existing fuel persistence
  serializer entries in `Overthrow.conf`.
- Building system (`resistance/building`): buildable definition + `OVT_BuildableComponent` typing
  (the `"Helipad"` type-string precedent).
- Economy manager for the server-side charge path; client→server seams follow the per-player
  controller-component pattern.
- Consumed by `resistance/high-command` (refuel) — this feature must land first.

## Out of Scope

- AI-driven fuel logistics (HC fuel-truck missions, automated depot resupply).
- Per-town fuel pricing or fuel station stock simulation (stations charge a flat rate and don't
  run dry).
- Fuel depots at FOBs, camps, towns, or villages (captured bases only).
- Selling fuel back, or fuel theft mechanics.

## Testing expectations

- Logic-tier coverage for the fee math (rate × time, difficulty scaling, can't-pay cutoff).
- Persistence-tier round trip for a partially filled depot.
- Play-test gated: pump charging feel, truck→depot fill flow, depot dispensing, and an HC vehicle
  group refueling at both source types (with high-command, once built).
