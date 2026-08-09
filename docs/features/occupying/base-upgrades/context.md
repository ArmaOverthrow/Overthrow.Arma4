# Base Upgrades - Context & Decisions

**Last Updated:** 2026-08-02
**Current Phase:** Retrospective Documentation
**Status:** ✅ Documented (Existing Feature)

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
- `Scripts/Game/Controllers/OccupyingFaction/OVT_BaseControllerComponent.c` — registration (`m_aBaseUpgrades` attribute), slot discovery, priority scheduler (`SpendResources`), `FindUpgrade` persistence key
- `Prefabs/Controllers/OVT_BaseController.et` — the ONLY place upgrades are instantiated (9 entries with priorities/allocations)
- `Configs/Factions/*_OverthrowData.conf` — composition tags (SmallBunker/AmmoCache/MGNest) and costs
- `Scripts/Game/Persistence/Serializers/Components/OVT_OccupyingFactionManagerSerializer.c` — persistence envelope (upgrades ride inside the base records)

---

## Important Decisions

- **Config objects, not components:** upgrades are `ScriptAndConfig` subclasses in a prefab attribute array — server-only, no own replication, hand-managed timers.
- **Value-banked proxying:** out-of-range groups convert to `m_iProxedResources` + replay lists; `GetResources()` = live + banked, which is what allocation targets measure against.
- **Replay persistence with one exception:** upgrade state replays on load (AI never persisted — self-spawn disabled in `Overthrow.conf`); only slotted compositions are entity-tracked via `OVT_PersistenceTracking.Track` (the `SpawnInSlot` comment documents the EPF migration).
- **Priority scheduling:** manager hands the base resources; controller loops priority 1..19 with threat gates; `-1` allocation = spend recklessly.

---

## Gotchas & Learnings

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
