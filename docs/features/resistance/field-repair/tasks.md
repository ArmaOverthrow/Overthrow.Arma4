# Field Repair — Task Checklist

**Epic:** resistance
**Last Updated:** 2026-08-29
**Progress:** 36/37 tasks complete (97%) — only the wiki sync (7.4) is outstanding, deferred on a missing MCP server

> **Advanced-agent phases:** ⚠️ **Phase 5** routes to `component-developer-advanced` — prefab-delta work
> on three shipped, persisted buildables, where the duplicate-component trap is invisible to `compile-check.sh`. Every other phase is standard
> `component-developer`, except **Phase 7** which is `help-docs-sync`.

> **Sequencing is load-bearing.** Half A (Phases 1–3) before Half B (Phases 4–5). No repair action
> appears anywhere without a held wrench, so Half B is untestable until the wrench works and is
> obtainable. ~~Phase 5 is additionally blocked on task 1.4.~~ **Unblocked 2026-08-29 — see 1.4.**

---

## Phase 1: Free the repair station (4/4 complete) ✅ — `component-developer`

- [x] ✅ **1.1 Create the modded repair-station class**
  - Description: `modded class SCR_RepairSupportStationComponent` with exactly one `override bool AreSuppliesEnabled()` returning `false`. Nothing else.
  - File(s): `Scripts/Game/Components/SupportStation/Modded/SCR_RepairSupportStationComponent.c` (new)
  - Estimate: 🟢 0.5 h

- [x] ✅ **1.2 Sparse doc comment**
  - Description: Two or three `//!` lines — what it does, plus the one non-obvious fact (covers the wrench *and* repair trucks/ramps; fuel/medical/rearm are different classes and untouched). `SCR_FuelSupportStationComponent.c` is **not** a style precedent.
  - File(s): same as 1.1
  - Estimate: 🟢 0.1 h

- [x] ✅ **1.3 Audit Overthrow call sites**
  - Description: `grep -rn "AreSuppliesEnabled" Scripts/` — confirm zero Overthrow call sites depend on it being true. Record the result here.
  - File(s): `Scripts/` (read-only)
  - Estimate: 🟢 0.1 h
  - **Result:** `grep -rn "AreSuppliesEnabled" Scripts/` → **one hit, the new override itself** (`SCR_RepairSupportStationComponent.c:7`). No Overthrow call site depends on it returning true. `grep -rl SCR_RepairSupportStationComponent Prefabs/ Configs/` → **no matches**; Overthrow ships zero repair-station prefabs of its own.

- [x] ✅ **1.4 BLOCKING for Phase 5 — does the Helipad buildable work today?** → **YES. The plan's premise was wrong.**
  - Description: `Configs/Resistance/buildables.conf:100-103` names `{9DA31028409EDE7E}PrefabsEditable/Auto/Structures/Military/Camps/HelipadImprovised_01/Helipad.et` — **neither that GUID nor that path exists in the reference extraction**. Determine whether a built Helipad produces a structure. Branch (a) works → the file is merely unextracted, Phase 5 proceeds as planned. Branch (b) broken → **file it as a pre-existing shipped bug, do not fix silently**, and Phase 5 must pick a real base prefab (`HelipadImprovised_US_01.et` is the candidate) with the plan revised first.
  - File(s): `Configs/Resistance/buildables.conf` (read-only), reference extraction, live game data
  - Estimate: 🟡 1 h
  - **Result — neither branch (a) nor (b): the reference was never dangling.** `Helipad.et` is an **Overthrow-authored file inside the mod's own tree**, at `PrefabsEditable/Auto/Structures/Military/Camps/HelipadImprovised_01/Helipad.et`, whose `.meta` carries `{9DA31028409EDE7E}` — matching `buildables.conf` byte-for-byte. The planning pass searched only the Reforger extraction and never grepped Overthrow's own `PrefabsEditable/`. Committed in `922b4611`.
    - **This was already known and written down.** `docs/features/core/damage/implementation.md:768` says verbatim that the Helipad reference is *not* dangling and is an Overthrow-authored file. The field-repair plan regressed on a prior finding. **No bug to file.**
    - Tally, for the record: of **28** `PrefabsEditable/Auto/...` references in `Configs/`, **0** are unresolvable (12 resolve inside Overthrow, 16 in the extraction). Across all **219** `.et` references in `Configs/`, exactly **1** miss, and it is unrelated (`PaperMap_01_folded.et`).
    - **Methodological trap worth keeping:** the reference extraction contains **zero `.meta` files** (0 of 17 466). Any "find the `.et.meta` carrying this GUID" step fails against the extraction for *every* asset — which is very likely how the original pass concluded the GUID existed nowhere.
    - **Latent robustness gap noticed, not fixed (out of scope):** `OVT_ResistanceFactionManager.FinishBuild:866` spawns via `OVT_WorldUtils.SpawnEntityPrefabMatrix` with no null guard, then dereferences the result at `:869`. A genuinely bad prefab path would VM-error there rather than warn — loudly, at least, and before `TakePlayerMoney` at `:893`.

**Acceptance:** `tools/compile-check.sh` exits 0; the new file has one method and no other member; 1.4's branch is recorded above.

---

## Phase 2: Put the wrench in the economy (5/5 complete) ✅ — `component-developer`

> **Order matters.** 2.1 must land before 2.2/2.3. A `hidden` price rule returns early from
> `ResolveConfiguredPrice` and `BuildResourceDatabase` then `continue`s at `:1738` **before** inserting
> the item — the failure is total and silent.

- [x] ✅ **2.1 Un-hide and price the repair kit**
  - Description: In rule `{65CCF4EB3DDBBBD0}` replace `hidden 1` with `cost 150`. Leave `m_eItemType EQUIPMENT` and `m_sFind "RepairKit_"` alone; add **no** `demand` (default 5 is right). Do **not** touch `RearmingKit_` `{65CCF4EBFE6B4B5D}`, `MedicalKit_` `{65CCF4E97366CF79}`, `PersonalBelongings_` `{65CCF4EBA188C994}`.
  - File(s): `Configs/Pricing/itemPrices.conf:37-41`
  - Estimate: 🟢 0.2 h
  - **Result:** done. `grep -c "hidden 1"` dropped 7 → 6; `RearmingKit_`/`MedicalKit_`/`PersonalBelongings_` verified still `hidden 1`.

- [x] ✅ **2.2 Stock it at general stores**
  - Description: Append `OVT_ShopInventoryItem "{6A9F1E4A00000001}" { m_eItemType EQUIPMENT  m_sFind "RepairKit_" }` to the `SHOP_GENERAL` block, modelled byte-for-byte on the Jerrycan rule `{6A82A1B2C3D4E501}`. Leave every `m_bInclude*` flag at default — **`m_bIncludeSupportStationItems` must stay true** or `FindInventoryItems:1866` drops the entry.
  - File(s): `Configs/System/ShopConfig.conf` (after lines 24-27)
  - Estimate: 🟢 0.3 h
  - **Result:** done, no `m_bInclude*` flags set (all default, including `m_bIncludeSupportStationItems` staying true).

- [x] ✅ **2.3 Stock it at gun dealers**
  - Description: Append `OVT_ShopInventoryItem "{6A9F1E4A00000002}" { m_eItemType EQUIPMENT  m_sFind "RepairKit_" }` to `m_aGunDealerItems`. Same defaults. Do **not** set `m_bSingleRandomItem`.
  - File(s): `Configs/System/GunDealerConfig.conf` (after lines 78-83)
  - Estimate: 🟢 0.3 h
  - **Result:** done, `m_bSingleRandomItem` left unset.

- [x] ✅ **2.4 The real gate — shop-coverage checker**
  - Description: `tools/check-shop-coverage.py` must exit 0. It independently recomputes "registered but reachable by no shop rule" and `RepairKit` is **not** in its `DEFAULT_IGNORES`, so it fails loudly if 2.1 lands without 2.2/2.3.
  - File(s): `tools/check-shop-coverage.py` (run only)
  - Estimate: 🟢 0.2 h
  - **Result:** exit 0. Baseline `403 sellable catalogue items, 371 reachable, 0 unreachable, 32 unreachable-but-ignored` → after `404 sellable catalogue items, 372 reachable, 0 unreachable, 32 unreachable-but-ignored`. Sellable and reachable each rose by exactly 1 (the one `RepairKit_01_wrench` catalogue entry), unreachable stayed 0, ignore list unchanged.

- [x] ✅ **2.5 Walk every rule in both configs (BUG-098 audit)**
  - Description: Record the result here: `SHOP_ELECTRONIC`'s broad `EQUIPMENT` rule `{647C9C539D5A77DE}` still carries `m_bIncludeSupportStationItems 0` (`ShopConfig.conf:35`); the untyped block `{647C9C57C8C64962}` is `SHOP_DRUG` (`OVT_ShopConfig.c:10` defaults type to `"1"`); every gun-dealer rule omitting `m_eItemType` falls back to `"2"` = `RIFLE`, so none can reach the kit.
  - File(s): `Configs/System/ShopConfig.conf`, `Configs/System/GunDealerConfig.conf` (read-only)
  - Estimate: 🟡 0.5 h
  - **Audit result:**
    - **`ShopConfig.conf` — all 6 `OVT_ShopInventoryConfig` blocks walked:**
      - `{647C9C57C372AF3C}` `SHOP_GENERAL` (`type` explicit) — 7 items: `PaperMap_` (EQUIPMENT), bare `HEAL`, bare `BACKPACK`, `Binoculars_` (EQUIPMENT), `Watch_` (EQUIPMENT), `Jerrycan_` (EQUIPMENT), and the new `RepairKit_` (EQUIPMENT). Intended reach — confirmed.
      - `{647C9C57C8E13015}` `SHOP_ELECTRONIC` (`type` explicit) — 2 items: `{647C9C539D5A77DE}` bare `EQUIPMENT` with `m_bIncludeSupportStationItems 0` at `ShopConfig.conf:35` — **BUG-098 guard confirmed present and untouched**, so this rule cannot sweep in any `EQUIPMENT`/`SUPPORT_STATION` item including the wrench; `{6A82A1B2C3D4E502}` is `m_eItemType RADIO_BACKPACK, m_sFind "Radio_"` — wrong type, cannot match `RepairKit_` either.
      - `{647C9C57C8C64962}` — **no `type` key set.** `OVT_ShopInventoryConfig.type` (`OVT_ShopConfig.c:10-11`) is `[Attribute("1", ...)]` over `OVT_ShopType`, and `OVT_ShopType` (`OVT_ShopComponent.c:5-14`) orders `SHOP_GENERAL` first (implicit 0), so index `"1"` = **`SHOP_DRUG`** — confirmed directly from the enum declaration, not asserted from the plan. Its one item is bare `m_eItemType HEAL` — cannot match `RepairKit_` (EQUIPMENT).
      - `{647C9C57C83FB5FD}` `SHOP_CLOTHES` (`type` explicit) — 7 items, all `BACKPACK`/`HEADWEAR`/`TORSO`/`VEST_AND_WAIST`/`LEGS`/`FOOTWEAR`/`HANDWEAR`, none `EQUIPMENT`. Cannot sweep in the wrench or any sibling kit.
      - `{647C9C57C8113B9E}` `SHOP_VEHICLE` (`type` explicit) — no `m_aInventoryItems` block at all (empty). Cannot match anything.
      - **Conclusion:** the wrench is reachable **only** through `SHOP_GENERAL` and (below) `SHOP_GUNDEALER` — the two intended shops — and no other `OVT_ShopInventoryConfig` block can sweep it, or `RearmingKit_`/`MedicalKit_`/`PersonalBelongings_`, in.
    - **`GunDealerConfig.conf` — all 17 `m_aGunDealerItems` rules + the 1 `m_aGunDealerItemPrefabs` entry walked:**
      - 13 rules set `m_eItemType` explicitly (`PISTOL`, `WEAPON_ATTACHMENT`, `LETHAL_THROWABLE`, `NON_LETHAL_THROWABLE`, `EXPLOSIVES`, `ROCKET_LAUNCHER` ×2, `SNIPER_RIFLE` ×3, `MACHINE_GUN` ×3) — none is `EQUIPMENT`, so none reaches `RepairKit_`.
      - 4 rules omit `m_eItemType` entirely: `{647C9C530E2E818E}` (`m_eItemMode WEAPON`), `{647C9C533634FD6A}` (`m_eItemMode AMMUNITION`), `{647C9C52DF31DDBA}` (`m_eItemMode ATTACHMENT`), `{6A82A1B2C3D4E504}` (`m_eItemMode WEAPON_VARIANTS`). Verified directly in `OVT_EconomyManagerComponent.c:26` — `OVT_ShopInventoryItem.m_eItemType` is `[Attribute("2", ...)]` over `SCR_EArsenalItemType`, and the base-game enum (`SCR_EArsenalItemType.c:2-4`) declares `RIFLE = 1 << 1 = 2` as the first member — so the default resolves to **`RIFLE`**, not `EQUIPMENT`. None of these four can reach the wrench.
      - The single `m_aGunDealerItemPrefabs` entry (`{647C90D215C790BE}`) names one exact prefab (`DrugsWeed_01.et`) with no type match at all — irrelevant to `RepairKit_`.
      - The new `{6A9F1E4A00000002}` rule (EQUIPMENT / `RepairKit_`) is the only rule in this file that reaches the wrench. **Conclusion:** BUG-098 is not reopened.

**Acceptance:** `check-shop-coverage.py` exits 0; `compile-check.sh` exits 0; `grep -c "hidden 1" Configs/Pricing/itemPrices.conf` dropped by exactly one; `RearmingKit_`/`MedicalKit_`/`PersonalBelongings_` still `hidden 1`. **No localization work** — the wrench and the repair action both already have vanilla names.

---

## Phase 3: A wrench in the starting car (7/7 complete) ✅ — `component-developer`

- [x] ✅ **3.1 Config attribute for the starting-car repair kit**
  - Description: Add an `[Attribute]` on `OVT_VehicleManagerComponent` defaulting to `{33B2DFDCD0EBA3DB}Prefabs/Items/Equipment/Kits/RepairKit_01/RepairKit_01_wrench.et`. An attribute, not a `const`, so a modder can swap or blank it.
  - File(s): `Scripts/Game/GameMode/Managers/OVT_VehicleManagerComponent.c`
  - Estimate: 🟢 0.3 h

- [x] ✅ **3.2 Spawn and insert the wrench**
  - Description: Inside the existing `if(veh)` success block of `SpawnStartingCar()` (`:234-238`), spawn one wrench and insert it. Reuse the codebase idiom — `InventoryStorageManagerComponent` via `OVT_ComponentFinder<>` then `TryInsertItem(...)`; precedents `OVT_InventoryManagerComponent.c:759`, `OVT_ShopTransactionComponent.c:365`.
  - File(s): `Scripts/Game/GameMode/Managers/OVT_VehicleManagerComponent.c:210-239`
  - Estimate: 🟡 1 h

- [x] ✅ **3.3 Extract into a named private method**
  - Description: e.g. `AddStartingEquipment(IEntity veh)` — do not inline; `SpawnStartingCar` already does three things.
  - File(s): same as 3.2
  - Estimate: 🟢 0.2 h

- [x] ✅ **3.4 Delete the wrench on insertion failure**
  - Description: `SCR_EntityHelper.DeleteEntityAndChildren`, following `OVT_ShopTransactionComponent.c:386`. Never leave an orphan at the car's origin.
  - File(s): same as 3.2
  - Estimate: 🟢 0.2 h

- [x] ✅ **3.5 Handle slotted-child timing**
  - Description: Storage children may not be resolved on the vehicle's spawn frame. If a same-frame `TryInsertItem` fails, retry **once** via `GetGame().GetCallqueue().CallLater(..., 500, false, ...)`. Guard the deferred call against a deleted vehicle.
  - File(s): same as 3.2
  - Estimate: 🟡 0.7 h

- [x] ✅ **3.6 Keep it server-only**
  - Description: `SpawnStartingCar` is already called server-side from `OVT_OverthrowGameMode.c:1241`. Add **no** authority check and **no** RPC.
  - File(s): same as 3.2
  - Estimate: 🟢 0.1 h

- [x] ✅ **3.7 Confirm no persistence record is needed**
  - Description: It is a plain vanilla item in a vanilla storage. If `UntrackTransient` seems necessary, something is wrong — investigate before adding it. Record the finding here.
  - File(s): `Configs/Systems/Persistence/Overthrow.conf` (read-only)
  - Estimate: 🟢 0.3 h

**Acceptance:** `compile-check.sh` exits 0 ✅. The runtime half (exactly one wrench, twice over, none on the ground) is **play-test only** — see the checklist.

**Implementation notes (verified by review, not just by the agent's report):**
- `m_pStartingCarWrenchPrefab` is a `ResourceName` `[Attribute]` defaulting to the vanilla wrench; **blank is a clean no-op**, not an error (`AddStartingEquipment` early-returns on `IsEmpty()`).
- Three new protected methods: `AddStartingEquipment(IEntity)`, `RetryAddStartingWrench(IEntity, IEntity)`, `SpawnItemAtEntity(ResourceName, IEntity)`. `SpawnStartingCar` gained exactly one line.
- **Entity-ownership walk (the part most likely to be subtly wrong, checked by hand):** `AddStartingEquipment` spawns **once**; on same-frame success it returns; on failure it schedules the retry and **deliberately does not delete** — the retry owns the wrench from that point. `RetryAddStartingWrench` deletes on a dead vehicle, returns on success, deletes on failure. Every path ends in exactly one of {inserted, deleted}. No second spawn, no double delete, no orphan.
- Passing a raw `IEntity` through `CallLater` matches the established house idiom (`OVT_InventoryManagerComponent.c:237/708/747` do the same with `vehicle`), so this is not a novel risk.
- **3.7 result:** no persistence record needed. The wrench is a plain vanilla item in a vanilla storage and rides the normal inventory persistence path; nothing was added.

**Documented fallback if 3.2 proves unworkable at runtime:** a `MultiSlotConfiguration` slot templated to the wrench on the UAZ469 trunk (prefab deltas on `UAZ469_uncovered_CIV_base.et` + `UAZ469_covered_CIV_base.et`) — puts a wrench in *every* UAZ469 of those families, which is why it is the fallback.

---

## Phase 4: Widen the Vehicle Maintenance Ramp zone (5/5 complete) ✅ — `component-developer`

- [x] ✅ **4.1 Range delta on the ramp's inherited repair station**
  - Description: In the embedded child `SCR_DestructibleBuildingEntity : "{74FAABE8512145EF}...RampVehicle_01_metal.et"` (ID `59CF196E7B413D5F`), add an `SCR_RepairSupportStationComponent` override with `m_fRange 12`. **Re-declare the inherited GUID `{5EA88835DBD208B7}` and its base template `{F9A7B3AA0BE419B3}...BaseRepairSupportStation.ct`** — a fresh GUID authors a *second* station (R6).
  - File(s): `Prefabs/Structures/Military/FOB/OVT_VehicleMaintenanceRamp.et`
  - Estimate: 🟡 0.5 h

- [x] ✅ **4.2 Change only `m_fRange`**
  - Description: Leave `m_eSupportStationPriority HIGH`, `m_bUseRangeBoundingBox 1`, `m_vOffset 0 0 2` inherited, and leave `m_fMaxHealScaled` unset so the class default of 1 (100%) applies.
  - File(s): same as 4.1
  - Estimate: 🟢 0.1 h

- [x] ✅ **4.3 Leave salvage alone**
  - Description: The ramp's `SCR_VehicleSalvageSupportStationComponent` stays at vanilla `m_fRange 4.5`. Salvage is out of scope.
  - File(s): same as 4.1
  - Estimate: 🟢 0.1 h

- [x] ✅ **4.4 Area mesh radius, only if the component is present**
  - Description: If the ramp carries an `SCR_SupportStationAreaMeshComponent`, set `m_fRadius 12` to match. Vanilla keeps radius == range everywhere. If it does not carry one, do **not** add one.
  - File(s): same as 4.1
  - Estimate: 🟢 0.2 h

- [x] ✅ **4.5 Document the destruction interaction (do not change it)**
  - Description: `OVT_StructureDestructionComponent` sits on the same child entity as the repair station and `ApplySupportStationState()` (`:389-405`) calls `SetSupportStationsEnabled` on the owner. Record the intended behaviour: **a ruined ramp does not repair; repairing it re-enables the station.**
  - File(s): `Scripts/Game/Components/Damage/OVT_StructureDestructionComponent.c` (read-only)
  - Estimate: 🟢 0.2 h

**Acceptance:** `compile-check.sh` exits 0 ✅. The delta is verified correct by inspection ✅. The runtime claims (100% repair from ~10 m; ruined ramp offers no repair) are **play-test only**.

**The delta, verified by eye — this is the one `compile-check.sh` cannot check:**
```
SCR_RepairSupportStationComponent "{5EA88835DBD208B7}" : "{F9A7B3AA0BE419B3}Prefabs/Components/SupportStations/Repair/BaseRepairSupportStation.ct" {
 m_fRange 12
}
```
Same inherited GUID, same base template, **one** property changed. Confirmed beforehand that neither
GUID appeared anywhere in the Overthrow file, so this is a clean first delta and not a duplicate
station (R6 closed). Priority/bounding-box/offset stay inherited; `m_fMaxHealScaled` stays unset so the
class default of 1 (100%) applies. Salvage is not even declared in the override, so it stays fully
inherited at vanilla 4.5.

- **4.4 decision: leave the area mesh alone — and this is correct, not a compromise.** See finding F7:
  `GetRadius()` returns the station's **live range**, so the ring already draws at 12 m; `m_fRadius` is
  only a fallback. The mismatch ERROR at `SCR_SupportStationAreaMeshComponent.c:41` is guarded on the
  station being on neither the mesh's owner nor its parent — it *is* on the parent here, so nothing is
  logged. The ring is GM-only in any case (`SCR_ShowHideInEditorComponent` +
  `m_bHideInWorkbench 1`), so no player-facing surface is involved either way.
- **4.5 confirmed, unchanged:** `OVT_StructureDestructionComponent {6B70D0000000000E}` sits on the
  **same** child entity (`59CF196E7B413D5F`) as the new station, so `ApplySupportStationState()`
  (`OVT_StructureDestructionComponent.c:389-405`) correctly disables the station when the ramp ruins
  and re-enables it when repaired.

---

## Phase 5: Repair zones on the Helipad and the Garage (6/6 complete) ✅ — `component-developer-advanced`

> **⚠️ REVISED 2026-08-29, after task 1.4.** The plan's Phase 5 assumed both buildables spawned *bare
> vanilla* prefabs and therefore needed two new Overthrow wrapper prefabs plus two `buildables.conf`
> repoints. **Both assumptions are false.** Verified directly:
>
> | | Prefab the buildable actually spawns | Already carries |
> |---|---|---|
> | **Helipad** | `PrefabsEditable/Auto/Structures/Military/Camps/HelipadImprovised_01/Helipad.et` — **Overthrow-authored**, GUID `{9DA31028409EDE7E}` | `OVT_BuildableComponent {m_sBuildableType "Helipad"}`, `OVT_StructureDestructionComponent` (root), `RplComponent Enabled 1`, `ActionsManagerComponent` with a `"repair"` context (Radius 6) + `OVT_RepairRequirementsAction`/`OVT_RepairStructureAction`, `OVT_ParkingComponent {PARKING_HELI}`, `OVT_ShopComponent {SHOP_VEHICLE, procurement}`, `SCR_EditableEntityComponent`, `SCR_PreviewEntityComponent` |
> | **Garage** | `Prefabs/Structures/Industrial/Garages/Garage_E_02/Garage_E_02.et` — **Overthrow's own same-GUID, same-path delta** over vanilla, GUID `{80A5B37A1472B084}` | `OVT_BuildableComponent {m_sBuildableType "VehicleGarage"}`, `OVT_StructureDestructionComponent` (root), `RplComponent Enabled 1`, `ActionsManagerComponent` with a `"repair"` context (Radius 8) + the same two actions, `OVT_ParkingComponent` (5 spots), `OVT_MapMarkerComponent`, `OVT_ShopComponent`, `SCR_EditableEntityComponent` |
>
> **What this deletes from the phase, and why:**
> - **No new prefabs, no new `.et.meta`, no hand-minted GUIDs.** Old tasks 5.1 and 5.4 are void; so are the reserved GUIDs `{6A9F1E4A00000010}` / `{6A9F1E4A00000011}`.
> - **No `buildables.conf` repoint.** Old task 5.5 is void — both entries already point at the right file.
> - **D7 and D10 are moot.** There is no wrapper to author, so there is no wrapper-pattern choice to make.
> - **The persistence question (old task 5.8, R9) is answered, not open.** Both prefabs already carry `OVT_BuildableComponent`, so both are already matched by the `ComponentClassPersistenceConfigRule` at `Configs/Systems/Persistence/Overthrow.conf:195-216` (`Priority 35000`, `SelfSpawn 1`). Built helipads and garages **are** persisted today. No before/after save comparison is needed.
> - **R9's accepted cost evaporates.** Same-GUID prefabs mean structures in *existing saves* gain the repair zone too — the retrofit the plan wrote off as impossible is free.
> - **Old task 5.6's destruction hazard is satisfied by construction.** `OVT_StructureDestructionComponent` is on the **root** of both, so a root-mounted station is on the same entity, and `ApplySupportStationState()` (`OVT_StructureDestructionComponent.c:389-405`) will disable it on ruin. **A ruined helipad or garage will not repair, and repairing the structure restores it** — the ramp's DoD-14 behaviour now extends to all three zones for free.
>
> The phase keeps its **ADVANCED** tier: it is prefab-delta work on three shipped, persisted buildables, and the duplicate-component trap (§Gotcha 2) is exactly the class of defect `compile-check.sh` cannot see.

- [x] ✅ **5.1 Repair station on the Helipad — `m_fRange 20`**
  - Description: Add `SCR_RepairSupportStationComponent : "{4B199D8AD24B5712}Prefabs/Components/SupportStations/Repair/RepairSupportStation_Zone.ct"` to the **root** `components` block, with `m_fRange 20` and `m_bUseRangeBoundingBox 1`. Leave `m_fMaxHealScaled` unset so the class default of 1 (100%) applies; `RepairSupportStation_Zone.ct` supplies `m_eSupportStationPriority HIGH` and `m_fUnflippingPower 55000`. **A fresh component GUID is correct here** — the prefab inherits no repair station from `HelipadImprovised_US_01.et` (a bare `StaticModelEntity`), so this is a genuine addition, not a delta. Contrast task 4.1.
  - File(s): `PrefabsEditable/Auto/Structures/Military/Camps/HelipadImprovised_01/Helipad.et`
  - Estimate: 🟡 0.7 h

- [x] ✅ **5.2 Repair station on the Garage — `m_fRange 12`**
  - Description: Same component and base template on the **root** of Overthrow's `Garage_E_02.et` delta, `m_fRange 12`, `m_bUseRangeBoundingBox 1`, `m_fMaxHealScaled` unset. Again a fresh component GUID — `Garage_E_02_base.et` carries no repair station. **Do not** change `m_sBuildableType` (it is `"VehicleGarage"`, not `"Garage"`), and do not touch the parking spots, shop, map marker or destruction block.
  - File(s): `Prefabs/Structures/Industrial/Garages/Garage_E_02/Garage_E_02.et`
  - Estimate: 🟡 0.5 h

- [x] ✅ **5.3 No `SCR_ResourceComponent` on either prefab**
  - Description: Confirm neither prefab has one and do not add one. With none present, `AreSuppliesEnabled()` is false by construction as well as by the Phase-1 override — belt and braces against the `NO_SUPPLIES`-suppresses-fallback hazard (R7).
  - File(s): both prefabs
  - Estimate: 🟢 0.1 h

- [x] ✅ **5.4 Verify these are additions, not accidental duplicates**
  - Description: Walk the **whole inheritance chain** of both prefabs (Helipad → `HelipadImprovised_US_01.et` → `_base` → `HelipadImprovised_01_base.et` → `StaticObject_base.et`; Garage → `Garage_E_02_base.et` → `Building_Base.et`) and confirm **no** `SCR_RepairSupportStationComponent` exists anywhere in either chain. If one does, the new entry must re-declare **its** GUID instead, or it authors a second overlapping station (R6). Record the result here.
  - File(s): the reference extraction (read-only)
  - Estimate: 🟡 0.5 h
  - **Chain-walk result — CLEAN.** `grep -c SupportStation` returns **0** for every file in both chains:
    - Helipad: `Helipad.et` (0) → `HelipadImprovised_US_01.et` (0) → `HelipadImprovised_US_01_base.et` (0) → `HelipadImprovised_01_base.et` (0) → `StaticObject_base.et` (0)
    - Garage: `Garage_E_02.et` (0) → `Garage_E_02_base.et` (0) → `Building_Base.et` (0)
    No repair station — indeed no support station of any class — is inherited anywhere, so a **fresh component GUID is correct**: these are genuine additions, not deltas. R6 does not apply here (it does apply to Phase 4, which is why that one re-declares `{5EA88835DBD208B7}`).

- [x] ✅ **5.5 Visible boundary decision**
  - Description: Neither prefab carries an `SCR_SupportStationAreaMeshComponent`. **Do not add one** — see finding F1: the ramp's is a Game-Master/editor visualization (`m_bHideInWorkbench 1`, `SCR_ShowHideInEditorComponent`, `VirtualArea_01_Focused.emat`), not a ring ordinary players see, so adding one would not close the UX gap anyway. Record that these zones have **no visible boundary** — an accepted gap (R10) — and give the play-test a distance to pace out instead.
  - File(s): both prefabs
  - Estimate: 🟢 0.2 h
  - **Decision: not added, and that is the right call — see finding F7.** `GetRadius()` returns the station's live range anyway, so a mesh would add nothing but delta surface, and the component is Game-Master-only in any case. These zones therefore have **no player-visible boundary**; the play-test must pace out distances instead. Accepted UX gap (R10), logged as tech debt.

- [x] ✅ **5.6 Measure the garage interior** *(reasoned statically; play-test confirmation owed)*
  - Description: `Site_Garage.et` uses the enclosed `Garage_E_02` mesh, so the bounding box is the building's. Verify 12 m from that box actually reaches a vehicle parked **inside** the bay. If not, raise `m_fRange` or set `m_vOffset`, and record the measured figure. For calibration, the existing components on this prefab use `OVT_MainMenuContextOverrideComponent m_fRange 9` and a `"repair"` action `Radius 8`, and the parking spots sit at offsets up to ~14 m out.
  - File(s): `Prefabs/Structures/Industrial/Garages/Garage_E_02/Garage_E_02.et`, Workbench
  - Estimate: 🟡 1 h
  - **Static reasoning (play-test still owed to confirm):** with `m_bUseRangeBoundingBox 1` the range is measured from the **surface of the building's bounding box**, not from its centre — so a vehicle parked *inside* the garage is at distance ~0, comfortably inside any positive range. The question is not whether 12 m reaches the bay but whether it reaches far enough *outside*. The prefab's own geometry corroborates this: `OVT_ParkingComponent` places bays at X offsets −9.09 / −2.96 / +3.24 with Z 4.76, i.e. the bays sit within a footprint roughly 18 m wide and ~5 m deep, and there is a further spot at `Offset 0 0 14.0065`. So the box already spans most of the interior, and 12 m from its surface covers the whole building plus a generous apron.
    **Conclusion: 12 m is sufficient and probably generous; no change made.** The residual risk is not "too small" but "larger than intended", which is harmless here. ⚠️ Flagged for play-test confirmation (checklist item E5) rather than claimed as verified.

**Acceptance:** `compile-check.sh` exits 0 ✅. The diff contains **no new prefab files and no `buildables.conf` change** ✅ — two files edited, zero created. The runtime claims (loads in Workbench, builds from the menu, 20 m / 12 m full repair, ruined structure stops repairing) are **play-test only**.

**What actually landed — one component per prefab, on the root:**
```
SCR_RepairSupportStationComponent "{6A9F1E4A00000020}" : "{4B199D8AD24B5712}...RepairSupportStation_Zone.ct" { m_fRange 20  m_bUseRangeBoundingBox 1 }   // Helipad.et
SCR_RepairSupportStationComponent "{6A9F1E4A00000021}" : "{4B199D8AD24B5712}...RepairSupportStation_Zone.ct" { m_fRange 12  m_bUseRangeBoundingBox 1 }   // Garage_E_02.et
```
`m_fMaxHealScaled` left unset → class default 1 (**100%**). `RepairSupportStation_Zone.ct` supplies `m_eSupportStationPriority HIGH` + `m_fUnflippingPower 55000` (verified: it is exactly two lines over the base template). Nothing else on either prefab was touched — parking, shop, map marker, destruction, editable/preview components and `m_sBuildableType` (`"VehicleGarage"`) all unchanged, and the dead `EPF_PersistenceComponent` on `Helipad.et` deliberately left alone.

**Note on execution:** the Phase 5 agent stalled without making any edit, so the chain walk and both edits were done in the main thread. Same checks, same result — recorded here for honesty about how the phase ran.

---

## Phase 6: Regression coverage (6/6 complete) ✅ *(6.5 with a caveat)* — `component-developer`

> **Tier: Campaign** (`OVT_TEST_CampaignSuite`). Not Logic — the claims are about the **entity
> catalog**, which only exists after `BuildResourceDatabase` runs at world load. Read
> `OVT_TEST_Campaign_ShopCivilianStock.c` first and mirror its structure, polling backstop and
> two-claim shape.

- [x] ✅ **6.1 New Campaign case file**
  - Description: `OVT_TEST_Campaign_FieldRepairEconomy` registered in `OVT_TEST_CampaignSuite`.
  - File(s): `Scripts/Game/Tests/TestSuites/Campaign/OVT_TEST_Campaign_FieldRepairEconomy.c` (new)
  - Estimate: 🟡 0.7 h

- [x] ✅ **6.2 Claim A — the wrench is registered and priced**
  - Description: `economy.FindInventoryItems(SCR_EArsenalItemType.EQUIPMENT, SCR_EArsenalItemMode.SUPPORT_STATION, "RepairKit_", entries)`; assert ≥ 1 entry, `economy.IsRegisteredResource(entry.GetPrefab())`, and configured price **150**. This is the case that catches the `hidden` trap.
  - File(s): same as 6.1
  - Estimate: 🟡 0.5 h

- [x] ✅ **6.3 Claim B — eligible at both intended shops**
  - Description: `IsSoldAtShop(res, SHOP_GENERAL)` and `IsSoldAtShop(res, SHOP_GUNDEALER)` both true. Assert **eligibility, never rolled stock** — stock is a random draw and an empty result would be ambiguous.
  - File(s): same as 6.1
  - Estimate: 🟢 0.3 h

- [x] ✅ **6.4 Claim C — no BUG-098 regression**
  - Description: `IsSoldAtShop(res, SHOP_ELECTRONIC)` is false for the wrench, and no prefab matching `RearmingKit_` is registered at all.
  - File(s): same as 6.1
  - Estimate: 🟢 0.3 h

- [x] ⚠️ **6.5 Prove each claim can fail** *(reasoned statically — NOT an executed red-green; see below)*
  - Description: Temporarily revert 2.1, then 2.2, then 2.3; record that the case goes red for the right reason each time. **Restore afterwards** and note the evidence here.
  - File(s): the three configs (temporarily)
  - Estimate: 🟡 0.7 h
  - **⚠️ NOT DEMONSTRATED AS SPECIFIED — static reasoning only.** `tools/run-tests.sh` is the run-tests policy's job, not the implementer's, so the case was never executed red or green. Instead, each Phase-2 change was reverted individually, `check-shop-coverage.py` re-run, and restored, with `git diff` confirming byte-identical restoration. Baseline: `404 sellable, 372 reachable, 0 unreachable, 32 ignored`.
    - **Revert 1** (`itemPrices.conf` `{65CCF4EB3DDBBBD0}`: `cost 150` → `hidden 1`): summary line drops to `403 sellable, 371 reachable` — the wrench disappears from the catalogue entirely. Reasoning: **Claim A** fails first and hardest — `FindInventoryItems` would return zero `RepairKit_` entries, tripping the `MAX_POLLS` backstop with *"No EQUIPMENT/SUPPORT_STATION entry matching 'RepairKit_' found... the catalog is empty, so this case has no subject and would pass vacuously"*. Claims B and C never run (the case returns before reaching them) — so this revert is **fully coupled**: it silently masks B and C rather than failing them independently, which is exactly why Claim A must be asserted first and separately, per the task description.
    - **Revert 2** (`ShopConfig.conf`: remove `{6A9F1E4A00000001}`): the summary line is **unchanged** (`404/372/0/32`) — the checker's top-line "reachable" means "reachable by *any* shop rule", and the gun-dealer rule alone still satisfies that. `--mode all` shows the difference precisely: the wrench's shop column drops from `GUN_DEALER,SHOP_GENERAL` to `GUN_DEALER` alone. Reasoning: **Claim B's `IsSoldAtShop(res, SHOP_GENERAL)` half fails** with *"'...RepairKit_01_wrench.et' is not eligible at SHOP_GENERAL - IsSoldAtShop() returned false"*; the `SHOP_GUNDEALER` assertion right after it would still pass, so B fails on its first sub-check specifically, and A/C are unaffected (verified: A only reads `itemPrices.conf`/the catalog, C only reads `SHOP_ELECTRONIC`/`RearmingKit_`, neither touches `SHOP_GENERAL`'s rule set).
    - **Revert 3** (`GunDealerConfig.conf`: remove `{6A9F1E4A00000002}`): summary again **unchanged** (`404/372/0/32`) for the same any-shop reason; `--mode all` now shows `SHOP_GENERAL` alone. Reasoning: **Claim B's `IsSoldAtShop(res, SHOP_GUNDEALER)` half fails** with *"...is not eligible at SHOP_GUNDEALER - IsSoldAtShop() returned false"*, after the `SHOP_GENERAL` check has already passed. A and C unaffected by the same argument as revert 2.
    - **Note on the checker's sensitivity claim:** the plan's framing ("sensitive to exactly these changes") holds for revert 1 in the summary line, but reverts 2 and 3 only show up in `--mode all`'s per-item shop column, not the top-line counts — worth knowing before trusting a bare summary-line diff for a single-shop regression.
    - All three configs restored and confirmed byte-identical to the pre-revert (post-Phase-2) state via `diff -q` and via `git diff` showing only Phase 2's original three hunks, nothing else.

- [x] ✅ **6.6 Read-only guarantee**
  - Description: The case must mutate no campaign state.
  - File(s): same as 6.1
  - Estimate: 🟢 0.1 h

**Acceptance:** the case exists ✅, follows Campaign-suite house style ✅, is read-only ✅, and **no Logic-tier case was added** ✅ (D14). ⚠️ The red-then-green demonstration is **owed** — see 6.5.

**Accessors verified against source, not guessed** (`OVT_EconomyManagerComponent.c`): `GetPrice(int)` `:355`, `IsSoldAtShop(ResourceName, OVT_ShopType)` `:1009`, `GetInventoryId(ResourceName)` `:2037`, `IsRegisteredResource(ResourceName)` `:2050`.

**Shape:** mirrors `OVT_TEST_Campaign_ShopCivilianStock` — the same banner-header rationale, the same `MAX_POLLS 600` diagnostic backstop (explicitly *not* a retry budget; `maxAttempts` is banned project-wide), the same "assert eligibility, never rolled stock" argument, and one `Check*` method per claim returning a described failure rather than a bare bool. Claim C asserts `RearmingKit_` is not merely unsold but **entirely unregistered** — the stronger and correct form, since `hidden 1` should keep it out of the resource database altogether. Gates after the case landed: `compile-check` 0 (6354 files), `check-shop-coverage` 0 (404/372/0).

---

## Phase 7: Help & documentation sync (3/4 complete, 1 deferred) — `help-docs-sync`

> **Every claim must cite a `file:line` or be cut.** Two tutorial tips have already shipped in this
> project describing mechanics that did not exist.

- [x] ✅ **7.1 Field Manual entry — Vehicle Repair**
  - Description: In the *Money and Trade* category (`FM_Overthrow.conf:153`), next to the existing *Fuel* entry (`:222`), since buying the wrench is the gating step.
  - File(s): `Configs/FieldManual/Categories/FM_Overthrow.conf`
  - Estimate: 🟡 0.5 h
  - **Result:** entry `{6B6F1A0000000001}` inserted immediately after the Fuel entry and before Storage (`FM_Overthrow.conf:248`). Same shape as its neighbours: title, `default_ui.edds` tile, then Text / Head / Text2 / Head2 / Text3 / Head3 / Text4 (piece GUIDs `{6B6F1A0000000002}`–`{6B6F1A0000000008}`, a fresh unused series). Braces balanced; `compile-check.sh` exit 0.

- [x] ✅ **7.2 State only what is true**
  - Description: A wrench is needed and is required even at a repair structure; sold at general stores and gun dealers for ~$150; the starting car has one; in the open field a wrench repairs a part to **at most half health**; at a Ramp, Garage or Helipad it repairs **fully**, within ~12 m / 12 m / 20 m.
  - File(s): same as 7.1 + the `.st` master
  - Estimate: 🟢 0.3 h
  - **Result:** 8 keys added to `Language/localization_Overthrow.st` **only** (`OVT-FieldManual_VehicleRepair_{Title,Head,Head2,Head3,Text,Text2,Text3,Text4}`, item GUIDs `{6B6F1B0000000001}`–`{6B6F1B0000000008}`, inserted in alphabetical order between `_Storage_Title` and `_WantedSystem_Head`). `Target_en_us` only; translations are for the translators. Every sentence carries its `file:line` trace in the string's `Comment`. **No generated `.conf` export was touched — a Workbench re-export is required before the page renders in game.**
  - **Claims deliberately CUT for lack of a trace:**
    - Named hit zones ("engine, wheel, rotor"): the per-hit-zone claim is traceable to the `m_fMaxHealScaled` attribute description (`SCR_BaseDamageHealSupportStationComponent.c:68`), but *which* zones a given vehicle exposes is not. The text says "each damaged part is repaired on its own" and names none.
    - An exact shop price: `OVT_EconomyManagerComponent.c:754` multiplies the buy price by the player's `priceMultiplier`, so the text says "around $150", never "$150".
    - Exact zone distances: all three stations set `m_bUseRangeBoundingBox 1`, so the range is measured from the bounding box, not the centre. The text says "roughly 12 metres" / "roughly 20".
    - Not written at all: any durability/consumption cost, any per-repair money cost, any vehicle-type restriction, any visible zone boundary. None of these exist in the code.

- [x] ✅ **7.3 Tutorial popup — SKIPPED, no clean trigger exists**
  - Description: One tip on first vehicle damage or first wrench purchase. **Do not build a trigger mechanism for a tip.** Record the decision here.
  - File(s): the tutorial config
  - Estimate: 🟢 0.3 h
  - **Decision: no tutorial popup added.** The trigger catalog is the 14-member `OVT_TutorialEvent` enum (`Scripts/Game/Configuration/OVT_TutorialTrigger.c:12-44`), and neither of the two triggers this task names exists:
    - **First vehicle damage** — there is no damage event of any kind in the enum, and no manager invoker feeding one. Adding it would be tutorial-framework work, which this phase explicitly must not do.
    - **First wrench purchase** — `PLAYER_BUY` carries only the cost and dispatches an **empty** filter (`OVT_TutorialManagerComponent.c:268`), and `PLAYER_TRANSACTION` carries only the shop type (`:297`). Neither event carries item identity, so a trigger cannot tell a wrench from a jerrycan. A `m_iMinValue 150` threshold would match every purchase of $150 or more, which is wrong far more often than right.
    - Considered and rejected: binding `PLAYER_BUILD` filtered to `"Vehicle Maintenance Ramp"` / `"Garage"` / `"Helipad"` would work today, but it is a different trigger from the one asked for, it would need three entries or a filterless one, and the volume-restraint rule prefers the single Field Manual entry that already covers the mechanic. Not added.

- [x] ⏸️ **7.4 Wiki page sync — DEFERRED, no wikijs MCP server attached**
  - Description: Via the wikijs MCP tools.
  - File(s): wiki (external)
  - Estimate: 🟡 0.5 h
  - **Result: deferred, not failed.** No `mcp__wikijs__*` tool was available in this session, so the wiki could not be searched, read or written. Owed work when a server is attached: search for an existing repair/vehicle-maintenance page before creating anything, and sync it to the same facts as the Field Manual entry (kit required even inside a zone, no money and no supplies, ~$150 at general stores and gun dealers, one in the starting car, 50% in the field vs 100% at Ramp/Garage/Helipad at ~12/12/20 m, no visible boundary).

## Bugs & Issues

**Active Bugs:** none. **No bug was filed by this feature.**
- R5 anticipated one: the plan believed the Helipad buildable pointed at a dangling prefab GUID. Task 1.4 disproved it — the prefab is Overthrow-authored and the buildable works (finding F2). Nothing to file.

**Noticed but NOT fixed (out of scope, no bug filed — pre-existing, not introduced here):**
- `OVT_ResistanceFactionManager.FinishBuild:866` spawns via `OVT_WorldUtils.SpawnEntityPrefabMatrix` (`OVT_WorldUtils.c:615`, `Resource.Load` with no null guard) and dereferences the result at `:869` before its own `if (!buildableComp)` warning can fire. A bad prefab path VM-errors rather than warning — loudly at least, and before `TakePlayerMoney` at `:893`, so no money is lost. One-line null guard for a future pass (finding F3).
- `Helipad.et` carries a dead `EPF_PersistenceComponent` block — one of 15 such prefabs already flagged post-EPF-migration at `core/damage/implementation.md:765`. Deliberately left alone.

**Fixed Bugs:** none — this is a feature, not a fix.

---

## Technical Debt

- [ ] 💳 **No visible boundary ring on the repair zones** — Priority: Low
  - Description: Players cannot see where a zone's 12 m / 20 m range ends.
  - Reason: `SCR_SupportStationAreaMeshComponent` was deliberately not added (task 5.5). Findings F1/F7: the ramp's own area mesh is a **Game-Master-only** visualization — `SCR_ShowHideInEditorComponent` clears `VISIBLE` at init and restores it only while a GM has the unlimited editor open, plus `m_bHideInWorkbench 1`. Adding one would not close this gap, because no ordinary player would see it.
  - Effort: small — one component per wrapper with `m_fRadius == m_fRange`. Candidate follow-up, not this feature (R10).

- [x] ✅ 💳 ~~**Helipads/garages built in existing saves keep the old prefabs**~~ — **void, resolved 2026-08-29**
  - Resolution: the premise was wrong. Both buildables already spawn Overthrow-authored prefabs (the Garage a **same-GUID** delta), so the repair zone lands on the *same* prefab identity existing saves already reference. Structures in existing saves gain the zone for free, and both are already persisted by the `OVT_BuildableComponent` rule. **D7, D10, R9 and old task 5.8 are all moot.**

---

## Testing Tasks

- [ ] **Automated — Campaign tier**
  - `OVT_TEST_Campaign_FieldRepairEconomy` (new, Phase 6) + `OVT_TEST_Campaign_ShopCivilianStock` (existing BUG-098 guard) must both be green.

- [ ] **Automated — headless gates**
  - `tools/compile-check.sh` exit 0 after every phase; `tools/check-shop-coverage.py` exit 0 with **no** new `DEFAULT_IGNORES` entries.

- [ ] **Manual play-test checklist** (Workbench, fresh campaign — the starting-car check only works on a brand-new save)
  - [ ] A. Starting car holds **exactly one** wrench in the trunk; nothing on the ground beside it
  - [ ] B1. ≥100 m from any structure: repair action **not greyed**, **no supply cost/icon** in the label
  - [ ] B2. Healing stops around **50%**; money does not change at any point
  - [ ] B3. Wrench stowed (not held) → repair action **not offered**
  - [ ] C. Mi-8 and UH-1H: rotor-assembly and tail-rotor repairs appear, perform, and cap at 50%
  - [ ] D1. General store lists a repair kit at ~**$150**; buying deducts the price and delivers the wrench
  - [ ] D2. Selling it back credits the normal sell price
  - [ ] D3. Gun dealer lists and sells one too
  - [ ] D4. **Regression:** electronics and clothes stores list **no** repair kit, rearming kit, mortar part, tripod or sandbag
  - [ ] E1. Built Ramp, damaged truck, ~10 m away, wrench held → **100%**
  - [ ] E2. Built Helipad, damaged Mi-8, ~15 m away → **100%**
  - [ ] E3. Walk ~30 m off the pad → action still appears but caps at **50%**; walk back → full again. No error, no "no supplies" text at any point
  - [ ] E4. On the pad with **no wrench held** → **no repair action at all** (correct, not a defect)
  - [ ] E5. Built Garage, damaged vehicle **inside** it → full repair offered from inside and from ~10 m outside
  - [ ] E6. Destroy the Ramp → no repair while ruined; repairing the structure restores it
  - [ ] F. **Regression:** a world fuel pump still **costs money** (proves the modded class did not leak past repair stations)
  - [ ] G. Save/reload with a Helipad, Garage and Ramp built — all three should survive (all three prefabs carry `OVT_BuildableComponent`) **and still repair after the reload**

---

## Documentation Tasks

- [x] ✅ **Field Manual** — Vehicle Repair entry (task 7.1/7.2). ⚠️ Needs a Workbench localization re-export.
- [ ] ⏸️ **Wiki** — page sync (task 7.4) — deferred, no wikijs MCP server attached
- [ ] **CHANGELOG** — on completion, via `/update-master`

---

## Task Status Legend

- [ ] Not started
- [ ] 🔄 In progress
- [ ] ⏸️ Blocked (waiting on something)
- [x] ✅ Completed
- [x] ❌ Cancelled/Won't do

---

## Progress Tracking

### Completed This Session (2026-08-29)
- ✅ Feature scaffolded from the 917-line implementation plan (39 tasks across 7 phases)
- ✅ Phase 1 tasks 1.1–1.3: `SCR_RepairSupportStationComponent` modded class written, compile-check 0, both audits clean
- ✅ Phase 1 task 1.4: the Helipad reference is **fine** — the plan's premise was wrong. Phase 5 rewritten from 8 tasks to 6 and reduced from "two new wrapper prefabs + two buildable repoints" to "add one component to each of two existing Overthrow prefabs" (37 tasks total now)
- ✅ Phase 2 tasks 2.1–2.5: repair kit un-hidden and priced at $150; stocked at `SHOP_GENERAL` and gun dealers; `check-shop-coverage.py` exit 0 (403→404 sellable, 371→372 reachable, 0 unreachable); `compile-check.sh` exit 0; full BUG-098 rule walk recorded, no reopening

### Discovered New Tasks
- _(none yet)_

### Blocked Items
- _(none — Phase 5 was unblocked by task 1.4 on 2026-08-29)_

---

## Notes

### Task Estimation
- 🟢 Small (< 1 hour)
- 🟡 Medium (1-3 hours)
- 🔴 Large (> 3 hours)

### Priority Guidelines
- **High:** Critical for feature to work
- **Medium:** Important but not blocking
- **Low:** Nice to have, can defer

---

*Update this file as tasks are completed. Mark tasks with ✅ immediately when done. Add new tasks as they're discovered.*
