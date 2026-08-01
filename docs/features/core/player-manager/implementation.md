# Player Manager - Implementation Plan (Retrospective)

**Status:** Implemented (Documented Retrospectively)
**Originally Implemented:** Unknown (legacy — predates Beast Mode)
**Documented:** 2026-08-02
**Last Updated:** 2026-08-02

---

## Executive Summary

The player management system owns persistent player identity and the per-player record. `OVT_PlayerManagerComponent` maps runtime player IDs ↔ persistent UIDs (Bohemia account UUID on dedicated; a *name-derived* synthetic UUID in SP/listen) and holds the `OVT_PlayerData` registry — the single record carrying money, XP/kills, skills, home/camp, officer flag. Registration flows through `OVT_SpawnLogic.DoSpawn_S` → `SetupPlayer` (identity, always) → `FinalizePlayerPreparation` (home/car/cash/officer, deferred until the campaign starts). All other managers (economy, skills, real estate, resistance, jobs, recruits) read and mutate this record rather than keeping their own player state.

**Note:** This is a retrospective implementation plan created by analyzing the existing codebase. The feature has already been implemented.

---

## Goals

### Primary Goals
- Stable persistent identity across sessions (survives reconnects; keyed by UID, not runtime ID)
- One canonical per-player record all systems share (no per-manager wallets)
- JIP: late joiners receive the full player table; live deltas broadcast after

### Success Criteria
- [x] Runtime↔persistent ID spaces resolve to the same record (asserted by `OVT_TEST_Persistence_PlayerMoney_RoundTrips`)
- [x] Money/skills/XP round-trip through the public manager API (Tier D, 8 green cases)
- [x] Skill-effect fields are derived (`[NonSerialized]`), rebuilt by replaying effects — narrow persistence contract
- [ ] Player state survives save/reload — blocked on `core/persistence` (quarantined round-trip suite is the gate)

---

## Current Architecture

### Key Components

| File | Role |
|---|---|
| `Scripts/Game/GameMode/Managers/OVT_PlayerManagerComponent.c` (469 ln) | ID maps (`m_mPersistentIDs`/`m_mPlayerIDs`), `OVT_PlayerData` registry (`m_mPlayers`), controller lifecycle, JIP `RplSave`/`RplLoad` |
| `Scripts/Game/Data/OVT_PlayerData.c` (103 ln) | The record: money, xp/kills, skills map, home/camp, officer, `initialized`, plus `[NonSerialized]` derived fields (priceMultiplier, stealthMultiplier, diplomacy, permissions) and the level curve (`1 + 0.1*sqrt(xp)`) |
| `Scripts/Game/Respawn/Logic/OVT_SpawnLogic.c` | **Live entry point**: derives UID via `EPF_Utils.GetPlayerUID`, calls `SetupPlayer`, defers finalization until `HasGameStarted()` |
| `Scripts/Game/GameMode/OVT_OverthrowGameMode.c` | `FinalizePlayerPreparation` (L577): home + starting car + starting cash + officer grants; disconnect handling (L500) |
| `Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c` (1430 ln) | Client→server `RpcAsk_*` channel (on the character AND the game mode), incl. hand-rolled money re-entrancy guards |
| `Scripts/Game/GameMode/Persistence/Components/OVT_PlayerSaveData.c` | EPF SaveData: copies `m_mPlayers` wholesale, skips uninitialized records, fires `m_OnPlayerDataLoaded` per player |
| `Scripts/Game/Respawn/Logic/OVT_PersistentRespawnLogic.c` | **Dead code** — referenced by no prefab/config (its only caller chain includes `PreparePlayer`, also dead) |

### Data Flow — identity

`EPF_Utils.GetPlayerUID` → `SCR_PlayerIdentityUtils.GetPlayerIdentityId`: server-only; primary source is the **Bohemia backend account UUID**; in SP/listen (non-dedicated) it falls back to a UUID **hashed from the player's profile name** — renaming the profile orphans money/home/skills. On dedicated with a broken backend the UID is empty and `SetupPlayer` rejects it.

### Data Flow — registration lifecycle

1. `OVT_SpawnLogic.DoSpawn_S(playerId)` derives the UID (retry via `CallLater` on failure)
2. `SetupPlayer(playerId, persistentId)`: writes both ID maps, creates `OVT_PlayerData` **only if absent** (save-loaded records reused), then (server) spawns the per-player `OVT_OverthrowController` + transfers ownership, and broadcasts `RpcDo_RegisterPlayer` so clients mirror the mapping
3. `FinalizePlayerPreparation`: deduped via session-scoped `m_aInitializedPlayers`; fires `m_OnPlayerConnected`; officer grants (SP/listen-host/config list); assigns random starting house + car (or bus-stop fallback); grants `startingCash` once (guarded by the *persisted* `player.initialized`); teleports home. Deferred until `DoStartGame()` when joining pre-start.
4. Disconnect: `player.id = -1`, EPF pause+save on the character, session-init set cleared (so reconnect re-runs home/officer but not cash), character RplEntity deleted. Controller cleanup is a 5 s polling sweep; ID maps and records are deliberately never pruned.

### Data Flow — replication

- **JIP**: manual `RplSave`/`RplLoad` bitstream of the whole player table (including offline players); `RplLoad` rebuilds both maps and replays skill effects via `OnPlayerDataLoaded`
- **Live deltas**: all `RplRcver.Broadcast` — money (`RpcDo_SetPlayerMoney`), xp/kills, skill levels (re-applies effects only for the local player), home, officer
- **Client→server**: `OVT_PlayerCommsComponent` `RpcAsk_*` via the dual-natured `OVT_Global.GetServer()`; money paths carry `addingMoney`/`takingMoney` re-entrancy guards ("Stop money glitch")

### Integration Points
- **Economy**: no wallet of its own — mutators take *runtime* IDs, accessors take *persistent* IDs; `m_OnPlayerMoneyChanged(persId, amount)`
- **Skills**: server subscribes to `m_OnPlayerDataLoaded` and replays `OVT_SkillEffect`s into the derived fields; XP sources wired in `PostGameStart`
- **Real estate / owner manager**: `home` on the record; ownership keyed by persistent ID in `OVT_OwnerManagerComponent` maps + `OVT_PlayerOwnerComponent.m_sOwnerUID` (`RplProp`)
- **Resistance** (officer, camp), **jobs/recruits/vehicles** (offline timers keyed by persistent ID), **wanted** (reads `stealthMultiplier`)

---

## Implementation Details

### Phase 1: Identity + record (COMPLETED)
ID maps, `OVT_PlayerData`, EPF UID derivation, two-phase registration split (identity vs finalization) so SP/listen can defer onboarding until Start is pressed.

### Phase 2: Replication + consumers (COMPLETED)
JIP full-table snapshot, broadcast deltas, comms-component RPC surface, manager integrations (economy/skills/real-estate/resistance/jobs/recruits).

### Phase 3: Potential Improvements (NOT STARTED)
Fix `IsOffline()`, prune stale ID maps, null-guard `OVT_PlayerData.Get()` consumers, delete dead respawn logic (see Future Enhancements).

---

## Key Technical Decisions

### Decision 1: Persistent-UID-keyed single record
**Context:** Runtime player IDs are session-scoped and reused.
**Implementation:** Two maps translate between spaces; every manager keys durable state by persistent ID.
**Trade-offs:** Clean cross-session identity; but the two spaces leak into API signatures inconsistently (economy mutators take runtime IDs, accessors take persistent IDs).

### Decision 2: Two-phase registration + two idempotency guards
**Context:** Players can join before the campaign starts; starting cash must be one-time.
**Implementation:** `SetupPlayer` always; `FinalizePlayerPreparation` deferred until started, deduped by session-scoped `m_aInitializedPlayers`; cash guarded by persisted `player.initialized`.
**Trade-offs:** Correct deferral and one-time grants; the two guards' different scopes are subtle (reconnect re-runs home/officer, not cash).

### Decision 3: Derived fields are never persisted or replicated wholesale
**Context:** Skill effects produce multipliers/permissions.
**Implementation:** `[NonSerialized]` fields rebuilt by replaying `OVT_SkillEffect.OnPlayerData` after load/JIP.
**Trade-offs:** Narrow, migration-friendly persistence contract (this is what lets the Tier D suites survive the EPF→vanilla migration); costs a replay pass on every load.

---

## Current State

### What's Working
- Identity mapping, money/skills/XP flows, JIP snapshot, onboarding (home/car/cash/officer) — Tier A (3 skills-logic cases) and Tier D (8 same-session round-trips) green
- Offline-player handling in jobs/recruits/vehicles via persistent-ID timers

### Known Issues
- **`IsOffline()` is wrong after disconnect**: it tests `id == 0`, but disconnect sets `id = -1` — a departed player reports online (affects jobs, recruits, job stages).
- **ID maps never pruned**: stale runtime IDs can be *reused by a different joiner*; `TakePlayerMoneyPersistentId` streams against dead IDs.
- **Unchecked nulls off `OVT_PlayerData.Get()`** across skill manager, spawn logic, and RPC receivers — a client that hasn't yet received `RpcDo_RegisterPlayer` dereferences null.
- **SP/listen identity is name-derived** — renaming the profile creates a fresh player and orphans all state.
- `persistence.IsActive()` call site resolves to the engine's generic "component enabled", not "persistence ready" (shared finding with `core/game-mode`).
- Possible double subscription in `OVT_RecruitManagerComponent` (`OnPostInit` + `EOnInit` both Insert the same handlers — unverified whether `ScriptInvoker.Insert` dedupes).
- Doc drift: `docs/technical-design.md:237` claims comms/UI components live on the controller; they live on the **character prefab**.

### Technical Debt
- Full-table JIP snapshot sends every player's balance to every joiner (O(players × skills), no privacy).
- Manual bitstream with no schema versioning (shared with config's replication).
- `OVT_PlayerCommsComponent` is a 1430-line god object (money, shops, real estate, FOBs, loadouts, recruits, camps).
- Dead code: `OVT_PersistentRespawnLogic.c` (210 ln), `PreparePlayer`, stale doc comment on `GetPersistentIDFromPlayerID` (also baked into generated docs).
- `RplSave` asymmetries: `initialized`/`levelNotified` persisted but not replicated; `firstSpawn` neither.

---

## Future Enhancements

### High Priority
- [ ] Fix `IsOffline()` (`id <= 0`, or an explicit flag)
- [ ] Null-guard `OVT_PlayerData.Get()` consumers on the client RPC paths

### Medium Priority
- [ ] Prune/repair ID maps on disconnect (or verify stale-ID reuse can't corrupt records)
- [ ] Delete `OVT_PersistentRespawnLogic` + `PreparePlayer`; fix the stale doc comment and `technical-design.md:237`
- [ ] Resolve the recruit-manager double subscription

### Low Priority / Nice to Have
- [ ] Trim the JIP snapshot (skip offline players; don't broadcast every balance)
- [ ] Split `OVT_PlayerCommsComponent` along domain lines (aligns with the `core/game-mode` controller migration)

---

## Testing

### Current Coverage
- **Logic tier** (`OVT_TEST_Logic_Skills.c`): skill effects write only their own field; permission idempotency; level curve accessors; fractional level progress
- **Persistence tier D** (green, All group): money round-trips across the two ID spaces; skills/XP round-trips through the manager API; subject resolution via `OVT_TEST_PersistenceSubject` (player registered ~18 ms before campaign start — no polling needed)
- **Persistence tier D′** (quarantined, red by design): money/skills survive save+reload — `core/persistence`'s acceptance gate

### Testing Gaps
- Connect/disconnect flow, `SetupPlayer` mapping semantics, controller lifecycle, `RplSave`/`RplLoad` JIP round-trip, `IsOffline()`, officer grants, home assignment, identity collisions — all JIP/MP territory, manual play-testing only

---

## Documentation

### Current Documentation
- `docs/technical-design.md` (with the known §237 drift), Tier A/D suite headers, `dev-ops/test-coverage` findings (observed CI UID, ordering guarantees)

### Documentation Needs
- The SP/listen name-derived identity behaviour deserves a prominent warning anywhere player data management is discussed

---

## Dependencies

### External Dependencies
- Bohemia backend API (account UUID); `SCR_PlayerIdentityUtils`, `SCR_RespawnSystemComponent`
- **EPF still live at runtime**: `EPF_Utils.GetPlayerUID`, `EPF_BaseSpawnLogic`, `EPF_PersistenceComponent` pause/save on disconnect — the vanilla migration swapped the persistence manager but not the identity/spawn/save-data layer

### Internal Dependencies
- `core/game-mode` (hosts the manager; finalization + disconnect live on the game mode; controller spawn shared)
- `core/persistence` (OVT_PlayerSaveData is EPF; nothing saves today — BUG-002)
- Consumed by economy, skills, real estate, resistance, jobs, recruits, vehicles, wanted, ~25 user actions, ~12 UI contexts

---

## Notes

**Discovered Information:**
- The player manager is initialized **first** in `EOnInit`, with `s_Instance` force-set before `Init` — a deliberate fix for the singleton resolving to the wrong instance.
- `OVT_PlayerCommsComponent` exists on *both* the game mode prefab and the character prefab; `OVT_Global.GetServer()` picks per side.
- The migration is half-done by design: identity/spawn still EPF, persistence manager already vanilla-stubbed.

**Retrospective Assessment:**
- The stored-vs-derived split on `OVT_PlayerData` is the system's best property — it's what made behaviour-level persistence tests (and the migration gate) possible. The debt clusters around lifecycle edges: disconnect state, stale maps, and null paths on clients.

---

*This retrospective plan was created by analyzing existing code. Use `/start-feature core/player-manager` to begin making improvements.*
