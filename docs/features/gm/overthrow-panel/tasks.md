# Overthrow Panel — Task Checklist

**Feature:** gm/overthrow-panel (epic `gm`, feature 2 of 5)
**Last Updated:** 2026-08-16
**Progress:** 26/26 tasks complete (100%) — ✅ closed 2026-08-16; the absorbed gm-state MP checklist confirmed by user (batched epic test session)

> Agent tiers per `implementation.md` §4: **no phase needs an advanced agent.** Phases 1–3 →
> `ui-developer`; Phase 0 → orchestrator (no agent); Phase 4 → user-driven; Phase 5 →
> orchestrator/`component-developer`.

---

## Phase 0: Baseline & doc-bug fix (3/3 complete) — S, no agent

- [x] ✅ **Record baseline in context.md**
  - Description: `compile-check.sh` exit 0 + file count; `git rev-parse --short HEAD` (plan cited `462308f5` — re-verify seam citations against current tree); highest allocated bug id (`ls docs/bugs/`, BUG-167 at planning)
  - File(s): `docs/features/gm/overthrow-panel/context.md`
  - Estimate: 🟢

- [x] ✅ **Re-grep GUID series `{6B08…}` free**
  - Description: 0 hits across `Prefabs`, `Configs`, `Scripts`, `UI` required before minting; spares `{6B09…}`, `{6B0A…}`
  - File(s): n/a (grep gate)
  - Estimate: 🟢

- [x] ✅ **Fix gm-state doc bug (I-4)**
  - Description: Replace `IsDistributionSuppressedByQRF()` / `IsPayoutSuppressedByNoPlayers()` with shipped names `IsDistributionSuppressed()` / `IsPayoutSuppressed()` — verify shipped names in `OVT_GMCampaignState.c` first
  - File(s): `docs/features/gm/gm-state/context.md:140`
  - Estimate: 🟢

---

## Phase 1: Injection, layout, chrome, empty state (6/6 complete) — M, `ui-developer`

- [x] ✅ **Modded injection point**
  - Description: `modded class SCR_EditModeEditorUIComponent`, override `HandlerAttachedScripted(Widget w)` (keep `protected`, call `super`), find `Mode_Edit_Element_Left` (null-check, quiet return), `CreateWidgets(PANEL_LAYOUT, left)` (null-check + one WARNING). **Append and stop — no child reordering** (§5 D2)
  - File(s): `Scripts/Game/UI/Modded/SCR_EditModeEditorUIComponent.c` (new)
  - Estimate: 🟢

- [x] ✅ **GMPanel.layout + .meta**
  - Description: New dir `UI/Layouts/GM/`; mint layout + widget GUIDs from `{6B08…}`; `.meta` with all five console configurations; run duplicate-GUID script from `overthrow-ui-patterns/layouts.md`
  - File(s): `UI/Layouts/GM/GMPanel.layout`, `UI/Layouts/GM/GMPanel.layout.meta` (new)
  - Estimate: 🟡

- [x] ✅ **Widget tree: all rows pre-authored & named, chrome matched**
  - Description: Names per plan §4 Phase 1 task 3 (`CampaignSection`, `NoDataLabel`, `LocalSection`, `DetailSection`, `Value_*`, `Status_*`); chrome per §3.4 (bg `0.007 0.012 0.014 1`, width 380, padding `36 16 36 16` — the GM variant's values); logo header; two TextWidgets per countdown row
  - File(s): `UI/Layouts/GM/GMPanel.layout`
  - Estimate: 🟡

- [x] ✅ **OVT_GMPanelUIComponent skeleton**
  - Description: extends `SCR_ScriptedWidgetComponent`; `HandlerAttached` caches named widgets, sets static instance, renders empty state; `HandlerDeattached` full hygiene (unsubs, CallQueue.Remove, null statics + m_wRoot). No data binding yet
  - File(s): `Scripts/Game/UI/GM/OVT_GMPanelUIComponent.c` (new)
  - Estimate: 🟡

- [x] ✅ **Localization strings**
  - Description: All `#OVT-GMPanel_*` items into the `.st` master with `Comment` filled; **never** touch `localization_Overthrow.<lang>.conf`
  - File(s): `Language/localization_Overthrow.st`
  - Estimate: 🟢

- [x] ✅ 🖐️ **USER: Workbench visual round-trip**
  - Description: Regenerate localization exports; Play mode → GM → EDIT mode: panel above mode menu? chrome parity? logo scale? five open/close cycles clean? Absent in Photo mode?
  - File(s): n/a (human verification — may be deferred to Phase 4 batch)
  - Estimate: 🟡 (user)

---

## Phase 2: Data binding, countdowns, suppression (8/8 complete) — M, `ui-developer`

- [x] ✅ **OVT_GMPanelFormat.c pure statics**
  - Description: `FormatCountdown(float)` (`1:23:45`/`12:34`/`0:07`, clamp negatives to `0:00`), `FormatThreat(float)` (one decimal). World-free — no `GetGame()`/`OVT_Global` in code **or comments**
  - File(s): `Scripts/Game/UI/GM/OVT_GMPanelFormat.c` (new)
  - Estimate: 🟢

- [x] ✅ **Bind campaign rows**
  - Description: Resolve seam per render (`OVT_ControllerComponent<OVT_GMRequestComponent>.Get()`, null → ShowNoData; `state.HasData()` false → ShowNoData); fill Value_Threat/OFResources/OFDeployment/Distribution/Payout
  - File(s): `Scripts/Game/UI/GM/OVT_GMPanelUIComponent.c`
  - Estimate: 🟡

- [x] ✅ **Three subscriptions with removal**
  - Description: `GetOnSnapshotUpdated()`→RenderAll, `GetOnStateCleared()`→ShowNoData, `m_OnResistanceMoneyChanged`→one SetText; all inserted in HandlerAttached, removed in HandlerDeattached
  - File(s): `Scripts/Game/UI/GM/OVT_GMPanelUIComponent.c`
  - Estimate: 🟢

- [x] ✅ **One-second tick**
  - Description: `CallLater(TickCountdowns, 1000, true)`; first line self-cancel guard (`!m_wRoot` → Remove(self)); updates ONLY `Status_Distribution`, `Status_Payout`, `Value_QRF` — SetText only, no visibility/layout changes
  - File(s): `Scripts/Game/UI/GM/OVT_GMPanelUIComponent.c`
  - Estimate: 🟢

- [x] ✅ **Suppression rendering**
  - Description: `IsDistributionSuppressed()` → reason in accent colour `0.761 0.392 0.08 1` instead of countdown; same for payout; amount stays visible
  - File(s): `Scripts/Game/UI/GM/OVT_GMPanelUIComponent.c`
  - Estimate: 🟢

- [x] ✅ **QRF row**
  - Description: "None" when `!m_bQRFActive`; else points + timer (ms→s) + town name when `m_iCurrentQRFTown >= 0`; null-check `GetOccupyingFaction()` and `GetTowns()`
  - File(s): `Scripts/Game/UI/GM/OVT_GMPanelUIComponent.c`
  - Estimate: 🟢

- [x] ✅ **Logic-tier test + prove-can-fail**
  - Description: Cases per plan §7 table (7 cases: under-minute, minutes, hour switch, 3599, zero, negative clamp, threat precision); self-registers via `[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]`; invert each expectation → named SetFailure → revert; record method in context.md
  - File(s): `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_GMPanelFormat.c` (new)
  - Estimate: 🟡

- [x] ✅ **Grep gates Q-2/Q-6/Q-7**
  - Description: Zero `Rpc(`/`[RplProp]`/`RplRcver`/`RplComponent` in feature files; no `SetVisible`/`CreateWidgets`/`RemoveFromHierarchy`/`AddChild` in tick path; every manager/seam accessor null-checked. Paste outputs into context.md
  - File(s): n/a (gates)
  - Estimate: 🟢

---

## Phase 3: Detail seam for hud-icons (4/4 complete) — S, `ui-developer`

- [x] ✅ **DetailSection empty container**
  - Description: Named, `Visible 0` at author time, occupies no vertical space when hidden
  - File(s): `UI/Layouts/GM/GMPanel.layout`
  - Estimate: 🟢

- [x] ✅ **Widget-level API**
  - Description: `GetDetailSlot()`, `ShowDetail(bool)`, `ClearDetail()` — Doxygen `//!` contract on each; no placeholder content, no data types
  - File(s): `Scripts/Game/UI/GM/OVT_GMPanelUIComponent.c`
  - Estimate: 🟢

- [x] ✅ **GetInstance() static**
  - Description: Set in HandlerAttached, nulled in HandlerDeattached; documented "may be null; never cache across frames"
  - File(s): `Scripts/Game/UI/GM/OVT_GMPanelUIComponent.c`
  - Estimate: 🟢

- [x] ✅ **Write the seam contract into context.md**
  - Description: The four calls, null-may-happen rule, panel makes no assumption about content — readable standalone by hud-icons' planner
  - File(s): `docs/features/gm/overthrow-panel/context.md`
  - Estimate: 🟢

---

## Phase 4: Verification gate — MP script + absorbed gm-state Phase 5 (1/2 — checklist log items deferred) — user-driven

- [x] ✅ 🖐️ **USER: MP verification script (plan §7)** — verified by user on own server 2026-08-15, all panel checks pass
  - Description: Dedicated server + client; establish auth path FIRST (admin login vs `-ovtGmDev` — record it); F-1…F-7 panel checks; ⚠️ warn before launching (windows on desktop)
  - File(s): results → `docs/features/gm/overthrow-panel/context.md`
  - Estimate: 🔴 (user)

- [x] ✅ 🖐️ **USER: absorbed gm-state Phase 5 checklist** - Completed: user confirmed 2026-08-16 the remaining items were covered in the epic's batched MP/JIP test session (with hud-icons); all tests green. Log measurements not recorded separately
  - Description: 8-row split table (plan §7 step 5) — what the panel proves vs what needs a log (`m_bDebugSnapshotTiming`); record counts + build ms; non-admin negative path; JIP; listen-server host
  - File(s): results → `docs/features/gm/gm-state/context.md`
  - Estimate: 🔴 (user)

---

## Phase 5: context.md & epic bookkeeping (3/3 complete) — S

- [x] ✅ **Final overthrow-panel context.md**
  - Description: Shipped row list + sources; Phase 1 Workbench findings (did D2 ordering hold?); detail-seam contract; Phase 4 results; "panel shows nothing" triage chain
  - File(s): `docs/features/gm/overthrow-panel/context.md`
  - Estimate: 🟢

- [x] ✅ **Update gm-state context.md**
  - Description: Mark Phase 5 discharged (or honestly deferred), auth path exercised, measured counts/build ms
  - File(s): `docs/features/gm/gm-state/context.md`
  - Estimate: 🟢

- [x] ✅ **Update epic-overview.md**
  - Description: Feature 2 status + task counts; feature 1 → fully built (if Phase 4 ran). **No help-docs phase** (deferred to epic end, `epic-overview.md:72-74`)
  - File(s): `docs/features/gm/epic-overview.md`
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
