# Test Coverage - Context & Decisions

**Last Updated:** 2026-08-02 04:45
**Current Phase:** Complete (all 7 phases + cross-phase review + fixes)
**Status:** ✅ Ready for Review
**Epic:** dev-ops (feature #3 of 5 — depends on #2 autotest-foundation ✅; runs ∥ #4 ci-pipeline; gates `core/persistence`)

---

## Quick Status

**What's Done:**
- ✅ Plan created (implementation.md — 7 phases / 62 tasks; architect read the gameplay + persistence source and #2's findings first)
- ✅ Dev docs scaffolded
- ✅ User directive folded in (2026-08-02): `.scripts/reset_save.sh` / `backup_save.sh` / `activate_save.sh` must be documented and used for fresh-campaign/known-state preconditions (Phase 2 + Decisions 7-8)
- ✅ Phase 1 (12/12): spike complete, findings.md (631 lines), 11 launches, all probe code deleted, compile-check 0. Headlines: Tier A world-free (8 s, harness runs ONCE); rung L3 (`SaveGame()` silently inert — warning itself null-guarded away); save path `<My Games>/OverthrowCI/profile/.db/Overthrow` (extra `profile/` level); campaign start 71 ms same-frame but **preset must be selected by name** ('Test World' is index 4, index 0 is 'Easy'); stale singletons DON'T manifest (engine nulls weak statics); `[BaseContainerProps()]` MANDATORY for group membership, group order alphabetical; timeoutS 30/45/60; 62 deterministic VM exceptions per campaign start (Bug #1 — console error counts unusable for CI)
- ✅ GATE applied 2026-08-02: Phases 2-7, Decisions 3/6, Risks R3/R5/R8 amended in place
- ✅ Phase 2 (7/7): the three `.scripts/` save tools are automation-callable. `backup_save.sh [<name>]`, `activate_save.sh [<name-or-file>]`, `--profile <name>` on all three (→ `<My Games>/<name>/profile/.db/Overthrow`), and the `reset_save.sh` destructive-path guard. 30-scenario matrix run against throwaway dirs and recorded in findings.md; interactive paths diffed against the pre-change scripts and are unchanged bar one added line

- ✅ Phase 3 (10/10): `OVT_TEST_SuiteBase` extended (`RequiresStartedCampaign()` opt-in + guarded campaign-start Setup step + `ResolveManager()` defensive helper) and **Overthrow's first four real assertions ship green** as `OVT_TEST_InitSuite` (Tier B). Smoke 0 / Meta 1 regression pair still holds; Init exits 0 three times identically (14 s, 4 cases); every case has a recorded can-fail proof; the fresh-campaign reset wiring is proven. The campaign-start branch was exercised for real (temporary override, reverted) — Phases 4-5 inherit proven machinery.

- ✅ Phase 4 (9/10; 4.10 optional, not started): **persistence coverage ships and the migration gate exists.** `OVT_TEST_PersistenceSuite` (Tier D) is 8 green same-session round-trips through the public manager API; `OVT_TEST_PersistenceRoundTripSuite` (Tier D') is 9 cases, quarantined, red by design, whose exit 1 → 0 is `core/persistence`'s acceptance criterion. Every green case has a can-fail proof; the DoD grep over the test tree returns **zero** persistence-type hits; `tools/run-tests.sh` is byte-identical.

- ✅ Phase 5 (8/8): **all four tier suites now exist and are green.** `OVT_TEST_LogicSuite` (Tier A) is 14 world-free cases in 6-9 s — town maths, modifier recalculation, pure job conditions, skill effects, player levelling — and `OVT_TEST_CampaignSuite` (Tier C) is 4 cases on a started campaign (start flags + difficulty preset, town activation, shop stocking, tax/donation income). 19 can-fail perturbations, 19 exit-1 runs. **The plan's integer-division defects do not exist** — EnforceScript chooses an expression's arithmetic mode from the type it converts TO — so R7 is closed and one genuinely new bug was pinned instead.

- ✅ Phase 6 (7/7): **the fast/slow contract ships.** Two hand-authored `SCR_AutotestGroup` configs in `Configs/Tests/` — **Fast `{6A6E29FF47ECB840}`** (Logic + Init, 18 cases, 13-16 s) and **All `{6A6E2A002F53A581}`** (+ Campaign + Persistence, 30 cases, 16-19 s). Both exit 0 three consecutive times with identical case counts. Meta, RoundTrip and Smoke are enumerated as `: 0` in the harness listing and contribute zero cases. Both GUIDs documented in `tools/README.md`; `tools/run-tests.sh` byte-identical.

- ✅ Phase 7 (8/8): **the docs now tell the truth about the new coverage position, in both directions.** Seven documents updated (technical-design §2/§7/§10/§12, mission-statement, `tools/README.md`, the `workbench-workflow` skill v1.3.0, core/persistence's context, `CLAUDE.md`, epic-overview) plus six sweep fixes. Every one of them names both what is covered *and* what is not — JIP/MP, UI, performance, AI movement, save/reload. Gates re-run clean: compile-check **0**, run-tests **0**.

**What's Next:**
- **DONE — cross-phase review passed (2026-08-02).** Fresh-eyes reviewer verified every DoD criterion independently (F/Q/I/D all green). One MAJOR found and FIXED before commit: the gate's stability case wrote `town.stability` directly, which a correctly-migrated persistence layer (recomputing from modifiers) would never restore — the gate could never have flipped green. Reworked onto the `TryAddStabilityModifier`/`RemoveStabilityModifier` seam, verified: Persistence 0/0, RoundTrip 1 with the standard diagnostic ×9, DoD greps clean. Three minors also fixed (null guards, cross-accessor comment downgraded, closure-5 ordering dependency stated honestly).
- **User:** review the working tree and commit (nothing is committed; the `core` epic move is staged separately by you). The 11 bugs found are filed as BUG-001…BUG-011 in beast-mode.
- Task **4.10 remains the single unstarted task** (61/62) — OPTIONAL, needs explicit user approval, off the critical path. Everything it would validate is already covered by the quarantined gate. Closing the feature without it is the documented, acceptable outcome.
- The honest coverage position, now written everywhere: **30 automated assertions across four tiers** (Logic 14 world-free, Init 4, Campaign 4, Persistence 8 same-session) in one command; **JIP, multiplayer, UI, performance and the save/reload round-trip remain manual or gated**. Do not let a later doc round it up.
- The two group GUIDs are the stable public contract for feature #4 — quote them, never read the `.conf`.
- `/update-master` will want `docs/overview.md`: its v1.1 changelog entry was deliberately left as dated history, so the epic row and a v1.2 entry are still owed.

**Blockers:**
- None

---

## Key Files

### Deliverables
- `Scripts/Game/Tests/TestFramework/OVT_TEST_SuiteBase.c` — EXTENDED: `RequiresStartedCampaign()` opt-in + campaign-start Setup step + manager-resolution helper
- `Scripts/Game/Tests/TestSuites/{Logic,Init,Campaign,Persistence}/` — the four tier suites + quarantined `OVT_TEST_PersistenceRoundTripSuite`
- `Configs/Tests/OVT_TestGroup_Fast.conf` / `OVT_TestGroup_All.conf` (+`.meta`) — the fast/slow targets for CI
- `.scripts/reset_save.sh` / `backup_save.sh` / `activate_save.sh` — argument forms + destructive-path guard (extended, never rewritten)
- `docs/features/dev-ops/test-coverage/findings.md` — Phase 1 empirical record + can-fail table + bug log

### Related
- `docs/features/dev-ops/test-coverage/implementation.md` — the plan (ground-truth table + Decisions 1-10; read before touching anything)
- `docs/features/dev-ops/autotest-foundation/findings.md` — #2's empirical ground truth (supersedes assumptions)
- `tools/run-tests.sh` — consumed, NEVER modified (byte-identical is DoD F8)
- `tools/lib/common.sh` — `ovt_profile_dir` for the `OverthrowCI` save path

---

## Important Decisions

(10 decisions live in implementation.md — the load-bearing ones:)

### Decision 1: Suites organised by setup cost (tier), not subject
Four tiers — Logic (pure), Init (world, no campaign), Campaign (started), Persistence (started + save seam). World transition + campaign start are per-suite costs; coverage grows by adding case FILES to a tier.

### Decision 4: Persistence assertions touch ONLY Overthrow's public manager API
The single permitted persistence reference in the whole test tree is the annotated `GetPersistence().SaveGame()` trigger. Grep-enforced (DoD Q4).

### Decision 5: Round-trip suite ships quarantined and RED — it IS the core/persistence acceptance gate
No working save path exists on this branch (SaveGame() stubbed; EPF disconnected by re-parenting). Green same-session suite ships in CI; `OVT_TEST_PersistenceRoundTripSuite` exits 1 today, and its flip to 0 is the migration's definition of done.

### Decision 6: Managers resolved from the live game mode, not `OVT_Global` statics
`s_Instance` is never nulled and the harness loads the world 3× per launch — stale statics are the #1 anticipated flake source.

### Decision 8: `reset_save.sh` gets a destructive-path guard
Its default target is the user's REAL Workbench campaign save; automation must not be able to delete it via an unset `OVERTHROW_SAVE_DIR`.

### Decision 10: Bugs found are pinned and logged, never fixed and never hidden
Pin current behaviour with honestly-named cases, log in findings.md. **Phase 5 update:** the integer-division defects the plan expected are NOT real (see the Phase 5 gotchas), so R7 is closed; the mechanism still earned its keep — it caught a real dealer-condition bug and pinned three deliberate-looking behaviours.

---

## Gotchas & Learnings

### From planning (read the ground-truth table in implementation.md for sources)
- **Campaign start is trivial but sharp-edged:** `DoStartNewGame()` + `DoStartGame()` are public and player-independent, but non-idempotent (guard with `!HasGameStarted()`) and default to Normal difficulty unless `m_Difficulty = m_aDifficultyPresets[0]` is set first (TestWorld preset: cash 100000, resources 200).
- **No save path on this branch, in either system** — `OVT_PersistenceManagerComponent` methods are TODO stubs AND the class re-parenting means EPF never reaches SETUP. "Passes against current EPF" is unsatisfiable here; hence the ladder (L1-L4) and Decision 5.
- **Save scripts default to the WORKBENCH profile** (`ArmaReforgerWorkbench/profile/.db/Overthrow`) but tests run under `OverthrowCI` — every automated call must set `OVERTHROW_SAVE_DIR` (exact path determined empirically in task 1.8).
- **Test world has exactly 1 town + 1 base controller** — assert `>= 1`, never magic counts.
- **Post-start deferred work:** occupying-faction resources at 5 s, deployments at 10 s — `timeoutS` budgets come from task 1.4.
- **Pre-existing noise:** `Failed to get SCR_PersistenceSystem instance!` once per world load; navmesh fails to load (no AI-movement tests).

### From Phase 2 (save scripts)
- **Call the save scripts by their repo-relative path, or with `--profile`.** `activate_save.sh` used to word-split its `.saves/` listing on the space in `/mnt/n/Projects/Arma 4/…` whenever it was invoked by absolute path — the menu filled with garbage and `du` walked the whole project tree (a 2-minute hang). Fixed in Phase 2, but it is a reminder that this repo's own path contains a space and every new script must quote accordingly.
- **`reset_save.sh` now refuses anything that is not an ABSOLUTE path ending in `.db/Overthrow`**, and prints `Resolved save directory: <path>` on every run. A relative path is refused too, so a wrong `cd` cannot make the target resolve somewhere unexpected.
- **A set-but-empty `OVERTHROW_SAVE_DIR` is now an error, not a fallback.** Previously `OVERTHROW_SAVE_DIR= .scripts/reset_save.sh` silently deleted the user's real Workbench save (the `${VAR:-default}` form treats empty as unset). This is exactly the CI failure mode Decision 8 was written for, and it was live until Phase 2.
- **`--profile <name>` resolves `<My Games>/<name>/profile/.db/Overthrow`** — note the `profile/` level (finding 1.8); `ovt_profile_dir` returns the profile ROOT, not `$profile:`. `OVERTHROW_SAVE_DIR` still wins and says so when both are given.
- **Exit codes:** 0 success (a missing save dir is "nothing to delete", still 0), 1 usage/refusal/missing-save-dir/unmatched-name/failed-op, 2 `--profile` unresolvable. `OVERTHROW_MYGAMES_DIR` (from `common.sh`) points the resolution at a fake `My Games` — that is how `--profile` was tested without touching a real profile.
- **`.saves/` is the user's fixture library** (six `testworld_*` archives). Anything a test run creates there must be deleted afterwards; verify with an `ls -1` diff.

### From Phase 3 (suite base + Tier B)
- **`PrintFormat` on a suite or case takes at most THREE string params** — the 4th positional is `LogLevel`, so a 4-param call is a compile error (`Cannot convert 'string' to 'int' for argument '4'`). Phase 1 hit this too; split the message.
- **Diagnostic `Print`/`PrintFormat` output goes to `console.log`, NOT `autotest.log`.** `autotest.log` only carries the suite prelude/epilogue and per-case verdicts. To prove an assertion was actually reached, grep `console.log`.
- **Case execution order inside a suite is alphabetical by case class name**, exactly like suite order inside a group (finding 1.10). No case may depend on another having run, and no case may leave state a later case needs.
- **Case count is free; suite count is not.** Four Tier-B cases cost the same 14 s as feature #2's single smoke case — the launch and the world transition are the whole cost. Grow coverage by adding cases (and case files) to an existing tier, never by adding suites.
- **Town and base controllers are registered during manager `Init` (world load), not at campaign start** — `FilterTownControllerEntities` and `InitializeBases`. That is what makes controller assertions legitimately Tier B rather than Tier C.
- **Economy price/demand maps take arbitrary int keys** (real IDs are indices into `m_aResources`), so a synthetic ID exercises the seams without touching any real item's price. Documented unknown-key fallbacks: price **500**, demand **5**.
- **Assert against values read from config, not hardcoded numbers** — `GetBuyPrice` is pinned as `Math.Round(price + price * m_fShopProfitMargin)` with the margin read live (0.25 today → 1250 on a base of 1000), plus the independent claim that a buy price is always above the base price.
- **Perturb the covered thing, not the comparison, where you can.** The getter-sweep can-fail proof added a genuinely-null getter (`GetController()`, one of the four excluded player-dependent ones) rather than inverting an `if` — it drives the real detection path and produces the real failure text.

### From Phase 4 (persistence tiers)
- **`Rpc()` self-delivery, settled empirically:** in the autotest client `RplSession.Mode()` is 0 (`RplMode.None`) and `Replication.IsServer()` is true. **`Rpc(RpcAsk_X, …)` (`RplRcver.Server`) executes locally and synchronously**; **`Rpc(RpcDo_X, …)` (`RplRcver.Broadcast`) does not execute locally at all** — which is exactly why gameplay code calls broadcast handlers directly *and* Rpc's them. So an RPC-mediated public mutator like `TryAddStabilityModifier()` is observable in the same frame and needs no polling.
- **The assertion rule's own tokens must not appear anywhere in the test tree, including comments.** Quoting Decision 4 verbatim in the suite headers tripped the DoD grep it describes. Headers now paraphrase the three type names and point at implementation.md — and say that this is deliberate.
- **The local player is set up ~18 ms BEFORE the campaign-start Setup step** (`Setting up player: <uid> with playerId: 1`), so player-scoped cases resolve the persistent ID directly with no polling, and fail with a named diagnostic if it is ever absent.
- **Campaign-tier cases must finish within ~10 s of the campaign start** — `MODIFIER_FREQUENCY` is 10 000 ms and that tick recalculates support, stability and population growth. Everything Phase 4 uses is synchronous, so the suite finishes ~1.5 s in; a future case that polled for seconds would race the tick and look flaky.
- **Two identifier spaces, one record:** money and ownership are written with a *runtime* player ID and read with a *persistent* one. Crossing them deliberately is the point — it is the seam most likely to break in a migration that re-keys player records.
- **`AddRecruit()` needs an `IEntity`, not a character** (it only uses `GetOrigin`/`GetID`/`FindComponent`), so the registered town controller entity is the subject: a test must not spawn AI (no navmesh) and must not hijack the player's character.
- **Pick subjects that are provably unowned.** The campaign start gives the local player a *randomly chosen* starting home, so "the nearest building to the town centre" would collide with it on some runs and not others. `GetRandomUnownedHouse()` is the deterministic-verdict choice.
- **Two more pre-existing bugs logged, not fixed** (findings.md items 9-10): `RemoveRecruit()` leaves the entity→recruit mappings behind; `DoSetOwnerPersistentId()` does not clear the previous owner, so after a transfer `IsOwner(oldOwner, …)` stays true while `GetOwnerID()` reports the new one.

### From Phase 5 (Tier A pure + Tier C campaign)
- **ENFORCESCRIPT DOES NOT DO C-STYLE INTEGER DIVISION.** An arithmetic expression's evaluation mode is chosen from the type it is being converted TO, not from its operands: `(a / b) * 100` with `a = 25, b = 50` is **50** when consumed as a float and **0** when consumed as an int. Truncation happens at the conversion, not at the division. So `Math.Round(...)`, a float return type, or a float local all make `int / int` a real division. Three "obvious" integer-division bugs in the plan (`SupportPercentage`, `GetLevelProgress`, `GetTaxIncome`) are therefore **not bugs**, and the opposite mistake — `int x = a / b;` — is the one to look for.
- **Settle this class of question by MEASURING, not by reading.** `GetTaxIncome()` at 100 stability behaves identically whether its stability factor truncates or not; the campaign case only distinguishes them because it lowers stability to 90 through the real modifier seam and observes 1125 instead of 1250.
- **`new` does NOT apply `[Attribute()]` defvalues.** A hand-built config object starts with every field zeroed. This is Tier A's biggest trap and it is worst where the declared default is not zero: an "unset" sentinel of -1 becomes 0 (a real constraint), and a multiplier defaulting to 1 becomes 0 (which silently zeroes the whole expression).
- **The Tier A purity grep (DoD Q5) covers comments.** No file under `TestSuites/Logic/` may contain the static manager accessor's name or the engine's game-mode getter, in code or in prose — the same trap Phase 4 hit with the persistence rule. Write around it and say that you did.
- **Gameplay code that prints `SCRIPT (E)` is not a test failure.** A green Tier A run contains 3 error lines, one of them from the `max == 0` guard a case deliberately drives. Verdicts come from `junit.xml` only.
- **Tier A is genuinely free per case:** 14 cases in 6-9 s, zero scenario changes, one harness start. Tier C's 4 cases cost 13-15 s — i.e. the campaign start and world transition are the whole bill.
- **Restore campaign state on the FAILURE path too.** The income case runs its checks in a helper that returns a problem string so the caller can put faction, support and stability back before reporting; cases run alphabetically and that one sorts first, so a red run would otherwise cascade.
- **`OVT_TEST_Campaign_Economy_IncomeMatchesTownState` does not actually need a started campaign** (proven: it stayed green when the campaign start was switched off). It sits in Tier C because the plan places it there and it costs nothing extra, but it is a Tier B case by dependency.
- **A bounded poll backstop earns its keep even for same-frame observables.** Both polling Tier C cases report what they last saw rather than dying on the harness timeout; the same design also made two of the can-fail perturbations produce a readable diagnostic instead of a timeout.

---

## Testing Approach

The tests ARE the deliverable — validation is V1-V5 (per case: can-fail, ran, assertion reached, deterministic ×3, tier-correct), S1-S8 (suites/groups), W1-W6 (save scripts, throwaway dir only). No flakes; `maxAttempts` needs recorded proof of engine non-determinism.

---

## Session Notes

### 2026-08-02 00:20 — Feature started
- Invoked via `/autorun-feature dev-ops/test-coverage` (autonomous, from Discord)
- Solution-architect wrote the plan after reading gameplay/persistence source + #2's findings; two reshaping discoveries: (1) no working save path on this branch → quarantined-gate architecture; (2) programmatic campaign start is a five-line Setup step
- Mid-plan user directive: use/document the `.scripts/` save tools — integrated as Phase 2 + Decisions 7-8
- Next: Phase 1 spike (ADVANCED agent with Bash — the project's `*-advanced` EnforceScript agents have no Bash, so use a full-toolset agent like #2's Phase 2 did)

### 2026-08-02 01:30 — Phase 2 complete (save-state control)
- All three `.scripts/` save tools extended in place — same location, same output tone, same archive format and `.saves/` naming. `tools/` untouched; `git status` shows only the three scripts plus this feature's docs.
- Argument forms: `backup_save.sh [--profile <name>] [<name>]`, `activate_save.sh [--profile <name>] [<name-or-file>]`, `reset_save.sh [--profile <name>]`, `-h` on all three.
- The guard is the phase's real deliverable: `reset_save.sh` refuses empty / `/` / relative / non-`.db/Overthrow` paths with exit 1 and deletes nothing, and always prints what it resolved. DoD Q10 and item 14 both verified by execution.
- **Two real defects were found by running the scripts rather than reading them**: (1) `activate_save.sh`'s array word-split on the repo path's space — broken for every absolute invocation, i.e. exactly how CI would call it; (2) a set-but-empty `OVERTHROW_SAVE_DIR` fell through to the user's real Workbench save. Both fixed; both recorded in findings.md.
- Verification: 30 scenarios, throwaway dirs only. Interactive parity proven by diffing old vs new with identical stdin — the only delta in any interactive path is reset's new `Resolved save directory:` line, and both `read -p` prompts are character-identical.
- Nothing was run against `ArmaReforgerWorkbench/profile` or the real `OverthrowCI` profile; the real path was only ever *printed*, via the read-only `backup_save.sh --profile OverthrowCI`.
- Phase 2 is independent of Phase 3 and could have run in parallel with it; Phase 3 (suite base + Init suite) is unblocked either way.

### 2026-08-02 01:55 — Phase 3 complete (suite base + Tier B Init suite)
- **Overthrow now has real automated assertions**, not just a harness: `OVT_TEST_InitSuite` (Tier B) ships four green cases — `OVT_Global` getter sweep (18 getters), towns populated, town+base controllers registered, economy price/demand seams and shop margin.
- `OVT_TEST_SuiteBase` gained the shared campaign-start machinery Phases 4-5 depend on. It is inert for every suite that does not opt in, and the opt-in branch was **exercised end to end** during the phase (temporary override on the Init suite, reverted): `difficulty preset selected by name = 'Test World'`, `initialized after 0 poll(s)` — reproducing finding 1.3 exactly. Phase 4 does not inherit an untested code path.
- Two deviations from the letter of the plan, both additive and recorded in findings.md: (1) `CloseLayout()` is called on **every** poll of the start step, not once, because the start menu opens ~4 ms after Setup and would otherwise race a multi-frame step — it is a no-op when the layout is not shown; (2) the start step has a 600-poll diagnostic backstop so a future async change fails with a message naming what it observed instead of hanging to the harness timeout. Neither is a retry (`maxAttempts` is still absent from the whole test tree).
- Can-fail proofs: 4 perturbations, 4 exit-1 runs, each `autotest_failed.log` naming exactly the perturbed case; method and verbatim failure text are in findings.md → "Can-fail proofs" (Phases 4-5 append to that table).
- Regression pair intact: `run-tests.sh` → 0 and `OVT_TEST_MetaSuite` → 1, verified both immediately after the base-class change and again on the final tree. Smoke and Meta were not modified — including no `[BaseContainerProps()]`, which Phase 6 will add if group membership needs it.
- Determinism: three consecutive Init runs, exit 0 / 4 testcases / 0 failures each, 14 s tool time each.
- Task 3.8 was Phase 2's first real consumer: `.scripts/reset_save.sh --profile OverthrowCI` → exit 0, resolved path printed, nothing to delete (the CI profile has never produced a save), verdict unchanged with and without it. The user's Workbench save was never a target.
- Scope: `git diff --stat -- Scripts/ ':!Scripts/Game/Tests/'` is empty; `tools/`, `Configs/` and `.scripts/` untouched by this phase.

---


### 2026-08-02 02:30 — Phase 4 complete (persistence: Tier D green + Tier D' gate)
- **Overthrow's persistence contract is now executable.** Eight green same-session round-trips (money, skills+XP, real estate ownership, recruits, town control/support/population/stability) and a nine-case quarantined round-trip suite whose exit code is the `core/persistence` acceptance criterion.
- Determinism: three consecutive `OVT_TEST_PersistenceSuite` runs on the final tree, exit 0 / 8 cases / 0 failures each. The quarantined suite: exit 1, 9 of 9, with one identical diagnostic sentence — quoted verbatim in findings.md.
- **The anti-vacuous-pass design is the real deliverable of the red suite**, and it is two-layered: the capability case asserts the *transition* (no save before → a save after, which is what makes the `reset_save.sh` precondition load-bearing), and every state-kind case *dirties* the value between saving and reloading, so a reload that restores nothing cannot pass — a closure that does not use `HasSaveGame()` at all and therefore survives a lying save layer.
- One throwaway probe was run and deleted (Phase 1 precedent): it proved `Rpc(RpcAsk_…)` self-delivers synchronously on the server, which let the stability case be **upgraded** from a record-field write to the real modifier seam (`TryAddStabilityModifier` → `RemoveStabilityModifier`, expected value derived from the modifier system's own recalculation). Observed 100 → 90 → 100.
- Deviations from the letter of the plan, both recorded: (1) the assertion rule is quoted in the headers with its three type-name tokens paraphrased, because quoting it verbatim tripped the grep that enforces it — the DoD grep now returns zero lines; (2) the round-trip suite's reload is implemented against rung L1 rather than stubbed, so the gate can flip to green without editing test code, and is documented as unreachable-and-untested today.
- Task **4.10 (L4 `main`-worktree validation) was not started** — optional and requires explicit user approval.
- Scope: `tools/run-tests.sh` md5 unchanged; `git diff --stat -- tools/` is `tools/README.md` only; nothing under `Scripts/Game/` outside `Tests/`, nothing in `Configs/`, `.scripts/`, or the Smoke/Meta/Init suites.

### 2026-08-02 03:15 — Phase 5 complete (Tier A pure logic + Tier C started campaign)
- **All four tier suites now exist and are green.** Tier A `OVT_TEST_LogicSuite`: 14 world-free cases (6-9 s, zero scenario changes) over town record maths, town modifier recalculation, the three pure job conditions, the four active skill effects and the player level curve. Tier C `OVT_TEST_CampaignSuite`: 4 cases (13-15 s) over the start flags and difficulty preset, town activation, shop stocking and the tax/donation income calculators.
- **The phase's headline is a correction to the plan, not a deliverable.** The two cases the plan told this phase to ship as integer-division bug pins FAILED on their first run, because the bugs are not real: EnforceScript chooses an expression's arithmetic mode from the type it converts TO. Both cases were rewritten as plain correctness assertions, `GetTaxIncome()` was re-measured below full stability to settle it by observation rather than inference, and risk R7 is closed. The probe that established the rule was thrown away after one run, per Phase 1 precedent.
- **One genuinely new bug found and pinned** (`OVT_TownHasDealerJobCondition` inspects only the X axis, so a dealer at X = 0 reads as absent — findings item 11), plus three behaviours pinned as "current" without calling them bugs: the hardcoded 500 m `IsWithinTownBounds` radius, `CopyFrom()` not copying location/size and sharing modifier arrays, and the absence of an upper clamp on area heat.
- Can-fail: **19 perturbations, 19 exit-1 runs**, every single-case row naming exactly the perturbed case. All of them change a real input rather than inverting a comparison; the strongest is the Tier C row that switches `RequiresStartedCampaign()` off and reds three of the four cases with their own honest diagnostics — which also proved that the income case does not depend on the campaign at all.
- Deviations from the letter of the plan, all recorded in findings.md: (1) no integer-division pins, for the reason above; (2) the random-condition "always" edge asserts a chance beyond the roll's range as well as the plan's literal 100, because `RandFloatXY`'s upper-bound inclusivity is undocumented and this feature does not ship a case that could theoretically flake; (3) the income case gained a fourth state (lowered stability) so the tax formula is settled by measurement; (4) `timeoutS` is 30 everywhere — no case needed the 45 s resource-distribution budget.
- Scope: only `Scripts/Game/Tests/TestSuites/{Logic,Campaign}/` was created. `tools/`, `Configs/`, `.scripts/`, `OVT_TEST_SuiteBase.c` and the Smoke/Meta/Init/Persistence suites are byte-identical; the final tree re-verified Smoke 0 / Meta 1 / Init 0 / Persistence 0.

### 2026-08-02 03:40 — Phase 6 complete (group configs: the fast/slow contract)
- **Overthrow's whole test suite is now one command.** `tools/run-tests.sh "{6A6E2A002F53A581}"` → exit 0, **30 cases**, ~18 s. The push-gate subset is `"{6A6E29FF47ECB840}"` → exit 0, **18 cases**, ~16 s. Both GUIDs are the stable contract feature #4 hard-codes; they are documented in a new `### Group targets (the fast/slow contract)` subsection of `tools/README.md`, alongside the CI usage (reset via `.scripts/reset_save.sh --profile OverthrowCI`, Fast every push, All nightly/pre-merge).
- **Both configs registered on the first launch, no Workbench round-trip** — `CLI autotest config: SCR_AutotestGroup<0x…>`, exactly as finding 1.10 predicted. The plan's entire `Unknown class` contingency path went unused because Phases 3-5 had already put `[BaseContainerProps()]` on all four tier suites (verified before the first run; **no suite class was edited by this phase**).
- GUIDs: 8 generated (2 config + 6 per-entry instance), all fresh, none reusing the probe's. Collision-checked by content (`rg --binary`) **and** filename across six roots including the 56 GB game install — zero hits, with a positive control proving the game-install scan was not an empty walk.
- Leak check is the phase's quiet win: the quarantined and meta suites are not merely absent from the group, they are **enumerated and disabled** (`OVT_TEST_MetaSuite: 0`, `OVT_TEST_PersistenceRoundTripSuite: 0`, `OVT_TEST_SmokeSuite: 0`) in every All run, and `4+4+14+8 = 30` leaves no room for a leaked case.
- Determinism: Fast 0/0/0 and All 0/0/0, identical case counts and summaries; only the duration digit moves (±3 s of client boot).
- **Numbers worth reusing:** grouping saves ~30-33 s on a full run (4 launches ~49 s → 1 launch 16-19 s) because ~13 s of client boot is paid per *launch*, not per suite. Marginal cost of one more suite in a group is ~+1.5 s, and a world-free suite (Logic) costs no world transition at all — which is why both groups beat Phase 1's estimate.
- **One deliberate omission:** the `workbench-workflow` skill was not touched. It has no targets table, and four of its statements are stale in ways Phase 7 task 7.4 rewrites wholesale; editing one line inside that section would have created a merge hazard for no benefit. 7.4 now owns two extra corrections (see What's Next).
- Scope: only `Configs/Tests/` (4 new files) and `tools/README.md` (rows + one subsection, plus one stale sentence corrected in place). `tools/run-tests.sh` md5 `c1b0cbb55fecf93c70b0fda77fe3c7e4` — unchanged since Phase 4. No script, suite or gameplay file touched. `compile-check` 0.

### 2026-08-02 04:20 — Phase 7 complete (documentation / Definition of Done)

- **The docs no longer describe a coverage position this feature changed — and none of them overstate the new one.** Seven named documents plus six sweep fixes; every updated file names what is covered *and* what is not (JIP/multiplayer, UI, performance, AI movement, the save/reload round-trip). D8 was treated as the binding constraint, not a nicety.
- **`docs/technical-design.md`** took the largest change: §10's "real but tiny" is now a six-suite / 32-case tier table with the can-fail and no-`maxAttempts` policies; **"The three dimensions that break" is rewritten** to say plainly that only persistence is half-automated and JIP and `modded class` are entirely manual; §7 carries the acceptance gate. Two claims there contradicted the facts and were corrected as well: §2's "that's the only verification that exists", and **§12's "persistence tests that pass against EPF today"** — which was never satisfiable on this branch and is now stated as the red-gate outcome it actually produced.
- **`tools/README.md`** needed only the missing piece: the group-targets (Phase 6) and acceptance-gate (Phase 4) subsections were verified correct and left alone. The new **Save-state control** section documents all three `.scripts/` tools canonically — precedence, the Workbench-vs-`OverthrowCI` trap as a warning, the path guard, exit codes, `.saves/` naming, and the EPF-layout migration note. DoD I5 is satisfied in one place.
- **The skill (v1.3.0)** is the artefact a future agent actually reads, so it got the tier table with a "put a case here when it…" column, the `RequiresStartedCampaign()` opt-in with the select-by-name trap, the manager-resolution rule, the test-world scale limits, the `new`-applies-no-defvalues trap, and **`maxAttempts` reclassified from "prefer not to" to banned**. Phase 6's two deferred corrections are folded in.
- **`docs/features/core/persistence/`** now carries the gate as an executable recipe in Quick Status, plus a dated session note recording the two findings this feature owed it: no working save path in *either* system (and the UI that reports success anyway), and the `.scripts/` tools' EPF `.db/Overthrow` assumption with the reason the replacement location can only be found empirically.
- **Sweep dispositions:** 6 fixed beyond the named tasks — three agent definitions (feature #2's precedent), technical-design §2, and two now-false current-fact claims inside `core/persistence`'s `requirements.md` / `implementation.md`, both annotated in the style feature #1 already used in that folder. Left as dated history: `docs/overview.md`'s v1.1 changelog, features #1/#2's plan docs, and this feature's own plan + findings.
- **One judgement call to flag:** no "Superseded by feature #3" note was added to the epic's Research Basis — every entry there is still accurate, and #3's findings (`[BaseContainerProps()]`, alphabetical order, world-free suites) are additive rather than corrective, unlike #2's supersession of #1.
- Scope: docs only. No code, config, `.scripts/` or `tools/` script touched — `tools/README.md` is the sole file under `tools/`. Gates re-run on the final tree: `compile-check` **0** (`OK (5984 files, Game module, 5s)`), `run-tests.sh` **0** (`OK (1 tests, 15s)`).

### 2026-08-02 — Cross-phase review fixes (persistence suites only)
- **One major finding, and it was a gate that could not open.** The quarantined suite's stability round-trip wrote `town.stability` directly and claimed no synchronous public mutator existed — false, and load-bearing: Overthrow derives stability from the modifier list, so a correctly migrated persistence layer would restore the *recomputed* value and that case would have failed forever, permanently pinning the migration's acceptance criterion at exit 1. It now drives the same seam the green suite does (`TryAddStabilityModifier` → save → `RemoveStabilityModifier` as the dirty step), with the expected value derived from the modifier system's own recalculation. Proven can-fail by a throwaway gate-bypass experiment (stability case green at saved 90 / dirty 100; perturbing the dirty step reds it with its own new diagnostic), file restored byte-for-byte afterwards.
- Three minor fixes: null guards with named diagnostics at five unchecked manager-accessor dereferences; the "cross-accessor" stability comment downgraded to what it actually proves (both accessors resolve the same live record — the assertion itself is unchanged, and no independent accessor exists to strengthen it with); anti-vacuous closure 5 corrected to name the one real order dependency (the capability case's fresh-session check) and the condition that would break it.
- Verification on the fixed tree: `compile-check` **0**; `OVT_TEST_PersistenceSuite` **0, 0** (two consecutive, 8 cases); `OVT_TEST_PersistenceRoundTripSuite` **1** with all nine cases still carrying the identical "Persistence capability absent" sentence — the red is unchanged in shape, which was the point. DoD greps still clean (persistence tokens 0, `maxAttempts` 0, no ternaries).
- Scope: the two persistence suite files plus these docs. No gameplay code, no `tools/`, no `Configs/`, no other suite. Full record in findings.md → "Cross-phase review fixes — persistence suites".

---

*Update this file at the end of each work session. Run `/dev-docs-update` before compacting conversations.*
