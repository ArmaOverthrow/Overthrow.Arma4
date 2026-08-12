# Recruit Missions — Requirements

**Epic:** missions
**Created:** 2026-08-13

## Overview

Post-MVP: missions that assign temporary or permanent recruits to a player — e.g. rescue missions where you pick someone up, they join your group as a recruit (orders, vehicles, etc.), and depending on the mission definition they either leave once delivered to a destination or stay as a fully-fledged member.

## Requirements

- A mission module can spawn/designate an NPC who joins a participant's group as a recruit under `OVT_RecruitManagerComponent`.
- Config decides the outcome: **temporary** (leaves the group when delivered to a defined location — the delivery is itself a mission objective) or **permanent** (becomes a normal recruit).
- Temporary mission recruits must be safe across save/load and participant disconnect (consistent with the recruit reservation model from BUG-130/131) — no orphaned or duplicated recruits.
- Works in co-op: the mission defines which participant receives the recruit (distribution policies from the framework, e.g. closest/triggerer).

## Dependencies

- `missions/framework` complete; integrates with the existing `OVT_RecruitManagerComponent`.
- Can be built in parallel with `missions/dialog` and `missions/authoring-tools`.

## Out of Scope

- Changes to the recruit system itself (limits, XP, loadouts) — only the mission-side granting/return flow.
- Escorted-NPC AI behaviours beyond what the recruit system already provides.
