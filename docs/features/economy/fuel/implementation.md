# Fuel — Implementation Plan

**Epic:** economy (feature #5)
**Status:** ✅ Complete (play-test green 2026-08-18; wiki sync outstanding — auth-blocked)
**Started:** 2026-08-18
**Target Completion:** TBD
**Last Updated:** 2026-08-18 (closed)

---

## Executive Summary

Reforger already ships a complete, working fuel economy that Overthrow never wired up: world fuel
pumps on Eden hold 50,000 L each and dispense for free, fuel trucks carry 5,000 L cargo tanks, and
the whole thing runs through one looped user action (`SCR_RefuelAtSupportStationAction`) driving one
server-side transfer routine (`SCR_FuelSupportStationComponent.OnExecutedServer`). This feature adds
**money** to that stack, and one **buildable** to it — nothing else.

Three deliverables:

1. **Refuelling at a static pump costs money.** A `modded class SCR_FuelSupportStationComponent`
   charges the performing player **per litre actually delivered**, server-side, difficulty-scaled,
   and refuses through the vanilla `IsValid` seam when the player cannot pay so the action greys out
   with a real reason string instead of failing silently. Vehicle-to-vehicle transfer and held
   jerrycans stay free — that fuel was already bought at a pump.
2. **A Fuel Depot buildable at captured bases.** A new prefab carrying vanilla `SCR_FuelManagerComponent`
   + `SCR_FuelSupportStationComponent` + its own refuel action. It starts empty, is filled by parking
   a fuel truck beside it and holding its Refuel action (the same vanilla flow that fills the truck
   at a pump), dispenses free fuel to anything parked in range, and its tank level persists through
   the vanilla `SCR_FuelManagerComponentSerializer`.
3. **A fuel-source API** (`OVT_FuelUtils`) so `resistance/high-command` can later ask "is there a
   fuel source here, and what does a litre cost?" without knowing anything about support stations.

**Zero new RPCs.** The charge is born on the server inside the vanilla flow; the client's affordability
display reads its own already-replicated money. This is the epic's cleanest possible authority story
and it must stay that way.

**Design commitment: every decision that can be a pure function is one.** Cost math, the fractional
charge accumulator, the tick estimate and the price formatting live in `Scripts/Game/Data/`, which is
the only reason any of this is Logic-tier testable — the engine fuel stack is not.

---

## Goals

### Primary Goals

1. **Fuel is a money sink.** Every static world fuel station charges `$X/litre` for what it actually
   delivers, server-authoritative, difficulty-scaled.
2. **Running out of money stops the pump, visibly.** The action greys out with a localized
   "can't afford" reason; a refuel already in progress cuts off and the player is told once.
3. **Free stays free.** Fuel truck → vehicle, jerrycan → vehicle, and depot → vehicle cost nothing.
   No regression to any of these paths.
4. **A depot the resistance builds, fills and drains.** Buildable at captured bases only, starts
   empty, filled from a truck through the vanilla action, dispenses free, persists its level.
5. **A minimal, uniform fuel-source API** for `resistance/high-command`: is this a source, does it
   cost, how much, and where are they.
6. **Logic-tier coverage** for all fee math, each new case proven able to fail once.

### Secondary Goals

1. **`fuelPricePerLitre = 0` disables the whole charging behaviour** — a one-value escape hatch for
   servers that don't want it, and the safe default if anything goes wrong.
2. **The modded classes degrade to vanilla** anywhere Overthrow isn't running (no game mode, no
   economy manager, autotest worlds, Workbench edit mode).

### Explicitly Out of Scope

- AI-driven fuel logistics; the high-command auto-refuel tick itself (this feature only supplies the API).
- Per-town fuel pricing, station stock simulation, restocking world pumps.
- Depots anywhere but captured bases; more than one depot per base is neither enforced nor forbidden.
- Selling fuel back, fuel theft, fuel as a tradeable shop good.
- A map marker for the depot. (`OVT_MapMarkerComponent` on `OVT_VehicleMaintenanceRamp.et:7` is the
  precedent if it is ever wanted; it is not wanted now.)
- Charging AI. A refuel performed by a character that resolves to no player is free — see D7.
- Any change to `OVT_EconomyManagerComponent`. The 1724-line god object gets nothing new.

---

## Architecture Overview

### The vanilla flow this feature hooks (all verified against 1.8.0.10)

```
  player holds "Refuel" on a vehicle
        │  (action lives on the RECEIVING vehicle: Vehicle_Base.et:164,
        │   BaseRefuel.conf → Duration -0.5, PerformPerFrame 1 → loops every 0.5 s)
        ▼
  SCR_BaseUseSupportStationAction.PerformContinuousAction        [client + server]
        │  LoopActionUpdate is only a tick accumulator — it moves no fuel
        ▼
  PerformAction                                                   [SERVER ONLY, m_bIsMaster]
        │  ResetReferencesOnServer() → CanBeShownScript() → CanBePerformedScript()
        │        └── GetClosestValidSupportStation()
        │                 └── station.IsValid(...)   ◄── ① CAN'T-PAY GATE
        ▼
  SCR_FuelSupportStationComponent.OnExecutedServer(owner, USER, action)  ◄── ② stash the user
        │  computes maxFlowCapacityOut for this tick, moves fuel node-by-node
        ▼
  OnFuelAddedToVehicleServer(litresActuallyDelivered, providerNode)      ◄── ③ CHARGE HERE
        │  vanilla: drains the provider node (early-returns when node == null)
```

`IsValid` runs on **both** machines — on the client every 500 ms while the action is hovered (driving
the greyed-out state and the reason string) and again on the server immediately before every 0.5 s
execution (the authority). That is why no RPC is needed: the client answers "can I afford it?" from
`OVT_EconomyManagerComponent.LocalPlayerHasMoney` (`:1025`), which reads its own already-streamed
money, and the server answers the same question authoritatively half a second later.

### Three hooks, one modded component

`Scripts/Game/Components/SupportStation/Modded/SCR_FuelSupportStationComponent.c`:

| Hook | Vanilla ref | What we add |
|---|---|---|
| `IsValid(...)` | `:57-70` | After `super` passes (so range/faction/destroyed/**no-fuel** already answered), if this is a *paid* source and the player cannot afford one full tick, set `reasonInvalid = OVT_CANNOT_AFFORD_FUEL` and return false. |
| `OnExecutedServer(...)` | `:135-264` | Stash `actionUser` in a member, call `super`, clear it. Also fires the one-shot "you ran out" notification if the charge clamped. |
| `OnFuelAddedToVehicleServer(litres, node)` | `:80-88` | Call `super` first (it drains the provider node), then charge the stashed player for `litres`. |

`OnFuelAddedToVehicleServer` receives the **exact litres delivered** (`maxFlowCapacityOut - fuelToAdded`,
line 234) — this is the whole reason the per-litre model is cheap. It is also called on the
`m_BackupMaxFlowCapacity` path with `node == null`, where vanilla early-returns at `:83`; our charge
must run **before** that early-return is reachable, i.e. charge first, `super` second — or charge
independently of the node. Either is fine; charging first is clearer.

`Scripts/Game/UserActions/Modded/SCR_RefuelAtSupportStationAction.c`:

| Hook | Vanilla ref | What we add |
|---|---|---|
| `GetActionNameScript(out outName)` | `SCR_BaseUseSupportStationAction.c:555` | Call `super` (which produces `Refuel (37.5)` with the live fill percentage), then append the price: `Refuel (37.5) ($1/L)`. Only when the resolved station charges and the action is active. |
| `GetInvalidPerformReasonString(reason)` | `SCR_RefuelAtSupportStationAction.c:67` | Map `OVT_CANNOT_AFFORD_FUEL` → `#OVT-Refuel_CannotAfford`, else `super`. |

> **Do not** override `GetActionStringParam()` to inject the price. Vanilla's `GetActionNameScript`
> suppresses the fuel-percentage readout whenever that param is non-empty (`:577-578`); appending
> after `super` keeps both.

### Paid vs free — the discrimination rule

`OVT_FuelUtils.IsFreeFuelSource(station)` returns true when **any** of:

| Test | Covers | How |
|---|---|---|
| Owner carries `OVT_FuelSourceComponent` with `m_bFree` | the Fuel Depot | `FindComponent` on owner, then root parent |
| Station is on a vehicle | fuel trucks, any future fuel vehicle | `SCR_BaseSupportStationComponentClass.CanMoveWithPhysics()` (the public accessor for `m_bIsVehicle`, set by `FuelSupportStation_Vehicle.ct`), backed up by `Vehicle.Cast(owner.GetRootParent())` |
| Station does not use range (`GetRange() <= 0`) | held jerrycans (`FuelSupportStation_Gadget.ct` sets `m_fRange -1`) and any self-only station | `station.UsesRange()` |

Everything else is **paid** at the difficulty price. That is: `FuelSupportStation_FuelPump.ct` (the
Eden commercial pumps and the military `FuelStation_USSR_01`), `FuelSupportStation_Zone.ct` and
`FuelSupportStation_FuelTank.ct` — all static, all charged, regardless of who owns the base they sit
at (user decision D2).

### The fractional-charge problem, and the ledger

Money is an `int`; the price is a float and a tick delivers a fraction of a litre-price. At the
dominant case — a car (`VEHICLE_SMALL`, 250 L/min in) at a pump (`VEHICLE_MEDIUM`, 700 L/min out) —
a 0.5 s tick delivers ~2.08 L, which at `$1/L` is `$2.08`. The remainder must not be silently
dropped every tick, nor rounded up every tick (that would nearly double the price at low rates).

`OVT_FuelChargeLedger` (pure, `Scripts/Game/Data/`) holds `map<string persistentId, float pending>`:

```
Accrue(persId, litres, pricePerLitre):
    pending += litres * pricePerLitre
    whole    = Math.Floor(pending)
    pending -= whole
    return whole                      // dollars to take right now
```

The ledger instance lives **on the modded station component** (one per pump), keyed by persistent id.
It is deliberately **never settled and never persisted** — the sub-dollar remainder simply waits at
that pump for the player's next visit. This removes every "settle on disconnect / on cancel / on
station destroyed" edge case at the cost of at most $0.99 of float per player per pump, which is
below the resolution of the currency. Say so in the code comment so nobody "fixes" it later.

### The affordability gate

`IsValid` needs the cost of the *next* tick before it happens:

```
duration = |action.GetActionDuration()|                     // 0.5 s from BaseRefuel.conf
GetFuelNodeInfo(maxFlow, node, duration)                    // protected, inherited — litres this tick
tickCost = max(1, ceil(maxFlow * pricePerLitre))
allowed  = economy.PlayerHasMoney(persId, tickCost)         // server
         / economy.LocalPlayerHasMoney(tickCost)            // client
```

`maxFlow` is the **station's** output for the tick, clamped to the node's remaining fuel — an upper
bound on what will actually be delivered (the receiving vehicle's own flow-in cap may be lower). The
gate is therefore conservative: a player can be left holding a few dollars they cannot spend because
they cannot afford one full-rate tick. That is intentional and cheap; the alternative (also reading
the target vehicle's nodes) buys nothing a player would notice.

### The depot

```
OVT_FuelDepot.et  (derives {B6370564C0BBAD45}Prefabs/Props/Military/Fuel/MobileWaterTank_FIA_01_fuel.et
                   — an FIA-liveried static fuel tank; its base chain already supplies MeshObject,
                   static RigidBody, multi-phase destruction and RplComponent)
├── OVT_BuildableComponent          m_sBuildableType "FuelDepot"
├── OVT_FuelSourceComponent         m_bFree 1                  ← free marker + HC discovery surface
├── SCR_EditableEntityComponent     m_Flags PLACEABLE          ← parity with the FOB tents
├── SCR_FuelManagerComponent
│     └── SCR_FuelNode  MaxFuel 10000 (A2.3; was 5000) · m_fInitialFuelTankState 0 · m_eFuelNodeType 11
│                       m_MaxFlowCapacityOut VEHICLE_MEDIUM · m_MaxFlowCapacityIn FUEL_CARGO
│                       m_iFuelTankID 1
├── SCR_FuelSupportStationComponent : BaseFuelSupportStation.ct
│                       m_fRange 7 · m_bIgnoreSelf 1 · m_eSupportStationPriority HIGH
├── ActionsManagerComponent
│     ├── UserActionContext "default"  (PointInfo offset at the tank's filler)
│     └── SCR_RefuelAtSupportStationAction : BaseRefuel.conf, ParentContextList { "default" }
└── SoundComponent      SupportStations_Vehicles.acp           ← so refuel audio plays positionally
```

Three prefab details are load-bearing and each has a verified reason:

- **`m_fInitialFuelTankState 0`.** It is in **litres**, not a fraction (`Vehicle_Fuel_Tank_Base.et`
  sets `MaxFuel 5000 / m_fInitialFuelTankState 2500`). Zero = starts empty, as required — *and* it is
  what makes an untouched depot serialize to nothing (see persistence below).
- **`m_eFuelNodeType 11`** = `CAN_RECEIVE_FUEL | CAN_PROVIDE_FUEL | IS_FUEL_STORAGE`, the same value
  the fuel truck's cargo tank uses. Receive satisfies `CanBeRefueledScripted` (needed to be filled),
  provide satisfies `HasFuelToProvide` (needed to dispense and to report `NO_FUEL_TO_GIVE` while empty).
- **`m_bIgnoreSelf 1`.** Without it the depot's own station is a valid provider for the depot's own
  refuel action (`SCR_BaseSupportStationComponent.IsValid:356`) and the depot fills itself out of
  itself. The fuel truck and the static fuel tank templates both set this for exactly this reason.

**Fill flow:** park a fuel truck within ~7 m of the depot's action point → walk to the depot → hold
its Refuel action. The depot is the `actionOwner`; the manager finds the truck's station as the
provider; vanilla moves fuel truck → depot. Identical in every respect to filling the truck at a pump,
which is what the user asked for (D3).

### Persistence

Add `SCR_FuelManagerComponentSerializer "{64C6E14228B31061}"` to the `OVT_BuildableComponent`
`EntityPersistenceConfig` in `Configs/Systems/Persistence/Overthrow.conf` (currently `:184-187`,
holding only `OVT_BuildableComponentSerializer`). The same GUID is already reused verbatim across
both Vehicles groups (`:108`, `:131`), so duplicating it a third time is the file's own convention.

The serializer only persists `SCR_FuelNode`-typed nodes (`GetScriptedFuelNodesList`) and **skips any
node whose fuel equals its `GetInitialFuelTankState()`**, returning `ESerializeResult.DEFAULT` when
nothing differs. With `m_fInitialFuelTankState 0` an empty depot writes nothing and reloads empty; a
partially filled depot writes `{tankId, fuel}` and reloads at that level. Adding the serializer to
the buildable group is harmless for every other buildable — none of them has a fuel manager.

### The fuel-source API (`Scripts/Game/Utilities/OVT_FuelUtils.c`)

Modelled on `OVT_VehicleRearmUtils` (static rules class, `static const` tuning constants, difficulty
lookup with a null-guarded fallback). Minimal by design — high-command's refuel tick is its own work.

```cpp
static float GetFuelPricePerLitre();                                  //!< difficulty knob, clamped >= 0
static bool  IsFreeFuelSource(SCR_BaseSupportStationComponent station);
static float GetFuelCostPerLitre(SCR_BaseSupportStationComponent station);  //!< 0 when free
static int   FindFuelSourcesCovering(vector position, out notnull array<SCR_FuelSupportStationComponent> stations);
static int   FindFuelSourcesNear(vector position, float radius, out notnull array<SCR_FuelSupportStationComponent> stations);
static string ResolvePersistentId(IEntity actionUser);                //!< "" when not a player
```

Enumeration goes through the vanilla registry rather than a parallel list, so destroyed/repaired
stations are handled for free (`SCR_BaseSupportStationComponent.OnDamageStateChanged` already
add/removes). The registry's accessor is protected, so one six-line modded class opens it:

```cpp
// Scripts/Game/Components/SupportStation/Modded/SCR_SupportStationManagerComponent.c
modded class SCR_SupportStationManagerComponent
{
    //! Copies (never aliases) the internal per-type array.
    int OVT_GetSupportStationsOfType(ESupportStationType type, notnull out array<SCR_BaseSupportStationComponent> stations)
}
```

`SCR_SupportStationManagerComponent` is already on `Prefabs/GameMode/OVT_OverthrowGameMode.et:369`,
so every range-bearing station in an Overthrow session is registered today. Held jerrycans
(`m_fRange -1`) are not registered, which is correct — they are not a place you can drive to.

---

## Implementation Phases

### Phase 1 — Fee math, difficulty knob, marker component → `component-developer`

*Estimate: ~0.5 day. Low risk; entirely additive.*

| # | Task |
|---|---|
| 1.1 | `Scripts/Game/Data/OVT_FuelPricing.c` — `ResolvePrice(float)` (clamp <0 → 0), `ComputeCost(litres, price)`, `EstimateTickCost(litresThisTick, price)` (ceil, floor of 1), `FormatPricePerLitre(price)` via `SCR_FormatHelper.FloatToStringNoZeroDecimalEndings(price, 2)` |
| 1.2 | `Scripts/Game/Data/OVT_FuelChargeLedger.c` — `Accrue(key, litres, price) → int`, `Clear(key)`, `GetPending(key)`, `GetTrackedCount()`. `ref map<string, float>` internally |
| 1.3 | `float fuelPricePerLitre` in the Economy block of `OVT_DifficultySettings.c` (after `vehiclePriceMultiplier:88`), `[Attribute(defvalue: "1.0", desc: "Fuel price per litre at static fuel stations (0 disables fuel charging)", category: "Economy")]` |
| 1.4 | Preset overrides in `Configs/Difficulty/`: Easy `0.5`, Hard `1.5`, Extreme `2.5`, Insane `4`, TestWorld `1`. Normal inherits the default (presets list only overrides) |
| 1.5 | Append to `OVT_OverthrowConfigComponent.RplSave` (after `allowFOBDuringQRF:688`) **and** `RplLoad` (after `:779`), **and bump `CONFIG_STREAM_VERSION` 3 → 4** with the file's "Version 4 appended …" doc paragraph |
| 1.6 | *(optional, 3 lines)* mirror into `OVT_OverthrowConfigStruct` + `SetDefaults()` + the `overrideDifficulty` block at `OVT_OverthrowGameMode.c:411-419` |
| 1.7 | `Scripts/Game/Components/OVT_FuelSourceComponent.c` — marker, single `[Attribute("1")] bool m_bFree`, `IsFree()` |
| 1.8 | `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_Fuel.c` — four cases (see Testing Strategy) |

**Acceptance:** `tools/compile-check.sh` exit 0. The four Logic cases exist, each proven red once by
a recorded edit. `CONFIG_STREAM_VERSION` is 4 and the write/read orders match field-for-field.

---

### Phase 2 — The paid-refuel charge path → ⚠️ `component-developer-advanced`

*Estimate: ~1 day. **Advanced tier**: it mods a vanilla class in the money path, it must be correct on
both listen-host and dedicated, and a mistake here either steals money or silently stops charging.*

| # | Task |
|---|---|
| 2.1 | `modded enum ESupportStationReasonInvalid { OVT_CANNOT_AFFORD_FUEL = 550 }` — explicit values in modded enums are legal (vanilla `ETagCategory.c` does it). 550 sits above the fuel band (500-502) so a broke player gets the actionable message, and below `HEAL_*` (600) |
| 2.2 | `OVT_FuelUtils.IsFreeFuelSource` / `GetFuelCostPerLitre` / `GetFuelPricePerLitre` / `ResolvePersistentId` (the range-query half lands in Phase 4) |
| 2.3 | `Scripts/Game/Components/SupportStation/Modded/SCR_FuelSupportStationComponent.c` — the three hooks. Members: `IEntity m_PendingChargeUser`, `ref OVT_FuelChargeLedger m_Ledger` (lazy), `bool m_bLastChargeClamped` |
| 2.4 | Charge routine: `owed = m_Ledger.Accrue(persId, litres, price)`; if `owed <= 0` return; `balance = economy.GetPlayerMoney(persId)`; if `owed > balance` → `owed = balance`, `m_Ledger.Clear(persId)`, `m_bLastChargeClamped = true`; if `owed > 0` → `economy.TakePlayerMoneyPersistentId(persId, owed)`. **`PlayerHasMoney` before any take**, because `TakePlayerMoneyPersistentId:1191` clamps at 0 and would otherwise hide an overdraw |
| 2.5 | One-shot refusal notification: after `super.OnExecutedServer(...)`, if `m_bLastChargeClamped` → `OVT_Global.GetNotify().SendTextNotification("CannotAfford", playerId)` and clear the flag. Fires exactly once because the next tick's `IsValid` refuses |
| 2.6 | `Scripts/Game/UserActions/Modded/SCR_RefuelAtSupportStationAction.c` — price suffix on `GetActionNameScript`, reason mapping on `GetInvalidPerformReasonString` |
| 2.7 | Loc keys `OVT-Refuel_PriceFormat` (`"%1 ($%2/L)"`) and `OVT-Refuel_CannotAfford` in `Language/localization_Overthrow.st` |
| 2.8 | Degradation guards: no economy manager / no config / `price <= 0` / no resolvable player ⇒ never gate, never charge, pure vanilla behaviour |

**Acceptance:** compile-check clean. At an Eden pump, money falls at the configured rate while
refuelling and stops when the tank fills. A truck-to-vehicle transfer and a jerrycan pour cost
nothing. A broke player sees the action greyed with the localized reason. Nothing in this phase adds
an `Rpc(` call.

> **Do not** wrap these files in `#ifndef DISABLE_FUEL`. Only `SCR_FuelNode.c:56` uses that guard, and
> only around its own simulation members — `SCR_FuelSupportStationComponent` and
> `SCR_FuelManagerComponent` are unconditional. (This corrects the reconnaissance note.)

---

### Phase 3 — Fuel Depot buildable + persistence → ⚠️ `component-developer-advanced`

*Estimate: ~1 day. **Advanced tier**: hand-authored prefab wiring across five systems (buildables
config, build path, handler, vanilla fuel stack, persistence config), where a wrong attribute produces
a silent runtime no-op rather than a compile error.*

| # | Task |
|---|---|
| 3.1 | `Prefabs/Structures/Military/FOB/OVT_FuelDepot.et` (+ `.et.meta`) per the anatomy above. Fresh 16-hex GUIDs throughout; hand-authored GUIDs are fine per project convention |
| 3.2 | `Configs/Resistance/buildables.conf` entry #7: `m_sName "Fuel Depot"`, `m_sTitle "#OVT-Build_FuelDepot"`, `m_sDescription`, prefab ref, `m_tPreview "{FD8DB4E782A43D86}UI/Textures/EditorPreviews/Auto/Systems/Stations/E_Station_Refuel.edds"`, `m_iCost 2000`, `m_iRewardXP 25`, `m_bBuildAtBase 1`, `handler OVT_FuelDepotHandler` |
| 3.3 | `Scripts/Game/GameMode/Placeables/OVT_FuelDepotHandler.c : OVT_PlaceableHandler` — `OnPlace` returns true for `playerId == -1` (server/test builds), otherwise requires a nearest base that is non-occupying and within `m_Difficulty.baseRange`. Server-side backstop only; the client's `OVT_BuildContext.CanBuild:294-303` is the UX gate. A `false` return deletes the entity **before** `TakePlayerMoney` (`OVT_ResistanceFactionManager.c:846-856`), so a rejected build is free |
| 3.4 | Add `SCR_FuelManagerComponentSerializer "{64C6E14228B31061}"` to the buildable `ComponentSerializers` block (`Configs/Systems/Persistence/Overthrow.conf:184-187`) |
| 3.5 | Loc keys `OVT-Build_FuelDepot`, `OVT-Build_FuelDepot_Description` |
| 3.6 | Persistence round-trip case appended to `OVT_TEST_PersistenceRoundTripSuite.c` (see Testing Strategy, including its documented fallback) |

**Acceptance:** the depot appears in the build menu only within `baseRange` of a captured base; it
builds, charges `m_iCost × buildableCostMultiplier`, and reads `NO_FUEL_TO_GIVE` while empty. A fuel
truck parked beside it fills it through the depot's own Refuel action. A car parked beside a filled
depot refuels **free** and the depot's level drops. The level survives save → reload.

---

### Phase 4 — Fuel-source discovery API + polish → `component-developer`

*Estimate: ~0.25 day.*

| # | Task |
|---|---|
| 4.1 | `Scripts/Game/Components/SupportStation/Modded/SCR_SupportStationManagerComponent.c` — `OVT_GetSupportStationsOfType`, copying rather than aliasing the internal array |
| 4.2 | `OVT_FuelUtils.FindFuelSourcesCovering(position, out)` (stations whose own `GetRange()` covers the position) and `FindFuelSourcesNear(position, radius, out)` |
| 4.3 | Doxygen `//!` on every public method; a file-header banner explaining what the class answers and for whom (the `OVT_VehicleRearmUtils.c:1-17` house style) |
| 4.4 | Final loc pass; note the `Language/localization_Overthrow.en-us.conf` re-export as a follow-up |

**Acceptance:** `FindFuelSourcesCovering` at a pump returns that pump with a non-zero price; at a
filled depot returns the depot with price 0; in open country returns nothing. Compile-check clean.

---

### Phase 5 — Help & documentation sync → `help-docs-sync`

*Estimate: ~0.25 day. Required: this feature changes what players see and do.*

Tutorial popups (`Configs/Tutorials/`), the Field Manual (`Configs/FieldManual/`) and the public wiki
must all say the same thing: fuel at stations costs money and how much; the depot exists, is built at
captured bases, starts empty and is filled from a fuel truck. **Every claim must cite a file:line or
be cut** — two tutorial tips have already shipped describing mechanics that did not exist.

---

## Key Technical Decisions

**D1 — Per-litre, not per-unit-time. (User decision, 2026-08-18, overriding `requirements.md`.)**
`requirements.md` specifies a "flat fee per unit time" and names `LoopActionUpdate` as the seam. The
user was asked and explicitly chose **per litre delivered**. The plan follows the user. This is
strictly better anyway: `LoopActionUpdate` runs on the client and moves no fuel, whereas
`OnFuelAddedToVehicleServer` is server-only and is handed the exact litres — a per-time fee would
charge a topping-off vehicle the same as an empty one, and would charge for ticks that delivered
nothing. The difficulty knob is therefore `float fuelPricePerLitre`, not an int per-second fee.
*(`requirements.md` should be annotated with this deviation when the feature is built.)*

**D2 — Charge at every static station, regardless of base ownership. (User decision.)** No world
edits, no per-station configuration, no ownership lookup at use time. The depot is the *only* free
static source. Simple to explain to players, simple to implement, and it keeps the depot meaningful.

**D3 — The depot is filled by the vanilla refuel action on the depot itself. (User decision.)** No
new user action, no new transfer code. Park the truck in range, hold the depot's Refuel. Identical to
how a truck's own cargo tank is filled at a pump, which players already know.

**D4 — No use-time ownership gate on the depot.** If a base changes hands, the depot keeps dispensing
free fuel to anyone who reaches it. That fuel was hauled and paid for by the resistance; losing it
with the base is a consequence, not a bug. Gating at use time would need a per-tick base-ownership
lookup for no gameplay gain.

**D5 — Free sources are identified structurally, not by a whitelist.** Marker component (depot) ∨ on a
vehicle (trucks) ∨ no range (jerrycans). A whitelist of prefabs would rot on the next Reforger update
and could never cover mod-added fuel vehicles.

**D6 — Refusal through `IsValid`, not through a bespoke check.** It is the seam the whole
support-station UI already plumbs: a `false` there greys the action, routes a reason enum to
`GetInvalidPerformReasonString`, and — because the server re-runs `CanBePerformedScript` inside
`PerformAction` — is simultaneously the authoritative stop. One hook does display and authority.

**D7 — A refuel by a non-player is free.** If no persistent id resolves from `actionUser` (an AI, a
recruit, a script), we do not charge and do not block. Blocking would break future AI logistics;
charging is impossible (there is no account). High-command charges its *owning player* through
`OVT_FuelUtils`, which is HC's job, not the station's. The theoretical exploit — order a recruit to
refuel your car — has no in-game path today (recruits have no refuel order).

**D8 — The ledger is never settled and never persisted.** Sub-dollar remainders live on the station
for the session. This deletes the entire class of "settle on disconnect / cancel / destroy" bugs for
a maximum leakage of $0.99 per player per pump.

**D9 — `fuelPricePerLitre <= 0` means free everywhere.** No gate, no charge, no label suffix, exactly
vanilla. It is the kill switch and the safest possible failure mode.

**D10 — Difficulty field goes on the wire (`CONFIG_STREAM_VERSION` → 4).** The client needs the price
for the action label and for the local affordability gate. The server-only-field precedent
(`recruitLoadoutFeeMultiplier`) does not apply.

**D11 — Nothing is added to `OVT_EconomyManagerComponent`.** The charge path calls the existing
`GetPlayerMoney` / `PlayerHasMoney` / `TakePlayerMoneyPersistentId` and nothing else. The epic's known
tech debt is that this manager is a god object; this feature does not feed it.

---

## Definition of Done

An independent evaluator should be able to verify all of the following without having read the code.

### Functional Criteria

- [x] **F1** Refuelling a car at an Eden fuel pump deducts money continuously while the action runs.
      Total deducted ≈ `litres added × fuelPricePerLitre` (±$1 for rounding), checked by noting fuel
      % and money before/after.
- [x] **F2** The refuel action's label shows the rate while hovered, e.g. `Refuel (37.5) ($1/L)`,
      **and still shows the fill percentage**.
- [x] **F3** A player who cannot afford one tick sees the action greyed out with a localized reason
      (not blank, not `#OVT-…` raw), and it does not start.
- [x] **F4** A refuel that exhausts the player's money mid-flow **stops**, the player keeps the fuel
      already delivered, money floors at 0 (never negative), and one "cannot afford" notification is
      shown — not one per tick.
- [x] **F5** Fuel truck → vehicle transfer costs **nothing**. Jerrycan → vehicle costs **nothing**.
- [x] **F6** Filling a fuel truck's cargo tank at a world pump **does** cost money (it uses the same
      action and the same paid station).
- [x] **F7** "Fuel Depot" appears in the build menu only within `baseRange` of a base the occupying
      faction does not hold, and is absent at FOBs, camps, towns and villages.
- [x] **F8** A newly built depot is **empty**: its Refuel-from-depot offer reports no fuel to give.
- [x] **F9** Parking a fuel truck beside the depot and holding the depot's Refuel action transfers
      fuel truck → depot, and the truck's level falls.
- [x] **F10** A vehicle parked beside a filled depot refuels **free**, and the depot's stored fuel
      falls by what was delivered.
- [x] **F11** Setting `fuelPricePerLitre 0` restores exactly vanilla behaviour everywhere: no charge,
      no gate, no price suffix.

### Quality Criteria

- [x] **Q1 — Server authority.** All money mutation happens inside `OnFuelAddedToVehicleServer` on the
      server. `grep -rn "Rpc(" ` over the files this feature adds returns **nothing**.
- [x] **Q2 — Money integrity.** `PlayerHasMoney`/`GetPlayerMoney` is consulted before every take; the
      amount taken never exceeds the balance; a player is never charged for litres not delivered.
- [x] **Q3 — No vanilla regression.** Refuel speed, the crewman speed bonus, refuel audio, the
      "someone is refuelling your vehicle" notifications, fuel-tank-full behaviour and the fill
      percentage readout are unchanged.
- [x] **Q4 — Listen-host and dedicated behave identically** (F1, F3, F4 verified on both).
- [x] **Q5 — Graceful degradation.** With the mod loaded but no Overthrow game mode / no economy
      manager, refuelling behaves exactly as vanilla and logs no errors.
- [x] **Q6 — House constraints.** No ternaries; `ref` on Managed in containers; `OVT_`/`m_` naming;
      Doxygen `//!` on public methods; no `#ifndef DISABLE_FUEL` wrapper (see Phase 2 note).
- [x] **Q7 — Compile and tests clean:** `tools/compile-check.sh` exit 0; the Fast and All groups green.

### Integration Criteria

- [x] **I1 — Difficulty replication.** A joining client reads the same `fuelPricePerLitre` the server
      has: the action label on a client shows the server's rate, not the client's local preset.
      `CONFIG_STREAM_VERSION` is 4 and a version-3 client is rejected loudly at connect.
- [x] **I2 — Persistence.** A depot at a non-zero, non-initial fuel level survives save → reload with
      that level. An empty depot reloads empty. The buildable itself, its owner and its associated
      base survive as before.
- [x] **I3 — Build flow.** The depot goes through the normal path: cost × `buildableCostMultiplier`,
      `CannotAfford` on insufficient funds, XP reward, `SetAssociatedBase`, `OVT_PersistenceTracking.Track`.
- [x] **I4 — API.** `OVT_FuelUtils.FindFuelSourcesCovering` at a pump returns it with `cost > 0`; at a
      filled depot returns it with `cost == 0`; on a fuel truck returns it with `cost == 0`.
- [x] **I5 — Localization.** Every new player-facing string is an `#OVT-` key present in
      `localization_Overthrow.st`. No raw English is drawn.
- [x] **I6 — Docs.** Tutorial and Field Manual describe the shipped behaviour (Phase 5). *Wiki half still owed — write path auth-blocked; content drafted in tasks.md T5.2.*

### Verification Method

**Automated (run these after each phase; record exit codes):**

```bash
tools/compile-check.sh                              # expect 0 — free, run constantly
tools/run-tests.sh "{6A6E29FF47ECB840}"             # Fast group — orchestrator only, after a phase
tools/run-tests.sh "{6A6E2A002F53A581}"             # All group  — orchestrator only, after a phase
```

**Manual play-test — the real gate. Fuel, MP and prefabs are not covered by the harness.**

*A. Paid refuel at a world pump (single player / listen host)*
1. Start a campaign, note your money. Drive a car to any Eden fuel station and burn some fuel first.
2. Hover the Refuel action: confirm the label shows both the fill percentage and `($1/L)`.
3. Hold it. Watch money fall continuously and the tank fill. Release at ~50%.
4. Note litres added (tank capacity × Δ%) and money spent — they should agree within a dollar or two.
5. Refuel to full: charging stops the moment the tank is full; the action reports tank-full.

*B. Running out mid-refuel*
6. Set your money to a small amount (dev console or spend down). Start a refuel at a pump.
7. Confirm: fuel stops arriving when the money runs out, money reads 0 (never negative), **one**
   "cannot afford" notification appears, and the action then greys with the can't-afford reason.
8. With $0, hover Refuel: it is greyed with the reason and cannot be started.

*C. Free paths (regression)*
9. Fuel truck beside a car → refuel the car from the truck: **no money moves**.
10. Fill a jerrycan at a pump (this **costs**), then pour it into a car: the pour is **free**.
11. Fill the fuel truck's own cargo tank at a pump: this **costs**.

*D. Depot build*
12. At a captured base, open the build menu: "Fuel Depot" is listed. At a FOB / in a town: it is not.
13. Build it. Money falls by the difficulty-scaled cost; XP is awarded; the model appears.
14. Try to refuel a car from the empty depot: refused with "no fuel to give".

*E. Depot fill and dispense*
15. Drive a fuel truck to within ~7 m of the depot. Walk to the depot, hold **its** Refuel action.
    Fuel moves truck → depot; the truck's gauge falls.
16. Park a car beside the filled depot and refuel: **free**, and the depot's stored fuel falls.
17. Refuel repeatedly until the depot is empty: it reports no fuel to give again.

*F. Persistence*
18. With the depot part-filled to a distinctive level, save and reload the session. The depot is still
    there, at the same level, and still dispenses.

*G. Multiplayer (two processes — the harness cannot do this)*
19. Dedicated server + client. Client refuels at a pump: **the client's** money falls, the server's
    other players' money does not. The client sees the label, the reason string and the notification.
20. Two clients refuel two vehicles at the same pump simultaneously: each is charged for their own
    litres; the pump's stored fuel falls by the sum; neither is charged for the other's fuel.
21. A third client joins after the difficulty is set to a non-default price (JIP): its action label
    shows the **server's** rate. (This is the `CONFIG_STREAM_VERSION` check.)
22. Client builds the depot and fills it from a truck; a second client refuels from it for free.

*H. Kill switch*
23. Set `fuelPricePerLitre 0` in the active difficulty preset. Everything in A behaves as it did
    before this feature: no charge, no suffix, no gate.

---

## Testing Strategy

**Logic tier (automated, world-free)** — `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_Fuel.c`,
registered with `[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]`, no group-config change needed:

| Case | Claim |
|---|---|
| `OVT_TEST_Logic_Fuel_CostMath` | `ComputeCost(litres, price)` = litres × price; a negative or zero price resolves to 0 and yields zero cost; large litre counts do not lose precision beyond the epsilon |
| `OVT_TEST_Logic_Fuel_ChargeLedger` | Accruing 0.3 dollars five times charges `0,0,0,1,0` and leaves 0.5 pending; the sum charged never exceeds the sum accrued; two persistent ids are independent; `Clear` drops the pending and does not touch the other key |
| `OVT_TEST_Logic_Fuel_TickEstimate` | `EstimateTickCost` = `ceil(litresThisTick × price)` with a floor of $1; a zero price yields 0 (never the floor); the estimate is ≥ the cost of the litres actually delivered when delivery ≤ the tick's flow |
| `OVT_TEST_Logic_Fuel_PriceFormat` | `1.0 → "1"`, `1.5 → "1.5"`, `2.25 → "2.25"`, `0 → "0"` — no trailing-zero noise in the action label |

Every case must be shown red once during development by a recorded edit. `maxAttempts` is never used.

**Persistence tier (automated)** — one round-trip case appended to
`Scripts/Game/Tests/TestSuites/Persistence/OVT_TEST_PersistenceRoundTripSuite.c`, following the
five-phase shape of `…_PlayerMoney_SurvivesSaveAndReload` (`:527-685`):

1. Resolve the "Fuel Depot" buildable **by name**, never by hard-coded index.
2. `OVT_Global.GetResistanceFaction().BuildItem(idx, 0, pos, angles, -1)` — `playerId == -1` skips
   funds/distance checks and is why `OVT_FuelDepotHandler` allows it.
3. Set the depot's node to a distinctive level (e.g. 1234 L). Save.
4. Dirty it (7 L). Reload.
5. Re-find the depot by sphere query for an `OVT_BuildableComponent` of type `"FuelDepot"` near the
   build position; assert its fuel is 1234.

Suite rule: a persistence case may not name any persistence-framework, vanilla-persistence or
save-data type — assertions go through public APIs. `SCR_FuelManagerComponent` is an ordinary vanilla
component and is fine to name.

**Documented fallback:** if spawning and re-finding a buildable inside the round-trip harness proves
impractical (spawn timing, navmesh rebuild, or the reload not restoring a mid-session build), degrade
to a same-session Tier-D case in `OVT_TEST_PersistenceSuite` asserting the depot's level survives a
save, and move the reload half to manual step F18. Do **not** delete the coverage silently.

**Play-test gated (nothing else can reach these):**
- Pump feel — charge rate, the label, the stop-when-broke moment.
- Truck → depot fill, and depot → vehicle dispense.
- All multiplayer and JIP behaviour, including the difficulty-replication check.
- Prefab correctness: the depot's action context, station range, node flags and `m_bIgnoreSelf`.
- The free paths (truck, jerrycan) — regressions here are silent and expensive.

---

## Dependencies

**Vanilla 1.8.0.10 fuel stack (read-only, hooked via modded classes):**
`SCR_FuelSupportStationComponent` (`IsValid:57`, `OnFuelAddedToVehicleServer:80`, `GetFuelNodeInfo:92`,
`OnExecutedServer:135`), `SCR_BaseSupportStationComponent` (`IsValid:332`, `CanMoveWithPhysics`,
`UsesRange`, `GetRange`, `GetPosition`), `SCR_BaseUseSupportStationAction`
(`GetClosestValidSupportStation:77`, `CanBePerformedScript:194`, `PerformAction:340`,
`GetActionNameScript:555`), `SCR_RefuelAtSupportStationAction` (`GetInvalidPerformReasonString:67`),
`SCR_FuelManagerComponent` (`CanBeRefueledScripted:98`, `HasFuelToProvide:173`), `SCR_FuelNode`,
`SCR_SupportStationManagerComponent`, `SCR_FuelManagerComponentSerializer`,
`ESupportStationReasonInvalid`.

**Internal — `economy/market` (shipped):** `GetPlayerMoney(persId):1004`,
`LocalPlayerHasMoney(amount):1025`, `PlayerHasMoney(persId, amount):1036`,
`TakePlayerMoneyPersistentId(persId, amount):1186`. Nothing new is added to the manager.

**Internal — `resistance/building` (shipped):** `OVT_BuildablesConfig` / `OVT_Buildable`,
`OVT_BuildContext.CanBuild:294`, `OVT_ResistanceRequestComponent.RpcAsk_BuildItem:238`,
`OVT_ResistanceFactionManager.BuildItem:782`, `OVT_PlaceableHandler.OnPlace`.

**Internal — `core/persistence` (shipped):** the `OVT_BuildableComponent` entity config in
`Configs/Systems/Persistence/Overthrow.conf:173-188`.

**Internal — config/difficulty:** `OVT_DifficultySettings`, `OVT_OverthrowConfigComponent`
RplSave/RplLoad + `CONFIG_STREAM_VERSION`, `Configs/Difficulty/*.conf`.

**Consumed by — `resistance/high-command` (not yet built):** `OVT_FuelUtils`. This feature must land
first. HC owns the auto-refuel tick and the "charge the owning player" decision; this feature only
answers *where* and *how much*.

---

## Risks & Mitigation

**R1 — Modded-vanilla-class fragility across Reforger updates.** *(Medium, structural)*
A future BI release could rename or restructure `OnFuelAddedToVehicleServer` / `IsValid`. A rename or
signature change is **loud** (an `override` of a non-existent method fails compile). The dangerous case
is silent: BI keeps the method but stops calling it, or moves the transfer into
`SCR_FuelManagerComponent.TransferFuelWithFlow` (already stubbed out and commented "Todo: move logic
from SCR_FuelSupportStationComponent" at `SCR_FuelManagerComponent.c:122`). **Mitigation:** run
manual step A1 after every Reforger update as part of `/update-reforger`; keep every hook a thin
wrapper that calls `super`, so re-pointing them at a new seam is a small edit.

**R2 — The backup-flow-capacity path passes a null node.** *(Low, verified)*
A station with no `SCR_FuelManagerComponent` uses `m_BackupMaxFlowCapacity` and calls
`OnFuelAddedToVehicleServer(litres, null)`, where vanilla returns early at `:83`. **Mitigation:**
charge **before** delegating to `super`, using only the `litres` argument; never make the charge
conditional on the node. Eden's pumps do have fuel managers, so this path is rare — which is exactly
why it would go unnoticed.

**R3 — Fractional accumulator edge cases.** *(Low, by construction)*
Disconnect mid-refuel, cancel mid-refuel, two players at one pump, station destroyed mid-refuel.
**Mitigation:** D8 — the ledger is never settled, so there is no "settle" path to get wrong. Keyed by
persistent id, so concurrent refuellers cannot cross-charge. Charging only ever happens after fuel has
already moved, so a disconnect can lose at most one sub-dollar remainder, never take money for fuel
not delivered. Logic case `…_ChargeLedger` pins the accrual arithmetic and the two-key independence.

**R4 — The depot's node is not `SCR_FuelNode`-typed, or its initial state is wrong.** *(Medium, silent)*
`SCR_FuelManagerComponentSerializer` only walks `GetScriptedFuelNodesList` (`SCR_FuelNode` only) and
skips any node whose fuel equals `GetInitialFuelTankState()`. A `BaseFuelNode`, or an initial state
authored as a fraction rather than litres, makes persistence a silent no-op. **Mitigation:** the
prefab spec pins `SCR_FuelNode` + `m_fInitialFuelTankState 0` with the litres-not-fraction evidence;
manual step F18 and the round-trip case both check a **non-zero, non-initial** level specifically.

**R5 — Action label localization.** *(Low)*
A missing key renders as `#OVT-Refuel_PriceFormat`; a wrong parameter order renders `($Refuel/L)`.
**Mitigation:** keys added in Phase 2 with the loc file's exact `CustomStringTableItem` shape, verified
on screen in manual step A2. Key-inside-a-larger-string rendering is known to work.

**R6 — Faction check on the fuel truck blocks the depot fill.** *(Medium, play-test only)*
`FuelSupportStation_Vehicle.ct` sets `m_eFactionUsageCheck 3` (same/friendly current faction). A
resistance player using a captured USSR fuel truck could be refused by
`SCR_BaseSupportStationComponent.IsUserValidFaction` — the same family of defect as BUG-132 (factions
reading hostile to themselves client-side). This is pre-existing vanilla behaviour, not something this
feature introduces, but the depot fill is the first flow that *depends* on it. **Mitigation:** manual
step E15 with a captured enemy truck specifically; if it refuses, the fix is a `FactionAffiliationComponent`
question on the truck, not a change to this feature's design.

**R7 — World pumps are finite and never restock.** *(Low, accepted)*
An Eden pump holds 50,000 L and `OnFuelAddedToVehicleServer` drains it (≈600 car fills). Restocking is
explicitly out of scope, and "stations don't run dry" in `requirements.md` is therefore only
approximately true. **Mitigation:** document the number; if a long campaign ever empties one, a modded
`HasFuelToProvide` or a periodic top-up is a contained follow-up.

**R8 — `m_bBuildAtBase` is not enforced server-side.** *(Low, pre-existing)*
`OVT_ResistanceFactionManager.BuildItem` never reads any of the five location booleans; only the
client's `OVT_BuildContext.CanBuild` does. **Mitigation:** `OVT_FuelDepotHandler` (task 3.3) closes it
for this buildable. The general gap belongs to `resistance/building`, and this feature does not widen it.

**R9 — The modded classes apply everywhere the mod is loaded.** *(Medium)*
Including vanilla Conflict worlds, the Game Master, autotest worlds and Workbench edit mode.
**Mitigation:** task 2.8 — every hook returns to pure vanilla when the economy manager, the config or
a resolvable player is missing, or when the price is ≤ 0. Q5 verifies it.

**R10 — The conservative gate leaves money unspendable.** *(Low, by design)*
A player can be refused with a few dollars left because they cannot afford one full-rate tick.
**Mitigation:** documented as expected behaviour in the Architecture section and in F3/F4; do not
"fix" it by charging in advance, which would take money for fuel not delivered.

**R11 — Hand-authored prefab and config GUIDs.** *(Medium)*
A duplicated or malformed GUID in `OVT_FuelDepot.et`, the buildables entry or the persistence config
surfaces as a runtime no-op, not a compile error. **Mitigation:** fresh 16-hex GUIDs everywhere,
except the deliberate reuse of `{64C6E14228B31061}` for the fuel serializer (which the file already
duplicates twice). Text-wired components always resolve, so no Workbench gate is required — but if the
depot's Refuel action never appears, suspect the `ParentContextList` name before suspecting anything else.

**R12 — The depot's action context name must match a declared context.** *(Medium)*
`BaseRefuel.conf` targets `"fuel_cap"`, a point that exists on vehicle models and does **not** exist on
a static tank prop. **Mitigation:** the depot declares its own `UserActionContext "default"` (the
`OVT_MedicalTent.et` pattern) and its action overrides `ParentContextList { "default" }` (the
`Refuel_Canister.conf` pattern). Manual step E15 is the check.

**R13 — Priority collision if a depot is ever built near a pump.** *(Low)*
`SCR_SupportStationManagerComponent.GetClosestValidSupportStation` prefers higher priority, and the
pump is `VERY_HIGH` against the depot's `HIGH` — a depot built within 6 m of a pump would be shadowed
and the player charged. **Mitigation:** accept it (bases and commercial fuel stations do not overlap
on Eden) and note it; raising the depot to `VERY_HIGH` is a one-word change if it ever matters.

**R14 — The persistence round-trip case may not be buildable in the harness.** *(Medium)*
Spawning a buildable inside a test, saving, reloading and re-finding it is new ground for this suite.
**Mitigation:** the documented degradation path in Testing Strategy — same-session Tier-D coverage
plus manual step F18. Losing the automated case is acceptable; losing the coverage silently is not.

---

## Quality Bar

This is a **backend/gameplay feature with a UI sliver**. It is held to the backend bar throughout and
to a small, specific interaction bar for the action label.

**Server authority and money integrity (the backbone):**
- Money moves **only** on the server, **only** inside the vanilla server-side fuel flow, and **only**
  after the fuel it pays for has already been transferred. No client ever computes a charge.
- No new client→server RPC exists. If one appears in a diff, the design has been abandoned.
- A player is never charged for litres that were not delivered, never charged twice for the same
  litres, and never driven below zero.
- `PlayerHasMoney`/`GetPlayerMoney` is consulted before every take, because the take clamps at 0 and
  would otherwise hide an overdraw as a successful transaction.
- Every free path stays free. A regression that starts charging for truck-to-vehicle transfer is a
  worse defect than never having shipped the feature.

**Multiplayer correctness — listen-host AND dedicated:**
- Both are verified for the charge, the gate and the notification. `m_bIsMaster` is true on a listen
  host, so a "works in SP" result proves nothing about a dedicated server, and vice versa.
- Concurrent refuellers at one station are independently accounted.
- The difficulty price reaches JIP clients (`CONFIG_STREAM_VERSION` bumped, both directions edited
  together, order preserved). A client that shows a different price than the server is a bug, not a
  cosmetic difference — it is the affordability gate disagreeing with the authority.

**No vanilla-behaviour regressions:**
- Refuel speed, the crewman bonus, audio, tank-full handling, the fill-percentage readout and the
  bystander notifications are untouched. The modded hooks call `super` and add, never replace.
- With Overthrow absent or the price at 0, the fuel stack behaves exactly as shipped by BI.

**UI sliver — the action label:**
- The price is **additive**: the label keeps the fill percentage it has always shown.
- The can't-afford state is a real, localized reason string on a greyed action — never a blank label,
  never a raw key, never a silently dead action.
- The "you ran out" notification fires **once** per event, not once per 0.5 s tick.

**Engineering hygiene:**
- Every pure decision lives in `Scripts/Game/Data/` with a Logic-tier case, and each new case has been
  proven able to fail once. A case that cannot go red does not ship.
- Modded vanilla classes live in a `Modded/` folder mirroring the vanilla subsystem path, are named
  after the vanilla class verbatim, guard first and call `super`.
- No ternaries; `ref` on Managed in containers; `OVT_`/`m_` naming; Doxygen `//!` on public methods;
  a file-header banner on each new class explaining *why* it exists.
- `tools/compile-check.sh` is clean at the end of every phase. `tools/run-tests.sh` is the
  orchestrator's gate after a phase completes — never run during planning, never inside a subagent.

---

*Plan created 2026-08-18 from `docs/features/economy/fuel/requirements.md`, the user's per-litre and
charge-everywhere decisions, and a verification sweep of the 1.8.0.10 vanilla support-station/fuel
stack, the Overthrow economy, buildable, difficulty-replication, persistence and test-harness code.
Every vanilla line reference above was read during planning, not inferred.*
