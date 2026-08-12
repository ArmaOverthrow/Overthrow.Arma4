# Base Upgrades - Implementation Plan (Retrospective)

**Status:** Implemented (Documented Retrospectively)
**Originally Implemented:** Unknown (inherited from early Overthrow Reforger development)
**Documented:** 2026-08-02
**Last Updated:** 2026-08-02

---

## Executive Summary

Base upgrades are how the occupying faction turns resources into physical presence at its bases: roaming defense patrols, static guards, tower snipers, road checkpoints, fortification compositions (bunker/ammo cache/MG nest), parked vehicles, and specops squads sent at resistance targets. They are config objects (not components) instantiated by the prefab attribute system on `OVT_BaseControllerComponent`, spent into by the faction manager's priority scheduler, virtualized by player proximity ("proxying"), and persisted as upgrade-state records replayed on load.

**Note:** This is a retrospective implementation plan created by analyzing the existing codebase. The feature has already been implemented. The archived `ModularDeploymentSystem.md` design names this system as *deprecated-in-intent* — the Deployments sibling was designed to replace it — but only one upgrade (TownPatrol) was ever migrated; base upgrades remain the live mechanism for all static base defense.

---

## Goals

### Primary Goals
- Visible, escalating base garrisons funded by the faction's resource economy.
- Cheap when unobserved: despawn AI when no player is near, refund the value, re-buy on approach.
- Survive save/load by replaying upgrade state rather than persisting AI entities.

### Success Criteria
- [x] Nine upgrade types registered on the base controller prefab, spending by priority under threat gates
- [x] Proxying (despawn/re-buy by player proximity, suspended during QRFs)
- [x] Upgrade state serialized through the faction-manager serializer and replayed on load
- [ ] Correct resource accounting (several leaks/inflations — see Known Issues)
- [ ] Checkpoint compositions returning after a load (they don't — guards do, compositions don't, and their slots stay blocked)

---

## Current Architecture

### Key Components
Class hierarchy (all extend `ScriptAndConfig` — config objects, server-only, one set per base):

- `OVT_BaseUpgrade` — abstract base: `m_iResourceAllocation` (-1 = spend recklessly), `m_iPriority`, `m_iMinimumThreat`, `Spend()`, `SpendToAllocation()`, `GetResources()`, `Serialize()`/`Deserialize()`.
  - `OVT_BaseUpgradeParkedVehicles` — cars/trucks into `OVT_ParkingComponent` spots (prio 10).
  - `OVT_BasePatrolUpgrade` — adds group ownership + proxying; parent of the rest:
    - `OVT_BaseUpgradeDefensePatrol` (prio 1) — roaming patrols, group type by threat (LIGHT→HEAVY→AT).
    - `OVT_BaseUpgradeDefensePosition` (prio 2) — static guards on SmartAction sentinel positions.
    - `OVT_BaseUpgradeTowerGuard` (prio 2) — snipers on `MDT_TOWER` buildings' CoverPost actions.
    - `OVT_BaseUpgradeCheckpoints` (prio 3) — road-slot checkpoint compositions (60/40 res) + guard patrols.
    - `OVT_BaseUpgradeSpecops` (prio 3) — squads sent at known targets; can start a QRF or recapture a radio tower; driven by the manager, not the normal spend path.
    - `OVT_BaseUpgradeTownPatrol` — **DEAD**: referenced by no prefab or config; superseded by the "Town Patrol" deployment (commit `d62e441`).
    - `OVT_SlottedBaseUpgrade` — slot selection + composition spawning + persistence tracking:
      - `OVT_BaseUpgradeComposition` ×3 tags — SmallBunker (prio 4), AmmoCache, MGNest (prio 1); fills ammo boxes, crews turrets.

Registration: `Configs/BaseUpgrades/overthrowBaseUpgrades.conf` (10 entries), referenced by `OVT_BaseControllerComponent.m_BaseUpgradesConfig` on `Prefabs/Controllers/OVT_BaseController.et` (moved off the prefab 2026-08-13 for per-base/modder customization). World base instances in `bases.layer` never override the upgrade list, but now can by delta-overriding the config object.

### Data Flow
- **Funding:** `OVT_OccupyingFactionManager` calls `base.SpendResources(resources, threat)` at campaign start (`DistributeInitialResources`) and every 6-hour tick. The controller loops priority 1..19, skipping upgrades gated by `m_iMinimumThreat`; allocation-capped upgrades spend to `m_iResourceAllocation × baseResourceCost`, `-1` spends the remainder.
- **Slots:** `FindSlots()` buckets `SCR_EditableEntityComponent` slots by size/road labels within `baseRange` (280 m); `m_aSlotsFilled` is the shared occupancy set; sentinel positions become defend positions; `OVT_VehiclePatrolSpawn` entities feed the vehicle systems.
- **Proxying:** each patrol upgrade ticks ~10 s; out of `m_iMilitarySpawnDistance` (or during any QRF) groups are deleted and their value banked in `m_iProxedResources` + prefab/position lists; in range they are re-bought. `GetResources()` = live agents × `baseResourceCost` + proxied bank — the value `SpendToAllocation` measures against.
- **Persistence:** upgrades serialize into `OVT_BaseUpgradeData {type, resources, groups[], tag, pos}` via the faction-manager serializer, matched on load by `FindUpgrade(type, tag)` and replayed (`BuyPatrol` per recorded group / `Spend(resources)`). Physical slotted compositions are the one entity-level exception: `SpawnInSlot` calls `OVT_PersistenceTracking.Track()` so vanilla persistence restores them (AI self-spawn is disabled in `Overthrow.conf` to avoid double-spawn). Checkpoint compositions are *not* tracked (see Known Issues).

### Integration Points
- **occupying/core** (sibling): the sole funder and scheduler; `RecoverResources` refund hook (only the dead TownPatrol used it); `UpdateSpecops` drives the specops upgrade from the known-target board.
- **occupying/qrf** (sibling): patrol upgrades suppress spawning while `m_CurrentQRF` is set; specops starts base QRFs; the base controller supplies QRF attack geometry.
- **occupying/deployments** (sibling): the designated successor that actually replaced only town patrols; both systems run concurrently and compete for the same `m_iResources` pool. Base upgrades use the *legacy* faction prefab-slot arrays; deployments use the newer registries.
- **Resistance:** reads `m_AllCloseSlots` for player-base garrison waypoints; `SpawnGarrison` handles the resistance half of a captured base.
- **Factions/config:** composition tags+costs from `*_OverthrowData.conf`; group prefabs from the legacy slot arrays; waypoints via `OVT_OverthrowConfigComponent.Spawn*Waypoint`.

---

## Implementation Details

### Phase 1: Upgrade Framework (COMPLETED)
Base class + priority scheduler + allocation model; per-upgrade repeating `CallLater` ticks (~10 s, jittered).

### Phase 2: Concrete Upgrades (COMPLETED)
Nine upgrade types (table in §Current Architecture). Costs: infantry group `baseResourceCost×4` (=40), tower sniper ×1, car ×3, truck ×6, checkpoints hardcoded 60/40, compositions raw `m_iCost` (9–15).

### Phase 3: Proxying & Persistence (COMPLETED)
Despawn/re-buy virtualization; serializer replay contract; slotted-composition persistence tracking (replaced EPF's `OVT_BaseUpgradeSaveData` — the 14-line comment in `SpawnInSlot` documents the migration).

### Phase 4: Migration to Deployments (STALLED)
TownPatrol migrated (and its upgrade class orphaned); the archived design's remaining phases (composition/defense/logistics modules) never happened. Base upgrades remain the live system for static defense.

---

## Key Technical Decisions

### Decision 1: Config objects, not components
**Context:** Upgrades are pure server logic with authored tuning.
**Implementation:** `ScriptAndConfig` subclasses in a prefab attribute array; no entities, no replication of their own.
**Trade-offs:** Zero network/prefab overhead and easy authoring; but no per-instance world overrides are used, and lifecycle (timers) is hand-managed — none of the repeating `CallLater`s are ever removed.

### Decision 2: Value-banked proxying
**Context:** A whole island of garrisons can't stay spawned.
**Implementation:** Despawned groups convert to `m_iProxedResources` (+ prefab/position replay lists); `GetResources()` counts both live and banked value.
**Trade-offs:** Cheap and save-friendly; but two subclasses bank without ever clearing the bank (monotonic inflation — see Known Issues), and the counters drift.

### Decision 3: Replay-based persistence with one tracked exception
**Context:** Same respawn-not-restore model as the faction manager.
**Implementation:** Upgrade-state records replayed on load; only slotted compositions are entity-tracked.
**Trade-offs:** Idempotent and EPF-free; but every upgrade that opts out of `Serialize()` (parked vehicles, specops) silently re-buys or forgets, and checkpoints landed in the worst spot: slots restored as filled, compositions gone.

---

## Current State

### What's Working
- All eight live upgrades spawn, spend, proxy and (mostly) persist; bases visibly garrison and fortify; specops missions run including radio-tower recapture; the QRF suppression seam works.

### Known Issues
- **Allocation clamp is dead** (`OVT_BaseControllerComponent.c:294-301`): computed `allocate` is discarded; `SpendToAllocation` recomputes from the attribute, so an upgrade can overspend the manager's remaining pool.
- **Proxied-resource inflation:** `DefensePosition` and `TowerGuard` never reset `m_iProxedResources` after re-spending it (unlike the parent), so `GetResources()` inflates without bound and `SpendToAllocation` goes permanently negative.
- **Overspend banked before clamping** (`OVT_BasePatrolUpgrade.c:161-172`, `DefensePatrol.c:46-53`): `m_iProxedResources += newres` happens before `newres` is clamped to available resources (both sites carry the To-Do).
- **`m_iNumGroups` double-increments** (BuyPatrol + Spend for the same logical group) and only `CheckClean` decrements — drifts upward; DefensePatrol branches on it for group type.
- **Checkpoint spend-then-check** (`Checkpoints.c:16-19`): the large-checkpoint branch spends 60 and fills the slot before null-checking the spawn (medium branch checks); failed spawn = burned resources + permanently blocked slot.
- **Checkpoints don't persist:** compositions aren't tracked and there's no `Serialize` override — after a load the guards return, the compositions don't, and the restored `slotsFilled` blocks a rebuild.
- **Composition `Deserialize` doesn't set `m_Spawned`** — the next tick buys a second composition (already logged in `core/persistence` tasks debt).
- **Parked vehicles re-buy on every load** on top of vanilla-restored ones (`Serialize` returns null; "RISK ACCEPTED" in core/persistence tasks).
- **Composition costs are raw** (`comp.m_iCost` 9–15) though documented as difficulty-multiplied — a bunker is cheaper than one infantry group.
- **Priority band cap:** the scheduler loops 1..19 while the attribute documents 1..100 — priorities ≥ 20 silently never spend.
- **Iterator mutation** in `Specops` (`m_Groups.RemoveItem` inside `foreach`); unchecked `[0]` indexing on faction prefab arrays (Checkpoints, DefensePosition); unchecked `SpawnEntityPrefab` results billed as success (`BuyGuard` returns true unconditionally).
- **`OVT_BaseUpgrade.c:21`:** `OVT_Global.GetConfig() = config;` — assignment to a function return value; the `config` parameter is effectively discarded.
- **BUG-001 latent here:** `FillAmmoboxes` triggers the vanilla `OnItemAdded` NULL deref (62 VM exceptions per campaign start), worked around in `Modded/SCR_InventoryStorageManagerComponent.c` rather than at the source.

### Technical Debt
- `OVT_BaseUpgradeTownPatrol.c` — entire 177-line dead class (and `Controllers/README.md` still lists it as live); `OVT_SlottedBaseUpgrade.NearestSlot` dead (duplicated live on the controller).
- Random slot probing (`while i < 30`) can fail with free slots remaining — silent 0-spend.
- Timer leak: no `CallQueue.Remove` anywhere in the subsystem; upgrades tick forever on captured bases.
- Inconsistent RNG sources (`Math.RandomInt` vs `s_AIRandomGenerator`); `ref` on value types throughout; `CallLater(fn, freq, true, GetOwner())` passing an argument to zero-arg methods (project-wide idiom).
- Magic numbers: 60/40 checkpoint costs, ×4/×3/×6 multipliers, 600000 ms specops capture timer, 20 m arrival radius, 7/15/23 m composition setup radii, 15–40 ammo items, 25/50/75 threat thresholds.
- Dual prefab-source split: base upgrades on legacy `m_a*PrefabSlots` arrays, deployments on the new registries ("Kept for compatibility with BaseUpgrade systems").

---

## Future Enhancements

### High Priority
- [ ] Fix the resource-accounting cluster: dead allocation clamp, proxied-bank resets, overspend-before-clamp, `m_iNumGroups` drift.
- [ ] Make checkpoints survive load (track compositions or serialize properly; stop pre-filling slots on failed spawns).
- [ ] Set `m_Spawned` in composition `Deserialize` (stops double-buying).

### Medium Priority
- [ ] Delete `OVT_BaseUpgradeTownPatrol` + fix `Controllers/README.md`; delete `NearestSlot`.
- [ ] Null-check spawns before billing; guard prefab-array indexing.
- [ ] Fix the specops iterator mutation.
- [ ] Decide the deployments migration's fate: finish it, or declare base upgrades permanent and remove the "deprecated" framing.

### Low Priority / Nice to Have
- [ ] Multiply composition costs by difficulty as documented (rebalance).
- [ ] Extend the priority loop to 100 or fix the attribute doc.
- [ ] Deterministic slot search; unified RNG; remove timer leaks.

---

## Testing

### Current Coverage
None on any upgrade class. Adjacent: `OVT_TEST_InitSuite` asserts base registration; campaign/persistence suites explicitly exclude garrisons (test world has one base, garrison never populated in a 16 s window).

### Testing Gaps
- Logic-tier candidates (world-free): the spend/allocation arithmetic (`SpendToAllocation`, proxied-bank math, priority scheduling) is pure enough to test if refactored to take injected values — and it's exactly where the bugs cluster.
- The serializer replay path (`FindUpgrade` matching, group replay) is untested end-to-end.
- Physical spawning, slot discovery and proxying need play-testing (world geometry + player proximity).

---

## Documentation

### Current Documentation
- This retrospective plan; `Scripts/Game/Controllers/README.md` (stale); archived `docs/archive/ModularDeploymentSystem.md` (the replacement design that stalled).

### Documentation Needs
- The coexistence status with deployments is the key thing to keep current — it is easy to assume either system owns everything.

---

## Dependencies

### External Dependencies
- Vanilla: editable-entity slot labels, SmartAction sentinels, destructible tower buildings, `SCR_AIGroup`, compartments/turrets, inventory storage (ammo boxes).

### Internal Dependencies
- occupying/core (funding, scheduling, persistence envelope, known targets), occupying/qrf (suppression + specops trigger), faction data configs (`*_OverthrowData.conf` composition tags, legacy prefab slots), `OVT_VehicleManagerComponent` (parking), `OVT_OverthrowConfigComponent` (waypoints, spawn distance).

---

## Notes

**Discovered Information:**
- The dead TownPatrol class is the one completed migration to deployments — deleted from the prefab in the same commit that added the deployment-driven `OVT_PatrolHarassmentStabilityModifier` (`d62e441`), but the class itself was left behind.
- `SpawnInSlot`'s persistence-tracking comment is the best in-code record of the EPF→vanilla composition migration.
- AmmoCache and MGNest run at priority 1 alongside defense patrols — fortifications beat most garrison spending at low threat.

**Retrospective Assessment:**
- The proxying design is genuinely clever and cheap; its accounting has rotted in the two subclasses that override it.
- Resource math errors here compound with the core spend-loop bugs — together they make the OF's actual build-out behavior nearly unrelated to its authored tuning.
- The half-finished deployments migration is the biggest architectural ambiguity in the epic.

---

*This retrospective plan was created by analyzing existing code. Use `/start-feature occupying/base-upgrades` to begin making improvements.*
