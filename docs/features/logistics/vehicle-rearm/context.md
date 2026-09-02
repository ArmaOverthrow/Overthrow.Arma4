# logistics/vehicle-rearm - Context & Decisions

**Last Updated:** 2026-09-01 23:40
**Current Phase:** Phase 7 - Localization, help & wiki sync
**Status:** 🟡 In Progress

---

## Quick Status

**What's Done:**
- ✅ Requirements agreed with the user 2026-08-29 (R1 amended same day: location gate applies only to the money portion)
- ✅ Implementation plan (7 phases, D1–D12) written and approved
- ✅ Phase 1 (2026-09-01): OVT_VehicleRearmRules.c, ResolveAutoCapacity armed cap, HiddenFromInventory, Logic cases; StorageSeam case E inverted EARLY (Phase 1's rule change made it red); gate Fast 575/582 — the 7 reds are pre-existing on main (list in tasks.md)
- ✅ Phase 2 (2026-09-01): `OVT_PrefabUtils.IsItemHiddenInInventory` + the `StepToInventory` guard + `OVT_TEST_Init_VehicleRearmSeam.c` cases A–B. compile-check exit 0; suite run is the orchestrator's
- ✅ Phase 3 (2026-09-01): rearm action moved to `Vehicle_Base.et` (7 contexts), storage contexts widened, `OVT_StorageComponent` on `Helicopter_Base.et`, helicopter storage serializer binding, StorageSeam case K + one Persistence round-trip. compile-check exit 0; **All** suite run is the orchestrator's
- ✅ Phase 4 (2026-09-01): `OVT_StorageUtils.CollectStores`/`PlayerMayDrawFrom`, the `OVT_WarehouseStockUtils` header note, `OVT_RearmUnit`/`BuildPlan`/`CollectRearmStores`/`QuoteRearm`/`CountCovered`/`IsAtRearmSite`, the rewritten `RpcAsk_RearmVehicle`, two broadcast presets and Init cases C-E. compile-check exit 0; **All** suite run is the orchestrator's
- ✅ Phase 5 (2026-09-01): the quote RPC pair (`RequestRearmQuote`/`RpcAsk_RearmQuote`/`RpcDo_RearmQuote`/`m_OnRearmQuote`/`SendRearmQuote`), the rewritten `OVT_RearmVehicleAction` (TTL 2000, quote-driven gates, conservative pre-quote fallback) and Init case F. compile-check exit 0; **All** suite run is the orchestrator's
- ✅ Phase 6 (2026-09-01, parallel session): the civilian Mi-8 delta + price entry

**What's Next:**
- 🔄 Phase 7: localization (4 `.st` keys), help sync, review (Tests: **skipped** - docs/`.st` only)

**Blockers:**
- None

---

## Key Files

### Core Implementation
- `Scripts/Game/UserActions/OVT_RearmVehicleAction.c` - the user action (rewritten Phase 5: quote-driven)
- `Scripts/Game/Utilities/OVT_VehicleRearmUtils.c` - discovery, plan, quote, site test (Phase 4)
- `Scripts/Game/Components/Controller/OVT_ShopTransactionComponent.c` - RpcAsk_RearmVehicle rewrite + quote RPC pair
- `Scripts/Game/Data/OVT_StorageRules.c` / `OVT_VehicleRearmRules.c` (NEW) - pure rules
- `Scripts/Game/Components/Controller/OVT_StorageRequestComponent.c` - hidden-item guard in StepToInventory
- `Scripts/Game/Components/OVT_StorageComponent.c` - m_iArmedVehicleCapacity
- `Prefabs/Vehicles/Core/Vehicle_Base.et`, `Helicopter_Base.et`, NEW `Prefabs/Vehicles/Helicopters/Mi8MT/Mi8MT_unarmed_civ_base.et`
- `Configs/Systems/Persistence/Overthrow.conf` - helicopter storage serializer binding (D10)

### Related Files
- `docs/features/logistics/vehicle-rearm/requirements.md` - five requirements + diagnosis
- `docs/features/logistics/vehicle-rearm/implementation.md` - the plan (§3 detail, D1–D12)
- `Scripts/Game/GameMode/Managers/OVT_HighCommandManagerComponent.c` - RearmGroup precedent (D2/D5)
- `Scripts/Game/Utilities/OVT_WarehouseStockUtils.c` - shared PrependStore/CountAvailable/TakeUpTo (D5)

---

## Important Decisions

See implementation.md §5 (D1–D12). Highlights: pro-rata price (D1); magazine prefab from the muzzle (D2 — deviation from R2's letter, flagged to user); gate follows the money, server quote RPC pair (D3); seven-name ParentContextList union, no new context (D4); helicopter persistence binding is mandatory (D10).

---

## Gotchas & Learnings

- `tools/run-tests.sh` is the orchestrator's, once per phase, per `.claude/test-policy.md`.
- Concurrent sessions share this tree — re-baseline (`git status`) before every phase.
- **`FindEntitySource` DOES resolve prefab inheritance** (memory: `Vest_6B3.et`, an empty delta, reports its base's 12 components) — so the zero-component read is about the RESOURCE, not about the delta. §3.6's literal "zero components → return false" would have made the mandatory concrete-variant case unreliable, because `Box_25x137_M242_150rnd_HEIT.et` authors nothing of its own. The walk climbs past an unreadable level instead, and only refuses to cache. See the Phase 2 deviation note below.
- **The concrete M242 box is `{8E0429589CD8A49A}`** — resolved from `LAV25_turret_base.et:714` / `InventoryItems_EntityCatalog_US.conf:1935`, not from a `.et.meta` (the vanilla reference tree ships none).
- **Test-case letters in `OVT_TEST_Init_StorageSeam.c` are NOT free the way the plan assumed.** The file runs A–N with K missing; the plan's Phase-3 "case F" and Phase-6 "case G" both collide with cases that already exist (`FRequestComponentResolves`, `GDeployedFOBIsUnlimited`). Phase 3 took the free **K** slot; Phase 6 should take **O**.
- **A vanilla child's `additionalActions` MERGES with its parent's, by entry GUID.** Proven in the reference tree, not inferred: vanilla `Helicopter_Base.et:171-181` re-declares only three of `Vehicle_Base.et:163-177`'s six entries (as heli-specific repair deltas, same GUIDs) — and helicopters still refuel at support stations, so `{5B02B547EDCF6F99}` survived unmentioned. That is what makes moving the rearm action up to `Vehicle_Base.et` reach helicopters at all, and what makes dropping Overthrow's now-empty `ActionsManagerComponent` delta from `Helicopter_Base.et` safe.
- **Helicopters already reached Vehicle_Base.et's three storage actions before this phase.** Both families declare `door_l01` (`UH1H_base.et:1551`, `Mi8_base.et:1691`); the actions were simply invisible because there was no `OVT_StorageComponent` to show. Adding action blocks on `Helicopter_Base.et` would have produced the duplicate §4's acceptance forbids.
- **🔴 `PrependStore` CANNOT promote a store the collector already returned.** Its guard is `if (stores.Find(store) != -1) return;` — correct for High Command, whose recruitment-tent crate is never a registered warehouse, and a silent no-op for vehicle rearm, because the vehicle is always inside its own 25 m search sphere and now has capacity 100. "Own load first" would have been lost with no error anywhere. `CollectRearmStores` `RemoveOrdered`s it first, then prepends (`RemoveOrdered`, not `Remove`: position is priority for `TakeUpTo`).
- **Two plan lines can name the SAME prefab** (a turret with two identical machine guns), and `CountAvailable` answers with the whole stock every time it is asked. `CountCovered` keeps a running claim per resource so the read models the sequential `TakeUpTo` the handler will run; without it coverage over-counts and the player is undercharged.
- **A freshly spawned armed vehicle is FULL**, so its rearm plan is empty and every coverage assertion would be vacuously true. Cases C-E drain every magazine with `SetAmmoCount(0)` first (authority only) and reject any catalogue candidate that still plans nothing.
- **`OVT_ControllerComponent<T>.Get()` is null forever on a dedicated server**, so the holder radius cannot be read that way inside a server-side quote. `ResolveHolderRadius(playerId)` goes through `OVT_PlayerManagerComponent.GetController(playerId)` — the ASKING player's own controller — and falls back to 25 only when that is unreachable.
- **`SetFailure` and `PrintFormat` both cap at three `%N` string parameters.** Four compiles as "Can't find matching overload" / "Too many parameters"; concatenate into three.
- **Only ONE user-action context is "current" at a time.** `SCR_InteractionHandlerComponent.c:353` picks a context and only lets it change once the player is outside `UserActionContext.GetVisibilityRange()`, so `CanBeShownScript`/`CanBePerformedScript` - and therefore `RefreshCache` and its quote ask - run for at most one re-arm action instance per player, however many armed vehicles are parked together. That is what makes §3.10's "one small reliable RPC pair per cache window" true rather than per-vehicle.
- **`RplId` is a `sealed class` but compares BY VALUE.** `==`/`!=` are the shipped idiom (`OVT_GMWaypointRenderer.c:244,327`, `OVT_RecruitManagerComponent.c:3314`) - the quote's "is this about my vehicle" test is a plain `!=`, not a pointer comparison.
- **A `ScriptInvoker` may carry an `RplId`.** `Invoke(void param1, ...)` is untyped (`ArmaReforger/scripts/GameLib/tools.c:119`) and vanilla already sends one (`SCR_SpawnPoint.c:663`). Untyped like `Rpc()`, so a listener with a different parameter list fails at run time only - `m_OnRearmQuote`'s 4-argument shape is in the arity table below.
- **A `ScriptedUserAction` CAN have a destructor**, and vanilla uses one for exactly this - dropping invoker subscriptions (`SCR_FactionCommanderVolunteerUserAction.c:219`). The re-arm action's subscription lives on the local player's controller, not on the vehicle, so without `~OVT_RearmVehicleAction()` a deleted vehicle would leave a handler firing into freed memory.
- **19 vanilla `.et` files author `m_bVisible`**, 18 of them `0`. Beyond the two M242 boxes the guard now also traps `Ammo_Rocket_Hydra70`, `Ammo_Rocket_S5`, `Ammo_Flare_40mm_StarParachute_Base`, `UGL_M203_base`, `Mortar_Base`/`Mortar_M252` and `RocketPod_Base` in whatever ledger holds them. The rest are vehicle parts and virtual arsenal slots that never become ledger lines. `ToolRack_01_tool_02.et` is the only one that authors `1`.

---

## Testing Approach

Per phase, see implementation.md §4/§7. Logic tier for pure rules; Init tier `OVT_TEST_Init_VehicleRearmSeam.c` (new); StorageSeam case E inverted in Phase 3; one Persistence round-trip (helicopter ledger). Fast group Phases 1–2; All for Phases 3–6; skipped Phase 7 (docs/.st only).

---

## Needs human verification

- **MP, both branches of the quote reply.** Case F proves only the listen-host/SP short circuit (`ShouldRespondLocally` -> direct call). The wire branch (`Rpc(RpcDo_RearmQuote, ...)` to a remote owner) has no automated coverage at all - DoD play-test C-27/C-29 are the only proof it works.
- **Two vehicles parked together** (DoD F1/§3.5): the quote's `RplId` filter and "a mismatch must not unsubscribe" are reasoned, not tested.
- **The label flip within one cache window** after a nearby truck's stock changes (DoD C-27, 2 s).
- (plus the play-tests owed at the end)

---

## Session Notes

### 2026-09-01 14:19
- Feature started via /autorun-feature. Docs scaffolded; Phase 1 next.

### 2026-09-01 17:10 — Phase 2

- Landed: `OVT_PrefabUtils.IsItemHiddenInInventory` (+ `ReadAuthoredVisibility`, `ReadItemVisibility`, `MAX_ANCESTRY_DEPTH`, `s_mHiddenInInventory`), the `StepToInventory` early-continue, and `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_VehicleRearmSeam.c` (cases A–B + `OVT_TEST_VehicleRearmSeamSubject`).
- **Deviation from §3.6, deliberate:** a zero-component read does not return immediately. It marks the walk *unreadable* and climbs anyway; the answer is cached only when the walk reached a conclusion (found the flag, or ran a fully-readable chain to its end). The plan's literal early-return would have answered "visible" for the very prefab the feature exists for, because the concrete M242 variant authors nothing of its own and would read zero components whenever its resource is not resident. Fail-open and never-cache-a-bad-read are both preserved.
- Acceptance grep: `grep -n TrySpawnPrefabToStorage Scripts/Game/Components/Controller/OVT_StorageRequestComponent.c` → one call site (`:1915`) plus one comment (`:2687`); the guard sits at `:1884`, before `ResolveHolderStorage` at `:1908`.
- compile-check exit 0 (6357 files).

### 2026-09-01 15:05
- Phase 1 landed (5 files). compile-check OK. Fast gate: first run 8 reds → case E was the plan's known inversion, done early in main thread; re-run 7 reds, ALL pre-existing on main (untouched subsystems, identical set both runs). `run-tests.sh` takes the group GUID, not the word "Fast" ({6A6E29FF47ECB840}).
- Working tree also carries a concurrent session's uncommitted `OVT_MapContext.c` change (map-gadget fix) — not ours, left alone.

### 2026-09-01 (post-Phase 2 gate)
- Fast gate 582/583: cases A/B green. Concurrent session fixed 6 of the 7 pre-existing reds and deleted OVT_TEST_Init_MedicalTent.c; only ObjectiveOperations_ARampConfigsResolveAndAreOrdered remains red (pre-existing, untouched subsystem).
- Phase 2 note RESOLVED 2026-09-01 (user asked; investigated): mortar/M203 ROUNDS were never trapped. The hidden prefabs are the deployed mortar TUBE (Mortar_Base/M252) and the M203 launcher ATTACHMENT (UGL_M203_base) — not their ammunition. 81/82 mm shells (SCR_VisibleInventoryItemComponent, MORTARS/AMMUNITION, priced $500 by itemPrices.conf {65CCF4E070A007A8}) and 40 mm grenades (M406/M433, AMMUNITION mode) are visible, catalogued, sold by the gun dealer's AMMUNITION row (GunDealerConfig.conf) and port-importable (GetAllNonOccupyingFactionItems includes AMMUNITION entries). Deployed mortars have no ammo store to re-arm — the crew hand-loads shells per shot (SCR_MortarMuzzleComponent, ReloadDuration 2), so no re-arm action is needed or possible there. Looted M203 rifles keep their launcher: the slot DECLARES the prefab, so StripWeapon's IsDeclaredPart guard skips it. Residual (accepted): a manually-detached spare M203 or a loose Hydra/S5 rocket converted into a ledger stays there — rockets are exactly what heli re-arm consumes (by design); a spare M203 line is a rare dead line.

### 2026-09-01 19:40 — Phase 3

**Landed (6 files):**
- `Prefabs/Vehicles/Core/Vehicle_Base.et` — `OVT_RearmVehicleAction` moved in at `:78` with GUIDs `{6A9F4C2DB1E07A31}`/`{6A9F4C2DB1E07A32}` verbatim, `Duration 5`, `CanAggregate 1`, `VisibilityRange -1`, `"Sort Priority" 105`, and the seven-name union `"heli_repair_point" "door_r01" "door_l01" "door_rear" "door_back_left" "door_back_right" "hatch_commander"`. `OVT_OpenStorageMenuAction`/`OVT_TransferAllToStorageAction`/`OVT_RenameStorageAction` each gained exactly `"door_back_left" "door_back_right" "hatch_commander"`; `OVT_SellVehicleCargoAction` deliberately untouched.
- `Prefabs/Vehicles/Core/Helicopter_Base.et` — rearm block removed; `OVT_StorageComponent "{6BA1C4E000000001}"` AUTO / 300 / 100 added in the `Wheeled_Base.et` slot order.
- `Configs/Systems/Persistence/Overthrow.conf:152` — `OVT_StorageComponentSerializer "{6B0E7A63F4051627}"` on `EntityPersistenceConfig {64EE8D74EB8192BA}` (D10).
- `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_StorageSeam.c` — `OVT_TEST_StorageSeamSubject.FindArmedHelicopter()` + case **K** `…_KArmedHelicopterHasSmallStorage`; file header's case-E line corrected (it still stated the pre-inversion rule).
- `Scripts/Game/Tests/TestSuites/Persistence/OVT_TEST_PersistenceRoundTripSuite.c` — `OVT_TEST_StorageRoundTripFixture.ResolveHelicopterPrefab()` + `…_StorageHelicopter_LedgerSurvivesSave`.
- `Scripts/Game/UserActions/OVT_RearmVehicleAction.c` — **comment only**, its header cited `Helicopter_Base.et` as the authoring file.

**Minted GUIDs (0 hits in BOTH trees before authoring):** `{6BA1C4E000000001}` (heli storage component; note `{6BA1C4E0FFFFFFFF}` is already in use as Phase 2's deliberately-missing prefab), `{6B0E7A63F4051627}` (persistence binding, taken from the plan).

**Deviations:**
1. **Case letter K, not F.** F and G are taken in `OVT_TEST_Init_StorageSeam.c`; K was the free slot. Phase 6's "case G" needs the same treatment (**O**).
2. **No storage action blocks on `Helicopter_Base.et`.** §3.8 step 4 asks only for the component; both heli families already reach `Vehicle_Base.et`'s three storage actions through `door_l01`. Adding blocks would have been the forbidden duplicate.
3. **The emptied `ActionsManagerComponent` block was dropped whole** rather than left as a bare re-declaration. §3.8 calls both inert, but a delta authoring `additionalActions { }` is the one shape that could plausibly clear the inherited array; no block at all cannot.
4. **One script file touched outside the phase scope** — `OVT_RearmVehicleAction.c`'s class header, comment only, because it named the wrong prefab as of this phase. Phase 5 rewrites the body.

**Acceptance evidence:**
- `grep -rln OVT_RearmVehicleAction Prefabs/` → 1 file, `Prefabs/Vehicles/Core/Vehicle_Base.et`.
- `{C97BE5489221AE18}` still the only actions-manager GUID in the tree (5 Overthrow files, all re-declarations).
- Brace counts balanced in every edited file; no CR bytes; no trailing-newline change.
- `tools/compile-check.sh` → **OK (6357 files), exit 0**.

**Owed / unproven:**
- The **All** group has not run — case K and `…_StorageHelicopter_…` have never executed.
- **Between Phase 3 and Phase 5 the action is reachable but still gated on a helipad**: `CanBePerformedScript` and the server handler both still demand `IsOnHelipadAtFriendlyBase`, so an LAV at a garage sees "Re-arm" blocked with `#OVT-MustBeOnHelipad`. Expected; Phases 4–5 close it.
- `RefreshCache()` (1 Hz slot walk) now runs on every vehicle a player stands next to rather than only helicopters. Bounded and null-safe (`GetRearmableWeapons` allocates its own out-arrays and returns early with no `SlotManagerComponent`), but it is new per-frame-adjacent work on the client — worth a glance during play-test.

### 2026-09-01 (post-Phase 3 gate)
- All gate 662/665. Our five cases GREEN (StorageSeam E+K, VehicleRearmSeam A+B, PersistenceRoundTrip StorageHelicopter). Reds, none ours: ObjectiveOperations_ARampConfigs (known pre-existing); Campaign_FieldRepairEconomy ("RepairKit_01_wrench not eligible at SHOP_GUNDEALER") and Campaign_GMGroupRegistry (0 virtual groups tagged in 55 s) — both from resistance/field-repair whose Campaign cases NEVER EXECUTED before (suites were blocked at its build); first run exposes main defects.

### 2026-09-01 22:05 — Phase 4

**Landed (7 files):**
- `Scripts/Game/Utilities/OVT_StorageUtils.c` — `CollectStores(vector, float, int playerId, out array<OVT_StorageComponent>)` and `PlayerMayDrawFrom(int, IEntity)`. One `new OVT_StorageHolderQuery()` per call, nothing static. `PlayerMayDrawFrom` is `PlayerMayUseVehicleFor` -> `PlayerMayUseWarehouse` -> `OVT_StructureDamage.IsUsable`, in `MayUseHolder`'s order (`:3055`, `:3061`, `:3067`), with no distance clause (D6). Also corrected `OVT_StorageHolderQuery.FilterHolders`' comment, which still said capacity 0 meant "an illegal or armed vehicle".
- `Scripts/Game/Utilities/OVT_WarehouseStockUtils.c` — **comment only** (6 insertions, 0 deletions): the three list ops now have two callers (D5).
- `Scripts/Game/Utilities/OVT_VehicleRearmUtils.c` — new `OVT_RearmUnit`; `BuildPlan`, `CollectRearmStores`, `QuoteRearm`, `CountCovered`, `CountReloadableBarrels`, `ResolveHolderRadius`; `IsOnHelipadAtFriendlyBase` -> `IsAtRearmSite` (FOB clause first, then the structure query); `HELIPAD_SEARCH_RADIUS` 15 -> `SITE_SEARCH_RADIUS` 20, `+ GARAGE_BUILDABLE_TYPE`, `+ FOB_SEARCH_RADIUS`, `+ FALLBACK_HOLDER_RADIUS`; `GetRearmableWeapons` gained an index-aligned `out array<BaseMuzzleComponent> muzzles` (4 params). `PerformRearm` unchanged in behaviour.
- `Scripts/Game/Components/Controller/OVT_ShopTransactionComponent.c` — `RpcAsk_RearmVehicle` rewritten; class-header note refreshed (no longer "stop-gap helicopter").
- `Scripts/Game/UserActions/OVT_RearmVehicleAction.c` — **two lines, compile fix only** (the 4-param `GetRearmableWeapons` and the renamed site test). Phase 5 rewrites the body.
- `Configs/overthrowBroadcastMessages.conf` — `"Rearmed"` `{6BA1C4E000000010}`/`{6BA1C4E000000011}` and `"RearmNeedsSupplyPoint"` `{6BA1C4E000000012}`/`{6BA1C4E000000013}` (0 hits in BOTH trees before authoring).
- `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_VehicleRearmSeam.c` — cases C/D/E, `OVT_TEST_VehicleRearmVehicleFixture`, and three subject helpers.

**§3.5 as implemented** — same order, steps 7-10 collapsed into `QuoteRearm`:

| Step | Where |
|---|---|
| 1-5 server / playerId / character / vehicle / 15 m | unchanged from the shipped handler |
| 6 `NeedsRearm` | before the plan, as specified |
| 7-10 plan, stores, covered, cost | **inside `QuoteRearm(vehicle, playerId, total, covered, cost)`** |
| 11 site then funds, only when `cost > 0` | `:534` / `:540` |
| 12 `TakeUpTo` per line | `:562`, skipped entirely when `covered == 0` |
| 13 `PerformRearm` | `:566` |
| 14 `TakePlayerMoney(cost)` | `:569` — the quoted figure, no recompute |
| 15 toast | `:571`, `taken` and `cost` as separate parameters |

**Deviations:**
1. **Steps 7-10 live in `QuoteRearm`, and the handler re-derives the plan and the store list for step 12.** The acceptance requires both the quote RPC and the handler to *call `QuoteRearm`*, and EnforceScript has no optional `out` parameters, so the plan cannot ride back out of the required signature. Cost: one extra `BuildPlan` and one extra 25 m sphere query per rearm press, both in the same frame with identical inputs. Benefit: the charged figure is literally the quoted figure, and "QuoteRearm mutates nothing" is a one-grep proof.
2. **`CollectRearmStores` removes the vehicle's own store before prepending it** — see the Gotchas entry; `PrependStore` alone was a no-op here.
3. **`OVT_RearmVehicleAction.c` touched (2 lines)** for the widened `GetRearmableWeapons` signature and the `IsAtRearmSite` rename. Behaviour is unchanged and Phase 5 rewrites the file.
4. **`IsArmed()` is now unused in production** (the action inlines the emptiness test). Left in place; Phase 5's rewrite may adopt it.
5. **Test-case letters C, D, E were free in this file** — unlike `OVT_TEST_Init_StorageSeam.c`, no collision.

**Acceptance evidence:**
- `QuoteRearm`'s body and its whole transitive closure (`BuildPlan`, `CollectRearmStores`, `CountCovered`, `CountAvailable`, `PrependStore`, `ProratedCost`, `ResolveHolderRadius`) contain no `Take`, no `SetAmmoCount`, no `BumpMe`, no `Spawn`, no `Reload`. The only `Clear()` is on the caller's own out-array.
- Site (`:534`) and funds (`:540`) both precede the first `TakeUpTo` (`:562`). No rollback path exists because none is reachable.
- RPC arity audit on `OVT_ShopTransactionComponent`: `RpcAsk_SellItems` 3/3, `RpcAsk_SellVehicleCargo` 2/2, `RpcAsk_BuyItems` 3/3, `RpcAsk_RearmVehicle` 1/1, `RpcDo_SellResult` 4/4. No `array<...>` on any handler. The rearm ask keeps the `if(Replication.IsServer()) Handler(...) else Rpc(Handler, ...)` branch (BUG-164).
- `git diff --stat Scripts/Game/Utilities/OVT_WarehouseStockUtils.c` = 6 insertions, 0 deletions, all comment lines (I3). No `core/damage` file modified (I2). `OVT_TransferContext.c` clean (I1).
- Brace counts balanced in every edited file; no CR bytes introduced.
- `tools/compile-check.sh` → **OK (6357 files), exit 0**.

**Owed / unproven:**
- The **All** group has not run — cases C, D and E have never executed.
- `#OVT-Msg-Rearmed` / `#OVT-Msg-RearmNeedsSupplyPoint` do not exist until Phase 7, so both toasts currently render the raw key.
- **Between Phase 4 and Phase 5 the CLIENT gate still demands a site**: `OVT_RearmVehicleAction.CanBePerformedScript` blocks with `#OVT-MustBeOnHelipad` whenever `IsAtRearmSite` is false, even for a rearm the server would now perform for free. The server is correct; the button is not.
- **`BuildPlan`'s loaded-magazine fallback is unexercised.** `ResolveAmmoPrefab(muzzleDefault, GetPrefabName(magazine.GetOwner()), "")` only reaches the second argument when the muzzle answers empty, and if a magazine component turns out to sit on the WEAPON entity rather than on a magazine entity, that fallback names the weapon prefab. Harmless (it can never match a ledger line, so the unit lands in the money remainder) but wrong-looking if it ever shows up in a log.
- MP untested; no case exercises `CollectStores` drawing from a SECOND holder (cases D/E use the vehicle's own ledger, which `CollectRearmStores` prepends). The nearby-truck path is play-test only (DoD F3).

### 2026-09-01 (post-Phase 4 gate)
- All gate 665/668: cases C/D/E green on first execution. Reds unchanged: ObjectiveOperations_ARampConfigs + the two field-repair Campaign cases (all pre-existing main defects, logged post-Phase 3).

### 2026-09-01 (Phase 6, parallel with Phase 5)
- Mi-8 civ delta landed: same-GUID {366EA0B41474A7F8}, re-declares storage GUID {6BA1C4E000000001} with UNLIMITED; vehiclePrices entry {6BA1C4E000000020} cost 90000 legal PARKING_HELI. Pricing match is LONGEST m_sFind wins (OVT_EconomyManagerComponent.c:1775-1795), order irrelevant, ties fall to later entry. StorageSeam case is O (G taken). Gate deferred until Phase 5 lands.

### 2026-09-01 23:40 — Phase 5

**Landed (3 files):**
- `Scripts/Game/Components/Controller/OVT_ShopTransactionComponent.c` — `m_OnRearmQuote`, `RequestRearmQuote(IEntity)`, `RpcAsk_RearmQuote(RplId)`, `RpcDo_RearmQuote(RplId,int,int,int)`, `SendRearmQuote(...)` beside `SendSellResult`, and a class-header note on why the quote rides this component.
- `Scripts/Game/UserActions/OVT_RearmVehicleAction.c` — rewritten. `CHECK_TTL_MS` 1000 → 2000; `RefreshCache` recomputes armed / needs-rearm / at-site locally and asks for a quote only when armed **and** something is missing; `QuotedCost()`; `OnRearmQuote` stores `(total, covered, cost)` only on an `RplId` match; `#OVT-Rearm_NeedsSupplyPoint` and `#OVT-RearmVehicle_FromStorage`; `SubscribeQuote`/`UnsubscribeQuote`/`ForgetQuote`/`GetOwnerRplId` and a destructor.
- `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_VehicleRearmSeam.c` — case **F** `…_FQuoteRpcPairAnswersTheAsker` + `OVT_TEST_VehicleRearmSeamSubject.ResolveSpawnBeside`; file header's case list updated.

**RPC ARITY TABLE (DoD Q3/Q4, §3.5).** `Rpc()` is an untyped variadic prototype: a wrong argument count compiles clean and dies silently at the wire (BUG-090), so every row below was read off the file rather than inferred.

| RPC | Receiver | Call site — direct (authority) | Call site — wire | Handler signature | Arity |
|---|---|---|---|---|---|
| `RpcAsk_RearmVehicle` | `RplRcver.Server` | `RpcAsk_RearmVehicle(vehicleRpl.Id())` `:495` — **1** | `Rpc(RpcAsk_RearmVehicle, vehicleRpl.Id())` `:497` — **1** | `protected void RpcAsk_RearmVehicle(RplId vehicleId)` `:513` — **1** | ✅ 1/1/1 |
| `RpcAsk_RearmQuote` | `RplRcver.Server` | `RpcAsk_RearmQuote(vehicleRpl.Id())` `:597` — **1** | `Rpc(RpcAsk_RearmQuote, vehicleRpl.Id())` `:599` — **1** | `protected void RpcAsk_RearmQuote(RplId vehicleId)` `:612` — **1** | ✅ 1/1/1 |
| `RpcDo_RearmQuote` | `RplRcver.Owner` | `RpcDo_RearmQuote(vehicleId, totalUnits, coveredUnits, cost)` `:1200` — **4** | `Rpc(RpcDo_RearmQuote, vehicleId, totalUnits, coveredUnits, cost)` `:1204` — **4** | `protected void RpcDo_RearmQuote(RplId vehicleId, int totalUnits, int coveredUnits, int cost)` `:644` — **4** | ✅ 4/4/4 |

One more untyped variadic in the same chain, audited for the same reason:

| Invoker | Invoke site | Listeners |
|---|---|---|
| `m_OnRearmQuote` | `m_OnRearmQuote.Invoke(vehicleId, totalUnits, coveredUnits, cost)` `:647` — **4** | `OVT_RearmVehicleAction.OnRearmQuote(RplId, int, int, int)` — **4**; `OVT_TEST_Init_VehicleRearmSeam_FQuoteRpcPairAnswersTheAsker.OnRearmQuote(RplId, int, int, int)` — **4** ✅ |

- **No `array<...>` on any RPC** in the file: `grep -n -A 1 RplRpc … | grep -c "array<"` → **0** (all seven handlers).
- **No `Rpc()` call is wrapped in an argument-hiding helper.** `SendRearmQuote` is the sanctioned `SendSellResult` shape (`:1175-1185`): a two-branch *delivery* helper whose four arguments are written out literally on **both** branches. Nothing builds an argument list, forwards a vararg, or packs a struct.
- `RpcDo_RearmQuote`'s body is two lines and contains no `Take`, `Set`, `Bump`, `Spawn`, `Reload` or `Delete` — `sed -n '644,649p' … | grep -cE "Take|Set|Bump|Spawn|Reload|Delete"` → **0**. Losing this packet costs a label, never a transaction.

**THE PRE-QUOTE FALLBACK IS STRICTLY CONSERVATIVE.** One number drives every gate: `QuotedCost()` answers `m_iQuotedCost` with a quote in hand and `OVT_VehicleRearmUtils.GetRearmCost()` — the full price, i.e. "nothing is covered" — without one.

1. `CanBePerformedScript` is monotone non-decreasing in that number: `cost <= 0` → allowed outright; `cost > 0` → requires `m_bCachedAtSite` **and** `LocalPlayerHasMoney(cost)`, and affording a larger sum implies affording a smaller one.
2. Every quote the server can send satisfies `m_iQuotedCost <= GetRearmCost()`: `ProratedCost` returns `0`, `fullCost`, or `Math.Round(fullCost * u / t)` with `u < t`, floored at 1 — all `<= fullCost` for every `fullCost >= 1`.
3. Therefore the no-quote state (site required, full price required) is a superset of the restrictions of any quote that could arrive. A missing, late, lost or spoofed quote can only under-offer, never over-offer, and the server re-derives all of it anyway.
4. ⚠ **One degenerate exception, recorded not fixed:** at `vehiclePriceMultiplier == 0`, `GetRearmCost()` is 0 (pre-quote → free and go-anywhere) while `ProratedCost`'s min-1 clamp quotes **$1** for a partially covered re-arm — so the pre-quote state would be $1 more permissive. The clamp exists so a rounding-down never gives ammunition away free; with a full cost of 0 it fabricates a price instead. Phase 1's Logic-tested rule, no shipped difficulty config sets that multiplier to 0, and the whole feature is free at that setting anyway.

**Deviations:**
1. **`RpcAsk_RearmQuote` is distance-gated** at `VEHICLE_MAX_DISTANCE` (15 m from the asking player's character), which §3.5 does not specify. The handler is a server read triggered by any client naming any `RplId` — it walks turret slots, runs a 25 m sphere query and counts ledgers — so leaving it unbounded is a free spam surface and a coverage-probe for arbitrary vehicles. The gate is the same constant the re-arm itself uses, so the invariant is "a quote is only reachable for a vehicle its asker could actually re-arm", and it can never make the button lie in the permissive direction: no quote → full price (see above).
2. **Case F arranges BESIDE the caller**, not in cases C–E's empty quarter (`ResolveSpawnBeside`, 8 m, surface-snapped), because of deviation 1. It asserts nothing about coverage, so a neighbour's ledger cannot contaminate it.
3. **The quote listener unsubscribes only on a MATCH.** Two armed vehicles parked together is the ordinary case and both action instances hold a subscription to the one controller-side invoker; unsubscribing on the neighbour's quote would leave one vehicle permanently un-quoted. A subscription therefore outlives an unanswered ask until the next window — which is why the destructor exists.
4. **A destructor was added** (`~OVT_RearmVehicleAction`), following `SCR_FactionCommanderVolunteerUserAction.c:219`. Not in the plan; the invoker lives on the controller, so a deleted vehicle's action would otherwise leave a handler in it.
5. **`m_iQuotedTotal` / `m_iQuotedCovered` are stored and not read** by any gate or label today. §4 asks the listener to store all three; they are what a future "covers 3 of 4" label would need.

**Acceptance evidence:**
- `grep -rn "MustBeOnHelipad" Scripts/` → **0 hits** (DoD verification step 3). The `.st` entry stays per D11 (`localization_Overthrow.st:13676`) — **tidy-up candidate**.
- Brace counts balanced in all three files (20/20, 135/135 in the seam file); no CR bytes; trailing newline unchanged.
- `SetFailure`/`PrintFormat` string parameters: max **3** in every new call (the cap that compiles as "Too many parameters").
- `tools/compile-check.sh` → **OK (6357 files), exit 0**.

**Owed / unproven:**
- The **All** group has not run — case F has never executed.
- `#OVT-Rearm_NeedsSupplyPoint` and `#OVT-RearmVehicle_FromStorage` do not exist until Phase 7, so the blocked reason and the covered label currently render the raw key.
- **MP untested.** Only the listen-host/SP branch is covered; the `Rpc(RpcDo_RearmQuote, ...)` wire branch has no automated proof.
- `Scripts/Game/UserActions/OVT_FillFuelAction.c:50` still says its cache window is "the same one second as OVT_RearmVehicleAction" — stale now that the re-arm window is 2 s. Comment-only drift in another feature's file, deliberately not touched in this phase; **tidy-up candidate**.

### 2026-09-01 (post-Phase 5+6 gate)
- All gate 667/670: cases F (quote RPC pair) and O (civ Mi-8 unlimited) green on first execution. Reds unchanged: the 3 pre-existing main defects (ObjectiveOps ramp, field-repair Campaign x2).

### 2026-09-01 (cross-phase review + help sync)
- Review verdict: ship-ready. Fixed in main thread: (1) CollectRearmStores now prepends the vehicle's own store only when PlayerMayDrawFrom passes (locked-vehicle seam); (2) IsItemHiddenInInventory no longer caches a verdict derived past an unreadable ancestry level (out cacheable); (3) toast wording magazine(s)→item(s). INFO items accepted: stale-low label window (D3 trade), vehiclePriceMultiplier==0 degenerate, F3 second-holder + dedicated wire = play-test only.
- help-docs-sync: Field Manual "Re-arming Vehicles" ({6BA1C4E000000050}-57), corrected FieldManual_Storage_Text2/Storage_Text3/Ports_Text + Tutorial_StorageFirstOpen_Body (stale claims). 8 new + 4 changed .st keys, en_us only — WORKBENCH RE-EXPORT OWED; 4 corrected bodies have stale ru/de translations. Wiki BLOCKED (no wikijs MCP this session) → wiki-pending.md.
