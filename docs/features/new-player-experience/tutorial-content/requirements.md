# Tutorial Content — Requirements

**Epic:** new-player-experience
**Created:** 2026-08-04

## Overview

The authored tutorial entries that make the framework worth having: early-game and mid-game coverage, each entry triggered by the player's own actions and written in the sandbox voice — explain the system just touched, hint at what it enables, assign nothing. This is where the epic's player-facing value lands.

## Requirements

- **Early-game coverage (the first hour):**
  - Home & starting situation (first time opening the map or main menu after spawn — complements first-spawn's welcome)
  - Money & the economy (first buy or sell; first shop visit)
  - Shops & gun dealers (first time near/inside each shop type)
  - Map info & fast travel (first map open; first fast-travel availability)
  - The wanted system (first time gaining a wanted level — needs the new invoker from tutorial-system; also disguise/undercover basics)
  - Skills & levelling (first XP gain or first level-up)
- **Mid-game coverage (first escalation):**
  - Recruiting civilians (first recruit gained, and/or first time the option is available)
  - Camps & placing (first placeable placed; first camp)
  - Base capture (first base capture started or completed; what control changes mean)
  - FOB basics & building (first FOB deployed; first build)
- Every entry: trigger binding + `#OVT-` localized title/body + "Learn more" link to its `field-manual` entry where one exists.
- **Tone rules (hard requirement):** entries inform, never instruct; no imperative goals ("go do X"), no implied sequence ("now that you've done X, do Y" is acceptable only as possibility, not direction); each entry reads correctly regardless of what the player did before.
- Trigger choices must fire for **each player individually** in multiplayer (the failure mode that killed the starter jobs).
- Entry set and trigger bindings reviewed against actual new-player bounce points; keep total volume restrained — a popup per minute is worse than no popups.
- Where a trigger the content needs doesn't exist and wasn't added by tutorial-system, the gap goes back to tutorial-system rather than being hacked in here.

## Dependencies

- `tutorial-system` must be complete (framework, popup UI, trigger registry, seen store).
- `field-manual` entry ids must be frozen for "Learn more" links.
- Can be built **in parallel with** `first-spawn`.

## Out of Scope

- Late-game entries (warehouse/port/import, real estate, loadouts, vehicle upgrades, donations) — deferred content pass.
- The first-spawn welcome sequence itself (→ `first-spawn`).
- Framework changes — any needed capability is a `tutorial-system` change.
- Removing the starter jobs (→ `starter-jobs-retirement`), though this feature must consciously cover everything those jobs taught: gun dealers, shops, placing equipment, recruiting, camps.
