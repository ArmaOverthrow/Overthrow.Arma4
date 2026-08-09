# First Spawn — Requirements

**Epic:** new-player-experience
**Created:** 2026-08-04

## Overview

Fix the opening minutes: a player currently gets teleported to a random house with a car and $100 and a lone 20-second hint that repeats every session. This feature replaces that with a popup-driven welcome sequence explaining what they have and what Overthrow is, and gives the campaign-setup menu real descriptions of the choices it offers.

## Requirements

- **Welcome sequence on first spawn:** a short multi-page dismissable sequence (built on tutorial-system's sequence primitive) shown when a player first spawns into a campaign, covering: what Overthrow is (persistent revolution, no objectives — you decide), your home (assigned house, what "home" does), your car and starting cash, and where to start looking (main menu, map, nearest town). Dismissable at any point; sandbox voice throughout.
- The welcome replaces the current `#OVT-IntroHint` 20-second `ShowCustom` hint and its session-only `m_aHintedPlayers` dedup — seen-state comes from the tutorial system's per-machine store, so it no longer repeats every session.
- Fires correctly for **every** player on a server (each player's own first spawn), including JIP, and handles the fallback-spawn path (bus stop, no house available) with accurate text.
- **Start-menu polish** (`OVT_StartGameContext`): the campaign setup screen describes what the occupying-faction, supporting-faction and difficulty selections actually mean (e.g. starting cash and price effects of difficulty presets), sourced from config values where they exist rather than hardcoded numbers.
- All strings via `#OVT-` stringtable keys.
- No change to spawn mechanics themselves (home assignment, starting car/cash amounts, teleport flow) — this feature explains the flow, it does not redesign it.

## Dependencies

- `tutorial-system` must be complete (popup + sequence primitives, seen store).
- Can be built **in parallel with** `tutorial-content`.

## Out of Scope

- Changing home assignment, starting cash/difficulty values, or the spawn/respawn logic itself.
- Character creation, intro cinematics, or start-camera changes.
- A "new campaign re-shows the welcome" behavior — seen tracking is per-machine by epic-level decision; the welcome shows once per machine.
