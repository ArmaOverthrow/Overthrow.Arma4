# FOB (Mobile Forward Operating Base) - Implementation Plan (Retrospective)

**Status:** Implemented (Documented Retrospectively)
**Originally Implemented:** Unknown (mobile FOBs added in commit `f81c65d2`; static placeable FOBs became camps in `a7318324`)
**Documented:** 2026-08-09
**Last Updated:** 2026-08-09

---

## Executive Summary

The FOB is the resistance's shared forward base, implemented as a **vehicle with two forms**: a purchasable Mobile FOB truck (`OverthrowMobileFOB.et`, a reskinned M923A1) that officers drive into position and deploy, and a static deployed form (`OverthrowMobileFOBDeployedet`, physics-frozen, flying the FIA flag with tents/camo/hedgehogs) that acts as a base: map icon, fast-travel anchor, build/place permission zone (100 m), garrison purchase point, respawn ("set home") point, and cargo store. Deploy and undeploy **swap the two prefabs at the same transform** and asynchronously move the cargo through the initiating player's `OVT_ContainerTransferComponent`.

FOB *records* (`OVT_FOBData`) live in `OVT_ResistanceFactionManager.m_FOBs` — the `resistance/core` feature hosts the registry; this feature owns the truck, the deploy/undeploy state machine, map presence, and all FOB-specific gameplay rules.

**Note:** This is a retrospective implementation plan created by analyzing the existing codebase. The feature has already been implemented. It was carved out of `resistance/core`'s documentation on 2026-08-09 so FOBs can be tracked as their own feature.

---

## Goals

### Primary Goals
- A mobile base vehicle: buy (or upgrade into) a truck, drive anywhere legal, deploy into a shared forward base, pack it back up with cargo preserved.
- Strategic placement rules: not too close to enemy bases or radio towers; visible to players on the map before they commit (restricted-area overlay).
- One designated **priority FOB** always visible on the map as the movement's current objective anchor.
- Officer-gated (configurable via `mobileFOBOfficersOnly`).

### Success Criteria
- [x] Truck purchasable (5000) at vehicle shops; upgradeable from an M923A1 transport (10000)
- [x] Deploy validates distance to every base (`baseCloseRange + 50`) and radio tower (70 m), client-side pre-check + server re-validation
- [x] Async cargo transfer both directions with progress UI; disconnect-mid-transfer recovery
- [x] Undeploy collects containers within 75 m into the truck, then cleans the area
- [x] Map icons (`fob`/`fob_priority`), priority FOB visible at every zoom, restricted-area overlay matches the enforced deploy radii (BUG-070)
- [x] Fast travel to any FOB; build/place within 100 m; garrisons; set-home respawn
- [x] Records replicate (JIP + delta RPCs) and persist idempotently (`OVT_PersistedFOB`)
- [ ] Officer gate enforced server-side (BUG-122 — currently client-side theatre with an ungated upgrade path)
- [ ] Failure paths conserve records and garrisons (BUG-121, BUG-125)
- [ ] Area cleanup respects ownership/association (BUG-124)

---

## Current Architecture

### Key Components

| File | Role |
|---|---|
| `Scripts/Game/GameMode/Managers/Factions/OVT_ResistanceFactionManager.c` | Hosts `OVT_FOBData` + `m_FOBs` registry (`:89`), deploy `:466-555` / undeploy `:557-621` state machine, completion handlers `:1711-1887`, register/unregister `:1106-1127`, priority FOB `:625-659`, `CleanupFOBArea` `:1630-1682`, shared deploy constants `:60-64` (`FOB_DEPLOY_BASE_BUFFER = 50`, `FOB_DEPLOY_TOWER_RANGE = 70`) |
| `Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c` | Legacy client→server funnel: `RpcAsk_DeployFOB` `:1234`, `RpcAsk_UndeployFOB` `:1253`, `RpcAsk_SetPriorityFOB` `:1994` (unauthenticated — BUG-123), `RpcAsk_AddGarrisonFOB` `:1211` |
| `Scripts/Game/Components/Controller/OVT_ContainerTransferComponent.c` | Per-player (on `OVT_OverthrowController`) cargo mover: `TransferStorage`, `UndeployFOBWithCollection` `:231-278` (75 m radius, 100/batch, skips ground weapons, deletes emptied containers), dead `TransferStorageForDeployment` `:76-139` (BUG-128) |
| `Scripts/Game/GameMode/Managers/OVT_InventoryManagerComponent.c:207-215` | `CollectContainersToVehicle` batching primitive |
| `Scripts/Game/UserActions/OVT_DeployFOBAction.c` | Client pre-check: officer gate `:17`, base range `:32-42`, tower range `:45-54` |
| `Scripts/Game/UserActions/OVT_UndeployFOBAction.c`, `OVT_SetPriorityFOBAction.c`, `OVT_SetHomeAction.c`, `OVT_ManageBaseAction.c` | Undeploy (officer gate only), priority designation, respawn-point set, FOB menu open (within 10 m of a record) |
| `Scripts/Game/UI/Map/OVT_MapIcons.c:664-692` | Icon per record; `fob_priority` + always-visible when `isPriority`; color hardcoded "FIA" |
| `Scripts/Game/UI/Map/OVT_MapRestrictedAreas.c` | Red/blue no-deploy circles at exactly the enforced radii (post-BUG-070) |
| `Scripts/Game/UI/Context/OVT_MapContext.c:121-123` | Fast travel: any FOB within `MAX_FOB_TRAVEL_DIS = 40` m, any player |
| `Scripts/Game/UI/Context/OVT_FOBMenuContext.c` + `UI/Layouts/Menu/FOBMenu.layout` | Garrison purchase menu (shared with camps) |
| `Scripts/Game/Persistence/Serializers/Components/OVT_ResistanceManagerSerializer.c` | `OVT_PersistedFOB` `:22-31`; serialize `:123-142`; idempotent re-apply via `ApplyPersistedResistance` (`OVT_ResistanceFactionManager.c:244-312`) |

### Prefabs & Config

- **Truck:** `Prefabs/Vehicles/Wheeled/M923A1/OverthrowMobileFOB.et` (+ `OverthrowMobileFOBCargoCanvas.et` carrying `OVT_DeployFOBAction` on `door_rear`, Duration 5). Named `#OVT-MobileFOB`.
- **Deployed:** `OverthrowMobileFOBDeployed.et` — `RigidBody Static`, FIA faction affiliation, vehicle-node components disabled, child composition (FIA flagpole, tent foundation, 2 hedgehogs, large camo net) + `OverthrowMobileFOBDeployedCargoCanvas.et` (`OVT_UndeployFOBAction`, `OVT_SetPriorityFOBAction`, `OVT_SetHomeAction`).
- **Flag prefabs** (`Prefabs/Structures/Military/Flags/FOB_V1_FIA.et`, `BaseFlag_FIA.et`) carry `OVT_SetHomeAction` + `OVT_ManageBaseAction` and the **empty** `OVT_ResistanceFOBControllerComponent` (dead marker, queried by nothing).
- **Game-mode config** (`Prefabs/GameMode/OVT_OverthrowGameMode.et:185-197`): `m_pMobileFOBPrefab`, `m_pMobileFOBDeployedPrefab`, and an `M923A1_transport → OverthrowMobileFOB` upgrade at 10000.
- **Pricing:** `Configs/Pricing/vehiclePrices.conf:88-93` — 5000, legal, `PARKING_TRUCK`.
- **Config knobs** (`OVT_OverthrowConfigComponent.c:32,37`, replicated): `mobileFOBOfficersOnly` (default true), `fobItemLimit` (default 100, shared with bases).
- **Notifications:** `Configs/overthrowBroadcastMessages.conf` — `PriorityFOBSet`, `TooCloseBase`, `TooCloseToRadioTower`, `FOBUndeployed`, `FOBUndeployFailed`, `FOBOperationInProgress`. The `DeployedFOB` tag has **no preset** (BUG-120).

### Data Model

`OVT_FOBData` (`OVT_ResistanceFactionManager.c:22-37`): `persistentId` ("FOB_<unixtime>_<rand>"), `name` (never assigned — BUG-119), `location`, `owner` (purchaser's persistent UID), `isPriority`, `garrison[]` (persisted prefab list), `garrisonEntities[]` (live, server-only, `[NonSerialized]`), `id` (`[NonSerialized]`, written only by persistence re-apply, read by nothing).

**In-flight operation state** (`:95-106`) is a set of bare manager members (`m_pCurrentMobileFOB`, `m_pCurrentDeploymentSource/Target`, transfer refs, `m_iFOBOperationPlayerId`) guarded by a single busy-check (`:521`/`:578` → `FOBOperationInProgress` refusal). One FOB operation server-wide at a time, by convention.

### Data Flow — Deploy

1. Player action on the truck's rear door → client pre-checks (officer if `mobileFOBOfficersOnly`; ≥ `baseCloseRange+50` from every base, ≥ 70 m from every radio tower) → `OVT_Global.GetServer().DeployFOB(truck)`.
2. `RpcAsk_DeployFOB(rplId, playerId)` → server resolves the real sender (`ResolveSenderPlayerId`, anti-spoof) → `OVT_ResistanceFactionManager.DeployFOB`.
3. Server re-validates geometry (targeted `TooCloseBase`/`TooCloseToRadioTower` refusals), **validates transfer availability before spawning** (the BUG-046 fix), takes the concurrency guard, then `SpawnVehicleMatrix(m_pMobileFOBDeployedPrefab, truckTransform, truckOwner)` and starts the async cargo transfer.
4. `OnFOBDeploymentComplete` (`:1814-1848`): delete truck → `RegisterFOB` (mint persistentId, broadcast `RpcDo_RegisterFOB`) → unsubscribe from the *stored* transfer component → clear op state. `OnFOBDeploymentError` (`:1852-1887`) registers the FOB anyway and still deletes the truck — deliberately biased against stranding two vehicles.
5. Disconnect recovery (`:154-176`): the initiator disconnecting mid-transfer drives the error handlers so shared op state can't wedge.

### Data Flow — Undeploy

1. Action (officer gate only, no geometry check) → `RpcAsk_UndeployFOB` → `UndeployFOB`: validate-before-spawn, concurrency guard, spawn truck at the FOB's transform with **physics deactivated** (two-body collision avoidance), remove FOB from vehicle registry.
2. `UndeployFOBWithCollection` → `CollectContainersToVehicle` (75 m, empties containers into the truck, deletes emptied ones, skips ground weapons).
3. `OnFOBCollectionComplete` (`:1711-1766`): `CleanupFOBArea(pos, 75)` — deletes **every** placeable/buildable in radius, no association filter (BUG-124) — delete deployed FOB, re-activate truck physics, `FOBUndeployed` notification (sent to the truck *owner*, not the actor — BUG-127), `UnregisterFOB` (10 m tolerance, nearest wins — the BUG-046 ghost fix). Garrison AI is never despawned (BUG-125).
4. `OnFOBCollectionError` (`:1770-1810`): deletes the deployed FOB but **never unregisters the record** (BUG-121) and shares the garrison orphaning.

### Map Presence

- One icon widget per record (`OVT_MapIcons.c:664-692`): `fob` decluttered at the zoom ceiling; `fob_priority` at range 0 (always visible). Icons colored by hardcoded faction key "FIA" (ignores `m_sPlayerFaction`). Imageset: `UI/Imagesets/overthrow_mapicons.imageset:59,95`.
- Restricted-area overlay (`OVT_MapRestrictedAreas.c`) draws the *exact* enforced no-deploy radii: red around occupying bases/towers, blue around resistance-held ones (which block deployment too).
- Fast travel: any FOB within 40 m anchors it, for any player — FOBs are shared; no privacy concept (camps have one).
- FOBs have no display name anywhere (BUG-119).

### Integration Points

- **resistance/core:** hosts the registry, persistence serializer, JIP streaming and the garrison spawn/charge machinery (`AddGarrisonFOB` → `SpawnGarrisonFOB`, server-charged since BUG-047/064). This feature is the largest client of core's record layer.
- **resistance/building:** `m_bBuildAtFOB` buildables (5 in `Configs/Resistance/buildables.conf`) legal within `MAX_FOB_BUILD_DIS = 100` m; any placeable within `MAX_FOB_PLACE_DIS = 100` m — **no ownership check** (camps require owner match). `OVT_ItemLimitChecker` counts a 150 m sphere filtered by stored FOB association; FOB and BASE share `fobItemLimit` (100). `FindNearestBase` stamps association with **no distance cap**.
- **occupying:** every FOB is an omnisciently-known `ATTACK` target (`OVT_OccupyingFactionManager.UpdateKnownTargets:1316-1326`), threat-scored like a broadcast tower (`GetThreatByLocation:1047-1050`). Open BUG-109: specops sent at an FOB do nothing on arrival.
- **resistance/wanted-system:** driving the Mobile FOB = wanted level 4, via hardcoded prefab GUID `{E6A31A5A6EA0AF04}` (`OVT_PlayerWantedComponent.c:694-700`).
- **vehicles:** both forms are registered player vehicles (`SpawnVehicleMatrix`, `m_aVehicles`, ownership inheritance, despawn/respawn managed); the deployed FOB persists through the vanilla `Vehicles` persistence group, its built structures self-spawn from the `Overthrow` group.
- **real-estate/spawn:** `OVT_SetHomeAction` on the deployed FOB → home position → respawn point (`OVT_SpawnLogic.c:787`).
- **economy:** purchase price, upgrade cost (client-charged — BUG-122), garrison costs.

---

## Implementation Details

### Phase 1: Vehicle Pair & Deploy/Undeploy State Machine (COMPLETED)
Truck/deployed prefab pair on the game-mode config; prefab-swap-at-transform deploy/undeploy with async container transfer through the initiating player's controller; completion/error/disconnect handlers on the manager.

### Phase 2: Base Behaviours (COMPLETED)
Record registration/broadcast, map icons + priority FOB, fast travel, build/place proximity permission, garrison purchase (shared FOB menu), set-home respawn, item limits.

### Phase 3: Hardening Round 1 (COMPLETED — BUG-046/047/070 era)
Validate-before-spawn on both operations; sender resolution on deploy/undeploy/garrison RPCs; 10 m-tolerance unregistration; server-side garrison charging; restricted-area overlay matched to the enforced radii; disconnect-mid-transfer recovery; shared deploy-radius constants.

### Phase 4: Remaining Hardening (NOT STARTED)
The open defect set filed 2026-08-09 from this discovery: BUG-119…128 (see Current State) plus the pre-existing open BUG-109 (occupying side) and BUG-116 (deployed-FOB cargo in scope of the restart item-loss investigation).

---

## Key Technical Decisions

### Decision 1: FOB mobility as a prefab swap, not a packable structure
**Context:** The FOB must keep its cargo through deploy/undeploy and read as a vehicle while mobile, a base while deployed.
**Implementation:** Two prefab forms swapped at the same transform; the deployed form is still a `Vehicle` (physics-frozen, controllers disabled) so it inherits vehicle ownership, persistence and registry behaviour for free.
**Trade-offs:** Free persistence/ownership plumbing; but "a base that is secretly a vehicle" surprises every system that special-cases vehicles (wanted level, despawn management), and the swap needs the async-transfer state machine below.

### Decision 2: Async cargo transfer through the initiating player's controller, op state on the manager
**Context:** Transfers are slow enough to need batching and progress UI; progress events already flow per-player through `OVT_ProgressEventHandler`.
**Implementation:** `OVT_ContainerTransferComponent` on the player's `OVT_OverthrowController` does the moving (the one part of the FOB feature migrated to the new controller architecture); the manager holds in-flight entities in bare members with a single busy-guard and finishes the swap in completion callbacks.
**Trade-offs:** Progress UI for free and per-player attribution; but op state is a global singleton's members — one operation server-wide, and every failure path must manually restore invariants (the source of BUG-121/125/127).

### Decision 3: Error paths biased toward destruction over duplication
**Context:** BUG-046 established that fall-through paths leaving *both* vehicles alive enable cargo duplication.
**Implementation:** Deploy errors still register the FOB and delete the truck; undeploy errors delete the deployed FOB "to prevent it being stuck".
**Trade-offs:** No duplication exploit; but the undeploy error path destroys without unregistering (BUG-121) — conservation of *records* wasn't part of the invariant.

### Decision 4: Position-keyed wire identity, persistentId only for saves
**Context:** Records need stable identity across saves; RPCs wanted a cheap wire format.
**Implementation:** `persistentId` matches saves; every live RPC (`RpcDo_RemoveFOB`, `RpcDo_SetPriorityFOB`, `SetPriorityFOB`) matches by position with 10 m tolerance, nearest wins.
**Trade-offs:** Works; but two FOBs within 10 m are indistinguishable on the wire, and `persistentId` — which both sides already hold — would be exact. Standing debt for any future rework.

### Decision 5: Deploy exclusion zones enforced against *all* bases, drawn on the map
**Context:** Players need to know where deployment is legal before driving there.
**Implementation:** Server validates distance against every base and tower regardless of controlling faction; `OVT_MapRestrictedAreas` draws exactly those radii from the same shared constants (`FOB_DEPLOY_BASE_BUFFER`, `FOB_DEPLOY_TOWER_RANGE`).
**Trade-offs:** Overlay can't drift from enforcement (the BUG-070 lesson: derive both from one constant). Blue (friendly) circles blocking deployment surprises some players but prevents FOB-stacking on captured bases.

---

## Current State

### What's Working
Happy-path deploy/undeploy with cargo conservation and progress UI; server-side geometry validation with targeted refusals; concurrency + disconnect protection; map icons, priority FOB, restricted-area overlay; fast travel; build/place at FOBs with item limits; garrison purchase (server-charged); JIP + idempotent persistence of records; garrison replay on load.

### Known Issues (filed 2026-08-09 from this discovery)
- **BUG-122 (high):** officer gate is client-side theatre; the M923A1 upgrade path bypasses it entirely with client-side payment.
- **BUG-124 (high):** `CleanupFOBArea` deletes anyone's placeables/buildables within 75 m — undeploy-adjacent griefing.
- **BUG-121 (medium):** failed undeploy leaks a permanent ghost record (phantom icon/fast-travel anchor).
- **BUG-125 (medium):** garrison AI orphaned on undeploy (both paths); lost without refund on next load.
- **BUG-123 (medium):** `RpcAsk_SetPriorityFOB` unauthenticated.
- **BUG-126 (medium):** empty-registry `GetNearestFOB` returns zero vector → fast-travel/build legal near world origin.
- **BUG-119/120 (low):** FOBs nameless; deploy announcement never shows (missing notification preset, orphaned localization).
- **BUG-127/128 (low):** undeploy confirmation to the wrong player; deploy progress shows the generic label (dead `TransferStorageForDeployment`).

### Related open bugs elsewhere
- **BUG-109** (occupying): specops sent at an FOB do nothing on arrival.
- **BUG-116** (persistence, in progress): restart item loss; deployed-FOB cargo (`OverthrowMobileFOBDeployed.et` re-creation appears in the load log) is in scope.

### Technical Debt
- Dead code: `OVT_UndeployFOBAction_New.c` (abandoned controller-migration sketch, `FindNearbyMobileFOB` always null), `OVT_ResistanceFOBControllerComponent` (empty class on two flag prefabs, queried by nothing — `Scripts/Game/Controllers/README.md:46-47` describes capabilities it doesn't have), `OVT_StorageProgressUIContext.c` (superseded by `OVT_ProgressInfo`), `fob.id` (+ the serializer comment claiming insertion sites set it).
- Documentation drift: `Scripts/Game/GameMode/Placeables/README.md:12-13` documents a nonexistent `OVT_PlaceableFOBHandler`.
- Orphan localization keys: `OVT-Place_FOB`, `OVT-Place_FOB_Description`, `OVT-ManageFOB`, `OVT-MobileFOBHelp`, `OVT-Msg-HasPlacedFOB` (the last un-orphans with BUG-120).
- Magic numbers duplicated across files: 75 (cleanup/collection radius ×2 files), 100 (place/build distance ×3 files), 40 (fast travel), 10 (wire-match tolerance ×3 sites), 150 (item-count sphere). Only the two deploy radii were promoted to shared constants.
- Hardcoded "FIA": map icon colors (`OVT_MapIcons.c` — camps too) and the wanted-system prefab GUID.
- Deploy/undeploy still routes through legacy `OVT_PlayerCommsComponent` (only the transfer half migrated to `OVT_OverthrowController`); per project policy, any new FOB client→server op must go on the controller.
- `LocalPlayerIsOfficer` (`OVT_PlayerManagerComponent.c:487-493`) derefs `player.isOfficer` unguarded, reached from every FOB action's `CanBeShownScript` (noted in BUG-122).

---

## Future Enhancements

### High Priority
- [ ] Fix BUG-122 (server-side officer gate + upgrade path) and BUG-124 (association-filtered cleanup) — the two player-facing hazards.
- [ ] Fix BUG-121 + BUG-125 together: make the undeploy error path despawn garrison → unregister record → decide truck fate.

### Medium Priority
- [ ] BUG-123 (authenticate priority RPC), BUG-126 (not-found contract for nearest-FOB queries).
- [ ] Migrate FOB RpcAsks off `OVT_PlayerCommsComponent` onto a controller component (finish what `OVT_UndeployFOBAction_New` started, then delete it).
- [ ] Key wire operations by `persistentId` instead of position+tolerance.

### Low Priority / Nice to Have
- [ ] BUG-119/120/127/128 (names, deploy announcement, notification target, progress label).
- [ ] Per-operation context object instead of shared manager members (would allow concurrent FOB ops and simplify error handling).
- [ ] Color map icons by `m_sPlayerFaction`; reference `m_pMobileFOBPrefab` instead of hardcoded GUID/path strings.
- [ ] Delete dead code (`OVT_UndeployFOBAction_New`, empty FOB controller component, `OVT_StorageProgressUIContext`, `fob.id`); fix the two stale READMEs.

---

## Testing

### Current Coverage
- Persistence tier: FOB records round-trip via `ApplyPersistedResistance` (same-session + save→dirty→re-apply suites); idempotence is an explicit serializer property.
- Nothing covers the deploy/undeploy state machine, map presence, or the gameplay rules.

### Testing Gaps
- Logic-tier candidates (world-free): deploy-radius math against base/tower sets (shared constants make this pure); priority-FOB exclusivity after `RpcDo_SetPriorityFOB`; `GetNearestFOB(Data)` empty-registry contract (pins BUG-126's fix); `FindPersistedFOB` matching rules; unregister nearest-within-10 m selection.
- Not automatable in the test world: the async transfer state machine (needs a live player controller), physics freeze/unfreeze, map icon rendering, fast travel, MP officer gating — play-test items (see `tools/launch-server.sh` for the MP loop).

---

## Documentation

### Current Documentation
- This retrospective plan; deploy/undeploy callback wiring also summarized in `resistance/core`'s docs (now pointing here).

### Documentation Needs
- In-game help: `OVT-MobileFOBHelp` exists in localization but is unreferenced — if FOB help is wanted, wire it (and fact-check it against code per project policy); otherwise delete the key.
- `Scripts/Game/Controllers/README.md` and `Scripts/Game/GameMode/Placeables/README.md` both describe FOB machinery that doesn't exist — correct alongside any FOB work.

---

## Dependencies

### External Dependencies
- Vanilla: `Vehicle`/physics activation, `SCR_AIGroup` + waypoints, `RplComponent`/RplId, `ActionsManagerComponent`, faction manager, map framework.

### Internal Dependencies
- `resistance/core` (registry, serializer, garrison machinery, officer checks), `resistance/building` (proximity permission + item limits), vehicle manager (spawn/ownership/registry), config component (`mobileFOBOfficersOnly`, `fobItemLimit`, difficulty `baseCloseRange`), occupying faction manager (base/tower sets for validation; known-target registration), economy (prices/charging), notification manager, real-estate manager (home position), `OVT_OverthrowController` (`OVT_ContainerTransferComponent`), persistence system.

---

## Notes

**Discovered Information:**
- The deployed FOB being a physics-frozen *vehicle* (not a structure) is the load-bearing trick: ownership, registry, despawn/respawn and persistence all ride the vehicle path.
- The BUG-046 fix generation is visible in source: validate-before-spawn on both ops, stored-component unsubscribe (replacing the always-failing `OVT_OverthrowController.Cast(GetOwner())`), 10 m-tolerance unregistration, disconnect recovery. `resistance/core`'s 2026-08-02 Known Issues describe the *pre-fix* state — this doc supersedes it for FOBs.
- Deploy errors deliberately complete the swap (register FOB, delete truck) rather than roll back — anti-duplication bias, at the cost of the record-conservation gaps now filed.
- `OVT_DeployFOBAction`/`OVT_UndeployFOBAction` share only a *name* with the occupying epic's deployment system (`occupying/deployments`) — unrelated machinery.

**Retrospective Assessment:**
- The record/persistence half is healthy (modern serializer, idempotent re-apply, test-covered) and the geometry validation + overlay pairing is exemplary (one constant, both consumers).
- The remaining defect surface is concentrated in the *edges* of the state machine (error paths, notification targets, cleanup filters) and in gates that were never given server twins — the mod's recurring signature, here in its post-BUG-046 residue form.

---

*This retrospective plan was created by analyzing existing code. Use `/start-feature resistance/fob` to begin making improvements.*
