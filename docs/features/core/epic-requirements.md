# Core - Epic Requirements

**Created:** 2026-08-02
**Phase:** Paused (persistence) / discovery backfill in progress

> Epic-level requirements — the higher-level scope for the whole epic, mirroring a feature's `requirements.md` but one level up. `/plan-epic core` reads this file (if it exists) to drive epic scoping; otherwise it uses the prompt. Each **child feature** below gets its own `requirements.md` (in the standard Overview / Requirements / Dependencies / Out of Scope shape) that `/plan-feature core/[feature-name]` consumes.

## Overview

The `core` epic owns Overthrow's very core systems: the game-mode/manager lifecycle, global access (`OVT_Global`), the client→server controller seam, configuration, player identity, and persistence. These belong together because everything else in the mod is built on them. Gameplay domains (towns, jobs, economy, occupying faction, etc.) are explicitly deferred to their own later epics.

## Requirements

- The persistence layer migrates from EPF to Reforger's vanilla persistence system (`core/persistence`, currently paused; acceptance gate: `OVT_TEST_PersistenceRoundTripSuite` exit 1 → 0).
- The legacy core systems (game mode/manager lifecycle, config, player identity) are documented via `/discover-feature` so future work on them starts from accurate docs rather than code archaeology.
- Core-system docs stay behaviour-accurate: what is covered by the automated test spine (Logic/Init/Campaign/Persistence tiers) and what still requires manual play-testing (JIP/MP, UI, save/reload) is stated explicitly.
- Player→server communication moves off the `OVT_PlayerCommsComponent` monolith onto domain components on `OVT_OverthrowController`, with server-side validation on every migrated RPC and the monolith deleted at the end (`core/controller-migration`).

## Planned Features

The features that make up this epic, in intended **build order**. Discovery features document existing code; `persistence` is in-flight work.

1. **game-mode** — Game mode, manager lifecycle, `OVT_Global`, `OVT_OverthrowController` — foundational; everything registers through it. *(discovery backfill)*
2. **config** — `OVT_OverthrowConfigComponent`, difficulty settings, faction/config registries. *(discovery backfill)*
3. **player-manager** — Persistent player identity and player data lifecycle. *(discovery backfill)*
4. **persistence** — EPF → vanilla persistence migration (paused; resumes after the dev-ops epic, validated by its quarantined round-trip gate). *(migrated from `vanilla-persistence`)*
5. **controller-migration** — Retire the 60-RPC `OVT_PlayerCommsComponent` monolith onto domain components on `OVT_OverthrowController` (the engine-intended per-player client→server seam), with server-side validation landing alongside each migrated RPC — after the 1.4.x patch cycle settles. *(planned 2026-08-03)*

## Dependencies

- **dev-ops epic** — supersedes `core/persistence` in priority; supplies the compile check, test harness and the persistence acceptance gate it resumes against.
- Reforger 1.7.0+ (vanilla persistence API, autotest framework).

## Out of Scope

- Gameplay domain systems — towns, jobs, economy, occupying faction, real estate, shops — each gets its own epic later.
- Completing the persistence migration under this epic's discovery work (it resumes as its own feature work).
- UI, JIP/multiplayer behaviour changes — discovery documents them but changes are separate features.

---

*Consumed by `/plan-epic core`. After planning, run `/plan-feature core/[feature-name]` per feature in the recommended order.*
