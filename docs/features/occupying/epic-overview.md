# Occupying - Epic Overview

**Epic:** occupying
**Status:** 🟢 Active — 4 documented retrospective (`base-upgrades` now **retired in code**), `counter-attacks` built 2026-08-19 (Phases 1–10; suites, play-test and a localization re-export owed)
**Last Updated:** 2026-08-19 (occupying/counter-attacks Phase 10)

> **This file is the epic marker.** Its presence in `docs/features/occupying/` is what tells every Beast Mode command (and future Web App / Discord clients) that this folder is an **epic**, not a plain feature. Keep it present and keep the required sections below filled in. It is the epic's equivalent of the project's `docs/overview.md`, scoped to this epic. The master `docs/overview.md` tracks this epic as a **single row**; the per-feature detail lives **here**.

---

## Purpose

The occupying epic owns Overthrow's AI antagonist: the occupying faction that holds the island at campaign start. It covers the faction's command layer (bases, radio towers, resources, threat), the **objective director** that decides what the faction is trying to take back and runs the three-phase campaign to take it, the QRF system that resolves every battle, and the modular deployment framework that places its AI in the world. Together these are the single adversarial system the player resistance plays against.

⚠ **The `base-upgrades` feature is retired in code.** `virtualization/base-defense-migration` deleted `Scripts/Game/Components/BaseUpgrades/` and every `OVT_BaseUpgrade*` behaviour class; base defence is now nine deployment configs on the deployments framework. The only `OVT_BaseUpgrade*` symbols left in the tree are the legacy **save-payload** classes in `OVT_OccupyingFactionManager.c:5-30` and `OVT_OccupyingFactionManagerSerializer.c`, kept so pre-migration saves still load. Its docs folder stays as the historical record.

---

## Features

Four already existed in code and were documented retrospectively via `/discover-feature` on 2026-08-02; `counter-attacks` is the epic's first planned-and-built feature.

| # | Feature | Status | Tasks | Description |
|---|---------|--------|-------|-------------|
| 1 | core | 📄 Documented (retrospective; substantially changed since) | — (retrospective) | `OVT_OccupyingFactionManager` + `OVT_BaseControllerComponent` — bases, radio towers, the resources/threat economy, battle start/resolution, capture flow, JIP + persistence. ⚠ It no longer **decides** to counter-attack: both legacy triggers were deleted by `counter-attacks` Phase 1 and that decision now lives in `OVT_ObjectiveDirectorComponent`. Remaining headline debt: the capture RPCs are unvalidated (**BUG-025**). The per-base spend allocation debt is gone with `SpendResources` itself. |
| 2 | base-upgrades | 🗄️ **Retired in code** (docs kept as history) | — (retrospective) | ⚠ Superseded by `virtualization/base-defense-migration`: `Scripts/Game/Components/BaseUpgrades/` and all `OVT_BaseUpgrade*` behaviour classes are **deleted**; base defence is nine deployment configs, funded from the one deployment pool. Only the legacy save-payload classes survive, so pre-migration saves still load. The debt listed below was resolved by deletion, not by fixing. Historical description: nine prefab-registered upgrade classes (patrols, defenses, tower guards, checkpoints, compositions, parked vehicles, specops) with value-banked proximity virtualization. Headline debt: resource-accounting bugs cluster here (dead allocation clamp, proxied-bank inflation); checkpoints don't survive load; `TownPatrol` class is dead code. |
| 3 | qrf | 📄 Documented (retrospective; gained a second mode) | — (retrospective) | `OVT_QRFControllerComponent` — the only thing that resolves a battle. STANDARD mode is unchanged: 120 s countdown, waves, 10 s zone-control scoring. COUNTER_ATTACK mode (from `counter-attacks`) is the silent siege. Retrospective debt now closed: QRFs **do** debit `m_iResources` (once per pass, outside the mode branch), the LZ clear-check trace works, recruits count toward scoring, and the landing-zone bearing is derived from real source geometry. Still true: a live battle deliberately rolls back on load. |
| 4 | deployments | 📄 Documented (retrospective) — **now the epic's only force-placement system** | — (retrospective) | Modular condition/spawning/behavior framework. The migration off `base-upgrades` **completed** in `virtualization/base-defense-migration`; the framework now ships the town patrol, two vehicle patrols, the tower garrison, nine base-defence configs and (from `counter-attacks`) five objective configs, all funded from one per-faction pool. Retrospective headline debt: per-faction deployment list leaks to its 100 cap (**BUG-028**); zero replication. |
| 5 | counter-attacks | 🟡 Built 2026-08-19, Phases 1–10 (Ready for Review) | 101/111 (91%) | Replaces the retired dice-roll counter-attack with a **single current objective** and a three-phase campaign: harassment (groups inserted by live truck, a stacking town-support debuff, radio-tower recapture, base sabotage that permanently destroys player structures cheapest-first), a **forward operating base** raised unannounced between the faction's nearest holding and the target, then a **counter-QRF fought as a silent siege** (whole budget in one pass, a 100–150 m encirclement, no notification until the ring is closed, then 30 **real** minutes in which nothing is scored, and an early resistance win if the whole force is killed inside it). Daylight-only start (05:00–15:00), waves biased toward the bearing of the source that sent them, twelve new difficulty fields with the sabotage gate scaled **inverted**. Server-only bar one appended GM record. **Owed:** the Fast/All suite runs, the play-test criteria F1–F19, and a Workbench localization re-export (Phases 5–10). T8.9's `GMPanel.layout` rows are present in the working tree but their task box is still unticked. |

> Reference any feature with the slash form `occupying/[feature-name]` (e.g. `/continue-feature occupying/core`). Task counts are pulled from each feature's own `tasks.md` and refreshed by `/update-epic`.

---

## Build Order / Dependencies

Documentation (and any future enhancement work) follows the dependency chain:

1. **core** — the command layer everything else hangs off; owns base/tower records, resources and threat.
2. ~~**base-upgrades**~~ — retired in code; see the note under Purpose. Nothing depends on it any more.
3. **qrf** — resolves every battle, player-initiated or not; QRF outcomes mutate core's base/town ownership. Gained a second **mode** (the counter-attack siege) from `counter-attacks`.
4. **deployments** — the modular framework core funds, and now the epic's only force-placement system.
5. **counter-attacks** — sits on top of all three: it decides *where* and *when*, and creates every group through a deployment module. It asks the virtualization core for nothing.

**Dependencies between features:**
- core → qrf (base/town battles are started through the faction manager; QRF outcomes mutate base ownership)
- core → deployments (faction manager funds the deployment pool; registry/config decide what spawns)
- core + deployments + qrf → counter-attacks (the director reads ownership/threat from core, creates every operation through deployments, and starts the battle through qrf)
- External: core (epic) game-mode/config/persistence; town system for stability/support inputs and QRF targets.

---

## Integration & Architecture

- **Within the epic:** `OVT_OccupyingFactionManager` (singleton on the game mode) is the brain for territory, income and threat; `OVT_BaseControllerComponent` instances are its per-base hands; `OVT_ObjectiveDirectorComponent` (server-only, ticked once per in-game minute) is the **strategic** brain and the only thing in the tree that starts an offensive operation; **deployments is now the single force-placement mechanism**; QRF is the combat layer, and since `counter-attacks` it has two modes rather than one. The two global war scalars (`m_iResources`, `m_iThreat`) live in the manager and fund/gate everything else, with the deployment manager holding a separate per-faction pool topped up from the same income.
- **With other epics / features:** consumes town stability/support (town system) for decisions and applies battle/patrol modifiers back; difficulty/config from the core epic (`OVT_DifficultySettings` OF tuning block); persists through three vanilla-persistence serializers (faction manager, deployment manager, deployment component). Map/HUD UI surfaces faction state (restricted areas, threat grid, icons, QRF banner) — deployments alone have zero client-visible surface.
- **Key architectural decisions for the epic as a whole:**
  - **Server-authoritative, replay-based persistence:** AI is never persisted as entities; garrisons/patrols respawn from upgrade state or config records on load (AI self-spawn disabled in `Overthrow.conf`). Exceptions: slotted compositions and deployment markers are entity-tracked. Live QRF battles are deliberately not persisted (clean rollback).
  - **One battle at a time, but three separate questions since `counter-attacks`:** `m_CurrentQRF` is still a global singleton slot and `m_bQRFActive` still blocks a second battle from the moment one exists. What it no longer implies is suppression: `IsQRFEngaged()` (the shooting has started) gates the economy tick, the deployment evaluator and the objective town's civilians, and `m_bQRFRevealed` (the client has been told) gates the HUD, the map circle, fast travel and respawn. For a player-initiated battle all three are true at once, exactly as before; a counter-attack's silent stage sets only the first.
  - **Two threat concepts:** the global escalation scalar and the spatial `GetThreatByLocation` score share a name but never interact.
  - **Virtualization everywhere, three different ways:** upgrades bank value ("proxying"), deployments toggle by proximity around a durable marker, towers spawn/despawn garrisons ad-hoc.

---

## Tech Debt / Findings

Cross-feature tech debt and review findings. **Populated and updated by `/review-epic`** (which also writes feature-specific findings into the relevant child `context.md` files). Seeded from discovery (2026-08-02); per-feature detail lives in each child's `implementation.md`/`context.md`.

- [ ] 💳 **Unvalidated client capture RPCs** (**BUG-025**) — core + qrf — `RpcAsk_StartBaseCapture` (no proximity/faction/alive/rate checks) and `RpcAsk_InstantCaptureBase` (unauthenticated instant base flip; DiagMenu gate is client-side only) in `OVT_PlayerCommsComponent.c:105-150`. The epic's biggest multiplayer-integrity hole; same class as the economy epic's client-authority bugs.
- [x] ✅ **The resource economy's arithmetic is broken end-to-end** (**BUG-026** spend loop, **BUG-027** free QRFs, **BUG-029** upgrade bank drift) — core + base-upgrades + qrf — **CLOSED.** BUG-026/029 went with the code: `SpendResources` and the proxied upgrade banks are deleted, and the whole defence share now goes to the one deployment pool. BUG-027 is fixed in `OVT_QRFControllerComponent.SendWave`, which debits `m_iResources` by exactly what the pass spent, once, clamped at zero, on **both** QRF modes (`counter-attacks` Phase 9 kept that statement deliberately outside the mode branch). ⚠ The successor invariant now worth guarding is the conserved total across the deployment pool; `counter-attacks` asserts it at the Init tier (its G5/Q6: `grep -rn "AddFactionResources" Scripts/Game/GameMode/Objectives/` must stay empty).
- [x] ✅ **Two force-placement systems, one stalled migration** — base-upgrades + deployments — **CLOSED 2026-08-18 by `virtualization/base-defense-migration`.** The migration finished: `Scripts/Game/Components/BaseUpgrades/` is deleted, base defence is nine deployment configs, and 80 % of every resource tick is credited unconditionally to the one deployment pool. `grep -rn "SpendResources" Scripts/` returns nothing; the only `OVT_BaseUpgrade*` symbols left are legacy save-payload classes.
- [ ] 💳 **Deployment bookkeeping shipped unfinished** (**BUG-028** faction-list leak) — deployments — `m_mFactionDeployments` leaks dead IDs to its 100 cap (silently halts all deploying on long campaigns); `m_iResourcesInvested`/`m_fThreatLevel` never set; seconds-vs-milliseconds mismatches; runtime conditions only evaluated by the reinforcement module.
- [ ] 💳 **Persistence blind spots** (**BUG-030** checkpoints) — core + base-upgrades + qrf — checkpoint compositions vanish on load while their slots stay blocked; composition `Deserialize` double-buys (`m_Spawned` unset); parked vehicles re-buy over restored ones; `m_aKnownTargets`/specops assignments reset every load; an autosave mid-QRF silently discards the battle. The `OVT_OccupyingFactionManagerSerializer` round trip is entirely untested.
- [x] ✅ **QRF landing zones broken two ways** (**BUG-031**) — qrf — **CLOSED.** Both halves are fixed. The file-scope LZ cache (`Goodqrfpos`/`Goodqrfbasepos`) is gone, each wave source resolves its own landing zone, the TraceBox clear-check no longer no-ops and the preferred-direction wrap bug is fixed, all in commit `d7e42362`. The "authoring is largely cosmetic" half is closed by `counter-attacks` Phase 8: `OVT_QRFBearing` derives each wave's preferred direction from the **real geometry** of the source that sent it (`source - target`, `atan2(dx, -dz)`), and the occupying faction's forward base joins the wave-source list, so a battle now visibly comes from where the enemy actually is.
- [x] ✅ **Regression: the occupying faction had no radio-tower recapture path** — core + base-upgrades — **CLOSED by `counter-attacks`.** Opened on 2026-08-18 when `base-defense-migration` deleted `OVT_BaseUpgradeSpecops` and its 600 s recapture timer with no replacement, leaving towers flowing one way only for the whole of that window. `OVT_TowerRecaptureBehaviorDeploymentModule` restores it as a reach-and-hold deployment (`Deployment_ObjectiveTowerRecapture.conf`, hold time `objectiveTowerRecaptureHoldSeconds`), scoped to towers near the current objective and pausable by player presence. Both Field Manual pages that asserted "no way of taking it back" were corrected in Phase 10.
- [ ] 💳 **Two documented pointers in `counter-attacks` disagree with what shipped** — counter-attacks — not defects, but they will mislead the next reader. (a) The requirements say the forward base is starved by a strong resistance presence **at its source base**; the shipped `IsPlayerAtFOB()` measures presence at the **forward base itself** (`OVT_ObjectiveDirectorComponent.c:2325-2332`). (b) `context.md`'s T6.9 block quotes the sabotage operation interval as "45 in-game minutes on Normal"; `Configs/Difficulty/Difficulty_Normal.conf:10` authors `objectiveHarassmentIntervalMinutes 60` (45 is Hard). The player-facing text follows the code in both cases and quotes neither number.
- [ ] 💳 **JIP/client divergence** — core + qrf — one-shot 1 s `SetClientBaseFactions` reconcile race; `m_iCurrentQRFBase/Town` missing from the JIP payload (wrong map circles); BUG-013 (`QRFPointsToWin`/`QRFFastTravelMode` unreplicated); `OVT_TownController` reading server-only `m_CurrentQRF` on clients.
- [ ] 💳 **No victory/defeat condition** — core — nothing ends the campaign for either side; the QRF's "final base" fallback + sea/air-reinforcement To-Do is the only endgame handling that exists.
- [ ] 💳 **Test coverage is ~3 assertions for the whole epic** — all — manager/deployment-manager resolve + base registration. The Logic-tier candidates are listed per feature (income/threat math, QRF point model and wave budgeting, upgrade allocation arithmetic, deployment cost/selection, `ValidateAllConfigs()`).

---

## Master Overview Rollup

How this epic is represented in the project's master `docs/overview.md` (one row, not its children). Kept in sync by `/update-epic` and `/update-master`.

- **Rollup status:** 🟢 Active — `counter-attacks` built 2026-08-19 (Ready for Review); 4 documented retrospective, of which `base-upgrades` is now retired in code
- **One-line summary for master:** The AI occupying faction: command layer (`core`: bases/resources/threat), `qrf` battles, the modular `deployments` framework, and the new **`counter-attacks`** (built 2026-08-19, Ready for Review). Counter-attacks retires the dice-roll: the faction now holds **one objective at a time** and works toward it in three legible phases through a new server-only objective director. Phase 1 inserts groups by live truck from a controlled base to harass a town (a stacking support debuff), retake radio towers, and sabotage a base by **permanently destroying** player-built structures cheapest-first. Phase 2 raises an **unannounced forward operating base** between their nearest holding and the target, which can be starved or dismantled by hand. Phase 3 is a **silent siege**: the whole budget spent in one pass, an encirclement at 100–150 m, no notification until the ring closes, then **30 real minutes** in which nothing is scored and wiping the force ends the battle in the resistance's favour. Daylight-only (05:00–15:00), waves biased toward the bearing they actually came from, twelve new difficulty fields with the sabotage gate scaled **inverted** so easier settings give more warning. Retrospective debt: **BUG-025** (unvalidated capture RPCs) and **BUG-028** (deployment-list leak) remain open; **BUG-026/027/029/031** and the stalled base-upgrades migration are now closed.

---

*This is a required file — do not delete it; it marks the folder as an epic. Update it with `/update-epic occupying` after working on the epic's features, and run `/review-epic occupying` to refresh the Tech Debt / Findings section.*
