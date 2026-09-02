# Vehicle Rearm — Requirements

**Epic:** logistics
**Created:** 2026-08-29 (agreed with the user in-session; a player report drove it)

## Player report and diagnosis

A player could not rearm the LAV-25. They imported 25 mm ammo at the port (ledger credit into a truck), then used Open Storage → Take → Inventory on the truck: the truck's vanilla cargo showed half full but nothing was visible. Diagnosis (2026-08-29):

- `Prefabs/Weapons/Magazines/Magazine_M242/Box_25x137_*.et` are authored by BI with `m_bVisible 0` and `ItemVolume 25000`. `OVT_StorageRequestComponent.StepToInventory` (`:1858`) spawns them into the trunk successfully; the vanilla UI never renders them. The ammo is trapped.
- Even visible, it would be useless: `SCR_TurretControllerComponent` never reloads from vehicle inventory. Vanilla's only turret/cannon/rocket rearm path is `SCR_ResupplyVehicleWeaponSupportStationAction` on the turret (`LAV25_turret_base.et:681`, `m_sItemPrefab` = the magazine it wants), which needs a `VEHICLE_WEAPON` support station in range (vanilla: only `HeliWeapons_ArsenalBox_*` compositions) and consumes Conflict supplies.
- The LAV has zero Overthrow storage: `OVT_StorageRules.ResolveAutoCapacity` (`Scripts/Game/Data/OVT_StorageRules.c:35`) returns 0 for a registered-but-illegal vehicle. Helicopters have no `OVT_StorageComponent` at all (only `Wheeled_Base.et` carries it).
- What exists: `OVT_RearmVehicleAction` + `OVT_VehicleRearmUtils` fully restock every slot-mounted turret magazine / rocket pod for money, but the action lives only on `Helicopter_Base.et` and both the client gate and `OVT_ShopTransactionComponent.RpcAsk_RearmVehicle` (`:502`) require a built helipad at a friendly base. Discovery is structural (SlotManager walk) and works for the LAV/BTR/BRDM unchanged.

## Requirements

1. **Rearm on every armed vehicle.** `OVT_RearmVehicleAction` moves from `Helicopter_Base.et` to `Vehicle_Base.et` (contexts `heli_repair_point` + the door contexts) so LAV25, BTR70, BRDM2 and the armed helicopters all carry it. **No location gate when the ledgers cover the whole rearm** — a crew that hauled the ammo can rearm in the field, anywhere. The location gate (vehicle on a built **Helipad** OR built **Garage** (`OVT_BuildableComponent` type strings) at a resistance-held base, **or** near a deployed FOB) applies only when some of the rearm has to be **bought**: off-site with a shortfall, the action is shown but blocked with a reason naming what is missing. Client gate (`CanBePerformedScript`) and `RpcAsk_RearmVehicle` both change; the reason strings stay honest (`#OVT-MustBeOnHelipad` needs a wider key).
2. **Rearm from imported ammo first, money second.** Server side, per rearmable weapon, resolve the magazine prefab (the turret's `SCR_ResupplyVehicleWeaponSupportStationAction.m_sItemPrefab`, falling back to the weapon's current magazine prefab) and `Take` it from the vehicle's own `OVT_StorageComponent` ledger, then from nearby holders' ledgers (same radius rules as the storage destination picker); only what cannot be covered is bought for the difficulty-scaled price, and the action label / toast says what was used. Rocket pods that have no magazine prefab stay money-only.
3. **Hidden magazines never leave the ledger.** `StepToInventory` refuses (counts as shortfall, line stays) any prefab whose `InventoryItemComponent` attributes carry `m_bVisible 0`, so vehicle-weapon ammo stays where (2) can consume it. Existing trapped boxes are cleared by the officer "Clear inventory" action — no migration.
4. **Armed vehicles get a small storage.** `ResolveAutoCapacity`: registered-but-illegal vehicle → new `m_iArmedVehicleCapacity` attribute (default 100) instead of 0. `Helicopter_Base.et` gets `OVT_StorageComponent` (AUTO) plus the Open/Transfer-all/Rename storage actions on helicopter contexts, so armed helis get the small cap and unarmed ones get the 300 default.
5. **One unlimited civilian helicopter.** A same-GUID delta of vanilla `Mi8MT_unarmed_civ_base.et` with `m_eCapacityMode UNLIMITED` and its own `vehiclePrices.conf` entry (the current `m_sFind "Mi8MT"` entry catches every Mi8 variant — the civ one must sort ahead of it or be matched more specifically). Resource cargo (`OVT_ResourceStoreComponent`) stays truck-only.

## Out of scope

- Any vanilla-inventory turret reload, support stations, or Conflict supplies.
- Resource (m³) cargo on helicopters.
- Per-weapon partial rearm UI; the rearm stays one action, one full restock.
