# Tutorial Content - Task Checklist

**Last Updated:** 2026-08-09 (Phase 3 complete: coverage mapping handed to `starter-jobs-retirement`, verification sweep green, play-test checklist written below. One wiki edit left unapplied - the wiki MCP page-read path was down, see Phase 3 report.)
**Progress:** 24/24 tasks complete (100%)

**Epic:** `new-player-experience` (feature #3 of 5) · **Plan:** `implementation.md` · **Scope truth:** `requirements.md`

> Task ids match the `<phase>.<n>` ids in `implementation.md` — do not renumber them.
> **Agent tiers are set by the plan (§4 and its closing line):** every content phase (**1**, **2**, **3**) routes to **`help-docs-sync`**; Phase **2b** routes to **`component-developer`**. **No phase is ADVANCED** — this is content and config work, not load-bearing runtime code. `/proceed` must respect the `help-docs-sync` routing rather than substituting `component-developer` on content phases.
> **Phase 2b is TAKEN, not dropped.** The plan makes it droppable only "if the play-test in §7 is scheduled promptly"; this build defers play-testing to the user, and the plan's own rule then says take it ("Take it if the guard is wanted permanently, **or if play-testing is deferred**").
> Every phase ends with `tools/compile-check.sh` clean (exit 0) and `tools/run-tests.sh "{6A6E29FF47ECB840}"` green (**47** cases — the measured baseline on this tree, not the stale count in `CLAUDE.md`). Phase 3 also runs the All group (**77-78**; the 78th is an untracked `Campaign/OVT_TEST_Campaign_TutorialSpawnTrigger.c` from a concurrent session, not a regression).
> **Never edit `Language/localization_Overthrow.<lang>.conf`** — the `.st` master only. **Never touch `Configs/Tutorials/proofWelcome.conf`** (it is `first-spawn`'s). **Zero EnforceScript** outside Phase 2b's test file.
> **Reserved GUID block: `6B3C…`** (verified 0 matches in code/config on 2026-08-08 and re-verified 2026-08-09). Allocations are tabled in `implementation.md` §3.6.

---

## Phase 1: Early-game batch — six entries (8/8 complete) — `help-docs-sync`

*Entries 1-6 of §3.1: home, money and shops (adopted), gun dealers, map and fast travel, the wanted system, skills. **Read §3.7's trap table before writing a word.***

- [x] **1.1 — Fact-check pass, before any writing**
  - Description: For each of the six topics, read the shipped field-manual strings (`#OVT-FieldManual_{YourHome,Shops,GunDealers,MapAndTravel,WantedSystem,Skills}_Text*` in the `.st`) **and** the source files in §3.2's right-hand column. Produce a one-line-per-sentence evidence note (`file:line`, or the manual string id plus its own cited source) that goes into the `Comment` fields. **A sentence with no evidence is cut, not softened.** Traps in scope: the integer-division price step, unreachable wanted levels 1 and 5, gun-dealer ammunition, the generic gun-dealer map icon, and **exactly three skills — `Trade`, `Stealth`, `Diplomacy`**.
  - File(s): (read-only) `Language/localization_Overthrow.st`, sources per §3.2
  - Estimate: 🟡 1 h

- [x] **1.2 — Adopt `economy-first-buy`**
  - Description: In `proofFirstBuy.conf` repoint `m_sFieldManualTitleKey` from `#OVT-FieldManual_MainMenu_Title` to `#OVT-FieldManual_Shops_Title`. **Do not touch `m_sId`, the file name, the resource GUID or the prefab element** (D3). Rewrite `#OVT-Tutorial_EconomyFirstBuy_Body` as the real money-and-shops entry; keep or refine `_Title`; extend the `Comment` with new evidence and **keep the existing anti-regression notes** about theft and deposits.
  - File(s): `Configs/Tutorials/proofFirstBuy.conf`, `Language/localization_Overthrow.st`
  - Estimate: 🟢 30 min

- [x] **1.3 — Author five new entry configs + `.meta`**
  - Description: `homeFirstOpen`, `shopsFirstGunDealer`, `mapFirstOpen`, `wantedFirstLevel`, `skillsFirstOpen`, copying `proofFirstBuy.conf` as the template. **Every member written out explicitly**, including ones equal to their attribute default (`tutorial-system/context.md` gotcha 32). GUIDs from §3.6. Triggers/filters exactly per §3.1 (`MENU_OPENED`+`OVT_RealEstateContext`, `PLAYER_TRANSACTION`+`SHOP_GUNDEALER`, `MAP_OPENED`, `PLAYER_WANTED` with `m_iMinValue 0` per D6, `MENU_OPENED`+`OVT_CharacterSheetContext` per D4).
  - File(s): 5 × `Configs/Tutorials/*.conf` + 5 × `.meta`
  - Estimate: 🟡 1 h

- [x] **1.4 — Add 10 string items to the `.st` master**
  - Description: 5 titles + 5 bodies, inserted alongside the existing `OVT-Tutorial_*` block so it stays contiguous. Each item: fresh `{6B3C0000000001xx}` GUID, `Target_en_us`, and a `Comment` that (a) says where the string appears and how long it may be, (b) states the tone rule, (c) **records the Rule 0 evidence from 1.1**. Titles 2-3 words; bodies **two short sentences** (a NONMODAL body wraps inside a 460 px HUD panel). No em-dashes.
  - File(s): `Language/localization_Overthrow.st` **only**
  - Estimate: 🟡 1 h

- [x] **1.5 — Append five elements to the game-mode prefab**
  - Description: Five `OVT_TutorialEntryConfig "{elemGuid}" : "{confGuid}Configs/Tutorials/<file>.conf" { }` rows appended to `OVT_TutorialManagerComponent.m_aEntries`, after the two existing rows. Append only, never reorder (R5).
  - File(s): `Prefabs/GameMode/OVT_OverthrowGameMode.et`
  - Estimate: 🟢 20 min

- [x] **1.6 — Hygiene sweep**
  - Description: duplicate-GUID grep over the new confs + prefab prints nothing; `git diff --stat Language/` shows **only** `localization_Overthrow.st`; no em-dash anywhere in authored content; balanced braces in every new `.conf`.
  - File(s): —
  - Estimate: 🟢 15 min

- [x] **1.7 — Gates: compile-check + Fast tier**
  - Description: `tools/compile-check.sh` (expect 0) and `tools/run-tests.sh "{6A6E29FF47ECB840}"` (expect 0, 47). Confirm the Init case's log line now reads **7 structurally valid entries**.
  - File(s): —
  - Estimate: 🟢 15 min

- [x] **1.8 — Report the export list**
  - Description: Report the complete list of new/changed string ids for the user's Workbench export, and note that none of the six tips will render its text until that export happens (R6).
  - File(s): (report only)
  - Estimate: 🟢 10 min

**Phase 1 acceptance:** six entries (one adopted, five new), each with exactly one §3.1 trigger, one page and a frozen-table link key · every body sentence has recorded evidence in its `Comment` · no §3.7 trap re-introduced · no imperative, no "now that you have…", no reference to another tip or to an order of play · compile-check 0, Fast 0/47, Init log 7 entries, `git diff --stat Language/` clean of `.lang.conf` · export list handed to the user.

---

## Phase 2: Mid-game batch — four entries (9/9 complete) — `help-docs-sync`

*Entries 7-10 of §3.1: recruits, placing and camps, building, base capture. Fact-check corpus is `#OVT-FieldManual_{Recruits,Camps,FOBs,BaseCapture}_Text*` plus §3.2 rows 9-12.*

- [x] **2.1 — Fact-check pass for the four topics**
  - Description: Exactly as 1.1. Traps in scope: the camp/base exclusion that is **not enforced**; building costs **money**, not supplies; capture is **not** a flag or planting mechanic; and **building happens at a held base, a FOB *or* a camp** — do not write "at a FOB" alone.
  - File(s): (read-only) `Language/localization_Overthrow.st`, sources per §3.2
  - Estimate: 🟡 45 min

- [x] **2.2 — Author four configs + `.meta`**
  - Description: `recruitsFirstRecruit`, `placeFirstPlaceable` (unfiltered `PLAYER_PLACE` per D5), `buildFirstStructure`, `basesFirstCapture`. GUIDs from §3.6; same explicit-every-member template rule as 1.3.
  - File(s): 4 × `Configs/Tutorials/*.conf` + 4 × `.meta`
  - Estimate: 🟡 45 min

- [x] **2.3 — Add 8 string items to the `.st` master**
  - Description: 4 titles + 4 bodies, same rules as 1.4 (fresh `6B3C…` GUIDs, `Target_en_us`, evidence-bearing `Comment`, 2-3 word titles, two-sentence bodies, no em-dashes).
  - File(s): `Language/localization_Overthrow.st` **only**
  - Estimate: 🟡 45 min

- [x] **2.4 — Append four elements to the game-mode prefab**
  - Description: Four more `OVT_TutorialEntryConfig` rows appended to `m_aEntries`.
  - File(s): `Prefabs/GameMode/OVT_OverthrowGameMode.et`
  - Estimate: 🟢 15 min

- [x] **2.5 — `basesFirstCapture` wording constraint**
  - Description: The body must read correctly for **either direction** of a control change and for a player who was merely nearby, because `BASE_CONTROL_CHANGE` is a 300 m proximity fan-out with no acting player (D7, R4). **No "you captured…"**, and no flag/planting mechanic (§3.7).
  - File(s): `Language/localization_Overthrow.st`
  - Estimate: 🟢 15 min

- [x] **2.6 — Hygiene + gates**
  - Description: As 1.6/1.7. Init log should now read **11 structurally valid entries**.
  - File(s): —
  - Estimate: 🟢 20 min

- [x] **2.7 — `buildFirstStructure` wording constraint**
  - Description: It fires on the first **build**, not on a FOB deploy, because no deploy event exists (§3.8). The body may explain what a FOB is; it must not read as though the player just deployed one.
  - File(s): `Language/localization_Overthrow.st`
  - Estimate: 🟢 15 min

- [x] **2.8 — Record the FOB-deploy gap in `context.md`**
  - Description: §3.8's gap written as a **non-blocking** note for `tutorial-system`, naming the `RegisterFOB` seam (`OVT_ResistanceFactionManager.c:1106-1120`). Do not file it as a blocker and do not implement it (I7).
  - File(s): `docs/features/new-player-experience/tutorial-content/context.md`
  - Estimate: 🟢 15 min

- [x] **2.9 — Report the new string ids for the export list**
  - Description: Eight more ids appended to the running export table.
  - File(s): (report only)
  - Estimate: 🟢 10 min

**Phase 2 acceptance:** ten entries total, ten distinct ids, ten link keys all present in the frozen table, no duplicate GUID anywhere · `basesFirstCapture` contains no sentence that assumes the reader acted; `buildFirstStructure` none that assumes a FOB was just deployed · same tone, evidence, compile and test criteria as Phase 1; Init log reports 11 entries.

---

## Phase 2b: Pin the enum-name filter contract (2/2 complete) — `component-developer`

*The one filter in the set produced by an engine call rather than authored: `SHOP_GUNDEALER` comes from `SCR_Enum.GetEnumName(OVT_ShopType, …)`. If that string is not literally `SHOP_GUNDEALER`, the gun-dealer tip **never fires and nothing reports it** (R2). **Taken because play-testing is deferred** — see the header note. This is the only phase that touches a `.c` file, and it touches a test file only.*

- [x] **2b.1 — Add one branch to the existing Init guard**
  - Description: In `OVT_TEST_Init_Tutorial_ManagerResolvesAndLoadsEntries`, for every authored trigger with `m_eEvent == PLAYER_TRANSACTION` and a non-empty `m_sFilter`, fail unless the filter equals `SCR_Enum.GetEnumName(OVT_ShopType, v)` for some `OVT_ShopType` value. The failure message must say the tip will silently never fire. **Strictly additive — 0 new cases.**
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c`
  - Estimate: 🟢 20 min

- [x] **2b.2 — Prove it red once**
  - Description: Temporarily break the filter (e.g. `SHOP_GUNDEALER` → `SHOP_GUNDEALERS`), capture the exact failure text, revert, and record text + date in `context.md`'s proven-red table. **No `maxAttempts`.**
  - File(s): `docs/features/new-player-experience/tutorial-content/context.md`
  - Estimate: 🟢 20 min

**Phase 2b acceptance:** the branch exists and is strictly additive (Fast case count still 47) · the proven-red record with exact failure text and date is in `context.md` · the temporary break is reverted and the tier is green.

---

## Phase 3: Coverage mapping, minimal wiki sync, verification (5/5 complete) — `help-docs-sync`

- [x] **3.1 — Starter-job coverage mapping, as-built**
  - Description: Write the **as-built** mapping (§3.5, corrected against what actually shipped) into this feature's `context.md`, and append a short pointer section to `starter-jobs-retirement/requirements.md` naming the five jobs, the covering entry ids, and the two residual gaps (marker-based discovery; recruit availability). `starter-jobs-retirement` cannot begin without this (I3).
  - File(s): `.../tutorial-content/context.md`, `.../starter-jobs-retirement/requirements.md`
  - Estimate: 🟡 30 min

- [x] **3.2 — Wiki, minimal**
  - Description: For each fact the Phase 1/2 fact-check proved a wiki page states wrongly, fix that sentence on that page and nothing else. Then add **one or two sentences** about the tip system where the wiki describes onboarding (primary candidate: `getting-started`; `home` only if it has an onboarding section). **Budget: at most 2 pages updated, 0 created** (D10).
  - File(s): wikijs MCP
  - Estimate: 🟡 30 min

- [x] **3.3 — Respect the wiki do-not-touch list**
  - Description: The `**Tutorial Jobs**` paragraph under `### 1. Jobs System` and item 6 under `## Systems Worth Knowing About` on `getting-started` belong to `starter-jobs-retirement`. Do not remove, reword or contradict them. Do not re-audit any page for staleness — `field-manual` swept them all on 2026-08-08.
  - File(s): —
  - Estimate: 🟢 10 min

- [x] **3.4 — Final verification sweep**
  - Description: `tools/compile-check.sh` (0); Fast `{6A6E29FF47ECB840}` (0); All `{6A6E2A002F53A581}` (0); `git diff --stat Language/` shows only the `.st`; repo-wide duplicate-GUID grep over the new `6B3C…` allocations; and a grep proving all ten `m_sFieldManualTitleKey` values appear verbatim in the frozen table (I2). Also confirm I6: zero diff under `Scripts/` (except the 2b test file), `UI/Layouts/`, `Configs/FieldManual/`, `Configs/System/`.
  - File(s): —
  - Estimate: 🟡 30 min

- [x] **3.5 — Write the play-test checklist into "Needs Human Verification"**
  - Description: Produce §7's checklist as a concrete, copy-pasteable list in this file, including the **seen-store reset procedure** and the **export prerequisite**. List every new string id one final time (I5).
  - File(s): `docs/features/new-player-experience/tutorial-content/tasks.md`
  - Estimate: 🟢 20 min

**Phase 3 acceptance:** coverage mapping recorded in `context.md` **and** pointed at from `starter-jobs-retirement/requirements.md` · wiki edits enumerated with page ids and a one-line reason each; no page created; the do-not-touch list intact · all four commands green; the exported string-id list handed over; the play-test checklist written down rather than described.

---

## Needs Human Verification

*Written by task 3.5, 2026-08-09. Everything below is play-test-only by project rule (UI rendering, trigger firing in a live world, whether a sentence reads well) - no automated gate can reach any of it. Work through it top to bottom; nothing here needs the plan open.*

### ⚠️ STEP 0 - PREREQUISITE: re-export the string table in Workbench

**Do this first. Nothing below is meaningful until it is done.** The nineteen strings listed here exist only in `Language/localization_Overthrow.st` (the editable master). Until Workbench regenerates the per-language runtime exports, **every new tip draws its raw `#OVT-…` key on screen instead of its text**, which looks exactly like a bug and is not one.

Never hand-edit `Language/localization_Overthrow.<lang>.conf` - regenerate them.

**Confirm it worked** by triggering one known tip (P4, opening the map, is the cheapest) and checking the popup shows words rather than `#OVT-Tutorial_MapFirstOpen_Body`.

**The 19 ids owed to the export - 18 new, 1 rewritten:**

| # | String id | GUID | State |
|---|---|---|---|
| 1 | `OVT-Tutorial_HomeFirstOpen_Title` | `{6B3C000000000101}` | NEW |
| 2 | `OVT-Tutorial_HomeFirstOpen_Body` | `{6B3C000000000102}` | NEW |
| 3 | `OVT-Tutorial_ShopsFirstGunDealer_Title` | `{6B3C000000000103}` | NEW |
| 4 | `OVT-Tutorial_ShopsFirstGunDealer_Body` | `{6B3C000000000104}` | NEW |
| 5 | `OVT-Tutorial_MapFirstOpen_Title` | `{6B3C000000000105}` | NEW |
| 6 | `OVT-Tutorial_MapFirstOpen_Body` | `{6B3C000000000106}` | NEW |
| 7 | `OVT-Tutorial_WantedFirstLevel_Title` | `{6B3C000000000107}` | NEW |
| 8 | `OVT-Tutorial_WantedFirstLevel_Body` | `{6B3C000000000108}` | NEW |
| 9 | `OVT-Tutorial_SkillsFirstOpen_Title` | `{6B3C000000000109}` | NEW |
| 10 | `OVT-Tutorial_SkillsFirstOpen_Body` | `{6B3C00000000010A}` | NEW |
| 11 | `OVT-Tutorial_RecruitsFirstRecruit_Title` | `{6B3C00000000010B}` | NEW |
| 12 | `OVT-Tutorial_RecruitsFirstRecruit_Body` | `{6B3C00000000010C}` | NEW |
| 13 | `OVT-Tutorial_PlaceFirstPlaceable_Title` | `{6B3C00000000010D}` | NEW |
| 14 | `OVT-Tutorial_PlaceFirstPlaceable_Body` | `{6B3C00000000010E}` | NEW |
| 15 | `OVT-Tutorial_BuildFirstStructure_Title` | `{6B3C00000000010F}` | NEW |
| 16 | `OVT-Tutorial_BuildFirstStructure_Body` | `{6B3C000000000110}` | NEW |
| 17 | `OVT-Tutorial_BasesFirstCapture_Title` | `{6B3C000000000111}` | NEW |
| 18 | `OVT-Tutorial_BasesFirstCapture_Body` | `{6B3C000000000112}` | NEW |
| 19 | `OVT-Tutorial_EconomyFirstBuy_Body` | `{6B3A0000000000B1}` | **REWRITTEN** |

`OVT-Tutorial_EconomyFirstBuy_Title` is unchanged and needs no attention. The ten framework chrome strings (`OVT-Tutorial_Dismiss`, `_LearnMore`, `_MoreInMenu`, and so on) shipped with `tutorial-system` and are already exported.

- [ ] Step 0 done: string table re-exported, and one tip confirmed rendering words rather than a `#OVT-` key.

---

### STEP 1 - Reset the seen store (do this before each pass, and any time you want to re-see a tip)

Every entry shows **once per machine, for ever**. Seen state is not per campaign and not per character, so a fresh campaign will **not** bring the tips back.

- Location: `$profile:.save/settings/ReforgerGameSettings.conf`, in a block named **`OVT_TutorialSettings`**.
- To re-test: clear that block. Either Workbench -> **User Settings -> Edit Game Settings**, or edit the file directly and delete the block.
- Confirm **`m_bTipsDisabled 0`** in that block (if it is `1`, tips are switched off and nothing will ever appear; the in-game toggle lives in the Tips screen).
- **An absent `m_aSeen` is the empty state, not corruption.** Do not "repair" it (`tutorial-system/context.md` gotcha 17).

- [ ] Seen store cleared and `m_bTipsDisabled 0` confirmed.

---

### STEP 2 - The ten per-entry trigger checks (P1-P10)

For each row: clear the seen store (Step 1), **perform the action once**, then **perform it a second time**. Tip must appear exactly once, showing a title and a body in words.

> **Read this before filing a bug:** tips triggered by a **menu**, by the **map**, or by a **purchase** deliberately appear **after that screen closes**, not on top of it, and land within about a second. That is the framework holding the popup back on purpose. It looks like a bug and it is not.

| # | Entry id | Action to perform | Must appear | Must NOT |
|---|---|---|---|---|
| P1 | `home-first-open` | Overthrow menu (`U` / hold D-pad down) -> **Real Estate** -> close it | after the screen closes | reappear on a second open |
| P2 | `economy-first-buy` | buy anything at any shop | after the shop menu closes | reappear on the next purchase |
| P3 | `shops-first-gun-dealer` | buy anything at a **gun dealer** | after the shop menu closes | fire at a general / clothes / food / drug / electronics / vehicle shop |
| P4 | `map-first-open` | open the map (`M`), close it | after the map closes | reappear on a second map open |
| P5 | `wanted-first-level` | get a wanted level: be seen by occupying troops while openly armed, or wound anybody | on the **first escalation**, whatever level it lands on | fire again as the level decays and rises a second time |
| P6 | `skills-first-open` | Overthrow menu -> **Character Sheet** -> close it | after the screen closes | reappear on a second open |
| P7 | `recruits-first-recruit` | use "Recruit Civilian" on a civilian | as the recruit joins your group | fire for a second recruit |
| P8 | `place-first-placeable` | Overthrow menu -> **Place** -> place a **Poster** (cheapest) | as the placement completes | fire on the next placeable, **including a Camp** (one entry covers the whole range, by design) |
| P9 | `build-first-structure` | standing at a camp, a FOB or a base the resistance holds, build any structure | as the build completes | fire on the next build |
| P10 | `bases-first-capture` | be within 300 m when a base changes hands (see below) | on the control change | fire on a **town** flip |

- [ ] P1 · [ ] P2 · [ ] P3 · [ ] P4 · [ ] P5 · [ ] P6 · [ ] P7 · [ ] P8 · [ ] P9 · [ ] P10

**P10 is the expensive one, and there is no shortcut.** A base must actually change hands. Control does **not** flip by taking a flag or planting anything: it flips purely on the **QRF point score**, which counts players against occupying soldiers inside **220 m** every ten seconds until a difficulty threshold is crossed. The procedure is:

1. Pick a base while **no QRF is running anywhere on the map** (only one runs at a time, and the capture action is not offered while another is active).
2. Use the capture action at the base to start the QRF.
3. **Clear the occupying AI inside 220 m** of the base centre.
4. **Stay inside that 220 m** until the score reaches the threshold and the base changes hands.

Two further notes: the tip is a **300 m proximity fan-out with no acting player**, so a squadmate who merely stood nearby gets it too - that is correct behaviour, and the body was written so it reads correctly for them. It also fires in **both** directions, so an occupying counter-attack that takes a base back is a valid way to see it.

**If P10 cannot be reached in the session, record it as deferred here with the reason** rather than ticking it. It is the entry least covered by the other nine passing, because it is the only one whose delivery path is the proximity fan-out rather than a per-player message.

- [ ] P10 deferred instead, reason: `________________________`

---

### STEP 3 - Link and presentation spot-checks (at least four entries, four different manual sub-categories)

**"Learn more" is not on the HUD popup.** Its only control is one input prompt labelled **"Overthrow Menu"**. Do not file a missing-button bug; the route is:

> **HUD tip -> press the "Overthrow Menu" prompt (`KC_U` on PC / gamepad `pad_down`) -> main menu -> Tips -> the tip opens as a modal -> Learn More -> the Field Manual page.**

Pressing the prompt deliberately does **not** mark the tip seen, so you still get a proper dismissal afterwards.

Suggested four, one per sub-category:

| Entry | Sub-category | "Learn More" must open |
|---|---|---|
| P1 `home-first-open` | Getting Started | **Your Home** |
| P2 `economy-first-buy` | Money and Trade | **Money and Shops** (not Main Menu - the link was repointed) |
| P5 `wanted-first-level` | Staying Hidden | **The Wanted System** |
| P8 `place-first-placeable` | The Resistance | **Camps and Placing** |

For each of the four:

- [ ] The prompt is visible on the HUD tip and reachable within its ~20 s window.
- [ ] **Tips** in the main menu shows **the same tip**, full text, with **Learn More** visible.
- [ ] **Learn More** opens the Field Manual **on that entry's page** - not the front page and not a neighbour.
- [ ] After backing out and dismissing, repeating the trigger shows nothing.
- [ ] The HUD tip never captured movement, aim or fire while it was up.
- [ ] At 1080p the text fits the panel with no clipping and no overlap with the wanted / economy / progress HUD elements.

---

### STEP 4 - One multiplayer sanity pass (one entry, not all ten)

```bash
tools/launch-server.sh --scenario eden
tools/launch-game.sh --timeout 3600 --profile OverthrowClient1 --allow-concurrent -- -client 127.0.0.1:2001
tools/launch-game.sh --timeout 3600 --profile OverthrowClient2 --allow-concurrent -- -client 127.0.0.1:2001
```

- [ ] Client A buys at a shop: **only A** sees `economy-first-buy`, and B's log does not mention the id.
- [ ] Client B then buys: B gets its own copy.

Nine of the ten entries share this per-player delivery path, so one entry demonstrates it. The exception is `bases-first-capture`, which is a documented 300 m fan-out and **may legitimately appear for both clients**.

---

### STEP 5 - Framework defects belong to `tutorial-system`, not here

⚠️ **`tutorial-system`'s own play-test is still owed.** If it has not been run by the time this checklist is worked through, **treat this first content play-test as covering both**. Anything that turns out to be a defect in the popup, the queue, the seen store, the Tips screen or the input prompt is filed as a **`tutorial-system`** bug and is **not** fixed inside `tutorial-content`. Only wrong words, wrong links and wrong triggers are this feature's.

Known-and-correct behaviours, so they are not filed twice:

- Menu-, map- and purchase-triggered tips appear **after** the screen closes.
- A first-ever purchase at a gun dealer fires **two** tips: `economy-first-buy` first, then `shops-first-gun-dealer` once the first is dismissed. They queue, they do not overlap.
- Tips do **not** come back on a new campaign or after an application restart - seen state is per machine.
- There is no **Learn More** button on the HUD popup (Step 3).

---

### Still open after Phase 3

- ⏳ **One wiki edit unapplied.** Two or three sentences describing the tip system were to be added to the `getting-started` page under **Essential Controls**. The wikijs MCP page-read path (`wikijs_get_page`, `wikijs_get_page_children`) was returning `RetryError` on 2026-08-09 while search and auth were healthy, so no page could be read and therefore none was safely edited. The exact text to paste and the insertion point are in the Phase 3 report. **No wiki page was created or modified by this feature.**
- ⏳ **Two inherited Field Manual claims reported but not corrected** (`Configs/FieldManual/` is out of this feature's scope, and both are one sentence each):
  1. `#OVT-FieldManual_WantedSystem_Text` opens by saying the occupying faction **comes looking for you**. No dispatch, search or hunt behaviour keyed to wanted level exists in code; the only wanted-driven behaviour is a perception override (`OVT_PlayerWantedComponent.CheckWanted:599-611`) plus recruit-perception branches. The tip deliberately says the narrow, verified thing instead.
  2. `#OVT-FieldManual_BaseCapture_Text` says a resistance-held base weakens the occupiers **"in that area"**. `m_iThreat` is a single **global** counter moved by plus or minus 250 in `ChangeBaseControl:895-915`, not an area-local value. The wiki's `base` page already states this correctly ("across the whole campaign").

## Notes

- **This feature adds 0 automated test cases.** The two existing Init guards already walk **every** entry (ids non-empty and unique, ≥1 page, ≥1 trigger; every non-empty `m_sFieldManualTitleKey` resolving to a real manual page), so nine additions are guarded for free (D9/I1). Phase 2b adds a *branch*, not a case.
- **Out of scope, deliberately:** late-game entries, the first-spawn welcome sequence, any framework change, removing the starter jobs, a second wiki staleness sweep, new field-manual pages.
- **The one real trigger gap** (FOB deployment has no invoker, §3.8) is recorded as a non-blocking `tutorial-system` note in task 2.8 — never worked around here.
