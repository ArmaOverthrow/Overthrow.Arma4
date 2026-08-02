# Resistance - Epic Overview

**Epic:** resistance
**Status:** 📄 Documented (4/4 retrospective)
**Last Updated:** 2026-08-02

> **This file is the epic marker.** Its presence in `docs/features/resistance/` is what tells every Beast Mode command (and future Web App / Discord clients) that this folder is an **epic**, not a plain feature. Keep it present and keep the required sections below filled in. It is the epic's equivalent of the project's `docs/overview.md`, scoped to this epic. The master `docs/overview.md` tracks this epic as a **single row**; the per-feature detail lives **here**.

---

## Purpose

The resistance epic owns the player's side of Overthrow's war: the resistance faction manager (player faction choice, camps, FOBs, garrisons, officers), the base building and placement system (placeables, buildables, build/place UI, item limits), the recruit system (recruiting and managing persistent AI squadmates), and the loadout system (saving/applying player and recruit equipment at equipment boxes). Together these are the tools the player uses to grow from a lone dissident into an armed movement — the mirror image of the `occupying` epic's AI antagonist.

---

## Features

The constituent features of this epic. All four already existed in code and were documented retrospectively via `/discover-feature` on 2026-08-02.

| # | Feature | Status | Tasks | Description |
|---|---------|--------|-------|-------------|
| 1 | core | 📄 Documented | — (retrospective) | `OVT_ResistanceFactionManager` (1663 L) — camps, FOBs (mobile deploy/undeploy state machine), player-bought garrisons, officer facade, player faction key; JIP + idempotent vanilla-persistence serializer. Healthy persistence layer; the interactive layer has a dedicated-server-broken officer promotion (BUG-045), FOB-swap duplication fall-throughs (BUG-046) and unvalidated garrison RPCs (BUG-047). |
| 2 | building | 📄 Documented | — (retrospective) | Placement & construction — 8 placeables / 7 buildables, ghost-preview place/build UI contexts, placeable/buildable marker components + handlers, per-location item limits, server spawn pipeline, vanilla persistence. Headline debt: everything is client-validated only (BUG-048), removal mode is dedicated-server-broken twice over (BUG-049), and pagination VM-errors on one click (BUG-050). |
| 3 | recruits | 📄 Documented | — (retrospective) | `OVT_RecruitManagerComponent` (2219 L) — persistent player-owned AI squadmates: recruit civilians/tents, XP & naming, vanilla-group command, offline body save/release + async respawn with gear, roster UI. Headline debt: unvalidated recruit RPCs (BUG-051), client-local rename (BUG-052), fast-travel leaves recruits behind after charging (BUG-053). |
| 4 | loadouts | 📄 Documented | — (retrospective) | `OVT_LoadoutManagerComponent` (2103 L) — save/apply equipment loadouts at equipment boxes (players + recruits, officer templates); recursive item tree with slot addressing; persistence rebuilt 2026-08-02 in the vanilla-persistence migration. Headline debt: box-apply spawns the equipped weapon free (BUG-042), zero-validation RPCs (BUG-043), stowed weapons never captured (BUG-044), officer templates inert. |

> Reference any feature with the slash form `resistance/[feature-name]` (e.g. `/continue-feature resistance/core`). Task counts are pulled from each feature's own `tasks.md` and refreshed by `/update-epic`.

---

## Build Order / Dependencies

Documentation (and any future enhancement work) follows the dependency chain:

1. **core** — the command layer everything else hangs off; owns camp/FOB records, garrisons, officers, and the `PlaceItem`/`BuildItem` server endpoints.
2. **building** — its UI contexts call core's endpoints; the camp handler registers camps back into core; item limits count by core's camp/FOB/base association.
3. **recruits** — recruits are commanded from camps/tents that building places and core registers; garrison purchase in core reuses `baseRecruitCost`.
4. **loadouts** — applied at equipment boxes that building places; officer templates gated by core's `IsOfficer`; loadouts applied to recruits via the recruits roster.

**Dependencies between features:**
- core → building (`PlaceItem`/`BuildItem`/`RemovePlacedItem` endpoints, camp registration handoff, `FindNearestBase` association, cleanup on camp/FOB removal)
- building → recruits (recruitment tent placeable), building → loadouts (equipment box placeable)
- core → loadouts (`IsOfficer` gates officer-template save)
- recruits ↔ loadouts (`ApplyLoadoutToEntityFromBox(recruitEntity)` handoff; recruit gear then persists with the body)
- External: core (epic) game-mode/config/player-manager/persistence; economy (costs, money RPCs); occupying (camps/FOBs registered as OF known targets, captured-base build rights, QRF reaction); towns (supporter draw-down, poster support modifiers); skills (place/build XP, stealth inheritance to recruits).

---

## Integration & Architecture

- **Within the epic:** `OVT_ResistanceFactionManager` (singleton on the game mode) is the record-keeper and server endpoint hub; the building UI contexts are pure clients of it; `OVT_RecruitManagerComponent` and `OVT_LoadoutManagerComponent` are sibling singletons owning their own tables. Placed/built entities carry marker components (`OVT_PlaceableComponent`/`OVT_BuildableComponent`) stamped server-side with owner UID + nearest camp/FOB/base association — nothing on them replicates.
- **With other epics / features:** client→server traffic funnels through `OVT_PlayerCommsComponent` RpcAsks (the epic's dominant defect surface); persistence is fully on the vanilla stack (three manager serializers — resistance, recruits v2, loadouts v2 — plus SelfSpawn entity configs and placeable/buildable/ammobox component serializers); every camp/FOB is registered as an occupying-faction known target; XP flows to the skills epic via `m_OnPlace`/`m_OnBuild` and recruit kill events.
- **Key architectural decisions for the epic as a whole:**
  - **Records vs bodies/entities:** durable string-keyed records (camps, FOBs, recruits, loadouts) are the persisted truth; world entities are respawned/re-asked from them (garrisons from prefab snapshots, recruit bodies via `SCR_PersistenceSystem` save-and-release, loadout items from box stock). The record half is the healthy half.
  - **Hand-rolled JIP + delta broadcasts:** camp/FOB/recruit/loadout-index tables ship via `RplSave`/`RplLoad` with reliable `RpcDo_*` deltas — no RplProps on the data model.
  - **Client-computed costs paid via a generic money RPC** — the charge and the effect are two unlinked RPCs everywhere (build, garrisons, recruits, fast-travel), which is the root of most of the epic's exploit class.
  - **Conservation-based loadouts:** apply consumes matching items from the box rather than spawning — violated only by the equipped-weapon path (BUG-042).
  - **Prefab-swap FOB mobility:** deploy/undeploy swaps truck↔FOB prefabs at the same transform with an async cargo transfer through the initiating player's controller — with shared mutable op state (BUG-046).

---

## Tech Debt / Findings

Cross-feature tech debt and review findings. **Populated and updated by `/review-epic`** (which also writes feature-specific findings into the relevant child `context.md` files). Seeded from discovery (2026-08-02); per-feature detail lives in each child's `implementation.md`/`context.md`.

- [ ] 💳 **Client-trust RPC seams across every feature** (**BUG-043** loadouts, **BUG-047** garrisons, **BUG-048** place/build, **BUG-051** recruits) — all — client-supplied `playerId`, unchecked indices/RplIds (several are remote server-crash vectors), and no server-side rule/cost re-checks. The epic's version of the economy (BUG-019…024) and occupying (BUG-025) client-authority class; fixing wants one shared "derive actor from RPC sender + validate + charge server-side" pattern in `OVT_PlayerCommsComponent`.
- [ ] 💳 **Three independent item/vehicle duplication paths** (**BUG-042** loadout weapon spawn, **BUG-046** FOB deploy fall-throughs, **BUG-048** uncharged buildables) — loadouts + core + building — each alone breaks the economy; together they make all resistance costs advisory.
- [ ] 💳 **"Works hosted, silently broken dedicated" feature set** (**BUG-045** officer promotion, **BUG-049** removal mode, **BUG-052** recruit rename) — core + building + recruits — client-side mutations that never had server round-trips. Officer promotion is the most severe: the officer role gates FOBs, templates and building rights, and cannot be granted in MP.
- [ ] 💳 **Client-side charging decoupled from server effects** (**BUG-053** fast-travel fees; garrison/build/recruit charges) — all — honest players can be charged for failed actions; `DoTakePlayerMoney` clamps at zero so underfunded purchases succeed.
- [ ] 💳 **Cleanup/lifecycle blind spots** — core + building — removing a camp/FOB orphans its garrison AI; `CleanupFOBArea`/`CleanupCampObjects` delete by radius with no association check (can destroy a neighbor camp's objects); `FindNearestBase` has no distance cap so association and limit accounting drift (a camp tent even self-associates with the *previous* nearest location); ghost FOB records from exact position-equality unregistration (BUG-046).
- [ ] 💳 **Loadout capture is lossy and officer templates are inert** (**BUG-044** stowed weapons; overwrite-save destroys the old loadout on empty extraction; officer templates saved but never listed to anyone) — loadouts.
- [ ] 💳 **Dead/vestigial code** — core + building + recruits — `m_pHiredCivilianPrefab`, the always-failing `OVT_OverthrowController` casts in FOB completion handlers, an unreachable duplicate `m_bAwayFromBases` rule block in `OVT_PlaceContext`, recruit XP hardcoding `"US"/"USSR"` faction keys (zero recruit XP on custom-faction campaigns), `FindRecruitEntity` mutating its map mid-iteration.
- [ ] 💳 **Test coverage is near-zero for the whole epic** — all — only the resistance serializer's round trip is covered. Logic-tier candidates are listed per feature (persisted-record matching/idempotency, pagination arithmetic, recruit level curve, loadout id/key round trips, item-limit thresholds, nearest-base selection).

---

## Master Overview Rollup

How this epic is represented in the project's master `docs/overview.md` (one row, not its children). Kept in sync by `/update-epic` and `/update-master`.

- **Rollup status:** 📄 Documented (4/4 retrospective)
- **One-line summary for master:** The player resistance faction — command layer (`core`: camps/FOBs/garrisons/officers), base `building`/placement, persistent AI `recruits` and equipment `loadouts` — backfilled with retrospective docs. Discovery catalogued dozens of concrete issues, headlined by: an infinite weapon-duplication loadout exploit, officer promotion silently broken on dedicated servers, FOB deploy/undeploy vehicle+cargo duplication, unvalidated place/build/garrison/recruit/loadout RPCs with client-side payment, and a one-click VM error in the place/build menus. Top 12 filed as **BUG-042…053**.

---

*This is a required file — do not delete it; it marks the folder as an epic. Update it with `/update-epic resistance` after working on the epic's features, and run `/review-epic resistance` to refresh the Tech Debt / Findings section.*
