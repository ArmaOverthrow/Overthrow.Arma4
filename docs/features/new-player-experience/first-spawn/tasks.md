# First Spawn - Task Checklist

**Last Updated:** 2026-08-09
**Progress:** 49/49 tasks complete (100%)

**Epic:** `new-player-experience` (feature #4 of 5) · **Plan:** `implementation.md` · **Scope truth:** `requirements.md`

> Task ids match the `<phase>.<n>` ids in `implementation.md` §4 — do not renumber them.
> **Agent tiers are set by the plan (§4 and its closing line).** Two phases are **ADVANCED**: Phase **1** → `network-specialist-advanced`, Phase **4** → `ui-developer-advanced`. Phases **0**, **2** and **6** route to `help-docs-sync`; Phases **3** and **5** to `component-developer`. `/proceed` and `/autorun-feature` must respect that routing rather than substituting a default agent.
> Every phase ends with `tools/compile-check.sh` exit 0. Phases touching a `.c` file (**1**, **3**, **4**, **5**) also end with the **All** group green (`tools/run-tests.sh "{6A6E2A002F53A581}"`); config/string-only phases (**2**) end with **Fast** (`"{6A6E29FF47ECB840}"`). **Baselines are measured in task 0.1, not inherited** — the counts in `CLAUDE.md`, `MEMORY.md` and the sibling plans disagree with each other and with the tree. **Measured 2026-08-09: Fast = 50 tests exit 0, All = 86 tests exit 0, compile-check exit 0.** **Measured AT CLOSE (Phase 5): Fast = 51, All = 88, compile-check exit 0.** The planned "Fast 52 / All 89" double-counted T2, which is a *branch* on the existing Init entry guard and therefore adds assertions rather than cases — the honest close is baseline +1 Fast (T1) and +2 All (T1, T3). Ignore every other figure in the repo.
> **Never edit `Language/localization_Overthrow.<lang>.conf`** — the `.st` master only. **Never touch `tutorial-content`'s ten `Configs/Tutorials/*.conf`** or anything under `Configs/FieldManual/` (Phase 6 excepted, and only for the Welcome page) or `Configs/System/`.
> **Reserved GUID block: `6B3D…`** (verified 0 matches repo-wide 2026-08-09; **re-verified in 0.1**). Allocations are tabled in `implementation.md` §3.6.
> **Rule 0 binds this feature twice over** — eight pages of welcome prose *and* eight campaign-setup descriptions. A sentence with no `file:line` is **cut, not softened**. §3.7 is the evidence pack and is **re-verified each phase, never inherited** (`tutorial-content` shipped a false trap row).

---

## Phase 0: Evidence pack and copy decisions (6/6 complete) — `help-docs-sync`

*No code. Rule 0 front-loading — §3.7 was assembled at planning time and must be re-verified, not trusted.*

- [x] **0.1 — Re-check preconditions and record the baseline**
  - Description: `git status` (expect only this feature's own docs plus whatever concurrent sessions left); re-run `grep -rEoh "\{6B3D[0-9A-F]{12}\}"` and confirm 0 matches outside `implementation.md`'s allocation table; run both test groups and **record the measured Fast/All case counts** as this feature's baseline in `context.md`.
  - File(s): (read-only) — records into `context.md`
  - Estimate: 🟢 20 min

- [x] **0.2 — Re-verify every row of §3.7 against the tree**
  - Description: Both halves — the nine "verified true" rows and the eight "verified false or unusable" rows. Any row that no longer holds is **struck in `implementation.md` in place**, with the date and the finding, the way `tutorial-content` struck its two false rows. A trap table is not evidence.
  - File(s): `docs/features/new-player-experience/first-spawn/implementation.md` (§3.7, if a row falls)
  - Estimate: 🟡 1.5 h

- [x] **0.3 — Confirm no third `PLAYER_SPAWNED` entry exists**
  - Description: `grep -l PLAYER_SPAWNED Configs/Tutorials/*.conf` must name only `proofWelcome.conf`. `tutorial-content` states its ten deliberately do not use the event — re-check rather than inherit (§3.3 backward-compat note).
  - File(s): (read-only) `Configs/Tutorials/*.conf`
  - Estimate: 🟢 10 min

- [x] **0.4 — Map the Field Manual Welcome prose that pages 1-2 compress**
  - Description: Read `#OVT-FieldManual_Welcome_Text`, `_Text2`, `_Head`, `_Head2` in the `.st`. Record which sentence of which page inherits which manual string, **and that string's own cited source** (the `tutorial-content` D12 pattern). Compression may say less than the manual; never more.
  - File(s): (read-only) `Language/localization_Overthrow.st`
  - Estimate: 🟢 30 min

- [x] **0.5 — Settle the difficulty numbers line**
  - Description: Confirm `startingCash` and `fastTravelCost` are still read where §3.4 says (`OVT_OverthrowGameMode.c:1048`, `OVT_MapContext.c:384,395`); decide whether a third number earns its place; write the chosen `#OVT-Difficulty_Numbers` format string with its explicit `%1..%n` → field mapping. `realEstateCostMultiplier` (dead) and `respawnCost` (gated) are disqualified — confirm both are still disqualified.
  - File(s): (read-only) sources per §3.4 — records the format string into `context.md`
  - Estimate: 🟡 1 h

- [x] **0.6 — Draft the sentence → evidence table**
  - Description: All eight welcome sentences and all eight setup descriptions as a table of *sentence → `file:line`* (or manual-string id plus its own source). **A sentence with no citation is cut, not softened.** This table becomes the `Comment` fields in Phases 2 and 4 — it is the artefact those phases read instead of inventing.
  - File(s): `docs/features/new-player-experience/first-spawn/context.md`
  - Estimate: 🔴 2 h

**Acceptance:** §3.7 re-verified with any correction struck in place and dated; no second `PLAYER_SPAWNED` entry; the sentence→evidence table exists and every row has a citation; the numbers line's format and parameter mapping decided; baseline counts recorded.

---

## Phase 1: The spawn-context transport (9/9 complete) — 🔴 **ADVANCED** `network-specialist-advanced`

*The phase that justifies advanced: an owner RPC on a per-player delivery path whose MP correctness has never been observed passing (epic F7), a changed return-value contract the game mode's retry loop depends on, and an edit to `FinalizePlayerPreparation` — the function that hands out homes, cars and cash to every player on the server. `Rpc()` arity is invisible to compile-check (BUG-090).*

- [x] **1.1 — Spawn-context state on `OVT_TutorialComponent`**
  - Description: Add `SPAWN_CONTEXT_HOUSE`/`SPAWN_CONTEXT_NOHOUSE` constants, `m_sSpawnContextFilter` **initialised to `"house"`**, and `m_bSpawnContextReceived`. **Document at the field why the default is `"house"` and not `""`** (D6): `Matches:116` treats `""` on the *trigger* as "no filter", but `""` on the *context* matches no filtered trigger at all, so an empty default suppresses **both** welcomes instead of degrading to one. It looks like a value that wants tidying.
  - File(s): `Scripts/Game/Components/Controller/OVT_TutorialComponent.c`
  - Estimate: 🟢 30 min

- [x] **1.2 — `SetSpawnContext` + the owner RPC**
  - Description: `SetSpawnContext(int playerId, string filter)` and `[RplRpc(RplChannel.Reliable, RplRcver.Owner)] RpcDo_SetSpawnContext(string filter)`. Mirror `Notify():124-139`'s local-direct-call branch (the engine never loops an RPC back to the sender, so SP/listen hosts get nothing without it) — **except** the ownership test, which must be a **player-id comparison** against `SCR_PlayerController.GetLocalPlayerId()` guarded by `>= FIRST_VALID_PLAYER_ID`, **not** `IsOwnedByLocalPlayer()`: that helper dereferences the controlled entity, which does not exist yet on the load-a-save path (`OVT_SpawnLogic.c:150-160`). Reject an empty filter.
  - File(s): `Scripts/Game/Components/Controller/OVT_TutorialComponent.c`
  - Estimate: 🟡 1.5 h

- [x] **1.3 — Thread the context into `PLAYER_SPAWNED`**
  - Description: `NotifyPlayerSpawnedLocal()` → `NotifyPlayerSpawnedLocal(bool acceptDefaultContext = false)`, threaded through `FireLocalEventOnLocalPlayer` (`:206-209`, `:223-238`). For `PLAYER_SPAWNED` set `ctx.m_sFilter = m_sSpawnContextFilter` instead of `""`; return `false` when the component exists but `!m_bSpawnContextReceived && !acceptDefaultContext`. Update the doc comment at `:199-209` — its "was there anyone to tell?" contract has changed.
  - File(s): `Scripts/Game/Components/Controller/OVT_TutorialComponent.c`
  - Estimate: 🟡 1 h

- [x] **1.4 — Server-side context map and accessors**
  - Description: `m_mSpawnContext` (allocated beside `m_aInitializedPlayers` at `:1167`), `SetPlayerSpawnContext(playerId, persistentId, filter)` and `GetPlayerSpawnContext(persistentId)`. The setter resolves controller → `OVT_TutorialComponent` with the same three-step resolve `OVT_TutorialManagerComponent.Deliver:509-525` uses. **Null at any step is a silent drop, never an error.**
  - File(s): `Scripts/Game/GameMode/OVT_OverthrowGameMode.c`
  - Estimate: 🟡 1 h

- [x] **1.5 — Call it from both Finalize branches and the early return**
  - Description: `FinalizePlayerPreparation:1011-1023` — the `!house` branch (fallback spawn) sets `"nohouse"`, the `else` branch (home + car + cash) sets `"house"`. The `already finalized, skipping` early return at `:989` **re-sends the cached value** so a reconnecting player is not left on the default. The outer `if (home[0] == 0)` being false for a returning player is correct and intentional — the client default of `"house"` stands, which is right for anyone who has a home.
  - File(s): `Scripts/Game/GameMode/OVT_OverthrowGameMode.c`
  - Estimate: 🟡 1 h

- [x] **1.6 — Final-attempt degradation in the push loop**
  - Description: `PushSpawnedTutorialTrigger:1461-1479` — on the **final** attempt (`m_iTutorialSpawnPushAttempts >= TUTORIAL_SPAWN_PUSH_ATTEMPTS`) call `NotifyPlayerSpawnedLocal(true)` once before giving up. **Degrade, never disappear:** a lost context costs one page of accuracy, never the whole welcome (D7).
  - File(s): `Scripts/Game/GameMode/OVT_OverthrowGameMode.c`
  - Estimate: 🟢 30 min

- [x] **1.7 — Prove the `Rpc()` arity by inspection and record it**
  - Description: `RpcDo_SetSpawnContext` takes 1 parameter, so the call site is `Rpc(RpcDo_SetSpawnContext, filter)` — **2 arguments total**. Write the count into the code comment beside the call. `Rpc()` is an untyped variadic proto: a wrong argument count compiles clean, passes every test and dies silently at the wire (BUG-090).
  - File(s): `Scripts/Game/Components/Controller/OVT_TutorialComponent.c`
  - Estimate: 🟢 15 min

- [x] **1.8 — Grep-verify the changed signature has no other caller**
  - Description: `grep -rn "NotifyPlayerSpawnedLocal" Scripts/` shows one definition and one call site, consistent in arity. A missed caller is a silent compile-clean default-argument change.
  - File(s): (read-only) `Scripts/`
  - Estimate: 🟢 10 min

- [x] **1.9 — Gates: compile-check + All tier**
  - Description: `tools/compile-check.sh` (expect 0) and `tools/run-tests.sh "{6A6E2A002F53A581}"` (expect 0 at the 0.1 baseline). `OVT_TEST_Campaign_Tutorial_SpawnTriggerSurvivesCampaignStart` must still pass — that is what proves the retry-contract change did not break delivery.
  - File(s): —
  - Estimate: 🟢 20 min

**Acceptance:** compile-check 0; All green at the recorded baseline; `grep -rn "NotifyPlayerSpawnedLocal" Scripts/` shows one definition and one call site consistent in arity; `grep -rn "SetPlayerSpawnContext" Scripts/` shows exactly three call sites (two branches plus the early return); the existing Campaign spawn-trigger case still passes.

**✅ Met 2026-08-09.** compile-check exit 0 (5944 files, 6 s); All group **86 tests, exit 0** — the 0.1 baseline exactly, 0 failures, 0 errors, empty `autotest_failed.log`. `NotifyPlayerSpawnedLocal`: one definition (`OVT_TutorialComponent.c:300`), one call site (`OVT_OverthrowGameMode.c:1588`), both 1-arity. `SetPlayerSpawnContext`: definition at `:1129` plus **exactly three** call sites (`:1012` early return, `:1047` nohouse, `:1057` house). `OVT_TEST_Campaign_Tutorial_SpawnTriggerSurvivesCampaignStart` passed, **delivering after 0 polls** — which is the runtime proof that the SP local-direct-call branch works (a missing direct call would still pass, but only via the ~4.5 s final-attempt fallback). Workbench spawn-mechanics play-test (home, car, cash unchanged) is still owed to the user.

---

## Phase 2: The two welcome entries (9/9 complete) — `help-docs-sync`

*Config and strings only. No `.c`. Read Phase 0's evidence table before writing a word.*

- [x] **2.1 — Read the evidence table first**
  - Description: Phase 0's sentence→evidence table is the input to this phase. **Nothing new is invented here.** Any sentence that wants writing and has no row gets a citation first or does not ship.
  - File(s): (read-only) `context.md`
  - Estimate: 🟢 15 min

- [x] **2.2 — Expand `proofWelcome.conf` to four pages**
  - Description: 2 pages → 4 (new page objects `{6B3D000000000021}`/`{6B3D000000000022}`), add `m_sFilter "house"` to the existing `PLAYER_SPAWNED` trigger, add `m_sFieldManualTitleKey "#OVT-FieldManual_Welcome_Title"`. **Do not change `m_sId`, the filename, or the file's resource GUID** (D2 — the id is already in players' seen stores). Keep every member written out explicitly; it is a template as much as data.
  - File(s): `Configs/Tutorials/proofWelcome.conf`
  - Estimate: 🟡 1 h

- [x] **2.3 — Create `welcomeNohome.conf` + `.meta`**
  - Description: `m_sId "welcome-nohome"`, MODAL, priority 100, the same link key, `m_sFilter "nohouse"`, pages `[WelcomeIntro_Body1, WelcomeNohome_Body2, WelcomeIntro_Body3, WelcomeIntro_Body4]`. GUIDs `{6B3D000000000010}`-`{…0015}` per §3.6. **Header comment must state that three of its four page keys are deliberately shared with `welcome-intro`** (D3) — the key name not matching the entry id is the price paid for making drift impossible.
  - File(s): `Configs/Tutorials/welcomeNohome.conf` + `.meta`
  - Estimate: 🟡 1 h

- [x] **2.4 — Author the strings: 3 new, 2 rewritten**
  - Description: `.st` master **only**. New: `WelcomeIntro_Body3`, `WelcomeIntro_Body4`, `WelcomeNohome_Body2`. Rewritten: `WelcomeIntro_Body1` (**the "you start with a house, a car and a little money" sentence must come out** — it is false for the houseless entry and belongs on page 2 anyway) and `WelcomeIntro_Body2` (becomes the house page). Every item: fresh `{6B3D0000000001xx}` GUID and a `Comment` carrying (a) where it appears and **which entries consume it** (shared keys name both), (b) the tone rule, (c) the Rule 0 evidence. **Move `_Body2`'s existing menu-fact-check note to `_Body4`** with the text it belongs to.
  - File(s): `Language/localization_Overthrow.st`
  - Estimate: 🔴 2 h

- [x] **2.5 — Append the new entry to the game-mode prefab**
  - Description: One `OVT_TutorialEntryConfig` element for `welcomeNohome.conf` appended to `m_aTutorialEntries` (currently `:212-219+`), element GUID `{6B3D000000000016}`. **Append, never reorder** (R8 — this is a file concurrent sessions touch).
  - File(s): `Prefabs/GameMode/OVT_OverthrowGameMode.et`
  - Estimate: 🟢 20 min

- [x] **2.6 — Tone and content constraints**
  - Description: No imperative, no goal, no objective, no order of play, no "now go and…". **No second home entry** — page 2 says "this is yours and you respawn here" and stops, leaving ownership mechanics to `tutorial-content`'s `home-first-open` (I1). **No mention of the starting pistol** (Extreme and Insane ship the radio alone, and three of four pages are shared). `welcome-nohome`'s page 2 must read correctly for someone who has cash and a home position but **owns nothing**.
  - File(s): `Configs/Tutorials/*.conf`, `Language/localization_Overthrow.st`
  - Estimate: 🟡 1 h

- [x] **2.7 — Hygiene sweep**
  - Description: duplicate-GUID grep over the new `6B3D…` allocations prints nothing; `git diff --stat Language/` shows **only** `localization_Overthrow.st`; no em-dashes; balanced braces in every new/edited `.conf`.
  - File(s): —
  - Estimate: 🟢 20 min

- [x] **2.8 — Gates: compile-check + Fast tier**
  - Description: `tools/compile-check.sh` (expect 0) and the Fast group (expect 0 at baseline). The Init entry guard should now report **12** structurally valid entries (10 content + 2 welcomes).
  - File(s): —
  - Estimate: 🟢 20 min

- [x] **2.9 — Report the export list**
  - Description: Report the complete list of new/changed string ids for the user's Workbench re-export, and state plainly that **neither welcome renders its text until that happens** — until then both draw raw `#OVT-` keys, which looks exactly like a bug (R6). Batch with Phase 4's list if the two land in one session.
  - File(s): (report only)
  - Estimate: 🟢 10 min

**Acceptance:** two entries, two distinct ids, both MODAL/priority 100/4 pages, filters `house` and `nohouse`, both linking a key in the frozen table; every sentence has evidence in a `Comment`; no §3.7 trap present; Init guard reports 12; export list handed over.

> ### ⛔ Workbench string export — user-owned, and it blocks the play-test
> New `.st` items are invisible in-game until the user re-exports the string table in Workbench. **No play-test of Phase 2 or Phase 4 can start before that step.** The same applies to Phase 4's difficulty and faction keys, so batch the request if the two phases land together.

---

## Phase 3: Retire the legacy hint (6/6 complete) — `component-developer`

*Separate from Phase 1 so the diff is unambiguous, and **after** Phase 2 so there is never a build with neither the hint nor the welcome.*

- [x] **3.1 — Verify §3.5's observed-change table before deleting anything**
  - Description: Confirm **by reading, not assuming**: `OnPlayerSpawnedLocal` has exactly one caller (`OVT_UIManagerComponent.AfterControlledByPlayer:138`) and fires on **every** possession on the controlling machine; `m_bGameStarted` is authority-only (`:92-94`, written only at `:255`) and therefore false on every dedicated client; and the SP respawn / Continue paths reach it with `m_bGameStarted` true. The code comment's "dead since the 1.6 spawn rework" claim is **two-thirds true** and is not inherited. Record what removal is *observed* to change in `context.md`.
  - File(s): (read-only) `Scripts/Game/GameMode/OVT_OverthrowGameMode.c`, `OVT_UIManagerComponent.c`
  - Estimate: 🟡 1 h

- [x] **3.2 — Delete the hint block and its dedup set**
  - Description: `OVT_OverthrowGameMode.c:1382-1391` (the hint block), the `m_aHintedPlayers` field (`:57`) and its allocation (`:1167`). **Leave the `PLAYER_SPAWNED` push that follows it completely alone** — it is the delivery path this whole feature depends on.
  - File(s): `Scripts/Game/GameMode/OVT_OverthrowGameMode.c`
  - Estimate: 🟢 30 min

- [x] **3.3 — Correct the stale rationale comment**
  - Description: The comment at `:96-107` cites "the legacy `#OVT-IntroHint` in `OnPlayerSpawnedLocal`, which the first-spawn feature owns" as one of three reasons `m_bCampaignRunningRpl` exists as a separate flag. That reason is now gone; the other two remain. **Do not change the flag itself.**
  - File(s): `Scripts/Game/GameMode/OVT_OverthrowGameMode.c`
  - Estimate: 🟢 15 min

- [x] **3.4 — Retire the two string items in place, do not delete them**
  - Description: **Keep** both `OVT-IntroHint` (`.st:3339`) and `OVT-Overthrow` (`.st:6359`) — they carry six languages of translation, `.st` deletions churn the exports the user regenerates by hand, and an unreferenced item costs nothing at runtime (D11). Rewrite `OVT-IntroHint`'s `Comment` to record retirement (date, feature, replacement) **and the three claims in its text that were or became false**: the handgun (false on Extreme/Insane), the "garage" (the car parks in a parking spot or at a kerb), and the Jobs-section tutorials (`starter-jobs-retirement` removes them). That record is the actual protection against a revival. **Do not touch any `.<lang>.conf`.**
  - File(s): `Language/localization_Overthrow.st`
  - Estimate: 🟢 30 min

- [x] **3.5 — Grep-verify the retirement is complete**
  - Description: `grep -rn "IntroHint\|m_aHintedPlayers" Scripts/ Configs/ UI/` returns **only** the `OVT_TutorialInfo.c:26` doc comment, which cites the hint's 20 s duration as design precedent and is still accurate.
  - File(s): (read-only)
  - Estimate: 🟢 10 min

- [x] **3.6 — Gates: compile-check + All tier**
  - Description: `tools/compile-check.sh` (expect 0); All group green at baseline.
  - File(s): —
  - Estimate: 🟢 20 min

**Acceptance:** compile-check 0; All green; no `m_aHintedPlayers` anywhere in `Scripts/`; both string items still present with the annotated comment; `git diff --stat Language/` shows only the `.st`; the observed-change table recorded in `context.md`.

**✅ Met 2026-08-09.** compile-check exit 0 (5944 files, 5 s); All group **86 tests, exit 0** — the 0.1 baseline exactly. `grep -rn "m_aHintedPlayers" Scripts/` returns **nothing**. Both `OVT-IntroHint` and `OVT-Overthrow` are still in the `.st`, with the retirement record on the former's `Comment`. `git diff --stat Language/` lists `localization_Overthrow.st` and nothing else. §3.5's table was re-verified by reading and **confirmed**, and is recorded in `context.md`. One deviation from 3.5's literal expectation, reported there: the grep returns **two** `IntroHint` lines, not one — `OVT_TutorialInfo.c:26` (allowed) plus a deliberate one-line tombstone on `OnPlayerSpawnedLocal`'s doc comment.

---

## Phase 4: Start-menu descriptions (9/9 complete) — 🔴 **ADVANCED** `ui-developer-advanced`

*Advanced because it edits a `.layout`: GUID discipline, slot cloning and text fit at 1080p are the failure modes, and the campaign-setup screen is the first thing every player sees — a description wrapping into the Start button reads as broken. It also changes the meaning of a config field across five files.*

- [x] **4.1 — Three new `TextWidget`s in `StartGameMenu.layout`**
  - Description: `OccupyingFactionDescription`, `SupportingFactionDescription` and `DifficultyNumbers`, each **cloning `DifficultyDescription`'s slot shape, padding, font size and alignment** (`:109-118`) and placed immediately after their spinner per §3.4's tree. Fresh `{6B3D0000000002xx}` GUIDs for every widget **and every slot**. Replace `DifficultyDescription`'s hardcoded `Text "Difficulty Description"` with a `#OVT-` key (see 4.6 on export ordering). *If it sits differently from the existing description, it is wrong.*
  - File(s): `UI/Layouts/Menu/StartGameMenu.layout`
  - Estimate: 🟡 1.5 h

- [x] **4.2 — One `RefreshDescriptions()`, called from four hooks**
  - Description: `protected void RefreshDescriptions()` sets all four texts from the current config selections; called from the end of `OnShow` (`:57-140`), `OnSpinOccupyingFaction` (`:142-168`), `OnSpinSupportingFaction` (`:170-196`) and `OnSpinDifficulty` (`:198-208`). This collapses the duplicated `text.SetText(preset.description)` at `:129` and `:134` into one place **so the two `RplSession.Mode()` branches cannot drift**. Both faction hooks must refresh **both** faction descriptions — the conflict swap at `:150-164` moves the *other* spinner, so refreshing only the one that moved leaves a stale line on screen.
  - File(s): `Scripts/Game/UI/Context/OVT_StartGameContext.c`
  - Estimate: 🟡 1.5 h

- [x] **4.3 — Faction descriptions keyed by faction key, not spinner index**
  - Description: The list is built by filtering `IsPlayable()` and `"CIV"` (`:94-108`) and a mod can change its contents and order, so index-keyed strings would silently mislabel factions (D13). Resolve a per-key `#OVT-` string (`US`, `USSR`, …); an unknown key shows **an empty description rather than a wrong one**. Two keys per faction — occupying role and supporting role mean different things.
  - File(s): `Scripts/Game/UI/Context/OVT_StartGameContext.c`, `Language/localization_Overthrow.st`
  - Estimate: 🟡 1 h

- [x] **4.4 — The numbers line via `SetTextFormat`, never concatenation**
  - Description: `TextWidget.SetTextFormat("#OVT-Difficulty_Numbers", …)` with the parameters Phase 0 decided, read from the selected `OVT_DifficultySettings`. Precedent: `OVT_TutorialContext.c:459` with `#OVT-Tutorial_PageIndicator`. **No English is ever built in script.**
  - File(s): `Scripts/Game/UI/Context/OVT_StartGameContext.c`
  - Estimate: 🟢 30 min

- [x] **4.5 — Difficulty configs: description → `#OVT-` key**
  - Description: `Configs/Difficulty/Difficulty_{Easy,Normal,Hard,Extreme,Insane}.conf` — replace the hardcoded English `description` with `#OVT-Difficulty_{Easy,…}_Desc` (D9: `SetText` resolves stringtable keys, and `description` is read in exactly one file, so no read-site change is needed). **Do not touch `Difficulty_TestWorld.conf`** — it is not in `m_aDifficultyPresets` (`OVT_OverthrowGameMode.et:65-79`) and a key-less description renders literally, which is correct for a dev preset.
  - File(s): 5 × `Configs/Difficulty/Difficulty_*.conf`
  - Estimate: 🟢 30 min

- [x] **4.6 — Author the setup strings**
  - Description: `.st` master only — five difficulty descriptions, the `#OVT-Difficulty_Numbers` format string, and the faction descriptions (two per faction). Same `Comment` discipline as Phase 2 including Rule 0 evidence. ⚠️ **A layout referencing an unexported key renders the raw key** — so either land the layout's `#OVT-` key together with the export request, or leave the literal placeholder and change it in the same session as the export (R6).
  - File(s): `Language/localization_Overthrow.st`
  - Estimate: 🔴 2 h

- [x] **4.7 — Content constraints from §3.7**
  - Description: The supporting-faction description may claim **only** the perception effect (its uniform is treated as hostile exactly as the resistance's is, so wearing it and being seen means an immediate wanted level) and the occupier coupling (the two cannot be the same, so moving one spinner moves the other). It may **not** say it sends troops, supplies equipment, affects prices, is a disguise, or that you fight alongside it — **all five are verified false**. The difficulty description may not quote `realEstateCostMultiplier` (dead field) or `respawnCost` (gated behind a hardcoded `money > 500`). No sentence may assert the supporting faction persists or replicates (F3).
  - File(s): `Language/localization_Overthrow.st`
  - Estimate: 🟡 1 h

- [x] **4.8 — Gamepad/console and text-fit check**
  - Description: The start menu's only action is `OverthrowStartGame` on `StartButton` and the spinners are `SCR_SpinBoxComponent`s — confirm the three new text widgets **add no focusable element and do not change tab order**. Check text fit at 1080p in the 900 px panel with the **longest** description.
  - File(s): `UI/Layouts/Menu/StartGameMenu.layout`
  - Estimate: 🟡 1 h

- [x] **4.9 — Gates: compile-check + All tier + export list**
  - Description: `tools/compile-check.sh` (expect 0); All group green at baseline; report the new string ids for the user's Workbench export.
  - File(s): —
  - Estimate: 🟢 20 min

**Acceptance:** compile-check 0; All green; all four texts change correctly when either faction spinner or the difficulty spinner moves, **including the auto-swap case where changing one faction moves the other**; no hardcoded English remains in the layout, the context or the five shipped difficulty configs; `Difficulty_TestWorld.conf` untouched; no new focusable widget; export list handed over.

**✅ Met 2026-08-09.** compile-check exit 0 (5944 files, 6 s); All group **86 tests, exit 0** — the 0.1 baseline exactly. Input-conflict checker exit 0 (0 errors, 0 warnings, 0 pre-existing; `chimeraInputCommon.conf` untouched, this phase adds no action). Layout: 17 widget GUIDs, **0 duplicates**, braces balanced; all 18 `6B3D…` allocations unique repo-wide. `git diff --stat Language/` lists `localization_Overthrow.st` and nothing else. `Difficulty_TestWorld.conf` and `Configs/System/` are byte-unchanged. **No new focusable widget by construction**: every `components {}` block in `StartGameMenu.layout` belongs to a `ButtonWidgetClass` (3 × `SCR_SpinBoxComponent`, 1 × `SCR_InputButtonComponent`), and all four description widgets are bare `TextWidgetClass` with no component block and no base-layout inheritance — the shipped `DifficultyDescription` is the control case. **Auto-swap refresh verified by trace, not by assumption** (`SCR_SpinBoxComponent.SetCurrentItem` invokes `m_OnChanged` synchronously, so the swap resolves depth-first and the outer handler redraws last with both keys settled). **Text fit computed, not rendered**: worst case 6 wrapped lines = 164 units against ~179 available; the 1080p geometry, the assumptions and three fallbacks are in `context.md`. ⚠️ **Blocked on the user's Workbench string export** — 10 new ids, listed in the session note; until it runs the setup screen draws raw `#OVT-` keys.

---

## Phase 5: Tests, verification and the play-test checklist (6/6 complete) — `component-developer`

*Three cases, one per tier that can hold one. The existing guards already cover entry structure, link resolution and spawn-trigger delivery for free — **do not duplicate any of them** (§7).*

- [x] **5.1 — Logic case: the spawn filter selects exactly one welcome**
  - Description: `OVT_TEST_Logic_Tutorial_SpawnContextSelectsOneWelcome`. Two in-memory `PLAYER_SPAWNED` entries filtered `house`/`nohouse`, both priority 100. `FindMatches` with ctx `"house"` → exactly one id (the house one); `"nohouse"` → exactly the other; **`""` → exactly zero**, which is the asymmetry that makes D6's client-side default load-bearing and the single most likely thing a future refactor breaks. **Prove it red once** (swap the expected ids, or change the `""` expectation to 2) and record the exact failure text, method and date in `context.md`.
  - File(s): `Scripts/Game/Tests/TestSuites/Logic/`
  - Estimate: 🟡 1 h

- [x] **5.2 — Init branch: spawn-filter validity and welcome coverage**
  - Description: A new branch on the existing entry guard, mirroring its `CheckTransactionFilters` shape: (a) every `PLAYER_SPAWNED` trigger's `m_sFilter` is one of `""`, `"house"`, `"nohouse"` — a typo'd filter otherwise means a welcome that **silently never fires**; (b) at least one **enabled** entry matches `"house"` and at least one matches `"nohouse"`, so deleting or disabling either variant fails the build. Deliberately "at least one", not "exactly one", so a third-party mod adding a spawn entry does not break the gate. **Prove it red once** (misspell `nohouse` in `welcomeNohome.conf`; expect a named failure) and record it.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c`
  - Estimate: 🟡 1 h

- [x] **5.3 — Campaign case: the server authored a spawn context**
  - Description: `OVT_TEST_Campaign_Tutorial_SpawnContextIsAuthored`. After the campaign starts, `GetPlayerSpawnContext()` for the test world's local player returns one of the two known values (**not `""`**) — this is what makes D12's server-side map worth having, pinning that `FinalizePlayerPreparation` ran the new code on a real start path. **Prove it red once** (comment out the `SetPlayerSpawnContext` call in the house branch; expect `""`) and record it.
  - File(s): `Scripts/Game/Tests/TestSuites/Campaign/`
  - Estimate: 🟡 1.5 h

- [x] **5.4 — Final verification gates**
  - Description: `tools/compile-check.sh` (0); Fast (0); All (0) — case count at the recorded baseline **+3**; duplicate-GUID grep over the `6B3D…` allocations prints nothing; `git diff --stat Language/` shows only the `.st`; `git diff --stat` shows no change under `Configs/FieldManual/` or `Configs/System/`. **No `maxAttempts` anywhere** — a test that needs retries is a bug in the test.
  - File(s): —
  - Estimate: 🟢 30 min

- [x] **5.5 — Write the play-test checklist into this file**
  - Description: Transcribe §7's single-player (P1-P10) and two-client (M1-M6) checklists into "Needs Human Verification" below as a copy-pasteable list, including the **seen-store reset procedure**, the **export prerequisite**, and the ⚠️ warning that a client launch opens a window on the user's desktop and can orphan (always pass a long `--timeout`; it defaults to 600 s and kills the client mid-test).
  - File(s): `docs/features/new-player-experience/first-spawn/tasks.md`
  - Estimate: 🟢 30 min

- [x] **5.6 — Record the findings for the bug backlog**
  - Description: §9's F1 (`realEstateCostMultiplier` is dead), F2 (`ChargeRespawn` is gated behind `money > 500`), F3 (the supporting faction is neither replicated nor persisted, and `OVT_WantedInfo.c:204-208` compares the undercover HUD icon against the wrong key when a host picked USSR) written into `context.md` **as findings, not fixes** — all three are balance or save-format changes wearing bug-fix clothes (R9). Plus D8's note that no shipped entry now exercises the Learn-more-hidden branch.
  - File(s): `docs/features/new-player-experience/first-spawn/context.md`
  - Estimate: 🟢 30 min

**Acceptance:** three cases added, each proven able to fail with method and date recorded, **no `maxAttempts` anywhere**; all four commands green; the play-test checklist written down rather than described; findings recorded.

**✅ Met 2026-08-09.** compile-check exit 0 (5945 files, 5 s); **Fast 51 exit 0, All 88 exit 0**. Three assertion sites: `OVT_TEST_Logic_Tutorial_SpawnContextSelectsOneWelcome` (appended to `Logic/OVT_TEST_Logic_Tutorial.c`), `CheckSpawnFilters` + `CheckWelcomeCoverage` as two new branches on `OVT_TEST_Init_Tutorial_ManagerResolvesAndLoadsEntries`, and `OVT_TEST_Campaign_Tutorial_SpawnContextIsAuthored` (new file `Campaign/OVT_TEST_Campaign_TutorialSpawnContext.c`). **Four mutations, four recorded failure texts** in `context.md`'s Proven-Red Table — the Init guard's two branches were proven **separately**, because one mutation exercises only one of them. **No `maxAttempts`** in any new code (the sole occurrence in `OVT_TEST_InitSuite.c` is a pre-existing comment at `:1497` stating there is none). ⚠️ **The `+3` target was arithmetically wrong**: T2 is a *branch* on an existing case and adds assertions, not cases, so the honest close is Fast +1 and All +2. `Configs/FieldManual/` and `Configs/System/` byte-unchanged; the `6B3D…` allocations are 27, each defined once (`{6B3D000000000010}` appears twice only as definition-plus-reference, which is required). `git diff --stat Language/` now also lists the six `.<lang>.conf` exports — **that is the user's Workbench export having run**, not an edit by any phase; Phase 5 touched nothing under `Language/`.

---

## Phase 6: Help and documentation sync (4/4 complete) — `help-docs-sync`

*Required: this feature changes what players see in their first two minutes and on the campaign-setup screen.*

- [x] **6.1 — Reconcile the Field Manual's Welcome page with the shipped welcomes**
  - Description: Verify `#OVT-FieldManual_Welcome_*` is consistent with the four shipped pages, and specifically that `_Text2`'s "a house … with a car parked outside it" does not contradict `welcome-nohome`. If it needs a clause about the houseless case, **add exactly that clause** — nothing more.
  - File(s): `Language/localization_Overthrow.st`
  - Estimate: 🟡 1 h

- [x] **6.2 — Wiki: scalpel, not sweep**
  - Description: The `tutorial-content` D10 rule. Update campaign-setup / difficulty documentation **only where it states something §3.7 proves false** — in particular anything claiming the supporting faction supplies troops or equipment, or that real-estate prices scale with difficulty. **Budget: at most 2 pages updated, 0 created.** Every page listed with its page id and a one-line reason. ⚠️ wikijs MCP: search returns wrong pageIds, update needs `tags`, and a failed update leaves the render stale — verify the live render after writing.
  - File(s): (wikijs MCP)
  - Estimate: 🟡 1.5 h

- [x] **6.3 — Respect `starter-jobs-retirement`'s territory**
  - Description: **Do not touch** the `**Tutorial Jobs**` paragraph or item 6 under "Systems Worth Knowing About" on `getting-started` — both belong to feature #5.
  - File(s): (constraint)
  - Estimate: 🟢 5 min

- [x] **6.4 — Do not re-audit for staleness**
  - Description: `field-manual` swept every page on 2026-08-08. This phase corrects what this feature made wrong or proved false; it does not re-open that sweep.
  - File(s): (constraint)
  - Estimate: 🟢 5 min

**Acceptance:** every wiki page updated is listed with its page id and a one-line reason; nothing created; the do-not-touch list intact; the manual's Welcome page and the two welcome entries do not contradict each other.

---

## Needs Human Verification

*Populated by task 5.5. Play-testing is the runtime gate for everything UI, multiplayer or truth-shaped — no automated gate in this project can catch a popup that does not render, a per-player RPC that leaks, or a well-formed lie.*

### ⛔ Prerequisites — do these two first, in this order

- [ ] **1. Workbench string-table export (user-owned, and it blocks everything below).** **15 ids** were added or rewritten across Phases 2 and 4: `OVT-Tutorial_WelcomeIntro_Body1`, `_Body2`, `_Body3`, `_Body4`, `OVT-Tutorial_WelcomeNohome_Body2`, `OVT-Faction_US_Occupying`, `OVT-Faction_USSR_Occupying`, `OVT-Faction_US_Supporting`, `OVT-Faction_USSR_Supporting`, `OVT-Difficulty_Easy_Desc`, `OVT-Difficulty_Normal_Desc`, `OVT-Difficulty_Hard_Desc`, `OVT-Difficulty_Extreme_Desc`, `OVT-Difficulty_Insane_Desc`, `OVT-Difficulty_Numbers`. **Until this runs, the welcome pages and all four campaign-setup descriptions draw raw `#OVT-` keys — which looks exactly like a bug and is not one.** First observation of the whole checklist: open the start menu and confirm the difficulty description is a sentence rather than `#OVT-Difficulty_Normal_Desc`. If it is a raw key, stop — the export has not been picked up.
- [ ] **2. Reset the seen store.** Seen state lives in `$profile:.save/settings/ReforgerGameSettings.conf` under an **`OVT_TutorialSettings`** block. Clear that block via Workbench → **User Settings → Edit Game Settings**, and confirm `m_bTipsDisabled 0` in the same block. **An absent `m_aSeen` is the empty state, not corruption** — do not "repair" it by adding one.
- [ ] **3. Refocus/reload Workbench before believing any "the fix didn't work" result.** Workbench can play-test **stale scripts** after WSL-side edits; a sibling feature's first play-test was a false negative for exactly this reason.

### Single player — P1 to P10

- [ ] **P1 — Occupying spinner, every option.** Move it through every value. *Expect:* the occupying description changes each time; **the supporting description also changes when the auto-swap fires** (the two factions cannot be the same, so moving one moves the other); no raw `#OVT-` keys anywhere. The shipped list is US and USSR only.
- [ ] **P2 — Difficulty spinner, all five presets.** *Expect:* the description **and** the numbers line both change. **Easy: 500 starting cash, free fast travel. Normal: 100 and 5** (this pair is the check that the config default-fallback resolves). **Extreme and Insane: 0 starting cash.** Nothing anywhere claims real-estate prices scale with difficulty, or that respawning costs a flat amount.
- [ ] **P2b — Layout sanity at 1080p.** The four description lines sit like the shipped difficulty description did — same padding, font and alignment. **The Start button must not move** when a description grows to two lines (the `Fill` spacer absorbs it). If the block looks squeezed, three cheap fallbacks are listed in `context.md`'s Phase 4 note, cheapest first.
- [ ] **P3 — First spawn, cleared seen store.** Start a new campaign. *Expect:* a four-page modal titled **"Welcome to Overthrow"** appears shortly after Start Game, page indicator `1 / 4`, page 2 describing a house, a car parked outside it, and starting cash.
- [ ] **P4 — Paging.** Next 1→2→3→4, Back 4→3→2→1, then Dismiss on page 4. *Expect:* both directions work, the indicator tracks, Dismiss ends it. Check page 3's `<action>` key glyph actually renders as a key, not as literal text.
- [ ] **P5 — 🔴 The single most important observation in this list.** Quit the game entirely (Alt-F4 is fine), relaunch, start **another new campaign**. *Expect:* **no welcome at all.** Repeat once more on a different world/campaign: still nothing. This is the per-machine seen store doing its job.
- [ ] **P6 — Dismiss on page 1.** Clear the seen store, start a campaign, Dismiss on **page 1**. *Expect:* it ends. Then confirm the Overthrow menu → **Tips** re-opens it, and **note which page it resumes on** — this is an observation to record, not a pass/fail.
- [ ] **P7 — 🔴 The houseless path.** Exhaust the starting houses (easiest: a world or config with few starting houses, or enough players) until the server log prints `No Starting homes left. Spawning at bus stop.` *Expect:* that player's page 2 is the **houseless** text — cash and a spawn point but nothing owned — and the house-and-car page **never** appears for them. This is the exact failure the feature exists to prevent, and it fails silently.
- [ ] **P8 — Die and respawn in single player.** *Expect:* **no `#OVT-IntroHint` corner hint** (the Phase 3 regression check — the hint genuinely used to fire here, and its removal is intended) and **no second welcome**.
- [ ] **P9 — Quit, then Continue the campaign.** *Expect:* no corner hint (it used to fire here too), no welcome, and the campaign resumes normally.
- [ ] **P10 — Learn More on the welcome modal.** *Expect:* the Field Manual opens on **Welcome to Overthrow**, not on the front page.
- [ ] **P11 — Spawn mechanics unchanged (owed since Phase 1).** A fresh player still gets a house, a car parked outside it (in a parking spot or at a kerb — not "in a garage") and the preset's starting cash. This feature reads a fact out of that flow; it must not have changed it.

### Two clients on a dedicated server — M1 to M6

> ⚠️ **A client launch opens a window on your desktop and can orphan.** Close both clients deliberately when you are done.
> ⚠️ **`tools/launch-game.sh` defaults to a 600 s timeout and will kill the client mid-test.** **Always pass a long `--timeout`.**

```bash
tools/launch-server.sh
tools/launch-game.sh --timeout 3600 --profile OverthrowClient1 --allow-concurrent -- -client 127.0.0.1:2001
tools/launch-game.sh --timeout 3600 --profile OverthrowClient2 --allow-concurrent -- -client 127.0.0.1:2001
```

- [ ] **M1 — Both profiles start with a cleared `OVT_TutorialSettings` block.** Each profile has its own settings file; clearing one does not clear the other.
- [ ] **M2 — Client A joins and spawns.** *Expect:* A sees its welcome. **B sees nothing**, and B's log does not mention a welcome entry id. (M2 and M4 are the **only** observations that can catch a wrong `Rpc()` arity — it compiles clean and passes every automated case.)
- [ ] **M3 — Client B joins and spawns.** *Expect:* B sees its own welcome, once.
- [ ] **M4 — JIP.** With the campaign already running, disconnect B, clear B's seen store, reconnect. *Expect:* B gets a welcome on its own first spawn, with page 2 matching **whichever branch the server took for B** — check the server log for `Player assigned home at …` versus `No Starting homes left`.
- [ ] **M5 — Whichever client got the bus stop, if any.** *Expect:* it reads the houseless page 2.
- [ ] **M6 — Neither client** ever sees two welcomes, or the other player's.

> **If M1-M6 fail:** this is the epic's outstanding F7 question and this feature's highest-risk unknown. A per-player failure that also reproduces for `economy-first-buy` is a **`tutorial-system` defect**, not this feature's filter — report it as such and say so. The degradation path is deliberate: if the spawn context is lost, the welcome still appears with the house page after about 5 seconds, so "wrong page 2" and "no welcome at all" are different bugs.

---

## Technical Debt

- [ ] 💳 **No shipped tutorial entry exercises the Learn-more-hidden branch** - Priority: Low
  - Description: `proofWelcome.conf` was deliberately the unlinked proof entry so `OVT_TutorialContext.c:499-500` shipped covered. Both welcomes now carry the Field Manual link, and `tutorial-content`'s ten all link, so nothing live exercises the hidden branch.
  - Reason: D8 — the requirement reserves `#OVT-FieldManual_Welcome_Title` for this feature, and the branch is UI-only and outside the automated spine anyway.
  - Effort: One-line inspection, or a future content pass keeps one entry unlinked.

*(Findings F1-F3 from §9 are recorded in `context.md` for the bug backlog — they are defects in other systems, not this feature's debt.)*

---

## Task Status Legend

- [ ] Not started
- [ ] 🔄 In progress
- [ ] ⏸️ Blocked (waiting on something)
- [x] ✅ Completed
- [x] ❌ Cancelled/Won't do

---

## Notes

### Task Estimation
- 🟢 Small (< 1 hour)
- 🟡 Medium (1-3 hours)
- 🔴 Large (> 3 hours)

---

*Update this file as tasks are completed. Mark tasks with ✅ immediately when done. Add new tasks as they're discovered.*
