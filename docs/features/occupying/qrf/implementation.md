# QRF (Quick Reaction Force) - Implementation Plan (Retrospective)

**Status:** Implemented (Documented Retrospectively)
**Originally Implemented:** Unknown (inherited from early Overthrow Reforger development)
**Documented:** 2026-08-02
**Last Updated:** 2026-08-18

---

## Executive Summary

The QRF system is how the occupying faction *fights back*. When the resistance assaults a base (or a town becomes unstable enough), the faction manager spawns a temporary battle controller that runs a 120-second countdown, ships in waves of AI groups, and then scores the contested zone every 10 seconds until one side reaches `QRFPointsToWin`. The outcome — and only the outcome — mutates base/town ownership.

**Note:** This is a retrospective implementation plan created by analyzing the existing codebase. The feature has already been implemented.

---

## Goals

### Primary Goals
- Make territory contestable: capturing a base is a fought battle, not a button press.
- Give the AI faction a reactive combat response to player aggression and town instability.
- Surface the battle to all players (HUD countdown + progress sliders, map restricted areas, notifications).

### Success Criteria
- [x] Player-initiated base assaults trigger a defended battle
- [x] AI-initiated counter-attacks and town suppression/uprising battles occur
- [x] Battle state (timer, points, location) replicates to all clients including JIP
- [x] Outcome flips base/town control, adjusts threat, and applies town modifiers
- [x] QRFs debit the faction's resource pool (BUG-027, closed)
- [x] Resistance AI counts toward the score (recruits added 2026-08-18)

---

## Current Architecture

One class does all the work; the faction manager owns its lifecycle.

### Key Components
- `Scripts/Game/Controllers/OccupyingFaction/OVT_QRFControllerComponent.c` — the entire QRF: countdown, wave allocation/spawn queue, LZ/target selection, point scoring, win detection, `m_OnFinished` invoker. 507 lines, single class, server-only.
- `Prefabs/Controllers/OVT_QRFController.et` — empty `GenericEntity` carrying the component + `RplComponent`. No persistence component.
- `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c` — owner/orchestrator (sibling feature `occupying/core`): spawns the controller at the contested base/town origin, enforces the one-QRF-at-a-time singleton (`m_CurrentQRF`), mirrors state to clients, applies outcomes in `OnQRFFinishedBase`/`OnQRFFinishedTown`.
- `Scripts/Game/Controllers/OccupyingFaction/OVT_BaseControllerComponent.c:16-25` and `Scripts/Game/Controllers/OVT_TownController.c:19-28` — per-base/per-town QRF attack geometry attributes (`m_iAttackDistanceMin/Max`, preferred direction ± variance); the town controller draws Workbench direction arrows.
- `Scripts/Game/Configuration/OVT_DifficultySettings.c` — `QRFPointsToWin` (100), `maxQRF` (500/750/1200/2000 by difficulty), `QRFFastTravelMode` (FREE/NOQRF/DISABLED).
- UI consumers: `OVT_EconomyInfo.c` (HUD countdown + two progress sliders), `OVT_MapRestrictedAreas.c` (red circles at 750 m/220 m), `OVT_MapContext.c` (fast-travel gating).

### Data Flow
1. **Trigger** (server): one of four paths calls `StartBaseQRF`/`StartTownQRF` on the faction manager.
2. Manager spawns `OVT_QRFController.et` at the objective, copies the base/town attack-geometry attributes into it, calls `Start()`, subscribes the finish callback, sets `m_bQRFActive`/`m_vQRFLocation`, broadcasts RPCs.
3. **Countdown** (120 s): `CheckUpdateTimer` ticks 1 s; after a deliberate 15 s delay (so despawning garrisons clear first) it drains one spawn-queue entry per second. Timer is RPC'd to clients every second.
4. **Waves:** `SendTroops()` budgets `max(m_iResources, 400)` capped at `maxQRF × player-count multiplier` (×2 at >4 players … ×6 at >32), splits it across all friendly source bases, and queues random GROUP-catalog prefabs. Groups spawn *at the landing zone* (source bases only set the bucket count) with Scout → SearchAndDestroy waypoints scheduled at 5/15/30/60 s. Leftover budget schedules one more wave 4–8 minutes out.
5. **Scoring** (every 10 s once timer ≤ 0): counts occupying AI vs the resistance — human players **plus** resistance-faction AI (recruits) — within 220 m (`QRF_POINT_RANGE`), with the enemy "zone clear" test at 750 m (`QRF_RANGE`). Only characters that are alive and conscious count on either side (`IsFightingFit`). +5/tick if zone clear of enemies, ±1 otherwise, decay toward 0 when tied; players in the zone earn 2 XP per tick. First to ±`QRFPointsToWin` ends it.
6. **Resolution:** `m_OnFinished` → manager callback: ownership change (±250 threat, notifications, `RpcDo_SetBaseFaction`), town support/stability modifiers (`RecentBattlePositive`/`Negative`, `RecentBattle`), `ResetSupport` on OF town wins (prevents battle looping). Controller entity deleted; surviving QRF AI is deliberately left in the world (commit `e115965`).

### Integration Points
- **occupying/core** (sibling): owns prefab ref, `m_CurrentQRF` slot, resource pool (read, never debited), base list, outcome mutators, and the economy freeze — the manager's whole `CheckUpdate` suspends (resources, threat decay, specops, counter-attacks, town evaluation) while a QRF runs.
- **occupying/base-upgrades** (sibling): supplies attack geometry; all garrison upgrades gate their spawn predicate on `!m_CurrentQRF`, so any QRF despawns every base garrison map-wide (including the contested base's defenders — load-bearing for the 15 s spawn delay). `OVT_BaseUpgradeSpecops` is the one upgrade that *starts* a QRF.
- **occupying/deployments** (sibling): `OVT_DeploymentManager.EvaluateDeployments` early-returns during a QRF; deployment AI still counts toward the QRF's enemy tally (probably unintended). The intended battle-tracking seam (`HasRecentBattleNearby`) is a TODO stub returning false.
- **Town system:** town controllers provide geometry; resolution calls `ChangeTownControl`, support/stability modifiers, `ResetSupport`. Civilians despawn during QRFs. Villages (size 1) never QRF — they flip peacefully in `OVT_TownManagerComponent`.
- **UI/notifications:** HUD/map read `QRF_RANGE`/`QRF_POINT_RANGE` consts directly off the controller class; broadcast tags `BaseBattle`/`TownBattle`/`CityBattle` (+ Discord external notifications).

---

## Implementation Details

### Phase 1: Battle Controller (COMPLETED)
- Single-class controller with countdown, spawn queue, scoring loop, and finish invoker.
- One-QRF-at-a-time enforced by the manager's `m_CurrentQRF` singleton slot.

### Phase 2: Triggers (COMPLETED)
Four server-side triggers:
1. **Player base assault** — `OVT_CaptureBaseAction` → `OVT_PlayerCommsComponent.RpcAsk_StartBaseCapture` → `StartBaseQRF`. No cost/cooldown/distance validation server-side.
2. **AI counter-attack** — once per in-game minute: `m_iResources > 2000` && no counter-attack timeout && 10% roll → random base; aborts if the pick is already OF-held (making counter-attacks rarer than the roll suggests).
3. **Specops recapture** — `OVT_BaseUpgradeSpecops` targets a resistance-held base.
4. **Town battles** — split 2026-08-16:
   - **Uprising (player-initiated):** `OVT_StartUprisingAction` on the town controller flag → `OVT_UprisingRequestComponent` (on `OVT_OverthrowController`) → `StartTownQRF`. Server validates: occupied non-village town, support > `UPRISING_SUPPORT_THRESHOLD` (75), requester alive and inside the town range; the client sends a town id, never a coordinate. The old auto-trigger (support > 75% + player within 300 m) is removed — it surprised new players.
   - **Suppression (still automatic):** every quarter-hour, resistance town with support < 25% and a player within 300 m.

### Phase 3: Replication & UI (COMPLETED)
- Manager-side broadcast RPCs are the live channel: `RpcDo_SetQRFTimer` (1/s), `RpcDo_SetQRFPoints` (1/10 s), `SetQRFActive`/`Base`/`Town`/`Inactive`, `SetBaseFaction`.
- JIP payload in the manager's `RplSave`/`RplLoad` carries location/points/timer/active — a JIP client gets a working HUD (but not `m_iCurrentQRFBase/Town`, so its map circles are wrong).
- The controller's own `RplProp m_iPoints`/`m_iWinningFaction` are vestigial — replication stops the moment the battle starts (BumpMe unreachable) and nothing reads them client-side.

### Phase 4: Potential Improvements (NOT STARTED)
See Future Enhancements.

---

## Key Technical Decisions

### Decision 1: Ephemeral battle entity owned by the manager
**Context:** Battles are rare, exclusive, and temporary.
**Implementation:** The manager spawns a bare controller entity per battle and deletes it on resolution; all persistent consequences flow through manager callbacks.
**Trade-offs:** Clean separation of battle mechanics from faction state; but state is mirrored twice (controller RplProps vs manager RPCs) and the controller's copy is effectively dead.

### Decision 2: Global garrison despawn during battles
**Context:** Perf headroom for a large spawned battle.
**Implementation:** Every garrison/patrol/civilian spawn predicate includes `!m_CurrentQRF`; the QRF waits 15 s before spawning to let them clear.
**Trade-offs:** Affordable battles, but the contested base loses its own defenders, and the whole island visibly empties during any battle anywhere.

### Decision 3: Zone-control scoring by head count (recruits included 2026-08-18)
**Context:** Simple, readable win condition.
**Implementation:** Head-count comparison inside 220 m every 10 s; ±points to a threshold. Both sides count only characters that are alive and conscious. The resistance count is human players + resistance-faction AI agents, with player-controlled entities skipped in the agent loop so a player is never counted twice.
**Trade-offs:** Legible on the HUD, and an assault carried by recruits now scores (it previously lost by default). Recruits alone can win a battle with no human in the zone — deliberate, they are committed forces the player paid for. Any stray OF AI within 750 m still blocks the +5 "zone clear" rate. Head count is unweighted, so a recruit is worth exactly one player.

### Decision 4: Battles are not persisted
**Context:** (Implicit rather than designed.) No QRF state is serialized; the controller and its troops are never persistence-tracked.
**Implementation:** On load, the battle never existed — base faction rolls back to its save-time value and garrisons respawn.
**Trade-offs:** Coherent rollback with zero serializer complexity; but a save/reload escapes a losing battle, and an autosave mid-battle silently discards progress (nothing defers saves during a QRF).

---

## Current State

### What's Working
- All four triggers fire; battles run countdown → waves → scoring → resolution end-to-end.
- Ownership flips, threat adjustments, town modifiers and notifications apply on resolution.
- HUD countdown/progress and map restricted areas work for present-at-start clients; JIP gets the HUD.
- Difficulty scaling via `maxQRF` × online-player-count ladder.

### Known Issues
> **Fixed since this doc was written (all bugs closed):** free QRFs (BUG-027 — `SendTroops` now debits `m_iResources` with a zero clamp), the LZ trace no-op and the `Goodqrfpos`/`Goodqrfbasepos` file-scope globals (BUG-031), the unvalidated capture RPCs (BUG-025), the spend loop (BUG-026) and client config replication (BUG-013).

- **Leaked wave timer:** the leftover-resources `CallLater(SendWave, 4-8 min)` is never removed before the manager deletes the entity.
- **JIP map circles wrong:** `m_iCurrentQRFBase/Town` missing from the JIP payload, so JIP clients keep drawing the contested base's own restricted circle.
- **Counter-attack rarity bug:** random base picked *then* filtered, aborting the attempt when the pick is OF-held.
- **Preferred-direction wrap bug:** ranges spanning 0°/360° normalize into inverted min/max (`:397-408`).
- **`OVT_TownController.c:113`** reads server-only `m_CurrentQRF` on clients (should use `m_bQRFActive`).
- **1 Hz reliable broadcast RPC** for the timer, and a full `GetAIAgents` world-walk with per-agent string faction compare every 10 s.
- **Unguarded `Print`s** in `GetRandomDirection` inside a 450-attempt loop (~900 log lines per LZ resolution).

### Technical Debt
- Dead code: `WinBattle()`/`KillAll()` unreachable; duplicate `case "DefendBase":`; 8 of 10 waypoint-switch arms never invoked; `QRF_DEPTH` unused; unused `RplComponent` locals; unreachable "Village" battle branch (and no `VillageBattle` broadcast tag exists).
- Duplicated state: controller RplProps vs manager RPC mirror — two sources of truth, one consumed.
- Magic numbers throughout: 120 s countdown, 15 s spawn delay, 220/750 m ranges, ±250 threat (five separate literals), player-count ladder duplicated verbatim in the manager, 5/15/30/60 s waypoint schedule, 400 emergency floor, 4–8 min second wave.
- `OVT_Global.IsOceanAtPosition` uses hardcoded sea level 1 m (LZ selection depends on it).
- UI reads `QRF_RANGE`/`QRF_POINT_RANGE` consts directly off the controller class rather than via the manager/config.
- TODOs: sea/air reinforcement for last-base fallback (`:218`); deployments' `HasRecentBattleNearby` stub; specops "target discovery not by magic".

---

## Future Enhancements

### High Priority
- [x] Validate `RpcAsk_StartBaseCapture` server-side and guard `RpcAsk_InstantCaptureBase` (BUG-025).
- [x] Debit `m_iResources` for QRF spend so battles have a real economic cost (BUG-027).
- [x] Replicate `QRFPointsToWin`/`QRFFastTravelMode` to clients (BUG-013).
- [x] Fix the LZ trace no-op and the `Goodqrfpos` global-cache bug (BUG-031).
- [ ] Defer autosaves while a QRF is active — an autosave mid-battle still silently discards it.

### Medium Priority
- [x] Count resistance AI (recruits/deployments) in the score model. **(done 2026-08-18)**
- [ ] Add `m_iCurrentQRFBase/Town` to the JIP payload.
- [ ] Cancel the pending `SendWave` CallLater on resolution.
- [ ] Filter-then-pick for counter-attack target selection.
- [ ] Defer autosaves while a QRF is active (or persist battle state).

### Low Priority / Nice to Have
- [ ] Move magic numbers into `OVT_DifficultySettings`/config (countdown, ranges, threat deltas, wave schedule).
- [ ] Replace the 1 Hz timer RPC with a start-time + client-side countdown.
- [ ] Sea/air reinforcement for the last-base fallback (existing To-Do).
- [ ] Remove dead code (WinBattle/KillAll, duplicate switch arms, vestigial RplProps).

---

## Testing

### Current Coverage
None. `grep -rni "qrf|battle" Scripts/Game/Tests/` returns no matches. Adjacent coverage exercises the outcome mutators only (`ChangeTownControl` round-trips in `OVT_TEST_PersistenceSuite`, `ResetSupport` as a fixture in `OVT_TEST_Campaign_Economy`).

### Testing Gaps
- Logic-tier candidates (world-free): point-model arithmetic, wave budget allocation (player-count ladder, per-base split, 16× clamp), direction/wrap math.
- Init-tier: QRF prefab resolves, manager holds the prefab ref.
- Not automatable today: full battle lifecycle (needs AI + players), JIP mirroring (needs two clients), LZ tracing (needs world geometry) — manual play-testing.

---

## Documentation

### Current Documentation
- This retrospective plan; epic docs at `docs/features/occupying/`.
- `docs/technical-design.md` references QRF at a high level; BUG-013 documents the config-replication issue.

### Documentation Needs
- The four trigger paths and their thresholds would benefit from player-facing documentation (support 75/25, counter-attack conditions).

---

## Dependencies

### External Dependencies
- Vanilla AI (`SCR_AIGroup`, `AIWaypoint` spawning via `OVT_OverthrowConfigComponent.Spawn*Waypoint`), `SCR_FactionAffiliationComponent`.

### Internal Dependencies
- `occupying/core` (owner/orchestrator, resources, base list, outcome mutators)
- `occupying/base-upgrades` (attack geometry, garrison suppression, specops trigger)
- `occupying/deployments` (evaluation freeze; unintended AI counting)
- Town system (`OVT_TownManagerComponent`, `OVT_TownController`), difficulty config, notification manager, map/HUD UI.

---

## Notes

**Discovered Information:**
- The 15-second pre-spawn delay exists specifically so the globally-despawning garrisons/civilians clear before QRF troops arrive (comment at `OVT_QRFControllerComponent.c:65`).
- Surviving QRF AI is intentionally left in the world post-battle (commit `e115965` "Don't cleanup AI after a QRF").
- Source bases contribute only *allocation buckets* — troops spawn at the LZ, not at bases; the "Final Base Detected" fallback fakes a source when no friendly base remains.
- The economy freeze during battles is total: no resource ticks, threat decay, spending, specops, counter-attacks or town evaluation anywhere on the map.

**Retrospective Assessment:**
- The single-controller + manager-callback shape is clean and the notification/UI surface is complete.
- The scoring model's players-only limitation and the free-resource bug are the two biggest gameplay-correctness gaps; the unvalidated client RPCs are the biggest multiplayer-integrity gap.
- Persistence-by-omission (battles roll back on load) is coherent but exploitable.

---

*This retrospective plan was created by analyzing existing code. Use `/start-feature occupying/qrf` to begin making improvements.*
