# Virtualization Core — Consumer API

**Status:** 🔒 **FROZEN — THE EPIC'S CONTRACT** (Phase 6, 2026-08-17 — implementation.md T6.5)
**As built:** Phase 6 (2026-08-17) — tracked groups, ambient spawn sources and registry persistence
are all wired. Persistence is **Route B**: core re-creates its own group entities on load
(see [§8](#8-persistence--what-is-and-is-not-in-a-save))
**Additively extended:** 2026-08-17 — two new methods on `OVT_AmbientSpawnSourceConfig`
(`IsEntityDead`, `OnEntityPruned`), requested by `virtualization/civilians` Phase 1 and recorded in
`context.md`. See [§4](#the-config-classes-the-modder-seam). Nothing was renamed, re-signed or
re-meant, and every source written against the original five methods behaves identically.
**Additively extended again:** 2026-08-17 — the second additive change since the freeze: one new
query on the manager, `array<int> GetAllHandles()`, requested by `virtualization/movement` Phase 2
and recorded in `context.md`. See [§3](#queries-and-reclaim) and
[§10](#10-entry-points-per-sibling-feature). Nothing was renamed, re-signed or re-meant, no payload
field moved, and no existing call path changed behaviour — the method had no caller at all when it
landed.
**Additively extended a third time:** 2026-08-17 — `int GetCurrentPlanIndex(handle)`, requested by
`virtualization/movement` for the live→dormant adoption-direction play-test fix (coincident-leg routes) and
recorded in `context.md`. Same discipline: nothing renamed, re-signed or re-meant, no payload field
moved, pure read.
**Additively extended a fourth time:** 2026-08-17 — the **AI observer** API:
`AddEntityObserver` / `RemoveEntityObserver` / `HasEntityObserver` / `GetEntityObserverCount`,
requested by `virtualization/integration` Phase 6 (its D14/D16) so a parked recruit squad can pull
dormant registered groups awake with no player nearby. See [§3](#ai-observers) and
[§10](#integration--tracked-group-lifecycle-and-reclaim), and the dated entry in `context.md`. Same
discipline: four new methods and one new manager attribute, nothing renamed, re-signed or re-meant,
no payload field added or moved, `CONFIG_STREAM_VERSION` unmoved, and no existing call path changed
behaviour — the methods had no caller in core at all when they landed.
**Owner:** `virtualization/core`
**Consumers:** `civilians`, `movement`, `integration`, `base-defense-migration` — see
[§10](#10-entry-points-per-sibling-feature) for the exact surface each of them programs against

This is the epic's contract. Every signature below is what the code actually declares today. If you
need something that is not here, ask for a core change — do not reach around the manager into
`SCR_AIGroup` lifecycle calls, because core's survivor mask and the engine's counts must never
disagree (see [D2](#d2--slot-accurate-survivor-truth-the-contract-that-matters)).

### What "frozen" means

Nothing on this page changes without a recorded breaking-change note in
`docs/features/virtualization/core/context.md`. Siblings may plan, review and merge against it as
written. **Additive** change (a new method, a new appended payload field behind a version bump) is
allowed; renaming, re-signing or re-meaning anything here is not.

### The five things a consumer must not get wrong

1. **The mask is the roster truth, not the engine's counts** — D2, and it only works because
   Overthrow's modded `SCR_AIGroup` overrides `ExpandOneMember`. Never read
   `GetDormantAliveCount()`, never call `SetDormantCounts()`, never spawn or despawn a registered
   group through `SCR_AIGroup` directly. [→ D2](#d2--slot-accurate-survivor-truth-the-contract-that-matters)
2. **Persistence is Route B: core's registry payload IS the truth.** Vanilla persists *nothing*
   about these groups; the group entities and their waypoints are re-created by core on load, and
   `m_bPersistGroupEntities` must stay **false**. [→ §8](#8-persistence--what-is-and-is-not-in-a-save)
3. **`SetEliminateWhenReached` is only ever stamped post-wipe**, never at registration — with it set,
   an observer inside 150 m of a dormant group can make the engine *delete* it.
   [→ D2 finding 2](#d2--slot-accurate-survivor-truth-the-contract-that-matters)
4. **"Nearby" means the engine's observers**, which include cameras, MP inserts and the optics far
   observer — not players. Never mix in a player-distance loop, and **never call
   `ObserversSystem.InsertObserverSP()` with a null entity** (it hard-freezes the client).
   [→ D2 finding 3](#d2--slot-accurate-survivor-truth-the-contract-that-matters)
5. **`spawnDistanceOverride == 0` is the `Manual` policy** (never materialise by proximity), and
   **force is a nudge, not a pin** — `ForceSpawn` can materialise nobody and the next lifecycle tick
   can despawn what it did. Since 2026-08-17 the policy is enforced at spawn-queue dispatch too: a
   masked Manual group refuses every dispatch core did not arm (a prefab's `m_bSpawnImmediately`
   init request would otherwise materialise it for any observer). `ForceSpawn` arms the exception.
   [→ §3](#3-tracked-groups--persisted-engine-lifecycle)

---

## 1. Access

```c
OVT_VirtualizationManagerComponent virt = OVT_Global.GetVirtualization();
if (!virt) return;   // null before the game mode exists, or if the component is off the prefab
```

**Server only.** Every entry point is guarded by `Replication.IsServer()` and the record map is only
allocated on the authority (`OnPostInit`). On a client every call fails closed: `RegisterGroup`
returns `-1` with a WARNING, queries return empty/0/null. Nothing in this feature replicates, and
nothing has a client half — if a consumer needs the client to know something, that is the consumer's
replication, not core's.

---

## 2. Types

```c
enum OVT_EVirtualWaypointType
{
    MOVE,
    PATROL,
    WAIT,
    DEFEND,
    CYCLE,
    SEARCH   // appended 2026-08-21 (occupying/deployments town sweep) - ADDITIVE, at the end; persisted plans are unaffected
}

//! Registration-time INPUT ONLY. Core turns it into real AIWaypoint entities owned by the record.
class OVT_VirtualWaypointPlan
{
    ref array<vector> m_aPositions = {};
    ref array<int> m_aTypes = {};       //!< OVT_EVirtualWaypointType, parallel to m_aPositions
    ref array<float> m_aParams = {};    //!< wait seconds / patrol radius, parallel to m_aPositions
    bool m_bCycle;
}

class OVT_VirtualGroupRecord
{
    int m_iHandle;
    string m_sOwnerSystem;              //!< "deployment", "tower_garrison", "economy_delivery", ...
    string m_sOwnerKey;                 //!< your identity, used to reclaim after load

    string m_sFactionKey;               //!< "USSR" — NEVER a faction index (indices are positional)
    string m_sGroupRegistryName;        //!< "light_patrol"
    ResourceName m_rResolvedPrefab;     //!< fallback if the registry entry is renamed away

    int m_iSpawnDistanceOverride = -1;  //!< -1 = use the global config value
    int m_eImportance;                  //!< SCR_EAISpawnImportance stamped at registration

    vector m_vPosition;                 //!< registration position, kept in step with SetPosition()
    ref OVT_VirtualWaypointPlan m_Plan; //!< the plan the owned waypoints were built from

    ref array<int> m_aSlotAlive = {};   //!< THE roster truth — 1/0 per prefab slot

    SCR_AIGroup m_Group;
    EntityID m_GroupId;                 //!< m_Group's id — the safe handle (see below)
    ref array<AIWaypoint> m_aOwnedWaypoints = {};

    bool m_bDespawning;                 //!< true inside the engine's dormant-teardown window
}

//! One entry of core's member -> (handle, slot) reverse map. Internal; listed because it is in the
//! same file as the record.
class OVT_VirtualMemberSlot : Managed
{
    int m_iHandle;
    int m_iSlot;
}
```

**Phase 3 additions:** `m_vPosition`, `m_Plan`, `m_GroupId` and `m_bDespawning` were added to the
record. Nothing was removed or renamed, so no consumer written against Phase 2 breaks. `m_vPosition`
and `m_Plan` exist because a restored group needs an anchor and its waypoints rebuilt (see
[Persistence](#8-persistence--what-is-and-is-not-in-a-save)); `m_GroupId` exists because the engine
can delete a group entity with no callback, so **core re-resolves `m_Group` through
`FindEntityByID(m_GroupId)` on every touch** — do the same if you hold a group across frames, or just
call `GetGroup(handle)` again, which does it for you.

**The three arrays in a plan are PARALLEL and are consumed by index.** A ragged plan is refused
outright (`RegisterGroup` returns `-1`) rather than half-built — a short `m_aParams` would silently
give the last waypoint a default radius or duration. An **empty** plan (or `null`) is legal: a
garrison with no waypoints is a normal registration.

⚠️ Known upstream wrinkle: `SpawnWaitWaypoint(pos, time)` accepts a duration and never applies it
(`OVT_OverthrowConfigComponent.c:485`). Do not rely on `WAIT` durations until that is fixed.

---

## 3. Tracked groups — persisted, engine-lifecycle

### Registration

```c
//! Creates a group entity (unspawned), stamps ProximityDriven policy + distances + importance,
//! builds waypoint entities from the plan, registers for persistence, returns a handle.
//! Returns -1 on failure (unresolvable composition, ragged plan, not server, not initialised).
int RegisterGroup(string ownerSystem, string ownerKey, string factionKey, string groupName,
                  vector position, OVT_VirtualWaypointPlan plan = null,
                  int spawnDistanceOverride = -1, int importance = -1);

//! Deletes any live members, the group entity and its owned waypoints, removes the record.
//! Idempotent; false if the handle is unknown.
bool UnregisterGroup(int handle);
```

- `ownerSystem` / `ownerKey` are **free-form strings**, deliberately not an enum: a new class of
  virtual thing needs no core edit and no serializer version bump.
- Composition is `(factionKey, groupName)` resolved through
  `OVT_Global.GetFactions().GetOverthrowFactionByKey()` → `OVT_Faction.GetGroupPrefabByName()`.
  **Never pass a faction index** — indices are positional across saves.
- `spawnDistanceOverride`: `-1` = use `virtualizationSpawnDistance`; `0` = *never* materialise by
  proximity (a legitimate "stay virtual" registration); a huge value = effectively always spawned.
  **A resolved distance of 0 is stamped as the engine's `Manual` lifecycle policy, not as a 0 m
  ring** — `SetLifecyclePolicy` ignores non-positive distances, so a `ProximityDriven` group given 0
  would silently keep vanilla's 600/800 m defaults and materialise anyway.
- `importance`: `-1` = Overthrow's default (NORMAL). See [§5](#5-importance-tiers).
- Handles come from a monotonic counter that is itself persisted, so **a handle is never reused
  across a reload**. You may persist handles, but you do not have to — see `FindGroupsByOwner`.

**What a registration actually does**, in order: spawns the group prefab with
`SCR_AIGroup.IgnoreSpawning(true)` so it has **zero** members → `SetFaction` →
`SetLifecyclePolicy(ProximityDriven, resolvedSpawn, resolvedSpawn × 1.15, -1)` → `SetImportance` →
builds the plan into owned `AIWaypoint` entities → captures the prefab's slot list into an all-alive
survivor mask and hands that mask to the group → opts the group into persistence *if that is switched
on* ([§8](#8-persistence--what-is-and-is-not-in-a-save)) → subscribes the engine's
`GetOnMembersDespawning` and `GetOnEliminatedWhenReached`. Nothing is booked and no handle is burned
if the group entity cannot be built, so a `-1` never leaves a half-record behind.

**`SetEliminateWhenReached` is deliberately NOT stamped at registration** — see
[D2 finding 2](#d2--slot-accurate-survivor-truth-the-contract-that-matters). It is enabled in exactly
one place: on a group core has already declared dead (wiped, unregistered-while-held), as the
cleanup mechanism for the husk.

⚠️ **Registry coverage today:** shipped faction registries (e.g. `USSR_OverthrowData.conf`) contain
only `light_patrol` (2 slots) and `light_fireteam` (4 slots). Consumers wanting larger compositions
must add registry entries — core resolves whatever the faction defines and refuses anything else.

### Queries and reclaim

```c
bool IsRegistered(int handle);
int  GetGroupCount();
OVT_VirtualGroupRecord GetRecord(int handle);   //!< server-side read access; do not mutate

array<int> FindGroupsByOwner(string ownerSystem, string ownerKey);
array<int> FindGroupsBySystem(string ownerSystem);
array<int> GetAllHandles();   //!< every registered handle; MAP order — not stable, sort if you need one
int GetCurrentPlanIndex(int handle);   //!< plan index of the group's LIVE current waypoint; -1 when unknowable (see context.md 2026-08-17)
```

### State (safe while dormant, and safe when there is no entity at all)

```c
SCR_AIGroup GetGroup(int handle);
bool   IsSpawned(int handle);              //!< member characters exist in the world right now
int    GetAliveMemberCount(int handle);    //!< mask-first (see D2); engine counts only as fallback
int    GetMemberCount(int handle);         //!< full roster size (slot count)
bool   GetMemberAlive(int handle, int slotIndex);
vector GetPosition(int handle);            //!< group origin — valid dormant or spawned
void   SetPosition(int handle, vector position);  //!< `movement` writes here WHILE DORMANT only

int    GetSpawnDistance(int handle);       //!< the resolved ring this record would be stamped with
int    GetImportance(int handle);          //!< the tier it was registered with; -1 if unknown handle
int    GetGlobalSpawnDistance();           //!< virtualizationSpawnDistance, or the fallback
```

Every accessor is null-safe. "The record exists but the group entity does not" is a **legitimate
runtime state** (the engine can delete a group entity with no callback), so nothing here dereferences
the group unguarded and nothing you call can VME on it. `GetPosition` falls back to the record's last
known position when the entity is gone.

`IsSpawned()` asks **only** whether member characters exist right now. It deliberately does not
consult the engine's `IsDormant()`, because that flag is derived from the dormant counts core
overwrites from the mask (see [D2 finding 1](#d2--slot-accurate-survivor-truth-the-contract-that-matters)).

### Death accounting

```c
//! Records a member death BY ROSTER SLOT. Killing the last living slot wipes the record —
//! OnGroupWiped fires BEFORE the record is removed. Idempotent per slot.
void ReportMemberKilled(int handle, int slotIndex);
```

Core drives this internally from `OVT_OverthrowGameMode.GetOnCharacterKilled()`. It is public so
consumers (and the round-trip tests) can report deaths they observed themselves — that is the test
seam for "partially wiped group" cases, no world combat required.

### Lifecycle overrides

```c
void ForceSpawn(int handle);      //!< RequestSpawn regardless of proximity
void ForceDespawn(int handle);    //!< DespawnMembers regardless of proximity
```

**Force is a nudge, not a pin.** `ForceSpawn` enqueues a request on the engine's importance-ordered
spawn queue, which **re-validates observer proximity at dispatch time** — so a force-spawn far from
any observer may materialise nobody at all — and the group's own 1 Hz lifecycle tick will despawn it
again as soon as no observer is inside its despawn ring. If you need a group to stay materialised,
register it with a huge `spawnDistanceOverride` instead.

A group the mask reports **wiped** can never be force-spawned: its record is gone, and the engine
refuses the request anyway (dormant alive 0 → `RequestSpawn` returns immediately).

`ForceDespawn` reports a held member (in a vehicle, player-engaged) and despawns anyway — an explicit
despawn request is honoured. `UnregisterGroup` is the call that *respects* held members, by retiring
the group in place instead of deleting it out from under whatever is using it.

### AI observers

*(added 2026-08-17 at `virtualization/integration`'s request — the fourth additive change since the
freeze.)*

```c
//! Parks an engine observer that FOLLOWS an entity, so registered groups near it materialise with no
//! player anywhere near. Server-only, idempotent per entity, keyed internally by EntityID.
//! REFUSES a null entity with a WARNING and false — a null-entity InsertObserverSP hard-freezes the
//! client (context.md gotcha 0) and has zero vanilla script callers.
//! REFUSES an entity whose GetID() is EntityID.INVALID with a WARNING and false — see below.
bool AddEntityObserver(IEntity entity);
bool RemoveEntityObserver(IEntity entity);   //!< false for null / invalid id / unknown / client
bool HasEntityObserver(IEntity entity);      //!< core's map, NEVER the engine (see below)
int  GetEntityObserverCount();               //!< core's observers only, not GetObserversSP()
```

⚠️ **The entity you pass must exist in the world — an entity with `EntityID.INVALID` is refused.**
The map is keyed on `EntityID`, and an entity that is not world-registered answers `GetID()` with
`EntityID.INVALID` — a value **every** such entity shares. Keying on it would make two unrelated
entities share one map entry: the second add hijacks the first's key, `HasEntityObserver` answers
true for something nobody added, and removing either removes the other's observer. Measured, not
theorised (`integration` Phase 6's own Init case caught it). Practical consequence for a consumer:
**do not add the observer from inside the entity's own initialisation** — an `OnPostInit` on a
component of the entity being spawned runs one call deep inside the spawn that created it. Defer it
by one call-queue hop (`CallLater(..., 0, false)`) and cancel that hop in your `OnDelete`, which is
what the shipped recruit-group consumer does. The same registration is what makes the stale-entity
sweep work at all: it drops any observer whose id no longer resolves through `FindEntityByID`.

⚠️ **Engine application is deferred by exactly one frame, insert *and* remove** — measured by
`integration`'s Phase 1 gate case: *"server-side `InsertObserverSP(key, 0, 0, entity)` is HONOURED,
application DEFERRED (invisible same-frame, visible within the settling budget) | settled after 1
frame(s), removal settled after 1"*. So `HasEntityObserver` and `GetEntityObserverCount` answer from
core's own `map<EntityID, int>`, never by asking `ObserversSystem`: they are correct the instant the
add/remove returns, while the engine's own proximity queries lag a frame. **Never verify this API by
querying the engine** — right after an add you read a false negative, right after a remove a false
leak.

⚠️ **An observer is the most expensive call on this page.** It holds *every* registered group inside
its ring materialised, with its AI running, for as long as it exists — a squad parked in a town keeps
that town's tower garrison and its patrols awake whether or not anybody is watching. Give it an
off-switch. The one shipped consumer (parked recruit groups) is gated by the manager attribute
`m_bRecruitGroupsAreObservers` (default **true**), readable as `GetRecruitGroupsAreObservers()`;
that gate is deliberately consulted **by the consumer**, not inside `AddEntityObserver`, so one
consumer's knob cannot silently disable another's.

Keys are a namespaced monotonic counter, **never persisted and never replicated** (a key names an
entry in one machine's local observer table and nothing else). Stale entries — an entity destroyed
without its owner removing the observer — are swept by core's existing 2 s ambient tick, but that is
a backstop: **remove your own observer**, from the same place you clean up everything else about the
thing it was following. Core removes all of them at world teardown, because an SP observer left
behind survives a quit-to-menu and pins content awake for the rest of the process.

### Events

```c
ScriptInvoker GetOnGroupWiped();        //!< (int handle) — fired BEFORE the record is removed
ScriptInvoker GetOnRecordsRestored();   //!< () — fired once per load, after PostGameStart()
```

**Reclaim from `GetOnRecordsRestored()`, never from your own deserialize.** Serializer order is not a
contract: a consumer that reclaims at its own deserialize may run before core's, and would find an
empty registry. The invoker exists to remove that whole ordering-bug class.

**When it fires, exactly** (Phase 5):

1. Core's serializer deserializes; `ApplyPersistedRegistry()` re-creates every restorable group
   **synchronously**, so records and their group entities are queryable the instant that returns.
2. The announcement is then deferred by one call-queue hop, so every subscriber runs *after the whole
   deserialize pass*, not in the middle of it.
3. It additionally waits until `HasGameStarted()` — so on a continued campaign it fires **after
   `PostGameStart()`**, which is where consumers register their groups on a *new* campaign. The wait
   is bounded (~10 s) and expiry fires anyway; on an in-session re-apply the campaign is already
   started and the wait is a single frame.

It fires **once per load**, whether or not the payload had any records, and whether or not any of
them resolved. Consumers therefore always get exactly one reclaim point.

**The reclaim flow**, which is the whole reason handles do not have to be persisted by consumers:

```c
virt.GetOnRecordsRestored().Insert(OnRecordsRestored);
...
protected void OnRecordsRestored()
{
    OVT_VirtualizationManagerComponent virt = OVT_Global.GetVirtualization();
    if (!virt) return;

    foreach (string baseKey : GetMyPersistedBaseKeys())
    {
        array<int> handles = virt.FindGroupsByOwner(OWNER_SYSTEM, baseKey);
        if (handles.IsEmpty())
        {
            // Nothing came back for this key. Three causes, all of which mean the same thing to you:
            // the group was WIPED before the save, its composition no longer resolves, or this is a
            // brand-new campaign. Register a fresh one if you still want one.
            SpawnGarrison(baseKey, GetBasePosition(baseKey));
            continue;
        }

        m_mHandlesByKey.Set(baseKey, handles[0]);
    }
}
```

⚠️ Because core re-creates its groups from its own payload, **`PostGameStart()` on a continued
campaign runs on a registry that is already populated.** If your consumer registers in
`PostGameStart()`, check `FindGroupsByOwner` first or you will register a second copy of every group
— core's own debug affordance does exactly that check.

### Spawn/despawn notification — subscribe on the GROUP, not the manager

Core deliberately does not wrap these; the engine already publishes them per group.

```c
SCR_AIGroup group = OVT_Global.GetVirtualization().GetGroup(handle);
if (group)
{
    group.GetOnMembersDespawning().Insert(OnMyGroupDespawning);   // SCR_AIGroup.c:2288, invoked :2894
    group.GetOnAgentAdded().Insert(OnMyMemberSpawned);
}
```

⚠️ **Do not gate on `Event_OnInit`.** In 1.8 it fires only on a COMPLETE group fill, which under
budget pressure may never happen. `GetOnAgentAdded` fires per member; `GetOnMembersDespawning` fires
on every teardown, including ordinary dormancy — it is not a wipe signal. Use `GetOnGroupWiped()`
for wipes.

---

## 4. Ambient spawn sources — not persisted, re-rolled

Ambient entities are loose `IEntity`s (civilians, parked vehicles), not groups, so the engine's
*group* lifecycle cannot drive them. Core gives them the one tick it owns, still asking the engine's
`ObserversSystem` rather than looping players.

```c
//! Registers a source at a position. Spawns NOTHING — the source is dormant until an observer
//! comes inside its spawn ring. Returns -1 if not server / not initialised.
int  RegisterAmbientSource(notnull OVT_AmbientSpawnSourceConfig config, vector position, string ownerKey);

//! Deletes the source's live entities (through OnEntityDespawning) and drops it. False if unknown.
bool UnregisterAmbientSource(int handle);
int  GetAmbientSourceCount();

//! A pruned COPY of the source's live entities; empty for an unknown handle.
array<IEntity> GetAmbientEntities(int handle);

//! OWNERSHIP TRANSFER. The entity stops being ambient: removed from its source's list, NOT deleted
//! on the next despawn. O(1). This is what `civilians` calls when a player recruits a civilian.
bool ReleaseAmbientEntity(notnull IEntity entity);

// -- convenience over the (optional, core-empty) authored registry --
OVT_AmbientSpawnSourceRegistry GetAmbientRegistry();
OVT_AmbientSpawnSourceConfig   FindAmbientSourceConfig(string name);
```

**Live as of Phase 4.** Ambient handles come from their own monotonic counter, separate from group
handles: ambient sources are never persisted, so they must not consume numbers from the persisted
group sequence. An ambient handle and a group handle can therefore both be `1` — they are different
namespaces, used through different calls.

### The config classes (the modder seam)

```c
[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sSourceName")]
class OVT_AmbientSpawnSourceConfig : ScriptAndConfig
{
    string m_sSourceName;
    ref array<ResourceName> m_aPrefabs;
    int   m_iMinCount;               //!< default 1
    int   m_iMaxCount;               //!< default 1, INCLUSIVE
    float m_fRadius;                 //!< default 100, scatter radius
    int   m_iSpawnDistanceOverride;  //!< default -1 = use virtualizationSpawnDistance; 0 = never spawn

    ResourceName RollPrefab();                                   //!< random element; Empty ends the activation
    int  RollCount();                                            //!< RollCountSafe(min, max) — inclusive, RandInt-safe
    vector RollPosition(vector origin, float radius);            //!< random non-ocean point near origin
    void OnEntitySpawned(IEntity entity, vector sourcePosition); //!< no-op — clothes, loadouts, waypoints
    void OnEntityDespawning(IEntity entity);                     //!< no-op

    // -- added 2026-08-17 for `civilians` (additive; existing sources unaffected) --
    bool IsEntityDead(IEntity entity);   //!< default: SCR_DamageManagerComponent state == DESTROYED
    void OnEntityPruned(IEntity entity); //!< no-op; fires AFTER the list AND reverse-map removals
}

[BaseContainerProps(configRoot: true), BaseContainerCustomTitleField("m_sRegistryName")]
class OVT_AmbientSpawnSourceRegistry : ScriptAndConfig
{
    string m_sRegistryName;
    ref array<ref OVT_AmbientSpawnSourceConfig> m_aSources;
    OVT_AmbientSpawnSourceConfig FindByName(string name);
    int GetSourceCount();
}
```

Subclass the config, override what you need, name the subclass in a `.conf`, point the manager's
`m_AmbientRegistry` attribute at it — **no core change**. `RollCount()` is rolled **once per
activation** and then spent across ticks, so an override that returns 20 gives you 20 entities over
several ticks, never 20 in one frame.

⚠ `RollCount()`'s default goes through `OVT_VirtualizationMath.RollCountSafe` because `RandInt` is
**max-exclusive** and `RandInt(n, n)` raises an engine error — a source authored with `min == max`
would otherwise error on every activation. Override it if you like, but keep that guard.

#### The two prune hooks (added 2026-08-17 for `civilians`, additive)

Both are called from the manager's prune pass, once per entity, and **only for entities that source
owns**. Neither existed before Phase 1 of `civilians`, and neither changes anything for a source that
does not override it.

```c
//! Is this entity finished — a corpse, a wreck — and no longer part of the live crowd?
//! DEFAULT: the damage-state check the manager used to do inline
//! (SCR_DamageManagerComponent.GetState() == DESTROYED), unchanged for every existing source.
bool IsEntityDead(IEntity entity);

//! Called after a pruned entity has been removed from the source's list AND its reverse-map entry.
void OnEntityPruned(IEntity entity);
```

**Why `IsEntityDead` has to be overridable.** The default can only see an entity that carries an
`SCR_DamageManagerComponent`. A source whose *tracked* entity is an `SCR_AIGroup` — which is what
`civilians` registers, because waypoints and `GetOnAgentAdded` attach to groups — has no damage
manager on the thing being tracked, so the default answers `false` forever and a dead civilian would
never be pruned. Such a source overrides it with its own predicate (for a group: *I have seen an
agent* **and** *the agent count is now 0*; the seen-an-agent half is mandatory, because member
spawning goes through the engine's queue and a freshly built group is legitimately memberless for one
or more frames). The manager keeps its own copy of the default as the fallback for the one state that
has no config to ask.

**The ordering of `OnEntityPruned` is load-bearing and is part of the contract.** Both removals — out
of the source's entity list *and* out of the manager's entity→source reverse map — happen **before**
the hook runs, so a hook can never be handed an entity core still thinks it owns. Two consequences:

- **The entity is no longer owned.** The source will never delete it, `GetAmbientEntities()` no longer
  counts it, and the next despawn will not touch it.
- **`ReleaseAmbientEntity()` on it is a no-op** and answers `false`. There is nothing left to release;
  do not read that `false` as a failure.

> **Leave the body, delete the companions.** `OnEntityPruned` is where a consumer deletes the things
> that only existed to serve the pruned entity — its waypoints, an emptied group husk — while
> deliberately leaving **the entity itself** in the world. Core prunes a dead ambient entity instead
> of deleting it because a corpse a player may be looting is worth more than a tidy entity count, and
> a consumer's cleanup must not undo that.

A hook that mutates the source's own entity list (deleting a companion that is itself ambient, or
calling `ReleaseAmbientEntity`) is tolerated — the prune walk pulls its cursor back inside the array —
but an entity displaced that way is simply re-examined on the next pass, not lost.

### How the tick behaves

- One `CallLater` at `m_iAmbientTickIntervalMs` (2000 ms), started on the server in `PostGameStart`
  and also (idempotently) by the first registration. **It is the only `CallLater` in the feature** —
  tracked groups are the engine's job.
- Each tick evaluates a **round-robin slice** of `m_iAmbientSourcesPerTick` (8) sources
  (`OVT_VirtualizationMath.SliceIndices` / `AdvanceCursor`), so no source can starve: 40 sources are
  each evaluated once per 10 s.
- Proximity is `ObserversSystem.HasObserverWithinRangeSq(x, z, rangeSq)` — the **engine's observer
  set**, not a player loop. **Spawn ring** = the config's override, or `virtualizationSpawnDistance`.
  **Despawn ring** = spawn × `m_fAmbientDespawnHysteresis` (1.15); the gap is the anti-thrash band,
  so an observer must leave further out than they came in. A resolved spawn ring of `0` means the
  source never materialises by proximity.
- An activated source builds at most `m_iAmbientSpawnsPerTick` (3) entities **per tick**.
- Each evaluation first **prunes** dead/deleted entities out of the source's list (a shot civilian
  stops being counted and its body is left alone, not deleted). "Dead" is the config's
  `IsEntityDead()`; each pruned entity then gets one `OnEntityPruned()` call, after both removals —
  see [the two prune hooks](#the-two-prune-hooks-added-2026-08-17-for-civilians-additive).
- `ReleaseAmbientEntity` re-verifies the hit against the source's own entity list before honouring
  it, so a recycled `EntityID` can never release an entity that was never ambient.
- Releasing from **inside** `OnEntityDespawning` is supported and is the "save this one" escape
  hatch: the hook runs while the entity is still owned, and a release there cancels the delete.
- Manager attribute `m_bDebugAmbientLogging` (default **false**) logs every activation, per-tick
  spawn batch and despawn with counts — the play-test view of "a source of 20 fills over several
  ticks".
- Observers include cameras, MP inserts and the optics far-observer (see
  [D2 finding 3](#d2--slot-accurate-survivor-truth-the-contract-that-matters)): a source near
  campaign start may stay permanently activated, and a source can activate with no player near it.

Nothing ambient appears in any save file, by construction. Despawn discards state; the next approach
**re-rolls** from config. Core ships **zero** authored ambient sources — `civilians` authors content.

---

## 5. Importance tiers

Vanilla defaults every group to `LOW` (0.50 budget cap, first evicted) and stamps *player* groups
`CRITICAL`. An unstamped Overthrow hostile is therefore budget-starved exactly when a player's
recruit squads are saturating the budget. **Every core registration stamps a tier.**

| Tier | Cap | Use for |
|---|---|---|
| `SCR_EAISpawnImportance.CRITICAL` | 1.00 | Mission-critical / player-led AI. Core registrations should not need this. |
| `SCR_EAISpawnImportance.HIGH` | 0.90 | Base defenders, tower/base garrisons, QRF targets — content the player came to fight. |
| `SCR_EAISpawnImportance.NORMAL` | 0.70 | **Core's default.** Scripted patrols, town garrisons, remnants. |
| `SCR_EAISpawnImportance.LOW` | 0.50 | Ambient feel and filler — civilians, decorative traffic. |

Pass `-1` to take the default. An out-of-range value falls back to the configured default, and a
nonsense configured default falls back to `NORMAL` — a registration can never silently land on `LOW`.

---

## 6. Configuration

`$profile:Overthrow_Config.json` → `OVT_OverthrowConfigStruct`:

```json
{ "virtualizationSpawnDistance": 1750 }
```

Server-side only; it is deliberately absent from the config's JIP bitstream, so no client ever reads
it and `CONFIG_STREAM_VERSION` did not move. Takes effect on the next campaign start, no code edit.
A very large value keeps every registered group spawned permanently (issue #100's ask); a small one
despawns everything not adjacent. Per-registration overrides always beat it.

It **replaced** `m_iMilitarySpawnDistance` outright. **No system reads `m_iMilitarySpawnDistance` any
more. The attribute itself was deleted in `base-defense-migration` Phase 7 (2026-08-18)** together
with the base-upgrade spawners that were its last readers, and `grep -rn "m_iMilitarySpawnDistance"
Scripts/` returns nothing. Deployments (town patrols, vehicle patrols, the nine base-defence configs)
and radio-tower garrisons ride this value instead, or a per-registration override. The one remaining
un-migrated system, the **QRF spawn queue**, reads neither: it spawns off its own trigger and its own
ranges (`OVT_QRFControllerComponent.c`), and is an epic-level exclusion rather than core's problem.
*(This paragraph is a factual correction to a stale statement, made under `base-defense-migration`
T8.4; no signature or contract in this document changed, api.md remains frozen.)* (Its civilian
counterpart was retired on
2026-08-17 by `civilians` Phase 2 along with the town-civilian spawner — ambient civilians ride this
value through their source's `m_iSpawnDistanceOverride`, authored as `-1`.)

---

## D2 — Slot-accurate survivor truth (the contract that matters)

> **A group that lost 3 of 8 comes back with exactly its 5 surviving slots — the same roles and
> loadouts — across despawn AND across save/load. A group that lost all 8 never returns.**

This is the promise core exists for, and it is *not* what the engine gives you.

**What the engine does.** Dormancy stores alive/dead **counts**, and vanilla refill always spawns
`slotIndex == current agent count` (`SCR_AIGroup.c:2731`) — a first-N refill. A group that lost only
its slot-1 machinegunner comes back at the right *strength* with the MG alive and a tail rifleman
missing instead. Identity is structurally lost.

**What core does.** The record carries `m_aSlotAlive`, one flag per prefab roster slot, captured at
registration. Deaths are recorded **by slot** through the game-mode kill invoker, and Overthrow's
modded `SCR_AIGroup` overrides `ExpandOneMember` to spawn the next *mask-alive, not-yet-materialised*
slot (unregistered groups keep vanilla behaviour). `GetAliveMemberCount()` answers from the mask
whenever the mask is populated.

### Four engine findings consumers must know (Phase 1 spike + Phase 6)

1. **The engine's dormant counts corrupt themselves — the despawn-mid-fill ratchet.** ANY despawn
   during an in-progress refill records the not-yet-spawned slots as **dead**
   (`SCR_AIGroup.c:2876`), and the ratchet is permanent (refill capacity is
   `totalSlots - dormantDead`, `:2726-2729`; nothing engine-side re-corrects it). Observed live: a
   6-man group ratcheted 6→4→2 **with zero kills**. Core therefore re-asserts
   `SetDormantCounts(maskAlive, maskDead)` from the mask after every despawn. **Consequence for
   you: never read `GetDormantAliveCount()` yourself — ask core.** The mask is the only roster
   truth; the engine's counts are scratch.

2. **`SetEliminateWhenReached` is NOT stamped at registration.** With it set, an observer inside
   `veryNearBlockDist` (default 150 m) of a dormant group can cause the **group entity to be deleted
   outright** (`:3038`, `:3045-3059`) — unrecoverable loss for a persistent campaign group. Core
   enables it only once the mask reports the group wiped, as the cleanup mechanism for an already-
   dead record. Wipe removal is driven by the mask, not by the engine's elimination signal.

3. **"Nearby" means observers, and observers are not only players.** Proximity is
   `HasObserverInRange` from the group entity origin (linear metres), and the observer set includes
   local cameras, connected players, fixed MP inserts and the optics far-observer. Two parked
   observers exist in this build: the deploy-point spawn preload inserts a **fixed MP observer at the
   deploy point** (`SCR_SpawnRequestComponent.c:541`) which is only removed in its destructor, and
   Overthrow's own start camera. **Anything registered near campaign start may never go dormant.**
   Choose spawn distances with that in mind, and never mix core's proximity semantics with your own
   player-distance loops.

   ⚠️ **Never call `ObserversSystem.InsertObserverSP(key, x, z, null)`** — a null-entity SP observer
   insert has zero vanilla callers and **hard-froze the game client** when a test case tried to park
   one (the main thread stopped dead; `context.md` gotcha 0). If you need a synthetic observer, give
   it a real entity, or arrange for a real one to be there.

4. **Driving past a dense area materialises almost nothing, and that is the engine's doing.** A
   dormant group enqueues a spawn request when an observer enters its spawn ring, and the queue
   **re-checks observer proximity at dispatch time** — a vehicle at speed is already out of the ring
   by the time its requests come up, so they are dropped rather than serviced. Core adds no
   drive-past handling of its own and consumers need none: do not build "am I moving too fast to
   spawn" logic on top of this. The only thing that survives a fast pass is a group whose ring you
   are still inside when the queue reaches it.

---

## 7. Worked example — register → reclaim after load → unregister

```c
class OVT_MyGarrisonConsumer
{
    protected const string OWNER_SYSTEM = "my_garrison";

    protected ref map<string, int> m_mHandlesByKey;   // my key -> core handle

    //------------------------------------------------------------------------------------------
    void Init()
    {
        m_mHandlesByKey = new map<string, int>();

        OVT_VirtualizationManagerComponent virt = OVT_Global.GetVirtualization();
        if (!virt) return;

        // Reclaim happens HERE, never in my own deserialize.
        virt.GetOnRecordsRestored().Insert(OnRecordsRestored);
        virt.GetOnGroupWiped().Insert(OnGroupWiped);
    }

    //------------------------------------------------------------------------------------------
    //! Registers one garrison. `baseKey` is MY identity for it and is all I have to persist.
    void SpawnGarrison(string baseKey, vector position)
    {
        OVT_VirtualizationManagerComponent virt = OVT_Global.GetVirtualization();
        if (!virt) return;

        OVT_VirtualWaypointPlan plan = new OVT_VirtualWaypointPlan();
        plan.m_aPositions.Insert(position);
        plan.m_aTypes.Insert(OVT_EVirtualWaypointType.DEFEND);
        plan.m_aParams.Insert(50);          // defend radius — parallel arrays, same length

        int handle = virt.RegisterGroup(OWNER_SYSTEM, baseKey, "USSR", "light_patrol",
            position, plan, -1, SCR_EAISpawnImportance.HIGH);

        if (handle == -1)
        {
            Print("[MyGarrison] registration refused for " + baseKey, LogLevel.WARNING);
            return;
        }

        m_mHandlesByKey.Set(baseKey, handle);
    }

    //------------------------------------------------------------------------------------------
    //! After a load: core has re-linked its records, so my keys resolve back to handles.
    protected void OnRecordsRestored()
    {
        OVT_VirtualizationManagerComponent virt = OVT_Global.GetVirtualization();
        if (!virt) return;

        foreach (string baseKey : GetMyPersistedBaseKeys())
        {
            array<int> handles = virt.FindGroupsByOwner(OWNER_SYSTEM, baseKey);
            if (handles.IsEmpty())
            {
                // The group was wiped before the save, or its composition no longer resolves.
                SpawnGarrison(baseKey, GetBasePosition(baseKey));
                continue;
            }

            m_mHandlesByKey.Set(baseKey, handles[0]);
        }
    }

    //------------------------------------------------------------------------------------------
    //! Fired BEFORE the record is removed, so GetRecord(handle) still answers here.
    protected void OnGroupWiped(int handle)
    {
        // NB: `owned` is a RESERVED EnforceScript keyword — never name a local that.
        foreach (string baseKey, int ownedHandle : m_mHandlesByKey)
        {
            if (ownedHandle != handle) continue;

            m_mHandlesByKey.Remove(baseKey);
            OnBaseGarrisonDestroyed(baseKey);
            break;
        }
    }

    //------------------------------------------------------------------------------------------
    void RemoveGarrison(string baseKey)
    {
        OVT_VirtualizationManagerComponent virt = OVT_Global.GetVirtualization();
        if (!virt) return;

        int handle;
        if (!m_mHandlesByKey.Find(baseKey, handle)) return;

        virt.UnregisterGroup(handle);      // deletes members, group entity and owned waypoints
        m_mHandlesByKey.Remove(baseKey);
    }
}
```

Note what the consumer does **not** do: no proximity check, no spawn/despawn call, no member
counting, no waypoint cleanup, and no persistence of handles or group state.

---

## 8. Persistence — what is and is not in a save

You do not have to do anything here beyond reclaiming from
[`GetOnRecordsRestored()`](#events). This section exists because the mechanism is unusual, and because
`movement` and `integration` both sit on top of it.

> **ROUTE B (Phase 5, shipped).** Core persists **complete re-creation state** for every registered
> group and **rebuilds the group entities itself** on load. Vanilla persists *nothing* about them.
> There is no group UUID and no `WhenAvailable` relink, and **no core-owned AI record is ever
> self-spawned back**: vanilla `AIGroup` / `AIUnit` / `AIWaypoint` records *are* written into the save
> (`Overthrow.conf` stamps them `SelfSpawn 0`), and nothing ever rebuilds an entity from one.
>
> *(Corrected 2026-08-17 by `integration` Phase 8 from the earlier wording "no vanilla `AIGroup` /
> `AIUnit` / `AIWaypoint` record for anything core owns", which a hand-decoded save disproved:
> `savepoint016` carries 27 `AIWaypoint` records and a pre-feature Eden save carries 342. Records
> existing is expected and required; a record being re-instantiated is what never happens. See
> `integration/context.md` T7.7 finding 5.)*

### Why it is done this way (the Phase 1 + Phase 3 findings, kept because they still bind)

Overthrow untracks **every** AI group, member and prefab waypoint unconditionally
(`Scripts/Game/Modded/SCR_AIGroup.c`, the BUG-118 fix): garrisons, patrols, QRFs, deployments and town
civilians are all rebuilt from manager state on every boot, and their records could never be claimed or
deleted — measured at ~490 permanently orphaned records per idle restart.

The original design let vanilla's `SCR_AIGroupSerializer` bring registered groups back and had core
relink by UUID. That needs the group entity to carry `SelfSpawn`, and `SelfSpawn` can only be granted
by a `.conf` rule matched with **engine-native** matchers — a scripted `PersistenceConfigRule.IsMatch`
is never consulted (measured, BUG-018), and the per-instance alternative
`OVT_PersistenceTracking.MarkForSelfSpawn` is banned outright as save-corrupting (BUG-116). None of the
native matchers can name "this runtime-spawned instance":

| Matcher | Why it does not work here |
|---|---|
| `PrefabPersistenceConfigRule` | Registered groups spawn from faction-registry prefabs that every legacy Overthrow spawner also uses — and the set is mod-extensible, so it cannot be enumerated in a `.conf`. |
| `ComponentClassPersistenceConfigRule` | Would need an Overthrow component on the group prefab; components cannot be added at runtime, and core does not own the prefabs. |
| `EntityClassPersistenceConfigRule "AIGroup"` | The narrowest available, and a **superset**. Overthrow's untrack retry queue *gives up* after 60 s (`OVT_PersistenceManagerComponent.TRANSIENT_UNTRACK_MAX_ATTEMPTS`), and its own comment records that such an entity "will produce a record the next save". Granting `SelfSpawn` class-wide would turn those harmless orphan records into **duplicated garrisons and patrols on every load**. |

Also unresolved on that route: vanilla's `SCR_AIGroupSerializer` overrides `SerializeSpawnData` to
write only `version` + `prefab` and never calls `SerializeSpawnDataNative`, so it was never established
that the group's **transform** reaches the payload at all.

Route B has none of those unknowns, so that is what shipped.

### What is in the payload

`OVT_VirtualizationManagerSerializer`, bound in the game-mode `ComponentSerializers` block of
`Configs/Systems/Persistence/Overthrow.conf`. Per record:

| Field | Why the rebuild needs it |
|---|---|
| `handle` | The match key. Records are re-applied by handle, in place. |
| `ownerSystem`, `ownerKey` | `FindGroupsByOwner` after the load — your reclaim. |
| `factionKey`, `groupRegistryName`, `resolvedPrefab` | The three-step composition resolution, below. **Never a faction index.** |
| `spawnDistanceOverride`, `importance` | Re-stamped onto the re-created group; a lost tier means a budget-starved garrison. |
| `position` | Read from the **group entity origin at save time**, not the registration position — `movement` advances dormant groups, so the registration position goes stale. |
| `slotAlive` | THE roster truth (D2). One entry per prefab slot. |
| waypoint positions / types / params / cycle | The plan the owned waypoints are rebuilt from. |

Plus the **group handle counter**, so a handle is never reused across a reload. (The *ambient* handle
counter is deliberately a separate, never-persisted sequence.)

### What deliberately never persists

- **Member characters.** They stay untracked. The roster truth is the per-slot mask, not a character
  record each; persisting both would double-count the roster on load.
- **Owned waypoint entities.** They are tracked (their prefabs carry the native `Persistence`
  component) but `SelfSpawn 0` applies to them, and it **must** — every legacy Overthrow spawner
  creates waypoints through the same helpers, so a waypoint self-spawn rule would resurrect every
  garrison and patrol waypoint in the save. They are rebuilt from the stored plan.
- **Ambient spawn sources.** Nothing ambient is ever persisted, by construction: a despawn discards the
  roll and the next approach re-rolls from config.
- **The group entity itself.** The Phase 3 tracking-exemption seam exists
  (`SCR_AIGroup.ArmPersistenceExemption`, the modded `SCR_AIGroupSerializer` load-path arm) but is
  **dormant**: the manager attribute `m_bPersistGroupEntities` stays `false`. Turning it on under
  Route B only writes orphan records — core re-creates its groups either way.

### What happens on load

1. `ApplyPersistedRegistry()` runs from the serializer, **synchronously**. Records and their group
   entities are queryable the moment it returns.
2. Live records the payload does **not** claim are `UnregisterGroup`-ed — entity and waypoints
   included. Same contract as `ApplyPersistedDeployment`.
3. Each payload record is matched **by handle**: an existing one is updated in place; a missing one is
   re-created through the same builder `RegisterGroup` uses, so a restored group carries the identical
   policy, distances, importance, waypoints and mask a fresh registration would.
4. The survivor mask is refilled **in place** (the modded group holds that exact array by reference)
   and re-pushed, and the engine's dormant alive/dead counts are asserted from it — so a group that
   was saved at 5 of 8 asks the spawn queue for **5** the moment an observer arrives, not 1 and not 8.
5. The handle counter is restored, clamped up to the payload's own highest handle + 1.
6. `GetOnRecordsRestored()` fires once, after `PostGameStart()` — see [Events](#events).

**A record whose mask is all-dead is never re-created.** Wiped stays wiped (F5), and that is asserted
by a round-trip test of its own.

### When a composition no longer resolves (mod removed, registry entry renamed)

Three steps, in order: **faction key** → **group registry name** → **stored `resolvedPrefab`**. A
fallback to the stored prefab logs a WARNING naming the key. All three failing **drops the record**
with a WARNING naming the key — never resurrects it as something else.

If the prefab still resolves but its **roster changed size**, the mask is resized against the new
roster: the overlap keeps its saved truth, a roster that grew gains **dead** slots (a group must never
come back stronger because someone edited a prefab), and a roster that shrank loses its tail. If that
leaves nothing alive, the record is dropped as the wipe it now is. Every resize logs a WARNING.

### Coverage

Two cases on the shared round-trip gate (`OVT_TEST_PersistenceRoundTripSuite`, the All group):
`..._VirtualGroups_SurviveSaveAndReload` (a partially wiped group is destroyed after the save and has
to be **re-created** from the payload, with the right slot still dead, at the right place, with the
right stamps and plan) and `..._VirtualGroupsWiped_DoNotComeBack` (a wiped group stays wiped, and a
group registered under its owner key afterwards is removed by the restore).

⚠️ The automated gate covers **save → dirty → re-apply in-session**. The real
quit-to-menu-and-Continue path is a manual play-test item (implementation.md §6 step 12) — the autotest
harness restarts the whole suite on a world transition, so no case can survive one.

---

## 9. The modded `SCR_AIGroup` surface

Core adds these to Overthrow's already-modded `SCR_AIGroup`. **Consumers should not call them** — the
manager keeps them and the record in step, and calling them directly is how the mask and the engine's
counts start disagreeing. They are documented because `movement` and the registry restore read them.

```c
static void ArmPersistenceExemption(bool exempt);   //!< one-shot, consumed by the next group's EOnInit
bool IsOVTPersistenceExempt();

void SetOVTSlotMask(array<int> mask);   //!< stored BY REFERENCE — the record and the group share one array
void ClearOVTSlotMask();
array<int> GetOVTSlotMask();
bool HasOVTSlotMask();                  //!< false ⇒ every override below is vanilla, byte for byte
array<int> GetOVTSpawnedSlots();        //!< slots materialised during the CURRENT activation

void ReassertOVTDormantCounts();        //!< SetDormantCounts(maskAlive, maskDead); no-op before the
                                        //!< engine has ever recorded counts
```

⚠️ That last no-op is why the **registry restore writes the counts directly** rather than calling it:
a re-created group has never been dormant (the engine's count is the `-1` "never despawned" sentinel)
*and* has no members, which is the one case where writing the counts is both safe and necessary — see
[§8, "What happens on load"](#what-happens-on-load).

Overridden engine seams, all of which fall through to vanilla when no mask is set:

| Override | What changes with a mask |
|---|---|
| `ExpandOneMember()` | Spawns the next **mask-alive, not-yet-materialised** slot instead of `slotIndex == agent count`. |
| `IsExpandComplete()` | Answers from the mask, so the engine's queue can tell "at capacity" from "retry". |
| `SpawnMembers()` | Materialises the mask-alive slots (the non-Chimera fallback path would otherwise fill 0..n-1). |
| `DespawnMembers()` | Runs vanilla's bookkeeping, then re-asserts the dormant counts from the mask. |
| `EOnInit()` | Skips the BUG-118 untrack when the exemption was armed. |
| `AddAIEntityToGroup()` | Publishes member → slot to the manager as each member materialises. |

---

## 10. Entry points per sibling feature

The frozen surface, split by who consumes what. Anything not listed for your feature is not part of
your contract — if you need it, that is a core change request, recorded in `context.md`.

### `civilians` — the ambient half, plus release

| Call | Why you need it |
|---|---|
| `int RegisterAmbientSource(config, position, ownerKey)` | One source per town crowd / spawn point. Registration spawns nothing; the tick activates it. |
| `bool UnregisterAmbientSource(handle)` | Town depopulated, or your own teardown. Deletes live entities through `OnEntityDespawning`. |
| `int GetAmbientSourceCount()` / `array<IEntity> GetAmbientEntities(handle)` | Counts and a **pruned copy** of the live crowd (corpses are already out of it). |
| `bool ReleaseAmbientEntity(entity)` | **The recruitment path.** The civilian stops being ambient and survives the next despawn. O(1). Also valid from *inside* `OnEntityDespawning` as the "save this one" hatch. |
| `OVT_AmbientSpawnSourceConfig` subclass — `RollPrefab` / `RollCount` / `RollPosition` / `OnEntitySpawned` / `OnEntityDespawning` | Where all your content lives. Core ships the classes and **zero** authored sources. |
| …plus `IsEntityDead` / `OnEntityPruned` (added 2026-08-17 at your request) | Your tracked entity is a **group**, which carries no damage manager — override the predicate, or your dead civilians are never pruned. `OnEntityPruned` is where you delete the pruned civilian's waypoints and its emptied husk and **leave the body**. |
| `GetAmbientRegistry()` / `FindAmbientSourceConfig(name)` | Read authored sources out of a `.conf` you point `m_AmbientRegistry` at. |

Rules that bind you: ambient state is **never persisted** and a despawn **re-rolls** — do not build
anything that assumes the same civilian comes back. `RollCount()` is rolled once per activation and
spent over ticks. Keep the `RollCountSafe` guard if you override it. Ambient handles are a separate
namespace from group handles. A source near campaign start may stay permanently activated
([D2 finding 3](#d2--slot-accurate-survivor-truth-the-contract-that-matters)).

### `movement` — dormant-group position writes

| Call | Why you need it |
|---|---|
| `vector GetPosition(handle)` | Group origin. Valid dormant **and** spawned; falls back to the record's last known position when the entity is gone. |
| `void SetPosition(handle, position)` | **Your only write.** Meaningful **while dormant only** — moving a group whose members exist relocates the record entity, not the men. |
| `SCR_AIGroup GetGroup(handle)` | The engine record itself, for reading state you cannot get from the manager. Can be null: "record exists, entity does not" is legal. |
| `bool IsSpawned(handle)` | Gate your tick: advance dormant groups, leave materialised ones to their waypoints and their AI. |
| `OVT_VirtualGroupRecord.m_aOwnedWaypoints` (via `GetRecord`) | The waypoint entities core owns for that group. **Read them; do not delete them** — core's unregister/wipe path is what deletes them (D6). |
| `array<int> FindGroupsBySystem(ownerSystem)` | Iterate the set you are advancing without holding your own list. |
| `array<int> GetAllHandles()` (added 2026-08-17 at your request) | Iterate every registered group — owner systems are free-form strings, so `FindGroupsBySystem` cannot enumerate them. The order is the registry map's and is **not stable**: a round-robin over it can starve a handle, so sort what you get back. |
| `int GetCurrentPlanIndex(handle)` (added 2026-08-17, play-test fix) | The plan index of the group's **live** current waypoint, matched by entity identity against the owned waypoints (cycle entity → 0). Use it on live→dormant adoption: on a route whose legs coincide, position-only projection cannot recover direction. `-1` = unknowable — fall back to your own heuristic. |

Plan semantics you must respect: a `OVT_VirtualWaypointPlan` is **registration-time input only** —
the `AIWaypoint` entities built from it are the persistent truth, and the plan is re-saved verbatim.
Core does not re-derive waypoints when you move a group, so a group you move away from its patrol
route still owns waypoints pointing at the old route; decide deliberately whether to re-register or
to leave them. Positions written here are what `SnapshotRegistry` persists (it reads the live entity
origin, not the registration position), so a moved group loads back where you left it.

### `integration` — tracked-group lifecycle and reclaim

| Call | Why you need it |
|---|---|
| `int RegisterGroup(ownerSystem, ownerKey, factionKey, groupName, position, plan, spawnDistanceOverride, importance)` | Every migrated garrison / patrol / QRF. `-1` on refusal, never a half-record. |
| `bool UnregisterGroup(handle)` / `bool IsRegistered(handle)` / `int GetGroupCount()` | Teardown and bookkeeping. Unregister respects held members by retiring the group in place. |
| `array<int> FindGroupsByOwner(ownerSystem, ownerKey)` / `FindGroupsBySystem(ownerSystem)` | **Reclaim.** You never have to persist handles. |
| `ScriptInvoker GetOnRecordsRestored()` | **The one reclaim point.** Never reclaim from your own deserialize. Fires once per load, after `PostGameStart()`. |
| `ScriptInvoker GetOnGroupWiped()` (`int handle`) | Your base/tower/deployment learns its garrison is gone. Fires **before** the record is removed, so you can still read it. |
| `int GetAliveMemberCount(handle)` / `GetMemberCount(handle)` / `bool GetMemberAlive(handle, slot)` | Strength and identity. Mask-first — this is how you drive "how defended is this base?". |
| `void ReportMemberKilled(handle, slot)` | Only if you observe a death core did not (core already hooks the game-mode kill invoker). |
| `void ForceSpawn(handle)` / `ForceDespawn(handle)` | Escape hatches. **Nudge, not pin** — see [§3](#lifecycle-overrides). |
| `int GetSpawnDistance(handle)` / `GetImportance(handle)` / `GetGlobalSpawnDistance()` | Diagnostics and UI/debug readouts. |
| `bool AddEntityObserver(entity)` (added 2026-08-17 at your request) | Park an observer that **follows that entity**, so registered groups near it materialise with no player present. Idempotent per entity; a re-add reuses the key. **Refuses null** (a null-entity insert hard-freezes the client) and **refuses an entity with `EntityID.INVALID`** (unkeyable — every unregistered entity shares that id), both with a WARNING. So add it a frame after the entity is spawned, not from inside its own init. Pass `GetGroup(handle)` to make a registered group observe for itself. |
| `bool RemoveEntityObserver(entity)` (added 2026-08-17 at your request) | **Do this yourself, from wherever you clean the entity up.** Core's 2 s sweep only catches entities that are already gone; until then a forgotten observer holds everything near it spawned. Null / invalid id / unknown / client are safe `false`s. |
| `bool HasEntityObserver(entity)` (added 2026-08-17 at your request) | Your bookkeeping question, answered from **core's map**, correct on the frame you ask. Engine application lags a frame in both directions — never verify this by querying `ObserversSystem`. False for an entity with an invalid id, for the same reason Add refuses one. |
| `int GetEntityObserverCount()` (added 2026-08-17 at your request) | How many *core* is holding. Not `GetObserversSP()`, which also contains cameras, the deploy-point MP preload insert and everything else the session parked. |
| `bool GetRecruitGroupsAreObservers()` | The operator's off-switch for the one shipped consumer (parked recruit groups, D16). Read it **in your consumer**; `AddEntityObserver` deliberately does not, because it serves everybody. |

**Importance guidance is yours to apply** ([§5](#5-importance-tiers)): base and tower garrisons and
anything the player travelled to fight → `HIGH`; scripted patrols, town garrisons and remnants →
`NORMAL` (the default); decorative filler → `LOW`; `CRITICAL` is for player-led AI and a core
registration should not need it. Never leave it unstamped and inherit vanilla's `LOW`, which is
capped at half the AI budget and evicted first — that is exactly how a garrison fails to appear on a
busy server (R11/F14).

Two migration rules: on a continued campaign the registry is **already populated** when
`PostGameStart()` runs, so check `FindGroupsByOwner` before registering; and a consumer that
registers on `GetOnRecordsRestored()` must be idempotent, because that invoker also fires on an
in-session re-apply.

---

## Phase map — what is real today

| Area | Phase 6 (now — frozen) | Lands in |
|---|---|---|
| Types, signatures, handles, owner tags | ✅ final | — |
| `RegisterGroup` composition resolution + plan validation + record booking | ✅ | — |
| Group entity creation, policy/distance/importance stamping, waypoint entities | ✅ | — |
| Roster capture / `m_aSlotAlive` population, `ExpandOneMember` override, death hook, count re-assertion | ✅ | — |
| `ReportMemberKilled` / `GetMemberAlive` / wipe + `OnGroupWiped` | ✅ live | — |
| `UnregisterGroup` entity + waypoint teardown | ✅ | — |
| `ForceSpawn` / `ForceDespawn` | ✅ | — |
| Ambient sources (config/registry classes, registration, tick, spawn/despawn, release, pruning) | ✅ live (Phase 4) | — |
| Registry persistence (`OVT_VirtualizationManagerSerializer`, bound in `Overthrow.conf`) | ✅ live (Phase 5) — **Route B**, see [§8](#8-persistence--what-is-and-is-not-in-a-save) | — |
| Group entities + owned waypoints coming back on load | ✅ **re-created by core** from the payload | — |
| `GetOnRecordsRestored` firing | ✅ once per load, after `PostGameStart()` | — |
| Persistence-**tracking** opt-in for group entities (`m_bPersistGroupEntities`) | ⚠️ built, deliberately **off** and dormant — Route B does not use it | — |
| Vanilla `SCR_AIGroupSerializer` records for virtual groups | ❌ **never written, by design** | — |
| Real quit-to-menu → Continue verification | ⚠️ manual play-test only (implementation.md §6 step 12) | — |

**Handles, owner tags, composition identity and the D2 contract will not change.** Anything that
does change after Phase 2 is a breaking change and gets recorded in `context.md`. Phase 3 changed
nothing that existed — it only added record fields (§2) and made the stubs real. Phase 4 landed the
frozen `RegisterAmbientSource` signature verbatim, made the other four ambient calls real, and added
two convenience accessors (`GetAmbientRegistry`, `FindAmbientSourceConfig`); nothing was renamed.
Phase 5 added no consumer-facing signature at all — it added `SnapshotRegistry` /
`ApplyPersistedRegistry` / `GetNextHandle` for its serializer, and made `GetOnRecordsRestored()` fire.
The **payload field order is frozen forever** (`OVT_PersistedVirtualGroup`): binary contexts are
positional, so new fields are appended behind a version bump, never inserted or reordered. Phase 6
added **no** consumer-facing signature either: it hardened the restart path (`GetInstance()` re-resolves
across a world, the ambient tick and the restore latch self-cancel for a manager the static no longer
names), extended the debug affordance with `m_iDebugTestGroupCount`, moved lifecycle chatter to
`VERBOSE`, and froze this page.

**Post-freeze, 2026-08-17 — the one additive change so far.** `virtualization/civilians` Phase 1 added
`IsEntityDead(IEntity)` and `OnEntityPruned(IEntity)` to `OVT_AmbientSpawnSourceConfig` and called both
from `PruneAmbientEntities`. The first one's default **is** the damage-state check the manager
previously did inline, moved unchanged, so no existing source's behaviour moved; the second defaults to
a no-op. Nothing was renamed, re-signed or re-meant, no payload field was touched, and
`CONFIG_STREAM_VERSION` did not move. Recorded in `context.md` under "Additive changes after the
freeze".

**Debug affordances.** `OVT_VirtualizationManagerComponent` carries `m_bDebugRegisterTestGroup`
(default **false**) on the game-mode prefab. With it on, `PostGameStart` registers a cycling patrol
near the campaign's starting town under owner system `"debug"` and logs the registration wall time,
the first handle, its position, roster size and spawn ring — the only way to play-test core before a
consumer exists. `m_iDebugTestGroupCount` (default **1**) turns that into the scale probe: any value
above 1 spreads that many groups over a disc of radius `m_fDebugTestGroupOffset` on a golden-angle
spiral (≈40 is the fast-travel measurement of implementation.md §6 step 8). `m_bDebugAmbientLogging`
(default **false**) logs ambient activations, per-tick spawn batches and despawns.

**Logging discipline (T6.6).** Everything core prints at `NORMAL` or above is either an
operator-actionable problem (a composition that will not resolve, a record being dropped, a plan that
does not line up — all `WARNING`, all naming the key) or output of a debug affordance that is off by
default. Ordinary lifecycle chatter — registrations, wipes, restores, force-despawns — is
`LogLevel.VERBOSE`. If you are debugging core, raise the log level; do not add prints to it.
