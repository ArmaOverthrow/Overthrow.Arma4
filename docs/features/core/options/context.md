# Core / Options - Context & Decisions

**Last Updated:** 2026-09-08 23:20
**Current Phase:** All 9 phases built
**Status:** 🟢 Ready for Review (play-test owed)

---

## Quick Status

**What's Done:**
- ✅ Requirements written 2026-08-09.
- ✅ Plan written 2026-09-08 (registry + generated form, 9 phases, 14 decisions).
- ✅ Docs scaffolded 2026-09-08.
- ✅ P1 built 2026-09-08: `OVT_OptionDefinition`, `OVT_OptionsRegistry`, 8 Logic cases. Logic suite: the 8 new cases green.
- ✅ P2 built 2026-09-08: `OVT_OptionsManagerComponent`, `OVT_Global.GetOptions()`, prefab block `6B8B000000000001`, Init case. Init suite 212/212.
- ✅ P3 built 2026-09-08: `OVT_OptionsRequestComponent`, `SetOption`/`RpcDo_SetOption`, JIP stream v1, controller prefab block `6B8B000000000002`, seam case line. Init suite 212/212.
- ✅ P4 built 2026-09-08: `OVT_OptionsManagerSerializer` v1, conf entry `6B8B000000000004`, `ApplyPersisted`/`GetPersistableOverrides`, round-trip case. Round-trip suite 46/46.
- ✅ P5 built 2026-09-08: `OVT_OptionsLocalStore`, `OVT_OptionsLocalSettings` + accessor, `SetLocalOption`/`FlushLocalOptions` on the manager, 3 Logic cases. Logic suite 319/319.
- ✅ P6 built 2026-09-08: `StopAutosaves`/`RestartAutosaves`/`IsAutosaveScheduled`, registry-driven `StartAutosaves`, `PushAutosaveDefaults`, invoker subscription, re-arm Init case. Init suite 213/213.
- ✅ P7 built 2026-09-08: four layouts + metas, `OVT_OptionsContext`, `OverthrowOptionsContext` action context, Options button enabled, context entry `6B8B000000000003`, ten `.st` items. Compile 0, input-conflict checker 0, Init suite 213/213.
- ✅ P8 done 2026-09-08: DoD 14/22 ticked with file:line, 5 manual, 2 partial (no CHOICE consumer, toggle round trip only at default). GUID ledger rewritten, 40 GUIDs, one definition each. Language diff = the `.st` master only. 5 comment blocks trimmed. All group 611/614.
- ✅ P9 done 2026-09-08: Field Manual Options sub-section, Main Menu page corrected, wiki draft at `docs/features/core/options/wiki-draft.md`.

**What's Next:**
- 📋 User: run the play-test checklist below (Workbench SP, gamepad, dedicated server).
- 📋 User: Workbench re-export of the string table (11 new keys, 1 changed key).
- ✅ 2026-09-10: `wiki-draft.md` published as the `options` page (id 75), with a link from `getting-started` (id 2) and `home` (id 1). See the session note below.
- 📋 User: commit. Nothing is committed.

**Blockers:**
- None.

**Needs human verification:**
- Workbench SP: the Options button opens the menu, the Server section shows two rows, the interval row reads a whole number plus `min`, the toggle stops and re-arms the autosave log line, the slider writes one request per release.
- Workbench SP: gamepad focus lands on the first row, d-pad travels row to row and reaches Close, `b` closes.
- Workbench SP: save, quit, Continue, the values persist in the menu.
- Localization: the ten new `.st` items need a Workbench re-export before they render.
- Scroll: the bare `ScrollLayoutWidgetClass` is unproven with pad focus below the fold (needs a third option).
- MP: forged write from a non-admin client is refused with one WARNING line (script console).
- MP: a client that joins after an option change reads the changed value (JIP stream).

---

## Key Files

### Core Implementation
- `Scripts/Game/Data/OVT_OptionDefinition.c` - the definition record and the two enums
- `Scripts/Game/Data/OVT_OptionsRegistry.c` - the pure registry, no engine call
- `Scripts/Game/GameMode/Managers/OVT_OptionsManagerComponent.c` - singleton, RPC, JIP, invoker
- `Scripts/Game/Components/Controller/OVT_OptionsRequestComponent.c` - client to server seam
- `Scripts/Game/Persistence/Serializers/Components/OVT_OptionsManagerSerializer.c` - save record
- `Scripts/Game/UI/Context/OVT_OptionsContext.c` - the generated form

### Related Files
- `Scripts/Game/GameMode/Managers/OVT_PersistenceManagerComponent.c` - the first consumer (autosave)
- `Scripts/Game/Global/OVT_MapLayerSettings.c` - the LOCAL store pattern to copy (D10)
- `Scripts/Game/Components/Controller/OVT_FuelRequestComponent.c` - the request seam pattern (R3)
- `docs/features/core/options/requirements.md` - requirements
- `docs/features/core/options/implementation.md` - the plan and decisions D1 to D14

---

## Important Decisions

The plan records D1 to D14 in `implementation.md` section 5. This file records only
decisions made during the build.

### Decision: The JIP stream sends every FACTION and WORLD value, not only the overrides
**Date:** 2026-09-08
**Context:** The plan said `GetOverrides` feeds both the save and the JIP stream. An override is a value that differs from the SERVER default. After P6 pushes a prefab attribute through `OverrideDefault`, a server with an edited attribute and no stored value sends nothing, and the client draws its compiled default.
**Decision:** `RplSave` writes the effective value of every FACTION and WORLD id. The serializer path stays overrides-only.
**Impact:** Two pairs on the wire today. A client mirror always equals the server value.

### Decision: Single player reports no admin role, so the host fallback is the live branch
**Date:** 2026-09-08
**Context:** P3.3 asked for a Workbench check. The vanilla source answers it: no script grants `EPlayerRole.ADMINISTRATOR`, and vanilla pairs every `IsAdminRole` test with `Replication.IsRunning()` to suppress it in single player.
**Decision:** `PlayerMayEditWorld` keeps the D7 fallback `Replication.IsServer() && playerId == SCR_PlayerController.GetLocalPlayerId()`. The comment above the method records the finding.
**Impact:** Single player, Workbench and a listen host reach the WORLD section through the fallback. A dedicated server needs the admin role.

### Decision: The Options menu has its own ActionContext
**Date:** 2026-09-08
**Context:** The plan's file map named no `chimeraInputCommon.conf` edit. Every other Overthrow menu context has a matching `ActionContext` block, and without one a gamepad cannot move inside the screen.
**Decision:** `OverthrowOptionsContext`, Priority 50, Flags 4, listing `MenuBack`, `MenuSelect`, `MenuUp`, `MenuDown`, `MenuLeft`, `MenuRight`. No new `Action`, so no new binding.
**Impact:** The input-conflict checker reports 0 errors, 0 warnings.

### Decision: The Close button is a navigation button on MenuBack
**Date:** 2026-09-08
**Context:** The plan asked for a `PauseMenuButton` wired through `m_OnClicked`. That button draws no key or pad glyph.
**Decision:** `WLib_NavigationButton` bound to `MenuBack`, wired through `m_OnActivated`. A mouse click reaches the same invoker through vanilla `OnClick`. `CloseLayout()` guards the second call when the context close listener also fires.
**Impact:** Console players see the `b` glyph. The footer matches every other Overthrow menu.

### Decision: Widget GUIDs use `6B8B000000000050..61` and `80..83`
**Date:** 2026-09-08
**Context:** The plan reserved 16 spares at `0040..004F`. The four layouts need 22 widget GUIDs.
**Decision:** The `0040` band stays free. P8.2 records the used rows in section 3.8.

### Decision: Earlier admin-command slice is out of this feature
**Date:** 2026-09-08
**Context:** The old `context.md` (deleted with the re-plan) recorded a `/give-money` admin chat command
built on 2026-08-10 as a first slice of "options".
**Decision:** That command shipped in commit a3810b49 and stays. This feature builds the registry and menu
only, as the 2026-09-08 plan describes.
**Impact:** Nothing in this feature touches `OVT_AdminCommandsComponent`.

---

## Gotchas & Learnings

### 1. Save context keys by local variable name
**Problem:** A renamed local in `Deserialize` silently reads zeros (R2).
**Solution:** The array is spelled `records` in both `Serialize` and `Deserialize`.
**Lesson:** Only the round-trip case can catch a regression.

### 2. A synchronous option echo breaks the vanilla toolbox click
**Problem:** The user saw the toggle with no highlight on the current value and a white line under the other one. A click on a `WLib_Checkbox` element runs `SetToggled` first (which reports the change) and `OnElementClicked` second (which moves the selection). In single player the server is local, so the write echoed back inside step one and the row redraw moved the selection early. Step two then saw "already selected" and toggled the new value off.
**Solution:** `OnOptionChanged` queues `RefreshRow` through `CallLater(..., 0)`, and `OnClose` removes the queued call. Fixed 2026-09-09.
**Lesson:** Never redraw a widget from inside its own change event. Defer one frame.

### 3. Suites are not deterministic under load
**Problem:** Init cases time out when several suites run in sequence.
**Solution:** The orchestrator runs one suite alone as the phase gate.

---

## Session Notes

### 2026-09-08 - Scaffold
- Plan reviewed. Docs scaffolded. P1 starts next.

### 2026-09-08 - P1 gate
- Logic suite 315/316. The 8 Options cases pass. The one red is `OVT_TEST_Logic_BaseDefenseEscalation_PartialAndDefensiveInputs`, a 500 ms framework timeout (0.63 s run) on a case that reads no Phase 1 file. Treated as load-related and pre-existing.
- Registry API adds `SetDefault`, `IsPending`, `ClearValue`, `HasDefinition`, `GetIdsForScope` beyond the plan text. Definition factories `MakeToggle`, `MakeSlider`, `MakeChoice` set every field explicitly.

### 2026-09-08 - P2 gate
- Init suite 212/212. `OnDelete` clears `s_Instance` (the persistence manager pattern), so no pointer survives a world reload.

### 2026-09-08 - P3 gate
- First Init run: all 212 cases passed, then the client hung at engine shutdown (no log line after `Audio -> Stop`) and the harness returned exit 2. Crash-log entries were identical to the green P2 run. Second run: 212/212, exit 0.
- Steps 6 and 7 of the handler share `PlayerMayEditScope`, so the form and the server use one predicate. FACTION accepts officer OR admin.
- `ApplyValue` is the one body behind `RpcDo_SetOption` and `RplLoad`.

### 2026-09-08 - P4 gate
- Round-trip suite 46/46. The Options case moves the interval to 1800, saves, dirties to 600, reloads, and reads 1800 back with the untouched toggle still at its default.

### 2026-09-08 - P5 gate
- Logic suite 319/319. The P1-run timeout on the base-defense case did not repeat, so it was load.
- `RegisterOption` pulls a late-registered LOCAL definition's value from the store. `FlushLocalOptions` is the only flush point, for the P7 menu close.

### 2026-09-08 - P6 gate
- Init suite 213/213. The options block sits after the persistence block on the game-mode prefab, so `PushAutosaveDefaults` runs from both `OnPostInit` (a no-op there) and `StartAutosaves` behind one flag.
- No `OVT_Global.GetPersistence()` exists. The path is `OVT_Global.GetOverthrow().GetPersistence()`.

### 2026-09-08 - P7 gate
- Init suite 213/213 after the prefab and input-config edits. UI itself is outside the automated spine: visual, gamepad and the minutes format are on the human list.
- Vanilla layout GUIDs came from existing Overthrow references (the reference extraction has no `.meta` files): checkbox `5D5055E10FD00549`, spinbox `C9DF0E6590F6C388`, slider `4A41296C0E9A889F`, nav button `08CF3B69CB1ACBC4`.
- No `ShowHint` for a missing manager: the ten reserved strings are all used, so the screen logs one ERROR and opens empty.

### 2026-09-08 - P8 gate
- All group 611/614 in 146 s. The three reds are `Timed out after 500ms` framework timeouts on `OVT_TEST_Init_MountedForce_HoldingFallsBackToTheMarch`, `OVT_TEST_PersistenceRoundTrip_HighCommandMemberBodies_RoundTrip` and `OVT_TEST_Init_HighCommandSeam_DHeartbeatFillsTheRecord`. No options file touches them, and each passed in its isolated suite run this session (Init 213/213, round-trip 46/46). This is the known load pattern. All 14 Options cases green.

### 2026-09-09 - Toggle highlight fix
- User play-test found the toggle highlight on the wrong value. Root cause and fix in Gotcha 2. Compile 0. Suites skipped (UI only). Play-test of the toggle owed again.

### 2026-09-08 - P9 and close
- Field Manual: `FM_Overthrow.conf` gains an Options sub-section under Main Menu (GUIDs `6B8B00000000002A`, `2B`), `OVT-FieldManual_MainMenu_Text2` no longer says the button is disabled, new key `OVT-FieldManual_Options_Text` (`6B8B00000000002C`). Suites skipped for this phase: conf, `.st` and docs only.
- The wikijs MCP server was not connected. The wiki page is a draft file.

### 2026-09-10 - Wiki pass (P9.1 done)
- Fact-checked the draft against the shipped source: `OVT_OptionsContext.c:106-108` (three sections), `:165-182` (empty-section and permission gate), `OVT_OptionsManagerComponent.c:72-91` (the two WORLD options and their ranges), `:267-282` (apply at once, broadcast), `OVT_OptionsRequestComponent.c:103-143` (officer and admin gate, host fallback). All facts held.
- Published a new `options` page (id 75) with the draft text, lint score 0.00 per 100 words. Linked it from `getting-started` (id 2, lint 2.04 for the whole page) and from `home` (id 1, a label bullet).
- `docs/features/resistance/vehicle-storage/requirements.md` changed on disk at 21:35 tonight. No agent of this run had it in scope. Left as found.
- Every change is uncommitted in the working tree on `main`.

### 2026-09-08 - P8 review
- DoD walked against the code. Every automated claim has a file:line citation in `implementation.md` section 7.
- The GUID audit found no collision. 40 GUIDs total, each with one definition site.
- `git diff --stat Language/` touches one file, 170 insertions, 0 deletions.
- Comment trim: 5 file-header or method blocks cut from 6 or more prose lines to under 6, across `OVT_OptionsRegistry.c`, `OVT_OptionsManagerSerializer.c`, `OVT_OptionsLocalSettingsAccessor.c`, `OVT_OptionsContext.c` and one `StartAutosaves()` doc comment in `OVT_PersistenceManagerComponent.c`. Every non-comment line stayed byte-identical. The combined comment prose of the two largest files scores 2.35 violations per 100 words on the STE linter.
- Two gaps found that the plan text did not call out. `CHOICE` has no shipped consumer, so its render and write path in `OVT_OptionsContext.c` cannot be play-tested at all, only proven at the registry level by a Logic case. The persistence round-trip case moves only the interval off its default. It never proves that a non-default `autosave.enabled` value survives a reload, only that the untouched default does.
- `tools/compile-check.sh` exits 0 after the trim.

---

## Play-test checklist

This checklist gives the manual steps the automated suites do not cover. Follow the numbered
steps in order for each group.

### Group 1: single player, in Workbench

1. Start a new campaign.
2. Open the main menu.
3. Select Options.
4. Confirm the menu shows three section titles.
5. Confirm the Server section shows two rows.
6. Read the autosave interval row. Confirm it shows a whole number and the word "min".
7. Turn off Automatic saving.
8. Check the console log. Confirm no new "Autosave scheduled every..." line appears.
9. Turn Automatic saving back on.
10. Move the interval slider to 5 minutes.
11. Check the console log. Confirm one new "Autosave scheduled every 300 seconds" line.
12. Close the menu.
13. Save the campaign.
14. Quit to the main menu.
15. Select Continue.
16. Open Options again.
17. Confirm the interval still reads 5 min.
18. Confirm the toggle is still on.

Note: the log line "[Overthrow] Save point FAILED" on every autosave is normal in Workbench. It
is not a defect. A deployed single-player or multiplayer session saves correctly.

Note: the ten new `.st` items need a Workbench re-export before they render. Until the export
runs, a row may show its raw string key instead of its label.

### Group 2: gamepad

1. Connect a gamepad.
2. Open the main menu with the gamepad.
3. Move focus to Options and select it.
4. Confirm focus lands on the first row with no extra input.
5. Press down on the d-pad. Confirm focus moves to the next row.
6. Press down until focus reaches the Close button.
7. Confirm the Close button shows a gamepad glyph, not a keyboard key.
8. Press the button bound to MenuBack.
9. Confirm the menu closes.
10. Reopen Options and move focus to the section list.
11. Scroll past the first screen of rows with the d-pad.
12. Confirm focus stays visible and does not jump off screen.

### Group 3: dedicated server

1. Start the server with `tools/launch-server.sh`.
2. Join with a second client that holds no officer or admin role.
3. Open Options on that client.
4. Confirm no section renders, because Local is empty and the other two stay hidden.
5. Promote that player to officer.
6. Reopen Options. Confirm the Resistance section appears.
7. Give that player the admin role.
8. Reopen Options. Confirm all three permitted sections appear.
9. As the admin, change the autosave interval.
10. Check the other client's copy of the menu. Confirm it shows the new value.
11. Join with a third client after the change.
12. Open Options on the third client. Confirm it shows the new value.
13. Open the Workbench script console on the non-admin client.
14. Resolve `OVT_ControllerComponent<OVT_OptionsRequestComponent>.Get()`.
15. Call `SetOption("autosave.interval", "3600")` on it.
16. Check the server log. Confirm one WARNING line naming the player id, the option id, and the
    WORLD scope.
17. Confirm the option value did not change.
