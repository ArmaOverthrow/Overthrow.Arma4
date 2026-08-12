# Base Upgrades - Task Checklist

**Last Updated:** 2026-08-13
**Progress:** Complete (Existing Feature) + enhancements below

---

## Original Implementation (COMPLETED)

All original implementation tasks have been completed. This feature was documented retrospectively.

- [x] ✅ Upgrade framework (priority scheduler, allocation model, threat gates)
- [x] ✅ Eight live upgrade types (patrols, positions, tower guards, checkpoints, compositions ×3, parked vehicles, specops)
- [x] ✅ Proxying virtualization + QRF suppression
- [x] ✅ Replay-based persistence (slotted compositions entity-tracked)
- [x] ✅ Retrospective documentation created

---

## Enhancements

### Tower guards actually man their towers (2026-08-13)

- [x] ✅ Rewrote `OVT_BaseUpgradeTowerGuard` placement: snipers are teleported onto the tower platform once the frame-deferred member spawn delivers the agent (vanilla `OccupySA` pattern — AI cannot path up ladders, and `SCR_AIGroup` snaps spawned members to ground navmesh)
- [x] ✅ Fixed `GetActionOffset()` math (local space — must be rotated by the tower's world transform)
- [x] ✅ Root-caused "sees nothing, never fires": the cabin's window glass blinds AI perception traces AND the CoverPost smart action is a pose loop with no fire path — final design stands the guard on the open-air walkway (`WALKWAY_OFFSET` +1.5 m forward, play-test tuned) with NO waypoint (idle group keeps full threat/attack reactions)
- [x] ✅ Release CoverPost reservations before proxy-despawn deletes a guard (defensive — reservation only auto-releases on death/action-end)
- [x] ✅ Overrode Serialize/Deserialize: tower guards now persist as banked value re-bought via `BuyGuard` on approach, instead of replaying as waypointless ground patrols via `BuyPatrol`
- [x] ✅ Null-check stale tower IDs in `BuyGuard` (destroyed towers are replaced by ruin entities — old code hard-crashed)
- [x] ✅ Refreshed `Group_US_Sniper.et`/`Group_USSR_Sniper.et` from flat years-old snapshots to children of the vanilla `Group_*_Base.et` (same GUIDs; picks up `UnderFire`/`FoundCorpse` reactions and future engine changes)
- [x] 🎮 Play-tested 2026-08-13: guard stands on the walkway, sees the player (wanted eye), and fires
- [ ] 🎮 Still to verify in play: proxy despawn/respawn cycle and save/load of tower guards
- [ ] 💡 Optional: sniper accuracy is poor — tune the sharpshooter character's AI aiming skill if guards should be more lethal

### Curated sniper positions (2026-08-13)

- [x] ✅ `OVT_SniperPositionComponent` — marker component with per-position `m_iMinimumThreat`; `OVT_SniperPosition` marker entity (Workbench facing arrow) + `Prefabs/GameMode/OVT_SniperPosition.et` (GUID `6A8F1E2D4C5B0901`)
- [x] ✅ `OVT_BaseUpgradeSniperPosition` — finds markers in base range, spawns a 2-man spotter+sniper team, teleports both onto the marker (spotter 1.2 m beside), facing the marker's forward; no waypoint; banked-value persistence (all per the tower-guard learnings)
- [x] ✅ Renamed dead `OVT_Faction.m_aGroupSniper2Prefab` → `m_aGroupSniperTeamPrefab`; US uses vanilla `Group_US_SniperTeam` (Spotter+Sniper), USSR got new `OVT_Group_USSR_SniperTeam.et` (Scout+Sharpshooter, GUID `6A8F1E2D4C5B0902`) — no vanilla USSR team exists
- [x] ✅ Registered on `OVT_BaseController.et` (priority 2, allocation -1)
- [ ] 🎮 Play-test: place `OVT_SniperPosition` markers near a base in Workbench, verify team spawns facing the arrow, engages, proxies, and persists

## Future Enhancements

See `implementation.md` Future Enhancements. Headline items: fix the resource-accounting cluster (dead clamp, proxied-bank inflation, counter drift), make checkpoints survive load, set `m_Spawned` on composition deserialize, delete the dead TownPatrol class, decide the deployments migration's fate.

---

*This task file was created retrospectively. Add new tasks here when working on enhancements.*
