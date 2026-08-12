# Base Upgrades - Context & Decisions

**Last Updated:** 2026-08-13
**Current Phase:** Enhancements
**Status:** ✅ Documented (Existing Feature) + enhancements landing

---

## Quick Status

**What's Done:**
- ✅ Feature fully implemented (existing code)
- ✅ Retrospective documentation created (thorough code investigation, 2026-08-02)

**What's Next:**
- 📋 Review for potential improvements — the resource-accounting bug cluster and checkpoint persistence are the highest-value fixes; the stalled deployments migration needs a decision

**Blockers:**
- None

---

## Key Files

- `Scripts/Game/Controllers/OccupyingFaction/BaseUpgrades/` — 11 classes (`OVT_BaseUpgrade` base, `OVT_BasePatrolUpgrade` proxying parent, `OVT_SlottedBaseUpgrade`, 8 concrete; `OVT_BaseUpgradeTownPatrol` is dead)
- `Scripts/Game/Controllers/OccupyingFaction/OVT_BaseControllerComponent.c` — `m_BaseUpgradesConfig` config reference (runtime `m_aBaseUpgrades` populated in `InitializeBase`), slot discovery, priority scheduler (`SpendResources`), `FindUpgrade` persistence key
- `Configs/BaseUpgrades/overthrowBaseUpgrades.conf` — the upgrade list (10 entries with priorities/allocations); `OVT_BaseUpgradesConfig` class in `Scripts/Game/Configuration/`
- `Prefabs/Controllers/OVT_BaseController.et` — points `m_BaseUpgradesConfig` at the conf (a base instance can still delta-override it per-base)
- `Configs/Factions/*_OverthrowData.conf` — composition tags (SmallBunker/AmmoCache/MGNest) and costs
- `Scripts/Game/Persistence/Serializers/Components/OVT_OccupyingFactionManagerSerializer.c` — persistence envelope (upgrades ride inside the base records)

---

## Important Decisions

- **Config objects, not components:** upgrades are `ScriptAndConfig` subclasses — server-only, no own replication, hand-managed timers.
- **Upgrade list lives in a config, not the prefab (2026-08-13):** `OVT_BaseControllerComponent.m_BaseUpgradesConfig` references `Configs/BaseUpgrades/overthrowBaseUpgrades.conf` (deployment-registry pattern). The old inline `m_aBaseUpgrades` prefab attribute is gone; the member survives as a runtime array filled from the config in `InitializeBase`, so the serializer/`FindUpgrade` contracts are untouched. Per-base customization = delta the config object on a base instance; modders can override the conf or point bases at their own.
- **Value-banked proxying:** out-of-range groups convert to `m_iProxedResources` + replay lists; `GetResources()` = live + banked, which is what allocation targets measure against.
- **Replay persistence with one exception:** upgrade state replays on load (AI never persisted — self-spawn disabled in `Overthrow.conf`); only slotted compositions are entity-tracked via `OVT_PersistenceTracking.Track` (the `SpawnInSlot` comment documents the EPF migration).
- **Priority scheduling:** manager hands the base resources; controller loops priority 1..19 with threat gates; `-1` allocation = spend recklessly.

---

## Gotchas & Learnings

- **AI cannot climb ladders — vanilla teleports.** The engine ships `NavmeshLadderLink` (GuardTower_01 carries one) but there is zero script-side AI ladder support (no behavior, no BT node). Vanilla's own elevated-post mechanism (`SCR_AIAllocateActionsForDefendActivity.OccupySA`, used by FastInit defend waypoints in Combat Ops) teleports the AI onto the post, reserves it, then issues PerformAction. Tower guards now copy the teleport (2026-08-13).
- **GuardTower_01's authored CoverPost is unusable: the cabin glass blinds AI.** Perception LOS traces (and the wanted system's eye) fail through the intact window glass, so a guard at the authored post never acquires a target — and the smart action itself has no fire path anyway. The working design stands the guard on the open-air walkway (+1.5 m forward of the post, tower-local) with **no waypoint**: an idle group keeps full threat/attack reactions, which is the configuration that actually engages (verified in play 2026-08-13).
- **`GetActionOffset()` is LOCAL space** — rotate by the owner's world transform (`mat[3] + offset.Multiply3(mat)`, per `SCR_AIGetSmartActionSentinelParams`). Two vanilla call sites get this wrong; don't copy `OccupySA`'s unrotated math.
- **`SCR_AIGroup` member spawning is frame-deferred AND navmesh-snapped** — you can never position a member synchronously after spawning a group; hook `GetOnAgentAdded()`.
- **Smart-action reservations leak on entity deletion** — `ReserveAction` only auto-releases on action end or user death; deleting a proxied guard without `ReleaseAction()` blocks the post forever.
- **`SA_CoverPost.bt` degrades gracefully** (move wrapped in ForceNodeResult Success — the AI poses wherever it stands, which is why guards used to "work" at ground level); `SA_ObservationPost.bt` hard-fails instead.

- **The allocation clamp is dead** (`OVT_BaseControllerComponent.c:294-301`) — upgrades can overspend the pool; combined with core's dead per-base split, OF spending behaves nothing like its tuning.
- **`DefensePosition`/`TowerGuard` never clear `m_iProxedResources`** after re-spending — unbounded inflation, permanently negative allocation math.
- **Checkpoints are the persistence blind spot:** compositions untracked + no `Serialize` override + slots restored as filled → after load, guards return, compositions don't, slots blocked forever.
- **Composition `Deserialize` doesn't set `m_Spawned`** → second composition bought next tick.
- **Priorities ≥ 20 never execute** (loop bound 1..19 vs documented 1..100).
- **`OVT_BaseUpgradeTownPatrol` is dead code** — migrated to the "Town Patrol" deployment in commit `d62e441`; the class and its README entry were left behind.
- **BUG-001 originates here** (`FillAmmoboxes` → vanilla `OnItemAdded` NULL deref), patched via the modded inventory component, not at the source.
- Base upgrades use the *legacy* faction prefab-slot arrays; deployments use the new registries — two parallel prefab-resolution systems.

---

*This context file was created retrospectively by analyzing existing code.*
