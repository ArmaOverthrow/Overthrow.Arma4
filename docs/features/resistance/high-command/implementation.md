# High Command — Implementation Plan

**Status:** ✅ Done — play-test green, merged, closed 2026-08-22
**Started:** 2026-08-22
**Target Completion:** TBD
**Last Updated:** 2026-08-22 11:07

**Epic:** `resistance` (see `docs/features/resistance/epic-overview.md`)
**Requirements:** `docs/features/resistance/high-command/requirements.md` — authoritative for scope. Its closing **"Planning decisions (2026-08-22)"** section overrides the older body wherever they conflict.
**Approach:** **A — one feature, three components.** `OVT_HighCommandManagerComponent` (game-mode singleton: records, JIP, deltas, heartbeat, serializer, caps, rearm/refuel ticks) + `OVT_HighCommandGroupComponent` on the live group entity (observer, owned waypoints, stance, owner id) + `OVT_HighCommandRequestComponent` (the only client→server seam) + a map layer and two UI contexts. Garrison retirement is its own late phase. This mirrors the proven `OVT_RecruitManagerComponent` + `OVT_InactiveRecruitGroupComponent` split, keeps the 3.6k-line recruit manager untouched except at the conversion seam, and keeps one serializer and one record model.
**Branch:** `v1.5` (concurrent sessions exist on this tree — re-baseline before every phase; every citation below carries a file:line so drift is detectable)

---

## 1. Executive Summary

The resistance can buy AI, but it cannot **command** it. A garrison bought at `OVT_BaseMenuContext` (`:80`) is spawned at a fixed point by `OVT_ResistanceFactionManager.AddGarrison` (`:1471`), given a defend or 3-stop patrol cycle (`AddPatrolWaypoints`, `:1611`), and then never moves again for the rest of the campaign. It costs a hardcoded `(baseRecruitCost + 300) * memberCount` (`:1459`) whose own comment is `//To-do: properly calculate equipment cost (factoring in warehouse)` (`OVT_BaseMenuContext.c:61`). It is stored as a list of prefab names replayed at `PostGameStart` (`SpawnGarrisons()`, `:456`) — the live entities are never persisted, never despawned when a camp is removed (BUG-125), and its count row on the map reads 0 on every client (`OVT_MapDataKeys.c:38-40`).

High Command replaces it. A player buys a **named group** at a barracks, owns it by persistent id, and thereafter orders it anywhere on the map with a stance — Defend, Patrol or Attack. The group is an **always-live `SCR_AIGroup`** carrying a virtualization AI observer, so it fights real fights wherever it is standing, with or without a player nearby, and so the occupying faction's registered patrols materialise around it (`OVT_VirtualizationManagerComponent.AddEntityObserver`, `:3372`; contract `docs/features/virtualization/core/api.md` §3). Travel is real time; the map shows the group walking.

Four commitments shape everything below.

1. **Records are the truth; clients never read live entities.** The HC table ships JIP through `RplSave`/`RplLoad`, deltas through reliable broadcasts, and position + status through a 10 s heartbeat that is its **own** RPC — because `RpcDo_RecruitUpdated` is already at the 8-argument ceiling (`OVT_RecruitCommandComponent.c:3477`, BUG-090). The map layer, the roster and every label read the record.
2. **The `//To-do` gets fulfilled.** Group price = per member `baseRecruitCost`, plus, per required item, **either** a unit consumed from a registered warehouse in range (free) **or** the local shop buy price × `recruitLoadoutFeeMultiplier`. The same deduction is retro-fitted to the equipped-recruit purchase at the recruitment tent, which is what the TODO was actually asking for. All of it is quoted **server-side** — `CONFIG_STREAM_VERSION` does not move (currently **6**, `OVT_OverthrowConfigComponent.c:767`).
3. **HC must work before the legacy is removed.** Garrison retirement is Phase 11, not Phase 1. Both serializers get a **version bump with a read-and-discard branch**, never a field deletion — the binary contexts are positional and the OF serializer says so in place (`OVT_OccupyingFactionManagerSerializer.c:264`).
4. **Gamepad is specified, not hoped for.** The map has no usable order context today (`OVT_MapLayersUI.c:7-12`). HC ships its own `ActionContext` on the `OverthrowMapLayersContext` recipe — Priority 70, `Flags 0x26 0`, re-activated every frame — and reuses `TickHoverMagnet`'s cursor maths (`OVT_OverthrowMapUI.c:497-568`) so a stick gets the same assist a mouse does.

**Deferred, explicitly:** transferring a group to another player and officer takeover of an offline player's groups. Ownership is per-player and permanent in v1.

---

## 2. Goals

### Primary

1. **Buy a group.** A barracks desk action opens a purchase screen for any player; the purchasable set is new authored data on the FIA faction config, including vehicle groups; pricing consumes warehouse stock or charges for it.
2. **Own it forever.** Per-player persistent-id ownership, a member cap (default 48, server-configurable), and groups that persist indefinitely — never despawned on owner disconnect, unlike recruits' 600 s `OFFLINE_DESPAWN_TIME` (`OVT_RecruitManagerComponent.c:99`).
3. **Command it from the map.** Green NATO symbols on every player's map, own groups bright and others dimmed; select, move the cursor, press order; stance decides what happens on arrival. **Gamepad-complete from day one.**
4. **Fight where it stands.** Always-live groups with a virtualization observer each, resistance faction affiliation (so they already count toward QRF zone control — `7642a252`), and no virtualization registration.
5. **Convert a recruit squad.** The recruit roster's inactive section is displayed split by group and gains a one-way "Convert to High Command" per group; converted recruits leave the roster and cap and their bodies (and therefore their gear) carry over.
6. **Keep itself supplied.** A rearm tick tops groups up from registered warehouses in range; a refuel tick fills vehicle groups from any fuel source, charging the owner with the same maths as a player fill.
7. **Retire the garrisons.** Every purchase path, spawn helper, serializer field, map row, UI copy and GM registry origin, dropped — with a changelog warning that persisted garrisons are lost on load.
8. **Persist and JIP correctly.** A new `OVT_HighCommandManagerSerializer` (v1); whole-table JIP; a joining client sees every group's position, stance, destination and status without opening anything.
9. **Fulfil the warehouse TODO on the tent too.** The equipped-recruit quote shows how much of the kit is covered by nearby warehouse stock, and the purchase consumes it.

### Secondary

1. **One radius constant** — `OVT_HighCommandRules.WAREHOUSE_RANGE` — shared by the purchase quote, the tent discount, the rearm tick and every client pre-check. The BUG-070 lesson.
2. **One pure spine.** Cap arithmetic, stance validation, status-flag derivation, id minting and the covered-vs-charged sum are world-free classes, so the cheapest test tier pins the half where a wrong number is money.
3. **No new AI plumbing.** Stances are the shipped waypoint kit (`OVT_OverthrowConfigComponent.c:545-730`); vehicle seating is `OVT_VehicleSpawningDeploymentModule.SeatMember`'s shape (`:652-712`).
4. **`OVT_ItemLimitChecker` untouched.** A barracks counts against the base's shared pool like any other buildable (D14).

### Explicitly out of scope

- **Group transfer between players, and officer takeover of an offline player's groups.** Deferred to a follow-up feature (requirements, planning decisions 2026-08-22). No ownership-change code ships.
- **A Warehouse buildable.** Already shipped by `logistics/resources` (`Configs/Resistance/buildables.conf:143-173`, `OVT_ResistanceFactionManager.RegisterBuiltWarehouse:1002`). Do not plan or build one.
- **Virtual-vs-virtual combat resolution.** The reason HC groups are always live.
- **Stances beyond Defend / Patrol / Attack.** The enum is append-only; adding a fourth later is a `.conf`-free script change.
- **Converting HC groups back to recruits.** One-way by design.
- **Any change to how recruits work** beyond the roster split, the convert action and the tent quote's warehouse line.
- **QRF code.** `OVT_QRFControllerComponent.CheckUpdatePoints` (`:468`) already counts any resistance-faction AI agent (`:501-517`). HC's only obligation is correct faction affiliation.
- **Sourcing items from anything but a registered warehouse.** Not from an arbitrary `OVT_StorageComponent` holder, not from a truck, not from a pile.
- **Resource requirements on the Barracks buildable.** Money-only in v1; `m_aResourceRequirements` exists and adding one is a `.conf` edit (D14).
- **A wanted component on HC members.** D9 settles it and states the consequence.

---

## 3. Architecture Overview

### 3.1 The shape

```
                       SERVER                                    │            EVERY CLIENT
                                                                 │
  OVT_HighCommandManagerComponent   (on OVT_OverthrowGameMode)   │   same component, records only
  ├─ m_mGroups: groupId -> OVT_HighCommandRecord  ◄── THE TRUTH ─┼──►  m_mGroups (mirror)
  ├─ m_mGroupsByOwner                                            │      built by RplLoad + RpcDo_* deltas
  ├─ m_mEntityToGroup   (server only)                            │
  ├─ purchase / cap / supporter / money                          │   OVT_MapHighCommandLayer  reads records
  ├─ 10 s SweepStatus() ──► RpcDo_HCStatus (broadcast) ──────────┼──►  position + flags + alive count
  ├─ rearm tick  ──► warehouse ledgers                           │   OVT_HighCommandRosterContext reads records
  ├─ refuel tick ──► OVT_FuelUtils + TakePlayerMoneyPersistentId │
  ├─ OVT_HighCommandManifest  (spawn-inspect cache, load time)   │
  └─ OVT_HighCommandManagerSerializer  (v1)                      │
             │ owns                                              │
             ▼                                                   │
  SCR_AIGroup  (OVT_Group_HighCommand.et, ALWAYS LIVE)           │
  └─ OVT_HighCommandGroupComponent    server-only state          │
     ├─ groupId + ownerPersistentId                              │
     ├─ AddEntityObserver / RemoveEntityObserver (deferred/OnDelete)
     ├─ m_aOwnedWaypoints  -> removed-then-deleted in OnDelete   │
     └─ ApplyStance(move -> Defend | Patrol cycle | S&D)         │
                                                                 │
  OVT_HighCommandRequestComponent  (on OVT_OverthrowController)  │   the ONLY client -> server seam
     RpcAsk_QuoteGroup / PurchaseGroup / OrderGroup              │   identity via ResolveOwningPlayerId()
     RpcAsk_ConvertRecruitGroup / DismissGroup                   │   no identity argument on the wire
```

### 3.2 The pure spine — `Scripts/Game/Data/`

Everything here is world-free and Logic-tier testable. **No manager accessor, `OVT_Global` or `GetGameMode` identifier may appear anywhere under `TestSuites/Logic/`, comments included** — so no pure signature names a manager.

**`OVT_HighCommandRecord : Managed`** — the record, and the only thing a client ever reads.

| Field | Wire (JIP + deltas) | Save | Notes |
|---|---|---|---|
| `string m_sGroupId` | ✅ | ✅ | `hc_<ownerPersistentId>_<unixtime>_<hex6>` — the `OVT_RecruitData` id shape |
| `string m_sOwnerPersistentId` | ✅ | ✅ | |
| `string m_sEntryKey` | ✅ | ✅ | names the authored `OVT_HighCommandGroupEntry`; supplies display name, NATO icon quad, composition |
| `int m_iStance` | ✅ | ✅ | `OVT_EHighCommandStance { DEFEND, PATROL, ATTACK }` — **append-only** |
| `vector m_vDestination` | ✅ | ✅ | |
| `vector m_vLastKnownPosition` | ✅ | ✅ | refreshed by the heartbeat and by the save-point sweep |
| `int m_iStatusFlags` | ✅ | ✅ | `OVT_HighCommandStatus` bits |
| `int m_iAliveMembers` / `int m_iTotalMembers` | ✅ | ✅ | cap arithmetic + roster line |
| `array<string> m_aMemberBodyIds` | ❌ | ✅ | persisted body UUIDs (D8) |
| `array<ResourceName> m_aMemberPrefabs` | ❌ | ✅ | the fallback when a body does not come back (D8) |
| `EntityID m_GroupEntityId`, `EntityID m_VehicleEntityId` | ❌ | ❌ | `[NonSerialized]`, server only |

**`OVT_HighCommandStatus`** (pure, `OVT_RecruitStatus.c` is the template) — `CONTACT = 1`, `NO_AMMO = 2`, `MOVING = 4`, `IN_VEHICLE = 8`; `Derive(bool contact, bool anyAmmo, bool moving, bool mounted)`, `TagIcon(int flags)` returning a quad name from `overthrow_mapicons.imageset` `{C7691945DE01FB28}` (`""` when nothing to show), and `HasFlag(int flags, int bit)`.

**`OVT_HighCommandRules`** (pure statics, every one a Logic case):

```
static const float WAREHOUSE_RANGE  = 150.0;   // THE one radius (D6)
static const float ARRIVAL_RADIUS   =  25.0;   // "moving" is derived, not tracked
static const int   DEFAULT_MEMBER_CAP = 48;

static string MintGroupId(string ownerPersistentId, int unixTime, int salt)
static bool   IsStanceValid(int stance)
static bool   FitsUnderCap(int currentMembers, int incomingMembers, int cap)   // cap <= 0 = unlimited
static int    RemainingCapacity(int currentMembers, int cap)
static bool   IsMoving(vector position, vector destination)                    // squared distance
static int    SupportersRequired(int memberCount, int supportersPerMember)     // floor 0, never negative
static bool   IsDestinationLegal(vector destination)                           // finite, non-zero
```

**`OVT_ItemSourcingRules`** (pure; shared by the HC purchase **and** the tent discount, which is why it is its own class):

```
class OVT_ItemSourcingLine : Managed { string m_sResource; int m_iNeeded; int m_iUnitPrice; }

//! Splits a manifest into "covered by stock" and "charged", given how much stock each line has.
//! Returns the charged subtotal; fills coveredUnits/coveredValue for the UI.
static int SplitCoverage(notnull array<ref OVT_ItemSourcingLine> manifest,
                         notnull array<int> availablePerLine,
                         out int coveredUnits, out int coveredValue, out int chargedUnits)
```

Rule: per line, `covered = Math.Min(needed, available)`, `charged = needed - covered`, `chargedSubtotal += charged * unitPrice`, `coveredValue += covered * unitPrice`. Integers throughout; nothing clamps to a negative; an unknown/zero unit price contributes zero and is reported so the caller can refuse.

The fee multiplier is then applied to `chargedSubtotal` by the **existing** `OVT_RecruitPurchaseRules.GearFee(subtotal, multiplier)` (`:63`), and the member cost by `TotalPrice` (`:83`). Nothing about the fee maths is re-implemented.

### 3.3 Authored data — the purchasable set

`Configs/Factions/FIA_OverthrowData.conf` is four lines today (`m_sFactionKey`, `m_bIsPlayable`, `m_sFlagPrefab`); `OVT_Faction.m_aGroupPrefabSlots` (`:364`) is **not** an attribute — it is built in `Init()` (`:366-389`) from FIA's vanilla `GROUP` entity catalog (`Configs/EntityCatalog/FIA/Groups_EntityCatalog_FIA.conf`, 13 entries). That list is what the garrison UI enumerates and it is the wrong shape for HC: no prices, no icons, no vehicles, no curation.

So HC adds **one new authored attribute** on `OVT_Faction` beside `m_GroupRegistry` (`:311-313`):

```c
[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sKey")]
class OVT_HighCommandGroupEntry
{
    [Attribute(desc: "Stable key - the wire, the save and the icon all use it")]              string m_sKey;
    [Attribute(desc: "Display name (localization key)")]                                      string m_sTitle;
    [Attribute(desc: "Description (localization key)")]                                       string m_sDescription;
    [Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Source group prefab - its m_aUnitPrefabSlots IS the composition", params: "et")]
                                                                                              ResourceName m_sGroupPrefab;
    [Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Optional vehicle - empty = foot group", params: "et")]
                                                                                              ResourceName m_sVehiclePrefab;
    [Attribute(defvalue: "Infantry_Friend", desc: "ICO_Land quad name for the map symbol")]    string m_sMapIcon;
}

[Attribute(desc: "Groups a player may buy at a barracks", category: "High Command")]
ref array<ref OVT_HighCommandGroupEntry> m_aHighCommandGroups;
```

The `[Attribute]` shape is copied verbatim from `m_aVehiclePrefabSlots` (`OVT_Faction.c:343-344`) and `m_sFlagPrefab` (`:358-359`). The `.conf` authoring syntax is `USSR_OverthrowData.conf:3-57`'s. **Every nested object needs a unique 16-hex id** — reserve a fresh series (§3.12).

**The v1 FIA list** (member counts from the vanilla group prefabs; GUIDs recovered from inbound references — the extracted reference tree ships no `.et.meta`, so re-verify each against Workbench before authoring):

| `m_sKey` | Group prefab | Members | Vehicle | Map icon |
|---|---|---|---|---|
| `sentry_team` | `{6E725D44CA973C24}Prefabs/Groups/INDFOR/Group_FIA_SentryTeam.et` | 2 | — | `Infantry_Friend` |
| `mg_team` | `{22F33D3EC8F281AB}Prefabs/Groups/INDFOR/Group_FIA_MachineGunTeam.et` | 2 | — | `Infantry_Friend` |
| `sharpshooter_team` | `{6307F42403E9B8A4}Prefabs/Groups/INDFOR/Group_FIA_SharpshooterTeam.et` | 2 | — | `Sniper_Friend` |
| `recon_team` | `{2E9C920C3ACA2C6F}Prefabs/Groups/INDFOR/Group_FIA_ReconTeam.et` | 2 | — | `Recon_Friend` |
| `light_fireteam` | `{1BB20A4B3A53D0F5}Prefabs/Groups/INDFOR/Group_FIA_LightFireTeam.et` | 4 | — | `Infantry_Friend` |
| `at_team` | `{2CC26054775FBA2C}Prefabs/Groups/INDFOR/Group_FIA_Team_AT.et` | 4 | — | `Antitank_Friend` |
| `fireteam` | `{5BEA04939D148B1D}Prefabs/Groups/INDFOR/Group_FIA_FireTeam.et` | 5 | — | `Infantry_Friend` |
| `rifle_squad` | `{CE41AF625D05D0F0}Prefabs/Groups/INDFOR/Group_FIA_RifleSquad.et` | 7 | — | `Infantry_Friend` |
| `technical_pkm` | `{6E725D44CA973C24}…Group_FIA_SentryTeam.et` | 2 | `{22B327C6752EC4D4}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_PKM_FIA.et` | `Motorized_Friend` |
| `motor_fireteam` | `{1BB20A4B3A53D0F5}…Group_FIA_LightFireTeam.et` | 4 | `{F7E9AA0C813EABDA}Prefabs/Vehicles/Wheeled/UAZ469/UAZ469_FIA.et` | `Motorized_Friend` |
| `truck_squad` | `{5BEA04939D148B1D}…Group_FIA_FireTeam.et` | 5 | `{16E32C3ABEAFC2C6}Prefabs/Vehicles/Wheeled/Ural4320/Ural4320_FIA_transport_covered.et` | `Motorized_Friend` |

⚠️ **There is no such thing as a vanilla vehicle group.** Every `Prefabs/Groups/**` file in the reference tree spawns characters only; `SCR_AIGroup.m_aStaticVehicles` (`:87-88`) is an `array<string>` of *world-editor entity names*, useless at runtime. `Group_US_Transport.et` is two drivers and an empty `SCR_TransportUnitComponent`. So a "vehicle group" is composed by HC: crew group prefab + vehicle prefab, seated on spawn (D5).

### 3.4 Composition and the required-item manifest

Two different reads, answered two different ways.

**Composition — read from the prefab source, no spawn.** `SCR_AIGroup.GetMembers(IEntitySource entitySource, out array<ResourceName> outPrefabs, out array<vector> outOffsets)` is `static` (`SCR_AIGroup.c:20`) and does `entitySource.Get("m_aUnitPrefabSlots", outPrefabs)` (`:48`). One `Resource.Load(entry.m_sGroupPrefab)` → `GetResource().ToEntitySource()` per entry gives the exact member prefab list with nothing spawned. This replaces the garrison UI's spawn-a-group-per-open (`OVT_FOBMenuContext.c:40-59`), which is the banned shape.

**Required items — spawn-inspect once, cached (D4).** Gear on a vanilla FIA character is authored across **five** mechanisms spread over a three-deep inheritance chain — verified on `Character_FIA_Rifleman.et` / `Character_FIA_BaseLoadout.et`:

1. `CharacterWeaponSlotComponent.WeaponTemplate` (primary)
2. `CharacterGrenadeSlotComponent.WeaponTemplate` (grenade)
3. `BaseLoadoutManagerComponent.Slots[].Prefab` (hat, jacket, pants, boots, vest)
4. `SCR_InventoryStorageManagerComponent.InitialInventoryItems[].PrefabsToSpawn[]` (magazines, medical, gadgets — the bulk of the value)
5. `SCR_EquipmentStorageComponent.InitialStorageSlots[].Prefab`, nested inside `SCR_CharacterInventoryStorageComponent.components`

Reading that from `IEntitySource` means hand-walking five component types across an inheritance chain the engine resolves for free. A spawned character resolves all five and the whole chain in one read, and Overthrow already owns a live-entity inventory walker.

`OVT_HighCommandManifest` (server only, on the manager) therefore:

- At `PostGameStart`, collects the **distinct** character prefabs across every `m_aHighCommandGroups` entry (typically 10–15 for the list above).
- Spawns them **one per call-queue hop**, at a scratch position (campaign start position, terrain-clamped, offset well outside any base), captures, `SCR_EntityHelper.DeleteEntityAndChildren`, next.
- Capture is deferred **one further frame** after spawn (`InitialInventoryItems` land during init, not synchronously with the spawn call), then retried up to `MANIFEST_CAPTURE_ATTEMPTS = 5` frames while the inventory reads empty. On expiry it records an empty manifest for that prefab and logs a WARNING naming it — an unpriceable member costs `baseRecruitCost` only, it never blocks a purchase.
- Every scratch character is `OVT_PersistenceManagerComponent.UntrackTransient(entity)`'d before deletion.
- Result: `map<ResourceName, ref array<ref OVT_ItemSourcingLine>>` keyed by character prefab; a group's manifest is the per-resource sum over its members, computed on demand and cached per entry key.
- The cache is idempotent and **lazily rebuilt on a miss**, so a quote that arrives before the load-time build finishes is correct rather than wrong.

Unit prices come from the shipped path, with the shipped guard: `economy.IsRegisteredResource(res)` **before every** `GetInventoryId` (an unregistered prefab resolves to id 0 = someone else's price; `GetPrice` answers 500 for an unknown id), then `GetBuyPrice(id, pos, playerId)` — exactly `OVT_RecruitLoadoutPricing.AddResource` (`:196-215`).

### 3.5 The group entity

**`Prefabs/Groups/INDFOR/OVT_Group_HighCommand.et`** — ONE prefab, inheriting `{242BC3C6BCE96EA5}Prefabs/Groups/INDFOR/Group_FIA_Base.et` (the precedent: `Prefabs/Groups/INDFOR/Group_CIV.et` line 1 does exactly this). `m_aUnitPrefabSlots` **empty**, `m_bPlayable 0`, `m_bDeleteWhenEmpty 1`, lifecycle policy **Manual**, `RplComponent` present. Members are spawned by the manager and joined with `SCR_PlayerControllerGroupComponent.AddAIToSlaveGroup` — the `OVT_RecruitManagerComponent.c:2406` path, which is server-side and does not depend on the owner's client being responsive.

⚠️ **Never `SetLifecyclePolicy`.** `ProximityDriven` deletes bodies at 800 m; Manual is what "always live" means. This is the same rule `OVT_InactiveRecruitGroupComponent` states in its header.

⚠️ **`m_bDeleteWhenEmpty 1` deletes the group one frame after its last member leaves** (`SCR_AIGroup.OnEmpty` → `CallLater(DeleteEntityAndChildren, 1)`). That is correct for a wiped HC group — but it also means the group must gain its first member in the frame it is created. The manager spawns and joins in the same call, as `CreateInactiveGroupFor` (`OVT_RecruitManagerComponent.c:2673`) does.

**`OVT_HighCommandGroupComponent : ScriptComponent`** — server-only state, the `OVT_InactiveRecruitGroupComponent` shape copied deliberately:

| Member | Notes |
|---|---|
| `m_sGroupId`, `m_sOwnerPersistentId` | set by the manager immediately after spawn |
| `m_aOwnedWaypoints : array<AIWaypoint>` | plain pointers, not `ref` — entity lifetime belongs to the engine |
| `OnPostInit` | server guard → `CallLater(InstallObserver, 0, false)` — deferred one frame because an entity mid-spawn answers `GetID()` with `EntityID.INVALID`, which core refuses (api.md §3) |
| `InstallObserver` | reads the **new** gate `virtualization.GetHighCommandGroupsAreObservers()` (D3), then `AddEntityObserver(owner)`, WARNING on refusal |
| `OnDelete` | `Remove(InstallObserver)` **first**, then `RemoveEntityObserver(owner)` **unconditionally**, then per owned waypoint `group.RemoveWaypoint(wp)` **then** `SCR_EntityHelper.DeleteEntityAndChildren(wp)`, then notify the manager (record removal + broadcast) |
| `AddOwnedWaypoint(AIWaypoint)` / `ClearOwnedWaypoints()` | `ClearOwnedWaypoints` is called by every re-order, using the same remove-then-delete order (`OVT_MultiTownPatrolBehaviorDeploymentModule.c:229-230`) |

⚠️ **Never `OVT_EntitySpawningAPI.CleanupGroup` / `CleanupEntity`** on an HC group — the recruit-ux rule, restated because this is the second consumer.

**Stances** (`ApplyStance`), all waypoints spawned through `OVT_OverthrowConfigComponent` and every one handed to `AddOwnedWaypoint`:

| Stance | Waypoints |
|---|---|
| DEFEND | `SpawnMoveWaypoint(dest)` (`:559`) → `SpawnDefendWaypoint(dest, 0)` (`:622`) |
| PATROL | `SpawnMoveWaypoint(dest)` → `GivePatrolWaypoints(group, OVT_PatrolType.…, dest, PATROL_RADIUS)` (`:677`) — road-snapped ring of `[patrol → wait]` in an `AIWaypointCycle` with `SetRerunCounter(-1)` |
| ATTACK | `SpawnSearchAndDestroyWaypoint(dest)` (`:573`) |

Every waypoint position is terrain-clamped with `world.GetSurfaceY(x, z)` before spawning — the `OVT_QRFControllerComponent.CreateWaypoint` clamp (`:914-917`), for the same reason.

**Vehicle groups** — on spawn, the vehicle is placed first (`OVT_WorldUtils` safe-position helper, angles from the barracks), then each member is seated with the `OVT_VehicleSpawningDeploymentModule` shape: `SCR_CompartmentAccessComponent.MoveInVehicle(vehicle, type)` (`:702-712`), driver → gunner/turret (`MoveInVehicle(vehicle, ECompartmentType.TURRET, false, emptyTurret)`, `:302-315`) → cargo. ⚠️ `MoveInVehicle` locks the slot it hands out for a frame (`:652`), so seating is **one member per call-queue hop**, not a tight loop. The vehicle entity id is held on the group component so the refuel tick and the DISMISS path can find it.

### 3.6 The seam — `OVT_HighCommandRequestComponent`

`class OVT_HighCommandRequestComponent : OVT_ControllerRequestComponent`, authored on `Prefabs/GameMode/OVT_OverthrowController.et` **before** the trailing `RplComponent "{65C4B2D3DE955867}"` (currently the last of 25 blocks). It inherits `ResolveOwningPlayerId()` and `ShouldRespondLocally(int)`. **No request carries an identity argument.**

⚠️ **RPC arity is a compile blind spot** (BUG-090 — `Rpc()` is untyped variadic). The table below is the audit; it is re-checked against the handlers in Phase 6 and written into `context.md`.

Client → server, `[RplRpc(RplChannel.Reliable, RplRcver.Server)] protected`:

| # | Signature | Arity |
|---|---|---|
| 1 | `RpcAsk_QuoteGroup(int entryIndex)` | 1 |
| 2 | `RpcAsk_PurchaseGroup(int entryIndex)` | 1 |
| 3 | `RpcAsk_OrderGroup(string groupId, int stance, vector destination)` | 3 |
| 4 | `RpcAsk_ConvertRecruitGroup(string anchorRecruitId)` | 1 |
| 5 | `RpcAsk_DismissGroup(string groupId)` | 1 |

Server → owner, `[RplRpc(RplChannel.Reliable, RplRcver.Owner)] protected`:

| # | Signature | Arity |
|---|---|---|
| 6 | `RpcDo_HCQuote(int entryIndex, int total, int coveredValue, int memberCount, int supportersNeeded, int refusalCode)` | 6 |
| 7 | `RpcDo_HCResult(int resultCode, int charged, string groupId)` | 3 |

Server → all, `[RplRpc(RplChannel.Reliable, RplRcver.Broadcast)]`, **on the manager**:

| # | Signature | Arity |
|---|---|---|
| 8 | `RpcDo_HCCreated(string groupId, string ownerPersistentId, string entryKey, vector position, int totalMembers)` | 5 |
| 9 | `RpcDo_HCRemoved(string groupId)` | 1 |
| 10 | `RpcDo_HCOrdered(string groupId, int stance, vector destination)` | 3 |
| 11 | `RpcDo_HCStatus(string groupId, vector position, int statusFlags, int aliveMembers)` | 4 |

**#11 is the heartbeat and is deliberately its own RPC** with four parameters, because the recruit equivalent it is modelled on is already at the eight-parameter ceiling and had nowhere to grow.

No `array<…>` on any RPC. Every `RpcDo_*` takes the `ShouldRespondLocally(playerId)` direct branch **first** (owner RPCs are silently dropped on a listen host), then `Rpc()`. Every refusal answers `RpcDo_HCResult` with a code — a silent return is a defect.

**No `RplId` on the wire at all.** The barracks is found by a server-side sphere query for `OVT_BarracksComponent` within `BARRACKS_USE_RADIUS` of the caller's own entity (D7), which removes the "does this vanilla building even have an `RplComponent`?" question entirely.

**Result codes** `RESULT_OK = 0` … extend-never-reuse, the `OVT_RecruitCommandComponent.RESULT_*` convention (`:156-260`): `NO_PLAYER`, `NO_ACTOR`, `NOT_AT_BARRACKS`, `BARRACKS_RUINED`, `BASE_NOT_FRIENDLY`, `BAD_ENTRY`, `AT_CAP`, `NO_SUPPORTERS`, `CANNOT_AFFORD`, `SPAWN_FAILED`, `NO_GROUP`, `NOT_OWNER`, `BAD_STANCE`, `BAD_DESTINATION`, `NO_RECRUIT_GROUP`, `CONVERT_AT_CAP`.

**The purchase gate ladder**, in order — the funds check is last (requirements' ordering law):

1. `playerId <= 0` → `NO_PLAYER`
2. caller entity resolves → `NO_ACTOR`
3. an `OVT_BarracksComponent` within `BARRACKS_USE_RADIUS` (5 m) of the caller → `NOT_AT_BARRACKS`
4. `OVT_StructureDamage.IsUsable(barracks)` → `BARRACKS_RUINED`
5. nearest base within `m_Difficulty.baseRange` and `!IsOccupyingFaction()` → `BASE_NOT_FRIENDLY`
6. `entryIndex` in range of `m_aHighCommandGroups` → `BAD_ENTRY`
7. `OVT_HighCommandRules.FitsUnderCap(ownerMembers, entryMembers, cap)` → `AT_CAP`
8. `towns.NearestTownHasSupporters(pos, SupportersRequired(members, perMember))` → `NO_SUPPORTERS`
9. `economy.PlayerHasMoney(persId, total)` → `CANNOT_AFFORD`

**The effect order**, once every gate passes: **spawn → equip/consume warehouse stock → take supporters → take money.** Spawn failure charges nothing. The quote is re-derived server-side at purchase time (stock may have moved since it was shown) and the re-derived total is what is checked and charged — the `RpcAsk_BuyEquippedRecruit` precedent (`OVT_RecruitCommandComponent.c:837`). The screen labels its number as an estimate.

### 3.7 Warehouse sourcing

`Scripts/Game/Utilities/OVT_WarehouseStockUtils.c` — server-side, per-call query objects, never a static accumulator:

```
//! Registered warehouses ONLY, in range, that this player may use.
static int CollectStores(vector pos, float radius, string persistentId, out array<OVT_StorageComponent> stores)
static int CountAvailable(notnull array<OVT_StorageComponent> stores, string res)
static int TakeUpTo(notnull array<OVT_StorageComponent> stores, string res, int qty)   // SERVER ONLY
```

`CollectStores` runs **one** `OVT_StorageHolderQuery` (`OVT_StorageUtils.c:125-186`) at `pos` with `radius`, then keeps a holder only when (a) some record in `OVT_RealEstateManagerComponent.m_aWarehouses` (`:39`, public) sits within `WAREHOUSE_MATCH_RANGE` (`:42`, 10) of it, and (b) `realEstate.PlayerMayUseWarehouse(persistentId, holder)` (`:624`) is true. That is "registered warehouses only", with the shipped privacy/rental rule, and it modifies **no** real-estate code.

`TakeUpTo` drains stores in query order via `OVT_StorageLedger.Take(res, qty)` (`:90`), returns the total taken, and calls `PublishCount()` on each store it touched exactly once.

⚠️ **Stock is not replicated** (`logistics/storage` pulls on open). Every coverage figure is therefore computed **server-side** and quoted; a client never computes one. This is the same reason `recruitLoadoutFeeMultiplier` never entered the config stream, and it is why `CONFIG_STREAM_VERSION` does not move for this feature.

### 3.8 The tent discount (the shipped TODO's other half)

Two small, additive changes:

- **`OVT_RecruitLoadoutPricing`** — `AddResource` (`:196`) already visits every resource exactly once; it gains a parallel per-resource manifest on `OVT_RecruitLoadoutPrice` (`array<ref OVT_ItemSourcingLine> m_aManifest`, appended after `m_sUnpriceableResource`). `m_iSubtotal` keeps its present meaning (everything, uncovered), so nothing that reads it changes.
- **`OVT_RecruitCommandComponent.BuildQuote`** (`:1014-1045`) — after `Price(...)`, run `OVT_WarehouseStockUtils.CollectStores(tentPos, WAREHOUSE_RANGE, persId, …)` and `OVT_ItemSourcingRules.SplitCoverage(...)`, feed the **charged** subtotal to `OVT_RecruitPurchaseRules.TotalPrice` instead of the raw one, and put `coveredValue` on the quote. `RpcDo_RecruitQuote(string loadoutName, int price, int itemCount, int refusalCode, string unpriceableResource)` (`:1107`) gains one `int coveredValue` → **arity 6**, still under the ceiling. `OVT_LoadoutsContext.OnRecruitQuote` (`:384`) shows the extra line. On purchase (`RpcAsk_BuyEquippedRecruit`, `:837`) the stock is consumed with `TakeUpTo` **before** money is taken.

### 3.9 Map command & control

**The layer** — `Scripts/Game/UI/Map/Visualization/OVT_MapHighCommandLayer.c`, an `SCR_MapUIBaseComponent` modelled line-for-line on `OVT_MapRecruitLocation.c` (450 L): `m_Widgets : map<string, ref Widget>` keyed by **group id**, widgets created with `workspace.CreateWidgets(m_Layout, m_RootWidget)` and destroyed with `RemoveFromHierarchy()`, positioned by `m_MapEntity.WorldToScreen(...)` + `DPIUnscale` + `FrameSlot.SetPos` (`:300-318`), everything volatile re-read per frame in `Update()`.

- **Symbols.** Base quad from `{8479B3B5347DF5CF}UI/Imagesets/MilitarySymbol/ID_D.imageset` — `Friend_Land_Bcg` normally, `Friend_Focus_Land` under the cursor, `Friend_Select_Land` when selected — tinted **green** with `OVT_MapFactionPalette.GetColorForRole(RESISTANCE_FACTION)`, the `OVT_MapRecruitLocation.c:175` call. The unit-type glyph rides a sibling widget from `{27F2439D610D02B3}UI/Imagesets/MilitarySymbol/ICO_Land.imageset` using the entry's `m_sMapIcon` (`Infantry_Friend`, `Motorized_Friend`, `Sniper_Friend`, `Recon_Friend`, `Antitank_Friend`). There is **no colour-variant imageset** — affiliation is shape (quad name) plus tint (`SetColor`).
- **Opacity is one owner's property.** `Update()` sets 1.0 for own groups and `OTHER_OWNER_OPACITY = 0.45` for everyone else's — the `INACTIVE_MARKER_OPACITY` precedent (`OVT_MapRecruitLocation.c:23`). `SetMarkersVisible` uses `SetVisible`, **never** `SetOpacity` (`:363-385`). Selection is a **third** channel — the ID_D quad swap — so no two rules fight over one property.
- **Badges.** A red `!` tag when `CONTACT` is set and an ammo badge when `NO_AMMO` is set, on a sibling `TagImage`, change-filtered through an `m_mTagQuads` map so `LoadImageFromSet` is not called per frame (the recruit layer's own optimisation).
- **Registration.** One block in `Configs/Map/MapOverthrow.conf` `m_aUIComponents` (`:56-82`) — the `OVT_MapRecruitLocation` block at `:66-71` is the template, including its `OVT_MapFactionPalette` child.
- **Filter row.** `OVT_MapLayersUI` gains a **third** hand-built row beside `BuildPlayerRow` (`:733-747`) and `BuildRecruitRow` (`:760-774`): `KEY_HIGHCOMMAND = "highcommand"`, `LABEL_HIGHCOMMAND = "#OVT-Map_Layer_HighCommand"`, an `ApplyHighCommandMarkerPreference()` beside `:990`, and an `ApplyHighCommandMarkers` branch inserted into `ApplyOne` (`:1064-1079`) **before** `ApplyCanvasLayer` — the hand-built rows share the `layer:` namespace and must be tested first. Gate on `IsAvailableThisSession()` so a campaign with no HC groups gets no row.

**Selection and ordering.**

- Selection uses the same cursor maths as `OVT_OverthrowMapUI.TickHoverMagnet()` (`:497-568`): `m_MapEntity.GetMapCursorWorldPosition(x, z)`, `radiusWorld = radiusPx / m_MapEntity.GetCurrentZoom()`, nearest marker within radius, squared comparison. The gamepad's virtual cursor updates the same value, so a pad gets the identical assist.
- **Click / `MenuSelect` on a marker selects.** Moving the cursor and pressing the **order** action issues `RpcAsk_OrderGroup(groupId, stance, cursorWorldPos)`. The **stance** action cycles Defend → Patrol → Attack for the selected group and re-issues at the same destination.
- Selecting **any** group — owned or not — shows a small info panel with owner, stance, destination and status. Ordering is refused server-side for a group the caller does not own (`NOT_OWNER`).

**The input context** — `Configs/System/chimeraInputCommon.conf`:

```
ActionContext OverthrowMapCommandContext {
 Priority 70
 Flags 0x26 0
 ActionRefs { "MenuUp" "MenuDown" "MenuLeft" "MenuRight" "MenuSelect"
              "OverthrowHCOrder" "OverthrowHCStance" }
}
```

Priority 70 / `Flags 0x26 0` is the `OverthrowMapLayersContext` recipe (`:1188-1198`), chosen because vanilla `MapContext` is Priority 50 / `Flags 0x6c 0`. Two new `Action` blocks on the `OverthrowRespawnHere` shape (`:679-695`): `OverthrowHCOrder` = `keyboard:KC_G` + `gamepad0:x`; `OverthrowHCStance` = `keyboard:KC_B` + `gamepad0:y`. **`gamepad0:shoulder_left` is VON at priority 110 and is banned** (BUG-092). Activation is a **single-frame lease** re-asserted every frame from `Update()`, with the **one-frame arm delay** (`m_bNavContextArmed`, `:227-246`) — without it the `a` that selects a group is re-offered to the order handler on the same frame. `python3 .claude/skills/overthrow-ui-patterns/scripts/check-input-conflicts.py` must stay at its baseline (`0 error(s), 0 warning(s), 3 combo note(s), 0 pre-existing, 1 acknowledged`).

### 3.10 The barracks

Two hosts, one action, one marker component.

**`OVT_BarracksComponent : ScriptComponent`** — a stateless marker (the `OVT_ResourcePileComponent` role). Its presence is what the server's sphere query and the client's action gate both look for. Nothing replicates.

**(a) The buildable.** `Prefabs/Structures/Military/Barracks/OVT_Barracks.et`, modelled on `Prefabs/Structures/Industrial/Houses/Warehouse_01/OVT_Warehouse.et:1-35`: inherit a vanilla barracks leaf, add `OVT_BuildableComponent { m_sBuildableType "Barracks" }`, `OVT_BarracksComponent`, `OVT_StructureDestructionComponent` (the `core/damage` retrofit — `m_PhaseModel` must be a **bare `.xob`**), `RplComponent { Enabled 1 }`, and a child desk prop carrying the action (below). One `OVT_Buildable` entry in `Configs/Resistance/buildables.conf` after the Warehouse entry (`:143-173`): `m_sName "Barracks"`, `m_bBuildAtBase 1` only, `m_iCost 6000`, `m_iRewardXP 40`, **no** `m_aResourceRequirements` (D14).

**(b) The vanilla overrides.** Same-GUID deltas of the **`_base.et`** prefabs, which every world-placed leaf inherits — the leaf GUIDs are not recoverable from the extracted reference tree, and `_base` is better granularity anyway (`Warehouse_01_Base.et`, `ShopHouse_E_2I01t_Base.et` and the transmitter towers are all shipped precedents). v1 targets three:

| Override path (identical to vanilla) | `.et.meta` `Name` GUID (vanilla's) |
|---|---|
| `Prefabs/Structures/Military/Houses/Barracks_01/Barracks_01_military_base.et` | `{5F97E54397247954}` |
| `Prefabs/Structures/Military/Houses/Barracks_01/Barracks_USSR_01_military_base.et` | `{2CB4D91249389DFD}` |
| `Prefabs/Structures/Military/Houses/Barracks_E_02/Barracks_E_02_base.et` | `{60A21DFAFF77773D}` |

The shape is the sleep bed override, verbatim (`Prefabs/Props/Furniture/Bed_01.et` + `.et.meta`): **line 1 repeats the vanilla file's own header** (entity class + parent GUID + parent path) byte-for-byte, **line 2 repeats the vanilla file's own `ID`**, and the `.et.meta` `Name` carries the **vanilla resource GUID** with the identical path. A wrong GUID produces an orphan nothing instances — the safe failure direction, but it is a Workbench-gated check.

Each override adds `OVT_BarracksComponent` and a child desk entity — `StaticModelEntity : "{7762F50A860DD074}Prefabs/Props/Military/Furniture/TableMilitary_US_01.et"` with an `ActionsManagerComponent` / `UserActionContext { ContextName "hcdesk"; Position PointInfo { Offset … }; Radius 0.65 }` / `additionalActions { OVT_ManageHighCommandAction }`, copied structurally from `OVT_RecruitmentTent.et:105-156`. The desk's `coords` inside each barracks are **hand-tuned in Workbench** — that is the one step of this feature that cannot be done from the CLI.

**`OVT_ManageHighCommandAction : ScriptedUserAction`** — the `OVT_BuyEquippedRecruitAction` shape (`:24-187`):

```
CanBeShownScript(user):
    OVT_StructureDamage.IsUsable(GetOwner())                       // core/damage D15 ruin gate
 && nearest base within m_Difficulty.baseRange
 && !baseController.IsOccupyingFaction()                            // OVT_ManageBaseAction.c:58
HasLocalEffectOnlyScript() => true                                  // opening a screen changes nothing
CanBePerformedScript: at member cap -> SetCannotPerformReason("#OVT-HC_AtCap")
PerformAction: ShowContext(OVT_HighCommandPurchaseContext)
```

Gate results cached on the 1 s TTL shape (`LOADOUT_CHECK_INTERVAL_MS = 1000`, `OVT_BuyEquippedRecruitAction.c:32-38`). **Any player**, not just officers.

### 3.11 Persistence

**`OVT_HighCommandManagerSerializer` (version 1)**, registered in the game-mode config `{65ACD95F40F6C669}` `ComponentSerializers` (`Configs/Systems/Persistence/Overthrow.conf:23-51`), beside `OVT_RecruitManagerSerializer` and the rest. Payload: one `array<ref OVT_PersistedHighCommandGroup>` with `groupId`, `owner`, `entryKey`, `stance`, `destination`, `lastPosition`, `memberBodyIds` (`array<string>`), `memberPrefabs` (`array<ResourceName>`).

⚠️ Mandatory rules, all previously measured on this project:

- **`SaveContext.Write(x)` / `LoadContext.Read(y)` key each property by the LOCAL VARIABLE'S NAME.** Serialize and Deserialize locals must be spelled identically or the load silently reads zeros and reports success.
- **A per-instance field loop is impossible** — one `array<ref …>` of a record class, never parallel per-instance writes.
- **Every `Read()` return is checked**; a failed read aborts the payload with an ERROR and leaves live state untouched.
- **Persisted record class names are written into every save as `$type` and are therefore frozen** — which is why `OVT_PersistedHighCommandGroup` is a dedicated class and not the live record, whose wire shape still gets to change.
- **Never `array<bool>`.**

**Load sequence** (server, after `PostGameStart`): one record per call-queue hop → spawn `OVT_Group_HighCommand.et` → for each member, `SCR_PersistenceSystem.RequestSpawn` filtered by the saved UUID with the recruit path's `BODY_SPAWN_TIMEOUT_MS = 15000` (`OVT_RecruitManagerComponent.c:131`, `RequestPersistedRecruitBody` `:1517`), falling back to a fresh spawn of `memberPrefabs[i]` on timeout or failure → join → `SetAffiliatedFactionByKey(config.m_sPlayerFaction)` → re-seat a vehicle group → `ApplyStance`. A group whose bodies all fail still comes back at full strength from prefabs; a group whose **record** is unreadable is dropped with an ERROR naming the id.

**Save-point sweep.** The manager writes every live group's current position into its record before a save, the `SyncRecruitPositions` precedent (`OVT_RecruitManagerComponent.c:1978`), so a reload never resurrects a group at a ten-minute-old position.

**Two version bumps in Phase 11** (garrison retirement), both read-and-discard, never field deletion:

| Serializer | Bump | Change |
|---|---|---|
| `OVT_ResistanceManagerSerializer` | **1 → 2** | `OVT_PersistedCamp.garrison` (`:8`) and `OVT_PersistedFOB.garrison` (`:23`) stay declared and are still **read** at v1 and discarded; at v2 they are not written. `WriteGarrison` (`:188`) and its two call sites (`:116`, `:137`) go. |
| `OVT_OccupyingFactionManagerSerializer` | **2 → 3** | `OVT_PersistedBase.garrison` (`:60`) same treatment; write path `:299-315` goes. The comment at `:264` warning that removing the preceding array would shift this one is exactly why. |

### 3.12 Configuration knobs

| Knob | Where | Default | Replicated? |
|---|---|---|---|
| Member cap per player | `OVT_DifficultySettings.highCommandMemberCap` + `OVT_OverthrowConfigComponent` server-only field | 48 | **No** — shipped inside the HC manager's own `RplSave` payload so the roster can render `n / cap` with **no `CONFIG_STREAM_VERSION` move** |
| Supporters per member | `OVT_DifficultySettings.highCommandSupportersPerMember` + server-only field | 1 | No — server-side only |
| Observer gate | `OVT_VirtualizationManagerComponent.m_bHighCommandGroupsAreObservers` `[Attribute]` + `GetHighCommandGroupsAreObservers()` | true | No |
| Rearm tick interval / range | `OVT_HighCommandManagerComponent` attributes | 60 s / `WAREHOUSE_RANGE` | No |
| Refuel tick interval | `OVT_HighCommandManagerComponent` attribute | 60 s | No |
| Status heartbeat interval | `STATUS_SYNC_INTERVAL_MS` const | 10000 | N/A |
| Patrol radius, arrival radius | `OVT_HighCommandRules` consts | 150 / 25 m | N/A |

The server-only-field pattern is `virtualizationSpawnDistance` / `civilianDensityMultiplier` (`OVT_OverthrowConfigComponent.c:70-88`), both of which are deliberately absent from `RplSave`/`RplLoad`.

### 3.13 File structure

```
Scripts/Game/Data/                                        PURE, Logic-tier, no world
├── OVT_HighCommandRecord.c            NEW   record + OVT_EHighCommandStance
├── OVT_HighCommandRules.c             NEW   cap / stance / id / moving / supporters
├── OVT_HighCommandStatus.c            NEW   flag bits + tag quad selection
└── OVT_ItemSourcingRules.c            NEW   covered-vs-charged (shared with the tent)

Scripts/Game/GameMode/Managers/
├── OVT_HighCommandManagerComponent.c  NEW   records, JIP, deltas, heartbeat, caps, ticks, spawn
├── OVT_HighCommandManifest.c          NEW   composition + required-item cache (server)
└── OVT_RecruitLoadoutPricing.c        TOUCH + per-resource manifest on OVT_RecruitLoadoutPrice

Scripts/Game/Components/
├── OVT_HighCommandGroupComponent.c    NEW   observer, owned waypoints, stance, ids
├── OVT_BarracksComponent.c            NEW   stateless marker
└── Controller/
    ├── OVT_HighCommandRequestComponent.c  NEW  : OVT_ControllerRequestComponent (5 asks, 2 replies)
    └── OVT_RecruitCommandComponent.c      TOUCH quote gains coveredValue; purchase consumes stock

Scripts/Game/Utilities/
└── OVT_WarehouseStockUtils.c          NEW   registered-warehouse collection + take (server)

Scripts/Game/Faction/OVT_Faction.c     TOUCH + OVT_HighCommandGroupEntry, + m_aHighCommandGroups
Scripts/Game/Global/OVT_Global.c       TOUCH + GetHighCommand()
Scripts/Game/Configuration/OVT_DifficultySettings.c        TOUCH + 2 fields
Scripts/Game/GameMode/Managers/OVT_OverthrowConfigComponent.c  TOUCH + 2 server-only fields/accessors
Scripts/Game/GameMode/Virtualization/OVT_VirtualizationManagerComponent.c  TOUCH + 1 attribute + accessor

Scripts/Game/UI/Context/
├── OVT_HighCommandPurchaseContext.c   NEW   barracks purchase screen
├── OVT_HighCommandRosterContext.c     NEW   "Manage Groups"
├── OVT_LoadoutsContext.c              TOUCH covered-by-stock line in purchase mode
├── OVT_RecruitsContext.c              TOUCH inactive section split by group + Convert action
├── OVT_MainMenuContext.c              TOUCH + Manage Groups
├── OVT_BaseMenuContext.c              TOUCH − the whole garrison half (Phase 11)
└── OVT_FOBMenuContext.c               TOUCH − the whole garrison half (Phase 11)

Scripts/Game/UI/Map/
├── Visualization/OVT_MapHighCommandLayer.c   NEW
├── OVT_MapLayersUI.c                         TOUCH + a third hand-built row
├── Core/OVT_MapDataKeys.c                    TOUCH − GARRISON_COUNT (Phase 11)
└── LocationTypes/OVT_MapLocation{Base,FOB,Camp}.c  TOUCH − garrison rows (Phase 11)

Scripts/Game/UserActions/OVT_ManageHighCommandAction.c   NEW

Scripts/Game/GameMode/Managers/Factions/OVT_ResistanceFactionManager.c  TOUCH (Phase 11)
    − GetGarrisonPrefab :1442, ChargeForGarrison :1455, AddGarrison :1471, AddGarrisonCamp :1508,
      AddGarrisonFOB :1542, SpawnGarrison :1576, SpawnGarrisonCamp :1585, SpawnGarrisonFOB :1597,
      AddPatrolWaypoints :1611, SpawnGarrisons :456 (+ its CallLater :268), ApplyPersistedGarrison :437
      (+ call sites :316/:339), garrison[] / garrisonEntities[] on OVT_CampData/OVT_FOBData
Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c   TOUCH (Phase 11)
    − the friendly-garrison restore in InitBaseControllers :1090-1099; garrison fields on OVT_BaseData
Scripts/Game/Components/Controller/OVT_ResistanceRequestComponent.c     TOUCH − AddGarrison :145,
      RpcAsk_AddGarrison :335, IsGarrisonPrefabIndexValid :529
Scripts/Game/Components/Controller/OVT_FOBRequestComponent.c            TOUCH − AddGarrisonCamp :72,
      AddGarrisonFOB :91, RpcAsk_AddGarrisonCamp :239, RpcAsk_AddGarrisonFOB :274,
      FindGarrisonPrefabIndex :515, IsGarrisonPrefabIndexValid :534
Scripts/Game/GameMode/Objectives/Modules/OVT_AssetStarvedObjectiveAbort.c  TOUCH — one line, :222
Scripts/Game/GameMode/GM/…OVT_EGroupOrigin                              TOUCH − BASE/CAMP/FOB_GARRISON

Scripts/Game/Persistence/Serializers/Components/
├── OVT_HighCommandManagerSerializer.c        NEW  v1
├── OVT_ResistanceManagerSerializer.c         TOUCH v1 → v2 (Phase 11)
└── OVT_OccupyingFactionManagerSerializer.c   TOUCH v2 → v3 (Phase 11)

Prefabs/
├── Groups/INDFOR/OVT_Group_HighCommand.et                        NEW
├── Structures/Military/Barracks/OVT_Barracks.et                  NEW  (the buildable)
├── Structures/Military/Houses/Barracks_01/Barracks_01_military_base.et       NEW same-GUID delta
├── Structures/Military/Houses/Barracks_01/Barracks_USSR_01_military_base.et  NEW same-GUID delta
├── Structures/Military/Houses/Barracks_E_02/Barracks_E_02_base.et            NEW same-GUID delta
├── GameMode/OVT_OverthrowGameMode.et                             TOUCH + the manager
└── GameMode/OVT_OverthrowController.et                           TOUCH + the request component

Configs/
├── Factions/FIA_OverthrowData.conf          TOUCH + m_aHighCommandGroups (11 entries)
├── Resistance/buildables.conf               TOUCH + Barracks entry
├── Map/MapOverthrow.conf                    TOUCH + OVT_MapHighCommandLayer block
├── System/chimeraInputCommon.conf           TOUCH + 1 ActionContext, + 2 Actions
├── Systems/Persistence/Overthrow.conf       TOUCH + OVT_HighCommandManagerSerializer
├── Difficulty/*.conf                        TOUCH only where they differ from the attribute default
└── overthrowBroadcastMessages.conf          TOUCH + HC presets (a tag with no preset is silently dropped — BUG-120)

UI/
├── Layouts/Map/MapHighCommandLocation.layout   NEW  (MapRecruitLocation.layout is the template)
├── Layouts/Menu/HighCommandMenu.layout         NEW
├── Layouts/Menu/HighCommandRoster.layout       NEW
├── Layouts/Menu/MainMenu.layout                TOUCH + "Manage Groups" named button
├── Layouts/Menu/BaseMenu.layout                TOUCH − garrison row (:104) (Phase 11)
└── Layouts/Menu/FOBMenu.layout                 TOUCH − garrison row (:104) (Phase 11)

Language/localization_Overthrow.st              TOUCH ~60 new keys, − #OVT-Garrison (Phase 11)

Scripts/Game/Tests/TestSuites/
├── Logic/OVT_TEST_Logic_HighCommandRules.c     NEW
├── Logic/OVT_TEST_Logic_HighCommandStatus.c    NEW
├── Logic/OVT_TEST_Logic_ItemSourcing.c         NEW
├── Init/OVT_TEST_Init_HighCommandSeam.c        NEW
├── Init/OVT_TEST_Init_ControllerSeam.c         TOUCH +1 line, count text 9 → 10
└── Persistence/OVT_TEST_PersistenceRoundTripSuite.c   + 3 cases
```

**Reserved GUID series: `6B1C3D…`** for prefab / layout / `.conf` instance GUIDs and **`6B0E7A8…`** for the `Configs/Systems/Persistence/Overthrow.conf` entry. Both read 0 hits in the Overthrow tree on 2026-08-22; **the vanilla-tree sweep timed out during planning and MUST be re-run before authoring** (`grep -rl 6B1C3D "/mnt/n/Projects/Arma 4/ArmaReforger"` and the same for `6B0E7A8`, both expected 0). **Inherited component GUIDs are copied, never minted** — a same-GUID delta that re-declares an inherited component must reuse that component's own GUID.

---

## 4. Implementation Phases

Every phase: `tools/compile-check.sh` exit 0 before hand-back. **`tools/run-tests.sh` is the orchestrator's** — run once after a phase completes, never inside an agent, never during planning (`.claude/test-policy.md`). Suites are run **by class name** (`OVT_TEST_LogicSuite`, `OVT_TEST_InitSuite`, `OVT_TEST_PersistenceRoundTripSuite`, `OVT_TEST_CampaignSuite`), never the Fast/All group GUIDs, and one at a time — they are not deterministic when several run back to back under load. Re-baseline (`git pull` / `git status`) before every phase.

**Baselines are not recorded here on purpose.** Concurrent sessions change this tree; the orchestrator takes a fresh baseline immediately before Phase 1 and compares against that.

### Phase 1 — The pure spine, the manager skeleton, the seam, serializer v1
**Estimate:** 6–8 h · **Agent:** `component-developer` · **Suites:** `OVT_TEST_LogicSuite`, `OVT_TEST_InitSuite`

- **T1.1** `OVT_HighCommandRecord.c` (+ `OVT_EHighCommandStance`), `OVT_HighCommandRules.c`, `OVT_HighCommandStatus.c`, `OVT_ItemSourcingRules.c` — §3.2.
- **T1.2** `OVT_HighCommandManagerComponent.c` skeleton: `s_Instance`, `GetInstance()`, the three maps, `GetGroup/GetGroupsByOwner/GetMemberCount/GetMemberCap`, `AddRecord/RemoveRecord` (server), the tuning `[Attribute]`s. **No spawning, no replication, no ticks yet.**
- **T1.3** `OVT_Global.GetHighCommand()` beside `GetRecruits()` (`OVT_Global.c:365`).
- **T1.4** `OVT_HighCommandManagerComponent` onto `Prefabs/GameMode/OVT_OverthrowGameMode.et`.
- **T1.5** `OVT_HighCommandRequestComponent.c` — class + the five ask stubs, each resolving `ResolveOwningPlayerId()` and answering `RpcDo_HCResult`. Registered on `Prefabs/GameMode/OVT_OverthrowController.et` **before** the trailing `RplComponent`, GUID from `6B1C3D…`.
- **T1.6** `OVT_TEST_Init_ControllerSeam.c`: one line in `FindFirstUnresolvedComponent()` (`:103-116`) and the count text 9 → 10 (`:95`).
- **T1.7** `OVT_HighCommandManagerSerializer.c` v1 + the `Overthrow.conf` `ComponentSerializers` entry (GUID from `6B0E7A8…`). Member arrays are written from the start; they are simply empty until Phase 2.
- **T1.8** `OVT_DifficultySettings.highCommandMemberCap` / `highCommandSupportersPerMember` + the two server-only `OVT_OverthrowConfigComponent` fields and accessors.
- **T1.9** `Logic/OVT_TEST_Logic_HighCommandRules.c`, `Logic/OVT_TEST_Logic_HighCommandStatus.c`, `Logic/OVT_TEST_Logic_ItemSourcing.c`.
- **T1.10** `Init/OVT_TEST_Init_HighCommandSeam.c` — the manager resolves through `OVT_Global.GetHighCommand()`; `OVT_ControllerComponent<OVT_HighCommandRequestComponent>.Get()` resolves **and sits on this player's own controller entity** (`OVT_TEST_Init_VehicleRequestSeam.c:30-31` is the template).

**Acceptance**
- **No manager accessor, `OVT_Global` or `GetGameMode` identifier appears anywhere under `TestSuites/Logic/`, not even in a comment** — the tier grep does not distinguish code from prose.
- Every new case proven able to fail once; the mutation and the resulting message recorded in `context.md`.
- `new` sets every field explicitly (`[Attribute]` defvalues do not apply to `new`); no ternaries; **no `maxAttempts`**; polls are preconditions with a named failure on expiry.
- Serialize/Deserialize locals **identically named**; every `Read()` return checked.
- `CONFIG_STREAM_VERSION` is unchanged — grep it and confirm it is still 6.
- Compile-check exit 0.

### Phase 2 — The group entity: spawn, observer, faction, owned waypoints, stances ⚠️ ADVANCED AGENT
**Estimate:** 10–12 h · **Agent:** `component-developer-advanced` · **Suite:** `OVT_TEST_InitSuite`
*Depends on Phase 1.*

- **T2.1** `Prefabs/Groups/INDFOR/OVT_Group_HighCommand.et` + `.et.meta` — §3.5.
- **T2.2** `OVT_HighCommandGroupComponent.c` — ids, deferred observer install, unconditional `OnDelete` removal, owned-waypoint array with remove-then-delete, manager notification on delete.
- **T2.3** `OVT_VirtualizationManagerComponent`: `m_bHighCommandGroupsAreObservers` `[Attribute]` (default true) + `GetHighCommandGroupsAreObservers()`, authored beside `m_bRecruitGroupsAreObservers` (`:62-63`, accessor ~`:3514`). **Do not reuse the recruit gate.**
- **T2.4** Manager `SpawnGroupFromEntry(entryKey, position, ownerPersistentId)` — spawn the group prefab, read the composition via `SCR_AIGroup.GetMembers(IEntitySource, …)`, spawn each member, `AddAIToSlaveGroup`, `SetAffiliatedFactionByKey(config.m_sPlayerFaction)` (the `SetRecruitFaction` body, `OVT_RecruitManagerComponent.c:2294-2313`), stamp the group component, create the record.
- **T2.5** Vehicle groups — spawn the vehicle, seat members one per call-queue hop (`OVT_VehicleSpawningDeploymentModule.c:302-315`, `:702-712`), hold the vehicle id on the group component.
- **T2.6** `ApplyStance(record)` and `ClearOwnedWaypoints()` — the three stance kits of §3.5, terrain-clamped.
- **T2.7** `DismissGroup(groupId)` — delete the group entity (which cascades observer + waypoint cleanup through `OnDelete`) and the vehicle, then drop the record.
- **T2.8** Manager `SyncGroupPositions()` — the save-point sweep.
- **T2.9** Extend `OVT_TEST_Init_HighCommandSeam.c`: spawn a foot group in the test world and assert (a) the group entity exists with the expected member count, (b) every member's faction key equals `m_sPlayerFaction`, (c) `virtualization.HasEntityObserver(groupEntity)` is true one frame later, (d) after `DismissGroup`, `HasEntityObserver` is false and the record is gone.

**Acceptance**
- `SetLifecyclePolicy` appears **nowhere** in this feature (grep). `OVT_EntitySpawningAPI.Cleanup*` appears nowhere (grep).
- `AddEntityObserver` is called only from `InstallObserver`, only after a `CallLater(…, 0, false)` hop, and only behind the **new** gate; `RemoveEntityObserver` in `OnDelete` is **unconditional** and the queued install is cancelled **first**.
- Every waypoint the feature spawns is passed to `AddOwnedWaypoint` — proven by reading every `Spawn*Waypoint` call site.
- Waypoint disposal is remove-then-delete, in that order.
- Observer verification in the test uses **core's** `HasEntityObserver`, never an engine `ObserversSystem` query (right after an add the engine reads a false negative — api.md §3).
- Compile-check exit 0.

### Phase 3 — Purchase, server half: authored data, manifest, pricing, the wire ⚠️ ADVANCED AGENT
**Estimate:** 8–10 h · **Agent:** `component-developer-advanced` · **Suites:** `OVT_TEST_LogicSuite`, `OVT_TEST_InitSuite`
*Depends on Phase 2.*

- **T3.1** `OVT_Faction` gains `OVT_HighCommandGroupEntry` + `m_aHighCommandGroups` and the read accessors — §3.3.
- **T3.2** `Configs/Factions/FIA_OverthrowData.conf` — the 11 entries of §3.3, each nested object with a fresh unique id from `6B1C3D…`. **Verify every group/vehicle GUID against the vanilla file before authoring** (the reference tree ships no `.et.meta`; GUIDs were recovered from inbound references).
- **T3.3** `OVT_WarehouseStockUtils.c` — §3.7.
- **T3.4** `OVT_HighCommandManifest.c` — composition read from the prefab source; the staggered spawn-inspect cache with the deferred capture, retry budget, `UntrackTransient` and WARNING-on-empty; per-entry manifest with unit prices via `IsRegisteredResource` → `GetInventoryId` → `GetBuyPrice`.
- **T3.5** `OVT_HighCommandRequestComponent.RpcAsk_QuoteGroup` / `RpcAsk_PurchaseGroup` — the nine-step gate ladder, the spawn→stock→supporters→money effect order, `RpcDo_HCQuote` / `RpcDo_HCResult`.
- **T3.6** `OVT_BarracksComponent.c` + the server-side barracks sphere query.
- **T3.7** Notification presets for every HC message tag in `Configs/overthrowBroadcastMessages.conf`.
- **T3.8** Logic cases for `SplitCoverage` (exact / short / surplus / zero-price line / multi-line) and for `SupportersRequired` / `FitsUnderCap` boundaries.

**Acceptance**
- **`OVT_RealEstateManagerComponent.c` is unmodified** — `git diff --exit-code`. `m_aWarehouses` (`:39`) and `PlayerMayUseWarehouse` (`:624`) are read as they are.
- `economy.IsRegisteredResource(res)` is checked **before every** `GetInventoryId` call — read every one.
- **The manifest never spawns a group prefab**, only individual character prefabs, and only once per distinct prefab per session — grep the spawn call sites.
- Every scratch character is untracked before deletion, and the scratch position is outside every base.
- **Every refusal answers `RpcDo_HCResult`**; none returns silently — proven by reading every `return` in both asks.
- The funds check is the **last** gate and money is taken **last**; a failed spawn charges nothing.
- Query accumulators are `new`-ed per call, never static members (`OVT_InventoryManagerComponent.c:497` is the shared-accumulator defect this project keeps re-learning).
- Compile-check exit 0.

### Phase 4 — The barracks and the purchase screen
**Estimate:** 7–9 h · **Agent:** `ui-developer` · **Suite:** `OVT_TEST_InitSuite`
*Depends on Phase 3.*

- **T4.1** `Prefabs/Structures/Military/Barracks/OVT_Barracks.et` + `.et.meta` — §3.10(a).
- **T4.2** The three same-GUID vanilla `_base.et` deltas — §3.10(b). Line 1 and `ID` copied byte-for-byte from the vanilla file; `.et.meta` `Name` carries the vanilla GUID.
- **T4.3** `Configs/Resistance/buildables.conf` — the Barracks entry, GUID from `6B1C3D…`.
- **T4.4** `OVT_ManageHighCommandAction.c` + the desk child block on all four hosts.
- **T4.5** `OVT_HighCommandPurchaseContext.c` + `UI/Layouts/Menu/HighCommandMenu.layout` — list of purchasable entries (name, member count, icon), a details panel showing the server quote (total, "covered by nearby stock", supporters required), Buy / Close. The quote is requested on selection and the Buy button stays disabled until it arrives or is refused.
- **T4.6** `.st` keys for everything this phase adds.

**Acceptance**
- The action is gated by `OVT_StructureDamage.IsUsable` **and** the friendly-base test, cached on a 1 s TTL, and `HasLocalEffectOnlyScript()` returns true.
- **Listen-host guard**: the context is created once per *local* player — remote characters are discarded by the `m_OnControlledByPlayer` subscriber (BUG-178).
- The purchase screen shows a **server** quote and never computes one; grep for any economy call in the context.
- Nothing is hidden that could be shown disabled with a reason (BUG-102: a row whose only outcome is a no-op click is a bug).
- `OnClose` removes exactly what `OnShow` inserted, including every `CallLater`.
- **Do not write `Configs/Language/*.conf`** — they are Workbench build output.
- Compile-check exit 0.

**⚠️ Workbench-gated within this phase:** the desk `coords` inside each of the three vanilla barracks families, and confirmation that each override resolves (open a leaf variant and see the desk).

### Phase 5 — The tent warehouse discount
**Estimate:** 3–4 h · **Agents:** `component-developer` (T5.1–T5.3) then `ui-developer` (T5.4) · **Suite:** `OVT_TEST_LogicSuite`
*Depends on Phase 3. May run in parallel with Phase 4.*

- **T5.1** `OVT_RecruitLoadoutPrice` gains `m_aManifest`, appended **after** `m_sUnpriceableResource`; `AddResource` (`:196`) fills it. `m_iSubtotal` keeps its present meaning.
- **T5.2** `OVT_RecruitCommandComponent.BuildQuote` (`:1014`) applies `CollectStores` + `SplitCoverage` and feeds the **charged** subtotal to `TotalPrice`.
- **T5.3** `RpcAsk_BuyEquippedRecruit` (`:837`) consumes the stock with `TakeUpTo` **before** `TakePlayerMoney`.
- **T5.4** `RpcDo_RecruitQuote` gains `int coveredValue` (arity 5 → **6**); `OVT_LoadoutsContext.OnRecruitQuote` (`:384`) renders "#OVT-Recruit_QuoteCovered".

**Acceptance**
- The arity change is applied at **both** `Rpc(...)` sites (`:1088`, `:1092`) and the handler (`:1107`), and written into the `context.md` audit table.
- With no warehouse in range the quoted total is **byte-identical** to today's — the standing regression proof.
- Stock is consumed only on a successful purchase, and only after the spawn succeeded.
- Compile-check exit 0.

### Phase 6 — Replication: JIP, deltas, heartbeat ⚠️ ADVANCED AGENT
**Estimate:** 9–11 h · **Agent:** `network-specialist-advanced` · **Suite:** `OVT_TEST_InitSuite`
*Depends on Phase 2. May run in parallel with Phases 4–5.*

- **T6.1** `RplSave` / `RplLoad` on the manager — positional per-group blocks, new fields **appended last**, plus the resolved member cap as the payload's first field so the roster can render `n / cap` with no config-stream move. Header comments state the append rule, the `OVT_RecruitManagerComponent.c:3178/3242` shape.
- **T6.2** The four broadcast deltas (#8–#11) and their send helpers.
- **T6.3** `SweepStatus()` on `STATUS_SYNC_INTERVAL_MS = 10000` — per group, read contact (any AI in the group with a current target), ammo (`ReadRecruitStatus`'s weapon-slot walk + `GetMagazineCountByWeapon` shape, `OVT_RecruitManagerComponent.c:2035`), mounted, and `IsMoving` from position vs destination; broadcast only when the flags **or** the position moved more than a threshold, so a parked group is silent.
- **T6.4** Client-side record mirror: `RplLoad` builds it, deltas maintain it, and nothing on a client ever dereferences a group entity.
- **T6.5** Extend `OVT_TEST_Init_HighCommandSeam.c`: after a spawn, the record's `m_iTotalMembers` and `m_iStatusFlags` are populated and `m_vLastKnownPosition` is non-zero within one heartbeat.

**Acceptance**
- **RPC arity audit table written into `context.md`**, all eleven rows checked against their handlers. No `Rpc()` call wrapped in a helper.
- No `array<…>` on any RPC. No `RplId` through a `ScriptInvoker`.
- Every `RpcDo_*` takes the `ShouldRespondLocally` branch first.
- The heartbeat is its **own** RPC at arity 4 and no RPC in the feature exceeds 6.
- `RplSave`/`RplLoad` are positional and symmetric — read the two side by side.
- A parked group with unchanged flags produces **no** heartbeat traffic.
- Compile-check exit 0.

### Phase 7 — Map command & control + the gamepad context ⚠️ ADVANCED AGENT
**Estimate:** 11–13 h · **Agent:** `ui-developer-advanced` · **Suite:** `OVT_TEST_InitSuite`
*Depends on Phase 6.*

- **T7.1** `OVT_MapHighCommandLayer.c` + `UI/Layouts/Map/MapHighCommandLocation.layout` — §3.9. Root **must** be a `FrameWidgetClass` (`FrameSlot.SetPos` is what positions it), `Alignment 0.5 0.5`, `Clipping False`.
- **T7.2** `Configs/Map/MapOverthrow.conf` registration block.
- **T7.3** `OVT_MapLayersUI` third row: `KEY_HIGHCOMMAND`, `LABEL_HIGHCOMMAND`, `BuildHighCommandRow`, `ApplyHighCommandMarkerPreference`, and the `ApplyOne` branch **before** `ApplyCanvasLayer`.
- **T7.4** Selection + hover magnet on the recruit-layer/`TickHoverMagnet` model, with the panel-suppression rule and the "reassign the claim first" ordering.
- **T7.5** `Configs/System/chimeraInputCommon.conf` — `OverthrowMapCommandContext` + `OverthrowHCOrder` + `OverthrowHCStance`; per-frame activation with the one-frame arm delay.
- **T7.6** The selected-group info panel (owner, stance, destination, distance, status) and the order/stance flow calling `RpcAsk_OrderGroup`.
- **T7.7** `python3 .claude/skills/overthrow-ui-patterns/scripts/check-input-conflicts.py`, plain and `--warnings`.

**Acceptance**
- `SetVisible` is the **only** thing the filter row touches; opacity belongs to `Update()`; selection uses a **third** channel (the ID_D quad).
- The input context is re-activated **every frame it is wanted** and armed one frame late.
- `check-input-conflicts.py` exits 0 at the shipped baseline `0 error(s), 0 warning(s), 3 combo note(s), 0 pre-existing, 1 acknowledged`.
- **No `ALWAYS_TOP` focusable widget** and **no hover target grown through the widget tree** — the trace is clipped to parent bounds and ancestor size overrides squeeze grown containers; use the cursor magnet.
- Every marker widget created in `OnMapOpen` is `RemoveFromHierarchy()`'d in `OnMapClose`.
- The layer reads **records only** — grep the file for `FindEntityByID` / `Replication.FindItem`, expect none.
- Compile-check exit 0.

### Phase 8 — "Manage Groups" roster + main-menu item
**Estimate:** 4–6 h · **Agent:** `ui-developer` · **Suite:** `OVT_TEST_InitSuite`
*Depends on Phase 6. May run in parallel with Phase 7.*

- **T8.1** `OVT_HighCommandRosterContext.c` + `UI/Layouts/Menu/HighCommandRoster.layout` — one flat entry list (the `OVT_RecruitsContext` model, whose header states the flat model exists so gamepad focus order never depends on widget-tree order), a `n / cap` capacity line, per-group status icons, **Show on Map** and **Dismiss** buttons with a confirm dialog on Dismiss.
- **T8.2** `UI/Layouts/Menu/MainMenu.layout` — a "Manage Groups" `ButtonWidgetClass` copied from the "Manage Recruits" block (`:361-…`), `m_sText "#OVT-MainMenu_ManageGroups"`.
- **T8.3** `OVT_MainMenuContext` — one `SCR_ButtonTextComponent.GetButtonText("Manage Groups", m_wRoot)` + `m_OnClicked.Insert(ManageGroups)` pair (`:150-153` is the template) and a `ManageGroups()` that closes the layout and `ShowContext(OVT_HighCommandRosterContext)` (`:250-254`).

**Acceptance**
- Entries are wired **by widget name**, not index — the shipped convention.
- The flat entry model is preserved: selection index maps to an array, not to widget-tree order.
- Dismiss goes through `RpcAsk_DismissGroup`; nothing client-side deletes anything.
- Do not fork the main-menu override mechanism `resistance/vehicle-storage` is planned against (`OVT_MainMenuContextOverrideComponent`) — this is a plain named button.
- Compile-check exit 0.

### Phase 9 — Recruit conversion + the split roster ⚠️ ADVANCED AGENT
**Estimate:** 7–9 h · **Agents:** `component-developer-advanced` (T9.1–T9.3) then `ui-developer` (T9.4–T9.5) · **Suites:** `OVT_TEST_LogicSuite`, `OVT_TEST_InitSuite`
*Depends on Phases 2 and 6.*

- **T9.1** `RpcAsk_ConvertRecruitGroup(string anchorRecruitId)` — server: resolve the caller, resolve the recruit record, confirm ownership, find its `OVT_InactiveRecruitGroupComponent` host group, collect every inactive recruit in that group.
- **T9.2** Conversion, in this order: create the HC record and stamp the existing group entity with `OVT_HighCommandGroupComponent` **or** create a new HC group and move the bodies into it (whichever the group's own component makes cleaner — the deciding constraint is that the inactive component's `OnDelete` must not delete waypoints the HC component now owns); re-point the observer; then for each recruit, **clear `m_sBodyPersistenceId` before dropping the record** and remove it through the existing remove path so the body is not reserved/released out from under the HC group.
- **T9.3** Cap interaction: converting is refused with `CONVERT_AT_CAP` when the incoming member count would exceed the HC cap; converted recruits leave the recruit cap in the same operation.
- **T9.4** `OVT_RecruitsContext` — the inactive section rendered **split by group**, using `OVT_RecruitInactiveGrouping` (`:34`, `DEFAULT_CLUSTER_RADIUS = 50.0`, `SelectClusterCandidates` `:62`) through `BuildSection` (`:229`), preserving the flat selection model and the "empty section hides its header" rule.
- **T9.5** A per-group **Convert to High Command** button with a confirm dialog that names the one-way consequence (progression ends, recruits leave the roster).

**Acceptance**
- **One-way**: no code path anywhere converts an HC group back into recruits (grep).
- The recruit remove path is the **existing** one — `OVT_RecruitManagerComponent.c` gains no second removal implementation.
- `m_sBodyPersistenceId` is cleared **before** the record is dropped; proven by reading the order.
- The observer is not double-installed and not orphaned: exactly one `AddEntityObserver` per surviving group entity after conversion, asserted in an Init case.
- The recruit roster's active section, capacity line and `KC_G` / `gamepad0:left_trigger` toggle are behaviourally unchanged.
- Compile-check exit 0.

### Phase 10 — Rearm and refuel ticks
**Estimate:** 5–6 h · **Agent:** `component-developer` · **Suite:** `OVT_TEST_LogicSuite`
*Depends on Phases 3 and 6.*

- **T10.1** Rearm tick — per group, on `REARM_INTERVAL_MS`: derive the needed magazine resources from the members' weapons (the `OVT_VehicleRearmUtils` division of labour: eligibility is client-answerable, **mutation is server-only**), `CollectStores(groupPos, WAREHOUSE_RANGE, ownerPersistentId, …)`, `TakeUpTo` per resource, deliver into the members' inventories. The `NO_AMMO` status bit is cleared by the same pass, which is what feeds the map badge.
- **T10.2** Refuel tick — for a group with a vehicle, `OVT_FuelUtils.FindFuelSourcesCovering(pos, …)` (`:144`) then `FindBestFillSource` (`:253`), `GetFuelCostPerLitre` (`:123`, 0 for a free source), `OVT_FuelPricing.ComputeFillPlan` (`:113`), deliver, drain the source, and charge the **owner** with `TakePlayerMoneyPersistentId`. Sub-dollar accrual across ticks goes through `OVT_FuelChargeLedger.Accrue` (`:42`) keyed by group id.
- **T10.3** Both ticks are server-only, skip an unspawned/dead group, and never run on a client.

**Acceptance**
- The refuel maths is the **same calls** the player-facing fill uses — no second pricing implementation (grep for a hand-rolled litres × price anywhere in this feature).
- A free fuel source (`OVT_FuelSourceComponent.IsFree()`) charges **nothing**.
- Rearm consumes stock only from **registered warehouses** the owner may use.
- Neither tick mutates anything on a client — `Replication.IsServer()` guard read at the top of both.
- `OVT_FuelChargeLedger` keys are cleared when a group is dismissed or wiped (no unbounded growth).
- Compile-check exit 0.

### Phase 11 — Garrison retirement, serializer bumps, map rows, changelog ⚠️ ADVANCED AGENT
**Estimate:** 6–8 h · **Agent:** `component-developer-advanced` · **Suites:** `OVT_TEST_PersistenceRoundTripSuite`, `OVT_TEST_InitSuite`
*Depends on every phase above — HC must work before the legacy is removed.*

- **T11.1** `OVT_ResistanceFactionManager` — delete `GetGarrisonPrefab` (`:1442`), `ChargeForGarrison` (`:1455`), `AddGarrison` (`:1471`), `AddGarrisonCamp` (`:1508`), `AddGarrisonFOB` (`:1542`), `SpawnGarrison` (`:1576`), `SpawnGarrisonCamp` (`:1585`), `SpawnGarrisonFOB` (`:1597`), `AddPatrolWaypoints` (`:1611`), `SpawnGarrisons` (`:456`) and its `CallLater` (`:268`), `ApplyPersistedGarrison` (`:437`) and its two call sites (`:316`, `:339`), and the `garrison` / `garrisonEntities` fields on `OVT_CampData` (`:17`, `:19`) and `OVT_FOBData` (`:34`, `:36`).
- **T11.2** `OVT_OccupyingFactionManager` — the friendly-garrison restore block in `InitBaseControllers` (`:1090-1099`) and the `garrison` / `garrisonEntities` fields on `OVT_BaseData` (`:36`, `:38`).
- **T11.3** `OVT_ResistanceRequestComponent` — `AddGarrison` (`:145`), `RpcAsk_AddGarrison` (`:335`), `IsGarrisonPrefabIndexValid` (`:529`). `OVT_FOBRequestComponent` — `AddGarrisonCamp` (`:72`), `AddGarrisonFOB` (`:91`), `RpcAsk_AddGarrisonCamp` (`:239`), `RpcAsk_AddGarrisonFOB` (`:274`), `FindGarrisonPrefabIndex` (`:515`), `IsGarrisonPrefabIndexValid` (`:534`).
- **T11.4** `OVT_BaseMenuContext` — the garrison half (`m_GarrisonButton` `:21`, `"GarrisonSpin"` `:25`, `"AddToGarrison"` `:31`, `AddToGarrison()` `:80`, the `Refresh()` group enumeration and the `//To-do` + `300 *` at `:61-62`) and `OVT_GroupUIInfo` (`:1-8`) once nothing references it. Same for `OVT_FOBMenuContext` (`:40-59`, `:46-47`, `:65`). Both layouts lose their garrison rows (`BaseMenu.layout:104`, `FOBMenu.layout:104`).
- **T11.5** Map rows — `OVT_MapDataKeys.GARRISON_COUNT` (`:40`) and its documented "reads 0 on every client" warning (`:38`), `OVT_MapLocationBase.c` (`:56`, `:80-84`), `OVT_MapLocationFOB.c` (`:35`, `:67-69`), `OVT_MapLocationCamp.c` (`:39`, `:64-66`), and the `#OVT-Garrison` key.
- **T11.6** `OVT_EGroupOrigin` — `BASE_GARRISON`, `CAMP_GARRISON`, `FOB_GARRISON`. ⚠️ **Removing an enum member re-ordinals the rest.** Confirm no persisted or replicated field carries these ordinals before deleting; if any does, retire them as reserved values instead of removing them.
- **T11.7** `OVT_ResistanceManagerSerializer` **1 → 2** and `OVT_OccupyingFactionManagerSerializer` **2 → 3** — read-and-discard at the old version, not written at the new one. §3.11.
- **T11.8** `docs/CHANGELOG.md` (or the project's changelog of record) — the explicit warning that **persisted garrisons are dropped on load** and are replaced by High Command groups.

**Acceptance**
- `grep -rin "garrison" Scripts/ UI/ Configs/` returns only the serializers' read-and-discard branches and the changelog.
- **Neither serializer deletes a field**; both bump the version and keep reading the old shape. Read both `Deserialize` bodies side by side.
- A save written **before** this phase loads without an error and without a garrison; a save written after loads identically. Both proven by a Persistence case, not by assertion.
- Nothing that formerly called a garrison method is left calling a stub.
- Compile-check exit 0.

### Phase 12 — Objectives predicate, QRF verification, persistence round trip, cross-phase review
**Estimate:** 6–8 h · **Agent:** `component-developer` (+ main thread for the review) · **Suite:** `OVT_TEST_PersistenceRoundTripSuite`

- **T12.1** **The one objectives touch point.** `Scripts/Game/GameMode/Objectives/Modules/OVT_AssetStarvedObjectiveAbort.c:222` — `IsPlayerAtAsset()` currently returns `OVT_WorldUtils.PlayerInRange(asset.position, difficulty.baseCloseRange)`. Swap it to `OVT_ResistancePresence.IsGroundHeld(asset.position, difficulty.baseCloseRange)` (`Scripts/Game/GameMode/Deployments/OVT_ResistancePresence.c:97`), which counts any living player-faction character. Update the players-only comment at `:210-213`. The predicate `OVT_ObjectivePhaseRules.IsFOBStarved(bool, int, bool)` (`:361`) is **unchanged**, and its Logic case at `OVT_TEST_Logic_ObjectiveScaling.c:1160` already describes the term as "the resistance camped on it", so it stays green. This is the migration the author already ruled for (`OVT_BaseBehaviorDeploymentModule.c:227-228`) and which this call site was missed by.
- **T12.2** **QRF verification, no QRF code.** An Init case asserts that a spawned HC member's `GetFactionKey()` equals `m_Config.m_sPlayerFaction` and that it is alive and conscious — the two conditions `CheckUpdatePoints` (`:501-517`) tests. `OVT_QRFControllerComponent.c` is **not modified**.
- **T12.3** Three cases appended to `OVT_TEST_PersistenceRoundTripSuite.c`, all sorted **after** `…_Capability_…`, dirtying state through the public facade before reloading, and asserting **only** through public manager APIs (no persistence-framework or save-data type name may appear anywhere in that tree outside the three annotated triggers on `OVT_TEST_PersistenceRoundTripGate`):
  - `…_HighCommandGroup_RoundTrips` — a purchased group survives a session reload with its owner, entry key, stance, destination and member count.
  - `…_HighCommandOrder_RoundTrips` — a group ordered to a new destination with a non-default stance reloads with **that** destination and stance, not the purchase-time one.
  - `…_HighCommandMemberBodies_RoundTrip` — a group whose members were altered (one removed) reloads with the altered count, and every member is alive and in the player faction.
- **T12.4** **Cross-phase review** on the main thread: re-read every phase's acceptance criteria against the built code, run the static gates of §6, and file anything that turns up. This is the step that has caught the one real defect in each of the last three features.

**Acceptance**
- The objectives change is **one expression**; `OVT_ObjectivePhaseRules.c` is unmodified (`git diff --exit-code`).
- `OVT_QRFControllerComponent.c` is unmodified (`git diff --exit-code`).
- Every new persistence case asserts through the public API only.
- Compile-check exit 0.

### Phase 13 — Localization, input check, help / Field Manual / wiki
**Estimate:** 3–4 h · **Agents:** main thread + `help-docs-sync` · **Suite:** none (announce the skip)

- **T13.1** `.st` audit: every runtime key exists in `Language/localization_Overthrow.st` with a filled `Comment`; **count braces before and after** (an unbalanced `.st` means the next Workbench save eats entries); fresh GUIDs; Id order; multi-line values use the trailing backslash. **Ask the user to re-export** — keys render raw until then. **Never write `Configs/Language/*.conf`.**
- **T13.2** `check-input-conflicts.py` plain and `--warnings`, back at baseline.
- **T13.3** `help-docs-sync`: this feature changes player-facing behaviour substantially — a new buildable, a new desk action, a new purchase screen, a new map layer with a new input scheme, a new main-menu roster, a converted recruit path, **and the removal of garrisons**. Tutorials (`Configs/Tutorials/`), the Field Manual (`Configs/FieldManual/`) and the wiki's resistance / base / map pages. Every help claim must cite a file:line before it ships — two tips have shipped invented mechanics before and no gate can catch a well-formed lie.

---

## 5. Key Technical Decisions

**D1 — HC groups are always-live `SCR_AIGroup`s and are never virtualization-registered.** There is no virtual-vs-virtual combat resolution, so a dormant HC group could never actually fight the occupying faction's virtual patrols — it would be a token on a map that loses every engagement it is not present for. Always-live means every engagement is a real engagement, wherever it happens and whoever is online. *Accepted cost:* real-time travel (a foot group walks at AI pace; vehicle groups are the fast option) and a permanent AI budget per group, which is exactly what the member cap is for. *Rejected:* registering with the virtualization core — cheaper on the server, and it would make "garrison this objective" a lie.

**D2 — Each group carries a virtualization AI observer.** Without one, an HC group parked in a town would stand next to dormant enemy patrols that never materialise, and the town would be quiet until a player walked in. `AddEntityObserver` (`:3372`) follows the group entity, so registered groups inside its ring wake up with nobody watching. The install is deferred one call-queue hop because an entity mid-spawn answers `GetID()` with `EntityID.INVALID`, which core refuses and which would silently collide two entities on one map key (api.md §3, measured). Removal in `OnDelete` is **unconditional** — an observer left behind is the one piece of state outside the world's entity graph, with nobody able to name the key that would remove it.

**D3 — HC gets its OWN observer gate attribute, not `m_bRecruitGroupsAreObservers`.** Core deliberately does not consult any consumer's knob inside `AddEntityObserver`, precisely so one consumer's off-switch cannot silently disable another's (`OVT_VirtualizationManagerComponent.c:3510-3519`). An operator who turns parked recruits inert has said nothing about High Command, and an operator with an AI-budget problem needs to be able to switch off the more expensive of the two. *Rejected:* reusing the recruit gate — one line cheaper and it conflates two unrelated budget decisions.

**D4 — Composition is read from the prefab source; required items are captured by spawning each distinct character prefab once at load.** Two different reads because the engine makes one easy and one hard. `SCR_AIGroup.GetMembers(IEntitySource, …)` is `static` and does `entitySource.Get("m_aUnitPrefabSlots", …)` (`SCR_AIGroup.c:20`, `:48`) — the member list costs nothing and spawns nothing, which retires the garrison UI's spawn-a-group-per-open (`OVT_FOBMenuContext.c:40-59`). Gear is the opposite: on a vanilla FIA character it is authored across **five** mechanisms spread over a three-deep inheritance chain (verified on `Character_FIA_Rifleman.et` / `Character_FIA_BaseLoadout.et`), and reading it from `IEntitySource` means hand-walking all five plus the ancestry the engine resolves for free. One spawn resolves everything. The cost is bounded — ~10–15 distinct prefabs, once per session, one per call-queue hop, deleted immediately, untracked — and the cache is idempotent with a lazy rebuild so an early quote is correct rather than wrong. *Rejected:* reading prefab resource data (brittle across engine updates, untestable, five walkers); authoring the item list in config (drifts silently the moment a prefab changes, and the requirement asks for inspection).

**D5 — A "vehicle group" is a crew group prefab plus a vehicle prefab, composed by HC.** There is **no vehicle group anywhere in the vanilla tree** — every `Prefabs/Groups/**` file spawns characters only, and `SCR_AIGroup.m_aStaticVehicles` (`:87-88`) is an `array<string>` of world-editor entity names, useless at runtime. So the authored entry carries an optional `m_sVehiclePrefab` and HC seats the crew with `SCR_CompartmentAccessComponent.MoveInVehicle`, the `OVT_VehicleSpawningDeploymentModule` shape (`:302-315`, `:702-712`), one member per call-queue hop because `MoveInVehicle` locks the slot it hands out for a frame (`:652`). *Rejected:* authoring 13 vehicle group prefabs (there is no vanilla mechanism to inherit); the `OVT_InsertionSpawningDeploymentModule` convoy machine (1600+ lines built for a *virtualized* crew driving to a landing zone — it solves a problem HC does not have).

**D6 — Items are sourced from REGISTERED warehouses only, in ONE shared radius, and every coverage figure is computed server-side.** "Registered" means a building with an `OVT_WarehouseData` record (`OVT_RealEstateManagerComponent.c:17`, `m_aWarehouses` `:39`), filtered by the shipped `PlayerMayUseWarehouse` privacy/rental rule (`:624`) — not any `OVT_StorageComponent` holder, because a player's own backpack, a truck and a loot crate are not a supply chain. `OVT_HighCommandRules.WAREHOUSE_RANGE` is consulted by the purchase quote, the tent discount, the rearm tick and every client pre-check — the BUG-070 lesson that a radius with two definitions has two behaviours. And because storage contents are **pulled on open, not replicated** (`logistics/storage` D-series), a client cannot compute coverage at all: the server quotes, exactly as it does for `recruitLoadoutFeeMultiplier`, which is why **`CONFIG_STREAM_VERSION` stays where it is (6)**.

**D7 — The barracks is identified by a marker component and a server-side sphere query, not by an `RplId` on the wire.** The vanilla barracks overrides are deltas of building prefabs that may or may not carry an `RplComponent`, and a request that marshals a handle the client chose is the client-trust class this epic already has too much of. `OVT_BarracksComponent` costs one stateless component; the ask becomes `RpcAsk_PurchaseGroup(int entryIndex)` at arity 1; and the same query answers "is there a barracks here" for both hosts identically. *Rejected:* `RpcAsk_PurchaseGroup(RplId barracks, int entryIndex)` — it would need an `RplComponent` on three vanilla building families and it would put a client-chosen handle on the wire.

**D8 — Member persistence stores BOTH the body UUID and the character prefab.** The requirement is that a converted recruit's gear carries over, which only a real persisted body delivers, so the primary path is the recruit one: `SCR_PersistenceSystem.RequestSpawn` filtered by UUID with the shipped 15 s timeout (`OVT_RecruitManagerComponent.c:1517`, `:131`). But a body that does not come back would silently shrink a group, and a group is a combat asset the player paid for — so the record also carries each member's character prefab and falls back to a fresh spawn. One extra string per member removes the whole "the save lost a body, the group is now three men" failure class. *Rejected:* prefabs only (loses converted recruits' gear — the point of conversion); bodies only (a lost body is a permanently weaker group with no recourse).

**D9 — HC members do NOT get `OVT_PlayerWantedComponent`.** Reading `SCR_ChimeraAIAgent.IsPerceivedEnemy` (`:7-69`) settles it: the component's shield at `:57-65` tests whether the **target** carries it, so a disguised player is already protected from HC fire by having it themselves; and the "unarmed recruit appears civilian" branch (`:33-51`) requires the *observer* to be unarmed and unwanted, which an armed FIA combatant never is. Adding it would therefore buy nothing and cost three things: every HC member joining the wanted system's 1 Hz per-character detection scan (48 members × N players against an already-flagged O(N·M) scan, BUG-077), an Overthrow delta of every vanilla FIA character prefab, and the possibility of an HC member becoming a hard-coded pacifist (the BUG-146 shape). **The stated consequence:** HC members are always openly hostile to the occupying faction and never benefit from the undercover mechanic. That is what "always-live means every engagement is a real engagement" implies, and the play-test checklist tests it.

**D10 — Defend is a real `SCR_DefendWaypoint`, not the silent hold cycle.** BUG-170 replaced the defend waypoint's continuous re-manage loop (cover picks, stance changes, order barks) with a `[move → wait]` cycle **for parked inactive recruits**, whose entire purpose is to be invisible. An HC Defend order is the opposite: a combat posture at a position the player chose, and the tactical management is the thing being bought. *Accepted cost:* radio chatter and cover churn near a defended position. *Recorded fallback, one line in `ApplyStance`:* the `[move → wait]` cycle with `SetRerunCounter(-1)`, which still engages perceived threats (`OVT_TownController.c:175`). Play-test step 14 is the decision point.

**D11 — Replication is record-based with a separate heartbeat RPC.** Whole-table JIP through `RplSave`/`RplLoad`, reliable broadcast deltas, and a 10 s status sweep — the `OVT_RecruitManagerComponent` model (`:3178`, `:3242`, `:3419-3518`, `STATUS_SYNC_INTERVAL_MS` `:139`) with one correction applied from the start: the heartbeat is **its own** four-parameter RPC, because the recruit equivalent it is modelled on is already at the eight-argument ceiling (`:3477`) and had nowhere to grow, and `Rpc()`'s untyped variadic prototype means a wrong count compiles clean and dies on the wire (BUG-090). Clients never read live entities, which is what makes the map layer correct for a group nobody is near.

**D12 — The cap counts MEMBERS, not groups, and rides the manager's own JIP payload.** Members are the server's AI budget; groups are a UI convenience, and a cap on groups would be gamed by buying rifle squads. Default 48. The value is a difficulty field with a server-only config override (the `virtualizationSpawnDistance` / `civilianDensityMultiplier` pattern, `OVT_OverthrowConfigComponent.c:70-88`, both deliberately absent from `RplSave`/`RplLoad`), and the **resolved** number is shipped as the first field of the HC manager's own JIP payload so the roster can render `n / cap` — **no `CONFIG_STREAM_VERSION` move**.

**D13 — Garrison retirement is a version bump with a read-and-discard branch, never a field deletion, and it is Phase 11.** The persistence contexts are positional; the OF serializer warns in place that removing the array preceding `garrison` would shift it (`OVT_OccupyingFactionManagerSerializer.c:264`). So `OVT_ResistanceManagerSerializer` goes 1 → 2 and `OVT_OccupyingFactionManagerSerializer` 2 → 3, both still reading the old shape and discarding it. Persisted garrisons are **dropped on load** with a changelog warning, per the requirement — no migration, because a garrison and an HC group have different owners, different costs and different lifetimes, and inventing an owner for a server-spawned garrison would be a guess. And it is late in the order because a half-built HC plus a removed garrison system is a campaign with no way to defend anything.

**D14 — The Barracks is an ordinary buildable: money-only, counted against the base's shared item pool.** `OVT_ItemLimitChecker` keys purely on `(locationId, EOVTBaseType)` and counts every `OVT_BuildableComponent`/`OVT_PlaceableComponent` at that location — there is no per-type cap anywhere in the mod (`:279-304`), and a base shares the FOB limit (default 100, `OVT_OverthrowConfigComponent.c:122-124`). Inventing a per-type cap for one buildable would be a new mechanism for one caller. Resource requirements are omitted in v1 (`m_aResourceRequirements` empty = byte-identical to today's behaviour) and adding them is a `.conf` edit if play-test says a barracks should cost timber. *Rejected:* a bespoke "one barracks per base" rule — YAGNI, and the three vanilla overrides mean most bases already have one for free.

**D15 — The vanilla barracks overrides target the `_base.et` prefabs, not the leaves.** The extracted reference tree ships no `.et.meta` files, so leaf GUIDs (`Barracks_USSR_01_military.et` and friends) are unrecoverable — but `_base` GUIDs are readable off their children's line 1, and world-placed leaves inherit them. It is also the better granularity: one file reaches every colour variant. It is the same choice Overthrow already made for `Warehouse_01_Base.et`, `ShopHouse_E_2I01t_Base.et` and the transmitter towers. The failure mode of a wrong GUID is an orphan prefab that nothing instances — visible, not silent — and the Workbench check in Phase 4 is what catches it.

**D16 — The map layer is a `SCR_MapUIBaseComponent` keyed on group id, and selection is a THIRD widget channel.** It must be a UI component in `m_aUIComponents`, not an `OVT_MapCanvasLayer` in `m_aModules`, because HC needs per-group widgets at hit-testable positions. Keying on the **record id** rather than the entity is what makes a client correct for a group it cannot see. And because `Update()` already owns opacity (own vs other-owner dimming) and the filter row already owns visibility, selection gets its own property — the ID_D quad swap (`Friend_Land_Bcg` / `Friend_Focus_Land` / `Friend_Select_Land`) — rather than a fourth party fighting over one of the first two. That is the discipline `OVT_MapRecruitLocation.c:363-370` establishes after learning it the hard way.

**D17 — Ordering gets a dedicated per-frame `ActionContext` at Priority 70 / `Flags 0x26 0`.** The map menu preset carries exactly one context — vanilla's `MapContext`, Priority 50, which declares no `Menu*` navigation action at all — so a pad has nothing bound to move focus and nothing to press (`OVT_MapLayersUI.c:7-12`). The shipped answer is `OverthrowMapLayersContext`, re-activated every frame with a one-frame arm delay so the button that opened the panel is not re-offered to the newly focused control. HC copies it exactly, including the arm delay, because "select a group with `a`, then press the order button" hits the identical double-fire. `gamepad0:shoulder_left` is VON at priority 110 and is banned.

**D18 — The tent quote is extended, not forked.** `OVT_RecruitLoadoutPricing.AddResource` already visits every resource exactly once; it gains a manifest field and nothing else changes meaning. `OVT_RecruitPurchaseRules` — the pure fee arithmetic, whose header says its numbers are money — is **not modified at all**; the warehouse split simply feeds it a smaller subtotal. With no warehouse in range the quoted total is byte-identical to today's, which is the standing regression proof.

**D19 — The quote is advisory; the server re-derives and charges at purchase time.** Warehouse stock can move between "the player read the price" and "the player pressed Buy", and a quote held on the server would be state with an expiry nobody wants to own. The re-derive-and-charge shape is exactly what `RpcAsk_BuyEquippedRecruit` (`:837`) already does. The screen labels its number as an estimate and the funds check runs against the re-derived total.

**D20 — The objectives touch point is one expression, and the seam already exists.** `OVT_AssetStarvedObjectiveAbort.IsPlayerAtAsset()` (`:216-223`) is the last players-only presence test outside the two the author deliberately kept (`OVT_BaseBehaviorDeploymentModule.c:236-239`); `OVT_ResistancePresence.IsGroundHeld` (`:97`) is the shipped resistance-presence primitive with five existing callers, and it counts **any** living player-faction character — which HC members are by construction. The pure predicate `IsFOBStarved(bool, int, bool)` (`:361`) does not change, and its Logic case already describes the term as "the resistance camped on it", so it stays green.

---

## 6. Definition of Done

### Functional

- **F1** At a friendly base, a barracks (built or vanilla) offers a desk action to **any** player; at an enemy-held base it does not appear; on a ruined one it does not appear.
- **F2** The purchase screen lists the authored FIA groups with member counts and shows a **server** quote: total, "covered by nearby warehouse stock", supporters required.
- **F3** Buying spawns the group at the barracks with the right members, all in the player faction, all armed; a vehicle group arrives with its vehicle and its crew seated.
- **F4** Purchase order is spawn → consume stock → take supporters → take money. A failed spawn charges nothing. Every refusal shows a reason.
- **F5** Warehouse stock in range is actually consumed and actually reduces the price; with no warehouse in range the price is the full item cost × the difficulty fee.
- **F6** The member cap is enforced server-side; the roster shows `n / cap`; a purchase that would exceed it is refused with a reason before the player pays.
- **F7** Every player's map shows every HC group as a green NATO symbol — own groups bright, others at 0.45 — with a red `!` on contact and an ammo badge when out of ammo, and a filter row in the layers panel that hides them.
- **F8** Selecting a group shows owner, stance, destination and status. Ordering a group you own moves it; ordering one you do not is refused.
- **F9** The three stances behave distinctly: Defend hunkers at the destination, Patrol circles it, Attack sweeps it.
- **F10** Travel is real time and visible: the marker moves, and `MOVING` clears on arrival.
- **F11** A group continues to its destination and holds its stance while its owner is **offline**, indefinitely.
- **F12** The recruit roster's inactive section is displayed **split by group** and each group has a Convert to High Command action; conversion is one-way, removes those recruits from the roster and cap, and the converted group keeps their bodies and gear.
- **F13** "Manage Groups" in the main menu lists the player's groups with status and allows Dismiss (with confirmation); Dismiss removes the group, its vehicle, its waypoints and its observer.
- **F14** A group standing near a registered warehouse with the right magazines rearms on the tick and the ammo badge clears.
- **F15** A vehicle group near a fuel source refuels on the tick and the **owner** is charged the same amount a player fill would cost; a free source costs nothing.
- **F16** HC members hold a QRF battle zone (the friendly point count rises with HC members present and no player nearby).
- **F17** An enemy FOB whose source base is held by an HC group is **not** starved (the objectives predicate).
- **F18** Groups, orders, stances, destinations and members survive save/continue; a reloaded group is at the position it was at when the game saved.
- **F19** A joining client sees every group's marker, position, stance, destination and status without opening anything.
- **F20** Every garrison purchase path, spawn helper, map row and UI copy is gone; a save containing garrisons loads cleanly without them; a new save round-trips at the new serializer versions.

### Quality

- **Q1** `tools/compile-check.sh` exit 0 at every phase boundary.
- **Q2** Logic cases for the cap, stance validation, id minting, status derivation, arrival and the coverage split — each proven able to fail once, the mutations recorded in `context.md`. **No `maxAttempts` anywhere.**
- **Q3** Persistence round-trip cases for a group, an order and altered members; all sorted after `…_Capability_…`; all asserting through public manager APIs only.
- **Q4** An RPC arity audit table in `context.md` covering all eleven RPCs against their handlers.
- **Q5** No `array<…>` on any RPC; no `Rpc()` call wrapped in a helper; no `RplId` through a `ScriptInvoker`; no identity argument on any ask.
- **Q6** No `SetLifecyclePolicy` and no `OVT_EntitySpawningAPI.Cleanup*` anywhere in the feature.
- **Q7** Every waypoint the feature spawns is owned by `OVT_HighCommandGroupComponent` and is removed-then-deleted.
- **Q8** No `Configs/Language/*.conf` modified; `.st` braces balanced with counts recorded before and after.
- **Q9** Comments sparse per `CLAUDE.md` — a line or two for a trap or a load-bearing ordering, never a rationale essay. Reasoning belongs in this document.
- **Q10** Every new `.et` and `.conf` has a `.meta`; every fresh GUID comes from `6B1C3D…` / `6B0E7A8…` and was re-verified against **both** trees; every inherited component GUID is copied, not minted.
- **Q11** `CONFIG_STREAM_VERSION` is unchanged.

### Integration

- **I1** `Scripts/Game/GameMode/Managers/OVT_RealEstateManagerComponent.c` is **unmodified**.
- **I2** `Scripts/Game/Controllers/OccupyingFaction/OVT_QRFControllerComponent.c` is **unmodified**.
- **I3** `Scripts/Game/GameMode/Objectives/OVT_ObjectivePhaseRules.c` is **unmodified**; the objectives change is one expression in `OVT_AssetStarvedObjectiveAbort.c`.
- **I4** `Scripts/Game/Data/OVT_RecruitPurchaseRules.c` is **unmodified**.
- **I5** No `logistics/storage` file is modified — `OVT_StorageComponent`, `OVT_StorageLedger`, `OVT_StorageUtils`, `OVT_StorageRequestComponent`, `OVT_StorageContext` are consumed as they are.
- **I6** No `economy/fuel` file is modified — `OVT_FuelUtils`, `OVT_FuelPricing`, `OVT_FuelChargeLedger` are called, never edited.
- **I7** `OVT_VirtualizationManagerComponent` gains exactly one attribute and one accessor; `AddEntityObserver` / `RemoveEntityObserver` are unchanged.
- **I8** `OVT_RecruitManagerComponent` gains no second removal path; the conversion seam calls the existing one.
- **I9** The main-menu override mechanism (`OVT_MainMenuContextOverrideComponent`) is untouched — `resistance/vehicle-storage` is planned against it.

### Verification method — an independent evaluator can follow this

**Static (no game):**

1. `cd "/mnt/n/Projects/Arma 4/Overthrow.Arma4" && tools/compile-check.sh` → exit 0.
2. `git diff --exit-code -- Scripts/Game/GameMode/Managers/OVT_RealEstateManagerComponent.c Scripts/Game/Controllers/OccupyingFaction/OVT_QRFControllerComponent.c Scripts/Game/GameMode/Objectives/OVT_ObjectivePhaseRules.c Scripts/Game/Data/OVT_RecruitPurchaseRules.c` → clean (I1–I4).
3. `git diff --exit-code -- Scripts/Game/Components/OVT_StorageComponent.c Scripts/Game/Data/OVT_StorageLedger.c Scripts/Game/Utilities/OVT_StorageUtils.c Scripts/Game/Utilities/OVT_FuelUtils.c Scripts/Game/Data/OVT_FuelPricing.c` → clean (I5, I6).
4. `git status --porcelain Configs/Language/` → empty (Q8).
5. `grep -rn "SetLifecyclePolicy\|OVT_EntitySpawningAPI.Cleanup" Scripts/Game --include=*.c | grep -i highcommand` → no hits (Q6).
6. `grep -rn "OVT_Global\.\|GetGame().GetGameMode()" Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_HighCommand*.c Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_ItemSourcing.c` → no hits (Logic-tier purity).
7. `grep -rn "maxAttempts" Scripts/Game/Tests/TestSuites/` → no new hits.
8. `grep -n "CONFIG_STREAM_VERSION" Scripts/Game/GameMode/Managers/OVT_OverthrowConfigComponent.c` → still `6` (Q11).
9. `grep -c "Rpc(" Scripts/Game/Components/Controller/OVT_HighCommandRequestComponent.c` plus the manager's broadcasts matches the eleven-row audit table in `context.md`, and each row's arity matches its handler (Q4).
10. `grep -rin "garrison" Scripts/ UI/ Configs/` → only the two serializers' read-and-discard branches (F20).
11. `grep -rn "version" Scripts/Game/Persistence/Serializers/Components/OVT_ResistanceManagerSerializer.c Scripts/Game/Persistence/Serializers/Components/OVT_OccupyingFactionManagerSerializer.c` → 2 and 3 respectively, both with a `version < N` read branch.
12. `python3 .claude/skills/overthrow-ui-patterns/scripts/check-input-conflicts.py` → exit 0 at `0 error(s), 0 warning(s), 3 combo note(s), 0 pre-existing, 1 acknowledged.`
13. `grep -rl 6B1C3D . ; grep -rl 6B1C3D "/mnt/n/Projects/Arma 4/ArmaReforger"` → only this feature's own files; same for `6B0E7A8`.
14. **Orchestrator only, after each phase:** `tools/run-tests.sh OVT_TEST_LogicSuite` / `OVT_TEST_InitSuite` / `OVT_TEST_PersistenceRoundTripSuite` / `OVT_TEST_CampaignSuite` **by class name**, one at a time. Announce the focus steal first.

**Workbench (user-gated):** open each of the three vanilla barracks `_base.et` overrides **and one leaf variant of each** and confirm the desk prop and the action are present and inherited; tune the desk `coords` inside each; open `OVT_Barracks.et`, `OVT_Group_HighCommand.et`, `OVT_OverthrowController.et`, `OVT_OverthrowGameMode.et` and `FIA_OverthrowData.conf` without dropped-attribute warnings.

**Play-test A — single player, mouse:**

15. Start a campaign, capture a base with a barracks. The desk action appears. At an enemy base it does not.
16. Buy a **rifle squad**. Money falls by the quoted amount, the nearest town's supporters fall by the member count, and seven armed FIA fighters appear at the barracks. Check the town's supporter count before and after.
17. Build a **Warehouse** at the base (already shipped) and stock it with rifles and magazines. Re-open the barracks screen: the quote now shows "covered by nearby stock" and the total is lower. Buy; the warehouse stock falls by exactly the covered units.
18. Open the map. The squad is a green infantry symbol. Select it → owner, stance, destination, status. Order it to a point 1 km away with **Patrol**. Watch the marker move in real time; it arrives and starts circling.
19. Re-order the same group to **Defend** at a captured base's flag. It hunkers. **D10 decision point:** if the radio chatter and cover churn are intolerable, flip `ApplyStance` to the silent hold cycle.
20. Order it **Attack** at an occupied village. It sweeps and engages.
21. Buy a **technical**. It arrives with a UAZ and a crewed gun. Order it 3 km away — it drives.
22. Park the technical near a fuel station with fuel in the tank half empty. On the next refuel tick, fuel rises and **your money falls** by the same amount a manual fill would cost. Park it at a built fuel depot → free.
23. Spend a squad's ammo in a firefight. The map shows the ammo badge. Walk the squad into range of a warehouse holding the right magazines → the badge clears and the men have ammo.
24. **Recruit conversion:** park four recruits as an inactive group, open Manage Recruits → the inactive section is split by group. Convert → the four leave the roster, the recruit capacity line drops by four, and a new HC group appears on the map with those bodies **and their loadouts**.
25. Main menu → **Manage Groups** → all groups listed with status and `n / cap`. Dismiss one → it disappears from the world and the map, its vehicle with it, no waypoints left standing.
26. Fill the member cap. The next purchase is refused with a reason **before** any money moves.
27. Station a squad at a base and trigger a QRF there. The friendly point count rises with no player nearby and the resistance can hold the zone.
28. Let the occupying faction start a FOB whose source base your squad is sitting on. The FOB is **not** starved.
29. Save, quit, Continue. Every group is back at the position it was left, with its stance, destination and members, and the members still have their gear.
30. **Garrison retirement:** the base and FOB menus have no garrison controls; the map info panels have no Garrison row; a save made before this feature loads with no garrisons and no errors.

**Play-test B — gamepad only (no mouse touched):**

31. Open the map with a pad. D-pad/stick moves the cursor; the group symbol highlights when the cursor is near it (the magnet).
32. `a` selects the group; the info panel appears and something is focused.
33. Move the cursor; press the **order** button (`x`). The group takes the order. Press the **stance** button (`y`) — the stance cycles and re-issues.
34. Open the layers panel and toggle the High Command row off and on. Markers vanish and return; nothing else changes.
35. `b` closes the map cleanly. `LB` still opens VON at every point above.
36. Barracks screen: d-pad walks the list, the quote appears on selection, `a` buys, `b` closes.
37. Manage Groups: d-pad walks, Dismiss confirms, focus lands somewhere real afterwards.

**Play-test C — dedicated server + JIP:**

38. Two clients. Client 1 buys a group. **Client 2 sees its marker, dimmed**, and can select it to read stance/destination/status.
39. Client 2 tries to order Client 1's group → refused with a reason; the group does not move.
40. Client 1 orders the group across the map. Client 2 watches the marker travel in real time.
41. **Client 1 disconnects.** The group keeps walking, arrives and holds its stance. Client 2 still sees it. Wait past the recruit 600 s despawn window and confirm the HC group is still there.
42. **JIP:** Client 3 joins after all of the above → sees every group's marker, position, stance, destination, status and the correct `n / cap` in Manage Groups, with no action taken.
43. Client 1 reconnects → the group is still his; he can order it.
44. Two clients buy from the same barracks against the same warehouse simultaneously → both succeed or one is refused with a reason; the warehouse stock afterwards is exactly right.
45. Kill a group's last member on the server → the group and its record vanish on **both** clients and the marker disappears.

### Bug-report candidates for the orchestrator — do not file from this plan

- `OVT_AssetStarvedObjectiveAbort.IsPlayerAtAsset()` measures presence at `asset.position` (the FOB) rather than `asset.sourceBasePosition`, flagged 🔴 at `:14-20` as a deliberate parity point. Pre-existing; T12.1 changes the *source* of the bool, not the position.
- `OVT_ResistanceFactionManager` camp/FOB removal never despawns `garrisonEntities` (BUG-125). Retired rather than fixed by Phase 11.
- `Prefabs/GameMode/OVT_FactionManager.et:2800-2802` carries a legacy hand-authored FIA `m_aGroupPrefabSlots` list and an `m_aInventoryConfigFiles` entry pointing at a `.conf` that does not exist. It appears to be a dead prefab superseded by `OVT_OverthrowFactionManager.et`. Worth confirming and deleting — **not** as part of this feature.
- `OVT_MapDataKeys.c:38` documents that `garrisonCount` reads 0 on every client and has done since it shipped. Removed rather than fixed by Phase 11.

---

## 7. Testing Strategy

**Logic tier** (world-free, `new`-built; **no manager accessor, `OVT_Global` or `GetGameMode` identifier may appear anywhere under `TestSuites/Logic/`, comments included**):

| Class | Case | Claim | Proof it can fail |
|---|---|---|---|
| `OVT_TEST_Logic_HighCommandRules` | `CapAdmitsExactFit` | `current + incoming == cap` fits; one more does not | use `<` instead of `<=` |
| | `CapZeroIsUnlimited` | `cap <= 0` admits anything | treat 0 as a literal cap |
| | `RemainingCapacityNeverNegative` | over-cap state reports 0 remaining, not a negative | drop the floor |
| | `StanceValidationRejectsOutOfRange` | −1 and `ATTACK + 1` refused; all three accepted | compare against the wrong bound |
| | `MovingIsSquaredDistance` | just inside the arrival radius is not moving, just outside is; the boundary is asserted with an epsilon, never an exact equality (**`vector.Distance` is not correctly rounded**) | compare unsquared distances at 1000 m |
| | `SupportersScaleWithMembers` | `members × perMember`; `perMember <= 0` yields 0, never negative | drop the floor |
| | `GroupIdIsStableAndUnique` | the id embeds the owner and differs for two salts | drop the salt |
| | `DestinationLegality` | zero vector and non-finite refused | accept everything |
| `OVT_TEST_Logic_HighCommandStatus` | `DeriveSetsEachBitIndependently` | four inputs, four bits, no cross-talk | OR two bits together |
| | `TagIconPrefersContact` | contact + no ammo shows the contact tag; no ammo alone shows the ammo tag; neither shows `""` | reverse the precedence |
| `OVT_TEST_Logic_ItemSourcing` | `CoverageExactlyMeetsNeed` | stock == need → charged 0, covered = full value | off-by-one on the min |
| | `CoveragePartial` | stock < need → charged = remainder × unit price | charge the whole line |
| | `CoverageSurplus` | stock > need → charged 0 and only `needed` units reported covered | consume the surplus |
| | `CoverageAcrossManyLines` | four lines, mixed coverage, summed correctly in both channels | sum only the first line |
| | `CoverageIgnoresZeroPriceLine` | a line with unit price 0 contributes nothing and is reported | let it contribute a negative |
| | `CoverageNeverNegative` | no combination produces a negative charged subtotal | remove the clamp |

**Init tier — `OVT_TEST_Init_HighCommandSeam.c`:** the manager resolves through `OVT_Global.GetHighCommand()`; `OVT_ControllerComponent<OVT_HighCommandRequestComponent>.Get()` resolves **and sits on this player's own controller entity**; the FIA faction config exposes ≥ 1 purchasable entry with a non-empty key and a resolvable group prefab, and every key is unique; a spawned HC group has the expected member count, every member's faction key equals `m_sPlayerFaction`, and `virtualization.HasEntityObserver(groupEntity)` is true one frame after spawn (asked of **core's** map, never the engine's); after `DismissGroup` the observer is gone and the record is gone; after a conversion there is exactly **one** observer on the surviving group. Polls are preconditions with a named failure on expiry — never retries.

Plus one line in `OVT_TEST_Init_ControllerSeam.FindFirstUnresolvedComponent()` and the count text bumped 9 → 10.

**Persistence tier — appended to `OVT_TEST_PersistenceRoundTripSuite.c`**, sorted after `…_Capability_…`, following the shipped state-machine template and dirtying state through the public facade before reloading. **No persistence-framework, vanilla-persistence or Overthrow save-data type name may appear anywhere in that tree except the three annotated triggers on `OVT_TEST_PersistenceRoundTripGate`.**

| Case | Subject |
|---|---|
| `…_HighCommandGroup_RoundTrips` | owner, entry key, stance, destination, member count |
| `…_HighCommandOrder_RoundTrips` | an ordered group reloads at the ordered destination and stance, not the purchase-time one |
| `…_HighCommandMemberBodies_RoundTrip` | an altered roster reloads altered, every member alive and in the player faction |

Phase 11 additionally needs a case (or a documented manual step, if the suite cannot author an old-format save) proving a **pre-bump save loads cleanly** at the new serializer versions.

**Every new case needs a recorded can-fail proof** — the mutation applied and the resulting failure message, written into `context.md`. **No `maxAttempts` anywhere.**

**What the automated spine cannot reach** — and therefore what the play-test gates exist for:

- **All multiplayer and JIP behaviour** (steps 38–45). The suites run one machine.
- **All map, UI, focus and gamepad behaviour** (steps 31–37), including the order context's arm delay and the hover magnet.
- **Real-time travel** over any distance, on foot or in a vehicle, and its continuation while the owner is offline (steps 18, 21, 41).
- **Combat**: whether Defend actually holds, whether Attack actually sweeps, whether HC members hold a QRF zone (steps 19, 20, 27).
- **The rearm and refuel ticks** end to end (steps 22, 23) — the suites can pin the arithmetic, not the delivery.
- **Every spawn and delete**: the observer's effect on nearby dormant AI, the vehicle seating, the delete-when-empty cascade.
- **Workbench prefab resolution** — a same-GUID delta of a vanilla prefab is only truly proven by opening a child.
- **Balance.** Every price, cap, radius and interval here is a starting value.

---

## 8. Dependencies

**Consumed, unmodified:**

- `virtualization/core` — `AddEntityObserver` / `RemoveEntityObserver` / `HasEntityObserver` (`:3372`, `:3447`, `~:3481`) and the frozen contract in `docs/features/virtualization/core/api.md` §3. HC adds exactly one attribute and one accessor to the manager (I7) and touches no observer code.
- `resistance/recruits` + `recruit-ux` — the record/JIP/delta/heartbeat model, `OVT_InactiveRecruitGroupComponent`'s observer and waypoint-ownership recipe, `OVT_RecruitInactiveGrouping`, the body reservation + `RequestSpawn` path, `OVT_RecruitsContext`'s flat roster model, `OVT_MapRecruitLocation`'s layer. The recruit manager is modified **only** at the conversion seam (I8).
- `resistance/loadouts` — read-only; the tent discount rides `OVT_RecruitLoadoutPricing`, which is a `recruits` file.
- `logistics/storage` — `OVT_StorageComponent`, `OVT_StorageLedger`, `OVT_StorageUtils` (`OVT_StorageHolderQuery`). **Unmodified** (I5).
- `logistics/resources` — the shipped **Warehouse buildable** and `RegisterBuiltWarehouse` (`:1002`). HC neither builds nor plans one.
- `economy` — `OVT_RealEstateManagerComponent` (`m_aWarehouses` `:39`, `PlayerMayUseWarehouse` `:624`, `WAREHOUSE_MATCH_RANGE` `:42`) and `OVT_EconomyManagerComponent` (`IsRegisteredResource`, `GetInventoryId`, `GetBuyPrice`, `PlayerHasMoney`, `TakePlayerMoney`, `TakePlayerMoneyPersistentId`). Read-only (I1).
- `economy/fuel` — `OVT_FuelUtils`, `OVT_FuelPricing`, `OVT_FuelChargeLedger`. Called, never edited (I6).
- `occupying/qrf` — the resistance-AI count fix `7642a252` is **already on v1.5** and counts any resistance-faction agent. No QRF code changes (I2).
- `occupying/objectives` + `deployments` — `OVT_ResistancePresence.IsGroundHeld` (`:97`) is the shipped primitive; `OVT_ObjectivePhaseRules` is untouched (I3).
- `towns` — `NearestTownHasSupporters` (`:1218`) and `TakeSupportersFromNearestTown` (`:1193`).
- `resistance/building` — `OVT_Buildable` / `buildables.conf`, `OVT_BuildableComponent`, `OVT_ItemLimitChecker` (consulted, not changed), the build pipeline.
- `core/damage` — `OVT_StructureDamage.IsUsable`; `OVT_StructureDestructionComponent` **authored** on the new barracks prefab. No `core/damage` file is modified.
- `core/controller-migration` — `OVT_OverthrowController`, `OVT_ControllerRequestComponent`, `OVT_ControllerComponent<T>.Get()`, `OVT_ComponentFinder`.
- `core/persistence` — `ScriptedComponentSerializer`, `OVT_PersistenceTracking`, `OVT_PersistenceReservation`, `Overthrow.conf` rules.
- `map` — `SCR_MapUIBaseComponent`, `OVT_MapFactionPalette`, `OVT_MapLayerPrefsStore`, `MapOverthrow.conf`.
- Vanilla — `SCR_AIGroup` (incl. the static `GetMembers(IEntitySource, …)`), `SCR_DefendWaypoint`, `SCR_EntityWaypoint`, `SCR_CompartmentAccessComponent`, `SCR_PlayerControllerGroupComponent.AddAIToSlaveGroup`, `SCR_ConfigurableDialogUi`, the `MilitarySymbol` imagesets.

**Modified:** `OVT_Faction`, `OVT_Global`, `OVT_DifficultySettings`, `OVT_OverthrowConfigComponent`, `OVT_VirtualizationManagerComponent` (one attribute), `OVT_RecruitLoadoutPricing`, `OVT_RecruitCommandComponent`, `OVT_RecruitsContext`, `OVT_LoadoutsContext`, `OVT_MainMenuContext`, `OVT_MapLayersUI`, `OVT_AssetStarvedObjectiveAbort` (one expression), and — in Phase 11 only — `OVT_ResistanceFactionManager`, `OVT_OccupyingFactionManager`, `OVT_ResistanceRequestComponent`, `OVT_FOBRequestComponent`, `OVT_BaseMenuContext`, `OVT_FOBMenuContext`, the three map location types, `OVT_MapDataKeys` and the two serializers.

**Downstream, planned against this:** none yet. The follow-up **transfer / officer-takeover** feature will need `OVT_HighCommandManagerComponent.SetOwner(groupId, newOwnerPersistentId)` plus one delta RPC; the record already carries the owner as its own field, so no schema change is expected. Nothing in this plan blocks it.

**Concurrent work on this tree:** other sessions commit mid-feature. Re-baseline before every phase; every claim here carries a file:line so drift is detectable. The files most likely to move under HC are `OVT_ResistanceFactionManager.c`, `OVT_MapLayersUI.c`, `Overthrow.conf` and `chimeraInputCommon.conf`.

---

## 9. Risks & Mitigation

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| R1 | **AI budget** — 48 always-live members per player, each group also holding an observer that keeps nearby registered groups materialised, tanks server frame time | **High** | The feature is unplayable on a populated server | The cap is the knob and it is server-configurable (D12); the observer has its own operator off-switch (D3); the observer's cost is stated in `api.md` and in the attribute's own description; play-test steps 16–29 run with several groups standing; if it still bites, the cap default drops and the observer gate ships off |
| R2 | **Spawn-inspect at load hitches or crashes** — spawning characters during `PostGameStart`, or an inventory that is not populated when captured | Medium | A load-time stall, or manifests that are silently empty and therefore prices that are silently wrong | One prefab per call-queue hop, never a loop; capture deferred one further frame with a 5-frame retry; empty manifest logs a WARNING naming the prefab and degrades to `baseRecruitCost` only; every scratch character is untracked and deleted; the cache is lazily rebuilt on a miss so an early quote is correct |
| R3 | **The observer leaks** — a group destroyed by a path HC did not write (vanilla delete-when-empty) leaves an observer following a dead entity | Medium | Every registered group near that spot stays materialised for the rest of the session, with nobody able to name the key that would remove it | Removal lives on the group's **own** `OnDelete`, which covers every destruction route, and is **unconditional**; the queued install is cancelled first; core's stale-entity sweep is the backstop; an Init case asserts the observer is gone after Dismiss |
| R4 | **Waypoints outlive their group** — `AddWaypoint`/`SetWaypoints` do not take ownership and vanilla only destroys what it spawned | Medium | Orphan waypoint entities accumulate across a long campaign | Every spawned waypoint goes to `AddOwnedWaypoint`; `OnDelete` and every re-order remove-then-delete; the acceptance criterion is to read every `Spawn*Waypoint` call site |
| R5 | **The same-GUID barracks deltas do not take** — wrong GUID, wrong parent line, wrong `ID` | Medium | Barracks are inert on every existing base; the buildable still works, so it fails quietly | GUID/parent/`ID` copied byte-for-byte from the vanilla file; the `.et.meta` `Name` carries the vanilla GUID; a wrong GUID makes an orphan nothing instances (visible, not silent); Phase 4's Workbench check opens a **leaf** variant of each family |
| R6 | **Vehicle seating fails** — `MoveInVehicle` locks the slot it hands out for a frame, so a tight loop seats one man and drops the rest | Medium | Vehicle groups arrive with an empty truck and men standing around it | One member per call-queue hop, the `OVT_VehicleSpawningDeploymentModule` shape (`:652`, `:702-712`); a member that cannot be seated stays on foot rather than being deleted; play-test step 21 |
| R7 | **The map ships controller-dead** — the known failure mode this requirement calls out by name | Medium | The whole command layer is mouse-only on a console-adjacent build | The `OverthrowMapLayersContext` recipe verbatim (Priority 70 / `Flags 0x26 0`), per-frame re-activation, the one-frame arm delay, the cursor magnet reused rather than reinvented, `shoulder_left` banned, the conflict checker at baseline, and Play-test B is **gamepad only with the mouse untouched** |
| R8 | **Serializer bump breaks a live save** — a positional context read at the wrong version | Low–Medium | Total loss of camps, FOBs or bases on load | Version bump with a read-and-discard branch, never a field deletion; the OF serializer's own in-place warning (`:264`) is the evidence; Phase 11 is late so a broken bump is caught with HC already working; a pre-bump-save load is a Definition-of-Done item |
| R9 | **Persisted bodies do not come back** — 48 members × several groups × async `RequestSpawn` with a 15 s timeout each | Medium | A long, stuttery load, or groups that reload short-handed | Loading is one group per call-queue hop; each member has a **prefab fallback** (D8) so a lost body costs gear, never a man; the recruit path's timeout and fresh-spawn fallback are reused, not reinvented; play-test step 29 checks gear specifically |
| R10 | **Warehouse coverage is exploitable** — quote at a stocked warehouse, buy after moving the stock, or two players racing the same stock | Medium | Free gear, or a double-spend | The quote is advisory and the server **re-derives and re-charges** at purchase time (D19); stock is taken with `OVT_StorageLedger.Take`, which returns what it actually took; the whole purchase is one server-side operation with no await in the middle; play-test step 44 races two clients |
| R11 | **Removing `OVT_EGroupOrigin` members re-ordinals the rest** | Medium | A persisted or replicated ordinal silently means a different origin after the change | T11.6 requires confirming no persisted/replicated field carries these ordinals **before** deleting; if any does, they are retired as reserved values instead |
| R12 | **Scope creep into transfer/officer takeover** — the requirements body describes both at length | Medium | The feature grows a permission model and an ownership-change protocol it was scoped out of | The planning-decisions section defers both explicitly; §2's non-goals restate it; the record already carries `m_sOwnerPersistentId` as its own field so the follow-up needs one setter and one delta, which is the argument for *not* building it now |
| R13 | **Balance is wrong on first contact** — a rifle squad costs a fortune or nothing; 48 members is absurd either way | **High** | The feature is either unusable or trivialises the campaign | Every price derives from the shipped `baseRecruitCost` × members plus real item prices; the cap, the supporter rate, both radii and both tick intervals are config; play-test steps 16–29 are the gate; nothing here needs a script change to retune |
| R14 | **Concurrent sessions** change the tree between phases | Medium | Merge pain, stale line references, a phase built against a moved seam | Re-baseline before every phase; every claim carries a file:line; `OVT_ResistanceFactionManager.c`, `OVT_MapLayersUI.c`, `Overthrow.conf` and `chimeraInputCommon.conf` are the shared files most likely to move |
| R15 | **The GUID series is not actually free** — the vanilla-tree sweep timed out during planning | Low | A GUID collision with a vanilla asset, which is a silent wrong-resource bug | Re-verify `6B1C3D` and `6B0E7A8` against **both** trees before authoring anything (§3.13, verification step 13); it is a Phase 1 precondition, not a hope |

---

## 10. Quality Bar

**Server integrity**

- **B1 — Server-authoritative without exception.** Every record mutation, spawn, order, charge, supporter draw-down and stock take happens on the server, reached only through `OVT_HighCommandRequestComponent` or the manager's server-gated ticks. No client writes a record, ever, including on a listen host. Client-side predicates (action gates, the purchase screen's enable state) are advisory and the server re-derives all of them.
- **B2 — Identity is never on the wire.** Every ask derives its actor from `ResolveOwningPlayerId()`. A request that carries a player id, a persistent id or an owner is a defect.
- **B3 — Ordering is the money rule.** Spawn → consume stock → take supporters → take money, funds checked last. A failed spawn charges nothing and consumes nothing. Nothing is charged for an effect that did not happen — the epic's dominant defect class.
- **B4 — Rejections are visible.** Every refused request answers `RpcDo_HCResult` with a code the player sees, and every broadcast tag has a preset in `Configs/overthrowBroadcastMessages.conf` (a tag with no preset is silently dropped — BUG-120). Silent returns and log-only rejections are the shape being avoided.
- **B5 — The wire is auditable.** Eleven RPCs, each with its arity written down and checked against its handler, because `Rpc()`'s untyped variadic prototype means the compiler will not do it (BUG-090). The heartbeat has its own RPC and headroom.
- **B6 — Nothing outlives its group.** Observer, waypoints, vehicle and record all die with the group entity, through its own `OnDelete`, which covers every destruction route including vanilla's delete-when-empty. Cleanup hung off a manager code path would be cleanup that mostly does not run.
- **B7 — Persistence never applies a failed read.** Every `Read()` return checked; abort + ERROR; live state untouched. Persisted record classes are frozen and separate from live ones. Serialize/Deserialize locals identically named.
- **B8 — Per-call state only.** Every sphere-query accumulator is a `new`-ed object, never a static member.
- **B9 — No exact float comparison decides anything.** Distances are compared squared; `vector.Distance` is not correctly rounded and an exact-boundary decision would be a coin flip.
- **B10 — One radius, one cap, one fee multiplier.** Each has exactly one definition, consulted by the client pre-check, the server validation and the display alike.

**Map and gamepad UX**

- **B11 — Gamepad parity, proven by a mouse-free play-test.** Selecting, ordering, cycling stance, filtering the layer, buying at a barracks and dismissing from the roster are all reachable with the d-pad/stick, `a`, `b`, `x`, `y` alone. `shoulder_left` is never bound.
- **B12 — The input context is a single-frame lease.** Re-activated every frame it is wanted, armed one frame late, and never left active when the map is closed.
- **B13 — One property, one owner.** Visibility belongs to the filter row, opacity to `Update()`, selection to the ID_D quad. No two rules write the same widget property.
- **B14 — Focus is never lost**, including across a live record refresh while the roster is open and across a purchase.
- **B15 — The labels never lie.** Every number on the map, the roster and the purchase screen comes from the replicated record or from a server quote, and the price shown is the price charged (or is honestly labelled an estimate). A client never computes a price.
- **B16 — No `ALWAYS_TOP` focusable widget**, and no hover target grown through the widget tree — the trace is clipped to parent bounds and ancestor size overrides squeeze grown containers. Use the cursor magnet.
- **B17 — Nothing is hidden that could be shown disabled with a reason.** A row whose only outcome is a no-op click is a bug (BUG-102).

---

## Agent Routing Summary

| Phase | Agent | Why |
|---|---|---|
| 1 — pure spine, manager skeleton, seam, serializer v1 | `component-developer` | Pure classes, a skeleton manager, a controller-component registration on rails |
| **2 — group entity, observer, faction, waypoints, stances** | **`component-developer-advanced`** ⚠️ | Entity lifecycle with three documented traps (deferred observer install, delete-when-empty, waypoint ownership), a new group prefab, and a leak whose only symptom is a slow server |
| **3 — purchase server half, manifest, pricing, wire** | **`component-developer-advanced`** ⚠️ | A money path with a nine-step gate ladder and a strict effect order, plus a spawn-inspect cache that must not hitch, crash or lie |
| 4 — barracks prefabs, desk action, purchase screen | `ui-developer` | Three same-GUID vanilla deltas on a shipped recipe, one action, one screen. **Workbench-gated for the desk positions** |
| 5 — tent warehouse discount | `component-developer` then `ui-developer` | An additive field, one arity change, one UI line |
| **6 — replication: JIP, deltas, heartbeat** | **`network-specialist-advanced`** ⚠️ | Eleven RPCs, a positional JIP payload, a heartbeat with a change filter, and BUG-090's compile blind spot |
| **7 — map C&C + gamepad context** | **`ui-developer-advanced`** ⚠️ | A new map layer, a new input scheme over an open map, a cursor magnet, and the "ships controller-dead" failure mode the requirement names |
| 8 — Manage Groups roster + main-menu item | `ui-developer` | A roster on the `OVT_RecruitsContext` model and one named button |
| **9 — recruit conversion + split roster** | **`component-developer-advanced`** then `ui-developer` ⚠️ | A one-way seam into the 3.6k-line recruit manager touching body reservation, cap accounting and observer ownership at once |
| 10 — rearm + refuel ticks | `component-developer` | Two server ticks over two shipped utility surfaces |
| **11 — garrison retirement + serializer bumps** | **`component-developer-advanced`** ⚠️ | Deletions across seven files plus two positional serializer version bumps whose failure mode is total loss of camps, FOBs or bases |
| 12 — objectives predicate, QRF check, persistence, review | `component-developer` + main thread | One expression, one assertion, three round-trip cases — then the cross-phase review that has caught the real defect in each of the last three features |
| 13 — localization, input check, help & wiki | main thread + `help-docs-sync` | `.st` structural safety (unbalanced braces = data loss) and the wiki's known write failure modes |

**Parallelism:** Phases 4 and 5 may run in parallel once 3 lands. Phase 6 may run in parallel with 4–5 once 2 lands. Phase 8 may run in parallel with 7. Phase 10 may run in parallel with 7–9. Phase 11 depends on everything above it and must not start early. Phases 12 and 13 are last, in that order.

**Every implementation-agent prompt must carry, verbatim:**

> Do not run `tools/run-tests.sh`. Your gate is `tools/compile-check.sh` exit 0 — I run the test suites myself after the phase completes.
