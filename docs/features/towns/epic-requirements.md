# Towns - Epic Requirements

**Created:** 2026-08-02
**Phase:** Retrospective documentation (backfill of shipped systems)

> Epic-level requirements — the higher-level scope for the whole epic, mirroring a feature's `requirements.md` but one level up. `/plan-epic towns` reads this file (if it exists) to drive epic scoping; otherwise it uses the prompt. Each **child feature** below gets its own `requirements.md` (in the standard Overview / Requirements / Dependencies / Out of Scope shape) that `/plan-feature towns/<feature>` consumes.

## Overview

The town system — the civilian heart of Overthrow's campaign. Towns have populations, stability and support values that shift over time and in response to everything the player and the occupying faction do; nearly every other gameplay domain (economy, jobs, occupying faction, resistance, skills) reads or writes town state. This epic owns the town manager and controllers, the stability and support modifier systems, gun dealers, and the map's town-information UI. It was pre-declared by the core epic ("gameplay domains — **towns**, jobs, economy … will get their own epics later") and backfilled with retrospective docs via `/discover-feature` on 2026-08-02.

## Requirements

- Towns are discovered from the world, tracked with population/stability/support/control state, and that state survives save/load and reaches all clients (including JIP).
- Stability and support change through a composable modifier system that other domains (economy, occupying faction, resistance, skills) can trigger without touching town internals.
- Each town can host a gun dealer that sells resistance equipment and feeds the early-game onboarding loop.
- The map presents town state (control, stability, support, population) and related overlays to players.

## Planned Features

The features that make up this epic, in intended **build order** (retrospective — all already shipped).

1. **core** — `OVT_TownManagerComponent` + `OVT_TownController`: town discovery, town data, population, queries, replication and persistence — the foundation every sibling reads.
2. **stability** — the shared `OVT_TownModifierSystem` framework plus the stability modifier system and its six shipped modifiers.
3. **support** — the support modifier system and its five modifiers, supporter conversion, placeable support handlers and the Diplomacy skill effect.
4. **gun-dealers** — per-town gun dealer spawning, config, shop wiring and user action.
5. **map-info** — the map UI: town info panel, icons, threat grid, restricted areas, player location canvas layers.

## Dependencies

- **core (epic)** — game-mode/manager lifecycle, config, persistence serializers (`OVT_TownManagerSerializer` rides `core/persistence`'s vanilla SaveGame wiring).
- **economy (epic)** — shop registry (gun dealers are shops), transaction events feeding stability/support modifiers.
- **occupying (epic)** — control flips, deaths and patrols feeding modifiers; QRF town battles; map overlays (threat, restricted areas).

## Out of Scope

- The occupying faction's decision-making that *consumes* town state (occupying epic).
- Shop/economy mechanics beyond the gun-dealer wiring itself (economy epic).
- Job conditions/stages that read town state (jobs epic).
- Recruitment/supporter draw-down mechanics on the resistance side (resistance epic).

---

*Consumed by `/plan-epic towns`. After planning, run `/plan-feature towns/<feature>` per feature in the recommended order.*
