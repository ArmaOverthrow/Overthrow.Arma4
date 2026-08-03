# Occupying - Epic Overview

**Epic:** occupying
**Status:** 📄 Documented (4/4 retrospective)
**Last Updated:** 2026-08-02

> **This file is the epic marker.** Its presence in `docs/features/occupying/` is what tells every Beast Mode command (and future Web App / Discord clients) that this folder is an **epic**, not a plain feature. Keep it present and keep the required sections below filled in. It is the epic's equivalent of the project's `docs/overview.md`, scoped to this epic. The master `docs/overview.md` tracks this epic as a **single row**; the per-feature detail lives **here**.

---

## Purpose

The occupying epic owns Overthrow's AI antagonist: the occupying faction that holds the island at campaign start. It covers the faction's command layer (bases, radio towers, resources, threat and counter-attack orchestration), the base-upgrade classes that garrison and fortify its bases, the QRF system it fights back with, and the modular deployment framework that places its AI in the world. Together these are the single adversarial system the player resistance plays against.

---

## Features

The constituent features of this epic. All four already existed in code and were documented retrospectively via `/discover-feature` on 2026-08-02.

| # | Feature | Status | Tasks | Description |
|---|---------|--------|-------|-------------|
| 1 | core | 📄 Documented | — (retrospective) | `OVT_OccupyingFactionManager` (1508 L) + `OVT_BaseControllerComponent` — bases, radio towers, the resources/threat economy, counter-attack and town-battle decisions, capture flow, JIP + persistence. Headline debt: the per-base spend allocation is dead code (top base drains the whole reserve) and the capture RPCs are unvalidated. |
| 2 | base-upgrades | 📄 Documented | — (retrospective) | Nine prefab-registered upgrade classes (patrols, defenses, tower guards, checkpoints, compositions, parked vehicles, specops) with value-banked proximity virtualization. Headline debt: resource-accounting bugs cluster here (dead allocation clamp, proxied-bank inflation); checkpoints don't survive load; `TownPatrol` class is dead code. |
| 3 | qrf | 📄 Documented | — (retrospective) | `OVT_QRFControllerComponent` — 120 s countdown, wave spawning, 10 s zone-control scoring, base/town battle resolution. Headline debt: QRFs never debit faction resources; LZ clear-check is a no-op; players-only scoring; battles roll back on load. |
| 4 | deployments | 📄 Documented | — (retrospective) | Modular condition/spawning/behavior framework — the designated (stalled) successor to base-upgrades; ships 3 configs (town patrol + 2 vehicle patrols). Headline debt: per-faction deployment list leaks to its 100 cap (long-campaign kill switch); invested-resources/threat never set; zero replication. |

> Reference any feature with the slash form `occupying/[feature-name]` (e.g. `/continue-feature occupying/core`). Task counts are pulled from each feature's own `tasks.md` and refreshed by `/update-epic`.

---

## Build Order / Dependencies

Documentation (and any future enhancement work) follows the dependency chain:

1. **core** — the command layer everything else hangs off; owns base/tower records, resources and threat.
2. **base-upgrades** — driven by core's per-base controllers and resource budget.
3. **qrf** — triggered by core (and by capture attempts); resolves battles that change core's base ownership.
4. **deployments** — the newer modular framework core funds; the designated successor to base-upgrades whose migration stalled after one upgrade (town patrols) — today it ships 3 configs while base-upgrades still owns all static base defense.

**Dependencies between features:**
- core → base-upgrades (base controllers instantiate and fund upgrades)
- core → qrf (counter-attack orchestration spawns QRFs; QRF outcomes mutate base ownership)
- core → deployments (faction manager requests deployments; registry/config decide what spawns)
- External: core (epic) game-mode/config/persistence; town system for stability/support inputs and QRF targets.

---

## Integration & Architecture

- **Within the epic:** `OVT_OccupyingFactionManager` (singleton on the game mode) is the brain; `OVT_BaseControllerComponent` instances are its per-base hands; base upgrades and deployments are the two force-placement mechanisms (deliberately redundant — deployments was designed to replace base-upgrades and the migration stalled after one upgrade); QRF is the reactive combat layer. The two global war scalars (`m_iResources`, `m_iThreat`) live in the manager and fund/gate everything else, with the deployment manager holding a separate per-faction pool topped up from the same income.
- **With other epics / features:** consumes town stability/support (town system) for decisions and applies battle/patrol modifiers back; difficulty/config from the core epic (`OVT_DifficultySettings` OF tuning block); persists through three vanilla-persistence serializers (faction manager, deployment manager, deployment component). Map/HUD UI surfaces faction state (restricted areas, threat grid, icons, QRF banner) — deployments alone have zero client-visible surface.
- **Key architectural decisions for the epic as a whole:**
  - **Server-authoritative, replay-based persistence:** AI is never persisted as entities; garrisons/patrols respawn from upgrade state or config records on load (AI self-spawn disabled in `Overthrow.conf`). Exceptions: slotted compositions and deployment markers are entity-tracked. Live QRF battles are deliberately not persisted (clean rollback).
  - **One battle at a time:** `m_CurrentQRF` is a global singleton slot; while set, the whole faction freezes (economy tick, deployments evaluation, all garrison spawning map-wide).
  - **Two threat concepts:** the global escalation scalar and the spatial `GetThreatByLocation` score share a name but never interact.
  - **Virtualization everywhere, three different ways:** upgrades bank value ("proxying"), deployments toggle by proximity around a durable marker, towers spawn/despawn garrisons ad-hoc.

---

## Tech Debt / Findings

Cross-feature tech debt and review findings. **Populated and updated by `/review-epic`** (which also writes feature-specific findings into the relevant child `context.md` files). Seeded from discovery (2026-08-02); per-feature detail lives in each child's `implementation.md`/`context.md`.

- [ ] 💳 **Unvalidated client capture RPCs** (**BUG-025**) — core + qrf — `RpcAsk_StartBaseCapture` (no proximity/faction/alive/rate checks) and `RpcAsk_InstantCaptureBase` (unauthenticated instant base flip; DiagMenu gate is client-side only) in `OVT_PlayerCommsComponent.c:105-150`. The epic's biggest multiplayer-integrity hole; same class as the economy epic's client-authority bugs.
- [ ] 💳 **The resource economy's arithmetic is broken end-to-end** (**BUG-026** spend loop, **BUG-027** free QRFs, **BUG-029** upgrade bank drift) — core + base-upgrades + qrf — dead per-base allocation (`perBase` unused, top base drains the reserve; divide-by-zero at zero OF bases), dead allocation clamp in `SpendResources`, proxied-bank inflation in two upgrades, and QRFs reading but never debiting `m_iResources`. Together the OF's actual behavior is nearly unrelated to its authored tuning — and no test covers any of the math.
- [ ] 💳 **Two force-placement systems, one stalled migration** — base-upgrades + deployments — deployments was designed to replace base-upgrades; one upgrade migrated (`TownPatrol`, now dead code), both systems run concurrently sharing the same income, on two different faction prefab-resolution systems (legacy slot arrays vs registries). Needs a finish-or-formally-close decision.
- [ ] 💳 **Deployment bookkeeping shipped unfinished** (**BUG-028** faction-list leak) — deployments — `m_mFactionDeployments` leaks dead IDs to its 100 cap (silently halts all deploying on long campaigns); `m_iResourcesInvested`/`m_fThreatLevel` never set; seconds-vs-milliseconds mismatches; runtime conditions only evaluated by the reinforcement module.
- [ ] 💳 **Persistence blind spots** (**BUG-030** checkpoints) — core + base-upgrades + qrf — checkpoint compositions vanish on load while their slots stay blocked; composition `Deserialize` double-buys (`m_Spawned` unset); parked vehicles re-buy over restored ones; `m_aKnownTargets`/specops assignments reset every load; an autosave mid-QRF silently discards the battle. The `OVT_OccupyingFactionManagerSerializer` round trip is entirely untested.
- [ ] 💳 **QRF landing zones broken two ways** (**BUG-031**) — qrf — the clear-box trace is a no-op and file-scope LZ-cache globals collapse every wave source to one spawn point; per-base attack-geometry authoring is largely cosmetic.
- [ ] 💳 **JIP/client divergence** — core + qrf — one-shot 1 s `SetClientBaseFactions` reconcile race; `m_iCurrentQRFBase/Town` missing from the JIP payload (wrong map circles); BUG-013 (`QRFPointsToWin`/`QRFFastTravelMode` unreplicated); `OVT_TownController` reading server-only `m_CurrentQRF` on clients.
- [ ] 💳 **No victory/defeat condition** — core — nothing ends the campaign for either side; the QRF's "final base" fallback + sea/air-reinforcement To-Do is the only endgame handling that exists.
- [ ] 💳 **Test coverage is ~3 assertions for the whole epic** — all — manager/deployment-manager resolve + base registration. The Logic-tier candidates are listed per feature (income/threat math, QRF point model and wave budgeting, upgrade allocation arithmetic, deployment cost/selection, `ValidateAllConfigs()`).

---

## Master Overview Rollup

How this epic is represented in the project's master `docs/overview.md` (one row, not its children). Kept in sync by `/update-epic` and `/update-master`.

- **Rollup status:** 📄 Documented (4/4 retrospective)
- **One-line summary for master:** The AI occupying faction — command layer (bases/resources/threat), base upgrades, QRF battles and the modular deployment framework — backfilled with retrospective docs. Discovery catalogued ~90 concrete issues, headlined by: unvalidated capture RPCs (incl. an unauthenticated instant base flip), a broken resource-spend chain (dead per-base allocation, free QRFs, upgrade accounting drift), a deployment-list leak that silently halts all deploying on long campaigns, and a stalled base-upgrades→deployments migration. Top 7 filed as **BUG-025…031**.

---

*This is a required file — do not delete it; it marks the folder as an epic. Update it with `/update-epic occupying` after working on the epic's features, and run `/review-epic occupying` to refresh the Tech Debt / Findings section.*
