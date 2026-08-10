# Map Legacy Retirement — Implementation Plan

**Epic:** map (feature 4 of 8 — last of the committed scope)
**Status:** Planning
**Started:** 2026-08-10
**Target Completion:** TBD
**Last Updated:** 2026-08-10

---

## 1. Executive Summary

This feature **deletes the old map**. `map/location-types` closed the content half of parity and removed
`OVT_MapIcons.RegisterPOI`'s last compile-level dependency; `map/fast-travel` closed the verb half, moved
travel server-side and re-pointed `OVT_CatchBusAction`. Both hard gates are satisfied, so the hand-rolled
`OVT_MapIcons` icon layer, the three flag-based modes inside `OVT_MapContext`, the two duplicated main-menu
entries that arm them, and four unvalidated fast-travel RPCs on the deprecated comms component all have no
remaining job.

Removing them is worth doing on its own terms. Every day both systems coexist is a day of double
maintenance, two sources of map truth, and a menu that offers the player two ways to do the same thing —
one of which is a live security hole (`RpcAsk_RequestFastTravel*`: any client teleports itself anywhere,
for free, with no validation).

**The approach is caller-first and compile-gated per phase.** Callers are cut before callees, so the tree
is shippable at every phase boundary and no intermediate state is broken. Phase 1 removes the main-menu
entries first — `OVT_MainMenuContext.c:218` is the *only* thing left that arms `m_bFastTravelActive`, so
after Phase 1 every legacy mode is provably unreachable before a single line of the map code is deleted.

**Net effect:** `OVT_MapContext` goes from 592 lines to under 80; `OVT_MapIcons.c` (846 lines), three
layouts and their `.meta` files, one config block, four RPCs, one string id and one broadcast preset go
away entirely. **No player-visible capability is removed** — everything the deleted code did is done by
`OVT_OverthrowMapUI` + `OVT_TravelRequestComponent` today.

This is a *removal* feature. If a gap surfaces during retirement, it goes back to `map/location-types` or
`map/fast-travel` — it is **not** fixed here.

---

## 2. Goals

**Primary**

1. Delete `Scripts/Game/UI/Map/OVT_MapIcons.c` and every asset that exists only to serve it.
2. Reduce `OVT_MapContext` to its four surviving map-gadget helpers.
3. Remove the two dead main-menu entries with **no replacement**.
4. Delete the four unvalidated `RequestFastTravel*` RPCs from `OVT_PlayerCommsComponent` (security fix).
5. Leave nothing dangling in the four compiler-invisible file classes: `.layout`, `.conf`, `.et`, `.st`.
6. Archive `docs/features/towns/map-info` and stop the towns epic claiming to own the map.

**Secondary**

7. Discharge `map/location-types`' outstanding Phase 7 gate in the same play-test session (§4 P7).
8. Correct the historical `OVT_MapIcons.c:<line>` and `OVT_MapContext.c:<line>` pointers scattered through
   surviving doc comments, which become references to files/lines that no longer exist.
9. Reconcile the in-game Field Manual, which currently documents both deleted menu entries (§3.4).

**Explicit non-goals:** any new map capability; refactoring the surviving `OVT_MapContext` beyond the
strip; touching the canvas-layer modules; reworking the main menu beyond the two rows; deleting
`docs/archive/OverthrowMapSystem.md`.

---

## 3. Architecture Overview

### 3.1 End state

```
                       ┌──────────────────────────────────────────┐
   OVT_CatchBusAction ─┤ OVT_MapContext  (~75 lines, was 592)     │
   (unchanged)         │   GetMap()  ShowMap()  HideMap()         │
                       │   OpenMap()   ← opens map, arms NOTHING  │
                       └──────────────────────────────────────────┘
                                          │ opens
                                          ▼
   Configs/Map/MapFullscreen.conf  (same-GUID DELTA over vanilla)
     m_aModules                          m_aUIComponents
       OVT_MapRestrictedAreas  KEEP        OVT_MapPlayerLocation   KEEP
       OVT_MapThreatGrid       KEEP        OVT_MapIcons            ✂ DELETE
         (shipped disabled)                OVT_OverthrowMapUI      KEEP
     m_DescriptorDefaultsConfig  KEEP  (bus-stop dupe suppression)
                                          │
                                          ▼
                       ┌──────────────────────────────────────────┐
                       │ OVT_OverthrowMapUI — the ONE map surface │
                       │  markers (SCR_MapUIElement) · info panels│
                       │  travel button → OVT_TravelRequestComp.  │
                       └──────────────────────────────────────────┘
```

After this feature there is exactly **one** map surface, **one** travel path (server-authoritative,
through `OVT_OverthrowController`), and **one** place a location's data is rendered.

### 3.2 What `OVT_MapContext` becomes

It stops being a *map* class and becomes a thin **map-gadget helper** for the player character: find the
map gadget, raise it, stow it. Nothing about markers, modes, clicks, towns, travel or input remains.

| Retained | Why |
|---|---|
| `GetMap()` | Resolves the map gadget; used by the other three |
| `ShowMap()` | Used by `OpenMap()` |
| `OpenMap()` | `OVT_CatchBusAction`'s entry point — the one live external caller |
| `HideMap()` | **Zero callers after the strip — retained deliberately, see §5 K-3** |

Everything else in the file is deleted: the three mode flags, the three `Enable*`/`Disable*` pairs,
`ShowTownInfo`, `CanFastTravel` (the duplicate rule set), `MapClick`, `MapExit`, `OnMapExit`,
`IsOverthrowInfoPanelVisible`, the `RegisterInputs`/`UnregisterInputs` overrides, `PostInit`, the three
distance constants, the four cached manager members, `m_SelectedTown`, and the three layout/colour
attributes.

Removing the `RegisterInputs`/`UnregisterInputs` overrides is safe: `OVT_UIContext`'s base versions guard
on `m_sOpenAction != ""` (`OVT_UIContext.c:69,82`) and the prefab block does not set it, so the base does
nothing. Removing `PostInit` reverts to the base's. Removing `m_Layout` from the prefab is safe because
`ShowLayout()` is never reached on this context and guards on `!m_Layout` anyway (`:116`).

### 3.3 Retained by explicit decision

- **All three canvas-layer modules.** `OVT_MapRestrictedAreas` (live — draws the FOB-deploy restriction
  rings `resistance/fob` enforces; **BUG-070's fix must not regress**), `OVT_MapThreatGrid` (shipped
  disabled, `map/territory-overlay` decides its fate), `OVT_MapPlayerLocation` (live). These are
  `SCR_MapConfig.m_aModules` / `m_aUIComponents` entries on a *different rendering path* from the marker
  widgets. This feature must not touch them beyond confirming they still draw.
- **`UI/Layouts/Map/MapPlayerLocation.layout`** — still bound at `MapFullscreen.conf:12`.
- **`m_DescriptorDefaultsConfig`** at `MapFullscreen.conf:34-43` — this is `map/location-types`' bus-stop
  duplicate-icon suppression. Nothing to do with `OVT_MapIcons`; it stays.
- **The `OVT_MapContext` block** in `Character_Player.et:37-44`, minus four attributes.

### 3.4 Two findings this plan adds (not in the requirements or the hand-off)

These were found while verifying the working tree and both are real, player-visible regressions if missed.

> **🔴 FINDING A — deleting the "Map Info" block removes the main menu's only initial-focus call.**
> `OVT_MainMenuContext.c:108` (`GetGame().GetWorkspace().SetFocusedWidget(comp.GetRootWidget())`) sits
> *inside* the `if (comp)` block for "Map Info", and it is the **only** `SetFocusedWidget` call in the
> whole context (grep-verified: the only others are in `OVT_VehicleMenuContext`). Delete the block naively
> and the Overthrow main menu opens with **no focused widget** — a controller/console user has nothing to
> navigate from. The call must be relocated to the new first entry, **"Place"**. This is *preserving*
> existing behaviour, so it is in scope for a deletion feature, and it is exactly the class of defect only
> the P7 gamepad gate would catch.

> **🟡 FINDING B — the in-game Field Manual documents both deleted entries.**
> `Configs/FieldManual/FieldManualConfigRoot.conf:19-30` holds two header/text pairs whose headers reuse
> the two menu ids. Their bodies describe the deleted workflow verbatim — `#OVT-FieldManual_MapInfo_Text`
> says *"Clicking on this in the main menu will open the map, then you can click anywhere on the map to
> show info about that location"*, and `#OVT-FieldManual_FastTravel_Text` says *"…you can click on any
> owned house, camp, FOB or base … to instantly travel there."* Both become **false** the moment Phase 1
> lands. Handled in P6 via `help-docs-sync` (§4 P6.4).

**Consequence for the `.st` master:** `#OVT-MainMenu_MapInfo` and `#OVT-MainMenu_FastTravel` are **both
retained**. `MainMenu_MapInfo` is consumed by `FieldManualConfigRoot.conf:20`; `MainMenu_FastTravel` by
`FieldManualConfigRoot.conf:26`, `OVT_OverthrowMapUI.c:929` (the travel verb label) and
`UI/Layouts/Map/Core/OVT_MapInfoPanel.layout:198`. The brief's suspicion about `MainMenu_FastTravel` was
right and also applies to `MainMenu_MapInfo`.

---

## 4. Implementation Phases

Seven phases. **Gate after every one:** `tools/compile-check.sh` (exit 0) **and**
`tools/run-tests.sh "{6A6E29FF47ECB840}"` (Fast). Run `tools/run-tests.sh "{6A6E2A002F53A581}"` (All)
before sign-off. **Commit at each phase boundary** — each phase is independently revertable via git.

> **Test counts must not move.** This feature adds no test and deletes no covered behaviour. Capture the
> baseline (expected **Fast 44 / All 79**) before starting P1; if a count changes at any gate, that is a
> **finding to investigate**, not a number to update.

> **Never delete by line range.** Every line number in this plan is a pointer for a human to look at, not a
> `sed` argument. Locate each item by its **symbol name** or its **`Id` string**, confirm the block
> boundaries by eye, and delete whole syntactic units. The hand-off list has already been wrong once, in a
> way that would have silently corrupted the `.st` master.

---

### Phase 0 — Baseline (XS · 10 min · no agent)

- **P0.1** Confirm a clean `git status` on `new-map`; note the HEAD sha for the revert path.
- **P0.2** Run `tools/compile-check.sh` → record exit code and file count.
- **P0.3** Run both test groups → record **Fast** and **All** case counts. These are the invariants.

**Acceptance:** compile exit 0; both groups exit 0; counts recorded in `context.md`.

---

### Phase 1 — Main-menu entries (S · 30 min · standard agent)

Nothing arms a legacy mode after this phase. Do it first.

- **P1.1** `Scripts/Game/UI/Context/OVT_MainMenuContext.c` — in `OnShow()`, delete the whole
  `// Map Info` block (locate by `GetButtonText("Map Info", m_wRoot)`) **and** the whole
  `// Fast Travel` block (locate by `GetButtonText("Fast Travel", m_wRoot)`).
- **P1.2** 🔴 **FINDING A** — before deleting, move
  `GetGame().GetWorkspace().SetFocusedWidget(comp.GetRootWidget());` into the surviving `// Place` block,
  immediately before `comp.m_OnClicked.Insert(Place);`. Add a `//!` note that this is the menu's initial
  gamepad focus.
- **P1.3** Delete the `MapInfo()` method (locate by `private void MapInfo()`) and the `FastTravel()` method
  (locate by `private void FastTravel()`).
- **P1.4** `UI/Layouts/Menu/MainMenu.layout` — delete the two whole `ButtonWidgetClass` units, located by
  their `Name "Map Info"` (GUID `{598AB670C1F2839C}`) and `Name "Fast Travel"` (GUID
  `{598AB6734AADE219}`) lines. Each is a ~40-line block ending after its nested `SizeLayoutWidgetClass`;
  the `m_sText` lines the requirements named are *inside* these blocks, not the units to remove.
- **P1.5** Do **not** touch `Language/localization_Overthrow.st` — both ids are still consumed (§3.4).
- **P1.6** Do **not** add a "Map" entry (settled decision D-2).

**Acceptance criteria**

- `grep -n "MapInfo\|FastTravel" Scripts/Game/UI/Context/OVT_MainMenuContext.c` returns nothing.
- `grep -c "SetFocusedWidget" Scripts/Game/UI/Context/OVT_MainMenuContext.c` returns `1`, and the
  surviving call is inside the `Place` block.
- `grep -n "OVT_MapContext" Scripts/Game/UI/Context/OVT_MainMenuContext.c` returns nothing.
- `grep -n 'Name "Map Info"\|Name "Fast Travel"' UI/Layouts/Menu/MainMenu.layout` returns nothing, and the
  file's brace balance is unchanged from a structural read.
- Compile 0; Fast green at the baseline count.

---

### Phase 2 — `OVT_MapContext` strip (M · 2–3 h · **advanced agent**)

> **Advanced (max-effort) dev agent.** 592 → <80 lines in a class instantiated from a prefab, whose base
> class contract, input registration and notification path all have to be reasoned about rather than
> pattern-matched. The failure mode is a silent one: a surviving reference in a `.et` attribute the
> compiler never reads.

- **P2.1** Delete, by symbol: `m_bMapInfoActive`, `m_bFastTravelActive`, `m_bBusTravelActive`;
  `MAX_HOUSE_TRAVEL_DIS`, `MAX_FOB_TRAVEL_DIS`, `RECRUIT_TRAVEL_RADIUS`; `m_TownManager`, `m_RealEstate`,
  `m_Resistance`, `m_OccupyingFaction`, `m_SelectedTown`; the `m_ModLayout`,
  `m_NegativeModifierColor`, `m_PositiveModifierColor` attributes.
- **P2.2** Delete, by symbol: `PostInit()` (override), `CanFastTravel()`, `EnableMapInfo()`,
  `ShowTownInfo()`, `EnableFastTravel()`, `EnableBusTravel()`, `OnMapExit()`, `DisableMapInfo()`,
  `DisableFastTravel()`, `DisableBusTravel()`, `RegisterInputs()` (override), `UnregisterInputs()`
  (override), `MapExit()`, `IsOverthrowInfoPanelVisible()`, `MapClick()`.
- **P2.3** **Retain verbatim:** `GetMap()`, `ShowMap()`, `HideMap()` (including its full `//!` block —
  it documents the `ToggleFocused`-then-stow ordering, the dead check and the held-gadget guard) and
  `OpenMap()`.
- **P2.4** Rewrite `OpenMap()`'s doc comment: it currently forward-references `EnableBusTravel` "below",
  which will not exist. Keep the substance (opens the map, arms nothing, bus eligibility is re-derived
  per panel build).
- **P2.5** 🟡 Add a `//!` block above `HideMap()` recording that it has **no callers today** and is
  retained as public API for `map/respawn` — see §5 K-3. Without this, the next reader deletes it.
- **P2.6** Add a class-level `//!` comment: this is a map-*gadget* helper, not a map UI class; the map UI
  is `OVT_OverthrowMapUI`.
- **P2.7** `Scripts/Game/UI/Map/OVT_OverthrowMapUI.c` — delete `IsInfoPanelVisible()` (at `:784`, doc
  comment from `:772`). Its sole caller was `IsOverthrowInfoPanelVisible`, deleted in P2.2. The doc
  comment says so explicitly. **Also** delete the related comment at `:774` that describes the legacy
  second listener, and correct the recruit-default rationale comment at `:50` which cites
  `OVT_MapContext.c:441`.
- **P2.8** Correct the stale `OVT_MapContext.c:<line>` prose pointers in
  `Scripts/Game/Services/OVT_FastTravelService.c:50,54` and
  `Scripts/Game/UI/Map/LocationTypes/OVT_MapLocationBusStop.c:43` — keep the *rationale*, drop the
  now-unresolvable line numbers.
- **P2.9** Correct `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_InitSuite.c:2004,2032,2153,2156` — these
  say "the lookup `OVT_MapContext` makes for bus travel" and that bus travel answers `"NeedBusStop"`, both
  of which stop being true. **Comments only — change no assertion, so the count must not move.**

**Acceptance criteria**

- `Scripts/Game/UI/Context/OVT_MapContext.c` is under 80 lines and contains exactly four methods.
- `grep -n "m_b.*Active\|MapClick\|ShowTownInfo\|CanFastTravel\|EnableMapInfo\|EnableFastTravel\|EnableBusTravel\|Disable" Scripts/Game/UI/Context/OVT_MapContext.c` returns nothing.
- `grep -rn "GetOnMapClose" --include=*.c Scripts/` returns **nothing** — the whole subscription is gone
  with `RegisterInputs`/`UnregisterInputs`, which structurally closes BUG-069 part 4 (already fixed at
  `:369-370`/`:382`; no other Overthrow context uses the pattern — verified).
- `grep -rn "IsInfoPanelVisible" --include=*.c Scripts/` returns nothing.
- Compile 0; Fast green at the baseline count.

---

### Phase 3 — Leaf assets (M · 2–3 h · **advanced agent**)

> **Advanced (max-effort) dev agent.** This phase touches **all four compiler-invisible file classes** in
> one pass — `.c`, `.conf`, `.layout` (+`.meta`) and `.et`. Compile 0 and green tests say *nothing* about
> whether a deleted GUID broke a prefab. Every deletion here must be grep-proven dead first.

- **P3.1** Delete `Scripts/Game/UI/Map/OVT_MapIcons.c` (846 lines). Grep-proven: its `static RegisterPOI`
  (`:46`) has **zero live callers** — only the definition plus prose in `OVT_MapMarkerComponent.c:20`.
- **P3.2** `Configs/Map/MapFullscreen.conf` — delete the whole `OVT_MapIcons "{5994FB72BE0F9051}" { … }`
  block from `m_aUIComponents` (currently `:14-24`). ⚠️ This file is a **same-GUID delta over vanilla's**,
  so removing the block changes what merges with vanilla rather than replacing a file. **Retain**
  `OVT_MapPlayerLocation`, `OVT_OverthrowMapUI`, both `m_aModules` entries and the whole
  `m_DescriptorDefaultsConfig` block.
- **P3.3** Delete `UI/Layouts/Map/MapIcon.layout` + `.meta` — GUID `{F5E0CFFFC9F27B19}`, referenced
  **only** by `MapFullscreen.conf:17`, removed in P3.2.
- **P3.4** Delete `UI/Layouts/Map/MapInfo.layout` + `.meta` — GUID `{0EC60966C99CE954}`, referenced
  **only** by `Character_Player.et:38`, removed in P3.6.
- **P3.5** Delete `UI/Layouts/Map/MapInfo/Modifier.layout` + `.meta` — GUID `{7BAC7637E5744768}`,
  referenced **only** by `Character_Player.et:41` and 8 sites *inside* `MapInfo.layout` (deleted in P3.4).
  Remove the now-empty `UI/Layouts/Map/MapInfo/` directory.
- **P3.6** `Prefabs/Characters/Factions/INDFOR/FIA/Character_Player.et` — inside the
  `OVT_MapContext "{598E83B6A7175CBE}" { … }` block, delete the `m_Layout`, `m_ModLayout`,
  `m_NegativeModifierColor` and `m_PositiveModifierColor` lines. **Keep the block itself** —
  `OVT_CatchBusAction` resolves the context by type. `m_bOpenActionCloses`/`m_bHideHUDOnShow` may stay.
- **P3.7** De-reference the nine surviving `OVT_MapIcons.c:<line>` provenance pointers so they no longer
  cite a deleted file: `OVT_MapMarkerComponent.c:20`, `OVT_JobManagerComponent.c:75`,
  `OVT_MapLocationHouse.c:4,118`, `OVT_MapLocationVehicle.c:5,130`, `OVT_MapLocationWaypoint.c:14,18`,
  `OVT_MapLocationWarehouse.c:5`. **Keep the rationale** (these explain *why* the new types behave as they
  do — that is genuine value); replace "`OVT_MapIcons.c:472`" with "the legacy `OVT_MapIcons` layer
  (deleted in `map/legacy-retirement`)".

**Acceptance criteria**

- `grep -rn "OVT_MapIcons" --include=*.c --include=*.conf --include=*.et --include=*.layout .` returns
  exactly the **nine prose mentions in six files** listed in P3.7 — no config entry, no class definition,
  no `file:line` pointer.
- `grep -rn "F5E0CFFFC9F27B19\|0EC60966C99CE954\|7BAC7637E5744768" . --include=*.c --include=*.conf --include=*.et --include=*.layout --include=*.meta` returns **nothing** outside `docs/`.
- `grep -rn "RegisterPOI" --include=*.c Scripts/` returns at most prose.
- `MapFullscreen.conf` still contains `OVT_MapRestrictedAreas`, `OVT_MapThreatGrid`,
  `OVT_MapPlayerLocation`, `OVT_OverthrowMapUI` and `m_DescriptorDefaultsConfig`.
- Compile 0; Fast green at the baseline count.
- ⚠️ **The real gate for this phase is P7** — open the world in the Workbench and confirm zero
  missing-resource errors.

---

### Phase 4 — Delete the fast-travel RPCs (S · 45 min · standard agent)

The highest-priority item on the hand-off list. These are unreachable after P1 but still **registered RPCs
on a live component**, and they perform `ResolveSenderPlayerId` + `TeleportPlayer` with no validation and
no payment.

- **P4.1** `Scripts/Game/Components/Player/OVT_PlayerCommsComponent.c` — delete, by symbol:
  `RequestFastTravel`, `RpcAsk_RequestFastTravel`, `RequestFastTravelWithRecruits`,
  `RpcAsk_RequestFastTravelWithRecruits` (currently `:1483-1548`).
- **P4.2** Confirm nothing is lost: the recruit ring-placement loop inside
  `RpcAsk_RequestFastTravelWithRecruits` (`:1529-1547`) is already copied verbatim into
  `OVT_TravelRequestComponent.TeleportRecruits` (`:298-321`).
- **P4.3** Leave the two prose mentions in `OVT_TravelRequestComponent.c:9,291` — they are the migration
  record. Confirm they read correctly once the originals are gone.
- **P4.4** Add nothing. ❌ **Never add a new client→server RPC to `OVT_PlayerCommsComponent`** — this
  phase only deletes.

**Acceptance criteria**

- `grep -rn "RequestFastTravel" --include=*.c Scripts/` returns only the two doc-comment mentions in
  `OVT_TravelRequestComponent.c`.
- `OVT_PlayerCommsComponent.c`'s remaining RPCs are untouched — verify by diffing that no other
  `RpcAsk_`/`RpcDo_` symbol changed.
- Compile 0; Fast green at the baseline count.
- ⚠️ The `Rpc()`-arity compile blind spot does **not** apply to a pure deletion, but exit 0 still does not
  prove the scripts load — read `run-tests.sh`'s verdict, not just the compiler's.

---

### Phase 5 — `OVT-NeedBusStop` and the orphan sweep (S · 1 h · standard agent, high care)

- **P5.1** `Language/localization_Overthrow.st` — locate `Id "OVT-NeedBusStop"` and delete the **whole
  enclosing `CustomStringTableItem "{…}" { … }` block**, opening brace to matching close (currently
  ~`:4353-4374`, ~18 lines holding six `Target_*` translations).
  🔴 **Confirm the block boundaries by eye. An earlier draft of the hand-off named a 4-line range inside
  this 18-line block; acting on it would have corrupted the master silently.**
- **P5.2** `Configs/overthrowBroadcastMessages.conf` — locate `m_sTag "NeedBusStop"` and delete the
  **whole enclosing `SCR_SimpleMessagePreset "{…}" { … }` block**, including its nested `m_UIInfo`
  (currently ~`:396-403`).
- **P5.3** ❌ **Never edit `Language/localization_Overthrow.<lang>.conf`.** Those six files are
  Workbench-generated exports; their `Ids{}`/`Texts{}` blocks are neither parallel nor same-length and
  hand-editing corrupted all six once. This feature removes the id from the `.st` master **only**, and
  lists the change for the user to regenerate.
- **P5.4** ❌ **Do not delete `#OVT-NotAtBusStop`** — a new, live id from `map/fast-travel`, consumed by
  `OVT_FastTravelService.c:182`.
- **P5.5 — Orphan sweep, default KEEP.** Every notification id raised from a deleted `MapClick` branch was
  audited. **Result: all of them stay.** Recorded here so the sweep is not re-run and nothing is cut by
  mistake:

  | Id | Verdict | Live consumer after this feature |
  |---|---|---|
  | `MustHaveMap` | **KEEP** | The **retained** `OVT_MapContext.OpenMap()` still raises it |
  | `CannotFastTravelThere` | **KEEP** | `OVT_FastTravelService.c:187,360` + 4 location types |
  | `CannotFastTravelDistance` | **KEEP** | `OVT_FastTravelService.ReasonKeyFor` (`:175`) |
  | `CannotFastTravelWanted` | **KEEP** | `ReasonKeyFor` (`:176`) |
  | `CannotFastTravelDuringQRF` | **KEEP** | `ReasonKeyFor` (`:177`) |
  | `CannotFastTravelToQRF` | **KEEP** | `ReasonKeyFor` (`:178`) |
  | `MustBeDriver` | **KEEP** | `ReasonKeyFor` (`:180`) |
  | `MustExitVehicle` | **KEEP** | `ReasonKeyFor` (`:181`) |
  | `CannotAfford` | **KEEP** | 14 live call sites across build, place, shop, resistance and travel |
  | `MainMenu_MapInfo` | **KEEP** | `FieldManualConfigRoot.conf:20` (§3.4) |
  | `MainMenu_FastTravel` | **KEEP** | Field Manual + `OVT_OverthrowMapUI.c:929` + `OVT_MapInfoPanel.layout:198` |
  | `NeedBusStop` | **DELETE** | Only consumer was the deleted bus branch |

**Acceptance criteria**

- `grep -rn "NeedBusStop" --include=*.st --include=*.conf --include=*.c . | grep -v "^./Language/localization_Overthrow\..*\.conf"` returns only the two `OVT_TEST_InitSuite.c` prose mentions corrected in P2.9 — no `.st` item, no broadcast preset.
- `Language/localization_Overthrow.st` still parses: brace count balanced, and every other `Id "OVT-…"`
  from the P5.5 table still present.
- The six `localization_Overthrow.<lang>.conf` files are **unmodified** (`git diff --stat` shows none).
- Compile 0; Fast green at the baseline count.

---

### Phase 6 — Documentation (S · 1 h · standard agent + `help-docs-sync`)

- **P6.1** Move `docs/features/towns/map-info/{context.md,implementation.md,tasks.md}` into `docs/archive/`
  (prefix them `towns-map-info-*.md` to avoid collisions), leaving a short pointer note that the successor
  is the `map` epic and that the legacy system was deleted by `map/legacy-retirement` on this date.
  `docs/archive/` already exists; **`docs/archive/OverthrowMapSystem.md` stays untouched.**
- **P6.2** `docs/features/towns/epic-overview.md` — remove the `map-info` row from the Features table
  (`:27`), the build-order entry (`:39`), the dependency line (`:45`), and drop "and the map UI (town info
  panel, icons, overlays, fast/bus travel)" from the Purpose paragraph (`:13`). Retarget the four Tech
  Debt bullets that name `map-info` (`:69,71,75,76`) at the `map` epic, noting BUG-067/068/069 are now
  **structurally impossible** and BUG-070 concerns `OVT_MapRestrictedAreas`, which is **retained**.
- **P6.3** `docs/overview.md` — update the towns row (`:22`) from "5/5 retrospective" to 4 features and
  drop `map-info` from the feature list; update the map row to reflect retirement complete.
- **P6.4** 🟡 **FINDING B — run `help-docs-sync`** scoped to: the two Field Manual header/text pairs at
  `Configs/FieldManual/FieldManualConfigRoot.conf:19-30`, plus the bodies of
  `#OVT-FieldManual_MapInfo_Text` and `#OVT-FieldManual_FastTravel_Text` in the `.st` master. Both
  describe the deleted click-anywhere workflow and are false after P1. Either remove the pairs or rewrite
  them to describe the current map. **Fact-check every replacement sentence against a `file:line`** — two
  tutorial tips have already shipped invented mechanics, and no gate catches a well-formed lie.
- **P6.5** `docs/features/map/epic-overview.md` — mark feature 4 complete, refresh the rollup, and move
  the "Legacy static is a hard dependency" tech-debt bullet to resolved.
- **P6.6** Write `docs/features/map/legacy-retirement/context.md` recording the baseline counts, the two
  new findings, and the P5.5 orphan-sweep table.

**Acceptance criteria**

- `docs/features/towns/map-info/` no longer exists; its three files are in `docs/archive/`.
- `grep -rn "map-info" docs/features/towns/ docs/overview.md` returns only the archive pointer.
- `grep -rn "FieldManual_MapInfo_Text\|FieldManual_FastTravel_Text" Configs/ Language/localization_Overthrow.st`
  is either empty or points at rewritten, fact-checked text.
- Compile 0; Fast green; **All** green at the baseline count.

---

### Phase 7 — Combined verification gate (L · 1–2 h · **user-driven, NO agent**)

> **This phase discharges two features at once.** `map/location-types` Phase 7 (V-3 … V-7) is still
> outstanding and is the epic's stated hard gate. The user has chosen to implement the deletions against
> the already-proven *code-level* parity and run **one** combined session. This phase's checklist folds in
> location-types' outstanding items.
>
> ⚠️ **Client launches open a real window on the user's desktop and can orphan.** Warn before launching.
> **Always pass a long `--timeout`** — the default is 600 s and will kill the client mid-test.
>
> 🔁 **Revert path:** every phase is a separate commit. If this gate finds a gap, `git revert` the offending
> phase — the deletions do not have to be re-derived, and the gap goes back to `map/location-types` or
> `map/fast-travel` per §2's non-goals.

**7a — Workbench load (this feature's own gate; catches P3's invisible risk)**

1. Open the project in the Workbench. Confirm **zero missing-resource / missing-GUID errors** in the log.
2. Open `Prefabs/Characters/Factions/INDFOR/FIA/Character_Player.et` and confirm the `OVT_MapContext`
   block shows no broken/red attribute rows.
3. Open `Configs/Map/MapFullscreen.conf` and confirm four UI components and two modules remain.

**7b — Single-player sweep (this feature + location-types V-3, V-4)**

4. Open the Overthrow main menu: **no "Map Info" row, no "Fast Travel" row**, and the highlight starts on
   **"Place"** (FINDING A).
5. Open the Field Manual → Overthrow → Introduction: entries match reality (FINDING B).
6. `map/location-types` **V-3** — start a campaign, buy a house, buy a vehicle, place a camp, deploy a FOB,
   build a maintenance ramp, accept a job with "show on map", mark a recruit; open the map and walk the
   full marker checklist.
7. `map/location-types` **V-4** — zoom sweep from max out to max in; every type appears at a sensible zoom,
   no illegible name overlap.
8. Walk to a bus stop, use **Catch Bus**: the map opens and the bus-stop panel offers a **priced** trip.
   Take it; confirm you arrive and are charged **once**.
9. Fast travel from a location info panel: confirm it works and charges **once**.
10. Confirm the FOB **restriction rings still draw**, at the radii the deploy check enforces (BUG-070).
11. Confirm the player-location marker still draws.
12. Confirm the map **item is stowed** on close, not left in hand.

**7c — Two-client MP/JIP gate (location-types V-5 + this feature's MP gate) — highest risk, do not skip**

13. `tools/launch-server.sh`
14. Client A: `tools/launch-game.sh --timeout 3600 --profile OverthrowClient1 --allow-concurrent -- -client 127.0.0.1:2001`
15. As A: buy a house, buy a vehicle, place a **private** camp, deploy a FOB, note a shop's caret rows.
16. Client B (**JIP — join after step 15**):
    `tools/launch-game.sh --timeout 3600 --profile OverthrowClient2 --allow-concurrent -- -client 127.0.0.1:2001`
17. On B, confirm: the shared marker set matches A's; **B does not see A's house, vehicle or private camp**;
    B **does** see A's warehouse and FOB; the FOB and shop panels populate identically.
18. On B, buy a house and a vehicle — they appear for B and **not** for A.
19. **Both clients fast-travel concurrently.** Each arrives at its own destination and is charged once.
    (This is `map/fast-travel` Phase 0's uncovered case.)
20. As B (JIP), fast travel: the server path executes for a client that joined mid-campaign.
21. Neither client's log shows a script error on map open, travel, or bus trip.

**7d — Gamepad/console gate (location-types V-6 + FINDING A)**

22. **Controller only, no mouse.** Open the Overthrow main menu: something is focused immediately and
    D-pad navigation works from the first press (FINDING A — the specific regression).
23. Move the map cursor onto each marker type; every info panel appears and is readable at 1080p.
24. The fast-travel button is reachable on every type that offers it; the recruit toggle is reachable via
    the map cursor (its only gamepad path).

**7e — Save compatibility (location-types V-7)**

25. Load a save created **before** these changes. The map opens, all markers render, no errors.
26. Save, reload, confirm identical. Expected: no difference — nothing this feature deleted was persisted.

**Acceptance criteria:** every box above ticked, or a filed BUG with an owning feature. The gate is not
discharged by "compile clean and tests green" — those cover none of 7a–7e.

---

## 5. Key Technical Decisions

**K-1 — Caller-first ordering, gated per phase.**
Cut the callers before the callees. `OVT_MainMenuContext.c:218` is the **only** remaining thing that arms
`m_bFastTravelActive`; removing it in P1 makes every legacy mode *provably* unreachable before one line of
map code is deleted. This means no intermediate state where a reachable code path calls a deleted symbol,
each phase is independently revertable, and the tree is shippable at every boundary. The alternative — one
big-bang deletion — would be faster to write and impossible to bisect.

**K-2 — `OVT_MapContext` is slimmed, not deleted** *(settled decision D-1)*.
`OVT_CatchBusAction` resolves it by type and calls `OpenMap()`; deleting the class would mean rewriting a
working world action and its prefab binding for no gain. The four survivors are genuinely useful map-gadget
helpers with no legacy coupling. Requirements §"Out of Scope" explicitly permits either outcome.

**K-3 — `HideMap()` is retained despite having zero callers.** *(verified, not assumed)*
All eight in-file callers (`MapExit` ×1, `MapClick` ×7) are deleted by P2, and grep confirms no external
caller: `OVT_OverthrowMapUI` has its **own private** `HideMap()` at `:1327` (called from `:1226`) and its
own `GetMap()` at `:1291` — neither touches this one. So after P2 it is public API with no consumers.
Retained anyway, per D-1, because: (a) it carries the hard-won fast-travel-era fix — `ToggleFocused(false)`
*then* stow, with the dead check and held-gadget guard — which was found by play-test, not by any gate, and
which would be re-derived incorrectly if lost; and (b) `map/respawn` (feature 5) needs exactly this
close-and-stow behaviour. This is a deliberate, documented exception to "delete what is dead", and P2.5
requires a `//!` comment saying so — otherwise the next reader cuts it as an orphan.

**K-4 — Both main-menu entries go with no replacement** *(settled decision D-2)*.
The map is opened by the vanilla map gadget (`GadgetMap`, `KC_M`/`gamepad0:view`) and by
`OVT_CatchBusAction`. A "Map" menu row would be a third way to do the same thing — precisely the
duplication this feature exists to remove. The two `.st` ids survive anyway because the Field Manual and
the map's own travel button consume them (§3.4).

**K-5 — The comms-component RPCs are deleted as a security fix, not a cleanup** *(settled decision D-3)*.
`RpcAsk_RequestFastTravel*` do `ResolveSenderPlayerId` + `TeleportPlayer` with no validation and no
payment. They are unreachable after P1 but remain **registered RPCs on a live component**, so the wire
surface exists regardless of whether any shipped UI calls it. Nothing is lost: the recruit ring-placement
loop is already duplicated verbatim in `OVT_TravelRequestComponent.TeleportRecruits`.

**K-6 — Build now against code-level parity; one combined play-test gate** *(settled decision D-4)*.
`map/location-types` Phase 7 is outstanding and is the epic's hard gate. Rather than block, the deletions
are implemented against the already-proven code-level parity and verified in **one** session that
discharges both features. Justified because each phase is a separate commit and `git revert` is the whole
remediation. The risk this accepts is explicitly recorded as R-1 below.

**K-7 — Default to KEEP on every string and preset; grep-prove death before cutting.**
The P5.5 sweep started from nine candidate ids and deleted **one**. `MustHaveMap` is the instructive case:
it looks like a fast-travel-mode string, but its surviving call site is inside the **retained**
`OpenMap()`. Deleting a string the live path still shows is a silent, player-visible defect that no gate
catches. The default is KEEP unless grep proves otherwise.

**K-8 — `MapFullscreen.conf` is a delta, not a replacement.**
Removing the `OVT_MapIcons` block changes what merges with vanilla's config. This is why
`SCR_MapRadialUI` is still live on Overthrow's map despite our conf never mentioning it, and why the
`m_DescriptorDefaultsConfig` block must be left alone — it is `map/location-types`' bus-stop duplicate
suppression, not `OVT_MapIcons` support.

**K-9 — Provenance comments keep their rationale but lose their line numbers.**
Nine surviving `//!` comments cite `OVT_MapIcons.c:<line>` to explain *why* a new location type behaves as
it does. That reasoning is worth keeping; a `file:line` pointer into a deleted file is actively
misleading. P3.7 rewrites the pointer, not the reasoning — and the DoD grep then has a known, enumerated
expected result instead of an open-ended one.

---

## 6. Definition of Done

An independent evaluator with no implementation context can verify every item below.

### 6.1 Functional criteria

- **F-1** The Overthrow main menu shows **no "Map Info" row and no "Fast Travel" row**. The remaining rows
  (Place, Resistance, Jobs, Build, Real Estate, Manage Recruits, Character Sheet, Save) all still work.
- **F-2** Opening the Overthrow main menu with a **controller** immediately focuses the first row
  ("Place") and D-pad navigation works from the first press. *(FINDING A)*
- **F-3** `OVT_CatchBusAction` at a bus stop still opens the map, a bus-stop info panel still offers a
  **priced** trip, and taking it moves the player and charges **once**.
- **F-4** Fast travel from a location info panel still works, still charges **once**, and still brings
  recruits when the toggle is on.
- **F-5** The FOB restriction rings still draw, at the radii the FOB deploy check enforces (BUG-070's fix
  has not regressed). The player-location marker still draws.
- **F-6** No player-visible capability that existed before this feature is missing. Clicking empty map to
  read town info is *not* such a capability — it was already superseded by the Town location panel.
- **F-7** The in-game Field Manual does not instruct the player to use a menu entry that no longer exists.
  *(FINDING B)*

### 6.2 Quality criteria

- **Q-1** `tools/compile-check.sh` → **exit 0**, no `file:line:` output.
- **Q-2** `tools/run-tests.sh "{6A6E29FF47ECB840}"` → **exit 0**, case count **unchanged from the P0
  baseline (expected 44)**.
- **Q-3** `tools/run-tests.sh "{6A6E2A002F53A581}"` → **exit 0**, case count **unchanged from the P0
  baseline (expected 79)**. A changed count is a finding, not a number to update.
- **Q-4** `grep -rn "OVT_MapIcons" --include=*.c --include=*.conf --include=*.et --include=*.layout .`
  returns **exactly these nine doc-comment prose mentions and nothing else** — no class definition, no
  config entry, and (after P3.7) no `file:line` pointer into the deleted file:
  `Scripts/Game/Components/Map/OVT_MapMarkerComponent.c:20` ·
  `Scripts/Game/GameMode/Managers/OVT_JobManagerComponent.c:75` ·
  `Scripts/Game/UI/Map/LocationTypes/OVT_MapLocationHouse.c:4,118` ·
  `.../OVT_MapLocationVehicle.c:5,130` · `.../OVT_MapLocationWaypoint.c:14,18` ·
  `.../OVT_MapLocationWarehouse.c:5`
- **Q-5** No dangling GUID references outside `docs/`:
  `grep -rn "F5E0CFFFC9F27B19\|0EC60966C99CE954\|7BAC7637E5744768" . --include=*.c --include=*.conf --include=*.et --include=*.layout --include=*.meta`
  returns **nothing**.
- **Q-6** `Scripts/Game/UI/Context/OVT_MapContext.c` is **under 80 lines** (from 592) and defines exactly
  `GetMap`, `ShowMap`, `HideMap`, `OpenMap`.
- **Q-7** `grep -rn "GetOnMapClose\|IsInfoPanelVisible\|RequestFastTravel" --include=*.c Scripts/` returns
  only the two migration-record prose mentions in `OVT_TravelRequestComponent.c`.
- **Q-8** `git diff --stat -- Language/localization_Overthrow.*.conf` is **empty** — the six generated
  exports were not hand-edited. `localization_Overthrow.st` has balanced braces and every id in the P5.5
  KEEP column still present.
- **Q-9** The Workbench loads the world with **no missing-resource errors**.
- **Q-10** Every phase is a separate commit; `git log --oneline` shows P1…P6 individually revertable.

### 6.3 Integration criteria

- **I-1** All three canvas-layer modules still register and draw: `OVT_MapRestrictedAreas` (rings),
  `OVT_MapPlayerLocation` (marker), `OVT_MapThreatGrid` (still present, still disabled).
- **I-2** `MapFullscreen.conf` retains `OVT_MapPlayerLocation`, `OVT_OverthrowMapUI`, both `m_aModules`
  entries and the whole `m_DescriptorDefaultsConfig` block.
- **I-3** `OVT_OverthrowMapUI` is otherwise unaffected — only `IsInfoPanelVisible()` and its doc comment
  are removed.
- **I-4** `OVT_TravelRequestComponent` on `OVT_OverthrowController` is the **only** server-side travel
  path. No new client→server RPC was added anywhere.
- **I-5** `OVT_PlayerCommsComponent` compiles and every remaining `RpcAsk_`/`RpcDo_` symbol is byte-identical
  to before.
- **I-6** `OVT_CatchBusAction` is **unmodified** — its `OpenMap()` call was already correct as of
  `map/fast-travel` Phase 4.

### 6.4 Verification method

Run in order; stop and fix at the first failure.

```bash
cd "/mnt/n/Projects/Arma 4/Overthrow.Arma4"

# Q-1
tools/compile-check.sh                              # expect exit 0

# Q-2 / Q-3
tools/run-tests.sh "{6A6E29FF47ECB840}"             # Fast  — exit 0, 44 cases
tools/run-tests.sh "{6A6E2A002F53A581}"             # All   — exit 0, 79 cases

# Q-4
grep -rn "OVT_MapIcons" --include=*.c --include=*.conf --include=*.et --include=*.layout .

# Q-5
grep -rn "F5E0CFFFC9F27B19\|0EC60966C99CE954\|7BAC7637E5744768" . \
  --include=*.c --include=*.conf --include=*.et --include=*.layout --include=*.meta

# Q-6
wc -l Scripts/Game/UI/Context/OVT_MapContext.c

# Q-7
grep -rn "GetOnMapClose\|IsInfoPanelVisible\|RequestFastTravel" --include=*.c Scripts/

# Q-8
git diff --stat -- Language/localization_Overthrow.*.conf   # must be empty
```

Then **§4 Phase 7** in full — 7a Workbench load, 7b single-player sweep (incl. location-types V-3/V-4),
7c two-client MP/JIP (V-5), 7d gamepad (V-6), 7e save compatibility (V-7).

**What the two-client run must confirm:** shared markers identical on both clients; A's house, vehicle and
private camp invisible to B; A's warehouse and FOB visible to B; B's own purchases invisible to A; both
clients fast-travelling concurrently each arrive correctly and are charged once; a JIP client can fast
travel; no script errors in either log on map open, travel or bus trip.

---

## 7. Testing Strategy

**What the automated gates cover:** that the scripts still compile and that manager init, campaign state,
economy and persistence still behave. That is the spine.

**What they do not cover, and this feature's diff is mostly there.** `.layout`, `.conf`, `.et` and `.st`
files are invisible to `compile-check.sh` **and** to both test groups. Four of the seven phases edit
nothing else. Concretely:

- A deleted layout GUID still referenced from a prefab → compile 0, tests green, **missing widget at
  runtime**.
- A removed config block that also removed something else by accident → compile 0, tests green, **module
  silently absent**.
- A partially-deleted `.st` block → compile 0, tests green, **corrupt string table**.

Additionally, **exit 0 from `compile-check.sh` does not mean the scripts load** — it has two known blind
spots (`Rpc()` arity, and argument-count overflow on base-class methods with defaulted params). Always read
`run-tests.sh`'s verdict too; an indeterminate (exit 2) with no `junit.xml` is the signature of a runtime
compiler rejection that the static check passed.

**No new tests.** This feature deletes no covered behaviour and adds no assertable logic. Adding a test
here would be padding. The invariant *is* the test: **the counts must not change** (44 / 79). Extending a
tier is the right move only if a deletion turns out to be assertable in the test world — and it is not.

**Manual verification is the gate**, and it is Phase 7. Test scenarios and edge cases:

| # | Scenario | Expected |
|---|---|---|
| 1 | Open the Overthrow main menu | No Map Info / Fast Travel rows; other rows intact |
| 2 | Open it with a controller | First row focused; D-pad works immediately *(FINDING A)* |
| 3 | Catch Bus at a stop | Map opens; bus panel priced; trip charges once |
| 4 | Fast travel from a panel | Works; charges once; recruits follow when toggled on |
| 5 | Fast travel with no money | Button disabled with a readable reason, no debit |
| 6 | Open/close the map repeatedly | Map item stows every time; no listener accumulation |
| 7 | Deploy near a restriction ring | Ring drawn where the deploy check refuses (BUG-070) |
| 8 | Two clients, concurrent travel | Each arrives correctly, each charged once |
| 9 | JIP client fast-travels | Server path executes for a mid-campaign joiner |
| 10 | Load a pre-change save | All markers render; save/reload identical |
| 11 | Field Manual → Introduction | No instructions for deleted menu entries *(FINDING B)* |
| 12 | Workbench project load | Zero missing-resource errors |

---

## 8. Dependencies

**Hard gates — both satisfied.**

- **`map/location-types`** (built 2026-08-10, Phases 1–6). Closed all four parity gaps and, critically,
  eliminated `OVT_MapIcons.RegisterPOI`'s compile-level dependency by migrating POIs and bus stops onto
  `OVT_MapMarkerComponent` + `OVT_MapMarkerManagerComponent`. **Its Phase 7 is outstanding and is folded
  into this feature's P7** (settled decision D-4).
- **`map/fast-travel`** (complete, play-tested green 2026-08-10). Moved travel server-side to
  `OVT_TravelRequestComponent`, restored recruit accompaniment, migrated bus travel with no armed mode, and
  re-pointed `OVT_CatchBusAction` at `OpenMap()`. **The "re-point `OVT_CatchBusAction`" item in
  requirements.md was already discharged by that feature's Phase 4 — no work is planned for it here.**

**Soft.**

- **`map/core`** — the documented contract that justifies calling parity.
- **`towns/map-info`** — the doc being archived; read as the authoritative list of what the legacy system
  did, so nothing is deleted that was never replaced.
- **Workbench** — the only tool that can verify the config/layout/prefab deletions (P7a).
- **`tools/launch-server.sh` + two `tools/launch-game.sh --profile` clients** — the P7c harness. Two
  clients into one campaign is verified working.
- **`help-docs-sync`** — P6.4, for FINDING B.

**Blocks:** `map/respawn`, `map/territory-overlay`, `map/map-layers`, `map/shared-markers` all build on
"the finished, legacy-free map", so this feature's end state is what they inherit.

**Known epic tech debt this feature must NOT fix** (removal only): BUG-133 (`IconLayout` widget-name
mismatch), BUG-134 (`CloseButton` mismatch), BUG-136 (`OnLocationDataChanged` has no callers), the three
manager-access idioms across the ten location types, and `OVT_MapLocationType.OnLocationClicked` being
unreachable because `HandleSelection()` has no callers. Each is a trap to note, not this feature's to fix.

---

## 9. Risks & Mitigation

| # | Risk | Likelihood | Impact | Mitigation |
|---|---|---|---|---|
| R-1 | **Building against unverified parity.** `map/location-types` Phase 7 has never run; if the new map has a gap, the legacy fallback is gone. | Medium | High | Settled decision D-4 accepts this. Every phase is a separate commit → `git revert` is the whole remediation. P7 explicitly folds in V-3…V-7 *before* sign-off. The gap goes back to `location-types`/`fast-travel`, not fixed here. |
| R-2 | **A deleted GUID is still referenced from a prefab or layout.** Compile 0 and green tests prove nothing here. | Low | High — silent missing widget | All three layout GUIDs were grep-verified fully self-contained *before* planning (each has exactly one live external reference, deleted in the same phase). Q-5 re-greps; P7a is the Workbench gate. |
| R-3 | **Partial `.st` block deletion corrupts the master.** Has nearly happened once. | Low | High — six exports break | P5.1 forbids line ranges, requires locating by `Id` string and deleting a whole `CustomStringTableItem` unit with boundaries confirmed by eye. Q-8 checks brace balance and that the six generated exports are untouched. |
| R-4 | **Gamepad focus regression in the main menu** (FINDING A). | **High if unmitigated** — the only `SetFocusedWidget` sits inside a deleted block | Medium — menu unusable on controller | P1.2 relocates the call to "Place"; F-2 and P7d step 22 test it specifically. |
| R-5 | **Stale in-game help** (FINDING B) — the Field Manual keeps telling players to use deleted menu entries. | **Certain if unmitigated** | Medium — player confusion | P6.4 runs `help-docs-sync` over the two entries, with every replacement sentence fact-checked against a `file:line`. |
| R-6 | **A string still shown by the live path gets deleted as an orphan.** | Low | Medium — raw `#OVT-` key on screen | K-7: default KEEP. The P5.5 table records the audit — nine candidates, one deletion. `MustHaveMap` is the worked example of why the default matters. |
| R-7 | **BUG-070 regresses** — restriction rings stop matching the enforced radii while `OVT_MapRestrictedAreas` is nearby in the config being edited. | Low | Medium | The module is explicitly retained and untouched; P3.2's acceptance criteria name it; F-5 and P7b step 10 test the rings against the deploy check. |
| R-8 | **Test counts move**, masking a deleted behaviour that *was* covered. | Low | Medium | P0 records the baseline; Q-2/Q-3 require the counts unchanged. A moved count is a finding to investigate, never a number to update. |
| R-9 | **`HideMap()` is deleted later as an orphan**, losing the play-test-derived stow ordering. | Medium (future) | Medium | K-3 + P2.5: a `//!` block on the method stating it has no callers today, why it survives, and who consumes it next. |

---

## 10. Quality Bar

This is a **deletion** feature. The bar is not new capability or visual polish — those would be scope
creep. The bar is three things:

**1. Nothing player-visible is lost.**
Every deletion must be preceded by proof that a live path already does the same job. That proof is a grep
result or a named replacement, recorded in the plan — not an assumption that "the new map probably covers
it". The two findings in §3.4 are exactly what this discipline catches: a `SetFocusedWidget` call hiding
inside a block that *looks* purely legacy, and in-game help that documents deleted UI. Neither is visible
from the symbol being deleted. **Read the whole enclosing block before removing it, and ask what else lives
there.**

**2. Nothing dangling is left behind — across four compiler-invisible file classes.**
`.layout`, `.conf`, `.et` and `.st` are invisible to `compile-check.sh` *and* to both test groups, and this
feature's diff is mostly in them. A dangling GUID, a half-deleted string block or an orphaned config entry
all pass every automated gate and fail at runtime. The specific disciplines:

- **Grep-prove a symbol dead before cutting it.** Not "it looks unused" — a grep across `*.c`, `*.conf`,
  `*.et`, `*.layout` and `*.meta` with the result recorded.
- **Delete whole syntactic units, located by symbol name or `Id` string.** Never a line range. Confirm the
  opening and closing brace by eye. This plan's line numbers are pointers to look at, and they have already
  drifted ~21 lines from the hand-off doc that produced them.
- **Reference integrity is bidirectional.** Removing a config block can orphan a layout; removing a layout
  can orphan a `.meta`; removing a `.st` id can orphan a broadcast preset; removing a menu row can orphan
  a Field Manual entry. Every deletion asks "what pointed at this, and what did this point at?"
- **Prose is part of the tree.** A `//!` comment citing `OVT_MapIcons.c:472` after that file is deleted is
  a dangling reference with a human reader instead of a compiler. Keep the rationale, drop the pointer.

**3. The tree is shippable at every phase boundary.**
Caller-first ordering, a compile + Fast-group gate after each phase, and a separate commit per phase. At no
point does a reachable code path call a deleted symbol. If P7 finds a gap, the remediation is `git revert`
of one phase — not an archaeology exercise.

**And one restraint.** If retirement surfaces a gap in the new map, the plan says where it goes: back to
`map/location-types` or `map/fast-travel`. Fixing it here would turn a clean, revertable deletion into a
mixed change that cannot be bisected — and would quietly re-open the parity question this feature exists to
close.

---

*Plan authored 2026-08-10. All line numbers verified against the working tree on `new-map`; they supersede
those in `docs/features/map/fast-travel/context.md`'s dead-code hand-off, which have drifted. Treat every
number here as a pointer to look at, never as a `sed` argument.*
