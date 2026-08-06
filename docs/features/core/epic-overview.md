# Core - Epic Overview

**Epic:** core
**Status:** 🟢 3/6 documented (legacy); persistence + player-groups BUILT — both awaiting their MP/dedicated play-test; controller-migration 📋 planned & scoped
**Last Updated:** 2026-08-06 23:05

> **This file is the epic marker.** Its presence in `docs/features/core/` is what tells every Beast Mode command (and future Web App / Discord clients) that this folder is an **epic**, not a plain feature. Keep it present and keep the required sections below filled in. It is the epic's equivalent of the project's `docs/overview.md`, scoped to this epic. The master `docs/overview.md` tracks this epic as a **single row**; the per-feature detail lives **here**.

---

## Purpose

The `core` epic owns Overthrow's foundational systems — the skeleton every other system stands on: the game-mode/manager lifecycle (`OVT_OverthrowGameMode`, `OVT_Global`, `OVT_OverthrowController`), configuration, player identity, and the persistence layer. Gameplay domains (towns, jobs, economy, occupying faction, etc.) will get their own epics later; this epic holds only the very core. It was created by migrating the pre-existing `vanilla-persistence` feature (now `core/persistence`) and will be backfilled with `/discover-feature` docs for the legacy core systems.

---

## Features

The constituent features of this epic, in build order. This is the epic's equivalent of `docs/overview.md`'s feature-status table, scoped to the epic. Each feature is a subfolder under `docs/features/core/`.

| # | Feature | Status | Tasks | Description |
|---|---------|--------|-------|-------------|
| 1 | game-mode | 📄 Documented (legacy) | — | `OVT_OverthrowGameMode` lifecycle, manager init order, `OVT_Global` service locator, `OVT_OverthrowController` client→server seam. Headline debt: controller migration stalled — 57 RPCs still on `OVT_PlayerCommsComponent`. Linked bugs: BUG-012, BUG-017. |
| 2 | config | 📄 Documented (legacy) | — | `OVT_OverthrowConfigComponent`, 46 `.conf` files across three loading mechanisms, faction role model, difficulty presets. Headline debt: 7 client-read difficulty fields never replicate. Linked bugs: BUG-009, BUG-011, BUG-013, BUG-014. |
| 3 | player-manager | 📄 Documented (legacy) | — | Persistent identity (backend UUID / name-derived in SP), `OVT_PlayerData` registry, two-phase registration, JIP snapshot. Headline debt: `IsOffline()` wrong after disconnect; ID maps never pruned. Linked bugs: BUG-015, BUG-016. |
| 4 | persistence | 🟢 Built — SP play-test green, MP half outstanding | 43/44 (98%) | EPF → vanilla persistence migration, **built and gate-verified 2026-08-02** (overnight autorun: plan v2 vs real 1.7.0.54 API, then Phases 1-6; Phase 8 closed GitHub #143 with in-session vehicle respawn). `OVT_TEST_PersistenceRoundTripSuite` flipped exit 1 → 0, was de-quarantined into the All group and now carries 10 cases, with 3 Init-tier persistence cases alongside it. Zero EPF/EDF references repo-wide. **SP play-test 2026-08-03: checklist items 1–15 ✅** after five same-day fixes. Remaining: section D (items 16–20 — JIP, two-player isolation, vehicle reconnect, dedicated restart), which needs a dev build on a server. BUG-002/BUG-006 closed; **BUG-018 open** — corpses still don't survive a continue, third fix missed, deferred 2026-08-04 past the 1.4.0 release and posted as public issue #153. |
| 5 | controller-migration | 📋 Planned | — | Migrate the 60-RPC `OVT_PlayerCommsComponent` monolith (1,776 L, 62 call sites) onto domain components on `OVT_OverthrowController` — the engine-intended per-player client→server seam, pattern proven by `OVT_ContainerTransferComponent`. Server-side validation rides with each migrated RPC, retiring the client-trust bug class (BUG-025 family) instead of porting it. **Scope expanded 2026-08-04** with the `OVT_Global` cleanup that precedes and enables it: a compile-verified generic `OVT_ControllerComponent<Class T>` accessor replacing the per-domain getters (so ~10 domains add nothing to `OVT_Global`), the local controller cached at owner assignment instead of derived from the possessed body, null guards on the accessors that outlive the feature, the two warehouse helpers moved onto `OVT_RealEstateManagerComponent`, and the file's ~470-line utility half split into `OVT_WorldUtils`/`OVT_PrefabUtils`/`OVT_LoadoutUtils`. Renaming the 17 manager forwarders (~900 call sites) is explicitly out of scope. |
| 6 | player-groups | 🟢 Built — MP play-test outstanding | 30/31 (97%) | Player squads: your own private group by default, opt-in joining of another player's group through vanilla's request/approve flow, a warned Group Menu (explanatory text + pre-join confirmation dialog), and the **recruits-follow-their-owner** rule — ownership (dismiss/rename/inventory/loadout/XP) never transfers and recruits return with you when you leave, switch groups or reconnect. Addresses the second half of issue **#147**. **Built 2026-08-06** in one autorun (6 phases, 3 on advanced agents): compile-check 0, Fast 38, All **66** (was 60 — 1 new Init case + 5 new Logic cases, each proven able to fail). New `OVT_PlayerGroupManagerComponent` (server-side reactor, no replicated state, nothing persisted), pure `OVT_GroupRecruitTransfer` helper, 3 `modded class` overrides on the vanilla group stack. **Phase 1's diagnosis found no residual Group-Menu fault** — every #147 symptom was downstream of BUG-088's single null — but the build then uncovered four defects the plan never predicted: `SetName` was the wrong API so **group names never displayed even in solo**; **vanilla has no Leave action at all** and its stand-in ("Create new group") was permanently disabled by a stray empty faction group, so **nobody could ever leave a group**; fixing the approve path silently activated a D2 violation on reconnect; and a **disconnect never reached the recruit pull-out**, leaving an ex-leader commanding a departed player's AI. **Remaining: the two-client dedicated-server play-test (§6, 16 steps), a gamepad pass, one Workbench export of 2 keys, and the AI-density measurement (budget shipped at 0 = disabled).** |

> Reference any feature with the slash form `core/[feature-name]` (e.g. `/continue-feature core/persistence`). Task counts are pulled from each feature's own `tasks.md` and refreshed by `/update-epic`. Features #1-#3 are `/discover-feature` retrospectives of legacy code — "Documented" means the docs exist, not that improvement work is planned or complete.

---

## Build Order / Dependencies

Which features come first, and why. This feeds planning and the next-step suggestions from `/continue-feature core`.

1. **game-mode** — foundational: manager registration/lifecycle and global access underpin everything else in the mod.
2. **config** — loaded by the game mode at init; other systems read it.
3. **player-manager** — persistent player identity; consumed by ownership, skills, and persistence.
4. **persistence** — spans all of the above (it serializes their state). Built 2026-08-02; validated by the (now de-quarantined) round-trip gate; SP play-test green 2026-08-03, MP/dedicated half still owed.
5. **controller-migration** — the first *forward-work* feature on the legacy core: replaces `OVT_PlayerCommsComponent` with domain components on `OVT_OverthrowController`. **Its scheduling dependency has cleared** — PR #152 (`1.4.0-bugfixes`) is merged, so the monolith is no longer being patched from two directions. Order within the feature: `OVT_Global`/controller-lifecycle hardening first, then domain-by-domain migration with validation, deleting the monolith at the end. Plan it once the in-flight `economy/shop-ux` branch merges — that branch is still adding controller getters to `OVT_Global`.
6. **player-groups** — sits on `game-mode`'s spawn logic (where the per-player group is created) and on `resistance/recruits` (whose agents follow their owner between groups). Independent of `controller-migration`. **BUILT 2026-08-06.** It needed **no** new client→server RPC (decision D1), so it adds nothing to `OVT_PlayerCommsComponent` and no getter to `OVT_Global` — verified by empty diffs. It does add a new Manager (`OVT_PlayerGroupManagerComponent`) to the game-mode prefab, and three `modded class` overrides on the vanilla group stack whose vanilla-update check-list lives in the feature's `context.md`. Remaining work is play-test only.

**Dependencies between features:**
- game-mode → config, player-manager, persistence (manager lifecycle hosts all of them)
- persistence → everything (serializes the state the other core systems own)
- game-mode + player-manager → controller-migration (controller spawn/ownership/registration is their code; debt items BUG-012/BUG-017 intersect)
- External: dev-ops epic supersedes `core/persistence` in priority and supplies its acceptance gate (`dev-ops/test-coverage`). controller-migration's 1.4.x wait is **discharged** (PR #152 merged 2026-08-04); its one remaining scheduling constraint is the in-flight `economy/shop-ux` branch, which is still adding controller-component getters to `OVT_Global`. Independent of the virtualization epic (parallel-safe).

---

## Integration & Architecture

How the features fit together as one coherent system, and how this epic integrates with the rest of the project.

- **Within the epic:** `OVT_OverthrowGameMode` instantiates and initializes the manager singletons; `OVT_Global` exposes them statically; `OVT_OverthrowController` is the client→server seam; `OVT_PersistenceManagerComponent` (feature #1) saves/loads the state those managers own.
- **`OVT_Global` is the epic's shared surface, and it is `game-mode`'s code:** 17 manager forwarders (`GetConfig` 313 call sites, `GetPlayers` 178, `GetTowns` 168 — ~900 in total, plus 152 uses across the test spine) alongside the client→server seam accessors. `controller-migration` is the feature that reshapes it: the locator half is deliberately frozen, the seam half is rebuilt around a generic controller accessor, and the utility half moves out. **Any epic adding a controller component should reach it through that generic accessor rather than adding a getter to `OVT_Global`.**
- **With other epics / features:** `dev-ops/test-coverage` provided the behaviour-level persistence suites; `OVT_TEST_PersistenceRoundTripSuite` was `core/persistence`'s acceptance gate and **discharged 2026-08-02** (exit 1 → 0; now a permanent All-group member, with `run-tests.sh` auto-resetting CI save state per run). Future domain epics (towns, jobs, economy…) all build on the manager lifecycle this epic documents. Persistence bindings are conf-first: `Configs/Systems/Persistence/Overthrow.conf` inherits vanilla `Common.conf` and is the single place serializers/collections are declared (see decision v2-5 in the feature's context.md for the "who persists what" dividing line).
- **Key architectural decisions for the epic as a whole:** Manager (gamemode singleton) vs Controller (per-entity) split; server authority with RplProp/RPC/JIP replication; big-bang EPF → vanilla persistence migration on the `vanilla-persistence` branch with `main` frozen until it lands.

---

## Tech Debt / Findings

Cross-feature tech debt and review findings. **Populated and updated by `/review-epic`** (which also writes feature-specific findings into the relevant child `context.md` files). Start empty.

- [ ] 💳 **No working save path in either system** — persistence — measured 2026-08-02 by `dev-ops/test-coverage`: `SaveGame()` is a stub and the EPF re-parenting means EPF never initialises either. Tracked as BUG-002 / BUG-006 (`linkedFeature: core/persistence`).
- (further findings: `/review-epic core` will surface cross-feature integration issues here)

---

## Master Overview Rollup

How this epic is represented in the project's master `docs/overview.md` (one row, not its children). Kept in sync by `/update-epic` and `/update-master`.

- **Rollup status:** 🟢 3/6 documented; persistence built (43/44, SP play-test green, MP half outstanding); **player-groups built (30/31, MP play-test outstanding)**; controller-migration planned & scoped
- **One-line summary for master:** Overthrow's core systems. `game-mode`, `config`, `player-manager` 📄 documented retrospectively — all 8 linked bugs (BUG-009/011/012/013/014/015/016/017) now closed. `persistence` — EPF → vanilla migration **built and gate-verified 2026-08-02**, EPF/EDF fully removed, breaking save change; **SP play-test green 2026-08-03 (items 1–15)** after five same-day fixes, with section D (JIP, two-player isolation, dedicated restart) still needing a server dev build and BUG-018 (corpses across a continue) deferred past 1.4.0 as public issue #153. `controller-migration` 📋 planned 2026-08-03 and **scoped 2026-08-04**: retire the 60-RPC `OVT_PlayerCommsComponent` monolith onto `OVT_OverthrowController` domain components with validation riding each RPC, preceded by an `OVT_Global` cleanup (compile-verified generic controller accessor, controller cached at owner assignment, utility half split out); its 1.4.x scheduling wait is discharged now that PR #152 is merged. `player-groups` **built 2026-08-06** — private-by-default groups, a warned join through vanilla's request/approve flow, and recruits that follow their owner without ownership ever transferring; All group 60 → **66** assertions. Diagnosis found no residual Group-Menu fault (all of #147 was downstream of BUG-088), but the build uncovered four unpredicted defects, two of them severe: group names never displayed even in solo, and **nobody could ever leave a group** because vanilla has no Leave action and its stand-in was permanently disabled. MP play-test, a gamepad pass, one Workbench export and the AI-density measurement remain.

---

*This is a required file — do not delete it; it marks the folder as an epic. Update it with `/update-epic core` after working on the epic's features, and run `/review-epic core` to refresh the Tech Debt / Findings section.*
