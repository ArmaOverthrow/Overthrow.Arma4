# Tutorial Content — Implementation Plan

**Epic:** new-player-experience (feature #3 of 5)
**Status:** 🟢 **COMPLETE** (24/24 tasks; all automated gates green; play-test passed and signed off 2026-08-09; string export done; wiki edit applied)
**Started:** 2026-08-09
**Target Completion:** 2026-08-09 (build complete)
**Last Updated:** 2026-08-09

**Requirements:** `docs/features/new-player-experience/tutorial-content/requirements.md`
**Contracts consumed:** `tutorial-system/implementation.md` §5 (entry ids, string keys, trigger catalog, add-an-entry procedure, Rule 0) · `field-manual/implementation.md` §3.2–3.3 (the frozen link keys)

---

## 1. Executive Summary

The tutorial framework shipped with two proof entries. This feature authors the **ten entries that make it worth having**: six that a player meets in their first hour (home, money and shops, gun dealers, the map and fast travel, the wanted system, skills and levelling) and four they meet at their first escalation (recruits, placing and camps, building, base capture).

It is **content, not code**. Nine new `.conf` files under `Configs/Tutorials/`, one adopted and rewritten existing entry, ~20 string items in `Language/localization_Overthrow.st`, nine one-line elements on the game-mode prefab, and a surgical wiki touch. **Zero EnforceScript** is expected: `tutorial-system` proved the add-an-entry path needs none (`welcome-intro` was 1 conf + 5 strings + 1 prefab line), and if this feature finds a trigger it needs and cannot express, that is a `tutorial-system` gap reported upward, never a hack here.

Two things make this cheaper and safer than it looks:

1. **The fact-check corpus already exists.** `field-manual` shipped twelve reference pages whose prose was verified against source in Phases 2–5 and exported on 2026-08-08. Each tutorial body is a one-or-two-sentence compression of an already-verified paragraph, so Rule 0 evidence is *inherited and re-checked* rather than derived from scratch — and every claim still gets a `file:line`.
2. **The gates are already built.** `OVT_TEST_Init_Tutorial_ManagerResolvesAndLoadsEntries` fails the build on an empty, duplicate, page-less or trigger-less entry (`OVT_TEST_InitSuite.c:1605-1633`), and `OVT_TEST_Init_FieldManual_DeltaMergesAndLinksResolve` fails it on a link key that resolves to no manual page (`:2200-2212`). Both walk **all** entries, so nine additions are guarded the moment they are registered. This plan adds no test cases to duplicate them.

The one thing no gate can catch is a well-formed lie, which is why Rule 0 is a per-phase acceptance criterion here, not advice.

**Every content-writing phase is executed by the `help-docs-sync` agent** — user instruction, 2026-08-08. Its definition already encodes the tone rules, the localization hard rules and the wiki conventions, and its toolset covers `.st` edits, `.conf` creation, prefab-array appends and running `tools/compile-check.sh` / the Fast tier. No phase in this plan needs an advanced (max-effort) agent; `/proceed` must respect the `help-docs-sync` routing rather than substituting `component-developer`.

---

## 2. Goals

### Primary

1. **Ten entries, all triggered by the player's own action**, covering the six early-game and four mid-game topics the requirements name.
2. **Every entry carries a working "Learn more"** — all ten link to one of the twelve frozen field-manual pages, matched exactly and case-sensitively.
3. **Every sentence is true.** Every claim traceable to a `file:line` (or to an already-verified field-manual string, itself cited), with the evidence recorded in the string item's `Comment` field.
4. **Sandbox tone held throughout.** Inform, never instruct. No imperatives, no implied order, no "now go and…". Each entry reads correctly no matter what the player did before it.
5. **Per-player triggers.** Nine of the ten hang off per-player or client-local events; the tenth (base capture) uses the framework's proximity fan-out and says so in this plan rather than pretending otherwise.
6. **Restrained volume.** Ten entries across a whole campaign, each shown once per machine, forever. Six of them are reachable in the first hour and only if the player actually does the six things.

### Secondary

7. **Record the starter-job coverage mapping** in this feature's docs — `starter-jobs-retirement` cannot start without it.
8. **Adopt rather than duplicate** the shipped `economy-first-buy` proof entry: keep its immutable id, rewrite its body as the real money-and-shops entry, repoint its link at the new Shops page.
9. **Leave a shorter path for the next author**: the traps found (below, §3.7) written down so nobody re-derives them.

### Explicitly out of scope

- **Late-game entries** — warehouse, port, importing, real estate purchase, loadouts, vehicle upgrades, donations. Deferred content pass, per requirements.
- **The first-spawn welcome sequence** (`first-spawn` owns `welcome-intro`, `#OVT-IntroHint` and `m_aHintedPlayers`). This feature does not touch `Configs/Tutorials/proofWelcome.conf`.
- **Framework changes of any kind** — no new trigger events, no schema fields, no UI, no keybindings, no layout edits.
- **Removing the starter jobs** (`starter-jobs-retirement`), including the wiki's `**Tutorial Jobs**` paragraph on `getting-started`, which `field-manual` deliberately left standing and handed to that feature.
- **A second wiki staleness sweep.** `field-manual` swept every player-facing page on 2026-08-08. This feature edits a page only when its own fact-check proves that page wrong.
- **New field-manual pages.** All ten topics already have one.

---

## 3. Architecture Overview

There is no architecture to design — the framework is fixed and this feature fills a data array. What needs designing is **the entry set**: which topics, which triggers, which links, and how the ten of them behave as a group over one campaign.

### 3.1 The entry set

Ten entries. All `NONMODAL`, all `m_iPriority 0`, all single-page, all `m_bEnabled 1`. One is adopted; nine are new.

| # | Entry id (immutable) | Presentation | Trigger event | Filter / threshold | "Learn more" link key | Content intent (one line) | Starter job it covers |
|---|---|---|---|---|---|---|---|
| 1 | `home-first-open` | NONMODAL | `MENU_OPENED` | `OVT_RealEstateContext` | `#OVT-FieldManual_YourHome_Title` | Ownership is recorded against the player, not the session, so a building you own stays yours across restarts; a home is where you come back to | — |
| 2 | `economy-first-buy` **(ADOPT)** | NONMODAL | `PLAYER_BUY` | none | `#OVT-FieldManual_Shops_Title` **(repointed)** | Shops both buy and sell, stock is finite and replenishes, and money is a balance on your record rather than something you carry | `findShop` |
| 3 | `shops-first-gun-dealer` | NONMODAL | `PLAYER_TRANSACTION` | `SHOP_GUNDEALER` | `#OVT-FieldManual_GunDealers_Title` | A gun dealer trades weapons and military equipment, draws on its own stock list and restocks separately from ordinary shops | `findGunDealer` |
| 4 | `map-first-open` | NONMODAL | `MAP_OPENED` | none | `#OVT-FieldManual_MapAndTravel_Title` | The map marks what you own, the resistance's camps and FOBs, and the shops, dealers and ports it knows about; carrying a map is what makes map info and fast travel usable | `findShop` / `findGunDealer` (the *discovery* half) |
| 5 | `wanted-first-level` | NONMODAL | `PLAYER_WANTED` | none (see D6) | `#OVT-FieldManual_WantedSystem_Title` | While your wanted level is above zero the occupying faction comes looking; some things have to be seen to count, violence does not, and the level only falls while nobody is watching | — |
| 6 | `skills-first-open` | NONMODAL | `MENU_OPENED` | `OVT_CharacterSheetContext` | `#OVT-FieldManual_Skills_Title` | Experience comes in from most resistance work, every level past the first is worth one point, and points buy skill levels on this screen | — |
| 7 | `recruits-first-recruit` | NONMODAL | `PLAYER_RECRUIT_ADDED` | none | `#OVT-FieldManual_Recruits_Title` | A recruit belongs to the player who hired them and stays with that player between sessions | `recruitACivilian` |
| 8 | `place-first-placeable` | NONMODAL | `PLAYER_PLACE` | none (see D5) | `#OVT-FieldManual_Camps_Title` | Placing puts resistance objects into the world for money, from small items up to a full camp; some are illegal, and being seen placing one is noticed | `placeEquipmentBox`, `placeACamp` |
| 9 | `build-first-structure` | NONMODAL | `PLAYER_BUILD` | none | `#OVT-FieldManual_FOBs_Title` | Building adds structures where the resistance already holds ground: a base it controls, a FOB or a camp, and what is on offer depends on which of those you are standing in | — |
| 10 | `bases-first-capture` | NONMODAL | `BASE_CONTROL_CHANGE` | none (global, 300 m fan-out) | `#OVT-FieldManual_BaseCapture_Title` | Bases change hands only after a fight; one under resistance control weakens the occupiers in that area, and they may spend reserves to take it back | — |

**Id derivation for string keys** (`#OVT-Tutorial_<PascalId>_Title` / `_Body`, per §5 of the framework contract):

| Entry id | Title key | Body key |
|---|---|---|
| `home-first-open` | `#OVT-Tutorial_HomeFirstOpen_Title` | `#OVT-Tutorial_HomeFirstOpen_Body` |
| `economy-first-buy` | `#OVT-Tutorial_EconomyFirstBuy_Title` *(exists)* | `#OVT-Tutorial_EconomyFirstBuy_Body` *(exists, rewritten)* |
| `shops-first-gun-dealer` | `#OVT-Tutorial_ShopsFirstGunDealer_Title` | `#OVT-Tutorial_ShopsFirstGunDealer_Body` |
| `map-first-open` | `#OVT-Tutorial_MapFirstOpen_Title` | `#OVT-Tutorial_MapFirstOpen_Body` |
| `wanted-first-level` | `#OVT-Tutorial_WantedFirstLevel_Title` | `#OVT-Tutorial_WantedFirstLevel_Body` |
| `skills-first-open` | `#OVT-Tutorial_SkillsFirstOpen_Title` | `#OVT-Tutorial_SkillsFirstOpen_Body` |
| `recruits-first-recruit` | `#OVT-Tutorial_RecruitsFirstRecruit_Title` | `#OVT-Tutorial_RecruitsFirstRecruit_Body` |
| `place-first-placeable` | `#OVT-Tutorial_PlaceFirstPlaceable_Title` | `#OVT-Tutorial_PlaceFirstPlaceable_Body` |
| `build-first-structure` | `#OVT-Tutorial_BuildFirstStructure_Title` | `#OVT-Tutorial_BuildFirstStructure_Body` |
| `bases-first-capture` | `#OVT-Tutorial_BasesFirstCapture_Title` | `#OVT-Tutorial_BasesFirstCapture_Body` |

The **content intent** column above is a paraphrase for planning, not copy to lift. Final wording is the authoring phase's job, after the fact-check, in the sandbox voice and with no em-dashes.

**18 new string items** (9 titles + 9 bodies) plus **2 rewritten** (`EconomyFirstBuy_Title` may keep its text; `_Body` is rewritten). No chrome keys — the framework owns all ten of those and they already exist and are exported.

### 3.2 The frozen field-manual link table

Copied verbatim from `field-manual/implementation.md` §3.3 as that section instructs. **Match is exact and case-sensitive**; an entry's `m_sTitle` key *is* its link id.

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

Ten of the twelve are used. **Row 1 (Welcome) and row 2 (Main Menu) are not linked by this feature**: Welcome belongs to `first-spawn`'s welcome sequence, and Main Menu is currently linked from `proofFirstBuy.conf` — the link this feature repoints at row 5. That leaves row 2 unlinked from any tip, which is fine (an unlinked manual page is still reachable normally) and is called out so nobody reads it as an oversight.

### 3.3 Trigger reference — what each chosen trigger actually carries

Verified against shipped code on 2026-08-08. This is the table the authoring agent needs; the full catalog is `tutorial-system/implementation.md` §5.

| Event used | Raised by | `m_sFilter` is | Value used here |
|---|---|---|---|
| `MENU_OPENED` | `OVT_UIContext.ShowLayout` → `OVT_TutorialComponent.NotifyMenuOpened(ClassName())` (`Scripts/Game/UI/OVT_UIContext.c:169`) | the opening context's runtime class name | `OVT_RealEstateContext`, `OVT_CharacterSheetContext` — both exist under `Scripts/Game/UI/Context/` |
| `MAP_OPENED` | `SCR_MapEntity.GetOnMapOpen()` → `OVT_TutorialComponent.OnMapOpened` (`OVT_TutorialComponent.c:264-267`) | `""` | none |
| `PLAYER_BUY` | `OVT_EconomyManagerComponent.m_OnPlayerBuy`, invoked at exactly **one** site — the shop purchase path (`OVT_PlayerCommsComponent.c:688`) | `""`; `m_iValue` = actual cost | none |
| `PLAYER_TRANSACTION` | `m_OnPlayerTransaction` (`OVT_PlayerCommsComponent.c:691` buy, `OVT_ShopTransactionComponent.c:377` sell) | `SCR_Enum.GetEnumName(OVT_ShopType, shop.m_ShopType)` (`OVT_TutorialManagerComponent.c:253-257`) → the bare enum member name | `SHOP_GUNDEALER` (`OVT_ShopComponent.c:5-14`) |
| `PLAYER_PLACE` | `OVT_ResistanceFactionManager.m_OnPlace`, invoked at exactly one site inside `PlaceItem()` (`:756`) | `placeable.m_sName` verbatim (`OVT_TutorialManagerComponent.c:265-271`) — plain English in `Configs/Resistance/placeables.conf`: `Poster` (:4), `Camp` (:30), `Ammobox` (:45), `Sandbags` (:55), `Hedgehog` (:73), `Floodlights` (:84), `Signs` (:94), `Furniture` (:120), `PirateRadio` (:146) | none (unfiltered) |
| `PLAYER_BUILD` | `m_OnBuild`, invoked at exactly one site inside `BuildItem()` (`:854`) | `buildable.m_sName` verbatim — the complete set is `Guard Tower` (:4), `Recruitment Tent` (:17), `Medical Tent` (:29), `Vehicle Maintenance Ramp` (:41), `Bunkers` (:54), `Garage` (:67), `Helipad` (:79) in `Configs/Resistance/buildables.conf`, and no override adds to it | none (unfiltered) |
| `PLAYER_RECRUIT_ADDED` | `OVT_RecruitManagerComponent.m_OnRecruitAdded` — server site `:489` inside `AddRecruit()` is the genuine "a civilian just joined"; `:2336` is the broadcast client mirror and never double-fires on the server. playerId is looked back up from `m_sOwnerPersistentId`, and an offline owner resolves to `-1` and is dropped (`OVT_TutorialManagerComponent.c:295-302`) | `""` | none |
| `PLAYER_WANTED` | `OVT_PlayerWantedComponent.GetOnWantedLevelChanged()` (static invoker `:46`, fired `:169`), **escalation only** — the decay path deliberately does not fire | `""`; `m_iValue` = the new level | none (see D6) |
| `BASE_CONTROL_CHANGE` | `OVT_OccupyingFactionManager.m_OnBaseControlChanged` (`:1375`, reached from `OVT_BaseControllerComponent.SetControllingFaction` at `:146`) → `DispatchToPlayersNear(..., NEAR_RADIUS_BASE = 300, ...)` (`OVT_TutorialManagerComponent.c:34, 345-355`) | `""` | none — **global**, no acting player |

**Every trigger in this set is restore-safe** — verified per invoker, so no tip can fire at save-load for a returning campaign:

- `m_OnPlace` / `m_OnBuild` are only reachable from `PlaceItem()` / `BuildItem()`, which are only called from two player RPCs (`OVT_PlayerCommsComponent.c:1117`, `:1142`); persistence restores placed and built objects through serializers, and camps/FOBs through `ApplyPersistedResistance()` (`OVT_ResistanceFactionManager.c:244`), neither of which invokes anything.
- `m_OnRecruitAdded` is not fired by `ApplyPersistedRecruits()` (`OVT_RecruitManagerComponent.c:327-384`), by the respawn path, or by `RpcDo_RecruitUpdated`.
- `m_OnBaseControlChanged` is suppressed on restore: the restore path passes `suppressEvents = true` (`OVT_OccupyingFactionManager.c:698`).
- `m_OnPlayerSkill` (unused here) is only fired from `AddSkillLevel` (`:128`), which no restore path calls.

**Not used, deliberately:** `PLAYER_SELL` (also fires from `AddPlayerMoney`, so it does not mean "sold at a shop" — the framework contract says prefer `PLAYER_TRANSACTION`), `PLAYER_SKILL` (fires only when a point is *spent*, see D4), `TOWN_CONTROL_CHANGE` (global with no acting player, and unlike a base capture the nearby player usually did not cause it), `PLAYER_SPAWNED` (`first-spawn`'s).

**Behaviour that will look like a bug and is not:** `MENU_OPENED` is fired *after* `Enable()`, and `MAP_OPENED` while the map is open, so the gate correctly holds the popup back and it appears **when the screen closes** (`OVT_UIContext.c:165-169`; gate reasoning in `OVT_TutorialComponent`). Same for `PLAYER_BUY`: the tip lands after the shop menu closes. Every play-test step below expects this.

### 3.4 Where "Learn more" actually lives

This matters to the Definition of Done, and it is not obvious. The non-modal HUD popup has **no Learn more button** — its only control is one input prompt (`TutorialMoreButton`, label `#OVT-Tutorial_MoreInMenu` = "Overthrow Menu") bound to the existing `OverthrowMainMenu` action, because `tutorial-system` R3 established there is no free gameplay input (`OVT_TutorialInfo.c:505-539`). Pressing it hands the entry to `OVT_TutorialContext` and opens the Overthrow menu; the menu's **Tips** entry then opens the same tip as a modal, and *that* is where **Learn More** appears (`OVT_TutorialContext.c:499-500`, visible only when `m_sFieldManualTitleKey != ""`).

So the route is: **HUD tip → "Overthrow Menu" prompt → main menu → Tips → modal → Learn More → the manual page.** The prompt deliberately does **not** mark the entry seen (`OVT_TutorialInfo.c:527-539`), so a player who goes reading still gets a proper dismissal afterwards. All ten entries are NONMODAL, so this is the route for all ten, and the play-test checklist uses it.

### 3.5 Starter-job coverage mapping

`starter-jobs-retirement` cannot begin without this, and its requirements name it as a precondition. The mapping below is the plan's proposal; Phase 3 records the **as-built** version in this feature's `context.md` and appends a pointer to `starter-jobs-retirement/requirements.md`, the way `field-manual` handed over its own note.

| Starter job (`Configs/Jobs/`) | What it taught | Covered by | Gap, if any |
|---|---|---|---|
| `findShop.conf` | shops exist; where one is | `economy-first-buy` (what a shop does) + `map-first-open` (shops are marked on the map) | The job placed a **marker**; the map tip only says the map knows about shops. Shop icons are created per shop type in `Scripts/Game/UI/Map/OVT_MapIcons.c:136-184`, so discovery is covered by the map itself, not by a popup |
| `findGunDealer.conf` | gun dealers exist; where one is | `shops-first-gun-dealer` + `map-first-open` | Same shape. ~~**Note:** `SHOP_GUNDEALER` has no dedicated map icon~~ — **corrected 2026-08-09**: gun dealers *do* get their own `"gundealer"` sprite via a separate enumeration path (`OVT_MapIcons.c:627-632`, `:111-134`), so a tip *may* say they are distinctly marked. See the struck §3.7 row |
| `placeEquipmentBox.conf` | the Place menu; placing equipment | `place-first-placeable` (unfiltered `PLAYER_PLACE`, so an `Ammobox` fires it) | none |
| `placeACamp.conf` | camps | `place-first-placeable` (body names the camp as the top of the range) + the Camps manual page | none |
| `recruitACivilian.conf` | recruiting civilians | `recruits-first-recruit` | Fires on the **first recruit gained**, not on the option becoming available; a player who never talks to a civilian never sees it — same as the job, which they could also ignore |

### 3.6 File inventory

```
Configs/Tutorials/
├── proofFirstBuy.conf                 ← EDITED (adopt: body key kept, link repointed). Filename NOT renamed (D3)
├── homeFirstOpen.conf         (+ .meta)   NEW
├── shopsFirstGunDealer.conf   (+ .meta)   NEW
├── mapFirstOpen.conf          (+ .meta)   NEW
├── wantedFirstLevel.conf      (+ .meta)   NEW
├── skillsFirstOpen.conf       (+ .meta)   NEW
├── recruitsFirstRecruit.conf  (+ .meta)   NEW
├── placeFirstPlaceable.conf   (+ .meta)   NEW
├── buildFirstStructure.conf   (+ .meta)   NEW
└── basesFirstCapture.conf     (+ .meta)   NEW

Language/
└── localization_Overthrow.st          ← the ONLY localization file this feature edits (18 new items, 1-2 rewritten)

Prefabs/GameMode/
└── OVT_OverthrowGameMode.et           ← 9 elements appended to OVT_TutorialManagerComponent.m_aEntries (currently :212-219)

docs/features/new-player-experience/
├── tutorial-content/{implementation,tasks,context}.md
└── starter-jobs-retirement/requirements.md   ← appended coverage-mapping pointer (Phase 3)

Wiki (wikijs MCP): at most 2 pages UPDATED, 0 CREATED (D10)

NOT TOUCHED: any file under Scripts/ · UI/Layouts/ · Configs/FieldManual/ · Configs/System/chimeraInputCommon.conf ·
             Configs/Tutorials/proofWelcome.conf · Language/localization_Overthrow.<lang>.conf
```

**GUID block: `6B3C0000…` is reserved for this feature.** Verified free on 2026-08-08: `grep -rEoh "\{6B3C[0-9A-F]{12}\}"` across the repo returns **0** (for comparison `6B3A…` = 150, tutorial-system; `6B3B…` = 237, field-manual). Allocate one 16-slot row per entry so the mapping stays readable:

| Entry | conf resource (`.meta` Name) | page object | trigger object | prefab element |
|---|---|---|---|---|
| `home-first-open` | `{6B3C000000000010}` | `{6B3C000000000011}` | `{6B3C000000000012}` | `{6B3C000000000013}` |
| `shops-first-gun-dealer` | `{6B3C000000000020}` | `…0021` | `…0022` | `…0023` |
| `map-first-open` | `{6B3C000000000030}` | `…0031` | `…0032` | `…0033` |
| `wanted-first-level` | `{6B3C000000000040}` | `…0041` | `…0042` | `…0043` |
| `skills-first-open` | `{6B3C000000000050}` | `…0051` | `…0052` | `…0053` |
| `recruits-first-recruit` | `{6B3C000000000060}` | `…0061` | `…0062` | `…0063` |
| `place-first-placeable` | `{6B3C000000000070}` | `…0071` | `…0072` | `…0073` |
| `build-first-structure` | `{6B3C000000000080}` | `…0081` | `…0082` | `…0083` |
| `bases-first-capture` | `{6B3C000000000090}` | `…0091` | `…0092` | `…0093` |

Stringtable items take `{6B3C0000000001xx}`, allocated sequentially in authoring order.

### 3.7 Known Rule 0 traps (read before writing a word)

`field-manual`'s source audit left sixteen open questions in `field-manual/context.md:163-179`. Five of them are landmines for exactly the sentences this feature wants to write. **Each is a claim that sounds right, reads well and is false or unproven.**

| Tempting sentence | Why it is a trap | Source |
|---|---|---|
| ~~"Prices move with the town — the scarcer an item is nearby, the more it costs."~~ **STRUCK 2026-08-09 — this row was wrong; the claim is TRUE.** | The integer-division diagnosis was **disproved at runtime** by BUG-105 on `main`: a Campaign case measured `1116 -> 1067 -> 1018` across stock 1/50/100, which is the float-division curve, not a `+10%/+0%` step. EnforceScript's `int / int` does **not** universally truncate like C — nested in an expression containing float operands it evaluates fractionally. (Contrast BUG-024, where `57 / 15` in a pure-int context genuinely truncated: the behaviour depends on the enclosing expression's type context.) The division is now written `(float)stock_level / max_stock` and pinned by a regression case. **The scarcity gradient is real.** The shipped tip's "prices differ from town to town" is still true and was left unchanged; the withdrawn instruction was the *do-not-say-curve* one | BUG-105 resolution; `OVT_EconomyManagerComponent.c:551` |
| "A camp cannot be placed near an occupying base." | Never enforced for camps: `OVT_PlaceContext.CanPlace:267-280` returns true as soon as the away-from-camps check passes, so the away-from-bases branch is unreachable, and the server re-checks only item limits | `field-manual/context.md:172` |
| "Your wanted level runs from one to five." | Levels **1 and 5 are unreachable by escalation** — every escalation site sets 2, 3 or 4; level 1 exists only as the last decay step, and nothing sets 5 even though five stars are drawn | `field-manual/context.md:170` |
| "Gun dealers also stock ammunition for the occupying faction's weapons." | Unresolved: `GunDealerConfig.conf` sets `m_bIncludeOccupyingFactionItems 0` on the ammunition rules, but calibre-to-faction membership is runtime data. The manual says only "Most equipment belonging to the occupying faction is not traded" | `field-manual/context.md:167` |
| ~~"Gun dealers show on the map with their own icon."~~ **STRUCK 2026-08-09 — this row was wrong; the claim is TRUE.** | It is correct that `SHOP_GUNDEALER` is absent from the icon switch in `TryCreateShopIcon`, but **gun dealers never reach that function**: they are enumerated separately from `economy.GetGunDealers()` (`OVT_MapIcons.c:360, 627-632`) and drawn by `TryCreateGunDealerIcon`, which loads a dedicated `"gundealer"` sprite (`:111-134`). `#OVT-FieldManual_GunDealers_Text2` is correct and needs no change. Found by the Phase 1 fact-check; verified independently before striking | `OVT_MapIcons.c:75, 111-134, 627-632` |
| "Building costs supplies." | Building costs **money** — `m_iCost` × a difficulty multiplier (`OVT_OverthrowConfigComponent.c:255-258`), charged at `OVT_ResistanceFactionManager.c:851`. There is no supply resource in the build path | verified 2026-08-08 |
| "Capture a base by taking its flag / planting something." | Control flips purely on the **QRF point score**: occupying-faction AI versus players counted inside 220 m every 10 s until a difficulty threshold is crossed (`OVT_QRFControllerComponent.c:20, 110-188`), then `ChangeBaseControl` (`OVT_OccupyingFactionManager.c:893-915`). The flag changing is a *consequence* of the faction affiliation change (`OVT_BaseControllerComponent.c:89-115, 143-150`), never a cause. Practically: clear the garrison inside 220 m and stay there | verified 2026-08-08 |
| "Skills like Trade, Stealth, Logistics, Medical…" | There are exactly **three** skills, and only three: `Trade`, `Stealth`, `Diplomacy` (`Configs/Player/overthrowSkills.conf:4, 58, 103`), with no override adding any. Name three or name none | verified 2026-08-08 |

Plus the two lies that already shipped once and must never come back (`tutorial-system/implementation.md` Rule 0): money can be **stolen** or **deposited** (neither mechanic exists — `OVT_PlayerData.money` is a persisted `int` on the player record), and the Overthrow menu contains a **money screen** or the **Field Manual** (it contains neither).

### 3.8 The one requirement with no trigger: FOB deployment

The requirements ask for "FOB basics & building (**first FOB deployed**; first build)". The first half is **not expressible with the shipped catalog**, established 2026-08-08:

- `DeployFOB()` (`OVT_ResistanceFactionManager.c:466`) is a separate code path that touches **neither** `m_OnPlace` (`:756`) nor `m_OnBuild` (`:854`) — those two are the manager's only `ScriptInvoker`s (`:109-110`), and there is no `m_OnFOB*` invoker anywhere.
- The only signals a successful deploy produces are a **broadcast** `SendTextNotification("DeployedFOB", -1, …)` (`RegisterFOB`, `:1120`) and the generic owner-scoped progress events on the acting player's `OVT_ContainerTransferComponent` (`OVT_BaseServerProgressComponent.c:50, 154`), whose args carry **no playerId** — the player is implied by which controller owns the component.
- Per-player signals exist only on **rejection** (`:494`, `:507`, `:523`).

**Per the requirements' own rule, this is recorded as a `tutorial-system` gap and not worked around here.** It is deliberately **not** filed as a blocking request, because the entry ships anyway: `build-first-structure` fires on the first *build*, which in practice happens at a base, FOB or camp, its body covers what a FOB is, and its "Learn more" opens the FOBs and Building page which documents deployment properly. If a future content pass wants a deploy-specific tip, the cheapest seam is an invoker beside `RegisterFOB` (`:1106-1120`) carrying the playerId that `OnFOBDeploymentComplete` already derives — a `tutorial-system` change of a few lines, and nothing this feature should attempt.

---

## 4. Implementation Phases

Every phase ends with `tools/compile-check.sh` exit 0 and `tools/run-tests.sh "{6A6E29FF47ECB840}"` (Fast) exit 0. Measured baselines on this tree, 2026-08-08 (the counts in `CLAUDE.md` are stale): **Fast 47**, **All 77–78** — the 78th is an untracked `Campaign/OVT_TEST_Campaign_TutorialSpawnTrigger.c` from a concurrent session, not a regression. **This plan adds 0 test cases**; the existing guards already cover it (optional Phase 2b is the single exception, and it is droppable).

### Phase 1 — Early-game batch: six entries
*Agent: **`help-docs-sync`**. Standard effort.* · *Estimate: 1 session*

One reviewable run producing entries 1–6 of §3.1. Read §3.7 first.

| # | Task |
|---|---|
| 1.1 | **Fact-check pass, before any writing.** For each of the six topics, read the shipped field-manual strings (`#OVT-FieldManual_{YourHome,Shops,GunDealers,MapAndTravel,WantedSystem,Skills}_Text*` in `Language/localization_Overthrow.st`) **and** the source files in §3.2's right-hand column. Produce a one-line-per-sentence evidence note (`file:line` or the manual string id plus its own cited source) that will go into the `Comment` fields. A sentence with no evidence is cut, not softened. Traps that apply to this batch: the integer-division price step, the unreachable wanted levels 1 and 5, gun-dealer ammunition, the generic gun-dealer map icon, and **exactly three skills — `Trade`, `Stealth`, `Diplomacy`** (`Configs/Player/overthrowSkills.conf:4, 58, 103`) |
| 1.2 | **Adopt `economy-first-buy`.** In `Configs/Tutorials/proofFirstBuy.conf`: change `m_sFieldManualTitleKey` from `#OVT-FieldManual_MainMenu_Title` to `#OVT-FieldManual_Shops_Title`. **Do not touch `m_sId`, the file name, the file's resource GUID or the prefab element.** Rewrite `#OVT-Tutorial_EconomyFirstBuy_Body` as the real money-and-shops entry (stock is finite and replenishes; prices differ by town — see the §3.7 integer-division trap; money is a balance on the player record). Keep or refine `_Title`; extend the `Comment` with the new evidence and **keep the existing anti-regression notes** about theft and deposits |
| 1.3 | Author five new entry configs + `.meta` files (`homeFirstOpen`, `shopsFirstGunDealer`, `mapFirstOpen`, `wantedFirstLevel`, `skillsFirstOpen`), copying `proofFirstBuy.conf` as the template. **Every member written out explicitly**, including ones equal to their attribute default — these files are templates as much as data (`tutorial-system/context.md` gotcha 32). GUIDs from §3.6 |
| 1.4 | Add **10 string items** (5 titles, 5 bodies) to `Language/localization_Overthrow.st` **only**, inserted alongside the existing `OVT-Tutorial_*` block (currently ~`:10083-10168`) so it stays contiguous. Each item: fresh `{6B3C0000000001xx}` GUID, `Target_en_us`, and a `Comment` that (a) says where the string appears and how long it may be, (b) states the tone rule, and (c) **records the Rule 0 evidence from 1.1**. Titles: 2–3 words. Bodies: **two short sentences**, because a NONMODAL body wraps inside a 460 px HUD panel |
| 1.5 | Append five elements to `OVT_TutorialManagerComponent.m_aEntries` on `Prefabs/GameMode/OVT_OverthrowGameMode.et` (after the two existing rows at `:213-218`), each `OVT_TutorialEntryConfig "{elemGuid}" : "{confGuid}Configs/Tutorials/<file>.conf" { }` |
| 1.6 | Hygiene: `grep -oE '\{[0-9A-F]{16}\}' <new confs> Prefabs/GameMode/OVT_OverthrowGameMode.et \| sort \| uniq -d` prints nothing; `git diff --stat Language/` shows **only** `localization_Overthrow.st`; no em-dash anywhere in authored content; balanced braces in every new `.conf` |
| 1.7 | `tools/compile-check.sh` (expect 0) and `tools/run-tests.sh "{6A6E29FF47ECB840}"` (expect 0, 47). Confirm the Init case's log line now reads **7 structurally valid entries** |
| 1.8 | Report the **complete list of new/changed string ids** for the user's Workbench export, and note that none of the six tips will render its text until that export happens |

**Acceptance:**
- Six entries exist (one adopted, five new), each with exactly one trigger from §3.1, one page and a link key from the frozen table.
- Every body sentence has recorded evidence in its `Comment`; no sentence names a mechanic that §3.7 lists as a trap.
- No imperative verb addressed to the player, no "now that you have…", no reference to another tip or to an order of play.
- `compile-check` 0; Fast tier 0 at 47; Init log reports 7 entries; `git diff --stat Language/` clean of `.lang.conf`.
- Export list handed to the user.

### Phase 2 — Mid-game batch: four entries
*Agent: **`help-docs-sync`**. Standard effort.* · *Estimate: 1 session*

Entries 7–10 of §3.1. Same shape as Phase 1; the fact-check corpus is `#OVT-FieldManual_{Recruits,Camps,FOBs,BaseCapture}_Text*` plus the source files in §3.2 rows 9–12.

| # | Task |
|---|---|
| 2.1 | Fact-check pass for the four topics, exactly as 1.1. Note the specific traps from §3.7: the camp/base exclusion that is **not enforced**; building costs **money**, not supplies; capture is **not** a flag or planting mechanic; and **building happens at a held base, a FOB *or* a camp** — do not write "at a FOB" alone (`#OVT-FieldManual_FOBs_Text`) |
| 2.2 | Author four configs + `.meta` (`recruitsFirstRecruit`, `placeFirstPlaceable`, `buildFirstStructure`, `basesFirstCapture`). GUIDs from §3.6 |
| 2.3 | Add **8 string items** to the `.st` master, same rules as 1.4 |
| 2.4 | Append four elements to `m_aEntries` on the game-mode prefab |
| 2.5 | `basesFirstCapture` specifically: the body must read correctly for **either direction** of a control change and for a player who was merely nearby, because `BASE_CONTROL_CHANGE` is a proximity fan-out with no acting player (D7). No "you captured…" |
| 2.6 | Hygiene + `compile-check` + Fast tier, as 1.6/1.7. Init log should now read **11 structurally valid entries** |
| 2.7 | `buildFirstStructure` specifically: it fires on the first **build**, not on a FOB deploy, because no deploy event exists (§3.8). The body may explain what a FOB is; it must not read as though the player just deployed one |
| 2.8 | Record the §3.8 FOB-deploy gap in this feature's `context.md` as a **non-blocking** note for `tutorial-system`, with the `RegisterFOB` seam named. Do not file it as a blocker and do not implement it |
| 2.9 | Report the new string ids for the export list |

**Acceptance:**
- Ten entries total, ten distinct ids, ten link keys all present in the frozen table, no duplicate GUID anywhere.
- `basesFirstCapture` contains no sentence that assumes the reader acted; `buildFirstStructure` none that assumes a FOB was just deployed.
- Same tone, evidence, compile and test criteria as Phase 1; Init log reports 11 entries.

### Phase 2b — OPTIONAL: pin the enum-name filter contract
*Agent: **`component-developer`** (standard). Droppable — see below.* · *Estimate: 30 minutes*

The one filter in the whole set whose value is produced by an engine call rather than written in a config is `SHOP_GUNDEALER`: the manager builds it with `SCR_Enum.GetEnumName(OVT_ShopType, shop.m_ShopType)` (`OVT_TutorialManagerComponent.c:255`), which is `typename.EnumToString` (`ArmaReforger/scripts/Game/Global/SCR_Enum.c:88-91`). If that string is not literally `SHOP_GUNDEALER`, the gun-dealer tip **never fires and nothing reports it**.

| # | Task |
|---|---|
| 2b.1 | Add one branch to the existing `OVT_TEST_Init_Tutorial_ManagerResolvesAndLoadsEntries`: for every authored trigger with `m_eEvent == PLAYER_TRANSACTION` and a non-empty `m_sFilter`, fail unless the filter equals `SCR_Enum.GetEnumName(OVT_ShopType, v)` for some `OVT_ShopType` value. Message must say the tip will silently never fire |
| 2b.2 | **Prove it red once** (e.g. `SHOP_GUNDEALER` → `SHOP_GUNDEALERS`) and record the exact failure text and date in `context.md`'s proven-red table. No `maxAttempts` |

**Drop this phase if** the play-test in §7 is scheduled promptly — step P3 observes the gun-dealer tip firing, which is the same evidence. Take it if the guard is wanted permanently, or if play-testing is deferred. It is the only phase in this plan that touches a `.c` file, and it touches a test file only.

### Phase 3 — Coverage mapping, minimal wiki sync, verification
*Agent: **`help-docs-sync`**. Standard effort.* · *Estimate: half a session*

| # | Task |
|---|---|
| 3.1 | Write the **as-built** starter-job coverage mapping (§3.5, corrected against what actually shipped) into `docs/features/new-player-experience/tutorial-content/context.md`, and append a short pointer section to `docs/features/new-player-experience/starter-jobs-retirement/requirements.md` naming the five jobs, the covering entry ids, and the two residual gaps (marker-based discovery; recruit availability) |
| 3.2 | **Wiki, minimal.** For each fact the Phase 1/2 fact-check proved a wiki page states wrongly, fix that sentence on that page and nothing else. Then add **one or two sentences** about the tip system where the wiki describes onboarding (primary candidate: `getting-started`; `home` only if it has an onboarding section). **Budget: at most 2 pages updated, 0 created** |
| 3.3 | **Wiki, do-not-touch list:** the `**Tutorial Jobs**` paragraph under `### 1. Jobs System` and item 6 under `## Systems Worth Knowing About` on `getting-started` belong to `starter-jobs-retirement` (`field-manual` D12, handoff recorded in that feature's requirements). Do not remove, reword or contradict them. Do not re-audit any page for staleness — `field-manual` swept them all on 2026-08-08 |
| 3.4 | Final verification: `tools/compile-check.sh` (0); Fast `{6A6E29FF47ECB840}` (0); All `{6A6E2A002F53A581}` (0); `git diff --stat Language/` shows only the `.st`; repo-wide duplicate-GUID grep over the new `6B3C…` allocations; and a grep proving all ten `m_sFieldManualTitleKey` values appear verbatim in the frozen table |
| 3.5 | Produce the **play-test checklist** of §7 as a concrete, copy-pasteable list in `tasks.md` under "Needs Human Verification", including the seen-store reset procedure and the export prerequisite. List every new string id one final time |

**Acceptance:**
- Coverage mapping recorded in this feature's `context.md` **and** pointed at from `starter-jobs-retirement/requirements.md`.
- Wiki edits enumerated with page ids and a one-line reason each; no page created; the do-not-touch list intact.
- All four commands green; the exported string-id list handed over; the play-test checklist written down rather than described.

---

## 5. Key Technical Decisions

**D1 — Ten entries, and no more.** The "popup per minute" rule is the hardest constraint in the requirements, so every entry has to justify a slot. Ten is what the requirements name (six early + four mid), and each one is the *only* tip for its system: money and shops share one, placing and camps share one, FOBs and building share one, the map and fast travel share one, and the wanted system's disguise material folds into the wanted tip rather than becoming an eleventh. Realistic exposure for a new player is **two to four popups in the first thirty minutes** (a menu open, a purchase, the map, maybe a wanted level), each shown once per machine for ever. Rejected: a separate shops-vs-money split (two popups from one purchase), a dedicated disguise tip (no trigger exists for going undercover), a fast-travel tip (no invoker; the map tip covers it), and any late-game entry (out of scope by requirement).

**D2 — All ten are NONMODAL at priority 0.** The framework contract reserves MODAL for multi-page sequences and priority for the first-spawn welcome (`m_iPriority 100`). A modal popup steals input mid-game; ordinary content must not. Consequence, accepted and documented in §3.4: "Learn more" is reached through the HUD prompt → main menu → Tips → modal, not from the HUD tip directly. That is the R3 fallback the framework already shipped, not something this feature can change.

**D3 — Adopt `economy-first-buy`; do not mint a second money tip, and do not rename its file.** The id is immutable and already in players' seen stores; a new `economy-first-shop` id would re-show the same information to everyone who already dismissed it, and retiring the old one would waste the slot. Only the body text, title text and link are free to change, and all three are changed. The **file name stays `proofFirstBuy.conf`**: renaming it would mean editing the `.meta` `Name` path and the prefab reference for a cosmetic gain, and the id — not the filename — is what anything reads. Noted here so the next reader does not file it as sloppiness.

**D4 — The skills tip triggers on opening the character sheet, not on a skill purchase.** The requirement says "first XP gain or first level-up", and **neither has an invoker**. The nearest available event, `PLAYER_SKILL`, fires only when a point is *spent* (`OVT_SkillManagerComponent.c:121-128`) — which is after the player has already found the screen, read the labels and worked it out, making the tip redundant at the moment it arrives. `MENU_OPENED` with filter `OVT_CharacterSheetContext` fires the first time they *look* at XP, levels and skills, is per-player and client-local, and needs no framework change. `PLAYER_SKILL` as a second trigger would be dead weight: you cannot spend a point without opening the sheet. **This is a content-side trigger choice, not a framework gap** — nothing goes back to `tutorial-system`.

**D5 — Placing is triggered unfiltered, not on `Camp`.** The contract's own example id is `place-first-camp`, but filtering on `placeable.m_sName == "Camp"` would miss the player who puts up a Poster or an Ammobox first — and "placing equipment" is one of the five things `starter-jobs-retirement` needs covered. An unfiltered `PLAYER_PLACE` fires on the first placeable of any kind and the body covers the range from small items up to a camp, which is also how the manual page is written. One entry, one trigger, better coverage.

**D6 — The wanted tip takes no threshold.** `m_iMinValue 0` is the "no threshold" sentinel, and `PLAYER_WANTED` is escalation-only, so the entry fires on the player's **first escalation** whatever level it lands on. A threshold of 1 would look meaningful and mean nothing: escalation sites set 2, 3 or 4, and level 1 is only ever reached on the way *down* (`field-manual/context.md:170`).

**D7 — Base capture accepts the proximity heuristic, and the text is written for it.** The framework contract advises preferring per-player triggers, so this choice was checked rather than assumed. **There is no per-player event anywhere in the capture flow**, established 2026-08-08 by walking it end to end: the capture action carries no playerId over the wire (`OVT_PlayerCommsComponent.c:187-193`, the server re-derives the sender's position), the QRF's `m_OnFinished` fires with **no args** (`OVT_QRFControllerComponent.c:187`), and `m_OnBaseControlChanged` carries only the base controller (`OVT_OccupyingFactionManager.c:1375`). So the choice is the 300 m fan-out (`OVT_TutorialManagerComponent.c:34, 345-355`) or nothing.

It is defensible **here specifically** because the people within 300 m of a base that just changed hands are the people who just fought over it — unlike a town flip, which can happen from stability with nobody involved, which is why `TOWN_CONTROL_CHANGE` is not used at all. Three consequences are handled by the writing rather than the config: the body must not say "you captured" (a nearby squadmate gets the same tip), it must read correctly if the occupiers **re-take** a base (the event fires in both directions), and it must not describe capture as taking a flag or planting anything (§3.7 — control flips on the QRF point score and the flag follows).

Rejected alternatives: hanging the tip off `PLAYER_BUILD` at a captured base (teaches capture at the wrong moment and duplicates entry 9), and asking `tutorial-system` for a per-player capture event. The second is recorded rather than requested, because two cheap seams exist if the heuristic ever proves wrong — `RpcAsk_StartBaseCapture` (`OVT_PlayerCommsComponent.c:192`, where the acting player is already resolvable) for "a capture started", and the QRF participant loop (`OVT_QRFControllerComponent.c:132-147`, which already enumerates every player inside 220 m every 10 s to award XP) for "you are in a base battle". Neither is needed to ship this entry.

**D8 — Two authoring batches, not one and not ten.** Each batch is one `help-docs-sync` run of six and four entries with a fact-check gate at its front and a compile/test gate at its back. One run of ten would put twenty string items and ten configs into a single unreviewable diff; ten runs of one would repeat the same source reading ten times. Early and mid also split cleanly by which manual pages they compress and by which traps apply.

**D9 — No new automated cases (except the optional 2b).** The two Init guards already walk **every** entry: ids non-empty and unique, pages ≥ 1, triggers ≥ 1 (`OVT_TEST_InitSuite.c:1605-1633`), and every non-empty `m_sFieldManualTitleKey` resolving to a real manual page (`:2200-2212`). Nine additions are therefore guarded for free. Adding cases that re-assert the same properties would be ceremony. The one genuinely new assertable seam is the engine-produced `SHOP_GUNDEALER` filter string, which is why 2b exists and why it is optional rather than assumed.

**D10 — The wiki gets a scalpel, not a sweep.** `field-manual` audited every player-facing page on 2026-08-08 and left sixteen numbered open questions for a gameplay owner. Re-auditing would re-find them. This feature edits a wiki page only when its own fact-check proves a specific sentence wrong, plus one brief mention of the tip system where onboarding is described — capped at two pages updated and none created, and explicitly forbidden from touching the starter-jobs paragraph that `field-manual` handed to feature #5.

**D11 — Content-only, and a trigger gap escalates rather than gets worked around.** No `.c`, no `.layout`, no keybinding, no schema field. If an entry cannot be expressed with the shipped thirteen events, the answer is a `tutorial-system` change request with the entry deferred, not a clever filter or a script. Two borderline cases were found while planning and both are resolved in this document rather than left for the implementer: skills (D4) resolved to an existing event, and FOB deployment (§3.8) has genuinely no event and is recorded as a non-blocking `tutorial-system` gap because the build-triggered entry covers the topic. Neither should be "fixed" later by adding an invoker nobody needs.

**D12 — Bodies compress already-verified prose.** Each tutorial body is a two-sentence reduction of a field-manual paragraph that was itself verified against source in `field-manual` Phases 2–5. The `Comment` cites **both** the manual string id and the underlying `file:line`, so the evidence chain survives a reword of either surface. This is the cheapest available defence against Rule 0 and it is why the fact-check task comes *before* the writing task in both phases, not after.

**D13 — All ten triggers are restore-safe, and that was verified rather than hoped.** A tip that fires while a saved campaign is being restored would hit a returning player with a burst of first-time popups for things they did months ago. Each of the seven invokers behind the ten entries was checked at its restore path: placing and building are unreachable from the serializers, recruit restore never invokes, base-control restore passes `suppressEvents = true`, and the client-local two (`MENU_OPENED`, `MAP_OPENED`) are driven by a human opening a screen. Citations in §3.3. No guard, filter or threshold was needed anywhere — which is the reason to record the check, because absent this note the next author may add one.

---

## 6. Definition of Done

An independent evaluator should be able to check every item below without having read the code.

### Functional

- [ ] **F1 — Ten entries live.** `OVT_TEST_Init_Tutorial_ManagerResolvesAndLoadsEntries` logs **11 structurally valid entries** (the ten of §3.1 plus `welcome-intro`) and passes.
- [ ] **F2 — Each entry fires from its own action, once.** For every row of the §7 table: perform the action on a fresh seen-store → the named popup appears exactly once, with title and body text (not raw `#OVT-` keys); repeat the action → nothing appears.
- [ ] **F3 — Each entry's "Learn more" lands on the right page.** Via the §3.4 route (HUD prompt → main menu → Tips → modal → Learn More), the Field Manual opens on the page named in §3.1 for that entry — not the front page and not a neighbour. Spot-checked on at least four entries covering four different sub-categories; the remaining six are covered by the automated link guard plus the frozen-table grep.
- [ ] **F4 — Nothing appears on top of a menu or the map.** Tips triggered by `MENU_OPENED`, `MAP_OPENED` or a shop purchase appear **after** the screen closes, within about a second.
- [ ] **F5 — Two tips from one action queue, they do not overlap.** On a fresh store, a first-ever purchase at a gun dealer shows `economy-first-buy` first and `shops-first-gun-dealer` only after it is dismissed (equal priority, arrival order: `m_OnPlayerBuy` at `OVT_PlayerCommsComponent.c:688` precedes `m_OnPlayerTransaction` at `:691`).
- [ ] **F6 — Nothing re-shows after a restart or a new campaign.** Seen state is per machine; a dismissed tip stays dismissed on a new campaign and after an application restart.
- [ ] **F7 — Nothing in this feature fires for a player who did not act.** In a two-client session, a tip caused by client A appears on A only (except `bases-first-capture`, which is a documented 300 m fan-out and may legitimately appear for both).
- [ ] **F8 — `first-spawn`'s territory is untouched.** `Configs/Tutorials/proofWelcome.conf`, `welcome-intro`, `#OVT-IntroHint` and `m_aHintedPlayers` are byte-identical to before this feature.

### Quality

- [ ] **Q1 — Rule 0 evidence recorded, per sentence.** Every new or rewritten string item's `Comment` names the `file:line` (or the field-manual string id, itself sourced) that makes each claim true. An evaluator can pick any sentence in any tip and find its evidence without asking the author.
- [ ] **Q2 — No trap re-introduced.** No tip claims money can be stolen or deposited, that the Overthrow menu holds a money screen or the Field Manual, that prices scale smoothly with town stock, that camps cannot be placed near bases, that wanted levels run 1–5, or that gun dealers stock occupying-faction ammunition (§3.7). ~~gun dealers have their own map icon~~ was **removed from this list on 2026-08-09** — that claim is true, and the §3.7 row asserting otherwise is struck.
- [ ] **Q3 — Tone.** No imperative addressed to the player, no goal, no objective, no "next", no reference to another tip, no assumption about what the player did before. Each body reads correctly in isolation and in any order.
- [ ] **Q4 — Length and markup.** Titles 2–3 words; bodies two short sentences that fit the 460 px HUD panel without clipping. `<br/>` used only if genuinely needed. **No em-dashes anywhere.**
- [ ] **Q5 — Localization hygiene.** `git diff --stat Language/` lists `localization_Overthrow.st` and nothing else. No `.lang.conf` export was edited by any agent. Every new item has a fresh unique `{6B3C…}` GUID and a translator-useful `Comment`.
- [ ] **Q6 — Config hygiene.** Every new `.conf` writes out **every** member explicitly (template rule); every `.meta` has a fresh GUID matching §3.6; a repo-wide duplicate-GUID grep over the new allocations prints nothing; braces balanced.
- [ ] **Q7 — Ids are right first time.** Ten ids, lowercase-kebab, `<system>-<event>`, ≤ 48 chars, unique, none reused from a retired entry, and `economy-first-buy` unchanged.
- [ ] **Q8 — Green.** `tools/compile-check.sh` exit 0; Fast `{6A6E29FF47ECB840}` exit 0; All `{6A6E2A002F53A581}` exit 0. Case count unchanged (Fast 47) unless optional Phase 2b was taken (+0 cases, +1 branch) — in which case its proven-red record exists.

### Integration

- [ ] **I1 — The existing guards did the work.** No new test case duplicates the id/page/trigger or link-resolution assertions; both existing Init cases pass with eleven entries.
- [ ] **I2 — Every link key is in the frozen table.** A grep of the ten `m_sFieldManualTitleKey` values against §3.2 matches ten for ten, exactly and case-sensitively.
- [ ] **I3 — Starter-job coverage recorded.** The as-built mapping is in this feature's `context.md` and pointed at from `starter-jobs-retirement/requirements.md`, including the two residual gaps.
- [ ] **I4 — Wiki edits enumerated.** Every page updated is listed with its page id and a one-line reason; nothing created; the `**Tutorial Jobs**` paragraph and item 6 on `getting-started` are unchanged.
- [ ] **I5 — Export handed over.** The complete list of new/changed string ids is reported for the user's Workbench re-export, with the note that tip text does not render until that happens.
- [ ] **I6 — No framework drift.** `git diff --stat` shows zero changes under `Scripts/` (except a test file if 2b was taken), `UI/Layouts/`, `Configs/FieldManual/` and `Configs/System/`.
- [ ] **I7 — The one real trigger gap is recorded, not worked around.** §3.8's FOB-deploy gap is written into this feature's `context.md` as a non-blocking `tutorial-system` note naming the `RegisterFOB` seam, and no attempt was made to synthesise a deploy trigger from the transfer-progress events or from a broadcast notification.

---

## 7. Testing Strategy

### Automated — nothing new, and that is the point

| Gate | What it proves for this feature | Where |
|---|---|---|
| `OVT_TEST_Init_Tutorial_ManagerResolvesAndLoadsEntries` | All eleven entries load; no empty or duplicate id; every entry has ≥ 1 page and ≥ 1 trigger. Fails **by name** on the offender | `OVT_TEST_InitSuite.c:1562-1633` |
| `OVT_TEST_Init_FieldManual_DeltaMergesAndLinksResolve` | Every one of the ten `m_sFieldManualTitleKey` values resolves to a real page in the merged manual — the exact rot a one-letter typo causes | `OVT_TEST_InitSuite.c:2200-2212` |
| `OVT_TEST_Init_Tutorial_InvokerSeamsExist` | Every invoker the chosen triggers depend on still exists and is allocated | `OVT_TEST_InitSuite.c:1656+` |
| `tools/compile-check.sh` | The `.conf` files parse and the prefab still loads | — |

```bash
tools/compile-check.sh                                            # expect 0
tools/run-tests.sh "{6A6E29FF47ECB840}"                           # Fast, expect 0 / 47
tools/run-tests.sh "{6A6E2A002F53A581}"                           # All,  expect 0 / 77-78
tools/run-tests.sh OVT_TEST_Init_Tutorial_ManagerResolvesAndLoadsEntries   # single case
git diff --stat Language/                                         # expect ONLY localization_Overthrow.st
grep -c "m_sFieldManualTitleKey" Configs/Tutorials/*.conf         # expect one per entry, ten total
```

**Structurally uncoverable here:** whether a sentence is *true* (Rule 0, human), whether a popup *renders* (UI is play-test-only by project rule), whether a trigger *fires* (needs the world plus a player action), and whether the text reads well.

### Prerequisite: the user's Workbench export

New `.st` items are invisible in-game until the user re-exports the string table in Workbench. **Until then every new tip draws its raw `#OVT-` key**, which looks exactly like a bug. The play-test cannot start before that step, and the phase reports must list the ids to export. (`field-manual` hit this and resolved it the same way — see its `context.md` session log for 2026-08-08.)

### Play-test — the seen-store reset

Seen state lives in `$profile:.save/settings/ReforgerGameSettings.conf` under an `OVT_TutorialSettings` block. To re-test an entry, clear that block (Workbench → **User Settings → Edit Game Settings**, or delete the block) and confirm `m_bTipsDisabled 0`. An absent `m_aSeen` is the empty state, not corruption (`tutorial-system/context.md` gotcha 17).

### Play-test — per-entry trigger checklist

Each row: perform the action on a cleared seen store, then perform it a second time.

| # | Entry | Action to perform | Popup must appear | And must not |
|---|---|---|---|---|
| P1 | `home-first-open` | Overthrow menu → **Real Estate** → close it | after the screen closes | reappear on a second open |
| P2 | `economy-first-buy` | buy anything at any shop | after the shop menu closes | reappear on the next purchase |
| P3 | `shops-first-gun-dealer` | buy anything at a **gun dealer** | after the shop menu closes | fire at a general/clothes/food shop |
| P4 | `map-first-open` | open the map (`M`), close it | after the map closes | reappear on a second map open |
| P5 | `wanted-first-level` | be seen by occupying troops while openly armed, or wound anybody | on the first escalation | fire again as the level decays and rises |
| P6 | `skills-first-open` | Overthrow menu → **Character Sheet** → close it | after the screen closes | reappear on a second open |
| P7 | `recruits-first-recruit` | recruit a civilian | on the recruit joining | fire for a second recruit |
| P8 | `place-first-placeable` | Place menu → place a **Poster** (cheapest) | on placement completing | fire on the next placeable, including a Camp |
| P9 | `build-first-structure` | at a camp, FOB or held base, build any structure | on the build completing | fire on the next build |
| P10 | `bases-first-capture` | be within 300 m when a base changes hands | on the control change | fire on a **town** flip |

**P10 is the expensive one.** It needs a base to actually change hands, and there is no shortcut in the mechanics: control flips on the QRF point score, so the procedure is *use the capture action on the base flag while no QRF is active, clear the occupying AI inside 220 m, and stay inside it* until the threshold is crossed (`OVT_QRFControllerComponent.c:110-188`). Nothing is placed, planted or captured in the flag sense. If it cannot be reached in the session, record it as deferred **with the reason** rather than ticking it — and note that the base-capture entry is the only one whose delivery path is the proximity fan-out rather than a per-player RPC, so it is the one least covered by the other nine passing.

### Play-test — link and presentation spot-checks

Run on at least four entries from different manual sub-categories (suggested: P2 → Money and Trade, P1 → Getting Started, P5 → Staying Hidden, P8 → The Resistance):

1. While the HUD tip is up, press the **Overthrow Menu** prompt on it (`KC_U` / gamepad `pad_down`) within the 20 s window. The main menu opens.
2. Choose **Tips**. The modal shows **the same tip**, full text, with **Learn More** visible.
3. **Learn More** opens the Field Manual **on that entry's page** per §3.1 — not the front page.
4. Back out, dismiss the tip. Repeat the trigger: nothing appears.
5. Confirm the HUD tip never captured movement, aim or fire while it was up.
6. Confirm the text fits the panel at 1080p with no clipping and no overlap with the wanted/economy/progress HUD elements.

### Play-test — multiplayer sanity (one pass, not per entry)

```bash
tools/launch-server.sh --scenario eden
tools/launch-game.sh --timeout 3600 --profile OverthrowClient1 --allow-concurrent -- -client 127.0.0.1:2001
tools/launch-game.sh --timeout 3600 --profile OverthrowClient2 --allow-concurrent -- -client 127.0.0.1:2001
```

Client A buys at a shop: only A sees `economy-first-buy`, and B's log does not mention the id. Then B buys: B gets its own. This re-uses `tutorial-system`'s I3/I4 protocol and only needs one entry to demonstrate it — the delivery path is shared by all nine per-player entries.

⚠️ **`tutorial-system`'s own play-test is still owed.** If it has not been run when this feature is verified, treat the first content play-test as covering both, and report framework defects as `tutorial-system` bugs rather than fixing them here.

---

## 8. Dependencies

- **`tutorial-system` (#1, built 2026-08-07, play-test owed).** Provides the schema, the manager, the delivery RPC, both popup surfaces, the seen store and the thirteen trigger events. The contract consumed is its §5, corrected against shipped code. **Nothing in this plan requires a framework change.**
- **`field-manual` (#2, complete and play-tested 2026-08-08).** Provides all ten link targets, frozen and exported, plus the verified prose each tip compresses (D12) and the sixteen open questions that became §3.7's trap list.
- **The user's Workbench string-table export.** A hard, external, human step between authoring and play-testing (see §7). Every authoring phase's report must end with the id list.
- **`first-spawn` (#4, planned, parallel).** Owns `welcome-intro`, `#OVT-IntroHint` and the welcome's home/car/cash material. Coordination: this feature's `home-first-open` goes *deeper* than the welcome (what ownership means mechanically) and must not restate it; `first-spawn` must not add a second home entry. Both may link `#OVT-FieldManual_YourHome_Title` — link keys are not exclusive.
- **`starter-jobs-retirement` (#5, planned, after).** Blocked on this feature's coverage mapping (§3.5, recorded in Phase 3.1). It also owns the wiki's starter-jobs paragraph, which Phase 3.3 forbids touching.
- **Base-game systems:** the stringtable pipeline, `ModuleGameSettings` (seen store), `SCR_FieldManualUI` (link target). None are modified.

---

## 9. Risks & Mitigation

**R1 — A tip states a mechanic that does not exist.**
*Likelihood: medium — it has already happened twice on two entries. Impact: high; a false tip is worse than no tip, because the player spends real time looking for the thing.*
No gate can catch it: compile-check, all 77 assertions, the id and link guards and the localization checks all pass happily on a well-formed lie. Mitigated by making Rule 0 a **phase-front task** (1.1, 2.1) rather than a review step; by §3.7 pre-loading the five traps `field-manual` already found plus the two that shipped; by D12's rule that a body may only compress prose that was already verified; and by requiring the evidence in the `Comment` field so the next author inherits it. **Acceptance criterion Q1 is the gate, and it is human.**

**R2 — The gun-dealer filter string is wrong and the tip silently never fires.**
*Likelihood: low. Impact: medium; one entry dead with no error anywhere.*
The filter is the only value in the set produced by an engine call (`typename.EnumToString`) rather than authored. Mitigated by play-test step P3, which fails visibly; by optional Phase 2b, which turns it into a build failure; and by the fallback being trivial (correct the string in one `.conf`).

**R3 — Two tips from one action, or a burst in the first two minutes.**
*Likelihood: medium. Impact: medium; annoyance is the exact thing this epic exists to reduce.*
Three overlaps exist by construction: a first-ever purchase at a gun dealer fires two (F5 — serialized, money first by arrival order), and `first-spawn`'s welcome plus an early menu open could land close together. Mitigated by D1's one-tip-per-system rule; by every entry sitting at priority 0 so the framework's queue serializes them one at a time; by deliberately **not** hanging the home tip on `MENU_OPENED / OVT_MainMenuContext`, which would fire seconds after the welcome; and by the accepted cost that the home tip fires later, or not at all, for a player who never opens Real Estate. Fallback if play-testing shows the home tip effectively never fires: add a second `MENU_OPENED / OVT_MainMenuContext` trigger — a two-line `.conf` change, no framework work.

**R4 — The base-capture tip fires for the wrong person or in the wrong direction.**
*Likelihood: medium. Impact: low-to-medium; a confusing tip rather than a broken one.*
`BASE_CONTROL_CHANGE` is a 300 m fan-out with no acting player and fires on control changes in **both** directions. Mitigated entirely by the writing (task 2.5: no "you captured", correct for either direction, correct for a bystander) rather than by config, because the config cannot distinguish these. **The save-load case is already closed**: the restore path passes `suppressEvents = true` (`OVT_OccupyingFactionManager.c:698`), so a restored campaign cannot fire it. If play-testing still shows the heuristic landing on the wrong people, the entry moves to `m_bEnabled 0` pending one of the two per-player seams named in D7 — a `tutorial-system` change, not a change here.

**R5 — Concurrent sessions touch the same three files.**
*Likelihood: medium — this tree has hosted parallel sessions all week, and `field-manual` measured a test count change from one. Impact: medium; a merge conflict or a duplicate GUID.*
Both authoring phases append to `Language/localization_Overthrow.st` and to one array on `Prefabs/GameMode/OVT_OverthrowGameMode.et` — the same two files `first-spawn` will touch, and the prefab is also the jobs array's home. Mitigated by the reserved `6B3C…` GUID block (0 current matches, so no collision is possible even under a concurrent allocation); by appending rather than reordering; by task 1.6/2.6's duplicate-GUID grep; and by re-checking `git status` and the highest allocated GUID at the start of each phase rather than trusting this document's snapshot.

**R6 — The string table is not exported and the play-test reports "raw keys everywhere".**
*Likelihood: medium; it is an external human step. Impact: low, but it wastes a play-test session.*
Mitigated by making the export list the last task of every authoring phase, by naming it as a prerequisite at the top of the play-test checklist, and by the checklist's first instruction being to confirm one known tip renders its text before working through the ten rows.

**R7 — `tutorial-system`'s owed play-test surfaces a framework defect mid-verification.**
*Likelihood: medium. Impact: medium; content verification blocks on a fix that is not this feature's to make.*
Mitigated by the §7 note: the first content play-test doubles as the framework's, defects are filed against `tutorial-system` and not patched here, and the content itself (ids, links, text, evidence) is verifiable by the automated gates and by reading, independently of whether a popup renders.

**R8 — Scope creep into an eleventh entry.**
*Likelihood: medium; every play-test produces "there should be a tip for…". Impact: medium; volume is the design constraint.*
Mitigated by D1 naming the exact ten and the rejected candidates with reasons, by the requirements' explicit out-of-scope list (all late-game topics), and by the rule that a new entry is a new content pass with its own justification against the popup-per-minute rule — not an addendum to this one.

---

## 10. Quality Bar

This is a **content** feature. Nothing here is polished by refactoring; it is made good by being true, being quiet and being findable.

### 1. Factual accuracy — every claim traceable to a source

- **A sentence with no `file:line` does not ship.** Not softened, not hedged, not moved to the manual: cut. The narrow claim that was checked beats the richer one that was assumed, every time.
- **Evidence lives in the artefact**, not in a session log: the string item's `Comment` field, where the next author and the next translator both find it. The two corrected proof entries are the shape to copy.
- **Known-false is written down.** §3.7 exists so the next content pass does not re-derive five traps that cost a source audit to find, and so a reviewer can check a tip against a list instead of against their memory.
- **Compression is not invention.** A tip may say less than the manual page it links to. It may never say more.

### 2. Tone compliance — the sandbox is not negotiable

- **Inform, never instruct.** No imperative addressed to the player, no goal, no objective, no completion, no "next".
- **Order-independent.** Every body reads correctly as the player's first tip or their tenth, and assumes nothing about what they did before.
- **No second-person accusations of action the player may not have taken** — the base-capture entry is the test case, because a bystander 300 m away gets the same text.
- **Quiet by default.** Ten tips across a campaign, one per system, one showing at a time, each once per machine for ever. If a play-tester describes the tips as frequent, the entry set is wrong, not the framework.

### 3. Localization hygiene

- **`localization_Overthrow.st` is the only file touched under `Language/`.** The generated per-language exports are the user's to regenerate; hand-editing has silently corrupted six of them before.
- **Every item gets a real `Comment`** — where the string appears, how much room it has, what markup must survive translation, and its evidence.
- **Every item gets a fresh GUID from the reserved block**, and no id is ever reused or renamed once shipped.
- **No em-dashes**, no `&`, and no hardcoded English anywhere outside the `.st`.

### 4. Link integrity

- **Ten links, ten exact matches** against the frozen table — case-sensitive, verified by grep and by the automated guard, not by eye.
- **A link points at the page that answers the tip's question**, not merely at a page that resolves. The guard proves the second; a human proves the first, on at least four of the ten.
- **The route to a link is documented, not assumed.** §3.4 exists because "Learn more" is not on the HUD popup, and a play-test that does not know that will report a missing button.

### 5. Zero framework drift

- **Not one line of gameplay script, layout, keybinding or schema.** The diff outside `Configs/Tutorials/`, `Language/localization_Overthrow.st`, the game-mode prefab's entry array and this feature's docs should be empty.
- **A missing capability is a report, not a workaround.** Nine entries proved the shipped trigger catalog sufficient; the tenth borderline case (skills) resolved to an existing event and the reasoning is recorded so nobody adds an invoker later believing it was needed.

---

*Plan authored 2026-08-08. Content phases route to `help-docs-sync`; `/proceed` must respect that routing, and no phase in this plan needs an advanced agent.*
