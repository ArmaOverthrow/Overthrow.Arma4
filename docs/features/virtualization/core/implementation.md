# Virtualization Core — Implementation Plan

**Status:** Ready for Review
**Started:** 2026-08-16
**Completed:** 2026-08-17 (all 6 phases; user play-tests tracked in context.md)
**Last Updated:** 2026-08-17

**Epic:** `virtualization` (feature #1 of 5 — see `docs/features/virtualization/epic-overview.md`)
**Requirements:** `docs/features/virtualization/core/requirements.md` (authoritative for scope)
**Addresses:** GitHub issue **#100**, rebuilt on vanilla persistence (the EPF `virtualization` branch is reference material only)

> **Revision 2 (2026-08-14): replanned around the Arma Reforger 1.8 native AI lifecycle system.**
> Reforger 1.8.0.10 shipped engine-level group virtualization: `SCR_EAIGroupLifecyclePolicy.ProximityDriven`
> (per-group proximity spawn/despawn via `ObserversSystem`), a budgeted importance-tiered spawn queue
> (`ChimeraAIWorld.EnqueueSpawnRequest` + `SCR_EAISpawnImportance`), engine dormancy with survivor counts
> (`SetDormantCounts` / `GetDormantAliveCount`), and vanilla persistence of dormant groups
> (`SCR_AIGroupSerializer`). This is most of what Revision 1 planned to hand-build. **Decision (user-approved
> 2026-08-14): adopt as a hybrid** — a thin Overthrow registry/config/API layer over the engine lifecycle —
> **with slot-accurate dead-member truth**: a per-slot alive mask owned by core, enforced through an
> `ExpandOneMember` override on Overthrow's already-modded `SCR_AIGroup` (see D2; the user initially
> accepted vanilla's count-based semantics, then revised to slot-accurate the same day).
> Revision 1's hand-rolled tick/queue/serializer design is in git history; see
> `docs/reforger/1.8.0.10-changes.md` for the full 1.8 findings.

> **Phase 1 verdict amendments (2026-08-16 — spike + play-test + engine-source investigation; evidence in `context.md`):**
> 1. **Persistence tracking (§3.3 step 6, §3.6, D8):** vanilla's `AIGroup` class rule WOULD auto-track runtime-spawned groups, but Overthrow's BUG-118 fix untracks every group unconditionally (`Modded/SCR_AIGroup.c:30-35`) and `Overthrow.conf:77-88` stamps `SelfSpawn 0` on AIGroup/AIUnit/AIWaypoint. Runtime-confirmed (`IsTracked=no` after retry-queue drain). Phase 3 must build BOTH a per-group untrack exemption AND a self-spawn path for registered groups only; `OVT_PersistenceTracking.MarkForSelfSpawn` is banned (save-corrupting, BUG-116).
> 2. **D2(b) broadened:** the count-corruption vector is not just budget under-fill — **any despawn during an in-progress refill records not-yet-spawned slots as dead** (`SCR_AIGroup.c:2876`), the ratchet is permanent (refill capacity `totalSlots - dormantDead`, `:2726-2729`; nothing engine-side re-corrects), and refill kills the roster TAIL first (`slotIndex = aliveCount`, `:2731`). Observed live: 6→2 with zero kills. The mask + post-despawn `SetDormantCounts` re-assertion is mandatory for correctness, not defensive.
> 3. **Eliminate-when-reached deferred:** `SetEliminateWhenReached(true)` + `veryNearBlockDist` (default 150 m) can delete a dormant group entity outright when any observer closes within 150 m (`SCR_AIGroup.c:3038-3059`) — and observers include **non-players** (deploy-point preload MP observer `SCR_SpawnRequestComponent.c:541`, cameras, optics far-observer). Registration must NOT stamp it unconditionally; enable it only once the mask reports the group wiped (or subscribe + re-verify against the mask before honouring the signal).
> 4. **Observer semantics:** "nearby" = `HasObserverInRange` from the group entity origin, linear metres, covering cameras + fixed MP inserts — core must never mix this with player-distance loops, and consumers must know campaign-start surroundings may never go dormant (parked deploy-point observer).
> 5. **Engine purge hole:** dormant groups never purge queued spawn requests when their observer leaves (`:2869-2870` early-out precedes the purge; LifecycleTick re-enqueues 1/s with no already-queued guard) — mid-fill despawns WILL happen in the wild; core's re-assertion (2) is the correction.
> 6. **Test-world green light (T1.4):** the autotest world runs the real `ChimeraAIWorld` queue + ObserversSystem; unobserved ProximityDriven groups stay memberless → Init-tier registration cases are safe, including queue-semantics assertions.

---

## 1. Executive Summary

Overthrow has four separate, incompatible answers to "when should this AI exist?" — base-upgrades value banking, deployments proximity toggling, radio-tower garrison spawn/despawn, and the town civilian spawner. Each re-implements `PlayerInRange` polling, none of them remembers who died, and none of them survives a save with its group state intact.

As of Reforger 1.8, the **engine** answers that question natively. A group given `SetLifecyclePolicy(ProximityDriven, spawnDist, despawnDist)` materialises when an observer comes in range and goes **dormant** (member characters deleted, group entity kept) when everyone leaves — through a world-scoped, importance-tiered, frametime-throttled spawn queue with navmesh back-pressure, anti-pop-in guards, held-member protection, and elimination semantics ("dormant alive = 0 never respawns"). Vanilla's own `SCR_AIGroupSerializer` persists the dormant state. Conflict's 1.8 ambient patrols run exactly this pattern (`SCR_AmbientPatrolSystem.c:140`).

This feature therefore builds a **thin layer, not an engine**: `OVT_VirtualizationManagerComponent`, a server-only Manager on the game mode that owns:

1. **The registry and consumer API.** Consumers register a group by `(factionKey, groupName)` + position + waypoint plan + owner tag; core resolves the composition, spawns the group entity *unspawned* (`SCR_AIGroup.IgnoreSpawning`), stamps policy, distances and importance, and hands back an `int` handle. `FindGroupsByOwner` reclaims handles after a load. This API is the epic's contract — `civilians`, `movement`, `integration` and `base-defense-migration` all program against it.
2. **The config surface.** Spawn distance is server-configurable in `Overthrow_Config.json` with a per-registration override; importance defaults are Overthrow policy (hostile campaign AI must not sit at vanilla's LOW tier — see D4).
3. **Slot-accurate survivor truth and wipe bookkeeping.** The engine tracks survivor *counts*; core keeps a per-slot alive mask, records deaths by slot, and enforces identity-accurate refill through an `ExpandOneMember` override on Overthrow's already-modded `SCR_AIGroup` (D2). The engine refuses to respawn a group whose dormant-alive count is 0; core turns the engine's signals (`GetOnEliminatedWhenReached`, dormant-aware empty handling) into record removal and a consumer-facing `OnGroupWiped`.
4. **The ambient spawn-source seam.** Config-declared, one-off, non-persisted spawning (town civilians, later parked vehicles) with ownership transfer — still Overthrow code, but proximity-checked through the engine's `ObserversSystem` instead of hand-rolled player loops.

**What core no longer builds** (engine-owned since 1.8): the proximity tick, the rate-limited spawn/despawn queues, frame-spread member spawning, hysteresis/anti-thrash, budget arbitration, dormant survivor *counting* (core refines it to per-slot truth — D2), and the group-state save format.

**Core ships seams, not consumers.** No town wiring, no civilian configs, no deployment migration, no virtual movement.

---

## 2. Goals

### Primary

- **G1** A single server-side registry owns AI group registration: create, query, reclaim, destroy — with no consumer touching engine lifecycle calls directly.
- **G2** Groups spawn when a player comes within the configured distance and despawn when none is, with **no frame hitch** when a player fast-travels into a dense area — delegated to the engine queue, **verified** in Overthrow's world, not assumed (Phase 1 gate).
- **G3** **Dead members stay dead, slot-accurate.** A group that lost 3 of 8 comes back with exactly its 5 surviving slots (roles and loadouts preserved), across despawn *and* across save/load. A group that lost all 8 is removed and never returns. (D2)
- **G4** Spawn distance is **server-configurable** in `Overthrow_Config.json` (not a code constant), with a per-registration override; a very large value keeps everything spawned — issue #100's server-owner ask.
- **G5** Registered groups round-trip through persistence — group state via vanilla's `SCR_AIGroupSerializer` (already connected through `Common.conf`), registry bookkeeping via a small Overthrow serializer — gated by Persistence-tier tests proven able to fail.
- **G6** Ambient spawn sources are declarative and modder-extendable via config classes, non-persisted, and support ownership transfer.
- **G7** Registered hostile groups carry an explicit `SCR_EAISpawnImportance` so campaign AI is not budget-starved at vanilla's default LOW tier (50% cap, first evicted).

### Secondary

- **G8** The registration API is stable enough that the four downstream features can be planned against it without core changing shape.
- **G9** Group position and waypoints are exposed as mutation points so `movement` is a tick strategy over dormant group entities, not a fork of core.
- **G10** Core never leaks waypoint entities — the defect present in every other spawner in the tree (see D6).
- **G11** The Economy 2.0 extensibility seam is documented and nothing agent-specific is built (§3.7).
- **G12** The cost of many dormant group entities (memory, replication) is **measured** at realistic campaign scale, not assumed (Phase 1 probe, Phase 6 re-measure).

---

## 3. Architecture Overview

### 3.1 Division of labour — engine vs Overthrow

```
SERVER ONLY — nothing Overthrow adds here replicates or has a client half.
(The SCR_AIGroup entity itself replicates as vanilla always has; dormant groups are
lightweight — no member characters exist while dormant.)

ENGINE (Reforger 1.8, verified in the 1.8.0.10 reference tree)
├─ SCR_EAIGroupLifecyclePolicy.ProximityDriven   per-group self-managed lifecycle
│    SetLifecyclePolicy(policy, spawnDist, despawnDist, veryNearBlockDist)  SCR_AIGroup.c:2915
│    defaults 600/800/150 m (:124-126); staggered coarse-timer ticks; pop-in guard (:3038)
├─ ObserversSystem                                observer positions (cameras, players, MP)
│    HasObserverInRange(range)  ChimeraAIGroup.c:24 · HasObserverWithinRangeSq  ObserversSystem.c:70
├─ Budgeted spawn queue                           ChimeraAIWorld.EnqueueSpawnRequest (:19)
│    importance tiers LOW 0.50 … CRITICAL 1.00 (SCR_EAISpawnImportance.c); one member per
│    dispatch; observer re-check at dequeue; PurgeSpawnRequestsForGroup on despawn (:21);
│    navmesh back-pressure with 30-attempt spawn-anyway cap
├─ Dormancy                                       DespawnMembers  SCR_AIGroup.c:2864
│    members deleted, group entity + counts kept; SetDormantCounts(alive, dead);
│    dormantAlive==0 ⇒ eliminated, RequestSpawn refuses (:2687);
│    SetEliminateWhenReached (:2963) deletes the record when a player reaches an empty group;
│    HasHeldMember (:2801) blocks despawn while a member is in a vehicle / player-engaged
└─ Vanilla persistence                            SCR_AIGroupSerializer
     faction, waypoints (as persisted entities), dormant alive/dead counts (:161-170, :351-361);
     live members persist as full characters (AIUnit.conf → Character.conf);
     connected via Common.conf (:93 AI group, :24 AIGroup collection) which Overthrow.conf inherits

OVERTHROW (this feature)
├─ OVT_VirtualizationManagerComponent            Manager on OVT_OverthrowGameMode
│    ├─ registry: handle → OVT_VirtualGroupRecord (owner tags, composition identity, group ref)
│    ├─ RegisterGroup / UnregisterGroup / FindGroupsByOwner / queries  (§3.3 — the epic's contract)
│    ├─ composition resolution: (factionKey, groupName) → prefab via OVT_Faction registries
│    ├─ config: virtualizationSpawnDistance global + per-registration override + importance
│    ├─ slot-accurate survivor mask: per-slot alive truth, death accounting by slot, refill
│    │    via modded SCR_AIGroup.ExpandOneMember override; corrects engine dormant counts (D2)
│    ├─ wipe bookkeeping: engine elimination signals → record removal → OnGroupWiped
│    ├─ waypoint-entity ownership: plan → AIWaypoint entities at registration, deleted on unregister
│    └─ ambient spawn sources: own light tick over ObserversSystem (§3.4) — the only tick core owns
├─ OVT_VirtualizationManagerSerializer           registry bookkeeping only (§3.6)
│    handles, owner tags, group-UUID relink; group STATE is vanilla's job
└─ Overthrow_Config.json                         virtualizationSpawnDistance (operator-editable)
```

### 3.2 Manager vs Controller — decided

Manager. System-wide server-only registry coordinating many groups; standard pair + `s_Instance` + `OVT_Global.GetVirtualization()`, per `OVT_DeploymentManagerComponent.c:19-61`. The *per-group* lifecycle state that Revision 1 kept in records now lives where the engine keeps it — on the `SCR_AIGroup` entity itself, which survives dormancy precisely to be that record (`SCR_AIGroup.c:2860-2863`).

### 3.3 The record class and registration API — the epic's contract

**This is the most important artifact in this feature.** Four downstream features program against it; treat any change after Phase 2 as a breaking change and record it in `context.md`.

`Scripts/Game/GameMode/Virtualization/OVT_VirtualGroupRecord.c`:

```c
enum OVT_EVirtualWaypointType
{
    MOVE,
    PATROL,
    WAIT,
    DEFEND,
    CYCLE
}

//! Registration-time INPUT ONLY (not persisted state — the built AIWaypoint entities are the
//! persistent truth, via vanilla's AIWaypoint.conf). Core turns this into real waypoint
//! entities attached to the group at registration; they survive dormancy with the group.
class OVT_VirtualWaypointPlan
{
    ref array<vector> m_aPositions = {};
    ref array<int> m_aTypes = {};       //!< OVT_EVirtualWaypointType, parallel to m_aPositions
    ref array<float> m_aParams = {};    //!< per-type parameter (wait seconds, patrol radius), parallel
    bool m_bCycle;
}

//! Registry bookkeeping. Lifecycle state (dormant counts, position, spawned members) lives on
//! the SCR_AIGroup entity — the engine's durable record. This class only carries what the
//! engine does not know: who owns the group and how it was composed.
class OVT_VirtualGroupRecord
{
    int m_iHandle;                      //!< stable identity, survives save/load
    string m_sOwnerSystem;              //!< "deployment", "tower_garrison", "economy_delivery", ...
    string m_sOwnerKey;                 //!< consumer-defined identity used to reclaim after load

    string m_sFactionKey;               //!< "USSR" — NEVER the faction index (D3)
    string m_sGroupRegistryName;        //!< "light_patrol"
    ResourceName m_rResolvedPrefab;     //!< fallback if the registry entry is gone

    int m_iSpawnDistanceOverride = -1;  //!< -1 = use the global config value
    int m_eImportance;                  //!< SCR_EAISpawnImportance stamped at registration (D4)

    ref array<int> m_aSlotAlive = {};   //!< 1/0 per prefab roster slot, captured at registration.
                                        //!< Core's AUTHORITATIVE survivor truth (D2) — persisted,
                                        //!< pushed to the modded group, corrects engine counts.

    SCR_AIGroup m_Group;                //!< the engine-side record; null only between wipe and cleanup
    ref array<AIWaypoint> m_aOwnedWaypoints = {};  //!< created from the plan; core deletes on unregister (D6)
}
```

```c
// ============ TRACKED GROUPS (persisted, engine-lifecycle) ============

//! Creates a group entity (unspawned), stamps ProximityDriven policy + distances + importance,
//! builds waypoint entities from the plan, registers for persistence, returns a handle.
//! Returns -1 on failure (unresolvable composition, not server, manager not initialised).
//! \param importance  SCR_EAISpawnImportance; -1 = Overthrow's configured default (NORMAL — D4)
int RegisterGroup(string ownerSystem, string ownerKey, string factionKey, string groupName,
                  vector position, OVT_VirtualWaypointPlan plan = null,
                  int spawnDistanceOverride = -1, int importance = -1);

//! Deletes any live members, the group entity and its owned waypoints, removes the record.
//! Idempotent; false if the handle is unknown.
bool UnregisterGroup(int handle);

bool IsRegistered(int handle);
int GetGroupCount();
OVT_VirtualGroupRecord GetRecord(int handle);   //!< server-side read access

//! Reclaim seam. After a load, a consumer calls this with the same (ownerSystem, ownerKey) it
//! registered with and gets its handles back without having persisted them itself.
array<int> FindGroupsByOwner(string ownerSystem, string ownerKey);
array<int> FindGroupsBySystem(string ownerSystem);

// -- state (thin wrappers over the group entity; safe while dormant) --
SCR_AIGroup GetGroup(int handle);               //!< the engine record; exists dormant or spawned
bool   IsSpawned(int handle);                   //!< spawned members exist (== !IsDormant && agents > 0)
int    GetAliveMemberCount(int handle);         //!< agents when spawned, GetDormantAliveCount when dormant
int    GetMemberCount(int handle);              //!< full roster size (slot count)
bool   GetMemberAlive(int handle, int slotIndex); //!< per-slot truth (D2)
vector GetPosition(int handle);                 //!< group origin — valid dormant or spawned
void   SetPosition(int handle, vector position); //!< `movement` writes here WHILE DORMANT only

//! Records a member death by roster slot. Called internally by the death hook; public so
//! consumers and tests can report deaths they observed themselves. Killing the last living
//! slot wipes the record (OnGroupWiped fires) exactly as an in-world wipe would.
void ReportMemberKilled(int handle, int slotIndex);

// -- lifecycle overrides (proximity is the default; these are the escape hatches) --
void ForceSpawn(int handle);      //!< RequestSpawn regardless of proximity
void ForceDespawn(int handle);    //!< DespawnMembers regardless of proximity (respects held members)

// -- events (all server-side ScriptInvokers) --
ScriptInvoker GetOnGroupWiped();        //!< (int handle) — fired BEFORE the record is removed
ScriptInvoker GetOnRecordsRestored();   //!< () — fired once after persisted records are re-linked
// Spawn/despawn notifications: subscribe on the group itself —
//   group.GetOnMembersDespawning()  (SCR_AIGroup.c:2288, invoked :2894)
//   group.GetOnAgentAdded() / Event_OnInit (fires only on COMPLETE fill in 1.8 — do not gate on it)
// api.md documents both; core adds manager-level wrappers only if `integration` proves the need.

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

**Naming discipline:** everything a consumer holds is an `int handle`. Handles are allocated from a monotonic counter that is itself persisted, so a handle is never reused across a reload. Consumers may persist handles, but `FindGroupsByOwner` exists so they do not have to (R3).

**The registration path** (the heart of Phase 3):

1. Resolve `(factionKey, groupName)` → prefab via `OVT_Global.GetFactions().GetOverthrowFactionByKey()` → `OVT_Faction.GetGroupPrefabByName()`; bail `-1` + WARNING on miss.
2. `SCR_AIGroup.IgnoreSpawning(true)` (static one-shot, `SCR_AIGroup.c:2217`, consumed `:2579`) → spawn the group prefab at `position` → **zero members exist**.
3. `SetFaction`, `SetLifecyclePolicy(ProximityDriven, ResolveSpawnDistance(override, global), spawn × hysteresis, -1)`, `SetImportance(importance)`. **Do NOT stamp `SetEliminateWhenReached(true)` at registration** (Phase 1 amendment 3: with non-player observers a dormant group inside the very-near ring gets deleted outright) — core enables it only after the mask reports the group wiped, as the cleanup mechanism for the eliminated record.
4. Build `AIWaypoint` entities from the plan (waypoint helpers on `OVT_OverthrowConfigComponent` :401-543), attach to the group, record in `m_aOwnedWaypoints`.
5. Capture the roster size from the prefab's slot list, initialise `m_aSlotAlive` all-alive, and hand the mask to the modded group (`SetOVTSlotMask` — D2) so the engine's refill spawns exactly the surviving slots.
6. Ensure the group entity (and its waypoints) are persistence-tracked — **Phase 1 verdict:** vanilla's class rule is defeated by Overthrow's unconditional untrack (`Modded/SCR_AIGroup.c:30-35`, BUG-118) and `SelfSpawn 0` (`Overthrow.conf:77-88`). Phase 3 builds a per-registered-group untrack exemption + a registered-groups-only self-spawn path (likely a dedicated higher-priority `EntityPersistenceConfig` with an Overthrow rule class; `MarkForSelfSpawn` is banned — BUG-116 save corruption).
7. Subscribe the group's wipe-relevant signals and build the death-hook mapping; store the record; return the handle.

A freshly registered group is dormant-by-construction (never spawned, `GetDormantAliveCount() == -1` sentinel = "never despawned" — `SCR_AIGroupSerializer.c:352`); the engine's own `LifecycleTick` materialises it when an observer arrives. Core never polls proximity for tracked groups.

### 3.4 Ambient spawn-source config classes

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

**Ambient lifecycle** is the one tick core still owns — ambient entities are loose `IEntity`s (civilians, parked vehicles), not groups, so the engine's group lifecycle cannot drive them. One `CallLater` (~2 s) round-robins ambient sources, asking `ObserversSystem.HasObserverWithinRangeSq(x, z, distSq)` — the engine's observer set (cameras, players, MP identities), not a hand-rolled player loop. Spawn work is spread across ticks (cap per tick); despawn discards; respawn re-rolls from config. Attributes: `m_iAmbientTickIntervalMs` (2000), `m_iAmbientSourcesPerTick` (8), `m_iAmbientSpawnsPerTick` (3), `m_fAmbientDespawnHysteresis` (1.15).

### 3.5 Importance policy (D4)

Vanilla defaults every group to LOW (50% budget cap, evicted first) and stamps *player* groups CRITICAL — so unstamped Overthrow hostiles are budget-starved precisely when recruit squads are saturating the budget. Core's policy: registration default **NORMAL**; consumers pass explicitly per role when they migrate (guidance in `api.md`: garrisons/base defenders HIGH per vanilla's own tier comments in `SCR_EAISpawnImportance.c`, ambient-feel patrols NORMAL, filler LOW). The existing un-migrated spawn paths (`OVT_EntitySpawningAPI`, tower defense, `BuyPatrol`) are the 1.8 hardening backlog's problem (`docs/reforger/1.8.0.10-changes.md` follow-ups), not core's — but core's `api.md` records the tier guidance both efforts share.

### 3.6 Persistence design

**Group state is vanilla's job; core persists only its registry.**

- **Vanilla, already connected:** `Overthrow.conf` inherits `Common.conf`, which wires `AIGroup.conf` / `AIUnit.conf` / `AIWaypoint.conf` into the `AIGroup` / `AIWaypoint` collections (`Common.conf:24,28,93`). `SCR_AIGroupSerializer` explicitly round-trips dormant groups — *"A dormant ChimeraAIGroup is legitimately empty … but still needs to round-trip through save/load so its alive / dead counts persist"* (`SCR_AIGroupSerializer.c:80-86`) — plus faction and waypoint references. Live members persist as full characters. **Phase 1 verifies this actually fires for runtime-spawned groups under Overthrow's config** (tracking mechanics are the spike's question #3).
- **Overthrow serializer — registry bookkeeping only.** `OVT_VirtualizationManagerSerializer : ScriptedComponentSerializer` on the manager persists, per record: `handle`, `ownerSystem`, `ownerKey`, `factionKey`, `groupRegistryName`, `resolvedPrefab`, `spawnDistanceOverride`, `importance`, the per-slot alive mask (parallel array — D2), and the group's persistence `UUID` (`GetSystem().GetId(record.m_Group)`) — plus the handle counter. Parallel arrays, `version` first, `if (version < 1) return true;`, append-only forever (the town serializer's rules, `OVT_TownManagerSerializer.c:1-12`).
- **Relink on load, vanilla's own idiom:** for each persisted entry, `GetSystem().WhenAvailable(groupUuid, task)` re-binds `m_Group` when vanilla restores the group entity (the exact pattern `SCR_AmbientPatrolSpawnPointComponentSerializer.OnGroupAvailable` uses). A record whose group UUID never resolves (group was wiped-and-deleted, or the save predates it) is dropped with a WARNING. Re-stamp policy/distances/importance on relink, re-push the slot mask to the modded group, and re-assert the engine's dormant counts from the mask (D2b) — none of this is part of vanilla's payload.
- **`GetOnRecordsRestored()` fires once** after all pending relinks resolve (count-down latch over the `WhenAvailable` tasks), so consumers reclaim deterministically (R2).
- **Idempotent re-apply:** match by handle, update in place, drop records absent from the payload — same contract as `ApplyPersistedDeployment` (`OVT_DeploymentComponent.c:100-153`), applied through a public `ApplyPersistedRegistry(...)` manager method so the serializer stays a pure codec.
- **Ambient sources are not in any payload.** Asserted by construction; stated in the serializer header.

Registration in `Configs/Systems/Persistence/Overthrow.conf`, in the game-mode `ComponentSerializers` block, continuing the `6B0E7A2x` series with a freshly generated, verified-unique GUID.

### 3.7 The Economy 2.0 extensibility seam (design-for, build-nothing)

| Seam | Why it is enough |
|---|---|
| `m_sOwnerSystem` is a **string**, not an enum | A new class of virtual thing (`"economy_delivery"`) needs no core edit, no enum bump, no serializer version bump. |
| Lifecycle is per-group engine policy, position is writable while dormant | A future economy tick moves dormant groups exactly the way `movement` will; core stays behaviour-free. |
| Composition is `(factionKey, groupName, resolvedPrefab)` | A non-combat agent registers a civilian or vehicle-group prefab through exactly the same field. Nothing in the record says "soldier". |
| `m_iSpawnDistanceOverride` is per record | An agent that should never materialise registers `0`; one that should always exist registers a huge value. No new lifecycle mode. |

Anything beyond this — virtual economy state, agent goals, transactions — belongs to Economy 2.0 and is **out of scope**.

---

## 4. Implementation Phases

Each phase ends compiling clean and with its automated gate green. **Phase 1 is a measurement gate: Phase 2 must not start until it has written answers.**

---

### Phase 1 — Engine-adoption spike (gate, ~1 day)

**Agent:** `component-developer`
**Estimate:** 5-8 h + one play-test round
**Why first:** the whole plan now leans on 1.8 engine behaviour that no Overthrow code has ever exercised. Every question below changes Phase 3's or Phase 5's shape if the answer surprises.

**Tasks** (throwaway spike code tagged `[OVT-VIRTSPIKE]`, removed before the phase closes; drive via the debug affordance or Script Console — see `docs/features/dev-ops/` live-session execution):

1. **T1.1 ObserversSystem is live in Overthrow's world.** Overthrow overrides `Configs/Systems/ChimeraSystemsConfig.conf` (delta); vanilla 1.8 added `ObserversSystem` + `VisibilityConsumerSystem` to it. Confirm the systems run in an Overthrow campaign (log from `GetGame().GetWorld().FindSystem`/spike probe) — group dormancy depends on them.
2. **T1.2 The lifecycle behaves as read.** Spawn a real faction group prefab with `IgnoreSpawning(true)` (confirm zero members), `SetLifecyclePolicy(ProximityDriven, 300, 350, -1)`, `SetImportance(NORMAL)`, `SetEliminateWhenReached(true)`. Play-test: members materialise progressively on approach (via the queue, no hitch); all despawn on withdrawal; **kill 3 of 8, withdraw, return → exactly 5 spawn**; wipe the group → record self-eliminates and never returns; reach an empty eliminated position → entity deletes and `GetOnEliminatedWhenReached` fires. **Also verify the D2 seam:** a modded `ExpandOneMember` override is actually called by the queue dispatcher (log + pick a non-default slot), `SpawnGroupMember(snapToTerrain, slotIndex, prefab)` accepts an arbitrary slot index, the member→slot mapping is observable at spawn — and observe what `DespawnMembers` records when the group was budget-under-filled (the count-corruption vector D2(b) corrects).
3. **T1.3 Persistence fires for runtime-spawned groups.** With the spike group dormant, take an in-game save. Decode it (persistence-forensics skill): is the group in the payload with its dormant counts? Answer **how** runtime-spawned `SCR_AIGroup`s get tracked under Overthrow's config (automatic via `Common.conf`'s `EntityClassPersistenceConfigRule "AIGroup"`, or does core need `OVT_PersistenceTracking.Track()` / a `SelfSpawn`-style flag?). Then quit-to-menu → Continue: the group returns dormant with the right counts, and its waypoints return.
4. **T1.4 Test-world behaviour.** In the autotest world: is `GetGame().GetAIWorld()` a `ChimeraAIWorld` (i.e. does `RequestSpawn` queue, or fall back to synchronous `SpawnMembers` — `SCR_AIGroup.c:2698-2704`)? Does registering an unspawned ProximityDriven group with no observers stay unspawned (safe for Init-tier cases)? Record what Init-tier tests may and may not assert.
5. **T1.5 Scale probe.** Spawn ~100 dormant registered groups spread over a region; record server memory/frame-time deltas and (with a connected client) replication behaviour. This prices the model inversion (D1) at campaign scale — ~30 bases × garrisons + patrols is the realistic ceiling.
6. **T1.6** Write every answer into `context.md` with evidence (log lines, save-decode output, numbers). **Amend Phases 3/5 in this document before Phase 2 starts** if any answer contradicts the design.

**Acceptance criteria**

- `context.md` records answers to T1.1-T1.5 with evidence; open contradictions are resolved in this document.
- All `[OVT-VIRTSPIKE]` code is removed; `tools/compile-check.sh` exits 0.
- The user has play-tested T1.2 and confirmed the survivor-count behaviour first-hand.

---

### Phase 2 — Records, registry API, manager scaffold, config field

**Agent:** `component-developer`
**Estimate:** 5-8 h
**Why now:** the API is the epic's contract. It lands, gets reviewed, and only then gets the engine wired behind it (Phase 3).

**Tasks**

1. **T2.1** Create `Scripts/Game/GameMode/Virtualization/OVT_VirtualGroupRecord.c` — `OVT_EVirtualWaypointType`, `OVT_VirtualWaypointPlan`, `OVT_VirtualGroupRecord` exactly as §3.3. `ref` on every Managed member of an array or map.
2. **T2.2** Create `OVT_VirtualizationManagerComponent.c` with the class pair, `s_Instance`/`GetInstance()`, `OnPostInit` (server guard **before** allocating collections, `OVT_DeploymentManager.c:64-84`), `Init(IEntity)`, empty `PostGameStart()`, and `OnDelete` clearing `s_Instance` and removing the (not yet started) ambient `CallLater`.
3. **T2.3** Implement the registry API surface of §3.3 **except** the engine wiring: register/unregister/query/reclaim/state accessors and both invokers. `RegisterGroup` resolves composition and allocates the record + handle but stubs the entity-creation step (Phase 3); `ForceSpawn`/`ForceDespawn` exist as no-op-with-WARNING stubs.
4. **T2.4** Create `OVT_VirtualizationMath.c` — pure, world-free statics the Logic tier can reach: `ResolveSpawnDistance(override, globalDefault)`, `ResolveDespawnDistance(spawnDistance, hysteresis)`, `ResolveImportance(requested, configuredDefault)`, `ValidateWaypointPlan(positions, types, params)` (parallel-array integrity), `CountAlive(mask)` / `IsWiped(mask)` / `NextSlotToSpawn(mask, spawnedSlots)` (the D2 refill selector, pinned world-free), `SliceIndices` / `AdvanceCursor` (the ambient round-robin), `RollCountSafe(min, max)` (the RandInt max-exclusive guard). **No manager, game mode or world reference anywhere in this file.**
5. **T2.5** Add `int virtualizationSpawnDistance;` to `OVT_OverthrowConfigStruct` (`OVT_OverthrowConfigComponent.c:24-66`) and `virtualizationSpawnDistance = 1750;` to `SetDefaults()`. No JIP bitstream change — server-only value. Leave `m_iMilitarySpawnDistance` / `m_iCivilianSpawnDistance` alone; un-migrated consumers still use them.
6. **T2.6** Add `OVT_Global.GetVirtualization()`.
7. **T2.7** Register the manager in `OVT_OverthrowGameMode`: field + `FindComponent`/`Init(this)` block in `EOnInit` (**after** Deployment at :1156), and a `PostGameStart()` block in `DoStartGame()`.
8. **T2.8** **User task (Workbench):** add `OVT_VirtualizationManagerComponent` to `Prefabs/GameMode/OVT_OverthrowGameMode.et`. Until this is done every `GetInstance()` returns null and the Init-tier case fails — the fail-proof for T2.10.
9. **T2.9** Create `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_Virtualization.c` covering T2.4's statics: override precedence (−1 → global, 0 → never, huge → always), hysteresis, importance defaulting, plan validation (mismatched parallel arrays rejected), mask counting/wipe detection, `NextSlotToSpawn` (skips dead slots, skips already-spawned slots, returns −1 when none left), slice arithmetic (wraparound, `sliceSize > count`, `count == 0`), `RollCountSafe` at `min == max`.
   ⚠️ **Tier rule:** the Logic directory is grepped for the manager-accessor and game-mode-getter identifiers — they may not appear **anywhere** under `TestSuites/Logic/`, *including in comments*.
10. **T2.10** Add to `OVT_TEST_InitSuite.c`: manager resolves through `OVT_Global`; `GetGroupCount()` is 0 before anything registers; `virtualizationSpawnDistance` reads back its default; `RegisterGroup` with an unknown faction key and an unknown group name both return −1 with a WARNING.
11. **T2.11** Write `docs/features/virtualization/core/api.md` — the §3.3 signatures, the importance-tier guidance (§3.5), the D2 slot-accuracy contract, the group-invoker subscription pattern for spawn/despawn notification, and a worked "register → reclaim after load → unregister" example. Siblings are planned against this; keep it current at every later phase.

**Acceptance criteria**

- `tools/compile-check.sh` exits 0; `tools/run-tests.sh "{6A6E29FF47ECB840}"` (Fast) exits 0 including the new cases.
- Every new case has its can-fail proof recorded in a preamble comment. No `maxAttempts`.
- `api.md` exists and matches the code.
- `grep -rn "Rpc\|RplProp\|Replication.Bump" Scripts/Game/GameMode/Virtualization/` is empty.

---

### Phase 3 — Engine lifecycle wiring, wipe bookkeeping, waypoints

**Agent:** `component-developer-advanced` — **advanced.** Entity lifetime across engine-driven dormancy/elimination, persistence tracking, and waypoint ownership. Getting the wipe signals or the tracking wrong silently corrupts every consumer's records.
**Estimate:** 8-12 h

**Tasks**

1. **T3.1** Implement the full registration path of §3.3 (steps 1-6): `IgnoreSpawning` → spawn → `SetFaction` → `SetLifecyclePolicy(ProximityDriven, resolved distances)` → `SetImportance` → `SetEliminateWhenReached(true)` → waypoint entities from the plan → persistence tracking per the Phase 1 T1.3 verdict → signal subscriptions.
2. **T3.2** Waypoint construction uses the centralized helpers on `OVT_OverthrowConfigComponent` (`SpawnPatrolWaypoint` :407, `SpawnDefendWaypoint` :468, `SpawnWaitWaypoint` :485, `GivePatrolWaypoints` :501). **Record every created `AIWaypoint` in `m_aOwnedWaypoints`.**
   ⚠️ `SpawnWaitWaypoint(pos, time)` accepts `time` and **never applies it** (:485) — do not rely on the duration until that is fixed; note it in `context.md`.
3. **T3.3** The D2 mask machinery: `SetOVTSlotMask`/clear seam and the `ExpandOneMember` override on Overthrow's modded `SCR_AIGroup` (spawn the next mask-alive, not-yet-materialised slot; vanilla fallback when no mask is set), plus the member→slot reverse map built as members spawn.
4. **T3.4** Death accounting: subscribe once to `OVT_OverthrowGameMode.GetOnCharacterKilled()`; victim → (handle, slot) via the reverse map; `ReportMemberKilled` updates the mask. Ignore anything arriving inside the group's despawn-teardown window (`GetOnMembersDespawning` guard). **No `GetOnAgentRemoved` anywhere** (D2a).
5. **T3.5** Count correction: after each engine despawn, re-assert `SetDormantCounts(CountAlive(mask), deadCount)` from the mask (D2b), so budget-under-fill never falsely kills members.
6. **T3.6** Wipe bookkeeping: subscribe `GetOnEliminatedWhenReached` (`SCR_AIGroup.c:2306`); additionally detect wiped-while-spawned (last member killed → vanilla's empty handling deletes non-dormant groups, `m_bDeleteWhenEmpty`) — hold the group by `EntityID`/null-check every touch, and on either signal fire `GetOnGroupWiped(handle)` **before** removing the record, then delete owned waypoints. A wiped group's persistence entry must also disappear (untrack per the T1.3 mechanics; note the BUG-118 IsTracked retry-queue hazard for entities still queuing).
7. **T3.7** `UnregisterGroup`: `DespawnMembers` if spawned (respecting `HasHeldMember` — log and proceed with despawn-when-released if held), delete the group entity and owned waypoints, untrack, remove the record.
8. **T3.8** `ForceSpawn` → `RequestSpawn()` (`SCR_AIGroup.c:2678`); `ForceDespawn` → `DespawnMembers()`. Document that a force-spawned group the engine later re-evaluates will re-despawn when out of range — force is a nudge, not a pin.
9. **T3.9** Add `[Attribute(defvalue: "false")] bool m_bDebugRegisterTestGroup` — registers one virtual group a short distance from campaign start in `PostGameStart()`. The only way to play-test core before any consumer exists. Default false, documented in `context.md`.
10. **T3.10** Init-tier cases (shaped by the Phase 1 T1.4 verdict on what the test world permits): register a group → `GetGroupCount()` is 1 and the group entity exists unspawned with the stamped policy/importance; unregister → entity and waypoints gone; register with a waypoint plan → the expected `AIWaypoint` entities exist and are owned; `ReportMemberKilled` on a slot flips `GetMemberAlive` and reduces `GetAliveMemberCount`; killing every slot removes the record and fires `OnGroupWiped`.
11. **T3.11** `OnDelete`: despawn/delete everything live, clear `s_Instance` (R7).

**Acceptance criteria**

- `tools/compile-check.sh` exits 0; Fast and All both exit 0.
- With `m_bDebugRegisterTestGroup` on, a play-test shows: group absent at distance, members appearing progressively on approach, all disappearing on withdrawal, reappearing minus **exactly** the members killed (slot-accurate — D2), a wiped group never returning, and **no thrash at the boundary** (engine hysteresis).
- `grep -rn "AIWaypoint" Scripts/Game/GameMode/Virtualization/` shows every creation site paired with a delete site.
- No hand-rolled proximity polling for tracked groups: `grep -rn "PlayerInRange\|NearestPlayer" Scripts/Game/GameMode/Virtualization/` is empty.
- No `GetOnAgentRemoved` anywhere in the feature (D2a).

---

### Phase 4 — Ambient spawn-source seam

**Agent:** `component-developer`
**Estimate:** 6-9 h

**Tasks**

1. **T4.1** Create `OVT_AmbientSpawnSourceConfig.c` and `OVT_AmbientSpawnSourceRegistry.c` per §3.4, with the four overridable roll/hook methods and safe defaults (`RollCountSafe` from T2.4).
2. **T4.2** Create `OVT_AmbientSpawnSourceInstance.c` — runtime pairing of config + position + ownerKey + live entity list + spawn-progress state.
3. **T4.3** Implement `RegisterAmbientSource` / `UnregisterAmbientSource` / `GetAmbientSourceCount` / `GetAmbientEntities`, and the ambient `CallLater` tick per §3.4: round-robin slice, `ObserversSystem.HasObserverWithinRangeSq` proximity, hysteresis on the despawn side.
4. **T4.4** Ambient spawn: roll count once per activation, then per entity roll prefab + position, spawn, call `OnEntitySpawned`, record entity + reverse-map entry. **Spread across ticks** — a source rolling 20 civilians must not spawn 20 in one frame (`m_iAmbientSpawnsPerTick`).
5. **T4.5** Ambient despawn: call `OnEntityDespawning`, delete every still-live entity, clear list + reverse-map. **No state kept** — next approach re-rolls.
6. **T4.6** `ReleaseAmbientEntity(entity)`: O(1) reverse-map lookup, remove from source list and map, return true; false for non-ambient entities.
7. **T4.7** Prune dead/deleted entities from source lists each evaluation.
8. **T4.8** Init-tier cases: a registered source resolves and is counted; a config subclass's overridden `RollCount()` is the one called; `ReleaseAmbientEntity` on an unknown entity returns false.

**Acceptance criteria**

- Fast and All both exit 0 including the new Init cases.
- Core ships **zero** authored ambient sources — `grep -rn "OVT_AmbientSpawnSourceConfig" Configs/` is empty.
- A registered source with 20 entities spawns them over several ticks (timestamped log lines in a play-test).
- Released entities survive the next despawn of their former source (play-test: release one, walk away, walk back — still there, not re-rolled).

---

### Phase 5 — Persistence: registry serializer + round-trip coverage

**Agent:** `component-developer-advanced` — **advanced.** Carries the epic's persistence promise and edits the shared round-trip suite that gates the All group.
**Estimate:** 8-12 h

**Tasks**

1. **T5.1** Create `Scripts/Game/Persistence/Serializers/Components/OVT_VirtualizationManagerSerializer.c` per §3.6 — registry bookkeeping only (parallel arrays incl. group UUIDs, handle counter, `version` first). Copy the shape and header discipline of a **shipped** serializer (`OVT_TownManagerSerializer.c`), not the stale template.
2. **T5.2** Implement `ApplyPersistedRegistry(...)` on the manager: idempotent match-by-handle, restore the counter, `WhenAvailable` relink per group UUID, re-stamp policy/distances/importance on relink, drop-with-WARNING for unresolvable groups or compositions (name the key).
3. **T5.3** `GetOnRecordsRestored()` fires once when all pending relinks have resolved or expired (count-down latch).
4. **T5.4** Register in `Configs/Systems/Persistence/Overthrow.conf`, game-mode `ComponentSerializers` block, fresh `6B0E7A2x`-series GUID, verified unique.
5. **T5.5** Ambient sources are **not** in the payload — asserted by construction, stated in the header.
6. **T5.6** Round-trip case `..._VirtualGroups_SurviveSaveAndReload` in `OVT_TEST_PersistenceRoundTripSuite.c` (shared `OVT_TEST_PersistenceRoundTripGate` phase machine; copy the shape of `..._TownControl_SurvivesSaveAndReload`):
   - **Mutate:** register a group with a distinctive owner key; `ReportMemberKilled` on a specific slot (the public API is the test seam — no world combat needed); save.
   - **Dirty:** change the counts and register a second, bogus group.
   - **Assert after re-apply:** the handle resolves, owner key matches, `GetAliveMemberCount()` is the saved reduced count, **the specific dead slot is still dead** (`GetMemberAlive` — D2), `FindGroupsByOwner` finds it, the bogus group is gone.
7. **T5.7** Second case: a **wiped** group does not come back — register, `ReportMemberKilled` every slot, save, re-apply, assert unregistered and `FindGroupsByOwner` empty.
8. **T5.8** Record the can-fail proof for both cases (e.g. drop the UUID write → relink fails → owner assertion red; drop the mask write → the dead-slot assertion goes red). **No `maxAttempts`.**

**Acceptance criteria**

- `tools/run-tests.sh "{6A6E2A002F53A581}"` (All) exits 0 with both new cases green, each with a recorded failure proof.
- `git diff Configs/Systems/Persistence/Overthrow.conf` shows exactly one added entry with a repo-unique GUID.
- The serializer writes `version` first and has no `[BaseContainerProps()]`.
- The quit-and-continue path (vanilla group state + Overthrow relink together) is verified in a play-test (§6 step 12) — the automated suite covers save→dirty→re-apply in-session only.

---

### Phase 6 — Measurement, hardening, API freeze

**Agent:** `component-developer`
**Estimate:** 4-6 h + one play-test round

**Tasks**

1. **T6.1** **Re-measure at scale** (G12): extend `m_bDebugRegisterTestGroup` to ~40 groups in one town; fast-travel in; record server frame time across the burst and time-to-populate; compare against the Phase 1 T1.5 numbers; write both into `context.md`. The knobs now live engine-side — if the burst hitches, the answer is importance tiers and registration density, not new queues; record the guidance.
2. **T6.2** Restart hardening: campaign → quit to menu → new campaign. No stale `s_Instance`, no orphaned ambient `CallLater`, no error spam (R7).
3. **T6.3** Missing-faction hardening: load a save whose records name a faction key the config no longer defines → drop-with-WARNING path verified (R4).
4. **T6.4** Drive-past check: drive past a dense registered area at speed — the engine queue's dequeue-time observer re-check should drop most requests; verify nothing fully materialises and nothing errors.
5. **T6.5** Freeze `api.md`: mark it the contract; state the D2 slot-accuracy contract (and its reliance on the modded `ExpandOneMember` override) prominently; list the exact entry points `civilians`, `movement` and `integration` each need.
6. **T6.6** Remove every remaining debug print not behind a `LogLevel.VERBOSE` guard.

**Acceptance criteria**

- `context.md` contains the measured numbers with the configuration used.
- Restart, missing-faction and drive-past cases verified by the user in a play-test, observations recorded.
- `api.md` is marked frozen and matches the shipped signatures.

---

## 5. Key Technical Decisions

**D1 — Adopt the 1.8 engine lifecycle; the dormant `SCR_AIGroup` entity is the durable record** *(user-approved 2026-08-14; supersedes Revision 1's "record, not entity")*. Revision 1's model — a plain data record with the entity as transient projection — was designed against a 1.7 engine with no lifecycle support. 1.8 inverted the economics: the engine keeps the group entity alive through dormancy *specifically to be the record* (`SCR_AIGroup.c:2860-2863`), drives it from `ObserversSystem`, arbitrates a global AI budget Overthrow code cannot see, and persists it. Hand-rolling a parallel layer now means **fighting** engine eviction (which can despawn our groups under us regardless) rather than being in control. Cost accepted: one lightweight replicated entity per virtual group — priced in Phase 1 T1.5 before the design is irreversible. Vanilla precedent for the whole pattern: Conflict's ambient patrols (`SCR_AmbientPatrolSystem.c:140,195`).

**D2 — Slot-accurate dead-member truth: core-owned mask + `ExpandOneMember` override** *(user decision 2026-08-14, revising an initial count-based acceptance the same day)*. Engine dormancy stores only alive/dead **counts**, and vanilla refill picks slots in order `0..alive-1` (`ExpandOneMember`, `SCR_AIGroup.c:2731`) — a group that lost only its slot-1 MG would come back at the right strength with the MG alive and a tail rifleman dead instead. Overthrow keeps identities: the record carries `m_aSlotAlive` (one flag per prefab slot, captured at registration), deaths are recorded **by slot** through the game-mode kill invoker (`OVT_OverthrowGameMode.GetOnCharacterKilled()`, raised by the modded damage manager for every character including AI; a member→slot reverse map is built at spawn), and Overthrow's already-modded `SCR_AIGroup` overrides the `ExpandOneMember` event (`ChimeraAIGroup.c:29`) to spawn the next *mask-alive, not-yet-materialised* slot instead of the next index — falling back to vanilla behaviour when no mask is set, so unregistered groups are untouched.

Two corollaries. **(a) Deaths come from the kill invoker, never `GetOnAgentRemoved`** — agent removal cannot distinguish a death from the engine's own dormancy teardown; note `DespawnMembers` deletes members via `RplComponent.DeleteRplEntity` (`SCR_AIGroup.c:2905`), which raises no kill event, but the teardown-guard discipline (ignore anything arriving inside the group's `GetOnMembersDespawning` window) is kept anyway. **(b) The mask is *more* authoritative than the engine's counts.** `DespawnMembers` records `alive = GetAgentsCount()` at despawn time (`SCR_AIGroup.c:2868-2876`), so a budget-under-filled group (the queue dropped 2 of 7 wanted members) would have its missing members silently counted dead — after each despawn core re-asserts `SetDormantCounts(maskAlive, maskDead)` from the mask, correcting a corruption vector the pure-vanilla count model carries. The Phase 1 spike observes both behaviours before Phase 3 builds on them.

**D3 — Composition identity is `(factionKey, groupRegistryName)` with a resolved-prefab fallback.** Faction **indices** are positional across saves (`OVT_DeploymentManagerSerializer.c:20-23`); `m_sFactionKey` is stable. `resolvedPrefab` survives a registry rename; if both fail, the record is dropped with a WARNING rather than resurrected wrong.

**D4 — Explicit importance, default NORMAL.** Vanilla defaults groups to LOW (50% budget cap, evicted first) and stamps player groups CRITICAL — unstamped hostile AI is starved exactly when player-recruit squads saturate the budget. Every core registration stamps importance; tier guidance lives in `api.md` (§3.5). Registration default NORMAL, per-registration parameter for consumers.

**D5 — Global default + per-registration override for spawn distance** *(carried from Revision 1, user-approved)*. Operator-editable `virtualizationSpawnDistance` in `Overthrow_Config.json` via `OVT_OverthrowConfigStruct`, defaulting to 1750 (today's `m_iMilitarySpawnDistance`); each record and ambient source may override; `-1` means "use the global". Fed into `SetLifecyclePolicy` per group, so issue #100's "very large value keeps everything spawned" works natively. Server-only — no JIP bitstream change.

**D6 — Core owns waypoint-entity lifetime.** Every spawner in the tree except `OVT_MultiTownPatrolBehaviorDeploymentModule` leaks `AIWaypoint` entities — including `OVT_EntitySpawningAPI.CleanupGroup()` (:379-403), which detaches without deleting. Core records every waypoint it creates in `m_aOwnedWaypoints` and deletes them on unregister/wipe. New wrinkle vs Revision 1: waypoints now also **persist** (vanilla `AIWaypoint.conf`), so deletion must also untrack — verified in the Phase 5 round trip.

**D7 — Server-only, no replication, no new comms RPC.** Nothing core adds has a client half: no `RplProp`, no `Rpc`, no JIP payload, nothing on the deprecated `OVT_PlayerCommsComponent`. The server guard goes in `OnPostInit` before any collection is allocated. (The group entities replicate as vanilla always has — that is the engine's business, not ours.)

**D8 — Registry bookkeeping persists via Overthrow; group state persists via vanilla.** One system per concern: `SCR_AIGroupSerializer` (connected through `Common.conf`, which `Overthrow.conf` inherits) owns faction/waypoints/dormant counts/live members; core's serializer owns handles/owner tags/config overrides and relinks by UUID with `WhenAvailable` — the exact idiom vanilla's own `SCR_AmbientPatrolSpawnPointComponentSerializer` uses for the same job. No double bookkeeping.

**D9 — `GetOnRecordsRestored()` removes a whole ordering-bug class.** Serializer ordering is not a contract; core fires one invoker after all relinks resolve, consumers reclaim there and never race. (R2)

**D10 — `int` handles, `string` owner tags.** Handles are stable across save/load (counter persisted); owner tags are strings so a new consumer class needs no enum change and no serializer bump — the Economy 2.0 seam (§3.7). `FindGroupsByOwner` is the preferred reclaim path.

**D11 — Ambient proximity via `ObserversSystem`, not player loops.** The one tick core owns (ambient sources — loose entities the engine's *group* lifecycle cannot drive) still delegates observer bookkeeping to the engine: `HasObserverWithinRangeSq` covers cameras, players and MP identities and is maintained by the same system driving group lifecycle, so tracked and ambient behaviour can never disagree about "is anyone nearby".

---

## 6. Definition of Done

An evaluator with **no implementation context** should be able to verify every item.

### Functional Criteria

- **F1** A registered virtual group with no player nearby has **no member characters in the world** (a dormant group entity is expected), and `IsSpawned(handle)` is false.
- **F2** When a player comes within the configured distance, the group materialises; members appear **progressively** (engine queue), not all in one frame.
- **F3** When the last player leaves the area, the group's member characters are deleted; `IsSpawned(handle)` is false again; the group entity and its waypoints remain.
- **F4** Killing 3 of an 8-member group, then walking away and back, produces a group of exactly the **5 surviving members** — the 3 slots you killed (roles/loadouts) do not return (slot-accurate — D2), and the group never exceeds 5 again.
- **F5** Killing **every** member removes the record: `IsRegistered(handle)` is false, `FindGroupsByOwner` no longer finds it, and it never respawns — including after reaching the position again (`SetEliminateWhenReached`).
- **F6** Setting `virtualizationSpawnDistance` in `$profile:Overthrow_Config.json` to a very large value keeps every registered group spawned permanently; a small value despawns everything not adjacent. Takes effect on next campaign start with no code edit.
- **F7** A record registered with a per-registration override uses the override, not the global — two records at the same position with different overrides behave differently.
- **F8** Save with a partially-wiped dormant group, quit, Continue: the group returns with its exact surviving **slots** (count *and* identities — D2), its position and waypoints (vanilla), and its owner key/handle (Overthrow relink); approaching materialises exactly those members.
- **F9** `FindGroupsByOwner(ownerSystem, ownerKey)` returns the same handles after a save/re-apply cycle as before it.
- **F10** A registered **ambient source** spawns its entities on approach and deletes them on withdrawal; the next approach produces a **freshly rolled** set, and nothing about them appears in any save file.
- **F11** An entity passed to `ReleaseAmbientEntity()` survives the next despawn of its former source and is not re-rolled.
- **F12** Standing at the spawn-distance boundary produces **no** repeated spawn/despawn cycling in the server log (engine spawn/despawn gap + very-near block).
- **F13** Fast-travelling into an area with many registered groups produces **no visible frame hitch**; groups fill in over several seconds.
- **F14** Every registered group reports the importance it was registered with (`GetImportance`), never vanilla's default LOW unless explicitly requested.

### Quality Criteria

- **Q1** `tools/compile-check.sh` exits **0** with no output.
- **Q2** Fast (`{6A6E29FF47ECB840}`) and All (`{6A6E2A002F53A581}`) both exit **0**.
- **Q3** Every new test case has a recorded proof that it can fail — the exact edit used, in a preamble comment. **No `maxAttempts` anywhere.**
- **Q4** Nothing core adds replicates: `grep -rn "Rpc\|RplProp\|Replication.Bump\|RplSave\|RplLoad" Scripts/Game/GameMode/Virtualization/` is empty, and `OVT_PlayerCommsComponent.c` is unchanged.
- **Q5** No waypoint leak: every `AIWaypoint` creation site in the feature is paired with a deletion site, and a 20-cycle spawn/despawn play-test leaves no growing entity count.
- **Q6** No persisted faction **index** in the registry payload.
- **Q7** No hand-rolled proximity for tracked groups: `grep -rn "PlayerInRange\|NearestPlayer" Scripts/Game/GameMode/Virtualization/` is empty; tracked-group lifecycle contains no Overthrow `CallLater`.
- **Q8** The Logic-tier file contains no manager-accessor or game-mode-getter identifier, in code **or** comments.
- **Q9** Core ships **no** authored ambient content: `grep -rn "OVT_AmbientSpawnSourceConfig" Configs/` is empty.
- **Q10** Debug affordances default off; all `[OVT-VIRTSPIKE]` instrumentation is removed.
- **Q11** `docs/features/virtualization/core/api.md` exists, is marked frozen, states the D2 slot-accuracy contract, and matches the shipped code.
- **Q12** No `GetOnAgentRemoved` in the feature (D2a) — deaths come from the kill invoker only.

### Integration Criteria

- **I1 No consumer migrated.** `OVT_TownController.c`, `OVT_DeploymentComponent.c`, `OVT_BasePatrolUpgrade.c` and `OVT_OccupyingFactionManager.c` are **unchanged**. Migration is `civilians` and `integration`, not here.
- **I2 Config seam.** `OVT_OverthrowConfigStruct` gained exactly one field; `m_iMilitarySpawnDistance` / `m_iCivilianSpawnDistance` still exist; the JIP `RplSave` block and `CONFIG_STREAM_VERSION` are unchanged.
- **I3 Game mode seam.** `OVT_OverthrowGameMode` gained one field, one `EOnInit` block and one `PostGameStart` block, after the Deployment manager, in the existing style.
- **I4 Persistence seam.** Exactly one entry added to `Configs/Systems/Persistence/Overthrow.conf`; no existing serializer changed; no vanilla persistence config overridden.
- **I5 Movement seam present.** `GetPosition`/`SetPosition` (dormant-write), `GetGroup` and the owned-waypoint list exist and are documented in `api.md` as `movement`'s entry points, with nothing in core advancing anything.
- **I6 Test-tier seams.** No test-group config changed (new cases join existing suites via `[Test(suite: …)]`).

### Verification Method

**Automated — run from the repo root, in order:**

1. `tools/compile-check.sh` → exit **0**, no output.
2. `tools/run-tests.sh "{6A6E29FF47ECB840}"` → exit **0** (Fast).
3. `tools/run-tests.sh "{6A6E2A002F53A581}"` → exit **0** (All, includes the persistence round trip).
4. `git diff --stat Scripts/Game/Controllers/ Scripts/Game/GameMode/Deployments/` → **empty** (I1).
5. `git diff Configs/Systems/Persistence/Overthrow.conf` → exactly one added `OVT_VirtualizationManagerSerializer` entry.
6. `grep -rn "Rpc\|RplProp\|PlayerInRange\|NearestPlayer\|GetOnAgentRemoved" "Scripts/Game/GameMode/Virtualization/"` → **empty** (Q4, Q7, Q12).

**Manual — solo play-test** (set `m_bDebugRegisterTestGroup` true on the manager in the game-mode prefab, Workbench):

1. Start the campaign, stay far away. **Expect:** no AI members there; log shows the record registered, group dormant. → F1
2. Approach. **Expect:** members appear progressively via the engine queue. → F2
3. Withdraw past the despawn distance. **Expect:** members gone; group entity dormant. → F3
4. Return, kill 3 distinctive members (e.g. the MG and two riflemen), withdraw, return. **Expect:** exactly 5 members, and none of the roles you killed is back (slot-accurate — D2). → F4
5. Kill the whole group, withdraw, return. **Expect:** nothing spawns, ever; walking onto the position deletes the empty record (log line). → F5
6. Stand at the boundary for 60 s. **Expect:** no repeated spawn/despawn pairs in the log. → F12
7. Set `virtualizationSpawnDistance` to `100000`, restart campaign. **Expect:** group spawned from campaign start. Set `50`, restart. **Expect:** despawns as soon as you step back. → F6
8. Extend the debug registration to ~40 groups in one town and fast-travel in. **Expect:** no visible hitch; groups fill in over seconds. Record server frame time. → F13, G12
9. Drive past that town at speed without stopping. **Expect:** most groups never materialise (dequeue-time observer re-check drops them). → F13
10. Register an ambient source (debug affordance or Script Console), approach, withdraw, approach. **Expect:** the second set differs from the first. → F10
11. `ReleaseAmbientEntity()` on one, withdraw, return. **Expect:** that entity is still there, unchanged. → F11
12. Kill 2 distinctive members of an 8-member group, save, quit to menu, **Continue**. **Expect:** the group is registered with 6 alive at its position; approaching materialises exactly the 6 survivors — the 2 killed roles stay dead. → F8, F9
13. Start a campaign, quit to menu, start a second campaign. **Expect:** no error spam, no duplicated ambient ticking, identical behaviour. → R7

*(Step 12 is the only path exercising the real quit-and-continue flow — the automated round-trip suite covers save→dirty→re-apply in-session only.)*

---

## 7. Testing Strategy

**The automated spine covers logic, wiring and the registry save format. The engine lifecycle itself (entities appearing/disappearing in a live world) is vanilla's code path — our tests cover Overthrow's use of it, and the play-test procedure covers the integration.**

### Logic tier — `TestSuites/Logic/OVT_TEST_Logic_Virtualization.c` (Fast)

World-free assertions on `OVT_VirtualizationMath`: distance resolution (override −1 / 0 / huge), hysteresis, importance defaulting, waypoint-plan validation, mask counting/wipe detection, the `NextSlotToSpawn` refill selector, ambient slice arithmetic, `RollCountSafe`. The tier's grep rule bans manager-accessor and game-mode-getter identifiers anywhere in the directory, comments included.

### Init tier — additions to `OVT_TEST_InitSuite.c` (Fast)

Manager resolves through `OVT_Global`; empty preconditions; register → count/entity/policy/importance assertions → unregister cleans up (scope per the Phase 1 T1.4 test-world verdict); unknown faction key and group name return −1; ambient source registration resolves, subclass roll override is called, `ReleaseAmbientEntity` on a non-ambient entity returns false.

Fail-proof for the resolution case: remove the component from the game-mode prefab, observe exit 1, restore.

### Persistence round trip — additions to `OVT_TEST_PersistenceRoundTripSuite.c` (All)

Two cases on the shared gate: partially-wiped group survives save → dirty → re-apply with its reduced count, the specific dead slot still dead, owner key and handle intact; wiped group does not come back. Assertions go through the public manager API only.

### Every new case must be proven able to fail once

Exact edit recorded in a preamble comment. `maxAttempts` is banned.

### Not automatable, and why

| Area | Why manual |
|---|---|
| Engine lifecycle in a live world | Needs real observers, real AI spawning — and it is vanilla's code |
| Frame-hitch behaviour under fast travel | A performance measurement, not an assertion |
| Waypoint-entity leaks | Repeated live cycles + entity count over time |
| The quit-and-continue path | `SaveGameManager.Load`'s world transition restarts the autotest harness |
| Multiplayer / JIP | Core adds nothing replicated, so risk is low, but "low" is not "verified" |
| Dormancy/budget interplay at scale | Phase 1 T1.5 / Phase 6 T6.1 measurements |

### Manual procedure

The numbered steps in §6 **are** the manual procedure. Gates by phase: Phase 1 → the spike session; Phase 3 → steps 1-6, 13; Phase 4 → steps 10, 11; Phase 5 → step 12; Phase 6 → steps 7, 8, 9.

---

## 8. Dependencies

**Hard preconditions (all satisfied today):**

- **Arma Reforger 1.8.0.10** — the engine lifecycle this plan adopts (`SCR_EAIGroupLifecyclePolicy`, `SCR_EAISpawnImportance`, `ObserversSystem`, dormancy, `SCR_AIGroupSerializer`). Migrated 2026-08-13; compile PASS, Fast 101 / All 142 green. See `docs/reforger/1.8.0.10-changes.md`.
- **`ObserversSystem` live in Overthrow's world** — vanilla adds it via `ChimeraSystemsConfig.conf`, which Overthrow overrides as a delta; Phase 1 T1.1 verifies the merge (also on the 1.8 report's checklist).
- **Vanilla persistence** — `ScriptedComponentSerializer` + `Overthrow.conf`, which inherits `Common.conf`'s AI entity configs (`AIGroup.conf` / `AIUnit.conf` / `AIWaypoint.conf`).
- **`OVT_Faction` group registries** — `OVT_FactionGroupRegistry` (`OVT_Faction.c:162-236`), `GetGroupPrefabByName()` (:498).
- **`OVT_OverthrowConfigStruct` / `LoadConfig()`** for the operator-editable distance.
- **Waypoint helpers on `OVT_OverthrowConfigComponent`** (:401-543).
- **Autotest harness** — `tools/run-tests.sh`, suites under `Scripts/Game/Tests/TestSuites/`.

**User-side (Workbench, interactive):** adding the manager to `Prefabs/GameMode/OVT_OverthrowGameMode.et` (T2.8); toggling `m_bDebugRegisterTestGroup`; the Phase 1 play-test session.

**Downstream (this feature blocks all of them):** `civilians` (ambient seam), `movement` (dormant-group position writes + owned waypoints), `integration` (tracked-group API + `GetOnRecordsRestored` + importance guidance), `base-defense-migration`.

**Related but separate:** the 1.8 hardening backlog (`docs/reforger/1.8.0.10-changes.md` follow-ups — dormancy-aware `GetAgentsCount()==0` audits, importance stamping on *existing* spawn paths, `GetOnInit` crew-boarding rework). Those fix today's systems; this feature builds the layer they later migrate onto. Do not fold them into core.

**Explicitly not depended on:** the old `virtualization` branch (design lessons only), EPF (nothing EPF-shaped anywhere), any client-side code.

---

## 9. Risks & Mitigation

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| **R1** | **The 1.8 lifecycle misbehaves in Overthrow's world** — ObserversSystem not merged through the config override, dormancy differing from the reference-tree reading, persistence not firing for runtime-spawned groups. | Medium | The plan's foundation | **Phase 1 is a gate, not a task**: every engine behaviour this plan leans on is verified in Overthrow's world with evidence in `context.md` before Phase 2 starts. Fallback if something is fundamentally broken: Revision 1's hand-rolled design is complete in git history. |
| **R2** | **Deserialize/relink ordering.** Vanilla restores the group entity on its own schedule; consumers reclaiming early find records not yet linked. | Medium | Deployments come back without their groups | `WhenAvailable`-based relink (vanilla's own idiom) + `GetOnRecordsRestored()` firing once after the latch drains (D9). Consumers reclaim there, never at their own deserialize. |
| **R3** | **A faction mod is removed** and a saved record names a key that no longer resolves. | Medium | Records vanish or resurrect wrong | Three-step resolution: key → registry name → `resolvedPrefab`. All fail ⇒ drop with a WARNING naming the key. Never a faction index. Verified Phase 6 T6.3. |
| **R4** | **Engine churn.** The lifecycle system is new in 1.8 and half of it is C++ (`CanActivateGroup`, the queue); BI will iterate in 1.8.x/1.9. | Medium per update | Behaviour shifts under us | Every vanilla symbol this design leans on is cited by file:line here and in `context.md` as the update-check checklist; the `/update-reforger` report process (which caught this system in the first place) is the tripwire. Counter-risk noted in D1: *not* adopting means fighting engine eviction instead. The D2 `ExpandOneMember` override adds one more drift surface — it is on the same checklist. |
| **R5** | **Dormant-entity cost at scale.** Hundreds of registered groups = hundreds of always-alive replicated entities. | Low-Medium | Server memory / replication overhead | Priced **before** commitment: Phase 1 T1.5 (~100 groups), re-measured Phase 6 T6.1 (~realistic density). If it is real, mitigation is registration density and importance tiers, decided on numbers. |
| **R6** | **Wipe-signal gaps.** A group wiped while spawned may be deleted by vanilla's empty handling before core notices; a group wiped while queued may linger. | Medium | Records leak or fire no `OnGroupWiped` | Two independent signals (elimination invoker + entity-gone detection on `EntityID`-held refs); Phase 1 T1.2 observes the actual sequence; §6 step 5 is the direct check. |
| **R7** | **`s_Instance` / ambient `CallLater` outlive a game-mode restart.** | Medium | Errors on the second campaign of a session | `OnDelete` removes the ambient tick, cleans up, clears `s_Instance`. Verified §6 step 13. |
| **R8** | **Waypoint-entity leak** — now with a persistence dimension (waypoints are tracked entities in 1.8). | High if not designed against | Unbounded entity growth + save bloat | D6: every created waypoint recorded and deleted+untracked on unregister/wipe; Q5's grep + 20-cycle play-test; Phase 5 round trip asserts no orphaned waypoints in the payload. |
| **R9** | **API churn after siblings start.** | Medium | Rework across the epic | `api.md` lands Phase 2, frozen Phase 6; changes recorded as breaking in `context.md`. The API is smaller than Revision 1's — easier to freeze. |
| **R10** | **Nothing consumes core**, so bugs stay invisible until `integration`. | High (by design) | Latent defects surface late | `m_bDebugRegisterTestGroup` makes core play-testable standalone; `civilians` is sequenced next to exercise the ambient half early. |
| **R11** | **Budget starvation of Overthrow AI.** Player/recruit groups are CRITICAL; unstamped hostiles are LOW. | High without D4 | Garrisons never materialise on busy servers | D4: every registration stamps importance, default NORMAL; F14 asserts it. The un-migrated spawn paths are the 1.8 backlog's items, tracked there. |
| **R12** | **Test-world limits.** The autotest world may lack `ChimeraAIWorld`/observers, constraining Init-tier registration cases (`RequestSpawn` falls back to synchronous spawn on non-Chimera worlds — `SCR_AIGroup.c:2698-2704`). | Medium | Weaker automated coverage | Phase 1 T1.4 answers exactly what the test world permits before any test is written; test scope is set by the verdict, not hope. Play-test procedure carries what the world cannot. |
| **R13** | **Death-accounting corruption.** If teardown deletions were ever counted as deaths, every despawn would mark the whole group dead and destroy the record — silently deleting campaign content. | Medium | Catastrophic and silent | D2a: `GetOnAgentRemoved` banned (Q12); deaths come only from the kill invoker; teardown-window guard; engine deletion raises no kill event (verified in the Phase 1 spike). §6 step 3→4 is the direct check: despawn 8, return, expect 8 — not 0. |

---

## Agent Routing Summary

| Phase | Agent | Advanced? |
|---|---|---|
| 1 — Engine-adoption spike (gate) | `component-developer` | no |
| 2 — Records, registry API, manager scaffold, config field | `component-developer` | no |
| 3 — Engine lifecycle wiring, wipe bookkeeping, waypoints | `component-developer-advanced` | **yes** — entity lifetime across engine-driven dormancy/elimination, persistence tracking, waypoint ownership |
| 4 — Ambient spawn-source seam | `component-developer` | no |
| 5 — Persistence: registry serializer + round-trip coverage | `component-developer-advanced` | **yes** — save-format + shared round-trip suite + UUID relink latch |
| 6 — Measurement, hardening, API freeze | `component-developer` | no |

**Skills to activate:** `enforcescript-patterns` (all phases), `overthrow-architecture` (2, 3, 4, 5), `workbench-workflow` (1, 3, 6), `persistence-forensics` (1, 5).

**No `ui-developer` or `network-specialist` work exists in this feature** — core adds nothing with a client surface (D7).
