# Map Legacy Retirement - Task Checklist

**Last Updated:** 2026-08-10
**Progress:** ✅ **COMPLETE — 51/51 build tasks + all 27 Phase 7 verification boxes.** Play-tested green 2026-08-10.

> Generated 2026-08-10 from `implementation.md` §4. Seven phases, **caller-first**: callers are cut
> before callees so the tree is shippable at every phase boundary.
>
> **Gate after every phase:** `tools/compile-check.sh` (exit 0) **and**
> `tools/run-tests.sh "{6A6E29FF47ECB840}"` (Fast). **All** group before sign-off.
>
> ⚠️ **Test counts must not move** (baseline Fast 44 / All 79). A changed count is a *finding*, not a
> number to update.
>
> ⚠️ **Never delete by line range.** Every `file:line` in the plan is a pointer to look at, not a `sed`
> argument. Locate by **symbol name** or **`Id` string**, confirm block boundaries by eye, delete whole
> syntactic units.
>
> **Advanced-agent phases:** Phase 2 and Phase 3.

---

## Phase 0 — Baseline (XS · no agent) ✅ COMPLETE

- [x] **P0.1** Note working-tree state and HEAD sha for the revert path — `28c2f957`, tree clean
- [x] **P0.2** `tools/compile-check.sh` → **exit 0, 5959 files**
- [x] **P0.3** `tools/run-tests.sh` both groups → **Fast 44 / All 79**, both exit 0 (the invariants)
- [x] **P0.4** Record all three baselines in `context.md`

---

## Phase 1 — Main-menu entries (S · standard agent) ✅ COMPLETE

*Nothing arms a legacy mode after this phase.*

- [x] **P1.2** 🔴 **FINDING A first** — relocate `GetGame().GetWorkspace().SetFocusedWidget(comp.GetRootWidget());` into the surviving `// Place` block (before `comp.m_OnClicked.Insert(Place);`), with a `//!` note that this is the menu's initial gamepad focus
- [x] **P1.1a** `OVT_MainMenuContext.c` — delete the whole `// Map Info` block in `OnShow()` (locate by `GetButtonText("Map Info", m_wRoot)`)
- [x] **P1.1b** `OVT_MainMenuContext.c` — delete the whole `// Fast Travel` block (locate by `GetButtonText("Fast Travel", m_wRoot)`)
- [x] **P1.3** Delete the `MapInfo()` and `FastTravel()` methods
- [x] **P1.4** `UI/Layouts/Menu/MainMenu.layout` — delete both whole `ButtonWidgetClass` units (`Name "Map Info"` `{598AB670C1F2839C}`, `Name "Fast Travel"` `{598AB6734AADE219}`) — ~40 lines each, ending after the nested `SizeLayoutWidgetClass`
- [x] **P1.5** ❌ Do **not** touch `Language/localization_Overthrow.st` — both ids are still consumed (§3.4)
- [x] **P1.6** ❌ Do **not** add a "Map" entry (settled decision D-2)
- [x] **P1.7** 🟠 **FINDING C (user-directed scope addition)** — reorder `MainMenu.layout` so `Place` is genuinely the first visible row, keeping focus on it
- [x] **P1 gate** — all 4 acceptance greps clean; braces 333/333 (was 389/389, −56 = 2×28); compile **0 / 5959**; Fast **44** ✅

---

## Phase 2 — `OVT_MapContext` strip (M · **ADVANCED agent**) ✅ COMPLETE

*592 → <80 lines. Failure mode is silent: a surviving reference in a `.et` attribute the compiler never reads.*

- [x] **P2.1** Delete by symbol: the 3 mode flags, 3 distance constants, 4 cached manager members, `m_SelectedTown`, and the `m_ModLayout` / `m_NegativeModifierColor` / `m_PositiveModifierColor` attributes
- [x] **P2.2** Delete by symbol: `PostInit()`, `CanFastTravel()`, `EnableMapInfo()`, `ShowTownInfo()`, `EnableFastTravel()`, `EnableBusTravel()`, `OnMapExit()`, `DisableMapInfo()`, `DisableFastTravel()`, `DisableBusTravel()`, `RegisterInputs()`, `UnregisterInputs()`, `MapExit()`, `IsOverthrowInfoPanelVisible()`, `MapClick()`
- [x] **P2.3** Retain **verbatim**: `GetMap()`, `ShowMap()`, `HideMap()` (incl. its full `//!` block) and `OpenMap()`
- [x] **P2.4** Rewrite `OpenMap()`'s doc comment — it forward-references `EnableBusTravel` "below", which will not exist
- [x] **P2.5** 🟡 Add a `//!` block above `HideMap()`: zero callers today, retained as public API for `map/respawn` (§5 K-3) — without this the next reader deletes it
- [x] **P2.6** Add a class-level `//!`: this is a map-*gadget* helper, not a map UI class
- [x] **P2.7** `OVT_OverthrowMapUI.c` — delete `IsInfoPanelVisible()` + its doc comment, the legacy-second-listener comment, and correct the recruit-default rationale comment citing `OVT_MapContext.c:441`
- [x] **P2.8** De-line-number the stale `OVT_MapContext.c:<line>` prose pointers in `OVT_FastTravelService.c` (×2) and `OVT_MapLocationBusStop.c` (×1) — keep the rationale
- [x] **P2.9** Correct 4 stale comments in `OVT_TEST_InitSuite.c` — **comments only, change no assertion**
- [x] **P2 gate** — `OVT_MapContext.c` **79 lines / exactly 4 methods**; `GetOnMapClose`, `IsInfoPanelVisible` and `SCR_MapEntity.GetOn` greps all empty (**BUG-069 part 4 structurally closed**); compile **0 / 5959**; Fast **44** ✅

---

## Phase 3 — Leaf assets (M · **ADVANCED agent**) ✅ COMPLETE

*Touches all four compiler-invisible file classes. Every deletion must be grep-proven dead first.*

- [x] **P3.1** Delete `Scripts/Game/UI/Map/OVT_MapIcons.c` (846 lines)
- [x] **P3.2** `Configs/Map/MapFullscreen.conf` — delete the whole `OVT_MapIcons "{5994FB72BE0F9051}" { … }` block from `m_aUIComponents`. ⚠️ **same-GUID delta over vanilla** — retain `OVT_MapPlayerLocation`, `OVT_OverthrowMapUI`, both `m_aModules` entries and the whole `m_DescriptorDefaultsConfig` block
- [x] **P3.3** Delete `UI/Layouts/Map/MapIcon.layout` + `.meta` (`{F5E0CFFFC9F27B19}`)
- [x] **P3.4** Delete `UI/Layouts/Map/MapInfo.layout` + `.meta` (`{0EC60966C99CE954}`)
- [x] **P3.5** Delete `UI/Layouts/Map/MapInfo/Modifier.layout` + `.meta` (`{7BAC7637E5744768}`) and the now-empty `UI/Layouts/Map/MapInfo/` directory
- [x] **P3.6** `Character_Player.et` — inside the `OVT_MapContext "{598E83B6A7175CBE}"` block delete only `m_Layout`, `m_ModLayout`, `m_NegativeModifierColor`, `m_PositiveModifierColor`. **Keep the block** — `OVT_CatchBusAction` resolves the context by type
- [x] **P3.7** De-reference the nine surviving `OVT_MapIcons.c:<line>` provenance pointers across six files — **keep the rationale**, replace the pointer with "the legacy `OVT_MapIcons` layer (deleted in `map/legacy-retirement`)"
- [x] **P3 gate** — Q-4 grep returns **8 prose mentions in 5 files** (plan said 9/6 — corrected, see FINDING G); Q-5 GUID grep **empty**; `MapFullscreen.conf` retains all four keepers + `m_DescriptorDefaultsConfig`, braces 24/24; `.et` braces 140/140; compile **0 / 5958** (the predicted −1); Fast **44** ✅
- [x] **P3 note** — ⚠️ the real gate for this phase is **P7a** (Workbench load) — items added to the 7a checklist below

---

## Phase 4 — Delete the fast-travel RPCs (S · standard agent) — **security fix** ✅ COMPLETE

*Unreachable after P1 but still registered RPCs on a live component doing `ResolveSenderPlayerId` + `TeleportPlayer` with no validation and no payment.*

- [x] **P4.1** `OVT_PlayerCommsComponent.c` — delete by symbol: `RequestFastTravel`, `RpcAsk_RequestFastTravel`, `RequestFastTravelWithRecruits`, `RpcAsk_RequestFastTravelWithRecruits`
- [x] **P4.2** Confirm nothing is lost — the recruit ring-placement loop is already copied verbatim into `OVT_TravelRequestComponent.TeleportRecruits`
- [x] **P4.3** Leave the two prose mentions in `OVT_TravelRequestComponent.c` (the migration record); confirm they read correctly once the originals are gone
- [x] **P4.4** ❌ Add nothing — **never** add a new client→server RPC to `OVT_PlayerCommsComponent`
- [x] **P4 gate** — `RequestFastTravel` grep returns only the two migration-record mentions; RPC symbol list **61 → 59**, diff shows *exactly* the two expected names and nothing else; braces 233/233; 67 deletions / 0 insertions; compile **0 / 5958**; Fast **44** ✅

---

## Phase 5 — `OVT-NeedBusStop` and the orphan sweep (S · standard agent, high care) ✅ COMPLETE

- [x] **P5.1** 🔴 `Language/localization_Overthrow.st` — locate `Id "OVT-NeedBusStop"` and delete the **whole enclosing `CustomStringTableItem "{…}" { … }` block** (~18 lines, six `Target_*` translations). **Confirm boundaries by eye** — an earlier hand-off named a 4-line range *inside* this block
- [x] **P5.2** `Configs/overthrowBroadcastMessages.conf` — delete the whole enclosing `SCR_SimpleMessagePreset "{…}" { … }` block for `m_sTag "NeedBusStop"`, including its nested `m_UIInfo`
- [x] **P5.3** ❌ **Never** edit `Language/localization_Overthrow.<lang>.conf` — Workbench-generated exports; list the change for the user to regenerate instead
- [x] **P5.4** ❌ Do **not** delete `#OVT-NotAtBusStop` — live, consumed by `OVT_FastTravelService.c`
- [x] **P5.6** 🟡 **FINDING F** — correct the two remaining stale `NeedBusStop` prose mentions in `OVT_TEST_InitSuite.c` (one is a `SetResultFailure` string). **Comments/strings only — no assertion, no case, count must not move**
- [x] **P5.5** Orphan sweep recorded as **KEEP** for all eleven other ids (table in `implementation.md` §4 P5.5 / `context.md`) — do not re-run, do not cut
- [x] **P5 gate** — `NeedBusStop` grep clean outside the generated exports; `.st` braces **1048/1048** and all **11 KEEP ids** present; broadcast conf **322/322** with both neighbours intact; `git diff --stat -- Language/localization_Overthrow.*.conf` **empty** (verified twice); compile **0 / 5958**; Fast **44** ✅

---

## Phase 6 — Documentation (S · standard agent; `help-docs-sync` unavailable — see note) ✅ COMPLETE

- [x] **P6.1** Move `docs/features/towns/map-info/{context,implementation,tasks}.md` → `docs/archive/towns-map-info-*.md` with a pointer note; **`docs/archive/OverthrowMapSystem.md` stays untouched**
- [x] **P6.2** `docs/features/towns/epic-overview.md` — remove the `map-info` row, build-order entry, dependency line, and the map clause in Purpose; retarget the four Tech Debt bullets naming `map-info` at the `map` epic (BUG-067/068/069 now structurally impossible; BUG-070 concerns the **retained** `OVT_MapRestrictedAreas`)
- [x] **P6.3** `docs/overview.md` — towns row → 4 features, drop `map-info`; update the map row for retirement complete
- [x] **P6.4** 🟡 **FINDING B** — `help-docs-sync` over the two Field Manual header/text pairs in `FieldManualConfigRoot.conf` + the `.st` bodies of `#OVT-FieldManual_MapInfo_Text` / `#OVT-FieldManual_FastTravel_Text`. Both describe the deleted click-anywhere workflow. **Fact-check every replacement sentence against a `file:line`**
- [x] **P6.5** `docs/features/map/epic-overview.md` — mark feature 4 complete, refresh the rollup, move the "Legacy static is a hard dependency" tech-debt bullet to resolved
- [x] **P6.6** Finalise `context.md` — baselines, the two findings, the P5.5 orphan-sweep table
- [x] **P6 gate** — `map-info` grep returns only archive pointers + the **v1.7 changelog entry, deliberately preserved** (rewriting a dated record would falsify it); compile 0; Fast 44; **All 79** ✅

---

## Phase 7 — Combined verification gate (L · **user-driven, NO agent**) ✅ **DISCHARGED 2026-08-10**

> **Discharges two features at once** — `map/location-types` Phase 7 (V-3 … V-7) folds in here.
> ⚠️ Client launches open a real window and can orphan. **Always pass a long `--timeout`.**

**7a — Workbench load** *(this feature's own gate; catches P3's invisible risk)*
- [x] Project opens with **zero missing-resource / missing-GUID errors**
- [x] ⚠️ **If `{B8F4C6A8C9D3E4F1}` trips, it is NOT this feature.** `Scripts/Game/UI/Map/OVT_MapThreatGrid.c.meta` is a **pre-existing orphan** — it names a path the file no longer occupies (it moved to `Visualization/`, which has no `.meta`). Committed in `96e6da4d Vehicle patrols`, untouched here. Easy to misattribute, since `OVT_MapThreatGrid` is one of the *retained* modules
- [x] Confirm the Workbench does not re-add a `.meta` for the deleted layouts on first open, and that the asset browser shows no red entries under `UI/Layouts/Map/`
- [x] `Character_Player.et` — the `OVT_MapContext` block is live with **exactly two** attribute rows (`m_bOpenActionCloses`, `m_bHideHUDOnShow`) and no red/broken rows. This is the specific thing P3.6 closed
- [x] `MapFullscreen.conf` — **two** UI components (`OVT_MapPlayerLocation`, `OVT_OverthrowMapUI`) and **two** modules remain. ⚠️ The plan said "four UI components"; that was written before P3.2 removed one, and the rest merge in from vanilla

**7b — Single-player sweep** *(this feature + location-types V-3/V-4)*
- [x] Main menu: no "Map Info" row, no "Fast Travel" row, highlight starts on **"Place"** (FINDING A)
- [x] Field Manual → Overthrow → Introduction matches reality (FINDING B)
- [x] **V-3** full marker checklist: house, vehicle, camp, FOB, maintenance ramp, job waypoint, recruit
- [x] **V-4** zoom sweep max-out → max-in; every type at a sensible zoom, no illegible name overlap
- [x] **Catch Bus** at a stop → map opens, bus panel **priced**, trip charges **once**
- [x] Fast travel from a location panel → works, charges **once**
- [x] FOB **restriction rings still draw** at the enforced radii (BUG-070)
- [x] Player-location marker still draws
- [x] Map **item is stowed** on close, not left in hand

**7c — Two-client MP/JIP gate** *(location-types V-5 + this feature's MP gate — highest risk, do not skip)*
- [x] `tools/launch-server.sh`; Client A joins
- [x] As A: buy a house, buy a vehicle, place a **private** camp, deploy a FOB, note a shop's caret rows
- [x] Client B **JIP** (join after A's state exists)
- [x] On B: shared markers match A's; **A's house/vehicle/private camp invisible**; A's warehouse + FOB visible; FOB and shop panels populate identically
- [x] On B: buy a house and a vehicle → visible to B, **not** to A
- [x] **Both clients fast-travel concurrently** — each arrives correctly, each charged **once**
- [x] As B (JIP), fast travel — the server path executes for a mid-campaign joiner
- [x] Neither log shows a script error on map open, travel or bus trip

**7d — Gamepad/console gate** *(location-types V-6 + FINDING A)*
- [x] **Controller only, no mouse** — main menu focuses something immediately, D-pad works from the first press
- [x] Map cursor onto each marker type — every info panel appears and is readable at 1080p
- [x] Fast-travel button reachable on every type that offers it; recruit toggle reachable via the map cursor

**7e — Save compatibility** *(location-types V-7)*
- [x] Load a **pre-change** save — map opens, all markers render, no errors
- [x] Save, reload, confirm identical (expected: no difference — nothing deleted was persisted)

---

## Summary

| Phase | Tasks | Agent |
|---|---|---|
| 0 — Baseline | 4 | none |
| 1 — Main-menu entries | 8 | standard |
| 2 — `OVT_MapContext` strip | 10 | **advanced** |
| 3 — Leaf assets | 9 | **advanced** |
| 4 — Fast-travel RPCs | 5 | standard |
| 5 — String + preset orphans | 6 | standard |
| 6 — Documentation | 7 | standard ×2 (`help-docs-sync` agent not installed) |
| **Build total (0–6)** | **51** | |
| 7 — Verification gate | 27 boxes ✅ | **user-driven — discharged** |

**Progress: 51/51 build tasks + 27/27 Phase 7 boxes — FEATURE COMPLETE.**
The user ran the full combined gate on 2026-08-10 and reported all green, **including the two-client
MP/JIP pass and the pre-change save load** — the two parts a single session most often skips — and
regenerated the six localization exports.
