# Tutorial System - Context & Decisions

**Last Updated:** 2026-08-11 (post-close change set: show-over-UI tips, HUD field-manual link, PLAYER_ENTER_BASE)
**Current Phase:** — (all 9 phases complete)
**Status:** 🟢 Build complete · ⏳ awaiting the string-table export and the play-test gates

**Epic:** `new-player-experience` (feature #1 of 5 — the framework everything else in the epic runs on)

---

## Quick Status

**What's Done:**
- ✅ Plan written (`implementation.md`, 9 phases) and reviewed; scaffolded to **63 tasks** (4 added mid-build: 3.0, 4.6, 6.7, 6.8)
- ✅ Dev docs scaffolded
- ✅ **Phase 0** — both economy doc comments corrected; `m_OnPlayerSkill` now carries `playerId`; `OVT_PlayerWantedComponent.GetOnWantedLevelChanged()` added and fired
- ✅ **Phase 1** — config schema + the four pure decision classes + five Logic cases (Fast 43, All 71)
- ✅ **Phase 2** — `OVT_TutorialManagerComponent`, TEN invoker subscriptions, game-mode wiring, placeholder proof entry, two Init cases (Fast 45, All 73)
- ✅ **Phase 3** — `OVT_TutorialComponent` Owner RPC, the whole client pipeline, three client-local hooks, `m_OnPlayerSkill` widened to carry the skill key (Fast 45, All 73)
- ✅ **Phase 4** — `OVT_TutorialSettings` + `OVT_TutorialSettingsAccessor`, all four `// PHASE 4:` seams wired, the bounded `PLAYER_SPAWNED` retry, and **R1 discharged by the new serialization-gate case** (Fast 46, All 74)
- ✅ **Phase 5** — `UI/Layouts/HUD/TutorialPopup.layout` + `OVT_TutorialInfo : SCR_InfoDisplay`, registered on the player prefab. **R3 FIRED and its fallback was taken: no new keybinding, `chimeraInputCommon.conf` unmodified** (Fast 46, All 74 — this phase adds no test cases; UI is outside the test spine)

- ✅ **Phase 6** — `UI/Layouts/Menu/TutorialPopup.layout` + `OVT_TutorialContext`, the modal action context, the place/build gate fix (6.7), and the `ShowLayout()` `Print` strip (6.6). **R3 re-surveyed inline-aware and CONFIRMED: F5 stays deferred**, the main-menu → Tips route replaces it (Fast 46, All 74)
- ✅ **Phase 7** — **R2 DISCHARGED: the same-GUID field-manual delta merges, measured, not assumed** (6 categories, 8 tile backgrounds, 141 entries). Root reduced to one inheriting element; content moved to `Configs/FieldManual/Categories/FM_Overthrow.conf`; the second "Introduction" retitled; `modded class SCR_FieldManualUI` + `OVT_FieldManualHelper` + the Learn-more wiring; one new Init case, three of its branches proven red (Fast 47, All 75)

- ✅ **Phase 8** — both proof entries shipped (`economy-first-buy` NONMODAL/1-page/linked, `welcome-intro` MODAL/2-page/unlinked), five string items, the §5 contract corrected against shipped code (I6), and the whole Definition of Done walked into a verdict table in `tasks.md` (Fast 47, All 75 — unchanged; Phase 8 adds data, not cases)

**What's Next:**
- 👤 **The user.** Export the string table (17 ids), then work the **Needs Human Verification** checklists in `tasks.md`, starting with the Phase 8 block.

**Blockers:**
- None for the build. The play-test gates are blocked on the Workbench string-table export — without it every popup renders raw `#OVT-` keys.

**Definition of Done: 8 ✅ MET · 20 👤 HUMAN · 1 ⚠️ DEFERRED (F5, retired by R3).** Full table in `tasks.md` → *Definition of Done — Verdict*.

**Owed to a human, in order:**
1. **Workbench string-table export — 17 new `#OVT-` ids.** Everything visual is blocked on this; until it runs, every popup and button renders a raw key.
2. The four **Needs Human Verification** checklists in `tasks.md` (Phases 0, 4, 5, 6, 7) — rendering, gamepad navigation, the settings store's cross-restart half, and the field manual's *drawing* (its config object is already machine-asserted; do not re-count categories by eye).
3. The **R7 sweep is mandatory**, not optional: task 6.6 changed the shared `OVT_UIContext.ShowLayout()` for all 17 contexts.
4. The **two-client MP protocol** (§8.4, steps 16–21) — `tools/launch-server.sh` plus two `--profile` clients. This is the regression class that broke the starter jobs (BUG-037) and the harness structurally cannot reach it.

---

## Key Files

### To be created (core)
- `Scripts/Game/GameMode/Managers/OVT_TutorialManagerComponent.c` — server: registry, invoker subscriptions, dispatch
- `Scripts/Game/Components/Controller/OVT_TutorialComponent.c` — Owner RPC + the whole client pipeline
- `Scripts/Game/Configuration/OVT_TutorialEntryConfig.c` / `OVT_TutorialTrigger.c` — the data schema
- `Scripts/Game/Data/OVT_Tutorial{Matcher,Queue,Gate,SeenStore}.c` — pure, Logic-tier testable
- `Scripts/Game/Global/OVT_TutorialSettings.c` / `OVT_TutorialSettingsAccessor.c` — per-machine seen store
- `Scripts/Game/UI/HUD/OVT_TutorialInfo.c` + `UI/Layouts/HUD/TutorialPopup.layout` (+ `.meta`) — non-modal overlay
- `Scripts/Game/UI/Context/OVT_TutorialContext.c` — modal + sequence
- `Scripts/Game/UI/Modded/SCR_FieldManualUI.c` — open-by-title seam (`OVT_OpenEntryByTitle`, `OVT_OpenByTitle`)
- `Scripts/Game/Global/OVT_FieldManualHelper.c` — `Open(titleKey)`, the one call Overthrow code makes
- `Configs/FieldManual/Categories/FM_Overthrow.conf` (+ `.meta`, `{6B3A000000000090}`) — the Overthrow category's real home; `field-manual` edits THIS, never the root delta

### Edited
- `Scripts/Game/GameMode/Managers/OVT_EconomyManagerComponent.c` — 2 doc comments
- `Scripts/Game/GameMode/Managers/OVT_SkillManagerComponent.c` — `m_OnPlayerSkill` gains `playerId`
- `Scripts/Game/Components/Player/OVT_PlayerWantedComponent.c` — new static invoker
- `Scripts/Game/Components/Player/OVT_UIManagerComponent.c` — `IsAnyContextActive()`
- `Scripts/Game/UI/Context/OVT_CharacterSheetContext.c` — skill listener wrapper
- `Prefabs/GameMode/OVT_OverthrowGameMode.et`, `Prefabs/GameMode/OVT_OverthrowController.et`, `Prefabs/Characters/Factions/INDFOR/FIA/Character_Player.et`
- `Configs/FieldManual/*`, `Language/localization_Overthrow.st`
- `Configs/System/chimeraInputCommon.conf` — untouched by Phase 5 (R3 fallback). Phase 6.4 added FOUR menu actions (`OverthrowTutorialBack`/`Next`/`LearnMore`/`Disable`) and `ActionContext OverthrowTutorialMenuContext`. **Still no gameplay-context action** — R3 held on re-survey.
- `Scripts/Game/UI/OVT_UIContext.c` — new virtual `IsBlockingPopups()`; ten debug `Print()` calls removed from `ShowLayout()`
- `Scripts/Game/UI/Context/OVT_PlaceContext.c` / `OVT_BuildContext.c` — `IsBlockingPopups()` overrides
- `Scripts/Game/UI/Context/OVT_MainMenuContext.c` + `UI/Layouts/Menu/MainMenu.layout` — the Tips entry

### Docs
- `implementation.md` — the plan (source of truth; §5 is the contract siblings consume)
- `requirements.md` — scope truth
- `../epic-overview.md` — epic build order and cross-feature decisions

---

## Important Decisions

The plan's §7 (`Key Technical Decisions`, D1–D13) is the authoritative decision record — it is not duplicated here.
This section records decisions made **during** implementation that the plan did not already settle.

### GUID block
**Date:** 2026-08-07
**Decision:** `6B3A0000…` reserved for this feature. **Verified unused 2026-08-07** — `grep -rEoh "\{6B3A[0-9A-F]{12}\}"` returns nothing; the repo's highest existing prefix is `6B2256EB`.

### Input-conflict baseline (for Phases 5 and 6) — **SUPERSEDED 2026-08-08**
**Original (2026-08-07):** `check-input-conflicts.py --warnings` → exit 0, 0 errors, **22 warnings, 12 pre-existing**, 3 acknowledged. Phase 6 ended at 23 warnings, +1 being structurally unavoidable.
**Now (after merging `main`, 2026-08-08):** → exit 0, **0 errors, 0 warnings, 0 pre-existing, 3 combo notes, 1 acknowledged.**
**Why it moved:** main fixed both of the input bugs this feature filed — it rewrote the checker to parse inline-declared `ActionContext { Actions { … } }` actions (BUG-092, the blind spot that made both of this feature's surveys wrong) and rebound `OverthrowMainMenu` off bare `pad_down` onto an `LT + pad_down` chord (BUG-093, the root of 17 of the 23 warnings).
**Impact:** the "+1 structural warning" Phase 6 had to argue for **no longer exists**. Q5 is clean on its own terms, measured by an instrument that can now see what it is checking. Any future phase should hold **0/0/0** — there is no longer a pre-existing pile to hide behind.

### The wanted invoker is lazily allocated, not eagerly
**Date:** 2026-08-07 (Phase 0)
**Context:** D13 names `SCR_MapEntity.GetOnMapOpen()` as the pattern, but that base-game invoker is eagerly initialised at its declaration.
**Decision:** Lazy-allocate inside the getter instead, so `GetOnWantedLevelChanged()` can never return null regardless of static-init order.
**Impact:** Same one-subscription-per-session property D13 wanted; strictly safer. Phase 2 can subscribe unconditionally without a null guard on the getter itself.

---

## Contract for the `field-manual` feature (Phase 7.8)

> Everything the sibling feature `field-manual` needs in order to add manual pages without breaking
> the deep links, the vanilla manual, or itself. Written 2026-08-07 against the shipped code and
> measured behaviour, not against the plan. `implementation.md` §5 "Field-manual link" is the same
> contract in shorter form; this section is the authoritative one.

### 1. Where content goes

**`Configs/FieldManual/Categories/FM_Overthrow.conf`** (`{6B3A000000000090}`). New categories,
sub-categories, entries and pieces all go in that file. Its top-level object is a
`SCR_FieldManualConfigCategory` — the same shape as vanilla's `Configs/FieldManual/Categories/FM_*.conf`,
which is legal because `SCR_FieldManualConfigCategory` is `[BaseContainerProps(configRoot: true)]`.

**Do NOT edit `Configs/FieldManual/FieldManualConfigRoot.conf`.** That file is a **same-GUID delta
override of the base game's field-manual root** — its `.meta` declares `{17295EF80DC38D53}`, the exact
GUID `UI/layouts/Menus/FieldManual/FieldManual.layout:16` hands to `SCR_ConfigUIComponent.m_ConfigPath`
and therefore the exact resource `SCR_FieldManualUI.OnMenuOpen` loads. It is now six lines holding one
element, `{59908331EDFD9788}`, which inherits from `FM_Overthrow.conf`. Two facts about it are
load-bearing:

- **The element GUID `{59908331EDFD9788}` must never change.** It is absent from vanilla's root, and
  that absence is the entire reason the element *appends* instead of overwriting. Proven the hard way
  on 2026-08-07: changing it to vanilla's Introduction element GUID `{5668E0CC56064794}` made
  Overthrow's category **replace** the base game's Introduction category — 5 categories instead of 6,
  Introduction gone.
- **The root delta never declares `m_aTileBackgrounds`.** It inherits the base root's eight, and
  `SCR_FieldManualUI.c:253` calls `m_aTileBackgrounds.GetRandomElement()` **unguarded** for every tile.
  An override that stopped inheriting would not degrade the manual, it would error on its first tile.

`OVT_TEST_Init_FieldManual_DeltaMergesAndLinksResolve` (Init tier, in Fast and All) fails the build if
either fact stops holding.

### 2. Title keys ARE the link ids

`OVT_TutorialEntryConfig.m_sFieldManualTitleKey` holds the **`m_sTitle` localization key** of the target
manual entry — e.g. `#OVT-FieldManual_MainMenu_Title`. The seam is
`SCR_FieldManualUI.OVT_OpenEntryByTitle`, which walks `m_aAllEntries` comparing `entry.m_sTitle`.

- The match is **exact and case-sensitive**. `#OVT-FieldManual_MainMenu_Titel` matches nothing.
- **Renaming a title key breaks every popup pointing at it**, silently: the manual opens on its front
  page and logs a warning (that is the designed behaviour, I2 — an unknown link must never error).
  Treat a shipped title key as immutable, exactly like a tutorial entry id.
- Title keys must be **unique** across the whole merged manual. There are 141 entries in it today, 140
  of them the base game's, so an `#OVT-` prefix is what keeps a collision impossible.
- Only entries the UI can actually reach are linkable, because `m_aAllEntries` is what
  `SetAllEntriesAndParents` built — see the pruning rules below. Linking to a pruned entry lands on the
  front page, which is correct: the page genuinely is not in the manual.
- **Why title keys and not `EFieldManualEntryId`** (plan decision D12): `OpenEntry` matches only on that
  base-game enum, and there is no `FindEntry` / `GetEntryById` / name lookup anywhere in the class. A
  `modded enum` was considered and rejected — it needs the enum *plus* a per-entry conf field *plus* a
  string→enum map. Overthrow's one entry deliberately leaves `m_eId` at `NONE`; there is no valid
  vanilla enum value for an Overthrow page, and borrowing one would hijack a vanilla hint's deep link.

The Init case above also asserts that **every authored tutorial entry's non-empty
`m_sFieldManualTitleKey` resolves to a real entry title in the merged root**. No hard-coded keys — it
reads whatever `tutorial-content` authored — so a broken link is a red build, not a shrug in play.

### 3. TWO category levels only, and empty nodes are deleted

`SCR_FieldManualUI.SetAllEntriesAndParents` (`:600-663`) walks exactly **root category → sub-category →
entries** and no deeper. A third level is **silently dropped** — no warning, no error, the pages simply
are not in the manual. It also mutates the config object it was handed:

- an entry with `m_bEnabled 0` **or an empty `m_aContent`** is removed from its sub-category;
- a sub-category with `m_bEnabled 0`, or left with no entries **and** no sub-categories, is removed;
- a category with `m_bEnabled 0`, or left with no entries **and** no sub-categories, is removed.

So an Overthrow category whose only sub-category lost its last entry does not render as an empty
section — it **disappears entirely**. The pruning is safe to trigger repeatedly because
`SCR_ConfigHelperT.GetConfigObject` builds a fresh instance from the container on every open.

Presentation follows from the two levels: `CreateCategoryMenuWidgets` (`:667-699`) draws a **category
that has sub-categories as a heading** and each **sub-category as a clickable button** in the left-hand
list. That is why the Overthrow heading needed a differently-named button under it — until Phase 7.3 the
sub-category reused `#AR-FieldManual_Category_Introduction_Title` and rendered as a second button
literally named "Introduction". It is now `#OVT-FieldManual_Category_GettingStarted_Title`.

### 4. Do not build on the "Reforger / Mods" tab split

`SCR_FieldManualConfigRoot.m_sDefaultTabName` and `m_sModsTabName` are declared in the base game
(`SCR_FieldManualConfigRoot.c:7-11`) and **read nowhere** — `grep` across the whole base-game script
tree returns only the declarations. The tab split was never implemented. There is no supported way to
put Overthrow's pages in their own tab; the category list is the only structure the UI has.

### 5. There is no other modding seam

The manual resolves exactly **one** config path: `SCR_ConfigUIComponent` is a five-line class holding a
single `ResourceName`, and `SCR_FieldManualConfigLoader.LoadConfigRoot` only walks the object it was
handed. No `array<ResourceName>` of extra roots, no directory scan, no registration API. The same-GUID
delta is the entire mechanism, which is why §1's rules are not style preferences.

---

## Gotchas & Learnings

> **All 35 numbered gotchas, in one place.** Numbers 1–11 and 16–18 sit in this section; the rest live at the end of the session note for the phase that found them (search `### <n>.`).

1. Where content goes
2. Title keys ARE the link ids
3. TWO category levels only, and empty nodes are deleted
4. Do not build on the "Reforger / Mods" tab split
5. There is no other modding seam
6. `set<T>` is sorted, not insertion-ordered
7. EnforceScript parser gotchas found in Phase 1
8. The catalog says "nine" server-side invokers; there are TEN
9. `PLAYER_SKILL` cannot carry a skill key
10. `m_OnTownControlChange` is declared `ScriptInvoker<IEntity>` but invoked with `OVT_TownData`
11. Never hand-edit `Language/localization_Overthrow.<lang>.conf`
12. `map` is a reserved type name and cannot be a local variable
13. An RPC is never looped back to the sending machine — a listen-server host must be delivered locally
14. The manager's `id → entry` registry does not exist on clients
15. `OVT_Global.GetUI()` dereferences the controlled entity unguarded
16. `GetGame().SaveUserSettings()` is THROTTLED, and drops rather than defers
17. A `ModuleGameSettings` block omits members that equal their `[Attribute]` default
18. Setting a value back to its default DOES persist through the container
19. Nothing on a gamepad is free during gameplay — R3's premise was right
20. `OverthrowPlaceContext` and `OverthrowBuildContext` are live while `IsAnyContextActive()` is FALSE
21. `InputSourceCombo` layers over its parts; it does not suppress them
22. `m_bCanBeDisabled 0` defeats the hidden-button-kills-its-shortcut mechanism
23. `int / int` is integer division in EnforceScript
24. `OVT_UIManagerComponent.IsAnyContextActive()` is now `IsAnyContextBlocking()`
25. The base-game conf declares 197 actions INLINE inside `ActionContext` blocks
26. `gamepad0:shoulder_left` and `keyboard:KC_T` belong to VON, at Priority 110
27. Any 17th menu context that lists `MenuDown` adds one conflict-script warning
28. The field-manual "Learn more" MUST close the Overthrow popup, or B does the wrong thing
29. `m_bOpenedFromOutside` changes what Back means, and that is deliberate
30. `SCR_ConfigHelperT.GetConfigObject` builds a FRESH instance every call
31. The reference script tree has no `.meta` files
32. A `.conf` that omits a member is silently taking its `[Attribute]` default — fine for data, wrong for a template
33. `EHudLayers.ALWAYS_TOP` is the only way to draw over a MenuManager layout
34. `SCR_HUDManagerComponent.SetVisible` and `SetVisibleLayers` are different mechanisms, and only one of them is selective
35. Not one letter key is free in this game

---

### 1. The field-manual config is NOT orphaned
**Problem:** `requirements.md` (and the epic brief) describe `Configs/FieldManual/FieldManualConfigRoot.conf` as orphaned — "nothing loads it".
**Truth:** Its `.meta` declares GUID `{17295EF80DC38D53}` — the exact GUID `FieldManual.layout:16` hands to `SCR_ConfigUIComponent.m_ConfigPath`. It is a **same-GUID delta override of the vanilla root and it is what the manual actually loads.**
**Lesson:** Phase 7 is a restructure + seam, not a rescue. The plan's §2.1 records the three in-repo merge precedents and the `m_aTileBackgrounds` null-deref canary.

### 2. Same-GUID `.conf`/prefab overrides are DELTAS, not replacements
**Lesson:** Element-wise merge **by element GUID**; an element GUID absent from the parent is an *append*. This is what makes the field-manual category land as a sixth category, and what lets a third-party mod append tutorial entries with `m_aTutorialEntries + { … }`.

### 3. `m_OnPlayerSell` fires from a third, non-shop site
**Problem (found in Phase 0, not in the plan):** the plan's §5 trigger catalog lists `m_OnPlayerSell` as invoked only from `OVT_ShopTransactionComponent.c:376`. There is a third invoke site: `OVT_EconomyManagerComponent.c:1004`, inside `AddPlayerMoney(playerId, amount, doEvent = true)`.
**Impact:** A `PLAYER_SELL` tutorial trigger will **also** fire on any non-shop money grant routed through `AddPlayerMoney` with `doEvent` set — job rewards, for example. The signature `(playerId, amount)` matches, so nothing breaks; it is a *semantics* trap for `tutorial-content`.
**Lesson:** Prefer `PLAYER_TRANSACTION` (which carries the shop) over `PLAYER_SELL` when an entry must mean "sold something at a shop". Fold into §5 in the Phase 8 doc pass.

### 4. `OVT_CharacterSheetContext` had an undocumented matching `Remove`
**Problem:** the plan named the `Insert` at `:33` as the single listener. `OnClose` at `:184` holds the matching `Remove`.
**Lesson:** When repointing a `ScriptInvoker` subscription at a new wrapper method, grep for the `Remove` too — repointing only the `Insert` leaks one subscription per open/close cycle.

### 5. `array<T>.Remove(index)` does not retain order
**Problem:** `Remove` is the fast swap-with-last removal (`Types.c:260`), not an ordered shift. Using it for a head dequeue silently reorders the tail.
**Solution:** `RemoveOrdered` wherever order is part of the contract.
**Lesson:** This was a **real** defect caught by `_QueueOrdering`, not a synthetic red. It would have been invisible in play until two tips queued at different priorities — exactly the class of bug the Logic tier exists to catch.

### 6. `set<T>` is sorted, not insertion-ordered
**Lesson:** `Find` is O(log n) and `Get(n)` reads the sorted array, so `OVT_TutorialSeenStore.WriteTo` emits canonical order. Harmless — nothing reads the seen list positionally, and it makes the stored profile block diff-stable — but do not assume insertion order downstream.

### 7. EnforceScript parser gotchas found in Phase 1
- **`out` and `event` are reserved words** — both are rejected as local-variable or parameter names.
- **`SetResultFailure` accepts at most three format arguments.**

### 8. The catalog says "nine" server-side invokers; there are TEN
**Problem:** the plan's Phase 2 brief and the `OVT_TutorialEvent` doc comment both say nine server-side invokers. §5's table lists ten non-client-local rows: eight per-player (`PLAYER_BUY`, `PLAYER_SELL`, `PLAYER_TRANSACTION`, `PLAYER_PLACE`, `PLAYER_BUILD`, `PLAYER_RECRUIT_ADDED`, `PLAYER_SKILL`, `PLAYER_WANTED`) plus the two global ones.
**Resolution:** all ten are subscribed. Nothing was dropped; only the count was wrong.

### 9. `PLAYER_SKILL` cannot carry a skill key
**Problem:** §5 says `m_sFilter` = skill key for `PLAYER_SKILL`. Phase 0 gave `m_OnPlayerSkill` the signature `ScriptInvoker<int>` — playerId and nothing else. Both invoke sites (`OVT_SkillManagerComponent.c:120`, `:341`) pass only the id; the key is available at `:120` but is not carried.
**Impact:** a `PLAYER_SKILL` trigger's `m_sFilter` is always `""`. An entry that must mean "bought THIS skill" is not expressible today. Fixing it means widening the invoker to `<int, string>` and touching its one other listener — deliberately NOT done in Phase 2, which was scoped to add no signature changes. Fold the correction into §5 in the Phase 8 doc pass, or widen the invoker if `tutorial-content` actually needs it.

### 10. `m_OnTownControlChange` is declared `ScriptInvoker<IEntity>` but invoked with `OVT_TownData`
**Problem:** `OVT_TownManagerComponent.c:143` declares `ref ScriptInvoker<IEntity> m_OnTownControlChange`, and `:676` invokes it with an `OVT_TownData`. The template argument is decorative — `ScriptInvoker.Invoke` is untyped at runtime.
**Truth:** the existing listeners (`OVT_OccupyingFactionManager.OnTownControlChanged`, `OVT_RevolutionaryMomentumSupportModifier`) both take `OVT_TownData`, which is the real contract. The tutorial handler follows them and reads `town.location` for the proximity fan-out.
**Lesson:** never trust a `ScriptInvoker<T>` declaration in this codebase — read an existing listener or the invoke site.

### 11. Never hand-edit `Language/localization_Overthrow.<lang>.conf`
**Lesson:** Workbench-generated exports; their `Ids{}`/`Texts{}` blocks are neither parallel nor same-length. Only `localization_Overthrow.st` is editable. Q11 gates this with `git diff --stat Language/`.

### 16. `GetGame().SaveUserSettings()` is THROTTLED, and drops rather than defers
**Measured 2026-08-07 against the real profile file.** Two `SaveUserSettings()` calls microseconds apart leave only the **first** on disk — the second is discarded, not queued, and 35 further seconds of test execution never flush it. Two calls **six seconds** apart both land.
**Impact:** D8's "flush on every mutation" still holds, but not because every flush lands. It holds because `OVT_TutorialSettingsAccessor.Save` writes the **whole record** every time rather than a delta — so a dropped flush loses nothing permanently, and the next mutation carries every id again. A delta writer would have silently lost ids.
**Residual exposure:** an unclean exit in the seconds after a *second* dismissal costs one repeated tip. Acceptable and self-healing.
**Consequence for tests:** any autotest that dirties the settings store must wait out the window before its cleanup write, or the cleanup is silently discarded and the CI profile stays polluted. `OVT_TEST_Init_Tutorial_SettingsStoreRoundTrips` waits 10 s for exactly this reason, which is why it is the slowest case in the Init suite.

### 17. A `ModuleGameSettings` block omits members that equal their `[Attribute]` default
Most vanilla blocks in `ReforgerGameSettings.conf` are literally `SCR_AudioSettings { }`. A cleaned `OVT_TutorialSettings` therefore reads `{ m_iVersion 1  m_bTipsDisabled 0 }` with **no `m_aSeen` block at all** — an absent member is the empty state, not corruption. This is also why `WriteToInstance` legitimately hands back a **null** array and why the accessor's `if (!settings.m_aSeen)` guard is mandatory rather than defensive.

### 18. Setting a value back to its default DOES persist through the container
Worth stating because gotcha 17 makes the opposite plausible: `ReadFromInstance` with `m_bTipsDisabled = false` really does clear a previously-stored `1`, and an emptied array really does empty the stored one. Asserted by `RestoreProfile()` inside the gate case, so a future engine change that made the toggle one-way would go red instead of shipping.

---

## Testing Approach

### Automated — this feature adds **9** cases (the plan forecast 7)

- **Logic tier** — `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_Tutorial.c`, 5 cases: trigger matching, matcher select+order, queue ordering, gate predicate, seen store. World-free; every subject built with `new`.
- **Init tier** — **4** cases appended to `OVT_TEST_InitSuite.c`: manager resolves + entries load; every catalogued invoker seam exists; the settings store round-trips through the real `BaseContainer` (the R1 gate); the field-manual delta merges and every tutorial deep link resolves (the R2 gate, 3 assertion branches).
- **3 of the 9 were proven red non-synthetically** — by genuine defects rather than deliberate breakage (`_QueueOrdering`, and two config-shape guards).
- **Fallibility rule:** every new case proven red once, method recorded below. No `maxAttempts`, ever.

### Commands
```bash
tools/compile-check.sh                                    # expect 0
tools/run-tests.sh "{6A6E29FF47ECB840}"                   # Fast  38 -> 47
tools/run-tests.sh "{6A6E2A002F53A581}"                   # All   66 -> 75
python3 .claude/skills/overthrow-ui-patterns/scripts/check-input-conflicts.py --warnings
git diff --stat Language/                                 # expect ONLY localization_Overthrow.st
```

### Proven-red record
| Case | How it was made to fail | Date |
|---|---|---|
| `OVT_TEST_Logic_Tutorial_TriggerMatching` | `OVT_TutorialTrigger.Matches`: `ctx.m_iValue < m_iMinValue` → `<=`. Failure: *"A minimum of 500 rejected a value of exactly 500; the threshold must be inclusive"* | 2026-08-07 |
| `_MatcherSelectsAndOrders` | `OVT_TutorialMatcher.FindMatches`: `if (!entry.m_bEnabled)` → `if (false)`. Failure: *"FindMatches returned [retired, high, mid, twin, low-a, low-b], expected [high, mid, twin, low-a, low-b]"* | 2026-08-07 |
| `_QueueOrdering` | **Not synthetic — a genuine pre-fix defect.** The first implementation used `array.Remove(0)` in `TryDequeue`; `Remove` is swap-with-last, so the head dequeue dropped the lowest-priority entry into slot 0. Failure: *"Dequeue 1 gave 'low-second', expected 'high-second'"* | 2026-08-07 |
| `_GatePredicate` | `OVT_TutorialGate.CanShowNow`: `if (blockingUiOpen)` → `if (false)`. Failure: *"The gate allowed a popup while a blocking UI (menu, context or the map) is open"* | 2026-08-07 |
| `_SeenStore` | `OVT_TutorialSeenStore.MarkSeen`: `Count() >= MAX_SEEN` → `> MAX_SEEN`. Failure: *"MarkSeen past the cap reported success"* | 2026-08-07 |
| `OVT_TEST_Init_Tutorial_ManagerResolvesAndLoadsEntries` | Emptied `m_aEntries` on `Prefabs/GameMode/OVT_OverthrowGameMode.et` (the one `OVT_TutorialEntryConfig` row deleted, component kept). Failure: *"No tutorial entries registered: m_aEntries on the game mode prefab is empty, so the tutorial framework has nothing to deliver"* — exit 1; restoring the row returned it to exit 0 | 2026-08-07 |
| `OVT_TEST_Init_Tutorial_SettingsStoreRoundTrips` | Removed `[Attribute()]` from `OVT_TutorialSettings.m_aSeen` — the member is then simply not serialized, which is precisely R1's failure mode rather than a synthetic break. Failure: *"The seen store came back with 0 ids, expected 2. The nested ref array&lt;ref OVT_SeenTutorialEntry&gt; did NOT survive the settings container - risk R1 has fired and the feature needs one of its ranked fallbacks."* — exit 1; restoring the attribute returned it to exit 0 | 2026-08-07 |
| `OVT_TEST_Init_FieldManual_DeltaMergesAndLinksResolve` (branch 1 of 3, the R2 guard) | Changed the root delta's element GUID from `{59908331EDFD9788}` to vanilla's Introduction element GUID `{5668E0CC56064794}` — a realistic copy-paste mistake, not a synthetic break. Failure: *"STOP - SAME-GUID MERGE SEMANTICS FALSIFIED. The merged field-manual root has 5 categories, only 4 of which are the base game's…"*, and the logged inventory showed vanilla's Introduction **replaced** by Overthrow's category. Exit 1; restoring the GUID returned it to exit 0 | 2026-08-07 |
| …(branch 2 of 3, the D12 link guard) | `Configs/Tutorials/proofFirstBuy.conf`: `m_sFieldManualTitleKey` → `#OVT-FieldManual_MainMenu_Titel` (one-letter typo — the exact way a link really rots). Failure: *"Tutorial entry 'economy-first-buy' links to field-manual page '#OVT-FieldManual_MainMenu_Titel', and no entry in the merged manual has that m_sTitle."* — exit 1 | 2026-08-07 |
| …(branch 3 of 3, the 7.3 nit guard) | `FM_Overthrow.conf`: put the sub-category title back to vanilla's `#AR-FieldManual_Category_Introduction_Title`. Failure: *"Overthrow's field-manual sub-category is titled '#AR-FieldManual_Category_Introduction_Title' - the BASE GAME's Introduction key."* — exit 1 | 2026-08-07 |
| `OVT_TEST_Init_Tutorial_InvokerSeamsExist` | `OVT_SkillManagerComponent.c:32`: `ref ScriptInvoker<int> m_OnPlayerSkill = new ScriptInvoker<int>();` → `ref ScriptInvoker<int> m_OnPlayerSkill;` (declared, never allocated — the realistic shape of the rot this case exists to catch). Failure: *"Tutorial trigger seam missing: OVT_SkillManagerComponent.m_OnPlayerSkill (PLAYER_SKILL). OVT_TutorialManagerComponent subscribes to it in SubscribeToInvokers(); with it gone that trigger silently never fires again."* — exit 1; restoring the allocation returned it to exit 0 | 2026-08-07 |

### Not automated (play-test only)
Rendering, input and gamepad navigation; the settings store's cross-restart round trip; per-player MP delivery and JIP; the field-manual override's **visual** result. All enumerated in the plan's §8 verification method and tracked in `tasks.md` → **Needs Human Verification**.

Phase 7 narrowed that last one considerably. The merged config object is now asserted (six categories, eight tile backgrounds, the Overthrow sub-category's title, every deep link resolving), so what is left for a human is genuinely only what the harness cannot see: that the six categories **draw**, that the tiles have art behind them, and that `OpenMenu` → `OVT_OpenEntryByTitle` lands on the page rather than the front page — an ordering property of `MenuManager` (`OnMenuShow` calls `SetCurrentEntry(null)`) that only a running menu can demonstrate.

---

## Open Questions

- [x] **Q:** Does the nested `ref array<ref OVT_SeenTutorialEntry>` round-trip through `ModuleGameSettings`?
      **A:** **YES — R1 discharged 2026-08-07.** Both ids and the flag survive the container round trip AND appear in `$profile:.save/settings/ReforgerGameSettings.conf` as a readable `OVT_TutorialSettings` block (quoted in `tasks.md` Phase 4). None of the three fallbacks is needed.
- [ ] 🔄 **Q (REOPENED 2026-08-08):** Is there a genuinely free gameplay-context binding for the escalation key (F5)?
      **A:** **Unknown again — the two surveys that closed this are now out of date.** Merging `main` moved several bindings (`OverthrowMainMenu` → `LT+pad_down` chord; loadout actions onto `shoulder_right`/`right_trigger`; others off `shoulder_left`/`thumb_*`) *and* replaced the checker with one that can see inline-declared actions — the exact blind spot that made both previous surveys unreliable. Re-surveying is cheap and the answer may now be yes. **Not attempted during the merge** (scope), so F5 remains deferred until someone looks.
      *The closed 2026-08-07 answer, retained for history:*
- [x] **Q:** Is there a genuinely free gameplay-context binding for the escalation key?
      **A:** **NO — R3 fired in Phase 5.4 and was CONFIRMED on re-survey in Phase 6.8. The question is closed.** Phase 6.7 removed the place/build collision that was supposed to free `shoulder_left`, and it made no difference: `VONContext` holds `shoulder_left` (and `KC_T`) at Priority 110 every frame the player is alive. `KC_K` and `KC_O` are `GadgetCompass`/`GadgetWatch`. All six are declared inline inside their `ActionContext` blocks, which is why an `ActionRefs`-only survey missed them (gotchas 25 and 26). The escalation route is the Overthrow main menu's Tips entry.
      *Phase 5's answer, retained:* **R3 has fired, and its fallback was taken in Phase 5.4.** On the keyboard, `KC_K`/`KC_O`/`KC_T` are unbound in both confs. On the gamepad, **all sixteen inputs are bound in at least one context that can be live while a non-modal HUD popup is up**; the only two free as single presses in *base* gameplay, `shoulder_left` and `shoulder_right`, are Overthrow's own `OverthrowRotateLeft/Right` in `OverthrowPlaceContext` / `OverthrowBuildContext` — which run with `IsActive()` false and so are *not* covered by the pipeline gate. Combos do not help (see gotcha 21). No action and no `ActionContext` were added; the popup's prompt reuses the existing `OverthrowMainMenu`. Full survey in `tasks.md` → Phase 5 → "R3 has fired".

---

## Session Notes

### 2026-08-08 — merged `main` back in (merge commit `7efb1c44`, 11 commits)

- **All five bugs this feature filed are fixed upstream and closed: BUG-090…094.** The three tech-debt entries in `tasks.md` are ticked.
- **One conflict, `Configs/System/chimeraInputCommon.conf`** — both sides had *added* actions at the same offset (ours: the four `OverthrowTutorial*`; main's: `OverthrowLoadoutsApplyToRecruit`/`ApplyToAll`). Nothing in competition, so all six were kept and both `ActionRefs` lists verified intact. Everything else auto-merged.
- **The auto-merge of `OVT_SkillManagerComponent.c` was checked, not assumed** — main's "Shop buy/sell XP cheese" fix touches the same file this feature changed the `m_OnPlayerSkill` signature in. Both `Invoke` sites still pass `(playerId, key)`, matching `ScriptInvoker<int, string>`. A wrong arity here would have compiled fine and failed only at runtime, since `ScriptInvoker.Invoke` is untyped.
- **Q5 went from "clean with an argued exception" to clean.** 23 warnings / 12 pre-existing → **0 / 0**. See the superseded baseline above.
- **Our four tutorial bindings survived main's rebinding sweep unchanged**, and that was verified *positively*: injecting a deliberate collision made the new checker emit `ERROR gamepad0:x  OverthrowTutorialBack, OverthrowTutorialNext` and exit 1, proving it parses `OverthrowTutorialMenuContext` rather than silently skipping it. Reverted immediately. "Zero findings" from an instrument that cannot see you is not a pass.
- **F5 is reopened as a question** (see Open Questions) — not re-attempted.
- Gates on the merged tree: compile-check **0** (5939 files) · Fast **47** · All **77** (main added 2 Campaign cases) · conflicts **0/0/0**.

---

> Newest phase first. Each block ends with the gotchas that phase discovered — see the index under **Gotchas & Learnings** for where each number lives.

### 2026-08-11 — tips on the screens they are about, a field-manual link on the HUD tip, and a new base trigger (post-close, user-requested)

Three user-reported gaps, all in the same sitting. Gates after all three: compile-check **0** (5946 files) · Fast **54** · All **92** · `check-input-conflicts.py --warnings` **0 errors / 0 warnings / 3 combo notes** · `git diff --stat Language/` **empty**.

**1. A tip about a screen is now drawn ON that screen.** Reported as "the first time I open the map I don't see the tutorial until I close the map again". New per-entry `OVT_TutorialEntryConfig.m_bShowOverUI`, default 0, set on the four entries whose trigger *is* a screen — `map-first-open`, `home-first-open`, `skills-first-open`, `place-first-placeable`. It waives the gate's `blockingUiOpen` veto **and only that veto**, in both directions: `OVT_TutorialGate.CanShowNow` gained a fifth argument, and `OVT_TutorialInfo.Tick` stops retiring the popup when a blocking UI opens. Three things had to be true for that to be visible at all:
- **`OVT_TutorialQueue.TryPeek`** is new. The gate is no longer a question about the world alone, so the pump must know *which* entry it is about to show before deciding. Dequeue-then-requeue was rejected: a refused entry would fall to the back of its priority band every tick.
- **`OVT_TutorialInfo` moved to `EHudLayers.ALWAYS_TOP`** (`m_eLayer` on `Character_Player.et`). That is the only HUD layer that can draw over the map, because `SCR_HUDManagerComponent.CreateHUDLayers` parents it to a second root created with `SetZOrder(100)` and says why in its own comment. The map is a MenuManager menu (`SCR_MapMenuUI`), and nothing in the base game hides the HUD when it opens.
- **`OVT_UIContext` stopped calling `hud.SetVisible(false)`** and now calls `SetVisibleLayers(ALWAYS_TOP)` / `SetVisibleLayers()`. `SetVisible` blanks *both* roots, so an Overthrow menu would have hidden the tip it was supposed to be under. Everything `m_bHideHUDOnShow` ever meant to hide is on BACKGROUND..OVERLAY and is still hidden, and `OVT_UIContext` is the **only** file in Overthrow that touches the HUD manager at all (grepped), so the mechanism change is fully contained.

**1b. A show-over-UI tip drops the "Overthrow Menu" prompt.** Follow-up from the same session, and it falls straight out of what the flag means: an `m_bShowOverUI` tip is on screen *because* the player opened something, so offering "open the Overthrow menu to read more" is wrong twice — they are already in a menu, and the route it advertises would replace the very screen the tip explains. Learn more stays, and is the better answer anyway since it lands on the page. Hiding disarms the handover for free (`m_bCanBeDisabled` defaults to 1) but takes nothing from the player: `OverthrowMainMenu` is a real action in `OverthrowGeneralContext` and still opens the menu. **Consequence a future author must know, now in the attribute's own doc comment:** a show-over-UI entry with no `m_sFieldManualTitleKey` shows *no* prompts at all. All four flagged entries have one.

**2. The non-modal HUD tip has a "Learn more" prompt.** It reuses the existing `#OVT-Tutorial_LearnMore` string and the existing `OVT_FieldManualHelper.Open()`, mirrors the modal's handler (open first, tear down only on success), and **dismisses rather than releasing** — following the link means the tip was read, unlike the escalation prompt beside it. Hidden for an entry with no `m_sFieldManualTitleKey`, which also makes the shortcut inert, because `SCR_InputButtonComponent.OnInput` early-outs on `m_bCanBeDisabled && !IsVisibleInHierarchy()` and that attribute defaults to 1.

**F5's premise finally moved, and the answer was still "nothing is free" — so this took the other road.** The survey was re-run against the post-BUG-092 checker: **there is not one free letter key** (all 26 bound across the two confs) and no free gamepad input, and the modal's own `KC_F`/`RB` are `CharacterAction`/`PerformAction` and `Freelook`/`FocusToggle` during gameplay. Rather than steal one, the new action `OverthrowTutorialHudLearnMore` binds **`keyboard:KC_F5`** (verified unbound in both confs) plus an **`left_trigger + y` `InputSourceCombo`**, in a new `OverthrowTutorialHudContext` that `OVT_UIManagerComponent.EOnFrame` activates **only while a non-modal tip is on screen**. Nothing is taken on keyboard, ever; the pad chord still fires its own parts, which is how `InputSourceCombo` works everywhere (gotcha 21) and the trade already accepted for `LT+pad_down`. **Chosen by the mod owner from three costed options** — the alternatives were reusing `F`/`RB` (single press, but no interact and no freelook for 20 s) and keyboard-only.
- The context is activated from `OVT_UIManagerComponent`, **not** from the HUD display, because an `ActionContext` must be activated every frame and `SCR_InfoDisplay.UpdateValues` does not run while the HUD is hidden — which is the exact situation this change set creates. That is also why the display has driven itself off a 100 ms `CallLater` since Phase 5.
- **The checker was proven to see the new context before its clean report was believed:** injecting a `KC_N` collision between `OverthrowTutorialHudLearnMore` and `OverthrowTutorialDisable` produced `ERROR keyboard:KC_N` and exit 1; reverted, back to 0/0.

**3. `bases-first-capture` fires on arrival, not on the outcome.** It was bound to `BASE_CONTROL_CHANGE` — a battle *already won* — which is the last moment a player needs to be told how bases work. New event **`PLAYER_ENTER_BASE`** (the eleventh server-side invoker; nine of them per-player now), raised by `OVT_PlayerWantedComponent.CheckBaseRangeForTutorial` as an **edge**, not a state, when a player crosses inside `baseCloseRange` of a base that `IsOccupyingFaction()`.
- It lives in the wanted component because that is the only per-player server tick that already resolves the nearest base every second, at the same radius. It sits **above** the `m_bWantedSystemEnabled` guard and outside the disguise branch on purpose: neither has anything to do with whether the player has arrived somewhere worth explaining. Recruits are excluded by the `playerId <= 0` test alone.
- The manager unsubscribes in `OnDelete` beside the wanted invoker — the same static-outliving-its-world class of bug that commit `982e8ba9` fixed.
- **The mechanic was verified before any text was written**, because the shipped string's own Comment carries a fact-checked "DO NOT describe capture as taking or planting a flag". Both are true and the distinction is the point: `OVT_CaptureBaseAction` (15 s, `#OVT-CaptureBase`) is mounted on `BaseFlag_US.et:18-26`/`BaseFlag_USSR.et` and **starts** the battle, while the flag *material* merely follows the faction change afterwards. `OVT_PlayerCommsComponent.RpcAsk_StartBaseCapture:192-217` re-checks `baseCloseRange` server-side — **the same radius as the new trigger**, so the tip fires exactly when the action becomes available.

**Owed, and deliberately not done: two `.st` edits.** `Language/localization_Overthrow.st` is an unresolved merge (`UU`) owned by another session, so nothing in `Language/` was touched. The exact blocks are written up in the scratchpad (`owed-string-edits.md`): the base-capture body (which still describes the old both-directions event) and two now-false authoring Comments. **No new string ids and no export are owed** — the HUD's Learn more prompt reuses the modal's existing key.

**New gotchas from this change set:**

### 33. `EHudLayers.ALWAYS_TOP` is the only way to draw over a MenuManager layout
`SCR_HUDManagerComponent.CreateHUDLayers` builds two roots and gives the second `SetZOrder(100)` with the comment *"set high to be always on top, even above MenuManager layouts"*; `ALWAYS_TOP` is the one layer parented to it. Everything else in the HUD is under a root that menus cover. The in-game map is a menu (`SCR_MapMenuUI` opens it through `SCR_MapEntity.OpenMap` with the menu's own root widget), so "draw over the map" and "draw over a base-game menu" are the same problem with the same one-line answer.

### 34. `SCR_HUDManagerComponent.SetVisible` and `SetVisibleLayers` are different mechanisms, and only one of them is selective
`SetVisible(bool)` hides the two ROOT widgets; `SetVisibleLayers(mask)` hides the per-layer frames beneath them. They do not restore each other: hiding with `SetVisible(false)` and showing with `SetVisibleLayers()` leaves the HUD dark. Any code that wants "hide the HUD except X" must use the layer mask on both sides of the pair.

### 35. Not one letter key is free in this game
Every `KC_A`..`KC_Z` is bound in `chimeraInputCommon.conf` (base plus mod), so "find an unused letter" is not an available answer to an input question — `KC_F3`..`KC_F8`, `KC_F11`, `KC_F12`, `KC_MINUS`, `KC_EQUALS`, `KC_BACKSLASH`, `KC_APOSTROPHE`, `KC_INSERT`, `KC_END`, `KC_PGUP`, `KC_PGDN` and three numpad keys are what is actually left. On the gamepad nothing at all is free, which is R3's original finding and it still holds; a combo plus a context that is live only while the UI is up is the way around it, and it is what the base game does for its own hint system (`HintContext` sits on `gamepad0:view` alongside `GadgetMap`).

### 2026-08-07 — Phase 8 complete (feature build finished)


- **8.1** — `proofFirstBuy.conf` was already correct against the shipped schema; what Phase 2 left provisional was that it leaned on **attribute defaults** for `m_ePresentation`, `m_iPriority` and `m_bEnabled`. All three are now written out explicitly. That is deliberate: §5 tells `tutorial-content` to copy this file, and a template that omits three of its nine fields teaches by omission.
- **8.2** — `proofWelcome.conf` (`welcome-intro`): `MODAL`, two pages, `m_iPriority 100`, one `PLAYER_SPAWNED` trigger, and **no field-manual link**. The missing link is a decision, not an oversight — the two proof entries now cover *both* Learn-more branches between them, so verifying "the button is absent on an unlinked entry" no longer requires temporarily vandalising a shipped key.
- **The priority pair is also deliberate.** 100 vs 0 makes F9 (one-at-a-time, priority order) verifiable with the shipped data alone: spawn, then buy, and the welcome must win. The plan's DoD step 12 asked for a throwaway third entry to demonstrate this; it is no longer needed.
- **8.3** — registered as `{6B3A0000000000A4}` beside `{6B3A000000000011}`.
- **8.4** — five string items (`{6B3A0000000000B0}`…`B4`). The **complete Phase 5–8 export list is 17 keys** and is tabulated in `tasks.md`. It was collated from `git diff -U0 Language/localization_Overthrow.st` rather than from the phase reports; the reports turned out to be exactly right, with nothing missing and nothing extra.
- **8.5 / I6 — §5 was corrected against shipped code**, which is what I6 actually demands and what three sibling features will be built from. Four deltas folded in: `PLAYER_SELL`'s second invoke site and the "prefer `PLAYER_TRANSACTION`" advice; `PLAYER_SKILL` genuinely carrying the skill key after task 3.0; **ten** invokers not nine; and `m_OnTownControlChange`'s decorative template argument. §5 also gained a presentation/priority subsection, the framework-owned chrome-key list, and an add-an-entry procedure that names both proof entries and the two Init cases that police a bad one.
- **The add-an-entry procedure was executed, not just written.** `welcome-intro` went in with one `.conf` + `.meta`, five string items and one prefab element — **zero lines of EnforceScript** — and compile-check stayed 0 with All at 75. That is F10, demonstrated rather than asserted.
- **One code doc defect fixed:** `OVT_TutorialEvent`'s header comment said "Nine of these are raised on the SERVER". It is ten.
- Gates: compile-check **0** (5937 files); **Fast 47**; **All 75**; `check-input-conflicts.py --warnings` exit **0** / 0 errors, 23 warnings, 12 pre-existing, 3 acknowledged — byte-identical to Phase 6/7 (no conf touched); `git diff --stat Language/` = the `.st` only, 289 insertions, 0 deletions; `OVT_PlayerCommsComponent` RPC count **132**, file unmodified.
- **Definition of Done walked in full: 8 ✅ MET, 20 👤 HUMAN, 1 ⚠️ DEFERRED.** The deferral is **F5** and it is R3's, not a shortcut — see Phase 5.4 and Phase 6.8. The high HUMAN count is structural: this is a UI feature in a project where UI is play-test-only by rule, and rendering, input and multiplayer are all outside the harness. Every HUMAN row names the exact action.

**New gotcha from this phase:**

### 32. A `.conf` that omits a member is silently taking its `[Attribute]` default — fine for data, wrong for a template
`proofFirstBuy.conf` behaved correctly with `m_ePresentation`, `m_iPriority` and `m_bEnabled` absent, because the engine fills them from the attribute defaults. But it is the file §5 tells every content author to copy, and a copy inherits the omissions. Any `.conf` that doubles as a template should spell out every field, including the defaulted ones. (Same family as gotcha 17, where an omitted member in a settings block *is* the clean state — omission and default are indistinguishable on the way in.)

---

### 2026-08-07 — Phase 7 complete (🚧 R2 discharged by measurement)


- **7.1 — THE MERGE SPIKE. Verdict: the delta merges. R2 is discharged.** The plan asked for a
  launch-and-look; a screen cannot be looked at from here, so the automatable half was built instead and
  is now permanent coverage: `OVT_TEST_Init_FieldManual_DeltaMergesAndLinksResolve` loads
  `{17295EF80DC38D53}` through `SCR_FieldManualConfigLoader.LoadConfigRoot` — the menu's own entry
  point, not a reimplementation — and measured **6 top-level categories** (`#AR-FieldManual_Category_Introduction_Title`,
  `#AR-Editor`, `#AR-FieldManual_Page_Conflict_Title`, `#AR-FieldManual_Category_Gameplay_Title`,
  `#AR-FieldManual_Category_Equipment_Title`, `#OVT-FieldManual_Category_Overthrow_Title`), **8 tile
  backgrounds** and **141 entries**. The tile-background number is the strong one: Overthrow's file
  never declares `m_aTileBackgrounds`, so eight of them can only mean the base root's value survived.
- **The merge mechanism was then demonstrated directly, not just inferred.** Red-proof branch 1
  repointed the delta's element GUID at vanilla's Introduction element and the Overthrow category
  **replaced** Introduction (5 categories, Introduction gone). Element-wise merge keyed on element GUID
  is now an observed behaviour of this engine, not a belief.
- **7.2** — content moved to `Configs/FieldManual/Categories/FM_Overthrow.conf` (`{6B3A000000000090}`,
  six platform configurations in its `.meta`). The root delta is six lines: one element,
  `{59908331EDFD9788}` unchanged, inheriting from the new file. Re-measured after the move: still 6 / 8 /
  141. `field-manual` should never need to touch the overridden file again.
- **7.3** — the sub-category is `#OVT-FieldManual_Category_GettingStarted_Title` ("Getting Started")
  instead of the base game's Introduction key, so the category list no longer shows two "Introduction"
  buttons. The entry keeps `#OVT-FieldManual_MainMenu_Title`: that key IS the published link id (plan
  §5 already names it as the example target), and renaming a title key is precisely the operation D12
  forbids. `m_eId` deliberately stays `NONE` — see the contract section, §2.
- **7.4 / 7.5** — `modded class SCR_FieldManualUI` with `OVT_OpenEntryByTitle(string)` and
  `static OVT_OpenByTitle(string)`. No override, no `super` call: vanilla's two open routes run
  byte-identical code. The static navigates **after** `OpenMenu` returns, with the reason written at the
  call site so nobody "tidies" it into an `OnMenuOpen` override, and it null-guards the `OpenMenu`
  result — which vanilla does not.
- **7.6** — `OVT_FieldManualHelper.Open(titleKey)` in `Scripts/Game/Global/`. Returns bool so a caller
  can decline to close itself when the manual refused to open; an unknown key is a warning plus the
  front page, never an error (I2).
- **7.7** — `OVT_TutorialContext.LearnMore` now opens the manual and closes the popup, and
  `Configs/Tutorials/proofFirstBuy.conf` links to `#OVT-FieldManual_MainMenu_Title`.
- **7.8** — the whole contract for `field-manual` is the new "Contract for the `field-manual` feature"
  section above.
- Gates: compile-check **0** (5937 files); **Fast 47**; **All 75**; `check-input-conflicts.py --warnings`
  **exit 0 / 0 errors, 23 warnings, 12 pre-existing, 3 acknowledged** — byte-identical to the Phase 6
  baseline, because `chimeraInputCommon.conf` was not touched (the Learn-more action
  `OverthrowTutorialLearnMore` already existed from Phase 6.4); `git diff --stat Language/` shows only
  the `.st`.
- **One new string key for the user to export: `OVT-FieldManual_Category_GettingStarted_Title`**
  ("Getting Started").

**New gotchas from this phase:**

### 28. The field-manual "Learn more" MUST close the Overthrow popup, or B does the wrong thing
`OVT_UIContext.EOnFrame` (`:109-117`) calls `m_InputManager.ActivateContext(m_sContextName)` **every
frame** a context is active. Leaving `OVT_TutorialContext` up behind the Field Manual would keep
`OverthrowTutorialMenuContext` live underneath it, and both it and the manual bind `MenuBack` and
`MenuSelect` — so on a gamepad, B would dismiss the tip instead of leaving the manual's reading panel.
The popup opens the manual first and closes itself only on success, so a manual that refuses to open
leaves the player with their Dismiss button rather than nothing.

### 29. `m_bOpenedFromOutside` changes what Back means, and that is deliberate
With it set, `CloseMenuOrReadingPanel` (`:831-837`) closes the **whole manual** instead of returning to
the tile grid. `OVT_OpenEntryByTitle` sets it for the same reason vanilla's `OpenEntry` does: a player
who arrived from a tip did not choose the grid they would otherwise be dropped onto.

### 30. `SCR_ConfigHelperT.GetConfigObject` builds a FRESH instance every call
`BaseContainerTools.LoadContainer` → `CreateInstanceFromContainer`, no cache. That is why an Init case
can load the field-manual root without disturbing the real menu, and it is also what makes
`SetAllEntriesAndParents`' destructive pruning safe to run on every open.

### 31. The reference script tree has no `.meta` files
`/mnt/n/Projects/Arma 4/ArmaReforger` ships `.conf`/`.layout` sources but **zero** `.meta` files, so a
vanilla resource's GUID can only be read from wherever it is *referenced* (here, `FieldManual.layout:16`
and the root's own inheritance lines). Do not go looking for a base-game `.meta` to confirm a GUID.

---

### 2026-08-09 — `TutorialImage` re-shaped to a wide header (post-close change, user-requested)

Both popup layouts drew the optional page image **square** — 256×256 in the modal, 128×128 on the HUD tip. That was never a design decision: **no shipped entry has ever set `m_sImage`** (all eleven carry `m_sImage ""`), so the geometry had never been exercised or reviewed. The user asked for a wide header band instead, ahead of authoring the first real screenshots.

**What changed — two `.layout` files, no script:**

- **`UI/Layouts/Menu/TutorialPopup.layout`** — `TutorialImage` **moved out of `BodyRow`** (a HorizontalLayout, which is why it rendered *beside* the text) and up into `TutorialLayout` as a sibling **immediately after `UpperStripe`**. `Size 256 256` → **`Size 952 238`**; slot padding `0 0 20 0` (a right-hand gutter to the text) → `0 0 0 12` (a bottom gutter to the body); `VerticalAlign 1` → `HorizontalAlign 1`. `BodyRow` survives holding only `TutorialBody`.
- **`UI/Layouts/HUD/TutorialPopup.layout`** — no move needed; this one **already** stacked the image between title and body. `Size 128 128` → **`Size 428 107`**.

**No `.c` change was needed** — `OVT_TutorialContext.c:416` resolves the widget with `FindAnyWidget("TutorialImage")`, so relocating it in the tree is invisible to script. Worth knowing before anyone assumes a layout move implies a context change.

**No new GUIDs were allocated, deliberately.** The widget keeps `{6B3A00000000004A}` because it is the same widget, relocated. Its slot GUID `{59A6DA4C4FC5D425}` is also kept: slot GUIDs in this codebase are **shared class ids, not unique instance ids** — that one appears in 6 layouts and 9 times repo-wide, and `{56EEDE07C9F827C2}` (UpperStripe's) appears in 7. Verified before editing rather than assumed, because "every widget and slot gets a fresh GUID" is the usual rule and it does **not** apply to slots.

**Sizing rationale, for whoever authors the art:**
- Usable widths are **952** in the modal (1000 panel, 24 inset each side) and **428** on the HUD (`WidthOverride 460`, 16 px padding each side).
- **One asset feeds both widgets** — same `m_sImage`, presentation mode picks the layout — so both *must* share an aspect ratio or it distorts in one. 4:1 satisfies both (952×238 and 428×107). Recommended source: **2048×512** — power-of-two for clean mips, and BCn compression needs multiples of 4 regardless.
- ⚠️ **Vertical budget, not taste, is what caps the ratio.** The modal is a **fixed** 620 tall and the body is `Min Font Size 22` with no shrink-to-fit, so a tall header does not compress the text — it **clips** it. Budget: 620 − insets − 48 header − 24 stripe − ~60 footer ≈ **448 px for image + body**. At 4:1 the image takes 238, leaving ~210 ≈ 6-7 lines. 3:1 (`952×317`) drops that to ~4 lines — fine for a one-line HUD tip, too tight for a multi-page sequence.

**Still unverified, and it is the same gap as before:** the `Is Visible` toggle branch (`OVT_TutorialContext.c:433-439`, `LoadImageTexture(0, page.m_sImage)`) has **never run live**, because nothing has ever set an image. Brace balance and structure were checked on both files; `.layout` files are not EnforceScript so `compile-check.sh` cannot see them. **The first entry that sets `m_sImage` needs a real play-test in both presentation modes**, not a compile gate. Note `m_sImage` is per **page**, not per entry (`OVT_TutorialEntryConfig.c` → `OVT_TutorialPage`), so a sequence can carry a different header per page.

---

### 2026-08-07 — Phase 6 complete (⚠️ R3 confirmed a second time)


- **`UI/Layouts/Menu/TutorialPopup.layout`** (+ `.meta`, six platform configurations, 19 widgets, no duplicate GUIDs). Centred 1000×620 panel on the `ManageVehicleMenu.layout` skeleton; five `WLib_NavigationButton`s, every one carrying the inherited `SCR_InputButtonComponent "{5D346C3DD81D95CD}"`.
- **`OVT_TutorialContext : OVT_UIContext`**, registered as the 17th context with no `m_sOpenAction`.
- **`OVT_TutorialContext.IsModal()` is the ONE copy of decision D10** and `OVT_TutorialInfo` calls it. The HUD surface's decline path no longer calls `Release("")` for a modal entry — that would clear the pipeline's showing flag out from under the modal about to open, and invoker order between the two surfaces is not guaranteed. It only releases when there is no modal surface wired up at all.
- Seen-marking lives in **`OnClose`**, not on the Dismiss button: `MenuBack` closes through the base class's own listener and never reaches a button handler. A close while the character is dead does not mark seen.
- **6.7 closed the place/build hole** that Phase 5's gotcha 20 identified — see gotcha 24 below.
- **6.6 removed ten (not eight) debug `Print()` calls** from `OVT_UIContext.ShowLayout()`. One of them evaluated `CanShowLayout()` a second time purely to log it, so all 17 contexts now run that predicate once instead of twice.
- Gates: compile-check **0** (5935 files); **Fast 46**; **All 74**; `check-input-conflicts.py --warnings` exit **0 / 0 errors, 23 warnings, 12 pre-existing, 3 acknowledged**; `git diff --stat Language/` shows only the `.st`.
- **Ten new string keys for the user to export**, listed in `tasks.md`.

**New gotchas from this phase:**

### 24. `OVT_UIManagerComponent.IsAnyContextActive()` is now `IsAnyContextBlocking()`
Renamed because the honest question is not "is a layout up". `OVT_UIContext` gained a virtual `IsBlockingPopups()` (default `IsActive()`), overridden by `OVT_PlaceContext` and `OVT_BuildContext` to include `m_bPlacing` / `m_bBuilding` / `m_bRemovalMode`. `StartPlace`, `StartBuild` and `StartRemovalMode` all call `CloseLayout()` first and set their flag second, so the *most* interruption-sensitive part of those modes ran with `IsActive()` false. Anything else that asks "is an Overthrow UI up?" should ask the new method.

### 25. The base-game conf declares 197 actions INLINE inside `ActionContext` blocks
`ActionContext X { Actions { Action Y { … } } }`. Those actions are live in that context and appear in **no** `ActionRefs` list, so any survey (or checker) that reads the top-level `Actions` block plus `ActionRefs` is blind to them. `check-input-conflicts.py` has this hole. It is why Phase 5 recorded `KC_T`, `KC_K` and `KC_O` as unbound when they are `VONDirect`/`VONDirectToggle`, `GadgetCompass` and `GadgetWatch`.

### 26. `gamepad0:shoulder_left` and `keyboard:KC_T` belong to VON, at Priority 110
`VONContext` (`Priority 110`, `Flags 0x2`) binds `shoulder_left` to `VONChannel` and `VONDirectToggle` as **plain presses**, and `KC_T` to `VONDirect`/`VONDirectToggle`. `SCR_VONController.Update` (`:963-964`) activates it every frame the player is alive and conscious, and 110 outranks every Overthrow menu context (50). So neither input is free for a gameplay action **or** for a menu action. **Overthrow already ships `OverthrowShopPrevCategory` and `OverthrowWarehouseTakeAll` on `shoulder_left`** — a pre-existing, checker-invisible collision worth its own bug.

### 27. Any 17th menu context that lists `MenuDown` adds one conflict-script warning
`OverthrowMainMenu` is bound to `gamepad0:pad_down` and lives in `OverthrowGeneralContext`, which `OVT_UIManagerComponent.EOnFrame` activates every frame. Every menu context listing `MenuDown` therefore overlaps it: 17 of the 23 warnings and 2 of the 12 `BASE` entries all trace to that single binding. Removing `OverthrowMainMenu` from `OverthrowMainMenuContext`'s `ActionRefs` would silence the report without fixing the behaviour (the general context still fires it) — a stale waiver. The real fix is rebinding the mod's primary open-menu pad input, which is an owner-level scheme decision.

---

### 2026-08-07 — Phase 5 complete (⚠️ R3 fired)


- **`UI/Layouts/HUD/TutorialPopup.layout`** (+ `.meta`, six platform configurations). Frame copied from `ProgressInfo.layout` (backdrop `SizeLayout → Overlay → Image` stretched behind a content `VerticalLayout`, both inside one `Overlay`); content copied from the base game's own `UI/layouts/HUD/Hint/Hint.layout` — `Color 0.007 0.012 0.014 0.9` panel, `Text_Heading4` title in Overthrow orange, `RichText_Body` body, a `WLib_ProgressBar` countdown strip, and a `WLib_NavigationButtonSmall` prompt. Fixed `WidthOverride 460` so `Wrap 1` has something to wrap against. **Positioned left edge, vertically centred** — the only large HUD region not already occupied (wanted stars and money are top right, QRF/notification/progress are top centre, place/build info is bottom right, and vanilla's own hint is top right).
- **`OVT_TutorialInfo : SCR_InfoDisplay`.** `OnStartDraw` caches six widget handles (each null-guarded), inserts into `m_OnShowTutorial`, and hides. `OnStopDraw` releases the pipeline, stops both timers, removes the invoker subscription **and** the button handler, and nulls every handle. Registered on `Character_Player.et` as `{6B3A000000000021}` with `m_bAdaptiveOpacity 0` — a tip that fades out at noon is not legible.
- **Subscription is retried, not attempted once.** `OVT_ProgressInfo.SubscribeToController` gives up after one miss; the controller is registered by an async `RpcDo_NotifyOwnerAssignment`, so a HUD that starts drawing first would have no popup surface *for the whole session*. `OVT_TutorialInfo` retries every 1000 ms up to 60 times, then gives up silently. **This is a latent bug in `OVT_ProgressInfo` too** — not fixed here, out of scope, but worth a look.
- **The 20 s countdown and the blocking-UI poll share one 100 ms `CallLater`** rather than `UpdateValues`, because several Overthrow contexts open with `m_bHideHUDOnShow` and a hidden HUD is exactly when the popup most needs to notice and retire itself.
- **Both dismissal routes mark the entry seen** via `NotifyDismissed(entryId)`; the two routes that are *not* "the player saw it" (a `MODAL`/multi-page entry this surface declines, and the character going away mid-popup) use `NotifyDismissed("")`, which clears the pipeline's showing flag **without** recording anything.
- Gates: compile-check **0** (5934 files); **All 74**; `check-input-conflicts.py --warnings` **exit 0 / 0 errors, 22 warnings, 12 pre-existing, 3 acknowledged** — byte-identical to baseline because the conf was not touched; `git diff --stat Language/` shows only the `.st`.
- **New string key for the user to export: `OVT-Tutorial_MoreInMenu`** ("Overthrow Menu").

**New gotchas from this phase:**

### 19. Nothing on a gamepad is free during gameplay — R3's premise was right
All sixteen pad inputs are bound in at least one context that can be co-active with a non-modal HUD popup (on foot, in a vehicle, in a turret, or in Overthrow's place/build modes). The keyboard is not the constraint: `KC_K`, `KC_O` and `KC_T` are bound by nothing in either conf. Any future feature that wants a gameplay-context pad input should assume it is *taking* something and argue for it, not assume something is spare.

### 20. `OverthrowPlaceContext` and `OverthrowBuildContext` are live while `IsAnyContextActive()` is FALSE
`OVT_PlaceContext.StartPlace` (`:398-435`) calls `CloseLayout()` — clearing `m_bIsActive` — and *then* sets `m_bPlacing = true`; `OnFrame` (`:53-78`) then activates `OverthrowPlaceContext` every frame. `OVT_BuildContext` is the same shape (`:352-366`, `:53-79`). **So the tutorial gate's `blockingUiOpen` term does not cover placement or building**, and a `PLAYER_PLACE`/`PLAYER_BUILD` tip can appear on screen while LB/RB rotate a ghost. This is the fact that killed both otherwise-free shoulder buttons, and it is worth knowing for anything else that reasons about "is an Overthrow UI up?".

### 21. `InputSourceCombo` layers over its parts; it does not suppress them
Vanilla ships `HintDismiss` on `pad_left + y` while `pad_left` (fire mode) and `y` (interact) stay live, and `VONMenu` on `shoulder_left + x` while `x` (perform action) stays live. So a chord inherits every collision of its constituent inputs and is **not** a way to dodge a busy input map. Combos *do* render correctly in `SCR_InputButtonComponent` (`m_sComboIndicatorWidget`, `COMBO_INDICATOR_KEY " + "`), so the objection is behavioural, not visual.

### 22. `m_bCanBeDisabled 0` defeats the hidden-button-kills-its-shortcut mechanism
`SCR_InputButtonComponent.OnInput()` opens `if (m_bCanBeDisabled && (!IsVisibleInHierarchy() || !IsEnabledInHierarchy())) return;`. Vanilla's `Hint.layout` sets `m_bCanBeDisabled 0` on all three of its buttons, so `HintDismiss`/`HintToggle`/`HintContext` fire with no hint on screen. **Do not copy that attribute** into an Overthrow layout that relies on visibility to retire a shortcut — it is the one line that turns the documented safety mechanism off.

### 23. `int / int` is integer division in EnforceScript
`float f = m_iRemainingMs / AUTO_DISMISS_MS;` evaluates the division as ints first, so a 20 000 ms countdown steps 1 → 0 and never draws. Assign to a `float` local first, then divide. Caught by reading, not by the compiler — it is perfectly legal code.

---

### 2026-08-07 — Phase 4 complete


- **`OVT_TutorialSettings`** (`Scripts/Game/Global/`) shipped in exactly the D7 shape: `[BaseContainerProps()] OVT_SeenTutorialEntry { string m_sId }` plus `OVT_TutorialSettings : ModuleGameSettings { int m_iVersion; bool m_bTipsDisabled; ref array<ref OVT_SeenTutorialEntry> m_aSeen }`. **No config entry was needed** — declaring the class really is the whole registration contract, now demonstrated rather than assumed.
- **`OVT_TutorialSettingsAccessor`** — `Load(store, out tipsDisabled)`, `Save(store, tipsDisabled)`, `Reset()`. `System.IsConsoleApp()` early-out, `GetGameUserSettings()`/`GetModule()` both null-guarded, mandatory `if (!m_aSeen) m_aSeen = new …` after every `WriteToInstance`, one array of self-describing structs (no parallel arrays), read-modify-write so an unknown future member is preserved, `UserSettingsChanged()` + `SaveUserSettings()` on every write.
  - `GetGame().GetGameUserSettings()` returns **`UserSettings`**, not `BaseContainer` — `GetModule` does not exist on the base type.
- **Version handling (4.3):** `Load` delegates the decision to `OVT_TutorialSeenStore.LoadFrom(ids, version)` (one copy of the rule), then, on a mismatch, logs once and **immediately rewrites the block at the current version** so the stale version is not re-detected every session. The tips-disabled flag deliberately **survives** a version reset: it is a preference, not a record of ids.
- **🚧 4.4 — R1 IS DISCHARGED.** New Init case `OVT_TEST_Init_Tutorial_SettingsStoreRoundTrips` writes two ids + the flag through the accessor and reads them back through a fresh instance; the on-disk block was then inspected by hand and contains both ids and `m_iVersion 1` (quoted in `tasks.md`). Proven red by removing `[Attribute()]` from `m_aSeen`. **No fallback needed** — strike `array<ResourceName>` / delimited string / enum array from R1.
- **Two engine findings came out of the gate** — gotchas 16 (the flush throttle drops writes) and 17/18 (default-valued members are omitted, but writing a default back does clear a stored value). 16 is the reason the gate case takes ~10 s.
- **4.5 wiring:** `GetSeenStore()` loads from the profile on first use and adopts the stored tips flag; `NotifyDismissed` flushes only on a genuinely NEW id (an already-seen dismiss must not cost a byte-identical write); `SetTipsDisabled` touches the store first so the lazy load cannot overwrite the value just set, then flushes.
- **4.6 — the `PLAYER_SPAWNED` race is closed.** The three static `Notify*` methods now return whether a local component received the event, and `OVT_OverthrowGameMode.PushSpawnedTutorialTrigger()` retries on a `CallLater`: `TUTORIAL_SPAWN_PUSH_ATTEMPTS = 10` × `TUTORIAL_SPAWN_PUSH_RETRY_MS = 500` — a hard ceiling of 5 seconds, then a **silent** give-up (a dedicated server and a player who left during the countdown both hit that path on every spawn, so a log line there would be noise, not diagnosis).
- Gates: compile-check **0**; **All 74**; **Fast 46**; `git diff --stat Language/` empty. The CI profile was left with an empty `OVT_TutorialSettings` block (`m_bTipsDisabled 0`, no `m_aSeen`) — verified on disk after the run.

---

### 2026-08-07 — Phase 3 complete


- **3.0 contract correction shipped.** `m_OnPlayerSkill` is now `ScriptInvoker<int, string>` = `(playerId, skillKey)`. Both invoke sites updated; `OVT_CharacterSheetContext.OnSkillChanged` widened (the `Insert`/`Remove` sites reference it by name, so no call-site change was needed there); the manager's `PLAYER_SKILL` handler now populates `m_sFilter` with the key. Gotcha 9 above is **resolved** — §5's published contract is accurate again (I6).
- **`OVT_TutorialComponent`** created in `Scripts/Game/Components/Controller/`. One `[RplRpc(RplChannel.Reliable, RplRcver.Owner)] void RpcDo_ShowTutorial(string entryId)`, a server-side `Notify(entryId)` wrapper, `FireLocalEvent(ctx)`, the queue, the 1000 ms pump, the gate inputs, and `ref ScriptInvoker m_OnShowTutorial` for Phases 5/6.
- The manager's `Deliver()` seam is filled: `OVT_Global.GetPlayers().GetController(playerId)` → `FindComponent(OVT_TutorialComponent)` → `Notify()`. Both lookups null-guarded; an unregistered controller drops the send.
- Registered on `Prefabs/GameMode/OVT_OverthrowController.et` as `{6B3A000000000020}` (reserved block; `…0001/2/3/10/11` were Phase 2's). The prefab **does** carry an `RplComponent` (`{65C4B2D3DE955867}`), so the Owner RPC can route.
- `OVT_Global.GetTutorials()` added in the `GetShopTransactions()` shape. `OVT_UIManagerComponent.IsAnyContextActive()` added.
- Client-local hooks: `SCR_MapEntity.GetOnMapOpen()` (bound **once, statically**), `OVT_UIContext.ShowLayout` → `NotifyMenuOpened(ClassName())`, `OVT_OverthrowGameMode.OnPlayerSpawnedLocal` → `NotifyPlayerSpawnedLocal()`. `#OVT-IntroHint` and `m_aHintedPlayers` untouched (I7).
- Gates: compile-check **0**; **All 73**; `OVT_PlayerCommsComponent` RPC count **132 → 132**, file unmodified (Q9); `git diff --stat Language/` empty.
- Four `// PHASE 4: load/persist via OVT_TutorialSettingsAccessor` seams left in the component (store allocation, dismiss flush, tips-disabled flush, and the load point). The seen store is in-memory only until Phase 4 — a tip does not repeat within a session but does across restarts.

**New gotchas from this phase:**

### 12. `map` is a reserved type name and cannot be a local variable
`SCR_MapEntity map = SCR_MapEntity.GetMapInstance();` fails with *"Variable name 'map' is already used as type name"*. Same family as Gotcha 7's `out`/`event`. Renamed to `mapEntity`.

### 13. An RPC is never looped back to the sending machine — a listen-server host must be delivered locally
The same fact BUG-035's comment in `OVT_SkillManagerComponent` records. `Rpc(RpcDo_ShowTutorial, …)` on the host's own controller reaches nobody, so `Notify()` checks `IsOwnedByLocalPlayer()` and calls `RpcDo_ShowTutorial(entryId)` **directly** in that case. The single-player/host path would otherwise show zero tips while the dedicated path worked — a bug that only two-machine testing finds. `OVT_BaseServerProgressComponent` and `OVT_ShopTransactionComponent.RpcDo_SellResult` do **not** do this and may have the same hole.

### 14. The manager's `id → entry` registry does not exist on clients
`BuildRegistry()` runs inside the server-only `PostGameStart()`, so `GetEntry()` returns null on every client. The authored `m_aEntries` array itself *is* present client-side (it is prefab data), so `OVT_TutorialComponent` builds its own lazy lookup from `GetEntries()`. Anything client-side that wants an entry object must go through the component, not the manager.

### 15. `OVT_Global.GetUI()` dereferences the controlled entity unguarded
`IEntity player = SCR_PlayerController.GetLocalControlledEntity(); return …Cast(player.FindComponent(…));` — a null player is a script error. The tutorial component resolves the UI manager itself rather than calling it, because it runs on a dedicated server and between respawns, which are exactly the two moments the entity is null.

---

### 2026-08-07 — Phase 2 complete

- `OVT_TutorialManagerComponent` built: `s_Instance`/`GetInstance()`, `Init(owner)`, server-only `PostGameStart()`, registry validation that names every offender, ten invoker handlers, per-persistent-id session sent-set, proximity fan-out for the two global events.
- Registry validation is TERMINAL, per §5's "the manager refuses to start": a duplicate or empty id logs a named error and NO invoker is subscribed that session. Tips go dark; nothing else does.
- `Deliver(playerId, entryId)` is the Phase 3 seam — one `Print`, bounded by the sent-set, replaced by the Owner RPC in Phase 3.
- Wired: game-mode field + `Init` + `PostGameStart`, `OVT_Global.GetTutorialManager()`, `OVT_TutorialManagerComponent` on the game-mode prefab.
- `Configs/Tutorials/proofFirstBuy.conf` created as a MINIMAL placeholder (id `economy-first-buy`, one page, one `PLAYER_BUY` trigger) purely so the Init case's ">= 1 entry" assertion has something to stand on. Phase 8 fleshes it out and adds `proofWelcome.conf`. Its `#OVT-` keys are NOT in `localization_Overthrow.st` yet — that is Phase 8.4, and nothing renders them in the meantime.
- GUIDs minted from the reserved block: `{6B3A000000000001}` (proofFirstBuy.conf), `{6B3A000000000002}` (its page), `{6B3A000000000003}` (its trigger), `{6B3A000000000010}` (component on the game-mode prefab), `{6B3A000000000011}` (entry element in `m_aEntries`).
- Gates: compile-check 0; **Fast 45**; **All 73**. Both new Init cases proven red — see the record above.
- Three plan corrections recorded above (Gotchas 8, 9 and 10).

---

### 2026-08-07 — Phase 1 complete

- Created the schema (`OVT_TutorialTrigger.c`, `OVT_TutorialEntryConfig.c`) and the four pure classes (`OVT_Tutorial{Matcher,Queue,Gate,SeenStore}.c`), plus `OVT_TEST_Logic_Tutorial.c` with five cases.
- `LoadFrom` gained a `version` parameter the plan's sketch omitted — a version mismatch is undetectable without it.
- Gates: compile-check 0; **Fast 43**; **All 71**; Logic-tier grep rule clean.
- All five cases proven red — see the record above. One of the five was a genuine defect (Gotcha 5).

---

### 2026-08-07 — Phase 0 complete

- Corrected `m_OnPlayerBuy` / `m_OnPlayerSell` doc comments against their real call sites.
- `m_OnPlayerSkill` → `ScriptInvoker<int>`; both invoke sites updated; `OVT_CharacterSheetContext` gained an explicit `OnSkillChanged(int playerId)` wrapper (no arity coercion) wired into **both** the `Insert` and the `Remove`.
- `OVT_PlayerWantedComponent` gained `static ScriptInvoker<int,int,int> GetOnWantedLevelChanged()` (lazy-alloc), fired from `SetBaseWantedLevel`'s escalation branch only — never from the decay-path `SetWantedLevel`.
- Gates: compile-check exit 0; All group exit 0 / 66 tests. Both match baseline.
- Two plan corrections recorded above (Gotchas 3 and 4).

---

### 2026-08-07 — autorun start

- Read the epic context (`epic-overview.md`, `epic-requirements.md`, all four sibling `requirements.md`) and the 806-line plan.
- Scaffolded `tasks.md` (59 tasks / 9 phases) and this file; flipped `implementation.md` to In Progress.
- Routing confirmed from the plan: Phases 5/6/7 → `ui-developer-advanced`; Phase 3 → `network-specialist`; the rest → `component-developer`.
- Next: Phase 0.

---

*Update this file at the end of each work session.*
