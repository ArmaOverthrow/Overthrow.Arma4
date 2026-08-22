# Resource Production — Implementation Plan

**Status:** 🟡 In Progress
**Started:** 2026-08-22
**Target Completion:** TBD
**Last Updated:** 2026-08-22

**Epic:** `logistics` (feature #5 — see `docs/features/logistics/epic-overview.md`; this plan adds the row)
**Requirements:** `docs/features/logistics/resource-production/requirements.md` (authoritative), plus the four user decisions of 2026-08-22 restated in §5 (D1, D3, D6, D9).
**Approach:** **A — maximum reuse of the `resources` wire and the shared transfer screen.** One new manager holding warehouse-shaped ownership records, one new marker component + prefab family authored on the map, one new small request component for ownership and privacy, **one appended op** (`SITE_BUY`) on the shipped `OVT_ResourceRequestComponent` checkout fan, one thin `OVT_TransferContext` consumer for the buy screen, and one new map location type. Nothing in `logistics/ui` or `logistics/storage` is touched.
**Branch:** `v1.5` (concurrent sessions exist on this tree — re-baseline before every phase; every claim below carries a `file:line` so drift is detectable)

---

## 1. Executive Summary

`logistics/resources` gave the mod a second currency axis but exactly one source: the port. Every litre of timber, cement and steel in a campaign is bought with money from an occupied port at a drifting price, which means the resource economy is a money sink with a driving minigame attached and the map itself contributes nothing.

This feature adds the **producer** side. A map author drops a production-site prefab beside a sawmill, a steel mill or a cement plant and authors four numbers on the instance: a localized name, which resource it makes, how many units per in-game hour, and what it costs to buy. The site drips its one resource into an `OVT_ResourceStoreComponent` on the same entity, one server-side batch per in-game hour, and it does so **whether or not anybody owns it**.

Unowned, the site is a shop: it sells the stock it has actually accumulated at **80 % of the live import price** — cheaper than the port, capped by what it has produced, and reachable only with a truck. Owned, it is infrastructure: the owner takes the resource out with a truck, builds with it, or hauls it to a port and exports it. Ownership is bought from an action on the prefab with personal money or (officer-gated) resistance funds, is not rentable, defaults to **private**, and can be flipped to **public** so the whole resistance can draw from it. Buying clears the stock, so nobody buys a site for its inventory.

Three commitments shape everything below.

1. **The site's mutable state lives in a manager record, not on the entity.** Real estate cannot own these prefabs — `OVT_RealEstateManagerComponent` only registers `SCR_DestructibleBuildingEntity` prefabs path-matched against `m_aBuildingTypes`, and a sawmill marker is neither. So the feature mirrors the shape real estate already uses for warehouses (`OVT_WarehouseData {id, location, owner, isPrivate}`, `OVT_RealEstateManagerComponent.c:17-23`) in a manager of its own, replicated by `RplSave`/`RplLoad` plus two targeted broadcasts and persisted by one serializer (D1).
2. **The wire is extended, not duplicated.** Buying stock from an unowned site is one appended `EOVT_ResourceOp` value on the shipped `Begin…Line…Commit` fan (`OVT_ResourceRequestComponent.c:5-11`), which brings whole-cart atomicity, the `MayUseHolder` ladder, the `MAX_LINE_QUANTITY` bound, the negative-total tripwires and the `spent` field of `RpcDo_TransferResult` (`:887`) along for free. A second purchase protocol would have to re-earn all of it.
3. **One access predicate, two callers.** Whether a player may open a site's storage is a pure function of `(viewer persistent id, owner, isPrivate)`. It is written once in `OVT_ResourceProductionRules`, called by the user action's client-side gate and by the server's `MayUseHolder` ladder, and asserted in the Logic tier. A client gate that disagrees with the server gate is the defect this project keeps re-learning; there is no second copy to drift.

Sites have no running costs, are not enemy targets, and produce one raw resource each. Chains are a later feature.

---

## 2. Goals

### Primary

1. **A map-authored production site** — location, name, resource, rate and price are all attributes on the placed instance, so adding a site is a world edit and nothing else.
2. **Production that runs unowned**, server-only, once per in-game hour, on the shipped hour-latch idiom, surviving a load and accruing across sleep.
3. **Fractional rates that work** — a per-site float carry in the manager record means `0.5 units/hour` produces one unit every two hours rather than nothing forever.
4. **Ownership like a warehouse, minus renting** — buy with personal money or resistance funds, no sell-back, private by default, an owner-only (or officer, for resistance-owned) public/private toggle.
5. **An unowned site sells its accumulated stock** at `round(live price × 0.8)`, floor 1, through the shipped resource checkout, capped by real stock, into a nearby truck, refused whole if the cart does not fit or is not paid for.
6. **A privately owned site sells nothing** — the shop actions disappear the moment the site is bought.
7. **Buying clears the stock** in the same server call that sets the owner.
8. **Map presence at base/town zoom** with the three authored colour states and an info panel showing stock, owner and public/private.
9. **Icons derived from the resource** through a new `m_sMapIconName` on `OVT_Resource`, with a shipped fallback quad so a missing art asset degrades to a crate rather than to nothing.
10. **Everything survives save/continue and JIP** — ownership, privacy, the fractional carry and the per-site stock.

### Secondary

1. **Zero new addressing schemes** — a site is addressed by its **position** on every RPC and in the save, the way camps already are (`OVT_FOBRequestComponent.c:305`), so no index has to agree across machines.
2. **Zero new `OVT_TransferContext` hooks.** The buy screen is a one-row consumer of the closed eight.
3. **Zero new persistence rules.** No `ComponentClassPersistenceConfigRule`, no entity persistence config on the site prefab — the manager's serializer carries the stock (D9's trap, `resources` D15/D16).
4. **A difficulty multiplier that finally does something** — `OVT_DifficultySettings.realEstateCostMultiplier` (`:123`) has shipped dead since it was written; this feature is its first reader.

### Explicitly out of scope

- **No production chains.** One site, one raw resource, no inputs. Requirement §27.
- **No running costs, no upkeep, no taxes** on an owned site.
- **No enemy interaction** — sites are not raid targets, cannot be sabotaged, cannot be captured by the occupying faction, and do not change hands on town flips.
- **No selling a site back**, no rent, no real-estate menu integration of any kind. Prefab actions only (requirement §15).
- **No selling from an owned site.** A privately or resistance-owned site has no shop actions at all; player-to-player trade is manual (requirement §23).
- **No per-site price drift.** The site quotes `OVT_ResourceManagerComponent.GetPrice(i) × 0.8`; the drift that moves it belongs to `resources`.
- **No new HUD.** Stock is on the map panel and in the storage screen.
- **No `Put` restriction.** If the shared transfer screen offers the owner Put (truck → site), that is allowed and deliberate (user decision).
- **No modification of `OVT_TransferContext`, either transfer model, or any `storage` file.** A ninth base hook is a plan defect — raise it, do not widen the base.

---

## 3. Architecture Overview

### 3.1 Where each piece lives

```
Scripts/Game/Data/
└── OVT_ResourceProductionRules.c        NEW   pure statics: pricing, access, privacy, drip maths, colour state

Scripts/Game/Configuration/
└── OVT_ResourcesConfig.c                TOUCH + string m_sMapIconName on OVT_Resource (appended)

Scripts/Game/Components/
├── OVT_ResourceProductionComponent.c    NEW   the site marker: name, resource id, rate, base cost
└── Controller/
    ├── OVT_ResourceProductionRequestComponent.c  NEW  : OVT_ControllerRequestComponent (2 asks, 1 reply)
    └── OVT_ResourceRequestComponent.c   TOUCH + EOVT_ResourceOp.SITE_BUY, gate refactor, money branch

Scripts/Game/GameMode/Managers/
└── OVT_ResourceProductionManagerComponent.c  NEW  records, discovery, hourly drip, replication

Scripts/Game/GameMode/Managers/OVT_OverthrowConfigComponent.c  TOUCH  + GetRealEstateCostMultiplier()
Scripts/Game/GameMode/OVT_OverthrowGameMode.c                  TOUCH  + one Init block (after :1477)
Scripts/Game/Global/OVT_Global.c                               TOUCH  + GetProduction()
Scripts/Game/Services/OVT_SleepService.c                       TOUCH  + one HandleTimeSkip call (after :374)

Scripts/Game/UI/Context/
└── OVT_ProductionSiteBuyContext.c       NEW   one-row OVT_TransferContext consumer

Scripts/Game/UI/Map/LocationTypes/
└── OVT_MapLocationProductionSite.c      NEW   : OVT_MapLocationType

Scripts/Game/UserActions/
├── OVT_BuySiteAction.c                  NEW   personal funds
├── OVT_BuySiteResistanceAction.c        NEW   resistance funds, officer-only
├── OVT_ToggleSitePrivacyAction.c        NEW   owner/officer only
├── OVT_BuySiteStockAction.c             NEW   opens the buy screen, unowned only
└── OVT_OpenResourceStoreAction.c        TOUCH + the site branch of the access gate (Phase 5)

Scripts/Game/Persistence/Serializers/Components/
└── OVT_ResourceProductionManagerSerializer.c  NEW  (OVT_ResourceManagerSerializer is the template)

Prefabs/Production/
├── OVT_ProductionSite_Base.et           NEW   store + production component + actions + RplComponent
├── OVT_ProductionSite_Sawmill.et        NEW   timber defaults
├── OVT_ProductionSite_SteelMill.et      NEW   steel defaults
└── OVT_ProductionSite_CementPlant.et    NEW   cement defaults

Prefabs/GameMode/OVT_OverthrowGameMode.et    TOUCH + OVT_ResourceProductionManagerComponent
Prefabs/GameMode/OVT_OverthrowController.et  TOUCH + OVT_ResourceProductionRequestComponent (before the RplComponent)
Prefabs/Characters/.../Character_Player.et   TOUCH + OVT_ProductionSiteBuyContext block

Configs/
├── Resistance/resources.conf            TOUCH + m_sMapIconName on all four entries
├── Map/OverthrowMap.conf                TOUCH + OVT_MapLocationProductionSite block
└── Systems/Persistence/Overthrow.conf   TOUCH + one serializer in the game-mode entity config {65ACD95F40F6C669}

Worlds/MP/OVT_Campaign_Test_Layers/default.layer   TOUCH + ONE Sawmill instance (test coverage, §7)

UI/Imagesets/overthrow_mapicons.imageset  TOUCH + 4 resource quads (art owed — fallback ships)
UI/Textures/Icons/overthrow_mapicons.*    TOUCH atlas re-import (user, in Workbench)

Language/localization_Overthrow.st        TOUCH ~30 new keys

docs/features/logistics/epic-overview.md      TOUCH row 5 + build order + rollup
docs/features/logistics/epic-requirements.md  TOUCH Out of Scope revision

Scripts/Game/Tests/TestSuites/
├── Logic/OVT_TEST_Logic_ProductionRules.c            NEW
├── Init/OVT_TEST_Init_ProductionSeam.c               NEW
├── Init/OVT_TEST_Init_ControllerSeam.c               TOUCH + 1 line (and the hard-coded "10" in its PrintFormat)
├── Persistence/OVT_TEST_PersistenceRoundTripSuite.c  + 2 cases
└── Campaign/…                                        + 1 case
```

**Reserved GUID series: `6A8E2F…`** for prefab / conf / widget instance GUIDs and **`6B0E7A9…`** for the one `Configs/Systems/Persistence/Overthrow.conf` entry. Both verified 0 hits on 2026-08-22 in **both** trees (`grep -rl 6A8E2F .` → 0 in `Overthrow.Arma4`, 0 in `/mnt/n/Projects/Arma 4/ArmaReforger`; `grep -rl 6B0E7A9 .` → 0). Allocation: `6A8E2F0…` prefab component/action instances, `6A8E2F1…` the `Character_Player.et` context block, `6A8E2F2…` map/imageset, `6A8E2F3…` `.st` entries and spare. **Re-verify before authoring** — another session may have claimed the range. **Inherited component GUIDs are copied, never minted.**

### 3.2 The pure spine — `OVT_ResourceProductionRules`

Statics, no world, no manager identifier anywhere in the file or its test (the Logic tier grep does not distinguish code from prose).

| Signature | Contract |
|---|---|
| `static int SitePrice(int livePrice, float ratio)` | `Math.Max(1, Math.Round(livePrice * ratio))`. `ratio` is `SITE_SELL_RATIO = 0.8`. **Not** `GetSellPrice` — that is the port's 0.5 export ratio and a different number. |
| `static int BuyCost(int baseCost, float multiplier)` | `Math.Max(1, Math.Round(baseCost * multiplier))`. Evaluated identically on the client (label) and the server (charge). |
| `static bool MayAccessStore(string viewerId, string owner, bool isPrivate)` | `owner == ""` → false; `owner == "resistance"` → true; `!isPrivate` → true; else `viewerId != "" && viewerId == owner`. **The single access predicate.** |
| `static bool MayTogglePrivacy(string viewerId, string owner, bool isOfficer)` | `owner == ""` → false; `owner == "resistance"` → `isOfficer`; else `viewerId != "" && viewerId == owner`. |
| `static bool MayBuySite(string owner)` | `owner == ""`. |
| `static bool MayBuyStock(string owner)` | `owner == ""`. Same predicate today, separate name because §23 says a later economy pass may let owners sell. |
| `static bool ShouldProduce(int currentHour, int latchedHour)` | `currentHour != latchedHour`. Every hour, unlike price drift's four windows. |
| `static int Produce(float unitsPerHour, int hours, float carryIn, out float carryOut)` | `unitsPerHour <= 0` or `hours <= 0` → 0, `carryOut = carryIn`. `hours` clamped to `MAX_SKIP_HOURS = 720`. `total = carryIn + unitsPerHour * hours`; `units = Math.Floor(total)`; `carryOut = total - units`, clamped into `[0,1)`. |
| `static int FitProduction(int units, int freeLitres, int litresPerUnit)` | `litresPerUnit <= 0` → 0; `freeLitres < 0` (unlimited) → `units`; else `Math.Min(units, freeLitres / litresPerUnit)`. Whole units that do not fit are **discarded**, not banked (D7). |
| `static int ColourState(string viewerId, string owner, bool isPrivate)` | `0` unowned, `1` owned-and-accessible (public, yours, or resistance), `2` owned-private-not-yours. Drives `GetIconColor` with no branching in the map class. |

`carryOut` is a float, and EnforceScript floats are IEEE **binary32**. That is safe here because the carry is always in `[0,1)` and is re-derived from a fresh sum each tick — it never accumulates absolute magnitude, which is the shape that has bitten this project before.

### 3.3 The site — `OVT_ResourceProductionComponent`

```
[ComponentEditorProps(category: "Overthrow/Components")]
class OVT_ResourceProductionComponentClass : OVT_ComponentClass {};
class OVT_ResourceProductionComponent : OVT_Component
```

| Member | Kind | Notes |
|---|---|---|
| `m_sSiteName` | `[Attribute]` string | Localization key, e.g. `#OVT-ProdSite_Sawmill`. Authored per instance. |
| `m_sResourceId` | `[Attribute]` string | Must match an `OVT_Resource.m_sId` in `resources.conf`. |
| `m_fUnitsPerHour` | `[Attribute(defvalue: "2")]` float | Units of that resource per in-game hour. Sub-1 works (D7). |
| `m_iBaseCost` | `[Attribute(defvalue: "8000")]` int | Multiplied by `realEstateCostMultiplier` at display and charge time. |

**No replicated state.** Every field is authored in a world file that is byte-identical on every machine, so a client reads them locally — the same reasoning that keeps `OVT_ResourceStoreComponent`'s capacity off the wire (`resources` D3 corollary). Everything mutable is a manager record (§3.4).

`OnPostInit` copies the two guards the sibling component already carries (`OVT_ResourceStoreComponent.c:110-133`): return on `SCR_Global.IsEditMode()`, return on `!owner || !owner.GetWorld()` (`ItemPreview` spawns worldless instances and any previewable prefab null-crashes without it), and log a named ERROR when `m_sResourceId` is empty or `GetRpl()` is null. Accessors are plain getters plus `OVT_ResourceStoreComponent GetStore()` (cached `OVT_ResourceUtils.GetStore(GetOwner())`, `OVT_ResourceUtils.c:68`).

**The prefab family** (`Prefabs/Production/`):

| Prefab | Contents |
|---|---|
| `OVT_ProductionSite_Base.et` | `GenericEntity`, **no mesh**. `OVT_ResourceProductionComponent`, `OVT_ResourceStoreComponent { m_fCargoVolume 20; m_sDefaultNameKey "#OVT-ProdSite_Storage" }`, **one** `ActionsManagerComponent` (one `UserActionContext`, `ContextName "default"`, `Radius 8`), `RplComponent` (mandatory — the store's `RplProp` is dead without it, BUG-193). Entity flags copied from `Prefabs/Controllers/OVT_BaseController.et:81`. |
| `OVT_ProductionSite_Sawmill.et` | `m_sResourceId "timber"`, rate 3, cost 8000, `m_sSiteName "#OVT-ProdSite_Sawmill"` |
| `OVT_ProductionSite_SteelMill.et` | `m_sResourceId "steel"`, rate 1.5, cost 20000 |
| `OVT_ProductionSite_CementPlant.et` | `m_sResourceId "cement"`, rate 2, cost 14000 |

The variants are convenience defaults; **the map author overrides all four attributes on the placed instance**, which is what requirement §25 asks for. The base carries no mesh because a site is placed beside existing industrial scenery — the map icon is how a player finds it, and the 8 m action radius is how they use it. If play-test says sites are hard to find on foot, adding a small vanilla sign prop to each variant is a one-line prefab edit; the asset choice is deferred to the user rather than invented here.

**Capacity is the throttle.** 20 m³ of timber at 0.1 m³/unit is 200 units; at 3 units/hour an untouched sawmill fills in ~67 in-game hours and then **pauses**. That is the whole overflow model (D7).

### 3.4 The manager — `OVT_ResourceProductionManagerComponent`

On `Prefabs/GameMode/OVT_OverthrowGameMode.et`, immediately after `OVT_ResourceManagerComponent "{6A8E2E0000000001}"` (`:172-176`). `s_Instance` + `GetInstance()` + `OVT_Global.GetProduction()` beside `GetResources()` (`OVT_Global.c:248`). Registered in `OVT_OverthrowGameMode.c` `EOnInit` with the `FindComponent` + `Init(this)` block that every manager uses (`:1472-1477` is the nearest template).

**The record:**

```
class OVT_ProductionSiteData : Managed
{
    vector location;      //!< The site entity's origin - the identity on every wire and in the save
    string owner;         //!< "" unowned, "resistance", or a player persistent id
    bool   isPrivate;     //!< Meaningless while unowned; set true on purchase
    float  carry;         //!< SERVER ONLY. Fractional production not yet worth a whole unit
    IEntity entity;       //!< Local resolve from discovery. NOT replicated, NOT persisted
}
```

Shaped after `OVT_WarehouseData` (`OVT_RealEstateManagerComponent.c:17-23`) minus the surrogate `id`, which nothing needs because position is the key.

**Discovery, not registration.** `InitializeSites()` runs on **every machine** from `Init()`:

```
GetGame().GetWorld().QueryEntitiesBySphere("0 0 0", 99999999, CheckSiteAdd, FilterSiteEntities,
                                           EQueryEntitiesFlags.STATIC);
```

— verbatim in shape from `OVT_OccupyingFactionManager.InitializeBases()` (`:1009-1016`). `FilterSiteEntities` casts `OVT_ResourceProductionComponent`; `CheckSiteAdd` appends a record with `owner ""`, `isPrivate true`, `carry 0`. Clients therefore have the full site list (positions, entities, authored data) with no replication at all; only `owner`/`isPrivate` are server truth.

**Position is the identity, never an index.** The JIP payload and both broadcasts carry `vector location`. Query order is not guaranteed identical across machines, so an index would be a silent mis-address; a position is not. Matching is `GetSite(vector pos)` → nearest record whose **squared** distance is under `SITE_MATCH_RANGE_SQ` (`SITE_MATCH_RANGE = 10`), because `vector.Distance` is not correctly rounded and an exact-boundary decision would be a coin flip. This is the camp precedent (`OVT_FOBRequestComponent.c:305-320`, which passes the **server's** copy of the position on to the mutator for exactly this reason).

**Replication:**

| Direction | Mechanism | Payload |
|---|---|---|
| JIP | `RplSave`/`RplLoad` | `int version` (1), `int count`, then per site `WriteVector(location)`, `WriteString(owner)`, `WriteBool(isPrivate)`. **`carry` is never written** — it is server-only state. `RplLoad` matches each triple onto the client's discovered record by position and logs a WARNING for an unmatched one. |
| Ownership change | `[RplRpc(Reliable, Broadcast)] RpcDo_SetSiteOwner(vector pos, string owner)` | arity **2** |
| Privacy change | `[RplRpc(Reliable, Broadcast)] RpcDo_SetSitePrivacy(vector pos, bool isPrivate)` | arity **2** |

Server API: `SetSiteOwner(vector pos, string owner)`, `SetSitePrivacy(vector pos, bool isPrivate)` (each mutates then broadcasts), `ClearSiteStock(IEntity site)` (`ledger.Clear()` then `store.PublishContents()` — `Clear()` fires no invoker by design, `OVT_ResourceLedger.c:272`, so the republish is the caller's job). Client-safe API: `GetSites()`, `GetSite(vector)`, `GetSiteForEntity(IEntity)`, `GetSiteOwner(IEntity)`, `IsSitePrivate(IEntity)`.

**The drip.** Its own tick, on the shipped idiom (`OVT_ResourceManagerComponent.c:138-148`):

```
float timeMul = 1;
OVT_TimeAndWeatherHandlerComponent tw = …;              // OVT_TimeAndWeatherHandlerComponent.c:9
if (tw) timeMul = tw.GetDayTimeMultiplier();
AssertHourLatchFromClock();                              // BUG-183: latch from the clock, never from -1
GetGame().GetCallqueue().CallLater(CheckProduction, PRODUCTION_UPDATE_FREQUENCY / timeMul, true, GetOwner());
```

`PRODUCTION_UPDATE_FREQUENCY = 60000`. `CheckProduction()`:

```
if (!Replication.IsServer()) return;
if (!m_bLatchAsserted && AssertHourLatchFromClock()) return;
TimeContainer time = ResolveGameTime();  if (!time) return;
if (!OVT_ResourceProductionRules.ShouldProduce(time.m_iHours, m_iHourProduced)) return;
m_iHourProduced = time.m_iHours;
ProduceForHours(1);
```

`ProduceForHours(int hours)` walks the records; for each it resolves the entity, the production component and the store, calls `Produce(rate, hours, rec.carry, rec.carry)` → `FitProduction(units, store.GetFreeLitres(), defs.LitresAt(idx))` → `ledger.Add(id, fitted, defs, capacity)` → **one** `PublishContents()` per site that actually changed. A site whose entity is not resolvable (streamed out, world not built) is skipped with its carry advanced, which is correct: production is world state, not observer state.

**Sleep.** `void HandleTimeSkip(int hours)` = `if (!Replication.IsServer() || hours <= 0) return;` then `ProduceForHours(hours)` then re-assert the latch from the pre-skip clock, exactly the three-step shape `OVT_EconomyManagerComponent.HandleTimeSkip` documents (`:223-250`). It is wired by **one new call** in `OVT_SleepService.PerformSleepNow()` immediately after the occupying-faction call (`:373-374`) and therefore **before** `AdvanceClock` — the order that file calls its contract.

### 3.5 Ownership and privacy — `OVT_ResourceProductionRequestComponent`

`class OVT_ResourceProductionRequestComponent : OVT_ControllerRequestComponent`, authored on `Prefabs/GameMode/OVT_OverthrowController.et` before the trailing `RplComponent "{65C4B2D3DE955867}"`. It inherits `ResolveOwningPlayerId()`, `ResolveEntity(RplId)` and `ShouldRespondLocally(int)`; **identity is never an RPC parameter**.

| # | Signature | Dir | Arity |
|---|---|---|---|
| 1 | `RpcAsk_BuySite(vector pos, bool useResistanceFunds)` | → Server | 2 |
| 2 | `RpcAsk_SetSitePrivacy(vector pos, bool isPrivate)` | → Server | 2 |
| 3 | `RpcDo_ProductionError(string messageKey)` | → Owner | 1 |

`RpcAsk_BuySite` mirrors `OVT_RealEstateRequestComponent.RpcAsk_BuyBuilding` (`:145-188`) branch for branch, with one addition the real-estate version does not have: **every refusal answers**. The ladder:

1. `Replication.IsServer()`; `playerId = ResolveOwningPlayerId()` > 0 → `#OVT-ProdSite_NoPlayer`
2. `GetSite(pos)` within `SITE_MATCH_RANGE` → `#OVT-ProdSite_NoSite`
3. `MayBuySite(rec.owner)` → `#OVT-ProdSite_AlreadyOwned`
4. Caller within `USE_RADIUS = 30` of the site (the same distance every resource gate uses) → `#OVT-Resource_TooFar`
5. `cost = BuyCost(component.GetBaseCost(), config.GetRealEstateCostMultiplier())`
6. **resistance branch:** `resistance.IsOfficer(playerId)` → `#OVT-ProdSite_NotOfficer`; `economy.ResistanceHasMoney(cost)` → `#OVT-ProdSite_NoFunds`; `economy.TakeResistanceMoney(cost)`; `owner = "resistance"`
   **personal branch:** `persId = players.GetPersistentIDFromPlayerID(playerId)` non-empty; `economy.PlayerHasMoney(persId, cost)` → `#OVT-ProdSite_NoMoney`; `economy.TakePlayerMoneyPersistentId(persId, cost)`; `owner = persId`
7. `manager.SetSiteOwner(pos, owner)`; `manager.SetSitePrivacy(pos, true)`; **`manager.ClearSiteStock(entity)`** — the stock is destroyed in the same call that takes the money (requirement §19).

`RpcAsk_SetSitePrivacy` is the camp-privacy shape (`OVT_FOBRequestComponent.c:304-335`): resolve, match by position, `MayTogglePrivacy(persId, rec.owner, resistance.IsOfficer(playerId))` → `#OVT-ProdSite_NotYours`, then `SetSitePrivacy`.

Both client-side senders take the `ShouldRespondLocally` / `Replication.IsServer()` direct-call branch first — an owner RPC on a listen host is silently dropped, and the whole reply fan arrives **synchronously inside the ask**, so any UI latch must be set before the call, never after.

**Mandatory:** add the one-line entry to `OVT_TEST_Init_ControllerSeam.c` `FindFirstUnresolvedComponent()` (`:105-115`) **and** bump the hard-coded `"all 10 asserted controller components"` in the `PrintFormat` two lines above it to 11.

### 3.6 Buying stock — `EOVT_ResourceOp.SITE_BUY` on the shipped fan

**Appended, value 4** (`OVT_ResourceRequestComponent.c:5-11` — the enum's own comment says append-only because the value travels as a bare int). Source = the site's store `RplId`; dest = the truck's store `RplId`.

Six surgical edits inside `OVT_ResourceRequestComponent.c`, all of them additive:

1. **`IsKnownOp`** (`:1113`) gains `SITE_BUY`. **`OpReadsDest`** (`:1142`) gains `SITE_BUY`. `OpReadsSource` (`:1133`) is `opKind != PORT_IMPORT` and is already correct.
2. **Gate refactor.** `MayUseHolder` (`:934-976`) splits: steps 1–5 (player, store, `OVT_StructureDamage.IsUsable`, `CallerIsWithin(m_fUseRadius)`, `PlayerMayUseVehicleFor`) move verbatim into `protected bool MayReachHolder(int playerId, IEntity holder, out string rejectKey)`; `MayUseHolder` becomes `MayReachHolder` + the existing `WarehouseIsAccessible` step + **a new site step** (Phase 5, §3.7).
3. **`protected bool MayBuyFromSite(int playerId, IEntity holder, out string rejectKey)`** — `MayReachHolder`, then the holder carries an `OVT_ResourceProductionComponent` (`#OVT-ProdSite_NoSite`), then a record exists (`#OVT-ProdSite_NoSite`), then `MayBuyStock(rec.owner)` (`#OVT-ProdSite_Owned`). **This is the one place an owned site is refused.**
4. **`protected bool MayUseHolderForOp(int playerId, IEntity holder, int opKind, bool isSource, out string rejectKey)`** — `SITE_BUY && isSource` → `MayBuyFromSite`, else `MayUseHolder`. `HoldersUsable` (`:986-997`) and Commit's two inline gate calls (`:508`, `:525`) all route through it, so there is exactly one op-aware seam.
5. **Money.** The `bool isPort` block at `:555-596` resolves `economy`, `persId`, the illegal permission and port control. Introduce `bool chargesMoney = (op == PORT_IMPORT || op == SITE_BUY)` and hoist the `economy` + `players` + `persId` resolution to `isPort || chargesMoney`; the `AtAPort` / illegal / port-control work stays behind `isPort`. In the re-derive loop, a `SITE_BUY` branch beside the `PORT_IMPORT` one (`:634-656`):
   ```
   moneyTotal = moneyTotal + (OVT_ResourceProductionRules.SitePrice(resources.GetPrice(line.m_iResIndex),
                                                                    SITE_SELL_RATIO) * line.m_iQuantity);
   if (moneyTotal < 0) → #OVT-Resource_BadRequest      // PlayerHasMoney accepts a negative and
                                                       // TakePlayerMoney of a negative PAYS the player
   ```
   The all-or-nothing branch (`:686-696`) gains `SITE_BUY` alongside `PORT_IMPORT` on the `PlayerHasMoney` check, and the payment block (`:754-758`) gains it on `TakePlayerMoney` + `spent = moneyTotal`.
6. Nothing else. Stock availability is already enforced by the generic `sourceStore.GetLedger().Count(id) < line.m_iQuantity` refusal (`:628-632`), the quantity bound by `MAX_LINE_QUANTITY = 10000` (`:606`), destination space by `destStore.GetFreeLitres() < totalLitres` (`:686`), and the reply carries `spent` already (`:887`). **Nothing clamps; the cart is refused whole.**

A defensive "is this the resource the site produces" check is deliberately **not** added: a site's store only ever holds its own resource, so the stock check is the same refusal with one fewer thing to keep in sync.

### 3.7 The access predicate — one function, two callers (R-1)

| Caller | Where | Effect |
|---|---|---|
| Server | the new site step in `MayUseHolder` (§3.6 item 2): holder carries `OVT_ResourceProductionComponent` → `MayAccessStore(persId, rec.owner, rec.isPrivate)` → `#OVT-Resource_NoAccess` | Every `HOLDER_TO_HOLDER` / `HOLDER_TO_GROUND` op touching a site is gated, including one forged by a modified client |
| Client | `OVT_OpenResourceStoreAction.c` — one branch beside the existing `WarehouseIsOpenTo` step (`:168-181`), setting `SetCannotPerformReason("#OVT-Resource_NoAccess")` | The action is **visible and disabled with a reason**, never hidden — the shipped gate policy of that file |

`OVT_OpenResourceStoreAction` is edited rather than subclassed: it is already "one class, three hosts" (`resources` §3.6), the site is the fourth, and a subclass would fork the 1 s label cache and the m³ formatting. It is not on the epic's wall list. **`PlayerMayUseWarehouse` is not reused** — its `isRented` clause is a known hole and a site cannot be rented.

**The trap this is designed against:** the two gates must share the *predicate*, not merely agree today. A DoD grep asserts `MayAccessStore` appears in exactly those two call sites plus the map class and the Logic suite, and nowhere is the ownership comparison re-implemented inline.

### 3.8 The buy screen — `OVT_ProductionSiteBuyContext`

A `logistics/ui` consumer on the **closed eight hooks**, registered in `Character_Player.et` `m_aContexts` next to `OVT_ResourceTransferContext "{6A8E2E1000000001}"` (`:117-125`), reusing `m_Layout "{6A8E2C1000000001}UI/Layouts/Menu/TransferMenu.layout"`, `m_sContextName "OverthrowTransferContext"` and the three shared row/cart/tab layouts. Instance GUID `6A8E2F1000000001`. **A context absent from `m_aContexts` silently never opens.**

| Hook | Implementation |
|---|---|
| `BuildModes` | one mode: `#OVT-ProdSite_Buy` (0) |
| `BuildEntries` | **one row**: the site's resource. `m_sId = "res:" + id`, `TEXTURE` + the definition's `m_tIcon`, `m_eValueKind = PRICE`, `m_iValue = SitePrice(GetPrice(idx), 0.8)`, `m_iMaxQuantity = Math.Min(stock, MAX_BUY_QUANTITY = 1000)`. Zero stock → the row is listed **disabled** with `#OVT-ProdSite_NoStock`. |
| `GetCategoryLabelKey` | `""` |
| `BuildDestinations` | nearby holders from a per-call `OVT_ResourceHolderQuery` (`OVT_ResourceUtils.c:196`) within `m_fHolderRadius = 25`, **minus the site itself**, sorted nearest-first (query order is spatially arbitrary and would reshuffle under the player's selection index). Empty list is allowed — `ValidateCart` explains it. |
| `FillDetails` | name; unit price; body = the definition description + the shipped per-unit m³/kg line |
| `IsAddAllAllowed` | `true` |
| `ValidateCart` | no destination → `#OVT-Resource_NeedTruck`; cart quantity > stock → `#OVT-Resource_NotEnough`; cart litres > destination `GetFreeLitres()` → `#OVT-Resource_NoCargoSpace`; cart price > player money → `#OVT-Resource_NoMoney`. Advisory only — the server re-derives every one of them. |
| `OnAccept` | **latch first**, then `RequestTransferBegin(siteRplId, destRplId, EOVT_ResourceOp.SITE_BUY, 1)` → one `RequestTransferLine` → `RequestTransferCommit` |
| `GetSummaryText()` (virtual) | total cost and total m³ for the cart |

`m_iMaxQuantity` is capped by **stock only**, not by the destination's free volume: the destination is selectable and a fit-derived cap would be wrong the instant the picker moves. Space is a whole-cart refusal, which is the shipped rule (`resources` D1).

Refresh is the site store's `GetOnContentsChanged()` invoker coalesced at 250 ms; `OnClose` removes exactly what `OnShow` added, including every `CallLater`.

### 3.9 Actions on the site

One `ActionsManagerComponent`, one `UserActionContext`, four authored actions plus the shared store action. `ParentContextList` resolves only within its own manager component, and there is exactly one per entity.

| Sort | Class | Shown when | Label |
|---|---|---|---|
| 1 | `OVT_BuySiteAction` | unowned | `#OVT-ProdSite_Buy_Price` with `BuyCost(...)` formatted in; disabled + reason when the player cannot afford it |
| 2 | `OVT_BuySiteResistanceAction` | unowned **and** the user is an officer | `#OVT-ProdSite_BuyResistance_Price`; disabled + `#OVT-ProdSite_NoFunds` when the treasury is short |
| 3 | `OVT_BuySiteStockAction` | unowned | `#OVT-ProdSite_BuyStock` + the stock figure; opens `OVT_ProductionSiteBuyContext` via `OVT_UIManagerComponent.GetContext`/`SetSite`/`ShowContext`, the `OVT_OpenResourceStoreAction.c:32-48` shape |
| 4 | `OVT_ToggleSitePrivacyAction` | owned **and** `MayTogglePrivacy` | `#OVT-ProdSite_MakePublic` / `#OVT-ProdSite_MakePrivate` |
| 5 | `OVT_OpenResourceStoreAction` (existing class) | always visible; gated by §3.7 | `Storage (x.x / 20.0 m³)` |

Two buy actions rather than one action plus a funding dialog: a `ScriptedUserAction` has no place to ask a question, the real-estate screen already presents the two funding paths as two buttons, and an extra dialog is a new input surface with its own gamepad questions. All labels use the 1 s TTL cache of `OVT_OpenResourceStoreAction.c:19-27`, and `HasLocalEffectOnlyScript()` returns true.

### 3.10 The map — `OVT_MapLocationProductionSite`

`: OVT_MapLocationType`, registered in `Configs/Map/OverthrowMap.conf` beside the base block (`:21-33` is the template) with `m_fVisibilityZoom 0` (visible at the same zoom as towns and bases — requirement §21), `m_IconImageset "{C7691945DE01FB28}UI/Imagesets/overthrow_mapicons.imageset"`, `m_sIconName "crate"` (the **fallback**), `m_bShowName 0`, `m_fShowNameZoom 0.18`, `m_fRefreshInterval 5`, instance GUID `6A8E2F2000000001`.

- **`PopulateLocations`** iterates `OVT_Global.GetProduction().GetSites()`, emits one `OVT_MapLocationData(rec.location, siteName, ClassName())` per record and sets **`m_EntityID` only**. `m_RplID` already defaults to `RplId.Invalid()` at the field (`OVT_MapLocationData.c:19`), so BUG-188 cannot recur here — do not assign it. Returns cleanly when the manager is not up.
- **`GetIconName(OVT_MapLocationData)`** returns the produced resource's `m_sMapIconName`, or `m_sIconName` when it is empty. That fallback is what lets the code ship before the art does.
- **`GetIconColor`** switches on `OVT_ResourceProductionRules.ColourState(localPersId, owner, isPrivate)` over three `[Attribute(UIWidgets.ColorPicker)]` fields: `0 0 0 1` unowned, `0 1 0 1` accessible, `0 1 0 0.5` private-not-yours. The renderer calls `image.SetColor(GetIconColor(location))` (`OVT_MapLocationType.c:659-661`) and honours the alpha directly — **no `GetArgb` is involved**; that helper belongs to the canvas layer and takes a faction index, which a site does not have.
- **`BuildInfoRows`** resolves `location.GetEntity()` at panel-open time and reads the **store on the entity**, never a copy on the record — the `OVT_MapLocationResourcePile` rule that a registry must not become a second source of truth. Rows: status (`#OVT-Unowned` / `#OVT-Public` / `#OVT-Private`), owner (player name, or `#OVT-Resistance`, omitted while unowned), stock (`resource name` → quantity, plus used/total m³), rate (`#OVT-ProdSite_Row_Rate`), and while unowned the buy price.
- **`GetLocationDescription`** returns the same status key.

**`m_sMapIconName` on `OVT_Resource`** is appended after `m_iIllegal` (`OVT_ResourcesConfig.c:48`) so no existing `.conf` entry shifts, and the four `resources.conf` entries get `"timber"`, `"cement"`, `"steel"`, `"hardware"`. The four quads plus the `.edds` atlas re-import are **owed art**, the same debt class as the crate glyph; the fallback means the feature is complete and testable without them.

### 3.11 Persistence

**One serializer, no rules.** `OVT_ResourceProductionManagerSerializer : ScriptedComponentSerializer` with `GetTargetType() = OVT_ResourceProductionManagerComponent`, listed in the game-mode `EntityPersistenceConfig "{65ACD95F40F6C669}"` in `Configs/Systems/Persistence/Overthrow.conf` beside `OVT_ResourceManagerSerializer "{6B0E7A70A1B2C3D4}"` (`:52`), GUID from `6B0E7A9…`.

**No `ComponentClassPersistenceConfigRule` and no entity persistence config on the site prefab.** A rule on `OVT_ResourceStoreComponent` would hijack every truck, warehouse and pile from their existing configs (`resources` D16), and an entity-level config on the site prefab would collide with whatever the placed scenery already matches (`resources` D15). The stock rides the manager's save instead — which is trivially correct because a site holds exactly **one** resource.

**Format** (version-first positional; `SaveContext.Write`/`Read` key on the **local variable's name**, so `sites` must be spelled identically in both methods):

```
context.WriteValue("version", 1);
array<ref OVT_PersistedProductionSite> sites = …;   context.Write(sites);
```

```
class OVT_PersistedProductionSite      // frozen $type - the class NAME is in every save
{
    vector location;
    string owner;
    bool   isPrivate;
    float  carry;
    string stockId;
    int    stockQuantity;
}
```

One array of one record class — never per-field parallel loops, never `array<bool>`. `OVT_PersistedWarehouseV2` is the precedent for a `bool` **inside** a record, which is fine.

**Apply is the manager's, not the serializer's.** Deserialization runs while the world is still being built, so `Deserialize` checks its single `Read()`, hands the array to `manager.StagePersisted(sites)` and returns. The manager applies it at the end of `Init()` (after `InitializeSites()`), and if any record fails to match a discovered site it schedules **one** retry via `CallLater(ApplyStaged, 1000)` and, on the second failure, logs a named ERROR per unmatched record and drops it — the v1 warehouse-migration shape. `ApplyStaged` is idempotent: it sets, never accumulates, so a second pass lands on the same state. Stock is re-applied with `store.ApplyPersisted(array<ref OVT_PersistedResourceLine>)` (`OVT_ResourceStoreComponent.c:167`) followed by `PublishContents()`.

**Matching is by nearest position within 10 m, squared.** A campaign whose world file moved a site by more than that loses that site's record and starts it unowned and empty, which is the honest answer.

### 3.12 Difficulty

`OVT_OverthrowConfigComponent` gains **one accessor**, mirroring `GetResourcePriceMultiplier()` (`:370-373`):

```
float GetRealEstateCostMultiplier()
{
    return m_Difficulty.realEstateCostMultiplier;
}
```

`realEstateCostMultiplier` (`OVT_DifficultySettings.c:123`, Easy 0.4 / default 0.5 / Hard 0.7 / Extreme 1.5 / Insane 2) is **already** written in `RplSave` (`:805`) and read in `RplLoad` (`:868`), so **`CONFIG_STREAM_VERSION` does not move** and no difficulty `.conf` is touched. Price is `BuyCost(baseCost, multiplier)` evaluated identically on both sides.

### 3.13 Replication summary

| State | Server truth | To clients | Persisted |
|---|---|---|---|
| Site set (position, name, resource, rate, cost) | world file | **not replicated** — every machine discovers it | no (it is the world) |
| `owner`, `isPrivate` | manager record | `RplSave`/`RplLoad` + 2 broadcasts | yes |
| `carry` | manager record | **never** | yes |
| Stock | the site's `OVT_ResourceStoreComponent` ledger | the shipped single `RplProp` packed string | yes (in the manager's record) |
| Buy price | derived | derived identically both sides from replicated difficulty + config | no |
| Sale price | derived | derived from the replicated live price table | no |

---

## 4. Implementation Phases

Every phase: `tools/compile-check.sh` exit 0 before hand-back. **`tools/run-tests.sh` is the orchestrator's** — never inside an agent, never during planning (`.claude/test-policy.md`). Suites are run **by class name** (`OVT_TEST_LogicSuite`, `OVT_TEST_InitSuite`, `OVT_TEST_PersistenceRoundTripSuite`, `OVT_TEST_CampaignSuite`), one at a time, **never** the Fast/All groups. Re-baseline (`git pull` / `git status`) before every phase; `OVT_ResourceRequestComponent.c`, `OVT_OverthrowGameMode.c` and `Overthrow.conf` are the three shared files most likely to have moved.

Every implementation-agent prompt must carry, verbatim:

> Do not run `tools/run-tests.sh`. Your gate is `tools/compile-check.sh` exit 0 — I run the test suites myself after the phase completes.

### Phase 1 — The pure spine + the icon field
**Estimate:** 3–4 h · **Agent:** `component-developer` · **Suite:** `OVT_TEST_LogicSuite`

1. `Scripts/Game/Data/OVT_ResourceProductionRules.c` — all ten statics of §3.2.
2. `OVT_ResourcesConfig.c` — `string m_sMapIconName` appended to `OVT_Resource` after `m_iIllegal`.
3. `Configs/Resistance/resources.conf` — the four icon names.
4. `Tests/TestSuites/Logic/OVT_TEST_Logic_ProductionRules.c` — §7's table.

**Acceptance**
- **No manager accessor and no game-mode getter identifier appears anywhere under `TestSuites/Logic/`**, not even in a comment.
- Every case proven able to fail once; the mutation and the resulting message recorded in `context.md`.
- `new` sets every field explicitly (`[Attribute]` defvalues do not apply to `new`); no ternaries; polls are preconditions.
- `SitePrice` never returns 0 and never calls anything on `OVT_ResourceManagerComponent`.
- `Produce` returns `carryOut` inside `[0,1)` for every input in the table, including a 720-hour skip and a 0.05 rate.
- `m_sMapIconName` is **appended**, and `resources.conf` still parses (`grep -c "OVT_Resource \""` unchanged at 4).
- Compile-check exit 0.

### Phase 2 — Manager, prefabs, discovery and the drip ⚠️ ADVANCED AGENT
**Estimate:** 6–7 h · **Agent:** `component-developer-advanced` · **Suite:** `OVT_TEST_InitSuite`

1. `OVT_ResourceProductionComponent.c` (§3.3) with both `OnPostInit` guards and the `GetRpl()`/empty-id ERRORs.
2. `OVT_ResourceProductionManagerComponent.c` (§3.4) — records, `InitializeSites()`, position matching, `RplSave`/`RplLoad`, the two broadcasts, `SetSiteOwner`/`SetSitePrivacy`/`ClearSiteStock`, the hour latch, `CheckProduction`, `ProduceForHours`, `HandleTimeSkip`.
3. `Prefabs/Production/OVT_ProductionSite_Base.et` + the three variants. **The store action is NOT authored yet** — it lands in Phase 5 with both halves of its gate, so there is never a build in which a site's storage is world-readable.
4. `OVT_OverthrowGameMode.et` + `OVT_OverthrowGameMode.c` Init block + `OVT_Global.GetProduction()`.
5. `OVT_SleepService.c` — one `HandleTimeSkip` call after `:374`, before `AdvanceClock`.
6. `Worlds/MP/OVT_Campaign_Test_Layers/default.layer` — **one Sawmill instance** (Phases 6 and 7 need it, and it is the only way discovery is ever asserted).
7. `Tests/TestSuites/Init/OVT_TEST_Init_ProductionSeam.c` — the manager resolves through `OVT_Global.GetProduction()`; the test world's Sawmill is discovered as exactly one record with `owner == ""`; its entity resolves a store at 20000 litres and an `OVT_ResourceProductionComponent` whose resource id is known to `GetDefs()`; a hand-driven `ProduceForHours(1)` raises the ledger by the authored rate.

**Acceptance**
- `Replication.IsServer()` guards `CheckProduction`, `ProduceForHours`, `HandleTimeSkip` and both setters — grep every one.
- The hour latch is asserted **from the clock** at Init (BUG-183), initialised to `-1`, and one production batch per in-game hour is enforced by `ShouldProduce`, not by the tick period.
- `PublishContents()` is called at most once per site per batch, and not at all for a site that produced nothing.
- Position matching compares **squared** distance; `vector.Distance` appears nowhere in the matcher.
- `RplSave` and `RplLoad` write and read the **same fields in the same order**, `carry` in neither.
- Both broadcast arities (2 and 2) written into `context.md` and checked against their handlers (BUG-090: `Rpc()` is untyped variadic).
- No `array<…>` on any RPC.
- The prefab carries **exactly one** `ActionsManagerComponent` and an explicit `RplComponent`; fresh GUIDs from `6A8E2F0…` re-verified 0 hits before authoring.
- Compile-check exit 0.

### Phase 3 — Ownership, privacy, and the difficulty accessor ⚠️ ADVANCED AGENT
**Estimate:** 4–5 h · **Agent:** `network-specialist-advanced` · **Suite:** `OVT_TEST_InitSuite`

1. `OVT_ResourceProductionRequestComponent.c` — the three RPCs of §3.5.
2. `OVT_OverthrowController.et` — append before the trailing `RplComponent`.
3. `OVT_TEST_Init_ControllerSeam.c` — one line **and** the `"10"` → `"11"` in the `PrintFormat`.
4. `OVT_OverthrowConfigComponent.GetRealEstateCostMultiplier()`.
5. `OVT_BuySiteAction.c`, `OVT_BuySiteResistanceAction.c`, `OVT_ToggleSitePrivacyAction.c` + their prefab entries (Sort 1, 2, 4).
6. Extend `OVT_TEST_Init_ProductionSeam.c`: `OVT_ControllerComponent<OVT_ResourceProductionRequestComponent>.Get()` is non-null **and** sits on this player's controller entity (`OVT_TEST_Init_VehicleRequestSeam.c:30-31` is the template); a server-side buy sets the owner, clears the stock and takes the money.

**Acceptance**
- **Every refusal answers `RpcDo_ProductionError` with a key.** A silent `return` is a defect — proven by reading every `return` in both asks.
- Identity is never an RPC parameter; `ResolveOwningPlayerId()` is the only source of the caller.
- Both asks resolve the site from the **server's** record position, and the mutators are called with that vector, not the client's (the `OVT_FOBRequestComponent.c:301-303` finding).
- `ClearSiteStock` runs in the same call as `SetSiteOwner`, and `PublishContents()` follows `Clear()` (which fires nothing).
- The buy price the action's label shows and the price the server charges come from **one** `BuyCost` call each, with the same two inputs.
- `CONFIG_STREAM_VERSION` is **unchanged** — grep proves no edit to `RplSave`/`RplLoad`.
- Officer gating is `OVT_ResistanceFactionManager.IsOfficer(playerId)`, matching `OVT_RealEstateRequestComponent.c:172`.
- Compile-check exit 0.

### Phase 4 — `SITE_BUY` on the wire + the buy screen ⚠️ ADVANCED AGENT
**Estimate:** 6–8 h · **Agents:** `network-specialist-advanced` (1–3) then `ui-developer` (4–6) · **Suite:** `OVT_TEST_InitSuite`

1. `OVT_ResourceRequestComponent.c` — the six edits of §3.6: enum value, `IsKnownOp`/`OpReadsDest`, the `MayReachHolder` extraction, `MayBuyFromSite`, `MayUseHolderForOp` and its three call sites, the money branch.
2. Logic cases for `SitePrice` composition against a hand-built defs table (already in Phase 1) plus the new refusal ordering documented in `context.md`.
3. Hand-audited arity table for the touched file re-checked (the shipped six are unchanged; the audit is to prove it).
4. `OVT_ProductionSiteBuyContext.c` — the eight hooks + `GetSummaryText()` of §3.8.
5. `Character_Player.et` — the context block, GUID `6A8E2F1000000001`.
6. `OVT_BuySiteStockAction.c` + its prefab entry (Sort 3).

**Acceptance**
- **`OVT_TransferContext.c`, `OVT_TransferListModel.c`, `OVT_TransferCartModel.c` unmodified** (`git diff --exit-code`). No ninth hook. If a base change seems necessary, stop and raise it.
- `SITE_BUY` is **appended** to the enum, never inserted; no existing member's value moves.
- `MayReachHolder` is the shipped steps 1–5 **verbatim**; `MayUseHolder`'s external behaviour for every pre-existing op is byte-identical (proven by reading the diff).
- An **owned** site refuses `SITE_BUY` in exactly one place (`MayBuyFromSite`) and answers `#OVT-ProdSite_Owned`.
- The money tripwire (`moneyTotal < 0`) is present on the new branch, and the quantity is bounded by `MAX_LINE_QUANTITY` **and** by real stock.
- **Nothing clamps.** A cart that does not fit, is not stocked or is not paid for is refused whole.
- The screen's latch is set **before** the ask, never after (the reply fan is synchronous on a listen host).
- The destination picker is sorted nearest-first and never lists the site itself.
- Compile-check exit 0.

### Phase 5 — The storage access predicate (both sides)
**Estimate:** 2–3 h · **Agent:** `component-developer` · **Suite:** `OVT_TEST_InitSuite`

1. `OVT_ResourceRequestComponent.MayUseHolder` — the site step of §3.7.
2. `OVT_OpenResourceStoreAction.c` — the client branch beside `WarehouseIsOpenTo`, `SetCannotPerformReason("#OVT-Resource_NoAccess")`.
3. The `OVT_OpenResourceStoreAction` entry on `OVT_ProductionSite_Base.et` (Sort 5).
4. Init cases: a site with `owner ""` refuses; owned-by-you allows; owned-private-by-another refuses; owned-public-by-another allows; `"resistance"` allows.

**Acceptance**
- **`OVT_ResourceProductionRules.MayAccessStore` is the only ownership comparison** — grep finds no inline `owner ==` / `isPrivate` test in the action, the request component or the map class.
- `PlayerMayUseWarehouse` is **not** called for a site (grep) — its `isRented` clause is a known hole.
- The action is visible-and-disabled with a reason, never hidden, matching the file's shipped policy.
- The warehouse path through `MayUseHolder` is unchanged (read the diff).
- Compile-check exit 0.

### Phase 6 — Map location type + icons
**Estimate:** 3–4 h · **Agent:** `ui-developer` · **Suite:** `OVT_TEST_InitSuite`
*May run in parallel with Phase 5.*

1. `OVT_MapLocationProductionSite.c` — §3.10.
2. `Configs/Map/OverthrowMap.conf` — the block, GUID `6A8E2F2000000001`, `m_fVisibilityZoom 0`.
3. `overthrow_mapicons.imageset` — four quads named `timber`, `cement`, `steel`, `hardware`. **The `.edds` atlas re-import is the user's Workbench step**; the code must be correct with the quads missing (fallback `crate`).

**Acceptance**
- `m_RplID` is **never assigned** (its field default is already `RplId.Invalid()`); `m_EntityID` is the only handle set.
- `BuildInfoRows` reads stock from `location.GetEntity()`'s store, not from the record — no second source of truth.
- `GetIconName` falls back to `m_sIconName` on an empty `m_sMapIconName`, and the shipped `.conf` names a quad that exists today.
- `GetIconColor` returns a `Color` (alpha honoured by `image.SetColor`); `GetArgb` appears nowhere.
- Three colour states, all three driven by `ColourState`, all three tunable as `[Attribute]` colours.
- `m_fRefreshInterval` set deliberately (sites never move; 5 matches bases).
- Compile-check exit 0.

### Phase 7 — Persistence + round trip ⚠️ ADVANCED AGENT
**Estimate:** 5–6 h · **Agent:** `component-developer-advanced` · **Suite:** `OVT_TEST_PersistenceRoundTripSuite`

1. `OVT_ResourceProductionManagerSerializer.c` + the frozen `OVT_PersistedProductionSite` (§3.11).
2. `StagePersisted` / `ApplyStaged` + the single bounded retry + the named ERROR on the manager.
3. `Overthrow.conf` — one serializer in `{65ACD95F40F6C669}`, GUID from `6B0E7A9…`.
4. Cases appended to `OVT_TEST_PersistenceRoundTripSuite.c`: `…_ProductionSiteOwnership_RoundTrips` (owner, isPrivate and carry survive) and `…_ProductionSiteStock_RoundTrips` (stock re-applied onto the entity's store).

**Acceptance**
- Serialize/Deserialize locals **identically named**; a deliberate rename shown to fail during development and recorded in `context.md` (a renamed local silently reads zeros and reports success).
- The single `Read()` return is checked; a forced failure leaves live state untouched and logs ERROR.
- **No new `ComponentClassPersistenceConfigRule`** anywhere, and no persistence config on the site prefab — `grep -c "ComponentClassPersistenceConfigRule" Configs/Systems/Persistence/Overthrow.conf` is unchanged from the pre-phase count.
- `ApplyStaged` is idempotent (a second call lands on the same state) and matches by squared distance.
- New cases sort **after** `…_Capability_…`.
- Compile-check exit 0.

### Phase 8 — Localization, epic docs, help & wiki sync
**Estimate:** 2–3 h · **Agents:** main thread + `help-docs-sync` · **Suite:** none (announce the skip)

1. `.st` audit: every runtime key exists in `Language/localization_Overthrow.st` with a filled `Comment`; **count braces before and after** (an unbalanced `.st` means the next Workbench save eats entries); fresh GUIDs; multi-line values use the trailing backslash. **Ask the user to re-export** — keys render raw until then. **Never write `Language/*.conf`** — they are Workbench build output.
2. `docs/features/logistics/epic-overview.md` — Features table row 5, build-order item 5 (after `resources`, independent of `building-repair`), the dependency list, and the rollup line.
3. `docs/features/logistics/epic-requirements.md` — Out of Scope: "Passive town/industry resource production" moves **in** scope as this feature; production **chains** remain out.
4. No input `.conf` is touched, so `check-input-conflicts.py` is not run (state the skip).
5. `help-docs-sync`: this feature adds player-facing behaviour (a new map icon class, five new actions, a new buy screen, a new ownership type), so the phase runs — tutorials (`Configs/Tutorials/`), the Field Manual (`Configs/FieldManual/`) and the wiki's economy/resources pages. **Every claim fact-checked against a `file:line`** before it ships; two tips have shipped invented mechanics before.

---

## 5. Key Technical Decisions

**D1 — Site state is a manager record, not real estate and not an entity component.** (User decision, 2026-08-22.) `OVT_RealEstateManagerComponent.SetOwnerPersistentId` only produces a record when `GetConfig(entity)` path-matches an `SCR_DestructibleBuildingEntity` against `m_aBuildingTypes`; a production-site marker is neither a destructible building nor path-matched, so real estate would silently register nothing. A manager mirroring the shipped `OVT_WarehouseData` shape costs one small class and buys ownership, privacy and the fractional carry in one persisted, replicated place. *Rejected:* extending real estate (a read-only dependency for this epic, and the change would touch every warehouse); replicated props on the site component (four `RplProp`s per site, a JIP story per site, and the carry would leak to clients).

**D2 — Position is the identity on every wire and in the save.** World-query order is not contractually identical across machines, so an index would be a silent mis-address; `OVT_FOBRequestComponent.RpcAsk_SetCampPrivacy` already addresses camps this way and its own comment explains why the **server's** copy of the position is what gets passed on. Matching is nearest-within-10 m by **squared** distance (`vector.Distance` is not correctly rounded). *Rejected:* a sorted-index scheme (a stable sort is one more thing that must agree, for a saving of a few bytes per message); an `RplId` (a site's `RplComponent` id is not stable across a save/load).

**D3 — Per-site capacity is the authored `m_fCargoVolume`, and production pauses when full.** (User decision, 2026-08-22.) The store component already caps in integer litres, already replicates its contents in one string, and already exposes `GetFreeLitres()`. A full site simply drips what fits and discards the remainder for that hour — there is no partial-overflow accounting to design, no spillage entity, and no second failure mode. 20 m³ is the base default; a map author raises it for a flagship site. *Rejected:* unlimited capacity (an unowned site would accumulate for a whole campaign and be worth more than the base it sits next to).

**D4 — `SITE_BUY` is an appended op on the shipped checkout, not a new protocol.** The fan already provides whole-cart atomicity, the six-step `MayUseHolder` gate, `MAX_LINE_QUANTITY`, the negative-total tripwires, the `spent` reply field and a client that knows how to drive it. A dedicated `RpcAsk_BuySiteStock` would have to re-earn every one of those, and the two would drift. The enum's own header says append-only, and appending is the whole change. *Rejected:* a bespoke purchase RPC; routing site purchases through the port's `PORT_IMPORT` (it would need an `AtAPort` exemption, an importable-flag exemption and an illegal-gate exemption — three holes in a shipped gate to save one enum value).

**D5 — One access predicate, called by the client gate and the server ladder.** The class of bug this designs out is a client gate that says yes where the server says no (or worse, the reverse), and the only durable fix is that there is nothing to keep in sync. `MayAccessStore(viewerId, owner, isPrivate)` is pure, Logic-tested, and named in a DoD grep. **`PlayerMayUseWarehouse` is deliberately not reused** — its `isRented` clause is a recorded hole and a site cannot be rented.

**D6 — Purchase takes personal money or resistance funds, mirroring real estate; there is no sell-back.** (User decision, 2026-08-22.) Two actions rather than one action and a funding dialog, because a `ScriptedUserAction` cannot ask a question and the real-estate screen already presents the two paths as two buttons. A resistance-owned site is accessible to everyone and its privacy toggle is officer-gated. No sell-back, because no refund path exists anywhere in the mod and inventing one here would need a price memory across a difficulty change.

**D7 — Rate is a float per in-game hour with a per-site fractional carry; the drip is one server batch per hour.** A sub-1 rate is the interesting case (a steel mill at 1.5/hour), and integer-per-hour would round it to nothing or to double. The carry lives in the record (persisted, never replicated) and stays inside `[0,1)`, so binary32 never accumulates magnitude. The tick is the shipped `CallLater(…, FREQ / timeMul, true, GetOwner())` idiom with an hour **latch**, asserted from the clock at load (BUG-183) and replayed by `HandleTimeSkip` so sleeping accrues — which needs one new call in `OVT_SleepService.PerformSleepNow()`, before `AdvanceClock`, matching the order that file's comment calls its contract. *Rejected:* per-minute accrual (60× the work for a number nobody watches); riding the price-drift tick (`resources` deliberately misses a window on a skip; production must not).

**D8 — Sites are discovered by a world query on every machine; nothing registers.** The set of sites is authored in the world file and is therefore identical everywhere, so the client can build the list itself and only the mutable fields need a wire. This is `OVT_OccupyingFactionManager.InitializeBases()` verbatim in shape (`:1009-1016`), and it is why the map location type needs no replicated registry and why `OVT_MapMarkerComponent` — which exists for **runtime-spawned** entities — is not used here.

**D9 — The stock is persisted by the manager's serializer; no persistence rule and no entity config.** (User decision, 2026-08-22.) A `ComponentClassPersistenceConfigRule` on `OVT_ResourceStoreComponent` would hijack every truck, warehouse and pile from their existing configs, and an entity persistence config on the site prefab would collide with whatever the scenery already matches — the two traps `resources` D15/D16 record. A site holds exactly one resource, so two extra scalars inside the ownership record cover it completely.

**D10 — The unowned sale price is `round(live × 0.8)`, floor 1 — not `GetSellPrice`, not the base price, not the stored price.** `GetSellPrice` (`OVT_ResourceManagerComponent.c:221`) is the **port export** ratio (0.5) and answers a different question; the base price is config, not state; the stored price is pre-multiplier. The site quotes the same live number the port import quotes, discounted — which is what makes "drive further, pay less" legible. The price is re-derived server-side at Commit, so a client that lies about it is refused, not obeyed.

**D11 — The map icon comes from a new `m_sMapIconName` on `OVT_Resource`, with a shipped fallback.** (User decision, 2026-08-22.) The alternative — a per-site icon attribute — would let two sawmills disagree; deriving from the resource is the requirement (§21) and makes a new resource's icon a `.conf` edit. The four quads and the atlas re-import are owed art; the fallback quad means the code is complete, testable and shippable before the art lands, exactly as the crate glyph was.

**D12 — The site prefab carries no mesh.** Sites are placed beside existing industrial scenery, so a mesh would either clash with it or duplicate it; the map icon is discovery and the 8 m action radius is use. If play-test says a site is hard to find on foot, adding a vanilla sign prop to each variant is a one-line prefab edit — the asset choice is the user's, not this plan's to invent.

**D13 — The store action is authored in Phase 5, not Phase 2.** Authoring it earlier would leave a build in which any player can open any site's storage, because the server gate does not exist until Phase 5. Deferring the prefab entry by three phases removes that window entirely and costs nothing.

**D14 — `OVT_OpenResourceStoreAction` is edited, not subclassed.** It is already "one class, N hosts" (truck, pile, warehouse), the site is the fourth, and a subclass would fork the 1 s label cache and the m³ formatting. It belongs to the sibling `resources` feature, which is Ready-for-Review rather than closed, so the edit is small, additive and called out in §9 R7 as a merge-conflict risk.

---

## 6. Definition of Done

### Functional

- **F1** A map-placed production site produces its authored resource into its own store once per in-game hour, whether or not anybody owns it, and pauses when the store is full.
- **F2** A rate below 1 unit/hour produces correctly over time (0.5/hour yields one unit every two hours), and the partial progress survives a save.
- **F3** Sleeping through N hours accrues N hours of production on every site — no more, no less.
- **F4** An **unowned** site shows Buy (personal), Buy (resistance funds, officers only) and Buy stock; an **owned** site shows none of them.
- **F5** Buying with personal money takes it from the buyer, sets them as owner, sets the site private, and **empties the stock in the same action**.
- **F6** Buying with resistance funds is refused for a non-officer and for an empty treasury, each with a message; on success the owner is `"resistance"` and everyone in the resistance can use the storage.
- **F7** The owner (or an officer, for a resistance site) can toggle public/private; the toggle appears for nobody else.
- **F8** Buy stock opens a one-row screen quoting `round(live × 0.8)`, capped at the site's real stock, with a nearby truck as the destination; buying moves the resource and takes the money, and the price charged equals the price shown.
- **F9** Buying more than the site holds, more than the truck can take, or more than the player can pay is refused **whole**, with a message. Nothing is ever partially bought.
- **F10** With no truck nearby, the buy screen's Accept is refused with `#OVT-Resource_NeedTruck` rather than being silently dead.
- **F11** Opening the storage is allowed for the owner, for anyone when the site is public or resistance-owned, and refused-with-a-reason otherwise; a client-side hack that skips the action is refused by the server with the same answer.
- **F12** The site appears on the map at town/base zoom in black (unowned), green (yours / public / resistance) or half-alpha green (private, not yours), with an info panel showing stock, owner, status, rate and — while unowned — the buy price.
- **F13** The icon is the produced resource's quad, or the crate fallback when the quad is missing.
- **F14** Ownership, privacy, the fractional carry and the stock all survive save/continue.
- **F15** A joining client immediately sees correct ownership, privacy, colours and stock with no action taken.
- **F16** Nothing about a site is a target for the occupying faction, and no upkeep is ever charged.

### Quality

- **Q1** `tools/compile-check.sh` exit 0 at every phase boundary.
- **Q2** Every refusal on every RPC answers with a localization key. No silent returns.
- **Q3** Every RPC's arity is written into `context.md` and checked against its handler.
- **Q4** No `array<…>` on any RPC; no `Rpc()` call wrapped in a helper.
- **Q5** No exact float comparison decides anything: litres are integers, distances are compared squared, the carry is bounded to `[0,1)`.
- **Q6** `PublishContents()` is called at most once per site per production batch and once per completed transfer.
- **Q7** `git status --porcelain Configs/Language/` is empty (the `.conf` exports are Workbench build output).
- **Q8** `Language/localization_Overthrow.st` brace count is balanced before and after; every new key has a filled `Comment`.
- **Q9** No new tutorial/help sentence lacks a `file:line` behind it.

### Integration

- **I1** `OVT_TransferContext.c`, `OVT_TransferListModel.c`, `OVT_TransferCartModel.c`, `OVT_StorageLedger.c`, `OVT_StorageComponent.c`, `OVT_StorageRequestComponent.c`, `OVT_StorageContext.c` are **unmodified**.
- **I2** `OVT_EconomyManagerComponent.c` and `OVT_RealEstateManagerComponent.c` are **unmodified** (read-only consumers).
- **I3** `OVT_ResourceManagerComponent.c`, `OVT_ResourceStoreComponent.c`, `OVT_ResourceLedger.c` and `OVT_ResourceUtils.c` are **unmodified** — this feature consumes them.
- **I4** The only edits inside `resources` are `OVT_ResourceRequestComponent.c` (the six additive edits of §3.6 plus §3.7's step), `OVT_ResourcesConfig.c` (one appended field) and `OVT_OpenResourceStoreAction.c` (one gate branch).
- **I5** `CONFIG_STREAM_VERSION` is unchanged.
- **I6** `EOVT_ResourceOp`'s four shipped values keep their numbers.
- **I7** No new `ComponentClassPersistenceConfigRule`; no persistence config on any new prefab.

### Verification method — an independent evaluator can follow this

**Static (no game):**

1. `cd "/mnt/n/Projects/Arma 4/Overthrow.Arma4" && tools/compile-check.sh` → exit 0.
2. `git diff --exit-code -- Scripts/Game/UI/Context/OVT_TransferContext.c Scripts/Game/Data/OVT_TransferListModel.c Scripts/Game/Data/OVT_TransferCartModel.c Scripts/Game/Data/OVT_StorageLedger.c Scripts/Game/Components/OVT_StorageComponent.c Scripts/Game/Components/Controller/OVT_StorageRequestComponent.c Scripts/Game/UI/Context/OVT_StorageContext.c` → clean (I1).
3. `git diff --exit-code -- Scripts/Game/GameMode/Managers/OVT_EconomyManagerComponent.c Scripts/Game/GameMode/Managers/OVT_RealEstateManagerComponent.c Scripts/Game/GameMode/Managers/OVT_ResourceManagerComponent.c Scripts/Game/Components/OVT_ResourceStoreComponent.c Scripts/Game/Data/OVT_ResourceLedger.c Scripts/Game/Utilities/OVT_ResourceUtils.c` → clean (I2, I3).
4. `grep -n "SITE_BUY" Scripts/Game/Components/Controller/OVT_ResourceRequestComponent.c` → the enum entry is **last**, and `HOLDER_TO_HOLDER`/`HOLDER_TO_GROUND`/`PORT_IMPORT`/`PORT_EXPORT` are still in that order (I6).
5. `grep -rn "MayAccessStore" Scripts/Game --include=*.c` → exactly four production call sites (request component, store action, map location type, plus the rules definition) and the Logic suite; **no inline `owner ==` comparison** anywhere else (D5).
6. `grep -rn "PlayerMayUseWarehouse" Scripts/Game/Components/Controller/OVT_ResourceRequestComponent.c` → only the pre-existing warehouse step.
7. `grep -c "ComponentClassPersistenceConfigRule" Configs/Systems/Persistence/Overthrow.conf` → unchanged from the pre-feature count (I7).
8. `grep -n "CONFIG_STREAM_VERSION" Scripts/Game/GameMode/Managers/OVT_OverthrowConfigComponent.c` → still `6` (I5).
9. `grep -rn "GetSellPrice\|GetBasePrice\|GetStoredPrice" Scripts/Game/Data/OVT_ResourceProductionRules.c Scripts/Game/UI/Context/OVT_ProductionSiteBuyContext.c` → **no hits** (D10).
10. `grep -rn "OVT_Global\.\|GetGame().GetGameMode()" Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_ProductionRules.c` → no hits (Logic-tier purity).
11. `grep -rn "vector.Distance" Scripts/Game/GameMode/Managers/OVT_ResourceProductionManagerComponent.c` → no hits in the matcher (Q5).
12. `grep -rl "6A8E2F" "/mnt/n/Projects/Arma 4/ArmaReforger"` → 0 (GUID series still exclusive).
13. `git status --porcelain Configs/Language/` → empty (Q7).
14. Orchestrator only, after each phase: `tools/run-tests.sh OVT_TEST_LogicSuite` / `OVT_TEST_InitSuite` / `OVT_TEST_PersistenceRoundTripSuite` / `OVT_TEST_CampaignSuite` **by class name**, one at a time. Announce the focus steal first.

**Workbench (user-gated):** open `OVT_ProductionSite_Base.et` and one variant without dropped-attribute warnings; confirm one `ActionsManagerComponent` with five actions and an `RplComponent`; open `OVT_OverthrowGameMode.et`, `OVT_OverthrowController.et` and `Character_Player.et`; re-import `overthrow_mapicons.edds` after the quads are drawn; re-export the string table.

**Play-test A — single player, mouse:**

15. Start a campaign near the test-world Sawmill. It is on the map at town zoom, **black**, and its panel shows 0 stock, Unowned, the rate and a buy price.
16. Sleep ~12 hours. The panel's stock has risen by `rate × 12` (rounded down) and the fractional remainder is invisible but real.
17. Walk up. Three actions: Buy, Buy (resistance) if you are an officer, Buy stock. **No** storage action is usable.
18. Buy stock with no truck nearby → the screen opens, the row is there, Accept refuses with "needs a truck".
19. Drive a truck within 25 m. Buy stock → the price is 80 % of the port's import price for that resource; buy 20 units. Money falls by exactly the quoted total; the truck's cargo HUD rises; the site's stock falls.
20. Try to buy more than the site holds → refused **whole** with a message; nothing is bought and nothing is charged.
21. Buy the site with personal money → the money goes, the icon turns **green**, the buy actions disappear, the storage action appears, and **the stock is zero**.
22. Toggle to public, then private. The panel status follows. Sleep again → stock accrues while owned.
23. Take resources out through the storage screen into the truck; put some back. Both directions work.
24. Save / Continue → reload → ownership, privacy, stock and the accrued fraction are exactly as left (sleep one hour and confirm the fraction did not reset).
25. As an officer with a funded treasury, buy a second site with resistance funds → owner reads Resistance, everyone can open the storage, the privacy toggle is officer-only.

**Play-test B — gamepad only (no mouse touched):**

26. Walk to an unowned site; all site actions are reachable and readable on a pad.
27. Open Buy stock: something is focused on arrival, d-pad walks the single row, `x`/`y`/`RT` add to the cart.
28. D-pad down to the destination picker; left/right changes the destination and **does not** move the focus column (`ui` D6's trap, second exercise).
29. `a`/`KC_F` on Accept → the purchase runs, the cart clears, focus lands somewhere real. `b` closes; `LB` still opens VON.
30. Open the map, focus the site icon, read the info panel with the pad alone.

**Play-test C — dedicated server + JIP:**

31. Two clients. Client 1 buys a site. **Client 2's map icon turns green and its actions update** without reopening anything.
32. Client 1 sets the site private; Client 2's storage action becomes disabled-with-a-reason, and a forged open attempt is refused by the server.
33. **JIP:** Client 3 joins after all of the above → sees the correct owner, privacy, colour and stock on first map open, with no action taken.
34. Two clients buy the same unowned site within the same second → exactly one succeeds; the other is refused with `#OVT-ProdSite_AlreadyOwned`, and no money is taken from the loser.
35. Two clients buy stock from the same site simultaneously → both succeed or one is refused; the site's stock afterwards is exactly right.

### Bug-report candidates for the orchestrator — do not file from this plan

- `OVT_TEST_Init_ControllerSeam.c` hard-codes `"all 10 asserted controller components"` in a `PrintFormat` two lines above a list that must grow with every new controller component. Every feature that adds one has to remember; a `Count()`-derived message would not need remembering.
- `OVT_DifficultySettings.realEstateCostMultiplier` (`:123`) has had no reader since it was written. This feature is its first; the real-estate buy price itself (`OVT_RealEstateManagerComponent.GetBuyPrice`) still ignores it, which is either a latent bug or a deliberate omission nobody recorded.

---

## 7. Testing Strategy

**Logic tier — `OVT_TEST_Logic_ProductionRules.c`** (world-free, `new`-built):

| Case | Claim | Proof it can fail |
|---|---|---|
| `SitePriceIsEightyPercentFloorOne` | `round(live × 0.8)`, and a live price of 1 still costs 1 | drop the `Math.Max(1, …)` |
| `SitePriceIsNotTheSellRatio` | 0.8 and 0.5 produce different answers at every live price in the table | wire the port ratio in |
| `BuyCostScalesAndFloors` | `round(base × mult)` across all five difficulty multipliers; never 0 | drop the floor |
| `AccessUnownedIsRefused` | `owner == ""` refuses every viewer, including `""` | return true for an empty owner |
| `AccessOwnerAlwaysAllowed` | the owner passes whether private or public | consult `isPrivate` first |
| `AccessPublicAllowsStrangers` | `!isPrivate` admits a different id | reverse the flag |
| `AccessPrivateRefusesStrangers` | private + different id refuses; **an empty viewer id never matches an empty owner** | compare with `==` alone |
| `AccessResistanceAllowsEveryone` | `"resistance"` admits any viewer | treat it as a plain id |
| `PrivacyTogglingIsOwnerOrOfficer` | owner yes; stranger no; resistance-owned needs the officer flag; unowned always no | drop the unowned guard |
| `ProduceAccumulatesFraction` | 0.5/hour over 1,2,3,4 hours yields 0,1,1,2 with the carry tracked | truncate the carry each tick |
| `ProduceIsZeroForNonPositive` | rate 0, rate −1, hours 0, hours −5 all yield 0 and leave the carry untouched | drop a guard |
| `ProduceClampsHugeSkips` | a 100,000-hour skip is clamped to `MAX_SKIP_HOURS` and returns a sane int | let it overflow |
| `ProduceCarryStaysInUnitRange` | after every call the carry is `>= 0` and `< 1` | forget the subtraction |
| `FitProductionPausesWhenFull` | 0 free litres fits nothing; a partial gap fits the whole units only; `-1` free fits everything | divide the wrong way |
| `FitProductionRejectsBadLitres` | `litresPerUnit <= 0` fits nothing | divide by zero |
| `ShouldProduceOncePerHour` | a second call in the same hour is a no-op; the next hour is not | compare `>` instead of `!=` |
| `ColourStateHasThreeAnswers` | unowned 0; yours/public/resistance 1; private-not-yours 2 | collapse two states |

**Init tier — `OVT_TEST_Init_ProductionSeam.c`:** the manager resolves through `OVT_Global.GetProduction()`; the test world's Sawmill (added in Phase 2) is discovered as exactly one record, unowned; its entity resolves an `OVT_ResourceProductionComponent` whose resource id is known to `OVT_Global.GetResources().GetDefs()` and a store at 20000 litres; `ProduceForHours(1)` raises the ledger by the authored rate and a second call in the same hour does not; `OVT_ControllerComponent<OVT_ResourceProductionRequestComponent>.Get()` resolves **and sits on this player's controller entity**; a server-side buy sets the owner, zeroes the stock and takes the money; the five access-predicate states resolve through `MayUseHolder` as §3.7 says. Polls are preconditions with a named failure on expiry, never retries; no `maxAttempts`.

**Persistence tier — appended to `OVT_TEST_PersistenceRoundTripSuite.c`**, both sorted after `…_Capability_…`, dirtying state through the public facade before reloading:

| Case | Subject |
|---|---|
| `…_ProductionSiteOwnership_RoundTrips` | owner, `isPrivate` and a non-zero fractional carry survive a reload |
| `…_ProductionSiteStock_RoundTrips` | the site's stock is re-applied onto the entity's store, and a second `ApplyStaged` is idempotent |

**Campaign tier — one case:** `…_ProductionSiteResistanceBuy_MovesTreasuryAndOwner` — fund the treasury, buy the test-world Sawmill server-side with `useResistanceFunds`, assert the treasury fell by `BuyCost(base, multiplier)`, the owner is `"resistance"`, the stock is zero and `MayAccessStore` answers true for an unrelated persistent id.

**What the automated spine cannot reach** — and therefore what the play-test gates exist for:

- **All multiplayer and JIP behaviour** (steps 31–35), including the two concurrent-buy races. The suites run one machine.
- **All UI and focus behaviour** (steps 26–30), including `ui`'s picker trap on a second consumer.
- **The map** — icon colour, alpha, zoom visibility and the info panel are drawn, not asserted (steps 15, 30).
- **The real sleep path** — the suites can call `HandleTimeSkip` directly but not exercise `OVT_SleepService`'s ordering against a moving clock (step 16).
- **Balance.** Every rate, price and capacity in §3.3 is a starting value; only steps 15–25 can say whether a sawmill is worth 8,000.
- **Workbench prefab resolution** and the atlas re-import.

---

## 8. Dependencies

**Consumed, unmodified:**

- `logistics/ui` — `OVT_TransferContext` + both models + `TransferMenu.layout` `{6A8E2C1000000001}` + `ActionContext OverthrowTransferContext`. **Must not be modified** (I1).
- `logistics/resources` — `OVT_ResourceStoreComponent`, `OVT_ResourceLedger`, `OVT_ResourceDefs`, `OVT_ResourceUtils` (+ `OVT_ResourceHolderQuery`), `OVT_ResourceManagerComponent.GetPrice/GetDefs`, `OVT_ResourceTransferContext`, the `crate` map quad. Read-only except the three files named in I4.
- `logistics/storage` — nothing. Named only because a site's storage is an `OVT_ResourceStoreComponent` and **never** an `OVT_StorageComponent` (the epic's two-ledger wall).
- `economy` — `PlayerHasMoney`, `TakePlayerMoneyPersistentId`, `TakePlayerMoney`, `ResistanceHasMoney`, `TakeResistanceMoney`, `GetPersistentIDFromPlayerID`, `GetPlayerName`. Read-only (I2).
- `economy` — `OVT_RealEstateManagerComponent` is **only** a shape reference (`OVT_WarehouseData`, `RpcAsk_BuyBuilding`). Not called, not modified (I2).
- `resistance` — `OVT_ResistanceFactionManager.IsOfficer(playerId)`.
- `core/controller-migration` — `OVT_OverthrowController`, `OVT_ControllerRequestComponent`, `OVT_ControllerComponent<T>.Get()`, `OVT_ComponentFinder`.
- `core/damage` — `OVT_StructureDamage.IsUsable` (through the untouched part of the gate ladder). **Do not modify any `core/damage` file.**
- `core/persistence` — `ScriptedComponentSerializer`, the game-mode `EntityPersistenceConfig`.
- `map` — `OVT_MapLocationType`, `OVT_MapLocationData`, `OVT_MapDataKeys`, `OverthrowMap.conf`. `OVT_MapMarkerComponent` is **not** used (D8).
- `core` — `OVT_TimeAndWeatherHandlerComponent.GetDayTimeMultiplier()`, `OVT_SleepService` (one call added).

**Modified:** `OVT_ResourceRequestComponent.c`, `OVT_ResourcesConfig.c`, `OVT_OpenResourceStoreAction.c`, `OVT_OverthrowConfigComponent.c`, `OVT_OverthrowGameMode.c`, `OVT_Global.c`, `OVT_SleepService.c`, `OVT_TEST_Init_ControllerSeam.c`, `resources.conf`, `OverthrowMap.conf`, `Overthrow.conf`, `overthrow_mapicons.imageset`, `localization_Overthrow.st`, three prefabs, one world layer, two epic docs.

**New:** everything marked NEW in §3.1.

**Downstream:** nothing is planned against this feature. Production **chains** (a later feature) will want `OVT_ResourceProductionComponent` to grow an input list and the manager's `ProduceForHours` to consume before it produces — both are additive, and neither is designed for here.

---

## 9. Risks & Mitigation

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| R1 | **Client and server access gates drift** — the action says yes where `MayUseHolder` says no, or the reverse | Medium | A visible action that always fails, or a private site readable by anyone | D5: one pure predicate, two callers, a Logic case per state and a DoD grep (§6 step 5) that fails if anyone re-implements the comparison inline |
| R2 | **Position matching mis-addresses a site** — two sites within 10 m, or a world edit that moves one | Low–Medium | An RPC or a save record lands on the wrong site | 10 m with **squared** comparison and nearest-wins; an unmatched persisted record is dropped with a named ERROR rather than guessed at; the map author is told (help docs) to keep sites ≥ 20 m apart |
| R3 | **The fractional carry drifts or resets** — binary32, or a save that drops it | Medium | Sub-1 rates silently produce nothing | The carry never exceeds 1 and is re-derived from a fresh sum each tick; `Produce` has three Logic cases including the range invariant; the round-trip case dirties a **non-zero** carry specifically |
| R4 | **Persisted stock is applied before the site entities exist** | **High** (deserialization runs while the world is being built) | Silent total loss of every site's stock on every load | D9's staging: the serializer never touches an entity, the manager applies after discovery, one bounded retry, then a named ERROR per record. `ApplyStaged` is idempotent, so the retry cannot double-apply |
| R5 | **The map quads do not exist** — art is owed and the `.edds` re-import is a manual Workbench step | **Certain** at first commit | Sites render with no icon at all | `GetIconName` falls back to the shipped `crate` quad, and the `.conf` authors that fallback; a missing quad is a cosmetic downgrade, never an invisible site |
| R6 | **`SITE_BUY` money maths overflows or goes negative** — `PlayerHasMoney` accepts a negative amount and `TakePlayerMoney` of a negative **pays** the player | Medium | An exploit that prints money | The same `moneyTotal < 0` tripwire the import branch already carries, plus `MAX_LINE_QUANTITY`, plus the stock bound, plus server-side re-derivation of the price at Commit. Three independent bounds, all on the server |
| R7 | **`OVT_ResourceRequestComponent.c` moves under this feature** — `resources` is Ready-for-Review, not closed, and concurrent sessions commit to this tree | **High** | Merge pain on the single most edited file | Every edit is additive and named with a `file:line`; the six edits are in one phase; re-baseline before Phases 4 and 5; if `resources` review work lands first, re-read the ladder before touching it |
| R8 | **Players cannot find a mesh-less site on foot** | Medium | The feature is only usable through the map | D12: 8 m action radius, a map icon at town zoom, and a one-line prefab edit reserved if play-test step 17 is awkward. The asset choice stays with the user |
| R9 | **The hour latch is wrong after a load or a sleep** | Medium | A double batch on the first hour after loading, or a whole day of production lost | `AssertHourLatchFromClock()` at Init (BUG-183's shape), `ShouldProduce` compares against the latched hour rather than a period, and `HandleTimeSkip` re-asserts from the **pre-skip** clock — the three-step order `OVT_EconomyManagerComponent.HandleTimeSkip` documents |
| R10 | **Two players buy the same site in the same second** | Low | Both charged, one owner | Both asks run on the server, serially; the second finds `owner != ""` and refuses at ladder step 3 **before** any money moves. Play-test step 34 is the proof |
| R11 | **A site accumulates a campaign's worth of stock and unbalances the economy** | Medium | An unowned sawmill is worth more than the base beside it | D3: capacity is the throttle and production pauses when full; 20 m³ is ~67 hours of timber; every number is an attribute and retunable without a script change |
| R12 | **The controller seam's hard-coded "10"** is not updated with the new component | Medium | A passing test whose message lies | Called out as an explicit Phase 3 task and an acceptance criterion, and filed as a bug-report candidate (§6) so the next feature does not hit it |
| R13 | **`Clear()` fires no invoker**, so a cleared store is not republished and clients keep showing the old stock | Medium | A bought site appears to still hold its stock on every other machine | `ClearSiteStock` is the only caller and always follows `Clear()` with `PublishContents()`; the Init case asserts the packed string is empty afterwards |
| R14 | **Concurrent sessions** change the tree between phases | Medium | Stale line references, a phase built against a moved seam | Re-baseline before every phase; every claim here carries a `file:line`; the GUID series is re-verified immediately before authoring |

---

## Quality Bar

**Backend**

- **B1 — Data integrity, provable by reading.** Every purchase validates the whole cart against live state *before* mutating either side; nothing clamps; a refused request mutates nothing. The worst outcome of a mid-request crash is a refused purchase, never a duplicated or lost line and never a charge for goods not delivered.
- **B2 — Server-authoritative without exception.** Every ownership, privacy, ledger and production mutation runs behind `Replication.IsServer()`. Client-side predicates (action labels, `ValidateCart`, the map colour) are advisory and the server re-derives all of them — including the price.
- **B3 — Money is bounded three ways.** Quantity bound, stock bound and a negative-total tripwire, all on the server, all before `PlayerHasMoney` is consulted.
- **B4 — Rejections are visible.** Every refused request answers with a key the player sees. Silent returns and log-only rejections are the shape being avoided.
- **B5 — One writer per replicated field.** `PublishContents()` is the only writer of the store's prop; `SetSiteOwner`/`SetSitePrivacy` are the only writers of the two record fields, and each broadcasts exactly once.
- **B6 — Persistence never applies a failed read.** The single `Read()` return is checked; a fault aborts with a named ERROR and leaves live state untouched. The persisted record class is frozen and separate from the live one.
- **B7 — The wire is auditable.** Three new RPCs plus one appended op, each with its arity written down and checked against its handler, because the compiler will not do it.
- **B8 — No exact float comparison decides anything.** Litres are integers, distances are squared, the carry is bounded, and the only floats are the authored rate and the difficulty multiplier.
- **B9 — Per-call state only.** The holder query is `new`-ed per call, never a static accumulator.

**UI**

- **B10 — Gamepad parity.** Every step — the five site actions, the buy screen, the destination picker, Accept, and the map info panel — is reachable with the d-pad, `x`, `y`, `RT`, `KC_F`/`left_trigger` and `b` alone.
- **B11 — Focus is never lost**, including across a live stock refresh with the cart open and across an Accept.
- **B12 — The labels never lie.** The buy price, the stock figure and the sale price all read from replicated state and are correct within one replication tick on every client. The price shown is the price charged.
- **B13 — Refusals are shown, not hidden.** A row or action that cannot be used is visible and disabled with a reason; hiding is reserved for genuinely irrelevant actions (buy actions on an owned site).
- **B14 — No `ALWAYS_TOP` focusable widget**, and no hover target grown through the widget tree.

---

## Agent Routing Summary

| Phase | Agent | Why |
|---|---|---|
| 1 — pure spine + icon field | `component-developer` | Pure statics and Logic cases; one appended config field |
| **2 — manager + prefabs + drip** | **`component-developer-advanced`** ⚠️ | A new replicated manager with a JIP payload, two broadcasts, a world-query discovery pass, an hour latch with a documented load-time trap, a new prefab family and an edit to the sleep path |
| **3 — ownership + privacy + difficulty** | **`network-specialist-advanced`** ⚠️ | Three RPCs with money on both branches, an officer gate, a clear-on-purchase ordering rule, and BUG-090's compile blind spot |
| **4 — `SITE_BUY` + buy screen** | **`network-specialist-advanced`** then `ui-developer` ⚠️ | A surgical refactor of a shipped six-step server gate plus a money branch inside the mod's most safety-critical Commit path — then a consumer on rails laid by `ui` that **must not touch the base** |
| 5 — access predicate | `component-developer` | Two small call sites and one prefab entry, but the *ordering* matters: it closes the window Phase 2 deliberately left |
| 6 — map location type + icons | `ui-developer` | A location type, a `.conf` block and an imageset — all on shipped rails |
| **7 — persistence + round trip** | **`component-developer-advanced`** ⚠️ | Property-name-keyed serialization with a silent total-loss failure mode and a load-ordering hazard that is *certain*, not hypothetical |
| 8 — loc, epic docs, help & wiki | main thread + `help-docs-sync` | `.st` structural safety (unbalanced braces = data loss) and the wiki's known write failure modes |

**Parallelism:** Phase 6 may run in parallel with Phase 5 (disjoint files) once Phases 3 and 4 land. Phase 7 depends on Phases 2 and 3. Phase 8 is last. Phases 4 and 5 both edit `OVT_ResourceRequestComponent.c` and must **not** run in parallel.

**Total estimate:** 31–40 h across 8 phases, 4 of them advanced.
