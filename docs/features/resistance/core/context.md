# Resistance Core (Faction Manager) - Context & Decisions

**Last Updated:** 2026-08-09
**Current Phase:** Retrospective Documentation
**Status:** ✅ Documented (Existing Feature)

> **2026-08-09:** The mobile FOB lifecycle (truck, deploy/undeploy, map presence, FOB rules) is now its own feature — **`resistance/fob`** — with its own docs and bug set (BUG-119…128). Core keeps the FOB *registry* only. FOB-specific gotchas below have moved there; several described the pre-BUG-046 state and are fixed in source.

---

## Quick Status

**What's Done:**
- ✅ Feature fully implemented (existing code)
- ✅ Retrospective documentation created (thorough code investigation, 2026-08-02)

**What's Next:**
- 📋 Review for potential improvements — the broken client "Make Officer" path is the highest-value core fix (the FOB fall-throughs and garrison RPC validation were fixed under BUG-046/047; the live FOB defect set is tracked in `resistance/fob`)

**Blockers:**
- None

---

## Key Files

- `Scripts/Game/GameMode/Managers/Factions/OVT_ResistanceFactionManager.c` — manager + `OVT_CampData`/`OVT_FOBData` records, `PlaceItem`/`BuildItem` endpoints (FOB deploy/undeploy state machine documented in `resistance/fob`)
- `Scripts/Game/Persistence/Serializers/Components/OVT_ResistanceManagerSerializer.c` — save/load (+ `OVT_PersistedCamp`/`OVT_PersistedFOB`)
- `Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c` — client→server funnel (`RpcAsk_AddGarrison{,Camp,FOB}`, camp privacy/delete, place/build; FOB RpcAsks documented in `resistance/fob`)
- `Scripts/Game/UserActions/OVT_ManageBaseAction.c` (base **or** FOB menu), `OVT_ManageCampAction.c`, `OVT_SaveOfficerLoadoutAction.c` (FOB actions in `resistance/fob`)
- `Scripts/Game/UI/Context/OVT_ResistanceMenuContext.c` (funds/tax/Make Officer), `OVT_FOBMenuContext.c` (garrisons, also for camps), `OVT_CampMenuContext.c` (privacy/delete)
- `Scripts/Game/UI/Map/OVT_MapIcons.c` (camp/FOB icons), `OVT_MapContext.c` (fast travel gating)
- `Scripts/Game/GameMode/Placeables/OVT_PlaceableCampHandler.c` — handoff from resistance/building → `RegisterCamp`
- Config: game-mode prefab `Prefabs/GameMode/OVT_OverthrowGameMode.et:157` (placeables/buildables configs, mobile FOB prefab pair, vehicle upgrade tree)

---

## Important Decisions

- **Records matched by persistentId for persistence, by position on the wire:** saves match `CAMP_/FOB_<time>_<rand>` ids (position fallback for pre-id saves); live RPCs match records by location — sometimes exact equality (remove/privacy), sometimes 10 m tolerance (priority). The exact-equality sites are the fragile ones.
- **FOB deploy/undeploy = prefab swap + async container transfer:** documented in `resistance/fob` (Important Decisions there), including the post-BUG-046 validate-before-spawn and busy-guard state.
- **Officer state lives on `OVT_PlayerData`** (player manager owns storage, JIP and persistence); this manager only fronts `IsOfficer`/`AddOfficer`. `AddOfficer` is broadcast-only — valid from server callers, a silent no-op from clients.
- **Garrisons respawn from prefab lists, never restore entities:** serializer prefers live groups (empty = wiped = deliberately dropped), `SpawnGarrisons` replays on `PostGameStart` for new and continued campaigns alike.
- **The serializer refuses to guess:** empty/unknown saved player-faction keys keep the live config (EPF used to substitute "FIA"); re-apply is idempotent (update-in-place by persistentId) so live re-application can't duplicate camps.
- **`PlaceItem`/`BuildItem` live here, not in the build feature:** the building UI is client-side; this manager is the server endpoint that spawns, charges, associates (`FindNearestBase`) and registers for persistence (`OVT_PersistenceTracking.Track`).

---

## Gotchas & Learnings

- **"Make Officer" only works where the clicker is the server:** `OVT_ResistanceMenuContext.c:206` calls `AddOfficer` client-side; the broadcast is dropped and no `RpcAsk` exists — on dedicated servers the promotion happens only on the promoter's screen.
- **Superseded by fixes (BUG-046/047, verified in source 2026-08-09):** the deploy/undeploy spawn-before-validate fall-throughs, the dead completion-handler unsubscribe, exact-equality FOB unregistration, and client-side garrison charging that used to be listed here are fixed. Current FOB gotchas live in `resistance/fob`'s context.md; the officer gate is still client-side theatre — now **BUG-122** there.
- **Removing a camp leaves its garrison AI alive** — `garrisonEntities` are never despawned on removal, only forgotten (FOB half: BUG-125).
- **Camps are called `fob` all over the manager** (`SpawnGarrisons`, `RegisterCamp`, `GetNearestCampData`...) — read variable *types*, not names.
- **`camp.id`/`fob.id` are never set on live registration** despite the serializer comment claiming every insertion site does — only `ApplyPersistedResistance` derives them; nothing consumes them.
- The FOB garrison menu spawns and deletes every group prefab on open just to read names/costs (`OVT_FOBMenuContext.c:40-59`); equipment cost is a hardcoded `300 × soldiers` placeholder with a To-do.
- Map icons color camps/FOBs by hardcoded faction key "FIA" (`OVT_MapIcons.c:654/:679`), not the configured player faction.

---

*This context file was created retrospectively by analyzing existing code.*
