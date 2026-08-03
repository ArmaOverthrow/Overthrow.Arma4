# Resistance - Epic Requirements

**Created:** 2026-08-02
**Phase:** Retrospective documentation (backfill via `/discover-feature`)

> Epic-level requirements — the higher-level scope for the whole epic, mirroring a feature's `requirements.md` but one level up. This epic was created by backfilling documentation for systems that already ship; the "requirements" below describe what the shipped systems are expected to do.

## Overview

The resistance epic owns the player faction's toolset: the resistance faction manager (faction choice, camps, FOBs, garrisons, officers), base building/placement, recruiting AI squadmates, and equipment loadouts. These belong together because they are the systems a player uses to build and command the resistance movement that fights the occupying faction.

## Requirements

- Players can establish and manage camps (personal spawn/stash points) and FOBs (shared forward bases), including deploying/undeploying mobile FOBs.
- Officers (designated players) can place structures, manage bases and set faction-level priorities.
- Players can place and build structures at camps/FOBs/bases via the place/build UI, bounded by item limits.
- Players can recruit civilians (and recruit from tents) into persistent AI squadmates and manage them.
- Players can save and load equipment loadouts, including officer squad loadouts for recruits.
- All of the above persists across sessions via the vanilla persistence serializers and replicates correctly to clients/JIP.

## Planned Features

Documented retrospectively, in dependency order:

1. **core** — `OVT_ResistanceFactionManager`: player faction, camps, FOBs, garrisons, officers — the command layer the rest hangs off.
2. **building** — placement & construction: `OVT_PlaceContext`/`OVT_BuildContext`, placeable/buildable components, handlers, item limits.
3. **recruits** — `OVT_RecruitManagerComponent`: recruiting, persistence and management of AI squadmates.
4. **loadouts** — `OVT_LoadoutManagerComponent`: player + officer loadout save/load.

## Dependencies

- core (epic): game-mode lifecycle, config, player-manager, persistence.
- economy (epic): money for placeables/buildables pricing; inventory for loadouts.
- occupying (epic): the adversary — QRF reacts to resistance base capture; threat reacts to resistance activity.
- skills (epic): skill effects touch recruit/loadout behavior (e.g. supporter recruitment).

## Out of Scope

- Town support/stability and supporter counts — the town system (undocumented, future epic).
- Vehicle ownership/management (`OVT_VehicleManagerComponent`) — economy-adjacent, not part of this epic.
- Jobs (`OVT_JobManagerComponent`) — separate system.
- The occupying faction's own bases/QRF/deployments — covered by the `occupying` epic.

---

*Backfilled epic — features documented via `/discover-feature resistance/<feature>` on 2026-08-02.*
