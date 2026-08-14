# GM - Epic Overview

**Epic:** gm
**Status:** 🟡 In Progress (1/5 features built)
**Last Updated:** 2026-08-14

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
| 1 | gm-state | ✅ Built (MP verify owed) | 24/26 | Shared read-only replication seam streaming Overthrow campaign state (threat, OF resources, countdowns, funds, per-entity detail) to authorized GM clients |
| 2 | overthrow-panel | Planned | — | New GM UI panel (above the settings panel, bottom-left, Overthrow logo) showing campaign-wide info and selected-icon detail |
| 3 | hud-icons | Planned | — | Extend/add GM HUD icons for towns, bases, groups and players; clicking shows Overthrow detail (support/stability, garrison/resources, group origin & purpose, player money/level) |
| 4 | waypoint-viz | Planned | — | Read-only visualization of Overthrow-generated AI waypoints in GM view (own implementation; not the E_ waypoint set) |
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

- **Pre-existing group-cleanup defects in the `occupying` epic's systems** (found by gm-state planning, routed
  around by its tag-and-sweep registry — **not gm-state's debt**): QRF groups are never deleted
  (`OVT_QRFControllerComponent.m_Groups` filled, drained nowhere; only the controller entity is deleted);
  `OVT_EntitySpawningAPI.CleanupGroup` deletes a group's soldiers but not the group entity; camp/FOB removal
  never touches `garrisonEntities`; six resistance-garrison rollback paths spawn-then-delete untracked groups.
  These belong to the `occupying` epic to fix.
- **Help-docs hand-off:** gm-state ships zero visible surface (a transport with no renderer), so it carries no
  help/wiki phase. **`overthrow-panel` — the epic's first visible feature — must carry the `help-docs-sync`
  phase** covering the whole Phase 1 GM experience.
- **gm-state deferred payloads (deliberate, additive later):** per-upgrade *positions* (gm-map adds when its
  icon layer needs them); deployment threat-level (no live field exists — `OVT_DeploymentComponent.m_fThreatLevel`
  is frozen at spawn time, `OVT_DeploymentConfig.m_iMinimumThreatLevel` is a spawn precondition; a gm-map
  extension must choose deliberately); civilian group records (record-budget dominance).

---

## Master Overview Rollup

- **Rollup status:** In Progress (1/5 features — gm-state built, MP verify owed)
- **One-line summary for master:** Game Master tooling for server owners — Phase 1 gives GMs read-only campaign inspection (Overthrow panel, HUD icons, waypoint viz, GM map layers) ahead of later management and cleanup phases; the gm-state data seam is built and awaiting MP play-test.

---

*This is a required file — do not delete it; it marks the folder as an epic. Update it with `/update-epic gm` after working on the epic's features, and run `/review-epic gm` to refresh the Tech Debt / Findings section.*
