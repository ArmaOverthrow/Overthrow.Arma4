# Virtualization Movement — Implementation Plan

**Status:** Ready for Review (user play-test PASSED)
**Started:** 2026-08-17
**Target Completion:** 2026-08-17
**Last Updated:** 2026-08-17 15:30 (all 4 phases + 3 play-test fixes; final gates Fast 190 / All 236; Workbench prefab check and the full §6 play-test incl. T4.5 confirmed green by the user — see context.md)

**Epic:** `virtualization` (feature #3 of 5 — see `docs/features/virtualization/epic-overview.md`)
**Requirements:** `docs/features/virtualization/movement/requirements.md` — authoritative, **amended 2026-08-17** (user decision: vehicle groups are never virtually moved; progress resume is stateless). Both amendments are recorded as [D2](#d2--infantry-only-by-construction-no-vehicle-flag-no-road-routing-user-amendment-2026-08-17) and [D3](#d3--progress-is-transient-and-re-derivable-no-serializer-user-amendment-2026-08-17).
**Consumes:** `docs/features/virtualization/core/api.md` — 🔒 **FROZEN**. §10's `movement` table is the entire surface this feature programs against. **One** additive core method is added ([§3.6](#36-the-one-additive-core-ask)); nothing existing is renamed, re-signed or re-meant.

> **Why this feature is third in the epic.** `integration` migrates real patrols onto core next. Without movement, every migrated patrol **freezes in place the moment it despawns** — the exact defect issue #100 exists to fix. Movement is the small, sharp tick strategy that has to exist before that migration is worth doing. It is also the feature that makes core's persisted `position` field mean something: `SnapshotRegistry` reads the **live group origin**, so movement's writes are what a save keeps.

---

## 1. Executive Summary

Core gives every registered group a durable, dormant `SCR_AIGroup` entity with a writable origin and a waypoint plan kept verbatim on its record. Nothing advances it. A patrol registered at the north end of its route is still at the north end of its route four campaign hours later, and materialises there when a player finally walks up.

This feature adds the one missing tick: **a server-only manager that walks dormant registered infantry groups along their own waypoint plans, in straight lines, at a fixed configurable speed.** It is deliberately the smallest thing that closes the gap:

1. **One manager, one `CallLater`, one write.** `OVT_VirtualMovementManagerComponent` on the game-mode prefab, a 2 s round-robin tick over a slice of registered handles (core's ambient-tick pattern, `SliceIndices`/`AdvanceCursor`), and `SetPosition(handle, pos)` as its only mutation of anything core owns.
2. **`IsSpawned()` is the whole gate.** A materialised group is skipped, so live AI is never fought with — and **vehicle groups, which stay materialised by design via a huge `spawnDistanceOverride`, are excluded by construction** with no vehicle flag and no road routing anywhere in the feature ([D2](#d2--infantry-only-by-construction-no-vehicle-flag-no-road-routing-user-amendment-2026-08-17)).
3. **Progress is transient and re-derivable.** Per-group leg/direction/wait state lives in a `map<int, ref OVT_VirtualMovementState>` that is built lazily and never persisted. After a load — or a restart, or a spawn cycle, or a consumer teleporting a group — the state is re-derived by **projecting the group's actual position onto its plan polyline**. Worst case a group re-walks part of one leg. There is no movement serializer and no second frozen payload ([D3](#d3--progress-is-transient-and-re-derivable-no-serializer-user-amendment-2026-08-17)).
4. **Every position written is valid.** Ground-snapped via `GetSurfaceY`, never in water (`OVT_WorldUtils.IsOceanAtPosition`), so a group that materialises mid-leg materialises somewhere a person could stand.
5. **The plan is the opt-in.** Movement advances every registered group whose plan has something to advance. An empty plan or a DEFEND-only plan means "this group belongs here" and is skipped. No new registration argument, no core field, no flag ([D10](#d10--the-plan-is-the-opt-in-there-is-no-movement-flag)).

**Two findings from planning shape the work and are built into the phases rather than discovered later:**

- **F-A — movement will move other people's test fixtures.** `OVT_TEST_PersistenceRoundTrip_VirtualGroups_SurviveSaveAndReload` registers a group with a two-point cycling PATROL plan whose second point is **150 m away** (`OVT_TEST_PersistenceRoundTripSuite.c:4587-4601`) and then asserts, many seconds later, that the restored group is within **1 m** of a position captured before the save (`:4464`). The moment movement's tick exists, that dormant group drifts and the shared **All-group gate goes red**. The fix is planned as the first task of Phase 3, not as an emergency ([D12](#d12--shared-test-fixtures-are-made-stationary-by-construction)).
- **F-B — a virtually moved group still resumes at its *first uncompleted* waypoint.** `AIGroup.CompleteWaypoint()` removes waypoints as live AI finishes them, and a group that was dormant never completed any — so on materialisation the engine's `GetCurrentWaypoint()` is plan index 0 regardless of how far movement advanced the origin. The group appears where movement put it and then walks its route from the top. This is the native handoff the epic asked for, and it is **documented as a known limitation, not engineered around** — movement is forbidden from rewriting waypoints, and the only legitimate fix would be a core change ([D11](#d11--movement-never-touches-waypoint-entities)).

Nothing in this feature replicates, persists, or has a client half. No player sees anything change until `integration` registers real patrols — which is why there is **no help/docs-sync phase here** ([§4](#4-implementation-phases)).

---

## 2. Goals

### Primary

- **G1** A dormant registered group with a movable plan advances along that plan at a fixed, configurable speed, in straight lines, without any consumer doing anything.
- **G2** A materialised group is **never** advanced. `IsSpawned()` is the only gate, which excludes always-spawned vehicle groups by construction.
- **G3** Every position movement writes is **valid**: ground-snapped and never in water, so a mid-leg materialisation puts men somewhere a person could stand.
- **G4** **Stateless resume.** No serializer, no payload, no persistence entry. Progress after a load, a restart or an external move is re-derived from `GetPosition()` + the record's plan, costing at most one leg of re-walk.
- **G5** **Flat cost.** Work per tick is bounded by the slice size regardless of how many groups are registered; per-group effective speed does **not** change when the registry grows ([D5](#d5--elapsed-time-is-measured-per-group-not-assumed-per-tick)).
- **G6** **The insertion/extraction seam works by construction.** A group registered into virtualization at its live delivery position is adopted by the next tick pass with no notification, no callback and no API — and so is a group any consumer teleports with `SetPosition` ([D9](#d9--auto-adoption-and-external-move-re-derivation-are-one-mechanism)).
- **G7** **Core is only extended.** Exactly one additive method (`GetAllHandles()`), recorded in `api.md` and dated in `core/context.md`. `git diff Scripts/Game/GameMode/Virtualization/` shows nothing else.

### Secondary

- **G8** The route/progression maths is world-free and Logic-tier tested: projection onto a polyline, stepping, plan classification, index advance, handle ordering.
- **G9** The tick's restart hygiene copies core's Phase 6 lessons verbatim (idempotent install, `s_Instance` arbiter, self-cancel, `OnDelete` removal) — the four bugs core found there are not re-introduced here.
- **G10** The feature is play-testable **without any consumer**, using core's existing `m_bDebugRegisterTestGroup` / `m_iDebugTestGroupCount` / `m_iDebugTestGroupSpawnDistance` affordances plus one debug-logging attribute of its own.

### 2.1 Quality Bar — the hard floor

This is a **correctness and cost** feature, not a feel feature. Being "done" means all four hold; any one failing sinks the phase:

| Bar | What it means concretely | How it is caught |
|---|---|---|
| **Never moves a live group** | A group with member characters in the world is never written to. Not "usually" — the gate is checked before anything else is read, and a group that materialises mid-route is dropped from the state map on the same pass. | Play-test §6 step 4; `IsSpawned()` gate is the first statement in the per-handle body |
| **Never writes an invalid position** | Every write is ground-snapped and on land. A route crossing water writes the last land sample and keeps virtual progress; the group never sits in the sea. | Play-test §6 step 6; the water rule has its own math static and Logic case |
| **Flat cost, no leaks** | Per-tick work is bounded by `m_iGroupsPerTick`; the state map holds an entry only for a dormant, actively-advancing group and is empty after everything unregisters. | Q7/Q8 §6; Init case; `m_bDebugMovementLogging` line counts |
| **Core stays frozen** | One added method, nothing renamed, no payload field, no behaviour change to any existing core path. | Q/I criteria grep on `git diff Scripts/Game/GameMode/Virtualization/` |

---

## 3. Architecture Overview

### 3.1 Division of labour

```
SERVER ONLY. No replication, no persistence, no UI, no client half, no .conf, no prefab of its own.

CORE (virtualization/core — FROZEN, api.md §10 "movement")
├─ array<int> GetAllHandles()        ← THE ONE ADDITIVE ASK (Phase 2). Only per-owner /
│                                       per-system finders exist today, and owner systems are
│                                       free-form strings movement cannot enumerate.
├─ GetRecord(handle).m_Plan          read: m_aPositions / m_aTypes / m_aParams / m_bCycle
├─ GetPosition(handle)               read: group origin; falls back to the record when the
│                                       engine has deleted the entity
├─ SetPosition(handle, position)     THE ONLY WRITE. Dormant-only is a DOC rule with no
│                                       assertion in code — movement self-enforces it
├─ IsSpawned(handle)                 THE TICK GATE
└─ (persistence) SnapshotRegistry reads the LIVE origin  ⇒  movement's writes ARE what is saved

MOVEMENT (this feature)                       Scripts/Game/GameMode/VirtualMovement/
├─ OVT_VirtualMovementManagerComponent        Manager on OVT_OverthrowGameMode
│    ├─ ONE CallLater (m_iTickIntervalMs, default 2000), installed idempotently at PostGameStart
│    ├─ round-robin slice of m_iGroupsPerTick handles (OVT_VirtualizationMath.SliceIndices /
│    │    AdvanceCursor — core's own pattern, reused, not re-implemented)
│    ├─ m_mState : map<int, ref OVT_VirtualMovementState>   TRANSIENT, lazily built, never saved
│    ├─ s_Instance + OVT_Global.GetVirtualMovement()
│    └─ OnDelete: remove the tick, clear the state map, clear s_Instance  (core Phase 6 hygiene)
├─ OVT_VirtualMovementState                   per-group progress; every field re-derivable
└─ OVT_VirtualMovementMath                    world-free statics — the Logic tier's whole subject

ENGINE / EXISTING UTILITIES (read-only)
├─ BaseWorld.GetSurfaceY(x, z)                the ground snap
└─ OVT_WorldUtils.IsOceanAtPosition(pos)      the water veto (OVT_WorldUtils.c:555)

NOT USED, DELIBERATELY
├─ RoadNetworkManager / BaseRoad              no script route query exists at all (D2)
├─ ObserversSystem / player-distance loops    proximity is core's and the engine's problem
├─ record.m_aOwnedWaypoints                   movement reads the PLAN, never the waypoint entities
└─ any serializer, .conf, ScriptInvoker, RPC  none of them is needed (D3, D13)
```

### 3.2 Manager vs Controller — decided: **Manager**

A new `OVT_VirtualMovementManagerComponent` on the game-mode prefab, following the standard Overthrow manager pair (`OVT_VirtualMovementManagerComponentClass : OVT_ComponentClass`), with `s_Instance`, a `GetInstance()` that re-resolves across worlds, and `OVT_Global.GetVirtualMovement()`. Copy `OVT_VirtualizationManagerComponent`'s shape exactly: **server guard before any allocation** in `OnPostInit`, `Init(IEntity)` from the game mode's `EOnInit`, `PostGameStart()` from `DoStartGame()`, `OnDelete` that unwinds everything.

Why not a Controller: the things being moved are **core's records**, not entities this feature owns. There is no per-entity component to hang, nothing to replicate to a client, and no per-instance persistence. It is one system-wide server-side tick over a registry that already exists — the textbook Manager case.

Why not folded into core's own tick (an option explicitly rejected, [D1](#d1--a-standalone-manager-not-a-fold-into-cores-tick)): core ships **seams, not consumers** (`core/implementation.md:50`, its G9/I5). Core's tick is the *ambient* tick with its own budget semantics; sharing it would entangle two unrelated cadences, and every movement change would reopen a frozen file.

### 3.3 The state model

```c
//! TRANSIENT. Never serialized, never replicated, never counted on. Every field is re-derivable
//! from GetPosition(handle) + GetRecord(handle).m_Plan, which is what makes the resume stateless.
class OVT_VirtualMovementState : Managed
{
    int    m_iTargetIndex;      //!< index into m_Plan.m_aPositions the group is walking toward
    int    m_iDirection;        //!< +1 / -1 — ping-pong on a non-cycling multi-point route (D8)
    float  m_fWaitRemaining;    //!< seconds left at a WAIT waypoint; 0 = not waiting
    bool   m_bStationary;       //!< latched: nothing left to advance (DEFEND reached, route ended)
    vector m_vVirtual;          //!< the unclamped straight-line accumulator (the water rule needs it)
    vector m_vLastWritten;      //!< the last position handed to SetPosition — the external-move oracle
    float  m_fLastTickMs;       //!< world time of the last touch; the per-group dt source (D5)
}
```

**Lazy build / projection resume.** On the first touch of a handle — which is also the first touch after a load, after a restart, after the group spent time materialised, and immediately after `integration` registers a delivered group — the state is derived:

1. `m_Plan` empty / null, or every type is `DEFEND` → `m_bStationary = true`, done. (Cheap, and the latch means the plan is not re-walked on later ticks.)
2. `m_vVirtual = m_vLastWritten = GetPosition(handle)`.
3. `m_iTargetIndex = OVT_VirtualMovementMath.ProjectOntoPlan(positions, cycle, position)` — the **end index of the plan leg whose segment is nearest the group's actual position**, i.e. "which leg was it walking?". A single-position plan answers 0. An off-route position (a fresh delivery 3 km from the route) answers "the nearest leg", which is exactly the sane thing to walk toward.
4. `m_iDirection = +1`. Direction is **not** recoverable by projection; a resumed ping-pong may walk the route the other way. Accepted by the requirements (worst case: re-walking part of one leg).

**Nothing else in the system holds progress.** Losing the whole map — world teardown, a spawn cycle, a `Continue` — costs one re-projection per group.

### 3.4 The tick

```
tick every m_iTickIntervalMs (default 2000)
 ├ if (s_Instance != this) → remove this CallLater and return      (core's dead-world arbiter)
 ├ virt = OVT_Global.GetVirtualization();  null → return   (core absent = nothing to advance)
 ├ handles = virt.GetAllHandles();  OVT_VirtualMovementMath.SortHandlesAscending(handles)
 │      handles are monotonic ⇒ ascending == registration order == a STABLE round-robin (D7)
 ├ if (m_mState.Count() > handles.Count()) purge state entries whose handle is gone
 ├ slice  = OVT_VirtualizationMath.SliceIndices(handles.Count(), m_iGroupsPerTick, m_iCursor)
 ├ cursor = OVT_VirtualizationMath.AdvanceCursor(m_iCursor, m_iGroupsPerTick, handles.Count())
 └ for each handle in the slice:
      ├ virt.IsSpawned(handle)?  → m_mState.Remove(handle); continue     ← THE GATE (G2)
      ├ record = virt.GetRecord(handle); no record → continue
      ├ state  = m_mState.Get(handle);  none → DeriveState(...)          ← §3.3
      ├ external move? DistanceXZ(virt.GetPosition(handle), state.m_vLastWritten) > 1 m
      │      → re-derive (this IS the insertion/extraction adoption path, D9)
      ├ dt = clamp((now - state.m_fLastTickMs) / 1000, 0, MAX_STEP_SECONDS);  state.m_fLastTickMs = now
      ├ state.m_bStationary? → continue
      ├ state.m_fWaitRemaining > 0? → drain by dt; continue              ← WAIT honoured (D6)
      ├ step  = m_fVirtualSpeedMs * dt
      ├ advance state.m_vVirtual toward positions[target] by step; arrived when
      │      remaining <= max(step, ARRIVAL_RADIUS_M) — clamped to the target, no remainder carried
      ├ arrived? apply the target waypoint's type, then pick the next index (§3.5)
      └ IsOceanAtPosition(state.m_vVirtual)?
             yes → write nothing this tick (progress still advanced — the water rule, D6)
             no  → pos = state.m_vVirtual with Y = GetSurfaceY(x, z);
                   virt.SetPosition(handle, pos);  state.m_vLastWritten = pos
```

Every branch that ends the pass still stamps `m_fLastTickMs`, so a group cannot bank dt while it is spawned, waiting or stationary and then teleport when it resumes.

### 3.5 Waypoint-type semantics while virtual

The plan is core's `OVT_VirtualWaypointPlan`: parallel `m_aPositions` / `m_aTypes` / `m_aParams`, plus `m_bCycle`. Movement reads it and never re-derives it.

| Type | While the group is dormant | Note |
|---|---|---|
| `MOVE` | Walk to the point, then advance to the next index. | The plain case. |
| `PATROL` | Identical to `MOVE`: walk to the point, advance. The `m_aParams` radius is **not** simulated. | Virtual groups do not patrol a circle — nobody can observe it and live AI does the patrolling on materialisation. |
| `WAIT` | On arrival, `m_fWaitRemaining = m_aParams[i]`; drained by dt; then advance. `<= 0` advances immediately. | **Honoured from the plan regardless of the upstream `SpawnWaitWaypoint(pos, time)` defect** (`OVT_OverthrowConfigComponent.c:503-511`, which `civilians` fixes for live AI in its Phase 3). Movement never calls that helper, so the two are independent. |
| `DEFEND` | On arrival the group is **at its post**: latch `m_bStationary` and stop advancing, permanently. | A DEFEND-only plan is stationary from the first touch and is never advanced at all. |
| `CYCLE` | Treated as end-of-route: wrap to index 0. | Core also appends an `AIWaypointCycle` entity when `m_bCycle` is set (`OVT_VirtualizationManagerComponent.c:750-755`); both routes mean the same thing to movement. |

**End of route** ([D8](#d8--cycling-plans-wrap-non-cycling-multi-point-routes-ping-pong)):

- `m_bCycle == true` → wrap to index 0 and keep walking, forever, direction always `+1`.
- `m_bCycle == false` and the plan has ≥2 points → **ping-pong**: flip `m_iDirection` at either end and walk back.
- `m_bCycle == false` and the plan has exactly 1 point → arrive and latch stationary.

### 3.6 Placement validity — the ground and water rule

Every write is `Vector(x, GetSurfaceY(x, z), z)`. The group origin is a locator, not a character, so no clearance probe, no `FindSafeSpawnPosition` (a documented trap for anything but a pedestrian-sized caller) and no `TraceBox` — the engine places members itself when the group materialises.

**Water:** the virtual accumulator advances through water; the *written* origin does not. If `OVT_WorldUtils.IsOceanAtPosition(m_vVirtual)` is true, the tick writes nothing and the group's origin stays at the last land sample — the shore. Progress keeps accruing, and the first land sample on the far side is written normally.

Chosen deliberately over "hold at the water's edge", which **deadlocks a cycling route forever** the first time a leg clips a bay. The cost of the chosen rule is that a group can cross water in a straight line as though it had a boat; the mitigations are that (a) it is unobservable by construction — the group is dormant, which is the entire premise of the feature, and (b) it can only ever *materialise* on land, which is the requirement that actually matters. See [D6](#d6--waypoint-semantics-and-the-water-rule).

### 3.7 The one additive core ask

```c
//! Every registered handle, in registry order (unordered — sort if you need stability).
//! Server-only, like every other entry point; empty array on a client or before init.
array<int> GetAllHandles();
```

Movement advances **every** registered group, and `ownerSystem` is a deliberately free-form, mod-extensible string (`api.md` §3) — there is no set of system names movement could enumerate with `FindGroupsBySystem`. This is the whole ask: one query method, no new type, no new field, no behaviour change to any existing path.

It lands with the same discipline `civilians` used for its two prune hooks:

- added to `api.md` §3 ("Queries and reclaim") **and** §10's `movement` table;
- a dated entry under **"Additive changes after the freeze"** in `core/context.md` naming `virtualization/movement` as the requester;
- `api.md`'s header "Additively extended" note extended to mention it.

Ordering is deliberately **not** core's problem: a map has no stable iteration order, and movement's round-robin needs one, so movement sorts ascending in its own world-free math ([D7](#d7--the-round-robin-order-is-movements-own-sorted-by-handle)).

### 3.8 The insertion/extraction seam contract *(design-for only — built in `integration`)*

This feature builds **no** delivery mechanics. It fixes the contract they will rely on:

> **A group registered into virtualization at its live delivery position is adopted by movement on the next tick pass, with no notification, no callback and no registration argument.** The first touch derives state by projecting the group's actual position onto its plan; a delivered group therefore starts walking its plan from wherever the vehicle left it. Extraction is the same thing in reverse: a consumer that `SetPosition`s a group, or unregisters it, needs no cooperation from movement.

Three consequences `integration` should plan against:

1. **The plan is the opt-in.** Register a garrison with an empty or DEFEND-only plan and it will never be moved. Register a patrol with MOVE/PATROL points and it patrols virtually the moment it goes dormant. There is no flag to set ([D10](#d10--the-plan-is-the-opt-in-there-is-no-movement-flag)).
2. **A moved group resumes its waypoints from the top** (finding F-B): the group materialises where movement put it, then live AI walks to plan index 0 and onward, because no waypoint was ever completed while dormant. For a cycling patrol this costs up to one lap of walking; for a single-target delivery plan it costs nothing. Movement must not rewrite waypoints, and does not ([D11](#d11--movement-never-touches-waypoint-entities)).
3. **A vehicle-borne group in transit is not virtual at all.** It stays registered with a huge `spawnDistanceOverride` and drives live; movement skips it because `IsSpawned()` is true. Nothing about that is movement's code ([D2](#d2--infantry-only-by-construction-no-vehicle-flag-no-road-routing-user-amendment-2026-08-17)).

### 3.9 Attributes and constants

| Surface | Default | Why |
|---|---|---|
| `m_fVirtualSpeedMs` | `1.5` | Infantry walking pace. The requirement's "fixed configurable speed". Doubles as the play-test accelerator — crank it to 10–20 to compress an observation window. |
| `m_iTickIntervalMs` | `2000` | Mirrors core's ambient cadence; the cost knob nobody should need to touch. |
| `m_iGroupsPerTick` | `8` | Same slice size core uses. 40 groups ⇒ each advanced once per 10 s, with dt making the effective speed identical to a group advanced every 2 s ([D5](#d5--elapsed-time-is-measured-per-group-not-assumed-per-tick)). |
| `m_bDebugMovementLogging` | `false` | The play-test instrument: one line per advance (handle, from→to, target index, step, wait, land/water verdict). Off by default; core's `m_bDebugAmbientLogging` is the precedent. |
| `MAX_STEP_SECONDS` (const) | `30` | Caps a single step after a stalled call queue or a debugger pause, so no group ever jumps hundreds of metres in one write. |
| `ARRIVAL_RADIUS_M` (const) | `10` | Arrival tolerance. Also what makes a large-dt step land *on* a waypoint instead of oscillating around it. |
| `EXTERNAL_MOVE_EPSILON_M` (const) | `1` | XZ distance above which "somebody else moved this group" is assumed. Comfortably above the ground-snap Y change (which is not compared) and floating-point noise. |

No `.conf`, no `$profile:Overthrow_Config.json` key, no config-stream change. A server operator knob is a later additive change if one is ever asked for ([D13](#d13--no-replication-no-persistence-no-config-stream-no-ui)).

---

## 4. Implementation Phases

Four phases. Phase 1 is pure world-free maths, Phase 2 is the tiny core ask, Phase 3 is the feature, Phase 4 is coverage and the seam documentation.

**No help/docs-sync phase, deliberately.** Nothing player-facing changes here: with the epic kill switch on and no consumer registering groups, only core's debug test groups move. The player-visible change ("patrols are not where you left them") arrives with `integration`, and the tutorial/Field-Manual/wiki obligation belongs to that feature. Recorded so the omission reads as a decision, not an oversight.

**Test-run policy:** `tools/compile-check.sh` runs freely. `tools/run-tests.sh` launches a real Reforger client and is run **by the orchestrator only, once, after a phase completes** — never during planning, never inside a subagent. `.claude/test-policy.md` is the rule.

---

### Phase 1 — Progression maths + Logic tier

**Agent:** `component-developer` — **standard.** Pure world-free statics in one new file plus one new test file. It touches nothing that exists and can break nothing.
**Estimate:** 3–4 h
**Suite the orchestrator runs after this phase:** **Fast** `{6A6E29FF47ECB840}`

**Tasks**

1. **T1.1** Create `Scripts/Game/GameMode/VirtualMovement/OVT_VirtualMovementMath.c`. World-free statics only — **no manager, game-mode, world, entity or registry identifier anywhere in the file**, so the Logic tier can assert it:
   - `float DistanceXZ(vector a, vector b)` — every comparison in this feature ignores Y, because the ground snap changes Y on every write.
   - `float DistancePointToSegmentXZ(vector p, vector a, vector b)` — the projection primitive; degenerate segment (`a == b`) answers the point distance.
   - `int ProjectOntoPlan(array<vector> positions, bool cycle, vector position)` — the end index of the nearest leg; `-1` for an empty array; `0` for a single point; the closing leg of a cycling plan answers `0`.
   - `bool IsStationaryPlan(array<vector> positions, array<int> types)` — true for null/empty positions and for an all-`DEFEND` plan. Ragged arrays answer **true** (a plan core would have refused must never be walked).
   - `int NextTargetIndex(int current, int count, bool cycle, inout int direction)` — wrap when cycling; flip `direction` at either end when not; `-1` when there is nowhere to go (single point, or count <= 0).
   - `float ResolveStepDistance(float speedMs, float dtSeconds, float maxStepSeconds)` — clamped, never negative, zero for a non-positive speed.
   - `vector AdvanceTowardsXZ(vector from, vector target, float step, out bool arrived)` — clamps to the target and reports arrival when the remaining distance is `<= max(step, ARRIVAL_RADIUS_M)`; Y is passed through untouched.
   - `float DrainWait(float remaining, float dtSeconds)` — never below 0.
   - `void SortHandlesAscending(array<int> handles)` — in-place insertion sort. **Hand-rolled rather than `array.Sort()`**: the tree's only `Sort()` call sites are object arrays, `array<int>` ordering semantics were not verified, and a hand-rolled sort is Logic-tier assertable.
   - `bool IsExternalMove(vector recorded, vector lastWritten, float epsilon)`.
   - Constants `ARRIVAL_RADIUS_M`, `MAX_STEP_SECONDS`, `EXTERNAL_MOVE_EPSILON_M` live here.
2. **T1.2** Create `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_VirtualMovement.c` (Fast group, `suite: OVT_TEST_LogicSuite`), modelled on `OVT_TEST_Logic_Virtualization.c`'s header/preamble style. Cases per [§7](#7-testing-strategy). Every case carries a **recorded can-fail proof** in its preamble comment; **no `maxAttempts`**.
   - ⚠ `vector.Distance` is **+1 ULP off at 1000 m and 2000 m** (project memory). No case may assert an exact distance boundary — assert with a tolerance, or at values probed to be safe.
   - ⚠ `out` **and** `owned` are reserved as local names in EnforceScript. `out` is fine as a parameter keyword (`out bool arrived`); it must never be used as a variable name.

**Acceptance criteria**

- `tools/compile-check.sh` exits **0**.
- `grep -rniE "OVT_Global|GetGame\(\)|GetInstance|Manager|World|Entity" Scripts/Game/GameMode/VirtualMovement/OVT_VirtualMovementMath.c` → **empty** (the Logic-tier rule bans these identifiers in the tier's own files, comments included; the subject class keeps itself clean so the tier can assert it).
- `grep -rn "maxAttempts" Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_VirtualMovement.c` → **empty**.
- `git status --short Scripts/Game/GameMode/Virtualization/` → **empty** (core untouched this phase).
- Fast group green with the new cases.

---

### Phase 2 — The additive core seam

**Agent:** `component-developer-advanced` — **advanced.** It edits the epic's **frozen** core and its contract documents. The change is three lines of code; the discipline around it is the phase.
**Estimate:** 1.5–2 h
**Suite the orchestrator runs after this phase:** **Fast** `{6A6E29FF47ECB840}`

**Tasks**

1. **T2.1** Add `array<int> GetAllHandles()` to `OVT_VirtualizationManagerComponent`, beside `FindGroupsBySystem` (~`:1578`). Same shape as the existing finders: allocate the result, null-guard `m_mRecords` (returns an empty array on a client or before init), iterate, return. Doc comment states plainly that the order is the registry map's and is **not** stable, and that a caller needing stability must sort.
2. **T2.2** Update `docs/features/virtualization/core/api.md`: add the signature to §3 "Queries and reclaim"; add a row to §10's `movement` table (`Iterate every registered group — owner systems are free-form strings, so FindGroupsBySystem cannot enumerate them`); extend the header's "Additively extended" note to name this second additive change.
3. **T2.3** Append a dated entry to `core/context.md` under **"Additive changes after the freeze"**, in the same shape as the 2026-08-17 `civilians` entry: requester (`virtualization/movement`, Phase 2, this plan's §3.7), what was added, why `FindGroupsBySystem` is not sufficient, and why it is not a breaking change (nothing renamed, no signature changed, no payload field touched, `CONFIG_STREAM_VERSION` unmoved).
4. **T2.4** One Init-tier case in `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c`, modelled on the existing virtualization cases (~`:3730`): register two groups through `OVT_TEST_VirtualizationFixture`, assert `GetAllHandles()` contains both and its count matches `GetGroupCount()`, unregister both (**cleanup before reporting**, the suite's own rule), assert the handles are gone. Recorded can-fail proof in the preamble.

**Acceptance criteria**

- compile **0**; Fast green.
- `git diff Scripts/Game/GameMode/Virtualization/` shows **exactly one added method and its doc comment** — no signature changed, nothing removed, no existing line touched.
- `api.md` §3 and §10 both list `GetAllHandles`; `core/context.md` carries the dated additive note naming `virtualization/movement`.
- `git diff Configs/` → **empty**.
- No behaviour change anywhere: nothing calls the new method yet.

---

### Phase 3 — The manager, the tick and the writes

**Agent:** `component-developer-advanced` — **advanced.** Wide integration surface: it writes positions into **every** registered group in the world (including other features' and other tests' fixtures), it wires a new manager into the game mode and prefab, and its restart hygiene is the thing core spent a whole phase getting right.
**Estimate:** 8–11 h
**Suite the orchestrator runs after this phase:** **All** `{6A6E2A002F53A581}` (the persistence gate is directly affected — see T3.1)

**Tasks**

1. **T3.1 — FIRST, before the tick exists: make the shared test fixtures stationary.** Finding F-A. In `Scripts/Game/Tests/TestSuites/Persistence/OVT_TEST_PersistenceRoundTripSuite.c`, `BuildPlan()` (`:4587-4601`) builds two `PATROL` points 150 m apart; the case then asserts a ±1 m position match (`:4464`) across a multi-second save/reload. Change the plan's **types** to `DEFEND` (keeping two distinct positions, two params and `m_bCycle = true`, so **every payload claim the case makes is unchanged in number and strength**) and extend the case's preamble to say why: *this fixture asserts persistence, not motion, and virtual movement advances any dormant group with a movable plan*. Then **sweep the rest of the tree** for the same shape — `grep -rn "RegisterGroup(" Scripts/Game/Tests/` — and record the verdict for each site (single-frame cases and null-plan registrations are safe; `OVT_TEST_InitSuite.c:3746` registers with `plan = null` and is safe).
2. **T3.2** Add the deliberate moved-position claim to the same round-trip case: before the save, call `SetPosition(handle, registrationPosition + Vector(0, 0, 42))` and assert the restored group comes back at **that** position, not the registration one. Timing-free (a direct write, not a tick), and it is the epic-level proof that *movement's writes are what get persisted* without a single movement-specific line in the persistence tier ([§7](#7-testing-strategy)).
3. **T3.3** Create `Scripts/Game/GameMode/VirtualMovement/OVT_VirtualMovementState.c` — the class of §3.3, plain `Managed`, no attributes, no serialization.
4. **T3.4** Create `Scripts/Game/GameMode/VirtualMovement/OVT_VirtualMovementManagerComponent.c`: the class pair, the attributes of §3.9, `s_Instance` + `GetInstance()` that **re-resolves when `s_Instance.GetOwner()` is not the current game-mode entity** (core `:151-167`), `OnPostInit` with the **server guard before allocating the state map**, `Init(IEntity)`, `PostGameStart()` installing the tick idempotently (`m_bTickRunning` latch, core `:2769-2783`), and `OnDelete` that removes the `CallLater` from the game's call queue, clears the state map and clears `s_Instance` (core `:267-322`).
5. **T3.5** Implement the tick exactly as §3.4: dead-world self-cancel first (`s_Instance != this` → remove and return, core `:2797-2814`), enumerate + sort + purge, slice via `OVT_VirtualizationMath.SliceIndices` / `AdvanceCursor`, then the per-handle body with the `IsSpawned` gate **first**.
6. **T3.6** Implement `DeriveState` (§3.3) and the external-move re-derivation (§3.4), the two halves of the same mechanism. The stationary latch, the wait drain, the arrival handling and the type semantics of §3.5 all live here.
7. **T3.7** Implement the write: ground snap via `GetGame().GetWorld().GetSurfaceY()`, water veto via `OVT_WorldUtils.IsOceanAtPosition()`, then `SetPosition`. **`SetPosition` is the only core mutation in the feature** — assert that by grep in the acceptance criteria.
8. **T3.8** Implement `m_bDebugMovementLogging`: one line per advance — handle, from → to, target index, step metres, wait remaining, `land`/`water`, and `adopted`/`resumed` when a state was derived. This is the play-test instrument of §6; off by default, and (unlike core's VERBOSE lifecycle chatter) printed at `NORMAL` only because the attribute is explicitly switched on.
9. **T3.9** Wire it up: `OVT_Global.GetVirtualMovement()` (`Scripts/Game/Global/OVT_Global.c`, beside `GetVirtualization()` at `:252`); the field + `EOnInit` `FindComponent`/`Init(this)` block **after** Virtualization's (`OVT_OverthrowGameMode.c:1457-1462`) and the `PostGameStart()` call after Virtualization's (`:356-360`); text-wire the component onto `Prefabs/GameMode/OVT_OverthrowGameMode.et` with a fresh repo-unique GUID. **Flag the Workbench verification as a user task** (core's T2.8 precedent: text-wired by an agent, opened by the user later).
10. **T3.10** Seed `docs/features/virtualization/movement/context.md`: findings F-A and F-B, the seam contract of §3.8, the play-test list of §6, and the note that the state map's contents are re-derivable by design so a "lost progress" bug report is expected behaviour, not a defect.

**Acceptance criteria**

- compile **0**; **Fast and All** green with the epic kill switch still **on** and every `OVT-VIRT-PLAYTEST-ONLY` guard intact (`grep -rn "OVT-VIRT-PLAYTEST-ONLY" Scripts/` returns exactly what it returned before this feature — `civilians` may have removed one guard line concurrently, and this feature removes none).
- `grep -rn "Rpc\|RplProp\|Replication.Bump\|RplSave\|RplLoad" Scripts/Game/GameMode/VirtualMovement/` → **empty**.
- `grep -rn "PlayerInRange\|NearestPlayer\|ObserversSystem\|GetPlayers" Scripts/Game/GameMode/VirtualMovement/` → **empty** (proximity is not this feature's business).
- `grep -rn "CallLater" Scripts/Game/GameMode/VirtualMovement/` → **exactly one** install site, plus its `Remove` in `OnDelete`.
- `grep -rn "virt\.\|GetVirtualization()\." Scripts/Game/GameMode/VirtualMovement/` shows **only** `GetAllHandles`, `GetRecord`, `GetPosition`, `SetPosition`, `IsSpawned` — the frozen §10 surface, and `SetPosition` is the only writer.
- `grep -rn "m_aOwnedWaypoints\|AIWaypoint\|CompleteWaypoint\|SCR_AIGroup" Scripts/Game/GameMode/VirtualMovement/` → **empty** (D11: movement reads the plan, never the waypoint entities, and never touches group lifecycle).
- `git diff Scripts/Game/GameMode/Virtualization/` → still only Phase 2's one method.
- `git diff Configs/Systems/Persistence/Overthrow.conf` → **empty**.
- Play-test with `m_bDebugRegisterTestGroup = true`, `m_iDebugTestGroupSpawnDistance = 300`, `m_bDebugMovementLogging = true` and `m_fVirtualSpeedMs = 10`: the debug group's origin walks its 150 m leg, the log shows the leg transition and the cycle wrap, and walking into the ring materialises the men **where the log last put them**.

---

### Phase 4 — Init-tier coverage, seam documentation, scale check

**Agent:** `component-developer` — standard. Test cases in an existing suite plus documentation; no new production behaviour.
**Estimate:** 3–4 h
**Suite the orchestrator runs after this phase:** **Fast** `{6A6E29FF47ECB840}` (and **All** if any case is added to a persistence-tier file)

**Tasks**

1. **T4.1** Init-tier case — **the tick actually advances a dormant group.** Register a group at `OVT_TEST_VirtualizationFixture.PickPosition()` with a two-point plan whose second point is ~200 m away, wait a bounded number of frames/seconds (the round-trip suite's poll pattern; ~6 s is 3 ticks at the default cadence), and assert the position moved **toward** the target by more than 1 m and less than the straight-line distance. Unregister in a cleanup step **before** reporting, so a red assertion cannot leak a moving group into later cases. Tolerance-based, never an exact boundary.
2. **T4.2** Init-tier case — **a stationary plan is never advanced.** Same shape with a DEFEND-only plan; assert the XZ position is unchanged (within 0.5 m) over the same window. This is the [D10](#d10--the-plan-is-the-opt-in-there-is-no-movement-flag) opt-in contract asserted.
3. **T4.3** Init-tier case — **the manager resolves and is server-side**: `OVT_Global.GetVirtualMovement()` is non-null, and its state map is empty when no registered group has a movable plan (the no-leak claim, asserted through a small read-only accessor such as `GetTrackedCount()` — a diagnostic getter, not an API).
4. **T4.4** Extend `docs/features/virtualization/movement/context.md` with the Phase 3 play-test results, and add a short **"for `integration`"** section restating §3.8's three consequences verbatim, so the next feature's planner does not have to read this whole document.
5. **T4.5** Scale check (documented, not automated): with `m_iDebugTestGroupCount = 40` and `m_bDebugMovementLogging` on, confirm every handle appears in the log within one full round-robin period (40 groups / 8 per tick × 2 s = 10 s) and that no group's per-advance step differs materially from `speed × 10 s` — i.e. the round-robin does **not** change effective speed ([D5](#d5--elapsed-time-is-measured-per-group-not-assumed-per-tick)). Record the numbers in `context.md`.
6. **T4.6** Update `docs/features/virtualization/epic-overview.md`'s feature table row for `movement` (status, one-line summary) and note in the `integration` row that the auto-adoption seam is now live.

**Acceptance criteria**

- compile **0**; Fast green (All if a persistence-tier file was touched).
- Every new case carries a recorded can-fail proof; **no `maxAttempts`**.
- `context.md` carries F-A, F-B, the seam contract and the scale numbers.
- No production file changed in this phase except the diagnostic getter of T4.3.

---

## 5. Key Technical Decisions

### D1 — A standalone Manager, not a fold into core's tick

*(user-approved 2026-08-17)*

`OVT_VirtualMovementManagerComponent` on the game-mode prefab, with its own `CallLater`. Folding the advance into `OVT_VirtualizationManagerComponent.AmbientTick` was considered and **rejected**: core's rule is that it ships **seams, not consumers** (`core/implementation.md:50`, its G9/I5); the ambient tick has its own budget semantics (`m_iAmbientSourcesPerTick`, `m_iAmbientSpawnsPerTick`) that have nothing to do with a movement cadence; and every later movement change would mean reopening a frozen file and re-arguing the freeze. The cost of a separate manager is one component, one `s_Instance` and one teardown — all of which core has already written the template for.

### D2 — Infantry-only by construction: no vehicle flag, no road routing

*(user amendment, 2026-08-17)*

Virtual **vehicle** movement is out of scope, and no code in this feature knows what a vehicle is. Two independent reasons:

1. **The engine exposes no script route query at all.** The complete road surface is `RoadNetworkManager.GetClosestRoad` / `GetRoadsInAABB` / `GetReachableWaypointInRoad` (`scripts/Game/generated/RoadNetwork/RoadNetworkManager.c:14-16`) plus `BaseRoad.GetWidth` / `GetPoints` (`:18-20`) — no identity, no type, no connectivity, and **no point-to-point path query anywhere**, navmesh included (`AIPathfindingComponent` exists only on live agents). Road-following would have meant hand-building a road graph and an A* in EnforceScript.
2. **The campaign wants live vehicle transit anyway.** Insertion/extraction means a live vehicle delivers a group and *then* the group enters virtualization, so vehicle movement is spawned by design.

The mechanism is therefore nothing: vehicle groups register with a huge `spawnDistanceOverride` (supported by core today), stay materialised, and are skipped because `IsSpawned()` is true. **No vehicle/infantry flag exists to get wrong.**

### D3 — Progress is transient and re-derivable; no serializer

*(user amendment, 2026-08-17)*

There is no movement serializer, no payload and no entry in `Configs/Systems/Persistence/Overthrow.conf`. Core already persists the live group origin (`api.md` §8: `position` is read from the **group entity origin at save time**, precisely because movement advances dormant groups), so after a load the group is in the right place and movement re-derives *which leg it was on* by projecting that position onto the plan polyline.

The rejected alternative — a `OVT_VirtualMovementSerializer` carrying leg index, direction and wait remaining — would buy exact resumption of an ambient patrol's leg and direction, which no player can perceive, at the price of a **second frozen append-only payload** in an epic that already has one, plus its own version bump discipline and round-trip test. Worst case under the chosen design is a group re-walking part of one leg, or walking a ping-pong route in the other direction. Accepted explicitly in the requirements.

The property is stronger than "survives a save": because *every* state field is re-derivable, losing the map for **any** reason — restart, world teardown, a spawn cycle, an external teleport — is self-healing.

### D4 — The runtime model is "walk toward the current target index"; projection is only the resume heuristic

Progress could have been modelled as a scalar distance along a polyline. It is instead modelled as *"which waypoint index am I heading for"*, with the polyline projection used **only** when state has to be built from nothing. That handles four cases with one rule: a single-point plan (no segments to interpolate along), a group delivered far off its route, a group a consumer teleported, and a group that just came back from being live somewhere unexpected. The projection itself stays a pure function of `(positions, cycle, position)` and lives in the Logic tier.

### D5 — Elapsed time is measured per group, not assumed per tick

Round-robin slicing means a group is touched every `ceil(count / slice) × interval` milliseconds, which changes as the registry grows. If the step were `speed × interval`, a campaign with 40 registered groups would move every group **five times slower** than a campaign with 8 — a silent, scale-dependent behaviour change. So each state carries `m_fLastTickMs` (`GetGame().GetWorld().GetWorldTime()`, the tree's idiom) and the step is `speed × (now - last)`, clamped into `[0, MAX_STEP_SECONDS]`. A negative delta (a world-time reset) clamps to 0 rather than throwing the group backwards. Every early-exit branch still stamps the timestamp, so time is never banked while a group is spawned, waiting or stationary.

### D6 — Waypoint semantics and the water rule

§3.5 is the type table; the two decisions inside it worth naming:

- **`WAIT` is honoured from `m_aParams`, from the plan, unconditionally.** The upstream defect where `SpawnWaitWaypoint(pos, time)` drops its duration (`OVT_OverthrowConfigComponent.c:503-511`) is a *live-AI* defect that `civilians` fixes in its own Phase 3. Movement never calls that helper — it reads the plan — so the two are independent and neither blocks the other.
- **Water: advance the virtual accumulator, write only land.** The alternative, holding at the water's edge, **permanently deadlocks** any cycling route with a leg that clips a bay: the group can never reach its target, so it never advances its index, so it waits at that shore for the rest of the campaign. The chosen rule keeps the world moving and guarantees the only observable — where the group materialises — is always valid land. It buys that with an unobservable straight-line water crossing, which is acceptable precisely because the group is dormant by definition while it happens.

### D7 — The round-robin order is movement's own, sorted by handle

Core's `GetAllHandles()` iterates a map, and a map has no stable iteration order — core's own ambient tick keeps a separate order array for exactly this reason (`OVT_VirtualizationManagerComponent.c:113-115`: *"a round-robin over an unstable order can starve a source forever"*). Rather than make core maintain a second ordered structure, movement sorts the returned handles ascending in its own world-free math. Handles come from a monotonic counter (`api.md` §3: never reused across a reload), so ascending order **is** registration order: stable, deterministic, and Logic-tier assertable. The sort is hand-rolled rather than `array.Sort()` because the tree's only `Sort()` call sites are object arrays and `array<int>` ordering was not verified.

### D8 — Cycling plans wrap; non-cycling multi-point routes ping-pong

`m_bCycle` is the loop rule core already persists, so cycling plans wrap to index 0 forever. For a **non-cycling** plan with two or more points the choice was between stopping at the last waypoint (mirroring what live AI does with the same waypoint list) and ping-ponging. **Ping-pong wins**: a patrol that reaches the end of its route and then freezes for the rest of the campaign is the exact frozen-world defect this feature exists to remove, and there is no observable contradiction with live behaviour, because a player who watches the group live sees live behaviour and a player who was away only ever sees *where it ended up*. The cost is one `int` of state and the accepted resume ambiguity of D3 (direction is not recoverable by projection). A single-point non-cycling plan arrives and latches stationary — there is nothing to ping-pong between.

### D9 — Auto-adoption and external-move re-derivation are one mechanism

Each touch compares `GetPosition(handle)` against the state's `m_vLastWritten` in XZ; a gap over `EXTERNAL_MOVE_EPSILON_M` drops the state so the next lines re-derive it. That single comparison covers three cases that would otherwise each need their own machinery: `integration` registering a group at a live delivery position (a handle with no state at all — the same code path), a consumer calling `SetPosition` itself, and a group handing back from live AI at a position movement never chose. **No invoker, no notification API, no registration argument** — which is what makes the seam contract of §3.8 something the plan can promise rather than hope for.

### D10 — The plan **is** the opt-in; there is no movement flag

Movement advances every registered group whose plan has something to advance. A consumer that wants a group to stay put registers an empty plan or a DEFEND plan — both of which are already legal, already persisted, and already exactly what a garrison registration looks like. The rejected alternative was an opt-in field on `OVT_VirtualGroupRecord`, which would mean a **payload change** to a frozen append-only serializer, a version bump and a new registration argument, to express something the plan already expresses. This is the contract `integration` programs against, so it is stated in §3.8 as well as here.

### D11 — Movement never touches waypoint entities

`api.md` §10 permits movement to read `m_aOwnedWaypoints`. It does not need to, and does not: it reads `m_Plan` (kept on the record verbatim and persisted, `OVT_VirtualGroupRecord.c:71`) and nothing else. The acceptance grep for `AIWaypoint` in the feature directory is empty by design, which is a narrower surface than the frozen contract even allows.

The consequence is finding **F-B**, documented rather than engineered around: a virtually moved group materialises where movement put it and then walks its route **from plan index 0**, because `CompleteWaypoint()` only ever removes waypoints that live AI finished and a dormant group finished none. Cost: up to one lap of walking for a cycling patrol, nothing for a single-target plan. If it ever proves unacceptable, the fix is a **core** change (re-order or rebuild owned waypoints at materialisation), requested through `api.md`'s additive process — never a waypoint rewrite bolted onto movement.

### D12 — Shared test fixtures are made stationary by construction

Finding **F-A**. Movement moves every registered dormant group in the world, including the ones test suites register. `OVT_TEST_PersistenceRoundTrip_VirtualGroups_SurviveSaveAndReload` is the one at-risk fixture: a two-point `PATROL` plan 150 m long (`:4587-4601`) and a ±1 m position assertion across a multi-second save/reload (`:4464`). Changing the fixture's waypoint **types** to `DEFEND` makes it stationary under movement while leaving every payload claim identical (two distinct positions, two types, two params, `m_bCycle = true` all still round-trip). The rejected alternatives were widening the tolerance (turns a precise claim into a timing lottery) and teaching movement to skip a test owner system (test-aware production code). T3.1 also requires a sweep of every other `RegisterGroup(` call site under `Scripts/Game/Tests/` with a recorded verdict.

### D13 — No replication, no persistence, no config stream, no UI

Nothing in this feature replicates (the epic rule), nothing is persisted (D3), `CONFIG_STREAM_VERSION` is not touched and `$profile:Overthrow_Config.json` gains no key. Speed and cadence are **prefab attributes**, which is what "fixed configurable speed" asks for and what a server operator can already edit in Workbench. A JSON knob is a later additive change if anyone asks; adding one now would be a config-surface change with no requester. `Rpc()` arity is a compile-check blind spot in this tree — the safest answer is a feature with zero remote calls, which is what the acceptance grep enforces.

---

## 6. Definition of Done

An evaluator with **no implementation context** should be able to verify every item below.

### Functional Criteria

- **F1** With core's `m_bDebugRegisterTestGroup` on and `m_bDebugMovementLogging` on, a dormant debug group's position advances along its two-point cycling patrol: the log shows repeated advances, a leg transition on arrival, and a wrap back to the first point. With no player near it, nothing materialises.
- **F2** Approaching the moved group materialises its members **where movement last put it**, on the ground — not at the position it was registered at.
- **F3** A registered group with **no plan** and a group with a **DEFEND-only** plan never move, ever, and produce no advance log lines.
- **F4** A **materialised** group is never advanced: while members exist, the log shows no advance lines for that handle; when it despawns again, movement resumes from wherever the live AI left it (an `adopted`/`resumed` log line), not from where movement last had it.
- **F5** A plan containing a `WAIT` waypoint with a 60 s parameter pauses the group's advance for ~60 s at that point, then continues.
- **F6** A route whose straight line crosses water never leaves the group in the sea: its written position holds at the shore while progress continues, and it reappears advancing on the far side. Every materialisation is on land.
- **F7** Save → quit → **Continue** with a moved group: it comes back where it was, and movement resumes advancing it within one tick — with **no** movement records in the save (no new serializer, no `Overthrow.conf` entry).
- **F8** **Auto-adoption:** a group registered at an arbitrary position (not on its plan) begins advancing toward its nearest plan leg on the next tick pass, with no other call. Equivalently, a group teleported with `SetPosition` resumes from its new position rather than jumping back.
- **F9** **Scale:** with `m_iDebugTestGroupCount = 40`, every handle is advanced once per full round-robin period (~10 s at defaults), each group's per-advance step matches `speed × elapsed`, and there is no visible server hitch.

### Quality Criteria

- **Q1** `tools/compile-check.sh` exits **0** with no output.
- **Q2** Fast `{6A6E29FF47ECB840}` and All `{6A6E2A002F53A581}` both exit **0**, with the epic kill switch still **on** and every `OVT-VIRT-PLAYTEST-ONLY` guard intact — this feature removes none.
- **Q3** Every new test case has a recorded proof that it can fail — the exact edit, in a preamble comment. **No `maxAttempts` anywhere.**
- **Q4 Nothing replicates:** `grep -rn "Rpc\|RplProp\|Replication.Bump\|RplSave\|RplLoad" Scripts/Game/GameMode/VirtualMovement/` → empty.
- **Q5 Nothing persists:** no serializer file, and `git diff Configs/Systems/Persistence/Overthrow.conf` → empty.
- **Q6 One tick, correctly torn down:** exactly one `CallLater` install site in the feature, guarded by an idempotence latch; a matching `Remove` in `OnDelete`; a `s_Instance != this` self-cancel at the top of the tick body; `s_Instance` cleared in `OnDelete`.
- **Q7 No leaks:** the state map holds entries only for dormant groups with movable plans. After every registered group is unregistered, the tracked count returns to **0** (T4.3's Init case).
- **Q8 No per-frame cost growth:** per-tick work is bounded by `m_iGroupsPerTick`. The only whole-registry operations per tick are the handle enumeration, the sort and the (count-guarded) purge; no per-group world query, no entity iteration, no proximity check.
- **Q9 Logic-tier grep clean:** no manager-accessor, game-mode-getter, world or entity identifier under `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_VirtualMovement.c` **or** in `OVT_VirtualMovementMath.c`, comments included.
- **Q10 Narrow core surface:** `grep` of the feature directory shows only `GetAllHandles`, `GetRecord`, `GetPosition`, `SetPosition`, `IsSpawned` against the virtualization manager, with `SetPosition` the only writer; `grep -rn "AIWaypoint\|SCR_AIGroup\|CompleteWaypoint" Scripts/Game/GameMode/VirtualMovement/` → empty.

### Integration Criteria

- **I1 Core is only extended, never changed.** `git diff Scripts/Game/GameMode/Virtualization/` shows **exactly one added method** (`GetAllHandles`) and its doc comment. `api.md` lists it in §3 and §10; `core/context.md` carries the dated additive note naming `virtualization/movement`.
- **I2 `civilians` is unaffected.** `git diff Scripts/Game/GameMode/Civilians/` and `git diff Configs/Civilians/` are empty. Ambient sources are not registered groups; nothing in this feature names an ambient identifier.
- **I3 Game mode.** One field, one `EOnInit` block and one `PostGameStart` call, all placed **after** Virtualization's, in the existing style; one component added to `Prefabs/GameMode/OVT_OverthrowGameMode.et` and nothing removed.
- **I4 Test fixtures.** The persistence round-trip virtual-group case still makes the same number of payload claims at the same strength, plus one new claim (a deliberately moved position round-trips). Its plan is stationary by construction, with the reason in its preamble.
- **I5 No config or stream change.** `OVT_OverthrowConfigStruct`, `RplSave`/`RplLoad` and `CONFIG_STREAM_VERSION` are untouched; `git diff Configs/` shows nothing but (optionally) nothing at all.

### Verification Method

**Automated — from the repo root, in order:**

1. `tools/compile-check.sh` → exit **0**.
2. `tools/run-tests.sh "{6A6E29FF47ECB840}"` → exit **0** (Fast).
3. `tools/run-tests.sh "{6A6E2A002F53A581}"` → exit **0** (All).
4. `git diff Scripts/Game/GameMode/Virtualization/` → one added method only. → I1
5. `grep -rn "Rpc\|RplProp\|RplSave\|RplLoad\|PlayerInRange\|NearestPlayer\|ObserversSystem" Scripts/Game/GameMode/VirtualMovement/` → **empty**. → Q4, Q8
6. `grep -rn "AIWaypoint\|SCR_AIGroup\|CompleteWaypoint\|m_aOwnedWaypoints" Scripts/Game/GameMode/VirtualMovement/` → **empty**. → Q10
7. `grep -rn "CallLater" Scripts/Game/GameMode/VirtualMovement/` → one install, one remove. → Q6
8. `git diff Configs/Systems/Persistence/Overthrow.conf` → **empty**. → Q5
9. `grep -rn "OVT-VIRT-PLAYTEST-ONLY" Scripts/ | wc -l` → unchanged from before the feature. → Q2

**Manual — solo play-test.** On the game-mode prefab set core's `m_bDebugRegisterTestGroup = true`, `m_iDebugTestGroupSpawnDistance = 300` (so a GM camera outside ~345 m cannot hold the group awake) and movement's `m_bDebugMovementLogging = true`. The epic kill switch stays **on**. For steps 1–6 set `m_fVirtualSpeedMs = 10` to compress the window; reset to the default before step 9.

1. Start a campaign, stay away from the debug group. **Expect:** repeated advance log lines; no members anywhere. → F1
2. Watch the log across a full leg. **Expect:** the step distance matches `speed × elapsed`, a leg transition on arrival, and a wrap back to point 0 for a cycling plan. → F1, F9
3. Note the group's last logged position, then walk/fly to it. **Expect:** members materialise **there**, standing on the ground. → F2
4. Stay inside the ring for a minute. **Expect:** **no** advance lines for that handle while it is spawned. Walk out past the despawn ring and wait. **Expect:** an `adopted`/`resumed` line, then advances continuing **from where the AI stood**, not from where movement last had it. → F4
5. Register (or temporarily author) a plan with a `WAIT` waypoint at 60 s. **Expect:** the advance stops for ~60 s at that point and then resumes. → F5
6. Point a debug leg across a bay (offset the second waypoint over water). **Expect:** the written position stops at the shore while the log keeps advancing, then reappears on the far side; at no point does approaching the group materialise it in the sea. → F6
7. Save, quit to menu, **Continue**. **Expect:** the group is where it was, and advancing again within a couple of seconds. Decode/inspect the save: **no** movement records. → F7
8. With a second registered group placed deliberately off its route (a delivery simulation), watch the first pass. **Expect:** it starts walking toward its nearest plan leg without any other action. → F8
9. Set `m_iDebugTestGroupCount = 40`, restart the campaign, and watch one full round-robin period. **Expect:** all 40 handles logged within ~10 s, each with a step matching `speed × elapsed`; no hitch. → F9, Q8
10. Start a second campaign in the same session without restarting the client. **Expect:** exactly one advance line per group per period — **not two** (the dead-world self-cancel of Q6 working). → Q6

---

## 7. Testing Strategy

**The automated spine covers the maths, the core seam and the fact that the tick moves a dormant group. Everything about placement quality, handoff feel and multi-campaign hygiene is a play-test — the suites cannot see a group standing in the sea or a second tick running from a dead world.**

### Logic tier — `TestSuites/Logic/OVT_TEST_Logic_VirtualMovement.c` (Fast)

World-free assertions on `OVT_VirtualMovementMath`, which is the whole reason the maths lives in a static rather than on the manager:

- **Projection:** a position on a leg answers that leg's end index; a position nearer a later leg answers that one; an off-route position answers the nearest leg; a single-point plan answers `0`; an empty plan answers `-1`; the closing leg of a cycling plan answers `0`.
- **Plan classification:** empty, null, all-`DEFEND` and ragged plans are stationary; a `MOVE`/`PATROL` plan is not; a mixed plan with one non-`DEFEND` point is not.
- **Index advance:** wrapping when cycling; direction flip at both ends when not; `-1` for a single-point plan; both directions asserted, not just the forward one.
- **Stepping:** `ResolveStepDistance` clamps at `MAX_STEP_SECONDS`, floors at 0 for a negative or zero dt, and returns 0 for a non-positive speed; `AdvanceTowardsXZ` clamps to the target and reports arrival rather than overshooting; a step longer than the remaining distance lands **on** the target.
- **Wait drain:** never below 0; a zero-duration wait is already finished.
- **Ordering:** `SortHandlesAscending` on an empty array, a single element, an already-sorted array and a reversed one.
- **External-move predicate:** a pure Y difference (the ground snap) is **not** an external move; a 5 m XZ difference is.

⚠ `vector.Distance` is +1 ULP off at 1000 m and 2000 m — no case asserts an exact distance boundary; every distance claim carries a tolerance or is taken at a probed-safe magnitude.

### Init tier — additions to `TestSuites/Init/OVT_TEST_InitSuite.c` (Fast)

Modelled on the existing virtualization cases (~`:3730`), which the Phase 1 T1.4 verdict established are safe: in the Fast world nothing materialises, so a registered group stays dormant for the whole case.

- `GetAllHandles()` returns every registered handle and drops them on unregister (Phase 2's gate).
- **The tick advances a dormant group with a movable plan** toward its target over a bounded wait — the one end-to-end claim that proves enumeration, slicing, state derivation and the write are all wired together.
- **A DEFEND-only plan is not advanced** over the same window (the D10 opt-in contract).
- The manager resolves through `OVT_Global.GetVirtualMovement()`, and its tracked count returns to 0 after everything is unregistered (Q7).

Every case unregisters **before** it reports, so a red assertion never leaves a moving group in the world for later cases.

### Persistence tier — **no serializer test, deliberately; one existing case amended**

There is nothing to round-trip: no serializer, no payload, no `Overthrow.conf` entry (D3). Q5's empty diff **is** the coverage.

What *is* added is one claim on the existing shared gate, `OVT_TEST_PersistenceRoundTrip_VirtualGroups_SurviveSaveAndReload`: the fixture is deliberately moved with `SetPosition` before the save and asserted to come back at the moved position (T3.2). That is timing-free — a direct write, not a tick — and it proves the epic-level property movement depends on: **core persists the live origin, so movement's writes are what a save keeps.** The same task makes the fixture stationary under movement (D12) so the case stops being a timing lottery.

### Every new case must be proven able to fail once

Exact edit recorded in a preamble comment. `maxAttempts` is banned project-wide.

### Not automatable, and why

| Area | Why manual |
|---|---|
| Materialisation position quality | Needs eyes on where the men actually appear relative to the log |
| The water rule | Needs a route over a real bay in a real world |
| Live → dormant handback | Needs a real observer walking in and out of a spawn ring |
| Save → quit → **Continue** | The autotest harness restarts the whole suite on a world transition (`api.md` §8) |
| Two campaigns in one session | Same reason; core's Phase 6 found four teardown bugs this way |
| Scale / hitch | A performance judgement, not an assertion |

---

## 8. Dependencies

**Hard preconditions (all satisfied today):**

- **`virtualization/core` complete and frozen.** `GetPosition`, `SetPosition`, `GetGroup`, `IsSpawned`, `GetRecord().m_Plan` and `FindGroupsBySystem` are live as of core Phase 6; `api.md` §10's `movement` table is the contract. The one additive ask (`GetAllHandles`, §3.7) is this feature's only core change.
- **Core's plan retention.** `record.m_Plan` is kept at runtime and persisted verbatim (`OVT_VirtualGroupRecord.c:34-39, :71`) — movement reads it every time it derives state.
- **Core's persistence reading the live origin** (`api.md` §8) — the reason no movement serializer is needed.
- **`OVT_WorldUtils.IsOceanAtPosition`** (`OVT_WorldUtils.c:555`) and `BaseWorld.GetSurfaceY` — the placement validity rule.
- **`OVT_VirtualizationMath.SliceIndices` / `AdvanceCursor`** (`:208`, `:239`) — reused, not re-implemented.
- **Core's debug affordances** — `m_bDebugRegisterTestGroup`, `m_iDebugTestGroupCount`, `m_iDebugTestGroupSpawnDistance`, `m_sDebugTestGroupName`, whose test group already registers a **two-point cycling patrol 150 m long** (`OVT_VirtualizationManagerComponent.c:2205-2223`). That is the entire play-test fixture; no consumer is needed.
- **The epic kill switch** `OVT_VirtPlaytestKillSwitch.DISABLE_LEGACY_AI_SPAWNS = true` stays **on** for the whole epic, so a movement play-test world contains only virtualization's own groups.

**Explicitly NOT depended on:**

- **`virtualization/civilians`** — ambient spawn sources are not registered groups and have no despawned life to advance. The two features are the epic's only parallelizable pair and must not acquire a dependency in either direction. `civilians` is making its own additive core edits to the **ambient config classes**; movement's edit is on the manager's query surface. Different files, different `api.md` sections, separate `context.md` notes — but **re-grep every file:line in this plan before editing** ([R7](#9-risks--mitigation)).
- Anything in `integration` or `base-defense-migration`. `integration` depends on **this**, not the other way round.

**Downstream (what this unblocks):** `integration` migrates deployments' patrols and tower garrisons onto core; without movement they would freeze while despawned. Its planner should read §3.8 first.

**User-side (Workbench, interactive):** adding `OVT_VirtualMovementManagerComponent` to `Prefabs/GameMode/OVT_OverthrowGameMode.et` (T3.9 — text-wired by an agent, opened and verified by the user, core's T2.8 precedent), and the play-test passes of §6.

---

## 9. Risks & Mitigation

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| **R1** | **Movement silently breaks the shared persistence gate** — finding F-A: a dormant test fixture with a 150 m plan drifts, and a ±1 m assertion across a multi-second case goes red. | **High if not designed against** | The All group goes red and looks like a persistence regression | T3.1 lands **before** the tick exists: the fixture's types become `DEFEND` (payload claims unchanged) and every other `RegisterGroup(` site under `Scripts/Game/Tests/` gets a recorded verdict. [D12](#d12--shared-test-fixtures-are-made-stationary-by-construction). |
| **R2** | **Projection ambiguity on ping-pong legs** — after a load, direction is not recoverable, so a group may walk its route the other way, or re-walk part of a leg. | High (by construction) | Cosmetic | **Accepted** by the requirements and D3. The worst case is bounded by one leg; nothing is lost, nothing desyncs, and no player can tell where a dormant patrol "should" have been. |
| **R3** | **A dormant group's entity was deleted by the engine** (eliminate-when-reached, world churn) and movement keeps advancing a record with no entity. | Medium | None, if handled | Already handled by core's own contract: `GetPosition` falls back to `record.m_vPosition` and `SetPosition` still books the record (`:1716`, `:1734`). Movement needs no special case — and must not add one, because "record exists, entity does not" is a legitimate runtime state. |
| **R4** | **Tick cost at scale** — hundreds of registered groups make the per-tick enumeration/sort/purge the dominant cost even though the slice is bounded. | Low-Medium | Server frame time | The per-tick whole-registry work is one array copy, one insertion sort and a count-guarded purge; the *per-group* work is bounded by `m_iGroupsPerTick`. T4.5 measures at 40 groups (core's own scale probe registered 100 in 73 ms). If it ever bites, the fix is caching the sorted order between ticks — a local change with no contract impact. |
| **R5** | **Water-crossing legs look wrong** — a group crosses a bay in a straight line as though it had a boat. | Medium | Cosmetic, unobservable | Deliberate ([D6](#d6--waypoint-semantics-and-the-water-rule)): the alternative deadlocks a cycling route forever. The group is dormant by definition while it happens, and can only ever **materialise** on land, which is the requirement that matters. Route authoring (a consumer's job) is where a sea-crossing leg should be avoided. |
| **R6** | **The waypoint-resume artefact** — finding F-B: a moved group materialises where movement put it and then walks its route from index 0, which can look like the AI "going back". | Medium | Cosmetic, up to one lap | Documented in §3.8 and [D11](#d11--movement-never-touches-waypoint-entities) rather than engineered around; movement is forbidden from rewriting waypoints. If it ever proves unacceptable, the fix is a **core** change requested through `api.md`'s additive process. |
| **R7** | **Concurrent sessions move the tree** — `civilians` is in progress and is also making additive core edits; bugfix sessions land between planning and implementation. | High | Stale file:line references | Every file:line here is dated **2026-08-17**; re-grep before editing. `civilians` touches `OVT_AmbientSpawnSourceConfig` and `PruneAmbientEntities`; movement touches the manager's query surface and `api.md` §3/§10 — different regions, **separate** dated notes in `core/context.md`. Nothing in this plan depends on a line number for correctness. |
| **R8** | **A consumer registers a group with a plan and does not expect it to move** (a garrison authored with patrol points "for later"). | Medium | A garrison wanders off its base | [D10](#d10--the-plan-is-the-opt-in-there-is-no-movement-flag) makes the rule explicit and puts it in the seam contract `integration` reads (§3.8): the plan **is** the opt-in, and a garrison registers with an empty or DEFEND plan. Cheap to get right, loud when got wrong (the group visibly leaves). |
| **R9** | **A second tick from a dead world** — the call queue outlives a campaign, so a second campaign in the same session advances every group twice as fast. | Medium | Doubled speed, invisible in a log | Core hit exactly this in its Phase 6 restart audit. Copied verbatim: idempotent install latch, `s_Instance != this` self-cancel at the top of the tick, `queue.Remove` in `OnDelete`, `s_Instance` cleared in `OnDelete`. §6 step 10 is the live check. |
| **R10** | **`SetPosition` is written to a group that is actually spawned** — the doc rule is not enforced in core's code, so a mis-ordered gate would relocate a group entity out from under its live members. | Low-Medium | Members and their group entity disagree | `IsSpawned()` is the **first** statement of the per-handle body, before any read; the state is dropped for spawned handles so nothing is even derived for them; the Q10 grep proves `SetPosition` has exactly one call site. |
| **R11** | **World time behaves unexpectedly** across a Continue or a world transition, producing a negative or enormous delta. | Low | One bad jump | The delta is clamped into `[0, MAX_STEP_SECONDS]` ([D5](#d5--elapsed-time-is-measured-per-group-not-assumed-per-tick)), so the worst case is one capped step. The state map is also rebuilt on a new world, so a stale timestamp cannot survive one. |

---

## Agent Routing Summary

| Phase | Agent | Advanced? |
|---|---|---|
| 1 — Progression maths + Logic tier | `component-developer` | no — new files only, world-free, touches nothing that exists |
| 2 — The additive core seam (`GetAllHandles` + api.md + context.md) | `component-developer-advanced` | **yes** — edits the epic's frozen core and its contract documents |
| 3 — Manager, tick, writes, wiring, fixture compatibility | `component-developer-advanced` | **yes** — writes positions into every registered group in the world; game-mode + prefab + shared test-suite surface; restart hygiene |
| 4 — Init-tier coverage, seam docs, scale check | `component-developer` | no |

**Skills to activate:** `enforcescript-patterns` (all phases), `overthrow-architecture` (2, 3), `workbench-workflow` (3 — the prefab wiring and the play-test).

**No `help-docs-sync` phase:** nothing player-facing changes until `integration` registers real patrols; that feature carries the tutorial / Field Manual / wiki obligation. Stated in [§4](#4-implementation-phases) so the omission is a decision on the record.
