# First Spawn - Context & Decisions

**Last Updated:** 2026-08-09
**Current Phase:** Phase 6 complete - all 49 tasks done, feature closed
**Status:** 🟢 **COMPLETE** — 49/49, **play-test passed and signed off 2026-08-09**, string export done (one small re-export owed for `OVT-FieldManual_Welcome_Text2`)

**Epic:** `new-player-experience` (feature #4 of 5 — depends on #1 `tutorial-system`, shipped; coordinates with #2 `field-manual` and #3 `tutorial-content`, both complete; #5 `starter-jobs-retirement` follows)

---

## Quick Status

**What's Done:**
- ✅ Plan written (`implementation.md`, phases 0-6) with the two-entry design, the server→client spawn-context transport, the start-menu wiring, and §3.7's seventeen-row evidence pack
- ✅ Docs scaffolded (`tasks.md`, this file); `implementation.md` status flipped to In Progress
- ✅ **Phase 0 complete (6/6).** Baselines measured (Fast **50**, All **86**, compile-check 0); §3.7 re-verified row by row with **all 17 rows standing**; `PLAYER_SPAWNED` confirmed unique to `proofWelcome.conf`; the Field Manual Welcome inheritance mapped; the numbers line settled at **two** numbers with its `%1/%2` mapping and all five presets' resolved values; the sentence→evidence table written, **9** setup descriptions derived (not 8), **9** sentences cut for want of evidence
- ✅ **Phase 1 complete (9/9).** The server→client spawn-context transport, in two `.c` files. Owner RPC + `"house"` default + the changed `NotifyPlayerSpawnedLocal` contract on `OVT_TutorialComponent`; session cache, setter/getter and three call sites on `OVT_OverthrowGameMode`. compile-check 0; All **86 exit 0** at baseline; the Campaign spawn-trigger case still green and delivering **on attempt 1**, which is live proof the SP direct-call branch works. `Rpc()` arity proven by inspection at **2 arguments** and recorded in the code
- ✅ **Phase 2 complete (9/9).** The two welcome entries, four pages each. `proofWelcome.conf` expanded and filtered `house`; `welcomeNohome.conf` + `.meta` created, filtered `nohouse`, appended to the game-mode prefab; **3 new + 2 rewritten** `.st` items with per-sentence Rule 0 evidence in every `Comment`. compile-check 0; Fast **50 exit 0** at baseline; the Init entry guard reports **12** structurally valid entries and the field-manual link guard is green. ⚠️ **Blocked on the user's Workbench string export** before any play-test

- ✅ **Phase 3 complete (6/6).** The legacy `#OVT-IntroHint` retired: the hint block, the `m_aHintedPlayers` field and its allocation deleted from `OVT_OverthrowGameMode.c`, the `PLAYER_SPAWNED` push left byte-identical, the stale `m_bCampaignRunningRpl` rationale corrected, and both string items **kept** with the retirement record on `OVT-IntroHint`'s `Comment` (D11). §3.5's table was re-verified by reading and **confirmed**. compile-check 0; All **86 exit 0** at baseline

- ✅ **Phase 4 complete (9/9).** The start-menu descriptions. Three new `TextWidget`s in `StartGameMenu.layout` (plus `Wrap 1` and a `#OVT-` key on the shipped `DifficultyDescription`), one `RefreshDescriptions()` called from four hooks in `OVT_StartGameContext`, the five shipped difficulty configs converted to `#OVT-` keys, and **10** new `.st` items with per-claim Rule 0 evidence. compile-check 0; All **86 exit 0** at baseline; conflict checker 0. No new focusable widget (proved from the file: every `components {}` block belongs to a `ButtonWidgetClass`). Text fit **computed, not rendered** — 6 lines worst case, 164 of ~179 available units

- ✅ **Phase 5 complete (6/6).** Three assertion sites across three tiers: `OVT_TEST_Logic_Tutorial_SpawnContextSelectsOneWelcome` (new case), two new branches on the existing Init entry guard (`CheckSpawnFilters` + `CheckWelcomeCoverage`), and `OVT_TEST_Campaign_Tutorial_SpawnContextIsAuthored` (new case). **Four mutations, four recorded failure texts**, the Init guard's two branches proven separately. compile-check 0; **Fast 51 exit 0, All 88 exit 0** — the `+3` target was arithmetically wrong, T2 is a branch and adds no case (see the session note). Play-test checklist written into `tasks.md`; F1-F3 plus two more findings recorded above

**What's Next:**
- 🔄 **Phase 6** (`help-docs-sync`) — reconcile the Field Manual's Welcome page with the shipped welcomes, and the wiki scalpel pass (at most 2 pages, 0 created)

**Blockers:**
- ⚠️ **The Workbench string-table export is now owed** (user-owned). **15 ids in total: five from Phase 2 and ten from Phase 4**, all listed in their session notes. Until the export runs both welcomes AND all four campaign-setup description widgets draw raw `#OVT-` keys. It blocks play-testing, not the build.

---

## Key Files

- `Configs/Tutorials/proofWelcome.conf` — **this feature owns it.** `welcome-intro` was reserved here by name; expanded 2 pages → 4. Id, filename and resource GUID are immutable (D2)
- `Configs/Tutorials/welcomeNohome.conf` — **new.** The houseless variant, differing from `welcome-intro` on exactly one page
- `Scripts/Game/Components/Controller/OVT_TutorialComponent.c` — one field, one owner RPC, one changed signature. The **only** `tutorial-system` file this feature modifies besides the game mode's push site (I2)
- `Scripts/Game/GameMode/OVT_OverthrowGameMode.c` — the busiest file in the repo. Spawn-context map + setter/getter, both `FinalizePlayerPreparation` branches, the final-attempt delivery, and the **legacy hint removal**
- `Scripts/Game/UI/Context/OVT_StartGameContext.c` — `RefreshDescriptions()` plus three wirings; the **only** file that reads `OVT_DifficultySettings.description` (verified repo-wide)
- `UI/Layouts/Menu/StartGameMenu.layout` — three new `TextWidget`s cloning `DifficultyDescription`'s slot shape
- `Language/localization_Overthrow.st` — **the only file under `Language/` this feature edits.** Never the `.<lang>.conf` runtime exports
- `Prefabs/GameMode/OVT_OverthrowGameMode.et` — one element **appended** to `m_aTutorialEntries`. Append only, never reorder (R8)
- `docs/features/new-player-experience/tutorial-system/implementation.md` §5 — the authoritative upstream contract (entry ids, string keys, trigger catalog, Rule 0)
- `docs/features/new-player-experience/field-manual/implementation.md` §3.3 — the frozen link table; row 1, `#OVT-FieldManual_Welcome_Title`, is reserved for this feature

---

## Key Decisions

*The full set with rationale is `implementation.md` §5 (D1-D14). Recorded here are the ones a future reader will trip over.*

- **D4 — the spawn context is server-authored because the client provably cannot derive it.** All three candidate client-side discriminators fail: `home != vector.Zero` is true in **both** branches (`SpawnPlayerAtFallbackPosition:824-839` calls `SetHomePos` too), `OVT_PlayerManagerComponent.RplSave/RplLoad` is a join-time snapshot sent before `FinalizePlayerPreparation` ever runs, and ownership records are server-side. `FinalizePlayerPreparation:1011-1023` is the only place in the codebase that knows.
- **D6 — the client-side default is `"house"`, and that asymmetry is load-bearing.** `OVT_TutorialTrigger.Matches:116` treats `""` on the **trigger** as "no filter", but `""` on the **context** matches no filtered trigger at all. An empty default would suppress *both* welcomes; `"house"` degrades to exactly today's behaviour. It looks like a value that wants tidying to `""` — it is documented at the field for that reason.
- **D7 — delivery waits for the context, but only within the existing retry budget.** No new timer: `NotifyPlayerSpawnedLocal` returns `false` while the context is unknown, which the existing 10 × 500 ms retry already treats as "try again". The **final** attempt passes `acceptDefaultContext = true`, so a lost context costs a possibly-wrong page 2 and never the whole welcome.
- **D3 — three of four pages share string keys between the two entries.** The key name therefore does not match the entry id for `welcome-nohome`. Deliberate: two copies of a sentence can drift and **no gate can see it**, and the houseless entry is precisely the one nobody play-tests. Paid for once in a header comment and in each shared item's `Comment` naming both consumers.
- **D9 — difficulty descriptions become `#OVT-` keys in the existing `description` field, not a new parallel field.** `TextWidget.SetText` resolves stringtable keys, and `description` is read in exactly one file, so the read site needs no change. A parallel `descriptionKey` field would double the surface and leave the old field as a trap.
- **D11 — the legacy hint's string items are retired in place, not deleted.** Six languages of translation, and `.st` deletions churn exports the user regenerates by hand. The `Comment` becomes the retirement record — including the three false claims in the hint's own text, which is the actual protection against a revival.

---

## Gotchas / Traps

- **Rule 0 binds this feature twice over** — eight pages of welcome prose *and* eight campaign-setup descriptions, and the welcome is the first text a player ever reads. **No gate can catch a well-formed lie.** The fact-check is Phase **0**, before any writing, not a review step.
- **⚠️ A trap table is not evidence.** §3.7 was assembled at planning time; `tutorial-content` shipped a **false row** in exactly such a table and caught it only by re-verifying at phase front. Every row is re-checked against source in task 0.2, and any that falls is struck **in `implementation.md` in place**, dated.
- **Three "obvious" claims are already disqualified** by the planning fact-check, and all three were on the shortlist of things to write about: `realEstateCostMultiplier` is **read by nothing** (`GetBuyPrice`/`GetRentPrice` ignore it), `respawnCost` is gated behind a hardcoded `money > 500`, and the supporting faction **sends no troops, supplies no equipment, touches no price** and is neither replicated nor persisted.
- **`Rpc()` arity is invisible to compile-check** (BUG-090): a wrong argument count compiles clean, passes every test and dies silently at the wire. `RpcDo_SetSpawnContext` takes 1 parameter → `Rpc(RpcDo_SetSpawnContext, filter)` is **2 arguments**. The count goes in a comment beside the call.
- **The ownership test must be a player-id comparison, not `IsOwnedByLocalPlayer()`.** That helper dereferences `GetLocalControlledEntity()`, and on the load-a-save path `FinalizePlayerPreparation` runs **before the character exists** (`OVT_SpawnLogic.c:150-160`).
- **The engine never loops an RPC back to the sender**, so every owner RPC needs a local-direct-call branch or single player and listen hosts silently get nothing. This is the mistake `Notify()` documents having had to solve.
- **The legacy hint's "dead since the 1.6 spawn rework" comment is two-thirds true** and is not inherited. It genuinely fires today on SP/listen-host respawn-after-death and after a Continue. Removal is still right — that is the requirements' "repeats every session" complaint — but §3.5's table is what gets recorded. **Re-verified by reading in task 3.1 and CONFIRMED (2026-08-09); the observed-change table is recorded above.**
- **Workbench can play-test stale scripts after WSL edits.** `tutorial-content`'s first play-test was a false negative for exactly this reason. Refocus/reload Workbench before believing a "the fix didn't work" report.
- **Concurrent sessions share this tree.** Re-check `git status` and the highest allocated GUID at the start of every phase rather than trusting a document snapshot (R8). *Confirmed live during Phase 1: another session's `tutorial-system` layout work appeared mid-phase.*
- **"After 0 poll(s)" in the Campaign spawn-trigger case is a regression canary, not decoration** (added Phase 1). The test world is `RplMode.None`, so no RPC can loop back; delivery on attempt 1 proves `SetSpawnContext`'s local-direct-call branch ran. If that branch is ever broken the case **still passes**, via the ~4.5 s final-attempt fallback — it just reports many polls instead of zero. Read the number, not the verdict.
- **A RETURNING player's `GetPlayerSpawnContext()` is `""`, correctly** — their finalization runs neither branch because they already have a home. Phase 5's T3 therefore depends on the test world taking the **new-player** path (it does: the log shows `Preparing NEW player`). Assert "one of the two known values", never `"house"`.

---

## Findings for the bug backlog

*Recorded by task 5.6 on 2026-08-09, **not fixed here** (R9). All three surfaced during the planning fact-check and were re-verified in task 0.2; all three are balance or save-format changes wearing bug-fix clothes, and each needs an owner outside this feature. This feature's only obligation was to **not describe them as working**, and it does not.*

### F1 — `realEstateCostMultiplier` is a dead field

Declared at `OVT_DifficultySettings.c:65` and replicated by the pair at `OVT_OverthrowConfigComponent.c:564,619`. **Nothing reads it.** `OVT_RealEstateManagerComponent.GetBuyPrice` (`:712`) and `GetRentPrice` (`:730`) ignore it entirely, so the authored 0.4 → 2.0 spread across the five presets has zero effect on any price a player pays.

**Why it is not fixed here:** wiring it in would immediately **double house prices on Insane and cut them 60 % on Easy**. That is a balance change, needs a gameplay owner, and would land silently in every running campaign. Filing note: the cheap half (deleting the field) is *also* a decision, because the config is authored data players' configs may already carry.

### F2 — `ChargeRespawn` is gated behind a hardcoded `money > 500`

`OVT_EconomyManagerComponent.c:1874` charges the respawn cost **only when the player's balance is above 500**, a hardcoded threshold unrelated to the `respawnCost` setting. The setting therefore never bites the players it was designed for — a player who is broke, which is the death-spiral case the cost exists to create.

**Why it is not fixed here:** removing the gate changes how punishing death is at low balances, on every difficulty at once. Same class as F1.

### F3 — The supporting faction is neither replicated nor persisted

`OVT_OccupyingFactionManager.RplSave/RplLoad` (`:1466-1507`) send the **occupying** faction and the **player** faction and nothing else, and there is no match for the field under `Scripts/Game/Persistence/`. So an MP client always evaluates the script default `"US"`, whatever the host chose, and the value does not survive a save either.

The visible consequence: `OVT_WantedInfo.c:204-208` compares the undercover HUD icon against the wrong key when a host picked USSR. **Related:** `IsDisguisedAsSupporting()` (`OVT_PlayerWantedComponent.c:130-140`) has **zero callers**, so the concept is half-built on both the network side and the gameplay side.

**Why it is not fixed here:** a replication *and* persistence change to a faction field, with save-format implications, on a value the campaign-setup screen now describes to players. Phase 4's supporting-faction descriptions were deliberately written to claim **only** the perception effect and the occupier coupling, and say nothing about persistence or replication, so nothing this feature shipped becomes false when F3 is fixed either way.

### D8 — no shipped entry now exercises the Learn-more-hidden branch

`proofWelcome.conf` was deliberately the *unlinked* proof entry so `OVT_TutorialContext.c:499-500` (the branch that hides the Learn More control when an entry has no `m_sFieldManualTitleKey`) shipped covered. Both welcomes now carry `#OVT-FieldManual_Welcome_Title`, and `tutorial-content`'s ten all link, so **nothing live exercises the hidden branch**. Accepted (D8): the branch is UI-only, outside the automated spine, and a one-line inspection. Also carried in `tasks.md` under Technical Debt. A future content pass that wants live coverage back need only leave one entry unlinked.

### New in Phase 4 — `overrideDifficulty` replaces `startingCash` after the setup screen has spoken

A server with `overrideDifficulty` set in `Overthrow_Config.json` replaces the selected preset **after the campaign starts** (`OVT_OverthrowGameMode.c:364`). The campaign-setup screen's numbers line is composed from the *selected* preset, so on such a server it shows the preset's authored figure and the player then receives the override's. Not a defect of this feature — the screen reports what was chosen, honestly — but it is the one configuration in which the number a player reads is not the number they get, and it belongs in the backlog with the other three.

---

## Proven-Red Table

*Every automated case added by this feature must be proven able to fail once, with the exact failure text, the method and the date. `maxAttempts` is banned — a test that needs retries is a bug in the test. Populated by Phase 5.*

| Case | Tier | Prove-red method | Failure text | Date |
|---|---|---|---|---|
| `OVT_TEST_Logic_Tutorial_SpawnContextSelectsOneWelcome` | Logic | Swapped the expected id on the house branch (`house.Get(0) != HOUSE_ID` → `!= NOHOUSE_ID`), then reverted | `A 'house' spawn context selected 'welcome-intro', expected 'welcome-nohome' - the player who was given a house would read the houseless page` | 2026-08-09 |
| `OVT_TEST_Init_Tutorial_ManagerResolvesAndLoadsEntries` — **`CheckSpawnFilters` branch (a)** | Init | Misspelled the filter in `Configs/Tutorials/welcomeNohome.conf`: `m_sFilter "nohouse"` → `"nohosue"`, then reverted | `Tutorial entry 'welcome-nohome' filters PLAYER_SPAWNED on 'nohosue', which is not a spawn context. The value it is compared against is authored by the server in OVT_OverthrowGameMode.FinalizePlayerPreparation and carried to the client by OVT_TutorialComponent, and it is only ever 'house' or 'nohouse'. This filter can therefore never match ANY spawn: the entry will SILENTLY NEVER FIRE - no compile error, no runtime warning, no log line. Fix the m_sFilter in the entry's .conf under Configs/Tutorials/ to 'house', 'nohouse', or "" for either.` — **names the offending entry**, which is the whole point of the guard | 2026-08-09 |
| `OVT_TEST_Init_Tutorial_ManagerResolvesAndLoadsEntries` — **`CheckWelcomeCoverage` branch (b)** | Init | Set `m_bEnabled 0` on `welcomeNohome.conf`, then reverted. **Proven separately from branch (a)** — they are different failures and a single prove-red would only have exercised one of them | `No enabled tutorial entry matches the 'nohouse' spawn context. A player for whom no starting house was free spawns at a bus stop with no house and no car, and would see no welcome at all - the exact player this feature exists for, and the one nobody play-tests. Check that Configs/Tutorials/welcomeNohome.conf still carries a PLAYER_SPAWNED trigger filtered 'nohouse', is still m_bEnabled 1, and is still listed in m_aTutorialEntries on the game mode prefab.` | 2026-08-09 |
| `OVT_TEST_Campaign_Tutorial_SpawnContextIsAuthored` | Campaign | Commented out the `SetPlayerSpawnContext(..., SPAWN_CONTEXT_HOUSE)` call in the home branch of `FinalizePlayerPreparation`, then reverted | `Player d13d3018-3b6a-461e-af2d-9e8ee5c4fddf was finalized by the campaign start but has NO spawn context ('' - unknown). FinalizePlayerPreparation took one of its two branches and neither recorded what the player was given, so nothing on the server can answer which welcome they should read, and every client falls back to the house page - including a player who spawned at a bus stop with no house and no car. Check that both branches of FinalizePlayerPreparation still call SetPlayerSpawnContext (the fallback branch with 'nohouse', the home branch with 'house').` | 2026-08-09 |

**No `maxAttempts` anywhere.** The only occurrence of the word in the three touched files is a pre-existing comment at `OVT_TEST_InitSuite.c:1497` saying there is none. Neither new case polls: T1 is pure, and T3's fact is written synchronously during campaign start (the same finalized-state precondition `OVT_TEST_Campaign_ContinuePlayerIdMapping` asserts without polling).

---

## Test Baseline

*Measured in task 0.1 on this tree, not inherited — `CLAUDE.md`, `MEMORY.md` and the sibling plans disagree with each other and with the tree.*

| Group | Baseline (measured 2026-08-09) | Target at feature close |
|---|---|---|
| Fast `{6A6E29FF47ECB840}` | **50 tests, exit 0** (24 s) | **51 — measured at close.** Baseline +1 (Logic T1). *The planned "52" double-counted Init T2, which is a branch on an existing case and adds assertions, not cases* |
| All `{6A6E2A002F53A581}` | **86 tests, exit 0** (30 s) | **88 — measured at close.** Baseline +2 (Logic T1, Campaign T3), same correction |
| `tools/compile-check.sh` | **exit 0** — "OK (5944 files, Game module, 6s)" | exit 0 |

⚠️ **These supersede every other number in the tree.** `CLAUDE.md` says Fast 38 / All 66; `MEMORY.md` says Fast 40 / All 75; `implementation.md` §4 counted case *declarations* as Logic 30 + Init 20 + Campaign 12. The measured **executed** counts are 50 and 86. Do not "correct" a phase gate against any of the other three figures.

**GUID block re-check (0.1).** `grep -rEoh "\{6B3D[0-9A-F]{12}\}" --include=* .` returns exactly four matches — `…0010`, `…0016`, `…0021`, `…0022` — and every one of them is inside `implementation.md`'s / `tasks.md`'s own allocation table. **No `6B3D…` GUID exists in any `.conf`, `.et`, `.layout`, `.meta` or `.st` in the tree.** The block is free.

**`git status` (0.1).** Branch `new-player-experience`, nothing staged or modified; three untracked files, all this feature's own docs (`context.md`, `implementation.md`, `tasks.md`). No concurrent session's work is in the tree.

**`PLAYER_SPAWNED` sweep (0.3).** `grep -l PLAYER_SPAWNED Configs/Tutorials/*.conf` names **`Configs/Tutorials/proofWelcome.conf` and nothing else**, across all eleven shipped entry configs. `tutorial-content`'s ten genuinely do not use the event, so §3.3's backward-compatibility argument (populating `ctx.m_sFilter` cannot change what any other entry matches) holds by inspection, not just by `OVT_TutorialTrigger.Matches:116`'s `""` sentinel.

---

## §3.7 Re-verification (0.2)

*Re-checked line by line against the tree on **2026-08-09**. A trap table is not evidence, so every citation below was opened.*

**Verdict: all seventeen rows still hold. No row fell, so `implementation.md` §3.7 is unamended.**

### Verified true (9/9 stand)

| Row | Re-verified at | Note |
|---|---|---|
| Random starting house → home → starting car | `OVT_OverthrowGameMode.c:1011-1023` | Exact. `SetOwner` :1021, `SetHome` :1022, `SpawnStartingCar` :1023 |
| Car parks in a parking spot or at a kerb, **not** a garage | `OVT_VehicleManagerComponent.c:169-191` | Exact. `GetParkingSpot(... PARKING_CAR)` :179 else `FindNearestKerbParking(..., 20, ...)` :183 |
| Home is the respawn position | `OVT_SpawnLogic.c:786-802` | Holds. Cited as `:787-802`; `vector homePos = player.home` is line **786**. Trivial drift, not a fall. Note `m_vLastKnownPosition` takes priority (`:780-785`) |
| Starting cash from the preset, granted once on the new-player branch | `OVT_OverthrowGameMode.c:1043-1053` | Exact. `startingCash` read :1048, `AddPlayerMoney` :1049, inside the `else` (not-`initialized`) branch |
| No free house → fallback spawn, home position but **no owned building and no car** | `OVT_OverthrowGameMode.c:1012-1017`, `:824-839` | Exact. The `!house` branch calls only `SpawnPlayerAtFallbackPosition`; that method calls only `SetHomePos` (:830 bus stop / :835 starting town centre) |
| Overthrow menu has **12** entries, **no** money screen, **no** Field Manual | `UI/Layouts/Menu/MainMenu.layout`, `OVT_MainMenuContext.c` | Re-counted: exactly 12 `m_sText "#…"` labels (MapInfo :172, FastTravel :212, Resistance :252, Jobs :292, Place :332, Build :372, RealEstate :412, ManageRecruits :452, CharacterSheet :492, Tips :532, Save :572, Options :613). No money entry, no Field Manual entry |
| Tips shown once per machine; the **Tips** entry re-opens the most recent | `OVT_MainMenuContext.c:290-301`, `OVT_TutorialComponent.NotifyDismissed:380-396` | Exact. `Tips()` at :290 calls `OVT_TutorialContext.ShowLayout()`; `NotifyDismissed` does `MarkSeen` + `PersistSettings` at :390-391 |
| `<action name="OverthrowMainMenu"/>` renders a key glyph in a body | `OVT_TutorialContext.c:415` (`RichTextWidget`), `chimeraInputCommon.conf:71` (the action exists) | Exact, and now with a **shipped precedent**: `.st:1861` (`OVT-Hint_MainMenu`) and `.st:3340` (`OVT-IntroHint`) both use the markup, in six languages |
| The occupying faction's uniform is the disguise that works | `OVT_PlayerWantedComponent.c:111` + consumers | Holds. `IsDisguisedAsOccupying()` has **7** live call sites incl. `SCR_CharacterDamageManagerComponent.c:27,69,121` (row said "5"; more, not fewer) |

### Verified false or unusable (8/8 stand)

| Row | Re-verified | Note |
|---|---|---|
| "Real estate costs scale with difficulty" | **Still dead.** `realEstateCostMultiplier` has exactly 3 script references: the declaration `OVT_DifficultySettings.c:65` and the `RplSave`/`RplLoad` pair `OVT_OverthrowConfigComponent.c:564,619`. `GetBuyPrice` (`OVT_RealEstateManagerComponent.c:712-723`) and `GetRentPrice` (`:727+`) compute from `m_BasePrice`/`m_BaseRent`, `m_DemandMultiplier`, town population and stability, and never touch it | F1 confirmed |
| "Dying costs you $N" | **Still gated.** `ChargeRespawn` (`OVT_EconomyManagerComponent.c:1869-1877`) wraps `TakePlayerMoney` in `if (money > 500)`, hardcoded, unrelated to `respawnCost` (read :1874) | F2 confirmed |
| Supporting faction sends troops / supplies gear / affects prices | **All three still false.** `Configs/Deployment/Deployment_TownPatrol.conf:26` is `m_iAllowedFactionTypes OCCUPYING_FACTION`; `Deployment_VehiclePatrol_Light/Heavy.conf` omit the field entirely so they take the `[Attribute("1")]` default (`OVT_DeploymentConfig.c:22-23`) = `OCCUPYING_FACTION` (`OVT_OverthrowConfigComponent.c:14`); `CanFactionUse` is a bitmask test (`OVT_DeploymentConfig.c:95-101`). `m_bIncludeSupportingFactionItems` is `[Attribute("true")]` at `OVT_EconomyManagerComponent.c:38-39` and **no `.conf` or `.et` in the tree sets it to 0**, so the filters at `OVT_TownController.c:314,330` and `OVT_EconomyManagerComponent.c:1777` never remove anything | |
| "You can disguise as the supporting faction" | **Still the opposite.** `OVT_PlayerWantedComponent.c:447-453` groups the supporting key with the player key as "not disguised"; `:709-715` sets `newLevel = 2` on sight for either. `IsDisguisedAsSupporting()` (`:130-140`) still has **zero** callers | |
| "You fight alongside the supporting faction" | **Still false.** `Character_Player.et:177-178` — `SCR_CharacterFactionAffiliationComponent { "faction affiliation" "CIV" }`. Recruits are set to the player faction (`OVT_RecruitManagerComponent.c:1720`). FIA is excluded from both spinners by `OVT_StartGameContext.c:98` (`if(faction.IsPlayable()) continue;` — FIA_OverthrowData.conf:3 is `m_bIsPlayable 1`) | |
| "You start with a pistol" | **Still difficulty-dependent.** Easy/Normal/Hard `startingItems` = M9 + 9x19 magazine + R148 radio; **Extreme and Insane = R148 radio only**. Read at `OVT_SpawnLogic.c:1177` (exact line) | |
| "The occupier comes looking for you / in that area" | Unchanged; not repeated anywhere in this feature's drafts | |
| Money stolen/deposited; menu holds money or the Field Manual | Unchanged; the 12-entry re-count above is the positive proof | |

### Supporting-faction truthful frame (re-verified, unchanged)

Perception effect (`OVT_PlayerWantedComponent.c:447-453`, `:709-715`) and occupier coupling (`OVT_StartGameContext.c:141-168`, `:169-196` — each spin handler walks the *other* spinner and moves it when the keys collide). **The shipped spinner list is US and USSR, confirmed by construction**: `OVT_OverthrowFactionManager.et:24-31` registers four `OVT_Faction`s (USSR, US, FIA, CIV); `:98` drops FIA (playable), `:99` drops CIV.

---

## Field Manual Welcome Inheritance (0.4)

The four manual strings live at `Language/localization_Overthrow.st:2765` (`_Head`), `:2782` (`_Head2`), `:2799` (`_Text`), `:2816` (`_Text2`), plus **`:2833` (`_Text3`)**, which the plan's task list omitted and which is the actual source for the "no required order" half of welcome page 1.

| Manual string | Its own cited source (from its `Comment`) | Inherited by |
|---|---|---|
| `#OVT-FieldManual_Welcome_Head` "What You Start With" | *(header, no factual content)* | nothing; framing only |
| `#OVT-FieldManual_Welcome_Head2` "No Set Order" | *(header, no factual content)* | nothing; framing only |
| `#OVT-FieldManual_Welcome_Text` | `docs/mission-statement.md` (persistent, no scripted mission, server-authoritative) | welcome **page 1, sentence 1** ("persistent sandbox … occupied island") and **page 1, sentence 3** (durable state carries across sessions/restarts) |
| `#OVT-FieldManual_Welcome_Text3` | `docs/mission-statement.md` (sandbox, no mission order, multiplayer-first) | welcome **page 1, sentence 2** ("no objective list and no required order") |
| `#OVT-FieldManual_Welcome_Text2` | `OVT_OverthrowGameMode.c:1011-1023` (house, home, car), `:1048` (cash), `OVT_SpawnLogic.c:787-802` (home is respawn). **Deliberately does not quote the cash amount** | welcome **page 2 (house variant), both sentences** |

**Compression rules taken from this mapping.**
- Page 1 is pure compression of `_Text` + `_Text3`. It adds nothing and it drops the multiplayer clause and the list of tools, both of which belong to the manual's longer form.
- Page 2 (house) is pure compression of `_Text2`, keeping its "registered as your home, which is where you respawn" and its refusal to quote a cash figure.
- **`_Text2` has no houseless clause**, so `welcome-nohome` page 2 inherits nothing and must stand on its own citations (see the table below). Phase **6.1** is where the manual gains a clause, not Phase 2.

---

## Difficulty Numbers Line (0.5) — settled

**Confirmed live reads.** `startingCash` at `OVT_OverthrowGameMode.c:1048`; `fastTravelCost` at `OVT_MapContext.c:384` (base) and `:395` (per nearby recruit). Both exact, both unchanged.

**Both disqualifications re-confirmed.** `realEstateCostMultiplier` is dead (declaration + `RplSave`/`RplLoad` only); `respawnCost` is behind the hardcoded `money > 500` at `OVT_EconomyManagerComponent.c:1874`.

### The third number does not earn its place. Ship two.

Every remaining candidate fails the "a verified live read a player meets in the first hour, statable as one number" test:

| Candidate | Live? | Why it is cut |
|---|---|---|
| `patrolGroupsMin/Max` | yes | **Two read sites with different arithmetic.** `OVT_OccupyingFactionManager.c:508` uses the raw range; `OVT_InfantrySpawningDeploymentModule.c:224` uses `Ceil(min*0.5)`…`Ceil(max*0.5)`. Any single quoted range is false at one of the two. Usable only as a *relative* claim, which is where it went (Hard/Insane descriptions) |
| `baseRecruitCost` | yes, 10 sites | Almost never the plain value: `*0.5` at `OVT_PlayerCommsComponent.c:1366` and `OVT_RecruitFromTentAction.c:46`, `*slotCount` at `OVT_BaseMenuContext.c:60` and `OVT_FOBMenuContext.c:45`, `(base+300)*slotCount` at `OVT_ResistanceFactionManager.c:912`. Only `OVT_RecruitCivilianAction.c:24` is bare. "Recruits cost $N" is false at most sites |
| `busTicketPrice` | yes, `OVT_MapContext.c:462` | It is a **rate per km** (`Round((dist/1000) * busTicketPrice)`), not a price. Needs a unit the line has no room for, and sits confusingly next to fast travel, which is its alternative |
| `placeableCostMultiplier` / `buildableCostMultiplier` | yes, `OVT_OverthrowConfigComponent.c:252,257` | A bare "x3" is meaningless to a player who has never seen a base price, and neither is guaranteed to be a first-hour path |

### The format string

```
Id:          OVT-Difficulty_Numbers
Target_en_us: "Starting cash: $%1     Fast travel: $%2"
```

| Placeholder | Field | Type | Read site that makes it true |
|---|---|---|---|
| `%1` | `OVT_DifficultySettings.startingCash` | `int` → `.ToString()` | `OVT_OverthrowGameMode.c:1048-1049` |
| `%2` | `OVT_DifficultySettings.fastTravelCost` | `int` → `.ToString()` | `OVT_MapContext.c:384` |

- Called as `TextWidget.SetTextFormat("#OVT-Difficulty_Numbers", cash, travel)` — **2 substitution args**, precedent `OVT_TutorialContext.c:459`. Never concatenation.
- **`$` lives inside the format string, not in script.** That is the established `.st` convention (`$%1` / `$%2` at `.st:5522, 5543, 5564, 5648`) and it keeps the currency symbol localizable. `OVT_MoneyFormat.FormatMoney` is *not* needed here: the largest value across all five presets is 500, so thousands-grouping never fires.
- Label wording is `Fast travel`, singular and unqualified, because `fastTravelCost` is charged **again per nearby recruit** (`OVT_MapContext.c:395`) and zeroed in debug mode (`:400`). "Fast travel: $25" is the base cost and must not be labelled as a total.

### The five presets' actual resolved values

⚠️ **Trap for Phase 4: three of these ten values are not in the `.conf` files.** `Difficulty_Normal.conf` omits `startingCash` *and* `fastTravelCost`; `Difficulty_Hard.conf` omits `startingCash`. Those fall back to `[Attribute(defvalue:)]` on `OVT_DifficultySettings.c:54` (`startingCash` = 100) and `:58` (`fastTravelCost` = 5). Anyone verifying by grepping `Configs/Difficulty/` will find blanks and conclude the line is broken. Read the resolved runtime value.

| Preset | `startingCash` | source | `fastTravelCost` | source |
|---|---|---|---|---|
| Easy | 500 | `Difficulty_Easy.conf:12` | 0 | `Difficulty_Easy.conf:14` |
| Normal | 100 | **class default**, `OVT_DifficultySettings.c:54` | 5 | **class default**, `OVT_DifficultySettings.c:58` |
| Hard | 100 | **class default**, `OVT_DifficultySettings.c:54` | 10 | `Difficulty_Hard.conf:17` |
| Extreme | 0 | `Difficulty_Extreme.conf:16` | 25 | `Difficulty_Extreme.conf:18` |
| Insane | 0 | `Difficulty_Insane.conf:15` | 25 | `Difficulty_Insane.conf:17` |

§3.4's quoted spreads (500/100/100/0/0 and 0/5/10/25/25) are therefore **correct**, but only once the defaults are resolved. Play-test P2's "Easy shows 500 starting cash and free fast travel" is right; add "Normal shows 100 and 5" as the check that the default fallback works.

---

## Sentence → Evidence Table (Phase 0)

**This is the artefact Phases 2 and 4 author from. Nothing outside it ships.** A sentence with no citation was cut, not softened; the cuts are listed at the end and are the most important part of this section.

Tone gate applied to every row: informs, never instructs; no imperative, no goal, no implied order; reads correctly for a player who ignores all of it for three hours. No em-dashes.

### A. Welcome pages (5 distinct bodies, 11 sentences)

`welcome-intro` pages 1/3/4 and `welcome-nohome` pages 1/3/4 are the **same three strings** (D3). Page 2 is the only divergence.

#### Page 1 — `#OVT-Tutorial_WelcomeIntro_Body1` (rewritten; both entries)

| # | Sentence | Evidence |
|---|---|---|
| 1.1 | "Overthrow is a persistent sandbox campaign fought on an occupied island." | `#OVT-FieldManual_Welcome_Text` (own source: `docs/mission-statement.md`) |
| 1.2 | "There is no objective list and no required order, and the island reacts to what you do." | `#OVT-FieldManual_Welcome_Text` + `#OVT-FieldManual_Welcome_Text3` (own source: `docs/mission-statement.md`) |
| 1.3 | "Towns, money, property, vehicles and captured ground are durable state, so a campaign carries on between sessions and across server restarts." | `#OVT-FieldManual_Welcome_Text` (own source: `docs/mission-statement.md`); corroborated by the shipped `persistence` feature and `OVT-Tutorial_HomeFirstOpen_Body`'s own citation `OVT_RealEstateManagerComponent.c:333-350` + `:255+` |

Markup: `<br/><br/>` between 1.2 and 1.3.
**The shipped clause "You start with a house, a car and a little money" is deleted from this string** (task 2.4). It is false for `welcome-nohome`, which shares this key, and it belongs on page 2.

#### Page 2, house variant — `#OVT-Tutorial_WelcomeIntro_Body2` (rewritten; `welcome-intro` only)

| # | Sentence | Evidence |
|---|---|---|
| 2.1 | "You have been given a house in one of the island's towns, with a car parked outside it, and some starting cash." | `OVT_OverthrowGameMode.c:1011-1023` (random starting house, `SetOwner` :1021, `SetHome` :1022, `SpawnStartingCar` :1023); `OVT_VehicleManagerComponent.c:179-191` (parking spot, else a kerb within 20 m — **"outside it", never "garage"**); `OVT_OverthrowGameMode.c:1048-1049` (cash). Compresses `#OVT-FieldManual_Welcome_Text2` |
| 2.2 | "That house is your home, which is where you respawn." | `OVT_OverthrowGameMode.c:1022` (`SetHome`); `OVT_SpawnLogic.c:786-802` (`player.home` is the spawn position). Compresses `#OVT-FieldManual_Welcome_Text2` |

The cash **amount** is deliberately not quoted: it is a difficulty setting (see the numbers table above), and the manual string it compresses refuses to quote it for the same reason. **Stops here** — ownership semantics belong to `home-first-open` (I1).

#### Page 2, houseless variant — `#OVT-Tutorial_WelcomeNohome_Body2` (new; `welcome-nohome` only)

| # | Sentence | Evidence |
|---|---|---|
| 2.1n | "No starting house was free, so you began in one of the island's towns rather than at a home of your own." | `OVT_OverthrowGameMode.c:1012-1017` (the `!house` branch calls **only** `SpawnPlayerAtFallbackPosition`: no `SetOwner`, no `SetHome`, no `SpawnStartingCar`); `:824-839` (a bus stop, or the starting town centre if none) |
| 2.2n | "You still have your starting cash, and for now you respawn where you started." | `OVT_OverthrowGameMode.c:1043-1053` (the cash block is outside the house `if/else`, so it runs on both branches); `:830` / `:835` (`SetHomePos` on the fallback path); `OVT_SpawnLogic.c:786-802` |
| 2.3n | "Property on the island can be bought, and a building you own can be set as your home." | `OVT_RealEstateManagerComponent.c:712` (`GetBuyPrice` exists and buildings are purchasable); `OVT_RealEstateContext.c:267-268` (the **Set as Home** button, enabled only when `isOwner`) and `:459-480` → `OVT_PlayerCommsComponent.c:408-434` → `OVT_RealEstateManagerComponent.c:567-576` |

Must read correctly for a player who has cash and a home *position* but owns nothing. 2.3n is the one place the houseless entry has to name buying, because §3.1's page plan requires it and `_Text2` gives it nothing to inherit. It stays to one sentence and does not restate `home-first-open`.

#### Page 3 — `#OVT-Tutorial_WelcomeIntro_Body3` (new; both entries)

| # | Sentence | Evidence |
|---|---|---|
| 3.1 | "Most of what Overthrow adds sits in the Overthrow menu, opened with `<color rgba='226,168,79,255'><action name="OverthrowMainMenu"/></color>`." | `Configs/System/chimeraInputCommon.conf:71` (`Action OverthrowMainMenu`); body is a `RichTextWidget` (`OVT_TutorialContext.c:415`); markup precedent `.st:1861` and `.st:3340`, both shipped in six languages |
| 3.2 | "It holds the map, fast travel, the resistance, property, your recruits and your character sheet." | `UI/Layouts/Menu/MainMenu.layout:172, 212, 252, 412, 452, 492`, wired at `OVT_MainMenuContext.c:109, 116, 130, 155, 163, 170` |

**Deliberately omits `Jobs`** (`MainMenu.layout:292`) even though it exists: `starter-jobs-retirement` (#5) follows this feature and §8 requires naming only entries with no pending change. Also omits Place, Build, Options and Save for length, and Tips because page 4 is about it.

#### Page 4 — `#OVT-Tutorial_WelcomeIntro_Body4` (new; both entries)

| # | Sentence | Evidence |
|---|---|---|
| 4.1 | "Occasional tips like this one appear the first time something new happens, and each is shown once." | `OVT_TutorialComponent.NotifyDismissed:380-396` (`MarkSeen` + `PersistSettings` at :390-391, per-machine seen store) |
| 4.2 | "The Tips entry in the Overthrow menu reopens the most recent one." | `MainMenu.layout:532` (`#OVT-MainMenu_Tips`); `OVT_MainMenuContext.c:184` (wiring) and `:290-301` (`Tips()` → `OVT_TutorialContext.ShowLayout()`) |

`_Body2`'s existing 12-entry menu fact-check `Comment` moves here with the text it belongs to (task 2.4).

### B. Campaign-setup descriptions — the count is **9**, not 8

Derived, not assumed: the spinners are built by `OVT_StartGameContext.c:94-108`, which walks `GetFactionsList()` and drops any faction where `IsPlayable()` is true (`:98`) or the key is `"CIV"` (`:99`). `OVT_OverthrowFactionManager.et:24-31` registers exactly four `OVT_Faction`s — USSR, US, FIA, CIV — and `FIA_OverthrowData.conf:3` is `m_bIsPlayable 1`. **Two factions survive: US and USSR.** Two roles each = 4 faction strings, plus 5 difficulty strings = **9**. The plan's "eight" was an estimate and was one short.

#### B1. Occupying-faction descriptions (2)

`#OVT-Faction_US_Occupying`, `#OVT-Faction_USSR_Occupying`

| # | Sentence | Evidence |
|---|---|---|
| O.1 | "The island is held by *(US / Soviet)* forces." | `OVT_StartGameContext.c:92` / `:168` (`SetOccupyingFaction`); the faction identity itself is `Configs/Factions/US.conf` / `USSR.conf` via `OVT_OverthrowFactionManager.et:13-15` |
| O.2 | "Their troops garrison the bases and radio towers and patrol the towns." | `OVT_OccupyingFactionManager.c:508` (defence groups at bases/towers, sized by `patrolGroupsMin/Max`); `OVT_BaseUpgradeDefensePosition.c:48` (`defenseGroupsBaseMax`); `Configs/Deployment/Deployment_TownPatrol.conf:26` (`OCCUPYING_FACTION`) |
| O.3 | "Their uniform is the one that works as a disguise." | `OVT_PlayerWantedComponent.c:111` (`IsDisguisedAsOccupying`) with 7 live consumers, incl. `SCR_CharacterDamageManagerComponent.c:27, 69, 121` and `OVT_PlayerWantedComponent.c:194, 584, 612` |

O.2 and O.3 are **identical for both factions**; only O.1 differs. That is honest: see cut C6.

#### B2. Supporting-faction descriptions (2)

`#OVT-Faction_US_Supporting`, `#OVT-Faction_USSR_Supporting`

| # | Sentence | Evidence |
|---|---|---|
| S.1 | "*(The United States / The Soviet Union)* is a foreign power sympathetic to the resistance, and not your own faction." | The player character is affiliated `CIV` (`Character_Player.et:177-178`) and recruits are set to the player faction (`OVT_RecruitManagerComponent.c:1720`); neither is the supporting faction |
| S.2 | "The occupation treats its uniform as hostile in exactly the way it treats the resistance's, so being seen wearing it means an immediate wanted level." | `OVT_PlayerWantedComponent.c:447-453` (supporting key grouped with the player key as "not disguised") and `:709-715` (`newLevel = 2` on sight for either) |
| S.3 | "The occupier and the supporter cannot be the same, so changing one of these also moves the other." | `OVT_StartGameContext.c:141-168` and `:169-196` (each spin handler walks the other spinner and reassigns it on a key collision) |

**Nothing else may be added.** The five richer claims are re-verified false above.

#### B3. Difficulty descriptions (5)

`#OVT-Difficulty_{Easy,Normal,Hard,Extreme,Insane}_Desc`. Each keeps the shipped recommendation verbatim (it is an authored recommendation, which carries itself) and gains at most one clause with a citation.

| Key | Sentence | Evidence |
|---|---|---|
| `_Easy_Desc` | "Recommended for solo players. Occupying garrisons and patrols are at their smallest, and you start with a handgun, a magazine and a radio." | Recommendation: `Difficulty_Easy.conf:3` (shipped `description`). Patrols: `Difficulty_Easy.conf:9-10` (`patrolGroupsMin 1` / `Max 2`, lowest of the five) + `Difficulty_Easy.conf:11` (`defenseGroupsBaseMax 2`), read at `OVT_OccupyingFactionManager.c:508`, `OVT_InfantrySpawningDeploymentModule.c:224`, `OVT_BaseUpgradeDefensePosition.c:48`. Items: `Difficulty_Easy.conf` `startingItems` (M9, 9x19 magazine, R148), read at `OVT_SpawnLogic.c:1177` |
| `_Normal_Desc` | "Recommended for small servers, hosted games and experienced solo players. This is the preset a hosted or dedicated game starts on." | Recommendation: `Difficulty_Normal.conf:3`. Default: `OVT_StartGameContext.c:133-135` (`spin.SetCurrentItem(1)` on the non-`RplMode.None` branch; `:127-131` picks index 0 in single player) |
| `_Hard_Desc` | "Recommended for busy servers. Garrisons and patrols are larger, and placing, building and recruiting all cost more." | Recommendation: `Difficulty_Hard.conf:3`. Patrols: `Difficulty_Hard.conf:11-13` (`patrolGroupsMin 4` / `Max 6` / `defenseGroupsBaseMax 10`). Costs: `Difficulty_Hard.conf` `placeableCostMultiplier 1.5` / `buildableCostMultiplier 1.5`, read at `OVT_OverthrowConfigComponent.c:252, 257`; `baseRecruitCost 500`, read at `OVT_RecruitCivilianAction.c:24` |
| `_Extreme_Desc` | "Recommended for very busy servers. You start with no cash and a radio alone, and fast travel is unavailable while a QRF is active." | Recommendation: `Difficulty_Extreme.conf:3`. Cash: `Difficulty_Extreme.conf:16` (`startingCash 0`). Items: its `startingItems` is the R148 radio only, read at `OVT_SpawnLogic.c:1177`. QRF: `Difficulty_Extreme.conf` `QRFFastTravelMode DISABLED`, read at `OVT_MapContext.c:78-80` |
| `_Insane_Desc` | "Recommended for highly experienced servers. You start with no cash and a radio alone, and the occupation fields its largest garrisons and patrols." | Recommendation: `Difficulty_Insane.conf:3`. Cash: `:15`. Items: `startingItems` = R148 only, read at `OVT_SpawnLogic.c:1177`. Patrols: `Difficulty_Insane.conf:11-13` (`patrolGroupsMin 8` / `Max 12` / `defenseGroupsBaseMax 30`, highest of the five) |

The patrol clauses are **relative** ("smallest", "larger", "largest"), never a number. That is what makes them survivable despite the two read sites disagreeing on arithmetic: both scale monotonically with the same two fields, so the ordering is true at both, and no single location is named.

Naming the handgun is legal **here** and illegal on the welcome pages: a per-preset description states it for the preset it is true of, whereas the three shared welcome pages are read by players on all five.

### C. Sentences CUT for lack of evidence

| # | Cut | Why |
|---|---|---|
| **C1** | "…the town around you…" from page 3's "where to look" list (§3.1's page plan) | No citable first-hour behaviour of "a town" that is not already owned by another entry. The honest versions (shops, stability, population) belong to `shops-first-gun-dealer` and the towns feature. Cut rather than softened |
| **C2** | "The map shows you where things are", as its own page-3 item | Map info and fast travel are gated on carrying a map gadget (`OVT_MapContext.c:135-146`, `:152-158`, `:277-285`), and stating the capability without the gate is a lie for a player who dropped the map. That gate is `map-first-open`'s sentence (`OVT-Tutorial_MapFirstOpen_Body`). Page 3 names the map only as a menu entry, which is unconditionally true |
| **C3** | Any starting-equipment sentence on a **shared** welcome page ("a handgun in your pants", "you start armed") | `startingItems` is R148-only on Extreme and Insane (`OVT_SpawnLogic.c:1177`). Survives only in the per-preset difficulty descriptions |
| **C4** | "…a car in your garage" | `OVT_VehicleManagerComponent.c:179-191` parks it in a parking spot or at a kerb within 20 m, and prints a failure when neither exists. Replaced with "parked outside it" |
| **C5** | "Dying costs you money" in any difficulty description; "houses cost more on higher difficulty" in any difficulty description | F2 (hardcoded `money > 500` gate) and F1 (dead field). Re-verified 2026-08-09 |
| **C6** | Any per-faction *mechanical* difference between US and USSR beyond identity ("you will be facing NATO-pattern weapons", "Soviet armour is heavier") | Nothing in `US_OverthrowData.conf` / `USSR_OverthrowData.conf` differs except which prefabs the generic slot names (`light_patrol`, `heavy_armed`, `Officer`) resolve to. The claim would be about base-game equipment I did not trace. The two occupying descriptions therefore differ **only in the faction name**, and that is the correct outcome, not laziness |
| **C7** | "The supporting faction supplies you with gear / sends troops / lowers prices / can be worn as a disguise / fights alongside you" | All five re-verified false 2026-08-09 (§3.7 table above) |
| **C8** | A third number in `#OVT-Difficulty_Numbers` | No candidate survives the "one number, one read site, first hour" test. See the 0.5 table |
| **C9** | "Money can be stolen from you" / "the Overthrow menu has a money screen or the Field Manual" | Already-shipped-once lies; the 12-entry re-count is the positive disproof |

---

## Legacy Hint Removal — Observed Change Table (3.1)

*Verified by reading on **2026-08-09**, before anything was deleted. **§3.5's table is CONFIRMED, not contradicted.** Only line numbers had drifted (Phase 1 moved this file), so every citation below is the re-read location.*

**The three facts, each re-read:**

1. **`OnPlayerSpawnedLocal` has exactly one caller and fires on EVERY possession.** `OVT_UIManagerComponent.AfterControlledByPlayer:138` (`Scripts/Game/Components/Player/OVT_UIManagerComponent.c` — the plan's path omitted `Player/`) calls it from the `else if (owner)` branch, i.e. every time `controlled` is true. There is **no first-possession guard** anywhere on the path; the only dedup was `m_aHintedPlayers` itself. `grep -rn "OnPlayerSpawnedLocal" Scripts/` finds one definition, one call site, the rest comments.
2. **`m_bGameStarted` is authority-only, hence false on every dedicated-server client, always.** Declared `OVT_OverthrowGameMode.c:109` (plan said `:92-94`), written at exactly one place, `:270` in `DoStartGame()` (plan said `:255`). `DoStartGame()` has one live caller, `OVT_StartGameContext.StartGame:294` (host/SP start menu), plus `ScheduleStartGame()` from the restore paths, which are `IsMaster()`-guarded. No client path writes it. Its own doc comment at `:107` already said so; that is now re-verified rather than inherited.
3. **The SP / listen-host respawn-after-death and Continue paths reach it with `m_bGameStarted` true.** Both are possessions that happen *after* `DoStartGame()` has run in that session, so the gate passes and the hint genuinely fires today.

| Situation | Hint before removal | After removal | Verified at |
|---|---|---|---|
| Dedicated-server client, any spawn | **never** — `m_bGameStarted` is false on a client, always | **no change** | `:109` decl, `:270` sole write, inside `DoStartGame()` |
| SP / listen host, **first** spawn of a new campaign | **never** — possession precedes Start Game | **no change** | `OVT_SpawnLogic.DoSpawn_S`; the same ordering is documented and asserted by `OVT_TEST_Campaign_TutorialSpawnTrigger.c:4-10` |
| SP / listen host, **respawn after death** | **fires**, once per persistent id per session (`m_aHintedPlayers` was reallocated in `EOnInit`) | **gone** — a veteran stops being welcomed after dying | one caller on every possession (`OVT_UIManagerComponent.c:138`) + `m_bGameStarted` true by then |
| SP / listen host, campaign resumed with **Continue** | **fires** — `RestoreStartedCampaign` → `ScheduleStartGame` → `DoStartGame` has already run when possession lands | **gone** | `OVT_OverthrowGameMode.c:439-460`, `:410` |
| Any machine, the `PLAYER_SPAWNED` tutorial push | unaffected | **unaffected** — the push block was left byte-identical | `OnPlayerSpawnedLocal` body after the deletion |

**So the observed change is exactly §3.5's: the corner hint stops appearing after a death and after a Continue, in single player or on a listen host, and changes nothing anywhere else.** That is the requirements' "repeats every session" complaint being answered, not a regression — the welcome modal now covers the same ground once per machine and without the hint's three false claims.

**The code comment's own claim was two-thirds true, as the plan predicted.** "Dead since the 1.6 spawn rework" is true for the dedicated-client row and true for the new-campaign first-spawn row, and **false** for the respawn and Continue rows. It was not inherited, and it is now deleted along with the code it described.

---

## Session Notes

### 2026-08-09 — Phase 6 complete (help and documentation sync) — FEATURE CLOSED 49/49

**6.1 required a string change, and therefore one more Workbench export.** `#OVT-FieldManual_Welcome_Text2` did contradict `welcome-nohome`: it stated flatly that a new character is given a house with a car parked outside it, and this page is the Learn-more target of **both** welcome entries, so a player who took the fallback spawn could follow "Learn more" straight into a paragraph telling them they were given a house. **Exactly one sentence was added**, between the existing sentence 2 and the closing "Everything past that is earned, bought, built or taken": *"When no starting house is free, a new character begins in one of the towns instead, with the starting cash but no house and no car, and respawns where they started until a building they own is set as their home."* Nothing else on the page moved. `_Text`, `_Text3`, `_Head` and `_Head2` were read and are consistent with the shipped pages (`_Head` "What You Start With" still covers the paragraph now that it describes both branches). The `Comment` carries the new citations (`OVT_OverthrowGameMode.c:1037-1046` and `:838-853` for the fallback branch, `:1043-1053` for cash on both branches, `OVT_SpawnLogic.c:787-802` for respawn, `OVT_RealEstateContext.c:267-268` → `OVT_RealEstateManagerComponent.c:567-576` for Set as Home) and the standing instruction that it must not say more than `OVT-Tutorial_WelcomeNohome_Body2` does. **`localization_Overthrow.st` was the only file under `Language/` touched; no `.conf` under `Configs/FieldManual/` needed a change**, since the page composes from these keys.

⚠️ **The user owes ONE more string export**: `OVT-FieldManual_Welcome_Text` **2** (id `OVT-FieldManual_Welcome_Text2`). Until it runs, the manual's Welcome page renders the *old* three-sentence paragraph from the existing runtime export — it will not show a raw key, so this failure is silent rather than obvious.

**6.2 — two wiki pages updated, zero created, budget met.**

| Page | id | Reason (one line) |
|---|---|---|
| `difficulty` | **48** | "Respawn Cost: Money deducted when you die" asserted a flat death cost; `ChargeRespawn` charges **only** while `money > 500` (`OVT_EconomyManagerComponent.c:1873`). Clause added, plus one sentence stating property prices are **not** difficulty-scaled (§3.7 F1, `realEstateCostMultiplier` is read by nothing) |
| `difficulty/settings` | **53** | Same false respawn claim in the `respawnCost` reference entry and again in the "Balancing Economy" tip; both corrected with the hardcoded $500 floor named as hardcoded. Also: `baseDisguiseEffectiveness` now states that only the **occupying** faction's uniform is a disguise, closing the "disguise as the supporting faction" reading (§3.7 — `OVT_PlayerWantedComponent.c:448-452`, `:709-715`). `description` entry noted as holding a localization key, which Phase 4 made true |

**Deliberately NOT touched, and why:**
- **`getting-started` (id 2)** — the `**Tutorial Jobs**` paragraph and item 6 of "Systems Worth Knowing About" belong to `starter-jobs-retirement` (6.3). The page was read only; **it is byte-unchanged**.
- **`factions` (id 3)** — its Supporting Faction section is the obvious §3.7 target, but every claim about aid, equipment drops and intelligence is already under explicit **"(Planned Features)"** headings, so it does not assert something false about the shipped game. The one unhedged sentence ("an external power that aids your resistance movement") and the "Your Allies" heading are soft enough not to spend a budget slot on, and the page carries no wanted-level warning about wearing SF kit. **Recorded as a candidate for the next wiki pass** rather than fixed here.
- **No staleness re-audit** (6.4): `field-manual`'s 2026-08-08 sweep stands, and the two known-wrong manual strings (`WantedSystem_Text`'s "comes looking for you", `BaseCapture_Text`'s "in that area") remain out of scope and untouched, wiki already right on both.

**Wiki hazards, re-confirmed this session.** `wikijs_search_pages` again returned **wrong page ids** (it reported `difficulty` = 25 and `difficulty/settings` = 30; the real ids resolved by slug are **48** and **53**, and search also claimed id 3 for `overthrow-config` when 3 is `factions`). Every write was aimed at an id obtained from `wikijs_get_page(slug=…)` with the returned `path` checked first. Both updates reported `status: updated` and **both were then verified in the live rendered HTML** over plain HTTP, not just in the stored source; no stale-render repair was needed this time.

### 2026-08-09 — Phase 5 complete (tests, verification and the play-test checklist)

**Three assertions added across three tiers, but only TWO new executed cases — and that arithmetic is the one thing in this phase worth reading carefully.**

| Where | What | New executed case? |
|---|---|---|
| `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_Tutorial.c` (appended) | `OVT_TEST_Logic_Tutorial_SpawnContextSelectsOneWelcome` | **Yes** |
| `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c` (two new branches on the existing entry guard) | `CheckSpawnFilters` + `CheckWelcomeCoverage` | **No** — a branch on `OVT_TEST_Init_Tutorial_ManagerResolvesAndLoadsEntries`, exactly as §7 T2 specifies |
| `Scripts/Game/Tests/TestSuites/Campaign/OVT_TEST_Campaign_TutorialSpawnContext.c` (new file) | `OVT_TEST_Campaign_Tutorial_SpawnContextIsAuthored` | **Yes** |

**⚠️ The `+3` target in `tasks.md`, `implementation.md` §4 and the Definition of Done Q7 was arithmetically wrong from the start.** T2 was always specified as a *branch on an existing case*, and a branch adds assertions, not cases. The honest closing figures are **Fast 51** (baseline 50 + T1) and **All 88** (baseline 86 + T1 + T3). Both were measured, both exit 0. Nothing was dropped and nothing failed to register — the case counts were checked against the run before and after. The target line in `tasks.md` has been corrected in place rather than left to send a future session hunting for a missing case.

**Final gates, measured:**

| Gate | Result |
|---|---|
| `tools/compile-check.sh` | **exit 0** — "OK (5945 files, Game module, 5s)" |
| Fast `{6A6E29FF47ECB840}` | **51 tests, exit 0** (24 s) |
| All `{6A6E2A002F53A581}` | **88 tests, exit 0** (28 s) |
| `6B3D…` duplicate-GUID grep | **Clean.** 27 allocations, every one defined once. The single repeated *string* is `{6B3D000000000010}`, which appears in `welcomeNohome.conf.meta`'s `Name` (the definition) and once in `OVT_OverthrowGameMode.et` (the reference to it) — a naive `uniq -d` flags that pair, and it is required, not a duplicate |
| `git diff --stat -- Configs/FieldManual/ Configs/System/` | **Empty.** Byte-unchanged |
| `git diff --stat Language/` | ⚠️ **Now lists the six `.<lang>.conf` runtime exports as well as the `.st` — and that is the USER's Workbench export having run, not an edit by any phase of this feature.** The exports carry this feature's 15 ids (`WelcomeNohome_Body2`, `WelcomeIntro_Body3/4`, the four faction keys, the five difficulty descriptions and `Difficulty_Numbers`). Phase 5 touched no file under `Language/` at all. The gate's intent — that no phase hand-edits a generated export — holds |

**Each of the four assertion sites was proven able to fail, individually.** The Proven-Red Table above carries the exact failure text, the mutation and the date for all four; note that the Init guard's two branches were proven **separately**, because a single mutation would only have exercised one of them and the other would have shipped unproven.

**T3 is honest about what it can and cannot pin.** It asserts "one of the two known values, not `""`", never `"house"` — a returning player's context is legitimately `""` and which branch a start takes is a property of the world's house supply. The prove-red run confirmed the test world takes the **house** branch (commenting out that one call produced `""`), which also confirms the Gotchas entry above: this case would be a hostage to the test world's house count if it hardcoded the value.

**The Phase 1 poll-count canary still reads 0.** `OVT_TEST_Campaign_Tutorial_SpawnTriggerSurvivesCampaignStart` reports delivery on attempt 1 in both full-group runs, so `SetSpawnContext`'s local-direct-call branch is still running — the regression this canary exists for (a missing direct call, which would still *pass* via the ~4.5 s fallback) has not happened.

**Nothing in the feature's shipped code was changed to make a test pass.** The two mutations that touched shipped files (`welcomeNohome.conf`'s filter and `m_bEnabled`, and the game mode's house-branch call) were prove-red mutations, each reverted and each verified reverted by grep and by a re-run of both groups.

### 2026-08-09 — Phase 4 complete (the start-menu descriptions)

**One `.layout`, one `.c`, five `.conf` and the `.st`. 9/9 tasks. Gates: compile-check exit 0 (5944 files, 6 s); All group 86 tests exit 0 — the 0.1 baseline exactly; input-conflict checker exit 0 (0 errors, 0 warnings, 0 pre-existing).**

**The authored copy, in full**, so a reviewer can read the nine descriptions without opening the `.st`. Every claim is a row of the Sentence → Evidence Table above. Nothing was invented; several sentences were **shortened**, which is the one direction compression is allowed to move.

| Key | Shown when | `Target_en_us` |
|---|---|---|
| `OVT-Faction_US_Occupying` | Occupying = US | "US forces hold the island. They garrison its bases and radio towers and patrol its towns, and their uniform is the one that works as a disguise." |
| `OVT-Faction_USSR_Occupying` | Occupying = USSR | "Soviet forces hold the island. They garrison its bases and radio towers and patrol its towns, and their uniform is the one that works as a disguise." |
| `OVT-Faction_US_Supporting` | Supporting = US | "The United States is a foreign power sympathetic to the resistance, and not your own faction. Being seen in its uniform means an immediate wanted level, and it cannot also be the occupier." |
| `OVT-Faction_USSR_Supporting` | Supporting = USSR | "The Soviet Union is a foreign power sympathetic to the resistance, and not your own faction. Being seen in its uniform means an immediate wanted level, and it cannot also be the occupier." |
| `OVT-Difficulty_Easy_Desc` | Difficulty = Easy | "Recommended for solo players. Occupying garrisons and patrols are at their smallest." |
| `OVT-Difficulty_Normal_Desc` | Difficulty = Normal | "Recommended for small servers, hosted games and experienced solo players." |
| `OVT-Difficulty_Hard_Desc` | Difficulty = Hard | "Recommended for busy servers. Garrisons and patrols are larger, and building costs more." |
| `OVT-Difficulty_Extreme_Desc` | Difficulty = Extreme | "Recommended for very busy servers. A radio is the only equipment you start with." |
| `OVT-Difficulty_Insane_Desc` | Difficulty = Insane | "Recommended for highly experienced servers. Occupying garrisons and patrols are at their largest." |
| `OVT-Difficulty_Numbers` | always | "Starting cash: $%1     Fast travel: $%2" |

**What the length budget cost, and why it is the right trade.** The description block has roughly **179 reference units** of vertical room (derivation below), which is about six wrapped lines. The Phase 0 table's sentences would have run to nine. So:

- The **supporting** description dropped "so changing one of these also moves the other" and kept "it cannot also be the occupier" — the underlying fact, which implies the coupling. It also dropped "not a faction you fight alongside" in favour of the shorter "not your own faction", which carries the same disproof of the fight-alongside myth.
- The **occupying** descriptions kept all three of O.1/O.2/O.3, merged into two sentences.
- The five **difficulty** descriptions are held to **one line each**, keeping the shipped recommendation verbatim plus exactly one cited clause. Cut for length but recorded in each `Comment`: Normal's "this is the preset a hosted game starts on", Extreme's QRF fast-travel clause, Insane's radio clause, and Easy's handgun clause (C3 forbids the handgun on the *shared welcome* pages, not here, but it lost to the garrison clause on room).
- The **numbers line** ships two numbers, exactly as 0.5 settled. No third.

**Nothing from the forbidden list appears anywhere**: no real-estate-scales-with-difficulty (F1, dead field), no flat respawn cost (F2, gated behind `money > 500`), no supporting-faction troops/equipment/prices/disguise/alliance (all five re-verified false), no claim that the supporting faction persists or replicates (F3). No em-dashes, no imperative, no goal, no implied order of play.

**Rule 0 re-verification done at phase front, not inherited.** Every citation used was reopened. Two line numbers had drifted since Phase 0 and are corrected in the `Comment` fields: `startingCash` is read at **`OVT_OverthrowGameMode.c:1080`** (Phase 0 said `:1048`; Phases 1 and 3 moved that file) with `AddPlayerMoney` at `:1081`, and Hard's patrol block is **`Difficulty_Hard.conf:12-14`**, not `:11-13`. Everything else was exact: `fastTravelCost` at `OVT_MapContext.c:384`/`:395`, QRF at `OVT_MapContext.c:78-84`, `placeableCostMultiplier`/`buildableCostMultiplier` at `OVT_OverthrowConfigComponent.c:252`/`:257`, `startingItems` at `OVT_SpawnLogic.c:1177`, `defenseGroupsBaseMax` at `OVT_BaseUpgradeDefensePosition.c:48`, `patrolGroupsMin/Max` at `OVT_OccupyingFactionManager.c:508` and halved at `OVT_InfantrySpawningDeploymentModule.c:224`, `IsDisguisedAsOccupying` at `OVT_PlayerWantedComponent.c:111` (7 consumers), `IsDisguisedAsSupporting` at `:130` (still **zero** callers), the not-disguised grouping at `:447-453` and `newLevel = 2` at `:709-715`, `Deployment_TownPatrol.conf:26` still `OCCUPYING_FACTION`.

**⚠️ One NEW finding, recorded in `OVT-Difficulty_Numbers`'s `Comment` and not fixed here.** `OVT_OverthrowGameMode.c:364` — a server running an `Overthrow_Config.json` with `overrideDifficulty` set **replaces `startingCash` after the campaign starts**. The setup screen therefore shows the preset's authored figure, not the override. This does not make the line false for the overwhelming majority of players (the override is opt-in, server-side, and applied after the menu is gone), so the number stays; it is written down so nobody reports it as a bug twice.

**The layout: what changed and the one deviation.**

| Widget | Change |
|---|---|
| `OccupyingFactionDescription` | **NEW** `TextWidgetClass {6B3D000000000201}`, immediately after `OccupyingFactionSpinner` |
| `SupportingFactionDescription` | **NEW** `TextWidgetClass {6B3D000000000202}`, immediately after `SupportingFactionSpinner` |
| `DifficultyNumbers` | **NEW** `TextWidgetClass {6B3D000000000203}`, immediately after `DifficultyDescription` |
| `DifficultyDescription` | hardcoded `Text "Difficulty Description"` → `#OVT-Difficulty_Normal_Desc`; gained `Wrap 1`; padding `10 10 10 10` → `10 4 10 4` |

All four now carry an identical property set: `Slot LayoutSlot "{6466E3EAD3800B8B}"`, `Padding 10 4 10 4`, `"Font Size" 18`, `"Min Font Size" 18`, `"Horizontal Alignment" Right`, `Wrap 1`.

- **Slot GUIDs are reused, not minted.** `{6466E3EAD3800B8B}` is `DifficultyDescription`'s existing slot id and all three new widgets share it. Slot GUIDs are shared class ids in this engine, not instance ids — the plan's "fresh GUID for every widget **and every slot**" is wrong for slots and was not followed. Widget GUIDs are unique: 17 in the file, 0 duplicates.
- **`Wrap 1` was added to all four, including the shipped `DifficultyDescription`.** `TextWidget` does not wrap unless the flag is set (`ArmaReforger/scripts/Core/generated/UI/TextWidget.c` class doc: *"Automatic wrapping is turned on by the WRAP_TEXT flag"*). Without it every description longer than the old 28-character placeholder renders as one clipped or overflowing line. This is a correctness fix, not a restyle; `Wrap 1` on a `TextWidgetClass` is the established Overthrow idiom (`JobsMenu.layout:96`, `CampMenu.layout:165`, `EconomyInfo.layout:104`).
- **⚠️ DEVIATION, deliberate: vertical padding is `4`, not the cloned `10`.** Four widgets at `Padding 10 10 10 10` spend **80** of the 179 available units on padding alone, leaving room for four and a half lines of text — less than the six the copy needs. At `10 4 10 4` padding costs 32 and the block fits with slack. All four remain identical to each other, which is what "must not look bolted-on" actually requires; the change to the shipped widget is 12 units.

**The geometry, derived rather than guessed** (Enfusion layouts are authored in a 1080-unit-tall reference space, so these numbers are resolution-independent on 16:9):

| Consumer | Units | Where it comes from |
|---|---|---|
| `Panel0` height | **680** | slot `Anchor 0 0 0 1`, `OffsetTop 200`, `OffsetBottom 200` → 1080 − 400 |
| `LogoFrame` | −310 | `OverthrowLogo` slot is 280 × 280 centred, plus `Padding 0 15 0 15` |
| 3 × spinner | −138 | `WLib_SpinBox` `SizeLayout HeightOverride 38` + `Padding 4 4 4 4` = 46 each |
| `Footer` | −53 | `WLib_NavigationButton` `SCR_InputButtonComponent m_iHeightInPixel 48` + `Padding 5 0 5 5` |
| **left for the description block + the `Fill` spacer** | **179** | |

Worst-case authored block: occupying 2 lines + supporting 2 + difficulty 1 + numbers 1 = **6 lines**. At a pessimistic 22-unit line height that is 6 × 22 + 4 × 8 = **164 units**; at 19 it is 146. Slack **15 to 33 units**.

**The Start button cannot move**, which is worth stating because it is the failure the plan named. `Fill` (`SizeMode Fill`) sits between `DifficultyNumbers` and `Footer` and absorbs every unit of slack, so a description growing from one line to two shrinks the gap rather than displacing the footer. The failure only bites if the block exceeds 179 units, at which point `Fill` is already zero.

**Residual risk: translations.** German and Russian typically run 110-130 % of English. At 130 % the supporting description would take a third line, putting the worst case at 186 units — about 7 over, so `Fill` collapses and the footer shifts a few units. Degraded, not broken, and no translation for these keys exists yet. **Three fallbacks, cheapest first, if the user sees a squeeze:** (1) `LogoFrame` `Padding 0 15 0 15` → `0 5 0 5`, +20 units and essentially invisible; (2) `"Font Size" 18` → `16` on all four descriptions, +~20 units and more characters per line; (3) `OverthrowLogo` slot 280 → 240, +40 units.

**⚠️ This is computed, not rendered. Nothing in this project parses a `.layout`.** compile-check compiles EnforceScript only. The fit is an arithmetic argument and the user's Workbench pass is the gate.

**The context: one refresh path, four hooks.** `RefreshDescriptions()` sets all four lines from `OVT_OverthrowConfigComponent`'s current selections and is called from the end of `OnShow`, `OnSpinOccupyingFaction`, `OnSpinSupportingFaction` and `OnSpinDifficulty`. The two `text.SetText(preset.description)` calls that used to sit one in each `RplSession.Mode()` branch are gone — that was exactly the shape that drifts when only one branch is edited.

**How the auto-swap case was verified — by tracing the invoker, not by assuming.** `SCR_SpinBoxComponent.SetCurrentItem` calls `SetCurrentItem_Internal(..., invokeOnChanged: true)`, which invokes `m_OnChanged` **synchronously** (`SCR_SpinBoxComponent.c:142-165`) and returns `false` without invoking when the index is unchanged. So the conflict swap inside `OnSpinOccupyingFaction` re-enters `OnSpinSupportingFaction` depth-first:

1. player moves occupying US ← USSR while supporting is US → key collision;
2. `sfSpin.SetCurrentItem(USSR)` fires `OnSpinSupportingFaction` **re-entrantly**; it sees occupying still holding the *old* key, so it walks `ofSpin`, finds it already on the new index (`SetCurrentItem` returns `false`, **no infinite recursion**), sets occupying := US, sets supporting := USSR and refreshes;
3. control returns to the outer handler, which sets occupying := US and refreshes **again** — this is the pass with both keys settled.

Both faction hooks therefore refresh **both** faction lines, and the last redraw is always the correct one. The double redraw is four `SetText` calls and is noted at the call site. The config, not the spinner, is read: `m_sOccupyingFaction`/`m_sSupportingFaction` are what the campaign actually starts with, and both are written before the refresh.

**D13 implemented as an explicit whitelist, not a composed key.** `ResolveOccupyingFactionDescription` / `ResolveSupportingFactionDescription` map `US`/`USSR` to their keys and return `""` for anything else. Composing `"#OVT-Faction_" + factionKey + "_Occupying"` was rejected and the reason is written at the function: an unresolved key does **not** degrade to nothing in Enfusion, it draws as its own raw text, so a modded faction would put a literal `#OVT-Faction_XYZ_Occupying` on the first screen a player ever sees. An empty description is the correct degradation.

**One small removal.** The bare `Print(OVT_Global.GetConfig().m_Difficulty.name)` in `OnSpinDifficulty` is gone. It was unprefixed debug output firing on every spinner move and dereferenced the preset without a guard; the preset's name is on the spinner and its description is now on screen.

**No focusable element was added, and it is provable from the file rather than asserted.** Every `components {}` block in `StartGameMenu.layout` belongs to a `ButtonWidgetClass`: `OccupyingFactionSpinner`, `SupportingFactionSpinner` and `DifficultySpinner` (`SCR_SpinBoxComponent`) plus `StartButton` (`SCR_InputButtonComponent {5D346C3DD81D95CD}` — the base-layout GUID, untouched). All four description widgets are bare `TextWidgetClass` with **no component block and no `: base.layout` inheritance**, so they attach no handler and cannot take focus. The shipped `DifficultyDescription` is the control case: it has sat in the same `VerticalLayout0` since the screen shipped and is not a tab stop. Focus count before **4**, after **4**; gamepad navigation still walks the three spinners and the Start button via `MenuUp`/`MenuDown` in `OverthrowStartContext` (priority 50, `Flags 4`), and interleaving non-focusable siblings cannot insert a stop. `Configs/System/chimeraInputCommon.conf` is byte-unchanged — this phase adds no action, and `OverthrowStartGame` stays on `KC_SPACE` / `gamepad0:y`, i.e. the screen's primary verb is not on `a` or `b`.

**GUIDs allocated this phase** — all within the reserved `6B3D…` block, all verified unique repo-wide:

| GUID | What |
|---|---|
| `{6B3D000000000201}` | `OccupyingFactionDescription` widget |
| `{6B3D000000000202}` | `SupportingFactionDescription` widget |
| `{6B3D000000000203}` | `DifficultyNumbers` widget |
| `{6B3D000000000106}`-`{…0109}` | the four faction description items |
| `{6B3D00000000010A}`-`{…010E}` | the five difficulty description items |
| `{6B3D00000000010F}` | `OVT-Difficulty_Numbers` |

No slot GUID was minted. The three new items in the `.st` were **appended at the end of the file** rather than inserted beside related items, to keep the diff away from the tutorial block a concurrent session is editing.

**⚠️ Blocked on the user: Workbench string-table export. Ten new ids** (the four `OVT-Faction_*`, the five `OVT-Difficulty_*_Desc`, and `OVT-Difficulty_Numbers`). **Until the export runs the campaign-setup screen draws raw `#OVT-` keys in all four description widgets**, which looks exactly like a bug and is not one. Batch this with Phase 2's five ids — one export covers both.

**Hygiene.** `git diff --stat Language/` lists `localization_Overthrow.st` and nothing else; no `.<lang>.conf` was opened. Layout: 17 widget GUIDs, 0 duplicates, 93/93 braces. `Difficulty_TestWorld.conf` byte-unchanged (it has no `description` field at all, so a key would have had nowhere to go). `Configs/System/`, `Configs/Tutorials/`, the game mode and `OVT_TutorialComponent.c` all untouched. The concurrent session's `UI/Layouts/HUD/TutorialPopup.layout`, `UI/Layouts/Menu/TutorialPopup.layout` and `UI/Textures/Tutorial/` were left alone — this phase touched `UI/Layouts/Menu/` but a different file.

### 2026-08-09 — Phase 3 complete (the legacy hint retired)

**One `.c` file for the deletion, two comment-only touch-ups elsewhere, one `.st` `Comment`. 6/6 tasks. Gates: compile-check exit 0 (5944 files, 5 s); All group **86 tests, exit 0** — the 0.1 baseline exactly.**

**3.1 confirmed §3.5 rather than contradicting it.** The table above is the record. All three claims held on re-reading; only the plan's line numbers had drifted (Phase 1 moved this file by roughly 100 lines) and one file path was wrong (`OVT_UIManagerComponent.c` lives under `Components/Player/`).

**What was deleted, found by content and not by line number:**

- the hint block in `OnPlayerSpawnedLocal` — the `if (m_bGameStarted && !m_aHintedPlayers.Contains(playerId))` guard, the `SCR_HintManagerComponent...ShowCustom("#OVT-IntroHint","#OVT-Overthrow",20)` call, the `Insert`, and the nine-line "LEGACY INTRO HINT" comment above them;
- the `m_aHintedPlayers` field declaration and its doc line (still at `:57`);
- its `new set<string>` allocation in `EOnInit` (at `:1274`, not `:1167`), beside `m_mSpawnContext`.

**The `PLAYER_SPAWNED` push that follows was not touched.** `m_bTutorialSpawnPending`, `m_bTutorialSpawnDelivered` and `TryPushSpawnedTutorialTrigger()` are byte-identical, and the Campaign spawn-trigger case is still green.

**The method's doc comment was rewritten rather than left describing a hint that no longer exists.** It now carries a one-line tombstone naming the retirement date, the feature and the replacement sequence.

**3.3.** The `m_bCampaignRunningRpl` rationale (now `:109-121`) lists two reasons instead of three; the removed one is noted parenthetically with its date so the count does not look like an accidental deletion. **The flag itself is untouched** — several systems key off it.

**3.4.** Both string items are still present. `OVT-IntroHint`'s `Comment` (was empty) now records the retirement date, the feature, the replacement, D11's reason for keeping the item, and the three false claims with citations (`OVT_SpawnLogic.c:1177` for the pistol, `OVT_VehicleManagerComponent.c:169-191` for the "garage", `starter-jobs-retirement` for the Jobs tutorials). `OVT-Overthrow` was left byte-identical and is cross-referenced from that comment. No `.<lang>.conf` was opened; `git diff --stat Language/` still lists `localization_Overthrow.st` alone.

**⚠️ Deviation from 3.5's literal expectation, and the reasoning.** The task expected the grep to return one line. As found, it returned **five**, and two of them were **not** anticipated by the plan: `OVT_TutorialSeenStore.c:13` and `OVT_TutorialSettingsAccessor.c:20`, both `tutorial-system` doc comments citing "the old `m_aHintedPlayers` set" as the bug that store exists to fix. Leaving them would have failed the hard acceptance ("no `m_aHintedPlayers` anywhere in `Scripts/`") and left a dangling symbol name that greps like a live reference. **Both were reworded, comment-only, to describe the same set without naming the deleted symbol**, keeping their historical point intact. The final grep returns **two** lines:

- `OVT_TutorialInfo.c:26` — the 20-second-duration design precedent, allowed and still accurate;
- `OVT_OverthrowGameMode.c:1486` — the new tombstone on `OnPlayerSpawnedLocal`'s doc comment.

The second is a deliberate keep: a reader arriving at the deletion site should find out where the welcome went without having to grep the `.st`. **`m_aHintedPlayers` now appears nowhere in `Scripts/` at all.**

**Not done, and correctly so.** No spawn mechanics were touched (home assignment, starting car, starting cash, teleport flow are byte-identical). Nothing under `Configs/Tutorials/`, `UI/` or the prefab was opened — Phase 2 owns those and has landed.

**Concurrent session still live in the tree (R8).** `git status` carries another session's `UI/Layouts/HUD/TutorialPopup.layout`, `UI/Layouts/Menu/TutorialPopup.layout` and `docs/.../tutorial-system/context.md`, plus an untracked `UI/Textures/Tutorial/` that was not there at Phase 1's close. None was touched.

**Play-test still owed for this phase specifically** (it is small, and it is the one thing no gate here can see): in single player, die once and confirm **no** corner hint appears on respawn, and Continue an existing campaign and confirm **no** corner hint appears on the first spawn of the session. The welcome modal replacing it cannot be observed until the Workbench string export runs.

### 2026-08-09 — Phase 2 complete (the two welcome entries)

**Config and strings only, no `.c`. 9/9 tasks. Gates: compile-check exit 0 (5944 files, 6 s); Fast group 50 tests exit 0 — the 0.1 baseline exactly. The Init entry guard now logs "Tutorial manager is live with 12 structurally valid entries", and `OVT_TEST_Init_FieldManual_DeltaMergesAndLinksResolve` is green with both welcomes' `#OVT-FieldManual_Welcome_Title` link.**

**The authored copy, in full**, so a reviewer can read the eight sentences without opening the `.st`. Every sentence is a row of the Sentence → Evidence Table above; nothing was invented in this phase.

| Key | Consumers | `Target_en_us` |
|---|---|---|
| `OVT-Tutorial_WelcomeIntro_Body1` (rewritten) | **both** | "Overthrow is a persistent sandbox campaign fought on an occupied island. There is no objective list and no required order, and the island reacts to what you do.`<br/><br/>`Towns, money, property, vehicles and captured ground are durable state, so a campaign carries on between sessions and across server restarts." |
| `OVT-Tutorial_WelcomeIntro_Body2` (rewritten → the house page) | `welcome-intro` | "You have been given a house in one of the island's towns, with a car parked outside it, and some starting cash.`<br/><br/>`That house is your home, which is where you respawn." |
| `OVT-Tutorial_WelcomeNohome_Body2` (new) | `welcome-nohome` | "No starting house was free, so you began in one of the island's towns rather than at a home of your own.`<br/><br/>`You still have your starting cash, and for now you respawn where you started. Property on the island can be bought, and a building you own can be set as your home." |
| `OVT-Tutorial_WelcomeIntro_Body3` (new) | **both** | "Most of what Overthrow adds sits in the Overthrow menu, opened with `<color rgba='226,168,79,255'><action name=\"OverthrowMainMenu\"/></color>`.`<br/><br/>`It holds the map, fast travel, the resistance, property, your recruits and your character sheet." |
| `OVT-Tutorial_WelcomeIntro_Body4` (new) | **both** | "Occasional tips like this one appear the first time something new happens, and each is shown once.`<br/><br/>`The Tips entry in the Overthrow menu reopens the most recent one." |

**What came out of the shipped text, and why.**

- **"You start with a house, a car and a little money"** is deleted from `_Body1` (task 2.4's hard requirement). It is false for `welcome-nohome`, which shares that key, and it belongs on page 2, where it now lives with its citations.
- **The old `_Body2` (the tips page) moved wholesale to `_Body4`**, and its 12-entry menu fact-check `Comment` moved with it. `_Body2` is now the house page.
- **Its "your jobs" clause did not survive the move.** `starter-jobs-retirement` (#5) follows this feature and §8 permits naming only menu entries with no pending change; page 3's list names the six that are stable.
- **The old page-2 sentence "Each is shown once and never again"** became "each is shown once". "Never again" overstates a per-machine seen store that a player can clear.

**Sentences from the Phase 0 table that were available and deliberately not used.** None was cut for want of evidence in this phase (that work was Phase 0's, and produced C1-C9). Two rows were shortened rather than dropped: page 1's sentence 3 lists five kinds of durable state rather than the manual's six (recruits dropped for length, and the manual keeps the longer form), and page 3 omits Jobs, Place, Build, Options, Save and Tips from the menu list for the reasons written into `_Body3`'s `Comment`. Nothing outside the table shipped.

**Tone gate, applied per sentence.** No imperative, no goal, no objective, no implied order. The one sentence that could have become an instruction is the houseless page's "Property on the island can be bought, and a building you own can be set as your home" — phrased as what is possible, never as what to do. Every page reads correctly for a player who ignores all of it for three hours. No em-dashes anywhere in the authored content.

**`m_sImage ""` on all eight page objects.** No art exists and the wide 4:1 header band the concurrent `tutorial-system` session just shipped is out of scope here.

**`//` comments parse fine in a `.conf`.** `welcomeNohome.conf` opens with a twelve-line header comment (D3's obligation: three of its four page keys are named after the *other* entry's id, deliberately). No `.conf` in the repo had one before, so this was a live question; the Init guard reporting **12** entries is the proof that the file still parses and loads.

**GUIDs allocated this phase** — all within the reserved `6B3D…` block, all verified unique repo-wide:

| GUID | What |
|---|---|
| `{6B3D000000000010}` | `welcomeNohome.conf` resource (in the `.meta` `Name` and in the prefab's inherit reference — 2 occurrences by construction, not a duplicate) |
| `{6B3D000000000011}`-`{…0014}` | its four page objects |
| `{6B3D000000000015}` | its `PLAYER_SPAWNED` trigger |
| `{6B3D000000000016}` | its element **appended** to `m_aTutorialEntries` (nothing reordered) |
| `{6B3D000000000021}`, `{…0022}` | `welcome-intro`'s two new page objects |
| `{6B3D000000000101}`-`{…0105}` | the five stringtable items, in page order |

**`welcome-intro` is otherwise untouched as D2 requires:** same `m_sId`, same filename, same resource GUID `{6B3A0000000000A0}`, same two original page GUIDs. Its trigger gained `m_sFilter "house"`, which makes it selective for the first time — intended, and the reason `welcome-nohome` must exist in the same build.

**D8 is now live: no shipped entry exercises the Learn-more-*hidden* branch** (`OVT_TutorialContext.c:499-500`). `proofWelcome.conf` was the last unlinked entry and it now carries `#OVT-FieldManual_Welcome_Title`. A future content pass wanting that coverage back must leave one entry's `m_sFieldManualTitleKey` empty.

**⚠️ Blocked on the user: Workbench string-table export.** Five ids are new or changed (`OVT-Tutorial_WelcomeIntro_Body1`, `_Body2`, `_Body3`, `_Body4`, `OVT-Tutorial_WelcomeNohome_Body2`; plus a `Comment`-only edit to `OVT-Tutorial_WelcomeIntro_Title`, which is translator metadata and needs no export of its own). **Until the export runs, neither welcome renders any text** — both draw raw `#OVT-` keys, which looks exactly like a bug and is not one. No Phase 2 play-test can start before it.

**Hygiene.** `git diff --stat Language/` lists `localization_Overthrow.st` and nothing else; no `.<lang>.conf` was opened. Duplicate-GUID grep over the `6B3D…` allocations is clean. Braces balanced in both `.conf` files (proved by the entries loading). `tutorial-content`'s ten configs are byte-unchanged (I1).

### 2026-08-09 — Phase 1 complete (the spawn-context transport)

**Two `.c` files, no config, no strings, no layout. 9/9 tasks. Gates: compile-check exit 0 (5944 files, 6 s); All group 86 tests exit 0 — the 0.1 baseline exactly, 0 failures, 0 errors, `autotest_failed.log` empty.**

**What was built.**

- `Scripts/Game/Components/Controller/OVT_TutorialComponent.c` — `SPAWN_CONTEXT_HOUSE` / `SPAWN_CONTEXT_NOHOUSE` constants; `m_sSpawnContextFilter` (defaulted to `"house"`, with D6's reasoning written at the field); `m_bSpawnContextReceived`; `void SetSpawnContext(int playerId, string filter)`; `[RplRpc(RplChannel.Reliable, RplRcver.Owner)] void RpcDo_SetSpawnContext(string filter)`; `static bool NotifyPlayerSpawnedLocal(bool acceptDefaultContext = false)`; the parameter threaded through `FireLocalEventOnLocalPlayer(..., bool acceptDefaultContext = false)`, which now substitutes the stored context as `ctx.m_sFilter` for `PLAYER_SPAWNED`. The class-level doc's "One RplRcver.Owner RPC" line was corrected — there are two now.
- `Scripts/Game/GameMode/OVT_OverthrowGameMode.c` — `protected ref map<string, string> m_mSpawnContext` (allocated beside `m_aInitializedPlayers` in `EOnInit`); `protected void SetPlayerSpawnContext(int playerId, string persistentId, string filter)`; `string GetPlayerSpawnContext(string persistentId)` (public, for Phase 5's T3); three call sites; and `PushSpawnedTutorialTrigger` now passing the final-attempt flag.

**`Rpc()` arity, proven by inspection (BUG-090).** `RpcDo_SetSpawnContext` declares **1** parameter (`string filter`), so the call is `Rpc(RpcDo_SetSpawnContext, filter)` — **2 arguments total**. Same shape as the shipped `Rpc(RpcDo_ShowTutorial, entryId)` two lines above it. The count is written into the comment beside the call. **No automated gate can check this**; M2/M4 are the only observation that can.

**One deviation from §3.3 / the task list, and it resolves a contradiction in them.** Task 1.6 reads as "on the final attempt call `NotifyPlayerSpawnedLocal(true)` **once** before giving up", which literally implemented means a *second* call in `PushSpawnedTutorialTrigger`. That would break acceptance 1.8, which requires the grep to show **one** call site. Implemented instead as one call carrying a computed flag:

```
bool isFinalAttempt = m_iTutorialSpawnPushAttempts >= TUTORIAL_SPAWN_PUSH_ATTEMPTS;
if (OVT_TutorialComponent.NotifyPlayerSpawnedLocal(isFinalAttempt)) { ...delivered... }
if (isFinalAttempt) { m_bTutorialSpawnRetrying = false; return; }
```

Behaviourally identical (the final attempt makes exactly one call, with `true`), one arity to keep right instead of two, and structurally incapable of double-firing.

**Three small additions §3.3 did not specify, all defensive:** `RpcDo_SetSpawnContext` also rejects `""` (the never-empty invariant is what the whole design rests on, and the receive path is where a bad value would actually land); `SetPlayerSpawnContext` lazily re-allocates `m_mSpawnContext` if it is somehow null (it runs inside the function that hands out homes, cars and cash — a null-map throw there is not worth a tutorial filter); and `SetPlayerSpawnContext` opens with `Replication.IsServer()`, mirroring every other method in that file.

**Runtime evidence, from the All-group log rather than from inspection.** In the test world the house branch ran (`[Overthrow] Player assigned home at <232.689, 1, 27.62>`) and the Campaign case reported *"PLAYER_SPAWNED was delivered … after **0** poll(s)"*, 145 ms later. That zero is meaningful: the test world is `RplMode.None`, so **no RPC can ever loop back**, and delivery on attempt 1 requires `m_bSpawnContextReceived == true`. **The local-direct-call branch (R3, the highest-impact risk in this phase) is therefore verified live, not just by inspection.** Had it been missing, the case would still have gone green — but only after the ~4.5 s final-attempt fallback, i.e. "many polls". Re-check that number, not just the pass, if this area is ever touched again.

**Backward compatibility verified at the config, not only at `Matches`.** `Configs/Tutorials/proofWelcome.conf:14-17` omits `m_sFilter` entirely, so the shipped trigger defaults to `""` = "no filter" and `welcome-intro` still matches now that the context carries `"house"`. **The tree between Phase 1 and Phase 2 is a working state**, not a broken one: populated context, unfiltered trigger, welcome still fires.

**Things a future reader will trip over.**

- **`SCR_PlayerController.GetLocalPlayerId()` returns `0` on a dedicated server** (`ArmaReforger/scripts/Game/Player/SCR_PlayerController.c:481-488` — no local `PlayerController` → `0`). Both the `>= FIRST_VALID_PLAYER_ID` guard and the equality test then send it down the `Rpc()` branch, which is correct. The guard is belt-and-braces; the equality alone would already be right.
- **The host is not assumed to be playerId 1.** Other code in this game mode hardcodes `playerId == 1` for the host; `SetSpawnContext` compares against the *actual* local id, so it is correct on a listen host whose own id is not 1.
- **`m_bSpawnContextReceived` is never reset, deliberately.** A respawn re-fires `PLAYER_SPAWNED`; making it wait again would delay a trigger whose entry is already seen. The context does not change within a session.
- **`GetPlayerSpawnContext` means "what finalization gave them", not "what they own now".** A houseless player who later buys a house keeps `"nohouse"` for the session. Harmless — the welcome is once-per-machine — but do not repurpose the map as a live ownership query.
- **A RETURNING player gets `""` from `GetPlayerSpawnContext`,** because their finalization runs neither branch (they already have a home). ⚠️ **Phase 5's T3 depends on the test world taking the NEW-player path.** It does today — the log shows `Preparing NEW player` and `Player assigned home at …` on every campaign start — but T3 would legitimately assert `""` in a world that shipped with pre-owned homes. Assert "one of the two known values", as §7 specifies, and not "house".
- **`FireLocalEventOnLocalPlayer`'s `filter` parameter is now IGNORED for `PLAYER_SPAWNED`** (substituted from the component). Every other event is unaffected. There is no caller that could supply it, and a second source for it would be a second thing to keep in step.

**Residual multiplayer risk, unchanged by this phase and still owed to the play-test.** The owner RPC inherits `Notify()`'s existing exposure: it is sent immediately after `AssignControllerOwnership`, and a client that has not yet instantiated the controller entity is the same window `OVT_TutorialManagerComponent.Deliver` already documents as a drop. If it drops, the retry's final attempt still delivers the welcome with the `"house"` default — degraded, not absent. **M2/M4 remain the only observations that can catch either that or a wrong `Rpc()` arity.**

**⚠️ Concurrent session in the tree (R8).** At Phase 1 start `git status` showed only this feature's three untracked docs. By the end it also carried another session's uncommitted work: `UI/Layouts/HUD/TutorialPopup.layout`, `UI/Layouts/Menu/TutorialPopup.layout` and `docs/features/new-player-experience/tutorial-system/context.md` (a post-close `tutorial-system` change re-shaping `TutorialImage` into a wide 4:1 header band). **No overlap with Phase 1's two `.c` files, and nothing was touched.** Phase 4 should note that the `.layout` half of `tutorial-system` moved under it; Phase 2 is unaffected.

### 2026-08-09 — Phase 0 complete (evidence pack and copy decisions)

- **Baselines measured**, not inherited: compile-check exit 0; Fast **50** tests exit 0; All **86** tests exit 0. All three of `CLAUDE.md`, `MEMORY.md` and `implementation.md` §4 were wrong, in three different directions.
- **§3.7 re-verified row by row: all 17 rows stand.** No strike was needed, so `implementation.md` is unamended by this phase. Two rows firmed up rather than fell (`IsDisguisedAsOccupying` has 7 consumers, not 5; the 12-entry menu count re-counted exactly).
- **`PLAYER_SPAWNED` is still unique to `proofWelcome.conf`** across all eleven entry configs.
- **`6B3D…` block still free** (4 matches, all inside this feature's own docs).
- **Numbers line settled at two numbers.** Four third-number candidates were examined and all four cut, three of them because the field has multiple read sites with different arithmetic.
- **New trap found and recorded**: `Difficulty_Normal.conf` and `Difficulty_Hard.conf` do not contain `startingCash` (and Normal does not contain `fastTravelCost`); those values come from `[Attribute(defvalue:)]` on `OVT_DifficultySettings.c:54,58`. §3.4's spreads are right, but only after resolving defaults.
- **Setup-description count derived as 9, not 8** (US and USSR are the only spinner factions; 2 roles x 2 factions + 5 difficulties).
- **Nine sentences cut for want of evidence** (C1-C9), including one the plan's own page-3 intent asked for ("the town around you") and the entire idea of a third difficulty number.

### 2026-08-09 — Feature started

- Read the epic context (`epic-overview.md`, the three built siblings' handoffs) and the 723-line plan.
- Scaffolded `tasks.md` (49 tasks across 7 phases) and this file; flipped `implementation.md` to In Progress.
- Phase routing confirmed from the plan's closing line: **1 → `network-specialist-advanced`**, **4 → `ui-developer-advanced`**, 0/2/6 → `help-docs-sync`, 3/5 → `component-developer`.
- Next: Phase 0, whose entire job is to make sure the eight welcome pages and eight setup descriptions are written from evidence rather than from a plan-time table.

---

### 2026-08-09 — Post-Phase-4 user feedback: setup descriptions shrunk 18 → 13

Seen on screen by the user during play-testing: the four campaign-setup description widgets were **too large**. Phase 4 had cloned `DifficultyDescription`'s shipped `Font Size 18`, which was correct as a clone but wrong as a design — 18 is a *body* size in this codebase, and these are secondary explanatory lines sitting under their spinners.

All four (`OccupyingFactionDescription`, `SupportingFactionDescription`, `DifficultyDescription`, `DifficultyNumbers`) moved to **`Font Size 13` / `Min Font Size 13`**. Both properties must move together — leaving `Min Font Size` at 18 would let the widget grow back.

**This also discharges Phase 4's residual fit risk.** That phase computed a worst case of ~164 units against 179 available — real but slim slack, with the note that German/Russian at 110-130 % of English would take the block to ~186 and overflow. Line height scales with font size, so 18 → 13 cuts the block to roughly **118 units**, restoring ~60 units of slack and taking the localized worst case comfortably inside the budget. **Phase 4's three documented fallbacks (logo padding, font drop, logo slot) are no longer needed** and should not be applied on top of this.

`DifficultyDescription` was a **shipped** widget, so this is the second deliberate change to it (Phase 4 added `Wrap 1` and trimmed vertical padding to `10 4 10 4`). Both were necessary and both are recorded so a reviewer does not read them as accidental scope creep: without `Wrap 1` a `TextWidget` does not wrap at all, and at padding 10 four widgets spend 80 of 179 units on padding alone.

Braces verified balanced (93/93). `.layout` files are not EnforceScript, so `compile-check.sh` cannot see them — **the gate for this change is the user's eye, and it was user-reported in the first place.**

---

### 2026-08-09 — PLAY-TEST PASSED, and the feature ships with art it was not planned to have

User verdict, in single player after the string export: *"everything looks fine, content is good, text all fits."* **Feature closed.**

**What that observation actually discharges.** F1 (the welcome fires once on first spawn) and F2 (four pages, navigable, dismissable) are **observed**, not inferred — and independently corroborated by the seen store, which came back carrying `welcome-intro` again after the run. That is the whole per-machine seen mechanism working end to end: entry fires, player dismisses, id is written, and it will not fire again on any campaign or server. Q3 (sandbox tone) and F8's fit half are signed off by eye. **The legacy hint's replacement is therefore proven in the only way it could be** — no automated gate in this project can see a popup render.

**Still not observed, and honestly carried forward:** F3 (the houseless page 2 — needs the starting houses exhausted, play-test P7), F5/F6 (**two clients on a dedicated server, and JIP** — the epic's outstanding F7 question, which this feature adds an owner RPC to and which has *never* been observed passing for the tutorial framework), and F7's death/Continue regression check. **None is known-broken; none was observed.**

#### The welcome ships with page art, which was never in the plan

The user authored four 2048×512 (4:1) headers during the build and wired them himself. This is the **first time any tutorial entry has ever set `m_sImage`** — all eleven prior entries carry `m_sImage ""`, so `OVT_TutorialContext.c:433-439`'s `Is Visible` toggle and `LoadImageTexture` call had never executed in a shipped configuration. **It works.** The widget geometry was re-shaped from square to a wide 4:1 header band in the same session (recorded in `tutorial-system/context.md`, which owns those layouts).

⚠️ **The catch worth remembering: an image carries a claim exactly as a sentence does, and Rule 0 does not exempt it.** The first wiring pointed **both** entries' page 2 at `welcome-intro-2-ui` — a car in a garage — so `welcome-nohome` illustrated "no house was free, you own nothing" with a picture of a car in a garage. That is precisely the contradiction the entire two-entry design exists to prevent, reintroduced through the artwork after the prose had been fact-checked to death. Caught by reading the images against the shipped copy rather than trusting the filenames. The user authored `welcome-intro-nohome-2-ui.edds` (`{6C355414394F5F27}`, verified against its `.meta`) and rewired page 2.

**Two consequences for whoever adds art next:**
1. **Fact-check images against the copy they sit above**, the same way the copy is fact-checked against source. A shared *string key* is deliberate (D3); a shared *image* across two entries that differ on that exact page is a bug.
2. `welcome-intro-3-ui` shows the Overthrow menu with **Jobs** visible. True today, and deliberately *not* named in the page-3 text because `starter-jobs-retirement` (#5) may remove that entry. **If #5 removes Jobs, that screenshot goes stale** — it is the only place in this feature that will need re-shooting, and nothing will flag it.

#### Font size: user-corrected, and it retired a risk

The four setup descriptions went 18 → 13 on user feedback (its own session note above). Beyond looking right, it **discharges Phase 4's residual localization overflow risk** — the block drops from ~164 to ~118 of 179 available units, so German/Russian at 110-130 % no longer threatens the Start button, and Phase 4's three documented fallbacks are now unnecessary rather than merely unused.

#### Final measured state

`tools/compile-check.sh` **exit 0** · Fast **51 exit 0** · All **88 exit 0** — re-run after the user's image wiring and the font change, not inherited from the phase runs.

---

*Update this file at the end of each work session. Run `/update-feature new-player-experience/first-spawn` before compacting conversations.*
