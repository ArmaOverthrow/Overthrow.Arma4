# Jobs Core - Implementation Plan (Retrospective)

**Status:** Implemented (Documented Retrospectively)
**Originally Implemented:** Unknown (pre-dates Beast Mode; ported from Overthrow's early Reforger releases)
**Documented:** 2026-08-02
**Last Updated:** 2026-08-02 23:42

---

## Executive Summary

The job system is Overthrow's quest layer: config-driven missions offered to players per town or per base (assassinate a traitor, recon a base, raise support, plus five tutorial-ish onboarding jobs). A single manager (`OVT_JobManagerComponent`) polls every 10 s, offers jobs whose conditions pass, ticks accepted jobs through an ordered stage pipeline, and pays money/XP/item rewards on completion. Jobs are defined entirely as data: one `.conf` per job composing reusable condition and stage classes — adding a job requires no manager changes.

**Note:** This is a retrospective implementation plan created by analyzing the existing codebase (`/discover-feature`, 2026-08-02). The feature has already been implemented and shipped.

---

## Goals

### Primary Goals
- Give players directed objectives that teach and reinforce the core loop (find the dealer/shop, recruit, build, raise support, fight).
- Make job authoring pure data: new jobs = one `.conf` + prefab list entry; new mechanics = one small condition/stage class.
- Multiplayer-correct offer/accept/complete flow with JIP support and per-player or public distribution.

### Success Criteria
- [x] Config-driven job composition (conditions AND-ed, stages pipelined)
- [x] Two distribution models: public (one instance per town/base) and player-allocated (per player, auto-accepted, per-player cap)
- [x] Rewards: money, XP, inventory items
- [x] JIP sync + runtime broadcast RPCs; client accept/decline via comms seam
- [x] Vanilla-persistence round trip with documented drop rules (see `core/persistence`)
- [ ] Per-player tutorial caps actually per-player (BUG-037 — global cap makes them per-server)
- [ ] Reactive jobs UI (menu currently refreshes only on open/accept/decline)

---

## Current Architecture

### Composition model

```
OVT_JobConfig (.conf asset, configRoot)              Configs/Jobs/*.conf (8)
  ├─ m_aConditions : array<ref OVT_JobCondition>     // ALL must pass (AND)
  └─ m_aStages     : array<ref OVT_JobStageConfig>   // ordered pipeline
                       └─ m_Handler : ref OVT_JobStage
```

Both `OVT_JobCondition` and `OVT_JobStage` derive from `ScriptAndConfig`, so subclasses are pickable in the Workbench object picker and each instance carries its own `[Attribute()]` tuning. The manager knows nothing about concrete jobs — it walks `m_aJobConfigs` on the game-mode prefab, and **`jobIndex` is the array position** (load-bearing across saves; reordering the prefab list invalidates persisted jobs, which the serializer drops with a warning).

### Key Components

| Area | Files | Role |
|---|---|---|
| Manager | `Scripts/Game/GameMode/Managers/OVT_JobManagerComponent.c` | 943-line singleton: `OVT_Job` runtime class, 10 s `CheckUpdate()` tick (offer + progression + reward), accept/decline, occupancy sets, `RplSave`/`RplLoad` JIP, 3 broadcast RPCs, persistence apply path |
| Config classes | `Scripts/Game/Configuration/OVT_JobConfig.c` | `OVT_JobFlags`, `OVT_JobConfig`, `OVT_JobStageConfig` |
| Framework bases | `Scripts/Game/GameMode/Systems/Jobs/OVT_JobCondition.c`, `OVT_JobStage.c` | `ShouldStart(town, base, playerId)`; `OnStart`/`OnTick` (return false ⇒ advance) + `OnEnd` |
| Conditions (8) | `Scripts/Game/GameMode/Systems/Jobs/Conditions/` | Random (weighted by town pop/stability/support), TownSupport window, TownHasDealer, PlayerInRange / PlayerNotInRange, IsNearest / IsNearestTownWithDealer / IsNearestTownWithShop |
| Stages (11) | `Scripts/Game/GameMode/Systems/Jobs/Stages/` | Side-effect stages (FindRandomHouse, GetDealerLocation, GetShopLocation, SpawnCivilian, SpawnGroup — all `OnStart`, instant-advance) and waiting stages (WaitTillJobAccepted, WaitTillPlayerInRange, WaitTillSupport, WaitTillDead, HasRecruit, PlaceableItem — all `OnTick`-only) |
| UI | `Scripts/Game/UI/Context/OVT_JobsContext.c`, `Scripts/Game/UI/Components/OVT_JobListEntryHandler.c`, `UI/Layouts/Menu/JobsMenu.layout`, `JobsMenu/JobCard.layout` | Jobs menu: card list, accept/decline/show-on-map, detail pane |
| Comms seam | `Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c:901-953` | `RpcAsk_AcceptJob` / `RpcAsk_DeclineJob` (client → server) |
| Persistence | `Scripts/Game/Persistence/Serializers/Components/OVT_JobManagerSerializer.c` + `Configs/Systems/Persistence/Overthrow.conf` | Vanilla `ScriptedComponentSerializer`; see `core/persistence` docs |
| Wiring | `Prefabs/GameMode/OVT_OverthrowGameMode.et` (`m_aJobConfigs` order = `jobIndex`), `Prefabs/Characters/.../Character_Player.et` (context registration), `Configs/System/chimeraInputCommon.conf` (`OverthrowJobsAccept`/`Decline`/`ShowOnMap` + context), `OVT_Global.GetJobs()` | |
| Content | `Configs/Jobs/*.conf` (8) | The shipped job catalog (below) |
| Map | `Scripts/Game/UI/Map/OVT_MapIcons.c:773-787` | Renders `m_JobManager.m_vCurrentWaypoint` as a waypoint icon |
| Localization | `Language/localization_Overthrow.*.conf` | 24 `OVT-Job*` strings across en/fr/ko/ru/uk/zh |

### Job lifecycle (server)

1. **Offer** — `CheckUpdate()` every 10 s (`JOB_FREQUENCY`), scheduled from `PostGameStart()`. Per config index: skip if the global lifetime cap `m_iMaxTimes` is hit or `GLOBAL_UNIQUE` and already running; then
   - **base-only** (`m_bBaseOnly`): iterate occupying-faction bases, one instance per base (`m_aBaseJobs` occupancy);
   - **public town job** (`m_bPublic`): iterate towns, one instance per town (`m_aTownJobs`), owner `""`;
   - **player-allocated** (`!m_bPublic`): iterate towns × online players, owner = persistent ID, auto-accepted, capped by `m_mPlayerJobCounts[m_iMaxTimesPlayer]` — no town/base occupancy slot.
2. **Start** — `StartJob()` builds `OVT_Job` (location from town/base, `stage = 0`, `accepted = (owner != "")`), increments `m_aJobCounts`, then loops `OnStart` skipping every stage that returns false. Returns null (and unwinds occupancy) if all stages were skipped.
3. **Progression** — only `accepted` jobs tick. `OnTick(job)` returning false ⇒ `OnEnd()`, `stage++`, skip-forward `OnStart` loop, `RpcDo_UpdateJob` broadcast.
4. **Completion** — past the last stage: `AddPlayerMoney(m_iReward)`, `GiveXP(m_iRewardXP)`, hint, `TrySpawnPrefabToStorage` per `m_aRewardItems`, `RpcDo_NotifyJobCompleted`, occupancy cleanup, `RpcDo_RemoveJob`.
5. **Accept/decline** — `AcceptJob()` locally then server RPC (client) or `StreamJobUpdate()` (server). `DeclineJob()` removes non-public jobs outright; public jobs append the player's persistent ID to `job.declined` (never broadcast — see Known Issues).

### Replication

- **No `RplProp`** — the whole board is script-replicated (variable-length array; records must exist for players not controlling an entity, same rationale as skills).
- **JIP:** manager `RplSave`/`RplLoad` serialize the full `m_aJobs` array including `declined` lists; `RplLoad` clears the client list first.
- **Runtime:** three `RplRcver.Broadcast` RPCs — `RpcDo_UpdateJob` (upsert; match key = `jobIndex` + townId/baseId, **no owner** — BUG-038), `RpcDo_RemoveJob` (adds owner), `RpcDo_NotifyJobCompleted` (hint).
- `job.entity` (`RplId`) is streamed but meaningless outside the session.

### Persistence (post-EPF, vanilla SaveGame)

`OVT_JobManagerSerializer` (version 1) writes the job board plus lifetime counters; nested `map<string, map<int,int>>` is flattened into index-aligned primitive arrays (same trick as the deployment serializer). Restore calls the idempotent `ApplyPersistedJobs()`, which rebuilds the board and **derives** occupancy sets via `RegisterRestoredJobOccupancy()`. Three drop rules in `FindRestorableJobConfig()`: out-of-range `jobIndex`, out-of-range `stage`, or resting on `OVT_WaitTillDeadJobStage` (stale `RplId` would pay out instantly); dropped jobs free their slot and re-offer later.

**The load-bearing correctness argument** (recorded in `docs/features/core/persistence/context.md` — cite, don't restate): every stage a job can *rest* at overrides `OnTick` only, and every side-effecting stage instant-advances out of `OnStart`, so running nothing on restore is correct, not merely safe. `RunJobToCurrentStage()` (the old EPF re-entrant restore) survives as dead code with zero callers.

### UI flow

Main menu → `OVT_MainMenuContext.Jobs()` → `OVT_JobsContext.OnShow()` wires four `SCR_InputButtonComponent`s and calls `Refresh()`: iterates the client's local `m_aJobs`, filters out other players' jobs and locally-declined ones, instantiates a `JobCard.layout` per job, auto-selects the first. Accept/Decline show for unaccepted jobs, ShowOnMap for accepted; ShowOnMap writes `m_JobManager.m_vCurrentWaypoint` (also written by `OVT_RecruitsContext` — shared scratchpad) which `OVT_MapIcons` renders.

---

## Job Catalog

`jobIndex` = position in `OVT_OverthrowGameMode.et`. Unstated fields take defaults (`m_bPublic 1`, `m_iReward 100`, `m_iRewardXP 5`, flags 1).

| idx | Config | Scope | Conditions | Stages | Reward | Limits |
|---|---|---|---|---|---|---|
| 0 | `assassinateTraitor` | Public, town | Random (0.05 base; ×0.5 low pop, ×4 low stability, ×2 low support) + PlayerNotInRange | WaitTillAccepted → FindRandomHouse → SpawnCivilian (traitor) → SpawnGroup (OF defenders) → WaitTillDead | $600 + 30 XP | GLOBAL_UNIQUE; unlimited |
| 1 | `baseRecon` | Base-only, public | PlayerInRange (1500) | WaitTillPlayerInRange (100 m) | $230 + 15 XP | maxTimes 2 |
| 2 | `findGunDealer` | Player-allocated | IsNearestTownWithDealer | GetDealerLocation → WaitTillPlayerInRange (10 m) | $50 + 5 XP | maxTimes 1 ⚠ (BUG-037) |
| 3 | `raiseSupport` | Public, town | PlayerInRange (800) + TownSupport (max 0) | WaitTillSupport (≥10%) | $100 (silent default) + 15 XP | maxTimes 4 |
| 4 | `findShop` | Player-allocated | IsNearestTownWithShop | GetShopLocation → WaitTillPlayerInRange (10 m) | $50 + 5 XP + 2× FieldDressing | maxTimes 1 ⚠ |
| 5 | `placeEquipmentBox` | Player-allocated (tutorial) | IsNearest | PlaceableItem ("Ammobox", 25 m) | $0 + 5 XP | maxTimes 1 ⚠ |
| 6 | `recruitACivilian` | Player-allocated (tutorial) | IsNearest | HasRecruit | $0 + 10 XP | maxTimes 1 ⚠ |
| 7 | `placeACamp` | Player-allocated (tutorial) | IsNearest | PlaceableItem ("Camp", 25 m) | $0 + 5 XP | maxTimes 1 ⚠ (no maxTimesPlayer) |

⚠ = the global `m_iMaxTimes 1` cap makes these once-per-*server*, not once-per-player (BUG-037).

---

## Authoring Guide: Adding a Job

`docs/mission-statement.md` names "developers extending Overthrow with new … jobs" as a target audience — this is the recipe:

1. **Create the config:** duplicate a `.conf` in `Configs/Jobs/`, set `m_sTitle`/`m_sDescription` (localization keys), reward fields, `m_bPublic`/`m_bBaseOnly`, caps and flags.
2. **Compose conditions** (all must pass at offer time) and **stages** (ordered pipeline) from the existing catalog in the Workbench object picker; tune each instance's attributes.
3. **Append it to `m_aJobConfigs`** on `Prefabs/GameMode/OVT_OverthrowGameMode.et`. **Append only — never reorder or remove entries**: `jobIndex` is positional and persisted; reordering silently invalidates saved jobs (they get dropped with a warning on load).
4. **Add localization keys** (`OVT-Job_*`) to `Language/localization_Overthrow.en-us.conf` (and siblings).
5. **New mechanic?** Write a ~20-line subclass of `OVT_JobCondition` (override `ShouldStart`) or `OVT_JobStage`. Protocol: `OnStart` returning **false** = "skip me / already satisfied, advance"; `OnTick` returning **false** = "finished, advance". A stage that waits must override `OnTick` only and keep `OnStart` a no-op — **this invariant is what makes persistence restore correct** (see `core/persistence`). A stage holding live-session state (entity refs) needs a drop rule in `OVT_JobManagerComponent.FindRestorableJobConfig()` (see `OVT_WaitTillDeadJobStage`).
6. **Beware the `new` trap in tests:** `[Attribute()]` defvalues are applied by the config loader, not by `new` — construct conditions in logic tests with explicit field assignment (see `OVT_TEST_Logic_Jobs.c` header).

---

## Key Technical Decisions

### Decision 1: Base class + config-driven composition
**Context:** Jobs needed to be authorable without engine/manager changes; same pattern as `OVT_Modifier`, `OVT_SkillEffect`, `OVT_Deployment`.
**Implementation:** `ScriptAndConfig` subclasses in `[Attribute(UIWidgets.Object)]` arrays.
**Trade-offs:** Excellent extensibility; but `jobIndex` positional identity couples saves to prefab list order.

### Decision 2: Boolean protocol instead of an enum state machine
**Context:** Stages needed both "can't start / already satisfied" and "finished" signals.
**Implementation:** `OnStart`/`OnTick` return false ⇒ advance; skip-forward loops in `StartJob`/`CheckUpdate`.
**Trade-offs:** Minimal API and it is exactly what makes the no-replay persistence restore argument hold; but the overloaded false is easy to get wrong in new stages.

### Decision 3: Broadcast RPCs, not RplProp
**Context:** Variable-length board; records must exist for players without a controlled entity.
**Implementation:** Full-board JIP via `RplSave`/`RplLoad`; incremental `RpcDo_UpdateJob`/`RpcDo_RemoveJob`.
**Trade-offs:** Simple and JIP-safe, but every private job leaks to every client and match keys are composite (and currently inconsistent — BUG-038).

### Decision 4: Occupancy derived, counters stored
**Context:** Restore must not double-offer or lose lifetime caps.
**Implementation:** `m_aGlobalJobs`/`m_aTownJobs`/`m_aBaseJobs` rebuilt from the board; `m_aJobCounts`/`m_mPlayerJobCounts` persisted verbatim.
**Trade-offs:** Idempotent restore; the two categories must never be conflated.

### Decision 5: Single 10 s poll, no events
**Implementation:** One `CallLater` loop over configs × towns × players.
**Trade-offs:** Dead simple; O(configs × towns × players) every 10 s with no spatial/dirty gating, plus a sphere query per active PlaceableItem job.

---

## Current State

### What's Working
- The full offer → accept → tick → reward loop, both distribution models, base-only jobs
- JIP board sync including declined lists
- Save/restore through vanilla persistence with the documented drop rules (playtest item 11 covers the manual gate)
- Data-driven authoring; 8 shipped jobs; localization in 6 languages

### Known Issues (filed)
- **BUG-005** (open, pinned by a deliberate test): `OVT_TownHasDealerJobCondition` checks only the X axis of `gunDealerPosition`
- **BUG-037**: global `m_iMaxTimes` makes the five per-player onboarding jobs once-per-server — the second player on any server never gets the tutorial chain
- **BUG-038**: `RpcDo_UpdateJob` match key omits `owner` — two players' same-index jobs in one town collide client-side (remove *does* include owner, so update/remove disagree on identity)
- **BUG-039**: declining a public job never broadcasts — other clients keep stale entries until a JIP reload
- **BUG-040**: completion hints broadcast to all clients (everyone sees everyone's completions) and the server also calls `SCR_HintManagerComponent` directly — double hint on listen hosts, pointless on dedicated
- **BUG-041**: null-deref hazards — town-dereferencing conditions crash on base-only jobs; `GetShopLocation`/`FindRandomHouse` stages unguarded at stage time (validated only at offer time)

### Technical Debt (unfiled)
- `CheckUpdate` has no `Replication.IsServer()` guard (server-only by call-site convention)
- Dead code/fields: `RunJobToCurrentStage()`, `OVT_JobStageConfig.m_iTimeout` (stages cannot time out), per-stage `m_sTitle`/`m_sDescription` (never surfaced — multi-stage jobs give no per-stage guidance), `OVT_JobFlags.ACTIVE`, `OVT_Job.GetBase()`
- `m_vCurrentWaypoint`: single global UI scratchpad on a server manager, shared with `OVT_RecruitsContext`, no clear, world-origin means unset
- Jobs menu never refreshes reactively (four "Optional: Trigger UI update" comments mark the missing hooks); `m_SelectedJob` can dangle
- `job.declined` grows forever; a declined public job is gone for that player for the whole campaign
- Reward items silently vanish (log-only) when inventory is full; player-allocated jobs are auto-accepted so Accept/Decline UI never applies to them
- Jobs bypass `OVT_NotificationManagerComponent` (only Overthrow subsystem using raw hints)
- `raiseSupport` pays the silent $100 default (never declares `m_iReward`)

---

## Future Enhancements

### High Priority
- [ ] Fix BUG-037 (per-player caps) — the tutorial chain is the new-player experience
- [ ] Fix BUG-038/039 (update identity + decline broadcast) — MP board consistency
- [ ] Targeted RPCs or client-side owner filtering for private jobs and completion hints (BUG-040)

### Medium Priority
- [ ] Reactive jobs menu (hook the RPCs' "Optional: Trigger UI update" points)
- [ ] Surface per-stage title/description for multi-stage jobs (fields already exist)
- [ ] Route job notifications through `OVT_NotificationManagerComponent`
- [ ] Stage-time null guards (BUG-041) and a `Replication.IsServer()` guard on `CheckUpdate`

### Low Priority / Nice to Have
- [ ] Stable job identity (GUID or config resource name) instead of positional `jobIndex`
- [ ] Stage timeouts (implement or delete `m_iTimeout`)
- [ ] Reward overflow → spawn on ground; declined-list expiry; more job content
- [ ] Event-driven or spatially-gated offer loop

---

## Testing

### Current Coverage
- `OVT_TEST_Logic_Jobs.c` (Tier A, world-free, 4 cases, all with recorded can-fail proofs): TownSupport min/max/unset-sentinel window, dealer set/unset, **a deliberate test pinning BUG-005's X-axis-only check** (goes red when the bug is fixed), Random-condition deterministic edges (0/100/200 chance + independent low-X factors)
- `OVT_TEST_InitSuite.c:71`: `OVT_Global.GetJobs()` resolves
- `core/persistence` playtest checklist item 11: in-progress job survives continue at its stage with no payout; kill-target jobs re-offer

### Testing Gaps
- The manager itself: `CheckUpdate`, `StartJob`, occupancy sets, caps, reward payout
- All 11 stage classes; the five impure conditions (`IsNearest*`, `PlayerInRange`, `PlayerNotInRange`) — explicitly scoped out in `test-coverage/findings.md` ("no campaign-tier need yet")
- `ApplyPersistedJobs` round trip; accept/decline RPC seam; all UI

---

## Dependencies

### Internal Dependencies
- **Towns** (`OVT_TownManagerComponent`): town lookup, population/stability/support, random houses, dealer position
- **Occupying faction** (`OVT_OccupyingFactionManager`): bases for base-only jobs; `SpawnGroupJobStage` spawns OF groups
- **Economy**: shop registry reads; `AddPlayerMoney` on reward
- **Skills**: `GiveXP` on reward (jobs are one of the four XP sources named in the skills epic)
- **Players**: persistent-ID mapping on every RPC hop; `IsOffline()` (BUG-015 history)
- **Recruits**: `GetRecruitCount` in `HasRecruitJobStage`; `OVT_RecruitsContext` shares `m_vCurrentWaypoint`
- **Placeables**: `PlaceableItemJobStage` sphere-queries `OVT_PlaceableComponent`
- **Persistence**: `OVT_JobManagerSerializer` ↔ `ApplyPersistedJobs` (see `core/persistence`)

### External Dependencies
- None beyond base-game Reforger (`ScriptAndConfig`, `SCR_HintManagerComponent`, inventory storage manager)

---

## Notes

**Discovered Information:**
- No `TODO`/`FIXME` comments exist anywhere in the job files; all debt above is inferred from source
- The persistence no-replay argument is the system's most valuable invariant and is documented at the source and in `core/persistence` — new stages must preserve it
- Commit `f7bc36f` ("Fix new tutorial jobs spawning for every town") is the likely origin of the global-cap regression now filed as BUG-037

**Retrospective Assessment:**
- The condition/stage composition framework is genuinely good — small, orthogonal, Workbench-authorable, and it made the EPF→vanilla persistence migration tractable
- The weak layer is multiplayer distribution book-keeping: composite match keys, no decline broadcast, broadcast-everything RPCs — all fixable at the manager without touching the framework
- The UI is a thin read-only view; its problems (staleness, dangling selection) are manager-notification problems, not layout problems

---

*This retrospective plan was created by analyzing existing code. Use `/start-feature jobs/core` to begin making improvements.*
