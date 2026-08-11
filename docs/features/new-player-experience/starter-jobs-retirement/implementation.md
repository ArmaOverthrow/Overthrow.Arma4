# Starter Jobs Retirement — Implementation Plan

**Status:** ✅ **COMPLETE** (52/52 tasks · all 7 phases · compile-check 0, Fast **54**, All **92**, delta +2/+3 · **play-test passed and signed off 2026-08-09, all eleven checks U1-U6 and M1-M5**). **U1 — Continue a pre-migration campaign save — passed on a real campaign**, which is the observation the whole phase order existed to make possible. **M1-M5 passed too**, giving the epic its first two-client observation on the jobs surface (see `context.md` for what that does and does not establish for `tutorial-system`'s F7).
**Started:** 2026-08-09
**Target Completion:** TBD
**Last Updated:** 2026-08-09

**Epic:** new-player-experience (feature **#5 of 5 — the last**)
**Requirements:** `docs/features/new-player-experience/starter-jobs-retirement/requirements.md`
**Contracts consumed:** `tutorial-content/context.md` §"Starter-job coverage mapping: AS BUILT" (the precondition; read that, **not** `tutorial-content/implementation.md` §3.5) · `field-manual/implementation.md` D12 (the wiki handoff) · `first-spawn/implementation.md` D11 (the string-retention precedent) · `OVT_JobManagerSerializer.c` file header (the no-replay restore invariant, which this feature must not weaken)

**Measured baseline for this tree, 2026-08-09:** `tools/compile-check.sh` clean · Fast `{6A6E29FF47ECB840}` ~~**exit 0, 51 tests**~~ **exit 0, 52 tests** · All `{6A6E2A002F53A581}` ~~**exit 0, 88 tests**~~ **exit 0, 89 tests** (**re-measured in Phase 0.2 on 2026-08-09 against commit `f6681d00`** — one Init case was added by a parallel session between planning and Phase 0, and both groups share `OVT_TEST_InitSuite`). Older docs saying 38/66 or 47/78 are stale. Re-derive before adding cases anyway — parallel sessions commit into this tree.

---

## 1. Executive Summary

Five starter jobs — `findGunDealer`, `findShop`, `placeEquipmentBox`, `recruitACivilian`, `placeACamp` — leave the shipped build. The ten tutorial entries that `tutorial-content` shipped now teach everything they taught, and a job is the one shape this epic exists to replace: it assigns a directed goal and drops a marker on one named instance.

**The stated reason for this feature is stale and the plan says so up front.** BUG-037 and BUG-040 were **already fixed in place on 2026-08-03** and are both `status: closed`. `OVT_JobManagerComponent.c:543` now reads `bool playerAllocated = !config.m_bBaseOnly && !config.m_bPublic;` and skips the global `m_iMaxTimes` gate for player-allocated configs; the completion path no longer calls `SCR_HintManagerComponent` server-side and its RPC filters by owner with a listen-host direct call. **Retirement discharges neither bug.** What is left is a *redundancy* decision against ten shipped tutorial entries, taken by the user with that correction in hand. The bug files get an accurate note ("obsoleted by removal"), not a false "closed by this feature", and the epic docs that still carry the old framing get corrected.

**The feature is not really about deleting five files. It is about the int that names them.**

`OVT_Job.jobIndex` is a **position** in `m_aJobConfigs` and it is persisted. The five configs are interleaved — positions 2, 4, 5, 6, 7 of twelve — so deleting them shifts every later job down, and `FindRestorableJobConfig()` only rejects *out-of-range* records. A saved job would come back attached to a **different** job, at a stage index that means something else, and would pay out that other job's reward. The lifetime counters are keyed by the same int and would cap the wrong jobs. No error, no log line, no crash.

So the order is: **give every job a stable id, move the save format onto it, prove it, and only then delete.** That is the sequence the whole plan is built around, and it also discharges a standing jobs-epic tech-debt item ("Positional `jobIndex` couples saves to prefab list order — a stable identity would remove the trap") and honours what a sibling already wrote into shipped code at `OVT_TutorialComponent.c:174-177`.

Scope discipline: no jobs-system rework, no replacement jobs, no new tutorial entry, no deletion of the now-orphaned framework classes. The user has declined all four.

---

## 2. Goals

### Primary

1. **No save corruption, ever.** A campaign save taken before this feature restores every surviving job to the *correct* config, and drops exactly the records naming the five removed jobs — each with a WARNING line naming the job.
2. **Stable job identity in the save format.** `OVT_JobConfig` gains `m_sId`; the serializer writes ids, not positions, for the board and both counter maps. Reordering or trimming `m_aJobConfigs` stops being a save-breaking act.
3. **The five configs and their prefab registrations are gone**, along with their `$100 / 10 XP / two field dressings`.
4. **The stale Eden world-layer job list is reconciled**, and if it turns out to have been *replacing* the game-mode list, that shipped bug is filed.
5. **Nothing else regresses:** the Jobs menu lists the seven survivors, occupancy sets and lifetime caps still gate, JIP and every RPC behave exactly as before.
6. **The wiki stops describing jobs that no longer exist**, and the two stringtable comments that point forward to this feature are corrected.

### Secondary

7. **Job persistence stops being uncovered.** Today it is 4 pure-condition Logic cases and one `GetJobs()` resolve assert. The manager's restore path, the serializer and the id mapping get real assertions, each proven able to fail.
8. **The orphaned framework classes are recorded, not deleted** (D8), so a modder authoring against `jobs/core`'s guide still finds them.
9. **The epic closes cleanly** — overview, requirements, bug files and rollup all telling the same true story.

### Explicitly out of scope

- **Fixing or reworking the jobs system.** Per-player caps, reactive UI refresh, stage titles, routing completions through `OVT_NotificationManagerComponent`, the `m_vCurrentWaypoint` shared scratchpad — all jobs-epic debt, none of it touched.
- **Migrating `RpcAsk_AcceptJob` / `RpcAsk_DeclineJob` off `OVT_PlayerCommsComponent`.** They are legacy and the project forbids *new* client→server RPCs there; moving the existing two is a jobs-epic job, not this one. **Left alone deliberately.**
- **Converting the in-session/wire handle to a string.** §3.3 draws the boundary and defends it.
- **Deleting the six orphaned stage/condition classes** (D8).
- **A new tutorial entry for either residual gap** (directed discovery, recruit availability). User decision: accept both.
- **Re-shooting `welcome-intro-3-ui.edds`.** See D12 — the Jobs *menu entry* survives, so `first-spawn`'s handover is discharged by inspection, not by work.

---

## 3. Architecture Overview

### 3.1 The problem, as a table

`Prefabs/GameMode/OVT_OverthrowGameMode.et:25-47`, `OVT_JobManagerComponent.m_aJobConfigs`. `*` = removed by this feature.

| v1 index | config | index after a naive delete | what a saved v1 record would become |
|---:|---|---:|---|
| 0 | `assassinateTraitor` | 0 | correct |
| 1 | `baseRecon` | 1 | correct |
| 2 | `findGunDealer` `*` | — | **→ `raiseSupport`** |
| 3 | `raiseSupport` | 2 | **→ `propagandaRun`** |
| 4 | `findShop` `*` | — | **→ `pirateRadio`** |
| 5 | `placeEquipmentBox` `*` | — | **→ `sabotageRadioTower`** |
| 6 | `recruitACivilian` `*` | — | **→ `assassinateOfficer`** |
| 7 | `placeACamp` `*` | — | out of range → dropped |
| 8 | `propagandaRun` | 3 | out of range → dropped |
| 9 | `pirateRadio` | 4 | out of range → dropped |
| 10 | `sabotageRadioTower` | 5 | out of range → dropped |
| 11 | `assassinateOfficer` | 6 | out of range → dropped |

**Two of twelve indices survive a naive delete.** Five resolve to the wrong job and five are dropped. The five wrong resolutions are the dangerous half:

- `FindRestorableJobConfig()` (`OVT_JobManagerComponent.c:304-339`) checks range and stage-count only. A `placeACamp` record at stage 1 becomes an `assassinateOfficer` at stage 1 — a perfectly valid stage index — and is accepted.
- `CheckUpdate()` then ticks it and, on completion, pays `config.m_iReward`: **$750 instead of $0**.
- `m_aJobCounts` and `m_mPlayerJobCounts` are keyed by the same int. `m_aJobCounts[5] = 1` (a spent `placeEquipmentBox` cap) becomes a spent `sabotageRadioTower` cap.

This is silent, permanent and player-visible only as "jobs behaving oddly on a continued campaign".

### 3.2 The stable id

`OVT_JobConfig` (`Scripts/Game/Configuration/OVT_JobConfig.c`) gains one attribute:

```
[Attribute(desc: "Stable identity used by the save format. NEVER change a shipped id.")]
string m_sId;
```

Authored into all twelve `Configs/Jobs/*.conf` as short lowercase kebab-case, matching the tutorial-entry id convention (`economy-first-buy`):

| conf | id | fate |
|---|---|---|
| `assassinateTraitor.conf` | `assassinate-traitor` | survives |
| `baseRecon.conf` | `base-recon` | survives |
| `raiseSupport.conf` | `raise-support` | survives |
| `propagandaRun.conf` | `propaganda-run` | survives |
| `pirateRadio.conf` | `pirate-radio` | survives |
| `sabotageRadioTower.conf` | `sabotage-radio-tower` | survives |
| `assassinateOfficer.conf` | `assassinate-officer` | survives |
| `findGunDealer.conf` | `find-gun-dealer` | **retired** |
| `findShop.conf` | `find-shop` | **retired** |
| `placeEquipmentBox.conf` | `place-equipment-box` | **retired** |
| `recruitACivilian.conf` | `recruit-a-civilian` | **retired** |
| `placeACamp.conf` | `place-a-camp` | **retired** |

The five retired ids are authored in Phase 1 **and then deleted with their files in Phase 4**. They live on only inside the frozen legacy table (§3.5), which is what lets a v1 drop name the job it dropped.

Two public helpers on the manager, both linear scans over twelve entries on a load-only path (a cached map is YAGNI and would need invalidation):

- `int FindJobIndexById(string jobId)` → `-1` when unknown
- `string GetJobIdByIndex(int index)` → `""` when out of range

### 3.3 Where the id/int boundary sits

**Persisted = stable string id. In-session and on the wire = positional int.** Nothing else changes.

| Surface | Identity | Why |
|---|---|---|
| `OVT_PersistedJob.jobId` | **string id** | outlives the config list |
| global counter keys in the payload | **string id** | same |
| `OVT_PersistedPlayerJobCounts.jobIds` | **string id** | same |
| `OVT_JobConfig` position, `GetConfig(int)` | int | the list is the session's own truth |
| `OVT_Job.jobIndex` | int | in-memory handle |
| `m_aJobCounts`, `m_mPlayerJobCounts` (live maps) | int | translated at the save boundary only |
| `m_aGlobalJobs`, `m_aTownJobs`, `m_aBaseJobs` | int | derived on restore; never persisted |
| `RplSave` / `RplLoad` (JIP) | int | server and client share one config list within a session |
| `RpcDo_UpdateJob`, `RpcDo_RemoveJob`, `RpcDo_DeclineJob`, `RpcDo_NotifyJobCompleted` | int | same |
| `OVT_PlayerCommsComponent.RpcAsk_AcceptJob` / `_DeclineJob` | int | legacy, out of scope |

**There are exactly two translation points, both server-side, both on the save/load path:**

1. `OVT_JobManagerSerializer.Serialize()` — index → id.
2. `OVT_JobManagerComponent.ApplyPersistedJobs()` — id → index, with the drop policy.

**Consequence worth stating loudly: no RPC signature changes and no `RplSave`/`RplLoad` field changes.** Project memory records that `Rpc()` arity is an untyped variadic proto and a wrong argument count compiles clean and dies silently at the wire (BUG-090). This design gives that hazard no surface at all — and Phase 2's acceptance criteria include a `git diff` proving zero change to any `[RplRpc]` signature or to `RplSave`/`RplLoad`.

### 3.4 The v1 → v2 migration

`OVT_JobManagerSerializer` writes `version 2`. `Deserialize` branches:

```
version < 1   -> no payload; return true (unchanged guard, and load-bearing:
                 without it an absent payload wipes the board and every counter)
version == 1  -> read into the FROZEN legacy record classes, convert by §3.5, apply
version >= 2  -> read the id-keyed records, apply
```

> **⚠️ CORRECTED BY MEASUREMENT 2026-08-09 (Phase 2): the names below are WRONG and would have wiped live job boards.** The binary container writes the concrete class name into the payload as a `$type` discriminator and **instantiates from it on load** (`SCR_PersistenceSerializationContext.c:34-43` enables it; it is visible at offset `0x28c` of the captured v1 fixture). Reading a version 1 payload into a renamed-but-identical class was measured: the read returns **false**, the array comes back **empty but non-null**, and every following property in the stream fails too — so `ApplyPersistedJobs()` would have applied an empty board and empty counter maps to a live campaign, silently. **The freeze must land on the classes the payload NAMES.** As built: `OVT_PersistedJob` and `OVT_PersistedPlayerJobCounts` are the frozen version 1 records; `OVT_PersistedJobV2` and `OVT_PersistedPlayerJobCountsV2` are the current ones. D4's reasoning is unchanged and correct — only the name assignment was wrong. Evidence and probe output: `context.md` §"The `$type` discovery".
>
> **A second correction in the same paragraph below:** the note that the matching-local-name rule is "belt-and-braces" is **false on this build**. It was measured too — writing `jobRecords` and reading the identical payload into a local named `readJobs` returns false and reads nothing. It is load-bearing.

**Read the v1 payload into separate frozen classes, not into the new ones.** ~~`OVT_PersistedJobV1` and `OVT_PersistedPlayerJobCountsV1`~~ **`OVT_PersistedJob` and `OVT_PersistedPlayerJobCounts` (see the correction above)** are byte-for-byte copies of today's member layout and are never edited again. The alternative — appending a trailing field and clearing it on v1, which `OVT_RecruitManagerSerializer.c:180-215` did successfully for `bodyPersistenceId` — works for one appended scalar but is being asked here to *replace* a field's meaning inside an array of records. A frozen shadow class is provably correct under every container semantic for the cost of one small class.

⚠️ **Two format facts the implementer must respect.**
- `LoadContext.Read(out void value)` derives the property name **from the local variable's name**. The v1 branch must therefore declare its locals with the **same names the v1 writer used** — `jobRecords`, `countIndices`, `countValues`, `playerCounts` — or a named (non-binary) context would look for the wrong key. Overthrow's contexts are binary and positional, so this is belt-and-braces; do it anyway and comment why.
- Binary contexts are **positional**: write order must equal read order, and the version value comes first. Unchanged from today.

The conversion is a pure function so it can be tested world-free (§7 T1):

```
static string LegacyIdForIndex(int index)   // frozen §3.5 table; "" when out of range
static bool   IsRetiredLegacyId(string id)  // the five
```

Drop messages — exact shapes, because the DoD checks for them:

| Case | Level | Message |
|---|---|---|
| v1 record naming a retired job | WARNING | `[Overthrow] Dropping saved job 'find-shop' from a version 1 save - that job was retired and no longer exists. Its lifetime counters are dropped with it` |
| v1 index outside the twelve | WARNING | `[Overthrow] Dropping a saved job with legacy index %1 - the version 1 job list had 12 entries` |
| v2 id matching no config | WARNING | `[Overthrow] Dropping saved job '%1' - no configured job carries that id` |
| a config with an empty `m_sId` at save time | ERROR | `[Overthrow] Job config '%1' (index %2) has no m_sId - its board entries and lifetime counters cannot be saved` |

The last one cannot happen for shipped content (the Init guard in Phase 1 makes it impossible) but can for a third-party config, and a loud error beats a silent loss.

**`ApplyPersistedJobs()` keeps its contract.** It stays a clear-and-rebuild of four collections, so `OVT_PersistenceManagerComponent.ReapplyLatestSaveData` re-applying the same save to a live session still produces the same board rather than a doubled one. The id→config resolve lands **inside `FindRestorableJobConfig()`**, which is already where the drop policy lives — including the `OVT_WaitTillDeadJobStage` drop, which is unaffected and must stay.

### 3.5 The frozen legacy table

Lives in `OVT_JobManagerSerializer` as a `static const ref array<string>`, with a header comment marking it immutable. It is history, not configuration.

```
0 assassinate-traitor    4 find-shop  (retired)         8  propaganda-run
1 base-recon             5 place-equipment-box (retired) 9  pirate-radio
2 find-gun-dealer (ret.) 6 recruit-a-civilian (retired)  10 sabotage-radio-tower
3 raise-support          7 place-a-camp (retired)        11 assassinate-officer
```

**The Eden question does not affect this table, and that is worth knowing before §3.6 is settled.** The Eden layer's five entries are the *first five of the prefab list, same GUIDs, same order*. So under "replace" an Eden v1 save only ever wrote indices 0-4, which map identically under the twelve-entry table; under "merge" it wrote 0-11, also covered. **One table is correct either way.**

The table also becomes a *rename guard*: the Init case in §7 asserts that all seven surviving legacy ids still resolve to a config. Rename `raise-support` and the build goes red.

### 3.6 The Eden world layer (F3)

> **✅ ANSWERED BY MEASUREMENT 2026-08-09 (Phase 0.3): the override MERGES.** A headless `tools/launch-server.sh --scenario eden` with a temporary count/title `Print` in `OVT_JobManagerComponent.Init()` reported **`m_aJobConfigs.Count() = 12`**, in the prefab's exact order, with **no duplicates** — with `Entity layer load @"$Overthrow:Worlds/MP/OVT_Campaign_Eden_Layers/managers.layer"` confirmed in the same log 121 lines earlier. **Eden has always run the full twelve jobs. There is no shipped bug, and Phase 0.4 files nothing.** D7 is unchanged: deleting the block in 4.2 is pure cleanup. Verbatim log lines and the exact method are in `context.md` §"F3 — the Eden override measurement".


`Worlds/MP/OVT_Campaign_Eden_Layers/managers.layer:28-40` overrides `OVT_JobManagerComponent.m_aJobConfigs` on the game-mode **instance** with a stale pre-v1.3 list of five entries — same GUIDs as the prefab's first five, **empty bodies**. Eden is the shipped mission (`Missions/24_OVT_Eden.conf`). No other world or layer in the repo carries a job list (verified: exactly two files mention `m_aJobConfigs`).

**Whether a same-GUID partial array override on a world entity MERGES or REPLACES is the plan's top risk (R1) — but it does not change the edit.** The block is same-GUID with empty bodies, so:

| semantics | what the block does today | what deleting it does |
|---|---|---|
| merge / patch-in-place | nothing (no member overridden) | nothing — pure cleanup |
| replace | Eden ships **7 jobs short**, including all four illegal-placeables jobs | **fixes a shipped bug** |
| naive append | Eden has 17 entries with 5 duplicates | removes the duplicates |

**Delete the whole `m_aJobConfigs` block from the layer.** Correct or neutral under every candidate semantic. The measurement (Phase 0.3) decides only *what gets filed*, not *what gets edited* — and it must still be taken, because "Eden has been shipping seven jobs short" is a significant standalone bug.

**Strong prior, already measured in this repo, do not re-derive it:** `OVT_TEST_SuiteBase.c:36-41` records that the test world's layer declares an `m_aDifficultyPresets` override and *"Enfusion **APPENDS** that array to the game mode prefab's four presets rather than replacing them: at runtime there are 5 presets, index 0 is 'Easy' and 'Test World' is index 4"* (findings.md 1.3c). That was a **different**-GUID element, which appended. Combined with the project's "same-GUID prefab/conf overrides are DELTAS" memory, the expected answer is merge-with-patch → 12 entries on Eden. **Expected is not measured.** ⚠️ And a documented trap is not evidence: `tutorial-content` shipped a false row in its own trap table into three documents. Measure it.

Note also that the layer *must* be reconciled regardless: after Phase 4 it would reference five `.conf` resources that no longer exist.

### 3.7 File inventory

```
Scripts/Game/
├── Configuration/
│   └── OVT_JobConfig.c                      + m_sId attribute
├── GameMode/Managers/
│   └── OVT_JobManagerComponent.c            + FindJobIndexById / GetJobIdByIndex
│                                            ~ ApplyPersistedJobs (id-keyed args)
│                                            ~ FindRestorableJobConfig (resolve by id)
├── Persistence/Serializers/Components/
│   └── OVT_JobManagerSerializer.c           ~ version 2, id-keyed records
│                                            + OVT_PersistedJobV1 (frozen)
│                                            + OVT_PersistedPlayerJobCountsV1 (frozen)
│                                            + LEGACY_V1_JOB_IDS, LegacyIdForIndex,
│                                              IsRetiredLegacyId
└── Tests/TestSuites/
    ├── Logic/OVT_TEST_Logic_Jobs.c          + T1 (legacy mapping, world-free)
    ├── Init/OVT_TEST_InitSuite.c            + T2 (id guard + legacy resolve)
    └── Persistence/OVT_TEST_PersistenceRoundTripSuite.c
                                             + T3 (job board round trip)

Configs/Jobs/
├── (7 surviving).conf                        + m_sId
└── findGunDealer|findShop|placeEquipmentBox|recruitACivilian|placeACamp
    .conf + .conf.meta                        DELETED (Phase 4)

Prefabs/GameMode/OVT_OverthrowGameMode.et     5 entries removed from m_aJobConfigs
Worlds/MP/OVT_Campaign_Eden_Layers/managers.layer
                                              m_aJobConfigs block deleted

Language/localization_Overthrow.st            10 items RETAINED, Comments become
                                              retirement records (D9);
                                              2 forward-pointing Comments corrected
```

**Not touched:** `Language/localization_Overthrow.<lang>.conf` (never), `OVT_JobsContext.c`, `OVT_JobListEntryHandler.c`, `OVT_MapIcons.c` (its only job dependency is `m_vCurrentWaypoint`), `OVT_PlayerCommsComponent.c`, `Configs/Systems/Persistence/Overthrow.conf` (the serializer binding at `:44` is by class name and is unchanged), any `Configs/Tutorials/` or `Configs/FieldManual/` file (verified: **zero** mentions of jobs in either).

### 3.8 Evidence pack — verified on this tree, 2026-08-09

Re-verify anything here before relying on it. Two of these rows exist *because* a previous feature's trap table was wrong.

> **RE-VERIFIED 2026-08-09 (Phase 0.1) against this tree.** Twelve of thirteen rows stand. **One row fell** (the rewards row — struck below). Three rows stand with corrected `file:line` citations, marked inline. The re-verification method was a direct read of each cited file, not a re-read of this table.

| Claim | Evidence |
|---|---|
| BUG-037 and BUG-040 are **closed**, fixed in place 2026-08-03 | **STANDS.** `docs/bugs/BUG-037.md:4`, `BUG-040.md:4` both `status: closed`, `updatedAt: 2026-08-03`. ~~`OVT_JobManagerComponent.c:540-544` and `:467-474`, `:979-998`~~ — line drift, corrected 2026-08-09: the player-allocated gate is at **`:543-544`**, the owner-filtered completion RPC send at **`:470-474`**, its receiver at **`:979-1000`** |
| BUG-037's claim that `placeACamp` omits `m_iMaxTimesPlayer` is **wrong** | **STANDS.** BUG-037 asserts it at `docs/bugs/BUG-037.md:15`; today all five configs carry `m_bPublic 0`, `m_iMaxTimes 1` and `m_iMaxTimesPlayer 1` (`placeACamp.conf:4,6,7`) |
| ~~Rewards lost: **$100, 10 XP, two field dressings**~~ | ~~`findGunDealer` $50 · `findShop` $50 + 2 × `FieldDressing_USSR_01.et` · `placeEquipmentBox` $0 · `recruitACivilian` 10 XP · `placeACamp` $0~~ **STRUCK 2026-08-09 (Phase 0.1) — the XP figure is FALSE.** The money half holds ($50 + $50 + $0 + $0 + $0 = **$100**) and the two `FieldDressing_USSR_01.et` entries hold (`findShop.conf:6-9`). But `m_iRewardXP` **defaults to 5** (`OVT_JobConfig.c:26-27`, `[Attribute("5")]`) and is paid unconditionally at `OVT_JobManagerComponent.c:440-442`. Only `recruitACivilian.conf:6` declares it (10); `findGunDealer`, `findShop`, `placeEquipmentBox` and `placeACamp` declare none and therefore each pay **5**. **The true figure is $100, 30 XP and two field dressings.** Nothing in the plan depends on the number, but it must not be carried into the wiki or a bug note |
| Nothing else references the five configs | **STANDS**, with the scope made explicit: a repo-wide grep over `*.c`, `*.conf`, `*.et`, `*.layer` and `*.meta` returns only `Prefabs/GameMode/OVT_OverthrowGameMode.et:30,34,36,38,40`, `Worlds/MP/OVT_Campaign_Eden_Layers/managers.layer:34,38`, the five `Configs/Jobs/*.conf.meta` self-declarations, and the `OVT_GetRadioTowerLocationJobStage.c:5` **comment** already listed as its own row below |
| Six framework classes become orphans | **STANDS.** `OVT_GetShopLocationJobStage` (findShop only), `OVT_GetDealerLocationJobStage` (findGunDealer only), `OVT_HasRecruitJobStage` (recruitACivilian only), `OVT_IsNearestTownWithShopJobCondition` (findShop only), `OVT_IsNearestTownWithDealerJobCondition` (findGunDealer only), `OVT_IsNearestJobCondition` (placeACamp, placeEquipmentBox, recruitACivilian only) — verified by grepping all twelve `Configs/Jobs/*.conf` |
| These are **not** orphaned | **STANDS.** `OVT_PlaceableItemJobStage` (pirateRadio, propagandaRun + the two removed), `OVT_WaitTillPlayerInRangeJobStage` (baseRecon + the two removed), `OVT_WaitTillJobAcceptedJobStage` (5 survivors), `OVT_TownSupportJobCondition` (raiseSupport, pirateRadio, propagandaRun), `OVT_RandomJobCondition` (4 survivors), `OVT_TownPlaceableCountJobCondition` (pirateRadio, propagandaRun) |
| `OVT_GetRadioTowerLocationJobStage.c:5` cites a job that is about to vanish | **STANDS**, verbatim at that exact line: *"shape as OVT_GetDealerLocationJobStage in findGunDealer."* — the class survives, the job does not; reword |
| **After removal no shipped config is player-allocated** | **STANDS.** `m_bPublic` defaults to **1** (`OVT_JobConfig.c:20-21`, `[Attribute("1")]`) and no survivor overrides it; `baseRecon.conf` and `assassinateOfficer.conf` additionally set `m_bBaseOnly 1`. So `playerAllocated` (`OVT_JobManagerComponent.c:543`) is false for all seven. `RpcDo_UpdateJob`'s owner-identity branch is at **`:879`** exactly. T3 must build its per-player records synthetically |
| Gun dealers **do** have a dedicated map icon | **STANDS.** `OVT_MapIcons.TryCreateGunDealerIcon` at **`:111`**, `"gundealer"` sprite loaded inside it; `TryCreateShopIcon` at **`:137`**; gun dealers enumerated with no discovery gate at **`:627`** (and again at `:360`). `tutorial-content/implementation.md` §3.5's struck claim is FALSE — do not carry it into the wiki pass |
| Ten `.st` items for the five jobs | **STANDS**, every line number exact: `.st:3823, 3845, 3879, 3900, 3987, 3995, 4023, 4031, 4157, 4165` |
| Two `.st` Comments point forward at this feature | **STANDS**, one citation corrected. `OVT-IntroHint`'s claim 3 is in the `Comment` at **`.st:3542`** (item `Id` at `:3509`). The welcome page-3 body's `Comment` is at ~~`.st:11021`~~ → **`.st:11055`** (item `OVT-Tutorial_WelcomeIntro_Body3`), and it does read *"The menu's Jobs entry is deliberately NOT named: the starter-jobs-retirement feature changes it"* — **now false**, see D12 |
| Version-migration precedents exist | **STANDS.** `OVT_PlayerManagerSerializer` (v3, branches at `:207/:217/:223`), `OVT_LoadoutManagerSerializer` (v2, `:409/:422`), `OVT_OccupyingFactionManagerSerializer` (v2, `:190/:212`), `OVT_RecruitManagerSerializer` (v2, ~~`:174-215`~~ → branches at **`:175/:185`**) |
| **(added 0.1)** The current job payload really is **version 1** | `OVT_JobManagerSerializer.c:97` — `context.WriteValue("version", 1);`. Any save taken on this tree before Phase 2 is a genuine v1 fixture |

---

## 4. Implementation Phases

**Sequencing rule, non-negotiable: persistence is migrated and proven before anything is deleted.** A phase order that deletes first produces exactly the mis-mapping in §3.1.

---

### Phase 0 — Verify, freeze, capture (8 tasks · S · no product change)

Nothing here changes shipped behaviour. Two of its outputs **cannot be produced later**.

- **0.1** Re-verify §3.8 against source. Anything that fails, fix in this document before proceeding.
- **0.2** Re-check `git status`, the current highest `BUG-` id (**BUG-132** at planning time; parallel sessions commit mid-discovery) and re-run both test groups to confirm the 51 / 88 baseline on today's tree.
- **0.3 — Settle F3.** `tools/launch-server.sh --scenario eden` with a temporary `Print` in `OVT_JobManagerComponent.Init()` logging `m_aJobConfigs.Count()` and each config's `m_sTitle`. Headless; **no client launch, so no window on the user's desktop.** Record the count and the titles verbatim in `context.md`, then revert the `Print`. Fallback if the Eden launch will not come up: replicate the layer's same-GUID partial-override shape in `Worlds/MP/OVT_Campaign_Test_Layers/default.layer`, read the count from an Init-tier `Print`, revert. State which method was used.
- **0.4** If 0.3 shows **replace** (or duplicates): file the next free `BUG-1xx` — "Eden ships N of 12 jobs; the world layer's stale `m_aJobConfigs` override suppresses the rest" — priority high, and note that this feature's Phase 4 fixes it.
- **0.5 — Capture a v1 save fixture. This is the task that cannot be done after Phase 2.** On the unmodified tree, seed a known board through the manager's public API (at least two soon-to-be-retired jobs, two survivors, and non-zero entries in both counter maps), trigger a save, then `.scripts/backup_save.sh --profile OverthrowCI jobs-v1-premigration`. See `tools/README.md` §"Save-state control".
- **0.6** Prove the fixture is worth keeping: `tools/decode-savepoint.py` over it, confirming job records and counter entries are actually present. **A fixture with an empty board proves nothing** and must be re-seeded.
- **0.7** Record the fixture's recreate recipe in `context.md` (a `git worktree` at the pre-migration commit + the 0.5 steps), since the archive itself stays out of git.
- **0.8** Write the frozen legacy table (§3.5) into `context.md` as the authoritative record, independent of the code that will hold it.

**Acceptance:** F3 answered with a quoted log line · a decoded, non-empty v1 fixture exists · baseline re-measured · zero product diff.

---

### Phase 1 — Stable ids on the config surface (7 tasks · S–M)

- **1.1** Add `m_sId` to `OVT_JobConfig` with a doc comment stating it is immutable once shipped and is not `m_sTitle`.
- **1.2** Author the id into all **twelve** `Configs/Jobs/*.conf` per §3.2 — including the five about to be removed, so Phase 2's v1 conversion can be exercised against live configs before they go.
- **1.3** Add `FindJobIndexById` and `GetJobIdByIndex` to the manager, Doxygen'd, with the "linear scan is deliberate" note.
- **1.4** Add the Init-tier guard **T2** (§7): every config's `m_sId` non-empty, lowercase-kebab, unique; every index→id→index round-trips; every *surviving* legacy id resolves; every *retired* legacy id resolves to nothing (this assertion goes live in Phase 4 and must be written to tolerate Phase 1's state — see 4.5).
- **1.5** Prove T2 red once (blank one id, or duplicate two) and record the exact failure text and method in `context.md`.
- **1.6** `tools/compile-check.sh` clean; both groups green at baseline **+1**.
- **1.7** Confirm no behaviour changed: `m_sId` is read by nothing yet.

**Acceptance:** twelve unique ids live · T2 green and proven red · Fast 52 / All 89 (or the re-derived equivalent) · zero runtime behaviour change.

---

### Phase 2 — The serializer migration (9 tasks · L · ⚠️ **ADVANCED (max-effort) dev agent**)

**Flagged advanced.** It rewrites a save format, adds a version branch, touches the manager's most load-bearing invariant, and a mistake corrupts campaigns silently. `/proceed` must route this to `component-developer-advanced`.

- **2.1** Re-read `OVT_JobManagerSerializer.c`'s file header end to end and restate its no-replay argument in the phase report — the migration must not weaken it.
- **2.2** Add the frozen `OVT_PersistedJobV1` and `OVT_PersistedPlayerJobCountsV1` classes, with a header marking them immutable.
- **2.3** Change `OVT_PersistedJob.jobIndex` → `string jobId`; `OVT_PersistedPlayerJobCounts.jobIndices` → `array<string> jobIds`.
- **2.4** Add `LEGACY_V1_JOB_IDS`, `LegacyIdForIndex()`, `IsRetiredLegacyId()` — pure statics, no world, no manager.
- **2.5** `Serialize()`: bump to `version 2`; translate index → id for the board and both counter maps; ERROR-and-skip a config with an empty id (§3.4).
- **2.6** `Deserialize()`: keep the `version < 1` guard verbatim; add the v1 branch (frozen classes, locals named as in §3.4, conversion by the legacy table, one WARNING per drop); the v2 branch reads directly.
- **2.7** `ApplyPersistedJobs()` and `FindRestorableJobConfig()`: resolve by id, set `job.jobIndex` from the resolved index, keep the clear-and-rebuild shape, keep the `OVT_WaitTillDeadJobStage` drop, keep the derived occupancy sets.
- **2.8** Document the v1 support policy in the serializer header (D5), including the concrete removal trigger.
- **2.9** **Prove the no-wire-change claim:** `git diff` shows zero change to any `[RplRpc]` signature, to `RplSave`/`RplLoad`, or to `OVT_PlayerCommsComponent.c`. Paste the `--stat` into the phase report.

**Acceptance:** compile clean · both groups still green at Phase 1's count · `git diff` proves zero RPC/JIP surface change · the v1 branch exists and is documented · **nothing deleted yet**.

---

### Phase 3 — Prove it (8 tasks · M–L · advanced recommended)

The gate on Phase 4. If this phase cannot demonstrate the migration, the deletion does not happen.

- **3.1** Add **T1** (Logic tier, world-free): the legacy mapping table and drop policy.
- **3.2** Add **T3** (PersistenceRoundTrip tier): seed board + both counter maps through the public manager API, save, re-apply, assert every job returns on the right config and both counter maps are intact; then apply twice and assert no doubling (idempotency). ⚠️ Obey that suite's **non-negotiable assertion rule** — no persistence/save-data type names anywhere except the two annotated triggers already in its gate class.
- **3.3** Prove T1 and T3 red once each; record exact failure text, breaking method and date in `context.md`. **No `maxAttempts`, ever.**
- **3.4** ✅ **DONE 2026-08-09, with a CORRECTED recipe.** Run the **v1 fixture check** and read the log for (a) the WARNING per retired job, naming it, and (b) the survivors restored on their correct configs. This is a documented manual check, **not** a group member — the groups reset save state, so it cannot live in one. ⚠️ The recipe written here originally (`activate_save.sh --profile OverthrowCI` + `launch-server.sh`) **could never have worked**: three independent mismatches, one of which no flag can fix. See §7's corrected block and `context.md` §"The v1 fixture check". Result: three named retired drops (`find-gun-dealer`, `find-shop`, `place-equipment-box`), `2 of 4` board entries and `2 of 4` global counters carried forward, survivors `assassinate-traitor` and `raise-support` restored on their own configs — matching Phase 2's byte-level prediction field for field.
- **3.5** Re-run 3.4 a second time against the same fixture to confirm the re-apply path is still idempotent.
- **3.6** ✅ **DONE.** Both groups green. **Measured: Fast 54 / All 92**, against the Phase 3 front's 53 / 90 — delta **+1 / +2** for this phase and **+2 / +3** for the feature against task 0.2's re-measured 52 / 89 baseline. (The plan's "+3 to both" was arithmetic against a 51 / 88 baseline that was already stale, and it also assumed all three cases sit in shared tiers; T3 is in the All group only.)
- **3.7** Write the phase report: what is proven automatically, what only the fixture check proves, what only the user can prove (§7).
- **3.8** **Decision gate.** Only if 3.4 and 3.6 pass does Phase 4 begin.

**Acceptance:** three new cases, each proven red · the v1 fixture restores correctly with named drops · idempotency re-confirmed · both groups green.

---

### Phase 4 — Delete (8 tasks · M)

- **4.1** Remove the five `OVT_JobConfig` entries from `m_aJobConfigs` in `Prefabs/GameMode/OVT_OverthrowGameMode.et`.
- **4.2** Delete the entire `m_aJobConfigs` block from `Worlds/MP/OVT_Campaign_Eden_Layers/managers.layer` (§3.6, D7).
- **4.3** Delete `Configs/Jobs/{findGunDealer,findShop,placeEquipmentBox,recruitACivilian,placeACamp}.conf` and their `.conf.meta` files.
- **4.4** Repo-wide grep for the five conf names and their five GUIDs: **zero** hits outside docs.
- **4.5** Activate T2's "retired legacy ids resolve to nothing" assertion and confirm it goes green here (it would have been red in Phases 1-3 by design; write it so the transition is explicit, not accidental).
- **4.6** Reword `OVT_GetRadioTowerLocationJobStage.c:5` so it cites the surviving *class* rather than the removed *job*.
- **4.7** Verify Eden loads: `tools/launch-server.sh --scenario eden`, log shows **7** configs and no missing-resource error.
- **4.8** compile-check clean; both groups green; the Init log still reports the tutorial manager's eleven entries (nothing in this feature touches them).

**Acceptance:** five confs and ten prefab/layer lines gone · zero dangling references · Eden loads with 7 jobs · both groups green.

---

### Phase 5 — Records, bugs, epic close (7 tasks · S–M)

- **5.1** `.st`: retire the ten items **in place** (D9). Each `Comment` becomes a retirement record naming the date, this feature, the removed job, and the reason for retention. **No `Text` edited, no item deleted, no `.lang.conf` opened.**
- **5.2** Correct the two forward-pointing Comments (§3.8): `OVT-IntroHint`'s claim 3 becomes past tense; the welcome page-3 comment is corrected to say the **Jobs menu entry survives** and the omission is now purely length (D12).
- **5.3** Update `docs/bugs/BUG-037.md` and `BUG-040.md`: they stay `closed`, with a dated note that the five jobs they described were subsequently **removed** by this feature and that the fixes remain live for the surviving configs. **Do not claim this feature closed them.**
- **5.4** Correct the stale BUG-037 framing in `docs/features/new-player-experience/epic-overview.md` (Purpose, the #5 row, the Build Order line, the dependency bullet) and in `epic-requirements.md` (the last Requirements bullet).
- **5.5** Record in `docs/features/jobs/epic-overview.md`: tick off the positional-`jobIndex` tech-debt item as **discharged**, and add the six orphaned-but-kept classes (D8) with their rationale.
- **5.6** Write `context.md`: the F3 answer, the proven-red table, the fixture recipe, the v1 policy, and the residual gaps accepted.
- **5.7** State the string-export position explicitly: only `Comment` fields changed, no rendered text, **so no Workbench re-export is required by this feature**. If the user runs one anyway, `first-spawn`'s owed `OVT-FieldManual_Welcome_Text2` lands in the same pass.

**Acceptance:** ten items retained with retirement records · two false forward-references corrected · bug files accurate · both epic docs de-staled · `git diff --stat Language/` lists `localization_Overthrow.st` and nothing else.

---

### Phase 6 — Help and documentation sync (5 tasks · S · **delegate to `help-docs-sync`**)

Player-facing behaviour changes, so this phase is required.

- **6.1** In-game help: confirm by grep that `Configs/Tutorials/` and `Configs/FieldManual/` contain **zero** references to jobs (verified at planning; re-verify). Expected outcome: **no in-game help edits**, which is a legitimate result, not a skipped task.
- **6.2** Wiki `getting-started` (**pageId 2** — ⚠️ resolve by **slug** and confirm the returned `path` before writing; search returns wrong pageIds): remove the `**Tutorial Jobs**:` paragraph under `### 1. Jobs System`, and item 6 under `## Systems Worth Knowing About`. The heading `### 1. Jobs System` **stays** — it still reads correctly with the generic jobs description alone.
- **6.3** **Do not touch the `v1_3` release note.** "3 new tutorial jobs" is historically accurate.
- **6.4** Do not regress `field-manual`'s two known-wrong strings' wiki counterparts: the public `wanted-system` and `base` pages are **already right** and must stay right.
- **6.5** Verify the **rendered** page, not just the stored source. `pages.update` needs `tags` passed and can report `succeeded: false` after having written; a failed update leaves the render stale until `pages.render` is called. Re-read and confirm.

**Acceptance:** both paragraphs gone from the rendered page · `v1_3` untouched · the `wanted-system` and `base` pages unchanged · no in-game help edit needed, and that is recorded.

---

## 5. Key Technical Decisions

**D1 — A new `m_sId`, not `m_sTitle`.** `m_sTitle` is a localization key (`#OVT-Job_RaiseSupport`) rendered in the Jobs menu and in the completion hint. Renaming a title is a legitimate content edit and must never break a save. A separate identity field costs one attribute and twelve config lines and makes the two concerns independent. Rejected alternatives: the config's **resource name** (the jobs-epic tech-debt note's suggestion) — it is stable but couples the save to a file path, so moving `Configs/Jobs/` breaks saves; and the config's **GUID** — stable and opaque, but unreadable in a log line, and "Dropping saved job '{5D9C33D122545AFD}'" tells a bug reporter nothing.

**D2 — Persisted = id, in-session and on the wire = int.** §3.3. Server and client share one `m_aJobConfigs` within a session, so a positional handle is sound there; converting `RplSave`/`RplLoad` and five RPCs would be scope creep with a real hazard attached (BUG-090: `Rpc()` arity compiles clean and dies at the wire). The save format is the only place identity outlives the list, and it is the only place that changes. **Zero RPC signatures change**, which is a Phase 2 acceptance criterion rather than an assurance.

**D3 — The live counter maps stay `int`-keyed; translation happens only at the save boundary.** Converting `m_aJobCounts` / `m_mPlayerJobCounts` to string keys would reach into `CheckUpdate()`'s offer loop and `StartJob()` for no runtime benefit — the keys are only meaningful within a session anyway. Two translation points, both on a load-only path, both testable.

**D4 — v1 is read through frozen shadow classes, not through the new ones.** `OVT_RecruitManagerSerializer` successfully appended a trailing scalar and cleared it on v1, which is evidence the container tolerates a trailing field. But this migration *replaces* a field's meaning inside an array of records, and if per-record reads are flat-positional an extra field consumes the next record's first field and desyncs everything silently. A frozen shadow class is provably correct under every candidate semantic for the cost of ~15 lines. Cheap insurance on the one code path where a mistake is invisible. **⚠️ AMENDED 2026-08-09 (Phase 2): the decision stands, the NAMES in §3.4 did not.** A persisted record class cannot be renamed at all — the payload carries the class name and the loader instantiates from it, so the frozen class must keep the name version 1 wrote. Built as `OVT_PersistedJob` / `OVT_PersistedPlayerJobCounts` (frozen, v1) and `OVT_PersistedJobV2` / `OVT_PersistedPlayerJobCountsV2` (current). Measured, not inferred — see §3.4's correction box and `context.md` §"The `$type` discovery".

**D5 — v1 support is kept until the next save-format-breaking change, with a written trigger.** Not "forever by inertia" and not "one release". The code is ~40 lines of pure mapping with no runtime cost on the v2 path (one integer comparison), while dropping it early wipes the job board and every lifetime counter on a live campaign — silently, because the `version < 1` guard would treat it as a normal load. **Removal trigger, recorded in the serializer header: the next time `OVT_PersistedJob` changes shape, v1 goes and v2 becomes the floor.** Until then it stays.

**D6 — `ApplyPersistedJobs()` keeps its idempotent clear-and-rebuild, and the id resolve lands inside `FindRestorableJobConfig()`.** `ReapplyLatestSaveData` re-applies the same save to a live session, so idempotency is load-bearing, not stylistic. Putting the resolve where the drop policy already lives means there is exactly one place that decides whether a record comes back — and one place a reviewer has to read.

**D7 — The Eden layer's `m_aJobConfigs` block is deleted outright, before the merge-vs-replace question is settled.** §3.6: the block is same-GUID with empty bodies, so deleting it is correct or neutral under every candidate semantic. The measurement (0.3) is still taken, because it determines whether a shipped bug gets filed — Eden silently running seven jobs short would be significant on its own. **This is the plan's one deliberate decoupling of a blocker from the work it blocks**, and it is what demotes R1 from "stops the feature" to "changes the bug list".

**D8 — The six orphaned stage and condition classes are KEPT.** User decision. They are valid config-composable primitives documented in `jobs/core`'s authoring guide; deleting them is a modder-facing break with no runtime gain — an unreferenced `ScriptAndConfig` class costs nothing but a symbol. Their orphan status is recorded in the jobs epic docs (5.5) so the next reader knows they are unexercised by shipped content, and `OVT_GetRadioTowerLocationJobStage.c:5`'s reference is reworded to cite the class rather than the vanished job.

**D9 — The ten stringtable items are retired in place, not deleted.** Follows `first-spawn` D11 exactly: six languages of translation, `.st` deletions churn exports the user regenerates by hand, and an unreferenced item costs nothing at runtime. The `Comment` becomes the retirement record. Confirmed against `first-spawn/context.md` — that is the shipped precedent, not an inference.

**D10 — The v1 fixture is a captured artifact, not a committed one, and it is captured in Phase 0.** After Phase 2 the tree can no longer *write* a v1 payload, so the fixture must exist before then. It stays out of git (a binary save blob) with a recreate recipe recorded instead. The check that uses it is a documented one-command manual step rather than a group member, because both groups reset save state before every run.

**D11 — This feature does NOT close BUG-037 or BUG-040.** They were fixed in place on 2026-08-03 and are already `closed`. The requirements, the epic overview and `epic-requirements.md` all still say otherwise and are corrected here. What survives as justification is the epic's own binding constraint: **a job assigns a directed goal and puts a marker on a named instance, which is exactly the shape this epic replaced** — plus straightforward redundancy against ten shipped tutorial entries. Stated plainly so that nobody later reads a false closure into the history.

**D12 — `welcome-intro-3-ui.edds` does not need re-shooting, and `first-spawn`'s handover is discharged in writing.** Seven jobs survive, so the Overthrow menu's **Jobs** entry stays and the screenshot showing it stays true. The consequence runs the other way: the welcome page-3 `Comment` claiming *"the Jobs entry is deliberately NOT named: the starter-jobs-retirement feature changes it"* is **now false** and is corrected in 5.2. An image carries a claim exactly as a sentence does (`first-spawn`'s hard-won lesson) — this one was checked and holds.

**D13 — No new tutorial entry for either residual gap.** User decision. *Directed discovery* is largely moot: `OVT_MapIcons` draws every registered shop (`TryCreateShopIcon:137-183`) and every gun dealer (`TryCreateGunDealerIcon:111-134`, dedicated `"gundealer"` sprite) with **no** discovery gate, so the jobs' unique contribution was a marker on one named instance. *Recruit availability* — `recruits-first-recruit` fires on the first recruit gained, not on the option appearing — is accepted as a known gap and recorded in `context.md`.

---

## 6. Definition of Done

An independent evaluator with no implementation context should be able to check every item below.

### Functional

- [ ] **F1 — A v1 save migrates correctly.** Loading a campaign save taken **before** this feature restores every surviving job to its correct config (same title, same stage, same owner, same town/base), and drops **exactly** the records naming the five removed jobs — **one WARNING line per drop, naming the job by id**. No job comes back attached to a config it did not belong to.
- [ ] **F2 — v1 counters migrate the same way.** Lifetime counters for surviving jobs come back on the right jobs; counters for the five removed jobs are dropped, not reassigned. A survivor's remaining allowance is unchanged by the migration.
- [ ] **F3 — v2 saves round-trip.** Save, re-apply, and the job board and both counter maps are identical. Applying the same payload twice produces the same board, not a doubled one.
- [ ] **F4 — The five jobs are gone from the shipped build.** Their `.conf` and `.conf.meta` files are deleted; `m_aJobConfigs` on the game-mode prefab has **7** entries; a repo grep for the five filenames and their five GUIDs returns nothing outside `docs/`.
- [ ] **F5 — Eden loads with the full job list.** `tools/launch-server.sh --scenario eden` starts clean, the job manager reports **7** configs, and there is no missing-resource error from the reconciled layer.
- [ ] **F6 — A fresh campaign is unaffected.** New campaign, no save: the seven surviving jobs offer, tick, complete and pay exactly as before.
- [ ] **F7 — Removal is honest, not silent.** Every dropped record produces a log line naming what was dropped and why. Nothing is dropped without one.

### Quality

- [x] **Q1 — Green.** `tools/compile-check.sh` exit 0 · Fast `{6A6E29FF47ECB840}` exit 0 · All `{6A6E2A002F53A581}` exit 0. ~~Counts are **baseline + 3** — expected **Fast 53 / All 91** against the measured 51 / 88~~ **MEASURED 2026-08-09 (Phase 3): Fast 54 / All 92.** The true baseline was 52 / 89 (task 0.2), and the true delta is **+2 Fast / +3 All** — T1 and T2 sit in tiers both groups share, T3 in the All group only. The *delta* is what must hold, and this is it.
- [ ] **Q2 — Every new assertion is proven able to fail.** Three new cases; for each, `context.md` records the exact failure text, the breaking method and the date. **`maxAttempts` appears nowhere.**
- [x] **Q3 — No localization damage.** `git diff --stat Language/` lists `localization_Overthrow.st` **and nothing else**. All ten job items still exist with their `Text` byte-identical; only `Comment` fields changed. No `.lang.conf` was opened. ✅ **VERIFIED 2026-08-09 (Phase 5) by measurement:** `git diff -U0 Language/` contains **zero** changed lines that are not `Comment` lines, and the `Target_*` line count is 2077 before and 2077 after.
- [ ] **Q4 — No wire surface changed.** `git diff` shows zero change to any `[RplRpc]` signature, to `RplSave`/`RplLoad`, and to `OVT_PlayerCommsComponent.c`.
- [ ] **Q5 — The restore invariant is intact.** `ApplyPersistedJobs()` still clears and rebuilds; the `version < 1` early return is byte-unchanged; the `OVT_WaitTillDeadJobStage` drop still fires; occupancy sets are still derived rather than stored.
- [ ] **Q6 — The legacy table is frozen and says so.** `LEGACY_V1_JOB_IDS` carries all twelve entries in §3.5's order, with a header comment forbidding edits, and a matching copy in `context.md`.
- [ ] **Q7 — Every doc claim is traceable.** Every factual sentence added to `implementation.md`, `context.md`, the bug files or the wiki cites a `file:line` or is cut. No sentence describes a mechanic that does not exist.
- [ ] **Q8 — House style.** `OVT_` prefix, `m_s`/`m_i`/`m_a` naming, Doxygen `//!` on every new method, **no ternary operators**, `ref` on Managed members in arrays/maps.

### Integration

- [ ] **I1 — The Jobs menu lists seven jobs.** Open the Overthrow menu → Jobs on a running campaign: the seven survivors appear, none of the five does, and accept/decline still work.
- [ ] **I2 — Caps and occupancy still gate.** A `GLOBAL_UNIQUE` job does not double-offer; `baseRecon` still respects `m_iMaxTimes 2`; a town does not host two of the same public job.
- [ ] **I3 — The tutorial entries covering the five topics still fire.** `economy-first-buy`, `shops-first-gun-dealer`, `place-first-placeable`, `recruits-first-recruit`, `map-first-open` are byte-unchanged and still deliver. `Configs/Tutorials/` and `Configs/FieldManual/` have **zero** diff.
- [ ] **I4 — The wiki matches the build.** `getting-started` no longer describes tutorial jobs, in the **rendered** page; the `v1_3` release note is untouched; the `wanted-system` and `base` pages are unchanged.
- [x] **I5 — The record is straight.** BUG-037 and BUG-040 note removal without claiming closure by it; `epic-overview.md` and `epic-requirements.md` no longer assert the stale framing; the jobs epic records the discharged tech debt and the six kept orphans. ✅ **DONE 2026-08-09 (Phase 5)** — the six orphans were re-verified by grep rather than transcribed, and two already-resolved jobs-epic debt items were ticked in the same pass.
- [x] **I6 — `first-spawn`'s handover is visibly discharged.** `welcome-intro-3-ui.edds` is unchanged and the reason is written down; the welcome page-3 `Comment` no longer says the Jobs entry is about to change. ✅ **DONE 2026-08-09 (Phase 5)** — `git diff --stat UI/` is empty and `UI/Layouts/Menu/MainMenu.layout:284, :292` (the Jobs button and its `#OVT-MainMenu_Jobs` label) is byte-unchanged, so the screenshot stays true and was not re-shot.

### Verification method

| Item | How |
|---|---|
| F1, F2 | `.scripts/activate_save.sh jobs-v1-premigration --profile OverthrowCI` then `tools/launch-server.sh`; read the log for the named drops and the restored survivors. **Plus** the user's own real campaign save (§7 U1) — the only fully honest version of this test |
| F3, Q5 | `tools/run-tests.sh "{6A6E2A002F53A581}"` — case **T3** |
| F4 | `git status` + repo grep for the five conf names and GUIDs |
| F5 | `tools/launch-server.sh --scenario eden`, read the startup log |
| F6, I1, I2 | Manual play-test §7 P1-P5 |
| F7 | Read the log lines produced by the F1 run |
| Q1, Q2 | Both `tools/run-tests.sh` group runs + the proven-red table in `context.md` |
| Q3, Q4 | `git diff --stat Language/`, `git diff` over `Scripts/Game/GameMode/Managers/OVT_JobManagerComponent.c` and `OVT_PlayerCommsComponent.c` |
| Q6, Q7, I5 | Read the serializer header, `context.md`, the bug files and both epic docs |
| Q8 | `tools/compile-check.sh` + review |
| I3 | `git diff --stat Configs/` + play-test P5 |
| I4 | Fetch the rendered wiki page over HTTP after the MCP write |
| I6 | `git log`/`git diff` on the `.edds` (expect none) + read the corrected `.st` Comment |

---

## 7. Testing Strategy

### Automated — three new cases, one per tier that can hold one

Existing coverage does some of this for free and **must not be duplicated**: `OVT_TEST_Logic_Jobs.c` holds 4 pure-condition cases, and `OVT_TEST_InitSuite.c:71` already asserts `OVT_Global.GetJobs()` resolves.

| # | Tier | Case | Asserts | Prove-red method |
|---|---|---|---|---|
| **T1** | **Logic** (world-free) | `OVT_TEST_Logic_Jobs_LegacyIndexMapping` | `LegacyIdForIndex()` returns the right id for all twelve v1 indices; `""` for -1 and 12; `IsRetiredLegacyId()` is true for exactly the five and false for the seven. This pins the one table whose corruption is undetectable at runtime | Swap two entries in the table, or move one id between the retired and surviving sets |
| **T2** | **Init** | `OVT_TEST_Init_Jobs_StableIdsAreUniqueAndResolve` | Every shipped config's `m_sId` is non-empty, lowercase-kebab and unique; index→id→index round-trips for every config; every **surviving** legacy id resolves to a config (this is the rename guard); every **retired** legacy id resolves to none (live from Phase 4) | Blank one `m_sId`; duplicate two; rename `raise-support` in one conf and expect the legacy-resolve half to name it |
| **T3** | **PersistenceRoundTrip** | `OVT_TEST_PersistenceRoundTrip_JobBoard_SurvivesSaveAndReload` | Seed a known board (a public town job, a base-only job, a synthetic player-allocated record — see §3.8: no shipped config is player-allocated after removal) plus non-zero entries in both counter maps, through the **public manager API only**; save; re-apply; assert every job is on its correct config with its stage, owner and location intact, and both counter maps match. Then re-apply again and assert no doubling | Break the id write (write `""`), or the id read (resolve by index instead), and expect a named mismatch |

⚠️ **T3 must obey `OVT_TEST_PersistenceRoundTripSuite`'s non-negotiable assertion rule**: no persistence-framework, vanilla-persistence or Overthrow save-data type name may appear in that tree except the two annotated triggers already in its gate class. Every assertion reads back through the same public manager API that wrote it.

Every case **proven able to fail once**, with the exact failure text, method and date in `context.md`. **`maxAttempts` is banned** — a test that needs retries is a bug in the test.

### Semi-automated — the v1 fixture check (§4 task 3.4)

Not a group member: both groups reset save state before every run, and this check *requires* pinned state.

> **⚠️ THE RECIPE BELOW IS THE CORRECTED ONE (Phase 3, 2026-08-09). The plan's original two commands could never have worked** — `.scripts/activate_save.sh` targets profile `OverthrowCI` while `tools/launch-server.sh` defaults to `OverthrowDS` (`tools/launch-server.sh:85-87,140`); the two name the campaign directory differently, because the harness loads a **world** and the server loads a **mission**; and — found in Phase 3 — a `--mode local` server authenticates nobody and writes to `profile/.save/game/…` with **no `app*_user*` component at all**, which is the only shape `activate_save.sh` can resolve (`.scripts/activate_save.sh:38-67`). No flag combination fixes that, so the fixture is staged directly instead. Neither script was modified; both are dev-ops-owned. Full diagnosis, the exact staging script, the evidence and the cleanup step are in `context.md` §"The v1 fixture check".

```bash
# Stage the fixture as the NEWEST save point in the server's own save tree, with
# meta-info.json's m_sMissionResource / m_iSavePointNr rewritten for the mission the
# server loads. The WorldState blob - the payload under test - is copied byte for byte.
#   <DS>/profile/.save/game/6B0E7A50D1E2F3A4-25-OVT-TestWorld/playthrough000/savepointNNN
tools/launch-server.sh --timeout 180 --quiet    # headless; no client window; EXIT_CODE=124 = success
grep -a "Loading savepoint\|Dropping saved job\|Migrated a version 1" "$LOG_DIR/console.log"
rm -rf "<DS>/…/playthrough000/savepointNNN"     # ALWAYS clean up
```

**This is a stronger check than the plan expected, not a weaker one.** A dedicated server auto-continues (`OVT_OverthrowGameMode.DecideDedicatedStart:465-487` → `LoadLatestSave()`), so the fixture is migrated through **`SaveGameManager` and a real world transition** — the very path this section's "What automation structurally cannot cover" calls out. Read the log for one WARNING per retired job (naming the job) and for the survivors restored on their correct configs; the survivors need a temporary `Print` in `ApplyPersistedJobs()`, since the migration's own line reports counts rather than names (same technique and same revert discipline as task 0.3's F3 probe). Run it twice to re-confirm idempotency — a 180 s window is well inside the 600 s autosave interval, so no new save point is written between runs and the second run really is against the same fixture. `tools/decode-savepoint.py` is available if the log leaves a question open.

### What automation structurally cannot cover

- **The real quit-and-continue path.** `SaveGameManager.Load` restarts the autotest harness, which is why the round-trip tier proves save→dirty→re-apply instead. ✅ **AMENDED 2026-08-09 (Phase 3): the fixture check DOES reach this path** — the corrected recipe runs a headless server that auto-continues, so `SaveGameManager` really does load a save point and the world really does transition. What it still cannot cover is *the user's own campaign*: the fixture holds four job records and two counter entries, seeded by this feature. The mechanism is now observed; the scale is not.
- **Multiplayer.** JIP and two-client behaviour are outside the spine entirely.
- **UI.** Whether the Jobs menu still looks and behaves right.
- **A real campaign's save.** The fixture is seeded and small; the user's campaign is neither.

### Play-test — single player (the user)

| # | Step | Expect |
|---|---|---|
| **U1** | ⚠️ **Back the save up first.** Take an existing campaign save from **before** this build and **Continue** it | The campaign resumes. Jobs on the board are the right jobs. The log carries a WARNING per dropped starter job, naming it. **This is the single most important observation in the feature** |
| U2 | Open Overthrow menu → Jobs | Seven job types possible; none of the five starter jobs; list renders, accept and decline work |
| U3 | Accept a job, complete it | Correct reward paid, completion hint shows to **you only**, job leaves the board |
| U4 | Save, quit, Continue again | Board and counters identical; nothing duplicated |
| U5 | Start a **fresh** campaign | No starter jobs ever appear; the seven survivors offer normally over time; tutorial popups still fire on first buy / first place / first recruit |
| U6 | Open the map | Every shop and every gun dealer is still marked (this is what makes the lost "directed discovery" acceptable) |

### Play-test — multiplayer (high value, and the epic's biggest outstanding question)

This feature touches the job manager's replication surface and the save format, and the epic records that **two-client per-player isolation has never been observed passing**. An MP pass here is planned, not assumed.

⚠️ **Warn the user before launching a client** — it opens a window on their desktop and can orphan. The server binary is genuinely headless. **Always pass a long `--timeout`**; it defaults to 600 s and will kill the client mid-test.

```bash
tools/launch-server.sh
tools/launch-game.sh --timeout 3600 --profile OverthrowClient1 --allow-concurrent -- -client 127.0.0.1:2001
tools/launch-game.sh --timeout 3600 --profile OverthrowClient2 --allow-concurrent -- -client 127.0.0.1:2001
```

| # | Step | Expect |
|---|---|---|
| M1 | Both clients join, open Jobs | Both see the same seven-type board; no client shows a starter job |
| M2 | Client A accepts a public job | Both boards update; A owns it; B still sees it correctly |
| M3 | A completes it | A gets the hint and the reward; **B does not get the hint** (BUG-040's fix, still live) |
| M4 | **JIP:** with the campaign running and jobs on the board, connect B fresh | B's board matches A's — this exercises `RplSave`/`RplLoad`, the one path the id change deliberately did not touch |
| M5 | Save on the server, restart it, both reconnect | Board and counters survive; no job attached to the wrong config |

---

## 8. Dependencies

- **`tutorial-content` (#3, complete 2026-08-09).** The precondition. Its as-built coverage mapping (`tutorial-content/context.md`) is what makes removal safe. Its ten entries are **not** touched.
- **`field-manual` (#2, complete 2026-08-08).** Source of the wiki handoff (D12 of that plan). Its two known-wrong strings stay out of scope, and the wiki pass must not regress the pages that are already right.
- **`first-spawn` (#4, complete 2026-08-09).** Source of the D9 string-retention precedent and of the `welcome-intro-3-ui.edds` liability, discharged by D12 here. One of its `.st` Comments is corrected by 5.2.
- **`jobs/core` (documented).** This feature edits its config surface and its save format, and discharges one of its tech-debt items. No other jobs-epic work may be in flight on `m_aJobConfigs` concurrently — check `git log` at the start of every phase.
- **`core/persistence`.** `OVT_JobManagerSerializer`'s no-replay restore argument is that epic's most load-bearing invariant. This feature must preserve it verbatim, and Phase 2.1 makes restating it a task.
- **Tooling:** `tools/compile-check.sh`, `tools/run-tests.sh` (both groups), `tools/launch-server.sh` (+ `launch-game.sh` for the MP pass), `.scripts/backup_save.sh` / `activate_save.sh`, `tools/decode-savepoint.py`.
- **The user**, for U1-U6 and M1-M5. **No Workbench string re-export is required** (5.7) — only `Comment` fields change.
- **External:** none. No base-game or EPF/EDF change.

---

## 9. Risks & Mitigation

**R1 — The Eden world-layer override REPLACES rather than merges, invalidating index reasoning and revealing a shipped bug.**
*Likelihood: low (contradicted by a measured in-repo finding). Impact: high if true — seven jobs, including all four illegal-placeables jobs, would never have spawned in the real campaign.*
This is the plan's top risk and it is **structurally defused before it is answered**: §3.6 shows deleting the block is correct or neutral under every candidate semantic, and §3.5 shows the legacy table is identical either way because the layer's five entries are a same-order prefix of the prefab's twelve. So a wrong guess changes the *bug list*, not the *code*. Measured directly in Phase 0.3 (headless Eden server + a temporary count `Print`), with a test-world replication as fallback. Prior evidence: `OVT_TEST_SuiteBase.c:36-41` records Enfusion **appending** a world-layer array override (measured, findings.md 1.3c), plus the project's "same-GUID overrides are deltas" memory. **Do not treat that prior as the answer** — see R6.

**R2 — The v1 branch reads the payload wrong and a real campaign's job board is silently wiped or mis-mapped.**
*Likelihood: low-medium. Impact: severe — silent save corruption on live campaigns, the worst failure this feature can produce.*
Mitigated by frozen shadow classes rather than a mutated record class (D4); by the v1 locals being named exactly as the v1 writer named them (§3.4); by a captured real v1 fixture proving the path end to end (0.5/3.4) rather than a synthetic one; by the honest-drop rule making every loss visible in the log; and by the phase gate at 3.8 — no deletion until the fixture check passes. The user's U1 on a real campaign is the last line and the reason U1 begins with "back the save up first".

**R3 — The v1 fixture is never captured, or is captured empty, and the migration ships unproven.**
*Likelihood: medium — it is the easiest task in the plan to skip, and it becomes impossible after Phase 2.*
Mitigated by making capture a Phase 0 task with its own decode-and-verify follow-up (0.6), by recording a recreate recipe against a pre-migration `git worktree` (0.7), and by naming it explicitly in D10 as the one artifact with a deadline. If it is missed, the recovery is a worktree at the pre-Phase-2 commit — annoying, not fatal, but only if the recipe was written down.

**R4 — Something outside the plan still holds a positional job reference.**
*Likelihood: low. Impact: medium.*
Only five files mention `jobIndex` (`OVT_JobManagerComponent`, `OVT_JobManagerSerializer`, `OVT_JobsContext`, `OVT_PlayerCommsComponent`, and a `OVT_TutorialComponent` **comment**), and by D2 four of the five keep using the int unchanged. Mitigated by re-running that grep at the start of Phase 2 and again in Phase 4, and by Q4's `git diff` proof.

**R5 — MP regresses in a way no gate can see.**
*Likelihood: low by design (no wire change) but never zero. Impact: high; the epic's biggest unverified area.*
Mitigated by D2 making the wire a no-op surface, by Q4 proving it with a diff, and by M1-M5 being a planned deliverable rather than a stretch. If MP fails in a way that reproduces on the pre-change build too, it is a jobs-epic or `tutorial-system` defect and must be reported as such rather than absorbed here.

**R6 — A documented trap is trusted instead of verified.**
*Likelihood: medium — it has already happened in this epic. Impact: medium-to-high; a false premise propagates through three documents before anyone notices.*
`tutorial-content`'s pre-loaded trap table contained a false row (gun dealers *do* have a dedicated map icon) that survived into three documents until a phase fact-check caught it, and this feature's own stated premise (BUG-037/040 open) was **stale on arrival**. Mitigated by Phase 0.1 re-verifying **every row of §3.8, including the rows written by this document**, and by Q7 requiring a `file:line` or a cut. Applies to the wiki pass with equal force: no gate catches a well-formed lie.

**R7 — Concurrent sessions collide on the shared files.**
*Likelihood: medium; this tree has hosted parallel sessions for weeks. Impact: medium.*
This feature edits `Prefabs/GameMode/OVT_OverthrowGameMode.et` and `Language/localization_Overthrow.st` — the two busiest shared files in the repo — and files bugs from a numbering space other sessions also use. Mitigated by re-checking `git status`, `git log` and the highest `BUG-` id **at the start of every phase** rather than trusting this document's snapshot (**BUG-132** at planning time), and by removing rather than appending, which conflicts loudly instead of silently.

**R8 — Scope creep into the jobs system.**
*Likelihood: medium; the manager has four open tech-debt items and they are all visible from the code this feature touches. Impact: medium; each is a behaviour change wearing a refactor's clothes.*
Only the positional-`jobIndex` item is in scope. The other four (notification routing, per-player caps done properly, reactive UI refresh, `m_vCurrentWaypoint`) are **recorded in the jobs epic and not touched**. `RpcAsk_AcceptJob`/`RpcAsk_DeclineJob` stay on `OVT_PlayerCommsComponent`, deliberately (§2).

---

## 10. Quality Bar

This is a **data-integrity and backwards-compatibility** feature. It ships almost no player-visible change — five jobs stop appearing — and its entire risk sits in a file format that fails silently. The bar is set accordingly.

**1. No save corruption. Not "probably fine".**
The chosen approach was taken over tombstoning specifically because it makes the removal *provable*. The proof is the fixture check plus T1 and T3, not an argument in this document. If the fixture check cannot be run, the deletion does not ship.

**2. No silent mis-mapping.**
A record that cannot be restored is **dropped and logged by name**. There is no path where a job comes back as a different job, and no path where something disappears without a line explaining it. §3.4's four message shapes are part of the DoD, not decoration.

**3. Idempotent re-apply, preserved verbatim.**
`ReapplyLatestSaveData` re-applies the same save to a live session. `ApplyPersistedJobs()`'s clear-and-rebuild and the no-replay stage argument in the serializer header are the epic's most load-bearing persistence invariant. They are re-read (2.1), preserved (Q5) and re-asserted (T3's second apply).

**4. Multiplayer unbroken, and demonstrably so.**
The design's answer to the wire is "do not touch it", and Q4 proves that with a diff rather than asserting it. M1-M5 then observe it, on the epic's least-verified surface.

**5. Every claim traceable to a `file:line`.**
In the code comments, in `context.md`, in the bug notes, in the epic corrections and on the wiki. This feature's own premise was stale, and its predecessor shipped a false trap row into three documents. The correction is not more care — it is a citation or a cut.

**6. Nothing deleted before it is safe to delete.**
The phase order is the feature. Phase 4 has a gate in front of it (3.8) and that gate is not advisory.
