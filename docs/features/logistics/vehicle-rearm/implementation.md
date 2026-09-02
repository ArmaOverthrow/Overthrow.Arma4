# Vehicle Rearm — Implementation Plan

**Status:** Ready for Review
**Started:** 2026-09-01
**Target Completion:** TBD
**Last Updated:** 2026-09-01 17:10

**Epic:** `logistics` (feature #6 — see `docs/features/logistics/epic-overview.md`)
**Requirements:** `docs/features/logistics/vehicle-rearm/requirements.md` — five requirements, all in scope, approved by the user 2026-08-29. R1 was amended the same day (location gate applies **only to the money portion**).
**Approach (agreed, not re-opened):** extend the shipped stop-gap rearm (`Scripts/Game/UserActions/OVT_RearmVehicleAction.c`, `Scripts/Game/Utilities/OVT_VehicleRearmUtils.c`, `OVT_ShopTransactionComponent.RpcAsk_RearmVehicle` `:502`) rather than build a new system. Ammunition is sourced **ledger-first** through `OVT_StorageComponent`/`OVT_StorageLedger`; the capacity rule changes in `Scripts/Game/Data/OVT_StorageRules.c`; prefab work lands on `Prefabs/Vehicles/Core/Vehicle_Base.et`, `Prefabs/Vehicles/Core/Helicopter_Base.et` and one new same-GUID delta of vanilla `Mi8MT_unarmed_civ_base.et`.
**Branch:** `main` at `14db4021`. Concurrent sessions share this tree — re-baseline before every phase.

---

## 1. Executive Summary

A player imported 25 mm ammunition for their LAV-25 and could not get it into the vehicle, could not see it, and could not use it. Every link in that chain is broken for a different reason, and the diagnosis in `requirements.md` names all four:

1. `Prefabs/Weapons/Magazines/Magazine_M242/Box_25x137_M242_150rnd_HEIT_Base.et:33` is authored `m_bVisible 0`, and vanilla's inventory refuses to draw a slot for it (`SCR_InventorySlotUI.c:103`). `OVT_StorageRequestComponent.StepToInventory` (`:1858`) spawns it successfully into the trunk, debits the ledger, and the item is gone forever.
2. Even a visible box would do nothing: `SCR_TurretControllerComponent` never reloads a turret from vehicle inventory. The only vanilla path is a `VEHICLE_WEAPON` support station.
3. The LAV has no Overthrow storage at all — `OVT_StorageRules.ResolveAutoCapacity` (`:35`) returns `0` for a registered-but-illegal vehicle, and `OVT_StorageHolderQuery.FilterHolders` (`OVT_StorageUtils.c:195`) then hides it from every destination picker.
4. Overthrow already ships a working full restock — `OVT_VehicleRearmUtils.PerformRearm` (`:148`) — but the action is authored only on `Helicopter_Base.et:10` under the `heli_repair_point` context, and both gates demand a built helipad.

This feature closes the loop without inventing a mechanism. The rearm stays **one action, one full restock**; what changes is where it lives, what it costs, and where the ammunition comes from.

**Ledger-first, money second.** Per rearmable weapon the server resolves the prefab that weapon eats — `BaseMuzzleComponent.GetDefaultMagazineOrProjectileName()` (`BaseMuzzleComponent.c:43`), the public accessor Overthrow already uses at `OVT_HighCommandManagerComponent.c:3172` — counts what nearby ledgers hold, `Take`s it, and charges the difficulty-scaled price only **pro rata on the units it could not cover**. Cover everything and the rearm is free; cover nothing and it costs exactly what it costs today. This is not new machinery: `OVT_HighCommandManagerComponent.RearmGroup` (`:3069-3119`) already does precisely this for AI groups against warehouse ledgers, through `OVT_WarehouseStockUtils.CountAvailable`/`TakeUpTo` (`:89`, `:124`). Vehicle rearm reuses those two list operations verbatim and supplies its own, wider collector.

**The location gate follows the money.** A crew that hauled the ammunition can rearm anywhere in the field. Only the *purchased* portion needs a supply point — a built Helipad (`"Helipad"`, `Helipad.et:14`) or a built Garage (`"VehicleGarage"`, `Garage_E_02.et:5`) at a resistance-held base, or within 100 m of a deployed FOB. Because contents never leave the server (`OVT_StorageComponent.c:35-38`), the client cannot compute coverage; a **server-computed quote** pushed to the one looking player supplies the number in the action label and the honest blocked reason.

**Hidden magazines stop leaking.** `StepToInventory` refuses any prefab whose `SCR_ItemAttributeCollection.m_bVisible` reads `0` anywhere in its prefab chain, counting it as shortfall and leaving the line where the rearm can eat it. That is the actual fix for the player's report: the ammunition can no longer be trapped, because it can no longer leave.

**Armed vehicles become holders.** `ResolveAutoCapacity`'s illegal branch stops returning `0` and returns a new small cap (100). The LAV, BTR and BRDM already carry `OVT_StorageComponent` through `Wheeled_Base.et:8` and need no prefab change at all; helicopters get one on `Helicopter_Base.et`, and one civilian Mi-8 gets an unlimited flying warehouse.

Net effect on shipped code: three files gain a rule, one job step gains a refusal, one server handler is rewritten, one user action is rewritten, four prefabs and two configs are touched, and nothing is deleted.

---

## 2. Goals

### Primary

1. **The Re-arm action reaches every armed vehicle** — LAV-25, BTR-70, BRDM-2, the armed jeeps (`M151A2.et:243,260` and `UAZ469_base.et:288,328` both carry `door_l01`/`door_r01`) and every helicopter — through one authored action on `Vehicle_Base.et` and a context list that is *proven* against each target prefab rather than assumed.
2. **Ammunition is spent before money.** A full restock draws from the vehicle's own ledger first, then from every usable holder in the destination-picker radius, and buys only the remainder.
3. **The price is honest and pro rata.** Cover every unit → free. Cover none → today's `OVT_VehicleRearmUtils.GetRearmCost()` exactly. Cover half → half.
4. **No location gate on a fully covered rearm**, and a blocked off-site rearm says what is missing rather than "must be on a helipad".
5. **Hidden items never leave a ledger** — the one-way trap that started this feature is closed for every hidden prefab in the game, not just the M242 boxes.
6. **Armed vehicles hold 100 items**; helicopters hold something at all; one civilian Mi-8 holds everything.
7. **Everything a helicopter now stores survives a save** — the helicopter persistence config gains the storage serializer it never had.

### Secondary

1. **One sourcing vocabulary.** Vehicle rearm and High Command's group rearm resolve the same magazine prefab through the same public accessor and drain ledgers through the same two statics. No second copy of "what does this weapon eat".
2. **The quote is a display value, never authority.** The server re-derives coverage, price, site and funds inside `RpcAsk_RearmVehicle`; a stale or spoofed quote can only mislabel a button.

### Explicitly out of scope

Everything in the requirements' Out of Scope section, restated only where an implementer might reach for it anyway:

- **No vanilla turret reload, no support stations, no Conflict supplies.** `SCR_ResupplyVehicleWeaponSupportStationAction` is *read about* in D3 and then not used.
- **No per-weapon partial rearm and no rearm UI.** One action, one full restock, one price. If the ledger covers three of four magazines, all four still come out full and the fourth is bought.
- **No resource (m³) cargo on helicopters.** `OVT_ResourceStoreComponent` stays on `Wheeled_Truck_Base.et` and is not mentioned again.
- **No new controller component.** The two new RPCs join `OVT_ShopTransactionComponent`, so `OVT_TEST_Init_ControllerSeam.c`'s hard-coded roster count (epic tech debt) does not move.
- **No storage manager, no new ledger, no capacity persistence.** Storage's D7/D8/D10 stand unchanged.

---

## 3. Architecture Overview

### 3.1 Where each piece lives

```
Scripts/Game/Data/                                    PURE, Logic-tier testable, no world
├── OVT_VehicleRearmRules.c          NEW   prefab resolution order + pro-rata price
└── OVT_StorageRules.c               TOUCH ResolveAutoCapacity + 6th arg; HiddenFromInventory()

Scripts/Game/Utilities/
├── OVT_VehicleRearmUtils.c          REWRITE  weapon->unit plan, site test widened, PerformRearm kept
├── OVT_StorageUtils.c               TOUCH  + CollectStores(), + PlayerMayDrawFrom()
├── OVT_WarehouseStockUtils.c        HEADER-ONLY  the three list ops are now shared (D5)
└── OVT_PrefabUtils.c                TOUCH  + IsItemHiddenInInventory() (ancestry walk + cache)

Scripts/Game/Components/
├── OVT_StorageComponent.c           TOUCH  + m_iArmedVehicleCapacity attribute, passed to the rule
└── Controller/
    ├── OVT_ShopTransactionComponent.c   REWRITE RpcAsk_RearmVehicle; + RpcAsk_RearmQuote,
    │                                    RpcDo_RearmQuote, m_OnRearmQuote
    └── OVT_StorageRequestComponent.c    TOUCH  StepToInventory refuses hidden prefabs (:1858)

Scripts/Game/UserActions/
└── OVT_RearmVehicleAction.c         REWRITE  quote-driven gate, three reasons, two labels

Prefabs/
├── Vehicles/Core/Vehicle_Base.et            + OVT_RearmVehicleAction (moved, wider contexts);
│                                              3 storage actions gain 3 context names
├── Vehicles/Core/Helicopter_Base.et         − OVT_RearmVehicleAction; + OVT_StorageComponent (AUTO)
└── Vehicles/Helicopters/Mi8MT/
    └── Mi8MT_unarmed_civ_base.et            NEW same-GUID delta {366EA0B41474A7F8}, UNLIMITED

Configs/
├── Pricing/vehiclePrices.conf               + "Mi8MT_unarmed_civ" entry (longest match wins)
├── Systems/Persistence/Overthrow.conf        + OVT_StorageComponentSerializer on the HELICOPTER
│                                              EntityPersistenceConfig {64EE8D74EB8192BA} (:132-154)
└── overthrowBroadcastMessages.conf           + "Rearmed" preset

Language/localization_Overthrow.st            + 4 keys (master only — never the .conf exports)

Scripts/Game/Tests/TestSuites/
├── Logic/OVT_TEST_Logic_StorageRules.c       TOUCH  6 existing assertions gain the 6th arg + 2 new
├── Logic/OVT_TEST_Logic_VehicleRearm.c       NEW
├── Init/OVT_TEST_Init_StorageSeam.c          TOUCH  case E inverted; case F/G added
├── Init/OVT_TEST_Init_VehicleRearmSeam.c     NEW
└── Persistence/OVT_TEST_PersistenceRoundTripSuite.c   + 1 helicopter-ledger case
```

### 3.2 The rearm plan — `OVT_VehicleRearmUtils`

Discovery is unchanged: `GetRearmableWeapons` (`:59`) walks the vehicle's `SlotManagerComponent`, collecting refillable `BaseMagazineComponent`s and rocket-pod entities. The new part is turning that into a **unit plan** — a list of `(prefab, units)` pairs plus a total.

```
class OVT_RearmUnit : Managed
{
    string m_sRes;      // prefab ResourceName; "" means "this weapon can only be bought"
    int    m_iUnits;    // 1 per deficient gun magazine, 1 per reloadable rocket barrel
}
```

`BuildPlan(IEntity vehicle, out array<ref OVT_RearmUnit> plan, out int totalUnits)`:

- **Gun magazine below `GetMaxAmmoCount()`** → 1 unit, prefab from `OVT_VehicleRearmRules.ResolveAmmoPrefab(muzzleDefault, loadedPrefab, "")`.
- **Rocket pod** → one unit per barrel where `CanReloadBarrel(i)` (`RocketEjectorMuzzleComponent.c:15`), prefab from `SCR_RocketEjectorMuzzleComponent.GetDefaultRocketPrefab()` (`:13-16`, already used at `OVT_VehicleRearmUtils.c:234`).
- A weapon whose prefab resolves empty still contributes its units to `totalUnits` — it is simply never coverable, so it is always part of the money remainder. That is R2's "rocket pods that have no magazine prefab stay money-only", generalised.

`GetRearmableWeapons` must additionally hand back each magazine's owning `BaseMuzzleComponent` so `GetDefaultMagazineOrProjectileName()` can be asked. `CollectWeapon` (`:194`) already holds the `MuzzleComponent` in a local at `:203` — the change is to keep it, not to find it again.

`PerformRearm` (`:148`) is **unchanged** and still restocks everything. Sourcing and pricing never decide how much ammunition appears; they decide who pays.

### 3.3 Sourcing — one collector, two shipped list operations

```
OVT_StorageUtils.CollectStores(vector pos, float radius, int playerId, out array<OVT_StorageComponent> stores)
```

Runs one `OVT_StorageHolderQuery` (`OVT_StorageUtils.c:148`, per-call accumulator), keeps every holder that passes `PlayerMayDrawFrom`, and returns their components in query order. It is deliberately **not** `OVT_WarehouseStockUtils.CollectStores` (`:30`), whose registered-warehouse filter is the whole point of that function and would exclude the truck the ammunition arrived in.

```
OVT_StorageUtils.PlayerMayDrawFrom(int playerId, IEntity holder)
```

The three shipped, public clauses of `OVT_StorageRequestComponent.MayUseHolder` that are not about distance, in the same order (`:3042`, `:3048`, `:3054`):
`OVT_ControllerRequestComponent.PlayerMayUseVehicleFor` → `OVT_RealEstateManagerComponent.PlayerMayUseWarehouse` → `OVT_StructureDamage.IsUsable`. A locked vehicle, a private warehouse and a ruined building are all off limits to a rearm exactly as they are to a Take.

The vehicle's own storage is put at the front with `OVT_WarehouseStockUtils.PrependStore` (`:73`) — that function's header already documents "position is priority", which is the semantic we want: burn your own load before your neighbour's.

Counting and draining are `OVT_WarehouseStockUtils.CountAvailable` (`:89`) and `TakeUpTo` (`:124`), unmodified. They take a plain `array<OVT_StorageComponent>` and know nothing about warehouses; `TakeUpTo` is already `Replication.IsServer()`-gated and already calls `PublishCount()` once per store that gave something up.

**Radius:** `OVT_StorageRequestComponent.GetHolderRadius()` (`:3222`; the `m_fHolderRadius` attribute defaults to 25 m and is authored on `Prefabs/GameMode/OVT_OverthrowController.et` — see the declaration and its comment at `OVT_StorageRequestComponent.c:209-213`). The rearm reads it off the sibling component on the same controller entity rather than declaring a second number — one authored value, one behaviour, no drift with the destination picker the player used to load the ammunition.

### 3.4 The gate — `IsAtRearmSite`, and when it applies

`OVT_VehicleRearmUtils.IsOnHelipadAtFriendlyBase(vector)` (`:175`) becomes `IsAtRearmSite(vector)`, same instance-method shape (the world query reports through a member callback, so it stays `new`-ed per call):

1. **Built Helipad or built Garage within `SITE_SEARCH_RADIUS` (20 m)** whose `OVT_BuildableComponent.GetBuildableType()` is `"Helipad"` or `"VehicleGarage"`, **and** that passes `OVT_StructureDamage.IsUsable` (a ruined garage supplies nothing), **and** the nearest base is not the occupying faction's (`OVT_OccupyingFactionManager.GetNearestBase` + `IsOccupyingFaction`, unchanged from `:180-181`); **or**
2. **Within 100 m of a deployed FOB** — `OVT_ResistanceFactionManager.GetNearestFOBData(pos)` (`:1533`) and the shipped `MAX_FOB_PLACE_DIS` constant (`OVT_SleepService.c:58`, `OVT_ItemLimitChecker.c:15`, `OVT_PlaceContext.c:29` — all three already hold 100).

The radius goes 15 → 20 m because a Garage is a building, not a landing box; the helipad's own parking box is 16 m wide (`OVT_VehicleRearmUtils.c:24-27`).

Both clauses are answerable on a client: the sphere query is local, base records replicate, and FOB records reach clients through `RpcDo_RegisterFOB` (`OVT_ResistanceFactionManager.c:1468`) and the JIP stream (`:1578`, `:1618`). So the client and the server run the **same function** and cannot disagree about the site — only about coverage, which is why coverage is quoted rather than computed.

**The gate applies only when `cost > 0`.** Fully covered rearms are performable anywhere.

### 3.5 The wire — quote out, rearm in

Contents never leave the server (`OVT_StorageComponent.c:35-38`), so a client physically cannot answer "does my truck hold enough 25 mm?". The action therefore asks, once per cache window, and renders the answer.

| RPC | Direction | Args | Arity |
|---|---|---|---|
| `RpcAsk_RearmQuote` | `RplRcver.Server` | `RplId vehicleId` | **1** |
| `RpcDo_RearmQuote` | `RplRcver.Owner` | `RplId vehicleId, int totalUnits, int coveredUnits, int cost` | **4** |
| `RpcAsk_RearmVehicle` | `RplRcver.Server` | `RplId vehicleId` | **1** (unchanged) |

Three RPCs, three arities, hand-audited — `Rpc()` is untyped variadic and a wrong count compiles clean and dies silently at the wire (BUG-090).

- The authority never loops an `RplRcver.Server` RPC back to itself (BUG-164), so both `Ask` wrappers keep the shipped `if(Replication.IsServer()) Handler(...) else Rpc(Handler, ...)` shape (`OVT_ShopTransactionComponent.c:484-489`).
- The owner reply mirrors `SendSellResult` (`:1065-1074`) exactly: `if(ShouldRespondLocally(playerId)) { RpcDo_RearmQuote(...); return; } Rpc(RpcDo_RearmQuote, ...);` — an Owner-targeted RPC to ourselves on a listen host is never delivered.
- `RpcDo_RearmQuote` fires `m_OnRearmQuote` (`RplId, int, int, int`) and **mutates nothing**. Same contract as `RpcDo_SellResult` (`:550-555`): losing this packet costs a label, never a transaction.
- The quote carries the vehicle's `RplId` so an action instance can ignore a quote for a different vehicle. Two armed vehicles parked together are the ordinary case.

**Server handler order in `RpcAsk_RearmVehicle`** — read first, gate, then consume, then perform, then charge. Nothing is ever put back, because nothing is taken before every refusal has been evaluated:

1. `Replication.IsServer()`
2. `playerId = ResolveOwningPlayerId()` > 0 — never from the payload
3. the player has a controlled character
4. the vehicle resolves from its `RplId`
5. the character is within `VEHICLE_MAX_DISTANCE` (`:57`, 15 m)
6. `NeedsRearm(vehicle)` — something is actually missing
7. `BuildPlan` → `(plan, totalUnits)`
8. `CollectStores` at `GetHolderRadius()`, own storage prepended
9. `covered = Σ min(unit.m_iUnits, CountAvailable(stores, unit.m_sRes))` — a **read**
10. `cost = OVT_VehicleRearmRules.ProratedCost(GetRearmCost(), totalUnits - covered, totalUnits)`
11. **if `cost > 0`:** `IsAtRearmSite(vehicle.GetOrigin())` else notify `"RearmNeedsSupplyPoint"` and return; `PlayerHasMoney(persId, cost)` else notify `"CannotAfford"` and return
12. `TakeUpTo(stores, unit.m_sRes, unit.m_iUnits)` per plan line → `taken`
13. `PerformRearm(vehicle)` — always a **full** restock
14. `if (cost > 0) TakePlayerMoney(playerId, cost)` — the quoted figure
15. notify `"Rearmed"` with `taken` and `cost`

Steps 9 and 12 run in one synchronous server frame against the same ledgers, so `taken == covered` by construction. The player is charged the number they were shown; the only reachable divergence would charge *less* than the work done, never more, and never destroys stock.

### 3.6 The hidden-item rule

`OVT_PrefabUtils.IsItemHiddenInInventory(ResourceName)` — true only when an authored `m_bVisible 0` is found. It walks the prefab chain the way `GetItemUIInfo` (`:77-107`) does, and for the reason that function documents at `:72-74`: a delta that authors no `InventoryItemComponent` of its own resolves nothing from its own source, so the ancestry walk is load-bearing, not defensive.

```
Resource holder            = Resource.Load(current);                                   // held in a LOCAL for the whole read
IEntitySource entitySource = SCR_BaseContainerTools.FindEntitySource(holder);          // SCR_BaseContainerTools.c:123
  -> if (!entitySource || entitySource.GetComponentCount() == 0) return false;         // fail OPEN, do not cache
IEntityComponentSource item = <the component whose class IsInherited(InventoryItemComponent)>
BaseContainer attrs        = item.GetObject("Attributes");                             // BaseContainer.c:44
bool visible;
bool authored              = attrs.Get("m_bVisible", visible);                          // BaseContainer.c:37
  -> if (authored) return OVT_StorageRules.HiddenFromInventory(true, visible);
  -> else climb to entitySource.GetAncestor().GetResourceName() and repeat, bounded at 16
```

Three traps, all closed by the shipped precedent:

- **`Resource` must live in a local for the whole read** (`OVT_PrefabUtils.c:84-85`) — a temporary can be evicted out from under the `IEntitySource` it produced, which reads back as a source with zero components.
- **A zero-component source is UNKNOWN, not "visible"** — it returns `false` (allow) and is **not cached**, so a poisoned entry cannot outlive the frame that produced it.
- **`Get` reads *this* level only.** `m_bVisible` is unauthored on the overwhelming majority of prefabs (its `[Attribute("1")]` default lives at `SCR_ItemAttributeCollection.c:16`); only 19 files in the vanilla tree author it at all. Absent everywhere in the chain → visible. The `Box_25x137_M242_*` family authors it on the `_Base` prefabs (`Box_25x137_M242_150rnd_HEIT_Base.et:33`, `Box_25x137_M242_60rnd_APDST_base.et:31`), which is exactly why the walk cannot stop at the concrete variant.

The consumer is one early-continue at the top of `StepToInventory`'s loop (`OVT_StorageRequestComponent.c:1871-1920`), before the `ResolveHolderStorage`/`TrySpawnPrefabToStorage` pair at `:1895-1902`:

```
if (OVT_PrefabUtils.IsItemHiddenInInventory(res))
{
    job.m_iShortfall += wanted;
    job.DropFrontLine();
    continue;
}
```

`ledger.Take` is never reached, so **the line stays**. That is R3 exactly, and it is the only ledger→inventory exit in the mod — port import credits without spawning, Export debits for money, loot and the sweep run the other way.

⚠ **Consequence to state plainly, because it is a deliberate one-way door:** a hidden item can no longer be pulled into a hand inventory by any player. Its remaining exits are another holder's ledger, port Export, and being eaten by a rearm. Existing trapped boxes in a live save are the officer "Clear inventory" action's problem — no migration, per R3.

### 3.7 Capacity

`OVT_StorageRules.ResolveAutoCapacity` gains a sixth parameter and one changed branch:

```
static int ResolveAutoCapacity(bool isVehicle, bool isRegistered, bool isLegalVehicle,
                               OVT_ParkingType parking, int defaultVehicleCapacity,
                               int armedVehicleCapacity)
...
    if (!isLegalVehicle) return armedVehicleCapacity;    // was: return 0
```

Two production call sites (`OVT_StorageComponent.c:420`, `:463`) and one Logic case. The new value comes from `[Attribute("100", desc: "Item capacity an AUTO-resolved registered but illegal or armed vehicle gets")] protected int m_iArmedVehicleCapacity;` authored beside `m_iAutoVehicleCapacity` (`:69-70`), so it is one Workbench field to retune on `Wheeled_Base.et` and on `Helicopter_Base.et`.

**Unregistered still means 0.** Only the *illegal* branch moves; granting storage to a vehicle prefab the economy does not know remains the worse failure (`OVT_StorageRules.c:18-20`).

**Downstream, deliberately:** capacity 0 is what `OVT_StorageHolderQuery.FilterHolders` (`:195`) and `MayUseHolder` (`:3030`) read as "not a holder". Once an armed vehicle resolves 100, it **appears in every destination picker and accepts Take** — which is how the player gets 25 mm boxes into the LAV in the first place. That is the intent, and it is a visible behaviour change that inverts a shipped assertion (see §4 Phase 3).

### 3.8 Prefabs

**Context reachability is the sharpest edge in this feature**, and it is not guessable — the three target APCs share no context name:

| Vehicle | Contexts that exist | file:line |
|---|---|---|
| LAV-25 | `door_back_left`, `door_back_right`, `door_side_left`, `hatch_driver`, `hatch_back_left/right`, `hatch_side_left`, `turret_01`, `push_front/rear` | `ArmaReforger/Prefabs/Vehicles/Wheeled/LAV25/LAV25_base.et:454,503,1842,1277,1297,1304,1311,1887,1817,1829` |
| BTR-70 | `door_l01`, `door_r01`, `driver_hatch`, `codriver_hatch` | `.../BTR70/BTR70_base.et:291,337,383,423` |
| BRDM-2 | `hatch_driver`, `hatch_commander`, `turret_01`, `Passenger` | `.../BRDM2/BRDM2_base.et:344,398,1356,596` |
| UH-1H | `door_r01`, `door_l01`, `door_l03`, `door_r03`, + `heli_repair_point` (inherited) | `.../UH1H/UH1H_base.et:1618,1551,1586,1653`; `ArmaReforger/Prefabs/Vehicles/Core/Helicopter_Base.et:136` |
| Mi-8 | `door_l01` **only**, `door_back_l/r`, + `heli_repair_point` (inherited) | `.../Mi8MT/Mi8_base.et:1691,1735,1745`; `Helicopter_Base.et:136` |

Vanilla `Vehicle_Base.et` and `Wheeled_APC_Base.et` declare **no** `ActionContexts` at all; `Wheeled_Base.et` declares only `Unflipper` (`:252`). There is no universal context to hang anything on.

Two facts settle the approach:

- **A child's `ActionContexts` array merges with its ancestors'.** Proven by shipped behaviour, not by inference: `heli_repair_point` is declared on `Helicopter_Base.et:136`, `UH1H_base.et` declares its own large array, and today's Re-arm action — authored solely on `heli_repair_point` — works on the UH-1H. (`resources/context.md:469` left this "unproven"; it is now proven.)
- **A `ParentContextList` entry naming a context a given vehicle lacks is harmless.** `Vehicle_Base.et:32` already lists `door_rear`, which most vehicles do not have, and `:80` lists `fuel_cap`, which many lack.

So: **widen the lists, mint no context.**

**`Prefabs/Vehicles/Core/Vehicle_Base.et`** (`ActionsManagerComponent {C97BE5489221AE18}` — the *inherited* GUID, re-declared, never a fresh one; a duplicate actions manager silently kills later components):

1. Add `OVT_RearmVehicleAction`, GUID moved verbatim from `Helicopter_Base.et:10` (`{6A9F4C2DB1E07A31}`, UIInfo `{6A9F4C2DB1E07A32}`), `Duration 5`, `CanAggregate 1`, `VisibilityRange -1`, `"Sort Priority" 105` (after Rename's 104, `:76`). `ParentContextList`:
   `"heli_repair_point" "door_r01" "door_l01" "door_rear" "door_back_left" "door_back_right" "hatch_commander"`
2. Append `"door_back_left" "door_back_right" "hatch_commander"` to the `ParentContextList` of `OVT_OpenStorageMenuAction` (`:32`), `OVT_TransferAllToStorageAction` (`:56`) and `OVT_RenameStorageAction` (`:68`), so the LAV-25's and BRDM-2's brand-new 100-item ledgers are openable at all. **Zero overlap** with the existing three names on any vehicle in the table above, so no vehicle shows a duplicated action.
   ⚠ **`door_back_left`/`door_back_right` are the LAV-25's spelling. The Mi-8's rear doors are `door_back_l`/`door_back_r`** (`Mi8_base.et:1735,1745`) — different names, which is exactly why there is no overlap and exactly the pair a typo would collapse.
   `OVT_SellVehicleCargoAction` (`:44`) is deliberately **not** widened — selling an APC's cargo at a shop is a separate feature's decision and this plan does not make it.

**`Prefabs/Vehicles/Core/Helicopter_Base.et`** (same-GUID delta `{CBD8C2393BE87581}` of vanilla `Prefabs/Vehicles/Core/Helicopter_Base.et`, proven by `UH1H_base.et:1` and `Mi8_base.et:1` both naming that GUID):

3. **Remove** the `OVT_RearmVehicleAction` block (`:10-21`) — it now arrives from `Vehicle_Base.et`. Leaving it would double the action at the nose, since `heli_repair_point` sits at offset 0.
4. **Add** `OVT_StorageComponent` with a fresh GUID from the `6BA1C4E0…` series, `m_eCapacityMode AUTO`, `m_iAutoVehicleCapacity 300`, `m_iArmedVehicleCapacity 100`. Armed helicopters are `illegal 1` by the `vehiclePrices.conf` default (`OVT_VehiclePricesConfig.c:23-24`) → 100; unarmed civil variants forced legal by the CIV branch (`OVT_EconomyManagerComponent.c:1800-1802`) → 300.
   After (3) the `ActionsManagerComponent` block is empty; dropping it or leaving a bare re-declaration of `{C97BE5489221AE18}` are both inert — what must **not** happen is a fresh GUID.

**`Prefabs/Vehicles/Helicopters/Mi8MT/Mi8MT_unarmed_civ_base.et`** — NEW same-GUID delta:

5. `.et.meta` `Name "{366EA0B41474A7F8}Prefabs/Vehicles/Helicopters/Mi8MT/Mi8MT_unarmed_civ_base.et"` (GUID proven from `Mi8MT_unarmed_civ_blue.et:1` and `_red.et:1`, both of which name it as parent), five `EntityTemplateResourceClass` configurations copied from `Wheeled_Truck_Base.et.meta`.
6. Header **byte-identical to vanilla's**: `Vehicle : "{B6B8C164FD6377EA}Prefabs/Vehicles/Helicopters/Mi8MT/Mi8MT_unarmed_base.et" {` and `ID "57DA6675519A417B"`.
7. Body: **re-declare the inherited `OVT_StorageComponent` GUID** from step 4 with `m_eCapacityMode UNLIMITED`. A fresh GUID here would add a *second* storage component to the same entity — the same class of defect as a duplicate `ActionsManagerComponent`.
8. Overthrow currently owns **no** file under `Prefabs/Vehicles/Helicopters/` and does not reference `{B6B8C164FD6377EA}` anywhere, so this is a clean first delta in that folder.

**`Configs/Pricing/vehiclePrices.conf`:**

9. New `OVT_VehiclePriceConfig` with a fresh GUID, `m_sFind "Mi8MT_unarmed_civ"`, `cost 90000`, `illegal 0`, `parking PARKING_HELI`. Selection is longest-`m_sFind`-wins (`OVT_EconomyManagerComponent.c:1775-1795`), so 17 characters beat the existing `"Mi8MT"` entry's 5 (`:33-37`) regardless of list order. The two concrete prefabs it must catch are in the CIV catalogue at `ArmaReforger/Configs/EntityCatalog/CIV/Vehicles_EntityCatalog_CIV.conf:405,412`.
    ⚠ It must **not** contain any of the seven `hidden 1` substrings (`_Conflict`, `_Arsenal`, `_engineer`, `_command`, `_arsenal`, `_ammo`, `_repair`) — a `hidden` match `break`s out of the loop before specificity is consulted (`:1786-1789`). `"Mi8MT_unarmed_civ"` contains none.

### 3.9 Persistence

Wheeled vehicles already persist their ledger: `OVT_StorageComponentSerializer {6B0E7A60C1D2E3F4}` is bound on the CAR `EntityPersistenceConfig {64C6B4937723DA61}` (`Configs/Systems/Persistence/Overthrow.conf:126`). The LAV, BTR and BRDM are covered the moment their capacity is non-zero — no persistence change for R4's wheeled half.

**Helicopters are not.** `EntityPersistenceConfig {64EE8D74EB8192BA}` (`:132-154`) lists `HelicopterControllerComponentSerializer`, fuel, hit zones, slots and `OVT_PlayerOwnerComponentSerializer` — and **no** `OVT_StorageComponentSerializer`. Adding `OVT_StorageComponent` to `Helicopter_Base.et` without adding the binding ships a storage that silently empties on every save/load, which is worse than no storage at all.

One new binding, GUID `{6B0E7A63F4051627}` (verified 0 hits in both trees), from the project's `6B0E7A7…`/`6B0E7A6…` persistence-binding series:

```
EntityPersistenceConfig "{64EE8D74EB8192BA}" {
 ComponentSerializers {
  ...
  OVT_PlayerOwnerComponentSerializer "{6B0E7A31B2C3D4E5}" { }
  OVT_StorageComponentSerializer "{6B0E7A63F4051627}" { }
 }
}
```

No serializer **code** changes, no version bump: `OVT_StorageComponentSerializer` is class-bound and already writes the ledger and the custom name for any holder it is attached to. This is a binding addition only — but it is a persistence-config edit, so every phase that touches it gates on the **All** group.

### 3.10 Replication summary

| Value | Mechanism | JIP |
|---|---|---|
| Armed-vehicle capacity | existing `m_iCapacity` `RplProp` (`OVT_StorageComponent.c:91-92`), resolved once on the server | free |
| Ledger contents | **never replicated** — server-side only, unchanged | n/a |
| Rearm quote | `RpcDo_RearmQuote`, reliable, to the asking owner only, display-only | n/a — re-asked on the next cache window |
| Site (helipad/garage/FOB/base) | already-replicated base and FOB records + a local sphere query | free |
| Rearm outcome | ammunition state travels as the engine's own magazine/barrel replication, as today | free |

Nothing new is added to the streamed state. One player looking at one armed vehicle costs one small reliable RPC pair per cache window; nobody else pays anything.

---

## 4. Implementation Phases

Every phase: `tools/compile-check.sh` exit 0 before hand-back. **`tools/run-tests.sh` is the orchestrator's**, run once after a phase completes — never inside an agent, never during planning (`.claude/test-policy.md`). Group per phase is **Fast** `{6A6E29FF47ECB840}` or **All** `{6A6E2A002F53A581}`; All is required whenever `Configs/Systems/Persistence/` or campaign/economy state is touched. Re-baseline (`git pull` / `git status`) before every phase — concurrent sessions share this tree.

### Phase 1 — Pure rules + Logic cases
**Estimate:** 2–3 h · **Agent:** `component-developer` · **Tests: Fast**

1. `Scripts/Game/Data/OVT_VehicleRearmRules.c` — NEW, two statics:
   - `ResolveAmmoPrefab(string muzzleDefault, string loadedPrefab, string rocketPrefab)` → first non-empty in that order, `""` when all three are empty.
   - `ProratedCost(int fullCost, int uncoveredUnits, int totalUnits)` → `0` when `totalUnits <= 0` or `uncoveredUnits <= 0`; `fullCost` when `uncoveredUnits >= totalUnits`; otherwise `Math.Round(fullCost * uncoveredUnits / (float)totalUnits)` **clamped to a minimum of 1** so a rounding-down never gives ammunition away free.
2. `Scripts/Game/Data/OVT_StorageRules.c` — `ResolveAutoCapacity` gains `int armedVehicleCapacity`; the `!isLegalVehicle` branch (`:35-36`) returns it. Add `HiddenFromInventory(bool authoredFound, bool authoredVisible)` → `authoredFound && !authoredVisible`.
3. `Scripts/Game/Components/OVT_StorageComponent.c` — new `m_iArmedVehicleCapacity` attribute, threaded into both call sites (`:420`, `:463`).
4. `Tests/TestSuites/Logic/OVT_TEST_Logic_StorageRules.c` — update the six existing `ResolveAutoCapacity` assertions (`:21,29,37,44,51,59`) for the sixth argument; **change the illegal assertion at `:37` from 0 to the supplied armed cap**, and add one asserting the armed cap is the *caller's* number (pass 7, expect 7) so a constant baked into the rule cannot pass. Add a `HiddenFromInventory` case covering all three input shapes — `(true,false)` hidden, `(true,true)` visible, `(false,anything)` **visible**.
5. `Tests/TestSuites/Logic/OVT_TEST_Logic_VehicleRearm.c` — NEW: prefab-resolution order incl. all-empty; pro-rata price at 0 %, 50 %, 100 %, `total == 0`, and the round-to-zero clamp.

**Acceptance**
- No case references a manager, the game mode or the world, and the identifiers for Overthrow's static manager accessor and the engine's game-mode getter appear nowhere under `TestSuites/Logic/`, not even in a comment.
- Every new case proven able to fail once; the mutation and message recorded in `context.md`.
- `ProratedCost` is asserted to be an **int** path with an explicit float divisor — an int/int expression here truncates and would silently under-charge.
- Compile-check exit 0.

### Phase 2 — Hidden items never leave a ledger ⚠️ ADVANCED AGENT
**Estimate:** 3–4 h · **Agent:** `component-developer-advanced` · **Tests: Fast**

1. `Scripts/Game/Utilities/OVT_PrefabUtils.c` — `IsItemHiddenInInventory(ResourceName)` per §3.6: bounded 16-deep ancestry walk, `Resource` in a local, zero-component source → `false` **and no cache write**, static `map<ResourceName,bool>` cache otherwise.
2. `Scripts/Game/Components/Controller/OVT_StorageRequestComponent.c` — the early-continue in `StepToInventory` (`:1871-1892`, before `ResolveHolderStorage` at `:1895`).
3. `Tests/TestSuites/Init/OVT_TEST_Init_VehicleRearmSeam.c` — NEW file, first two cases:
   - `AHiddenPrefabIsDetected`: `Box_25x137_M242_150rnd_HEIT.et` (the **concrete** variant, resolved from the vanilla path, not the `_Base`) reads hidden; a common rifle magazine reads visible; a garbage `ResourceName` reads visible (fail-open).
   - `BHiddenLineSurvivesTakeToInventory`: credit a hidden prefab into a spawned holder's ledger, run a `TO_INVENTORY` job for it, assert the ledger count is unchanged and the job's shortfall is the requested quantity.

**Acceptance**
- The concrete-variant case is mandatory: `BaseContainer.Get` reads one level only, and a case written against the `_Base` prefab would pass while the shipped path failed.
- The cache is never written from a zero-component read — assert by calling the function once before any prefab is spawned and once after, expecting the same answer.
- No other ledger→inventory path exists; `grep -n "TrySpawnPrefabToStorage" Scripts/Game/Components/Controller/OVT_StorageRequestComponent.c` shows the one call site the guard precedes.
- Compile-check exit 0.

### Phase 3 — Armed-vehicle capacity, helicopter storage, prefab moves ⚠️ ADVANCED AGENT
**Estimate:** 4–6 h · **Agent:** `component-developer-advanced` · **Tests: All**

1. `Prefabs/Vehicles/Core/Vehicle_Base.et` — §3.8 steps 1–2.
2. `Prefabs/Vehicles/Core/Helicopter_Base.et` — §3.8 steps 3–4.
3. `Configs/Systems/Persistence/Overthrow.conf` — §3.9, one binding on `{64EE8D74EB8192BA}`.
4. `Tests/TestSuites/Init/OVT_TEST_Init_StorageSeam.c`:
   - **Invert case E** (`:564-672`). It currently asserts an illegal vehicle resolves 0 *and is excluded from the holder query* — both claims become false. Rename to `…_EArmedVehicleHasSmallStorage` (the `E` keeps its alphabetical slot), assert `100` and assert it **is** offered by `OVT_StorageHolderQuery`. Rewrite the failure messages: the old one names a rule that no longer exists.
   - New case F: a spawned armed helicopter from the economy's own catalogue resolves `100` through a component that exists at all — the only automated proof `Helicopter_Base.et` kept its block.
5. `Tests/TestSuites/Persistence/OVT_TEST_PersistenceRoundTripSuite.c` — one helicopter-ledger round-trip modelled on `…_VehicleReserveRelease_KeepsOwnerAndContents` (`:2076`). Class name **must sort after** `…_Capability_…` (`:361`).

**Acceptance**
- `ActionsManagerComponent` on `Vehicle_Base.et` keeps GUID `{C97BE5489221AE18}`; no fresh actions-manager GUID is minted anywhere.
- The moved `OVT_RearmVehicleAction` keeps GUIDs `{6A9F4C2DB1E07A31}`/`{6A9F4C2DB1E07A32}` and appears in exactly **one** prefab (`grep -rln OVT_RearmVehicleAction Prefabs/` → 1 file).
- The three widened storage `ParentContextList`s add exactly three names each and no vehicle in §3.8's table gains a duplicate action.
- Fresh GUIDs come from `6BA1C4E0…` (verified 0 hits in both trees before authoring); the persistence binding from `{6B0E7A63F4051627}`.
- Compile-check exit 0.

### Phase 4 — Ledger-first rearm, server side ⚠️ ADVANCED AGENT
**Estimate:** 6–8 h · **Agent:** `component-developer-advanced` · **Tests: All**

1. `Scripts/Game/Utilities/OVT_StorageUtils.c` — `CollectStores` and `PlayerMayDrawFrom` per §3.3.
2. `Scripts/Game/Utilities/OVT_WarehouseStockUtils.c` — **header comment only**: record that `PrependStore`/`CountAvailable`/`TakeUpTo` are holder-agnostic list operations now shared with vehicle rearm. No body changes (D5).
3. `Scripts/Game/Utilities/OVT_VehicleRearmUtils.c` — `OVT_RearmUnit`, `BuildPlan`, `QuoteRearm(IEntity vehicle, int playerId, out int totalUnits, out int coveredUnits, out int cost)`, `IsAtRearmSite` per §3.2/§3.4. `GetRearmableWeapons` hands back muzzles alongside magazines. `PerformRearm` unchanged.
4. `Scripts/Game/Components/Controller/OVT_ShopTransactionComponent.c` — `RpcAsk_RearmVehicle` rewritten to §3.5's fifteen steps.
5. `Configs/overthrowBroadcastMessages.conf` — `"Rearmed"` and `"RearmNeedsSupplyPoint"` presets; `.st` keys land in Phase 7 but the tags are wired here.
6. `Tests/TestSuites/Init/OVT_TEST_Init_VehicleRearmSeam.c` — cases C–E:
   - `CPlanCountsUnitsPerWeapon`: a spawned armed vehicle's plan has a non-empty prefab for at least one gun magazine and a total ≥ 1.
   - `DQuoteIsFreeWhenCovered`: credit the vehicle's own ledger with the planned prefab at the planned quantity, assert `cost == 0`; empty it, assert `cost == GetRearmCost()`.
   - `EQuoteIsProRata`: credit half the units, assert the cost lands strictly between 0 and the full price.

**Acceptance**
- `QuoteRearm` is a pure **read** — no `Take`, no `SetAmmoCount`, no `BumpMe` — proven by grep over its body. Both the quote RPC and the rearm handler call it.
- The site test and the funds test both run **before** the first `TakeUpTo`; no rollback path exists because none is reachable.
- The charged figure is the quoted figure; the toast reports `taken` and `cost` separately.
- `IsAtRearmSite` calls `OVT_StructureDamage.IsUsable` and does not modify any `core/damage` file.
- One new sphere-query object per call; nothing static, nothing shared (`OVT_StorageUtils.c:141-147`).
- Compile-check exit 0.

### Phase 5 — Quote wire + client gate ⚠️ ADVANCED AGENT
**Estimate:** 4–5 h · **Agent:** `network-specialist-advanced` · **Tests: All**

1. `OVT_ShopTransactionComponent.c` — `RequestRearmQuote(IEntity)`, `RpcAsk_RearmQuote(RplId)`, `RpcDo_RearmQuote(RplId,int,int,int)`, `m_OnRearmQuote`, and a `SendRearmQuote` helper mirroring `SendSellResult` (`:1065-1074`).
2. `Scripts/Game/UserActions/OVT_RearmVehicleAction.c` — rewritten:
   - `CHECK_TTL_MS` 1000 → **2000** (the window now costs an RPC).
   - `RefreshCache` recomputes armed / needs-rearm / at-site locally and fires `RequestRearmQuote` when armed **and** something is missing.
   - Quote listener stores `(total, covered, cost)` only when the quote's `RplId` matches this owner's.
   - `CanBeShownScript` → armed (unchanged).
   - `CanBePerformedScript`: `!needsRearm` → `#OVT-NothingToRearm`; **no quote yet** → assume nothing is covered, i.e. today's rule, so the pre-quote state can only be more restrictive than the truth; `cost > 0 && !atSite` → **`#OVT-Rearm_NeedsSupplyPoint`**; `cost > 0 && !LocalPlayerHasMoney(cost)` → `#OVT-CannotAfford`; else true.
   - `GetActionNameScript`: `cost > 0` → `"#OVT-RearmVehicle ($N)"` (existing shape, `:100`); `cost == 0` with a quote in hand → `"#OVT-RearmVehicle_FromStorage"`.
   - `#OVT-MustBeOnHelipad` loses its only reference. The `.st` entry (`localization_Overthrow.st:13676`) is **left in place** — removing entries is an export hazard for no gain — and noted as a tidy-up candidate.
3. `Tests/TestSuites/Init/OVT_TEST_Init_VehicleRearmSeam.c` — case F: the quote RPC pair resolves on the local player's `OVT_ShopTransactionComponent` and the invoker fires once for a spawned armed vehicle (the SP/listen-host `ShouldRespondLocally` branch).

**Acceptance**
- **RPC arity table in `context.md`** covering all three RPCs against their handlers (§3.5). No `Rpc()` call is wrapped in a helper that hides its argument list.
- No `array<...>` on any RPC.
- `RpcDo_RearmQuote` mutates nothing — grep its body for `Take`, `Set`, `Bump`.
- The pre-quote fallback is proven strictly conservative: with no quote the action is never *more* permissive than after one arrives.
- Compile-check exit 0.

### Phase 6 — The unlimited civilian Mi-8
**Estimate:** 2 h · **Agent:** `component-developer` · **Tests: All**

1. New `Prefabs/Vehicles/Helicopters/Mi8MT/Mi8MT_unarmed_civ_base.et` + `.et.meta` — §3.8 steps 5–8.
2. `Configs/Pricing/vehiclePrices.conf` — §3.8 step 9.
3. `Tests/TestSuites/Init/OVT_TEST_Init_StorageSeam.c` case G: the civilian Mi-8 resolves `EOVT_StorageCapacityMode.UNLIMITED` **and** capacity `-1`. Asserting the mode as well as the number is what stops a dropped attribute passing through the AUTO legal-heli branch (which answers 300, not −1 — so the number alone would actually catch it here; assert both anyway, for the same reason every other case in that file does).

**Acceptance**
- The `.et.meta` `Name` GUID is `{366EA0B41474A7F8}` byte-for-byte and the header parent line matches vanilla's `Mi8MT_unarmed_civ_base.et:1` exactly.
- The `OVT_StorageComponent` block re-declares the GUID minted on `Helicopter_Base.et` in Phase 3 — `grep -c` proves exactly one storage component GUID across the two files.
- `"Mi8MT_unarmed_civ"` contains none of the seven `hidden 1` substrings.
- Compile-check exit 0.

**Parallelism:** Phase 6 touches files no other phase touches after Phase 3 lands, and may run in parallel with Phases 4–5.

### Phase 7 — Localization, help & wiki sync
**Estimate:** 2–3 h · **Agent:** main thread + `help-docs-sync` · **Tests: skipped (docs/`.st` only — say so)**

1. `Language/localization_Overthrow.st` — **master only, never the generated `Language/*.conf` exports**. Four keys:
   - `OVT-Rearm_NeedsSupplyPoint` — "Take it to a helipad, garage or FOB to buy the missing ammunition"
   - `OVT-RearmVehicle_FromStorage` — "Re-arm (from storage)"
   - `OVT-Msg-Rearmed` — the toast, two parameters: units drawn from storage, dollars paid
   - `OVT-Msg-RearmNeedsSupplyPoint` — the server-side refusal
   Record the `grep -c 'Id "OVT-'` count before and after (**1231** at planning time). Ask the user for a Workbench re-export; do not write the `.conf`s.
2. `help-docs-sync` — this feature changes what players see and do, so the closing phase is mandatory. It must cover: Re-arm now appears on APCs and armed jeeps, not just helicopters; it is free when you brought the ammunition; the supply points are helipad, garage and FOB; armed vehicles now have a small storage; vehicle-weapon ammunition cannot be taken into a hand inventory. Every sentence carries a `file:line` citation or is cut — two shipped help entries have already been fiction.

**Acceptance**
- `git status --porcelain Language/` shows only the `.st` master.
- `.st` braces balanced; entry count moves by exactly 4.
- No help sentence states a mechanic without a citation in this document or the code.

---

## 5. Key Technical Decisions

**D1 — The unit of accounting is one ledger item, and the price is pro rata.** One deficient gun magazine costs one magazine item; one reloadable rocket barrel costs one rocket. The money price is `GetRearmCost()` scaled by the fraction of units the ledgers could not cover, so a fully-money rearm costs **exactly what it costs today** and needs no new balance constant, while a fully-covered one is free. *Rejected:* (a) a flat per-weapon price — a UH-1H with two pods and two door guns would jump from $1000 to 4×; (b) pricing each magazine at `OVT_EconomyManagerComponent.GetBuyPriceForPrefab` (`:768`) — more "correct", but it makes the rearm bill vary with town stock and market drift for no gameplay gain, and it introduces a second thing that can quote `-1`. The pro-rata function is pure and Logic-tested, which is where a pricing rule belongs.

**D2 — The magazine prefab comes from the muzzle, not from the resupply action.** `BaseMuzzleComponent.GetDefaultMagazineOrProjectileName()` (`BaseMuzzleComponent.c:43`) is public, config-derived, has no runtime-initialisation dependency, and is **already** how Overthrow answers this exact question for AI groups (`OVT_HighCommandManagerComponent.c:3172`). *Deviation from requirements R2, flagged for the user:* R2 names `SCR_ResupplyVehicleWeaponSupportStationAction.m_sItemPrefab` first. That route is reachable — `SCR_BaseItemHolderSupportStationAction.GetItemPrefab()` is public at `SCR_BaseItemHolderSupportStationAction.c:86-89`, no `modded class` needed — but `m_sItemPrefab` is populated at runtime in `DelayedInit()` (`SCR_ResupplyVehicleWeaponSupportStationAction.c:58`) from the nested `m_ResupplyData`, and reading it means enumerating a *second* `ActionsManagerComponent` on each turret child entity and hoping it has initialised. The muzzle answers the same question from config, on the entity the plan already holds. **If a play-test shows a mismatch on any turret, adding `GetItemPrefab()` as a first probe is a five-line change inside `BuildPlan` and nothing else moves.**

**D3 — The gate follows the money, and the client is told the money by the server.** R1 (amended 2026-08-29) makes a covered rearm performable anywhere. Coverage is un-knowable on a client by construction — `OVT_StorageComponent`'s contents are server-only (`:35-38`) and the epic will not move them — so the alternative to a quote is a client gate that lies in one direction or the other. One reliable RPC pair per 2 s per player looking at one armed vehicle is cheaper than either lie. The quote is display-only; `RpcAsk_RearmVehicle` re-derives coverage, price, site and funds from scratch (§3.5), so a spoofed or stale quote can mislabel a button and nothing else. *Rejected:* replicating a per-vehicle "coverage" `RplProp` — it would have to recompute on every ledger change anywhere in a 25 m radius, for every armed vehicle in the world, forever.

**D4 — Widen `ParentContextList`s; mint no new context.** The three target APCs share no context name (§3.8's table), so "the door contexts" is not one thing. Seven names on one action cover LAV-25, BTR-70, BRDM-2, both helicopter families and the armed jeeps, with no overlap and therefore no duplicated action anywhere. Two facts make this safe: a child's `ActionContexts` array **merges** with its ancestors' (proven by the shipped `heli_repair_point` action working on the UH-1H, which declares its own array), and a `ParentContextList` naming an absent context is already harmless in this very file (`Vehicle_Base.et:32` lists `door_rear`, `:80` lists `fuel_cap`). *Rejected:* an Overthrow-owned `ovt_rearm_point` context on `Vehicle_Base.et` — it would reach every vehicle deterministically, but its radius would have to be right for a Humvee, an LAV and a Mi-8 at once, and a radius large enough for the last is large enough to fire from inside the first. **Fallback if a future armed vehicle has none of the seven:** mint that context then, for that case, with a measured radius.

**D5 — Reuse `OVT_WarehouseStockUtils`' three list operations where they are.** `PrependStore` (`:73`), `CountAvailable` (`:89`) and `TakeUpTo` (`:124`) take a plain `array<OVT_StorageComponent>` and contain no warehouse logic; `TakeUpTo` is already server-gated and already batches `PublishCount()` correctly. Moving them to `OVT_StorageUtils` would be tidier and would touch High Command's tested rearm path for a naming improvement. The class header gains a line saying it now has two callers. *Accepted cost:* the file name under-describes its contents. *Rejected:* copying the three bodies — two implementations of "drain ledgers in order" is exactly how the two halves of a filter drift apart.

**D6 — Nearby-holder access reuses the shipped permission clauses, not the shipped method.** `MayUseHolder` (`:3000-3061`) is `protected`, carries a `rejectKey` out-parameter and tests distance **from the caller**, none of which fits a per-store filter. `PlayerMayDrawFrom` is three public one-line calls in the same order (`:3042`, `:3048`, `:3054`) with no distance clause, because the radius is the collector's job. *Accepted cost:* two call sites for one rule. The risk is low — all three clauses are single delegations to shipped public bodies — and the alternative (refactoring the storage feature's most safety-critical gate to hand back per-clause keys) is a much larger blast radius for a smaller benefit.

**D7 — Hidden means "an ancestor authored `m_bVisible 0`"; everything else is visible.** The flag defaults to `1` (`SCR_ItemAttributeCollection.c:16`) and only 19 vanilla files author it, so the predicate fails **open** by construction: an unreadable prefab, a missing component source, a chain with no `InventoryItemComponent` — all visible. A fail-closed reading would make arbitrary items permanently un-takeable the first time a prefab source read badly, and `FindEntitySource` on an unloaded prefab is known to return a non-null source with zero components. The zero-component branch additionally refuses to write the cache, so one bad read cannot poison the session.

**D8 — Armed vehicles become real storage holders, with the picker consequences that follow.** Capacity 0 is the mod's "not a holder" sentinel, read by `OVT_StorageHolderQuery.FilterHolders` (`:195`) and `MayUseHolder` (`:3030`). Returning 100 instead means a captured BTR appears in every nearby destination picker and accepts Take — which is precisely how a player loads 25 mm boxes into a LAV, and precisely what `OVT_TEST_Init_StorageSeam_E…` currently asserts must never happen. That case is inverted, deliberately, in Phase 3. *Rejected:* a separate "ammunition-only" capacity axis — a second capacity model on the same ledger, for one use case, against the epic's "two ledgers, deliberately" grain.

**D9 — The storage component goes on `Helicopter_Base.et`, not `Vehicle_Base.et`.** Putting it one level higher would reach helicopters *and* every tracked, tracked-APC and boat class in one line, handing storage to vehicle families nobody has scoped, priced or play-tested. R4 names `Helicopter_Base.et`; the narrower seam is also the correct one. The APCs need no prefab change at all — they already inherit the component from `Wheeled_Base.et:8` and only the capacity rule was stopping them.

**D10 — The helicopter persistence binding is not optional.** `EntityPersistenceConfig {64EE8D74EB8192BA}` (`Overthrow.conf:132-154`) has no `OVT_StorageComponentSerializer`, so a helicopter ledger would empty on every save/load with no error anywhere. Shipping D9 without §3.9 would be worse than shipping neither. No serializer code changes and no version bump — the class-bound serializer already handles any holder.

**D11 — `OVT_MustBeOnHelipad` is superseded, not repurposed.** A key whose ID says "helipad" and whose text says "helipad, garage or FOB" is a trap for the next person grepping. A new key is added; the old entry stays in the `.st` unreferenced (deleting entries is an export hazard) and is recorded as a tidy-up candidate.

**D12 — No new controller component, no new manager.** The two new RPCs join `OVT_ShopTransactionComponent`, whose header already explains why the rearm purchase lives there (`:41-43`): money for ammunition is a shop transaction even though no shop entity is involved. Adding a component would move `OVT_TEST_Init_ControllerSeam.c`'s hard-coded roster count for one RPC pair. There is no system-wide state here — every rearm is one player, one vehicle, one frame.

---

## 6. Definition of Done

### Functional

- **F1** The Re-arm action appears on LAV-25, BTR-70, BRDM-2, the armed jeeps and every armed helicopter, and does **not** appear on unarmed vehicles.
- **F2** With enough matching ammunition in the vehicle's own ledger, Re-arm reads "Re-arm (from storage)", is performable **anywhere in the world**, costs $0, and empties exactly the units it used.
- **F3** With ammunition in a truck parked within the holder radius (25 m) and none in the vehicle, the same is true and the truck's count drops.
- **F4** With partial coverage, the label shows a price strictly between $0 and the full price, and performing it charges exactly that and consumes exactly the covered units.
- **F5** With no coverage and no supply point in range, the action is **shown and blocked** with a reason naming the missing ammunition — not "must be on a helipad".
- **F6** With no coverage, on a built Helipad or built Garage at a resistance-held base, or within 100 m of a deployed FOB, the action performs at the full difficulty-scaled price, exactly as it does today.
- **F7** A ruined garage/helipad does not count as a supply point.
- **F8** Every rearm restocks **every** weapon fully, regardless of how it was paid for.
- **F9** A toast names what was drawn from storage and what was paid.
- **F10** Open Storage → Take → **Inventory** on a truck holding 25 mm boxes moves nothing, reports the shortfall, and **leaves the boxes in the ledger**.
- **F11** Open Storage → Take → **the LAV-25** moves them, and the LAV then shows a non-zero storage count.
- **F12** An armed wheeled vehicle resolves 100 capacity and appears in nearby destination pickers; an unregistered vehicle still resolves 0.
- **F13** A helicopter has storage; an armed one holds 100, an unarmed civil one 300, and the civilian Mi-8 is unlimited.
- **F14** A helicopter's ledger and custom name survive save/continue.
- **F15** The civilian Mi-8 has its own price and is legal to buy at a civilian dealer; the armed Mi-8 keeps its 150000 price.

### Quality

- **Q1** `tools/compile-check.sh` exit 0 at every phase boundary.
- **Q2** Logic cases for both new rules and the changed capacity rule, each proven able to fail once, mutations recorded in `context.md`.
- **Q3** An RPC arity audit table in `context.md` covering all three rearm RPCs against their handlers.
- **Q4** No `array<...>` on any RPC; no `Rpc()` call wrapped in a helper.
- **Q5** `QuoteRearm` provably mutates nothing; the site and funds gates provably precede the first `TakeUpTo`.
- **Q6** Every sphere-query accumulator is per-call; nothing static, nothing shared.
- **Q7** No `Language/*.conf` export modified — `git status --porcelain Language/` lists only `localization_Overthrow.st`. (Note: `Configs/Language/` does **not** exist in this tree; checking it would be a vacuous gate, as `resource-production`'s Q7 was.)
- **Q8** `.st` braces balanced; `grep -c 'Id "OVT-'` moves from 1231 to 1235.
- **Q9** Comments sparse per `CLAUDE.md` — a line or two for a trap or a load-bearing ordering, never a rationale essay.
- **Q10** No `array.Remove()` introduced where order matters; `RandInt` is not used; no identifier named `owned` or `out`.
- **Q11** Fresh GUIDs verified unused in **both** trees before authoring; inherited component GUIDs copied, never minted.

### Integration

- **I1** `OVT_TransferContext.c` and both transfer models are **unmodified** — `git diff --exit-code` clean.
- **I2** No `core/damage` file is modified; `OVT_StructureDamage.IsUsable` is called, not changed.
- **I3** `OVT_WarehouseStockUtils.c` changes by comment only — `git diff` shows no body change, and `OVT_HighCommandManagerComponent.RearmGroup` is untouched.
- **I4** `OVT_StorageComponentSerializer.c` is unmodified; only its **binding** list grows.
- **I5** No new entry in `OVT_TEST_Init_ControllerSeam.c`'s controller roster, and its hard-coded count does not move.
- **I6** `OVT_ResourceStoreComponent` appears nowhere in this feature's diff.
- **I7** `OVT_SellVehicleCargoAction`'s `ParentContextList` is unchanged.

### Verification method — an independent evaluator can follow this

**Static (no game):**
1. `cd "/mnt/n/Projects/Arma 4/Overthrow.Arma4" && tools/compile-check.sh` → exit 0.
2. `grep -rln OVT_RearmVehicleAction Prefabs/` → exactly one file, `Prefabs/Vehicles/Core/Vehicle_Base.et`.
3. `grep -rn "MustBeOnHelipad" Scripts/` → no hits (the `.st` entry may remain).
4. `grep -n "OVT_StorageComponentSerializer" Configs/Systems/Persistence/Overthrow.conf` → five hits: CAR, HELICOPTER, Structures. *(count corrected 2026-09-01: placeable/buildable bindings pre-date this feature)*
5. `git diff --exit-code -- Scripts/Game/UI/Context/OVT_TransferContext.c Scripts/Game/Data/OVT_TransferListModel.c Scripts/Game/Data/OVT_TransferCartModel.c Scripts/Game/Persistence/Serializers/Components/OVT_StorageComponentSerializer.c` → clean.
6. `git diff -- Scripts/Game/Utilities/OVT_WarehouseStockUtils.c` → comment lines only.
7. `grep -c "Rpc(" Scripts/Game/Components/Controller/OVT_ShopTransactionComponent.c` matches the audit table in `context.md`, and each row's arity matches its handler.
8. `git status --porcelain Language/` → only `localization_Overthrow.st`; `grep -c 'Id "OVT-' Language/localization_Overthrow.st` → 1235.
9. `grep -rn "m_sFind \"Mi8MT" Configs/Pricing/vehiclePrices.conf` → two entries, the civ one 17 characters long.
10. Orchestrator only: `tools/run-tests.sh "{6A6E29FF47ECB840}"` (Fast) after Phases 1–2; `tools/run-tests.sh "{6A6E2A002F53A581}"` (All) after Phases 3, 4, 5 and 6. Announce the focus steal first.

**Workbench (user-gated):** open `Vehicle_Base.et`, `Helicopter_Base.et` and the new `Mi8MT_unarmed_civ_base.et` with no dropped-attribute warnings; open one child of each delta (`UH1H_base.et`, `Mi8MT_unarmed_civ_blue.et`) and confirm exactly **one** `OVT_StorageComponent` and **one** `ActionsManagerComponent` resolve on each, with the civilian Mi-8's blue livery intact.

**Play-test A — the LAV-25 repro, single player, mouse:**
11. Start a campaign, take a garage or helipad at a resistance base, buy or capture a **LAV-25** and a truck.
12. Drive the truck to a port and **Import 25 mm ammunition** (the `Box_25x137_M242_*` entries). The truck's storage count rises; nothing is spawned.
13. On the truck: **Open Storage → Take → destination "Inventory"** → the transfer reports a shortfall, **nothing appears in the trunk, and the boxes are still listed**. *(This is the bug, fixed: before this feature they vanished.)*
14. Park the truck within 25 m of the LAV. On the truck: **Open Storage → Take → destination "LAV-25"** → the boxes move; the LAV now reads a non-zero storage count. *(Confirms D8: the LAV is a holder.)*
15. Fire the LAV's 25 mm cannon until the magazine is below full. Walk to a rear door. The **Re-arm** action is there and reads **"Re-arm (from storage)"**, with **no price**.
16. Drive the LAV **into open country, far from any base, helipad, garage or FOB.** Re-arm still performs. The cannon is full; the LAV's storage count drops by the units used; the toast names them; **no money is deducted.**
17. Empty the LAV's storage. Fire the cannon again. Off-site, the action is **shown and blocked**, and the reason names the missing ammunition — not "must be on a helipad".
18. Drive to a **built Garage** at a resistance base. The action is now performable at the full difficulty-scaled price; performing it charges exactly that and fills every weapon.
19. Repeat step 18 within 100 m of a **deployed FOB**. Same result.
20. Destroy (or find a ruined) garage and re-check step 18 → blocked again.
21. Partial coverage: put **half** the needed units in a nearby ammo box, empty the LAV, and confirm the label shows a price strictly between $0 and the full price, that performing it charges exactly that, and that every weapon still comes out **full**.

**Play-test B — helicopters and the Mi-8:**
22. An **armed UH-1H**: Re-arm appears at the repair point *and* at the cockpit doors is **not** duplicated; storage exists and reads 100 capacity.
23. A **civilian Mi-8**: storage is unlimited; the blue/red livery is correct; it is purchasable at a civilian vehicle dealer at its own price.
24. Load the Mi-8 with items, **save, Continue, reload** → the count and any custom name are exactly as left. *(This is D10's gate; without the binding it would read 0.)*
25. Fly ammunition to a front-line LAV and rearm it from the helicopter's ledger with the helicopter parked within 25 m.

**Play-test C — dedicated server, two clients:**
26. Client 1 stands at an armed vehicle for 30 s. Client 2, standing beside it, sees **no traffic** and a flat frame time.
27. Client 1 looks at a covered vehicle; the label reads "from storage" within a second of arriving. Client 2 empties the source truck through its own screen; Client 1's label flips to a price within one cache window (2 s).
28. Two clients rearm two different vehicles from **one** shared truck simultaneously → both complete correctly or the second is short-changed *only* on ammunition it can see gone, the ledger total is exactly right, and neither is charged for units it did not receive.
29. **JIP:** Client 3 joins and looks at an armed vehicle → gets a quote and a correct label with no contents traffic.
30. Client 1 performs a rearm while standing 20 m from the vehicle (outside `VEHICLE_MAX_DISTANCE`, forced by walking away between the label refresh and the click) → the server refuses; no ammunition and no money move.

### Bug-report candidates for the orchestrator — do not file from this plan

- `OVT_EconomyManagerComponent.c:1790` reads `if(cfg.m_sFind.Length() < bestMatch) continue;` where `bestMatch` starts at `-1`. It works, but a first entry with an empty `m_sFind` and length 0 is compared against `-1` rather than against "nothing matched yet" — worth a look while touching this file in Phase 6.
- `OVT_TEST_Init_ControllerSeam.c` hard-codes its roster count two lines above the list (epic tech debt, `epic-overview.md`). Not touched here.

---

## 7. Testing Strategy

**Logic tier** (world-free, `new`-built, ~1 s):

| Case | Claim | Proof it can fail |
|---|---|---|
| `RulesAutoCapacity` (extended) | illegal → the **caller's** armed cap, not a constant; truck −1; car default; unregistered still 0 | return `m_iAutoVehicleCapacity` from the illegal branch → the "pass 7 expect 7" assertion fails |
| `RulesHiddenFromInventory` | `(true,false)` hidden; `(true,true)` visible; `(false,*)` **visible** | drop the `authoredFound` clause → an unauthored prefab reads hidden and every item stops being takeable |
| `RearmResolveAmmoPrefab` | muzzle default wins; loaded prefab is the fallback; rocket prefab is the third; all-empty → `""` | swap the first two → the loaded prefab wins and a part-used belt names the wrong stack |
| `RearmProratedCostFull` | `uncovered == total` → exactly `fullCost` | round-trip through the fraction → off-by-one at the full price |
| `RearmProratedCostZero` | `uncovered == 0` → 0; `total == 0` → 0 | drop the guard → division by zero |
| `RearmProratedCostHalf` | 50 % coverage → half, rounded | write the expression in pure ints → truncation, silently under-charging |
| `RearmProratedCostFloor` | a fraction that rounds to 0 with `uncovered > 0` still costs 1 | drop the clamp → free ammunition on a large plan |

**Init tier** — `OVT_TEST_Init_StorageSeam.c` (cases E inverted, F and G added) and the new `OVT_TEST_Init_VehicleRearmSeam.c` (cases A–F, §4). Polls are **preconditions with a named failure on expiry**, never retries; no `maxAttempts`; every spawned subject is deleted before the case reports. Case A's subject must be the **concrete** `Box_25x137_M242_*` prefab, not the `_Base` — a case written against the base would pass while the shipped path failed (§3.6).

**Persistence tier** — one helicopter-ledger round-trip appended to `OVT_TEST_PersistenceRoundTripSuite.c`, modelled on `…_VehicleReserveRelease_KeepsOwnerAndContents` (`:2076`), dirtying state through the public facade before reloading. The class name **must sort after** `…_Capability_…` (`:361`).

**Campaign tier** — nothing new. Every claim in this feature is answerable at Init (the economy catalogue and the storage component both resolve there, which is why `OVT_TEST_Init_StorageSeam` lives at that tier) and a campaign start would cost seconds for no extra coverage.

**What the automated spine cannot reach** — and therefore what the play-test gates exist for:

- **Context reachability.** No suite can stand a player at a rear door. §3.8's table is read off the vanilla prefabs; whether the action is actually *reachable* is play-test steps 15 and 22, and it is the single most likely thing to be wrong.
- **Every multiplayer behaviour.** Quote delivery to one owner, the listen-host `ShouldRespondLocally` branch on a real dedicated server, JIP, two clients on one truck. Steps 26–30.
- **All UI**, including whether "Re-arm (from storage)" reads sensibly next to a price on the same action list.
- **Save/reload of the helicopter ledger** beyond the single round-trip case — step 24 is the real gate.
- **The same-GUID Mi-8 delta actually resolving**, and the civilian livery surviving it. Step 23 and the Workbench check.
- **Performance.** "No traffic" is a frame-watch claim (step 26), not an assertion.

---

## 8. Quality Bar

**Backend reliability**

- **B1 — Never charge for what was not delivered, never destroy stock.** Read coverage, gate on it, consume, restock fully, then charge the quoted figure. No path takes from a ledger before every refusal has been evaluated, so no rollback exists to get wrong. The worst reachable outcome of a mid-frame anomaly is charging the quote for a full restock.
- **B2 — The rearm is one operation, one frame, per player.** No job engine, no progress bar, no shared accumulator. Every sphere query is a `new`-ed object (`OVT_StorageUtils.c:141-147`'s rule), so two players rearming at once cannot see each other's results.
- **B3 — Server-authoritative without exception.** `RpcAsk_RearmVehicle` re-derives the player, the character, the distance, the plan, the coverage, the price, the site and the funds. The quote is never trusted, including on a listen host.
- **B4 — Rejections are visible.** Every refusal — off-site, broke, nothing to rearm, too far — answers with a key the player sees. The one silent return that remains is "you are not the server", which no player can reach.
- **B5 — The label never lies for long.** The quote is refreshed every 2 s while the action is on screen; before the first one arrives the gate is strictly conservative, so the action can only be *less* permissive than the truth, never more.
- **B6 — Persistence is proven, not assumed.** D10 exists because a missing binding is completely silent. A round-trip case and play-test step 24 both cover it.
- **B7 — The wire is auditable.** Three RPCs, arities written down and checked against their handlers, because the compiler will not do it (BUG-090).
- **B8 — Fail open on unreadable data.** An unreadable prefab source makes an item visible and takeable, never permanently trapped, and never poisons the cache.

**Prefab correctness**

- **B9 — One `ActionsManagerComponent`, one `OVT_StorageComponent` per entity.** Inherited GUIDs are re-declared, never minted. A duplicate actions manager silently kills every component declared after it; a duplicate storage component gives one vehicle two ledgers, one of which is invisible.
- **B10 — Same-GUID deltas copy vanilla's header byte-for-byte.** Parent path, parent GUID and `ID` line all match the vanilla file; the delta adds components and changes nothing else. Proven for the Mi-8 by opening a child variant in the Workbench and by the livery surviving.
- **B11 — Fresh GUIDs are proven unused in both trees before authoring.** `6BA1C4E0…` for prefab/config, `{6B0E7A63F4051627}` for the persistence binding; both verified 0 hits at planning time and re-verified at authoring.
- **B12 — The action exists in exactly one prefab.** A moved action left in both places shows twice on a helicopter and nowhere is that visible to a compile check.
- **B13 — Every widened `ParentContextList` is checked against §3.8's table for overlap**, so no vehicle in the game gains a duplicated entry in its action menu.

---

## 9. Dependencies

**Consumed, unmodified:**
- `logistics/storage` — `OVT_StorageComponent`, `OVT_StorageLedger`, `OVT_StorageHolderQuery`, `OVT_StorageComponentSerializer`, `OVT_StorageRequestComponent.GetHolderRadius()`. Only `OVT_StorageRules` and one step of the job engine change.
- `core/controller-migration` — `OVT_ControllerRequestComponent` (`ResolveOwningPlayerId`, `ResolveEntity`, `GetEntityRpl`, `ShouldRespondLocally`, `PlayerMayUseVehicleFor`), `OVT_ControllerComponent<T>.Get()`, `OVT_ComponentFinder`.
- `economy` — `OVT_EconomyManagerComponent` (`PlayerHasMoney`, `TakePlayerMoney`, `LocalPlayerHasMoney`, `ResolvePricingResource`, `IsLegalVehicle`, `GetParkingType`), `OVT_DifficultySettings.vehiclePriceMultiplier`.
- `resistance` — `OVT_ResistanceFactionManager.GetNearestFOBData` (`:1533`), `OVT_BuildableComponent.GetBuildableType` (`:20-23`).
- `towns` / bases — `OVT_OccupyingFactionManager.GetNearestBase`, `OVT_BaseData.IsOccupyingFaction`.
- `core/damage` — `OVT_StructureDamage.IsUsable` (a seam only; **do not modify any `core/damage` file**).
- `core/persistence` — the `Overthrow.conf` binding list; no serializer code.
- High Command — `OVT_WarehouseStockUtils.PrependStore/CountAvailable/TakeUpTo`, called and not changed (D5).
- Vanilla — `BaseMuzzleComponent.GetDefaultMagazineOrProjectileName()` (`:43`), `GetMagazine()` (`:40`), `GetBarrelsCount()` (`:27`), `BaseMagazineComponent.Get/SetAmmoCount` (`:21,23,25`), `SCR_RocketEjectorMuzzleComponent.GetDefaultRocketPrefab()` (`:13`), `RocketEjectorMuzzleComponent.CanReloadBarrel/ReloadBarrel` (`:15,18`), `SCR_BaseContainerTools.FindEntitySource/FindComponentSource` (`:123,239`), `BaseContainer.Get/GetObject/GetAncestor` (`:37,44,21`).

**Modified:** `OVT_StorageRules.c`, `OVT_StorageComponent.c`, `OVT_StorageUtils.c`, `OVT_PrefabUtils.c`, `OVT_VehicleRearmUtils.c`, `OVT_StorageRequestComponent.c` (one step), `OVT_ShopTransactionComponent.c`, `OVT_RearmVehicleAction.c`, `Vehicle_Base.et`, `Helicopter_Base.et`, `vehiclePrices.conf`, `Overthrow.conf`, `overthrowBroadcastMessages.conf`, `localization_Overthrow.st`, four test files.

**New:** `OVT_VehicleRearmRules.c`, `Mi8MT_unarmed_civ_base.et` (+`.meta`), `OVT_TEST_Logic_VehicleRearm.c`, `OVT_TEST_Init_VehicleRearmSeam.c`.

**Deleted:** nothing.

**Concurrent work on this tree:** other sessions commit mid-feature. Re-baseline before every phase; every citation here carries a `file:line` so drift is detectable.

---

## 10. Risks & Mitigation

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| R1 | **The action is not reachable on an LAV-25 or BRDM-2** — the seven-name context list misses, or a name is spelled differently in a variant | Medium | The feature's headline case does nothing, and no static check sees it | §3.8's table is read off the vanilla prefabs with `file:line`; play-test steps 15 and 22 are the gate; D4 names the fallback (mint a context with a measured radius) |
| R2 | **`GetDefaultMagazineOrProjectileName()` returns something that is not a ledger item** on some turret — an ammo-type config or a bare projectile | Medium | That weapon is silently always money-only | Fails safe by construction (an unmatched prefab is simply never covered); D2 names the five-line `GetItemPrefab()` probe as the fix; play-test step 21 exercises partial coverage on a real LAV |
| R3 | **The hidden-item predicate reads badly on a prefab source** — `FindEntitySource` on an unloaded prefab returns zero components | Medium | Either a hidden item leaks (mild) or an ordinary item becomes un-takeable (severe) | Fail-open by design (D7); zero-component reads are not cached; `Resource` held in a local for the whole read, the mitigation `OVT_PrefabUtils.c:84-85` already documents; the Init case uses the **concrete** prefab |
| R4 | **The Mi-8 same-GUID delta breaks the civilian livery** or is never loaded (GUID typo, wrong parent line) | Medium | A visibly broken helicopter, or unlimited storage that never appears | GUID `{366EA0B41474A7F8}` proven from two children's parent lines; header copied byte-for-byte; Workbench opens a child variant; play-test step 23 checks the livery. Two shipped precedents (`Warehouse_01_Base.et`, `Wheeled_Truck_Base.et`) |
| R5 | **Helicopter ledger silently does not persist** — the binding is forgotten or lands on the wrong `EntityPersistenceConfig` | Medium | Players lose everything in a helicopter on every reload, with no error | D10 makes it a first-class phase item; a round-trip case and play-test step 24; the config GUIDs are quoted in §3.9 |
| R6 | **Armed vehicles as holders has knock-on effects nobody scoped** — pickers, `MayUseHolder`, warehouse stock sweeps, FOB undeploy collection | Medium | Surprising behaviour far from this feature | D8 states the consequence explicitly; the shipped assertion that encodes the old rule is **inverted rather than deleted**, so the change is recorded in the suite; the FOB collection query filters on placeables/buildables (`OVT_StorageUtils.c:385`), not on capacity, so it is unaffected |
| R7 | **RPC arity mistake** — `Rpc()` is untyped variadic, a wrong count compiles clean and dies at the wire | Medium | Silent, intermittent, hard to trace | Q3's audit table; three RPCs only; owner replies take the `ShouldRespondLocally` branch (`:1067`) |
| R8 | **The quote round trip makes the label flicker** on a high-latency server, or the 2 s window feels stale | Medium | Cosmetic but annoying | The pre-quote state is conservative, so a flicker can only go from blocked to allowed; the window is one constant to retune; play-test step 27 measures it |
| R9 | **Pro-rata pricing feels wrong in play** — free rearms trivialise ammunition, or partial prices feel arbitrary | Medium | A balance complaint, not a defect | `REARM_BASE_COST` and the difficulty multiplier are untouched, so the money-only case is unchanged; the fraction is one pure function to retune; flagged for the user at play-test |
| R10 | **Hidden items become permanently stuck** for a player who has no armed vehicle and no port | Low–Medium | A dead ledger line with no exit | Port Export and holder-to-holder moves both still work; the officer "Clear inventory" action is the documented escape; stated plainly in §3.6 |
| R11 | **Widening three storage `ParentContextList`s duplicates an action** on some vehicle not in §3.8's table | Low | Two identical entries in an action menu | The three added names are checked for overlap against every target; a duplicate is visible immediately in play-test steps 15/22 and is a one-line revert |
| R12 | **`OVT_StorageRules.ResolveAutoCapacity`'s signature change breaks a caller** added by a concurrent session | Low | Compile error, caught immediately | Two production call sites today; compile-check is the gate; re-baseline before Phase 1 |
| R13 | **Concurrent sessions** change the tree between phases | Medium | Merge pain, stale line references | Re-baseline before every phase; every claim here carries a `file:line` |

---

## Agent Routing Summary

| Phase | Agent | Why |
|---|---|---|
| 1 — pure rules + Logic cases | `component-developer` | Two pure statics, one signature change, test cases. No world, no networking |
| **2 — hidden-item gate** | **`component-developer-advanced`** ⚠️ | Prefab-source archaeology with a known trap (`FindEntitySource` on an unloaded prefab), a cache that must not poison itself, and an edit inside the shipped job engine whose ordering guarantees are load-bearing |
| **3 — capacity + prefab moves + persistence binding** | **`component-developer-advanced`** ⚠️ | The two most-inherited vehicle prefabs in the game, an inherited-GUID re-declaration whose failure mode is silent, a persistence binding whose absence is silent, and a shipped assertion that must be **inverted** rather than deleted |
| **4 — ledger-first rearm** | **`component-developer-advanced`** ⚠️ | Money and stock mutated in one server handler; the gate/consume/charge ordering is the feature's whole data-integrity story; it reaches into storage ledgers, the economy and `core/damage`'s usability seam at once |
| **5 — quote wire + client gate** | **`network-specialist-advanced`** ⚠️ | A new RPC pair on a shipped component, listen-host owner replies, BUG-090's compile blind spot, and a client gate that must be provably conservative before its first quote |
| 6 — civilian Mi-8 | `component-developer` | One new same-GUID delta with a proven GUID, one config entry, one Init case. Independent of Phases 4–5 |
| 7 — localization + help & wiki | main thread + `help-docs-sync` | `.st` master structural safety, and player-facing behaviour changed in five ways |

**Every implementation-agent prompt must carry, verbatim:**

> Do not run `tools/run-tests.sh`. Your gate is `tools/compile-check.sh` exit 0 — I run the test suites myself after the phase completes.

Phases 1 → 2 → 3 are sequential (each supplies the next). Phase 4 needs Phase 3's capacity rule; Phase 5 needs Phase 4's `QuoteRearm`. **Phase 6 may run in parallel with Phases 4–5** once Phase 3 has landed — it touches only the new Mi-8 delta, `vehiclePrices.conf` and one Init case, all disjoint from everything else.
