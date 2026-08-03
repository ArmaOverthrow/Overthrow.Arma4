# Economy - Epic Overview

**Epic:** economy
**Status:** 🟡 In Progress (shop-ux built, pending play-test)
**Last Updated:** 2026-08-04 08:10

> **This file is the epic marker.** Its presence in `docs/features/economy/` is what tells every Beast Mode command (and future Web App / Discord clients) that this folder is an **epic**, not a plain feature. Keep it present and keep the required sections below filled in. It is the epic's equivalent of the project's `docs/overview.md`, scoped to this epic. The master `docs/overview.md` tracks this epic as a **single row**; the per-feature detail lives **here**.

---

## Purpose

The economy epic owns every system that moves money and goods in Overthrow: the central market (player money, prices, stock and resupply driven by `OVT_EconomyManagerComponent`), the physical shops players buy and sell at, and the real-estate system for owning homes and buildings. These features belong together because they share one currency, one price model and one manager-driven server-authority pattern — shops and real estate are both consumers of the market's money and price APIs. This epic was created by backfilling documentation for existing, shipped systems via `/discover-feature`.

---

## Features

The constituent features of this epic, in build order. This is the epic's equivalent of `docs/overview.md`'s feature-status table, scoped to the epic. Each feature is a subfolder under `docs/features/economy/`.

| # | Feature | Status | Tasks | Description |
|---|---------|--------|-------|-------------|
| 1 | market | 📄 Documented (Retrospective) | — | Central economy: player money, prices, stock, resupply and resistance funds via `OVT_EconomyManagerComponent` |
| 2 | shops | 📄 Documented (Retrospective) | — | Physical shop entities, buy/sell UI flow and shop inventory (`OVT_ShopComponent`, ShopMenu) |
| 3 | real-estate | 📄 Documented (Retrospective) | — | Building ownership, homes and rentals via `OVT_RealEstateManagerComponent` |
| 4 | shop-ux | 🔍 Ready for Review | 37/37 (100%) | Shop menu rework (issue #145): category tabs, split Buy/Sell modes with player-inventory sell browser, vehicle trunk “Sell All” — built 2026-08-04 (server-authoritative sell on `OVT_ShopTransactionComponent`, legacy `RpcAsk_Sell` deleted, +6 automated cases); human play-test outstanding |

> Reference any feature with the slash form `economy/<feature>` (e.g. `/continue-feature economy/market`). Task counts are pulled from each feature's own `tasks.md` and refreshed by `/update-epic`.

---

## Build Order / Dependencies

Which features come first, and why. (Retrospective — this is the dependency order the existing code exhibits, and the order any rework should follow.)

1. **market** — Foundational. Owns money, prices and stock; both siblings call into `OVT_EconomyManagerComponent` for every transaction.
2. **shops** — Depends on market for price lookup, stock mutation and player-money changes; adds the physical entities and UI on top.
3. **real-estate** — Depends on market for money/prices; independent of shops (could be reworked in parallel with it).

**Dependencies between features:**
- market → shops (price/stock/money APIs)
- market → real-estate (money/price APIs)
- shops and real-estate are independent of each other.

---

## Integration & Architecture

How the features fit together as one coherent system, and how this epic integrates with the rest of the project.

- **Within the epic:** `OVT_EconomyManagerComponent` is the single money/price authority; shops and real estate are entity-level consumers of its APIs. All client→server mutation funnels through `OVT_PlayerCommsComponent` (`OVT_Global.GetServer()`); no `RplProp` anywhere — JIP snapshots (`RplSave`/`RplLoad`) plus explicit reliable broadcast RPCs. The market's int resource IDs (array indices into `m_aResources`, built independently per machine) are the wire format for all shop stock — a determinism assumption nothing validates.
- **With other epics / features:** Town stability/support modifiers subscribe to `m_OnPlayerTransaction` (`OVT_StrongEconomyStabilityModifier`, `OVT_BlackMarketStabilityModifier`, `OVT_BlackMarketSupportModifier`); the skills system takes XP from `m_OnPlayerBuy`/`m_OnPlayerSell`; the jobs system targets shops (`OVT_IsNearestTownWithShopJobCondition`, `OVT_GetShopLocationJobStage`); persistence goes through `core/persistence` vanilla serializers (`OVT_EconomyManagerSerializer` — treasury+tax only, `OVT_RealEstateManagerSerializer`; player money rides `OVT_PlayerManagerSerializer`; shop stock is deliberately NOT persisted); Campaign-tier test coverage lives in dev-ops (`OVT_TEST_Campaign_Economy`).
- **Key architectural decisions for the epic as a whole:** Server authority over money/stock/ownership mutation (honoured by the shop *buy* path; violated by shop *sell* and all real-estate asks — see per-feature Known Issues); manager singletons on the game mode accessed via `OVT_Global`; the economy clock is game-time-driven and frozen when no players are online.

---

## Tech Debt / Findings

Cross-feature tech debt and review findings. **Populated and updated by `/review-epic`** (which also writes feature-specific findings into the relevant child `context.md` files). Start empty.

Seeded from `/discover-feature` findings (2026-08-02); `/review-epic` will refresh and extend.

- [ ] 💳 **Client-authoritative money paths** (**BUG-020**, **BUG-021**) — shops (sell), real-estate (all asks) — Shop *sell* and every real-estate mutation trust client-supplied amounts/IDs with money debited client-side; a modified client can mint money or claim any building. The shop *buy* path (`RpcAsk_Buy` recomputes price + checks funds server-side) is the correct pattern to align both with.
- [ ] 💳 **Shop restocking has never worked** (**BUG-019**) — market, shops — `ReplenishStock` compares `stock < Round(stock*0.5)` (never true) and iterates all shops once per town with the wrong town's max stock; town shop stock only ever decreases after the initial roll.
- [ ] 💳 **JIP warehouse desync** (**BUG-022**) — real-estate — `RplSave`/`RplLoad` warehouse inventory loops use mismatched bounds (`OVT_RealEstateManagerComponent.c:762-767, 799-805`); any server with ≠1 warehouse corrupts the joining client's stream.
- [ ] 💳 **`"SCR_DestructibleBuildingEntity"` literal class-name filter** — shops, real-estate — appears in ~7 places across both features; a vanilla rename silently unregisters every shop and disables real estate with no warning.
- [ ] 💳 **Unstable int resource IDs on the wire** — market, shops — IDs are array indices built independently per machine with no client/server checksum; also blocks shop-stock persistence without a resource-name-keyed format.
- [ ] 💳 **`UpdateRents` early-return** (**BUG-023**) — market, real-estate — first resistance-owned/rented property aborts the whole nightly rent pass (`OVT_EconomyManagerComponent.c:243-256`).
- [ ] 💳 **Shop pagination arithmetic** (**BUG-024**) — shops — integer division hides catalogue remainders and can index the inventory map at -15.

---

## Master Overview Rollup

How this epic is represented in the project's master `docs/overview.md` (one row, not its children). Kept in sync by `/update-epic` and `/update-master`.

- **Rollup status:** 🟡 In Progress (3 features documented retrospectively; `shop-ux` built 37/37, pending play-test)
- **One-line summary for master:** Money, prices, shops and real estate. New: `shop-ux` (issue #145) built — category tabs, Buy/Sell mode split browsing the player's own inventory, vehicle trunk "Sell All", server-authoritative selling (BUG-020's shop half closed, BUG-024 fixed by construction), money formatting/HUD delta ticker + warehouse/real-estate QOL; awaiting human play-test.

---

*This is a required file — do not delete it; it marks the folder as an epic. Update it with `/update-epic economy` after working on the epic's features, and run `/review-epic economy` to refresh the Tech Debt / Findings section.*
