# Towns - Epic Overview

**Epic:** towns
**Status:** 📄 Documented (5/5 retrospective)
**Last Updated:** 2026-08-03 00:45

> **This file is the epic marker.** Its presence in `docs/features/towns/` is what tells every Beast Mode command (and future Web App / Discord clients) that this folder is an **epic**, not a plain feature. Keep it present and keep the required sections below filled in. It is the epic's equivalent of the project's `docs/overview.md`, scoped to this epic. The master `docs/overview.md` tracks this epic as a **single row**; the per-feature detail lives **here**.

---

## Purpose

The town system — the civilian heart of Overthrow's campaign. Towns have populations, stability and support that shift in response to everything both factions do; nearly every other domain (economy, jobs, occupying, resistance, skills) reads or writes town state. This epic owns the town manager and controllers, the shared stability/support modifier framework and both modifier systems, per-town gun dealers, and the map UI (town info panel, icons, overlays, fast/bus travel). It was pre-declared by the core epic ("gameplay domains — **towns**, jobs, economy … will get their own epics later") and backfilled with retrospective docs via `/discover-feature` on 2026-08-02. Future town work (undercover heat, town map icons, the `new-map` migration) lands as features here.

---

## Features

The constituent features of this epic, in build order. This is the epic's equivalent of `docs/overview.md`'s feature-status table, scoped to the epic. Each feature is a subfolder under `docs/features/towns/`.

| # | Feature | Status | Tasks | Description |
|---|---------|--------|-------|-------------|
| 1 | core | 📄 Documented | — (retrospective) | `OVT_TownManagerComponent` (1579 L) + `OVT_TownController` — discovery from world-authored controllers, `OVT_TownData` records, population growth, village flips, queries, index-addressed RPC/JIP sync, location-matched persistence. Headline debt: unvalidated RPC surface, broken legacy-map path, first-not-nearest range query. |
| 2 | stability | 📄 Documented | — (retrospective) | The **shared modifier framework** (`OVT_TownModifierSystem` base) + the stability system: 11 conf-registered modifiers, derived `clamp(100 + Σ effects)` value. Headline debt: permanent modifiers wiped on tick rebuild (+ client chip desync), shared-instance hour-gates only ever process town[0], game-start `OnStart` burst dead. |
| 3 | support | 📄 Documented | — (retrospective) | Supporter headcounts moved by a ±1-per-70 s random walk with equilibrium damping; 13 modifiers, civilian conversion (Diplomacy), posters, momentum; drives flips, QRFs, donations, recruitment. Headline debt: fully client-trusted conversion RPC, unclamped accounting (support > population), momentum stacking to 5×. |
| 4 | gun-dealers | 📄 Documented | — (retrospective) | One invulnerable dealer per non-village town: catalog-query inventory with one random heavy weapon per category (the travel mechanic), black-market modifier tie-in. Headline debt: weapon stock never restocks (save/load reroll is the only refill), BUG-005's X-only check lives in **five** sites incl. the spawner, empty-town spawn crash. |
| 5 | map-info | 📄 Documented | — (retrospective) | Vanilla-map extension by config delta: town info panel + modifier chips, icon system with RplId retry/fallback, restricted-area overlays, fast/bus travel. Headline debt: per-frame validation (ms/s units), icon parallel-array desyncs, leaky close paths (stuck panel, surviving bus mode), drawn ≠ enforced radii. |

> Reference any feature with the slash form `towns/core` (e.g. `/continue-feature towns/core`). Task counts are pulled from each feature's own `tasks.md` and refreshed by `/update-epic`.

---

## Build Order / Dependencies

1. **core** — the data model, discovery, tick, transport and persistence everything else rides on.
2. **stability** — owns the shared modifier framework docs; the manager instantiates/ticks it.
3. **support** — reuses the framework with an overridden `Recalculate`; cross-feeds stability both ways.
4. **gun-dealers** — spawned by the town controller, recorded on `OVT_TownData`, feeds the black-market modifiers.
5. **map-info** — pure consumer: renders the records, modifier configs and registries client-side.

**Dependencies between features:**
- core → stability/support (data + transport vs. algorithm + payload semantics — the dividing line is documented in each)
- stability → support (framework base classes; config-index identity convention)
- core + economy → gun-dealers (controller spawns; economy registry/pricing)
- all → map-info (read-only display)
- External: core epic (game-mode lifecycle, config, persistence), economy (shops/transactions/money), occupying (control, deaths, QRF, threat, patrols), resistance (supporter draw-down, posters), skills (Diplomacy), jobs (town conditions/stages).

---

## Integration & Architecture

- **Within the epic:** one manager owns the data and all transport (index-addressed RPCs + hand-rolled JIP + location-matched vanilla persistence); the two modifier systems share one framework and differ only in `Recalculate` (derived sum vs. ±1 random walk) — the asymmetries (no recalc-on-add for support, raw-restore vs. recompute) are deliberate and documented per feature. Gun dealers and the map are consumers on opposite ends (world entity vs. UI).
- **With other epics / features:**
  - **core/persistence** — `OVT_TownManagerSerializer` (location match, modifier id/timer arrays, stability recomputed / support raw); the restore-ordering contract (EOnInit before AFTER_ENTITY_FINALIZE).
  - **economy** — tax/donation/stock/NPC-buying formulas read population/stability/support; transaction events drive StrongEconomy/BlackMarket modifiers; dealers are excluded shops.
  - **occupying** — control-change threat, QRF town selection + controller-authored attack geometry, RecentBattle + `ResetSupport` outcomes, tower/base proximity modifiers, patrol harassment (fed by deployments — BUG-028 upstream), threat scoring.
  - **resistance** — garrison/tent recruitment draws down supporters *and population*; poster placeables use the `OnPlace` bool contract.
  - **skills** — Diplomacy → `player.diplomacy`, read only by the conversion action.
  - **jobs** — town/support/dealer conditions and stages; jobs' BUG-005/BUG-041 target town fields.
  - **dev-ops** — 6 Logic town cases + Init/Campaign/Persistence/RoundTrip coverage of the manager seams; the map layer has zero tests (UI is play-test gated).
- **Key architectural decisions for the epic as a whole:** town identity = array index everywhere (wire, save-adjacent, shop/job keys) with client-side rediscovery — compact but unvalidated; modifier identity = config index (append-only configs); derived-stability/raw-support persistence split; `RpcAsk` executes locally+synchronously on the server (empirically settled — "RPC-only" mutators are correct, not dead).

---

## Tech Debt / Findings

Cross-feature tech debt and review findings. **Populated and updated by `/review-epic`** (which also writes feature-specific findings into the relevant child `context.md` files). Seeded from the 2026-08-02 discovery:

- [ ] 💳 **Client-trust RPC seams** — core, support, map-info — `RpcAsk_Add*Modifier` and `RpcAsk_AddSupporters` validate nothing (server crash / instant village flips, BUG-060/063); fast/bus travel charge money client-side and two of five travel paths teleport client-locally (BUG-053's class). Same class as the skills/resistance epic findings.
- [ ] 💳 **The modifier tick rebuild is the epic's highest-leverage fix cluster** — stability, support — permanent modifiers wiped + client chip desync (BUG-057), shared-instance hour gates (BUG-058), dead `OnStart` (BUG-059): three < 20-line fixes in two files.
- [ ] 💳 **Index-identity everywhere, validated nowhere** — core, stability, support, map-info — town index (client rediscovery, no checksum; JIP/RpcDo handlers unguarded) and modifier config index (client map UI indexes wire ids unguarded; conf reorder remaps saves). Bounds checks + append-only discipline docs are the cheap mitigation.
- [ ] 💳 **`gunDealerPosition` as position-and-boolean** — gun-dealers, core, jobs — BUG-005's X-only test exists in five sites including the spawner's persisted-reuse branch; an explicit `hasGunDealer` flag retires the class (and the pinning test must flip in the same commit).
- [ ] 💳 **Support accounting is unclamped and silently fallible** — support, core, resistance — support > population breaks every percentage gate (test-pinned 300%); `TakeSupportersFromNearestTown` silent-fails into free garrisons; population-0 towns ERROR-spam (BUG-064).
- [ ] 💳 **Dead feedback loops that read as implemented** — support, occupying — town-patrol modifiers use names that exist in no config + a headcount-vs-percentage gate (BUG-066); RecentGunfire is a dead entry the persistence tests happen to depend on picking.
- [ ] 💳 **Deprecated town ranges vs controller ranges vs hardcoded 500 m** — core, support, map-info — three range regimes decide "in town" for shops, modifiers, placeables and bounds; unify on the controller attribute.
- [ ] 💳 **Map lifecycle mechanics** — map-info — per-frame validation (BUG-067), icon array desync (BUG-068), close-path leaks (BUG-069), drawn ≠ enforced (BUG-070); the `new-map` branch is the designed successor — check it before deep investment.
- [ ] 💳 **No victory condition exists** — epic-wide observation (matches the occupying epic's finding) — support drives control, not endgame; a win/lose layer is a future feature, not a bug fix.

---

## Master Overview Rollup

How this epic is represented in the project's master `docs/overview.md` (one row, not its children). Kept in sync by `/update-epic` and `/update-master`.

- **Rollup status:** 📄 Documented (5/5 retrospective)
- **One-line summary for master:** Towns — populations, stability & support modifiers, gun dealers and the map UI — backfilled with retrospective docs (`core`, `stability`, `support`, `gun-dealers`, `map-info`). Discovery catalogued ~100 concrete issues; top 17 filed as BUG-055…071.

---

*This is a required file — do not delete it; it marks the folder as an epic. Update it with `/update-epic towns` after working on the epic's features, and run `/review-epic towns` to refresh the Tech Debt / Findings section.*
