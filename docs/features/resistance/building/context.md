# Resistance Building - Context & Decisions

**Last Updated:** 2026-08-02
**Current Phase:** Retrospective Documentation
**Status:** ✅ Documented (Existing Feature)

---

## Quick Status

**What's Done:**
- ✅ Feature fully implemented (existing code)
- ✅ Retrospective documentation created (thorough code investigation, 2026-08-02)

**What's Next:**
- 📋 Review for potential improvements — server-side validation (indices/rules/limits/funds), the missing server-side buildable charge, and multiplayer removal mode (EntityID over the wire + unreplicated ownership) are the highest-value fixes

**Blockers:**
- None

---

## Key Files

- `Scripts/Game/UI/Context/OVT_PlaceContext.c` / `OVT_BuildContext.c` — client flows: rule evaluation (`CanPlace`/`CanBuild`), ghost preview, paging, removal mode; BuildContext adds the `SCR_ManualCamera` build camera
- `Scripts/Game/Configuration/OVT_PlaceablesConfig.c` / `OVT_BuildablesConfig.c` — config model; instances in `Configs/Resistance/placeables.conf` (8) + `buildables.conf` (7)
- `Scripts/Game/GameMode/Placeables/` — `OVT_PlaceableHandler` `OnPlace` hooks (camp registration, town support modifier); README is stale
- `Scripts/Game/Components/OVT_PlaceableComponent.c` / `OVT_BuildableComponent.c` — twin marker components (owner UID, associated base ID + `EOVTBaseType`); server-side only, nothing replicates
- `Scripts/Game/Utilities/OVT_ItemLimitChecker.c` — location resolution + item counting for the house/camp/FOB limits (`Overthrow_Config.json`: 20/40/100)
- `Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c:676-708` — the three RPC endpoints (no validation)
- `Scripts/Game/GameMode/Managers/Factions/OVT_ResistanceFactionManager.c:599-746, 1306-1390` — `PlaceItem`/`BuildItem`/`RemovePlacedItem`, `FindNearestBase`, `CleanupCampObjects`
- `Scripts/Game/Persistence/Serializers/Components/OVT_PlaceableComponentSerializer.c` / `OVT_BuildableComponentSerializer.c` + `Configs/Systems/Persistence/Overthrow.conf` (SelfSpawn configs) — persistence round-trip

---

## Important Decisions

- **Client-side validation, fire-and-forget RPCs:** all rules, limits and affordability run in the UI contexts; the server spawn path validates nothing and cannot reject. Latency-free UX, wide-open trust boundary.
- **Marker components with stamped association:** owner + nearest camp/FOB/base written once at spawn and persisted, never re-derived — cleanup and limits stay stable when the map changes around an item.
- **Instant construction:** no build progress or funding pool; "building" differs from "placing" only in menu, camera, allowed locations and who charges the money.
- **Vanilla persistence via SelfSpawn:** runtime-spawned entities re-create themselves from saves (prefab + transform + component state); placeables also persist inventory contents. EPF's `OVT_PlaceableData` twins are gone.
- **XP + economy via invokers:** `m_OnPlace`/`m_OnBuild` decouple skills/economy from the spawn pipeline.

---

## Gotchas & Learnings

- **`BuildItem` never charges server-side** (`OVT_ResistanceFactionManager.c:670-717`) — the client pays through a generic client-priced `TakePlayerMoney` RPC (`OVT_BuildContext.c:533`). `PlaceItem` charges server-side (:646). Free building for a hostile client.
- **No server validation at all:** `RpcAsk_PlaceItem`/`RpcAsk_BuildItem` index config arrays unchecked (server VM error on bad indices), skip rules/limits, and `DoTakePlayerMoney` clamps at 0 — under-funded purchases succeed.
- **Removal mode is dedicated-server-broken twice:** client-local `EntityID` sent over the network (`OVT_PlaceContext.c:733`), and owner UID never replicates so the client-side permission check (`:688-700`) always fails for non-officers.
- **Pagination int-division** (`OVT_PlaceContext.c:144`, `OVT_BuildContext.c:161`): `Math.Ceil(8/14)` = 0 pages → "1/0" display, and Next drives the loop to `m_aPlaceables[-14]` (out-of-bounds).
- **`CanPlace` has a dead duplicate `m_bAwayFromBases` block** (:315-327) and dereferences `GetNearestBase` without null checks (:254, :278, :362) — BuildContext and the limit checker both guard.
- **`FindNearestBase` has no distance cap** — items associate with camps/FOBs/bases arbitrarily far away; house limits count by radius, camp/FOB/base limits by association, so the two systems disagree.
- **A camp tent associates with the *previous* nearest location**, not itself — association is stamped before the handler registers the camp.
- Item limits are enforced only client-side; `BuildItem` ignores its handler's return value (contrast `PlaceItem`).
- `RemoveCamp` in the same manager already solves delete-by-network-reference correctly with `RplId` — the fix pattern for `RemovePlacedItem` is local.
- `m_aFurniturePrefabs` is dead config; `Placeables/README.md` documents a nonexistent `OVT_PlaceableFOBHandler` and claims EPF persistence.

---

*This context file was created retrospectively by analyzing existing code.*
