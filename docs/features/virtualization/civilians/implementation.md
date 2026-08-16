# Virtualization Civilians — Implementation Plan

**Status:** Ready for Review
**Created:** 2026-08-17
**Started:** 2026-08-17
**Last Updated:** 2026-08-17

**Epic:** `virtualization` (feature #2 of 5 — see `docs/features/virtualization/epic-overview.md`)
**Requirements:** `docs/features/virtualization/civilians/requirements.md` (authoritative for scope) **plus two user-approved amendments (2026-08-17)** recorded in [D1](#d1--variety-comes-from-overthrow-authored-prefab-variants-not-the-civ-entity-catalog-user-amendment-2026-08-17) and [D2](#d2--parked-vehicles-derive-their-spots-from-roadside-geometry-never-from-ovt_parkingcomponent-user-amendment-2026-08-17)
**Consumes:** `docs/features/virtualization/core/api.md` — 🔒 **FROZEN**. §4 (ambient sources) and §10's `civilians` table are the entire surface this feature programs against. Two **additive** core methods are added ([§3.4](#34-the-two-additive-core-hooks)); nothing existing is renamed, re-signed or re-meant.

> **Why this feature is second in the epic.** It is the first consumer of core's ambient seam and is deliberately sequenced before any combat migration, so the extensibility seam gets exercised by real content while core is still fresh. Whatever registry/config conventions land here are what `integration` and `base-defense-migration` copy — the authored `.conf` is a template, not just content.

---

## 1. Executive Summary

Town civilians are Overthrow's oldest ad-hoc spawner: a per-town 10 s `CallLater` in `OVT_TownControllerComponent` that, when any player is within 1000 m, spawns `population × 0.1` one-man `Group_CIV.et` groups **in a single frame** (St. Phillipe: 28 groups + 140 waypoint entities at once), each ping-ponging forever between two road points. It never prunes the dead, never deletes its waypoints, despawns every town's crowd whenever a QRF starts *anywhere*, and can delete a civilian a player just recruited.

This feature retires all of it and rebuilds town ambience as **declarative content on core's ambient spawn-source seam**:

1. **One ambient source per town**, registered at `ActivateTown()` and owned by a new server-only Manager. Core's tick decides when it materialises (engine `ObserversSystem`, hysteresis band, ≤3 spawns per tick), so frame-spreading, anti-thrash and proximity stop being Overthrow's problem.
2. **A per-town config instance over an authored `.conf` template.** The registry declares what a town crowd *is* (prefab pool by civilian type, behaviour archetypes, density rate, clamps); the instance binds it to one town (live population, town range, allowed types) and overrides the five roll/hook methods. Modders extend by editing a `.conf` or subclassing — no script change.
3. **Runtime operator tuning:** `civilianDensityMultiplier` and `maxCiviliansPerTown` in `$profile:Overthrow_Config.json`, server-side only, no JIP stream change.
4. **Lifecycle correctness by construction:** waypoints are owned and deleted with their civilian, dead civilians are pruned (bodies left where they fell), recruited civilians are transferred out of ambient ownership, and QRF suppression becomes **town-local**.
5. **More life:** 4–6 Overthrow-authored civilian type variants with per-type clothing, ≥3 behaviour archetypes, placement that prefers doorways and POIs over the middle of the road, and — as a final, explicitly droppable phase — parked civilian vehicles along kerbs and roadsides.

**Three defects found while planning** shape the design and are fixed here rather than inherited:

- **F-A — deleting a group entity does *not* delete its members.** `SCR_AIGroup.SpawnGroupMember` spawns members as **world-root** entities (`SCR_AIGroup.c:1658-1757`, no `AddChild`, no `spawnParams.Parent`), the destructor only destroys *edit-mode* scene instances (`:3391-3409`, `m_aSceneGroupUnitInstances` is filled only when `editMode`, `:1750`), and vanilla's own `DespawnMembers` deletes controlled entities **explicitly** (`:2905`). So today's `DespawnCivilians()` → `DeleteEntityAndChildren(group)` deletes the group and **leaves every civilian body in the world**. The new despawn hook deletes members explicitly. (Happy consequence: leaving a *corpse* behind when its group husk is deleted is free — no reparenting.)
- **F-B — the civilian wanted-system disable is a no-op.** `SCR_AIGroup.GetOnAgentAdded()` publishes an **`AIAgent`** (`OVT_LoadoutUtils.RandomizeCivilianClothes(AIAgent)` resolves `agent.GetControlledEntity()`; `OVT_BaseUpgradeTowerGuard.c:190` uses the same signature), but `OVT_TownController.DisableCivilianWantedSystem(IEntity)` calls `FindComponent(OVT_PlayerWantedComponent)` **on the agent**, where that component does not exist. `m_bWantedSystemEnabled` defaults **true**, so civilians have been running with the wanted system live.
- **F-C — `SpawnWaitWaypoint(pos, time)` drops `time`** (`OVT_OverthrowConfigComponent.c:506-511`). The missing call is `SCR_TimedWaypoint.SetHoldingTime(float)`, which itself silently no-ops when the prefab does not author `m_TimedWaypointParameters` — so the prefab needs checking too.

The town controller keeps its flag material, gun dealer and QRF geometry, and **shrinks by roughly 90 lines**.

---

## 2. Goals

### Primary

- **G1** Town civilians spawn exclusively through `RegisterAmbientSource` — no Overthrow proximity poll, no per-town civilian `CallLater`, no all-or-nothing frame.
- **G2** Civilian ambience is **declarative**: density inputs, prefab pools by type, archetypes and placement rules live in `Configs/Civilians/CivilianAmbience.conf`; a modder adds a civilian type or a whole new ambient source without touching script.
- **G3** Density derives from **live** `OVT_TownData.population` through a documented formula with explicit clamps, an operator multiplier and a hard per-town cap — all world-free and Logic-tier tested.
- **G4** Operators tune density at runtime in `$profile:Overthrow_Config.json` (`civilianDensityMultiplier`, `maxCiviliansPerTown`), server-side only, `CONFIG_STREAM_VERSION` unchanged.
- **G5** **No leaks:** every `AIWaypoint` a civilian owns is deleted with it, every member character is deleted with its group, dead civilians are pruned from their source, and no group husk survives a recruit.
- **G6** **Player interactions survive:** recruit, convert-supporter, sell-drugs, the wanted-system disable (now actually working — F-B) and the civilian-death stability/support modifiers all behave as before. A recruited civilian leaves ambient ownership and survives every later despawn.
- **G7** QRF suppression is **town-local**: only the town under QRF loses its crowd.
- **G8** Towns feel inhabited: ≥4 civilian looks with per-type clothing, ≥3 behaviour archetypes, believable placement (doorways/POIs preferred, roads as fallback, never water), and real pauses (F-C fixed).

### Secondary

- **G9** The authored `.conf` and the per-town-instance pattern are **exemplary** — `integration` and `base-defense-migration` copy this shape.
- **G10** Nothing about a civilian is persisted, asserted by construction (no serializer, no payload, no save-tier test needed).
- **G11** Stretch: parked civilian vehicles make towns look inhabited; a vehicle a player takes leaves ambient management, survives, and starts being persisted.

### 2.1 Quality Bar — the hard floor

This is a **gameplay-feel and reliability** feature. Being "done" means all four of these hold, and any one of them failing sinks the phase:

| Bar | What it means concretely | How it is caught |
|---|---|---|
| **No hitches** | Approaching a city never spawns a crowd in one frame. Core's budget (3 entities/tick, 2 s tick) is the only fill rate; nothing in this feature bypasses it or spawns in a loop. | `m_bDebugAmbientLogging` timestamps; play-test §6 step 2 |
| **No leaks** | Waypoints, member characters, group husks and reverse-map entries all die with their civilian. Entity counts return to baseline after a spawn/despawn cycle. | Q-criteria grep (creation site ↔ delete site) + the 10-cycle entity count, §6 |
| **Believable placement** | Civilians stand and walk where people would — outside doorways, around POIs, along streets — never in the sea, never inside a wall, never all in one line down the middle of the road. | Play-test, eyes on |
| **Zero interaction regressions** | Recruit / convert / sell-drugs / death-modifiers work exactly as before the migration. A regression here is worse than shipping no variety at all. | §6 Integration criteria, one scripted play-test pass |

---

## 3. Architecture Overview

### 3.1 Division of labour

```
SERVER ONLY. Nothing here replicates; every entry point is behind core's server guard
or the manager's own. Ambient state is NEVER persisted — despawn discards, approach re-rolls.

CORE (virtualization/core — frozen, api.md §4)
├─ RegisterAmbientSource(config, position, ownerKey) / UnregisterAmbientSource(handle)
├─ the 2 s round-robin tick: ObserversSystem proximity, spawn ring + 1.15 hysteresis band,
│    RollCount() once per activation, ≤3 spawns per tick, prune-before-evaluate
├─ ReleaseAmbientEntity(entity)  — ownership transfer, O(1)
└─ m_AmbientRegistry attribute   — points at THIS feature's .conf

CIVILIANS (this feature)
├─ OVT_CivilianAmbienceManagerComponent          Manager on OVT_OverthrowGameMode
│    ├─ townId -> ref OVT_TownCivilianSourceConfig  (the per-town config INSTANCE)
│    ├─ townId -> ambient handle(s)                 (civilians; + vehicles in Phase 5)
│    ├─ ActivateTown / DeactivateTown               (called by the town controller)
│    ├─ QRF locality: one subscription to m_OnQRFTownChanged (new, additive)
│    ├─ ReleaseRecruitedCivilian(character)         (called by OVT_RecruitManagerComponent)
│    └─ OnDelete: unregister every source, clear s_Instance   (R7 hygiene)
├─ OVT_CivilianAmbienceConfig : OVT_AmbientSpawnSourceConfig    the AUTHORED template
│    types (prefab + weight + min town size + optional loadout), archetype weights,
│    population rate, min/max clamps, placement mix
├─ OVT_TownCivilianSourceConfig : OVT_AmbientSpawnSourceConfig  the RUNTIME per-town instance
│    holds the template + townId + town size + allowed types + radius;
│    overrides RollCount / RollPrefab / RollPosition / OnEntitySpawned /
│    OnEntityDespawning / IsEntityDead / OnEntityPruned
├─ OVT_CivilianAmbienceMath                      world-free statics (Logic tier)
└─ Configs/Civilians/CivilianAmbience.conf       the authored registry — the modder seam

TOWNS (unchanged except for two seams)
└─ OVT_TownControllerComponent
     keeps flag material, gun dealer, QRF geometry attributes, size/population/range
     gains  m_aCivilianTypes attribute + one ActivateTown() call + an OnDelete teardown
     loses  m_aCivilians, CheckSpawnCivilian, SpawnCivilians, SpawnCivilian,
            DespawnCivilians, GetGroup, DisableCivilianWantedSystem, m_bCiviliansSpawned
```

### 3.2 Manager vs Controller — decided: **Manager**

A new `OVT_CivilianAmbienceManagerComponent` on the game-mode prefab, standard pair + `s_Instance` + `OVT_Global.GetCivilianAmbience()`, copying `OVT_VirtualizationManagerComponent`'s shape (server guard **before** allocating collections; `Init(IEntity)` from `EOnInit`; `PostGameStart()`; `OnDelete` clearing `s_Instance`).

Why not keep it on the town controller:

- **The recruit chokepoint needs a global entry point.** `OVT_RecruitManagerComponent.RecruitCivilian()` holds a character and must reach "whoever owns this civilian" in O(1). A manager accessor is that; 20 controllers plus a static registry is a worse version of the same thing.
- **QRF locality wants one subscriber, not twenty.** One `m_OnQRFTownChanged` subscription that dispatches by town id beats 20 controllers each filtering the same event.
- **The town controller runs on clients too** (`OnPostInit` has no server guard — the flag material is deliberately local). Server-only ambient state does not belong there.
- **Teardown.** The town controller has *no* `OnDelete` today and two repeating `CallLater`s that are never removed. A manager gives the feature one authoritative teardown; the controller gets a minimal one for the per-town case.
- **Phase 5 plugs in for free** — the vehicle source is a second handle in the same per-town record.

The **per-town runtime config instance** is the "controller" of this feature in all but name: one object per town, holding that town's state, created at activation and dropped at teardown. It is not an entity component because core's seam takes a config object, not a component.

### 3.3 The class model

```c
// ---- AUTHORED (Configs/Civilians/CivilianAmbience.conf) ----------------------------------

[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sTypeName")]
class OVT_CivilianTypeConfig
{
    string       m_sTypeName;        //!< "generic", "businessman", "dockworker", ...
    ResourceName m_rGroupPrefab;     //!< Group_CIV_<type>.et (1 slot, Overthrow character)
    int          m_iWeight;          //!< roll weight within the allowed set
    OVT_TownSize m_eMinTownSize;     //!< default VILLAGE = anywhere; CITY keeps businessmen out of villages
    ref OVT_LoadoutConfig m_Loadout; //!< OPTIONAL per-type clothing; null = the global CivilianClothes.conf
}

[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sArchetypeName")]
class OVT_CivilianArchetypeConfig
{
    string  m_sArchetypeName;
    OVT_ECivilianArchetype m_eArchetype;  //!< PINGPONG, WANDER, LOITER
    int     m_iWeight;
    int     m_iPointCount;                //!< WANDER: how many route points
    float   m_fWaitMin, m_fWaitMax;       //!< seconds at each stop (needs the F-C fix to matter)
}

[BaseContainerProps(), SCR_BaseContainerCustomTitleField("m_sSourceName")]
class OVT_CivilianAmbienceConfig : OVT_AmbientSpawnSourceConfig
{
    ref array<ref OVT_CivilianTypeConfig>      m_aTypes;
    ref array<ref OVT_CivilianArchetypeConfig> m_aArchetypes;
    float m_fPopulationRate;          //!< 0.1 — parity with today's m_fCivilianSpawnRate
    float m_fBuildingPlacementChance; //!< 0..1 share of spawns placed at a doorway/POI instead of a road
}

// ---- RUNTIME (constructed per town at ActivateTown) --------------------------------------

class OVT_TownCivilianSourceConfig : OVT_AmbientSpawnSourceConfig
{
    OVT_CivilianAmbienceConfig m_Template;   //!< read-only declarative data; NOT copied field by field
    int          m_iTownId;
    OVT_TownSize m_eTownSize;
    ref array<ref OVT_CivilianTypeConfig> m_aAllowedTypes;   //!< resolved once at activation
    ref array<vector> m_aPlacementCache;                     //!< doorway/POI points, built LAZILY
    ref map<EntityID, ref OVT_AmbientCivilianRecord> m_mCivilians;

    // overrides: RollCount, RollPrefab, RollPosition, OnEntitySpawned,
    //            OnEntityDespawning, IsEntityDead, OnEntityPruned
}

class OVT_AmbientCivilianRecord : Managed
{
    EntityID m_GroupId;
    bool     m_bSeenAgent;                  //!< set from GetOnAgentAdded — the dead-check precondition
    ref array<AIWaypoint> m_aWaypoints;     //!< everything this civilian owns; deleted with it
    string   m_sTypeName;
}
```

Only the base-class fields core reads directly are set on the instance (`m_sSourceName`, `m_fRadius` = the town's range, `m_iSpawnDistanceOverride`, `m_iMinCount`/`m_iMaxCount` as the density clamps). Everything else is read **through** `m_Template`, so there is no field-copy method to drift out of sync when the template gains an attribute.

### 3.4 The two additive core hooks

Both are **additive** to `OVT_AmbientSpawnSourceConfig` — the frozen `api.md` explicitly permits new methods. Both land as Phase 1, are documented in `api.md` §4 and are recorded as an additive change in `core/context.md`.

```c
//! Is this entity finished — a corpse, a wreck — and no longer part of the live crowd?
//! DEFAULT: the damage-state check the manager used to do inline
//! (SCR_DamageManagerComponent.GetState() == DESTROYED), unchanged for every existing source.
bool IsEntityDead(IEntity entity);

//! Called after a pruned entity has been removed from the source's list AND its reverse-map entry.
//! The entity is NO LONGER OWNED when this runs: the source will never delete it, and calling
//! ReleaseAmbientEntity() on it is a no-op. This is where a consumer deletes the COMPANIONS of a
//! pruned entity (waypoints, an empty group husk) while deliberately leaving the entity itself.
void OnEntityPruned(IEntity entity);
```

**Why a config hook and not `GetOnCharacterKilled()`** (the alternative offered in the brief — only one is built): the prune hook runs inside core's own ownership window, exactly once per pruned entity, only for entities this source owns, and needs no global subscription, no character→agent→group→source resolution and no "was that a death or a teardown?" disambiguation. It is also symmetric with `OnEntitySpawned`/`OnEntityDespawning` — the seam later ambient consumers (wrecked vehicles) will want. The kill-invoker route would re-derive information core already has. **Do not build both.**

**The dead-check predicate for a group-shaped ambient entity** (the whole reason `IsEntityDead` has to be overridable): a `SCR_AIGroup` entity has no `SCR_DamageManagerComponent`, so core's default answers `false` forever and a dead civilian would never be pruned. The override is:

```
IsEntityDead(group) := record.m_bSeenAgent && group.GetAgentsCount() == 0
```

`m_bSeenAgent` is set from `GetOnAgentAdded` and is **mandatory**: member spawning goes through the engine's queue in 1.8, so a freshly spawned group is legitimately memberless for one or more frames, and a naive `GetAgentsCount() == 0` would prune every civilian the instant it was created. The predicate also (correctly, if bluntly) treats an engine budget **eviction** as death — see [D8](#d8--a-killed-civilian-is-not-replaced-until-the-next-approach).

### 3.5 Lifecycle, in prose

**Registration.** `OVT_TownManagerComponent.PostGameStart()` (or the deprecated `SpawnTownControllers` path) calls `ActivateTown()` on each controller, which calls `OVT_Global.GetCivilianAmbience().ActivateTown(this, m_Town, townId)`. The manager resolves the `"town_civilians"` template through `virt.FindAmbientSourceConfig()`, builds a `OVT_TownCivilianSourceConfig` bound to that town (radius = `m_iTownRange`, allowed types resolved from the town's size and the controller's `m_aCivilianTypes`), and calls `RegisterAmbientSource(cfg, town.location, "town:" + townId)`. **Nothing spawns.** Registration is cheap enough to do for all 20 Eden towns at start.

**Activation.** Core's tick finds an observer inside the spawn ring, calls `RollCount()` **once** — our override reads the town's *live* population and applies the formula — and then spends that number across ticks at ≤3 per tick. Each entity: `RollPrefab()` picks a type (weighted, filtered), `RollPosition()` picks a doorway/POI or a road point, core spawns the group prefab and calls `OnEntitySpawned`, where we hook `GetOnAgentAdded` (clothes, wanted-disable via `agent.GetControlledEntity()`, `m_bSeenAgent`), build the archetype's waypoints, and open a record.

**Death.** The member dies → the agent leaves the group → the next prune sees `IsEntityDead(group)` true → core drops it from the list and the reverse map → `OnEntityPruned(group)` deletes the waypoints and the (now memberless) group husk. **The body stays where it fell** — it is a world-root entity, never a child of the group (F-A), so nothing cascades to it.

**Despawn.** Every observer leaves the despawn ring → core calls `OnEntityDespawning(group)` for each live civilian: we delete the **member characters explicitly** (iterate `GetAgents()` → `GetControlledEntity()` → `RplComponent.DeleteRplEntity`, vanilla's own idiom at `SCR_AIGroup.c:2905`) and the waypoints, then core deletes the group entity. Nothing is remembered; the next approach re-rolls.

**Recruitment.** `RecruitCivilian()` succeeds → before `AddRecruitToPlayerGroup()` reparents the agent, the manager resolves character → `AIControlComponent.GetAIAgent()` → `GetParentGroup()` and calls `ReleaseAmbientEntity(group)`. Core hands ownership over; the source will never delete that group again. The manager then deletes the emptied husk and its waypoints itself (the record is still in its map) — `OnEntityPruned` will never fire for a released entity, so this is an explicit cleanup, not a hook. Releasing a civilian that was never ambient (a tent recruit) is a safe no-op by core's own re-verification.

**QRF.** `StartTownQRF` invokes `m_OnQRFTownChanged(townID)`; the manager unregisters that town's sources (core despawns them through `OnEntityDespawning`). `OnQRFFinishedTown` invokes `(-1)`; the manager re-registers, and the next approach re-rolls a fresh crowd — which is the ambient contract anyway. Base QRFs do **not** suppress town civilians ([D6](#d6--qrf-suppression-is-town-local-and-base-qrfs-do-not-suppress-town-civilians)).

**Teardown.** Manager `OnDelete` unregisters every source (deleting live civilians through the same hook) and clears `s_Instance`; the town controller's new `OnDelete` calls `DeactivateTown(townId)` for the per-town case.

### 3.6 Density model

```
OVT_CivilianAmbienceMath.ResolveTownCivilianCount(
        int population, float rate, float multiplier,
        int minCount, int maxCount, int hardCap) -> int
```

1. `population <= 0` → **0** (a depopulated town has no crowd; the min clamp must not resurrect one).
2. `rate * multiplier <= 0` → **0** (`civilianDensityMultiplier = 0` is the documented "turn civilians off" switch).
3. `raw = Math.Round(population * rate * multiplier)`.
4. Clamp into `[minCount, maxCount]` — the config's existing `m_iMinCount`/`m_iMaxCount`, reused as the per-source floor/ceiling rather than inventing new fields.
5. `hardCap > 0` → `Math.Min(count, hardCap)`; `hardCap <= 0` means **no cap** (so an old `Overthrow_Config.json` without the key can never silently zero a server's civilians).

Parity check with today: St. Phillipe `283 × 0.1 = 28`, Tyrone `38 × 0.1 = 4`, Gravette `27 × 0.1 = 3`. Authored defaults: `m_fPopulationRate 0.1`, `m_iMinCount 2`, `m_iMaxCount 40`, `civilianDensityMultiplier 1.0`, `maxCiviliansPerTown 30`.

`RollCount()` is rolled once per activation and re-rolled on every later approach, so a town whose population changed since the last visit gets the new number without any re-registration — **population is never baked at registration time**.

### 3.7 Config surfaces

| Surface | What lands there | Notes |
|---|---|---|
| `Configs/Civilians/CivilianAmbience.conf` | `OVT_AmbientSpawnSourceRegistry` with `town_civilians` (+ `town_vehicles` in Phase 5) | The first authored ambient content in the tree; core ships zero |
| `Prefabs/GameMode/OVT_OverthrowGameMode.et` | `m_AmbientRegistry` → the `.conf`; the new manager component; `m_pCivilianPrefab` binding **removed** | Text-wiring + user Workbench verification (core's T2.8 precedent) |
| `$profile:Overthrow_Config.json` | `civilianDensityMultiplier`, `maxCiviliansPerTown` | Server-only; **not** in `RplSave`/`RplLoad`; `CONFIG_STREAM_VERSION` stays 3. `LoadConfig()` runs `SetDefaults()` before `ReadValue`, so an existing file without the keys keeps the defaults |
| `OVT_TownControllerComponent` (world layer `towns.layer`) | `m_aCivilianTypes` per placed town | Eden authors 20 towns individually today (`m_Size`, `m_iPopulation`, `m_iTownRange`) — per-town flavour is authorable, not theoretical |
| `Configs/Civilians/CivilianClothes.conf` (+ optional per-type files) | Widened choices; per-type overrides | See [D1](#d1--variety-comes-from-overthrow-authored-prefab-variants-not-the-civ-entity-catalog-user-amendment-2026-08-17) |

Retired: `m_pCivilianPrefab` (`OVT_OverthrowConfigComponent.c:119`), `m_fCivilianSpawnRate` (:171), `m_iCivilianSpawnDistance` (:174) — all three exclusively fed the retired path. `m_pCycleWaypointPrefab` (:153) is **shared** (base upgrades, deployments, resistance) and stays.

---

## 4. Implementation Phases

Each phase ends compiling clean with its automated gate green. Phases 1–2 land **parity**, 3–4 land **enrichment**, 5 is **droppable**, 6 syncs help/docs.

**Test-run policy:** `tools/compile-check.sh` runs freely; `tools/run-tests.sh` launches a real Reforger client that steals desktop focus, so it is run **by the orchestrator only, once, after a phase completes** — never during planning, never inside a subagent. See `.claude/test-policy.md`.

---

### Phase 1 — Core seam, declarative model, density math

**Agent:** `component-developer-advanced` — **advanced.** It edits the epic's **frozen** core: a mistake in `PruneAmbientEntities` corrupts the ownership bookkeeping every later consumer relies on.
**Estimate:** 5–7 h

**Tasks**

1. **T1.1** Add `bool IsEntityDead(IEntity entity)` to `OVT_AmbientSpawnSourceConfig`, its body being `OVT_VirtualizationManagerComponent.IsAmbientEntityDead`'s current damage-state check verbatim. `PruneAmbientEntities` (manager ~:3060) calls `config.IsEntityDead(entity)`, keeping the manager's own method as the fallback for a null config. **Existing sources must behave byte-for-byte as before.**
2. **T1.2** Add `void OnEntityPruned(IEntity entity)` (default no-op), called from `PruneAmbientEntities` **after** the list removal and the reverse-map removal — order is load-bearing and must be stated in the header, so a hook can never be handed an entity core still thinks it owns.
3. **T1.3** Update `api.md` §4 (config class listing + the two hooks + the "leave the body, delete the companions" rule) and append a dated **additive change** note to `core/context.md` naming this feature as the requester. Nothing existing is renamed or re-signed.
4. **T1.4** Create `Scripts/Game/GameMode/Civilians/OVT_CivilianAmbienceMath.c` — world-free statics: `ResolveTownCivilianCount(...)` (§3.6), `TypeAllowed(typeMinSize, townSize, allowedNames, typeName)`, `PickWeightedIndex(weights, roll)`. **No manager, game-mode or world identifier anywhere in the file.**
5. **T1.5** Add `float civilianDensityMultiplier` and `int maxCiviliansPerTown` to `OVT_OverthrowConfigStruct` (`OVT_OverthrowConfigComponent.c:24-81`) with doc comments copying `virtualizationSpawnDistance`'s (:51-57) explanation of why they are absent from the JIP stream; defaults in `SetDefaults()` (:59). **Do not touch `RplSave`/`RplLoad` or `CONFIG_STREAM_VERSION`.**
6. **T1.6** New Logic-tier file `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_CivilianAmbience.c` (Fast): parity numbers (283→28, 38→4), clamps at both ends, `multiplier = 0` → 0, `population = 0` → 0, hard cap applied, `hardCap <= 0` = uncapped, weighted pick at first/last/out-of-range rolls, type filter by min size and by explicit allow-list.
7. **T1.7** Init-tier case (`OVT_TEST_InitSuite.c`): a config subclass's overridden `IsEntityDead` is the one core calls — same shape as the existing "subclass's `RollCount()` override is called" case (~:4744).
8. **T1.8** Init-tier case: both new config-struct fields read back their defaults through `OVT_Global.GetConfig().m_ConfigFile`, modelled on `OVT_TEST_InitSuite.c:3442-3501`.

**Acceptance criteria**

- `tools/compile-check.sh` exits 0. Fast group green including the new cases; every new case carries a recorded can-fail proof in a preamble comment; **no `maxAttempts`**.
- `git diff Scripts/Game/GameMode/Virtualization/` shows **only** the two added methods plus their two call sites — no signature changed, nothing removed.
- `api.md` §4 lists both hooks; `core/context.md` carries the dated additive note.
- `grep -rn "Rpc\|RplProp\|Replication.Bump" Scripts/Game/GameMode/Civilians/` is empty.
- Logic-tier grep clean (no manager-accessor or game-mode-getter identifier under `TestSuites/Logic/`, comments included).
- **No behaviour change yet** — `OVT_TownController.c` is untouched.

---

### Phase 2 — The migration (parity)

**Agent:** `component-developer-advanced` — **advanced.** It retires a live system, edits `OVT_RecruitManagerComponent` (a chokepoint where a mistake orphans or deletes a player's recruit), and owns entity lifetime across spawn/despawn/prune/release.
**Estimate:** 12–16 h

**Tasks**

1. **T2.1** Create the authored classes of §3.3 (`OVT_CivilianTypeConfig`, `OVT_ECivilianArchetype`, `OVT_CivilianArchetypeConfig`, `OVT_CivilianAmbienceConfig`) under `Scripts/Game/GameMode/Civilians/`.
2. **T2.2** Create `OVT_TownCivilianSourceConfig` + `OVT_AmbientCivilianRecord` with all seven overrides. Parity behaviour for this phase: one type (`generic` → `Group_CIV.et`), one archetype (`PINGPONG`, today's two road points), `RollPosition` = today's `GetRandomNonOceanPositionNear` + `FindNearestRoad`. The lifecycle work is the phase's real content:
   - `OnEntitySpawned`: cast to `SCR_AIGroup`; insert `OVT_LoadoutUtils.RandomizeCivilianClothes`, a wanted-disable callback **taking `AIAgent` and resolving `agent.GetControlledEntity()` (F-B)**, and a `m_bSeenAgent` setter; build waypoints; open the record. Do **not** optimise away `DisableWantedSystem()`'s internal `SetWantedLevel(0)` — that call carries the replication bump.
   - `OnEntityDespawning`: **delete the member characters explicitly (F-A)** — `GetAgents()` → `GetControlledEntity()` → `RplComponent.DeleteRplEntity(ent, false)` — then the waypoints, then drop the record. Core deletes the group entity itself.
   - `IsEntityDead` / `OnEntityPruned` per §3.4/§3.5: prune deletes waypoints + husk and **leaves the body**.
3. **T2.3** Author `Configs/Civilians/CivilianAmbience.conf` — `OVT_AmbientSpawnSourceRegistry` root, one `town_civilians` source at parity settings (rate 0.1, min 2, max 40, `m_iSpawnDistanceOverride -1`, radius overwritten per town). Fresh, repo-unique GUID. Copy the authoring shape of `Configs/Deployment/overthrowDeployments.conf`.
4. **T2.4** Text-wire `m_AmbientRegistry` → the new `.conf` in `Prefabs/GameMode/OVT_OverthrowGameMode.et`; flag the Workbench verification as a user task.
5. **T2.5** Create `OVT_CivilianAmbienceManagerComponent` (§3.2): class pair, `s_Instance`/`GetInstance()`, server guard before allocating collections, `Init(IEntity)`, `PostGameStart()`, `ActivateTown(controller, town, townId)`, `DeactivateTown(townId)`, `ReleaseRecruitedCivilian(SCR_ChimeraCharacter)`, `OnDelete`.
6. **T2.6** Add `OVT_Global.GetCivilianAmbience()`; add the field + `EOnInit` `FindComponent`/`Init(this)` block **after** Virtualization (`OVT_OverthrowGameMode.c:1457-1462`) and the `PostGameStart()` call after Virtualization's (:356-360); text-wire the component onto the game-mode prefab.
7. **T2.7** Rewrite `OVT_TownControllerComponent`: `ActivateTown()` keeps the gun-dealer branch and gains one manager call; add `OnDelete` → `DeactivateTown`; **delete** `m_aCivilians` (:40, :96), `m_bCiviliansSpawned` (:36), `CheckSpawnCivilian` (:128/:134), `DespawnCivilians` (:145), `GetGroup` (:157), `SpawnCivilians` (:163 — the `OVT-VIRT-PLAYTEST-ONLY` guard at :165 goes with it), `SpawnCivilian` (:176), `DisableCivilianWantedSystem` (:211).
8. **T2.8** Retire `m_pCivilianPrefab` (+ its binding at `OVT_OverthrowGameMode.et:68`), `m_fCivilianSpawnRate`, `m_iCivilianSpawnDistance`; grep-prove zero remaining readers **before** deleting, and update the one prose reference in core's `api.md` §6.
9. **T2.9** Hook the release in `OVT_RecruitManagerComponent.RecruitCivilian()` (:983) — **after** `AddRecruit`/`SetRecruitFaction` have succeeded (:1011-1015) and **before** `AddRecruitToPlayerGroup()` (:1026), so a refused recruit never releases and a released civilian is never left half-owned. Null-safe when the manager is absent.
10. **T2.10** Init-tier cases: `FindAmbientSourceConfig("town_civilians")` resolves and is an `OVT_CivilianAmbienceConfig`; a per-town instance built from it exposes the template's rate/pool and the town's radius; the manager resolves through `OVT_Global`; `ReleaseRecruitedCivilian(null)` and a non-ambient character are safe no-ops.
11. **T2.11** Seed `docs/features/virtualization/civilians/context.md` with F-A/F-B/F-C, the play-test list, and a note to **file F-A and F-B as bugs** against the pre-migration code so the history is not lost.

**Acceptance criteria**

- compile 0; **Fast and All** green (the All group because town/campaign state is touched).
- `grep -rn "m_aCivilians\|CheckSpawnCivilian\|SpawnCivilians\|DespawnCivilians\|m_pCivilianPrefab\|m_fCivilianSpawnRate\|m_iCivilianSpawnDistance" Scripts/ Prefabs/` is empty.
- `grep -rn "OVT-VIRT-PLAYTEST-ONLY" Scripts/` returns **one fewer line** than before, and every remaining guard is intact — all suites stay green with the kill switch **on**.
- `git diff --stat Scripts/Game/Controllers/OVT_TownController.c` shows a net deletion.
- Play-test with `m_bDebugAmbientLogging` on: approaching a city logs `ACTIVATED … rolled N` once and then `spawned 3 this tick` lines until N — never one burst.

---

### Phase 3 — Lifecycle correctness: QRF locality, real pauses, believable placement, archetypes

**Agent:** `component-developer` (T3.1 touches `OVT_OccupyingFactionManager` — small and additive, but review it as a manager edit)
**Estimate:** 8–11 h

**Tasks**

1. **T3.1** Add `ref ScriptInvoker<int> m_OnQRFTownChanged` to `OVT_OccupyingFactionManager` beside the existing public invokers (:176-178), invoked with the town id at `StartTownQRF` (:928) and with `-1` at `OnQRFFinishedTown` (:1019). The civilians manager subscribes once and unregisters/re-registers that town's sources. **Unregister/re-register is the mechanism** — it is already supported, it despawns through the normal hook, and re-roll-on-return is the ambient contract.
2. **T3.2** Fix `SpawnWaitWaypoint` (`OVT_OverthrowConfigComponent.c:506-511`): call `SCR_TimedWaypoint.SetHoldingTime(time)`. **Then verify `Prefabs/AI/Waypoints/AIWaypoint_Wait.et` authors `m_TimedWaypointParameters`** — the setter silently does nothing when that object is null (`SCR_TimedWaypoint.c:38-42`), which would be a second invisible no-op. Note in `context.md` that the two existing callers (`GivePatrolWaypoints` PERIMETER, deployments) start actually pausing 45–75 s.
3. **T3.3** Placement: `RollPosition` picks a doorway/POI point with probability `m_fBuildingPlacementChance`, else today's road point. The candidate list is built **lazily on first use** per town (a `QueryEntitiesBySphere` over the town range collecting house origins and `OVT_SpawnPointComponent` points — never at registration, or 20 towns query at campaign start) and cached for the session. Ocean is impossible on both paths (`GetRandomNonOceanPositionNear` / building origins).
4. **T3.4** Archetypes: implement `WANDER` (N scattered points, patrol + short wait, cycle) and `LOITER` (one POI, move + long wait, cycle) beside `PINGPONG`; per-archetype waypoint construction in `OnEntitySpawned`, every created waypoint recorded in the civilian's record. Author weights in the `.conf`.
5. **T3.5** *(optional, small)* Bound `OVT_TownManagerComponent.m_ConvertedCivilians` (:138) — a never-pruned `set<RplId>` that now grows with every re-rolled crowd. Cheapest correct fix: on insert past a threshold, drop ids that no longer resolve. Not persisted, not load-bearing — drop this task if the phase is running long.
6. **T3.6** Logic cases for archetype weight resolution; play-test steps for QRF locality and archetype behaviour (§6).

**Acceptance criteria**

- compile 0; Fast green (All if T3.5 lands, since it touches the town manager).
- Play-test: start a QRF in town A while standing in town B → **B keeps its civilians**, A loses them; QRF ends → returning to A produces a fresh crowd.
- Play-test: loiterers visibly stand still for their configured pause instead of pacing.
- `grep -rn "AIWaypoint" Scripts/Game/GameMode/Civilians/` shows every creation site paired with a deletion site.

---

### Phase 4 — Variety: prefab variants, per-type clothing, per-town filtering

**Agent:** `component-developer` + **user Workbench verification** for the new prefabs
**Estimate:** 6–9 h + one Workbench session

**Tasks**

1. **T4.1** Author 4–6 Overthrow civilian **pairs**. Each `Prefabs/Characters/Factions/CIV/Character_CIV_<type>.et` is a delta over a vanilla CIV look (businessman `{E024A74F8A4BC644}`, dockworker `{C6FAF52907A544AC}`, construction worker `{6F5A71376479B353}`, plus generic shirt/denim variants) carrying **exactly** the three component blocks `Character_CIV.et` `{F5943DA35CB16C09}` declares — `OVT_PlayerOwnerComponent`, `OVT_PlayerWantedComponent`, and the `ActionsManagerComponent` with all six user actions (sell-drugs, recruit, convert-supporter, and the three recruit-management actions). Each `Prefabs/Groups/INDFOR/Group_CIV_<type>.et` is a delta over `Group_CIV.et` `{1AF5B9AE5CFD4434}` overriding only `m_aUnitPrefabSlots`. `RplComponent` and the faction-affiliation component come from the vanilla chain — **diff each new prefab against `Character_CIV.et` before shipping** (memory: Overthrow prefabs have shipped missing vanilla components twice).
2. **T4.2** Widen `Configs/Civilians/CivilianClothes.conf` (today 7 pants / 27 tops / 4 shoes / 21 hats from the ~94 uniform and ~91 headgear prefabs vanilla ships) **and** add the optional per-type loadout: `OVT_LoadoutUtils.ApplyCivilianLoadout(IEntity character, OVT_LoadoutConfig config)` overload with the existing one-arg signature delegating to the global config. This matters more than it looks: `ApplyCivilianLoadout` **overwrites top/pants/shoes/hat**, so without per-type clothing every variant would end up dressed identically regardless of its prefab (F-G). Ship per-type files for at least businessman and construction worker.
3. **T4.3** Add `[Attribute()] ref array<string> m_aCivilianTypes` to `OVT_TownControllerComponent` (empty = "whatever this town's size allows"), resolved through `OVT_CivilianAmbienceMath.TypeAllowed`. Author it on Eden's cities in `Worlds/MP/OVT_Campaign_Eden_Layers/towns.layer` where the size default is not enough.
4. **T4.4** Author the type entries (weights + `m_eMinTownSize`) in `CivilianAmbience.conf`; keep `generic` unrestricted so a village is never empty.
5. **T4.5** Logic case for the filter (min-size path and explicit-allow-list path); Init case that a per-town instance resolves a different allowed set for a CITY than for a VILLAGE.

**Acceptance criteria**

- compile 0; Fast green.
- Every new prefab opens clean in Workbench, and a spawned variant offers all six user actions.
- Play-test: a city crowd shows visibly different people; a village shows none of the city-only types.

---

### Phase 5 — Ambient parked civilian vehicles *(final, explicitly droppable)*

**Agent:** `component-developer`
**Estimate:** 8–12 h (spike included). **Droppable:** the feature ships its full value without this phase; drop it if the epic needs the time for `movement`/`integration`.

**Tasks**

1. **T5.1 Placement spike (time-boxed, ~2 h, do this first).** The road-network risk is **much smaller than assumed**: `OVT_WorldUtils.FindNearestRoadSpawn(center, maxDistance, out position, out angles)` (`OVT_WorldUtils.c:259-337`) already does `GetClosestRoad` → `GetPoints` → nearest-segment projection → `Math3D.DirectionAndUpMatrix` road-aligned angles, and is production code with three callers (`OVT_TravelRequestComponent.c:384,475`). `OVT_VehicleManagerComponent.FindNearestKerbParking` (:217-255) already parks vehicles **along kerbs** by matching `Pavement_`/`Kerb_` static models. Spike output: for 3 towns of different sizes, how many kerb spots and how many road-derived spots are found inside the town range, and whether a lateral offset from the road centreline lands on the shoulder or in the lane. **Decide from numbers:** kerb-first with road-derived fallback, or road-derived only. **Bail-out** if both disappoint: authored prefab pool placed at road points with a fixed lateral offset and no cleverness.
2. **T5.2** `OVT_TownVehicleSourceConfig` + a `town_vehicles` source in the same `.conf`; count scaled by town size (villages 0–1, cities 4–8), vehicle pool authored as `m_aPrefabs` (the base-class array — no new plumbing; `m_CivilianVehicleEntityCatalog` is bound to a vanilla conf with **zero entries** and stays out of this feature's way).
3. **T5.3 Untrack at spawn.** Vehicles carry native persistence components, so an ambient vehicle would write a save record and come back **duplicated** on load — the BUG-118 shape. Call `OVT_PersistenceManagerComponent.UntrackTransient(vehicle)` immediately after spawn, exactly as the gun dealer does (`OVT_TownController.c:300`).
4. **T5.4 Release on first player entry.** Trigger: the vehicle's compartment-entry signal (preferred — explicit and immediate) → `ReleaseAmbientEntity(vehicle)` → **re-track it for persistence** (`CancelUntrackTransient` + `Track`, the recruit-body precedent) and register it with `OVT_VehicleManagerComponent` so the rest of Overthrow sees it. Core's `OnEntityDespawning` release hatch stays as the belt-and-braces path for an occupied vehicle at despawn time.
5. **T5.5** Obstruction check before spawning: `TraceBox` with the vehicle-sized box already used by `OVT_WorldUtils.FindVehicleSpawnNear` (`Mins "-1.5 0 -3"` / `Maxs "1.5 2.5 3"`), rejecting on `TracePosition() < 0` (the `OVT_ParkingComponent` idiom — **not** the `>= 0` predicate BUG-031 already caught). Do **not** use `FindSafeSpawnPosition` (2 m probe radius, a known trap for vehicle-sized callers) or `OVT_EntitySpawningAPI.ValidateSpawnPosition` (sticky state).
6. **T5.6** Init case that the vehicle source resolves and rolls a vehicle prefab; play-test steps for placement quality and the take-a-car path.

**Acceptance criteria**

- compile 0; **All** green (persistence tracking is touched).
- Play-test: parked cars sit alongside roads facing along them, none inside a building or on the pavement centre, none in the sea.
- Play-test: drive an ambient car out of town, leave, return → **the car is still where you left it** and is in the save after a quit→Continue.
- A save taken next to an untouched ambient crowd of vehicles contains **no** records for them.

---

### Phase 6 — Help & documentation sync

**Agent:** `help-docs-sync`
**Estimate:** 2–3 h

Players see new things (varied civilians, town-local QRF behaviour) and server operators get two new knobs, so the closing sync is in scope.

1. **T6.1** Wiki server-configuration page: `civilianDensityMultiplier` and `maxCiviliansPerTown` documented with their defaults, the "0 = no civilians" behaviour and the "spawn distance rides `virtualizationSpawnDistance`" note.
2. **T6.2** Fact-check every existing civilian mention in `Configs/Tutorials/` and `Configs/FieldManual/` against the shipped code (recruit/convert/sell still true; anything about civilians "always" being present near towns needs the QRF-locality nuance). **Cite a file:line or cut the sentence.**
3. **T6.3** Modder-facing note (wiki): how to add a civilian type or a whole ambient source by editing `CivilianAmbience.conf`, with the per-town `m_aCivilianTypes` override.

**Acceptance criteria** — all three surfaces (tutorials, Field Manual, wiki) agree with the code; no invented mechanics; the suites are untouched (docs-only phase, gate skipped and said so).

---

## 5. Key Technical Decisions

### D1 — Variety comes from Overthrow-authored prefab variants, not the CIV entity catalog *(user amendment, 2026-08-17)*

Seeding the pool from vanilla's CIV catalog is **dead**: those 29 catalog prefabs carry none of Overthrow's civilian components — no `OVT_PlayerWantedComponent`, no `OVT_PlayerOwnerComponent`, and none of the six user actions — so a catalog-spawned civilian could not be recruited, converted or sold to, breaking the "player interactions preserved" requirement outright. Instead: 4–6 authored pairs (`Character_CIV_<type>.et` delta over a vanilla look + `Group_CIV_<type>.et` delta), **plus** widened clothing, **plus** per-town type filtering. The per-type loadout override is what actually makes the variants visible, because `ApplyCivilianLoadout` replaces the visible clothing slots on every civilian regardless of prefab.

### D2 — Parked vehicles derive their spots from roadside geometry, never from `OVT_ParkingComponent` *(user amendment, 2026-08-17)*

Only **11** building prefabs in the tree carry `OVT_ParkingComponent`; town-wide ambient parking on authored spots would mean authoring thousands of points. Positions are derived from the world instead — kerb geometry first (`FindNearestKerbParking` already matches `Pavement_`/`Kerb_` models and yaws the car along the kerb) and the road network second (`FindNearestRoadSpawn` already returns a projected road point **with road-aligned angles**). Both are existing, production-exercised Overthrow code, which is why the "no vanilla call site for `GetClosestRoad`" memory is a smaller risk here than it looks: Overthrow is the call site.

### D3 — Group-shaped ambient civilians *(user-approved)*

Each civilian stays a one-man `SCR_AIGroup` and the source's entity list holds **group** entities. Rationale: waypoints attach to groups, `GetOnAgentAdded` is the clothes/wanted hook, and the recruit path already reparents agents between groups — today's proven model. The cost is that core's stock dead-check (a damage manager on the tracked entity) cannot see a group, which is what [D4](#d4--two-additive-core-hooks-not-a-kill-invoker-subscription) buys.

### D4 — Two additive core hooks, not a kill-invoker subscription

`IsEntityDead` + `OnEntityPruned` on the config (§3.4). The alternative — subscribing `OVT_OverthrowGameMode.GetOnCharacterKilled()` and cleaning up companions there — needs a global subscription, a character→agent→group→source resolution, and its own teardown-vs-death disambiguation, to reach a conclusion core already reaches. The hooks are also the seam Phase 5's wrecks and any future ambient consumer will want. **Only one is built.**

### D5 — A new Manager, and the per-town config instance is the "controller"

See §3.2. `RollCount()` takes no arguments, so per-town state (town id, live population source, allowed types, radius) **must** live on the config instance — the authored `.conf` supplies templates, the manager constructs one bound instance per town. The instance reads the template by reference rather than copying fields, so a new template attribute cannot go missing.

> **Amendment (user, 2026-08-17, mid-Phase-3):** QRF suppression is now **off by default**. The always-despawn was a perf shortcut from when Reforger AI was expensive; players recruit civilians to fight the QRF and they vanished mid-recruit. A third config-struct key, `despawnCiviliansDuringQRF` (default `false`, server-only), gates the town-local suppression T3.1 builds — the invoker/unregister mechanics are unchanged, only the default flips. I4's "exactly two fields" becomes three under this amendment.

### D6 — QRF suppression is town-local, and base QRFs do not suppress town civilians

`m_bQRFActive` / `m_iCurrentQRFTown` are already public and replicated (`OVT_OccupyingFactionManager.c:161-164`); suppression becomes "this town's id equals the active QRF town id", delivered by one additive invoker rather than a poll. **Base QRFs deliberately do not suppress**: they happen at bases, not in towns, and the old global despawn was a performance shortcut, not a design. If a base QRF ever proves to need it, `m_vQRFLocation` (:162) makes a distance test a one-line follow-up — not built now (YAGNI).

### D7 — Spawn distance rides `virtualizationSpawnDistance`

Per the requirements, the authored source ships `m_iSpawnDistanceOverride = -1`. Note the consequence honestly: the global default is **1750 m** where the retired path used 1000 m, so more towns are populated simultaneously than before. The bounding controls are the per-town hard cap and the density multiplier, and the `.conf`'s override field remains the zero-code escape hatch for an operator who wants the old 1000 m ring back. **Measure the AI count in the Phase 2 play-test** before assuming it is fine.

### D8 — A killed civilian is not replaced until the next approach

Core's activation spends a **cursor**, not a headcount (`OVT_AmbientSpawnSourceInstance.HasPendingSpawns` compares `m_iSpawnCursor` to `m_iRollTarget`), so a pruned civilian is not re-spawned during the same activation. This is kept deliberately: a crowd that refills itself while a player is shooting into it is an immersion break and a farming loop. The crowd thins as civilians die and is re-rolled fresh on the next approach — exactly the ambient contract. It also bounds the blast radius of an engine budget **eviction** being read as death: fewer civilians for this visit, nothing worse.

### D9 — Nothing civilian-shaped is ever persisted

No serializer, no payload, no Persistence-tier test. Ambient civilians are untracked by the modded `SCR_AIGroup` chokepoint (BUG-118) and their corpses die with the session. The **only** civilians that persist are recruited ones, which the recruit manager already tracks (`CancelUntrackTransient` + `Track`, BUG-131) — and which are out of ambient ownership by then. Phase 5's vehicles are the exception that proves the rule and need [T5.3](#phase-5--ambient-parked-civilian-vehicles-final-explicitly-droppable) to stay out of the save.

### D10 — Civilians stay at vanilla's `LOW` AI-importance tier

Ambient entities are not registered groups, so nothing stamps importance. That is correct here: `api.md` §5 names `LOW` as the tier for "ambient feel and filler — civilians, decorative traffic", i.e. the first thing evicted when a firefight needs the budget. No action, recorded so nobody "fixes" it later.

### D11 — Fix F-B rather than port it

The wanted-disable is rewritten to take an `AIAgent` and resolve `GetControlledEntity()`. This is a behaviour change (civilians stop being wanted-system participants for the first time), so it is called out in `context.md` and in the play-test steps rather than slipped in silently.

### D12 — The teardown seam is new, and deliberately narrow

`OVT_TownControllerComponent` gets its first `OnDelete`, calling `DeactivateTown`. `CheckUpdateFlag`'s repeating 10 s `CallLater` (`:100`) is **out of scope** and stays un-removed — noted in `context.md` as a known leftover so the next person does not think it was missed.

---

## 6. Definition of Done

An evaluator with **no implementation context** should be able to verify every item below.

### Functional Criteria

- **F1** With no player near a town, that town contains **no** ambient civilians and the server log shows no civilian spawn activity.
- **F2** Walking or driving toward a town materialises the crowd **progressively over several ticks** — never all in one frame. With `m_bDebugAmbientLogging` on, one `ACTIVATED … rolled N` line is followed by repeated `spawned 3 this tick` lines until N is reached.
- **F3** Leaving the town (past the despawn ring, which is 1.15× the spawn ring) removes **every** civilian body and every waypoint entity belonging to them. Returning produces a **different** crowd (different count and/or different people).
- **F4** Killing a civilian leaves **the body in the world**, and the crowd does **not** conjure a replacement while you stand there. Its group entity and waypoints are gone within a few seconds (the next prune).
- **F5** Recruiting a civilian and then leaving and returning to the town leaves the recruit **alive and yours** — it is not deleted by the despawn and not counted in the next crowd.
- **F6** Convert-supporter and sell-drugs still work on an ambient civilian, and killing one still moves the town's stability/support (the civilian-death modifiers fire).
- **F7** A QRF in town A does not remove civilians from town B. When the QRF ends, town A repopulates on the next approach.
- **F8** Setting `civilianDensityMultiplier` to `2.0` in `$profile:Overthrow_Config.json` roughly doubles a town's crowd; setting it to `0` produces **no civilians anywhere**; `maxCiviliansPerTown` caps the largest city regardless of population.
- **F9** A city and a village show **different civilian types** (per-town filtering observable), and within a city the crowd is visibly not all the same person.
- **F10** Civilians pause where their archetype says they should — a loiterer stands at its spot for its configured seconds instead of pacing continuously.
- **F11** *(Phase 5 only)* Parked civilian vehicles appear alongside roads and kerbs, aligned with the road, none in a building or in the sea; a vehicle a player drives away survives the town's next despawn **and** a quit→Continue.

### Quality Criteria

- **Q1** `tools/compile-check.sh` exits **0** with no output.
- **Q2** Fast (`{6A6E29FF47ECB840}`) and All (`{6A6E2A002F53A581}`) both exit **0**, with the epic kill switch still **on** and every remaining `OVT-VIRT-PLAYTEST-ONLY` guard intact.
- **Q3** Every new test case has a recorded proof that it can fail — the exact edit, in a preamble comment. **No `maxAttempts` anywhere.**
- **Q4 No waypoint leak.** Every `AIWaypoint` creation site under `Scripts/Game/GameMode/Civilians/` is paired with a deletion site, and a live 10-cycle approach/withdraw test leaves the world's `AIWaypoint` entity count back at its starting value (±0).
- **Q5 No character leak.** After a full approach → withdraw cycle, no civilian character entities remain (this is the F-A defect, and the migration is what fixes it).
- **Q6** Nothing in this feature replicates: `grep -rn "Rpc\|RplProp\|Replication.Bump\|RplSave\|RplLoad" Scripts/Game/GameMode/Civilians/` is empty.
- **Q7** No hand-rolled proximity: `grep -rn "PlayerInRange\|NearestPlayer" Scripts/Game/GameMode/Civilians/` is empty, and no `CallLater` exists anywhere in the feature.
- **Q8** The Logic-tier file contains no manager-accessor or game-mode-getter identifier, in code **or** comments.
- **Q9** Nothing civilian-shaped appears in any save: no new serializer, no entry added to `Configs/Systems/Persistence/Overthrow.conf`.
- **Q10** The retired symbols are gone tree-wide (T2.8's grep), and `OVT_TownController.c` is net-shorter.

### Integration Criteria

- **I1 Core is only extended, never changed.** `git diff Scripts/Game/GameMode/Virtualization/` shows exactly two added methods and two added call sites; `api.md` records them as additive and `core/context.md` carries the dated note.
- **I2 Recruit path.** `OVT_RecruitManagerComponent.RecruitCivilian()` gains exactly one call, positioned after the recruit record is created and before `AddRecruitToPlayerGroup`. Tent recruiting (the non-ambient caller) is unaffected.
- **I3 Occupying faction.** `OVT_OccupyingFactionManager` gains exactly one invoker field and two invoke sites; `m_bQRFActive` / `m_iCurrentQRFTown` semantics are unchanged.
- **I4 Config seam.** `OVT_OverthrowConfigStruct` gains exactly two fields; `RplSave`/`RplLoad` and `CONFIG_STREAM_VERSION` (3) are untouched; an `Overthrow_Config.json` written before this feature still loads and behaves at parity.
- **I5 Towns data.** `OVT_TownData` is unchanged — population, range and location stay the density inputs, and `OVT_PatrolHarassmentStabilityModifier` (which reads deployments, not civilians) is untouched.
- **I6 Game mode.** One field, one `EOnInit` block and one `PostGameStart` block, after Virtualization, in the existing style; one component and one registry binding added to the prefab, one (`m_pCivilianPrefab`) removed.

### Verification Method

**Automated — from the repo root, in order:**

1. `tools/compile-check.sh` → exit **0**.
2. `tools/run-tests.sh "{6A6E29FF47ECB840}"` → exit **0** (Fast).
3. `tools/run-tests.sh "{6A6E2A002F53A581}"` → exit **0** (All).
4. `grep -rn "m_aCivilians\|CheckSpawnCivilian\|SpawnCivilians\|DespawnCivilians\|m_pCivilianPrefab\|m_fCivilianSpawnRate\|m_iCivilianSpawnDistance" Scripts/ Prefabs/` → **empty**.
5. `grep -rn "Rpc\|RplProp\|PlayerInRange\|NearestPlayer\|CallLater" Scripts/Game/GameMode/Civilians/` → **empty**.
6. `git diff Configs/Systems/Persistence/Overthrow.conf` → **empty** (Q9).
7. `git diff --stat Scripts/Game/GameMode/Virtualization/` → two methods, two call sites, nothing else (I1).

**Manual — solo play-test.** Set `m_bDebugAmbientLogging = true` on `OVT_VirtualizationManagerComponent` (game-mode prefab) — that is the only flag needed; the epic kill switch stays **on** so no legacy AI noise competes.

1. Stand 2 km from a city. **Expect:** no civilians; no ambient log lines for that town. → F1
2. Walk in. **Expect:** one `ACTIVATED … rolled N` line, then `spawned 3 this tick` batches until N; no frame hitch; civilians appear over ~10–20 s. → F2, Quality Bar "no hitches"
3. Note where they are. **Expect:** people near doorways and POIs as well as on streets; nobody in the sea or inside geometry. → Quality Bar "believable placement"
4. Withdraw ~2 km, then return. **Expect:** first crowd fully gone (bodies **and** waypoints), second crowd different. → F3
5. Before withdrawing, use the Workbench entity browser (or a Script Console count) to record the number of `AIWaypoint` and civilian character entities; repeat after 10 approach/withdraw cycles. **Expect:** the same counts. → Q4, Q5
6. Shoot one civilian. **Expect:** the body stays; the crowd does not replace it; within seconds its group entity and waypoints are gone from the entity browser. → F4
7. Recruit a civilian, withdraw past the despawn ring, return. **Expect:** the recruit is alive, still yours, still in your group; the rest of the crowd was re-rolled. → F5
8. Convert a supporter and sell drugs to two other civilians; kill a third and watch town stability/support move. **Expect:** all three behave exactly as before the migration. → F6
9. Trigger a QRF in town A (flag action) while standing in town B. **Expect:** B keeps its crowd; A loses its own. End the QRF, approach A. **Expect:** fresh crowd. → F7
10. Edit `$profile:Overthrow_Config.json` (dedicated server): `civilianDensityMultiplier 2.0` → restart → roughly double; `0` → none anywhere; `maxCiviliansPerTown 5` → the biggest city has at most 5. → F8
11. Compare a city crowd with a village crowd. **Expect:** different type mix, and no city-only type in the village. → F9
12. Watch one loiterer for a minute. **Expect:** it stands still for its configured pause. → F10
13. *(Phase 5)* Walk the town's streets. **Expect:** parked cars along kerbs/roads, aligned, unobstructed. Drive one out, leave, return, then save → quit → Continue. **Expect:** it is where you left it, both times. → F11

---

## 7. Testing Strategy

**The automated spine covers the maths, the config resolution and the seam wiring. Everything about how a crowd *feels* is a play-test — the suites cannot see a hitch, a leak over time, or a civilian standing in a wall.**

### Logic tier — `TestSuites/Logic/OVT_TEST_Logic_CivilianAmbience.c` (Fast)

World-free assertions on `OVT_CivilianAmbienceMath`: the density formula at parity numbers, both clamps, `multiplier = 0`, `population = 0`, the hard cap and the uncapped `hardCap <= 0` case, weighted picking at the first/last/out-of-range roll, and type filtering by min town size and by explicit allow-list. **The tier's grep rule bans manager-accessor and game-mode-getter identifiers anywhere in the directory, comments included** — which is exactly why the density maths lives in a static and not on the config class.

### Init tier — additions to `OVT_TEST_InitSuite.c` (Fast)

Modelled on the existing ambient cases (~:4582-4650, :4744, :4880) and the config-struct case (:3442-3501):

- the authored registry loads and `FindAmbientSourceConfig("town_civilians")` returns an `OVT_CivilianAmbienceConfig`;
- a per-town instance built from the template exposes the template's rate/pool plus the town's radius and allowed types, and resolves a **different** allowed set for a CITY than a VILLAGE;
- `civilianDensityMultiplier` / `maxCiviliansPerTown` read back their defaults through `OVT_Global.GetConfig()`;
- a config subclass's overridden `IsEntityDead` is the one core calls (virtual dispatch through a base-typed reference);
- the civilians manager resolves through `OVT_Global`, and `ReleaseRecruitedCivilian` on null / on a non-ambient character is a safe no-op.

Everything asserted here is **registration-level**: no case waits for an activation, because the Fast world has no observers and the All world's observer set is not a fixture.

### Persistence tier — **not needed, and that is a deliberate assertion**

Nothing this feature creates is ever persisted (D9). There is no serializer to round-trip, no payload field to version, and no entry in `Configs/Systems/Persistence/Overthrow.conf` — Q9's empty diff *is* the coverage. The one persistence-adjacent behaviour, a recruited civilian surviving a save, belongs to the recruit manager's existing tests. Phase 5's vehicles add one persistence-shaped requirement (untracked while ambient, tracked once claimed) which is verified by the §6 step 13 play-test and a "no vehicle records in a save near an untouched crowd" check, not by a new round-trip case.

### Every new case must be proven able to fail once

Exact edit recorded in a preamble comment. `maxAttempts` is banned project-wide.

### Not automatable, and why

| Area | Why manual |
|---|---|
| Spawn feel and frame-spread | A perception/performance judgement, not an assertion |
| Waypoint / character / husk leaks | Needs repeated live cycles and an entity count over time |
| Placement believability | Nobody can assert "that looks like a doorway" |
| QRF locality | Needs two towns, a real QRF and a player standing in the other one |
| Interaction regression (recruit / convert / sell) | User actions need a real player and a real action context |
| Per-town type filtering | Needs eyes on two towns of different sizes |
| Vehicle placement quality | Same, plus the take-a-car path |

---

## 8. Dependencies

**Hard preconditions (all satisfied today):**

- **`virtualization/core` complete and frozen** — the ambient seam (`RegisterAmbientSource`, `UnregisterAmbientSource`, `GetAmbientEntities`, `ReleaseAmbientEntity`, `FindAmbientSourceConfig`, the config/registry classes and the tick) is live as of core Phase 4/6. `api.md` §10's `civilians` row is the contract; the two additive hooks of §3.4 are this feature's only core change.
- **Towns system** — `OVT_TownControllerComponent` / `OVT_TownManagerComponent` provide activation, location, range, size and **live population**; `OVT_TownData` stays the persisted numeric truth and is not modified.
- **`OVT_RecruitManagerComponent.RecruitCivilian()`** — the single server chokepoint both recruit callers funnel through.
- **`OVT_OccupyingFactionManager`** — public `m_bQRFActive` / `m_iCurrentQRFTown`, already replicated, plus the existing public-invoker style to copy.
- **`Configs/Civilians/CivilianClothes.conf`** + `OVT_LoadoutUtils`; the vanilla 1.8.0.10 reference tree at `/mnt/n/Projects/Arma 4/ArmaReforger` for the CIV look prefabs and clothing pool.
- **Phase 5 only:** `OVT_WorldUtils.FindNearestRoadSpawn`, `OVT_VehicleManagerComponent.FindNearestKerbParking`, `OVT_PersistenceManagerComponent.UntrackTransient`.

**Explicitly NOT depended on:**

- **`virtualization/movement`** — ambient spawns have no despawned life to advance. The two features are the epic's only parallelizable pair and must not acquire a dependency in either direction.
- Anything in `integration` or `base-defense-migration`; the epic kill switch stays in the tree until epic end and this feature deletes only the single guard line that sits on the code it removes.

**User-side (Workbench, interactive):** adding `OVT_CivilianAmbienceManagerComponent` and the `m_AmbientRegistry` binding to `Prefabs/GameMode/OVT_OverthrowGameMode.et` (T2.4/T2.6, text-wired by an agent and verified by the user — core's T2.8 precedent); verifying the new character/group prefabs open clean (T4.1); authoring `m_aCivilianTypes` on Eden's towns (T4.3); the play-test passes of §6.

---

## 9. Risks & Mitigation

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| **R1** | **The deferred-member dead-check misfires** — a civilian is pruned before its member ever spawns (crowd silently thins to nothing), or a corpse is never pruned (waypoints leak). | Medium | The whole crowd model | `m_bSeenAgent` gates the predicate (§3.4); the Init case proves the override is dispatched; §6 steps 2/6 are the direct live checks. A misfire is loud (empty town or growing entity count), not silent. |
| **R2** | **Engine budget eviction reads as death** — a firefight evicts a LOW-tier civilian group's member and we delete the group. | Medium | Thinner crowds during combat | Accepted by D8: fewer civilians for this visit, re-rolled on the next approach. Documented, not engineered around; if it ever proves ugly, the fix is a `GetOnMembersDespawning` guard, one subscription. |
| **R3** | **F-A is wrong** and member characters *are* deleted with their group after all, making the explicit member deletion redundant. | Low | Harmless | The explicit deletion is idempotent — deleting an already-deleted entity is a null check. Confirm in the Phase 2 play-test with an entity count (§6 step 5), which is owed anyway. |
| **R4** | **1750 m spawn ring populates half the map's towns at once** (D7), pushing AI counts past today's. | Medium | Server frame time | Hard per-town cap + density multiplier are the throttles; the `.conf`'s `m_iSpawnDistanceOverride` restores 1000 m with no code change. **Measure the live AI count in the Phase 2 play-test before enrichment lands.** |
| **R5** | **Road/kerb placement disappoints** for parked vehicles — too few kerb models in small towns, or road points land in the driving lane. | Medium | Phase 5 quality | T5.1 is a time-boxed spike that answers with numbers before any code is written, with a documented bail-out (authored pool + fixed lateral offset). The phase is droppable in full. |
| **R6** | **Ambient vehicles pollute the save** — vehicles are natively tracked, so every parked car writes a record and duplicates on load (the BUG-118 shape). | High if not designed against | Save bloat, duplicated cars | T5.3 untracks at spawn, T5.4 re-tracks on claim; verified by a "no vehicle records near an untouched crowd" save check. |
| **R7** | **Prefab variants ship missing a vanilla component** — Overthrow prefabs have lost an `RplComponent` and a faction-delegate component before. | Medium | Actions or replication silently broken on one variant | T4.1 requires a diff of each new prefab against `Character_CIV.et` **and** a Workbench open; §6 F9's play-test exercises all six actions on a variant. |
| **R8** | **A town near campaign start never goes dormant** — observers include the GM camera, the deploy-point MP insert and the optics far-observer, so a source can stay permanently activated. | Medium | One town permanently populated | Accepted epic-wide (`api.md` D2 finding 3). It costs one town's civilians, and the density cap bounds that. Do not add a player-distance loop to "fix" it — that would make "nearby" mean two different things. |
| **R9** | **The recruit release lands at the wrong point** — released too early (a refused recruit leaks an unowned civilian) or too late (the agent has already been reparented and the group resolution fails). | Medium | Orphaned civilians or deleted recruits | T2.9 pins the position between `SetRecruitFaction` and `AddRecruitToPlayerGroup`, with the reasoning in a code comment; §6 step 7 is the direct check; `ReleaseAmbientEntity` on a non-ambient entity is a documented safe no-op. |
| **R10** | **Concurrent sessions move the tree** — bugfix sessions and `movement` will land between planning and implementation. | High | Stale line numbers | Every file:line here is dated 2026-08-17; re-grep before editing. Nothing in the plan depends on a line number for correctness. |
| **R11** | **The `.conf` authoring shape is wrong for a `ScriptAndConfig` subclass array** and the registry fails to resolve at runtime. | Low-Medium | Phase 2 blocked | Copy `Configs/Deployment/overthrowDeployments.conf`'s proven shape; T2.10's Init case (`FindAmbientSourceConfig` returns a subclass instance) is the automated proof, and it fails loudly rather than silently spawning nothing. |
| **R12** | **`SetHoldingTime` is a second no-op** because `AIWaypoint_Wait.et` does not author `m_TimedWaypointParameters`. | Medium | Archetype pauses still do nothing | T3.2 explicitly verifies the prefab, not just the call; §6 step 12 watches a loiterer with a stopwatch. |

---

## Agent Routing Summary

| Phase | Agent | Advanced? |
|---|---|---|
| 1 — Core seam, declarative model, density math | `component-developer-advanced` | **yes** — edits the epic's frozen core and its ownership bookkeeping |
| 2 — The migration (parity) | `component-developer-advanced` | **yes** — retires a live system, edits `OVT_RecruitManagerComponent`, owns entity lifetime |
| 3 — QRF locality, wait fix, placement, archetypes | `component-developer` | no (T3.1 is a small additive manager edit — review it as one) |
| 4 — Variety: prefabs, clothing, per-town filtering | `component-developer` + user Workbench pass | no |
| 5 — Ambient parked vehicles *(droppable)* | `component-developer` | no |
| 6 — Help & documentation sync | `help-docs-sync` | no |

**Skills to activate:** `enforcescript-patterns` (all phases), `overthrow-architecture` (1, 2, 3, 5), `workbench-workflow` (2, 4, 5).
