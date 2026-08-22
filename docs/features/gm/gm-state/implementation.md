# GM State — Implementation Plan

**Status:** Ready for Review (MP play-test owed — see context.md "Needs Human Verification")
**Epic:** gm (feature 1 of 5 — Phase 1 of the 3-phase epic)
**Started:** 2026-08-14
**Target Completion:** TBD
**Last Updated:** 2026-08-14 17:55 AEST

> All `file:line` citations in this document are load-bearing — they were verified against the working
> tree at `f47b66a1` and against the Reforger 1.8 reference tree at `/mnt/n/Projects/Arma 4/ArmaReforger`.
> Keep them when editing. Where this plan and `requirements.md` disagree, **this plan wins** and the
> disagreement is recorded in §5.

---

## 1. Executive Summary

Overthrow's campaign runs almost entirely on the server. A server owner who opens Game Master today can see
_entities_ — a group standing in a field, a base, a town — but none of the **state that explains them**: how
much threat has accumulated, what the occupying faction has to spend, when the next resource distribution or
resistance payout lands, why that group exists at all or which base sent it.

This feature builds the one seam that answers those questions. It is **read-only**, **gated to authorized
Game Masters**, and it is the **data spine for the other four Phase 1 features** — the Overthrow panel, HUD
icons, waypoint visualization and GM map layers are pure consumers of it. Phase 2 of the epic will add
write-side management actions to the same component, so it is named for the role (`OVT_GMRequestComponent`),
not for this phase's contents.

**The shape, in one paragraph.** A specialized component on `OVT_OverthrowController`
(`Prefabs/GameMode/OVT_OverthrowController.et`, 17 components today) extends `OVT_ControllerRequestComponent`
and carries one client→server request: "send me a campaign snapshot". The client fires it when the Game
Master editor opens and re-fires it on a timer while it stays open; the server checks the caller's engine role
on **every** handler entry and answers with a **fan of small owner-targeted RPCs** — a `Begin`, a handful of
campaign records, one record per base / base upgrade / deployment / tagged AI group, and an `End`. Every RPC
in the fan carries the client's own **sequence id**, so a client that has already superseded a request
discards its late arrivals instead of interleaving them. The client assembles the fan into a plain
`OVT_GMCampaignState` store and fires a `ScriptInvoker`; siblings read the store and never touch an RPC.

**Four decisions were made by the user before planning and are not re-opened here** (§5 records them with
their rationale): poll-while-GM-open rather than a delta protocol; the gate is `EPlayerRole.GAME_MASTER` OR
`SCR_Global.IsAdmin`; group origin is recorded by tagging groups at their spawn sites; and the threat *grid*
is deferred to `gm-map`, with the request protocol made versioned and extensible so gm-map can add a request
type without breaking the wire.

**Three code facts found during planning change the work and are worth stating up front:**

1. **The obvious way to predict the next resource distribution would mutate the campaign.**
   `OVT_OccupyingFactionManager.GainResources()` (`Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c:1433`)
   computes the amount **and then adds it** — `m_iResources += newResources` (`:1461`) — and calls
   `AllocateDeploymentResourcesIfNeeded(newResources)` (`:1466`). A strictly-read-only feature calling it to
   fill a "next distribution" field would hand the occupying faction a free income tick every time a GM opened
   the editor. Phase 1 therefore **extracts the arithmetic into a pure static** that both the live path and the
   prediction path call. That is also the only reason any of this feature's maths becomes unit-testable.

2. **The suggested time-multiplier is the wrong one.**
   `OVT_TimeAndWeatherHandlerComponent.GetDayTimeMultiplier()`
   (`Scripts/Game/GameMode/OVT_TimeAndWeatherHandlerComponent.c:9`) returns `m_fDayTimeAcceleration` — the
   **day-only** configured value. The engine switches to `m_fNightTimeAcceleration` at night
   (`ArmaReforger/scripts/Game/Components/Environment/SCR_TimeAndWeatherHandlerComponent.c:265-283`), so that
   value is simply wrong for half of every campaign day. `TimeAndWeatherManagerEntity.GetDayDuration()`
   (proto on `ArmaReforger/scripts/GameLib/generated/Entities/BaseWeatherManagerEntity.c:24`) returns real
   seconds per in-game day **under whichever acceleration is currently in force**, and vanilla already uses
   `86400 / GetDayDuration()` as exactly this conversion
   (`ArmaReforger/scripts/Game/UI/Components/SCR_IngameClockUIComponent.c:77`). Use it. See §5 D5.

3. **There is no chokepoint for group spawns, but there are two partial funnels — and there must be no
   untag sites at all.** A full survey found **11 distinct server-side group-spawn statements across 10 files**
   and **27 delete/drop sites**, several of which already leak: nobody ever deletes
   `OVT_QRFControllerComponent.m_Groups` entries (only the controller entity is deleted, at
   `OVT_OccupyingFactionManager.c:933` and `:1000`); `OVT_EntitySpawningAPI.CleanupGroup`
   (`Scripts/Game/GameMode/Deployments/OVT_EntitySpawningAPI.c:379`) deletes a group's **soldiers but not the
   group entity**; camp/FOB removal never touches `garrisonEntities`. Mirroring that mess with 27 untag calls
   would inherit every one of its leaks. The registry therefore **never untags** — it **sweeps** entries whose
   EntityID no longer resolves, each time a snapshot is built. Tagging drops to ~13 one-line insertions and
   cleanup becomes structurally impossible to get wrong. See §5 D7.

---

## 2. Goals

### Primary

1. **A GM can see the campaign's hidden numbers.** Threat, occupying-faction resources (both pools), the next
   resource distribution (amount + countdown), the next resistance payout (amount + countdown).
2. **A GM can see why an entity exists.** Every Overthrow-spawned AI group reports which base or town produced
   it and what for; every base reports its aggregate resources, garrison group count and per-upgrade
   breakdown; every deployment reports its config name, faction, invested resources and active state.
3. **A non-GM client receives nothing.** No new data reaches regular clients — not one RPC, not one byte.
   The gate is server-side and re-checked on every handler.
4. **Strictly read-only.** No campaign state is mutated by any path this feature adds, including the
   prediction path.
5. **Countdowns stay accurate without per-second traffic.** The server sends a deadline; the client ticks it
   down locally and re-syncs on the next poll.
6. **Siblings consume a documented client-side cache, never the wire.** One store, two invokers, one
   accessor — written down before the first sibling is planned.

### Secondary

7. **The wire format is versioned and extensible**, so `gm-map` can add a threat-grid request type and
   `overthrow-panel`/`hud-icons` can add fields without a coordinated client/server break.
8. **The maths is unit-tested.** Deadline derivation and resource-gain prediction move into pure statics that
   the Logic tier can assert — coverage that does not exist today for either.
9. **Phase 2 has a home.** Write actions land on this component without a rename or a second seam.

### Explicit non-goals

- **Any UI.** No panel, no icon, no map layer, no widget. Those are features 2–5.
- **Any mutation.** No give-resources, no give-money, no spawn-deployment. That is Phase 2.
- **A threat grid payload.** Deferred to `gm-map` by user decision (§5 D9).
- **A delta protocol.** Full snapshot per poll, by user decision (§5 D1).
- **Persistence of anything.** The group registry is runtime-only and never saved (§5 D8).
- **Per-entity on-demand request RPCs.** The snapshot already carries every record; "on demand" is a cache
  lookup with zero traffic (§5 D4).
- **Per-upgrade *position* data and civilian group records.** See §5 D10 and D7 for what is deliberately left
  out and who picks it up.

---

## 3. Architecture Overview

### 3.1 Component hierarchy

```
OVT_OverthrowController  (per-player entity, Prefabs/GameMode/OVT_OverthrowController.et)
│
└── OVT_GMRequestComponent : OVT_ControllerRequestComponent          NEW  ← the ONLY networked class
    │   server: the gate, the snapshot fan
    │   client: the poll timer, the editor hook, the staging buffer
    │
    ├── ref OVT_GMCampaignState m_State                              NEW  client-side store (Managed)
    │     ScriptInvoker GetOnSnapshotUpdated()                            ← siblings subscribe here
    │     ScriptInvoker GetOnStateCleared()
    │
    └── (server only) OVT_GMSnapshotBuilder                          NEW  walks managers → record arrays
          reads: OVT_OccupyingFactionManager, OVT_DeploymentManagerComponent,
                 OVT_EconomyManagerComponent, OVT_BaseControllerComponent, OVT_GMGroupRegistry

OVT_GMGroupRegistry                                                  NEW  server-only scripted singleton
    EntityID → OVT_GMGroupOrigin { originType, originIndex, reason }
    static Tag(...)   ← ~13 one-line insertions at spawn sites
    Sweep()           ← drops entries whose EntityID no longer resolves. NO untag sites.

OVT_GMSchedule                                                       NEW  PURE statics, world-free
    InGameSecondsToNextMark(h, m, s)
    RealSecondsFor(inGameSeconds, dayDurationRealSeconds)
    PredictResourceGain(basePerTick, perTick, threat, playerCount)   ← extracted from GainResources()
```

Nothing here is a Manager and nothing here is a Controller. The state is **per-player and client-side**
(a GM's view of the campaign), which is precisely what a component on `OVT_OverthrowController` is for; the
registry is server-side bookkeeping with no entity of its own, which is what a scripted singleton is for.
**No `OVT_Global` accessor is added** — the project rule is that controller components are reached through
`OVT_ControllerComponent<T>.Get()` (`Scripts/Game/Components/Controller/OVT_ControllerComponent.c:31-40`).

### 3.2 Data flow, one editor session

```
CLIENT                                            SERVER
──────                                            ──────
SCR_EditorManagerCore
  Event_OnEditorManagerInitOwner        ← hook once, per player
      │  (ArmaReforger .../SCR_EditorManagerCore.c:36)
      ├─ mgr.GetOnOpened()  ──┐              (SCR_EditorManagerEntity.c:615)
      └─ mgr.GetOnClosed()  ──┤              (SCR_EditorManagerEntity.c:645)
                              │
      editor OPENED ──────────┘
      │
      ├─ if (mgr.IsLimited()) return          ← politeness gate: a limited editor never polls
      ├─ m_iSeq++
      ├─ RequestSnapshot(CAMPAIGN, m_iSeq) ───────►  RpcAsk_Snapshot(requestType, seq)
      └─ CallLater(RequestSnapshot, poll, true)        ├─ playerId = ResolveOwningPlayerId()   ← never a param
                                                       ├─ if (playerId <= 0) return
                                                       ├─ if (!IsAuthorizedGM(playerId))        ← THE gate
                                                       │      throttled WARNING, return          (no reply at all)
                                                       ├─ builder.Build(...)                     ← read-only walk
                                                       └─ fan, in order:
      ◄──────────────────────────────────────────────  RpcDo_SnapshotBegin(seq, WIRE_VERSION)
      ◄──────────────────────────────────────────────  RpcDo_CampaignResources(seq, ...)
      ◄──────────────────────────────────────────────  RpcDo_CampaignSchedule(seq, ...)
      ◄──────────────────────────────────────────────  RpcDo_Base(seq, baseIndex, ...)         × bases
      ◄──────────────────────────────────────────────  RpcDo_BaseUpgrade(seq, baseIndex, ...)  × non-empty
      ◄──────────────────────────────────────────────  RpcDo_Deployment(seq, rplId, ...)       × deployments
      ◄──────────────────────────────────────────────  RpcDo_Group(seq, rplId, ...)            × tagged groups
      ◄──────────────────────────────────────────────  RpcDo_SnapshotEnd(seq, recordCount)
      │
      ├─ Begin: if wireVersion != WIRE_VERSION → log once, refuse to stage
      ├─ Begin: m_iStagingSeq = seq; staging arrays cleared
      ├─ every record: if (seq != m_iStagingSeq) DROP        ← the stale-discard rule
      └─ End:   if (seq == m_iStagingSeq) commit staging → m_State, stamp arrival time,
                                          GetOnSnapshotUpdated().Invoke()

      editor CLOSED
      ├─ GetGame().GetCallqueue().Remove(RequestSnapshot)     ← poll stops. No further traffic.
      ├─ m_State.Clear()
      └─ GetOnStateCleared().Invoke()
```

**On a listen-server host** the player is both client and server. Every `RpcDo_*` therefore uses the mandatory
BUG-090 short-circuit from the base class
(`Scripts/Game/Components/Controller/OVT_ControllerRequestComponent.c:105-130`) — the engine never loops an
Rpc back to the sender, so an `RplRcver.Owner` response a host sends to its own controller is silently dropped
and the host sees an empty panel. Every send site in the fan reads:

```
if (ShouldRespondLocally(playerId)) { RpcDo_Thing(a, b); return; }
Rpc(RpcDo_Thing, a, b);
```

### 3.3 What crosses the wire and what does not

**Sent** (server-only today, invisible to any client):

| Datum | Source (verified) |
|---|---|
| Campaign threat | `OVT_OccupyingFactionManager.m_iThreat` `:148` — **a `float`**; note `GetThreatLevel()` `:1129` truncates to `int`, so read the field or add a float accessor |
| OF reserve resources | `OVT_OccupyingFactionManager.m_iResources` `:147` |
| OF deployment pool | `OVT_DeploymentManagerComponent.m_mFactionResources` `Scripts/Game/GameMode/Deployments/OVT_DeploymentManager.c:39` |
| Next distribution amount + deadline | derived, §5 D5/D6 |
| Next payout amount + deadline | derived, §5 D5/D6 |
| Per-base resources / groups / upgrade count | summed over `OVT_BaseControllerComponent.m_aBaseUpgrades` (`Scripts/Game/Controllers/OccupyingFaction/OVT_BaseControllerComponent.c:17`) via `OVT_BaseUpgrade.GetResources()` (`.../BaseUpgrades/OVT_BaseUpgrade.c:39`) and `OVT_BasePatrolUpgrade.GetNumGroups()` (`:55`) |
| Per-upgrade type / resources / groups | same walk, one record per **non-empty** upgrade |
| Deployment name / faction / invested / active | `OVT_DeploymentComponent.GetDeploymentName()` `Scripts/Game/GameMode/Deployments/OVT_DeploymentComponent.c:464`, `GetControllingFaction()` `:458`, `GetResourcesInvested()` `:460` |
| Group origin / reason | `OVT_GMGroupRegistry` (new) |

**Deliberately NOT sent — already replicated to every client, so consumers read them locally:**

| Datum | Why it is already there |
|---|---|
| Resistance funds | `OVT_EconomyManagerComponent.m_iResistanceMoney` — JIP `:1893`/`:1928`, broadcast `RpcDo_SetResistanceMoney` `:2007`, change hook `m_OnResistanceMoneyChanged` `:109` |
| Town support / stability / population / faction / modifiers | `OVT_TownData` fully replicated — `OVT_TownManagerComponent` RplSave `:1324` / RplLoad `:1371`, per-field RPCs `:1526-1690` |
| Player money / XP / level / officer | `OVT_PlayerManagerComponent.RplSave :820`, `OVT_EconomyManagerComponent.RpcDo_SetPlayerMoney :1987` (Broadcast), `OVT_SkillManagerComponent :350`; level via `OVT_PlayerData.GetLevel()` |
| Base location + faction | JIP `OVT_OccupyingFactionManager.c:1526-1542`, `RpcDo_SetBaseFaction :1626` |
| Radio tower location / faction / downtime | `OVT_RadioTowerData` `:54` |
| QRF active / location / points / timer | `:1601-1658`, JIP `:1544-1547` |

Duplicating any of these would be pure waste and a second source of truth. **This table is the contract**;
a sibling that wants a town's stability calls `OVT_Global.GetTowns()` locally.

### 3.4 Join keys — how a sibling matches a record to a thing on screen

- **Groups and deployments: `RplId`.** The only entity reference that means the same thing on both machines
  (`OVT_ControllerRequestComponent.c:84-107` carries `ResolveEntity`/`GetEntityRpl` for exactly this). A GM's
  selection yields an `SCR_EditableEntityComponent`; from its owner entity's `RplComponent.Id()` the sibling
  looks the record up. Deployment **position** is therefore not sent — resolve the RplId and ask the entity.
- **Bases: positional index into `m_Bases`.** Clients already receive an index-aligned base list through JIP
  (`:1526-1542`), and this is the established Overthrow idiom — `OVT_TownManagerComponent.RplSave :1324`
  streams positionally and `RplLoad` warns on a count mismatch at `:1382`. A sibling maps `baseIndex` →
  `OVT_Global.GetOccupyingFaction().m_Bases[baseIndex].location`.
- **Towns and players: not keyed here at all** — they are locally replicated (§3.3).

---

## 4. Implementation Phases

Effort is **S / M / L** relative to one focused session. "Agent" is the routing hint for `/proceed`.

> **Phase 3 needs `component-developer-advanced`.** It inserts ~13 tagging calls across 10 files in four
> subsystems (base upgrades, QRF, occupying-faction manager, resistance manager, deployment modules), in code
> whose despawn behaviour is already leaky in three documented ways. It is the integration-heavy phase and it
> must not perturb spawn behaviour.

---

### Phase 0 — Baseline — **S — no agent**

Record, in `context.md`, before any code:

| Gate | How |
|---|---|
| `tools/compile-check.sh` | exit 0 + file count |
| Highest allocated bug id | `ls docs/bugs/` — **BUG-167** at planning time |
| Free GUID series | **`{6B07…}` is unused** (0 hits across `Prefabs`, `Configs`, `Scripts`); the controller prefab's component GUIDs run in an arithmetic series ending `{6AFB9FC82D7B61E5}` (`Prefabs/GameMode/OVT_OverthrowController.et:36`), so `{6B07B0D93E8C72F6}` continues it. **Grep-prove uniqueness before use.** |
| `git status` / highest bug id | **Re-check at every phase boundary** — parallel bugfix sessions commit into this tree mid-feature and have done so throughout this project |

**Do NOT run `tools/run-tests.sh`.** Planning and implementation stop at `compile-check.sh` exit 0; the
orchestrator runs the suites after a phase completes (`.claude/test-policy.md`). A baseline taken now would be
stale by the time this plan is implemented.

---

### Phase 1 — Pure foundations: schedule maths and a non-mutating gain predictor — **S/M — `component-developer`**

> No networking, no GM concepts. This phase exists because it is the **only** part of the feature an automated
> gate can see, and because §1 fact 1 means a read-only feature cannot be built on top of `GainResources()`.

**Tasks**

1. Create `Scripts/Game/GameMode/GM/OVT_GMSchedule.c` — a class of pure statics, **no `GetGame()`, no
   `OVT_Global`, no `BaseWorld`, not even in comments** (the Logic tier rule is enforced by a directory-wide
   grep that does not distinguish code from prose):
   - `static float InGameSecondsToNextMark(int hours, int minutes, int seconds)` — marks at 0/6/12/18.
     Returns seconds to the next mark **strictly after** now, in `(0, 21600]`; exactly on a mark returns a
     full 21600 (the tick has just fired). Document that boundary; assert it.
   - `static float RealSecondsFor(float inGameSeconds, float dayDurationRealSeconds)` —
     `inGameSeconds * dayDurationRealSeconds / 86400.0`. Guard a non-positive day duration by returning
     the in-game value unchanged rather than dividing by zero.
   - `static int PredictResourceGain(int baseResourcesPerTick, int resourcesPerTick, float threat, int playerCount)`
     — the arithmetic lifted verbatim from `OVT_OccupyingFactionManager.GainResources()`
     (`:1437-1459`), including the `threatFactor` clamp at 4 (`:1438`) and the five player-count multiplier
     bands (`:1444-1459`). **No side effects.**
2. **Refactor `GainResources()` to call it** (`OVT_OccupyingFactionManager.c:1433-1469`): the method keeps its
   `Print`s, keeps `m_iResources += newResources` (`:1461`) and keeps
   `AllocateDeploymentResourcesIfNeeded` (`:1466`) — only the arithmetic moves. Behaviour must be
   byte-identical; this is a pure extraction.
3. **Verify the payout predictors are already pure.** `OVT_EconomyManagerComponent.GetDonationIncome()`
   (`:481`) and `GetTaxIncome()` (`:499`) are believed side-effect-free over already-replicated town data.
   **Read both and confirm.** If either mutates, extract it the same way and record the finding — a
   read-only feature may not call a mutating predictor.
4. Add a float-precision threat accessor to `OVT_OccupyingFactionManager` (or use `m_iThreat` directly and
   comment why). `GetThreatLevel()` (`:1129`) declares `int` while `m_iThreat` is `float` (`:148`) — a silent
   truncation that would show the GM `3` where the campaign holds `3.87`.
5. Create `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_GMSchedule.c` and register it in
   `OVT_TEST_LogicSuite.c`. Case list in §8.

**Acceptance**

- `tools/compile-check.sh` → exit **0**, file count +2.
- Logic tier grows by the new case count; **every new case proven able to fail**, with the inversion recorded
  in `context.md`.
- No `OVT_Global`, no `GetGame()`, no world reference anywhere under `TestSuites/Logic/`.
- `GainResources()` produces the same numbers as before the extraction (read the diff; the Campaign tier's
  existing economy assertions are the regression net).

---

### Phase 2 — The seam: component, gate, framing, campaign records, poll lifecycle — **M — `network-specialist`**

> The smallest end-to-end vertical slice. When this phase lands, `overthrow-panel` is unblocked: it has
> campaign-wide numbers and two live countdowns. The heavy per-entity fan comes later and additively.

**Tasks**

1. Create `Scripts/Game/Components/Controller/OVT_GMRequestComponent.c` extending
   `OVT_ControllerRequestComponent`, with the `[ComponentEditorProps(category: "Overthrow/Components/Controller", ...)]`
   header and `OVT_GMRequestComponentClass : OVT_ControllerRequestComponentClass` pattern used by every
   sibling (e.g. `OVT_EconomyRequestComponent.c:1-2`).
2. **Add the prefab block** to `Prefabs/GameMode/OVT_OverthrowController.et` with a grep-proven-unique GUID.
   The `.et` is plain text and editable directly. A missing block fails silently — no compile error, no
   runtime error, no log line — which is why task 8 exists.
3. **The gate**, as one static on this component so Phase 2 of the epic reuses it verbatim:
   ```
   static bool IsAuthorizedGM(int playerId)   // SERVER-SIDE ONLY
   ```
   `playerId <= 0` → false; `GetGame().GetPlayerManager().HasPlayerRole(playerId, EPlayerRole.GAME_MASTER)`
   (proto: `ArmaReforger/scripts/Game/generated/Network/PlayerManager.c:62`; the role is granted/cleared by
   `SCR_EditorManagerEntity.UpdateLimited()` at `ArmaReforger/scripts/Game/Editor/Entities/SCR_EditorManagerEntity.c:587-589`)
   **OR** `SCR_Global.IsAdmin(playerId)` (`ArmaReforger/scripts/Game/Global/Functions.c:1918-1922`).
   Called at the top of **every** `RpcAsk_*` handler, after `ResolveOwningPlayerId()`, following
   `OVT_AdminCommandsComponent.c:190`.
4. **Refusal behaviour:** log one `LogLevel.WARNING` audit line naming the player id, send **nothing** back,
   and **throttle** — the component is per-player, so a single `float m_fLastRefusalLog` field naturally
   rate-limits per player and stops a spamming client flooding the server log. No notification: an
   unauthorized poll is a bug or an attack, and neither deserves feedback.
5. **The dev override, for local play-testing only:**
   `if (System.IsCLIParam("ovtGmDev")) return true;` inside `IsAuthorizedGM`, following the existing
   `-ovtDevUid` precedent (`Scripts/Game/Global/OVT_Global.c:4`, `:37`). It is read **server-side**, so a
   client cannot set it, and `tools/launch-server.sh` already forwards extra server args after `--`
   (`tools/launch-server.sh:66`, `:271`). §8 explains how the negative test is still honest.
6. **Wire format** — `const int WIRE_VERSION = 1;` and an `enum OVT_EGMRequestType { CAMPAIGN_SNAPSHOT }`
   (one value; `gm-map` adds `THREAT_GRID` later). Frame:
   | RPC | Params | Dir |
   |---|---|---|
   | `RpcAsk_Snapshot` | `int requestType, int seq` | client→server (`RplRcver.Server`) |
   | `RpcDo_SnapshotBegin` | `int seq, int wireVersion` | server→owner |
   | `RpcDo_CampaignResources` | `int seq, float threat, int ofResources, int ofDeploymentResources, int flags` | server→owner |
   | `RpcDo_CampaignSchedule` | `int seq, int distAmount, float distSeconds, int payoutAmount, float payoutSeconds` | server→owner |
   | `RpcDo_SnapshotEnd` | `int seq, int recordCount` | server→owner |
   `flags` is a bitfield carrying **why a countdown may not fire**: OF distribution is suppressed while a QRF
   runs (`OVT_OccupyingFactionManager.c:1169`) and the resistance payout is suppressed at zero players
   (`OVT_EconomyManagerComponent.c:162-165`). Without it a GM watches a countdown reach zero and nothing
   happen, and files a bug.
   **No RPC may exceed 8 parameters** — see §6.
7. **The sequence id is client-generated and echoed.** The client increments `m_iSeq` per request and the
   server passes it through untouched. That is what makes "is this record answering my current request?"
   answerable without server-side per-client bookkeeping, and it cannot collide because the component is
   per-player.
8. Create `Scripts/Game/GameMode/GM/OVT_GMCampaignState.c` — a plain `Managed` store with the campaign
   scalars, the arrays Phase 4 fills, `float m_fReceivedWorldTime`, `Clear()`, and **locally-ticking
   countdown readers**:
   `float GetDistributionSecondsRemaining()` / `GetPayoutSecondsRemaining()` subtract
   `(GetGame().GetWorld().GetWorldTime() - m_fReceivedWorldTime) / 1000` and clamp at 0. This is the exact
   pattern already proven in this codebase by `OVT_RadioTowerData.GetDisabledRemaining()`
   (`OVT_OccupyingFactionManager.c:101-111`) with its arrival stamp at `:90-94` — copy it, do not reinvent it.
9. **Invokers** on the component, lazily created: `ScriptInvoker GetOnSnapshotUpdated()` (fired on a committed
   `SnapshotEnd`) and `ScriptInvoker GetOnStateCleared()` (fired on editor close). Plus
   `OVT_GMCampaignState GetState()`. **This trio is the sibling contract** (§7 Integration).
10. **Client lifecycle.** Hook `SCR_EditorManagerCore.Event_OnEditorManagerInitOwner`
    (`ArmaReforger/scripts/Game/Editor/Core/SCR_EditorManagerCore.c:36` — "Called when an editor manager is
    initialized on owner's machine"), reached by
    `SCR_EditorManagerCore.Cast(SCR_EditorManagerCore.GetInstance(SCR_EditorManagerCore))` (precedent:
    `ArmaReforger/scripts/Game/Building/SCR_CampaignBuildingCompositionComponent.c:433`). From the delivered
    manager subscribe `GetOnOpened()` (`SCR_EditorManagerEntity.c:615`) and `GetOnClosed()` (`:645`). This
    removes any need for a retry loop.
    - On open: **if `mgr.IsLimited()` (`:529`) return without sending** — a non-GM's limited editor generates
      zero traffic. This is politeness, not security; the server gate is the boundary. Verify at runtime that
      `IsLimited()` is meaningful client-side (`UpdateLimited()` is driven from `AddMode`/`RemoveMode` at
      `:1218`/`:1236`, which run on both sides); **if it proves unreliable, poll anyway and let the server
      refuse** — record which happened.
    - On open: request immediately, then `CallLater(RequestSnapshot, m_fPollIntervalMs, true)`.
    - On close: `GetGame().GetCallqueue().Remove(RequestSnapshot)`, `m_State.Clear()`, fire
      `GetOnStateCleared()`.
11. `[Attribute]` tunables on the component: `m_fPollIntervalMs` (default **8000**),
    `m_iMaxRecordsPerSnapshot` (default **400**, Phase 4's safety valve), `m_bDebugSnapshotTiming` (0).
12. Create `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_GMRequestSeam.c`, modelled line-for-line on
    `OVT_TEST_Init_EconomyRequestSeam.c` — both claims (resolves through `OVT_ControllerComponent<T>.Get()`,
    **and** is the instance on this player's own controller entity) and the same
    poll-as-precondition-not-retry structure.

**Acceptance**

- `tools/compile-check.sh` → exit **0**.
- Init tier +1; the new case **proven able to fail** by removing the prefab block and restoring it.
- On a listen-server host with `-ovtGmDev`: opening GM produces a non-zero threat, both resource pools and two
  countdowns that visibly decrease between polls (verified by a temporary `Print`, since there is no UI yet).
- Closing GM stops the traffic: a `Print` in `RpcAsk_Snapshot` stops firing.

---

### Phase 3 — 🔴 Group origin registry and spawn-site tagging — **M/L — `component-developer-advanced`**

> **Advanced, and isolated in its own phase, because it touches four subsystems' spawn paths.** The rule for
> this phase: **behaviour must not change.** Every insertion is one statement after an existing successful
> spawn, and nothing is deleted, reordered or conditioned.

**Tasks**

1. Create `Scripts/Game/GameMode/GM/OVT_GMGroupRegistry.c`:
   - `enum OVT_EGroupOrigin { UNKNOWN, BASE_PATROL, BASE_DEFENCE, BASE_SNIPER, TOWER_GUARD, TOWN_PATROL, BASE_GARRISON, RADIO_TOWER_GARRISON, CAMP_GARRISON, FOB_GARRISON, QRF, DEPLOYMENT, JOB }`
   - `class OVT_GMGroupOrigin : Managed { int m_iType; int m_iIndex; string m_sReason; }` — `m_iIndex` is the
     base index / town id / tower id, or `-1`; `m_sReason` is the concrete upgrade `ClassName()` or the
     deployment config name.
   - Scripted singleton: `static OVT_GMGroupRegistry GetInstance()` creating on first use.
   - `static void Tag(IEntity group, int originType, int originIndex, string reason)` — a **static** so each
     call site is genuinely one line. It **no-ops unless `Replication.IsServer()`** and no-ops on a null
     group.
   - `void Sweep()` — drop every entry whose `EntityID` no longer resolves via
     `GetGame().GetWorld().FindEntityByID(id)`.
     ⚠️ **Do NOT prune on `GetAgentsCount() == 0`.** Reforger 1.8's AI spawn queue and dormancy break the
     "0 agents = dead" inference (recorded in this project's `reforger-1.8-update.md` memory, still unfixed) —
     an alive-but-queued group would be pruned and its origin lost. Entity-resolution is the only safe test.
   - `OVT_GMGroupOrigin Find(EntityID id)`, `int Count()`, `void GetAll(out array<EntityID>, out array<OVT_GMGroupOrigin>)`.
   - **Never persisted, never `[RplProp]`.** `Modded/SCR_AIGroup.c` untracks all AI groups from persistence
     (BUG-118); a saved registry would name entities that no longer exist.
2. **The tagging insertions.** All verified present at these lines; re-confirm before editing (this tree moves):

   | # | Site | Origin | Index available | Reason |
   |---|---|---|---|---|
   | 1 | `Controllers/OccupyingFaction/BaseUpgrades/OVT_BasePatrolUpgrade.c:127` (in `BuyPatrol`, sig `:101`) | BASE_PATROL / DEFENCE / etc. by subclass | `m_BaseController` | `ClassName()` — resolves to the concrete subclass, so **this one insertion covers five upgrade classes** (`OVT_BasePatrolUpgrade`, `OVT_BaseUpgradeDefensePatrol`, `OVT_BaseUpgradeSpecops`, `OVT_BaseUpgradeCheckpoints`, `OVT_SlottedBaseUpgrade`/`Composition`) **and both the initial spawn and the re-spawn from `m_ProxiedGroups`** (`:70-78` routes back through `BuyPatrol` at `:72`) |
   | 2 | `.../OVT_BaseUpgradeDefensePosition.c:99` (in `BuyGuard`, sig `:90`) | BASE_DEFENCE | `m_BaseController`, defend-position `id` | `ClassName()` |
   | 3 | `.../OVT_BaseUpgradeSniperPosition.c:151` (in `BuyTeam`, sig `:134`) | BASE_SNIPER | `m_BaseController` | `ClassName()` |
   | 4 | `.../OVT_BaseUpgradeTowerGuard.c:163` (in `BuyGuard`, sig `:131`) | TOWER_GUARD | `m_BaseController` | `ClassName()` |
   | 5 | `.../OVT_BaseUpgradeTownPatrol.c:137` (in `BuyTownPatrol`, sig `:115`) | TOWN_PATROL | `townID` (computed `:117`) | `ClassName()` |
   | 6 | `Controllers/OccupyingFaction/OVT_QRFControllerComponent.c:383` (in `SpawnFromQueue`, sig `:367`) | QRF | −1 (the owning base/town lives on the manager as `m_CurrentQRFBase`/`m_CurrentQRFTown`) | `"QRF"` |
   | 7 | `GameMode/Managers/Factions/OVT_OccupyingFactionManager.c:561` (in `CheckRadioTowers`, sig `:522`) | RADIO_TOWER_GARRISON | `tower.id` | `"RadioTower"` |
   | 8–10 | `GameMode/Managers/Factions/OVT_ResistanceFactionManager.c:951` (`AddGarrison`, sig `:924`), `:982` (`AddGarrisonCamp`, `:959`), `:1013` (`AddGarrisonFOB`, `:990`) | BASE/CAMP/FOB_GARRISON | base id / camp / FOB | method name |
   | 11–12 | `.../OVT_ResistanceFactionManager.c:425` and `:438` (in `SpawnGarrisons()`, `:413` — the boot-time restore path) | CAMP/FOB_GARRISON | as above | `"Restored"` |
   | 13 | `GameMode/Managers/Factions/OVT_OccupyingFactionManager.c:775` (in `InitBaseControllers`, sig `:729` — saved resistance base garrison) | BASE_GARRISON | base index | `"Restored"` |
   | 14–16 | `GameMode/Deployments/Modules/OVT_InfantrySpawningDeploymentModule.c:185` (initial, `SpawnInfantryGroups` `:136`) and `:379` (`Reinforce` `:318`); `.../OVT_VehicleSpawningDeploymentModule.c:228` (crew, `SpawnVehicles` `:163`) | DEPLOYMENT | −1 | `m_ParentDeployment.GetDeploymentName()` |
   | 17 | `GameMode/Systems/Jobs/Stages/OVT_SpawnGroupJobStage.c:41` (`OnStart` `:18`) | JOB | −1 | job name |

   Tag **beside the existing tracking insertion**, not at the raw spawn call — that is where the context is
   already in scope and where the code reads naturally.

3. **Deliberately NOT tagged, and why** (record in `context.md`):
   - `Controllers/OVT_TownController.c:166` — **town civilians**. They are `SCR_AIGroup` entities and could be
     numerous; a GM does not need "why does this civilian exist", and including them would dominate the record
     budget. Faction already distinguishes them.
   - `GameMode/Managers/OVT_PlayerGroupManagerComponent.c:262` — player-playable groups. Not Overthrow-spawned
     AI.
   - Recruits — not group spawns at all (`OVT_RecruitManagerComponent.c:778` joins a lone character into an
     existing group; see the explicit carve-out at `Modded/SCR_AIGroup.c:18-22`).
   - `UI/Context/OVT_BaseMenuContext.c:57` and `UI/Context/OVT_FOBMenuContext.c:42` — **client-local throwaway
     preview groups**, spawned and deleted inside `Refresh()` purely to count unit slots. The
     `Replication.IsServer()` guard in `Tag()` excludes them structurally.
4. **There are no untag sites.** See §5 D7. Do not add any.
5. **Verify groups carry an `RplComponent`.** The wire key is `GetEntityRpl(group).Id()`
   (`OVT_ControllerRequestComponent.c:102-107`). If a spawned `SCR_AIGroup` has no `RplComponent`, its record
   cannot be sent and `hud-icons` cannot match it — that is a **finding**, not something to work around: stop
   and report it, because the fallback (position-based matching) changes the epic's join-key contract.
6. Create `Scripts/Game/Tests/TestSuites/Campaign/OVT_TEST_Campaign_GMGroupRegistry.c` — Campaign tier, not
   Logic, because the registry is keyed on `EntityID` and needs a world. §8 has the assertion and the
   anti-vacuity requirement.

**Acceptance**

- `tools/compile-check.sh` → exit **0**.
- Campaign tier +1, **proven able to fail** by removing one `Tag()` call.
- **No behavioural diff in spawning**: read the diff and confirm every hunk is a pure insertion — nothing
  deleted, nothing reordered, nothing newly conditional.
- A `Print` of `registry.Count()` in a running campaign is non-zero and grows as bases spend resources.

---

### Phase 4 — The per-entity fan: bases, upgrades, deployments, groups — **M — `network-specialist`**

**Tasks**

1. Create `Scripts/Game/GameMode/GM/OVT_GMRecords.c` — four plain `Managed` records used **on both sides**
   (server fills them, the fan carries them, the client store holds them; one set of classes, not two):
   `OVT_GMBaseRecord { int m_iBaseIndex; int m_iResources; int m_iGroups; int m_iUpgrades; }`,
   `OVT_GMBaseUpgradeRecord { int m_iBaseIndex; string m_sType; int m_iResources; int m_iGroups; }`,
   `OVT_GMDeploymentRecord { RplId m_RplId; string m_sName; int m_iFaction; int m_iResourcesInvested; bool m_bActive; }`,
   `OVT_GMGroupRecord { RplId m_RplId; int m_iOriginType; int m_iOriginIndex; string m_sReason; }`.
2. Create `Scripts/Game/GameMode/GM/OVT_GMSnapshotBuilder.c` — **server-side, read-only**, returning filled
   arrays. Keeping it out of the component honours the base class's stated rule that a request component is
   "a thin seam onto a fat manager" (`OVT_ControllerRequestComponent.c:6-8`).
   - Bases: iterate `OVT_OccupyingFactionManager.m_Bases` **by index**; for each, `GetBase(data.entId)` →
     sum `OVT_BaseUpgrade.GetResources()` and `GetNumGroups()` over `m_aBaseUpgrades`
     (`OVT_BaseControllerComponent.c:17`, walk pattern at `:303`).
   - Base upgrades: one record per upgrade **only where `GetResources() > 0 || GetNumGroups() > 0`**. Empty
     upgrades are silent — this roughly halves the largest record class for free.
   - Deployments: iterate `OVT_DeploymentManagerComponent.m_aActiveDeployments` (`OVT_DeploymentManager.c:37`),
     resolve each `EntityID` to its `OVT_DeploymentComponent`, read `GetDeploymentName()` `:464`,
     `GetControllingFaction()` `:458`, `GetResourcesInvested()` `:460`, and the marker entity's `RplId`.
     ⚠️ **Verify a threat-level field exists before sending one** — the requirements mention it; the fields
     confirmed present are name/faction/invested/active. If `OVT_DeploymentConfig` carries a threat level, add
     it; if not, drop it and record that.
   - Groups: `registry.Sweep()` first, then one record per surviving entry with a resolvable `RplComponent`.
3. Add the four record RPCs to the fan (arities from §3.2), each with the `ShouldRespondLocally`
   short-circuit.
4. **Cap and budget.** Stop emitting at `m_iMaxRecordsPerSnapshot` and log a single WARNING naming the
   truncated class when it trips — a silently truncated snapshot is a bug that looks like missing data.
   Under `m_bDebugSnapshotTiming`, `Print` the per-class record counts and the build time.
5. Extend `OVT_GMCampaignState` with the four arrays, `FindGroup(RplId)`, `FindBase(int)`, and clear them in
   `Clear()`.

**Acceptance**

- `tools/compile-check.sh` → exit **0**.
- **Measured on a populated campaign** (a save with all bases upgraded and several deployments running), with
  the numbers written into `context.md`: total records per snapshot, per-class breakdown, build time in ms.
  Expected order of magnitude: ~10–15 bases, ~40–80 upgrade records after the non-empty filter, ~10–40
  deployments, ~30–80 groups → **~100–200 records per poll**. If it exceeds 400, the cap trips and §10 R3's
  mitigation is taken.
- A GM sees group origins that match reality: a base patrol reports its base, a town patrol reports its town,
  a deployment group reports the deployment's config name.

---

### Phase 5 — Verification gate — **M — user-driven, no agent**

Run §8's Verification Method end to end. This is the only evidence that exists for the parts no automated gate
can see: the gate under real roles, the fan, JIP, listen-server correctness, and the poll lifecycle.

---

### Phase 6 — The consumption contract and `context.md` — **S — `component-developer`**

**Tasks**

1. Write `docs/features/gm/gm-state/context.md`: the shipped wire table (every RPC with its exact arity), the
   measured record counts, the tagging insertion list as actually applied, every decision taken during
   implementation, and a "where to look when a GM sees nothing" triage section (prefab block → gate → editor
   hook → seq mismatch → wire version).
2. Write the **sibling consumption contract** into the same file, in enough detail that
   `overthrow-panel`/`hud-icons`/`waypoint-viz`/`gm-map` can be planned from it alone: the `GetState()` /
   `GetOnSnapshotUpdated()` / `GetOnStateCleared()` trio, the record shapes, the join keys (§3.4), the
   locally-ticking countdown readers, and — importantly — **§3.3's "already replicated, read locally" table**,
   so no sibling asks this seam for a town's stability.
3. Update `docs/features/gm/epic-overview.md`: feature 1 status and any Tech Debt / Findings surfaced (in
   particular the group-cleanup gaps in §5 D7, which are **pre-existing defects this feature routes around,
   not defects it introduces** — they belong in the epic's Findings so the `occupying` epic can see them).

**No `help-docs-sync` phase.** This feature ships **zero** player-facing or GM-facing surface — it is a
transport with no renderer. The epic's first visible change arrives with `overthrow-panel`, and that is the
feature that should carry the help/wiki phase. Record that hand-off in `epic-overview.md` so it is not lost.

**Acceptance**

- `context.md` exists and a reader with no context can answer "what does a sibling call to get a base's
  garrison count?" from it alone.
- `epic-overview.md` names the help-docs hand-off.

---

## 5. Key Technical Decisions

### D1 — Poll-while-GM-open, no delta protocol — **user decision, not re-litigated**

The GM client requests a **full** snapshot on editor open and re-requests every `m_fPollIntervalMs` (default
8 s) while it stays open; the poll stops on close.

**Rationale.** A delta protocol needs per-client server-side state (what does this client already know?),
invalidation hooks on a dozen managers, and a resync path when a delta is missed — all to save bandwidth that,
at 1–3 GMs and ~150 small records per 8 s, is not scarce. The polling design has one failure mode (a stale
snapshot) and one defence against it (the sequence id), and both are testable. YAGNI.

### D2 — The gate is `GAME_MASTER` **or** `IsAdmin`, checked server-side on every handler — **user decision**

`GetGame().GetPlayerManager().HasPlayerRole(playerId, EPlayerRole.GAME_MASTER)` OR
`SCR_Global.IsAdmin(playerId)` (`ArmaReforger/scripts/Game/Global/Functions.c:1918-1922`), following
`OVT_AdminCommandsComponent.c:190`.

**Why both.** The `GAME_MASTER` role is only granted while a player's editor is **unlimited**
(`SCR_EditorManagerEntity.UpdateLimited()`, `.../SCR_EditorManagerEntity.c:583-590`), and Overthrow's editor
is not configured yet (that configuration is the epic's **Phase 3**, explicitly out of scope). Until then, on
a real server, admins are the population that gets an unlimited editor — so `IsAdmin` is what actually makes
the feature usable today, and `HasPlayerRole` is what keeps working once Phase 3 configures the editor
properly. Neither alone is sufficient.

**Why on every handler and not once at connect.** Roles change mid-session — an admin logs in, a vote
promotes someone, an editor mode is removed. A cached authorization is a stale authorization. The check is two
proto calls; it is cheaper than the record it would guard.

**Identity is never a parameter.** `ResolveOwningPlayerId()` derives the caller from the entity the RPC
arrived on (`OVT_ControllerRequestComponent.c:36-49`), which is what makes it unspoofable.

### D3 — The component is named `OVT_GMRequestComponent`, not `OVT_GMStateComponent`

Epic constraint: Phase 2 adds write actions (give resources, adjust funds, spawn deployments) to the **same
home**. `*RequestComponent` matches the family already on the controller prefab (Economy, Vehicle, RealEstate,
FOB, Recruit, Loadout, Possession, Job, Campaign, Resistance, Respawn, Travel) and describes the role, not the
payload. The **state** is a separate `OVT_GMCampaignState` object the component owns — which also honours the
base class's stated rule that these components carry no domain state
(`OVT_ControllerRequestComponent.c:6-8`).

### D4 — No "per-entity detail on demand" request

`requirements.md` asks for per-entity detail "on demand (for a selected/hovered entity)". Under D1 the
snapshot **already contains every record**, so "on demand" is a dictionary lookup in the client store with
**zero** additional traffic and zero additional latency. A second request/response pair would be strictly
worse in every dimension. `requirements.md` line 13 is satisfied by the cache, not by an RPC.

### D5 — Deadlines are derived from `GetDayDuration()`, not `GetDayTimeMultiplier()`

`OVT_TimeAndWeatherHandlerComponent.GetDayTimeMultiplier()` exposes `m_fDayTimeAcceleration` only. The engine
runs a **different** acceleration at night and switches between them in `HandleDaytimeAcceleration`
(`ArmaReforger/scripts/Game/Components/Environment/SCR_TimeAndWeatherHandlerComponent.c:258-283`), so the day
value is wrong for half of every campaign day.

Use `86400 / TimeAndWeatherManagerEntity.GetDayDuration()` — real-seconds-per-day under the acceleration
**currently in force** (proto: `ArmaReforger/scripts/GameLib/generated/Entities/BaseWeatherManagerEntity.c:24`;
vanilla precedent for this exact conversion: `SCR_IngameClockUIComponent.c:77`).

**The accepted residual error.** If a countdown spans a day↔night switch the estimate is off by the ratio of
the two accelerations for the part that crosses. It is **self-correcting**: the client re-polls every 8 s and
the error shrinks monotonically as the deadline nears. This is why the countdown is a *server-computed real
seconds* value re-sent every poll rather than a one-shot absolute timestamp — a timestamp computed once would
keep its error forever.

**This overrides the task brief's suggestion** to use `GetDayTimeMultiplier()`.

### D6 — Both deadlines are computed from the same 6-hour marks, but reported separately with suppression flags

Both loops fire at in-game hours {0, 6, 12, 18} — OF distribution at `OVT_OccupyingFactionManager.c:1172-1177`
(requiring `m_iMinutes == 0`), resistance payout at `OVT_EconomyManagerComponent.c:170-180` (gated on
`m_iHourPaidIncome != time.m_iHours`, so it fires on the first observation of the hour). Both `CheckUpdate`s
run every `60000 / timeMultiplier` real ms (`OVT_OccupyingFactionManager.c:299`,
`OVT_EconomyManagerComponent.c:1291`), i.e. about once per in-game minute.

They are reported as **two independent countdowns plus a flags bitfield**, because their suppression
conditions differ: OF distribution is skipped entirely while a QRF is running (`:1169`), and the payout
returns early at zero connected players (`OVT_EconomyManagerComponent.c:162-165`). A GM watching a countdown
hit zero with nothing happening needs to be told which of those it was.

The **amounts** are server-computed and sent even though the payout half (`GetDonationIncome()` `:481` +
`GetTaxIncome()` `:499`) is pure over already-replicated town data and a client could recompute it. One
implementation on the server is one place to be wrong; two implementations that disagree is a bug report.

### D7 — The registry tags at spawn and **never untags** — it sweeps

**The alternative was surveyed and rejected on evidence.** There are **27** delete/drop sites, and the
existing cleanup is already leaky in ways that would become the registry's leaks:

- **QRF groups are never deleted at all.** `OVT_QRFControllerComponent.m_Groups` is filled at `:383` and
  drained nowhere; `OnQRFFinishedBase`/`OnQRFFinishedTown` delete only the controller entity
  (`OVT_OccupyingFactionManager.c:933`, `:1000`).
- **`OVT_EntitySpawningAPI.CleanupGroup` (`OVT_EntitySpawningAPI.c:379`) deletes a group's soldiers, not the
  group entity** — every deployment "despawn" leaves a live empty group behind.
- **Camp/FOB removal never touches `garrisonEntities`** (`OVT_ResistanceFactionManager.RemoveCamp :1503`,
  `CleanupCampObjects :1557`), so garrison groups outlive their camp.
- `m_Groups.Clear()` in the four base-upgrade despawn paths (`OVT_BasePatrolUpgrade.c:89`,
  `OVT_BaseUpgradeDefensePosition.c:37`, `OVT_BaseUpgradeSniperPosition.c:76`,
  `OVT_BaseUpgradeTowerGuard.c:81`) is wholesale, with no per-entry hook.
- Six resistance-garrison rollback paths (`OVT_ResistanceFactionManager.c:941/:947/:972/:978/:1003/:1009`)
  spawn a group and then delete it before it is ever tracked.

Sweeping on entity resolution handles **every one of these uniformly**, costs one map walk per snapshot build,
and cannot be forgotten at a new spawn site added by a future feature (that group simply never appears — a
missing icon, not a wrong one).

**Do not prune on agent count.** "0 agents = dead" is broken under Reforger 1.8's AI spawn queue and dormancy
(project memory `reforger-1.8-update.md`, unfixed) and would silently drop legitimately-queued groups.

**And the base-patrol despawn/respawn churn resolves itself.** `OVT_BasePatrolUpgrade.CheckUpdate` (`:60-94`)
banks prefab+position into `m_ProxiedGroups`/`m_ProxiedPositions` and deletes the entity (`:87`), then
re-spawns through `BuyPatrol` on the inverse branch (`:72`). Since insertion #1 tags **inside `BuyPatrol`**,
the re-spawned group is re-tagged automatically and the dead EntityID is swept. One insertion covers the whole
cycle.

### D8 — The registry is a scripted singleton, not a manager, and is never persisted

It has no entity, no replication, no config surface and exactly one consumer (the snapshot builder, on the
server). A manager component would cost a prefab edit on `OVT_OverthrowGameMode.et` plus an `OVT_Global`
accessor for zero benefit. It is never persisted because `Modded/SCR_AIGroup.c` untracks all AI groups from
persistence (BUG-118) — a restored registry would name entities that do not exist.

**Rejected alternative: hooking `Modded/SCR_AIGroup.EOnInit` (`SCR_AIGroup.c:29`).** It genuinely is a
universal chokepoint for group entities — but it knows nothing about **who asked** for the group, so using it
would require a static "pending origin" set immediately before each spawn: the same N call sites with worse
coupling and a race. It is also not `Replication.IsServer()`-guarded and would capture the two client-local
throwaway preview groups spawned by `OVT_BaseMenuContext.c:57` / `OVT_FOBMenuContext.c:42`.

### D9 — The threat **grid** is deferred to `gm-map`; the protocol is versioned so it can be added — **user decision**

Only the campaign-wide threat scalar ships now. `RpcAsk_Snapshot(int requestType, int seq)` carries a
request-type discriminator with one value today (`CAMPAIGN_SNAPSHOT`), and `RpcDo_SnapshotBegin` carries
`WIRE_VERSION`. `gm-map` adds `THREAT_GRID` as a second request type and its own record RPC under the same
`seq`/version framing — **additively**, with no change to any existing signature.

### D10 — Base **upgrade positions** and civilian groups are out; per-upgrade aggregates are in

`OVT_BaseUpgradeData.pos` exists (`OVT_OccupyingFactionManager.c:11`) and `gm-map` will want it for its
base-upgrade icon layer — but this feature has no renderer, so shipping positions now is speculative payload.
Per-upgrade **type / resources / groups** ships (it is what `requirements.md` line 13 asks for and what the
panel and HUD icons need); **positions** are a two-field additive extension `gm-map` makes when it needs them.

Civilian groups (`OVT_TownController.c:166`) are excluded from the registry entirely — see Phase 3 task 3.

### D11 — The client-side "am I a GM?" check is politeness; the server check is security

The client skips polling when `SCR_EditorManagerEntity.IsLimited()` (`:529`) is true, so a regular player's
editor generates no traffic at all. That is a **traffic** property, not a **security** property: a modified
client can send whatever it likes, which is exactly why `IsAuthorizedGM` runs on the server inside every
handler and the refusal path returns without sending anything.

---

## 6. Quality Bar

This is a backend/networking feature with no UI to hide behind. Four properties matter more than anything
else, and each has a Definition-of-Done criterion behind it.

**1. Wire-format discipline.**
- Every snapshot is framed `Begin … End` and every RPC in it carries the **client-generated sequence id**.
  A record whose seq does not match the staging seq is **dropped**, never merged.
- `RpcDo_SnapshotBegin` carries `WIRE_VERSION`. A client that receives a version it does not know **refuses to
  stage** and logs once. A mismatched build fails loudly instead of mis-parsing.
- **No RPC exceeds 8 parameters**, and **every `Rpc(RpcDo_X, …)` call site is arity-diffed against its
  handler by hand at review**. `Rpc()` is an untyped variadic prototype: a wrong argument count **compiles
  clean, passes every automated gate, and dies silently at the wire** (the BUG-090 family; the base class
  documents exactly this at `OVT_ControllerRequestComponent.c:118-122`, which is why it deliberately does not
  wrap `Rpc()`). This is the single highest-value code-review checkpoint in the feature — make it an explicit
  line item in the phase acceptance, not an assumption.
- No RPC carries an `array<>`. No hand-rolled bitstream: `ScriptBitWriter` constructs fine and **hard-crashes
  on first use from script** (project memory), so a packed payload cannot even be unit-tested.

**2. Server authority and zero leakage.**
- The gate runs **inside** every `RpcAsk_*` handler, after `ResolveOwningPlayerId()`, before any read of
  campaign state. Not in the public entry point, not once at connect, not on the client.
- A refused request produces **no response RPC of any kind** — not an error, not an empty snapshot. The only
  observable effect is a throttled server log line.
- **A grep is part of the DoD**: no `[RplProp]` and no `RplRcver.Broadcast` anywhere in this feature. Every
  response is `RplRcver.Owner`. A broadcast here would leak occupying-faction internals to every player in the
  session.
- **Strictly read-only**: no code path this feature adds mutates campaign state, including the prediction path
  (§1 fact 1 is the trap this rule exists for).

**3. Listen-server correctness.**
- Every owner-targeted response uses the `ShouldRespondLocally` short-circuit. A host who is the authority
  receives nothing over the wire, so without it the host — the single most common person to open GM on a
  self-hosted server — sees a permanently empty panel with no error anywhere.
- The public request entry point branches `if (Replication.IsServer()) RpcAsk_X(); else Rpc(RpcAsk_X, …);`
  (precedent `OVT_EconomyRequestComponent.c:57-67`). Without it, a host's request is an `RplRcver.Server` RPC
  marshalled by the server and delivered to nobody.
- **The host is a first-class test case, not an afterthought.** DoD Q-2 tests it explicitly.

**4. It costs nothing when nobody is looking.**
- No editor open ⇒ no request ⇒ no traffic, no builder run, no registry sweep. The entire feature is inert on
  a server with no GM connected — which is the normal state of every server, most of the time.

---

## 7. Definition of Done

Criteria an independent evaluator with no implementation context can verify.

### Functional

- **F-1** With a GM (or admin) client connected to a running campaign, opening the Game Master editor causes
  the client's `OVT_GMCampaignState` to be populated within one poll interval, containing: a non-zero threat
  value, an OF reserve resource figure, an OF deployment-pool figure, a next-distribution amount and
  countdown, and a next-payout amount and countdown.
- **F-2** Both countdowns **decrease continuously between polls** (the client ticks locally) and **re-sync on
  each poll**, with no visible jump larger than a second or two at a poll boundary.
- **F-3** The snapshot contains one record per occupying-faction base carrying resources, garrison group count
  and upgrade count; one record per **non-empty** base upgrade carrying its type, resources and group count;
  one record per active deployment carrying its config name, controlling faction, invested resources and
  active flag; and one record per tagged AI group carrying its origin type, origin index and reason.
- **F-4** Group origins are **correct, not merely present**: a base patrol names its base; a town patrol names
  its town; a deployment group names the deployment's config name; a QRF group is typed QRF; a radio-tower
  garrison names the tower.
- **F-5** A regular (non-GM, non-admin) client **receives no RPC from this feature at all** — verified by a
  temporary `Print` at the top of every `RpcDo_*` on the client, which never fires for that player, even while
  a GM on the same server is polling.
- **F-6** A regular client that is coerced into sending a request anyway (temporarily bypass the client-side
  `IsLimited()` gate in a test build) receives **no reply**, and the server logs exactly one throttled
  WARNING naming the player id.
- **F-7** Resistance funds, town support/stability/population, and player money/level are **not** carried in
  the snapshot. Verified by grep over the feature's files: no RPC parameter and no record field for any of
  them.

### Quality

- **Q-1 Stale-snapshot discard.** With the poll interval temporarily lowered to ~200 ms (so snapshots
  overlap), the client's committed state never contains a mixture of two snapshots: a `Print` of the staging
  sequence at commit time shows each commit belongs to exactly one `seq`, and records with a non-matching
  `seq` are counted as dropped rather than merged.
- **Q-2 Listen-server host works as GM.** A host (not a dedicated server) who opens GM sees the same populated
  state as a remote GM client. This is the criterion the `ShouldRespondLocally` short-circuit exists for.
- **Q-3 The poll stops on close.** Closing the editor stops `RpcAsk_Snapshot` arriving at the server
  (server-side `Print` goes quiet) and clears the client store; `GetOnStateCleared()` fires exactly once.
- **Q-4 No traffic for non-GMs.** A regular client with the editor unavailable/limited never sends a request
  (client-side `Print` in `RequestSnapshot` never fires).
- **Q-5 Wire hygiene.** Grep proves: zero `[RplProp]` in the feature's files; zero `RplRcver.Broadcast`; no
  RPC signature with more than 8 parameters; no `array<>` parameter on any RPC. Every `Rpc(RpcDo_*, …)` call
  site has been arity-diffed against its handler and the diff recorded in `context.md`.
- **Q-6 Read-only.** Grep proves no assignment to any manager field and no call to a mutating manager method
  from `OVT_GMSnapshotBuilder` or `OVT_GMRequestComponent`. In particular `GainResources()` is **not** called
  from any prediction path.
- **Q-7 Version guard.** With `WIRE_VERSION` temporarily bumped on the server only, the client refuses to
  stage, logs once, and the previous state is left untouched rather than half-overwritten.
- **Q-8 Spawn behaviour unchanged.** The Phase 3 diff is a pure set of insertions — nothing deleted,
  reordered, or newly conditional — and a campaign started after Phase 3 spawns the same groups, in the same
  places, as one started before it.

### Integration

- **I-1** The sibling contract exists in `docs/features/gm/gm-state/context.md` and names: the accessor
  (`OVT_ControllerComponent<OVT_GMRequestComponent>.Get()`), the store (`GetState()`), the two invokers
  (`GetOnSnapshotUpdated()`, `GetOnStateCleared()`), the four record shapes, the two join keys (`RplId` for
  groups/deployments, positional index for bases), the locally-ticking countdown readers, and the
  "already replicated — read locally" table.
- **I-2** No sibling needs to touch an RPC: a reader of `context.md` alone can state how
  `overthrow-panel` gets the OF resource figure and how `hud-icons` gets a selected group's origin.
- **I-3** No new `OVT_Global` accessor was added (project rule; `OVT_ControllerComponent.c:10-14`).
- **I-4** `OVT_GMRequestComponent` is present in `Prefabs/GameMode/OVT_OverthrowController.et` with a unique
  GUID, and `OVT_TEST_Init_GMRequestSeam` asserts it.
- **I-5** The pre-existing group-cleanup defects this feature routes around (§5 D7) are recorded in
  `docs/features/gm/epic-overview.md`'s Findings section, attributed to the `occupying` epic, and **not**
  presented as this feature's debt.

### Verification Method

1. **Compile.** `tools/compile-check.sh` → exit **0** at each phase boundary. Fix parsed `file:line: message`
   errors and re-run until clean.
2. **Automated suites** (run by the **orchestrator after a phase completes**, never by a planning or
   implementation agent, never mid-phase — `.claude/test-policy.md`):
   - After Phase 1: **Fast** group `{6A6E29FF47ECB840}` — the new Logic cases.
   - After Phase 2: **Fast** — the new Init seam case.
   - After Phase 3: **All** group `{6A6E2A002F53A581}` — the Campaign-tier registry case, plus the campaign
     and persistence tiers as the regression net for the spawn-path edits.
   - After Phase 4: **All**.
   A **changed** count at a boundary that is not explained by the new cases is a finding to investigate, never
   a number to update.
3. **Manual multiplayer play-test** (Phase 5). ⚠️ **Warn the user before launching** — client launches open a
   window on their desktop and can orphan.
   1. `tools/launch-server.sh -- -ovtGmDev` (the `--` passthrough is supported: `tools/launch-server.sh:66`,
      `:271`).
   2. `tools/launch-game.sh --timeout 3600 --profile OverthrowClient1 --allow-concurrent -- -client 127.0.0.1:2001`
      — **always pass the long timeout**; the 600 s default kills the client mid-test.
   3. A second client: same command with `--profile OverthrowClient2`.
   4. **Positive path (preferred, tests the real gate):** on client 1, log in as admin via the in-game admin
      login using the server's admin password (`devadmin` by default, `tools/launch-server.sh:168`), then open
      GM. If that grants `SESSION_ADMINISTRATOR` (and therefore `SCR_Global.IsAdmin`), the real gate is
      exercised end to end. **If the local session does not authenticate an admin login** — local mode
      authenticates nobody — fall back to the `-ovtGmDev` server flag from step 1, and **record in
      `context.md` which path was used**, because the two prove different things.
   5. **Negative path (always run, and run WITHOUT `-ovtGmDev`):** restart the server with no flag; on client
      2, confirm no snapshot RPC ever arrives (F-5) and that a forced request is refused with exactly one
      throttled log line (F-6).
   6. **Host path:** run a listen-server host rather than a dedicated server and repeat step 4 (Q-2).
   7. **JIP:** with a GM already polling, join client 2 into the established campaign, make it the GM, and
      confirm its first snapshot is complete and correct (JIP is the most common regression class in this
      project and is **not** covered by any suite).
   8. **Lifecycle:** close the editor and confirm the server-side request `Print` goes quiet (Q-3).
4. **Grep gates** (Q-5, Q-6, F-7, I-3) — run them and paste the output into `context.md`. They are cheap and
   they are the only check on the properties that no test can see.

---

## 8. Testing Strategy

**What the suites can genuinely see, and what they cannot.** Coverage in this project is a spine, not a
surface: **JIP/multiplayer, UI, performance and the real restart path are uncovered**. This feature is almost
entirely multiplayer, so the honest split is small and precise.

### Logic tier — `OVT_TEST_Logic_GMSchedule.c` (Phase 1)

World-free, pure. Register in `OVT_TEST_LogicSuite.c`.

| Case | Asserts |
|---|---|
| Next mark, mid-period | 03:20:00 → 9600 s (to 06:00) |
| Next mark, just past a mark | 06:00:30 → 21570 s |
| Next mark, exactly on a mark | 06:00:00 → 21600 s (documented boundary: the tick has just fired) |
| Next mark, wrap over midnight | 19:30:00 → 16200 s (to 00:00 next day) |
| Next mark, seconds precision | 11:59:59 → 1 s |
| Real-seconds conversion | 21600 in-game s at a 3600 s day → 900 real s |
| Real-seconds conversion, degenerate day duration | 0 or negative → returns the in-game value, does not divide by zero |
| Gain prediction, threat clamp | threat 9000 gives the same result as threat 4000 (the `threatFactor > 4` clamp, `OVT_OccupyingFactionManager.c:1438`) |
| Gain prediction, player bands | the five multiplier bands at their boundaries (4/5, 8/9, 16/17, 24/25, 32/33 players) — `:1444-1459` |
| Gain prediction, purity | calling it twice with identical inputs returns identical output (it holds no state) |

⚠️ The Logic-tier rule is enforced by a **directory-wide grep that does not distinguish code from prose** —
the names of Overthrow's static manager accessor and the engine's game-mode getter must not appear in the
file, **including comments**. A previous feature tripped exactly this by quoting the rule.

### Init tier — `OVT_TEST_Init_GMRequestSeam.c` (Phase 2)

Modelled on `OVT_TEST_Init_EconomyRequestSeam.c`. Two claims: the component resolves through
`OVT_ControllerComponent<OVT_GMRequestComponent>.Get()`, **and** the instance returned is the one on the local
player's own controller entity (a `Get()` that searched the wrong entity would satisfy claim 1 alone). The
controller poll is a **precondition, not a retry** — expiry is a named failure carrying the diagnosis. This is
the **only** gate that catches a missing prefab block, which produces no compile error, no runtime error and
no log line.

### Campaign tier — `OVT_TEST_Campaign_GMGroupRegistry.c` (Phase 3)

Not Logic: the registry is keyed on `EntityID` and needs a world.

- Assert `registry.Count() >= 1` in a started campaign after the occupying faction has spent resources.
- Assert **no** registered entry has `originType == UNKNOWN`.
- Assert `Sweep()` is idempotent and does not reduce the count when every entry still resolves.
- **Anti-vacuity requirement:** a count-based assertion can pass for the wrong reason and can also pass
  vacuously if it is written as "if any exist, they are valid". The failure message must print the per-base
  upgrade group counts so an empty registry is diagnosable rather than mysterious, and the case **must be
  proven able to fail** by removing one `Tag()` call and re-running. Record which call and what the failure
  said.

### Not testable — MP-only, and named honestly

The RPC fan, the gate under real engine roles, the sequence-guard discard, the version guard, the poll
lifecycle, the listen-server short-circuit and JIP are **all** MP-only. They are covered by the §7
Verification Method's manual script and by the grep gates, and by nothing else. Do not manufacture a test that
appears to cover them.

**Two things are deliberately *not* asserted by any suite**, and the reason is worth recording: (a) the gate
itself cannot be exercised without a real connected player carrying a real engine role — a mocked role is a
test of the mock; (b) `ScriptBitWriter` cannot round-trip from script, so no wire-format assertion is possible
even in principle.

---

## 9. Dependencies

### Internal — read-only, all verified present

| System | What is read | Where |
|---|---|---|
| Occupying faction | `m_iThreat`, `m_iResources`, `m_Bases`, base controllers, `GainResources` arithmetic | `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c` |
| Deployments | `m_aActiveDeployments`, `m_mFactionResources`, `OVT_DeploymentComponent` getters | `Scripts/Game/GameMode/Deployments/` |
| Economy | `GetDonationIncome()`, `GetTaxIncome()`, `CheckUpdate` schedule | `Scripts/Game/GameMode/Managers/OVT_EconomyManagerComponent.c` |
| Base upgrades | `GetResources()`, `GetNumGroups()`, `m_aBaseUpgrades` | `Scripts/Game/Controllers/OccupyingFaction/` |
| Controller seam | `OVT_ControllerRequestComponent`, `OVT_ControllerComponent<T>.Get()` | `Scripts/Game/Components/Controller/` |
| Time | `ChimeraWorld.GetTimeAndWeatherManager()`, `GetTime()`, `GetDayDuration()` | engine |

### One write-side dependency

Phase 1 **modifies** `OVT_OccupyingFactionManager.GainResources()` (a pure extraction) and Phase 3 **inserts**
~13 tagging statements into base upgrades, the QRF controller, both faction managers, two deployment modules
and one job stage. Nothing else in the codebase is edited.

### External

- **Base game only.** `EPlayerRole`, `PlayerManager.HasPlayerRole`, `SCR_Global.IsAdmin`,
  `SCR_EditorManagerCore`, `SCR_EditorManagerEntity`, `TimeAndWeatherManagerEntity`.
- **No persistence dependency.** Nothing this feature adds is saved; no serializer, no entry in
  `Configs/Systems/Persistence/Overthrow.conf`.
- **Tooling:** `tools/compile-check.sh`, `tools/launch-server.sh`, `tools/launch-game.sh`.

### Blocks

All four sibling features (`overthrow-panel`, `hud-icons`, `waypoint-viz`, `gm-map`) and epic Phase 2.

### Not a dependency

The **"Game Master is not configured with this game mode" warning fix** (`SCR_EditorSettingsEntity`,
`m_bIsUnlimitedEditorLegal`) is epic **Phase 3** and explicitly out of scope. This feature works with the
editor exactly as configured today — which is precisely why the gate includes `IsAdmin` (§5 D2).

---

## 10. Risks & Mitigation

**R1 — `Rpc()` arity mistakes compile clean and die silently at the wire.**
The highest-probability defect in this feature: ~9 send sites, each an untyped variadic call. Symptom is
indistinguishable from "the GM has no permission" or "the server didn't build a snapshot".
**Mitigation:** the §6 review checkpoint (hand arity-diff every call site, recorded in `context.md`); no RPC
over 8 parameters; a temporary `Print` at the top of every `RpcDo_*` during Phase 2 and Phase 4 bring-up so a
missing record is visible immediately rather than at play-test time.

**R2 — The gate is bypassed on a path someone adds later.**
Phase 2 of the epic will add write handlers to this same component. A write handler that forgets
`IsAuthorizedGM` is a privilege escalation, not a missing feature.
**Mitigation:** one static, named, documented, with the audit log inside the refusal branch; a comment on the
class stating the invariant in one sentence; and the invariant restated in `context.md` where the Phase 2
planner will read it.

**R3 — Record volume grows past the budget on a large populated campaign.**
~150 records per 8 s per GM is comfortable; a 40-deployment, 15-base, 120-group campaign with three GMs is
less so.
**Mitigation, in order, and only if the measurement in Phase 4 misses:** (a) the non-empty filter on upgrade
records is already applied; (b) raise `m_fPollIntervalMs`; (c) split the fan — campaign records every poll,
per-entity records every Nth poll; (d) if it is still a problem, an area-of-interest filter around the GM
camera is a **follow-up feature**, not a Phase 4 scope creep. The `m_iMaxRecordsPerSnapshot` cap with its
WARNING guarantees the failure is loud rather than silently truncated.

**R4 — `IsLimited()` turns out not to be meaningful client-side, so non-GM clients poll uselessly.**
**Mitigation:** the client gate is politeness only; the server gate is the security boundary, and it refuses
without replying. Worst case is a wasted request every 8 s from a player who never sees anything. Record the
observed behaviour in `context.md`; if it is unreliable, remove the client gate rather than leaving a check
that reads as security but is not.

**R5 — Spawned `SCR_AIGroup`s turn out to lack an `RplComponent`, so group records cannot be keyed.**
**Mitigation:** this is a Phase 3 **verification task with a stop condition**, not something to work around
inline — a position-based fallback would change the epic's join-key contract and must be a re-plan, not an
improvisation. Discovered early (Phase 3) rather than at Phase 4 integration.

**R6 — The local play-test cannot produce a real GM/admin, so the gate is never exercised for real.**
Local mode authenticates nobody.
**Mitigation:** two paths (§7 Verification Method step 4), preference stated, and the outcome **recorded**.
The negative path is run **without** `-ovtGmDev` in every case, so "a non-GM receives nothing" is always
proven against the real gate even when the positive path uses the override.

**R7 — A parallel session changes the spawn sites Phase 3 edits.**
This tree receives concurrent bugfix commits; every line number in §4 Phase 3 is a snapshot of `f47b66a1`.
**Mitigation:** re-check `git status` and re-confirm each cited line before editing, at every phase boundary.
The cited *method signatures* are the durable anchor; the line numbers are a convenience.

**R8 — The extraction in Phase 1 subtly changes `GainResources()`.**
An off-by-one in the player-count bands or a lost `Math.Floor` changes campaign balance silently.
**Mitigation:** it is a pure extraction — read the diff and confirm the arithmetic is character-identical
before the refactor's `Print`s. The Logic cases pin the bands and the clamp; the Campaign tier's existing
economy assertions are the regression net.

---

*Sibling features: `overthrow-panel`, `hud-icons`, `waypoint-viz`, `gm-map` — all consumers of §7 I-1's
contract. Epic: `docs/features/gm/epic-overview.md`.*
