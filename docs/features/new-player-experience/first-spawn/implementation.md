# First Spawn — Implementation Plan

**Epic:** new-player-experience (feature #4 of 5)
**Status:** 🟢 **COMPLETE** (49/49 tasks · compile-check 0, Fast 51, All 88 · play-test passed and signed off 2026-08-09 · string export done · one `.st` re-export owed for `OVT-FieldManual_Welcome_Text2`)
**Started:** 2026-08-09
**Completed:** 2026-08-09
**Last Updated:** 2026-08-09

**Requirements:** `docs/features/new-player-experience/first-spawn/requirements.md`
**Contracts consumed:** `tutorial-system/implementation.md` §"Contract for Sibling Features" (entry ids, string keys, trigger catalog, presentation/priority rules, add-an-entry procedure, **Rule 0**) · `field-manual/implementation.md` §3.3 (frozen link keys — row 1, `#OVT-FieldManual_Welcome_Title`, is reserved for this feature) · `tutorial-content/implementation.md` §3.1 (the coordination point on `home-first-open`)

---

## 1. Executive Summary

A new player's first two minutes today are: a teleport to a random house, a car, some cash, and a 20-second corner hint that repeats. This feature replaces that with a **four-page dismissable welcome modal** built on the sequence primitive `tutorial-system` shipped, and gives the **campaign-setup screen real descriptions** of the three choices it offers.

It is smaller than it looks and sharper than it looks.

**Smaller:** the welcome entry already exists. `Configs/Tutorials/proofWelcome.conf` ships `welcome-intro` — MODAL, `m_iPriority 100`, a `PLAYER_SPAWNED` trigger, two pages — and was reserved for this feature by name. Expanding it to four pages is config plus stringtable. The `.layout` and `OVT_StartGameContext` work is three widgets and three wirings against a pattern that already exists for difficulty.

**Sharper:** two things carry real risk.

1. **The houseless spawn.** `OVT_OverthrowGameMode.FinalizePlayerPreparation` has two branches (`:1011-1023`): one assigns a house, an owner record and a starting car; the other calls `SpawnPlayerAtFallbackPosition` and gives **neither**. A welcome that says "you have been given a house and a car" is a lie for the second player. **The client cannot answer the question itself** — `SpawnPlayerAtFallbackPosition:824-839` calls `SetHomePos` too, so `home != vector.Zero` is true in *both* branches, and `OVT_PlayerManagerComponent.RplSave/RplLoad:798-874` is a join-time snapshot sent before finalization ever runs. So the fact has to come from the server. This feature adds one owner RPC to carry it and **two** entries — `welcome-intro` (filter `house`) and `welcome-nohome` (filter `nohouse`) — differing on exactly one page, so the player ever sees one modal.

2. **Rule 0 binds this feature twice over.** Two proof entries have already shipped with invented mechanics, and `tutorial-content` caught a false row in its own trap table. This feature writes eight pages of player-facing prose *and* eight campaign-setup descriptions. The planning fact-check already found three claims that would have shipped false: **`realEstateCostMultiplier` is a dead field read by nothing**, **`respawnCost` is gated behind a hardcoded `money > 500`**, and the **supporting faction does almost nothing** — it sends no troops, supplies no equipment, touches no price, and is neither replicated nor persisted. All three were on the shortlist of "obvious" things to write about. §3.7 is the evidence pack.

The legacy `#OVT-IntroHint` retires here, and §3.5 records what its removal is actually **observed** to change rather than inheriting the code comment's claim — because that claim is only two-thirds true.

---

## 2. Goals

### Primary

1. **One welcome, four pages, once per machine.** MODAL, priority 100, dismissable on any page, shown on the player's own first spawn and never again — on any campaign, any server, after any restart.
2. **The houseless player gets true text.** A player who spawned at a bus stop reads a page 2 that describes what they actually have, and never sees the house-and-car version.
3. **Every player on a server gets their own welcome, and only their own** — including a player who joins a running campaign.
4. **The legacy hint is gone**, with its session-only dedup, and nothing that depended on it regresses.
5. **The campaign-setup screen explains all three choices**, in localized text, with numbers composed at runtime from the selected preset.
6. **Every sentence is true**, with a `file:line` recorded in the string's `Comment` field.

### Secondary

7. **No change to spawn mechanics.** Home assignment, starting cash, the starting car and the teleport flow are untouched. This feature reads one fact out of that flow and explains it.
8. **Localize the difficulty descriptions**, which are currently hardcoded English in five `.conf` files — an instance of the debt class the epic forbids adding to.
9. **Leave the findings behind.** The dead field, the gated respawn cost and the unreplicated supporting faction are recorded for the bug backlog (§9), not fixed here.

### Explicitly out of scope

- Changing home assignment, starting cash, difficulty values, or any spawn/respawn logic.
- Character creation, intro cinematics, start-camera changes.
- "New campaign re-shows the welcome" — seen tracking is per-machine by epic decision.
- **A second home entry.** `tutorial-content`'s `home-first-open` covers what ownership *means* mechanically and deliberately goes deeper. Page 2 says "this is yours and you respawn here"; it does not restate ownership semantics. Both may link `#OVT-FieldManual_YourHome_Title` — link keys are not exclusive.
- Framework changes to `tutorial-system` beyond the one filter field on the spawn event.
- Fixing the three findings in §9. Recording them is in scope; fixing them is not.

---

## 3. Architecture Overview

### 3.1 The two welcome entries

```
PLAYER_SPAWNED (client-local, pushed by OVT_OverthrowGameMode)
        │
        │  ctx.m_sFilter = the client's stored spawn context ("house" | "nohouse")
        ▼
OVT_TutorialMatcher.FindMatches(entries, ctx, ids)      [pure, Logic-tier pinned]
        │
        ├── welcome-intro   m_sFilter "house"    ── matches iff ctx == "house"
        └── welcome-nohome  m_sFilter "nohouse"  ── matches iff ctx == "nohouse"
                                                     exactly one, ever
```

| | `welcome-intro` | `welcome-nohome` |
|---|---|---|
| Id (immutable) | `welcome-intro` **(exists — do not change)** | `welcome-nohome` **(new)** |
| File | `Configs/Tutorials/proofWelcome.conf` **(edited, not renamed)** | `Configs/Tutorials/welcomeNohome.conf` **(new)** |
| Presentation | MODAL | MODAL |
| Priority | 100 | 100 |
| Trigger | `PLAYER_SPAWNED`, `m_sFilter "house"` | `PLAYER_SPAWNED`, `m_sFilter "nohouse"` |
| Learn more | `#OVT-FieldManual_Welcome_Title` | `#OVT-FieldManual_Welcome_Title` |
| Pages | 4 | 4 |

**The filename stays `proofWelcome.conf`.** Renaming means editing the `.meta` name path and the prefab reference for a cosmetic gain; the id, not the filename, is what anything reads. This mirrors `tutorial-content` D3 and is noted so nobody files it as sloppiness.

**Page plan** (content intent, not copy — final wording is the authoring phase's job, after the fact-check):

| Page | `welcome-intro` | `welcome-nohome` |
|---|---|---|
| 1 | What Overthrow is: a persistent sandbox on an occupied island; no mission list, no required order; the island reacts to what you do | **same string** |
| 2 | What you were given: a house that is yours and is where you respawn, a car parked outside it, and some starting cash | **different string**: no house was free, so you started in a town with your cash; property is bought, and where you buy becomes where you respawn |
| 3 | Where to look: the map, the town around you, and the Overthrow menu | **same string** |
| 4 | How tips work: they appear the first time something new happens, each is shown once, and the Tips entry in the Overthrow menu reopens the last one | **same string** |

### 3.2 String keys — shared, not duplicated

The requirement brief anticipated "roughly 8 body keys + 2 titles". **Three of the four pages are literally the same sentence in both entries**, and `m_sBody` is a plain localization key with no rule tying it to the owning entry's id. So both entries point at the same keys for pages 1, 3, 4 and the title, and only page 2 differs.

| Key | Status | Used by |
|---|---|---|
| `#OVT-Tutorial_WelcomeIntro_Title` | exists | both |
| `#OVT-Tutorial_WelcomeIntro_Body1` | **rewritten** | both |
| `#OVT-Tutorial_WelcomeIntro_Body2` | **rewritten** (becomes page 2, the house version) | `welcome-intro` only |
| `#OVT-Tutorial_WelcomeNohome_Body2` | **new** | `welcome-nohome` only |
| `#OVT-Tutorial_WelcomeIntro_Body3` | **new** | both |
| `#OVT-Tutorial_WelcomeIntro_Body4` | **new** (carries the tips text currently in `_Body2`) | both |

**3 new items, 2 rewritten** — against 5 new plus a rewrite for the duplicated shape. Two copies of a shared sentence can drift, and nothing in the pipeline can see it; the houseless variant is exactly the one nobody play-tests. **`_Body1` must be rewritten regardless**: its shipped text ends "You start with a house, a car and a little money", which is false for `welcome-nohome` and belongs on page 2 anyway.

The cost is that the key name does not match the entry id for three of `welcome-nohome`'s four pages. Mitigation is explicit and cheap: a header comment in `welcomeNohome.conf` saying so, and a `Comment` on each shared string item naming **both** consumers, so an author editing page 3 knows they are editing both entries — which is the intent.

None of `WelcomeIntro_Title/_Body1/_Body2` has a non-`en_us` translation today, so the rewrite discards no translator work.

### 3.3 The server→client spawn context

**The exact signatures.** `Rpc()` arity is a compile-check blind spot (BUG-090: a wrong argument count compiles clean and dies at the wire), so these are written out and the implementer must prove the call matches.

On `OVT_TutorialComponent` (`Scripts/Game/Components/Controller/OVT_TutorialComponent.c`):

```enforce
//! The client's stored answer to "what did my spawn actually give me?".
//! DEFAULTS TO "house" and never to "": OVT_TutorialTrigger.Matches treats "" on the TRIGGER as
//! "no filter", but a "" on the CONTEXT matches no filtered trigger at all, so a lost context
//! would suppress BOTH welcomes rather than degrading to one of them.
protected string m_sSpawnContextFilter = OVT_TutorialComponent.SPAWN_CONTEXT_HOUSE;

//! True once the server's answer has actually arrived on this machine.
protected bool m_bSpawnContextReceived;

static const string SPAWN_CONTEXT_HOUSE   = "house";
static const string SPAWN_CONTEXT_NOHOUSE = "nohouse";

//! SERVER: tell one player's client what their spawn gave them.
//! Mirrors Notify()'s local-direct-call branch, because the engine never loops an RPC back to the
//! machine that sent it - without it a listen host and single player never receive their own context.
//! The ownership test is a PLAYER-ID comparison, not Notify()'s IsOwnedByLocalPlayer(): that helper
//! dereferences GetLocalControlledEntity(), and on the load-a-save path FinalizePlayerPreparation
//! runs BEFORE the character exists (OVT_SpawnLogic.c:150-160).
void SetSpawnContext(int playerId, string filter);          // 2 params

[RplRpc(RplChannel.Reliable, RplRcver.Owner)]
void RpcDo_SetSpawnContext(string filter);                   // 1 param
// call site inside SetSpawnContext:  Rpc(RpcDo_SetSpawnContext, filter);   // Rpc() gets 2 args total

//! CHANGED SIGNATURE. Was NotifyPlayerSpawnedLocal(); the parameter lets the game mode's LAST retry
//! attempt deliver with the default context rather than dropping the welcome entirely.
//! \return true when a local tutorial component existed AND (the context has arrived OR
//!         acceptDefaultContext is true).
static bool NotifyPlayerSpawnedLocal(bool acceptDefaultContext = false);
```

`FireLocalEventOnLocalPlayer` gains the same optional parameter and sets `ctx.m_sFilter = m_sSpawnContextFilter` for `PLAYER_SPAWNED` instead of the current `""` (`:206-209`, `:223-238`).

On `OVT_OverthrowGameMode`:

```enforce
//! Persistent id -> spawn context, authored once per player per session by FinalizePlayerPreparation.
//! Server-side. Exists for three reasons: a reconnecting player is re-sent their context on the
//! "already finalized, skipping" early return (:989); nothing else on the server can answer the
//! question afterwards; and it gives the Campaign tier something assertable.
protected ref map<string, string> m_mSpawnContext;

//! Records the context and pushes it to that player's client.
protected void SetPlayerSpawnContext(int playerId, string persistentId, string filter);

//! \return "house", "nohouse", or "" when unknown. Public for the Campaign-tier case.
string GetPlayerSpawnContext(string persistentId);
```

**Where it is called.** Both branches of `FinalizePlayerPreparation` (`:1011-1023`):

- `if (!house)` → `SpawnPlayerAtFallbackPosition(playerId)` → `SetPlayerSpawnContext(playerId, persistentId, "nohouse")`
- `else` → `SetOwner` / `SetHome` / `SpawnStartingCar` → `SetPlayerSpawnContext(playerId, persistentId, "house")`
- The outer `if (home[0] == 0)` is false for a **returning** player, so neither branch runs; the client's default of `"house"` stands. That is the right answer for anyone who has a home, which is every returning player by construction.
- The `already finalized, skipping` early return at `:989` re-sends the cached value.

**Ordering, verified end to end.**

| Path | Sequence | Race? |
|---|---|---|
| **New campaign, host / SP** | `DoSpawn_S` (`OVT_SpawnLogic.c:121-161`) possesses before Start Game → `DoStartGame` sets `m_bGameStarted` (`:255`), bumps `m_bCampaignRunningRpl` (`:260`), runs `PrepareConnectedPlayers` (`:264`) → `FinalizePlayerPreparation` → context set **synchronously via the direct-call branch** → `TryPushSpawnedTutorialTrigger()` (`:371`) | **None.** Same machine, same call stack. |
| **JIP into a running dedicated server** | `DoSpawn_S` → `SetupPlayer` spawns the controller and calls `NotifyOwnerAssignment` (`OVT_PlayerManagerComponent.c:658-671`) → `HasGameStarted()` true → `FinalizePlayerPreparation` → context RPC → `SpawnDeferredPlayer` → possession lands on the client later | **None in practice.** Both are Reliable RPCs on the same entity's channel, sent in that order; the client cannot register its controller (and so cannot push the trigger) before the first, and the context follows it. |
| **Remote client present when Start Game is pressed** (listen host, or a dedicated server whose players joined pre-start) | `m_bCampaignRunningRpl` is bumped at `:260` **before** `PrepareConnectedPlayers` at `:264`. On the client, `OnCampaignRunningReplicated` (`:1410-1413`) can therefore fire before the context RPC arrives — RplProp-vs-RPC ordering is not guaranteed | **Yes. This is the one real race.** |

**The guard for that one race**, and it reuses machinery that already exists. `PushSpawnedTutorialTrigger` (`:1461-1479`) already retries 10 × 500 ms because the controller assignment is async, and treats a `false` return as "not delivered, try again". `NotifyPlayerSpawnedLocal` now also returns `false` while `m_bSpawnContextReceived` is false — so the push simply waits for the fact. On the **final** attempt the game mode calls `NotifyPlayerSpawnedLocal(true)`, which delivers with the `"house"` default rather than dropping the welcome. Worst case after a 5-second silence: a houseless player reads the house page, which is today's behaviour exactly.

**Backward compatibility is by construction.** `OVT_TutorialTrigger.Matches:116` treats `m_sFilter == ""` on the *trigger* as "no filter", so populating the context filter cannot change which entries any other entry matches. Verify at implementation time that no other entry uses `PLAYER_SPAWNED` (`tutorial-content` states it deliberately does not; re-check rather than inherit).

### 3.4 Start-menu descriptions

`UI/Layouts/Menu/StartGameMenu.layout` today has one description widget, `DifficultyDescription` (`:109-118`), a `TextWidget` in the root `VerticalLayout0` with `Text "Difficulty Description"` hardcoded, font 18, right-aligned. Two more go in, immediately after their spinners:

```
VerticalLayout0
├── LogoFrame
├── OccupyingFactionSpinner        (:73-84)
├── OccupyingFactionDescription    NEW TextWidget  ── copy DifficultyDescription's slot/padding/font
├── SupportingFactionSpinner       (:85-96)
├── SupportingFactionDescription   NEW TextWidget
├── DifficultySpinner              (:97-108)
├── DifficultyDescription          (:109-118) — placeholder Text replaced with a #OVT- key
├── DifficultyNumbers              NEW TextWidget  ── the runtime-composed line (see below)
├── Fill / Footer / StartButton
```

Wiring in `OVT_StartGameContext.c`, alongside the existing `OnSpinDifficulty` pattern (`:198-208`):

| Hook | Today | Add |
|---|---|---|
| `OnShow` (`:57-140`) | sets the difficulty description twice, once per `RplSession.Mode()` branch (`:129`, `:134`) | set all four descriptions once, from the selected faction/preset, through one shared `RefreshDescriptions()` so the two branches cannot drift |
| `OnSpinOccupyingFaction` (`:142-168`) | swaps a conflicting supporting faction, sets the config | refresh **both** faction descriptions — the conflict swap at `:150-164` changes the *other* spinner too, so refreshing only the one that moved leaves a stale line on screen |
| `OnSpinSupportingFaction` (`:170-196`) | mirror image | same |
| `OnSpinDifficulty` (`:198-208`) | sets one description | set description **and** numbers |

**Localization mechanism — resolved, not assumed.** `TextWidget.SetText` **does** resolve `#`-prefixed stringtable keys: the engine binding says so in its own doc comment (`ArmaReforger/scripts/Core/generated/UI/TextWidget.c:115-116` — *"Sets text for the widget. String-table entries are translated."*), the base game passes runtime-assembled keys into it (`SCR_PrivacyPolicy.c:52`), and Overthrow already ships ~29 `SetText("#OVT-...")` call sites — including `OVT_TutorialContext.c:403,407`, which is play-tested. So the five `description` fields in `Configs/Difficulty/Difficulty_*.conf` can simply hold `#OVT-` keys and `text.SetText(preset.description)` keeps working unchanged.

`description` is read **only** in `OVT_StartGameContext` (`:129`, `:134`, `:207`) — verified repo-wide — so this change is contained to one file's rendering. `Difficulty_TestWorld.conf` is not in `m_aDifficultyPresets` (`Prefabs/GameMode/OVT_OverthrowGameMode.et:65-79` lists exactly Easy/Normal/Hard/Extreme/Insane, in that order) and needs no key; a `description` without a `#` renders as its literal text, so leaving it alone is safe.

**The numbers line uses `SetTextFormat`, never concatenation.** `TextWidget.SetTextFormat(key, p1..p9)` is the substitution API (`TextWidget.c:110-111`), already used in this codebase at `OVT_TutorialContext.c:459` with `#OVT-Tutorial_PageIndicator` = `"%1 / %2"`. One key, `#OVT-Difficulty_Numbers`, with `%1`-style placeholders, filled from the selected `OVT_DifficultySettings` instance. No English is ever built in script.

**Which numbers — and which were disqualified.** See §3.7 for the evidence.

| Field | Verdict | Evidence |
|---|---|---|
| `startingCash` | **USE.** The first number a player ever meets. 500 / 100 / 100 / 0 / 0 | read at `OVT_OverthrowGameMode.c:1048`, applied through `AddPlayerMoney` on the new-player branch only |
| `fastTravelCost` | **USE.** Fast travel home is the most-used convenience of the first hour, and the old hint advertised it. 0 / 5 / 10 / 25 / 25 | read at `OVT_MapContext.c:384,395` |
| `realEstateCostMultiplier` | **DO NOT USE — dead field.** Its only references are the declaration and the replication pair; `GetBuyPrice`/`GetRentPrice` (`OVT_RealEstateManagerComponent.c:712,730`) never touch it. The authored 0.4→2.0 spread has zero effect | verified repo-wide |
| `respawnCost` | **DO NOT USE without the caveat.** `ChargeRespawn` only charges when `money > 500` (`OVT_EconomyManagerComponent.c:1874`), a hardcoded gate unrelated to the setting. "Respawn costs $30" is false for exactly the players it would matter to | `OVT_EconomyManagerComponent.c:1874` |
| `placeableCostMultiplier` / `buildableCostMultiplier` | eligible, second tier | read at `OVT_OverthrowConfigComponent.c:252,257` |
| `patrolGroupsMin`/`Max` | eligible, second tier — the most *felt* difference (1-2 groups on Easy vs 8-12 on Insane) | read at `OVT_OccupyingFactionManager.c:508`, and halved at `OVT_InfantrySpawningDeploymentModule.c:224`, so any claim must not name a single location |
| `startingItems` | **worth a sentence, not a number.** Easy/Normal/Hard give an M9 + magazine + radio; **Extreme and Insane give the radio only** | read at `OVT_SpawnLogic.c:1177`; per-preset values in the five configs |

Ship with **`startingCash` and `fastTravelCost`** as the two numbers. A third is optional and must earn its place in the fact-check; the pistol difference is a candidate for the *description* prose rather than the numbers line.

All of this is read client-side from `config.m_aDifficultyPresets`, which replicates with the game-mode prefab, so no networking is involved.

### 3.5 Legacy hint retirement — what removal actually changes

Three lines and a field: the block at `OVT_OverthrowGameMode.c:1382-1391`, the `m_aHintedPlayers` declaration (`:57`) and its allocation (`:1167`). The code comment there claims the hint "has been dead since the 1.6 spawn rework". **That claim is two-thirds true and the plan does not inherit it.** Traced:

`OnPlayerSpawnedLocal` has exactly one caller — `OVT_UIManagerComponent.AfterControlledByPlayer:138` — which fires on **every** possession on the machine that controls the character, not only the first. The hint is gated on `m_bGameStarted`, which is authority-only (`:92-94`, written only at `:255`).

| Situation | Hint today | After removal |
|---|---|---|
| Dedicated-server client, any spawn | never (`m_bGameStarted` is false on every client, always) | no change |
| SP / listen host, **first** spawn of a new campaign | never — possession precedes Start Game (`OVT_SpawnLogic.c:142-158`, and `OVT_TEST_Campaign_TutorialSpawnTrigger.c:4-10` documents the same ordering) | no change |
| SP / listen host, **respawn after death** | **fires**, once per persistent id per session (`m_aHintedPlayers` is allocated fresh in Init) | gone — a veteran stops being welcomed after dying |
| SP / listen host, campaign resumed with **Continue** | **fires** — `DoStartGame` has already run, so possession finds `m_bGameStarted` true | gone |

So the observable change is: **the corner hint stops appearing after a death and after a Continue in single player / on a listen host.** That is not a regression, it is the requirements' "repeats every session" complaint, and the welcome modal covers the same ground once and correctly.

Retiring it also removes three claims that are now or soon wrong: *"a handgun is in your pants"* (false on Extreme and Insane, which ship the radio only), *"a car parked in your garage"* (the code parks it in a parking spot or at a kerb — `OVT_VehicleManagerComponent.c:179-191`), and *"a set of tutorials in the Jobs section"* (`starter-jobs-retirement` removes them).

**The two orphaned string items are KEPT, not deleted.** `OVT-IntroHint` (`.st:3339`) and `OVT-Overthrow` (`.st:6359`, used only as that hint's title) become unreferenced by any code. Deleting them would discard six languages of translation for no runtime gain, and `.st` deletions churn the exports the user regenerates by hand. Instead: leave both items in place and **rewrite `OVT-IntroHint`'s `Comment`** to record that it is retired, by which feature, on what date, that the welcome sequence replaced it, and that its text contains three claims that were or became false — so nobody revives it. Do not touch the `.<lang>.conf` exports, which will keep carrying both harmlessly.

### 3.6 File inventory and GUID block

```
Configs/Tutorials/
├── proofWelcome.conf                  ← EDITED: 2 pages → 4, filter "house", link key added.
│                                        Id, filename and resource GUID unchanged (D2)
└── welcomeNohome.conf  (+ .meta)        NEW

Configs/Difficulty/
├── Difficulty_Easy.conf                ← EDITED: description → #OVT- key
├── Difficulty_Normal.conf              ← same
├── Difficulty_Hard.conf                ← same
├── Difficulty_Extreme.conf             ← same
└── Difficulty_Insane.conf              ← same
    (Difficulty_TestWorld.conf NOT TOUCHED - it is not in m_aDifficultyPresets)

UI/Layouts/Menu/
└── StartGameMenu.layout                ← EDITED: 3 new TextWidgets; DifficultyDescription's
                                          hardcoded placeholder Text replaced with a #OVT- key

Language/
└── localization_Overthrow.st           ← the ONLY file under Language/ this feature edits

Prefabs/GameMode/
└── OVT_OverthrowGameMode.et            ← 1 element appended to m_aTutorialEntries (currently :212-219+)

Scripts/Game/
├── Components/Controller/OVT_TutorialComponent.c   ← spawn-context field, RPC, changed Notify signature
├── GameMode/OVT_OverthrowGameMode.c                ← context map + setter/getter, both Finalize branches,
│                                                     final-attempt delivery, LEGACY HINT REMOVED
├── UI/Context/OVT_StartGameContext.c               ← RefreshDescriptions + 3 wirings
└── Tests/TestSuites/{Logic,Init,Campaign}/         ← 3 new/extended cases (§7)

docs/features/new-player-experience/first-spawn/{implementation,tasks,context}.md

NOT TOUCHED: Configs/FieldManual/ · Configs/System/chimeraInputCommon.conf · UI/Layouts/HUD/ ·
             UI/Layouts/Menu/TutorialPopup.layout · Language/localization_Overthrow.<lang>.conf ·
             any other Configs/Tutorials/*.conf (tutorial-content's ten)
```

**GUID block: `6B3D0000…` is reserved for this feature.** Verified free 2026-08-09: `grep -rEoh "\{6B3D[0-9A-F]{12}\}"` returns **0** repo-wide (for comparison `6B3A` = 151 tutorial-system, `6B3B` = 237 field-manual, `6B3C` = 95 tutorial-content). Working tree was clean at planning time. **Re-check both before allocating** — concurrent sessions share this tree.

| Purpose | Allocation |
|---|---|
| `welcomeNohome.conf` resource (`.meta` Name) | `{6B3D000000000010}` |
| its 4 page objects | `…0011` … `…0014` |
| its trigger object | `…0015` |
| its element on the game-mode prefab | `…0016` |
| `welcome-intro`'s two **new** page objects (Body3, Body4) | `{6B3D000000000021}`, `{6B3D000000000022}` |
| new stringtable items | `{6B3D0000000001xx}`, sequential in authoring order |
| new `.layout` widgets and their slots | `{6B3D0000000002xx}` |

### 3.7 Rule 0 evidence pack

Pre-loaded from the planning fact-check so the authoring phases start from evidence instead of deriving it — **and re-verified at each phase, because a documented trap is not evidence** (`tutorial-content` shipped a false row in exactly such a table).

#### Verified true — safe to compress

| Claim | Source |
|---|---|
| A new character is given a random starting house, that house is set as their home, and a car is spawned at it | `OVT_OverthrowGameMode.c:1011-1023` |
| The car is parked in the house's parking spot, or failing that at a nearby kerb — **not** "in your garage" | `OVT_VehicleManagerComponent.c:169-191` |
| Home is the respawn position | `OVT_SpawnLogic.c:787-802`, and `#OVT-FieldManual_Welcome_Text2` which cites it |
| Starting cash comes from the difficulty preset and is granted once, on the new-player branch only | `OVT_OverthrowGameMode.c:1043-1053` |
| When no starting house is free the player spawns at a bus stop (or the starting town centre) with a home position but **no owned building and no car** | `OVT_OverthrowGameMode.c:1014-1018`, `:824-839` |
| The Overthrow menu has 12 entries and holds **neither** a money screen **nor** the Field Manual | already fact-checked and recorded in `#OVT-Tutorial_WelcomeIntro_Body2`'s `Comment` |
| Tips are shown once per machine, and the menu's **Tips** entry re-opens the most recent one | `OVT_MainMenuContext.c:284-301`, `OVT_TutorialComponent.NotifyDismissed:380-396` |
| `<action name="OverthrowMainMenu"/>` renders the bound key glyph inside a tutorial body | framework contract; body is a `RichTextWidget` (`OVT_TutorialContext.c:415`) |
| The occupying faction's uniform is the disguise that works | `IsDisguisedAsOccupying()` has 5 consumers incl. `SCR_CharacterDamageManagerComponent` |

#### Verified false or unusable — cut on sight

| Tempting claim | Reality |
|---|---|
| "Real estate costs scale with difficulty." | **`realEstateCostMultiplier` is read by nothing.** Only three references exist: the declaration (`OVT_DifficultySettings.c:65`) and the `RplSave`/`RplLoad` pair (`OVT_OverthrowConfigComponent.c:564,619`). `GetBuyPrice` (`OVT_RealEstateManagerComponent.c:712`) and `GetRentPrice` (`:730`) ignore it |
| "Dying costs you $N." | `ChargeRespawn` charges **only if `money > 500`** (`OVT_EconomyManagerComponent.c:1874`) — a hardcoded gate unrelated to the setting |
| "The supporting faction sends troops / supplies weapons / affects prices." | **All three false.** No shipped deployment config permits `SUPPORTING_FACTION` (`Deployment_TownPatrol.conf:26` is `OCCUPYING_FACTION`; the other two default to `1`, and `CanFactionUse` is a bitmask test at `OVT_DeploymentConfig.c:97-100`). The equipment hook `m_bIncludeSupportingFactionItems` defaults **true** (`OVT_EconomyManagerComponent.c:37-39`) and no shipped config sets it to 0, so the stock filter never removes anything. No economy value reads it |
| "You can disguise as the supporting faction." | The opposite. Being perceived as the supporting faction is treated exactly like being perceived as the resistance: not a disguise (`OVT_PlayerWantedComponent.c:448-452`) and **instant wanted level 2 on being seen** (`:709-715`). `IsDisguisedAsSupporting()` (`:130-140`) has zero callers |
| "You fight alongside the supporting faction." | The player character spawns affiliated to **CIV** (`Prefabs/Characters/Factions/INDFOR/FIA/Character_Player.et:177-179`); recruits are set to the player faction, FIA (`OVT_RecruitManagerComponent.c:1720`). Neither is the supporting faction, and FIA is excluded from both spinners by `IsPlayable()` (`OVT_StartGameContext.c:98`) |
| "You start with a pistol." | Only on Easy/Normal/Hard. Extreme and Insane ship the radio alone (`startingItems`, read at `OVT_SpawnLogic.c:1177`) — so the **shared** welcome pages must not mention it |
| "The occupying faction comes looking for you." / "…in that area." | The two known-wrong Field Manual strings the epic carries. No search or dispatch behaviour is keyed to wanted level, and `m_iThreat` is a single **global** counter. Out of scope to fix; must not be repeated here |
| Money can be stolen or deposited; the Overthrow menu holds money or the Field Manual | The two lies that already shipped once. `OVT_PlayerData.money` is a persisted `int` on the player record |

#### What the supporting-faction description may truthfully say

The narrow, verified frame — and nothing richer:

- It is a **foreign power sympathetic to the resistance**, not your own faction and not something you fight alongside.
- Its **one mechanical effect** is on perception: the occupying army treats its uniform as hostile, exactly as it treats the resistance's. Wearing it and being seen means an immediate wanted level.
- **Choosing it also decides the occupier**: the two cannot be the same, so changing one spinner moves the other (`OVT_StartGameContext.c:147-165`, `:190-193`), and the shipped faction list is US and USSR only.

Do **not** write that it persists or that multiplayer clients see the same value — it is neither serialised nor replicated (§9, F3).

---

## 4. Implementation Phases

Every phase ends with `tools/compile-check.sh` exit 0. Every phase that touches a `.c` file also ends with the **All** tier green (`tools/run-tests.sh "{6A6E2A002F53A581}"`); config/string-only phases end with **Fast** (`"{6A6E29FF47ECB840}"`). **Measure the baseline at the start of Phase 0 and record it** — the counts in `CLAUDE.md`, `MEMORY.md` and the sibling plans disagree with each other and with the tree (case declarations counted 2026-08-09: Logic 30, Init 20, Campaign 12).

### Phase 0 — Evidence pack and copy decisions
*Agent: **`help-docs-sync`** (standard).* · *Estimate: half a session*

No code. This exists because Rule 0 binds this feature twice — eight pages of welcome prose and eight setup descriptions — and because §3.7 was assembled at planning time and must be re-verified, not inherited.

| # | Task |
|---|---|
| 0.1 | Re-check `git status` clean-ish, re-run the `6B3D` GUID grep, and record the measured Fast/All case counts as this feature's baseline |
| 0.2 | Re-verify every row of §3.7 against the tree. Any row that no longer holds is struck **in this document**, in place, with the date and the finding — the way `tutorial-content` struck its two false rows |
| 0.3 | Confirm no tutorial entry other than the two welcomes uses `PLAYER_SPAWNED`: `grep -l PLAYER_SPAWNED Configs/Tutorials/*.conf` |
| 0.4 | Read `#OVT-FieldManual_Welcome_Text`, `_Text2`, `_Head`, `_Head2` in the `.st`. These are the already-verified paragraphs pages 1 and 2 compress (the `tutorial-content` D12 pattern). Record which sentence of each page inherits which manual string, and that string's own cited source |
| 0.5 | Settle the **numbers line**: confirm `startingCash` and `fastTravelCost` are still read where §3.4 says, decide whether a third number earns its place, and write the chosen `#OVT-Difficulty_Numbers` format string with its `%1..%n` mapping |
| 0.6 | Draft all eight welcome sentences and all eight setup descriptions as a table of *sentence → `file:line`*. **A sentence with no citation is cut, not softened.** This table becomes the `Comment` fields in Phase 2 and Phase 4 |

**Acceptance:** §3.7 re-verified with any correction struck in place and dated; no second `PLAYER_SPAWNED` entry; the sentence→evidence table exists and every row has a citation; the numbers line's format and parameter mapping are decided; baseline counts recorded.

### Phase 1 — The spawn-context transport
*Agent: **`network-specialist-advanced`** (max effort).* · *Estimate: 1 session*

**Advanced, and this is the phase that justifies it.** It adds an owner RPC to a per-player delivery path whose MP correctness has never been observed passing (epic F7), changes the return-value contract of a method the game mode's retry loop depends on, and edits `FinalizePlayerPreparation` — the function that hands out homes, cars and cash to every player on the server. `Rpc()` arity is invisible to compile-check.

| # | Task |
|---|---|
| 1.1 | `OVT_TutorialComponent`: add `SPAWN_CONTEXT_HOUSE` / `SPAWN_CONTEXT_NOHOUSE` constants, `m_sSpawnContextFilter` **initialised to `"house"`**, and `m_bSpawnContextReceived`. Document why the default is `"house"` and not `""` (§3.3) — a future reader will otherwise "tidy" it |
| 1.2 | Add `SetSpawnContext(int playerId, string filter)` + `[RplRpc(RplChannel.Reliable, RplRcver.Owner)] RpcDo_SetSpawnContext(string filter)`. Mirror `Notify():124-139`'s local-direct-call branch, but test ownership by **player-id comparison** against `SCR_PlayerController.GetLocalPlayerId()` (guarded by `>= FIRST_VALID_PLAYER_ID`), **not** `IsOwnedByLocalPlayer()` — that helper dereferences the controlled entity, which does not exist yet on the load-a-save path. Reject an empty filter |
| 1.3 | Change `NotifyPlayerSpawnedLocal()` → `NotifyPlayerSpawnedLocal(bool acceptDefaultContext = false)` and thread the parameter through `FireLocalEventOnLocalPlayer`. For `PLAYER_SPAWNED`, set `ctx.m_sFilter = m_sSpawnContextFilter`; return `false` when the component exists but `!m_bSpawnContextReceived && !acceptDefaultContext`. Update the doc comment at `:199-209` — its "was there anyone to tell?" contract has changed |
| 1.4 | `OVT_OverthrowGameMode`: add `m_mSpawnContext` (allocated beside `m_aInitializedPlayers` at `:1167`), `SetPlayerSpawnContext(playerId, persistentId, filter)` and `GetPlayerSpawnContext(persistentId)`. The setter resolves the player's controller through `OVT_Global.GetPlayers().GetController(playerId)` and its `OVT_TutorialComponent` — the same three-step resolve `OVT_TutorialManagerComponent.Deliver:509-525` uses. Null at any step is a silent drop, never an error |
| 1.5 | Call it from **both** branches of `FinalizePlayerPreparation:1011-1023`, and re-send the cached value on the `already finalized, skipping` early return at `:989` |
| 1.6 | `PushSpawnedTutorialTrigger:1461-1479`: on the **final** attempt (`m_iTutorialSpawnPushAttempts >= TUTORIAL_SPAWN_PUSH_ATTEMPTS`) call `NotifyPlayerSpawnedLocal(true)` once before giving up, so a lost context degrades to today's behaviour rather than to no welcome at all |
| 1.7 | **Prove the `Rpc()` arity by inspection and record it**: `RpcDo_SetSpawnContext` takes 1 parameter, so the call is `Rpc(RpcDo_SetSpawnContext, filter)` — 2 arguments. Write the count in the code comment (BUG-090) |
| 1.8 | Grep-verify no other caller of `NotifyPlayerSpawnedLocal` exists outside the game mode |
| 1.9 | `tools/compile-check.sh`; All tier green |

**Acceptance:** compile-check 0; All tier green at the recorded baseline; `grep -rn "NotifyPlayerSpawnedLocal" Scripts/` shows one definition and one call site, consistent in arity; `grep -rn "SetPlayerSpawnContext" Scripts/` shows exactly three call sites (two branches plus the early return); the existing `OVT_TEST_Campaign_Tutorial_SpawnTriggerSurvivesCampaignStart` still passes, proving the retry contract change did not break delivery; starting a campaign in the Workbench still assigns a home, a car and cash exactly as before.

### Phase 2 — The two welcome entries
*Agent: **`help-docs-sync`** (standard).* · *Estimate: 1 session*

Config and strings only. No `.c`.

| # | Task |
|---|---|
| 2.1 | Read Phase 0's sentence→evidence table before writing a word. Nothing new is invented here |
| 2.2 | `Configs/Tutorials/proofWelcome.conf`: 2 pages → 4 (new page objects get `{6B3D000000000021}`/`{6B3D000000000022}`), add `m_sFilter "house"` to the existing trigger, add `m_sFieldManualTitleKey "#OVT-FieldManual_Welcome_Title"`. **Do not change `m_sId`, the filename, or the file's resource GUID.** Keep every member written out explicitly — it is a template as much as data |
| 2.3 | Create `Configs/Tutorials/welcomeNohome.conf` + `.meta` with `m_sId "welcome-nohome"`, MODAL, priority 100, the same link key, `m_sFilter "nohouse"`, and pages `[WelcomeIntro_Body1, WelcomeNohome_Body2, WelcomeIntro_Body3, WelcomeIntro_Body4]`. **Header comment must state that three of its four page keys are shared with `welcome-intro` deliberately** (§3.2) |
| 2.4 | `Language/localization_Overthrow.st` **only**: 3 new items (`WelcomeIntro_Body3`, `WelcomeIntro_Body4`, `WelcomeNohome_Body2`), 2 rewritten (`WelcomeIntro_Body1` — the "you start with a house, a car and a little money" sentence **must** come out, it is false for the houseless entry; `WelcomeIntro_Body2` — becomes the house page). Every item: fresh `{6B3D0000000001xx}` GUID and a `Comment` carrying (a) where it appears and which entries consume it, (b) the tone rule, (c) the Rule 0 evidence. **Move `_Body2`'s existing menu-fact-check note to `_Body4`** with the text it belongs to |
| 2.5 | Append one element to `m_aTutorialEntries` on `Prefabs/GameMode/OVT_OverthrowGameMode.et` for `welcomeNohome.conf`. Append, never reorder |
| 2.6 | Tone and content constraints: no imperative, no goal, no objective, no "now go and…"; **no second home entry** — page 2 says "this is yours and you respawn here" and stops, leaving ownership mechanics to `home-first-open`; no mention of the starting pistol (difficulty-dependent); page 2 of `welcome-nohome` must read correctly for someone who has cash and a home position but owns nothing |
| 2.7 | Hygiene: duplicate-GUID grep over the new `6B3D…` allocations; `git diff --stat Language/` shows **only** `localization_Overthrow.st`; no em-dashes; balanced braces |
| 2.8 | `tools/compile-check.sh`; Fast tier green. The Init entry guard should now report **12** structurally valid entries (10 content + 2 welcomes) |
| 2.9 | Report the complete list of new/changed string ids for the user's Workbench export, and state that neither welcome renders its text until that happens |

**Acceptance:** two entries, two distinct ids, both MODAL/priority 100/4 pages, filters `house` and `nohouse`, both linking a key that exists in the frozen table; every sentence has evidence in a `Comment`; no §3.7 trap present; Init guard reports 12; export list handed over.

> ### ⛔ Workbench string export — user-owned, and it blocks here
> New `.st` items are invisible in-game until the user re-exports the string table in Workbench. Until then both welcomes draw raw `#OVT-` keys, which looks exactly like a bug. **No play-test of Phase 2 or Phase 4 can start before this step.** The same applies to the difficulty description keys added in Phase 4, so batch the request if the two phases land together.

### Phase 3 — Retire the legacy hint
*Agent: **`component-developer`** (standard).* · *Estimate: half a session*

Separate from Phase 1 so the diff is unambiguous, and **after** Phase 2 so there is never a build with neither the hint nor the welcome.

| # | Task |
|---|---|
| 3.1 | **Verify §3.5's table before deleting anything.** Specifically confirm by reading (not assuming) that `OnPlayerSpawnedLocal` fires on every possession, that `m_bGameStarted` is false on clients, and that the SP respawn / Continue paths reach it with `m_bGameStarted` true. Record what removal is observed to change in `context.md` |
| 3.2 | Delete `OVT_OverthrowGameMode.c:1382-1391` (the hint block), the `m_aHintedPlayers` field (`:57`) and its allocation (`:1167`). Leave the `PLAYER_SPAWNED` push that follows it completely alone |
| 3.3 | Update the comment at `:96-107`, which cites "the legacy `#OVT-IntroHint` in `OnPlayerSpawnedLocal`, which the first-spawn feature owns" as a reason `m_bCampaignRunningRpl` exists as a separate flag. That reason is now gone; the other two remain. Do **not** change the flag |
| 3.4 | `Language/localization_Overthrow.st`: **keep** both `OVT-IntroHint` and `OVT-Overthrow`. Rewrite `OVT-IntroHint`'s `Comment` to record retirement (date, feature, replacement) and the three false claims in its text, so it is never revived. Do not delete either item and do not touch any `.<lang>.conf` |
| 3.5 | `grep -rn "IntroHint\|m_aHintedPlayers" Scripts/ Configs/ UI/` returns only the `OVT_TutorialInfo.c:26` doc comment (which cites the hint's 20 s duration as design precedent and is still accurate) |
| 3.6 | `tools/compile-check.sh`; All tier green |

**Acceptance:** compile-check 0; All green; no `m_aHintedPlayers` anywhere in `Scripts/`; both string items still present with the annotated comment; `git diff --stat Language/` shows only the `.st`; the observed-change table recorded in `context.md`.

### Phase 4 — Start-menu descriptions
*Agent: **`ui-developer-advanced`** (max effort).* · *Estimate: 1 session*

**Advanced because it edits a `.layout`.** Layout GUID discipline, slot cloning and text fit at 1080p are the failure modes here, and the campaign-setup screen is the first thing every player sees — a description that wraps into the Start button or a spinner label that no longer lines up reads as broken. It also touches five configs and changes the meaning of a config field.

| # | Task |
|---|---|
| 4.1 | `UI/Layouts/Menu/StartGameMenu.layout`: add `OccupyingFactionDescription`, `SupportingFactionDescription` and `DifficultyNumbers` `TextWidget`s, cloning `DifficultyDescription`'s slot shape, padding, font size and alignment (`:109-118`). Fresh `{6B3D0000000002xx}` GUIDs for every widget **and** every slot. Replace `DifficultyDescription`'s hardcoded `Text "Difficulty Description"` with a `#OVT-` key (see 4.6 on the export ordering) |
| 4.2 | `OVT_StartGameContext`: add one `protected void RefreshDescriptions()` that sets all four texts from the current config selections, and call it from the end of `OnShow`, `OnSpinOccupyingFaction`, `OnSpinSupportingFaction` and `OnSpinDifficulty`. This collapses the duplicated `text.SetText(preset.description)` at `:129` and `:134` into one place so the two `RplSession.Mode()` branches cannot drift |
| 4.3 | Faction descriptions are keyed off the **faction key** (`US`, `USSR`, …), not the spinner index — the list is built by filtering `IsPlayable()` and `"CIV"` (`:94-108`) and a mod can change it. Resolve a per-key `#OVT-` string; fall back to an empty description for an unknown key rather than showing a wrong one. Two keys per faction (occupying role, supporting role) — the same faction means different things in the two slots |
| 4.4 | The numbers line uses `TextWidget.SetTextFormat("#OVT-Difficulty_Numbers", …)` with the parameters Phase 0 decided, read from the selected `OVT_DifficultySettings`. **Never** string concatenation. `SetTextFormat` precedent: `OVT_TutorialContext.c:459` |
| 4.5 | `Configs/Difficulty/Difficulty_{Easy,Normal,Hard,Extreme,Insane}.conf`: replace the hardcoded English `description` with `#OVT-Difficulty_{Easy,…}_Desc`. **Do not touch `Difficulty_TestWorld.conf`** — it is not in `m_aDifficultyPresets` and a key-less description renders literally, which is correct for a dev preset |
| 4.6 | `Language/localization_Overthrow.st`: add the five difficulty descriptions, the numbers format string, and the faction descriptions. Same `Comment` discipline as Phase 2, including the Rule 0 evidence. ⚠️ **A layout referencing a key that has not been exported yet must use literal text until the user re-exports** — so either land the layout's `#OVT-` key together with the export request, or leave the placeholder and change it in the same session as the export |
| 4.7 | Content constraints from §3.7: the supporting-faction description may claim only the perception effect and the occupier coupling; the difficulty description may not quote `realEstateCostMultiplier` (dead) or `respawnCost` (gated); no sentence may assert that the supporting faction persists or replicates |
| 4.8 | Gamepad/console check: the start menu's only action is `OverthrowStartGame` on `StartButton` and the spinners are `SCR_SpinBoxComponent`s — confirm the three new text widgets add no focusable element and do not change tab order. Check text fit at 1080p in the 900 px panel with the **longest** description |
| 4.9 | `tools/compile-check.sh`; All tier green; report the new string ids for export |

**Acceptance:** compile-check 0; All green; all four texts change correctly when either faction spinner or the difficulty spinner moves, including the auto-swap case where changing one faction moves the other; no hardcoded English remains in the layout, the context or the five difficulty configs; `Difficulty_TestWorld.conf` untouched; no new focusable widget; export list handed over.

### Phase 5 — Tests, verification and the play-test checklist
*Agent: **`component-developer`** (standard).* · *Estimate: half a session*

| # | Task |
|---|---|
| 5.1 | Logic-tier case: the matcher's spawn-filter behaviour (§7 T1). **Prove it red once** and record the exact failure text, method and date in `context.md`'s proven-red table |
| 5.2 | Init-tier branch: spawn-filter validity and welcome coverage (§7 T2). **Prove it red once**, record it |
| 5.3 | Campaign-tier case: the server authored a spawn context for the local player (§7 T3). **Prove it red once**, record it |
| 5.4 | Final gates: `tools/compile-check.sh` (0); Fast (0); All (0); duplicate-GUID grep over the `6B3D…` allocations; `git diff --stat Language/` shows only the `.st`; `git diff --stat` shows no change under `Configs/FieldManual/` or `Configs/System/` |
| 5.5 | Write the §7 play-test checklist into `tasks.md` under "Needs Human Verification" as a copy-pasteable list, including the seen-store reset procedure, the export prerequisite, and the ⚠️ warning that a client launch opens a window on the user's desktop and can orphan |
| 5.6 | Record in `context.md` the three findings of §9 (F1-F3) for the bug backlog, plus the Learn-more-hidden coverage note (D8) |

**Acceptance:** three cases added, each proven able to fail with the method and date recorded, **no `maxAttempts` anywhere**; all four commands green; the play-test checklist written down rather than described; findings recorded.

### Phase 6 — Help and documentation sync
*Agent: **`help-docs-sync`** (standard).* · *Estimate: half a session*

Required: this feature changes what players see in their first two minutes and on the campaign-setup screen.

| # | Task |
|---|---|
| 6.1 | Verify the Field Manual's **Welcome to Overthrow** page (`#OVT-FieldManual_Welcome_*`) is consistent with the shipped welcome pages, and specifically that `_Text2`'s "a house … with a car parked outside it" does not contradict `welcome-nohome`. If it needs a clause about the houseless case, add exactly that clause |
| 6.2 | **Wiki, scalpel not sweep** (the `tutorial-content` D10 rule). Update the campaign-setup / difficulty documentation where it states something §3.7 proves false — in particular anything claiming the supporting faction supplies troops or equipment, or that real-estate prices scale with difficulty. Budget: at most 2 pages updated, 0 created |
| 6.3 | **Do not touch** the `**Tutorial Jobs**` paragraph or item 6 under "Systems Worth Knowing About" on `getting-started` — those belong to `starter-jobs-retirement` |
| 6.4 | Do not re-audit pages for staleness; `field-manual` swept them on 2026-08-08 |

**Acceptance:** every wiki page updated is listed with its page id and a one-line reason; nothing created; the do-not-touch list intact; the manual's Welcome page and the two welcome entries do not contradict each other.

---

## 5. Key Technical Decisions

**D1 — Two entries and one modal, rather than one entry with conditional text or two stacked popups.** A tutorial page's body is a single localization key; the framework has no conditional text and adding one would be a schema change to `tutorial-system` for one use. Two entries filtered on the same event is the shape the framework already supports, and it means the player sees exactly **one** modal, of four pages, whichever spawn they got. Rejected: a shared 3-page welcome plus a separate 1-page "your home" entry, which shows two popups back to back in the first ten seconds — the precise annoyance this epic exists to remove.

**D2 — `welcome-intro` keeps its id, its file and its resource GUID.** Ids are immutable once shipped and never reused; the entry is already in players' seen stores. Only the pages, the filter and the link key change. The new entry gets the new id.

**D3 — Shared string keys for the three identical pages.** §3.2. Two copies of a sentence can drift and no gate can see it, and the houseless entry is the one least likely to be play-tested. The cost — a key name that does not match its entry id — is paid once in a comment; the benefit is paid every time somebody edits the copy. This deviates from the contract's key-naming convention deliberately and says so in both files.

**D4 — The spawn context is server-authored, because the client provably cannot derive it.** Three candidate client-side discriminators were checked and all three fail: `home != vector.Zero` is true in **both** branches because `SpawnPlayerAtFallbackPosition:824-839` calls `SetHomePos` (`:584-588`) too; `OVT_PlayerManagerComponent.RplSave/RplLoad:798-874` is a join-time snapshot sent before `FinalizePlayerPreparation` ever runs, so its `home` field is stale for exactly this decision; and ownership records live in the real-estate manager's `m_mOwned`, which is server-side. `FinalizePlayerPreparation:1011-1023` is the only place in the codebase that knows, so that is where the fact is captured.

**D5 — An owner RPC carrying the fact, not a server-side match-and-deliver.** The alternative — have the server run the matcher for `PLAYER_SPAWNED` and `Notify()` the winning id, the way the other ten events work — is arguably cleaner, and is explicitly **not** taken. `PLAYER_SPAWNED` is client-local because the server does not know when the client's possession completes; the whole bounded-retry apparatus (`:1445-1479`) exists because of that. Converting it to a server-driven event would be a `tutorial-system` framework change and would throw away machinery that a Campaign-tier case already pins. Carrying one string to the client and letting the existing pipeline decide keeps one pipeline.

**D6 — The client-side default is `"house"`, and that asymmetry is load-bearing.** `OVT_TutorialTrigger.Matches:116` treats `""` on the **trigger** as "no filter", but a `""` on the **context** matches no filtered trigger at all. So an empty default would suppress *both* welcomes rather than degrading to one. `"house"` degrades to today's behaviour: everybody reads the house page, which is what shipped before this feature. Documented at the field, because it looks like a value that wants tidying to `""`.

**D7 — The delivery waits for the context, but only for the existing retry budget.** The one genuine race is a client that learns the campaign is running (RplProp `m_bCampaignRunningRpl`, bumped at `:260`) before the context RPC lands (sent from `PrepareConnectedPlayers` at `:264`); RplProp-vs-RPC ordering is not guaranteed. Rather than add a timer, `NotifyPlayerSpawnedLocal` returns `false` while the context is unknown, which the existing 10 × 500 ms retry already treats as "try again". The **final** attempt passes `acceptDefaultContext = true` so a lost context costs a possibly-wrong page 2, never the whole welcome. Rejected: reordering `DoStartGame` to bump the RplProp after `PrepareConnectedPlayers`, which does not actually guarantee ordering across two different replication mechanisms and would perturb a start path that several other systems key off.

**D8 — Both welcomes carry the Field Manual link, and this costs the framework a play-test case.** `proofWelcome.conf` was deliberately the unlinked proof entry "so the Learn-more-hidden branch ships covered", and `tutorial-content`'s ten all link. Adding the link here means **no shipped entry exercises the hidden branch** (`OVT_TutorialContext.c:499-500`). Accepted: the requirement names `#OVT-FieldManual_Welcome_Title` as reserved for this feature, the branch is UI-only and outside the automated spine anyway, and it is a one-line inspection. Recorded in `context.md` so a future content pass can keep one entry unlinked if it wants live coverage back.

**D9 — Difficulty descriptions become `#OVT-` keys in the existing `description` field, not a new parallel field.** `TextWidget.SetText` resolves stringtable keys (`ArmaReforger/scripts/Core/generated/UI/TextWidget.c:115-116`, plus ~29 shipped Overthrow call sites), and `description` is read in exactly one file (`OVT_StartGameContext.c:129,134,207`). So the change is a key in a config and no script change at the read site. A parallel `descriptionKey` field would double the surface for no gain and leave the old field as a trap.

**D10 — The numbers come from the preset at runtime through `SetTextFormat`, and two of the three obvious candidates were disqualified by the fact-check.** `realEstateCostMultiplier` is read by nothing, and `respawnCost` is gated behind a hardcoded `money > 500`. `startingCash` and `fastTravelCost` are verified live reads on paths a player meets in the first hour. This is precisely the failure Rule 0 exists to catch: all three were on the shortlist, all three "obviously" describe the difficulty preset, and quoting the first two would have shipped two false sentences that no gate could see.

**D11 — The legacy hint's string items are retired in place, not deleted.** They carry six languages of translation, `.st` deletions churn exports the user regenerates by hand, and an unreferenced string item costs nothing at runtime. The `Comment` becomes the retirement record, including the three claims in its text that were or became false — which is the actual protection against somebody reviving it.

**D12 — `m_mSpawnContext` is kept server-side even though the RPC could be fire-and-forget.** It buys three things for about six lines: a reconnecting player is re-sent their context on the `already finalized` early return (`:989`), the server can answer the question afterwards at all, and the Campaign tier gets something assertable about a path that is otherwise entirely UI and network. The third is the reason it is not YAGNI.

**D13 — Faction descriptions are keyed by faction key, not spinner index.** The spinner is built by filtering the live faction list (`:94-108`); its contents and order are data, and a mod can change them. Index-keyed strings would silently mislabel factions. An unknown key shows nothing rather than the wrong thing.

**D14 — No new gameplay keybinding, and none is needed.** `tutorial-system` R3 established there is no free gamepad input during gameplay (all 16 bound in contexts live under a popup; `shoulder_left` and `KC_T` are VON at priority 110). The welcome is MODAL and focusable, so it brings its own Next/Back/Dismiss actions in the existing `OverthrowTutorial*` context. Nothing in this feature touches `chimeraInputCommon.conf`.

---

## 6. Definition of Done

An independent evaluator with no implementation context should be able to check every item below.

### Functional

- [ ] **F1 — The welcome fires once, on first spawn, per machine.** On a machine with a cleared seen store, starting a new campaign shows a four-page modal titled "Welcome to Overthrow" within a few seconds of the campaign starting. Dismiss it, restart the game, start another new campaign: **nothing appears.** Repeat once more on a *different* campaign/world: still nothing.
- [ ] **F2 — Four pages, navigable, dismissable anywhere.** Next advances 1→2→3→4, Back reverses, the page indicator reads `n / 4`, and the Dismiss control ends it from **any** page. Dismissing on page 1 marks the whole entry seen (expected; see the play-test note).
- [ ] **F3 — The houseless player gets the houseless text.** Forced by exhausting starting houses (§7 P3), a player who spawns at a bus stop sees page 2 describing what they actually have, and **never** the house-and-car page. Server log shows the fallback branch was taken.
- [ ] **F4 — Exactly one welcome, ever.** No session, on any path, shows both `welcome-intro` and `welcome-nohome`.
- [ ] **F5 — Two clients on a dedicated server each get their own welcome and only their own.** Two clients join via `tools/launch-server.sh` + two `tools/launch-game.sh --profile` sessions with cleared seen stores. Each sees one welcome. Neither client's log mentions the other's entry id or shows a second popup when the other player spawns.
- [ ] **F6 — JIP works.** A client joining a campaign that is already running gets its welcome on its own first spawn, with the correct page 2 for whichever branch the server took.
- [ ] **F7 — The legacy hint is gone and nothing regressed.** No `#OVT-IntroHint` corner hint appears after a death or after a Continue in single player. Home assignment, the starting car and starting cash are unchanged: a fresh player still spawns at a house with a car outside and the preset's cash.
- [ ] **F8 — Every start-menu selection shows an accurate description.** Moving the occupying spinner, the supporting spinner or the difficulty spinner updates the corresponding description immediately; the difficulty line shows the preset's real starting cash and fast-travel cost; when changing one faction auto-swaps the other, **both** descriptions update.

### Quality

- [ ] **Q1 — Rule 0 evidence, per sentence.** Every new or rewritten string item's `Comment` names the `file:line` (or the field-manual string id, itself sourced) making each claim true. An evaluator can pick any sentence in any welcome page or any setup description and find its evidence without asking the author.
- [ ] **Q2 — No disqualified claim present.** Nothing anywhere says real-estate prices scale with difficulty, that respawning costs a flat amount, that the supporting faction sends troops or supplies equipment, that you can disguise as the supporting faction, that you start with a pistol, that money can be stolen or deposited, or that the Overthrow menu holds money or the Field Manual.
- [ ] **Q3 — Sandbox tone.** No imperative addressed to the player, no goal, no objective, no order of play, no "now go and…". Each page reads correctly regardless of what the player did first.
- [ ] **Q4 — No hardcoded English added anywhere.** `grep` over the diff finds no user-facing literal in `.c` or `.layout`; the five shipped difficulty configs carry `#OVT-` keys; `Difficulty_TestWorld.conf` is untouched.
- [ ] **Q5 — Localization hygiene.** `git diff --stat Language/` lists `localization_Overthrow.st` and nothing else. Every new item has a fresh unique `{6B3D…}` GUID and a translator-useful `Comment`. `OVT-IntroHint` and `OVT-Overthrow` still exist, annotated as retired.
- [ ] **Q6 — Config and layout hygiene.** New `.conf` files write out every member explicitly; every new `.meta`, widget and slot has a fresh `{6B3D…}` GUID; a repo-wide duplicate-GUID grep over the new allocations prints nothing; no em-dashes.
- [ ] **Q7 — Green.** `tools/compile-check.sh` exit 0; Fast exit 0; All exit 0. Case count is the recorded baseline **+3**, and each of the three is recorded as proven able to fail, with method and date. No `maxAttempts` anywhere.

### Integration

- [ ] **I1 — No second home entry.** `tutorial-content`'s ten entries are byte-unchanged; `home-first-open` still owns ownership mechanics; page 2 does not restate them.
- [ ] **I2 — The framework is not drifted.** The only `tutorial-system` files changed are `OVT_TutorialComponent.c` (one field, one RPC, one signature) and the game mode's push site. No schema field, no new event, no UI layout under `UI/Layouts/HUD` or `Menu/TutorialPopup.layout`, no keybinding.
- [ ] **I3 — Both link keys resolve.** `OVT_TEST_Init_FieldManual_DeltaMergesAndLinksResolve` passes with twelve entries, covering both welcomes' `#OVT-FieldManual_Welcome_Title` for free.
- [ ] **I4 — Ids are right first time.** `welcome-intro` unchanged; `welcome-nohome` lowercase-kebab, ≤ 48 chars, unique, never used before.
- [ ] **I5 — Export handed over.** The complete list of new/changed string ids is reported for the user's Workbench re-export, with the note that welcome text and setup descriptions render as raw keys until then.
- [ ] **I6 — Findings recorded, not fixed.** §9's F1-F3 are written into `context.md` for the bug backlog; no attempt was made to wire up `realEstateCostMultiplier`, un-gate `respawnCost` or replicate the supporting faction.

### Verification Method

| Item | How |
|---|---|
| F1, F2, F3, F4, F7, F8, Q3 | Manual play-test, §7 checklist |
| F5, F6 | Two-client MP pass, §7 |
| Q1, Q2 | Read the `Comment` fields against the code they cite |
| Q4, Q5, Q6 | `git diff` + the greps named in Phase 5.4 |
| Q7, I3 | `tools/compile-check.sh`, `tools/run-tests.sh` both groups |
| I1, I2 | `git diff --stat` scoped to `Configs/Tutorials/`, `Scripts/`, `UI/Layouts/` |
| I4 | The Init entry guard (fails by name on a duplicate) |
| I5, I6 | The phase reports and `context.md` |

---

## 7. Testing Strategy

### Automated — three cases, one per tier that can hold one

The existing guards already cover a great deal for free: `OVT_TEST_Init_Tutorial_ManagerResolvesAndLoadsEntries` (`OVT_TEST_InitSuite.c:1566-1650`) walks **all** entries for empty/duplicate ids, missing pages and missing triggers; `OVT_TEST_Init_FieldManual_DeltaMergesAndLinksResolve` walks all link keys; and `OVT_TEST_Campaign_Tutorial_SpawnTriggerSurvivesCampaignStart` already pins that `PLAYER_SPAWNED` is delivered at all on the new-campaign path. **Do not duplicate any of them.** The three below assert things none of them can.

| # | Tier | Case | Asserts | Prove-red method |
|---|---|---|---|---|
| **T1** | **Logic** (world-free) | `OVT_TEST_Logic_Tutorial_SpawnContextSelectsOneWelcome` | Build two in-memory entries with `PLAYER_SPAWNED` triggers filtered `house` and `nohouse`, both priority 100. `OVT_TutorialMatcher.FindMatches` with ctx filter `"house"` returns **exactly one** id, the house one; with `"nohouse"`, exactly the other; **with `""`, exactly zero** — which is the asymmetry that makes D6's client-side default load-bearing, and the single most likely thing a future refactor breaks | Swap the two expected ids, or change the `""` expectation to 2 |
| **T2** | **Init** | New branch on the existing entry guard, mirroring its `CheckTransactionFilters` shape | (a) every `PLAYER_SPAWNED` trigger's `m_sFilter` is one of `""`, `"house"`, `"nohouse"` — a typo'd filter otherwise means a welcome that silently never fires; (b) at least one **enabled** entry matches `"house"` and at least one matches `"nohouse"`, so deleting or disabling either variant fails the build. Deliberately "at least one", not "exactly one", so a third-party mod adding a spawn entry does not break the gate | Misspell `nohouse` in `welcomeNohome.conf`; expect a named failure |
| **T3** | **Campaign** | `OVT_TEST_Campaign_Tutorial_SpawnContextIsAuthored` | After the campaign starts, `GetPlayerSpawnContext()` for the test world's local player returns one of the two known values (not `""`). This is what makes D12's server-side map worth having: it pins that `FinalizePlayerPreparation` actually ran the new code on a real start path | Comment out the `SetPlayerSpawnContext` call in the house branch; expect `""` |

Every case must be **proven able to fail once** with the exact failure text, method and date recorded in `context.md`. **`maxAttempts` is banned** — a test that needs retries is a bug in the test.

### What automation structurally cannot cover here

This feature is unusually exposed. Of its eight deliverables, four are outside the spine entirely:

- **Whether a popup renders, and how it feels.** The modal's four-page pacing, whether page 3's `<action>` glyph draws, whether the text fits 1000 × 620 without clipping, whether the first thing a player ever sees looks deliberate. UI is play-test-only by project rule.
- **Whether the campaign-setup screen looks bolted-on.** Three new text widgets in a vertical layout, one of them right-aligned under a spinner. Only a human can say whether it reads as designed.
- **Gamepad navigation** on the modal and the setup screen.
- **Multiplayer.** Per-player isolation on two clients was never observed passing for the tutorial framework (epic F7 — "unverified rather than passed"), and this feature adds a new owner RPC to that same path. **The two-client pass is a first-class deliverable, not a nice-to-have.**
- **Whether any sentence is true.** Nothing in the pipeline can detect a well-formed lie.

### Prerequisite: the user's Workbench export

New `.st` items are invisible until the user re-exports the string table in Workbench. **Until then the welcome and the setup descriptions draw raw `#OVT-` keys, which looks exactly like a bug.** The play-test cannot start before that step. First instruction of the checklist below is to confirm one known string renders its text.

### Play-test — single player

Reset procedure first: seen state lives in `$profile:.save/settings/ReforgerGameSettings.conf` under an `OVT_TutorialSettings` block. Clear that block (Workbench → **User Settings → Edit Game Settings**) and confirm `m_bTipsDisabled 0`. An absent `m_aSeen` is the empty state, not corruption.

| # | Step | Expect |
|---|---|---|
| P1 | Start menu: move the occupying spinner through every option | The occupying description changes each time; the supporting description **also** changes when the auto-swap fires; no raw `#OVT-` keys |
| P2 | Move the difficulty spinner through all five presets | Description and numbers line both change; Easy shows 500 starting cash and free fast travel; Extreme/Insane show 0 starting cash |
| P3 | Start a new campaign on a cleared seen store | The four-page welcome appears shortly after Start Game, titled "Welcome to Overthrow", page indicator `1 / 4`, page 2 describing a house, a car and cash |
| P4 | Page through 1→4 with Next, back with Back, then Dismiss on page 4 | Paging works both ways; the indicator tracks; Dismiss ends it |
| P5 | Alt-F4 or quit, relaunch, start another new campaign | **No welcome.** This is the per-machine seen store doing its job and is the single most important observation in the list |
| P6 | Clear the seen store, start a campaign, and Dismiss on **page 1** | It ends. Confirm the Overthrow menu → **Tips** re-opens it (framework behaviour: it re-opens the most recent tip) and note which page it resumes on — this is an observation, not a pass/fail |
| P7 | **Houseless path.** Exhaust the starting houses (easiest: a world/config with few starting houses, or join enough players) until the server log prints `No Starting homes left. Spawning at bus stop.` | That player's page 2 is the houseless text; the house-and-car page never appears for them |
| P8 | Die and respawn in single player | **No `#OVT-IntroHint` corner hint** (this is the Phase 3 regression check) and no second welcome |
| P9 | Quit, Continue the campaign | No corner hint, no welcome, and the campaign resumes normally |
| P10 | Learn More on the welcome modal | The Field Manual opens on **Welcome to Overthrow**, not the front page |

### Play-test — two clients on a dedicated server

⚠️ **Warn the user before launching a client** — it opens a window on their desktop and can orphan. **Always pass a long `--timeout`**; it defaults to 600 s and will kill the client mid-test.

```bash
tools/launch-server.sh
tools/launch-game.sh --timeout 3600 --profile OverthrowClient1 --allow-concurrent -- -client 127.0.0.1:2001
tools/launch-game.sh --timeout 3600 --profile OverthrowClient2 --allow-concurrent -- -client 127.0.0.1:2001
```

| # | Step | Expect |
|---|---|---|
| M1 | Both profiles start with a cleared `OVT_TutorialSettings` block | — |
| M2 | Client A joins and spawns | A sees its welcome. **B sees nothing** and B's log does not mention a welcome entry id |
| M3 | Client B joins and spawns | B sees its own welcome, once |
| M4 | **JIP:** with the campaign already running, disconnect B, clear B's seen store, reconnect | B gets a welcome on its own first spawn, with page 2 matching whichever branch the server took for B (check the server log) |
| M5 | Whichever client got the bus stop (if any) | Reads the houseless page 2 |
| M6 | Neither client | Sees two welcomes, or sees the other's |

This is the epic's outstanding F7 question and this feature's highest-risk unknown. If it fails, the failure is almost certainly in the shared delivery path rather than in this feature's filter — report it as a `tutorial-system` defect and say so.

---

## 8. Dependencies

- **`tutorial-system` (#1, built 2026-08-07).** Provides the schema, the multi-page MODAL sequence primitive, the seen store, the `PLAYER_SPAWNED` trigger with its bounded retry, and the owner-RPC delivery pattern this feature copies. **This is the only sibling this feature modifies**, and only at two points: one field plus one RPC on `OVT_TutorialComponent`, and the push site's final-attempt behaviour.
- **`field-manual` (#2, complete 2026-08-08).** Provides `#OVT-FieldManual_Welcome_Title` (frozen link table row 1, reserved for this feature) and the already-verified `_Text`/`_Text2` prose that pages 1 and 2 compress.
- **`tutorial-content` (#3, complete 2026-08-09).** Coordination only: its `home-first-open` owns ownership mechanics, and this feature must not add a second home entry. Its ten `.conf` files are not touched.
- **The user's Workbench string-table export.** A hard, external, human step between Phases 2/4 and any play-test.
- **`starter-jobs-retirement` (#5, after).** Page 3 and page 4 may name the Overthrow menu's **Jobs** entry only if #5 leaves it standing — #5 retires the five starter *jobs*, not necessarily the menu entry. Confirm before naming it, or name only entries with no pending change.
- **Base-game systems:** `TextWidget.SetText`/`SetTextFormat` translation, `ModuleGameSettings` (seen store), `SCR_FieldManualUI` (link target), `SCR_SpinBoxComponent`. None modified.
- **Tooling:** `tools/compile-check.sh`, `tools/run-tests.sh`, `tools/launch-server.sh` + `tools/launch-game.sh`.

---

## 9. Risks & Mitigation

**R1 — A welcome page or a setup description states a mechanic that does not exist.**
*Likelihood: medium — it has already happened twice on two entries, and the planning fact-check caught three more before a word was written. Impact: high; the welcome is the first text a player ever reads.*
No gate can catch it. Mitigated by making the fact-check **Phase 0**, before any writing; by §3.7 pre-loading both the verified and the disqualified claims with citations; by the rule that a body may only compress prose already verified in `field-manual`; and by requiring the evidence in the string's `Comment`. **Q1 and Q2 are the gate, and they are human.** Re-verify §3.7 rather than trusting it — `tutorial-content` shipped a false row in exactly such a table.

**R2 — The spawn context does not arrive and the houseless player reads the house page.**
*Likelihood: low-medium. Impact: medium; the exact failure this feature exists to prevent, and it fails silently.*
One real race exists (D7: `m_bCampaignRunningRpl` bumped at `:260` before the context RPC at `:264`, and RplProp-vs-RPC ordering is not guaranteed). Mitigated by the receive flag plus the existing 10 × 500 ms retry, which already treats `false` as "try again"; by the final attempt delivering with the default rather than dropping the welcome; and by the server-side cache re-sending on reconnect. Detection is play-test P7 and M5. Fallback if it still misses: move the context onto the `NotifyOwnerAssignment` RPC itself, which is guaranteed to precede controller registration.

**R3 — The listen-server / single-player direct-call branch is wrong and the host never gets its context.**
*Likelihood: medium — this is the mistake `Notify()` documents having to solve.* *Impact: high in SP, which is how most people play.*
The engine never loops an RPC back to the sender, so a missing direct-call branch means SP silently defaults. Mitigated by mirroring `Notify():124-139` exactly — **except** for the ownership test, which must be a player-id comparison rather than `IsOwnedByLocalPlayer()`, because that helper dereferences the controlled entity and `FinalizePlayerPreparation` runs before the character exists on the load-a-save path (`OVT_SpawnLogic.c:150-160`). Detection: P3 in single player is a direct test of this branch.

**R4 — `Rpc()` arity is wrong and the call dies at the wire.**
*Likelihood: low. Impact: high; compiles clean, tests pass, nothing works in MP (BUG-090).*
Mitigated by writing the signature and the argument count into this plan (§3.3), by task 1.7 requiring the count in a code comment, and by M2/M4 being the only observation that can catch it.

**R5 — Two clients still do not get their own welcomes.**
*Likelihood: unknown — never observed either way.* *Impact: high; it is the whole point of the epic's per-player delivery design and BUG-037's ghost.*
The epic records F7 as "unverified rather than passed", and this feature adds to the same path. Mitigated by making the two-client pass a first-class deliverable (M1-M6) rather than a stretch goal, and by the diagnosis rule: a per-player failure that reproduces for `economy-first-buy` too is a `tutorial-system` defect, not this feature's.

**R6 — A start-menu description is localized but the layout still shows a raw key.**
*Likelihood: medium; the export is an external human step. Impact: low, but it wastes a play-test session.*
Mitigated by task 4.6's explicit choice — either land the `#OVT-` key together with the export request or leave the literal placeholder until the same session — by every authoring phase ending with the id list, and by the checklist's first instruction being to confirm one known string renders.

**R7 — Dismissing on page 1 loses pages 2-4 forever.**
*Likelihood: medium; a player who wants to get moving will dismiss. Impact: low-medium.*
Accepted by requirement ("dismissable at any point"). Partially mitigated by the framework: the Overthrow menu's **Tips** entry re-opens the most recent tip (`OVT_MainMenuContext.c:284-301`), so the welcome is recoverable within the session. P6 observes and records what actually happens, including which page it resumes on. If play-testing shows this is a real loss, the cheapest answer is ordering — put the most valuable page first — not a framework change.

**R8 — Concurrent sessions touch the same files.**
*Likelihood: medium; this tree has hosted parallel sessions all week. Impact: medium; merge conflict or duplicate GUID.*
This feature appends to `Language/localization_Overthrow.st` and to `m_aTutorialEntries` on `Prefabs/GameMode/OVT_OverthrowGameMode.et` — the same two files `tutorial-content` and the jobs list touch — and edits `OVT_OverthrowGameMode.c`, which is the busiest file in the repo. Mitigated by the reserved `6B3D…` block (0 matches, so no collision is possible even under concurrent allocation), by appending rather than reordering, by the per-phase duplicate-GUID grep, and by **re-checking `git status` and the highest allocated GUID at the start of every phase** rather than trusting this document's snapshot.

**R9 — Scope creep into fixing what the fact-check found.**
*Likelihood: medium; three real defects surfaced during planning and all three are tempting one-liners. Impact: medium; they are balance changes wearing bug-fix clothes.*
Recorded and **not fixed here**:

| # | Finding | Why it is not this feature's job |
|---|---|---|
| **F1** | `realEstateCostMultiplier` is declared, replicated and read by nobody; `GetBuyPrice`/`GetRentPrice` (`OVT_RealEstateManagerComponent.c:712,730`) ignore it. The authored 0.4→2.0 spread has zero effect | Wiring it in would immediately double house prices on Insane and cut them 60 % on Easy. That is a balance change requiring a gameplay owner |
| **F2** | `ChargeRespawn` only charges when `money > 500` (`OVT_EconomyManagerComponent.c:1874`), a hardcoded gate unrelated to `respawnCost`, so the setting never bites the players it was designed for | Same: changing it changes the death-spiral difficulty |
| **F3** | The supporting faction is neither replicated (`OVT_OccupyingFactionManager.RplSave/RplLoad:1466-1507` send only occupying and player factions) nor persisted (no match under `Scripts/Game/Persistence/`), so an MP client always evaluates the script default `"US"` — and `OVT_WantedInfo.c:204-208` compares the undercover HUD icon against the wrong key when a host picked USSR. Related: `IsDisguisedAsSupporting()` (`OVT_PlayerWantedComponent.c:130-140`) has zero callers | A replication/persistence change to a faction field, with save-format implications |

All three go into `context.md` for the bug backlog. This feature's only obligation is to **not describe them as working**.

---

## 10. Quality Bar

This feature is two things at once, and each half has a different failure mode.

### 1. It is the first thing a player ever sees

- **It must look deliberate.** A four-page modal at 1000 × 620 with a page indicator, on top of a fresh campaign, is a designed moment or it is a bug report. Text that clips, a page that is one sentence long next to a page that is five, a raw `#OVT-` key: any of these and the player's first impression of the mod is "unfinished".
- **It must be brief enough to read and short enough to skip.** Four pages is the ceiling, not a target. Each page is a small number of short sentences.
- **The campaign-setup screen must not look bolted-on.** Three new text widgets inherit the existing description's slot shape, padding, font and alignment. If the occupying description sits differently from the difficulty description, it is wrong.
- **Sandbox tone is not negotiable.** Inform, never instruct. The welcome explains what the player has and where things are; it never tells them what to do next, and it never implies an order. The test: every sentence still reads correctly for a player who ignores all of it for three hours.

### 2. It is a per-player network path

- **Server authority, one direction.** The server owns the fact and pushes it; the client stores it and decides. No client-side inference, no round trip, no polling.
- **The direct-call branch is not optional.** Single player and listen hosts never receive their own RPCs. Every owner RPC in this feature has a local branch, and the branch is tested by the SP play-test, not assumed.
- **Arity is written down.** `Rpc()` is a variadic proto and compile-check cannot see a wrong argument count. The signature and the count live in a comment beside the call.
- **Degrade, never disappear.** Every failure mode of the transport ends with the player seeing *a* welcome. A lost context costs one page of accuracy; it never costs the welcome.
- **Two clients is the gate.** The epic's F7 was never observed passing. Anything shipped here is provisional until M1-M6 are run.

### 3. Truth, in both halves

- **A sentence with no `file:line` does not ship.** Not softened, not hedged, not moved to the manual: cut. The narrow verified claim beats the rich assumed one, every time.
- **Evidence lives in the artefact** — the string item's `Comment` — so the next author and the next translator both inherit it.
- **Compression is not invention.** A welcome page may say less than the manual page it links to. It may never say more.
- **Known-false is written down.** §3.7's disqualified list exists so nobody re-derives three defects that a nine-site source read cost to find, and so a reviewer can check copy against a list rather than against memory. It is re-verified each phase, because a trap table is not evidence.

### 4. Localization hygiene

- **`localization_Overthrow.st` is the only file touched under `Language/`.** The generated exports are the user's to regenerate; hand-editing has silently corrupted six of them before.
- **No hardcoded English survives** in the layout, the context, or the five shipped difficulty configs.
- **Numbers are composed through `SetTextFormat`**, never concatenated into a sentence.
- **Retired strings are annotated, not deleted**, and no id is ever reused.

---

*Plan authored 2026-08-09. Phase 1 routes to `network-specialist-advanced` and Phase 4 to `ui-developer-advanced`; `/proceed` must respect that routing. Phases 0, 2 and 6 route to `help-docs-sync`.*
