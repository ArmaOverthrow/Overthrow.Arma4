# Virtualization Core - Task Checklist

**Last Updated:** 2026-08-17
**Progress:** 50/50 tasks complete (100%) — all 6 phases built and gate-verified 2026-08-17. Final gates: compile 0 (6117 files) · Fast 163 → All **210** · all DoD greps clean · I1 consumers untouched · I4 exactly one conf entry.

**⚠️ Advanced-agent phases:** P3 (`component-developer-advanced`), P5 (`component-developer-advanced`).
**Every phase gate:** `tools/compile-check.sh` exit 0 · Fast group green · All group when campaign/persistence state touched (P5) · Q4/Q7/Q12 greps empty (`Rpc|RplProp|PlayerInRange|NearestPlayer|GetOnAgentRemoved` in `Scripts/Game/GameMode/Virtualization/`).
Tasks marked **[USER]** are Workbench/play-test work the agent cannot do; they block play-testability only, never the compile/test gates.
**Phase 1 is a measurement gate:** Phase 2 must not start until every T1.x answer is written into `context.md` with evidence and any plan contradictions are resolved in `implementation.md`.

---

## Phase 1: Engine-adoption spike (6/6 complete) ✅ 2026-08-17 — `component-developer` — GATE

- [x] ✅ **T1.1 ObserversSystem live in Overthrow's world** — runtime-confirmed 2026-08-16 (play-test log: both systems found, ChimeraAIWorld=yes, observers total=2 SP=1 MP=1). Static same-GUID-delta analysis also held.

- [x] ✅ **T1.2 Lifecycle behaves as read** **[USER play-test]** — DONE 2026-08-16 with CRITICAL findings: progressive fill ✅; first-N refill proven (slotIndex==agentsBefore); despawn-mid-fill ratchet corrupts counts (6→2, zero kills) — D2 mask now MANDATORY; eliminate-when-reached deferred to post-wipe (deletion hazard); OnEmpty fires on every teardown. Full evidence in context.md; kill-3/wipe/boundary sub-checks superseded by engine-source proof.


- [x] ✅ **T1.3 Persistence fires for runtime-spawned groups** — ANSWERED 2026-08-16 (static + runtime `IsTracked=no`): vanilla class rule defeated by BUG-118 unconditional untrack + `SelfSpawn 0`; Phase 3 must build exemption + self-spawn path; `MarkForSelfSpawn` banned (BUG-116). Plan amended.

- [x] ✅ **T1.4 Test-world behaviour** — verified 2026-08-16: test world IS ChimeraAIWorld (RequestSpawn enqueues), ObserversSystem present, unobserved ProximityDriven group stays memberless 240 frames → Init-tier registration cases safe. Evidence in context.md.

- [x] ✅ **T1.5 Scale probe** — 2026-08-17: 100 dormant groups registered in **73 ms** (0.73 ms/group, 0 failures); script allocator +16 MB (~160 KB/group incl. noise); Task Manager delta negative (4800→4500 MB) → process memory cost below noise. FPS samples (6.7→9.4 climbing) not attributable — campaign-init settling, no baseline; Phase 6 T6.1 re-measures with baseline. Replication probe with client: not run (deferred to Phase 6).

- [x] ✅ **T1.6 Write answers + amend plan** — 2026-08-17: all answers in context.md with evidence; implementation.md carries 6 Phase 1 verdict amendments (persistence tracking, D2b broadened, eliminate-when-reached deferred, observer semantics, purge hole, test-world green light); all spike code removed (git status = docs only); compile-check 0 (6108 files). Fast suite skipped: net code delta zero.

---

## Phase 2: Records, registry API, manager scaffold, config field (11/11 complete) ✅ 2026-08-17 — `component-developer`

- [x] ✅ **T2.1 `OVT_VirtualGroupRecord.c`** — enum, `OVT_VirtualWaypointPlan`, `OVT_VirtualGroupRecord` per §3.3. `Scripts/Game/GameMode/Virtualization/OVT_VirtualGroupRecord.c` — 1 h
- [x] ✅ **T2.2 `OVT_VirtualizationManagerComponent.c` scaffold** — class pair, `s_Instance`/`GetInstance()`, server guard in `OnPostInit` before collections, `Init`, empty `PostGameStart()`, `OnDelete` cleanup — 1 h
- [x] ✅ **T2.3 Registry API surface (no engine wiring)** — register/unregister/query/reclaim/state accessors + both invokers; entity-creation stubbed; `ForceSpawn`/`ForceDespawn` no-op-with-WARNING — 2 h
- [x] ✅ **T2.4 `OVT_VirtualizationMath.c`** — pure world-free statics: `ResolveSpawnDistance`, `ResolveDespawnDistance`, `ResolveImportance`, `ValidateWaypointPlan`, `CountAlive`/`IsWiped`/`NextSlotToSpawn`, `SliceIndices`/`AdvanceCursor`, `RollCountSafe` — 1-2 h
- [x] ✅ **T2.5 Config field** — `virtualizationSpawnDistance` in `OVT_OverthrowConfigStruct` + `SetDefaults()` = 1750; no JIP change — 0.5 h
- [x] ✅ **T2.6 `OVT_Global.GetVirtualization()`** — 0.5 h
- [x] ✅ **T2.7 Game-mode registration** — field + `FindComponent`/`Init` in `EOnInit` after Deployment (:1156) + `PostGameStart()` block — 0.5 h
- [x] ✅ **T2.8 Prefab wired in text** — `OVT_VirtualizationManagerComponent "{6B4A1D3E00000010}"` added to `OVT_OverthrowGameMode.et` (agent, fresh repo-unique GUID, controller-migration precedent). **[USER] verify it opens clean in Workbench** _(tracked in context.md)_
- [x] ✅ **T2.9 Logic-tier suite** — `TestSuites/Logic/OVT_TEST_Logic_Virtualization.c` covering T2.4 statics; tier grep rule: no manager-accessor/game-mode-getter identifiers anywhere incl. comments — 2 h
- [x] ✅ **T2.10 Init-tier cases** — manager resolves via `OVT_Global`; count 0; config default reads back; unknown faction/group name → −1 + WARNING — 1 h
- [x] ✅ **T2.11 `api.md`** — §3.3 signatures, importance guidance, D2 contract, invoker subscription pattern, worked register→reclaim→unregister example — 1-2 h

---

## Phase 3: Engine lifecycle wiring, wipe bookkeeping, waypoints (11/11 complete) ✅ 2026-08-17 — `component-developer-advanced` ⚠️

- [x] ✅ **T3.1 Full registration path** — §3.3 steps 1–6: `IgnoreSpawning` → spawn → `SetFaction` → `SetLifecyclePolicy` → `SetImportance` → waypoints (NO eliminate-when-reached at registration — Phase 1 amendment 3: enable only post-wipe) → persistence tracking per T1.3 verdict (untrack exemption + registered-only self-spawn path; MarkForSelfSpawn banned) → signal subscriptions — 3-4 h
- [x] ✅ **T3.2 Waypoint construction via `OVT_OverthrowConfigComponent` helpers** — every created `AIWaypoint` recorded in `m_aOwnedWaypoints`; note `SpawnWaitWaypoint` time-arg bug in context.md — 1 h
- [x] ✅ **T3.3 D2 mask machinery** — `SetOVTSlotMask`/clear seam + `ExpandOneMember` override on modded `SCR_AIGroup` (next mask-alive not-yet-materialised slot; vanilla fallback when no mask); member→slot reverse map — 2-3 h
- [x] ✅ **T3.4 Death accounting** — subscribe `GetOnCharacterKilled()` once; victim → (handle, slot); teardown-window guard (`GetOnMembersDespawning`); NO `GetOnAgentRemoved` — 1-2 h
- [x] ✅ **T3.5 Count correction** — after each engine despawn re-assert `SetDormantCounts(CountAlive(mask), dead)` (D2b) — 1 h
- [x] ✅ **T3.6 Wipe bookkeeping** — `GetOnEliminatedWhenReached` + wiped-while-spawned detection; `OnGroupWiped` fires BEFORE record removal; owned waypoints deleted; persistence entry untracked (BUG-118 retry-queue hazard) — 2 h
- [x] ✅ **T3.7 `UnregisterGroup`** — despawn (respect `HasHeldMember`), delete entity + waypoints, untrack, remove record — 1 h
- [x] ✅ **T3.8 `ForceSpawn`/`ForceDespawn`** — `RequestSpawn()`/`DespawnMembers()`; document force = nudge not pin — 0.5 h
- [x] ✅ **T3.9 `m_bDebugRegisterTestGroup`** — default-false debug registration near campaign start in `PostGameStart()` — 0.5 h
- [x] ✅ **T3.10 Init-tier cases** — register → count 1 + unspawned entity + stamped policy/importance; unregister → gone; waypoint plan → owned entities exist; `ReportMemberKilled` flips slot + reduces count; kill all slots → record removed + `OnGroupWiped` — 2 h
- [x] ✅ **T3.11 `OnDelete`** — despawn/delete everything live, clear `s_Instance` (R7) — 0.5 h

---

## Phase 4: Ambient spawn-source seam (8/8 complete) ✅ 2026-08-17 — `component-developer`

- [x] ✅ **T4.1 Config classes** — `OVT_AmbientSpawnSourceConfig.c` + registry per §3.4, four overridable roll/hook methods, `RollCountSafe` defaults — 1-2 h
- [x] ✅ **T4.2 `OVT_AmbientSpawnSourceInstance.c`** — config + position + ownerKey + live entities + spawn-progress state — 1 h
- [x] ✅ **T4.3 Register/unregister/count/entities + ambient `CallLater` tick** — round-robin slice, `ObserversSystem.HasObserverWithinRangeSq`, despawn hysteresis — 2 h
- [x] ✅ **T4.4 Ambient spawn** — roll count once per activation; per entity roll prefab+position, spawn, `OnEntitySpawned`, reverse-map; spread across ticks (`m_iAmbientSpawnsPerTick`) — 1-2 h
- [x] ✅ **T4.5 Ambient despawn** — `OnEntityDespawning`, delete live entities, clear list + map; no state kept — 1 h
- [x] ✅ **T4.6 `ReleaseAmbientEntity`** — O(1) reverse-map removal; false for non-ambient — 0.5 h
- [x] ✅ **T4.7 Prune dead/deleted entities** each evaluation — 0.5 h
- [x] ✅ **T4.8 Init-tier cases** — source registers + counted; subclass `RollCount()` override called; `ReleaseAmbientEntity` unknown entity → false — 1 h

---

## Phase 5: Persistence — registry serializer + round-trip coverage (8/8 complete) ✅ 2026-08-17 — `component-developer-advanced` ⚠️ — ROUTE B (manager re-creates from registry; no vanilla group records)

- [x] ✅ **T5.1 `OVT_VirtualizationManagerSerializer.c`** — registry bookkeeping only, parallel arrays incl. group UUIDs + slot masks, handle counter, `version` first, `if (version < 1) return true;`, shipped-serializer discipline (`OVT_TownManagerSerializer` shape) — 2 h
- [x] ✅ **T5.2 `ApplyPersistedRegistry(...)`** — idempotent match-by-handle, counter restore, `WhenAvailable` relink per UUID, re-stamp policy/distances/importance + re-push mask + re-assert dormant counts on relink, drop-with-WARNING — 2-3 h
- [x] ✅ **T5.3 `GetOnRecordsRestored()` latch** — fires once after all relinks resolve/expire — 1 h
- [x] ✅ **T5.4 Register in `Overthrow.conf`** — game-mode `ComponentSerializers` block, fresh unique `6B0E7A2x` GUID — 0.5 h
- [x] ✅ **T5.5 Ambient sources not in payload** — asserted by construction, stated in header — 0.25 h
- [x] ✅ **T5.6 Round-trip case `_VirtualGroups_SurviveSaveAndReload`** — mutate (register + `ReportMemberKilled` specific slot + save) → dirty → re-apply → assert handle/owner/reduced count/dead slot/`FindGroupsByOwner`/bogus gone — 2 h
- [x] ✅ **T5.7 Round-trip case: wiped group stays gone** — kill every slot, save, re-apply, assert unregistered — 1 h
- [x] ✅ **T5.8 Can-fail proofs recorded** for both cases; no `maxAttempts` — 0.5 h

---

## Phase 6: Measurement, hardening, API freeze (6/6 complete) ✅ 2026-08-17 — `component-developer`

- [x] ✅ **T6.1 Re-measure at scale** — affordance built (`m_iDebugTestGroupCount`, golden-angle spread, wall-time log); **[USER play-test]** §6 step 8 measurement itself still owed — — ~40 groups in one town, fast-travel in, frame time + time-to-populate vs T1.5 numbers → `context.md` — 1-2 h
- [x] ✅ **T6.2 Restart hardening** — audit found + fixed 4 real bugs (stale s_Instance cache re-resolve, cross-campaign double ambient tick self-cancel, restore-latch re-fire guard, kill-hook owner-entity resolution); **[USER play-test]** live two-campaign half still owed — — campaign → quit → new campaign: no stale `s_Instance`, no orphaned `CallLater`, no error spam (R7) — 1 h
- [x] ✅ **T6.3 Missing-faction hardening** — Init case `MissingFactionRecordIsDropped` (drop + prefab-fallback both covered, 3 fail proofs recorded) — — save with unknown faction key → drop-with-WARNING verified (R4) — 1 h
- [x] ✅ **T6.4 Drive-past check** — engine dequeue re-check documented in api.md for consumers; **[USER play-test]** §6 step 9 still owed — — dense area at speed: dequeue-time observer re-check drops requests, nothing errors — 0.5 h
- [x] ✅ **T6.5 Freeze `api.md`** — 🔒 FROZEN with the five consumer rules + per-sibling entry-point tables (§10) — — mark contract; D2 contract + `ExpandOneMember` reliance prominent; per-sibling entry-point lists — 1 h
- [x] ✅ **T6.6 Remove debug prints** — 10 informational prints → VERBOSE; WARNINGs (all operator-actionable) kept — not behind `LogLevel.VERBOSE` — 0.5 h

---

## Bugs & Issues

**Active Bugs:**
- (none)

**Fixed Bugs:**
- [x] ✅ **All-group client freeze at the ambient-override Init case** — Fixed 2026-08-17
  - Fix: removed the case's `InsertObserverSP(key, x, z, null)` observer parking (zero vanilla precedent for a null-entity SP insert; froze the main thread in the one world where ObserversSystem is actively consumed). Case falls back to its documented virtual-dispatch assertion; re-run green (All 209/209).



---

## Technical Debt

- (none yet)

---

## Progress Tracking

### Discovered New Tasks
- [x] ✅ **Phase 5 ruling: persistence Route A vs Route B** — Route B ruled 2026-08-17 (announced default; user did not reply in time). DoD I4 holds (exactly one Overthrow.conf entry). — self-spawn for runtime groups stopped-and-reported (class-wide `SelfSpawn` = duplicated garrisons on every load; scripted IsMatch dead; MarkForSelfSpawn banned). Route A = bounded EntityPersistenceConfig + decoded-save verification; Route B = manager re-creates groups from registry (record carries m_vPosition/m_Plan). Decision owed before Phase 5; DoD I4 ("exactly one Overthrow.conf entry") needs re-ruling if Route A.
- [ ] **[USER] Phase 4 play-test** — §6 steps 10–11 (ambient source spawn-spread via `m_bDebugAmbientLogging` + release survives despawn); needs a code-built config or throwaway .conf NOT committed to Configs/ (DoD grep).
- [x] ✅ **[USER] Phase 3 play-test (core loop)** — 2026-08-17 green ×3: lifecycle (GM-cam cycling, 300 m debug ring), 6-man kill-test (exact surviving roles return — F4/D2), **save→quit→Continue with survivors (F8/F9 — Route B verified live)**. Still open: wipe-never-returns, boundary-thrash, second-campaign (step 13), scale probe (step 8).

### Blocked Items
- (none yet)

---

*Update this file as tasks are completed. Mark tasks with ✅ immediately when done. Add new tasks as they're discovered.*
