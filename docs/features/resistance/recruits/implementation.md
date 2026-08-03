# Resistance Recruits - Implementation Plan (Retrospective)

**Status:** Implemented (Documented Retrospectively)
**Originally Implemented:** Unknown (inherited from early Overthrow Reforger development; body-persistence route reworked in the 2026 `vanilla-persistence` epic)
**Documented:** 2026-08-02
**Last Updated:** 2026-08-02

---

## Executive Summary

Recruits are the resistance's AI manpower: a player pays to turn a civilian (or a tent at a camp) into a persistent, named AI squadmate that joins their vanilla group, earns XP for kills, carries whatever gear the player gives it, and survives disconnects and quit/continue — body, inventory and all. One manager owns everything: `OVT_RecruitManagerComponent` keeps the server-authoritative recruit table, mirrors it to clients, despawns bodies 10 minutes after the owner leaves (save-and-release through the persistence system) and asks for the exact same character back when the owner returns.

**Note:** This is a retrospective implementation plan created by analyzing the existing codebase. The feature has already been implemented.

---

## Goals

### Primary Goals
- Let a lone player grow into a squad: recruit civilians in towns and supporters at camp tents, up to a hard cap.
- Make recruits *persistent characters*, not consumables: name, XP/level, kills, and carried gear survive owner disconnects and server restarts.
- Command through the vanilla group/commanding systems rather than a parallel one (plus a custom "open inventory" command).
- Manage the roster through a dedicated UI (list, status, rename, dismiss, show on map).

### Success Criteria
- [x] Civilian recruiting (paid) and tent recruiting (half price + 1 town supporter) work end-to-end
- [x] Recruits join the owner's vanilla group and take commands; unwanted recruits read as civilians to enemy AI
- [x] Recruit records replicate to all clients including JIP; recruit UI works on clients
- [x] Records persist through the vanilla-persistence serializer; bodies round-trip with their inventory via `PersistenceSystem.RequestSpawn`
- [x] Death is permanent (record dropped, corpse stays lootable); dismissal is server-validated
- [ ] Renaming works in multiplayer (currently client-local — see Known Issues)
- [ ] Fast travel actually brings nearby recruits along (currently searches the wrong position — see Known Issues)

---

## Current Architecture

One manager owns the data model and the whole lifecycle; user actions and RPCs are thin entry points into it.

### Key Components
- `Scripts/Game/GameMode/Managers/OVT_RecruitManagerComponent.c` — the heart (2,219 lines, singleton on the game mode): recruit table, owner index, entity/RplId maps, add/remove/XP/rename, death handling, offline despawn timers, body respawn via the persistence system, group insertion, JIP payload and live broadcast RPCs.
- `Scripts/Game/Data/OVT_RecruitData.c` — the record: id, name, owner persistent id, kills/XP/cached level, skills map, training flags, last known position, `m_sBodyPersistenceId` (UUID of the stored body), `m_bIsOnline`, hometown town id. Level curve duplicated from `OVT_PlayerData`.
- `Scripts/Game/UserActions/OVT_RecruitCivilianAction.c` (+ `OVT_BaseCivilianUserAction.c`) — hold-action on a live civilian: client checks limit + money, charges `baseRecruitCost`, asks the server.
- `Scripts/Game/UserActions/OVT_RecruitFromTentAction.c` — action on a placed tent: client checks limit, town supporters and money (50% of `baseRecruitCost`), asks the server.
- `Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c` — client→server seams: `RpcAsk_RecruitCivilian` (:826), `RpcAsk_RecruitFromTent` (:847), `RpcAsk_DismissRecruit` (:1312), `RpcAsk_RequestFastTravelWithRecruits` (:972).
- `Scripts/Game/UI/Context/OVT_RecruitsContext.c` + `Scripts/Game/UI/Components/OVT_RecruitListEntryHandler.c` — management UI off the main menu: roster list with live/unconscious/offline status, details pane, rename dialog, dismiss confirm, show-on-map.
- `Scripts/Game/Persistence/Serializers/Components/OVT_RecruitManagerSerializer.c` — versioned record serializer (v2 = body id appended); the body itself is persisted by vanilla character persistence, not here.
- `Scripts/Game/Components/OVT_PlayerOwnerComponent.c` (+ its serializer) — "which player owns this character", used by commanding/inventory/loadouts ownership checks.
- AI-side integration: `Scripts/Game/AI/Modded/SCR_ChimeraAIAgent.c` and `SCR_AIRetreatFromTargetBehavior.c` (unwanted recruits perceive/are perceived as civilians), `Scripts/Game/Components/Player/OVT_PlayerWantedComponent.c` (per-recruit wanted state; stealth inherited from the owner's skills), `Scripts/Game/Commanding/Commands/OVT_OpenInventoryCommand.c` (owner-only inventory access via temporary possession).
- Config: `OVT_DifficultySettings.baseRecruitCost` (default 250, replicated to clients in the config JIP payload — `OVT_OverthrowConfigComponent.c:558`); `MAX_RECRUITS_PER_PLAYER = 16` hard constant (`OVT_RecruitManagerComponent.c:11`); recruit prefab `Character_CIV_Recruit.et` wired on the game-mode prefab.

### Data Flow
1. **Recruit (civilian):** client action validates limit + money, charges the local player, `RpcAsk_RecruitCivilian(rplId, playerId)` → server `RecruitCivilian()`: re-checks the limit, enables the wanted system, sets `OVT_PlayerOwnerComponent`, `AddRecruit()` (record, name extracted from the civilian's identity, hometown = nearest town, body id captured), sets player faction, vanilla `RequestAddAIAgent` into the owner's group.
2. **Recruit (tent):** client validates limit + supporters + money, `RpcAsk_RecruitFromTent(tentPos, playerId)` → server takes 1 supporter + 1 population from the nearest town, spawns the recruit prefab beside the tent with a civilian loadout, then runs the same `RecruitCivilian()` path.
3. **Live mirroring:** three broadcast RPCs (`RpcDo_RecruitCreated/Removed/Updated`) keep client tables current; the component's `RplSave`/`RplLoad` ships the whole table (with per-recruit RplIds and skills) to JIP clients. Clients resolve entities through `m_mRplIdToRecruit`; the server uses `m_mEntityToRecruit`.
4. **XP:** occupying-faction kills by a recruit award 10 XP + kill count (`OnAIKilled`, faction keys hardcoded `US`/`USSR`); level = `1 + 0.1*sqrt(XP)`; level-ups notify the online owner. Recruit-vs-recruit kills are excluded.
5. **Death:** the game mode's universal `OnCharacterKilled` → record removed permanently, body id cleared first so nothing can respawn the corpse; the body stays in the world as lootable remains; owner notified.
6. **Owner disconnect:** positions + body ids synced immediately; a 10-minute timer (`OFFLINE_DESPAWN_TIME`) then `ReleaseRecruitBody()`s each body — write to storage, capture id, untrack *keeping* the data, delete the entity (vanilla's disconnecting-player idiom).
7. **Owner return:** spawn-logic's group-created event → 3 s delay + leader-check retry loop → `RespawnPlayerRecruits()`: per recruit, prefer `RequestPersistedRecruitBody()` (async `PersistenceSystem.RequestSpawn` filtered to the stored UUID, guarded by a pending list and a 15 s timeout) and fall back to a fresh prefab + civilian loadout at the last known position. `AttachRecruitBody()` re-links maps, re-tracks, reactivates AI, restores the name, re-owns, re-factions and re-groups the body, then broadcasts.
8. **Save:** `OVT_OverthrowGameMode.PreShutdownPersist` (:598-603) calls `SyncRecruitPositions()` so live bodies' positions and ids are current, then the serializer writes one versioned record per recruit; on load the owner index is rebuilt from records and bodies come back only when their owner does (recruit bodies are `SelfSpawn 0`).

### Integration Points
- **resistance/building** (sibling): places the tent the tent-recruit action sits on; recruit spawn position is tent-relative.
- **resistance/core** (sibling): camps/FOBs give the tent context; base/FOB *garrison* recruiting reuses `baseRecruitCost` and supporter checks but is a separate system (`OVT_BaseMenuContext.c:60`, `OVT_FOBMenuContext.c:45`) — not recruits.
- **resistance/loadouts** (sibling): `OVT_LoadoutsContext` lists nearby recruits via `GetPlayerRecruitEntitiesInRadius()` and `OVT_LoadoutManagerComponent.c:238-268` resolves recruit names/notifications when applying a loadout to a recruit. Loadout save/load internals live there.
- **Town system:** tent recruiting consumes 1 supporter + 1 population (`OVT_TownManagerComponent.TakeSupportersFromNearestTown` :1190); recruits record their hometown; civilian-death support/stability modifiers explicitly skip recruits.
- **Occupying faction:** XP source (`m_OnAIKilled`); recruited civilians stop reading as targets while unwanted (modded AI perception).
- **Skills/progression epic:** the recruit record carries a skills map and training fields, but nothing writes them yet (see Technical Debt); recruits inherit *the owner's* stealth skill through `OVT_PlayerWantedComponent.c:397-406`.
- **Jobs:** `OVT_HasRecruitJobStage` gates a tutorial job on owning ≥1 recruit; the UI's show-on-map reuses the job waypoint.
- **Map/fast travel:** `OVT_MapContext.c:381-498` charges per-recruit fees and calls `RequestFastTravelWithRecruits` (50 m radius) for on-foot and bus travel.
- **Vanilla persistence feature:** shares the `OVT_PersistenceTracking` save/untrack-keep-data idiom with `OVT_VehicleManagerComponent`; covered by the persistence test tiers.

---

## Implementation Details

### Phase 1: Data Model & Recruiting (COMPLETED)
- Server-authoritative `m_mRecruits` map + derived `m_mRecruitsByOwner` index; `MAX_RECRUITS_PER_PLAYER` cap; unique id generation (`recruit_<owner>_<unixtime>_<hex6>`).
- Two acquisition paths (civilian hold-action, tent action) funnelling into one server `RecruitCivilian()`.

### Phase 2: Group & AI Behaviour (COMPLETED)
- Vanilla group insertion via `RequestAddAIAgent`, with a host shortcut and a client path (6 s delayed broadcast RPC + 10×2 s entity-replication retry ladder).
- Modded AI perception so unwanted recruits neither fear nor are targeted by the occupying faction; per-recruit wanted levels.
- `OVT_OpenInventoryCommand` (commanding radial): owner-only, alive-only, opens the recruit's inventory through temporary possession.

### Phase 3: Replication (COMPLETED)
- Full-table JIP via `RplSave`/`RplLoad` (:1751-1887) including skills and per-recruit RplIds.
- Live broadcast RPCs for create/remove/update; clients derive `m_bIsOnline` from RplId validity.

### Phase 4: Persistence & Body Round-Trip (COMPLETED, reworked 2026)
- Versioned record serializer (v1 records, v2 + body UUID); owner index rebuilt on load; idempotent re-apply to a running campaign.
- Save-and-release despawn; async stored-body respawn with pending-list dedupe, 15 s timeout, fresh-body fallback, and owner-left-again release. Extensively documented in-file.

### Phase 5: Management UI (COMPLETED)
- `OVT_RecruitsContext`: roster, selection (mouse + gamepad bindings), details (level/XP/kills/hometown/distance), rename dialog, dismiss confirm dialog, show-on-map.

### Phase 6: Progression Depth (NOT STARTED)
- Skills map, training flags and an XP-gained invoker exist on the record and in the wire/persistence formats, but no system writes or reads them yet.

---

## Key Technical Decisions

### Decision 1: Records and bodies are separate identities
**Context:** A recruit must outlive its physical character (despawns, deaths of the *body's* storage, prefab changes).
**Implementation:** The record (generated string id) is the durable thing; the body is an ordinary vanilla-tracked character whose persistence UUID the record carries. `CaptureRecruitBodyId()` is the single write point.
**Trade-offs:** Clean permadeath (drop the record, keep the corpse) and gear-preserving round-trips; but two id spaces and three lookup maps to keep coherent.

### Decision 2: Bodies exist only while the owner is present
**Context:** Empty servers full of other people's AI squads are wasted perf and free kills.
**Implementation:** 10-minute offline timer → save-and-release; respawn is driven off the owner's group-created event, preferring the stored body via `PersistenceSystem.RequestSpawn` (vanilla's own returning-player pattern).
**Trade-offs:** Matches player expectation ("my squad is where I left it, carrying what I gave it"); the cost is a long async tail of edge cases (pending list, timeout, owner-left-again), all handled explicitly.

### Decision 3: Vanilla group system, not a custom commander
**Context:** Reforger already has group slots, commanding and formation logic.
**Implementation:** Recruits are plain AI agents added to the player's slave group through `SCR_PlayerControllerGroupComponent`; commanding, formations and orders are all vanilla. Overthrow adds only ownership (`OVT_PlayerOwnerComponent`) and one custom command.
**Trade-offs:** Free UI/commanding/JIP behaviour; but group insertion is timing-sensitive (leader must exist first), hence the delay/retry ladders on both server and client.

### Decision 4: Client-side cost, server-side effect
**Context:** (Inherited pattern rather than a decision.) Player money lives client-side in Overthrow's economy seams.
**Implementation:** Both recruit actions check and deduct money on the client, then fire an RPC that performs the recruitment without re-validating cost, supporters (civilian path) or the sender's identity.
**Trade-offs:** Simple and latency-free for honest clients; but the server will recruit for free for anyone who skips the action layer — see Known Issues.

### Decision 5: Whole-table JIP + per-event broadcasts
**Context:** Clients need the roster for UI and entity resolution.
**Implementation:** `RplSave/RplLoad` carries everything (including body-less recruits, which RplProps couldn't), broadcasts keep it live; `m_sBodyPersistenceId` is deliberately excluded from the wire — it is meaningless off the server.
**Trade-offs:** Correct JIP for arbitrary roster sizes; but three hand-rolled RPCs must each maintain the same three collections, and they have drifted (see Technical Debt: inconsistent host guards, `RpcDo_RecruitUpdated` not syncing training/skills).

---

## Current State

### What's Working
- Both recruiting paths, group insertion, commanding, owner-only inventory, XP/levels/notifications, permadeath, dismissal, rename (single-player/host), roster UI, JIP.
- The full offline→despawn→return→respawn loop with gear intact, and the save/load story on top of vanilla persistence (covered by automated persistence tests).
- Fast travel charges for and intends to move nearby recruits; wanted/civilian-perception integration works.

### Known Issues
- **Rename is client-local in multiplayer:** `OVT_RecruitsContext.c:443` calls `m_RecruitManager.RenameRecruit()` directly on the client's replica — there is no rename RPC in `OVT_PlayerCommsComponent`. On a dedicated server the authoritative record never changes, so the new name is not persisted, other clients never see it, and the next `RpcDo_RecruitUpdated`/rejoin reverts it.
- **Fast travel leaves recruits behind:** `RpcAsk_RequestFastTravelWithRecruits` (`OVT_PlayerCommsComponent.c:985-992`) teleports the player *first*, then searches for recruits within `recruitRadius` of `playerEntity.GetOrigin()` — which is now the destination. `SCR_Global.TeleportPlayer` moves the entity synchronously (base `Functions.c:1677-1680`), so the recruits standing at the origin are never found, while the client has already charged per-recruit fees (`OVT_MapContext.c:390,466`).
- **Tent-recruit RPC validates nothing server-side:** `RpcAsk_RecruitFromTent` (`OVT_PlayerCommsComponent.c:847-864`) never checks `CanRecruit` before spawning — at the cap, `RecruitCivilian()` returns early and the freshly spawned civilian is orphaned at the tent (body with wanted-system off, owned by nobody). The supporter cost is decoupled from the effect: `TakeSupportersFromNearestTown` silently no-ops when the town lacks supporters (`OVT_TownManagerComponent.c:1194`) but the recruit spawns anyway. Money is neither checked nor charged server-side, and `playerId` is client-supplied.
- **Civilian-recruit RPC accepts any character:** `RpcAsk_RecruitCivilian` (`OVT_PlayerCommsComponent.c:826-839`) recruits whatever `SCR_ChimeraCharacter` the RplId resolves to — no faction/civilian check, no proximity check, no cap on the *sender* (only on the passed `playerId`), no server-side cost.
- **Map mutation during iteration:** `FindRecruitEntity` removes stale entries from `m_mEntityToRecruit` inside the `foreach` over that same map and keeps iterating (`OVT_RecruitManagerComponent.c:1587`) — the exact invalidated-iteration hazard `SyncRecruitPositions` documents and avoids at `:1496`.
- **Kill-XP hardcodes faction keys:** `OnAIKilled` only awards XP when the victim's key is `"US"` or `"USSR"` (`OVT_RecruitManagerComponent.c:679`) instead of asking the config for the occupying faction — custom-faction campaigns produce zero recruit XP.
- **Race on client-charged money:** both actions deduct money before the server acts; any server-side early return (cap reached in a race, spawn failure) keeps the player's money with no refund path.

### Technical Debt
- **Dead progression surface:** `m_bIsTraining`/`m_fTrainingCompleteTime` and the recruit `m_mSkills` map are persisted, JIP'd and broadcast-created, but nothing ever sets them (no `AddSkill` caller on recruits); `m_OnRecruitXPGained` has no listeners; `OVT_RecruitData.GetLevelProgress()` is uncalled (the UI uses the `OVT_PlayerData` copy).
- **Duplicated level maths:** the XP/level curve exists in both `OVT_PlayerData` and `OVT_RecruitData` with no shared source; the Logic-tier tests pin only the player copy.
- **RPC drift:** `RpcDo_RecruitRemoved`/`Updated` guard against running on a non-client, `RpcDo_RecruitCreated` does not (`OVT_RecruitManagerComponent.c:2008`); `RpcDo_RecruitUpdated` never syncs training/skills, so a JIP client and a broadcast-updated client can disagree on those fields.
- **Double subscription:** `OnPostInit` (:108-113) and `EOnInit` (:148-153) both insert `OnPlayerConnected`/`OnPlayerDisconnected` into the player manager's invokers.
- **Targeted work on a broadcast channel:** `RpcDo_AddRecruitToGroup` is a broadcast every client receives and all but one discard; the 6 s pre-delay and 10×2 s retry ladder are magic numbers papering over replication timing.
- **`ShowOnMap` half-implemented:** fetches the marker manager, never uses it, and hijacks the *job* waypoint instead (`OVT_RecruitsContext.c:477-487`).
- **`GenerateRecruitName()`** is a placeholder TODO (ten hardcoded English names) used only when the civilian has no identity component.
- **Magic numbers:** 16 cap, 10 min despawn, 15 s spawn timeout, 3 s/500 ms leader retries, 6 s group RPC delay, 10 XP/kill, 32-char name limit, 50% tent discount, 50 m travel radius.

---

## Future Enhancements

### High Priority
- [ ] Server-validate the recruit RPCs: sender-derived `playerId`, `CanRecruit` before any spawn, civilian-faction + proximity check on `RpcAsk_RecruitCivilian`, supporter-and-spawn as one transaction on the tent path, server-side money debit (or at least refund on failure).
- [ ] Add a rename RPC (`RpcAsk_RenameRecruit`) so renames reach the authority, persist and replicate.
- [ ] Capture the player's position *before* `TeleportPlayer` in `RpcAsk_RequestFastTravelWithRecruits`.
- [ ] Fix the `FindRecruitEntity` mid-iteration removal (return after removal, or collect-then-remove).

### Medium Priority
- [ ] Award XP against the configured occupying faction key instead of hardcoded `US`/`USSR`.
- [ ] Either implement recruit skills/training or strip the dead fields from the record, wire format and serializer (a v3 would be needed — field order is the format).
- [ ] Unify the level curve into one shared implementation and pin it in the Logic tier.
- [ ] Make the recruit cap and per-kill XP difficulty-configurable.

### Low Priority / Nice to Have
- [ ] Real map markers for show-on-map instead of the job waypoint.
- [ ] Proper generated names (localized name pools) for identity-less recruits.
- [ ] Owner-targeted RPC for group insertion; replace retry ladders with replication callbacks.
- [ ] Recruit counting toward QRF scoring (shared gap with `occupying/qrf`).

---

## Testing

### Current Coverage
- **Init tier:** `OVT_Global.GetRecruits()` resolves (`OVT_TEST_InitSuite.c:77`).
- **Persistence tier:** `OVT_TEST_Persistence_Recruits_RoundTrip` (add → XP → remove through the manager API, `OVT_TEST_PersistenceSuite.c:444`); the save→dirty→re-apply round trip asserts recruit XP survives re-application (`OVT_TEST_PersistenceRoundTripSuite.c:1068`); `OVT_TEST_PersistenceSubject.ResolveRecruitSubjectEntity` documents why tests must not spawn characters in the test world.
- The Logic tier pins the *player* level curve, not the recruit copy.

### Testing Gaps
- Logic-tier candidates (world-free): recruit level curve parity with `OVT_PlayerData` (`GetLevel`/`GetNextLevelXP`/`GetLevelXP`/`GetLevelProgress`); `AddXP` keeping the cached `m_iLevel` consistent; `CanRecruit` boundary at the cap; `GenerateRecruitId`/`GenerateRandomHex` format and uniqueness; `ParseFullName` ↔ rename-splitting round trip (alias quoting); `ApplyPersistedRecruits` idempotency (no duplicate owner-index entries, body id adopted only when empty, online flag preserved); `ApplyPersistedRecruitSkills` with mismatched/empty key arrays; serializer v1 payloads getting body ids blanked.
- Not automatable today: group insertion timing, JIP roster mirroring, dedicated-server rename behaviour, the offline-despawn → reconnect body round trip with real players, AI perception of unwanted recruits — manual play-testing (two clients for the multiplayer cases).
- Worth one manual check: a recruited town civilian surviving the town's civilian despawn sweep (`OVT_TownController.DespawnCivilians` deletes the civilian *group* and children; the recruit is re-grouped, but the interaction is untested).

---

## Documentation

### Current Documentation
- This retrospective plan; epic docs at `docs/features/resistance/`.
- The manager, serializer and data class carry unusually thorough in-code documentation of the body-persistence contract (written during the `vanilla-persistence` epic).

### Documentation Needs
- Player-facing: costs (base vs tent), the 16 cap, the 10-minute offline rule, permadeath, and that gear survives despawns.

---

## Dependencies

### External Dependencies
- Vanilla groups/commanding (`SCR_PlayerControllerGroupComponent`, `SCR_AIGroup`), character identity, `SCR_PersistenceSystem` (spawn requests, collections), AI perception classes (modded).

### Internal Dependencies
- `OVT_PlayerManagerComponent` (persistent ids, connect/disconnect events), `OVT_SpawnLogic` (group-created event), `OVT_PersistenceTracking` + `OVT_RecruitManagerSerializer` (persistence), `OVT_PlayerOwnerComponent` (ownership), `OVT_TownManagerComponent` (supporters, hometowns), `OVT_EconomyManagerComponent` (client-side money), difficulty config, notification manager, UI context framework.

---

## Notes

**Discovered Information:**
- The body-persistence design is deliberate and well-documented in-code: records are the durable identity, bodies are vanilla-tracked characters (`SelfSpawn 0`), and `ReleaseRecruitBody` uses vanilla's own save-untrack-keep-data idiom (`SCR_SpawnLogic.OnPlayerEntityCleanup_S`).
- Death permanence is enforced by clearing the body id *before* dropping the record, so nothing holding the record can resurrect the corpse; in-flight spawn requests for dead/dismissed recruits delete the arriving body and its stored data.
- `baseRecruitCost` *is* replicated to clients (config JIP), unlike the QRF difficulty fields (BUG-013) — the recruit action prices shown on clients are correct.
- The recruit prefab inherits `Character_Base.et` (native Persistence component) and lands in vanilla's `"Character"` collection — verified in-code against `Common.conf`, not assumed.

**Retrospective Assessment:**
- The persistence half of this feature is the most carefully engineered code in its class — explicit about async edges, idempotency and ownership. The multiplayer command surface around it is the weak half: the recruit RPCs trust the client for identity, cost and target, rename never reaches the server, and the fast-travel helper searches the wrong end of the teleport.
- The progression scaffolding (skills, training) has been carried through every format (record, JIP, broadcasts, serializer v1) without ever being implemented — it should be built or removed before the formats calcify further.

---

*This retrospective plan was created by analyzing existing code. Use `/start-feature resistance/recruits` to begin making improvements.*
