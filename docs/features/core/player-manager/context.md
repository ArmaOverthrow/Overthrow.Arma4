# Player Manager - Context & Decisions

**Last Updated:** 2026-08-02
**Current Phase:** Retrospective Documentation
**Status:** ✅ Documented (Existing Feature)

---

## Quick Status

**What's Done:**
- ✅ Feature fully implemented (existing legacy code)
- ✅ Retrospective documentation created (2026-08-02, via `/discover-feature`)

**What's Next:**
- 📋 Review for potential improvements — headline: the `IsOffline()` bug and unpruned ID maps (see `implementation.md`)

**Blockers:**
- Save/reload of player state is blocked on `core/persistence` (nothing saves today — BUG-002)

---

## Key Files

- `Scripts/Game/GameMode/Managers/OVT_PlayerManagerComponent.c` — ID maps, `m_mPlayers` registry, `SetupPlayer` (L218), JIP `RplSave`/`RplLoad` (L381-457), controller cleanup sweep (L328)
- `Scripts/Game/Data/OVT_PlayerData.c` — the record + level curve; `[NonSerialized]` derived fields
- `Scripts/Game/Respawn/Logic/OVT_SpawnLogic.c` — live entry point (UID derivation, registration, deferred finalization)
- `Scripts/Game/GameMode/OVT_OverthrowGameMode.c` — `FinalizePlayerPreparation` (L577), `OnPlayerDisconnected` (L500)
- `Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c` — client→server RPC surface (on character AND game mode)
- `Scripts/Game/GameMode/Persistence/Components/OVT_PlayerSaveData.c` — EPF SaveData (paused migration)
- Dead: `Scripts/Game/Respawn/Logic/OVT_PersistentRespawnLogic.c` (no prefab references it)

---

## Important Decisions

- **Persistent UID keys everything durable**; runtime IDs are session-scoped and translated via two maps. Economy mutators take runtime IDs, accessors take persistent IDs — asserted by the Tier D money round-trip.
- **Two-phase registration**: `SetupPlayer` (identity, always) vs `FinalizePlayerPreparation` (home/car/cash/officer, deferred until campaign start) — with two idempotency guards of different scope (session set vs persisted `initialized` flag).
- **Stored vs derived split**: skill-effect outputs are `[NonSerialized]`, rebuilt by replaying effects after load/JIP. Keep this in the persistence migration — it's why the test suites survive it.

---

## Gotchas & Learnings

- **SP/listen identity is derived from the profile NAME** (backend UUID fallback hashes the name) — renaming the profile orphans money/home/skills. Dedicated uses the real Bohemia account UUID (observed CI value in test-coverage findings).
- **`IsOffline()` tests `id == 0` but disconnect sets `id = -1`** — departed players report online (jobs/recruits consume this).
- ID maps are never pruned; a stale runtime ID can be reused by a different joiner.
- `OVT_PlayerData.Get()` returns null on clients that haven't received `RpcDo_RegisterPlayer` yet — many consumers don't check.
- The doc comment on `GetPersistentIDFromPlayerID` describes behaviour that no longer exists (lazy setup + "Workbench hack"); `technical-design.md:237` wrongly places comms/UI components on the controller.
- Full defect list (14 items): see `implementation.md` § Known Issues / Technical Debt.

---

*This context file was created retrospectively by analyzing existing code.*
