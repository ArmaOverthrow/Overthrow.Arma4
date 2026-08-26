# Virtualization Movement - Context & Decisions

**Last Updated:** 2026-08-17 15:30
**Current Phase:** COMPLETE — all 4 phases + 3 play-test fixes; user play-test passed
**Status:** ✅ Ready for Review — 22/22 tasks, Fast 190 / All 236 green, user confirmed "all play-test items are green" (2026-08-17)

**Epic:** `virtualization` (feature 3/5). Consumes `core/api.md` (🔒 FROZEN — §10 `movement` table is the whole surface; ONE additive method `GetAllHandles()` allowed, Phase 2). Parallel-safe with `civilians` (no dependency either way). `integration` depends on this.

---

## Quick Status

**What's Done:**
- ✅ Plan approved (implementation.md, 4 phases, D1–D13, findings F-A/F-B baked into phases)
- ✅ Phase 1: `OVT_VirtualMovementMath.c` (10 statics + 3 consts) + `OVT_TEST_Logic_VirtualMovement.c` (7 cases). Compile 0, **Fast 186 green**. Logic-tier grep clean (file header avoids the word "world" — the case-insensitive grep would trip on it).

- ✅ Phase 2: `GetAllHandles()` on core (+23/-0, exactly one method), api.md §3/§10 + header note, dated core context entry, Init case `GetAllHandlesEnumeratesRegistry`. Compile 0, **Fast 187 green**.

- ✅ Phase 3 (code): T3.1 fixture made stationary + whole-tree `RegisterGroup(` sweep, T3.2 moved-position claim, `OVT_VirtualMovementState`, `OVT_VirtualMovementManagerComponent` (tick + derive + arrival + write + debug log), `OVT_Global.GetVirtualMovement()`, game-mode field/Init/PostGameStart, prefab text-wire `{6B4C3D6E00000010}`. Compile **0**; every acceptance grep clean. **All** suite + Workbench prefab check + play-test still owed.

- ✅ Phase 4: three Init cases (tick advances a dormant group / a DEFEND-only plan never is / the manager resolves and its state map does not leak), one read-only diagnostic getter `GetTrackedCount()`, the `For integration` section below, the T4.5 scale-check recipe, and the epic-overview row. Compile **0** (probed: an unknown type in the new fixture produced exit 1 naming the line).

**What's Next:**
- ✅ Orchestrator ran **All** `{6A6E2A002F53A581}`: **233/233 green** (2026-08-17 08:25; was 225 baseline + 8 new movement/seam cases). Console log clean — no script errors.
- ✅ Orchestrator ran **Fast** `{6A6E29FF47ECB840}` after Phase 4: **190/190 green** (2026-08-17 09:40) — after one real fix (Init worlds never run PostGameStart; the three movement cases now install the tick themselves, see Gotcha 6) and one unrelated `Setup_Checkpoint` 500 ms I/O flake (Gotcha 7).
- ✅ User confirmed 2026-08-17: the text-wired component resolves in Workbench ("hand-authored GUIDs are never a problem" — standing guidance, applies to future text-wired components too)
- ✅ User play-test PASSED 2026-08-17: "all play-test items are green" (§6 steps + T4.5 scale check; the two defects it surfaced first are fixed — see the ~15:10 session note; scale numbers not individually recorded)
- 📋 Next in the epic: `integration` (read the "For `integration`" section below first)

**Blockers:**
- None

---

## Key Files

### Core Implementation (this feature)
- `Scripts/Game/GameMode/VirtualMovement/OVT_VirtualMovementMath.c` - world-free progression statics (Logic-tier subject)
- `Scripts/Game/GameMode/VirtualMovement/OVT_VirtualMovementState.c` - transient per-group progress (never persisted)
- `Scripts/Game/GameMode/VirtualMovement/OVT_VirtualMovementManagerComponent.c` - the manager + 2 s round-robin tick
- `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_VirtualMovement.c` - Fast Logic cases

### Related Files
- `Scripts/Game/GameMode/Virtualization/OVT_VirtualizationManagerComponent.c` - frozen core; gains ONLY `GetAllHandles()`
- `docs/features/virtualization/core/api.md` - the frozen contract (§10 `movement` table)
- `Scripts/Game/Tests/TestSuites/Persistence/OVT_TEST_PersistenceRoundTripSuite.c` - shared fixture made stationary (T3.1, finding F-A)
- `Prefabs/GameMode/OVT_OverthrowGameMode.et` - manager text-wired in Phase 3

---

## Important Decisions

(Plan decisions D1–D13 live in implementation.md §5 — not repeated here. Session-discovered decisions go below.)

### Key plan facts every phase must respect
- **IsSpawned() is the whole gate** — vehicle groups excluded by construction (huge `spawnDistanceOverride`, always live). No vehicle flag, no road routing (D2).
- **No serializer/persistence/replication/config-stream change** (D3, D13). Progress re-derived by projecting position onto the plan polyline.
- **The plan is the opt-in** (D10): empty/DEFEND-only plan = never moved.
- **Movement never touches waypoint entities** (D11) → finding F-B (resume from plan index 0) is documented, not engineered around.
- **Water rule:** virtual accumulator advances; written origin holds at last land sample (D6).
- **Per-group dt** (`m_fLastTickMs`), clamped [0, MAX_STEP_SECONDS] — flat cost at scale (D5).
- ⚠ EnforceScript reserved local names: `out` AND `owned` (core's Phase 2 find).
- ⚠ `vector.Distance` +1 ULP at 1000/2000 m — no exact distance boundaries in tests.

---

## Gotchas & Learnings

### 1. F-A — movement moves other people's test fixtures ✅ CLOSED in Phase 3 (T3.1)
**Problem:** `OVT_TEST_PersistenceRoundTrip_VirtualGroups_SurviveSaveAndReload` registered a 150 m two-point PATROL plan and asserts ±1 m position across save/reload — movement's tick would drift it and redden the All gate.
**Solution:** T3.1, landed **before** any tick code existed: the fixture's waypoint **types** are now `DEFEND` (two distinct positions, two types, two params, `m_bCycle` all still round-trip, so every payload claim is unchanged in number and strength; the `PATROL`-type assertion became a `DEFEND`-type assertion). The reason is in the case preamble and in `BuildPlan()`'s doc comment, so nobody reverts it by accident.

**The whole-tree sweep (`grep -rn "RegisterGroup(" Scripts/Game/Tests/`), verdict per site.** Two properties make a fixture safe: **(a)** it registers a null / empty / DEFEND-only plan (nothing to advance), or **(b)** it registers and unregisters inside **one frame** (a `CallLater` tick cannot interleave).

| Site | Case | Verdict |
|---|---|---|
| `Init:3540`, `Init:3562` | `RegisterRefusesUnknownComposition` | **safe** — both registrations are *refused* (`-1`), no record is ever booked |
| `Init:3746` | `RegisterBuildsDormantGroup` | **safe** — `plan = null` (a) **and** single frame (b) |
| `Init:3903`, `Init:3905` | `GetAllHandlesEnumeratesRegistry` | **safe** — no plan argument → null (a) **and** single frame (b); asserts containment/counts, never position |
| `Init:4071` | waypoint ownership / deletion | **safe by (b) only** — this one registers a genuinely **movable** plan (PATROL + MOVE, 120 m, cycling) but unregisters in the same frame, before it starts polling; asserts nothing about position |
| `Init:4216` | `DeathsFlipMaskAndWipeRecord` | **safe** — `plan = null` (a) + single frame (b) |
| `Init:4404` | mask-driven refill | **safe** — `plan = null` (a) + single frame (b); deliberately materialises a member, which the `IsSpawned` gate skips anyway |
| `Persistence:3913` | wiped group | **safe** — `plan = null`; asserts absence, never position |
| `Persistence:4012` | resurrection group | **safe** — `plan = null` |
| `Persistence:4344` | BOGUS group | **safe** — `plan = null` |
| `Persistence:4564` | **THE fixture** | **was UNSAFE — fixed by T3.1** (types → DEFEND) |

**Lesson:** movement advances EVERY registered dormant group with a movable plan, including one a test registered. A new fixture must satisfy (a) or (b) — or accept that it will walk.

### 2. F-B — a moved group resumes waypoints from the top
**Problem:** Dormant groups never complete a waypoint (the engine only retires one live AI actually finished), so on materialisation live AI walks to plan index 0 regardless of where movement put the origin.
**Solution:** Documented limitation (§3.8, D11), restated in the manager's file header. Cost: ≤ one lap for a cycling patrol, nothing for a single-target plan. Any real fix is a **core** change via api.md's additive process — never a waypoint rewrite bolted onto movement. The acceptance grep for waypoint/group-lifecycle identifiers in the feature directory is empty, comments included.

### 3. Lost movement state is re-derivable BY DESIGN — a "lost progress" report is not a defect
Every field of `OVT_VirtualMovementState` is recoverable from `GetPosition(handle)` + `GetRecord(handle).m_Plan`. Losing the whole map — world teardown, a `Continue`, a restart, a spawn cycle, a consumer teleporting a group — costs at most one re-projection per group, i.e. re-walking part of one leg. Direction on a non-cycling ping-pong route is **not** recoverable and always resumes `+1`, so a resumed patrol may walk its route the other way. **All of this is intended (D3):** if a play-test or a bug report says "movement forgot where a patrol was after a load", the expected answer is "yes — and it re-derived, which is the design", not a fix.

### 4. The state map holds no entry for a plan that cannot move — but keeps one that latched at runtime
A group whose **plan** is empty / null / DEFEND-only is re-classified cheaply on every pass and holds **no** map entry, which is what keeps the map empty in a campaign full of garrisons (and what makes Phase 4's T4.3 "tracked count is 0" claim true). A group that latched stationary **at runtime** — it reached a DEFEND point, or its route ran out — **keeps** its entry: dropping it would let the next pass re-derive a movable plan and walk the group straight off the post it just took up. Phase 4's diagnostic getter should count the map as-is and assert against the *plan-stationary* case.

### 5. The count-guarded purge can leave a stale entry for a while (bounded, harmless)
The purge only runs when `m_mState.Count() > handles.Count()`, so unregistering a *walking* group while a *garrison* holds no entry can leave one stale entry behind until the next real shrink. It is never touched (the slice only walks live handles) and handles are monotonic and never reused, so it can never be misattributed. When **everything** is unregistered the map is cleared outright, which is the case Q7/T4.3 assert.

### 6. Init-tier test worlds NEVER run PostGameStart — a tick-dependent case must install its own tick
**Problem:** The first Fast run after Phase 4 reddened T4.1 with "moved 0.451 m in 10000 ms (0 handle(s) tracked)". The Init suite does not override `RequiresStartedCampaign()`, so `DoStartGame()` → `PostGameStart()` never runs in that world and the movement `CallLater` was never installed. T4.2/T4.3 were passing **vacuously** (nothing could have moved anything).
**Solution:** All three movement Init cases now call `OVT_Global.GetVirtualMovement().PostGameStart()` in their Arrange/Begin step — it is public and idempotent (`m_bTickRunning` latch), so repeat installs are free and a started campaign is unaffected.
**Lesson:** any future Init case asserting *tick behaviour* (core's ambient tick included) must install the tick itself, or it asserts against a tick that never ran.

### 7. Direction is unrecoverable by projection on coincident-leg routes; live handoff now uses `GetCurrentPlanIndex`
**Problem (user play-test):** despawned groups resumed walking AWAY from their live current waypoint. On a two-point cycling patrol the outbound and closing legs are the same line segment, so `ProjectOntoPlan` ties on every mid-leg position and its tie-break always answers the outbound leg — a group that despawned walking home resumed walking out.
**Solution (user-approved A):** core gained `GetCurrentPlanIndex(handle)` (third additive change — entity-identity match of `GetCurrentWaypoint()` against the owned waypoints; cycle entity → 0; -1 = refuse to guess). Movement's adoption consults it first (`liveIndex` param on `DeriveState`); projection stays the fallback for loads, teleports and off-route deliveries — where R2's accepted ambiguity still applies.
**Lesson:** position-only resume can never recover *direction* when route legs coincide; any consumer route that doubles back on itself has the same property. The live entity is the only truth, and the read belongs in core (movement's greps ban waypoint identifiers).

### 8. `Setup_Checkpoint` 500 ms I/O flake — the one non-deterministic red seen in this feature
The vanilla framework's `Setup_Checkpoint` step (`SCR_AutotestCaseBase.c:168`, `timeoutMs: 500`) writes log files at case start and can exceed its budget on the N: NTFS mount → `TestResultTimeout: "Timed out after 500ms."` in junit.xml, killing the case before its Main stage ever runs. Seen once (2026-08-17, second Fast run) on `TickAdvancesDormantGroup` — provably unrelated to the case body (no movement code had executed yet in that session; the identical path passed on the runs before and after). If a case dies with this exact error at ~3 s with no assertion text, it is the checkpoint, not the code.

---

## Testing Approach

- **Logic tier (Fast):** all of `OVT_VirtualMovementMath` — projection, classification, index advance, stepping, wait, sort, external-move predicate.
- **Init tier (Fast):** `GetAllHandles` (Phase 2); tick-advances-dormant-group, DEFEND-never-moves, manager-resolves/no-leak (Phase 4).
- **Persistence tier:** NO new serializer test (nothing persists); one existing case amended (stationary fixture + moved-position claim).
- Every case: recorded can-fail proof, no `maxAttempts`. Suites run by orchestrator only, once per phase (Fast/Fast/All/Fast).

### Manual Testing Checklist (implementation.md §6, steps 1–10) — ALL OWED AFTER PHASE 3

**Setup.** On `Prefabs/GameMode/OVT_OverthrowGameMode.et`: core's `m_bDebugRegisterTestGroup = true` and
`m_iDebugTestGroupSpawnDistance = 300` (so a GM camera outside ~345 m cannot hold the group awake), plus
movement's `m_bDebugMovementLogging = true`. Epic kill switch stays **on**. For steps 1–6 set
`m_fVirtualSpeedMs = 10` to compress the window; reset to 1.5 before step 9. Core's debug group already
registers a two-point cycling patrol 150 m long — that is the entire fixture, no consumer needed.

- [ ] **1** Start a campaign, stay away from the debug group → repeated advance lines, no members anywhere (F1)
- [ ] **2** Watch a full leg → step distance matches `speed × elapsed`, a leg transition on arrival, a wrap back to point 0 (F1/F9)
- [ ] **3** Note the last logged position, walk/fly to it → members materialise **there**, on the ground (F2)
- [ ] **4** Stay inside the ring a minute → **no** advance lines for that handle; walk back out past the despawn ring → an `adopted`/`resumed` line, then advances **from where the AI stood** (F4)
- [ ] **5** A plan with a 60 s `WAIT` → the advance stops ~60 s at that point (the log keeps printing `wait` lines with the timer draining), then resumes (F5)
- [ ] **6** Point a debug leg across a bay → the written position holds at the shore while the log keeps advancing (`water` verdict), then reappears on the far side; approaching never materialises anyone in the sea (F6)
- [ ] **7** Save → quit → **Continue** → the group is where it was and advancing again within a couple of seconds; decode the save: **no** movement records, no `Overthrow.conf` entry (F7)
- [ ] **8** A second group placed deliberately **off** its route (a delivery simulation) → it starts walking toward its nearest plan leg with no other action (F8, the §3.8 seam)
- [ ] **9** `m_iDebugTestGroupCount = 40`, restart, watch one full round-robin period → all 40 handles logged within ~10 s, each step matching `speed × elapsed`, no hitch (F9/Q8/T4.5)
- [ ] **10** Start a **second campaign in the same session** without restarting the client → exactly **one** advance line per group per period, **not two** (the dead-world self-cancel; core found four teardown bugs exactly here) (Q6)
- [ ] **T4.5 scale check (pending user play-test)** — the 40-group measurement is a live play-test, not an
      automated claim, so Phase 4 documented the recipe instead of running it. **Recipe:** on
      `Prefabs/GameMode/OVT_OverthrowGameMode.et` set core's `m_bDebugRegisterTestGroup = true` and
      `m_iDebugTestGroupCount = 40`, and movement's `m_bDebugMovementLogging = true` (leave
      `m_fVirtualSpeedMs` at 1.5 and `m_iGroupsPerTick` at 8). Start a campaign, stay away from every
      group, and watch one full round-robin period. **Expect:** (a) *every* one of the 40 handles appears
      in the log within one period — 40 groups / 8 per tick × 2 s = **10 s** — so no group is starved by
      the round-robin; (b) each handle's per-advance step ≈ `speed × elapsed since THAT handle was last
      touched` ≈ 1.5 × 10 = **~15 m**, i.e. materially the same metres-per-second a registry of 8 groups
      gets (D5 — the whole point is that effective speed does not change with registry size); (c) no
      visible server hitch on the pass. **Record back into this file:** how many distinct handles were
      logged in one 10 s window, the min/max/typical step distance, and any hitch. → F9 / Q8
- [ ] **Workbench prefab verification (T3.9, user task):** open the game-mode prefab and confirm `OVT_VirtualMovementManagerComponent` `{6B4C3D6E00000010}` resolves with its four attributes at their defaults (1.5 / 2000 / 8 / false). Text-wired by an agent — core's T2.8 precedent.

---

## For `integration` — the §3.8 seam contract

**Addressed to `integration`'s planner: this section is the whole of what you have to read here.** It is
implementation.md §3.8 restated, so the next feature does not have to read this document or that plan to
know what movement promises. Nothing below is new in Phase 4 — it is the contract Phase 3 built.

> **A group registered into virtualization at its live delivery position is adopted by movement on the
> next tick pass, with no notification, no callback and no registration argument.** The first touch
> derives state by projecting the group's actual position onto its plan, so a delivered group starts
> walking its plan from wherever the vehicle left it. Extraction is the same thing in reverse: a
> consumer that `SetPosition`s a group, or unregisters it, needs no cooperation from movement.

Three consequences `integration` should plan against — **verbatim from implementation.md §3.8**:

1. **The plan is the opt-in.** Register a garrison with an empty or DEFEND-only plan and it will never be moved. Register a patrol with MOVE/PATROL points and it patrols virtually the moment it goes dormant. There is no flag to set (D10).
2. **A moved group resumes its waypoints from the top** (finding F-B): the group materialises where movement put it, then live AI walks to plan index 0 and onward, because no waypoint was ever completed while dormant. For a cycling patrol this costs up to one lap of walking; for a single-target delivery plan it costs nothing. Movement must not rewrite waypoints, and does not (D11).
3. **A vehicle-borne group in transit is not virtual at all.** It stays registered with a huge `spawnDistanceOverride` and drives live; movement skips it because `IsSpawned()` is true. Nothing about that is movement's code (D2).

Asserted, not just promised: Phase 4's Init cases assert 1 from both sides — a movable plan is walked
(`OVT_TEST_Init_VirtualMovement_TickAdvancesDormantGroup`) and a DEFEND-only one never is
(`OVT_TEST_Init_VirtualMovement_StationaryPlanIsNeverAdvanced`). 2 and 3 are documented limitations by
construction and have no automated claim.

Mechanically, 1 and the adoption path are one comparison: each touch compares `GetPosition(handle)` against the state's last written position in XZ, and a gap over 1 m drops the state so the next lines re-derive it (D9).

---

## Next Steps

### Immediate
1. Orchestrator: run **All** `{6A6E2A002F53A581}` (Phase 3's gate — T3.1/T3.2 touch the persistence tier)
2. User: Workbench prefab verification + the play-test list above
3. Phase 4 via `component-developer` (standard): T4.1–T4.6 (Init coverage, seam docs, scale check)

### Future (after this feature)
1. `integration` consumes the §3.8 seam contract above (auto-adoption; plan-is-the-opt-in; vehicle transit stays live)

---

## Open Questions

- (none)

---

## Session Notes

### 2026-08-17 07:07
- Feature started via /autorun-feature (Discord). Docs scaffolded from implementation.md (4 phases, 22 tasks).
- Working tree clean at start; concurrent-session risk R7 noted — re-grep file:line refs before each phase.
- Next: Phase 1 delegation.

### 2026-08-17 07:20 (Phase 1 complete)
- Phase 1 by `component-developer`: two new files, compile 0, Fast 186 green (baseline 179 + 7 new Logic cases).
- Deliberate deviation (documented in code): `NextTargetIndex` returns -1 for count <= 1 regardless of `cycle` — a cycling single-point plan would re-arrive forever; -1 lets the caller latch stationary (matches D8).
- Agent gotcha for later phases: `SetFailure` has no 4-arg format overload — concatenate the message into one string first.
- Next: Phase 2 (advanced) — `GetAllHandles()` on frozen core.

### 2026-08-17 07:45 (Phase 2 complete)
- Phase 2 by `component-developer-advanced`: one additive method on the frozen core (verified +23/-0 in `git diff`), api.md/core-context contract notes, Init case. Compile 0, Fast 187 green.
- Phase 3 hook from the agent: `GetAllHandles()` allocates a fresh array per call — the tick may sort it in place without copying.
- Next: Phase 3 (advanced) — T3.1 (stationary fixtures) FIRST, then state/manager/tick/write/wiring.

### 2026-08-17 08:20 (Phase 3 code complete — suites and play-test owed)
- Phase 3 by `component-developer-advanced`, in task order: **T3.1 first**, before a single line of tick code existed. Fixture types → DEFEND, whole-tree `RegisterGroup(` sweep with a per-site verdict (table under Gotchas #1 — one unsafe site in the tree, and it was the one F-A named).
- T3.2's moved-position claim is a **pair**: a pre-save guard that the group really moved, and a post-reload claim that it did **not** come back at its registration position. The second is implied by the first while the fixture really moves — its job is to make "the fixture really moves" self-enforcing, which is stated in the fail proof rather than dressed up as an independent claim.
- Two new production files (`OVT_VirtualMovementState`, `OVT_VirtualMovementManagerComponent`), three wiring edits (`OVT_Global`, game mode ×2, prefab). **Core untouched this phase** — `git diff Scripts/Game/GameMode/Virtualization/` is still exactly Phase 2's +23/-0.
- Compile **0** (and the gate was probed: a deliberate unknown-type in the new manager file produced exit 1 naming the line, so the file is genuinely covered).
- Deliberate deviations, both documented in code: (1) the external-move check runs **before** the derive rather than after, so a re-derivation happens once instead of twice — same outcome, one allocation; (2) a `step <= 0` early return, so the derivation pass (dt = 0) and a configured speed of 0 never manufacture an arrival out of `ARRIVAL_RADIUS_M` nor rewrite an unchanged position.
- Gotchas found and worth keeping: `int y = ;` is only a *warning* in EnforceScript (a useless can-fail probe — use an unknown type); the acceptance grep for waypoint/lifecycle identifiers matches **comments** too, so the F-B note had to be reworded to describe the engine behaviour without naming the class.
- Next: orchestrator runs **All**; user does the prefab check and the §6 play-test; then Phase 4.

### 2026-08-17 15:30 (FEATURE COMPLETE — user play-test passed)
- User on Discord: "yep all good now. all play-test items are green." §6 steps 1–10 + T4.5 scale check confirmed (scale numbers not individually recorded — the pass verdict is the user's).
- 22/22 tasks. Final suite state: Fast 190/190, All 236/236. Feature is Ready for Review; git is the user's (everything uncommitted on v1.5).

### 2026-08-17 ~15:10 (play-test findings — three fixes, gates re-closed Fast 190 / All 236)
- User play-test surfaced two defects, both fixed in core with dated context entries (per user instruction these are NOT in the bug tracker — in-dev features don't file bugs, IDs collide with main):
  1. **Waypoint entities buried/floating** — `CreatePlannedWaypoint` now surface-snaps the ENTITY (plan payload untouched); debug plan authoring snapped too.
  2. **Wrong resume direction** — on coincident-leg routes projection can't recover direction; core gained `GetCurrentPlanIndex(handle)` (additive #3) and `DeriveState` takes a `liveIndex` param, projection stays the fallback.
- Fixing those turned T4.1 red and exposed a THIRD, deeper defect: **registered groups materialise in the autotest world a few seconds after registration even under Manual policy** — vanilla's `m_bSpawnImmediately` init request + observer-only dispatch re-check (the autotest camera is an observer). Fixed with the modded-`SCR_AIGroup` Manual-spawn guard (`ArmOVTManualSpawn` latch, armed by `ForceSpawn`, cleared on despawn; `IsExpandComplete` true → queue drops). The movement Init cases now register with `spawnDistanceOverride = 0` — dormant by construction. **The earlier green Fast run had won this race by luck** (the case early-exits on >1 m before the spawn queue landed).
- Debugging arc for the record: full-suite red → isolated single-case red (ruled out suite interaction) → temp tick diagnostics (tick fired, handle seen, dropped at the IsSpawned gate) → vanilla source read found `m_bSpawnImmediately`/dispatch hole. All temp diagnostics removed; debug-logging default back to false.
- beast_ask worked for the option-A decision; the user deleted the two filed bugs and set the no-bugs-for-in-dev rule mid-arc.

### 2026-08-17 09:40 (Phase 4 gate closed — feature code complete)
- Fast run 1: T4.1 red — the Init world never installs the tick (Gotcha 6). Fixed by the orchestrator: the three movement cases install it themselves via the idempotent `PostGameStart()`.
- Fast run 2: `TestResultTimeout 500ms` in the vanilla `Setup_Checkpoint` step — I/O flake, unrelated (Gotcha 7). beast_ask transport was down twice; proceeded on the recommended option per the away protocol.
- Fast run 3: **190/190 green.** All four phase gates now closed: Fast 186 → Fast 187 → All 233 → Fast 190.
- Remaining: user Workbench prefab check, §6 play-test incl. T4.5 scale numbers.

### 2026-08-17 09:10 (Phase 4 complete — Fast run and the play-test owed)

- Phase 4 by `component-developer`: three Init cases in `OVT_TEST_InitSuite.c` (inserted after the
  `GetAllHandles` seam case) plus a small shared `OVT_TEST_VirtualMovementFixture`, and exactly one
  production change in the whole phase — `OVT_VirtualMovementManagerComponent.GetTrackedCount()`, a
  read-only diagnostic with a doc comment saying so. `git diff Scripts/Game/GameMode/Virtualization/`
  is still only Phase 2's one method; no `maxAttempts` anywhere; every case carries a recorded fail proof.
- **The observation windows are wall-clock, not frame counts** (`world.GetWorldTime()` against a deadline
  captured at registration): 10 s, which is ~5 passes at the 2000 ms cadence. Sized that way because
  **the first pass over a handle cannot move it** — state is derived and stamped with `now`, so its dt,
  and therefore its step, is 0 by construction. At the default 1.5 m/s that leaves ~4 advances (~12 m)
  against a 200 m leg, comfortably inside the "> 1 m and < the whole leg" claim.
- **The leg is picked, not assumed.** The fixture tries eight compass directions and samples each
  candidate leg at five points with the same `IsOceanAtPosition` predicate the manager uses. A leg
  pointed into a bay would produce zero writes (the water rule) and read exactly like a dead tick — a
  false red with a misleading diagnostic. Both the walking case and the DEFEND case use the same picked
  leg, so the pair is a controlled experiment in which only the waypoint TYPE differs.
- **The no-leak case settles first.** It waits (bounded) for `GetTrackedCount()` to reach 0 *before*
  registering anything, because the map is emptied by the tick rather than by `UnregisterGroup`, and
  Gotcha #5's count-guarded purge means "0" is a state to wait for rather than to assume. That wait IS
  the Q7 assertion; the DEFEND registration that follows is the Gotcha #4 claim (a plan-stationary group
  holds no entry, so it contributes 0 while registered).
- T4.5 was **not** run: the 40-group scale measurement needs a live campaign. The exact recipe and the
  numbers to record are in the Manual Testing Checklist above, flagged pending.

---

*Update this file at the end of each work session.*
