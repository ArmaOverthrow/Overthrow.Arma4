# Game Mode Foundation - Context & Decisions

**Last Updated:** 2026-08-02
**Current Phase:** Retrospective Documentation
**Status:** ✅ Documented (Existing Feature)

---

## Quick Status

**What's Done:**
- ✅ Feature fully implemented (existing legacy code)
- ✅ Retrospective documentation created (2026-08-02, via `/discover-feature`)

**What's Next:**
- 📋 Review for potential improvements (see `implementation.md` — controller migration is the big one)

**Blockers:**
- None (BUG-002/BUG-006 are owned by `core/persistence`)

---

## Key Files

- `Scripts/Game/GameMode/OVT_OverthrowGameMode.c` — lifecycle: `EOnInit` (manager init order), `DoStartNewGame` (L103), `DoStartGame` (L173), `FinalizePlayerPreparation` (L577), `OnPlayerDisconnected` (L500)
- `Scripts/Game/Global/OVT_Global.c` — 17 manager getters + `GetUI()`/`GetController()` + the identity helpers. **302 lines since 2026-08-14**: `core/controller-migration` deleted `GetServer()` and the six per-domain controller getters, and moved the world/prefab/loadout utilities to `Scripts/Game/Utilities/`.
- `Scripts/Game/GameMode/OVT_OverthrowController.c` — per-player entity, `RpcDo_NotifyOwnerAssignment`
- `Scripts/Game/GameMode/Managers/OVT_PlayerManagerComponent.c` — controller spawn (`SetupPlayer` L218), ownership (`AssignControllerOwnership` L290), cleanup sweep (L328)
- ~~`Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c` — legacy 57-RPC client→server channel~~ — **DELETED 2026-08-14** by `core/controller-migration`. Its replacements live in `Scripts/Game/Components/Controller/`.
- `Prefabs/GameMode/OVT_OverthrowGameMode.et`, `Prefabs/GameMode/OVT_OverthrowController.et`
- `Scripts/Game/Respawn/Logic/OVT_SpawnLogic.c`, `OVT_PersistentRespawnLogic.c` — spawn/prep gating on `HasGameStarted()`/`IsInitialized()`

---

## Important Decisions

- **Two-flag start state**: `m_bGameStarted` set at the *start* of `DoStartGame()`, `m_bGameInitialized` at the *end* — a half-completed start is observable; the Campaign tier asserts both.
- ~~**Dual client→server channels**~~ — **RESOLVED 2026-08-14** by `core/controller-migration`. There is now exactly ONE client→server channel: `OVT_OverthrowController`, carrying 17 components (10 added by the migration, 7 already there) reached through `OVT_ControllerComponent<T>.Get()`. The character-hosted comms monolith is deleted and both prefabs that listed it are stripped. Historical note: the migration was stalled at Phase 3 of `docs/archive/OverthrowController.md` from 2023 until 2026-08-14.
- **Manager singletons are engine-nulled weak refs** — `GetInstance()` self-heals across world reloads (measured in `dev-ops/test-coverage` findings §1.5). Don't cache manager pointers across worlds.
- **Init order is hand-maintained and load-bearing** (Towns before OccupyingFaction); it exists only as statement order in `EOnInit`/`DoStartGame` plus the prefab.

---

## Gotchas & Learnings

- `GetPersistence()` returns **null on clients** (`m_Persistence` assigned after the `!IsMaster()` return).
- ~~`OVT_Global.GetServer()`/`GetUI()` dereference the local controlled entity with **no null check**~~ — fixed 2026-08-14 (`core/controller-migration` T1.4/T10.1): `GetServer()` is deleted outright and `GetUI()` now returns null instead of VME-ing on a dead / pre-spawn / start-menu player. The Init suite still excludes `GetUI()`/`GetController()` because they need a LOCAL player, which is a different question from "is the game mode carrying its managers".
- JSON difficulty overrides run *after* `PostGameStart()`; managers snapshotting difficulty at start see pre-override values (and `NewGameStart()` reads difficulty even earlier).
- ~~`OVT_OverthrowGameMode.c` no-op self-assignment leaving `m_Config` null on the save-load path~~ — fixed 2026-08-02 (BUG-012 closed).
- ~~Disconnect events firing twice (`OnPlayerDisconnected` duplicated its `super` body inline)~~ — fixed 2026-08-02 (BUG-017 closed).
- Full oddity list (15 items): see `implementation.md` § Technical Debt and the discovery analysis.

---

*This context file was created retrospectively by analyzing existing code.*
