# Fuel - Task Checklist

**Last Updated:** 2026-08-18 (feature CLOSED — play-test green, committed a765bd12)
**Progress:** 35/36 tasks complete (97%)

> Agent routing: phases 2 and 3 are **ADVANCED** (`component-developer-advanced`); phases 1 and 4 are STANDARD (`component-developer`); phase 5 is `help-docs-sync`. Suite per phase: 1–3 → **All** `{6A6E2A002F53A581}` (config-stream / economy / persistence state), 4 → **Fast** `{6A6E29FF47ECB840}`, 5 → skipped (docs-only). Source of truth: `implementation.md` phase tables.

---

## Phase 1: Fee math, difficulty knob, marker component (8/8 complete) — STANDARD

- [x] **T1.1 `OVT_FuelPricing` pure helpers**
  - Description: `ResolvePrice(float)` (clamp <0 → 0), `ComputeCost(litres, price)`, `EstimateTickCost(litresThisTick, price)` (ceil, floor of $1, zero price → 0), `FormatPricePerLitre(price)` via `SCR_FormatHelper.FloatToStringNoZeroDecimalEndings(price, 2)`.
  - File(s): `Scripts/Game/Data/OVT_FuelPricing.c` (new)
  - Estimate: 1 h

- [x] **T1.2 `OVT_FuelChargeLedger` fractional accumulator**
  - Description: `Accrue(key, litres, price) → int`, `Clear(key)`, `GetPending(key)`, `GetTrackedCount()`; `ref map<string, float>` internally; code comment stating the never-settled/never-persisted design (D8).
  - File(s): `Scripts/Game/Data/OVT_FuelChargeLedger.c` (new)
  - Estimate: 1 h

- [x] **T1.3 `fuelPricePerLitre` difficulty field**
  - Description: `[Attribute(defvalue: "1.0", desc: "Fuel price per litre at static fuel stations (0 disables fuel charging)", category: "Economy")] float fuelPricePerLitre` after `vehiclePriceMultiplier` in the Economy block.
  - File(s): `Scripts/Game/Config/OVT_DifficultySettings.c` (path per codebase)
  - Estimate: 0.5 h

- [x] **T1.4 Difficulty preset overrides**
  - Description: Easy `0.5`, Hard `1.5`, Extreme `2.5`, Insane `4`, TestWorld `1`; Normal inherits the 1.0 default.
  - File(s): `Configs/Difficulty/*.conf`
  - Estimate: 0.5 h

- [x] **T1.5 Config replication: stream version 3 → 4**
  - Description: Append `fuelPricePerLitre` to `OVT_OverthrowConfigComponent.RplSave` (after `allowFOBDuringQRF`) AND `RplLoad`, same order; bump `CONFIG_STREAM_VERSION` to 4 with the file's "Version 4 appended …" doc paragraph.
  - File(s): `Scripts/Game/Components/OVT_OverthrowConfigComponent.c` (path per codebase)
  - Estimate: 1 h

- [x] **T1.6 Struct mirror (optional, 3 lines)**
  - Description: Mirror into `OVT_OverthrowConfigStruct` + `SetDefaults()` + the `overrideDifficulty` block in `OVT_OverthrowGameMode.c:411-419`.
  - File(s): struct + game mode files
  - Estimate: 0.5 h

- [x] **T1.7 `OVT_FuelSourceComponent` marker**
  - Description: Marker component, single `[Attribute("1")] bool m_bFree`, `IsFree()` accessor, Doxygen header explaining who reads it (OVT_FuelUtils + high-command discovery).
  - File(s): `Scripts/Game/Components/OVT_FuelSourceComponent.c` (new)
  - Estimate: 0.5 h

- [x] **T1.8 Logic-tier test cases (×4), each proven red once**
  - Description: `CostMath`, `ChargeLedger`, `TickEstimate`, `PriceFormat` per implementation.md Testing Strategy; `[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]`; record the red-proof edits.
  - File(s): `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_Fuel.c` (new)
  - Estimate: 2 h

---

## Phase 2: The paid-refuel charge path (8/8 complete) — ⚠️ ADVANCED

- [x] **T2.1 Modded reason enum**
  - Description: `modded enum ESupportStationReasonInvalid { OVT_CANNOT_AFFORD_FUEL = 550 }`.
  - File(s): modded enum file under `Scripts/Game/**/Modded/`
  - Estimate: 0.25 h

- [x] **T2.2 `OVT_FuelUtils` core half**
  - Description: `GetFuelPricePerLitre()`, `IsFreeFuelSource(station)` (marker ∨ vehicle ∨ no-range), `GetFuelCostPerLitre(station)`, `ResolvePersistentId(actionUser)`; range-query half deferred to Phase 4.
  - File(s): `Scripts/Game/Utilities/OVT_FuelUtils.c` (new)
  - Estimate: 1.5 h

- [x] **T2.3 Modded `SCR_FuelSupportStationComponent` — three hooks**
  - Description: `IsValid` affordability gate (after `super`), `OnExecutedServer` user stash + clear, `OnFuelAddedToVehicleServer` charge-before-`super` (null-node/backup path safe). Members: `m_PendingChargeUser`, lazy `ref OVT_FuelChargeLedger m_Ledger`, `m_bLastChargeClamped`.
  - File(s): `Scripts/Game/Components/SupportStation/Modded/SCR_FuelSupportStationComponent.c` (new)
  - Estimate: 3 h

- [x] **T2.4 Charge routine with clamp**
  - Description: `owed = Accrue(...)`; balance check via `GetPlayerMoney` BEFORE any take; clamp to balance → `Clear(key)` + set `m_bLastChargeClamped`; `TakePlayerMoneyPersistentId` only for `owed > 0`.
  - File(s): same modded component
  - Estimate: 1 h

- [x] **T2.5 One-shot "ran out" notification**
  - Description: After `super.OnExecutedServer`, if clamped → `OVT_Global.GetNotify().SendTextNotification(...)` once, clear flag; next tick's IsValid refuses so it cannot repeat.
  - File(s): same modded component
  - Estimate: 0.5 h

- [x] **T2.6 Modded `SCR_RefuelAtSupportStationAction`**
  - Description: `GetActionNameScript` — call `super`, append price suffix only when station charges and action active (keep the fill %); `GetInvalidPerformReasonString` — map `OVT_CANNOT_AFFORD_FUEL` → `#OVT-Refuel_CannotAfford`, else `super`. Do NOT touch `GetActionStringParam`.
  - File(s): `Scripts/Game/UserActions/Modded/SCR_RefuelAtSupportStationAction.c` (new)
  - Estimate: 1.5 h

- [x] **T2.7 Localization keys**
  - Description: `OVT-Refuel_PriceFormat` (`"%1 ($%2/L)"`), `OVT-Refuel_CannotAfford` in the string table, exact `CustomStringTableItem` shape.
  - File(s): `Language/localization_Overthrow.st`
  - Estimate: 0.5 h

- [x] **T2.8 Degradation guards**
  - Description: No economy manager / no config / `price <= 0` / unresolvable player ⇒ never gate, never charge, pure vanilla. No `Rpc(` anywhere in the new files.
  - File(s): all Phase 2 files
  - Estimate: 1 h

---

## Phase 3: Fuel Depot buildable + persistence (6/6 complete) — ⚠️ ADVANCED

- [x] **T3.1 `OVT_FuelDepot.et` prefab (+ .et.meta)**
  - Description: Derive `{B6370564C0BBAD45}` MobileWaterTank_FIA_01_fuel.et; components per the anatomy in implementation.md (`SCR_FuelNode` MaxFuel 5000 - **raised to 10000 by amendment A2.3**, `m_fInitialFuelTankState 0` **litres**, `m_eFuelNodeType 11`, station `m_fRange 7` + `m_bIgnoreSelf 1`, own `UserActionContext "default"` + refuel action with `ParentContextList { "default" }`, SoundComponent). Fresh 16-hex GUIDs.
  - File(s): `Prefabs/Structures/Military/FOB/OVT_FuelDepot.et` (new)
  - **MODEL SWAPPED ON USER FEEDBACK (2026-08-18).** Built first on `{B6370564C0BBAD45}` MobileWaterTank_FIA_01_fuel.et (a small FIA water-tank trailer), then re-based on the user's choice: `{2D92D7E09B3424BC}Prefabs/Structures/Industrial/Containers/FuelTanks/FuelTank_02/FuelTank_02_green.et` — a ~9 m industrial fuel tank, which reads far better as a depot. Root class becomes `SCR_DestructibleBuildingEntity`; root ID mirrors the parent's (`51BA89020930D32D`). Our resource GUID `{6B4E1F1000000001}` and ALL twelve component GUIDs are unchanged, so buildables.conf, the persistence config and the test are untouched by the swap.
  - New chain `OVT_FuelDepot.et <- FuelTank_02_green.et <- FuelTank_02_Base.et <- Building_Base.et` supplies MeshObject, `RigidBody ModelGeometry 1 / Static 1`, **`RplComponent Enabled 1`** (proven by `check-placeables.py --strict`), `SCR_DestructibleBuildingComponent` (metal-small, ruins swap) and — new vs the water tank — a native vanilla `Persistence` component and an `Occluder`/`WorldSubsceneComponent`. It carries NO fuel, support-station, actions, sound or editable-entity component, so nothing we add is a duplicate. The sibling `FuelTank_02_Pump_green.et` would have been the WRONG base: it attaches a full vanilla 50,000 L `FuelStation_E_01_tankpistol` pump as a child (`FuelSupportStation_FuelPump.ct`, priority VERY_HIGH, PAID), which would shadow our free HIGH station (R13) and give the depot a second, charging fuel source.
  - Estimate: 3 h

- [x] **T3.2 buildables.conf entry**
  - Description: Entry #7 "Fuel Depot": `#OVT-Build_FuelDepot`, cost 2000, XP 25, `m_bBuildAtBase 1`, refuel-station preview texture, `handler OVT_FuelDepotHandler`.
  - File(s): `Configs/Resistance/buildables.conf`
  - Estimate: 0.5 h

- [x] **T3.3 `OVT_FuelDepotHandler` server-side backstop**
  - Description: `OnPlace` true for `playerId == -1`; otherwise require nearest base non-occupying within `baseRange`; false return is free (delete precedes `TakePlayerMoney`).
  - File(s): `Scripts/Game/GameMode/Placeables/OVT_FuelDepotHandler.c` (new)
  - Estimate: 1 h

- [x] **T3.4 Persistence serializer wiring**
  - Description: Add `SCR_FuelManagerComponentSerializer "{64C6E14228B31061}"` to the `OVT_BuildableComponent` `ComponentSerializers` block (deliberate GUID reuse — file convention).
  - File(s): `Configs/Systems/Persistence/Overthrow.conf`
  - Estimate: 0.5 h

- [x] **T3.5 Localization keys**
  - Description: `OVT-Build_FuelDepot`, `OVT-Build_FuelDepot_Description`.
  - File(s): `Language/localization_Overthrow.st`
  - Estimate: 0.25 h

- [x] **T3.6 Persistence round-trip case**
  - Description: Build depot by name via `BuildItem(idx, 0, pos, angles, -1)`, set node to 1234 L, save, dirty to 7 L, reload, re-find by sphere query, assert 1234. Documented fallback: same-session Tier-D case + manual F18 — never delete coverage silently.
  - **LANDED AS THE DOCUMENTED FALLBACK (degraded, not deleted).** The reload half is structurally impossible: `OVT_PersistenceManagerComponent.ReapplyLatestSaveData()` requests exactly one instance, the GAME MODE entity (`request.Instances = {owner}`), and its own doc comment excludes "world entities, characters, vehicles, placeables". A depot is a separate tracked root (`SelfSpawn 1` under the `OVT_BuildableComponent` entity config), so nothing re-applies its record. The case therefore builds → checks the two prefab facts the vanilla fuel serializer silently depends on (scripted `SCR_FuelNode`, `MaxFuel` 10000 since A2.3, initial state 0 litres — R4) → fills to 1234 L → takes a real save → re-finds the depot by sphere query and asserts the level. **Deviation from the plan sentence:** it lives in `OVT_TEST_PersistenceRoundTripSuite`, not `OVT_TEST_PersistenceSuite`, because that suite's header forbids taking a save ("Nothing in THIS file triggers a save at all") AND it runs BEFORE this one in `OVT_TestGroup_All.conf`, so a save there would trip the round-trip capability gate's fresh-session precondition and turn the All group red. `LegacyBaseUpgrades_*` is the precedent for a save-taking, reload-free case in this suite. The reload half is owed to manual play-test step F18.
  - File(s): `Scripts/Game/Tests/TestSuites/Persistence/OVT_TEST_PersistenceRoundTripSuite.c`
  - Estimate: 2 h

---

## Phase 4: Fuel-source discovery API + polish (4/4 complete) — STANDARD

- [x] **T4.1 Modded `SCR_SupportStationManagerComponent`**
  - Description: `OVT_GetSupportStationsOfType(type, out stations)` — copies, never aliases, the internal per-type array.
  - File(s): `Scripts/Game/Components/SupportStation/Modded/SCR_SupportStationManagerComponent.c` (new)
  - Estimate: 0.5 h

- [x] **T4.2 `OVT_FuelUtils` range queries**
  - Description: `FindFuelSourcesCovering(position, out)` (station's own `GetRange()` covers position) and `FindFuelSourcesNear(position, radius, out)`.
  - File(s): `Scripts/Game/Utilities/OVT_FuelUtils.c`
  - Estimate: 1 h

- [x] **T4.3 Doxygen + file-header banners**
  - Description: `//!` on every public method; banner per `OVT_VehicleRearmUtils.c:1-17` house style.
  - File(s): all new files
  - Estimate: 0.5 h

- [x] **T4.4 Final loc pass**
  - Description: Verify all `#OVT-` keys registered; note the `Language/localization_Overthrow.en-us.conf` re-export as a follow-up.
  - File(s): `Language/localization_Overthrow.st`
  - Estimate: 0.25 h

---

## Phase 5: Help & documentation sync (1/2 complete) — help-docs-sync

- [x] **T5.1 In-game help**
  - Description: Tutorial popups + Field Manual: fuel costs money at stations (rate is difficulty-scaled), depot exists / built at captured bases / starts empty / filled from a fuel truck / dispenses free. Every claim cites a file:line or is cut.
  - File(s): `Configs/Tutorials/`, `Configs/FieldManual/`
  - Estimate: 1 h
  - Done 2026-08-18: new Field Manual page **Fuel** in the Money and Trade category (`Configs/FieldManual/Categories/FM_Overthrow.conf`, entry `{6B4E1F4000000001}`, 8 new `#OVT-FieldManual_Fuel_*` keys) covering paid static stations + the rate on the action label + the difficulty range + the can't-afford stop + the free paths + the depot. New tutorial popup **fuel-depot-built** (`Configs/Tutorials/fuelDepotBuilt.conf` + `.meta`, registered on `Prefabs/GameMode/OVT_OverthrowGameMode.et`), fired by `PLAYER_BUILD` filtered on the buildable name `"Fuel Depot"`, deep-linking to the new Field Manual page. Gap left deliberately: the trigger catalogue (`OVT_TutorialTrigger.c:12-45`) has **no refuel / vehicle event**, so "fuel now costs money" cannot get a popup of its own; in game it is carried by the action label, the greyed-out reason and the Field Manual page.

- [ ] **T5.2 Public wiki sync**
  - Description: Same content on the wiki via wikijs MCP; wiki + in-game text must agree.
  - File(s): wiki (external)
  - Estimate: 0.5 h
  - **BLOCKED 2026-08-18 (auth):** the `wikijs` MCP tools were not available in the sync session, and the API token in `Overthrow.Wiki.MCP/wiki-js-mcp/.env` can only run `pages.list` — `pages.single` returns `PageViewForbidden 6013`, so no page can be read or updated with it. Owed: new `fuel` page; a "Fuel Depot" bullet on the `base` page's owning-a-base advantages list (id 11); `fuelPricePerLitre` in the Economy table on `difficulty/settings` (id 53) and in the preset pages `difficulty/easy|normal|hard|extreme` (ids 49-52). Drafted copy is in the Phase 5 report.

---

## Amendment A1: Fast "Fill" action (4/4 complete, partly superseded by A2) — ⚠️ ADVANCED (network) — user request 2026-08-18

> Play-test feedback: the vanilla trickle refuel is too slow for 5000 L tanks. New hold-to-fill action with radial progress and upfront cost label. User chose: any vehicle at any fuel source; also fills the depot from a truck. Vanilla Refuel stays untouched.

- [x] **A1.1 Pure fill-plan math + Logic case**
  - Description: `ComputeFillPlan(litresNeeded, sourceAvailable, balance, price) → {litres, cost}` — litres = min(needed, available, affordable), cost = whole dollars ≤ balance, free source ⇒ cost 0. Logic-tier case, proven red once.
  - File(s): `Scripts/Game/Data/OVT_FuelFillPlan.c` (new), `Scripts/Game/Data/OVT_FuelPricing.c` (`ComputeFillPlan`), `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_Fuel.c` (`OVT_TEST_Logic_Fuel_FillPlan`)
  - Done 2026-08-18: cost rounds UP in the affordable branch (safe because the balance it is compared against is an int, so `ceil(fullCost) <= balance` holds by construction) and equals the balance exactly in the money-bound branch, so the caller can take it without re-checking. Free/negative price returns the full litres and never reads the balance. **Red-proven:** the source-clamp assertion was inverted to expect 100 L instead of 25 L, `tools/compile-check.sh` exit 0 with the inversion in place, then restored.
  - Estimate: 1 h

- [x] **A1.2 Server endpoint (controller-component pattern)**
  - Description: `RpcAsk_FillFuel`-style endpoint following `RpcAsk_RearmVehicle` — server re-derives source in range, litres, cost, balance; moves fuel instantly (fills receiving `SCR_FuelNode`s, drains source), charges via existing economy APIs.
  - File(s): `Scripts/Game/Components/Controller/OVT_FuelRequestComponent.c` (new), `Prefabs/GameMode/OVT_OverthrowController.et`, `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_ControllerSeam.c`
  - **LANDED AS A NEW CONTROLLER COMPONENT, not on `OVT_ShopTransactionComponent`.** The rearm precedent's own comment gives its reason for staying put — re-arm is a stop-gap the logistics epic will delete, so a component for one RPC pair was not worth it — and neither half applies here: the fast fill is permanent, a fill from a truck or the depot moves **no money at all** (so "a shop transaction with no shop" is wrong in the common case), and `resistance/high-command`'s auto-refuel tick is a named future consumer of exactly this seam. `OVT_TowerSabotageComponent` is the granularity precedent (a whole controller component for one action). Wired on the controller prefab as `{6B4E1F6000000001}` and asserted by the Init-tier roster case, which is the only thing that catches "component written but never added to the prefab".
  - Wire encoding: **one `RplId` naming the target**, nothing else — the caller comes from `ResolveOwningPlayerId()` on the controller entity the RPC arrived on. Server re-derives player → character → target → 15 m distance → every fuel manager on the target and its slotted parts → capacity → best covering non-self source with fuel → source stock → price → balance → plan; then transfers, drains, and charges for what actually arrived. **Superseded by A2.1/A2.2:** the wire is now `RplId` + tank id, and the fill is scoped to ONE manager.
  - Estimate: 2 h

- [x] **A1.3 `OVT_FillFuelAction` held action + prefab wiring**
  - Description: hold ~5 s with radial progress; label shows real cost (`Fill Tank ($X)` / free); hidden with no source in range, reason-gated otherwise; on vehicles (Vehicle_Base delta, trunk-Sell-All precedent) AND on the depot (fills depot from truck, excludes self as source).
  - File(s): `Scripts/Game/UserActions/OVT_FillFuelAction.c` (new), `Prefabs/Vehicles/Core/Vehicle_Base.et`, `Prefabs/Structures/Military/FOB/OVT_FuelDepot.et`, `Scripts/Game/Utilities/OVT_FuelUtils.c` (shared discovery/measurement half)
  - Done 2026-08-18: `Duration 5` on both prefabs (the `OVT_RearmVehicleAction` hold shape). Vehicle entry `{6B4E1F1000000010}` on the **`"fuel_cap"` context**, not the trunk's door contexts — the Sell-All *mechanism* was mirrored (same delta prefab, same `additionalActions` block, same `VisibilityRange -1`), but a fuel action belongs where the vanilla Refuel already is. Depot entry `{6B4E1F100000000E}` on its `"default"` context. **A2.2 added a third instance** on the fuel-tank part's `"supportStation_fuel"` context.
  - Estimate: 2 h

- [x] **A1.4 Loc keys + Field Manual sentence**
  - Description: action label/reason keys; one cited sentence on the FM Fuel page about fast fill.
  - File(s): `Language/localization_Overthrow.st`
  - Done 2026-08-18: 4 keys `{6B4E1F300000000D..10}` — `OVT-FillFuel`, `OVT-FillFuel_Free`, `OVT-FillFuel_SourceEmpty`, `OVT-FillFuel_TankFull` — inserted **alphabetically** before `OVT-FlipVehicle`, not at EOF; the pre-existing unbalanced brace at `{6B09A1C0E4D50121}` was left alone and the file's open/close delta is unchanged at 1. The broke-at-a-paid-source reason reuses the existing `#OVT-Refuel_CannotAfford` ("You cannot afford fuel here"), which is already exactly the right sentence. Field Manual: **one sentence** appended to `OVT-FieldManual_Fuel_Text`, with every clause cited in that key's Comment. No `FM_Overthrow.conf` edit was needed (the sentence went into an existing paragraph).
  - Estimate: 0.5 h

---

## Amendment A3: vanilla "Check Fuel" on the depot (1/1 complete) — orchestrator, 2026-08-18

- [x] **A3.1 `SCR_CheckFuelAction` on the depot's "default" context**
  - Description: Vanilla action reused verbatim (UIInfo `{8EF2434DCE844759}ActionUIInfo_CheckFuel.conf`, `#AR-Action_CheckFuel`, Duration 2.5 — the exact `Vehicle_Fuel_Tank_Base.et:48` shape). GUIDs `{6B4E1F1000000014/15}`, collision-checked. Suites skipped: prefab-only, no case asserts actions.
  - File(s): `Prefabs/Structures/Military/FOB/OVT_FuelDepot.et`

---

## Amendment A2: Fill scoping, cargo context, bigger depot (3/3 complete) — ⚠️ ADVANCED (network) — user play-test 2026-08-18

> Play-test of A1: filling at a fuel truck's fuel cap drained a full 5,000 L depot to 21%, because one press filled the chassis tanks AND the 4,500 L cargo tank (Ural-4320 300+60 / cargo 4500; M923A1 306 / cargo 4550 — 3,600+360 = the observed 3,960 L). User directive: "it should only fill the one you are doing the action on."

- [x] **A2.1 Scope Fill to the tank whose action you are holding**
  - Description: mirror vanilla's own scoping — `SCR_RefuelAtSupportStationAction.Init` resolves `pOwnerEntity.FindComponent(SCR_FuelManagerComponent)`, which is why vanilla's BaseRefuel on `fuel_cap` fills the chassis and `Refuel_CargoTank.conf` on the cargo part's `supportStation_fuel` fills the cargo tank. A1's `GetAllFuelManagers` slot walk is removed.
  - File(s): `Scripts/Game/Utilities/OVT_FuelUtils.c` (`GetOwnFuelManager` replaces `CollectFillableManagers`/`GetRefuelableCapacityOfEntity`), `Scripts/Game/Components/Controller/OVT_FuelRequestComponent.c`, `Scripts/Game/UserActions/OVT_FillFuelAction.c`
  - Done 2026-08-18: one press fills every receiving node of ONE manager — so a Ural's fuel cap still fills its 300 L main *and* 60 L reserve together (two nodes, one tank), and never touches the cargo tank. `AddFuelToManagers` deleted; `AddFuelToManager` is the only transfer path.

- [x] **A2.2 Fill on the truck cargo context (`supportStation_fuel`)**
  - Description: so fuel can move depot → truck storage → another depot.
  - File(s): `Prefabs/Vehicles/Core/Vehicle_Fuel_Tank_Base.et` (+ `.et.meta`) — **NEW same-GUID delta**
  - **Landed as the BASE delta, not per-prefab.** Verified that a base-authored action binds to a context declared only in derived prefabs: vanilla's own `Vehicle_Fuel_Tank_Base.et` authors `SCR_RefuelAtSupportStationAction {5E2CE357444CF3AF}` with `ParentContextList { "supportStation_fuel" }` and declares **no `ActionContexts` at all** — the three contexts come from `Ural4320_fuel_tank.et` / `M923A1_fuel_tank.et`, which are the only two prefabs in the game that declare `supportStation_fuel` (every CIV/FIA/MERDC variant derives from those two). One delta therefore covers every fuel truck. Meta GUID is the vanilla `{6B0F3D4B5193FD07}` and the header reproduces vanilla's verbatim — the same convention `Prefabs/Vehicles/Core/Vehicle_Base.et` and `Helicopter_Base.et` already follow. Action `{6B4E1F1000000012}` + UIInfo `{...13}`, `Duration 5`.
  - **Wire redesign (the cargo tank is not addressable).** `Vehicle_Fuel_Tank_Base.et` carries no `RplComponent`, so the request can never name it by `RplId`. Encoding is now **`RpcAsk_FillFuel(RplId rootId, int fuelTankId)`**: the root is `GetOwner().GetRootParent()` (the truck, which is networked; the depot resolves to itself), and the tank is named by the authored `m_iFuelTankID` — prefab data, identical on both machines, distinct across a vehicle (Ural chassis 1 and 2, cargo 10), and vanilla's own scoping key (`SCR_RefuelAtSupportStationAction.m_aFuelTankIDs` / `CheckIfFuelNodeIsValid`). `OVT_FuelUtils.FindFuelManagerByTankId` resolves it server-side, bounded to that root. The client only sends an id that resolves back to its own manager, else it stays hidden.
  - **Abuse analysis.** The id search is bounded to the root the server already resolved, range-checked and found a source for; an unrecognised id is a flat rejection with no fall-back. The worst a spoofed id achieves is filling the cargo tank while standing at the chassis fuel cap instead of walking three metres — a tank the same player could fill anyway, from another context on the same vehicle, at the same range, from the same source, at the same price, out of their own wallet. It cannot name a tank on any other entity. The security boundaries (whose money, which entity, how far, which source) are all re-derived and untouched; the scoping is a gameplay rule, not a security one.

- [x] **A2.3 Depot capacity 5,000 → 10,000 L**
  - Description: user decision — a depot should hold two full truck deliveries.
  - File(s): `Prefabs/Structures/Military/FOB/OVT_FuelDepot.et` (`MaxFuel 10000`), `Scripts/Game/Tests/TestSuites/Persistence/OVT_TEST_PersistenceRoundTripSuite.c` (`EXPECTED_MAX_FUEL` + its header line), `Language/localization_Overthrow.st` (Field Manual + tutorial text and both fact-check Comments), `docs/features/economy/fuel/implementation.md`, this file
  - Done 2026-08-18: `m_fInitialFuelTankState` stays 0 and `SAVED_FUEL` 1234 is still well inside the tank, so the persistence case needed only the constant. **The build-menu description was NOT touched**: the user had already shortened `OVT-Build_FuelDepot_Description` to "Stores and delivers fuel" during play-test, so only its now-stale fact-check Comment was rewritten.

---

## Needs human verification (running list)

- [x] ✅ Manual play-test steps A–H in `implementation.md` — **user confirmed all green 2026-08-18** (pump feel, broke-mid-refuel, free paths, depot build/fill/dispense, persistence F18, MP/JIP, kill switch)
- [x] ✅ `Language/localization_Overthrow.en-us.conf` re-export — done 2026-08-18 (new keys verified present in the generated file; committed in a765bd12)
- [x] ✅ 🔴 **Verify the two restored loc keys still render** — covered by the green play-test 2026-08-18: `#OVT-StartUprising` and `#OVT-SupportTooLow` were silently dropped when the string table was re-saved from Workbench (they had been parsed as nested inside the unclosed `{6B09A1C0E4D50121}` item). Restored at top level with their original GUIDs; the brace imbalance that caused it is now gone.
- [x] ✅ **A1 play-test (step I below)** — user confirmed all green 2026-08-18

### A1 manual play-test (step I)

*I1 — a car at a paid pump.* Burn some fuel, drive to any Eden pump, stand at the fuel cap. Both actions
are listed: vanilla **Refuel** and **Fill Tank ($X)**. Hold Fill for five seconds: the tank fills in one
go, money falls by roughly X, and the pump's own stock falls by the litres delivered.

*I2 — a broke player at a paid pump.* Spend down to a small amount. The label now reads the smaller
figure (what the money actually buys). Hold it: the tank part-fills, the balance lands at 0, never
negative. At exactly $0 the action is **visible and greyed** with "You cannot afford fuel here".

*I3 — a full tank.* Fill again immediately: the action is **visible and greyed** with "Tanks are
already full", and the label shows no price.

*I4 — the fuel truck, and the A2.1 scoping.* Park a fuel truck at a pump. At the truck's **fuel cap**,
Fill tops up the chassis tanks ONLY (Ural-4320: 300 L main + 60 L reserve, so ~$360 at $1/L — not
thousands). Walk to the cargo **nozzle** at the back: a second Fill there, priced for the ~4,500 L
cargo tank. Neither ever fills the other. This is the defect the user found: before A2.1 one press at
the fuel cap pulled ~3,960 L and emptied a full depot to 21%.

*I5 — free sources.* Park a filled fuel truck beside a car and Fill the car: the label reads
**"Fill Tank (Free)"**, no money moves, and the TRUCK's cargo level falls by what the car received.
Same at the Fuel Depot. A truck standing on its own, with no other source in range, shows **no Fill
action at all** on itself — at either of its two contexts (self-exclusion).

*I6 — the depot fill, and the round trip.* Park a fuel truck alongside a built Fuel Depot (broadside,
within the truck's 9 m station range of the depot's origin) and hold the depot's own **Fill Tank
(Free)** action: the depot fills from the truck in one press and the truck's gauge falls. With no truck
nearby the depot's Fill action is **hidden**, not greyed. Now the A2.2 round trip: park the (empty)
truck at a **filled** depot and press Fill at the truck's **cargo nozzle** — the depot drains into the
truck's storage, free. Drive to a second depot and press Fill on that depot. Depot → truck → depot
works in both directions, which is what A2.2 was for. A 10,000 L depot takes two full truck loads.

*I7 — free beats paid.* If a depot or a fuel truck is ever in range at the same time as a pump, the
Fill label must read **(Free)** — the source ranking prefers free over vanilla's priority order.

*I8 — clutter check.* A car parked in open country, in a town away from a station, or at a FOB shows
**no Fill action**. This is the gate that keeps it off every vehicle on the map.

*I9 — MULTIPLAYER (dedicated server + client; the harness cannot do this).* A client holds Fill: the
**client's** money falls and nobody else's; the client's own gauge and the source's level both update.
A second player watching the same vehicle sees the level change too. A listen-server HOST holds Fill:
it must work identically — the request takes the direct-call branch there, and a regression would be a
silent no-op with no error.

*I10 — kill switch.* With `fuelPricePerLitre 0`, every Fill everywhere reads **(Free)** and takes no
money.

---

## Progress Tracking

### Discovered New Tasks
- (none yet)

### Blocked Items
- T5.2 public wiki sync - no usable wiki write path in the sync session (see the task note).

---

*Update this file as tasks are completed. Mark tasks with ✅ immediately when done. Add new tasks as they're discovered.*
