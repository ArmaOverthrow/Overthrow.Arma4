# Overthrow Panel — Implementation Plan

**Status:** Planning
**Epic:** gm (feature 2 of 5 — Phase 1 of the 3-phase epic)
**Started:** 2026-08-14
**Target Completion:** TBD
**Last Updated:** 2026-08-14 18:19 AEST

> All `file:line` citations are load-bearing. Overthrow-side citations were verified against the working
> tree at **`462308f5`** (`feat: gm/gm-state`, the commit that built the seam this feature consumes);
> base-game citations against the Reforger 1.8 reference tree at `/mnt/n/Projects/Arma 4/ArmaReforger`.
> Keep them when editing. Where this plan and `requirements.md` disagree, **this plan wins** and the
> disagreement is recorded in §5.

---

## 1. Executive Summary

`gm-state` built a transport with no renderer. A Game Master today can open the editor and the seam will
faithfully stream threat, both occupying-faction resource pools, two countdowns and a hundred-odd per-entity
records to their machine — where nothing displays them. This feature is the first thing that looks at that
data, and it is deliberately the **smallest** thing that can: one read-only panel docked in the editor's
bottom-left stack, above the base game's mode/settings menu, showing campaign-wide numbers.

**The shape, in one paragraph.** The base game's `SCR_EditModeEditorUIComponent`
(`ArmaReforger/scripts/Game/Editor/UI/Components/Modes/SCR_EditModeEditorUIComponent.c:1`) is an **empty
class** referenced by exactly one layout in the entire base tree — `Mode_Edit.layout:4`, the EDIT-mode root.
A `modded class` override of its `HandlerAttachedScripted(Widget w)` hook is therefore a zero-fork injection
point that fires exactly once per EDIT-mode activation, with the mode root in hand. It finds
`Mode_Edit_Element_Left` (`Mode_Edit.layout:257`), creates one mod-owned layout under it, and stops. The
layout's root carries `OVT_GMPanelUIComponent`, which resolves the gm-state seam through
`OVT_ControllerComponent<OVT_GMRequestComponent>.Get()`, subscribes to its two invokers, reads the store,
and writes text into pre-authored named rows. A one-second `CallLater` re-renders only the two countdown
strings between polls.

**Four facts found during planning change the work and are worth stating up front:**

1. **"Visible only in the unlimited GM editor" costs zero script.** `Mode_Edit.layout` is the
   `m_HideableLayout` of `EditorModeEdit.et` (`ArmaReforger/Prefabs/Editor/Modes/EditorModeEdit.et:211`),
   and that prefab is the only editor mode that does **not** set `m_bIsLimited 1` — `EditorModePhoto.et:362`,
   `EditorModeBuilding.et:440` and `EditorModeSaveScreenshot.et:357` all do. Injecting into
   `SCR_EditModeEditorUIComponent` therefore restricts the panel to the unlimited editor **structurally**.
   No `IsLimited()` check, no role check, no visibility toggle is needed or wanted in this feature.

2. **The widget lifetime is engine-scoped, and the engine creates and destroys it.**
   `SCR_MenuLayoutEditorComponent.EOnEditorPostActivate()` calls `CreateWidgets(m_HideableLayout, …)`
   (`ArmaReforger/scripts/Game/Editor/Components/Editor/SCR_MenuLayoutEditorComponent.c:87`) and
   `EOnEditorPostDeactivate()` calls `RemoveFromHierarchy()` on it (`:109`). Our panel is a child of that
   tree, so it is built and torn down with EDIT mode. The panel does **zero** lifecycle plumbing.

3. **The re-append recipe in the task brief is wrong, and the correct behaviour is provable from base-game
   source.** `"Fill Origin" Bottom` on a `VerticalLayoutWidget` places the **first** child at the bottom and
   stacks later children **upward**. Proof: `ChatPanel.layout:52` is such a container; its first authored
   child is the chat edit box (`:55`), message-line widgets are appended after it
   (`SCR_ChatPanel.c:319`), and vanilla's own comment on the fill loop reads `// Widget 0 is at the bottom`
   (`SCR_ChatPanel.c:407`) — i.e. input at the bottom, history stacking upward, which is what the chat looks
   like on screen. Applied here: `ModeMenu_GameMaster0` is `Mode_Edit_Element_Left`'s authored first child
   (`Mode_Edit.layout:272`), so a panel appended by `CreateWidgets` lands **above** it with **no reordering
   at all**. `RemoveChild`/`AddChild` re-appending the mode menu would *invert* the desired order, not fix
   it. See §5 D2 — the Workbench check stays, but it now confirms an expectation rather than resolving a
   coin flip.

4. **Two documented method names on the seam do not exist.** `gm-state/context.md:140` advertises
   `IsDistributionSuppressedByQRF()` / `IsPayoutSuppressedByNoPlayers()`; the shipped methods are
   `IsDistributionSuppressed()` (`OVT_GMCampaignState.c:98`) and `IsPayoutSuppressed()` (`:106`). Fixing
   that doc line is a Phase 0 task here, before an implementer is misled by it.

---

## 2. Goals

### Primary

1. **A GM sees the campaign's hidden numbers without a debug print.** Threat, OF reserve, OF deployment
   pool, next distribution (amount + countdown), next payout (amount + countdown), resistance funds, QRF
   status — in one glance, in the editor, docked where a GM already looks.
2. **A countdown that will not fire says so.** Suppression is rendered, not implied. This is the single
   highest-value row on the panel: without it a GM watches a countdown reach zero, sees nothing happen, and
   files a bug (`gm-state/context.md:124-126`).
3. **"No data" is a first-class state, not a screen full of zeros.** The store is empty before the first
   snapshot, and stays empty for an unauthorized player. Zeros would be a lie.
4. **It looks like it belongs.** Same background colour, same width, same padding as the mode menu it docks
   above (§5 D3). A GM should not be able to tell which panel the base game shipped.
5. **Zero base-game files forked.** One `modded class` over an empty base class; everything else is
   mod-owned.
6. **It is the real readout that discharges gm-state's deferred MP gate.** gm-state's Phase 5 checklist was
   deliberately parked until this panel existed (`gm-state/context.md:26`); §7 absorbs it, honestly split
   into what the panel proves and what still needs a log.

### Secondary

7. **A documented, empty seam for `hud-icons`** — a named container and three widget-level methods, with no
   data contract and no placeholder content (§5 D5).
8. **The one piece of real logic is unit-tested.** Countdown formatting is a pure static with Logic-tier
   coverage; everything else in this feature is widget writes that no suite can see.

### Explicit non-goals

- **No actions.** Epic Phase 1 is strictly read-only. No refresh button (user-declined), no buttons at all.
- **No record rendering.** Bases, upgrades, deployments and groups are in the store and stay there; they are
  `hud-icons`' and `gm-map`'s to draw.
- **No detail views.** The detail seam ships empty (§5 D5).
- **No truncation indicator** (user-declined) and **no new networking of any kind** (§5 D4).
- **No help/wiki phase.** All Phase 1 help and wiki work is one consolidated `help-docs-sync` pass after
  `gm-map` closes the phase — recorded in `epic-overview.md:72-74`. Do not add one here.

---

## 3. Architecture Overview

### 3.1 Component hierarchy

```
SCR_EditModeEditorUIComponent                          BASE GAME — empty class, one layout reference
└── modded (Scripts/Game/UI/Modded/SCR_EditModeEditorUIComponent.c)     NEW  ~25 lines
       override HandlerAttachedScripted(Widget w)
         super.HandlerAttachedScripted(w)              ← hides hints; do not drop it
         left = w.FindAnyWidget("Mode_Edit_Element_Left")
         GetGame().GetWorkspace().CreateWidgets(PANEL_LAYOUT, left)     ← appends: lands ABOVE the menu

UI/Layouts/GM/GMPanel.layout                           NEW  mod-owned, new GUID series
└── root VerticalLayoutWidgetClass
      components { OVT_GMPanelUIComponent "{…}" {} }
      ├── Header       ImageWidget  logo_overthrow.edds
      ├── CampaignSection   (named container — hidden when the store is empty)
      │     Row_Threat / Row_OFResources / Row_OFDeployment
      │     Row_Distribution  (Value_* amount + Status_* "in 12:34" | suppression)
      │     Row_Payout        (same shape)
      ├── NoDataLabel   TextWidget  (shown when the store is empty)
      ├── LocalSection      (always live — locally replicated sources)
      │     Row_Funds / Row_QRF
      └── DetailSection (named, initially collapsed, EMPTY — hud-icons fills it)

Scripts/Game/UI/GM/OVT_GMPanelUIComponent.c            NEW  : SCR_ScriptedWidgetComponent
Scripts/Game/UI/GM/OVT_GMPanelFormat.c                 NEW  PURE statics — world-free, Logic-testable
```

Nothing here is a Manager and nothing is a Controller. There is no new entity, no new prefab, no new
persistence, and **no new replication** — the panel is a renderer over one existing client-side store plus
two already-replicated managers.

### 3.2 Data flow, one EDIT-mode session

```
EDIT mode activates
  SCR_MenuLayoutEditorComponent.EOnEditorPostActivate  (:87)  CreateWidgets(Mode_Edit.layout)
    └─ SCR_EditModeEditorUIComponent.HandlerAttached          (MenuRootSubComponent.c:58)
         └─ CallLater(HandlerAttachedScripted, 0)             (:82 — deferred one frame; tree is complete)
              └─ MODDED override: CreateWidgets(GMPanel.layout, Mode_Edit_Element_Left)
                   └─ OVT_GMPanelUIComponent.HandlerAttached
                        ├─ resolve gm = OVT_ControllerComponent<OVT_GMRequestComponent>.Get()   (may be NULL)
                        ├─ gm.GetOnSnapshotUpdated().Insert(OnSnapshot)
                        ├─ gm.GetOnStateCleared().Insert(OnCleared)
                        ├─ economy.m_OnResistanceMoneyChanged.Insert(OnFundsChanged)
                        ├─ RenderAll()                    ← read state NOW; do not wait for an invoker
                        └─ CallLater(TickCountdowns, 1000, repeat)

   ~every 8 s   gm-state commits a snapshot ──► OnSnapshot() ──► RenderAll()
   ~every 1 s   TickCountdowns() ──► two SetText calls, nothing else
   on change    m_OnResistanceMoneyChanged ──► one SetText call
   ~every 1 s   TickCountdowns() also refreshes the QRF row (m_iQRFTimer is broadcast, not invoked)

EDIT mode deactivates  (mode switch OR editor close)
  EOnEditorPostDeactivate (:109) RemoveFromHierarchy
    └─ OVT_GMPanelUIComponent.HandlerDeattached
         ├─ remove the CallLater
         ├─ remove all three subscriptions
         └─ null m_wRoot and the static instance
```

**Two ordering properties fall out of this and both matter.**

- The panel attaches *before* the first snapshot commits in the common case (network RTT beats a frame), so
  **the empty state is the normal first frame**, not an error path.
- Switching EDIT → PHOTO → EDIT destroys and rebuilds the panel while the editor stays open and the seam
  keeps polling. That is why `HandlerAttached` calls `RenderAll()` immediately instead of waiting for
  `GetOnSnapshotUpdated()` — otherwise the rebuilt panel would sit empty for up to 8 seconds with live data
  already in the store.

### 3.3 Where every row's value comes from

| Row | Source | Citation |
|---|---|---|
| Threat | `state.m_fThreat` (float, full precision) | `OVT_GMCampaignState.c:39` |
| OF Resources | `state.m_iOFResources` | `:42` |
| Deployment Pool | `state.m_iOFDeploymentResources` | `:45` |
| Next Distribution — amount | `state.m_iDistributionAmount` | `:52` |
| Next Distribution — countdown | `state.GetDistributionSecondsRemaining()` | `:115` |
| Next Distribution — suppressed | `state.IsDistributionSuppressed()` | `:98` |
| Next Payout — amount | `state.m_iPayoutAmount` | `:58` |
| Next Payout — countdown | `state.GetPayoutSecondsRemaining()` | `:124` |
| Next Payout — suppressed | `state.IsPayoutSuppressed()` | `:106` |
| Empty-state test | `state.HasData()` | `:90` |
| Resistance Funds | `OVT_Global.GetEconomy().GetResistanceMoney()`, live via `m_OnResistanceMoneyChanged` | `OVT_EconomyManagerComponent.c:1239`, `:109` |
| QRF active | `OVT_Global.GetOccupyingFaction().m_bQRFActive` | `OVT_OccupyingFactionManager.c:161` |
| QRF points | `.m_iQRFPoints` | `:165` |
| QRF timer | `.m_iQRFTimer` — **milliseconds** | `:166`; unit proven by `OVT_EconomyInfo.c:288` dividing by 1000 |
| QRF town target | `.m_iCurrentQRFTown` → `OVT_Global.GetTowns().GetTownName(id)` | `:164`, `OVT_TownManagerComponent.c:858` |

The QRF block is **already broadcast to every client** — RPCs at `OVT_OccupyingFactionManager.c:1599-1656`,
JIP at `:1542-1545` — so reading it costs nothing new and violates no gate. gm-state's grep gate F-7
forbids resistance funds and QRF from travelling on *its* wire precisely because they already travel on
this one (`gm-state/context.md:155-164`).

**A base-targeted QRF renders without a target name**: `OVT_BaseData` carries `id`, `faction`, `location`
and upgrade lists but **no name** (`OVT_OccupyingFactionManager.c:20-38`). Inventing one here would
duplicate work `hud-icons` and `gm-map` own. Town QRFs get a name because it is one existing call.

### 3.4 Chrome, matched to the mode menu

| Property | Value | Citation |
|---|---|---|
| Background colour | `0.007 0.012 0.014 1` | `ArmaReforger/UI/layouts/Editor/Toolbar/ModeMenu/ModeMenu.layout:21` |
| Width | `380` (`SizeLayoutWidgetClass` `WidthOverride`) | `ModeMenu.layout:30`; the GM variant re-asserts 380 at `ModeMenu_GameMaster.layout:10` |
| Inner padding | `36 16 36 16` | `ModeMenu_GameMaster.layout:20-22` — **the GM variant overrides** `ModeMenu.layout:39`'s `36 24 36 18`; match the variant, it is what sits directly below us |
| Slot padding in the left stack | `24 0 0 8` | mirrors the mode menu's `Padding 24 0 0 0` (`Mode_Edit.layout:276`) plus an 8 px gap below our panel |
| Logo | `{4A7D823D7914A85A}UI/Textures/logo_overthrow.edds` | usage precedent `UI/Layouts/Menu/MainMenu.layout:113`, `Size 150 300` at 280×280 — scale down for a 380-wide panel header |
| Accent (suppression text) | `0.761 0.392 0.08 1` | Overthrow accent, `overthrow-ui-patterns/layouts.md` |

---

## 4. Implementation Phases

Effort is **S / M / L** relative to one focused session. "Agent" is the routing hint for `/proceed`.

> **No phase needs an advanced agent.** This is a contained UI feature: one modded override, one layout, one
> widget component, two pure statics. `ui-developer` fits Phases 1–3; `component-developer` fits the docs
> phase. If Phase 1's injection fails in Workbench, that is a **finding to report**, not a reason to escalate
> an agent tier.

---

### Phase 0 — Baseline and the doc-bug fix — **S — no agent**

Record in `context.md` before any code:

| Gate | How |
|---|---|
| `tools/compile-check.sh` | exit 0 + file count |
| `git status` / `git rev-parse --short HEAD` | plan citations were taken at **`462308f5`**; re-check, this tree receives concurrent bugfix commits |
| Highest allocated bug id | `ls docs/bugs/` — **BUG-167** at planning time |
| Free GUID series | **`{6B08…}` proven free** — 0 hits across `Prefabs`, `Configs`, `Scripts`, `UI` at planning time (`{6B09…}`, `{6B0A…}` also free as spares). **Re-grep before minting.** |
| Seam citations resolve | `OVT_GMCampaignState.c:25/:90/:98/:106/:115/:124`, `OVT_GMRequestComponent.c:48/:155/:164/:174` |

**Task:** fix `docs/features/gm/gm-state/context.md:140` — replace `IsDistributionSuppressedByQRF()` /
`IsPayoutSuppressedByNoPlayers()` with the real names `IsDistributionSuppressed()` /
`IsPayoutSuppressed()`. One line; do it before any implementer reads it.

**Do NOT run `tools/run-tests.sh`.** Planning and implementation stop at `compile-check.sh` exit 0; the
orchestrator runs suites after a phase completes (`.claude/test-policy.md`).

**Acceptance:** baseline table filled; the context.md line reads the shipped names.

---

### Phase 1 — Injection, layout, chrome and the empty state — **M — `ui-developer`**

> The smallest visible slice, and the one carrying every unknown. When this lands, a correctly-chromed
> panel with a logo and a "waiting for campaign data" line sits above the mode menu in EDIT mode, with no
> data binding at all. Everything after it is text writes.

**Tasks**

1. `Scripts/Game/UI/Modded/SCR_EditModeEditorUIComponent.c` (folder convention: `Scripts/Game/UI/Modded/`,
   matching `SCR_FieldManualUI.c`, `SCR_GroupSubMenuBase.c`):
   ```
   modded class SCR_EditModeEditorUIComponent
   {
       protected static const ResourceName PANEL_LAYOUT = "{…}UI/Layouts/GM/GMPanel.layout";

       override protected void HandlerAttachedScripted(Widget w)
       {
           super.HandlerAttachedScripted(w);     // SCR_BaseModeEditorUIComponent hides the hint — keep it
           …
       }
   }
   ```
   - `super` is **mandatory**: the base does real work
     (`SCR_BaseModeEditorUIComponent.c:6` → `SCR_HintManagerComponent.HideHint()`).
   - Keep the `protected` qualifier — the base declares `override protected void`
     (`SCR_BaseModeEditorUIComponent.c:4`).
   - `Widget left = w.FindAnyWidget("Mode_Edit_Element_Left");` — null-check and return quietly if the base
     game renames it; a missing panel is acceptable, a script error in the editor root is not.
   - `GetGame().GetWorkspace().CreateWidgets(PANEL_LAYOUT, left)`
     (proto: `ArmaReforger/scripts/Core/generated/UI/WorkspaceWidget.c:75`). Null-check the result and log
     one `LogLevel.WARNING` if it fails — an unresolvable layout is otherwise completely silent.
   - **Do not reorder children.** §5 D2. If the Workbench check in task 6 contradicts the citation, the fix
     is the two-line `left.RemoveChild(modeMenu); left.AddChild(modeMenu);`
     (`ArmaReforger/scripts/Core/generated/UI/Widget.c:48` / `:37`) — but write it only if reality demands it,
     and record that it was needed.
2. `UI/Layouts/GM/GMPanel.layout` + **`UI/Layouts/GM/GMPanel.layout.meta`**. New directory. Mint the layout
   resource GUID and all widget-instance GUIDs from the `{6B08…}` series; slot GUIDs are copied from
   sibling widgets and repeat freely. **A missing `.meta` makes the layout unresolvable** and
   `compile-check.sh` cannot see it — follow `overthrow-ui-patterns/layouts.md` exactly, keep all five
   console configurations, and run the duplicate-GUID script from that document before finishing.
3. Author the widget tree of §3.1 with **every row pre-authored and named** — no dynamic instantiation
   anywhere. Names to be findable from script:
   `CampaignSection`, `NoDataLabel`, `LocalSection`, `DetailSection`,
   `Value_Threat`, `Value_OFResources`, `Value_OFDeployment`,
   `Value_Distribution`, `Status_Distribution`, `Value_Payout`, `Status_Payout`,
   `Value_Funds`, `Value_QRF`.
   Chrome per §3.4. Two `TextWidget`s per countdown row (amount + status) so suppression is a **text and
   colour swap, never a visibility toggle** — see the Quality bar on layout thrash.
4. `Scripts/Game/UI/GM/OVT_GMPanelUIComponent.c` extending `SCR_ScriptedWidgetComponent`
   (Overthrow's own base for widget components — `OVT_ShopMenuCardComponent.c:9` et al; it stores `m_wRoot`
   in `HandlerAttached` and exposes `GetRootWidget()`).
   - `HandlerAttached`: `super`, cache every named widget once into fields, set the static instance, render
     the empty state.
   - `HandlerDeattached`: unsubscribe everything, `GetGame().GetCallqueue().Remove(...)`, null the static,
     null `m_wRoot`. Hygiene template: `SCR_BudgetEditorUIComponent.c` (`HandlerAttachedScripted` :207 /
     `HandlerDeattached` :240 — the editor-UI component that already does exactly this pairing).
   - No data binding in this phase.
5. **Strings.** Add every `#OVT-GMPanel_*` item to `Language/localization_Overthrow.st` in this phase, with
   `Comment` filled in (it is the only context a translator gets). **Never touch**
   `Language/localization_Overthrow.<lang>.conf` — Workbench-generated, hand-editing corrupts them silently.
   §5 D7 records why keys are used from the start rather than literals.
6. 🖐️ **USER ROUND-TRIP — Workbench.** The user opens the mod in Workbench, **regenerates the localization
   exports** (otherwise every label renders as a raw key and the visual check is worthless), enters Play
   mode, opens Game Master, and reports:
   - Does the panel appear at all? Above or below the mode menu? (§5 D2's confirmation)
   - Is the chrome indistinguishable from the mode menu — colour, width, padding, left alignment?
   - Does the logo read at panel width, or does it need a smaller `Size`?
   - Open and close the editor **five times**: any script errors, any duplicated panels, any leftover
     widgets? (§9 R1's evidence)

**Acceptance**

- `tools/compile-check.sh` → exit **0**, file count +3.
- Duplicate-GUID script over `GMPanel.layout` reports none; `.meta` present with all five configurations.
- Zero base-game files modified — `git status` shows only new mod files plus the `.st`.
- User confirms: panel visible in EDIT mode, **above** the mode menu, chrome matched, five open/close cycles
  clean.
- Panel **does not appear** in Photo mode (structural, per §1 fact 1 — confirm it, do not assume it).

---

### Phase 2 — Data binding, countdowns and suppression — **M — `ui-developer`**

**Tasks**

1. `Scripts/Game/UI/GM/OVT_GMPanelFormat.c` — pure statics, **world-free**:
   - `static string FormatCountdown(float seconds)` → `"1:23:45"` above an hour, `"12:34"` below, `"0:07"`
     under a minute; negative and NaN-ish inputs clamp to `"0:00"`.
   - `static string FormatThreat(float threat)` → one decimal place, so a GM sees `3.9` where
     `GetThreatLevel()` would have truncated to `3` (`OVT_OccupyingFactionManager.c:1134` vs `:1141`).
   - ⚠️ **Logic-tier rule:** the directory-wide grep that guards `TestSuites/Logic/` does not distinguish
     code from prose. This file is *not* in that directory, but its test is — so keep `GetGame()`,
     `OVT_Global` and any world reference out of **both**, including comments. A previous feature tripped
     exactly this by quoting the rule.
2. Bind the campaign rows. Resolve the seam **on every render**, never cached across renders:
   ```
   OVT_GMRequestComponent gm = OVT_ControllerComponent<OVT_GMRequestComponent>.Get();
   if (!gm) { ShowNoData(); return; }
   OVT_GMCampaignState state = gm.GetState();   // never null
   if (!state.HasData()) { ShowNoData(); return; }
   ```
   `Get()` returns null on a dedicated server, before ownership assignment, and if the prefab block is
   missing — all documented at `OVT_ControllerComponent.c:22-29`, all requiring a null check.
3. **Subscriptions**, all three inserted in `HandlerAttached` and removed in `HandlerDeattached`:
   `gm.GetOnSnapshotUpdated()` → `RenderAll()`; `gm.GetOnStateCleared()` → `ShowNoData()`;
   `OVT_Global.GetEconomy().m_OnResistanceMoneyChanged` → one `SetText`.
   Both gm-state invokers are argument-less: read `GetState()` inside the handler
   (`OVT_GMRequestComponent.c:164`, `:174`).
4. **The one-second tick.** `CallLater(TickCountdowns, 1000, true)`. It updates **only**
   `Status_Distribution`, `Status_Payout` and `Value_QRF` — three `SetText` calls, no visibility changes, no
   re-layout. `GetDistributionSecondsRemaining()` / `GetPayoutSecondsRemaining()` extrapolate locally
   against world time and clamp at 0, so they are safe to call at any rate
   (`OVT_GMCampaignState.c:249-262`) — the 8 s poll re-syncs them with no visible jump.
   **First line of the tick is a self-cancel guard:** `if (!m_wRoot) { remove self; return; }` (§9 R1).
5. **Suppression rendering.** When `IsDistributionSuppressed()`, `Status_Distribution` shows the suppression
   reason in the accent colour instead of the countdown; same for the payout. The amount stays visible —
   a GM wants to know what *would* have landed.
6. **Both countdowns currently carry the identical value** (both loops fire on the same in-game 6-hour
   marks; two wire fields exist for future divergence — `gm-state/context.md:84`). Render both as designed;
   **do not collapse them**, and do not write copy that implies they are independent clocks
   ("Next distribution" / "Next payout", not "in X minutes, then Y minutes later").
7. **QRF row.** `Value_QRF` shows the localized "None" when `!m_bQRFActive`; otherwise points + timer
   (milliseconds → seconds, per `OVT_EconomyInfo.c:288`) plus the town name when `m_iCurrentQRFTown >= 0`.
   Null-check `OVT_Global.GetOccupyingFaction()` — a client can attach this panel before managers resolve.
8. `Scripts/Game/Tests/TestSuites/Logic/OVT_TEST_Logic_GMPanelFormat.c`. Cases in §7. Self-registers via
   `[Test(suite: OVT_TEST_LogicSuite, timeoutS: 30)]` — no edit to `OVT_TEST_LogicSuite.c`
   (`gm-state/context.md:74`).

**Acceptance**

- `tools/compile-check.sh` → exit **0**.
- Logic tier grows by the new case count; **every new case proven able to fail**, with the inversion method
  recorded in `context.md`.
- On a listen-server host with the editor open: every row shows a plausible live value, the two countdowns
  decrease once per second, and closing/reopening the editor returns to the empty state and back.
- Grep proves **zero** `[RplProp]`, zero `Rpc(`, zero `RplRcver` anywhere in this feature's files — the
  panel adds no networking (§5 D4).

---

### Phase 3 — The detail seam for `hud-icons` — **S — `ui-developer`**

> Empty on purpose. `hud-icons` owns the contract for what goes in it; this feature owns only the slot's
> existence and lifecycle. **No placeholder views, no speculative data shape** (§5 D5).

**Tasks**

1. `DetailSection` in the layout: a named, empty container, `Visible 0` at author time.
2. Three methods on `OVT_GMPanelUIComponent`, each with a Doxygen `//!` block stating the contract:
   - `Widget GetDetailSlot()` — the container to parent content into. Never creates anything.
   - `void ShowDetail(bool show)` — visibility of the section.
   - `void ClearDetail()` — removes every child of the slot and hides the section.
3. `static OVT_GMPanelUIComponent GetInstance()` — set in `HandlerAttached`, **nulled in
   `HandlerDeattached`**. Documented as *may be null; never cache across frames* — the panel does not exist
   outside EDIT mode, which is exactly when `hud-icons` will be asking.
4. Write the contract into `context.md`: the four calls, the null-may-happen rule, and one sentence stating
   that the panel makes no assumption about what the content widget contains.

**Acceptance**

- `tools/compile-check.sh` → exit **0**.
- A reader of `context.md` with no other context can write `hud-icons`' fill code from it alone.
- `DetailSection` is invisible and occupies no vertical space in the Workbench check.

---

### Phase 4 — Verification gate (absorbs gm-state's deferred MP checklist) — **M — user-driven, no agent**

Run §7's Verification Method end to end. This is the only evidence that exists for the panel's real
behaviour and, by design, for gm-state's seam under multiplayer. ⚠️ **Warn the user before launching** —
client launches open a window on their desktop and can orphan.

---

### Phase 5 — `context.md` and epic bookkeeping — **S — `component-developer`**

**Tasks**

1. Write `docs/features/gm/overthrow-panel/context.md`: the shipped row list with its sources, the Phase 1
   Workbench findings (especially whether §5 D2's ordering held), the detail-seam contract, the measured MP
   results from Phase 4, and a "where to look when the panel shows nothing" triage section (mode is EDIT? →
   `Get()` returned null? → `HasData()` false? → gm-state's own triage at `gm-state/context.md:166-176`).
2. Update `docs/features/gm/gm-state/context.md`: mark Phase 5 discharged, record which auth path was
   exercised and the measured record counts / build ms from Phase 4.
3. Update `docs/features/gm/epic-overview.md`: feature 2 status, feature 1 → fully built. **Do not add a
   help-docs phase** — `epic-overview.md:72-74` records the deferral to epic end.

**Acceptance:** all three files updated; the epic table's task counts refreshed.

---

## 5. Key Technical Decisions

### D1 — Inject via `modded class SCR_EditModeEditorUIComponent` — **user decision**

The class is **empty** in the base game (`SCR_EditModeEditorUIComponent.c:1-3`), extends
`SCR_BaseModeEditorUIComponent` whose only member is a `HandlerAttachedScripted(Widget w)` hook receiving
the Mode_Edit root (`SCR_BaseModeEditorUIComponent.c:4-7`), and is referenced by exactly **one** layout in
the whole base tree (`Mode_Edit.layout:4`). That combination — empty, hooked, single-reference — is as close
to a purpose-built extension point as the base game offers, and it forks nothing.

**Two properties come free and are worth naming, because a later reader will otherwise re-derive them:**
- **EDIT-mode-only visibility** is structural (§1 fact 1). Photo, Building and Screenshot modes set
  `m_bIsLimited 1` and use different layouts; EDIT does not.
- **Lifetime is engine-managed** (§1 fact 2). `EOnEditorPostActivate` builds the tree, `EOnEditorPostDeactivate`
  destroys it. The panel never asks "is the editor open?".

The hook is also **deferred one frame** by `MenuRootSubComponent.HandlerAttached`
(`CallLater(HandlerAttachedScripted, 0, false, w)` — `MenuRootSubComponent.c:82`), so the full Mode_Edit
tree exists when it fires and `FindAnyWidget` is safe.

**Rejected:** forking `Mode_Edit.layout` (a base-game file, and a merge conflict on every Reforger update);
a same-GUID layout override (project memory: same-GUID overrides are *deltas*, which makes the override's
behaviour depend on base-game internals it does not control); and a `SCR_InfoDisplay` HUD element (wrong
lifetime — HUD displays are not editor-scoped and would need their own open/close plumbing).

### D2 — Append and stop; do **not** reorder children — **corrects the task brief**

The brief prescribed a defensive `left.RemoveChild(modeMenu); left.AddChild(modeMenu);` on the grounds that
`"Fill Origin" Bottom` child-order semantics were unknown and the re-append "fixes either outcome". It does
not — it *flips* the order, so it is right in exactly one of the two cases, and the case it is right in is
the one that is false.

**The semantics are provable from base-game source.** `ChatPanel.layout:52` is a `VerticalLayoutWidget`
with `"Fill Origin" Bottom`; its first authored child is the chat edit box (`:55`); message lines are
appended into the same container (`SCR_ChatPanel.c:319`); and the fill loop comments the mapping explicitly:
`int widgetId = i; // Widget 0 is at the bottom` (`SCR_ChatPanel.c:407`). On screen that is the input box at
the bottom with history stacking upward. **Therefore: earlier children are lower.**

Applied to `Mode_Edit_Element_Left` (`Mode_Edit.layout:257`, `"Fill Origin" Bottom` at `:270`), whose sole
authored child is `ModeMenu_GameMaster0` (`:272`): a widget appended by `CreateWidgets` becomes the last
child and renders **above** the mode menu. Which is the requirement.

**The Workbench check stays** (Phase 1 task 6) — it now confirms a citation rather than resolving a coin
flip, and the escape hatch is documented if reality disagrees.

### D3 — Match the mode menu's chrome exactly, and match the **GM variant** where they differ

Values and citations in §3.4. The one trap: `ModeMenu.layout:39` specifies padding `36 24 36 18`, but
`ModeMenu_GameMaster.layout:20-22` — the layout actually instantiated in `Mode_Edit_Element_Left` — overrides
it to `36 16 36 16`. Match the variant. The base value would leave our panel visibly looser than the widget
it sits on top of.

### D4 — Zero new networking; two data sources, both already on the client

The panel reads (a) the gm-state client store, and (b) two managers whose values are already replicated to
every client. It sends nothing and receives nothing of its own.

- **Resistance funds** are already broadcast with a change invoker
  (`OVT_EconomyManagerComponent.c:104`, `:109`, `:1239`) — gm-state's grep gate **F-7 forbids** them from
  its wire for exactly this reason (`gm-state/context.md:155-164`).
- **QRF** state is already broadcast (`OVT_OccupyingFactionManager.c:1599-1656`) and JIP-streamed (`:1542-1545`).

So the panel's DoD carries a grep: **no `Rpc(`, no `[RplProp]`, no `RplRcver` in any file this feature adds.**
A future contributor who "just needs one more number" must add it to gm-state's fan, not to the panel.

### D5 — The detail seam is an empty named container plus three methods — **user decision**

`requirements.md:20` asks the panel to show detail for a selected HUD icon. `hud-icons` is not built, so any
data contract written now would be invented. The panel therefore ships the **widget-level** half only:
a named collapsed container and `GetDetailSlot()` / `ShowDetail()` / `ClearDetail()`. `hud-icons` creates its
own layout into the slot and owns everything about its content.

**Rejected:** a placeholder detail view (dead code that will be deleted); a typed
`SetDetail(OVT_GMSomethingRecord)` API (invents `hud-icons`' shape before it exists, and record rendering is
explicitly out of scope).

### D6 — The empty state hides the campaign block only, not the whole panel

Threat, both resource pools and both countdowns come from the gm-state store and are genuinely unknown until
a snapshot lands. Resistance funds and QRF come from locally replicated managers and are known immediately,
snapshot or not. Blanking them alongside the campaign block would be a *different* lie from showing zeros.

So the layout carries two sections: `CampaignSection` (swapped with `NoDataLabel`) and `LocalSection`
(always live). This costs one extra named container and one extra visibility toggle **at a state
transition**, never per frame.

The empty state is reached in four situations, all normal, none an error: before the first snapshot (the
usual first frame), while the editor is shut, in a limited editor that never polls, and for a player the
server did not authorize (`gm-state/context.md:114-116`). The label must therefore read as "waiting", not as
"broken" — a GM whose gate refused them silently sees the same screen as one whose first poll is in flight,
and gm-state's triage (`context.md:166-176`) is where they go next.

### D7 — Localization keys from the start, with export regeneration folded into Phase 1's Workbench visit

Project law: new strings go in `Language/localization_Overthrow.st`; the runtime
`localization_Overthrow.<lang>.conf` exports are Workbench-generated and **must never be hand-edited** (six
were corrupted this way once). Until the user regenerates, a layout referencing a new key renders the raw
key on screen.

**The choice: use `#OVT-GMPanel_*` keys from the first commit** and make "regenerate the localization
exports" the *first* step of Phase 1's Workbench round-trip, before the visual check. Rationale: literals
would mean a second editing pass over every string, and literals that ship are how untranslatable UI
happens. Raw keys in the interim are self-diagnosing (project memory: raw keys on screen mean unregenerated
exports) and this is a GM-only panel, so the blast radius of a few minutes of raw keys is zero.

### D8 — Fixed authored rows; one `SetText` per value; no dynamic widgets

Every row exists in the layout and is found once at attach. Updates are `SetText` (and `SetColor` for
suppression). Nothing is created, destroyed, shown or hidden on the one-second tick. This is the
`overthrow-ui-patterns` guidance ("fixed grids over dynamic ones") applied to a panel that redraws twice a
second for as long as the editor is open, and it is what keeps the countdown smooth (§6 Q-2).

---

## 6. Definition of Done

Criteria an independent evaluator with no implementation context can verify.

### Functional

- **F-1** With the Game Master editor open in **EDIT** mode, an Overthrow panel is visible in the
  bottom-left stack, **above** the base game's mode/settings menu, with the Overthrow logo at its top.
- **F-2** In **Photo** mode (or any limited editor mode), the panel is **absent** — not blank, not empty:
  absent.
- **F-3** With a campaign running and the seam authorized, every row shows a live value: threat (one
  decimal), OF resources, OF deployment pool, next-distribution amount, next-payout amount, resistance
  funds, QRF status.
- **F-4** Both countdowns decrease once per second and **re-sync on each 8 s poll with no visible jump**.
- **F-5** When a QRF is running, the distribution row shows the suppression reason instead of a countdown,
  in the accent colour; when zero players are connected, the payout row does the same. Neither shows a
  countdown that will not fire.
- **F-6** Before the first snapshot lands — and after the editor closes and reopens — the campaign block
  shows the "waiting for campaign data" line instead of zeros, while the resistance-funds and QRF rows stay
  live.
- **F-7** Resistance funds update within a second of the value changing server-side (spend or earn), driven
  by `m_OnResistanceMoneyChanged` and not by the poll.

### Quality

- **Q-1 Chrome parity.** Background colour, width (380) and inner padding (`36 16 36 16`) match the mode
  menu directly below; the two panels' left edges align. A screenshot should not reveal which one shipped
  with the base game.
- **Q-2 No layout thrash.** The one-second tick performs `SetText` only. Grep proves no `SetVisible`, no
  `CreateWidgets`, no `RemoveFromHierarchy` and no `AddChild` inside the tick path.
- **Q-3 Open/close hygiene.** Opening and closing the editor **five times** produces no script errors, no
  duplicated panels, and no leftover widgets. Verified with a temporary `Print` in `HandlerAttached` and
  `HandlerDeattached` showing a matched pair per cycle.
- **Q-4 Unsubscribe hygiene.** After the panel is destroyed, a snapshot commit invokes no handler on it — a
  temporary `Print` in `OnSnapshot` is silent while the editor is shut, and the one-second `CallLater` has
  stopped.
- **Q-5 Listen-server host.** A host who opens GM sees the same populated panel as a remote GM client. This
  is the case gm-state's `ShouldRespondLocally` short-circuit exists for and the single most likely GM in
  the world.
- **Q-6 No new networking.** Grep over every file this feature adds: zero `Rpc(`, zero `[RplProp]`, zero
  `RplRcver`, zero `RplComponent`.
- **Q-7 Null-safety.** Grep proves every `OVT_ControllerComponent<OVT_GMRequestComponent>.Get()`,
  `OVT_Global.GetEconomy()`, `OVT_Global.GetOccupyingFaction()` and `OVT_Global.GetTowns()` call site is
  null-checked. `Get()` is null on a dedicated server and before ownership assignment by design
  (`OVT_ControllerComponent.c:22-29`).

### Integration

- **I-1 Zero forks.** `git diff --stat` shows no modified file under any base-game path, and exactly one
  `modded class` (`Scripts/Game/UI/Modded/SCR_EditModeEditorUIComponent.c`).
- **I-2 Detail seam exists and is documented.** `GetDetailSlot()`, `ShowDetail(bool)`, `ClearDetail()` and
  `GetInstance()` are present, Doxygen-commented, and written into `context.md` in enough detail that
  `hud-icons` can be planned from it alone. `DetailSection` is empty and invisible.
- **I-3 No `OVT_Global` accessor was added** for any controller component (project rule,
  `OVT_ControllerComponent.c:10-14`).
- **I-4 The gm-state doc bug is fixed** — `gm-state/context.md:140` names `IsDistributionSuppressed()` /
  `IsPayoutSuppressed()`.
- **I-5 gm-state's Phase 5 is discharged or honestly deferred** — §7's absorbed checklist is run and its
  results recorded in `gm-state/context.md`, including which auth path was exercised.
- **I-6 No help/wiki content was added** — the consolidated pass belongs to epic end
  (`epic-overview.md:72-74`).

### Verification Method

See §7. In short: `compile-check.sh` exit 0 at every phase boundary → Workbench visual and lifecycle check
(user) → the MP script including gm-state's absorbed checklist (user) → grep gates pasted into `context.md`.

---

## 7. Testing Strategy

**What the suites can genuinely see here is almost nothing, and saying so plainly is the point.** Coverage
in this project is a spine, not a surface: **JIP/multiplayer, UI, performance and the real restart path are
uncovered**. This feature is a widget tree inside the Game Master editor. No suite can open the editor.

### Logic tier — `OVT_TEST_Logic_GMPanelFormat.c` (Phase 2)

World-free and pure. This is the entirety of the feature's automatable surface, and it is worth having
because countdown formatting fails in exactly the cases nobody looks at.

| Case | Asserts |
|---|---|
| Under a minute | `7` → `"0:07"` (leading zero on seconds, no leading zero on minutes) |
| Minutes | `754` → `"12:34"` |
| Exactly an hour | `3600` → `"1:00:00"` (the format switches) |
| Just under an hour | `3599` → `"59:59"` (it does not switch early) |
| Zero | `0` → `"0:00"` |
| Negative clamps | `-5` → `"0:00"`, never `"-0:05"` |
| Threat precision | `3.87` → `"3.9"`, and `3.0` → `"3.0"` (not `"3"`) |

**Prove each case can fail** before shipping it — invert the expectation, confirm the failure is a *named*
`SetFailure` and not a silent pass, revert, and record the method in `context.md`.

⚠️ Keep `GetGame()`, `OVT_Global` and every world reference out of both `OVT_GMPanelFormat.c` and the test
file, **including comments** — the Logic-tier guard is a directory-wide grep that does not read prose.

### No new Init / Campaign / Persistence cases

The panel adds no component to a prefab, no manager, no persisted state. gm-state's
`OVT_TEST_Init_GMRequestSeam` already asserts the seam this feature consumes. Adding a case that asserts
"the layout file exists" would be a test of the filesystem. **The Fast and All groups run unchanged as a
regression net**, by the orchestrator, after a phase completes — never by a planning or implementation agent
(`.claude/test-policy.md`).

### Workbench check (Phase 1, user round-trip)

Listed as Phase 1 task 6. Its four questions are the only evidence for D2's ordering, chrome parity, logo
scaling and open/close hygiene. `tools/compile-check.sh` compiles EnforceScript and **does not parse
layouts, `.meta` files or the string table** — a malformed layout is a missing panel at runtime and a
missing `.meta` is an unresolvable resource, with no build-time signal for either.

### Multiplayer script (Phase 4) — **and gm-state's absorbed checklist**

⚠️ **Warn the user before launching** — client launches open a window on their desktop and can orphan.

**Auth, and the thing that will trip this up first.** `-ovtGmDev` opens gm-state's **data gate** only
(`OVT_GMRequestComponent.DEV_CLI_PARAM`, `gm-state/context.md:171`). It does **not** grant an unlimited
editor, and without an unlimited editor there is no EDIT mode, and without EDIT mode **this panel does not
exist**. So the panel almost certainly requires the **admin-login path**, and `-ovtGmDev` alone is not
sufficient to see it. Establish this first and **record it in `context.md`** — it is the single most useful
fact this play-test can produce for the rest of the epic.

1. `tools/launch-server.sh --mode dedicated -- -ovtGmDev` (dedicated mode provides the admin password;
   default `devadmin`).
2. `tools/launch-game.sh --timeout 3600 --profile OverthrowClient1 --allow-concurrent -- -client 127.0.0.1:2001`
   — **always pass the long timeout**; the 600 s default kills the client mid-test.
3. On client 1: log in as admin, open Game Master, confirm EDIT mode is reachable and the panel appears.
   **Record whether admin login alone sufficed, whether `-ovtGmDev` was also needed, and whether the panel
   was reachable at all without admin.**
4. **Panel checks (F-1 … F-7):** all rows populate within one poll; countdowns tick and re-sync without
   jumps; spend resistance money in-game and watch the funds row update off the invoker, not the poll;
   trigger a QRF and confirm the QRF row populates **and** the distribution row flips to suppressed.
5. **Absorbed from gm-state Phase 5 — what the panel proves and what still needs a log.** Be honest about
   the split; do not let a green panel stand in for evidence it cannot give.

   | gm-state item | Proven by | How |
   |---|---|---|
   | (1) State populates within one poll; countdowns tick and re-sync | **The panel** | Direct observation — this is what the panel was built to prove |
   | (2) Per-record data present, origins plausible | **A log, not the panel** | The panel renders **no records** by design. Set `m_bDebugSnapshotTiming` on the controller prefab and read the client's staged counts and the server's origins |
   | (3) Record counts + build ms on a populated campaign (~100–200 expected) | **A log** | Same flag; write the measured per-class numbers into `gm-state/context.md` |
   | (4) Non-admin receives zero `RpcDo_*` | **A log** | Restart the server **without** `-ovtGmDev`; a second, non-admin client; temporary `Print` at the top of each `RpcDo_*` never fires |
   | (5) Listen-server host GM sees state | **The panel** | Run a host instead of a dedicated server and repeat step 3 (Q-5) |
   | (6) JIP: second client joins an established campaign, becomes GM, first snapshot completes | **The panel** | Join client 2 late; the most common regression class in this project and covered by no suite |
   | (7) Close editor → requests stop, `GetOnStateCleared()` fires exactly once | **Both** | Panel returns to the empty state; server-side request `Print` goes quiet; a temporary counter in the panel's `OnCleared` reads exactly 1 |
   | (8, optional) Stale-discard under a ~200 ms poll | **A log** | Lower `m_fPollIntervalMs`; commits stay single-seq |

6. **Grep gates** (Q-2, Q-6, Q-7, I-1, I-3) — run them and paste the output into `context.md`. They are
   cheap and they are the only check on properties no test can see.

---

## 8. Dependencies

### Internal — all built, all read-only

| System | What is used | Where |
|---|---|---|
| gm-state seam | `GetState()`, `GetOnSnapshotUpdated()`, `GetOnStateCleared()` | `Scripts/Game/Components/Controller/OVT_GMRequestComponent.c:155/:164/:174` |
| gm-state store | scalars, `HasData()`, both `Get*SecondsRemaining()`, both `Is*Suppressed()` | `Scripts/Game/GameMode/GM/OVT_GMCampaignState.c` |
| Controller accessor | `OVT_ControllerComponent<T>.Get()` | `Scripts/Game/Components/Controller/OVT_ControllerComponent.c:36` |
| Economy | `GetResistanceMoney()`, `m_OnResistanceMoneyChanged` | `Scripts/Game/GameMode/Managers/OVT_EconomyManagerComponent.c:1239`, `:109` |
| Occupying faction | `m_bQRFActive`, `m_iQRFPoints`, `m_iQRFTimer`, `m_iCurrentQRFTown` | `Scripts/Game/GameMode/Managers/Factions/OVT_OccupyingFactionManager.c:161-166` |
| Towns | `GetTownName(int)` | `Scripts/Game/GameMode/Managers/OVT_TownManagerComponent.c:858` |
| Widget base | `SCR_ScriptedWidgetComponent` | base game; Overthrow precedent `OVT_ShopMenuCardComponent.c:9` |

### Base game — read and extended

`SCR_EditModeEditorUIComponent` (modded), `SCR_BaseModeEditorUIComponent`, `MenuRootSubComponent`,
`WorkspaceWidget.CreateWidgets`, `Mode_Edit.layout`'s `Mode_Edit_Element_Left` widget name,
`ModeMenu.layout` / `ModeMenu_GameMaster.layout` chrome values, `logo_overthrow.edds`.

**`Mode_Edit_Element_Left` is a base-game widget *name* we depend on**, which is the one durable coupling
this feature creates. A Reforger update that renames it degrades the panel to "absent" (never to an error,
per Phase 1 task 1's null check). Cheap to detect, cheap to fix.

### Files modified outside this feature's own directories

Exactly two: `Language/localization_Overthrow.st` (new items) and
`docs/features/gm/gm-state/context.md` (the doc-bug fix plus Phase 4's results). **No** base-game file, no
prefab, no `.conf`, no persistence registration.

### Blocks / blocked by

- **Blocked by:** `gm-state` — built and committed at `462308f5`.
- **Blocks:** `hud-icons` (needs the detail seam from Phase 3). `waypoint-viz` and `gm-map` are independent
  of this feature.

---

## 9. Risks & Mitigation

**R1 — `HandlerDeattached` may not fire when the editor tears the widget tree down, leaking a repeating
`CallLater` and three subscriptions per open/close cycle.**
This is the feature's top risk: the tick would keep firing forever against a dead widget, and N stale panel
components would each wake on every snapshot. `EOnEditorPostDeactivate` calls `RemoveFromHierarchy()`
(`SCR_MenuLayoutEditorComponent.c:109`), which is not by itself proof that the handler event fires.
**Mitigation, three layers:** (a) `HandlerDeattached` does the unsubscribe — the pattern
`SCR_BudgetEditorUIComponent.c:239` already relies on, in editor UI, which is good evidence it fires;
(b) the tick's **first line is a self-cancel guard** (`if (!m_wRoot) { Remove(self); return; }`), so a
leaked tick dies on its next fire rather than running forever; (c) every invoker handler early-returns on a
null `m_wRoot`. **Evidence:** Q-3's five-cycle open/close check with matched `Print` pairs. If the pairs do
not match, that is a finding to record, not something to paper over.

**R2 — §5 D2's child-ordering conclusion is wrong and the panel renders below the mode menu.**
Derived from a strong citation chain (`ChatPanel.layout:52` + `SCR_ChatPanel.c:319` + the `// Widget 0 is at
the bottom` comment at `:407`) but not observed in this container.
**Mitigation:** Phase 1's Workbench check answers it in the first thirty seconds of play, and the fix is two
lines (`RemoveChild` then `AddChild` on the mode menu — `Widget.c:48` and `:37`). Cost of being wrong: one
round-trip. Record which way it went.

**R3 — The panel cannot be reached locally because `-ovtGmDev` does not grant an unlimited editor.**
`-ovtGmDev` opens gm-state's data gate, nothing more. EDIT mode needs a real unlimited editor, which needs
admin — and local mode authenticates nobody. If admin login does not work locally, **the panel cannot be
seen in a local MP test at all**, and Phase 4 degrades to the host path plus Workbench Play mode.
**Mitigation:** establish the auth path as **step 3 of Phase 4, before anything else**, and record it. If
admin login fails locally, say so plainly in `context.md` rather than reporting a partial play-test as a
full one — and note that the base game's `SCR_EditorSettingsEntity` / "not configured with this game mode"
work is epic **Phase 3**, explicitly out of scope here.

**R4 — Text overflow at 380 px.** "Next Distribution" plus an amount plus "paused — QRF active" is a lot of
characters for one row, and a localized string can be 40% longer than English.
**Mitigation:** two-line rows (label above, value+status below) rather than one wide row; check the longest
realistic strings during Phase 1's Workbench visit, before any data is bound; keep suppression copy short
(the reason, not a sentence).

**R5 — A missing `.meta`, a duplicate widget GUID or a malformed layout ships silently.**
`compile-check.sh` does not parse layouts, `.meta` files or the string table.
**Mitigation:** the duplicate-GUID script from `overthrow-ui-patterns/layouts.md` is a Phase 1 acceptance
item; the `.meta` with all five console configurations is a Phase 1 acceptance item; the Workbench visit is
the only real gate and is scheduled inside Phase 1, not deferred to the end.

**R6 — A future contributor adds a row and reaches for a new RPC.**
The panel is a tempting place to "just fetch one more number", and doing it here would bypass gm-state's
authorization gate entirely.
**Mitigation:** D4's grep is in the DoD (Q-6); the class-level Doxygen block states the rule in one sentence
("this component sends and receives nothing; new data goes on gm-state's fan"); and `context.md` repeats it
where the next planner will read it.

**R7 — A parallel session changes the seam or the managers this panel reads.**
This tree receives concurrent bugfix commits; every Overthrow-side line number here is a snapshot of
`462308f5`.
**Mitigation:** re-check `git status` and re-confirm the cited lines at every phase boundary. The cited
*method names* are the durable anchor; the line numbers are a convenience.

**R8 — Both countdowns showing the same number reads as a bug.**
They currently do, by construction (`gm-state/context.md:84`), and a GM who notices will report it.
**Mitigation:** display copy names the two events distinctly ("Next distribution" / "Next payout") rather
than implying independent clocks; the fact is recorded in `context.md` so triage is one lookup, not an
investigation. **Do not "fix" it by collapsing the rows** — the two wire fields exist so the schedules can
diverge without a wire change.

---

*Sibling features: `hud-icons` (fills this panel's detail seam), `waypoint-viz`, `gm-map`. Data spine:
`docs/features/gm/gm-state/`. Epic: `docs/features/gm/epic-overview.md`.*
