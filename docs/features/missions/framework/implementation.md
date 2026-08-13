# Missions Framework — Implementation Plan

**Epic:** missions (feature #1 of 7 — foundational)
**Status:** Planning
**Started:** 2026-08-13
**Target Completion:** TBD
**Last Updated:** 2026-08-13

---

## 1. Executive Summary

This feature builds the mission runtime that every other feature in the `missions` epic consumes: a
declarative, Workbench-authorable mission format; a server-authoritative instance state machine that
**branches**; co-op participation tracked by **persistent ID**; rewards grantable at any point with
five distribution policies; world-placed mission items; replication (JIP + incremental); and its own
persistence serializer.

It is a ground-up replacement for the legacy Jobs backend, not a refactor of it. **Only the idea
survives** — config-as-data, composed from small pickable pieces. The architecture is deliberately
different in five specific ways, each of which fixes a named defect of the jobs system:

| Jobs | Missions | Why |
|---|---|---|
| Ordered stage list, `stage++` | **Named-node graph** with typed transitions | Branching is the headline requirement; a linear index cannot express it |
| `jobIndex` = array position (+ a v1→v2 migration to escape it) | Stable `m_sId` from day one, instance ids `configId#seq` | Positional identity is what forced the migration |
| Composite RPC match key (`index`+`townId`+`baseId`, and *remove* adds owner — BUG-038) | **One key: the instance id.** One upsert, one remove | Update and remove cannot disagree if there is only one key |
| One owner, one payout | Participant **set** by persistent id; reward policies | Co-op is a first-class requirement |
| Poll `configs × towns × players` every 10 s, no server guard | Server-guarded, **staggered round-robin** over `configs × anchors` | The player factor is gone (no player-allocated missions survive) and the sweep is bounded per tick |

The hardest constraint shapes the whole design: **restore is state assignment, never module replay.**
Jobs got this right by accident (every side-effecting stage instant-advances, so a job never rests on
one). Branching makes it easy to get wrong, so missions make it **explicit**: every module declares an
`OVT_MissionRestorePolicy`, `OnEnter` is *never* called on load, and no entity handle is ever persisted
or replicated — modules find world objects through a **tag registry** keyed by `(instanceId, key)` that
tagged entities re-populate themselves on spawn *and* on vanilla restore.

The jobs system is untouched and keeps running alongside this one. Teardown is `mvp-missions`.

The proof this feature ships is a **minimal end-to-end test mission** (`m_bOfferable 0`, never offered
to players) driven entirely by test suites — offer → two players join → signal → node advance → split
reward → complete → removed from board → survives a persistence round trip. No UI is involved;
`mission-ui` comes next.

---

## 2. Goals

### Primary Goals

- A mission format authorable **entirely in `.conf`**, with branching expressed as named nodes and typed
  transitions.
- A server-authoritative instance state machine with exactly one active node per instance in v1, and a
  **save/wire format that already stores the active node as a list** so a future parallel/composite node
  needs no format break.
- Participation by **explicit join**, tracked by persistent ID, any number of participants, join
  mid-mission.
- Rewards (money / XP / items) grantable from **any node** as well as on completion, with distribution
  policies: split evenly, closest, triggerer, finder, specific.
- **Mission items**: place a tagged item in the world, players pick it up and deliver it to a location.
- Config-driven offer conditions with at least the expressive power the 7 legacy jobs needed.
- Replication: JIP full board + one incremental upsert/remove pair keyed by instance id; clients hold a
  read-only mirror the UI and map will read.
- Persistence: own `ScriptedComponentSerializer`, version-first, positional, frozen record class names,
  idempotent apply, **no replay**.
- A parity module library able to express all 7 legacy jobs.

### Secondary Goals

- Lifecycle `ScriptInvoker`s (started / node changed / completed / failed / reward granted) as the Intel
  epic's (GH #11) hook seam.
- A persistent-ID XP helper alongside the existing money one, so offline participants can be paid.
- Per-instance current location exposed in replicated state so `mission-ui` can render waypoints without
  new traffic.
- Cheaper, server-guarded offer scheduling than the jobs poll.

### Explicitly Out of Scope

- **All UI** — list, detail, accept buttons, map waypoint rendering: `mission-ui`.
- **Shipped mission content** beyond the one non-offerable test mission: `mvp-missions`.
- **Jobs removal** — jobs keep running; no job class renamed, no `Configs/Jobs/*.conf` edited, no job
  serializer or prefab entry touched.
- **Escrow, officer authoring, group assignment** — `resistance-missions`. A seam is left (see D12); no
  escrow code ships.
- **Intel implementation** — invokers and the module base class are the seam; no intel module ships.
- **Recruit-granting and dialog modules** — later features.
- **Player-allocated (private) missions.** The five player-allocated jobs were retired on 2026-08-09;
  all 7 surviving jobs are public or base-scoped. Dropping the per-player offer dimension is the single
  biggest cost reduction in the scheduler and nothing in the epic asks for it back.
- **Help/wiki sync.** This feature ships nothing a player can see or do (the test mission is not
  offerable and there is no UI), so there is deliberately **no `help-docs-sync` phase here**.
  `mission-ui` and `mvp-missions` each own one — flag it in their plans.

---

## 3. Architecture Overview

### 3.1 Layer map

```
CONFIG (authored .conf, Workbench-pickable, no code)
  OVT_MissionRegistryConfig            Configs/Missions/overthrowMissions.conf   [configRoot]
    └─ ref array<ref OVT_MissionConfig>        one entry per mission, each inheriting a .conf asset
         OVT_MissionConfig                Configs/Missions/<name>.conf           [configRoot]
           m_sId, m_sTitle, m_sDescription, m_eScope, m_bOfferable,
           m_iMaxActive, m_iMaxTimes, m_iOfferExpirySeconds,
           m_bAllowLateJoin, m_bFailOnLastAbandon,
           m_sEntryNode
           ├─ ref array<ref OVT_MissionCondition>  m_aOfferConditions   (ALL must pass)
           ├─ ref array<ref OVT_MissionReward>     m_aCompletionRewards
           └─ ref array<ref OVT_MissionNode>       m_aNodes             (FLAT, named)
                OVT_MissionNode
                  m_sName, m_sObjective (loc key), m_bContributesLocation
                  ├─ ref OVT_MissionModule            m_Module      (exactly one)
                  └─ ref array<ref OVT_MissionTransition> m_aTransitions
                       OVT_MissionTransition { m_sOutcome, m_sTargetNode }

RUNTIME (server truth; clients hold a mirror of the same record type)
  OVT_MissionManagerComponent  : OVT_Component     on OVT_OverthrowGameMode
    m_MissionsConfig            ref OVT_MissionRegistryConfig   (prefab attribute → registry conf)
    m_aMissions                 ref array<ref OVT_MissionInstance>       the board
    m_mMissionsById             ref map<string, OVT_MissionInstance>     lookup
    m_mLifetimeCounts           ref map<string,int>   configId → times started   (persisted)
    m_iNextInstanceSeq          int                                              (persisted)
    m_mEntityRegistry           ref map<string, ref OVT_MissionEntitySet>  instanceId → tagged entities (RUNTIME ONLY)
    invokers: m_OnMissionStarted / NodeChanged / Completed / Failed / RewardGranted / ParticipantsChanged

  OVT_MissionInstance   (plain Managed record — runtime state, wire payload and save payload shape)
    m_sInstanceId       "assassinate-traitor#7"  — stable, unique, never an index
    m_sConfigId         stable config id
    m_aActiveNodes      ref array<string>   EXACTLY ONE in v1 (plural by design — see D2)
    m_iState            OFFERED / ACTIVE                     (terminal states are transient, then removed)
    m_iTownId, m_iBaseId    anchor context, -1 when N/A
    m_vLocation, m_bHasLocation    current node's waypoint contribution
    m_aParticipants     ref array<string>   PERSISTENT ids, insertion-ordered (order is the split order)
    m_aVarKeys / m_aVarValues   ref array<string> ×2   parallel string variable bag
    m_iNodeEnteredAtMs  world time on entry (timers, and "how long has this been stuck")
    m_iOfferedAtMs      world time the instance was created (offer expiry)

COMMS (client → server)
  OVT_MissionRequestComponent : OVT_Component      on OVT_OverthrowController
    RequestMissionAction(int verb, string instanceId)      ONE RPC pair, two verbs (JOIN / LEAVE)
    RpcAsk_MissionAction  [Server]      identity resolved from controller ownership, never the payload
    RpcDo_MissionActionResult [Owner]   display-only

WORLD TAGGING (how modules reference world objects without handles)
  OVT_MissionEntityComponent  { m_sInstanceId, m_sKey }   self-registers with the manager on post-init
  OVT_MissionItemComponent    { m_sInstanceId, m_sKey }   + its own tiny serializer (survives save)

PERSISTENCE
  OVT_MissionManagerSerializer : ScriptedComponentSerializer
    record classes (NAMES FROZEN): OVT_PersistedMissionV1, OVT_PersistedMissionCounterV1
    bound in the ComponentSerializers block of Configs/Systems/Persistence/Overthrow.conf

PURE (world-free, Logic-tier testable, Scripts/Game/Data/Missions/)
  OVT_MissionGraph            transition resolution + whole-graph validation
  OVT_MissionRewardDistribution  recipient resolution + conserving split maths
  OVT_MissionWireCodec        list ⇄ delimited string for RPC fields
  OVT_MissionIds              instance id compose/parse, id shape rules
```

### 3.2 The module protocol

```cpp
class OVT_MissionModule : ScriptAndConfig
{
    //! Entered. Return an outcome key to leave immediately, or "" to stay resident.
    string OnEnter(OVT_MissionInstance mission);
    //! Polled while resident. Return "" to keep waiting, or an outcome key to leave.
    string OnTick(OVT_MissionInstance mission);
    //! Leaving, for any reason including mission failure. Cleanup only — never gameplay.
    void OnExit(OVT_MissionInstance mission, string outcome);
    //! Restore contract. See §5 D6.
    OVT_MissionRestorePolicy GetRestorePolicy();
    //! Re-acquire whatever this module needs after a load. Only called for REDERIVE. Idempotent.
    //! Return false to fail the instance.
    bool OnRestore(OVT_MissionInstance mission);
    //! Optional waypoint contribution for this node.
    bool GetLocation(OVT_MissionInstance mission, out vector location);
}
```

Outcome keys are plain strings matched against the node's transitions. Two conventions, enforced by
validation: `""` means "still resident" and may never be a transition outcome; target names beginning
`@` are **reserved terminals** — `@complete` and `@fail` — and need no node.

`OVT_MissionRestorePolicy`: `RESIDENT_SAFE` (assign state, run nothing), `REDERIVE` (assign state, call
`OnRestore`), `NOT_RESTORABLE` (drop the instance on load, free its slot, keep lifetime counters, log
one line naming mission id + node).

### 3.3 Data flow

**Offer (server only).** `EvaluateOffers` ticks every 5 s under `Replication.IsServer()`. Each tick
takes the next config off a round-robin cursor and the next *slice* of anchors (bounded, `OFFER_SLICE`
= 8 towns/bases) off a second cursor. Cheap gates first — `m_bOfferable`, lifetime cap, concurrent cap,
already-active-here — then `ShouldOffer(ctx)` on each condition (AND). On success: build the instance,
enter the entry node, insert on the board, broadcast upsert, invoke `m_OnMissionStarted`, notify.

**Join.** Client → `OVT_MissionRequestComponent.RequestMissionAction(JOIN, instanceId)` → server
resolves the player from the controller entity, maps to persistent id, validates (mission exists, not
terminal, not already a participant, late-join allowed if ACTIVE), appends to `m_aParticipants`, flips
OFFERED→ACTIVE on the first join, broadcasts upsert, replies to the owner with a result code.

**Progress.** `TickMissions` (server, 1 s) walks ACTIVE instances, calls `OnTick` on the active node's
module. A non-empty outcome → `OVT_MissionGraph.ResolveTarget(node, outcome)`:
- a node name → `OnExit(old)`, set `m_aActiveNodes`, `m_iNodeEnteredAtMs`, recompute location,
  `OnEnter(new)` — which may itself return an outcome, so this is a **bounded loop** (max
  `MAX_TRANSITIONS_PER_TICK` = 32, then fail the instance and log; that is a config cycle, not a
  gameplay state);
- `@complete` → grant `m_aCompletionRewards`, invoke, notify, remove from board (broadcast remove);
- `@fail` → invoke, notify, remove from board;
- no matching transition → **fail the instance**, log the unmatched outcome. Silence here is how a
  branching system loses missions.

**Rewards.** `OVT_MissionRewardDistribution.ResolveRecipients(policy, participants, ctx)` is pure and
returns persistent ids in a deterministic order. The manager then pays: money via
`OVT_EconomyManagerComponent.AddPlayerMoneyPersistentId`, XP via the **new**
`OVT_SkillManagerComponent.GiveXPPersistentId`, items via `TrySpawnPrefabToStorage` for online
recipients only (an offline recipient's items are skipped and the fact is logged and notified — not
silently dropped, which is the current jobs behaviour).

**Restore.** See §5 D6.

### 3.4 Module library — parity mapping

Legacy inventory: 12 conditions + 15 stages, of which **7 are orphaned** (referenced by no shipped
`.conf`) and are explicitly **not ported**.

**Conditions** (8 new, all offer-time, all take an `OVT_MissionOfferContext`):

| New | Replaces | Notes |
|---|---|---|
| `OVT_MissionCondition_Random` | `OVT_RandomJobCondition` | port the pop/stability/support weighting maths verbatim; Logic-tier |
| `OVT_MissionCondition_TownSupport` | `OVT_TownSupportJobCondition` | min/max window, keep the `-1 = unset` sentinel |
| `OVT_MissionCondition_PlayerInRange` | `OVT_PlayerInRangeJobCondition` | any online player within R of the anchor |
| `OVT_MissionCondition_PlayerNotInRange` | `OVT_PlayerNotInRangeJobCondition` | |
| `OVT_MissionCondition_TownPlaceableCount` | `OVT_TownPlaceableCountJobCondition` | |
| `OVT_MissionCondition_TownHasEnemyTower` | `OVT_TownHasEnemyTowerJobCondition` | |
| `OVT_MissionCondition_BaseIsOccupied` | `OVT_BaseIsOccupiedJobCondition` | |
| `OVT_MissionCondition_SupportModifierSpace` | `OVT_SupportModifierSpaceJobCondition` | |
| — | `OVT_IsNearestJobCondition` | **no successor** (orphan) |
| — | `OVT_IsNearestTownWithDealerJobCondition` | **no successor** (orphan) |
| — | `OVT_IsNearestTownWithShopJobCondition` | **no successor** (orphan) |
| — | `OVT_TownHasDealerJobCondition` | **no successor** (orphan; also carries BUG-005's X-axis-only check — do not port the bug) |

**Modules** (15 new):

| New | Kind / restore policy | Replaces | Notes |
|---|---|---|---|
| `OVT_MissionModule_FindLocation` | instant, RESIDENT_SAFE | `OVT_FindRandomHouseJobStage`, `OVT_GetRadioTowerLocationJobStage` | one module, `m_eKind` enum (RANDOM_HOUSE / RADIO_TOWER / TOWN_CENTRE / BASE); writes a location variable |
| `OVT_MissionModule_SpawnCharacter` | instant + tag, NOT_RESTORABLE for the node that waits on it | `OVT_SpawnCivilianJobStage`, `OVT_SpawnFactionCharacterJobStage` | merged; faction/prefab attributes; tags the spawn |
| `OVT_MissionModule_SpawnGroup` | instant + tag | `OVT_SpawnGroupJobStage` | tags each member |
| `OVT_MissionModule_WaitForDeath` | resident, **NOT_RESTORABLE** | `OVT_WaitTillDeadJobStage` | polls the tag registry; emits `lost` when the tag is gone. Matches the shipped job drop rule, expressed declaratively |
| `OVT_MissionModule_WaitForPlayerInRange` | resident, RESIDENT_SAFE | `OVT_WaitTillPlayerInRangeJobStage` | |
| `OVT_MissionModule_WaitForTownSupport` | resident, RESIDENT_SAFE | `OVT_WaitTillSupportJobStage` | |
| `OVT_MissionModule_WaitForPlaceable` | resident, RESIDENT_SAFE | `OVT_PlaceableItemJobStage` | sphere query, same as legacy |
| `OVT_MissionModule_WaitForTowerDisabled` | resident, RESIDENT_SAFE | `OVT_WaitTillTowerDisabledJobStage` | |
| `OVT_MissionModule_AddSupportModifier` | instant, RESIDENT_SAFE | `OVT_AddSupportModifierJobStage` | |
| `OVT_MissionModule_GrantReward` | instant, RESIDENT_SAFE | *(none)* | **NEW** — "rewards at any point" |
| `OVT_MissionModule_PlaceMissionItem` | instant + tag, REDERIVE | *(none)* | **NEW** — spawns a tagged item, tracks it for vanilla persistence |
| `OVT_MissionModule_WaitForItemDelivery` | resident, RESIDENT_SAFE | *(none)* | **NEW** — polls "a tagged item is within R of the delivery point"; needs no handle, so restore is free |
| `OVT_MissionModule_Wait` | resident, RESIDENT_SAFE | `OVT_JobStageConfig.m_iTimeout` (never implemented) | timer off `m_iNodeEnteredAtMs` |
| `OVT_MissionModule_Branch` | instant, RESIDENT_SAFE | *(none)* | **NEW** — evaluates conditions and emits one of several outcome keys. The branching primitive |
| `OVT_MissionModule_WaitForSignal` | resident, RESIDENT_SAFE | *(none)* | **NEW** — completes when `SignalMission(instanceId, name)` is called. Drives the end-to-end test mission today; the scripted-trigger seam for resistance-missions and dialog later |
| — | — | `OVT_WaitTillJobAcceptedJobStage` | **no successor by design** — acceptance is framework state (OFFERED→ACTIVE), not a node |
| — | — | `OVT_GetDealerLocationJobStage` | **no successor** (orphan) |
| — | — | `OVT_GetShopLocationJobStage` | **no successor** (orphan) |
| — | — | `OVT_HasRecruitJobStage` | **no successor** (orphan; `recruit-missions` may add one) |

**Parity check** — every shipped job expressible:

| Job | Nodes |
|---|---|
| assassinate-traitor | FindLocation(RANDOM_HOUSE) → SpawnCharacter(civ) → SpawnGroup(OF) → WaitForDeath → `@complete` |
| base-recon | WaitForPlayerInRange(100) → `@complete` |
| raise-support | WaitForTownSupport(≥10) → `@complete` |
| propaganda-run | WaitForPlaceable("Poster") → `@complete` |
| pirate-radio | WaitForPlaceable("PirateRadio") → `@complete` |
| sabotage-radio-tower | FindLocation(RADIO_TOWER) → WaitForTowerDisabled → `@complete` |
| assassinate-officer | SpawnCharacter(officer) → WaitForDeath → AddSupportModifier → `@complete` |

The "wait till accepted" first stage of four of these disappears: an OFFERED mission does not tick.

### 3.5 File structure

```
Scripts/Game/
├── Data/Missions/                                   # pure, world-free, Logic-tier
│   ├── OVT_MissionGraph.c
│   ├── OVT_MissionRewardDistribution.c
│   ├── OVT_MissionWireCodec.c
│   └── OVT_MissionIds.c
├── Configuration/Missions/
│   ├── OVT_MissionEnums.c                           # scope, state, verb, result, reward policy, restore policy
│   ├── OVT_MissionConfig.c                          # config + node + transition
│   └── OVT_MissionRegistryConfig.c
├── GameMode/Managers/
│   └── OVT_MissionManagerComponent.c                # manager + OVT_MissionInstance
├── GameMode/Systems/Missions/
│   ├── OVT_MissionModule.c
│   ├── OVT_MissionCondition.c
│   ├── OVT_MissionOfferContext.c
│   ├── OVT_MissionReward.c
│   ├── Conditions/            (8 files)
│   └── Modules/               (15 files)
├── Components/Controller/
│   └── OVT_MissionRequestComponent.c
├── Components/Missions/
│   ├── OVT_MissionEntityComponent.c
│   └── OVT_MissionItemComponent.c
└── Persistence/Serializers/Components/
    ├── OVT_MissionManagerSerializer.c               # + OVT_PersistedMissionV1, OVT_PersistedMissionCounterV1
    └── OVT_MissionItemComponentSerializer.c

Configs/Missions/
├── overthrowMissions.conf                           # OVT_MissionRegistryConfig — the list
└── frameworkSmoke.conf                              # end-to-end test mission, m_bOfferable 0

Prefabs/ (Workbench edits — fresh GUIDs, user may need to open once)
├── GameMode/OVT_OverthrowGameMode.et                # + OVT_MissionManagerComponent, m_MissionsConfig → registry conf
├── GameMode/OVT_OverthrowController.et              # + OVT_MissionRequestComponent
└── Props/Missions/OVT_MissionDocuments.et           # example mission item carrying OVT_MissionItemComponent

Modified:
├── Scripts/Game/Global/OVT_Global.c                 # + GetMissions() (GetJobs() untouched)
├── Scripts/Game/GameMode/OVT_OverthrowGameMode.c    # + Init/PostGameStart hooks
├── Scripts/Game/GameMode/Managers/OVT_SkillManagerComponent.c   # + GiveXPPersistentId
└── Configs/Systems/Persistence/Overthrow.conf       # + two ComponentSerializers entries

Tests:
├── Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_Missions.c
├── Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_Missions_ConfigsResolve.c
├── Scripts/Game/Tests/TestSuites/Campaign/OVT_TEST_Campaign_Missions_Lifecycle.c
└── Scripts/Game/Tests/TestSuites/Persistence/  (cases added to the existing suite files)
```

---

## 4. Implementation Phases

Every phase ends with `tools/compile-check.sh` exit 0 and the **All** group green
(`tools/run-tests.sh "{6A6E2A002F53A581}"`). Phase 1 additionally establishes the **measured** test
baseline — do not quote counts from any doc, they drift.

### Phase 0 — Baseline & two spikes — **S — standard dev agent**

| # | Task |
|---|---|
| 0.1 | Run and record: `tools/compile-check.sh`, Fast group, All group. Record exit codes **and case counts** as the pre-change baseline in `context.md`. ⚠️ `run-tests.sh` launches a game client — warn the user before running it |
| 0.2 | **Spike: `Rpc()` array marshalling.** Determine empirically whether `Rpc(fn, array<string>)` round-trips. If yes, the upsert RPC may carry arrays directly; if no (or unproven), it uses `OVT_MissionWireCodec` delimited strings. Record the answer with the method used. Do **not** guess — a wrong arity/type compiles clean and dies silently (BUG-090) |
| 0.3 | Confirm the registry-conf pattern loads: a `configRoot` class holding `ref array<ref X>` where each entry inherits a separate `.conf` asset (jobs prove per-asset inheritance, base upgrades prove the registry conf; the *combination* is new) |

**Acceptance:** baseline recorded with real numbers; 0.2 has a written yes/no with evidence; 0.3 either
confirmed or the fallback (list directly on the manager attribute) chosen and written down.

### Phase 1 — Pure data model + persisted record shapes + Logic tier — **M — standard dev agent**

Nothing here touches a manager, the game mode or the world. The persisted record **classes** are
declared in this phase (and their names frozen) so the state machine is built against the save shape
rather than retrofitted to it.

| # | Task |
|---|---|
| 1.1 | `OVT_MissionEnums.c` — `OVT_MissionScope`, `OVT_MissionState`, `OVT_MissionVerb`, `OVT_MissionActionResult`, `OVT_MissionRewardPolicy`, `OVT_MissionRestorePolicy` |
| 1.2 | `OVT_MissionIds` — compose `configId#seq`, parse back, validate config-id shape (lowercase-kebab, non-empty, no `#`, no `@`), validate node-name shape (non-empty, unique per config, no leading `@`) |
| 1.3 | `OVT_MissionGraph` — `ResolveTarget(node, outcome) → string` (`""` = unmatched); `Validate(config, out array<string> errors)`: entry node exists, node names unique and legal, every transition target is a real node or a reserved terminal, no empty-string outcomes, no node with zero transitions unless every path out is terminal, reachability warning for orphan nodes |
| 1.4 | `OVT_MissionRewardDistribution` — `ResolveRecipients(policy, participants, triggererId, finderId, specificId, positions) → array<string>`; `SplitEvenly(total, n) → array<int>` conserving the total (`base = total / n`, remainder to the first `total % n` in participant order) |
| 1.5 | `OVT_MissionWireCodec` — `Join(array<string>) → string` / `Split(string) → array<string>` with a delimiter no persistent id or node name can contain; empty list ⇄ empty string; single element; embedded-delimiter rejection |
| 1.6 | `OVT_MissionInstance` record class — plain `Managed`, all fields per §3.1, with `GetVar/SetVar/HasVar` over the parallel arrays and `AddParticipant/RemoveParticipant/IsParticipant` |
| 1.7 | Persisted record classes `OVT_PersistedMissionV1` and `OVT_PersistedMissionCounterV1`, each with the **frozen-name header comment** (cite the measurement in `OVT_PersistedJob`'s header — the class name is the `$type` discriminator and a rename fails the read *and* poisons the rest of the stream) |
| 1.8 | `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_Missions.c` — the five cases in §7 |
| 1.9 | Prove each new Logic case red once; record how, per case, in `context.md`. No `maxAttempts`, ever |

**Acceptance:** compile clean; Fast group green with 5 additional cases; `Scripts/Game/Tests/TestSuites/Logic/`
still contains no reference to the static manager accessor or the engine game-mode getter (the tier's
grep rule, which also applies to comments); every new case has a recorded failure method.

### Phase 2 — Config schema, registry, manager skeleton, Init tier — **M — standard dev agent**

| # | Task |
|---|---|
| 2.1 | `OVT_MissionConfig` (`[BaseContainerProps(configRoot: true)]`) with the fields in §3.1; `m_sId` carries a header comment copied in spirit from `OVT_JobConfig.m_sId` (identity ≠ presentation; immutable once shipped) |
| 2.2 | `OVT_MissionNode`, `OVT_MissionTransition` (both `ScriptAndConfig`), `OVT_MissionModule` base, `OVT_MissionCondition` base, `OVT_MissionOfferContext` |
| 2.3 | `OVT_MissionRegistryConfig` (`configRoot`, `ref array<ref OVT_MissionConfig> m_aMissions`) + `Configs/Missions/overthrowMissions.conf` |
| 2.4 | `OVT_MissionManagerComponent` skeleton: `s_Instance`/`GetInstance`, `Init(owner)`, `PostGameStart()`, `m_MissionsConfig` attribute, `GetConfigCount()`, `GetConfig(int)`, `GetMissionIdByIndex(int)`, `FindConfigById(string)` with the same out-of-range contract jobs has (`""` / `-1`, never a silent match) |
| 2.5 | Config **validation at Init**: run `OVT_MissionGraph.Validate` on every config; log each error naming mission id + node; refuse to offer an invalid config (mark it non-offerable) rather than failing at runtime |
| 2.6 | `OVT_Global.GetMissions()` (next to `GetJobs():269`, which stays); `OVT_OverthrowGameMode` hooks following the `m_JobManager` pattern — `FindComponent` + `Init(this)` in `EOnInit` at `:1378`, `PostGameStart()` in the `DoStartGame` fan-out at `:335`. ⚠️ **The `Init` block must go ABOVE the `if(!IsMaster()) return;` gate at `:1408`** — anything resolved below it exists on the server only, and clients need this manager for the mirror |
| 2.7 | Manager lifecycle per current best practice (`OVT_MapMarkerManagerComponent`): `s_Instance = this` in `OnPostInit`, `s_Instance = null` in `OnDelete`, all collections allocated in the constructor so accessors are safe before `PostGameStart` |
| 2.8 | Prefab: add `OVT_MissionManagerComponent` to `Prefabs/GameMode/OVT_OverthrowGameMode.et`'s `components {}` block (fresh repo-unique GUID; the two newest managers use hand-picked sequential series) and point `m_MissionsConfig` at the registry conf |
| 2.9 | Add `GetMissions()` to `FindFirstNullGetter()` in **`OVT_TEST_Init_Globals_ManagersResolve`** (`OVT_TEST_InitSuite.c:34`, helper at `:57`) — that case is the project's manager-wiring guard and a new manager that skips it is unguarded |
| 2.10 | `OVT_TEST_Init_Missions_ConfigsResolve.c` — modelled on `OVT_TEST_Init_Jobs_StableIdsAreUniqueAndResolve` (`OVT_TEST_InitSuite.c:3180`): manager resolves, ≥1 config, every id non-empty/legal-shape/unique, index↔id round-trips, out-of-range and unknown-id both miss, **plus** every config passes graph validation with zero errors. Register with `[Test(suite: OVT_TEST_InitSuite, timeoutS: 30)]` on the case class |
| 2.11 | Prove the Init case red once (temporarily break an id or a transition target); record the method |

**Acceptance:** compile clean; All group green with the new Init case; `OVT_Global.GetMissions()` non-null
in a started campaign; a deliberately broken transition target makes the Init case go red naming the
mission and node.

### Phase 3 — Instance state machine + offer scheduler — **L — standard dev agent (largest phase)**

Not flagged advanced (it is new code, not a refactor, and its integrations are read-only), but it is the
highest-consequence logic in the feature: **the restore invariant is established here, not in Phase 7.**

| # | Task |
|---|---|
| 3.1 | Instance lifecycle: `CreateInstance(configId, ctx)` → id from `m_iNextInstanceSeq`, entry node, OFFERED; `EnterNode`, `LeaveNode`, `ApplyOutcome` with the bounded transition loop (`MAX_TRANSITIONS_PER_TICK`, then fail + log); `CompleteMission`, `FailMission`, `RemoveInstance` |
| 3.2 | **Unmatched outcome fails the instance and logs it.** No silent swallow, ever |
| 3.3 | `TickMissions` (1 s, server-guarded) over ACTIVE instances only; OFFERED instances do not tick |
| 3.4 | Location recomputation on every node change (`m_bContributesLocation` + `module.GetLocation`), stored on the instance for the UI/map to read later |
| 3.5 | Offer scheduler: `EvaluateOffers` (5 s, `Replication.IsServer()` first line), round-robin config cursor + bounded anchor slice (`OFFER_SLICE`), cheap gates before conditions (offerable → lifetime cap → concurrent cap → already-active-here), then AND over `m_aOfferConditions`, evaluated in authored order and **short-circuiting on the first false** so expensive conditions can be authored last |
| 3.6 | **Offer expiry.** `m_iOfferExpirySeconds` on the config (0 = never). The same sweep that offers also expires: an OFFERED instance older than its expiry is removed and its slot freed. Jobs has no expiry and OFFERED jobs do not tick, so an unaccepted public job holds its town slot **forever** — that dead end is the single most concrete "don't inherit this" item in the legacy system |
| 3.7 | **Lifetime counters increment only on successful creation**, and are rolled back if the entry node aborts. Jobs increments before success is known and never decrements, so `m_iMaxTimes` counts attempts rather than missions |
| 3.8 | **One id space per anchor kind.** A town id or base id used in the instance record, in occupancy scanning and in any lookup must be the *same* number everywhere. Jobs keys occupancy by the `m_Bases` array index while writing `base.id` onto the record — they agree only by accident of insertion order, and any base removal desynchronises three things at once |
| 3.9 | Occupancy **derived**, counters **stored** — the jobs division, which is what makes restore consistent: "is this config already active at this anchor" is answered by scanning the board, never by a parallel set |
| 3.10 | Public server API the tests and later features drive: `ForceOfferMission(configId, townId, baseId) → instanceId`, `JoinMission(instanceId, persId) → result`, `LeaveMission(instanceId, persId) → result`, `SignalMission(instanceId, signal)`, `GetMission(instanceId)`, `GetMissionsForParticipant(persId)` |
| 3.11 | Lifecycle `ScriptInvoker`s (started / node changed / completed / failed / reward granted / participants changed) — the Intel seam. **No intel code** |
| 3.12 | Notifications via `OVT_NotificationManagerComponent.SendTextNotification(tag, playerId, ...)` (`:106`) for offered / node advanced / completed / failed. Note the targeting model: a `playerId`-targeted notification is still **broadcast** and discarded client-side by everyone else (`RpcDo_RcvTextNotification:175`) — there is no per-player channel, so nothing secret may travel in a notification parameter. New preset tags in `Configs/overthrowBroadcastMessages.conf`; new strings in `Language/localization_Overthrow.st` **only** (never the generated runtime exports) |

**Acceptance:** compile clean; All group green; a mission forced from a test advances through a
two-node graph and completes; an outcome with no transition fails the instance and prints a line
naming the mission, node and outcome; a config cycle is caught by the bounded loop rather than hanging
the tick; `EvaluateOffers` does nothing on a client; an OFFERED instance past its expiry is removed and
its anchor becomes offerable again.

### Phase 4 — Participation, comms and replication — **L — ⚠️ ADVANCED: `network-specialist-advanced`**

The one authority boundary in the feature, a new controller component, a new prefab entry, JIP, and a
client mirror. This is the class of defect the harness cannot catch.

| # | Task |
|---|---|
| 4.1 | `OVT_MissionRequestComponent : OVT_Component` on `OVT_OverthrowController`, modelled line-for-line on `OVT_TravelRequestComponent`: `[ComponentEditorProps(category: "Overthrow/Components/Controller", ...)]`, `RequestMissionAction(verb, instanceId)` with the `if(Replication.IsServer()) RpcAsk_X(...) else Rpc(RpcAsk_X, ...)` idiom, `RpcAsk_MissionAction` `[Server]`, `RpcDo_MissionActionResult` `[Owner]`, `ResolveOwningPlayerId()` from the controller entity (never the payload), the listen-host direct-call branch in the result sender, and the two BUG-090 diagnostic prints (request received / result sent) |
| 4.1b | **No optimistic client write.** The client never sets participation state locally and waits for the server (jobs' `AcceptJob` writes `accepted`/`owner` client-side first and has no rollback if the server refuses) |
| 4.2 | **One RPC signature per direction.** Two verbs, `(int, string)`. Reject an unknown verb outright before any state is touched |
| 4.3 | Server-side join/leave rules: mission exists, not terminal, `m_bAllowLateJoin` when ACTIVE, not already a participant, is a participant for LEAVE. Result codes for every refusal — never silence |
| 4.4 | Last participant leaves → back to OFFERED with the cursor unchanged, unless `m_bFailOnLastAbandon` |
| 4.5 | JIP: manager `RplSave(ScriptBitWriter)` / `RplLoad(ScriptBitReader)` over the whole board — instance id, config id, active-node list, state, location + flags, town/base, participants, variable arrays. Arrays as count-then-elements. **Every read is `if(!reader.ReadX(dest)) return false;`**, and the client list is cleared only *after* the leading count read succeeds, so a truncated payload leaves the existing mirror alone instead of blanking it (`OVT_JobManagerComponent.c:889-955` is the shape; note it reads `RplId` into a temporary — we write no `RplId` at all) |
| 4.6 | Incremental: `RpcDo_UpsertMission(...)` and `RpcDo_RemoveMission(string instanceId)`, both `RplRcver.Broadcast`, both keyed **only** by instance id. List-valued fields carried per the Phase 0.2 spike (arrays if proven, else `OVT_MissionWireCodec`) |
| 4.7 | Client mirror: `m_aMissions` maintained on clients purely from JIP + upsert/remove; **read-only** — no client code mutates mission state |
| 4.8 | `OVT_SkillManagerComponent.GiveXPPersistentId(string persId, int num)` — resolves the record by persistent id, awards XP, streams only when the player is online, level-up notification only when online. `GiveXP(playerId,...)` is left alone (jobs still calls it) but may delegate |
| 4.9 | Prefab: add `OVT_MissionRequestComponent` to `Prefabs/GameMode/OVT_OverthrowController.et` (fresh GUID) |

**Acceptance:** compile clean; All group green; a join request from a client reaches the server (the
diagnostic print fires), is validated server-side and produces a result on the owner; **no client→server
RPC was added to `OVT_PlayerCommsComponent`** (grep proves it); a second client joining an existing
campaign receives the full board through JIP; upsert and remove use the same single key.

### Phase 5 — Rewards, mission items, entity tagging — **M — standard dev agent**

| # | Task |
|---|---|
| 5.1 | `OVT_MissionReward` config class (`m_ePolicy`, `m_sRecipientVar`, `m_iMoney`, `m_iXP`, `m_aItems`) and the manager's `GrantReward(mission, reward, triggererPersId)` built on the Phase 1 pure resolver |
| 5.2 | Payment: money `AddPlayerMoneyPersistentId`; XP `GiveXPPersistentId`; items through the **careful** insert path (`CanInsertResource(res, PURPOSE_DEPOSIT)` pre-check → spawn → `TryInsertItem`, per `OVT_PlayerCommsComponent.c:625-660`), **not** the jobs reward path (`OVT_JobManagerComponent.c:560-568`) which logs a warning and drops the item on a full inventory. Offline recipients are skipped **loudly** (log + notification), never silently |
| 5.3 | `OVT_MissionEntityComponent { m_sInstanceId, m_sKey }` — modelled on `OVT_PlaceableComponent` (`[RplProp]` string fields + `Replication.BumpMe()` on set); registers with the manager on post-init, unregisters on delete; manager keeps `instanceId → key → entity` (runtime only) |
| 5.4 | `OVT_MissionItemComponent` (same two fields) + `OVT_MissionItemComponentSerializer` so a carried/dropped mission item keeps its tag across a save; entry in `Configs/Systems/Persistence/Overthrow.conf` under a `ComponentClassPersistenceConfigRule`, not the game-mode block |
| 5.5 | `OVT_MissionModule_PlaceMissionItem` — spawns via `OVT_Global.SpawnEntityPrefabMatrix` at a location variable, tags it, then **last** calls `OVT_PersistenceTracking.Track()` (ask `IsTracked()` first), following `OVT_ResistanceFactionManager.PlaceItem:770` — track only after the object is known good, so a rejected spawn is never registered |
| 5.5b | Conversely, **transient** spawns (mission AI) must be released with `OVT_PersistenceManagerComponent.UntrackTransient` if anything ever tracked them, per BUG-118 and the precedent in `OVT_SpawnCivilianJobStage.c:45` — an untracked-but-recorded entity becomes a permanent orphan in the save tree |
| 5.6 | `OVT_MissionModule_WaitForItemDelivery` — polls "an entity tagged `(instanceId, key)` is within R of the delivery point"; **holds no handle**, which is why it is `RESIDENT_SAFE` |
| 5.7 | `OVT_MissionModule_GrantReward`, `OVT_MissionModule_WaitForSignal`, `OVT_MissionModule_Wait`, `OVT_MissionModule_Branch` |
| 5.8 | Example mission-item prefab `Prefabs/Props/Missions/OVT_MissionDocuments.et` carrying `OVT_MissionItemComponent` |

**Acceptance:** compile clean; All group green; a two-participant split reward pays each the right
amount with the total conserved (Campaign-tier case); a placed mission item is findable through the
registry by its tag, and a deleted one leaves no stale registry entry.

### Phase 6 — Parity module + condition library — **M — standard dev agent (separable)**

| # | Task |
|---|---|
| 6.1 | The 8 conditions in §3.4, each a thin port of its job counterpart against `OVT_MissionOfferContext`. Fix, do not port, BUG-005's X-axis comparison if any ported code has the same shape |
| 6.2 | `OVT_MissionModule_FindLocation` (4 kinds), `SpawnCharacter`, `SpawnGroup` — all tagging their spawns |
| 6.3 | `WaitForDeath` (NOT_RESTORABLE, `lost` outcome), `WaitForPlayerInRange`, `WaitForTownSupport`, `WaitForPlaceable`, `WaitForTowerDisabled`, `AddSupportModifier` |
| 6.4 | Every module declares `GetRestorePolicy()` explicitly — **no default**. Make the base class return `NOT_RESTORABLE` so a module that forgets is safe-by-omission (drops the instance) rather than silently replaying |
| 6.5 | Logic-tier cases for the pure conditions (Random weighting edges, TownSupport window incl. the `-1` sentinel) built with `new` + explicit field assignment — `[Attribute()]` defvalues are **not** applied by `new` |

**Acceptance:** compile clean; All group green; every module returns a non-default restore policy; a
one-line grep shows no module inheriting the base `GetRestorePolicy` by accident.

### Phase 7 — Persistence — **L — ⚠️ ADVANCED: `component-developer-advanced`**

| # | Task |
|---|---|
| 7.1 | `OVT_MissionManagerSerializer : ScriptedComponentSerializer` — `override static typename GetTargetType()`, `override protected ESerializeResult Serialize(notnull IEntity, notnull GenericComponent, notnull SaveContext)`, `override protected bool Deserialize(notnull IEntity, notnull GenericComponent, notnull LoadContext)`. `version` written **first**; `version < 1` → `return true` and touch nothing |
| 7.2 | Payload order (positional, frozen): version → `array<ref OVT_PersistedMissionV1>` → `array<ref OVT_PersistedMissionCounterV1>` → `m_iNextInstanceSeq` |
| 7.2b | ⚠️ **The local variable names ARE the property names.** `LoadContext.Read()` derives the key from the variable handed to it, so the reader's locals must be spelled exactly as the writer's — measured 2026-08-09: writing `jobRecords` and reading into `readJobs` **fails**. Name the reader's locals identically and say so in a comment |
| 7.2c | ⚠️ **Every read is checked, and a failed read is not benign.** A failed `LoadContext.Read()` leaves the destination array non-null and **empty**, which the apply path would then apply — wiping a live board. A genuinely empty board reads back `ok = TRUE` with zero records, so the two cases *are* distinguishable. On any failed read, abort and leave live state untouched (jobs' `AbortUnreadablePayload` returns `true` — "consumed" — deliberately: the choice is between applying garbage and applying nothing) |
| 7.3 | Record fields: instance id, config id, **active-node list** (`ref array<string>`), state, town/base, location + hasLocation, participants (`ref array<string>`), var keys + var values (parallel arrays), node-entered and offered timestamps. **No `RplId`, no entity handle, ever** |
| 7.4 | Header comment carrying the rules, in the shape of `OVT_JobManagerSerializer`'s: why the class name is part of the format; write order = read order; ids not indices; derive occupancy, store counters; no RPC in the apply path; and the **restore-is-assignment** argument for *this* system, which is enforced by policy rather than inherited by luck |
| 7.5 | `ApplyPersistedMissions()` — idempotent, safe to run against a live session (`ReapplyLatestSaveData`): rebuild the board by id (upsert, don't blind-append), drop unknown config ids and unknown node names with a log line, then per instance switch on `GetRestorePolicy()`: RESIDENT_SAFE = nothing, REDERIVE = `OnRestore()` (false → drop), NOT_RESTORABLE = drop and free the slot, counters retained |
| 7.6 | **`OnEnter` is never called from the restore path.** Assert it in review; there is no code path from `Deserialize` to `OnEnter` |
| 7.7 | Register `OVT_MissionManagerSerializer` in the game-mode `ComponentSerializers` block of `Configs/Systems/Persistence/Overthrow.conf` (entry `{65ACD95F40F6C669}`, block opens at `:23`), **appended after `OVT_VehicleManagerSerializer` at `:46` and before the vanilla `SCR_*` entries at `:48-63`**, continuing the `6B0E7A2x` GUID series with a repo-verified-unique GUID. ⚠️ Two failure modes: not listed = silently never called; and this block **replaces** the inherited list rather than merging into it (measured — `core/persistence/context.md:284`), so the eight vanilla entries must survive the edit untouched |
| 7.8 | Persistence-tier cases (§7) added to the existing suite files, obeying the tier's assertion rule (no persistence-framework, vanilla-persistence or save-data type name may appear in those files at all). Model the round-trip case on `OVT_TEST_PersistenceRoundTrip_JobBoard_SurvivesSaveAndReload` (`:3227`), which already has the `PHASE_ASSERT_IDEMPOTENT` phase this feature needs, and seed records with synthetic ids and counters the manager's own offer loop cannot touch |
| 7.9 | Prove each new Persistence case red once; record the method |

**Acceptance:** compile clean; All group green; a mission board written through the public manager API
reads back identically; re-applying the same payload twice produces the same board (no duplicates, no
double rewards); an instance resting on a `NOT_RESTORABLE` node is dropped with a log line and its
config becomes offerable again.

### Phase 8 — End-to-end test mission + verification gate — **M — standard dev agent, then user-driven**

| # | Task |
|---|---|
| 8.1 | `Configs/Missions/frameworkSmoke.conf` — `m_sId "framework-smoke"`, `m_bOfferable 0`, three nodes: `start` = `WaitForSignal("step1")` → `pay` = `GrantReward(SPLIT_EVENLY, $200 + 10 XP)` → `@complete`. A second transition `start --fail--> @fail` exercises branching |
| 8.2 | Register it in `overthrowMissions.conf` |
| 8.3 | `OVT_TEST_Campaign_Missions_Lifecycle.c` — the full loop: force-offer → two persistent ids join → state flips to ACTIVE → signal → node advance → both paid ($100 each) → completed → off the board |
| 8.4 | A second Campaign case for the **branch**: force-offer, join, signal the fail outcome, assert `@fail` was taken and no reward was paid |
| 8.5 | Persistence case: force-offer → join → save/re-apply → the instance is still on the board with the same id, node and participants |
| 8.6 | Prove each new case red once; record the method |
| 8.7 | **Manual gate** (user-driven): `tools/launch-server.sh` + `tools/launch-game.sh --timeout 3600 --profile OverthrowClient1 --allow-concurrent -- -client 127.0.0.1:2001`, second client with its own profile. Script-console-drive a force-offer, join from both clients, verify both mirrors and the JIP payload for a late joiner |

**Acceptance:** All group green with the new Campaign and Persistence cases; the smoke mission is never
offered to a player (grep + a Campaign assertion that `m_bOfferable` is 0); the manual MP check passes.

---

## 5. Key Technical Decisions

**D1 — Named-node graph, flat, with typed transitions.**
A mission is a dictionary of named nodes; each node holds exactly one module and a list of
`(outcome → target)` transitions. Instance state is *the active node's name plus a variable bag* —
one short string, trivially persistable, trivially validatable, and human-readable in a log line. The
alternative (nested/tree config) makes the persisted cursor a path, which is exactly the positional
identity the jobs migration existed to escape.

**D2 — Single active node, plural container.**
The runtime enforces exactly one active node in v1 (no scheduler, no join semantics, no
partial-completion rules — all YAGNI). But `m_aActiveNodes` is an **array** in the runtime record, on
the wire and in the save. Adding a composite/parallel node later is then a behaviour change, not a
save-format break. This costs one field's type today and buys the entire future option.

**D3 — Explicit join only; participants are persistent ids.**
The participant set is always known, so reward distribution is always well-defined, and no heuristic
("was this player nearby?") is ever needed. Persistent ids because a mission outlives a connection; a
runtime player id in a save or a participant list is a bug waiting for a reconnect. `OFFERED` means
zero participants; the first join flips it to `ACTIVE`; a mission that nobody has joined does not tick,
which also deletes the entire "wait till accepted" stage class.

**D4 — One identity, one key, everywhere.**
`instanceId = configId + "#" + seq`. It is the map key, the RPC key for **both** upsert and remove, the
save key and the log token. Jobs' update/remove key disagreement (BUG-038) is impossible by
construction here, not fixed by care. `m_iNextInstanceSeq` is persisted so ids never collide after a
load.

**D5 — No entity handle is ever persisted or replicated. Modules find world objects by tag.**
`OVT_MissionEntityComponent(instanceId, key)` self-registers with the manager. A module asks the
registry, never holds an `RplId`, and never writes one to a save. Three consequences fall out for free:
a vanilla-restored mission item re-registers itself on load with zero framework code; a wait module that
only ever queries is `RESIDENT_SAFE` by definition; and a missing target is an ordinary `lost` outcome
the config can route, instead of a hardcoded drop rule in the serializer.

**D6 — Every module declares a restore policy; the base default is the *safe* one.**
`RESIDENT_SAFE` (assign and run nothing), `REDERIVE` (assign, then `OnRestore` — idempotent, no
gameplay side effects), `NOT_RESTORABLE` (drop the instance, free the slot, keep counters). The base
class returns `NOT_RESTORABLE`, so a module author who forgets loses a mission rather than
double-spawning a squad. **`OnEnter` is never reachable from `Deserialize`.** Jobs got the same outcome
by an accident of its two-method protocol; branching makes the accident unreliable, so it is replaced
with a declaration.

**D7 — Replication: JIP full board + one upsert/remove pair, no `RplProp`.**
Same rationale as jobs and skills: the board is variable-length and records must exist for players with
no controlled entity. `RplSave`/`RplLoad` carry the whole board on join; runtime changes go out as one
broadcast upsert or one broadcast remove. Whether list-valued fields travel as arrays or as codec
strings is settled by the Phase 0.2 spike rather than assumed — `Rpc()`'s prototype is untyped variadic,
so a wrong shape compiles clean and dies at the wire (BUG-090). Clients hold a **read-only** mirror;
eligibility filtering is client-side display filtering, exactly as notifications already work.

**D8 — Client→server on a new controller component, one signature per direction.**
`OVT_MissionRequestComponent` on `OVT_OverthrowController`, copying `OVT_TravelRequestComponent`
wholesale: identity from the controller entity, unknown verb refused before any state is touched, an
explicit result code for every refusal, the listen-host direct-call branch, and the two diagnostic
prints that let a play-test tell "request never arrived" from "request refused". Nothing is added to
`OVT_PlayerCommsComponent` — project rule.

**D9 — Offer scheduling: server-guarded, staggered round-robin, no player dimension.**
`Replication.IsServer()` is the first line (jobs has no guard at all). Each 5 s tick evaluates one
config against a bounded slice of anchors, so per-tick cost is constant in world size; a full sweep of
7 configs × ~20 anchors takes ~18 ticks (~90 s), which is far inside the responsiveness a mission board
needs. The `× players` factor is gone because no surviving mission is player-allocated. Cheap gates
(offerable, caps, already-active-here) run before any condition touches the world, and conditions
short-circuit in authored order so the expensive ones go last. `RequestOfferEvaluation(configId)` exists
as the event-driven seam for the Intel epic; nothing calls it yet.

**D9b — Offers expire; the sweep that creates them also retires them.**
An OFFERED mission does not tick (that is what makes acceptance free), so nothing inside the state
machine can ever clean one up. Jobs has exactly this shape and no expiry, which means an unaccepted
public job holds its town's occupancy slot **for the rest of the campaign** — the config's `m_iTimeout`
field exists for it and was never read. `m_iOfferExpirySeconds` is checked in `EvaluateOffers`, so the
one loop that can create an offer is also the one that can retire it, and there is no second timer to
fall out of step.

**D10 — Persistence: ids not indices, records frozen by name, derive what can be derived.**
Records are keyed by stable ids (config id, instance id, node name); the class names
`OVT_PersistedMissionV1` / `OVT_PersistedMissionCounterV1` are part of the binary format because the
container writes a `$type` discriminator, and a rename **fails the read and poisons the rest of the
stream** — measured, documented in `OVT_PersistedJob`'s header, and repeated in ours. Two further
measured rules that read like pedantry and are not: the **reader's local variable names must equal the
writer's**, because `LoadContext.Read()` derives the property key from the variable it is handed; and a
**failed read yields a non-null empty array**, which the apply path would happily apply over a live
board — so every read is checked and any failure aborts without touching live state. Nested maps become
parallel primitive arrays. Occupancy is derived from the restored board; lifetime counters are stored
because nothing on the board implies them. `Deserialize` is idempotent because it also runs against a
live session.

**D11 — Rewards resolve purely, then pay.**
`ResolveRecipients` and `SplitEvenly` are static functions over plain values, so the Logic tier pins the
maths — including remainder conservation, which is where a split silently loses money. Payment is a
separate step that touches managers. Offline recipients are paid money and XP (both have persistent-id
paths, one of which this feature adds) and are **loudly skipped** for items, because there is no offline
inventory.

**D12 — Escrow is a future *append*, not a seam in code.**
`resistance-missions` will hold escrowed balances in this manager. Nothing is built now. The format is
designed so it is additive: escrow becomes a **new top-level array appended after the existing writes**
under version 2, leaving `OVT_PersistedMissionV1`'s shape and name untouched. Stating the growth path
now is what stops someone reserving fields "just in case".

**D13 — The test mission ships here, non-offerable.**
`m_bOfferable 0` means the offer scheduler never selects it, so it is invisible to players and
`mvp-missions` has no cleanup to do. It is driven exclusively through `ForceOfferMission` /
`SignalMission`, which are real public APIs later features need — not test-only backdoors.

**D14 — Coexistence with jobs is total and one-directional.**
Missions read the same managers jobs reads and writes nothing jobs owns. No job class is renamed or
deleted, no `Configs/Jobs/*.conf` is edited, `OVT_Global.GetJobs()` stays, and the job serializer entry
in `Overthrow.conf` is untouched. Both boards run, both notify, both persist. That is accepted noise
for one release; `mvp-missions` removes the jobs half after parity is play-verified.

---

## 6. Definition of Done

An independent evaluator should be able to verify all of the following without having read the
implementation.

### Functional Criteria

- [ ] **F1 — Configs resolve.** `OVT_Global.GetMissions().GetConfigCount() ≥ 1` in a started campaign;
      every config has a non-empty, lowercase-kebab, unique `m_sId`; index↔id round-trips; an unknown id
      and an out-of-range index both miss (`-1` / `""`).
- [ ] **F2 — Graphs validate.** Every shipped config passes `OVT_MissionGraph.Validate` with zero
      errors. A deliberately broken transition target makes the Init case go red naming the mission and
      the node.
- [ ] **F3 — Missions offer.** With the smoke mission temporarily made offerable, an eligible anchor
      produces exactly one instance, on the server only, with a unique instance id and OFFERED state.
- [ ] **F4 — Join via the controller component.** A client join request travels through
      `OVT_MissionRequestComponent`, is validated server-side with identity taken from the controller
      entity, adds the player's **persistent id** to the participant list, and flips OFFERED→ACTIVE on
      the first join. Every refusal returns a specific result code.
- [ ] **F5 — Branch transition taken on module outcome.** The smoke mission's `start` node routes to
      `pay` on one outcome and to `@fail` on another; both paths are exercised and produce different
      end states.
- [ ] **F6 — Unmatched outcome fails loudly.** A module emitting an outcome with no transition fails the
      instance and logs a line naming mission, node and outcome. It never silently continues.
- [ ] **F7 — Reward distributed by policy to multiple participants by persistent id.** A
      SPLIT_EVENLY reward of $200 across two participants pays $100 each; across three pays 67/67/66
      (total conserved); CLOSEST / TRIGGERER / FINDER / SPECIFIC each select the documented recipient.
- [ ] **F8 — Rewards at any point.** A `GrantReward` node mid-graph pays without completing the
      mission.
- [ ] **F9 — Mission item round trip.** `PlaceMissionItem` spawns a tagged item; a player picks it up
      and carries it to the delivery point; `WaitForItemDelivery` fires. The item is findable through
      the tag registry at every step and after a save.
- [ ] **F10 — Offline participants are paid.** Money and XP reach a participant who is not connected;
      item rewards for that participant are skipped with a log line and a notification, never silently
      dropped.
- [ ] **F11 — Late join.** A player joins an ACTIVE mission when `m_bAllowLateJoin` is set and is
      refused with a specific code when it is not.
- [ ] **F12 — Abandon.** The last participant leaving returns the mission to OFFERED with its cursor
      unchanged (or fails it when `m_bFailOnLastAbandon`).
- [ ] **F13 — Offers expire.** An OFFERED instance older than `m_iOfferExpirySeconds` is removed and its
      anchor becomes offerable again. With expiry set to 0 it persists indefinitely (the opt-out).

### Quality Criteria

- [ ] **Q1 — Compile clean.** `tools/compile-check.sh` exit 0.
- [ ] **Q2 — No regressions.** Fast and All groups both exit 0, with **more** cases than the Phase 0
      baseline and none removed.
- [ ] **Q3 — Jobs still work.** The jobs board still offers, accepts, advances, rewards and persists.
      `grep -rn "OVT_Job" Scripts/ Configs/ Prefabs/` shows no deletion or rename attributable to this
      feature; `Configs/Jobs/*.conf` and the job serializer's `Overthrow.conf` entry are byte-identical.
- [ ] **Q4 — Server authority.** No client code mutates mission state. The client mirror is written only
      by `RplLoad` and the two broadcast RPCs.
- [ ] **Q5 — No legacy comms.** Nothing was added to `OVT_PlayerCommsComponent`
      (`git diff --stat` on that file is empty).
- [ ] **Q6 — No handles in state.** `grep -n "RplId" ` over the mission serializer and the persisted
      record classes returns nothing.
- [ ] **Q7 — Restore cannot replay.** There is no call path from `Deserialize` to any module's
      `OnEnter`. Every module returns an explicit `GetRestorePolicy()`.
- [ ] **Q8 — EnforceScript constraints.** No ternaries; `ref` on every Managed in an array or map;
      `RplId` (not `EntityID`) anywhere a reference crosses the wire; `OVT_` / `m_i m_f m_s m_b m_a m_m`
      naming; Doxygen `//!` on every public method.
- [ ] **Q9 — Every new test case has a recorded way it was made to fail.** No `maxAttempts` anywhere.
- [ ] **Q10 — Localization.** Every new user-facing string is a `#OVT-` key added to
      `Language/localization_Overthrow.st` **only**. No generated `localization_Overthrow.<lang>.conf`
      was edited.

### Integration Criteria

- [ ] **I1 — `OVT_Global.GetMissions()` resolves** in a started campaign and returns the same instance
      as `OVT_MissionManagerComponent.GetInstance()`. `GetJobs()` still resolves.
- [ ] **I2 — Manager lifecycle.** `Init` runs on both new game and load; `PostGameStart` runs on new
      game only; both are wired in `OVT_OverthrowGameMode` next to the job manager's.
- [ ] **I3 — Serializer registered and round-trips.** `OVT_MissionManagerSerializer` appears in the
      game-mode `ComponentSerializers` block of `Configs/Systems/Persistence/Overthrow.conf`, and a
      Persistence-tier case proves a board survives a save→re-apply with identical ids, nodes and
      participants.
- [ ] **I3b — Nothing was displaced.** That block still lists all twelve pre-existing Overthrow
      serializers **and** the eight vanilla `SCR_*` ones. The block replaces rather than merges, so a
      `git diff` on that file must show additions only.
- [ ] **I4 — Idempotent apply.** Applying the same payload twice yields the same board — no duplicate
      instances, no second reward.
- [ ] **I5 — JIP payload present.** A client joining a session with an active board receives it: the
      client mirror is non-empty and matches the server's board by id, node and participant list.
- [ ] **I6 — Notifications routed.** Mission lifecycle notifications go through
      `OVT_NotificationManagerComponent`, not raw hints.
- [ ] **I7 — Waypoint seam.** Each replicated instance exposes `m_vLocation` + `m_bHasLocation`, and
      `GetMissionsForParticipant(persId)` returns the local player's missions on a client — the exact
      surface `mission-ui` will consume.
- [ ] **I8 — Intel seam.** The six lifecycle invokers exist and fire. No intel code ships.

### Verification Method

**Automated — run these and record exit codes.** ⚠️ `run-tests.sh` launches a Reforger **client**,
which opens a window on the user's desktop; warn before running.

```bash
tools/compile-check.sh                                  # expect 0
tools/run-tests.sh "{6A6E29FF47ECB840}"                 # Fast — expect 0
tools/run-tests.sh "{6A6E2A002F53A581}"                 # All  — expect 0

# single-suite / single-case forms for debugging
tools/run-tests.sh OVT_TEST_Logic_Missions
tools/run-tests.sh OVT_TEST_Campaign_Missions_Lifecycle
tools/run-tests.sh OVT_TEST_PersistenceSuite
```

Compare case counts against the Phase 0 baseline — **measured, never quoted from a doc.**

**Greps an evaluator can run:**

```bash
grep -rn "OVT_MissionManagerSerializer" Configs/Systems/Persistence/Overthrow.conf   # must hit
grep -rn "RplId" Scripts/Game/Persistence/Serializers/Components/OVT_MissionManagerSerializer.c  # must miss
grep -rn "OnEnter" Scripts/Game/Persistence/Serializers/Components/OVT_MissionManagerSerializer.c # must miss
git diff --stat Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c            # must be empty
git status --porcelain Configs/Jobs/                                                 # must be empty
grep -rn "GetRestorePolicy" Scripts/Game/GameMode/Systems/Missions/Modules/ | wc -l  # must equal the module count
```

**Manual MP check** (the class of defect no suite reaches):

1. `tools/launch-server.sh` (local mode).
2. `tools/launch-game.sh --timeout 3600 --profile OverthrowClient1 --allow-concurrent -- -client 127.0.0.1:2001`
3. Second client: same command with `--profile OverthrowClient2`.
4. From the server console, force-offer the smoke mission. Confirm both clients' mirrors contain it
   (script-console dump of `GetMissionsForParticipant` / board count).
5. Join from client 1, then from client 2. Confirm participant list on the server has both persistent
   ids and both clients see the ACTIVE state.
6. Signal the mission. Confirm both clients see the node change and both players are paid $100.
7. **JIP:** connect a third client *after* step 5 and confirm it receives the board with the correct
   participants and node.
8. Save, quit to menu, Continue. Confirm the board returns with the same instance id and node, that no
   AI was double-spawned, and that no reward was paid twice.

---

## 7. Testing Strategy

**How cases register — get this right or the suite silently runs nothing.** A *case* is a
`SCR_AutotestCaseBase` subclass carrying `[Test(suite: OVT_TEST_XSuite, timeoutS: N)]`; it may live in
the suite file or in a sibling file in the same tier directory (both patterns ship today —
`OVT_TEST_Init_Jobs_...` is inline, `OVT_TEST_Init_FactionDelegates_Spawn.c` is a sibling). The
*group* configs `Configs/Tests/OVT_TestGroup_Fast.conf` and `OVT_TestGroup_All.conf` list **suites, not
cases**, so a new case in an existing suite needs no config edit. `[BaseContainerProps()]` is mandatory
on any concrete suite class. Cases execute **alphabetically by class name** and must be independent.

**Logic tier** (`OVT_TEST_Logic_Missions.c`, world-free — no manager, no game mode, no world, and those
identifiers may not appear even in comments):

| Case | Claim |
|---|---|
| `..._GraphTransitionResolution` | A matching outcome returns its target; a non-matching outcome returns `""`; reserved terminals `@complete`/`@fail` resolve as terminals, not as node names; the first matching transition wins when two share an outcome |
| `..._GraphValidation` | Missing entry node, duplicate node name, transition to a non-existent node, empty-string outcome and a node name starting `@` are each reported as an error naming the offender; a well-formed graph reports zero errors |
| `..._RewardSplitConserves` | `SplitEvenly(200,2)=[100,100]`; `(200,3)=[67,67,66]` and sums to 200; `(0,3)` all zeros; `(5,0)` returns empty and does not divide by zero; `(1,3)=[1,0,0]` |
| `..._RewardRecipientPolicies` | Each of the five policies selects the documented recipient(s) from a hand-built participant list; SPECIFIC/FINDER with an unset variable returns empty rather than a wrong player |
| `..._WireCodecRoundTrip` | Empty list ⇄ `""`; one element; many; a value containing the delimiter is rejected rather than silently split |
| `..._MissionIds` | `Compose/Parse` round-trips; ids with `#`, `@`, uppercase or spaces are rejected; sequence numbers are monotonic |

*(Phase 6 adds condition cases — Random weighting edges, TownSupport window incl. the `-1` sentinel —
built with `new` plus explicit field assignment, because `new` does not apply `[Attribute()]` defvalues.)*

**Init tier** (`OVT_TEST_Init_Missions_ConfigsResolve.c`, modelled on the jobs stable-id case): manager
resolves; ≥1 config; ids non-empty, legal-shape and unique; index↔id round-trips; out-of-range and
unknown-id both miss; **every config passes graph validation with zero errors**. This is the case that
catches a typo'd transition target before it reaches a player.

**Campaign tier** (`OVT_TEST_Campaign_Missions_Lifecycle.c`, needs a started campaign): force-offer →
two persistent ids join → ACTIVE → signal → node advance → both paid $100 → completed → off the board.
Second case for the fail branch (different outcome, `@fail` taken, nothing paid). Third case asserting
the smoke mission's `m_bOfferable` is 0 so it can never reach a player.

**Persistence tier** (cases added to the existing suite files, obeying that tier's assertion rule —
no persistence type name may appear in those files): a board with one joined instance survives a
save→re-apply with identical instance id, active node, participants and variables; re-applying twice
produces no duplicate and no second payout; an instance parked on a `NOT_RESTORABLE` node is dropped
and its config becomes offerable again.

**Not automatable — manual, and named explicitly:**
- Everything multiplayer: join over the wire, JIP payload, upsert/remove ordering, two clients'
  mirrors agreeing. **The most common regression class in this project, and the harness cannot reach
  it.** Steps 1–7 above are a hard gate.
- The real quit-and-continue restart path (the suite covers in-session re-apply only). Step 8.
- Physical module behaviour: spawned characters/groups, item pickup and carriage, placeable detection,
  radio-tower disable — all need world geometry and a player.
- Offer scheduler *behaviour over time* (does the board fill at a reasonable rate) — watch a campaign
  for a few in-game hours.

**Fallibility rule:** every new automated case is shown red once by a recorded edit before it ships. A
case that has never failed is not evidence. `maxAttempts` is never used.

---

## 8. Dependencies

**Internal — consumed read-only through existing APIs:**
`OVT_TownManagerComponent` (town data, support/stability, random houses), `OVT_OccupyingFactionManager`
(bases, radio towers), `OVT_EconomyManagerComponent` (`AddPlayerMoneyPersistentId` at `:1188` — already
offline-safe: it skips the stream and still writes the record), `OVT_SkillManagerComponent` (`GiveXP` at
`:289` — **extended** with `GiveXPPersistentId`), `OVT_PlayerManagerComponent`
(`GetPersistentIDFromPlayerID:530`, `GetPlayerIDFromPersistentID:553` — returns `-1` when offline,
`GetPlayer`, `GetController`), **`OVT_ResistanceFactionManager`** (placeables live *here*, via
`m_PlaceablesConfig` at `:69` and the `PlaceItem`/`BuildItem` paths — **there is no
`OVT_PlaceablesManagerComponent`**), `OVT_PlaceableComponent` (the `[RplProp] m_sOwnerPersistentId`
pattern `OVT_MissionEntityComponent` copies), `OVT_NotificationManagerComponent`
(`SendTextNotification(tag, playerId, p1, p2, p3)` at `:106`), `OVT_Global`
(`SpawnEntityPrefab:647` / `SpawnEntityPrefabMatrix:670`, safe-spawn helpers),
`OVT_PersistenceTracking` (`Track:37` / `IsTracked:72` — server-only by construction, a silent no-op on
clients), `OVT_PersistenceManagerComponent.UntrackTransient` (`:780`).

**Internal — patterns copied, not called:**
`OVT_TravelRequestComponent` (the controller-comms template), `OVT_JobManagerComponent` (JIP shape only),
`OVT_JobManagerSerializer` (the persistence rules), `OVT_BaseUpgradesConfig` (registry-conf pattern),
`OVT_TEST_Init_Jobs_StableIdsAreUniqueAndResolve` (the Init-tier guard shape).

**Modified files (four, all additive):** `OVT_Global.c`, `OVT_OverthrowGameMode.c`,
`OVT_SkillManagerComponent.c`, `Configs/Systems/Persistence/Overthrow.conf`.

**External (vanilla):** `ScriptAndConfig`, `BaseContainerProps(configRoot)`, `ScriptComponent` /
`ScriptComponentClass`, `ScriptedComponentSerializer` + `SaveContext` / `LoadContext`, `ScriptBitWriter`
/ `ScriptBitReader` (JIP only — **never** hand-rolled outside `RplSave`/`RplLoad`: constructing one from
script hard-crashes on first use, so JIP bitstreams can never be unit-tested), `RplRpc` / `Replication`,
`ScriptInvoker`, `SCR_AutotestCaseBase`.

**Workbench work the user must do (or confirm):** the three prefab edits in §3.5. A missing component
entry surfaces as a null manager at runtime, not a compile error.

**Blocks:** `mission-ui` (needs the client mirror + `OVT_MissionRequestComponent`), and through it
`mvp-missions`, `resistance-missions`, `recruit-missions`, `dialog`.

---

## 9. Risks & Mitigation

**R1 — Restore replays a module and double-spawns.** *(High impact, the defect this design exists to
prevent)*
Jobs avoided it by luck of its two-method protocol; branching removes that luck. **Mitigation:**
explicit `GetRestorePolicy()` on every module with a **safe base default** (`NOT_RESTORABLE`); no code
path from `Deserialize` to `OnEnter`; a grep in the Definition of Done proving every module declares a
policy; a Persistence case asserting a `NOT_RESTORABLE` instance is dropped rather than restored.

**R2 — RPC arity/type mismatch dies silently.** *(High likelihood without discipline — BUG-090)*
`Rpc()`'s prototype is untyped variadic, so a wrong argument count or type compiles clean and fails at
the wire. **Mitigation:** exactly one client→server signature and one owner-result signature; a Phase 0
spike settles whether arrays marshal at all before any code depends on it; the codec fallback keeps every
field a primitive; the two diagnostic prints from `OVT_TravelRequestComponent` are copied so a play-test
can distinguish "never arrived" from "refused".

**R3 — Coexistence with jobs produces duplicate or confusing player-facing behaviour.** *(Medium)*
Two boards, two notification streams, two serializers, two offer loops running at once for one release.
**Mitigation:** the framework ships **no offerable mission**, so players see nothing new until
`mvp-missions`; jobs files are byte-untouched (asserted by `git status` in the DoD); the notification
tags are distinct.

**R4 — Save-format freeze discipline lapses.** *(Medium likelihood, severe consequence)*
Renaming a persisted record class **fails the read and poisons the rest of the stream** — measured, and
it silently hands the manager an empty board that then overwrites a live campaign. **Mitigation:**
frozen-name header comments on both record classes citing the measurement; record fields append-only;
escrow explicitly planned as a *new appended array* under version 2 rather than a field insertion;
`version` written first with `version < 1 → return true`.

**R5 — Graph typos ship.** *(Medium — string-keyed transitions are the price of a data-driven graph)*
A transition pointing at a node that does not exist is invisible until a player takes that branch.
**Mitigation:** `OVT_MissionGraph.Validate` runs at Init on every config and marks a failing config
non-offerable; the Init-tier case fails the build's test gate if any shipped config has an error; an
unmatched outcome at runtime fails the instance **loudly** rather than stalling it.

**R6 — Scope: 8 conditions + 15 modules is a lot of surface for one feature.** *(Medium)*
**Mitigation:** Phase 6 is explicitly separable and can slip to `mvp-missions` without blocking anything
— the end-to-end proof (Phase 8) depends only on Phases 1–5 and 7. Ship the framework green with a small
module set rather than a big module set with a shaky core.

**R7 — Modules ship unexercised at runtime.** *(Medium, accepted)*
Only the smoke mission's modules actually run in this feature; the parity library is compile-checked and
Logic-tested where pure, but not play-tested until `mvp-missions` authors configs against it.
**Mitigation:** state it plainly in `context.md` as a known limitation and hand `mvp-missions` a list of
never-executed modules to exercise first.

**R8 — Offer scheduler starves or bursts.** *(Low–Medium)*
A round-robin over configs × slices can, at pathological cursor alignments, favour early anchors.
**Mitigation:** the anchor cursor advances independently of the config cursor so the pairing rotates;
per-config concurrent and lifetime caps bound the damage; observation over a long campaign is a named
manual check.

**R9 — Tagged AI is not restored by vanilla, so kill-target missions die on load.** *(Low, intended)*
Overthrow disables AI self-spawn in `Overthrow.conf`. A `WaitForDeath` node therefore cannot survive a
load. **Mitigation:** that is exactly why it is `NOT_RESTORABLE` — the instance is dropped, its slot
freed and it re-offers later. Identical to today's shipped job behaviour, and now declarative instead of
a hardcoded drop rule.

**R10 — Client mirror diverges from the server board.** *(Medium)*
**Mitigation:** one key for both upsert and remove; the mirror is written from exactly three places
(`RplLoad`, upsert, remove) and nowhere else; a manual MP step compares both clients' boards against the
server's after a join and a node change.

**R12 — The persistence config edit displaces existing serializers.** *(Low likelihood, campaign-wide
consequence)*
`ComponentSerializers` in a GUID-matched override **replaces** the inherited list rather than merging
into it — measured, and the reason the block currently re-declares all eight vanilla entries. An edit
that "tidies" it, or a tool that rewrites it, silently disables persistence for every component it drops.
**Mitigation:** the change is a two-line append; DoD item I3b makes "additions only" a checked
criterion; a Persistence-tier run catches a dropped Overthrow serializer immediately (though not
necessarily a dropped vanilla one).

**R13 — Offers accumulate and clog the board.** *(Medium, and the exact failure jobs has today)*
An OFFERED mission does not tick, so without expiry nothing can ever remove one, and its anchor stays
occupied for the campaign. **Mitigation:** D9b — expiry is checked in the same sweep that creates
offers; F13 makes it a checked criterion; the default expiry is a real number, not 0.

**R11 — Hand-authored GUIDs and Workbench-stale scripts.** *(Medium)*
Three prefab edits, two config files and two serializer registrations all need fresh GUIDs, and a
missing component entry is a runtime null, not a compile error. **Mitigation:** generate fresh 16-hex
GUIDs; have the user open each edited prefab once; remember that after WSL edits Workbench can play-test
**stale** scripts — refocus/reload before concluding a fix did not work.

---

## 10. Quality Bar

This is a **backend feature with no user-visible surface**, so the bar is not "does it feel good" but
"is it correct when nobody is watching".

**Reliability**
- A mission never stalls silently. Every terminal condition — unmatched outcome, missing config, missing
  node, exceeded transition budget, lost target entity — produces a state change **and** a log line
  naming mission, node and cause.
- The offer scheduler is bounded per tick and guarded by `Replication.IsServer()` as its first line. No
  loop in this feature is `O(configs × anchors × players)`.
- No timer is registered that is never removed; the manager cleans up its `CallLater`s.

**Data integrity**
- One identity, everywhere: instance id is the map key, the RPC key, the save key and the log token.
  There is no second key and no composite key anywhere in the feature.
- Rewards conserve their total. A split that loses a currency unit to integer division is a bug, and the
  Logic tier pins it.
- Participants are persistent ids in every layer. A runtime player id never enters mission state, the
  wire payload or the save.
- No entity handle is persisted or replicated. World objects are reached through the tag registry only.

**Save-format discipline**
- `version` is written first; `version < 1` returns true and touches nothing.
- Write order equals read order; every read is checked.
- Record class **names** are part of the format and carry a freeze comment explaining why, with the
  measured consequence of ignoring it.
- Keys are stable ids, never config indices or array positions.
- Nested maps become parallel primitive arrays.
- `Deserialize` is idempotent and safe against a live session; it contains no RPC and no module entry
  call.
- Growth is additive and planned (escrow → appended array, version 2), not improvised.

**Multiplayer correctness**
- The server decides everything. The client mirror is read-only and is written from exactly three
  places.
- Identity is resolved from the controller entity that received the RPC, never from the payload.
- One client→server signature, one owner-result signature; an unknown verb is refused before any state
  is touched.
- Every refusal returns a specific result code — silence is indistinguishable from a request that never
  left the client.
- JIP carries the complete board; a late joiner's view is identical to an early joiner's.

**Engineering hygiene**
- Every decision that can be a pure function is one, and lives in `Scripts/Game/Data/Missions/` with a
  Logic-tier case that has been proven able to fail.
- No ternaries; `ref` on every Managed in a container; `RplId` never `EntityID` over the wire; `OVT_` and
  `m_*` naming; Doxygen `//!` on public methods.
- Every phase leaves `tools/compile-check.sh` clean and the All group green, with case counts compared
  against a **measured** baseline.
- The jobs system is left exactly as found.

---

*Plan created 2026-08-13 from `docs/features/missions/framework/requirements.md`, the epic docs, and two
full exploration passes over the tree: (a) the legacy jobs system — all 12 conditions, 15 stages, 7
shipped configs, the 1108-line manager and the 665-line serializer, including its measured persistence
rules; and (b) the manager/serializer/test infrastructure — `OVT_Global`, the game-mode init order and
its `IsMaster()` gate, `OVT_TravelRequestComponent`, `OVT_NotificationManagerComponent`,
`OVT_BaseUpgradesConfig`, `Configs/Systems/Persistence/Overthrow.conf`, the four test tiers and their
group configs, the persistent-id helpers, and the placeable/tracking precedents. All `file:line`
citations are load-bearing — keep them when editing.*
