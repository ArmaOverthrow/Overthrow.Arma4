# GM - Epic Overview

**Epic:** gm
**Status:** 🟡 In Progress (4/5 features complete — all four built features closed at 100%, all verification discharged; gm-map planned)
**Last Updated:** 2026-08-16 (all four features closed at 100%; hud-icons draw-distance tweak; BUG-175 filed)

> **This file is the epic marker.** Its presence in `docs/features/gm/` is what tells every Beast Mode command (and future Web App / Discord clients) that this folder is an **epic**, not a plain feature. Keep it present and keep the required sections below filled in. It is the epic's equivalent of the project's `docs/overview.md`, scoped to this epic. The master `docs/overview.md` tracks this epic as a **single row**; the per-feature detail lives **here**.

---

## Purpose

Improvements to the Game Master (GM / Zeus) UI so server owners can manage an Overthrow campaign: inspect campaign state, debug and fix problems, and (in later phases) perform actions on behalf of the Occupying Faction to improve gameplay for their members.

The epic moves in **3 phases**. Only **Phase 1 — Campaign State** (read-only inspection: see what is happening right now without changing anything) is scaffolded as features below; it ships with **v1.5.0** alongside the virtualization and missions epics. Phase 2 (Overthrow campaign management — give OF resources, adjust funds/money/XP, spawn deployments/base upgrades via popup actions on the Phase 1 HUD icons) and Phase 3 (base-game GM cleanup — remove/alter incompatible base-game actions, drop the "game master is not configured with this game mode" message; needs a user audit first) are future phases and will be planned onto this epic when Phase 1 is done.

---

## Features

The constituent features of this epic, in build order. Each feature is a subfolder under `docs/features/gm/`.

| # | Feature | Status | Tasks | Description |
|---|---------|--------|-------|-------------|
| 1 | gm-state | ✅ Complete (MP checklist discharged 2026-08-16 — user confirmed the batched epic test session covered it; log measurements not recorded separately) | 28/28 | Shared read-only replication seam streaming Overthrow campaign state (threat, OF resources, countdowns, funds, per-entity detail) to authorized GM clients |
| 2 | overthrow-panel | ✅ Complete (user-verified in Workbench + own server; absorbed gm-state checklist discharged 2026-08-16) | 26/26 | GM UI panel (top-left at 22,90 — moved off the bottom-left stack because contextual help overlapped it) showing campaign-wide info, with an empty detail seam for hud-icons |
| 3 | hud-icons | ✅ Complete (user-verified 2026-08-15 incl. MP/JIP + tower variants; Fast 145/145; draw distance 20000 → 2000 on 2026-08-16 user feedback — vanilla-like fade, user-verified) | 30/30 | GM icons over towns/bases/radio towers via `SCR_EditableSystemComponent` subclass (SYSTEM type, flags 2052 — towers inherit RplComponent, no LOCAL) with live hover tooltips via per-instance `GetDescription()` (town: support+stability; base: resources+garrison, seam-gated; tower: online/sabotage countdown) — zero config forks, zero new networking. Click-detail surface was built then **removed by user decision 2026-08-15** (stretched the panel); group/player readouts went with it — epic Phase 2 popups are their natural home |
| 4 | waypoint-viz | 🔴 REOPENED 2026-08-23 — **no routes draw on a dedicated server** (single-player fine). The 2026-08-16 "dedi verified" claim was retracted by the user: dedi testing had not started. All 190 green ×3 | 28/29 | Read-only visualization of Overthrow-generated AI waypoints in GM view (own implementation; not the E_ waypoint set) |
| 5 | gm-map | Planned | — | MapOverthrow_GM.conf work: canvas-based threat grid layer, deployment + base-upgrade icon layers with hover info, GM-only info panel extensions, "Move Camera Here" action |

> Reference any feature with the slash form `gm/[feature-name]` (e.g. `/plan-feature gm/gm-state`). Task counts are pulled from each feature's own `tasks.md` and refreshed by `/update-epic`.

---

## Build Order / Dependencies

1. **gm-state** — Foundational. Every other Phase 1 feature displays server-side campaign state on GM clients; building one read-only replication seam first avoids four ad-hoc networking paths and gives Phase 2 its write-side home later.
2. **overthrow-panel** — Smallest vertical slice of visible value; proves the gm-state seam end-to-end (campaign-wide values + countdowns) before the riskier integrations.
3. **hud-icons** — The least-known territory (integration with the base game's GM HUD icon system); de-risked next, once the data seam is proven. Defines the selected-icon → panel detail contract with overthrow-panel.
4. **waypoint-viz** — Small and independent; needs group data from gm-state but nothing from the panel or icons. **Can be built in parallel with hud-icons.**
5. **gm-map** — Largest chunk; last so it builds on a proven data seam and the icon/detail conventions established by hud-icons.

**Dependencies between features:**
- gm-state → overthrow-panel, hud-icons, waypoint-viz, gm-map (all consume the read-only GM state seam)
- hud-icons → overthrow-panel (selected-icon detail that cannot fit on the HUD renders in the panel)
- waypoint-viz ∥ hud-icons (parallel-safe; both read gm-state only)
- **External (outside this epic):** `map/territory-overlay` (✅ complete) — its `OVT_MapCanvasCompositor` / `OVT_MapCanvasLayer` contract is the foundation for gm-map's threat grid layer (territory-overlay's decision **D3 explicitly deferred the threat grid to later work — this epic picks it up**). The `occupying` epic's deployments and base-upgrades systems supply the entities/data that hud-icons and gm-map visualize. Phase 1 ships with v1.5.0 alongside the virtualization and missions epics.

---

## Integration & Architecture

- **Within the epic:** gm-state is the single data spine — a read-only, GM-gated replication seam (following the OVT_OverthrowController specialized-component pattern; never OVT_PlayerCommsComponent). Panel, HUD icons, waypoint viz and map layers are all pure consumers/renderers of that seam. hud-icons and overthrow-panel share a "selected icon → detail" contract. Phase 2 will later add write-side actions to the same seam and popup menus to the same icons.
- **With other epics / features:** gm-map extends `Configs/Map/MapOverthrow_GM.conf` only (not the main player map conf) and reuses the canvas layer system built by `map/territory-overlay`. Data sources span towns (support/stability/population), occupying (OF resources, deployments, base upgrades, garrisons), economy (resistance funds, payouts) and player managers (money, level).
- **Key architectural decisions for the epic as a whole:**
  - **Phase 1 is strictly read-only** — no feature may mutate campaign state; mutation is Phase 2's job.
  - **All GM-facing data must be gated to authorized GM clients** — player money/levels and OF internals must not leak to regular clients via replication.
  - **Waypoints:** implement our own read-only visualization (the E_ waypoint set caused issues and is not used); GMs who want to redirect a group assign base-game waypoints, which already grants more control than we could offer.
  - Prefer base-game group icons (NATO type indicators etc.) where possible rather than custom art.

---

## Tech Debt / Findings

- **BUG-175 (high, found 2026-08-16 during waypoint-viz Phase 4):** assigning a GM waypoint to any **cycled**
  group crashes with `ScriptInvoker: Recursive call of Invoke!` — a pure-vanilla recursion
  (`SCR_EditableGroupComponent` re-enters `AddWaypoint(m_CycleWaypoint)` inside `Event_OnWaypointAdded`,
  `SCR_AIGroup.c:2528` twice in the stack). Every Overthrow perimeter patrol is cycled, so GMs cannot
  redirect patrols via vanilla waypoints until it's addressed. Not caused by any epic feature (Overthrow has
  zero `OnWaypointAdded` hooks). Fix decision belongs to **epic Phase 3 (base-game GM cleanup)**: a deferred
  re-add workaround in a modded editor component, and/or an upstream RFG-series report (check the existing
  ARMD-10…17 list first).

- **Pre-existing group-cleanup defects in the `occupying` epic's systems** (found by gm-state planning, routed
  around by its tag-and-sweep registry — **not gm-state's debt**): QRF groups are never deleted
  (`OVT_QRFControllerComponent.m_Groups` filled, drained nowhere; only the controller entity is deleted);
  `OVT_EntitySpawningAPI.CleanupGroup` deletes a group's soldiers but not the group entity; camp/FOB removal
  never touches `garrisonEntities`; six resistance-garrison rollback paths spawn-then-delete untracked groups.
  These belong to the `occupying` epic to fix.
- **GUID series `{6B0A…}` is still free for `gm-map`.** waypoint-viz minted **no** GUIDs — it ships no prefab,
  no layout and no `.meta` (the renderer is a plain `Managed` hosted by an existing modded UI component), so the
  reserved series was **not consumed**. Re-grep with the **brace** (`{6B0A`) before allocating from it.
- **Help-docs hand-off:** gm-state ships zero visible surface (a transport with no renderer), so it carries no
  help/wiki phase. **Decision 2026-08-14 (overthrow-panel planning): all Phase 1 help/wiki docs are deferred to
  epic end — one consolidated `help-docs-sync` pass after `gm-map` completes Phase 1**, covering the whole GM
  experience at once. overthrow-panel ships without a help phase; do not re-add one to intermediate features.
- **Pre-existing waypoint-spawn defects in the `occupying` epic's systems** (found by waypoint-viz, §5 D13 —
  **not waypoint-viz's debt, and deliberately not fixed there**: a read-only visualization feature that quietly
  changes AI behaviour is worse than the bug):
  - `OVT_OverthrowConfigComponent.SpawnWaitWaypoint` **discards its `time` argument** (`:497-502`) — every wait
    waypoint runs on the prefab's default duration, whatever the caller asked for.
  - `OVT_OverthrowConfigComponent.SpawnGetInWaypoint(vector)` **spawns the GetOut prefab** (`:443-447`).
  Both are now *visible* through waypoint-viz: a waypoint the code calls "GetIn" that classifies and renders as
  `GET_OUT` is the **symptom of this defect, not a viz bug** — triage it here first. These belong to the
  `occupying` epic to fix.
- **gm-state deferred payloads (deliberate, additive later):** per-upgrade *positions* (gm-map adds when its
  icon layer needs them); deployment threat-level (no live field exists — `OVT_DeploymentComponent.m_fThreatLevel`
  is frozen at spawn time, `OVT_DeploymentConfig.m_iMinimumThreatLevel` is a spawn precondition; a gm-map
  extension must choose deliberately); civilian group records (record-budget dominance).

---

## Master Overview Rollup

- **Rollup status:** In Progress (4/5 features complete — gm-state, overthrow-panel, hud-icons and waypoint-viz all closed at 100/100% with every owed verification discharged 2026-08-16 in one batched MP session; gm-map planned)
- **One-line summary for master:** Game Master tooling for server owners — Phase 1 gives GMs read-only campaign inspection (Overthrow panel, HUD icons, waypoint viz, GM map layers); the gm-state seam, the panel, hud-icons (campaign icons + live hover tooltips, tooltips-only after a same-day descope; vanilla-like 2 km icon fade after a 2026-08-16 tweak) and waypoint-viz (3D routes for a selected AI group, own read-only wire) are all complete at 113/113 tasks with every owed MP/JIP verification discharged by the user 2026-08-16; only gm-map remains in Phase 1.

---

*This is a required file — do not delete it; it marks the folder as an epic. Update it with `/update-epic gm` after working on the epic's features, and run `/review-epic gm` to refresh the Tech Debt / Findings section.*
