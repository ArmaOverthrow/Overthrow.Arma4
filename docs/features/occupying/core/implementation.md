# Occupying Core (Faction Manager) - Implementation Plan (Retrospective)

**Status:** Implemented (Documented Retrospectively)
**Originally Implemented:** Unknown (inherited from early Overthrow Reforger development)
**Documented:** 2026-08-02
**Last Updated:** 2026-08-02

---

## Executive Summary

The command layer of the AI occupying faction. `OVT_OccupyingFactionManager` (a 1508-line singleton on the game mode) owns the two global war scalars — resources and threat — plus the registries of bases, radio towers and known targets, and makes every strategic decision: when to gain and spend resources, when to counter-attack, when to fight over towns, and how base/tower ownership changes. `OVT_BaseControllerComponent` instances (flagpole entities placed in the world) are its per-base hands: slot/parking discovery, the upgrade list, faction affiliation and flags.

**Note:** This is a retrospective implementation plan created by analyzing the existing codebase. The feature has already been implemented.

---

## Goals

### Primary Goals
- Give the occupying faction a persistent strategic state (resources, threat, territory) that reacts to resistance activity.
- Represent bases and radio towers as capturable world assets with faction affiliation, flags and notifications.
- Drive the sibling systems: fund base upgrades and deployments, orchestrate QRFs, feed the map threat overlay.

### Success Criteria
- [x] Bases/towers discovered from the world at load, stamped to the OF at campaign start
- [x] Resource economy: 6-hourly income scaled by threat and player count; priority-ordered spending across bases
- [x] Threat responds to kills, territory changes and battles; decays over time
- [x] State persists (resources, threat, base upgrades/slots/garrisons, tower factions) and JIP-replicates
- [ ] Per-base budget allocation (computed but dead — the top base drains the whole reserve, see Known Issues)
- [ ] A victory/defeat condition (none exists anywhere)

---

## Current Architecture

### Key Components
- `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c` — the manager + its record classes (`OVT_BaseData`, `OVT_RadioTowerData`, `OVT_BaseUpgradeData`, `OVT_TargetData`, enums). Static `s_Instance`, accessed via `OVT_Global.GetOccupyingFaction()`.
- `Scripts/Game/Controllers/OccupyingFaction/OVT_BaseControllerComponent.c` — per-base controller on `Prefabs/Controllers/OVT_BaseController.et` (FIA flagpole + faction affiliation + capture/manage actions); 9 instances placed in `Worlds/MP/OVT_Campaign_Eden_Layers/bases.layer` with per-base name/QRF-geometry overrides.
- `Scripts/Game/Controllers/OccupyingFaction/OVT_TowerControllerComponent.c` — 7-line empty marker class; its only job is making radio towers findable by entity query. All tower behavior lives in the manager.
- `Scripts/Game/Persistence/Serializers/Components/OVT_OccupyingFactionManagerSerializer.c` — vanilla-persistence serializer (registered in `Configs/Systems/Persistence/Overthrow.conf:36`).
- `Scripts/Game/UserActions/OVT_CaptureBaseAction.c` / `OVT_ManageBaseAction.c` — player interaction on the flagpole.
- `Scripts/Game/GameMode/Systems/Modifiers/{Stability,Support}/OVT_OccupyingFactionDeath*Modifier.c` — OF-death town modifiers driven by the manager's `m_OnAIKilled` event.
- Config: `OVT_DifficultySettings` (startingResources, resource ticks, baseResourceCost, ranges, counterAttackTimeout, baseThreat, threatReductionFactor), `OVT_OverthrowConfigComponent` (occupying faction key/index, `SetBaseAndTownOwners`).

### Data Flow
- **Two global scalars:** `m_iResources` (war chest) and `m_iThreat` (actually a `float` despite the prefix). Threat sources: +5 per OF AI killed, +size×150 on a town flip, ±250 on base/town battle outcomes; decays by `threatReductionFactor` (0.4%/15 min), floored at 0.
- **Income** (`GainResources`, at in-game 00/06/12/18:00): `baseResourcesPerTick + resourcesPerTick × clamp(threat/1000, …, 4)`, multiplied by a player-count ladder (×2 >4 … ×6 >32). Half of new income tops up the deployment manager's pool when it runs low (`AllocateDeploymentResourcesIfNeeded`).
- **Spending** (`CheckUpdate`, same tick): OF bases sorted by spatial threat score, each handed the reserve via `base.SpendResources(m_iResources, m_iThreat)` (see Known Issues — the per-base split is dead); bases with a player within `baseCloseRange+100` are skipped ("don't spawn stuff if a player is watching"). Then `UpdateSpecops` hands known targets to base specops while `m_iResources > maxQRF`.
- **Base data ↔ controller:** `OVT_BaseData.entId` links record→entity; the controller does a reverse nearest-position lookup for record access. `id` = insertion index, used by RPCs and UI.
- **Radio towers** (`CheckRadioTowers`, every 9 s): OF towers spawn `patrolGroupsMin..Max` defense patrols when a player is in military spawn range (despawned when out of range or during any QRF). Wiping the garrison *is* the capture mechanic — when the last group dies, the tower flips to the players. There is no OF re-capture path except `OVT_BaseUpgradeSpecops`' 600 s capture timer.
- **Known targets** (`UpdateKnownTargets`): the OF omnisciently registers every base, FOB and camp as `OVT_TargetData` (the "not by magic" To-Do); consumed by specops and the spatial threat score `GetThreatByLocation` (which also feeds the map threat overlay — a *separate* concept from the global threat scalar).

### Integration Points
- **occupying/base-upgrades** (sibling): controller owns `m_aBaseUpgrades`; `SpendResources` iterates priorities 1..19; `Serialize`/`Deserialize`/`FindUpgrade(type, tag)` is the persistence contract; `RecoverResources` is the refund hook.
- **occupying/qrf** (sibling): manager spawns/owns the QRF controller, copies attack geometry in, receives `m_OnFinished`, applies outcomes (`ChangeBaseControl`, town modifiers), mirrors state to clients; whole `CheckUpdate` freezes while `m_CurrentQRF` is set.
- **occupying/deployments** (sibling): `AllocateDeploymentResources(IfNeeded)` are the only two calls in; the deployment manager reads `m_Bases`/`m_RadioTowers`/`GetThreatLevel` back.
- **Towns:** subscribes `m_OnTownControlChange` (threat); town manager reads `m_RadioTowers`/`m_Bases` each tick for `NearbyRadioTower`/`NearbyBase` support modifiers.
- **Resistance:** `SpawnGarrison` on load for player-held bases; resistance reads `m_Bases`/`m_RadioTowers` for FOB placement rules and garrison management.
- **Consumed widely:** jobs (`GetBaseByIndex`, `GetNearestBase`), economy/real-estate UI gates, build/place restrictions, map icons/threat grid/restricted areas, spawn logic.

---

## Implementation Details

### Phase 1: Registries & Lifecycle (COMPLETED)
- World-query discovery of base controllers and tower markers at `Init` (two whole-world sphere queries); `OVT_BaseData`/`OVT_RadioTowerData` records.
- New campaign: `SetBaseAndTownOwners` + `NewGameStart` seeds threat (`baseThreat`), resources (`maxQRF`), stamps everything OF, seeds the deployment pool. Continue: serializer restores state and clears `m_bDistributeInitial` so the opening build-out isn't doubled; `InitBaseControllers` is the single replay point for both paths (replays upgrades, re-resolves slots, respawns resistance garrisons).

### Phase 2: Economy & Decisions (COMPLETED)
- Income/spend/specops loop, counter-attack roll (once per in-game hour: resources > 2000, timeout elapsed, 10% chance, random base), town battle checks (quarterly: support >75 on OF towns / <25 on resistance towns, player within 300 m). Villages flip peacefully via the town manager instead.

### Phase 3: Replication & Persistence (COMPLETED)
- No RplProps — hand-rolled JIP (`RplSave`/`RplLoad`: faction keys, base/tower locations+factions, live QRF snapshot) plus reliable broadcast RPCs for deltas (`RpcDo_SetBaseFaction`, `RpcDo_SetRadioTowerFaction`, six QRF-state RPCs). Client flags reconcile once via `SetClientBaseFactions` 1 s after init.
- Serializer: version, faction key, resources, threat, per-base upgrades/slots/garrison-prefabs (OF bases read the live controller; resistance bases store garrison prefab lists), tower factions. Not persisted: known targets, counter-attack timeout, live QRF state, live entity IDs.

### Phase 4: Potential Improvements (NOT STARTED)
See Future Enhancements.

---

## Key Technical Decisions

### Decision 1: Records + world-entity controllers, linked by position
**Context:** Bases exist as world-placed flagpole prefabs; the manager needs a saveable registry.
**Implementation:** Plain data records (`OVT_BaseData`) with `entId` resolved at discovery; nearest-position matching for persistence restore and controller→record lookup.
**Trade-offs:** Survives entity ID churn across sessions; but reverse lookups are O(n) positional matches, and a missing marker entity crashes `GetBase()` (the serializer works around it; the manager doesn't).

### Decision 2: Two unrelated "threat" concepts
**Context:** The faction needs both an escalation level and a spatial danger map.
**Implementation:** Global scalar `m_iThreat` (income multiplier, upgrade gates) plus `GetThreatByLocation()` (spatial score over targets/towns for base prioritization and the map overlay).
**Trade-offs:** Both are useful; the shared name and co-location make them easy to confuse — they never interact.

### Decision 3: Garrisons respawn from prefab lists, not persisted entities
**Context:** Vanilla persistence would double-spawn AI that the manager re-buys.
**Implementation:** AI self-spawn disabled in `Overthrow.conf`; the serializer stores prefab+position (or upgrade state) and `InitBaseControllers` re-spawns.
**Trade-offs:** Simple and idempotent (documented for `ReapplyLatestSaveData`); loses individual unit state, and anything not in the replay path (checkpoint compositions) silently fails to return.

### Decision 4: Hand-rolled positional JIP instead of RplProps
**Context:** Variable-length base/tower lists don't fit RplProp.
**Implementation:** Positional binary `RplSave`/`RplLoad` with no version field; `RplLoad` also writes the faction keys back into the shared config component.
**Trade-offs:** Compact and works; fragile to reordering, and a JIP stream mutating another component's config state is a surprising ownership violation.

---

## Current State

### What's Working
- Discovery, campaign start/continue, income/spend/threat loop, counter-attacks, town battles, base capture flow, tower garrisons and capture, notifications (incl. Discord), JIP, save/load of the core state.

### Known Issues
- **Per-base allocation is dead code** (`OVT_OccupyingFactionManager.c:1063`): `perBase` computed, never used — each base gets the *whole* reserve, so the highest-threat base drains everything. `toSpend` (the 80% cap) is likewise never decremented, so the cap is unimplemented.
- **Divide-by-zero** at the same line when the resistance holds every base (reached every 6-hour tick in the endgame).
- **The last base can never be counter-attacked**: `RandInt(0, Count()-1)` excludes the final index; and the pick-then-filter shape silently wastes most counter-attack rolls.
- **`patrolGroupsMax` off-by-one** (`:448`): towers always spawn `patrolGroupsMin` groups at default settings.
- **`UpdateSpecops` uses `break` where `continue` belongs** (`:1150`): one target with no nearby OF base abandons the whole target list. `OVT_TargetData.completed` is also never written, so targets are re-offered forever.
- **Unvalidated client RPCs** (`OVT_PlayerCommsComponent.c:105-150`): `RpcAsk_StartBaseCapture` checks nothing; `RpcAsk_InstantCaptureBase` is a shipped unauthenticated instant base flip (the DiagMenu gate is client-side only).
- **`GetBase()` crashes on a missing marker** (`:667-671`, no null check) — called unguarded from `DistributeInitialResources` and `CheckUpdate`.
- **1 s client-reconcile race** (`:152`): `SetClientBaseFactions` is a one-shot bet that JIP data + streamed entities arrived within 1 s; a lost bet leaves wrong flags forever.
- **Tower garrisons despawn map-wide during any QRF** (`:431`), and towers only ever flip player-ward through the garrison-wipe mechanic.
- **`m_OnAIKilled` declared `ScriptInvoker<IEntity>` but invoked with two args** (`:129` vs `:1368`); all subscribers take two.
- Latent: `VillageBattle` notification tag has no preset (currently unreachable); `RpcDo_SetBaseFaction` updates client data but not client presentation (relies on vanilla affiliation replication).

### Technical Debt
- Dead code: `WinBattle()`, `GetBasesWithinDistance()`, `OVT_BaseData.Get()`, `m_OnBaseControlChanged` (raised, zero subscribers), `OVT_OrderType.DEFEND/DESTROY`, `OVT_TargetType.WAREHOUSE` (scored, never created), `m_iOccupyingFactionIndex` (written, never read), several unused locals.
- Naming/typing: `float m_iThreat` (and `GetThreatLevel()` silently truncates to int — consumed downstream by deployments); `int m_bCounterAttackTimeout`; `array<ref EntityID>` etc. (`ref` on value types, project-wide pattern).
- Global cleanup of destroyed AI agents lives inside the OF tick's zero-players early-return (`:1017-1033`) — unrelated responsibility.
- Magic numbers throughout: ±250 threat literals ×5, +5/kill, size×150, 0.8 spend fraction, 2000/0.9 counter-attack gates, 75/25 support thresholds, 300 m player radius, 9 s tower timer (not time-scaled, unlike `CheckUpdate`), hardcoded fallback `timeMul = 6`, query radius 99999999, player-count ladder duplicated in the QRF controller.
- `m_aKnownTargets` not persisted — specops assignments reset every load.
- Side-effecting query filters (`FilterSlotEntities`, `FindParking`'s always-false filter used as collector).

---

## Future Enhancements

### High Priority
- [ ] Fix the spend loop: use `perBase`, decrement `toSpend`, guard `Count() == 0` (three bugs, one function).
- [ ] Validate/guard the two capture RPCs (shared with occupying/qrf — the entry points live here in spirit: they call straight into this manager).
- [ ] Null-guard `GetBase()` (mirror the serializer's workaround at the source).

### Medium Priority
- [ ] Counter-attack: filter-then-pick, include the last base.
- [ ] Fix `patrolGroupsMax` off-by-one; scale the tower timer with game time.
- [ ] `UpdateSpecops`: `continue` instead of `break`; write `completed` on targets.
- [ ] Replace the 1 s client-reconcile `CallLater` with a verified/retrying reconcile.
- [ ] Persist `m_aKnownTargets` (or explicitly accept the reset and delete `completed`).

### Low Priority / Nice to Have
- [ ] A victory/defeat condition (nothing ends the campaign today).
- [ ] Rename `m_iThreat`→`m_fThreat`, `m_bCounterAttackTimeout`→int naming; split spatial vs global threat naming.
- [ ] Move threat/counter-attack/support literals into `OVT_DifficultySettings`.
- [ ] Delete dead code; fix the `m_OnAIKilled` invoker signature.

---

## Testing

### Current Coverage
- `OVT_TEST_InitSuite`: `GetOccupyingFaction()` resolves; `m_Bases.Count() >= 1` and base 0 resolves a controller (Tier B, world-load registration).
- Incidental: town-control round trips touch `IsOccupyingFaction()`/faction indices.

### Testing Gaps
- The serializer round trip (upgrades, slots, garrison prefab lists, tower factions, resources, threat, `m_bDistributeInitial` suppression) — the biggest untested persistence surface in the mod.
- Logic-tier candidates (world-free): income math (threat factor + player ladder), threat sources/decay, `GetThreatByLocation` weights, counter-attack gate math.
- Not automatable: JIP symmetry (two clients), tower capture, live battle outcomes.

---

## Documentation

### Current Documentation
- This retrospective plan; `Scripts/Game/Controllers/README.md` (stale for TownPatrol); epic docs at `docs/features/occupying/`.

### Documentation Needs
- The two-threat-concepts distinction and the garrison respawn-not-restore model are the two things most likely to trip up future contributors; both now documented here.

---

## Dependencies

### External Dependencies
- Vanilla: `SCR_FactionAffiliationComponent`, editable-entity slot labels, destructible buildings (towers), AI groups/waypoints.

### Internal Dependencies
- core (epic): game-mode bootstrap, config component, difficulty settings, persistence system, time/weather multiplier.
- Town system, resistance faction manager, notification manager.
- Siblings: occupying/base-upgrades, occupying/qrf, occupying/deployments (all driven from here).

---

## Notes

**Discovered Information:**
- The player-proximity spend skip is deliberate and commented ("Dont spawn stuff if a player is watching lol").
- `RplLoad` writes faction keys back into the shared config component — clients trust the JIP stream for config identity.
- The serializer restores the occupying faction key *first* because every other faction index in the payload is relative to it.
- No victory/defeat condition exists; the closest thing is the QRF's "Final Base Detected" fallback with its sea/air-reinforcement To-Do.

**Retrospective Assessment:**
- The record/controller split and the replay-based persistence are sound and survived the EPF→vanilla migration well.
- The strategic layer's arithmetic has quietly rotted: the allocation bug means base build-out behaves nothing like the code's evident intent, and nobody could tell because there is no test on any of the math.
- The unvalidated comms RPCs are the mod's most serious multiplayer-integrity issue (shared with qrf).

---

*This retrospective plan was created by analyzing existing code. Use `/start-feature occupying/core` to begin making improvements.*
