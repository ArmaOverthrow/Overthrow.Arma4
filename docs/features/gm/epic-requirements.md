# Gm — Epic Requirements

This epic will manage improvements to the Game Master (GM) UI for Overthrow to allow server owners to manage the campaign, debug and fix problems, inspect the state of the campaign and perform some actions on behalf of the Occupying Faction to improve the gameplay for their server members.

We move this epic in 3 phases, these phases may be deployed separately, but at least phase 1 will be shipped with v1.5.0 alongside the virtualization and mission epics. When in /plan-epic just scaffold Phase 1 for now, the other phases can just get a mention in the overview and requirements.

Phase 1: Campaign state
We add tools to see whats happening right now in the campaign, without being able to change anything. 

Overthrow Panel:
A new panel above the settings panel bottom left shows Overthrow campaign-wide information such as:
- Current threat levels
- OF Resources
- Next resource distribution and amounts (with coiuntdown)
- Next resistance payout and amount (with countdown)
- Resistance Funds
- Any other information that makes sense
- Any Overthrow information on selected HUD icons below (if they cant be shown on the HUD itself)

The panel should have the Overthrow logo at the top

HUD Icons:
The GM view already shows icons in the HUD above towns and other locations, we leverage this to add icons and change existing ones for anything we can thats overthrow related. 

- Clicking a town shows the support/stability/population details etc
- Icons at a base show any resource information, total garrison etc
- Existing group icons show what base/town the group came from and their reason for existing (deployment type, base upgrade type, etc)
- Clicking a player shows their money, level, etc

Waypoints:
We dont use the E_ waypoint set atm which draws waypoints in GM and makes them editable. We had issues with it but it doesnt matter anyway as it's best if we implement our own version. Overthrow-generated waypoints can just be visualized so the GM can see where a group was told to go. If they want to override the group's waypoints they can just assign them a new one using the base game waypoints anyway and that gives them way more control than we could give them.

The Map (OverthrowMap_GM.conf):
Rewrite the old threat grid layer to use our new canvas drawing capabilities and be performant (see map/territory-overlay for details). enable it for GM conf (not main conf)

Add icon layer showing all current deployments (min zoom 0 so they always show) and icon layers showing base upgrades (when zoomed in at about shop level). Hovering them shows info panel with type, Use the base-game group icons where possible to leverage things such as NATO type indicators

Extend base info panel (for GM only) to show resource information, garrison, etc

Extend all existing info panels (shops, towns, etc) to add a "Move Camera Here" action for GMs

Phase 2 - Overthrow campaign management:
This future phase will add the ability to change Overthrow-specific things. Give the OF resources, give/take resistance funds, give/take money to/from individual players, Give XP to individual players, make players an officer, that kind of thing. This is done via popup menu actions on the HUD icons created/extended in phase 1. Ability to add specific base upgrades to a base or spawn deployments is also made possible by this phase

Phase 3 - Base Game GM cleanup
Remove any base game actions that are incompatible with overthrow and alter others to add Overthrow support. Will require a user audit  in GM before this is scoped properly. Once this phase is done we can remove the "game master is not configured with this game mode" message shown currently

## Phase 1 Feature Decomposition (agreed 2026-08-14)

Phase 1 is scaffolded as five features, in build order:

1. **gm-state** — shared read-only replication seam streaming campaign state (threat, OF resources, distribution/payout countdowns, funds, per-entity detail) to authorized GM clients. Foundational: all other features consume it; Phase 2 adds its write-side here later.
2. **overthrow-panel** — the bottom-left GM panel; smallest vertical slice, proves gm-state end-to-end.
3. **hud-icons** — GM-view HUD icon extensions for towns/bases/groups/players; riskiest base-game integration, includes waypoint-adjacent group detail (origin, purpose).
4. **waypoint-viz** — own small feature: read-only visualization of Overthrow-generated waypoints (our own implementation, not E_). Parallel-safe with hud-icons.
5. **gm-map** — one feature covering all `MapOverthrow_GM.conf` work: canvas threat grid (picks up territory-overlay's deferred D3), deployment + base-upgrade icon layers, GM-only info panel extensions, "Move Camera Here".

Epic-level constraints: Phase 1 is strictly read-only; all GM data replication is gated to authorized GM clients (no leaking player money/OF internals to regular clients). Note the GM map conf's real filename is `Configs/Map/MapOverthrow_GM.conf`.
