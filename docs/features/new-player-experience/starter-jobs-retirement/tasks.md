# Starter Jobs Retirement - Task Checklist

**Last Updated:** 2026-08-09
**Progress:** 52/52 tasks complete (100%) — Phases 0, 1, 2, 3, 4, 5 and **6** complete 2026-08-09. Phase 6 needed **no in-game help edit** (`Configs/Tutorials/` and `Configs/FieldManual/` contain zero references to jobs, and zero diff — DoD I3) and removed the two starter-jobs mentions from the wiki `getting-started` page (pageId 2), verified on the **rendered** page; `v1_3`, `wanted-system` and `base` are untouched (DoD I4). The five starter jobs are **deleted**, `RETIRED_IDS_ARE_DELETED` is **true** and green, and the record is straight: ten `.st` items retired in place (Comment fields only), two false forward-references corrected, both bug files noted-but-still-`closed`, both `new-player-experience` epic docs de-staled and the jobs epic's positional-`jobIndex` debt ticked discharged. Final counts: compile-check 0 · Fast **54** · All **92** (delta **+2 / +3** against the 52 / 89 baseline). ✅ **FEATURE COMPLETE — all eleven play-test checks (U1-U6, M1-M5) passed and were signed off by the user on 2026-08-09.** Nothing is owed.

**Epic:** `new-player-experience` (feature #5 of 5 — the last) · **Plan:** `implementation.md` · **Scope truth:** `requirements.md`

> Task ids match the `<phase>.<n>` ids in `implementation.md` §4 — do not renumber them.
> **Agent tiers are set by the plan.** Phase **2** is flagged **ADVANCED** by §4 and routes to `component-developer-advanced`; Phase **3** is "advanced recommended" and routes there too. Phases **0**, **1**, **4** and **5** route to `component-developer`. Phase **6** routes to `help-docs-sync`. `/proceed` and `/autorun-feature` must respect that routing rather than substituting a default agent.
> **Sequencing rule, non-negotiable (§4):** persistence is migrated and proven **before** anything is deleted. Phase 4 sits behind the decision gate at **3.8**, and that gate is not advisory.
> **✅ BASELINE MEASURED 2026-08-09 (task 0.2) on `f6681d00`: compile-check exit 0 · Fast `{6A6E29FF47ECB840}` = **52** · All `{6A6E2A002F53A581}` = **89**.** The plan's 51 / 88 was already one behind (a parallel session added one Init case, which both groups share). Highest `BUG-` id is **BUG-133**, so the next free id is **BUG-134** — not the plan's BUG-132. Use 52 / 89 as this feature's baseline and re-derive at each phase front anyway.
> **Baselines are measured in task 0.2, not inherited.** The plan's measured figures for this tree (2026-08-09) are **Fast `{6A6E29FF47ECB840}` = 51, All `{6A6E2A002F53A581}` = 88, compile-check exit 0**. Older docs saying 38/66 or 47/78 are stale. Re-derive anyway — parallel sessions commit into this tree.
> Every phase ends with `tools/compile-check.sh` exit 0. Phases touching a `.c` file (**1**, **2**, **3**, **4**) also end with the **All** group green; config/string/doc-only phases (**0**, **5**, **6**) end with **Fast** unless they touched script.
> **Never edit `Language/localization_Overthrow.<lang>.conf`** — the `.st` master only, and in Phase 5 only `Comment` fields, never a `Text`.
> **`maxAttempts` is banned.** Every new assertion is proven able to fail once, with the exact failure text, method and date recorded in `context.md`.
> **R6 binds this feature:** a documented trap is not evidence. §3.8 is re-verified in **0.1** and re-checked at phase fronts; this feature's own stated premise (BUG-037/040 open) was **stale on arrival**.
> **R7:** re-check `git status`, `git log` and the highest `BUG-` id at the start of every phase — this tree hosts parallel sessions. **BUG-132** was the highest at planning time.

---

## Phase 0: Verify, freeze, capture (8/8 complete ✅) — `component-developer`

*No product change. Two of its outputs — the F3 measurement and the v1 fixture — **cannot be produced later**.*

- [x] **0.1 — Re-verify §3.8 against source**
  - Description: Every row of the evidence pack, **including the rows this document wrote**. Anything that fails is fixed in `implementation.md` in place, dated. R6 exists because a sibling shipped a false trap row into three documents.
  - File(s): (read-only sweep) — corrections land in `implementation.md` §3.8
  - Estimate: 🟡 1 h

- [x] **0.2 — Re-check preconditions and re-measure the baseline**
  - Description: `git status` / `git log`; current highest `BUG-` id (**BUG-132** at planning time); both test groups run and their **measured** case counts recorded in `context.md` as this feature's baseline.
  - File(s): (read-only) — records into `context.md`
  - Estimate: 🟢 20 min

- [x] **0.3 — Settle F3: does the Eden layer's `m_aJobConfigs` override MERGE or REPLACE?**
  - Description: `tools/launch-server.sh --scenario eden` with a temporary `Print` in `OVT_JobManagerComponent.Init()` logging `m_aJobConfigs.Count()` and each config's `m_sTitle`. **Headless — no client launch, no window on the user's desktop.** Record the count and titles **verbatim** in `context.md`, then revert the `Print`. Fallback: replicate the same-GUID partial-override shape in `Worlds/MP/OVT_Campaign_Test_Layers/default.layer` and read the count from an Init-tier `Print`. State which method was used. ⚠️ The prior (`OVT_TEST_SuiteBase.c:36-41`, "same-GUID overrides are deltas") is **not** the answer — measure it.
  - File(s): `Scripts/Game/GameMode/Managers/OVT_JobManagerComponent.c` (temporary, reverted)
  - Estimate: 🟡 1.5 h

- [x] **0.4 — File the Eden bug if 0.3 shows replace (or duplicates)**
  - Description: Next free `BUG-1xx` — "Eden ships N of 12 jobs; the world layer's stale `m_aJobConfigs` override suppresses the rest" — priority high, noting that this feature's Phase 4 fixes it. If 0.3 shows merge, record the measurement and file nothing.
  - File(s): `docs/bugs/BUG-1xx.md` (conditional)
  - Estimate: 🟢 20 min

- [x] **0.5 — Capture the v1 save fixture (⚠️ impossible after Phase 2)**
  - Description: On the **unmodified** tree, seed a known board through the manager's public API — at least **two soon-to-be-retired jobs, two survivors, and non-zero entries in both counter maps** — trigger a save, then `.scripts/backup_save.sh --profile OverthrowCI jobs-v1-premigration`. See `tools/README.md` §"Save-state control".
  - File(s): save archive (out of git)
  - Estimate: 🟡 1.5 h

- [x] **0.6 — Prove the fixture is worth keeping**
  - Description: `tools/decode-savepoint.py` over it; confirm job records and counter entries are actually present. **A fixture with an empty board proves nothing** and must be re-seeded.
  - File(s): (read-only) — result into `context.md`
  - Estimate: 🟢 30 min

- [x] **0.7 — Record the fixture's recreate recipe**
  - Description: A `git worktree` at the pre-migration commit plus the 0.5 steps, written into `context.md`, since the archive itself stays out of git (D10).
  - File(s): `docs/features/new-player-experience/starter-jobs-retirement/context.md`
  - Estimate: 🟢 20 min

- [x] **0.8 — Write the frozen legacy table into `context.md`**
  - Description: §3.5's twelve entries in order, as the authoritative record independent of the code that will hold it (Q6 requires a matching copy).
  - File(s): `docs/features/new-player-experience/starter-jobs-retirement/context.md`
  - Estimate: 🟢 15 min

**Acceptance:** F3 answered with a quoted log line · a decoded, non-empty v1 fixture exists · baseline re-measured · **zero product diff**.

---

## Phase 1: Stable ids on the config surface (7/7 complete ✅) — `component-developer`

- [x] **1.1 — Add `m_sId` to `OVT_JobConfig`**
  - Description: One `[Attribute]`, with a doc comment stating it is **immutable once shipped** and is **not** `m_sTitle` (D1).
  - File(s): `Scripts/Game/Configuration/OVT_JobConfig.c`
  - Estimate: 🟢 20 min

- [x] **1.2 — Author the id into all twelve `Configs/Jobs/*.conf`**
  - Description: Per §3.2's table, short lowercase kebab-case matching the tutorial-entry id convention. **Including the five about to be removed**, so Phase 2's v1 conversion can be exercised against live configs before they go.
  - File(s): `Configs/Jobs/*.conf` (12 files)
  - Estimate: 🟡 45 min

- [x] **1.3 — Add `FindJobIndexById` and `GetJobIdByIndex` to the manager**
  - Description: Doxygen'd, `-1` / `""` on miss, with the "linear scan over twelve entries on a load-only path is deliberate; a cached map is YAGNI and would need invalidation" note.
  - File(s): `Scripts/Game/GameMode/Managers/OVT_JobManagerComponent.c`
  - Estimate: 🟢 30 min

- [x] **1.4 — Add the Init-tier guard T2**
  - Description: Every config's `m_sId` non-empty, lowercase-kebab and unique; every index→id→index round-trips; every **surviving** legacy id resolves; every **retired** legacy id resolves to nothing — that last assertion goes live in **Phase 4** and must be written so the transition is **explicit, not accidental** (see 4.5).
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c`
  - Estimate: 🟡 1.5 h

- [x] **1.5 — Prove T2 red once**
  - Description: Blank one id, or duplicate two. Record the **exact failure text**, the breaking method and the date in `context.md`. No `maxAttempts`, ever.
  - File(s): `context.md`
  - Estimate: 🟢 30 min

- [x] **1.6 — Green gate**
  - Description: `tools/compile-check.sh` exit 0; both groups at the 0.2 baseline **+1** (or the re-derived delta — T2 may be a branch on an existing case rather than a new case, which is the arithmetic that bit `first-spawn`).
  - File(s): (verification)
  - Estimate: 🟢 20 min

- [x] **1.7 — Confirm no behaviour changed**
  - Description: `m_sId` is read by nothing yet outside the guard. State it explicitly in the phase report.
  - File(s): (verification)
  - Estimate: 🟢 15 min

**Acceptance:** twelve unique ids live · T2 green and proven red · both groups green at baseline + the re-derived delta · **zero runtime behaviour change**.

---

## Phase 2: The serializer migration (9/9 complete ✅) — ⚠️ **`component-developer-advanced`**

*Rewrites a save format, adds a version branch, touches the manager's most load-bearing invariant. A mistake corrupts campaigns **silently**.*

- [x] **2.1 — Re-read the serializer's file header and restate its no-replay argument**
  - Description: End to end, in the phase report. The migration must not weaken it (`core/persistence`'s most load-bearing invariant).
  - File(s): (read-only) `Scripts/Game/Persistence/Serializers/Components/OVT_JobManagerSerializer.c`
  - Estimate: 🟢 30 min

- [x] **2.2 — Add the frozen `OVT_PersistedJobV1` and `OVT_PersistedPlayerJobCountsV1`**
  - Description: Byte-for-byte copies of today's member layout, with a header marking them **immutable, never edited again** (D4).
  - File(s): `Scripts/Game/Persistence/Serializers/Components/OVT_JobManagerSerializer.c`
  - Estimate: 🟢 30 min
  - ⚠️ **DONE, BUT UNDER DIFFERENT NAMES — AND THE PLAN'S NAMES WOULD HAVE CORRUPTED SAVES.** The binary container writes a `$type` discriminator naming the concrete class and instantiates from it on load; a renamed-but-identical class fails the read outright and takes the rest of the stream with it (measured — see `context.md` §"The `$type` discovery"). The freeze therefore had to land on the classes the version 1 payload actually **names**: `OVT_PersistedJob` and `OVT_PersistedPlayerJobCounts` are now the frozen version 1 records (`OVT_JobManagerSerializer.c:34`, `:52`), and the current records are `OVT_PersistedJobV2` / `OVT_PersistedPlayerJobCountsV2` (`:80`, `:102`). D4's intent — a frozen shadow class, never edited — is honoured exactly; only which class carries which name changed.

- [x] **2.3 — Move the v2 records onto string ids**
  - Description: `OVT_PersistedJob.jobIndex` → `string jobId`; `OVT_PersistedPlayerJobCounts.jobIndices` → `array<string> jobIds`.
  - File(s): `Scripts/Game/Persistence/Serializers/Components/OVT_JobManagerSerializer.c`
  - Estimate: 🟢 30 min

- [x] **2.4 — Add `LEGACY_V1_JOB_IDS`, `LegacyIdForIndex()`, `IsRetiredLegacyId()`**
  - Description: Pure statics — no world, no manager — so T1 can test them world-free. Header comment marks the table **frozen: it is history, not configuration** (§3.5, Q6).
  - File(s): `Scripts/Game/Persistence/Serializers/Components/OVT_JobManagerSerializer.c`
  - Estimate: 🟡 45 min

- [x] **2.5 — `Serialize()`: version 2, index → id**
  - Description: Bump the version; translate for the board and **both** counter maps; **ERROR-and-skip** a config with an empty id, using §3.4's exact message shape.
  - File(s): `Scripts/Game/Persistence/Serializers/Components/OVT_JobManagerSerializer.c`
  - Estimate: 🟡 1 h

- [x] **2.6 — `Deserialize()`: keep the `version < 1` guard verbatim, add the v1 branch**
  - Description: v1 reads into the frozen classes with locals named **exactly as the v1 writer named them** (`jobRecords`, `countIndices`, `countValues`, `playerCounts` — `LoadContext.Read` derives the property name from the local's name; comment why); conversion by the legacy table; **one WARNING per drop**, in §3.4's exact shapes. v2 reads directly. ⚠️ Binary contexts are **positional** — write order must equal read order, version first.
  - File(s): `Scripts/Game/Persistence/Serializers/Components/OVT_JobManagerSerializer.c`
  - Estimate: 🔴 2 h

- [x] **2.7 — `ApplyPersistedJobs()` and `FindRestorableJobConfig()` resolve by id**
  - Description: Set `job.jobIndex` from the resolved index; **keep the clear-and-rebuild shape** (D6 — `ReapplyLatestSaveData` idempotency is load-bearing), **keep the `OVT_WaitTillDeadJobStage` drop**, keep the derived occupancy sets. The id resolve lands **inside `FindRestorableJobConfig()`**, where the drop policy already lives.
  - File(s): `Scripts/Game/GameMode/Managers/OVT_JobManagerComponent.c`
  - Estimate: 🔴 2 h

- [x] **2.8 — Document the v1 support policy in the serializer header**
  - Description: D5, including the **concrete removal trigger**: the next time `OVT_PersistedJob` changes shape, v1 goes and v2 becomes the floor.
  - File(s): `Scripts/Game/Persistence/Serializers/Components/OVT_JobManagerSerializer.c`
  - Estimate: 🟢 20 min

- [x] **2.9 — Prove the no-wire-change claim**
  - Description: `git diff` shows **zero** change to any `[RplRpc]` signature, to `RplSave`/`RplLoad`, or to `OVT_PlayerCommsComponent.c`. Paste the `--stat` into the phase report (Q4). BUG-090: `Rpc()` arity compiles clean and dies at the wire — this design gives that hazard no surface, and the diff is the proof rather than the assurance.
  - File(s): (verification)
  - Estimate: 🟢 20 min

**Acceptance:** compile clean · both groups still green at Phase 1's count · `git diff` proves zero RPC/JIP surface change · the v1 branch exists and is documented · **nothing deleted yet**.

---

## Phase 3: Prove it (8/8 complete ✅) — `component-developer-advanced`

*The gate on Phase 4. If this phase cannot demonstrate the migration, the deletion does not happen.*

- [x] **3.1 — Add T1 (Logic tier, world-free)**
  - Description: `OVT_TEST_Logic_Jobs_LegacyIndexMapping` — `LegacyIdForIndex()` correct for all twelve v1 indices and `""` for -1 and 12; `IsRetiredLegacyId()` true for exactly the five, false for the seven. Pins the one table whose corruption is undetectable at runtime.
  - File(s): `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_Jobs.c`
  - Estimate: 🟡 1 h

- [x] **3.2 — Add T3 (PersistenceRoundTrip tier)**
  - Description: `OVT_TEST_PersistenceRoundTrip_JobBoard_SurvivesSaveAndReload` — seed board + **both** counter maps through the **public manager API only**, save, re-apply, assert every job returns on its correct config with stage/owner/location intact and both counter maps match; then apply **twice** and assert no doubling. Build the per-player record **synthetically** (§3.8: after removal no shipped config is player-allocated). ⚠️ Obey the suite's **non-negotiable assertion rule** — no persistence/save-data type names anywhere in that tree except the two annotated triggers already in its gate class.
  - File(s): `Scripts/Game/Tests/TestSuites/Persistence/OVT_TEST_PersistenceRoundTripSuite.c`
  - Estimate: 🔴 2.5 h

- [x] **3.3 — Prove T1 and T3 red once each**
  - Description: T1 — swap two table entries, or move one id between the retired and surviving sets. T3 — break the id write (`""`) or the id read (resolve by index). Record **exact failure text, breaking method and date** in `context.md`. **No `maxAttempts`, ever.**
  - File(s): `context.md`
  - Estimate: 🟡 1 h

- [x] **3.4 — Run the v1 fixture check**
  - Description: ⚠️ **The recipe written here was broken and was replaced** — see `implementation.md` §7 and `context.md` §"The v1 fixture check" for the corrected staging script and why `activate_save.sh` structurally cannot reach a local-mode server's save tree. Read the log for (a) **one WARNING per retired job, naming it**, and (b) the survivors restored on their **correct** configs. A documented manual check, **not** a group member — the groups reset save state. **PASSED**: exactly three named drops, `2 of 4` board entries and `2 of 4` global counters carried, both survivors on their own configs — and it went through the **real `SaveGameManager` continue path**, which is more than the plan asked for.
  - File(s): (verification) — log excerpts into `context.md`
  - Estimate: 🟡 1 h

- [x] **3.5 — Re-run 3.4 against the same fixture**
  - Description: Confirms the re-apply path is still idempotent (D6).
  - File(s): (verification)
  - Estimate: 🟢 30 min

- [x] **3.6 — Green gate**
  - Description: **Measured Fast 54 / All 92**, compile-check exit 0. Phase delta **+1 / +2**; feature delta **+2 / +3** against task 0.2's 52 / 89. The plan's "+3 to both" assumed a stale baseline *and* that all three cases sat in shared tiers — T3 is in the All group only, so All moved by 2 this phase and Fast by 1. Re-derived, not assumed.
  - File(s): (verification)
  - Estimate: 🟢 20 min

- [x] **3.7 — Write the phase report**
  - Description: What is proven automatically, what only the fixture check proves, what only the user can prove (§7's four structural gaps: the real quit-and-continue path, MP, UI, a real campaign's save).
  - File(s): `context.md`
  - Estimate: 🟢 30 min

- [x] **3.8 — Decision gate**
  - Description: **Only if 3.4 and 3.6 pass does Phase 4 begin.** This gate is not advisory (Quality Bar §6). **BOTH PASSED — Phase 4 is CLEARED.** Reasoning and evidence: `context.md` §"2026-08-09 — Phase 3 complete".
  - File(s): (gate)
  - Estimate: 🟢 10 min

**Acceptance:** three new cases, each proven red · the v1 fixture restores correctly with named drops · idempotency re-confirmed · both groups green.

---

## Phase 4: Delete (8/8 complete ✅) — `component-developer`

- [x] **4.1 — Remove the five entries from `m_aJobConfigs` on the game-mode prefab**
  - Description: `findGunDealer`, `findShop`, `placeEquipmentBox`, `recruitACivilian`, `placeACamp` — positions 2, 4, 5, 6, 7 of twelve. ⚠️ R7: this is one of the two busiest shared files in the repo; re-check `git status` first.
  - File(s): `Prefabs/GameMode/OVT_OverthrowGameMode.et`
  - Estimate: 🟢 30 min

- [x] **4.2 — Delete the entire `m_aJobConfigs` block from the Eden layer**
  - Description: §3.6 / D7 — correct or neutral under every candidate override semantic, and it *must* be reconciled regardless since after 4.3 it would reference five resources that no longer exist.
  - File(s): `Worlds/MP/OVT_Campaign_Eden_Layers/managers.layer`
  - Estimate: 🟢 20 min

- [x] **4.3 — Delete the five `.conf` and `.conf.meta` files**
  - Description: `Configs/Jobs/{findGunDealer,findShop,placeEquipmentBox,recruitACivilian,placeACamp}.conf` and their metas.
  - File(s): `Configs/Jobs/` (10 files deleted)
  - Estimate: 🟢 15 min

- [x] **4.4 — Repo-wide grep for the five conf names and their five GUIDs**
  - Description: **Zero** hits outside `docs/` (F4). Re-run R4's `jobIndex` grep in the same pass.
  - File(s): (verification)
  - Estimate: 🟢 20 min

- [x] **4.5 — Activate T2's "retired legacy ids resolve to nothing" assertion**
  - Description: It would have been red in Phases 1-3 by design. Confirm it goes green **here**, and that the transition is explicit in the code rather than accidental.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c`
  - Estimate: 🟢 30 min

- [x] **4.6 — Reword `OVT_GetRadioTowerLocationJobStage.c:5`**
  - Description: It cites *"the same shape as OVT_GetDealerLocationJobStage in findGunDealer"* — the class survives, the job does not. Cite the **class**, not the vanished job (D8).
  - File(s): `Scripts/Game/Configuration/Jobs/OVT_GetRadioTowerLocationJobStage.c` (path to confirm)
  - Estimate: 🟢 15 min

- [x] **4.7 — Verify Eden loads**
  - Description: `tools/launch-server.sh --scenario eden`; log shows **7** configs and **no missing-resource error** (F5). Headless.
  - File(s): (verification)
  - Estimate: 🟡 45 min

- [x] **4.8 — Green gate**
  - Description: compile-check clean; both groups green; the Init log still reports the tutorial manager's **eleven** entries (nothing here touches them — I3 also wants `git diff --stat Configs/Tutorials/ Configs/FieldManual/` empty).
  - File(s): (verification)
  - Estimate: 🟢 30 min

**Acceptance:** five confs and ten prefab/layer lines gone · zero dangling references · Eden loads with 7 jobs · both groups green.

---

## Phase 5: Records, bugs, epic close (7/7 complete ✅) — `component-developer`

- [x] **5.1 — Retire the ten `.st` items in place**
  - Description: D9 — each `Comment` becomes a retirement record naming the **date, this feature, the removed job and the reason for retention**. **No `Text` edited, no item deleted, no `.lang.conf` opened.** Items at `.st:3823, 3845, 3879, 3900, 3987, 3995, 4023, 4031, 4157, 4165` (re-locate; line numbers drift).
  - File(s): `Language/localization_Overthrow.st`
  - Estimate: 🟡 1 h
  - ✅ **DONE 2026-08-09.** All ten located by id (the plan's line numbers were still exact). ⚠️ **Two shapes, not one:** four items carried `Comment ""` and were replaced; the other six are short-form items with **no `Comment` key at all** and needed one inserted before `Modified` — hence a `12 insertions / 6 deletions` diff rather than ten replacements. **Q3 verified by measurement:** `git diff -U0 Language/` contains zero changed lines that are not `Comment` lines.

- [x] **5.2 — Correct the two forward-pointing Comments**
  - Description: `OVT-IntroHint`'s claim 3 (`.st:3542`) becomes past tense; the welcome page-3 comment (`.st:11021`) — *"The menu's Jobs entry is deliberately NOT named: the starter-jobs-retirement feature changes it"* — is **now false** and is corrected to say the **Jobs menu entry survives** and the omission is purely length (D12, I6).
  - File(s): `Language/localization_Overthrow.st`
  - Estimate: 🟢 30 min
  - ✅ **DONE 2026-08-09.** `OVT-IntroHint` claim 3 → past tense and dated. The welcome page-3 Comment was the genuinely **false** one and now says the **Jobs menu entry survives**: verified, not assumed — `UI/Layouts/Menu/MainMenu.layout:284, :292` is byte-unchanged and `git diff --stat UI/` is empty, so `welcome-intro-3-ui.edds` stays true (**I6 / D12 discharged by inspection**).

- [x] **5.3 — Update `BUG-037.md` and `BUG-040.md`**
  - Description: They stay `closed`, with a dated note that the five jobs they described were subsequently **removed** by this feature and that the fixes remain live for the surviving configs. **Do not claim this feature closed them** (D11).
  - File(s): `docs/bugs/BUG-037.md`, `docs/bugs/BUG-040.md`
  - Estimate: 🟢 30 min
  - ✅ **DONE 2026-08-09.** Both stay `closed`; both carry a dated removal note that explicitly does **not** claim closure. Fix citations re-verified on the post-removal tree and **corrected for drift**: `OVT_JobManagerComponent.c:638-639` (was `:543`), `:565-569`, `:1079-1086`, `:1091`. BUG-037's note states the true reward figure — **$100, 30 XP, two field dressings** — with its derivation cited.

- [x] **5.4 — De-stale the epic docs**
  - Description: The BUG-037 framing in `epic-overview.md` (Purpose, the #5 row, the Build Order line, the dependency bullet) and in `epic-requirements.md` (the last Requirements bullet) all assert a closure that will not happen. Correct all five sites.
  - File(s): `docs/features/new-player-experience/epic-overview.md`, `epic-requirements.md`
  - Estimate: 🟡 45 min
  - ✅ **DONE 2026-08-09.** All four `epic-overview.md` sites plus the last `epic-requirements.md` bullet, and three more that carried the same false framing: the stale `welcome-intro-3-ui` "must be re-shot" liability (twice), the wrong reward total, and the 4/5-built header and rollup with pre-feature counts. `epic-requirements.md`'s Overview and Planned-Features lines were corrected too — BUG-037 was fixed **one day before that file was created**, so "MP-broken" was stale the day it was written.

- [x] **5.5 — Record in the jobs epic**
  - Description: Tick the positional-`jobIndex` tech-debt item as **discharged**; add the **six orphaned-but-kept classes** (D8) with their rationale so the next reader knows they are unexercised by shipped content.
  - File(s): `docs/features/jobs/epic-overview.md`
  - Estimate: 🟢 30 min
  - ✅ **DONE 2026-08-09.** Tech-debt item ticked **DISCHARGED**, naming `m_sId`, the v2 format, `LEGACY_V1_JOB_IDS` (`OVT_JobManagerSerializer.c:194-207`) and the two save-boundary translation points — and recording that the item's own suggested fix (the config resource name) was rejected by D1. The six orphans were **re-verified by grep before being written down**; each returns only its own definition. Two already-resolved jobs-epic debt items were ticked in the same pass.

- [x] **5.6 — Write `context.md`**
  - Description: The F3 answer, the proven-red table, the fixture recipe, the v1 policy, and the two residual gaps accepted (directed discovery, recruit availability — D13).
  - File(s): `context.md`
  - Estimate: 🟡 45 min
  - ✅ **DONE 2026-08-09.** `context.md` now carries the F3 answer, the five-row proven-red table, the corrected fixture recipe, the **v1 support policy with its concrete removal trigger**, and the **two accepted residual gaps** (directed discovery — moot, the map marks every shop and dealer ungated; recruit availability — `recruits-first-recruit` fires on the first recruit *gained*, D13).

- [x] **5.7 — State the string-export position explicitly**
  - Description: Only `Comment` fields changed, no rendered text, **so no Workbench re-export is required by this feature**. If the user runs one anyway, `first-spawn`'s owed `OVT-FieldManual_Welcome_Text2` lands in the same pass.
  - File(s): `context.md`
  - Estimate: 🟢 10 min
  - ✅ **DONE 2026-08-09.** **No Workbench re-export is required by this feature** — `Comment` fields only, no rendered text. If the user runs one anyway, `first-spawn`'s owed `OVT-FieldManual_Welcome_Text2` lands in the same pass.

**Acceptance:** ten items retained with retirement records · two false forward-references corrected · bug files accurate · both epic docs de-staled · `git diff --stat Language/` lists `localization_Overthrow.st` **and nothing else**.

---

## Phase 6: Help and documentation sync (5/5 complete ✅) — **`help-docs-sync`**

*Player-facing behaviour changes, so this phase is required.*

- [x] **6.1 — Confirm zero in-game help references jobs**
  - Description: Grep `Configs/Tutorials/` and `Configs/FieldManual/`. Expected outcome: **no in-game help edits**, which is a legitimate result, not a skipped task. Record it.
  - File(s): (read-only)
  - Estimate: 🟢 15 min
  - ✅ **DONE 2026-08-09.** `grep -ril "job" Configs/Tutorials/ Configs/FieldManual/` returns **nothing** (exit 1). `git diff --stat` on both directories is **empty** — DoD **I3** measured, not asserted. **No in-game help edit was needed; that is the result.** No `.st` item was touched either, so Phase 5's position holds: **no Workbench re-export is required by this feature.**

- [x] **6.2 — Wiki `getting-started`: remove the two starter-jobs mentions**
  - Description: The `**Tutorial Jobs**:` paragraph under `### 1. Jobs System`, and item 6 under `## Systems Worth Knowing About`. The heading `### 1. Jobs System` **stays** — it still reads correctly with the generic jobs description alone. ⚠️ **pageId 2, but resolve by slug and confirm the returned `path` before writing** — search returns wrong pageIds.
  - File(s): wiki `getting-started` (pageId 2)
  - Estimate: 🟡 45 min
  - ✅ **DONE 2026-08-09.** Resolved **by slug**: `getting-started` → `pageId 2`, `path: "getting-started"` — the handoff's id was right, but it was confirmed before writing rather than trusted. Both mentions removed verbatim: the `**Tutorial Jobs**: Five starter jobs introduce the basics…` paragraph, and item **6** `**Tutorial Jobs**: A set of starter jobs that introduce these features`. `### 1. Jobs System` **kept** and still reads correctly for the seven survivors; items 1-5 stayed contiguous so no renumbering was needed. Nothing else on the page changed. `tutorial-content`'s owed tip-system paragraph was **already applied** and survived intact.

- [x] **6.3 — Do not touch the `v1_3` release note**
  - Description: "3 new tutorial jobs" is historically accurate (the three added *in v1.3* were `placeEquipmentBox`, `recruitACivilian`, `placeACamp`). Confirm it is unchanged.
  - File(s): wiki `v1_3` (read-only)
  - Estimate: 🟢 10 min
  - ✅ **CONFIRMED UNCHANGED 2026-08-09** (read-only). `pageId 32`, `lastModified 2025-07-07T09:33:37.392Z`. "3 new tutorial jobs" is historically accurate — `placeEquipmentBox`, `recruitACivilian` and `placeACamp` were the three added *in v1.3*; `findGunDealer` and `findShop` predate it.

- [x] **6.4 — Do not regress `field-manual`'s already-correct pages**
  - Description: The public `wanted-system` and `base` pages are **already right** and must stay right.
  - File(s): wiki (read-only)
  - Estimate: 🟢 15 min
  - ✅ **CONFIRMED UNCHANGED 2026-08-09** (read-only). `wanted-system` `pageId 24`, `lastModified 2026-08-08T08:18:05.688Z`; `base` `pageId 11`, `lastModified 2026-08-08T08:39:56.138Z` — both still yesterday's `field-manual` pass, so nothing regressed.

- [x] **6.5 — Verify the rendered page, not the stored source**
  - Description: `pages.update` needs `tags` passed and can report `succeeded: false` **after having written**; a failed update leaves the render stale. Re-read over HTTP and confirm.
  - File(s): (verification)
  - Estimate: 🟢 30 min
  - ✅ **DONE 2026-08-09.** Verified over HTTP against the **rendered** page: `grep -ic "tutorial job"` = **0**, `grep -ic "starter job"` = **0**, `1. Jobs System` still present, all eighteen landmark headings present exactly once, 33,885 bytes — so the whole-document write neither truncated nor duplicated. **None of the three recorded MCP hazards reproduced** this session: the connection was authenticated, `pages.single` resolved by slug, and the update reported `status: updated` honestly. The MCP wrapper exposes no `tags` argument and the write still landed and rendered.

**Acceptance:** both paragraphs gone from the **rendered** page · `v1_3` untouched · `wanted-system` and `base` unchanged · no in-game help edit needed, and that is recorded.

---

## Play-test checklist (the user — after Phase 6) — ✅ **ALL PASSED, signed off 2026-08-09**

Automation structurally cannot cover the real quit-and-continue path, multiplayer, UI, or a real campaign's save (§7). **All eleven checks below — U1-U6 single player and M1-M5 multiplayer — were run by the user and passed on 2026-08-09.** That closes the feature.

> **M1-M5 passing is worth more than this feature.** The epic recorded that **two-client per-player isolation had never been observed passing**. It has now been observed on the **jobs** surface: M3 (client A completes a job, A gets the hint and the reward, **B does not get the hint**) is a direct observation of BUG-040's owner-filtered send working with two real clients, and M4 is a direct observation of the job board surviving JIP through `RplSave`/`RplLoad` — the one replication path this feature deliberately did not touch, which is exactly why it is worth having watched. ⚠️ **Do not over-read it:** this exercises the *job completion* delivery path, **not** `tutorial-system`'s popup delivery path. That framework's own F7 (two clients, tutorial popups) remains unobserved. What M1-M5 establishes is that owner-targeted delivery and JIP restore both work *here*, which is strong circumstantial support for the framework and not a substitute for testing it.

### Single player

| # | Step | Expect | Result |
|---|---|---|---|
| **U1** | ⚠️ **Back the save up first.** Take an existing campaign save from **before** this build and **Continue** it | The campaign resumes. Jobs on the board are the right jobs. The log carries a WARNING per dropped starter job, naming it. **The single most important observation in the feature** | ✅ **PASS** |
| U2 | Overthrow menu → Jobs | Seven job types possible; none of the five starter jobs; list renders, accept and decline work | ✅ **PASS** |
| U3 | Accept a job, complete it | Correct reward paid, completion hint shows to **you only**, job leaves the board | ✅ **PASS** |
| U4 | Save, quit, Continue again | Board and counters identical; nothing duplicated | ✅ **PASS** |
| U5 | Start a **fresh** campaign | No starter jobs ever appear; the seven survivors offer normally; tutorial popups still fire on first buy / first place / first recruit | ✅ **PASS** |
| U6 | Open the map | Every shop and every gun dealer still marked (this is what makes the lost "directed discovery" acceptable) | ✅ **PASS** |

### Multiplayer — the epic's biggest outstanding question

⚠️ **A client launch opens a window on the user's desktop and can orphan.** The server binary is genuinely headless. **Always pass a long `--timeout`** — it defaults to 600 s and will kill the client mid-test.

| # | Step | Expect | Result |
|---|---|---|---|
| M1 | Both clients join, open Jobs | Same seven-type board; no client shows a starter job | ✅ **PASS** |
| M2 | Client A accepts a public job | Both boards update; A owns it; B still sees it correctly | ✅ **PASS** |
| M3 | A completes it | A gets the hint and the reward; **B does not get the hint** (BUG-040's fix, still live) | ✅ **PASS** |
| M4 | **JIP:** with jobs on the board, connect B fresh | B's board matches A's — exercises `RplSave`/`RplLoad`, the one path the id change deliberately did not touch | ✅ **PASS** |
| M5 | Save on the server, restart, both reconnect | Board and counters survive; no job attached to the wrong config | ✅ **PASS** |
