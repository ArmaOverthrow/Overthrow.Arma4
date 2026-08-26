# Resources — Implementation Plan

**Status:** ✅ Ready for Review — all 11 phases + a cross-phase review built 2026-08-21
**Started:** 2026-08-21
**Target Completion:** TBD
**Last Updated:** 2026-08-21 (built; final gate Logic 247/247 · Init 174/175 · PersistenceRoundTrip 40/40 · Campaign 18/18 · Persistence 13/13)

**Epic:** `logistics` (feature #3 of 4 — see `docs/features/logistics/epic-overview.md`)
**Requirements:** `docs/features/logistics/resources/requirements.md` (authoritative for scope; sections A–F + Out of Scope, plus the seven user decisions of 2026-08-21 restated in §5)
**Approach:** **A — a parallel resource spine on `storage`'s seams.** Own pure `OVT_ResourceLedger` + `OVT_ResourceStoreComponent` (m³-capped holder, **contents replicated**) + `OVT_ResourceManagerComponent` (definitions, live prices, drift) + `OVT_ResourceRequestComponent` on the controller + own serializers. **Nothing in `storage` is modified** — the epic's "two ledgers, deliberately" is a wall. One new `logistics/ui` consumer (`OVT_ResourceTransferContext`) on the closed eight hooks; resources join the port as a **category** inside the shipped Import/Export modes; `BuildItem` **branches** into a construction site; the warehouse gains a store and becomes a buildable.
**Branch:** `v1.5` (concurrent sessions exist on this tree — re-baseline before every phase; every citation below carries a file:line so drift is detectable)

---

## 1. Executive Summary

Overthrow's mid-game buildings cost only money. `OVT_ResistanceFactionManager.BuildItem` (`:838-923`) takes `m_Config.GetBuildableCost(buildable)` (`OVT_OverthrowConfigComponent.c:331-334`) out of the player's wallet and spawns a garage in the same frame. There is no logistics, no supply line, and nothing for a truck to do that a wallet cannot do faster.

This feature adds the second axis. A config-driven set of resources (`resources.conf`, MVP Timber / Cement / Steel / Hardware) is bought at the port at a **drifting, war-biased, persisted price**, hauled in a **volume-capped cargo store** authored on a new same-GUID delta of vanilla `Wheeled_Truck_Base.et` `{E03D5609EEA6E03D}` so every truck in the game hauls, dropped as **merging crate piles** that show on the map and can be inspected, stored in **warehouses** beside `storage`'s item ledger, and consumed to raise **construction sites** into finished buildings through the same build path that exists today. The warehouse itself becomes the first resource-costed buildable.

Three commitments shape everything below.

1. **Two ledgers, one wall.** `storage` owns counts; this feature owns litres. No file under `OVT_StorageLedger` / `OVT_StorageComponent` / `OVT_StorageRequestComponent` / `OVT_StorageContext` / `OVT_TransferContext` / the two transfer models is touched (§6 I1, with a `git diff --exit-code` that proves it). Where the two meet — the warehouse, the truck, the port screen — they meet as two components on one entity and two categories on one screen.
2. **Contents replicate; the client never guesses.** A resource ledger is at most a handful of lines, so the whole thing rides one `RplProp` string (§3.3). That single decision removes the pull-on-open protocol, the async first frame, the listen-host re-entrancy trap and the "requirements readout needs a server round trip" problem in one go — the cargo HUD, the pile inspect label, the map rows and the Build action's shortfall text are all local reads.
3. **Prices are state.** Overthrow has never persisted a price (`OVT_EconomyManagerComponent.SetPrice`, `:668-671`, is a plain write called only from `BuildResourceDatabase`). The live price is stored, replicated, JIP-correct, persisted and drifted on the shipped 6-hour hour-gate idiom (`CheckUpdate`, `:159-217`). Nothing reads the config base as if it were live.

Volume is the only constraint; weight is data and display (epic decision). Difficulty scales three new `Economy` multipliers. The illegal gate ships built and unexercised, proven by a test that flips a flag.

---

## 2. Goals

### Primary

1. **A config-driven resource set** — adding, removing or re-pricing a resource is a `.conf` edit, no script change (`OVT_BuildablesConfig` shape, loaded by the `LoadConfigs` idiom at `OVT_ResistanceFactionManager.c:229-251`).
2. **A pure, Logic-testable ledger** with the capacity **passed in**, never held, so piles and warehouses are unlimited by applying no cap.
3. **Every truck hauls** — one component on one new same-GUID delta of `Wheeled_Truck_Base.et`, inherited by both vanilla truck families (`M923A1.et` `{9A0D72816DFFDB7F}`, `Ural4320.et` `{4597626AF36C0858}`) and therefore by every truck any system spawns.
4. **Merging crate piles** with inspect, map markers and persistence; unlimited capacity by construction.
5. **Port trade** as a category inside the shipped `OVT_PortContext` Import/Export modes — no new screen, no new `ActionContext`, capacity-aware, `importable`-gated, illegal-gated, at the **live** price.
6. **Price drift** — server-only, one step per 6-hour window, bounded to a band around base, random walk plus a war-pressure term, volatility scaling the step and a level multiplier applied after the clamp, with a stated and asserted composition order.
7. **Construction sites** — `OVT_Buildable` gains an optional requirement list; an empty list is byte-identical to today; a non-empty list places a site (money charged then), and Build consumes nearby piles and re-enters the *same* finish path (ownership, base association, `m_OnBuild`/XP, `Track`).
8. **Position-based helpers** — `NearbyAvailability(vector pos, …)` / `Consume(vector pos, …)` and a reusable readout, callable by `building-repair` against a `core/damage` ruin.
9. **Warehouses hold resources** and **become a buildable**, registered through `OVT_RealEstateManagerComponent.SetOwnerPersistentId` (`:265-296`) so every consumer sees a real warehouse record.
10. **Everything survives save/continue and JIP** — truck load, piles, sites, warehouse stock and the live price table.

### Secondary

1. **One vocabulary for "somewhere resources live"** — truck, pile, warehouse are all `OVT_ResourceStoreComponent` behind one `RplId`. No second addressing scheme.
2. **Integer litres internally** so no capacity decision ever depends on a binary32 rounding (§5 D3).
3. **A pile registry the mod already owns** — `OVT_MapMarkerComponent` self-registers runtime-spawned entities on every machine (`:26-32`), so no replicated registry is invented.
4. **The Buildable persistence config gains the serializers a *built* holder needs** — an oversight this feature is the first to hit (§3.11).

### Explicitly out of scope

Everything in the requirements' Out of Scope section, restated only where an implementer might reach for it anyway:

- **No production, salvage or passive supply.** Import is the only source. No buying directly into a warehouse or a pile.
- **No weight effect on handling.** `Physics.SetMass` is never called. Weight is a config number that is displayed.
- **No continuous held load/unload actions.** The transfer screen does chosen-quantity moves; Overthrow authors only `Duration 0` actions today. Deferred, not designed out (§5 D12).
- **No build timers, progress bars, multi-stage construction, or banking partial deliveries into a site.** A site is satisfied or it is not.
- **No consuming from a truck store or a warehouse at a site** — piles only (requirement D).
- **No per-resource pile meshes, no per-port prices, no player supply-and-demand, no drift for non-resource items.**
- **No refunds** anywhere, including removing a site (no refund path exists in the mod).
- **No new `OVT_TransferContext` hook.** §3.6 shows every consumer landing on the shipped eight. If an implementer finds one that does not, that is a plan defect — raise it, do not widen the base.
- **No change to `storage`.** Not a field, not a branch, not a "while I'm here".

---

## 3. Architecture Overview

### 3.1 Where each piece lives

```
Scripts/Game/Data/                                   PURE, Logic-tier testable, no world
├── OVT_ResourceLedger.c            NEW   ledger + OVT_ResourceAmount record
├── OVT_ResourceDefs.c              NEW   parallel-array definition table (ids/litres/kg/base price/flags)
├── OVT_ResourcePack.c              NEW   packed-wire encode/decode (index:qty pairs)
└── OVT_ResourceRules.c             NEW   statics: drift step, merge selection, requirement maths, readout text

Scripts/Game/Configuration/
├── OVT_ResourcesConfig.c           NEW   OVT_ResourcesConfig + OVT_Resource
└── OVT_BuildablesConfig.c          TOUCH + m_aResourceRequirements, + m_SitePrefab, + OVT_BuildableResourceRequirement

Scripts/Game/Components/
├── OVT_ResourceStoreComponent.c    NEW   the holder: ledger + litre cap + ONE RplProp
├── OVT_ResourcePileComponent.c     NEW   marker (persistence rule target + map/inspect identity)
├── OVT_ConstructionSiteComponent.c NEW   buildable index + prefab index + placement
└── Controller/
    └── OVT_ResourceRequestComponent.c  NEW  : OVT_ControllerRequestComponent — 6 RPCs + MayUseHolder

Scripts/Game/Utilities/
└── OVT_ResourceUtils.c             NEW   RplId↔holder, OVT_ResourceHolderQuery, OVT_ResourcePileQuery (per-call)

Scripts/Game/GameMode/Managers/
└── OVT_ResourceManagerComponent.c  NEW   definitions, live prices, drift tick, pile spawn/merge

Scripts/Game/GameMode/Managers/Factions/OVT_ResistanceFactionManager.c   TOUCH
    BuildItem() split into BuildItem() + FinishBuild(…, bool charge); + CompleteSite(); + requirement helpers

Scripts/Game/Global/OVT_Global.c    TOUCH  + GetResources()
Scripts/Game/Configuration/OVT_DifficultySettings.c        TOUCH  + 3 Economy fields
Scripts/Game/.../OVT_OverthrowConfigComponent.c            TOUCH  + 3 accessors, CONFIG_STREAM_VERSION 5→6

Scripts/Game/UI/Context/
├── OVT_ResourceTransferContext.c   NEW   OVT_TransferContext consumer (Take + Put, 8 hooks)
├── OVT_PortContext.c               TOUCH + resource category in both shipped modes
└── OVT_BuildContext.c              TOUCH + requirement rows on the card details, + town-control reason

Scripts/Game/UI/HUD/OVT_CargoInfo.c            NEW   : SCR_InfoDisplay (OVT_ProgressInfo is the template)
Scripts/Game/UI/Map/LocationTypes/OVT_MapLocationResourcePile.c  NEW
Scripts/Game/Components/Map/OVT_MapMarkerComponent.c   TOUCH  + RESOURCE_PILE enum value (append-only)

Scripts/Game/UserActions/
├── OVT_OpenResourceStoreAction.c   NEW   truck "Cargo (x/y m³)", pile, warehouse — one class, three hosts
├── OVT_SiteRequirementsAction.c    NEW   readout dialog
└── OVT_BuildFromSiteAction.c       NEW   "Build X" / "Need 12 Cement"

Scripts/Game/Persistence/Serializers/Components/
├── OVT_ResourceStoreComponentSerializer.c   NEW
├── OVT_ConstructionSiteComponentSerializer.c NEW
└── OVT_ResourceManagerSerializer.c          NEW   (OVT_EconomyManagerSerializer is the template)

Prefabs/
├── Vehicles/Core/Wheeled_Truck_Base.et      NEW same-GUID delta {E03D5609EEA6E03D}  (store + HUD)
├── Vehicles/Wheeled/M923A1/M923A1_transport.et       TOUCH  cap override
├── Vehicles/Wheeled/Ural4320/Ural4320_transport.et   TOUCH  cap override
├── Vehicles/Wheeled/M923A1/OverthrowMobileFOB{,Deployed}.et  TOUCH  cap override
├── Props/Resources/OVT_ResourcePile.et      NEW   (: vanilla CrateStack_01_base.et)
├── Structures/OVT_ConstructionSite.et       NEW   (: vanilla ConcreteMixer_01.et)
├── Structures/Industrial/Houses/Warehouse_01/Warehouse_01_Base.et  TOUCH  + store, + action
├── Structures/Industrial/Houses/Warehouse_01/OVT_Warehouse.et      NEW   (: vanilla Warehouse_01.et {12310677867A85D4})
├── GameMode/OVT_OverthrowGameMode.et        TOUCH  + OVT_ResourceManagerComponent + m_rResourcesConfigFile
├── GameMode/OVT_OverthrowController.et      TOUCH  + OVT_ResourceRequestComponent (before the RplComponent, :48)
└── Characters/.../Character_Player.et       TOUCH  + OVT_ResourceTransferContext block

Configs/
├── Resistance/resources.conf                NEW
├── Resistance/buildables.conf               TOUCH  Garage/Helipad/Guard Tower requirements + Warehouse entry
├── Difficulty/Difficulty_{Easy,Hard,Extreme,Insane}.conf  TOUCH  3 fields where they differ from default
├── Map/OverthrowMap.conf                    TOUCH  + OVT_MapLocationResourcePile block
└── Systems/Persistence/Overthrow.conf       TOUCH  5 edits (§3.11), GUIDs from 6B0E7A7…

UI/
├── Imagesets/overthrow_mapicons.imageset    TOUCH  + one crate quad
├── Textures/Icons/resource_*.edds           NEW    4 icons (one placeholder reused is acceptable in MVP)
└── Layouts/HUD/CargoInfo.layout             NEW

Language/localization_Overthrow.st           TOUCH  ~45 new keys

Scripts/Game/Tests/TestSuites/
├── Logic/OVT_TEST_Logic_ResourceLedger.c    NEW
├── Logic/OVT_TEST_Logic_ResourceRules.c     NEW
├── Init/OVT_TEST_Init_ResourceSeam.c        NEW
├── Persistence/OVT_TEST_PersistenceRoundTripSuite.c   + 5 cases
└── Campaign/…                               + 2 cases
```

**Reserved GUID series: `6A8E2E…`** for prefab/layout/conf/widget instance GUIDs, **`6B0E7A7…`** for `Configs/Systems/Persistence/Overthrow.conf` entries. Both re-verified 0 hits in both trees on 2026-08-21 (`grep -rl 6A8E2E .` → 0 in `Overthrow.Arma4`, 0 in `ArmaReforger`; `grep -rl 6B0E7A7 .` → 0). Allocation: `6A8E2E0…` prefab component/action instances, `6A8E2E1…` `Character_Player.et` context block, `6A8E2E2…` map/imageset/HUD, `6A8E2E3…` spare. **Inherited component GUIDs are copied, never minted** — the truck delta's HUD component is `SCR_BaseHUDComponent "{53151CEE6C0A409F}"` (`Wheeled_Base.et:12`), the transport trucks' action manager is `ActionsManagerComponent "{C97BE5489221AE18}"` (`M923A1_transport.et:8`), the warehouse delta's is `"{6A8E2D0000000030}"` (`Warehouse_01_Base.et:8`).

### 3.2 The ledger and the pure spine

**`OVT_ResourceLedger : Managed`** — id → quantity. Pure: no world, no manager, no engine type in a signature. **Capacity is passed in, never held.**

| Method | Contract |
|---|---|
| `int Add(string id, int qty, OVT_ResourceDefs defs, int capacityLitres)` | Returns how many **fitted**. `capacityLitres < 0` = unlimited. Ignores empty id, `qty <= 0`, and an id `defs` does not know. |
| `int Take(string id, int qty)` | Returns how many were taken; clamps to held; **a line that reaches zero is removed** (copied from `OVT_StorageLedger.Take`, `:90`). |
| `int Count(string id)` | 0 when absent. |
| `int TotalLitres(OVT_ResourceDefs defs)` | Maintained as a field, O(1) — action labels and the HUD poll it every frame. |
| `float TotalWeightKg(OVT_ResourceDefs defs)` | Display only. Iterates; nothing polls it per frame. |
| `int FreeLitres(OVT_ResourceDefs defs, int capacityLitres)` | `capacityLitres < 0` → `int.MAX`; else `Math.Max(0, capacity − total)`. |
| `bool WouldFit(string id, int qty, OVT_ResourceDefs defs, int capacityLitres)` | Exact integer comparison; no epsilon anywhere. |
| `int LineCount()` / `void GetLines(out array<string> ids, out array<int> qty)` | Enumeration for the wire, the serializer and the UI. |
| `void Clear()` | Fires nothing (same rule as `OVT_StorageLedger.Clear`, `:188` — the caller republishes). |
| `ref ScriptInvoker m_OnChanged` | `(string id, int newQty)`. Lazily allocated. |

`OVT_ResourceAmount : Managed { string m_sId; int m_iQuantity; }` is the enumeration/argument record and the currency of every helper signature below. It is **not** the persisted record (§3.11 freezes a separate one).

**`OVT_ResourceDefs : Managed`** — the pure definition table the manager builds once from `resources.conf`: parallel `array<string> m_aIds`, `array<int> m_aLitresPerUnit`, `array<float> m_aKgPerUnit`, `array<int> m_aBasePrice`, `array<int> m_aImportable` / `m_aIllegal` (0/1 ints — **never `array<bool>`**, the persistence rule that also applies to any parallel-array carrier). Lookup `IndexOf(string id)`, `IdAt(int)`, `Count()`. A Logic case `new`s one and fills it by hand — which is what keeps every rule below out of the manager and inside the cheapest test tier. **The Logic tier forbids the manager accessor and game-mode getter identifiers appearing anywhere under `TestSuites/Logic/`, even in a comment**, so no pure signature may name a manager.

**`OVT_ResourcePack`** — `static string Encode(OVT_ResourceLedger, OVT_ResourceDefs)` / `static bool Decode(string packed, OVT_ResourceDefs, notnull OVT_ResourceLedger out)`. Format `"idx:qty|idx:qty"` using the definition **index**, which is identical on every machine because `resources.conf` is a mod file. Empty ledger → `""`. Decode rejects a malformed token by returning false and leaving the ledger untouched.

**`OVT_ResourceRules`** — statics, pure, every one of them a Logic case:

```
static int   DriftStep(int basePrice, int current, float roll, float pressure,
                       float volatility, float stepFraction, float bandMin, float bandMax)
static int   ApplyLevelMultiplier(int storedPrice, float levelMultiplier)   // read-time, after the clamp
static int   SellPrice(int livePrice, float sellRatio)
static float WarPressure(float threat, float threatReference, float controlledPortFraction)
static int   SelectMergeTarget(array<float> sqDistances, array<int> litres, float radiusSq)  // -1 = none
static int   ScaleRequirement(int baseQty, float multiplier)                // never 0 for a non-zero input
static bool  IsSatisfied(array<ref OVT_ResourceAmount> need,
                         array<ref OVT_ResourceAmount> have, out string shortId)
static void  SortPilesForConsumption(array<float> sqDistances, out array<int> order)  // stable, nearest first
static string FormatReadout(array<ref OVT_ResourceAmount> need,
                            array<ref OVT_ResourceAmount> have, OVT_ResourceDefs defs)
```

### 3.3 The holder — `OVT_ResourceStoreComponent : OVT_Component`

```
[ComponentEditorProps(category: "Overthrow/Components")]
class OVT_ResourceStoreComponentClass : OVT_ComponentClass {};
class OVT_ResourceStoreComponent : OVT_Component
```

| Member | Kind | Notes |
|---|---|---|
| `m_fCargoVolume` | `[Attribute(defvalue: "-1")]` float, m³ | `-1` = unlimited (pile, warehouse). Converted once in `OnPostInit` to `m_iCapacityLitres = Math.Round(m3 * 1000)`. |
| `m_sDefaultNameKey` | `[Attribute]` string | `"#OVT-Resource_Pile"` on the pile, `"#OVT-Warehouse"` on the warehouse. |
| `m_Ledger` | `ref OVT_ResourceLedger` | Server truth; on a client it is a **mirror** rebuilt from the packed prop. |
| `[RplProp(onRplName: "OnContentsChanged")] m_sPacked` | string | **The entire replicated surface.** |

- **Capacity is NOT replicated.** It is a prefab attribute, so every machine reads the same number from the same file — unlike `storage`, whose AUTO mode resolved from a server-built economy catalogue and therefore had to replicate the result (`storage` D7). A corollary worth having: the *capacity-0-on-the-spawn-frame* trap does not apply here, because there is no deferred resolve.
- **`PublishContents()` is the only writer.** It re-encodes `m_sPacked`, calls `Replication.BumpMe()` **once**, invokes `m_OnContentsChanged`, and calls `EnsureTracked()` when the ledger is non-empty (buildings and props are not tracked by default — the `OVT_StorageComponent.PublishCount` shape, `:183-196`). It is called once per finished request, never per line.
- **`OnPostInit` guards a worldless owner** — `if (!owner || !owner.GetWorld()) return;`, the same guard `OVT_StorageComponent.c:130-133` carries. `ItemPreview` spawns a worldless instance and the warehouse *is* previewed by the real-estate screen; without the guard every previewable prefab null-crashes.
- **`OnPostInit` asserts `GetRpl()`** and logs an ERROR naming the prefab if it is null (BUG-193 is exactly this found late). The pile and site prefabs therefore author an `RplComponent` explicitly (`OVT_AmmoBox_Base.et:123` is the precedent for a prop that needed one).
- Client-safe API: `GetLedger()` (mirror), `GetCapacityLitres()`, `GetUsedLitres()`, `GetFreeLitres()`, `GetDisplayName()`, `m_OnContentsChanged`. Server-only: `GetLedger()` for mutation, `PublishContents()`, `ApplyPersisted(array<ref OVT_PersistedResourceLine>)`.

**Hosts:**

| Prefab | Edit | Capacity |
|---|---|---|
| `Prefabs/Vehicles/Core/Wheeled_Truck_Base.et` **NEW same-GUID delta** `{E03D5609EEA6E03D}` | + store, + `SCR_BaseHUDComponent "{53151CEE6C0A409F}"` restating **all three** InfoDisplays | **15 m³** |
| `M923A1_transport.et`, `Ural4320_transport.et` | cap override | **20 m³** |
| `OverthrowMobileFOB.et`, `OverthrowMobileFOBDeployed.et` | cap override | **8 m³** (it is already carrying a base) |
| `Warehouse_01_Base.et` (shipped Overthrow delta) | + store, + a third action at Sort Priority 3 | **−1** |
| `Props/Resources/OVT_ResourcePile.et` **NEW** | store + `OVT_ResourcePileComponent` + `OVT_MapMarkerComponent` + `ActionsManagerComponent` + `RplComponent` | **−1** |

Cars, APCs and helicopters get **no** component and therefore cannot haul (requirement B: non-truck hauling is out of scope). The two vanilla truck families that inherit `Wheeled_Truck_Base.et` are `M923A1.et` and `Ural4320.et` (verified: those are the only two files in the vanilla tree whose header names it).

⚠️ **Restate the whole `InfoDisplays` array on the truck delta.** `Wheeled_Base.et:12-23` authors `OVT_WantedInfo "{59B70A6E01375E1B}"` and `OVT_EconomyInfo "{59B70A616D989C46}"`; the truck delta adds `OVT_CargoInfo` and must carry all three with the inherited GUIDs copied. That is correct whether the container merges arrays by entry GUID or replaces them — and the shipped evidence points at merge (`M923A1_transport.et:8-25` re-declares `ActionsManagerComponent` with only the Loot action, and `storage`'s `Vehicle_Base.et` actions still appeared on a transport truck in its play-test), but "correct under both" costs nothing.

### 3.4 The manager — `OVT_ResourceManagerComponent`

On `OVT_OverthrowGameMode.et` (append near `OVT_ResistanceFactionManager`, `:166-168`, whose `[Attribute] ResourceName m_rBuildablesConfigFile` this copies). `s_Instance` + `GetInstance()` + one accessor `OVT_Global.GetResources()` beside `GetEconomy()` (`OVT_Global.c:243-246`). Config load is `BaseContainerTools.LoadContainer` → `CreateInstanceFromContainer`, the `LoadConfigs()` idiom at `OVT_ResistanceFactionManager.c:229-251`.

**Owns:**

| State | Kind | Notes |
|---|---|---|
| `ref OVT_ResourcesConfig m_Config` + `ref OVT_ResourceDefs m_Defs` | server + client | Built in `OnPostInit` from `m_rResourcesConfigFile`. Identical on every machine. |
| `ref array<int> m_aCurrentPrice` | **server truth, replicated** | Index-aligned with `m_Defs`. Initialised to base on a new campaign. |
| `int m_iHourPricesDrifted` | server | The `m_iHourPaid*` latch (`OVT_EconomyManagerComponent.c:99-101`), init `-1`, asserted from the clock at start exactly like `AssertHourLatchesFromClock()` (`:314-325`). |
| tuning attributes | prefab | `m_fPriceBandMin` 0.5, `m_fPriceBandMax` 2.0, `m_fPriceStepFraction` 0.08, `m_fSellRatio` 0.5, `m_fMergeRadius` 6, `m_fSupplyRadius` 30, `m_fThreatReference` 2000, `m_rPilePrefab`, `m_rDefaultSitePrefab` |

**The drift tick.** Its own `CallLater(CheckPrices, ECONOMY_UPDATE_FREQUENCY / timeMul, true, GetOwner())` scheduled from `Init(IEntity)` — the exact shape and cadence of `OVT_EconomyManagerComponent.c:1484` (`ECONOMY_UPDATE_FREQUENCY = 60000`, `:81`; `timeMul` from `GetDayTimeMultiplier()`, `:1465-1469`). **Nothing in `economy` is modified.** `CheckPrices()`:

```
if (!Replication.IsServer()) return;
TimeContainer time = m_Time.GetTime();                          // no date read; TimeContainer has no day
if (time.m_iHours != 0 && != 6 && != 12 && != 18) return;
if (m_iHourPricesDrifted == time.m_iHours) return;              // one drift per window
m_iHourPricesDrifted = time.m_iHours;
float pressure   = OVT_ResourceRules.WarPressure(threat, m_fThreatReference, controlledPortFraction);
float volatility = OVT_Global.GetConfig().GetResourcePriceVolatility();
foreach (i) {
    float roll = Math.RandomFloat(-1, 1);
    int next = OVT_ResourceRules.DriftStep(base[i], m_aCurrentPrice[i], roll, pressure,
                                           volatility, m_fPriceStepFraction, m_fPriceBandMin, m_fPriceBandMax);
    if (next == m_aCurrentPrice[i]) continue;
    m_aCurrentPrice[i] = next;
    Rpc(RpcDo_SetPrice, i, next);                                // broadcast, arity 2
}
```

**`DriftStep`, exactly:**

```
delta = (roll + pressure) * stepFraction * volatility     // fraction of BASE, float
step  = Math.Round(basePrice * delta)                     // int
next  = current + step
lo    = Math.Round(basePrice * bandMin);  hi = Math.Round(basePrice * bandMax)
next  = Math.Clamp(next, lo, hi);  if (next < 1) next = 1
```

**`WarPressure`, exactly:** `Math.Clamp(Math.Clamp(threat / threatReference, 0, 1) − 0.5 * controlledPortFraction, −1, 1)`. `threat` is `OVT_OccupyingFactionManager.GetThreatFloat()` (`:1625`); `controlledPortFraction` is `count(ports where ResistanceControlsNearestPort(port.GetOrigin()) ) / GetAllPorts().Count()` (`OVT_EconomyManagerComponent.c:1018-1037`, `:1113-1116`) — cheap, ports are few, and it is the global answer requirement C wants (per-port variation is out of scope). **Direction: threat pushes prices UP, resistance port control pushes them DOWN.**

**Composition order — stated once and asserted in a Logic case:**

1. **Stored** price is the walked value, **clamped to the band**. Volatility scales `delta` and therefore the step; it never touches `lo`/`hi`.
2. **Live** price = `ApplyLevelMultiplier(stored, resourcePriceMultiplier)` = `Math.Max(1, Math.Round(stored * multiplier))`, evaluated **at read time, after the clamp**. On Hard the effective price may therefore exceed `base × bandMax` — deliberate, and the Logic case asserts it.
3. **Sell** price = `Math.Max(1, Math.Round(live * m_fSellRatio))`. One ratio, no second walk.

Reading the base as the live price is the predictable bug the epic names; `GetBasePrice(i)` exists only for the drift maths and the "relative to base" readout.

**Pile spawn/merge (server only).** `IEntity SpawnOrMergePile(vector pos, notnull array<ref OVT_ResourceAmount> amounts)`:
`OVT_ResourcePileQuery` (per-call, never a static accumulator — `OVT_InventoryManagerComponent.c:497` is the shared-accumulator defect this project keeps re-learning) collects piles within `m_fMergeRadius`; `OVT_ResourceRules.SelectMergeTarget` picks **nearest by squared distance, ties to the larger pile, stable** (squared distance, because `vector.Distance` is not correctly rounded and an exact-boundary decision would be a coin flip); on `-1` spawn `m_rPilePrefab` via `OVT_WorldUtils.SpawnEntityPrefab` (`:433`) at a terrain-snapped `pos` and `OVT_PersistenceTracking.Track(pile)` (`:37-48`). Then add, `PublishContents()`.

**Pile cleanup.** A pile whose ledger reaches 0 after a take or a Build consumption is deleted: `OVT_PersistenceTracking.Untrack(pile, false)` (`:222-238`) then `SCR_EntityHelper.DeleteEntityAndChildren`. Cleanup is server-only and happens in the same request that emptied it.

### 3.5 The request component and the wire — `OVT_ResourceRequestComponent`

`class OVT_ResourceRequestComponent : OVT_ControllerRequestComponent`, authored on `Prefabs/GameMode/OVT_OverthrowController.et` before the trailing `RplComponent` (`:48`). It **is** an `OVT_ControllerRequestComponent` (unlike `storage`'s), so it inherits `ResolveOwningPlayerId` (`:46`), `ResolveEntity(RplId)` (`:89`), `ShouldRespondLocally(int)` (`:136`) and `PlayerMayUseVehicleFor` (`:173`). Identity is never an RPC parameter.

**No progress component.** Every operation is ≤ a handful of map writes plus at most one entity spawn; there is nothing to chunk and nothing to show a bar for. `OVT_BaseServerProgressComponent` would buy a busy latch and a progress HUD this feature has no use for.

**Attributes:** `m_fHolderRadius` 25 (destination picker), `m_fUseRadius` 30 (matches `OVT_StorageRequestComponent.EXPORT_MAX_PORT_DISTANCE`/`m_fUseRadius`), `m_fPortRadius` 30, `m_iMaxCartLines` 16, `m_fUnloadOffset` 4.

#### RPC table — **hand-audited arities** (BUG-090: `Rpc()` is untyped variadic; a wrong count compiles clean and dies on the wire)

Client → server, `[RplRpc(RplChannel.Reliable, RplRcver.Server)] protected`:

| # | Signature | Arity | Meaning |
|---|---|---|---|
| 1 | `RpcAsk_TransferBegin(RplId source, RplId dest, int opKind, int seq, int lineCount)` | 5 | Opens a checkout. `opKind` = `EOVT_ResourceOp`. |
| 2 | `RpcAsk_TransferLine(int seq, int index, int resIndex, int qty)` | 4 | One cart line. `resIndex` is the definition index. |
| 3 | `RpcAsk_TransferCommit(int seq, int lineCount)` | 2 | `lineCount` repeated so a short stream is detectable. |
| 4 | `RpcAsk_BuildFromSite(RplId site)` | 1 | Consume nearby piles, complete the site. |

Server → owner, `[RplRpc(RplChannel.Reliable, RplRcver.Owner)] protected`:

| # | Signature | Arity |
|---|---|---|
| 5 | `RpcDo_TransferResult(int seq, int movedLitres, int earned, int spent)` | 4 |
| 6 | `RpcDo_ResourceError(int seq, string messageKey)` | 2 |

Server → all, `[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]` **on the manager, not here**:

| # | Signature | Arity |
|---|---|---|
| 7 | `RpcDo_SetPrice(int resIndex, int price)` | 2 |

`EOVT_ResourceOp { HOLDER_TO_HOLDER, HOLDER_TO_GROUND, PORT_IMPORT, PORT_EXPORT }` — **append-only**. Rules that are not negotiable:

- **No `array<…>` on any RPC.** The checkout is a `Begin…Line…Commit` fan, the shipped `OVT_GMRequestComponent.RequestSnapshot` shape.
- **Both `RplId` slots always carry a valid holder**; the op kind decides which is read (`HOLDER_TO_GROUND` passes the source in both). Nothing marshals an invalid handle.
- **The checkout is refused exactly once, at `Begin`.** Lines and Commit answer nothing — the client streams the whole fan before a reply can arrive, so per-line refusals would send up to 16 messages for one order (`storage` learned this; `context.md` Phase 4).
- **Every `RpcDo_*` takes the `ShouldRespondLocally(playerId)` direct branch first**, then `Rpc()` — owner RPCs on a listen host are silently dropped.
- **Seq 0 = `SEQ_NONE`** and is reserved for action-path refusals (#4), which the screen ignores and a notification handler surfaces. The screen's counter starts at 1 and only ever increments.
- ⚠️ **On a listen host the whole reply fan runs synchronously inside the ask.** The screen must latch **before** calling, never after (`storage/context.md` Phase 6 — the ordering *is* the termination argument).

#### The single server gate — `bool MayUseHolder(int playerId, IEntity holder, out string rejectKey)`

Called by every ask. Ladder, in order:

1. `playerId <= 0` → `#OVT-Resource_NoPlayer`
2. Holder resolves and carries an `OVT_ResourceStoreComponent` → `#OVT-Resource_NoStore`
3. `OVT_StructureDamage.IsUsable(holder)` (`:82-92`) → `#OVT-Resource_Ruined`
4. `CallerIsWithin(playerId, holder.GetOrigin(), m_fUseRadius)` → `#OVT-Resource_TooFar` *(a local 6-line copy of `OVT_StorageRequestComponent.CallerIsWithin`, `:2803`; lifting it to the shared base is tech debt, not a licence to edit a `storage` file)*
5. `PlayerMayUseVehicleFor(playerId, holder)` (`:173`) → `#OVT-Resource_Locked`. This one call covers **both** a locked vehicle and any holder with an `OVT_PlayerOwnerComponent`; `storage` proved they collapse.
6. Warehouse (`OVT_RealEstateManagerComponent.GetConfig(holder).m_IsWarehouse`) → `PlayerMayUseWarehouse(persId, holder)` (`:624-656`) → `#OVT-Resource_NoAccess`

Port ops additionally require `AtAPort(playerId, holder)` (the `OVT_StorageRequestComponent.AtAPort` shape, `:2625`, at `m_fPortRadius`) and, for an illegal resource, `HasPermission("IllegalImports") || ResistanceControlsNearestPort(portPos)` — the same expression `OVT_PortContext.CollectImportables` uses (`:314-316`). Import additionally requires `importable`.

**Every refusal answers `RpcDo_ResourceError` with a key.** A silent return is a defect.

**Whole-cart atomicity (decision 1).** At `Commit` the server: re-reads both holders, re-derives every line's litres, sums, compares to the destination's `FreeLitres`, compares the money total to `PlayerHasMoney` for an import — and if any check fails, refuses **the whole cart** with a key that names the shortfall. Only then does it mutate. Nothing clamps, ever.

### 3.6 The screen — `OVT_ResourceTransferContext` and the eight hooks

A `logistics/ui` consumer. Opened by `SetHolder(IEntity)` + `ShowContext(OVT_ResourceTransferContext)`, the `OVT_VehicleMenuContext:159-167` / `OVT_ShopAction.c:45` shape. `Character_Player.et` gets a block reusing `TransferMenu.layout` `{6A8E2C1000000001}` and `m_sContextName "OverthrowTransferContext"`, instance GUID from `6A8E2E1…`.

**Vocabulary: Take / Put** (decision D8), matching `storage`'s "Take" and requirement E's wording. Never "Load/Unload" — those words are already on `storage`'s ammo-box actions.

| Hook | Implementation |
|---|---|
| `BuildModes` | two: `#OVT-Resource_Take` (0), `#OVT-Resource_Put` (1) |
| `BuildEntries(mode, model)` | **Source** = Take: the opened holder; Put: the selected destination. One row per non-zero line of the source's *replicated* ledger. `m_sId = "res:" + id`, `m_eImageKind = TEXTURE`, `m_sImage` = the definition icon `.edds`, `m_iValue = m_iMaxQuantity = qty`, `m_eValueKind = QUANTITY`, `m_iCategoryId = 0`. Row disabled with `#OVT-Resource_NoCargoSpace` when the destination cannot take even one unit. **No pull, no latch, no async first frame** — contents are replicated. |
| `GetCategoryLabelKey` | `""` (single category) |
| `BuildDestinations` | Take: every nearby holder with a store within `m_fHolderRadius`, minus the opened one, **sorted nearest-first** (query order is spatially arbitrary and would reshuffle under the player's selection index), labelled `GetDisplayName()`; **plus a synthetic `"ground"` destination `#OVT-Resource_DestGround` whose `m_Entity` is the player character** — a real entity, so the base never sees a null and the pile spawns where the player is. Put: the same list **without** ground. |
| `FillDetails` | name; quantity; body = the definition's description + `#OVT-Resource_PerUnit` ("%1 m³ · %2 kg per unit") |
| `OnAccept` | latch, then `TransferBegin` → one `TransferLine` per line → `TransferCommit`, with `opKind` from (mode, destination): Take+holder → `HOLDER_TO_HOLDER`; Take+ground → `HOLDER_TO_GROUND`; Put → `HOLDER_TO_HOLDER` with source/dest swapped |
| `IsAddAllAllowed` | `true` in both modes |
| `ValidateCart` | no destination → `#OVT-Resource_NoDestination`; ground → always fits; else cart litres > destination `FreeLitres` → `#OVT-Transfer_NoSpace` **whole cart** with the remaining m³ in the message |
| `GetSummaryText()` **override** | `#OVT-Resource_SummaryVolume` — "x.x m³ of y.y m³" for the destination after the cart is applied. This is the m³ override point the base reserved (`OVT_TransferContext.c:199`). |

Live refresh is the holder's `m_OnContentsChanged` invoker, coalesced at 250 ms (`storage`'s number), with `Reconcile` handling the cart. `OnClose` removes exactly what `OnShow` added, including every `CallLater`.

**Entry points:** the truck (`OVT_OpenResourceStoreAction` on the truck delta's action manager, label "Cargo (12.5 / 20 m³)"), the pile (same class, label "Crate pile (4.4 m³)"), the warehouse (same class on the shipped `Warehouse_01_Base.et` action manager at Sort Priority 3). One action class, three hosts, cached label on the 1 s TTL shape of `OVT_FillFuelAction.c:51-93`, `HasLocalEffectOnlyScript()` true, hidden on a ruin and refused-with-a-reason otherwise (`OVT_FillFuelAction`'s gate policy, `:35-39`). **No vehicle-menu button** — the seated-driver path is deferred (§5 D12); the truck's own action covers it.

### 3.7 Port integration — a category, not a mode

`OVT_PortContext` keeps `MODE_IMPORT = 0` / `MODE_EXPORT = 1` (`:22-23`) and gains a **category**: `CATEGORY_RESOURCES = 9`, one past `OVT_ShopCategory.OTHER` (`OVT_ShopCategory.c:17`, `OTHER = 8`). `GetPopulatedCategories` returns ids ascending (`ui` §3.2), so Resources is the last tab. Four edits, all inside `OVT_PortContext.c`:

1. **`BuildEntries(MODE_IMPORT)`** (`:133-164`) appends one row per **importable** resource after the prefab rows: `m_sId = "res:" + id`, `TEXTURE`, icon `.edds`, `m_iValue = resources.GetPrice(i)` (**live**), `PRICE`, `m_iCategoryId = CATEGORY_RESOURCES`, `m_iMaxQuantity = Math.Min(IMPORT_MAX_QUANTITY, freeLitres / litresPerUnit)`. Non-importable resources are **not listed** in Import (requirement C; BUG-102 — a row whose only outcome is a no-op click is a bug). Illegal resources obey the shipped gate expression (`:314-316`). When the player is in no vehicle, or in one with no store, rows are listed **disabled** with `#OVT-Resource_NeedTruck`; when the store is full, `#OVT-Resource_NoCargoSpace`. Requirement C is explicit that the reason is shown, not hidden.
2. **`BuildEntries(MODE_EXPORT)`** appends one row per resource the occupied vehicle's store holds — **including non-importable ones** — at `resources.GetSellPrice(i)`, `m_iMaxQuantity` = held quantity.
3. **`GetCategoryLabelKey`** (`:177-181`) returns `#OVT-ShopCategory_Resources` for `CATEGORY_RESOURCES` **before** delegating to `OVT_ShopCategoryHelper.GetLabelKey`, whose fall-through returns `#OVT-ShopCategory_Other` for anything it does not know (`OVT_ShopCategory.c:130`).
4. **`ValidateCart`** (`:228-244`) and **`OnAccept`** (`:265-288`) partition lines by the `"res:"` prefix. A `ResourceName` is `"{GUID}Prefabs/…"` and can never begin with `res:`, so the prefix is collision-proof — but the routing must happen **before** `m_Economy.GetInventoryId(line.m_sId)` at `:283`, which would resolve an unknown string to id 0, i.e. some other item's identity (`OVT_EconomyManagerComponent.c:1900-1903`). Resource lines add a litre check against the destination's store and go to `OVT_ResourceRequestComponent` as one `PORT_IMPORT`/`PORT_EXPORT` fan; item lines keep the shipped per-line `ImportToVehicle` / Export batch exactly as they are.

**Drift legibility is text, not an icon** (§5 D10). `FillDetails` for a resource row appends one line derived from `live / base`: `#OVT-Resource_PriceFar{Below,Below,Normal,Above,FarAbove}`. `FillDetails` has no image channel and the row image is already the resource icon; the `overthrow_priceicons.imageset` up/down quads (`{A5EA4C81F9A25690}`, used by `OVT_MapShopPriceIndicator.c:56`) stay available for a later polish pass.

### 3.8 Construction

**Config.** `OVT_Buildable` (`OVT_BuildablesConfig.c:8-52`) gains two optional fields, appended after `handler` so no existing `.conf` entry shifts:

```
[Attribute("", UIWidgets.Object, desc: "Resource requirements (empty = money only, instant)")]
ref array<ref OVT_BuildableResourceRequirement> m_aResourceRequirements;

[Attribute("", UIWidgets.ResourceNamePicker, "", "et", desc: "Construction site prefab (empty = generic)")]
ResourceName m_SitePrefab;
```

`OVT_BuildableResourceRequirement { string m_sResourceId; int m_iQuantity; }`. **An empty list behaves exactly as today** — that is a Definition-of-Done item, not a hope.

**The `BuildItem` split.** `BuildItem`'s external signature (`:838`) is unchanged. Its body splits:

```
IEntity BuildItem(idx, prefabIdx, pos, angles, playerId, runHandler = true)     // :838, unchanged signature
{
    …the guards (:842-845) and the playerId > -1 validation block (:851-870), plus:
      - if buildable is town-buildable and the position sits inside a town, that town must be
        resistance-controlled (§3.10) → return null
    if (playerId > -1 && HasRequirements(buildable))
        return PlaceConstructionSite(idx, prefabIdx, pos, angles, playerId);   // charges money here
    return FinishBuild(idx, prefabIdx, pos, angles, playerId, runHandler, charge: true);
}

protected IEntity FinishBuild(idx, prefabIdx, pos, angles, playerId, runHandler, bool charge)
{
    …lines :872-922 VERBATIM, with :912's TakePlayerMoney wrapped in `if (charge)`
}

IEntity CompleteSite(IEntity site, int playerId)          // server only
{
    read idx/prefabIdx/pos/angles from OVT_ConstructionSiteComponent
    OVT_ResourceRequirements.NearbyAvailability(pos, need, have);  if (!IsSatisfied) → null + reason
    OVT_ResourceRequirements.Consume(pos, need)                   // all-or-nothing
    DestroyPlacedItem(site)                                        // :972-978, the only way a structure leaves
    return FinishBuild(idx, prefabIdx, pos, angles, playerId, runHandler: true, charge: false);
}
```

Consequences, all deliberate: the ordering inside `FinishBuild` is untouched (handler failure still deletes and returns null **before** any charge, `:901-912`); `m_OnBuild.Invoke` (`:917`) fires **once, at completion**, so XP (`OVT_SkillManagerComponent.OnBuild`, `:196-199`) and the tutorial subscriber (`:231-232`) land when the building appears, not when the site does; `OVT_PersistenceTracking.Track` (`:920`) still runs last. **`playerId == -1` never places a site** — server-initiated builds are free of money and free of resources, which is `core/damage`'s existing convention for the occupying repair module, and it keeps the Campaign-tier warehouse case simple.

**The site entity.** `OVT_ConstructionSite.et` (`: {…}Prefabs/Props/Construction/ConcreteMixer_01.et` — a single vanilla prop that reads unmistakably as work-in-progress, unlike every crate/pile candidate which would collide with the resource pile's own visual) carrying:

- `OVT_ConstructionSiteComponent` — `int m_iBuildableIndex`, `int m_iPrefabIndex`, `vector m_vAngles`, plus `[RplProp] string m_sBuildableName` so the client's action label can name the building without a lookup race. Worldless guard as in §3.3.
- `OVT_BuildableComponent` — so the **shipped removal flow** works unchanged (`OVT_BuildContext.DoRemove:735-779` → `RemovePlacedItem(rpl.Id())` → `CanRemoveItem:720-732`, officer-or-owner) and so ownership/base association persist for free.
- `RplComponent`, `ActionsManagerComponent` with two actions, and `SCR_DestructionMultiPhaseComponent { Enabled 0 }` — piles and sites being destructible is out of scope, and leaving vanilla destruction on would make a site (and its money) vanish to a stray grenade.

`m_SitePrefab` overrides the manager's `m_rDefaultSitePrefab` per buildable; the MVP authors none, so all three use the generic one.

**Actions on the site.** `OVT_SiteRequirementsAction` (Sort Priority 1) shows `OVT_ResourceRules.FormatReadout(need, have, defs)` in a `SCR_ConfigurableDialogUi` — the same `CreateFromPreset` + `SetTitle`/`SetMessage` shape `storage` used for the rename dialog (`OVT_RecruitsContext.c:641-716`), with an information preset. `OVT_BuildFromSiteAction` (Sort Priority 2) is named `#OVT-Resource_BuildNow` when satisfied and `#OVT-Resource_ShortOf` naming the first shortfall when not, `CanBePerformedScript` false in that case, 1 s cached label. Both read **replicated** pile contents, so both are correct on every client with no round trip; the server re-derives everything in `RpcAsk_BuildFromSite`.

**The position-based helpers** — `Scripts/Game/Data/OVT_ResourceRequirements.c`, the two entry points `building-repair` will call:

```
static void NearbyAvailability(vector pos, notnull array<ref OVT_ResourceAmount> requirements,
                               out array<ref OVT_ResourceAmount> available)     // client-safe (replicated)
static bool Consume(vector pos, notnull array<ref OVT_ResourceAmount> requirements)   // SERVER ONLY, all-or-nothing
static void ScaleForDifficulty(notnull array<ref OVT_BuildableResourceRequirement> conf,
                               out array<ref OVT_ResourceAmount> scaled)
```

Radius is `OVT_ResourceManagerComponent.GetSupplyRadius()` (attribute, 30 m). `Consume` computes availability first and refuses whole if short; then drains piles **nearest-first** using `OVT_ResourceRules.SortPilesForConsumption` (stable, squared distances) and deletes any pile it empties. The query object is `new`-ed per call.

**Displayed and consumed amounts are both the scaled figure.** `OVT_OverthrowConfigComponent.GetBuildableResourceCost(int baseQty)` = `Math.Round(m_Difficulty.buildableResourceCostMultiplier * baseQty)`, floored at **1** when `baseQty > 0` — shaped exactly like `GetBuildableCost` (`:331-334`). `OVT_BuildContext.Refresh` (`:152-204`) puts the scaled requirement rows in the card details beside the money price (`OVT_BuildMenuCardComponent.c:21`).

### 3.9 MVP data (starting values — tune in play-test)

| id | title | m³/unit | kg/unit | base price | importable | illegal |
|---|---|---|---|---|---|---|
| `timber` | Timber | 0.10 | 25 | 40 | yes | no |
| `cement` | Cement | 0.05 | 50 | 60 | yes | no |
| `steel` | Steel | 0.04 | 90 | 120 | yes | no |
| `hardware` | Hardware | 0.02 | 10 | 200 | yes | no |

| buildable | money (shipped) | requirement | m³ |
|---|---|---|---|
| Guard Tower (`buildables.conf:3-15`) | 1200 | timber 40, steel 10 | 4.4 |
| Helipad (`:78-88`) | 1500 | cement 60, steel 20 | 3.8 |
| Garage (`:66-77`) | 8000 | timber 60, cement 120, steel 60, hardware 30 | 15.0 — **one truckload** |
| **Warehouse (new)** | 12000 | timber 100, cement 200, steel 100, hardware 40 | 24.8 — two truckloads |

Tents, bunkers, the maintenance ramp and the fuel depot stay money-only (user decision 6), which is also the empty-list regression proof.

### 3.10 Warehouse

- **Resources beside items.** `Warehouse_01_Base.et` (the shipped Overthrow same-GUID delta `{E35EA41864A3B0ED}`) gains `OVT_ResourceStoreComponent` (unlimited) beside `OVT_StorageComponent` (`:4-7`) and a third action in the existing `ActionsManagerComponent "{6A8E2D0000000030}"` at Sort Priority 3. `Warehouse_01_Workshop.et` inherits `Building_Base.et` directly and is outside the delta — a known, recorded `storage` residual, unchanged here.
- **The buildable.** `Prefabs/Structures/Industrial/Houses/Warehouse_01/OVT_Warehouse.et`, inheriting vanilla `Warehouse_01.et` `{12310677867A85D4}` (which inherits the Overthrow delta, so it gets both stores and the actions for free). It adds: `OVT_BuildableComponent { m_sBuildableType "Warehouse" }` and `OVT_StructureDestructionComponent` — the `core/damage` retrofit, shaped like `Garage_E_02.et:84-105` (`m_bDeleteAfterFinalPhase 0`, one `SCR_DamagePhaseData` whose `m_PhaseModel` is the **bare** `.xob` `{4FEF9F10015F63FC}Assets/Structures/Industrial/Houses/Warehouse_01/Warehouse_01_Ruin.xob`; a phase model that is not a bare `.xob` is a `core/damage` trap). Its path contains `Warehouse_01`, so `OVT_RealEstateManagerComponent.GetConfig` matches it by substring (`:797-803`) and it is an `SCR_DestructibleBuildingEntity`, satisfying the hard class gate at `:792-795`.
- **Registration after build** (user decision 3). A one-line subscriber on `m_OnBuild` (`:124`) — or the tail of `FinishBuild` — calls `OVT_RealEstateManagerComponent.SetOwnerPersistentId(builderUid, building)` (`:265-296`), which creates the `OVT_WarehouseData` record, broadcasts it and puts it on the map. Owner = the builder's persistent id, `isPrivate = false` (public). **Zero real-estate code change expected**; if one is needed, that is a plan defect to raise.
- **Where it may be built.** `m_bBuildAtBase 1` + `m_bBuildInTown 1`. **No shipped `.conf` sets `m_bBuildInTown`**, so the warehouse is the branch's first user and the town-control rule can be added with zero regression: `OVT_BuildContext.CanBuild`'s town branch (`:305-319`) gains `town.faction == GetPlayerFactionIndex()` else `#OVT-CannotBuildUncontrolledTown`, and `BuildItem`'s validation block gains the matching server-side check via a pure `OVT_ResourceRules`-style predicate (`townFaction == playerFaction || distance >= range`). Villages and FOBs are excluded (YAGNI).
- **Persistence config precedence — the finding this feature is first to hit.** See §3.11.

### 3.11 Persistence

**Three serializers**, all version-first positional, `OVT_EconomyManagerSerializer` / `OVT_StorageComponentSerializer` as templates.

| Serializer | Target | Payload |
|---|---|---|
| `OVT_ResourceStoreComponentSerializer` | `OVT_ResourceStoreComponent` | `version 1`; `array<ref OVT_PersistedResourceLine { string resourceId; int quantity; }>` — **sparse, keyed on the stable id string**, so adding a resource to config never shifts saved stock |
| `OVT_ConstructionSiteComponentSerializer` | `OVT_ConstructionSiteComponent` | `version 1`; `buildableIndex`, `prefabIndex`, `angles` |
| `OVT_ResourceManagerSerializer` | `OVT_ResourceManagerComponent` | `version 1`; two index-aligned arrays `priceIds` (string) + `priceValues` (int) — the `OVT_DeploymentManagerSerializer.c:57-71` / `OVT_JobManagerSerializer.c:333-352` sparse-map shape. An id the config no longer knows is dropped with a warning; an id the save does not know starts at base |

⚠️ Mandatory rules, all measured: **`SaveContext.Write(x)` / `LoadContext.Read(y)` key each property by the LOCAL VARIABLE'S NAME**, so Serialize and Deserialize locals must be spelled identically (a mismatch silently reads zeros and returns success); **a per-entry field loop is impossible** — one `array<ref …>` of a record class, never parallel per-instance writes; **every `Read()` return is checked**, and a failed read aborts the payload with an ERROR and leaves live state untouched; the persisted record class names are written into every save as `$type` and are therefore **frozen** — which is why `OVT_PersistedResourceLine` is a dedicated record and not `OVT_ResourceAmount`, whose shape the wire and the UI still get to change; **never `array<bool>`**.

**Five bindings in `Configs/Systems/Persistence/Overthrow.conf`, GUIDs from `6B0E7A7…`:**

| # | Where | Edit | Why |
|---|---|---|---|
| 1 | Game-mode config `{65ACD95F40F6C669}` `ComponentSerializers` (`:23-51`) | + `OVT_ResourceManagerSerializer` | the price table |
| 2 | CAR config `{64C6B4937723DA61}` (`:97-123`) | + `OVT_ResourceStoreComponentSerializer` | truck loads |
| 3 | Building config `{65B682661F79DDBE}` (`:154-160`) | + `OVT_ResourceStoreComponentSerializer` | **purchased** warehouses |
| 4 | **Buildable** config `{6B0E7A27C0D539F2}` (`:186-203`) | + `OVT_ResourceStoreComponentSerializer` **and** + `OVT_StorageComponentSerializer` **and** + `OVT_ConstructionSiteComponentSerializer` | **built** warehouses and sites |
| 5 | New `EntityPersistenceConfig` in the `Overthrow` group, `ComponentClassPersistenceConfigRule "OVT_ResourcePileComponent"`, Priority 35000, `SelfSpawn 1`, `ParentHandling "Ignore always"`, `GenericEntitySerializer`, serializer `OVT_ResourceStoreComponentSerializer` | piles | the Placeable/Buildable/Deployment block shape, `:165-218` |

**Binding 4 is the load-bearing finding.** An entity gets exactly **one** `EntityPersistenceConfig`. A built warehouse carries `OVT_BuildableComponent`, so it matches the Overthrow Buildable rule at **Priority 35000** as well as the vanilla Building `PrefabPersistenceConfigRule` (which authors no priority at all — `ArmaReforger/Configs/Systems/Persistence/Configuration/Building/Building.conf:1-11`). The Buildable rule wins. **Empirical proof it already works this way:** the shipped Garage buildable is an `SCR_DestructibleBuildingEntity` descending from `Building_Base.et` (`Garage_E_02_base.et:1`) and carrying `OVT_BuildableComponent` (`Garage_E_02.et:4`), and buildables persist today. The consequence nobody has hit yet is that a built holder gets **only** the Buildable config's serializers — so without binding 4 a built warehouse would silently lose both its item ledger and its resource stock on reload. A serializer whose target component is absent is simply not applied, so adding all three to the Buildable config is harmless for every other buildable (the Placeable config already carries `BaseInventoryStorageComponentSerializer` on that basis, `:180`).

**No new `ComponentClassPersistenceConfigRule` on `OVT_ResourceStoreComponent`.** It would hijack trucks, warehouses and buildings away from their existing configs — the trap `storage` §3.9 names. The pile gets a rule because `OVT_ResourcePileComponent` exists on nothing else; the site does **not**, because it carries `OVT_BuildableComponent` and the Buildable config already claims it.

**Tracking.** `BuildItem`/`FinishBuild` already `Track` (`:920`). Piles are tracked at spawn. Warehouses and other buildings are tracked lazily by `PublishContents()` → `EnsureTracked()` behind an `IsTracked` check (`OVT_PersistenceTracking.c:59-72`), the ask-first precedent at `OVT_VehicleManagerComponent.c:1138-1140`. Nothing is ever untracked except a deleted pile.

### 3.12 HUD and map

**HUD.** `OVT_CargoInfo : SCR_InfoDisplay`, `UI/Layouts/HUD/CargoInfo.layout`, registered in the truck delta's `SCR_BaseHUDComponent` (§3.3). `OVT_ProgressInfo` is the template — `OnStartDraw` caches widgets, `OnStopDraw` releases, subscription is async with retry (`:23-28`, `:55`). It draws the occupied truck's non-zero lines and `used / total m³` from the replicated ledger. It is **vehicle-scoped**, so BUG-097 (`OVT_EconomyInfo.c:19-24` — one declaration per occupiable prefab makes per-instance state wrong for *player*-scoped data) does not apply; the display's `owner` is the truck.

**Map.** No registry is invented. `OVT_MapMarkerComponent` self-registers runtime-spawned entities from `OnPostInit` on **every machine** and unregisters in `OnDelete` (`:26-32`, `:59-80`) — explicitly designed for "a player-built ramp appears on the next map open without a restart", which is exactly a crate pile. So: append `RESOURCE_PILE` to `OVT_MapMarkerCategory` (`:4-8`, append-only), author the component on the pile prefab, add `OVT_MapLocationResourcePile : OVT_MapLocationType` (template `OVT_MapLocationPOI.c:38-60`, which iterates `markers.GetMarkers(category)`), register one block in `Configs/Map/OverthrowMap.conf`, and add one crate quad to `overthrow_mapicons.imageset` `{C7691945DE01FB28}` (24 quads today, no crate glyph). Info rows come from the pile's own **replicated** store — no second source of truth for non-map state, which the marker component's header insists on.

### 3.13 Replication summary

| What | Mechanism | JIP |
|---|---|---|
| Holder contents (truck, pile, warehouse) | one `[RplProp(onRplName:"OnContentsChanged")] string m_sPacked`, bumped once per finished request | **Free** — `RplProp` carries streamed-in state (`OVT_ReservationSyncComponent.c:16-25`) |
| Holder capacity | **not replicated** — prefab attribute, identical on every machine | N/A |
| Live prices | `RplSave`/`RplLoad` on the manager (connect-time; the game-mode `RplComponent` is `Streamable Disabled`, `OVT_OverthrowGameMode.et:381-383`) + `RpcDo_SetPrice` broadcast per drift | Covered by `RplSave` |
| Site identity | `[RplProp] string m_sBuildableName` + serialized indices | Free |
| Pile markers | **not replicated** — every machine registers its own from the entity's own `OnPostInit` | Free by construction |
| Difficulty multipliers | appended to `OVT_OverthrowConfigComponent`'s positional stream, `CONFIG_STREAM_VERSION` **5 → 6** | The stream *is* the JIP path |
| Transfer results / refusals | owner RPCs, `ShouldRespondLocally` branch first | N/A |

**`CONFIG_STREAM_VERSION` 5 → 6** (`OVT_OverthrowConfigComponent.c:714`): append the three new floats at the **end of the difficulty block** in `RplSave` (after `repairCostMultiplier`, `:744`) and in `RplLoad` (after `:841`), before the server-config fields. A mismatch hard-fails with the shipped message (`:764-767`).

### 3.14 Difficulty

`OVT_DifficultySettings` gains three `category: "Economy"` fields beside `repairCostMultiplier` (`:120`):

| Field | attr default | Easy | Hard | Extreme | Insane |
|---|---|---|---|---|---|
| `buildableResourceCostMultiplier` | `"1"` | 0.8 | 1.5 | 3 | 4 |
| `resourcePriceMultiplier` | `"1"` | 0.8 | 1.25 | 1.5 | 2 |
| `resourcePriceVolatility` | `"1"` | 0.5 | 1.5 | 2 | 2 |

Normal and TestWorld author **none** — `.conf` files override only where they differ from the attribute default, which is why `Difficulty_Normal.conf` lists neither cost multiplier today. Accessors: `GetBuildableResourceCost(int)`, `GetResourcePriceMultiplier()`, `GetResourcePriceVolatility()`.

---

## 4. Implementation Phases

Every phase: `tools/compile-check.sh` exit 0 before hand-back. **`tools/run-tests.sh` is the orchestrator's**, run once after a phase completes — never inside an agent, never during planning (`.claude/test-policy.md`). Suites are run **by class name** (`OVT_TEST_LogicSuite`, `OVT_TEST_InitSuite`, `OVT_TEST_PersistenceRoundTripSuite`, `OVT_TEST_CampaignSuite`), **never** the Fast/All groups, which hang. Re-baseline (`git pull` / `git status`) before every phase.

Suite baselines to beat, measured 2026-08-21: Logic 216/216, Init 162/163 (one pre-existing red, `CompositionSlotGate_AcceptedTypesMatchTheCompositions` — belongs to base-defense/deployments), PersistenceRoundTrip 34/34, Campaign 16/16, Persistence 13/13.

### Phase 1 — The pure spine
**Estimate:** 5–6 h · **Agent:** `component-developer` · **Suite:** `OVT_TEST_LogicSuite`

1. `OVT_ResourceLedger.c` (+ `OVT_ResourceAmount`), `OVT_ResourceDefs.c`, `OVT_ResourcePack.c`, `OVT_ResourceRules.c` — §3.2.
2. `Tests/TestSuites/Logic/OVT_TEST_Logic_ResourceLedger.c`, `…_ResourceRules.c`.

**Acceptance**
- **No manager accessor and no game-mode getter identifier appears anywhere under `TestSuites/Logic/`, not even in a comment** — the tier grep does not distinguish code from prose.
- Every case proven able to fail once; the mutation and the resulting message recorded in `context.md`.
- `new` sets every field explicitly (`[Attribute]` defvalues do not apply to `new`); no ternaries; no `maxAttempts`; polls are preconditions with a named failure.
- **No float appears in any capacity comparison** — grep the ledger for `float` and confirm only `TotalWeightKg` uses one.
- `TotalLitres()` is O(1) by construction.
- Compile-check exit 0.

### Phase 2 — Config + manager + `OVT_Global`
**Estimate:** 4–5 h · **Agent:** `component-developer` · **Suite:** `OVT_TEST_InitSuite`

1. `OVT_ResourcesConfig.c`, `Configs/Resistance/resources.conf` with the four MVP resources of §3.9.
2. `OVT_ResourceManagerComponent.c` — config load, `m_Defs`, price table init-to-base, accessors `GetPrice`/`GetSellPrice`/`GetBasePrice`/`GetDefs`/`GetSupplyRadius`/`GetMergeRadius`. **No drift yet** (Phase 7), **no pile spawn yet** (Phase 5).
3. `OVT_OverthrowGameMode.et` — the component + `m_rResourcesConfigFile`.
4. `OVT_Global.GetResources()`.
5. `Tests/TestSuites/Init/OVT_TEST_Init_ResourceSeam.c` — manager resolves; config populates 4 definitions with non-zero litres and base price; every definition id is unique and non-empty.

**Acceptance**
- Config load uses `BaseContainerTools.LoadContainer` + `CreateInstanceFromContainer`, not a bespoke reader.
- Prices initialise to base **through `m_aCurrentPrice`**; no call site reads `m_aBasePrice` as a live price (grep).
- The `.conf` GUID comes from `6A8E2E…`; `.meta` authored.
- Compile-check exit 0.

### Phase 3 — Store component + prefabs ⚠️ ADVANCED AGENT
**Estimate:** 6–7 h · **Agent:** `component-developer-advanced` · **Suite:** `OVT_TEST_InitSuite`

1. `OVT_ResourceStoreComponent.c` (§3.3) + `OVT_ResourcePileComponent.c` + `OVT_ResourceUtils.c` (per-call query objects).
2. **New same-GUID delta** `Prefabs/Vehicles/Core/Wheeled_Truck_Base.et` `{E03D5609EEA6E03D}` — header naming vanilla's parent `{62F416029692CE40}Prefabs/Vehicles/Core/Wheeled_Base.et` byte-for-byte, `ID "BBCBA43A9778AE21"`, the store, and the `SCR_BaseHUDComponent` restating all three InfoDisplays (the third is authored in Phase 10; restate the two now and add the third then, or author the placeholder now — either way the array is complete at every commit).
3. Cap overrides on `M923A1_transport.et`, `Ural4320_transport.et`, `OverthrowMobileFOB.et`, `OverthrowMobileFOBDeployed.et`.
4. `Warehouse_01_Base.et` — + store (unlimited).
5. `Prefabs/Props/Resources/OVT_ResourcePile.et` — `: {…}Prefabs/Props/Military/MilitaryCrates/CrateStack_01/CrateStack_01_base.et`, + store (−1), + `OVT_ResourcePileComponent`, + `RplComponent`, + `ActionsManagerComponent`, + `SCR_DestructionMultiPhaseComponent { Enabled 0 }`.
6. Extend `OVT_TEST_Init_ResourceSeam.c`: a spawned M923A1 transport, the test world's `Warehouse_01` (`default.layer:94`) and a spawned pile each resolve a store with the expected litre capacity (20000 / −1 / −1); a spawned **car** resolves none.

**Acceptance**
- `OnPostInit` carries the `!owner.GetWorld()` guard and the `GetRpl()` ERROR; both proven by reading.
- The truck delta's GUID and parent line match the vanilla file exactly; fresh GUIDs from `6A8E2E0…`; **inherited component GUIDs copied, not minted**; `grep -rl 6A8E2E` re-verified before authoring.
- Capacity is read from the attribute on both server and client — grep confirms `m_iCapacityLitres` is never written outside `OnPostInit`.
- `Replication.BumpMe()` appears in exactly one method (`PublishContents`).
- Compile-check exit 0.

### Phase 4 — Persistence ⚠️ ADVANCED AGENT
**Estimate:** 5–6 h · **Agent:** `component-developer-advanced` · **Suite:** `OVT_TEST_PersistenceRoundTripSuite`

1. `OVT_ResourceStoreComponentSerializer.c` (+ frozen `OVT_PersistedResourceLine`) and `OVT_ResourceManagerSerializer.c` — §3.11. (`OVT_ConstructionSiteComponentSerializer` lands in Phase 8.)
2. Four of the five `Overthrow.conf` bindings (1, 2, 3, 5); binding 4's site serializer follows in Phase 8, its two store/storage serializers land **now**.
3. `EnsureTracked()` on first non-empty publish.
4. Cases appended to `OVT_TEST_PersistenceRoundTripSuite.c`: truck load round-trip; pile round-trip (spawned, tracked, `SelfSpawn`); warehouse resource stock round-trip; **price state round-trip** (drift a price by hand through the manager's setter, reload, assert the drifted value, not base).

**Acceptance**
- Serialize/Deserialize locals **identically named**; a deliberate rename shown to fail during development and recorded in `context.md`.
- Every `Read()` return checked; a forced failure leaves live state untouched and logs ERROR.
- **No new `ComponentClassPersistenceConfigRule` on `OVT_ResourceStoreComponent`** (grep).
- New saving cases sort **after** `…_Capability_…`.
- Compile-check exit 0.

### Phase 5 — Request component, wire, pile spawn/merge ⚠️ ADVANCED AGENT
**Estimate:** 6–8 h · **Agent:** `network-specialist-advanced` · **Suite:** `OVT_TEST_InitSuite`

1. `OVT_ResourceRequestComponent.c` — the six RPCs, `MayUseHolder`, the checkout state machine, whole-cart atomicity, `CallerIsWithin`.
2. `OVT_ResourceManagerComponent.SpawnOrMergePile` + pile cleanup.
3. `OVT_OverthrowController.et` — append before the `RplComponent` (`:48`).
4. Extend `OVT_TEST_Init_ResourceSeam.c` with the mandatory controller-component seam case: `OVT_ControllerComponent<OVT_ResourceRequestComponent>.Get()` is non-null **and** sits on this player's controller entity (`OVT_TEST_Init_VehicleRequestSeam.c:30-31` is the template).

**Acceptance**
- **RPC arity audit table written into `context.md`**, all 7 rows (6 here + the manager's `RpcDo_SetPrice`) checked against their handlers. No `Rpc()` call wrapped in a helper.
- No `array<…>` on any RPC; both `RplId` slots always valid.
- Every `RpcDo_*` takes the `ShouldRespondLocally` branch first.
- Every refusal answers `RpcDo_ResourceError`; none returns silently — proven by reading every `return` in every ask.
- **Nothing clamps.** A cart that does not fit is refused whole. Proven by reading `Commit`.
- `RemoveOrdered` wherever a line array's order is observable (`array.Remove` is swap-with-last).
- Compile-check exit 0.

### Phase 6 — The transfer screen + actions
**Estimate:** 5–7 h · **Agent:** `ui-developer` · **Suite:** `OVT_TEST_InitSuite`

1. `OVT_ResourceTransferContext.c` — the eight hooks of §3.6 + the `GetSummaryText()` override.
2. `Character_Player.et` — the context block, layout `{6A8E2C1000000001}`, `m_sContextName "OverthrowTransferContext"`, GUID from `6A8E2E1…`.
3. `OVT_OpenResourceStoreAction.c` + entries on the truck delta, the pile prefab and the warehouse delta (Sort Priority 3).
4. `.st` keys for everything new in this phase.

**Acceptance**
- **`OVT_TransferContext.c`, `OVT_TransferListModel.c`, `OVT_TransferCartModel.c` unmodified** — `git diff --exit-code`. If a base change seems necessary, stop and raise it.
- The latch is set **before** the ask, never after (listen-host synchronicity).
- The picker shows ≥ 2 entries in Take mode (holder + ground at minimum) and is sorted nearest-first.
- `OnClose` removes exactly what `OnShow` inserted, including the contents invoker and every pending `CallLater`.
- Action labels read from the replicated ledger and update with no menu open; 1 s cached.
- **Do not write `Configs/Language/*.conf`.**
- Compile-check exit 0.

### Phase 7 — Price drift, difficulty, port category ⚠️ ADVANCED AGENT
**Estimate:** 6–7 h · **Agents:** `component-developer-advanced` (1–3) then `ui-developer` (4) · **Suites:** `OVT_TEST_LogicSuite`, `OVT_TEST_InitSuite`
*May run in parallel with Phase 8 (disjoint files) once Phases 5–6 land.*

1. `OVT_ResourceRules.DriftStep` / `WarPressure` / `ApplyLevelMultiplier` / `SellPrice` wired into the manager's `CheckPrices` tick + latch + `RplSave`/`RplLoad` + `RpcDo_SetPrice`.
2. `OVT_DifficultySettings` — three fields; four `.conf` overrides (§3.14).
3. `OVT_OverthrowConfigComponent` — three accessors, `CONFIG_STREAM_VERSION` **5 → 6**, appends in **both** `RplSave` and `RplLoad`.
4. `OVT_PortContext` — `CATEGORY_RESOURCES = 9`, resource rows in both modes, `GetCategoryLabelKey` special case, `ValidateCart`/`OnAccept` prefix routing, the drift text in `FillDetails`.
5. Logic cases for the whole drift surface (§7).

**Acceptance**
- The composition order of §3.4 is asserted by a Logic case, including that the level multiplier is applied **after** the band clamp.
- Volatility scales the step and **not** the band — asserted at both clamp edges.
- Exactly one drift per 6-hour window — asserted by the latch case.
- The stream bump appears in **both** directions and the version constant is a single source (grep for `5` in that file's stream region returns nothing stale).
- `OnAccept` routes `"res:"` lines **before** any `GetInventoryId` call (read the diff).
- Import lists no non-importable resource; Export lists them.
- Compile-check exit 0.

### Phase 8 — Construction ⚠️ ADVANCED AGENT
**Estimate:** 8–10 h · **Agent:** `component-developer-advanced` · **Suites:** `OVT_TEST_LogicSuite`, `OVT_TEST_PersistenceRoundTripSuite`
*May run in parallel with Phase 7.*

1. `OVT_BuildablesConfig.c` — the two new `OVT_Buildable` fields + `OVT_BuildableResourceRequirement`.
2. `OVT_ResourceRequirements.c` — the three position-based entry points of §3.8.
3. `OVT_ResistanceFactionManager` — the `BuildItem` / `FinishBuild` / `CompleteSite` split, `PlaceConstructionSite`.
4. `OVT_ConstructionSiteComponent.c` + `OVT_ConstructionSiteComponentSerializer.c` + binding 4's third serializer.
5. `Prefabs/Structures/OVT_ConstructionSite.et` + two actions + the readout dialog.
6. `buildables.conf` — requirements on Garage, Helipad, Guard Tower.
7. `OVT_BuildContext` — scaled requirement rows on the card details.
8. Logic cases for requirement maths + consumption order; a Persistence case for a site round-trip.

**Acceptance**
- **A buildable with an empty requirement list is byte-identical in behaviour** — asserted by reading the branch and exercised in play-test step 12 (a Recruitment Tent still builds instantly for money).
- `FinishBuild` contains lines `:872-922` unchanged except the `if (charge)` wrapper; the handler-failure path still returns **before** any charge.
- `m_OnBuild` fires exactly once per finished building, at completion.
- `playerId == -1` never places a site (grep the branch condition).
- `Consume` is all-or-nothing and never partially drains — proven by reading.
- The consumption order is deterministic and uses **squared** distances (`vector.Distance` is not correctly rounded).
- `ScaleRequirement` never turns a non-zero requirement into zero; displayed and consumed figures come from the same call.
- Compile-check exit 0.

### Phase 9 — Warehouse resources + the buildable warehouse ⚠️ ADVANCED AGENT
**Estimate:** 6–7 h · **Agent:** `component-developer-advanced` · **Suites:** `OVT_TEST_CampaignSuite`, `OVT_TEST_PersistenceRoundTripSuite`
*Depends on Phases 3, 4, 8.*

1. `Prefabs/…/Warehouse_01/OVT_Warehouse.et` — `OVT_BuildableComponent` + `OVT_StructureDestructionComponent` (the `core/damage` retrofit, §3.10).
2. `buildables.conf` — the Warehouse entry (`m_bBuildAtBase 1`, `m_bBuildInTown 1`, money 12000, the §3.9 requirement).
3. Registration through `SetOwnerPersistentId` after build.
4. Town-control gate: client reason in `OVT_BuildContext.CanBuild`, server re-check in `BuildItem`, pure predicate in `OVT_ResourceRules`.
5. Binding 4 verified end-to-end (item ledger + resource stock on a **built** warehouse).
6. Campaign cases: a built warehouse is registered exactly like a purchased one (`OVT_WarehouseData` record present, on the map, `PlayerMayUseWarehouse` true for the builder); a port purchase moves money **and** resources.

**Acceptance**
- Zero lines changed in `OVT_RealEstateManagerComponent.c`. If a change is needed, raise it as a plan defect.
- The new prefab's path contains `Warehouse_01` and `GetConfig` matches it (asserted in the Campaign case, not assumed).
- The destruction component's `m_PhaseModel` is a **bare `.xob`**.
- A built warehouse's stock survives a reload (Persistence case) — this is binding 4's proof.
- Compile-check exit 0.

### Phase 10 — Cargo HUD + map marker
**Estimate:** 4–5 h · **Agent:** `ui-developer` · **Suite:** `OVT_TEST_InitSuite`
*May run in parallel with Phases 8–9.*

1. `OVT_CargoInfo.c` + `UI/Layouts/HUD/CargoInfo.layout` + the truck delta's third InfoDisplay.
2. `OVT_MapMarkerCategory.RESOURCE_PILE` (append-only) + the marker on the pile prefab.
3. `OVT_MapLocationResourcePile.c` + the `OverthrowMap.conf` block + one crate quad in `overthrow_mapicons.imageset`.

**Acceptance**
- The HUD reads the **occupied truck's** ledger, not a player-scoped cache (BUG-097's shape avoided).
- `RESOURCE_PILE` is appended to the enum, never inserted.
- The location type reads pile contents from the pile's own component — no second source of truth (`OVT_MapMarkerComponent.c:22-24`).
- `m_fRefreshInterval` set deliberately (piles move rarely; `OVT_MapLocationVehicle`'s `2` is the busy end of the range).
- Compile-check exit 0.

### Phase 11 — Localization, conflict check, help & wiki sync
**Estimate:** 2–3 h · **Agents:** main thread + `help-docs-sync` · **Suite:** none (announce the skip)

1. `.st` audit: every runtime key exists in `Language/localization_Overthrow.st` with a filled `Comment`; **count braces before and after** (an unbalanced `.st` means the next Workbench save eats entries); fresh GUIDs; Id order; multi-line values use the trailing backslash. **Ask the user to re-export** — keys render raw until then.
2. `python3 .claude/skills/overthrow-ui-patterns/scripts/check-input-conflicts.py`, plain and `--warnings` — **only if any input file was touched**; the plan expects none.
3. Every tutorial/help claim fact-checked against a file:line before it ships (two tips have shipped invented mechanics before).
4. `help-docs-sync`: this feature changes player-facing behaviour substantially (a new screen, three new actions, a new port category, construction sites, a buildable warehouse, a cargo HUD, a new map marker), so the phase runs. Tutorials (`Configs/Tutorials/`), the Field Manual (`Configs/FieldManual/`) and the wiki's port / building / warehouse pages.

---

## 5. Key Technical Decisions

**D1 — Over capacity refuses the whole cart; nothing ever clamps.** (User decision, 2026-08-21.) It matches the shipped port rule (`OVT_PortContext.ValidateCart:237-241`, whose own comment says a clamped purchase "reads as a half-broken purchase"), it is the same rule at the port, the pile and the warehouse, and it is what makes a `Begin…Line…Commit` fan the right wire shape instead of per-line asks. *Rejected:* clamping — cheaper to implement, impossible to explain.

**D2 — Money is charged at placement, not completion.** (User decision, 2026-08-21.) The charge stays exactly where it is today, inside the build request, so no new "who owes what" state exists. Removing a site refunds nothing, because no refund path exists anywhere in the mod. *Rejected:* charging at completion — it would need the site to remember a price across sessions and across a difficulty change.

**D3 — Volume is integer litres internally, m³ in config and on screen.** Every capacity decision is exact: no epsilon, no `Math.Abs(a-b) < EPSILON` in a fit check, no binary32 surprise (EnforceScript `float` is IEEE binary32, and this project has already been bitten by it). The `[Attribute]` stays a friendly `float` in m³ and is converted once in `OnPostInit`. *Rejected:* float capacity — every Logic case would need an epsilon and every boundary case would be a judgement call.

**D4 — Contents replicate as ONE packed `RplProp` string.** A ledger is ≤ a handful of lines, and replicating it buys the cargo HUD, the pile inspect label, the map rows and the Build action's shortfall text with zero protocol. `[RplProp] string` is proven in this codebase (`OVT_StorageComponent.m_sCustomName`, `:87-88`); `RplProp` on an array is not, and this feature should not be the first to find out. `RplSave`/`RplLoad` was the other candidate and loses on testability: **`ScriptBitWriter` cannot be round-tripped from script** (it hard-crashes on first use), so a bitstream payload can never be unit-tested, whereas `OVT_ResourcePack.Encode`/`Decode` is a pure pair with a Logic case. One `BumpMe` per finished request. *Rejected:* `storage`'s pull-on-open — correct there (500-item boxes, one reader), wrong here (4 lines, many readers, every one of them a label).

**D5 — Prices are stored per resource, drifted server-side, replicated by `RplSave` + a broadcast, and persisted by stable id.** The epic's "prices are state" rule with the shipped mechanisms: the manager `RplSave`/`RplLoad` pattern (`OVT_EconomyManagerComponent.c:2110-2185`), the `RpcDo_Set…` broadcast pattern (`:1273`), and the two-index-aligned-arrays sparse-map save shape (`OVT_DeploymentManagerSerializer.c:57-71`). Index on the wire (the config is a mod file, identical everywhere), id in the save (a config edit must not corrupt stock or prices).

**D6 — Price drift runs on the resource manager's own tick, not the economy's.** Same cadence, same latch idiom, same 6-hour windows (user decision 4) — but a copy of `CheckUpdate`'s shape rather than an edit to `OVT_EconomyManagerComponent.CheckUpdate` (`:159-217`), which is the mod's most load-bearing hour gate. `economy` is **read** by this feature and never written. *Accepted cost:* a long `HandleTimeSkip` can skip a window entirely, exactly as it can for income and rent today.

**D7 — `Begin/Line/Commit`, not per-line asks.** D1 makes atomicity a correctness requirement, and per-line asks cannot be atomic: three lines that each fit against a running total, with a fourth that does not, *is* a clamp. The fan is the shipped `OVT_GMRequestComponent` / `OVT_StorageRequestComponent` shape, so it costs no invention. *Rejected:* one `RpcAsk_MoveResource` per line (4 RPCs, no state) — simpler, and wrong under D1.

**D8 — Vocabulary is Take / Put, with the opened holder as the anchor.** Take: source = the holder you opened, destination = picked. Put: source = picked, destination = the holder you opened. `BuildEntries` always lists the source; `ValidateCart` always checks the destination. That is symmetric, needs one entry point per holder, and gives requirement E its literal "Take and Put modes". "Load/Unload" is rejected because those words are already on `storage`'s ammo-box actions and would mean something different two menus away.

**D9 — Resources are a category (id 9) at the port, not a third mode.** `OVT_ShopCategory.OTHER` is 8, `GetPopulatedCategories` returns ids ascending, so Resources lands as the last tab in both shipped modes with no mode-toggle churn and no change to `BuildModes`. The id prefix is `"res:"`, collision-proof because a `ResourceName` always begins `"{GUID}…"` — but the routing must precede `GetInventoryId`, which maps an unknown string to id 0.

**D10 — Drift is legible as text, not an icon.** `FillDetails` returns three strings and has no image channel, and the row's image slot already carries the resource icon. A one-line "well above base" in the details body is honest, localizable and free. The `overthrow_priceicons` up/down quads remain available for a later polish pass; adding an image channel would mean widening the closed base.

**D11 — The pile has no registry; `OVT_MapMarkerComponent` already solves this.** Its header states it self-registers runtime-spawned entities on every machine and is deliberately not replicated (`:26-32`), which is precisely "a crate pile dropped ten seconds ago appears on the next map open". This removes a replicated `array<RplId>`, its two broadcasts, its JIP half and its failure modes. *Rejected:* a manager-held replicated registry (what the planning notes assumed before this was found) and a per-map-open world query (cost on the map's hot path).

**D12 — Held continuous load/unload actions are deferred, and the seated-driver vehicle-menu button with them.** Requirement B names Conflict's `SCR_ResourceContainerVehicleLoadAction` as a *UX shape*; the chosen-quantity requirement is fully met by the transfer screen, Overthrow authors only `Duration 0` actions today, and a `Duration -1` + `PerformPerFrame` action is a new input surface with its own gamepad questions. Sugar, not scope. Recorded so nobody reads its absence as an oversight.

**D13 — `BuildItem` splits into `BuildItem` + `FinishBuild(…, bool charge)`; `CompleteSite` re-enters `FinishBuild`.** One spawn/register/handler/navmesh/invoke/track path, one ordering, one place to break. `BuildItem`'s external signature is unchanged, so every shipped caller (`OVT_ResistanceRequestComponent.RpcAsk_BuildItem:279`, the deployment modules, the console commands) is untouched. *Rejected:* a parallel completion path — it would duplicate the handler-before-charge ordering, which is the one part of `BuildItem` with a recorded bug history.

**D14 — The buildable warehouse is a vanilla `Warehouse_01` variant with `OVT_BuildableComponent`, owned by the builder, public.** (User decision, 2026-08-21.) `SetOwnerPersistentId` (`:265-296`) already creates the `OVT_WarehouseData` record because `GetConfig` matches `"Warehouse_01"` on an `SCR_DestructibleBuildingEntity` (`:789-806`), so there is no "built warehouse" concept to maintain. The **consequence** is D15.

**D15 — The Overthrow Buildable persistence config wins over the vanilla Building config, so it must carry the holder serializers.** One `EntityPersistenceConfig` per entity; the Buildable rule authors `Priority 35000` (`Overthrow.conf:191`) and the vanilla Building config authors no priority at all (`Building.conf:1-11`). The shipped Garage proves the precedence empirically — it is a `Building_Base.et` descendant carrying `OVT_BuildableComponent` and it persists. So a *built* holder sees only the Buildable config's serializers, and this feature appends `OVT_ResourceStoreComponentSerializer`, `OVT_StorageComponentSerializer` and `OVT_ConstructionSiteComponentSerializer` there. The storage serializer's absence is a latent `storage` gap this feature is simply the first to reach; a serializer with no matching component is inert, so the addition is free for every other buildable.

**D16 — The site carries `OVT_BuildableComponent` and therefore needs no persistence rule of its own; the pile carries a dedicated marker and gets one.** A rule on `OVT_ResourceStoreComponent` would hijack trucks, warehouses and buildings from their existing configs — the trap `storage` §3.9 names explicitly. `OVT_ResourcePileComponent` exists on exactly one prefab, so a rule on it is safe and is the fourth block in the Overthrow group.

**D17 — MVP requirements go on Garage, Helipad and Guard Tower; everything else stays money-only.** (User decision 6, 2026-08-21.) Three buildables is enough to feel the loop at three price points, and the untouched majority is the standing regression proof that an empty requirement list changes nothing.

**D18 — One generic crate-pile prefab; per-resource meshes deferred.** (User decision 5, 2026-08-21.) `CrateStack_01_base.et` is a single vanilla prop with the right silhouette; the contents are read from the inspect screen, the map panel and the HUD, all of which name resources exactly. Per-resource meshes would multiply the merge rule by N.

**D19 — Truck capacity is a component `[Attribute]` on a new same-GUID `Wheeled_Truck_Base.et` delta.** (User decision 7, 2026-08-21.) There is no runtime component creation in EnforceScript, so a component must be authored on a prefab; `Wheeled_Truck_Base.et` is the narrowest prefab that reaches both vanilla truck families and therefore every truck any system spawns, and it excludes cars and APCs by construction rather than by a runtime capacity check. *Rejected:* `Wheeled_Base.et` + an AUTO capacity rule (would put a resource store on every civilian car and reintroduce the deferred-resolve/replicated-capacity machinery `storage` needed); a scripted prefab→capacity table (no per-instance persistence, no `RplProp`, and it misses every prefab a later config adds).

**D20 — The illegal gate ships built and unexercised, and a test proves it works.** No MVP resource is illegal (epic decision). A Logic case constructs an `OVT_ResourceDefs` with one resource flagged illegal and asserts the predicate refuses it without the permission and admits it with resistance port control. That is the only thing standing between "deliberately inert" and "dead code somebody deletes in six months".

---

## 6. Definition of Done

### Functional

- **F1** Every truck in the game (vanilla or Overthrow, spawned by any system) shows a "Cargo (x / y m³)" action; cars, APCs and helicopters show none.
- **F2** At a port, in a truck, the port screen has a **Resources** tab in both Import and Export; prices are the live drifted ones and the price charged equals the price shown.
- **F3** Buying more than the truck can hold is refused **whole**, with a message naming the remaining m³. Nothing is ever partially bought.
- **F4** Not being in a truck with a store leaves the resource rows **visible and disabled** with a reason, never hidden.
- **F5** The resource transfer screen opens from a truck, a pile and a warehouse; Take and Put both work; the destination picker lists nearby holders nearest-first plus "Ground (new pile)" in Take.
- **F6** Unloading to the ground spawns a crate pile; unloading again within the merge radius **adds to the same pile** instead of spawning a second; a pile drained to zero disappears.
- **F7** A pile shows on the map with its contents in the info panel, and its inspect action names its total m³.
- **F8** A truck with cargo shows a HUD readout of contents and used/total m³ while the player is seated in it.
- **F9** A buildable with **no** resource requirement builds exactly as it does today — instantly, for money.
- **F10** A buildable **with** requirements places a construction site and charges the money at that moment; the site shows "Requirements" (needed vs available nearby, what is short) and "Build", the latter enabled only when satisfied and otherwise naming the shortfall.
- **F11** Build consumes nearby piles nearest-first, deletes the emptied ones, removes the site and spawns the finished building with the correct owner, base association and XP.
- **F12** A site is removable through the existing removal flow, leaves nothing behind, and refunds nothing.
- **F13** A warehouse holds resources beside its items, and both are usable from their own screens.
- **F14** A **Warehouse** can be built at a base or in a resistance-controlled town for money + resources, and afterwards is indistinguishable from a purchased warehouse (map, ownership, access, item storage, resource storage).
- **F15** Building in a town the resistance does not control is refused, client-side with a reason and server-side on re-validation.
- **F16** Prices move at most once per 6-hour game-time window, stay inside the configured band, move up under war pressure and down under resistance port control, and the movement is visible in the details panel.
- **F17** Truck loads, piles, sites, warehouse stock (purchased **and** built) and the price table all survive save/continue.
- **F18** A joining client immediately sees correct prices, pile contents, truck loads and site state without opening anything.

### Quality

- **Q1** `tools/compile-check.sh` exit 0 at every phase boundary.
- **Q2** Logic cases for the ledger, the pack, every rule and the drift surface, each proven able to fail once, mutations recorded in `context.md`.
- **Q3** Persistence round-trip cases for a truck load, a pile, a site, a purchased warehouse's stock, a **built** warehouse's stock and the price table; all sorted after `…_Capability_…`.
- **Q4** An RPC arity audit table in `context.md` covering all seven RPCs against their handlers.
- **Q5** No `array<…>` on any RPC; no `Rpc()` call wrapped in a helper; no `RplId` through a `ScriptInvoker`.
- **Q6** `Replication.BumpMe()` is reachable at most once per holder per finished request; `PublishContents` is its only call site.
- **Q7** No `Configs/Language/*.conf` modified; `.st` braces balanced with counts recorded before and after.
- **Q8** Comments sparse per `CLAUDE.md` — a line or two for a trap or a load-bearing ordering, never a rationale essay. Reasoning belongs in this document.
- **Q9** No float in any capacity comparison anywhere in the feature.
- **Q10** Every new `.et` and `.conf` has a `.meta`; every fresh GUID comes from `6A8E2E…` / `6B0E7A7…`; every inherited component GUID is copied.

### Integration

- **I1** `Scripts/Game/Data/OVT_StorageLedger.c`, `Scripts/Game/Components/OVT_StorageComponent.c`, `Scripts/Game/Components/Controller/OVT_StorageRequestComponent.c`, `Scripts/Game/UI/Context/OVT_StorageContext.c`, `Scripts/Game/UI/Context/OVT_TransferContext.c`, `Scripts/Game/Data/OVT_TransferListModel.c` and `Scripts/Game/Data/OVT_TransferCartModel.c` are **unmodified**.
- **I2** `OVT_EconomyManagerComponent.c` and `OVT_RealEstateManagerComponent.c` are unmodified (read-only consumers).
- **I3** `BuildItem`'s external signature is unchanged and every shipped caller compiles untouched.
- **I4** No `core/damage` file is modified; the feature only *calls* `OVT_StructureDamage` and *authors* an `OVT_StructureDestructionComponent` on its one new building prefab.
- **I5** No new hook on `OVT_TransferContext`; `GetSummaryText()` is the only override of a base virtual.
- **I6** `OVT_ShopContext`, `OVT_ShopTransactionComponent` and the shop screens are untouched.

### Verification method — an independent evaluator can follow this

**Static (no game):**
1. `cd "/mnt/n/Projects/Arma 4/Overthrow.Arma4" && tools/compile-check.sh` → exit 0.
2. `git diff --exit-code -- Scripts/Game/Data/OVT_StorageLedger.c Scripts/Game/Components/OVT_StorageComponent.c Scripts/Game/Components/Controller/OVT_StorageRequestComponent.c Scripts/Game/UI/Context/OVT_StorageContext.c Scripts/Game/UI/Context/OVT_TransferContext.c Scripts/Game/Data/OVT_TransferListModel.c Scripts/Game/Data/OVT_TransferCartModel.c` → clean (I1).
3. `git diff --exit-code -- Scripts/Game/GameMode/Managers/OVT_EconomyManagerComponent.c Scripts/Game/GameMode/Managers/OVT_RealEstateManagerComponent.c` → clean (I2).
4. `git status --porcelain Configs/Language/` → empty (Q7).
5. `grep -rn "float" Scripts/Game/Data/OVT_ResourceLedger.c` → only `TotalWeightKg` (Q9).
6. `grep -c "Rpc(" Scripts/Game/Components/Controller/OVT_ResourceRequestComponent.c` plus the manager's one broadcast matches the seven-row audit table in `context.md`, and each row's arity matches its handler (Q4).
7. `grep -rn "ComponentClassPersistenceConfigRule" Configs/Systems/Persistence/Overthrow.conf` → exactly **four** occurrences (Placeable, Buildable, Deployment, ResourcePile) and none names `OVT_ResourceStoreComponent` (D16).
8. `grep -n "CONFIG_STREAM_VERSION" Scripts/Game/Components/OVT_OverthrowConfigComponent.c` → the constant is `6`, and `RplSave`/`RplLoad` each gained exactly three appends after `repairCostMultiplier`.
9. `grep -rn "GetBasePrice" Scripts/Game --include=*.c` → every call site is the drift maths or the "relative to base" readout; **no** UI or request path reads it as a live price.
10. `grep -rn "OVT_Global\.\|GetGame().GetGameMode()" Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_Resource*.c` → no hits (Logic tier purity).
11. Only if an input `.conf` was touched: `python3 .claude/skills/overthrow-ui-patterns/scripts/check-input-conflicts.py` → exit 0 at the shipped baseline `0 error(s), 0 warning(s), 3 combo note(s), 0 pre-existing, 1 acknowledged.`
12. Orchestrator only, after each phase: `tools/run-tests.sh OVT_TEST_LogicSuite` / `OVT_TEST_InitSuite` / `OVT_TEST_PersistenceRoundTripSuite` / `OVT_TEST_CampaignSuite` **by class name**, one at a time (suites are not deterministic when several run back to back under load). Announce the focus steal first.

**Workbench (user-gated):** open the new `Wheeled_Truck_Base.et` delta and one child (`M923A1_transport.et`) and confirm the store, the cap override and all three HUD displays are present and inherited; open `OVT_ResourcePile.et`, `OVT_ConstructionSite.et`, `OVT_Warehouse.et`, `OVT_OverthrowController.et`, `Character_Player.et` and `OVT_OverthrowGameMode.et` without dropped-attribute warnings; confirm `Warehouse_01_Base.et` still shows the item storage component **and** the new resource store.

**Play-test A — single player, mouse:**
13. Start a campaign, drive an M923A1 to the port with ~$50,000. The port screen has a **Resources** tab in Import. Buy 60 Cement. Watch the frame: no hitch, no entities in the truck bed. The HUD shows `3.0 / 20.0 m³`.
14. Try to buy 10,000 Timber → refused **whole** with the remaining m³ in the message. Nothing is bought.
15. Leave the truck, get in a **car**, return to the port → resource rows are listed **disabled** with "needs a truck".
16. Drive to a base. Truck action "Cargo (3.0 / 20.0 m³)" → Take → destination **Ground** → 60 Cement. A crate pile appears. Repeat with 20 Steel → **the same pile grows**, no second pile. Open the pile → contents and total m³.
17. Open the map → the pile has a marker; its info panel lists the contents.
18. Build menu → **Garage** now shows a resource requirement beside its price. Place it → a construction site appears and **$8,000 is charged now**. "Requirements" lists needed vs nearby; "Build" is disabled and names the shortfall.
19. Haul the rest, unload near the site, "Build" → the site vanishes, the Garage appears, XP is awarded, emptied piles are gone.
20. **Empty-list regression:** build a Recruitment Tent → instant, money only, no site (F9).
21. Warehouse: walk to one you own → the resource action appears beside the storage action. Put 40 Timber in; Take 20 out; the counts are right on both.
22. Build menu → **Warehouse** → in a resistance-controlled town it is allowed; in an enemy-held town it is refused with a reason. Build it. Afterwards it is on the map, you own it, it takes items and resources, and the vehicle-menu warehouse buttons work on it.
23. Save (or Continue) → reload → truck load, both piles, the site (place a second one first), both warehouses' stock and the drifted prices are exactly as left.
24. Sleep/skip ~12 hours → at least one price has moved, moved by no more than the band allows, and the details panel says so. Sell at the port → the money matches the shown sell price.

**Play-test B — gamepad only (no mouse touched):**
25. Open the resource screen on a truck with two nearby holders. Something is focused on arrival; d-pad walks the list.
26. **D6, on a second consumer:** d-pad down to the destination picker; left/right changes the destination and **does not** move the focus column.
27. Build a cart, `a`/`KC_F` on Accept → the transfer runs, the cart clears, focus lands somewhere real.
28. Open the port screen, `RB`/`View` between Import and Export, `thumb_left`/`thumb_right` onto the **Resources** tab, buy and sell. `b` closes; `LB` still opens VON.
29. The site's two actions and the pile's action are all reachable and readable on a pad.

**Play-test C — dedicated server + JIP:**
30. Two clients. Client 1 buys resources and drops a pile. **Client 2 sees the pile, its map marker and its contents** without opening anything.
31. Client 1 places a site; Client 2's "Build" action shows the same shortfall text.
32. **JIP:** Client 3 joins after all of the above → sees the same prices as Client 1 (check one drifted resource against the server's), the pile's contents, the truck's cargo HUD when it gets in, and the site's state — with no action taken.
33. Two clients accept a cart against the same warehouse simultaneously → both succeed or one is refused with a reason; the total is exactly right afterwards.
34. Client 1 disconnects mid-transfer → nothing is duplicated or lost.

### Bug-report candidates for the orchestrator — do not file from this plan

- `OVT_VehicleMenuContext.c:90` shows the port button at `dist < 20` while every server port gate is 30 m (`OVT_VehicleRequestComponent.IMPORT_MAX_PORT_DISTANCE = 30`, `:60`; `OVT_StorageRequestComponent.EXPORT_MAX_PORT_DISTANCE = 30`). Pre-existing; this feature adds a third 30 m consumer and does not fix it.
- The Overthrow **Buildable** persistence config `{6B0E7A27C0D539F2}` carries no `OVT_StorageComponentSerializer`, so any *built* holder loses its item ledger on reload. Latent since `storage` shipped; fixed incidentally by binding 4 (§3.11).
- `Warehouse_01_Workshop.et` inherits `Building_Base.et` directly and so is a real-estate warehouse with neither an item ledger nor a resource store. A recorded `storage` residual; unchanged here.
- `OVT_StorageComponent`'s `EnsureTracked` is `Building.Cast`-gated (`:341`), so a *prop* holder is never lazily tracked. Not a defect for storage (its prop holders are placeables) but worth a look now that props hold value.

---

## 7. Testing Strategy

**Logic tier — `OVT_TEST_Logic_ResourceLedger.c` / `…_ResourceRules.c`** (world-free, `new`-built):

| Case | Claim | Proof it can fail |
|---|---|---|
| `LedgerAddClampsToCapacity` | `Add` returns what fitted in litres; total never exceeds the cap | drop the clamp → returns the full qty |
| `LedgerAddUnlimited` | `capacity < 0` fits everything | treat −1 as a literal cap |
| `LedgerTakeClampsAndDropsLine` | over-take returns what was held and **removes** the line; `LineCount()` falls | clamp-and-keep → line count stays |
| `LedgerTotalIsMaintained` | `TotalLitres` tracks add/take/clear across mixed ids | recompute from a stale field |
| `LedgerIgnoresGarbage` | empty id, `qty <= 0` and an id `defs` does not know are no-ops | remove the guards |
| `LedgerWouldFitIsExact` | a load that exactly fills the cap fits; one unit more does not | use `<` instead of `<=` |
| `LedgerWeightAggregates` | weight sums across mixed ids and never influences a fit decision | make `WouldFit` consult weight |
| `PackRoundTrips` | encode→decode reproduces every line; empty → `""` | drop a separator |
| `PackRejectsMalformed` | a bad token returns false and leaves the target untouched | return true on parse failure |
| `DriftClampsAtBothEdges` | a huge positive roll stops at `base × bandMax`; a huge negative at `base × bandMin`; never below 1 | drop one clamp |
| `DriftVolatilityScalesStepNotBand` | doubling volatility roughly doubles the step and **leaves both clamp edges identical** | multiply the band by volatility |
| `DriftMultiplierAppliesAfterClamp` | live = `round(stored × level)` and may exceed `base × bandMax` on Hard | apply the level multiplier before the clamp |
| `DriftOneStepPerWindow` | a second call in the same hour window is a no-op | drop the latch compare |
| `WarPressureDirection` | threat up ⇒ pressure up; controlled ports ⇒ pressure down; always in [−1, 1] | flip the control sign |
| `SellFollowsLive` | sell = `round(live × ratio)`, floor 1, no second walk | price the sell from `stored` |
| `MergeSelectsNearestThenLargest` | nearest within radius wins; equal distance → larger pile; outside radius → −1 | compare unsquared distances at the boundary; use an exact `==` on a 1000 m case (**`vector.Distance` is not correctly rounded**) |
| `RequirementScaleNeverZero` | `round(1 × 0.1)` is 1, not 0; `round(0 × 5)` is 0 | drop the floor |
| `RequirementSatisfied` | exact / short-by-one / surplus / summed across four piles; `shortId` names the first shortfall | sum only the nearest pile |
| `ConsumptionOrderIsNearestFirst` | a stable nearest-first order over four piles, second removed by index | use `array.Remove` (swap-with-last) instead of `RemoveOrdered` |
| `ImportablePredicate` | non-importable is refused on import and admitted on export | check the same flag both ways |
| `IllegalPredicate` (**D20**) | a resource flagged illegal in a hand-built `OVT_ResourceDefs` is refused without the permission and admitted with resistance port control | ignore the flag |
| `ReadoutNamesTheShortfall` | the formatted readout lists every requirement with needed/available and marks the short one | format only the short lines |

**Init tier — `OVT_TEST_Init_ResourceSeam.c`:** the manager resolves via `OVT_Global.GetResources()`; the config populates four definitions with non-zero litres and base price and unique ids; `OVT_ControllerComponent<OVT_ResourceRequestComponent>.Get()` resolves and sits on **this player's** controller entity (mandatory for every new controller component; `OVT_TEST_Init_VehicleRequestSeam.c:30-31` is the template); a spawned M923A1 transport resolves a store at 20000 litres, the test world's `Warehouse_01` (`Worlds/MP/OVT_Campaign_Test_Layers/default.layer:94`) and a spawned pile resolve one at −1, and a spawned civilian car resolves **none**. Polls are preconditions with a named failure on expiry, never retries; no `maxAttempts`.

**Persistence tier — appended to `OVT_TEST_PersistenceRoundTripSuite.c`**, all sorted after `…_Capability_…`, following the 7-phase state-machine template at `:8479` and dirtying state through the public facade before reloading:

| Case | Subject |
|---|---|
| `…_ResourceTruckLoad_RoundTrips` | a truck's ledger across `RequestInstanceReload` (the per-instance precedent is `…_VehicleReserveRelease_KeepsOwnerAndContents:2293`) |
| `…_ResourcePile_RoundTrips` | a spawned, tracked pile self-spawns with its contents |
| `…_ConstructionSite_RoundTrips` | buildable index, prefab index and angles survive; the site is completable after the reload |
| `…_WarehouseResources_RoundTrip` | the test world's purchased warehouse (Building config binding) |
| `…_BuiltWarehouseResources_RoundTrip` | a **built** warehouse (Buildable config binding — this is D15's proof) |
| `…_ResourcePrices_RoundTrip` | a hand-drifted price table reloads drifted, not at base |

**Campaign tier — `OVT_TEST_CampaignSuite`.** The test world has a port and a warehouse but **no garage**, so a construction case resolves its buildable by name: iterate `OVT_Global.GetResistanceFaction().m_BuildablesConfig.m_aBuildables` for `m_sName == "Garage"` and use that index (never a literal — `buildables.conf` order is not a contract, and 7 of 8 entries already have `m_sBuildableType != m_sName`). Two cases:

- **`…_PortPurchase_MovesMoneyAndResources`** — spawn a truck at the port, seat the test player, drive the `PORT_IMPORT` path server-side, assert money fell by `qty × live price` and the truck's ledger rose by `qty`.
- **`…_BuiltWarehouse_RegistersLikeAPurchasedOne`** — `BuildItem` the Warehouse index with a real `playerId` (TestWorld difficulty grants `startingCash 100000`), spawn a pile with the requirement, `CompleteSite`, then assert an `OVT_WarehouseData` record exists at the building's location, `GetConfig(building).m_IsWarehouse` is true, and `PlayerMayUseWarehouse(builderUid, building)` is true.

**What the automated spine cannot reach** — and therefore what the play-test gates exist for:

- **All multiplayer and JIP behaviour** (steps 30–34). The suites run one machine.
- **All UI and focus behaviour**, including the second exercise of `ui`'s D6 picker trap (steps 25–29).
- **Every real spawn and delete** — the pile merge, the site→building swap, the emptied-pile cleanup are proven by reading and by steps 16, 19.
- **The HUD** — no suite draws an `SCR_InfoDisplay` (step 13).
- **The map marker** and its self-registration on a runtime-spawned entity (step 17).
- **Balance.** Every number in §3.9 is a starting value; only steps 13–24 can say whether a garage is a truckload or a chore.
- **Workbench prefab resolution** — a same-GUID delta of a vanilla prefab is only truly proven by opening a child.

---

## 8. Quality Bar

**Backend**

- **B1 — Data integrity, provable by reading.** Every transfer validates the whole cart against live state *before* mutating either side; nothing clamps; a refused request mutates nothing. The worst outcome of a mid-request crash is a refused transfer, never a duplicated or lost line.
- **B2 — Server-authoritative without exception.** Every ledger and price mutation goes through `OVT_ResourceRequestComponent` or the manager's server-gated tick. No client writes a ledger, ever, including on a listen host. Client-side predicates (action labels, `ValidateCart`, the site readout) are advisory and the server re-derives all of them.
- **B3 — Per-call state only.** Every sphere-query accumulator is a `new`-ed object, never a static member. `OVT_InventoryManagerComponent.m_aContainerSearchResults` (`:497`) is the shared-accumulator defect this project keeps re-learning.
- **B4 — Rejections are visible.** Every refused request answers with a key the player sees. Silent returns and log-only rejections are the shape being avoided.
- **B5 — No `BumpMe` storms.** `PublishContents()` is the only writer of the replicated prop and is called once per finished request. Price broadcasts are at most one per resource per 6-hour window.
- **B6 — Persistence never applies a failed read.** Every `Read()` return checked; abort + ERROR; live state untouched. Persisted record classes are frozen and separate from live ones.
- **B7 — The wire is auditable.** Seven RPCs, each with its arity written down and checked against its handler, because the compiler will not do it.
- **B8 — No exact float comparison decides anything.** Litres are integers; distances are compared squared; the only floats are prices-in-progress, weights and config multipliers.

**UI**

- **B9 — Gamepad parity.** Every step of the resource screen, the port's Resources tab, the pile inspect and the two site actions is reachable with the d-pad, `x`, `y`, `RT`, `KC_F`/`left_trigger` and `b` alone.
- **B10 — Focus is never lost**, including across a live contents refresh while the cart is open and across an Accept.
- **B11 — The labels never lie.** "Cargo (12.5 / 20.0 m³)", "Need 12 Cement" and the port's price column all read from replicated state and are correct within one replication tick on every client. The price shown is the price charged.
- **B12 — No `ALWAYS_TOP` focusable widget**, and no hover target grown through the widget tree (the trace is clipped to parent bounds).

---

## 9. Dependencies

**Consumed, unmodified:**
- `logistics/ui` — `OVT_TransferContext` + both models + `TransferMenu.layout` `{6A8E2C1000000001}` + `ActionContext OverthrowTransferContext`. **Must not be modified** (I1, I5).
- `logistics/storage` — `OVT_StorageComponent` on the warehouse and the truck (a sibling component, never touched), `OVT_StorageUtils`, the `Warehouse_01_Base.et` delta as a host. **Must not be modified** (I1).
- `economy` — `OVT_EconomyManagerComponent`: `GetAllPorts`, `GetNearestPort`, `ResistanceControlsNearestPort`, `PlayerHasMoney`, `TakePlayerMoney`, `AddPlayerMoney`/`DoAddPlayerMoney`, `HasPermission("IllegalImports")`, `GetDayTimeMultiplier` via the time handler. Read-only (I2).
- `economy` — `OVT_RealEstateManagerComponent`: `SetOwnerPersistentId`, `GetConfig`, `PlayerMayUseWarehouse`, `GetNearestWarehouse`. Read-only (I2).
- `towns` — `OVT_TownManagerComponent.GetNearestTown` + `OVT_TownData.faction`; `OVT_OccupyingFactionManager.GetThreatFloat()`.
- `resistance` — `OVT_BuildContext`, `OVT_BuildableComponent`, the removal flow, `OVT_ItemLimitChecker`, `OVT_SkillManagerComponent.OnBuild`.
- `core/damage` — `OVT_StructureDamage.IsUsable`/`Resolve`; `OVT_StructureDestructionComponent` **authored** on the one new building prefab. **Do not modify any `core/damage` file** (I4).
- `core/controller-migration` — `OVT_OverthrowController`, `OVT_ControllerRequestComponent`, `OVT_ComponentFinder`, `OVT_ControllerComponent<T>.Get()`.
- `core/persistence` — `ScriptedComponentSerializer`, `OVT_PersistenceTracking`, `Overthrow.conf` rules.
- `map` — `OVT_MapMarkerComponent` / `OVT_MapMarkerManagerComponent` / `OVT_MapLocationType` and `OverthrowMap.conf`.
- Vanilla — `SCR_InfoDisplay`, `SCR_ConfigurableDialogUi`, `SCR_DestructionMultiPhaseComponent`, `BaseContainerTools`.

**Modified:** `OVT_BuildablesConfig`, `OVT_ResistanceFactionManager` (the `BuildItem` split), `OVT_BuildContext`, `OVT_PortContext`, `OVT_DifficultySettings`, `OVT_OverthrowConfigComponent`, `OVT_Global`, `OVT_MapMarkerComponent` (one enum value), `buildables.conf`, four difficulty `.conf`s, `OverthrowMap.conf`, `Overthrow.conf`, `overthrow_mapicons.imageset`, `localization_Overthrow.st`, and eight prefabs.

**New:** everything listed NEW in §3.1.

**Downstream, planned against this — `logistics/building-repair` calls exactly these:**

```
OVT_ResourceRequirements.NearbyAvailability(vector pos,
        notnull array<ref OVT_ResourceAmount> requirements,
        out array<ref OVT_ResourceAmount> available)                  // client-safe
OVT_ResourceRequirements.Consume(vector pos,
        notnull array<ref OVT_ResourceAmount> requirements)           // server only, all-or-nothing, returns bool
OVT_ResourceRequirements.ScaleForDifficulty(
        notnull array<ref OVT_BuildableResourceRequirement> conf,
        out array<ref OVT_ResourceAmount> scaled)
OVT_ResourceRules.IsSatisfied(need, have, out string shortId)
OVT_ResourceRules.FormatReadout(need, have, OVT_ResourceDefs defs)
```

Nothing in that list takes a site entity, a buildable index or a holder — position and amounts only, which is precisely so a `core/damage` ruin can be the second caller. `building-repair` stacks `repairCostMultiplier` on top of `ScaleForDifficulty`'s output; the composition order is *its* assertion to make.

**Concurrent work on this tree:** other sessions commit mid-feature. Re-baseline before every phase; every claim here carries a file:line so drift is detectable.

---

## 10. Risks & Mitigation

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| R1 | **Persistence-config collision on the buildable warehouse** — a built warehouse matches both the Buildable rule and the vanilla Building config, and only one wins | **Certain** (the collision is structural) | A built warehouse silently loses its item ledger **and** its resource stock on reload | D15 settles it with empirical evidence (the shipped Garage), binding 4 adds all three serializers to the Buildable config, and a dedicated Persistence case (`…_BuiltWarehouseResources_RoundTrip`) is the proof rather than the assumption |
| R2 | **Replicating pile/truck contents costs more than expected** — a `BumpMe` per line, or a long packed string on a busy server | Low–Medium | The spike `storage` exists to remove, reintroduced | One `RplProp`, one writer (`PublishContents`), one bump per finished request; Q6 is a Definition-of-Done grep; the MVP string is ~20 characters |
| R3 | **Price maths drifts on float** — binary32 rounding, or a step computed on a float price | Medium | Prices creep, a clamp edge is off by one, a Logic case is flaky | Prices are `int` end to end; only `delta` is float and it is immediately rounded; both clamp edges have their own case; the level multiplier is applied at read time so no rounding accumulates in stored state |
| R4 | **Merge-radius nondeterminism** — two piles at equal distance, or an exact-boundary comparison | Medium | A second pile spawns where the player expected a merge; a Logic case is flaky at 1000 m | `SelectMergeTarget` compares **squared** distances with a stable tie rule (larger pile wins); `vector.Distance`'s +1 ULP behaviour is why, and the case asserts with an epsilon, never on an exact boundary |
| R5 | **The `BuildItem` refactor regresses the instant path** — the handler-before-charge ordering is subtle and has bug history | Medium | Free buildings, or a charge for a building that never spawned | `FinishBuild` is lines `:872-922` **verbatim** with one `if (charge)` wrapper; the acceptance criterion is "proven by reading the diff"; play-test step 20 exercises an empty-requirement buildable; D17 leaves five buildables money-only as a standing regression check |
| R6 | **A removed site leaves orphaned state** — money charged, resources banked, a marker left behind | Low | Player-visible loss with no recourse | Money is charged at placement and never refunded (D2, stated in the help text); **nothing is ever banked into a site** (out of scope); the site is removed by the shipped flow and carries no other state |
| R7 | **Port category id collides** — id 9 is not free, or `GetPopulatedCategories` puts Resources somewhere surprising | Low | A resource row files itself under "Other", or the tab row rebuilds and eats gamepad focus | `OVT_ShopCategory.OTHER` is 8 (`OVT_ShopCategory.c:17`), ids are returned ascending, and `GetCategoryLabelKey` special-cases 9 **before** delegating to a helper whose fall-through is "Other" |
| R8 | **The `"res:"` id prefix reaches `GetInventoryId`** — routing added in the wrong order | Medium | The port imports *some other item* at a resource's price (id 0 is a real item) | The prefix split happens first in both `ValidateCart` and `OnAccept`; the acceptance criterion is to read the diff at `OVT_PortContext.c:283`; the trap is documented in place in the economy manager |
| R9 | **Listen-host re-entrancy** — the reply fan runs synchronously inside the ask, re-entering the screen | Medium | Infinite recursion or a list drawn from a half-filled model | Contents are replicated, so the screen fires **no** pull at all — the whole class of bug is designed out. The remaining synchronous path is Accept's result, which schedules a coalesced refresh rather than refreshing inline (`storage/context.md` Phase 6's recorded fix) |
| R10 | **The same-GUID `Wheeled_Truck_Base.et` delta does not take** — GUID typo, wrong parent path, or an `InfoDisplays`/`additionalActions` array replaced rather than merged | Low–Medium | Trucks silently cannot haul, or a shipped HUD/action disappears from every truck | GUID and parent line copied byte-for-byte; all three InfoDisplays restated with inherited GUIDs; an Init case resolves the store on a spawned transport truck; a Workbench check opens a child variant; play-test step 13 would show a missing wanted/economy HUD immediately |
| R11 | **The JIP stream bump breaks an in-flight client** | Low | A client cannot join (hard fail with a clear message, which is the designed behaviour) | The constant is bumped once, both directions appended in the same commit, and the mismatch path already logs the two versions (`:764-767`) |
| R12 | **Scope pressure toward `storage`** — "the truck already has a ledger, just add litres to it" | Medium | The epic's two-ledger wall breaks and both features get harder | I1's `git diff --exit-code` is a Definition-of-Done item; §2's out-of-scope list is explicit; the two components sit side by side on the same prefabs precisely so the boundary is visible |
| R13 | **Balance is wrong on first contact** — a garage costs three truckloads, or prices never move enough to matter | High | The sub-game is tedious or pointless | Every number in §3.9 and §3.4 is a prefab/`.conf` attribute, retunable without a script change; play-test steps 13–24 are the gate; the drift band, step fraction and merge radius are all attributes on the manager |
| R14 | **Concurrent sessions** change the tree between phases | Medium | Merge pain, stale line references, a phase built against a moved seam | Re-baseline before every phase; every claim here carries a file:line; `OVT_PortContext`, `OVT_ResistanceFactionManager` and `Overthrow.conf` are the three shared files most likely to move |
| R15 | **`ItemPreview` worldless instance** — the warehouse and the pile are previewable and `OVT_Component.OnPostInit` null-crashes on a worldless owner | Medium | A hard crash the moment a preview renders | Both new components carry the `!owner.GetWorld()` guard `OVT_StorageComponent.c:130-133` already ships; it is a Phase 3 acceptance criterion, not a code-review hope |

---

## Agent Routing Summary

| Phase | Agent | Why |
|---|---|---|
| 1 — pure spine + Logic cases | `component-developer` | Pure classes and test cases; no world, no networking |
| 2 — config + manager + `OVT_Global` | `component-developer` | A standard manager on a shipped idiom; no replication yet |
| **3 — store component + prefabs** | **`component-developer-advanced`** ⚠️ | A same-GUID delta of a **vanilla** prefab whose blast radius is every truck in the game, a new replicated prop prefab, an `RplProp` and two lifecycle traps (worldless owner, missing `RplComponent`) that compile-check cannot see |
| **4 — persistence** | **`component-developer-advanced`** ⚠️ | Property-name-keyed serialization with a silent total-loss failure mode, and a config-binding decision where the wrong choice hijacks vehicles or buildings from their existing configs |
| **5 — request component + wire + pile spawn** | **`network-specialist-advanced`** ⚠️ | Seven RPCs, a fan protocol, whole-cart atomicity, a six-step server gate, listen-host owner replies and BUG-090's compile blind spot |
| 6 — transfer screen + actions | `ui-developer` | A consumer on rails laid by `ui`, three action hosts. **Must not touch the base** |
| **7 — drift + difficulty + port category** | **`component-developer-advanced`** then `ui-developer` ⚠️ | A positional JIP stream bump in two directions, a new replicated+persisted state class, a drift formula with an asserted composition order — then a category on a shipped screen |
| **8 — construction** | **`component-developer-advanced`** ⚠️ | A refactor of the mod's build path with a recorded ordering bug history, a new persisted entity, two actions, a dialog and the helper surface a downstream feature is planned against |
| **9 — warehouse + buildable warehouse** | **`component-developer-advanced`** ⚠️ | The feature's biggest integration surface: real estate, `core/damage`'s retrofit, the persistence-precedence finding, a town-control gate on both sides of the wire, and a Campaign-tier proof |
| 10 — cargo HUD + map marker | `ui-developer` | An `SCR_InfoDisplay`, a layout, a map location type and an imageset quad — all on shipped rails |
| 11 — loc, conflict check, help & wiki | main thread + `help-docs-sync` | `.st` structural safety (unbalanced braces = data loss) and the wiki's known write failure modes |

**Parallelism:** Phases 7 and 8 touch disjoint files and **may run in parallel** once 5 and 6 land. Phase 10 may run in parallel with 8 and 9. Phase 9 depends on 3, 4 and 8. Phase 11 is last.

**Every implementation-agent prompt must carry, verbatim:**

> Do not run `tools/run-tests.sh`. Your gate is `tools/compile-check.sh` exit 0 — I run the test suites myself after the phase completes.
