# Field Repair — Implementation Plan

**Epic:** resistance
**Status:** Ready for Review
**Started:** 2026-08-29
**Target Completion:** TBD
**Last Updated:** 2026-08-29 (built — see tasks.md and context.md for where the plan was overturned)

---

## Executive Summary

Reforger already ships a complete field-repair system that Overthrow accidentally switched off.
Every vehicle in the base game — including helicopters — carries a full set of
`SCR_RepairAtSupportStationAction` user actions (`Prefabs/Vehicles/Core/Vehicle_Base.et:169-176`,
`Prefabs/Vehicles/Core/Helicopter_Base.et:174-176`), and the handheld wrench
(`RepairKit_01_wrench.et`, GUID `{33B2DFDCD0EBA3DB}`) is the gadget those actions require. Two
things stop it working in Overthrow:

1. **The wrench charges supplies, and Overthrow has no supplies.** `RepairKit_01_base.et` carries an
   `SCR_ResourceComponent` alongside its `SCR_RepairSupportStationComponent`, so
   `AreSuppliesEnabled()` answers true and every repair is refused with `NO_SUPPLIES`.
2. **The wrench cannot be bought.** `Configs/Pricing/itemPrices.conf:37-41` marks `RepairKit_`
   `hidden 1`, which drops it from the resource database entirely — it has no price, no id, and no
   shop can ever list it.

The feature has **two halves**, and Half A is a hard prerequisite for testing Half B (no repair action
appears at all without a held wrench).

### Half A — the handheld wrench works, and you can get one

1. **A one-method modded class** — `SCR_RepairSupportStationComponent.AreSuppliesEnabled() → false` —
   makes repairs free. This alone fixes the user's headline complaint.
2. **The wrench enters the economy** at ~$150, stocked at general stores and gun dealers, through
   existing `OVT_ShopInventoryItem` machinery and the existing `OVT_ShopTransactionComponent` path.
3. **The starting car spawns with one wrench**, inserted server-side in
   `OVT_VehicleManagerComponent.SpawnStartingCar()`.

Field repair with the wrench alone is capped at **50%** per hit zone
(`RepairKit_01_base.et` overrides `m_fMaxHealScaled` to `0.5`). That cap is deliberate — see D2.

### Half B — three resistance structures become full-repair zones

1. **Vehicle Maintenance Ramp — 12 m** (up from vanilla's 4.5 m). *Nothing else changes:* the vanilla
   ramp already repairs to **100%** and already disables supplies on itself. This is a range widening
   and nothing more.
2. **Helipad — 20 m.** Needs an Overthrow wrapper prefab; the vanilla improvised helipad is a bare
   `StaticModelEntity` with a `MeshObject` and nothing else.
3. **Garage — 12 m.** Same treatment; the Garage buildable currently spawns a bare vanilla
   `Garage_E_02.et`.

**The wrench is still required in all three zones.** `SCR_RepairAtSupportStationAction.RequiresGadget()`
returns true and `SCR_BaseUseSupportStationAction.CanBeShownScript()` (`:156-158`) hides the action
outright when no matching gadget is held, regardless of any station in range. This is the user's
explicit intent ("with the wrench") and a Definition-of-Done criterion, not a defect.

**The 50%-vs-100% fallback needs no code.** `GetClosestValidSupportStation`
(`SCR_BaseUseSupportStationAction.c:77-148`) asks the support-station manager first (`:112-119`) and
only falls back to the held gadget when the manager found nothing (`:122-140`), because
`PrioritizeHeldGadget()` is false for repair. The zones register `HIGH` priority with full heal; the
wrench is `LOW` with `m_fRange -1` and never registers. Inside a zone you get 100%; outside it you get
the wrench's 50%. That is the whole UX, delivered by vanilla ordering.

**Zero new RPCs, zero new persisted records, zero new UI, zero new script systems.** Half B is prefab
and config work plus one four-line script file from Half A.

---

## Goals

### Primary Goals

1. **A player holding a wrench can repair a damaged vehicle anywhere in the world, for free**, to 50%
   per hit zone. No supply cost is displayed, none is charged, no `NO_SUPPLIES` refusal.
2. **Helicopters are repairable in the field** — rotor assembly, tail rotor, engine, hull, drivetrain.
3. **A wrench costs ~$150** and is stocked at general stores and gun dealers, bought and sold through
   the normal shop path.
4. **A new player's starting car contains one wrench.**
5. **A helicopter parked on a built Helipad repairs to 100%**, anywhere within 20 m of the pad, while
   holding a wrench.
6. **A ground vehicle at a built Vehicle Maintenance Ramp or Garage repairs to 100%** within 12 m —
   meaningfully further than vanilla's 4.5 m.
7. **Walking out of a zone degrades gracefully** to the wrench's 50%, with no error and no dead action.
8. **No economy regression.** Rearming kits, medical kits, mortar parts, tripods and sandbags stay
   exactly as unavailable as they are today — the BUG-098 class.

### Secondary Goals

9. **Static and vehicle-mounted repair stations become supply-free too**, consistently — a captured
   repair truck behaves like a hand wrench.
10. **One Campaign-tier regression case** proving the wrench is registered, priced and reachable, and
    that the rearming kit still is not.

### Explicitly Out of Scope (YAGNI — do not build these)

- **Repair-kit durability / consumption.** The wrench is reusable forever, like vanilla.
- **A money or resource cost per repair**, at the wrench or at any zone.
- **Any new repair UI, HUD element or notification.** Vanilla's action label and notifications stand.
- **Vehicle-type filtering.** See D8 — the system cannot express it, and a car parked on the helipad
  getting full repair is intended.
- **Salvage, rearm, arsenal or vehicle-weapon-resupply features riding along** with the zones. The
  ramp's existing `SCR_VehicleSalvageSupportStationComponent` stays at its vanilla 4.5 m; the new
  wrappers get a repair station and nothing else.
- **Any buildable structure beyond the three named.**
- **Free wrenches in bought, captured or ambient vehicles.** Starting car only (D3).
- **An Overthrow-authored wrench prefab.** The vanilla one is already in all four faction catalogs.
- **Un-hiding the rearming kit, medical kit or fuel-kit price rules.** Only `RepairKit_` moves.
- **Retrofitting helipads and garages already built in existing saves.** See D7 and R9.

---

## Architecture Overview

### The vanilla flow this feature unblocks

```
player holds a wrench, walks to a damaged vehicle
        |
SCR_RepairAtSupportStationAction  (authored on the VEHICLE, not the wrench)
   CanBeShownScript: RequiresGadget() && no gadget held -> HIDDEN     [:156-158]
        |
GetClosestValidSupportStation                                         [:77-148]
   +-- PrioritizeHeldGadget() == false for repair, so skip the gadget-first branch  [:83]
   +-- ask SCR_SupportStationManagerComponent for the closest valid station  [:112-119]
   |        -> a ramp/helipad/garage ZONE in range wins (HIGH priority, 100% heal)
   +-- only if the manager found nothing: fall back to the held gadget  [:122-140]
            -> the wrench (LOW priority, m_fRange -1, 50% heal)
        |
SCR_BaseSupportStationComponent.IsValid(...)
   +-- if (AreSuppliesEnabled()) { ... NO_SUPPLIES }                   [:384]     <-- TODAY'S FAILURE
        |
SCR_RepairSupportStationComponent.OnExecutedServer(...)              [SERVER]
   +-- if (AreSuppliesEnabled()) OnConsumeSuppliesServer(...)          [:152]     <-- AND HERE
   +-- heal hit zones up to m_fMaxHealScaled
```

`AreSuppliesEnabled()` lives at `SCR_BaseSupportStationComponent.c:736-741`:

```
SCR_ResourceComponent resourceComponent = GetResourceComponent();
return resourceComponent && resourceComponent.IsResourceTypeEnabled();
```

`GetResourceComponent()` (`:758-765`) resolves `SCR_ResourceComponent.FindResourceComponent(owner)`
on the **support-station entity itself** — for the handheld kit, the wrench. The wrench has one
(`SCR_ResourceComponent {5E4A9574582A1D38}` holding a `Consumer_Gadget.conf` consumer) and `SUPPLIES`
is globally enabled, so the answer is true and every repair is refused.

### One hook closes four paths

| Call site | Effect of `AreSuppliesEnabled() → false` |
|---|---|
| `SCR_BaseSupportStationComponent.c:384` — the `IsValid` gate | `NO_SUPPLIES` refusal skipped entirely |
| `SCR_BaseDamageHealSupportStationComponent.c:158-162` — `GetSupplyAmountAction` | early-returns `0`, so no cost is shown |
| `SCR_BaseDamageHealSupportStationComponent.c:201-208` — `OnExecutedServer` | `OnConsumeSuppliesServer` skipped |
| `SCR_RepairSupportStationComponent.c:121-123` — the fire-extinguish `GetSupplyAmountAction` override | early-returns `0`, so vehicle fires are free too |

**The override also protects Half B.** Note `GetClosestValidSupportStation:122`:
`if (reasonInvalid != ESupportStationReasonInvalid.NO_SUPPLIES)`. A zone that was in range but
supply-starved would suppress the wrench fallback and leave the player with **no** repair at all.
Forcing `AreSuppliesEnabled()` false on every `SCR_RepairSupportStationComponent` makes that state
unreachable. D1 and Half B reinforce each other.

### Blast radius of the override (enumerated, not guessed)

Four vanilla prefabs carry an `SCR_RepairSupportStationComponent`, and all four also carry an
`SCR_ResourceComponent`:

| Prefab | What it is | Consequence |
|---|---|---|
| `Prefabs/Items/Equipment/Kits/RepairKit_01/RepairKit_01_base.et` | the handheld wrench | **the target** — free field repair to 50% |
| `Prefabs/Vehicles/Core/Vehicle_RepairBox_Base.et` | vehicle-mounted repair box (repair trucks) | a captured repair truck works — intended |
| `Prefabs/Structures/Industrial/Repair/RampVehicle_01/RampVehicle_01_metal_base.et` | industrial repair ramp | already sets `m_aDisabledResourceTypes { SUPPLIES }`, so this is a **no-op** for it |
| `.../RampVehicle_01_concrete_base.et` | industrial repair ramp | same — already supply-free |

Overthrow ships **zero** repair-station prefabs today (`grep -rl SCR_RepairSupportStationComponent
Prefabs/ Configs/` in the working tree returns nothing). Fuel, medical and rearm stations are
different classes and are untouched.

### What the ramp already does, and what it does not

`RampVehicle_01_metal_base.et:34-49`:

```
SCR_RepairSupportStationComponent "{5EA88835DBD208B7}" : "{F9A7B3AA0BE419B3}...BaseRepairSupportStation.ct" {
 m_eSupportStationPriority HIGH
 m_fRange 4.5                  // concrete variant: 5.5
 m_bUseRangeBoundingBox 1
 m_vOffset 0 0 2               // concrete variant: 0 0 1
}
SCR_ResourceComponent {
 m_aConsumers { SCR_ResourceConsumerServicePoint : "...Consumer_SupportStation.conf" {} }
 m_aDisabledResourceTypes { SUPPLIES }
}
```

`BaseRepairSupportStation.ct` does **not** set `m_fMaxHealScaled`, so the class default applies —
`[Attribute("1", ...)]` at `SCR_BaseDamageHealSupportStationComponent.c:68-69`, i.e. **100%**. So the
ramp already repairs fully and already ignores supplies. **The only thing wrong with it is the
range.** Say so plainly and do not plan work that is already done.

Overthrow already ships the ramp as a buildable: `Configs/Resistance/buildables.conf:40-52`,
`OVT_Buildable {5D7C489A4968ED23}`, "Vehicle Maintenance Ramp", `m_iCost 1500`, buildable at
base/FOB/camp, spawning `{3D2270BA89BC2AA4}Prefabs/Structures/Military/FOB/OVT_VehicleMaintenanceRamp.et`.
That wrapper embeds the vanilla ramp as a **child entity**
(`SCR_DestructibleBuildingEntity : "{74FAABE8512145EF}...RampVehicle_01_metal.et"`, ID
`59CF196E7B413D5F`), so the range override goes into that child's component block.

### What the Helipad and the Garage have: nothing

- **Helipad.** `HelipadImprovised_01_base.et` is a bare `StaticModelEntity : StaticObject_base.et`
  with a single `MeshObject`. The editable wrapper adds only preview + `RplComponent`. No support
  station, no `OVT_BuildableComponent`.
- **Garage.** `Configs/Resistance/buildables.conf:66-96` spawns
  `{80A5B37A1472B084}Prefabs/Structures/Industrial/Garages/Garage_E_02/Garage_E_02.et` — a **bare
  vanilla structure** (`SCR_DestructibleBuildingEntity` → `Garage_E_02_base.et` → `Building_Base.et`).
  No Overthrow components at all.

Both therefore need an Overthrow wrapper prefab (D7).

### Vanilla's own helipad repair zone — the shape to copy, at a distance

`Prefabs/Compositions/Slotted/SlotFlatLarge/Helipad_L_US_01.et:398` embeds
`{7BDAD0F4A6195C01}PrefabsEditable/SupportStationSystems/E_SupportStationSystem_Repair_Helipad_BoundingBox.et`:

```
SCR_RepairSupportStationComponent "{5E1CF31185E317FE}" { m_fRange 10  m_bUseRangeBoundingBox 1 }
SCR_SupportStationAreaMeshComponent "{5B2AD5B869A9C26D}" { m_fRadius 10 }
SCR_VehicleSalvageSupportStationComponent { m_fRange 10  m_bUseRangeBoundingBox 1 }
SCR_ArsenalComponent / SCR_VehicleWeaponSupportStationComponent / storages : Enabled 0
```

That is the right *shape*, but it inherits `E_SupportStationSystem_Repair_Medium.et` →
`E_SupportStationSystem_Repair.et`, which drags in an `SCR_ResourceComponent` with three supply
generators, an `SCR_ServicePointComponent { m_eType REPAIR_DEPOT }`, a vehicle-weapon resupply
station and an arsenal. We want none of that (see Out of Scope). **D9 takes the leaner route:** a bare
`SCR_RepairSupportStationComponent` inheriting `{4B199D8AD24B5712}RepairSupportStation_Zone.ct`
directly on the Overthrow wrapper.

`RepairSupportStation_Zone.ct` is exactly two lines over the base template:
`m_fUnflippingPower 55000`, `m_eSupportStationPriority HIGH`. Full heal comes from the base default.

### The wrapper pattern to follow: `OVT_FuelDepot.et`, not the ramp

The ramp wraps by **embedding a child**. `OVT_FuelDepot.et` wraps by **inheritance**:

```
SCR_DestructibleBuildingEntity : "{2D92D7E09B3424BC}...FuelTank_02_green.et" {
  OVT_BuildableComponent, OVT_FuelSourceComponent, SCR_EditableEntityComponent,
  SCR_FuelManagerComponent, SCR_FuelSupportStationComponent, ActionsManagerComponent,
  OVT_StructureDestructionComponent, SCR_DestructibleBuildingComponent, RplComponent
}
```

Inheritance is the correct pattern here and the child-embedding is not, for one concrete reason:
`OVT_StructureDestructionComponent.ApplySupportStationState()`
(`Scripts/Game/Components/Damage/OVT_StructureDestructionComponent.c:389-405`) walks the owner **and
one level of children**, calling `SCR_BaseSupportStationComponent.SetEnabled()` per damage phase. A
station on the *root* with the destruction component on a *child* would never be switched off when
the structure is ruined. Put the station and the destruction component on the same entity. The fuel
depot does; copy it.

### Data flow, end to end

```
Configs/Pricing/itemPrices.conf   ──(un-hide, cost 150)──>  BuildResourceDatabase
                                                                   |
Configs/System/ShopConfig.conf        ──> FindInventoryItems ──> shop stock  [EconomyManager:2128]
Configs/System/GunDealerConfig.conf   ──> FindInventoryItems ──> dealer stock [TownController:335]
                                                                   |
                                                   OVT_ShopTransactionComponent  (existing)
                                                                   |
                                                         wrench in player inventory

OVT_OverthrowGameMode.c:1241  ──>  SpawnStartingCar()   [SERVER]  ──> wrench into the trunk

Configs/Resistance/buildables.conf ──> BuildItem/FinishBuild ──> OVT_Helipad.et / OVT_Garage.et
                                                                   |
                                                   SCR_RepairSupportStationComponent (HIGH, 100%)
                                                   self-registers with SCR_SupportStationManagerComponent
```

No replicated state is added by this feature. The car's storage contents replicate through the vanilla
inventory stack; the zones are ordinary components on ordinary entities.

---

## Implementation Phases

**Sequencing is load-bearing:** Half A (Phases 1-3) before Half B (Phases 4-5). No repair action
appears anywhere without a held wrench, so Half B is untestable until the wrench works and is
obtainable.

**Advanced-agent routing:** **Phase 5 warrants `component-developer-advanced`.** It creates two new
prefabs, repoints a persisted buildable's prefab identity, and interacts with the buildable
persistence rule and the structure-destruction walk. Every other phase is standard
`component-developer`.

---

### Phase 1 — Free the repair station, and one blocking verification → `component-developer`

*Estimate: ~0.25 day. Very low risk for 1.1-1.3; 1.4 is a Workbench check that gates Phase 5.*

| # | Task |
|---|---|
| 1.1 | Create `Scripts/Game/Components/SupportStation/Modded/SCR_RepairSupportStationComponent.c` containing exactly one `modded class` with one `override bool AreSuppliesEnabled()` returning `false` |
| 1.2 | Doc-comment it to the project's sparse standard: **two or three `//!` lines**. State what it does and the one non-obvious fact (it covers the wrench *and* repair trucks/ramps; fuel/medical/rearm are different classes and untouched). The rationale belongs in this document. The neighbouring `SCR_FuelSupportStationComponent.c` predates the sparse-comments rule and is **not** a style precedent |
| 1.3 | `grep -rn "AreSuppliesEnabled" Scripts/` — confirm zero Overthrow call sites depend on it being true |
| 1.4 | **BLOCKING for Phase 5.** In Workbench, verify that the Helipad buildable actually works today. `Configs/Resistance/buildables.conf:100-103` names `{9DA31028409EDE7E}PrefabsEditable/Auto/Structures/Military/Camps/HelipadImprovised_01/Helipad.et` — **neither that GUID nor that path exists anywhere in the reference extraction**, although the directory does (it holds `E_HelipadImprovised_US_01.et` and `E_HelipadImprovised_USSR_01.et`). Build a Helipad in a test campaign and record whether a structure appears. Two branches: (a) it works → the file is simply unextracted, proceed with Phase 5 as written; (b) it does not → **that is a pre-existing shipped bug, file it as a bug and do not fix it silently inside this feature**; Phase 5 then also has to choose a real base prefab (`HelipadImprovised_US_01.et`, referenced by `Site_Helipad.et`'s mesh, is the obvious candidate) and the plan must be revised before Phase 5 starts |

**Acceptance:**
- `tools/compile-check.sh` exits 0.
- The new file contains exactly one method and no other member; comments do not outweigh code.
- Task 1.4's result is recorded in `tasks.md` with the branch taken.

---

### Phase 2 — Put the wrench in the economy → `component-developer`

*Estimate: ~0.5 day. Low risk, but this phase carries the silent-failure trap.*

`ResolveConfiguredPrice` (`OVT_EconomyManagerComponent.c:878-909`) walks the price configs in file
order; later matches override earlier ones, **but a `hidden` match returns immediately**, and
`BuildResourceDatabase` then hits `if(hidden) continue;` at `:1738` **before** inserting the item into
`m_aResources` / `m_aResourceIndex` / `m_aEntityCatalogEntries`. An item with no resource id cannot be
priced, stocked, listed, bought or sold by anything, and it fails *silently*.

**Task 2.1 must land before 2.2 and 2.3, and must be verified independently.**

| # | Task |
|---|---|
| 2.1 | `Configs/Pricing/itemPrices.conf` — in rule `{65CCF4EB3DDBBBD0}` (lines 37-41), replace `hidden 1` with `cost 150`. Leave `m_eItemType EQUIPMENT` and `m_sFind "RepairKit_"` alone. Do **not** add `demand` — the attribute default of 5 (`OVT_PricesConfig.c:24-25`) is right for a convenience item. Do **not** touch `RearmingKit_` `{65CCF4EBFE6B4B5D}`, `MedicalKit_` `{65CCF4E97366CF79}` or `PersonalBelongings_` `{65CCF4EBA188C994}` |
| 2.2 | `Configs/System/ShopConfig.conf` — append to the `SHOP_GENERAL` block (after the Jerrycan rule `{6A82A1B2C3D4E501}` at lines 24-27): `OVT_ShopInventoryItem "{6A9F1E4A00000001}" { m_eItemType EQUIPMENT  m_sFind "RepairKit_" }`. Model it byte-for-byte on the Jerrycan rule and leave every `m_bInclude*` flag at its default — **especially `m_bIncludeSupportStationItems`, which must stay true**, because the catalog files the wrench as `EQUIPMENT`/`SUPPORT_STATION` and `FindInventoryItems` drops such entries when it is false (`OVT_EconomyManagerComponent.c:1866`) |
| 2.3 | `Configs/System/GunDealerConfig.conf` — append to `m_aGunDealerItems` (after `{6A82A1B2C3D4E506}` at lines 78-83): `OVT_ShopInventoryItem "{6A9F1E4A00000002}" { m_eItemType EQUIPMENT  m_sFind "RepairKit_" }`. Same defaults. Do **not** set `m_bSingleRandomItem` |
| 2.4 | Run `tools/check-shop-coverage.py`; it must exit 0. It knows nothing about this feature — it independently recomputes "registered but reachable by no shop rule", and `RepairKit` is **not** in its `DEFAULT_IGNORES` (`tools/check-shop-coverage.py:39-42`). It therefore fails loudly if 2.1 lands without 2.2/2.3, and passes only when both halves are done. **This is the phase's real gate** |
| 2.5 | Walk every rule in both configs and record the result in `tasks.md`: `SHOP_ELECTRONIC`'s broad `EQUIPMENT` rule `{647C9C539D5A77DE}` keeps `m_bIncludeSupportStationItems 0` (the BUG-098 guard, `ShopConfig.conf:35`); the untyped block `{647C9C57C8C64962}` is `SHOP_DRUG` (`OVT_ShopConfig.c:10` defaults `type` to `"1"`); every gun-dealer rule that omits `m_eItemType` falls back to the attribute default `"2"` = `SCR_EArsenalItemType.RIFLE`, not `EQUIPMENT`, so none can reach the kit |

**No localization work.** The wrench already has a vanilla display name (`#AR-Item_RepairKit_Name`)
and the repair action a vanilla label (`#AR-SupportStation_EmergencyRepair_ActionName`, set by
`RepairSupportStation_Gadget.ct`). Nothing new is named, so the `.st` master and its exports are
untouched.

**Acceptance:**
- `tools/check-shop-coverage.py` exits 0; `tools/compile-check.sh` exits 0.
- `grep -c "hidden 1" Configs/Pricing/itemPrices.conf` has dropped by exactly one vs `main`.
- `grep -n -A 4 "RearmingKit_" Configs/Pricing/itemPrices.conf` still shows `hidden 1`.

---

### Phase 3 — A wrench in the starting car → `component-developer`

*Estimate: ~0.5 day. Low-medium risk.*

**The storage question is answered.** `UAZ469_base.et:665` carries an
`SCR_UniversalInventoryStorageComponent` ("`#AR-Inventory_Trunk`") with `MaxCumulativeVolume 100000`,
`MaxItemSize 50 200 50` and `m_fMaxWeight 700`; the wrench is `ItemDimensions 5 5 5` at `0.2` kg.
`Vehicle_Base.et:160` supplies the `SCR_VehicleInventoryStorageManagerComponent`. `m_SlotType
SLOT_GADGETS_STORAGE` governs which *equipment* slot the item takes on a character; it does not gate
generic vehicle cargo. Vanilla already pre-loads this same trunk with two field dressings via
`MultiSlots` (`UAZ469_base.et:694-701`) — direct evidence the container accepts items at spawn.

| # | Task |
|---|---|
| 3.1 | Add a config attribute on `OVT_VehicleManagerComponent` for the starting-car repair kit, defaulting to `{33B2DFDCD0EBA3DB}Prefabs/Items/Equipment/Kits/RepairKit_01/RepairKit_01_wrench.et`. An attribute rather than a `const` so a modder can swap or blank it |
| 3.2 | In `SpawnStartingCar()` (`OVT_VehicleManagerComponent.c:210-239`), inside the existing `if(veh)` success block at `:234-238`, spawn one wrench and insert it. Reuse the codebase idiom — `InventoryStorageManagerComponent` via `OVT_ComponentFinder<>` then `TryInsertItem(...)`; precedents at `OVT_InventoryManagerComponent.c:759` (vehicle storage) and `OVT_ShopTransactionComponent.c:365` |
| 3.3 | Extract it into a small named private method (e.g. `AddStartingEquipment(IEntity veh)`) rather than inlining; `SpawnStartingCar` already does three things |
| 3.4 | **On insertion failure, delete the spawned wrench** — `SCR_EntityHelper.DeleteEntityAndChildren`, following `OVT_ShopTransactionComponent.c:386`. Never leave an orphan at the car's origin |
| 3.5 | Handle slotted-child timing: storage children may not be resolved on the frame the vehicle spawns. If a same-frame `TryInsertItem` fails, retry once via a short `GetGame().GetCallqueue().CallLater(..., 500, false, ...)`. Guard the deferred call against a deleted vehicle |
| 3.6 | Keep it server-only. `SpawnStartingCar` is already called server-side from `OVT_OverthrowGameMode.c:1241`; add no authority check and no RPC |
| 3.7 | Confirm the wrench needs no persistence record of its own. It is a plain vanilla item in a vanilla storage; if `UntrackTransient` seems necessary, something is wrong — investigate before adding it |

**Documented fallback if 3.2 proves unworkable at runtime:** append a `MultiSlotConfiguration` slot
templated to the wrench onto the UAZ469 trunk, as vanilla intends
(`Configs/Inventory/InventoryItem_RepairKit.conf` is exactly this pattern). Cost: prefab deltas on
`UAZ469_uncovered_CIV_base.et` and `UAZ469_covered_CIV_base.et`, and a wrench in *every* UAZ469 of
those families rather than just the starting car — which is why it is the fallback.

**Acceptance:**
- `tools/compile-check.sh` exits 0.
- A new campaign's starting car contains exactly **one** `RepairKit_01_wrench`.
- A second new campaign produces exactly one again. No loose wrench on the ground in any case.

---

### Phase 4 — Widen the Vehicle Maintenance Ramp zone → `component-developer`

*Estimate: ~0.25 day. Low risk. Deliberately first in Half B: it is the smallest possible change that
proves the whole zone mechanism, on a structure that already exists and already works.*

| # | Task |
|---|---|
| 4.1 | In `Prefabs/Structures/Military/FOB/OVT_VehicleMaintenanceRamp.et`, inside the embedded child `SCR_DestructibleBuildingEntity : "{74FAABE8512145EF}...RampVehicle_01_metal.et"` (ID `59CF196E7B413D5F`), add an `SCR_RepairSupportStationComponent` override setting `m_fRange 12`. **Re-declare the inherited component GUID `{5EA88835DBD208B7}` and its base template `{F9A7B3AA0BE419B3}Prefabs/Components/SupportStations/Repair/BaseRepairSupportStation.ct`** — a same-GUID entry is a delta over the inherited component, and a fresh GUID would author a *second* station |
| 4.2 | Change **only** `m_fRange`. Leave `m_eSupportStationPriority HIGH`, `m_bUseRangeBoundingBox 1` and `m_vOffset 0 0 2` at their inherited values, and leave `m_fMaxHealScaled` unset so the class default of 1 (100%) still applies |
| 4.3 | Leave the ramp's `SCR_VehicleSalvageSupportStationComponent` at its vanilla `m_fRange 4.5`. Salvage is out of scope |
| 4.4 | Check whether the ramp carries an `SCR_SupportStationAreaMeshComponent`. If it does, set `m_fRadius` to 12 to match. Vanilla keeps radius and range equal on every station in the reference tree (e.g. `E_SupportStationSystem_Repair_Helipad_BoundingBox.et` pairs `m_fRange 10` with `m_fRadius 10`), and a mismatch means the visible ring lies about the real range. If it does not carry one, do **not** add one |
| 4.5 | Confirm the destruction interaction is already correct: `OVT_StructureDestructionComponent` sits on the same child entity as the repair station, and `ApplySupportStationState()` (`OVT_StructureDestructionComponent.c:389-405`) calls `SetSupportStationsEnabled` on the owner. Document the intended behaviour — **a ruined ramp does not repair, and repairing the ramp re-enables it** — rather than changing it |

**Acceptance:**
- A truck parked at a built ramp and a player holding a wrench get a repair action that heals to
  **100%**, from at least 10 m away (the old limit was 4.5 m).
- The area mesh ring, if present, matches the working range.
- A ruined ramp offers no vehicle repair; repairing the ramp restores it.

---

### Phase 5 — Helipad and Garage wrapper prefabs → ⚠️ `component-developer-advanced`

*Estimate: ~1 day. The highest-risk phase: two new prefabs, two buildable repoints, and an
interaction with buildable persistence.*

**Blocked on Phase 1 task 1.4.** Do not start until the Helipad buildable's current behaviour is
known.

| # | Task |
|---|---|
| 5.1 | Create `Prefabs/Structures/Military/FOB/OVT_Helipad.et` (+ `.et.meta`, hand-authored GUID `{6A9F1E4A00000010}`). **Follow `OVT_FuelDepot.et`'s inheritance pattern, not the ramp's child-embedding pattern** — inherit the vanilla helipad prefab directly and put the components on the root |
| 5.2 | Components on `OVT_Helipad.et`, and no others: `SCR_RepairSupportStationComponent : "{4B199D8AD24B5712}Prefabs/Components/SupportStations/Repair/RepairSupportStation_Zone.ct"` with `m_fRange 20`, `m_bUseRangeBoundingBox 1`; `OVT_BuildableComponent { m_sBuildableType "Helipad" }`; `RplComponent`; and `OVT_MapMarkerComponent` mirroring the ramp's (`repair` icon from `icons_wrapperUI-32.imageset`). **Do not** inherit `E_SupportStationSystem_Repair_*.et` — it drags in an `SCR_ResourceComponent` with three supply generators, an `SCR_ServicePointComponent`, a vehicle-weapon resupply station and an arsenal, all out of scope (D9) |
| 5.3 | Do **not** add an `SCR_ResourceComponent` to either wrapper. With none present, `AreSuppliesEnabled()` is false by construction as well as by the Phase-1 override — belt and braces on the `NO_SUPPLIES`-suppresses-fallback hazard |
| 5.4 | Create `Prefabs/Structures/Military/FOB/OVT_Garage.et` (+ `.et.meta`, GUID `{6A9F1E4A00000011}`) the same way, inheriting `{80A5B37A1472B084}Prefabs/Structures/Industrial/Garages/Garage_E_02/Garage_E_02.et`, with `m_fRange 12` and `m_sBuildableType "Garage"` |
| 5.5 | Repoint `Configs/Resistance/buildables.conf`: the Helipad entry `{5EE4FAB564EA1AA1}` (`m_aPrefabs` at line 100-103) and the Garage entry `{5D98AC7AC353F540}` (line 70-72) to the new prefabs. Change nothing else in those entries — cost, XP, resource requirements and `m_SitePrefab` all stay |
| 5.6 | **Decide and document the destruction story.** Neither structure carries `OVT_StructureDestructionComponent` today. Adding one is scope creep; not adding one means a helipad cannot be ruined, which is the status quo. Default: **do not add it.** If a later feature does, the station must sit on the same entity as the destruction component (`ApplySupportStationState` walks owner + one level of children only) — record that constraint in a one-line comment on the prefab's Overthrow doc, not in the prefab |
| 5.7 | If `SCR_SupportStationAreaMeshComponent` is added to either wrapper for a visible ring, keep `m_fRadius` equal to `m_fRange` (20 and 12). If not added, note in `tasks.md` that these zones have **no visible boundary** — that is an accepted UX gap for now, and the play-test steps must give the tester a distance to pace out instead |
| 5.8 | Verify the 12 m bounding-box zone actually reaches a vehicle parked **inside** the garage. `Site_Garage.et` uses the `Garage_E_02` mesh, so the finished structure is an enclosed building and the bounding box is the building's, not a flat pad's. If 12 m from the bounding box does not cover the interior bay, raise the range or adjust `m_vOffset` and record the measured figure |

**Persistence — verify, do not assume.** Built structures are persisted by
`Configs/Systems/Persistence/Overthrow.conf:195-216`: an `EntityPersistenceConfig` whose `Rule` is
`ComponentClassPersistenceConfigRule { ComponentClass "OVT_BuildableComponent" }`, `Priority 35000`,
`SelfSpawn 1`. Neither the current Helipad nor the current Garage prefab has an
`OVT_BuildableComponent`, and neither is matched by any other self-spawning rule — the vanilla
`Building.conf` rule that catches `Garage_E_02.et` (via `Building_Base.et`) sets no `SelfSpawn`, and
the helipad descends from `StaticObject_base.et` and matches nothing. **The strong expectation is
therefore that built helipads and garages do not survive a save/load today, and that adding the
wrapper fixes that as a side effect.** Confirm this by save/reload in Workbench before and after, and
record both results. If it turns out they *are* persisted by some path this plan did not find, stop
and re-assess R9 before shipping.

**Acceptance:**
- Both prefabs load in Workbench with no missing-component or missing-parent errors.
- Both buildables build successfully from the resistance build menu at a base.
- A helicopter within 20 m of a built helipad, player holding a wrench, repairs to 100%.
- A truck within 12 m of a built garage (including parked inside it), player holding a wrench,
  repairs to 100%.
- Save/reload behaviour before and after the change is recorded in `tasks.md`.

---

### Phase 6 — Regression coverage → `component-developer`

*Estimate: ~0.5 day.*

**Tier: Campaign** (`OVT_TEST_CampaignSuite`, `Scripts/Game/Tests/TestSuites/Campaign/`). Not Logic —
the claims are about the **entity catalog**, which only exists after `BuildResourceDatabase` runs at
world load, and what can go wrong is vanilla catalog data meeting an Overthrow config rule. This is
the argument `OVT_TEST_Campaign_ShopCivilianStock.c` makes in its own header; read that file first and
mirror its structure, polling backstop and two-claim shape.

| # | Task |
|---|---|
| 6.1 | New case `Scripts/Game/Tests/TestSuites/Campaign/OVT_TEST_Campaign_FieldRepairEconomy.c` in `OVT_TEST_CampaignSuite` |
| 6.2 | Claim A — **the wrench is registered and priced.** Resolve it via `economy.FindInventoryItems(SCR_EArsenalItemType.EQUIPMENT, SCR_EArsenalItemMode.SUPPORT_STATION, "RepairKit_", entries)`, assert at least one entry, assert `economy.IsRegisteredResource(entry.GetPrefab())` and that its configured price is 150. This is the case that catches the `hidden` trap |
| 6.3 | Claim B — **eligible at both intended shops.** `economy.IsSoldAtShop(res, OVT_ShopType.SHOP_GENERAL)` and `IsSoldAtShop(res, OVT_ShopType.SHOP_GUNDEALER)` both true. Assert eligibility, never rolled stock — stock is a random draw and an empty result would be ambiguous |
| 6.4 | Claim C — **no BUG-098 regression.** `IsSoldAtShop(res, SHOP_ELECTRONIC)` is false for the wrench, and no prefab matching `RearmingKit_` is registered at all |
| 6.5 | Prove each claim can fail: temporarily revert 2.1, then 2.2, then 2.3, and record that the case goes red for the right reason each time. Restore afterwards; note the evidence in `tasks.md` |
| 6.6 | Read-only — the case must mutate no campaign state |

Do **not** add a Logic-tier case. There is no pure function in this feature; a Logic case would be
vacuous or would duplicate the config parse `check-shop-coverage.py` already does headlessly. The zone
behaviour cannot be automated either — it needs a held gadget, a damaged vehicle and a user action.

**Acceptance:** the case exists, follows Campaign-suite house style, is read-only, and each of its
three claims has a recorded red-then-green demonstration. **The orchestrator runs the suite after
implementation — this plan does not.**

---

### Phase 7 — Help & documentation sync → `help-docs-sync`

*Estimate: ~0.25 day. Required: this feature changes what players see and do.*

| # | Task |
|---|---|
| 7.1 | Field Manual — a **Vehicle Repair** entry in the *Money and Trade* category (`Configs/FieldManual/Categories/FM_Overthrow.conf:153`), next to the existing *Fuel* entry (`:222`), since buying the wrench is the gating step |
| 7.2 | The entry states, and only states, what is true: a wrench is needed and is required even at a repair structure; it is sold at general stores and gun dealers for about $150; the starting car has one; in the open field a wrench repairs a part to **at most half health**; at a Vehicle Maintenance Ramp, Garage or Helipad it repairs **fully**, within roughly 12 m / 12 m / 20 m |
| 7.3 | Consider one tutorial popup on first vehicle damage or first wrench purchase — **only** if a clean trigger already exists. Do not build a trigger mechanism for a tip |
| 7.4 | Wiki page sync via the wikijs MCP tools |

**Every claim must cite a `file:line` or be cut.** Two tutorial tips have already shipped in this
project describing mechanics that did not exist. The "half health" claim traces to
`RepairKit_01_base.et`'s `m_fMaxHealScaled 0.5`; the ranges trace to the values set in Phases 4 and 5;
the "wrench required everywhere" claim traces to
`SCR_BaseUseSupportStationAction.CanBeShownScript():156-158`.

---

## Key Technical Decisions

**D1 — Remove the supply requirement via a modded script class, scoped to repair stations.**
*(User decision.)* `Scripts/Game/Components/SupportStation/Modded/SCR_RepairSupportStationComponent.c`
overrides `AreSuppliesEnabled()` to `false`. One file, no prefab deltas, covering the handheld wrench
*and* every static/vehicle repair station consistently. Rearm, medical and fuel stations are different
classes and keep vanilla behaviour.

*Rejected alternative A — a same-GUID prefab delta on `RepairKit_01_base.et` setting
`m_aDisabledResourceTypes { SUPPLIES }`.* Data-only, but scoped to the handheld kit alone:
`Vehicle_RepairBox_Base.et` would still demand supplies, so a captured repair truck would stay broken.

*Rejected alternative B — disabling `SUPPLIES` game-mode-wide on `OVT_OverthrowGameMode.et`.*
`SCR_ResourceComponent.IsResourceTypeEnabled()` (`:139`) consults
`SCR_ResourceSystemHelper.IsGlobalResourceTypeEnabled()` (`:9`) → `SCR_BaseGameMode.IsResourceTypeEnabled()`
→ `!m_aDisabledResourceTypes.Contains(resourceType)`. One line, but the blast radius is the entire
resource system: rearm, medical and building budgets read the same flag, and the logistics cargo-bed
presentation rides the same `SCR_ResourceComponent` path.

**D2 — Keep the vanilla 50% cap on the handheld wrench.** *(User decision.)* `RepairKit_01_base.et`
overrides `m_fMaxHealScaled` to `0.5` (the underlying `RepairSupportStation_Gadget.ct` ships `0.2`;
the kit raises it). Leave both alone. Field repair makes a vehicle *usable*, never *good* — and with
Half B, the cap is what makes the repair structures worth building. **No config change implements this
decision; the work is to not touch it.**

**D3 — Only the starting car gets a free wrench.** *(User decision.)* One insertion point, in
`SpawnStartingCar()` (`OVT_VehicleManagerComponent.c:210-239`), server-side. Bought, captured and
ambient vehicles get nothing free — at $150 the wrench is not a gate, and seeding every vehicle would
flood the world with them.

**D4 — ~$150, at general stores AND gun dealers.** *(User decision.)* For calibration against the
existing table (`itemPrices.conf`): a weapon attachment is $150, binoculars $100, a radio $75, a
pistol $400.

**D5 — Zone ranges: Helipad 20 m, Ramp 12 m, Garage 12 m, all with `m_bUseRangeBoundingBox 1`.**
*(User decision.)* A Mi-8 is roughly 15-20 m rotor tip to tail, so 20 m lets a player work anywhere
around one on the pad. 12 m is a truck plus room to walk around it, about 2.7× vanilla's 4.5 m. Where
an `SCR_SupportStationAreaMeshComponent` is present its `m_fRadius` must equal `m_fRange` — vanilla
keeps them equal on every station in the reference tree (e.g.
`E_SupportStationSystem_Repair_Helipad_BoundingBox.et` pairs `m_fRange 10` with `m_fRadius 10`), and a
mismatch means the visible ring lies about the real range.

**D6 — The Garage is in scope.** *(User decision.)* Players will expect a garage to repair vehicles,
it is the more expensive structure ($8,000 plus 60 timber / 120 cement / 60 steel / 30 hardware), and
it is the same one-component change as the helipad.

**D7 — New Overthrow wrapper prefabs for the Helipad and the Garage.** *(User decision.)* Both
buildables currently spawn bare vanilla prefabs — `HelipadImprovised_01` descends from
`StaticObject_base.et` with only a `MeshObject`; the Garage spawns
`{80A5B37A1472B084}Garage_E_02.et`. Overthrow's convention is a wrapper prefab under
`Prefabs/Structures/Military/FOB/`, and four already exist there (`OVT_FuelDepot`, `OVT_MedicalTent`,
`OVT_RecruitmentTent`, `OVT_VehicleMaintenanceRamp`).

*Rejected alternative — a same-GUID delta on the vanilla helipad/garage prefabs.* It would retrofit
every instance including already-built ones, but it breaks Overthrow's wrapper convention and
silently changes vanilla content mod-wide, including helipads and garages that are part of authored
world compositions and have nothing to do with the resistance.

**Accepted cost:** helipads and garages already built in existing saves keep the old prefab. See R9 —
the likely reality is that they were never persisted at all, which makes this a non-issue, but that
must be verified in Phase 5 rather than assumed.

**D8 — No vehicle-type filtering.** *(User decision.)* There is no allowed-vehicle-type attribute
anywhere on `SCR_BaseSupportStationComponent` — only `m_eFactionUsageCheck`
(`SCR_BaseSupportStationComponent.c:120-121`) and `m_bIsVehicle` (whether the *station* is mounted on
a vehicle). "Helicopters at the helipad, ground vehicles at the ramp" is not expressible. The bounding
box and where the thing is parked do the job naturally. **A car parked on the helipad will also get
full repair, and that is intended.** Do not invent a scripted filter.

**D9 — Author a bare `SCR_RepairSupportStationComponent` on the wrappers, rather than embedding
vanilla's `E_SupportStationSystem_Repair_Helipad_BoundingBox.et`.** Embedding is closest to vanilla and
would bring salvage and unflipping for free, but it inherits
`E_SupportStationSystem_Repair_Medium.et` → `E_SupportStationSystem_Repair.et`, which drags in an
`SCR_ResourceComponent` with three supply generators, an `SCR_ServicePointComponent { m_eType
REPAIR_DEPOT }`, a vehicle-weapon resupply station and an arsenal — all explicitly out of scope, and
the resource component reintroduces exactly the supply coupling D1 exists to remove. The lean route
(`RepairSupportStation_Zone.ct`, which is only `m_fUnflippingPower 55000` + `m_eSupportStationPriority
HIGH` over the base template) is fully under our control and is the same reason Overthrow wrapped the
ramp itself. Unflipping still comes along, from the `.ct`.

**D10 — Inherit, do not child-embed, in the new wrappers.** `OVT_VehicleMaintenanceRamp.et` embeds
the vanilla ramp as a child; `OVT_FuelDepot.et` inherits its base prefab and puts everything on the
root. Follow the fuel depot. Concrete reason: `OVT_StructureDestructionComponent.ApplySupportStationState()`
(`OVT_StructureDestructionComponent.c:389-405`) walks the owner **and one level of children** — a
station on the root with a destruction component on a child would never be disabled on ruin. Keeping
the station and any future destruction component on the same entity keeps that correct by
construction.

**D11 — The station priority ordering is the fallback mechanism; write no code for it.**
`GetClosestValidSupportStation` (`SCR_BaseUseSupportStationAction.c:77-148`) asks the manager first
(`:112-119`) and only falls back to the held gadget (`:122-140`) when the manager returned nothing,
because `PrioritizeHeldGadget()` is false for repair. The manager returns "only support stations of
highest available priority" (`SCR_SupportStationManagerComponent.c:70`, ordered insert at `:168-172`).
Zones are `HIGH`; the wrench is `LOW` and, with `m_fRange -1`, never registers at all. **One hazard:**
the fallback at `:122` is skipped when `reasonInvalid == NO_SUPPLIES`, so a supply-starved zone would
leave a player with no repair whatsoever. D1's override plus D9's no-resource-component rule make that
state unreachable.

**D12 — Ride the existing shop transaction path; add no bespoke charging.** The epic's dominant defect
class is client-computed costs paid via a generic money RPC, with the charge and the effect as two
unlinked RPCs (BUG-042/043/047/048/051/053). The wrench is an ordinary catalog item and
`OVT_ShopTransactionComponent` already handles spawn, insert, charge and refund-on-failure
(`:340-395`).

**D13 — Un-hiding `RepairKit_` must not re-open BUG-098, and the existing guard covers it.** Vanilla
files repair, rearming and fuel kits plus crew-served weapon parts as `EQUIPMENT` in `SUPPORT_STATION`
mode, and a broad `EQUIPMENT` rule with no `m_sFind` matches all of them — how tripods reached a
civilian electronics store (`OVT_EconomyManagerComponent.c:44-51`). The fix already shipped:
`m_bIncludeSupportStationItems 0` on the `SHOP_ELECTRONIC` rule (`ShopConfig.conf:35`), enforced at
`FindInventoryItems:1866`. Both new rules use an explicit `m_sFind`, so they are narrow by
construction; the reverse direction is safe by the Phase-2.5 audit.

**D14 — Campaign tier for the regression case, not Logic.** The claim under test is about the entity
catalog, which only exists post-`BuildResourceDatabase`. `check-shop-coverage.py` covers the same
ground statically and is the fast gate; the Campaign case runs against real loaded catalogs.

---

## Definition of Done

### Functional Criteria — Half A (the wrench)

1. **Field repair works with no supply cost.** A player holding a `RepairKit_01_wrench`, standing at
   the open hood of a damaged UAZ469 in a running campaign, sees the vanilla repair action (labelled
   from `#AR-SupportStation_EmergencyRepair_ActionName`). It is **not** greyed out, its label shows
   **no supply cost**, and holding it raises the damaged hit zone's health. It is never refused with a
   "no supplies" reason.
2. **The 50% cap applies away from any repair structure.** Repairing a badly damaged engine with the
   wrench alone, at least 50 m from any ramp/garage/helipad, stops at roughly half health and the
   action then reports the part cannot be healed further. Health does not reach 100%.
3. **Vehicle fires can be extinguished with the wrench** at no supply cost.
4. **Helicopters repair in the field.** On a damaged Mi-8 and a damaged UH-1H, rotor-assembly and
   tail-rotor repair actions are available and perform while the player holds a wrench.
5. **The wrench is registered in the economy.** In a started campaign,
   `OVT_Global.GetEconomy().IsRegisteredResource("{33B2DFDCD0EBA3DB}Prefabs/Items/Equipment/Kits/RepairKit_01/RepairKit_01_wrench.et")`
   returns true and its configured price is **150**.
6. **A repair kit appears in a general store's stock list priced at ~$150** and can be bought; money
   decreases by the shown price and the wrench arrives in the buyer's inventory.
7. **A repair kit appears in a gun dealer's stock list** and can be bought there too.
8. **A freshly spawned starting car contains exactly one `RepairKit_01_wrench`** in its trunk. Not
   zero, not two, and no wrench lying on the ground beside the car.

### Functional Criteria — Half B (the zones)

9. **A helicopter on a built Helipad repairs to 100%.** A damaged Mi-8 landed on a Helipad built at a
   base, player holding a wrench: every damaged hit zone can be repaired to **full** health, not 50%.
   This works while the player stands anywhere within **20 m** of the pad and stops working beyond it.
10. **A truck at a built Vehicle Maintenance Ramp repairs to 100% from at least 10 m.** Pace out
    roughly 10 m from the ramp — clearly beyond vanilla's 4.5 m — and the repair action still offers
    full repair.
11. **A vehicle at a built Garage repairs to 100% within 12 m, including parked inside the building.**
12. **The fallback works in both directions.** The same player who just got a 100% repair on the pad
    walks ~30 m off it, and the repair action still appears but now caps at **50%**. Walking back onto
    the pad restores full repair. No error, no dead action, no "no supplies" message at any point.
13. **The wrench is required at the zones too.** A player standing on a built Helipad beside a damaged
    helicopter with **no wrench held** sees **no repair action at all**. This is correct behaviour and
    must not be reported as a defect.
14. **A ruined Vehicle Maintenance Ramp offers no vehicle repair**, and repairing the ramp restores it.

### Quality Criteria

15. **No economy regression (BUG-098 class).** None of the following has become purchasable or
    sellable at any shop: `RearmingKit_*`, `MedicalKit_*`, mortar parts (`Items/Equipment/Mortars/`),
    tripods (`Items/Equipment/Tripods/`), sandbags, barbed tape. Specifically: the `RearmingKit_`,
    `MedicalKit_` and `PersonalBelongings_` price rules still carry `hidden 1`, and
    `IsSoldAtShop(<any EQUIPMENT/SUPPORT_STATION prefab other than RepairKit_/Jerrycan_>, SHOP_ELECTRONIC)`
    is false.
16. **`tools/compile-check.sh` exits 0.**
17. **`tools/check-shop-coverage.py` exits 0** with no new entries added to its `DEFAULT_IGNORES`.
18. **`OVT_TEST_Campaign_ShopCivilianStock` still passes** — the standing BUG-098 guard.
19. **No new RPC, no new serializer, no new replicated property, no new localization key** was added.
    New prefabs are limited to `OVT_Helipad.et` and `OVT_Garage.et` (plus `.meta`).
20. **Comments are sparse.** The new modded class is not majority-comment.
21. **No salvage, rearm, arsenal or supply behaviour was added to any zone.** Neither new wrapper
    carries an `SCR_ResourceComponent`, an `SCR_ArsenalComponent`, an `SCR_ServicePointComponent` or a
    `SCR_VehicleWeaponSupportStationComponent`; the ramp's salvage station is still at 4.5 m.

### Integration Criteria

22. **The wrench buys and sells through the normal path.** Purchase goes through
    `OVT_ShopTransactionComponent`; selling a wrench back to a general store credits the player at the
    normal sell price. No feature-specific transaction code exists.
23. **Other support stations are undisturbed.** Refuelling at a world fuel pump still **costs money**
    (the `economy/fuel` feature); medical and rearm stations behave exactly as before.
24. **Repair trucks and industrial repair ramps also repair for free** and throw no errors on spawn or
    deletion.
25. **Both new buildables build and cost correctly.** The Helipad still costs $1,500 + 60 cement + 20
    steel; the Garage still costs $8,000 + 60 timber + 120 cement + 60 steel + 30 hardware; both still
    go through their construction-site prefabs.
26. **Save/reload behaviour of built helipads and garages is documented**, before and after the
    change, in `tasks.md`.
27. **Server authority is intact.** The starting-car wrench is spawned and inserted only on the
    server. Repairs are validated and applied by vanilla's `OnExecutedServer`.

### Verification Method

**Step 1 — Static checks (cheap, do these first).**

```bash
cd "/mnt/n/Projects/Arma 4/Overthrow.Arma4"

# The modded class exists and is minimal
cat Scripts/Game/Components/SupportStation/Modded/SCR_RepairSupportStationComponent.c
#   expect: one `modded class SCR_RepairSupportStationComponent`, one
#           `override bool AreSuppliesEnabled()` returning false, nothing else

# Price rule un-hidden and costed
grep -n -A 5 "65CCF4EB3DDBBBD0" Configs/Pricing/itemPrices.conf
#   expect: m_eItemType EQUIPMENT / m_sFind "RepairKit_" / cost 150 / NO `hidden 1`

# Kits that must STAY hidden
grep -n -A 4 "RearmingKit_\|MedicalKit_\|PersonalBelongings_" Configs/Pricing/itemPrices.conf
#   expect: `hidden 1` on all three

# Both shop rules exist and neither disables support-station items
grep -n -B 2 -A 3 "RepairKit_" Configs/System/ShopConfig.conf Configs/System/GunDealerConfig.conf

# The BUG-098 guard is still on the electronics rule
grep -n -A 3 "647C9C539D5A77DE" Configs/System/ShopConfig.conf
#   expect: m_bIncludeSupportStationItems 0

# Starting-car insertion, inside the success block, with delete-on-failure
grep -n -A 30 "void SpawnStartingCar" Scripts/Game/GameMode/Managers/OVT_VehicleManagerComponent.c

# Ramp range widened as a DELTA on the inherited component GUID
grep -n -A 6 "5EA88835DBD208B7" Prefabs/Structures/Military/FOB/OVT_VehicleMaintenanceRamp.et
#   expect: the same GUID and the same BaseRepairSupportStation.ct base path, m_fRange 12

# The two new wrappers, and what they do NOT contain
grep -n "m_fRange\|SCR_RepairSupportStationComponent\|OVT_BuildableComponent\|RplComponent" \
    Prefabs/Structures/Military/FOB/OVT_Helipad.et Prefabs/Structures/Military/FOB/OVT_Garage.et
#   expect: m_fRange 20 (helipad) / 12 (garage)
grep -c "SCR_ResourceComponent\|SCR_ArsenalComponent\|SCR_ServicePointComponent" \
    Prefabs/Structures/Military/FOB/OVT_Helipad.et Prefabs/Structures/Military/FOB/OVT_Garage.et
#   expect: 0 for both

# Buildables repointed, nothing else changed in those entries
git diff main -- Configs/Resistance/buildables.conf
#   expect: two m_aPrefabs lines only

# No scope creep
git diff --stat main -- Scripts/ Configs/ Prefabs/
#   expect: no new RpcAsk/RpcDo, no new *Serializer*, no .st changes

tools/compile-check.sh          # expect exit 0
tools/check-shop-coverage.py    # expect exit 0
```

**Step 2 — Workbench play-test, in this order.** Start a **fresh** campaign; the starting-car check
only works on a brand-new save.

*A. Starting car (first — a second campaign destroys it)*
1. Start a new campaign, reach your home, find the parked starting UAZ469.
2. Open its inventory: **exactly one wrench** in the trunk. Nothing on the ground beside the car.

*B. Free field repair and the 50% cap*
3. Take the wrench. Drive at least 100 m from home so no repair structure is anywhere near. Damage
   the car.
4. Open the hood and hover the repair action: **not greyed**, and **no supply cost or supply icon** in
   the label.
5. Hold it. The engine hit zone heals, then healing **stops around 50%** and the action reports it can
   do no more. Money does **not** change at any point.
6. Stow the wrench (do not hold it): the repair action is **not offered**. The gadget requirement is
   still enforced.

*C. Helicopter in the field*
7. Spawn a Mi-8 and a UH-1H via the editor, away from any structure. Damage the rotor assembly and
   tail rotor on each.
8. With a wrench held, confirm rotor-assembly and tail-rotor repair actions appear and perform, and
   cap at 50%.

*D. Shops*
9. Travel to a town general store: a repair kit is listed at about **$150**. Buy one; money falls by
   the price and the wrench appears in inventory.
10. Sell it back; money rises by the normal sell price.
11. Find a gun dealer: a repair kit is listed there too. Buy one.
12. **Regression:** open an electronics store and a clothes store. **No** repair kit, rearming kit,
    mortar part, tripod or sandbag is listed at either.

*E. The repair zones — the Half B gate*
13. Capture or find a base. Build a **Vehicle Maintenance Ramp**. Park a damaged truck at it. Standing
    ~10 m from the ramp with a wrench held, repair a damaged part: it goes to **100%**, not 50%.
14. Build a **Helipad**. Land a damaged Mi-8 on it. Standing ~15 m from the pad with a wrench held,
    repair the rotor assembly: **100%**.
15. **Walk ~30 m off the pad** with the same damage on a second hit zone. The repair action still
    appears, but now caps at **50%**. Walk back onto the pad: full repair is available again. No error
    message and no "no supplies" text at any point in this walk.
16. With **no wrench held**, stand on the pad beside the damaged helicopter: **no repair action at
    all**. Correct.
17. Build a **Garage**. Drive a damaged vehicle **inside** it. With a wrench held, confirm full repair
    is offered from inside the building and from ~10 m outside it.
18. Destroy the Vehicle Maintenance Ramp. Confirm it offers no vehicle repair while ruined, and that
    repairing the structure restores the capability.

*F. Other support stations (regression)*
19. Drive to a world fuel pump and refuel: it **still costs money**. This proves the modded class did
    not leak past repair stations.

*G. Persistence*
20. With a Helipad, a Garage and a Ramp built, save and reload. Record what survives. Compare against
    the same test run on `main` before the change (Phase 5's before/after record).

**Step 3 — Autotests.** Run the **Campaign** tier, which holds both the new
`OVT_TEST_Campaign_FieldRepairEconomy` and the standing BUG-098 guard
`OVT_TEST_Campaign_ShopCivilianStock`; then the **All** group for a full sweep. Suite class
`OVT_TEST_CampaignSuite`, under `Scripts/Game/Tests/TestSuites/Campaign/`. *(Planning does not run
these; the orchestrator runs them after implementation.)*

---

## Testing Strategy

| Layer | Covers | Notes |
|---|---|---|
| `tools/compile-check.sh` | the modded class and the vehicle-manager edit compile | Free. Run after every phase. |
| `tools/check-shop-coverage.py` | the un-hide/shop-rule pair is complete and consistent | Headless. **The primary gate for Phase 2** — it independently re-derives reachability and fails if either half is missing. |
| Campaign tier — `OVT_TEST_Campaign_FieldRepairEconomy` (new) | wrench registered, priced 150, eligible at `SHOP_GENERAL` + `SHOP_GUNDEALER`, **not** at `SHOP_ELECTRONIC`; rearming kit still unregistered | Read-only. Mirrors `OVT_TEST_Campaign_ShopCivilianStock`. |
| Campaign tier — `OVT_TEST_Campaign_ShopCivilianStock` (existing) | BUG-098 guard | Must stay green. |
| Manual play-test | everything else | The supply gate, the 50%/100% split, the zone ranges, the fallback, helicopter actions, the starting-car insert, prefab loading and multiplayer are **all** manual. |

**Not covered by any automated tier, and known to be so:**
- Any repair action executing (needs a held gadget, a damaged vehicle and a user action).
- The 50% cap and the 100% zone heal.
- The zone ranges and the in/out fallback — these are the feature's headline behaviour and are
  **only** provable by pacing them out in Workbench.
- The starting-car insertion.
- Both new prefabs loading, building and persisting.
- Multiplayer / JIP: a client joining after a zone was built; two players repairing the same vehicle.

**No Logic-tier case.** There is no pure function in this feature. A Logic case would be vacuous or
would re-parse the same configs `check-shop-coverage.py` already parses.

---

## Dependencies

**Requires (all present; nothing to build first):**
- `OVT_EconomyManagerComponent` — resource database, price configs, `FindInventoryItems`,
  `IsSoldAtShop`, `IsRegisteredResource`.
- `OVT_ShopTransactionComponent` — the purchase/sell path (`:340-395`).
- `OVT_TownController` — gun-dealer stocking (`:324-374`).
- `OVT_VehicleManagerComponent.SpawnStartingCar()` — the insertion point (`:210-239`).
- `OVT_ResistanceFactionManager.BuildItem` / `FinishBuild` — the buildable spawn path (`:784-860`),
  and `Configs/Resistance/buildables.conf`.
- `OVT_BuildableComponent` + the buildable persistence rule
  (`Configs/Systems/Persistence/Overthrow.conf:195-216`).
- `OVT_StructureDestructionComponent.ApplySupportStationState()` (`:389-405`).
- `Scripts/Game/Components/SupportStation/Modded/` and its house patterns.
- Vanilla: `SCR_RepairSupportStationComponent`, `SCR_RepairAtSupportStationAction`,
  `SCR_SupportStationManagerComponent`, `RepairSupportStation_Zone.ct`,
  `SCR_VehicleInventoryStorageManagerComponent`, and the repair actions already authored on
  `Vehicle_Base.et` / `Helicopter_Base.et`.

**Blocked by:** nothing external. Internally, **Phase 5 is blocked by Phase 1 task 1.4** and Half B is
blocked by Half A.

**Blocks:** nothing.

**Coordination risk:** `Configs/Pricing/itemPrices.conf`, `Configs/System/ShopConfig.conf`,
`Configs/System/GunDealerConfig.conf` and `Configs/Resistance/buildables.conf` are shared,
frequently-edited files. Re-read them immediately before editing and keep each edit to the smallest
possible hunk.

---

## Risks & Mitigation

| # | Risk | Severity | Mitigation |
|---|---|---|---|
| R1 | **A shop rule is added without un-hiding the price rule.** The item never appears, nothing is logged, and it looks like the shop rule is wrong. | **High** (likelihood), Medium (impact) | Phase 2 orders 2.1 first and makes `check-shop-coverage.py` the gate — it fails loudly on exactly this state. Claim A of the Campaign case asserts `IsRegisteredResource` directly. |
| R2 | **Un-hiding `RepairKit_` lets another rule sweep it, or a sibling kit, onto a wrong shelf** — the BUG-098 class. | Medium | Both new rules carry an explicit `m_sFind`. Phase 2.5 walks every rule in both configs. `SHOP_ELECTRONIC` keeps `m_bIncludeSupportStationItems 0`. Guarded by the existing `ShopCivilianStock` case and new Claim C. |
| R3 | **The UAZ469 refuses the wrench at spawn time** because slotted storage children are not resolved on the spawn frame. | Medium | Task 3.5's deferred single retry. Prefab evidence is strong (`UAZ469_base.et:665` universal trunk, `MaxItemSize 50 200 50` vs a 5×5×5 / 0.2 kg item, plus vanilla's own two pre-slotted bandages at `:694-701`), so this is a timing risk, not a capability one. `MultiSlotConfiguration` fallback documented in Phase 3. |
| R4 | **A spawned wrench is orphaned in the world** when insertion fails. | Low | Task 3.4 — delete on failure, following `OVT_ShopTransactionComponent.c:386`. |
| R5 | **The Helipad buildable is already broken.** `buildables.conf:100-103` names `{9DA31028409EDE7E}...Helipad.et`, and neither that GUID nor that path exists anywhere in the reference extraction. | **High** (uncertainty), High (impact on Phase 5) | Phase 1 task 1.4 is a blocking Workbench check with two explicit branches. If broken, file it as a pre-existing shipped bug and revise Phase 5's base prefab choice before starting — **do not fix it silently inside this feature**. |
| R6 | **A fresh-GUID component on the ramp authors a SECOND repair station** instead of overriding the inherited one, giving overlapping zones with different ranges. | Medium | Task 4.1 requires re-declaring the inherited GUID `{5EA88835DBD208B7}` and its base template path. Verify by the static grep in the Verification Method. Same-GUID entries are deltas; fresh GUIDs are additions. |
| R7 | **A supply-starved zone suppresses the wrench fallback**, leaving a player with no repair at all — `GetClosestValidSupportStation:122` skips the gadget branch when `reasonInvalid == NO_SUPPLIES`. | Medium (if it happened), Very low (likelihood) | Two independent guards: D1's override forces `AreSuppliesEnabled()` false on every repair station, and D9/task 5.3 give the new wrappers no `SCR_ResourceComponent` at all. Play-test step E15 exercises the boundary explicitly. |
| R8 | **The 12 m garage zone does not reach a vehicle parked inside the building.** `Site_Garage.et` uses the `Garage_E_02` mesh, so the finished structure is enclosed and the bounding box is the building's. | Medium | Task 5.8 measures it; raise `m_fRange` or adjust `m_vOffset` and record the figure. DoD criterion 11 tests inside the building specifically. |
| R9 | **Existing saves keep the old Helipad/Garage prefabs and never gain repair.** | Low (accepted by the user) | Accepted. Likely a non-issue: built structures are persisted only by the `ComponentClass "OVT_BuildableComponent"` rule (`Configs/Systems/Persistence/Overthrow.conf:195-216`, `SelfSpawn 1`), neither current prefab has that component, and neither is matched by another self-spawning rule — vanilla's `Building.conf` (which catches `Garage_E_02.et` via `Building_Base.et`) sets no `SelfSpawn`, and the helipad descends from `StaticObject_base.et` and matches nothing. So they were most likely never restored anyway, and the wrapper *fixes* that. **Verify by save/reload before and after in Phase 5** rather than assuming; if they turn out to be persisted by some path, re-assess before shipping. |
| R10 | **The area mesh ring lies about the real range**, or the zones have no visible boundary at all and players cannot tell where they are. | Low-Medium (UX) | D5 requires `m_fRadius == m_fRange` wherever the component is present. Task 5.7 requires recording the decision if no ring is added, and the play-test steps give paced distances instead. Accepted UX gap for now; a visible ring is a candidate follow-up, not this feature. |
| R11 | **Dangling invoker on a deleted ranged repair station.** `DelayedInit` inserts `TEMP_OnInteractorReplicated` into the resource component's invoker unconditionally (`SCR_BaseSupportStationComponent.c:1108`), but `OnDelete` removes it only when `AreSuppliesEnabled()` is true (`:1207-1215`). | Low | **Cannot affect the handheld wrench**: `RepairSupportStation_Gadget.ct` sets `m_fRange -1` and `OnDelete` early-returns at `if (!UsesRange()) return;` first. **Cannot affect the new wrappers**: they have no `SCR_ResourceComponent`, so `m_ResourceConsumer`/`m_ResourceGenerator` stay null and the block is skipped. Only the vanilla ramps and repair boxes reach it, where the resource component lives on the same entity and dies with it. Watch for warnings on repair-truck destruction during play-test. |
| R12 | **Free full repair at cheap structures trivialises vehicle attrition.** | Low (design) | Accepted. Bounded by the build cost ($1,500 / $8,000 plus resources), by the need to hold a $150 wrench, and by the fact that the structures only exist at bases, FOBs and camps you already control. The 50% field cap keeps the away game hard. |
| R13 | **A concurrent session edits the same four config files** and a merge silently drops a rule. | Low | Small hunks; re-read before editing; `check-shop-coverage.py` after any merge involving them. |
| R14 | **`m_fMaxHealScaled 0.5` is accidentally "fixed"** by a later contributor reading it as a bug. | Low | D2 records it as deliberate, and DoD criterion 2 asserts the cap as a *requirement*, not an artefact. |

---

## Quality Bar

This is a backend/config/prefab feature touching a live player-facing economy and three persisted
buildables. Weight verification toward **reliability, economy integrity, correct server authority and
prefab correctness** — there is nothing visual to polish.

**Non-negotiable:**

1. **No economy regression.** The BUG-098 family (deployable parts on civilian shelves) is the single
   most likely way this feature causes harm, and it is invisible in normal play until a player
   reports buying a mortar bipod at a corner shop. `check-shop-coverage.py` must exit 0, the existing
   `ShopCivilianStock` case must stay green, and the Phase 2.5 rule walk must be **recorded** rather
   than merely asserted.
2. **The un-hide is verified independently of the shop rules.** "The wrench appears in a shop"
   confirms both halves; "the wrench does not appear" confirms neither and tells you nothing about
   which broke. Assert `IsRegisteredResource` and the price on their own.
3. **Prefab deltas must be deltas.** Re-declare inherited component GUIDs; a fresh GUID silently adds
   a second component. This has cost this project a full session before (a duplicate
   `ActionsManagerComponent` made a later component unreachable), and `compile-check.sh` cannot see
   it. The static grep in the Verification Method exists for exactly this.
4. **Server authority stays where vanilla put it.** All repair validation and application runs in
   `OnExecutedServer`, which this feature does not touch. The starting-car insert is server-only. If
   an `RpcAsk`, an `RplProp` or a serializer appears in the diff, the design has been abandoned — the
   epic's dominant defect class is client-computed effects paid through unlinked money RPCs.
5. **The zones stay repair-only.** No salvage, rearm, arsenal, service-point or supply behaviour rides
   along. Every one of those is a whole subsystem with its own economy implications, and each would be
   free to add by inheriting the wrong vanilla prefab — which is precisely why D9 rejects that route.
6. **Minimal surface.** One new script file with one method, one new test file, one function edited,
   two new prefabs, and a handful of config lines. A meaningfully larger diff needs justifying against
   this plan.
7. **Sparse comments.** The reasoning lives in this document. The modded class gets two or three `//!`
   lines, not the essay `SCR_FuelSupportStationComponent.c` carries — that file predates the current
   standard and is not a style precedent.

**Explicitly lower priority:** action label wording, notification polish, shop-list ordering, stock
quantities, and the absence of a visible zone boundary ring. All vanilla or existing behaviour.
