# Virtualization Core — Implementation Plan

**Status:** Planning
**Started:** 2026-08-07
**Target Completion:** TBD
**Last Updated:** 2026-08-07

**Epic:** `virtualization` (feature #1 of 5 — see `docs/features/virtualization/epic-overview.md`)
**Requirements:** `docs/features/virtualization/core/requirements.md` (authoritative for scope)
**Addresses:** GitHub issue **#100**, rebuilt on vanilla persistence (the EPF `virtualization` branch is reference material only)

---

## 1. Executive Summary

Overthrow has four separate, incompatible answers to "when should this AI exist?" — base-upgrades value banking, deployments proximity toggling, radio-tower garrison spawn/despawn, and the town civilian spawner. Each re-implements `PlayerInRange` polling, none of them remembers who died, and none of them survives a save with its group state intact.

This feature builds the one layer they will all converge on: **`OVT_VirtualizationManagerComponent`**, a server-only Manager on the game mode that owns *virtual group records* and their spawn/despawn lifecycle.

The design has four load-bearing parts:

1. **A record, not an entity.** A virtual group is a plain server-side record — faction, composition, per-member alive state, position, waypoint plan, owner tag. The live `SCR_AIGroup` is a transient projection of the record, created when a player comes close and deleted when they leave. The record is the truth; the entity never is.
2. **One central tick, two rate-limited work queues.** A single ~1 s `CallLater` evaluates a round-robin slice of records, decides spawn/despawn, and pushes operations onto queues drained a few per tick. Cost is flat as record count grows and there is exactly one budget knob to turn. Within a single group, member materialization is handed to vanilla's own delayed spawner, which already spreads members across frames with navmesh back-pressure.
3. **Per-member truth, end to end.** The record holds one alive flag per roster slot. Deaths are recorded the moment they happen (the game mode's existing `GetOnCharacterKilled()` invoker), respawns materialize exactly the surviving slots, and a wiped group's record is deleted and never comes back. This is the player-facing promise issue #100 left open.
4. **Persistence ships in this feature, not later.** A vanilla-persistence serializer with dedicated persisted-record classes, plus Persistence-tier round-trip cases proven able to fail.

Alongside tracked records, core ships a **second registration class**: config-declared **ambient spawn sources** for one-off, non-persisted, untracked entities (town civilians, later parked vehicles) on the same proximity/frame-spread lifecycle, with an **ownership-transfer** seam so a player-claimed entity leaves ambient management instead of being deleted.

**Core ships seams, not consumers.** No town wiring, no civilian configs, no deployment migration, no virtual movement. Its whole job is to be a stable API for four downstream features.

---

## 2. Goals

### Primary

- **G1** A single server-side registry owns AI group lifecycle: create, query, reclaim, destroy — with no consumer touching record internals.
- **G2** Groups spawn when a player comes within the configured distance and despawn when none is, with **no frame hitch** when a player fast-travels into a dense area.
- **G3** **Dead members stay dead.** A group that lost 3 of 8 comes back with 5, across despawn *and* across save/load. A group that lost all 8 is removed and never returns.
- **G4** Spawn distance is **server-configurable** in `Overthrow_Config.json` (not a code constant), with a per-registration override; a very large value keeps everything spawned — issue #100's server-owner ask.
- **G5** Every record round-trips through a vanilla-persistence serializer, gated by a Persistence-tier test proven able to fail.
- **G6** Ambient spawn sources are declarative and modder-extendable via config classes, non-persisted, and support ownership transfer.

### Secondary

- **G7** The registration API is stable enough that `civilians`, `movement`, `integration` and `base-defense-migration` can be planned against it without core changing shape.
- **G8** The record's position + waypoint plan + progress are exposed as mutation points so `movement` is a tick strategy over core, not a fork of it.
- **G9** Core never leaks waypoint entities — the defect present in every other spawner in the tree (see D9).
- **G10** The Economy 2.0 extensibility seam is documented and nothing agent-specific is built (§3.8).
- **G11** The spawn/despawn budget is **measured** under a dense fast-travel, not assumed (Phase 6).

---

## 3. Architecture Overview

### 3.1 Where each piece lives

```
SERVER ONLY — nothing here replicates, nothing here has a client half.

OVT_OverthrowGameMode (EOnInit)                OVT_OverthrowConfigComponent
  m_Virtualization.Init(this)                    m_ConfigFile.virtualizationSpawnDistance
  (DoStartGame) .PostGameStart()  ── starts ──┐   (Overthrow_Config.json, default 1750)
                                              │
                        OVT_VirtualizationManagerComponent
                        ┌──────────────────────┴────────────────────────────┐
                        │  Tick()  every ~1000 ms                           │
                        │   1. evaluate round-robin slice of N records      │
                        │      -> OVT_Global.NearestPlayerDistance()        │
                        │      -> enqueue spawn / despawn (nearest first)   │
                        │   2. fire m_OnRecordEvaluated(record, dt)  <── movement subscribes here
                        │   3. drain despawn queue (M/tick)                 │
                        │   4. drain spawn queue (K/tick)                   │
                        └───────────────────────────────────────────────────┘
                              │                                  │
        TRACKED (persisted)   │                                  │   AMBIENT (not persisted)
   m_aRecords: array<ref OVT_VirtualGroupRecord>      m_aAmbientSources: array<ref OVT_AmbientSpawnSourceInstance>
     handle, ownerSystem/ownerKey                       handle, ownerKey, position
     factionKey + groupRegistryName + prefab            ref OVT_AmbientSpawnSourceConfig  (config class, modder-subclassable)
     memberPrefabs[] + memberAlive[]                    live entity list (discarded on despawn, re-rolled on respawn)
     virtual position, waypoint plan + progress         m_mEntityToSource: map<EntityID,int> (ownership transfer, O(1))
     spawnDistanceOverride
     live: SCR_AIGroup (transient) + owned waypoints

  DEATH IN  <── OVT_OverthrowGameMode.GetOnCharacterKilled()  (fires for AI; SCR_CharacterDamageManagerComponent.c:52)
  SAVE OUT  ──> OVT_VirtualizationManagerSerializer  ──> Configs/Systems/Persistence/Overthrow.conf
  RESTORE   <── ApplyPersistedVirtualGroups()  then  m_OnRecordsRestored.Invoke()  <── consumers reclaim here
```

### 3.2 Manager vs Controller — decided

**Manager.** System-wide singleton state, coordinates many instances, server-only, no per-entity component needed. Records are *data*, not entities; making each a controller component would put an entity in the world for every virtual group — the exact cost virtualization exists to avoid. Follows the standard pair (`OVT_DeploymentManagerComponent.c:19-61` is the closest shape):

```c
[EntityEditorProps(category: "Overthrow/Managers", description: "Owns virtual AI group records and their spawn/despawn lifecycle")]
class OVT_VirtualizationManagerComponentClass : OVT_ComponentClass {}

class OVT_VirtualizationManagerComponent : OVT_Component
{
    static OVT_VirtualizationManagerComponent s_Instance;
    static OVT_VirtualizationManagerComponent GetInstance();   // gameMode.FindComponent, lazy
    override void OnPostInit(IEntity owner);                   // if (!Replication.IsServer()) return; then allocate
    void Init(IEntity owner);                                  // called from OVT_OverthrowGameMode.EOnInit
    void PostGameStart();                                      // starts the tick — called from DoStartGame()
    override void OnDelete(IEntity owner);                     // removes the CallLater, clears s_Instance (R7)
}
```

Accessor added to `OVT_Global` (`Scripts/Game/Global/OVT_Global.c`, alongside `GetDeploymentManager()` :216):

```c
static OVT_VirtualizationManagerComponent GetVirtualization()
{
    return OVT_VirtualizationManagerComponent.GetInstance();
}
```

> The `core/player-groups` plan froze the `OVT_Global` locator half. This is a deliberate, justified exception: four sibling features program against this manager and `OVT_Global` is where every one of their authors will look first. It is the last locator this epic adds.

### 3.3 The record classes

`Scripts/Game/GameMode/Virtualization/OVT_VirtualGroupRecord.c`:

```c
//! One virtual member = one slot of the group prefab's m_aUnitPrefabSlots, by index.
class OVT_VirtualGroupMember
{
    ResourceName m_rPrefab;   //!< captured from the group prefab's slot list at registration
    bool m_bAlive;
}

enum OVT_EVirtualWaypointType
{
    MOVE,
    PATROL,
    WAIT,
    DEFEND,
    CYCLE
}

//! Data only. Core stores the plan; `movement` advances it; the spawn path turns it into
//! real AIWaypoint entities (whose lifetime core owns — see D9).
class OVT_VirtualWaypointPlan
{
    ref array<vector> m_aPositions = {};
    ref array<int> m_aTypes = {};       //!< OVT_EVirtualWaypointType, parallel to m_aPositions
    ref array<float> m_aParams = {};    //!< per-type parameter (wait seconds, patrol radius), parallel
    int m_iCurrentIndex;                //!< index the group is heading to; `movement` mutates this
    float m_fLegProgress;               //!< 0..1 along the current leg; `movement` mutates this
    bool m_bCycle;
}

class OVT_VirtualGroupRecord
{
    int m_iHandle;                      //!< stable identity, survives save/load
    string m_sOwnerSystem;              //!< "deployment", "tower_garrison", "economy_delivery", ...
    string m_sOwnerKey;                 //!< consumer-defined identity used to reclaim after load

    string m_sFactionKey;               //!< "USSR" — NEVER the faction index (D4)
    string m_sGroupRegistryName;        //!< "light_patrol"
    ResourceName m_rResolvedPrefab;     //!< fallback if the registry entry is gone

    ref array<ref OVT_VirtualGroupMember> m_aMembers = {};

    vector m_vPosition;                 //!< virtual position while despawned
    ref OVT_VirtualWaypointPlan m_WaypointPlan;
    int m_iSpawnDistanceOverride = -1;  //!< -1 = use the global config value

    // runtime only — never persisted
    SCR_AIGroup m_SpawnedGroup;
    ref array<AIWaypoint> m_aLiveWaypoints = {};
    bool m_bDespawning;                 //!< suppresses death accounting during our own teardown
    int m_iLastEvaluatedMs;             //!< world time of last tick evaluation; feeds `movement`'s dt
    int m_iQueuedOp;                    //!< 0 none / 1 spawn / 2 despawn — prevents double-queueing
}
```

**Roster capture at registration, not at first spawn.** `SCR_AIGroup` exposes a static helper (`SCR_AIGroup.c:~40-60`) that reads `m_aUnitPrefabSlots` straight off a prefab's entity source without instantiating it. Core uses it so `m_aMembers` exists — with per-member alive state — from the moment `RegisterGroup()` returns, before anything has ever spawned. Fallback if the read fails: resolve on first spawn and log a WARNING.

### 3.4 Registration API — the epic's contract

**This is the most important artifact in this feature.** Four downstream features program against it; treat any change after Phase 1 as a breaking change and record it in `context.md`.

```c
// ============ TRACKED GROUPS (persisted, per-member state) ============

//! Creates a virtual group record. Returns its handle, or -1 on failure (unresolvable
//! composition, not server, manager not initialised).
//! \param ownerSystem  which system owns this (free-form; see the Economy 2.0 seam, §3.8)
//! \param ownerKey     consumer-defined identity, used by FindGroupsByOwner() to reclaim after a load
//! \param factionKey   OVT_Faction.m_sFactionKey, e.g. "USSR" — never a faction index
//! \param groupName    OVT_FactionGroupEntry.m_sGroupName, e.g. "light_patrol"
//! \param plan         optional waypoint plan; may be null (a static garrison has none)
//! \param spawnDistanceOverride  -1 to use the global configured distance
int RegisterGroup(string ownerSystem, string ownerKey, string factionKey, string groupName,
                  vector position, OVT_VirtualWaypointPlan plan = null, int spawnDistanceOverride = -1);

//! Despawns any live entities and removes the record. Idempotent; false if the handle is unknown.
bool UnregisterGroup(int handle);

bool IsRegistered(int handle);
int GetGroupCount();

//! Server-side read access. Returns the live ref (server-only, single-threaded); prefer the
//! typed accessors below for anything a consumer wants to CHANGE.
OVT_VirtualGroupRecord GetRecord(int handle);

//! Reclaim seam. After a load, a consumer calls this with the same (ownerSystem, ownerKey) it
//! registered with and gets its handles back without having persisted them itself.
array<int> FindGroupsByOwner(string ownerSystem, string ownerKey);
array<int> FindGroupsBySystem(string ownerSystem);

// -- state --
vector GetVirtualPosition(int handle);
void   SetVirtualPosition(int handle, vector position);         //!< `movement` writes here
OVT_VirtualWaypointPlan GetWaypointPlan(int handle);
void   SetWaypointPlan(int handle, OVT_VirtualWaypointPlan plan);
int    GetAliveMemberCount(int handle);
int    GetMemberCount(int handle);                              //!< roster size incl. dead
bool   IsSpawned(int handle);
SCR_AIGroup GetSpawnedGroup(int handle);                        //!< null while virtual

//! Records a member death by roster slot. Called internally by the kill hook; public so
//! consumers (and tests) can report deaths they observed themselves.
//! Removing the last living member removes the record and fires OnGroupWiped.
void ReportMemberKilled(int handle, int slotIndex);

// -- lifecycle overrides (proximity is the default; these are the escape hatches) --
void ForceSpawn(int handle);      //!< queue a spawn regardless of proximity
void ForceDespawn(int handle);    //!< queue a despawn regardless of proximity

// -- events (all server-side ScriptInvokers) --
ScriptInvoker GetOnGroupSpawned();      //!< (int handle, SCR_AIGroup group)
ScriptInvoker GetOnGroupDespawned();    //!< (int handle, int survivorsRemaining)
ScriptInvoker GetOnGroupWiped();        //!< (int handle) — fired BEFORE the record is removed
ScriptInvoker GetOnRecordEvaluated();   //!< (OVT_VirtualGroupRecord record, float deltaSeconds) — `movement`'s seam
ScriptInvoker GetOnRecordsRestored();   //!< () — fired once after persisted records are applied

// ============ AMBIENT SPAWN SOURCES (not persisted, untracked, re-rolled) ============

int  RegisterAmbientSource(notnull OVT_AmbientSpawnSourceConfig config, vector position, string ownerKey);
bool UnregisterAmbientSource(int handle);          //!< deletes live entities and drops the source
int  GetAmbientSourceCount();
array<IEntity> GetAmbientEntities(int handle);

//! OWNERSHIP TRANSFER. The entity stops being ambient: it is removed from its source's list and
//! will NOT be deleted on the next despawn. Returns false if the entity was not ambient.
//! O(1) via the reverse entity->source map. This is what `civilians` calls when a player
//! recruits a civilian, and what a future vehicle-theft path calls.
bool ReleaseAmbientEntity(notnull IEntity entity);
```

**Naming discipline:** everything a consumer holds is an `int handle`. Handles are allocated from a monotonic counter that is itself persisted, so a handle is never reused across a reload. Consumers may persist handles, but `FindGroupsByOwner` exists so they do not have to — prefer it (R3).

### 3.5 Ambient spawn-source config classes

`Scripts/Game/GameMode/Virtualization/OVT_AmbientSpawnSourceConfig.c` — declarative and subclassable, following `OVT_DeploymentRegistry`'s shape (`ScriptAndConfig` + `[BaseContainerProps(configRoot: true)]`):

```c
[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sSourceName")]
class OVT_AmbientSpawnSourceConfig : ScriptAndConfig
{
    [Attribute(desc: "Unique name for this source type")]
    string m_sSourceName;

    [Attribute(uiwidget: UIWidgets.ResourceNamePicker, desc: "Prefabs this source may roll", params: "et")]
    ref array<ResourceName> m_aPrefabs;

    [Attribute(defvalue: "1", desc: "Minimum entities to spawn")]
    int m_iMinCount;
    [Attribute(defvalue: "1", desc: "Maximum entities to spawn (inclusive)")]
    int m_iMaxCount;
    [Attribute(defvalue: "100", desc: "Scatter radius around the source position")]
    float m_fRadius;
    [Attribute(defvalue: "-1", desc: "Spawn distance override; -1 uses the configured global")]
    int m_iSpawnDistanceOverride;

    // ---- modder seam: override these in a subclass, no core change required ----
    ResourceName RollPrefab();                                   //!< default: random element
    int RollCount();                                             //!< default: RandInt(min, max + 1)
    vector RollPosition(vector origin, float radius);            //!< default: random non-ocean near origin
    void OnEntitySpawned(IEntity entity, vector sourcePosition); //!< default: no-op (clothes, waypoints, flags)
    void OnEntityDespawning(IEntity entity);                     //!< default: no-op
}

[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sRegistryName")]
class OVT_AmbientSpawnSourceRegistry : ScriptAndConfig
{
    [Attribute()] string m_sRegistryName;
    [Attribute()] ref array<ref OVT_AmbientSpawnSourceConfig> m_aSources;
    OVT_AmbientSpawnSourceConfig FindByName(string name);
}
```

The manager carries `[Attribute()] ref OVT_AmbientSpawnSourceRegistry m_AmbientRegistry` so a `.conf` can supply sources later. **Core ships it empty** — `civilians` authors the content.

⚠️ `RandInt` is **max-exclusive** and `RandInt(n, n)` raises an engine error. `RollCount()`'s default must be `RandInt(m_iMinCount, m_iMaxCount + 1)`, with an equality short-circuit.

### 3.6 Tick and queue design

```c
[Attribute(defvalue: "1000", desc: "Milliseconds between virtualization ticks")]
int m_iTickIntervalMs;
[Attribute(defvalue: "25", desc: "Records evaluated per tick (round-robin)")]
int m_iRecordsPerTick;
[Attribute(defvalue: "1", desc: "Group spawn operations drained per tick")]
int m_iSpawnOpsPerTick;
[Attribute(defvalue: "3", desc: "Group despawn operations drained per tick")]
int m_iDespawnOpsPerTick;
[Attribute(defvalue: "1.15", desc: "Despawn distance multiplier (anti-thrash hysteresis)")]
float m_fDespawnHysteresis;
[Attribute(defvalue: "150", desc: "Milliseconds between members of one group")]
int m_iMemberSpawnDelayMs;
```

One `CallLater(Tick, m_iTickIntervalMs, true)` started in `PostGameStart()`. Per tick:

1. **Evaluate a slice.** `m_iRecordsPerTick` records starting at a round-robin cursor. For each: resolve the effective spawn distance, read `OVT_Global.NearestPlayerDistance(record.m_vPosition)`, decide the desired state, and enqueue if it differs and no op is already queued.
2. **Fire `m_OnRecordEvaluated(record, deltaSeconds)`** with the real elapsed time since that record's last evaluation. Uneven slicing is therefore safe for `movement`: it integrates dt rather than assuming a fixed cadence.
3. **Drain despawns first** (they *free* budget), then spawns, nearest-player-first.

Ambient sources ride the same loop with the same rules (their own cursor, their own queue entries).

**Worst-case reaction latency** is `ceil(recordCount / m_iRecordsPerTick) * m_iTickIntervalMs`. At the defaults, 200 records react within 8 s — 220 m of driving at 100 km/h against a 1750 m radius. Both numbers are attributes, so an operator can trade CPU for reactivity without a rebuild.

**Cancellation.** A record queued for spawn that leaves range before the queue reaches it is *dequeued*, not spawned-then-despawned. Same in reverse. This is what makes a fast drive past a dense area cheap.

**Hysteresis.** Despawn distance is `spawnDistance * m_fDespawnHysteresis`. Without it, a player standing on the boundary thrashes a group in and out every tick.

### 3.7 Persistence design

**Dedicated persisted-record classes, never the live record.** Same reasoning as `OVT_TownManagerSerializer.c:1-12`: the live class carries runtime-only members (`m_SpawnedGroup`, `m_aLiveWaypoints`, queue state) and is free to change with gameplay work; a save format is not.

```c
class OVT_PersistedVirtualGroup
{
    int handle;
    string ownerSystem;
    string ownerKey;
    string factionKey;
    string groupRegistryName;
    ResourceName resolvedPrefab;
    vector position;
    int spawnDistanceOverride;

    // Roster travels as PARALLEL ARRAYS (the town serializer's idiom): a slot whose prefab no
    // longer exists can be dropped on load without leaving a half-built member object behind.
    ref array<ResourceName> memberPrefabs = {};
    ref array<int> memberAlive = {};          // 0/1, parallel to memberPrefabs

    // Waypoint plan, also flat.
    ref array<vector> waypointPositions = {};
    ref array<int> waypointTypes = {};
    ref array<float> waypointParams = {};
    int waypointIndex;
    float waypointLegProgress;
    bool waypointCycle;
}
```

Serializer `OVT_VirtualizationManagerSerializer : ScriptedComponentSerializer` — `GetTargetType()` → the manager, `context.WriteValue("version", 1)` first, `if (version < 1) return true;` on read (an absent payload must never zero live state), `Write`/`Read` order identical, append-only forever. Apply through the **public** manager method:

```c
void ApplyPersistedVirtualGroups(array<ref OVT_PersistedVirtualGroup> records, int nextHandle);
```

The serializer stays a pure codec; every side effect lives in the manager (precedent: `OVT_DeploymentComponent.ApplyPersistedDeployment()` :100-153).

Contract points:

- **Idempotent.** Re-applying to a live session (what `ReapplyLatestSaveData()` does, and what the round-trip test does) must produce the same state: match by `handle`, update in place, create only what is missing, drop live records the payload does not contain.
- **Restore does not spawn.** It writes records; the next tick reconciles. A record whose live group is already spawned keeps it and has its virtual state refreshed.
- **`m_OnRecordsRestored` fires once at the end**, so consumers reclaim deterministically regardless of serializer ordering (R2).
- **Faction key, not index.** Indices are positional across saves (`OVT_DeploymentManagerSerializer.c:20-23`). The key is the match; `resolvedPrefab` is the fallback when the key or the registry entry is gone; if both fail the record is dropped with a WARNING (precedent: `ApplyPersistedDeployment` drops a deployment whose config was renamed).
- **Nothing spawned is entity-persisted.** Core never calls `OVT_PersistenceTracking.Track()`. Live members are transient projections; the record is the truth.
- **Liveness is current at save time.** Deaths are recorded when they happen, and `Serialize()` does a defensive reconciliation pass against live groups before writing.

Registration in `Configs/Systems/Persistence/Overthrow.conf`, in the game-mode `ComponentSerializers` block (:23-64), continuing the `6B0E7A2x` series with a **freshly generated, verified-unique** GUID:

```
        OVT_VirtualizationManagerSerializer "{6B0E7A226C80E4B1}" {
        }
```

### 3.8 The Economy 2.0 extensibility seam (design-for, build-nothing)

Requirements say the abstraction must not preclude future non-combat virtual agents. Four specific properties make that true, and all four are already in the design above — **nothing agent-specific is built**:

| Seam | Why it is enough |
|---|---|
| `m_sOwnerSystem` is a **string**, not an enum | A new class of virtual thing (`"economy_delivery"`, `"economy_citizen"`) needs no core edit, no enum bump, no serializer version bump. |
| `GetOnRecordEvaluated()` gives every record a **per-record tick with real dt** | Behaviour lives in the consumer. `movement` is the first subscriber; a future economy tick is the second. Core stays behaviour-free. |
| Composition is `(factionKey, groupName, resolvedPrefab)` | A non-combat agent registers a civilian or vehicle prefab through exactly the same field. Nothing in the record says "soldier". |
| `m_iSpawnDistanceOverride` is per record | An agent that should never materialize registers `0`; one that should always exist registers a huge value. No new lifecycle mode. |

Anything beyond this — virtual economy state, agent goals, transactions — belongs to Economy 2.0 and is **out of scope**.

---

## 4. Implementation Phases

Each phase ends compiling clean and with its automated gate green. **Phase 2 is a measurement gate: Phase 3 must not start until it has a written answer.**

---

### Phase 1 — Records, registration API, manager scaffold, config field

**Agent:** `component-developer`
**Estimate:** 6-10 h
**Why first:** the API is the epic's contract. It lands, gets reviewed, and only then gets a lifecycle behind it.

**Tasks**

1. **T1.1** Create `Scripts/Game/GameMode/Virtualization/OVT_VirtualGroupRecord.c` — `OVT_VirtualGroupMember`, `OVT_EVirtualWaypointType`, `OVT_VirtualWaypointPlan`, `OVT_VirtualGroupRecord` exactly as §3.3. `ref` on every Managed member of an array or map.
2. **T1.2** Create `OVT_VirtualizationManagerComponent.c` with the class pair, `s_Instance`/`GetInstance()`, `OnPostInit` (server guard **before** allocating collections, `OVT_DeploymentManager.c:64-84`), `Init(IEntity)`, empty `PostGameStart()`, and `OnDelete` clearing `s_Instance` and removing the (not yet started) `CallLater`.
3. **T1.3** Implement the **whole tracked-group API surface of §3.4** except the lifecycle behaviour: register/unregister/query/reclaim/state accessors/`ReportMemberKilled`/all five invokers. `ForceSpawn`/`ForceDespawn` exist and enqueue; the queue is drained in Phase 3.
   - `RegisterGroup` resolves composition via `OVT_Global.GetFactions().GetOverthrowFactionByKey(factionKey)` → `OVT_Faction.GetGroupPrefabByName(groupName)`; both return null/`""` on miss — bail with `-1` and a WARNING naming the key and the group name.
   - Capture the roster from the group prefab's `m_aUnitPrefabSlots` via `SCR_AIGroup`'s static prefab-source helper (§3.3); no instantiation.
   - `ReportMemberKilled` marking the last living member removes the record and fires `GetOnGroupWiped()` **before** removal.
4. **T1.4** Create `OVT_VirtualizationMath.c` — the pure, world-free statics the Logic tier can reach: `ResolveSpawnDistance(override, globalDefault)`, `ResolveDespawnDistance(spawnDistance, hysteresis)`, `CountAlive(array<int>)`, `IsWiped(array<int>)`, `SurvivingPrefabs(roster, aliveFlags)`, `SliceIndices(count, cursor, sliceSize, out array<int>)`, `AdvanceCursor(cursor, sliceSize, count)`, `ShouldSpawn(isSpawned, distance, spawnDistance)`, `ShouldDespawn(isSpawned, distance, despawnDistance)`. **No manager, game mode or world reference anywhere in this file.**
5. **T1.5** Add `int virtualizationSpawnDistance;` to `OVT_OverthrowConfigStruct` (`OVT_OverthrowConfigComponent.c:24-66`) and `virtualizationSpawnDistance = 1750;` to `SetDefaults()` (:46-65). No JIP bitstream change — this is a server-only value and the positional `RplSave` at :554-586 carries only client-needed values. Leave `m_iMilitarySpawnDistance` / `m_iCivilianSpawnDistance` alone; un-migrated consumers still use them.
6. **T1.6** Add `OVT_Global.GetVirtualization()` and `OVT_Global.NearestPlayerDistance(vector pos)` — the latter mirroring `PlayerInRange` (`OVT_Global.c:231-260`), **skipping dead players** (`NearestPlayer` :262 does not), returning `float.MAX` when there are none.
7. **T1.7** Register the manager in `OVT_OverthrowGameMode`: field + `FindComponent`/`Init(this)` block in `EOnInit` (:1056-1219, **after** Deployment at :1156), and a `PostGameStart()` block in `DoStartGame()` (:244 area).
8. **T1.8** **User task (Workbench):** add `OVT_VirtualizationManagerComponent` to `Prefabs/GameMode/OVT_OverthrowGameMode.et`. Prefab editing is the interactive path; the agent must not hand-edit the `.et`. Until this is done every `GetInstance()` returns null and the Init-tier case fails — which is exactly the fail-proof for T1.10.
9. **T1.9** Create `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_Virtualization.c` covering T1.4's statics: override precedence (-1 → global, 0 → never, huge → always), hysteresis, alive counting, wipe detection, survivor-prefab selection preserving slot order, slice arithmetic with wraparound and with `sliceSize > count`, and the spawn/despawn predicates either side of both thresholds.
   ⚠️ **Tier rule:** the Logic directory is grepped for the manager-accessor and game-mode-getter identifiers — they may not appear **anywhere** under `TestSuites/Logic/`, *including in comments* (`OVT_TEST_LogicSuite.c` header, rule 2). Write the prose around them.
10. **T1.10** Add to `OVT_TEST_InitSuite.c`: a case asserting the manager resolves through `OVT_Global`, that `GetGroupCount()` is 0 before anything registers, and that `virtualizationSpawnDistance` reads back its default.
11. **T1.11** Write `docs/features/virtualization/core/api.md` — the §3.4 signatures plus a worked "how a consumer registers, reclaims after a load, and unregisters" example. This is what siblings are planned against; keep it current at every later phase.

**Acceptance criteria**

- `tools/compile-check.sh` exits 0.
- `tools/run-tests.sh "{6A6E29FF47ECB840}"` (Fast) exits 0, including the new Logic cases and the new Init case.
- Every new case has its can-fail proof recorded in a preamble comment. No `maxAttempts`.
- `RegisterGroup` with an unknown faction key, and with an unknown group name, both return -1 and log a WARNING — asserted in the Init tier.
- `api.md` exists and matches the code.
- `grep -rn "Rpc\|RplProp\|Replication.Bump" Scripts/Game/GameMode/Virtualization/` is empty.

---

### Phase 2 — Partial-spawn validation spike (gate, ~half a day)

**Agent:** `component-developer`
**Estimate:** 3-5 h + one play-test round
**Why it is its own phase:** the entire per-member promise (G3) rests on being able to materialize *exactly the surviving slots* of a group prefab. Nothing in Overthrow has ever used this seam. Phase 3's shape depends on the answer.

**What the code already tells us (verified — do not re-derive):**

- `SCR_AIGroup.SetMaxUnitsToSpawn(n)` is consumed at `CreateUnitEntities` (`SCR_AIGroup.c:1624`) as `Math.Min(slots.Count(), m_iMaxUnitsToSpawn)` and then spawns slots `[0..n-1]`. **First N, deterministic, no randomization** — so count-only respawn always keeps the squad leader and always loses the tail, regardless of who actually died.
- `m_aUnitPrefabSlots` (`:79-80`) is a **public** attribute array, and `SpawnUnits()` (`:2581`) reads it directly at call time.
- `m_bSpawnImmediately` defaults to 1 and `EOnInit` (`:2522`) calls `SpawnUnits()` — so `SetSpawnImmediately(false)` **after** `SpawnEntityPrefab` returns is too late. The static one-shot `SCR_AIGroup.IgnoreSpawning(true)` (`:2185`, consumed and self-cleared at `:2515-2521`) is the documented way to get an unspawned group entity.
- Delayed spawning is per-frame with navmesh back-pressure already: `SpawnGroupMember` returns false and retries next frame while `navmesh.IsTileRequested(pos)` (`:1698`).
- `Event_OnInit` also fires **early and short-staffed** when the engine AI limit is hit (`:1662-1663`).
- `m_bDeleteWhenEmpty` defaults to 1 — the group entity deletes itself when its last member dies.

**Tasks**

1. **T2.1** Throwaway spike (tagged `[OVT-VIRTSPIKE]`, removed before the phase closes) that, for a real faction group prefab: sets `SCR_AIGroup.IgnoreSpawning(true)`, spawns the prefab, confirms **zero** members exist, mutates `m_aUnitPrefabSlots` down to a chosen subset, calls `SetMemberSpawnDelay(150)` then `SpawnUnits()`, and logs each member as it appears.
2. **T2.2** Answer, in writing: (a) does `IgnoreSpawning` reliably suppress the initial spawn and self-clear? (b) is `m_aUnitPrefabSlots` writable from script on a live instance? (c) do exactly the chosen prefabs appear, in slot order? (d) does `GetOnInit()` fire once, after the last one? (e) does the short-staffed AI-limit path fire `Event_OnInit` early, and how is it distinguishable?
3. **T2.3** Confirm the wipe signal: does `GetOnEmpty()` (`:2229`) fire, and does `m_bDeleteWhenEmpty` delete the entity out from under a held reference? Record whether Phase 3 must hold an `EntityID` rather than a pointer.
4. **T2.4** Confirm the death signal: `OVT_OverthrowGameMode.GetOnCharacterKilled()` (invoked from `Scripts/Game/Components/Damage/Modded/SCR_CharacterDamageManagerComponent.c:52`) fires for a **spawned AI member**, with the member entity as `victim`.
5. **T2.5** Pick the spawn strategy and record it in `context.md` with the evidence:
   - **A1 — slot-list surgery + native delayed spawn** (preferred: exact member identity *and* engine frame-spread for free).
   - **A2 — `SetMaxUnitsToSpawn(survivorCount)`** (fallback: count-only fidelity; acceptable only for homogeneous rosters, and the limitation must be written into `api.md`).
   - **A3 — spawn an empty group and add characters per surviving slot** (fallback: full control, more code, core owns the frame-spread).
6. **T2.6** Amend Phase 3's task list in this document before Phase 3 starts if the answer is not A1.

**Acceptance criteria**

- `context.md` records the answers to T2.2-T2.4 and names the chosen strategy with the evidence for it.
- All `[OVT-VIRTSPIKE]` code is removed; `tools/compile-check.sh` exits 0.
- The user has seen the spike output from a play-test session and confirmed the member counts.

---

### Phase 3 — Tick, queues, spawn/despawn, per-member survivors

**Agent:** `component-developer-advanced` — **advanced.** This is the feature's engine room: a central scheduler, two rate-limited queues, entity lifetime across despawn and self-deleting groups, a global death hook, and waypoint-entity ownership. Getting the death accounting or the despawn suppression wrong silently corrupts every record.
**Estimate:** 14-20 h

**Tasks**

1. **T3.1** Implement `Tick()` per §3.6: round-robin slice, `NearestPlayerDistance`, desired-state decision via `OVT_VirtualizationMath`, enqueue with `m_iQueuedOp` guarding against double-queueing, **cancellation** when a queued record's desired state flips before the op runs.
2. **T3.2** Fire `GetOnRecordEvaluated()` per evaluated record with real elapsed seconds from `m_iLastEvaluatedMs`. Nothing in core subscribes — this exists for `movement`.
3. **T3.3** Drain order: despawns (`m_iDespawnOpsPerTick`) before spawns (`m_iSpawnOpsPerTick`); spawn queue ordered nearest-player-first so the closest groups appear first under a backlog.
4. **T3.4** Spawn path, per the Phase 2 verdict: materialize **only living slots**, set `SetMemberSpawnDelay(m_iMemberSpawnDelayMs)`, subscribe `GetOnInit()`, store the live group, and reconcile the actual agent count against the expected survivor count (the AI-limit path can under-deliver) — log a WARNING on mismatch and trust the record, not the world.
5. **T3.5** Build waypoint entities from the record's plan using the centralized helpers on `OVT_OverthrowConfigComponent` (`SpawnPatrolWaypoint` :407, `SpawnDefendWaypoint` :468, `SpawnWaitWaypoint` :485, `GivePatrolWaypoints` :501). **Record every created `AIWaypoint` in `m_aLiveWaypoints`.**
   ⚠️ `SpawnWaitWaypoint(pos, time)` accepts `time` and **never applies it** (:485) — do not rely on the duration until that is fixed; note it in `context.md`.
   ⚠️ `OVT_PatrolType` has only `DEFEND` and `PERIMETER` (:19-22).
6. **T3.6** Despawn path: set `m_bDespawning`, capture surviving liveness back into the record, `RemoveWaypoint` **and** `SCR_EntityHelper.DeleteEntityAndChildren` every entry of `m_aLiveWaypoints` (D9), delete the members, clear `m_SpawnedGroup`, fire `GetOnGroupDespawned(handle, survivors)`, clear `m_bDespawning`.
   ⚠️ `OVT_EntitySpawningAPI.CleanupGroup()` (:379-403) removes waypoints from the group but **does not delete the waypoint entities** — do not reuse it as-is for waypoint cleanup.
7. **T3.7** Death accounting: subscribe once to `OVT_OverthrowGameMode.GetOnCharacterKilled()`; map the victim entity → (handle, slot) through a reverse map built at spawn; call `ReportMemberKilled`. **Ignore every kill while `m_bDespawning`** on that record. Do **not** use `GetOnAgentRemoved()` — it cannot distinguish a death from our own teardown.
8. **T3.8** Wipe path: last living member dies → fire `GetOnGroupWiped(handle)`, delete the live group and its waypoints, remove the record. It never respawns. Handle the case where `m_bDeleteWhenEmpty` already deleted the group entity (hold the group by `EntityID` or null-check on every touch, per T2.3).
9. **T3.9** `OnDelete`: remove the repeating `CallLater`, despawn everything live, clear `s_Instance` (R7).
10. **T3.10** Add a `[Attribute(defvalue: "false")] bool m_bDebugRegisterTestGroup` that, when enabled, registers one virtual group a short distance from the campaign start in `PostGameStart()`. This is the only way the user can play-test core before any consumer exists. Default false, one small guarded block, documented in `context.md` as a dev affordance.
11. **T3.11** Init-tier cases: register a two-member group → `GetGroupCount()` is 1; `ReportMemberKilled` on both → the record is gone and `IsRegistered` is false; `ForceSpawn` on a record with no players nearby still queues.

**Acceptance criteria**

- `tools/compile-check.sh` exits 0; Fast and All both exit 0.
- With `m_bDebugRegisterTestGroup` on, a play-test shows: group absent at distance, members appearing one at a time on approach, all disappearing on withdrawal, and reappearing minus anyone killed.
- Server log shows **no** spawn/despawn thrash when standing at the boundary distance.
- `grep -rn "AIWaypoint" Scripts/Game/GameMode/Virtualization/` shows every creation site paired with a delete site.
- No `GetOnAgentRemoved` anywhere in the feature.

---

### Phase 4 — Ambient spawn-source seam

**Agent:** `component-developer`
**Estimate:** 6-9 h

**Tasks**

1. **T4.1** Create `OVT_AmbientSpawnSourceConfig.c` and `OVT_AmbientSpawnSourceRegistry.c` per §3.5, with the four overridable roll/hook methods and safe defaults. `RollCount()` must respect `RandInt`'s max-exclusive contract and short-circuit `min == max`.
2. **T4.2** Create `OVT_AmbientSpawnSourceInstance.c` — the runtime pairing of config + position + ownerKey + live entity list + queue state.
3. **T4.3** Implement `RegisterAmbientSource` / `UnregisterAmbientSource` / `GetAmbientSourceCount` / `GetAmbientEntities`, and fold ambient sources into the Phase 3 tick with their own cursor and queue entries.
4. **T4.4** Ambient spawn: roll count, roll prefab and position per entity, spawn, call `OnEntitySpawned`, record the entity and its reverse map entry. **Spread across ticks** — a source rolling 20 civilians must not spawn 20 in one frame; cap per tick with the same budget attributes.
5. **T4.5** Ambient despawn: call `OnEntityDespawning`, delete every still-live entity, clear the list and the reverse-map entries. **No state is kept** — the next spawn re-rolls from config.
6. **T4.6** `ReleaseAmbientEntity(entity)`: O(1) reverse-map lookup, remove from the source's list and from the map, return true. The entity is now nobody's business but the caller's. Returns false for a non-ambient entity.
7. **T4.7** Prune dead/deleted entities from source lists each evaluation — a killed civilian must not keep a null slot forever.
8. **T4.8** Init-tier cases (requirements call for this explicitly): a registered source resolves and is counted; a config subclass's overridden `RollCount()` is the one called; `ReleaseAmbientEntity` on an unknown entity returns false.

**Acceptance criteria**

- Fast and All both exit 0 including the new Init cases.
- Core ships **zero** authored ambient sources — `grep -rn "OVT_AmbientSpawnSourceConfig" Configs/` is empty.
- A registered source with 20 entities spawns them over several ticks, verified by timestamped log lines in a play-test.
- Released entities survive the next despawn of their former source (play-test: release one, walk away, walk back — it is still there and was not re-rolled).

---

### Phase 5 — Persistence serializer + round-trip coverage

**Agent:** `component-developer-advanced` — **advanced.** This carries the epic's persistence promise, defines a save format that is append-only forever, and edits the shared round-trip suite that gates the All group.
**Estimate:** 10-14 h

**Tasks**

1. **T5.1** Create `Scripts/Game/Persistence/Serializers/Components/OVT_VirtualizationManagerSerializer.c` with `OVT_PersistedVirtualGroup` per §3.7. Copy the **shape and the header discipline** of `OVT_TownManagerSerializer.c` (why a dedicated record, why parallel arrays, post-load behaviour, idempotency, format note).
   ⚠️ Do **not** copy `docs/features/core/persistence/templates/_OVT_ComponentSerializerTemplate.c` — it declares the overrides with `BaseSerializationSaveContext`/`BaseSerializationLoadContext` and no `notnull`, matching neither the engine base nor any of the 16 shipped serializers. Copy a shipped serializer.
2. **T5.2** Implement `ApplyPersistedVirtualGroups(records, nextHandle)` on the manager: idempotent match-by-handle, restore the handle counter, drop records with unresolvable composition (WARNING, name the key), never spawn directly.
3. **T5.3** `Serialize()` reconciles liveness from any spawned groups before writing, so a save taken mid-firefight is accurate.
4. **T5.4** Fire `GetOnRecordsRestored()` once at the end of the apply.
5. **T5.5** Register in `Configs/Systems/Persistence/Overthrow.conf` in the game-mode `ComponentSerializers` block (:23-64) with a freshly generated GUID continuing the `6B0E7A2x` series. Verify uniqueness across the repo before committing it.
6. **T5.6** Ambient sources are **not** in the payload. Assert it by construction and say so in the serializer header.
7. **T5.7** Add `OVT_TEST_PersistenceRoundTrip_VirtualGroups_SurviveSaveAndReload` to `OVT_TEST_PersistenceRoundTripSuite.c`, using the shared `OVT_TEST_PersistenceRoundTripGate` phase machine (`PHASE_MUTATE_AND_SAVE` … `PHASE_ASSERT`, `TriggerSaveOnce` / `PollSaveSettled` / `RequestSessionReload` / `ReloadInProgress` / `RequireRestoredCampaign`) — copy the case shape from `..._TownControl_SurvivesSaveAndReload` (:1199).
   - **Mutate:** register a group at `OVT_TEST_PersistenceSubject.ResolveFirstTown()`'s location with a distinctive owner key; `ReportMemberKilled` on one slot; move its virtual position by a distinctive offset. Save.
   - **Dirty:** move the position somewhere else and register a second, bogus group.
   - **Assert after re-apply:** the handle still resolves, the owner key matches, the position is the saved one, `GetAliveMemberCount()` is the saved (reduced) count, the specific dead slot is still dead, and `FindGroupsByOwner` finds it.
8. **T5.8** Second round-trip case: a **wiped** group does not come back. Register, kill every member, save, re-apply, assert the handle is unregistered and `FindGroupsByOwner` returns empty.
9. **T5.9** Record the can-fail proof for both cases (e.g. drop the `memberAlive` write from `Serialize()` → the alive assertion goes red; skip the position write → the position assertion goes red). Exact edit in a preamble comment. **No `maxAttempts`.**

**Acceptance criteria**

- `tools/run-tests.sh "{6A6E2A002F53A581}"` (All) exits 0 with both new cases green, and each has a recorded failure proof.
- No group-config change was needed (new cases in an existing suite are collected by the `[Test(suite: …)]` attribute).
- `git diff Configs/Systems/Persistence/Overthrow.conf` shows exactly one added entry with a GUID unique in the repo.
- The serializer file has no `[BaseContainerProps()]` (none of the 16 shipped ones do) and writes `version` first.

---

### Phase 6 — Budget measurement, hardening, API freeze

**Agent:** `component-developer`
**Estimate:** 4-6 h + one play-test round

**Tasks**

1. **T6.1** **Measure** the fast-travel case (G11): with `m_bDebugRegisterTestGroup` extended to register ~40 groups in one town, fast-travel in and record server frame time across the spawn burst and the time to fully populate. Write both numbers, and the attribute values used, into `context.md`. Adjust the defaults if the measurement says so — do not guess first.
2. **T6.2** Restart hardening: start a campaign, quit to menu, start again. Assert no stale `s_Instance`, no orphaned `CallLater`, no error spam (R7).
3. **T6.3** Missing-faction hardening: register with a faction key, then verify the drop-with-WARNING path by loading a save whose records name a key the current config does not define (R4).
4. **T6.4** Backlog hardening: confirm queue cancellation actually fires by driving past a dense area at speed without stopping — nothing should fully spawn (R5).
5. **T6.5** Freeze `api.md`: mark it the contract, note any fidelity limitation the Phase 2 verdict imposed, and list the exact entry points `civilians`, `movement` and `integration` each need.
6. **T6.6** Remove every remaining debug print that is not behind a `LogLevel.VERBOSE` guard.

**Acceptance criteria**

- `context.md` contains the measured frame-time and populate-time numbers with the attribute values used.
- Restart, missing-faction and backlog cases all verified by the user in a play-test with the observations recorded.
- `api.md` is marked frozen and matches the shipped signatures.

---

## 5. Key Technical Decisions

**D1 — Manager, not Controller.** System-wide server-only state coordinating many instances; records are data, not entities. A controller per virtual group would put a world entity behind every virtual group, which is precisely the cost this layer exists to remove. Standard pair + `s_Instance` + `OVT_Global` accessor, per `OVT_DeploymentManagerComponent.c:19-61`.

**D2 — One central tick with rate-limited work queues** *(user-approved; not re-litigated)*. Per-record timers cost O(records) call-queue entries and give no central budget; one loop evaluating a round-robin slice is flat as the count grows and has a single knob. Precedent: the QRF spawn queue (`OVT_QRFControllerComponent.c:30` `m_aSpawnQueue`, drained by `SpawnFromQueue()` :367-390). Member-level spreading is delegated to vanilla's own delayed spawner, which already backs off on navmesh tile requests (`SCR_AIGroup.c:1698`) — so core rate-limits *groups*, the engine rate-limits *members*, and neither duplicates the other.

**D3 — Global default + per-registration override for spawn distance** *(user-approved)*. New operator-editable `virtualizationSpawnDistance` in `Overthrow_Config.json` via `OVT_OverthrowConfigStruct`, defaulting to 1750 to match today's `m_iMilitarySpawnDistance`; each record and ambient source may override with `-1` meaning "use the global". This preserves today's civilian(1000)/military(1750) split when consumers migrate, satisfies issue #100's "very large value keeps everything spawned", and needs **no JIP bitstream change** because it is server-only. `SetDefaults()` runs before the JSON load, so existing operator config files silently gain the default.

**D4 — Composition identity is `(factionKey, groupRegistryName)` with a resolved-prefab fallback.** Faction **indices** are positional across saves and are documented as unsafe to persist (`OVT_DeploymentManagerSerializer.c:20-23`); `m_sFactionKey` is stable. The registry name (`"light_patrol"`) is the authoring-level identity a modder edits. `resolvedPrefab` survives a registry rename; if both fail, the record is dropped with a WARNING rather than resurrected wrong — the same honest outcome `ApplyPersistedDeployment` chose for a renamed deployment config.

**D5 — Native `SCR_AIGroup` delayed spawn, with slot-list surgery for member identity** *(spike-gated — Phase 2)*. `SetMaxUnitsToSpawn(n)` provably spawns slots `[0..n-1]` (`SCR_AIGroup.c:1624`), so count-only respawn always resurrects the squad leader and always drops the tail. Because `m_aUnitPrefabSlots` is a public attribute array read by `SpawnUnits()` at call time, and `SCR_AIGroup.IgnoreSpawning(true)` (`:2185`) suppresses the automatic `EOnInit` spawn, the manager can trim the slot list to exactly the survivors and then call `SpawnUnits()` — exact identity *and* the engine's frame-spread for free. Phase 2 proves or disproves this before Phase 3 commits, with A2/A3 fallbacks named.

**D6 — Dedicated persisted-record classes, parallel arrays, version-first, append-only.** The live record carries transient members and will keep changing; a save format must not. Parallel arrays let a slot whose prefab is gone be dropped without a half-built object. Binary contexts are positional, so field order *is* the format. All three rules are the town serializer's, verbatim (`OVT_TownManagerSerializer.c:1-12, :73`).

**D7 — Deaths come from the game mode's kill invoker, not `GetOnAgentRemoved()`.** `OVT_OverthrowGameMode.GetOnCharacterKilled()` is raised for every character including AI (`Scripts/Game/Components/Damage/Modded/SCR_CharacterDamageManagerComponent.c:52`) and already has three consumers. `GetOnAgentRemoved()` cannot distinguish a death from our own despawn teardown — using it would mark every member of every despawning group dead. Core subscribes once, maps victim → (handle, slot), and ignores anything arriving while that record is `m_bDespawning`.

**D8 — Server-only, no replication, no new comms RPC.** Nothing here has a client half: no `RplProp`, no `Rpc`, no JIP payload, and nothing added to the deprecated `OVT_PlayerCommsComponent`. The server guard goes in `OnPostInit` **before** any collection is allocated, so a client instance holds nothing (`OVT_DeploymentManager.c:64-84`).

**D9 — Core owns waypoint-entity lifetime.** Every spawner in the tree except `OVT_MultiTownPatrolBehaviorDeploymentModule` (:71-84) creates `AIWaypoint` entities and never deletes them — including the shared `OVT_EntitySpawningAPI.CleanupGroup()` (:379-403), which detaches waypoints but leaks the entities. Since core despawns groups repeatedly by design, inheriting that leak would be unbounded. Every waypoint core creates is recorded in `m_aLiveWaypoints` and deleted on despawn. Fixed by construction, not by later cleanup.

**D10 — `int` handles, `string` owner tags.** Handles are stable across save/load (the counter is persisted), so consumers may hold them; owner tags are strings so a new consumer class needs no enum change, no serializer version bump, and no core edit — the Economy 2.0 seam (§3.8). `FindGroupsByOwner` is the preferred reclaim path because it works even when the consumer's own save is older than core's.

**D11 — Hysteresis on the despawn threshold.** A player at exactly the spawn distance would otherwise thrash a group every tick. Despawn distance is `spawnDistance × m_fDespawnHysteresis` (default 1.15). It is a config attribute, and the predicate is a pure function pinned in the Logic tier.

**D12 — `GetOnRecordsRestored()` removes a whole ordering-bug class.** Component serializers all deserialize at `AFTER_ENTITY_FINALIZE` and their relative order is not something a consumer should have to reason about. Core fires one invoker after its records are applied; consumers reclaim there and never race. Cheap now, and it is the difference between `integration` being straightforward and being a debugging exercise.

---

## 6. Definition of Done

An evaluator with **no implementation context** should be able to verify every item.

### Functional Criteria

- **F1** A registered virtual group with no player nearby has **no entities in the world**, and `IsSpawned(handle)` is false.
- **F2** When a player comes within the configured distance, the group materializes; its members appear **progressively**, not all in one frame.
- **F3** When the last player leaves the area, the group's members **and all of its waypoint entities** are deleted; `IsSpawned(handle)` is false again.
- **F4** Killing 3 of an 8-member group, then walking away and back, produces a group of **5**, and the 3 dead ones do not return.
- **F5** Killing **every** member removes the record: `IsRegistered(handle)` is false, `FindGroupsByOwner` no longer finds it, and it never respawns.
- **F6** Setting `virtualizationSpawnDistance` in `$profile:Overthrow_Config.json` to a very large value keeps every registered group spawned permanently; setting it to a small value despawns everything not immediately adjacent. The change takes effect on the next campaign start with no code edit.
- **F7** A record registered with a per-registration override uses the override, not the global — verifiable by two records at the same position with different overrides behaving differently.
- **F8** Save with a partially-wiped group, dirty the state, re-apply: the group comes back with its **exact** surviving member count, its saved position, and its owner key.
- **F9** `FindGroupsByOwner(ownerSystem, ownerKey)` returns the same handles after a save/re-apply cycle as before it.
- **F10** A registered **ambient source** spawns its entities on approach and deletes them on withdrawal; the next approach produces a **freshly rolled** set (different prefabs/positions), and nothing about them appears in any save file.
- **F11** An entity passed to `ReleaseAmbientEntity()` survives the next despawn of its former source and is not re-rolled.
- **F12** Standing at exactly the configured spawn distance produces **no** repeated spawn/despawn cycling in the server log.
- **F13** Fast-travelling into an area with many registered groups produces **no visible frame hitch**; groups fill in over several seconds, nearest first.

### Quality Criteria

- **Q1** `tools/compile-check.sh` exits **0** with no output.
- **Q2** `tools/run-tests.sh "{6A6E29FF47ECB840}"` (Fast) exits **0** and `tools/run-tests.sh "{6A6E2A002F53A581}"` (All) exits **0**.
- **Q3** Every new test case has a recorded proof that it can fail — the exact edit used, in a preamble comment. **No `maxAttempts` anywhere.**
- **Q4** Nothing in the feature replicates: `grep -rn "Rpc\|RplProp\|Replication.Bump\|RplSave\|RplLoad" Scripts/Game/GameMode/Virtualization/` is empty, and `OVT_PlayerCommsComponent.c` is unchanged.
- **Q5** No waypoint leak: every `AIWaypoint` creation site in the feature is paired with a deletion site, and a despawn/respawn cycle repeated 20 times leaves no growing entity count.
- **Q6** No persisted faction **index** anywhere in the save format — `grep -n "factionIndex\|m_iFaction" ` on the serializer is empty.
- **Q7** Core never entity-tracks spawned members: `grep -rn "OVT_PersistenceTracking" Scripts/Game/GameMode/Virtualization/` is empty.
- **Q8** No `GetOnAgentRemoved` in the feature (D7).
- **Q9** The Logic-tier file contains no manager-accessor or game-mode-getter identifier, in code **or** comments (the tier's grep rule).
- **Q10** Core ships **no** authored ambient content: `grep -rn "OVT_AmbientSpawnSourceConfig" Configs/` is empty.
- **Q11** Debug affordances are off by default (`m_bDebugRegisterTestGroup` defaults false) and all spike instrumentation is removed.
- **Q12** `docs/features/virtualization/core/api.md` exists, is marked frozen, and its signatures match the shipped code.

### Integration Criteria

- **I1 No consumer migrated.** `OVT_TownController.c`, `OVT_DeploymentComponent.c`, `OVT_BasePatrolUpgrade.c` and `OVT_OccupyingFactionManager.c` are **unchanged** — their four ad-hoc `PlayerInRange` sites still use `m_iCivilianSpawnDistance` / `m_iMilitarySpawnDistance`. Migration is `civilians` and `integration`, not here.
- **I2 Config seam.** `OVT_OverthrowConfigStruct` gained exactly one field; `m_iMilitarySpawnDistance` and `m_iCivilianSpawnDistance` (`OVT_OverthrowConfigComponent.c:159,162`) still exist with their values; the JIP `RplSave` block (:554-586) and `CONFIG_STREAM_VERSION` are unchanged.
- **I3 Game mode seam.** `OVT_OverthrowGameMode` gained one field, one `Init` block in `EOnInit` and one `PostGameStart` block in `DoStartGame()`, in the existing style and after the Deployment manager.
- **I4 Persistence seam.** Exactly one entry added to `Configs/Systems/Persistence/Overthrow.conf`; no existing serializer changed; no existing save-format field reordered.
- **I5 Movement seam present.** `GetOnRecordEvaluated()`, `SetVirtualPosition()`, `GetWaypointPlan()` and `m_fLegProgress` exist and are documented in `api.md` as `movement`'s entry points, with nothing in core advancing them.
- **I6 Test-tier seams.** No group config changed (new cases join existing suites via `[Test(suite: …)]`).

### Verification Method

**Automated — run from the repo root, in order:**

1. `tools/compile-check.sh` → expect exit **0**, no output.
2. `tools/run-tests.sh "{6A6E29FF47ECB840}"` → expect exit **0** (Fast).
3. `tools/run-tests.sh "{6A6E2A002F53A581}"` → expect exit **0** (All, includes the persistence round trip).
4. `git diff --stat Scripts/Game/Controllers/ Scripts/Game/GameMode/Deployments/` → expect **empty** (I1).
5. `git diff Configs/Systems/Persistence/Overthrow.conf` → expect exactly one added `OVT_VirtualizationManagerSerializer "{…}" { }` entry.
6. `grep -rn "Rpc\|RplProp\|OVT_PersistenceTracking\|GetOnAgentRemoved" "Scripts/Game/GameMode/Virtualization/"` → expect **empty** (Q4, Q7, Q8).

**Manual — solo play-test.** Core has no consumers, so testing needs the dev affordance: set `m_bDebugRegisterTestGroup` to true on the manager component in the game-mode prefab (Workbench), start a campaign, and run:

1. Start the campaign and stay far from the debug group's position. **Expect:** no AI there; the log shows the record registered and not spawned. → F1
2. Walk/drive toward it. **Expect:** members appear **one at a time**, roughly `m_iMemberSpawnDelayMs` apart, not all in one frame. → F2
3. Walk away past the despawn distance. **Expect:** every member **and every waypoint entity** is gone. → F3
4. Return, kill 3 members, walk away, come back. **Expect:** exactly the original count minus 3, and the 3 you killed are not among them. → F4
5. Kill the whole group, walk away, come back. **Expect:** nothing spawns, ever. The log shows the record removed. → F5
6. Stand at exactly the configured spawn distance for 60 s. **Expect:** the log shows **no** repeated spawn/despawn pairs. → F12
7. Edit `$profile:Overthrow_Config.json`, set `virtualizationSpawnDistance` to `100000`, restart the campaign. **Expect:** the group is spawned from the moment the campaign starts, anywhere on the map. Set it to `50` and restart. **Expect:** it despawns as soon as you step back. → F6
8. Extend the debug registration to ~40 groups in one town and **fast-travel in**. **Expect:** no visible hitch; groups fill in over seconds, closest first. Record the server frame time. → F13, G11
9. Drive past that town at speed **without stopping**. **Expect:** the log shows queued spawns being **cancelled**, and most groups never materialize. → F13
10. Register an ambient source (debug affordance or Script Console — see `docs/features/dev-ops/` on live-session code execution), approach, withdraw, approach again. **Expect:** the second set differs from the first. → F10
11. Call `ReleaseAmbientEntity()` on one of them, withdraw, return. **Expect:** that one entity is still there, unchanged, and was not re-rolled. → F11
12. Kill 2 of an 8-member group, take a save (in-game save), quit to the main menu, **Continue**. **Expect:** the group is registered with 6 living members at its saved position; approaching materializes exactly 6. → F8, F9
13. Start a campaign, quit to menu, start a second campaign. **Expect:** no error spam, no duplicated ticking, groups behave identically in the second session. → R7

*(Step 12 is the only path that exercises the real quit-and-continue flow — the automated round-trip suite covers save→dirty→re-apply in-session only, and cannot cover this.)*

---

## 7. Testing Strategy

**The automated spine covers logic, wiring and the save format. It does not cover the thing this feature is mostly about — entities appearing and disappearing in a live world.** Plan the manual half accordingly.

### Logic tier — `TestSuites/Logic/OVT_TEST_Logic_Virtualization.c` (Fast)

World-free assertions on `OVT_VirtualizationMath`: effective-distance resolution (override −1 / 0 / huge), hysteresis, alive counting, wipe detection, survivor-prefab selection preserving slot order, round-robin slice arithmetic (wraparound, `sliceSize > count`, `count == 0`), and both spawn/despawn predicates either side of their thresholds. This is where the maths gets pinned because it is the only tier that cannot flake.

⚠️ The tier's grep rule bans the manager-accessor and game-mode-getter identifiers **anywhere** in the directory, comments included.

### Init tier — additions to `OVT_TEST_InitSuite.c` (Fast)

Manager resolves through `OVT_Global`; empty-registry preconditions; register → count → unregister; unknown faction key and unknown group name both return −1; `ReportMemberKilled` on the last member removes the record; ambient source registration resolves and a subclass's overridden roll is the one called; `ReleaseAmbientEntity` on a non-ambient entity returns false.

Fail-proof for the resolution case: remove the component from the game-mode prefab, observe exit 1, restore.

### Persistence round trip — additions to `OVT_TEST_PersistenceRoundTripSuite.c` (All)

Two cases on the shared `OVT_TEST_PersistenceRoundTripGate` phase machine: partially-wiped group survives save → dirty → re-apply with its exact survivors and position; wiped group does not come back. Every assertion goes through the public manager API — the suite must not know whether the serializer exists.

### Every new case must be proven able to fail once

The exact edit that made it go red is recorded in a preamble comment (matching `OVT_TEST_Logic_Skills.c`). A test that needs retries is a bug in the test; `maxAttempts` is banned.

### Not automatable, and why

| Area | Why manual |
|---|---|
| Spawn/despawn in a live world | Needs a real world, a real player position and real AI spawning |
| Frame-hitch behaviour under fast travel | A performance measurement, not an assertion |
| Waypoint-entity leaks | Needs repeated live cycles and an entity count over time |
| The quit-and-continue path | `SaveGameManager.Load`'s world transition restarts the autotest harness |
| Multiplayer / JIP | Uncovered by the spine; core is server-only, so risk is low, but "low" is not "verified" |
| The `SCR_AIGroup` partial-spawn seam | Phase 2's spike is a play-test, deliberately |

### Manual procedure

The numbered steps in §6 Verification Method **are** the manual procedure. Play-test gates by phase: Phase 2 → the spike session; Phase 3 → steps 1-6, 13; Phase 4 → steps 10, 11; Phase 5 → step 12; Phase 6 → steps 7, 8, 9.

---

## 8. Dependencies

**Hard preconditions (all satisfied today):**

- **Vanilla persistence (shipped v1.4.0)** — `ScriptedComponentSerializer` + the `ComponentSerializers` block in `Configs/Systems/Persistence/Overthrow.conf`; 16 working precedents in `Scripts/Game/Persistence/Serializers/Components/`.
- **`OVT_Faction` group registries** — `OVT_FactionGroupRegistry` (`OVT_Faction.c:162-236`) and the facade `GetGroupPrefabByName()` (:498); authored in `Configs/Factions/*_OverthrowData.conf`.
- **`OVT_OverthrowConfigStruct` / `LoadConfig()`** (`OVT_OverthrowConfigComponent.c:24-66, :198-231`) for the operator-editable distance.
- **`OVT_OverthrowGameMode.GetOnCharacterKilled()`** (:164) — the death seam, already consumed by recruits and the civilian-death modifiers.
- **Waypoint helpers on `OVT_OverthrowConfigComponent`** (:401-543).
- **Autotest harness** — `tools/run-tests.sh`, suites under `Scripts/Game/Tests/TestSuites/`.

**User-side (Workbench, interactive):** adding `OVT_VirtualizationManagerComponent` to `Prefabs/GameMode/OVT_OverthrowGameMode.et` (T1.8), and toggling `m_bDebugRegisterTestGroup` for play-tests.

**Downstream (this feature blocks all of them):** `civilians` (ambient seam), `movement` (records + evaluated invoker), `integration` (tracked-group API + `GetOnRecordsRestored`), `base-defense-migration`.

**Explicitly not depended on:** the old `virtualization` branch (design lessons only), EPF (nothing EPF-shaped anywhere), any client-side code.

---

## 9. Risks & Mitigation

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| **R1** | **The partial-spawn seam does not behave as read.** `m_aUnitPrefabSlots` may not be script-writable on a live instance, or `IgnoreSpawning` may not suppress reliably — and `SetMaxUnitsToSpawn` provably takes the **first N** slots, so the fallback loses member identity (the squad leader always survives). | Medium | G3 (dead-stay-dead) degrades from per-member to per-count | **Phase 2 is a gate**, not a task: prove it before Phase 3 commits. Three strategies named (A1 slot surgery / A2 count-only / A3 manual per-slot spawn) with the chosen one and its evidence recorded in `context.md`. If A2 is forced, the fidelity limit goes into `api.md` so `integration` plans around it. |
| **R2** | **Deserialize/Init ordering.** Component serializers run at `AFTER_ENTITY_FINALIZE` — after `EOnInit` — and the order between two serializers on the same entity is not a contract. A consumer that reclaims during its own deserialize may find core's records not yet applied. | Medium | Deployments come back without their groups | Records are **self-contained** (owner tag + full composition), so core restores without consulting anyone. `GetOnRecordsRestored()` (D12) gives consumers one deterministic reclaim point. Records are applied but **never spawned** during restore; the tick reconciles. The `ApplyPersistedDeployment` ordering lesson (:100-153) — flags before init — is followed. |
| **R3** | **A faction mod is removed** and a saved record names a key that no longer resolves. | Medium | Records silently vanish, or worse, resurrect as the wrong faction | Three-step resolution: key → registry name → `resolvedPrefab` fallback. If all fail, **drop with a WARNING naming the key** rather than guess — the honest outcome `ApplyPersistedDeployment` already chose for a renamed config. Never fall back to a faction index (they are positional). Verified in Phase 6 T6.3. |
| **R4** | **Queue starvation / backlog.** A fast-travel into a dense area queues dozens of spawns at 1/tick; the player stands in an empty town for a minute. | Medium-High | Feels broken even though it is working | Despawns drain first (they free budget); the spawn queue is ordered **nearest-first**; queued ops **cancel** when the desired state flips. Budget is four attributes, tunable without a rebuild. Phase 6 T6.1 **measures** the real numbers before the defaults are frozen. |
| **R5** | **Boundary thrash.** A player parked at exactly the spawn distance cycles a group every tick. | High without mitigation | Log spam, AI popping, wasted budget | `m_fDespawnHysteresis` (D11), default 1.15, and the predicate is a pure Logic-tier-pinned function. Verified by §6 step 6. |
| **R6** | **Death accounting corruption.** If our own despawn deletions are counted as deaths, every despawn marks the whole group dead and the record is destroyed. This is the failure mode that silently deletes campaign content. | Medium | Catastrophic and silent — groups vanish permanently | `GetOnAgentRemoved` is **banned** (D7, Q8). Deaths come from the game-mode kill invoker only, and every kill arriving while `m_bDespawning` is ignored. §6 step 3→4 is the direct check: despawn 8, return, expect 8 — not 0. |
| **R7** | **`CallLater` and `s_Instance` outlive a game-mode restart.** Nothing else in the codebase cleans these up; a stale static or an orphaned repeating call ticks against a dead manager. | Medium | Errors on the second campaign of a session | `OnDelete` removes the `CallLater`, despawns everything live, and clears `s_Instance`. `GetInstance()` re-resolves lazily. Verified by §6 step 13. |
| **R8** | **Waypoint-entity leak** inherited from the shared helpers — `CleanupGroup()` detaches waypoints without deleting them, and core despawns constantly. | High if not designed against | Unbounded entity growth over a long session | D9: every created waypoint is recorded in `m_aLiveWaypoints` and deleted on despawn; `CleanupGroup()` is explicitly not reused for waypoints. Q5 makes it a reviewable grep plus a 20-cycle play-test. |
| **R9** | **API churn after siblings start.** Four features plan against §3.4; a change after Phase 1 breaks all of them. | Medium | Rework across the epic | `api.md` lands in Phase 1 and is frozen in Phase 6; any change is recorded in `context.md` as breaking. The API is deliberately small and free of behaviour, which is what makes freezing it realistic. |
| **R10** | **Nothing consumes core**, so bugs stay invisible until `integration`. | High (by design — core ships seams) | Latent defects surface late and expensively | The `m_bDebugRegisterTestGroup` affordance makes core play-testable standalone (§6 steps 1-9), and `civilians` is deliberately sequenced next to exercise the ambient half early. |
| **R11** | **`SCR_AIGroup` self-deletes when empty** (`m_bDeleteWhenEmpty` default 1) under a held pointer. | Medium | Null dereference on the wipe path | Phase 2 T2.3 confirms the behaviour; Phase 3 T3.8 holds the group by `EntityID` or null-checks every touch. |
| **R12** | **A vanilla update changes the partial-spawn internals** (`CreateUnitEntities`, `IgnoreSpawning`, `m_aUnitPrefabSlots`). | Low per update, certain eventually | Groups spawn full-strength or not at all | Every vanilla line this design leans on is cited by file:line in `context.md` as an update-check checklist, and the survivor-count reconciliation in T3.4 turns a silent regression into a logged WARNING. |

---

## Agent Routing Summary

| Phase | Agent | Advanced? |
|---|---|---|
| 1 — Records, API, manager scaffold, config field | `component-developer` | no |
| 2 — Partial-spawn validation spike (gate) | `component-developer` | no |
| 3 — Tick, queues, spawn/despawn, per-member survivors | `component-developer-advanced` | **yes** — central scheduler, two queues, entity lifetime across self-deleting groups, global death hook, waypoint ownership |
| 4 — Ambient spawn-source seam | `component-developer` | no |
| 5 — Persistence serializer + round-trip coverage | `component-developer-advanced` | **yes** — defines an append-only save format and edits the shared round-trip suite that gates the All group |
| 6 — Budget measurement, hardening, API freeze | `component-developer` | no |

**Skills to activate:** `enforcescript-patterns` (all phases), `overthrow-architecture` (1, 3, 4, 5), `workbench-workflow` (2, 3, 6).

**No `ui-developer` or `network-specialist` work exists in this feature** — core is server-only with no client surface (D8).
