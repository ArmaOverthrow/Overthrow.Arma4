# Resistance Core (Faction Manager) - Context & Decisions

**Last Updated:** 2026-08-02
**Current Phase:** Retrospective Documentation
**Status:** ✅ Documented (Existing Feature)

---

## Quick Status

**What's Done:**
- ✅ Feature fully implemented (existing code)
- ✅ Retrospective documentation created (thorough code investigation, 2026-08-02)

**What's Next:**
- 📋 Review for potential improvements — the broken client "Make Officer" path, the FOB deploy/undeploy fall-through duplication, and the unvalidated garrison RpcAsks are the highest-value fixes

**Blockers:**
- None

---

## Key Files

- `Scripts/Game/GameMode/Managers/Factions/OVT_ResistanceFactionManager.c` (1663 L) — manager + `OVT_CampData`/`OVT_FOBData` records, FOB deploy/undeploy state machine, `PlaceItem`/`BuildItem` endpoints
- `Scripts/Game/Persistence/Serializers/Components/OVT_ResistanceManagerSerializer.c` — save/load (+ `OVT_PersistedCamp`/`OVT_PersistedFOB`)
- `Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c` — client→server funnel (`RpcAsk_DeployFOB`, `RpcAsk_AddGarrison{,Camp,FOB}`, camp privacy/delete, priority FOB, place/build)
- `Scripts/Game/UserActions/OVT_DeployFOBAction.c`, `OVT_UndeployFOBAction.c` (+ dead `OVT_UndeployFOBAction_New.c`), `OVT_SetPriorityFOBAction.c`, `OVT_ManageBaseAction.c` (base **or** FOB menu), `OVT_ManageCampAction.c`, `OVT_SaveOfficerLoadoutAction.c`
- `Scripts/Game/UI/Context/OVT_ResistanceMenuContext.c` (funds/tax/Make Officer), `OVT_FOBMenuContext.c` (garrisons, also for camps), `OVT_CampMenuContext.c` (privacy/delete)
- `Scripts/Game/UI/Map/OVT_MapIcons.c` (camp/FOB icons), `OVT_MapContext.c` (fast travel gating)
- `Scripts/Game/GameMode/Placeables/OVT_PlaceableCampHandler.c` — handoff from resistance/building → `RegisterCamp`
- `Scripts/Game/Controllers/ResistanceFaction/OVT_ResistanceFOBControllerComponent.c` — empty marker on FOB flag prefabs, queried by nothing
- Config: game-mode prefab `Prefabs/GameMode/OVT_OverthrowGameMode.et:157` (placeables/buildables configs, mobile FOB prefab pair, vehicle upgrade tree)

---

## Important Decisions

- **Records matched by persistentId for persistence, by position on the wire:** saves match `CAMP_/FOB_<time>_<rand>` ids (position fallback for pre-id saves); live RPCs match records by location — sometimes exact equality (remove/privacy), sometimes 10 m tolerance (priority). The exact-equality sites are the fragile ones.
- **FOB deploy/undeploy = prefab swap + async container transfer:** counterpart vehicle spawned at the same transform, cargo moved through the *initiating player's* `OVT_ContainerTransferComponent`, completion callbacks on the manager finish the swap. Operation state is bare manager members — one op at a time, by hope not by lock.
- **Officer state lives on `OVT_PlayerData`** (player manager owns storage, JIP and persistence); this manager only fronts `IsOfficer`/`AddOfficer`. `AddOfficer` is broadcast-only — valid from server callers, a silent no-op from clients.
- **Garrisons respawn from prefab lists, never restore entities:** serializer prefers live groups (empty = wiped = deliberately dropped), `SpawnGarrisons` replays on `PostGameStart` for new and continued campaigns alike.
- **The serializer refuses to guess:** empty/unknown saved player-faction keys keep the live config (EPF used to substitute "FIA"); re-apply is idempotent (update-in-place by persistentId) so live re-application can't duplicate camps.
- **`PlaceItem`/`BuildItem` live here, not in the build feature:** the building UI is client-side; this manager is the server endpoint that spawns, charges, associates (`FindNearestBase`) and registers for persistence (`OVT_PersistenceTracking.Track`).

---

## Gotchas & Learnings

- **"Make Officer" only works where the clicker is the server:** `OVT_ResistanceMenuContext.c:206` calls `AddOfficer` client-side; the broadcast is dropped and no `RpcAsk` exists — on dedicated servers the promotion happens only on the promoter's screen.
- **Deploy/undeploy spawn before they validate:** if the player's transfer component is busy or missing, the method returns mid-swap — duplicate FOB vehicles (and cargo) in-world. `IsAvailable()` false is reachable by any player at will.
- **The completion-handler unsubscribe never runs:** `OVT_OverthrowController.Cast(GetOwner())` at `OVT_ResistanceFactionManager.c:1518/:1565/:1606/:1648` — `GetOwner()` is the *game mode*; subscription used the player's controller. Stale handlers accumulate on players' transfer components.
- **Garrison RpcAsks trust the client completely:** unchecked `baseId`/`prefabIndex` array indices and a nullable nearest-camp lookup → server crash vectors; payment is taken client-side in `OVT_FOBMenuContext.c:85`, so the server never charges anyone.
- **Removing a camp/FOB leaves its garrison AI alive** — `garrisonEntities` are never despawned on removal, only forgotten.
- **FOB unregistration matches positions with `==`:** if the deployed FOB settled at all since registration, the record survives as a ghost (map icon + fast travel). The priority-FOB RPC already does it right (10 m tolerance) three functions away.
- **Camps are called `fob` all over the manager** (`SpawnGarrisons`, `RegisterCamp`, `GetNearestCampData`...) — read variable *types*, not names.
- **`camp.id`/`fob.id` are never set on live registration** despite the serializer comment claiming every insertion site does — only `ApplyPersistedResistance` derives them; nothing consumes them.
- **`mobileFOBOfficersOnly` is client-side theatre** — enforced in `CanBeShownScript` and the shop UI; the server RpcAsks accept anyone.
- The FOB garrison menu spawns and deletes every group prefab on open just to read names/costs (`OVT_FOBMenuContext.c:40-59`); equipment cost is a hardcoded `300 × soldiers` placeholder with a To-do.
- Map icons color camps/FOBs by hardcoded faction key "FIA" (`OVT_MapIcons.c:654/:679`), not the configured player faction.

---

*This context file was created retrospectively by analyzing existing code.*
