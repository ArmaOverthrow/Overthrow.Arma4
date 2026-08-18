# Resistance - Epic Overview

**Epic:** resistance
**Status:** 🟢 Active (6 documented retrospective + `sleep` **done** + recruit-ux Ready for Review + vehicle-storage planned)
**Last Updated:** 2026-08-19

> **This file is the epic marker.** Its presence in `docs/features/resistance/` is what tells every Beast Mode command (and future Web App / Discord clients) that this folder is an **epic**, not a plain feature. Keep it present and keep the required sections below filled in. It is the epic's equivalent of the project's `docs/overview.md`, scoped to this epic. The master `docs/overview.md` tracks this epic as a **single row**; the per-feature detail lives **here**.

---

## Purpose

The resistance epic owns the player's side of Overthrow's war: the resistance faction manager (player faction choice, camps, FOB records, garrisons, officers), the mobile FOB system (the deployable truck that becomes a shared forward base), the base building and placement system (placeables, buildables, build/place UI, item limits), the recruit system (recruiting and managing persistent AI squadmates), the loadout system (saving/applying player and recruit equipment at equipment boxes), and the wanted system (per-character wanted levels, stealth detection and disguises governing occupying-faction hostility). Together these are the tools the player uses to grow from a lone dissident into an armed movement — the mirror image of the `occupying` epic's AI antagonist.

---

## Features

The constituent features of this epic. All already existed in code and were documented retrospectively via `/discover-feature` (core/building/recruits/loadouts on 2026-08-02; wanted-system on 2026-08-03; fob carved out of core on 2026-08-09).

| # | Feature | Status | Tasks | Description |
|---|---------|--------|-------|-------------|
| 1 | core | 📄 Documented | — (retrospective) | `OVT_ResistanceFactionManager` — camps, FOB *records*, player-bought garrisons, officer facade, player faction key; JIP + idempotent vanilla-persistence serializer. Healthy persistence layer; headline remaining debt: dedicated-server-broken officer promotion (BUG-045) and residual client-trust in garrison RPC inputs (BUG-046/047 fixes landed). |
| 2 | fob | 📄 Documented | Complete (100%) | Mobile FOB — truck↔deployed-base prefab swap with async cargo transfer, deploy exclusion zones (server-validated + map overlay), priority FOB, fast travel, build/place zone, garrisons, set-home respawn. Discovery (2026-08-09) filed **BUG-119…128**: officer gate bypassable via the vehicle-upgrade path with client-side payment (BUG-122), undeploy area-wipes neighbours' property (BUG-124), error-path record/garrison leaks (BUG-121/125), unauthenticated priority RPC (BUG-123), zero-vector nearest-FOB queries (BUG-126), plus nameless FOBs and dropped notifications. |
| 3 | building | 📄 Documented | — (retrospective) | Placement & construction — 8 placeables / 7 buildables, ghost-preview place/build UI contexts, placeable/buildable marker components + handlers, per-location item limits, server spawn pipeline, vanilla persistence. Headline debt: everything is client-validated only (BUG-048), removal mode is dedicated-server-broken twice over (BUG-049), and pagination VM-errors on one click (BUG-050). |
| 4 | recruits | 📄 Documented | — (retrospective) | `OVT_RecruitManagerComponent` (2219 L) — persistent player-owned AI squadmates: recruit civilians/tents, XP & naming, vanilla-group command, offline body save/release + async respawn with gear, roster UI. Headline debt: unvalidated recruit RPCs (BUG-051), client-local rename (BUG-052), fast-travel leaves recruits behind after charging (BUG-053). |
| 5 | loadouts | 📄 Documented | — (retrospective) | `OVT_LoadoutManagerComponent` (2103 L) — save/apply equipment loadouts at equipment boxes (players + recruits, officer templates); recursive item tree with slot addressing; persistence rebuilt 2026-08-02 in the vanilla-persistence migration. Headline debt: box-apply spawns the equipped weapon free (BUG-042), zero-validation RPCs (BUG-043), stowed weapons never captured (BUG-044), officer templates inert. |
| 6 | wanted-system | 📄 Documented | — (retrospective) | `OVT_PlayerWantedComponent` (810 L) — per-character 0–4 star wanted/stealth layer: 1 Hz perception+LOS detection scan, outfit-faction disguises, combat escalation, perceived-faction override as the AI interface, stars/seen-eye/undercover HUD. Headline debt (**BUG-072…078**): FRIENDLY-bucket-only scan drops "seen" mid-firefight (BUG-072), client-authoritative wanted state with server co-writers + likely double-registered tick (BUG-073), listen-host loot event flags every seen recruit (BUG-074), invoker leak (BUG-075), plus O(N·M) world scans and zero tests. |
| 7 | recruit-ux | 🟢 Ready for Review | 73/73 (100%) | Recruit squad management built 2026-08-14: **inactive ("holding") recruits** (park/recall via held actions + roster, self-owned delete-when-empty AI groups with 50 m clustering, serializer v3, JIP + broadcast), recruit **map marker layer** with armed/ammo badges + filter row, **sectioned roster** (flat gamepad-safe selection, capacity header, status icons, G/LT toggle), **loadout swap** (entity-transfer only — nothing spawned/deleted), help/FM/wiki synced. **Phase 9 added same-day** (user request): buy a recruit at the tent pre-equipped with a saved loadout — live local shop pricing × difficulty-scaled convenience fee, server-computed and charged, shared tent-spawn internals with placement hardening. Play-test riders: BUG-170 fixed (defend→wait waypoint), hold durations shortened. Rode along: BUG-107 fixed; BUG-166/167 filed. All automated gates green (final All 165/165). Pending: manual play-test checklists + localization re-export (Phase 9 keys). |
| 8 | vehicle-storage | 📋 Planned | — (requirements only) | Building-mounted vehicle storage component (store/retrieve via parking spaces) — requirements written, not yet planned. |
| 9 | sleep | ✅ **Done** (T6.3 wiki outstanding) | 32/33 (97%) | **Single-player time skip built 2026-08-18/19.** A "Sleep" action on the seven vanilla bed prefabs (same-path overrides) and a new **Cot** placeable skips **8 in-game hours** — and every accounting sweep the skipped window contained actually runs: income/taxes/donations per 6-hour boundary, NPC shop buying, the 07:00 restock, midnight rent, occupying-faction resource gain/spend and 15-minute threat decay, all replayed through the *same* methods the live tick calls. Gated to an owned house / own camp / deployed FOB / captured base, disabled-with-reason during a QRF or while wanted, 12 in-game-hour cooldown with a live countdown in the label (persisted, serializer **v5**). **Rode along: BUG-183 fixed** (unpersisted economy hour latches were re-paying income on every save load — a shipped money exploit); **BUG-186 filed** (both `CheckUpdate` timers schedule off the *day* multiplier only, so the occupying faction's minute-exact gates are skipped at night). Cross-phase review caught the one real defect — the occupying replay had neither of the economy's two edge defences, double-counting its landing boundary and losing its starting one — fixed with latches mirroring `m_iHourPaidIncome`. All automated gates green (All **179/179**) **and the full 26-item play-test passed 2026-08-19** — which closed **BUG-183**. Only outstanding: **T6.3**, the public wiki sync (no wikijs MCP server was attached to the build session). |

> Reference any feature with the slash form `resistance/[feature-name]` (e.g. `/continue-feature resistance/core`). Task counts are pulled from each feature's own `tasks.md` and refreshed by `/update-epic`.

---

## Build Order / Dependencies

Documentation (and any future enhancement work) follows the dependency chain:

1. **core** — the command layer everything else hangs off; owns camp/FOB records, garrisons, officers, and the `PlaceItem`/`BuildItem` server endpoints.
2. **fob** — the mobile FOB lifecycle over core's registry: deploy/undeploy writes records via `RegisterFOB`/`UnregisterFOB`, garrison purchase and officer gating come from core.
3. **building** — its UI contexts call core's endpoints; the camp handler registers camps back into core; item limits count by core's camp/FOB/base association; FOB proximity (from fob's records) grants place/build rights.
4. **recruits** — recruits are commanded from camps/tents that building places and core registers; garrison purchase in core reuses `baseRecruitCost`.
5. **loadouts** — applied at equipment boxes that building places; officer templates gated by core's `IsOfficer`; loadouts applied to recruits via the recruits roster.

**Dependencies between features:**
- core → fob (record registry, serializer, JIP, garrison machinery, `IsOfficer`; fob's `RegisterFOB`/`UnregisterFOB`/`CleanupFOBArea` live in core's manager file)
- fob → building (FOB proximity legalises place/build within 100 m; undeploy cleanup deletes placeables/buildables — unfiltered, BUG-124)
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
  - **Prefab-swap FOB mobility:** deploy/undeploy swaps truck↔FOB prefabs at the same transform with an async cargo transfer through the initiating player's controller (full treatment in `resistance/fob`). The BUG-046 fall-throughs are fixed; the op state is still a shared manager singleton (one operation server-wide), and the remaining edge-path defects are BUG-119…128.

---

## Tech Debt / Findings

Cross-feature tech debt and review findings. **Populated and updated by `/review-epic`** (which also writes feature-specific findings into the relevant child `context.md` files). Seeded from discovery (2026-08-02); per-feature detail lives in each child's `implementation.md`/`context.md`.

- [ ] 💳 **Client-trust RPC seams across every feature** (**BUG-043** loadouts, **BUG-047** garrisons, **BUG-048** place/build, **BUG-051** recruits) — all — client-supplied `playerId`, unchecked indices/RplIds (several are remote server-crash vectors), and no server-side rule/cost re-checks. The epic's version of the economy (BUG-019…024) and occupying (BUG-025) client-authority class; fixing wants one shared "derive actor from RPC sender + validate + charge server-side" pattern in `OVT_PlayerCommsComponent`.
- [ ] 💳 **Item/vehicle duplication paths** (**BUG-042** loadout weapon spawn, **BUG-048** uncharged buildables; the third — BUG-046 FOB deploy fall-throughs — is fixed) — loadouts + building — each alone breaks the economy. The FOB feature's replacement economy hole is **BUG-122** (uncharged/ungated Mobile FOB via vehicle upgrade, `resistance/fob`).
- [ ] 💳 **"Works hosted, silently broken dedicated" feature set** (**BUG-045** officer promotion, **BUG-049** removal mode, **BUG-052** recruit rename) — core + building + recruits — client-side mutations that never had server round-trips. Officer promotion is the most severe: the officer role gates FOBs, templates and building rights, and cannot be granted in MP.
- [ ] 💳 **Client-side charging decoupled from server effects** (**BUG-053** fast-travel fees; garrison/build/recruit charges) — all — honest players can be charged for failed actions; `DoTakePlayerMoney` clamps at zero so underfunded purchases succeed.
- [ ] 💳 **Cleanup/lifecycle blind spots** — core + fob + building — removing a camp/FOB orphans its garrison AI (FOB half now **BUG-125**); `CleanupFOBArea` deletes by radius with no association check (now **BUG-124**; `CleanupCampObjects` filters correctly); `FindNearestBase` has no distance cap so association and limit accounting drift (a camp tent even self-associates with the *previous* nearest location); ghost FOB records now arise only from the undeploy *error* path (**BUG-121** — the BUG-046 exact-equality path is fixed).
- [ ] 💳 **Loadout capture is lossy and officer templates are inert** (**BUG-044** stowed weapons; overwrite-save destroys the old loadout on empty extraction; officer templates saved but never listed to anyone) — loadouts.
- [ ] 💳 **Dead/vestigial code** — core + fob + building + recruits — `m_pHiredCivilianPrefab`, `OVT_UndeployFOBAction_New.c` + the empty `OVT_ResistanceFOBControllerComponent` (fob; the always-failing controller casts in FOB completion handlers are fixed), an unreachable duplicate `m_bAwayFromBases` rule block in `OVT_PlaceContext`, recruit XP hardcoding `"US"/"USSR"` faction keys (zero recruit XP on custom-faction campaigns), `FindRecruitEntity` mutating its map mid-iteration.
- [ ] 💳 **Test coverage is near-zero for the whole epic** — all — only the resistance serializer's round trip is covered. Logic-tier candidates are listed per feature (persisted-record matching/idempotency, pagination arithmetic, recruit level curve, loadout id/key round trips, item-limit thresholds, nearest-base selection).

---

## Master Overview Rollup

How this epic is represented in the project's master `docs/overview.md` (one row, not its children). Kept in sync by `/update-epic` and `/update-master`.

- **Rollup status:** 🟢 Active — `sleep` **done** (play-test passed, 32/33, only the wiki sync owed); `recruit-ux` Ready for Review (73/73); 6 documented retrospective; `vehicle-storage` planned
- **One-line summary for master:** The player resistance faction — command layer (`core`), mobile `fob`, base `building`, persistent AI `recruits`, `loadouts` and the `wanted-system` (retrospective docs), plus **`recruit-ux`** (built 2026-08-14): inactive "holding" recruits, a recruit map layer, a sectioned gamepad-safe roster and a whole-kit loadout swap — and **`sleep`** (built 2026-08-18/19): a single-player 8-hour time skip on beds and a new Cot placeable that *replays* every accounting sweep the skipped hours contained, with a persisted 12-hour cooldown — which fixed shipped money exploit **BUG-183** and filed **BUG-186** on the way. `sleep` is **play-test-passed and done** bar the wiki sync; `recruit-ux` is still play-test-pending. All automated gates green (All 179/179). `vehicle-storage` is planned next. Legacy discovery debt: **BUG-042…053** (several fixed) and FOB's **BUG-119…128**.

---

*This is a required file — do not delete it; it marks the folder as an epic. Update it with `/update-epic resistance` after working on the epic's features, and run `/review-epic resistance` to refresh the Tech Debt / Findings section.*
