# Waypoint Viz — Task Checklist

**Feature:** gm/waypoint-viz (epic `gm`, feature 4 of 5)
**Last Updated:** 2026-08-24 (Phase 6 planned — rendering rework)
**Progress:** 28/38 (Phase 6 planned 2026-08-24, 0/9) - ROOT-CAUSED 2026-08-23, needs a Phase 6 rendering rework. The visual is built on `Shape.Create*`, which is **Workbench-only debug geometry**: it paints nothing in a shipped client. Wire, walk, classification and renderer logic are all now positively confirmed good against a real dedicated server. (Phase 4 Step 2 REOPENED 2026-08-23) — Workbench render rounds green; the **dedicated-server pass never happened** (user retracted the 2026-08-16 claim on 2026-08-23) and routes **do not draw on a dedicated server**

> **Phases 0–3 and 5 stand: the wire, the walk, the classification and the renderer's logic are all
> positively confirmed working against a real dedicated server (2026-08-23 trace).** What does not work is
> the draw primitive — `Shape.Create*` is Workbench-only debug geometry. **Phase 6 replaces it with a
> `CanvasWidget`**; Phase 4 Step 2 is reopened and is discharged by Phase 6's final task.

> Agent tiers per `implementation.md` §4: **no phase is routed to an advanced agent.** Phase 0 → orchestrator
> (no agent); Phase 1 → `component-developer`; Phase 2 → `network-specialist` (highest-risk phase — additive
> only into the seam; `/proceed-advanced` is a sensible opt-in here, nothing else warrants it); Phase 3 →
> `component-developer` (deliberately not `ui-developer` — no layout/widget/`.meta`/GUID); Phase 4 →
> user-driven; Phase 5 → `component-developer`.

---

## Phase 0: Baseline (3/3 complete) — S, no agent

- [x] ✅ **Record baseline in context.md**
  - Description: `tools/compile-check.sh` exit 0 + file count; `git status` / `git rev-parse --short HEAD` (plan cited `b01782c3`; re-check at every phase boundary — concurrent bugfix sessions commit to this tree); highest allocated bug id (`ls docs/bugs/` — BUG-174 at planning)
  - File(s): `docs/features/gm/waypoint-viz/context.md`
  - Estimate: 🟢

- [x] ✅ **Verify seam + base-game citations resolve**
  - Description: `OVT_GMRequestComponent.c` `IsAuthorizedGM`/`WIRE_VERSION`/`OVT_EGMRequestType`/`RequestSnapshot`/`OnEditorClosed`/`IsStagingRecord` (~:133/:52/:9-12/:300/:279/:848); `OVT_ControllerRequestComponent.c:89` `ResolveEntity`, `:127` `ShouldRespondLocally`; base game `AIGroup.c:38-39`, `AIWaypointCycle.c:32`, `Shape.c:37`, `MenuRootSubComponent.c:23`, `MenuRootBase.c:72`, `SCR_WaypointLinesEditorUIComponent.c:53`
  - File(s): n/a (read gate)
  - Estimate: 🟢

- [x] ✅ **Confirm no-GUID stance**
  - Description: This feature mints **no** GUIDs. Record in context.md that `{6B0A…}` (braced grep!) stays reserved for `gm-map`. If a later phase thinks it needs a GUID, re-grep with the brace first
  - File(s): `docs/features/gm/waypoint-viz/context.md`
  - Estimate: 🟢

---

## Phase 1: Classification, route geometry, server-side walk (6/6 complete) — M, `component-developer`

- [x] ✅ **OVT_GMWaypointFormat.c — enum + pure statics (world-free)**
  - Description: `enum OVT_EGMWaypointType` (UNKNOWN…CYCLE); `ClassifyPrefab(string)` matching the 12 prefab stems from `OVT_OverthrowGameMode.et:69-82` — ⚠️ most-specific-token-first (`AIWaypoint_Defend_ConflictBaseTeamPatrol` contains "Patrol" and must classify DEFEND); unknown/empty → UNKNOWN; `LegCount(int,bool)` + `IsHighlightLeg(int,int)` per §3.4. **Zero `GetGame()`/`OVT_Global` incl. comments** (Logic-tier grep guard)
  - File(s): `Scripts/Game/GameMode/GM/OVT_GMWaypointFormat.c` (new)
  - Estimate: 🟡

- [x] ✅ **OVT_GMWaypointWalk.c — server-only read-only walk**
  - Description: `Flatten(src,max,out flat,out cyclic,out truncated)` — replace `AIWaypointCycle` elements with children (`AIWaypointCycle.c:32`), recursion depth cap 2, never re-enter an emitted element, honour max + truncated; `Collect(group,max,…)` — `GetWaypoints()` → Flatten → `currentIndex` by identity vs `GetCurrentWaypoint()`, −1 if absent. **Never a world scan** (header states why — leaked detached waypoints); zero mutating waypoint APIs
  - File(s): `Scripts/Game/GameMode/GM/OVT_GMWaypointWalk.c` (new)
  - Estimate: 🟡

- [x] ✅ **Record observed cycle semantics in context.md**
  - Description: Does `AIGroup.GetWaypoints()` return the cycle container or expanded children? Does `GetCurrentWaypoint()` return container or executing child? Walk is written correct either way; record the observation (durable finding of the feature)
  - File(s): `docs/features/gm/waypoint-viz/context.md`
  - Estimate: 🟢

- [x] ✅ **Logic-tier test OVT_TEST_Logic_GMWaypointFormat.c + prove-can-fail**
  - Description: Cases per §7 table: 12 prefab names classify; Defend/Patrol ordering trap; `{GUID}` prefix invariance; unknown/empty → UNKNOWN; `LegCount(4,false)`=4, `(4,true)`=5, `(1,true)`=1, `(0,…)`=0; highlight resolution incl. `-1` never highlights. Self-registers `[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]`. Prove-can-fail (named SetFailure, ≤3 format args), record method in context.md
  - File(s): `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_GMWaypointFormat.c` (new)
  - Estimate: 🟡

- [x] ✅ **Campaign-tier test OVT_TEST_Campaign_GMWaypointWalk.c + prove-can-fail**
  - Description: Constructed fixture — spawn 4 patrol + 4 wait waypoints + `AIWaypointCycle`, `SetWaypoints(children)` (mirrors `OVT_OverthrowConfigComponent.c:524-551`), no group/AI. Assert Flatten → 8 entries, no container, cyclic, !truncated, entries classify PATROL/WAIT; second case max=3 → 3 + truncated. Anti-vacuity failure message prints count + first class name. Delete every spawned entity. Prove-can-fail: remove cycle branch → named failure with count 1
  - File(s): `Scripts/Game/Tests/TestSuites/Campaign/OVT_TEST_Campaign_GMWaypointWalk.c` (new)
  - Estimate: 🟡

- [x] ✅ **Phase 1 grep gates**
  - Description: Zero `GetGame()`/`OVT_Global` in format file + Logic test; zero waypoint-write API (`AddWaypoint(`, `RemoveWaypoint(`, `CompleteWaypoint(`, `SetWaypoints(`, …) in any new file; `compile-check.sh` exit 0, file count +4
  - File(s): n/a (grep gate)
  - Estimate: 🟢

---

## Phase 2: 🔴 The wire — request type, fan, client route store (8/8 complete) — M, `network-specialist`

- [x] ✅ **Extend OVT_EGMRequestType additively**
  - Description: Add `GROUP_WAYPOINTS` **after** `CAMPAIGN_SNAPSHOT` (`OVT_GMRequestComponent.c:9-12`) — appending preserves ordinals
  - File(s): `Scripts/Game/Components/Controller/OVT_GMRequestComponent.c`
  - Estimate: 🟢

- [x] ✅ **RpcAsk_GroupWaypoints — gated ask**
  - Description: `[RplRpc(RplChannel.Reliable, RplRcver.Server)]`, arity 3 `(requestType, seq, groupRplId)`. Body order: `Replication.IsServer()` → `ResolveOwningPlayerId()` → `playerId <= 0` return → `IsAuthorizedGM` → `LogRefusal` + silent return → request-type check → `ResolveEntity` → `AIGroup.Cast`. Copy `RpcAsk_Snapshot` (:344-361). Identity never a parameter
  - File(s): `Scripts/Game/Components/Controller/OVT_GMRequestComponent.c`
  - Estimate: 🟡

- [x] ✅ **Server fan — SendWaypointsBegin/SendWaypoint/SendWaypointsEnd**
  - Description: Three owner-targeted sends, each with `ShouldRespondLocally` short-circuit, `Rpc()` at call site. **Empty answer is still an answer**: no-waypoint group / unresolvable RplId / non-group all get `Begin(0)+End(0)`; only auth failure is silent. `[Attribute] m_iMaxWaypointsPerGroup` default 32; on truncation set FLAG_TRUNCATED + one throttled server WARNING (RplId + true count)
  - File(s): `Scripts/Game/Components/Controller/OVT_GMRequestComponent.c`
  - Estimate: 🟡

- [x] ✅ **Client staging + commit under separate route seq**
  - Description: Three `RplRcver.Owner` handlers mirroring snapshot staging: Begin checks `WIRE_VERSION` (reuse `m_bLoggedVersionMismatch`), opens `m_iRouteStagingSeq`; every record drops on seq mismatch; End commits staging → live in one step + fires invoker. **Distinct `m_iRouteSeq`/`m_iRouteStagingSeq` — never share `m_iSeq`** with the snapshot poll
  - File(s): `Scripts/Game/Components/Controller/OVT_GMRequestComponent.c`
  - Estimate: 🟡

- [x] ✅ **Client contract — RequestGroupWaypoints/GetRoute/GetOnRouteUpdated**
  - Description: `RequestGroupWaypoints(RplId)` re-asserts `IsLocalControllerOwner()`, bumps route seq, clears live route, listen-server branch per `RequestSnapshot` (:300-314); `GetRoute()` never null; `GetOnRouteUpdated()` lazily created. Document trio in class header alongside existing contract
  - File(s): `Scripts/Game/Components/Controller/OVT_GMRequestComponent.c`
  - Estimate: 🟢

- [x] ✅ **OnEditorClosed teardown — clear route above the early return**
  - Description: Clear route store **above** `if(!m_State.HasData()) return;` at :287 (the one permitted non-insertion). No second "cleared" invoker — consumers use `GetOnStateCleared()`
  - File(s): `Scripts/Game/Components/Controller/OVT_GMRequestComponent.c`
  - Estimate: 🟢

- [x] ✅ **OVT_GMWaypointRecords.c + pointer in OVT_GMRecords.c**
  - Description: `OVT_GMWaypointRecord` (index, pos, type) + `OVT_GMWaypointRoute` (group RplId, currentIndex, flags FLAG_CYCLIC=1/FLAG_TRUNCATED=2, complete, waypoints array never null, group pos, `Clear()`/`CopyFrom()`), both plain Managed, `OVT_GMRecords.c` doc style; two-line header pointer in `OVT_GMRecords.c`
  - File(s): `Scripts/Game/GameMode/GM/OVT_GMWaypointRecords.c` (new), `Scripts/Game/GameMode/GM/OVT_GMRecords.c`
  - Estimate: 🟢

- [x] ✅ **Arity-diff table + Phase 2 gates**
  - Description: Hand arity-diff all 4 new `Rpc()` call sites vs handlers → table into context.md (BUG-090); max arity 6, no `array<>`/object params. `git diff` on seam file = insertions + the one moved line only. Greps: every new `RpcAsk_*` has `IsAuthorizedGM`; zero `RplRcver.Broadcast`; zero `[RplProp]`. `compile-check.sh` exit 0, +1 file. (Workbench Print bring-up check batched into Phase 4)
  - File(s): `docs/features/gm/waypoint-viz/context.md`
  - Estimate: 🟢

---

## Phase 3: Renderer + selection hookup (6/6 complete) — M, `component-developer`

- [x] ✅ **OVT_GMWaypointRenderer.c — lifecycle (Attach/Detach)**
  - Description: Plain Managed, no widget. `Attach(MenuRootBase)`: subscribe `GetOnMenuUpdate()`, SELECTED filter `GetOnChanged()` (null filter → retry exactly once via `CallLater(…,0)` then give up quietly — hud-icons idiom `OVT_GMDetailUIComponent.c:97-101`), seam `GetOnRouteUpdated()`+`GetOnStateCleared()`. `Detach()`: remove **every** subscription + queued retry, drop route
  - File(s): `Scripts/Game/Components/GM/OVT_GMWaypointRenderer.c` (new)
  - Estimate: 🟡

- [x] ✅ **OnSelectionChanged — re-read filter, entities[0], dedupe**
  - Description: Ignore inserted/removed sets; re-read `filter.GetEntities`, take `entities[0]` (same convention as hud-icons `:363-379`); non-GROUP → clear + return; resolve RplId from owner's RplComponent (local 3-line helper); same id as cached/in-flight → no-op (box-select spam)
  - File(s): `Scripts/Game/Components/GM/OVT_GMWaypointRenderer.c`
  - Estimate: 🟡

- [x] ✅ **OnMenuUpdate — allocation-free draw path**
  - Description: Bail on no seam/no route/empty/group deselected. Member `vector m_aPoints[33]`; colours packed once at attach (`Color.PackToInt()`); `Shape.CreateLines(base, VISIBLE|NOZBUFFER|ONCE, pts, n+1, thickness)`; highlight leg 2-point line drawn after (overdraw); cyclic + n≥2 → close wp[n−1]→wp[0] via `CreateLine` (**not** `CreateLinesLoop`, §5 D6); `CreateCircle` per waypoint, highlight colour for current. Live group vertex each frame (RplId→entity→`GetOrigin()`; unresolvable → draw waypoint chain alone). Constants on class, no `[Attribute]`
  - File(s): `Scripts/Game/Components/GM/OVT_GMWaypointRenderer.c`
  - Estimate: 🟡

- [x] ✅ **Modded SCR_EditModeEditorUIComponent hookup (~8 lines)**
  - Description: `ref OVT_GMWaypointRenderer` member; `Attach(GetMenu())` at end of `HandlerAttachedScripted` (after super + panel creation); `Detach()`+null in `HandlerDeattached` before super; null-check `GetMenu()`. ⚠️ Re-read the file first (hud-icons now committed at `bc3d48a3`); pure insertions, no reorder; **no client-side IsLimited()/role check** (structural gate per file header :8-11)
  - File(s): `Scripts/Game/UI/Modded/SCR_EditModeEditorUIComponent.c`
  - Estimate: 🟢

- [x] ✅ **Print-free operation sweep**
  - Description: No selection / non-group / no record / no waypoints / late route — all silent. Only permitted logs: server truncation WARNING + gm-state's once-per-session version-mismatch line
  - File(s): `Scripts/Game/Components/GM/OVT_GMWaypointRenderer.c`
  - Estimate: 🟢

- [x] ✅ **Phase 3 grep gates**
  - Description: Renderer + format + walk: zero `Rpc(`, `[RplProp]`, `RplRcver`, `SetDrawCommands(`, `ClearDetail(`, `GetDetailSlot(`, `ShowDetail(`, `SCR_EditableWaypointComponent`. `compile-check.sh` exit 0, +1 file
  - File(s): n/a (grep gate)
  - Estimate: 🟢

---

## Phase 4: Verification gate (2/3 — Step 2 REOPENED) — M, user-driven

- [x] ✅ **Step 1 — Workbench Play mode (host path)** — Completed 2026-08-16 via live feedback rounds (route draws, selection resolution, solid lines + blue current leg)
  - Description: §8 checks F-1…F-8, Q-3, I-4, I-5 per Verification Method Step 1 (perimeter patrol = closed 8-vertex loop; 1 vertex = cycle recursion failed). ⚠️ Warn the user before launching anything
  - File(s): n/a (manual)
  - Estimate: 🟡

- [ ] 🔴 **Step 2 — Multiplayer** — REOPENED 2026-08-23. The 2026-08-16 completion was recorded in error (dedi testing had not started). **Observed defect: a GM on a dedicated server sees no waypoint lines at all; single-player is fine.**
  - Description: GM client on dedicated server (the check the wire exists for); record auth path used (discharges gm-state Phase 5 debt); negative path without `-ovtGmDev` → zero `RpcDo_Waypoint*` for non-admin (F-7) + one throttled WARNING on coerced request; listen-server host (Q-2); dormant-group observation (D12); no log spam over 20 rapid selections
  - File(s): n/a (manual)
  - Estimate: 🟡

- [x] ✅ **Step 3 — grep gates pasted into context.md**
  - Description: Run Q-4…Q-9, I-3, I-6 greps and paste output into context.md
  - File(s): `docs/features/gm/waypoint-viz/context.md`
  - Estimate: 🟢

---

## Phase 5: context.md + epic bookkeeping (3/3 complete) — S, `component-developer`

- [x] ✅ **Finalize context.md**
  - Description: Shipped wire table with hand-checked arities; observed cycle semantics (durable finding); visual constants as built; Phase 4 results; "GM sees no lines" triage chain (GROUP? → Begin arrived? → gate refused? → group has waypoints? → gm-state triage). Must answer "what does a GM see when a group has no waypoints?" standalone
  - File(s): `docs/features/gm/waypoint-viz/context.md`
  - Estimate: 🟢

- [x] ✅ **Update epic-overview.md**
  - Description: Feature 4 status + task count. **No help-docs phase** (consolidated pass at epic end). Note `{6B0A…}` NOT consumed, remains free for gm-map
  - File(s): `docs/features/gm/epic-overview.md`
  - Estimate: 🟢

- [x] ✅ **Record upstream findings (D13)**
  - Description: `SpawnWaitWaypoint` discards `time` (`OVT_OverthrowConfigComponent.c:497-502`); `SpawnGetInWaypoint(vector)` spawns the GetOut prefab (`:443-447`) — report into epic-overview Findings attributed to `occupying`; do NOT fix here
  - File(s): `docs/features/gm/epic-overview.md`
  - Estimate: 🟢

---

## Phase 6: 🔴 Rendering rework — `Shape` → `CanvasWidget` (0/9 complete) — L, `ui-developer`

> **Why:** `Shape.Create*` is Workbench-only debug geometry and paints nothing in a shipped client, so this
> feature has never actually drawn for a real player. See `implementation.md` §4 Phase 6 (D14–D16) and
> `context.md` → 2026-08-23 root-cause note. The wire, the walk and the renderer's logic are all confirmed
> good against a real dedicated server — **only the draw primitive changes.**

- [ ] **Mint the canvas layout**
  - Description: `UI/Layouts/GM/GMWaypointCanvas.layout` + `.meta`, GUID `{6B08D3A17C4B1016}` (next free in the gm series; highest in use `…1015`). Root `CanvasWidgetClass` named `OVT_GMWaypointCanvas`, `FrameWidgetSlot` anchored `0 0 1 1`, **must not take cursor input**. ⚠️ re-grep `{6B0A` (braced) first — reserved for gm-map
  - File(s): `UI/Layouts/GM/GMWaypointCanvas.layout` (new), `.layout.meta` (new)
  - Estimate: 🟡

- [ ] **Hookup: create the canvas before the panel and pass it to the renderer**
  - Description: In `HandlerAttachedScripted`, create the canvas **before** `PANEL_LAYOUT` so panel/hud-icons keep z-order above the route; widen to `Attach(menu, canvas)`
  - File(s): `Scripts/Game/UI/Modded/SCR_EditModeEditorUIComponent.c`
  - Estimate: 🟢

- [ ] **Renderer: cache workspace/world + the projection helper**
  - Description: Cache `GetWorkspace()`/`GetWorld()` at Attach; `ProjWorldToScreenNative(pos, world)`, native result straight into `m_Vertices` (no DPI conversion). Behind-camera test is `posScreen[2] > 0`; **D16 — drop the leg, never clamp**
  - File(s): `Scripts/Game/Components/GM/OVT_GMWaypointRenderer.c`
  - Estimate: 🟡

- [ ] **Renderer: legs as `LineDrawCommand`s**
  - Description: One command per contiguous same-colour run, highlight leg always its own. `m_fWidth = LINE_THICKNESS`, `m_fOutlineWidth = 0`, `m_bShouldEnclose = false`. ⚠️ **D6 survives**: the cyclic closing leg is an explicit extra vertex last→first — never `m_bShouldEnclose`, which would close back to the GROUP vertex
  - File(s): `Scripts/Game/Components/GM/OVT_GMWaypointRenderer.c`
  - Estimate: 🟡

- [ ] **Renderer: waypoint markers as projected world-space N-gons (D15)**
  - Description: No canvas circle command exists. Build the circle in **world** space on the xz-plane at `CIRCLE_RADIUS` metres, `CIRCLE_SEGMENTS = 12`, project each vertex, emit one enclosed `LineDrawCommand`. Keeps the metre-based radius so markers shrink with distance as before
  - File(s): `Scripts/Game/Components/GM/OVT_GMWaypointRenderer.c`
  - Estimate: 🟡

- [ ] **Renderer: per-frame command lifecycle + Detach flush**
  - Description: One reusable array, `Clear()` each frame, **exactly one `SetDrawCommands` per frame even when empty** (skipping it leaves the last route painted forever); `Detach()` pushes one empty `SetDrawCommands` before dropping the canvas
  - File(s): `Scripts/Game/Components/GM/OVT_GMWaypointRenderer.c`
  - Estimate: 🟢

- [ ] **Extract the colour-run split as a pure static + Logic case**
  - Description: Move "legs → contiguous colour runs" into `OVT_GMWaypointFormat` so it stays Logic-testable; extend `OVT_TEST_Logic_GMWaypointFormat`. Projection itself is NOT unit-testable and must not hide behind an untested helper
  - File(s): `Scripts/Game/GameMode/GM/OVT_GMWaypointFormat.c`, `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_GMWaypointFormat.c`
  - Estimate: 🟡

- [ ] **Strip `Shape` entirely + grep gate**
  - Description: Delete `m_eFlags`, `m_aPoints[33]` and every `Shape.` reference. Gate: `grep -n "Shape\." Scripts/Game/Components/GM/OVT_GMWaypointRenderer.c` = 0. Run `compile-check.sh` + the All group
  - File(s): `Scripts/Game/Components/GM/OVT_GMWaypointRenderer.c`
  - Estimate: 🟢

- [ ] 🔴 **Verify on a REAL client against a dedicated server** (discharges the reopened Phase 4 Step 2)
  - Description: ⚠️ **Workbench is no longer evidence of anything visual.** Route draws with the current leg highlighted; cyclic patrol closes (finally count R1's vertices); route does NOT block click-select or box-select; panel + hud-icons tooltips render ABOVE the route; camera turned so the group is behind it drops the route cleanly with no screen-spanning artefacts; deselect / editor close / Photo mode leave no residue. ⚠️ Warn the user before launching
  - File(s): n/a (manual)
  - Estimate: 🟡

---

## Bugs & Issues

**Active Bugs:**
- (none)

**Fixed Bugs:**
- (none)

---

## Technical Debt

- 🔴 **The whole visual must move off `Shape.Create*` onto a `CanvasWidget`** (found 2026-08-23). `Shape` is debug-only geometry that renders in Workbench and nowhere else, so this feature has never actually drawn for a real player. Replacement: `workspace.ProjWorldToScreenNative()` + `LineDrawCommand` + `canvas.SetDrawCommands()`, per vanilla `SCR_WaypointLinesEditorUIComponent`'s cycle-arrow branch. Kills plan 3.4's "owns no widget, needs no layout" premise; needs off-screen/behind-camera culling; must NOT share a canvas with vanilla's writer (`SetDrawCommands` replaces the list). See `context.md` → 2026-08-23 root-cause note.

---

## Task Status Legend

- [ ] Not started · [ ] 🔄 In progress · [ ] ⏸️ Blocked · [x] ✅ Completed · [x] ❌ Cancelled

---

*Update this file as tasks are completed. Mark tasks with ✅ immediately when done. Add new tasks as they're discovered.*
