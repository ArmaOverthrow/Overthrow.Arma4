# Starter Jobs Retirement - Context & Decisions

**Last Updated:** 2026-08-09
**Current Phase:** **All 7 phases complete (0-6).** Build finished 2026-08-09; play-test passed and signed off the same day.
**Status:** 🟢 **COMPLETE — 52/52 tasks** · compile-check 0, Fast **54**, All **92** · ✅ **play-test passed 2026-08-09, all eleven checks (U1-U6, M1-M5)**.

**U1 passed on a real campaign save** — the observation the whole phase order existed to make possible. A campaign taken from before this build Continues correctly, with the right jobs on the board and a named WARNING per dropped starter job. The save-format migration is therefore verified where it actually matters: on a real player's campaign, at a scale the seeded fixture could not reach.

**M1-M5 passed, and that is worth more than this feature.** The epic recorded that **two-client per-player isolation had never been observed passing**. It has now been observed on the **jobs** surface: M3 (A completes a job, A gets the hint and the reward, **B does not**) is a direct observation of BUG-040's owner-filtered send working with two real clients, and M4 is the job board surviving JIP through `RplSave`/`RplLoad` — the one replication path this feature deliberately did not touch, which is precisely why watching it was worth doing. ⚠️ **Do not over-read it:** this exercises the *job completion* delivery path, **not** `tutorial-system`'s popup delivery path. That framework's own F7 remains unobserved. Strong circumstantial support; not a substitute.

**Epic:** `new-player-experience` (feature **#5 of 5 — the last**). Depends on #3 `tutorial-content` (complete 2026-08-09, the coverage precondition), consumes handoffs from #2 `field-manual` (the wiki) and #4 `first-spawn` (the D9 string-retention precedent and the `welcome-intro-3-ui.edds` liability).

---

## Quick Status

**What's Done:**
- ✅ Plan written (`implementation.md`, phases 0-6) — the stable-id migration, the frozen legacy table, the Eden reconciliation, the deletion and the record-straightening
- ✅ Docs scaffolded (`tasks.md`, this file); `implementation.md` status flipped to In Progress

- ✅ **Phase 0 complete (2026-08-09)** — §3.8 re-verified row by row (**12 stand, 1 struck**), baseline re-measured (**52 / 89**, not the plan's 51 / 88), **F3 settled by direct measurement: the Eden override MERGES, Eden runs all 12 jobs, no bug filed**, and the **v1 save fixture is captured and decoded non-empty**. Zero product diff.

- ✅ **Phase 1 complete (2026-08-09)** — `m_sId` on `OVT_JobConfig` (`Scripts/Game/Configuration/OVT_JobConfig.c:11-20`), authored into **all twelve** `Configs/Jobs/*.conf`, `GetJobConfigCount` / `FindJobIndexById` / `GetJobIdByIndex` on the manager (`OVT_JobManagerComponent.c:401-454` — `GetJobConfigCount` `:404`, `FindJobIndexById` `:422`, `GetJobIdByIndex` `:445`), and **T2** landed as a new Init case (`OVT_TEST_InitSuite.c:3127-3346`), green and **proven red twice**. Measured green gate **Fast 53 / All 90** — exactly baseline **+1**. Zero runtime behaviour change: nothing outside T2 reads `m_sId`.

- ✅ **Phase 2 complete (2026-08-09)** — the serializer migration. Version 2 writes stable ids for the board and **both** counter maps; the version 1 branch reads the **frozen** record classes and converts through `LEGACY_V1_JOB_IDS`; the id→position resolve lives inside `FindRestorableJobConfig()`. Measured green gate **Fast 53 / All 90** — unchanged, as planned. **Zero wire-surface change, proven by diff.** ⚠️ **The plan's D4 class NAMES would have silently wiped the job board on every version 1 save** — see §"The `$type` discovery" below; the freeze landed on the classes the payload actually names.

- ✅ **Phase 3 complete (2026-08-09)** — prove it. **T1** (`OVT_TEST_Logic_Jobs.c:319-541`, world-free, pins the frozen v1 table against its own literals) and **T3** (`OVT_TEST_PersistenceRoundTripSuite.c:3163-3794`, board + both counter maps, save → dirty → re-apply → re-apply again) are green and **each proven red** — T3 twice, by two independent breaks. The **v1 fixture check ran, and it ran through the REAL `SaveGameManager` continue path**, producing exactly the three named retired drops and exactly the two survivors on their correct configs that Phase 2's byte-level decode predicted; the re-run was line-for-line identical. Measured green gate **Fast 54 / All 92** — baseline +2 / +3. **Nothing deleted.** ✅ **DECISION GATE 3.8: Phase 4 is CLEARED.**

- ✅ **Phase 4 complete (2026-08-09)** — **the deletion happened.** Ten files removed (`Configs/Jobs/{findGunDealer,findShop,placeEquipmentBox,recruitACivilian,placeACamp}.conf` + `.conf.meta`), five entries out of `Prefabs/GameMode/OVT_OverthrowGameMode.et` (`m_aJobConfigs` now holds **7**), the stale `m_aJobConfigs` block gone from `Worlds/MP/OVT_Campaign_Eden_Layers/managers.layer`, and `RETIRED_IDS_ARE_DELETED` flipped to **true** (`OVT_TEST_InitSuite.c:3170`). **Zero dangling references outside `docs/`.** Eden loads with **7** configs and no missing-resource error. Measured green gate **Fast 54 / All 92**, compile-check exit 0 — no count moved, exactly as planned.

- ✅ **Phase 5 complete (2026-08-09)** — records, bugs, epic close. The ten starter-job `.st` items are **retired in place** with retirement records on their `Comment` fields (zero `Text` changed, zero items deleted, no `.<lang>.conf` opened); the two forward-pointing `.st` Comments are corrected; BUG-037 and BUG-040 each carry a dated note saying the jobs were **removed** while the bugs stay `closed` and **were not closed by this feature**; both `new-player-experience` epic docs are de-staled at every site that asserted the false closure; and the jobs epic records the discharged positional-`jobIndex` debt plus the six orphaned-but-kept classes. Gates unmoved: compile-check exit 0, Fast **54**.

**What's Next:**
- 🔄 **Phase 6** (`help-docs-sync`) — the wiki `getting-started` edit and the in-game-help confirmation. `Configs/Tutorials/` and `Configs/FieldManual/` are expected to need **no** edit; that is a legitimate result to record, not a skipped task.

**Blockers:**
- None. R1 (the Eden merge-vs-replace unknown) is **retired** — measured, answered, and it did not change the code.
- ~~Phase 3.4's fixture recipe will not work~~ **RESOLVED in Phase 3.** The plan's recipe had *three* mismatches, not the two Phase 2 found, and `activate_save.sh` turned out to be structurally unable to reach a local-mode server's save tree at all. The replacement stages the fixture's save point directly and is **better** than what the plan asked for — it goes through the real continue path. Recipe, evidence and cleanup step: §"The v1 fixture check" below. Neither `tools/launch-server.sh` nor `.scripts/activate_save.sh` was modified.

**Carried forward into later phases:**
- The struck §3.8 rewards row: the real figure is **$100, 30 XP** and two field dressings, not 10 XP. Do not let 10 XP reach a bug note or the wiki (Phase 5/6).
- `tools/README.md`'s "backup_save.sh still speaks only EPF" sentence is stale (see the fixture section). Not this feature's file to fix.

---

## The one thing to understand before touching anything

**This feature is not about deleting five files. It is about the int that names them.**

`OVT_Job.jobIndex` is a **position** in `m_aJobConfigs` and it is **persisted**. The five doomed configs sit at positions **2, 4, 5, 6, 7 of twelve**, so a naive delete shifts every later job down. `FindRestorableJobConfig()` only rejects *out-of-range* records — so five saved records would come back attached to a **different** job, at a stage index that means something else, and would pay out that other job's reward ($750 instead of $0 in the worst case). The lifetime counter maps are keyed by the same int and would cap the wrong jobs. **No error, no log line, no crash.**

Hence the non-negotiable phase order: **give every job a stable id → move the save format onto it → prove it → only then delete.** Phase 4 sits behind the decision gate at 3.8.

---

## Key Files

- `Scripts/Game/Configuration/OVT_JobConfig.c` — gains `m_sId` (Phase 1)
- `Scripts/Game/GameMode/Managers/OVT_JobManagerComponent.c` — `FindJobIndexById` / `GetJobIdByIndex`; `ApplyPersistedJobs()` and `FindRestorableJobConfig()` resolve by id. **Keeps its clear-and-rebuild shape — `ReapplyLatestSaveData` idempotency is load-bearing, not stylistic**
- `Scripts/Game/Persistence/Serializers/Components/OVT_JobManagerSerializer.c` — the heart of the feature: version 2, the frozen v1 record classes (`OVT_PersistedJob` `:34`, `OVT_PersistedPlayerJobCounts` `:52` — **the frozen ones are the un-suffixed names**, see §"The `$type` discovery"), the current records (`OVT_PersistedJobV2` `:80`, `OVT_PersistedPlayerJobCountsV2` `:102`) and the frozen legacy table (`:194`). Its file header carries the **no-replay restore invariant** that survives verbatim, plus the version history and the v1 removal trigger (`:159-170`)
- `Configs/Jobs/*.conf` — twelve gain `m_sId` in Phase 1; five are deleted in Phase 4
- `Prefabs/GameMode/OVT_OverthrowGameMode.et` — five entries leave `m_aJobConfigs`. ⚠️ **One of the two busiest shared files in the repo** (R7)
- `Worlds/MP/OVT_Campaign_Eden_Layers/managers.layer` — the stale five-entry `m_aJobConfigs` block, deleted outright (D7)
- `Language/localization_Overthrow.st` — **the only file under `Language/` this feature edits**, and only its `Comment` fields (D9). Never the `.<lang>.conf` runtime exports
- `docs/features/new-player-experience/tutorial-content/context.md` §"Starter-job coverage mapping: AS BUILT" — the **authoritative** coverage mapping. Read that, **not** `tutorial-content/implementation.md` §3.5, which has a struck false row

---

## Key Decisions

*The full set with rationale is `implementation.md` §5 (D1-D13). Recorded here are the ones a future reader will trip over.*

- **D1 — a new `m_sId`, not `m_sTitle`, not the resource name, not the GUID.** A title is a localization key and renaming it is a legitimate content edit that must never break a save. A resource name couples the save to a file path. A GUID is stable but `"Dropping saved job '{5D9C33D122545AFD}'"` tells a bug reporter nothing.
- **D2 — persisted = stable string id; in-session and on the wire = positional int.** There are exactly **two** translation points, both server-side, both on the save/load path. **Zero RPC signatures change and `RplSave`/`RplLoad` is untouched** — and that is a Phase 2 *acceptance criterion proven with a diff*, not an assurance. BUG-090 (`Rpc()` arity compiles clean and dies at the wire) is why the design gives that hazard no surface at all.
- **D4 — v1 is read through frozen shadow classes, not through the mutated new ones.** `OVT_RecruitManagerSerializer` appended a trailing scalar and cleared it on v1, which works for one appended field. This migration *replaces a field's meaning inside an array of records* — if per-record reads are flat-positional, an extra field consumes the next record's first field and desyncs everything silently. ~15 lines of insurance on the one code path where a mistake is invisible.
  - ⚠️ **CORRECTED IN PHASE 2 BY MEASUREMENT, 2026-08-09 — the decision holds, its NAMES did not.** The container writes the concrete class name as a `$type` discriminator and instantiates from it on load, so the shadow class had to keep the name the v1 payload carries. The plan's `OVT_PersistedJobV1` / `OVT_PersistedPlayerJobCountsV1` would have failed the read and handed `ApplyPersistedJobs()` an empty board — the exact silent wipe D4 exists to prevent. As shipped: `OVT_PersistedJob` / `OVT_PersistedPlayerJobCounts` are the **frozen v1** records, `OVT_PersistedJobV2` / `OVT_PersistedPlayerJobCountsV2` the current ones. Full evidence in §"The `$type` discovery".
- **D5 — v1 support stays until the next save-format-breaking change, with the trigger written into the serializer header.** Not "forever by inertia", not "one release". Dropping it early wipes the job board and every lifetime counter on a live campaign *silently*, because the `version < 1` guard would treat it as a normal load.
- **D7 — the Eden layer's `m_aJobConfigs` block is deleted before the merge-vs-replace question is settled.** The block is same-GUID with empty bodies, so deleting it is correct or neutral under every candidate semantic. **This is the plan's one deliberate decoupling of a blocker from the work it blocks** — the measurement changes the *bug list*, not the *code*.
- **D9 — the ten stringtable items are retired in place, not deleted.** Exactly `first-spawn` D11: six languages of translation, `.st` deletions churn exports the user regenerates by hand, and an unreferenced item costs nothing at runtime. The `Comment` becomes the retirement record.
- **D11 — this feature does NOT close BUG-037 or BUG-040.** They were **fixed in place on 2026-08-03** and are already `closed`. The requirements, `epic-overview.md` and `epic-requirements.md` all still say otherwise and are corrected in 5.4. What survives as justification is redundancy against ten shipped tutorial entries, plus the epic's binding constraint: **a job assigns a directed goal and puts a marker on a named instance, which is exactly the shape this epic replaced.**
- **D13 — no new tutorial entry for either residual gap.** *Directed discovery* is largely moot — `OVT_MapIcons` draws every shop and every gun dealer with **no** discovery gate. *Recruit availability* (`recruits-first-recruit` fires on the first recruit **gained**, not on the option appearing) is accepted as a known gap.

---

## Gotchas / Traps

- **⚠️ The stated premise of this feature was stale on arrival.** BUG-037 and BUG-040 were fixed in place on 2026-08-03 and are closed. Retirement discharges **neither**. Anything in the epic docs that says otherwise is wrong and is corrected in Phase 5.
- **⚠️ A documented trap is not evidence (R6).** `tutorial-content` shipped a **false row** in its own trap table into three documents — the struck claim that gun dealers lack a dedicated map icon. They have one: `OVT_MapIcons.TryCreateGunDealerIcon:111-134`, `"gundealer"` sprite, enumerated at `:626-638`. §3.8 is re-verified in 0.1 **including the rows this plan wrote**.
- **⚠️ The v1 fixture has a deadline.** After Phase 2 the tree can no longer *write* a v1 payload. Capture is task **0.5**, its decode-and-verify is **0.6**, and the recreate recipe is **0.7** — because the archive stays out of git.
- **`LoadContext.Read(out void value)` derives the property name from the local variable's name — and it is VALIDATED, not advisory.** ⚠️ **Measured 2026-08-09: writing `jobRecords` and reading the identical payload into a local called `readJobs` returns false and reads nothing.** The plan called this "belt-and-braces"; on this build it is load-bearing. The v1 branch's locals are `jobRecords`, `countIndices`, `countValues`, `playerCounts`, verified character-for-character against the fixture blob's own property names.
- **⚠️ A PERSISTED RECORD CLASS CAN NEVER BE RENAMED.** The binary container writes the concrete class name into the payload (`$type`) and creates the instance from it on load; a renamed class fails the read outright and poisons the rest of the stream. This is repo-wide, not jobs-specific — it binds every `OVT_Persisted*` class in `Scripts/Game/Persistence/Serializers/`. See §"The `$type` discovery".
- **Binary contexts are positional:** write order must equal read order, and the version value comes first.
- **The `version < 1` early return is load-bearing**, not a formality: without it an absent payload wipes the board and every counter. It stays byte-unchanged (Q5).
- **After removal, no shipped config is player-allocated.** All seven survivors are `m_bPublic` or `m_bBaseOnly`. `m_mPlayerJobCounts` will be empty in practice and `RpcDo_UpdateJob`'s owner-identity branch goes unexercised — so **T3 must build its per-player records synthetically**.
- **`OVT_TEST_PersistenceRoundTripSuite` has a non-negotiable assertion rule:** no persistence-framework, vanilla-persistence or Overthrow save-data type name may appear in that tree except the two annotated triggers already in its gate class. Every assertion reads back through the same public manager API that wrote it.
- **R7 — this tree hosts parallel sessions.** The two files this feature edits most (`OVT_OverthrowGameMode.et`, `localization_Overthrow.st`) are the two busiest in the repo, and the `BUG-` numbering space is shared. Re-check `git status`, `git log` and the highest BUG id **at the start of every phase** rather than trusting the plan's **BUG-132** snapshot.
- **`m_iMaxTimes` is no longer a global-only gate.** `OVT_JobManagerComponent.c:543` reads `bool playerAllocated = !config.m_bBaseOnly && !config.m_bPublic;` and skips the global cap for player-allocated configs — the BUG-037 fix. Do not reason from the pre-fix shape.

---

## Measured baselines

*Filled in by task 0.2. The plan's figures for this tree (2026-08-09) were **Fast 51 / All 88 / compile-check 0**; every other count in the repo (38/66, 47/78, 50/86) is stale. Re-derive rather than inherit.*

| When | compile-check | Fast `{6A6E29FF47ECB840}` | All `{6A6E2A002F53A581}` |
|---|---|---|---|
| Baseline (0.2), measured 2026-08-09 on `f6681d00` | **exit 0** — `compile-check: OK (5945 files, Game module, 6s)` | **exit 0, 52 tests** — `run-tests: OK (52 tests, 34s)` | **exit 0, 89 tests** — `run-tests: OK (89 tests, 39s)` |
| Phase 1 front, re-measured 2026-08-09 (R7) | **exit 0** — `compile-check: OK (5945 files, Game module, 4s)` | **exit 0, 52 tests** | **exit 0, 89 tests** |
| Phase 1 green gate (1.6), measured 2026-08-09 | **exit 0** — `compile-check: OK (5945 files, Game module, 4s)` | **exit 0, 53 tests** — `run-tests: OK (53 tests, 35s)` | **exit 0, 90 tests** — `run-tests: OK (90 tests, 39s)` |
| Phase 2 front, re-measured 2026-08-09 (R7) | **exit 0** — `compile-check: OK (5945 files, Game module, 5s)` | **exit 0, 53 tests** — `run-tests: OK (53 tests, 33s)` | **exit 0, 90 tests** — `run-tests: OK (90 tests, 38s)` |
| Phase 2 green gate, measured 2026-08-09 | **exit 0** — `compile-check: OK (5945 files, Game module, 4s)` | **exit 0, 53 tests** — `run-tests: OK (53 tests, 34s)` | **exit 0, 90 tests** — `run-tests: OK (90 tests, 39s)` |
| Phase 3 front, re-measured 2026-08-09 (R7) | **exit 0** — `compile-check: OK (5945 files, Game module, 5s)` | **exit 0, 53 tests** — `run-tests: OK (53 tests, 33s)` | **exit 0, 90 tests** — `run-tests: OK (90 tests, 40s)` |
| **Phase 3 green gate (3.6), measured 2026-08-09** | **exit 0** — `compile-check: OK (5945 files, Game module, 4s)` | **exit 0, 54 tests** — `run-tests: OK (54 tests, 33s)` | **exit 0, 92 tests** — `run-tests: OK (92 tests, 39s)` |
| Phase 4 front, re-measured 2026-08-09 (R7) | **exit 0** — `compile-check: OK (5945 files, Game module, 5s)` | **exit 0, 54 tests** — `run-tests: OK (54 tests, 34s)` | *(contaminated — the run overlapped the 4.3 deletion and went `FAILED (1 of 92)` on the T2 tripwire; re-taken clean at the gate below)* |
| **Phase 4 green gate (4.8), measured 2026-08-09 on the FINAL tree** | **exit 0** — `compile-check: OK (5945 files, Game module, 4s)` | **exit 0, 54 tests** — `run-tests: OK (54 tests, 34s)` | **exit 0, 92 tests** — `run-tests: OK (92 tests, 40s)` |
| **Phase 5 gate, measured 2026-08-09** | **exit 0** — `compile-check: OK (5945 files, Game module, 5s)` | **exit 0, 54 tests** — `run-tests: OK (54 tests, 35s)` | *(not re-run — Phase 5 touched no `.c` file; per `tasks.md`, doc/string-only phases gate on Fast)* |

**FINAL COUNTS FOR THIS FEATURE: compile-check exit 0 · Fast `{6A6E29FF47ECB840}` = 54 · All `{6A6E2A002F53A581}` = 92.** Against task 0.2's re-measured baseline of **52 / 89**, the honest feature delta is **+2 Fast / +3 All** — T1 (Logic) and T2 (Init) sit in tiers both groups share, T3 (PersistenceRoundTrip) in the All group only. **The plan's "+3 to both" was arithmetically wrong twice over:** it was computed against a 51 / 88 baseline that a parallel session had already made stale, and it assumed all three new cases sat in shared tiers.

**Phase 3 delta: +1 Fast / +2 All, exactly as planned.** T1 landed as a **new case** in the Logic tier, which both groups share, so it raised both by one; T3 landed as a **new case** in the PersistenceRoundTrip tier, which only the All group carries, so it raised All alone. Neither landed as a branch on an existing case — the arithmetic that bit `first-spawn` did not apply here either, and the shape was again chosen deliberately so a red run names `Logic_Jobs_LegacyIndexMapping` or `PersistenceRoundTrip_JobBoard`, not some unrelated case they were bolted onto. **Cumulative feature delta against the 0.2 baseline of 52 / 89: +2 / +3.**

**Phase 1 delta: +1 / +1, exactly as planned.** T2 landed as a **new case**, not as a branch on an existing one, so both counts rose by one (the groups share `OVT_TEST_InitSuite`). The arithmetic that bit `first-spawn` — a case that lands as a *branch* adds assertions but no case count — did not apply here, and the shape was chosen deliberately: the guard needs its own name in the report so a red run says "job stable ids", not "tutorial entries".

⚠️ **The plan's 51 / 88 is already stale — this tree measures 52 / 89.** One case was added to each group by a parallel session between planning and Phase 0 (both groups share `OVT_TEST_InitSuite`, so a single added Init case raises both counts by one — consistent with `f6681d00 "(add) option to reset tutorial"` and BUG-133). **Every later phase's expected count is derived from 52 / 89, not from the plan's figures.** The plan's *deltas* still hold: Phase 1 → Fast 53 / All 90; Phase 3 → Fast 55 / All 92. Re-derive anyway at each phase front (R7).

**Preconditions re-checked at the same time (0.2):**

| Check | Result |
|---|---|
| `git status` | Clean except this feature's own three doc files |
| `git log --oneline -1` | `f6681d00 (add) option to reset tutorial` |
| Highest `BUG-` id in `docs/bugs/` | **BUG-133** — *not* the plan's BUG-132 snapshot. `BUG-133.md` is `new-player-experience/tutorial-system`, open, filed 2026-08-09. **The next free id is BUG-134** (re-check again at each phase front) |

---

## The frozen legacy v1 job table

*Authoritative copy per Q6, **confirmed against source in task 0.8** and mirrored in `LEGACY_V1_JOB_IDS` from Phase 2. It is **history, not configuration**: it records what the twelve-entry v1 job list was, and it is never edited again.*

**Confirmed 2026-08-09, no correction needed.** The order below matches `Prefabs/GameMode/OVT_OverthrowGameMode.et` **lines 26, 28, 30, 32, 34, 36, 38, 40, 42, 44, 46, 48** exactly (the `m_aJobConfigs` block spans `:25-49`; the plan's ":25-47" is one line short of the closing brace). Independently corroborated at runtime by the F3 probe above, which printed the same twelve `m_sTitle` values in the same order on Eden.

**This table is correct for Eden too.** F3 measured merge, so an Eden v1 save wrote indices 0-11 against this identical list. It would also have been correct under replace, because the layer's five entries are a same-order prefix — which is why §3.5 could commit to one table before the measurement was taken.

| v1 index | id | fate |
|---:|---|---|
| 0 | `assassinate-traitor` | survives |
| 1 | `base-recon` | survives |
| 2 | `find-gun-dealer` | **retired** |
| 3 | `raise-support` | survives |
| 4 | `find-shop` | **retired** |
| 5 | `place-equipment-box` | **retired** |
| 6 | `recruit-a-civilian` | **retired** |
| 7 | `place-a-camp` | **retired** |
| 8 | `propaganda-run` | survives |
| 9 | `pirate-radio` | survives |
| 10 | `sabotage-radio-tower` | survives |
| 11 | `assassinate-officer` | survives |

**Mirrored in code at `OVT_JobManagerSerializer.c:194-207` (`LEGACY_V1_JOB_IDS`), with the retired five split out at `:218-224` (`RETIRED_V1_JOB_IDS`) and the two pure lookups at `:240` / `:254`. Q6 satisfied: twelve entries, v1 order, header forbidding edits, matching copy here.**

---

## The `$type` discovery — measured in Phase 2, and it changed the design

*This is the most important thing Phase 2 found, and it is a repo-wide fact, not a jobs fact.*

**The persistence binary container writes the CONCRETE CLASS NAME into the payload and instantiates from it on load. A persisted record class can therefore never be renamed.**

- `SCR_PersistenceBinarySaveContext` calls `EnableTypeDiscriminator(true)` (base game, `Scripts/Game/Plugins/Persistence/System/SCR_PersistenceSerializationContext.c:34-43`), and `SerializationContext.ConfigureTypeDiscriminator`'s own doc says *"Type is written inside the object when saving. When the load happens, it creates the instance based on this type."* (`Scripts/Game/generated/Plugins/Serialization/SerializationContext.c:20-24`).
- It is visible verbatim in the captured v1 fixture blob: `$type` at `0x28c`, value `OVT_PersistedJob` at `0x292`, then `jobIndex`, `location`, `townId`, `baseId`, `stage`, `owner`, `accepted`, `declined` — the field names are written too, one set per record.

**Measured with an in-memory round trip** through `SCR_PersistenceBinarySaveContext` / `SCR_PersistenceBinaryLoadContext` in a throwaway autotest (created, run, deleted; `git status` clean afterwards). Class `B` was a byte-for-byte copy of `A` with a different name; the reads were ordered **B, C, A** so that A succeeding *last* rules out "a container can only be read once":

```
[PROBE] A->B (frozen shadow class, identical layout, different name):
        loaded=true version(true)=1 records(false)=0 first{(none)} countIndices(false)=[] countValues(false)=[]
[PROBE] A->C (mutated field 0, different name):
        loaded=true version(true)=1 records(false)=0 first{(none)} countIndices(false)=[] countValues(false)=[]
[PROBE] A->A (control, LAST):
        loaded=true version(true)=1 records(true)=3 first{jobIndex=0 townId=0 stage=0 owner=PROBEOWNERA accepted=true declined=1}
        countIndices(true)=[2,4,0,11] countValues(true)=[1,1,3,2]
```

**What this means for D4, stated plainly: the plan's chosen names would have destroyed live campaigns.** A frozen shadow class called `OVT_PersistedJobV1` cannot read a version 1 payload — the read returns false, the array comes back **empty but non-null**, and every following property in the stream fails too. `ApplyPersistedJobs()` would then have been handed an empty board and empty counter maps and would have applied them: **job board wiped, every lifetime counter wiped, no error, no crash.** Exactly the failure this feature exists to prevent, introduced by the mitigation for it.

**The fix keeps D4's intent exactly and only reassigns the names:** the freeze lands on the classes the payload actually names.

| Role | Class | Where |
|---|---|---|
| Version 1, **FROZEN** | `OVT_PersistedJob` | `OVT_JobManagerSerializer.c:34` |
| Version 1, **FROZEN** | `OVT_PersistedPlayerJobCounts` | `:52` |
| Version 2, current | `OVT_PersistedJobV2` | `:80` |
| Version 2, current | `OVT_PersistedPlayerJobCountsV2` | `:102` |

**Two further format facts measured in the same pass, both load-bearing:**

1. **The local variable name IS the property name, and it is validated.** `LoadContext.Read(out void value)` derives the key from the variable handed to it; writing `jobRecords` and reading the identical payload into a local called `readJobs` **returns false and reads nothing**. §3.4 called this "belt-and-braces" — on this build it is not. The version 1 branch's locals are `jobRecords`, `countIndices`, `countValues`, `playerCounts` (`OVT_JobManagerSerializer.c:445-461`), verified character-for-character against the fixture blob's own property names at `0x26b`, `0x277`, `0x4f7`, `0x519`, `0x53a`.
2. **An empty collection and a failed read are distinguishable.** An empty board writes and reads back as `Read() == true` with zero records; `false` only ever came back from a real failure. That is what makes the new read guard safe: every `context.Read()` in both version branches is checked, and a failure **aborts without touching the board** (`AbortUnreadablePayload`, `:603`) instead of replacing a live campaign's jobs with nothing. This is an addition beyond the plan's task list, justified by the measurement above.

**Static verification of the version 1 read path against the real fixture.** The fixture's job payload was decoded field by field and matches the frozen classes exactly: property names `version`(=1), `jobRecords`, `countIndices`, `countValues`, `playerCounts`; `$type OVT_PersistedJob` with fields in declaration order; `$type OVT_PersistedPlayerJobCounts` with `playerId` / `jobIndices` / `counts`. Its content is board indices `0, 3, 2, 4`, global counters `[4,0,2,11] -> [1,3,1,2]`, and one per-player record `OVTFIXTUREOWNERB` with `[4,2,5] -> [1,1,1]`. Under this migration that yields **exactly three** retired-job WARNINGs (`find-gun-dealer`, `find-shop`, `place-equipment-box`), the survivors `assassinate-traitor` and `raise-support` restored, and global counters `assassinate-traitor = 3`, `assassinate-officer = 2`. **Phase 3.4 should expect precisely that.**

---

## The v1 fixture check — the plan's recipe was broken; this is the corrected one

**Settled and RUN TWICE in Phase 3 (2026-08-09).** Neither `tools/launch-server.sh` nor `.scripts/activate_save.sh` was modified — both are dev-ops-owned contract scripts, and neither needed changing.

### Why the plan's recipe cannot work — THREE mismatches, not the two Phase 2 found

The plan's `.scripts/activate_save.sh jobs-v1-premigration --profile OverthrowCI` followed by `tools/launch-server.sh` addresses two different saves:

1. **Different profiles.** `tools/launch-server.sh` defaults to profile `OverthrowDS` (`tools/launch-server.sh:85-87` documents it as *"NOT the OverthrowCI profile the test harness uses"*; the default comes from `OVERTHROW_SERVER_PROFILE`, default `OverthrowDS`, at `:140`).
2. **Different campaign directory names, which a matching `--profile` does NOT fix.** The directory is named after *the thing that was loaded*. The autotest harness loads the **world** `{D87EF7EED4210569}Worlds/MP/OVT_Campaign_Test.ent` (`docs/features/dev-ops/autotest-foundation/implementation.md:149`) and saves under `game/OVT-Campaign-Test/`; the server loads the **mission** `{6B0E7A50D1E2F3A4}Missions/25_OVT_TestWorld.conf` (`tools/launch-server.sh:52`) and saves under `game/6B0E7A50D1E2F3A4-25-OVT-TestWorld/`.
3. **(FOUND IN PHASE 3, and it is the one that decides the shape of the fix) different save ROOTS.** In `--mode local` the server authenticates nobody, so its save path has **no `app<appid>_user<steamid>` component at all** — it writes straight to `profile/.save/game/…`. `activate_save.sh` only ever resolves `<root>/profile/.save/app*_user*/game` (`.scripts/activate_save.sh:38-67`, which fails with *"No app_user dir under …"* at `:58` when there is none), so it cannot even name the directory a local-mode server uses. Measured: the `OverthrowDS` profile carries **both** trees — `profile/.save/game/6B0E7A50D1E2F3A4-25-OVT-TestWorld/playthrough000/` with savepoints 000-006 (newest 2026-08-09 15:33, written by `--mode local` runs) and `profile/.save/app1874880_user76561198000167250/game/…` with one savepoint (2026-08-06 22:05, written by a `--mode dedicated` run).

`activate_save.sh` is therefore the wrong tool for this fixture, and no combination of its flags makes it the right one.

### What replaces it — and it is STRONGER than what the plan asked for

Stage the fixture's save point directly into the server's own save tree, then run the server normally. This is not a workaround dressed up: **it exercises the real player-facing continue path**, which is the one thing the plan's §7 says automation structurally cannot reach.

A dedicated server auto-continues. `OVT_OverthrowGameMode.DecideDedicatedStart()` (`:465-487`) waits for the async save scan, and on `HasSaveGame()` calls `m_Persistence.LoadLatestSave()` (`:477-481`) — which hands the save point to `SaveGameManager`, transitions the world, and runs the deserializers on the other side. `--mode local` reaches that path: it is `RplMode.Dedicated` as far as `OVT_OverthrowGameMode.c:1418` is concerned, proven by the F3 Eden run's own log line *"[Overthrow] Dedicated server: no existing campaign - starting a new game"*.

**Two facts that make the staging honest rather than a fudge:**

- **The payload under test is byte-identical.** The fixture's `WorldState/00e63bef-8b1d-8d00-8800-0f0a07a12329.blob` is copied verbatim (`md5 e0ab789b1652ba0bd9ef86917a28bf2f`, verified equal on both sides). Only `meta-info.json` — the save point's index card — is rewritten.
- **The blob belongs to this mission already.** `Missions/25_OVT_TestWorld.conf:2` declares `World "{D87EF7EED4210569}Worlds/MP/OVT_Campaign_Test.ent"` — the same world the fixture was captured in — and the DS savepoints carry the **same** WorldState blob UUID as the fixture. The mission header is a different wrapper around one world, which is exactly why only the index card differs.

### The recipe (run from the repo root)

```bash
FX=.saves/jobs-v1-premigration_20260809_205446.tar.gz
DS="/mnt/c/Users/Aaron Static/OneDrive/Documents/My Games/OverthrowDS/profile/.save/game/6B0E7A50D1E2F3A4-25-OVT-TestWorld/playthrough000"

# 1. Unpack the fixture somewhere scratch
rm -rf /tmp/ovtfx && mkdir -p /tmp/ovtfx && tar -xzf "$FX" -C /tmp/ovtfx
SRC=/tmp/ovtfx/game/OVT-Campaign-Test/playthrough000/savepoint000

# 2. Install it as the NEXT savepoint number, so it becomes the latest and
#    nothing already in the profile is touched. 007 here because 000-006 existed.
rm -rf "$DS/savepoint007" && mkdir -p "$DS/savepoint007"
cp -r "$SRC/WorldState" "$DS/savepoint007/"
python3 - "$SRC/meta-info.json" "$DS/savepoint007/meta-info.json" <<'PY'
import json,sys,time,uuid
m=json.load(open(sys.argv[1]))
m["m_sMissionResource"]="{6B0E7A50D1E2F3A4}Missions/25_OVT_TestWorld.conf"
m["m_iSavePointNr"]=7
m["m_Id"]=str(uuid.uuid4())
m["m_iSavedAtUnix"]=int(time.time())
m["m_sSavePointDisplayName"]="jobs-v1-premigration"
json.dump(m,open(sys.argv[2],"w"),indent=4)
PY

# 3. Run headless. No client, no window. 180s is well under the 600s autosave
#    interval (OVT_PersistenceManagerComponent.c:37-38), so the run writes NO
#    new savepoint and the fixture stays the latest for a repeat run.
tools/launch-server.sh --timeout 180 --quiet     # EXIT_CODE=124 is the success shape

# 4. Read the log directory the launcher printed (LOG_DIR=...)
grep -a "Loading savepoint\|Dropping saved job\|Migrated a version 1" "$LOG_DIR/console.log"

# 5. Clean up: the fixture savepoint is the ONLY thing that was added
rm -rf "$DS/savepoint007"
```

⚠️ **Pick the savepoint number by looking**, not by copying `007` — it must be one higher than the highest already present, or the server loads something else. ⚠️ **Delete it afterwards**, or the next `tools/launch-server.sh` continues from a version 1 fixture instead of the user's own dev campaign.

**Observing the SURVIVORS (part b of the check) needs a probe.** The migration's own log line reports counts, not names, so a temporary `Print` was added at the end of `OVT_JobManagerComponent.ApplyPersistedJobs()` listing each restored job's resolved id, index, stage, town, base and owner plus both counter maps — the same technique, and the same discipline, as the F3 probe in task 0.3. It was **reverted immediately after the second run**; `grep -rn "V1PROBE" Scripts/` returns nothing and `compile-check` is clean.

---

## Proven-red record

*Q2: every new assertion is proven able to fail once, with the exact failure text, the breaking method and the date. **`maxAttempts` appears nowhere.** Filled in by tasks 1.5 and 3.3.*

| Case | Tier | Breaking method | Exact failure text | Date |
|---|---|---|---|---|
| T2 `..._StableIdsAreUniqueAndResolve` | Init | **Duplicate two ids** — `Configs/Jobs/raiseSupport.conf` `m_sId` set to `base-recon` (already carried by `baseRecon.conf` at index 1), then `tools/run-tests.sh OVT_TEST_Init_Jobs_StableIdsAreUniqueAndResolve` | `Job config at index 3 repeats the id 'base-recon'. Job ids must be unique: a duplicate makes one job's saved board entries and lifetime counters resolve to the OTHER job on load, which is exactly the silent mis-attachment the stable id exists to prevent.` — exit **1**, `run-tests: FAILED (1 of 1) in 13s` | 2026-08-09 |
| T2 — **second, independent break** (the rename guard) | Init | **Rename a shipped id** — `raiseSupport.conf` `m_sId` set to `raise-support-renamed` (still unique and still well-formed, so only the legacy-resolve branch can catch it) | `The surviving job id 'raise-support' resolves to no config. Either its .conf under Configs/Jobs/ was renamed, its m_sId was edited, or the job was removed. That id is already written into saved campaigns, so every board entry and lifetime counter naming it would be DROPPED on the next load. Job ids are immutable once shipped - put it back.` — exit **1** | 2026-08-09 |
| T1 `..._LegacyIndexMapping` | Logic | **Swap two entries in the frozen table** — `LEGACY_V1_JOB_IDS` indices 3 and 4 exchanged (`raise-support` ↔ `find-shop`, i.e. a survivor swapped with a retired job), then `tools/run-tests.sh OVT_TEST_Logic_Jobs_LegacyIndexMapping` | `The frozen version 1 job table is WRONG at index 3: it says 'find-shop', but that index named 'raise-support'. Every campaign saved before the stable-id migration would restore that job onto the wrong job - same stage index, different reward, wrong lifetime counters, and no error anywhere. The table is history and must never be edited: put it back.` — exit **1** | 2026-08-09 |
| T3 `..._JobBoard_SurvivesSaveAndReload` | PersistenceRoundTrip | **Break the id write, wrong id** — `OVT_JobManagerSerializer.WriteJob()` `record.jobId = jobId;` → `record.jobId = "propaganda-run";`, so every saved job claims to be a different job | `A saved job came back on the WRONG JOB after the reload: it was saved on 'base-recon' and came back on 'propaganda-run' (config index 8). This is the exact silent corruption the stable job id exists to prevent - the job would tick that other job's stages and pay that other job's reward.` — exit **1** | 2026-08-09 |
| T3 — **second, independent break** | PersistenceRoundTrip | **Break the id write, empty id** — `WriteJob()` `record.jobId = "";`, the plan's own suggested method. The run log also carried three `[Overthrow] Dropping saved job '' - no configured job carries that id` WARNINGs, so the honest-drop rule was visible in the same failure | `The job saved on 'base-recon' at <424200, 0, 1> did not come back after the reload. It was saved and then deliberately destroyed in memory, so a board without it means the saved job board was not restored from storage.` — exit **1** | 2026-08-09 |

Both T3 breaks were reverted by exact string replacement and re-verified: `record.jobId = jobId;` is present at `OVT_JobManagerSerializer.c:571` (`ConvertJob`) and `:644` (`WriteJob`), and the case is green again. T1's table was likewise restored verbatim (`:194-207`).

---

## The v1 fixture check (3.4 / 3.5) — what was actually observed

*Run 2026-08-09 with the corrected recipe above. **Both runs went through the real `SaveGameManager` continue path**, not through an in-session re-application: this is the only place in the whole feature where the player-facing quit-and-continue flow was actually exercised.*

**The load happened, and it loaded the fixture.** From `<OverthrowDS>/logs/logs_2026-08-09_22-07-21/console.log:255-257, 316`:

```
22:07:26.659 SCRIPT       : [Overthrow] Dedicated server: a save exists for this mission - continuing the campaign
22:07:26.659 DEFAULT      : [SaveGameManager] Loading savepoint nr.7 'jobs-v1-premigration' from playthrough nr.0 '' for mission '{6B0E7A50D1E2F3A4}Missions/25_OVT_TestWorld.conf'.
22:07:27.082      SCRIPT       : [Overthrow] Session was launched from a save point, continuing the campaign
```

**(a) One WARNING per retired job, naming it — exactly three, exactly the three predicted** (`console.log:331-334`):

```
22:07:27.087  SCRIPT    (W): [Overthrow] Dropping saved job 'find-gun-dealer' from a version 1 save - that job was retired and no longer exists. Its lifetime counters are dropped with it
22:07:27.088  SCRIPT    (W): [Overthrow] Dropping saved job 'find-shop' from a version 1 save - that job was retired and no longer exists. Its lifetime counters are dropped with it
22:07:27.088  SCRIPT    (W): [Overthrow] Dropping saved job 'place-equipment-box' from a version 1 save - that job was retired and no longer exists. Its lifetime counters are dropped with it
22:07:27.088  SCRIPT       : [Overthrow] Migrated a version 1 job payload: 2 of 4 board entries and 2 of 4 global counters carried forward
```

**(b) The survivors came back on their correct configs** (`console.log:335-339`, temporary probe, since reverted):

```
22:07:27.088  SCRIPT       : [OVT-V1PROBE] board after restore = 2 jobs
22:07:27.088  SCRIPT       : [OVT-V1PROBE] job id=assassinate-traitor index=0 stage=0 town=0 base=-1 owner=OVTFIXTUREOWNERA
22:07:27.088  SCRIPT       : [OVT-V1PROBE] job id=raise-support index=3 stage=0 town=1 base=-1 owner=
22:07:27.088  SCRIPT       : [OVT-V1PROBE] globalcount id=assassinate-traitor index=0 value=3
22:07:27.088  SCRIPT       : [OVT-V1PROBE] globalcount id=assassinate-officer index=11 value=2
```

**Every one of those lines matches Phase 2's byte-level prediction, field for field.** §"The `$type` discovery" predicted from the blob alone: board indices `0, 3, 2, 4`; globals `[4,0,2,11] → [1,3,1,2]`; per-player `OVTFIXTUREOWNERB` `[4,2,5] → [1,1,1]`; therefore three distinct retired drops, the survivors `assassinate-traitor` and `raise-support` restored, and global counters `assassinate-traitor = 3`, `assassinate-officer = 2`. That is exactly what came out, including `stage`, `townId` and `owner` matching the fixture's own seed table row for row (index 0: stage 0, town 0, `OVTFIXTUREOWNERA`; index 3: stage 0, town 1, unowned). **A static decode and a live migration agreeing to the field is the strongest evidence this feature can produce.**

**No `playercount` line is a pass, not a gap.** The fixture's only per-player record is `OVTFIXTUREOWNERB` with indices `4, 2, 5` — all three retired. Every entry was dropped, so the player's restored counter map is empty and the probe (which prints per entry) prints nothing for it. F2 wanted retired counters dropped rather than reassigned; they were.

**3.5 — the re-run is line-for-line identical.** `logs_2026-08-09_22-11-06/console.log:257, 331-339` carries the same nine lines with the same values, differing only in timestamp: same savepoint loaded, same three named drops, same `2 of 4 / 2 of 4`, same two restored jobs, same two counters. Nothing doubled, nothing accumulated, nothing lost. The save tree also proves the re-run was against the *same* fixture: `playthrough000` still held exactly `savepoint000`-`savepoint007` after both runs, so no new save point was written between them and `LoadLatestSave()` picked the fixture both times.

---

## F3 — the Eden override measurement

*Task 0.3, measured 2026-08-09. The prior (`OVT_TEST_SuiteBase.c:36-41`, "same-GUID overrides are deltas") was **not** used as the answer — this is a direct measurement, per R6.*

- **Method: the PRIMARY method, not the fallback.** A temporary `Print` was added to `OVT_JobManagerComponent.Init()` (`Scripts/Game/GameMode/Managers/OVT_JobManagerComponent.c:97-101`) logging `m_aJobConfigs.Count()` and each config's `m_sTitle`, compile-checked clean, then run headlessly with `tools/launch-server.sh --scenario eden --timeout 120` (**no client was launched — no window on the user's desktop**). The `Print` was reverted immediately afterwards and `git diff` confirmed zero product change. The test-world fallback in the plan was **not** needed.
- **The layer was proven to be in play**, which is the whole point of the measurement. The same `console.log` carries, at line 149:
  ```
  20:49:28.874  WORLD        : Entity layer load @"$Overthrow:Worlds/MP/OVT_Campaign_Eden_Layers/managers.layer"
  ```
  and the mission is Eden (line 142: `[SaveGameManager] Starting new playthrough nr.0 '' for mission '{3DAD390C31623F04}Missions/24_OVT_Eden.conf'`).
- **Measured `m_aJobConfigs.Count()` on Eden: 12.** Verbatim, from `<OverthrowDS profile>/logs/logs_2026-08-09_20-49-23/console.log:270-282`:

```
20:49:45.734      SCRIPT       : [OVT-F3PROBE] m_aJobConfigs.Count() = 12
20:49:45.734      SCRIPT       : [OVT-F3PROBE] 0 = #OVT-Job_AssassinateTraitor
20:49:45.734      SCRIPT       : [OVT-F3PROBE] 1 = #OVT-Job_BaseRecon
20:49:45.734      SCRIPT       : [OVT-F3PROBE] 2 = #OVT-Job_FindGunDealer
20:49:45.734      SCRIPT       : [OVT-F3PROBE] 3 = #OVT-Job_RaiseSupport
20:49:45.734      SCRIPT       : [OVT-F3PROBE] 4 = #OVT-Job_FindShop
20:49:45.734      SCRIPT       : [OVT-F3PROBE] 5 = #OVT-Job_PlaceEquipmentBox
20:49:45.734      SCRIPT       : [OVT-F3PROBE] 6 = #OVT-Job_RecruitACivilian
20:49:45.734      SCRIPT       : [OVT-F3PROBE] 7 = #OVT-Job_PlaceCamp
20:49:45.734      SCRIPT       : [OVT-F3PROBE] 8 = #OVT-Job_PropagandaRun
20:49:45.734      SCRIPT       : [OVT-F3PROBE] 9 = #OVT-Job_PirateRadio
20:49:45.734      SCRIPT       : [OVT-F3PROBE] 10 = #OVT-Job_SabotageRadioTower
20:49:45.734      SCRIPT       : [OVT-F3PROBE] 11 = #OVT-Job_AssassinateOfficer
```

- **Verdict: MERGE (patch-in-place).** Twelve entries, in the game-mode prefab's exact order (`Prefabs/GameMode/OVT_OverthrowGameMode.et:26-48`), with **no duplicates** — so it is neither *replace* (which would have shown 5) nor *naive append* (which would have shown 17 with 5 duplicates). A same-GUID array-element override on a world entity patches the matching element in place and contributes no new entry. Note this is a **different** result shape from the `m_aDifficultyPresets` prior recorded at `OVT_TEST_SuiteBase.c:36-41`, and that is exactly as expected: that one was a **different**-GUID element, which appended.
- **Consequence: Eden has always shipped the full twelve jobs.** The "Eden runs seven jobs short" hypothesis (R1) is **disproven**. The stale five-entry block at `Worlds/MP/OVT_Campaign_Eden_Layers/managers.layer:29-40` is inert today — it overrides no member of any of the five configs it names (all five bodies are empty).
- **Bug filed (0.4): NONE.** The condition for filing was "replace, or duplicates"; the measurement shows neither. This is recorded as the measurement, per the task's own instruction. The next free bug id remains **BUG-134**.
- **This does not change the code (D7).** Phase 4.2 still deletes the block — under merge it is pure cleanup, and it *must* go regardless because after 4.3 it would name five `.conf` resources that no longer exist.

---

## The v1 fixture

*Tasks 0.5-0.7. The archive stays out of git (D10); this is the recreate recipe.*

- **Archive:** `.saves/jobs-v1-premigration_20260809_205446.tar.gz` (4.8 KB), profile `OverthrowCI`, captured 2026-08-09 on commit `f6681d00` with an otherwise-unmodified tree. **Out of git (D10)** — `.saves/` is gitignored, so the recipe below is the durable artifact.
- **It is genuinely v1.** `OVT_JobManagerSerializer.c:97` writes `context.WriteValue("version", 1)` on this tree, so this payload is exactly what a real pre-feature campaign save carries. **After Phase 2 this tree can no longer write one.**

### How it was seeded — stated plainly

**Not** by playing. The board was seeded **through the job manager's public `ApplyPersistedJobs()` entry point** — the manager's own restore path, and the one public API that can place a *known* board deterministically — from a **temporary** autotest suite, which was then deleted. Two reasons this was the right vehicle rather than a running server: the starter jobs are player-allocated and only offer to a *player* (a headless server has none), and `CheckUpdate()`'s offer loop is condition- and random-gated, so "wait until the right five jobs appear" is not a procedure.

The temporary file was `Scripts/Game/Tests/TestSuites/Persistence/OVT_TEST_TempJobFixtureSuite.c` (suite `OVT_TEST_TempJobFixtureSuite` with `RequiresStartedCampaign()` true, one case `OVT_TEST_TempJobFixture_SeedAndSave`). It reused the round-trip suite's two annotated seams via `OVT_TEST_PersistenceRoundTripGate.TriggerSaveOnce()` / `PollSaveSettled()` rather than opening a third. **It was deleted after capture and `git status` is clean** — see the recipe below to recreate it.

**Seeded board** (all `location`, `townId`, `stage` chosen so `FindRestorableJobConfig()` accepts them — no stage is an `OVT_WaitTillDeadJobStage`):

| v1 index | job | stage | townId | owner | fate after this feature |
|---:|---|---:|---:|---|---|
| 0 | `assassinateTraitor` | 0 | 0 | `OVTFIXTUREOWNERA` | **survivor** |
| 3 | `raiseSupport` | 0 | 1 | *(unowned)* | **survivor** |
| 2 | `findGunDealer` | 0 | 0 | `OVTFIXTUREOWNERB` | **retired** |
| 4 | `findShop` | 1 | 1 | `OVTFIXTUREOWNERB` | **retired** |

Every record also carries `declined = ["OVTFIXTUREDECLINER"]`. The three `OVTFIXTURE*` strings are deliberate **decode markers** — they are the only human-readable tokens in the job payload, and finding them in the blob is what proves the board is really in there (0.6).

**Seeded counters — non-zero in BOTH maps, as required:**

- `m_aJobCounts` (global): `{2: 1, 4: 1, 0: 3, 11: 2}` — two retired indices, two surviving indices.
- `m_mPlayerJobCounts` (per-player): `{"OVTFIXTUREOWNERB": {2: 1, 4: 1, 5: 1}}` — three retired indices under one player. Built **synthetically**, because no shipped config is player-allocated after removal (§3.8).

Seed confirmed live before the save, from the run's `console.log`:

```
[OVT-FIXTURE] board after seed = 4 jobs
[OVT-FIXTURE] seeded jobIndex=0 stage=0 town=0 owner=OVTFIXTUREOWNERA
[OVT-FIXTURE] seeded jobIndex=3 stage=0 town=1 owner=
[OVT-FIXTURE] seeded jobIndex=2 stage=0 town=0 owner=OVTFIXTUREOWNERB
[OVT-FIXTURE] seeded jobIndex=4 stage=1 town=1 owner=OVTFIXTUREOWNERB
[OVT-FIXTURE] save settled after 1 poll(s); board = 12 jobs
```

⚠️ **One honest caveat.** The board read **12** by the time the save settled: the manager's own `CheckUpdate()` had offered eight further public jobs in the ~190 ms between the seed and the save landing. The four seeded records are unaffected (nothing removes a job that has not completed) and the decode below confirms them, but a consumer of this fixture must expect **more than four** job records, not exactly four, and must assert on the marker strings rather than on a record count.

### Decode result (0.6) — the fixture is non-empty and worth keeping

`tools/decode-savepoint.py summary` on the captured save point: `bytes=13267 records=12 System=3 Player=2 Character=12 Vehicle=1`.

`tools/decode-savepoint.py strings <savepoint> --min 8` shows the job payload in full, in order — **this is the proof**:

```
shift=0 0x23d    OVT_JobManagerComponent:59A8D178EC1AA2AE
shift=0 0x277    jobRecords
shift=0 0x292    OVT_PersistedJob
shift=0 0x2a3    jobIndex
shift=0 0x2b0    location
shift=0 0x2c3    HCtownId
shift=0 0x2eb    OVTFIXTUREOWNERA
shift=0 0x2fc    accepted
shift=0 0x306    declined
shift=0 0x314    OVTFIXTUREDECLINER
shift=0 0x363    KCtownId
shift=0 0x3f3    JCtownId
shift=0 0x41b    OVTFIXTUREOWNERB
shift=0 0x493    LCtownId
shift=0 0x4f7    countIndices
shift=0 0x519    countValues
shift=0 0x53a    playerCounts
shift=0 0x557    OVT_PersistedPlayerJobCounts
shift=0 0x574    playerId
shift=0 0x58e    jobIndices
```

Read off that dump: the job manager's record exists; `jobRecords` holds `OVT_PersistedJob` entries (four `townId` field markers, one per record, at `0x2c3 / 0x363 / 0x3f3 / 0x493`); **both** fixture owner markers and the declined marker are present; and **both counter maps are written** — `countIndices` + `countValues` for the global map and `playerCounts` → `OVT_PersistedPlayerJobCounts` → `playerId` / `jobIndices` for the per-player map. **The board is not empty. The fixture is kept.**

*(The counter **values** are ints and are not readable in a `strings` dump; they are corroborated by the seeding log above and by the fact that the record sections exist at all. Phase 3.4 reads them back through the manager, which is the real assertion.)*

### Recreate recipe (0.7)

The archive is out of git, so this is how to make another one. **It only works on a pre-Phase-2 tree** — after the serializer migration the tree writes version 2 and cannot produce a v1 payload at all.

```bash
# 1. A worktree at the pre-migration commit. f6681d00 is the commit this fixture
#    was captured on; any commit before Phase 2's serializer change will do.
git worktree add ../Overthrow.v1fixture f6681d00
cd ../Overthrow.v1fixture

# 2. Recreate the temporary seeder at
#    Scripts/Game/Tests/TestSuites/Persistence/OVT_TEST_TempJobFixtureSuite.c
#    - a [BaseContainerProps()] suite extending OVT_TEST_SuiteBase with
#      RequiresStartedCampaign() -> true, and one [Test(...)] case that:
#        a) resolves OVT_Global.GetJobs()
#        b) builds the four OVT_PersistedJob records in the table above
#           (markers OVTFIXTUREOWNERA / OVTFIXTUREOWNERB / OVTFIXTUREDECLINER)
#        c) builds countIndices/countValues {2:1, 4:1, 0:3, 11:2} and one
#           OVT_PersistedPlayerJobCounts for "OVTFIXTUREOWNERB" {2:1, 4:1, 5:1}
#        d) calls jobs.ApplyPersistedJobs(records, countIndices, countValues, playerCounts)
#        e) calls OVT_TEST_PersistenceRoundTripGate.TriggerSaveOnce() and then
#           polls PollSaveSettled(baseline) until SAVE_SETTLED
#    Do NOT add it to either group config - it is run by class name.
tools/compile-check.sh                                   # must exit 0

# 3. Seed + save (run-tests.sh resets OverthrowCI's save state itself first)
tools/run-tests.sh --keep-artifacts OVT_TEST_TempJobFixture_SeedAndSave
grep -a "OVT-FIXTURE" .tmp/run-tests/console.log         # board after seed = 4 jobs

# 4. Archive it
.scripts/backup_save.sh --profile OverthrowCI jobs-v1-premigration

# 5. Prove it is non-empty BEFORE trusting it (0.6)
tools/decode-savepoint.py strings \
  "<My Games>/OverthrowCI/profile/.save/app*_user*/game/OVT-Campaign-Test/playthrough000/savepoint000" \
  --min 8 | grep -i OVTFIXTURE                           # must print three markers

# 6. DELETE the temporary suite and confirm a clean tree
rm Scripts/Game/Tests/TestSuites/Persistence/OVT_TEST_TempJobFixtureSuite.c
tools/compile-check.sh && git status --porcelain
```

**Consuming it later (Phase 3.4):** `.scripts/activate_save.sh jobs-v1-premigration --profile OverthrowCI`, then `tools/launch-server.sh` (headless). ⚠️ `activate_save.sh` must be given `--profile OverthrowCI` — its default target is the user's real Workbench campaign save.

> **Doc correction found while doing this:** `tools/README.md` §"Save-state control" still says `backup_save.sh`/`activate_save.sh` "**still speak only EPF** and are pending the fixture rework". That is **stale** — `backup_save.sh` resolves `<profile>/profile/.save/app*_user*/game` (`.scripts/backup_save.sh:29-62`) and archived a vanilla save point correctly here. Not corrected in this phase (out of scope, and `tools/README.md` is a contract document owned by the dev-ops epic), but worth knowing before anyone plans around the stale sentence.

---

## The version 1 support policy, and the trigger that ends it

*D5, written into the serializer header in task 2.8 and restated here so it survives a file rewrite. Recorded verbatim from `OVT_JobManagerSerializer.c:158-174`.*

**Version 1 support is KEPT until the next save-format-breaking change to this component, and then it goes.** It is stated as a trigger rather than a date so it cannot rot:

> **THE NEXT TIME `OVT_PersistedJobV2` CHANGES SHAPE, VERSION 1 GOES AND VERSION 2 BECOMES THE FLOOR:** `DeserializeVersion1()`, `OVT_PersistedJob`, `OVT_PersistedPlayerJobCounts`, `LEGACY_V1_JOB_IDS`, `LegacyIdForIndex()` and `IsRetiredLegacyId()` are all deleted together, and the guard becomes `if (version < 2) return true;`.

Why it is kept at all, and why not forever — the cost is asymmetric in both directions:

- **Keeping it is nearly free.** The version 1 path is ~40 lines of pure mapping and costs one integer comparison on the version 2 path.
- **Dropping it early is catastrophic and silent.** The `version < 1` early return would see a perfectly valid version 1 payload and treat it as a normal load, wiping the job board and every lifetime counter off a live campaign with no error (`OVT_JobManagerSerializer.c:169-173`).
- **Keeping it forever is a liability.** A frozen class nobody exercises rots. The trigger above is precisely the moment its cost stops being nearly zero.

Two things a future editor must not undo:
1. **The frozen record classes can never be renamed** — the payload names them (§"The `$type` discovery").
2. **The `version < 1` guard is byte-unchanged and load-bearing** (Q5). Without it, an *absent* payload wipes the board.

---

## Residual gaps accepted — decided, not overlooked (D13)

*The user's decision was to accept both and add no new tutorial entry. Recorded here because "accepted" and "unnoticed" look identical in six months.*

**1. Directed discovery.** `findShop` and `findGunDealer` each wrote one specific instance's position into `job.location`, i.e. they put a marker on a **named instance** and no tip does that. **Largely moot, and this is measured rather than argued:** `OVT_MapIcons.c:137` `TryCreateShopIcon` draws **every** registered shop with a per-type sprite, and `:111` `TryCreateGunDealerIcon` draws **every** gun dealer with a dedicated `"gundealer"` sprite — both enumerated with **no discovery or knowledge gate** of any kind. A player with a map already sees all of them. What is genuinely lost is "go to *this* one", which is exactly the directed-goal shape the `new-player-experience` epic exists to remove.
⚠️ The related trap: `tutorial-content/implementation.md` §3.5 once claimed gun dealers have **no** dedicated map icon. That claim is **FALSE** and was struck in three documents. Do not revive it.

**2. Recruit availability.** `recruits-first-recruit` (`Configs/Tutorials/recruitsFirstRecruit.conf`) triggers on `PLAYER_RECRUIT_ADDED` with an empty filter — it fires on the first recruit **gained**, not on the recruit option becoming available. A player who never uses "Recruit Civilian" never sees it. The retired `recruitACivilian` job had the same completion shape (`OVT_HasRecruitJobStage` also only completed once a recruit was held) but it **appeared in the Jobs list beforehand**, which the tip system has no equivalent of. **This is the one capability the removal genuinely gives up**, and it is accepted.

Neither gap is a reason to keep a job: a job is a directed goal with a marker on a named instance, which is the shape this epic replaced.

---

## The string-export position (task 5.7)

**No Workbench string re-export is required by this feature.**

- The only file under `Language/` this feature touched is the master `Language/localization_Overthrow.st`, and the only fields written are **`Comment`** fields (ten retirement records + two corrected forward-references). `git diff -U0 Language/` on close contains **zero** changed lines that are not `Comment` lines — checked, not asserted.
- `Comment` is editorial metadata. **No rendered text changed**, no `Id` was added or removed, and no item was deleted, so the runtime `localization_Overthrow.<lang>.conf` exports remain correct as they stand.
- **No `.<lang>.conf` was opened.** Those are Workbench-generated, their `Ids{}`/`Texts{}` blocks are neither parallel nor same-length, and six of them were corrupted once by hand-editing and had to be reverted.

**If the user runs an export anyway, it is harmless and it collects a debt already owed:** `first-spawn` left one string un-exported — **`OVT-FieldManual_Welcome_Text2`** (its Welcome page gained a houseless clause so it stops contradicting the `welcome-nohome` entry that deep-links to it). That failure is silent rather than a raw key — the manual simply shows the older paragraph — so it will land in the same pass without any extra step.

---

## Session Notes

### 2026-08-09 — Feature started (autorun)

Docs scaffolded from `implementation.md`. 52 tasks across 7 phases (0-6). Phase 2 routes to `component-developer-advanced` per the plan's explicit flag; Phase 3 follows it there. Phase 6 routes to `help-docs-sync`.

The plan arrives with an unusual property worth noting up front: **its own stated justification was already stale**, and the plan says so in its executive summary. That is a good sign about the plan and a warning about everything else — R6 is a live risk in this epic, not a hypothetical.

### 2026-08-09 — Phase 0 complete (verify, freeze, capture)

All eight tasks done. **Zero product change:** `git diff` touches only `docs/`. Two temporary code artifacts were created and both were reverted — the F3 `Print` in `OVT_JobManagerComponent.Init()` and the `OVT_TEST_TempJobFixtureSuite.c` seeder — each verified gone by a clean `git status` and a green `compile-check.sh` afterwards.

**0.1 — R6 caught one.** Twelve of thirteen §3.8 rows stand; **the rewards row fell**. `m_iRewardXP` defaults to **5** (`OVT_JobConfig.c:26-27`) and is paid unconditionally (`OVT_JobManagerComponent.c:440-442`), and four of the five retired configs never declare it — so the XP lost is **30, not 10**. Struck in place in §3.8 with the correction and the date, not silently fixed. Three further rows stand but had drifted line numbers, corrected inline. The row that mattered most to R6 — gun dealers *do* have a dedicated map icon — **stands**.

**0.2 — the plan's baseline was already one behind.** Measured **Fast 52 / All 89** against the plan's 51 / 88, and the highest bug id is **BUG-133**, not BUG-132. Both are exactly the parallel-session drift R7 predicts, three days after planning. Deltas, not absolutes, from here on.

**0.3 — F3 is answered, and R1 is dead.** The Eden layer's same-GUID `m_aJobConfigs` override **MERGES**: 12 configs, prefab order, no duplicates, with the layer proven loaded in the same log. Eden has never been short a job. Worth noting *why* the prior did not settle this: `OVT_TEST_SuiteBase.c:36-41` observed an **append**, but for a **different**-GUID element — the two findings are consistent, not contradictory, and only the measurement could tell them apart.

**0.4 — nothing filed**, which was the conditional outcome. Next free id stays BUG-134.

**0.5-0.7 — the fixture exists and is proven non-empty.** Seeded through the manager's public `ApplyPersistedJobs()` from a temporary autotest, saved through the round-trip suite's existing annotated save seam, archived to `.saves/jobs-v1-premigration_20260809_205446.tar.gz`, and decoded: three fixture marker strings, `OVT_PersistedJob` records and **both** counter-map sections are visibly in the blob. The one caveat worth carrying: `CheckUpdate()` added eight more public jobs before the save landed, so the fixture holds **12** board records, not 4 — assert on the markers, not on a count.

**0.8 — the frozen table needed no correction.** Confirmed against `OVT_OverthrowGameMode.et:26-48` and independently against the F3 probe's runtime title order.

### 2026-08-09 — Phase 1 complete (stable ids on the config surface)

All seven tasks done. **Zero runtime behaviour change**, and that is a checked claim rather than an intention: a repo grep for `FindJobIndexById`, `GetJobIdByIndex`, `GetJobConfigCount` and for `m_sId` on an `OVT_JobConfig` returns the definitions themselves plus **only** `OVT_TEST_InitSuite.c`. Nothing in the serializer, the manager's own logic, the UI or any RPC reads the new field yet. The whole diff is one attribute, twelve one-line config insertions, three getters and one test case.

**Preconditions re-checked at the phase front (R7):** `git status` clean except this feature's three doc files, `git log -1` still `f6681d00`, highest bug id still **BUG-133** (next free **BUG-134**). Baseline re-measured before touching anything and it had **not** moved: Fast 52 / All 89, compile-check exit 0. No parallel session landed anything during the phase.

**T2 is a NEW case, not a branch — count delta +1 / +1.** Stated explicitly because `first-spawn` got caught by the opposite: a case added as a *branch* on an existing case raises the assertion count but not the test count, and the expected-count arithmetic then looks wrong. Here the shape was chosen deliberately for diagnosis — a red run should name "job stable ids", not some unrelated case it was bolted onto.

**How the Phase 4 assertion was made inert — and why it still cannot pass silently.** `RETIRED_IDS_ARE_DELETED` (`OVT_TEST_InitSuite.c:3164`) is a `static const bool` set to `false`, documented as *the only thing task 4.5 has to change*. It is a **switch, not a commented-out block**, and that matters: while it is false the case does not skip the retired ids, it asserts the **opposite** — every one of the five must **still** resolve. So the state today is genuinely pinned rather than merely tolerated, and the moment Phase 4 deletes the five configs the case goes **red on its own**, with a message that names the constant and task 4.5 and tells the reader to flip it. Deleting without flipping is impossible to do quietly, and flipping early is equally impossible: `RETIRED_IDS_ARE_DELETED = true` on today's tree fails on the first retired id that still resolves. A commented-out block would have satisfied the letter of "present but inert" while being invisible to every run — this does not.

**One addition beyond the plan's two helpers: `GetJobConfigCount()`** (`OVT_JobManagerComponent.c:404-408` (declared at `:404`)). `m_aJobConfigs` is `protected` and there was no public count accessor, so the guard could not iterate the configs at all. The alternative — walking indices until `GetJobIdByIndex` returns `""` — was rejected because an empty id is precisely one of the failure modes T2 exists to catch, and using it as a terminator would make that assertion unreachable. It is a plain Doxygen'd getter over a protected member, which is house style.

**Handover to Phase 2 — the guard currently holds its OWN legacy id lists.** `SURVIVING_LEGACY_IDS` (seven) and `RETIRED_LEGACY_IDS` (five) are literal arrays inside the T2 case (`OVT_TEST_InitSuite.c:3167-3188`), because `LEGACY_V1_JOB_IDS` does not exist until task 2.4. That is a **deliberate, documented duplication with a note in the code saying so** — Phase 2 should decide whether to point the case at the frozen table instead of maintaining two copies of the same history. Whichever way it goes, Q6 still wants the twelve-entry table in this file and in the serializer.

**Nothing surprising surfaced.** The twelve configs took the new attribute cleanly (name-keyed `.conf` parsing, one-line insertions, no line-ending churn), and `Configs/Tutorials/` and `Configs/FieldManual/` have a **zero** diff, as the sibling feature requires.

### 2026-08-09 — Phase 2 complete (the serializer migration)

All nine tasks done. **Nothing deleted** — the five job configs, their prefab registrations and the Eden layer block are all exactly where Phase 1 left them, and `git status` shows no `D` line.

**Preconditions re-checked at the phase front (R7):** `git log -1` still `f6681d00`, highest bug id still **BUG-133** (next free **BUG-134**), working tree carrying only this feature's own Phase 0-1 changes. Baseline re-measured before touching anything and it had **not** moved: compile-check exit 0, Fast **53**, All **90**.

**The phase's real finding is that the plan's D4 was measurably unsafe, and it was caught before it shipped.** §"The `$type` discovery" above has the full evidence; the short version is that the binary container writes the concrete class name into the payload and instantiates from it, so a frozen shadow class with a *new* name cannot read an *old* payload — the read fails, hands back an empty-but-non-null array, poisons the rest of the stream, and `ApplyPersistedJobs()` applies the emptiness. The plan's own worst case, reached by way of the plan's own mitigation. The fix keeps D4's intent whole and only swaps which class carries which name: `OVT_PersistedJob` / `OVT_PersistedPlayerJobCounts` are now the frozen version 1 records, `OVT_PersistedJobV2` / `OVT_PersistedPlayerJobCountsV2` the current ones.

**Method note, because it is the transferable part.** The answer was not deduced, it was measured: two throwaway autotest suites doing in-memory round trips through the *real* `SCR_PersistenceBinarySaveContext` / `SCR_PersistenceBinaryLoadContext`, plus a byte-level decode of the captured v1 fixture. Both suites were deleted afterwards and `compile-check` re-run clean. The engine's serialization API is `proto` — there is no source to read — so a probe is the only way to know, and three separate assumptions in the plan and this file turned out to need correcting (`$type`, the local-variable name rule, and Phase 3.4's recipe).

**The `version < 1` guard is byte-unchanged**, and that is checkable rather than asserted: in `git diff` its four lines and their comment appear as **context**, not as `+`/`-`. Q5's other three halves are intact too — `ApplyPersistedJobs()` still clears and rebuilds the same four collections, the `OVT_WaitTillDeadJobStage` drop still fires (`OVT_JobManagerComponent.c:371-377`), and the occupancy sets are still derived by `RegisterRestoredJobOccupancy()` rather than stored.

**Q4 proved by diff, not by assurance.** Across the whole uncommitted tree, the only line matching `RplRpc|RplSave|RplLoad|Rpc\(|RplProp|Replication\.|writer\.Write|reader\.Read` is a **comment** in `ApplyPersistedJobs()`'s own header saying the wire did not change. `OVT_PlayerCommsComponent.c` has no diff at all. R4's `jobIndex` grep returns six files, not the plan's five: the four that still use the int unchanged (`OVT_JobsContext`, `OVT_PlayerCommsComponent`, `OVT_JobManagerComponent`, and a comment in `OVT_TutorialComponent`), the serializer, and `OVT_TEST_InitSuite.c` — the sixth is a **comment** added by Phase 1's own T2 header, not a new positional dependency.

**One addition beyond the task list: every `context.Read()` is now checked.** A failed read used to be indistinguishable from an empty board because the return value was discarded; the probe showed an empty board reads back `true` with zero records and `false` only on a genuine failure, so the two *are* distinguishable and a failure now aborts without touching the live board (`AbortUnreadablePayload`, `OVT_JobManagerSerializer.c:603`). This also makes "a version 3 save loaded by a version 2 build" safe by construction rather than by luck.

**Drop-message call sites**, all four §3.4 shapes, exact:

| Case | Level | Site |
|---|---|---|
| v1 record naming a retired job | WARNING | `OVT_JobManagerSerializer.c:554`, in `ResolveLegacyIndex()` |
| v1 index outside the twelve | WARNING | `OVT_JobManagerSerializer.c:545`, in `ResolveLegacyIndex()` |
| v2 id matching no config | WARNING | `OVT_JobManagerComponent.c:345` (board, inside `FindRestorableJobConfig()`), `:231` (global counters), `:262` (per-player counters) |
| config with empty `m_sId` at save time | ERROR | `OVT_JobManagerSerializer.c:633`, in `ReportIdlessConfig()` |

**One WARNING per retired job, not per record — deliberate, and it is what 3.4 expects.** The retired message says the job's lifetime counters are dropped with it, so repeating it for each counter entry would contradict its own wording; `ResolveLegacyIndex()` dedupes by id through a `set<string>` that lives for one load. Everything else logs per occurrence, which is what F7 wants. The retired drop is also **unconditional** — it does not check whether the config still exists — because the five `.conf` files outlive this phase by design, and a save's contract must not depend on which day the build was cut.

**T2's duplicated legacy lists: KEPT, decided rather than deferred.** `SURVIVING_LEGACY_IDS` / `RETIRED_LEGACY_IDS` stay literal in `OVT_TEST_InitSuite.c` and are **not** pointed at `LEGACY_V1_JOB_IDS`. The reasoning is written into the code at the lists themselves (`OVT_TEST_InitSuite.c:3179-3193`) so it cannot drift from them: they are different invariants that merely share strings today (the frozen table is *positional history* and must never grow; T2's lists describe the *live* config list and should grow when a thirteenth job is added); an independent witness is the whole value of a guard, and the realistic mistake is renaming a `.conf` id and then "keeping the table in sync", which two copies catch and one does not; and the frozen table is not left unguarded, because Phase 3's T1 pins it against its own literals in the Logic tier. The edit was **comment-only** — no assertion changed, no count moved.

**What Phase 2 did NOT prove, and Phase 3 must.** The version 1 branch has been verified statically against the real fixture's bytes and its component parts have been measured, but nothing has yet *executed* it end to end: no test drives `DeserializeVersion1()`, and the v2 path was only exercised with an empty board (confirmed by decoding the save point the All-group run left behind — `version` = **2**, properties `jobRecords`, `countIds`, `countValues`, `playerCounts`, all empty). T1, T3 and the 3.4 fixture check are what turn "verified" into "observed", and 3.8 is the gate. Note also that the four `version 2` save points now sitting in the `OverthrowCI` profile are **not** fixtures — they were written by the test harness and are reset on every run.

### 2026-08-09 — Phase 3 complete (prove it), and the decision gate

All eight tasks done. **Nothing was deleted** — the five job configs, their prefab registrations and the Eden layer block are exactly where Phase 2 left them, and `git status` shows no `D` line. The only product files this phase changed are the **two test files**; the temporary probe in `OVT_JobManagerComponent.ApplyPersistedJobs()` and the two deliberate breaks in `OVT_JobManagerSerializer.c` were all reverted and re-verified by grep (`V1PROBE` → 0 hits, `record.jobId = jobId;` present at both `:571` and `:644`, `LEGACY_V1_JOB_IDS` verbatim at `:194-207`).

**Preconditions re-checked at the phase front (R7):** `git log -1` still `f6681d00`, highest bug id still **BUG-133** (next free **BUG-134**), working tree carrying only this feature's own Phase 0-2 changes. Baseline re-measured before touching anything and it had **not** moved: compile-check exit 0, Fast **53**, All **90**.

#### The blocker was worse than Phase 2 thought, and the fix is better than the plan asked for

Phase 2 flagged two mismatches in the 3.4 recipe. There were **three**: in `--mode local` a server authenticates nobody and writes to `profile/.save/game/…` with **no `app*_user*` component at all**, and `activate_save.sh` resolves *only* `app*_user*` paths (`.scripts/activate_save.sh:38-67`). So the plan's tool could never have addressed the target, with any flags.

Working that out changed the shape of the answer for the better. Because the *server* auto-continues (`OVT_OverthrowGameMode.DecideDedicatedStart:465-487` → `LoadLatestSave()`), staging the fixture as the newest save point in the server's own tree runs the migration through **`SaveGameManager` and a real world transition** — the exact flow §7 lists as something automation "structurally cannot cover", and which the round-trip test tier deliberately substitutes an in-session re-application for. The harness route was considered and rejected. One half of that is measured, the other is reasoning, and they are worth separating: the harness **cannot** perform a save-point load at all — `OVT_TEST_PersistenceRoundTripSuite.c:141-149` records that a mid-case load restarts the whole suite in an infinite loop, which is why that tier substitutes an in-session re-application in the first place. So the harness could only ever have reached the fixture through `ReapplyLatestSaveData()`, which asks the persistence system for the stored record of the live game-mode instance (`OVT_PersistenceManagerComponent.c:533-537`) — and in a session that started fresh, the only record it can have is one that same session wrote, i.e. a version 2 payload. **That second step was NOT measured**, because the server route was strictly better regardless: it is the real path, and measuring a route in order to discard it buys nothing.

#### What is proven automatically, and by what

| Claim | Proven by | Tier / gate |
|---|---|---|
| The frozen v1 index→id table is correct for all twelve indices and out-of-range resolves to nothing | **T1**, against its own independently transcribed literals | Logic — in **both** groups |
| Exactly the five retired ids are retired, and none of the seven survivors is | **T1** | Logic — both groups |
| The v2 board round-trips through storage on the **right config**, with stage, owner, location, town, base, accepted and the decline list intact | **T3** | PersistenceRoundTrip — All group |
| Both lifetime counter maps round-trip, keyed to the right jobs | **T3** | All group |
| Re-applying the **same** payload twice produces the same board, not a doubled one (D6) | **T3**'s second re-application | All group |
| Every config's `m_sId` is non-empty, kebab-case and unique; index→id→index round-trips; every surviving legacy id still resolves | **T2** (Phase 1) | Init — both groups |
| Nothing else regressed | Fast 54 / All 92, both exit 0 | both groups |

#### What only the fixture check proves

- **A genuine pre-migration campaign save loads through the real continue path** and is migrated — not a synthetic payload, not an in-session re-application. `SaveGameManager` loaded it and the world transitioned.
- **F1 and F7 in the shape the DoD asks for:** one WARNING per retired job, *naming* it, three of them, and the two survivors restored on the configs they were saved on with their stage, town and owner intact.
- **F2:** the global counters came back on `assassinate-traitor` and `assassinate-officer` only; the `find-shop` and `find-gun-dealer` entries were dropped rather than reassigned, and the one per-player record — every entry of which named a retired job — came back empty.
- **The v1 branch actually executes.** Phase 2 verified it statically against the fixture's bytes and measured its components in isolation, but nothing had run `DeserializeVersion1()` end to end. It has now, twice, and its output matches the static decode field for field.

#### What only the user can prove (§7's four structural gaps, unchanged)

1. **The real quit-and-continue path on a REAL campaign.** The fixture check now covers the *mechanism* — `SaveGameManager.Load`, world transition, deserialize — but on a 4-record test-world save that this feature seeded itself. The user's own campaign is neither small nor seeded, and **U1 remains the single most important observation in the feature**. Back the save up first.
2. **Multiplayer and JIP.** Untouched by this phase and untouchable by it. The design's answer is that the wire did not change (Q4, proven by diff in Phase 2), but M1-M5 are the only thing that observes it — and the epic records that two-client per-player isolation has never been seen passing.
3. **UI.** Whether the Jobs menu still lists, accepts and declines correctly. Nothing here opens a menu.
4. **A real campaign's save.** The fixture holds four job records and two counter entries. A long campaign holds neither of those numbers, and only the user has one.

Two smaller gaps worth writing down rather than leaving implied:
- **`accepted = true` is covered; a job that is accepted AND ticking is not.** T3's record A is accepted, and is deliberately parked where the manager's tick loop provably cannot advance it (unknown owner → `OVT_WaitTillPlayerInRangeJobStage.c:10-11` returns true). A job actively progressing through stages across a save is play-test territory (U3/U4).
- **The four `.st` items and the wiki are untouched by this phase** — Phases 5 and 6 own them.

#### 3.8 — DECISION GATE: **Phase 4 is CLEARED to proceed**

Both conditions the gate names are met, and neither is a judgement call:

- **3.4 PASSED.** The v1 fixture loaded through the real continue path; exactly three distinct retired jobs were dropped, each with a WARNING naming it; both survivors came back on their correct configs with their state intact; the counters came back on the survivors only. Verbatim log lines are quoted above. **3.5 PASSED** — the re-run was line-for-line identical.
- **3.6 PASSED.** `tools/compile-check.sh` exit 0; Fast `{6A6E29FF47ECB840}` exit 0 with **54** tests; All `{6A6E2A002F53A581}` exit 0 with **92** tests. Delta +1 / +2 for this phase, +2 / +3 for the feature, re-derived and measured rather than assumed.
- **Q2 satisfied:** all three new assertions across Phases 1 and 3 are proven able to fail, with exact failure text, method and date recorded in the table above. `grep -rn "maxAttempts" Scripts/Game/Tests/` returns only two comments saying there are none.

**Phase 4 may begin.** It should still re-check `git status`, `git log` and the highest `BUG-` id at its own front (R7), and it inherits one live tripwire: `RETIRED_IDS_ARE_DELETED` (`OVT_TEST_InitSuite.c:3164`) goes red the moment the five configs are deleted, which is task 4.5's job to flip and is *designed* to make a quiet deletion impossible.

### 2026-08-09 — Phase 4 complete (delete)

All eight tasks done. **This is the phase the whole plan was sequenced around**, and it went exactly as Phases 0-3 predicted: no count moved, no test changed shape, and the only surprises were two the tripwire and the log surfaced for free.

**Preconditions re-checked at the phase front (R7):** `git log -1` still `f6681d00`, working tree carrying only this feature's own Phase 0-3 changes plus its three doc files. Critically, `Prefabs/GameMode/OVT_OverthrowGameMode.et` — one of the two busiest shared files in the repo — had **no** uncommitted modification from a parallel session, so 4.1 stepped on nothing. Baseline re-measured: compile-check exit 0, Fast **54**. (The All re-measurement was contaminated — see below — and was re-taken clean afterwards.)

**What was deleted, exactly:**

| What | Where | Result |
|---|---|---|
| 5 `OVT_JobConfig` entries | `Prefabs/GameMode/OVT_OverthrowGameMode.et` | `m_aJobConfigs` block now `:25-40` (closing brace at `:40`), **7 entries** |
| the whole `m_aJobConfigs` block | `Worlds/MP/OVT_Campaign_Eden_Layers/managers.layer` | `OVT_JobManagerComponent "{59A8D178EC1AA2AE}"` left as an **empty override** — the same shape `OVT_PersistenceManagerComponent` already has in that file (`managers.layer:145-146`), so this is the file's own idiom, not a new one |
| 5 `.conf` + 5 `.conf.meta` | `Configs/Jobs/` | 10 files, removed with `rm`; 7 `.conf` remain |

**4.4 — the grep is clean, and the GUIDs were captured before the metas went.** The five resource GUIDs, read from the `.conf.meta` files immediately before deletion: `{A66B34BC59667AE6}` findGunDealer, `{11EE393C09B6A661}` findShop, `{C791AAAF88555841}` placeEquipmentBox, `{696B3B013BFD45FF}` recruitACivilian, `{56139D1CED432410}` placeACamp. A repo-wide grep (excluding `.git/`, `.tmp/`, `.saves/`) for all five **names** returns exactly **one** non-`docs/` hit — `OVT_GetRadioTowerLocationJobStage.c:5`, which is task 4.6's own comment and was reworded in the same pass. The five **resource GUIDs** return **zero** non-`docs/` hits, and so do the five prefab **element** GUIDs (`{5D9C33D6746B603D}`, `{5D9C33D122545AFD}`, `{65CD0E98C7F3EDD1}`, `{65CD1C80C1CC8883}`, `{65CD1C846520EBD8}`). F4 satisfied.

**R4's `jobIndex` grep now names SEVEN files, not Phase 2's six — and the seventh is expected.** The four legitimate in-session users (`OVT_JobsContext.c`, `OVT_PlayerCommsComponent.c`, `OVT_JobManagerComponent.c`, plus a comment in `OVT_TutorialComponent.c`), the serializer, `OVT_TEST_InitSuite.c` (a comment from Phase 1's T2 header) and now `OVT_TEST_PersistenceRoundTripSuite.c` — which is **Phase 3's own T3**, added after Phase 2 wrote that list. No new positional dependency was introduced by this phase.

**4.5 — the tripwire fired for real, and it was not a drill.** The phase-front All-group re-measurement was still running when the five configs were deleted, and it came back `run-tests: FAILED (1 of 92)` naming exactly one case: `OVT_TEST_Init_Jobs_StableIdsAreUniqueAndResolve`. That is an **accidental but genuine live demonstration** of what Phase 1 built the switch for — a deletion without the flip is loud, immediate and self-describing. The flip then went green in the same tree. Both directions of the switch have now been observed failing on this tree, which is more than Q2 asks of it.

The constant's comment was rewritten from a *pending instruction* into a *completed record*: it states the date, that the flip happened in the same change as the deletion, that both mis-states were observed red, and — the part a future reader actually needs — that it is now a **permanent regression guard** rather than a spent one, because re-adding a config carrying one of the five ids would make a version 1 save's dropped records start resolving again onto jobs they were never saved on. `OVT_TEST_InitSuite.c:3170`; the case header at `:3145-3146` and the `RETIRED_LEGACY_IDS` comment at `:3183` were moved to past tense to match.

**4.6 — the comment cites the class, and says why the class is still there.** `OVT_GetRadioTowerLocationJobStage.c:4-6` now reads *"the same shape as OVT_GetDealerLocationJobStage. (That class survives and is still config-composable, but no shipped job uses it since findGunDealer was retired on 2026-08-09.)"* — which discharges the reword and records D8's decision at the one site a reader would otherwise trip over it. **No class was deleted** (D8): all six orphans are untouched.

**4.7 — Eden loads with 7, measured.** Method identical to Phase 0's F3 probe: a temporary `Print` in `OVT_JobManagerComponent.Init()`, compile-checked, then `tools/launch-server.sh --scenario eden --timeout 150 --quiet` headless — **no client was launched, no window on the user's desktop**. From `<OverthrowDS>/logs/logs_2026-08-09_22-30-12/console.log`, with the reconciled layer proven loaded at `:149` and the mission confirmed Eden at `:142`:

```
22:30:16.113 DEFAULT      : [SaveGameManager] Starting new playthrough nr.0 '' for mission '{3DAD390C31623F04}Missions/24_OVT_Eden.conf'.
22:30:17.275  WORLD        : Entity layer load @"$Overthrow:Worlds/MP/OVT_Campaign_Eden_Layers/managers.layer"
22:30:32.337      SCRIPT       : [OVT-P4PROBE] m_aJobConfigs.Count() = 7
22:30:32.337      SCRIPT       : [OVT-P4PROBE] 0 = assassinate-traitor / #OVT-Job_AssassinateTraitor
22:30:32.337      SCRIPT       : [OVT-P4PROBE] 1 = base-recon / #OVT-Job_BaseRecon
22:30:32.337      SCRIPT       : [OVT-P4PROBE] 2 = raise-support / #OVT-Job_RaiseSupport
22:30:32.337      SCRIPT       : [OVT-P4PROBE] 3 = propaganda-run / #OVT-Job_PropagandaRun
22:30:32.337      SCRIPT       : [OVT-P4PROBE] 4 = pirate-radio / #OVT-Job_PirateRadio
22:30:32.337      SCRIPT       : [OVT-P4PROBE] 5 = sabotage-radio-tower / #OVT-Job_SabotageRadioTower
22:30:32.337      SCRIPT       : [OVT-P4PROBE] 6 = assassinate-officer / #OVT-Job_AssassinateOfficer
```

**No missing-resource error, and that is a checked claim rather than an absence noticed in passing.** A grep of the whole Eden log for all five conf names and all five GUIDs returns **nothing**. The log's `(E)` lines are all pre-existing engine/asset noise (`TransparentMat.emat`, `Hierarchy` component, `PATHFINDING` tiles). The only two errors emitted *by the layer load itself* — `Unknown keyword/data 'm_aCameraPositions'` and `'m_rDefaultPrefab'` — are **byte-for-byte present in Phase 0's Eden log at the same line numbers** (`logs_2026-08-09_20-49-23/console.log:150-151`), so they predate this change. Their reported offsets moved from `1621`/`6682` to `2145`/`7206`: a shift of exactly **524 bytes in both**, which is the size of the deleted block — incidental corroboration that the layer parsed exactly as intended and nothing else in it moved.

**The probe was reverted and the revert is proven:** `grep -rn "P4PROBE" Scripts/` returns nothing, the only `Print` additions in `git diff` are Phase 2's three drop WARNINGs, and `compile-check` is clean. Eden's `--timeout 150` is well under the 600 s autosave interval, so the run wrote **no** save point: the DS save tree still holds only `6B0E7A50D1E2F3A4-25-OVT-TestWorld/playthrough000/savepoint000-006`, i.e. Phase 3's staged `savepoint007` is confirmed cleaned up and no Eden campaign dir was created.

**4.8 — green gate, and the counts did not move.** compile-check exit 0; Fast `{6A6E29FF47ECB840}` exit 0 with **54**; All `{6A6E2A002F53A581}` exit 0 with **92**. Delta **0 / 0**, exactly as expected: 4.5 changes what an existing case *asserts*, not how many cases there are. Both groups were re-run on the **final** tree, after the Eden probe was reverted, so the gate stands on precisely what ships. T2's own success line confirms the new world from inside the harness:

```
Job stable ids: 7 configs, all non-empty, lowercase-kebab, unique and index<->id round-tripping; 7 surviving legacy ids resolve; retired-ids-deleted switch is true
```

**I3 holds: `git diff --stat Configs/Tutorials/ Configs/FieldManual/` is empty.** ⚠️ **But the expected tutorial-entry count in the plan and the task brief is stale.** Both say the Init log should report **eleven** entries; this tree reports **twelve**, from twelve committed `Configs/Tutorials/*.conf` files with **zero** diff (`[Overthrow.Tutorial] Loaded 12 tutorial entries`; `Tutorial manager is live with 12 structurally valid entries`). Nothing in this phase touched them — the twelfth arrived from a parallel session before this feature started, the same drift R7 predicts and the same drift that made the plan's 51/88 baseline wrong. **The real I3 requirement — a zero diff — is met.** The "eleven" figure should not be carried into Phase 5 or 6.

**Nothing unexpected in the deletion itself.** No file outside the ten referenced the five configs; the `.conf` parsing took the removal as cleanly as Phase 1's insertion; the Eden layer's empty component override parses without complaint.

### 2026-08-09 — Phase 5 complete (records, bugs, epic close)

All seven tasks done. **Zero product code.** The whole diff for this phase is `Language/localization_Overthrow.st` (Comment fields only), two bug files, three epic docs and this file.

**Preconditions re-checked at the phase front (R7):** `git log -1` still `f6681d00`; the working tree carries only this feature's own Phase 0-4 changes plus its three doc files; **highest `BUG-` id still `BUG-133`**, so the next free id remains **BUG-134**. No parallel session landed anything during the feature.

**5.1 — the ten items are retired in place, and the mechanics were not uniform.** Four of the ten (`OVT-Job_FindGunDealer`, `OVT-Job_FindGunDealerDescription`, `OVT-Job_FindShop`, `OVT-Job_FindShopDescription`) carried an empty `Comment ""` that was replaced. The other six (`OVT-Job_PlaceCamp`, `OVT-Job_PlaceCamp_Description`, `OVT-Job_PlaceEquipmentBox`, `OVT-Job_PlaceEquipmentBox_Description`, `OVT-Job_RecruitACivilian`, `OVT-Job_RecruitACivilian_Description`) are **short-form items with no `Comment` key at all** — they carry only `Id`, their `Target_*` lines, `Modified`, `Author` and `LastChanged` — so a `Comment` line had to be **inserted**, immediately before `Modified`, which is where the field sits in the file's full-form items. That is why the diff reads `12 insertions(+), 6 deletions(-)` rather than ten replacements. **The plan's ten line numbers (`3823, 3845, 3879, 3900, 3987, 3995, 4023, 4031, 4157, 4165`) were still exact**, but they were re-located by id anyway, which is what made the two shapes visible.

Each record names the date, the feature, the removed job, its deleted `.conf`, the tutorial entry that replaces it, the reason for retirement, and D9's reason for retention. Each also states explicitly that the removal **does not close BUG-037 or BUG-040** — the `.st` is the one place a future reader meets these strings without any surrounding documentation.

**5.2 — both forward-pointing Comments were false in different ways, and only one of them was expected.**
- `OVT-IntroHint`'s claim 3 (`.st`, the `Comment` on item `{59969DFBF96C7EF4}`) said the Jobs-section tutorials "is removed by the starter-jobs-retirement feature" — now past tense, dated, and narrowed to the accurate statement: the five tutorial jobs are gone and the seven survivors are ordinary gameplay jobs.
- The welcome page-3 body (`OVT-Tutorial_WelcomeIntro_Body3`, item `{6B3D000000000104}`) said *"The menu's Jobs entry is deliberately NOT named: the starter-jobs-retirement feature changes it."* **That was the false one.** It is corrected to say the Jobs entry **survives**: this feature removed five job *configs*, not the menu entry. Verified rather than assumed — `UI/Layouts/Menu/MainMenu.layout:284, :292` (the `Jobs` button and its `#OVT-MainMenu_Jobs` label) is byte-unchanged and `git diff --stat UI/` is empty. **I6 and D12 are therefore discharged by inspection: `welcome-intro-3-ui.edds` remains true and was not re-shot.** The omission of Jobs from the page-3 text is now purely one of length, on the same footing as Place, Build, Options and Save.

**Q3 verified as a measurement, not a claim.** `git diff --stat Language/` lists `localization_Overthrow.st` **and nothing else**; `git diff -U0 Language/` contains **zero** changed lines that are not `   Comment ` lines, so every `Target_*` is byte-identical and all ten items are still present. No `.<lang>.conf` was opened at any point.

**5.3 — the bug files stay `closed`, and say why loudly.** Both now carry a dated note that the five jobs were **subsequently removed** by this feature and that the removal **closed neither bug** — they were fixed in place on 2026-08-03 and were already closed when this feature was planned. The surviving fixes were re-verified on the post-removal tree rather than copied from the plan, and their line numbers had drifted: the player-allocated gate is now `OVT_JobManagerComponent.c:638-639` (the plan and this file both said `:543`, correct before Phases 2 and 4), the listen-host completion send is `:565-569`, and the owner filter in the receiver is `:1079-1086` with the only surviving `ShowCustom` at `:1091`, inside the client-side RPC. Both notes also record the consequence that **no shipped config is player-allocated any more**, so both fixes' branches are currently exercised only by tests.

**The rewards figure was corrected at every site this phase wrote, and the remaining false copies are named.** BUG-037's note states `$100, 30 XP and two field dressings`, with the derivation cited (`m_iRewardXP` defaults to **5** at `OVT_JobConfig.c:37-38` and is paid whenever above zero at `OVT_JobManagerComponent.c:535-537`; only `recruitACivilian.conf` declared 10). The wrong **total** ("$100 and 10 XP") still stands at `tutorial-content/context.md:90`, `starter-jobs-retirement/requirements.md:86` and `docs/overview.md:22, :37` — none of which this feature was scoped to edit. The correction and those four locations are flagged in `epic-overview.md` so the next writer cannot inherit the figure by accident.

**5.4 — every site listed, plus three the list did not name.** In `epic-overview.md`: the **Purpose** paragraph (with an explicit correction box, because a silent rewrite loses the fact that the epic's own premise was stale), the **#5 features row** (status `🟡 Built`, 47/52, and "closes BUG-037 by removal" replaced with what actually happened), the **Build Order** line for #5, and the **dependency bullet** that asserted "BUG-037/BUG-040 are discharged by starter-jobs-retirement". Not on the list but corrected in the same file because they were false or stale on the same subject: the `tutorial-content` handover bullet carrying the wrong reward figure, the `first-spawn` handover bullet still saying `welcome-intro-3-ui.edds` "must be re-shot" (twice — Integration and Rollup), and the header/rollup status still reading 4/5 built with the pre-feature Fast 51 / All 88 counts. In `epic-requirements.md`: the last Requirements bullet, plus the Overview sentence calling the jobs "multiplayer-broken" and the Planned Features line saying the same — with the dates made explicit, since BUG-037 was fixed on **2026-08-03**, one day *before* `epic-requirements.md` was created, so that phrase was **stale the day it was written**.

**5.5 — the jobs epic now records both halves.** The positional-`jobIndex` tech-debt item is ticked **DISCHARGED**, naming what replaced it (`m_sId`, the version 2 format, the frozen `LEGACY_V1_JOB_IDS` table at `OVT_JobManagerSerializer.c:194-207`, and the two save-boundary translation points) and recording that the item's own suggested fix — the config resource name — was **rejected** by D1 because it couples saves to a file path. The Integration line describing `jobIndex` as "append-only, it is persisted" was corrected in the same pass: it is still the in-session and wire handle, but it is no longer what gets persisted.

**The six orphaned-but-kept classes were re-verified before being written down**, not transcribed from §3.8: a grep over `Configs/`, `Prefabs/`, `Worlds/` and `Scripts/` returns, for each of `OVT_GetShopLocationJobStage`, `OVT_GetDealerLocationJobStage`, `OVT_HasRecruitJobStage`, `OVT_IsNearestTownWithShopJobCondition`, `OVT_IsNearestTownWithDealerJobCondition` and `OVT_IsNearestJobCondition`, **only its own definition** — plus two prose mentions that are not references (`OVT_GetRadioTowerLocationJobStage.c:5`'s reworded comment and a `OVT_JobManagerSerializer.c:128` comment about which stages override `OnTick`). All six still exist; none is referenced by any shipped `.conf`. Recorded with D8's rationale and with the consequence a future reader actually needs: **they are unexercised by shipped content**, so a change to one of them is untested by any live job.

**Two jobs-epic debt items that were already resolved were also ticked**, because leaving them open would have left the same false framing in a second epic: "Global caps break the tutorial chain in MP" (BUG-037, fixed in place) and "MP distribution book-keeping is the weak layer" (BUG-038/039/040, all three `status: closed`). Both ticks carry the caveat that **closed is not observed** — two-client behaviour has still never been play-tested.

**5.6 / 5.7 — this file now carries the whole record for a stranger:** the F3 answer with its verbatim log, the five-row proven-red table, the corrected fixture recipe and why the plan's could never have worked, the v1 support policy with its removal trigger, the two accepted residual gaps, and the string-export position. The last of those is the short one: **no re-export is required by this feature**, and if the user runs one anyway `first-spawn`'s owed `OVT-FieldManual_Welcome_Text2` lands in the same pass.

**Gates: compile-check exit 0, Fast `{6A6E29FF47ECB840}` exit 0 with 54 tests.** The All group was not re-run: this phase touched no `.c` file, and `tasks.md` gates doc/string-only phases on Fast. **The feature's final counts stand at compile 0 / Fast 54 / All 92, a delta of +2 / +3 against the 52 / 89 baseline.**

**One thing left undone deliberately.** `docs/overview.md` (the master) still describes this epic as 4/5 built, still carries the "only ever reach the first player on a server (BUG-037)" framing and still carries the wrong reward total in two places. It is the master rollup, owned by `/update-master`, and rewriting it from inside a feature phase would collide with that command rather than help it. **Flagged, not fixed** — and named here so it is not mistaken for an oversight.

---

## 2026-08-09 — Phase 6 complete (help and documentation sync)

**Preconditions re-checked at the phase front (R7):** `git log -1` still `f6681d00`; working tree carries only this feature's Phases 0-5 plus its three doc files. No parallel session landed anything.

**6.1 — no in-game help edit was needed, and that is the recorded result, not a skipped task.** `grep -ril "job" Configs/Tutorials/ Configs/FieldManual/` returns **nothing** (exit 1, zero files). Neither surface has ever named a job, so the removal of the five starter jobs leaves the twelve tutorial entries and every Field Manual piece factually intact. **DoD I3's second half is measured:** `git diff --stat Configs/Tutorials/ Configs/FieldManual/` is **empty** — those are `tutorial-content`'s and `field-manual`'s files and this feature has zero diff against them. No `.st` item was touched in this phase either, so **Phase 5's string-export position is unchanged: no Workbench re-export is required by this feature.**

**6.2 — the wiki page resolved to the id the handoff predicted, which is not something to assume.** `wikijs_get_page(slug: "getting-started")` returned `pageId 2` with `path: "getting-started"` — matching the handoff's "pageId 2". The path was confirmed **before** writing, per the recorded hazard that search returns wrong pageIds. Two removals, both exact:

1. Under `### 1. Jobs System`: the paragraph `**Tutorial Jobs**: Five starter jobs introduce the basics - finding a gun dealer, finding a shop, placing an equipment box, recruiting a civilian, and placing a camp.`
2. Item **6** of the list under `## Systems Worth Knowing About`: `**Tutorial Jobs**: A set of starter jobs that introduce these features`

`### 1. Jobs System` **stays**, as instructed, and still reads correctly: its `How it works` line, Pros and Cons describe the jobs system generically and are true of the seven survivors. Removing item 6 left items 1-5 contiguous, so no renumbering was needed. **Nothing else on the page was altered** — the update had to send the whole document, so the rest was reproduced verbatim from the read.

**6.5 — verified on the RENDERED page over HTTP, not on the stored source.** `curl https://wiki.armaoverthrow.com/getting-started` after the write: `grep -ic "tutorial job"` = **0** and `grep -ic "starter job"` = **0**, while `1. Jobs System` is still present. All eighteen landmark headings and closing lines (`Your Starting Assets` … `Good luck, revolutionary`) each appear exactly once, and the rendered document is 33,885 bytes, so the full-document write did not truncate or duplicate anything. **The MCP behaved this session:** `wikijs_connection_status` reported `healthy`/authenticated, `pages.single` worked by slug, and `wikijs_update_page` returned `status: updated` honestly — none of the three recorded hazards (wrong pageId from search, `succeeded: false` after writing, stale render) reproduced. The MCP wrapper takes no `tags` argument and the write still landed and rendered.

**6.3 — `v1_3` is untouched** (`pageId 32`, `lastModified 2025-07-07T09:33:37.392Z`, read-only). Its "**3 new tutorial jobs**" is **historically accurate**: the three added *in v1.3* were `placeEquipmentBox`, `recruitACivilian` and `placeACamp`; `findGunDealer` and `findShop` predate it. A release note records history and is not corrected by a later removal.

**6.4 — `field-manual`'s two pages are unchanged and still right.** `wanted-system` (`pageId 24`, `lastModified 2026-08-08T08:18:05.688Z`) and `base` (`pageId 11`, `lastModified 2026-08-08T08:39:56.138Z`) were read only; both timestamps are yesterday's `field-manual` pass, so this feature caused no regression there.

**The opportunistic `tutorial-content` edit was already applied and needed nothing.** `getting-started` already carries the `**Overthrow Tips**:` paragraph under `## Essential Controls`, and it survived this write byte-for-byte. `tutorial-content/context.md:26, :218` records that it landed once the MCP's lost auth was diagnosed — so there was no owed wiki edit left on this page.

**Three false figures were kept off the wiki deliberately** (Q7). Nothing written this phase asserts the "10 XP" reward total (the true figure is **$100, 30 XP and two field dressings**), the "eleven tutorial entries" count (there are **twelve**), or `tutorial-content/implementation.md` §3.5's struck claim that gun dealers lack a map icon (they have a dedicated `"gundealer"` sprite, `OVT_MapIcons.TryCreateGunDealerIcon:111-134`). No sentence added to the wiki claims a bug closure.

**Gate:** this phase touched no `.c`, no `.conf` and no `.st` file — its only repository writes are `tasks.md` and this file — so no test group was re-run. The feature's final counts stand at **compile 0 / Fast 54 / All 92**.
