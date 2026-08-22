# Game Mode Foundation - Implementation Plan (Retrospective)

**Status:** Implemented (Documented Retrospectively)
**Originally Implemented:** Unknown (legacy — predates Beast Mode)
**Documented:** 2026-08-02
**Last Updated:** 2026-08-02

---

## Executive Summary

The game-mode foundation is the skeleton every other Overthrow system stands on: `OVT_OverthrowGameMode` (the game-mode entity that owns and initializes ~20 manager components), `OVT_Global` (the static service locator used at 1083 call sites across 157 files), and `OVT_OverthrowController` (the per-player server-spawned entity intended as the client→server seam). It owns the campaign lifecycle — world init, "start new game" vs "load save" branching, the two-flag started/initialized state machine, and per-player preparation (home, car, starting cash, officer status).

**Note:** This is a retrospective implementation plan created by analyzing the existing codebase. The feature has already been implemented.

---

## Goals

### Primary Goals
- Host every manager singleton on one game-mode entity with a deterministic init order
- Provide global, null-safe-ish access to managers from anywhere (`OVT_Global`)
- Drive the campaign lifecycle: fresh start (menu/dedicated) vs save-load, and per-player onboarding
- Provide a per-player, client-owned entity for client→server RPC (`OVT_OverthrowController`)

### Success Criteria
- [x] Managers resolve via `OVT_Global` (asserted by `OVT_TEST_Init_Globals_ManagersResolve` — 18 getters)
- [x] `DoStartNewGame()` + `DoStartGame()` produce a started + initialized campaign (asserted by `OVT_TEST_Campaign_GameMode_IsStartedAndInitialized`)
- [x] Per-player controller spawn/ownership transfer works (manual play-testing only — JIP/MP territory)
- [x] Controller migration completed — **done 2026-08-14** by `core/controller-migration` (see Phase 3): the monolith is deleted, 50 surviving requests live on domain components on `OVT_OverthrowController` (17 components on the prefab)

---

## Current Architecture

### Key Components

| File | Role |
|---|---|
| `Scripts/Game/GameMode/OVT_OverthrowGameMode.c` | Game mode: manager lifecycle, campaign start, player prep, per-frame camera/debug |
| `Scripts/Game/Global/OVT_Global.c` | Static service locator: 17 manager getters + client-scoped helpers. **302 lines since 2026-08-14** — the spawn/loot/clothing utilities moved to `Scripts/Game/Utilities/OVT_WorldUtils.c`, `OVT_PrefabUtils.c` and `OVT_LoadoutUtils.c`, with thin forwarders kept for the three highest-traffic helpers |
| `Scripts/Game/GameMode/OVT_OverthrowController.c` | Per-player `GenericEntity`; owns `OVT_ProgressEventHandler`; ownership handed to the client via `RplComponent.GiveExt` |
| `Scripts/Game/GameMode/Managers/OVT_PlayerManagerComponent.c` | Spawns/assigns/cleans up controllers (`SetupPlayer` L218-288, `AssignControllerOwnership` L290-322) |
| ~~`Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c`~~ | **DELETED 2026-08-14.** Was the legacy client→server channel on the character prefab — 57 RPCs reached via `OVT_Global.GetServer()`. Replaced by domain components in `Scripts/Game/Components/Controller/` (20 files: 17 components + the shared request base + the progress base + the generic accessor), reached via `OVT_ControllerComponent<T>.Get()` |
| `Prefabs/GameMode/OVT_OverthrowGameMode.et` | Wires ~16 OVT managers + ~20 vanilla SCR components + EPF SaveData registration |
| `Prefabs/GameMode/OVT_OverthrowController.et` | Controller prefab: **17 components** + `RplComponent` (was `OVT_ContainerTransferComponent` alone until `core/controller-migration` landed on 2026-08-14) |

### Data Flow — campaign lifecycle

1. **`EOnInit`** (`OVT_OverthrowGameMode.c:757+`): find + `Init()` managers in hand-maintained order — Player (also force-sets its `s_Instance`), RealEstate (cached only), Towns, Economy, OccupyingFaction, ResistanceFaction, Vehicles, Jobs, Skills, Deployment. Then `if(!IsMaster()) return;` — everything after (config `LoadConfig()`, persistence resolve, start branching) is **server-only**.
2. Start branching: save exists → request start on post-process; dedicated → `DoStartNewGame()` immediately; else wait for the start menu (`OVT_StartGameContext.StartGame()` → `DoStartNewGame()` + `DoStartGame()`).
3. **`DoStartNewGame()`** (L103-144): apply `Overthrow_Config.json` faction picks (dedicated), `SetBaseAndTownOwners()`, `OccupyingFactionManager.NewGameStart()`. Assigns `m_Config` here — *not* in `EOnInit`.
4. **`DoStartGame()`** (L173-270): cache faction indices → **`m_bGameStarted = true`** → `PrepareConnectedPlayers()` → `PostGameStart()` on Economy, Towns, OccupyingFaction, ResistanceFaction, Jobs, Skills, Deployment → JSON difficulty overrides → **`m_bGameInitialized = true`**.
5. The two-flag split (`HasGameStarted()` vs `IsInitialized()`) is deliberate: a half-completed start is detectable, and the campaign test asserts exactly that.

### Data Flow — per-player controller

`SetupPlayer(playerId, persistentId)` populates ID maps + `OVT_PlayerData`, then (server) spawns `OVT_OverthrowController.et`, stores it in `m_mPlayerControllers`, transfers network ownership via `RplComponent.GiveExt(playerRplID, true)`, and fires the owner-only RPC `RpcDo_NotifyOwnerAssignment` so the client registers its controller locally. A 5-second `CallLater` sweep (`CheckDisconnectedPlayers`) deletes controllers of disconnected players while keeping their data for reconnect.

### Integration Points
- **All managers** live as components on the game-mode prefab; three init contracts coexist (game-mode-driven `Init(this)`, `OnPostInit` self-registration, pure lazy `GetInstance()`).
- **Factions**: `OVT_OverthrowFactionManager` is a separate world entity (`GetGame().GetFactionManager()`), reached via `OVT_Global.GetFactions()`.
- **Spawning**: `OVT_RespawnSystemComponent` + `OVT_SpawnLogic` (`Scripts/Game/Respawn/Logic/`) — `DoSpawn_S` defers `FinalizePlayerPreparation` until `HasGameStarted()` (race-condition fix, commit `89372c0`).
- **Persistence**: `GetPersistence()` returns `OVT_PersistenceManagerComponent` — **server-only** (`m_Persistence` is assigned after the `!IsMaster()` return, so null on clients). The `core/persistence` migration owns that component's current stub state.
- **Kill feed**: `GetOnCharacterKilled()` invoker fired from the modded `SCR_CharacterDamageManagerComponent`, consumed by recruits + town stability/support modifiers.

---

## Implementation Details

### Phase 1: Core Lifecycle (COMPLETED)
Game mode entity, manager `Init()` ordering, start branching (menu / dedicated / save-load), `DoStartNewGame`/`DoStartGame`, player preparation (home + car + starting cash + officer grants), disconnect handling.

### Phase 2: Controller Seam (COMPLETED — partially adopted)
`OVT_OverthrowController` spawn + ownership transfer + client registration; `OVT_BaseServerProgressComponent` owner-RPC progress pattern; `OVT_ContainerTransferComponent` as the first (and so far only) migrated domain. Documented in `docs/archive/OverthrowController.md`.

### Phase 3: Controller Migration (COMPLETED 2026-08-14)
Done as its own feature — `core/controller-migration`, ten phases, P1-P10. Every remaining domain (vehicles, real estate + warehouses, economy + shop purchase, resistance ops, FOBs/camps, recruits, loadouts, possession, jobs, campaign actions) moved off `OVT_PlayerCommsComponent` onto a domain component on `OVT_OverthrowController`, with server-side validation re-derived per handler and every client-supplied identity parameter DELETED from the signature rather than ignored. The monolith and `OVT_Global.GetServer()` were then deleted outright and the component stripped from both prefabs. Nine zero-validation endpoints were closed on the way, and two shipped defects were found and filed (BUG-161, BUG-162). **The 21-step MP play-test in that feature's `implementation.md` §6 is the outstanding gate.**

---

## Key Technical Decisions

### Decision 1: Managers as game-mode components + static lazy singletons
**Context:** Enfusion's entity-component model; every system needs cheap global access.
**Implementation:** Each manager exposes `GetInstance()` that lazily resolves `GetGame().GetGameMode().FindComponent(...)` into `s_Instance`; `OVT_Global` wraps these in static getters.
**Trade-offs:** Trivially accessible everywhere; but the statics are engine-nulled weak refs (self-healing across world reloads — measured in `dev-ops/test-coverage` findings §1.5), and three init contracts coexist with no registry.

### Decision 2: Two-flag start state (`m_bGameStarted` before managers start, `m_bGameInitialized` after)
**Context:** `PostGameStart()` bodies themselves call code that checks `HasGameStarted()`.
**Implementation:** Set at the start and end of `DoStartGame()` respectively.
**Trade-offs:** Makes a half-completed start observable (and testable); but callers must know which flag they mean.

### Decision 3: Dual client→server channels during a stalled migration — SUPERSEDED 2026-08-14
**Context:** `OVT_PlayerCommsComponent` (on the character) predated the controller.
**Was:** `OVT_Global.GetServer()` resolved to the game-mode's comms component on the server and the local character's on clients — so `RpcAsk_*` calls worked uniformly in SP/listen/dedicated.
**Now:** there is one channel. `core/controller-migration` moved every request onto a domain component on `OVT_OverthrowController` and deleted both the monolith and `GetServer()`. The uniformity that made the old accessor attractive is preserved by the entry-point convention (`if(Replication.IsServer()) direct-call; else Rpc(...)`), and the character-dies-with-the-channel failure mode is gone because the controller is a separate per-player entity that outlives the body.
**Trade-offs of the new shape:** twelve small components instead of one big one; the accessor returns null in strictly MORE situations than `GetServer()` did (dedicated server, pre-owner-assignment), so every call site carries a null guard.

---

## Current State

### What's Working
- Manager resolution, town/base registration, campaign start — all green in the Fast/All test groups
- Player onboarding (home/car/cash/officer), controller spawn + ownership (manual testing)
- Start menu vs dedicated vs save-load branching

### Known Issues
- **BUG-006**: `OVT_PersistenceManagerComponent.OnPostInit` fails to get `SCR_PersistenceSystem` at component-init time (init order), then the game mode unconditionally prints "Initializing Persistence" — misleading success on a failure path (`OVT_OverthrowGameMode.c:879`).
- **BUG-002** (downstream): `m_PersistenceSystem` null → `SaveGame()`/`AutoSave()` silently no-op.
- `GetPersistence()` returns null on clients; `OVT_PlayerStartMenuHandlerComponent` works around it with its own `FindComponent`.

### Technical Debt
- **BUG-012**: dead statement `OVT_Global.GetConfig() = OVT_Global.GetConfig();` (`OVT_OverthrowGameMode.c:789`) — almost certainly a mangled `m_Config = ...`; `m_Config` stays null through `EOnInit` and the save-load path.
- `OVT_ResistanceFactionManager` unsubscribe blocks cast `GetOwner()` to `OVT_OverthrowController` (always null on a game-mode component) — `Remove()` never runs; masked by defensive `Remove`-before-`Insert` at subscribe sites (L1324/1371/1412/1454).
- **BUG-017**: `OnPlayerDisconnected` re-implements its `super` body inline after calling `super` — disconnect events fire twice (verified against the 1.7.0.54 base: `SCR_BaseGameMode.c:920` is not empty).
- ~~`persistence.IsActive()` at `OVT_PlayerCommsComponent.c:21` resolves to the engine's `GenericComponent.IsActive()` ("component enabled"), not "persistence ready"~~ — moot since 2026-08-14: the file is deleted and the save request now lives on `OVT_CampaignRequestComponent.RpcAsk_RequestSave`.
- `OVT_InventoryManagerComponent.Init()` is dead code (never called); `m_bHasOpenedMenu` never read/written.
- Unguarded null derefs: `EOnFrame` debug handlers (`m_EconomyManager`, local controlled entity). ~~`OVT_Global.GetServer()`/`GetUI()`~~ fixed 2026-08-14 (`GetServer()` deleted, `GetUI()` and `GetDifficulty()` null-guarded).
- Manager init order is implicit — literal statement order in two places plus the prefab; no registry or dependency declaration.
- JSON difficulty overrides applied *after* `PostGameStart()` — managers that snapshot difficulty at start see pre-override values.

---

## Future Enhancements

### High Priority
- [x] ~~Complete the controller migration (Phase 3)~~ — **done 2026-08-14** by `core/controller-migration`; the monolith is deleted, not merely deprecated
- [ ] Fix the `m_Config` self-assignment and the double disconnect dispatch

### Medium Priority
- [ ] Fix the `OVT_ResistanceFactionManager` unsubscribe casts
- [ ] Null-guard the `EOnFrame` debug handlers (~~`OVT_Global.GetServer()`/`GetUI()`~~ done 2026-08-14)
- [ ] Move the JSON difficulty override before `PostGameStart()` (or re-broadcast after)

### Low Priority / Nice to Have
- [ ] A manager registry with declared init order/dependencies instead of hand-ordered statements
- [ ] Remove dead code (`OVT_InventoryManagerComponent.Init`, `m_bHasOpenedMenu`, `PreShutdownPersist` path)

---

## Testing

### Current Coverage
- **Init tier**: `OVT_TEST_Init_Globals_ManagersResolve` (18 `OVT_Global` getters, dependency-safe order), towns populated, controllers registered, economy seams
- **Campaign tier**: `OVT_TEST_Campaign_GameMode_IsStartedAndInitialized` (`HasGameStarted()` + `IsInitialized()` + difficulty-by-name)
- **Persistence tier** uses `GetOverthrow()`/`GetPersistence()`/`HasGameStarted()`; round-trip suite quarantined (gate for `core/persistence`)
- Measured timings (test-coverage findings §1.4): flags flip same-frame; shops ≤604 ms; OF resources ~6.5 s; deployments ~12 s

### Testing Gaps
- Controller spawn/ownership path (`SetupPlayer` → `GiveExt` → `NotifyOwnerAssignment`) — JIP/MP, manual play-testing only
- Start-menu flow, `PrepareConnectedPlayers`, disconnect cleanup, and every controller request RPC (`Scripts/Game/Components/Controller/`) — the Init tier asserts each component RESOLVES off a registered controller, but nothing automated exercises a request end to end

---

## Documentation

### Current Documentation
- `docs/technical-design.md` (architecture), `docs/archive/OverthrowController.md` (controller design + phases), `overthrow-architecture` + `enforcescript-patterns` skills

### Documentation Needs
- The implicit manager init-order contract (this doc now records it; code still doesn't)

---

## Dependencies

### External Dependencies
- Arma Reforger 1.7.0.54 (`SCR_BaseGameMode`, `SCR_RespawnSystemComponent`, RplComponent ownership APIs)
- EPF (current save path — via `EPF_PersistenceComponent` on the prefab; being replaced by `core/persistence`)

### Internal Dependencies
- Consumed by literally everything: all managers, UI contexts, user actions, tests
- `core/config` (config component lives on this prefab; `LoadConfig()` called from `EOnInit`)
- `core/player-manager` (controller spawn lives in the player manager)

---

## Notes

**Discovered Information:**
- Init order is load-bearing: `OVT_OccupyingFactionManager.Init` subscribes to `OVT_Global.GetTowns().m_OnTownControlChange`, so Towns must init first (and does).
- `OVT_Global` is also a grab-bag of world utilities (spawn helpers, loot, civilian clothing, road finding) beyond manager access.
- The controller prefab is spawned with a `null` world argument — inconsistent with other spawn sites but harmless (no transform).

**Retrospective Assessment:**
- The lifecycle is well-factored enough to be driven headlessly (the test framework reproduces the menu's two-call start sequence), which is what made the autotest spine possible.
- The single biggest architectural drag is the stalled dual client→server channel; finishing the controller migration would delete 1430 lines of character-coupled RPC surface.

---

*This retrospective plan was created by analyzing existing code. Use `/start-feature core/game-mode` to begin making improvements.*
