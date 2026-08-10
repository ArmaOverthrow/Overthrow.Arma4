# Tutorial Content - Context & Decisions

**Last Updated:** 2026-08-09 (feature closed — play-test passed, wiki edit applied)
**Current Phase:** none, feature complete
**Status:** 🟢 **COMPLETE** (24/24 tasks, play-test passed and signed off 2026-08-09, all owed items discharged)

**Epic:** `new-player-experience` (feature #3 of 5 — depends on #1 `tutorial-system` and #2 `field-manual`, both shipped; parallel-buildable with #4 `first-spawn`)

---

## Quick Status

**What's Done:**
- ✅ Plan written (`implementation.md`, phases 1 / 2 / 2b / 3) with the ten-entry set, the frozen link table copied verbatim, the verified trigger reference, and §3.7's eight-row trap table
- ✅ Docs scaffolded (`tasks.md`, this file); `implementation.md` status flipped to In Progress
- ✅ Preconditions re-verified on this tree 2026-08-09: the `6B3C…` GUID block is still free (0 matches outside `implementation.md`'s own allocation table), `Configs/Tutorials/` holds only the two proof entries, and the working tree is clean apart from this feature's own docs

- ✅ **Phase 1 complete (2026-08-09)** — six early-game entries live, gates green, Init log reports 7 structurally valid entries. See the session log below for the sentence cut for lack of evidence and the §3.7 trap that turned out to be wrong.
- ✅ **Phase 2 complete (2026-08-09)** — the four mid-game entries live, gates green, Init log reports **11** structurally valid entries. All ten authored entries now exist. Session log below records the four §3.7 traps re-verified (all correct this time) and the FOB-deploy note.

- ✅ **Phase 2b complete (2026-08-09)** - the `SHOP_GUNDEALER` enum-name filter contract is pinned by one additive branch, proven red once and reverted. Fast still 47.
- ✅ **Phase 3 complete (2026-08-09)** - as-built starter-job coverage mapping recorded below and handed to `starter-jobs-retirement`; verification sweep green; play-test checklist written into `tasks.md`. **No wiki page was edited** (see the session log).

- ✅ **String table exported by the user (2026-08-09)** — all 19 ids live; tips render their text.
- ✅ **Play-test passed and signed off (2026-08-09)** — *"all the tips seem fine"*. The first attempt was a false negative caused by stale compiled scripts in Workbench; see the closing session log.
- ✅ **The wiki edit is applied** — `getting-started` (page id 2) carries the tip-system paragraph, verified live in the rendered page, with `starter-jobs-retirement`'s two `**Tutorial Jobs**` paragraphs intact.

**What's Next:**
- ✅ **Nothing on this feature.** It is closed.
- ▶️ **`first-spawn`** and **`starter-jobs-retirement`** are the epic's remaining two, and #5's precondition (the as-built coverage mapping below) is satisfied.
- 📋 **Never individually exercised, and honestly unverified rather than passed:** F5 (two tips queueing from one gun-dealer purchase), F7 (per-player isolation across two clients — the MP pass was not run), P10 (`bases-first-capture`), and the four Learn More link spot-checks. `bases-first-capture` carries the most residual risk, being the only entry delivered by the 300 m proximity fan-out rather than a per-player RPC.
- 📋 **Two Field Manual strings are still known wrong** and were out of scope every phase: `WantedSystem_Text`'s "comes looking for you" and `BaseCapture_Text`'s "in that area". The public wiki is already right on both.

**Blockers:**
- None. Feature closed.

---

## Key Files

- `Configs/Tutorials/proofFirstBuy.conf` — **adopted, not replaced** (D3). Its id `economy-first-buy` and its filename are immutable; only body text, title text and the link key change
- `Configs/Tutorials/proofWelcome.conf` — **DO NOT TOUCH.** `first-spawn` owns `welcome-intro`, `#OVT-IntroHint` and `m_aHintedPlayers` (F8)
- `Language/localization_Overthrow.st` — **the only localization file this feature edits.** Never the `.<lang>.conf` runtime exports (a deny rule exists on one machine only; this is the portable half)
- `Prefabs/GameMode/OVT_OverthrowGameMode.et` — nine elements **appended** to `OVT_TutorialManagerComponent.m_aEntries`. Append only, never reorder (R5)
- `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c` — the two guards that do this feature's automated work for free, and the single file Phase 2b touches
- `docs/features/new-player-experience/tutorial-system/implementation.md` §5 — the authoritative upstream contract (entry ids, string keys, trigger catalog, add-an-entry procedure, Rule 0)
- `docs/features/new-player-experience/field-manual/implementation.md` §3.3 — the twelve frozen deep-link ids, copied into this plan's §3.2

---

## Key Decisions

*The full set with rationale is `implementation.md` §5 (D1-D13). Recorded here are the ones a future reader will trip over.*

- **D3 — `economy-first-buy` is adopted, and its file is not renamed.** The id is already in players' seen stores; a fresh id would re-show the same information to everyone who dismissed it. `proofFirstBuy.conf` keeps its cosmetically wrong name because the id, not the filename, is what anything reads.
- **D4 — the skills tip fires on opening the character sheet, not on spending a point.** "First XP gain" and "first level-up" have no invoker; `PLAYER_SKILL` fires only *after* the player has already found the screen and worked it out. This is a content-side trigger choice, **not** a framework gap.
- **D5 — placing is unfiltered.** Filtering on `Camp` would miss the player who puts up a Poster first, and "placing equipment" is one of the five things `starter-jobs-retirement` needs covered.
- **D7 — base capture accepts the 300 m proximity fan-out**, because no per-player event exists anywhere in the capture flow. The three consequences (no "you captured", correct in both directions, correct for a bystander) are handled by the **writing**, not the config.
- **D13 — all ten triggers are restore-safe, and that was verified rather than hoped.** No guard, filter or threshold was needed anywhere — which is exactly why the check is recorded, so the next author does not add one.

---

## Gotchas / Traps

- **The only failure mode no gate can catch is a well-formed lie.** compile-check, all 77 assertions, the id/link guards and the localization checks all pass happily on a false sentence. Rule 0 is a **phase-front task** (1.1, 2.1), not a review step.
- **§3.7 pre-loads eight traps** that already cost a source audit to find. The three that still stand here: wanted levels 1 and 5 are unreachable by escalation, camps are *not* actually blocked near bases, and there are **exactly three** skills (`Trade`, `Stealth`, `Diplomacy`).
- **⚠️ A SECOND §3.7 row is struck (2026-08-09, after merging `main`).** The "prices are an integer-division step, not a curve" trap is false: **BUG-105 disproved it at runtime** (measured `1116 -> 1067 -> 1018` across stock 1/50/100 — the float curve). EnforceScript's `int / int` does not universally truncate like C; nested in an expression with float operands it evaluates fractionally, which is why the sibling BUG-024 case *did* truncate and this one did not. **The shipped tip text needs no change** ("prices differ from town to town" is true either way) — what was wrong was the `Comment`'s *do-not-reintroduce-the-curve-wording* instruction, now withdrawn in the `.st`. **Two of eight trap rows have now proved false.** The rule stands and is stronger for it: verify a trap against source before relying on it, including one written by a previous phase of your own feature.
- **⚠️ One §3.7 row was itself wrong and is struck (2026-08-09).** The "gun dealers have no dedicated map icon" trap is false: gun dealers are enumerated from `economy.GetGunDealers()` and drawn with a dedicated `"gundealer"` sprite by `TryCreateGunDealerIcon` (`OVT_MapIcons.c:627-632`, `:111-134`), never reaching `TryCreateShopIcon`'s switch. Found by the Phase 1 fact-check, verified independently, struck in `implementation.md` §3.5, §3.7 and DoD Q2. **A trap list is not evidence** — this one row would have propagated into the Phase 3 wiki pass as a "correction" that broke a correct page.
- **Plus the two lies that already shipped once:** money can be stolen or deposited (neither mechanic exists), and the Overthrow menu contains a money screen or the Field Manual (it contains neither).
- **"Learn more" is not on the HUD popup.** The route is HUD tip → "Overthrow Menu" prompt → main menu → Tips → modal → Learn More. A play-test that does not know this will report a missing button (§3.4).
- **Menu-triggered tips appear when the screen closes**, not while it is open. This looks like a bug and is not (§3.3).
- **New `.st` items are invisible in-game until the user re-exports in Workbench** — until then every new tip draws its raw `#OVT-` key (R6).

---

## Starter-job coverage mapping: AS BUILT (task 3.1, recorded 2026-08-09)

**This is the precondition `starter-jobs-retirement` (feature #5) named and could not begin without.** It is the *as-built* version, re-derived against the ten shipped entries and against source, **not** a transcription of `implementation.md` §3.5, one row of which (`findGunDealer`) was proven wrong in Phase 1 and is struck there. A pointer to this section is appended to `starter-jobs-retirement/requirements.md`.

All five job configs are `m_bPublic 0`, `m_iMaxTimes 1`, `m_iMaxTimesPlayer 1`, and are registered on `Prefabs/GameMode/OVT_OverthrowGameMode.et:30, :34, :36, :38, :40`.

| Starter job | What it actually did in code | Covered as-built by | Residual gap after retirement |
|---|---|---|---|
| `Configs/Jobs/findShop.conf` | `OVT_GetShopLocationJobStage.c:18-29` picks the **first** shop registered in the nearest town and writes its position into `job.location` (a job marker pointing at one specific instance), then `OVT_WaitTillPlayerInRangeJobStage` at `m_iRange 10` completes it. Reward: $50 + two field dressings | `economy-first-buy` (id unchanged, adopted; body: shops sell and most buy back, stock is finite and replenishes, prices differ by town, money is a balance on the player record) + `map-first-open` (body names shops among what the map marks) | **Directed discovery.** No tip points at a *named instance*. It does not need to: `OVT_MapIcons.TryCreateShopIcon:137-183` draws **every** registered shop with a per-type sprite, ungated by any discovery or knowledge flag, for any player carrying a map. The job's unique contribution was a marker on one shop plus the cash/items |
| `Configs/Jobs/findGunDealer.conf` | `OVT_GetDealerLocationJobStage` writes the nearest town's dealer position into `job.location`; same 10 m arrival stage. Reward: $50 | `shops-first-gun-dealer` (`PLAYER_TRANSACTION` filtered `SHOP_GUNDEALER`; body: a dealer trades weapons and military equipment from its own stock list, restocks separately, and takes almost anything priced) + `map-first-open` | **Directed discovery only, and it is the weaker of the two gaps.** ⚠️ **§3.5's claim that gun dealers have no dedicated map icon is FALSE and must not be carried into feature #5.** Gun dealers are enumerated separately from `economy.GetGunDealers()` (`OVT_MapIcons.c:626-638`) and drawn by `TryCreateGunDealerIcon:111-134` with a dedicated `"gundealer"` sprite; they never reach `TryCreateShopIcon`'s switch. They are marked distinctly and unconditionally. Also note the tip is **transaction**-triggered, so a player who finds a dealer and buys nothing sees nothing |
| `Configs/Jobs/placeEquipmentBox.conf` | `OVT_PlaceableItemJobStage` with `m_sPlaceableName "Ammobox"`. Reward: $0 | `place-first-placeable` (**unfiltered** `PLAYER_PLACE` per D5, so an `Ammobox` fires it) | **None.** The unfiltered trigger is strictly broader than the job's single placeable |
| `Configs/Jobs/placeACamp.conf` | `OVT_PlaceableItemJobStage` with `m_sPlaceableName "Camp"`. Reward: $0 | `place-first-placeable` (same entry; the shipped body names the camp explicitly as the top of the range: *"from small items up to a full camp"*) + its "Learn more" → `#OVT-FieldManual_Camps_Title` | **None for the topic.** One nuance: a player whose first placeable is a Poster gets the tip then and **not** again at their first Camp, since an entry shows once per machine. The camp material is still reachable in the manual page the tip links |
| `Configs/Jobs/recruitACivilian.conf` | `OVT_HasRecruitJobStage` (completes once the player holds any recruit). Reward: $0 + 10 XP | `recruits-first-recruit` (`PLAYER_RECRUIT_ADDED`, unfiltered; body: a recruit belongs to the player who hired them, joins their group, takes group orders, persists between sessions, sixteen per player) | **Availability, not existence.** The tip fires on the **first recruit gained**, not on the option becoming available, so a player who never uses "Recruit Civilian" never sees it. The job had the same shape (it also only completed on a recruit being held) but it *appeared in the Jobs list* beforehand, which the tip system has no equivalent of |

**Net for feature #5:** all five topics are covered by a live entry. The two residual gaps are **directed discovery** (`findShop`, `findGunDealer` pointed a marker at one named instance; tips never do, and the map already marks every shop and dealer unconditionally) and **recruit availability** (nothing announces that recruiting is possible before the player recruits). Neither is a reason to keep a job that only the first player on a server ever receives (BUG-037). The small rewards disappear with the jobs: **$100 + 10 XP + two field dressings** across all five, once per campaign per player.

---

## Non-blocking notes for other features

*(populated by tasks 2.8 and 3.1)*

### `tutorial-system` — FOB deployment has no trigger (NON-BLOCKING, recorded 2026-08-09 by task 2.8)

**Not a blocker, and nothing is owed.** The requirements ask for a "first FOB deployed" tip; the shipped trigger catalogue cannot express one, and `tutorial-content` ships without it deliberately rather than working around it (§3.8, D11, I7).

- `DeployFOB()` (`OVT_ResistanceFactionManager.c:466-553`) touches **neither** `m_OnPlace` (`:756`) nor `m_OnBuild` (`:854`), which are the manager's only two `ScriptInvoker`s (`:109-110`). There is no `m_OnFOB*` invoker anywhere in the repo.
- The only signals a *successful* deploy produces are a **broadcast** `SendTextNotification("DeployedFOB", -1, …)` inside `RegisterFOB` (`:1120`) and the owner-scoped progress events on `OVT_ContainerTransferComponent`, whose args carry no playerId. Per-player signals exist only on **rejection** (`:494`, `:507`, `:523`).
- **The cheapest seam, if a future content pass ever wants the tip:** a `ScriptInvoker` beside `RegisterFOB` (`OVT_ResistanceFactionManager.c:1106-1120`) carrying the playerId that `OnFOBDeploymentComplete` already derives, plus one `OVT_ETutorialEvent` member and one dispatch branch in `OVT_TutorialManagerComponent`. A few lines, and **a `tutorial-system` change, not a `tutorial-content` one**.
- **Coverage in the meantime:** `build-first-structure` fires on the first *build* (which happens at a held base, a FOB or a camp), and its "Learn more" opens `#OVT-FieldManual_FOBs_Title`, whose "Deploying a FOB" section documents deployment properly. Do not synthesise a deploy trigger from the transfer-progress events or from the broadcast notification.

---

## Proven-Red Record

*Every new assertion must be proven able to fail once, with the exact failure text and date recorded. No `maxAttempts`.*

Phase 2b added exactly one branch (no new case) to `OVT_TEST_Init_Tutorial_ManagerResolvesAndLoadsEntries` (`Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c`, helper `CheckTransactionFilters`). The breakage was applied to `Configs/Tutorials/shopsFirstGunDealer.conf`, run through `tools/run-tests.sh "{6A6E29FF47ECB840}"` (**exit 1**, `FAILED (1 of 47)`, the named case being `OVT_TEST_Init_Tutorial_ManagerResolvesAndLoadsEntries`), then reverted by the inverse `sed` and verified byte-exact against the pre-break file (`md5 7c34dd905c31d0f9d8c1190747c1636a`), with the Fast group re-run green (**exit 0**, 47 tests, Init log back to *"Tutorial manager is live with 11 structurally valid entries"*).

| Assertion | How it was broken | Exact failure text | Date |
|---|---|---|---|
| Every `PLAYER_TRANSACTION` trigger with a non-empty `m_sFilter` names a real `OVT_ShopType` member (compared against `SCR_Enum.GetEnumNames(OVT_ShopType, …)`, so a new shop type is accepted automatically and a renamed one is caught) | `m_sFilter "SHOP_GUNDEALER"` → `m_sFilter "SHOP_GUNDEALERS"` in `Configs/Tutorials/shopsFirstGunDealer.conf` | `Tutorial entry 'shops-first-gun-dealer' filters PLAYER_TRANSACTION on 'SHOP_GUNDEALERS', which is not the name of any OVT_ShopType value. The manager builds the value it compares against with SCR_Enum.GetEnumName(OVT_ShopType, shop.m_ShopType) (OVT_TutorialManagerComponent.c:255), so this filter can never match ANY transaction: the tip will SILENTLY NEVER FIRE - no compile error, no runtime warning, no log line, just a tutorial nobody ever sees. Fix the m_sFilter in the entry's .conf under Configs/Tutorials/ to one of: SHOP_GENERAL, SHOP_DRUG, SHOP_CLOTHES, SHOP_FOOD, SHOP_ELECTRONIC, SHOP_GUNDEALER, SHOP_VEHICLE` | 2026-08-09 | ✅ reverted |

**Finding from the red-proof, and it settles risk R2 in the affirmative:** the enumerated names printed by the failure message are the **bare member names** — `SHOP_GENERAL, SHOP_DRUG, SHOP_CLOTHES, SHOP_FOOD, SHOP_ELECTRONIC, SHOP_GUNDEALER, SHOP_VEHICLE`. That is the same list `SCR_Enum.GetEnumName` returns at runtime for `shop.m_ShopType`, so the shipped filter `SHOP_GUNDEALER` is **literally correct** and the gun-dealer tip is not dead. The plan's assumption held; the branch now pins it so a rename of the enum member (or of the filter) goes red instead of going quiet.

---

## Session Log

### 2026-08-09 - Phase 3 shipped (coverage mapping, verification, play-test checklist)

**Zero product changes.** Phase 3 produced documentation only: the as-built starter-job coverage mapping above, the pointer section appended to `starter-jobs-retirement/requirements.md`, and the play-test checklist in `tasks.md`. No `.conf`, no `.st`, no prefab, no script was touched.

**Verification sweep (task 3.4), all on this tree:**

| Gate | Result |
|---|---|
| `tools/compile-check.sh` | **exit 0**, 5940 files, Game module |
| Fast `{6A6E29FF47ECB840}` | **exit 0**, **47** tests |
| All `{6A6E2A002F53A581}` | **exit 0**, **78** tests |
| `git diff --stat Language/` | `localization_Overthrow.st` only, no `.lang.conf` |
| I2, ten link keys vs the frozen §3.2 table | ten for ten, verbatim and case-sensitive |
| I6, framework drift | zero diff under `UI/Layouts/`, `Configs/FieldManual/`, `Configs/System/`; the only `Scripts/` diff is Phase 2b's `Tests/TestSuites/Init/OVT_TEST_InitSuite.c` |
| F8, `first-spawn` territory | `Configs/Tutorials/proofWelcome.conf` byte-identical to HEAD; zero occurrences of `IntroHint` or `m_aHintedPlayers` anywhere in the diff |
| Duplicate GUIDs | 54 unique `6B3C…` allocations. The nine apparent duplicates are each a conf resource GUID and the prefab's reference to it, which is what a reference is. No duplicate *declaration* anywhere |
| Q7, ids | ten ids, lowercase-kebab, all unique, `economy-first-buy` unchanged |

**On the All-group count:** the plan expected 77-78 and predicted the 78th came from an untracked `Campaign/OVT_TEST_Campaign_TutorialSpawnTrigger.c`. That file is now **tracked**, so 78 is simply the tree's number. Not a regression, and nothing this feature added.

**The wiki fact-check found nothing to correct, which is the interesting result.** All three candidate false claims the phase brief named were checked against the live pages and **all three are already right**:

- `wanted-system` says *"2+ Stars: Hostile - the OF stops reading you as a civilian and shoots on sight"*. It does **not** claim the faction comes looking for the player. Matches the narrow verified claim the Phase 1 wanted tip settled on.
- `base` says *"Taking a base also raises the pressure on the occupying faction across the whole campaign"*, explicitly global. It does **not** say "in that area". (The **Field Manual** string `#OVT-FieldManual_BaseCapture_Text` still does, and remains a candidate correction outside this feature's scope.)
- `base` says *"You capture a base by approaching the flag there and using the capture action on it. That starts a QRF ... the base only changes hands once the battle has been won"*, and `getting-started` says *"use the action to trigger a battle to capture the base"*. Both are correct: the capture action is at the flag, but the flip is on the QRF score, and neither page claims a flag-take or a planting mechanic. `qrf` describes the point count accurately too.

This is what `field-manual`'s 2026-08-08 sweep was for. Task 3.2 part (a) therefore required **zero edits**, which is a legitimate outcome of a scalpel pass, not a skipped task.

**Task 3.2 part (b) could not be applied: the wikijs MCP page-read path is broken.** `wikijs_connection_status` reports healthy and authenticated, and `wikijs_search_pages` returns results, but **every** call to `wikijs_get_page` and `wikijs_get_page_children` fails with `RetryError[<Future ... raised Exception>]`, for page ids and for slugs, repeatedly over the whole session. Page content was audited by fetching the rendered pages over plain HTTP instead (all HTTP 200), which is enough to **read** but not enough to **write**: `wikijs_update_page` takes the full page body, and reconstructing markdown from rendered HTML would have rewritten the whole page, destroying link syntax and formatting and risking the `**Tutorial Jobs**` paragraph that task 3.3 forbids touching. **Nothing was written to the wiki. No page was created or updated.** The exact sentences and insertion point are in the Phase 3 report and in `tasks.md`.

**Two inherited Field Manual claims stay reported rather than fixed** (`Configs/FieldManual/` is out of scope for every phase of this feature): the wanted page's "comes looking for you" opener and the base-capture page's "in that area". Both were narrowed in the tip text instead.

### 2026-08-09 — Phase 2 shipped (four mid-game entries)

**Shipped:** `recruits-first-recruit` (`PLAYER_RECRUIT_ADDED`, unfiltered), `place-first-placeable` (`PLAYER_PLACE`, unfiltered per D5), `build-first-structure` (`PLAYER_BUILD`, unfiltered), `bases-first-capture` (`BASE_CONTROL_CHANGE`, the 300 m fan-out per D7). Four new `.conf` + `.meta` under `Configs/Tutorials/`, 8 new string items in `Language/localization_Overthrow.st` (`{6B3C00000000010B}`–`{6B3C000000000112}`), four elements appended to `m_aEntries` on `Prefabs/GameMode/OVT_OverthrowGameMode.et`. Zero EnforceScript; zero diff under `Scripts/`, `UI/Layouts/`, `Configs/FieldManual/`, `Configs/System/`.

**Gates:** `compile-check` exit 0 (5940 files) · Fast `{6A6E29FF47ECB840}` exit 0 / **47** cases (unchanged) · Init log now reads *"Tutorial manager is live with 11 structurally valid entries"* · `git diff --stat Language/` lists `localization_Overthrow.st` only · duplicate-GUID grep over the four new confs + the prefab prints nothing · braces balanced 7/7 in each new conf · no em-dash in authored content.

**Nothing was cut for lack of evidence this phase.** Every sentence written survived the 2.1 fact-check with a `file:line`. Two candidate sentences were *narrowed* rather than cut:

- **"A base under resistance control weakens the occupiers in that area"** (the §3.1 content intent and the wording of `#OVT-FieldManual_BaseCapture_Text`) was **not** written, because `m_iThreat` is a single global counter moved by ±250 in `ChangeBaseControl:895-915`, not an area-local value. The shipped second sentence says the verified thing instead: the occupying faction keeps reserves and spends a surplus on retaking a base the resistance holds (`OVT_OccupyingFactionManager.c:1178-1194`). The field-manual page's "in that area" wording is an inherited claim, reported to the user as a candidate correction rather than edited here (`Configs/FieldManual/` is out of scope for this phase).
- **Recruit persistence** is stated as "stay with their owner between sessions" rather than a ten-minute despawn figure: `OFFLINE_DESPAWN_TIME:41` is 600 s and `RespawnPlayerRecruits:1004-1035` restores them, but a two-sentence HUD body has no room for the mechanism and the manual page carries it.

**Traps re-verified against source rather than trusted (per the Phase 1 lesson that §3.7 itself can be wrong).** All four in scope proved **correct as written**:

| §3.7 trap | Verdict | Evidence checked this phase |
|---|---|---|
| The camp/base exclusion is not enforced | **Correct** | `Configs/Resistance/placeables.conf:29-43` sets both `m_bAwayFromCamps 1` and `m_bAwayFromBases 1` on `Camp`, but `OVT_PlaceContext.CanPlace:267-280` `return true`s inside the away-from-camps branch, so the `m_bAwayFromBases` branch below is unreachable for it |
| Building costs money, not supplies | **Correct** | `OVT_ResistanceFactionManager.BuildItem:775-858` checks funds at `:792` and charges `economy.TakePlayerMoney` at `:849`; price is `GetBuildableCost:255-258`. No material or supply stock exists anywhere in `OVT_BuildContext.CanBuild:285-349` |
| Capture is not a flag or planting mechanic | **Correct** | `OVT_QRFControllerComponent.CheckUpdatePoints:100-192` (counts inside `QRF_POINT_RANGE = 220` every 10 s, clamps at `QRFPointsToWin`, fires `m_OnFinished`), then `ChangeBaseControl:895-915`. The flag material follows in `SetControllingFaction:143-152` |
| Building happens at a held base, a FOB **or** a camp | **Correct, and worth keeping** | `CanBuild:293-347` accepts a non-occupying base within `baseRange`, a camp, or a FOB. `buildables.conf`: all 7 set `m_bBuildAtBase`, 5 set `m_bBuildAtFOB`, only the Vehicle Maintenance Ramp (`:51`) sets `m_bBuildAtCamp` — which is exactly why the tip says the offer depends on where you are standing |

**Wording constraints (tasks 2.5 and 2.7) as shipped:**

- `bases-first-capture`: *"Bases change hands only after a battle, decided by how many players hold the middle of the base against how many occupying soldiers are still there. Control runs both ways: the occupying faction keeps reserves and spends a surplus of them on retaking a base the resistance holds."* No second person, no "you captured", no assumption the reader acted, correct in both directions, no flag mechanic. The constraint is repeated inside the string's own `Comment` so a future reword cannot lose it.
- `build-first-structure`: *"Building adds structures where the resistance already holds ground: a base it controls, a FOB or a camp. What the build menu offers depends on which of those you are in, and a structure costs money and nothing else."* Says nothing about deploying, undeploying or acquiring a FOB, so it reads correctly for a player building at a camp who has never seen a FOB.
- The `place-first-placeable` body was written to fit an unfiltered trigger (D5): it reads correctly for a Poster, an Ammobox or a Camp, and names no location rule.

**Task 2.8** recorded the FOB-deploy gap above as a non-blocking `tutorial-system` note naming the `RegisterFOB` seam. Not filed as a blocker; nothing implemented.

### 2026-08-09 — Phase 1 shipped (six early-game entries)

**Shipped:** `home-first-open`, `economy-first-buy` (adopted), `shops-first-gun-dealer`, `map-first-open`, `wanted-first-level`, `skills-first-open`. Five new `.conf` + `.meta` under `Configs/Tutorials/`, 10 new string items + 1 rewritten body in `Language/localization_Overthrow.st`, five elements appended to `m_aEntries` on `Prefabs/GameMode/OVT_OverthrowGameMode.et`. Zero EnforceScript, zero diff under `Scripts/`, `UI/Layouts/`, `Configs/FieldManual/`, `Configs/System/`.

**Gates:** `compile-check` exit 0 (5940 files) · Fast `{6A6E29FF47ECB840}` exit 0 / **47** cases (unchanged) · Init log now reads *"Tutorial manager is live with 7 structurally valid entries"* · `git diff --stat Language/` lists `localization_Overthrow.st` only.

**Non-obvious decisions made during authoring:**

- **Cut, for lack of evidence: "the occupying faction comes looking for you".** This is the clause `#OVT-FieldManual_WantedSystem_Text` opens with, and it was the obvious first sentence for the wanted tip. **No dispatch, search or hunt behaviour keyed to wanted level exists**: `grep -i wanted Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c` returns nothing, and the only wanted-driven behaviour found anywhere is the *perception override* at `OVT_PlayerWantedComponent.CheckWanted:599-611` (above level 1 the player is perceived as the hostile player faction) plus the recruit-perception branches in `SCR_ChimeraAIAgent.c:29-46`. The shipped sentence is therefore the narrow, verified one: *"Above the first wanted level the occupying faction stops reading you as a civilian and treats you as an enemy on sight."* **The field-manual page's wider claim is an inherited, uncited clause and is left standing for now** (editing `Configs/FieldManual/` is out of scope for this phase); it is reported to the user as a candidate correction.
- **§3.7's "gun dealers have no dedicated map icon" trap is wrong as written, and the field manual is right.** `SHOP_GUNDEALER` really is absent from the icon switch in `OVT_MapIcons.TryCreateShopIcon:154-172` (the basis of the trap), **but gun dealers are never routed through that function**: they are enumerated from `economy.GetGunDealers()` at `:626-638` and drawn by `TryCreateGunDealerIcon:111-134`, which loads the dedicated `"gundealer"` sprite. `#OVT-FieldManual_GunDealers_Text2`'s "Dealers and ordinary shops both carry their own icon on the map" is correct. The tip says nothing about icons either way, so nothing here depends on the resolution.
- **`economy-first-buy` body: "prices differ from town to town", never "prices move with the town".** The previous shipped wording implied a curve; the town-stock term in `GetSellPrice:549` is `(1 - (stock_level / max_stock))` with two ints, so it is a `+10%` / `+0%` step. The `Comment` now carries an explicit do-not-reintroduce note.
- **"most of them buy back" rather than "shops buy and sell".** `OVT_ShopSellRules.ShopBuysFromPlayers:30-42` refuses procurement points and vehicle shops outright, and gun dealers when the difficulty multiplier is zero. "Both buy and sell" would have been false for three cases.
- **`m_sImage ""` and `m_sFilter ""` are written out explicitly** in the new confs, per the template rule (`tutorial-system/context.md` gotcha 32). There was no in-repo precedent for an empty-string member in `Configs/`, so it was checked against the reference tree (`ArmaReforger/Configs/Buildings/DeerStand_01.conf:6`) and then proven by the Init guard loading all seven entries.
- **`proofFirstBuy.conf` was NOT brought up to the template rule.** D3 restricts the edit to body text, title text and the link key; adding `m_iMinValue` / `m_sFilter` / `m_sImage` to it would have been a fourth change. Its structure is left as shipped.

### 2026-08-09 — Feature started
- Resolved `new-player-experience/tutorial-content` inside the epic; loaded the epic overview and the two upstream contracts.
- Re-verified the plan's snapshot assumptions on this tree (R5): `6B3C…` block free, tree clean, `Configs/Tutorials/` holding only the two proof entries.
- Scaffolded `tasks.md` (24 tasks across 4 phase blocks) and this file; flipped `implementation.md` to In Progress.
- **Decided: Phase 2b is taken, not dropped.** The plan makes it droppable only if the play-test is scheduled promptly; this build defers play-testing to the user, which is the plan's own stated condition for taking it.

### 2026-08-09 — Feature closed: play-test passed, wiki edit applied

**Signed off by the user** after exporting the string table and play-testing: *"all the tips seem fine"*. See `tasks.md`'s "Needs Human Verification" header for the honest split between what was attested and what was never individually exercised (F5, F7, P10, the Learn More spot-checks).

**The first play-test was a false negative, and the diagnosis is worth keeping.** The report was "no tutorial tips at all — including `wanted-first-level`, which is new". Three pieces of evidence, gathered before proposing any cause, ruled out everything in this feature:

- `[Overthrow.Tutorial] Loaded 11 tutorial entries` in the Workbench log — the configs, `.meta` GUIDs and the nine appended prefab rows were all live. Not a data problem.
- The seen store (`profile/.save/app1874910_user76561198000167250/settings/ReforgerGameSettings.conf`) read `m_bTipsDisabled 0` with exactly two ids, `economy-first-buy` and `welcome-intro` — the two `tutorial-system` proof entries. All nine new entries were eligible. Not a seen-store or opt-out problem.
- The string export could not explain it either: an unexported tip still *renders*, showing its raw `#OVT-` key. "Nothing at all" is a different failure from "raw keys".

**Cause: Workbench was play-testing stale compiled scripts.** The user recompiled and the retest was clean. This is a known project trap and it produced a textbook false "the fix didn't work" report — the same class of error the feature spent three phases guarding against in *content*, arriving instead through the *build*. **Check what the runtime actually loaded before suspecting the work.**

**The wiki edit owed out of Phase 3 is now applied** (`getting-started`, page id 2): the tip-system paragraph after "Essential Controls", +468 characters, nothing else touched. The blocker was diagnosed rather than waited out — the MCP server's process had lost its auth, so it was issuing **unauthenticated** GraphQL calls; `pages.list`/search are public and worked, while `pages.single` returned *"You are not authorized to view this page"* at HTTP 200, which the tenacity wrapper turned into an opaque `RetryError`. Reconnecting the server fixed it.

**Three hazards found while making that one edit, all worth knowing before the next wiki write:**
1. **`wikijs_search_pages` returns wrong `pageId` values.** It reported `pageId 1` for "Wanted System"; page 1 is actually `home`, and `getting-started` is 2. **Resolve by slug and confirm the returned `path` before any update** — trusting a search id would overwrite the wrong page.
2. **`pages.update` fails unless `tags` is passed** (`Cannot read properties of undefined (reading 'map')`), and it fails *after* saving the content — so a mutation reporting `succeeded: false` may still have written. **Always re-read to establish what actually happened.** A pre-write guard asserting the inserted text was not already present is what caught this.
3. **A failed update leaves the rendered HTML stale.** The source had the new paragraph while the public page did not, until `pages.render(id:2)` was called explicitly. Checking the live page, not just the stored source, is the real verification.

The two protected `**Tutorial Jobs**` paragraphs that `starter-jobs-retirement` owns were asserted intact before the write and verified present in the rendered page afterwards.
