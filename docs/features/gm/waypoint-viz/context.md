# Waypoint Viz — Context & Decisions

**Feature:** gm/waypoint-viz (epic `gm`, feature 4 of 5)
**Last Updated:** 2026-08-15 (Phase 5 complete — docs finalized)
**Current Phase:** Complete
**Status:** 🔴 ROOT-CAUSED, needs a rendering rework - the visual is built on `Shape.Create*`, which is **Workbench-only debug geometry** and paints nothing in a shipped client. Wire/walk/logic all positively confirmed good against a real dedicated server. Previously described as: Built + Workbench-verified; **BROKEN ON DEDICATED SERVER** — reopened 2026-08-23. The 2026-08-16 "dedicated-server GM client green" claim was RETRACTED by the user on 2026-08-23: dedi testing had not actually started yet. Routes draw in single-player/Workbench and draw NOTHING on a dedicated server — i.e. **D4's wire has never been observed working**, which is the entire reason it exists.

---

## Quick Status

**What's Done:**
- ✅ Plan (`implementation.md`, 6 phases 0–5, citations verified at `b01782c3`)
- ✅ Dev docs scaffolded
- ✅ Phase 0: baseline recorded
- ✅ Phase 1: `OVT_GMWaypointFormat` + `OVT_GMWaypointWalk` + Logic and Campaign cases (`compile-check.sh` exit 0, **6104 files**, +4 on the 6100 baseline; grep gates clean)
- ✅ Phase 1 regression gate: **All group 0 (190 tests, 52s)** — run by orchestrator 2026-08-15 ~22:40, includes the new Logic + Campaign cases

- ✅ Phase 2 (🔴 the wire): `GROUP_WAYPOINTS` + gated ask + 3-RPC owner fan + separate-seq route staging/commit +
  client contract trio, all **purely additive** into `OVT_GMRequestComponent.c` (`git diff`: 385 insertions,
  the single "deletion" being `CAMPAIGN_SNAPSHOT` → `CAMPAIGN_SNAPSHOT,` for the appended enum value — no
  existing signature, field, method or behaviour changed); `OVT_GMWaypointRecords.c` new.
  `compile-check.sh` exit 0, **6105 files** (+1). Arity table below.
- ✅ Phase 2 regression gate: **All group 0 (190 tests, 51s)** — run by orchestrator 2026-08-15 ~23:00

- ✅ Phase 3: `OVT_GMWaypointRenderer` (plain Managed, Shape API only) + the hookup in
  `SCR_EditModeEditorUIComponent.c` (**pure insertions**: one `ref` member, 6 lines at the end of
  `HandlerAttachedScripted`, a new `HandlerDeattached` override tearing down before `super`).
  `compile-check.sh` exit 0, **6106 files** (+1). Grep gates clean (below).
- ✅ Phase 3 regression gate: All group run 2026-08-15 ~22:56 — **exit 124 (300s timeout) but the artifacts
  are conclusive: 190/190 SUCCESS** in `autotest_succeded.log` (incl. both new GMWaypoint cases), zero FAIL
  lines; the client finished all tests then wedged at the post-test start menu instead of exiting. Phase 3
  code cannot run in the autotest world (the GM editor never opens), and the two prior runs this session
  exited cleanly in ~52s — read as an environment wedge, not a regression. If it recurs, investigate.
- ✅ Phase 4 Step 3 (grep gates, orchestrator 2026-08-15 ~23:10): **Q-4** 0 hits in shipped files (single
  `SetWaypoints(` at `OVT_TEST_Campaign_GMWaypointWalk.c:200` is the permitted fixture construction);
  **Q-5** 0; **Q-6** 0; **Q-7** 0 Broadcast / 0 `[RplProp]` (whole seam file); **Q-8** gate order verified
  `:500 ResolveOwningPlayerId` → `:503 IsAuthorizedGM` → `:511 ResolveEntity`; **Q-9** 0; **I-3**
  `OVT_Global.c` untouched (seam reached via `OVT_ControllerComponent<T>.Get()`); **I-6** diff touches no
  base-game path, no `Configs/`, exactly one modded class file (`SCR_EditModeEditorUIComponent.c`, 28 pure
  insertions) + the seam + the 4-line `OVT_GMRecords.c` header pointer

- ✅ Phase 5: docs finalized — triage chain, "no waypoints" answer, human-verification list, epic bookkeeping
  (`epic-overview.md` feature row + D13 upstream findings attributed to `occupying`)

**What's Next:**
- 🔴 **Phase 6 — rendering rework, `Shape` → `CanvasWidget`** (planned 2026-08-24, 9 tasks, `ui-developer`).
  See `implementation.md` §4 Phase 6 for D14–D16 and the full work list. Its last task discharges the
  reopened Phase 4 Step 2. Everything else about this feature is confirmed working on a real dedicated
  server; only the draw primitive changes.

**Superseded (kept for history):**
- ⏸️ Phase 4 Steps 1–2 — **deferred by the user 2026-08-15** to a later session. The drawing, the selection
  hookup, the mode-switch teardown, the dedicated-server path and the negative auth path are all still
  unobserved; see "Needs human verification" below for the exact list. The feature is **code-complete pending
  those eyes** — no further code work is planned or expected.

**Blockers:**
- None (the outstanding work is human observation, not implementation)

---

## Baseline (Phase 0)

Recorded 2026-08-15 22:25.

| Gate | Result |
|---|---|
| `tools/compile-check.sh` | ✅ exit 0 — `OK (6100 files, Game module, 8s)` |
| Git state | clean tree at `bc3d48a3` (plan citations taken at `b01782c3`; hud-icons now **committed** — R8 retired to a sequence, not a merge) |
| Highest bug id | BUG-174 (unchanged since planning) |
| GUID series | This feature mints **no** GUIDs; `{6B0A` braced grep = 0 hits across Prefabs/Configs/Scripts/UI/Language — stays reserved for `gm-map` |
| Seam citations | ✅ all resolve: `IsAuthorizedGM` :133, `WIRE_VERSION` :52, `OVT_EGMRequestType` :9, `RequestSnapshot` :300, `OnEditorClosed` :279; `ResolveEntity` :89 / `ShouldRespondLocally` :127 / `ResolveOwningPlayerId` :46 in `OVT_ControllerRequestComponent.c` |
| Base-game citations | ✅ `AIGroup.c:38-39`, `AIWaypointCycle.c:32`, `Shape.c:37/:46/:74`, `MenuRootBase.c:72` `GetOnMenuUpdate`. ⚠️ One path correction: `MenuRootSubComponent.c` lives at `scripts/Game/UI/Components/` (not `UI/Menu/`); `GetMenu()` at `:23` as cited |

---

## Key Files

### New (this feature)
- `Scripts/Game/GameMode/GM/OVT_GMWaypointFormat.c` — pure statics: type enum, `ClassifyPrefab`, `LegCount`, `IsHighlightLeg` (world-free, Logic-testable)
- `Scripts/Game/GameMode/GM/OVT_GMWaypointWalk.c` — server-only read-only: `Flatten` (cycle recursion), `Collect`
- `Scripts/Game/GameMode/GM/OVT_GMWaypointRecords.c` — `OVT_GMWaypointRecord` / `OVT_GMWaypointRoute`, plain Managed
- `Scripts/Game/Components/GM/OVT_GMWaypointRenderer.c` — Managed renderer, Shape API only, no widget
- `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_GMWaypointFormat.c`
- `Scripts/Game/Tests/TestSuites/Campaign/OVT_TEST_Campaign_GMWaypointWalk.c`

### Modified
- `Scripts/Game/Components/Controller/OVT_GMRequestComponent.c` — additive wire: `GROUP_WAYPOINTS`, ask + 3-RPC fan, route staging/commit, client contract trio (gm-state owns this file)
- `Scripts/Game/UI/Modded/SCR_EditModeEditorUIComponent.c` — ~8 lines hosting the renderer (overthrow-panel owns this file; hud-icons also inserts here)
- `Scripts/Game/GameMode/GM/OVT_GMRecords.c` — two-line header pointer (optional)

---

## Important Decisions

Settled in `implementation.md` §5 (D1–D13) — not re-derived here. The load-bearing ones:
- **D3** Fetch once at selection; **reselect to refresh** (stale current-waypoint highlight and late QRF waypoints are designed behaviour, not bugs).
- **D4** The wire extension is forced: vanilla waypoint prefabs have no `RplComponent` — server-only entities; local read would silently fail on dedicated servers only.
- **D6** Loop closes via one extra `CreateLine` wp[n−1]→wp[0]; `CreateLinesLoop` would close back to the *group*.
- **D9** Second independent SELECTED subscriber; both features re-read the filter and act on `entities[0]`. Never touch the panel (`ClearDetail` deletes hud-icons' widget tree).
- **D10** One live route + one staging twin; no keyed store.

---

## Wire (Phase 2) — arity diff table

Every `Rpc()` call site hand-diffed against its handler declaration, by eye, 2026-08-15 (BUG-090: `Rpc()` is
an untyped variadic prototype — a wrong argument count **compiles clean, passes every automated gate and dies
silently at the wire**). All line numbers are in
`Scripts/Game/Components/Controller/OVT_GMRequestComponent.c` as shipped by Phase 2.

| Call site (file:line) | Handler (decl line) | Declared params | Passed args (after the method ref) | Count | Match |
|---|---|---|---|---|---|
| `OVT_GMRequestComponent.c:431` `Rpc(RpcAsk_GroupWaypoints, requestType, m_iRouteSeq, groupRplId)` | `RpcAsk_GroupWaypoints` (:496) | `int requestType, int seq, RplId groupRplId` | `requestType`(int), `m_iRouteSeq`(int), `groupRplId`(RplId) | 3 / 3 | ✓ |
| `OVT_GMRequestComponent.c:916` `Rpc(RpcDo_WaypointsBegin, seq, WIRE_VERSION, groupRplId, count, currentIndex, flags)` | `RpcDo_WaypointsBegin` (:1163) | `int seq, int wireVersion, RplId groupRplId, int count, int currentIndex, int flags` | `seq`(int), `WIRE_VERSION`(int), `groupRplId`(RplId), `count`(int), `currentIndex`(int), `flags`(int) | 6 / 6 | ✓ |
| `OVT_GMRequestComponent.c:937` `Rpc(RpcDo_Waypoint, seq, index, pos, type)` | `RpcDo_Waypoint` (:1195) | `int seq, int index, vector pos, int type` | `seq`(int), `index`(int), `pos`(vector), `type`(int) | 4 / 4 | ✓ |
| `OVT_GMRequestComponent.c:953` `Rpc(RpcDo_WaypointsEnd, seq, sent)` | `RpcDo_WaypointsEnd` (:1213) | `int seq, int sent` | `seq`(int), `sent`(int) | 2 / 2 | ✓ |

**Local-call twins also diffed** (the `ShouldRespondLocally` short-circuit calls the same handlers directly, so
a typed compile error *would* catch those — they are listed for completeness, not because they can rot
silently): `:912` `RpcDo_WaypointsBegin(seq, WIRE_VERSION, groupRplId, count, currentIndex, flags)` 6/6 ✓;
`:933` `RpcDo_Waypoint(seq, index, pos, type)` 4/4 ✓; `:949` `RpcDo_WaypointsEnd(seq, sent)` 2/2 ✓;
`:429` `RpcAsk_GroupWaypoints(requestType, m_iRouteSeq, groupRplId)` 3/3 ✓.

**Properties held:** max arity **6** (matches the component's existing maximum, well inside the ≤8 rule);
**zero `array<>` parameters and zero object parameters** — records cross field-by-field as scalars, and a
hand-rolled bitstream was never an option (`ScriptBitWriter` hard-crashes on first use from script); every
response is `RplRcver.Owner` (grep: **0** `RplRcver.Broadcast`, **0** `[RplProp]` in the whole file); the one
new `RpcAsk_*` calls `IsAuthorizedGM` before it resolves the RplId to anything.

### Wire as shipped (Phase 2)

| RPC | Params | Arity | Direction |
|---|---|---|---|
| `RpcAsk_GroupWaypoints` | `int requestType, int seq, RplId groupRplId` | 3 | client→server (`RplRcver.Server`) |
| `RpcDo_WaypointsBegin` | `int seq, int wireVersion, RplId groupRplId, int count, int currentIndex, int flags` | 6 | server→owner |
| `RpcDo_Waypoint` | `int seq, int index, vector pos, int type` | 4 | server→owner |
| `RpcDo_WaypointsEnd` | `int seq, int sent` | 2 | server→owner |

**Route seq is independent of the snapshot seq.** `m_iRouteSeq` / `m_iRouteStagingSeq` / `m_bRouteStaging` are
a second, parallel set of staging fields; sharing `m_iSeq` would let the 8-second campaign poll silently
invalidate a route fan in flight. `IsStagingRouteRecord(seq)` is the route's own stale-discard rule.

**An empty answer is still an answer.** Past the gate every outcome sends `Begin(count 0, currentIndex -1) +
End(0)`: a group with no waypoints, an RplId that resolves to nothing, and an RplId that resolves to a
non-group. **Only an authorization failure is silent** (and throttled-logged server-side). The committed route
carries `m_bComplete = true` with an empty `m_aWaypoints`, which is what tells the Phase 3 renderer to stop
drawing the previously selected group rather than to keep it.

**Truncation.** `[Attribute] m_iMaxWaypointsPerGroup` defaults to **32**; on truncation the route carries
`FLAG_TRUNCATED` and the server logs ONE warning per `TRUNCATION_LOG_INTERVAL` (10 s) per player, naming the
group RplId, the sent count and the **true** count (re-walked once on that cold path under
`TRUNCATION_PROBE_CAP = 4096`). Real routes are ≤9, so this should never fire.

---

## Observed cycle semantics (Phase 1 finding)

**Status: assumed and coded for BOTH shapes; runtime observation deferred** to the orchestrator's suite run
and to Phase 4 (a running campaign with a real cycled patrol is the only place the answer is visible, and no
Phase 1 surface can see it — the Campaign case constructs its own cycle rather than observing a group's).

**What the generated API says (verified against the 1.8 reference tree):**
- `AIGroup.GetWaypoints(out array<AIWaypoint>)` returns `int`; `AIWaypointCycle.GetWaypoints(out array<AIWaypoint>)`
  returns `void`. Both are out-array signatures — neither returns an array.
- `AIWaypointCycle.PerformOn(AIGroup)` is documented as inserting *"the cycling waypoints (and itself)"* into the
  running group's waypoint list. So a group **executing** a cycle can plausibly report the expanded children
  **plus the container**, while a group that has only been handed the cycle reports the **container alone**.

**What the walk assumes (i.e. what it is correct under):**

| Engine behaviour | What `Flatten` does | What the GM sees |
|---|---|---|
| `GetWaypoints()` returns the **container** | expands it one level, drops the container, marks the route cyclic | the full 8-vertex patrol loop |
| `GetWaypoints()` returns the **expanded children** | pass-through, one entry each | the full route; not marked cyclic unless a container is also present, so the closing leg is simply not drawn |
| Returns **children + the re-inserted container** | container expanded once, children de-duplicated by the `Emit` identity guard, so each waypoint appears exactly once | the full route, marked cyclic |
| `GetCurrentWaypoint()` returns the **executing child** | identity match → `currentIndex >= 0` | current leg highlighted |
| `GetCurrentWaypoint()` returns the **container** | no identity match → `currentIndex = -1` | route drawn, nothing highlighted |

**Neither degradation prints anything and none of them can loop**: `MAX_DEPTH = 2` means a cycle among a
cycle's children is emitted as an entry (classifying `CYCLE`) rather than opened, and an `expanded` list makes
a self-containing cycle open at most once.

**What still has to be observed (Phase 4, one line in the triage):** select a real perimeter patrol and count
the vertices. **8** means the container shape (or the mixed shape with de-duplication working); **1** means the
expansion did not happen at all. That count is the whole observation.

---

## Visual constants as built (Phase 3)

All of them are named `protected static const` values on `OVT_GMWaypointRenderer` — **no `[Attribute]`**,
because the class has no prefab to tune them from and nobody is going to author them.

| Constant | Value | Why |
|---|---|---|
| Base colour | `Color.FromRGBA(255, 190, 60, 220)` — amber, slightly transparent | reads against grass, forest and road alike; packed **once** at `Attach()` into an `int` (`PackToInt()`, vanilla's own idiom at `SCR_WaypointLinesEditorUIComponent.c:153`) |
| Highlight colour | `Color.FromRGBA(60, 220, 255, 255)` — cyan, opaque | maximally distinct from amber for the current waypoint and the leg into it; also packed once |
| Line thickness | `2` (screen pixels — `Shape.c:37`'s last parameter) | thin enough not to smear a dense patrol loop, thick enough to see from a GM's altitude |
| Circle radius | `1.5` m, `Shape.CreateCircle` (`Shape.c:74`), slices `0` = engine default | vertices must be legible from the top-down camera; xz-plane so it reads as a ring on the ground |
| Shape flags | `ShapeFlags.VISIBLE \| ShapeFlags.NOZBUFFER \| ShapeFlags.ONCE` | `NOZBUFFER` draws through terrain (the only thing vanilla's canvas path was buying); **`ONCE` means shapes are re-issued per frame and never accumulate — which is why no `ref Shape` member exists anywhere in the class** |
| Point buffer | member `vector m_aPoints[33]` (1 group vertex + the 32-waypoint cap) | the draw path allocates nothing script-side per frame: the buffer is refilled in place and the whole route is **one** `CreateLines` call (a line strip) |

**Draw order, and why:** base polyline → cyclic closing leg (`CreateLine` from waypoint `n-1` to waypoint `0`,
**never** `CreateLinesLoop`, which would close back to the *group* vertex — §5 D6) → then one pass over the
waypoints drawing the highlight leg and the per-waypoint circle, so the highlight **overdraws** the polyline.
The closing leg is gated on `OVT_GMWaypointFormat.LegCount(count, IsCyclic()) > count` rather than on a
re-derived condition, and the highlight is selected by `OVT_GMWaypointFormat.IsHighlightLeg(i, currentIndex)` —
the geometry rules live in the Logic-tested file, not inline in the renderer.

**The group vertex is live, the waypoints are a snapshot.** Vertex 0 is `ResolveEntity(route.m_GroupRplId).GetOrigin()`
re-read every frame. If the group stops resolving the chain simply starts at waypoint 0 (`offset` drops from 1
to 0 and every index shifts with it) rather than drawing a leg to the world origin.

**Bail conditions (all silent, all drawing exactly zero shapes):** no seam; `GetRoute()` null or
`!HasRoute()` (which covers "not complete yet" and "complete with zero waypoints"); or
`route.m_GroupRplId != m_RequestedGroupRplId`, which is how a fan that lands after the GM moved on is
discarded rather than drawn.

---

## Testing

- **Logic:** `OVT_TEST_Logic_GMWaypointFormat` — classifier (incl. Defend/Patrol ordering trap), leg count, highlight rules.
  **Prove-can-fail method (2026-08-15):** the ordering-trap expectation was temporarily inverted in place —
  `ExpectClass("AIWaypoint_Defend_ConflictBaseTeamPatrol.et", OVT_EGMWaypointType.PATROL, …)` — and
  `compile-check.sh` re-run: **exit 0, 6104 files**, i.e. the inverted case reaches its `SetFailure`
  ("…classified as DEFEND, expected PATROL") at runtime rather than failing to build or passing silently.
  Reverted immediately; the shipped file expects **DEFEND**. All `SetFailure` calls carry ≤2 format args.
- **Campaign:** `OVT_TEST_Campaign_GMWaypointWalk` — constructed cycle fixture (4 patrol + 4 wait inside one
  `AIWaypointCycle`, no group and no AI), plus the `max = 3` truncation case.
  **Prove-can-fail method (2026-08-15):** the `AIWaypointCycle.Cast` branch was temporarily deleted from
  `OVT_GMWaypointWalk.Flatten`, leaving the plain `Emit` path to handle every element; `compile-check.sh`
  re-run on the mutilated walk: **exit 0, 6104 files** (it compiles, so the case would really run against it).
  With that branch gone the fixture's single source element — the container — is emitted as itself, so case 1
  goes red on its first assertion with `"Flattening one cycle holding 8 waypoints gave 1. Flattened 1
  waypoint(s); first entry is AIWaypointCycle (a count of 1 naming AIWaypointCycle means the cycle was never
  opened)"` and `cyclic` false. That message is deliberately distinguishable from the other failure mode,
  `"The flattened list is EMPTY - the fixture spawned nothing"`. The branch was restored and the compile
  re-verified. *(Suites themselves are the orchestrator's to run — `.claude/test-policy.md`; the runtime
  failure text above is derived by inspection of the mutilated build, not from a suite run.)*
- **Suites run by the orchestrator only** after a phase completes (`.claude/test-policy.md`); implementation agents stop at `compile-check.sh` exit 0.
- What only a human can verify: legibility, colours, auth under a real role, listen vs dedicated, hud-icons coexistence, vanilla waypoint editing non-interference → Phase 4, batched in one session.

---

## Triage — "the GM sees no lines"

Work the chain **in order**; each step rules out everything above it. The whole feature is silent by design
(Q-1), so *nothing in the log* is the normal state and is **not** evidence of a fault.

**1. Is the selected entity actually a `GROUP`?**
The renderer takes `entities[0]` from the editor's SELECTED filter and clears the route for any editable type
other than `GROUP` (and for a group whose owner has no replicated `RplComponent`). Cross-check against the
`hud-icons` panel: with both features active they read the *same* filter and the *same* `entities[0]`, so the
panel's detail section names the entity the renderer is drawing for. If the panel shows a town, a base or a
vehicle, the selection is not a group and nothing will ever be drawn.

**2. Was the route fetched *before* the waypoints existed?**
The fetch is **once, at selection** (§5 D3). Waypoints assigned to the group after that moment — a QRF being
re-tasked, a fresh patrol order — are invisible until the group is **re-selected**. Likewise the current-waypoint
highlight is a snapshot and goes stale as the group advances. This is designed behaviour, not a bug: click empty
ground, then click the group again. If re-selecting makes lines appear, triage is finished.

**3. Did a `WaypointsBegin` ever arrive on the client?**
Add a temporary `Print` at the **top of each** of `RpcDo_WaypointsBegin` / `RpcDo_Waypoint` / `RpcDo_WaypointsEnd`
in `Scripts/Game/Components/Controller/OVT_GMRequestComponent.c` and re-select.
- **No `Begin` at all** → the ask never reached the server, or the server refused it → step 4.
- **`Begin` with `count 0`** → the server answered honestly and the group has no waypoints → step 5.
- **`Begin` with a count, but no lines** → the fan arrived after the GM moved on (the route's `m_GroupRplId`
  no longer matches the renderer's `m_RequestedGroupRplId`, and it is discarded rather than drawn), or the
  `WIRE_VERSION` mismatched (that one *does* log, once per session, from gm-state).

**4. Did the server gate refuse?**
**Authorization failure is the one silent-to-the-client outcome** — the client gets no RPC whatsoever. Look at
the **server** log for the throttled `WARNING` from `LogRefusal` naming the player. A refusal means the client is
not an authorized GM (no admin/role, no `-ovtGmDev`), so the fault is authorization, not visualization.

**5. Does the group have any waypoints at all?**
A group with no waypoints **draws nothing and prints nothing — by design (F-5).** The server still answers:
`Begin(count 0, currentIndex -1)` + `End(0)`, committed as a complete route with an empty waypoint array, which
is precisely what tells the renderer to *stop drawing the previously selected group* rather than to keep its
lines on screen. So "I select a group and everything disappears" is the correct, intended appearance of an
empty route. Confirm independently — e.g. a static garrison group genuinely has none — before suspecting code.

**6. Lines appear, but the shape is wrong — count the vertices on a perimeter patrol.**
Select a town or base perimeter patrol (the shape whose real route is a cycle of 8):
- **8 vertices, closed loop** → cycle expansion is working (either the container shape or the mixed shape with
  the `Emit` de-duplication doing its job).
- **1 vertex** → the cycle was never opened; `Flatten` emitted the `AIWaypointCycle` container as a single
  entry. That is the R1 failure mode and the only observation Phase 1 could not make. See "Observed cycle
  semantics" above for the branch table.
- **A route drawn but nothing highlighted** → `GetCurrentWaypoint()` returned the container rather than the
  executing child; no identity match, `currentIndex = -1`. Degraded, not broken.

**7. Still nothing → it is a seam problem, not a renderer problem.**
The renderer owns no networking; if no route ever commits, the fault is upstream in gm-state. Continue with the
triage in `docs/features/gm/gm-state/context.md` (seam ownership, `IsLocalControllerOwner`, editor-close
teardown, `WIRE_VERSION`).

### What does a GM see when a group has **no** waypoints?

**Nothing — and that is correct.** No lines, no circles, no stub marker, no placeholder, and **no log line on
either the client or the server.** The server still sends a real answer (`Begin(0, -1)` + `End(0)`); the committed
route is `m_bComplete = true` with an empty `m_aWaypoints`; the renderer's `HasRoute()` bail then draws exactly
zero shapes, which also **clears any route still on screen from the previously selected group**. Silence here is
the feature working (F-5 / Q-1), not a failure to respond.

---

## Needs human verification (Phase 4 Steps 1–2, deferred 2026-08-15)

The feature is **code-complete**: every automated gate is green (`compile-check.sh` exit 0 at 6106 files, all
Phase 4 Step 3 grep gates clean, the full suite 190/190 three times) and **nothing else can be proved without
human eyes**. No suite can open the GM editor, and no assertion can look at a line. The user chose to run these
checks in a later session; until then treat every item below as *unobserved*, not *working*.

| # | Check | What proves it |
|---|---|---|
| 1 | **Workbench Play mode, host path** — §8 **F-1…F-8**, **Q-3**, **I-4**, **I-5** | Route drawn group→wp1→…; cycled patrol closes; current leg in cyan; selection change/deselect/editor close/Photo mode all clear it; empty group draws nothing; lines readable through terrain from camera altitude; re-select refreshes (D3); 5× open/close leaves no duplicate drawing or stray subscription (temporary `Attach`/`Detach` `Print` pair matching 1:1); hud-icons panel and the route describe the same entity; base-game waypoint assign/move/delete still behaves |
| 2 | **Dedicated server, GM client** | The single check the whole wire exists for — vanilla waypoint prefabs carry no `RplComponent`, so a local read would fail **only** here (D4). Record which auth path was used; that also discharges gm-state's Phase 5 authorization debt |
| 3 | **Negative auth path (F-7)** | Non-admin client without `-ovtGmDev` receives **zero** `RpcDo_Waypoint*` of any kind (temporary client `Print`) while an authorized GM on the same server selects groups; a coerced request produces exactly one throttled server `WARNING` from `LogRefusal` |
| 4 | **Listen-server host (Q-2)** | Host opens GM, selects a group, sees the route — the `ShouldRespondLocally` short-circuit's reason to exist |
| 5 | **Dormant / virtualized group (D12)** | Select a dormant or distant group: either its route draws, or nothing draws. Either is acceptable; an **error** is not. No dormancy-specific code was written on purpose |
| 6 | **R1 — cycle expansion vertex count** | The one durable engine finding this feature was never able to make: select a real perimeter patrol and count vertices. **8** = expansion works; **1** = the cycle was never opened. See "Observed cycle semantics" for what each shape implies |
| 7 | **No log spam** | 20 rapid selections produce no repeated output (the dedupe key + throttles) |

Also unverified by anything automated: colour legibility against grass/forest/road, line thickness at GM camera
altitude, and coexistence look-and-feel with hud-icons.

---

## Session Notes

### 2026-08-23 - REOPENED: no routes on a dedicated server; temporary trace added
- **The 2026-08-16 "dedicated-server GM client green" record was RETRACTED by the user** - dedi testing had
  not begun. D4's wire has therefore never been observed working; the earlier MP row was recorded in error.
- **Symptom:** GM on a dedicated server sees no waypoint lines at all. Single-player/Workbench is fine.
- **Halved by one observation (user, 2026-08-23): the Overthrow panel and the hud-icons tooltips DO show
  live data on the same dedicated server, authorized by admin login.** That proves, on the real server:
  `IsAuthorizedGM`, `ResolveOwningPlayerId`, `IsLocalControllerOwner`/`OVT_Global.GetController()`, the
  client->server ask direction, the server->owner fan direction and `WIRE_VERSION` are all GOOD. The fault
  is isolated to the waypoint fan.
- **Static analysis found no defect.** Every element of the waypoint path has a proven-working twin in the
  same component: 6-arg arity (`RpcDo_Deployment`), `RplId` as a payload (`RpcDo_Deployment`/`RpcDo_Group`),
  `vector` as a payload (proven across ~20 Overthrow RPCs and vanilla). Editable entities are engine-checked
  to carry an `RplComponent` (`SCR_EditableEntityComponent.c:2237-2253`), so the client CAN name the group.
- **Prime remaining suspect:** `RpcAsk_GroupWaypoints` performs the **only client->server `RplId`
  resolution in the entire seam** - the snapshot only ever sends RplIds server->client. If
  `ResolveEntity(groupRplId)` answers null on the server, the group casts null, the walk collects 0, and the
  honest empty answer (`Begin(0,-1)` + `End(0)`) renders as **exactly nothing** - indistinguishable from the
  designed "group has no waypoints" appearance. That is the shape of this bug.
- **Temporary `[OVT-WPVIZ]` trace added** (compile-check OK, 6341 files) on both sides:
  `OVT_GMWaypointRenderer.OnSelectionChanged`/`OnRouteUpdated`, and in `OVT_GMRequestComponent`
  `RequestGroupWaypoints`, `RpcAsk_GroupWaypoints` (arrival / player / auth / **RplId resolution**),
  `SendGroupWaypoints` (walk result) and the three client `RpcDo_Waypoint*` handlers.
  **These MUST be stripped before this feature closes again** - `grep -rn "OVT-WPVIZ" Scripts/` must be 0.
- **Round 1 logs (2026-08-23, dedicated server + client) - THE WIRE IS COMPLETELY INNOCENT.** Server:
  `ask ARRIVED seq 1` -> `RplId -2147482596 resolved to entity 1, AIGroup cast 1` -> `walked group -> count 5,
  currentIndex 0, flags 1` (CYCLIC). Client: `sending ask seq 1 (isServer 0)` -> `Begin ARRIVED count 5` ->
  five `Waypoint ARRIVED` with real world positions -> `End ARRIVED sent 5` -> `route committed - waypoints 5,
  currentIndex 0, complete 1`. **D4's wire demonstrably works on a dedicated server.** The prime suspect above
  (client->server RplId resolution) is CLEARED - it resolved first try.
- **The fault is therefore in the CLIENT DRAW PATH, with a fully valid route in hand.** At draw time
  `HasRoute()` should be true (complete + 5 waypoints) and `route.m_GroupRplId` should equal
  `m_RequestedGroupRplId` (both -2147482596, and `OnRouteUpdated` re-syncs it). Ruled out by reading:
  `m_Route` is only cleared by `OnEditorClosed` and by a new request (`:391`, `:446`) - the 8-second snapshot
  poll does NOT touch it; `m_aPoints[33]` is big enough for 5+1; `m_eFlags` is
  `VISIBLE|NOZBUFFER|ONCE`. The renderer was definitely constructed and attached, because
  `HandlerAttachedScripted` returns early when `GetMenu()` is null and `OnSelectionChanged` demonstrably fired.
- **Round 2 trace added** (compile-check OK, 6341 files): `Attach` reports whether it got a menu;
  `OnStateCleared` announces a spurious dedupe-key reset; `OnMenuUpdate` reports, throttled to ~once per 2 s,
  which gate it exits at (no seam / `HasRoute()` false / id mismatch) or that it is DRAWING and with how many
  points. Grep the client log for `OVT-WPVIZ] draw:` - **absence of those lines is itself the answer** (the
  per-frame hook is not ticking).
- **Round 2 logs: `draw: DRAWING 3 points (offset 1, groupEntity 1), first <7016.82,96.86,4686.44>` every
  frame, forever.** The renderer ticks, `HasRoute()` is true, the ids match, the group entity resolves and
  valid world coordinates go into `Shape.CreateLine`. **Every line of this feature's logic is correct.**

### 2026-08-23 - ROOT CAUSE: `Shape` is DEBUG-ONLY geometry and does not render in a shipped client
- **User's diagnosis, and it is right: `Shape.Create*` draws only in Workbench.** The API lives in
  `scripts/Core/generated/`**`Debug`**`/Shape.c`. In a real client the calls succeed, return a `ref Shape`,
  log nothing, and paint nothing. Corroboration: essentially **every** vanilla `Shape.Create` call site is
  inside `#ifdef WORKBENCH` / `ENABLE_DIAG` / a DiagMenu gate, and the one notable ungated user,
  `SCR_WaypointLinesEditorUIComponent.c`, **also carries a `CanvasWidget` path** - BI knows the Shape half
  is debug-only.
- **So "works in single player" meant "works in Workbench".** Phase 4 Step 1 only ever exercised Workbench
  Play mode, and no automated gate can see this: it compiles, runs, and silently draws nothing.
  **A per-frame "I am drawing" trace that still shows nothing on screen means the DRAW PRIMITIVE is wrong,
  not the data** - that is the reusable lesson, saved to memory as `shape-debug-lines-workbench-only`.
- **This is not a bug in the wire, the walk, the classification or the renderer's logic** - all four are now
  positively confirmed working against a real dedicated server, which is more than the feature could claim
  before. It is the choice of draw primitive, made in plan 3.4 / Phase 3, that is invalid for shipped builds.
- **The fix is a rendering rework**, not a patch: project each vertex with
  `workspace.ProjWorldToScreenNative(worldPos, world)` and emit `LineDrawCommand`s into a `CanvasWidget`
  (`canvas.SetDrawCommands(...)`), the way vanilla's cycle-arrow branch does. Consequences to face when it is
  planned: the renderer stops being widget-free (plan 3.4's "owns no widget and needs no layout" is dead, and
  a layout + `.meta` + GUID may now be needed - **re-grep `{6B0A` before allocating**, that series is
  reserved for gm-map); `SetDrawCommands` **replaces** the whole list so the canvas must not be shared with
  vanilla's own writer; behind-camera and off-screen vertices need culling that 3D shapes gave for free;
  and terrain occlusion is gone (acceptable - `NOZBUFFER` was already the intent). D6's closing-leg rule and
  the highlight-leg logic carry over unchanged.
- Temporary `[OVT-WPVIZ]` trace **stripped** (`grep -rn "OVT-WPVIZ" Scripts/` = 0, compile-check OK 6341
  files). Note: commit `c525181d` "(fix) various dedi bugs from testing" **captured the trace** before it
  was removed; the removal is a working-tree change on top of it.


### 2026-08-15 — Phase 3 built (renderer + selection hookup)
- `Scripts/Game/Components/GM/OVT_GMWaypointRenderer.c` (new, plain `Managed`, no widget/prefab/layout/GUID):
  `Attach(MenuRootBase)` / `Detach()` / `SubscribeFilter()` + `RetrySubscribeFilter()` / `OnSelectionChanged()` /
  `OnRouteUpdated()` / `OnStateCleared()` / `ClearRoute()` / `OnMenuUpdate(float)` / `GetEntityRplId()` /
  `ResolveEntity()`. Zero `Print` in the file.
- **Hookup is pure insertions** in `SCR_EditModeEditorUIComponent.c`: a `protected ref OVT_GMWaypointRenderer`
  member, 6 lines at the end of `HandlerAttachedScripted` (after `super` **and** after panel creation, with a
  `GetMenu()` null-check), and a **new** `HandlerDeattached(Widget)` override that detaches the renderer
  *before* calling `super`. No existing line moved or reordered; **no `IsLimited()`/role check added** — hosting
  in this class is itself the GM gate (file header :8-11).
- **Teardown is symmetric and deliberate.** Everything `Attach()` subscribes — `GetOnMenuUpdate()`, the SELECTED
  filter's `GetOnChanged()`, the seam's `GetOnRouteUpdated()` and `GetOnStateCleared()` — is removed in
  `Detach()`, along with the queued `CallLater` retry if it has not fired. The filter, the menu and the seam all
  outlive the renderer.
- **The SELECTED filter retry is one-shot, not a poll.** `SubscribeFilter()` queues `CallLater(RetrySubscribeFilter, 0)`
  exactly once (the flag is held *up* across the retry so the retry's own failure cannot queue a third attempt)
  and then gives up silently. The editor's entity manager initialises around the UI, so an absent filter on the
  attach frame is normal; an absent filter on the retry frame means this session simply draws nothing.
- **Nothing is cached but the group id.** The seam is re-resolved via
  `OVT_ControllerComponent<OVT_GMRequestComponent>.Get()` at every use (the `OVT_GMPanelUIComponent.c:21` rule —
  it is null on a dedicated server and before ownership assignment, and a cached null would never heal), and the
  route object is re-read from the store each frame. The single cached value is `m_RequestedGroupRplId`: the
  box-select/rapid-click dedupe key **and** the test a late fan must pass before it is drawn. `OnRouteUpdated()`
  re-syncs it with whatever actually committed, so a second consumer of the shared store cannot make this
  renderer believe a foreign route is its own.
- **Selection convention followed exactly:** the inserted/removed sets are ignored, `filter.GetEntities()` is
  re-read into a reused member `set`, and `entities[0]` decides. Non-`GROUP` (or no replicated `RplComponent`)
  → `ClearRoute()` (store cleared + dedupe key invalidated) and return.
- Gates: `compile-check.sh` **exit 0, 6106 files** (+1 on 6105). Greps over the renderer + `OVT_GMWaypointFormat.c`
  + `OVT_GMWaypointWalk.c`: `Rpc(` 0, `[RplProp]` 0, `RplRcver` 0, `SetDrawCommands(` 0, `ClearDetail(` 0,
  `GetDetailSlot(` 0, `ShowDetail(` 0, `SCR_EditableWaypointComponent` 0, `Print` 0.
- **Not observed:** no suite can open the editor or look at a line — everything visual, the mode-switch teardown
  and the dedicated-server path are Phase 4's to see.

### 2026-08-15 — Phase 1 built
- `OVT_GMWaypointFormat.c`: `OVT_EGMWaypointType` + `ClassifyPrefab` / `Stem` / `LegCount` / `IsHighlightLeg`.
  **Classification is exact-stem, not contains** — the GUID, the directories and the extension are stripped and
  the result is compared for equality, so the Defend/Patrol trap cannot exist regardless of branch order (a
  contains-chain would only be safe as long as nobody reordered it). Grep-clean of `GetGame()`/`OVT_Global`
  including comments, in both the file and its Logic test.
- `OVT_GMWaypointWalk.c`: `Flatten` + `Collect` + a private `Emit`. Bounded by construction — `MAX_DEPTH = 2`
  (a nested cycle is emitted as an entry, never opened), an `expanded` list so a self-containing cycle opens
  once, and an identity de-duplication in `Emit` so the container `PerformOn` re-inserts cannot double a vertex.
  Zero mutating waypoint APIs; the header's "no SetWaypoints" line is deliberately written without parentheses
  so the Q-4 grep (`SetWaypoints(`) stays a real gate rather than matching prose.
- Tests: 12 prefab classifications + trap + prefix invariance + unknown/empty + 5 leg counts + 3 highlight
  assertions (Logic); constructed 8-waypoint cycle + truncation at `max = 3` (Campaign), fixture rebuilt and
  torn down per case, every spawned entity deleted on every exit path including failures.
- **Not observed yet:** the real `GetWaypoints()` shape on a live cycled group — see "Observed cycle semantics".

### 2026-08-16 — Phase 4 closed: MP verification green — feature COMPLETE (29/29)
- ⚠ **RETRACTED 2026-08-23 — THIS ENTRY WAS WRONG.** The user reports dedi testing had not begun on 2026-08-16, so nothing below about the dedicated server was actually observed. Treat the whole MP row as UNVERIFIED. Original (false) text follows:
- ~~**User confirmed: dedicated-server GM client works and the non-admin negative path is green**~~ — the two
  checks the wire extension exists for (D4/F-7). Combined with the earlier Workbench rounds, Phase 4 Steps
  1–2 are discharged and the feature is 29/29.
- Not individually attested (minor, recorded for honesty, no action planned): listen-host Q-2 (the
  self-hosted case — `ShouldRespondLocally` short-circuit; exercised implicitly in every Workbench Play
  session, where routes drew), the explicit 8-vertex count on a perimeter patrol (R1 cycle-semantics
  observation — routes and loops visibly drew across sessions, so the walk works in practice), the
  dormant-group observation (D12), and which auth path the MP test used (gm-state's lingering bookkeeping
  item, still unrecorded).
- Remaining open item from this feature's verification: **BUG-175** (vanilla cycled-group waypoint
  assignment crash) — owned by epic Phase 3, workaround offer open.

### 2026-08-16 — Phase 4 third round: render fixes confirmed; prints stripped; BUG-175 filed
- **User confirmed both render fixes** (solid lines all the way round, solid blue current leg). The
  `[OVT-WPVIZ]` triage prints are **stripped** (grep 0, compile clean, 6106 files). Workbench-host checks
  F-1/F-3/F-4-partial are effectively discharged by this session; remaining human checks: F-2 8-vertex loop
  count (probably seen but not explicitly counted), F-5–F-8, Q-3, I-4, MP Step 2.
- **BUG-175 filed (high):** assigning a GM waypoint to a *cycled* group throws
  `ScriptInvoker: Recursive call of Invoke!` — pure vanilla recursion
  (`SCR_EditableGroupComponent.OnChildEntityChanged` re-enters `AddWaypoint(m_CycleWaypoint)` inside
  `Event_OnWaypointAdded`), zero Overthrow frames, zero Overthrow subscriptions (grep-verified). Every
  Overthrow perimeter patrol is cycled, so I-5 ("vanilla waypoint editing unaffected") is **failed by the
  base game itself**, not by this feature. Decision (workaround-mod vs upstream RFG report) parked with
  the epic's Phase 3 cleanup; linked to this feature for discovery only.

### 2026-08-16 — Phase 4 second round: route draws; two render fixes
- **User confirmed the route draws** after the `GetAIGroup()` fix. Two visual defects reported and fixed:
  1. **"Flashing" current waypoint/leg** — the cyan highlight leg was drawn OVER the amber base strip
     (plan §4 Phase 3 task 4 said "drawn after so it overdraws"); two coincident 3D lines z-fight per
     frame, which reads as flashing. Fix: each leg is drawn exactly once in its final colour — no overdraw.
  2. **Base polyline did not render at all** (only the highlight leg + circles were visible — "circles but
     no lines between them"). `Shape.CreateLines` fed from a **member** fixed array (`vector m_aPoints[33]`)
     with `num < declared size` rendered nothing, while `CreateLine`/`CreateCircle` in the same frame drew
     fine. Vanilla only ever passes a **local** fixed array sized exactly to `num`
     (`SCR_AIWorld.c:241`: `vector points[19]` + `num 19`). Root cause unproven (member-array marshalling
     vs num<size — not separated); the fix sidesteps it entirely.
- **Divergence from plan §1 fact 2 / §5 D6:** the route is now **per-leg `CreateLine` calls** (≤33 tiny
  native calls for the one selected group), not one `CreateLines` strip. The plan's "whole route is one
  native call" is retired by observed engine behaviour; the closing-leg rule and "never `CreateLinesLoop`"
  are unchanged.

### 2026-08-16 — Phase 4 first user test: fix + triage prints
- **User report: "nothing at all with a selected group in GM"** (fresh Workbench, new scripts confirmed
  loaded via triage print). Root cause found by the `[OVT-WPVIZ]` triage prints:
  **clicking a soldier in the world selects its CHARACTER editable (type 2), not the GROUP** — the GROUP
  editable is only directly selected via its icon. The plan's `entities[0]`-is-a-GROUP assumption was wrong
  for the click-a-soldier path.
- **Fix:** `OnSelectionChanged` now resolves the selection through vanilla's own
  `SCR_EditableEntityComponent.GetAIGroup()` (GROUP → itself, CHARACTER → parent group, VEHICLE → AI
  occupants' group, else null — `SCR_EditableCharacterComponent.c:607`) before the GROUP type check.
  Free UX win: selecting any soldier of a patrol (or a crewed vehicle) now draws its group's route.
- ⚠️ **Temporary `[OVT-WPVIZ]` triage prints are still in** (renderer attach/selection/request; seam server
  ask + route commit) — remove after Phase 4 verification passes.
- Note for hud-icons/D9: the "both features read entities[0]" convention still holds — hud-icons classifies
  the raw selection for its tooltip, waypoint-viz *resolves* it to a group; they still describe the same
  clicked entity.

### 2026-08-15 23:20 — autorun complete
- Full /autorun-feature run (Discord): Phases 0–3 + 5 built and gated in one session; Phase 4 Steps 1–2
  deferred by the user to a later session ("finish the docs now, I'll verify later").
- Gates: compile 6100 → 6106 files across three phases; All group **190/190 green** after Phases 1 and 2;
  after Phase 3 the run timed out at client shutdown but artifacts show 190/190 SUCCESS (environment wedge,
  see Phase 3 gate note). All §8 Step-3 greps pass.
- `implementation.md` Status → **Ready for Review**. Changes left **uncommitted** on `v1.5` per the
  no-git rule; the user owns branching/committing.
- Next session: run the "Needs human verification" list (Workbench host + MP checks), then record results
  here and flip the epic row to user-verified.

### 2026-08-15 22:20
- Feature started via /autorun-feature (Discord). Docs scaffolded from the existing plan.
- Tree clean at `bc3d48a3` — hud-icons committed since planning (`b01782c3`), so the shared-file risk R8 is now sequence, not merge.
- Next: Phase 0 baseline gates.

---

*Update this file at the end of each work session.*
