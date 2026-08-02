# Test Coverage - Task Checklist

**Last Updated:** 2026-08-02 04:20
**Progress:** 61/62 tasks complete (98%) — task 4.10 is OPTIONAL and deliberately deferred
**Epic:** dev-ops (feature #3 of 5)
**Advanced phases:** Phase 1 (ADVANCED opus, requires Bash-capable agent) and Phase 4 (ADVANCED opus, requires Bash). Phases 2/3/5/6/7 STANDARD (high); all except 2 and 7 need Bash for `run-tests.sh` verification.

> Task numbering mirrors `implementation.md` — read the matching phase there for full detail and acceptance criteria before starting a task. Phase 1 ends with a **GATE**: amend Phases 2-7 in place against `findings.md` before proceeding.

---

## Phase 1: Empirical spike — settle the unknowns (12/12 complete) ✅ — **ADVANCED (opus), requires Bash**

- [x] ✅ **1.1 Create `findings.md`** — #2's shape: build stamp, experiment table, "Differs from assumptions", "Bugs found (log only)"
  - File(s): `docs/features/dev-ops/test-coverage/findings.md`
  - Estimate: 🟢 Small
  - Result: `docs/features/dev-ops/test-coverage/findings.md` written (build 1.7.0.54 / engine 190965).
- [x] ✅ **1.2 World-free tier feasibility** — throwaway suite overriding `GetWorldFile()` → empty; compile-check; if it compiles, run and record whether the scenario change is absent
  - Estimate: 🟢 Small
  - Result: **FEASIBLE.** `GetWorldFile()` -> `ResourceName.Empty` compiles and runs; 0 scenario changes; harness runs once; **8 s**.
- [x] ✅ **1.3 Campaign start probe** — set `m_Difficulty = m_aDifficultyPresets[0]`, `DoStartNewGame()` + `DoStartGame()`, poll `IsInitialized()`; record timings, console output, start-menu interference
  - Estimate: 🟡 Medium
  - Result: **WORKS.** Same-frame `HasGameStarted`/`IsInitialized`; 71 ms from a suite Setup step. **`m_aDifficultyPresets[0]` is 'Easy', not 'Test World' (index 4) — select by name.**
- [x] ✅ **1.4 Post-start settling budget** — when do shops/bases/garrisons exist; confirm 5 s / 10 s deferrals; produces Phase 4-5 `timeoutS` values
  - Estimate: 🟢 Small
  - Result: managers/shops/bases settle <=604 ms; resource distribution ~6.5 s; deployments ~12 s. `timeoutS` 30 / 45 / 60.
- [x] ✅ **1.5 Stale-singleton check** — across the run's three world loads, does `OVT_Global.GetTowns()` == `FindComponent` on the live game mode
  - Estimate: 🟢 Small
  - Result: **stale singletons do NOT manifest** — the engine nulls the weak statics; `OVT_Global` == `FindComponent` in 100% of samples. Helper is defensive only.
- [x] ✅ **1.6 Base-class step ordering** — does a Setup step on `OVT_TEST_SuiteBase` run after inherited `Setup_AwaitWorld`, before derived steps
  - Estimate: 🟢 Small
  - Result: base-class Setup step runs **after** all BI steps (world loaded) and **before** derived-suite and case steps.
- [x] ✅ **1.7 Persistence reality check** — call `GetPersistence().SaveGame()` with campaign started; record prints, disk writes, `HasSaveGame()`; grep for EPF init. **Selects the ladder rung** (expected L3)
  - Estimate: 🟡 Medium
  - Result: **rung L3.** `SaveGame()`/`AutoSave()` return silently (m_PersistenceSystem null), `HasSaveGame()` false, zero disk writes, zero EPF init.
- [x] ✅ **1.8 Save-directory determination** — exact WSL path of the `OverthrowCI` profile's `.db/Overthrow`; record vanilla `SaveGameManager` location too
  - Estimate: 🟢 Small
  - Result: `<My Games>/OverthrowCI/**profile**/.db/Overthrow` — one level deeper than the plan predicted. `$saves:` is not script-writable.
- [x] ✅ **1.9 In-session reload survivability** — only if 1.7 finds a save path; decides L1 vs L2
  - Estimate: 🟡 Medium
  - Result: **N/A** — gated on 1.7 finding a save path; none exists. (Noted: the script VM does survive an in-session world transition, so L1 is the likely rung if one ever appears.)
- [x] ✅ **1.10 Group config proof** — hand-author `OVT_TestGroup_Probe.conf` + `.meta` (16-hex GUID, collision-checked); prove `{GUID}` runs 1 then 2 suites; record extra transition cost
  - File(s): `Configs/Tests/OVT_TestGroup_Probe.conf` (throwaway)
  - Estimate: 🟡 Medium
  - Result: **works.** Hand-authored `.conf`+`.meta` GUID registered first try; **`[BaseContainerProps()]` on each concrete suite class is MANDATORY**; 2 suites ran; +1 s / +1 transition per extra suite; execution order is alphabetical, not config order.
- [x] ✅ **1.11 Timing baseline** — wall times: Tier-A-shaped, Tier-B-shaped, campaign-start suite, two-suite group
  - Estimate: 🟢 Small
  - Result: Tier A 8 s, Tier B 17 s, campaign 16 s (30 s with a 16 s observation window), 2-suite group 16-17 s.
- [x] ✅ **1.12 Delete probe code, compile clean, write up** — compile-check exit 0; "Differs from assumptions" written
  - Estimate: 🟢 Small
  - Result: all probe code + probe group config deleted; `git diff` over `Scripts/`, `Configs/`, `tools/`, `.scripts/` is empty; `compile-check` exit 0; smoke suite still exit 0.

> **GATE APPLIED 2026-08-02** — Phases 2-7 amended in `implementation.md` (Tier A world-free; rung L3; save path `<My Games>/OverthrowCI/profile/.db/Overthrow`; difficulty preset by NAME; `[BaseContainerProps()]` mandatory on suite classes; alphabetical group order; `timeoutS` 30/45/60; R3/R5/R8 resolved).

---

## Phase 2: Save-state control (7/7 complete) ✅ — STANDARD (high), Bash only

- [x] ✅ **2.1 `backup_save.sh [<name>]`** — name from `$1` skips prompt; no argument keeps prompt
  - File(s): `.scripts/backup_save.sh`
  - Estimate: 🟢 Small
  - Result: `backup_save.sh [--profile <name>] [<name>]`. With `$1` the suggestion line **and** prompt are skipped (proven with stdin `/dev/null`); same sanitisation, same `.saves/` location, same archive naming.
- [x] ✅ **2.2 `activate_save.sh [<name-or-file>]`** — exact file, else newest matching archive; unmatched → non-zero + list; no argument keeps menu
  - File(s): `.scripts/activate_save.sh`
  - Estimate: 🟢 Small
  - Result: resolves exact path → exact filename in `.saves/` → newest `*<arg>*.tar.gz` by mtime. Unmatched → exit 1 + the numbered list, without resetting. Menu unchanged. **Fixed a latent bug**: `saves=($(find …))` word-split on the repo path's space for absolute invocations (see findings).
- [x] ✅ **2.3 Destructive-path guard in `reset_save.sh`** — refuse empty / `/` / non-save-shaped path; print resolved path before deleting. Highest-value change in the phase
  - File(s): `.scripts/reset_save.sh`
  - Estimate: 🟢 Small
  - Result: refuses empty, `/`, relative, and any path not ending in `.db/Overthrow` (exit 1, nothing deleted); trailing slashes accepted. `Resolved save directory: <path>` is printed on **every** run before anything is touched. A set-but-empty `OVERTHROW_SAVE_DIR` is no longer silently replaced by the Workbench default (DoD item 14).
- [x] ✅ **2.4 `--profile <name>` on all three** — resolve `<My Games>/<name>/`**`profile`**`/.db/Overthrow` via `tools/lib/common.sh`'s `ovt_profile_dir` (gate ruling, finding 1.8 — one level deeper than this line originally said); explicit `OVERTHROW_SAVE_DIR` still wins
  - File(s): `.scripts/*.sh`
  - Estimate: 🟡 Medium
  - Result: verified against a fake `My Games` (`OVERTHROW_MYGAMES_DIR`) and print-only against the real one — `--profile OverthrowCI` → `/mnt/c/Users/Aaron Static/OneDrive/Documents/My Games/OverthrowCI/profile/.db/Overthrow`. `OVERTHROW_SAVE_DIR` wins and says so. `tools/` untouched.
- [x] ✅ **2.5 Exit codes** — 0 success, non-zero + message on every failure path
  - Estimate: 🟢 Small
  - Result: 0 success (including "nothing to delete"), 1 usage/refusal/missing-save-dir/unmatched/failed-op, 2 `--profile` unresolvable (passed through from `common.sh`). Documented in each script header. No silent success anywhere.
- [x] ✅ **2.6 Verification matrix by execution** — against a THROWAWAY directory only; record in `findings.md`
  - Estimate: 🟡 Medium
  - Result: 30 scenarios (W1-W6, P1-P5, U1-U5) run against `/tmp/ovt-save-probe/.db/Overthrow` and a fake `My Games`; recorded in `findings.md` → "Phase 2: save-script verification matrix". Real saves untouched; the three test archives were removed and `.saves/` is byte-identical before/after.
- [x] ✅ **2.7 Interactive paths unchanged** — no-argument behaviour identical to before
  - Estimate: 🟢 Small
  - Result: old vs new scripts diffed with identical stdin from identical fake repos. Only delta anywhere: reset's new `Resolved save directory:` line. Both `read -p` prompt strings are character-for-character unchanged.

---

## Phase 3: Suite base extension + Tier B Init suite (10/10 complete) ✅ — STANDARD (high), requires Bash

- [x] ✅ **3.1 Extend `OVT_TEST_SuiteBase`** — `RequiresStartedCampaign()` virtual (default false); guarded campaign-start Setup step; manager-resolution helper per finding 1.5
  - File(s): `Scripts/Game/Tests/TestFramework/OVT_TEST_SuiteBase.c`
  - Estimate: 🟡 Medium
  - Result: opt-in virtual + `Setup_StartCampaign()` (preset by NAME → `DoStartNewGame()`+`DoStartGame()` guarded by `!HasGameStarted()` → `CloseLayout()` every poll → `IsInitialized()`, 600-poll diagnostic backstop) + `FindDifficultyPreset()` + `ResolveManager()` (documented DEFENSIVE ONLY). Header states the campaign start is a test concern shipped code must never rely on. **The true branch was exercised for real** (temporary override, reverted): `preset selected by name = 'Test World'` / `initialized after 0 poll(s)`.
- [x] ✅ **3.2 Smoke + Meta regression check** — `run-tests.sh` → 0, `run-tests.sh OVT_TEST_MetaSuite` → 1
  - Estimate: 🟢 Small
  - Result: **0 / 1**, checked twice — immediately after the base-class change and again on the final tree. Smoke and Meta were not modified.
- [x] ✅ **3.3 Create `OVT_TEST_InitSuite`** (`RequiresStartedCampaign()` = false)
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c`
  - Estimate: 🟢 Small
  - Result: created with `[BaseContainerProps()]` (mandatory for Phase 6 group membership, finding 1.10); header carries the tier rule and the no-magic-counts rule.
- [x] ✅ **3.4 Case: OVT_Global getter sweep** — every non-player-dependent getter non-null; failure names the first null; excludes GetServer/GetUI/GetController/GetContainerTransfer
  - Estimate: 🟡 Medium
  - Result: `OVT_TEST_Init_Globals_ManagersResolve` — all **18** getters resolved; ordered so `GetConfig()` is proven before `GetDifficulty()` dereferences it.
- [x] ✅ **3.5 Case: towns populated** — `Count() >= 1`, first town has population + location; no magic counts
  - Estimate: 🟢 Small
  - Result: `OVT_TEST_Init_Towns_ArePopulated` — 1 town, population 50 at `<208.237, 1, 102.173>`.
- [x] ✅ **3.6 Case: controllers registered** — ≥1 town + ≥1 base controller reachable; town controller resolves into `m_Towns`
  - Estimate: 🟢 Small
  - Result: `OVT_TEST_Init_Controllers_AreRegistered` — controller `'Town'` resolves to town id 0; 1 base, `GetBaseByIndex(0)` non-null. Both registrations happen at Init, not at campaign start — which is why this is legitimately Tier B.
- [x] ✅ **3.7 Case: economy seeded** — `SetPrice`/`SetDemand` → `GetPrice`/`GetDemand` round-trips; `GetBuyPrice` applies profit margin
  - Estimate: 🟡 Medium
  - Result: `OVT_TEST_Init_Economy_PriceAndDemandSeams` — synthetic IDs (no real item disturbed); pins the documented unknown-key defaults (500 / 5); buy price 1250 on a base of 1000 with `m_fShopProfitMargin` 0.25, expectation derived from the config, plus the independent claim `buy > base`.
- [x] ✅ **3.8 Fresh-campaign precondition proof** — reset via `OVERTHROW_SAVE_DIR=<CI path> reset_save.sh`, verdict unchanged; wiring proven
  - Estimate: 🟢 Small
  - Result: `.scripts/reset_save.sh --profile OverthrowCI` → exit 0, resolved path printed, `Nothing to delete` (the CI profile still has no `.db` at all). Init suite exits **0 with and without** the reset, as expected while `HasSaveGame()` is hardcoded false. Workbench profile never touched.
- [x] ✅ **3.9 Can-fail proof for every case** — perturb, observe exit 1, revert; record method + failure text
  - Estimate: 🟡 Medium
  - Result: 4 perturbation runs, all exit **1**, each `autotest_failed.log` naming exactly the perturbed case. Method + verbatim failure text recorded in findings.md → "Can-fail proofs". No probe code remains.
- [x] ✅ **3.10 Gates** — compile-check 0; `run-tests.sh OVT_TEST_InitSuite` → 0 three consecutive runs
  - Estimate: 🟢 Small
  - Result: `compile-check` **0**; three consecutive Init runs **0 / 0 / 0**, 4 testcases and 0 failures each, 14 s tool time each.

---

## Phase 4: Persistence — Tier D green + Tier D' quarantined gate (9/10 complete, 4.10 optional/not started) — **ADVANCED (opus), requires Bash**

- [x] ✅ **4.1 Create `OVT_TEST_PersistenceSuite`** (`RequiresStartedCampaign()` = true); header states the assertion rule verbatim
  - File(s): `Scripts/Game/Tests/TestSuites/Persistence/OVT_TEST_PersistenceSuite.c`
  - Estimate: 🟢 Small
  - Result: created with `[BaseContainerProps()]` + `RequiresStartedCampaign()` = true. Header carries the assertion rule, the covered/deferred state-kind tables, the run recipe and the tier house rules. **Deviation, deliberate:** the rule is quoted with its three type-name tokens replaced by descriptions (pointing at implementation.md for the verbatim text) because quoting them literally tripped the DoD grep — see findings.md observation 6.
- [x] ✅ **4.2 Same-session round-trip cases** — town control/stability/population, player money, skills/XP/level, real estate ownership, recruits; public API only
  - Estimate: 🔴 Large
  - Result: **8 cases, all green.** money (`AddPlayerMoney`/`TakePlayerMoney` → `GetPlayerMoney`/`PlayerHasMoney`, runtime ID in / persistent ID out), skills+XP (`GiveXP`/`TakeXP`/`AddSkillLevel`, skill key read from config), real estate (`SetOwnerPersistentId`/`SetOwner(-1)` → three independent accessors), recruits (`AddRecruit`/`AddRecruitXP`/`RemoveRecruit` → `GetRecruit`/`GetRecruitCount`/`GetPlayerRecruits`), town control (`ChangeTownControl`), town support (`AddSupport`/`ResetSupport`), town population and town stability via documented closest seams. Subject resolution shared in `OVT_TEST_PersistenceSubject.c`; persistent ID resolved through the players manager (playerId 1, no polling needed — the player is set up ~18 ms before the campaign-start Setup step).
- [x] ✅ **4.3 Document deferred state kinds** — vehicles, structures, container inventories (stubbed), held items (disabled), loadouts — with reasons
  - Estimate: 🟢 Small
  - Result: recorded in the Tier D suite header and in findings.md → "Deferred state kinds, with cause" (+ garrisons, which never populate in the test world). No persistence type names used anywhere in the test tree, including comments.
- [x] ✅ **4.4 Create `OVT_TEST_PersistenceRoundTripSuite`** — save+reload per the Phase 1 rung; capitalised quarantine warning
  - File(s): `Scripts/Game/Tests/TestSuites/Persistence/OVT_TEST_PersistenceRoundTripSuite.c`
  - Estimate: 🔴 Large
  - Result: **9 cases** (the 8 state kinds + the capability gate), `[BaseContainerProps()]` so it is addressable, in **no** group. Capitalised quarantine header in the Meta-suite style, stating exit 1 → exit 0 as the migration's acceptance criterion. The one permitted `SaveGame()` trigger lives in a single annotated line in `OVT_TEST_PersistenceRoundTripGate`. The rung-L1 reload (re-request the world through the framework helper, await the transition, require a restored started campaign) is implemented rather than stubbed, so the suite can flip to green on its own — documented as UNREACHABLE and UNTESTED today.
- [x] ✅ **4.5 Diagnostic failure** — missing save path fails with a message naming the capability, not a null deref or timeout
  - Estimate: 🟡 Medium
  - Result: every case fails with `Persistence capability absent: SaveGame() produced no save (HasSaveGame() still false). The vanilla-persistence migration is not complete.` — one line in `junit.xml`, no source reading required. Recorded verbatim in findings.md.
- [x] ✅ **4.6 Anti-vacuous-pass guard** — impossible to go green without persistence actually working
  - Estimate: 🟡 Medium
  - Result: five adversarial closures, tabulated in the suite header and findings.md. The two load-bearing ones: the capability case asserts the whole **transition** (no save before → a save after, which needs the `reset_save.sh` precondition), and every state-kind case **dirties** the value between saving and reloading so that a reload restoring nothing cannot pass. The second closure does not use `HasSaveGame()` at all, so it survives the first being defeated. No case depends on execution order.
- [x] ✅ **4.7 Run recipe documented** — suite header + `tools/README.md`: save-state precondition, command, expected exits
  - Estimate: 🟢 Small
  - Result: acceptance procedure in both suite headers and in a new `### Persistence acceptance gate (vanilla-persistence)` subsection of `tools/README.md` (exit-code table, why the reset is mandatory, the never-without-`--profile` warning, pointer to Phase 7's full save-state section). `tools/run-tests.sh` is byte-identical (md5 unchanged).
- [x] ✅ **4.8 Can-fail proof for every green case**
  - Estimate: 🟡 Medium
  - Result: **8 perturbations, 8 exit-1 runs**, each `autotest_failed.log` naming exactly the perturbed case and the other seven staying green. Every perturbation drives a real detection path (a player ID that does not exist, the opposite seam, a wrong persistent key, a recruit ID that resolves to nothing, one supporter more than the town has, a modifier the town does not hold) rather than inverting a comparison. Method + verbatim failure text in findings.md → "Can-fail proofs".
- [x] ✅ **4.9 Gates** — `PersistenceSuite` → 0 (×3); `PersistenceRoundTripSuite` → 1 with diagnostic, recorded verbatim
  - Estimate: 🟢 Small
  - Result: `compile-check` **0**; three consecutive `OVT_TEST_PersistenceSuite` runs on the final tree **0 / 0 / 0**, 8 testcases and 0 failures each (15/13/13 s); `OVT_TEST_PersistenceRoundTripSuite` **1**, 9 of 9 failing with the identical diagnostic, quoted verbatim in findings.md. DoD greps: `EPF_|SCR_Persistence|SaveData` over `Scripts/Game/Tests/` → **zero** lines; `maxAttempts` / `ifdef WORKBENCH` → none.
- [ ] **4.10 (OPTIONAL — user approval required, do not start unassisted)** L4 `main`-worktree validation
  - Estimate: 🔴 Large
  - Status: **OPTIONAL — not started (needs user approval).** Off the critical path; abandoning it is an acceptable, documented result per the plan. Everything it would validate is already covered by the quarantined gate.

---

## Phase 5: Campaign logic — Tier A pure + Tier C campaign (8/8 complete) ✅ — STANDARD (high), requires Bash

- [x] ✅ **5.1 Create `OVT_TEST_LogicSuite`** — world strategy per finding 1.2; header: no manager, no game mode, no world
  - File(s): `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_LogicSuite.c`
  - Estimate: 🟢 Small
  - Result: **world-free confirmed in production shape** — `GetWorldFile()` → `ResourceName.Empty`, **0** `Requesting scenario change:` lines, one harness start, 14 cases in **6-9 s**. `[BaseContainerProps()]` present. Header states the tier rule and that the Q5 grep covers COMMENTS, so neither forbidden identifier appears anywhere in the directory. Shared `OVT_TEST_LogicFixture` (hand-built town / modifier config / epsilon compare) lives in the same file, keeping the tier to the plan's four files.
- [x] ✅ **5.2 `OVT_TEST_Logic_Town.c`** — `SupportPercentage()` boundary table, modifier `Recalculate()` (sum/clamp/identity), support-modifier deterministic branches, bounds/heat/CopyFrom
  - Estimate: 🟡 Medium
  - Result: **6 cases.** Full boundary table (0 population, partial, ==, >, no upper clamp); `Recalculate()` identity/sum/null-skip/min+max clamps/non-default floor/rounding-not-truncation; support-modifier **deterministic branches only** (> 75, < -75, summed-effect clamp, both value clamps, `max == 0` guard) with the RNG branch NAMED and skipped in a comment; `IsWithinTownBounds` (499 in / 500 out / 3D); `SetAreaHeat` lower clamp + pinned absence of an upper clamp; `CopyFrom` copies campaign state, preserves location and size, shares modifier arrays.
- [x] ✅ **5.3 `OVT_TEST_Logic_Jobs.c`** — town-support condition min/max/unset, dealer condition, random condition at 0/100
  - Estimate: 🟡 Medium
  - Result: **4 cases.** Support condition across unset (-1) / min-only / max-only / inclusive window; dealer condition set and unset; **a new bug pinned** (`DealerCondition_PinsAxisOnlyCheckBug` — the condition only inspects X, so a dealer at X = 0 reads as absent); random condition at chance 0 (never) and 100 (always), **plus** a chance beyond the roll's range so the "always" claim is provable regardless of whether `RandFloatXY`'s upper bound is inclusive, plus one zero-factor assertion per low-X multiplier.
- [x] ✅ **5.4 `OVT_TEST_Logic_Skills.c`** — each `OVT_SkillEffect` writes exactly its claimed field; permission idempotency; `OVT_PlayerData` levelling
  - Estimate: 🟡 Medium
  - Result: **4 cases.** Trade → `priceMultiplier`, stealth → `stealthMultiplier`, support → `diplomacy`, permission → `permissions`, each on a FRESH record with a spill check proving the other three fields were untouched; `GivePermission` idempotent over three applications and additive for a different permission; level curve / thresholds / `GetNextLevelXP` / `CountSkills`; level progress fractional (see 5.6). `OVT_StaminaSkillEffect` documented as intentionally inert and not asserted.
- [x] ✅ **5.5 `OVT_TEST_CampaignSuite` + `OVT_TEST_Campaign_Economy.c`** — started/initialized flags, towns activated, shop inventory, tax/donation income; `timeoutS` per finding 1.4
  - File(s): `Scripts/Game/Tests/TestSuites/Campaign/`
  - Estimate: 🔴 Large
  - Result: **4 cases, exit 0 in 13-15 s.** Flags case also asserts the difficulty preset in force is 'Test World' (catches the by-index selection trap silently); towns-activated polls for the gun dealer the controller writes back at activation (**poll 0**); shops-initialise polls for stocked inventory (**poll 1**, 5/5 shops, 286 entries); income case drives the town through four states (occupied → liberated → +supporters → lowered stability) with expectations DERIVED from live records and the difficulty config. `timeoutS` **30** everywhere — no case needed 45. No garrison, deployment or magic-count assertions.
- [x] ✅ **5.6 Pin-and-log suspected bugs** — integer-division defects etc.: pin current behaviour with honestly-named cases; log under "Bugs found (log only)"; never fix, never hide
  - Estimate: 🟡 Medium
  - Result: **The plan's integer-division defects DO NOT EXIST.** Both pinning cases failed on their first run; a throwaway probe (deleted) established that **EnforceScript picks an expression's arithmetic mode from the type it converts TO, not from its operands** — `(a/b)*100` is 50 into a float and 0 into an int. `SupportPercentage`, `GetLevelProgress` and `GetTaxIncome` are all correct; the last was **measured** at 90 stability (1125 vs 1250) because at 100 the two behaviours are indistinguishable. R7 closed. **One new bug found and pinned instead** (dealer condition X-axis, findings item 11), plus three behaviours pinned as "current" without calling them bugs (hardcoded 500 m town bounds; `CopyFrom` not copying location/size and sharing arrays; no upper clamp on area heat).
- [x] ✅ **5.7 Can-fail proof for every case**
  - Estimate: 🟡 Medium
  - Result: **19 perturbations, 19 exit-1 runs** covering all 18 cases (the income case has two). Every single-case row named **exactly** the perturbed case. Each perturbation changes a real INPUT — a town's supporters, a modifier's configured effect, a dealer position, an xp value, a faction handover, or the campaign opt-in itself — never an inverted comparison. Two campaign rows demand more than the test world contains, driving the real poll-exhaustion path. Method + verbatim failure text in findings.md → Phase 5 → "Can-fail proofs".
- [x] ✅ **5.8 Gates** — `LogicSuite` → 0 and `CampaignSuite` → 0, each ×3 identical
  - Estimate: 🟢 Small
  - Result: `compile-check` **0**; Logic **0 / 0 / 0** (14 cases, 0 failures, 8/6/7 s); Campaign **0 / 0 / 0** (4 cases, 0 failures, 14/13/14 s). Regression sweep on the final tree: Smoke **0**, Meta **1**, Init **0**, Persistence **0**. DoD greps clean — Q5 (Logic dir) **no output**, Q4 **zero lines**, `maxAttempts` / `ifdef WORKBENCH` **none**, no probe code left.

---

## Phase 6: Group configs — fast/slow contract (7/7 complete) ✅ — STANDARD (high), requires Bash

- [x] ✅ **6.1 `OVT_TestGroup_Fast.conf` + `.meta`** — Logic + Init; fresh collision-checked GUID
  - File(s): `Configs/Tests/OVT_TestGroup_Fast.conf` (+ `.conf.meta`)
  - Estimate: 🟡 Medium
  - Result: **GUID `6A6E29FF47ECB840`** (not the probe's). All 8 new GUIDs (2 config + 6 per-entry) collision-checked content (`rg --binary`) **and** filename across this repo, `ArmaReforger`, EPF, EDF, `<My Games>/…/addons` and the 56 GB game install — **zero hits everywhere**, with a positive control (`58D0FB3206B6F859` → 2 files) proving the game-install scan was real. Shapes copied byte-for-byte in form from the Phase 1 probe / `overthrowDeployments.conf.meta` (six `CONFResourceClass` blocks, LF, no trailing newline). **Registered first try** — `CLI autotest config: SCR_AutotestGroup<0x…>`, no `Invalid resource path`, no `Unknown class 'OVT_TEST_…'`.
- [x] ✅ **6.2 `OVT_TestGroup_All.conf` + `.meta`** — Logic + Init + Campaign + Persistence; NEVER Meta or RoundTrip
  - File(s): `Configs/Tests/OVT_TestGroup_All.conf` (+ `.conf.meta`)
  - Estimate: 🟢 Small
  - Result: **GUID `6A6E2A002F53A581`**, exactly the four tier suites. `OVT_TEST_MetaSuite`, `OVT_TEST_PersistenceRoundTripSuite` and `OVT_TEST_SmokeSuite` are in neither group. All four member classes verified to carry `[BaseContainerProps()]` **before** the first run (Logic:56, Init:22, Campaign:48, Persistence:82) — **no suite class was edited**.
- [x] ✅ **6.3 Verify by execution** — both GUIDs → 0 with expected cases in `junit.xml`; wall times recorded
  - Estimate: 🟡 Medium
  - Result: Fast **exit 0, 18 cases** (Logic 14 + Init 4), 0 failures, **16 s wall** / 15 s tool. All **exit 0, 30 cases** (+ Campaign 4 + Persistence 8), 0 failures, **20 s wall** / 19 s tool. Both counts match Phase 5's prediction exactly. `compile-check` **0** with the configs present.
- [x] ✅ **6.4 Leak check** — Meta + RoundTrip disabled in harness listing, zero of their cases in `junit.xml`
  - Estimate: 🟢 Small
  - Result: the All run's `Tests to run:` listing enumerates `OVT_TEST_MetaSuite: 0`, `OVT_TEST_PersistenceRoundTripSuite: 0` and `OVT_TEST_SmokeSuite: 0` — **disabled, not merely absent** — while the four members show `: 1`. `grep -c` for their case-name prefixes in `junit.xml` → **0 on all three All runs**, and `4+4+14+8 = 30` accounts for every `<testcase>`.
- [x] ✅ **6.5 Determinism** — ×3 each group: identical exits, case counts, summaries
  - Estimate: 🟢 Small
  - Result: Fast **0 / 0 / 0** — 18 cases, 0 failures every run, 16/17/15 s wall. All **0 / 0 / 0** — 30 cases, 0 failures, leak-grep 0 every run, 20/18/18 s wall. Only the duration digit varies (client boot, not test work).
- [x] ✅ **6.6 Document targets** — GUIDs in `tools/README.md` (no `run-tests.sh` change) + skill; CI usage guidance
  - Estimate: 🟢 Small
  - Result: new **`### Group targets (the fast/slow contract)`** subsection in `tools/README.md` (same style as Phase 4's Persistence-acceptance subsection) — two-row target table, commands, never-in-a-group list, CI usage (`reset_save.sh --profile OverthrowCI`; Fast every push, All nightly/pre-merge), explicit "no `OVERTHROW_TEST_TIMEOUT` needed", and the add-a-suite `[BaseContainerProps()]` procedure; the `{GUID}` row of the target-forms table now names the Fast GUID. `tools/run-tests.sh` **byte-identical** (`md5 c1b0cbb55fecf93c70b0fda77fe3c7e4`). **The skill was deliberately not touched** — it has no targets table, and four of its statements are stale in ways only 7.4 is scoped to fix wholesale.
- [x] ✅ **6.7 Record per-suite group cost** in `findings.md`
  - Estimate: 🟢 Small
  - Result: **All saves ~30-33 s** (4 launches ~49 s → 1 launch 16-19 s); Fast saves ~5-8 s. **Marginal cost of one extra suite in a group: ~+1.5 s** (refines finding 1.10's 1→2-suite figure). Client boot (~13 s) is paid once per launch, not per suite. World transitions Fast 2 / All 4 — the world-free Logic suite **contributes none**, which is also why both groups beat finding 1.11's estimate.

---

## Phase 7: Documentation — Definition of Done (8/8 complete) ✅ — STANDARD (high)

- [x] ✅ **7.1 `docs/technical-design.md` §10 + §7** — coverage position; "three dimensions" rewrite; acceptance gate noted
  - Estimate: 🟡 Medium
  - Result: §10's "real but tiny" replaced by a **six-suite / 32-case tier table** naming what each tier covers, plus the can-fail + no-`maxAttempts` policy and an explicit "still entirely manual" list (JIP/MP, UI, performance, AI movement, save/reload, `modded class`). Gate #2 of "What we do instead" now carries both group GUIDs and the save-reset precondition. **"The three dimensions that break" rewritten** as three bullets stating that only persistence is *half* automated and the other two are entirely manual. §7 gained the acceptance-gate paragraph (command, precondition, exit 1 → 0, and the no-save-path-in-either-system branch fact). Also corrected: the intro (line 7), §2's struck-through test bullet, §2's "only verification that exists", and §12's now-false "tests that pass against EPF today".
- [x] ✅ **7.2 `docs/mission-statement.md`** — "Automating the quality gate" reflects real coverage
  - Estimate: 🟢 Small
  - Result: "Two stages have landed" → three. The closing two paragraphs now say what the machine proves (30 assertions, each proven able to fail) and what it still does not — JIP/MP as the place most regressions live, UI/performance/AI, and the save/reload round-trip as *written but gated*. Same voice, no restructure.
- [x] ✅ **7.3 `tools/README.md`** — group targets, Save-state control section (all three scripts, profile trap, guard), round-trip acceptance procedure
  - Estimate: 🟡 Medium
  - Result: group targets (Phase 6) and the acceptance gate (Phase 4) were verified present and correct — **not duplicated**. Added the missing **`## Save-state control (`.scripts/`)`** section: synopses, the argument-vs-interactive table, the `OVERTHROW_SAVE_DIR` > `--profile` > default precedence with the Workbench-vs-`OverthrowCI` trap called out as a warning, the destructive-path guard, exit codes 0/1/2, `.saves/` layout + naming, automation recipes, and the EPF-`.db`-layout migration note. The Phase 4 forward-pointer ("lands in the Save-state control section, feature #3 Phase 7") now links to it.
- [x] ✅ **7.4 `workbench-workflow` skill** — tier table, campaign opt-in, manager resolution, can-fail, no-flake, group targets, save preconditions
  - File(s): `.claude/skills/workbench-workflow/SKILL.md`
  - Estimate: 🟡 Medium
  - Result: **v1.2.0 → v1.3.0.** All five stale coverage statements replaced (Testing Guidelines, Critical Constraints, Development Cycle step 5, Testing Cycle step 2, Key Differences). "Running the Autotests" now leads with both group GUIDs, the save-state precondition + the never-without-`--profile` warning, the acceptance gate, and the "never judge a run by console error counts" rule. Plus **Phase 6's two deferred corrections**: the "until someone authors an `SCR_AutotestGroup` config" sentence is gone, and the bash block has the targets. "Writing Autotests" gained a **five-row tier table with a "put a case here when it…" column**, the `RequiresStartedCampaign()` opt-in (with the by-name preset trap), the manager-resolution rule, the test-world scale limits, the `new`-applies-no-defvalues trap, the can-fail requirement, the determinism rule, and `maxAttempts` reclassified from "prefer not to" to **banned**.
- [x] ✅ **7.5 `docs/features/core/persistence/context.md`** — acceptance gate: suite name, command, precondition, exit 1 → 0 criterion; no-save-path + `.db` layout findings
  - Estimate: 🟢 Small
  - Result: Quick Status gained a **"machine-checkable definition of done"** block (exact two-command recipe, the verbatim diagnostic, exit 1 → 0, the behaviour-level assertion rule, and the on-green checklist), and Blockers gained the no-save-path entry. A dated 2026-08-02 session note records all three findings: the gate + its two anti-vacuous-pass closures that must not be weakened; **no working save path in either system** (silent `SaveGame()`, EPF never reaching SETUP, and the UI reporting success unconditionally); and the `.scripts/` tools' EPF `.db/Overthrow` assumption with the reason the replacement location is only findable empirically.
- [x] ✅ **7.6 `CLAUDE.md`** — single-smoke-test claims replaced; no-debugger claim untouched
  - Estimate: 🟢 Small
  - Result: the Development Workflow bullet now names both group targets and the four tiers and splits into a separate **"Not covered, and still manual"** bullet naming JIP/MP, UI, performance and the quarantined round-trip gate. The Critical Constraints bullet is now "Coverage is a spine", plus a new **"No `maxAttempts`"** constraint. **"No debugger" is byte-identical.** (File is gitignored — edited anyway, per instruction.)
- [x] ✅ **7.7 `docs/features/dev-ops/epic-overview.md`** — feature #3 status, task count, rollup, integration note
  - Estimate: 🟢 Small
  - Result: feature #3 → **✅ Complete, 61/62 (1 optional)**, with a note under the table explaining that 4.10 is deliberately deferred. Rollup → **3/5 features complete**. The `vanilla-persistence` bullet in Integration & Architecture now records that the gate **exists**, with its command and exit criterion, and that the coupling turned out sharper than planned (no save path in either system → red gate is the honest artefact). Purpose and the documentation-policy bullet de-staled. **No Research Basis supersession note added** — nothing in that table was falsified by #3 (its findings are additive), unlike #2's correction of #1.
- [x] ✅ **7.8 Stale-claim sweep** — `grep -ri "single smoke test\|smoke test only\|no real coverage"`; fix or flag every hit (dated findings.md records stay)
  - Estimate: 🟢 Small
  - Result: ran the specified patterns plus the variants ("one smoke test", "only a smoke test", "proves the harness runs", "asserts nothing", "no test suite", "manual testing only", "testing is manual", "coverage does not"). **6 hits fixed beyond the named tasks**: the three agent definitions (`solution-architect.md`, `component-developer.md`, `component-developer-advanced.md` — feature #2's precedent), `docs/technical-design.md` §2's "only verification that exists", and two now-false current-fact claims inside `docs/features/core/persistence/` (`requirements.md` constraint 4, `implementation.md`'s "Workbench has no automated testing") — both annotated in the same style feature #1 already used in that file. Everything else left as dated history: `docs/overview.md`'s v1.1 changelog, features #1/#2's plan docs, this feature's own plan + findings.

> **Gates re-run on the final tree:** `tools/compile-check.sh` → **0** (`OK (5984 files, Game module, 5s)`); `tools/run-tests.sh` → **0** (`OK (1 tests, 15s)`). No code, config or `tools/` script was touched by this phase — `tools/README.md` is the only file under `tools/`.

---

## Bugs & Issues

**Active Bugs:**
- (none yet — suspected pre-existing gameplay bugs go to findings.md "Bugs found (log only)", not here; fixing them is out of scope)

---

## Technical Debt

- (none yet)

---

## Progress Tracking

### Discovered New Tasks
- (none yet)

### Blocked Items
- (none)

---

*Update this file as tasks are completed. Mark tasks with ✅ immediately when done. Add new tasks as they're discovered.*
