# Occupying - Epic Requirements

**Created:** 2026-08-02
**Phase:** Retrospective documentation (backfill of existing, shipped systems)

> Epic-level requirements — the higher-level scope for the whole epic, mirroring a feature's `requirements.md` but one level up. `/plan-epic occupying` reads this file (if it exists) to drive epic scoping; otherwise it uses the prompt. Each **child feature** below gets its own `requirements.md` (in the standard Overview / Requirements / Dependencies / Out of Scope shape) that `/plan-feature occupying/[feature-name]` consumes.

## Overview

The occupying epic owns the AI antagonist of Overthrow: the occupying faction that holds the island at campaign start and that the player resistance fights to overthrow. It covers the faction's command layer (bases, radio towers, resources and threat), how it garrisons and upgrades its bases, how it reacts to attacks with Quick Reaction Forces, and the modular deployment framework that spawns and manages its AI in the world. These features belong together because they form one adversarial system: the core manager decides *what* the faction does, base upgrades and deployments decide *how force is placed in the world*, and QRF decides *how it fights back*.

## Requirements

- The occupying faction must hold bases and radio towers at campaign start, garrison them, and visibly project force (patrols, checkpoints, defenses) into the world.
- The faction must operate on a resource/threat economy: it accumulates resources over time, spends them on garrisons/upgrades/deployments, and escalates in response to resistance activity.
- The faction must contest territory: respond to base capture attempts and town instability with QRFs, and attempt to retake lost assets.
- All faction state (bases, resources, threat, active deployments) must persist across sessions via the vanilla persistence layer and replicate correctly to clients, including JIP.
- The system must scale AI presence to player proximity (virtualization) so a whole-island faction is affordable at runtime.

## Planned Features

The features that make up this epic. All four already exist in code; they are documented retrospectively in **discovery order**, which follows the dependency chain:

1. **core** — `OVT_OccupyingFactionManager` + `OVT_BaseControllerComponent`: bases, radio towers, resources, threat, counter-attack orchestration — the command layer every other feature hangs off.
2. **base-upgrades** — the resource-spending upgrade classes that garrison and fortify bases (patrols, checkpoints, compositions, defenses, parked vehicles, specops, tower guards).
3. **qrf** — `OVT_QRFControllerComponent` and its triggering: contested base/town battles, the point model, and battle resolution.
4. **deployments** — the modular condition/spawning/behavior deployment framework (`GameMode/Deployments/`), the newer system for placing faction AI in the world.

## Dependencies

- **core (epic)** — game-mode bootstrap (`OVT_OverthrowGameMode`, `OVT_Global`), config (`OVT_OverthrowConfigComponent`, difficulty settings), and the vanilla persistence layer (serializers for the faction manager and deployments).
- **Town system** (`OVT_TownManagerComponent`, `OVT_TownController`) — towns supply stability/support state that drives faction decisions and QRF targets.

## Out of Scope

- The resistance faction (`OVT_ResistanceFactionManager`, `Controllers/ResistanceFaction/`, FOB deploy/undeploy actions) — the player side is a separate concern.
- Town population/stability/support simulation itself (lives with the town system; this epic only consumes it).
- Economy/shops (economy epic), even where the occupying faction affects prices or map info UI.

---

*Consumed by `/plan-epic occupying`. After planning, run `/plan-feature occupying/[feature-name]` per feature in the recommended order.*
