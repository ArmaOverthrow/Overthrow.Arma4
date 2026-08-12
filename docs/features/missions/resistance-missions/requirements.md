# Resistance Missions — Requirements

**Epic:** missions
**Created:** 2026-08-13

## Overview

Officer-created missions that feed directly into the mission list alongside spawned ones: simple, template-driven tasks ("clear this area", "put posters up here") funded from Resistance Funds via escrow, assignable to specific groups or open to anyone. Uses the same module system as spawned missions, so server owners can still author complex ones in Workbench.

## Requirements

- Officers can create missions from pre-authored config templates (location + reward parameters); creation UI is simple — this is not a mission editor.
- **Escrow:** the completion reward is taken from Resistance Funds (`DoTakeResistanceMoney`) at creation and held by the missions manager; refunded on cancel. Escrowed balances persist in the missions serializer (economy serializer untouched).
- **Per-kill rewards** (e.g. "clear this area"): the officer sets an optional per-kill amount paid from Resistance Funds on each qualifying kill. If funds are unavailable the kill simply doesn't pay, and both the participant and the officers are notified so they can rectify manually. Guard every payout — `DoTakeResistanceMoney` does not clamp at zero.
- **Assignment:** a mission can be assigned to a specific group (max 1 active assigned mission per group) or placed on the open mission list. Group identity = the leader's persistent ID (vanilla group IDs are runtime-scoped and never persisted).
- **No XP on top:** resistance missions award no mission XP (exploit vector) — participants still earn normal gameplay XP (kills etc.), which needs no suppression.
- **MP-only:** creation is gated on being an officer AND `RplSession.Mode()` indicating a hosted/dedicated MP session (SP/listen player 1 is always auto-officer, so `IsOfficer` alone is insufficient).
- Officer operations (create/fund/assign/cancel) go on a new `OVT_MissionAuthoringComponent` on `OVT_OverthrowController`, so the officer gate is one component-wide precondition.
- Created missions replicate, persist, and appear in the mission list exactly like spawned missions.

## Dependencies

- `missions/framework` (module system, escrow-capable persistence, participant/reward model), `missions/mission-ui` (list integration, creation UI surface), and ideally `missions/mvp-missions` stability before release.

## Out of Scope

- A full in-game mission editor — templates only.
- Officer demotion / permission tiers beyond the existing one-way `isOfficer` flag.
- Escrow/ledger concepts inside `OVT_EconomyManagerComponent` — escrow state lives in the missions manager.
