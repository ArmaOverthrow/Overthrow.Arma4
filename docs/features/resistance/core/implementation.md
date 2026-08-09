# Resistance Core - Implementation Plan (Retrospective)

**Status:** Implemented (Documented Retrospectively)
**Originally Implemented:** Unknown (inherited from early Overthrow Reforger development)
**Documented:** 2026-08-02
**Last Updated:** 2026-08-09

> **2026-08-09:** The mobile FOB feature (truck, deploy/undeploy state machine, map presence, FOB gameplay rules) was carved out into its own feature — **`resistance/fob`**. Core keeps hosting the FOB *registry* (`OVT_FOBData`/`m_FOBs`, serializer, garrison machinery); everything else FOB-specific in this document is superseded by `docs/features/resistance/fob/`. In particular, the FOB Known Issues below describe the **pre-BUG-046-fix** state; the current defect set is BUG-119…128, filed against `resistance/fob`.

---

## Executive Summary

The command layer of the player's resistance faction. `OVT_ResistanceFactionManager` (a singleton on the game mode) owns the registries of **camps** (personal spawn/fast-travel/stash points, one per player) and **FOBs** (shared forward bases — records only; the mobile FOB truck and its deploy/undeploy lifecycle are the sibling **`resistance/fob`** feature), the **officer** role (promotion + checks consumed across the mod), resistance **garrisons** (player-bought AI defense groups at bases, camps and FOBs), and the **player faction identity** (`m_sPlayerFaction` restore on load). It also hosts `PlaceItem`/`BuildItem` — the server-side spawn/charge/track endpoints the sibling `resistance/building` feature calls into.

**Note:** This is a retrospective implementation plan created by analyzing the existing codebase. The feature has already been implemented.

---

## Goals

### Primary Goals
- Give players persistent forward infrastructure: a personal camp each, plus shared FOBs (the FOB lifecycle itself is `resistance/fob`), both usable for fast travel, building and garrisons.
- An officer role gating strategic actions (FOB deploy, tax, funds, officer loadout templates), grantable in-game and by config/admin.
- Persist and replicate all of it.

### Success Criteria
- [x] Camps: place (via `resistance/building` handler), one-per-player replacement, privacy toggle, owner-gated manage/delete, map icons, fast travel
- [x] FOB registry: records, garrison purchase, JIP + persistence (the truck/deploy/undeploy/map lifecycle is `resistance/fob`)
- [x] Officers: auto-grant (SP/host/admin/config list), promotion UI, checks used by loadouts/shops/actions
- [x] Persistence: player faction key, camp/FOB records + garrison prefab lists round-trip (idempotent re-apply, covered by the persistence test tier)
- [x] JIP: camps/FOBs stream to late joiners via `RplSave`/`RplLoad`; deltas via broadcast RPCs
- [ ] Officer promotion working in dedicated MP (the client path is a local no-op — see Known Issues)

---

## Current Architecture

### Key Components
- `Scripts/Game/GameMode/Managers/Factions/OVT_ResistanceFactionManager.c` (1663 L) — the manager + record classes (`OVT_CampData`, `OVT_FOBData`, `OVT_VehicleUpgrades`/`OVT_VehicleUpgrade`). Static `s_Instance`, accessed via `OVT_Global.GetResistanceFaction()`.
- `Scripts/Game/Persistence/Serializers/Components/OVT_ResistanceManagerSerializer.c` — vanilla-persistence serializer (+ `OVT_PersistedCamp`/`OVT_PersistedFOB` records); registered in `Configs/Systems/Persistence/Overthrow.conf`.
- `Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c` — the client→server RPC funnel: `RpcAsk_PlaceItem`, `RpcAsk_BuildItem`, `RpcAsk_RemovePlacedItem`, `RpcAsk_AddGarrison{,Camp,FOB}`, `RpcAsk_DeployFOB`, `RpcAsk_UndeployFOB`, `RpcAsk_SetCampPrivacy`, `RpcAsk_DeleteCamp`, `RpcAsk_SetPriorityFOB`.
- `Scripts/Game/UserActions/` — `OVT_ManageBaseAction` (routes to base **or** FOB menu by proximity), `OVT_ManageCampAction` (owner-only), `OVT_SaveOfficerLoadoutAction` (officer template save via loadout manager). FOB actions (deploy/undeploy/priority) are documented in `resistance/fob`.
- `Scripts/Game/UI/Context/` — `OVT_ResistanceMenuContext` (funds/tax/leaderboard/Make Officer), `OVT_FOBMenuContext` (garrison purchase for FOBs *and* camps), `OVT_CampMenuContext` (privacy/delete).
- `Scripts/Game/UI/Map/OVT_MapIcons.c` — camp icons (privacy-filtered), FOB icons (priority FOB always visible); `Scripts/Game/UI/Context/OVT_MapContext.c` — fast travel gating to camps (owner/public) and FOBs.
- `Scripts/Game/GameMode/Placeables/OVT_PlaceableCampHandler.c` — the handoff from `resistance/building`: placing the "Camp" placeable calls `RegisterCamp`.
- Config on the game-mode prefab (`Prefabs/GameMode/OVT_OverthrowGameMode.et:157`): placeables/buildables config files, vehicle upgrade tree (consumed by `OVT_ManageVehicleContext`), the mobile FOB prefab pair (documented in `resistance/fob`), `m_pHiredCivilianPrefab` (dead — no consumers).

### Data Flow
- **Camps:** `resistance/building` places the camp composition → `OVT_PlaceableCampHandler.OnPlace` → `RegisterCamp(entity, playerId)` (server): creates `OVT_CampData` (persistentId `CAMP_<unixtime>_<rand>`, owner persId, name "#OVT-Place_Camp <player>"), replaces the player's previous camp (`RemoveOldCamp` — deletes entity, cleans associated placeables/buildables by `BelongsTo(persistentId, CAMP)` within 75 m), writes `player.camp`, broadcasts `RpcDo_RegisterCamp`. Privacy (`SetCampPrivacy`) and deletion (`RemoveCamp` — also removes the entity by RplId) are position-matched (`camp.location == pos`, exact equality) and mirrored by broadcast RPCs.
- **FOBs (mobile):** the full truck→deployed-base lifecycle (deploy/undeploy state machine, cargo transfer, priority FOB, map presence, cleanup) is documented in **`resistance/fob`**. From core's perspective: `RegisterFOB`/`UnregisterFOB` maintain `m_FOBs` and broadcast `RpcDo_RegisterFOB`/`RpcDo_RemoveFOB`; the records feed the serializer, JIP, garrison spawns and `FindNearestBase`.
- **Garrisons:** `OVT_FOBMenuContext.AddToGarrison` charges the *client* (`TakeLocalPlayerMoney`), then comms `RpcAsk_AddGarrison{Camp,FOB}(location, prefabIndex)` → server resolves nearest camp/FOB record and spawns `faction.m_aGroupPrefabSlots[prefabIndex]` with a defend waypoint (`SpawnGarrisonCamp`/`SpawnGarrisonFOB`), records the group id in `garrisonEntities`, and takes supporters from the nearest town. Base garrisons (`AddGarrison`) additionally get a 3-stop patrol cycle over the base controller's close slots. `garrison` (prefab list) is the *persisted* shape; `garrisonEntities` (live ids) is the runtime shape.
- **Officers:** `OVT_PlayerData.isOfficer`, stored/replicated by the **player manager** (JIP `writer.WriteBool(player.isOfficer)` at `OVT_PlayerManagerComponent.c:642`, persisted via `OVT_PlayerManagerSerializer`). Grants: SP/listen-host (`OVT_OverthrowGameMode.c:852-856`), config officer list (`:909-916`), admin role change (`:731-746`), and the resistance menu's Make Officer button. `AddOfficer` = local apply + broadcast `RpcDo_AddOfficer` (hint on the promoted client).
- **Player faction:** effectively a config attribute (`m_sPlayerFaction`, default FIA); the serializer stores the key and `ApplyPersistedPlayerFaction` restores it (refusing empty/unknown keys rather than guessing).

### Integration Points
- **resistance/building** (sibling): `PlaceItem`/`BuildItem`/`RemovePlacedItem` are hosted here — spawn, ownership stamp, nearest-base association (`FindNearestBase` over camps/FOBs/player bases), cost, navmesh rebuild, `OVT_PersistenceTracking.Track`, `m_OnPlace`/`m_OnBuild` invokers; the placeables/buildables configs are loaded here; `OVT_PlaceableCampHandler` is the camp handoff; `OVT_ItemLimitChecker` counts items per camp/FOB via the association.
- **occupying epic:** every camp and FOB is registered as an OF known target (`OVT_OccupyingFactionManager.c:1175-1237`) — the OF is omniscient about them; FOB deploy validates against OF base/tower ranges; `AddGarrison` targets captured bases (`OVT_BaseData.garrisonEntities`), and the OF manager respawns those garrisons on load.
- **towns:** garrison hires call `TakeSupportersFromNearestTown`; notifications resolve town names.
- **economy:** vehicle-upgrade prefab registration (`RegisterUpgrades`), placeable costs (`TakePlayerMoney`), resistance funds/tax/donations UI in the resistance menu.
- **vehicles:** deploy/undeploy uses `SpawnVehicleMatrix` + `m_aVehicles` bookkeeping + `GetOwnerID`.
- **resistance/loadouts** (sibling): `IsOfficer` gates officer template save/delete (`OVT_LoadoutManagerComponent.c:321`).
- **player manager / OVT_OverthrowController:** officer + `player.camp` storage and replication; the per-player controller carries the `OVT_ContainerTransferComponent` the FOB state machine borrows.
- **map:** `OVT_MapIcons` (camp privacy filter, priority FOB), `OVT_MapContext` fast travel (camp owner/public within 40 m; any FOB within 40 m).

---

## Implementation Details

### Phase 1: Records & Registries (COMPLETED)
- `OVT_CampData`/`OVT_FOBData` plain records with `persistentId` (timestamp+random), owner persId, location; `id` documented as "the array index" but only ever written by the persistence re-apply path (live registration never sets it — it is effectively unused).
- Nearest/distance query helpers (`GetNearestCamp(Data)`, `GetNearestFOB(Data)`, `DistanceToCamp`, `FindNearestBase`).

### Phase 2: Camp & FOB Lifecycle (COMPLETED)
- Camp register/replace/privacy/delete with associated-object cleanup; FOB register/unregister backing the `resistance/fob` deploy/undeploy lifecycle.

### Phase 3: Garrisons & Officers (COMPLETED)
- Garrison purchase per base/camp/FOB, waypoint assignment, supporter draw-down; officer grants from four paths; officer checks exported mod-wide.

### Phase 4: Replication & Persistence (COMPLETED)
- Hand-rolled positional JIP (`RplSave`/`RplLoad`: camp and FOB record fields, no garrison lists — those are server-only) plus reliable broadcast delta RPCs (`RpcDo_RegisterCamp/RegisterFOB/RemoveCamp/RemoveFOB/SetCampPrivacy/SetPriorityFOB/AddOfficer`).
- Serializer: version, player faction key, camp records, FOB records; garrison snapshots prefer live groups (wiped groups are dropped) and fall back to the stored prefab list. `ApplyPersistedResistance` is idempotent (match by persistentId, position fallback for pre-id saves) and spawns nothing — `PostGameStart` → `SpawnGarrisons` replays the prefab lists, the same path a fresh campaign takes. Not persisted: live entity ids, the deployed-FOB/truck entities themselves (tracked separately by the persistence system), officer flags and `player.camp` (owned by the player manager serializer).

### Phase 5: Potential Improvements (NOT STARTED)
See Future Enhancements.

---

## Key Technical Decisions

### Decision 1: Records matched by persistentId, operated on by position
**Context:** Camps/FOBs need stable identity across saves, but RPCs need a cheap wire format.
**Implementation:** `persistentId` strings for persistence matching; live operations (privacy, delete, unregister, priority) match records by **position** — sometimes exact vector equality, sometimes a 10 m tolerance.
**Trade-offs:** Saves survive entity churn; but the mixed matching is fragile — exact equality breaks if the entity settled after registration (see Known Issues), and nothing uses `persistentId` on the wire even though both sides have it.

### Decision 2: FOB deploy/undeploy as prefab swap + async container transfer
**Moved to `resistance/fob`** (Key Technical Decisions 1–3 there), including the post-BUG-046 state of the fall-through and unsubscribe defects this section originally described.

### Decision 3: Officer state lives on OVT_PlayerData, not here
**Context:** Officer checks are needed by many systems and must survive JIP and saves.
**Implementation:** The manager only fronts `IsOfficer`/`AddOfficer`; storage, JIP and persistence ride the player manager's existing record replication.
**Trade-offs:** One source of truth, free persistence; but `AddOfficer` itself is server-broadcast-only and no comms path exists, so the one *client-side* caller (Make Officer button) silently does nothing beyond the caller's own screen on a dedicated server.

### Decision 4: Garrisons respawn from prefab lists, not persisted entities
**Context:** Same as the occupying faction — vanilla persistence would double-spawn AI.
**Implementation:** Serializer snapshots prefab names (live groups preferred; empty groups deliberately dropped so wiped garrisons stay dead); `SpawnGarrisons` replays on `PostGameStart` for both new and continued campaigns.
**Trade-offs:** Simple and idempotent; loses unit state; a stale prefab name in a save crashes the replay (no null guard on spawn).

---

## Current State

### What's Working
- Camp place/replace/privacy/delete/cleanup, FOB registry (lifecycle: see `resistance/fob`), garrison purchase at bases/camps/FOBs, officer grants (server-side paths), fast travel and map icons, JIP of camp/FOB lists, full save/load round trip of records + garrison prefab lists (covered by the persistence test tier).

### Known Issues
- **"Make Officer" is a no-op in dedicated MP** (`OVT_ResistanceFactionManager.c:408-412` + `OVT_ResistanceMenuContext.c:206`): the button calls `AddOfficer` *on the client*; its broadcast `Rpc(RpcDo_AddOfficer, …)` is dropped (clients can't broadcast) and only the local `RpcDo_AddOfficer` runs — the promoting player sees a hint, the server and everyone else (including the promotee's saved record) never learn. No `RpcAsk_AddOfficer` exists in comms.
- **FOB lifecycle issues have moved:** the deploy/undeploy fall-through duplication and dead unsubscribe originally listed here were fixed under **BUG-046**; the current FOB defect set (officer-gate bypass, unfiltered area cleanup, error-path record/garrison leaks, unauthenticated priority RPC, nameless FOBs, and more) is **BUG-119…128**, tracked in `resistance/fob`.
- **Unvalidated garrison RPCs can crash or defraud the server** (`OVT_PlayerCommsComponent.c:719-721`, `:732-736`, `:747-751`): `m_Bases[baseId]` unchecked (OOB), `GetNearestCampData/GetNearestFOBData` may return null → `fob.location` deref inside `AddGarrisonCamp/FOB` (`OVT_ResistanceFactionManager.c:769-775`, `:784-790`), `prefabIndex` unchecked into `m_aGroupPrefabSlots` (`:752/:767/:782`). Payment is **client-side** (`OVT_FOBMenuContext.c:85`) — a modified client gets free garrisons; an honest one is charged even when the server call fails.
- **Deleting a camp orphans its garrison** (`RemoveCamp :1141-1173`, `RemoveOldCamp :1227-1254`): `garrisonEntities` are never despawned — the AI groups defend a base that no longer exists (until a save/load drops them, since their record is gone). The FOB-undeploy half of this is **BUG-125** (`resistance/fob`); fix both with one shared helper.
- ~~**`SpawnGarrisons` crashes on a stale prefab**~~ — fixed since this doc was written: `SpawnGarrisons` (`:412-440`) now null-guards each spawn (verified 2026-08-09 during the `resistance/fob` discovery).
- Latent: `player.camp[0] != 0` sentinel breaks for camps at world-edge x≈0 (`:860`, `:904`); map icons hardcode faction key "FIA" for coloring (`OVT_MapIcons.c:654`, `:679`); `RpcDo_RegisterCamp` resolves owner persId on the client from a runtime id that may not have replicated yet.

### Technical Debt
- Dead code: `m_TempVehicle`/`m_TempGroup`/`MoveInGunner` (`:87`, `:94`, `:969-982` — no callers), `m_pHiredCivilianPrefab` (no consumers), `GetEntityDisplayName` (`:1452`, unused), `camp.id`/`fob.id` (only the persistence re-apply writes them; the serializer's "every insertion site" comment is wrong — `RegisterCamp`/`RegisterFOB` never do). FOB-side dead code (`OVT_UndeployFOBAction_New.c`, the empty `OVT_ResistanceFOBControllerComponent`, `OVT_StorageProgressUIContext`) is tracked in `resistance/fob`.
- Naming: camps held in variables named `fob` throughout (`SpawnGarrisons`, `RegisterCamp`, nearest-camp helpers); `OVT_FOBMenuContext` doubles as the camp garrison menu via a second `m_Camp` field.
- `OVT_FOBMenuContext.Refresh` (`:40-59`) spawns and deletes every group prefab client-side each open just to read display names/costs.
- Vehicle upgrade tree lives on this manager but belongs to the vehicles system (`OVT_ManageVehicleContext` is its only consumer).
- Magic numbers: 75 m camp/FOB cleanup radius (duplicating `PlaceContext`/`ItemLimitChecker` constants), 10 m/15 m manage-action radii, 40 m fast-travel range, 300/soldier equipment cost placeholder (`OVT_FOBMenuContext.c:48`, has a To-do).
- `CleanupFOBArea`'s missing `BelongsTo` filter is now **BUG-124** (`resistance/fob`); camp cleanup filters correctly.

---

## Future Enhancements

### High Priority
- [ ] Add `RpcAsk_AddOfficer` to comms with a server-side "caller is officer" check; route the Make Officer button through it.
- [ ] Bounds/null-check the garrison RpcAsks that remain client-trusting (deploy/undeploy validation and server-side garrison charging landed with BUG-046/047).

### Medium Priority
- [ ] Despawn `garrisonEntities` when a camp is removed (and refund or release supporters) — shared helper with the FOB half (BUG-125, `resistance/fob`).
- [ ] Match camp removal/privacy by `persistentId` (or ≤10 m tolerance) instead of exact position equality (the FOB sites were fixed under BUG-046).

> FOB-specific enhancements (names, cleanup filter, priority RPC auth, error-path conservation, controller migration) have moved to `resistance/fob`'s backlog + BUG-119…128.

### Low Priority / Nice to Have
- [ ] Delete dead code (`MoveInGunner` block, hired-civilian prefab); either use or remove `camp.id`/`fob.id` and fix the serializer comment.
- [ ] Use the player faction key for map icon colors instead of hardcoded "FIA".
- [ ] Cache group display names/costs instead of spawn-probing prefabs in the FOB menu.
- [ ] Move the vehicle upgrade tree to the vehicle manager.

---

## Testing

### Current Coverage
- Persistence tier: the resistance manager round-trips through `ApplyPersistedResistance` (same-session and save→dirty→re-apply suites); idempotence of re-apply is an explicit design property of the serializer.
- Init tier: manager resolves via `OVT_Global.GetResistanceFaction()`.

### Testing Gaps
- Logic-tier candidates (world-free, pure record math):
  - `FindPersistedCamp`/`FindPersistedFOB` matching (id match wins, empty-id position fallback <1 m, no false cross-match).
  - `ApplyPersistedResistance` id re-derivation and update-in-place (no duplicates on re-apply).
  - `ApplyPersistedGarrison` (clears target, filters empty prefab names, null tolerance).
  - `ApplyPersistedPlayerFaction` refusal rules (empty key, unknown key keep config).
  - `GetNearestCampData`/`GetNearestFOBData`/`GetNearestCamp`/`GetNearestFOB` nearest selection + empty-registry null/zero-vector behaviour.
  - `FindNearestBase` type tie-breaking across camp/FOB/player-base and occupying-base exclusion.
  - `DistanceToCamp` sentinel semantics (no player, no camp).
  - `GenerateUniquePersistentId` format/prefix.
  - Priority-FOB exclusivity (`RpcDo_SetPriorityFOB` record math: exactly one `isPriority` after apply).
- Not automatable: JIP symmetry of camp/FOB lists (two clients), the container-transfer deploy/undeploy state machine, fast travel, map icon rendering.

---

## Documentation

### Current Documentation
- This retrospective plan; the serializer's extensive in-file doc comments (scope, idempotence, deferred job system); epic docs at `docs/features/resistance/`.

### Documentation Needs
- The deploy/undeploy state machine's callback wiring (who subscribes on what entity) is the least obvious part of the FOB feature and the source of three of its worst bugs — now documented in `resistance/fob`.

---

## Dependencies

### External Dependencies
- Vanilla: `SCR_AIGroup` + waypoints, `RplComponent`/RplId, `ActionsManagerComponent`, physics activation, dialog presets.

### Internal Dependencies
- core (epic): game-mode bootstrap (`Init` at `OVT_OverthrowGameMode.c:1082-1087`, after the occupying faction; `PostGameStart` at `:219-223` → `SpawnGarrisons`), config component (player faction, difficulty, `mobileFOBOfficersOnly`), persistence system (`OVT_PersistenceTracking`), player manager (officer + camp storage, controllers).
- Occupying faction manager (base records, target registration, FOB placement rules), town manager (supporters), economy, vehicle manager, notification manager.
- Siblings: resistance/building (calls in), resistance/loadouts (officer gate), resistance/recruits (adjacent; hired-civilian prefab here is dead).

---

## Notes

**Discovered Information:**
- `DeployFOB`'s geometry re-validation exists precisely because the client action's check is advisory ("Silently fail - client should have already validated") — the officer check got no such server twin.
- The serializer deliberately drops wiped garrisons (live-but-empty groups are "found" but contribute no prefab) so a defeated garrison stays dead across a save.
- The job system's absence from this serializer is documented in-file as a decision (EPF's restore re-executed job stages; a fresh job board is the lesser evil until jobs get their own idempotent serializer).
- `OVT_ManageBaseAction` serves double duty: on a base flag it opens the base menu, on an FOB flag (no base controller within 10 m) it opens the FOB menu.

**Retrospective Assessment:**
- The record/serializer half is the healthiest part — modern, documented, idempotent, and test-covered; it postdates the EPF→vanilla migration.
- The interactive half (officer promotion, deploy/undeploy, garrison purchase) predates it and carries the mod's recurring defect signature: client-trusting RPCs, client-side payment, fall-through error paths, and position-equality record matching. The `GetOwner()` cast bug shows the completion handlers were moved from the controller onto the manager without updating the unsubscribe path.

---

*This retrospective plan was created by analyzing existing code. Use `/start-feature resistance/core` to begin making improvements.*
