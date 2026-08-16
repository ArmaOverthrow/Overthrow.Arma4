# Map Layers & Legend - Task Checklist

**Last Updated:** 2026-08-13
**Progress:** 100/112 tasks complete (89%) — **Phase 7's SP, gamepad, persistence and Phase-4b probe rows all discharged by the user 2026-08-13; only the two-client MP rows (I-2 … I-4) remain**

> Generated from `implementation.md` §5 by `/start-feature map/map-layers`.
>
> ⚠️ **Most of this feature is invisible to every automated gate.** `.layout`, `.meta`, `.conf` and `.st`
> edits are seen by neither `tools/compile-check.sh` nor either test group, and this project has no widget,
> input or rendering test tier. The tool-menu entry, the panel, every `FindAnyWidget` name, all gamepad
> behaviour, the `ModuleGameSettings` round trip and MP isolation can only be observed by a human in a
> running session — **Phase 7 is this feature's only evidence** for §7 of the plan.
>
> ⚠️ **Baselines are re-measured at every phase boundary, never quoted.** `CLAUDE.md` says Fast 38 / All 66
> and `territory-overlay` says Fast 66 / All 101 — **both are stale.** Parallel sessions commit to this
> tree. A changed count is a **finding to investigate**, never a number to update.

---

## Phase 0 — Baseline — **S — no agent** (5/5 complete)

- [x] `tools/compile-check.sh` → exit 0, **5988 files**, Game module, 5 s
- [x] `tools/run-tests.sh "{6A6E29FF47ECB840}"` (Fast) → OK, **87 tests**, 35 s
- [x] `tools/run-tests.sh "{6A6E2A002F53A581}"` (All) → OK, **125 tests**, 39 s
- [x] Re-check `git status` (clean but for this feature's untracked docs) and highest `docs/bugs/` id (**BUG-145**), at HEAD `ecf1a696`
- [x] Confirm the `{6A85…}` GUID series is still free (`grep -rl "6A85"` → zero hits)

**Expected end state:** compile **5994 files**, Fast **89**, All **127** (+6 files, +2 Logic cases).

---

## Phase 1 — 🔴 Entry-point and layout skeletons — `ui-developer` (8/8 complete) ✅

> The plan shapes this as a throwaway spike **run and observed before anything depends on it**. In an
> autonomous run nobody is at the screen, so the four spike questions **P1 – P4 are folded into the Phase 7
> gate** (rows are carried there verbatim) and the layouts are authored in their final widget-name shape
> from the start, exactly as the plan already asks. Recorded as **D1** in `context.md`.

- [x] **P1 (code reading substitute)** — establish whether `SCR_MapToolMenuUI` reaches Overthrow's fullscreen map: confirm it is in **vanilla's** `Configs/Map/MapOverthrow.conf` and that Overthrow's file is a same-GUID delta. If Overthrow's `m_aUIComponents` does not carry it, add `SCR_MapToolMenuUI "{599C7D68E8F6B9A8}" { }` — a **delta on that entry, not a duplicate**, safe whether the array merges or replaces
- [x] `Scripts/Game/UI/Map/OVT_MapLayersUI.c` — `SCR_MapUIBaseComponent` subclass; `[Attribute] string m_sToolMenuIcon` default **`"filters"`** (K6), `[Attribute] int m_iSortPriority` default **`3`** (K11)
- [x] 🔴 **`Init()` registers the tool-menu entry — never `OnMapOpen`** (K7): resolve via `m_MapEntity.GetMapUIComponent(SCR_MapToolMenuUI)`, `RegisterToolMenuEntry(SCR_MapToolMenuUI.s_sToolMenuIcons, m_sToolMenuIcon, m_iSortPriority, m_bIsExclusive)`, register **once**. Comment the trap at the call site — `OVT_MapLocationType.Init()` has the **opposite** lifetime
- [x] Wire `m_OnClick.Insert(TogglePanel)`, `GetOnDisableMapUIInvoker().Insert(ClosePanel)`, `SetEnabled(true)` (cosmetic border only — **not** a gate)
- [x] `ResolveDockParent()` — `ToolFramesOverlay` → `ToolMenuContainer` → `m_RootWidget`, ERROR-logging which one it settled on; panel **created into** the overlay, vanilla layout never overridden (K9)
- [x] `Configs/Map/MapOverthrow.conf` — register `OVT_MapLayersUI` in `m_aUIComponents` with **`m_bIsExclusive 1`** (K12 — a rendering requirement, not a preference)
- [x] `UI/Layouts/Map/Core/OVT_MapLayersPanel.layout` + `.meta` — final widget-name shape per §3.6 (`LayersPanel`, `PanelTitle`, `OverlaysHeader`, `OverlayRows`, `MarkersHeader`, `TypeRows`, `FocusProxy`), placeholder content. GUIDs from `{6A85…}`, **all six platform configurations** in the `.meta`
- [x] Gate: compile **exit 0 / 5989 files**; Fast **87**, All **125** — unchanged (nothing here is assertable)

---

## Phase 2 — The preference store — `component-developer` (8/8 complete) ✅

> The one genuinely testable part, split three ways so it can be. Mirror
> `OVT_TutorialSeenStore` / `OVT_TutorialSettings` / `OVT_TutorialSettingsAccessor` **exactly** (K1).

- [x] `Scripts/Game/Data/OVT_MapLayerPrefsStore.c` — **pure**: no engine call, no `BaseContainer`, no widget. `ref set<string>` of **hidden** keys; **absent ⇒ visible** (K13)
- [x] Store API — `IsVisible(string)`, `SetHidden(string, bool)`, `LoadFrom(array<string>, int)`, `WriteTo(out array<string>)`, `Count()`, `Clear()`
- [x] `static TypeKey(className)` → `"type:" + className`; `static LayerKey(layerId)` → `"layer:" + layerId` — **namespace-prefixed so the two id spaces cannot collide**
- [x] Guards — `CURRENT_VERSION = 1` and a mismatch **clears** rather than half-trusting; `MAX_HIDDEN` cap that **warns once and refuses** rather than evicting; empty keys refused; `LoadFrom(null, …)` treated as empty
- [x] `Scripts/Game/Global/OVT_MapLayerSettings.c` — `[BaseContainerProps()] OVT_MapHiddenLayerEntry { string m_sKey; }` + `OVT_MapLayerSettings : ModuleGameSettings` with `m_iVersion` and `ref array<ref OVT_MapHiddenLayerEntry> m_aHidden`. 🔴 **Nested struct in an object array — never a top-level `ref array<string>`, never parallel arrays** (K1a/K1b)
- [x] `Scripts/Game/Global/OVT_MapLayerSettingsAccessor.c` — `Load` / `Save` / `Reset`; `System.IsConsoleApp()` early-out; null-guard `GetGameUserSettings()` **and** `GetModule()` (neither worth a log line); **read-modify-write against the live container**; allocate `m_aHidden` when the loader hands back null; write the **whole record** every time. ⚠️ `WriteToInstance` is the **load**, `ReadFromInstance` is the **store**
- [x] `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_MapLayerPrefs.c` — **two cases**: `…_HiddenSet` (absent ⇒ visible, idempotent both ways, empty key refused, version mismatch discards, `LoadFrom(null)` is empty not a crash, `WriteTo` round-trips into a reused pre-populated buffer with nothing stale) and `…_KeyNamespaces` (a type and a layer with the same bare name do not collide)
- [x] 🔴 **Prove both cases able to fail** — each inversion applied **alone** to a pristine copy, compiled, that one case run, reverted, clean gate re-run; exact failure message recorded in `context.md`. ❌ No `maxAttempts`
- [x] Gate: compile **exit 0 / 5993 files**; Fast **89**, All **127** — the +2 are the new Logic cases and are the **only** expected change

---

## Phase 3 — Toggle primitives and labels — `component-developer-advanced` (**ADVANCED**) (12/12 complete) ✅

> **Advanced** because it edits the base class all 14 types inherit _and_ a published contract, a hot path,
> the map UI component, and a retained legacy component `legacy-retirement` marked do-not-touch.

- [x] `OVT_MapLocationType` — `[Attribute(defvalue: "")] protected string m_sCategoryName` + `GetCategoryName()` with the fallback chain `m_sCategoryName` → `GetDisplayName()` → `ClassName()` and a **warn-once**
- [x] `OVT_MapLocationType` — `protected bool m_bPlayerVisible = true;` + `SetPlayerVisible(bool)` / `IsPlayerVisible()`. ⚠️ **Do not touch `m_sName` or `m_sDisplayName`** (K2). ⚠️ **Do not reset `m_bPlayerVisible` in `Init()`** — `Init()` runs on **every** map open
- [x] `OVT_MapLocationElement.SetVisible` — the **fourth gate, inserted first**, immediately after the `!m_LocationType` guard: hidden ⇒ `super.SetVisible(false); return;` so a hidden type **skips** the zoom lookup and `ShouldShowLocation` entirely (Q-4)
- [x] 🔴 The gate does **not** go in `ShouldShowLocation` (a per-record virtual with live manager lookups), and **no element is ever destroyed or recreated by a toggle** (G2 / BUG-136 hazards)
- [x] `OVT_MapLocationElement.RefreshVisibility()` → `SetVisible(m_bVisible)`; `OnZoomChanged()` **and** `OnLocationDataChanged()` both call it — one implementation, not two
- [x] `OVT_OverthrowMapUI.GetLocationTypes()` — null-safe accessor over `m_Config.m_aLocationTypes`
- [x] `OVT_OverthrowMapUI.RefreshAllVisibility()` — the `m_mIcons` sweep, shaped **exactly** like `OnMapZoom`
- [x] `OVT_OverthrowMapUI` — generalise `IsSelectionOnInfoPanel` into `IsSelectionInsideWidget(Widget, vector)`, keep the old name as a thin wrapper, and have `OnMapSelection` **also** test the layers panel so a click on a filter row does not unpin the info panel (K14)
- [x] `OVT_MapPlayerLocation` — `protected bool m_bMarkersVisible = true;` + `SetMarkersVisible(bool)` (loops `m_Widgets`, uses **`SetVisible`, never opacity**, so it cannot interact with F3's opacity defect) / `AreMarkersVisible()`; `Update()` early-returns when hidden
- [x] `OVT_MapPlayerLocation` — `m_bAvailableThisSession` + `IsAvailableThisSession()`, set **true** only past **both** `OnMapOpen` early returns (no controlled entity; `m_Difficulty.showPlayerOnMap` false), reset **false** at the top. A difficulty-disabled marker must present **no row**, not a dead toggle. ⚠️ Leave the class otherwise as-is — the vestigial `m_ToolMenuEntry` / `ZoomInOnPlayer` is **F1, not this feature's to delete** (K3)
- [x] `Language/localization_Overthrow.st` — **18 new `CustomStringTableItem` blocks**, GUIDs from `{6A85…}`, `Target_en_us` only, `Comment` filled for the translator: `OVT-Map_Layers_Title/_Overlays/_Markers`, `OVT-Map_Layer_Players`, and 14 × `OVT-Map_Category_*` with English **taken verbatim from each type's existing `m_sName`** (already the correct plural in all 14 cases). ❌ **NEVER touch `localization_Overthrow.<lang>.conf`**
- [x] `Configs/Map/OverthrowMap.conf` — `m_sCategoryName "#OVT-Map_Category_*"` on all **14** entries. ⚠️ `.conf` files carry **no comments**
- [x] Gate: compile **exit 0 / 5993 files** (unchanged — no new `.c`); Fast **89**, All **127**. **By inspection:** with no panel yet the map is byte-for-byte unchanged

---

## Phase 4 — The panel — `ui-developer-advanced` (**ADVANCED**) (10/10 complete) ✅

> **Advanced.** The whole player-facing surface, console-critical, and where the `FindAnyWidget` failure
> class lives (BUG-133 `IconLayout`, BUG-134 `CloseButton`). **Nothing in it is visible to any gate.**

- [x] `OVT_MapLayersPanel.layout` final content — fixed-width vertical layout, **top-left aligned inside its `OverlayWidgetSlot`** so it hugs the icon strip; background at the shop menu's opacity so labels read against terrain; **scroll container** around the rows (17 rows will not fit a short screen — R7/Q-2)
- [x] `UI/Layouts/Map/Core/OVT_MapLayerRow.layout` + `.meta` — `RowIcon` (13×13, the epic's info-row convention), `RowLabel`, `RowCheckbox`
- [x] 🔴 `RowCheckbox` inherits `{?}UI/layouts/WidgetLibrary/ToolBoxes/WLib_Checkbox.layout` — its `SCR_CheckboxComponent` override **MUST reuse the base layout's component GUID `{546A9B7B0A8AD927}`**. A fresh GUID adds a second, unconfigured component and the checkbox goes dead
- [x] `Scripts/Game/UI/Map/Core/OVT_MapLayerRowComponent.c : SCR_ScriptedWidgetComponent` — `Init(key, label, imageset, icon, visible, owner)`. **`Init()` does the wiring, not `HandlerAttached()`** (sibling-handler ordering is not guaranteed); `SCR_CheckboxComponent.GetCheckboxComponent("RowCheckbox", m_wRoot)` then `m_OnChanged` wired **once** behind an `m_bWired` flag
- [x] Rows **never mutate state themselves** — they call back into the owner with the key and the new state. `RowIcon` hidden when the imageset is empty (overlay and player rows have no icon source)
- [x] `OVT_MapLayersUI.BuildRows()` from the **three** sources (§3.4) with the **four skip filters**, each with a one-time WARNING naming what was skipped: empty `m_sLayerId`, empty `m_sDisplayName`, duplicate key, (and the disabled threat grid, which is excluded **structurally** — `ActivateModules` skips it before `SetActive(true)`, so it can never be in `GetLayers()`)
- [x] Clear children before building (`while (container.GetChildren()) container.RemoveChild(container.GetChildren())`) so a rebuild cannot stack duplicates; rebuild rows on **every** panel open
- [x] 🔴 **Row state is read live from the objects** (`IsPlayerVisible` / `IsLayerVisible` / `AreMarkersVisible`), **never from the store** — the panel cannot disagree with the map
- [x] `TogglePanel()` copying `SCR_MapJournalUI.ToggleVisible` — flip visibility, `m_ToolMenuEntry.SetActive`, and **focus the first row when becoming visible** (K10); `FocusProxy` bounces engine focus to the first row via `SCR_EventHandlerComponent.GetOnFocus()`; `ClosePanel()` on the disable-map-UI invoker; teardown removes exactly what was inserted; `GetPanelWidget()` returns null when closed (consumed for K14)
- [x] Gate: compile **exit 0 / 5994 files** (+1); Fast **89**, All **127** — unchanged

---

## Phase 5 — Persistence wiring and resilience — `component-developer` (6/6 complete) ✅

- [x] `OVT_MapLayersUI` owns one `ref OVT_MapLayerPrefsStore`, loaded **once** on the first `OnMapOpen` and cached for the process (the profile cannot change under a running client)
- [x] `ApplyPreferences()` — walk all three sources and push `store.IsVisible(key)` into each object. **Idempotent by construction**
- [x] 🔴 **Call it twice per map open** (K8): from `OnMapOpen` (no visible flash) **and** from the first `Update` tick after an open, one-shot (provably after every module and component has run its `OnMapOpen`, whatever the subscription order). Layers only exist in `GetLayers()` while a map is open — _"my saved toggles didn't stick"_ is the single most likely bug in this feature
- [x] **Flush points: panel close and map close only** (K15) — never per toggle. The engine **throttles** `SaveUserSettings()`: two calls microseconds apart leave only the **first** on disk. Both points write the **whole record**, so a dropped flush loses nothing
- [x] Degrade cleanly — on console-app/headless, or with no settings module, the store stays in memory and **every toggle still works for the session**. Applying to the map is decoupled from flushing
- [x] Gate: compile **exit 0 / 5994 files** (unchanged); Fast **89**, All **127**

---

## Phase 4b — 🔴 Gamepad row navigation — `ui-developer-advanced` (**ADVANCED**) (4/4 complete) ✅

> **Not in the original plan. Added 2026-08-11 by user decision D4** after Phase 4 found that the plan's
> D1 ("a vanilla tool-menu entry closes the gamepad requirement for free") is only **half** true. The entry
> is reachable on pad for free; **walking the rows inside the panel is a different input surface**, and
> `MapContext` carries **zero** `Menu*` navigation actions — verified independently.
>
> Vanilla hit this exact problem **twice** and solved it the same way both times: `TaskListMapContext`
> (priority 70, above `MapContext`'s 50) re-activated **every frame** by `SCR_UITaskManagerComponent` while
> the task list is up, and `MapMarkerEditContext` likewise by `SCR_MapMarkersUI`. That is vanilla telling us
> the answer, not a guess.
>
> ⚠️ **This deliberately overrides acceptance criterion Q-3** ("`git diff` on `chimeraInputCommon.conf` is
> empty"). Q-3's _purpose_ was "no new binding" — do not consume one of the scarce free keys on a context
> that already has 41 live actions and `KC_H` taken three times over. **No new key is consumed here**: the
> new context only **re-references `Menu*` actions that already exist**. The letter of Q-3 is broken; its
> reason is intact. Recorded so the next reader does not think it was missed.

- [x] Add an Overthrow `ActionContext` to `Configs/System/chimeraInputCommon.conf` re-referencing the existing `MenuUp` / `MenuDown` / `MenuLeft` / `MenuRight` / `MenuSelect` (+ `MenuBack` if it helps close the panel) at a priority **above `MapContext`'s 50** — copy `TaskListMapContext`'s shape verbatim. ❌ **No new key or pad button may be consumed**
- [x] Activate it **per frame from `OVT_MapLayersUI`, and only while the panel is open** — mirroring `SCR_UITaskManagerComponent`. It must **not** be active when the panel is closed, or it steals map navigation
- [x] Re-run `python3 .claude/skills/overthrow-ui-patterns/scripts/check-input-conflicts.py` → **exit 0**. ⚠️ Known blind spot: the checker cannot see the base game's 197 inline `ActionContext` actions, so a clean exit is necessary and **not sufficient** — reason about the priority ordering by hand as well
- [x] Gate: compile **exit 0 / 5994 files**; Fast **89**, All **127** — unchanged. ⚠️ `.conf` edits are invisible to all three; **Q-1 in Phase 7 is the only evidence this works**

---

## Phase 6 — Boundary audit, contract records, docs — `component-developer` (7/7 complete) ✅

- [x] Re-run `territory-overlay`'s **I-4 boundary greps** over every new and changed file and record the output — no `[RplProp]`, no `[RplRpc]`, no `Rpc(`, no `EPF_`, nothing in `OVT_PlayerCommsComponent`, no write to any campaign record
- [x] Confirm every new file sits under `Scripts/Game/UI/Map/`, `Scripts/Game/Data/`, `Scripts/Game/Global/` or `Scripts/Game/Tests/`
- [x] Write the **three new `OVT_MapLocationType` contract rows** (§3.7) into `docs/features/map/core/context.md` — `m_sCategoryName`, `GetCategoryName()`, and `m_bPlayerVisible`/`SetPlayerVisible`/`IsPlayerVisible` with its 🔴 _this is not campaign visibility_ note
- [x] Record the **"no rows added to the canvas-layer contract"** note (a contract that needed nothing added when its first consumer arrived is worth saying out loud) and the **`OVT_MapPlayerLocation` exception** (K5) beneath that table
- [x] Add the **layout ↔ code name table** (§3.6) to the same file, following the two tables already there
- [x] Findings **F1 – F6** written up in full (symptom, symbol, why not fixed here, suggested severity) for the user to file from **BUG-146** onward *(actually filed 2026-08-13 as BUG-149…155)* — **none fixed here**. Originally scoped as F1–F3: (vestigial `m_ToolMenuEntry`/`ZoomInOnPlayer`; 5 of 14 `m_sDisplayName` values are raw English literals; `OVT_MapPlayerLocation.Update`'s never-restored `SetOpacity(0)` and uncleared `m_Widgets`)
- [x] Gate: compile **exit 0**, Fast **89**, All **127** — all unchanged; this is a docs-and-audit phase and **any movement is a finding**. ❌ No `file:line` pointers in shipped code comments (epic K-9)

---

## Phase 7 — 🔴 Verification gate — **user-driven, no agent** (34/37 complete)

> ✅ **Discharged 2026-08-13 by the user's play-test session, all green**, with two specific observations
> transcribed below (A-on-row, D-pad Left) and one finding fixed the same day at epic level (marker hover
> hitbox — see the session note in `context.md`). ⚠️ **Scope honesty:** the SP, persistence, gamepad and
> Phase-4b rows below were confirmed as a block by the user ("all ticks on everything"), with only the two
> named observations transcribed individually. **The two-client MP rows (I-2 … I-4) were NOT part of the
> session and remain open.**

> ⚠️ **Warn the user before launching anything** — `tools/launch-game.sh` opens a window on their desktop
> and can orphan. Always pass `--timeout 3600`.
>
> **Steps 1–4 are automated and are ticked by the build; 5–8 need a human; 9 needs two clients.**
> **Phase 1's four spike questions P1 – P4 are folded in here** (rows at the end) because the autonomous
> run had nobody at the screen to answer them earlier.

**Automated (run at the end of the build)**

- [x] 1. `git status` — record the commit
- [x] 2. `tools/compile-check.sh` → exit 0, file count **5994**
- [x] 3. `tools/run-tests.sh "{6A6E29FF47ECB840}"` → OK, **89**
- [x] 4. `tools/run-tests.sh "{6A6E2A002F53A581}"` → OK, **127**

**Workbench + single-player pass (human)**

- [x] ✅ 5. **Workbench clean load** — no unresolved-resource or GUID errors. The **only** gate in the project that can see a dangling GUID in a new `.layout`/`.meta`. ⚠️ The orphaned `OVT_MapThreatGrid` `.meta` (`{B8F4C6A8C9D3E4F1}`) is **pre-existing, not this feature's**
- [x] ✅ **F-1** The tool-menu icon strip is on the left of the fullscreen map and carries an Overthrow entry with a filter glyph, above the ruler/compass/watch tools
- [x] ✅ **F-2** Clicking it opens a docked panel titled "Map Layers"; clicking again closes it. Opening the journal or task list closes the layers panel, and vice versa (K12 exclusivity)
- [x] ✅ **F-3** **Seventeen rows, no more and no fewer** — 14 location types (Towns, Bases, Radio Towers, FOBs, Ports, Camps, Houses, Shops, Gun Dealers, Warehouses, Bus Stops, Vehicles, Points of Interest, Waypoints) + Territory + Restricted Areas + Players. **No blank row, no duplicate, and no row for the disabled threat grid**
- [x] ✅ **F-4** Every location-type row shows that type's map icon and a plural name; overlay/player rows show a name and no icon. Nothing reads as a raw _editor_ string
- [x] ✅ **F-5** Every toggle works **within one frame** — territory shading, restriction rings and player markers all respond; turning a row back on returns it
- [x] ✅ **F-6** With an info panel **pinned**, toggle three unrelated rows: the pin survives, the panel stays open, no marker flickers or moves, the map does not re-centre (K14)
- [x] ✅ **F-7** 🔴 Hide **Towns** (5 s refresh) and **Vehicles** (2 s refresh), leave the map open **30 s** — neither reappears. This is the R8/BUG-136 interaction
- [x] ✅ **F-8** On a profile that has never opened the panel the map looks exactly as it did before this feature and every row reads on (D3)
- [x] ✅ **F-9** Hide Houses and Territory → close/reopen the map → still hidden → quit to the main menu, rejoin → still hidden → re-enable both, close/reopen → both back
- [x] ✅ **F-10** The panel sits beside the icon strip and never covers the centre of the map; markers outside its footprint still hover, pin and offer travel
- [x] ✅ **F-11** ⚠️ **After the user regenerates the six exports in Workbench**, all 18 new ids render as English with no `#OVT-` key visible. Raw keys **before** the regeneration are expected, not a defect, and were **not** worked around by hardcoding literals
- [x] ✅ 8. **Persistence pass** — F-9 including a full quit to the main menu **and** a full game relaunch; the preferences must still be there

**Gamepad pass (human, no mouse touched)**

- [x] ✅ **Q-1** 🔴 The full round trip: open the map → **D-pad Left** → tool menu focused → **D-pad Up/Down** → the entry → **A** → the panel opens **with focus already on the first row** → D-pad Up/Down walks every row **with a visible focus highlight** → **A** toggles and the map responds → D-pad Left returns to the strip → **A** closes the panel
- [x] ✅ **Q-2** With 17 rows the list is fully reachable on a 1080p screen, by pad **and** by mouse wheel

**Two-client MP pass (human — ⚠️ warn first; each client opens a window and can orphan)**

- [ ] **I-2** 🔴 Client A hides Houses and Territory; **B's map is unchanged.** B then hides Bases; **A's map is unchanged and A's houses/territory stay hidden.** Two profiles is also what proves preferences are **per profile**
- [ ] **I-3** **JIP** — B joins _after_ A has accumulated state and toggled filters. B's map is complete and unfiltered; A's filters persist across B's join
- [ ] **I-4** With every row on, the map is **indistinguishable from the pre-feature map** — markers, info panels, fast travel, bus travel, territory shading and restriction rings all behave as before
- [ ] **I-5** *(added 2026-08-16 — BUG-172/BUG-173 fix verification, player-reported MP-only defects fixed blind)* On a **client**: base markers exist for **both** factions, a captured base shows the resistance icon/colour and its real name, and fast travel to a captured base works. Camps: A places a camp while B is connected → both see it; A toggles it private → A keeps it, B loses it; A relogs → still on A's map (JIP)

**Phase 1's spike questions, folded in**

- [x] ✅ **P1** Does vanilla's tool menu actually exist on Overthrow's map? Record yes/no and which fallback (if any) was needed
- [x] ✅ **P2** Does the entry appear, in the right place, with a legible glyph? Confirm the sort position relative to journal/tasks and that **`"filters"` reads as a filter control at 64 px** (it is an `[Attribute]` — swapping it is a config change, K6)
- [x] ✅ **P3** Does the panel dock beside the icon strip rather than over the middle of the map?
- [x] ✅ **P4** ⚠️ **THE PLAN'S EXPECTATION HERE IS NOW WRONG BY CONSTRUCTION — do not test against it.** The plan predicted "the left stick still pans the map, expected yes", and called the opposite "the one finding that would force a redesign". Phase 4b's context (**D4**, user-approved) binds `MenuUp`/`MenuDown` to `left_thumb_vertical±` and `MenuLeft`/`MenuRight` to `left_thumb_horizontal±` at priority 70, **above** `MapPanVGamepad`/`MapPanHGamepad` at 50. So the left stick **will** walk and toggle rows and **will not** pan the map while the panel is open. That is the direct price of the approved fix, not a redesign trigger. **Record the actual behaviour either way**

**Phase 4b's own consequences — hand-derived from reading the base config, entirely untested (⚠️ the conflict checker is cross-context-blind, so none of this is machine-verified):**

- [x] ✅ 🔴 **`pad_left` is shadowed — this is the sharp edge.** `MapToolMenuFocus` (D-pad Left) is the **only** pad route to the tool strip, and while the panel is open the new context takes it. **D-pad Left will untick the focused row instead of returning to the strip.** `MenuLeft` was included deliberately: `SCR_ToolboxComponent` registers listeners for `MenuLeft`/`MenuRight` only, and with `m_bCycleMode` false, stepping YES→NO is **`MenuLeft` alone** — so **without it a pad could turn layers on but never off**, i.e. a filter panel that cannot filter. Vanilla accepts the identical shadow in `MapMarkerEditContext` → **OBSERVED 2026-08-13, and worse than predicted: D-pad Left unticks the focused row AND leaves the panel**, so every untick costs a re-entry into the panel. Recorded as **F7** below, filed as **BUG-155**
- [x] ✅ **Press A on a focused row.** Expected: **nothing** (the toolbox does not listen to `MenuSelect` without multiselect). ⚠️ **If A _does_ toggle, say so — that is the good outcome**: it would mean `MenuLeft` is droppable and `pad_left` reclaimable for returning to the strip → **OBSERVED 2026-08-13: A did nothing.** `MenuLeft` stays load-bearing and `pad_left` is not reclaimable this way — any fix for F7 has to come from somewhere else (e.g. a custom `MenuSelect` listener on the row component)
- [x] ✅ 🔴 **Press A on the tool entry and watch the first row** — if it toggles instantly, the one-frame grace delay (copied from `SCR_MapMarkersUI`, which needs it for the same reason: the click that opens the panel is **A**, and this context puts `MenuSelect` on **A**) is insufficient and needs a second frame
- [x] ✅ **B still closes the map** from inside the panel. `MenuBack` was **deliberately excluded** so `MapEscape` (`gamepad0:b`, priority 55) survives as the pad escape hatch
- [x] ✅ **Close the panel, then D-pad Left** — the tool strip must be reachable again **within one frame**. `ActivateContext`'s default duration is a **single-frame lease**, so not renewing it _is_ the entire teardown; this row is the proof it releases
- [x] ✅ **Panel closed: D-pad Up/Down/Left** — the map's compass/watch/pencil/protractor shortcuts and `MapToolMenuFocus` must all behave **exactly as before**. This is the proof the context does not leak
- [x] ✅ **Keyboard with the panel open** — W/A/S/D and the arrows walk/toggle rows, and **the character stops moving**. Consistent with all 20 other Overthrow menus, but **new for the map**. Confirm that reads as acceptable rather than as a bug
- [x] ✅ **If D-pad focus is dead entirely, the context is not going live — check `Flags`.** Shipped value is `Flags 0x26 0`, copied verbatim from vanilla's `TaskListMapContext`. First alternative: `Flags 4` (this mod's own norm for all 20 contexts; `OverthrowRespawnContext` proves it works over an open map). Second: `Flags 0xc 0` (`MapMarkerEditContext`, the structural twin)
- [x] ✅ **Failure mode to watch: panel open, focus lost, pad stranded.** The context activates on _panel visibility_, not on focus — so the actions stay live while nothing listens, and `pad_left` still will not reach the strip. One-line fix if seen: additionally require the focused widget to be inside the panel before activating
- [x] ✅ **R5 check** — open and close the map **three times** and count the tool-menu entries. More than one means the registration drifted out of `Init()` (K7)
- [x] ✅ 10. Record **every** observation in `context.md`, including the ones that passed

---

## Phase 8 — Help & documentation sync — `help-docs-sync` (4/4 complete) ✅

- [x] ✅ Tutorial popups (`Configs/Tutorials/`) — one sentence added to the existing `map-first-open` body (`#OVT-Tutorial_MapFirstOpen_Body`): the filter is on the tool strip and opens a panel of on/off rows. **No `.conf` change and no new popup.** ⚠️ **The "how to open it on the pad" half was deliberately CUT**: the pad walkthrough is unobserved, and Phase 4b established that `OverthrowMapLayersContext` shadows `MapToolMenuFocus`, so the plan's D-pad Left step is known wrong. A second `OVT_TutorialPage` was also rejected — `OVT_TutorialInfo.ShowEntry` only ever reads `m_aPages[0]`, and `m_bShowOverUI 1` hides the escalation button, so a page 2 on this entry would be unreachable content
- [x] ✅ Field Manual (`Configs/FieldManual/`) — new "Filtering the Map" header + 2 text pieces in the Map and Fast Travel entry, after the Territory section. Second paragraph states **per profile, not per campaign** explicitly
- [x] ✅ Public wiki — new player page `/map-filters` (pageId 60), cross-link section on `/territory` (pageId 59), index link on `/home` (pageId 1). All three re-read after writing to confirm the true state
- [x] ✅ **Every sentence backed by shipped behaviour** — symbol-level fact-check citations recorded in each new `.st` item's `Comment` (symbols only, no line numbers, per epic rule K-9). Sentences cut rather than guessed are listed in the Phase 8 session note

**Gates re-run at the phase boundary and all three unchanged**, which is the whole acceptance criterion for a content phase: compile **exit 0 / 5994 files / Game module / 5 s**, Fast **OK, 89 tests, 34 s**, All **OK, 127 tests, 40 s**.

**New `.st` ids — 3, GUIDs `{6A85D1E000000080}` – `{6A85D1E000000082}`** (the block's next free id is now `{6A85D1E000000083}`; `{…006B}`–`{…007F}` remain a permanent gap): `OVT-FieldManual_MapLayers_Head`, `OVT-FieldManual_MapLayers_Text`, `OVT-FieldManual_MapLayers_Text2`. Plus one **edited** existing id, `OVT-Tutorial_MapFirstOpen_Body`. ⚠️ These join the feature's existing **18** ids pending a Workbench export and render as raw `#OVT-` keys until then.

---

## Bugs & Issues

**Active Bugs:** **BUG-155** (F7 — D-pad Left unticks + exits the panel; found by the Phase 7 gamepad pass).

**Incidental findings — written up by Phase 6 (2026-08-11) and NOT fixed. ✅ All filed 2026-08-13 as BUG-149 … BUG-155** (BUG-146…148 were already taken by parallel sessions — the predicted "from BUG-146" was stale by filing time):

- [x] ✅ 🐛 **F1 → filed as BUG-149 (2026-08-13)** — `OVT_MapPlayerLocation` carries a vestigial `SCR_MapToolEntry m_ToolMenuEntry` that is **never assigned**, an empty `Init()`, and a `ZoomInOnPlayer()` with **zero callers** (grep-verified). The user should decide: delete, or wire it to a second tool-menu entry — it looks like it was meant to be one
- [x] ✅ 🐛 **F2 → filed as BUG-150 (2026-08-13)** — **5 of 14 `m_sDisplayName` values are raw English literals** (Town, Bus Stop, Vehicle, Point of Interest, Waypoint in `Configs/Map/OverthrowMap.conf`; ⚠️ the line numbers this row used to cite were already stale by Phase 6 — Phase 3's own 14 `m_sCategoryName` additions shifted them, which is epic rule K-9 demonstrating itself inside one feature in under a day) and render untranslated in the info panel **today**, independently of this feature
- [x] ✅ 🐛 **F3 → filed as BUG-151 (2026-08-13)** — `OVT_MapPlayerLocation.Update()` sets `SetOpacity(0)` for a player with no controlled entity and **never restores it**, and `m_Widgets` is not cleared on map close, so it can hold refs to destroyed widgets

**Found during the build (not in the plan) — also file in Phase 6:**

- [x] ✅ 🐛 **F4 → filed as BUG-152 (2026-08-13)** — `OVT_MapPlayerLocation.OnMapOpen` dereferences `OVT_Global.GetConfig().m_Difficulty.showPlayerOnMap` with **no null check on the config**. Found in Phase 3 and deliberately not fixed, under the same "leave the class otherwise exactly as-is" rule that preserves F1
- [x] ✅ 🐛 **F5 → filed as BUG-153 (2026-08-13)** — `Language/localization_Overthrow.st` carries **4 duplicate `CustomStringTableItem` GUIDs**: `{5D5C558A6E391A20}`, `{5D86A310C893DBDE}`, `{5D86A310C893DBDF}`, `{A1B2C3D4E5F60003}`. **Verified pre-existing at HEAD**, not introduced by this feature. Zero duplicate `Id`s, which is why nothing has broken yet
- [x] ✅ 🐛 **F6 → filed as BUG-154 (2026-08-13)** — 🔴 **Pre-existing cross-context input collision, found in Phase 4b.** `map/core`'s `OverthrowCloseInfoPanel` binds `gamepad0:b` and is added to `MapContext`, which vanilla declares at **Priority 50**; vanilla's `GadgetMapContext` sits at **Priority 55** and its `MapEscape` also binds `gamepad0:b`. Higher priority wins, so on a pad **B** very likely closes the _map_ and never reaches the info panel's close button — leaving that affordance keyboard-only. Invisible to `check-input-conflicts.py` because the collision is **cross-context** (and `MapEscape` is one of the base game's ~197 inline actions the checker cannot see). Confirm on a pad before designing a fix
- [x] ✅ 🐛 **F7 → filed as BUG-155 (2026-08-13)** — 🔴 **Found by the Phase 7 gamepad pass (2026-08-13): D-pad Left on a focused row unticks it AND leaves the panel**, so turning a layer off costs a full re-entry per row. Phase 4b predicted the untick-instead-of-return half (the `MenuLeft` shadow over `MapToolMenuFocus`); the panel-exit half was not predicted — both actions appear to fire despite the priority-70 context, or the toolbox's own `MenuLeft` handling moves focus out of the row list. The A-button probe closed the easy fix: **A does nothing on a focused row**, so `MenuLeft` cannot simply be dropped. A real fix likely needs a custom `MenuSelect` listener on `OVT_MapLayerRowComponent` (making A toggle), after which `MenuLeft` becomes droppable and D-pad Left returns to the strip cleanly

---

## Technical Debt

- [ ] 💳 **The `{6A85…}` GUID series is allocated by this feature** — record the exact GUIDs used so the next feature starts from a genuinely free series
- [ ] 💳 **`m_sToolMenuIcon` ships as a reused vanilla quad (`"filters"`)** — deliberate (K6, zero art dependency). A bespoke Overthrow glyph is a **config-only** change with no code change, and is not scheduled

---

## Task Status Legend

- [ ] Not started · [ ] 🔄 In progress · [ ] ⏸️ Blocked · [x] ✅ Completed · [x] ❌ Cancelled

---

_Update this file as tasks are completed. `context.md` is the authority for what was actually built where
this file and `implementation.md` disagree._
