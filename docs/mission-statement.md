# Overthrow

## Mission Statement

**Overthrow turns Arma Reforger's sandbox into a living, persistent insurgency — one where a single player or a whole server grinds an occupying army out of a country town by town, and the world remembers everything they did.**

Arma's official scenarios reset. You capture an objective, the round ends, and nothing carries. Overthrow replaces that with a campaign that never resets: towns have populations, stability and support that shift over time; the occupying faction reacts to what you actually do; money, gear, vehicles, houses and recruits persist across sessions and server restarts. It is a *platform*, not a mission — a simulation layer that runs underneath whatever the players decide to do next.

---

## Core Philosophy

### The world runs whether you're watching or not

Towns, the occupying faction, and the economy are simulated by server-side managers on their own timers. The player is a participant in a system, not the trigger for a scripted sequence. Emergent consequence beats authored beats: if you stir up a town and leave, the QRF still comes, stability still falls, and the next player to walk in inherits that.

### Persistence is the feature, not a save button

Everything meaningful — town control, base ownership, player wealth, real estate, vehicles, inventories, loadouts, recruits, skills, placed structures — is durable state. A server that has run for three months should feel like it has run for three months. This is why persistence is treated as a first-class architectural concern rather than a convenience layer bolted on at the end.

### Multiplayer-first, server-authoritative

Overthrow is designed for hosted and dedicated multiplayer with players joining and leaving continuously. The server owns the truth; clients request and receive. Any feature that can't survive a late join, a host migration, or twenty players doing unrelated things at once isn't finished. Singleplayer is the one-player case of the multiplayer design, not a separate mode.

### A mod built to be modded

Overthrow ships configuration as data (`Configs/`), not hardcoded constants — factions, pricing, jobs, difficulty, modifiers and town behaviour are all `.conf` files. Community servers should be able to retune the campaign without touching EnforceScript, and other modders should be able to extend it through the same component patterns the core uses.

---

## What We're Building

### A persistent revolution simulation layered over Arma Reforger

At its core Overthrow is a set of server-side **managers** (economy, towns, jobs, players, real estate, vehicles, inventory, recruits, skills, loadouts, persistence) attached to the game mode, plus per-entity **controllers** (towns, bases, ports, QRF, towers, FOBs) that own the behaviour of individual places on the map. Around those sit an occupying faction that garrisons and reinforces, a resistance faction the players build up, a civilian economy with prices and shops, a job and modifier system that gives towns evolving character, and a UI layer for the map, HUD, menus and player interactions.

Players start with nothing in an occupied country. They earn money, buy gear and property, recruit and equip fighters, build FOBs, take towns, and push a real front line — and the campaign state survives the session.

### The target outcome

A server you can leave running for months, that a community can build a shared story on, and that gives a solo player a full campaign without a scripted mission in sight.

---

## Who This Is For

### Community server operators

Groups running a persistent Arma Reforger server who want a long-running campaign their members can drop into at any hour and find the world exactly as the last shift left it. They need it to survive restarts, handle continuous join/leave, and be tunable to their group's taste.

### Milsim and co-op groups

Squads who want an open-ended operational sandbox rather than a linear mission — somewhere to run their own planning, logistics and tactics against a reactive opponent, with consequences that carry between sessions.

### Solo and small-group players

Players who want a full singleplayer/small-co-op campaign in Reforger: start broke, build an insurgency, take the map. The same persistence that serves a 40-player server serves one person over many evenings.

### Modders and contributors

Developers extending Overthrow with new factions, jobs, town behaviours or systems — served by the configuration-as-data approach, the documented component patterns, and an open MIT-licensed codebase.

---

## What Overthrow Is Not

- **Not a mission or a scenario.** There is no objective list and no ending cutscene. It is a simulation platform that generates situations.
- **Not a round-based game mode.** Nothing resets between sessions. If you want a clean slate you start a new save deliberately.
- **Not singleplayer-first.** Every system is designed against the multiplayer, server-authoritative, late-join case. Features that only work locally are incomplete features.
- **Not a hardcoded ruleset.** Prices, factions, difficulty, jobs and town behaviour live in config files precisely so servers can disagree with our defaults.
- **Not a total conversion.** Overthrow builds on Arma Reforger's vanilla assets, factions and mechanics rather than replacing them.

---

## Design Pillars

1. **Persistence over convenience** — if state matters to the player, it survives the restart. We accept extra architectural cost to make that true.

2. **Server authority, always** — the server drives state and clients receive it. Client-side prediction is a presentation detail, never the source of truth.

3. **Emergence over scripting** — systems that interact beat sequences that fire. We'd rather add a rule that produces a hundred situations than author ten of them.

4. **Configuration over code** — tuning knobs belong in `Configs/`, not in EnforceScript constants. Server owners shouldn't need a compiler to change their campaign.

5. **Late join is not an edge case** — a player arriving mid-session must see a correct world. JIP handling is part of the feature, not a follow-up ticket.

6. **Vanilla-compatible** — prefer the engine's own systems and Reforger's own content over bespoke reimplementation, so the mod keeps working as the game evolves.

---

## Technical Direction

### Enfusion's entity-component architecture, used as intended

Overthrow is written in EnforceScript against the Enfusion engine and follows its component model rather than fighting it: singleton **Manager** components on the game mode for whole systems, **Controller** components on individual entities for local behaviour, and `OVT_Global` as the single static accessor tying them together. This keeps the mod legible to anyone who already knows Reforger modding.

### Native persistence, moving off third-party frameworks

Persistence has been built on the Enfusion Persistence Framework (EPF). With Reforger shipping a first-party persistence system, the project is migrating to it — for native performance, less custom serialization, and console support without the `#ifdef PLATFORM_CONSOLE` carve-outs EPF requires. See `docs/features/vanilla-persistence/`.

### Replication discipline

Simple state replicates via `RplProp`; complex operations go through explicit RPCs (`RpcAsk` client→server, `RpcDo` server→client); late-join state is restored through `RplSave`/`RplLoad`. Network identity is always `RplId`, never `EntityID`. These rules are non-negotiable because violations of them fail silently and only in multiplayer.

### Automating the quality gate

For most of this project's life the only quality gate was a human: build in the Workbench, host a session, join a second client, play the change. That made every edit expensive and is why the codebase has never had a test suite.

Reforger 1.7.0 changed the ground. The game ships a script test framework with JUnit output, and the Workbench exposes command-line automation — so compilation and logic can now be verified without a person in the loop. The project is building that pipeline (see the `dev-ops` epic), on the principle that **automation should cover what is mechanically checkable so human testing is spent on what isn't.** Compile correctness, campaign logic and persistence round-trips are machine work. Feel, balance, and whether an emergent situation is actually fun remain human work, and always will.

The first stage of that pipeline landed 2026-08-01: `tools/compile-check.sh` verifies compilation automatically, so compile correctness is now machine work. Until the autotest features land, the runtime discipline is unchanged: conservative patterns, explicit documentation, and specific reproducible test steps attached to every change — verified by play-testing.

---

*This document is a living guide. It will evolve as the project develops, but the core mission does not change.*
