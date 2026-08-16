# HUD Icons — Task Checklist

**Feature:** gm/hud-icons (epic `gm`, feature 3 of 5)
**Last Updated:** 2026-08-16
**Progress:** 30/30 tasks complete (100%) — ✅ user-verified 2026-08-15 (incl. MP/JIP + tower variants); descoped to tooltips-only same day; draw distance lowered 20000 → 2000 on 2026-08-16 and user-verified (see context.md)

> Agent tiers per `implementation.md` §4: **Phase 1 is ADVANCED** (`component-developer-advanced`) — the
> epic's riskiest base-game integration with four silent failure modes. Phase 0 → orchestrator (no agent);
> Phases 2–3 → `ui-developer`; Phase 4 → `component-developer`; Phase 5 → user-driven; Phase 6 →
> `component-developer`.

---

## Phase 0: Baseline (4/4 complete) — S, no agent

- [x] ✅ **Record baseline in context.md**
  - Description: `compile-check.sh` exit 0 + file count; `git status` / `git rev-parse --short HEAD` (plan cited `b01782c3` — re-verify at every phase boundary); highest allocated bug id (`ls docs/bugs/`, BUG-174 at planning)
  - File(s): `docs/features/gm/hud-icons/context.md`
  - Estimate: 🟢

- [x] ✅ **Re-grep GUID series `{6B09…}` free**
  - Description: 0 hits for the literal `{6B09` (with brace!) across `Prefabs Configs Scripts UI Language`; spare `{6B0A…}`. waypoint-viz may build in parallel — must not collide
  - File(s): n/a (grep gate)
  - Estimate: 🟢

- [x] ✅ **Verify seam + panel citations resolve**
  - Description: `OVT_GMPanelUIComponent.c` `GetInstance`/`GetDetailSlot`/`ShowDetail`/`ClearDetail` (~:232/:247/:261/:278); `OVT_GMCampaignState.c:90` `HasData`; `OVT_ControllerComponent.c:36` `Get`
  - File(s): n/a (read gate)
  - Estimate: 🟢

- [x] ✅ **Verify tower prefab inheritance**
  - Description: Diff `TransmitterTower_01{,_medium,_small}_base.et` against `Prefabs/Structures/Core/Tower_Base.et` — confirm no `RplComponent` anywhere in the chain (LOCAL flag depends on it)
  - File(s): n/a (read gate)
  - Estimate: 🟢

---

## Phase 1: Editable components, prefab blocks, Init gate (6/6 complete) — M, `component-developer-advanced`

- [x] ✅ **OVT_GMCampaignUIInfo.c + kind enum**
  - Description: `enum OVT_EGMIconKind { TOWN, BASE, RADIO_TOWER }` (public, gm-map reuses); `class OVT_GMCampaignUIInfo : SCR_EditableEntityUIInfo` with `IEntity m_Owner` + kind; `Configure(owner, kind)` writes `Icon`+`IconSetName`; `override GetName()` (town name bounds-checked via GetTownName, "Base %1", "#OVT-GMIcon_RadioTower"). GetDescription() is Phase 4 — leave base behaviour
  - File(s): `Scripts/Game/Components/GM/OVT_GMCampaignUIInfo.c` (new)
  - Estimate: 🟡

- [x] ✅ **OVT_GMEditableCampaignComponent.c**
  - Description: extends `SCR_EditableSystemComponent`; `[Attribute] OVT_EGMIconKind m_eKind`; `protected ref OVT_GMCampaignUIInfo m_Info` (**must be strong ref** — SetInfoInstance stores weak); `OnPostInit` → super, build+Configure+CopyFrom(GetInfo())+SetInfoInstance; `override GetFaction()` (TOWN/RADIO_TOWER from Overthrow data, BASE → super; null-guard every manager — runs at world init + dedicated server); `override IsReplicated() → false` with comment naming protected call sites; UI refresh trigger on faction change (or record "next snapshot tick" choice)
  - File(s): `Scripts/Game/Components/GM/OVT_GMEditableCampaignComponent.c` (new)
  - Estimate: 🟡

- [x] ✅ **Prefab blocks — town + base controllers**
  - Description: Component block per §3.4: `m_EntityType SYSTEM`, `m_bAutoRegister ALWAYS`, `m_Flags 2052`, `m_fMaxDrawDistance 20000`, `m_vIconPos` `0 10 0` / `0 8 0`, kind TOWN/BASE. GUIDs from `{6B09…}`. Values copied from `ConflictBase_Base.et:36-45` precedent
  - File(s): `Prefabs/Controllers/OVT_TownController.et`, `Prefabs/Controllers/OVT_BaseController.et`
  - Estimate: 🟢

- [x] ✅ **Prefab blocks — three tower prefabs**
  - Description: Same but `m_Flags 2060` (+LOCAL — towers have NO RplComponent, mismatch nulls the component with an ERROR both ways), `m_vIconPos` `0 40 0`/`0 25 0`/`0 6 0`, kind RADIO_TOWER, imageset `tower`
  - File(s): `Prefabs/Structures/TransmitterTower/TransmitterTower_01_base.et`, `..._medium_base.et`, `..._small_base.et` (paths per repo)
  - Estimate: 🟢

- [x] ✅ **Init-tier test OVT_TEST_Init_GMIcons.c + prove-can-fail**
  - Description: 4 cases per plan §7 (town/base/tower carry component + SYSTEM type + NON_DELETABLE; tower count ≥ 1 + LOCAL; LOCAL == (RplComponent==null) invariant). Header comment: `_base`/`_medium_base` towers NOT in test world → Phase 5 Step 2. Prove each case can fail (remove block → named SetFailure → revert), record method in context.md
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_GMIcons.c` (new)
  - Estimate: 🟡

- [x] ✅ 🖐️ **USER SMOKE CHECK — Workbench, 2 min** - Completed 2026-08-15 (batched into Phase 5, user-verified)
  - Description: (a) icons over town/base/tower? (b) still visible at strategic altitude + 1 km? (c) delete refused, drag inert? (d) red script errors on load? A "no" to (a) is a finding, not a build-on
  - File(s): n/a (human verification)
  - Estimate: 🟢 (user)

---

## Phase 2: Detail surface + selection plumbing (7/7 complete — ⚠️ surface later REMOVED by user decision; format statics + Logic test + injection hygiene survive) — M, `ui-developer`

- [x] ✅ **GMIconDetail.layout + .meta**
  - Description: All rows pre-authored/named: `Detail_Title`, `Detail_Subtitle`, `Detail_Row_0..7` (`Detail_Label_N`+`Detail_Value_N`), `Detail_Note`. GUIDs from `{6B09…}`; `.meta` with all five console configs; duplicate-GUID script clean; spacing on LayoutSlot paddings NOT AlignableSlot (measure trap); align enums 0=left 1=center 2=right 3=stretch; chrome matches GMPanel rows
  - File(s): `UI/Layouts/GM/GMIconDetail.layout` + `.meta` (new)
  - Estimate: 🟡

- [x] ✅ **OVT_GMDetailUIComponent.c skeleton + subscriptions**
  - Description: extends `SCR_ScriptedWidgetComponent`; HandlerAttached caches widgets, subscribes SELECTED filter `GetOnChanged()` + gm-state `GetOnSnapshotUpdated`/`GetOnStateCleared`, ShowDetail(false); HandlerDeattached removes all three, nulls widgets; filter-null-at-attach → one CallLater(0) retry then quiet give-up; re-read filter contents on change (don't trust the sets); multi-select → first entity + count in Detail_Note
  - File(s): `Scripts/Game/UI/GM/OVT_GMDetailUIComponent.c` (new)
  - Estimate: 🟡

- [x] ✅ **Panel integration via four-call contract**
  - Description: `GetInstance()` → null-check → `GetDetailSlot()` → null-check; **never cache across frames**; **never call ClearDetail()** (deletes own widget tree — write rule in class Doxygen); visibility via `ShowDetail(bool)` only
  - File(s): `Scripts/Game/UI/GM/OVT_GMDetailUIComponent.c`
  - Estimate: 🟢

- [x] ✅ **Injection: modded SCR_EditModeEditorUIComponent (+~6 lines)**
  - Description: After existing `CreateWidgets(PANEL_LAYOUT,…)`: `CallLater(CreateDetail, 0)`; CreateDetail resolves GetInstance→GetDetailSlot→CreateWidgets(DETAIL_LAYOUT, slot), null-checks each step, one WARNING on layout resolve failure
  - File(s): `Scripts/Game/UI/Modded/SCR_EditModeEditorUIComponent.c`
  - Estimate: 🟢

- [x] ✅ **OVT_GMIconFormat.c pure statics**
  - Description: `FormatSupport`, `FormatPopulation`, `FormatUpgradeType` (strip OVT_BaseUpgrade prefix), `FormatOriginType`, `FormatOrigin`. Reuse `OVT_GMPanelFormat.FormatCountdown` for tower downtime — no duplicate. **World-free incl. comments** (Logic-tier grep guard reads prose)
  - File(s): `Scripts/Game/UI/GM/OVT_GMIconFormat.c` (new)
  - Estimate: 🟢

- [x] ✅ **Town/base/tower detail rendering + Logic test + strings**
  - Description: Rows per §3.3 table; base seam rows show "waiting for campaign data" when `!state.HasData()` while faction/location stay live; `OVT_TEST_Logic_GMIconFormat.c` cases per §7 (support %, zero-guard, extremes, population, upgrade readability, origin indexed/unindexed) + prove-can-fail; `#OVT-GMIcon_*` strings into `.st` with Comment (never touch `.conf` exports)
  - File(s): `Scripts/Game/UI/GM/OVT_GMDetailUIComponent.c`, `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_GMIconFormat.c` (new), `Language/localization_Overthrow.st`
  - Estimate: 🟡

- [x] ✅ **Grep gates: zero Rpc(/[RplProp]/RplRcver/ClearDetail( in feature files**
  - Description: Paste outputs into context.md
  - File(s): n/a (gates)
  - Estimate: 🟢

---

## Phase 3: Group + player detail (5/5 complete — ⚠️ REMOVED with the detail surface; group/player have no surface now) — S, `ui-developer`

- [x] ✅ **Group detail via RplId join**
  - Description: GROUP-type selection → `GetOwner().FindComponent(RplComponent)` → `Id()` → `state.FindGroup(id)`; render origin type + index resolved to human label (base idx → base, townID → town name, tower id → tower, -1 → reason only) + reason
  - File(s): `Scripts/Game/UI/GM/OVT_GMDetailUIComponent.c`
  - Estimate: 🟡

- [x] ✅ **"No Overthrow record" as first-class state**
  - Description: Swept/stale groups vanish from `m_aGroups` (known occupying-epic debt) — render the row, no log, no warn
  - File(s): `Scripts/Game/UI/GM/OVT_GMDetailUIComponent.c`
  - Estimate: 🟢

- [x] ✅ **Player detail**
  - Description: CHARACTER with `GetPlayerID() > 0` → `OVT_PlayerData.Get(playerId)` → money (`GetPlayerMoney`) + level (`GetLevel()`); both already broadcast — no seam
  - File(s): `Scripts/Game/UI/GM/OVT_GMDetailUIComponent.c`
  - Estimate: 🟢

- [x] ✅ **Non-player character → ShowDetail(false)**
  - Description: AI characters get no Overthrow rows — not an empty box
  - File(s): `Scripts/Game/UI/GM/OVT_GMDetailUIComponent.c`
  - Estimate: 🟢

- [x] ✅ **Snapshot re-render + state-clear handling**
  - Description: Re-render current selection on `GetOnSnapshotUpdated()` (~8 s poll); clear seam-sourced rows on `GetOnStateCleared()`
  - File(s): `Scripts/Game/UI/GM/OVT_GMDetailUIComponent.c`
  - Estimate: 🟢

---

## Phase 4: Hover tooltips (3/3 complete) — S, `component-developer`

- [x] ✅ **GetDescription() override on OVT_GMCampaignUIInfo**
  - Description: 1–2 lines per kind via Phase 2 format helpers: town → support % + stability; base → faction + garrison count; tower → online / "sabotaged — 4:12". Tooltip is 276 px wide. Never return empty string for a live entity (HasDescription gate kills the widget). Do NOT add a tick — tooltip written once per hover by design
  - File(s): `Scripts/Game/Components/GM/OVT_GMCampaignUIInfo.c`
  - Estimate: 🟢

- [x] ✅ **#OVT-GMIcon_Tooltip_* strings**
  - Description: Into `.st` with Comment filled
  - File(s): `Language/localization_Overthrow.st`
  - Estimate: 🟢

- [x] ✅ **Record no-group/player-tooltip rationale in context.md**
  - Description: §5 D6 reasoning (group info instance owned by vanilla, no back-reference) so next planner doesn't re-derive
  - File(s): `docs/features/gm/hud-icons/context.md`
  - Estimate: 🟢

- [x] ✅ **(Discovered) Fifth Init case: InfoHasNameAndDescription** - Completed 2026-08-15
  - Description: Guards the HasDescription trap (empty description silently destroys the vanilla tooltip detail widget); asserts GetInfo() resolves, GetName() non-empty, HasDescription() true for all three families; can-fail proven with a named failure (1 of 43)
  - File(s): `Scripts/Game/Tests/TestSuites/Init/OVT_TEST_Init_GMIcons.c`
  - Estimate: 🟢

---

## Phase 5: Verification gate (1/1 complete) — M, user-driven

- [x] ✅ 🖐️ **USER: full verification method (plan §6)** - Completed 2026-08-15 (user: all checks pass incl. MP/JIP + tower variants; tooltips-only scope — F-4…F-8/F-10 obsolete post-descope)
  - Description: Step 1 Workbench host (F-1…F-10, Q-2/Q-3/Q-5/Q-9, regen localization first); Step 2 tower `_base`/`_medium_base` variants in Eden; Step 3 MP (own server preferred; seam-vs-local row asymmetry, JIP, negative path, record auth path); Step 4 grep gates pasted into context.md. ⚠️ Warn before launching anything
  - File(s): results → `docs/features/gm/hud-icons/context.md`
  - Estimate: 🔴 (user)

---

## Phase 6: context.md + epic bookkeeping (3/3 complete) — S, `component-developer`

- [x] ✅ **Final hud-icons context.md** - Completed 2026-08-15
  - Description: Shipped flag/draw-distance table as built; Phase 1 Workbench findings; icon-asset conventions stated for gm-map reuse; Phase 5 results; "GM sees no icons" triage chain
  - File(s): `docs/features/gm/hud-icons/context.md`
  - Estimate: 🟢

- [x] ✅ **Update epic-overview.md** - Completed 2026-08-15
  - Description: Feature 3 status + task count. **No help-docs phase** (consolidated pass at epic end)
  - File(s): `docs/features/gm/epic-overview.md`
  - Estimate: 🟢

- [x] ✅ **gm-state context.md if Phase 5 discharged owed MP items** - Completed 2026-08-15
  - Description: Record any log-based MP checks Phase 5 covered
  - File(s): `docs/features/gm/gm-state/context.md`
  - Estimate: 🟢

---

## Bugs & Issues

**Active Bugs:**
- (none)

**Fixed Bugs:**
- (none)

---

## Technical Debt

- (none yet)

---

*Update this file as tasks are completed. Mark tasks with [x] immediately when done. Add new tasks as they're discovered.*
