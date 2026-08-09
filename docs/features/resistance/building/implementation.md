# Resistance Building - Implementation Plan (Retrospective)

**Status:** Implemented (Documented Retrospectively)
**Originally Implemented:** Unknown (inherited from early Overthrow Reforger development; removal mode + item limits added in `841d033`/`0f6789a`)
**Documented:** 2026-08-02
**Last Updated:** 2026-08-02

---

## Executive Summary

Building is how players turn money into physical resistance infrastructure: small **placeables** (posters, camp tents, ammoboxes, sandbags, furniture — 8 entries) placed first-person near owned houses/camps/FOBs/captured bases, and larger **buildables** (guard towers, recruitment/medical tents, garages, helipads — 7 entries) constructed at camps/FOBs/towns/captured bases through a top-down build camera. Both flows are client-driven ghost-preview UIs (`OVT_PlaceContext`/`OVT_BuildContext`) that validate location rules, per-location item limits and cost locally, then fire a fire-and-forget RPC; the server spawns the prefab, stamps ownership + a camp/FOB/base association onto a marker component, runs an optional config handler (camp registration, town support modifiers), awards XP, and registers the entity with vanilla persistence. A removal mode lets owners (and officers, for anyone's items) delete placed/built objects by looking at them.

**Note:** This is a retrospective implementation plan created by analyzing the existing codebase. The feature has already been implemented. Construction is **instant** — despite the "fund construction" framing in old docs there is no build progress, funding pool, or partially-built state anywhere in the code.

---

## Goals

### Primary Goals
- Let any player decorate and fortify their own locations cheaply, and grow camps/FOBs into functional bases (healing, recruiting, garages) via buildables.
- Keep placed/built infrastructure persistent across sessions and visible to all players.
- Enforce authored placement rules (near/away from towns, bases, camps) and per-location item caps for server performance.

### Success Criteria
- [x] Ghost-preview placement with rotation, prefab cycling, wall snapping (placeables) and a bounded top-down build camera (buildables)
- [x] Server-side spawn with ownership, base association, handler dispatch, XP reward, navmesh rebuild and persistence tracking
- [x] Placed/built entities and their component state survive save/load (vanilla persistence, SelfSpawn)
- [ ] Server-authoritative validation (there is none — position, indices, limits and affordability are all client-trusted; see Known Issues)
- [ ] Removal mode working in multiplayer (broken twice over on dedicated servers — see Known Issues)

---

## Current Architecture

### Key Components

**UI contexts** (client, `Scripts/Game/UI/Context/`):
- `OVT_PlaceContext.c` (756 L) — place menu (15-card paged grid, remove card first), `CanPlace()` rule evaluation, first-person ghost via local spawn of the real prefab, `DoPlace()` → server RPC, removal mode (raycast + highlight + `RemovePlacedItem`).
- `OVT_BuildContext.c` (771 L) — same shape for buildables plus a spawned `SCR_ManualCamera` build camera (WASD pan clamped to 50 m of the player, zoom 10–25 m above ground), ghost via `SCR_PrefabPreviewEntity` with preview material.
- `OVT_PlaceMenuCardComponent.c` / `OVT_BuildMenuCardComponent.c` — cards; disabled cards show the restriction reason; the remove card starts removal mode. Menus open from `OVT_MainMenuContext` (no officer gate — any player can build).

**Config model** (`Scripts/Game/Configuration/`, instances in `Configs/Resistance/placeables.conf` + `buildables.conf`):
- `OVT_Placeable` — name/title/description, prefab array, preview, `m_iCost`, `m_iRewardXP`, location flags (`m_bIgnoreLocation`, `m_bAwayFromTownsBases`, `m_bAwayFromCamps`, `m_bAwayFromBases`, `m_bNearTown`, `m_bPlaceOnWall`, `m_bRandomizePrefab`, `m_bAssociateWithNearest`), optional `handler`.
- `OVT_Buildable` — same plus `m_bBuildAtBase/InTown/InVillage/AtFOB/AtCamp` flags and a dead `m_aFurniturePrefabs` array (never read).
- `OVT_PlaceableHandler` (`Scripts/Game/GameMode/Placeables/`) — `OnPlace(entity, playerId)` hook: `OVT_PlaceableCampHandler` → `RegisterCamp` (resistance/core), `OVT_PlaceableSupportModHandler` → town support modifier (poster); a false return deletes the entity and aborts the charge.

**Marker components** (`Scripts/Game/Components/`): `OVT_PlaceableComponent` / `OVT_BuildableComponent` — field-for-field twins carrying `m_sPlaceableType`, owner persistent ID and the associated base ID + `EOVTBaseType`. Set server-side only; **no replication** of any field.

**Limits:** `Scripts/Game/Utilities/OVT_ItemLimitChecker.c` — resolves which location a position belongs to (owned house 30 m → own camp 75 m → FOB 100 m → captured base within `baseRange`; build variant checks base → town → camp 50 m → FOB 100 m), then counts items by sphere query: houses count *everything* in radius, camps/FOBs/bases count by stored association. Limits from `Overthrow_Config.json` (`houseItemLimit` 20 / `campItemLimit` 40 / `fobItemLimit` 100; ≤0 = unlimited).

**Server side:** `OVT_PlayerCommsComponent.c:676-708` (`RpcAsk_PlaceItem`/`RpcAsk_RemovePlacedItem`/`RpcAsk_BuildItem` — thin pass-throughs) → `OVT_ResistanceFactionManager.PlaceItem()` (:599), `BuildItem()` (:670), `RemovePlacedItem()` (:720), `FindNearestBase()` (:1335), `CleanupCampObjects()` (:1306).

### Data Flow

**Place:** card click → `StartPlace` (local `CanPlace` + affordability) → local ghost spawn → `DoPlace` re-validates at ghost transform → `OVT_Global.GetServer().PlaceItem(placeableIndex, prefabIndex, pos, angles, playerId)` → server spawns prefab, stamps owner + nearest association (`m_bAssociateWithNearest`), runs handler (failure = delete + no charge), **takes money** (`TakePlayerMoney`, ResistanceFactionManager.c:646), rebuilds navmesh, fires `m_OnPlace` (→ `OVT_SkillManagerComponent.OnPlace` XP), sets `OVT_PlayerOwnerComponent`, `OVT_PersistenceTracking.Track()`. Client immediately respawns the ghost for repeat placement.

**Build:** identical shape, but the **client** takes the money (`OVT_BuildContext.c:533` → generic `TakePlayerMoney` RPC) and server `BuildItem` never charges; one build per menu open (context closes after).

**Remove:** raycast 50 m → `CanRemoveItem` (owner or officer, client-side) → `RemovePlacedItem(hitEntity.GetID(), playerId)` → server re-checks ownership/officer and deletes. No refund.

**Persistence:** placed/built entities are runtime spawns with no world-file instance, so `Configs/Systems/Persistence/Overthrow.conf` gives both component classes `SelfSpawn 1` configurations (prefab + transform via `GenericEntitySerializer`; buildables also `SavePrefabChildren`; placeables additionally persist `OVT_PlayerOwnerComponent` and inventory storage — ammobox contents survive). `OVT_PlaceableComponentSerializer` / `OVT_BuildableComponentSerializer` round-trip owner + association (versioned, positional binary, enum ordinals — `EOVTBaseType` must never be reordered). Association is deliberately stored, not re-derived, so loads don't re-point objects at whatever is nearest now. Camp *records* ride the resistance-manager serializer (resistance/core); the tent entity persists here.

**JIP/MP:** entities replicate as ordinary dynamic spawns (prefabs carry `RplComponent`, mostly via vanilla parents); component fields do **not** replicate, which removal mode wrongly depends on client-side.

### Integration Points
- **resistance/core** (sibling): `OVT_PlaceableCampHandler` → `RegisterCamp`; camp/FOB data (`GetNearestCampData`/`GetNearestFOBData`/`GetNearestCamp`/`GetNearestFOB`) drives every location rule; `RemoveCamp`/`RemoveOldCamp` → `CleanupCampObjects` deletes associated items by component association (75 m sphere); `CleanupFOBArea` does the radius equivalent for mobile-FOB undeploy. FOB deployment itself is a vehicle flow in core, not a placeable (the README's `OVT_PlaceableFOBHandler` does not exist).
- **skills epic:** `m_OnPlace`/`m_OnBuild` → `OVT_SkillManagerComponent` XP (`m_iRewardXP`).
- **economy epic:** `GetPlaceableCost`/`GetBuildableCost` = `m_iCost` × difficulty multipliers (`OVT_OverthrowConfigComponent.c:250-258`); charges via `OVT_EconomyManagerComponent.TakePlayerMoney` (clamps at zero — see Known Issues).
- **occupying epic:** `OVT_BaseData.IsOccupyingFaction()` gates building at captured bases; `baseRange` bounds base placement checks.
- **towns:** poster handler adds town support modifiers; town size/range constants shape near/away rules.
- **real-estate:** `GetNearestOwned` defines the house placement bubble.

---

## Implementation Details

### Phase 1: Config Model & UI Contexts (COMPLETED)
Placeable/buildable config classes + paged card menus + ghost preview + rotation/prefab cycling + build camera.

### Phase 2: Server Spawn Pipeline (COMPLETED)
Comms RPCs → `PlaceItem`/`BuildItem`: spawn, ownership, association, handler dispatch, XP, navmesh, tracking.

### Phase 3: Item Limits & Removal Mode (COMPLETED)
`OVT_ItemLimitChecker` + config limits (`841d033`); raycast removal mode with owner/officer permissions (`0f6789a`).

### Phase 4: Vanilla Persistence Migration (COMPLETED)
EPF SaveData (`OVT_PlaceableData`) replaced by SelfSpawn configs + component serializers (vanilla-persistence branch, 2026-08).

---

## Key Technical Decisions

### Decision 1: Client-side validation, fire-and-forget server spawn
**Context:** Validation needs UI feedback (restriction reasons on cards, hints).
**Implementation:** `CanPlace`/`CanBuild`, item limits and affordability all run on the client; the server RPCs validate nothing and cannot reject.
**Trade-offs:** Zero round-trip latency and rich feedback; but the server is fully client-trusted — indices, position, limits and funds are unverified (the epic's largest security hole; see Known Issues).

### Decision 2: Marker components + stamped association
**Context:** Limits, cleanup and removal permissions all need "whose is this and where does it belong".
**Implementation:** Twin `OVT_PlaceableComponent`/`OVT_BuildableComponent` stamped at spawn with owner UID and nearest camp/FOB/base; association persisted, never re-derived.
**Trade-offs:** Robust against camps moving/renaming; but nothing replicates, so client UIs can't actually read ownership, and `FindNearestBase` has no distance cap, so far-away items get nonsense associations.

### Decision 3: Two different ghost mechanisms
**Context:** Placeables preview at arm's length; buildables preview from a camera.
**Implementation:** PlaceContext locally spawns the *real* prefab (physics off, ~15 m trace); BuildContext uses `SCR_PrefabPreviewEntity` with the editor preview material (500 m trace from the build camera).
**Trade-offs:** The build ghost looks like a proper hologram; the place ghost looks like a finished object (its `SetMaterial` call is commented out, `OVT_PlaceContext.c:433`) and carries live components/actions while previewing.

---

## Current State

### What's Working
- Both flows work end-to-end in single-player/listen-host: menus, restriction feedback, ghosts, rotation, prefab cycling, camp registration, poster modifiers, XP, costs, item limits, persistence of placed/built entities with ownership and inventory contents, camp cleanup deleting associated objects.

### Known Issues
- **Buildables are never charged server-side** (`OVT_ResistanceFactionManager.c:670-717` has no `TakePlayerMoney`; the client pays via a generic client-priced RPC, `OVT_BuildContext.c:533` → `OVT_PlayerCommsComponent.c:615-628`): a modified client builds for free, and even legit flow is two unlinked RPCs (charge can land without build or vice versa). Asymmetric with `PlaceItem`, which charges server-side (:646).
- **Zero server-side validation** (`OVT_PlayerCommsComponent.c:682-685, :705-708`): `placeableIndex`/`prefabIndex` are indexed unchecked (`ResistanceFactionManager.c:602/:605/:673-674` — out-of-range values from a hostile client throw VM errors on the server), position/rules/limits are never re-checked, `playerId` is client-supplied, and there is no funds check — `DoTakePlayerMoney` clamps at 0 (`OVT_EconomyManagerComponent.c:958-964`), so a $1 player "buys" a $250 camp.
- **Removal mode is broken on dedicated servers, twice:** (1) the RPC ships a client-local `EntityID` (`OVT_PlaceContext.c:733`, `OVT_BuildContext.c:749`, `OVT_PlayerCommsComponent.c:687-696`) which the server resolves against *its* world (`ResistanceFactionManager.c:722`) — IDs don't match across machines (the project's own RplId rule); (2) owner UID never replicates, so client-side `CanRemoveItem` (`OVT_PlaceContext.c:688-700`) compares `""` to the player and non-officers can't even highlight their own items. The "#OVT-ItemRemoved" hint shows regardless of server outcome.
- **Pagination integer-division** (`OVT_PlaceContext.c:144`, `OVT_BuildContext.c:161`): `Math.Ceil(count / 14)` divides ints first — with the shipped 8 placeables/7 buildables `m_iNumPages` = 0 ("1/0" displayed), and clicking Next drives `m_iPageNum` to -1 (`NextPage` clamps to `m_iNumPages-1`), making `Refresh` read `m_aPlaceables[-14]` — out-of-bounds VM error. Any count of 14k+1..14k+13 also hides the tail items.
- **Dead duplicate rule + null base derefs in `CanPlace`:** the second `m_bAwayFromBases` block (`OVT_PlaceContext.c:315-327`) is unreachable (the first at :252-263 always returns); `GetNearestBase` results are dereferenced without null checks at :254, :278-281 and :362-364 (BuildContext guards, :283; `OVT_ItemLimitChecker` guards, :103/:122) — NPE on maps with no bases.
- **`FindNearestBase` has no distance cap** (`ResistanceFactionManager.c:1335-1390`): an item placed at a house associates with a camp/FOB/base arbitrarily far away; limit counting is then inconsistent (houses count by 30 m radius, camps/FOBs/bases by association inside fixed query spheres of 125/150/500 m — `OVT_ItemLimitChecker.c:188-213`), and cleanup only deletes associated items within 75 m (`:1306-1331`), so associations and counts drift.
- **Item limits are advisory** — enforced only in the client contexts; the server spawn path never counts.
- **Camp tent self-association:** `PlaceItem` stamps the association *before* the handler registers the new camp (`ResistanceFactionManager.c:626-634` vs `:637-644`), so a camp tent belongs to whatever pre-existed nearest, not itself.
- **`BuildItem` ignores handler failure** (`:706-709` — return unchecked; contrast `PlaceItem` :637-644 which deletes and aborts) — a failed handler still yields a spawned, tracked, XP-awarded structure.
- **Float equality on trace results** (`traceDis == 1`, `OVT_PlaceContext.c:590`, `OVT_BuildContext.c:606`) — works only because `TraceMove` returns exactly 1.0 on no-hit.

### Technical Debt
- `OVT_Placeable`/`OVT_Buildable` and their components/serializers are copy-paste twins — every fix lands twice by hand (the serializers document this deliberately; the contexts don't and have already diverged: null-guards, camera, charge site).
- `m_aFurniturePrefabs` (`OVT_BuildablesConfig.c:24`) is dead config; `Scripts/Game/GameMode/Placeables/README.md` documents a nonexistent `OVT_PlaceableFOBHandler` and still claims EPF persistence.
- Removal UI duplicated wholesale between the two contexts (raycast, highlight, permission check ×2).
- Magic numbers: 30/50/75/100 m radii (duplicated between contexts and `OVT_ItemLimitChecker`), 14 cards/page, 50 m removal raycast, +50 query padding, camera bounds 50 m/10-25 m.
- The place ghost is a live local entity with user actions, not a preview; its preview-material line is commented out.
- `takingMoney` client latch (`OVT_PlayerCommsComponent.c:615-628`) serializes all purchases through one reliable round-trip — a lost owner-RPC would wedge purchasing until reconnect.

---

## Future Enhancements

### High Priority
- [ ] Server-side validation for `RpcAsk_PlaceItem`/`RpcAsk_BuildItem`: bounds-check indices, re-run location rules + item limits, check funds before spawning, derive `playerId` from the RPC sender.
- [ ] Charge buildables in `BuildItem` server-side (mirror `PlaceItem`) and drop the client-side `TakePlayerMoney`.
- [ ] Fix removal mode for multiplayer: send `RplId` instead of `EntityID`, and either replicate owner UID or make highlight/permission a server query.
- [ ] Fix the pagination arithmetic (float ceil or `(count + 13) / 14`) in both contexts.

### Medium Priority
- [ ] Delete the dead `m_bAwayFromBases` block; null-guard `GetNearestBase` in `CanPlace`.
- [ ] Cap `FindNearestBase` by per-type radii so associations match the limit checker's location resolution.
- [ ] Check `BuildItem`'s handler return like `PlaceItem` does.
- [ ] Enforce item limits server-side (shared checker — it already lives outside the UI).

### Low Priority / Nice to Have
- [ ] Unify the twin component/config/context pairs behind shared base classes; extract the shared removal-mode code.
- [ ] Give the place ghost a preview material; consider previewing buildable furniture or deleting `m_aFurniturePrefabs`.
- [ ] Refund (part of) cost on removal; officer-remove notification to the owner.
- [ ] Rewrite `Placeables/README.md`.

---

## Testing

### Current Coverage
None. No test file references placeables, buildables, the contexts, the limit checker or the manager's place/build/remove methods. Adjacent: persistence suites cover the vanilla save/re-apply machinery these entities ride on, but no placed/built entity round-trips in a test.

### Testing Gaps
- **Logic-tier candidates (world-free, pure):** page-count arithmetic (`ceil(count/14)` for 0/7/8/14/15/29 — proves the int-division bug); `GetPlaceableCost`/`GetBuildableCost` rounding under multipliers; the limit-threshold decision (limit ≤ 0 unlimited, count == limit-1 allowed, count == limit blocked, per `EOVTBaseType`); `FindNearestBase` nearest-of-three selection and occupying-faction filtering (data lists are injectable); `CanPlace`/`CanBuild` flag precedence if extracted from the contexts into pure rule functions.
- The full place/build round trip (RPC → spawn → association → persistence) is assertable in the Init/Persistence tiers via the manager API (`PlaceItem`/`BuildItem` are directly callable server-side).
- Removal mode, ghosts, cameras and JIP visibility need play-testing (dedicated server + client — the known-broken paths above are exactly the ones automation can't reach).

---

## Documentation

### Current Documentation
- This retrospective plan; `Scripts/Game/GameMode/Placeables/README.md` (stale — nonexistent handler, EPF claims); serializer header comments (excellent — the best in-code record of the persistence contract).

### Documentation Needs
- The client-trust security posture should stay documented until fixed — it is easy to assume the server validates because the client UI does.

---

## Dependencies

### External Dependencies
- Vanilla: `SCR_PrefabPreviewEntity` + editor preview material, `SCR_ManualCamera`, `TraceMove`, `QueryEntitiesBySphere`, `SCR_AIWorld` navmesh rebuild, `RplComponent` on placeable prefab parents, vanilla persistence system (`SCR_PersistenceSystem.StartTracking`).

### Internal Dependencies
- resistance/core (camp/FOB records, RegisterCamp, cleanup), economy (costs, money), skills (XP), occupying/core (base data, faction check), towns (ranges, support modifiers), real-estate (owned houses), `OVT_PlayerCommsComponent` (RPC transport), `OVT_PersistenceTracking` + `Overthrow.conf` persistence configs.

---

## Notes

**Discovered Information:**
- `PlaceItem`/`BuildItem` take a `runHandler` flag, so other systems can spawn placeables without side effects — but nothing currently passes `false`.
- Placeable persistence configs also serialize inventory storage: a placed ammobox keeps its contents across saves.
- The remove card occupies slot 0 of every menu page, which is why the grid pages by 14.
- `RemoveCamp` (resistance/core) correctly uses `RplId` for the same delete-an-entity problem `RemovePlacedItem` gets wrong with `EntityID` — the fix pattern already exists in the same file.

**Retrospective Assessment:**
- The authored-config + handler model is genuinely extensible (posters and camps share one pipeline), and the vanilla-persistence migration left this feature with the cleanest persistence story in the mod.
- The client/server trust boundary is the weakest in the codebase: every place/build/remove decision the player sees enforced is enforced only on their own machine.
- The twin-class duplication is the root cause of most divergence bugs here; unifying it would halve the surface.

---

*This retrospective plan was created by analyzing existing code. Use `/start-feature resistance/building` to begin making improvements.*
