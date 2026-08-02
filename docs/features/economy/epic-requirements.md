# Economy - Epic Requirements

**Created:** 2026-08-02
**Phase:** Retrospective documentation (systems already shipped)

> Epic-level requirements — the higher-level scope for the whole epic, mirroring a feature's `requirements.md` but one level up. `/plan-epic economy` reads this file (if it exists) to drive epic scoping; otherwise it uses the prompt. Each **child feature** below gets its own `requirements.md` (in the standard Overview / Requirements / Dependencies / Out of Scope shape) that `/plan-feature economy/<feature>` consumes.

## Overview

The economy epic covers Overthrow's money and goods systems: the central market (player money, item/vehicle prices, shop stock and resupply), physical shops where players buy and sell, and real-estate ownership of homes and buildings. These are existing, shipped systems — this epic was created to backfill documentation via `/discover-feature` so future enhancements can use the standard dev-docs workflow.

## Requirements

- Players earn, hold and spend a single persistent currency, with server-authoritative balances replicated to clients.
- Items and vehicles have prices derived from configuration (`OVT_PricesConfig`, `OVT_VehiclePricesConfig`) and market conditions.
- Towns host shops with stock that players can buy from and sell to; stock resupplies over time.
- Players can purchase and own buildings (homes/real estate) with ownership persisted across sessions.
- All economy state survives save/reload via the persistence system (`core/persistence`).
- Economy state feeds town stability/support (e.g. strong-economy and black-market modifiers).

## Planned Features

The features that make up this epic, in intended **build order** (retrospective — all three already exist).

1. **market** — Central economy manager: money, prices, stock, resupply, resistance funds — Foundational; both siblings consume its APIs.
2. **shops** — Shop entities, buy/sell flow and ShopMenu UI — Depends on market's price/stock/money APIs.
3. **real-estate** — Building ownership, homes and rentals — Depends on market's money/price APIs; independent of shops.

## Dependencies

- `core/game-mode` — managers live on `OVT_OverthrowGameMode` and are accessed via `OVT_Global`.
- `core/persistence` — `OVT_EconomyManagerSerializer` / `OVT_RealEstateManagerSerializer` (vanilla serializers; the old EPF SaveData classes were deleted in the migration). Player money persists via `OVT_PlayerManagerSerializer`; shop stock is not persisted at all.
- `core/config` — price and shop configuration loading.
- Towns system (undocumented) — shops and economy are town-scoped.

## Out of Scope

- Town stability/support simulation itself (modifiers that *read* economy state are integration points, not epic members).
- The jobs system (it targets shops but is its own system).
- Loot/inventory mechanics unrelated to buying and selling.

---

*Consumed by `/plan-epic economy`. After planning, run `/plan-feature economy/<feature>` per feature in the recommended order.*
