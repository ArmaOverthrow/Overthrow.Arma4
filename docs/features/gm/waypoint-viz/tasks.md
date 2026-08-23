# Waypoint Viz — Task Checklist

**Feature:** gm/waypoint-viz (epic `gm`, feature 4 of 5)
**Last Updated:** 2026-08-15 (Phase 5 complete)
**Progress:** 28/29 (Phase 4 Step 2 REOPENED 2026-08-23) — Workbench render rounds green; the **dedicated-server pass never happened** (user retracted the 2026-08-16 claim on 2026-08-23) and routes **do not draw on a dedicated server**

> The remaining **2** are the deferred user-driven **Phase 4 Steps 1–2** (Workbench host pass and the
> multiplayer/dedicated + negative-auth pass) — deferred by the user 2026-08-15 to a later session. All
> implementation and documentation work is complete; see `context.md` → "Needs human verification".

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

## Phase 4: Verification gate (3/3 complete) — M, user-driven

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

## Bugs & Issues

**Active Bugs:**
- (none)

**Fixed Bugs:**
- (none)

---

## Technical Debt

- (none yet)

---

## Task Status Legend

- [ ] Not started · [ ] 🔄 In progress · [ ] ⏸️ Blocked · [x] ✅ Completed · [x] ❌ Cancelled

---

*Update this file as tasks are completed. Mark tasks with ✅ immediately when done. Add new tasks as they're discovered.*
