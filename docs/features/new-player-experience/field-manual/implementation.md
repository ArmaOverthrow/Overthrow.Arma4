# Field Manual — Implementation Plan

**Status:** ✅ **COMPLETE** (all 9 phases · play-test passed 2026-08-08 · string-table exported · all 12 tiles delivered and wired · 24/24 Definition of Done)
**Started:** 2026-08-08
**Target Completion:** TBD
**Last Updated:** 2026-08-08 20:40

**Epic:** `new-player-experience` (feature **#2 of 5**)
**Feature slug:** `new-player-experience/field-manual`

---

## 1. Executive Summary

Overthrow's field manual currently holds **one** page ("Main Menu") under one sub-category ("Getting Started"), inside a category the mod appends to the base game's manual via a same-GUID delta. This feature grows that into **four themed sub-categories holding twelve entries**, covering every system the epic's tutorial content will touch: what Overthrow is, the main menu, your home, the map and fast travel, money and shops, gun dealers, the wanted system, skills, recruits, camps and placing, FOBs and building, and capturing bases.

The feature is **content-only**: `.conf` data, stringtable items in `Language/localization_Overthrow.st`, and matching edits to the public wiki. There is no gameplay EnforceScript in it. The single sanctioned script change is a **test-only** hardening of the existing Init guard (§5, D8) so the twelve frozen title keys are protected the moment they land.

It also owns a **full staleness audit of the wiki's player-facing pages** (~30 pages), not just the pages that mirror a manual entry. Writing the in-game page and the wiki page from one reading of the source is the mechanism that keeps them in agreement, which is why every content phase is executed by the **`help-docs-sync`** agent rather than being split across surfaces.

Phase 1 lands the whole skeleton with all twelve title keys frozen and real (if short) bodies, so `tutorial-content` can wire "Learn more" links on day one.

---

## 2. Goals

### Primary

1. **Twelve reachable manual entries** across four named sub-category buttons, each describing a system in reference voice.
2. **Frozen, documented entry ids** (`m_sTitle` keys) that `tutorial-content` can consume immediately, and that this feature guarantees not to rename.
3. **The wiki and the manual agree** on names, numbers and behaviour for every system covered, and every remaining player-facing wiki page is audited for staleness.
4. **Nothing regresses**: the base game's five categories, the eight tile backgrounds, and the shipped `economy-first-buy` deep link all still work.

### Secondary

5. Retire the "one page called Main Menu that is actually four pages in a trench coat" structure without renaming its published key or discarding its five existing translations.
6. Hand the epic's later features what they need: a `Welcome` entry `first-spawn` can link to, and an explicit, written handoff of the wiki's starter-jobs content to `starter-jobs-retirement`.
7. Produce a screenshot shot list the user can work through in parallel, with a fallback that guarantees the feature is never blocked on art.

### Explicit non-goals

- Late-game entries (warehouse/port, real estate trading, loadouts, vehicle upgrades). Out of scope per `requirements.md`.
- Bespoke illustrated art. Screenshot-grade imagery only.
- Any framework or gameplay code change. If the base framework cannot do something, the need goes to `tutorial-system` or is dropped.
- Wiki restructuring, page deletion, or rewriting release-note history (`v1_3`, `v1-4-update`).

---

## 3. Architecture Overview

### 3.1 The config tree that will exist when done

```
SCR_FieldManualConfigRoot                 (base game, {17295EF80DC38D53} — Overthrow delta appends one element)
├ #AR-FieldManual_Category_Introduction_Title   (vanilla)
├ #AR-Editor                                     (vanilla)
├ #AR-FieldManual_Page_Conflict_Title            (vanilla)
├ #AR-FieldManual_Category_Gameplay_Title        (vanilla)
├ #AR-FieldManual_Category_Equipment_Title       (vanilla)
└ #OVT-FieldManual_Category_Overthrow_Title      ← HEADING (has sub-categories)
   ├ #OVT-FieldManual_Category_GettingStarted_Title   ← BUTTON  "Getting Started"
   │   ├ Welcome to Overthrow
   │   ├ Main Menu                     (existing entry, key immutable)
   │   ├ Your Home
   │   └ The Map and Fast Travel
   ├ #OVT-FieldManual_Category_MoneyAndTrade_Title    ← BUTTON  "Money and Trade"
   │   ├ Money and Shops
   │   └ Gun Dealers
   ├ #OVT-FieldManual_Category_StayingHidden_Title    ← BUTTON  "Staying Hidden"
   │   ├ The Wanted System
   │   └ Skills and Levelling
   └ #OVT-FieldManual_Category_TheResistance_Title    ← BUTTON  "The Resistance"
       ├ Recruits
       ├ Camps and Placing
       ├ FOBs and Building
       └ Capturing Bases
```

**Why the heading/button split is what it is:** `SCR_FieldManualConfigCategory.CreateWidget` renders a category with sub-categories through `CreateMainCategory` (a `TextWidget` heading) and one without through `CreateSubCategory` (a `ButtonWidget`). `SCR_FieldManualUI.CreateCategoryMenuWidgets` (`:667-699`) draws the category then each of its sub-categories into the same left-hand list. So "Overthrow" is a non-clickable heading and the four sub-categories are the four buttons. This is verified in the base-game source, not assumed.

### 3.2 The id scheme (this is what `tutorial-content` consumes)

**Rule: an entry's `m_sTitle` localization key IS its link id.** `OVT_TutorialEntryConfig.m_sFieldManualTitleKey` holds it verbatim; `SCR_FieldManualUI.OVT_OpenEntryByTitle` walks `m_aAllEntries` comparing `entry.m_sTitle` with an **exact, case-sensitive** string match.

| Kind | Pattern | Example | Mutability |
|---|---|---|---|
| Sub-category title | `#OVT-FieldManual_Category_<Pascal>_Title` | `#OVT-FieldManual_Category_MoneyAndTrade_Title` | Frozen after Phase 1 |
| **Entry title (the link id)** | `#OVT-FieldManual_<Pascal>_Title` | `#OVT-FieldManual_WantedSystem_Title` | **Immutable forever** |
| Entry body text piece | `#OVT-FieldManual_<Pascal>_Text`, then `_Text2`, `_Text3`… | `#OVT-FieldManual_WantedSystem_Text2` | Free to edit, move, retire |
| Entry section header piece | `#OVT-FieldManual_<Pascal>_Head`, then `_Head2`… | `#OVT-FieldManual_Recruits_Head2` | Free to edit, move, retire |

Only the **title** is a contract. Body and header pieces are free to be reworded, split, reordered or moved between entries — nothing links to them. That distinction is what makes §5 D4's redistribution of the Main Menu body safe.

Retirement path for an entry, if one is ever needed: set `m_bEnabled 0`. **Never recycle or rename a title key.** A renamed key does not error; the manual opens on its front page and logs a warning (designed behaviour I2 from `tutorial-system`), which is exactly the kind of failure nobody notices for three releases.

### 3.3 The twelve entries — frozen key table

`tutorial-content` should copy this table verbatim into its own plan.

| # | Sub-category | Display title (en_us) | **Title key (link id)** | Status | Source of truth to verify against |
|---|---|---|---|---|---|
| 1 | Getting Started | Welcome to Overthrow | `#OVT-FieldManual_Welcome_Title` | NEW | `docs/mission-statement.md`, `Scripts/Game/GameMode/OVT_OverthrowGameMode.c` |
| 2 | Getting Started | Main Menu | `#OVT-FieldManual_MainMenu_Title` | **EXISTS — immutable, already linked from `Configs/Tutorials/proofFirstBuy.conf`** | `Scripts/Game/UI/Context/OVT_MainMenuContext.c`, `UI/Layouts/Menu/MainMenu.layout` |
| 3 | Getting Started | Your Home | `#OVT-FieldManual_YourHome_Title` | NEW | `Scripts/Game/GameMode/Managers/OVT_RealEstateManagerComponent.c`, `Scripts/Game/UI/Context/OVT_RealEstateContext.c` |
| 4 | Getting Started | The Map and Fast Travel | `#OVT-FieldManual_MapAndTravel_Title` | NEW | `Scripts/Game/UI/Context/OVT_MapContext.c`, `OVT_PlayerCommsComponent.c` (fast-travel path), `Scripts/Game/Configuration/OVT_DifficultySettings.c` (cost) |
| 5 | Money and Trade | Money and Shops | `#OVT-FieldManual_Shops_Title` | NEW | `Scripts/Game/GameMode/Managers/OVT_EconomyManagerComponent.c`, `Scripts/Game/Components/Economy/OVT_ShopComponent.c`, `Scripts/Game/Configuration/OVT_ShopConfig.c` |
| 6 | Money and Trade | Gun Dealers | `#OVT-FieldManual_GunDealers_Title` | NEW | `OVT_ShopConfig.c` (shop types), `Scripts/Game/UI/Context/OVT_ShopContext.c`, wiki `gun-dealer` |
| 7 | Staying Hidden | The Wanted System | `#OVT-FieldManual_WantedSystem_Title` | NEW | `Scripts/Game/Components/Player/OVT_PlayerWantedComponent.c`, `Scripts/Game/UI/HUD/OVT_WantedInfo.c` |
| 8 | Staying Hidden | Skills and Levelling | `#OVT-FieldManual_Skills_Title` | NEW | `Scripts/Game/GameMode/Managers/OVT_SkillManagerComponent.c`, `Scripts/Game/UI/Context/OVT_CharacterSheetContext.c` |
| 9 | The Resistance | Recruits | `#OVT-FieldManual_Recruits_Title` | NEW | `Scripts/Game/GameMode/Managers/OVT_RecruitManagerComponent.c` |
| 10 | The Resistance | Camps and Placing | `#OVT-FieldManual_Camps_Title` | NEW | `Scripts/Game/GameMode/Managers/Factions/OVT_ResistanceFactionManager.c` (`m_OnPlace`, placeables), `Configs/Resistance/` |
| 11 | The Resistance | FOBs and Building | `#OVT-FieldManual_FOBs_Title` | NEW | `OVT_ResistanceFactionManager.c` (`m_OnBuild`, FOB), `Scripts/Game/UI/Context/OVT_BuildContext.c` |
| 12 | The Resistance | Capturing Bases | `#OVT-FieldManual_BaseCapture_Title` | NEW | `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c` (`m_OnBaseControlChanged`), base controller components |

**Display strings deliberately avoid the `&` character.** A grep of `Language/localization_Overthrow.st` returns **zero** `Target_en_us` values containing `&` across the whole master table. That is a strong enough house convention to follow rather than test, so the user's "Money & Trade" ships as **"Money and Trade"** and the four-way split and entry allocation are otherwise exactly as chosen.

### 3.4 Where the Main Menu entry's current body goes

The existing entry bundles four topics. Its **title key does not move**; its **pieces** do. Existing body keys are reused verbatim wherever the text is still accurate, because each already carries five translations (fr, ru, ko, zh, uk) that minting a new id would throw away.

| Existing piece | Current key | Destination |
|---|---|---|
| Intro text (keybind line) | `#OVT-FieldManual_MainMenu_Text` | **Stays** on Main Menu |
| Header "Map Info" | `#OVT-MainMenu_MapInfo` | **Moves** to entry 4 |
| Text | `#OVT-FieldManual_MapInfo_Text` | **Moves** to entry 4 (reused, keeps translations) |
| Header "Fast Travel" | `#OVT-MainMenu_FastTravel` | **Moves** to entry 4 |
| Text | `#OVT-FieldManual_FastTravel_Text` | **Moves** to entry 4 (reused, keeps translations) |
| Header "Resistance" | `#OVT-MainMenu_Resistance` | **Stays** on Main Menu |
| Text | `#OVT-FieldManual_Resistance_Text` | **Stays** on Main Menu (reused, keeps translations) |
| Body image `overthrowbig_ui.edds` | — | **Moves** to entry 1 (Welcome), which is the natural home for the mod's banner |

Main Menu then reads: keybind line → one **new** `_Text2` listing what the menu holds → the Resistance-funds section it already had. The resistance funds/tax/officer content has no home in the four-way split and describes a main-menu screen, so it stays where it is; a future feature may relocate it freely because body pieces are not link ids.

### 3.5 Files

```
Configs/FieldManual/
├── FieldManualConfigRoot.conf            ← DO NOT TOUCH (same-GUID delta, 6 lines)
├── FieldManualConfigRoot.conf.meta       ← DO NOT TOUCH ({17295EF80DC38D53})
└── Categories/
    ├── FM_Overthrow.conf                 ← the ONLY config this feature edits ({6B3A000000000090})
    └── FM_Overthrow.conf.meta            ← unchanged

Language/
└── localization_Overthrow.st             ← the ONLY localization file this feature edits

UI/Textures/FieldManual/
├── overthrow_ui.edds        {032FD1094792F0AC}  400x300  ← interim fallback, now UNREFERENCED
├── overthrowbig_ui.edds     {7C783AEA8CF9E7B1}  600x200  ← body banner (Welcome entry)
└── Tiles/
    ├── default_ui.edds      {CF6B203430123E78}  400x300  ← THE universal tile fallback
    └── <kebab>_ui.edds                                   ← per-entry tiles, imported by the USER

Scripts/Game/Tests/TestSuites/Init/
└── OVT_TEST_InitSuite.c                  ← ONE test case extended (Phase 1b). No gameplay script.

docs/features/new-player-experience/
├── field-manual/{implementation,tasks,context}.md
└── starter-jobs-retirement/requirements.md  ← appended handoff note (Phase 7)
```

**GUID block: `6B3B0000…` is reserved for this feature.** Verified free on 2026-08-08: `grep -rEoh "\{6B3B[0-9A-F]{12}\}"` across the repo returns **0** matches (`6B3A…`, tutorial-system's block, returns 148). Allocate sequentially: `6B3B0000000000xx` for `FM_Overthrow.conf` inline objects, `6B3B0000000001xx` for stringtable items.

### 3.6 What the entries may and may not contain

Available piece classes, all confirmed present in `/mnt/n/Projects/Arma 4/ArmaReforger/scripts/Game/FieldManual/Models/Pieces/`:

| Piece | Use | Notes |
|---|---|---|
| `SCR_FieldManualPiece_Text` | Body prose | Rich text works: `<br/>`, `<color rgba='...'>`, `<action name="OverthrowMainMenu"/>` |
| `SCR_FieldManualPiece_Header` | Section header inside an entry | Optional `m_ImagePath` icon |
| `SCR_FieldManualPiece_Image` | Body image | `insertable: false` (no Workbench picker) but valid hand-written in `.conf` — the shipped entry already does this |
| `SCR_FieldManualPiece_LineBreak` / `_Separator` | Spacing | `insertable: false`, same caveat |

**Not used, deliberately:** `SCR_FieldManualPiece_Keybind` / `_KeybindList`. The inline `<action name="…"/>` form is already proven working with an Overthrow action in the shipped `#OVT-FieldManual_MainMenu_Text`; the dedicated keybind pieces are unproven against Overthrow's `chimeraInputCommon.conf` actions and proving them is a spike this feature does not need. YAGNI.

**Entry class:** `SCR_FieldManualConfigEntry_Standard` for all twelve. `SCR_FieldManualConfigEntry` is `insertable: false` and its own title field literally says "Do NOT use". `m_eId` stays at its `NONE` default on every entry — there is no valid `EFieldManualEntryId` value for an Overthrow page and borrowing one hijacks a vanilla hint's deep link.

---

## 4. Implementation Phases

Every phase ends with `tools/compile-check.sh` clean (exit 0) and `tools/run-tests.sh "{6A6E29FF47ECB840}"` green.

**Verified baselines, measured 2026-08-08 on this working tree (the counts in `CLAUDE.md` are stale):**
- Fast group `{6A6E29FF47ECB840}` — **47 tests**, 24 s, exit 0
- All group `{6A6E2A002F53A581}` — **77 tests**, 30 s, exit 0. ⚠️ **Re-measured 78 during Phase 1b (2026-08-08).** A concurrent session in this tree added an untracked `Scripts/Game/Tests/TestSuites/Campaign/OVT_TEST_Campaign_TutorialSpawnTrigger.c` holding exactly one case; Campaign is in All but not Fast. **Expect 78 while that file exists** — it is not a regression from this feature, and Fast is unaffected at 47.

No phase in this plan changes either count (Phase 1b extends an existing case rather than adding one).

---

### Phase 1a — Skeleton, frozen keys, placeholder bodies
*Agent: **`help-docs-sync`**. Standard.* · *Estimate: 1 session*

The whole structure lands here so `tutorial-content` is unblocked on day one. Bodies are short but **real** — see the critical constraint below.

| # | Task |
|---|---|
| 1a.1 | Add three sub-category string items to `Language/localization_Overthrow.st`: `OVT-FieldManual_Category_MoneyAndTrade_Title` = "Money and Trade", `OVT-FieldManual_Category_StayingHidden_Title` = "Staying Hidden", `OVT-FieldManual_Category_TheResistance_Title` = "The Resistance". Each needs a fresh `6B3B…` GUID, `Target_en_us`, and a translator `Comment` modelled on the existing `OVT-FieldManual_Category_GettingStarted_Title` comment (which explains the narrow left-hand list context and the two-or-three-word budget) |
| 1a.2 | Add eleven entry **title** items (`#OVT-FieldManual_<Pascal>_Title` per §3.3, rows 1 and 3-12). Each with a `Comment` stating it is a Field Manual page title shown on a tile and as a page heading, and that **it is also the deep-link id used by tutorial popups and must not be renamed** |
| 1a.3 | Add eleven first-body items (`#OVT-FieldManual_<Pascal>_Text`) with **genuine one-to-three-sentence placeholder prose** in the reference voice. Not lorem, not "TODO": a true, short statement of what the system is. A later phase expands it |
| 1a.4 | Restructure `Configs/FieldManual/Categories/FM_Overthrow.conf`: keep the root `SCR_FieldManualConfigCategory` and its `m_sTitle`; keep sub-category element `{59908331F77F1D0F}` (Getting Started) and entry element `{59908331D44CD51F}` (Main Menu) **with their existing GUIDs**; add three new sub-category elements and eleven new entry elements with fresh `6B3B…` GUIDs |
| 1a.5 | Redistribute the Main Menu body per §3.4. Move the Map Info and Fast Travel header+text pieces into entry 4, move the banner image piece into entry 1, leave the Resistance pieces on Main Menu. **Piece GUIDs move with their pieces** — do not mint new ones for relocated pieces |
| 1a.6 | Set `m_Image "{032FD1094792F0AC}UI/Textures/FieldManual/overthrow_ui.edds"` on **all twelve** entries. This is mandatory now, not in Phase 6 (see D7) |
| 1a.7 | Set `m_bEnabled 1` and leave `m_eId` at `NONE` explicitly on every entry — this file is a template as much as it is data (gotcha 32) |
| 1a.8 | Re-read the edited `.conf` for balanced braces and zero duplicate GUIDs: `grep -oE '\{[0-9A-F]{16}\}' Configs/FieldManual/Categories/FM_Overthrow.conf \| sort \| uniq -d` must print nothing |
| 1a.9 | Report the **complete list of new string ids** for the user's Workbench export, and append it to §6's consolidated export table |

**⚠️ CRITICAL — placeholders must contain content.** `SCR_FieldManualUI.SetAllEntriesAndParents` (`:600-663`, read and confirmed) removes any entry where `!entry.m_bEnabled || entry.m_aContent.IsEmpty()`, then removes any sub-category left with no entries and no sub-categories, then any category left with neither. An entry with an empty `m_aContent` **does not exist** in the merged manual, is absent from `m_aAllEntries`, and its link resolves to nothing. Every entry created in this phase ships with at least one real `SCR_FieldManualPiece_Text`.

**Also note:** the same method only walks `category → subCategory → subCategory.m_aEntries`. Entries placed directly on a **top-level** category are never added to `m_aAllEntries` and are therefore unlinkable. All twelve entries live under a sub-category.

**Acceptance:**
- `FM_Overthrow.conf` declares one root category, four sub-categories and twelve entries; every entry has ≥1 content piece and a non-empty `m_Image`.
- `#OVT-FieldManual_MainMenu_Title` is byte-identical to what shipped.
- `git status` shows `Configs/FieldManual/FieldManualConfigRoot.conf` **untouched**.
- `git diff --stat Language/` lists **only** `localization_Overthrow.st`.
- compile-check 0; Fast 47.

---

### Phase 1b — Harden the Init guard around the frozen ids
*Agent: **`component-developer`**. Standard. **This is the only phase that touches a `.c` file, and it is test code only.*** · *Estimate: half session*

**First, a correction to the received brief.** `OVT_TEST_Init_FieldManual_DeltaMergesAndLinksResolve` does **not** assert an exact entry count. Read at `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c:1908-2196`, it asserts:

1. the loader returns non-null;
2. ≥1 category titled `#OVT-FieldManual_Category_Overthrow_Title`;
3. `total - overthrowCount >= VANILLA_CATEGORY_FLOOR (5)` — an explicit **floor**, documented in the case header as "THE COUNTS ARE FLOORS, NOT MAGIC NUMBERS";
4. `m_aTileBackgrounds` non-empty;
5. Overthrow's category has ≥1 sub-category, and no sub-category is titled with vanilla's Introduction key;
6. every authored tutorial `m_sFieldManualTitleKey` resolves to some entry `m_sTitle`.

The 141-entry number lives in `DescribeRoot()`, which is **printed to the log, never asserted**. So adding three sub-categories and eleven entries cannot turn this case red, and **no existing assertion needs updating or weakening.**

What the feature *does* create is a genuine gap: between Phase 1a and `tutorial-content`, eleven frozen ids exist with nothing pointing at them, so branch 6 cannot see them. If one is silently pruned or misspelled, nothing notices until a popup lands on the front page months later. Three new branches close that, added **to the existing case** rather than as new cases (same subject, same load, no test-count churn).

| # | Task |
|---|---|
| 1b.1 | New branch **A — every Overthrow entry has content.** Walk Overthrow's category → sub-categories → entries; fail on any entry whose `m_aContent` is null or empty. Message must name the entry title and explain that `SetAllEntriesAndParents` prunes it silently, so it is not in the manual and its deep link is dead |
| 1b.2 | New branch **B — the four sub-categories are present.** A `static const ref array<string>` of the four keys from §3.1; fail if any is missing from Overthrow's category. **Membership, not equality** — a fifth sub-category added later must not turn this red |
| 1b.3 | New branch **C — Overthrow entry titles are unique across the merged manual.** Collect all entry titles via the existing `CollectEntryTitles`; for each title starting with `#OVT-FieldManual_`, fail if it appears more than once. A collision (with a vanilla page or another Overthrow page) makes `OVT_OpenEntryByTitle` land on whichever comes first, which is silent and wrong |
| 1b.4 | **Prove each branch red once, and record the method** in `context.md`'s proven-red table with the exact failure text and the date. Suggested realistic breakages: A — delete the single `SCR_FieldManualPiece_Text` from one entry; B — rename one sub-category key by one character; C — paste an existing title key onto a second entry. Revert immediately after each |
| 1b.5 | Update the case's header comment to describe the three new branches and restate that the entry count is a log line, not an assertion |

**Rejected as over-constraint:** hard-coding all twelve entry title keys in the test. It duplicates the `.conf` into the test file and turns every future content edit into a two-file change. Branches A-C plus the pre-existing branch 6 already make a broken or pruned link a red build once `tutorial-content` links it, and A alone catches the pruning trap immediately.

**Acceptance:** compile-check 0; Fast **47** and All **77** (unchanged — a case was extended, not added); three red-proofs recorded with verbatim failure text.

---

### Phase 1c — Issue the screenshot shot list
*Agent: none — the shot list is §4.1 of this document, so this phase is discharged the moment the plan is accepted.* · *Estimate: zero*

The user captures PNGs at their convenience and imports them as `.edds` in Workbench under `UI/Textures/FieldManual/`. Phase 6 wires whatever arrived. Nothing downstream blocks on this.

#### 4.1 SHOT LIST — 12 tiles

**Format:** PNG, **400 x 300** (4:3), matching the existing `overthrow_ui.png`. Import in Workbench as `.edds` into `UI/Textures/FieldManual/` (the importer writes the `.meta` and assigns a GUID — Phase 6 reads the GUID from that `.meta`). Screenshot-grade is fine; readability beats artistry. HUD may be visible. Avoid capturing player names or server IPs.

| Filename | Entry | Capture | Must be visible |
|---|---|---|---|
| `fm_welcome.png` | Welcome to Overthrow | A wide town or countryside shot with FIA presence | Something recognisably Overthrow: a resistance flag, a town, or the player's car outside a house |
| `fm_mainmenu.png` | Main Menu | The Overthrow main menu, open | The full button list, legible |
| `fm_yourhome.png` | Your Home | The exterior of an owned house with the player's starting car | House and vehicle in one frame |
| `fm_map.png` | The Map and Fast Travel | The map screen zoomed to a town | Town markers and at least one fast-travel-eligible icon |
| `fm_shops.png` | Money and Shops | The shop menu open at a general store | Item list with prices, and the player's cash |
| `fm_gundealer.png` | Gun Dealers | A gun dealer's shop menu, or the dealer NPC at their location | Weapons in the list, or the dealer clearly identifiable |
| `fm_wanted.png` | The Wanted System | The wanted HUD element at a non-zero level, with occupying forces nearby | The wanted indicator, legible |
| `fm_skills.png` | Skills and Levelling | The character sheet / skills screen | Skill names and levels |
| `fm_recruits.png` | Recruits | The recruit management screen, or the player with two or three recruits following | Recruits identifiable as yours |
| `fm_camps.png` | Camps and Placing | A placed camp, or the placement preview mid-placement | The camp composition, or the green/red placement ghost |
| `fm_fobs.png` | FOBs and Building | A built-up FOB, or the build menu open | Built structures, or the buildable list |
| `fm_basecapture.png` | Capturing Bases | A military base under attack or just captured, with the capture indicator up | The base and its control indicator |

**Optional body banners (only if easy):** 600 x 200 wide crops named `fm_<entry>_wide.png` for Welcome and Capturing Bases. The feature does not need them.

---

### Phase 2 — Getting Started: 4 entries + wiki sync
*Agent: **`help-docs-sync`**. Standard.* · *Estimate: 1 session*

| # | Task |
|---|---|
| 2.1 | Read the source files listed in §3.3 rows 1-4 before writing. **Never document behaviour not verified in the source** |
| 2.2 | Expand entry **1 Welcome to Overthrow** to full length: what Overthrow is (a persistent occupied-island sandbox), that there are no assigned objectives, what the player starts with, that the resistance grows from what the player chooses to do. Carries the `overthrowbig_ui.edds` banner relocated in 1a.5 |
| 2.3 | Expand entry **2 Main Menu**: keep `_Text` (keybind line) as-is; add `_Text2` describing what the menu holds; keep the Resistance header+text section |
| 2.4 | Expand entry **3 Your Home**: what "owned" means, the starting house and car, what a home gives you (respawn, storage, fast-travel destination), how ownership is acquired and what it costs. Verify against `OVT_RealEstateManagerComponent.c` |
| 2.5 | Expand entry **4 The Map and Fast Travel** around the two relocated pieces: what map info shows, the map-in-inventory requirement, what fast travel needs and what it costs, and that cost scales with difficulty. Verify against `OVT_DifficultySettings.c` before quoting any number |
| 2.6 | **Wiki group A** — read, then surgically update: `home`, `getting-started`, `your-home`, `fast-travel`, `real-estate`. For `getting-started` see the constraint below |
| 2.7 | Report new/changed string ids for the export list |

**`getting-started` (pageId 2) — what this phase fixes and what it must not touch.** The page is written entirely in "New in v1.3" framing, says the job system "currently has some known bugs", and describes the five starter tutorial jobs.

- **Fix now:** remove the "New in v1.3" version framing (the page describes current behaviour, not a release); replace the vague "known bugs" sentence with a factual description or nothing; correct anything that contradicts shipped behaviour; fix broken links.
- **Do NOT touch now:** the section describing the five starter jobs. Those jobs still exist in the shipped game; deleting their documentation before `starter-jobs-retirement` removes them would make the wiki wrong in the other direction.
- **Handoff instead:** Phase 7 writes an explicit note into `starter-jobs-retirement`'s requirements. Phase 2 leaves the section intact and flags it in its report.

**Acceptance:** four entries with substantive bodies; five wiki pages read and updated in place with no restructuring; `getting-started` de-versioned with its starter-jobs section intact; no em-dash in anything authored; compile-check 0; Fast 47.

---

### Phase 3 — Money and Trade: 2 entries + wiki sync
*Agent: **`help-docs-sync`**. Standard.* · *Estimate: half session*

| # | Task |
|---|---|
| 3.1 | Expand entry **5 Money and Shops**: where money comes from, that shops buy and sell, that prices differ by town and by stock, what affects availability. Verify against `OVT_EconomyManagerComponent.c` / `OVT_ShopComponent.c` / `OVT_ShopConfig.c` |
| 3.2 | Expand entry **6 Gun Dealers**: what a gun dealer is, how it differs from a general shop, what gates access to better stock. Verify against `OVT_ShopConfig.c` shop types |
| 3.3 | **Wiki group B** — read, then surgically update: `gun-dealer`, `resources`, `town-taxes`, `resistance-funds`, `resistance-tax`, `officer` |
| 3.4 | If `wikijs_search_pages` confirms **no** page covers shops/economy generally, create **one** page `shops` under the same flat hierarchy. If any equivalent exists, update it instead. Do not create more than this one page in this phase |
| 3.5 | Report new/changed string ids |

**Acceptance:** two entries substantive; six wiki pages audited; at most one new wiki page and only on confirmed absence; the manual's and wiki's account of prices/stock agree; compile-check 0; Fast 47.

---

### Phase 4 — Staying Hidden: 2 entries + wiki sync
*Agent: **`help-docs-sync`**. Standard.* · *Estimate: half session*

| # | Task |
|---|---|
| 4.1 | Expand entry **7 The Wanted System**: what raises a wanted level, what it means for the occupying faction's behaviour, how it decays, what the HUD indicator shows. Verify against `OVT_PlayerWantedComponent.c` (note: escalation and decay take different code paths) and `OVT_WantedInfo.c` |
| 4.2 | Expand entry **8 Skills and Levelling**: how XP is earned, what skills exist, where they are spent, what levelling changes. Verify against `OVT_SkillManagerComponent.c` and `OVT_CharacterSheetContext.c` |
| 4.3 | **Wiki group C** — read, then surgically update: `wanted-system`, `threat`, `stability`, `town-support` |
| 4.4 | If search confirms **no** skills page exists, create **one** page `skills`. Otherwise update the existing one. Do not create more than this one page in this phase |
| 4.5 | Report new/changed string ids |

**Note for the author:** these two entries sit together under "Staying Hidden" because both are about surviving contact, not because skills are a stealth system. The Skills entry must not pretend otherwise — write it as "Skills and Levelling", let the sub-category name be a shelf label rather than a claim.

**Acceptance:** two entries substantive; four wiki pages audited; at most one new page; wanted-level numbers in the manual match the source; compile-check 0; Fast 47.

---

### Phase 5 — The Resistance: 4 entries + wiki sync
*Agent: **`help-docs-sync`**. Standard, but this is the heaviest verification load in the feature — four systems in one pass. If the agent's context gets tight, split 5.1-5.2 and 5.3-5.4 into two runs.* · *Estimate: 1 to 1.5 sessions*

| # | Task |
|---|---|
| 5.1 | Expand entry **9 Recruits**: how civilians become recruits, what they cost, what they can be told to do, that they persist. Verify against `OVT_RecruitManagerComponent.c` |
| 5.2 | Expand entry **10 Camps and Placing**: what placing is, what a camp gives you, where placement is allowed, that some placeables are illegal and can be seen. Verify against `OVT_ResistanceFactionManager.c` and `Configs/Resistance/` (note the `m_bIllegal` flag on placeables) |
| 5.3 | Expand entry **11 FOBs and Building**: what a FOB is, how it differs from a camp, what building needs (supplies, a build zone), what it enables. Verify against `OVT_ResistanceFactionManager.c` build path and `OVT_BuildContext.c` |
| 5.4 | Expand entry **12 Capturing Bases**: what a base is, what capturing involves, what changes on control change, what the occupying faction does about it. Verify against `OVT_OccupyingFactionManager.c` |
| 5.5 | **Wiki group D** — read, then surgically update: `recruits`, `camp`, `fob`, `base`, `qrf`, `factions` |
| 5.6 | Report new/changed string ids |

**Acceptance:** four entries substantive; six wiki pages audited; placing/building content folded into the existing `camp` and `fob` pages rather than new pages; compile-check 0; Fast 47.

---

### Phase 6 — Wire the delivered screenshots
*Agent: **`help-docs-sync`**. Standard.* · *Estimate: quarter session*

| # | Task |
|---|---|
| 6.1 | `ls UI/Textures/FieldManual/*.edds.meta` and read the GUID out of each new `.meta`'s `Name` line. **Never invent a GUID for an imported texture** |
| 6.2 | For each delivered texture, replace that entry's fallback `m_Image` with `"{<guid>}UI/Textures/FieldManual/<file>.edds"` |
| 6.3 | Any entry with no delivered texture **keeps the fallback**. This phase is partial-credit by design |
| 6.4 | Where a 600x200 wide banner was delivered, add a `SCR_FieldManualPiece_Image` as the entry's first content piece (fresh `6B3B…` GUID), mirroring the shipped Welcome banner |
| 6.5 | Re-run the duplicate-GUID grep from 1a.8 |

**Acceptance:** every `m_Image` resolves to a file that exists on disk; no entry has an empty `m_Image`; compile-check 0; Fast 47.

---

### Phase 7 — Full wiki audit of the remaining player-facing pages
*Agent: **`help-docs-sync`**, run **three separate times** (one per group). Standard.* · *Estimate: 1 to 1.5 sessions total*

Pages already covered by Phases 2-5: `home`, `getting-started`, `your-home`, `fast-travel`, `real-estate`, `gun-dealer`, `resources`, `town-taxes`, `resistance-funds`, `resistance-tax`, `officer`, `wanted-system`, `threat`, `stability`, `town-support`, `recruits`, `camp`, `fob`, `base`, `qrf`, `factions`. This phase sweeps the rest.

**Splitting into three runs is a requirement, not a suggestion** — no single agent run should have to hold thirty pages of content.

| Run | Pages | Depth |
|---|---|---|
| **7A — Player mechanics remainder** | `faq`, `difficulty` (and its 6 child pages), `player-groups`, `loadout-manager`, `Importing` | Full staleness audit: does each statement match shipped behaviour? Fix or flag |
| **7B — Operator and modder pages** | `customizing-overthrow`, `overthrow-config`, `dedicated-server-setup`, `discord-web-hook`, `custom-maps-porting-guide`, `reporting-bugs` | Lighter touch: config keys still exist, links still resolve, no obviously dead instructions. Do not rewrite |
| **7C — Release notes and handoff** | `v1_3`, `v1-4-update` | **Links and accuracy of links only. Do not rewrite history** — these are release notes and their version framing is correct by definition. Plus the starter-jobs handoff below |

| # | Task |
|---|---|
| 7.1 | Each run: `wikijs_connection_status`, then `wikijs_get_page` before any `wikijs_update_page`. Surgical edits, preserve voice and structure |
| 7.2 | **No page is deleted. No hierarchy is changed. No page under `development-documentation/` is touched** unless it directly contradicts shipped behaviour, in which case flag it in the report rather than editing it |
| 7.3 | Any page whose staleness cannot be resolved without a gameplay decision is **reported, not guessed** |
| 7.4 | **Starter-jobs handoff (run 7C).** Append a "Documentation handoff (from `field-manual`, 2026-08-XX)" section to `docs/features/new-player-experience/starter-jobs-retirement/requirements.md` naming: the wiki page (`getting-started`, pageId 2) and the specific section describing the five starter jobs; that `field-manual` deliberately left it intact because the jobs still exist; and that `starter-jobs-retirement`'s own sync pass must remove it when the jobs go |
| 7.5 | Produce a one-table audit summary in this feature's `context.md`: page, verdict (current / edited / flagged), one-line reason |

**Acceptance:** every page in the three groups has a recorded verdict; zero pages deleted; zero hierarchy changes; the starter-jobs handoff exists in `starter-jobs-retirement/requirements.md`; every flagged-not-fixed page has a stated reason.

---

### Phase 8 — Verification and the human play-test checklist
*Agent: **`component-developer`** (it runs tooling, not content).* · *Estimate: half session, plus the user's play-test*

| # | Task |
|---|---|
| 8.1 | `tools/compile-check.sh` → expect **0** |
| 8.2 | `tools/run-tests.sh "{6A6E29FF47ECB840}"` → expect **47** · `tools/run-tests.sh "{6A6E2A002F53A581}"` → expect **78** while the other session's untracked Campaign case is present (77 once it is gone). Report the measured number either way |
| 8.3 | `git status --porcelain Configs/FieldManual/FieldManualConfigRoot.conf*` → expect **no output** |
| 8.4 | `git diff --stat Language/` → expect **only** `localization_Overthrow.st` |
| 8.5 | Em-dash gate (see §6 Verification for the exact command) → expect **no matches** |
| 8.6 | Duplicate-GUID gate on `FM_Overthrow.conf` → expect **no output** |
| 8.7 | Collate the **consolidated export list** — every new `#OVT-` id from Phases 1a-6, gathered from `git diff -U0 Language/localization_Overthrow.st` rather than from phase reports, and written into `tasks.md` |
| 8.8 | Write the human play-test checklist into `tasks.md` under **Needs Human Verification**, using §7.3's steps verbatim |
| 8.9 | Update `context.md` (phase log, gotchas, the audit table) and `../epic-overview.md` (feature #2 status and rollup) |

**Acceptance:** every gate above passes or has a written reason; the export list is complete; the play-test checklist is in `tasks.md`.

---

## 5. Key Technical Decisions

**D1 — All content goes in `Configs/FieldManual/Categories/FM_Overthrow.conf`; the root delta is never touched.**
`FieldManualConfigRoot.conf` is a same-GUID delta override of the base game's root. Its `.meta` declares `{17295EF80DC38D53}` — the exact GUID `UI/layouts/Menus/FieldManual/FieldManual.layout:16` hands to `SCR_ConfigUIComponent.m_ConfigPath`, and therefore the exact resource `SCR_FieldManualUI.OnMenuOpen` loads. It is six lines holding one element, `{59908331EDFD9788}`, and two facts about it are load-bearing: that element GUID is **absent from vanilla's root**, which is the entire reason the element appends instead of overwriting (proven the hard way on 2026-08-07 — swapping it to vanilla's Introduction element GUID made Overthrow's category *replace* Introduction, 5 categories instead of 6); and the delta **never declares `m_aTileBackgrounds`**, inheriting the base root's eight, while `SCR_FieldManualUI.c:253` calls `m_aTileBackgrounds.GetRandomElement()` **unguarded** for every tile. An override that stopped inheriting would not degrade the manual, it would error on its first tile. There is also no alternative seam: `SCR_ConfigUIComponent` holds a single `ResourceName`, `SCR_FieldManualConfigLoader.LoadConfigRoot` only walks the object it was handed, and there is no array of extra roots, no directory scan and no registration API.

**D2 — Two category levels, four sub-categories, all entries under a sub-category.**
`SetAllEntriesAndParents` walks root category → sub-category → entries and no deeper; a third level is silently dropped. Entries hung directly on the top-level category are never inserted into `m_aAllEntries` and are therefore unlinkable. Four sub-categories is the user's chosen shape and fits the presentation: the Overthrow category renders as a heading and each sub-category as a clickable button.

**D3 — Title keys are the ids, and they freeze in Phase 1.**
`m_sFieldManualTitleKey` on a tutorial entry holds the target's `m_sTitle` key; the match in `OVT_OpenEntryByTitle` is exact and case-sensitive. Freezing all twelve in Phase 1a is what lets `tutorial-content` start immediately instead of waiting for Phase 5. The `#OVT-FieldManual_` prefix is what makes a collision with the base game's 140 pages impossible; branch C of the Init guard makes a collision *within* Overthrow's own pages impossible too.

**D4 — `#OVT-FieldManual_MainMenu_Title` is immutable; only its pieces move.**
It is already published and already linked from `Configs/Tutorials/proofFirstBuy.conf`. Renaming it breaks that popup silently. Content pieces are not link ids, so redistributing the body across entries 2 and 4 costs nothing.

**D5 — Reuse the existing translated body keys rather than minting new ones.**
`#OVT-FieldManual_MapInfo_Text`, `#OVT-FieldManual_FastTravel_Text` and `#OVT-FieldManual_Resistance_Text` each carry five translations (fr, ru, ko, zh, uk) alongside the English. Moving the piece keeps them; recreating the string under a tidier id throws them away and puts the mod back on one language for that paragraph. Tidiness is not worth fifteen lost translations.

**D6 — A placeholder entry still ships real content.**
`SetAllEntriesAndParents` prunes an entry whose `m_aContent` is empty, then prunes a sub-category left with no entries, then a category left with neither. A content-free placeholder is not a stub, it is an absence — the tile never draws and the deep link resolves to the front page. Every entry gets at least one real `SCR_FieldManualPiece_Text` from the moment it is created.

**D7 — Every entry ships with a fallback tile image from creation.**
`m_Image` on all twelve in Phase 1a. **Updated 2026-08-08:** the fallback is now `{CF6B203430123E78}UI/Textures/FieldManual/Tiles/default_ui.edds`, a purpose-made 400 x 300 Overthrow-logo tile the user supplied during Phase 6. Phase 1a shipped `{032FD1094792F0AC}…/overthrow_ui.edds` as an interim; that asset is now **unreferenced by any `.conf`, `.layout`, `.c` or `.et`** (grep-verified) and is left on disk rather than deleted, since removing an asset is not this feature's call. The screenshot pipeline is an **upgrade path, not a dependency**: if the user never captures a single PNG, the feature still ships twelve entries with a consistent tile. `CreateTileWidget` calls `cardComp.SetImage(entry.m_Image)` for every tile, so a blank there is a visibly broken card rather than a graceful default.

**D8 — Extend the existing Init case with three branches; do not add a case, and do not weaken anything.**
Corrects the brief: the case asserts a **floor** of five vanilla categories and prints (never asserts) the entry count, so this feature's additions cannot turn it red and no existing assertion needs touching. The real gap is that eleven frozen ids will exist with nothing linking to them until `tutorial-content` lands, so branch 6 is blind to them. Branches A (every Overthrow entry has content), B (the four sub-categories exist), and C (Overthrow entry titles are unique) close that. Extending keeps Fast at 47 and All at 77 and keeps one failure surface for one subject. Each branch gets a recorded red-proof, per project rule.

**D9 — `m_eId` stays `NONE`, and `EFieldManualEntryId` is not extended.**
`OpenEntry` matches only on that base-game enum and there is no name lookup anywhere in the class. A `modded enum` needs the enum *plus* a per-entry conf field *plus* a string→enum map, and borrowing an existing value hijacks a vanilla hint's deep link. This was decided as D12 in `tutorial-system` and this feature inherits it.

**D10 — GUID block `6B3B0000…`, verified free.**
`grep -rEoh "\{6B3B[0-9A-F]{12}\}"` returns 0 matches repo-wide (2026-08-08). `6B3A…` is tutorial-system's and returns 148.

**D11 — Wiki: update in place, create at most two pages, delete none.**
The `help-docs-sync` agent's standing rule is search-then-update. Two gaps are known from the live page list — no general shops/economy page and no skills page — and each is authorised as **one** new page in its own phase, only after `wikijs_search_pages` confirms absence. Placing and building content folds into the existing `camp` and `fob` pages. Everything else is an in-place edit.

**D12 — `getting-started` is de-versioned now; its starter-jobs section is handed off, not deleted.**
The "New in v1.3" framing is stale today and is fixed in Phase 2. The five starter jobs it promotes **still exist in the shipped game**, so deleting their documentation now would make the wiki wrong in the opposite direction. Phase 7.4 writes an explicit handoff into `starter-jobs-retirement/requirements.md` so the removal happens with the code change rather than ahead of it or never.

**D13 — Inline `<action name="…"/>` markup, not `SCR_FieldManualPiece_Keybind`.**
The inline form is proven working with an Overthrow action in the shipped `#OVT-FieldManual_MainMenu_Text`. The dedicated keybind pieces are unproven against Overthrow's `chimeraInputCommon.conf` actions, and proving them is a spike with no payoff here.

**D14 — No `&` in display strings.**
Zero of the master stringtable's `Target_en_us` values contain `&`. Following the house convention costs nothing; discovering that the rich-text parser or an export pass dislikes it would cost a debugging session.

---

## 6. Definition of Done

Every row is observable by someone with no implementation context.

### Functional

| # | Criterion |
|---|---|
| F1 | Opening the Field Manual in-game shows **six** top-level entries in the left-hand list region: the base game's five categories plus an **Overthrow heading** |
| F2 | Under the Overthrow heading there are exactly **four clickable buttons**, reading (in English) "Getting Started", "Money and Trade", "Staying Hidden", "The Resistance" |
| F3 | Clicking each button shows its tiles: 4, 2, 2, 4 respectively — **twelve tiles total**, every one with a picture behind it and a legible title |
| F4 | Every one of the twelve entries opens to a page with **real prose**, not a placeholder sentence, not a raw `#OVT-` key (after the string-table export) |
| F5 | Triggering the `economy-first-buy` popup in-game and pressing **Learn more** still lands on the **Main Menu** page, not the manual's front page |
| F6 | The Main Menu page no longer contains the Map Info and Fast Travel sections; those now read as part of **The Map and Fast Travel** |
| F7 | Each of the twelve entries has a non-empty `m_Image` that resolves to a file present in `UI/Textures/FieldManual/` |
| F8 | Every entry title key in §3.3 appears verbatim in `Configs/FieldManual/Categories/FM_Overthrow.conf`, and `#OVT-FieldManual_MainMenu_Title` is byte-identical to its pre-feature form |

### Quality

| # | Criterion |
|---|---|
| Q1 | **Zero em-dash characters** in anything this feature authored: the `.st` file's new/changed items, `FM_Overthrow.conf`, and every wiki page edited. Verified by command, not by eye |
| Q2 | **Tone compliance, checkable per entry.** No sentence in any of the twelve entries is an imperative addressed to the player ("go", "you must now", "your next step"). "You can", "shops sell", "a wanted level rises when" are all fine. An independent reader can walk all twelve and mark each pass/fail |
| Q3 | Every string item added to `Language/localization_Overthrow.st` has a non-empty translator `Comment` describing context and constraints (where it appears, length budget, any rich-text markup it contains) |
| Q4 | No hardcoded English anywhere in `FM_Overthrow.conf` — every `m_sTitle` and `m_sText` is a `#OVT-` or `#AR-` key |
| Q5 | `git status` proves **no** `Language/localization_Overthrow.<lang>.conf` was modified |
| Q6 | `git status` proves `Configs/FieldManual/FieldManualConfigRoot.conf` and its `.meta` were **not** modified |
| Q7 | No duplicate GUID inside `FM_Overthrow.conf`, and every new GUID is in the reserved `6B3B…` block |
| Q8 | Wiki: **zero** pages deleted, **zero** hierarchy changes, **at most two** pages created, and every created page was preceded by a search that found no equivalent |

### Integration

| # | Criterion |
|---|---|
| I1 | The frozen title-key table (§3.3) exists in this document and is referenced from `context.md`, ready for `tutorial-content` to copy |
| I2 | `OVT_TEST_Init_FieldManual_DeltaMergesAndLinksResolve` passes, with three **added** branches and **zero weakened or deleted** assertions — verifiable by `git diff` on `OVT_TEST_InitSuite.c` |
| I3 | Each of the three new branches has a recorded proven-red entry in `context.md`: the exact breakage applied, the verbatim failure message, the date, and confirmation the breakage was reverted |
| I4 | Test counts are unchanged: Fast **47**, All **77** |
| I5 | For every system covered by both a manual entry and a wiki page, the two agree on **names, numbers and behaviour**. Any disagreement is either fixed or written down with a reason |
| I6 | The starter-jobs handoff section exists in `docs/features/new-player-experience/starter-jobs-retirement/requirements.md`, naming the page and the section |
| I7 | Every page in Phase 7's three groups has a verdict recorded in `context.md`'s audit table |
| I8 | The consolidated export list of new `#OVT-` ids is in `tasks.md`, collated from `git diff -U0 Language/localization_Overthrow.st` |

### Verification Method

**Automated — run these exact commands from the repo root:**

```bash
# 1. Compilation (exit 0 = verified clean)
tools/compile-check.sh

# 2. Tests. Expect "47 tests" and "78 tests" (78 = 77 + a concurrent session's untracked
#    Campaign case; see the baseline note in section 4). Both exit 0.
tools/run-tests.sh "{6A6E29FF47ECB840}"
tools/run-tests.sh "{6A6E2A002F53A581}"

# 3. Q5 - no runtime language export was touched. Expect ONLY localization_Overthrow.st.
git diff --stat Language/

# 4. Q6 - the root delta is untouched. Expect NO OUTPUT.
git status --porcelain Configs/FieldManual/FieldManualConfigRoot.conf Configs/FieldManual/FieldManualConfigRoot.conf.meta

# 5. Q1 - no em-dash entered authored content. Expect NO OUTPUT from both.
grep -n -- "—" Configs/FieldManual/Categories/FM_Overthrow.conf
git diff main...HEAD -- Language/localization_Overthrow.st | grep '^+' | grep -n -- "—"

# 6. Q7 - no duplicate ELEMENT GUID in the config. Expect NO OUTPUT.
#    Element position only. The naive whole-file form is a permanent FALSE POSITIVE: task 1a.6
#    mandates the same texture GUID {032FD1094792F0AC} on all twelve m_Image lines, and a texture
#    reference is not an element id. Corrected 2026-08-08 after Phase 1a hit it.
grep -oE '^\s*\S+ "\{[0-9A-F]{16}\}"' Configs/FieldManual/Categories/FM_Overthrow.conf | grep -oE '\{[0-9A-F]{16}\}' | sort | uniq -d

# 7. Q7 - every new GUID is in the reserved block. Every line printed must start {6B3B, {5990, {5992 or {032F/{7C78 (textures).
grep -oE '\{[0-9A-F]{16}\}' Configs/FieldManual/Categories/FM_Overthrow.conf | sort -u

# 8. F7 - every m_Image resolves. Each path printed must exist under UI/Textures/FieldManual/.
grep -oE 'UI/Textures/FieldManual/[a-z0-9_]+\.edds' Configs/FieldManual/Categories/FM_Overthrow.conf | sort -u | while read -r p; do test -f "$p" && echo "OK   $p" || echo "MISS $p"; done

# 9. F8 - all twelve title keys present.
grep -c 'm_sTitle "#OVT-FieldManual_' Configs/FieldManual/Categories/FM_Overthrow.conf   # expect 17 - see note below
grep    'm_sTitle "#OVT-FieldManual_' Configs/FieldManual/Categories/FM_Overthrow.conf   # eyeball against the table in section 3.3
```

*Note on check 9:* the count is 1 root category + 4 sub-categories + 12 entries = **17** `m_sTitle` lines, of which 12 are entry title keys. Confirm the twelve against §3.3 by reading the grep output; the raw number alone is not the assertion.

**Human play-test — the automation cannot reach any of this.**

⚠️ **Precondition: the user must export the string table in Workbench first.** Until then every new page renders a raw `#OVT-` key and F4 cannot be judged. This is the same blocker `tutorial-system` already owes.

1. Launch Overthrow (Workbench Play mode is sufficient — the Field Manual is client-local).
2. Open the Field Manual (main menu or pause menu, whichever route you normally use).
3. **F1:** count the left-hand list. Five base-game categories plus an **Overthrow** heading. The Overthrow line must be a **heading**, not a clickable button.
4. **F2:** confirm exactly four buttons under it, reading "Getting Started", "Money and Trade", "Staying Hidden", "The Resistance". None of them says "Introduction".
5. **F3:** click each of the four. Confirm the tile counts (4 / 2 / 2 / 4) and that **every tile has a background picture and a foreground image** — a blank tile means an `m_Image` or a tile background failed.
6. **F4:** open all twelve pages. Confirm real prose and **no raw `#OVT-` keys**. Note any page whose text overflows or clips.
7. **F6:** on the Main Menu page, confirm the Map Info and Fast Travel sections are gone, and that they appear on **The Map and Fast Travel** instead.
8. **F5 (the deep link, the important one):** start a fresh campaign or use a machine that has not seen the tip, buy anything from a shop to raise the `economy-first-buy` popup, press **Learn more**. It must open the Field Manual **directly on the Main Menu page** — not the front page, not the tile grid. Press Back once and confirm the manual closes entirely (`m_bOpenedFromOutside` behaviour, gotcha 29).
9. **Gamepad pass:** repeat steps 3-6 on a controller. Confirm the four buttons are reachable and the tile grid navigates.
10. **Q2 tone pass:** read all twelve entries end to end and mark any sentence that tells the player what to do. There should be none.

**Wiki verification (human, ~10 minutes):** open https://wiki.armaoverthrow.com and spot-check three pages edited in different phases against their manual entries. Names, numbers and behaviour must match. Confirm no page's structure was rewritten and no page is missing.

---

## 7. Testing Strategy

### 7.1 What automation covers

| Layer | Covers | Where |
|---|---|---|
| Compile | Nothing in this feature except the Phase 1b test edit — `.conf` and `.st` are data | `tools/compile-check.sh` |
| Init tier, existing branches | The delta still merges (≥5 vanilla categories + Overthrow's); tile backgrounds survive; Overthrow's category has sub-categories; no sub-category reuses vanilla's Introduction key; every tutorial deep link resolves | `OVT_TEST_Init_FieldManual_DeltaMergesAndLinksResolve` |
| Init tier, **new** branches (Phase 1b) | No Overthrow entry has empty content (the pruning trap); the four sub-categories are present; Overthrow entry titles are unique across the merged manual | Same case, branches A / B / C |

The case loads through `SCR_FieldManualConfigLoader.LoadConfigRoot` — the menu's own entry point — and is side-effect free because `SCR_ConfigHelperT.GetConfigObject` builds a fresh instance per call.

### 7.2 The fallibility rule

Project rule: **every test case must be proven able to fail once, and the method recorded.** No `maxAttempts`, ever. The three new branches each get a realistic breakage (not a synthetic `if (false)`), the verbatim failure text captured, and the breakage reverted immediately. Recorded in `context.md`'s proven-red table beside the existing three branches.

### 7.3 What automation structurally cannot cover

Rendering, tile art, focus order, text overflow, gamepad navigation, and the `MenuManager` ordering property that makes `OpenMenu` → `OVT_OpenEntryByTitle` land on a page rather than the front page. All of it is in §6's human play-test, steps 1-10, and goes into `tasks.md` under **Needs Human Verification**.

### 7.4 Not needed

- **No MP/JIP testing.** The Field Manual is a client-local menu reading a client-local config. `tools/launch-server.sh` and `tools/launch-game.sh` are not part of this feature's gate, and **no client launch should be suggested to the user for it** (a client launch opens a window on their desktop).
- **No performance testing.** Twelve entries against the base game's 140.
- **No persistence testing.** Nothing here is saved.

---

## 8. Dependencies

### Consumed (all already shipped)

| Dependency | State | What this feature relies on |
|---|---|---|
| `tutorial-system` Phase 7 | 🟢 Shipped 2026-08-07 | `FM_Overthrow.conf` exists as the content home; the root delta is reduced to one element; `OVT_OpenEntryByTitle` / `OVT_FieldManualHelper.Open` exist; the Init guard exists |
| Base-game field-manual framework | Vanilla | `SCR_FieldManualConfigCategory`, `..._Entry_Standard`, `..._Piece_{Text,Header,Image}`, `SCR_FieldManualConfigLoader` |
| `Language/localization_Overthrow.st` | Live | The only editable localization surface |
| wikijs MCP tools | Live | `help-docs-sync`'s wiki half. If unreachable, the in-game half still proceeds and the wiki work is reported pending |

### Blocking nothing / blocked by nothing in code

There is **no code dependency** on `tutorial-system`; the two were always parallel-buildable. What this feature needs from it was delivered in Phase 7.

### Provides

| Consumer | What it gets | When |
|---|---|---|
| `tutorial-content` | Twelve frozen `m_sFieldManualTitleKey` targets (§3.3) | **End of Phase 1a** — not Phase 5 |
| `first-spawn` | `#OVT-FieldManual_Welcome_Title` as a Learn-more target for the welcome sequence | End of Phase 1a |
| `starter-jobs-retirement` | A written handoff of the `getting-started` wiki page's starter-jobs section | End of Phase 7 |

### Human dependencies

| # | Owed by the user | Blocks |
|---|---|---|
| H1 | **Export the string table in Workbench** after each content phase (or once at the end) | All visual verification. Until then every new page renders raw `#OVT-` keys |
| H2 | Capture and import the twelve screenshots (§4.1) | Phase 6 only. **Never blocks the feature** — D7's fallback ships regardless |
| H3 | Run the play-test checklist (§6) | The feature's completion verdict |

---

## 9. Risks & Mitigation

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| **R1** | An entry ships with empty `m_aContent`, is silently pruned, and its frozen link id resolves to nothing | Medium (it is the exact shape of "I'll fill this in later") | High — a dead link that nobody notices for months | D6 makes real content mandatory at creation; Phase 1b branch A turns it into a red build immediately, before `tutorial-content` exists to catch it |
| **R2** | A title key is typo'd or renamed after a popup links to it | Medium | High — silent front-page fallback, never errors | `#OVT-FieldManual_` prefix + branch C (uniqueness) + the pre-existing branch 6 (link resolution). §3.2 states the immutability rule in the table the consumer copies |
| **R3** | The string table is never exported, so the manual ships as a wall of `#OVT-` keys | Medium — it is already an outstanding debt from `tutorial-system` | High — visibly broken | Consolidated export list in `tasks.md` (Phase 8.7), collated from the diff rather than from reports; H1 named as an explicit human dependency; every play-test step gated on it |
| **R4** | The Phase 7 audit exhausts an agent's context and a page is silently skipped | Medium | Medium — a stale page survives | Three separate runs by group (7A/7B/7C); every page gets an explicit verdict row in `context.md`, so a skip is visible as a missing row rather than an absence of complaint |
| **R5** | Screenshots never arrive | Medium | Low | D7's universal fallback. Phase 6 is partial-credit by design; the feature is done without it |
| **R6** | Authored content states behaviour that is not true (a cost, a threshold, a mechanic that changed) | Medium — this is the classic documentation failure | High — worse than no documentation | Every entry names its source file in §3.3; the phase task says "verify against the source, never document unverified behaviour"; wiki and manual written from the same reading so a mismatch surfaces during authoring |
| **R7** | An em-dash or an instructional sentence slips into content | Medium | Low-Medium — a house-style break and a sandbox-tone break | Q1's grep runs in Phase 8; Q2 is a per-entry human read of twelve short pages; the `help-docs-sync` agent carries both rules in its own instructions |
| **R8** | A `Language/localization_Overthrow.<lang>.conf` export gets hand-edited | Low — a deny rule exists on one machine, and the agent's rules forbid it | **Severe** — six files were silently corrupted this way once | Q5's `git diff --stat Language/` gate in Phase 8, and the same check named in every phase's acceptance |
| **R9** | Someone "tidies" the root delta or its element GUID | Low | **Severe** — Overthrow's category replaces vanilla's Introduction, five categories instead of six | D1; Q6's `git status --porcelain` gate; the pre-existing Init branch that measures the vanilla floor |
| **R10** | The wiki's `getting-started` starter-jobs content is deleted early, or forgotten forever | Medium | Medium — the wiki is wrong in one direction or the other | D12 splits it explicitly: de-version now, delete never, hand off in writing (Phase 7.4, DoD I6) |
| **R11** | Two sessions edit `FM_Overthrow.conf` concurrently (the repo has had parallel sessions before) | Low-Medium | Medium — merge conflict in a brace-sensitive file | Phases are sequential and each owns a disjoint set of entries; Phase 8's duplicate-GUID grep catches a bad merge |

---

## 10. Quality Bar

This is a **content and documentation** feature. Runtime reliability is not the axis it is judged on; being *true*, *readable*, *translatable* and *consistent across surfaces* is.

| Descriptor | Standard | How it is judged |
|---|---|---|
| **Factual accuracy** | Every claim in every entry is traceable to a source file, and the source was read during authoring. No number is quoted from memory, from the wiki, or from an older doc | §3.3 names a source per entry; spot-check three claims per phase against the cited file |
| **Tone compliance** | Inform, never instruct. No imperatives, no implied order, no goals. An entry reads correctly whatever the player did before | DoD Q2 — a per-sentence read of twelve short pages |
| **Link integrity** | Twelve title keys, unique, frozen, documented in one table, machine-checked | DoD I1-I2, Init branches B and C |
| **Translator-friendliness** | Every new string has a `Comment` that says where it appears, how long it can be, and what markup it contains. Existing translated strings are reused rather than replaced | DoD Q3, D5 |
| **Wiki / in-game agreement** | The same system described on both surfaces uses the same names and the same numbers. The wiki is the longer form, not a different story | DoD I5; enforced structurally by authoring both from one reading |
| **Surgical editing** | Wiki pages are edited in place, preserving voice and structure. `.conf` edits touch only the elements they need and never the root delta | DoD Q6, Q8 |
| **Reversibility** | Every entry can be retired with `m_bEnabled 0`; no key is ever recycled; no GUID is ever reused | §3.2 retirement rule; DoD Q7 |
| **Honest reporting** | A page that could not be verified is flagged, not guessed. A phase that could not finish says so | Phase 7.3; every phase's report names what it deliberately left |

---

## Appendix A — Corrections to the received brief

Recorded because the brief will be read again by other agents.

1. **The Init test does not assert an entry count.** The brief said branch 1 asserts "a measured entry count (141 at the time of writing)" and that adding entries would therefore require changing it. It does not: `VANILLA_CATEGORY_FLOOR = 5` is an explicit floor, and the 141 lives only in `DescribeRoot()`, a `Print` that runs before any assertion. **No existing assertion needs updating.** The Phase 1b hardening is an addition made for a different reason (D8).
2. **Fast is 47 and All is 77, not 38 / 66.** Measured on this working tree, 2026-08-08. `CLAUDE.md`'s numbers are stale; `tutorial-system`'s "Fast 47 / All 75" was correct at the time and `main` has since added two Campaign cases.
3. **The stringtable master contains zero `&` characters** in any `Target_en_us`. The user's "Money & Trade" ships as "Money and Trade" (D14).
4. **`FieldManualConfigRoot.conf.meta` declares five platform configurations, not six** (PC, XBOX_ONE, XBOX_SERIES, PS4, HEADLESS — no PS5). `FM_Overthrow.conf.meta` declares six, including PS5. Neither is edited by this feature; noted so nobody "fixes" the asymmetry into a conflict.
5. **`SCR_FieldManualPiece_Image`, `_Separator` and `_LineBreak` are `insertable: false`** — they cannot be added through the Workbench property picker, only hand-written into the `.conf`. The shipped entry already hand-writes an `_Image`, so this is proven, but a Workbench user will not find them in the dropdown.
