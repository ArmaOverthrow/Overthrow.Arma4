# Virtualization Core - Context & Decisions

**Last Updated:** 2026-08-17
**Current Phase:** Complete — all 6 phases built and gate-verified
**Status:** ✅ Ready for Review (user play-tests owed — see "Needs human verification")

---

## Quick Status

**What's Done:**
- ✅ Planning complete (Revision 2, 2026-08-14 — replanned on Reforger 1.8 native lifecycle; user-approved hybrid + slot-accurate D2)
- ✅ Feature started 2026-08-16; tasks.md scaffolded (50 tasks / 6 phases)
- ✅ **Phase 1 (engine-adoption spike) complete 2026-08-17** — all five questions answered with evidence (below), plan amended (6 verdict amendments in implementation.md), spike removed, compile 0. Includes user play-test + engine-source investigation.

- ✅ **Phase 2 complete 2026-08-17** — registry API + manager scaffold + `OVT_VirtualizationMath` + config field + prefab text-wiring `{6B4A1D3E00000010}` + api.md. Gates: compile 0 (6112), Fast **156** (was 146 baseline), Q4/Q7 greps + Logic-tier grep clean.

- ✅ **Phase 3 complete 2026-08-17** — full registration path (no eliminate-when-reached at registration; `RetireGroupEntity` stamps it only post-wipe), real mask-driven `ExpandOneMember` + `IsExpandComplete` + `SpawnMembers` fallback overrides, death accounting via kill invoker with structural teardown guard (`m_bDespawning` + reverse-map clear, no timers), T3.5 `ReassertOVTDormantCounts` after every despawn, waypoint ownership (2 create sites ↔ 1 delete site, 3 callers), persistence **exemption seam** (`ArmPersistenceExemption` one-shot + modded `SCR_AIGroupSerializer` load-path arm + `CancelUntrackTransient`/`Track` fast-path handling) gated behind `m_bPersistGroupEntities=false`, 4 Init cases incl. runtime mask-slot-selection proof. Gates: compile 0 (6113), **All 204** (was 190 baseline), all greps clean, Overthrow.conf net-untouched.

- ✅ **Phase 4 complete 2026-08-17** — ambient config/registry/instance classes, idempotent 2 s `CallLater` tick (round-robin `SliceIndices` slices, float-squared observer rings — int would overflow on "always spawned" values, spawn-ring activation vs despawn-ring teardown anti-thrash), frame-spread fill (cursor advances on failed spawns too), release path re-verifies map hits against the source list (EntityID recycling guard), prune-first evaluation, 3 Init cases. Separate `m_iNextAmbientHandle` namespace (never persisted). Gates: compile 0 (6116), **Fast 163**, all greps clean, `Configs/` grep empty (zero authored content).

- ✅ **Phase 5 complete 2026-08-17 (ROUTE B)** — `OVT_VirtualizationManagerSerializer` (frozen append-only payload: version, nextHandle, per-record handle/owner/composition/override/importance/live-position/mask/plan), synchronous re-creation inside `ApplyPersistedRegistry` (payload carries resolvedPrefab → no faction-manager dependency at deserialize), `GetOnRecordsRestored` fires once post-`HasGameStarted` (~10 s bounded), all-dead records never re-created, one `Overthrow.conf` entry `{6B0E7A2F4A5EB27B}`. Gates: compile 0 (6117), **All 209** after one fixed freeze (below), greps clean.

- ✅ **Phase 6 complete 2026-08-17** — scale affordance (`m_iDebugTestGroupCount`, golden-angle spread, wall-time log), restart audit **fixed 4 real bugs** (stale `s_Instance` re-resolve, cross-campaign double ambient tick self-cancel, restore-latch re-fire guard, kill-hook owner-entity resolution), missing-faction Init case (drop + prefab-fallback), api.md 🔒 FROZEN with per-sibling entry-point tables, debug-print sweep (10 → VERBOSE). Final gates: compile 0 (6117), **All 210**, all DoD greps clean, I1 consumers untouched, I4 one conf entry.

**What's Next:**
- 📋 User play-tests (see "Needs human verification") — most valuable first: §6 step 12 (quit→Continue, the one path Route B changes most) and §6 steps 1–6 (slot-accurate refill in the live game)
- 📋 `civilians` is unblocked (ambient seam + frozen api.md §10 table); `movement` unblocked (dormant-write seam)
- 📋 Candidate upstream report: `InsertObserverSP(null entity)` client freeze (check RFG series first)

**Blockers:**
- Phase 2 T2.8 is a user Workbench task (add manager component to the game-mode prefab) — project precedent (controller-migration) allows text-wiring by agent + user Workbench verification later
- Phase 3 open item: `SpawnGroupMember(slotIndex)` arbitrary-index acceptance was never runtime-proven (PROVE_ARBITRARY_SLOT pass not run); T3.3's Init case must prove it

---

## Key Files

### Core Implementation (to be created)
- `Scripts/Game/GameMode/Virtualization/OVT_VirtualGroupRecord.c` — record + waypoint plan classes (Phase 2)
- `Scripts/Game/GameMode/Virtualization/OVT_VirtualizationManagerComponent.c` — the Manager (Phase 2+)
- `Scripts/Game/GameMode/Virtualization/OVT_VirtualizationMath.c` — world-free statics for Logic tier (Phase 2)
- `Scripts/Game/GameMode/Virtualization/OVT_AmbientSpawnSourceConfig.c` — ambient seam (Phase 4)
- `Scripts/Game/Persistence/Serializers/Components/OVT_VirtualizationManagerSerializer.c` (Phase 5)
- `docs/features/virtualization/core/api.md` — the epic's consumer contract (Phase 2, frozen Phase 6)

### Engine surfaces this feature leans on (R4 update-check checklist)
- `SCR_AIGroup.c` — `SetLifecyclePolicy` :2915, `IgnoreSpawning` :2217, `DespawnMembers` :2864, `RequestSpawn` :2678/:2687, `SetEliminateWhenReached` :2963, `HasHeldMember` :2801, `ExpandOneMember` :2731, `GetOnMembersDespawning` :2288
- `ChimeraAIGroup.c` — `HasObserverInRange` :24, `ExpandOneMember` event :29
- `ChimeraAIWorld.c` — `EnqueueSpawnRequest` :19, `PurgeSpawnRequestsForGroup` :21
- `SCR_EAISpawnImportance.c` — tier semantics (LOW 0.50 … CRITICAL 1.00)
- `ObserversSystem.c` — `HasObserverWithinRangeSq` :70
- `SCR_AIGroupSerializer.c` — dormant round-trip :80-86, counts :161-170/:351-361
- `Common.conf` — AIGroup/AIUnit/AIWaypoint wiring :24, :28, :93
- Vanilla precedent: `SCR_AmbientPatrolSystem.c` :140, :195

---

## Important Decisions

(Plan decisions D1–D11 live in `implementation.md` §5 — recorded there, not duplicated. Session decisions land here.)

---

## Additive changes after the freeze

`api.md` froze on 2026-08-17 (Phase 6). The freeze permits **additive** change — a new method, a new
appended payload field behind a version bump — and forbids renaming, re-signing or re-meaning
anything already on the page. Every additive change lands here, dated, with the requester named.
**Breaking** changes would get their own recorded note; there have been none.

### 2026-08-17 — Manual-policy spawn guard on the modded `SCR_AIGroup` (behaviour bugfix)

**Found by** `virtualization/movement`'s Init cases (the first tests ever to watch a registered group across seconds): a group registered with `spawnDistanceOverride = 0` (the documented "never materialise by proximity" Manual policy) **still materialised its members a few seconds after registration**. Vanilla enqueues a FULL spawn at entity init when the group prefab sets `m_bSpawnImmediately` (`SCR_AIGroup.c:2595-2601`), and the queue re-validates only OBSERVER presence at dispatch — a GM camera or the autotest camera qualifies — so the policy core stamped was ignored. The refusal cannot happen at init (the request is queued before core stamps mask/policy), so it lands at DISPATCH.

**What changed** (`Scripts/Game/Modded/SCR_AIGroup.c` + `ForceSpawn`): masked (core-owned) groups whose policy is `Manual` refuse `ExpandOneMember`/`SpawnMembers` dispatches unless core armed the new `ArmOVTManualSpawn()` latch (armed by `ForceSpawn` before its `RequestSpawn`, cleared on `DespawnMembers` so every force spawn re-arms). `IsExpandComplete` answers **true** for the guarded condition so the queue books the request complete and drops it instead of retrying forever. Groups without a mask, and masked ProximityDriven groups, are byte-for-byte untouched.

**Consequence:** `spawnDistanceOverride == 0` now means what api.md always said it means. Movement's three Init cases register with override 0 and rely on it (dormant by construction — the autotest camera can no longer materialise them mid-case).

### 2026-08-17 — `GetCurrentPlanIndex(handle)` on the manager (play-test fix: live-handoff direction)

**Requested by:** `virtualization/movement` (user-approved option A on Discord, during the §6 play-test). Play-test showed despawned groups resuming AWAY from their live waypoint: on a two-point cycling patrol both legs are the same line, so movement's position-only projection cannot recover direction and its tie-break always answers the outbound leg.

**What was added** (`OVT_VirtualizationManagerComponent.c`, beside `GetAllHandles`): `int GetCurrentPlanIndex(int handle)` — matches `AIGroup.GetCurrentWaypoint()` by identity against `record.m_aOwnedWaypoints` and answers the plan index; the cycle entity (appended last when the plan cycles) answers 0; `-1` whenever the answer would be a guess (no record/entity/current waypoint, or an owned list that does not match the plan 1:1 because a point failed to build). Uses `ResolveGroup` (the safe entity path), never dereferences `m_Group` raw.

**Why movement cannot do this itself:** its own acceptance bans waypoint/group identifiers in its directory (`AIWaypoint|SCR_AIGroup|m_aOwnedWaypoints` grep = 0) — the entity read had to live in core. Projection remains movement's fallback for loads, teleports and off-route deliveries.

**Why it is not a breaking change:** pure read, nothing renamed or re-signed, no payload field, `CONFIG_STREAM_VERSION` unmoved, no existing call path changed.

### 2026-08-17 — play-test bugfix: waypoint entities and the debug plan are now surface-snapped

**Not an API change — a behaviour bugfix** (recorded here so the post-freeze core diff stays accounted for). User play-test report during `virtualization/movement` §6: "test group waypoints are always either inside the terrain or flying in the air."

**What changed** (`OVT_VirtualizationManagerComponent.c`): `CreatePlannedWaypoint` snaps the spawned waypoint **entity**'s Y to `GetSurfaceY` (plans are authored in XZ with an arbitrary Y — a completion radius smaller than the Y error would make live AI unable to complete the waypoint); `RegisterDebugTestGroupAt` snaps the debug anchor and its 150 m far point at authoring time. The **plan payload is untouched** — positions still round-trip verbatim, no persistence claim moves, no signature changed, `CONFIG_STREAM_VERSION` unmoved.

**Consequence for consumers:** author plans in XZ freely; the entity spawn corrects Y. A deliberately elevated waypoint (e.g. a tower post) is NOT supported through the plan path — if one is ever needed, that is a new additive ask, not a revert of this fix.

### 2026-08-17 — two prune hooks on `OVT_AmbientSpawnSourceConfig`

**Requested by:** `virtualization/civilians` (feature #2 of the epic), Phase 1 — its implementation
plan §3.4 and tasks T1.1/T1.2. Built by that feature, in core's files.

**What was added** (`Scripts/Game/GameMode/Virtualization/OVT_AmbientSpawnSourceConfig.c`):

```c
bool IsEntityDead(IEntity entity);   //!< default: SCR_DamageManagerComponent state == DESTROYED
void OnEntityPruned(IEntity entity); //!< no-op; fires AFTER the list AND reverse-map removals
```

Both are called from `OVT_VirtualizationManagerComponent.PruneAmbientEntities` — the only two call
sites, and the only lines of existing core code the change touched.

**Why.** `civilians` tracks a **group** entity per civilian (one civilian = one one-man `SCR_AIGroup`,
because waypoints and `GetOnAgentAdded` attach to groups, D3 of that feature's plan). A group carries
no `SCR_DamageManagerComponent`, so core's stock dead-check answers `false` forever and a dead
civilian would never be pruned — its husk and its waypoints would accumulate for the whole session.
The alternative considered and **rejected** was a `GetOnCharacterKilled()` subscription: it needs a
global subscription, a character→agent→group→source resolution and its own death-vs-teardown
disambiguation, all to reach a conclusion core already reaches inside its own ownership window. Only
one of the two was built.

**Why it is not a breaking change.**

- `IsEntityDead`'s body **is** the manager's previous inline check, moved unchanged. Every existing
  source keeps pruning exactly what it pruned before; the manager's own `IsAmbientEntityDead` stays
  in place as the fallback for an instance with no config to ask.
- `OnEntityPruned` defaults to a no-op, so a source that ignores it is unaffected.
- No signature changed, nothing was removed, no persisted payload field moved, and
  `CONFIG_STREAM_VERSION` did not move.

**The one contract this adds:** `OnEntityPruned` fires **after** the entity has left both the source's
entity list and the manager's entity→source reverse map. The entity is therefore no longer owned when
a consumer sees it, and `ReleaseAmbientEntity()` on it is a no-op. That ordering is what makes the
rule *leave the body, delete the companions* safe: a consumer deletes the pruned entity's waypoints
and its emptied husk there, and deliberately leaves the corpse in the world.

### 2026-08-17 — `GetAllHandles()` on the manager

**Requested by:** `virtualization/movement` (feature #3 of the epic), Phase 2 — its implementation
plan §3.7 and task T2.1. Built by that feature, in core's file.

**What was added** (`Scripts/Game/GameMode/Virtualization/OVT_VirtualizationManagerComponent.c`,
beside `FindGroupsBySystem` in the TRACKED GROUPS - QUERIES section):

```c
array<int> GetAllHandles();  //!< every registered handle; map order, NOT stable
```

Same shape as the two finders above it: allocate the result array, null-guard `m_mRecords` (so a
client or a call before initialisation gets an empty array, never null), iterate, return. It has
**no call site in core** and none anywhere else yet — `movement`'s tick, which is the requester, lands
in that feature's Phase 3.

**Why `FindGroupsBySystem` is not sufficient.** `movement` advances *every* registered dormant group
whose plan has something to advance, whoever registered it. `ownerSystem` is a deliberately free-form,
mod-extensible string (`api.md` §3) — there is no enumerable set of system names, and any list
`movement` hard-coded would silently skip a consumer or a third-party mod's groups the moment one
registered under a tag it had never heard of. Per-owner and per-system finders answer "which of *mine*
are registered"; nothing on the frozen surface answered "what is registered".

**Why it is not a breaking change.**

- Nothing was renamed, no signature changed, no existing line of core was touched — the diff is the
  method and its doc comment, 23 added lines and zero removed.
- No persisted payload field was added, moved or re-meant, and `CONFIG_STREAM_VERSION` did not move.
  The registry serializer neither reads nor writes anything new.
- No behaviour changed anywhere: the method is a pure read over `m_mRecords` with no side effects, and
  at the time it was added nothing called it.

**The one contract this adds:** the returned order is the registry map's own and is **not stable** — a
map has no ordering guarantee, and core's own ambient tick keeps a separate order array for exactly
this reason (a round-robin over an unstable order can starve an entry forever). Ordering was
deliberately left out of core: `movement` sorts the handles ascending in its own world-free maths,
which works because handles come from a monotonic counter, so ascending order *is* registration order.
A consumer that needs stability must do the same.

---

## Gotchas & Learnings

### 0a. Live-session observer findings (user play-test + VIRT-DIAG console dump, 2026-08-17)
- **The GM free camera is an observer**: flying the GM cam to a dormant-candidate group keeps it spawned (caught live: SP observer 109 m from the group, player character 6.4 km away). Operator-facing consequence: **server admins spectating in GM keep everything near their camera spawned** — document in operator/GM docs when consumers migrate.
- **The deploy-point preload MP observer upgraded correctly to follow-the-character** in the SP/Workbench session (MP observer position == player character position at 6.4 km). The Phase 1 "parked phantom at deploy point" hazard did not manifest here; it remains a risk for dedicated/MP joins where the preload-finished RPC can be lost.
- Diagnostic tooling: `.tmp/virt-observer-diag.c` (paste-into-Script-Console one-shot dump: group state, rings, engine observer verdict, full SP/MP observer tables with distances).
- Registry gap re-confirmed in practice: the debug group registers the first resolvable registry entry = the 2-man sentry patrol; `agents=2` is a full roster, not corruption. **Partially closed 2026-08-17:** `rifle_squad` (6-slot `Group_USSR_RifleSquad`, GUID `{6A9C4D1B0000000A}`, cost 30) added to `USSR_OverthrowData.conf` — a shipped registry addition (consumers need a mid-size entry), appended last so `FindComposition()` still returns `light_patrol` first.
- Debug affordances extended for GM-camera testing (the GM cam is an un-removable engine observer): `m_iDebugTestGroupSpawnDistance` (−1 = global; ~300 = watch spawn/despawn from a zoomed GM cam at 400–600 m; warning: resolved 0 = Manual policy) and `m_sDebugTestGroupName` (default now `""` = first resolvable entry, was hard-coded `light_patrol`; set `rifle_squad` for the 6-man kill test; falls back with WARNING).

### 0. `InsertObserverSP(key, x, z, null)` froze the game client (2026-08-17)
**Problem:** the All-group run froze the main thread (total log silence, per-case timeout never fired, killed at 300 s) starting the exact frame a test case that parked a null-entity SP observer into `ObserversSystem` began — in the one world where the system is actively consumed (real player present). The Fast world (observers never honoured) ran the same call harmlessly.
**Solution:** removed the parking; the case's designed fallback (virtual-dispatch assertion) covers observer-less worlds and natural observers cover the rest. Re-run green (All 209/209).
**Lesson:** a null-entity SP observer insert has ZERO vanilla callers (the only null-entity insert in the 1.8 tree is the MP variant, `SCR_SpawnRequestComponent.c:541`). Never call `InsertObserverSP` with a null entity. Candidate upstream report (check `docs/reforger` RFG series first).

### 1. Known upstream wrinkles inherited by this feature (from planning)
- `SpawnWaitWaypoint(pos, time)` accepts `time` and never applies it (`OVT_OverthrowConfigComponent.c:485`) — do not rely on wait durations (T3.2).
- `RandInt` is max-exclusive; `RandInt(n, n)` is an engine error — `RollCountSafe` guards this (T2.4).
- BUG-118: untracking entities still in the persistence IsTracked retry queue needs care (T3.6).
- `Event_OnInit` fires only on COMPLETE group fill in 1.8 — never gate on it.
- Deaths must come from the kill invoker, never `GetOnAgentRemoved` (D2a) — teardown deletions are not deaths.

---

## Phase 1 Spike Answers (GATE — fill with evidence before Phase 2)

- **T1.1 ObserversSystem live in Overthrow world:** ✅ CONFIRMED RUNTIME 2026-08-16 (user play-test log): `ObserversSystem found=yes | VisibilityConsumerSystem found=yes`, `observers at campaign start: total=2 SP=1 MP=1`, `AI world is ChimeraAIWorld=yes`. Static analysis (same-GUID delta over vanilla's `ChimeraSystemsConfig.conf`, which adds the systems at :31/:33) also held.
- **T1.2 Lifecycle behaviour + D2 seam:** ⚠️ PARTIALLY CONFIRMED + CRITICAL FINDING (user play-test 2026-08-16, full log in Discord thread):
  - ✅ `IgnoreSpawning(true)` → 0 members at spawn; policy stamp reads back (300/350, NORMAL, eliminateWhenReached).
  - ✅ Progressive fill via the queue works: agents 0→6 over ~8 s, one `ExpandOneMember` per dispatch, ~0.5 s apart.
  - ✅ D2 premise proven deterministically: vanilla `ExpandOneMember` ALWAYS picks `slotIndex == agentsBefore` (first-N refill) — identity truth is structurally lost without our mask. `liveCapacity` caps refill at dormantAlive.
  - 🔴 **D2(b) corruption is WORSE than planned — not just budget under-fill: ANY despawn during an in-progress refill records not-yet-spawned slots as DEAD.** Observed live with ZERO kills: three despawn-mid-fill cycles took the group 6→4→2 (`DESPAWNING liveAgents=4 rosterSlots=6 => RECORDED dormantAlive=4 dormantDead=2`, then `liveAgents=2 => dormantAlive=2 dormantDead=4`). User-visible symptom: "group of 6 slowly dropped to 2, nobody killed". Core's mask + post-despawn `SetDormantCounts` re-assertion (D2b) is MANDATORY, not defensive.
  - ✅ **Anomaly RESOLVED** (engine reference investigation 2026-08-16, full file:line evidence below): the probe was NOT buggy — a real **non-player observer** sat within 350 m of the group. Observers = local cameras + connected players + fixed MP inserts (`ChimeraAIGroup.c:23`). Two concrete parked-observer mechanisms exist in this build: (a) `SCR_SpawnRequestComponent.StartSpawnPreload` inserts a **fixed-position MP observer at the deploy point with `pEntity=null`** (`SCR_SpawnRequestComponent.c:541`), upgraded to follow-the-character only on the preload-finished RPC / controlled-entity change, removed only in the destructor — Overthrow does not override this; (b) Overthrow's own StartCam (`OVT_OverthrowGameMode.c:1321-1337`). Also: `SCR_2DOpticsComponent.c:620` registers a **far observer projected up to the sight's zeroing range along the view line** while aiming through optics — it sweeps >100 m/s under mouse-look. `SP=1 MP=1` at DoStartGame = StartCam + deploy-point preload observer.
  - 🔴 **Engine purge hole (second respawn mechanism):** `DespawnMembers` early-outs at `aliveCount==0` (`SCR_AIGroup.c:2869-2870`) BEFORE `PurgeSpawnRequestsForGroup` (`:2878-2883`), and LifecycleTick's despawn branch is gated on `GetAgentsCount()>0` (`:3005`) → **a dormant group that enqueued spawn requests and then lost its observer never purges them**; LifecycleTick also re-enqueues one request/second unconditionally while an observer is inside the spawn ring (`:3072`, no already-queued guard), and multi-slot requests are serviced as "spawn 1, re-enqueue rest" (`:2671-2675`) with dequeue-time re-validation semantics not visible from script. Stale/continuation requests can materialise members unguarded; the next 1 Hz tick then despawns mid-fill = the corruption trigger.
  - ✅ Lifecycle semantics confirmed from source: 1 Hz `LifecycleTick` (`:128`, `:2985-3073`); despawn when no observer within despawnDist AND agents>0; real hysteresis band 300–350 (`:3025-3027`, a stationary in-band observer cannot flap); `RequestSpawn(-1, m_fSpawnDistance)` — dequeue re-check uses the SPAWN ring; `HasObserverInRange` is linear metres measured from the **group entity origin** (`ChimeraAIGroup.c:24`, proof `SCR_DefenderSpawnerComponent.c:621-631`); queue dispatch 2 Hz, one member per dispatch (~3 s for 6 slots).
  - 🔴 **Ratchet is permanent & engine-enforced:** `SetDormantCounts(GetAgentsCount(), totalSlots-alive)` at `:2876`; refill capacity `totalSlots - dormantDead` (`:2726-2729`); refill always picks `slotIndex = aliveCount` (`:2731`) so the roster TAIL dies first; `dormantAlive==0` ⇒ RequestSpawn refuses forever (`:2680-2688`); `SetDormantCounts` has exactly two script callers (DespawnMembers + save restore) — **nothing engine-side ever re-corrects**. C++ eviction routes through the same script `DespawnMembers` (`:2854-2857`).
  - 🔴 **NEW HAZARD — eliminate-when-reached + veryNearBlock:** with `SetEliminateWhenReached(true)`, once `wasInSpawnRange` was true, an observer inside `veryNearBlockDist` (default 150 m, `:126`) of a dormant group can trigger outright **deletion of the group entity** (`:3038`, `:3045-3059`). With parked/phantom observers in play this is unrecoverable loss for a persistent campaign group. Phase 3 must verify the exact dormantAlive precondition and/or defer stamping eliminate-when-reached until the mask reports wiped.
  - ✅ `OnEmpty` fires on EVERY despawn teardown (not just wipes) → T3.6's wiped-while-spawned detection must NOT use OnEmpty alone; needs dormancy-aware guard.
  - Kill-3/full-wipe/boundary manual checks: superseded — first-N refill proven deterministically from `ExpandOneMember` logs + source (`:2731`); wipe semantics read from source (`:2680-2688`); hysteresis confirmed in source. Survivor-count behaviour was observed first-hand by the user (the 6→2 ratchet).
  - **Open question (optional, not phase-blocking):** phantom-observer vs stale-queue attribution for THIS specific flap. Decisive experiment if ever needed: log `HasObserverInRange(300/350)` + `GetOrigin()` inside the `ExpandOneMember` override at dispatch time, and dump `ObserversSystem.GetObservers()` positions in the monitor. Core's defensive design covers both causes regardless.

### Core-layer defensive requirements (from the investigation — bind Phase 3/5)
1. Mask is the ONLY roster truth; engine dormant counts are scratch. Re-assert `SetDormantCounts(maskAlive, maskDead)` after EVERY despawn (already D2b — now proven mandatory, not defensive).
2. Handle despawn-mid-fill: the re-assertion must run after vanilla's `DespawnMembers` bookkeeping (modded-class hook ordering), and refill logic must tolerate capacity being wrong until re-asserted.
3. Observer semantics: core's "nearby" must be `HasObserverInRange`/ObserversSystem consistently (accept cameras/preload observers as observers) — never mix with player-distance loops.
4. Parked-observer audit: deploy-point preload MP observer (never removed until destructor) + StartCam keep everything within despawn range of campaign start permanently non-dormant — document for consumers; consider it when choosing default distances.
5. Verify `SetEliminateWhenReached` precondition before stamping at registration; safest shape: only enable once the mask says wiped (or subscribe + re-verify against mask before honouring).
- **T1.3 Persistence/tracking mechanics for runtime-spawned groups:** STATIC ANSWER (⚠️ contradicts plan §3.3 step 6 optimism — plan amendment required):
  1. Vanilla DOES auto-match runtime-spawned groups: `EntityClassPersistenceConfigRule` on `EntityClass "AIGroup"` (`Configs/Systems/Persistence/Configuration/AI/AIGroup.conf:2`, wired via `Common.conf:24,93`), inherited by `Overthrow.conf`.
  2. BUT Overthrow **unconditionally untracks every AI group** — `Scripts/Game/Modded/SCR_AIGroup.c:30-35` (`EOnInit` → `UntrackTransient(this)`, the BUG-118 fix, deferred via the retry queue `OVT_PersistenceManagerComponent.c:751-770`).
  3. AND `Overthrow.conf:80-82` overrides the AIGroup config `{654CB71C1CF2147B}` with `SelfSpawn 0` (AIUnit `:77-79`, AIWaypoint `:85-88`) — so even a tracked record could never re-instantiate on load.
  → Phase 3 needs BOTH a per-group exemption from the modded untrack AND a self-spawn path for registered groups only. `OVT_PersistenceTracking.MarkForSelfSpawn` is NOT usable (`OVT_PersistenceTracking.c:132` "DO NOT USE — corrupts the save", BUG-116). Likely shape: dedicated higher-priority `EntityPersistenceConfig` with an Overthrow rule class matching only registered groups. ✅ RUNTIME CONFIRMED 2026-08-16: spike logged `IsTracked=no persistentId=''` both immediately after spawn AND 10 s later (untrack retry queue drained) — registered groups are invisible to persistence today, exactly as predicted.
- **T1.4 Test-world verdict (what Init tier may assert):** ✅ VERIFIED 2026-08-16 (`tools/run-tests.sh OVT_TEST_VirtSpikeSuite` — 2/2 green, 15 s). Evidence (`.tmp/run-tests/console.log` 17:44:11):
  - `GetAIWorld()` non-null AND is `ChimeraAIWorld` → **RequestSpawn ENQUEUES in the test world** — Init-tier cases may assert on queue semantics (no synchronous-fallback divergence).
  - `ObserversSystem present = true` in the autotest world.
  - A group spawned with `IgnoreSpawning(true)` + ProximityDriven and no observers stayed **memberless for 240 frames** (`dormantAlive=-1` never-despawned sentinel confirmed, `IsDormant=false`) → **Init-tier registration cases are safe** (register/assert/unregister without anything materialising).
- **T1.5 Scale-probe numbers (~100 dormant groups):** ✅ 2026-08-17 (user session, logs `logs_2026-08-17_00-06-09`): registration of 100 dormant groups took **73 ms wall** (0.73 ms/group, 0 failures); script allocator 3020116→3036257 KB (+16 MB, ~160 KB/group incl. unrelated noise); **Task Manager process delta negative** (4800→4500 MB before/after) → per-group process memory below measurement noise. FPS samples 6.7→9.4 over first 25 s = campaign-init settling, NOT attributable to the groups (no baseline run; each dormant group costs one 1 Hz trivial tick). D1's cost model is priced: dormant-entity overhead is negligible at 3× realistic campaign density. Phase 6 T6.1 re-measures with a baseline + client replication probe.
- **Registry gap finding:** shipped faction registries (`USSR_OverthrowData.conf:5-13`) contain only `light_patrol` (2 slots) and `light_fireteam` (4 slots) — no mid-size entry suitable for "kill 3 of 8". Spike spawns `Group_USSR_RifleSquad` (6 slots, 6 distinct roles) directly. Epic-relevant: consumers will need richer registry entries.

### Spike code inventory (all `[OVT-VIRTSPIKE]`, removed at end of Phase 1)
- `Scripts/Game/GameMode/Virtualization/OVT_VirtSpike.c` (whole spike; toggles `SPAWN_TEST_GROUP`=true, `SCALE_PROBE`=false, `PROVE_ARBITRARY_SLOT`=false)
- `Scripts/Game/Tests/TestSuites/OVT_TEST_VirtSpike.c` (suite `OVT_TEST_VirtSpikeSuite`, in no group)
- `Scripts/Game/GameMode/OVT_OverthrowGameMode.c` — 3-line hook at end of `DoStartGame()` (~:409)
- `Scripts/Game/Modded/SCR_AIGroup.c` — observe-only `ExpandOneMember` override + `OVT_VIRTSPIKE_SpawnSlot(int)`
- Spike quirks: statics survive Continue/new-campaign in the same process — restart client between spike runs.

---

## Testing Approach

- **Logic tier (Fast):** `OVT_TEST_Logic_Virtualization.c` — world-free statics only; tier grep bans manager-accessor/game-mode-getter identifiers incl. comments.
- **Init tier (Fast):** additions to `OVT_TEST_InitSuite.c` — scope set by the Phase 1 T1.4 verdict.
- **Persistence tier (All):** two round-trip cases on the shared `OVT_TEST_PersistenceRoundTripGate`.
- Every new case needs a recorded can-fail proof; `maxAttempts` banned.
- Test GUIDs: Fast `{6A6E29FF47ECB840}`, All `{6A6E2A002F53A581}`. Baseline at feature start: Fast 146 / All 190 (per 2026-08-16 merge).

### ⚠️ EPIC-SCOPED TEST SCAFFOLDING IN THE TREE (user decision 2026-08-17: stays ON for the whole epic build-out — remove at epic completion)
`Scripts/Game/GameMode/Virtualization/OVT_VirtPlaytestKillSwitch.c` (`DISABLE_LEGACY_AI_SPAWNS = true`) + tagged guards silence ALL systemic legacy AI spawning (tower garrisons, base upgrade ticks + resource spends, QRF queue, town civilians, deployment evaluation + infantry-group backstop) so each epic consumer (`civilians`, `movement`, `integration`) is play-tested without legacy noise. Player-triggered spawning (recruits, bought patrols, jobs, gun dealer) untouched. `OVT_TEST_Campaign_GMGroupRegistry` carries a kill-switch-aware guard (trivial-pass with a loud WARNING while the switch is on — its only observable producers are the gated paths), so **all suites stay green as a gate during the epic**. Removal set: `grep -rn "OVT-VIRT-PLAYTEST-ONLY" Scripts/` (10 lines incl. the test guard + the switch file). Note for `integration`/`base-defense-migration`: several guarded paths are exactly the code those features retire — remove each guard as its system migrates, and the whole switch at epic end.

### Needs human verification (running list — feature complete)
**✅ VERIFIED by user play-test 2026-08-17 (Discord):**
- ~~Phase 1 T1.2 play-test~~ ✅ 2026-08-16
- ~~Workbench prefab check~~ ✅ implicit 2026-08-17 — user edited the manager's debug attributes on the prefab in Workbench (component present, attributes editable, prefab opens clean)
- ~~§6 steps 1–4 lifecycle + slot accuracy~~ ✅ "lifecycle mode: green" (GM-cam cycling with 300 m debug ring: dormant → progressive fill → despawn → refill) + "6-man kill-test mode: green" (kill 3 of rifle_squad, despawn/respawn → exactly the survivors' roles return — F4/D2 verified in the live game)
- ~~§6 step 12 save/Continue~~ ✅ "6-man kill-test save/continue: green" — **F8/F9 verified: Route B re-creation preserves exact surviving slots across quit→Continue** (was the one unverified path)
- ~~F7 per-registration override~~ ✅ implicit — debug group ran a 300 m override while the global stayed 1750

**Still open (lower value, non-blocking):**
- §6 step 8 scale probe (40-group fast-travel, `m_iDebugTestGroupCount=40`) + step 9 drive-past
- §6 steps 10–11 ambient source spawn-spread + release-survives-despawn (needs a throwaway config)
- §6 step 13 two-campaigns-in-one-session restart hygiene (Phase 6 fixed 4 bugs here; live confirm)
- F6 global-config distance experiment via `$profile:Overthrow_Config.json`
- Fail-proof executions recorded in preambles but not executed red. Optional review debt.
- ⚠️ Before committing: remove/flip the `OVT-VIRT-PLAYTEST-ONLY` kill switch (see section above).

### Phase 3 → Phase 4/5 handoff (from the implementing agent)
- **Self-spawn STOPPED (Phase 5 decision owed):** only 3 native persistence matchers exist (scripted IsMatch dead per BUG-018); Prefab rule fails (shared, mod-extensible prefabs), ComponentClass rule fails (no runtime component add), `EntityClass "AIGroup"` is a superset AND the untrack retry queue gives up at 60 attempts (`OVT_PersistenceManagerComponent.c:855-863`) leaving lazy-registered orphans that class-wide `SelfSpawn` would DUPLICATE on every load. Route A = bounded config + decoded-save verification; **Route B = manager-driven re-creation from registry (recommended; record carries `m_vPosition`/`m_Plan` for exactly this)**. Also: vanilla `SCR_AIGroupSerializer.SerializeSpawnData` writes only version+prefab — group TRANSFORM may not be in the payload at all; decide by decoding a save, not reading code.
- `m_bPersistGroupEntities` (manager attribute) gates the whole exemption seam, defaults **false** (tracking without self-spawn = one orphan record per group per boot, the BUG-118 shape).
- `OnDelete` DETACHES, does not destroy (deliberate T3.11 deviation — deleting tracked entities at quit-to-menu would erase the records persistence exists to keep).
- `spawnDistanceOverride == 0` → stamped `Manual` policy (SetLifecyclePolicy ignores non-positive distances — a 0 ring would silently become vanilla 600/800).
- `IsSpawned()` no longer consults `IsDormant()` (core overwrites the counts that flag derives from).
- `PushSlotMask(record, group)` is public — **Phase 5 MUST call it after replacing `record.m_aSlotAlive` on relink** (mask shared by reference between record and group).
- Waypoint helper line numbers moved: SpawnPatrolWaypoint :428, SpawnDefendWaypoint :489, SpawnWaitWaypoint :506 (time still ignored upstream), GivePatrolWaypoints :522.
- Modded-class surface: `ArmPersistenceExemption`/`IsOVTPersistenceExempt`, `SetOVTSlotMask`(by ref)/`ClearOVTSlotMask`/`GetOVTSlotMask`/`HasOVTSlotMask`/`GetOVTSpawnedSlots`, `ReassertOVTDormantCounts` (no-op while dormantAlive<0). New file `Scripts/Game/Modded/SCR_AIGroupSerializer.c` (load-path exemption arm).

### Phase 2 → Phase 3 handoff (from the implementing agent)
- `RegisterGroup` validates then stores a booking: `m_Group` null, `m_aSlotAlive` empty, `position`/`plan` DROPPED after validation (record has no position field) — T3.1 must spawn the entity inside `RegisterGroup` from the arguments. Insertion point marked `// Phase 3 (T3.1)` with ordered steps incl. the no-eliminate-when-reached amendment.
- `ReportMemberKilled` already does mask-flip → `IsWiped` → `OnGroupWiped` (before removal) → removal; idempotent per slot; inert until the mask is populated. T3.5/T3.6 call sites marked inside it.
- `GetAliveMemberCount()` is mask-first (engine counts only for maskless groups) — deliberate deviation per Phase 1 amendment 2.
- `RegisterAmbientSource` NOT declared yet (param type is Phase 4's; no forward declarations in EnforceScript) — other 4 ambient methods are inert stubs; signature frozen in api.md §4.
- Additive API: `GetSpawnDistance(handle)`, `GetImportance(handle)`, `GetGlobalSpawnDistance()`; ragged waypoint plans rejected −1+WARNING.
- Manager attributes `m_fDespawnHysteresis`, `m_iFallbackSpawnDistance` exist; `ResolveDespawnDistance` not yet called.
- ⚠️ EnforceScript reserved identifiers: `owned` (known) AND **`out`** (new find) — both break as local names.

---

## Session Notes

### 2026-08-16 17:18
- Feature started via /autorun-feature (Discord). Docs scaffolded from implementation.md Revision 2.
- Next: Phase 1 spike — delegate spike implementation, run headless parts, coordinate user play-test for T1.2.

### 2026-08-17 (autorun completion)
- Phases 2–6 built via /autorun-feature: Phase 2 (`component-developer`, Fast 156), Phase 3 (advanced, All 204), Phase 4 (Fast 163), Phase 5 (advanced, Route B, All 209 after the null-observer freeze fix), Phase 6 (All 210).
- Route B persistence ruled by announced default (user away); payload FROZEN (see Phase 5 sections).
- One real regression caught + fixed at the Phase 5 gate (gotcha 0: null-entity `InsertObserverSP` client freeze).
- Phase 6 restart audit fixed 4 latent teardown bugs before any consumer ever hit them.
- Feature is Ready for Review; sibling features `civilians` and `movement` are unblocked against the frozen api.md.

### 2026-08-16 18:00
- Spike built by component-developer agent (files in "Spike code inventory" above); compile-check 0 (6110 files).
- T1.4 CLOSED via `tools/run-tests.sh OVT_TEST_VirtSpikeSuite` (2/2 green) — verdicts recorded above.
- T1.1/T1.3 static analysis recorded; T1.3 contradicts plan §3.3 step 6 / D8 → amendment owed at T1.6 (bespoke opt-in tracking + self-spawn path; MarkForSelfSpawn is save-corrupting per BUG-116 — do not use).
- ⏸️ BLOCKED on user play-test (T1.2, T1.3 runtime confirm, T1.5). Play-test script posted to Discord (beast_reply fallback — beast_ask transport was flaky). Phase 2 must not start until answers land here.
- NOTE for resuming session: spike statics survive Continue/new-campaign — user must restart the client between spike runs. `SCALE_PROBE` and `PROVE_ARBITRARY_SLOT` default false.

---

*Update this file at the end of each work session.*
