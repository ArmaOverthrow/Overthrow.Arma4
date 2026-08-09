# FOB (Mobile Forward Operating Base) - Context & Decisions

**Last Updated:** 2026-08-09
**Current Phase:** Retrospective Documentation
**Status:** ✅ Documented (Existing Feature)

---

## Quick Status

**What's Done:**
- ✅ Feature fully implemented (existing code); carved out of `resistance/core`'s docs into its own feature 2026-08-09
- ✅ Retrospective documentation created (very-thorough code investigation, 2026-08-09)
- ✅ Ten defects found during discovery filed as **BUG-119…128** (`linkedFeature: resistance/fob`)

**What's Next:**
- 📋 Highest-value fixes: BUG-122 (officer gate bypass via vehicle upgrade, client-side payment) and BUG-124 (undeploy deletes neighbours' property); then the BUG-121+125 error-path pair

**Blockers:**
- None

---

## Key Files

- `Scripts/Game/GameMode/Managers/Factions/OVT_ResistanceFactionManager.c` — `OVT_FOBData`/`m_FOBs` (registry hosted by `resistance/core`), deploy `:466` / undeploy `:557`, completion/error handlers `:1711-1887`, register/unregister `:1106-1127`, `CleanupFOBArea` `:1630`, shared deploy constants `:60-64`
- `Scripts/Game/Components/Controller/OVT_ContainerTransferComponent.c` — per-player cargo mover on `OVT_OverthrowController` (the one migrated piece); `TransferStorageForDeployment` is dead (BUG-128)
- `Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c:1222-1258, 1984-2003` — legacy RpcAsk funnel (deploy/undeploy resolved-sender; priority unauthenticated, BUG-123)
- `Scripts/Game/UserActions/` — `OVT_DeployFOBAction` (client pre-checks), `OVT_UndeployFOBAction`, `OVT_SetPriorityFOBAction`, `OVT_SetHomeAction`, `OVT_ManageBaseAction`; `OVT_UndeployFOBAction_New.c` is a dead migration sketch
- `Prefabs/Vehicles/Wheeled/M923A1/OverthrowMobileFOB*.et` — truck / deployed / two cargo-canvas prefabs (actions live on `door_rear`)
- `Scripts/Game/UI/Map/OVT_MapIcons.c:664-692`, `OVT_MapRestrictedAreas.c`, `OVT_MapContext.c:121-123` — icons, no-deploy overlay, fast travel
- `Scripts/Game/UI/Context/OVT_FOBMenuContext.c` — garrison menu (double-duty for camps)
- `Scripts/Game/Persistence/Serializers/Components/OVT_ResistanceManagerSerializer.c` — `OVT_PersistedFOB`
- `Prefabs/GameMode/OVT_OverthrowGameMode.et:185-197` — prefab pair + M923A1 upgrade; `Configs/Pricing/vehiclePrices.conf:88` — price 5000

---

## Important Decisions

- **The deployed FOB is a physics-frozen *vehicle*, not a structure** — prefab swap at the same transform keeps ownership, registry, despawn management and persistence on the vehicle path for free. Everything that special-cases vehicles (wanted level, vehicle persistence group) sees it.
- **Cargo moves through the *initiating player's* `OVT_ContainerTransferComponent`**; the manager holds in-flight op state as bare members with one busy-guard — one FOB operation server-wide at a time, plus disconnect recovery driving the error handlers.
- **Error paths bias toward destruction over duplication** (post-BUG-046): a failed deploy still registers + deletes the truck; a failed undeploy deletes the deployed FOB. Record/garrison conservation was not part of that invariant — hence BUG-121/125.
- **Wire identity is position + 10 m tolerance** everywhere (`RpcDo_RemoveFOB`, priority); `persistentId` is save-matching only, never a wire key.
- **Deploy exclusion zones derive from shared constants** (`FOB_DEPLOY_BASE_BUFFER = 50` over `baseCloseRange`, `FOB_DEPLOY_TOWER_RANGE = 70`) consumed by client pre-check, server validation *and* the map overlay — the BUG-070 lesson institutionalized. Applies to **all** bases/towers, friendly included (blue circles).

---

## Gotchas & Learnings

- **`resistance/core`'s 2026-08-02 Known Issues describe the pre-BUG-046 state** — the fixes (validate-before-spawn, stored-component unsubscribe, 10 m unregistration, disconnect recovery) are in source now. Trust this feature's docs for FOB current-state.
- **The officer gate is client-side theatre with a UI-reachable bypass**: `OVT_ManageVehicleContext.Upgrade()` converts any M923A1 transport for any player, client-charged (BUG-122). The shop gate string-matches the prefab *path*.
- **Undeploy is a 75 m area wipe with no ownership filter** (BUG-124) — `CleanupCampObjects` filters by `BelongsTo`; `CleanupFOBArea` never did.
- **Failed undeploy = permanent phantom FOB** (BUG-121): record never unregistered; icon + fast-travel anchor + 100 m build zone survive saves.
- **Empty registry ⇒ `GetNearestFOB` returns the zero vector** (BUG-126) — fast travel/build checks against it pass near world origin.
- **FOBs have no name anywhere** (BUG-119) and the deploy announcement has no notification preset so it never shows (BUG-120 — `if(!preset) return;` drops it silently). When adding a broadcast message, the tag **must** exist in `overthrowBroadcastMessages.conf`; there is no error path.
- **The action components sit on the cargo canvas child** — deploy passes `GetParent()` (the truck); `OVT_SetPriorityFOBAction` passes the canvas itself and only works because of the 10 m tolerance.
- **Driving the truck is instant wanted level 4** via hardcoded prefab GUID `{E6A31A5A6EA0AF04}` (`OVT_PlayerWantedComponent.c:694`).
- **`LocalPlayerIsOfficer` derefs `player.isOfficer` unguarded** (`OVT_PlayerManagerComponent.c:487-493`) — reached from every FOB action's `CanBeShownScript`; JIP timing can plausibly null it.
- **Name collision, no relation**: `OVT_DeployFOBAction` vs the occupying epic's "deployments" system — unrelated. Also camps are called `fob` in variables all over the manager; read types, not names.
- `OVT_ResistanceFOBControllerComponent` is an empty class on two flag prefabs; two READMEs describe FOB machinery that doesn't exist (`Controllers/README.md:46`, `Placeables/README.md:12`).

---

*This context file was created retrospectively by analyzing existing code.*
