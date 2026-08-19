# Deployments - Implementation Plan (Retrospective)

**Status:** Implemented (Documented Retrospectively)
**Originally Implemented:** ~2025 (design doc archived as `docs/archive/ModularDeploymentSystem.md`)
**Documented:** 2026-08-02
**Last Updated:** 2026-08-02

---

## Executive Summary

The Deployments system is a modular, faction-agnostic framework for placing AI in the world: authored `OVT_DeploymentConfig`s compose **condition** modules (where/when a deployment may exist), **spawning** modules (what entities it owns) and **behavior** modules (what the AI does). A server-only manager evaluates candidate positions every 30 s, scores them by threat, and creates deployment marker entities that virtualize their forces by player proximity and persist through vanilla persistence with self-spawn.

It was designed to **replace** the BaseUpgrades system. In practice exactly one migration completed (town patrols): today it contributes three deployment types (town patrols + two rare vehicle patrols) while BaseUpgrades still owns all static base defense. Both run concurrently, funded from the same faction resource pool.

**Note:** This is a retrospective implementation plan created by analyzing the existing codebase. The feature has already been implemented.

---

## Goals

### Primary Goals
- A composable, authorable alternative to hard-coded upgrade classes: new AI presence = new `.conf`, not new code.
- Faction-agnostic (any faction could deploy), location-flag driven (town/base/tower/port/airfield/...).
- Virtualized (marker entity is the durable record; forces spawn only near players) and persistent (respawn-on-load via `SelfSpawn`).

### Success Criteria
- [x] Module pattern (condition/spawning/behavior) with prototype-clone instantiation from configs
- [x] Periodic candidate evaluation with threat scoring, cost/chance/priority/max-instance filters
- [x] Proximity activation/deactivation; stolen vehicles survive despawn (40 m rule)
- [x] Persistence: manager pools + per-deployment state, respawn-on-load, eliminated-flag ordering fixed for vanilla persistence
- [ ] The BaseUpgrades migration (Phases 3-4 of the design: composition/defense/logistics modules — never built)
- [ ] Resource-refund and threat bookkeeping on live instances (both fields never set at creation — see Known Issues)
- [ ] Any client-visible surface (no replication, no map/UI presence at all)

---

## Current Architecture

### Key Components
- `Scripts/Game/GameMode/Deployments/OVT_DeploymentManager.c` (1127 L) — `OVT_DeploymentManagerComponent`, server-only singleton on the game mode (`OVT_Global.GetDeploymentManager()`): registry, per-faction resource pools (`m_mFactionResources`), active-deployment index, the evaluate→score→create loop (30 s; first run +10 s after `PostGameStart`).
- `.../OVT_DeploymentComponent.c` (513 L) — per-instance component on `Prefabs/GameMode/OVT_Deployment.et` (bare GenericEntity + RplComponent): clones config modules, 10 s jittered update loop, proximity activation, eliminated flag, `ApplyPersistedDeployment`.
- `.../OVT_DeploymentConfig.c` — authored config: module list, faction/location flags, base cost, min threat, priority, chance, max instances. `.../OVT_DeploymentRegistry.c` — config container; `FindConfigByName` is the runtime key.
- `.../OVT_EntitySpawningAPI.c` — static spawning helper (only `SpawnInfantryGroup`/`CleanupGroup` actually used).
- `Modules/` — 4 base classes + concrete: conditions (`BaseControlCondition`, `TownConditional`), spawning (`InfantrySpawning`, `VehicleSpawning`), behavior (`PatrolBehavior`, `MultiTownPatrolBehavior`, `ReinforcementBehavior`).
- Configs: `Configs/Deployment/overthrowDeployments.conf` registry with three entries — **Town Patrol** (OF towns, cost 0, priority 1, proximity-gated), **Light Vehicle Patrol** (bases, cost 25, 2% chance, max 1, always-on), **Heavy Vehicle Patrol** (bases, cost 50, threat ≥ 1200, 1% chance, max 1, always-on).
- Persistence: `OVT_DeploymentManagerSerializer` (faction pools) + `OVT_DeploymentComponentSerializer` (config name, faction, threat, invested, eliminated) + `Overthrow.conf` entity rule (`SelfSpawn 1`, priority 35000).

### Data Flow
1. **Funding:** `OVT_OccupyingFactionManager.AllocateDeploymentResourcesIfNeeded` (the only funder) tops the OF pool up when it drops below 500, transferring up to half of each income tick.
2. **Evaluation** (30 s, skipped with zero players or during a QRF): for every engine faction — collect candidate positions (towns from the town manager, bases/towers from the faction manager; port/airfield/checkpoint getters are TODO stubs), filter by 100 m spacing + ground trace, score `global threat + GetThreatByLocation(pos)` ±20% jitter, sort descending.
3. **Selection:** per candidate, the valid configs (faction/location/cost/threat/static conditions/max instances) roll `m_fChance`; the lowest-priority winner is created — marker spawned, `OVT_PersistenceTracking.Track`ed, `InitializeDeployment` clones the modules and starts the update loop. Max 10 creations per faction per cycle; 100 active per faction cap.
4. **Instance loop** (10 s): tick modules (spawning→behavior→condition), then proximity check (`m_iMilitarySpawnDistance`) toggles Activate (spawn groups/vehicles, apply waypoints) / Deactivate (delete groups; vehicles only within 40 m of the marker). Configs with `m_bEnableProximityActivation` false (both vehicle patrols) activate once and never deactivate.
5. **Reinforcement:** the reinforcement behavior module re-buys eliminated infantry groups (cost per group, cooldowns) while conditions still pass; it is the *only* caller of runtime condition evaluation.
6. **Persistence:** marker entities self-spawn on load; `ApplyPersistedDeployment` sets scalars (eliminated flag **before** module init — the EPF-era ordering bug is documented in-code), resolves the config by name, spawns nothing (proximity re-spawns later). Missing config name → warning + silent drop.

### Integration Points
- **occupying/core** (sibling): sole funder; the manager reads `m_Bases`/`m_RadioTowers`/`GetNearestBase`/`GetThreatLevel` back. Same resource pool as base upgrades — the two systems compete.
- **occupying/qrf** (sibling): evaluation freezes while `m_CurrentQRF` is set (one-way); QRF counts deployment AI in its zone tally (unintended); `HasRecentBattleNearby` is a TODO stub returning false.
- **occupying/base-upgrades** (sibling): designated successor, one migration done (TownPatrol — commit `d62e441` moved it and added `OVT_PatrolHarassmentStabilityModifier`); everything else still BaseUpgrades. Deployments use the new faction group/vehicle registries; upgrades use the legacy prefab-slot arrays.
- **Towns:** candidate positions + `TownConditional` gates; `OVT_PatrolHarassmentStabilityModifier` is the **only consumer** of deployment state anywhere — it looks up the deployment *by the string literal "Town Patrol"* and toggles a town stability modifier while the patrol lives.
- **Bases:** `GetRandomVehiclePatrolSpawn()` for vehicle spawn points. It answers a position plus a **float heading** (yaw only, pitch/roll discarded) — a vehicle spawn must be upright, and a float cannot be handed to `Math3D.AnglesToMatrix` in the wrong slot the way a `GetAngles()` vector can. The heading goes into the **spawn transform** via `OVT_BaseSpawningDeploymentModule.GetUprightSpawnRotation()`; never re-orient a spawned vehicle with `SetAngles()` (that desynchronises the rigid body and is what put patrol vehicles on their noses).
- Nothing resistance-side ever creates a deployment despite the faction-agnostic design. `OVT_DeployFOBAction`/`OVT_UndeployFOBAction` are unrelated resistance FOB actions — naming overlap only (verified).

---

## Implementation Details

### Phase 1: Framework (COMPLETED)
Manager + component + config/registry + module base classes; prototype-clone pattern (`CloneModule` with hand-written per-class field copies).

### Phase 2: First Modules & Configs (COMPLETED)
Infantry/vehicle spawning, patrol/multi-town-patrol/reinforcement behaviors, base-control/town conditions; three authored configs; town-patrol migration from BaseUpgrades.

### Phase 3: Full Migration (NOT STARTED)
The design doc's composition/defense/logistics modules, proximity condition module, and the removal of BaseUpgrades never happened. Four of seven location flags (PORT, AIRFIELD, CHECKPOINT, OPEN_TERRAIN) can never produce candidates (stub getters).

### Phase 4: Faction Integration (NOT STARTED)
Resistance/civilian deployments unbuilt; evaluation still iterates every engine faction and discards the results.

---

## Key Technical Decisions

### Decision 1: Marker entity as the durable record
**Context:** Deployments must outlive their spawned forces and survive load.
**Implementation:** A bare replicated GenericEntity per deployment, persistence-tracked with `SelfSpawn`; forces are always reconstructable from config + flags.
**Trade-offs:** Clean virtualization and load story; but module-internal state (patrol routes, spawn counts, cooldowns) is not persisted — a mid-route patrol restarts on load.

### Decision 2: Prototype-clone modules
**Context:** Configs are shared; instances need private state.
**Implementation:** `Type().Spawn()` + hand-written `CopyTo` per concrete module.
**Trade-offs:** Simple; but every new attribute must be manually added to the clone list — `m_fMaxCruiseSpeed` already fell through this crack.

### Decision 3: Config-name string as the persistence + integration key
**Context:** Configs are authored data; entity IDs don't survive sessions.
**Implementation:** `FindConfigByName` on load; `GetDeploymentNearPosition("Town Patrol", ...)` in the stability modifier.
**Trade-offs:** Robust to entity churn; but renaming a config silently orphans every persisted instance *and* disables the stability modifier.

### Decision 4: Conditions gate creation, not existence
**Context:** (Apparent accident rather than decision.) `EvaluateCondition` is only called by the reinforcement module; nothing re-checks conditions on live deployments.
**Implementation:** A base flipping to the resistance leaves its vehicle patrols running forever.
**Trade-offs:** The design doc's `AreAllConditionsMet` runtime gate was never implemented.

---

## Current State

### What's Working
- Town patrols deploy to OF towns, scale with town size, reinforce, and drive the harassment stability modifier; the two vehicle patrols spawn and run multi-town routes; virtualization + persistence round the campaign correctly (including the eliminated-flag ordering fix from the vanilla-persistence migration).

### Known Issues
- **`m_iResourcesInvested` is never set at creation** — `CreateDeployment` debits the pool but never tells the instance; the multi-town patrol's resource-refund feature always refunds 0, and the persisted field is always 0.
- **`m_fThreatLevel` is never set at creation** — candidate threat is computed then discarded; `SetThreatLevel` has no callers; persisted threat is always 0.
- **`m_mFactionDeployments` leaks stale entity IDs** — cleanup prunes only `m_aActiveDeployments`; the per-faction list monotonically grows toward the 100 cap, after which the faction silently stops deploying. **This is the system's long-campaign kill switch.**
- **World-time unit mismatches:** `PatrolBehavior.m_fCheckInterval` (60 "seconds") and `TownConditional.m_fCacheTimeout` (30.0) compared against millisecond world time — the scan runs every tick and the cache never caches; the reinforcement module uses ms correctly, so both conventions coexist.
- **Waypoints double-inserted** into `m_aWaypoints` (module + caller) → double-delete on deactivate.
- **The marker entity teleports** to the last-spawned vehicle (`SetOrigin` inside the per-vehicle loop) — mutating the persisted position.
- **Runtime conditions effectively unwired** (see Decision 4).
- **Vehicle reinforcement configured but impossible** (`GetMissingUnitsCount` returns 0 for vehicle modules; no `Reinforce` on them).
- **No replication at all:** clients can't see deployment state; several manager getters would null-deref if a client called them (collections never allocated client-side).
- Malformed `string.Format` specifiers in three log lines; latent enum confusion (`OVT_FactionType` vs `OVT_FactionTypeFlag`) in two unused registry methods; `ValidateSpawnPosition`'s static latch bug (unused).

### Technical Debt
- Large dead surface: `m_fActivationRange` and `m_iResourceAllocation` authored-but-never-read; `RequiresSlots`/`GetRequiredSlotType` stubs; the whole-map slot query whose only consumer is a debug print; ~10 zero-caller manager methods (incl. byte-identical `DestroyDeployment`/`DeleteDeployment`); most of `OVT_EntitySpawningAPI`; the entire behavior-module base-class contract (both concrete behaviors bypass it); many unused condition helpers; `OVT_UndeployFOBAction_New.c` "example migration" file.
- Seven TODO stubs: port/airfield/checkpoint positions, water/airfield suitability, road intersections, `HasRecentBattleNearby`; the referenced "virtualization system" replacement doesn't exist; cruise-speed block commented out ("bugged in Reforger") while configs still author the value.
- Evaluation iterates every engine faction (civilians included) and discards the work; magic numbers throughout (2000 m threat radius, 250 m dedupe, ±0.2 jitter, 40 m vehicle rule, `SetMaxMembers(8)`, the `* 0.5` difficulty halving).
- `m_DeploymentConfig` doubles as prefab attribute and "already initialized" sentinel — authoring it on the prefab would break `ApplyPersistedDeployment`.
- `DestroyDeployment` deletes a persistence-tracked entity relying on vanilla noticing (no explicit untrack).

---

## Future Enhancements

### High Priority
- [ ] Fix the `m_mFactionDeployments` leak (the long-campaign deployment kill switch).
- [ ] Set `m_iResourcesInvested` and `m_fThreatLevel` at creation (unlocks the refund feature and honest persistence).
- [ ] Fix the two world-time unit bugs (patrol scan + town cache).

### Medium Priority
- [ ] Wire runtime condition evaluation into the instance update loop (`AreAllConditionsMet` from the design).
- [ ] Fix the waypoint double-insert/double-delete and the marker-teleport bug.
- [ ] Either implement vehicle reinforcement or stop authoring it.
- [ ] Skip factions with no usable configs in evaluation.

### Low Priority / Nice to Have
- [ ] Resume the BaseUpgrades migration (design Phases 3-4) or formally close it.
- [ ] Client-visible deployment surface (map markers) if ever desired.
- [ ] Dead-code sweep (the manager and `OVT_EntitySpawningAPI` especially); replace the "Town Patrol" magic-string coupling with a typed key.

---

## Testing

### Current Coverage
One assertion: `OVT_TEST_InitSuite` checks `GetDeploymentManager()` resolves. Both serializers are untested despite shipping in the vanilla-persistence migration; `ValidateAllConfigs()` (the registry's built-in validator) is never invoked by anything.

### Testing Gaps
- Init-tier: invoke `ValidateAllConfigs()` on the shipped registry — cheap and real.
- Logic-tier candidates: cost computation, candidate filtering/scoring (needs mild refactoring for injectability), `FindBestDeploymentConfig` selection rules.
- Persistence-tier: manager pool round trip; a deployment record round trip through `ApplyPersistedDeployment` (config-name resolution, eliminated-flag ordering).
- Not automatable: proximity activation, AI behavior, multi-town routes — play-testing.

---

## Documentation

### Current Documentation
- This retrospective plan; the archived design `docs/archive/ModularDeploymentSystem.md` (still the best statement of intent, including the never-executed migration strategy).

### Documentation Needs
- The coexistence-with-BaseUpgrades status (mirrored in the base-upgrades docs) and the "Town Patrol" string coupling.

---

## Dependencies

### External Dependencies
- Vanilla: `SCR_AIGroup` (+ `GetOnInit` async crewing), AI waypoints/cycle, compartments, road network queries, vanilla persistence self-spawn.

### Internal Dependencies
- occupying/core (funding + base/tower/threat reads), town system (candidates + conditions), faction group/vehicle registries (`OVT_Faction`), `OVT_OverthrowConfigComponent` (spawn distance, waypoint factories), vanilla persistence config (`Overthrow.conf` entity rule).

---

## Notes

**Discovered Information:**
- The eliminated-flag ordering in `ApplyPersistedDeployment` (set *before* module init) is a deliberate fix for an EPF-era bug that resurrected wiped-out forces on load — documented in-code.
- The design doc's migration strategy ("runs alongside … deprecated but continues functioning … remove once migrated") stalled after one migration; `OVT_DeploymentManager.c:118` still defers to "how slots are implemented in the base upgrade system".
- The 40 m vehicle-despawn rule is what lets players steal patrol vehicles permanently.

**Retrospective Assessment:**
- The module architecture is the right shape and the persistence story is the most modern in the epic (it got real attention during the vanilla migration).
- Its actual runtime footprint is small (three configs), and the unset-fields + faction-list-leak bugs suggest it shipped before its bookkeeping was finished.
- The strategic question is the migration: two force-placement systems sharing one budget is the epic's core architectural tension.

---

*This retrospective plan was created by analyzing existing code. Use `/start-feature occupying/deployments` to begin making improvements.*
