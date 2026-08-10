# Towns - Epic Overview

**Epic:** towns
**Status:** 📄 Documented (4/4 retrospective)
**Last Updated:** 2026-08-10

> **This file is the epic marker.** Its presence in `docs/features/towns/` is what tells every Beast Mode command (and future Web App / Discord clients) that this folder is an **epic**, not a plain feature. Keep it present and keep the required sections below filled in. It is the epic's equivalent of the project's `docs/overview.md`, scoped to this epic. The master `docs/overview.md` tracks this epic as a **single row**; the per-feature detail lives **here**.

---

## Purpose

The town system — the civilian heart of Overthrow's campaign. Towns have populations, stability and support that shift in response to everything both factions do; nearly every other domain (economy, jobs, occupying, resistance, skills) reads or writes town state. This epic owns the town manager and controllers, the shared stability/support modifier framework and both modifier systems, and per-town gun dealers. It was pre-declared by the core epic ("gameplay domains — **towns**, jobs, economy … will get their own epics later") and backfilled with retrospective docs via `/discover-feature` on 2026-08-02. Future town work (undercover heat, town-state and economy ties) lands as features here.

> **The map is no longer this epic's.** Everything the player sees or does on the fullscreen map — including the town info panel, town markers, overlays and the fast/bus travel verbs — belongs to the **`map` epic** (`docs/features/map/`). The former feature 5, `map-info`, was archived on 2026-08-10; see the note under the Features table.

---

## Features

The constituent features of this epic, in build order. This is the epic's equivalent of `docs/overview.md`'s feature-status table, scoped to the epic. Each feature is a subfolder under `docs/features/towns/`.

| # | Feature | Status | Tasks | Description |
|---|---------|--------|-------|-------------|
| 1 | core | 📄 Documented | — (retrospective) | `OVT_TownManagerComponent` (1579 L) + `OVT_TownController` — discovery from world-authored controllers, `OVT_TownData` records, population growth, village flips, queries, index-addressed RPC/JIP sync, location-matched persistence. Headline debt: unvalidated RPC surface, broken legacy-map path, first-not-nearest range query. |
| 2 | stability | 📄 Documented | — (retrospective) | The **shared modifier framework** (`OVT_TownModifierSystem` base) + the stability system: 11 conf-registered modifiers, derived `clamp(100 + Σ effects)` value. Headline debt: permanent modifiers wiped on tick rebuild (+ client chip desync), shared-instance hour-gates only ever process town[0], game-start `OnStart` burst dead. |
| 3 | support | 📄 Documented | — (retrospective) | Supporter headcounts moved by a ±1-per-70 s random walk with equilibrium damping; 13 modifiers, civilian conversion (Diplomacy), posters, momentum; drives flips, QRFs, donations, recruitment. Headline debt: fully client-trusted conversion RPC, unclamped accounting (support > population), momentum stacking to 5×. |
| 4 | gun-dealers | 📄 Documented | — (retrospective) | One invulnerable dealer per non-village town: catalog-query inventory with one random heavy weapon per category (the travel mechanic), black-market modifier tie-in. Headline debt: weapon stock never restocks (save/load reroll is the only refill), BUG-005's X-only check lives in **five** sites incl. the spawner, empty-town spawn crash. |

> **Archived — former feature 5, `map-info`.** The vanilla-map extension (town info panel + modifier chips, the `OVT_MapIcons` layer, restricted-area overlays, fast/bus travel) was **superseded by the `map` epic**, and the legacy code it documented was **deleted by `map/legacy-retirement` on 2026-08-10**. Its three docs are preserved as history at `docs/archive/towns-map-info-{context,implementation,tasks}.md`; the successor is `docs/features/map/epic-overview.md`.

> Reference any feature with the slash form `towns/core` (e.g. `/continue-feature towns/core`). Task counts are pulled from each feature's own `tasks.md` and refreshed by `/update-epic`.

---

## Build Order / Dependencies

1. **core** — the data model, discovery, tick, transport and persistence everything else rides on.
2. **stability** — owns the shared modifier framework docs; the manager instantiates/ticks it.
3. **support** — reuses the framework with an overridden `Recalculate`; cross-feeds stability both ways.
4. **gun-dealers** — spawned by the town controller, recorded on `OVT_TownData`, feeds the black-market modifiers.

*(The map UI, formerly built last as a pure consumer of all four, is now the `map` epic's — it reads town records, modifier configs and registries client-side across an epic boundary.)*

**Dependencies between features:**
- core → stability/support (data + transport vs. algorithm + payload semantics — the dividing line is documented in each)
- stability → support (framework base classes; config-index identity convention)
- core + economy → gun-dealers (controller spawns; economy registry/pricing)
- External: core epic (game-mode lifecycle, config, persistence), economy (shops/transactions/money), occupying (control, deaths, QRF, threat, patrols), resistance (supporter draw-down, posters), skills (Diplomacy), jobs (town conditions/stages), **map** (read-only display of town records and modifier chips — the epic that absorbed the former `map-info`).

---

## Integration & Architecture

- **Within the epic:** one manager owns the data and all transport (index-addressed RPCs + hand-rolled JIP + location-matched vanilla persistence); the two modifier systems share one framework and differ only in `Recalculate` (derived sum vs. ±1 random walk) — the asymmetries (no recalc-on-add for support, raw-restore vs. recompute) are deliberate and documented per feature. Gun dealers are the epic's world-entity consumer; the UI consumer (the map) now lives in the `map` epic.
- **With other epics / features:**
  - **core/persistence** — `OVT_TownManagerSerializer` (location match, modifier id/timer arrays, stability recomputed / support raw); the restore-ordering contract (EOnInit before AFTER_ENTITY_FINALIZE).
  - **economy** — tax/donation/stock/NPC-buying formulas read population/stability/support; transaction events drive StrongEconomy/BlackMarket modifiers; dealers are excluded shops.
  - **occupying** — control-change threat, QRF town selection + controller-authored attack geometry, RecentBattle + `ResetSupport` outcomes, tower/base proximity modifiers, patrol harassment (fed by deployments — BUG-028 upstream), threat scoring.
  - **resistance** — garrison/tent recruitment draws down supporters *and population*; poster placeables use the `OnPlace` bool contract.
  - **skills** — Diplomacy → `player.diplomacy`, read only by the conversion action.
  - **jobs** — town/support/dealer conditions and stages; jobs' BUG-005/BUG-041 target town fields.
  - **map** — the fullscreen map renders this epic's records (town markers, the town info panel and its modifier chips) as a **read-only projection of replicated state**, and must not become a second source of truth for town data. It absorbed the former `map-info` feature on 2026-08-10.
  - **dev-ops** — 6 Logic town cases + Init/Campaign/Persistence/RoundTrip coverage of the manager seams; the map UI that renders these records has zero tests either way (UI is play-test gated).
- **Key architectural decisions for the epic as a whole:** town identity = array index everywhere (wire, save-adjacent, shop/job keys) with client-side rediscovery — compact but unvalidated; modifier identity = config index (append-only configs); derived-stability/raw-support persistence split; `RpcAsk` executes locally+synchronously on the server (empirically settled — "RPC-only" mutators are correct, not dead).

---

## Tech Debt / Findings

Cross-feature tech debt and review findings. **Populated and updated by `/review-epic`** (which also writes feature-specific findings into the relevant child `context.md` files). Seeded from the 2026-08-02 discovery:

- [ ] 💳 **Client-trust RPC seams** — core, support — `RpcAsk_Add*Modifier` and `RpcAsk_AddSupporters` validate nothing (server crash / instant village flips, BUG-060/063). Same class as the skills/resistance epic findings. *The travel half of this bullet has left the epic and is **closed**: `map/fast-travel` moved execution and payment server-side onto `OVT_TravelRequestComponent` (validate → teleport → charge only on success), and `map/legacy-retirement` deleted the four unvalidated `RequestFastTravel*` RPCs from `OVT_PlayerCommsComponent` on 2026-08-10. BUG-053's recruit half is tracked by the `map` epic.*
- [ ] 💳 **The modifier tick rebuild is the epic's highest-leverage fix cluster** — stability, support — permanent modifiers wiped + client chip desync (BUG-057), shared-instance hour gates (BUG-058), dead `OnStart` (BUG-059): three < 20-line fixes in two files.
- [ ] 💳 **Index-identity everywhere, validated nowhere** — core, stability, support (+ the `map` epic) — town index (client rediscovery, no checksum; JIP/RpcDo handlers unguarded) and modifier config index (conf reorder remaps saves). Bounds checks + append-only discipline docs are the cheap mitigation. *The client-UI half moved with the town info panel to the `map` epic and **survived the rewrite**: the deleted `OVT_MapContext` indexed `m_aModifiers[data.id]` without a bounds check, and so does its successor `OVT_MapLocationTown.c:272` (the `if (!modifierConfig)` that follows catches a null entry, not an out-of-range id).*
- [ ] 💳 **`gunDealerPosition` as position-and-boolean** — gun-dealers, core, jobs — BUG-005's X-only test exists in five sites including the spawner's persisted-reuse branch; an explicit `hasGunDealer` flag retires the class (and the pinning test must flip in the same commit).
- [ ] 💳 **Support accounting is unclamped and silently fallible** — support, core, resistance — support > population breaks every percentage gate (test-pinned 300%); `TakeSupportersFromNearestTown` silent-fails into free garrisons; population-0 towns ERROR-spam (BUG-064).
- [ ] 💳 **Dead feedback loops that read as implemented** — support, occupying — town-patrol modifiers use names that exist in no config + a headcount-vs-percentage gate (BUG-066); RecentGunfire is a dead entry the persistence tests happen to depend on picking.
- [ ] 💳 **Deprecated town ranges vs controller ranges vs hardcoded 500 m** — core, support (+ the `map` epic) — three range regimes decide "in town" for shops, modifiers, placeables and bounds; unify on the controller attribute. *The map-side consumer of these ranges is now the `map` epic's, but the ranges themselves are still this epic's to unify.*
- [x] ➡️ **Map lifecycle mechanics — MOVED to the `map` epic (2026-08-10).** No longer this epic's debt. The `new-map` rewrite this bullet said to check before investing **is what landed**, and `map/legacy-retirement` then deleted the legacy layer. **BUG-067** (per-frame validation), **BUG-068** (icon parallel-array desync) and **BUG-069** (close-path leaks) are now **structurally impossible** — `OVT_MapIcons.c`, the three flag-based `OVT_MapContext` modes and the static map-close subscription that could exhibit them no longer exist. **BUG-070 (drawn ≠ enforced restriction radii) is still open** and lives in the `map` epic: it concerns `OVT_MapRestrictedAreas`, which retirement deliberately **retained and did not touch**, and which must stay in agreement with `resistance/fob`'s deploy check.
- [ ] 💳 **No victory condition exists** — epic-wide observation (matches the occupying epic's finding) — support drives control, not endgame; a win/lose layer is a future feature, not a bug fix.

---

## Master Overview Rollup

How this epic is represented in the project's master `docs/overview.md` (one row, not its children). Kept in sync by `/update-epic` and `/update-master`.

- **Rollup status:** 📄 Documented (4/4 retrospective)
- **One-line summary for master:** Towns — populations, stability & support modifiers and gun dealers — backfilled with retrospective docs (`core`, `stability`, `support`, `gun-dealers`). Discovery catalogued ~100 concrete issues; top 17 filed as BUG-055…071, of which the four map bugs (BUG-067…070) moved to the `map` epic when the fifth feature, `map-info`, was archived on 2026-08-10.

---

*This is a required file — do not delete it; it marks the folder as an epic. Update it with `/update-epic towns` after working on the epic's features, and run `/review-epic towns` to refresh the Tech Debt / Findings section.*
