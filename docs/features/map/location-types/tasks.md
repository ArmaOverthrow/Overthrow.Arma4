# Map Location Types - Task Checklist

**Last Updated:** 2026-08-11
**Progress:** ✅ **42/42 tasks complete (100%) — FEATURE COMPLETE 2026-08-11.** Phases 1–7 built and
play-tested green, the caret art redraw landed, and the seven deferred findings are filed as
**BUG-138 … BUG-144** against `map/core`, `resistance/fob` and `economy/shops` (Phase 7b).

> Derived from `implementation.md` §5. Phase order is load-bearing: the bus-stop migration
> (Phase 1) is the riskiest item and `map/fast-travel` F5 is blocked on it.
>
> **Advanced phases:** Phase 1 (`component-developer-advanced`) and Phase 5 (`ui-developer-advanced`).

---

## Phase 1: Marker component, registry, bus-stop migration (G4) — **ADVANCED** (8/8 complete) ✅

- [x] ✅ **Create `OVT_MapMarkerComponent`**
  - Description: `OVT_Component` subclass. `[Attribute] OVT_MapMarkerCategory m_eCategory` (enum `BUS_STOP`/`POI`), `ref SCR_UIInfo m_UiInfo`, `bool m_bMustOwnBase`. `OnPostInit` → `CallLater(Register, 0)`; `OnDelete` → unregister. Guard `if (SCR_Global.IsEditMode()) return;`
  - File(s): `Scripts/Game/Components/Map/OVT_MapMarkerComponent.c` (NEW)
  - Estimate: 🟡 1-2 hours

- [x] ✅ **Create `OVT_MapMarkerManagerComponent` registry**
  - Description: `GetInstance()` singleton; idempotent `RegisterMarker`/`UnregisterMarker`; `GetMarkers(category)`; `GetNearestMarker(pos, category, maxDist)`. One `QueryEntitiesBySphere("0 0 0", 99999999, Check, Filter, EQueryEntitiesFlags.STATIC)` at init, **outside any `Replication.IsServer()` guard** (pattern: `InitializePorts` `:1649-1656`). Log the scan count at init (R1 mitigation).
  - File(s): `Scripts/Game/GameMode/Managers/OVT_MapMarkerManagerComponent.c` (NEW)
  - Estimate: 🔴 2-3 hours

- [x] ✅ **Register the manager on the game-mode prefab**
  - Description: Add the manager component block with a fresh unique GUID. `OVT_TownManagerComponent` at `:212` is the shape to copy.
  - File(s): `Prefabs/GameMode/OVT_OverthrowGameMode.et`
  - Estimate: 🟢 < 1 hour

- [x] ✅ **Add `OVT_Global.GetMapMarkers()`**
  - Description: Static accessor alongside the existing manager accessors (`:141-226`), resolving through `GetInstance()`.
  - File(s): `Scripts/Game/Global/OVT_Global.c`
  - Estimate: 🟢 < 1 hour

- [x] ✅ **Add the marker component to the bus-stop prefab delta**
  - Description: Add `OVT_MapMarkerComponent { m_eCategory BUS_STOP }` to the **existing** Overthrow delta (N9). Vanilla has exactly one bus-stop prefab, so one file covers every world-placed stop in every world with no world editing.
  - File(s): `Prefabs/Structures/Signs/Traffic/SignBusStop_01.et`
  - Estimate: 🟢 < 1 hour

- [x] ✅ **Create `OVT_MapLocationBusStop` + config entry**
  - Description: Registry query over category `BUS_STOP`, one record per marker. Icon **`bus`** — the art landed 2026-08-10 (`overthrow_mapicons.imageset:101`), so the K8 `port` fallback is **not** used. Fresh GUID for the conf entry. Set `m_fVisibilityZoom`/`m_fShowNameZoom` deliberately — bus stops are a dense type (R6).
  - File(s): `Scripts/Game/UI/Map/LocationTypes/OVT_MapLocationBusStop.c` (NEW), `Configs/Map/OverthrowMap.conf`
  - Estimate: 🟡 1-2 hours

- [x] ✅ **Retire `GetNearestBusStop` / `FindBusStop`; re-point the caller**
  - Description: Delete `OVT_TownManagerComponent.GetNearestBusStop` (`:881`) and `FindBusStop` (`:1197-1203`); re-point `OVT_MapContext.c:453` at `OVT_Global.GetMapMarkers().GetNearestMarker(pos, BUS_STOP, 15)`. **Keep the 15 m radius** — changing it changes bus-travel behaviour, which is `map/fast-travel`'s call.
  - File(s): `Scripts/Game/GameMode/Managers/OVT_TownManagerComponent.c`, `Scripts/Game/UI/Context/OVT_MapContext.c`
  - Estimate: 🟡 1-2 hours

- [x] ✅ **Duplicate-icon check (R2)**
  - Description: Vanilla's `SCR_MapDescriptorComponent { MainType "Bus Stop" }` still merges in from the base prefab (same-GUID overrides are **deltas**). If vanilla draws it on the fullscreen map, suppress it in the Overthrow delta; if it does not, record that finding in `context.md` and move on.
  - File(s): `Prefabs/Structures/Signs/Traffic/SignBusStop_01.et`, `context.md`
  - Estimate: 🟢 < 1 hour

**Acceptance:** `tools/compile-check.sh` exit 0 · every world bus stop shows exactly one Overthrow marker · `OVT_CatchBusAction` → map → click a stop still works · no `GetNearestBusStop`/`MDT_BUSSTOP` reference outside vanilla · a pre-migration save loads identically.

---

## Phase 2: POI migration (G3), remove the `RegisterPOI` dependency (5/5 complete) ✅

- [x] ✅ **Create `OVT_MapLocationPOI` + config entry**
  - Description: Registry query over category `POI`. Name/icon from the marker's `SCR_UIInfo` (`GetName()`, `GetIconPath()`/imageset+icon). `m_fVisibilityZoom ≈ 1.0` (legacy showed the real icon only at zoom ≥ 1). Fresh GUID.
  - File(s): `Scripts/Game/UI/Map/LocationTypes/OVT_MapLocationPOI.c` (NEW), `Configs/Map/OverthrowMap.conf`
  - Estimate: 🟡 1-2 hours

- [x] ✅ **Preserve legacy POI gating exactly**
  - Description: `m_bMustOwnBase` ⇒ nearest base exists **and** within 220 m **and** `!base.IsOccupyingFaction()`. Both halves of legacy's gating (`OVT_MainMenuContextOverrideComponent.c:45-53` at registration; `OVT_MapIcons.c:495-498` at display). Evaluate at **populate** time — **not** in `CanFastTravel`/`ShouldShowLocation` (hot path, Q-7).
  - File(s): `Scripts/Game/UI/Map/LocationTypes/OVT_MapLocationPOI.c`
  - Estimate: 🟡 1-2 hours

- [x] ✅ **Add POI markers to the three live prefabs**
  - Description: `OVT_MapMarkerComponent { m_eCategory POI; m_UiInfo <copy of existing SCR_UIInfo>; m_bMustOwnBase <same> }`. **Copy the existing `SCR_UIInfo` values; do not invent new ones.** Garage_E_02 keeps `m_bMustOwnBase 1`; the other two keep 0.
  - File(s): `Prefabs/Structures/Industrial/Garages/Garage_E_02/Garage_E_02.et`, `Prefabs/Structures/Military/Houses/GarageMilitary_E_01/GarageMilitary_E_01_base.et`, `Prefabs/Structures/Military/FOB/OVT_VehicleMaintenanceRamp.et`
  - Estimate: 🟡 1-2 hours

- [x] ✅ **Strip the `RegisterPOI` branch from `OVT_MainMenuContextOverrideComponent`**
  - Description: Delete the `EOnFrame` map-registration branch, the `m_bRegistered` flag, `m_bShowOnMap`, and the now-unneeded `EntityEvent.FRAME` in the event mask. **Leave `m_ContextName`, `m_fRange`, `m_bMustBeDriving` and `CanShow` untouched** — the in-world menu role is not this feature's.
  - File(s): `Scripts/Game/Components/OVT_MainMenuContextOverrideComponent.c`
  - Estimate: 🟡 1-2 hours

- [x] ✅ **Verify no `OVT_MapIcons.` references remain (F-2)**
  - Description: `grep -rn "OVT_MapIcons\." --include=*.c Scripts/ | grep -v "UI/Map/OVT_MapIcons.c"` must return nothing.
  - File(s): (grep only)
  - Estimate: 🟢 < 1 hour

**Acceptance:** F-2 grep clean · garages and maintenance ramps render with their authored icons/names · `Garage_E_02` markers appear only near a **resistance-held** base and disappear when it flips · a newly built ramp appears on the **next** map open without a restart · compile exit 0.

---

## Phase 3: Vehicle type (G1), house privacy (N1), home marker (N3), T2/T3/T1 (6/6 complete) ✅

- [x] ✅ **Create `OVT_MapLocationVehicle` + config entry**
  - Description: `m_Vehicles.GetOwned(localPersId)` → `set<EntityID>`; resolve each entity, skip nulls. **Local player only** (matches legacy `:710`). Icon `vehicle` (already in the imageset). Uses the **inherited** `m_Vehicles` cache — closes T2. Fresh GUID.
  - File(s): `Scripts/Game/UI/Map/LocationTypes/OVT_MapLocationVehicle.c` (NEW), `Configs/Map/OverthrowMap.conf`
  - Estimate: 🟡 1-2 hours

- [x] ✅ **Vehicle icon yaw rotation**
  - Description: `OnSetupIconWidget` → `image.SetRotation(ent.GetYawPitchRoll()[0])` (legacy parity, `OVT_MapIcons.c:727`).
  - File(s): `Scripts/Game/UI/Map/LocationTypes/OVT_MapLocationVehicle.c`
  - Estimate: 🟢 < 1 hour

- [x] ✅ **🔴 N1 — fix the house privacy leak**
  - Description: Rewrite `PopulateLocations` to use `m_RealEstate.GetOwned(localPersId)` / `GetRented(localPersId)` (`OVT_OwnerManagerComponent.c:224`, `:243` — exactly what legacy called at `OVT_MapIcons.c:472`/`:543`) instead of iterating every player's `m_mOwned`/`m_mRented`. **Leave `OVT_MapLocationWarehouse` alone** — public is intentional (N2).
  - File(s): `Scripts/Game/UI/Map/LocationTypes/OVT_MapLocationHouse.c`
  - Estimate: 🟡 1-2 hours

- [x] ✅ **N3 — distinguish the player's home**
  - Description: Add an `isHome` key via `m_RealEstate.IsHome(persId, entityId)` (`:595`) and a distinct home colour/icon. Keep rented houses visually distinct as today.
  - File(s): `Scripts/Game/UI/Map/LocationTypes/OVT_MapLocationHouse.c`
  - Estimate: 🟢 < 1 hour

- [x] ✅ **N4 — remove the inert `visibilityZoom` writes**
  - Description: Delete the per-location `visibilityZoom` data writes from House (`:45`, `:79`) and FOB (`:39`) so nobody reads them as working behaviour. **Do not change `map/core`** — the element reads only the type-level value (K9); the fix is filed, not made here.
  - File(s): `Scripts/Game/UI/Map/LocationTypes/OVT_MapLocationHouse.c`, `Scripts/Game/UI/Map/LocationTypes/OVT_MapLocationFOB.c`
  - Estimate: 🟢 < 1 hour

- [x] ✅ **T3/T1 — data-key constants, opportunistically**
  - Description: Introduce `OVT_MapDataKeys.c` with `static const string` constants for the shared key sets (House/Warehouse ownership; FOB/Camp owner/persistentId/garrison) and use them **in every type this feature touches only**. Same for T1: touched types switch to the inherited manager cache. Do **not** refactor `Base`/`RadioTower`/`Town`.
  - File(s): `Scripts/Game/UI/Map/Core/OVT_MapDataKeys.c` (NEW), touched location types
  - Estimate: 🟡 1-2 hours

**Acceptance:** your own vehicles appear at their positions with correct facing, **another player's do not** · only your own owned/rented houses appear; home visually distinguished · compile exit 0 · `tools/run-tests.sh "{6A6E2A002F53A581}"` still green.

---

## Phase 4: Waypoint markers (G2) (3/3 complete) ✅

- [x] ✅ **Add `m_vRecruitWaypoint` to the job manager**
  - Description: `vector m_vRecruitWaypoint;` next to `m_vCurrentWaypoint` (`:64`), with a comment recording that both are client-local, non-replicated, non-persisted. Re-check git state before editing (R9 — parallel sessions commit mid-work in this tree).
  - File(s): `Scripts/Game/GameMode/Managers/OVT_JobManagerComponent.c`
  - Estimate: 🟢 < 1 hour

- [x] ✅ **Point the recruits context at the new slot**
  - Description: `OVT_RecruitsContext.ShowOnMap` (`:489`) writes `m_vRecruitWaypoint` instead of `m_vCurrentWaypoint`. Nothing else in that context changes. This is what stops "show job on map" and "show recruit on map" clobbering each other.
  - File(s): `Scripts/Game/UI/Context/OVT_RecruitsContext.c`
  - Estimate: 🟢 < 1 hour

- [x] ✅ **Create `OVT_MapLocationWaypoint` + config entry**
  - Description: Up to two records (job slot, recruit slot); skip a slot when it is `"0 0 0"` (same emptiness test legacy used, `OVT_MapIcons.c:780`). Icon `waypoint` (exists). `m_fVisibilityZoom 0` (legacy always-visible), `m_bShowName 1`, distinct colour and display name per slot. Fresh GUID.
  - File(s): `Scripts/Game/UI/Map/LocationTypes/OVT_MapLocationWaypoint.c` (NEW), `Configs/Map/OverthrowMap.conf`
  - Estimate: 🟡 1-2 hours

**Acceptance:** Jobs-menu "show on map" places a job waypoint; Recruits-menu "show on map" places a recruit waypoint; **both visible at once** and neither clears the other · compile exit 0.

---

## Phase 5: Shared info-row mechanism + non-shop panels — **ADVANCED** (6/6 complete) ✅

- [x] ✅ **Create the row widget + handler**
  - Description: `OVT_MapInfoRow.layout` with `RowLabel` (TextWidget), `RowValue` (TextWidget), `RowIcon` (ImageWidget, hidden unless set) + `OVT_MapInfoRowHandler : SCR_ScriptedWidgetComponent` caching them in `HandlerAttached` and exposing `Init(label, value, imageset, icon)`. Modelled **exactly** on `OVT_TownModifierWidget.layout` + `OVT_TownModifierWidgetHandler`.
  - File(s): `UI/Layouts/Map/Core/OVT_MapInfoRow.layout` (NEW), `Scripts/Game/UI/Map/Core/OVT_MapInfoRowHandler.c` (NEW)
  - Estimate: 🟡 1-2 hours

- [x] ✅ **Create the rows container layout**
  - Description: `OVT_MapInfoRows.layout` — container with a vertical layout named **`Rows`**.
  - File(s): `UI/Layouts/Map/Core/OVT_MapInfoRows.layout` (NEW)
  - Estimate: 🟢 < 1 hour

- [x] ✅ **K5 — extend the `OVT_MapLocationType` contract (additive)**
  - Description: Add `[Attribute] ResourceName m_SharedInfoLayout`, the `BuildInfoRows(location, rowsContainer)` virtual, and `AddInfoRow` / `AddInfoIconRow` / `ClearInfoRows` helpers. `UpdateInfoPanel` gains an `else if (!m_SharedInfoLayout.IsEmpty())` branch (§4.5). **Existing behaviour unchanged when `m_InfoLayout` is set and when both are empty.** Record the contract change in `map/core`'s `context.md` (I-1).
  - File(s): `Scripts/Game/UI/Map/Core/OVT_MapLocationType.c`, `docs/features/map/core/context.md`
  - Estimate: 🔴 2-3 hours

- [x] ✅ **Implement `BuildInfoRows` for the nine shared-panel types**
  - Description: FOB (Priority; Garrison **only when > 0** — N5), Camp (Access; Garrison > 0), House (Status Home/Owned/Rented; Renter; Rent via `GetRentPrice` — N12), Warehouse (Owner/Renter; Contents top 3 from `GetWarehouseInventory` — N11, or "Empty"), Port (static explanatory lines), BusStop (one line), POI (`m_UiInfo.GetDescription()` when set), Vehicle (vehicle name), Waypoint (job/recruit). Row table is §4.5.
  - File(s): nine files under `Scripts/Game/UI/Map/LocationTypes/`
  - Estimate: 🔴 3+ hours

- [x] ✅ **Add the new string ids to the localization master**
  - Description: Ids listed in §10. **`Language/localization_Overthrow.st` ONLY** — never edit `localization_Overthrow.<lang>.conf` (Workbench-generated; hand-editing has corrupted all six files before). List the affected ids for the user to regenerate in Workbench.
  - File(s): `Language/localization_Overthrow.st`
  - Estimate: 🟢 < 1 hour

- [x] ✅ **Q-8 — widget-name audit**
  - Description: For **every** `FindAnyWidget` added in this feature, confirm the name exists in the corresponding `.layout`, name by name. Record the layout↔code name contract in a comment at the top of each new handler. This is exactly the D1/D2 failure mode and the compiler cannot catch it.
  - File(s): all new layouts + handlers
  - Estimate: 🟡 1-2 hours

**Acceptance:** selecting any of the nine shared-panel types shows a populated panel, not a bare header · Town/Base/RadioTower panels visually unchanged (Q-6) · every row label resolves to real text once exports are regenerated (literal text acceptable before that, and listed for the user) · readable at 1080p and legible with a gamepad cursor.

---

## Phase 6: Shop price indicator + GunDealer signature weapons (7/7 complete) ✅

- [x] ✅ **Build `OVT_MapShopPriceIndicator` (the calculator)**
  - Description: §4.6 — per-item `scarcityPct = 100 × 0.1 × (1 − townStock/max(1, townMaxStock))` with the 7-band caret table; shop-level `remotenessPct = 0.01 × distToPort` with the 4-band badge table; top-3-up / top-3-down selection skipping the neutral band; **vehicle-shop suppression**; **null-safe town-stock loop (never call `GetTownStock` — N6/Q-4)**; negative `DistanceToNearestPort` → hidden (N7); display-name cache (N8). Town resolution must use `GetNearestTown(pos)`→`GetTownID`, **not** `OVT_ShopComponent.m_iTownId`, so the panel agrees with the price charged.
  - File(s): `Scripts/Game/UI/Map/LocationTypes/Shop/OVT_MapShopPriceIndicator.c` (NEW)
  - Estimate: 🔴 3+ hours

- [x] ✅ **Create the shop info layout**
  - Description: `OVT_MapInfoShop.layout` — composes a `Rows` vertical layout (so `AddInfoRow` works unchanged) **plus** a `Badge` overlay and a `ScarcityRows` container.
  - File(s): `UI/Layouts/Map/LocationTypes/OVT_MapInfoShop.layout` (NEW)
  - Estimate: 🟡 1-2 hours

- [x] ✅ **Wire Shop to the panel**
  - Description: `OVT_MapLocationShop` sets `m_InfoLayout` and implements `OnSetupLocationInfo` (shop-type row, badge, caret rows). **Computed once, at panel-open time** (Q-7).
  - File(s): `Scripts/Game/UI/Map/LocationTypes/OVT_MapLocationShop.c`, `Configs/Map/OverthrowMap.conf`
  - Estimate: 🟡 1-2 hours

- [x] ✅ **⭐ GunDealer panel — the four signature weapons (§4.6b)**
  - Description: **Different content from Shop, same layout.** List what this dealer *stocks*, not what is dear. Filter `m_aInventory` to `stock > 0` (N15 — sold-out ids stay at 0) → keep `OVT_ShopCategory.WEAPONS` (cached via `GetItemCategory`) → resolve `SCR_EArsenalItemType` for that small residue and keep **only** RIFLE / SNIPER_RIFLE / MACHINE_GUN / ROCKET_LAUNCHER, **discarding PISTOL** (every dealer stocks every pistol, so pistols differentiate nothing — N13/N16). Cache id → arsenal type in a member map, mirroring `OVT_ShopContext.m_mDisplayNames` (`:73`). Each row: weapon name + category label + price caret. **Omit a category the dealer did not roll — never print "None".** Copy must be present-tense: the four re-roll on every campaign load (N14), so nothing may imply permanence. Remoteness badge renders unchanged — it is still meaningful for a gun dealer.
  - File(s): `Scripts/Game/UI/Map/LocationTypes/OVT_MapLocationGunDealer.c`, `Configs/Map/OverthrowMap.conf`
  - Estimate: 🔴 2-3 hours

- [x] ✅ **K8 — dual-affordance caret rendering**
  - Description: The row carries both an `ImageWidget` (`CaretIcon`) and a `TextWidget` (`CaretText`). Until the imageset has the caret glyphs, set the text and hide the image. One-line switch when the art lands. **Up vs down must not rely on colour alone.**
  - File(s): `UI/Layouts/Map/LocationTypes/OVT_MapInfoShop.layout`, shop indicator/handler
  - Estimate: 🟡 1-2 hours

- [x] ✅ **New `.st` ids for the shop panel**
  - Description: Section header, badge, vehicle-shop line, empty-shop line (§10). Master `.st` only.
  - File(s): `Language/localization_Overthrow.st`
  - Estimate: 🟢 < 1 hour

- [x] ✅ **(Optional, recommended) Logic-tier test for the banding maths**
  - Description: The caret/badge banding is pure maths over `(townStock, townMaxStock)` and `(distToPort)`. If written as a world-free static, it belongs in the **Logic** tier. **Prove the case can fail before shipping it** and record the method. No `maxAttempts`.
  - File(s): `Scripts/Game/Tests/TestSuites/Fast/…`
  - Estimate: 🟡 1-2 hours

**Acceptance:** mixed-stock shop shows ≥1 up row and ≥1 down row and **no numbers anywhere** · all-neutral shop shows the "nothing unusual" line, not an empty section · `SHOP_VEHICLE` shows the flat-price explanation, **no** carets, **no** badge · two shops in the same town show identical carets and badges (expected — it is a town signal) · far-from-port shop shows a badge, port-adjacent shows none · 20 open/close cycles do not degrade frame time.

---

## Phase 7: Verification, parity sign-off, bug filing — **user-driven, no agent** (7/7 complete) ✅

> **✅ PHASE COMPLETE.** The user play-tested the feature and reported everything green, including
> multiplayer (2026-08-11); V-3 … V-7 and the F-1 parity checklist are discharged. **7b closed the same
> day** — the seven deferred findings are filed as BUG-138 … BUG-144 against other features.

- [x] ✅ **V-3 — single-player marker sweep** — Completed 2026-08-11
  - Description: Start a campaign; buy a house, buy a vehicle, place a camp, deploy a FOB, build a maintenance ramp, accept a job and "show on map", mark a recruit on the map. Walk the F-1 checklist. Confirm F-9 and F-10.
  - **Also (new — BUG-136 landed in `map/core` after this feature was built):** live refresh is now on for these types (5 s Town/Base/RadioTower/FOB/Camp, 2 s Vehicle). Leave the map open **>5 s with an info panel pinned**, and again **with a vehicle marker pinned while the vehicle is moving**. Confirm: the panel survives a refresh tick, the vehicle marker follows the vehicle, and hover/selection still work afterwards. Reconciliation destroys elements mid-session, so a missed reference cleanup re-creates **BUG-135 on a timer** (`core/context.md:116`). Nothing in the automated spine can see this.
  - Estimate: 🔴 > 3 hours

- [x] ✅ **V-4 — zoom sweep (Q-5)** — Completed 2026-08-11
  - Description: Max zoom-out → max zoom-in. Each type appears at a sensible zoom; names do not overlap into illegibility; no type is visible where it is pure noise. This is the clutter gate, not a code review.
  - Estimate: 🟡 1-2 hours

- [x] ✅ **V-5 — two-client MP/JIP gate (highest risk)** — Completed 2026-08-11. **This is the row that
      mattered most**: it is the only confirmation that the N1 house-privacy fix actually isolates per
      player across clients, rather than merely reading correctly in single player where every id is yours.
  - Description: `tools/launch-server.sh` + two `tools/launch-game.sh --timeout 3600 --profile …` clients. Verify B does **not** see A's house / vehicle / private camp, **does** see A's warehouse and FOB, sees identical shop carets, and that opening the map immediately on join prints no script errors (Q-3). ⚠️ Always pass a long `--timeout` — the 600 s default kills the client mid-test.
  - Estimate: 🔴 > 3 hours

- [x] ✅ **V-6 — gamepad/console gate** — Completed 2026-08-11
  - Description: Controller only, no mouse. Every new marker type selectable; every panel readable without scrolling at 1080p; shop caret column distinguishable at panel size; fast-travel button still reachable. Note (do not fix) dismissal awkwardness — `map/core` D2/D3 own that.
  - Estimate: 🟡 1-2 hours

- [x] ✅ **V-7 — save compatibility** — Completed 2026-08-11. The §3.10 assumption held: bus stops are
      world statics and are never persisted, so a pre-Phase-1 save loads with no difference.
  - Description: Load a save created **before** Phase 1; open the map; confirm bus stops, POIs and all pre-existing markers render; save, reload, confirm the same. Expected: no difference (bus stops are world statics, never persisted — §3.10). **If any difference appears, that assumption is wrong and the plan must be revised.**
  - Estimate: 🟡 1-2 hours

- [x] ✅ **7a — F-1 parity sign-off** — Completed 2026-08-11
  - The legacy icon enumeration (`camp`, `gundealer`, `house`, `port`, `tower`, `vehicle`, `warehouse`,
    `waypoint`, POI registry, bus stops) is ticked on screen, and the parity statement was handed to
    `map/legacy-retirement` — which has since **shipped and deleted the legacy map**. That deletion,
    play-tested green, is a stronger parity sign-off than the checklist was ever going to be: the old
    icons are not merely matched, they no longer exist to compare against.

- [x] ✅ **7b — file the deferred findings as bugs (I-4)** — **DONE 2026-08-11. BUG-138 … BUG-144.**
  - Seven findings that belong to other features are now filed, **each re-verified against the working
    tree before filing** rather than copied from the write-up. Two claims changed on re-verification and
    both are recorded in the bug files: N5 turned out to have an in-session server-side half as well
    (the `garrison` prefab list is only ever populated by a save load, so the count is 0 on the host too
    until a reload), and N18's exposure was narrowed to a specific, real trigger (a listen-server host
    pressing Start after players have joined — dealers register at *campaign* start, unlike shops and
    ports which register at *world* init and are therefore complete before anyone can connect).
  - | Finding | Bug | Against | Priority |
    |---|---|---|---|
    | core D8 / N4 — per-location `visibilityZoom` never read | **BUG-138** | `map/core` | low |
    | N5 — FOB/Camp garrison never replicated | **BUG-139** | `resistance/fob` | medium |
    | N6 + addendum — three unguarded null derefs | **BUG-140** | `economy/shops` | medium |
    | N17 — sniper ammunition never stocked | **BUG-141** | `economy/shops` | medium |
    | N18 — `RegisterGunDealer` has no broadcast RPC | **BUG-142** | `economy/shops` | medium |
    | N19 — dealer prices pinned at max scarcity | **BUG-143** | `economy/shops` | medium |
    | N14 — signature weapons re-roll every load (design question) | **BUG-144** | `economy/shops` | low |
  - ⚠️ **BUG-143 carries the live consequence**: gun-dealer weapon carets ship **default-off**
    (`m_bShowWeaponCarets`) *because* of it, and the bug file now says so under its own heading, so the
    reasoning no longer lives only in this feature's `context.md` Decision 8.

---

## Bugs & Issues

**Active Bugs:**
- [x] ✅ 🐛 **N1 — House markers leak every player's property** — **FIXED Phase 3 (2026-08-10), CONFIRMED at runtime 2026-08-11.** Now uses `GetOwned(persId)`/`GetRented(persId)` and fails closed (emits nothing) when the local persistent id cannot be resolved. Also replaced a `GetNearestBuilding(pos)` position lookup with a direct `FindEntityByID`, which removes a latent wrong-building resolution. **The V-5 two-client check is done and green** — this is now an observed fix, not a code-reading one, which matters because single-player can never distinguish "filters correctly" from "every id is yours".
- [x] ✅ 🔴 🐛 ~~N1 original report~~ — **CLOSED** - Priority: High
  - Description: `OVT_MapLocationHouse.PopulateLocations` iterates `m_RealEstate.m_mOwned`/`m_mRented` for **all** players (`:25`, `:59`). Legacy drew only the local player's.
  - File: `Scripts/Game/UI/Map/LocationTypes/OVT_MapLocationHouse.c:25`
  - Impact: Violates `requirements.md:21`; a regression against the system being replaced. **Fixed in Phase 3.**

**Filed 2026-08-11 — 7b is DONE.** All seven belong to other features and were deliberately not fixed
here. Each was re-verified against the working tree at filing time; the write-ups below are kept as the
provenance record, and the bug files are now the authority:
- [x] ✅ **core D8** (N4) → **BUG-138** (`map/core`, low) — the per-location `visibilityZoom` data key is never read; the element uses only the type-level `GetVisibilityZoom()` (`OVT_MapLocationElement.c:298`, `:482`). Priority FOBs are therefore **not** always visible (K9).
- [x] ✅ **N5** → **BUG-139** (`resistance/fob`, medium) — FOB/Camp `garrison` is not replicated; `garrisonCount` reads 0 on every remote client. **Re-verification added a second half**: the three `AddGarrison*` paths insert only into `garrisonEntities`, so `garrison` is empty on the server too until a save is loaded.
- [x] ✅ **N6 + addendum** → **BUG-140** (`economy/shops`, medium) — three unguarded null derefs in one file: `GetTownStock` (`:601-611`), `GetShopByRplId` (`:435-440`, unguarded `rpl.GetEntity()`), `DistanceToNearestPort` (`:846-858`, unguarded on both).
- [x] ✅ **N17** 🐛 → **BUG-141** (`economy/shops`, medium) — `GunDealerConfig.conf:51-54` omits `m_eItemType`, which defaults to RIFLE (`2`) and duplicates `:26-29` instead of being the SNIPER_RIFLE rule the pattern requires. **Confirmed player-visible**: vanilla types SVD magazines `SNIPER_RIFLE`+`AMMUNITION` on distinct prefabs, so no rifle rule can reach them.
- [x] ✅ **N18** 🐛 → **BUG-142** (`economy/shops`, medium) — `RegisterGunDealer` has no broadcast RPC; `m_aGunDealers` reaches clients only via JIP `RplSave`. **Trigger narrowed on re-verification**: dealers register at *campaign* start (`ActivateTown` → `SpawnGunDealer`), so on a listen server every client already connected when the host presses Start gets no dealers at all — whereas shops/ports register at *world* init and are safe by timing.
- [x] ✅ **N19** 🔴🐛 → **BUG-143** (`economy/shops`, medium) — **everything a gun dealer sells is permanently priced at the maximum scarcity markup.** No town shop stocks a weapon **and** `RegisterGunDealer` never inserts into `m_mTownShops`, so `GetTownStock` is 0 for every dealer item and the term `(1 − 0/max) × 0.1` is exactly +10% forever. Not just weapons — pistols, ammunition, attachments, throwables and explosives are dealer-only too. This is why `m_bShowWeaponCarets` ships off.
- [x] ✅ **N14** ⚠️ → **BUG-144** (`economy/shops`, low, filed as a **design question**) — the four signature weapons are rolled from the unseeded global generator and nothing persists them, so they re-roll on every campaign load; only `gunDealerPosition` survives. Three options written up (leave / seed per town / persist), each with its cost.

---

## Technical Debt

- [ ] 💳 **T1 — Three manager-access idioms across ten types** - Priority: Low
  - Description: Inherited cache vs own shadowing member (`Base`/`RadioTower`) vs per-call `OVT_Global` lookup (`Shop`/`Port`/`GunDealer`).
  - Reason: Grew organically across ten types.
  - Effort: Folded in opportunistically (Phase 3) — only types this feature already touches.

- [ ] 💳 **T3 — Unregistered string data keys** - Priority: Low
  - Description: A typo silently yields the default. Duplicated by convention between House/Warehouse and FOB/Camp.
  - Effort: `OVT_MapDataKeys.c` constants introduced in Phase 3, applied only to touched types.

- [ ] 💳 **T5 — `OVT_MapLocationTown` is 307 lines** - Priority: Low
  - Description: Three times the next largest; modifier chips built inline.
  - Reason: Not addressed — `Town` is not touched by this feature (K11).

- [ ] 💳 **N10 — `OVT_MapLocationBase.GetIconNameForBase` (`:139-150`) is dead code** - Priority: Low
  - Description: No callers; Base never overrides `GetIconName`/`OnSetupIconWidget`. Delete opportunistically **only** if Base is touched, otherwise leave.

**Resolved:**
- [x] ✅ 💳 **T2 — `m_Vehicles` cached on every type, used by none** — resolved when the Vehicle type landed (Phase 3).
- [x] ✅ 💳 **T4 — Seven types have no info panel** — closed by Phase 5 (2026-08-10). Nine types now build rows; Shop and GunDealer are Phase 6.

---

## Testing Tasks

- [x] ✅ **Compile gate after every phase** — `tools/compile-check.sh`, exit 0 / 5956 files at every phase (V-1 / Q-1)
- [x] ✅ **Fast regression group after every phase** — `tools/run-tests.sh "{6A6E29FF47ECB840}"` → **43**
- [x] ✅ **All group before sign-off** — `tools/run-tests.sh "{6A6E2A002F53A581}"` → **78**, exit 0 (V-2 / Q-2)
- [x] ✅ **Manual scenario matrix** — the 14 rows in `implementation.md` §9, walked by the user and green 2026-08-11. **No map-UI autotest was added — UI is not automatable here.**

> The counts above are this feature's sign-off numbers and are **historical**. Later features have added
> cases; always re-baseline by running the script, never by quoting a doc.

---

## External / Workbench Dependencies (non-blocking — all have fallbacks)

- [x] ✅ 🎨 **Caret redraw** — **DONE by the user 2026-08-11.** New icons imported to
      `UI/Textures/Map/overthrow_priceicons_atlas.png` (and the `.edds` the engine actually loads —
      both are modified in the tree, so this was a real re-import, not a PNG drop).
  - **Solved differently from the prescription, and better for it.** The plan called for redrawing the
    chevrons **side by side** in a wide, short quad. What shipped keeps the stacked arrangement and the
    original 64×64 quads, and instead **widens the glyph to nearly fill them**. Measured from the alpha
    channel, old → new: ink width **28 → 54 px**; ink heights `up_1`/`up_2`/`up_3` **17/29/43 → 29/39/49**.
  - **Why that fixes it:** the row icon is unchanged at 13×13 (`OVT_MapInfoRow.layout` was not touched —
    the fix is entirely in the art, as intended), so the 64×64 quad scales by 0.203. The ink therefore
    renders **~11 px wide instead of ~5.7 px** — roughly double the ink area — and the three levels differ
    by ~2 px of height each across a much larger, higher-contrast glyph. **Confirmed legible on screen by
    the user**, which is the only gate that counts here.
  - No re-clipping risk: the glyph grew **wider**, and row height is driven by the text. Only a *taller*
    icon could have reintroduced the clipping fixed on 2026-08-10.
  - F-6 still holds regardless — the words "Dearer"/"Cheaper" carry direction without relying on colour.
- [x] ✅ ⚠️ ~~**`overthrow_priceicons.imageset` declares `size 1 1`** against `RefSize 200 134`~~ — **closed 2026-08-11: the carets draw.** The discrepancy is real but harmless; no atlas re-import is needed. (Kept as a pointer: if they ever stop drawing, the quad names are asserted by the Logic test, so a blank icon would be an atlas problem, not a string problem.)

- [x] ✅ 🎨 **Bus-stop icon** — **DONE 2026-08-10.** Added by the user as `bus` (`overthrow_mapicons.imageset:101`, Pos 2 392, 128×128); atlas rebuilt. The K8 `port` fallback is dead — use `bus` directly.
- [x] ⚠️ 🎨 **Caret icon set** — delivered, but **needs a redraw for magnitude to read** (see below). **DONE 2026-08-10.** `{A5EA4C81F9A25690}UI/Imagesets/overthrow_priceicons.imageset`, icons `up_1`/`up_2`/`up_3`/`down_1`/`down_2`/`down_3`, 64×64 each. **No neutral icon — correct**, §4.6 omits the row entirely inside the neutral band. Phase 6 renders images, not the `CaretText` fallback.
  - ⚠️ **Check before Phase 6 ships:** the texture block declares `size 1 1` against `RefSize 200 134`; the working `overthrow_mapicons.imageset` declares `size 784 522` matching its RefSize. May need a Workbench re-import — worth confirming the carets actually draw.
- [x] ✅ 🌐 **Localization export regeneration** — **DONE for Phase 5's 17 ids** by the user 2026-08-10 02:46, and the **Phase 6 second pass is also done**. Verified by measurement 2026-08-11, not by report: all **521** ids in `Language/localization_Overthrow.st` are present in `localization_Overthrow.en-us.conf`, none missing.
- [x] ✅ 🌐 ~~Localization export regeneration~~ (original entry) — fallback was literal English until regenerated; no longer needed. Ids added:
  - `OVT-Map_JobWaypoint` → "Job Waypoint"
  - `OVT-Map_RecruitWaypoint` → "Recruit Waypoint"
  - *(Phase 5/6 will add more — see the running list in `context.md`)*
  - Until regenerated, `OverthrowMap.conf` carries literal English via `m_sJobWaypointName`/`m_sRecruitWaypointName`; after regeneration swap those two conf values to the `#OVT-…` keys, no code change.
- [x] ✅ 🔧 **Workbench confirmation** that the new prefab/conf blocks load (hand-edited plain text with fresh GUIDs) — confirmed by the green play-test: the markers those blocks declare render in game.

---

## ✅ Verification COMPLETE — 2026-08-11

**The user play-tested the feature and reported everything green, including multiplayer.** All six
verification rows (V-3 … V-7 plus the F-1 parity sign-off) are discharged.

| Gate | State |
|---|---|
| Phases 1–6 build | ✅ 2026-08-10 — compile 0 (5956 files), Fast 43, All 78 |
| Localization exports (both passes) | ✅ Measured 2026-08-11 — 521/521 `.st` ids present in `en-us` |
| V-3 marker sweep (incl. the BUG-136 live-refresh addendum) | ✅ 2026-08-11 |
| V-4 zoom/clutter · V-6 gamepad · V-7 save compatibility | ✅ 2026-08-11 |
| **V-5 two-client MP/JIP** | ✅ 2026-08-11 — the N1 privacy fix is now *observed*, not inferred |
| F-1 parity sign-off | ✅ Superseded and strengthened: `map/legacy-retirement` shipped and deleted the legacy map |

**Both remaining items closed the same day:**

1. ~~**7b — file the seven deferred findings**~~ — **✅ DONE 2026-08-11: BUG-138 … BUG-144.** The defects
   this feature discovered in *other* features now have ids and live outside this checklist. **BUG-143
   carries the reason `m_bShowWeaponCarets` ships off**, in the bug file rather than only in a feature doc.
2. ~~The caret art redraw~~ — **✅ DONE 2026-08-11.** New icons imported; the glyph was widened from 28 px
   to 54 px of ink so it nearly fills the 13×13 row icon, and the user confirms magnitude now reads.

**Nothing is open. The feature is closed.**

---

## Task Status Legend

- [ ] Not started
- [ ] 🔄 In progress
- [ ] ⏸️ Blocked (waiting on something)
- [x] ✅ Completed
- [x] ❌ Cancelled/Won't do

---

## Notes

### Task Estimation
- 🟢 Small (< 1 hour)
- 🟡 Medium (1-3 hours)
- 🔴 Large (> 3 hours)

### Traps recorded in the plan
- ~~**Do not override `OnLocationClicked`**~~ — **lifted 2026-08-10 (BUG-137).** The container now invokes it from `OnMapSelection` when a click pins a location; overriding it works. Its default body is empty.
- **Do not call `GetTownStock`** — N6, it can null-deref on a client. Q-4 greps for it.
- **Do not do work in `CanFastTravel` or `ShouldShowLocation`** — both run per element on every zoom change (Q-7).
- **`FindAnyWidget` returning null is a silent no-op** — Q-8 name audit is mandatory.
- **Same-GUID prefab/conf overrides are DELTAS, not replacements** — vanilla components merge in unless overridden (R2).

---

*Update this file as tasks are completed. Mark tasks with ✅ immediately when done. Add new tasks as they're discovered.*
