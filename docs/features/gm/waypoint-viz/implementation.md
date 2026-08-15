# Waypoint Viz — Implementation Plan

**Status:** Planning
**Epic:** gm (feature 4 of 5 — Phase 1 of the 3-phase epic)
**Started:** 2026-08-15
**Target Completion:** TBD
**Last Updated:** 2026-08-15 20:55 AEST

> All `file:line` citations are load-bearing and were **verified during planning**. Overthrow-side citations
> against the working tree at **`b01782c3`** (`feat: gm/overthrow-panel`); base-game citations against the
> Reforger 1.8 reference tree at `/mnt/n/Projects/Arma 4/ArmaReforger`. Keep them when editing. Where this
> plan and `requirements.md` disagree, **this plan wins** and the disagreement is recorded in §5.

---

## 1. Executive Summary

A Game Master can see an Overthrow AI group standing in a field. What they cannot see is **where it was told
to go**. Overthrow spawns waypoints from ~20 sites across town patrols, base upgrades, QRF, deployments and
jobs, and every one of those waypoints is invisible: the vanilla waypoint prefabs carry no `RplComponent`
(`Prefabs/AI/Waypoints/AIWaypoint_Base.et` has exactly one component, `Persistence`), so they exist **only on
the server**. This feature draws that hidden intent: select a group in the GM editor and its route appears in
the 3D view as a connected line — group → WP1 → WP2 … — with the current waypoint and the leg leading to it
picked out in a second colour.

**The shape, in one paragraph.** When the GM's selection changes to a group, a client-side renderer asks the
gm-state seam for that one group's waypoints (a new request type on the existing `OVT_GMRequestComponent`).
The server resolves the `RplId` to the group entity, walks `AIGroup.GetWaypoints()` — recursing one level into
`AIWaypointCycle` children, because Overthrow's perimeter patrols nest **all** their waypoints inside a cycle
(`OVT_OverthrowConfigComponent.c:548-551`) — classifies each waypoint from its prefab name, and fans the route
back as `Begin … per-waypoint … End` under the same seq/version framing the campaign snapshot already uses.
The client commits the fan in one step and re-issues a handful of `Shape` calls every frame while the group
stays selected. **Nothing is editable, no widget is created, no proxy entity is spawned, and no base-game file
is forked.**

**Four findings from planning that shape the work and are worth stating up front:**

1. **The wire extension is mandatory, not a design preference.** No waypoint prefab in the vanilla tree carries
   an `RplComponent` — that is precisely what the rejected E_ editable set *adds*. A local-read design would
   work perfectly on a Workbench listen host and show a dedicated-server GM nothing at all, which is the worst
   possible failure shape: it passes every check a developer can run alone.

2. **`Shape.CreateLines` is a line *strip*, and there is a thickness parameter.**
   `ArmaReforger/scripts/Core/generated/Debug/Shape.c:37` —
   `CreateLines(int color, ShapeFlags flags, vector p[], int num, float thickness = 1)`, documented "Create
   line strip", with vanilla passing 19 points in one call (`SCR_AIWorld.c:241`). So the **whole route is one
   native call**, not one call per leg, and a fixed-size `vector` array sized to the cap means the draw path
   allocates nothing script-side per frame. `CreateLinesLoop` (`:46`) exists but is **not** usable here — it
   would close the loop back to the *group*, not back to WP0 (§5 D6).

3. **The renderer needs no layout, no `.meta` and no GUID.** `SCR_EditModeEditorUIComponent` —
   overthrow-panel's existing injection point — descends from `MenuRootSubComponent`
   (`SCR_EditModeEditorUIComponent.c:1` → `SCR_BaseModeEditorUIComponent.c:1` → `MenuRootSubComponent`), so it
   already has `GetMenu()` (`MenuRootSubComponent.c:23`) and therefore
   `MenuRootBase.GetOnMenuUpdate()` (`MenuRootBase.c:72`) — the exact per-frame invoker vanilla's own waypoint
   renderer uses (`SCR_WaypointLinesEditorUIComponent.c:171-173`). The renderer is a plain `Managed` object
   owned by that modded class, ticked from the mode root's own update. **`{6B0A…}` is verified free (0 hits
   across `Prefabs Configs Scripts UI Language`) and this feature does not need it** — it stays available for
   `gm-map`.

4. **A group-driven read is immune to the known waypoint leaks, and there is no dormancy special case.**
   Overthrow leaks detached waypoint entities in several places (QRF group lists are never drained;
   `OVT_EntitySpawningAPI.CleanupGroup` deletes soldiers but not the group). Asking the *group* for its
   waypoints can never surface an orphan — a world scan for `AIWaypoint` entities would surface all of them.
   Waypoints are also session-scoped and never persisted (`OVT_WorldUtils.SpawnEntityPrefab` untracks every
   `AIWaypoint` it spawns; `Modded/SCR_AIGroup.AddWaypointsDynamic` untracks the dynamic ones), so this feature
   has **no save/load and no JIP dimension at all**.

---

## 2. Goals

### Primary

1. **A GM can see where a selected group was told to go.** Selecting an Overthrow AI group in the Game Master
   editor draws its route in the 3D view: group position → WP1 → WP2 → … , closing the loop when the route is
   a cycle.
2. **The current waypoint and its leg are distinguishable at a glance**, in a second colour.
3. **It is read-only in the strongest sense.** No Overthrow code path this feature adds calls a single
   mutating waypoint or group API, and a grep proves it (§6 Q-4).
4. **It does not interfere with base-game waypoint editing.** No `SCR_EditableWaypointComponent`, no E_ prefab,
   no editable waypoint entity, no canvas contention. A GM who assigns a vanilla waypoint sees exactly what
   they saw before this feature existed.
5. **Only authorized GMs receive waypoint data.** The new request handler runs the same server-side gate as
   every other handler on the seam; an unauthorized caller gets no reply of any kind.
6. **The seam is extended additively.** No existing RPC signature, record shape or client-facing method
   changes, so `overthrow-panel`, `hud-icons` and `gm-map` are unaffected by this feature landing.

### Secondary

7. **The classifier and the route geometry are unit-tested** (Logic tier), and the cycle recursion — the one
   piece of server logic that can be silently wrong — is pinned by a constructed Campaign-tier fixture.
8. **Conventions `gm-map` can copy** when it renders routes on the map screen: the waypoint type enum, the
   record shape and the "which leg is highlighted" rule are named and documented once.

### Explicit non-goals

- **No editing.** No reordering, no deleting, no dragging, no context actions. Epic Phase 1 is read-only.
- **No E_ waypoint set** — settled by epic decision, recorded in §5 D1, not re-litigated.
- **No map-screen rendering.** `gm-map` owns the map; if it wants routes it reuses this feature's records
  (§5 D10).
- **No re-poll while selected.** Fetch once at selection; reselect to refresh (user decision, §5 D3).
- **No all-groups mode**, no "show every route in the world" toggle.
- **No panel detail text.** The detail slot is owned by `hud-icons`' `OVT_GMDetailUIComponent`; this feature
  does not touch it and never calls `ClearDetail()` (§5 D9).
- **No new prefab, layout, `.meta`, GUID, localization key or persistence registration.**
- **No help/wiki phase.** One consolidated `help-docs-sync` pass runs after `gm-map` closes epic Phase 1
  (`epic-overview.md`). Do not add one here.

---

## 3. Architecture Overview

### 3.1 Component hierarchy

```
SERVER SIDE — pure logic + one new handler on the existing seam

OVT_GMWaypointFormat                              NEW  Scripts/Game/GameMode/GM/
    PURE statics, world-free, used by BOTH sides. Logic-tier testable.
    enum OVT_EGMWaypointType { UNKNOWN, MOVE, PATROL, DEFEND, SEARCH_DESTROY,
                               WAIT, SCOUT, GET_IN, GET_OUT, ACTION, CYCLE }
    static int  ClassifyPrefab(string prefabResourceName)
    static int  LegCount(int waypointCount, bool cyclic)
    static bool IsHighlightLeg(int legIndex, int currentIndex)

OVT_GMWaypointWalk                                NEW  Scripts/Game/GameMode/GM/
    SERVER-ONLY, strictly read-only.
    static int Flatten(array<AIWaypoint> src, int max, out array<AIWaypoint> flat,
                       out bool cyclic, out bool truncated)     <- the cycle recursion
    static int Collect(AIGroup group, int max, out array<AIWaypoint> flat,
                       out int currentIndex, out bool cyclic, out bool truncated)

OVT_GMRequestComponent                            MODIFIED  (gm-state owns this file)
    + OVT_EGMRequestType.GROUP_WAYPOINTS
    + RpcAsk_GroupWaypoints         client -> server, GATED
    + RpcDo_WaypointsBegin / RpcDo_Waypoint / RpcDo_WaypointsEnd   server -> owner
    + RequestGroupWaypoints(RplId)  public client API
    + GetRoute() / GetOnRouteUpdated()             the consumption contract for this feature


CLIENT SIDE — exists only while the GM editor is in EDIT mode

OVT_GMWaypointRecords.c                           NEW  Scripts/Game/GameMode/GM/
    class OVT_GMWaypointRecord : Managed { int m_iIndex; vector m_vPos; int m_iType; }
    class OVT_GMWaypointRoute  : Managed { RplId m_GroupRplId; int m_iCurrentIndex;
                                           int m_iFlags; bool m_bComplete;
                                           ref array<ref OVT_GMWaypointRecord> m_aWaypoints;
                                           vector m_vGroupPos; ... Clear() / CopyFrom() }

modded SCR_EditModeEditorUIComponent              MODIFIED  (~8 lines, overthrow-panel owns this file)
    HandlerAttachedScripted -> new OVT_GMWaypointRenderer().Attach(GetMenu())
    HandlerDeattached       -> renderer.Detach(); renderer = null

OVT_GMWaypointRenderer                            NEW  Scripts/Game/Components/GM/
    plain Managed - owns NO widget, draws with the Shape API only
    Attach(MenuRootBase)  subscribe SELECTED filter + GetOnMenuUpdate() + seam invokers
    OnSelectionChanged()  re-read filter -> first entity -> GROUP? -> RequestGroupWaypoints
    OnMenuUpdate(float)   re-issue the route's Shape calls for this frame
    Detach()              remove every subscription, drop the cached route
```

Nothing here is a Manager and nothing is a Controller. There is no new entity, no new prefab, no new
persistence and **no new `OVT_Global` accessor** — the seam is reached through
`OVT_ControllerComponent<OVT_GMRequestComponent>.Get()` as the gm-state contract requires
(`Scripts/Game/Components/Controller/OVT_ControllerComponent.c:36`).

### 3.2 Data flow, one selection

```
CLIENT                                                SERVER
──────                                                ──────
GM clicks a group icon
  SELECTED filter OnChanged fires
    OVT_GMWaypointRenderer.OnSelectionChanged
      ├─ re-read filter contents (sets NOT trusted)
      ├─ first entity; type != GROUP  -> clear route, draw nothing, DONE
      ├─ rplId = owner.RplComponent.Id()
      ├─ rplId == the route already cached/in flight -> DONE   (kills box-select spam)
      └─ gm.RequestGroupWaypoints(rplId) ────────────►  RpcAsk_GroupWaypoints(type, seq, rplId)
             m_iSeq++                                   ├─ if(!Replication.IsServer()) return
             route.Clear()                              ├─ playerId = ResolveOwningPlayerId()
                                                        ├─ if(playerId <= 0) return
                                                        ├─ if(!IsAuthorizedGM(playerId))   ← THE GATE
                                                        │      LogRefusal(); return        (no reply at all)
                                                        ├─ if(requestType != GROUP_WAYPOINTS) return
                                                        ├─ entity = ResolveEntity(rplId)
                                                        ├─ group  = AIGroup.Cast(entity)
                                                        └─ OVT_GMWaypointWalk.Collect(group, cap, …)
                                                             group.GetWaypoints()          AIGroup.c:39
                                                             recurse AIWaypointCycle        AIWaypointCycle.c:32
                                                             group.GetCurrentWaypoint()     AIGroup.c:38
  ◄──────────────────────────────────────────────────  RpcDo_WaypointsBegin(seq, WIRE_VERSION, rplId,
                                                                            count, currentIndex, flags)
  ◄──────────────────────────────────────────────────  RpcDo_Waypoint(seq, index, pos, type)   × count
  ◄──────────────────────────────────────────────────  RpcDo_WaypointsEnd(seq, sent)
  │
  ├─ Begin: wireVersion mismatch -> refuse to stage, log once (shares gm-state's flag)
  ├─ Begin: staging seq = seq, staging route cleared, group/current/flags recorded
  ├─ Waypoint: seq != staging seq -> DROP        ← the same stale-discard rule
  └─ End:   seq matches -> commit staging into m_Route in one step,
            GetOnRouteUpdated().Invoke()

EVERY FRAME while EDIT mode is up
  MenuRootBase.GetOnMenuUpdate()
    OVT_GMWaypointRenderer.OnMenuUpdate
      ├─ no route, or route empty, or route's group no longer selected -> draw NOTHING, return
      ├─ points[0] = live group position (re-read each frame, so a moving group stays connected)
      ├─ points[1..n] = the snapshot waypoint positions
      ├─ Shape.CreateLines(base colour, VISIBLE|NOZBUFFER|ONCE, points, n+1, thickness)
      ├─ cyclic && n >= 2 -> Shape.CreateLine(points[n], points[1])         loop close
      ├─ currentIndex >= 0 -> Shape.CreateLines(highlight, …, 2 points of that leg)
      └─ Shape.CreateCircle at each waypoint (highlight colour for the current one)

DESELECT / EDITOR CLOSE / MODE SWITCH
  filter change with no group    -> route.Clear(), nothing drawn from the next frame
  GetOnStateCleared()            -> route.Clear()
  HandlerDeattached (mode root)  -> Detach(): unsubscribe all, drop the route
```

### 3.3 What crosses the wire, and what it costs

| RPC | Params | Arity | Direction |
|---|---|---|---|
| `RpcAsk_GroupWaypoints` | `int requestType, int seq, RplId groupRplId` | 3 | client→server (`RplRcver.Server`) |
| `RpcDo_WaypointsBegin` | `int seq, int wireVersion, RplId groupRplId, int count, int currentIndex, int flags` | 6 | server→owner |
| `RpcDo_Waypoint` | `int seq, int index, vector pos, int type` | 4 | server→owner |
| `RpcDo_WaypointsEnd` | `int seq, int sent` | 2 | server→owner |

Max arity **6**, matching the existing maximum on this component and well inside the ≤8 rule. **No `array<>`
parameter and no object parameter anywhere** — records cross field-by-field as scalars, exactly as
`OVT_GMRecords.c:4-9` requires (a hand-rolled bitstream is not an option: `ScriptBitWriter` hard-crashes on
first use from script).

`flags` is a bitfield: `FLAG_CYCLIC = 1` (the route closes back to WP0), `FLAG_TRUNCATED = 2` (the group had
more waypoints than the cap). `currentIndex` is `-1` when the group's current waypoint is not one of the
emitted records — a normal, print-free state (§5 D5).

**Volume.** A real Overthrow route is small: a perimeter patrol is 4 patrol + 4 wait waypoints inside one
cycle (`OVT_OverthrowConfigComponent.c:524-551`), a defend group is one waypoint, a QRF group accumulates 4
over the first minute (`OVT_QRFControllerComponent.c:389-392`). One selection therefore costs ~3–11 tiny RPCs,
**once**. A GM clicking through twenty groups costs less traffic than one campaign snapshot poll.

### 3.4 The route model, stated exactly

For `n` waypoints numbered `0 … n-1`:

- **Vertices:** `V0` = the group's live position (re-read every frame), `V(i+1)` = waypoint `i`'s snapshot
  position.
- **Legs:** leg `i` runs `Vi → V(i+1)`, i.e. **leg `i` terminates at waypoint `i`**, for `i` in `0 … n-1`.
  When the route is cyclic and `n >= 2` there is one additional leg, `n`, running waypoint `n-1` → waypoint `0`.
- **Highlight:** leg `currentIndex` and waypoint `currentIndex`. `currentIndex == -1` highlights nothing.
- **`n == 1`:** a single leg, group → WP0. Cyclic is meaningless and the loop-close leg is skipped.
- **`n == 0`:** nothing is drawn at all. Not an error, not a log line.

This is what `OVT_GMWaypointFormat.LegCount()` and `IsHighlightLeg()` encode, and it is the entire
Logic-testable surface of the renderer.

---

## 4. Implementation Phases

Effort is **S / M / L** relative to one focused session. "Agent" is the routing hint for `/proceed`.

> **No phase here is a refactor and no phase is routed to an advanced agent.** Phase 2 is the highest-risk
> phase — it edits the epic's 852-line data spine that three other features depend on — but it is *purely
> additive* into a file whose patterns are documented in its own header, which is exactly the shape gm-state's
> own wire phases (2 and 4) took with a standard `network-specialist`. If the orchestrator wants belt and
> braces, `/proceed-advanced` on Phase 2 is the sensible opt-in; nothing else in this plan warrants it.

---

### Phase 0 — Baseline — **S — no agent**

Record in `context.md` before any code:

| Gate | How |
|---|---|
| `tools/compile-check.sh` | exit 0 + file count |
| `git status` / `git rev-parse --short HEAD` | plan citations taken at **`b01782c3`**, tree carries hud-icons' uncommitted work; this tree receives concurrent bugfix commits — **re-check at every phase boundary** |
| Highest allocated bug id | `ls docs/bugs/` — **BUG-174** at planning time |
| GUID series | **This feature mints no GUIDs.** `{6B0A…}` was proven free at planning time (0 hits, braced grep across `Prefabs Configs Scripts UI Language`) and **stays reserved for `gm-map`**. If a later phase discovers a GUID is needed, re-grep with the brace before minting — a bare `6B0A` grep gives false hits inside unrelated GUIDs. |
| Seam citations resolve | `OVT_GMRequestComponent.c:133` (`IsAuthorizedGM`), `:52` (`WIRE_VERSION`), `:9-12` (`OVT_EGMRequestType`), `:300` (`RequestSnapshot`), `:279` (`OnEditorClosed`), `:848` (`IsStagingRecord`); `OVT_ControllerRequestComponent.c:89` (`ResolveEntity`), `:127` (`ShouldRespondLocally`) |
| Base-game citations resolve | `AIGroup.c:38-39`, `AIWaypointCycle.c:32`, `Shape.c:37`, `MenuRootSubComponent.c:23`, `MenuRootBase.c:72`, `SCR_WaypointLinesEditorUIComponent.c:53` |

**Do NOT run `tools/run-tests.sh`** — planning and implementation stop at `compile-check.sh` exit 0; the
orchestrator runs suites after a phase completes (`.claude/test-policy.md`).

**Acceptance:** baseline table filled in `context.md`.

---

### Phase 1 — Classification, route geometry and the server-side walk — **M — `component-developer`**

> Everything that is not the wire and not the drawing. It is also **the entire automatable surface of this
> feature**, which is why it comes first and why it carries both new test cases.

**Tasks**

1. `Scripts/Game/GameMode/GM/OVT_GMWaypointFormat.c` — pure statics, **world-free**:
   - `enum OVT_EGMWaypointType { UNKNOWN, MOVE, PATROL, DEFEND, SEARCH_DESTROY, WAIT, SCOUT, GET_IN, GET_OUT, ACTION, CYCLE }`
     — coarse on purpose; it drives colour and nothing else.
   - `static int ClassifyPrefab(string prefabResourceName)` — matches on the file stem of the resource name.
     The twelve prefabs Overthrow actually assigns are authored at
     `Prefabs/GameMode/OVT_OverthrowGameMode.et:69-82`:
     `AIWaypoint_Move` → MOVE, `AIWaypoint_Patrol` → PATROL, `AIWaypoint_Defend` and
     `AIWaypoint_Defend_ConflictBaseTeamPatrol` → DEFEND, `AIWaypoint_Loiter_CO` → PATROL,
     `AIWaypoint_GetIn` → GET_IN, `AIWaypoint_GetOut` → GET_OUT, `AIWaypoint_Scout` → SCOUT,
     `AIWaypoint_Wait` → WAIT, `AIWaypoint_Cycle` → CYCLE,
     `AIWaypoint_SearchAndDestroy` → SEARCH_DESTROY, `AIWaypoint_UserAction` → ACTION.
     - ⚠️ **ORDER IS A TRAP.** `AIWaypoint_Defend_ConflictBaseTeamPatrol.et` contains the substring `Patrol`.
       A naive "contains" chain that tests PATROL first classifies every base defend group as a patrol. Test
       the most specific tokens first, and §7's Logic case pins exactly this.
     - Anything else — a vanilla GM-assigned waypoint, a modded one, an empty string — is **UNKNOWN**, which
       renders in the base colour. Never guess.
   - `static int LegCount(int waypointCount, bool cyclic)` and
     `static bool IsHighlightLeg(int legIndex, int currentIndex)` — §3.4's rules, so the renderer's geometry
     is asserted rather than eyeballed.
   - ⚠️ **Logic-tier rule:** `GetGame()`, `OVT_Global` and every world reference must stay out of this file
     **and its test, including comments** — the guard is a directory-wide grep that does not read prose. Two
     features have already tripped it.
2. `Scripts/Game/GameMode/GM/OVT_GMWaypointWalk.c` — **server-only, strictly read-only** statics:
   - `static int Flatten(array<AIWaypoint> src, int max, out array<AIWaypoint> flat, out bool cyclic, out bool truncated)`
     — copies `src` into `flat`, and for any element that casts to `AIWaypointCycle`, replaces it with that
     cycle's children (`AIWaypointCycle.GetWaypoints`, `AIWaypointCycle.c:32`) and sets `cyclic`. **Recursion
     depth is capped at 2** and an element already emitted is never re-entered — a malformed
     cycle-inside-itself must not hang the server. Stops at `max` and sets `truncated`.
   - `static int Collect(AIGroup group, int max, out array<AIWaypoint> flat, out int currentIndex, out bool cyclic, out bool truncated)`
     — `group.GetWaypoints()` (`AIGroup.c:39`) → `Flatten` → then `currentIndex` by **identity comparison** of
     `group.GetCurrentWaypoint()` (`AIGroup.c:38`) against each flattened entry, `-1` when it matches none.
   - **Never a world scan.** The only entry point is a group. Write that in the class header with the reason
     (§1 fact 4): a scan would surface the leaked, detached waypoints Overthrow is known to leave behind.
   - **Nothing in this file writes.** No `AddWaypoint`, no `RemoveWaypoint`, no `SetWaypoints`, no
     `CompleteWaypoint`, no `SetCompletionRadius`. §6 Q-4 greps for it.
3. **Record the observed cycle semantics in `context.md`** (this is a finding, not a guess): does
   `AIGroup.GetWaypoints()` on a cycled group return the cycle container or its expanded children, and does
   `GetCurrentWaypoint()` return the container or the child currently being executed? Vanilla's
   `SCR_EditableGroupComponent.ReindexWaypoints` (`:243-253`) compares the group's current waypoint against
   the **cycle's children**, which implies the child — but that is an inference, not a proof. **The walk is
   written to be correct either way**: if the engine already expands cycles, `Flatten` is a no-op pass-through
   and the route simply is not marked cyclic; if `GetCurrentWaypoint()` returns the container,
   `currentIndex` is `-1` and nothing is highlighted. Neither degradation prints anything.
4. `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_GMWaypointFormat.c` — Logic tier, cases in §7.
5. `Scripts/Game/Tests/TestSuites/Campaign/OVT_TEST_Campaign_GMWaypointWalk.c` — Campaign tier, cases in §7.
   The fixture spawns **waypoints only, no group and no AI**, so the case is fast and deterministic; the Init
   suite already spawns a real patrol waypoint in this world (`OVT_TEST_InitSuite.c:1541`, `:1625`), which is
   the precedent that it works. **Delete every spawned entity at the end of the case.**

**Acceptance**

- `tools/compile-check.sh` → exit **0**, file count +4.
- Logic tier +1 case set, Campaign tier +1 case; **each proven able to fail**, method recorded in `context.md`
  (for the Campaign case, removing the cycle-recursion branch must produce a named `SetFailure` that prints
  the flattened count, not a silent pass).
- Grep over both new files: zero `GetGame()`/`OVT_Global` in the format file and its test; zero waypoint-write
  API anywhere.

---

### Phase 2 — 🔴 The wire: request type, fan, client route store — **M — `network-specialist`**

> The highest-risk phase. It edits `OVT_GMRequestComponent.c`, the epic's data spine, which
> `overthrow-panel` and `hud-icons` already consume in production. **The rule for this phase: additive only.**
> No existing signature, field, method or behaviour changes. Read the file's header block before touching it —
> it states the invariants this phase must not break.

**Tasks**

1. **Extend the request enum** (`OVT_GMRequestComponent.c:9-12`): add `GROUP_WAYPOINTS` **after**
   `CAMPAIGN_SNAPSHOT`. Appending preserves every existing ordinal.
2. **The ask** — `RpcAsk_GroupWaypoints(int requestType, int seq, RplId groupRplId)`,
   `[RplRpc(RplChannel.Reliable, RplRcver.Server)]`. The handler body, in this order and no other:
   `Replication.IsServer()` guard → `ResolveOwningPlayerId()` → `playerId <= 0` return →
   **`IsAuthorizedGM(playerId)` → `LogRefusal(playerId)` and return with nothing sent** → request-type check →
   `ResolveEntity(rplId)` → `AIGroup.Cast`. Copy the shape of `RpcAsk_Snapshot` (`:344-361`) exactly.
   **Identity is never a parameter.**
3. **The reply**, three sends, each with the mandatory `ShouldRespondLocally` short-circuit
   (`OVT_ControllerRequestComponent.c:127`) and the `Rpc()` call left at the call site:
   `SendWaypointsBegin` / `SendWaypoint` / `SendWaypointsEnd`, arities per §3.3.
   - **An empty answer is still an answer.** A group that resolves but has no waypoints, and a `RplId` that
     resolves to nothing or to a non-group, both get `Begin(count 0) + End(0)`. Only an **authorization**
     failure produces silence. Without this, fetch-once leaves a stale route on screen forever when a request
     legitimately finds nothing.
   - `[Attribute]` `m_iMaxWaypointsPerGroup`, default **32**. On truncation set `FLAG_TRUNCATED` and log one
     throttled server WARNING naming the group's RplId and the true count. Real routes are ≤9, so this should
     never fire — which is exactly why it must be loud if it does.
4. **Client staging and commit.** Three `[RplRpc(RplChannel.Reliable, RplRcver.Owner)]` handlers mirroring the
   snapshot's staging discipline: `Begin` checks `WIRE_VERSION` (reusing `m_bLoggedVersionMismatch` so a
   mismatched build logs once for the whole component, not once per surface) and opens a **separate** staging
   seq for routes; every record checks that seq and **drops** on mismatch; `End` commits staging into the live
   route in one step and fires the invoker. Use a distinct `m_iRouteSeq` / `m_iRouteStagingSeq` pair — sharing
   `m_iSeq` with the snapshot poll would make an 8-second poll silently invalidate an in-flight route.
5. **The client-facing contract** (three additions, documented in the class header alongside the existing
   trio):
   - `void RequestGroupWaypoints(RplId groupRplId)` — increments the route seq, clears the live route, and
     branches `if (Replication.IsServer()) RpcAsk_GroupWaypoints(...); else Rpc(...)` — the listen-server half
     (`RequestSnapshot`, `:300-314`, is the template). Re-asserts `IsLocalControllerOwner()` first.
   - `OVT_GMWaypointRoute GetRoute()` — **never null**.
   - `ScriptInvoker GetOnRouteUpdated()` — lazily created, fired on a committed `End`.
6. **Editor-close teardown.** In `OnEditorClosed()` (`:279-292`) clear the route store **above** the
   `if(!m_State.HasData()) return;` early return at `:287` — otherwise a session that received a route but no
   campaign snapshot keeps a stale route forever. Do not add a second "cleared" invoker: consumers already
   subscribe to `GetOnStateCleared()`.
7. `Scripts/Game/GameMode/GM/OVT_GMWaypointRecords.c` — `OVT_GMWaypointRecord` and `OVT_GMWaypointRoute`
   (§3.1), both plain `Managed`, arrays never null, with `Clear()` and `CopyFrom()`. Follow `OVT_GMRecords.c`'s
   documentation style; add a two-line pointer in `OVT_GMRecords.c`'s header saying where waypoint records
   live, so the next reader finds them.
8. **Arity-diff every new `Rpc()` call site against its handler by hand and record the diff in `context.md`**
   (BUG-090: `Rpc()` is an untyped variadic prototype — a wrong argument count compiles clean, passes every
   automated gate, and dies silently at the wire). This is the single highest-value review checkpoint in the
   feature.
9. Temporary bring-up aid: a `Print` at the top of each `RpcDo_*` while developing, removed before the phase
   closes.

**Acceptance**

- `tools/compile-check.sh` → exit **0**, file count +1.
- `git diff` on `OVT_GMRequestComponent.c` shows **only insertions plus the one moved line in
  `OnEditorClosed`** — no existing signature or behaviour altered.
- Arity diff table for all four new RPCs written into `context.md`; max arity 6; no `array<>` parameter.
- Greps: every new `RpcAsk_*` handler contains `IsAuthorizedGM`; zero `RplRcver.Broadcast`; zero `[RplProp]`.
- On a Workbench host with a temporary `Print`: selecting a group produces a Begin/records/End trio in the
  log with a plausible waypoint count, and a **non-GM path produces nothing at all**.

---

### Phase 3 — The renderer and the selection hookup — **M — `component-developer`**

> Routed to `component-developer` rather than `ui-developer` deliberately: there is **no layout, no widget, no
> `.meta`, no input binding and no localization** in this phase. The only UI-shaped work is ~8 lines in an
> existing modded class; everything else is the Shape API and invoker hygiene.

**Tasks**

1. `Scripts/Game/Components/GM/OVT_GMWaypointRenderer.c` — a plain `Managed` class:
   - `void Attach(MenuRootBase menu)`: subscribe `menu.GetOnMenuUpdate()`; subscribe the SELECTED filter
     (`SCR_BaseEditableEntityFilter.GetInstance(EEditableEntityState.SELECTED).GetOnChanged()`); subscribe the
     seam's `GetOnRouteUpdated()` and `GetOnStateCleared()`.
     **If the filter is null, retry exactly once via `CallLater(..., 0)` and then give up quietly** — the
     editor's entity manager initialises around the UI. This is `hud-icons`' proven idiom
     (`OVT_GMDetailUIComponent.c:97-101`, `:224-236`); copy its shape, do not invent a poll.
   - `void Detach()`: remove **every** subscription and the queued retry, then drop the cached route. The
     filter and the seam component both outlive this object; a missed removal wakes a dead renderer.
   - `OnSelectionChanged(state, inserted, removed)`: **ignore both sets** and re-read the filter's current
     contents (`filter.GetEntities`), take `entities[0]`, exactly as `hud-icons` does
     (`OVT_GMDetailUIComponent.c:363-379`) so the two features never disagree about which entity is "the"
     selection. Not a `GROUP` → clear and return. Resolve `RplId` from the owner entity's `RplComponent`
     (`:807-819` is the same helper, re-implemented locally — a three-line accessor is not worth a dependency
     on another feature's widget component). **If the resolved id equals the route currently cached or in
     flight, do nothing** — box-select and rapid clicks fire several changes for one visible action.
   - `OnMenuUpdate(float timeSlice)`: the draw path in §3.2. Bail on: no seam, no route, `m_aWaypoints`
     empty, or the cached route's group no longer selected.
2. **The draw path allocates nothing script-side per frame.** A **member** `vector m_aPoints[33]`
   (1 group vertex + the 32-waypoint cap) is refilled in place each frame and handed to
   `Shape.CreateLines(colourPacked, ShapeFlags.VISIBLE | ShapeFlags.NOZBUFFER | ShapeFlags.ONCE, m_aPoints, n+1, thickness)`.
   Pack the two colours **once** at attach into `int` members (`Color.PackToInt()`), as vanilla does
   (`SCR_WaypointLinesEditorUIComponent.c:153`). `ONCE` means the shape is re-issued per frame and never
   accumulates.
3. **The group vertex is live, the waypoints are a snapshot.** Read the group entity's position every frame
   (resolve the cached `RplId` → entity → `GetOrigin()`); if it no longer resolves, drop the first vertex and
   draw the waypoint chain alone rather than drawing to the origin of the world.
4. **Visual spec**, and nothing beyond it:
   - base colour polyline for the whole route, `thickness` ~2;
   - a separate 2-point line in the highlight colour for leg `currentIndex`, drawn **after** the polyline so
     it overdraws;
   - a separate 2-point line closing the loop (waypoint `n-1` → waypoint `0`) when `FLAG_CYCLIC` and `n >= 2`
     — `CreateLinesLoop` cannot be used, see §5 D6;
   - a small horizontal `Shape.CreateCircle` (`Shape.c:74`) at each waypoint so vertices are legible from the
     top-down camera a GM actually uses, in the highlight colour for waypoint `currentIndex`;
   - waypoint **type** selects nothing but the circle's colour tint; there is no legend, no label, no icon.
   - `[Attribute]`-free: the two colours, the thickness and the circle radius are named constants on the
     class. Nobody is going to tune them from a prefab, and this class has no prefab.
5. `Scripts/Game/UI/Modded/SCR_EditModeEditorUIComponent.c` — ~8 lines: a `ref OVT_GMWaypointRenderer` member,
   `Attach(GetMenu())` at the end of `HandlerAttachedScripted` (after the existing `super` call and panel
   creation), `Detach()` + null in `HandlerDeattached` **before** `super`. Null-check `GetMenu()`.
   - ⚠️ **`hud-icons` is code-complete but unmerged/unverified in this same file.** Re-read it before editing
     and keep the diff to pure insertions; do not reorder the existing panel/detail creation.
   - Hosting here is the structural GM gate — `Mode_Edit.layout` is instantiated only by `EditorModeEdit.et`,
     the one unlimited mode. **Never add an `IsLimited()` or role check client-side** (the file's own header
     says so at `:8-11`).
6. **Print-free operation.** No selection, a non-group selection, a group with no record, a group with no
   waypoints, a route that arrives after the GM has moved on — all silent. The only log lines this feature may
   ever produce are the server's throttled truncation warning and gm-state's existing once-per-session
   version-mismatch line.

**Acceptance**

- `tools/compile-check.sh` → exit **0**, file count +1.
- Greps over the renderer and the format file: zero `Rpc(`, zero `[RplProp]`, zero `RplRcver`, zero
  `SetDrawCommands(`, zero `ClearDetail(`, zero `SCR_EditableWaypointComponent`.
- On a Workbench host: selecting a town patrol draws a closed 8-vertex loop; selecting a base defend group
  draws one leg; clicking empty ground clears it within a frame; switching EDIT → PHOTO → EDIT leaves no
  drawing and no duplicate subscription.

---

### Phase 4 — Verification gate — **M — user-driven, no agent**

Run §6's Verification Method end to end. This is the only evidence that exists for visual clarity, the real
authorization path, the listen-server case, a dedicated-server client, and coexistence with `hud-icons` and
with vanilla waypoint editing. ⚠️ **Warn the user before launching** — client launches open a window on their
desktop and can orphan.

---

### Phase 5 — `context.md` and epic bookkeeping — **S — `component-developer`**

1. Write `docs/features/gm/waypoint-viz/context.md`: the shipped wire table with hand-checked arities; the
   **observed cycle semantics** from Phase 1 task 3 (this is the durable finding of the feature); the visual
   constants as built; the Phase 4 results; and a "the GM sees no lines" triage section — *is the selection a
   GROUP? → did a Begin arrive (temporary Print)? → did the gate refuse (server WARNING)? → does the group
   have waypoints at all? → then gm-state's own triage.*
2. Update `docs/features/gm/epic-overview.md`: feature 4 status and task count. **Do not add a help-docs
   phase** — the consolidated pass belongs to epic end.
3. Note in `epic-overview.md` that `{6B0A…}` was **not** consumed by this feature and remains free.

**Acceptance:** both files updated; a reader with no context can answer "what does a GM see when a group has
no waypoints?" from `context.md` alone.

---

## 5. Key Technical Decisions

### D1 — The E_ editable waypoint set is not used — **epic decision, settled, not re-litigated**

`requirements.md:8` and the epic both reject it. Recorded here so nobody re-derives it: the E_ set exists to
make waypoints **editable**, which this feature explicitly does not want; adopting it would put
`SCR_EditableWaypointComponent` + `RplComponent` + `[RplProp]` state on every Overthrow-spawned waypoint —
replication cost and editor-list clutter for hundreds of entities — and would put Overthrow waypoints into the
same editing flow the requirements say must not be disturbed. **Do not re-open.**

### D2 — Selected-group, on-demand fetch — **user decision 2026-08-15, settled**

The client asks only for the group the GM has selected, and only when the selection changes. Rejected
alternatives, named so they are not re-proposed: an all-groups layer (bounded by nothing, and a screen full of
crossing lines is less legible than one route); folding waypoints into the 8-second campaign snapshot (pays
for every group on every poll to serve the one the GM is looking at).

### D3 — Fetch once at selection; reselect to refresh — **user decision, settled**

The drawn route, **including which waypoint is current**, is a snapshot from fetch time. It does not re-poll.

**The accepted consequence, stated plainly:** a QRF group gains waypoints on `CallLater` timers at 5 s, 15 s,
30 s and 60 s after spawn (`OVT_QRFControllerComponent.c:389-392`), and a group that advances through its
route will show a stale "current" highlight. Both are invisible until the GM reselects, which is the
documented refresh gesture. This is the right trade for a tool a GM uses in bursts, and the alternative — a
per-selection poll timer — buys accuracy nobody asked for at the cost of a second traffic source that must
then be lifecycle-managed. `context.md` must state it so it is not later filed as a bug.

### D4 — The wire extension is forced by the engine, not chosen

Vanilla waypoint prefabs carry no `RplComponent`: `Prefabs/AI/Waypoints/AIWaypoint_Base.et` contains exactly
one component, `Persistence`. Waypoints therefore exist only on the server, and the only way a GM client can
learn a route is to be told. A "read it locally" design would appear to work in Workbench Play mode and on a
listen host, and show a dedicated-server GM an empty world — the failure mode a solo developer cannot catch.

The extension follows gm-state's published extension rule verbatim (`gm-state/context.md`, "Extending the
wire"): a new value appended to `OVT_EGMRequestType`, new `RpcDo_*` handlers under the same seq/version
framing, **no existing signature touched**, records field-by-field as scalars, and `IsAuthorizedGM` after
`ResolveOwningPlayerId()` at the top of the new `RpcAsk_*` before anything is read
(`OVT_GMRequestComponent.c:133-155`, `:344-361`).

### D5 — `currentIndex` is one int in the Begin frame, and `-1` is a first-class answer

The alternative — a `bool isCurrent` on every waypoint record — costs a field per waypoint to express a fact
that is true of exactly one of them. One `int` in the framing RPC says the same thing, and the sentinel `-1`
gives an honest home to three real cases: the group has no current waypoint; the current waypoint is the cycle
*container* rather than one of its children (vanilla's own reindex code, `SCR_EditableGroupComponent.c:243-253`,
implies the child but does not prove it); and truncation dropped the current waypoint. All three draw the route
with **nothing highlighted** and print nothing.

### D6 — The polyline starts at the group, so `CreateLinesLoop` cannot close the cycle

`Shape.CreateLinesLoop` (`Shape.c:46`) connects the last point back to the **first**, and the first point here
is the *group's* position, not WP0. Using it would draw a spurious leg from the last waypoint back to the
group. The loop is therefore closed by one extra 2-point `Shape.CreateLine` from waypoint `n-1` to waypoint
`0`, skipped when `n < 2`.

Related: the whole route is **one** `CreateLines` call, not one call per leg —
`Shape.c:29-37` documents it as "Create line strip", and vanilla passes 19 points in a single call
(`SCR_AIWorld.c:241`).

### D7 — Pure 3D `Shape` rendering; the editor canvas is not touched

Vanilla's own renderer also draws screen-space legs into the shared editor canvas via
`ProjWorldToScreenNative` + `LineDrawCommand` + `SetDrawCommands`
(`SCR_WaypointLinesEditorUIComponent.c:63-81`). **This feature does not**, for two reasons: `m_wCanvas` is
shared, and a direct `SetDrawCommands` discards every other user's commands (the compositor discipline
`OVT_MapCanvasLayer.c:4-7` exists to prevent); and a 3D line with `NOZBUFFER` already draws through terrain,
which is the only thing the canvas path was buying. §6 Q-6 greps for `SetDrawCommands(` to keep it that way.

### D8 — The renderer is hosted from the modded EDIT-mode root and owns no widget

`SCR_EditModeEditorUIComponent` descends from `MenuRootSubComponent` (`:1` → `SCR_BaseModeEditorUIComponent.c:1`),
so it already provides `GetMenu()` (`MenuRootSubComponent.c:23`) and thus `GetOnMenuUpdate()`
(`MenuRootBase.c:72`) — the exact per-frame hook vanilla's waypoint renderer uses
(`SCR_WaypointLinesEditorUIComponent.c:171-173`). A `Managed` renderer owned by that class therefore gets the
right lifetime (created when EDIT mode activates, destroyed when it deactivates) and the structural GM gate
for free, **without a layout, a `.meta`, a widget or a GUID**.

**Rejected:** a one-widget layout hosting a `SCR_BaseEditorUIComponent` subclass. It buys exactly the same
lifecycle and adds a `.layout` + `.meta` + two GUIDs, a duplicate-GUID risk and a silent-failure mode
(`compile-check.sh` parses none of it) — all to host a component that never touches a widget. YAGNI.

### D9 — Two SELECTED subscribers, coordinated by convention, not by coupling

`hud-icons` already subscribes to the SELECTED filter from its detail widget component
(`OVT_GMDetailUIComponent.c:196-204`). This feature adds a **second, independent** subscription rather than
hooking that component's classify path, because the two have different lifetimes: `hud-icons`' component lives
inside the panel's detail slot and does not exist if the panel failed to create, while the 3D drawing must
work regardless. `ScriptInvoker` supports many subscribers and vanilla itself attaches several to these
filters.

The coordination is a **convention both sides already follow**: ignore the inserted/removed sets, re-read the
filter's current contents, act on `entities[0]`. That guarantees the panel's text and the 3D route always
describe the same entity. Multi-select therefore behaves consistently: `hud-icons` shows the first entity plus
a count, this feature draws the first entity's route, and if `entities[0]` is a town the route is simply
cleared.

**This feature never touches the panel:** no `GetDetailSlot()`, no `ShowDetail()`, and above all no
`ClearDetail()` — that call would delete `hud-icons`' own widget tree.

### D10 — One route, not a keyed store

The task brief suggested a client store keyed by group `RplId`. With selected-group-only fetch and no
re-polling, at most one route is ever live, so a map keyed by group id would be a one-entry map with an
eviction policy nobody needs. The component holds **one** `OVT_GMWaypointRoute` (plus a staging twin, the same
shape gm-state uses for snapshots). If `gm-map` later wants several routes at once, promoting one route to a
small array is a contained change inside one class.

### D11 — Waypoint type is coarse and derived from the prefab resource name

There is no runtime API for "what kind of waypoint is this". `AIWaypoint.GetCompletionType()`
(`AIWaypoint.c:26`) returns `EAIWaypointCompletionType { All, Leader, Any }` — a completion rule, not a kind.
The `Type "Patrol"` / `Title` lines visible in `Prefabs/AI/Waypoints/AIWaypoint_Patrol.et` are **prefab editor
metadata**, not script-readable attributes. Nearly every waypoint is the same class (`SCR_AIWaypoint`) with a
different behaviour tree, so `ClassName()` collapses Move, Patrol, Scout and Search-and-Destroy into one
answer.

The prefab resource name — read with the existing `OVT_Global.GetPrefabName(entity)` helper — distinguishes all
twelve of the prefabs Overthrow assigns (`OVT_OverthrowGameMode.et:69-82`). Classification is a **pure string
function**, which is what makes it the one genuinely unit-testable piece of this feature, and unrecognised
names classify as `UNKNOWN` rather than guessing.

### D12 — Dormant and virtualized groups need no special path

The read is `group.GetWaypoints()` on the group entity the GM selected. A dormant group under the
virtualization epic's model *is* a group entity holding its waypoints (the epic's own note: waypoints never
leave the group across dormancy), and the GM can only select an entity that exists. If a future lifecycle
change ever detaches waypoints from a dormant group, this feature draws nothing for it — an empty route, not
an error. `requirements.md:15`'s "to whatever extent their waypoint data is available" is satisfied by
construction, and Phase 4 carries a manual check for it rather than speculative code.

### D13 — Known upstream waypoint defects are out of scope

`SpawnWaitWaypoint` discards its `time` argument (`OVT_OverthrowConfigComponent.c:497-502`) and
`SpawnGetInWaypoint(vector)` spawns the **GetOut** prefab (`:443-447`). Both are real defects and both will be
*visible* through this feature (a "GetIn" waypoint that classifies as GET_OUT is exactly the symptom). **Do
not fix them here** — a read-only visualization feature that quietly changes AI behaviour is worse than the
bug. Report them as findings into `epic-overview.md`'s Findings section, attributed to the owning epic.

---

## 6. Quality Bar

This feature has two halves with different failure modes, and both need naming.

**A. The data half — wire correctness and authorization.**
- Every new `RpcAsk_*` handler calls `ResolveOwningPlayerId()` then `IsAuthorizedGM(playerId)` **before it
  reads anything**, and refuses by returning with nothing sent plus one throttled log line. A handler that
  forgets this is privilege escalation, not a missing feature.
- **Identity is never an RPC parameter.** The caller is derived from the entity the RPC arrived on.
- Every response is `RplRcver.Owner`. **Zero `RplRcver.Broadcast`, zero `[RplProp]`** — a broadcast here would
  hand every player in the session the occupying faction's movement plans.
- Framing is `Begin … End` under a client-generated sequence id; a record whose seq does not match staging is
  **dropped, never merged**. The route seq is **independent** of the snapshot poll's seq.
- **No `array<>` and no object parameter**; max arity ≤8 (this feature's max is 6); every `Rpc()` call site
  hand-arity-diffed against its handler and the diff recorded (BUG-090 — wrong arity compiles clean and dies
  silently at the wire).
- Every owner-targeted send carries the `ShouldRespondLocally` short-circuit, because the most likely GM in
  the world is the host of a self-hosted server and the engine never loops an Rpc back to its sender.
- **Strictly read-only:** no mutating waypoint or group API is called from any path this feature adds.

**B. The render half — clarity, cost and silence.**
- **A route is legible in one glance:** which way it goes, where it is now, and whether it loops.
- **No per-frame script allocation in the draw path.** The point buffer is a fixed-size member; the two colours
  are packed once at attach; the only per-frame work is filling `n+1` vectors and issuing a handful of `Shape`
  calls with `ONCE`, which is the same pattern vanilla's own renderer uses.
- **Nothing is drawn when there is nothing to draw.** No selection, a non-group selection, an empty route, a
  cleared store, a closed editor — all draw exactly zero shapes.
- **Print-free operation.** Every empty and negative state is silent. The only permitted log lines are the
  server's throttled truncation warning (which should never fire on a real route) and gm-state's existing
  once-per-session wire-version line.
- **No canvas contention**, no widget, no proxy entity, no base-game file forked.
- **Teardown is symmetric.** Every subscription made in `Attach` is removed in `Detach`, and every deferred
  call queued is removed. The SELECTED filter and the seam component both outlive the renderer.

---

## 7. Testing Strategy

**Say plainly what the suites can see: the classifier, the geometry rules and the cycle recursion. Nothing
else.** Coverage in this project is a spine, not a surface — **JIP/multiplayer, UI, performance and save/reload
are uncovered** — and this feature is an RPC fan plus a 3D drawing inside the Game Master editor. No suite can
open the editor and no suite can look at a line.

### Logic tier — `OVT_TEST_Logic_GMWaypointFormat.c` (Phase 1)

World-free and pure. Self-registers via `[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]` — no edit to
`OVT_TEST_LogicSuite.c` is needed.

| Case | Asserts | Why it matters |
|---|---|---|
| Each configured prefab classifies | the twelve resource names authored at `OVT_OverthrowGameMode.et:69-82` each map to their intended `OVT_EGMWaypointType` | the whole colour scheme rests on this table |
| **The Defend/Patrol ordering trap** | `AIWaypoint_Defend_ConflictBaseTeamPatrol.et` → **DEFEND**, not PATROL | a "contains Patrol" chain tested first would mis-colour every base defend group; this is the case most likely to catch a real regression |
| A `{GUID}` prefix does not change the answer | the braced and unbraced forms of one name classify identically | the wire carries whatever `GetPrefabName` returned |
| Unknown and empty input | an unrecognised name and `""` both → `UNKNOWN`, never a wrong type and never a throw | an unrecognised waypoint must render neutrally, not as a defend point |
| Leg count, open route | `LegCount(4, false)` → 4 (group→WP0 … WP2→WP3) | off-by-one here draws a leg to a vertex that does not exist |
| Leg count, cyclic route | `LegCount(4, true)` → 5 | the loop-closing leg |
| Leg count, degenerate | `LegCount(1, true)` → 1 (no loop for a single waypoint); `LegCount(0, …)` → 0 | the two documented edge cases in §3.4 |
| Highlight resolution | `IsHighlightLeg(2, 2)` true; `IsHighlightLeg(2, 1)` false; **`IsHighlightLeg(anything, -1)` false** | `-1` is the first-class "nothing is current" answer (§5 D5) |

**Prove each case can fail** before shipping: invert the expectation, confirm the failure is a *named*
`SetFailure` (max 3 format args) and not a silent pass, revert, record the method in `context.md`.

⚠️ Keep `GetGame()`, `OVT_Global` and every world reference out of both `OVT_GMWaypointFormat.c` and this test
file **including comments** — the Logic guard is a directory-wide grep that does not read prose.

### Campaign tier — `OVT_TEST_Campaign_GMWaypointWalk.c` (Phase 1)

Not Logic: `Flatten` takes real `AIWaypoint` entities and needs a world. **The fixture is constructed, not
observed**, so the case is deterministic and cannot pass vacuously.

- **Fixture:** spawn 4 patrol + 4 wait waypoints through the config helpers, spawn an `AIWaypointCycle`, call
  `cycle.SetWaypoints(children)` — precisely what `GivePatrolWaypoints(PERIMETER)` does
  (`OVT_OverthrowConfigComponent.c:524-551`) — then call `Flatten({cycle}, 32, …)`. **No group and no AI are
  spawned**, so the case is fast. Precedent that waypoint spawning works in the autotest world:
  `OVT_TEST_InitSuite.c:1541`, `:1625`.
- **Asserts:** the flattened list has **8** entries, none of them the cycle container; `cyclic` is true;
  `truncated` is false; every entry classifies to a known type (patrols → PATROL, waits → WAIT), which
  simultaneously pins the classifier against real prefab names rather than string literals.
- **Second case:** `Flatten` with `max = 3` returns 3 entries and sets `truncated` — the cap is honoured
  rather than merely declared.
- **Anti-vacuity:** the failure message prints the flattened count and the first entry's class name, so
  "recursion missing" (count 1, `AIWaypointCycle`) reads differently from "fixture never spawned" (count 0).
- **Prove-can-fail:** remove the `AIWaypointCycle` branch from `Flatten` → the case must fail with the named
  message showing count 1. Record it.
- **Cleanup:** delete every spawned waypoint at the end of the case. They are already untracked from
  persistence by `OVT_WorldUtils.SpawnEntityPrefab`, so nothing leaks into a save.

### No Init tier case, and the reason

This feature adds **no prefab block, no layout, no `.meta` and no `.conf`** — the surfaces the Init tier exists
to gate, because they are otherwise fully silent. The seam component's prefab block is already gated by
`OVT_TEST_Init_GMRequestSeam`, and everything this feature adds is EnforceScript that `compile-check.sh`
parses. Adding an Init case here would test the compiler.

### What only a human can verify

Line legibility and colour at real camera distances, whether the highlight reads as "current", the loop
closing correctly, the authorization gate under a real engine role, listen-server versus dedicated-server
behaviour, coexistence with `hud-icons`' selection handling, and non-interference with vanilla waypoint
editing. All of it is §6's Verification Method. `tools/compile-check.sh` compiles EnforceScript and sees none
of it.

**The suites are run by the orchestrator after a phase completes — never by a planning or implementation
agent** (`.claude/test-policy.md`). Expected gates: **Fast** after Phase 1 (+ the Logic case set), **All**
after Phase 1 (+ the Campaign cases) and after Phases 2 and 3 as the regression net. A count that changes for
a reason other than the new cases is a finding to investigate, never a number to update.

---

## 8. Definition of Done

Criteria an independent evaluator with no implementation context can verify.

### Functional

- **F-1** With the Game Master editor open on a running campaign, selecting an Overthrow AI group that has
  waypoints draws a connected line in the 3D view from the group to its first waypoint and on through the rest,
  in order.
- **F-2** A **cycled** route (a town or base perimeter patrol) is drawn as a closed loop — the last waypoint
  connects back to the first.
- **F-3** The **current** waypoint and the leg leading to it are drawn in a visibly different colour from the
  rest of the route.
- **F-4** Selecting a different group replaces the drawing with that group's route. Deselecting (clicking empty
  ground), selecting a non-group entity, closing the editor, or switching to Photo mode all remove the drawing
  entirely.
- **F-5** A group with **no** waypoints draws nothing at all — no stub, no marker, no log line.
- **F-6** Lines are visible **through terrain and buildings** (a route behind a hill is still readable) and
  from a strategic camera altitude.
- **F-7** A non-authorized client receives **no** waypoint RPC of any kind, verified by a temporary `Print` at
  the top of each `RpcDo_*` which never fires for that player while an authorized GM on the same server is
  selecting groups.
- **F-8** Re-selecting a group refreshes its route (waypoints added since the first fetch appear). This is the
  documented refresh gesture (§5 D3) and the DoD asserts it works, not that the route auto-updates.

### Quality

- **Q-1 Empty and negative states are honest and silent.** No selection, non-group selection, unresolvable
  `RplId`, group with no waypoints, route arriving after the GM moved on — none of them draw anything and none
  of them print anything.
- **Q-2 Listen-server host works as GM.** A host who opens GM and selects a group sees the route. This is the
  criterion the `ShouldRespondLocally` short-circuit exists for.
- **Q-3 Open/close hygiene.** Opening and closing the editor **five times**, with a group selected each time,
  produces no script errors, no duplicated drawing and no leftover subscriptions — verified with a temporary
  `Print` pair in `Attach`/`Detach` matching one-to-one, and a route commit invoking nothing while the editor
  is shut.
- **Q-4 Strictly read-only.** Grep over every file this feature adds or edits finds **zero** occurrences of
  `AddWaypoint(`, `AddWaypointAt(`, `RemoveWaypoint(`, `RemoveWaypointAt(`, `CompleteWaypoint(`,
  `SetWaypoints(`, `SetCompletionRadius(`, `SetCompletionType(`.
- **Q-5 No editable-waypoint machinery.** Grep finds zero `SCR_EditableWaypointComponent`, zero
  `EnableCycledWaypoints`, and no reference to any `E_`-prefixed waypoint prefab.
- **Q-6 Renderer files are networking-free and canvas-free.** Grep over `OVT_GMWaypointRenderer.c`,
  `OVT_GMWaypointFormat.c` and `OVT_GMWaypointWalk.c`: zero `Rpc(`, zero `[RplProp]`, zero `RplRcver`, zero
  `SetDrawCommands(`.
- **Q-7 Wire hygiene.** Zero `RplRcver.Broadcast` and zero `[RplProp]` among the additions to
  `OVT_GMRequestComponent.c`; no RPC over 8 parameters; no `array<>` parameter; every new `Rpc()` call site
  arity-diffed by hand and the diff pasted into `context.md`.
- **Q-8 The gate is on every new handler.** Grep proves each new `RpcAsk_*` body contains `IsAuthorizedGM`,
  and that the call sits after `ResolveOwningPlayerId()` and before any entity resolution.
- **Q-9 No panel interference.** Grep proves zero `ClearDetail(`, zero `GetDetailSlot(` and zero
  `ShowDetail(` in this feature's files.
- **Q-10 No per-frame allocation churn in the draw path.** Read the draw method: no `new`, no array
  construction, no `string.Format`, no colour packing. The point buffer is a fixed-size member and the colours
  are packed once.

### Integration

- **I-1 The seam extension is additive.** `git diff` of `OVT_GMRequestComponent.c` shows insertions only, plus
  the single relocated clear in `OnEditorClosed`. No existing RPC signature, record shape, attribute or public
  method changed. `overthrow-panel` and `hud-icons` behave exactly as before.
- **I-2 gm-state's invariants are honoured**: identity is never a parameter; the gate runs inside the handler;
  responses are owner-targeted; records cross as scalars; the route staging seq is independent of the snapshot
  seq; the store clears on editor close.
- **I-3 No new `OVT_Global` accessor** — the seam is reached only through
  `OVT_ControllerComponent<OVT_GMRequestComponent>.Get()` (project rule, `OVT_ControllerComponent.c:10-14`).
- **I-4 `hud-icons` coexistence.** With both features active, selecting a group fills the panel's detail
  section **and** draws the route, and both describe the same entity. Neither breaks when the other is absent:
  the route still draws if the panel failed to create, and the panel still fills if the renderer is removed.
- **I-5 Vanilla waypoint editing is unaffected.** Assigning a waypoint to a group through the base game's own
  GM flow works exactly as before, its editable waypoint entity appears and can be moved and deleted, and
  vanilla's own waypoint lines render as they always did.
- **I-6 No base-game file forked.** `git diff --stat` shows no file under any base-game path; no `Configs/`
  change; exactly one `modded class` file touched, the one `overthrow-panel` already created.
- **I-7 No GUIDs consumed.** `{6B0A…}` is still free after this feature ships, and `context.md` says so for
  `gm-map`'s benefit.
- **I-8 No help/wiki content added** — the consolidated pass belongs to epic end.

### Verification Method

⚠️ **Warn the user before launching anything.** Client launches open a window on their desktop and can orphan.
Batch all user-driven checks into **one** session (Phase 4).

**Step 1 — Workbench Play mode (host path).** `IsAuthorizedGM` returns true unconditionally under
`#ifdef WORKBENCH` (`OVT_GMRequestComponent.c:137-143`), so the data path lights up without any login.

1. Start a campaign, let the occupying faction spend resources so base patrols exist, open Game Master.
2. Select a **town or base perimeter patrol** group. Confirm **F-1** and **F-2**: a closed loop through the
   patrol points. Count the vertices — a perimeter patrol should show **8** (4 patrol + 4 wait). Seeing **1**
   means the cycle recursion did not happen; that is the single most likely defect and Phase 1's Campaign case
   is its gate.
3. Confirm **F-3**: one leg and one waypoint marker are a different colour. Watch the group move; the
   highlight does **not** follow it — that is D3 working as designed, not a bug.
4. Select a **base defend** group (one waypoint). Confirm a single leg from the group to it.
5. Click empty ground, then a town icon, then a vehicle. Confirm **F-4** each time.
6. Find a group with no waypoints (or temporarily clear one). Confirm **F-5** — nothing drawn, nothing logged.
7. Fly behind terrain and up to ~300 m. Confirm **F-6**.
8. Re-select the same group after a QRF has run for a minute. Confirm **F-8** — the Scout/S&D waypoints
   scheduled at `OVT_QRFControllerComponent.c:389-392` are now on the route.
9. Open and close the editor five times with a selection live each time. Confirm **Q-3**.
10. **Vanilla flow (I-5):** assign a base-game waypoint to a group through the editor, move it, delete it.
    Everything must behave as it did before. Then select the group again and confirm this feature's route
    redraws without interfering.
11. **`hud-icons` coexistence (I-4):** with a group selected, the panel's detail section shows its origin and
    the 3D route is drawn, and they refer to the same group.

**Step 2 — multiplayer.** Prefer the user's own server, or
`tools/launch-server.sh -- -ovtGmDev` then
`tools/launch-game.sh --timeout 3600 --profile OverthrowClient1 --allow-concurrent -- -client 127.0.0.1:2001`.
**Always pass the long timeout** — the 600 s default kills the client mid-test. Note that a local
`--mode dedicated` join wedged twice at Steam backend auth during `overthrow-panel`'s verification; prefer the
user's own server or `--mode local`.

1. As a GM client on a **dedicated** server, repeat Step 1 checks 2, 3, 4. **This is the check the whole wire
   exists for** — it is the only topology where a local-read design would have shown nothing.
2. **Record which authorization path was used** (in-game admin login / `-ovtGmDev` / GM role). This is still
   owed to the epic from gm-state's Phase 5 and this is a chance to discharge it.
3. **Negative path — restart the server WITHOUT `-ovtGmDev`:** a second, non-admin client selecting groups
   must receive **no** `RpcDo_Waypoint*` of any kind (**F-7**, temporary client-side `Print`), and a coerced
   request must produce no reply plus exactly one throttled server WARNING.
4. **Host path:** run a listen-server host and repeat check 1 (**Q-2**).
5. **Dormant/virtualized groups (D12):** if any group in the session is dormant or far from the GM, select it
   and record whether a route is returned. This is an observation for `context.md`, not a pass/fail gate.
6. Watch the server log while clicking rapidly through twenty groups: **no** truncation warnings, **no**
   refusal spam, **no** errors.

**Step 3 — grep gates.** Run Q-4, Q-5, Q-6, Q-7, Q-8, Q-9, I-3, I-6 and paste the output into `context.md`.
They are cheap and they are the only check on properties no test can see.

---

## 9. Risks & Mitigation

**R1 — The cycle semantics are inferred, not proven, and getting them wrong is silent.**
Whether `AIGroup.GetWaypoints()` returns the cycle container or its expanded children, and whether
`GetCurrentWaypoint()` returns the container or the executing child, is inferred from vanilla's own reindex
code (`SCR_EditableGroupComponent.c:243-253`) rather than documented. Guess wrong and a GM sees a one-vertex
route for every patrol, with nothing in any log.
**Mitigation:** `Flatten` is written to be correct under **both** readings (a pass-through if cycles are
pre-expanded, a recursion if not), `currentIndex == -1` is a first-class no-highlight answer, the Phase 1
Campaign case pins the recursion against a real constructed cycle, Step 1 check 2 counts vertices by eye, and
the observed answer is recorded in `context.md` as the feature's durable finding.

**R2 — `Rpc()` arity mistakes compile clean and die silently at the wire (BUG-090).**
Four new send sites, each an untyped variadic call. The symptom — nothing draws — is indistinguishable from
"the GM has no permission" and from "the group has no waypoints".
**Mitigation:** hand arity-diff every call site against its handler and record the table in `context.md`
(a Phase 2 acceptance item); max arity 6; a temporary `Print` at the top of every `RpcDo_*` during Phase 2
bring-up so a missing frame is visible immediately rather than at play-test time.

**R3 — Fetch-once shows stale data, and someone files it as a bug.**
A QRF group's waypoints arrive on timers over the first minute (`OVT_QRFControllerComponent.c:389-392`), and
the current-waypoint highlight freezes at fetch time.
**Mitigation:** it is a settled user decision (§5 D3), the refresh gesture is reselection, and `context.md`
must state it in the triage section so the next reader recognises it as designed behaviour. If it proves
annoying in practice, the smallest honest change is a re-request on the seam's existing snapshot-poll invoker
— a follow-up, not a scope creep.

**R4 — A second SELECTED subscriber races or diverges from `hud-icons`.**
Two independent handlers on the same invoker could disagree about which entity is selected, showing one
group's text next to another's route.
**Mitigation:** both ignore the change sets and re-read the filter's current contents, taking `entities[0]` —
the convention is stated in §5 D9 and in both classes' headers, and I-4 tests it directly. Neither holds
mutable state the other can see.

**R5 — The drawing is illegible in practice.**
Thin lines through terrain at strategic altitude, over a busy world, with colours that do not read against
Everon's greens.
**Mitigation:** `NOZBUFFER` so terrain never hides a route, a thickness constant, circles at the vertices so
the polyline's shape is readable from directly above, and Step 1 checks 2/3/7 as the only real gate. The
constants are named on the class and are a one-line change if Phase 4 says so — **do not** pre-emptively build
an attribute surface for them.

**R6 — A late reply draws the wrong group's route.**
The GM clicks group A then immediately group B; A's fan arrives after B's request went out.
**Mitigation:** the route staging seq (independent of the snapshot poll's seq) drops every record from a
superseded request, and `RpcDo_WaypointsBegin` also carries the group `RplId` so the renderer can refuse a
route for a group it no longer has selected. Two independent guards, both cheap.

**R7 — Editing `OVT_GMRequestComponent.c` breaks a shipped consumer.**
Two features already read this seam in production.
**Mitigation:** additive-only is a phase rule and an acceptance item (I-1, read the diff); the only non-insertion
is one relocated statement in `OnEditorClosed`, called out explicitly in Phase 2 task 6 so it is reviewed
rather than discovered.

**R8 — `hud-icons` is code-complete but unverified in the same modded file.**
Both features insert into `SCR_EditModeEditorUIComponent.c`.
**Mitigation:** Phase 3 re-reads the file before editing and keeps its diff to pure insertions at the ends of
the two existing overrides; the epic's build order puts `hud-icons` first precisely so this is a sequence, not
a merge.

**R9 — A parallel session changes the seam, the spawn sites or the modded class.**
This tree receives concurrent bugfix commits; every Overthrow-side line number here is a snapshot of
`b01782c3`, taken while `hud-icons`' work is still uncommitted.
**Mitigation:** re-check `git status` and re-confirm the cited lines at every phase boundary (a Phase 0
acceptance item, repeated). The cited *method and class names* are the durable anchor; line numbers are a
convenience.

---

## 10. Dependencies

### Internal — all built, all read-only except the one seam file

| System | What is used | Where |
|---|---|---|
| gm-state seam | `IsAuthorizedGM`, `WIRE_VERSION`, `OVT_EGMRequestType`, the staging/commit pattern, `LogRefusal` | `Scripts/Game/Components/Controller/OVT_GMRequestComponent.c:133`, `:52`, `:9`, `:667-851`, `:368` |
| Controller request base | `ResolveOwningPlayerId()`, `ResolveEntity(RplId)`, `ShouldRespondLocally(int)` | `Scripts/Game/Components/Controller/OVT_ControllerRequestComponent.c:46`, `:89`, `:127` |
| Controller accessor | `OVT_ControllerComponent<T>.Get()` | `Scripts/Game/Components/Controller/OVT_ControllerComponent.c:36` |
| overthrow-panel injection point | the existing `modded class` and its two overrides | `Scripts/Game/UI/Modded/SCR_EditModeEditorUIComponent.c:31`, `:52` |
| hud-icons selection idiom | ignore the sets, re-read the filter, `entities[0]`, one deferred retry | `Scripts/Game/UI/GM/OVT_GMDetailUIComponent.c:196-236`, `:363-379` |
| Prefab-name helper | `OVT_Global.GetPrefabName(entity)` | `Scripts/Game/Global/OVT_Global.c` (usage precedent `OVT_BasePatrolUpgrade.c:207`) |
| Waypoint prefab table | the twelve resource names the classifier maps | `Prefabs/GameMode/OVT_OverthrowGameMode.et:69-82` |
| Route construction reference | how a perimeter patrol is actually built (4 patrol + 4 wait inside one cycle) | `Scripts/Game/GameMode/Managers/OVT_OverthrowConfigComponent.c:524-551` |
| Shape precedent | `ref Shape` lifetime, `ONCE`, `NOZBUFFER` | `Scripts/Game/Components/OVT_ParkingComponent.c:127`, `Scripts/Game/Controllers/OVT_TownController.c:56-82` |

### Base game — extended, never forked

`AIGroup.GetWaypoints()` / `GetCurrentWaypoint()` (`generated/AI/AIGroup.c:39`, `:38`),
`AIWaypointCycle.GetWaypoints()` (`generated/AI/AIWaypointCycle.c:32`),
`Shape.CreateLines` / `CreateLine` / `CreateCircle` (`Core/generated/Debug/Shape.c:37`, `:28`, `:74`),
`MenuRootSubComponent.GetMenu()` (`:23`), `MenuRootBase.GetOnMenuUpdate()` (`:72`),
`SCR_BaseEditableEntityFilter` (SELECTED). The rendering pattern is modelled on
`SCR_WaypointLinesEditorUIComponent.c` (`:21-88`, `:147-177`) without inheriting from it.

**One durable coupling this feature creates:** the twelve waypoint prefab **names**. Renaming or repointing a
prefab in `OVT_OverthrowGameMode.et` silently degrades that type to `UNKNOWN` — a neutral colour, not a
failure. The Campaign case classifies against real spawned prefabs rather than string literals, which is the
tripwire.

### Files modified outside this feature's own new files

`Scripts/Game/Components/Controller/OVT_GMRequestComponent.c` (additive wire),
`Scripts/Game/UI/Modded/SCR_EditModeEditorUIComponent.c` (~8 lines),
`Scripts/Game/GameMode/GM/OVT_GMRecords.c` (a two-line header pointer, optional), and — at Phase 5 —
`docs/features/gm/epic-overview.md`. **No** prefab, **no** layout, **no** `.meta`, **no** `.conf`, **no**
localization file, **no** persistence registration, **no** base-game file.

### Blocks / blocked by

- **Blocked by:** `gm-state` (built; MP play-test partially owed) at `b01782c3`.
- **Parallel-safe with:** `hud-icons` — one shared file (`SCR_EditModeEditorUIComponent.c`, pure insertions at
  the ends of two existing methods) and no shared GUID series. This feature mints **no** GUIDs at all.
- **Blocks:** nothing. `gm-map` may reuse the type enum and the record shape if it renders routes on the map
  screen, but does not depend on this feature landing.
- **Not a dependency:** the virtualization epic. §5 D12 explains why no dormancy-specific code is written.

---

*Sibling features: `gm-state` (the data spine this extends), `overthrow-panel` (owns the modded injection
point), `hud-icons` (shares the SELECTED filter and that file), `gm-map` (may reuse the records).
Epic: `docs/features/gm/epic-overview.md`.*
