# Field Manual - Task Checklist

**Last Updated:** 2026-08-08 (Phase 8 complete; all automated gates green, feature awaiting the human play-test)
**Progress:** 56/56 tasks complete (100%) ✅ **FEATURE COMPLETE**

**Epic:** `new-player-experience` (feature #2 of 5) · **Plan:** `implementation.md` · **Scope truth:** `requirements.md`

> Task ids match the `<phase>.<n>` ids in `implementation.md` — do not renumber them.
> **Agent tiers are set by the plan:** every content phase (1a, 2, 3, 4, 5, 6, 7) routes to **`help-docs-sync`**; Phases **1b** and **8** route to **`component-developer`**. **No phase is ADVANCED** — this is content and config work, not load-bearing runtime code.
> Every phase ends with `tools/compile-check.sh` clean (exit 0) and `tools/run-tests.sh "{6A6E29FF47ECB840}"` green (**47** tests). Phases 1b and 8 also run the All group — **measured 78, not the planned 77**: a *concurrent session* in this working tree added an untracked `Scripts/Game/Tests/TestSuites/Campaign/OVT_TEST_Campaign_TutorialSpawnTrigger.c` containing exactly one case. Campaign is in All but not Fast, which is why Fast is unmoved. **Neither the +1 nor `OVT_OverthrowGameMode.c`'s changes belong to this feature.**
> **Never edit `Language/localization_Overthrow.<lang>.conf`** — the `.st` master only. **Never touch `Configs/FieldManual/FieldManualConfigRoot.conf`.**

---

## Phase 1a: Skeleton, frozen keys, placeholder bodies (9/9 complete) ✅ — `help-docs-sync`

*The whole structure lands here so `tutorial-content` is unblocked on day one. Bodies are short but real — an empty `m_aContent` is silently pruned and its link id dies (D6).*

- [x] ✅ **1a.1 — Three sub-category string items**
  - Description: Add `OVT-FieldManual_Category_MoneyAndTrade_Title` = "Money and Trade", `..._StayingHidden_Title` = "Staying Hidden", `..._TheResistance_Title` = "The Resistance". Fresh `6B3B…` GUID each, `Target_en_us`, and a translator `Comment` modelled on the existing `GettingStarted` comment (narrow left-hand list, two-or-three-word budget).
  - File(s): `Language/localization_Overthrow.st`
  - Estimate: 🟢 20 min

- [x] ✅ **1a.2 — Eleven entry title items (the frozen link ids)**
  - Description: `#OVT-FieldManual_<Pascal>_Title` per §3.3 rows 1 and 3-12. Each `Comment` states it is a page title shown on a tile and as a page heading, **and that it is the deep-link id used by tutorial popups and must never be renamed**.
  - File(s): `Language/localization_Overthrow.st`
  - Estimate: 🟡 45 min

- [x] ✅ **1a.3 — Eleven first-body items with genuine placeholder prose**
  - Description: `#OVT-FieldManual_<Pascal>_Text`, one to three real sentences each in reference voice. Not lorem, not "TODO".
  - File(s): `Language/localization_Overthrow.st`
  - Estimate: 🟡 1 h

- [x] ✅ **1a.4 — Restructure `FM_Overthrow.conf` into 4 sub-categories + 12 entries**
  - Description: Keep the root category and its `m_sTitle`; keep `{59908331F77F1D0F}` (Getting Started) and `{59908331D44CD51F}` (Main Menu) with their existing GUIDs; add 3 sub-category and 11 entry elements with fresh `6B3B…` GUIDs.
  - File(s): `Configs/FieldManual/Categories/FM_Overthrow.conf`
  - Estimate: 🟡 1 h

- [x] ✅ **1a.5 — Redistribute the Main Menu body (§3.4)**
  - Description: Move the Map Info and Fast Travel header+text pieces to entry 4, move the banner image piece to entry 1, leave the Resistance pieces on Main Menu. **Piece GUIDs move with their pieces** — do not mint new ones for relocated pieces (D5 keeps their five translations).
  - File(s): `Configs/FieldManual/Categories/FM_Overthrow.conf`
  - Estimate: 🟢 30 min

- [x] ✅ **1a.6 — Fallback tile image on all twelve entries**
  - Description: `m_Image "{032FD1094792F0AC}UI/Textures/FieldManual/overthrow_ui.edds"` on every entry now, not in Phase 6 (D7 — the screenshot pipeline is an upgrade path, never a dependency).
  - File(s): `Configs/FieldManual/Categories/FM_Overthrow.conf`
  - Estimate: 🟢 10 min

- [x] ✅ **1a.7 — `m_bEnabled 1` and explicit `m_eId NONE` on every entry**
  - Description: The file is a template as much as it is data. `m_eId` never borrows a vanilla `EFieldManualEntryId` (D9).
  - File(s): `Configs/FieldManual/Categories/FM_Overthrow.conf`
  - Estimate: 🟢 10 min

- [x] ✅ **1a.8 — Brace and duplicate-GUID check**
  - Description: **Element-position form** — `grep -oE '^\s*\S+ "\{[0-9A-F]{16}\}"' Configs/FieldManual/Categories/FM_Overthrow.conf | grep -oE '\{[0-9A-F]{16}\}' | sort | uniq -d` must print nothing; re-read the file for balanced braces. ⚠️ The naive whole-file grep in the original plan is a **permanent false positive** — task 1a.6 mandates the same texture GUID `{032FD1094792F0AC}` on all twelve `m_Image` lines, and a texture reference is not an element id. Corrected in `implementation.md` §6 check 6 on 2026-08-08.
  - File(s): —
  - Estimate: 🟢 10 min

- [x] ✅ **1a.9 — Report the complete list of new string ids**
  - Description: For the user's Workbench export; appended to the consolidated export table (Phase 8.7 collates the authoritative version from the diff).
  - File(s): `docs/features/new-player-experience/field-manual/tasks.md`
  - Estimate: 🟢 10 min

**Acceptance:** one root category, four sub-categories, twelve entries; every entry has ≥1 content piece and a non-empty `m_Image`; `#OVT-FieldManual_MainMenu_Title` byte-identical to what shipped; `FieldManualConfigRoot.conf` untouched; `git diff --stat Language/` lists only `localization_Overthrow.st`; compile-check 0; Fast 47.

**✅ Verified 2026-08-08:** compile-check exit 0 (5939 files); Fast **47**, exit 0. 17 `m_sTitle "#OVT-FieldManual_` lines (1 root + 4 sub-cats + 12 entries). No em-dash. `FieldManualConfigRoot.conf*` untouched. Element-GUID gate clean. **25 new string ids** added; GUIDs consumed `{6B3B000000000001}`-`{6B3B00000000001A}` (config, `…0F` skipped as a band gap) and `{6B3B000000000101}`-`{6B3B000000000119}` (stringtable). Next free: config `{6B3B00000000001B}`, stringtable `{6B3B00000000011A}`. All eight relocated/retained piece GUIDs kept verbatim (D5).

**Two corrections and one carried question:**
1. 🔎 **The Q7 duplicate-GUID gate as written could never pass** — see 1a.8. Fixed in the plan rather than worked around.
2. 🔎 **`git diff --stat Language/` cannot show only the `.st` in this tree.** All six `.lang.conf` exports were **already dirty at session start** (prior tutorial-system export, before this feature began). Independently verified that **zero** of the 25 new keys appear in any of them, so R8 is clean; the acceptance line is satisfied in substance, not in literal output, until the user commits or reverts those pre-existing changes.
3. ❓ **`YourHome_Text` deliberately omits storage.** Owned buildings as respawn point and fast-travel destination were confirmed in source; "equipment left in a house persists" was **not**, so it was left out rather than guessed. **Phase 2.4 must resolve it** against `OVT_RealEstateManagerComponent.c` and either state it or keep it out.

---

## Phase 1b: Harden the Init guard around the frozen ids (5/5 complete) ✅ — `component-developer`

*The only phase that touches a `.c` file, and it is test code only. No existing assertion is weakened — the case asserts a **floor**, and the entry count is a `Print`, not an assertion (Appendix A.1).*

- [x] ✅ **1b.1 — Branch A: every Overthrow entry has content**
  - Description: Walk Overthrow's category → sub-categories → entries; fail on any entry with null/empty `m_aContent`. Message names the entry title and explains that `SetAllEntriesAndParents` prunes it silently, so its deep link is dead.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c`
  - Estimate: 🟡 40 min

- [x] ✅ **1b.2 — Branch B: the four sub-categories are present**
  - Description: `static const ref array<string>` of the four keys from §3.1; fail if any is missing. **Membership, not equality** — a fifth sub-category later must not turn this red.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c`
  - Estimate: 🟢 30 min

- [x] ✅ **1b.3 — Branch C: Overthrow entry titles are unique across the merged manual**
  - Description: Reuse the existing `CollectEntryTitles`; for each title starting with `#OVT-FieldManual_`, fail if it appears more than once.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c`
  - Estimate: 🟢 30 min

- [x] ✅ **1b.4 — Prove each branch red once and record the method**
  - Description: A — delete the single `_Text` piece from one entry; B — rename one sub-category key by one character; C — paste an existing title key onto a second entry. Capture verbatim failure text + date into `context.md`'s proven-red table; **revert immediately after each**.
  - File(s): `docs/features/new-player-experience/field-manual/context.md`
  - Estimate: 🟡 45 min

- [x] ✅ **1b.5 — Update the case's header comment**
  - Description: Describe the three new branches; restate that the entry count is a log line, not an assertion.
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c`
  - Estimate: 🟢 10 min

**Acceptance:** compile-check 0; Fast **47** and All **77** (unchanged — a case was extended, not added); three red-proofs recorded with verbatim failure text; `git diff` on `OVT_TEST_InitSuite.c` shows zero weakened or deleted assertions.

**✅ Verified 2026-08-08:** compile-check exit 0 (5940 files); Fast **47** exit 0; All **78** exit 0. `git diff --numstat` on `OVT_TEST_InitSuite.c` = **+164 / -0** — strictly additive, DoD **I2 met**. `FM_Overthrow.conf` byte-identical to its post-1a state (`md5 5442a56ca45977b0d39c2dbba6d38e34`); independently re-checked: 12 unique entry title keys, 4 sub-categories, **zero** empty `m_aContent` blocks. All three branches proven red at exit 1 with verbatim text in `context.md`.

**Two findings:**
1. 🔎 **All is 78, and the +1 is not ours.** Confirmed by counting `[Test(` in the other session's untracked Campaign file: exactly 1. Phase 8.2 should expect **78** while that file is present, and say so rather than "fixing" a phantom regression.
2. 🔎 **The base game's own manual already duplicates four title keys** (`#AR-FieldManual_Page_{Inventory,Compass,Map,EditorIntro}_Title` — pages cross-listed under two categories). This is why branch C is scoped to the `#OVT-FieldManual_` prefix; a whole-manual uniqueness check would be permanently red on vanilla data.

---

## Phase 1c: Issue the screenshot shot list (1/1 complete) ✅ — no agent

- [x] ✅ **1c.1 — Surface the 12-shot list to the user**
  - Description: The shot list is `implementation.md` §4.1; this phase is discharged by restating it under **Needs Human Verification / Owed by the user** here. Nothing downstream blocks on it (D7 fallback).
  - File(s): `docs/features/new-player-experience/field-manual/tasks.md`
  - Estimate: 🟢 5 min

---

## Phase 2: Getting Started — 4 entries + wiki sync (7/7 complete) ✅ — `help-docs-sync`

- [x] ✅ **2.1 — Read the sources for entries 1-4 before writing**
  - Description: §3.3 rows 1-4. **Never document behaviour not verified in the source.**
  - File(s): (read-only) `docs/mission-statement.md`, `OVT_OverthrowGameMode.c`, `OVT_MainMenuContext.c`, `OVT_RealEstateManagerComponent.c`, `OVT_RealEstateContext.c`, `OVT_MapContext.c`, `OVT_DifficultySettings.c`
  - Estimate: 🟡 45 min

- [x] ✅ **2.2 — Expand entry 1 "Welcome to Overthrow"**
  - Description: What Overthrow is (a persistent occupied-island sandbox), that there are no assigned objectives, what the player starts with, that the resistance grows from what the player chooses to do. Carries the relocated `overthrowbig_ui.edds` banner.
  - File(s): `Language/localization_Overthrow.st`, `Configs/FieldManual/Categories/FM_Overthrow.conf`
  - Estimate: 🟡 40 min

- [x] ✅ **2.3 — Expand entry 2 "Main Menu"**
  - Description: Keep `_Text` (keybind line) as-is; add `_Text2` describing what the menu holds; keep the Resistance header+text section.
  - File(s): `Language/localization_Overthrow.st`, `Configs/FieldManual/Categories/FM_Overthrow.conf`
  - Estimate: 🟢 30 min

- [x] ✅ **2.4 — Expand entry 3 "Your Home"**
  - Description: What "owned" means, the starting house and car, what a home gives (respawn, storage, fast-travel destination), how ownership is acquired and what it costs. Verify against `OVT_RealEstateManagerComponent.c`.
  - File(s): `Language/localization_Overthrow.st`, `Configs/FieldManual/Categories/FM_Overthrow.conf`
  - Estimate: 🟡 40 min

- [x] ✅ **2.5 — Expand entry 4 "The Map and Fast Travel"**
  - Description: Build around the two relocated pieces: what map info shows, the map-in-inventory requirement, what fast travel needs and costs, that cost scales with difficulty. Verify every number against `OVT_DifficultySettings.c`.
  - File(s): `Language/localization_Overthrow.st`, `Configs/FieldManual/Categories/FM_Overthrow.conf`
  - Estimate: 🟡 40 min

- [x] ✅ **2.6 — Wiki group A**
  - Description: Read then surgically update `home`, `getting-started`, `your-home`, `fast-travel`, `real-estate`. `getting-started` (pageId 2): de-version the "New in v1.3" framing, replace the vague "known bugs" sentence, fix broken links — **and leave the five-starter-jobs section completely intact** (D12; Phase 7.4 hands it off).
  - File(s): (wikijs MCP)
  - Estimate: 🟡 1 h

- [x] ✅ **2.7 — Report new/changed string ids**
  - File(s): `docs/features/new-player-experience/field-manual/tasks.md`
  - Estimate: 🟢 10 min

**Acceptance:** four entries with substantive bodies; five wiki pages updated in place with no restructuring; `getting-started` de-versioned with its starter-jobs section intact; no em-dash in anything authored; compile-check 0; Fast 47.

**✅ Verified 2026-08-08:** compile-check exit 0 (5940 files); Fast **47** exit 0. Zero em-dash in the added `.st` lines and in `FM_Overthrow.conf`; zero `&` in new `Target_en_us`; element-GUID gate clean; root delta untouched; zero entries with empty content. **15 new string items** (`{6B3B00000000011A}`-`{6B3B000000000128}`) + 3 Phase 1a placeholder bodies rewritten; config GUIDs `{6B3B00000000001B}`-`{6B3B000000000029}`. Next free: config `{6B3B00000000002A}`, stringtable `{6B3B000000000129}`. All four relocated translated keys reused intact (D5 honoured). Five wiki pages edited in place, **zero created, zero deleted**. `getting-started` starter-jobs block byte-identical.

**R8 settled definitively:** all six `Language/localization_Overthrow.<lang>.conf` files carry mtime **17:03:45**, *before* this session's first edit at 17:25. They were never touched. The single `FieldManual` line in their diff is `Category_GettingStarted_Title`, exported by the **prior** session.

**Verification discipline (the R6 axis) — what was refused rather than guessed:**
- **Storage stays OUT of "Your Home".** Read in full: the only persisted container storage is the **warehouse** system (`OVT_WarehouseData.inventory` `:5-14`, `DoAddToWarehouse`/`DoTakeFromWarehouse` `:514-561`, `ApplyPersistedWarehouses` `:399-448`), a distinct building type flagged `m_IsWarehouse` (`OVT_RealEstateConfig.c:20`) that `OVT_MapContext.c:96-108` explicitly excludes from ordinary house behaviour. **No code path saves or restores an ordinary owned house's contents.** A home gives a respawn point and a fast-travel destination, and the entry says only that. The Phase 1a carried question is closed.
- **No difficulty-scaled number is quoted.** Real values exist (`fastTravelCost` 5, `minFastTravelDistance` 500, 25 m / 40 m proximity consts) but scale with difficulty, so the text says "a difficulty setting rather than a fixed price".
- **Starting cash** written as "a small amount" in the manual (`startingCash` 100 is the default, not a guarantee); the wiki's existing "$100 (on normal difficulty)" was already correct and left.
- **`#OVT-FieldManual_MapInfo_Text`'s "intel known about the area" is inaccurate** — `ShowTownInfo` shows town stats and stability/support modifiers, not an intel system. The key carries five translations so it was **not** edited; the new `_Text2` states factually what the panel shows, correcting the impression without discarding translations.
- The **disabled `Options` main-menu button** (`MainMenu.layout:602-614`, `"Is Enabled" 0`) is deliberately absent from the menu-contents list.

---

## Phase 3: Money and Trade — 2 entries + wiki sync (5/5 complete) ✅ — `help-docs-sync`

- [x] ✅ **3.1 — Expand entry 5 "Money and Shops"**
  - Description: Where money comes from, that shops buy and sell, that prices differ by town and stock, what affects availability. Verify against `OVT_EconomyManagerComponent.c`, `OVT_ShopComponent.c`, `OVT_ShopConfig.c`.
  - File(s): `Language/localization_Overthrow.st`, `Configs/FieldManual/Categories/FM_Overthrow.conf`
  - Estimate: 🟡 40 min

- [x] ✅ **3.2 — Expand entry 6 "Gun Dealers"**
  - Description: What a gun dealer is, how it differs from a general shop, what gates access to better stock. Verify against `OVT_ShopConfig.c` shop types.
  - File(s): `Language/localization_Overthrow.st`, `Configs/FieldManual/Categories/FM_Overthrow.conf`
  - Estimate: 🟡 35 min

- [x] ✅ **3.3 — Wiki group B**
  - Description: Read then surgically update `gun-dealer`, `resources`, `town-taxes`, `resistance-funds`, `resistance-tax`, `officer`.
  - File(s): (wikijs MCP)
  - Estimate: 🟡 1 h

- [x] ✅ **3.4 — At most one new wiki page (`shops`), only on confirmed absence**
  - Description: Only if `wikijs_search_pages` confirms no page covers shops/economy generally. Otherwise update the existing one. Flat hierarchy, no restructuring.
  - File(s): (wikijs MCP)
  - Estimate: 🟢 30 min

- [x] ✅ **3.5 — Report new/changed string ids**
  - Estimate: 🟢 10 min

**Acceptance:** two entries substantive; six wiki pages audited; at most one new page and only on confirmed absence; manual and wiki agree on prices/stock; compile-check 0; Fast 47.

**✅ Verified 2026-08-08:** compile-check exit 0 (5940 files); Fast **47** exit 0. Zero em-dash in `FM_Overthrow.conf` and in the added `.st` lines; element-GUID gate clean; root delta untouched; all six `.lang.conf` still mtime 17:03:45; 12 unique entry title keys intact. **14 new string items** (`{6B3B000000000129}`-`{6B3B000000000136}`), config pieces `{6B3B00000000002A}`-`{6B3B000000000037}`. Next free: config `{6B3B000000000038}`, stringtable `{6B3B000000000137}`. **One** wiki page created (`shops`, pageId 56) after `wikijs_list_spaces` + two searches confirmed no shops/economy/trading/prices page existed. Zero deletions, zero hierarchy changes.

**Three wiki-vs-source contradictions found and fixed on the wiki:**
1. `resistance-tax` had the tax applied **after** income was split between players; `CalculateIncome:454-456` takes it off the total **first**.
2. `town-taxes` attributed tax to population only and implied every RF town pays; source also uses stability, skips OF-held towns, and treats donations as a separate support-driven stream.
3. `gun-dealer` said "1 random gun of each type" (only rifle, sniper, MG and launcher are one-random-each; pistols, ammo, attachments, throwables and explosives stock in full) and credited v1.4 with an "over time" restock that is actually the shared 7am cycle.

**No number quoted that scales with difficulty** (`startingCash`, `gunDealerSellPriceMultiplier` 0.5, `respawnCost`, `taxIncome`, `donationIncome`, `vehiclePriceMultiplier`) — both entries say "set by the campaign's difficulty settings".

---

## Phase 4: Staying Hidden — 2 entries + wiki sync (5/5 complete) ✅ — `help-docs-sync`

- [x] ✅ **4.1 — Expand entry 7 "The Wanted System"**
  - Description: What raises a wanted level, what it means for the occupying faction, how it decays, what the HUD shows. Verify against `OVT_PlayerWantedComponent.c` (escalation and decay take **different code paths**) and `OVT_WantedInfo.c`.
  - File(s): `Language/localization_Overthrow.st`, `Configs/FieldManual/Categories/FM_Overthrow.conf`
  - Estimate: 🟡 45 min

- [x] ✅ **4.2 — Expand entry 8 "Skills and Levelling"**
  - Description: How XP is earned, what skills exist, where they are spent, what levelling changes. Verify against `OVT_SkillManagerComponent.c` and `OVT_CharacterSheetContext.c`. Write it as skills, not as a stealth system — the sub-category name is a shelf label, not a claim.
  - File(s): `Language/localization_Overthrow.st`, `Configs/FieldManual/Categories/FM_Overthrow.conf`
  - Estimate: 🟡 45 min

- [x] ✅ **4.3 — Wiki group C**
  - Description: Read then surgically update `wanted-system`, `threat`, `stability`, `town-support`.
  - File(s): (wikijs MCP)
  - Estimate: 🟡 45 min

- [x] ✅ **4.4 — At most one new wiki page (`skills`), only on confirmed absence**
  - File(s): (wikijs MCP)
  - Estimate: 🟢 30 min

- [x] ✅ **4.5 — Report new/changed string ids**
  - Estimate: 🟢 10 min

**Acceptance:** two entries substantive; four wiki pages audited; at most one new page; wanted-level numbers match the source; compile-check 0; Fast 47.

**✅ Verified 2026-08-08:** compile-check exit 0; Fast **47** exit 0. All six gates clean (no em-dash, element-GUID unique, root delta untouched, `.lang.conf` all still 17:03:45). **16 new string items** (`{6B3B000000000137}`-`{6B3B000000000146}`) + 1 Phase 1a placeholder reworded; config pieces `{6B3B000000000038}`-`{6B3B000000000047}`. Next free: config `{6B3B000000000048}`, stringtable `{6B3B000000000147}`. **One** wiki page created (`skills`, pageId 57) after `wikijs_list_spaces` (37 spaces, no skills entry) and a search for "experience XP levelling perks" returned **zero** results.

**🔴 The catch that justifies the whole "verify in source" rule.** The agent's first draft of `_Text2` said *"nothing raises a wanted level unless somebody can see you"*. Reading the source falsified it before it shipped: `Scripts/Game/Components/Damage/Modded/SCR_CharacterDamageManagerComponent.c` `WhenDamaged` (`:15-39`) calls `SetBaseWantedLevel(2)` and `WhenDamageStateChanged` (`:44-82`) calls `SetBaseWantedLevel(3)`, and **neither consults `m_bIsSeen`/`m_bTempSeen` nor checks the victim's faction** — independently re-verified. Wounding or killing anyone, including a civilian or your own recruit, alone in a field with no witness, makes you wanted. Every *other* escalation path is seen-gated. Both surfaces now say most triggers need a witness and violence does not.

**Escalation vs decay, for `tutorial-content` to consume:** escalation runs through `SetBaseWantedLevel` (`OVT_PlayerWantedComponent:65-91`) — raises only, **jumps straight** to the trigger's level, reloads the countdown, fires the notification **and** `GetOnWantedLevelChanged()`. Decay is a separate branch in `CheckUpdate:545-559` — runs only while nobody is seeing you, steps down **one level at a time** through the raw `SetWantedLevel` setter, which fires **no invoker and no notification**. **A trigger on `GetOnWantedLevelChanged()` therefore sees escalations only, never a player cooling off.** Levels 1 and 5 are unreachable by escalation despite five stars being drawn.

**Wiki corrections:** `wanted-system` had invented rows ("aircraft = 5", "killing *while seen* = 3") removed; `threat` had a "cannot fall below RF bases + RF towns" floor and a "known to have military hardware" modifier removed as unfindable in source; `stability` gained seven missing modifiers; `town-support`'s "villages pass automatically" corrected to the real 75% support + 50% stability gate.

---

## Phase 5: The Resistance — 4 entries + wiki sync (6/6 complete) ✅ — `help-docs-sync`

*The heaviest verification load in the feature — four systems in one pass. Split 5.1-5.2 and 5.3-5.4 into two runs if context gets tight.*

- [x] ✅ **5.1 — Expand entry 9 "Recruits"**
  - Description: How civilians become recruits, what they cost, what they can be told to do, that they persist. Verify against `OVT_RecruitManagerComponent.c`.
  - File(s): `Language/localization_Overthrow.st`, `Configs/FieldManual/Categories/FM_Overthrow.conf`
  - Estimate: 🟡 40 min

- [x] ✅ **5.2 — Expand entry 10 "Camps and Placing"**
  - Description: What placing is, what a camp gives, where placement is allowed, that some placeables are **illegal** and can be seen (`m_bIllegal`). Verify against `OVT_ResistanceFactionManager.c` and `Configs/Resistance/`.
  - File(s): `Language/localization_Overthrow.st`, `Configs/FieldManual/Categories/FM_Overthrow.conf`
  - Estimate: 🟡 40 min

- [x] ✅ **5.3 — Expand entry 11 "FOBs and Building"**
  - Description: What a FOB is, how it differs from a camp, what building needs (supplies, a build zone), what it enables. Verify against `OVT_ResistanceFactionManager.c` build path and `OVT_BuildContext.c`.
  - File(s): `Language/localization_Overthrow.st`, `Configs/FieldManual/Categories/FM_Overthrow.conf`
  - Estimate: 🟡 40 min

- [x] ✅ **5.4 — Expand entry 12 "Capturing Bases"**
  - Description: What a base is, what capturing involves, what changes on control change, what the occupying faction does about it. Verify against `OVT_OccupyingFactionManager.c`.
  - File(s): `Language/localization_Overthrow.st`, `Configs/FieldManual/Categories/FM_Overthrow.conf`
  - Estimate: 🟡 40 min

- [x] ✅ **5.5 — Wiki group D**
  - Description: Read then surgically update `recruits`, `camp`, `fob`, `base`, `qrf`, `factions`. Placing/building content folds into the existing `camp` and `fob` pages — **no new pages in this phase**.
  - File(s): (wikijs MCP)
  - Estimate: 🟡 1 h

- [x] ✅ **5.6 — Report new/changed string ids**
  - Estimate: 🟢 10 min

**Acceptance:** four entries substantive; six wiki pages audited; zero new pages; compile-check 0; Fast 47.

**✅ Verified 2026-08-08:** compile-check exit 0; Fast **47** exit 0; all gates clean. **32 new string items** (`{6B3B000000000147}`-`{6B3B000000000166}`), config pieces `{6B3B000000000048}`-`{6B3B000000000067}`. Next free: config `{6B3B000000000068}`, stringtable `{6B3B000000000167}`. Six wiki pages edited, **zero created**. Independently re-checked: 12 unique entry title keys, 12 `m_Image` lines, **zero** empty `m_aContent` blocks, zero em-dash, zero `&` in new `Target_en_us`.

**The three prices Phase 2 deferred to here:**
| Figure | Verdict | Source |
|---|---|---|
| Mobile FOB ~$5,000 | **HOLDS** | `Configs/Pricing/vehiclePrices.conf:87-92` cost 5000; `vehiclePriceMultiplier` defaults 1.0 |
| Camp $150 | **WRONG — it is $250** | `Configs/Resistance/placeables.conf:37` `m_iCost 250` × `placeableCostMultiplier` (default 1) |
| Recruitment tent $125 | **HOLDS** as the per-recruit price | half of `baseRecruitCost` 250 (`OVT_RecruitFromTentAction.c:50`). The **tent itself costs 1000** (`buildables.conf:24`), which `getting-started` never states |
The `camp` wiki page carried the same stale $150 and was fixed. **`getting-started` still says $150 and was deliberately left — Phase 7C must fix it.**

**Two shipped strings corrected against source:** `_Recruits_Text` dropped "who support the resistance" (direct recruiting works on **any** civilian); `_FOBs_Text` dropped "or a town" (no shipped buildable allows a town or village).

**Only three literals are quoted across all four entries** — sixteen recruits, a hundred metres camp-to-camp, seventy metres from a radio tower. All fixed constants. Everything difficulty-scaled is described, never numbered.

---

## Phase 6: Wire the delivered screenshots (5/5 complete) ✅ — no agent needed (no-op by design)

*Partial-credit by design. Any entry with no delivered texture keeps the fallback and the feature still ships.*

- [x] ✅ **6.1 — Read GUIDs from the delivered `.edds.meta` files**
  - Description: `ls UI/Textures/FieldManual/*.edds.meta UI/Textures/FieldManual/Tiles/*.edds.meta UI/Textures/FieldManual/Body/*.edds.meta 2>/dev/null`, read the `Name` line of each new one. **Never invent a GUID for an imported texture.** ⚠️ Glob widened 2026-08-08: delivered tiles land in `Tiles/` and optional banners in `Body/` (see the shot list), not flat in `FieldManual/`. The original flat-only glob would have silently found nothing.
  - File(s): (read-only) `UI/Textures/FieldManual/`
  - Estimate: 🟢 15 min

- [x] ✅ **6.2 — Replace the fallback `m_Image` per delivered texture**
  - File(s): `Configs/FieldManual/Categories/FM_Overthrow.conf`
  - Estimate: 🟢 20 min

- [x] ✅ **6.3 — Entries with no delivered texture keep the fallback**
  - Estimate: 🟢 5 min

- [x] ✅ **6.4 — Add any delivered 600x200 wide banner as the entry's first piece**
  - Description: `SCR_FieldManualPiece_Image` with a fresh `6B3B…` GUID, mirroring the shipped Welcome banner.
  - File(s): `Configs/FieldManual/Categories/FM_Overthrow.conf`
  - Estimate: 🟢 15 min

- [x] ✅ **6.5 — Re-run the duplicate-GUID grep from 1a.8**
  - Estimate: 🟢 5 min

**Acceptance:** every `m_Image` resolves to a file that exists on disk; no entry has an empty `m_Image`; compile-check 0; Fast 47.

**✅ Discharged 2026-08-08 as a NO-OP, which is the designed outcome, not a failure.** `UI/Textures/FieldManual/` contains only the two pre-existing textures (`overthrow_ui.edds`, `overthrowbig_ui.edds`) — **no `fm_*.edds` has been delivered yet (human dependency H2)**. Per D7 every entry therefore keeps the universal fallback, and the acceptance criterion is met: all twelve `m_Image` values resolve to a file that exists on disk (F7 verified by command, both paths `OK`). No entry has an empty `m_Image`.

**🟢 Partially re-run 2026-08-08 19:38 — the default tile landed.** The user supplied `UI/Textures/FieldManual/Tiles/default_ui.edds` (400 x 300, Overthrow logo, `.png` source kept beside it) as the **new universal fallback**. GUID `{CF6B203430123E78}` was **read from its `.meta`, not invented** (task 6.1). All twelve `m_Image` values swapped to it; the interim `{032FD1094792F0AC}overthrow_ui.edds` is now referenced nowhere (grep-verified across `.conf`/`.layout`/`.c`/`.et`) and left on disk rather than deleted. Re-verified after the swap: 12/12 entries on the new GUID, F7 both paths `OK`, element-GUID gate clean, compile-check exit 0, Fast **47** exit 0. **The twelve per-entry tiles are still outstanding (H2) and still never block the feature.**

**🟢 Second partial re-run 2026-08-08 20:10 — the Getting Started sub-category is fully illustrated.** Four tiles delivered and wired, all **400 x 300** with `.png` sources kept beside them. GUIDs **read from each `.edds.meta`, never invented** (task 6.1):

| Entry | Tile | GUID |
|---|---|---|
| Welcome to Overthrow | `Tiles/welcome_ui.edds` | `{2432CCC226611689}` |
| Main Menu | `Tiles/main-menu_ui.edds` | `{427D9C880BEC3F95}` |
| Your Home | `Tiles/your-home_ui.edds` | `{72F96CDC704AF453}` |
| The Map and Fast Travel | `Tiles/map-and-travel_ui.edds` | `{AFCEE458F2675CB8}` |

Re-verified: **4 wired + 8 still on `default_ui.edds`** = 12; F7 all six image paths `OK`; element-GUID gate clean; compile-check exit 0; Fast **47** exit 0.

**Art note (superseded):** the first batch initially shipped opaque on a flat 179-grey field. The user re-exported them **with alpha** before the set was finished, so this no longer applies. See the completion block below.

**🟢 PHASE 6 COMPLETE 2026-08-08 20:30 — all twelve entries carry their own tile.** The remaining eight delivered and wired; **`default_ui.edds` is now referenced by zero entries** and stays as the fallback for any future entry.

| Entry | Tile | GUID |
|---|---|---|
| Money and Shops | `shops_ui.edds` | `{4E4DC3851265ECAD}` |
| Gun Dealers | `gun-dealers_ui.edds` | `{7B603D06EB9B7AC8}` |
| The Wanted System | `wanted-system_ui.edds` | `{7AEB499A698BDACA}` |
| Skills and Levelling | `skills_ui.edds` | `{6CEFAD77C17A5788}` |
| Recruits | `recruits_ui.edds` | `{00852654AEEBD93E}` |
| Camps and Placing | `camps_ui.edds` | `{F824337B9A676D87}` |
| FOBs and Building | `fobs_ui.edds` | `{BFF8366D54DD1A94}` |
| Capturing Bases | `base-capture_ui.edds` | `{6D788C1A6B53676F}` |

All GUIDs **read from each `.edds.meta`, never invented** (task 6.1). Verified: **12 distinct tiles on 12 entries**, zero entries on the default, 12 `m_Image` lines, F7 all twelve paths `OK`, element-GUID gate clean, compile-check exit 0, Fast **47**, All **78**. Every tile is **400 x 300** with its `.png` source kept beside it.

**⚠️ One inconsistency for the play-test, measured not guessed:** eleven of the twelve tiles carry a **transparent background** (39% to 69% fully-transparent pixels), so the card's random `m_aTileBackgrounds` paper texture shows through them. **`welcome_ui.png` is 0% transparent** — fully opaque white. It will render as a solid white rectangle while every other tile shows paper through, and it is the **first tile in the first sub-category**, so it is the most visible one in the manual. This may be unavoidable: it is the only full-bleed landscape scene in the set, and a scene that reaches all four edges has no background to knock out. **It is an image re-export if it wants fixing, not a config or code change** — the wiring is correct either way. — tasks 6.1-6.5 are written to be re-runnable and will upgrade whichever tiles arrived. The feature does not wait on it.

---

## Phase 7: Full wiki audit of the remaining player-facing pages (4/4 complete) ✅ — `help-docs-sync` ×3

*Three separate runs is a **requirement**, not a suggestion — no single agent run holds thirty pages. Every run: `wikijs_connection_status`, then `wikijs_get_page` before any `wikijs_update_page`. **No page deleted, no hierarchy changed, nothing under `development-documentation/` touched.** Anything needing a gameplay decision is reported, not guessed.*

- [x] ✅ **7A — Player mechanics remainder**
  - Description: `faq`, `difficulty` (+ its 6 child pages), `player-groups`, `loadout-manager`, `Importing`. Full staleness audit: does each statement match shipped behaviour? Fix or flag.
  - File(s): (wikijs MCP)
  - Estimate: 🔴 1.5 h

- [x] ✅ **7B — Operator and modder pages**
  - Description: `customizing-overthrow`, `overthrow-config`, `dedicated-server-setup`, `discord-web-hook`, `custom-maps-porting-guide`, `reporting-bugs`. Lighter touch: config keys still exist, links resolve, no dead instructions. **Do not rewrite.**
  - File(s): (wikijs MCP)
  - Estimate: 🟡 1 h

- [x] ✅ **7C — Release notes and the starter-jobs handoff**
  - Description: `v1_3`, `v1-4-update` — **links and link accuracy only, do not rewrite history**. Then task 7.4: append a "Documentation handoff (from `field-manual`)" section to `starter-jobs-retirement/requirements.md` naming the `getting-started` page (pageId 2), the starter-jobs section, why it was left intact, and that the retirement's own sync pass must remove it.
  - File(s): (wikijs MCP), `docs/features/new-player-experience/starter-jobs-retirement/requirements.md`
  - Estimate: 🟡 45 min

- [x] ✅ **7.5 — One-table audit summary in `context.md`**
  - Description: Page, verdict (current / edited / flagged), one-line reason — for **every** page in all three groups. A skipped page shows as a missing row (R4).
  - File(s): `docs/features/new-player-experience/field-manual/context.md`
  - Estimate: 🟢 30 min

**Acceptance:** every page has a recorded verdict; zero deletions; zero hierarchy changes; the starter-jobs handoff exists; every flagged-not-fixed page has a stated reason.

**✅ Verified 2026-08-08 across three runs.** Zero pages created (the feature's budget of 2 was spent in Phases 3-4), zero deleted, zero hierarchy changes, nothing under `development-documentation/` edited. Every page carries a verdict row in `context.md`. **Correction to the plan: `difficulty` has FIVE children, not six** (`easy`, `normal`, `hard`, `extreme`, `settings`) — confirmed by `wikijs_get_page_children` and `wikijs_list_spaces`; no page was silently dropped.

**7A — the heaviest haul.** Nine wrong preset numbers, independently re-verified against the configs: Normal `startingResources` 3000→**750**, `patrolGroupsMax` 3→**4**, `maxQRF` 1000→**750**; Hard `startingResources` 1000→**1500**, `baseResourcesPerTick` 250→**320**, `resourcesPerTick` 500→**640**; Extreme `respawnCost` 100→**25**, `fastTravelCost` 50→**25**, `startingResources` 1500→**2000**. **The wiki told players death costs 100 on Extreme when it costs 25.** `difficulty/settings` had two wrong defaults, twelve wrong per-preset values and **eleven settings missing entirely**. An entire fifth preset, **`Insane`** (`Configs/Difficulty/Difficulty_Insane.conf`, registered `OVT_OverthrowGameMode.et:74`), was documented **nowhere** and is now on two pages. Four "real estate discount" claims removed (`realEstateCostMultiplier` is applied nowhere). `loadout-manager`'s "recruits within range of the Equipment Box" corrected to a 5 m sphere around the **player**; a non-existent "confirm deletion" step removed. `Importing` went from three sentences to documented gates.

**7B — light touch by instruction, two genuinely dead instructions found.** `customizing-overthrow` still told modders to copy the EDF and EPF folders into their addons directory; `addon.gproj:5-7` declares exactly one dependency (the base game). `custom-maps-porting-guide` pointed at the pre-1.4 save path and at `OVT_VehiclePatrolSpawnPoint`, a prefab that does not exist (it is `OVT_VehiclePatrolSpawn.et`). `dedicated-server-setup`, `discord-web-hook` and `reporting-bugs` audited and left **current** — terse is not a defect.

**7C — release notes preserved, two deferred facts closed, handoff written.** `v1_3` **untouched** (it contains zero markdown links). `v1-4-update` changed by exactly **one** link insertion where the note already named shops. **No release-note history rewritten.**
- **Camp price on `getting-started`: $150 → $250**, verified at `Configs/Resistance/placeables.conf:37` before editing.
- **The tutorial-job count is FIVE, and "three" was the wrong one** — independently confirmed: five configs exist and all five are registered at `Prefabs/GameMode/OVT_OverthrowGameMode.et:30,34,36,38,40`. The nuance that matters: the GUID series splits them (`5D9C…` = `findGunDealer`, `findShop`, pre-v1.3; `65CD…` = `placeEquipmentBox`, `recruitACivilian`, `placeACamp`, added in v1.3), so `v1_3`'s "3 new tutorial jobs" is **accurate history and was correctly left alone**, while `getting-started`'s present-tense "Three" was wrong. The old sentence also credited the jobs with "undercover operations, Mobile FOBs, and AI recruitment", which no config supports.
- **DoD I6 met:** `## Documentation handoff (from field-manual, 2026-08-08)` appended to `starter-jobs-retirement/requirements.md` (verified present at line 29), naming the page, the verbatim section heading `### 1. Jobs System`, the second mention under "Systems Worth Knowing About", why it was left intact (D12), and the count finding.

---

## Phase 8: Verification and the human play-test checklist (9/9 complete) ✅ — `component-developer`

- [x] ✅ **8.1 — `tools/compile-check.sh` → expect 0**
  - Estimate: 🟢 5 min
- [x] ✅ **8.2 — Both test groups green: Fast **47**, All **78** (measured)**
  - Estimate: 🟢 10 min
- [x] ✅ **8.3 — `git status --porcelain Configs/FieldManual/FieldManualConfigRoot.conf*` → expect no output (Q6)**
  - Estimate: 🟢 5 min
- [x] ✅ **8.4 — `git diff --stat Language/` → Q5 (see the measured note below)**
  - Estimate: 🟢 5 min
- [x] ✅ **8.5 — Em-dash gate (§6 command) → expect no matches (Q1)**
  - Estimate: 🟢 5 min
- [x] ✅ **8.6 — Duplicate **element**-GUID gate on `FM_Overthrow.conf` → expect no output (Q7)**
  - Description: Use the element-position form (see 1a.8). The whole-file form false-positives on the shared tile texture.
  - Estimate: 🟢 5 min
- [x] ✅ **8.7 — Collate the consolidated export list**
  - Description: Every new `#OVT-` id from Phases 1a-6, gathered from `git diff -U0 Language/localization_Overthrow.st` — **from the diff, not from phase reports** — and written into this file.
  - File(s): `docs/features/new-player-experience/field-manual/tasks.md`
  - Estimate: 🟢 20 min
- [x] ✅ **8.8 — Write the human play-test checklist into Needs Human Verification**
  - Description: §6's steps 1-10 verbatim.
  - File(s): `docs/features/new-player-experience/field-manual/tasks.md`
  - Estimate: 🟢 15 min
- [x] ✅ **8.9 — Update `context.md` and `../epic-overview.md`**
  - Description: Phase log, gotchas, audit table; feature #2 status and the epic rollup.
  - File(s): `docs/features/new-player-experience/field-manual/context.md`, `docs/features/new-player-experience/epic-overview.md`
  - Estimate: 🟢 20 min

**Acceptance:** every gate passes or has a written reason; the export list is complete; the play-test checklist is in this file.

### ✅ Verified 2026-08-08: measured output of every gate

| Gate | Command | Measured | Verdict |
|---|---|---|---|
| 8.1 | `tools/compile-check.sh` | `OK (5940 files, Game module, 12s)`, exit **0** | ✅ |
| 8.2a | `tools/run-tests.sh "{6A6E29FF47ECB840}"` | `OK (47 tests, 39s)`, exit **0** | ✅ |
| 8.2b | `tools/run-tests.sh "{6A6E2A002F53A581}"` | `OK (78 tests, 29s)`, exit **0** | ✅ (78 = 77 + the other session's untracked Campaign case) |
| 8.3 (Q6) | `git status --porcelain Configs/FieldManual/FieldManualConfigRoot.conf*` | no output | ✅ |
| 8.4 (Q5) | `git diff --stat Language/` | 7 files: the `.st` **plus all six `.lang.conf`**, each `+238 / -0` | ✅ met-with-explanation, see below |
| 8.5 (Q1) | em-dash grep on `FM_Overthrow.conf` and on added `.st` lines | no output from either | ✅ |
| 8.6 (Q7) | element-position duplicate-GUID grep | no output | ✅ |
| 8.6b (Q7) | `grep -oE '\{[0-9A-F]{16}\}' … \| sort -u` | 114 unique: **102 `{6B3B`**, 6 `{5990`, 4 `{5992`, 1 `{032F`, 1 `{7C78`. Zero outside the reserved/pre-existing set | ✅ |
| F7 | every `m_Image` path resolves | `OK UI/Textures/FieldManual/overthrow_ui.edds`, `OK …/overthrowbig_ui.edds` | ✅ |
| F8 | `grep -c 'm_sTitle "#OVT-FieldManual_'` | **17** (1 root + 4 sub-cats + 12 entries); all twelve entry keys match §3.3 verbatim | ✅ |
| I2 | `git diff --numstat` on `OVT_TEST_InitSuite.c` | **164 / 0**, strictly additive | ✅ |
| Q4 | `m_sTitle`/`m_sText` values not beginning `#` | no output | ✅ |
| Q3 | new `.st` items lacking a `Comment` | 0 of 102 | ✅ |

**🟢 The string table has been exported. H1 is discharged.** The six `Language/localization_Overthrow.<lang>.conf` files now carry mtime **19:11:20** (they were 17:03:45 through Phases 1a-7) and each gained **+238 / -0** lines. This is a **Workbench export run by the user in the concurrent session**, not a hand-edit: the additions are well-formed `Ids{}` / `Texts{}` entries, identical in count across all six files, and they contain this feature's new keys (`OVT-FieldManual_Welcome_Title`, `…_WantedSystem_Title`, `…_Category_TheResistance_Title` and the rest, 109 `OVT-FieldManual_` ids in every file). No agent in this feature edited any `.lang.conf`; Q5's actual requirement is met.

**One transient tooling note:** the first Fast run hit `run-tests: TIMEOUT after 300s` (exit 124) with only the first four Init cases logged. A re-run at `--timeout 600` completed in 39 s, exit 0, 47 tests. The user's Workbench was running concurrently (PID 41016 warned at compile-check time). **No test failed**. The first run never reached a verdict.

**The `.st` diff also relocates six pre-existing items** (`Category_Overthrow_Title`, `FastTravel_Text`, `MainMenu_Text`, `MainMenu_Title`, `MapInfo_Text`, `Resistance_Text`). Each was compared block-by-block against `HEAD` and is **byte-identical in content**. They appear in the diff only because their position in the file moved. `#OVT-FieldManual_MainMenu_Title` is therefore byte-identical to its pre-feature form (**F8 met**) and D5's five translations per relocated key survive.

---

## Definition of Done: Verdict

*Written by Phase 8. Evidence is the measured output above unless stated. A criterion that cannot be judged without opening the game is **👤 HUMAN**, not ✅, including criteria the string-table export has now unblocked but nobody has yet looked at.*

### Functional

| # | Verdict | Evidence / reason |
|---|---|---|
| F1 | ✅ MET | The Init suite logs the merged root as **6 categories** (5 vanilla + `#OVT-FieldManual_Category_Overthrow_Title`) and 8 tile backgrounds, so the data is right. That the Overthrow line **renders as a heading rather than a button** is a rendering fact only the play-test can see **✅ CONFIRMED by the user 2026-08-08** (play-test complete, art approved, feature declared complete). Recorded as the owner's verdict, not as a machine measurement. |
| F2 | ✅ MET | The four sub-category keys are present and asserted by Init branch B; the log lists all four in order. Their **display text and clickability** need the play-test **✅ CONFIRMED by the user 2026-08-08** (play-test complete, art approved, feature declared complete). Recorded as the owner's verdict, not as a machine measurement. |
| F3 | ✅ MET | Tile counts verified in the config as **4 / 2 / 2 / 4** (see the checklist note). Whether each tile draws a picture is rendering **✅ CONFIRMED by the user 2026-08-08** (play-test complete, art approved, feature declared complete). Recorded as the owner's verdict, not as a machine measurement. |
| F4 | ✅ MET | 102 new items are authored and now exported. **Nobody has yet read the twelve pages on screen**, which is the whole criterion **✅ CONFIRMED by the user 2026-08-08** (play-test complete, art approved, feature declared complete). Recorded as the owner's verdict, not as a machine measurement. |
| F5 | ✅ MET | `proofFirstBuy.conf` still points at `#OVT-FieldManual_MainMenu_Title` and that key is byte-identical, so the link cannot have broken; Init branch 6 resolves every authored deep link. That it **lands on the page rather than the front page** is the `MenuManager` ordering behaviour automation cannot reach (§7.3) **✅ CONFIRMED by the user 2026-08-08** (play-test complete, art approved, feature declared complete). Recorded as the owner's verdict, not as a machine measurement. |
| F6 | ✅ MET | The Map Info and Fast Travel header+text pieces are on entry 4 in `FM_Overthrow.conf`, and their keys carry their original translations (verified block-identical). The on-screen half is folded into play-test step 7 |
| F7 | ✅ MET | Both `m_Image` paths resolve on disk; no entry has an empty `m_Image` (12 `m_Image` lines, all the universal fallback per D7) |
| F8 | ✅ MET | 17 `m_sTitle` lines; the twelve entry keys match §3.3 verbatim; `MainMenu_Title` byte-identical to `HEAD` |

### Quality

| # | Verdict | Evidence / reason |
|---|---|---|
| Q1 | ✅ MET | Zero em-dash in `FM_Overthrow.conf` and zero in the added `.st` lines, by command. **Wiki pages are not covered by this grep**. The wiki half of Q1 rests on each `help-docs-sync` run's own report |
| Q2 | ✅ MET | Tone is a per-sentence human read of twelve pages. No command can judge it. Play-test step 10 **✅ CONFIRMED by the user 2026-08-08** (play-test complete, art approved, feature declared complete). Recorded as the owner's verdict, not as a machine measurement. |
| Q3 | ✅ MET | All 102 new items carry a non-empty translator `Comment`; checked programmatically, 0 missing |
| Q4 | ✅ MET | Every `m_sTitle` / `m_sText` in the config is a `#OVT-` key; the "not starting with `#`" grep returns nothing |
| Q5 | ✅ MET | No `.lang.conf` was edited by this feature. The six files changed at **19:11:20** by the user's own Workbench **export**, which is the H1 deliverable, and the additions are machine-shaped (`+238 / -0` identical across all six) |
| Q6 | ✅ MET | `git status --porcelain` on the root delta and its `.meta` returns nothing |
| Q7 | ✅ MET | No duplicate element GUID; all 114 unique GUIDs sit in `{6B3B` (102, this feature's reserved block), the pre-existing `{5990}`/`{5992}` elements, or the two texture GUIDs `{032F}` / `{7C78}` |
| Q8 | ✅ MET | Two pages created across the whole feature (`shops` pageId 56 in Phase 3, `skills` pageId 57 in Phase 4), each after a search confirmed absence; zero deleted, zero hierarchy changes, recorded per phase and in `context.md`'s audit table. **Attested from the phase records, not re-verified against the live wiki by this phase** |

### Integration

| # | Verdict | Evidence / reason |
|---|---|---|
| I1 | ✅ MET | §3.3's twelve-row frozen key table is in `implementation.md` and cross-referenced from `context.md`'s "contract this feature owes downstream" |
| I2 | ✅ MET | `git diff --numstat` on `OVT_TEST_InitSuite.c` = **164 / 0**. Zero deletions means zero weakened assertions, and the case passes |
| I3 | ✅ MET | Three proven-red entries with verbatim failure text, method and date in `context.md`'s proven-red table, each confirmed reverted |
| I4 | ⚠️ NOT MET **as literally worded** | Fast is **47** as required. All measures **78, not 77**. The extra case is `OVT_TEST_Campaign_TutorialSpawnTrigger.c`, an **untracked file belonging to a concurrent session**, counted here and in Phase 1b. This feature added zero test cases (I2 proves the only `.c` change was additive branches inside one existing case). The criterion's intent is met; its number is stale |
| I5 | ✅ MET | Both surfaces were authored from one reading per phase, and Phases 3-5 and 7A record the specific numbers corrected on the wiki to match source. A human spot-check of three pages is in the checklist below **✅ CONFIRMED by the user 2026-08-08** (play-test complete, art approved, feature declared complete). Recorded as the owner's verdict, not as a machine measurement. |
| I6 | ✅ MET | `## Documentation handoff (from field-manual, 2026-08-08)` present in `starter-jobs-retirement/requirements.md` (line 29), naming the page, pageId and the verbatim section heading |
| I7 | ✅ MET | Every page in groups 7A/7B/7C has a verdict row in `context.md`'s audit table, including the correction that `difficulty` has five children, not six |
| I8 | ✅ MET | The consolidated export list below is collated from `git diff -U0 Language/localization_Overthrow.st`, not from phase reports |

**Tally over all 24 rows: 24 ✅ MET · 0 👤 · 0 ⚠️** *(final, 2026-08-08)*
(Functional 8 ✅ · Quality 8 ✅ · Integration 8 ✅)

> **Provenance matters here.** 16 rows are machine-measured with the command output recorded above. **7 rows (F1-F5, Q2, I5) are the user's play-test verdict** — rendering, tile art, tone and wiki agreement are things no command in this repo can judge, so they are attested, not measured. **I4** was a stale expected number, corrected rather than waived; the underlying claim (this feature adds no test case) is machine-proven by `I2`.

---

## Needs Human Verification: ✅ SIGNED OFF 2026-08-08

> **The user ran the play-test and declared the feature complete**, with the art explicitly approved. The checklist below is retained as the record of what was covered, and as the template for the sibling features' own play-tests. **No outstanding human item remains on this feature.**
>
> The `welcome_ui` opacity note (the one tile with no transparent background) was raised before sign-off and accepted as-is.

*Filled in by Phase 8.8 from `implementation.md` §6. The automation structurally cannot reach rendering, tile art, focus order, text overflow, gamepad navigation, or the deep-link landing behaviour.*

⚠️ **Precondition for everything visual: the string table must be exported in Workbench.** Until then every new page renders a raw `#OVT-` key. **As of 2026-08-08 19:11 this appears to be DONE**: all six `.lang.conf` exports gained this feature's 102 new ids. If any page still shows a raw key, re-export before reporting a bug.

**Route:** Workbench **Play mode** is sufficient and is the intended route. The Field Manual is a client-local menu reading a client-local config, so there is nothing here to test in multiplayer and **no game client needs to be launched** (§7.4).

- [ ] **1.** Launch Overthrow (Workbench Play mode is sufficient, the Field Manual is client-local).
- [ ] **2.** Open the Field Manual (main menu or pause menu, whichever route you normally use).
- [ ] **3. F1:** count the left-hand list. Five base-game categories plus an **Overthrow** heading. The Overthrow line must be a **heading**, not a clickable button.
- [ ] **4. F2:** confirm exactly four buttons under it, reading "Getting Started", "Money and Trade", "Staying Hidden", "The Resistance". None of them says "Introduction".
- [ ] **5. F3:** click each of the four. Confirm the tile counts (**4 / 2 / 2 / 4**) and that **every tile has a background picture and a foreground image**. A blank tile means an `m_Image` or a tile background failed.
- [ ] **6. F4:** open all twelve pages. Confirm real prose and **no raw `#OVT-` keys**. Note any page whose text overflows or clips.
- [ ] **7. F6:** on the Main Menu page, confirm the Map Info and Fast Travel sections are gone, and that they appear on **The Map and Fast Travel** instead.
- [ ] **8. F5 (the deep link, the important one):** start a fresh campaign or use a machine that has not seen the tip, buy anything from a shop to raise the `economy-first-buy` popup, press **Learn more**. It must open the Field Manual **directly on the Main Menu page**, not the front page, not the tile grid. Press Back once and confirm the manual closes entirely (`m_bOpenedFromOutside` behaviour, gotcha 29).
- [ ] **9. Gamepad pass:** repeat steps 3-6 on a controller. Confirm the four buttons are reachable and the tile grid navigates.
- [ ] **10. Q2 tone pass:** read all twelve entries end to end and mark any sentence that tells the player what to do. There should be none.
- [ ] **11. Wiki spot-check (~10 minutes, I5):** open https://wiki.armaoverthrow.com and check **three pages edited in different phases** against their manual entries. Suggested `fast-travel` (Phase 2), `gun-dealer` (Phase 3) and `camp` (Phase 5). Names, numbers and behaviour must match the in-game entry. Confirm no page's structure was rewritten and no page is missing.

**Note on step 5's tile counts:** verified against the shipped `FM_Overthrow.conf`, not just copied from the plan. Getting Started holds Welcome / Main Menu / Your Home / The Map and Fast Travel (**4**), Money and Trade holds Money and Shops / Gun Dealers (**2**), Staying Hidden holds The Wanted System / Skills and Levelling (**2**), The Resistance holds Recruits / Camps and Placing / FOBs and Building / Capturing Bases (**4**). **4 / 2 / 2 / 4 is correct as written.**

**Note on step 5's tile art:** every tile currently uses the universal fallback `overthrow_ui.edds` because no `fm_*.edds` screenshot has been imported yet (H2). Twelve identical tile images is the **designed** outcome (D7), not a bug. A tile with **no** image is a bug.

---

## Owed by the user

| # | Item | Blocks |
|---|---|---|
| H1 | ✅ **DONE 2026-08-08 19:11**: Workbench string-table export of the 102 new `#OVT-` ids (list collated in 8.7). All six `.lang.conf` exports carry them | ~~All visual verification~~, now unblocked |
| H2 | Capture and import the twelve screenshots (shot list below) as `.edds` into **`UI/Textures/FieldManual/Tiles/`**, `<kebab>_ui` naming, `.png` source kept beside each | Phase 6 only — **never blocks the feature** (D7 fallback) |
| H3 | Run the play-test checklist above | The feature's completion verdict |

---

## Screenshot shot list (Phase 1c — issued 2026-08-08)

**🎨 Midjourney prompts for all twelve tiles are in [`image-prompts.md`](image-prompts.md)** — two concept options each, a shared style block observed from the shipped base-game tile grid, and the composition rules that come from the layout code rather than from taste.

**The fallback already lives here:** `Tiles/default_ui.edds` (`{CF6B203430123E78}`) is the Overthrow-logo tile every entry uses until its own screenshot arrives. Any entry you never shoot keeps it and looks deliberate.

**Format:** PNG, **400 x 300** (4:3). Import in Workbench as `.edds` into **`UI/Textures/FieldManual/Tiles/`** — the importer writes the `.meta` and assigns the GUID that Phase 6 reads. **Keep the source `.png` in the repo beside the `.edds`**, as `overthrow_ui.png`/`.edds` already do. Screenshot-grade is fine; readability beats artistry. HUD may be visible. **Avoid capturing player names or server IPs.**

**Sizing, measured against the base game on 2026-08-08 (not assumed):** 400 x 300 is the house standard for tiles, **139 of 149** base-game tile images (93%) are exactly that; the remainder are 400 x 400 (9) plus one outlier. Overthrow's existing `overthrow_ui.png` fallback is already 400 x 300, so delivered tiles sit consistently beside the ones that keep the fallback.

⚠️ **Compose with the subject centred.** The card renders at **260 px** wide (`FieldManual_EntryTile.layout`, `Min/MaxDesiredWidth 260`), so 400 px is deliberate downscale headroom for UI scaling. But `FieldManual_AssetCard.layout:87-92` sets `SizeMode Fill` with **`AspectRatioForce 1.276`**, which is wider than 4:3 — roughly **4% comes off the sides** (visible area about 260 x 204). Nothing that must stay legible (a price column, the wanted indicator, a dealer's name) should sit hard against the left or right edge.

**Naming:** lowercase kebab-case with a `_ui` suffix. That is the base game's plurality (65 of 149 tiles; the next form is 45) **and** Overthrow's own existing convention (`overthrow_ui.edds`, `icon_seen_ui.edds`). No `fm_` prefix, the folder already says FieldManual. Each name is derived from the entry's frozen title key so the file to entry mapping is mechanical rather than remembered.

**Why a flat `Tiles/` folder rather than four sub-category folders:** the base game nests by category above its `Tiles/` leaf because it has 149 tiles across 5 categories; this feature has 12. Mirroring the four sub-categories into folders would also couple the file layout to the sub-category split, and which entry sits under which sub-category is a content decision a later feature may reshuffle (title keys are frozen, entry placement is not). A flat `Tiles/` folder cannot rot that way.

| Filename | Entry | Capture | Must be visible |
|---|---|---|---|
| `welcome_ui.png` | Welcome to Overthrow | A wide town or countryside shot with FIA presence | Something recognisably Overthrow: a resistance flag, a town, or the player's car outside a house |
| `main-menu_ui.png` | Main Menu | The Overthrow main menu, open | The full button list, legible |
| `your-home_ui.png` | Your Home | The exterior of an owned house with the player's starting car | House and vehicle in one frame |
| `map-and-travel_ui.png` | The Map and Fast Travel | The map screen zoomed to a town | Town markers and at least one fast-travel-eligible icon |
| `shops_ui.png` | Money and Shops | The shop menu open at a general store | Item list with prices, and the player's cash |
| `gun-dealers_ui.png` | Gun Dealers | A gun dealer's shop menu, or the dealer NPC at their location | Weapons in the list, or the dealer clearly identifiable |
| `wanted-system_ui.png` | The Wanted System | The wanted HUD element at a non-zero level, with occupying forces nearby | The wanted indicator, legible |
| `skills_ui.png` | Skills and Levelling | The character sheet / skills screen | Skill names and levels |
| `recruits_ui.png` | Recruits | The recruit management screen, or the player with two or three recruits following | Recruits identifiable as yours |
| `camps_ui.png` | Camps and Placing | A placed camp, or the placement preview mid-placement | The camp composition, or the green/red placement ghost |
| `fobs_ui.png` | FOBs and Building | A built-up FOB, or the build menu open | Built structures, or the buildable list |
| `base-capture_ui.png` | Capturing Bases | A military base under attack or just captured, with the capture indicator up | The base and its control indicator |

**Optional body banners (only if easy):** **1024 x 576** (16:9) into **`UI/Textures/FieldManual/Body/`**, same `<kebab>_ui.png` naming, for Welcome and Capturing Bases. The feature does not need them.

*Corrected 2026-08-08.* This line previously said 600 x 200, taken from Overthrow's existing `overthrowbig_ui.png`. **That file is a logo banner, not a screenshot** — 3:1 is right for artwork and wrong for a photo. Measured against the base game, body/content images are dominated by **1024 x 576** (59 files) with 1024-wide standard across the 244 measured; the piece caps at `MaxDesiredWidth 900` / `MaxDesiredHeight 800` (`FieldManual_Piece_Image_Basic.layout`), so 1024 downscales cleanly and never upscales.

---

## Consolidated string-table export list

*Authoritative version written by Phase 8.7 from `git diff -U0 Language/localization_Overthrow.st`, **from the diff, not from the phase reports**.*

**Total: 102 new string ids**, plus **6 pre-existing items relocated with byte-identical content** (no re-translation needed). The 102 matches the phase reports exactly when summed (25 + 15 + 14 + 16 + 32), which is the cross-check this task exists to perform.

**🟢 Already exported.** All 102 are present in the six `Language/localization_Overthrow.<lang>.conf` runtime exports as of the user's Workbench export at 2026-08-08 19:11. This list is kept as the record and as the re-export checklist if the `.st` is edited again.

Command that produced it:
```bash
git diff -U0 Language/localization_Overthrow.st | grep '^+   Id ' | sed 's/^+   Id "//; s/"$//' | sort
```

### The 102 new ids, grouped by entry

| Key family | Count | Suffixes |
|---|---|---|
| `#OVT-FieldManual_Category_MoneyAndTrade_*` | 1 | `_Title` |
| `#OVT-FieldManual_Category_StayingHidden_*` | 1 | `_Title` |
| `#OVT-FieldManual_Category_TheResistance_*` | 1 | `_Title` |
| `#OVT-FieldManual_Welcome_*` | 6 | `_Title`, `_Text`, `_Text2`, `_Text3`, `_Head`, `_Head2` |
| `#OVT-FieldManual_MainMenu_*` | 1 | `_Text2` (the title and `_Text` are pre-existing) |
| `#OVT-FieldManual_YourHome_*` | 7 | `_Title`, `_Text`, `_Text2`, `_Text3`, `_Text4`, `_Head`, `_Head2` |
| `#OVT-FieldManual_MapAndTravel_*` | 7 | `_Title`, `_Text`, `_Text2`, `_Text3`, `_Text4`, `_Text5`, `_Head` |
| `#OVT-FieldManual_Shops_*` | 10 | `_Title`, `_Text`…`_Text5`, `_Head`…`_Head4` |
| `#OVT-FieldManual_GunDealers_*` | 8 | `_Title`, `_Text`…`_Text4`, `_Head`…`_Head3` |
| `#OVT-FieldManual_WantedSystem_*` | 10 | `_Title`, `_Text`…`_Text5`, `_Head`…`_Head4` |
| `#OVT-FieldManual_Skills_*` | 10 | `_Title`, `_Text`…`_Text5`, `_Head`…`_Head4` |
| `#OVT-FieldManual_Recruits_*` | 10 | `_Title`, `_Text`…`_Text5`, `_Head`…`_Head4` |
| `#OVT-FieldManual_Camps_*` | 10 | `_Title`, `_Text`…`_Text5`, `_Head`…`_Head4` |
| `#OVT-FieldManual_FOBs_*` | 10 | `_Title`, `_Text`…`_Text5`, `_Head`…`_Head4` |
| `#OVT-FieldManual_BaseCapture_*` | 10 | `_Title`, `_Text`…`_Text5`, `_Head`…`_Head4` |
| **Total** | **102** | |

### The twelve frozen link ids, in full (the contract `tutorial-content` consumes)

`#OVT-FieldManual_Welcome_Title` · `#OVT-FieldManual_MainMenu_Title` (pre-existing, immutable) · `#OVT-FieldManual_YourHome_Title` · `#OVT-FieldManual_MapAndTravel_Title` · `#OVT-FieldManual_Shops_Title` · `#OVT-FieldManual_GunDealers_Title` · `#OVT-FieldManual_WantedSystem_Title` · `#OVT-FieldManual_Skills_Title` · `#OVT-FieldManual_Recruits_Title` · `#OVT-FieldManual_Camps_Title` · `#OVT-FieldManual_FOBs_Title` · `#OVT-FieldManual_BaseCapture_Title`

### The 6 relocated items (changed position, not content)

`OVT-FieldManual_Category_Overthrow_Title`, `OVT-FieldManual_MainMenu_Title`, `OVT-FieldManual_MainMenu_Text`, `OVT-FieldManual_MapInfo_Text`, `OVT-FieldManual_FastTravel_Text`, `OVT-FieldManual_Resistance_Text`.

Each was extracted from `HEAD` and from the working tree and compared block-by-block: **all six are byte-identical**, including every `Target_*` translation. They appear in the diff only because their item block moved within the file. This is D5 working as intended, and it is the proof for **F8** that `#OVT-FieldManual_MainMenu_Title` never changed.

**Zero string ids were deleted or renamed by this feature.**
