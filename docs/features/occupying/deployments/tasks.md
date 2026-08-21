# Deployments - Task Checklist

**Last Updated:** 2026-08-21
**Progress:** Complete (Existing Feature)

---

## Original Implementation (COMPLETED)

All original implementation tasks have been completed. This feature was documented retrospectively.

- [x] ✅ Module framework (condition/spawning/behavior, prototype-clone, registry)
- [x] ✅ Evaluate→score→create loop with cost/chance/priority/max-instance filters
- [x] ✅ Proximity virtualization + vanilla persistence (self-spawn, eliminated-flag ordering)
- [x] ✅ Three shipped configs (Town Patrol + two vehicle patrols); town-patrol migration from BaseUpgrades
- [x] ✅ Retrospective documentation created

---

## Enhancements (post-retrospective)

### 2026-08-21 — Town patrols sweep houses instead of standing on road corners
- [x] ✅ `OVT_EVirtualWaypointType.SEARCH` appended (core enum + `ValidateWaypointPlan` bound + `CreatePlannedWaypoint` → Search & Destroy prefab, radius `SEARCH_WAYPOINT_RADIUS_M` 15, hold from the plan; movement treats it as WAIT)
- [x] ✅ `OVT_PatrolType.TOWN_SWEEP` + `OVT_PatrolBehaviorDeploymentModule.BuildTownSweepPlan` — per-group roll: house route (`m_iSweepHouseCount` houses, nearest-neighbour from the group, 60–120 s hold each) or loose un-snapped ring (0.3–1.0 × the controller's authored `m_iTownRange`, sea corners road-snapped)
- [x] ✅ `OVT_VirtualPlanFactory.BuildSearchPlan` + `OrderNearestNeighbour` (pure, Logic-tier pinned)
- [x] ✅ `Deployment_TownPatrol.conf` → `TOWN_SWEEP`
- [x] ✅ Tests: Logic `SearchPlan`, `NearestNeighbourRoute` (new, green); Init `TownPatrolPlanCycles` updated (green); `compile-check.sh` OK
- [x] ✅ Relaxed search: `OVT_HouseSearchAI.c` (behaviour/activity/tree node), `AI/BehaviorTrees/Overthrow/{Waypoints/WP_HouseSearch,Soldier/HouseSearch}.bt` (stand/walk/weapon down), `OVT_AIWaypoint_HouseSearch.et`, `m_pHouseSearchWaypointPrefab` on the game mode; Init `…_HouseSearchWaypointResolves` green (both trees load by GUID)
- [ ] ⏸️ Open both `.bt` files in Workbench's BT editor once and resave (hand-authored text copies)
- [ ] ⏸️ Play-test: do live patrols actually enter house interiors (navmesh-dependent), does the S&D posture read as a search, does the route feel like a patrol; tune `SEARCH_WAYPOINT_RADIUS_M`, hold band, `m_iSweepHouseCount`, `m_fSweepHouseChance`
- [ ] ⏸️ Civilian reaction to a searching patrol (flee/cower) — deferred by author 2026-08-21, belongs to `virtualization/civilians` archetypes
- [ ] ⏸️ Run the All suite before commit (single-case runs only so far; Workbench was open concurrently)

## Future Enhancements

See `implementation.md` Future Enhancements. Headline items: fix the `m_mFactionDeployments` leak, set `m_iResourcesInvested`/`m_fThreatLevel` at creation, fix the world-time unit bugs, wire runtime condition evaluation, decide the BaseUpgrades migration's fate.

---

*This task file was created retrospectively. Add new tasks here when working on enhancements.*
